/* $Id$ */
/** @file
 * Shared Clipboard Service - Win32 host.
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
#include <iprt/win/windows.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/GuestHost/SharedClipboard-win.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include <VBox/GuestHost/SharedClipboard-transfers.h>
#endif

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ldr.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include <iprt/utf16.h>
#endif

#include <process.h>
#include <shlobj.h> /* Needed for shell objects. */

#include "VBoxSharedClipboardSvc-internal.h"
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include "VBoxSharedClipboardSvc-transfers.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int vboxClipboardSvcWinSyncInternal(PSHCLCONTEXT pCtx);

struct SHCLCONTEXT
{
    /** Handle for window message handling thread. */
    RTTHREAD    hThread;
    /** Structure for keeping and communicating with service client. */
    PSHCLCLIENT pClient;
    /** Windows-specific context data. */
    SHCLWINCTX  Win;
};


/** @todo Someone please explain the protocol wrt overflows...  */
static void vboxClipboardSvcWinGetData(uint32_t u32Format, const void *pvSrc, uint32_t cbSrc,
                                       void *pvDst, uint32_t cbDst, uint32_t *pcbActualDst)
{
    LogFlowFunc(("cbSrc = %d, cbDst = %d\n", cbSrc, cbDst));

    if (   u32Format == VBOX_SHCL_FMT_HTML
        && SharedClipboardWinIsCFHTML((const char *)pvSrc))
    {
        /** @todo r=bird: Why the double conversion? */
        char *pszBuf = NULL;
        uint32_t cbBuf = 0;
        int rc = SharedClipboardWinConvertCFHTMLToMIME((const char *)pvSrc, cbSrc, &pszBuf, &cbBuf);
        if (RT_SUCCESS(rc))
        {
            *pcbActualDst = cbBuf;
            if (cbBuf > cbDst)
            {
                /* Do not copy data. The dst buffer is not enough. */
                RTMemFree(pszBuf);
                return;
            }
            memcpy(pvDst, pszBuf, cbBuf);
            RTMemFree(pszBuf);
        }
        else
            *pcbActualDst = 0;
    }
    else
    {
        *pcbActualDst = cbSrc;

        if (cbSrc > cbDst)
        {
            /* Do not copy data. The dst buffer is not enough. */
            return;
        }

        memcpy(pvDst, pvSrc, cbSrc);
    }

#ifdef LOG_ENABLED
    ShClDbgDumpData(pvDst, cbSrc, u32Format);
#endif

    return;
}

static int vboxClipboardSvcWinDataSet(PSHCLCONTEXT pCtx, UINT cfFormat, void *pvData, uint32_t cbData)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn   (cbData, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    HANDLE hMem = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, cbData);

    LogFlowFunc(("hMem=%p\n", hMem));

    if (hMem)
    {
        void *pMem = GlobalLock(hMem);

        LogFlowFunc(("pMem=%p, GlobalSize=%zu\n", pMem, GlobalSize(hMem)));

        if (pMem)
        {
            LogFlowFunc(("Setting data\n"));

            memcpy(pMem, pvData, cbData);

            /* The memory must be unlocked before inserting to the Clipboard. */
            GlobalUnlock(hMem);

            /* 'hMem' contains the host clipboard data.
             * size is 'cb' and format is 'format'.
             */
            HANDLE hClip = SetClipboardData(cfFormat, hMem);

            LogFlowFunc(("hClip=%p\n", hClip));

            if (hClip)
            {
                /* The hMem ownership has gone to the system. Nothing to do. */
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
        }
        else
            rc = VERR_ACCESS_DENIED;

        GlobalFree(hMem);
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardSvcWinDataRead(PSHCLCONTEXT pCtx, UINT uFormat, void **ppvData, uint32_t *pcbData)
{
    SHCLFORMAT fFormat = SharedClipboardWinClipboardFormatToVBox(uFormat);
    LogFlowFunc(("uFormat=%u -> uFmt=0x%x\n", uFormat, fFormat));

    if (fFormat == VBOX_SHCL_FMT_NONE)
    {
        LogRel2(("Shared Clipbaord: Windows format %u not supported, ingoring\n", uFormat));
        return VERR_NOT_SUPPORTED;
    }

    SHCLEVENTID idEvent = 0;
    int rc = ShClSvcDataReadRequest(pCtx->pClient, fFormat, &idEvent);
    if (RT_SUCCESS(rc))
    {
        PSHCLEVENTPAYLOAD pPayload;
        rc = ShClEventWait(&pCtx->pClient->EventSrc, idEvent, 30 * 1000, &pPayload);
        if (RT_SUCCESS(rc))
        {
            *ppvData = pPayload ? pPayload->pvData : NULL;
            *pcbData = pPayload ? pPayload->cbData : 0;

            /* Detach the payload, as the caller then will own the data. */
            ShClEventPayloadDetach(&pCtx->pClient->EventSrc, idEvent);
            /**
             * @todo r=bird: The payload has already been detached,
             * ShClEventPayloadDetach and ShClEventWait does the exact same
             * thing, except for the extra waiting in the latter.
             */
        }

        ShClEventUnregister(&pCtx->pClient->EventSrc, idEvent);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static LRESULT CALLBACK vboxClipboardSvcWinWndProcMain(PSHCLCONTEXT pCtx,
                                                       HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AssertPtr(pCtx);

    LRESULT lresultRc = 0;

    const PSHCLWINCTX pWinCtx = &pCtx->Win;

    switch (uMsg)
    {
        case WM_CLIPBOARDUPDATE:
        {
            LogFunc(("WM_CLIPBOARDUPDATE\n"));

            int rc = RTCritSectEnter(&pWinCtx->CritSect);
            if (RT_SUCCESS(rc))
            {
                const HWND hWndClipboardOwner = GetClipboardOwner();

                LogFunc(("WM_CLIPBOARDUPDATE: hWndClipboardOwnerUs=%p, hWndNewClipboardOwner=%p\n",
                         pWinCtx->hWndClipboardOwnerUs, hWndClipboardOwner));

                if (pWinCtx->hWndClipboardOwnerUs != hWndClipboardOwner)
                {
                    int rc2 = RTCritSectLeave(&pWinCtx->CritSect);
                    AssertRC(rc2);

                    /* Clipboard was updated by another application, retrieve formats and report back. */
                    rc = vboxClipboardSvcWinSyncInternal(pCtx);
                    if (RT_SUCCESS(rc))
                        rc = shClSvcSetSource(pCtx->pClient, SHCLSOURCE_LOCAL);
                }
                else
                {
                    int rc2 = RTCritSectLeave(&pWinCtx->CritSect);
                    AssertRC(rc2);
                }
            }

            if (RT_FAILURE(rc))
                LogRel(("Shared Clipboard: WM_CLIPBOARDUPDATE failed with %Rrc\n", rc));

            break;
        }

        case WM_CHANGECBCHAIN:
        {
            LogFunc(("WM_CHANGECBCHAIN\n"));
            lresultRc = SharedClipboardWinHandleWMChangeCBChain(pWinCtx, hWnd, uMsg, wParam, lParam);
            break;
        }

        case WM_DRAWCLIPBOARD:
        {
            LogFunc(("WM_DRAWCLIPBOARD\n"));

            int rc = RTCritSectEnter(&pWinCtx->CritSect);
            if (RT_SUCCESS(rc))
            {
                const HWND hWndClipboardOwner = GetClipboardOwner();

                LogFunc(("WM_DRAWCLIPBOARD: hWndClipboardOwnerUs=%p, hWndNewClipboardOwner=%p\n",
                         pWinCtx->hWndClipboardOwnerUs, hWndClipboardOwner));

                if (pWinCtx->hWndClipboardOwnerUs != hWndClipboardOwner)
                {
                    int rc2 = RTCritSectLeave(&pWinCtx->CritSect);
                    AssertRC(rc2);

                    /* Clipboard was updated by another application, retrieve formats and report back. */
                    rc = vboxClipboardSvcWinSyncInternal(pCtx);
                    if (RT_SUCCESS(rc))
                        shClSvcSetSource(pCtx->pClient, SHCLSOURCE_LOCAL);
                }
                else
                {
                    int rc2 = RTCritSectLeave(&pWinCtx->CritSect);
                    AssertRC(rc2);
                }
            }

            lresultRc = SharedClipboardWinChainPassToNext(pWinCtx, uMsg, wParam, lParam);
            break;
        }

        case WM_TIMER:
        {
            int rc = SharedClipboardWinHandleWMTimer(pWinCtx);
            AssertRC(rc);

            break;
        }

        case WM_RENDERFORMAT:
        {
            LogFunc(("WM_RENDERFORMAT\n"));

            /* Insert the requested clipboard format data into the clipboard. */
            const UINT       uFormat = (UINT)wParam;
            const SHCLFORMAT fFormat = SharedClipboardWinClipboardFormatToVBox(uFormat);
            LogFunc(("WM_RENDERFORMAT: uFormat=%u -> fFormat=0x%x\n", uFormat, fFormat));

            if (   fFormat       == VBOX_SHCL_FMT_NONE
                || pCtx->pClient == NULL)
            {
                /* Unsupported clipboard format is requested. */
                LogFunc(("WM_RENDERFORMAT unsupported format requested or client is not active\n"));
                SharedClipboardWinClear();
            }
            else
            {
                void    *pvData = NULL;
                uint32_t cbData = 0;
                int rc = vboxClipboardSvcWinDataRead(pCtx, uFormat, &pvData, &cbData);
                if (   RT_SUCCESS(rc)
                    && pvData
                    && cbData)
                {
                    rc = vboxClipboardSvcWinDataSet(pCtx, uFormat, pvData, cbData);

                    RTMemFree(pvData);
                    cbData = 0;
                }

                if (RT_FAILURE(rc))
                    SharedClipboardWinClear();
            }

            break;
        }

        case WM_RENDERALLFORMATS:
        {
            LogFunc(("WM_RENDERALLFORMATS\n"));

            int rc = SharedClipboardWinHandleWMRenderAllFormats(pWinCtx, hWnd);
            AssertRC(rc);

            break;
        }

        case SHCL_WIN_WM_REPORT_FORMATS:
        {
            /* Announce available formats. Do not insert data -- will be inserted in WM_RENDERFORMAT (or via IDataObject). */
            SHCLFORMATS fFormats = (uint32_t)lParam;
            LogFunc(("SHCL_WIN_WM_REPORT_FORMATS: fFormats=0x%x\n", fFormats));

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            if (fFormats & VBOX_SHCL_FMT_URI_LIST)
            {
                PSHCLTRANSFER pTransfer;
                int rc = shClSvcTransferStart(pCtx->pClient,
                                              SHCLTRANSFERDIR_FROM_REMOTE, SHCLSOURCE_REMOTE,
                                              &pTransfer);
                if (RT_SUCCESS(rc))
                {
                    /* Create the IDataObject implementation the host OS needs and assign
                     * the newly created transfer to this object. */
                    rc = SharedClipboardWinTransferCreate(&pCtx->Win, pTransfer);

                    /*  Note: The actual requesting + retrieving of data will be done in the IDataObject implementation
                              (ClipboardDataObjectImpl::GetData()). */
                }
                else
                    LogRel(("Shared Clipboard: Initializing read transfer failed with %Rrc\n", rc));
            }
            else
            {
#endif
                int rc = SharedClipboardWinOpen(hWnd);
                if (RT_SUCCESS(rc))
                {
                    SharedClipboardWinClear();

                    rc = SharedClipboardWinAnnounceFormats(pWinCtx, fFormats);

                    SharedClipboardWinClose();
                }
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            }
#endif
            LogFunc(("SHCL_WIN_WM_REPORT_FORMATS: lastErr=%ld\n", GetLastError()));
            break;
        }

        case WM_DESTROY:
        {
            LogFunc(("WM_DESTROY\n"));

            int rc = SharedClipboardWinHandleWMDestroy(pWinCtx);
            AssertRC(rc);

            PostQuitMessage(0);
            break;
        }

        default:
            break;
    }

    LogFlowFunc(("LEAVE hWnd=%p, WM_ %u\n", hWnd, uMsg));
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/**
 * Static helper function for having a per-client proxy window instances.
 */
static LRESULT CALLBACK vboxClipboardSvcWinWndProcInstance(HWND hWnd, UINT uMsg,
                                                           WPARAM wParam, LPARAM lParam)
{
    LONG_PTR pUserData = GetWindowLongPtr(hWnd, GWLP_USERDATA);
    AssertPtrReturn(pUserData, 0);

    PSHCLCONTEXT pCtx = reinterpret_cast<PSHCLCONTEXT>(pUserData);
    if (pCtx)
        return vboxClipboardSvcWinWndProcMain(pCtx, hWnd, uMsg, wParam, lParam);

    return 0;
}

/**
 * Static helper function for routing Windows messages to a specific
 * proxy window instance.
 */
static LRESULT CALLBACK vboxClipboardSvcWinWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    /* Note: WM_NCCREATE is not the first ever message which arrives, but
     *       early enough for us. */
    if (uMsg == WM_NCCREATE)
    {
        LogFlowFunc(("WM_NCCREATE\n"));

        LPCREATESTRUCT pCS = (LPCREATESTRUCT)lParam;
        AssertPtr(pCS);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)vboxClipboardSvcWinWndProcInstance);

        return vboxClipboardSvcWinWndProcInstance(hWnd, uMsg, wParam, lParam);
    }

    /* No window associated yet. */
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

DECLCALLBACK(int) vboxClipboardSvcWinThread(RTTHREAD hThreadSelf, void *pvUser)
{
    LogFlowFuncEnter();

    bool fThreadSignalled = false;

    const PSHCLCONTEXT pCtx    = (PSHCLCONTEXT)pvUser;
    AssertPtr(pCtx);
    const PSHCLWINCTX  pWinCtx = &pCtx->Win;

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);

    /* Register the Window Class. */
    WNDCLASS wc;
    RT_ZERO(wc);

    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = vboxClipboardSvcWinWndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);

    /* Register an unique wnd class name. */
    char szWndClassName[32];
    RTStrPrintf2(szWndClassName, sizeof(szWndClassName),
                 "%s-%RU64", SHCL_WIN_WNDCLASS_NAME, RTThreadGetNative(hThreadSelf));
    wc.lpszClassName = szWndClassName;

    int rc;

    ATOM atomWindowClass = RegisterClass(&wc);
    if (atomWindowClass == 0)
    {
        LogFunc(("Failed to register window class\n"));
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        /* Create a window and make it a clipboard viewer. */
        pWinCtx->hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                       szWndClassName, szWndClassName,
                                       WS_POPUPWINDOW,
                                       -200, -200, 100, 100, NULL, NULL, hInstance, pCtx /* lpParam */);
        if (pWinCtx->hWnd == NULL)
        {
            LogFunc(("Failed to create window\n"));
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            SetWindowPos(pWinCtx->hWnd, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

            rc = SharedClipboardWinChainAdd(&pCtx->Win);
            if (RT_SUCCESS(rc))
            {
                if (!SharedClipboardWinIsNewAPI(&pWinCtx->newAPI))
                    pWinCtx->oldAPI.timerRefresh = SetTimer(pWinCtx->hWnd, 0, 10 * 1000, NULL);
            }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            if (RT_SUCCESS(rc))
            {
                HRESULT hr = OleInitialize(NULL);
                if (FAILED(hr))
                {
                    LogRel(("Shared Clipboard: Initializing window thread OLE failed (%Rhrc) -- file transfers unavailable\n", hr));
                    /* Not critical, the rest of the clipboard might work. */
                }
                else
                    LogRel(("Shared Clipboard: Initialized window thread OLE\n"));
            }
#endif
            int rc2 = RTThreadUserSignal(hThreadSelf);
            AssertRC(rc2);

            fThreadSignalled = true;

            MSG msg;
            BOOL msgret = 0;
            while ((msgret = GetMessage(&msg, NULL, 0, 0)) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            /*
             * Window procedure can return error, * but this is exceptional situation that should be
             * identified in testing.
             */
            Assert(msgret >= 0);
            LogFunc(("Message loop finished. GetMessage returned %d, message id: %d \n", msgret, msg.message));

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
            OleUninitialize();
#endif
        }
    }

    pWinCtx->hWnd = NULL;

    if (atomWindowClass != 0)
    {
        UnregisterClass(szWndClassName, hInstance);
        atomWindowClass = 0;
    }

    if (!fThreadSignalled)
    {
        int rc2 = RTThreadUserSignal(hThreadSelf);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Synchronizes the host and the guest clipboard formats by sending all supported host clipboard
 * formats to the guest.
 *
 * @returns VBox status code, VINF_NO_CHANGE if no synchronization was required.
 * @param   pCtx                Clipboard context to synchronize.
 */
static int vboxClipboardSvcWinSyncInternal(PSHCLCONTEXT pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc;

    if (pCtx->pClient)
    {
        SHCLFORMATDATA Formats;
        RT_ZERO(Formats);

        rc = SharedClipboardWinGetFormats(&pCtx->Win, &Formats);
        if (   RT_SUCCESS(rc)
            && Formats.Formats != VBOX_SHCL_FMT_NONE) /** @todo r=bird: BUGBUG: revisit this. */
            rc = ShClSvcHostReportFormats(pCtx->pClient, Formats.Formats);
    }
    else /* If we don't have any client data (yet), bail out. */
        rc = VINF_NO_CHANGE;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
 * Public platform dependent functions.
 */

int ShClSvcImplInit(void)
{
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
        LogRel(("Shared Clipboard: Initializing OLE failed (%Rhrc) -- file transfers unavailable\n", hr));
        /* Not critical, the rest of the clipboard might work. */
    }
    else
        LogRel(("Shared Clipboard: Initialized OLE\n"));
#endif

    return VINF_SUCCESS;
}

void ShClSvcImplDestroy(void)
{
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
    OleUninitialize();
#endif
}

int ShClSvcImplConnect(PSHCLCLIENT pClient, bool fHeadless)
{
    RT_NOREF(fHeadless);

    LogFlowFuncEnter();

    int rc;

    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)RTMemAllocZ(sizeof(SHCLCONTEXT));
    if (pCtx)
    {
        rc = SharedClipboardWinCtxInit(&pCtx->Win);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadCreate(&pCtx->hThread, vboxClipboardSvcWinThread, pCtx /* pvUser */, _64K /* Stack size */,
                                RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
            if (RT_SUCCESS(rc))
            {
                int rc2 = RTThreadUserWait(pCtx->hThread, 30 * 1000 /* Timeout in ms */);
                AssertRC(rc2);
            }
        }

        pClient->State.pCtx = pCtx;
        pClient->State.pCtx->pClient = pClient;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplSync(PSHCLCLIENT pClient)
{
    /* Sync the host clipboard content with the client. */
    return vboxClipboardSvcWinSyncInternal(pClient->State.pCtx);
}

int ShClSvcImplDisconnect(PSHCLCLIENT pClient)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    PSHCLCONTEXT pCtx = pClient->State.pCtx;
    if (pCtx)
    {
        if (pCtx->Win.hWnd)
            PostMessage(pCtx->Win.hWnd, WM_DESTROY, 0 /* wParam */, 0 /* lParam */);

        if (pCtx->hThread != NIL_RTTHREAD)
        {
            LogFunc(("Waiting for thread to terminate ...\n"));

            /* Wait for the window thread to terminate. */
            rc = RTThreadWait(pCtx->hThread, 30 * 1000 /* Timeout in ms */, NULL);
            if (RT_FAILURE(rc))
                LogRel(("Shared Clipboard: Waiting for window thread termination failed with rc=%Rrc\n", rc));

            pCtx->hThread = NIL_RTTHREAD;
        }

        SharedClipboardWinCtxDestroy(&pCtx->Win);

        if (RT_SUCCESS(rc))
        {
            RTMemFree(pCtx);
            pCtx = NULL;

            pClient->State.pCtx = NULL;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplFormatAnnounce(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                              PSHCLFORMATDATA pFormats)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    RT_NOREF(pCmdCtx);

    int rc;

    PSHCLCONTEXT pCtx = pClient->State.pCtx;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("uFormats=0x%x, hWnd=%p\n", pFormats->Formats, pCtx->Win.hWnd));

    /*
     * The guest announced formats. Forward to the window thread.
     */
    PostMessage(pCtx->Win.hWnd, SHCL_WIN_WM_REPORT_FORMATS,
                0 /* wParam */, pFormats->Formats /* lParam */);

    rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplReadData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                        PSHCLDATABLOCK pData, uint32_t *pcbActual)
{
    AssertPtrReturn(pClient,             VERR_INVALID_POINTER);
    RT_NOREF(pCmdCtx);
    AssertPtrReturn(pClient->State.pCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("uFormat=%02X\n", pData->uFormat));

    HANDLE hClip = NULL;

    const PSHCLWINCTX pWinCtx = &pClient->State.pCtx->Win;

    /*
     * The guest wants to read data in the given format.
     */
    int rc = SharedClipboardWinOpen(pWinCtx->hWnd);
    if (RT_SUCCESS(rc))
    {
        LogFunc(("Clipboard opened\n"));

        if (pData->uFormat & VBOX_SHCL_FMT_BITMAP)
        {
            hClip = GetClipboardData(CF_DIB);
            if (hClip != NULL)
            {
                LPVOID lp = GlobalLock(hClip);

                if (lp != NULL)
                {
                    LogFunc(("CF_DIB\n"));

                    vboxClipboardSvcWinGetData(VBOX_SHCL_FMT_BITMAP, lp, GlobalSize(hClip),
                                               pData->pvData, pData->cbData, pcbActual);

                    GlobalUnlock(hClip);
                }
                else
                {
                    hClip = NULL;
                }
            }
        }
        else if (pData->uFormat & VBOX_SHCL_FMT_UNICODETEXT)
        {
            hClip = GetClipboardData(CF_UNICODETEXT);
            if (hClip != NULL)
            {
                LPWSTR uniString = (LPWSTR)GlobalLock(hClip);

                if (uniString != NULL)
                {
                    LogFunc(("CF_UNICODETEXT\n"));

                    vboxClipboardSvcWinGetData(VBOX_SHCL_FMT_UNICODETEXT, uniString, (lstrlenW(uniString) + 1) * 2,
                                               pData->pvData, pData->cbData, pcbActual);

                    GlobalUnlock(hClip);
                }
                else
                {
                    hClip = NULL;
                }
            }
        }
        else if (pData->uFormat & VBOX_SHCL_FMT_HTML)
        {
            UINT format = RegisterClipboardFormat(SHCL_WIN_REGFMT_HTML);
            if (format != 0)
            {
                hClip = GetClipboardData(format);
                if (hClip != NULL)
                {
                    LPVOID lp = GlobalLock(hClip);
                    if (lp != NULL)
                    {
                        /** @todo r=andy Add data overflow handling. */
                        vboxClipboardSvcWinGetData(VBOX_SHCL_FMT_HTML, lp, GlobalSize(hClip),
                                                   pData->pvData, pData->cbData, pcbActual);
#ifdef VBOX_STRICT
                        LogFlowFunc(("Raw HTML clipboard data from host:"));
                        ShClDbgDumpHtml((char *)pData->pvData, pData->cbData);
#endif
                        GlobalUnlock(hClip);
                    }
                    else
                    {
                        hClip = NULL;
                    }
                }
            }
        }
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
        else if (pData->uFormat & VBOX_SHCL_FMT_URI_LIST)
        {
            AssertFailed(); /** @todo */
        }
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */
        SharedClipboardWinClose();
    }

    if (hClip == NULL)
    {
        /* Reply with empty data. */
        vboxClipboardSvcWinGetData(0, NULL, 0, pData->pvData, pData->cbData, pcbActual);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClSvcImplWriteData(PSHCLCLIENT pClient, PSHCLCLIENTCMDCTX pCmdCtx,
                         PSHCLDATABLOCK pData)
{
    LogFlowFuncEnter();

    int rc = ShClSvcDataReadSignal(pClient, pCmdCtx, pData);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
int ShClSvcImplTransferCreate(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    RT_NOREF(pClient, pTransfer);

    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

int ShClSvcImplTransferDestroy(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    LogFlowFuncEnter();

    SharedClipboardWinTransferDestroy(&pClient->State.pCtx->Win, pTransfer);

    return VINF_SUCCESS;
}

int ShClSvcImplTransferGetRoots(PSHCLCLIENT pClient, PSHCLTRANSFER pTransfer)
{
    LogFlowFuncEnter();

    const PSHCLWINCTX pWinCtx = &pClient->State.pCtx->Win;

    int rc = SharedClipboardWinGetRoots(pWinCtx, pTransfer);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

