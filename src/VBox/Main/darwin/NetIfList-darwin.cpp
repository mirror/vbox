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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN

#include <iprt/err.h>
#include <iprt/alloc.h>

#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <errno.h>
#include <unistd.h>

#include "netif.h"
#include "iokit.h"
#include "Logging.h"

PNETIFINFO NetIfList()
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
        return NULL;
    }

    PNETIFINFO pIfs = NULL;
    PNETIFINFO pPrev = NULL;
    PDARWINETHERNIC pEtherNICs = DarwinGetEthernetControllers();
    while (pEtherNICs)
    {
        size_t cbNameLen = strlen(pEtherNICs->szName) + 1;
        PNETIFINFO pNew = (PNETIFINFO)RTMemAllocZ(RT_OFFSETOF(NETIFINFO, szName[cbNameLen]));
        pNew->MACAddress = pEtherNICs->Mac;
        pNew->enmType = NETIF_T_ETHERNET;
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

        if (pIfs)
            pPrev->pNext = pNew;
        else
            pIfs = pNew;
        pPrev = pNew;

        /* next, free current */
        void *pvFree = pEtherNICs;
        pEtherNICs = pEtherNICs->pNext;
        RTMemFree(pvFree);
    }

    freeifaddrs(IfAddrs);
    close(sock);
    return pIfs;
}
