/* $Id$ */
/** @file
 * Main - NetIfList, Darwin implementation.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
/*
 * Deal with conflicts first.
 * PVM - BSD mess, that FreeBSD has correct a long time ago.
 * iprt/types.h before sys/param.h - prevents UINT32_C and friends.
 */
#include <iprt/types.h>
#include <sys/param.h>
#undef PVM

#define LOG_GROUP LOG_GROUP_MAIN

#include <iprt/err.h>
#include <iprt/alloc.h>

#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <ifaddrs.h>
#include <errno.h>
#include <unistd.h>
#include <list>

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"
#include "iokit.h"
#include "Logging.h"

#if 0
int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        Log(("NetIfList: socket() -> %d\n", errno));
        return NULL;
    }
    struct ifaddrs *IfAddrs, *pAddr;
    int rc = getifaddrs(&IfAddrs);
    if (rc)
    {
        close(sock);
        Log(("NetIfList: getifaddrs() -> %d\n", rc));
        return VERR_INTERNAL_ERROR;
    }

    PDARWINETHERNIC pEtherNICs = DarwinGetEthernetControllers();
    while (pEtherNICs)
    {
        size_t cbNameLen = strlen(pEtherNICs->szName) + 1;
        PNETIFINFO pNew = (PNETIFINFO)RTMemAllocZ(RT_OFFSETOF(NETIFINFO, szName[cbNameLen]));
        pNew->MACAddress = pEtherNICs->Mac;
        pNew->enmMediumType = NETIF_T_ETHERNET;
        pNew->Uuid = pEtherNICs->Uuid;
        Assert(sizeof(pNew->szShortName) > sizeof(pEtherNICs->szBSDName));
        memcpy(pNew->szShortName, pEtherNICs->szBSDName, sizeof(pEtherNICs->szBSDName));
        pNew->szShortName[sizeof(pEtherNICs->szBSDName)] = '\0';
        memcpy(pNew->szName, pEtherNICs->szName, cbNameLen);

        struct ifreq IfReq;
        strcpy(IfReq.ifr_name, pNew->szShortName);
        if (ioctl(sock, SIOCGIFFLAGS, &IfReq) < 0)
        {
            Log(("NetIfList: ioctl(SIOCGIFFLAGS) -> %d\n", errno));
            pNew->enmStatus = NETIF_S_UNKNOWN;
        }
        else
            pNew->enmStatus = (IfReq.ifr_flags & IFF_UP) ? NETIF_S_UP : NETIF_S_DOWN;

        for (pAddr = IfAddrs; pAddr != NULL; pAddr = pAddr->ifa_next)
        {
            if (strcmp(pNew->szShortName, pAddr->ifa_name))
                continue;

            struct sockaddr_in *pIPAddr, *pIPNetMask;
            struct sockaddr_in6 *pIPv6Addr, *pIPv6NetMask;

            switch (pAddr->ifa_addr->sa_family)
            {
                case AF_INET:
                    if (pNew->IPAddress.u)
                        break;
                    pIPAddr = (struct sockaddr_in *)pAddr->ifa_addr;
                    Assert(sizeof(pNew->IPAddress) == sizeof(pIPAddr->sin_addr));
                    pNew->IPAddress.u = pIPAddr->sin_addr.s_addr;
                    pIPNetMask = (struct sockaddr_in *)pAddr->ifa_netmask;
                    Assert(pIPNetMask->sin_family == AF_INET);
                    Assert(sizeof(pNew->IPNetMask) == sizeof(pIPNetMask->sin_addr));
                    pNew->IPNetMask.u = pIPNetMask->sin_addr.s_addr;
                    break;
                case AF_INET6:
                    if (pNew->IPv6Address.s.Lo || pNew->IPv6Address.s.Hi)
                        break;
                    pIPv6Addr = (struct sockaddr_in6 *)pAddr->ifa_addr;
                    Assert(sizeof(pNew->IPv6Address) == sizeof(pIPv6Addr->sin6_addr));
                    memcpy(pNew->IPv6Address.au8,
                           pIPv6Addr->sin6_addr.__u6_addr.__u6_addr8,
                           sizeof(pNew->IPv6Address));
                    pIPv6NetMask = (struct sockaddr_in6 *)pAddr->ifa_netmask;
                    Assert(pIPv6NetMask->sin6_family == AF_INET6);
                    Assert(sizeof(pNew->IPv6NetMask) == sizeof(pIPv6NetMask->sin6_addr));
                    memcpy(pNew->IPv6NetMask.au8,
                           pIPv6NetMask->sin6_addr.__u6_addr.__u6_addr8,
                           sizeof(pNew->IPv6NetMask));
                    break;
            }
        }

        ComObjPtr<HostNetworkInterface> IfObj;
        IfObj.createObject();
        if (SUCCEEDED(IfObj->init(Bstr(pEtherNICs->szName), HostNetworkInterfaceType_Bridged, pNew)))
            list.push_back(IfObj);
        RTMemFree(pNew);

        /* next, free current */
        void *pvFree = pEtherNICs;
        pEtherNICs = pEtherNICs->pNext;
        RTMemFree(pvFree);
    }

    freeifaddrs(IfAddrs);
    close(sock);
    return VINF_SUCCESS;
}
#else

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void extractAddresses(int iAddrMask, caddr_t cp, caddr_t cplim, PNETIFINFO pInfo)
{
    struct sockaddr *sa, *addresses[RTAX_MAX];

    for (int i = 0; i < RTAX_MAX && cp < cplim; i++) {
        if (!(iAddrMask & (1 << i)))
            continue;

        sa = (struct sockaddr *)cp;

        addresses[i] = sa;
        
        ADVANCE(cp, sa);
    }

    switch (addresses[RTAX_IFA]->sa_family)
    {
        case AF_INET:
            if (!pInfo->IPAddress.u)
            {
                pInfo->IPAddress.u = ((struct sockaddr_in *)addresses[RTAX_IFA])->sin_addr.s_addr;
                pInfo->IPNetMask.u = ((struct sockaddr_in *)addresses[RTAX_NETMASK])->sin_addr.s_addr;
            }
            break;
        case AF_INET6:
            if (!pInfo->IPv6Address.s.Lo && !pInfo->IPv6Address.s.Hi)
            {
                memcpy(pInfo->IPv6Address.au8,
                       ((struct sockaddr_in6 *)addresses[RTAX_IFA])->sin6_addr.__u6_addr.__u6_addr8,
                       sizeof(pInfo->IPv6Address));
                memcpy(pInfo->IPv6NetMask.au8,
                       ((struct sockaddr_in6 *)addresses[RTAX_NETMASK])->sin6_addr.__u6_addr.__u6_addr8,
                       sizeof(pInfo->IPv6NetMask));
            }
            break;
        default:
            Log(("NetIfList: Unsupported address family: %u\n", sa->sa_family));
            break;
    }
}

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
    int rc = VINF_SUCCESS;
    size_t cbNeeded;
    char *pBuf, *pNext;
    int aiMib[6];
    
    aiMib[0] = CTL_NET;
    aiMib[1] = PF_ROUTE;
    aiMib[2] = 0;
    aiMib[3] = 0;	/* address family */
    aiMib[4] = NET_RT_IFLIST;
    aiMib[5] = 0;

    if (sysctl(aiMib, 6, NULL, &cbNeeded, NULL, 0) < 0)
    {
        Log(("NetIfList: Failed to get estimate for list size (errno=%d).\n", errno));
        return RTErrConvertFromErrno(errno);
    }
    if ((pBuf = (char*)malloc(cbNeeded)) == NULL)
        return VERR_NO_MEMORY;
    if (sysctl(aiMib, 6, pBuf, &cbNeeded, NULL, 0) < 0)
    {
        free(pBuf);
        Log(("NetIfList: Failed to retrieve interface table (errno=%d).\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        free(pBuf);
        Log(("NetIfList: socket() -> %d\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    PDARWINETHERNIC pNIC;
    PDARWINETHERNIC pEtherNICs = DarwinGetEthernetControllers();

    char *pEnd = pBuf + cbNeeded;
    for (pNext = pBuf; pNext < pEnd;)
    {
        struct if_msghdr *pIfMsg = (struct if_msghdr *)pNext;

        if (pIfMsg->ifm_type != RTM_IFINFO)
        {
            Log(("NetIfList: Got message %u while expecting %u.\n",
                 pIfMsg->ifm_type, RTM_IFINFO));
            rc = VERR_INTERNAL_ERROR;
            break;
        }
        struct sockaddr_dl *pSdl = (struct sockaddr_dl *)(pIfMsg + 1);

        size_t cbNameLen = pSdl->sdl_nlen + 1;
        for (pNIC = pEtherNICs; pNIC; pNIC = pNIC->pNext)
            if (!strncmp(pSdl->sdl_data, pNIC->szBSDName, pSdl->sdl_len))
            {
                cbNameLen = strlen(pEtherNICs->szName) + 1;
                break;
            }
        PNETIFINFO pNew = (PNETIFINFO)RTMemAllocZ(RT_OFFSETOF(NETIFINFO, szName[cbNameLen]));
        if (!pNew)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        memcpy(pNew->MACAddress.au8, LLADDR(pSdl), sizeof(pNew->MACAddress.au8));
        pNew->enmMediumType = NETIF_T_ETHERNET;
        Assert(sizeof(pNew->szShortName) >= cbNameLen);
        memcpy(pNew->szShortName, pSdl->sdl_data, cbNameLen);
        /*
         * If we found the adapter in the list returned by
         * DarwinGetEthernetControllers() copy the name and UUID from there.
         */
        if (pNIC)
        {
            memcpy(pNew->szName, pNIC->szName, cbNameLen);
            pNew->Uuid = pNIC->Uuid;
        }
        else
        {
            memcpy(pNew->szName, pSdl->sdl_data, cbNameLen);
            /* Generate UUID from name and MAC address. */
            RTUUID uuid;
            RTUuidClear(&uuid);
            memcpy(&uuid, pNew->szShortName, RT_MIN(cbNameLen, sizeof(uuid)));
            uuid.Gen.u8ClockSeqHiAndReserved = (uuid.Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
            uuid.Gen.u16TimeHiAndVersion = (uuid.Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
            memcpy(uuid.Gen.au8Node, pNew->MACAddress.au8, sizeof(uuid.Gen.au8Node));
            pNew->Uuid = uuid;
        }

        pNext += pIfMsg->ifm_msglen;
        while (pNext < pEnd)
        {
            struct ifa_msghdr *pIfAddrMsg = (struct ifa_msghdr *)pNext;

            if (pIfAddrMsg->ifam_type != RTM_NEWADDR)
                break;
            extractAddresses(pIfAddrMsg->ifam_addrs, (char *)(pIfAddrMsg + 1), pIfAddrMsg->ifam_msglen + (char *)pIfAddrMsg, pNew);
            pNext += pIfAddrMsg->ifam_msglen;
        }

        if (pSdl->sdl_type == IFT_ETHER)
        {
            struct ifreq IfReq;
            strcpy(IfReq.ifr_name, pNew->szShortName);
            if (ioctl(sock, SIOCGIFFLAGS, &IfReq) < 0)
            {
                Log(("NetIfList: ioctl(SIOCGIFFLAGS) -> %d\n", errno));
                pNew->enmStatus = NETIF_S_UNKNOWN;
            }
            else
                pNew->enmStatus = (IfReq.ifr_flags & IFF_UP) ? NETIF_S_UP : NETIF_S_DOWN;

            HostNetworkInterfaceType_T enmType;
            if (strncmp("vboxnet", pNew->szName, 7))
                enmType = HostNetworkInterfaceType_Bridged;
            else
                enmType = HostNetworkInterfaceType_HostOnly;

            ComObjPtr<HostNetworkInterface> IfObj;
            IfObj.createObject();
            if (SUCCEEDED(IfObj->init(Bstr(pNew->szName), enmType, pNew)))
                list.push_back(IfObj);
        }
        RTMemFree(pNew);
    }
    for (pNIC = pEtherNICs; pNIC;)
    {
        void *pvFree = pNIC;
        pNIC = pNIC->pNext;
        RTMemFree(pvFree);
    }
    close(sock);
    free(pBuf);
    return rc;
}

int NetIfGetConfigByName(PNETIFINFO pInfo)
{
    int rc = VINF_SUCCESS;
    size_t cbNeeded;
    char *pBuf, *pNext;
    int aiMib[6];
    
    aiMib[0] = CTL_NET;
    aiMib[1] = PF_ROUTE;
    aiMib[2] = 0;
    aiMib[3] = 0;	/* address family */
    aiMib[4] = NET_RT_IFLIST;
    aiMib[5] = 0;

    if (sysctl(aiMib, 6, NULL, &cbNeeded, NULL, 0) < 0)
    {
        Log(("NetIfList: Failed to get estimate for list size (errno=%d).\n", errno));
        return RTErrConvertFromErrno(errno);
    }
    if ((pBuf = (char*)malloc(cbNeeded)) == NULL)
        return VERR_NO_MEMORY;
    if (sysctl(aiMib, 6, pBuf, &cbNeeded, NULL, 0) < 0)
    {
        free(pBuf);
        Log(("NetIfList: Failed to retrieve interface table (errno=%d).\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        free(pBuf);
        Log(("NetIfList: socket() -> %d\n", errno));
        return RTErrConvertFromErrno(errno);
    }

    char *pEnd = pBuf + cbNeeded;
    for (pNext = pBuf; pNext < pEnd;)
    {
        struct if_msghdr *pIfMsg = (struct if_msghdr *)pNext;

        if (pIfMsg->ifm_type != RTM_IFINFO)
        {
            Log(("NetIfList: Got message %u while expecting %u.\n",
                 pIfMsg->ifm_type, RTM_IFINFO));
            rc = VERR_INTERNAL_ERROR;
            break;
        }
        struct sockaddr_dl *pSdl = (struct sockaddr_dl *)(pIfMsg + 1);

        bool fSkip = !!strcmp(pInfo->szShortName, pSdl->sdl_data);

        pNext += pIfMsg->ifm_msglen;
        while (pNext < pEnd)
        {
            struct ifa_msghdr *pIfAddrMsg = (struct ifa_msghdr *)pNext;

            if (pIfAddrMsg->ifam_type != RTM_NEWADDR)
                break;
            if (!fSkip)
                extractAddresses(pIfAddrMsg->ifam_addrs, (char *)(pIfAddrMsg + 1), pIfAddrMsg->ifam_msglen + (char *)pIfAddrMsg, pInfo);
            pNext += pIfAddrMsg->ifam_msglen;
        }

        if (!fSkip && pSdl->sdl_type == IFT_ETHER)
        {
            size_t cbNameLen = pSdl->sdl_nlen + 1;
            memcpy(pInfo->MACAddress.au8, LLADDR(pSdl), sizeof(pInfo->MACAddress.au8));
            pInfo->enmMediumType = NETIF_T_ETHERNET;
            /* Generate UUID from name and MAC address. */
            RTUUID uuid;
            RTUuidClear(&uuid);
            memcpy(&uuid, pInfo->szShortName, RT_MIN(cbNameLen, sizeof(uuid)));
            uuid.Gen.u8ClockSeqHiAndReserved = (uuid.Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
            uuid.Gen.u16TimeHiAndVersion = (uuid.Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
            memcpy(uuid.Gen.au8Node, pInfo->MACAddress.au8, sizeof(uuid.Gen.au8Node));
            pInfo->Uuid = uuid;

            struct ifreq IfReq;
            strcpy(IfReq.ifr_name, pInfo->szShortName);
            if (ioctl(sock, SIOCGIFFLAGS, &IfReq) < 0)
            {
                Log(("NetIfList: ioctl(SIOCGIFFLAGS) -> %d\n", errno));
                pInfo->enmStatus = NETIF_S_UNKNOWN;
            }
            else
                pInfo->enmStatus = (IfReq.ifr_flags & IFF_UP) ? NETIF_S_UP : NETIF_S_DOWN;

            return VINF_SUCCESS;
        }
    }
    close(sock);
    free(pBuf);
    return rc;
}

#endif
