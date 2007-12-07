/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Host-Guest Communication Manager
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* These public functions can be only used by other drivers.
 * They all do an IOCTL to VBoxGuest.
 */

/* Entire file is ifdef'ed with !VBGL_VBOXGUEST */
#ifndef VBGL_VBOXGUEST

#include <VBox/VBoxGuestLib.h>
#include "VBGLInternal.h"

#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

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

DECLINLINE(int) vbglHandleHeapEnter (void)
{
    int rc = RTSemFastMutexRequest(g_vbgldata.mutexHGCMHandle);

    VBGL_HGCM_ASSERTMsg(VBOX_SUCCESS(rc),
                        ("Failed to request handle heap mutex, rc = %Vrc\n", rc));

    return rc;
}

DECLINLINE(void) vbglHandleHeapLeave (void)
{
    RTSemFastMutexRelease(g_vbgldata.mutexHGCMHandle);
}

struct VBGLHGCMHANDLEDATA *vbglHGCMHandleAlloc (void)
{
    struct VBGLHGCMHANDLEDATA *p;
    int rc = vbglHandleHeapEnter ();
    uint32_t i;

    if (VBOX_FAILURE (rc))
        return NULL;

    p = NULL;

    /** Simple linear search in array. This will be called not so often, only connect/disconnect.
     * @todo bitmap for faster search and other obvious optimizations.
     */

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

void vbglHGCMHandleFree (struct VBGLHGCMHANDLEDATA *pHandle)
{
    int rc;

    if (!pHandle)
       return;

    rc = vbglHandleHeapEnter ();

    if (VBOX_FAILURE (rc))
        return;

    VBGL_HGCM_ASSERTMsg(pHandle->fAllocated,
                        ("Freeing not allocated handle.\n"));

    memset(pHandle, 0, sizeof (struct VBGLHGCMHANDLEDATA));
    vbglHandleHeapLeave ();
    return;
}

DECLVBGL(int) VbglHGCMConnect (VBGLHGCMHANDLE *pHandle, VBoxGuestHGCMConnectInfo *pData)
{
    int rc;
    struct VBGLHGCMHANDLEDATA *pHandleData;

    if (!pHandle || !pData)
        return VERR_INVALID_PARAMETER;

    pHandleData = vbglHGCMHandleAlloc ();

    rc = VINF_SUCCESS;

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

    return rc;
}

#endif /* VBGL_VBOXGUEST */
