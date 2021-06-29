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
#include <iprt/asm.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-x11.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <iprt/errcore.h>

#include "VBoxSharedClipboardSvc-internal.h"

/* Number of currently extablished connections. */
static volatile uint32_t g_cShClConnections;


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


int ShClBackendInit(void)
{
    LogFlowFuncEnter();
    return VINF_SUCCESS;
}

void ShClBackendDestroy(void)
{
    LogFlowFuncEnter();
}

/**
 * @note  On the host, we assume that some other application already owns
 *        the clipboard and leave ownership to X11.
 */
int ShClBackendConnect(PSHCLCLIENT pClient, bool fHeadless)
{
    int rc;

    /* Check if maximum allowed connections count has reached. */
    if (ASMAtomicIncU32(&g_cShClConnections) > VBOX_SHARED_CLIPBOARD_X11_CONNECTIONS_MAX)
    {
        ASMAtomicDecU32(&g_cShClConnections);
        LogRel(("Shared Clipboard: maximum amount for client connections reached\n"));
        return VERR_OUT_OF_RESOURCES;
    }

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

        if (RT_FAILURE(rc))
        {
            pClient->State.pCtx = NULL;
            RTMemFree(pCtx);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
    {
        /* Restore active connections count. */
        ASMAtomicDecU32(&g_cShClConnections);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClBackendSync(PSHCLCLIENT pClient)
{
    LogFlowFuncEnter();

    /* Tell the guest we have no data in case X11 is not available.  If
     * there is data in the host clipboard it will automatically be sent to
     * the guest when the clipboard starts up. */
    return ShClSvcHostReportFormats(pClient, VBOX_SHCL_FMT_NONE);
}

/*
 * Shut down the shared clipboard service and "disconnect" the guest.
 * Note!  Host glue code
 */
int ShClBackendDisconnect(PSHCLCLIENT pClient)
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

    /* Decrease active connections count. */
    ASMAtomicDecU32(&g_cShClConnections);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClBackendFormatAnnounce(PSHCLCLIENT pClient, SHCLFORMATS fFormats)
{
    int rc = ShClX11ReportFormatsToX11(&pClient->State.pCtx->X11, fFormats);

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
int ShClBackendReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, SHCLFORMAT uFormat,
                        void *pvData, uint32_t cbData, uint32_t *pcbActual)
{
    AssertPtrReturn(pClient,   VERR_INVALID_POINTER);
    AssertPtrReturn(pCmdCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pvData,    VERR_INVALID_POINTER);
    AssertPtrReturn(pcbActual, VERR_INVALID_POINTER);

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
            /* Note: ShClX11ReadDataFromX11() will consume pReq on success. */
            rc = ShClX11ReadDataFromX11(&pClient->State.pCtx->X11, uFormat, pReq);
            if (RT_SUCCESS(rc))
            {
                PSHCLEVENTPAYLOAD pPayload;
                rc = ShClEventWait(&pClient->EventSrc, idEvent, 30 * 1000, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    if (pPayload)
                    {
                        memcpy(pvData, pPayload->pvData, RT_MIN(cbData, pPayload->cbData));

                        *pcbActual = (uint32_t)pPayload->cbData;

                        ShClPayloadFree(pPayload);
                    }
                    else /* No payload given; could happen on invalid / not-expected formats. */
                        *pcbActual = 0;
                }
            }

            ShClEventUnregister(&pClient->EventSrc, idEvent);
        }
        else
            rc = VERR_SHCLPB_MAX_EVENTS_REACHED;

        if (RT_FAILURE(rc))
            RTMemFree(pReq);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Error reading host clipboard data from X11, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClBackendWriteData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, SHCLFORMAT uFormat, void *pvData, uint32_t cbData)
{
    RT_NOREF(pClient, pCmdCtx, uFormat, pvData, cbData);

    LogFlowFuncEnter();

    /* Nothing to do here yet. */

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

DECLCALLBACK(void) ShClX11ReportFormatsCallback(PSHCLCONTEXT pCtx, uint32_t fFormats)
{
    LogFlowFunc(("pCtx=%p, fFormats=%#x\n", pCtx, fFormats));

    int rc = VINF_SUCCESS;
    PSHCLCLIENT pClient = pCtx->pClient;
    AssertPtr(pClient);

    rc = RTCritSectEnter(&pClient->CritSect);
    if (RT_SUCCESS(rc))
    {
        /** @todo r=bird: BUGBUG: Revisit this   */
        if (fFormats != VBOX_SHCL_FMT_NONE) /* No formats to report? */
        {
            rc = ShClSvcHostReportFormats(pCtx->pClient, fFormats);
        }

        RTCritSectLeave(&pClient->CritSect);
    }

    LogFlowFuncLeaveRC(rc);
}

DECLCALLBACK(void) ShClX11RequestFromX11CompleteCallback(PSHCLCONTEXT pCtx, int rcCompletion,
                                                         CLIPREADCBREQ *pReq, void *pv, uint32_t cb)
{
    AssertPtrReturnVoid(pCtx);
    AssertPtrReturnVoid(pReq);

    LogFlowFunc(("rcCompletion=%Rrc, pReq=%p, pv=%p, cb=%RU32, idEvent=%RU32\n", rcCompletion, pReq, pv, cb, pReq->idEvent));

    if (pReq->idEvent != NIL_SHCLEVENTID)
    {
        int rc2;

        PSHCLEVENTPAYLOAD pPayload = NULL;
        if (   RT_SUCCESS(rcCompletion)
            && pv
            && cb)
        {
            rc2 = ShClPayloadAlloc(pReq->idEvent, pv, cb, &pPayload);
            AssertRC(rc2);
        }

        rc2 = RTCritSectEnter(&pCtx->pClient->CritSect);
        if (RT_SUCCESS(rc2))
        {
            ShClEventSignal(&pCtx->pClient->EventSrc, pReq->idEvent, pPayload);
            /* Note: Skip checking if signalling the event is successful, as it could be gone already by now. */
            RTCritSectLeave(&pCtx->pClient->CritSect);
        }
    }

    if (pReq)
        RTMemFree(pReq);

    LogRel2(("Shared Clipboard: Request for clipboard data from X11 host completed with %Rrc\n", rcCompletion));
}

DECLCALLBACK(int) ShClX11RequestDataForX11Callback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, uint32_t *pcb)
{
    LogFlowFunc(("pCtx=%p, uFmt=0x%x\n", pCtx, uFmt));

    if (pCtx->fShuttingDown)
    {
        /* The shared clipboard is disconnecting. */
        LogRel(("Shared Clipboard: Host requested guest clipboard data after guest had disconnected\n"));
        return VERR_WRONG_ORDER;
    }

    PSHCLCLIENT pClient = pCtx->pClient;
    AssertPtr(pClient);

    RTCritSectEnter(&pClient->CritSect);

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /*
     * Note: We always return a generic URI list here.
     *       As we don't know which Atom target format was requested by the caller, the X11 clipboard codes needs
     *       to decide & transform the list into the actual clipboard Atom target format the caller wanted.
     */
    if (uFmt == VBOX_SHCL_FMT_URI_LIST)
    {
        PSHCLTRANSFER pTransfer;
        rc = shClSvcTransferStart(pCtx->pClient,
                                  SHCLTRANSFERDIR_FROM_REMOTE, SHCLSOURCE_REMOTE,
                                  &pTransfer);
        if (RT_SUCCESS(rc))
        {

        }
        else
            LogRel(("Shared Clipboard: Initializing read transfer from guest failed with %Rrc\n", rc));

        *ppv = NULL;
        *pcb = 0;

        rc = VERR_NO_DATA;
    }
#endif

    if (RT_SUCCESS(rc))
    {
        /* Request data from the guest. */
        SHCLEVENTID idEvent;
        rc = ShClSvcGuestDataRequest(pCtx->pClient, uFmt, &idEvent);
        if (RT_SUCCESS(rc))
        {
            RTCritSectLeave(&pClient->CritSect);

            PSHCLEVENTPAYLOAD pPayload;
            rc = ShClEventWait(&pCtx->pClient->EventSrc, idEvent, 30 * 1000, &pPayload);
            if (RT_SUCCESS(rc))
            {
                if (   !pPayload
                    || !pPayload->cbData)
                {
                    rc = VERR_NO_DATA;
                }
                else
                {
                    *ppv = pPayload->pvData;
                    *pcb = pPayload->cbData;
                }
            }

            RTCritSectEnter(&pClient->CritSect);

            ShClEventRelease(&pCtx->pClient->EventSrc, idEvent);
            ShClEventUnregister(&pCtx->pClient->EventSrc, idEvent);
        }
    }

    RTCritSectLeave(&pClient->CritSect);

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Requesting data in format %#x for X11 host failed with %Rrc\n", uFmt, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS

int ShClBackendTransferCreate(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS_HTTP
    return ShClHttpTransferRegister(&pClient->State.pCtx->X11.HttpCtx, pTransfer);
#else
    RT_NOREF(pClient, pTransfer);
#endif
    return VERR_NOT_IMPLEMENTED;
}

int ShClBackendTransferDestroy(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS_HTTP
    return ShClHttpTransferUnregister(&pClient->State.pCtx->X11.HttpCtx, pTransfer);
#else
    RT_NOREF(pClient, pTransfer);
#endif

    return VINF_SUCCESS;
}

int ShClBackendTransferGetRoots(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
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

