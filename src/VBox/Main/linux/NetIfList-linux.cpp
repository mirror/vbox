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
#include <list>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0)
    {
        char pBuffer[2048];
        struct ifconf ifConf;
        ifConf.ifc_len = sizeof(pBuffer);
        ifConf.ifc_buf = pBuffer;
        if (ioctl(sock, SIOCGIFCONF, &ifConf) >= 0)
        {
            for (struct ifreq *pReq = ifConf.ifc_req; (char*)pReq < pBuffer + ifConf.ifc_len; pReq++)
            {
                if (ioctl(sock, SIOCGIFHWADDR, pReq) >= 0)
                {
                    if (pReq->ifr_hwaddr.sa_family == ARPHRD_ETHER)
                    {
                        NETIFINFO Info;
                        memset(&Info, 0, sizeof(Info));
                        Info.enmType = NETIF_T_ETHERNET;
                        /* Pick up some garbage from stack. */
                        RTUUID uuid;
                        Assert(sizeof(uuid) <= sizeof(*pReq));
                        memcpy(uuid.Gen.au8Node, &pReq->ifr_hwaddr.sa_data, sizeof(uuid.Gen.au8Node));
                        Info.Uuid = uuid;
                        memcpy(&Info.MACAddress, pReq->ifr_hwaddr.sa_data, sizeof(Info.MACAddress));

                        if (ioctl(sock, SIOCGIFADDR, pReq) >= 0)
                            memcpy(Info.IPAddress.au8,
                                   &((struct sockaddr_in *)&pReq->ifr_addr)->sin_addr.s_addr,
                                   sizeof(Info.IPAddress.au8));

                        if (ioctl(sock, SIOCGIFNETMASK, pReq) >= 0)
                            memcpy(Info.IPNetMask.au8,
                                   &((struct sockaddr_in *)&pReq->ifr_addr)->sin_addr.s_addr,
                                   sizeof(Info.IPNetMask.au8));

                        if (ioctl(sock, SIOCGIFFLAGS, pReq) >= 0)
                            Info.enmStatus = pReq->ifr_flags & IFF_UP ? NETIF_S_UP : NETIF_S_DOWN;

                        FILE *fp = fopen("/proc/net/if_inet6", "r");
                        if (fp)
                        {
                            RTNETADDRIPV6 IPv6Address;
                            unsigned uIndex, uLength, uScope, uTmp;
                            char szName[30];
                            while (fscanf(fp,
                                          "%08x%08x%08x%08x"
                                          " %02x %02x %02x %02x %20s\n",
                                          &IPv6Address.au32[0], &IPv6Address.au32[1],
                                          &IPv6Address.au32[2], &IPv6Address.au32[3],
                                          &uIndex, &uLength, &uScope, &uTmp, szName) != EOF)
                            {
                                if (!strcmp(pReq->ifr_name, szName))
                                {
                                    Info.IPv6Address.au32[0] = htonl(IPv6Address.au32[0]);
                                    Info.IPv6Address.au32[1] = htonl(IPv6Address.au32[1]);
                                    Info.IPv6Address.au32[2] = htonl(IPv6Address.au32[2]);
                                    Info.IPv6Address.au32[3] = htonl(IPv6Address.au32[3]);
                                    ASMBitSetRange(&Info.IPv6NetMask, 0, uLength);
                                }
                            }
                            fclose(fp);
                        }
                        ComObjPtr<HostNetworkInterface> IfObj;
                        IfObj.createObject();
                        if (SUCCEEDED(IfObj->init(Bstr(pReq->ifr_name), &Info)))
                            list.push_back(IfObj);
                    }
                }
            }
        }
        close(sock);
    }
    return VINF_SUCCESS;
}

