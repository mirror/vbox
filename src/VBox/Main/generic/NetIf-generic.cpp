/* $Id$ */
/** @file
 * VirtualBox Main - Generic NetIf implementation.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"

int NetIfEnableStaticIpConfig(HostNetworkInterface * pIf, ULONG ip, ULONG mask)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableStaticIpConfigV6(HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableDynamicIpConfig(HostNetworkInterface * pIf)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfCreateHostOnlyNetworkInterface (VirtualBox *pVbox, IHostNetworkInterface **aHostNetworkInterface, IProgress **aProgress)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfRemoveHostOnlyNetworkInterface (VirtualBox *pVbox, IN_GUID aId, IHostNetworkInterface **aHostNetworkInterface, IProgress **aProgress)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfGetConfig(HostNetworkInterface * pIf, NETIFINFO *)
{
    return VERR_NOT_IMPLEMENTED;
}
