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

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/process.h>
#include <iprt/env.h>
#include <iprt/path.h>
#include <iprt/param.h>

#include "HostNetworkInterfaceImpl.h"
#include "netif.h"

#define VBOXNETADPCTL_NAME "VBoxNetAdpCtl"

static int NetIfAdpCtl(HostNetworkInterface * pIf, char *pszAddr, char *pszMask)
{
    const char *args[] = { NULL, NULL, pszAddr, NULL, NULL, NULL };
    if (pszMask)
    {
        args[3] = "netmask";
        args[4] = pszMask;
    }

    char szAdpCtl[RTPATH_MAX];
    int rc = RTPathProgram(szAdpCtl, sizeof(szAdpCtl) - sizeof("/" VBOXNETADPCTL_NAME));
    if (RT_FAILURE(rc))
    {
        LogRel(("NetIfAdpCtl: failed to get program path, rc=%Vrc.\n", rc));
        return rc;
    }
    strcat(szAdpCtl, "/" VBOXNETADPCTL_NAME);
    args[0] = szAdpCtl;
    Bstr interfaceName;
    pIf->COMGETTER(Name)(interfaceName.asOutParam());
    Utf8Str strName(interfaceName);
    args[1] = strName;
    if (!RTPathExists(szAdpCtl))
    {
        LogRel(("NetIfAdpCtl: path %s does not exist. Failed to run " VBOXNETADPCTL_NAME " helper.\n",
                szAdpCtl));
        return VERR_FILE_NOT_FOUND;
    }

    RTPROCESS pid;
    rc = RTProcCreate(szAdpCtl, args, RTENV_DEFAULT, 0, &pid);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS Status;
        rc = RTProcWait(pid, 0, &Status);
        if (   RT_SUCCESS(rc)
            && Status.iStatus == 0
            && Status.enmReason == RTPROCEXITREASON_NORMAL)
            return VINF_SUCCESS;
    }
    else
        LogRel(("NetIfAdpCtl: failed to create process for %.\n",
                szAdpCtl));
    return rc;
}

int NetIfEnableStaticIpConfig(VirtualBox *vBox, HostNetworkInterface * pIf, ULONG ip, ULONG mask)
{
    char szAddress[16]; /* 4*3 + 3*1 + 1 */
    char szNetMask[16]; /* 4*3 + 3*1 + 1 */
    uint8_t *pu8Addr = (uint8_t *)&ip;
    uint8_t *pu8Mask = (uint8_t *)&mask;
    RTStrPrintf(szAddress, sizeof(szAddress), "%d.%d.%d.%d",
                pu8Addr[0], pu8Addr[1], pu8Addr[2], pu8Addr[3]);
    RTStrPrintf(szNetMask, sizeof(szNetMask), "%d.%d.%d.%d",
                pu8Mask[0], pu8Mask[1], pu8Mask[2], pu8Mask[3]);
    return NetIfAdpCtl(pIf, szAddress, szNetMask);
}

int NetIfEnableStaticIpConfigV6(VirtualBox *vBox, HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    char szAddress[5*8 + 1 + 5 + 1];
    RTStrPrintf(szAddress, sizeof(szAddress), "%ls/%d",
                aIPV6Address, aIPV6MaskPrefixLength);
    return NetIfAdpCtl(pIf, szAddress, NULL);
}

int NetIfEnableDynamicIpConfig(VirtualBox *vBox, HostNetworkInterface * pIf)
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
