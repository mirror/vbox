/* $Id$ */
/** @file
 * libalias helper for using the host resolver instead of dnsproxy.
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
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
static int  dns_alias_handler(PNATState pData, int type);
static void CStr2QStr(const char *pcszStr, char *pszQStr, size_t cQStr);
static void QStr2CStr(const char *pcszQStr, char *pszStr, size_t cStr);
#ifdef VBOX_WITH_DNSMAPPING_IN_HOSTRESOLVER
static void alterHostentWithDataFromDNSMap(PNATState pData, struct hostent *pHostent);
#endif

static int
fingerprint(struct libalias *la, struct ip *pIp, struct alias_data *ah)
{

    NOREF(la);
    NOREF(pIp);
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

static void doanswer(union dnsmsg_header *pHdr, struct dns_meta_data *pReqMeta, char *pszQname, struct ip *pIp, struct hostent *pHostent)
{
    int i;

    if (!pHostent)
    {
        pHdr->X.qr = 1; /* response */
        pHdr->X.aa = 1;
        pHdr->X.rd = 1;
        pHdr->X.rcode = 3;
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
        m_inc(m, pHostent->h_length * sizeof(struct dnsmsg_answer) + strlen(pszQname) + 2 * sizeof(uint16_t));
#endif
        packet_len = (pIp->ip_hl << 2)
                   + sizeof(struct udphdr)
                   + sizeof(union dnsmsg_header)
                   + strlen(pszQname)
                   + sizeof(struct dns_meta_data); /* ip + udp + header + query */
        query = (char *)&pHdr[1];

        strcpy(query, pszQname);
        query += strlen(pszQname) + 1;
        /* class & type informations lay right after symbolic inforamtion. */
        meta = (struct dns_meta_data *)query;
        meta->type = pReqMeta->type;
        meta->class = pReqMeta->class;

        /* answers zone lays after query in response packet */
        answers = (char *)&meta[1];

        off = (char *)&pHdr[1] - (char *)pHdr;
        off |= (0x3 << 14);

        /* add aliases */
        for (cstr = pHostent->h_aliases; cstr && *cstr; cstr++)
        {
            uint16_t len;
            struct dnsmsg_answer *ans = (struct dnsmsg_answer *)answers;
            ans->name = htons(off);
            ans->meta.type = htons(5); /* CNAME */
            ans->meta.class = htons(1);
            *(uint32_t *)ans->ttl = htonl(3600); /* 1h */
            c = (addr_off == (uint16_t)~0 ? pHostent->h_name : *cstr);
            len = strlen(c) + 2;
            ans->rdata_len = htons(len);
            ans->rdata[len - 1] = 0;
            CStr2QStr(c, (char *)ans->rdata, len);
            off = (char *)&ans->rdata - (char *)pHdr;
            off |= (0x3 << 14);
            if (addr_off == (uint16_t)~0)
                addr_off = off;
            answers = (char *)&ans[1] + len - 2;  /* note: 1 symbol already counted */
            packet_len += sizeof(struct dnsmsg_answer) + len - 2;
            pHdr->X.ancount++;
        }
        /* add addresses */

        for(i = 0; i < pHostent->h_length && pHostent->h_addr_list[i] != NULL; ++i)
        {
            struct dnsmsg_answer *ans = (struct dnsmsg_answer *)answers;

            ans->name = htons(off);
            ans->meta.type = htons(1);
            ans->meta.class = htons(1);
            *(uint32_t *)ans->ttl = htonl(3600); /* 1h */
            ans->rdata_len = htons(4); /* IPv4 */
            *(uint32_t *)ans->rdata = *(uint32_t *)pHostent->h_addr_list[i];
            answers = (char *)&ans[1] + 2;
            packet_len += sizeof(struct dnsmsg_answer) + 3;
            pHdr->X.ancount++;
        }
        pHdr->X.qr = 1; /* response */
        pHdr->X.aa = 1;
        pHdr->X.rd = 1;
        pHdr->X.ra = 1;
        pHdr->X.rcode = 0;
        HTONS(pHdr->X.ancount);
        /* don't forget update m_len */
        pIp->ip_len = htons(packet_len);
    }
}

static int
protohandler(struct libalias *la, struct ip *pIp, struct alias_data *ah)
{
    int i;
    /* Parse dns request */
    char *qw_qname = NULL;
    struct hostent *pHostent = NULL;
    char pszCname[255];
    int cname_len = 0;
    struct dns_meta_data *meta;

    struct udphdr *udp = NULL;
    union dnsmsg_header *pHdr = NULL;
    NOREF(la);
    NOREF(ah);
    udp = (struct udphdr *)ip_next(pIp);
    pHdr = (union dnsmsg_header *)udp_next(udp);

    if (pHdr->X.qr == 1)
        return 0; /* this is respose */

    memset(pszCname, 0, sizeof(pszCname));
    qw_qname = (char *)&pHdr[1];
    Assert((ntohs(pHdr->X.qdcount) == 1));
    if ((ntohs(pHdr->X.qdcount) != 1))
    {
        static bool fMultiWarn;
        if (!fMultiWarn)
        {
            LogRel(("NAT:alias_dns: multiple quieries isn't supported\n"));
            fMultiWarn = true;
        }
        return 1;
    }

    for (i = 0; i < ntohs(pHdr->X.qdcount); ++i)
    {
        meta = (struct dns_meta_data *)(qw_qname + strlen(qw_qname) + 1);
        Log(("pszQname:%s qtype:%hd qclass:%hd\n",
            qw_qname, ntohs(meta->type), ntohs(meta->class)));

        QStr2CStr(qw_qname, pszCname, sizeof(pszCname));
        cname_len = RTStrNLen(pszCname, sizeof(pszCname));
        /* Some guests like win-xp adds _dot_ after host name
         * and after domain name (not passed with host resolver)
         * that confuses host resolver.
         */
        if (   cname_len > 2
            && pszCname[cname_len - 1] == '.'
            && pszCname[cname_len - 2] == '.')
        {
            pszCname[cname_len - 1] = 0;
            pszCname[cname_len - 2] = 0;
        }
        pHostent = gethostbyname(pszCname);
#ifdef VBOX_WITH_DNSMAPPING_IN_HOSTRESOLVER
        if (   pHostent
            && !LIST_EMPTY(&la->pData->DNSMapHead))
            alterHostentWithDataFromDNSMap(la->pData, pHostent);
#endif
        fprintf(stderr, "pszCname:%s\n", pszCname);
        doanswer(pHdr, meta, qw_qname, pIp, pHostent);
    }

    /*
     * We have changed the size and the content of udp, to avoid double csum calculation
     * will assign to zero
     */
    udp->uh_sum = 0;
    udp->uh_ulen = ntohs(htons(pIp->ip_len) - (pIp->ip_hl << 2));
    pIp->ip_sum = 0;
    pIp->ip_sum = LibAliasInternetChecksum(la, (uint16_t *)pIp, pIp->ip_hl << 2);
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

#ifdef VBOX_WITH_DNSMAPPING_IN_HOSTRESOLVER
static bool isDnsMappingEntryMatchOrEqual2Str(const PDNSMAPPINGENTRY pDNSMapingEntry, const char *pcszString)
{
    return (    (   pDNSMapingEntry->pszCName
                 && !strcmp(pDNSMapingEntry->pszCName, pcszString))
            || (   pDNSMapingEntry->pszPattern
                && RTStrSimplePatternMultiMatch(pDNSMapingEntry->pszPattern, RTSTR_MAX, pcszString, RTSTR_MAX, NULL)));
}

static void alterHostentWithDataFromDNSMap(PNATState pData, struct hostent *pHostent)
{
    PDNSMAPPINGENTRY pDNSMapingEntry = NULL;
    bool fMatch = false;
    LIST_FOREACH(pDNSMapingEntry, &pData->DNSMapHead, MapList)
    {
        char **pszAlias = NULL;
        if (isDnsMappingEntryMatchOrEqual2Str(pDNSMapingEntry, pHostent->h_name))
        {
            fMatch = true;
            break;
        }

        for (pszAlias = pHostent->h_aliases; *pszAlias && !fMatch; pszAlias++)
        {
            if (isDnsMappingEntryMatchOrEqual2Str(pDNSMapingEntry, *pszAlias))
            {

                PDNSMAPPINGENTRY pDnsMapping = RTMemAllocZ(sizeof(DNSMAPPINGENTRY));
                fMatch = true;
                if (!pDnsMapping)
                {
                    LogFunc(("Can't allocate DNSMAPPINGENTRY\n"));
                    LogFlowFuncLeave();
                    return;
                }
                pDnsMapping->u32IpAddress = pDNSMapingEntry->u32IpAddress;
                pDnsMapping->pszCName = RTStrDup(pHostent->h_name);
                if (!pDnsMapping->pszCName)
                {
                    LogFunc(("Can't allocate enough room for %s\n", pHostent->h_name));
                    RTMemFree(pDnsMapping);
                    LogFlowFuncLeave();
                    return;
                }
                LIST_INSERT_HEAD(&pData->DNSMapHead, pDnsMapping, MapList);
                LogRel(("NAT: user-defined mapping %s: %RTnaipv4 is registered\n",
                        pDnsMapping->pszCName ? pDnsMapping->pszCName : pDnsMapping->pszPattern,
                        pDnsMapping->u32IpAddress));
            }
        }
        if (fMatch)
            break;
    }

    /* h_lenght is lenght of h_addr_list in bytes, so we check that we have enough space for IPv4 address */
    if (   fMatch
        && pHostent->h_length >= sizeof(uint32_t)
        && pDNSMapingEntry)
    {
        pHostent->h_length = 1;
        *(uint32_t *)pHostent->h_addr_list[0] = pDNSMapingEntry->u32IpAddress;
    }

}
#endif
