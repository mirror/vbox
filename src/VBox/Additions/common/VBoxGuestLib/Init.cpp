/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Library initialization
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#if defined(__LINUX__) && defined(__KERNEL__)
#ifndef bool /* Linux 2.6.19 C++ nightmare */
#define bool bool_type
#define true true_type
#define false false_type
#define _Bool int
#define bool_Init_cpp
#endif
#include <linux/string.h>
#ifdef bool_Init_cpp
#undef bool
#undef true
#undef false
#undef _Bool
#undef bool_Init_cpp
#endif
#else
#include <string.h>
#endif

#include <VBox/VBoxGuestLib.h>

#define VBGL_DECL_DATA
#include "VBGLInternal.h"

#include <iprt/assert.h>

VBGLDATA g_vbgldata;

#ifndef VBGL_VBOXGUEST
static int vbglQueryVMMDevPort (void)
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
    
    return rc;
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

    rc = g_vbgldata.status != VbglStatusNotInitialized? VINF_SUCCESS: VERR_VBGL_NOT_INITIALIZED;
    
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
        /* Obtain VMMDev port via IOCTL to VBoxGuest main driver. */
        vbglQueryVMMDevPort ();

#ifdef VBOX_HGCM
        rc = vbglHGCMInit ();
#endif

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

