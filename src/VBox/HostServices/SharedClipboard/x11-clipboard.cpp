/** @file
 *
 * Shared Clipboard:
 * Linux host.
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

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD

#include <string.h>

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include "VBoxClipboard.h"

/** A request for clipboard data from VBox */
typedef struct _VBOXCLIPBOARDREQFROMVBOX
{
    /** Data received */
    void *pv;
    /** The size of the data */
    uint32_t cb;
    /** Format of the data */
    uint32_t format;
    /** A semaphore for waiting for the data */
    RTSEMEVENT finished;
} VBOXCLIPBOARDREQFROMVBOX;

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
    VBOXCLIPBOARDCONTEXTX11 *pBackend;
    /** Pointer to the VBox host client data structure. */
    VBOXCLIPBOARDCLIENTDATA *pClient;
    /** We set this when we start shutting down as a hint not to post any new
     * requests. */
    bool fShuttingDown;
};

static int vboxClipboardWaitForReq(VBOXCLIPBOARDCONTEXT *pCtx,
                                   VBOXCLIPBOARDREQFROMVBOX *pReq,
                                   uint32_t u32Format)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pCtx=%p, pReq=%p, u32Format=%02X\n", pCtx, pReq, u32Format));
    /* Request data from VBox */
    vboxSvcClipboardReportMsg(pCtx->pClient,
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
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/** Post a request for clipboard data to VBox/the guest and wait for it to be
 * completed. */
static int vboxClipboardPostAndWaitForReq(VBOXCLIPBOARDCONTEXT *pCtx,
                                          VBOXCLIPBOARDREQFROMVBOX *pReq,
                                          uint32_t u32Format)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pCtx=%p, pReq=%p, u32Format=%02X\n", pCtx, pReq,
                 u32Format));
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
        rc = vboxClipboardWaitForReq(pCtx, pReq, u32Format);
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/**
 * Send a request to VBox to transfer the contents of its clipboard to X11.
 *
 * @param  pCtx      Pointer to the host clipboard structure
 * @param  u32Format The format in which the data should be transfered
 * @param  ppv       On success and if pcb > 0, this will point to a buffer
 *                   to be freed with RTMemFree containing the data read.
 * @param  pcb       On success, this contains the number of bytes of data
 *                   returned
 * @note   Host glue code.
 */
int VBoxX11ClipboardReadVBoxData (VBOXCLIPBOARDCONTEXT *pCtx,
                                   uint32_t u32Format, void **ppv,
                                   uint32_t *pcb)
{
    VBOXCLIPBOARDREQFROMVBOX request = { NULL };

    LogFlowFunc(("pCtx=%p, u32Format=%02X, ppv=%p, pcb=%p\n", pCtx,
                 u32Format, ppv, pcb));
    if (pCtx->fShuttingDown)
    {
        /* The shared clipboard is disconnecting. */
        LogFunc(("host requested guest clipboard data after guest had disconnected.\n"));
        return VERR_WRONG_ORDER;
    }
    int rc = RTSemEventCreate(&request.finished);
    if (RT_SUCCESS(rc))
    {
        rc = vboxClipboardPostAndWaitForReq(pCtx, &request,
                                            u32Format);
        RTSemEventDestroy(request.finished);
    }
    if (RT_SUCCESS(rc))
    {
        *ppv = request.pv;
        *pcb = request.cb;
    }
    LogFlowFunc(("returning %Rrc\n", rc));
    if (RT_SUCCESS(rc))
        LogFlowFunc(("*ppv=%.*ls, *pcb=%u\n", *pcb / 2, *ppv, *pcb));
    return rc;
}

/**
 * Report formats available in the X11 clipboard to VBox.
 * @param  pCtx        Opaque context pointer for the glue code
 * @param  u32Formats  The formats available
 * @note  Host glue code
 */
void VBoxX11ClipboardReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx,
                                      uint32_t u32Formats)
{
    LogFlowFunc(("called.  pCtx=%p, u32Formats=%02X\n", pCtx, u32Formats));
    vboxSvcClipboardReportMsg(pCtx->pClient,
                              VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS,
                              u32Formats);
}

/**
 * Initialise the host side of the shared clipboard.
 * @note  Host glue code
 */
int vboxClipboardInit (void)
{
    return VINF_SUCCESS;
}

/**
 * Terminate the host side of the shared clipboard.
 * @note  host glue code
 */
void vboxClipboardDestroy (void)
{

}

/**
 * Connect a guest to the shared clipboard.
 * @note  host glue code
 * @note  on the host, we assume that some other application already owns
 *        the clipboard and leave ownership to X11.
 */
int vboxClipboardConnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    int rc = VINF_SUCCESS;
    VBOXCLIPBOARDCONTEXTX11 *pBackend = NULL;

    LogRel(("Starting host clipboard service\n"));
    VBOXCLIPBOARDCONTEXT *pCtx =
        (VBOXCLIPBOARDCONTEXT *) RTMemAllocZ(sizeof(VBOXCLIPBOARDCONTEXT));
    if (!pCtx)
        rc = VERR_NO_MEMORY;
    else
    {
        RTCritSectInit(&pCtx->clipboardMutex);
        pBackend = VBoxX11ClipboardConstructX11(pCtx);
        if (pBackend == NULL)
            rc = VERR_NO_MEMORY;
        else
        {
            pCtx->pBackend = pBackend;
            pClient->pCtx = pCtx;
            pCtx->pClient = pClient;
            rc = VBoxX11ClipboardStartX11(pBackend);
        }
        if (RT_FAILURE(rc) && pBackend)
            VBoxX11ClipboardStopX11(pCtx->pBackend);
        if (RT_FAILURE(rc))
            RTCritSectDelete(&pCtx->clipboardMutex);
    }
    if (RT_FAILURE(rc))
    {
        RTMemFree(pCtx);
        LogRel(("Failed to initialise the shared clipboard\n"));
    }
    LogFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called
 * after a save and restore of the guest.
 * @note  Host glue code
 */
int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    /* Tell the guest we have no data in case X11 is not available.  If
     * there is data in the host clipboard it will automatically be sent to
     * the guest when the clipboard starts up. */
    vboxSvcClipboardReportMsg (pClient,
                               VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, 0);
    return VINF_SUCCESS;
}

/**
 * Shut down the shared clipboard service and "disconnect" the guest.
 * @note  Host glue code
 */
void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    LogFlow(("vboxClipboardDisconnect\n"));

    LogRel(("Stopping the host clipboard service\n"));
    VBOXCLIPBOARDCONTEXT *pCtx = pClient->pCtx;
    /* Drop the reference to the client, in case it is still there.  This
     * will cause any outstanding clipboard data requests from X11 to fail
     * immediately. */
    pCtx->fShuttingDown = true;
    /* If there is a currently pending request, release it immediately. */
    vboxClipboardWriteData(pClient, NULL, 0, 0);
    int rc = VBoxX11ClipboardStopX11(pCtx->pBackend);
    /** @todo handle this slightly more reasonably, or be really sure
     *        it won't go wrong. */
    AssertRC(rc);
    if (RT_SUCCESS(rc))  /* And if not? */
    {
        VBoxX11ClipboardDestructX11(pCtx->pBackend);
        RTCritSectDelete(&pCtx->clipboardMutex);
        RTMemFree(pCtx);
    }
}

/**
 * VBox is taking possession of the shared clipboard.
 *
 * @param pClient    Context data for the guest system
 * @param u32Formats Clipboard formats the guest is offering
 * @note  Host glue code
 */
void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient,
                                  uint32_t u32Formats)
{
    LogFlowFunc(("called.  pClient=%p, u32Formats=%02X\n", pClient,
                 u32Formats));
    VBoxX11ClipboardAnnounceVBoxFormat (pClient->pCtx->pBackend, u32Formats);
}

/**
 * Called when VBox wants to read the X11 clipboard.
 *
 * @param  pClient   Context information about the guest VM
 * @param  u32Format The format that the guest would like to receive the data in
 * @param  pv        Where to write the data to
 * @param  cb        The size of the buffer to write the data to
 * @param  pcbActual Where to write the actual size of the written data
 * @note   Host glue code
 */
int vboxClipboardReadData (VBOXCLIPBOARDCLIENTDATA *pClient,
                           uint32_t u32Format, void *pv, uint32_t cb,
                           uint32_t *pcbActual)
{
    LogFlowFunc(("pClient=%p, u32Format=%02X, pv=%p, cb=%u, pcbActual=%p",
                 pClient, u32Format, pv, cb, pcbActual));
    int rc = VBoxX11ClipboardReadX11Data(pClient->pCtx->pBackend, u32Format,
                                         pv, cb, pcbActual);
    LogFlowFunc(("returning %Rrc\n", rc));
    if (RT_SUCCESS(rc))
        LogFlowFunc(("*pv=%.*ls, *pcbActual=%u\n", *pcbActual / 2, pv,
                     *pcbActual));
    return rc;
}

/**
 * Called when we have requested data from VBox and that data has arrived.
 *
 * @param  pClient   Context information about the guest VM
 * @param  pv        Buffer to which the data was written
 * @param  cb        The size of the data written
 * @param  u32Format The format of the data written
 * @note   Host glue code
 */
void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient,
                             void *pv, uint32_t cb, uint32_t u32Format)
{
    LogFlowFunc (("called.  pClient=%p, pv=%p (%.*ls), cb=%u, u32Format=%02X\n",
                  pClient, pv, cb / 2, pv, cb, u32Format));

    VBOXCLIPBOARDCONTEXT *pCtx = pClient->pCtx;
    /* Grab the mutex and check whether there is a pending request for data.
     */
    RTCritSectEnter(&pCtx->clipboardMutex);
    VBOXCLIPBOARDREQFROMVBOX *pReq = pCtx->pReq;
    if (pReq != NULL)
    {
        if (cb > 0)
        {
            pReq->pv = RTMemDup(pv, cb);
            if (pReq->pv != NULL)  /* NULL may also mean no memory... */
            {
                pReq->cb = cb;
                pReq->format = u32Format;
            }            
        }
        /* Signal that the request has been completed. */
        RTSemEventSignal(pReq->finished);
    }
    pCtx->pReq = NULL;
    RTCritSectLeave(&pCtx->clipboardMutex);
}
