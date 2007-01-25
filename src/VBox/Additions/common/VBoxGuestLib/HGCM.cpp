/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Host-Guest Communication Manager
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

/* These public functions can be only used by other drivers.
 * They all do an IOCTL to VBoxGuest.
 */

/* Entire file is ifdef'ed with !VBGL_VBOXGUEST */
#ifndef VBGL_VBOXGUEST

/** @todo r=bird: These two issues with string.h and bool are handled by
 * iprt/string.h and iprt/types.h respectivly. Please change after the release. */
#if defined(__LINUX__) && defined(__KERNEL__)
#ifndef bool /* Linux 2.6.19 C++ nightmare */
#define bool bool_type
#define true true_type
#define false false_type
#define _Bool int
#define bool_HGCM_cpp
#endif
#include <linux/string.h>
#ifdef bool_HGCM_cpp
#undef bool
#undef true
#undef false
#undef _Bool
#undef bool_HGCM_cpp
#endif
#else
#include <string.h>
#endif

#include <VBox/VBoxGuestLib.h>
#include "VBGLInternal.h"

#include <iprt/assert.h>
#include <iprt/semaphore.h>

#define VBGL_HGCM_ASSERTMsg AssertReleaseMsg

int vbglHGCMInit (void)
{
    RTSemFastMutexCreate(&g_vbgldata.mutexHGCMHandle);

    return VINF_SUCCESS;
}

int vbglHGCMTerminate (void)
{
    RTSemFastMutexDestroy(g_vbgldata.mutexHGCMHandle);

    return VINF_SUCCESS;
}

static DECLINLINE(int) vbglHandleHeapEnter (void)
{
    int rc = RTSemFastMutexRequest(g_vbgldata.mutexHGCMHandle);

    VBGL_HGCM_ASSERTMsg(VBOX_SUCCESS(rc),
                        ("Failed to request handle heap mutex, rc = %Vrc\n", rc));

    return rc;
}

static DECLINLINE(void) vbglHandleHeapLeave (void)
{
    RTSemFastMutexRelease(g_vbgldata.mutexHGCMHandle);
}

VBGLHGCMHANDLEDATA *vbglHGCMHandleAlloc (void)
{
    int rc = vbglHandleHeapEnter ();

    if (VBOX_FAILURE (rc))
    {
        return NULL;
    }

    VBGLHGCMHANDLEDATA *p = NULL;

    /** Simple linear search in array. This will be called not so often, only connect/disconnect.
     * @todo bitmap for faster search and other obvious optimizations.
     */

    uint32_t i;

    for (i = 0; i < ELEMENTS(g_vbgldata.aHGCMHandleData); i++)
    {
        if (!g_vbgldata.aHGCMHandleData[i].fAllocated)
        {
            p = &g_vbgldata.aHGCMHandleData[i];

            p->fAllocated = 1;

            break;
        }
    }

    vbglHandleHeapLeave ();

    VBGL_HGCM_ASSERTMsg(p != NULL,
                        ("Not enough HGCM handles.\n"));

    return p;
}

void vbglHGCMHandleFree (VBGLHGCMHANDLEDATA *pHandle)
{
    if (!pHandle)
    {
       return;
    }

    int rc = vbglHandleHeapEnter ();

    if (VBOX_FAILURE (rc))
    {
        return;
    }

    VBGL_HGCM_ASSERTMsg(pHandle->fAllocated,
                        ("Freeing not allocated handle.\n"));

    memset(pHandle, 0, sizeof (VBGLHGCMHANDLEDATA));

    vbglHandleHeapLeave ();

    return;
}

DECLVBGL(int) VbglHGCMConnect (VBGLHGCMHANDLE *pHandle, VBoxGuestHGCMConnectInfo *pData)
{
    if (!pHandle || !pData)
    {
        return VERR_INVALID_PARAMETER;
    }

    VBGLHGCMHANDLEDATA *pHandleData = vbglHGCMHandleAlloc ();

    int rc = VINF_SUCCESS;

    if (!pHandleData)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        rc = vbglDriverOpen (&pHandleData->driver);

        if (VBOX_SUCCESS(rc))
        {
            rc = vbglDriverIOCtl (&pHandleData->driver, IOCTL_VBOXGUEST_HGCM_CONNECT, pData, sizeof (*pData));

            if (VBOX_SUCCESS(rc))
            {
                *pHandle = pHandleData;
            }
            else
            {
                vbglDriverClose (&pHandleData->driver);
            }
        }

        if (VBOX_FAILURE(rc))
        {
            vbglHGCMHandleFree (pHandleData);
        }
    }

    return rc;
}

DECLVBGL(int) VbglHGCMDisconnect (VBGLHGCMHANDLE handle, VBoxGuestHGCMDisconnectInfo *pData)
{
    int rc = VINF_SUCCESS;

    rc = vbglDriverIOCtl (&handle->driver, IOCTL_VBOXGUEST_HGCM_DISCONNECT, pData, sizeof (*pData));

    vbglDriverClose (&handle->driver);

    vbglHGCMHandleFree (handle);

    return rc;
}

DECLVBGL(int) VbglHGCMCall (VBGLHGCMHANDLE handle, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    VBGL_HGCM_ASSERTMsg(cbData >= sizeof (VBoxGuestHGCMCallInfo) + pData->cParms * sizeof (HGCMFunctionParameter),
                        ("cbData = %d, cParms = %d (calculated size %d)\n", cbData, pData->cParms, sizeof (VBoxGuestHGCMCallInfo) + pData->cParms * sizeof (VBoxGuestHGCMCallInfo)));

    rc = vbglDriverIOCtl (&handle->driver, IOCTL_VBOXGUEST_HGCM_CALL, pData, cbData);

    dprintf(("vbglDriverIOCtl rc = %Vrc\n", rc));

    return rc;
}

#endif /* VBGL_VBOXGUEST */
