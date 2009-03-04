/* $Id$ */
/** @file
 * Main - NetIfList, OS/2 implementation.
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

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableStaticIpConfig(HostNetworkInterface * pIf, ULONG ip, ULONG mask, ULONG gw)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableStaticIpConfigV6(HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength, IN_BSTR aIPV6DefaultGateway)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableDynamicIpConfig(HostNetworkInterface * pIf)
{
    return VERR_NOT_IMPLEMENTED;
}

