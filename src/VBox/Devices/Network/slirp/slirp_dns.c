/* $Id$ */
/** @file
 * NAT - dns initialization.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "slirp.h"
#ifdef RT_OS_OS2
# include <paths.h>
#endif

#include <VBox/err.h>
#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>

#ifdef RT_OS_WINDOWS
# include <Winnls.h>
# define _WINSOCK2API_
# include <IPHlpApi.h>

static int get_dns_addr_domain(PNATState pData,
                               const char **ppszDomain)
{
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX; /*GAA_FLAG_INCLUDE_ALL_INTERFACES;*/ /* all interfaces registered in NDIS */
    PIP_ADAPTER_ADDRESSES pAdapterAddr = NULL;
    PIP_ADAPTER_ADDRESSES pAddr = NULL;
    PIP_ADAPTER_DNS_SERVER_ADDRESS pDnsAddr = NULL;
    ULONG size;
    int wlen = 0;
    char *pszSuffix;
    struct dns_domain_entry *pDomain = NULL;
    ULONG ret = ERROR_SUCCESS;

    /* @todo add SKIPing flags to get only required information */

    /* determine size of buffer */
    size = 0;
    ret = pData->pfGetAdaptersAddresses(AF_INET, 0, NULL /* reserved */, pAdapterAddr, &size);
    if (ret != ERROR_BUFFER_OVERFLOW)
    {
        Log(("NAT: error %lu occurred on capacity detection operation\n", ret));
        return -1;
    }
    if (size == 0)
    {
        Log(("NAT: Win socket API returns non capacity\n"));
        return -1;
    }

    pAdapterAddr = RTMemAllocZ(size);
    if (!pAdapterAddr)
    {
        Log(("NAT: No memory available\n"));
        return -1;
    }
    ret = pData->pfGetAdaptersAddresses(AF_INET, 0, NULL /* reserved */, pAdapterAddr, &size);
    if (ret != ERROR_SUCCESS)
    {
        Log(("NAT: error %lu occurred on fetching adapters info\n", ret));
        RTMemFree(pAdapterAddr);
        return -1;
    }

    for (pAddr = pAdapterAddr; pAddr != NULL; pAddr = pAddr->Next)
    {
        int found;
        if (pAddr->OperStatus != IfOperStatusUp)
            continue;

        for (pDnsAddr = pAddr->FirstDnsServerAddress; pDnsAddr != NULL; pDnsAddr = pDnsAddr->Next)
        {
            struct sockaddr *SockAddr = pDnsAddr->Address.lpSockaddr;
            struct in_addr  InAddr;
            struct dns_entry *pDns;

            if (SockAddr->sa_family != AF_INET)
                continue;

            InAddr = ((struct sockaddr_in *)SockAddr)->sin_addr;

            /* add dns server to list */
            pDns = RTMemAllocZ(sizeof(struct dns_entry));
            if (!pDns)
            {
                Log(("NAT: Can't allocate buffer for DNS entry\n"));
                RTMemFree(pAdapterAddr);
                return VERR_NO_MEMORY;
            }

            Log(("NAT: adding %RTnaipv4 to DNS server list\n", InAddr));
            if ((InAddr.s_addr & RT_H2N_U32_C(IN_CLASSA_NET)) == RT_N2H_U32_C(INADDR_LOOPBACK & IN_CLASSA_NET))
                pDns->de_addr.s_addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | CTL_ALIAS);
            else
                pDns->de_addr.s_addr = InAddr.s_addr;

            TAILQ_INSERT_HEAD(&pData->pDnsList, pDns, de_list);

            if (pAddr->DnsSuffix == NULL)
                continue;

            /* uniq */
            RTUtf16ToUtf8(pAddr->DnsSuffix, &pszSuffix);
            if (!pszSuffix || strlen(pszSuffix) == 0)
            {
                RTStrFree(pszSuffix);
                continue;
            }

            found = 0;
            LIST_FOREACH(pDomain, &pData->pDomainList, dd_list)
            {
                if (   pDomain->dd_pszDomain != NULL
                    && strcmp(pDomain->dd_pszDomain, pszSuffix) == 0)
                {
                    found = 1;
                    RTStrFree(pszSuffix);
                    break;
                }
            }
            if (!found)
            {
                pDomain = RTMemAllocZ(sizeof(struct dns_domain_entry));
                if (!pDomain)
                {
                    Log(("NAT: not enough memory\n"));
                    RTStrFree(pszSuffix);
                    RTMemFree(pAdapterAddr);
                    return VERR_NO_MEMORY;
                }
                pDomain->dd_pszDomain = pszSuffix;
                Log(("NAT: adding domain name %s to search list\n", pDomain->dd_pszDomain));
                LIST_INSERT_HEAD(&pData->pDomainList, pDomain, dd_list);
            }
        }
    }
    RTMemFree(pAdapterAddr);
    return 0;
}

#else /* !RT_OS_WINDOWS */

static int RTFileGets(RTFILE File, void *pvBuf, size_t cbBufSize, size_t *pcbRead)
{
    size_t cbRead;
    char bTest;
    int rc = VERR_NO_MEMORY;
    char *pu8Buf = (char *)pvBuf;
    *pcbRead = 0;

    while (   RT_SUCCESS(rc = RTFileRead(File, &bTest, 1, &cbRead))
           && (pu8Buf - (char *)pvBuf) < cbBufSize)
    {
        if (cbRead == 0)
            return VERR_EOF;

        if (bTest == '\r' || bTest == '\n')
        {
            *pu8Buf = 0;
            return VINF_SUCCESS;
        }
        *pu8Buf = bTest;
         pu8Buf++;
        (*pcbRead)++;
    }
    return rc;
}

static int slirpOpenResolvConfFile(PRTFILE pResolvConfFile)
{
    int rc;
    char buff[512];
    char *etc = NULL;
    char *home = NULL;
    AssertPtrReturn(pResolvConfFile, VERR_INVALID_PARAMETER);
# ifdef RT_OS_OS2
    /* Try various locations. */
    NOREF(home);
    etc = getenv("ETC");
    if (etc)
    {
        RTStrmPrintf(buff, sizeof(buff), "%s/RESOLV2", etc);
        rc = RTFileOpen(pResolvConfFile, buff, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    }
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(buff, sizeof(buff), "%s/RESOLV2", _PATH_ETC);
        rc = RTFileOpen(pResolvConfFile, buff, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    }
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(buff, sizeof(buff), "%s/resolv.conf", _PATH_ETC);
        rc = RTFileOpen(pResolvConfFile, buff, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    }
# else /* !RT_OS_OS2 */
#  ifndef DEBUG_vvl
    rc = RTFileOpen(pResolvConfFile, "/etc/resolv.conf", RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
#  else
    NOREF(etc);
    home = getenv("HOME");
    RTStrPrintf(buff, sizeof(buff), "%s/resolv.conf", home);
    rc = RTFileOpen(pResolvConfFile, buff, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
        Log(("NAT: DNS we're using %s\n", buff));
    else
    {
        rc = RTFileOpen(pResolvConfFile, "/etc/resolv.conf", RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        Log(("NAT: DNS we're using %s\n", buff));
    }
#  endif
# endif /* !RT_OS_OS2 */
    return rc;
}
static int get_dns_addr_domain(PNATState pData, const char **ppszDomain)
{
    char buff[256];
    char buff2[256];
    RTFILE ResolvConfFile;
    int cNameserversFound = 0;
    bool fWarnTooManyDnsServers = false;
    struct in_addr tmp_addr;
    int rc;
    size_t bytes;

    rc = slirpOpenResolvConfFile(&ResolvConfFile);
    if (RT_FAILURE(rc))
    {
        LogRel(("NAT: there're some problems with accessing resolv.conf (or known analog), thus NAT switches to use host resolver mechanism\n"));
        pData->fUseHostResolver = 1;
        return VINF_SUCCESS;
    }

    if (ppszDomain)
        *ppszDomain = NULL;

    Log(("NAT: DNS Servers:\n"));
    while (    RT_SUCCESS(rc = RTFileGets(ResolvConfFile, buff, sizeof(buff), &bytes))
            && rc != VERR_EOF)
    {
        struct dns_entry *pDns = NULL;
        if (   cNameserversFound == 4
            && !fWarnTooManyDnsServers
            && sscanf(buff, "nameserver%*[ \t]%255s", buff2) == 1)
        {
            fWarnTooManyDnsServers = true;
            LogRel(("NAT: too many nameservers registered.\n"));
        }
        if (   sscanf(buff, "nameserver%*[ \t]%255s", buff2) == 1
            && cNameserversFound < 4) /* Unix doesn't accept more than 4 name servers*/
        {
            if (!inet_aton(buff2, &tmp_addr))
                continue;

            /* localhost mask */
            pDns = RTMemAllocZ(sizeof (struct dns_entry));
            if (!pDns)
            {
                Log(("can't alloc memory for DNS entry\n"));
                return -1;
            }

            /* check */
            pDns->de_addr.s_addr = tmp_addr.s_addr;
            if ((pDns->de_addr.s_addr & RT_H2N_U32_C(IN_CLASSA_NET)) == RT_N2H_U32_C(INADDR_LOOPBACK & IN_CLASSA_NET))
            {
                if ((pDns->de_addr.s_addr) == RT_N2H_U32_C(INADDR_LOOPBACK))
                    pDns->de_addr.s_addr = RT_H2N_U32(RT_N2H_U32(pData->special_addr.s_addr) | CTL_ALIAS);
                else
                {
                    /* Modern Ubuntu register 127.0.1.1 as DNS server */
                    LogRel(("NAT: DNS server %RTnaipv4 registration detected, switching to the host resolver.\n",
                            pDns->de_addr.s_addr));
                    RTMemFree(pDns);
                    /* Releasing fetched DNS information. */
                    slirpReleaseDnsSettings(pData);
                    pData->fUseHostResolver = 1;
                    return VINF_SUCCESS;
                }
            }
            TAILQ_INSERT_HEAD(&pData->pDnsList, pDns, de_list);
            cNameserversFound++;
        }
        if ((!strncmp(buff, "domain", 6) || !strncmp(buff, "search", 6)))
        {
            char *tok;
            char *saveptr;
            struct dns_domain_entry *pDomain = NULL;
            int fFoundDomain = 0;
            tok = strtok_r(&buff[6], " \t\n", &saveptr);
            LIST_FOREACH(pDomain, &pData->pDomainList, dd_list)
            {
                if (   tok != NULL
                    && strcmp(tok, pDomain->dd_pszDomain) == 0)
                {
                    fFoundDomain = 1;
                    break;
                }
            }
            if (tok != NULL && !fFoundDomain)
            {
                pDomain = RTMemAllocZ(sizeof(struct dns_domain_entry));
                if (!pDomain)
                {
                    Log(("NAT: not enought memory to add domain list\n"));
                    return VERR_NO_MEMORY;
                }
                pDomain->dd_pszDomain = RTStrDup(tok);
                Log(("NAT: adding domain name %s to search list\n", pDomain->dd_pszDomain));
                LIST_INSERT_HEAD(&pData->pDomainList, pDomain, dd_list);
            }
        }
    }
    RTFileClose(ResolvConfFile);
    if (!cNameserversFound)
        return -1;
    return 0;
}

#endif /* !RT_OS_WINDOWS */

int slirpInitializeDnsSettings(PNATState pData)
{
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);
    LogFlowFuncEnter();
    if (!pData->fUseHostResolver)
    {
        TAILQ_INIT(&pData->pDnsList);
        LIST_INIT(&pData->pDomainList);
        /**
         * Some distributions haven't got /etc/resolv.conf
         * so we should other way to configure DNS settings.
         */
        if (get_dns_addr_domain(pData, NULL) < 0)
            pData->fUseHostResolver = 1;
        else
            dnsproxy_init(pData);

        if (!pData->fUseHostResolver)
        {
            struct dns_entry *pDNSEntry = NULL;
            int cDNSListEntry = 0;
            TAILQ_FOREACH_REVERSE(pDNSEntry, &pData->pDnsList, dns_list_head, de_list)
            {
                LogRel(("NAT: DNS#%i: %RTnaipv4\n", cDNSListEntry, pDNSEntry->de_addr.s_addr));
                cDNSListEntry++;
            }
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int slirpReleaseDnsSettings(PNATState pData)
{
    struct dns_entry *pDns = NULL;
    struct dns_domain_entry *pDomain = NULL;
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pData, VERR_INVALID_PARAMETER);
    LogFlowFuncEnter();

    while (!TAILQ_EMPTY(&pData->pDnsList))
    {
        pDns = TAILQ_FIRST(&pData->pDnsList);
        TAILQ_REMOVE(&pData->pDnsList, pDns, de_list);
        RTMemFree(pDns);
    }

    while (!LIST_EMPTY(&pData->pDomainList))
    {
        pDomain = LIST_FIRST(&pData->pDomainList);
        LIST_REMOVE(pDomain, dd_list);
        if (pDomain->dd_pszDomain != NULL)
            RTStrFree(pDomain->dd_pszDomain);
        RTMemFree(pDomain);
    }
    LogFlowFuncLeaveRC(rc);
    return rc;
}
