/* $Id$ */
/** @file
 * Shared Clipboard Service - Linux host.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-x11.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <iprt/errcore.h>

#include "VBoxSharedClipboardSvc-internal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Global context information used by the host glue for the X11 clipboard backend.
 */
struct SHCLCONTEXT
{
    /** This mutex is grabbed during any critical operations on the clipboard
     * which might clash with others. */
    RTCRITSECT           CritSect;
    /** X11 context data. */
    SHCLX11CTX           X11;
    /** Pointer to the VBox host client data structure. */
    PSHCLCLIENT          pClient;
    /** We set this when we start shutting down as a hint not to post any new
     * requests. */
    bool                 fShuttingDown;
};


int ShClSvcImplInit(void)
{
    LogFlowFuncEnter();
    return VINF_SUCCESS;
}

void ShClSvcImplDestroy(void)
{
    LogFlowFuncEnter();
}

/**
 * @note  On the host, we assume that some other application already owns
 *        the clipboard and leave ownership to X11.
 */
int ShClSvcImplConnect(PSHCLCLIENT pClient, bool fHeadless)
{
    int rc;

    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)RTMemAllocZ(sizeof(SHCLCONTEXT));
    if (pCtx)
    {
        rc = RTCritSectInit(&pCtx->CritSect);
        if (RT_SUCCESS(rc))
        {
            rc = ShClX11Init(&pCtx->X11, pCtx, fHeadless);
            if (RT_SUCCESS(rc))
            {
                pClient->State.pCtx = pCtx;
                pCtx->pClient = pClient;

                rc = ShClX11ThreadStart(&pCtx->X11, true /* grab shared clipboard */);
                if (RT_FAILURE(rc))
                    ShClX11Destroy(&pCtx->X11);
            }

            if (RT_FAILURE(rc))
                RTCritSectDelete(&pCtx->CritSect);
        }
        else
            RTMemFree(pCtx);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplSync(PSHCLCLIENT pClient)
{
    LogFlowFuncEnter();

    /* Tell the guest we have no data in case X11 is not available.  If
     * there is data in the host clipboard it will automatically be sent to
     * the guest when the clipboard starts up. */
    return ShClSvcHostReportFormats(pClient, VBOX_SHCL_FMT_NONE);
}

/**
 * Shut down the shared clipboard service and "disconnect" the guest.
 * @note  Host glue code
 */
int ShClSvcImplDisconnect(PSHCLCLIENT pClient)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pCtx = pClient->State.pCtx;
    AssertPtr(pCtx);

    /* Drop the reference to the client, in case it is still there.  This
     * will cause any outstanding clipboard data requests from X11 to fail
     * immediately. */
    pCtx->fShuttingDown = true;

    int rc = ShClX11ThreadStop(&pCtx->X11);
    /** @todo handle this slightly more reasonably, or be really sure
     *        it won't go wrong. */
    AssertRC(rc);

    ShClX11Destroy(&pCtx->X11);
    RTCritSectDelete(&pCtx->CritSect);

    RTMemFree(pCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplFormatAnnounce(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                              PSHCLFORMATDATA pFormats)
{
    RT_NOREF(pCmdCtx);

    int rc = ShClX11ReportFormatsToX11(&pClient->State.pCtx->X11, pFormats->Formats);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** Structure describing a request for clipoard data from the guest. */
struct CLIPREADCBREQ
{
    /** User-supplied data pointer, based on the request type. */
    void                *pv;
    /** The size (in bytes) of the the user-supplied pointer in pv. */
    uint32_t             cb;
    /** The actual size of the data written. */
    uint32_t            *pcbActual;
    /** The request's event ID. */
    SHCLEVENTID          idEvent;
};

/**
 * @note   We always fail or complete asynchronously.
 * @note   On success allocates a CLIPREADCBREQ structure which must be
 *         freed in ClipCompleteDataRequestFromX11 when it is called back from
 *         the backend code.
 */
int ShClSvcImplReadData(PSHCLCLIENT pClient,
                        PSHCLCLIENTCMDCTX pCmdCtx, SHCLFORMAT uFormat, void *pvData, uint32_t cbData, uint32_t *pcbActual)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    AssertPtrReturn(pCmdCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,  VERR_INVALID_POINTER);

    RT_NOREF(pCmdCtx);

    LogFlowFunc(("pClient=%p, uFormat=%02X, pv=%p, cb=%u, pcbActual=%p\n",
                 pClient, uFormat, pvData, cbData, pcbActual));

    int rc = VINF_SUCCESS;

    CLIPREADCBREQ *pReq = (CLIPREADCBREQ *)RTMemAllocZ(sizeof(CLIPREADCBREQ));
    if (pReq)
    {
        pReq->pv        = pvData;
        pReq->cb        = cbData;
        pReq->pcbActual = pcbActual;
        const SHCLEVENTID idEvent = ShClEventIdGenerateAndRegister(&pClient->EventSrc);
        pReq->idEvent    = idEvent;
        if (idEvent != NIL_SHCLEVENTID)
        {
            rc = ShClX11ReadDataFromX11(&pClient->State.pCtx->X11, uFormat, pReq);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pClient->EventSrc, idEvent, 30 * 1000, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    memcpy(pvData, pPayload->pvData, RT_MIN(cbData, pPayload->cbData));

                    *pcbActual = (uint32_t)pPayload->cbData;

                    ShClPayloadFree(pPayload);
                }
            }

            ShClEventUnregister(&pClient->EventSrc, idEvent);
        }
        else
        {
            RTMemFree(pReq);
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplWriteData(PSHCLCLIENT pClient,
                         PSHCLCLIENTCMDCTX pCmdCtx, SHCLFORMAT uFormat, void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    AssertPtrReturn(pCmdCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,  VERR_INVALID_POINTER);

    LogFlowFunc(("pClient=%p, pv=%p, cb=%RU32, uFormat=%02X\n",
                 pClient, pvData, cbData, uFormat));

    int rc = ShClSvcDataReadSignal(pClient, pCmdCtx, uFormat, pvData, cbData);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reports formats available in the X11 clipboard to VBox.
 *
 * @note   Runs in Xt event thread.
 *
 * @param  pCtx                 Opaque context pointer for the glue code.
 * @param  fFormats             The formats available.
 */
DECLCALLBACK(void) ShClX11ReportFormatsCallback(PSHCLCONTEXT pCtx, uint32_t fFormats)
{
    LogFlowFunc(("pCtx=%p, fFormats=%#x\n", pCtx, fFormats));

    /** @todo r=bird: BUGBUG: Revisit this   */
    if (fFormats == VBOX_SHCL_FMT_NONE) /* No formats to report? Bail out early. */
        return;

    int rc = ShClSvcHostReportFormats(pCtx->pClient, fFormats);
    RT_NOREF(rc);

    LogFlowFuncLeaveRC(rc);
}

/**
 * Completes a request from the host service for reading the X11 clipboard data.
 * The data should be written to the buffer provided in the initial request.
 *
 * @note   Runs in Xt event thread.
 *
 * @param  pCtx                 Request context information.
 * @param  rcCompletion         The completion status of the request.
 * @param  pReq                 Request to complete.
 * @param  pv                   Address of data from completed request. Optional.
 * @param  cb                   Size (in bytes) of data from completed request. Optional.
 *
 * @todo   Change this to deal with the buffer issues rather than offloading them onto the caller.
 */
DECLCALLBACK(void) ShClX11RequestFromX11CompleteCallback(PSHCLCONTEXT pCtx, int rcCompletion,
                                                         CLIPREADCBREQ *pReq, void *pv, uint32_t cb)
{
    RT_NOREF(rcCompletion);

    LogFlowFunc(("rcCompletion=%Rrc, pReq=%p, pv=%p, cb=%RU32, idEvent=%RU32\n", rcCompletion, pReq, pv, cb, pReq->idEvent));

    AssertMsgRC(rcCompletion, ("Clipboard data completion from X11 failed with %Rrc\n", rcCompletion));

    if (pReq->idEvent != NIL_SHCLEVENTID)
    {
        int rc2;

        PSHCLEVENTPAYLOAD pPayload = NULL;
        if (pv && cb)
        {
            rc2 = ShClPayloadAlloc(pReq->idEvent, pv, cb, &pPayload);
            AssertRC(rc2);
        }

        RTCritSectEnter(&pCtx->pClient->CritSect);
        rc2 = ShClEventSignal(&pCtx->pClient->EventSrc, pReq->idEvent, pPayload);
        AssertRC(rc2);
        RTCritSectLeave(&pCtx->pClient->CritSect);
    }

    RTMemFree(pReq);
}

/**
 * Reads clipboard data from the guest and passes it to the X11 clipboard.
 *
 * @note   Runs in Xt event thread.
 *
 * @param  pCtx      Pointer to the host clipboard structure.
 * @param  fFormat   The format in which the data should be transferred
 *                   (VBOX_SHCL_FMT_XXX).
 * @param  ppv       On success and if pcb > 0, this will point to a buffer
 *                   to be freed with RTMemFree containing the data read.
 * @param  pcb       On success, this contains the number of bytes of data
 *                   returned.
 */
DECLCALLBACK(int) ShClX11RequestDataForX11Callback(PSHCLCONTEXT pCtx, SHCLFORMAT fFormat, void **ppv, uint32_t *pcb)
{
    LogFlowFunc(("pCtx=%p, Format=0x%x\n", pCtx, fFormat));

    if (pCtx->fShuttingDown)
    {
        /* The shared clipboard is disconnecting. */
        LogRel(("Shared Clipboard: Host requested guest clipboard data after guest had disconnected\n"));
        return VERR_WRONG_ORDER;
    }

    int rc;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    if (fFormat == VBOX_SHCL_FMT_URI_LIST)
        rc = VINF_SUCCESS;
    else
#endif
    {
        /* Request data from the guest. */
        SHCLEVENTID idEvent;
        rc = ShClSvcDataReadRequest(pCtx->pClient, fFormat, &idEvent);
        if (RT_SUCCESS(rc))
        {
            PSHCLEVENTPAYLOAD pPayload;
            rc = ShClEventWait(&pCtx->pClient->EventSrc, idEvent, 30 * 1000, &pPayload);
            if (RT_SUCCESS(rc))
            {
                *ppv = pPayload ? pPayload->pvData : NULL;
                *pcb = pPayload ? pPayload->cbData : 0;

                /* Detach the payload, as the caller then will own the data. */
                ShClEventPayloadDetach(&pCtx->pClient->EventSrc, idEvent);
            }

            ShClEventUnregister(&pCtx->pClient->EventSrc, idEvent);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS

int ShClSvcImplTransferCreate(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    RT_NOREF(pClient, pTransfer);

    int rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplTransferDestroy(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    RT_NOREF(pClient, pTransfer);

    int rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplTransferGetRoots(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    LogFlowFuncEnter();

    int rc;

    SHCLEVENTID idEvent = ShClEventIdGenerateAndRegister(&pClient->EventSrc);
    if (idEvent != NIL_SHCLEVENTID)
    {
        CLIPREADCBREQ *pReq = (CLIPREADCBREQ *)RTMemAllocZ(sizeof(CLIPREADCBREQ));
        if (pReq)
        {
            pReq->idEvent = idEvent;

            rc = ShClX11ReadDataFromX11(&pClient->State.pCtx->X11, VBOX_SHCL_FMT_URI_LIST, pReq);
            if (RT_SUCCESS(rc))
            {
                /* X supplies the data asynchronously, so we need to wait for data to arrive first. */
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pClient->EventSrc, idEvent, 30 * 1000, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    rc = ShClTransferRootsSet(pTransfer,
                                              (char *)pPayload->pvData, pPayload->cbData + 1 /* Include termination */);
                }
            }
        }
        else
            rc = VERR_NO_MEMORY;

        ShClEventUnregister(&pClient->EventSrc, idEvent);
    }
    else
        rc = VERR_SHCLPB_MAX_EVENTS_REACHED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */
