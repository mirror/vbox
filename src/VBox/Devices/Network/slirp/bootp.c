/*
 * QEMU BOOTP/DHCP server
 *
 * Copyright (c) 2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <slirp.h>

/* XXX: only DHCP is supported */


static const uint8_t rfc1533_cookie[] = { RFC1533_COOKIE };

DECLINLINE(void) dprintf(const char *pszFormat, ...)
{
#ifdef LOG_ENABLED
    va_list args;
    va_start(args, pszFormat);
    Log(("dhcp: %N", pszFormat, &args));
    va_end(args);
#endif
}

static BOOTPClient *get_new_addr(PNATState pData, struct in_addr *paddr)
{
    BOOTPClient *bc;
    int i;

    for(i = 0; i < NB_ADDR; i++) {
        if (!bootp_clients[i].allocated)
            goto found;
    }
    return NULL;
 found:
    bc = &bootp_clients[i];
    bc->allocated = 1;
    paddr->s_addr = htonl(ntohl(special_addr.s_addr) | (i + START_ADDR));
    return bc;
}

static int release_addr(PNATState pData, struct in_addr *paddr)
{
    unsigned i;

    i = ntohl(paddr->s_addr) - START_ADDR - ntohl(special_addr.s_addr);
    if (i >= NB_ADDR)
        return 0;
    memset(bootp_clients[i].macaddr, '\0', 6);
    bootp_clients[i].allocated = 0;
    return 1;
}

static BOOTPClient *find_addr(PNATState pData, struct in_addr *paddr, const uint8_t *macaddr)
{
    BOOTPClient *bc;
    int i;

    for(i = 0; i < NB_ADDR; i++) {
        if (!memcmp(macaddr, bootp_clients[i].macaddr, 6))
            goto found;
    }
    return NULL;
 found:
    bc = &bootp_clients[i];
    bc->allocated = 1;
    paddr->s_addr = htonl(ntohl(special_addr.s_addr) | (i + START_ADDR));
    return bc;
}

static void dhcp_decode(const uint8_t *buf, int size,
                        int *pmsg_type, struct in_addr *req_ip)
{
    const uint8_t *p, *p_end;
    int len, tag;

    *pmsg_type = 0;

    p = buf;
    p_end = buf + size;
    if (size < 5)
        return;
    if (memcmp(p, rfc1533_cookie, 4) != 0)
        return;
    p += 4;
    while (p < p_end) {
        tag = p[0];
        if (tag == RFC1533_PAD) {
            p++;
        } else if (tag == RFC1533_END) {
            break;
        } else {
            p++;
            if (p >= p_end)
                break;
            len = *p++;
            dprintf("dhcp: tag=0x%02x len=%d\n", tag, len);

            switch(tag) {
            case RFC2132_REQ_ADDR:
                if (len >= 4)
                    *req_ip = *(struct in_addr*)p;
                break;
            case RFC2132_MSG_TYPE:
                if (len >= 1)
                    *pmsg_type = p[0];
                break;
            default:
                break;
            }
            p += len;
        }
    }
}

static void bootp_reply(PNATState pData, struct bootp_t *bp)
{
    BOOTPClient *bc;
    struct mbuf *m;
    struct bootp_t *rbp;
    struct sockaddr_in saddr, daddr;
    struct in_addr dns_addr_dhcp;
    int dhcp_msg_type, val;
    uint8_t *q;
    struct in_addr requested_ip; /* the requested IP in DHCPREQUEST */
    int send_nak = 0;
    uint32_t ipv4_addr;

    /* extract exact DHCP msg type */
    requested_ip.s_addr = 0xffffffff;
    dhcp_decode(bp->bp_vend, DHCP_OPT_LEN, &dhcp_msg_type, &requested_ip);
    dprintf("bootp packet op=%d msgtype=%d\n", bp->bp_op, dhcp_msg_type);

    if (dhcp_msg_type == 0)
        dhcp_msg_type = DHCPREQUEST; /* Force reply for old BOOTP clients */

    if (dhcp_msg_type == DHCPRELEASE) {
        int rc;
        ipv4_addr = ntohl(bp->bp_ciaddr.s_addr);
        rc = release_addr(pData, &bp->bp_ciaddr);
        LogRel(("NAT: %s %u.%u.%u.%u\n",
                rc ? "DHCP released IP address" : "Ignored DHCP release for IP address",
                ipv4_addr >> 24, (ipv4_addr >> 16) & 0xff, (ipv4_addr >> 8) & 0xff, ipv4_addr & 0xff));
        dprintf("released addr=%08x\n", ntohl(bp->bp_ciaddr.s_addr));
        /* This message is not to be answered in any way. */
        return;
    }
    if (dhcp_msg_type != DHCPDISCOVER &&
        dhcp_msg_type != DHCPREQUEST)
        return;
    /* XXX: this is a hack to get the client mac address */
    memcpy(client_ethaddr, bp->bp_hwaddr, 6);

    if ((m = m_get(pData)) == NULL)
        return;
    m->m_data += if_maxlinkhdr;
    rbp = (struct bootp_t *)m->m_data;
    m->m_data += sizeof(struct udpiphdr);
    memset(rbp, 0, sizeof(struct bootp_t));

    if (dhcp_msg_type == DHCPDISCOVER) {
        /* Do not allocate a new lease for clients that forgot that they had a lease. */
        bc = find_addr(pData, &daddr.sin_addr, bp->bp_hwaddr);
        if (!bc)
        {
    new_addr:
            bc = get_new_addr(pData, &daddr.sin_addr);
            if (!bc) {
                LogRel(("NAT: DHCP no IP address left\n"));
                dprintf("no address left\n");
                return;
            }
            memcpy(bc->macaddr, client_ethaddr, 6);
        }
    } else {
        bc = find_addr(pData, &daddr.sin_addr, bp->bp_hwaddr);
        if (!bc) {
            /* if never assigned, behaves as if it was already
               assigned (windows fix because it remembers its address) */
            goto new_addr;
        }
    }

    if (tftp_prefix && RTDirExists(tftp_prefix) && bootp_filename)
        RTStrPrintf((char*)rbp->bp_file, sizeof(rbp->bp_file), "%s", bootp_filename);

    saddr.sin_addr.s_addr = htonl(ntohl(special_addr.s_addr) | CTL_ALIAS);
    saddr.sin_port = htons(BOOTP_SERVER);

    daddr.sin_port = htons(BOOTP_CLIENT);

    rbp->bp_op = BOOTP_REPLY;
    rbp->bp_xid = bp->bp_xid;
    rbp->bp_htype = 1;
    rbp->bp_hlen = 6;
    memcpy(rbp->bp_hwaddr, bp->bp_hwaddr, 6);

    rbp->bp_yiaddr = daddr.sin_addr; /* Client IP address */
    rbp->bp_siaddr = saddr.sin_addr; /* Server IP address */

    q = rbp->bp_vend;
    memcpy(q, rfc1533_cookie, 4);
    q += 4;

    if (dhcp_msg_type == DHCPDISCOVER) {
        *q++ = RFC2132_MSG_TYPE;
        *q++ = 1;
        *q++ = DHCPOFFER;
    } else if (dhcp_msg_type == DHCPREQUEST) {
        *q++ = RFC2132_MSG_TYPE;
        *q++ = 1;
        if (   requested_ip.s_addr != 0xffffffff
            && requested_ip.s_addr != daddr.sin_addr.s_addr)
        {
            /* network changed */
            *q++ = DHCPNAK;
            send_nak = 1;
        }
        else
            *q++ = DHCPACK;
    }

    if (send_nak)
    {
        ipv4_addr = ntohl(requested_ip.s_addr);
        LogRel(("NAT: Client requested IP address %u.%u.%u.%u -- sending NAK\n",
                ipv4_addr >> 24, (ipv4_addr >> 16) & 0xff, (ipv4_addr >> 8) & 0xff, ipv4_addr & 0xff));
    }
    else
    {
        ipv4_addr = ntohl(daddr.sin_addr.s_addr);
        LogRel(("NAT: DHCP offered IP address %u.%u.%u.%u\n",
                ipv4_addr >> 24, (ipv4_addr >> 16) & 0xff, (ipv4_addr >> 8) & 0xff, ipv4_addr & 0xff));
        dprintf("offered addr=%08x\n", ntohl(daddr.sin_addr.s_addr));
    }

    if (dhcp_msg_type == DHCPDISCOVER ||
        dhcp_msg_type == DHCPREQUEST) {
        *q++ = RFC2132_SRV_ID;
        *q++ = 4;
        memcpy(q, &saddr.sin_addr, 4);
        q += 4;
    }

    if (!send_nak &&
        (dhcp_msg_type == DHCPDISCOVER ||
         dhcp_msg_type == DHCPREQUEST)) {
        *q++ = RFC1533_NETMASK;
        *q++ = 4;
        *q++ = (pData->netmask & 0xff000000) >> 24;
        *q++ = (pData->netmask & 0x00ff0000) >> 16;
        *q++ = (pData->netmask & 0x0000ff00) >>  8;
        *q++ = (pData->netmask & 0x000000ff);

        *q++ = RFC1533_GATEWAY;
        *q++ = 4;
        memcpy(q, &saddr.sin_addr, 4);
        q += 4;

        *q++ = RFC1533_DNS;
        *q++ = 4;
        dns_addr_dhcp.s_addr = htonl(ntohl(special_addr.s_addr) | CTL_DNS);
        memcpy(q, &dns_addr_dhcp, 4);
        q += 4;

        *q++ = RFC2132_LEASE_TIME;
        *q++ = 4;
        val = htonl(LEASE_TIME);
        memcpy(q, &val, 4);
        q += 4;

        if (*slirp_hostname) {
            val = strlen(slirp_hostname);
            *q++ = RFC1533_HOSTNAME;
            *q++ = val;
            memcpy(q, slirp_hostname, val);
            q += val;
        }

        if (pData->pszDomain && pData->fPassDomain)
        {
            val = strlen(pData->pszDomain);
            *q++ = RFC1533_DOMAINNAME;
            *q++ = val;
            memcpy(q, pData->pszDomain, val);
            q += val;
        }
    }
    *q++ = RFC1533_END;

    m->m_len = sizeof(struct bootp_t) -
        sizeof(struct ip) - sizeof(struct udphdr);
    /* Reply to the broadcast address, as some clients perform paranoid checks. */
    daddr.sin_addr.s_addr = INADDR_BROADCAST;
    udp_output2(pData, NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
}

void bootp_input(PNATState pData, struct mbuf *m)
{
    struct bootp_t *bp = mtod(m, struct bootp_t *);

    if (bp->bp_op == BOOTP_REQUEST) {
        bootp_reply(pData, bp);
    }
}
