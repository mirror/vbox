/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Host-Guest Communication Manager internal functions, implemented by VBoxGuest
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

/* Entire file is ifdef'ed with VBGL_VBOXGUEST */
#ifdef VBGL_VBOXGUEST

/** @todo r=bird: These two issues with string.h and bool are handled by
 * iprt/string.h and iprt/types.h respectivly. Please change after the release. */
#if defined(__LINUX__) && defined(__KERNEL__)
#ifndef bool /* Linux 2.6.19 C++ nightmare */
#define bool bool_type
#define true true_type
#define false false_type
#define _Bool int
#define bool_HGCMInternal_cpp
#endif
#include <linux/string.h>
#ifdef bool_HGCMInternal_cpp
#undef bool
#undef true
#undef false
#undef _Bool
#undef bool_HGCMInternal_cpp
#endif
#else
#include <string.h>
#endif
#include <VBox/VBoxGuestLib.h>
#include "VBGLInternal.h"

/* These functions can be only used by VBoxGuest. */

DECLVBGL(int) VbglHGCMConnect (VBoxGuestHGCMConnectInfo *pConnectInfo,
                               VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData,
                               uint32_t u32AsyncData)
{
    if (!pConnectInfo || !pAsyncCallback)
    {
        return VERR_INVALID_PARAMETER;
    }

    VMMDevHGCMConnect *pHGCMConnect = NULL;

    /* Allocate request */
    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMConnect, sizeof (VMMDevHGCMConnect), VMMDevReq_HGCMConnect);

    if (VBOX_SUCCESS(rc))
    {
        /* Initialize request memory */
        pHGCMConnect->header.fu32Flags = 0;

        memcpy (&pHGCMConnect->loc, &pConnectInfo->Loc, sizeof (HGCMServiceLocation));
        pHGCMConnect->u32ClientID = 0;

        /* Issue request */
        rc = VbglGRPerform (&pHGCMConnect->header.header);

        if (VBOX_SUCCESS(rc))
        {
            /* Check if host decides to process the request asynchronously. */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                /* Wait for request completion interrupt notification from host */
                pAsyncCallback (&pHGCMConnect->header, pvAsyncData, u32AsyncData);
            }

            pConnectInfo->result = pHGCMConnect->header.result;

            if (VBOX_SUCCESS (pConnectInfo->result))
            {
                pConnectInfo->u32ClientID = pHGCMConnect->u32ClientID;
            }
        }

        VbglGRFree (&pHGCMConnect->header.header);
    }

    return rc;
}


DECLVBGL(int) VbglHGCMDisconnect (VBoxGuestHGCMDisconnectInfo *pDisconnectInfo,
                                  VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    if (!pDisconnectInfo || !pAsyncCallback)
    {
        return VERR_INVALID_PARAMETER;
    }

    VMMDevHGCMDisconnect *pHGCMDisconnect = NULL;

    /* Allocate request */
    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMDisconnect, sizeof (VMMDevHGCMDisconnect), VMMDevReq_HGCMDisconnect);

    if (VBOX_SUCCESS(rc))
    {
        /* Initialize request memory */
        pHGCMDisconnect->header.fu32Flags = 0;

        pHGCMDisconnect->u32ClientID = pDisconnectInfo->u32ClientID;

        /* Issue request */
        rc = VbglGRPerform (&pHGCMDisconnect->header.header);

        if (VBOX_SUCCESS(rc))
        {
            /* Check if host decides to process the request asynchronously. */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                /* Wait for request completion interrupt notification from host */
                pAsyncCallback (&pHGCMDisconnect->header, pvAsyncData, u32AsyncData);
            }

            pDisconnectInfo->result = pHGCMDisconnect->header.result;
        }

        VbglGRFree (&pHGCMDisconnect->header.header);
    }

    return rc;
}


DECLVBGL(int) VbglHGCMCall (VBoxGuestHGCMCallInfo *pCallInfo,
                            VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    if (!pCallInfo || !pAsyncCallback)
    {
        return VERR_INVALID_PARAMETER;
    }

    dprintf (("VbglHGCMCall: pCallInfo->cParms = %d, pHGCMCall->u32Function = %d\n", pCallInfo->cParms, pCallInfo->u32Function));

    VMMDevHGCMCall *pHGCMCall = NULL;

    uint32_t cbParms = pCallInfo->cParms * sizeof (HGCMFunctionParameter);

    /* Allocate request */
    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMCall, sizeof (VMMDevHGCMCall) + cbParms, VMMDevReq_HGCMCall);

    dprintf (("VbglHGCMCall Allocated gr %p, rc = %Vrc, cbParms = %d\n", pHGCMCall, rc, cbParms));

    if (VBOX_SUCCESS(rc))
    {
        /* Initialize request memory */
        pHGCMCall->header.fu32Flags = 0;

        pHGCMCall->u32ClientID = pCallInfo->u32ClientID;
        pHGCMCall->u32Function = pCallInfo->u32Function;
        pHGCMCall->cParms      = pCallInfo->cParms;

        if (cbParms)
        {
            memcpy (VMMDEV_HGCM_CALL_PARMS(pHGCMCall), VBOXGUEST_HGCM_CALL_PARMS(pCallInfo), cbParms);
        }

        dprintf (("calling VbglGRPerform\n"));

        /* Issue request */
        rc = VbglGRPerform (&pHGCMCall->header.header);

        dprintf (("VbglGRPerform rc = %Vrc\n", rc));

        if (VBOX_SUCCESS(rc))
        {
            /* Check if host decides to process the request asynchronously. */
            if (rc == VINF_HGCM_ASYNC_EXECUTE)
            {
                /* Wait for request completion interrupt notification from host */
                pAsyncCallback (&pHGCMCall->header, pvAsyncData, u32AsyncData);
            }

            if (pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE)
            {
                if (cbParms)
                {
                    memcpy (VBOXGUEST_HGCM_CALL_PARMS(pCallInfo), VMMDEV_HGCM_CALL_PARMS(pHGCMCall), cbParms);
                }
                pCallInfo->result = pHGCMCall->header.result;
            }
            else
            {
                /* The callback returns without completing the request,
                 * that means the wait was interrrupted. That can happen
                 * if system reboots or the VBoxService ended abnormally.
                 * In both cases it is OK to just leave the allocated memory
                 * in the physical heap. The memory leak does not affect normal
                 * operations.
                 * @todo VbglGRCancel (&pHGCMCall->header.header) need to be implemented.
                 *       The host will not write to the cancelled memory.
                 */
                pHGCMCall->header.fu32Flags |= VBOX_HGCM_REQ_CANCELLED;
            }
        }

        if ((pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_CANCELLED) == 0)
        {
            VbglGRFree (&pHGCMCall->header.header);
        }
        else
        {
            rc = VERR_INTERRUPTED;
        }
    }

    return rc;
}


#endif /* VBGL_VBOXGUEST */
