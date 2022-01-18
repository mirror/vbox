/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, DRM client handling.
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
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

#if defined(RT_OS_LINUX)

/** Defines the DRM client executable (image). */
# define VBOX_DRMCLIENT_EXECUTABLE           "/usr/bin/VBoxDRMClient"
# define VBOX_DRMCLIENT_LEGACY_EXECUTABLE    "/usr/bin/VBoxClient"
/** Defines the guest property that defines if the DRM resizing client needs to be active or not. */
# define VBOX_DRMCLIENT_GUEST_PROP_RESIZE    "/VirtualBox/GuestAdd/DRMResize"

#endif /* RT_OS_LINUX */

/**
 * Returns if the DRM resizing client is needed.
 * This is achieved by querying existence of a guest property.
 *
 * @returns \c true if the DRM resizing client is needed, \c false if not.
 */
VBGLR3DECL(bool) VbglR3DrmClientIsNeeded(void)
{
#if defined(RT_OS_LINUX)
    bool fStartClient = false;

# ifdef VBOX_WITH_GUEST_PROPS
    uint32_t idClient;
    int rc = VbglR3GuestPropConnect(&idClient);
    if (RT_SUCCESS(rc))
    {
        fStartClient = VbglR3GuestPropExist(idClient, VBOX_DRMCLIENT_GUEST_PROP_RESIZE /*pszPropName*/);
        VbglR3GuestPropDisconnect(idClient);
    }
# endif
    return fStartClient;

#else /* !RT_OS_LINUX */
    return false;
#endif
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

static int VbglR3DrmStart(const char *pszCmd, const char **apszArgs)
{
#if defined(RT_OS_LINUX)
    return RTProcCreate(pszCmd, apszArgs, RTENV_DEFAULT,
                        RTPROC_FLAGS_DETACHED | RTPROC_FLAGS_SEARCH_PATH, NULL);
#else
    return VERR_NOT_SUPPORTED;
#endif
}

/**
 * Starts (executes) the DRM resizing client process ("VBoxDRMClient").
 *
 * @returns VBox status code.
 */
VBGLR3DECL(int) VbglR3DrmClientStart(void)
{
#if defined(RT_OS_LINUX)
    const char *apszArgs[1] = { NULL }; /** @todo r=andy Pass path + process name as argv0? */
    return VbglR3DrmStart(VBOX_DRMCLIENT_EXECUTABLE, apszArgs);
#else
    return VERR_NOT_SUPPORTED;
#endif
}

/**
 * Starts (executes) the legacy DRM resizing client process ("VBoxClient --vmsvga").
 *
 * @returns VBox status code.
 */
VBGLR3DECL(int) VbglR3DrmLegacyClientStart(void)
{
#if defined(RT_OS_LINUX)
    const char *apszArgs[3] = { VBOX_DRMCLIENT_LEGACY_EXECUTABLE, "--vmsvga", NULL };
    return VbglR3DrmStart(VBOX_DRMCLIENT_LEGACY_EXECUTABLE, apszArgs);
#else
    return VERR_NOT_SUPPORTED;
#endif
}

