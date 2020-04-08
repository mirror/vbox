/* $Id$ */
/** @file
 * Shared Clipboard Service - Mac OS X host.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
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
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/process.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "darwin-pasteboard.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Global clipboard context information */
typedef struct SHCLCONTEXT
{
    /** We have a separate thread to poll for new clipboard content. */
    RTTHREAD                hThread;
    /** Termination indicator.   */
    bool volatile           fTerminate;
    /** The reference to the current pasteboard */
    PasteboardRef           hPasteboard;
    /** Shared clipboard client. */
    PSHCLCLIENT             pClient;
    /** Random 64-bit number embedded into szGuestOwnershipFlavor. */
    uint64_t                idGuestOwnership;
    /** The guest ownership flavor (type) string. */
    char                    szGuestOwnershipFlavor[64];
} SHCLCONTEXT;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Only one client is supported. There seems to be no need for more clients. */
static SHCLCONTEXT g_ctx;


/**
 * Checks if something is present on the clipboard and calls shclSvcReportMsg.
 *
 * @returns IPRT status code (ignored).
 * @param   pCtx    The context.
 */
static int vboxClipboardChanged(SHCLCONTEXT *pCtx)
{
    if (pCtx->pClient == NULL)
        return VINF_SUCCESS;

    uint32_t fFormats = 0;
    bool fChanged = false;
    /* Retrieve the formats currently in the clipboard and supported by vbox */
    int rc = queryNewPasteboardFormats(pCtx->hPasteboard, &fFormats, &fChanged);
    if (   RT_SUCCESS(rc)
        && fChanged)
        rc = ShClSvcHostReportFormats(pCtx->pClient, fFormats);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @callback_method_impl{FNRTTHREAD, The poller thread.
 *
 * This thread will check for the arrival of new data on the clipboard.}
 */
static DECLCALLBACK(int) vboxClipboardThread(RTTHREAD ThreadSelf, void *pvUser)
{
    SHCLCONTEXT *pCtx = (SHCLCONTEXT *)pvUser;
    AssertPtr(pCtx);
    LogFlowFuncEnter();

    while (!pCtx->fTerminate)
    {
        /* call this behind the lock because we don't know if the api is
           thread safe and in any case we're calling several methods. */
        ShClSvcLock();
        vboxClipboardChanged(pCtx);
        ShClSvcUnlock();

        /* Sleep for 200 msecs before next poll */
        RTThreadUserWait(ThreadSelf, 200);
    }

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


int ShClSvcImplInit(void)
{
    g_ctx.fTerminate = false;

    int rc = initPasteboard(&g_ctx.hPasteboard);
    AssertRCReturn(rc, rc);

    rc = RTThreadCreate(&g_ctx.hThread, vboxClipboardThread, &g_ctx, 0,
                        RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
    if (RT_FAILURE(rc))
    {
        g_ctx.hThread = NIL_RTTHREAD;
        destroyPasteboard(&g_ctx.hPasteboard);
    }

    return rc;
}

void ShClSvcImplDestroy(void)
{
    /*
     * Signal the termination of the polling thread and wait for it to respond.
     */
    ASMAtomicWriteBool(&g_ctx.fTerminate, true);
    int rc = RTThreadUserSignal(g_ctx.hThread);
    AssertRC(rc);
    rc = RTThreadWait(g_ctx.hThread, RT_INDEFINITE_WAIT, NULL);
    AssertRC(rc);

    /*
     * Destroy the hPasteboard and uninitialize the global context record.
     */
    destroyPasteboard(&g_ctx.hPasteboard);
    g_ctx.hThread = NIL_RTTHREAD;
    g_ctx.pClient = NULL;
}

int ShClSvcImplConnect(PSHCLCLIENT pClient, bool fHeadless)
{
    RT_NOREF(fHeadless);

    if (g_ctx.pClient != NULL)
    {
        /* One client only. */
        return VERR_NOT_SUPPORTED;
    }

    ShClSvcLock();

    pClient->State.pCtx = &g_ctx;
    pClient->State.pCtx->pClient = pClient;

    ShClSvcUnlock();

    return VINF_SUCCESS;
}

int ShClSvcImplSync(PSHCLCLIENT pClient)
{
    /* Sync the host clipboard content with the client. */
    ShClSvcLock();

    int rc = vboxClipboardChanged(pClient->State.pCtx);

    ShClSvcUnlock();

    return rc;
}

int ShClSvcImplDisconnect(PSHCLCLIENT pClient)
{
    ShClSvcLock();

    pClient->State.pCtx->pClient = NULL;

    ShClSvcUnlock();

    return VINF_SUCCESS;
}

int ShClSvcImplFormatAnnounce(PSHCLCLIENT pClient, SHCLFORMATS fFormats)
{
    LogFlowFunc(("fFormats=%02X\n", fFormats));

    if (fFormats == VBOX_SHCL_FMT_NONE)
    {
        /* This is just an automatism, not a genuine announcement */
        return VINF_SUCCESS;
    }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    if (fFormats & VBOX_SHCL_FMT_URI_LIST) /* No transfer support yet. */
        return VINF_SUCCESS;
#endif

    SHCLCONTEXT *pCtx = pClient->State.pCtx;
    ShClSvcLock();

    /*
     * Generate a unique flavor string for this format announcement.
     */
    uint64_t idFlavor = RTRandU64();
    pCtx->idGuestOwnership = idFlavor;
    RTStrPrintf(pCtx->szGuestOwnershipFlavor, sizeof(pCtx->szGuestOwnershipFlavor),
                "org.virtualbox.sharedclipboard.%RTproc.%RX64", RTProcSelf(), idFlavor);

    /*
     * Empty the pasteboard and put our ownership indicator flavor there
     * with the stringified formats as value.
     */
    char szValue[32];
    RTStrPrintf(szValue, sizeof(szValue), "%#x", fFormats);

    takePasteboardOwnership(pCtx->hPasteboard, pCtx->idGuestOwnership, pCtx->szGuestOwnershipFlavor, szValue);

    ShClSvcUnlock();

    /*
     * Now, request the data from the guest.
     */
    return ShClSvcDataReadRequest(pClient, fFormats, NULL /* pidEvent */);
}

int ShClSvcImplReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, SHCLFORMAT fFormat,
                        void *pvData, uint32_t cbData, uint32_t *pcbActual)
{
    RT_NOREF(pCmdCtx);

    ShClSvcLock();

    /* Default to no data available. */
    *pcbActual = 0;

    int rc = readFromPasteboard(pClient->State.pCtx->hPasteboard, fFormat, pvData, cbData, pcbActual);

    ShClSvcUnlock();

    return rc;
}

int ShClSvcImplWriteData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx, SHCLFORMAT fFormat, void *pvData, uint32_t cbData)
{
    RT_NOREF(pCmdCtx);

    ShClSvcLock();

    writeToPasteboard(pClient->State.pCtx->hPasteboard, pClient->State.pCtx->idGuestOwnership, pvData, cbData, fFormat);

    ShClSvcUnlock();

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS

int ShClSvcImplTransferReadDir(PSHCLCLIENT pClient, PSHCLDIRDATA pDirData)
{
    RT_NOREF(pClient, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int ShClSvcImplTransferWriteDir(PSHCLCLIENT pClient, PSHCLDIRDATA pDirData)
{
    RT_NOREF(pClient, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int ShClSvcImplTransferReadFileHdr(PSHCLCLIENT pClient, PSHCLFILEHDR pFileHdr)
{
    RT_NOREF(pClient, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int ShClSvcImplTransferWriteFileHdr(PSHCLCLIENT pClient, PSHCLFILEHDR pFileHdr)
{
    RT_NOREF(pClient, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int ShClSvcImplTransferReadFileData(PSHCLCLIENT pClient, PSHCLFILEDATA pFileData)
{
    RT_NOREF(pClient, pFileData);
    return VERR_NOT_IMPLEMENTED;
}

int ShClSvcImplTransferWriteFileData(PSHCLCLIENT pClient, PSHCLFILEDATA pFileData)
{
    RT_NOREF(pClient, pFileData);
    return VERR_NOT_IMPLEMENTED;
}

#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

