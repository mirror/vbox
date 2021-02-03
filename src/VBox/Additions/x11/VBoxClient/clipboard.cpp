/** $Id$ */
/** @file
 * Guest Additions - X11 Shared Clipboard.
 */

/*
 * Copyright (C) 2007-2020 Oracle Corporation
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
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
# include <iprt/dir.h>
#endif
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-x11.h>

#include "VBoxClient.h"

#include "clipboard.h"
#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
# include "clipboard-fuse.h"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/** Only one context is supported at a time for now. */
SHCLCONTEXT g_Ctx;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
SHCLFUSECTX g_FuseCtx;
#endif


DECLCALLBACK(int) ShClX11RequestDataForX11Callback(PSHCLCONTEXT pCtx, SHCLFORMAT uFmt, void **ppv, uint32_t *pcb)
{
    LogFlowFunc(("pCtx=%p, uFmt=%#x\n", pCtx, uFmt));

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    if (uFmt == VBOX_SHCL_FMT_URI_LIST)
    {
        //rc = VbglR3ClipboardRootListRead()
        rc = VERR_NO_DATA;
    }
    else
#endif
    {
        uint32_t cbRead = 0;

        uint32_t cbData = _4K; /** @todo Make this dynamic. */
        void    *pvData = RTMemAlloc(cbData);
        if (pvData)
        {
            rc = VbglR3ClipboardReadDataEx(&pCtx->CmdCtx, uFmt, pvData, cbData, &cbRead);
        }
        else
            rc = VERR_NO_MEMORY;

        /*
         * A return value of VINF_BUFFER_OVERFLOW tells us to try again with a
         * larger buffer.  The size of the buffer needed is placed in *pcb.
         * So we start all over again.
         */
        if (rc == VINF_BUFFER_OVERFLOW)
        {
            /* cbRead contains the size required. */

            cbData = cbRead;
            pvData = RTMemRealloc(pvData, cbRead);
            if (pvData)
            {
                rc = VbglR3ClipboardReadDataEx(&pCtx->CmdCtx, uFmt, pvData, cbData, &cbRead);
                if (rc == VINF_BUFFER_OVERFLOW)
                    rc = VERR_BUFFER_OVERFLOW;
            }
            else
                rc = VERR_NO_MEMORY;
        }

        if (!cbRead)
            rc = VERR_NO_DATA;

        if (RT_SUCCESS(rc))
        {
            *pcb = cbRead; /* Actual bytes read. */
            *ppv = pvData;
        }
        else
        {
            /*
             * Catch other errors. This also catches the case in which the buffer was
             * too small a second time, possibly because the clipboard contents
             * changed half-way through the operation.  Since we can't say whether or
             * not this is actually an error, we just return size 0.
             */
            RTMemFree(pvData);
        }
    }

    if (RT_FAILURE(rc))
        LogRel(("Requesting data in format %#x from host failed with %Rrc\n", uFmt, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Opaque data structure describing a request from the host for clipboard
 * data, passed in when the request is forwarded to the X11 backend so that
 * it can be completed correctly.
 */
struct CLIPREADCBREQ
{
    /** The data format that was requested. */
    SHCLFORMAT Format;
};

DECLCALLBACK(void) ShClX11ReportFormatsCallback(PSHCLCONTEXT pCtx, SHCLFORMATS fFormats)
{
    RT_NOREF(pCtx);

    LogFlowFunc(("fFormats=%#x\n", fFormats));

    int rc2 = VbglR3ClipboardReportFormats(pCtx->CmdCtx.idClient, fFormats);
    RT_NOREF(rc2);
    LogFlowFuncLeaveRC(rc2);
}

DECLCALLBACK(void) ShClX11RequestFromX11CompleteCallback(PSHCLCONTEXT pCtx, int rcCompletion,
                                                         CLIPREADCBREQ *pReq, void *pv, uint32_t cb)
{
    LogFlowFunc(("rcCompletion=%Rrc, Format=0x%x, pv=%p, cb=%RU32\n", rcCompletion, pReq->Format, pv, cb));

    if (RT_SUCCESS(rcCompletion)) /* Only write data if the request succeeded. */
    {
        AssertPtrReturnVoid(pv);
        AssertReturnVoid(pv);

        rcCompletion = VbglR3ClipboardWriteDataEx(&pCtx->CmdCtx, pReq->Format, pv, cb);
    }

    RTMemFree(pReq);

    LogFlowFuncLeaveRC(rcCompletion);
}

/**
 * Connect the guest clipboard to the host.
 *
 * @returns VBox status code.
 */
static int vboxClipboardConnect(void)
{
    LogFlowFuncEnter();

    int rc = ShClX11Init(&g_Ctx.X11, &g_Ctx, false /* fHeadless */);
    if (RT_SUCCESS(rc))
    {
        rc = ShClX11ThreadStart(&g_Ctx.X11, false /* grab */);
        if (RT_SUCCESS(rc))
        {
            rc = VbglR3ClipboardConnectEx(&g_Ctx.CmdCtx, VBOX_SHCL_GF_0_CONTEXT_ID);
            if (RT_FAILURE(rc))
                ShClX11ThreadStop(&g_Ctx.X11);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
    {
        VBClLogError("Error connecting to host service, rc=%Rrc\n", rc);

        VbglR3ClipboardDisconnectEx(&g_Ctx.CmdCtx);
        ShClX11Destroy(&g_Ctx.X11);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * The main loop of our clipboard reader.
 */
int vboxClipboardMain(void)
{
    int rc;

    PSHCLCONTEXT pCtx = &g_Ctx;

    bool fShutdown = false;

    /* The thread waits for incoming messages from the host. */
    for (;;)
    {
        PVBGLR3CLIPBOARDEVENT pEvent = (PVBGLR3CLIPBOARDEVENT)RTMemAllocZ(sizeof(VBGLR3CLIPBOARDEVENT));
        AssertPtrBreakStmt(pEvent, rc = VERR_NO_MEMORY);

        LogFlowFunc(("Waiting for host message (fUseLegacyProtocol=%RTbool, fHostFeatures=%#RX64) ...\n",
                     pCtx->CmdCtx.fUseLegacyProtocol, pCtx->CmdCtx.fHostFeatures));

        uint32_t idMsg  = 0;
        uint32_t cParms = 0;
        rc = VbglR3ClipboardMsgPeekWait(&pCtx->CmdCtx, &idMsg, &cParms, NULL /* pidRestoreCheck */);
        if (RT_SUCCESS(rc))
        {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            rc = VbglR3ClipboardEventGetNextEx(idMsg, cParms, &pCtx->CmdCtx, &pCtx->TransferCtx, pEvent);
#else
            rc = VbglR3ClipboardEventGetNext(idMsg, cParms, &pCtx->CmdCtx, pEvent);
#endif
        }

        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Getting next event failed with %Rrc\n", rc));

            VbglR3ClipboardEventFree(pEvent);
            pEvent = NULL;

            if (fShutdown)
                break;

            /* Wait a bit before retrying. */
            RTThreadSleep(1000);
            continue;
        }
        else
        {
            AssertPtr(pEvent);
            LogFlowFunc(("Event uType=%RU32\n", pEvent->enmType));

            switch (pEvent->enmType)
            {
                case VBGLR3CLIPBOARDEVENTTYPE_REPORT_FORMATS:
                {
                    ShClX11ReportFormatsToX11(&g_Ctx.X11, pEvent->u.fReportedFormats);
                    break;
                }

                case VBGLR3CLIPBOARDEVENTTYPE_READ_DATA:
                {
                    /* The host needs data in the specified format. */
                    CLIPREADCBREQ *pReq;
                    pReq = (CLIPREADCBREQ *)RTMemAllocZ(sizeof(CLIPREADCBREQ));
                    if (pReq)
                    {
                        pReq->Format = pEvent->u.fReadData;
                        ShClX11ReadDataFromX11(&g_Ctx.X11, pReq->Format, pReq);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                    break;
                }

                case VBGLR3CLIPBOARDEVENTTYPE_QUIT:
                {
                    VBClLogVerbose(2, "Host requested termination\n");
                    fShutdown = true;
                    break;
                }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                case VBGLR3CLIPBOARDEVENTTYPE_TRANSFER_STATUS:
                {
                    /* Nothing to do here. */
                    rc = VINF_SUCCESS;
                    break;
                }
#endif
                case VBGLR3CLIPBOARDEVENTTYPE_NONE:
                {
                    /* Nothing to do here. */
                    rc = VINF_SUCCESS;
                    break;
                }

                default:
                {
                    AssertMsgFailedBreakStmt(("Event type %RU32 not implemented\n", pEvent->enmType), rc = VERR_NOT_SUPPORTED);
                }
            }

            if (pEvent)
            {
                VbglR3ClipboardEventFree(pEvent);
                pEvent = NULL;
            }
        }

        if (fShutdown)
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vbclShClInit(void)
{
    int rc;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    rc = ShClTransferCtxInit(&g_Ctx.TransferCtx);
#else
    rc = VINF_SUCCESS;
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnWorker}
 */
static DECLCALLBACK(int) vbclShClWorker(bool volatile *pfShutdown)
{
    RT_NOREF(pfShutdown);

    /* Initialise the guest library. */
    int rc = vboxClipboardConnect();
    if (RT_SUCCESS(rc))
    {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
        rc = VbClShClFUSEInit(&g_FuseCtx, &g_Ctx);
        if (RT_SUCCESS(rc))
        {
            rc = VbClShClFUSEStart(&g_FuseCtx);
            if (RT_SUCCESS(rc))
            {
#endif
                /* Let the main thread know that it can continue spawning services. */
                RTThreadUserSignal(RTThreadSelf());

                rc = vboxClipboardMain();

#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
                int rc2 = VbClShClFUSEStop(&g_FuseCtx);
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }
#endif
    }

    if (RT_FAILURE(rc))
        VBClLogError("Service terminated abnormally with %Rrc\n", rc);

    if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        rc = VINF_SUCCESS; /* Prevent automatic restart by daemon script if host service not available. */

    return rc;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vbclShClStop(void)
{
    /* Disconnect from the host service.
     * This will also send a VBOX_SHCL_HOST_MSG_QUIT from the host so that we can break out from our message worker. */
    VbglR3ClipboardDisconnect(g_Ctx.CmdCtx.idClient);
    g_Ctx.CmdCtx.idClient = 0;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnTerm}
 */
static DECLCALLBACK(int) vbclShClTerm(void)
{
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    ShClTransferCtxDestroy(&g_Ctx.TransferCtx);
#endif

    return VINF_SUCCESS;
}

VBCLSERVICE g_SvcClipboard =
{
    "shcl",                      /* szName */
    "Shared Clipboard",          /* pszDescription */
    ".vboxclient-clipboard.pid", /* pszPidFilePath */
    NULL,                        /* pszUsage */
    NULL,                        /* pszOptions */
    NULL,                        /* pfnOption */
    vbclShClInit,                /* pfnInit */
    vbclShClWorker,              /* pfnWorker */
    vbclShClStop,                /* pfnStop*/
    vbclShClTerm                 /* pfnTerm */
};

