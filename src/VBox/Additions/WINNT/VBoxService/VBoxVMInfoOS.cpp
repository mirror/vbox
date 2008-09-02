/* $Id$ */
/** @file
 * VBoxVMInfoOS - Guest OS information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "VBoxService.h"
#include "VBoxVMInfo.h"
#include "VBoxVMInfoOS.h"

#include <iprt/system.h>

int getPlatformInfo(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    if (FALSE == a_pCtx->fFirstRun)		/* Only do this at the initial run. */
        return 0;

    char szInfo[256] = {0};
    int rc = 0;

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szInfo, sizeof(szInfo));
    vboxVMInfoWriteProp(a_pCtx, "GuestInfo/OS/Product", szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szInfo, sizeof(szInfo));
    vboxVMInfoWriteProp(a_pCtx, "GuestInfo/OS/Release", szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szInfo, sizeof(szInfo));
    vboxVMInfoWriteProp(a_pCtx, "GuestInfo/OS/Version", szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szInfo, sizeof(szInfo));
    vboxVMInfoWriteProp(a_pCtx, "GuestInfo/OS/ServicePack", szInfo);

    return 0;
}

int vboxVMInfoOS(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    getPlatformInfo(a_pCtx);

    return 0;
}

