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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
# include "clipboard-fuse.h"
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
typedef struct _SHCLCTXFUSE
{
    RTTHREAD Thread;
} SHCLCTXFUSE;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_FUSE */

/**
 * Global clipboard context information.
 */
struct SHCLCONTEXT
{
    /** Client command context */
    VBGLR3SHCLCMDCTX CmdCtx;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /** Associated transfer data. */
    SHCLTRANSFERCTX  TransferCtx;
# ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
    SHCLCTXFUSE      FUSE;
# endif
#endif
    /** X11 clipboard context. */
    SHCLX11CTX       X11;
};

/** Only one client is supported. There seems to be no need for more clients. */
static SHCLCONTEXT g_Ctx;


/**
 * Get clipboard data from the host.
 *
 * @returns VBox result code
 * @param   pCtx                Our context information.
 * @param   Format              The format of the data being requested.
 * @param   ppv                 On success and if pcb > 0, this will point to a buffer
 *                              to be freed with RTMemFree containing the data read.
 * @param   pcb                 On success, this contains the number of bytes of data
 *                              returned.
 */
DECLCALLBACK(int) ShClX11RequestDataForX11Callback(PSHCLCONTEXT pCtx, SHCLFORMAT Format, void **ppv, uint32_t *pcb)
{
    RT_NOREF(pCtx);

    LogFlowFunc(("Format=0x%x\n", Format));

    int rc = VINF_SUCCESS;

    uint32_t cbRead = 0;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    if (Format == VBOX_SHCL_FMT_URI_LIST)
    {
        //rc = VbglR3ClipboardRootListRead()
    }
    else
#endif
    {
        uint32_t cbData = _4K; /** @todo Make this dynamic. */
        void    *pvData = RTMemAlloc(cbData);
        if (pvData)
        {
            rc = VbglR3ClipboardReadDataEx(&pCtx->CmdCtx, Format, pvData, cbData, &cbRead);
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
                rc = VbglR3ClipboardReadDataEx(&pCtx->CmdCtx, Format, pvData, cbData, &cbRead);
                if (rc == VINF_BUFFER_OVERFLOW)
                    rc = VERR_BUFFER_OVERFLOW;
            }
            else
                rc = VERR_NO_MEMORY;
        }

        if (RT_SUCCESS(rc))
        {
            *pcb = cbRead; /* Actual bytes read. */
            *ppv = pvData;
        }

        /*
         * Catch other errors. This also catches the case in which the buffer was
         * too small a second time, possibly because the clipboard contents
         * changed half-way through the operation.  Since we can't say whether or
         * not this is actually an error, we just return size 0.
         */
        if (RT_FAILURE(rc))
            RTMemFree(pvData);
    }

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

/**
 * Tell the host that new clipboard formats are available.
 *
 * @param   pCtx            Our context information.
 * @param   fFormats        The formats to report.
 */
DECLCALLBACK(void) ShClX11ReportFormatsCallback(PSHCLCONTEXT pCtx, SHCLFORMATS fFormats)
{
    RT_NOREF(pCtx);

    LogFlowFunc(("Formats=0x%x\n", fFormats));

    int rc2 = VbglR3ClipboardReportFormats(pCtx->CmdCtx.idClient, fFormats);
    RT_NOREF(rc2);
    LogFlowFuncLeaveRC(rc2);
}

/**
 * This is called by the backend to tell us that a request for data from
 * X11 has completed.
 *
 * @param  pCtx                 Our context information.
 * @param  rc                   The IPRT result code of the request.
 * @param  pReq                 The request structure that we passed in when we started
 *                              the request.  We RTMemFree() this in this function.
 * @param  pv                   The clipboard data returned from X11 if the request succeeded (see @a rc).
 * @param  cb                   The size of the data in @a pv.
 */
DECLCALLBACK(void) ShClX11RequestFromX11CompleteCallback(PSHCLCONTEXT pCtx, int rc, CLIPREADCBREQ *pReq, void *pv, uint32_t cb)
{
    RT_NOREF(pCtx, rc);

    LogFlowFunc(("rc=%Rrc, Format=0x%x, pv=%p, cb=%RU32\n", rc, pReq->Format, pv, cb));

    int rc2 = VbglR3ClipboardWriteDataEx(&pCtx->CmdCtx, pReq->Format, pv, cb);
    RT_NOREF(rc2);

    RTMemFree(pReq);

    LogFlowFuncLeaveRC(rc2);
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
    LogRel(("Worker loop running\n"));

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
                    LogRel2(("Host requested termination\n"));
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

    LogRel(("Worker loop ended\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
static DECLCALLBACK(int) vboxClipoardFUSEThread(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf, pvUser);

    VbglR3Init();

    LogFlowFuncEnter();

    RTThreadUserSignal(hThreadSelf);

    SHCL_FUSE_OPTS Opts;
    RT_ZERO(Opts);

    Opts.fForeground     = true;
    Opts.fSingleThreaded = false; /** @todo Do we want multithread here? */

    int rc = RTPathTemp(Opts.szMountPoint, sizeof(Opts.szMountPoint));
    if (RT_SUCCESS(rc))
    {
        rc = RTPathAppend(Opts.szMountPoint, sizeof(Opts.szMountPoint), "VBoxSharedClipboard");
        if (RT_SUCCESS(rc))
        {
            rc = RTDirCreate(Opts.szMountPoint, 0700,
                             RTDIRCREATE_FLAGS_NO_SYMLINKS);
            if (rc == VERR_ALREADY_EXISTS)
                rc = VINF_SUCCESS;
        }
    }

    if (RT_SUCCESS(rc))
    {
        rc = ShClFuseMain(&Opts);
    }
    else
        LogRel(("Error creating FUSE mount directory, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardFUSEStart()
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pCtx = &g_Ctx;

    int rc = RTThreadCreate(&pCtx->FUSE.Thread, vboxClipoardFUSEThread, &pCtx->FUSE, 0,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLFUSE");
    if (RT_SUCCESS(rc))
        rc = RTThreadUserWait(pCtx->FUSE.Thread, 30 * 1000);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardFUSEStop()
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pCtx = &g_Ctx;

    int rcThread;
    int rc = RTThreadWait(pCtx->FUSE.Thread, 1000, &rcThread);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_FUSE */

static const char *getName()
{
    return "Shared Clipboard";
}

static const char *getPidFilePath()
{
    return ".vboxclient-clipboard.pid";
}

static int init(struct VBCLSERVICE **pSelf)
{
    RT_NOREF(pSelf);

    int rc;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    rc = ShClTransferCtxInit(&g_Ctx.TransferCtx);
#else
    rc = VINF_SUCCESS;
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    RT_NOREF(ppInterface, fDaemonised);

    /* Initialise the guest library. */
    int rc = vboxClipboardConnect();
    if (RT_SUCCESS(rc))
    {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
        rc = vboxClipboardFUSEStart();
        if (RT_SUCCESS(rc))
        {
#endif
            rc = vboxClipboardMain();

#ifdef VBOX_WITH_SHARED_CLIPBOARD_FUSE
            int rc2 = vboxClipboardFUSEStop();
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
#endif
    }

    if (RT_FAILURE(rc))
        VBClLogError("Service terminated abnormally with %Rrc\n", rc);

    if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        rc = VINF_SUCCESS; /* Prevent automatic restart by daemon script if host service not available. */

    return rc;
}

static void cleanup(struct VBCLSERVICE **ppInterface)
{
    RT_NOREF(ppInterface);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    ShClTransferCtxDestroy(&g_Ctx.TransferCtx);
#endif
}

struct VBCLSERVICE vbclClipboardInterface =
{
    getName,
    getPidFilePath,
    init,
    run,
    cleanup
};

struct CLIPBOARDSERVICE
{
    struct VBCLSERVICE *pInterface;
};

struct VBCLSERVICE **VBClGetClipboardService(void)
{
    struct CLIPBOARDSERVICE *pService =
        (struct CLIPBOARDSERVICE *)RTMemAlloc(sizeof(*pService));

    if (!pService)
        VBClLogFatalError("Out of memory\n");
    pService->pInterface = &vbclClipboardInterface;
    return &pService->pInterface;
}
