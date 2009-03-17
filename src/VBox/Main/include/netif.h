/** @file
 * Main - Network Interfaces.
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#ifndef ___netif_h
#define ___netif_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/net.h>
#include <iprt/asm.h>

#define VBOXNET_IPV4ADDR_DEFAULT "192.168.56.1"
#define VBOXNET_IPV4MASK_DEFAULT "255.255.255.0"

#if 1
/**
 * Encapsulation type.
 */
typedef enum NETIFTYPE
{
    NETIF_T_UNKNOWN,
    NETIF_T_ETHERNET,
    NETIF_T_PPP,
    NETIF_T_SLIP
} NETIFTYPE;

/**
 * Current state of the interface.
 */
typedef enum NETIFSTATUS
{
    NETIF_S_UNKNOWN,
    NETIF_S_UP,
    NETIF_S_DOWN
} NETIFSTATUS;

/**
 * Host Network Interface Information.
 */
typedef struct NETIFINFO
{
    NETIFINFO     *pNext;
    RTNETADDRIPV4  IPAddress;
    RTNETADDRIPV4  IPNetMask;
    RTNETADDRIPV6  IPv6Address;
    RTNETADDRIPV6  IPv6NetMask;
    BOOL           bDhcpEnabled;
    RTMAC          MACAddress;
    NETIFTYPE      enmMediumType;
    NETIFSTATUS    enmStatus;
    RTUUID         Uuid;
    char           szShortName[50];
    char           szName[1];
} NETIFINFO;

/** Pointer to a network interface info. */
typedef NETIFINFO *PNETIFINFO;
/** Pointer to a const network interface info. */
typedef NETIFINFO const *PCNETIFINFO;
#endif

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list);
int NetIfEnableStaticIpConfig(VirtualBox *pVbox, HostNetworkInterface * pIf, ULONG aOldIp, ULONG aNewIp, ULONG aMask);
int NetIfEnableStaticIpConfigV6(VirtualBox *pVbox, HostNetworkInterface * pIf, IN_BSTR aOldIPV6Address, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength);
int NetIfEnableDynamicIpConfig(VirtualBox *pVbox, HostNetworkInterface * pIf);
int NetIfCreateHostOnlyNetworkInterface (VirtualBox *pVbox, IHostNetworkInterface **aHostNetworkInterface, IProgress **aProgress);
int NetIfRemoveHostOnlyNetworkInterface (VirtualBox *pVbox, IN_GUID aId, IHostNetworkInterface **aHostNetworkInterface, IProgress **aProgress);
int NetIfGetConfig(HostNetworkInterface * pIf, NETIFINFO *);
int NetIfDhcpRediscover(VirtualBox *pVbox, HostNetworkInterface * pIf);

DECLINLINE(Bstr) composeIPv6Address(PRTNETADDRIPV6 aAddrPtr)
{
    char szTmp[8*5] = "";

    if (aAddrPtr->s.Lo || aAddrPtr->s.Hi)
        RTStrPrintf(szTmp, sizeof(szTmp),
                    "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                    "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                    aAddrPtr->au8[0], aAddrPtr->au8[1],
                    aAddrPtr->au8[2], aAddrPtr->au8[3],
                    aAddrPtr->au8[4], aAddrPtr->au8[5],
                    aAddrPtr->au8[6], aAddrPtr->au8[7],
                    aAddrPtr->au8[8], aAddrPtr->au8[9],
                    aAddrPtr->au8[10], aAddrPtr->au8[11],
                    aAddrPtr->au8[12], aAddrPtr->au8[13],
                    aAddrPtr->au8[14], aAddrPtr->au8[15]);
    return Bstr(szTmp);
}

DECLINLINE(ULONG) composeIPv6PrefixLenghFromAddress(PRTNETADDRIPV6 aAddrPtr)
{
    int res = ASMBitFirstClear(aAddrPtr, sizeof(RTNETADDRIPV6)*8);
    return res != -1 ? res : 128;
}

DECLINLINE(int) prefixLength2IPv6Address(ULONG cPrefix, PRTNETADDRIPV6 aAddrPtr)
{
    if(cPrefix > 128)
        return VERR_INVALID_PARAMETER;
    if(!aAddrPtr)
        return VERR_INVALID_PARAMETER;

    memset(aAddrPtr, 0, sizeof(RTNETADDRIPV6));

    ASMBitSetRange(aAddrPtr, 0, cPrefix);

    return VINF_SUCCESS;
}

DECLINLINE(Bstr) composeHardwareAddress(PRTMAC aMacPtr)
{
    char szTmp[6*3];

    RTStrPrintf(szTmp, sizeof(szTmp),
                "%02x:%02x:%02x:%02x:%02x:%02x",
                aMacPtr->au8[0], aMacPtr->au8[1],
                aMacPtr->au8[2], aMacPtr->au8[3],
                aMacPtr->au8[4], aMacPtr->au8[5]);
    return Bstr(szTmp);
}

#endif  /* ___netif_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
