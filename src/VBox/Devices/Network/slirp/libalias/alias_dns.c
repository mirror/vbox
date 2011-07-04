/* $Id$ */
/** @file
 * libalias helper for using the host resolver instead of dnsproxy.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef RT_OS_WINDOWS
# include <netdb.h>
#endif
#include <iprt/ctype.h>
#include <iprt/assert.h>
#include <slirp.h>
#include "alias.h"
#include "alias_local.h"
#include "alias_mod.h"
#define isdigit(ch)    RT_C_IS_DIGIT(ch)
#define isalpha(ch)    RT_C_IS_ALPHA(ch)

#define DNS_CONTROL_PORT_NUMBER 53
/* see RFC 1035(4.1.1) */
union dnsmsg_header
{
    struct
    {
        unsigned id:16;
        unsigned rd:1;
        unsigned tc:1;
        unsigned aa:1;
        unsigned opcode:4;
        unsigned qr:1;
        unsigned rcode:4;
        unsigned Z:3;
        unsigned ra:1;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    } X;
    uint16_t raw[6];
};
AssertCompileSize(union dnsmsg_header, 12);

struct dns_meta_data
{
    uint16_t type;
    uint16_t class;
};

struct dnsmsg_answer
{
    uint16_t name;
    struct dns_meta_data meta;
    uint16_t ttl[2];
    uint16_t rdata_len;
    uint8_t  rdata[1];  /* depends on value at rdata_len */
};

/* see RFC 1035(4.1) */
static int dns_alias_handler(PNATState pData, int type);
static void CStr2QStr(const char *pcszStr, char *pszQStr, size_t cQStr);
static void QStr2CStr(const char *pcszQStr, char *pszStr, size_t cStr);

static int
fingerprint(struct libalias *la, struct ip *pip, struct alias_data *ah)
{

    if (!ah->dport || !ah->sport || !ah->lnk)
        return -1;

    Log(("NAT:%s: ah(dport: %hd, sport: %hd) oaddr:%RTnaipv4 aaddr:%RTnaipv4\n",
        __FUNCTION__, ntohs(*ah->dport), ntohs(*ah->sport),
        ah->oaddr, ah->aaddr));

    if (   (ntohs(*ah->dport) == DNS_CONTROL_PORT_NUMBER
        || ntohs(*ah->sport) == DNS_CONTROL_PORT_NUMBER)
        && (ah->oaddr->s_addr == htonl(ntohl(la->pData->special_addr.s_addr)|CTL_DNS)))
        return 0;

    return -1;
}

static void doanswer(struct libalias *la, union dnsmsg_header *hdr, struct dns_meta_data *pReqMeta, char *qname, struct ip *pip, struct hostent *h)
{
    int i;

    if (!h)
    {
        hdr->X.qr = 1; /* response */
        hdr->X.aa = 1;
        hdr->X.rd = 1;
        hdr->X.rcode = 3;
    }
    else
    {
        char *query;
        char *answers;
        uint16_t off;
        char **cstr;
        char *c;
        uint16_t packet_len = 0;
        uint16_t addr_off = (uint16_t)~0;
        struct dns_meta_data *meta;

#if 0
        /* here is no compressed names+answers + new query */
        m_inc(m, h->h_length * sizeof(struct dnsmsg_answer) + strlen(qname) + 2 * sizeof(uint16_t));
#endif
        packet_len = (pip->ip_hl << 2)
                   + sizeof(struct udphdr)
                   + sizeof(union dnsmsg_header)
                   + strlen(qname)
                   + sizeof(struct dns_meta_data); /* ip + udp + header + query */
        query = (char *)&hdr[1];

        strcpy(query, qname);
        query += strlen(qname) + 1;
        /* class & type informations lay right after symbolic inforamtion. */
        meta = (struct dns_meta_data *)query;
        meta->type = pReqMeta->type;
        meta->class = pReqMeta->class;

        /* answers zone lays after query in response packet */
        answers = (char *)&meta[1];

        off = (char *)&hdr[1] - (char *)hdr;
        off |= (0x3 << 14);

        /* add aliases */
        for (cstr = h->h_aliases; *cstr; cstr++)
        {
            uint16_t len;
            struct dnsmsg_answer *ans = (struct dnsmsg_answer *)answers;
            ans->name = htons(off);
            ans->meta.type = htons(5); /* CNAME */
            ans->meta.class = htons(1);
            *(uint32_t *)ans->ttl = htonl(3600); /* 1h */
            c = (addr_off == (uint16_t)~0 ? h->h_name : *cstr);
            len = strlen(c) + 2;
            ans->rdata_len = htons(len);
            ans->rdata[len - 1] = 0;
            CStr2QStr(c, (char *)ans->rdata, len);
            off = (char *)&ans->rdata - (char *)hdr;
            off |= (0x3 << 14);
            if (addr_off == (uint16_t)~0)
                addr_off = off;
            answers = (char *)&ans[1] + len - 2;  /* note: 1 symbol already counted */
            packet_len += sizeof(struct dnsmsg_answer) + len - 2;
            hdr->X.ancount++;
        }
        /* add addresses */

        for(i = 0; i < h->h_length && h->h_addr_list[i] != NULL; ++i)
        {
            struct dnsmsg_answer *ans = (struct dnsmsg_answer *)answers;

            ans->name = htons(off);
            ans->meta.type = htons(1);
            ans->meta.class = htons(1);
            *(uint32_t *)ans->ttl = htonl(3600); /* 1h */
            ans->rdata_len = htons(4); /* IPv4 */
            *(uint32_t *)ans->rdata = *(uint32_t *)h->h_addr_list[i];
            answers = (char *)&ans[1] + 2;
            packet_len += sizeof(struct dnsmsg_answer) + 3;
            hdr->X.ancount++;
        }
        hdr->X.qr = 1; /* response */
        hdr->X.aa = 1;
        hdr->X.rd = 1;
        hdr->X.ra = 1;
        hdr->X.rcode = 0;
        HTONS(hdr->X.ancount);
        /* don't forget update m_len */
        pip->ip_len = htons(packet_len);
    }
}
static int
protohandler(struct libalias *la, struct ip *pip, struct alias_data *ah)
{
    int i;
    /* Parse dns request */
    char *qw_qname = NULL;
    uint16_t *qw_qtype = NULL;
    uint16_t *qw_qclass = NULL;
    struct hostent *h = NULL;
    char cname[255];
    int cname_len = 0;
    struct dns_meta_data *meta;

    struct udphdr *udp = NULL;
    union dnsmsg_header *hdr = NULL;
    udp = (struct udphdr *)ip_next(pip);
    hdr = (union dnsmsg_header *)udp_next(udp);

    if (hdr->X.qr == 1)
        return 0; /* this is respose */

    memset(cname, 0, sizeof(cname));
    qw_qname = (char *)&hdr[1];
    Assert((ntohs(hdr->X.qdcount) == 1));
    if ((ntohs(hdr->X.qdcount) != 1))
    {
        static bool fMultiWarn;
        if (!fMultiWarn)
        {
            LogRel(("NAT:alias_dns: multiple quieries isn't supported\n"));
            fMultiWarn = true;
        }
        return 1;
    }

    for (i = 0; i < ntohs(hdr->X.qdcount); ++i)
    {
        meta = (struct dns_meta_data *)(qw_qname + strlen(qw_qname) + 1);
        Log(("qname:%s qtype:%hd qclass:%hd\n",
            qw_qname, ntohs(meta->type), ntohs(meta->class)));

        QStr2CStr(qw_qname, cname, sizeof(cname));
        cname_len = RTStrNLen(cname, sizeof(cname));
        /* Some guests like win-xp adds _dot_ after host name
         * and after domain name (not passed with host resolver)
         * that confuses host resolver.
         */
        if (   cname_len > 2
            && cname[cname_len - 1] == '.'
            && cname[cname_len - 2] == '.')
        {
            cname[cname_len - 1] = 0;
            cname[cname_len - 2] = 0;
        }
        h = gethostbyname(cname);
        fprintf(stderr, "cname:%s\n", cname);
        doanswer(la, hdr, meta, qw_qname, pip, h);
    }

    /*
     * We have changed the size and the content of udp, to avoid double csum calculation
     * will assign to zero
     */
    udp->uh_sum = 0;
    udp->uh_ulen = ntohs(htons(pip->ip_len) - (pip->ip_hl << 2));
    pip->ip_sum = 0;
    pip->ip_sum = LibAliasInternetChecksum(la, (uint16_t *)pip, pip->ip_hl << 2);
    return 0;
}

/*
 * qstr is z-string with -dot- replaced with \count to next -dot-
 * e.g. ya.ru is \02ya\02ru
 * Note: it's assumed that caller allocates buffer for cstr
 */
static void QStr2CStr(const char *pcszQStr, char *pszStr, size_t cStr)
{
    const char *q;
    char *c;
    size_t cLen = 0;

    Assert(cStr > 0);
    for (q = pcszQStr, c = pszStr; *q != '\0' && cLen < cStr-1; q++, cLen++)
    {
        if (   isalpha(*q)
            || isdigit(*q)
            || *q == '-'
            || *q == '_')
        {
           *c = *q;
            c++;
        }
        else if (c != &pszStr[0])
        {
            *c = '.';
            c++;
        }
    }
    *c = '\0';
}

/*
 *
 */
static void CStr2QStr(const char *pcszStr, char *pszQStr, size_t cQStr)
{
    const char *c;
    const char *pc;
    char *q;
    size_t cLen = 0;

    Assert(cQStr > 0);
    for (c = pcszStr, q = pszQStr; *c != '\0' && cLen < cQStr-1; q++, cLen++)
    {
        /* at the begining or at -dot- position */
        if (*c == '.' || (c == pcszStr && q == pszQStr))
        {
            if (c != pcszStr)
                c++;
            pc = strchr(c, '.');
            *q = pc ? (pc - c) : strlen(c);
        }
        else
        {
            *q = *c;
            c++;
        }
    }
    *q = '\0';
}


int
dns_alias_load(PNATState pData)
{
    return dns_alias_handler(pData, MOD_LOAD);
}

int
dns_alias_unload(PNATState pData)
{
    return dns_alias_handler(pData, MOD_UNLOAD);
}

#define handlers pData->dns_module
static int
dns_alias_handler(PNATState pData, int type)
{
    int error;

    if (!handlers)
        handlers = RTMemAllocZ(2 * sizeof(struct proto_handler));

    handlers[0].pri = 20;
    handlers[0].dir = IN;
    handlers[0].proto = UDP;
    handlers[0].fingerprint = &fingerprint;
    handlers[0].protohandler = &protohandler;
    handlers[1].pri = EOH;

    switch (type)
    {
        case MOD_LOAD:
            error = 0;
            LibAliasAttachHandlers(pData, handlers);
            break;

        case MOD_UNLOAD:
            error = 0;
            LibAliasDetachHandlers(pData, handlers);
            RTMemFree(handlers);
            handlers = NULL;
            break;

        default:
            error = EINVAL;
    }
    return error;
}
