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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
    HGCMFunctionParameter *pParm;
    unsigned iParm;
    int rc;

    if (!pCallInfo || !pAsyncCallback || pCallInfo->cParms > VBOX_HGCM_MAX_PARMS)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* Anyone who needs this can re-enable it locally */
    /* dprintf (("VbglHGCMCall: pCallInfo->cParms = %d, pHGCMCall->u32Function = %d\n", pCallInfo->cParms, pCallInfo->u32Function)); */

    pHGCMCall = NULL;

    cbParms = pCallInfo->cParms * sizeof (HGCMFunctionParameter);

    /* Allocate request */
    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pHGCMCall, sizeof (VMMDevHGCMCall) + cbParms, VMMDevReq_HGCMCall);

    /* Anyone who needs this can re-enable it locally */
    /* dprintf (("VbglHGCMCall Allocated gr %p, rc = %Vrc, cbParms = %d\n", pHGCMCall, rc, cbParms)); */

    if (VBOX_SUCCESS(rc))
    {
        void *apvCtx[VBOX_HGCM_MAX_PARMS];
        memset (apvCtx, 0, sizeof(void *) * pCallInfo->cParms);

        /* Initialize request memory */
        pHGCMCall->header.fu32Flags = 0;
        pHGCMCall->header.result    = VINF_SUCCESS;

        pHGCMCall->u32ClientID = pCallInfo->u32ClientID;
        pHGCMCall->u32Function = pCallInfo->u32Function;
        pHGCMCall->cParms      = pCallInfo->cParms;

        if (cbParms)
        {           
            /* Lock user buffers. */
            pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);

            for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
            {
                switch (pParm->type)
                {
                case VMMDevHGCMParmType_LinAddr_Locked_In:
                    pParm->type = VMMDevHGCMParmType_LinAddr_In;
                    break;
                case VMMDevHGCMParmType_LinAddr_Locked_Out:
                    pParm->type = VMMDevHGCMParmType_LinAddr_Out;
                    break;
                case VMMDevHGCMParmType_LinAddr_Locked:
                    pParm->type = VMMDevHGCMParmType_LinAddr;
                    break;

                case VMMDevHGCMParmType_LinAddr_In:
                case VMMDevHGCMParmType_LinAddr_Out:
                case VMMDevHGCMParmType_LinAddr:
                    /* PORTME: When porting this to Darwin and other systems where the entire kernel isn't mapped
                       into every process, all linear address will have to be converted to physical SG lists at
                       this point. Care must also be taken on these guests to not mix kernel and user addresses
                       in HGCM calls, or we'll end up locking the wrong memory. If VMMDev/HGCM gets a linear address
                       it will assume that it's in the current memory context (i.e. use CR3 to translate it).

                       These kind of problems actually applies to some patched linux kernels too, including older
                       fedora releases. (The patch is the infamous 4G/4G patch, aka 4g4g, by Ingo Molnar.) */
                    rc = vbglLockLinear (&apvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size, (pParm->type == VMMDevHGCMParmType_LinAddr_In) ? false : true /* write access */);                   
                    break;
                default:
                    /* make gcc happy */
                    break;
                }
                if (VBOX_FAILURE (rc))
                    break;
            }
            memcpy (VMMDEV_HGCM_CALL_PARMS(pHGCMCall), VBOXGUEST_HGCM_CALL_PARMS(pCallInfo), cbParms);
        }

        /* Check that the parameter locking was ok. */
        if (VBOX_SUCCESS(rc))
        {
            /* Anyone who needs this can re-enable it locally */
            /* dprintf (("calling VbglGRPerform\n")); */

            /* Issue request */
            rc = VbglGRPerform (&pHGCMCall->header.header);

            /* Anyone who needs this can re-enable it locally */
            /* dprintf (("VbglGRPerform rc = %Vrc (header rc=%d)\n", rc, pHGCMCall->header.result)); */

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
        pParm = VBOXGUEST_HGCM_CALL_PARMS(pCallInfo);

        for (iParm = 0; iParm < pCallInfo->cParms; iParm++, pParm++)
        {
            if (   pParm->type == VMMDevHGCMParmType_LinAddr_In
                || pParm->type == VMMDevHGCMParmType_LinAddr_Out
                || pParm->type == VMMDevHGCMParmType_LinAddr)
            {
                if (apvCtx[iParm] != NULL)
                {
                    vbglUnlockLinear (apvCtx[iParm], (void *)pParm->u.Pointer.u.linearAddr, pParm->u.Pointer.size);
                }
            }
            else
                Assert(!apvCtx[iParm]);
        }

        if ((pHGCMCall->header.fu32Flags & VBOX_HGCM_REQ_CANCELLED) == 0)
            VbglGRFree (&pHGCMCall->header.header);
        else
            rc = VERR_INTERRUPTED;
    }

    return rc;
}

#endif /* VBGL_VBOXGUEST */
