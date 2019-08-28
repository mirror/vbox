/* $Id$ */
/** @file
 * Shared Clipboard Service - Linux host.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/err.h>

#include "VBoxSharedClipboardSvc-internal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
struct _VBOXCLIPBOARDREQFROMVBOX;
typedef struct _VBOXCLIPBOARDREQFROMVBOX VBOXCLIPBOARDREQFROMVBOX;

/** Global context information used by the host glue for the X11 clipboard
 * backend */
struct _VBOXCLIPBOARDCONTEXT
{
    /** This mutex is grabbed during any critical operations on the clipboard
     * which might clash with others. */
    RTCRITSECT clipboardMutex;
    /** The currently pending request for data from VBox.  NULL if there is
     * no request pending.  The protocol for completing a request is to grab
     * the critical section, check that @a pReq is not NULL, fill in the data
     * fields and set @a pReq to NULL.  The protocol for cancelling a pending
     * request is to grab the critical section and set pReq to NULL.
     * It is an error if a request arrives while another one is pending, and
     * the backend is responsible for ensuring that this does not happen. */
    VBOXCLIPBOARDREQFROMVBOX *pReq;
    /** Pointer to the opaque X11 backend structure */
    CLIPBACKEND *pBackend;
    /** Pointer to the VBox host client data structure. */
    PVBOXCLIPBOARDCLIENT pClient;
    /** We set this when we start shutting down as a hint not to post any new
     * requests. */
    bool fShuttingDown;
};



/**
 * Report formats available in the X11 clipboard to VBox.
 * @param  pCtx        Opaque context pointer for the glue code
 * @param  u32Formats  The formats available
 * @note  Host glue code
 */
void ClipReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Formats)
{
    LogFlowFunc(("pCtx=%p, u32Formats=%02X\n", pCtx, u32Formats));

    vboxSvcClipboardOldReportMsg(pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS_WRITE, u32Formats);
}

/**
 * Initialise the host side of the shared clipboard.
 * @note  Host glue code
 */
int VBoxClipboardSvcImplInit(void)
{
    return VINF_SUCCESS;
}

/**
 * Terminate the host side of the shared clipboard.
 * @note  host glue code
 */
void VBoxClipboardSvcImplDestroy(void)
{

}

/**
 * Connect a guest to the shared clipboard.
 * @note  host glue code
 * @note  on the host, we assume that some other application already owns
 *        the clipboard and leave ownership to X11.
 */
int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENT pClient, bool fHeadless)
{
    int rc = VINF_SUCCESS;

    LogRel(("Starting host clipboard service\n"));

    PVBOXCLIPBOARDCONTEXT pCtx = (PVBOXCLIPBOARDCONTEXT)RTMemAllocZ(sizeof(VBOXCLIPBOARDCONTEXT));
    if (!pCtx)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        RTCritSectInit(&pCtx->clipboardMutex);
        CLIPBACKEND *pBackend = ClipConstructX11(pCtx, fHeadless);
        if (!pBackend)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            pCtx->pBackend = pBackend;
            pClient->State.pCtx = pCtx;
            pCtx->pClient = pClient;

            rc = ClipStartX11(pBackend, true /* grab shared clipboard */);
            if (RT_FAILURE(rc))
                ClipDestructX11(pBackend);
        }

        if (RT_FAILURE(rc))
        {
            RTCritSectDelete(&pCtx->clipboardMutex);
            RTMemFree(pCtx);
        }
    }

    if (RT_FAILURE(rc))
        LogRel(("Failed to initialize the Shared Clipboard host service, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called
 * after a save and restore of the guest.
 * @note  Host glue code
 */
int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENT pClient)
{
    LogFlowFuncEnter();

    /* Tell the guest we have no data in case X11 is not available.  If
     * there is data in the host clipboard it will automatically be sent to
     * the guest when the clipboard starts up. */
    return vboxSvcClipboardOldReportMsg(pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS_WRITE, 0);
}

/**
 * Shut down the shared clipboard service and "disconnect" the guest.
 * @note  Host glue code
 */
int VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENT pClient)
{
    LogFlowFuncEnter();

    LogRel(("Stopping the host clipboard service\n"));

    PVBOXCLIPBOARDCONTEXT pCtx = pClient->State.pCtx;

    /* Drop the reference to the client, in case it is still there.  This
     * will cause any outstanding clipboard data requests from X11 to fail
     * immediately. */
    pCtx->fShuttingDown = true;

    /* If there is a currently pending request, release it immediately. */
    SHAREDCLIPBOARDDATABLOCK dataBlock = { 0, NULL, 0 };
    VBoxClipboardSvcImplWriteData(pClient, NULL, &dataBlock);

    int rc = ClipStopX11(pCtx->pBackend);
    /** @todo handle this slightly more reasonably, or be really sure
     *        it won't go wrong. */
    AssertRC(rc);

    if (RT_SUCCESS(rc))  /* And if not? */
    {
        ClipDestructX11(pCtx->pBackend);
        RTCritSectDelete(&pCtx->clipboardMutex);
        RTMemFree(pCtx);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * VBox is taking possession of the shared clipboard.
 *
 * @param pClient               Context data for the guest system.
 * @param pCmdCtx               Command context to use.
 * @param pFormats              Clipboard formats the guest is offering.
 */
int VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx,
                                       PSHAREDCLIPBOARDFORMATDATA pFormats)
{
    RT_NOREF(pCmdCtx);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    if (pFormats->uFormats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST) /* No URI support yet. */
        return VINF_SUCCESS;
#endif

    ClipAnnounceFormatToX11(pClient->State.pCtx->pBackend, pFormats->uFormats);

    return VINF_SUCCESS;
}

/** Structure describing a request for clipoard data from the guest. */
struct _CLIPREADCBREQ
{
    /** Where to write the returned data to. */
    void *pv;
    /** The size of the buffer in pv */
    uint32_t cb;
    /** The actual size of the data written */
    uint32_t *pcbActual;
};

/**
 * Called when VBox wants to read the X11 clipboard.
 *
 * @returns VINF_SUCCESS on successful completion
 * @returns VINF_HGCM_ASYNC_EXECUTE if the operation will complete
 *          asynchronously
 * @returns iprt status code on failure
 *
 * @param pClient               Context information about the guest VM
 * @param pCmdCtx               Command context to use.
 * @param pData                 Data block to put read data into.
 * @param pcbActual             Where to write the actual size of the written data
 *
 * @note   We always fail or complete asynchronously.
 * @note   On success allocates a CLIPREADCBREQ structure which must be
 *         freed in ClipCompleteDataRequestFromX11 when it is called back from
 *         the backend code.
 *
 */
int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENT pClient,
                                 PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx, PSHAREDCLIPBOARDDATABLOCK pData, uint32_t *pcbActual)
{
    RT_NOREF(pCmdCtx);

    LogFlowFunc(("pClient=%p, uFormat=%02X, pv=%p, cb=%u, pcbActual=%p\n",
                 pClient, pData->uFormat, pData->pvData, pData->cbData, pcbActual));

    int rc = VINF_SUCCESS;

    CLIPREADCBREQ *pReq = (CLIPREADCBREQ *)RTMemAlloc(sizeof(CLIPREADCBREQ));
    if (!pReq)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        pReq->pv = pData->pvData;
        pReq->cb = pData->cbData;
        pReq->pcbActual = pcbActual;
        rc = ClipRequestDataFromX11(pClient->State.pCtx->pBackend, pData->uFormat, pReq);
        if (RT_SUCCESS(rc))
            rc = VINF_HGCM_ASYNC_EXECUTE;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Complete a request from VBox for the X11 clipboard data.  The data should
 * be written to the buffer provided in the initial request.
 * @param  pCtx  request context information
 * @param  rc    the completion status of the request
 * @param  pReq  request
 * @param  pv    address
 * @param  cb    size
 *
 * @todo   change this to deal with the buffer issues rather than offloading
 *         them onto the caller
 */
void ClipCompleteDataRequestFromX11(VBOXCLIPBOARDCONTEXT *pCtx, int rc,
                                    CLIPREADCBREQ *pReq, void *pv, uint32_t cb)
{
    if (cb <= pReq->cb && cb != 0)
        memcpy(pReq->pv, pv, cb);

    RTMemFree(pReq);

    vboxSvcClipboardOldCompleteReadData(pCtx->pClient, rc, cb);
}

/** A request for clipboard data from VBox */
struct _VBOXCLIPBOARDREQFROMVBOX
{
    /** Data received */
    void *pv;
    /** The size of the data */
    uint32_t cb;
    /** Format of the data */
    uint32_t format;
    /** A semaphore for waiting for the data */
    RTSEMEVENT finished;
};

/** Wait for clipboard data requested from VBox to arrive. */
static int clipWaitForDataFromVBox(VBOXCLIPBOARDCONTEXT *pCtx,
                                   VBOXCLIPBOARDREQFROMVBOX *pReq,
                                   uint32_t u32Format)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pCtx=%p, pReq=%p, u32Format=%02X\n", pCtx, pReq, u32Format));

    /* Request data from VBox */
    vboxSvcClipboardOldReportMsg(pCtx->pClient,
                              VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA,
                              u32Format);
    /* Which will signal us when it is ready.  We use a timeout here
     * because we can't be sure that the guest will behave correctly.
     */
    rc = RTSemEventWait(pReq->finished, CLIPBOARD_TIMEOUT);

    /* If the request hasn't yet completed then we cancel it.  We use
     * the critical section to prevent these operations colliding. */
    RTCritSectEnter(&pCtx->clipboardMutex);

    /* The data may have arrived between the semaphore timing out and
     * our grabbing the mutex. */
    if (rc == VERR_TIMEOUT && pReq->pv != NULL)
        rc = VINF_SUCCESS;
    if (pCtx->pReq == pReq)
        pCtx->pReq = NULL;
    Assert(pCtx->pReq == NULL);

    RTCritSectLeave(&pCtx->clipboardMutex);

    if (RT_SUCCESS(rc) && (pReq->pv == NULL))
        rc = VERR_NO_DATA;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** Post a request for clipboard data to VBox/the guest and wait for it to be
 * completed. */
static int clipRequestDataFromVBox(VBOXCLIPBOARDCONTEXT *pCtx, VBOXCLIPBOARDREQFROMVBOX *pReq, uint32_t u32Format)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pCtx=%p, pReq=%p, u32Format=%02X\n", pCtx, pReq, u32Format));

    /* Start by "posting" the request for the next invocation of
     * vboxClipboardWriteData. */
    RTCritSectEnter(&pCtx->clipboardMutex);

    if (pCtx->pReq != NULL)
    {
        /* This would be a violation of the protocol, see the comments in the
         * context structure definition. */
        Assert(false);
        rc = VERR_WRONG_ORDER;
    }
    else
        pCtx->pReq = pReq;

    RTCritSectLeave(&pCtx->clipboardMutex);

    if (RT_SUCCESS(rc))
        rc = clipWaitForDataFromVBox(pCtx, pReq, u32Format);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Send a request to VBox to transfer the contents of its clipboard to X11.
 *
 * @param  pCtx      Pointer to the host clipboard structure
 * @param  u32Format The format in which the data should be transferred
 * @param  ppv       On success and if pcb > 0, this will point to a buffer
 *                   to be freed with RTMemFree containing the data read.
 * @param  pcb       On success, this contains the number of bytes of data
 *                   returned
 * @note   Host glue code.
 */
int ClipRequestDataForX11(VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format, void **ppv, uint32_t *pcb)
{
    VBOXCLIPBOARDREQFROMVBOX request = { NULL, 0, 0, NIL_RTSEMEVENT };

    LogFlowFunc(("pCtx=%p, u32Format=%02X, ppv=%p, pcb=%p\n", pCtx, u32Format, ppv, pcb));

    if (pCtx->fShuttingDown)
    {
        /* The shared clipboard is disconnecting. */
        LogRel(("Clipboard: Host requested guest clipboard data after guest had disconnected\n"));
        return VERR_WRONG_ORDER;
    }

    int rc = RTSemEventCreate(&request.finished);
    if (RT_SUCCESS(rc))
    {
        rc = clipRequestDataFromVBox(pCtx, &request, u32Format);
        RTSemEventDestroy(request.finished);
    }

    if (RT_SUCCESS(rc))
    {
        *ppv = request.pv;
        *pcb = request.cb;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Called when we have requested data from VBox and that data has arrived.
 *
 * @param pClient               Context information about the guest VM.
 * @param pData                 Data block to write to clipboard.
 */
int VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENT pClient,
                                  PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx, PSHAREDCLIPBOARDDATABLOCK pData)
{
    RT_NOREF(pCmdCtx);

    LogFlowFunc(("pClient=%p, pv=%p (%.*ls), cb=%u, uFormat=%02X\n",
                 pClient, pData->pvData, pData->cbData / 2, pData->pvData, pData->cbData, pData->uFormat));

    PVBOXCLIPBOARDCONTEXT pCtx = pClient->State.pCtx;

    /* Grab the mutex and check whether there is a pending request for data. */
    RTCritSectEnter(&pCtx->clipboardMutex);

    VBOXCLIPBOARDREQFROMVBOX *pReq = pCtx->pReq;
    if (pReq != NULL)
    {
        if (pData->cbData > 0)
        {
            pReq->pv = RTMemDup(pData->pvData, pData->cbData);
            if (pReq->pv != NULL)  /* NULL may also mean no memory... */
            {
                pReq->cb     = pData->cbData;
                pReq->format = pData->uFormat;
            }
        }

        /* Signal that the request has been completed. */
        RTSemEventSignal(pReq->finished);
        pCtx->pReq = NULL;
    }

    RTCritSectLeave(&pCtx->clipboardMutex);

    return VINF_SUCCESS;
}

#if 0
int VBoxClipboardSvcImplURIReadDir(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClient, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteDir(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClient, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileHdr(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClient, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileHdr(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClient, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClient, pFileData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClient, pFileData);
    return VERR_NOT_IMPLEMENTED;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

#ifdef TESTCASE
# include <iprt/initterm.h>
# include <iprt/stream.h>

# define TEST_NAME "tstClipboardX11-2"

struct _CLIPBACKEND
{
    uint32_t formats;
    struct _READDATA
    {
        uint32_t format;
        int rc;
        CLIPREADCBREQ *pReq;
    } readData;
    struct _COMPLETEREAD
    {
        int rc;
        uint32_t cbActual;
    } completeRead;
    struct _WRITEDATA
    {
        void *pv;
        uint32_t cb;
        uint32_t format;
        bool timeout;
    } writeData;
    struct _REPORTDATA
    {
        uint32_t format;
    } reportData;
};

int vboxSvcClipboardOldReportMsg(PVBOXCLIPBOARDCLIENT pClient, uint32_t uMsg, uint32_t uFormats)
{
    RT_NOREF(uFormats);
    CLIPBACKEND *pBackend = pClient->State.pCtx->pBackend;

    int rc;

    if (   (uMsg == VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA)
        && !pBackend->writeData.timeout)
    {
        SHAREDCLIPBOARDDATABLOCK dataBlock;
        RT_ZERO(dataBlock);

        dataBlock.pvData  = pBackend->writeData.pv;
        dataBlock.cbData  = pBackend->writeData.cb;
        dataBlock.uFormat = pBackend->writeData.format;

        rc = VBoxClipboardSvcImplWriteData(pClient, NULL /* pCmdCtx */, &dataBlock);
    }
    else
        rc = VERR_NOT_SUPPORTED;

    return rc;
}

int vboxSvcClipboardOldCompleteReadData(PVBOXCLIPBOARDCLIENT pClient, int rc, uint32_t cbActual)
{
    CLIPBACKEND *pBackend = pClient->State.pCtx->pBackend;
    pBackend->completeRead.rc = rc;
    pBackend->completeRead.cbActual = cbActual;

    return VINF_SUCCESS;
}

CLIPBACKEND* ClipConstructX11(VBOXCLIPBOARDCONTEXT *pFrontend, bool)
{
    RT_NOREF(pFrontend);
    return (CLIPBACKEND *)RTMemAllocZ(sizeof(CLIPBACKEND));
}

void ClipDestructX11(CLIPBACKEND *pBackend)
{
    RTMemFree(pBackend);
}

int ClipStartX11(CLIPBACKEND *pBackend, bool)
{
    RT_NOREF(pBackend);
    return VINF_SUCCESS;
}

int ClipStopX11(CLIPBACKEND *pBackend)
{
    RT_NOREF1(pBackend);
    return VINF_SUCCESS;
}

int ClipAnnounceFormatToX11(CLIPBACKEND *pBackend, VBOXCLIPBOARDFORMATS vboxFormats)
{
    pBackend->formats = vboxFormats;
    return VINF_SUCCESS;
}

extern int ClipRequestDataFromX11(CLIPBACKEND *pBackend, VBOXCLIPBOARDFORMAT vboxFormat,
                                  CLIPREADCBREQ *pReq)
{
    pBackend->readData.format = vboxFormat;
    pBackend->readData.pReq = pReq;
    return pBackend->readData.rc;
}

int main()
{
    VBOXCLIPBOARDCLIENT client;
    unsigned cErrors = 0;
    int rc = RTR3InitExeNoArguments(0);
    RTPrintf(TEST_NAME ": TESTING\n");
    AssertRCReturn(rc, 1);
    rc = VBoxClipboardSvcImplConnect(&client, false);
    CLIPBACKEND *pBackend = client.State.pCtx->pBackend;
    AssertRCReturn(rc, 1);

    SHAREDCLIPBOARDFORMATDATA formatData;
    RT_ZERO(formatData);
    formatData.uFormats = VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;

    VBoxClipboardSvcImplFormatAnnounce(&client, NULL /* pCmdCtx */, &formatData);

    if (pBackend->formats != VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        RTPrintf(TEST_NAME ": vboxClipboardFormatAnnounce failed with VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT\n");
        ++cErrors;
    }
    pBackend->readData.rc = VINF_SUCCESS;

    client.State.Old.asyncRead.callHandle = (VBOXHGCMCALLHANDLE)pBackend;
    client.State.Old.asyncRead.paParms = (VBOXHGCMSVCPARM *)&client;

    uint32_t u32Dummy;

    SHAREDCLIPBOARDDATABLOCK dataBlock;
    RT_ZERO(dataBlock);
    dataBlock.pvData  = &u32Dummy;
    dataBlock.cbData  = 42;
    dataBlock.uFormat = VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;

    rc = VBoxClipboardSvcImplReadData(&client, &dataBlock, &u32Dummy);
    if (rc != VINF_HGCM_ASYNC_EXECUTE)
    {
        RTPrintf(TEST_NAME ": vboxClipboardReadData returned %Rrc\n", rc);
        ++cErrors;
    }
    else
    {
        if (   pBackend->readData.format != VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT
            || pBackend->readData.pReq->pv != &u32Dummy
            || pBackend->readData.pReq->cb != 42
            || pBackend->readData.pReq->pcbActual != &u32Dummy)
        {
            RTPrintf(TEST_NAME ": format=%u, pReq->pv=%p, pReq->cb=%u, pReq->pcbActual=%p\n",
                     pBackend->readData.format, pBackend->readData.pReq->pv,
                     pBackend->readData.pReq->cb,
                     pBackend->readData.pReq->pcbActual);
            ++cErrors;
        }
        else
        {
            ClipCompleteDataRequestFromX11(client.State.pCtx, VERR_NO_DATA,
                                           pBackend->readData.pReq, NULL, 43);
            if (   pBackend->completeRead.rc != VERR_NO_DATA
                || pBackend->completeRead.cbActual != 43)
            {
                RTPrintf(TEST_NAME ": rc=%Rrc, cbActual=%u\n",
                         pBackend->completeRead.rc,
                         pBackend->completeRead.cbActual);
                ++cErrors;
            }
        }
    }
    void *pv;
    uint32_t cb;
    pBackend->writeData.pv = (void *)"testing";
    pBackend->writeData.cb = sizeof("testing");
    pBackend->writeData.format = 1234;
    pBackend->reportData.format = 4321;  /* XX this should be handled! */
    rc = ClipRequestDataForX11(client.State.pCtx, 23, &pv, &cb);
    if (   rc != VINF_SUCCESS
        || strcmp((const char *)pv, "testing") != 0
        || cb != sizeof("testing"))
    {
        RTPrintf("rc=%Rrc, pv=%p, cb=%u\n", rc, pv, cb);
        ++cErrors;
    }
    else
        RTMemFree(pv);
    pBackend->writeData.timeout = true;
    rc = ClipRequestDataForX11(client.State.pCtx, 23, &pv, &cb);
    if (rc != VERR_TIMEOUT)
    {
        RTPrintf("rc=%Rrc, expected VERR_TIMEOUT\n", rc);
        ++cErrors;
    }
    pBackend->writeData.pv = NULL;
    pBackend->writeData.cb = 0;
    pBackend->writeData.timeout = false;
    rc = ClipRequestDataForX11(client.State.pCtx, 23, &pv, &cb);
    if (rc != VERR_NO_DATA)
    {
        RTPrintf("rc=%Rrc, expected VERR_NO_DATA\n", rc);
        ++cErrors;
    }
    /* Data arriving after a timeout should *not* cause any segfaults or
     * memory leaks.  Check with Valgrind! */
    RT_ZERO(dataBlock);

    const char *pszString = "tested";

    dataBlock.pvData  = (void *)pszString;
    dataBlock.cbData  = (uint32_t)(strlen(pszString) + 1);
    dataBlock.uFormat = 999;

    VBoxClipboardSvcImplWriteData(&client, NULL /* pCmdCtx */, &dataBlock);

    VBoxClipboardSvcImplDisconnect(&client);

    RTPrintf(TEST_NAME ": errors: %u\n", cErrors);

    return cErrors > 0 ? 1 : 0;
}
#endif  /* TESTCASE */

