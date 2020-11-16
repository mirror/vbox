/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, DRM client handling.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxGuestR3LibInternal.h"

#include <iprt/env.h>
#include <iprt/path.h>
#include <iprt/process.h>

/** Defines the DRM client executable (image). */
#define VBOX_DRMCLIENT_EXECUTABLE        "VBoxDRMClient"
/** Defines the guest property that defines if the DRM resizing client needs to be active or not. */
#define VBOX_DRMCLIENT_GUEST_PROP_RESIZE "/VirtualBox/GuestAdd/DRMResize"

/**
 * Returns if the DRM resizing client is needed.
 * This is achieved by querying existence of a guest property.
 *
 * @returns \c true if the DRM resizing client is needed, \c false if not.
 */
VBGLR3DECL(bool) VbglR3DrmClientIsNeeded(void)
{
    bool fStartClient = false;

#ifdef VBOX_WITH_GUEST_PROPS
    uint32_t idClient;
    int rc = VbglR3GuestPropConnect(&idClient);
    if (RT_SUCCESS(rc))
    {
        fStartClient = VbglR3GuestPropExist(idClient, VBOX_DRMCLIENT_GUEST_PROP_RESIZE /*pszPropName*/);
        VbglR3GuestPropDisconnect(idClient);
    }
#endif

    return fStartClient;
}

/**
 * Returns if the DRM resizing client already is running.
 * This is achieved by querying existence of a guest property.
 *
 * @returns \c true if the DRM resizing client is running, \c false if not.
 */
VBGLR3DECL(bool) VbglR3DrmClientIsRunning(void)
{
    return VbglR3DrmClientIsNeeded();
}

/**
 * Starts (executes) the DRM resizing client process ("VBoxDRMClient").
 *
 * @returns VBox status code.
 */
VBGLR3DECL(int) VbglR3DrmClientStart(void)
{
    char szDRMClientPath[RTPATH_MAX];
    int rc = RTPathExecDir(szDRMClientPath, RTPATH_MAX);
    if (RT_SUCCESS(rc))
    {
        RTPathStripSuffix(szDRMClientPath);
        rc = RTPathAppend(szDRMClientPath, RTPATH_MAX, VBOX_DRMCLIENT_EXECUTABLE);
        if (RT_SUCCESS(rc))
        {
            const char *apszArgs[1] = { NULL }; /** @todo r=andy Pass path + process name as argv0? */
            rc = RTProcCreate(VBOX_DRMCLIENT_EXECUTABLE, apszArgs, RTENV_DEFAULT,
                              RTPROC_FLAGS_DETACHED | RTPROC_FLAGS_SEARCH_PATH, NULL);
        }
   }

   return rc;
}

