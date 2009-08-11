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
static void bootp_reply(PNATState pData, struct mbuf *m0, int off, uint16_t flags);

static uint8_t *dhcp_find_option(uint8_t *vend, uint8_t tag)
{
    uint8_t *q = vend;
    uint8_t len;
    /*@todo magic validation */
    q += 4; /*magic*/ 
    while(*q != RFC1533_END)
    {
        if (*q == RFC1533_PAD)
            continue;
        if (*q == tag)
            return q;
        q++;
        len = *q;
        q += 1 + len; 
    }
    return NULL; 
}
static BOOTPClient *alloc_addr(PNATState pData)
{
    int i;
    for (i = 0; i < NB_ADDR; i++)
    {
        if (!bootp_clients[i].allocated)
        {
            BOOTPClient *bc;

            bc = &bootp_clients[i];
            memset(bc, 0, sizeof(BOOTPClient));
            bc->allocated = 1;
            bc->number = i;
            return bc;
        }
    }
    return NULL;
}
static BOOTPClient *get_new_addr(PNATState pData, struct in_addr *paddr)
{
    BOOTPClient *bc;
    bc = alloc_addr(pData);
    if (bc == NULL)
        return NULL;
    paddr->s_addr = htonl(ntohl(special_addr.s_addr) | (bc->number + START_ADDR));
    bc->addr.s_addr = paddr->s_addr;
    return bc;
    return NULL;
}

static int release_addr(PNATState pData, struct in_addr *paddr)
{
    unsigned i;
    for (i = 0; i < NB_ADDR; i++)
    {
        if (paddr->s_addr == bootp_clients[i].addr.s_addr)
        {
            memset(&bootp_clients[i], 0, sizeof(BOOTPClient));
            return 1;
        }
    }
    return 0;
}

/*
 * from RFC 2131 4.3.1
 * Field      DHCPOFFER            DHCPACK             DHCPNAK
 * -----      ---------            -------             -------
 * 'op'       BOOTREPLY            BOOTREPLY           BOOTREPLY
 * 'htype'    (From "Assigned Numbers" RFC)
 * 'hlen'     (Hardware address length in octets)
 * 'hops'     0                    0                   0
 * 'xid'      'xid' from client    'xid' from client   'xid' from client
 *            DHCPDISCOVER         DHCPREQUEST         DHCPREQUEST
 *            message              message             message
 * 'secs'     0                    0                   0
 * 'ciaddr'   0                    'ciaddr' from       0
 *                                 DHCPREQUEST or 0
 * 'yiaddr'   IP address offered   IP address          0
 *            to client            assigned to client
 * 'siaddr'   IP address of next   IP address of next  0
 *            bootstrap server     bootstrap server
 * 'flags'    'flags' from         'flags' from        'flags' from
 *            client DHCPDISCOVER  client DHCPREQUEST  client DHCPREQUEST
 *            message              message             message
 * 'giaddr'   'giaddr' from        'giaddr' from       'giaddr' from
 *            client DHCPDISCOVER  client DHCPREQUEST  client DHCPREQUEST
 *            message              message             message
 * 'chaddr'   'chaddr' from        'chaddr' from       'chaddr' from
 *            client DHCPDISCOVER  client DHCPREQUEST  client DHCPREQUEST
 *            message              message             message
 * 'sname'    Server host name     Server host name    (unused)
 *            or options           or options
 * 'file'     Client boot file     Client boot file    (unused)
 *            name or options      name or options
 * 'options'  options              options
 * 
 * Option                    DHCPOFFER    DHCPACK            DHCPNAK
 * ------                    ---------    -------            -------
 * Requested IP address      MUST NOT     MUST NOT           MUST NOT
 * IP address lease time     MUST         MUST (DHCPREQUEST) MUST NOT
 *                                        MUST NOT (DHCPINFORM)
 * Use 'file'/'sname' fields MAY          MAY                MUST NOT
 * DHCP message type         DHCPOFFER    DHCPACK            DHCPNAK
 * Parameter request list    MUST NOT     MUST NOT           MUST NOT
 * Message                   SHOULD       SHOULD             SHOULD
 * Client identifier         MUST NOT     MUST NOT           MAY
 * Vendor class identifier   MAY          MAY                MAY
 * Server identifier         MUST         MUST               MUST
 * Maximum message size      MUST NOT     MUST NOT           MUST NOT
 * All others                MAY          MAY                MUST NOT
 */
static BOOTPClient *find_addr(PNATState pData, struct in_addr *paddr, const uint8_t *macaddr)
{
    int i;

    for (i = 0; i < NB_ADDR; i++)
    {
        if (!memcmp(macaddr, bootp_clients[i].macaddr, 6))
        {
            BOOTPClient *bc;

            bc = &bootp_clients[i];
            bc->allocated = 1;
            paddr->s_addr = htonl(ntohl(special_addr.s_addr) | (i + START_ADDR));
            return bc;
        }
    }
    return NULL;
}

static struct mbuf *dhcp_create_msg(PNATState pData, struct bootp_t *bp, struct mbuf *m, uint8_t type)
{
    struct bootp_t *rbp;
    struct ethhdr *eh;
    uint8_t *q;

    rbp = mtod(m, struct bootp_t *);
    memset(rbp, 0, sizeof(struct bootp_t));
    eh = mtod(m, struct ethhdr *);
    memcpy(eh->h_source, bp->bp_hwaddr, ETH_ALEN); /* XXX: if_encap just swap source with dest*/
    m->m_data += if_maxlinkhdr; /*reserve ether header */
    rbp = mtod(m, struct bootp_t *);
    rbp->bp_op = BOOTP_REPLY;
    rbp->bp_xid = bp->bp_xid; /* see table 3 of rfc2131*/
    rbp->bp_flags = bp->bp_flags;
    rbp->bp_giaddr.s_addr = bp->bp_giaddr.s_addr;
#if 0 /*check flags*/
    saddr.sin_port = htons(BOOTP_SERVER);
    daddr.sin_port = htons(BOOTP_CLIENT);
#endif
    rbp->bp_htype = 1;
    rbp->bp_hlen = 6;
    memcpy(rbp->bp_hwaddr, bp->bp_hwaddr, 6);

    memcpy(rbp->bp_vend, rfc1533_cookie, 4); /* cookie */
    q = rbp->bp_vend;
    q += 4;
    *q++ = RFC2132_MSG_TYPE;
    *q++ = 1;
    *q++ = type;

    return m;
}

static int dhcp_do_ack_offer(PNATState pData, struct mbuf *m, BOOTPClient *bc, int is_from_request)
{
    int off = 0;
    struct bootp_t *rbp = NULL;
    uint8_t *q;
    struct in_addr saddr;
    int val;

    struct dns_entry *de = NULL;
    struct dns_domain_entry *dd = NULL;
    int added = 0;
    uint8_t *q_dns_header = NULL;
    uint32_t lease_time = htonl(LEASE_TIME);
    uint32_t netmask = htonl(pData->netmask);

    rbp = mtod(m, struct bootp_t *);
    q = &rbp->bp_vend[0];
    q += 7; /* !cookie rfc 2132 + TYPE*/

    /*DHCP Offer specific*/
    if (   tftp_prefix
        && RTDirExists(tftp_prefix)
        && bootp_filename)
        RTStrPrintf((char*)rbp->bp_file, sizeof(rbp->bp_file), "%s", bootp_filename);

    Log(("NAT: DHCP: bp_file:%s\n", &rbp->bp_file));
    /* Address/port of the DHCP server. */
    rbp->bp_yiaddr = bc->addr; /* Client IP address */
    Log(("NAT: DHCP: bp_yiaddr:%R[IP4]\n", &rbp->bp_yiaddr));
    rbp->bp_siaddr = pData->tftp_server; /* Next Server IP address, i.e. TFTP */
    Log(("NAT: DHCP: bp_siaddr:%R[IP4]\n", &rbp->bp_siaddr));
    if (is_from_request)
    {
        rbp->bp_ciaddr.s_addr = bc->addr.s_addr; /* Client IP address */
    }
#ifndef VBOX_WITH_NAT_SERVICE
    saddr.s_addr = htonl(ntohl(special_addr.s_addr) | CTL_ALIAS);
#else
    saddr.s_addr = special_addr.s_addr;
#endif
    Log(("NAT: DHCP: s_addr:%R[IP4]\n", &saddr));

#define FILL_BOOTP_EXT(q, tag, len, pvalue)                     \
    do {                                                        \
        struct bootp_ext *be = (struct bootp_ext *)(q);         \
        be->bpe_tag = (tag);                                    \
        be->bpe_len = (len);                                    \
        memcpy(&be[1], (pvalue), (len));                        \
        (q) = (uint8_t *)(&be[1]) + (len);                      \
    }while(0)
/* appending another value to tag, calculates len of whole block*/
#define FILL_BOOTP_APP(head, q, tag, len, pvalue)               \
    do {                                                        \
        struct bootp_ext *be = (struct bootp_ext *)(head);      \
        memcpy(q, (pvalue), (len));                             \
        (q) += (len);                                           \
        Assert(be->bpe_tag == (tag));                           \
        be->bpe_len += (len);                                   \
    }while(0)

    
    
    FILL_BOOTP_EXT(q, RFC1533_NETMASK, 4, &netmask);
    FILL_BOOTP_EXT(q, RFC1533_GATEWAY, 4, &saddr);
    
    if (pData->use_dns_proxy)
    {
        uint32_t addr = htonl(ntohl(special_addr.s_addr) | CTL_DNS);
        FILL_BOOTP_EXT(q, RFC1533_DNS, 4, &addr);
        goto skip_dns_servers;
    }
    
    if (!TAILQ_EMPTY(&pData->dns_list_head)) 
    {
        de = TAILQ_LAST(&pData->dns_list_head, dns_list_head);
        q_dns_header = q;
        FILL_BOOTP_EXT(q, RFC1533_DNS, 4, &de->de_addr.s_addr);
    }
    
    TAILQ_FOREACH_REVERSE(de, &pData->dns_list_head, dns_list_head, de_list)
    {
        if (TAILQ_LAST(&pData->dns_list_head, dns_list_head) == de)
            continue; /* first value with head we've ingected before */
        FILL_BOOTP_APP(q_dns_header, q, RFC1533_DNS, 4, &de->de_addr.s_addr);
    }

skip_dns_servers:
    if (LIST_EMPTY(&pData->dns_domain_list_head))
    {
            /* Microsoft dhcp client doen't like domain-less dhcp and trimmed packets*/
            /* dhcpcd client very sad if no domain name is passed */
            FILL_BOOTP_EXT(q, RFC1533_DOMAINNAME, 1, " "); 
    }
    if (pData->fPassDomain)
    {
        LIST_FOREACH(dd, &pData->dns_domain_list_head, dd_list)
        {
            
            if (dd->dd_pszDomain == NULL)
                continue;
            /* never meet valid separator here in RFC1533*/
            if (added != 0) 
                FILL_BOOTP_EXT(q, RFC1533_DOMAINNAME, 1, ","); 
            else
                added = 1;
            val = (int)strlen(dd->dd_pszDomain);
            FILL_BOOTP_EXT(q, RFC1533_DOMAINNAME, val, dd->dd_pszDomain);
        }
    }
    
    FILL_BOOTP_EXT(q, RFC2132_LEASE_TIME, 4, &lease_time);
    
    if (*slirp_hostname)
    {
        val = (int)strlen(slirp_hostname);
        FILL_BOOTP_EXT(q, RFC1533_HOSTNAME, val, slirp_hostname);
    }
    return q - rbp->bp_vend; /*return offset */
}

static int dhcp_send_nack(PNATState pData, struct bootp_t *bp, BOOTPClient *bc, struct mbuf *m)
{
    struct bootp_t *rbp;
    uint8_t *q = NULL;
    rbp = mtod(m, struct bootp_t *);

    dhcp_create_msg(pData, bp, m, DHCPNAK);
    
    return 7;
}
static int dhcp_send_ack(PNATState pData, struct bootp_t *bp, BOOTPClient *bc, struct mbuf *m, int is_from_request)
{
    struct bootp_t *rbp;
    int off = 0; /* boot_reply will fill general options and add END before sending response*/

    dhcp_create_msg(pData, bp, m, DHCPACK);
    off = dhcp_do_ack_offer(pData, m, bc, is_from_request);
    return off;
}
static int dhcp_send_offer(PNATState pData, struct bootp_t *bp, BOOTPClient *bc, struct mbuf *m)
{
    struct bootp_t *rbp;
    int off = 0; /* boot_reply will fill general options and add END before sending response*/

    dhcp_create_msg(pData, bp, m, DHCPOFFER);
    off = dhcp_do_ack_offer(pData, m, bc, 0);
    return off;
}

/**
 *  decoding client messages RFC2131 (4.3.6)
 *  ---------------------------------------------------------------------
 *  |              |INIT-REBOOT  |SELECTING    |RENEWING     |REBINDING |
 *  ---------------------------------------------------------------------
 *  |broad/unicast |broadcast    |broadcast    |unicast      |broadcast |
 *  |server-ip     |MUST NOT     |MUST         |MUST NOT     |MUST NOT  |
 *  |requested-ip  |MUST         |MUST         |MUST NOT     |MUST NOT  |
 *  |ciaddr        |zero         |zero         |IP address   |IP address|
 *  ---------------------------------------------------------------------
 *
 */
enum DHCP_REQUEST_STATES{INIT_REBOOT, SELECTING, RENEWING, REBINDING, NONE};
static int dhcp_decode_request(PNATState pData, struct bootp_t *bp, const uint8_t *buf, int size, struct mbuf *m)
{
    BOOTPClient *bc;
    struct in_addr daddr;
    int off;
    uint8_t *opt;
    uint8_t *req_ip;
    uint8_t *server_ip;
    uint32_t ui32;
    enum DHCP_REQUEST_STATES dhcp_stat = NONE;
    /*need to understand which type of request we get */
    req_ip = dhcp_find_option(&bp->bp_vend[0], RFC2132_REQ_ADDR);
    server_ip = dhcp_find_option(&bp->bp_vend[0], RFC2132_SRV_ID);
    bc = find_addr(pData, &daddr, bp->bp_hwaddr);

    if (server_ip != NULL)
    {
        /*selecting*/
        if (!bc)
        {
             LogRel(("NAT: DHCP no IP wasn't allocated\n"));
             return -1;
        }
        dhcp_stat = SELECTING;
        Assert((bp->bp_ciaddr.s_addr == INADDR_ANY));
        Assert((*(uint32_t *)(req_ip + 2) == bc->addr.s_addr)); /*the same address as in offer*/
#if 0
        /* DSL xid in request differ from offer */
        Assert((bp->bp_xid == bc->xid));
#endif
    }
    else
    {
        if (req_ip != NULL)
        {
            /* init-reboot */
            dhcp_stat = INIT_REBOOT;
        }
        else
        {
            if (bp->bp_flags & DHCP_FLAGS_B)
                dhcp_stat = RENEWING;
            else 
                dhcp_stat = REBINDING; /*??rebinding??*/
        }
    }
        /*?? renewing ??*/
    switch (dhcp_stat)
    {
        case RENEWING: 
        {
            Assert((   server_ip == NULL 
                && req_ip == NULL
                && bp->bp_ciaddr.s_addr != INADDR_ANY));
            if (bc != NULL)
            {
                Assert((bc->addr.s_addr == bp->bp_ciaddr.s_addr));
                /*if it already here well just do ack, we aren't aware of dhcp time expiration*/
            }
            else
            {
               if ((bp->bp_ciaddr.s_addr & htonl(pData->netmask)) != special_addr.s_addr) 
               {
                    off = dhcp_send_nack(pData, bp, bc, m);
                    return off;
               }
               bc = alloc_addr(pData);
               if (bc == NULL)
               {
                   LogRel(("NAT: can't alloc address. RENEW has been silently ignored\n"));
                   return -1;
               }
               Assert((bp->bp_hlen == ETH_ALEN));
               memcpy(bc->macaddr, bp->bp_hwaddr, bp->bp_hlen);
               bc->addr.s_addr = bp->bp_ciaddr.s_addr; 
            }
        }
        case INIT_REBOOT:
            Assert(server_ip == NULL);
            Assert(req_ip != NULL);
            ui32 = *(uint32_t *)(req_ip + 2);
            if ((ui32 & htonl(pData->netmask)) != special_addr.s_addr) 
            {
                LogRel(("NAT: address %R[IP4] has been req.\n", &ui32));
                 off = dhcp_send_nack(pData, bp, bc, m);
                 return off;
            }
            bc = alloc_addr(pData);
            if (bc == NULL)
            {
                LogRel(("NAT: can't alloc address. RENEW has been silently ignored\n"));
                return -1;
            }
            Assert((bp->bp_hlen == ETH_ALEN));
            memcpy(bc->macaddr, bp->bp_hwaddr, bp->bp_hlen);
            bc->addr.s_addr = ui32; 
        break;
        case NONE:
            Assert((dhcp_stat != NONE));
            return -1;
        default:
        break;
    }
    off = dhcp_send_ack(pData, bp, bc, m, 1);
    return off;
}

static int dhcp_decode_discover(PNATState pData, struct bootp_t *bp, const uint8_t *buf, int size, int flag, struct mbuf *m)
{
    BOOTPClient *bc;
    struct in_addr daddr;
    int off;
    /* flag == 1 discover */
    if (flag == 1)
    {
        bc = find_addr(pData, &daddr, bp->bp_hwaddr);
        if (!bc)
        {
            bc = get_new_addr(pData, &daddr);
            if (!bc)
            {
                LogRel(("NAT: DHCP no IP address left\n"));
                Log(("no address left\n"));
                return -1;
            }
            memcpy(bc->macaddr, bp->bp_hwaddr, 6);
        }
        bc->xid = bp->bp_xid;
        /*bc isn't NULL */
        off = dhcp_send_offer(pData, bp, bc, m); 
        return off;
    } 
    else
    {
        /* flag == 0 inform */
        bc = find_addr(pData, &daddr, bp->bp_hwaddr);
        if (bc == NULL)
        {
            LogRel(("NAT: DHCP Inform was ignored no boot client was found\n"));
            return -1;
        }
        off = dhcp_send_ack(pData, bp, bc, m, 0); 
        return off;
    }
    return -1;
}

static int dhcp_decode_release(PNATState pData, struct bootp_t *bp, const uint8_t *buf, int size, int flag)
{
    return -1;
}
/**
 * fields for discovering t
 * Field      DHCPDISCOVER          DHCPREQUEST           DHCPDECLINE,
 *            DHCPINFORM                                  DHCPRELEASE
 * -----      ------------          -----------           -----------
 * 'op'       BOOTREQUEST           BOOTREQUEST           BOOTREQUEST
 * 'htype'    (From "Assigned Numbers" RFC)
 * 'hlen'     (Hardware address length in octets)
 * 'hops'     0                     0                     0
 * 'xid'      selected by client    'xid' from server     selected by
 *                                  DHCPOFFER message     client
 * 'secs'     0 or seconds since    0 or seconds since    0
 *            DHCP process started  DHCP process started
 * 'flags'    Set 'BROADCAST'       Set 'BROADCAST'       0
 *            flag if client        flag if client
 *            requires broadcast    requires broadcast
 *            reply                 reply
 * 'ciaddr'   0 (DHCPDISCOVER)      0 or client's         0 (DHCPDECLINE)
 *            client's              network address       client's network
 *            network address       (BOUND/RENEW/REBIND)  address
 *            (DHCPINFORM)                                (DHCPRELEASE)
 * 'yiaddr'   0                     0                     0
 * 'siaddr'   0                     0                     0
 * 'giaddr'   0                     0                     0
 * 'chaddr'   client's hardware     client's hardware     client's hardware
 *            address               address               address
 * 'sname'    options, if           options, if           (unused)
 *            indicated in          indicated in
 *            'sname/file'          'sname/file'
 *            option; otherwise     option; otherwise
 *            unused                unused
 * 'file'     options, if           options, if           (unused)
 *            indicated in          indicated in
 *            'sname/file'          'sname/file'
 *            option; otherwise     option; otherwise
 *            unused                unused
 * 'options'  options               options               (unused)
 * Requested IP address       MAY           MUST (in         MUST
 *                            (DISCOVER)    SELECTING or     (DHCPDECLINE),
 *                            MUST NOT      INIT-REBOOT)     MUST NOT
 *                            (INFORM)      MUST NOT (in     (DHCPRELEASE)
 *                                          BOUND or
 *                                          RENEWING)
 * IP address lease time      MAY           MAY              MUST NOT
 *                            (DISCOVER)
 *                            MUST NOT
 *                            (INFORM)
 * Use 'file'/'sname' fields  MAY           MAY              MAY
 * DHCP message type          DHCPDISCOVER/ DHCPREQUEST      DHCPDECLINE/
 *                            DHCPINFORM                     DHCPRELEASE
 * Client identifier          MAY           MAY              MAY
 * Vendor class identifier    MAY           MAY              MUST NOT
 * Server identifier          MUST NOT      MUST (after      MUST
 *                                          SELECTING)
 *                                          MUST NOT (after
 *                                          INIT-REBOOT,
 *                                          BOUND, RENEWING
 *                                          or REBINDING)
 * Parameter request list     MAY           MAY              MUST NOT
 * Maximum message size       MAY           MAY              MUST NOT
 * Message                    SHOULD NOT    SHOULD NOT       SHOULD
 * Site-specific              MAY           MAY              MUST NOT
 * All others                 MAY           MAY              MUST NOT
 * 
 */
static void dhcp_decode(PNATState pData, struct bootp_t *bp, const uint8_t *buf, int size)
{
    const uint8_t *p, *p_end;
    int rc;
    int pmsg_type; 
    struct in_addr req_ip;
    int flag = 0;
    int len, tag;
    struct mbuf *m = NULL;

    pmsg_type = 0;

    p = buf;
    p_end = buf + size;
    if (size < 5)
        return;
    if (memcmp(p, rfc1533_cookie, 4) != 0)
        return;
    p = dhcp_find_option(bp->bp_vend, RFC2132_MSG_TYPE);
    Assert(p);
    if (p == NULL)
        return;
    if ((m = m_get(pData)) == NULL)
    {
        LogRel(("NAT: can't alocate memory for response!\n"));
        return;
    }
    switch(*(p+2))
    {
        case DHCPDISCOVER:
            flag = 1;
            /**/
        case DHCPINFORM:
            rc = dhcp_decode_discover(pData, bp, buf, size, flag, m);
            if (rc > 0)
                goto reply;
        break;
        case DHCPREQUEST:
            rc = dhcp_decode_request(pData, bp, buf, size, m);
            if (rc > 0)
                goto reply;
        break;
        case DHCPRELEASE:
            flag = 1;
#if 0
        case DHCPDECLINE:
#endif
            rc = dhcp_decode_release(pData, bp, buf, size, flag);
            if (rc > 0)
                goto reply;
        break;
        default:
            AssertMsgFailed(("unsupported DHCP message type"));
    }
    Assert(m);
    /*silently ignore*/
    m_free(pData, m);
    return;
reply:
    bootp_reply(pData, m, rc, bp->bp_flags);
    return;
}

static void bootp_reply(PNATState pData, struct mbuf *m, int off, uint16_t flags)
{
    struct sockaddr_in saddr, daddr; 
    struct bootp_t *rbp = NULL;
    uint8_t *q = NULL;
    int nack;
    rbp = mtod(m, struct bootp_t *);
    Assert((m));
    Assert((rbp));
    q = rbp->bp_vend;
    nack = (q[6] == DHCPNAK);
    q += off;

#ifndef VBOX_WITH_NAT_SERVICE
    saddr.sin_addr.s_addr = htonl(ntohl(special_addr.s_addr) | CTL_ALIAS);
#else
    saddr.sin_addr.s_addr = special_addr.s_addr;
#endif

    FILL_BOOTP_EXT(q, RFC2132_SRV_ID, 4, &saddr.sin_addr);

    
    *q++ = RFC1533_END; /*end of message */
    

    m->m_len = sizeof(struct bootp_t)
             - sizeof(struct ip)
             - sizeof(struct udphdr);
    m->m_data += sizeof(struct udphdr)
               + sizeof(struct ip);
    if ((flags & DHCP_FLAGS_B) || nack != 0)
        daddr.sin_addr.s_addr = INADDR_BROADCAST;
    else
        daddr.sin_addr.s_addr = rbp->bp_yiaddr.s_addr; /*unicast requested by client*/
    saddr.sin_port = htons(BOOTP_SERVER);
    daddr.sin_port = htons(BOOTP_CLIENT);
    udp_output2(pData, NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
}

void bootp_input(PNATState pData, struct mbuf *m)
{
    struct bootp_t *bp = mtod(m, struct bootp_t *);

    if (bp->bp_op == BOOTP_REQUEST)
    {
        dhcp_decode(pData, bp, bp->bp_vend, DHCP_OPT_LEN);
    }
}
