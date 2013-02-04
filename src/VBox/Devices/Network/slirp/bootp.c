/* $Id$ */
/** @file
 * NAT - BOOTP/DHCP server emulation.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
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

/** Entry in the table of known DHCP clients. */
typedef struct
{
    uint32_t xid;
    bool allocated;
    uint8_t macaddr[6];
    struct in_addr addr;
    int number;
} BOOTPClient;
/** Number of DHCP clients supported by NAT. */
#define NB_ADDR     16

#define bootp_clients ((BOOTPClient *)pData->pbootp_clients)

/* XXX: only DHCP is supported */
static const uint8_t rfc1533_cookie[] = { RFC1533_COOKIE };

static void bootp_reply(PNATState pData, struct mbuf *m0, int offReply, uint16_t flags);

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

static BOOTPClient *bc_alloc_client(PNATState pData)
{
    int i;
    LogFlowFuncEnter();
    for (i = 0; i < NB_ADDR; i++)
    {
        if (!bootp_clients[i].allocated)
        {
            BOOTPClient *bc;

            bc = &bootp_clients[i];
            memset(bc, 0, sizeof(BOOTPClient));
            bc->allocated = 1;
            bc->number = i;
            LogFlowFunc(("LEAVE: bc:%d\n", bc->number));
            return bc;
        }
    }
    LogFlowFunc(("LEAVE: NULL\n"));
    return NULL;
}

static BOOTPClient *get_new_addr(PNATState pData, struct in_addr *paddr)
{
    BOOTPClient *bc;
    LogFlowFuncEnter();
    bc = bc_alloc_client(pData);
    if (!bc)
        return NULL;

    paddr->s_addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | (bc->number + START_ADDR));
    bc->addr.s_addr = paddr->s_addr;
    LogFlowFunc(("LEAVE: paddr:%RTnaipv4, bc:%d\n", paddr->s_addr, bc->number));
    return bc;
}

static int release_addr(PNATState pData, struct in_addr *paddr)
{
    unsigned i;
    for (i = 0; i < NB_ADDR; i++)
    {
        if (paddr->s_addr == bootp_clients[i].addr.s_addr)
        {
            memset(&bootp_clients[i], 0, sizeof(BOOTPClient));
            return VINF_SUCCESS;
        }
    }
    return VERR_NOT_FOUND;
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

    LogFlowFunc(("macaddr:%RTmac\n", macaddr));
    for (i = 0; i < NB_ADDR; i++)
    {
        if (   memcmp(macaddr, bootp_clients[i].macaddr, ETH_ALEN) == 0
            && bootp_clients[i].allocated != 0)
        {
            BOOTPClient *bc;

            bc = &bootp_clients[i];
            bc->allocated = 1;
            paddr->s_addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | (i + START_ADDR));
            LogFlowFunc(("LEAVE: paddr:%RTnaipv4 bc:%d\n", paddr->s_addr, bc->number));
            return bc;
        }
    }
    LogFlowFunc(("LEAVE: NULL\n"));
    return NULL;
}

static struct mbuf *dhcp_create_msg(PNATState pData, struct bootp_t *bp, struct mbuf *m, uint8_t type)
{
    struct bootp_t *rbp;
    struct ethhdr *eh;
    uint8_t *q;

    eh = mtod(m, struct ethhdr *);
    memcpy(eh->h_source, bp->bp_hwaddr, ETH_ALEN); /* XXX: if_encap just swap source with dest */

    m->m_data += if_maxlinkhdr; /*reserve ether header */

    rbp = mtod(m, struct bootp_t *);
    memset(rbp, 0, sizeof(struct bootp_t));
    rbp->bp_op = BOOTP_REPLY;
    rbp->bp_xid = bp->bp_xid; /* see table 3 of rfc2131*/
    rbp->bp_flags = bp->bp_flags; /* figure 2 of rfc2131 */
    rbp->bp_giaddr.s_addr = bp->bp_giaddr.s_addr;
#if 0 /*check flags*/
    saddr.sin_port = RT_H2N_U16_C(BOOTP_SERVER);
    daddr.sin_port = RT_H2N_U16_C(BOOTP_CLIENT);
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

static int dhcp_do_ack_offer(PNATState pData, struct mbuf *m, BOOTPClient *bc, int fDhcpRequest)
{
    struct bootp_t *rbp = NULL;
    uint8_t *q;
    struct in_addr saddr;
    int val;

    struct dns_entry *de = NULL;
    struct dns_domain_entry *dd = NULL;
    int added = 0;
    uint8_t *q_dns_header = NULL;
    uint32_t lease_time = RT_H2N_U32_C(LEASE_TIME);
    uint32_t netmask = RT_H2N_U32(pData->netmask);

    rbp = mtod(m, struct bootp_t *);
    q = &rbp->bp_vend[0];
    q += 7; /* !cookie rfc 2132 + TYPE*/

    /*DHCP Offer specific*/
    /*
     * we're care in built-in tftp server about existence/validness of the boot file.
     */
    if (bootp_filename)
        RTStrPrintf((char*)rbp->bp_file, sizeof(rbp->bp_file), "%s", bootp_filename);

    Log(("NAT: DHCP: bp_file:%s\n", &rbp->bp_file));
    /* Address/port of the DHCP server. */
    rbp->bp_yiaddr = bc->addr; /* Client IP address */
    Log(("NAT: DHCP: bp_yiaddr:%RTnaipv4\n", rbp->bp_yiaddr));
    rbp->bp_siaddr = pData->tftp_server; /* Next Server IP address, i.e. TFTP */
    Log(("NAT: DHCP: bp_siaddr:%RTnaipv4\n", rbp->bp_siaddr));
    if (fDhcpRequest)
    {
        rbp->bp_ciaddr.s_addr = bc->addr.s_addr; /* Client IP address */
    }
    saddr.s_addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | CTL_ALIAS);
    Log(("NAT: DHCP: s_addr:%RTnaipv4\n", saddr));

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

    if (pData->fUseDnsProxy || pData->fUseHostResolver)
    {
        uint32_t addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | CTL_DNS);
        FILL_BOOTP_EXT(q, RFC1533_DNS, 4, &addr);
    }
    else
    {
        if (!TAILQ_EMPTY(&pData->pDnsList))
        {
            de = TAILQ_LAST(&pData->pDnsList, dns_list_head);
            q_dns_header = q;
            FILL_BOOTP_EXT(q, RFC1533_DNS, 4, &de->de_addr.s_addr);
        }

        TAILQ_FOREACH_REVERSE(de, &pData->pDnsList, dns_list_head, de_list)
        {
            if (TAILQ_LAST(&pData->pDnsList, dns_list_head) == de)
                continue; /* first value with head we've ingected before */
            FILL_BOOTP_APP(q_dns_header, q, RFC1533_DNS, 4, &de->de_addr.s_addr);
        }
    }
    if (pData->fPassDomain && !pData->fUseHostResolver)
    {
        LIST_FOREACH(dd, &pData->pDomainList, dd_list)
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
    slirp_arp_cache_update_or_add(pData, rbp->bp_yiaddr.s_addr, bc->macaddr);
    return q - rbp->bp_vend; /*return offset */
}

static int dhcp_send_nack(PNATState pData, struct bootp_t *bp, BOOTPClient *bc, struct mbuf *m)
{
    NOREF(bc);

    dhcp_create_msg(pData, bp, m, DHCPNAK);
    return 7;
}

static int dhcp_send_ack(PNATState pData, struct bootp_t *bp, BOOTPClient *bc, struct mbuf *m, int fDhcpRequest)
{
    int offReply = 0; /* boot_reply will fill general options and add END before sending response */

    dhcp_create_msg(pData, bp, m, DHCPACK);
    offReply = dhcp_do_ack_offer(pData, m, bc, fDhcpRequest);
    return offReply;
}

static int dhcp_send_offer(PNATState pData, struct bootp_t *bp, BOOTPClient *bc, struct mbuf *m)
{
    int offReply = 0; /* boot_reply will fill general options and add END before sending response */

    dhcp_create_msg(pData, bp, m, DHCPOFFER);
    offReply = dhcp_do_ack_offer(pData, m, bc, /* fDhcpRequest=*/ 0);
    return offReply;
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

enum DHCP_REQUEST_STATES
{
    INIT_REBOOT,
    SELECTING,
    RENEWING,
    REBINDING,
    NONE
};

static int dhcp_decode_request(PNATState pData, struct bootp_t *bp, struct mbuf *m)
{
    BOOTPClient *bc = NULL;
    struct in_addr daddr;
    int offReply;
    uint8_t *req_ip = NULL;
    uint8_t *server_ip = NULL;
    uint32_t ui32;
    enum DHCP_REQUEST_STATES dhcp_stat = NONE;

    /* need to understand which type of request we get */
    req_ip = dhcp_find_option(&bp->bp_vend[0], RFC2132_REQ_ADDR);
    server_ip = dhcp_find_option(&bp->bp_vend[0], RFC2132_SRV_ID);

    bc = find_addr(pData, &daddr, bp->bp_hwaddr);

    if (server_ip != NULL)
    {
        /* selecting */
        if (!bc)
        {
             LogRel(("NAT: DHCP no IP was allocated\n"));
             return -1;
        }

        if (   !req_ip
            || bp->bp_ciaddr.s_addr != INADDR_ANY)
        {
            LogRel(("NAT: Invalid SELECTING request\n"));
            return -1; /* silently ignored */
        }
        dhcp_stat = SELECTING;
        Assert((bp->bp_ciaddr.s_addr == INADDR_ANY));
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
            /* table 4 of rfc2131 */
            if (bp->bp_flags & RT_H2N_U16_C(DHCP_FLAGS_B))
                dhcp_stat = REBINDING;
            else
                dhcp_stat = RENEWING;
        }
    }

    /*?? renewing ??*/
    switch (dhcp_stat)
    {
        case RENEWING:
            /**
             *  decoding client messages RFC2131 (4.3.6)
             *  ------------------------------
             *  |              |RENEWING     |
             *  ------------------------------
             *  |broad/unicast |unicast      |
             *  |server-ip     |MUST NOT     |
             *  |requested-ip  |MUST NOT     |
             *  |ciaddr        |IP address   |
             *  ------------------------------
             */
            Assert((server_ip == NULL && req_ip == NULL && bp->bp_ciaddr.s_addr != INADDR_ANY));
            if (   server_ip
                || req_ip
                || bp->bp_ciaddr.s_addr == INADDR_ANY)
            {
                LogRel(("NAT: invalid RENEWING dhcp request\n"));
                return -1; /* silent ignorance */
            }
            if (bc != NULL)
            {
                Assert((bc->addr.s_addr == bp->bp_ciaddr.s_addr));
                /*if it already here well just do ack, we aren't aware of dhcp time expiration*/
            }
            else
            {
               if ((bp->bp_ciaddr.s_addr & RT_H2N_U32(pData->netmask)) != pData->special_addr.s_addr)
               {
                   LogRel(("NAT: Client %RTnaipv4 requested IP -- sending NAK\n", bp->bp_ciaddr));
                   offReply = dhcp_send_nack(pData, bp, bc, m);
                   return offReply;
               }

               bc = bc_alloc_client(pData);
               if (!bc)
               {
                   LogRel(("NAT: can't alloc address. RENEW has been silently ignored.\n"));
                   return -1;
               }

               Assert((bp->bp_hlen == ETH_ALEN));
               memcpy(bc->macaddr, bp->bp_hwaddr, bp->bp_hlen);
               bc->addr.s_addr = bp->bp_ciaddr.s_addr;
            }
            break;

        case INIT_REBOOT:
            /**
             *  decoding client messages RFC2131 (4.3.6)
             *  ------------------------------
             *  |              |INIT-REBOOT  |
             *  ------------------------------
             *  |broad/unicast |broadcast    |
             *  |server-ip     |MUST NOT     |
             *  |requested-ip  |MUST         |
             *  |ciaddr        |zero         |
             *  ------------------------------
             *
             */
            Assert(server_ip == NULL);
            Assert(req_ip != NULL);
            if (   server_ip
                || !req_ip
                || bp->bp_ciaddr.s_addr != INADDR_ANY)
            {
                LogRel(("NAT: Invalid INIT-REBOOT dhcp request\n"));
                return -1; /* silently ignored */
            }
            ui32 = *(uint32_t *)(req_ip + 2);
            if ((ui32 & RT_H2N_U32(pData->netmask)) != pData->special_addr.s_addr)
            {
                LogRel(("NAT: address %RTnaipv4 has been requested -- sending NAK\n", ui32));
                offReply = dhcp_send_nack(pData, bp, bc, m);
                return offReply;
            }

            bc = bc_alloc_client(pData);
            if (!bc)
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
            if (dhcp_stat == REBINDING)
                LogRel(("NAT: REBINDING state isn't impemented\n"));
            else if (dhcp_stat == SELECTING)
                LogRel(("NAT: SELECTING state isn't impemented\n"));
            return -1;

        default:
            break;
    }

    LogRel(("NAT: DHCP offered IP address %RTnaipv4\n", bc->addr));
    offReply = dhcp_send_ack(pData, bp, bc, m, /* fDhcpRequest=*/ 1);
    return offReply;
}

static int dhcp_decode_discover(PNATState pData, struct bootp_t *bp, int fDhcpDiscover, struct mbuf *m)
{
    BOOTPClient *bc;
    struct in_addr daddr;
    int offReply;

    if (fDhcpDiscover)
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
        LogRel(("NAT: DHCP offered IP address %RTnaipv4\n", bc->addr));
        offReply = dhcp_send_offer(pData, bp, bc, m);
        return offReply;
    }
    else
    {
        bc = find_addr(pData, &daddr, bp->bp_hwaddr);
        if (!bc)
        {
            LogRel(("NAT: DHCP Inform was ignored no boot client was found\n"));
            return -1;
        }

        LogRel(("NAT: DHCP offered IP address %RTnaipv4\n", bc->addr));
        offReply = dhcp_send_ack(pData, bp, bc, m, /* fDhcpRequest=*/ 0);
        return offReply;
    }

    return -1;
}

static int dhcp_decode_release(PNATState pData, struct bootp_t *bp)
{
    int rc = release_addr(pData, &bp->bp_ciaddr);
    LogRel(("NAT: %s %RTnaipv4\n",
            RT_SUCCESS(rc) ? "DHCP released IP address" : "Ignored DHCP release for IP address",
            &bp->bp_ciaddr));
    return 0;
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
    const uint8_t *pu8RawDhcpObject;
    int rc;
    struct in_addr req_ip;
    int fDhcpDiscover = 0;
    uint8_t *parameter_list = NULL;
    struct mbuf *m = NULL;

    pu8RawDhcpObject = buf;
    if (size < 5)
        return;

    if (memcmp(pu8RawDhcpObject, rfc1533_cookie, 4) != 0)
        return;

    /* note: pu8RawDhcpObject doesn't point to parameter buf */
    pu8RawDhcpObject = dhcp_find_option(bp->bp_vend, RFC2132_MSG_TYPE);
    Assert(pu8RawDhcpObject);
    if (!pu8RawDhcpObject)
        return;
    /*
     * We're going update dns list at least once per DHCP transaction (!not on every operation
     * within transaction), assuming that transaction can't be longer than 1 min.
     */
    if (   !pData->fUseHostResolver
        && (   pData->dnsLastUpdate == 0
            || curtime - pData->dnsLastUpdate > 60 * 1000)) /* one minute*/
    {
        uint8_t i = 2; /* i = 0 - tag, i == 1 - length */
        parameter_list = dhcp_find_option(&bp->bp_vend[0], RFC2132_PARAM_LIST);
        for (;parameter_list && i < parameter_list[1]; ++i)
        {
            if (parameter_list[i] == RFC1533_DNS)
            {
                slirpReleaseDnsSettings(pData);
                slirpInitializeDnsSettings(pData);
                pData->dnsLastUpdate = curtime;
                break;
            }
        }
    }

    m = m_getcl(pData, M_DONTWAIT, MT_HEADER, M_PKTHDR);
    if (!m)
    {
        LogRel(("NAT: can't alocate memory for response!\n"));
        return;
    }

    switch (*(pu8RawDhcpObject + 2))
    {
        case DHCPDISCOVER:
            fDhcpDiscover = 1;
            /* fall through */
        case DHCPINFORM:
            rc = dhcp_decode_discover(pData, bp, fDhcpDiscover, m);
            if (rc > 0)
                goto reply;
            break;

        case DHCPREQUEST:
            rc = dhcp_decode_request(pData, bp, m);
            if (rc > 0)
                goto reply;
            break;

        case DHCPRELEASE:
            dhcp_decode_release(pData, bp);
            /* no reply required */
            break;

        case DHCPDECLINE:
            /* note: pu8RawDhcpObject doesn't point to DHCP header, now it's expected it points
             * to Dhcp Option RFC2132_REQ_ADDR
             */
            pu8RawDhcpObject = dhcp_find_option(&bp->bp_vend[0], RFC2132_REQ_ADDR);
            if (!pu8RawDhcpObject)
            {
                Log(("NAT: RFC2132_REQ_ADDR not found\n"));
                break;
            }
            req_ip.s_addr = *(uint32_t *)(pu8RawDhcpObject + 2);
            rc = bootp_cache_lookup_ether_by_ip(pData, req_ip.s_addr, NULL);
            if (RT_FAILURE(rc))
            {
                /* Not registered */
                BOOTPClient *bc;
                bc = bc_alloc_client(pData);
                Assert(bc);
                if (!bc)
                {
                    LogRel(("NAT: can't allocate bootp client object\n"));
                    break;
                }
                bc->addr.s_addr = req_ip.s_addr;
                slirp_arp_who_has(pData, bc->addr.s_addr);
                LogRel(("NAT: %RTnaipv4 has been already registered\n", req_ip));
            }
            /* no response required */
            break;

        default:
            AssertMsgFailed(("unsupported DHCP message type"));
    }
    /* silently ignore */
    m_freem(pData, m);
    return;

reply:
    bootp_reply(pData, m, rc, bp->bp_flags);
}

static void bootp_reply(PNATState pData, struct mbuf *m, int offReply, uint16_t flags)
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
    q += offReply;

    saddr.sin_addr.s_addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | CTL_ALIAS);

    FILL_BOOTP_EXT(q, RFC2132_SRV_ID, 4, &saddr.sin_addr);

    *q++ = RFC1533_END; /* end of message */

    m->m_pkthdr.header = mtod(m, void *);
    m->m_len = sizeof(struct bootp_t)
             - sizeof(struct ip)
             - sizeof(struct udphdr);
    m->m_data += sizeof(struct udphdr)
               + sizeof(struct ip);
    if (   (flags & RT_H2N_U16_C(DHCP_FLAGS_B))
        || nack != 0)
        daddr.sin_addr.s_addr = INADDR_BROADCAST;
    else
        daddr.sin_addr.s_addr = rbp->bp_yiaddr.s_addr; /*unicast requested by client*/
    saddr.sin_port = RT_H2N_U16_C(BOOTP_SERVER);
    daddr.sin_port = RT_H2N_U16_C(BOOTP_CLIENT);
    udp_output2(pData, NULL, m, &saddr, &daddr, IPTOS_LOWDELAY);
}

void bootp_input(PNATState pData, struct mbuf *m)
{
    struct bootp_t *bp = mtod(m, struct bootp_t *);

    if (bp->bp_op == BOOTP_REQUEST)
        dhcp_decode(pData, bp, bp->bp_vend, DHCP_OPT_LEN);
}

int bootp_cache_lookup_ip_by_ether(PNATState pData,const uint8_t* ether, uint32_t *pip)
{
    int i;

    if (!ether || !pip)
        return VERR_INVALID_PARAMETER;

    for (i = 0; i < NB_ADDR; i++)
    {
        if (   bootp_clients[i].allocated
            && memcmp(bootp_clients[i].macaddr, ether, ETH_ALEN) == 0)
        {
            *pip = bootp_clients[i].addr.s_addr;
            return VINF_SUCCESS;
        }
    }

    *pip = INADDR_ANY;
    return VERR_NOT_FOUND;
}

int bootp_cache_lookup_ether_by_ip(PNATState pData, uint32_t ip, uint8_t *ether)
{
    int i;
    for (i = 0; i < NB_ADDR; i++)
    {
        if (   bootp_clients[i].allocated
            && ip == bootp_clients[i].addr.s_addr)
        {
            if (ether != NULL)
                memcpy(ether, bootp_clients[i].macaddr, ETH_ALEN);
            return VINF_SUCCESS;
        }
    }

    return VERR_NOT_FOUND;
}

/*
 * Initialize dhcp server
 * @returns 0 - if initialization is ok, non-zero otherwise
 */
int bootp_dhcp_init(PNATState pData)
{
    pData->pbootp_clients = RTMemAllocZ(sizeof(BOOTPClient) * NB_ADDR);
    if (!pData->pbootp_clients)
        return VERR_NO_MEMORY;

    return VINF_SUCCESS;
}

int bootp_dhcp_fini(PNATState pData)
{
    if (pData->pbootp_clients != NULL)
        RTMemFree(pData->pbootp_clients);

    return VINF_SUCCESS;
}
