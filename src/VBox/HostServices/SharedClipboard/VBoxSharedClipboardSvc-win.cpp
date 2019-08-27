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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ldr.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <iprt/utf16.h>
#endif

#include <process.h>
#include <shlobj.h> /* Needed for shell objects. */

#include "VBoxSharedClipboardSvc-internal.h"
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include "VBoxSharedClipboardSvc-uri.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int vboxClipboardSvcWinSyncInternal(PVBOXCLIPBOARDCONTEXT pCtx);

typedef struct _VBOXCLIPBOARDCONTEXTOLD
{
    /** Event which gets triggered if the host clipboard needs to render its data. */
    RTSEMEVENT               hRenderEvent;
} VBOXCLIPBOARDCONTEXTOLD;

struct _VBOXCLIPBOARDCONTEXT
{
    /** Handle for window message handling thread. */
    RTTHREAD                 hThread;
    /** Structure for keeping and communicating with service client. */
    PVBOXCLIPBOARDCLIENT     pClient;
    /** Windows-specific context data. */
    VBOXCLIPBOARDWINCTX      Win;
    /** Old cruft, needed for the legacy protocol (v0). */
    VBOXCLIPBOARDCONTEXTOLD  Old;
};


/** @todo Someone please explain the protocol wrt overflows...  */
static void vboxClipboardGetData(uint32_t u32Format, const void *pvSrc, uint32_t cbSrc,
                                 void *pvDst, uint32_t cbDst, uint32_t *pcbActualDst)
{
    LogFlowFunc(("cbSrc = %d, cbDst = %d\n", cbSrc, cbDst));

    if (   u32Format == VBOX_SHARED_CLIPBOARD_FMT_HTML
        && VBoxClipboardWinIsCFHTML((const char *)pvSrc))
    {
        /** @todo r=bird: Why the double conversion? */
        char *pszBuf = NULL;
        uint32_t cbBuf = 0;
        int rc = VBoxClipboardWinConvertCFHTMLToMIME((const char *)pvSrc, cbSrc, &pszBuf, &cbBuf);
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
    VBoxClipboardDbgDumpData(pvDst, cbSrc, u32Format);
#endif

    return;
}

static int vboxClipboardSvcWinDataSet(PVBOXCLIPBOARDCONTEXT pCtx, UINT cfFormat, void *pvData, uint32_t cbData)
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

/**
 * Requests data of a specific format from the guest, optionally waiting for its arrival via VBoxClipboardSvcImplWriteData()
 * and copying the retrieved data into the OS' clipboard.
 *
 * Legacy protocol, do not use anymore.
 *
 * @returns VBox status code.
 * @param   pCtx                Clipboard context to use.
 * @param   cfFormat            Windows clipboard format to receive data in.
 * @param   uTimeoutMs          Timeout (in ms) to wait until the render event has been triggered.
 *                              Specify 0 if no waiting is required.
 */
static int vboxClipboardSvcWinOldRequestData(PVBOXCLIPBOARDCONTEXT pCtx, UINT cfFormat,
                                             RTMSINTERVAL uTimeoutMs)
{
    Assert(pCtx->Old.hRenderEvent);
    Assert(pCtx->pClient->State.Old.data.pv == NULL && pCtx->pClient->State.Old.data.cb == 0 && pCtx->pClient->State.Old.data.u32Format == 0);

    LogFlowFunc(("cfFormat=%u, uTimeoutMs=%RU32\n", cfFormat, uTimeoutMs));

    const VBOXCLIPBOARDFORMAT fFormat = VBoxClipboardWinClipboardFormatToVBox(cfFormat);

    int rc = vboxSvcClipboardOldReportMsg(pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, fFormat);
    if (   RT_SUCCESS(rc)
        && uTimeoutMs)
    {
        rc = RTSemEventWait(pCtx->Old.hRenderEvent, uTimeoutMs /* Timeout in ms */);
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("rc=%Rrc, pv=%p, cb=%RU32, u32Format=%RU32\n",
                         rc, pCtx->pClient->State.Old.data.pv, pCtx->pClient->State.Old.data.cb,
                         pCtx->pClient->State.Old.data.u32Format));

            if (   RT_SUCCESS (rc)
                && pCtx->pClient->State.Old.data.pv != NULL
                && pCtx->pClient->State.Old.data.cb > 0
                && pCtx->pClient->State.Old.data.u32Format == fFormat)
            {
                rc = vboxClipboardSvcWinDataSet(pCtx, cfFormat,
                                                pCtx->pClient->State.Old.data.pv, pCtx->pClient->State.Old.data.cb);

                if (pCtx->pClient->State.Old.data.pv)
                {
                    RTMemFree(pCtx->pClient->State.Old.data.pv);
                    pCtx->pClient->State.Old.data.pv = NULL;
                }
            }

            if (RT_FAILURE(rc))
            {
                if (pCtx->pClient->State.Old.data.pv)
                {
                    RTMemFree(pCtx->pClient->State.Old.data.pv);
                    pCtx->pClient->State.Old.data.pv = NULL;
                }

                pCtx->pClient->State.Old.data.cb        = 0;
                pCtx->pClient->State.Old.data.u32Format = 0;

                /* Something went wrong. */
                VBoxClipboardWinClear();
            }
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardSvcWinDataRead(PVBOXCLIPBOARDCONTEXT pCtx, UINT cfFormat,
                                       void **ppvData, uint32_t *pcbData)
{
    LogFlowFunc(("cfFormat=%u\n", cfFormat));

    int rc;

    PVBOXCLIPBOARDCLIENTMSG pMsgReadData = vboxSvcClipboardMsgAlloc(VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA,
                                                                    VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA);
    if (pMsgReadData)
    {
        const uint16_t uEvent = SharedClipboardEventIDGenerate(&pCtx->pClient->Events);

        const VBOXCLIPBOARDFORMAT fFormat = VBoxClipboardWinClipboardFormatToVBox(cfFormat);
        const uint32_t            uCID    = VBOX_SHARED_CLIPBOARD_CONTEXTID_MAKE(pCtx->pClient->Events.uID, uEvent);

        HGCMSvcSetU32(&pMsgReadData->m_paParms[0], uCID);
        HGCMSvcSetU32(&pMsgReadData->m_paParms[1], fFormat);
        HGCMSvcSetU32(&pMsgReadData->m_paParms[2], pCtx->pClient->State.cbChunkSize);

        LogFlowFunc(("CID=%RU32\n", uCID));

        rc = vboxSvcClipboardMsgAdd(pCtx->pClient, pMsgReadData, true /* fAppend */);
        if (RT_SUCCESS(rc))
        {
            int rc2 = SharedClipboardEventRegister(&pCtx->pClient->Events, uEvent);
            AssertRC(rc2);

            rc = vboxSvcClipboardClientWakeup(pCtx->pClient);
            if (RT_SUCCESS(rc))
            {
                PSHAREDCLIPBOARDEVENTPAYLOAD pPayload;
                rc = SharedClipboardEventWait(&pCtx->pClient->Events, uEvent,
                                              30 * 1000, &pPayload);
                if (RT_SUCCESS(rc))
                {
                    *ppvData = pPayload->pvData;
                    *pcbData = pPayload->cbData;

                    /* Detach the payload, as the caller then will own the data. */
                    SharedClipboardEventPayloadDetach(&pCtx->pClient->Events, uEvent);
                }
            }

            SharedClipboardEventUnregister(&pCtx->pClient->Events, uEvent);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static LRESULT CALLBACK vboxClipboardSvcWinWndProcMain(PVBOXCLIPBOARDCONTEXT pCtx,
                                                       HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AssertPtr(pCtx);

    LRESULT lresultRc = 0;

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

    switch (uMsg)
    {
        case WM_CLIPBOARDUPDATE:
        {
            const HWND hWndClipboardOwner = GetClipboardOwner();
            if (pWinCtx->hWndClipboardOwnerUs != hWndClipboardOwner)
            {
                LogFunc(("WM_CLIPBOARDUPDATE: hWndClipboardOwnerUs=%p, hWndNewClipboardOwner=%p\n",
                         pWinCtx->hWndClipboardOwnerUs, hWndClipboardOwner));

                /* Clipboard was updated by another application, retrieve formats and report back. */
                int rc = vboxClipboardSvcWinSyncInternal(pCtx);
                if (RT_SUCCESS(rc))
                    vboxSvcClipboardSetSource(pCtx->pClient, SHAREDCLIPBOARDSOURCE_LOCAL);
            }

            break;
        }

        case WM_CHANGECBCHAIN:
        {
            LogFunc(("WM_CHANGECBCHAIN\n"));
            lresultRc = VBoxClipboardWinHandleWMChangeCBChain(pWinCtx, hWnd, uMsg, wParam, lParam);
            break;
        }

        case WM_DRAWCLIPBOARD:
        {
            LogFunc(("WM_DRAWCLIPBOARD\n"));

            if (GetClipboardOwner() != hWnd)
            {
                /* Clipboard was updated by another application, retrieve formats and report back. */
                int rc = vboxClipboardSvcWinSyncInternal(pCtx);
                if (RT_SUCCESS(rc))
                    vboxSvcClipboardSetSource(pCtx->pClient, SHAREDCLIPBOARDSOURCE_LOCAL);
            }

            lresultRc = VBoxClipboardWinChainPassToNext(pWinCtx, uMsg, wParam, lParam);
            break;
        }

        case WM_TIMER:
        {
            int rc = VBoxClipboardWinHandleWMTimer(pWinCtx);
            AssertRC(rc);

            break;
        }

        case WM_RENDERFORMAT:
        {
            LogFunc(("WM_RENDERFORMAT\n"));

            /* Insert the requested clipboard format data into the clipboard. */
            const UINT cfFormat = (UINT)wParam;

            const VBOXCLIPBOARDFORMAT fFormat = VBoxClipboardWinClipboardFormatToVBox(cfFormat);

            LogFunc(("WM_RENDERFORMAT: cfFormat=%u -> fFormat=0x%x\n", cfFormat, fFormat));

            if (   fFormat       == VBOX_SHARED_CLIPBOARD_FMT_NONE
                || pCtx->pClient == NULL)
            {
                /* Unsupported clipboard format is requested. */
                LogFunc(("WM_RENDERFORMAT unsupported format requested or client is not active\n"));
                VBoxClipboardWinClear();
            }
            else
            {
                int rc;

                if (pCtx->pClient->State.uProtocolVer == 0)
                {
                    rc = vboxClipboardSvcWinOldRequestData(pCtx, cfFormat, 30 * 1000 /* 30s timeout */);
                }
                else
                {
                    void    *pvData = NULL;
                    uint32_t cbData = 0;
                    rc = vboxClipboardSvcWinDataRead(pCtx, cfFormat, &pvData, &cbData);
                    if (   RT_SUCCESS(rc)
                        && pvData
                        && cbData)
                    {
                        rc = vboxClipboardSvcWinDataSet(pCtx, cfFormat, pvData, cbData);

                        RTMemFree(pvData);
                    }
                    else
                        AssertFailed();
                }
            }

            break;
        }

        case WM_RENDERALLFORMATS:
        {
            LogFunc(("WM_RENDERALLFORMATS\n"));

            int rc = VBoxClipboardWinHandleWMRenderAllFormats(pWinCtx, hWnd);
            AssertRC(rc);

            break;
        }

        case VBOX_CLIPBOARD_WM_REPORT_FORMATS:
        {
            LogFunc(("VBOX_CLIPBOARD_WM_REPORT_FORMATS\n"));

            /* Some legcay protocol checks. */
            if (   pCtx->pClient->State.uProtocolVer == 0
                && (   pCtx->pClient == NULL
                    || pCtx->pClient->State.Old.fHostMsgFormats))
            {
                /* Host has pending formats message. Ignore the guest announcement,
                 * because host clipboard has more priority.
                 */
                LogFunc(("VBOX_CLIPBOARD_WM_REPORT_FORMATS ignored; pClient=%p\n", pCtx->pClient));
                break;
            }

            /* Announce available formats. Do not insert data -- will be inserted in WM_RENDERFORMAT. */
            VBOXCLIPBOARDFORMATS fFormats = (uint32_t)lParam;
            if (fFormats != VBOX_SHARED_CLIPBOARD_FMT_NONE) /* Could arrive with some older GA versions. */
            {
                int rc = VBoxClipboardWinOpen(hWnd);
                if (RT_SUCCESS(rc))
                {
                    VBoxClipboardWinClear();
#if 0
                    if (fFormats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
                    {
                        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_URI_LIST\n"));

                        PSHAREDCLIPBOARDURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(&pCtx->pClient->URI,
                                                                                                 0 /* uIdx */);
                        if (pTransfer)
                        {
                            rc = VBoxClipboardWinURITransferCreate(pWinCtx, pTransfer);

                            /* Note: The actual requesting + retrieving of data will be done in the IDataObject implementation
                                     (ClipboardDataObjectImpl::GetData()). */
                        }
                        else
                            AssertFailedStmt(rc = VERR_NOT_FOUND);

                        /* Note: VBoxClipboardWinURITransferCreate() takes care of closing the clipboard. */
                    }
                    else
                    {
#endif
                        rc = VBoxClipboardWinAnnounceFormats(pWinCtx, fFormats);

                        VBoxClipboardWinClose();
#if 0
                    }
#endif
                }

                LogFunc(("VBOX_CLIPBOARD_WM_REPORT_FORMATS: fFormats=0x%x, lastErr=%ld\n", fFormats, GetLastError()));
            }

            break;
        }

        case WM_DESTROY:
        {
            LogFunc(("WM_DESTROY\n"));

            int rc = VBoxClipboardWinHandleWMDestroy(pWinCtx);
            AssertRC(rc);

            PostQuitMessage(0);
            break;
        }

        default:
            break;
    }

    LogFlowFunc(("hWnd=%p, WM_ %u\n", hWnd, uMsg));
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

    PVBOXCLIPBOARDCONTEXT pCtx = reinterpret_cast<PVBOXCLIPBOARDCONTEXT>(pUserData);
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

    const PVBOXCLIPBOARDCONTEXT pCtx    = (PVBOXCLIPBOARDCONTEXT)pvUser;
    AssertPtr(pCtx);
    const PVBOXCLIPBOARDWINCTX  pWinCtx = &pCtx->Win;

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
                 "%s-%RU64", VBOX_CLIPBOARD_WNDCLASS_NAME, RTThreadGetNative(hThreadSelf));
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

            rc = VBoxClipboardWinChainAdd(&pCtx->Win);
            if (RT_SUCCESS(rc))
            {
                if (!VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
                    pWinCtx->oldAPI.timerRefresh = SetTimer(pWinCtx->hWnd, 0, 10 * 1000, NULL);
            }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
            if (RT_SUCCESS(rc))
            {
                HRESULT hr = OleInitialize(NULL);
                if (FAILED(hr))
                {
                    LogRel(("Clipboard: Initializing OLE failed (%Rhrc) -- file transfers unavailable\n"));
                    /* Not critical, the rest of the clipboard might work. */
                }
                else
                    LogRel(("Clipboard: Initialized OLE\n"));
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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
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
static int vboxClipboardSvcWinSyncInternal(PVBOXCLIPBOARDCONTEXT pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc;

    if (pCtx->pClient)
    {
        SHAREDCLIPBOARDFORMATDATA Formats;
        RT_ZERO(Formats);

        rc = VBoxClipboardWinGetFormats(&pCtx->Win, &Formats);
        if (   RT_SUCCESS(rc)
            && Formats.uFormats != VBOX_SHARED_CLIPBOARD_FMT_NONE)
        {
            if (pCtx->pClient->State.uProtocolVer == 0)
            {
                rc = vboxSvcClipboardOldReportMsg(pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS_WRITE, Formats.uFormats);
            }
            else /* Protocol v1 and up. */
            {
                rc = vboxSvcClipboardSendFormatsWrite(pCtx->pClient, &Formats);
            }
        }
    }
    else /* If we don't have any client data (yet), bail out. */
        rc = VINF_NO_CHANGE;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
 * Public platform dependent functions.
 */

int VBoxClipboardSvcImplInit(void)
{
    /* Initialization is done in VBoxClipboardSvcImplConnect(). */
    return VINF_SUCCESS;
}

void VBoxClipboardSvcImplDestroy(void)
{
    /* Destruction is done in VBoxClipboardSvcImplDisconnect(). */
}

int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENT pClient, bool fHeadless)
{
    RT_NOREF(fHeadless);

    LogFlowFuncEnter();

    int rc;

    PVBOXCLIPBOARDCONTEXT pCtx = (PVBOXCLIPBOARDCONTEXT)RTMemAllocZ(sizeof(VBOXCLIPBOARDCONTEXT));
    if (pCtx)
    {
        /* Check that new Clipboard API is available. */
        rc = VBoxClipboardWinCheckAndInitNewAPI(&pCtx->Win.newAPI);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventCreate(&pCtx->Old.hRenderEvent);
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

            if (RT_FAILURE(rc))
                RTSemEventDestroy(pCtx->Old.hRenderEvent);
        }

        pClient->State.pCtx = pCtx;
        pClient->State.pCtx->pClient = pClient;

        /* Sync the host clipboard content with the client. */
        rc = VBoxClipboardSvcImplSync(pClient);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENT pClient)
{
    /* Sync the host clipboard content with the client. */
    return vboxClipboardSvcWinSyncInternal(pClient->State.pCtx);
}

int VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENT pClient)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    PVBOXCLIPBOARDCONTEXT pCtx = pClient->State.pCtx;
    if (pCtx)
    {
        if (pCtx->Win.hWnd)
            PostMessage(pCtx->Win.hWnd, WM_DESTROY, 0 /* wParam */, 0 /* lParam */);

        rc = RTSemEventDestroy(pCtx->Old.hRenderEvent);
        if (RT_SUCCESS(rc))
        {
            if (pCtx->hThread != NIL_RTTHREAD)
            {
                LogFunc(("Waiting for thread to terminate ...\n"));

                /* Wait for the window thread to terminate. */
                rc = RTThreadWait(pCtx->hThread, 30 * 1000 /* Timeout in ms */, NULL);
                if (RT_FAILURE(rc))
                    LogRel(("Shared Clipboard: Waiting for window thread termination failed with rc=%Rrc\n", rc));

                pCtx->hThread = NIL_RTTHREAD;
            }
        }

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

int VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx,
                                       PSHAREDCLIPBOARDFORMATDATA pFormats)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    RT_NOREF(pCmdCtx);

    PVBOXCLIPBOARDCONTEXT pCtx = pClient->State.pCtx;
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("uFormats=0x%x, hWnd=%p\n", pFormats->uFormats, pCtx->Win.hWnd));

    /*
     * The guest announced formats. Forward to the window thread.
     */
    PostMessage(pCtx->Win.hWnd, VBOX_CLIPBOARD_WM_REPORT_FORMATS,
                0 /* wParam */, pFormats->uFormats /* lParam */);

    return VINF_SUCCESS;
}

int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx,
                                 PSHAREDCLIPBOARDDATABLOCK pData, uint32_t *pcbActual)
{
    AssertPtrReturn(pClient,             VERR_INVALID_POINTER);
    RT_NOREF(pCmdCtx);
    AssertPtrReturn(pClient->State.pCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("uFormat=%02X\n", pData->uFormat));

    HANDLE hClip = NULL;

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pClient->State.pCtx->Win;

    /*
     * The guest wants to read data in the given format.
     */
    int rc = VBoxClipboardWinOpen(pWinCtx->hWnd);
    if (RT_SUCCESS(rc))
    {
        LogFunc(("Clipboard opened\n"));

        if (pData->uFormat & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
        {
            hClip = GetClipboardData(CF_DIB);
            if (hClip != NULL)
            {
                LPVOID lp = GlobalLock(hClip);

                if (lp != NULL)
                {
                    LogFunc(("CF_DIB\n"));

                    vboxClipboardGetData(VBOX_SHARED_CLIPBOARD_FMT_BITMAP, lp, GlobalSize(hClip),
                                         pData->pvData, pData->cbData, pcbActual);

                    GlobalUnlock(hClip);
                }
                else
                {
                    hClip = NULL;
                }
            }
        }
        else if (pData->uFormat & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
        {
            hClip = GetClipboardData(CF_UNICODETEXT);
            if (hClip != NULL)
            {
                LPWSTR uniString = (LPWSTR)GlobalLock(hClip);

                if (uniString != NULL)
                {
                    LogFunc(("CF_UNICODETEXT\n"));

                    vboxClipboardGetData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT, uniString, (lstrlenW(uniString) + 1) * 2,
                                         pData->pvData, pData->cbData, pcbActual);

                    GlobalUnlock(hClip);
                }
                else
                {
                    hClip = NULL;
                }
            }
        }
        else if (pData->uFormat & VBOX_SHARED_CLIPBOARD_FMT_HTML)
        {
            UINT format = RegisterClipboardFormat(VBOX_CLIPBOARD_WIN_REGFMT_HTML);
            if (format != 0)
            {
                hClip = GetClipboardData(format);
                if (hClip != NULL)
                {
                    LPVOID lp = GlobalLock(hClip);
                    if (lp != NULL)
                    {
                        /** @todo r=andy Add data overflow handling. */
                        vboxClipboardGetData(VBOX_SHARED_CLIPBOARD_FMT_HTML, lp, GlobalSize(hClip),
                                             pData->pvData, pData->cbData, pcbActual);
#ifdef VBOX_STRICT
                        LogFlowFunc(("Raw HTML clipboard data from host:"));
                        VBoxClipboardDbgDumpHtml((char *)pData->pvData, pData->cbData);
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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
        else if (pData->uFormat & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
        {
            AssertFailed(); /** @todo */
        }
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */
        VBoxClipboardWinClose();
    }

    if (hClip == NULL)
    {
        /* Reply with empty data. */
        vboxClipboardGetData(0, NULL, 0, pData->pvData, pData->cbData, pcbActual);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardSvcWinOldWriteData(PVBOXCLIPBOARDCLIENT pClient,
                                           void *pv, uint32_t cb, uint32_t u32Format)
{
    LogFlowFuncEnter();

    const PVBOXCLIPBOARDCLIENTSTATE pState = &pClient->State;

    /*
     * The guest returns data that was requested in the WM_RENDERFORMAT handler.
     */
    Assert(   pState->Old.data.pv == NULL
           && pState->Old.data.cb == 0
           && pState->Old.data.u32Format == 0);

#ifdef LOG_ENABLED
    VBoxClipboardDbgDumpData(pv, cb, u32Format);
#endif

    if (cb > 0)
    {
        char *pszResult = NULL;

        if (   u32Format == VBOX_SHARED_CLIPBOARD_FMT_HTML
            && !VBoxClipboardWinIsCFHTML((const char*)pv))
        {
            /* check that this is not already CF_HTML */
            uint32_t cbResult;
            int rc = VBoxClipboardWinConvertMIMEToCFHTML((const char *)pv, cb, &pszResult, &cbResult);
            if (RT_SUCCESS(rc))
            {
                if (pszResult != NULL && cbResult != 0)
                {
                    pState->Old.data.pv        = pszResult;
                    pState->Old.data.cb        = cbResult;
                    pState->Old.data.u32Format = u32Format;
                }
            }
        }
        else
        {
            pState->Old.data.pv = RTMemDup(pv, cb);
            if (pState->Old.data.pv)
            {
                pState->Old.data.cb = cb;
                pState->Old.data.u32Format = u32Format;
            }
        }
    }

    AssertPtr(pState->pCtx);
    int rc = RTSemEventSignal(pState->pCtx->Old.hRenderEvent);
    AssertRC(rc);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENT pClient, PVBOXCLIPBOARDCLIENTCMDCTX pCmdCtx,
                                  PSHAREDCLIPBOARDDATABLOCK pData)
{
    LogFlowFuncEnter();

    int rc;

    if (pClient->State.uProtocolVer == 0)
    {
        rc = vboxClipboardSvcWinOldWriteData(pClient, pData->pvData, pData->cbData, pData->uFormat);
    }
    else
    {
        const uint16_t uEvent = VBOX_SHARED_CLIPBOARD_CONTEXTID_GET_EVENT(pCmdCtx->uContextID);

        PSHAREDCLIPBOARDEVENTPAYLOAD pPayload;
        rc = SharedClipboardPayloadAlloc(uEvent, pData->pvData, pData->cbData, &pPayload);
        if (RT_SUCCESS(rc))
        {
            rc = SharedClipboardEventSignal(&pClient->Events, uEvent, pPayload);
             if (RT_FAILURE(rc))
                SharedClipboardPayloadFree(pPayload);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardSvcImplURITransferCreate(PVBOXCLIPBOARDCLIENT pClient, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    RT_NOREF(pClient, pTransfer);

    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

int VBoxClipboardSvcImplURITransferDestroy(PVBOXCLIPBOARDCLIENT pClient, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    VBoxClipboardWinURITransferDestroy(&pClient->State.pCtx->Win, pTransfer);

    return VINF_SUCCESS;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

