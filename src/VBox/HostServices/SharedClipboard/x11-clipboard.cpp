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

/** @todo create a clipboard log group */
#define LOG_GROUP LOG_GROUP_HGCM

#include <string.h>

#include <iprt/asm.h>        /* For atomic operations */
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>

#include <VBox/log.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include "VBoxClipboard.h"

/** Global context information used by the host glue for the X11 clipboard
 * backend */
struct _VBOXCLIPBOARDCONTEXT
{
    /** Since the clipboard data moves asynchronously, we use an event
     * semaphore to wait for it.  When a function issues a request for
     * clipboard data it must wait for this semaphore, which is triggered
     * when the data arrives. */
    RTSEMEVENT waitForData;
    /** Who (if anyone) is currently waiting for data?  Used for sanity
     * checks when data arrives. */
    volatile uint32_t waiter;
    /** This mutex is grabbed during any critical operations on the clipboard
     * which might clash with others. */
    RTSEMMUTEX clipboardMutex;

    /** Pointer to the client data structure */
    VBOXCLIPBOARDCLIENTDATA *pClient;
};

/* Only one client is supported. There seems to be no need for more clients. 
 */
static VBOXCLIPBOARDCONTEXT g_ctxHost;

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
    volatile VBOXCLIPBOARDCLIENTDATA *pClient = pCtx->pClient;

    LogFlowFunc(("u32Format=%02X\n", u32Format));
    if (pClient == NULL)
    {
        /* This can legitimately happen if we disconnect during a request for
         * data from X11. */
        LogFunc(("host requested guest clipboard data after guest had disconnected.\n"));
        VBoxX11ClipboardAnnounceVBoxFormat(0);
        pCtx->waiter = NONE;
        return VERR_TIMEOUT;
    }
    /* Assert that no other transfer is in process (requests are serialised)
     * and that the last transfer cleaned up properly. */
    AssertLogRelReturn(   pClient->data.pv == NULL
                       && pClient->data.cb == 0
                       && pClient->data.u32Format == 0,
                       VERR_WRONG_ORDER
                      );
    /* No one else (X11 or VBox) should currently be waiting.  The first because
     * requests from X11 are serialised and the second because VBox previously
     * grabbed the clipboard, so it should not be waiting for data from us. */
    AssertLogRelReturn (ASMAtomicCmpXchgU32(&pCtx->waiter, X11, NONE), VERR_DEADLOCK);
    /* Request data from VBox */
    vboxSvcClipboardReportMsg(pCtx->pClient,
                              VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA,
                              u32Format);
    /* Which will signal us when it is ready.  We use a timeout here because
     * we can't be sure that the guest will behave correctly. */
    int rc = RTSemEventWait(pCtx->waitForData, CLIPBOARDTIMEOUT);
    if (rc == VERR_TIMEOUT)
        rc = VINF_SUCCESS;  /* Timeout handling follows. */
    /* Now we have a potential race between the HGCM thread delivering the data
     * and our setting waiter to NONE to say that we are no longer waiting for
     * it.  We solve this as follows: both of these operations are done under
     * the clipboard mutex.  The HGCM thread will only deliver the data if we
     * are still waiting after it acquires the mutex.  After we release the
     * mutex, we finally do our check to see whether the data was delivered. */
    RTSemMutexRequest(g_ctxHost.clipboardMutex, RT_INDEFINITE_WAIT);
    pCtx->waiter = NONE;
    RTSemMutexRelease(g_ctxHost.clipboardMutex);
    AssertLogRelRCSuccess(rc);
    if (RT_FAILURE(rc))
    {
        /* I believe this should not happen.  Wait until the assertions arrive
         * to prove the contrary. */
        RTMemFree(pClient->data.pv);
        g_ctxHost.pClient->data.pv = 0;
        g_ctxHost.pClient->data.cb = 0;
        g_ctxHost.pClient->data.u32Format = 0;
        vboxClipboardFormatAnnounce(NULL, 0);
        return rc;
    }
    if (pClient->data.pv == NULL)
        return VERR_TIMEOUT;
    LogFlowFunc(("wait completed.  Returning.\n"));
    *ppv = pClient->data.pv;
    *pcb = pClient->data.cb;
    g_ctxHost.pClient->data.pv = 0;
    g_ctxHost.pClient->data.cb = 0;
    g_ctxHost.pClient->data.u32Format = 0;
    return VINF_SUCCESS;
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
    int rc = VINF_SUCCESS;
    LogRel(("Initializing host clipboard service\n"));
    RTSemEventCreate(&g_ctxHost.waitForData);
    RTSemMutexCreate(&g_ctxHost.clipboardMutex);
    rc = VBoxX11ClipboardInitX11(&g_ctxHost);
    if (RT_FAILURE(rc))
    {
        RTSemEventDestroy(g_ctxHost.waitForData);
        RTSemMutexDestroy(g_ctxHost.clipboardMutex);
        LogRel(("Failed to start the host shared clipboard service.\n"));
    }
    return rc;
}

/**
 * Terminate the host side of the shared clipboard.
 * @note  host glue code
 */
void vboxClipboardDestroy (void)
{
    int rc = VINF_SUCCESS;
    LogRelFunc(("shutting down host clipboard\n"));
    /* Drop the reference to the client, in case it is still there.  This
     * will cause any outstanding clipboard data requests from X11 to fail
     * immediately. */
    g_ctxHost.pClient = NULL;
    /* The backend may be waiting for data from VBox.  At this point it is no
     * longer going to arrive, and we must release it to allow the event
     * loop to terminate.  In this case the buffer where VBox would have
     * written the clipboard data will still be empty and we will just
     * return "no data" to the backend.  Any subsequent attempts to get the
     * data from VBox will fail immediately as the client reference is gone.
     */
    /** @note  This has been made unconditional, as it should do no harm
     *         even if we are not waiting. */
    RTSemEventSignal(g_ctxHost.waitForData);
    rc = VBoxX11ClipboardTermX11();
    if (RT_SUCCESS(rc))
    {
        /* We can safely destroy these as the backend has exited
         * successfully and no other calls from the host code should be
         * forthcoming. */
        /** @todo  can the backend fail to exit successfully?  What then? */
        RTSemEventDestroy(g_ctxHost.waitForData);
        RTSemMutexDestroy(g_ctxHost.clipboardMutex);
    }
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
    LogFlowFunc(("\n"));
    /* Only one client is supported for now */
    AssertLogRelReturn(g_ctxHost.pClient == 0, VERR_NOT_SUPPORTED);
    pClient->pCtx = &g_ctxHost;
    pClient->pCtx->pClient = pClient;
    /** The pClient pointer is a dummy anyway, as we only support a single
     * client at a time. */
    rc = VBoxX11ClipboardStartX11(X11 /* initial owner */);
    return rc;
}

/**
 * Synchronise the contents of the host clipboard with the guest, called
 * after a save and restore of the guest.
 * @note  Host glue code
 */
int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient)
{

    /* On a Linux host, the guest should never synchronise/cache its
     * clipboard contents, as we have no way of reliably telling when the
     * host clipboard data changes.  So instead of synchronising, we tell
     * the guest to empty its clipboard, and we set the cached flag so that
     * we report formats to the guest next time we poll for them. */
    /** @note  This has been changed so that the message is sent even if
     *         X11 is not available. */
    vboxSvcClipboardReportMsg (g_ctxHost.pClient,
                               VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, 0);
    VBoxX11ClipboardRequestSyncX11();

    return VINF_SUCCESS;
}

/**
 * Shut down the shared clipboard service and "disconnect" the guest.
 * @note  Host glue code
 */
void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *)
{
    LogFlow(("vboxClipboardDisconnect\n"));

    RTSemMutexRequest(g_ctxHost.clipboardMutex, RT_INDEFINITE_WAIT);
    g_ctxHost.pClient = NULL;
    VBoxX11ClipboardStopX11();
    RTSemMutexRelease(g_ctxHost.clipboardMutex);
}

/**
 * VBox is taking possession of the shared clipboard.
 *
 * @param pClient    Context data for the guest system
 * @param u32Formats Clipboard formats the guest is offering
 * @note  Host glue code
 */
void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Formats)
{
    VBoxX11ClipboardAnnounceVBoxFormat (u32Formats);
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
    int rc = VINF_SUCCESS;
    VBOXCLIPBOARDREQUEST request;
    /* No one else (VBox or X11) should currently be waiting.  The first
     * because requests from VBox are serialised and the second because X11
     * previously grabbed the clipboard, so it should not be waiting for
     * data from us. */
    AssertLogRelReturn (ASMAtomicCmpXchgU32(&g_ctxHost.waiter, VB, NONE),
                        VERR_DEADLOCK);
    request.pv = pv;
    request.cb = cb;
    request.pcbActual = pcbActual;
    rc = VBoxX11ClipboardReadX11Data(u32Format, &request);
    g_ctxHost.waiter = NONE;
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
void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv, uint32_t cb, uint32_t u32Format)
{
/* Assume that if this gets called at all then the X11 backend is running. */
#if 0
    if (!g_fHaveX11)
        return;
#endif

    LogFlowFunc (("called\n"));

    /* Assert that no other transfer is in process (requests are serialised)
     * or has not cleaned up properly. */
    AssertLogRelReturnVoid (   pClient->data.pv == NULL
                            && pClient->data.cb == 0
                            && pClient->data.u32Format == 0);

    /* Grab the mutex and check that X11 is still waiting for the data before
     * delivering it.  See the explanation in VBoxX11ClipboardReadVBoxData. */
    RTSemMutexRequest(g_ctxHost.clipboardMutex, RT_INDEFINITE_WAIT);
    if (g_ctxHost.waiter == X11 && cb > 0)
    {
        pClient->data.pv = RTMemAlloc (cb);

        if (pClient->data.pv)
        {
            memcpy (pClient->data.pv, pv, cb);
            pClient->data.cb = cb;
            pClient->data.u32Format = u32Format;
        }
    }
    RTSemMutexRelease(g_ctxHost.clipboardMutex);

    RTSemEventSignal(g_ctxHost.waitForData);
}

