/* $Id$ */
/** @file
 * Main - NetIfList, Linux implementation.
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
#define LOG_GROUP LOG_GROUP_MAIN

#include <iprt/err.h>
#include <list>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <iprt/asm.h>

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"
#include "Logging.h"

static int getInterfaceInfo(int iSocket, const char *pszName, PNETIFINFO pInfo)
{
    memset(pInfo, 0, sizeof(*pInfo));
    struct ifreq Req;
    memset(&Req, 0, sizeof(Req));
    strncpy(Req.ifr_name, pszName, sizeof(Req.ifr_name) - 1);
    if (ioctl(iSocket, SIOCGIFHWADDR, &Req) >= 0)
    {
        switch (Req.ifr_hwaddr.sa_family)
        {
            case ARPHRD_ETHER:
                pInfo->enmMediumType = NETIF_T_ETHERNET;
                break;
            default:
                pInfo->enmMediumType = NETIF_T_UNKNOWN;
                break;
        }
        /* Generate UUID from name and MAC address. */
        RTUUID uuid;
        RTUuidClear(&uuid);
        memcpy(&uuid, Req.ifr_name, RT_MIN(sizeof(Req.ifr_name), sizeof(uuid)));
        uuid.Gen.u8ClockSeqHiAndReserved = (uuid.Gen.u8ClockSeqHiAndReserved & 0x3f) | 0x80;
        uuid.Gen.u16TimeHiAndVersion = (uuid.Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;
        memcpy(uuid.Gen.au8Node, &Req.ifr_hwaddr.sa_data, sizeof(uuid.Gen.au8Node));
        pInfo->Uuid = uuid;

        memcpy(&pInfo->MACAddress, Req.ifr_hwaddr.sa_data, sizeof(pInfo->MACAddress));

        if (ioctl(iSocket, SIOCGIFADDR, &Req) >= 0)
            memcpy(pInfo->IPAddress.au8,
                   &((struct sockaddr_in *)&Req.ifr_addr)->sin_addr.s_addr,
                   sizeof(pInfo->IPAddress.au8));

        if (ioctl(iSocket, SIOCGIFNETMASK, &Req) >= 0)
            memcpy(pInfo->IPNetMask.au8,
                   &((struct sockaddr_in *)&Req.ifr_addr)->sin_addr.s_addr,
                   sizeof(pInfo->IPNetMask.au8));

        if (ioctl(iSocket, SIOCGIFFLAGS, &Req) >= 0)
            pInfo->enmStatus = Req.ifr_flags & IFF_UP ? NETIF_S_UP : NETIF_S_DOWN;

        FILE *fp = fopen("/proc/net/if_inet6", "r");
        if (fp)
        {
            RTNETADDRIPV6 IPv6Address;
            unsigned uIndex, uLength, uScope, uTmp;
            char szName[30];
            for (;;)
            {
                memset(szName, 0, sizeof(szName));
                int n = fscanf(fp,
                               "%08x%08x%08x%08x"
                               " %02x %02x %02x %02x %20s\n",
                               &IPv6Address.au32[0], &IPv6Address.au32[1],
                               &IPv6Address.au32[2], &IPv6Address.au32[3],
                               &uIndex, &uLength, &uScope, &uTmp, szName);
                if (n == EOF)
                    break;
                if (n != 9 || uLength > 128)
                {
                    Log(("getInterfaceInfo: Error while reading /proc/net/if_inet6, n=%d uLength=%u\n",
                         n, uLength));
                    break;
                }
                if (!strcmp(Req.ifr_name, szName))
                {
                    pInfo->IPv6Address.au32[0] = htonl(IPv6Address.au32[0]);
                    pInfo->IPv6Address.au32[1] = htonl(IPv6Address.au32[1]);
                    pInfo->IPv6Address.au32[2] = htonl(IPv6Address.au32[2]);
                    pInfo->IPv6Address.au32[3] = htonl(IPv6Address.au32[3]);
                    ASMBitSetRange(&pInfo->IPv6NetMask, 0, uLength);
                }
            }
            fclose(fp);
        }
    }
    return VINF_SUCCESS;
}

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
    int rc = VINF_SUCCESS;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0)
    {
        FILE *fp = fopen("/proc/net/dev", "r");
        if (fp)
        {
            char buf[256];
            while (fgets(buf, sizeof(buf), fp))
            {
                char *pszEndOfName = strchr(buf, ':');
                if (!pszEndOfName)
                    continue;
                *pszEndOfName = 0;
                int iFirstNonWS = strspn(buf, " ");
                char *pszName = buf+iFirstNonWS;
                NETIFINFO Info;
                rc = getInterfaceInfo(sock, pszName, &Info);
                if (RT_FAILURE(rc))
                    break;
                if (Info.enmMediumType == NETIF_T_ETHERNET)
                {
                    ComObjPtr<HostNetworkInterface> IfObj;
                    IfObj.createObject();

                    HostNetworkInterfaceType_T enmType;
                    if (strncmp("vboxnet", pszName, 7))
                        enmType = HostNetworkInterfaceType_Bridged;
                    else
                        enmType = HostNetworkInterfaceType_HostOnly;

                    if (SUCCEEDED(IfObj->init(Bstr(pszName), enmType, &Info)))
                        list.push_back(IfObj);
                }

            }
            fclose(fp);
        }
        close(sock);
    }

    return rc;
}
