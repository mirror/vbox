/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Host-Guest Communication Manager internal functions, implemented by VBoxGuest
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#include <VBox/VBoxGuestLib.h>
#include "VBGLInternal.h"
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/alloca.h>

/* These functions can be only used by VBoxGuest. */

DECLVBGL(int) VbglHGCMConnect (VBoxGuestHGCMConnectInfo *pConnectInfo,
                               VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData,
                               uint32_t u32AsyncData)
{
    VMMDevHGCMConnect *pHGCMConnect;
    int rc;

    if (!pConnectInfo || !pAsyncCallback)
        return VERR_INVALID_PARAMETER;

    pHGCMConnect = NULL;

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMConnect, sizeof (VMMDevHGCMConnect), VMMDevReq_HGCMConnect);

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
                pConnectInfo->u32ClientID = pHGCMConnect->u32ClientID;
        }

        VbglGRFree (&pHGCMConnect->header.header);
    }

    return rc;
}


DECLVBGL(int) VbglHGCMDisconnect (VBoxGuestHGCMDisconnectInfo *pDisconnectInfo,
                                  VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData)
{
    VMMDevHGCMDisconnect *pHGCMDisconnect;
    int rc;

    if (!pDisconnectInfo || !pAsyncCallback)
        return VERR_INVALID_PARAMETER;

    pHGCMDisconnect = NULL;

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMDisconnect, sizeof (VMMDevHGCMDisconnect), VMMDevReq_HGCMDisconnect);

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
    VMMDevHGCMCall *pHGCMCall;
    uint32_t cbParms;
    int rc;

    if (!pCallInfo || !pAsyncCallback || pCallInfo->cParms > VBOX_HGCM_MAX_PARMS)
        return VERR_INVALID_PARAMETER;

    dprintf (("VbglHGCMCall: pCallInfo->cParms = %d, pHGCMCall->u32Function = %d\n", pCallInfo->cParms, pCallInfo->u32Function));

    pHGCMCall = NULL;

    cbParms = pCallInfo->cParms * sizeof (HGCMFunctionParameter);

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMCall, sizeof (VMMDevHGCMCall) + cbParms, VMMDevReq_HGCMCall);

    dprintf (("VbglHGCMCall Allocated gr %p, rc = %Vrc, cbParms = %d\n", pHGCMCall, rc, cbParms));

    if (VBOX_SUCCESS(rc))
    {
        void **papvCtx = NULL;

        /* Initialize request memory */
        pHGCMCall->header.fu32Flags = 0;
        pHGCMCall->header.result    = VINF_SUCCESS;

        pHGCMCall->u32ClientID = pCallInfo->u32ClientID;
        pHGCMCall->u32Function = pCallInfo->u32Function;
        pHGCMCall->cParms      = pCallInfo->cParms;

        if (cbParms)
        {
            memcpy (VMMDEV_HGCM_CALL_PARMS(pHGCMCall), VBOXGUEST_HGCM_CALL_PARMS(pCallInfo), cbParms);
            
            /* Lock user buffers. */
            if (pCallInfo->cParms > 0)
            {
                papvCtx = (void **)alloca(pCallInfo->cParms * sizeof (papvCtx[0]));
                memset (papvCtx, 0, pCallInfo->cParms * sizeof (papvCtx[0]));
            }
            
            HGCMFunctionParameter *pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);
            
            unsigned iParm = 0;
            for (; iParm < pCallInfo->cParms; iParm++, pParm++)
            {
                if (   pParm->type == VMMDevHGCMParmType_LinAddr_In
                    || pParm->type == VMMDevHGCMParmType_LinAddr_Out
                    || pParm->type == VMMDevHGCMParmType_LinAddr)
                {
                    rc = vbglLockLinear (&papvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size);
                    
                    if (VBOX_FAILURE (rc))
                    {
                        break;
                    }
                }
            }
        }

        /* Check that the parameter locking was ok. */
        if (VBOX_SUCCESS(rc))
        {
            dprintf (("calling VbglGRPerform\n"));

            /* Issue request */
            rc = VbglGRPerform (&pHGCMCall->header.header);

            dprintf (("VbglGRPerform rc = %Vrc (header rc=%d)\n", rc, pHGCMCall->header.result));

            /** If the call failed, but as a result of the request itself, then pretend success  
             *  Upper layers will interpret the result code in the packet.
             */
            if (VBOX_FAILURE(rc) && rc == pHGCMCall->header.result)
            {
                Assert(pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_DONE);
                rc = VINF_SUCCESS;
            }

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
        }
            
        /* Unlock user buffers. */
        if (papvCtx != NULL)
        {
            HGCMFunctionParameter *pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);
            
            unsigned iParm = 0;
            for (; iParm < pCallInfo->cParms; iParm++, pParm++)
            {
                if (   pParm->type == VMMDevHGCMParmType_LinAddr_In
                    || pParm->type == VMMDevHGCMParmType_LinAddr_Out
                    || pParm->type == VMMDevHGCMParmType_LinAddr)
                {
                    if (papvCtx[iParm] != NULL)
                    {
                        vbglUnlockLinear (papvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size);
                    }
                }
            }
        }

        if ((pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_CANCELLED) == 0)
            VbglGRFree (&pHGCMCall->header.header);
        else
            rc = VERR_INTERRUPTED;
    }

    return rc;
}

#endif /* VBGL_VBOXGUEST */
