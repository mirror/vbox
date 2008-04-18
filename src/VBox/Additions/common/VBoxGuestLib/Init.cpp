/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Library initialization
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


#include <VBox/VBoxGuestLib.h>

#define VBGL_DECL_DATA
#include "VBGLInternal.h"

#include <iprt/string.h>
#include <iprt/assert.h>

VBGLDATA g_vbgldata;

#ifndef VBGL_VBOXGUEST
/* The guest library uses lazy initialization for VMMDev port and memory,
 * because these values are provided by the VBoxGuest driver and it might
 * be loaded later than other drivers.
 * The VbglEnter checks the current library status, tries to retrive these
 * values and fails if they are unavailable.
 */
static void vbglQueryVMMDevPort (void)
{
    int rc = VINF_SUCCESS;

    VBGLDRIVER driver;

    rc = vbglDriverOpen (&driver);

    if (VBOX_SUCCESS(rc))
    {
        VBoxGuestPortInfo port;

        rc = vbglDriverIOCtl (&driver, IOCTL_VBOXGUEST_GETVMMDEVPORT, &port, sizeof (port));

        if (VBOX_SUCCESS (rc))
        {
            dprintf (("port = 0x%04X, mem = %p\n", port.portAddress, port.pVMMDevMemory));
            
            g_vbgldata.portVMMDev = port.portAddress;
            g_vbgldata.pVMMDevMemory = port.pVMMDevMemory;

            g_vbgldata.status = VbglStatusReady;
        }

        vbglDriverClose (&driver);
    }

    dprintf (("vbglQueryVMMDevPort rc = %d\n", rc));
}
#endif

int VbglEnter (void)
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

    memset(&g_vbgldata, 0, sizeof (VBGLDATA));

    g_vbgldata.status = VbglStatusInitializing;

    rc = VbglPhysHeapInit ();

    if (VBOX_SUCCESS(rc))
    {
        /* other subsystems, none yet */
        ;
    }

    dprintf(("vbglInitCommon: rc = %d\n", rc));
    
    return rc;
}

DECLVBGL(void) vbglTerminateCommon (void)
{
    VbglPhysHeapTerminate ();

    memset(&g_vbgldata, 0, sizeof (VBGLDATA));

    return;
}

#ifdef VBGL_VBOXGUEST

DECLVBGL(int) VbglInit (VBGLIOPORT portVMMDev, VMMDevMemory *pVMMDevMemory)
{
    int rc = VINF_SUCCESS;

    dprintf(("vbglInit: starts g_vbgldata.status %d\n", g_vbgldata.status));
    
    if (g_vbgldata.status == VbglStatusInitializing
        || g_vbgldata.status == VbglStatusReady)
    {
        /* Initialization is already in process. */
        return rc;
    }

    rc = vbglInitCommon ();

    if (VBOX_SUCCESS(rc))
    {
        g_vbgldata.portVMMDev = portVMMDev;
        g_vbgldata.pVMMDevMemory = pVMMDevMemory;

        g_vbgldata.status = VbglStatusReady;
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

    if (VBOX_SUCCESS(rc))
    {
        /* Try to obtain VMMDev port via IOCTL to VBoxGuest main driver. */
        vbglQueryVMMDevPort ();

#ifdef VBOX_HGCM
        rc = vbglHGCMInit ();
#endif /* VBOX_HGCM */

        if (VBOX_FAILURE(rc))
        {
            vbglTerminateCommon ();
        }
    }

    return rc;
}

DECLVBGL(void) VbglTerminate (void)
{
    vbglTerminateCommon ();

#ifdef VBOX_HGCM
    vbglHGCMTerminate ();
#endif

    return;
}

#endif /* !VBGL_VBOXGUEST */

