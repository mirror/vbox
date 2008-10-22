/* $Id$ */
/** @file
 * VBoxVMInfoOS - Guest OS information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

