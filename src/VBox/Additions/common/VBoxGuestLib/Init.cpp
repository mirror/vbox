/* $Revision$ */
/** @file
 * VBoxGuestLibR0 - Library initialization.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define VBGL_DECL_DATA
#include "VBGLInternal.h"

#include <iprt/string.h>
#include <iprt/assert.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The global VBGL instance data.  */
VBGLDATA g_vbgldata;

/**
 * Used by vbglQueryVMMDevPort and VbglInit to try get the host feature mask and
 * version information (g_vbgldata::hostVersion).
 *
 * This was first implemented by the host in 3.1 and we quietly ignore failures
 * for that reason.
 */
static void vbglR0QueryHostVersion (void)
{
    VMMDevReqHostVersion *pReq;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **) &pReq, sizeof (*pReq), VMMDevReq_GetHostVersion);

    if (RT_SUCCESS (rc))
    {
        rc = VbglGRPerform (&pReq->header);

        if (RT_SUCCESS (rc))
        {
            g_vbgldata.hostVersion = *pReq;
            Log (("vbglR0QueryHostVersion: %u.%u.%ur%u %#x\n",
                  pReq->major, pReq->minor, pReq->build, pReq->revision, pReq->features));
        }

        VbglGRFree (&pReq->header);
    }
}

#ifndef VBGL_VBOXGUEST
/**
 * The guest library uses lazy initialization for VMMDev port and memory,
 * because these values are provided by the VBoxGuest driver and it might
 * be loaded later than other drivers.
 *
 * The VbglEnter checks the current library status, tries to retrieve these
 * values and fails if they are unavailable.
 *
 */
static void vbglQueryVMMDevPort (void)
{
    int rc = VINF_SUCCESS;

    VBGLDRIVER driver;

    rc = vbglDriverOpen (&driver);

    if (RT_SUCCESS(rc))
    {
        /*
         * Try query the port info.
         */
        VBoxGuestPortInfo port;

        rc = vbglDriverIOCtl (&driver, VBOXGUEST_IOCTL_GETVMMDEVPORT, &port, sizeof (port));

        if (RT_SUCCESS (rc))
        {
            dprintf (("port = 0x%04X, mem = %p\n", port.portAddress, port.pVMMDevMemory));

            g_vbgldata.portVMMDev = port.portAddress;
            g_vbgldata.pVMMDevMemory = port.pVMMDevMemory;

            g_vbgldata.status = VbglStatusReady;

            vbglR0QueryHostVersion();
        }

        vbglDriverClose (&driver);
    }

    dprintf (("vbglQueryVMMDevPort rc = %d\n", rc));
}
#endif /* !VBGL_VBOXGUEST */

/**
 * Checks if VBGL has been initialized.
 *
 * The client library, this will lazily complete the initialization.
 *
 * @return VINF_SUCCESS or VERR_VBGL_NOT_INITIALIZED.
 */
int vbglR0Enter (void)
{
    int rc;

#ifndef VBGL_VBOXGUEST
    if (g_vbgldata.status == VbglStatusInitializing)
    {
        vbglQueryVMMDevPort ();
    }
#endif

    rc = g_vbgldata.status == VbglStatusReady? VINF_SUCCESS: VERR_VBGL_NOT_INITIALIZED;

    // dprintf(("VbglEnter: rc = %d\n", rc));

    return rc;
}

int vbglInitCommon (void)
{
    int rc = VINF_SUCCESS;

    RT_ZERO(g_vbgldata);

    g_vbgldata.status = VbglStatusInitializing;

    rc = VbglPhysHeapInit ();

    if (RT_SUCCESS(rc))
    {
        /* other subsystems, none yet */
        ;
    }
    else
        LogRel(("vbglInitCommon: VbglPhysHeapInit failed. rc=%Rrc\n", rc));

    dprintf(("vbglInitCommon: rc = %d\n", rc));

    return rc;
}

DECLVBGL(void) vbglTerminateCommon (void)
{
    VbglPhysHeapTerminate ();

    RT_ZERO(g_vbgldata);

    return;
}

#ifdef VBGL_VBOXGUEST

DECLVBGL(int) VbglInit (VBGLIOPORT portVMMDev, VMMDevMemory *pVMMDevMemory)
{
    int rc = VINF_SUCCESS;

# ifdef RT_OS_WINDOWS /** @todo r=bird: this doesn't make sense. Is there something special going on on windows? */
    dprintf(("vbglInit: starts g_vbgldata.status %d\n", g_vbgldata.status));

    if (g_vbgldata.status == VbglStatusInitializing
        || g_vbgldata.status == VbglStatusReady)
    {
        /* Initialization is already in process. */
        return rc;
    }
# else
    dprintf(("vbglInit: starts\n"));
# endif

    rc = vbglInitCommon ();

    if (RT_SUCCESS(rc))
    {
        g_vbgldata.portVMMDev = portVMMDev;
        g_vbgldata.pVMMDevMemory = pVMMDevMemory;

        g_vbgldata.status = VbglStatusReady;

        vbglR0QueryHostVersion();
    }
    else
    {
        g_vbgldata.status = VbglStatusNotInitialized;
    }

    return rc;
}

DECLVBGL(void) VbglTerminate (void)
{
    vbglTerminateCommon ();

    return;
}


#else /* !VBGL_VBOXGUEST */

DECLVBGL(int) VbglInit (void)
{
    int rc = VINF_SUCCESS;

    if (g_vbgldata.status == VbglStatusInitializing
        || g_vbgldata.status == VbglStatusReady)
    {
        /* Initialization is already in process. */
        return rc;
    }

    rc = vbglInitCommon ();

    if (RT_SUCCESS(rc))
    {
        /* Try to obtain VMMDev port via IOCTL to VBoxGuest main driver. */
        vbglQueryVMMDevPort ();

# ifdef VBOX_WITH_HGCM
        rc = vbglR0HGCMInit ();
# endif /* VBOX_WITH_HGCM */

        if (RT_FAILURE(rc))
        {
            vbglTerminateCommon ();
        }
    }

    return rc;
}

DECLVBGL(void) VbglTerminate (void)
{
# ifdef VBOX_WITH_HGCM
    vbglR0HGCMTerminate ();
# endif

    vbglTerminateCommon ();

    return;
}

#endif /* !VBGL_VBOXGUEST */

