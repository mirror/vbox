/* $Id$ */
/** @file
 * Host resolver
 */

/*
 * Copyright (C) 2009-2015 Oracle Corporation
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
static void CStr2QStr(const char *pcszStr, char *pszQStr, size_t cQStr);
static void QStr2CStr(const char *pcszQStr, char *pszStr, size_t cStr);
#ifdef VBOX_WITH_DNSMAPPING_IN_HOSTRESOLVER
static void alterHostentWithDataFromDNSMap(PNATState pData, struct hostent *pHostent);
#endif


static void
doanswer(struct mbuf *m, struct hostent *pHostent)
{
    union dnsmsg_header *pHdr;
    int i;

    pHdr = mtod(m, union dnsmsg_header *);

    if (!pHostent)
    {
        pHdr->X.qr = 1; /* response */
        pHdr->X.aa = 1;
        pHdr->X.rd = 1;
        pHdr->X.rcode = 3;
    }
    else
    {
        char *answers;
        uint16_t off;
        char **cstr;
        char *c;
        size_t anslen = 0;
        uint16_t addr_off = (uint16_t)~0;
        struct dns_meta_data *meta;

        /* answers zone lays after query in response packet */
        answers = (char *)pHdr + m->m_len;

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
            anslen += sizeof(struct dnsmsg_answer) + len - 2;
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
            anslen += sizeof(struct dnsmsg_answer) + 3;
            pHdr->X.ancount++;
        }
        pHdr->X.qr = 1; /* response */
        pHdr->X.aa = 1;
        pHdr->X.rd = 1;
        pHdr->X.ra = 1;
        pHdr->X.rcode = 0;
        HTONS(pHdr->X.ancount);

        m->m_len += anslen;
    }
}

int
hostresolver(PNATState pData, struct mbuf *m)
{
    int i;
    /* Parse dns request */
    char *qw_qname = NULL;
    struct hostent *pHostent = NULL;
    char pszCname[255];
    int cname_len = 0;
    struct dns_meta_data *meta;

    union dnsmsg_header *pHdr = NULL;

    pHdr = mtod(m, union dnsmsg_header *);

    if (pHdr->X.qr == 1)
        return 1; /* this is respose */

    memset(pszCname, 0, sizeof(pszCname));
    qw_qname = (char *)&pHdr[1];

    if ((ntohs(pHdr->X.qdcount) != 1))
    {
        static bool fMultiWarn;
        if (!fMultiWarn)
        {
            LogRel(("NAT: alias_dns: Multiple queries isn't supported\n"));
            fMultiWarn = true;
        }
        return 1;
    }

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
            && !LIST_EMPTY(&pData->DNSMapHead))
            alterHostentWithDataFromDNSMap(pData, pHostent);
#endif
        doanswer(m, pHostent);
    }

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
                LogRel(("NAT: User-defined mapping %s: %RTnaipv4 is registered\n",
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
