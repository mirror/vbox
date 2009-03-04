/** @file
 * Main - Network Interfaces.
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

#ifndef ___netif_h
#define ___netif_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/net.h>

//#include "VBox/com/ptr.h"
//#include <list>

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
    RTNETADDRIPV4  IPDefaultGateway;
    RTNETADDRIPV6  IPv6Address;
    RTNETADDRIPV6  IPv6NetMask;
    RTNETADDRIPV6  IPV6DefaultGateway;
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
int NetIfEnableStaticIpConfig(HostNetworkInterface * pIf, ULONG ip, ULONG mask, ULONG gw);
int NetIfEnableStaticIpConfigV6(HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength, IN_BSTR aIPV6DefaultGateway);
int NetIfEnableDynamicIpConfig(HostNetworkInterface * pIf);

#endif

