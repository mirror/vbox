/* $Id$ */
/** @file
 * Shared Clipboard Service - Mac OS X host.
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
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
#include <iprt/thread.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "darwin-pasteboard.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Global clipboard context information */
struct SHCLCONTEXT
{
    /** We have a separate thread to poll for new clipboard content */
    RTTHREAD                thread;
    bool volatile           fTerminate;
    /** The reference to the current pasteboard */
    PasteboardRef           pasteboard;
    PSHCLCLIENT    pClient;
};


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
    int rc = queryNewPasteboardFormats(pCtx->pasteboard, &fFormats, &fChanged);
    if (   RT_SUCCESS(rc)
        && fChanged)
        rc = ShClSvcHostReportFormats(pCtx->pClient, fFormats);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * The poller thread.
 *
 * This thread will check for the arrival of new data on the clipboard.
 *
 * @returns VINF_SUCCESS (not used).
 * @param   ThreadSelf  Our thread handle.
 * @param   pvUser      Pointer to the SHCLCONTEXT structure.
 *
 */
static int vboxClipboardThread(RTTHREAD ThreadSelf, void *pvUser)
{
    LogFlowFuncEnter();

    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    SHCLCONTEXT *pCtx = (SHCLCONTEXT *)pvUser;

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

    int rc = initPasteboard(&g_ctx.pasteboard);
    AssertRCReturn(rc, rc);

    rc = RTThreadCreate(&g_ctx.thread, vboxClipboardThread, &g_ctx, 0,
                        RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
    if (RT_FAILURE(rc))
    {
        g_ctx.thread = NIL_RTTHREAD;
        destroyPasteboard(&g_ctx.pasteboard);
    }

    return rc;
}

void ShClSvcImplDestroy(void)
{
    /*
     * Signal the termination of the polling thread and wait for it to respond.
     */
    ASMAtomicWriteBool(&g_ctx.fTerminate, true);
    int rc = RTThreadUserSignal(g_ctx.thread);
    AssertRC(rc);
    rc = RTThreadWait(g_ctx.thread, RT_INDEFINITE_WAIT, NULL);
    AssertRC(rc);

    /*
     * Destroy the pasteboard and uninitialize the global context record.
     */
    destroyPasteboard(&g_ctx.pasteboard);
    g_ctx.thread = NIL_RTTHREAD;
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

int ShClSvcImplFormatAnnounce(PSHCLCLIENT pClient,
                              PSHCLCLIENTCMDCTX pCmdCtx, PSHCLFORMATDATA pFormats)
{
    RT_NOREF(pCmdCtx);

    LogFlowFunc(("uFormats=%02X\n", pFormats->Formats));

    if (pFormats->Formats == VBOX_SHCL_FMT_NONE)
    {
        /* This is just an automatism, not a genuine announcement */
        return VINF_SUCCESS;
    }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    if (pFormats->Formats & VBOX_SHCL_FMT_URI_LIST) /* No transfer support yet. */
        return VINF_SUCCESS;
#endif

    return ShClSvcDataReadRequest(pClient, pFormats->Formats, NULL /* pidEvent */);
}

int ShClSvcImplReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                        PSHCLDATABLOCK pData, uint32_t *pcbActual)
{
    RT_NOREF(pCmdCtx);

    ShClSvcLock();

    /* Default to no data available. */
    *pcbActual = 0;

    int rc = readFromPasteboard(pClient->State.pCtx->pasteboard,
                                pData->uFormat, pData->pvData, pData->cbData, pcbActual);

    ShClSvcUnlock();

    return rc;
}

int ShClSvcImplWriteData(PSHCLCLIENT pClient,
                         PSHCLCLIENTCMDCTX pCmdCtx, PSHCLDATABLOCK pData)
{
    RT_NOREF(pCmdCtx);

    ShClSvcLock();

    writeToPasteboard(pClient->State.pCtx->pasteboard, pData->pvData, pData->cbData, pData->uFormat);

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

