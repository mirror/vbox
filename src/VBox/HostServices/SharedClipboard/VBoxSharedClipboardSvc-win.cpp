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

/** Static window class name. */
static char s_szClipWndClassName[] = VBOX_CLIPBOARD_WNDCLASS_NAME;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int vboxClipboardWinSyncInternal(PVBOXCLIPBOARDCONTEXT pCtx);

struct _VBOXCLIPBOARDCONTEXT
{
    /** Handle for window message handling thread. */
    RTTHREAD                 hThread;
    /** Event which gets triggered if the host clipboard needs to render its data. */
    RTSEMEVENT               hRenderEvent;
    /** Structure for keeping and communicating with client data (from the guest). */
    PVBOXCLIPBOARDCLIENTDATA pClientData;
    /** Windows-specific context data. */
    VBOXCLIPBOARDWINCTX      Win;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    /** Structure for keeping URI clipboard information around. */
    SHAREDCLIPBOARDURICTX    URI;
#endif
};

/* Only one client is supported. There seems to be no need for more clients. */
static VBOXCLIPBOARDCONTEXT g_ctx;


#ifdef LOG_ENABLED
static void vboxClipboardDump(const void *pv, size_t cb, uint32_t u32Format)
{
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT:\n"));
        if (pv && cb)
            LogFunc(("%ls\n", pv));
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_BITMAP\n"));
    else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_HTML)
    {
        LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_HTML:\n"));
        if (pv && cb)
        {
            LogFunc(("%s\n", pv));

            //size_t cb = RTStrNLen(pv, );
            char *pszBuf = (char *)RTMemAllocZ(cb + 1);
            RTStrCopy(pszBuf, cb + 1, (const char *)pv);
            for (size_t off = 0; off < cb; ++off)
            {
                if (pszBuf[off] == '\n' || pszBuf[off] == '\r')
                    pszBuf[off] = ' ';
            }

            LogFunc(("%s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else
        LogFunc(("Invalid format %02X\n", u32Format));
}
#else  /* !LOG_ENABLED */
#   define vboxClipboardDump(__pv, __cb, __format) do { NOREF(__pv); NOREF(__cb); NOREF(__format); } while (0)
#endif /* !LOG_ENABLED */

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

    vboxClipboardDump(pvDst, cbSrc, u32Format);

    return;
}

/**
 * Reads data of a specific format by waiting for its arrival.
 *
 * @returns VBox status code.
 * @param   pCtx                Clipboard context to use.
 * @param   fFormat             Format to receive data in.
 * @param   uTimeoutMs          Timeout in ms.
 */
static int vboxClipboardWinReadData(PVBOXCLIPBOARDCONTEXT pCtx, VBOXCLIPBOARDFORMAT fFormat,
                                    RTMSINTERVAL uTimeoutMs)
{
    Assert(pCtx->pClientData);
    Assert(pCtx->hRenderEvent);
    Assert(pCtx->pClientData->State.data.pv == NULL && pCtx->pClientData->State.data.cb == 0 && pCtx->pClientData->State.data.u32Format == 0);

    LogFlowFunc(("fFormat=%02X\n", fFormat));

    int rc = vboxSvcClipboardReportMsg(pCtx->pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, fFormat);
    if (RT_SUCCESS(rc))
        rc = RTSemEventWait(pCtx->hRenderEvent, uTimeoutMs /* Timeout in ms */);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static LRESULT CALLBACK vboxClipboardWinWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lresultRc = 0;

    const PVBOXCLIPBOARDCONTEXT pCtx    = &g_ctx;
    const PVBOXCLIPBOARDWINCTX  pWinCtx = &pCtx->Win;

    switch (msg)
    {
        case WM_CLIPBOARDUPDATE:
        {
            LogFunc(("WM_CLIPBOARDUPDATE\n"));

            if (GetClipboardOwner() != hwnd)
            {
                /* Clipboard was updated by another application, retrieve formats and report back. */
                int rc = vboxClipboardWinSyncInternal(pCtx);
                AssertRC(rc);
            }
        } break;

        case WM_CHANGECBCHAIN:
        {
            LogFunc(("WM_CHANGECBCHAIN\n"));

            if (VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
            {
                lresultRc = DefWindowProc(hwnd, msg, wParam, lParam);
                break;
            }

            HWND hwndRemoved = (HWND)wParam;
            HWND hwndNext    = (HWND)lParam;

            if (hwndRemoved == pWinCtx->hWndNextInChain)
            {
                /* The window that was next to our in the chain is being removed.
                 * Relink to the new next window.
                 */
                pWinCtx->hWndNextInChain = hwndNext;
            }
            else
            {
                if (pWinCtx->hWndNextInChain)
                {
                    /* Pass the message further. */
                    DWORD_PTR dwResult;
                    lresultRc = SendMessageTimeout(pWinCtx->hWndNextInChain, WM_CHANGECBCHAIN, wParam, lParam, 0,
                                                    VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS,
                                                    &dwResult);
                    if (!lresultRc)
                        lresultRc = (LRESULT)dwResult;
                }
            }
        } break;

        case WM_DRAWCLIPBOARD:
        {
            LogFunc(("WM_DRAWCLIPBOARD\n"));

            if (GetClipboardOwner() != hwnd)
            {
                /* Clipboard was updated by another application, retrieve formats and report back. */
                int vboxrc = vboxClipboardWinSyncInternal(pCtx);
                AssertRC(vboxrc);
            }

            if (pWinCtx->hWndNextInChain)
            {
                LogFunc(("WM_DRAWCLIPBOARD next %p\n", pWinCtx->hWndNextInChain));

                /* Pass the message to next windows in the clipboard chain. */
                DWORD_PTR dwResult;
                lresultRc = SendMessageTimeout(pWinCtx->hWndNextInChain, msg, wParam, lParam, 0, VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS,
                                               &dwResult);
                if (!lresultRc)
                    lresultRc = dwResult;
            }
        } break;

        case WM_TIMER:
        {
            if (VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
                break;

            HWND hViewer = GetClipboardViewer();

            /* Re-register ourselves in the clipboard chain if our last ping
             * timed out or there seems to be no valid chain. */
            if (!hViewer || pWinCtx->oldAPI.fCBChainPingInProcess)
            {
                VBoxClipboardWinRemoveFromCBChain(&pCtx->Win);
                VBoxClipboardWinAddToCBChain(&pCtx->Win);
            }

            /* Start a new ping by passing a dummy WM_CHANGECBCHAIN to be
             * processed by ourselves to the chain. */
            pWinCtx->oldAPI.fCBChainPingInProcess = TRUE;

            hViewer = GetClipboardViewer();
            if (hViewer)
                SendMessageCallback(hViewer, WM_CHANGECBCHAIN,
                                    (WPARAM)pWinCtx->hWndNextInChain, (LPARAM)pWinCtx->hWndNextInChain,
                                    VBoxClipboardWinChainPingProc, (ULONG_PTR)pWinCtx);
        } break;

        case WM_RENDERFORMAT:
        {
            LogFunc(("WM_RENDERFORMAT\n"));

            /* Insert the requested clipboard format data into the clipboard. */
            const UINT cfFormat = (UINT)wParam;

            const VBOXCLIPBOARDFORMAT fFormat = VBoxClipboardWinClipboardFormatToVBox(cfFormat);

            LogFunc(("WM_RENDERFORMAT: cfFormat=%u -> fFormat=0x%x\n", cfFormat, fFormat));

            if (   fFormat       == VBOX_SHARED_CLIPBOARD_FMT_NONE
                || pCtx->pClientData == NULL)
            {
                /* Unsupported clipboard format is requested. */
                LogFunc(("WM_RENDERFORMAT unsupported format requested or client is not active\n"));
                VBoxClipboardWinClear();
            }
            else
            {
                int rc = vboxClipboardWinReadData(pCtx, fFormat,
                                                  30 * 1000 /* 30s timeout */);

                LogFunc(("vboxClipboardReadDataFromClient rc = %Rrc, pv %p, cb %d, u32Format %d\n",
                          rc, pCtx->pClientData->State.data.pv, pCtx->pClientData->State.data.cb, pCtx->pClientData->State.data.u32Format));

                if (   RT_SUCCESS (rc)
                    && pCtx->pClientData->State.data.pv != NULL
                    && pCtx->pClientData->State.data.cb > 0
                    && pCtx->pClientData->State.data.u32Format == fFormat)
                {
                    HANDLE hMem = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, pCtx->pClientData->State.data.cb);

                    LogFunc(("hMem %p\n", hMem));

                    if (hMem)
                    {
                        void *pMem = GlobalLock(hMem);

                        LogFunc(("pMem %p, GlobalSize %d\n", pMem, GlobalSize(hMem)));

                        if (pMem)
                        {
                            LogFunc(("WM_RENDERFORMAT setting data\n"));

                            if (pCtx->pClientData->State.data.pv)
                            {
                                memcpy(pMem, pCtx->pClientData->State.data.pv, pCtx->pClientData->State.data.cb);

                                RTMemFree(pCtx->pClientData->State.data.pv);
                                pCtx->pClientData->State.data.pv        = NULL;
                            }

                            pCtx->pClientData->State.data.cb        = 0;
                            pCtx->pClientData->State.data.u32Format = 0;

                            /* The memory must be unlocked before inserting to the Clipboard. */
                            GlobalUnlock(hMem);

                            /* 'hMem' contains the host clipboard data.
                             * size is 'cb' and format is 'format'.
                             */
                            HANDLE hClip = SetClipboardData(cfFormat, hMem);

                            LogFunc(("vboxClipboardHostEvent hClip %p\n", hClip));

                            if (hClip)
                            {
                                /* The hMem ownership has gone to the system. Nothing to do. */
                                break;
                            }
                        }

                        GlobalFree(hMem);
                    }
                }

                RTMemFree(pCtx->pClientData->State.data.pv);
                pCtx->pClientData->State.data.pv        = NULL;
                pCtx->pClientData->State.data.cb        = 0;
                pCtx->pClientData->State.data.u32Format = 0;

                /* Something went wrong. */
                VBoxClipboardWinClear();
            }
        } break;

        case WM_RENDERALLFORMATS:
        {
            LogFunc(("WM_RENDERALLFORMATS\n"));

            /* Do nothing. The clipboard formats will be unavailable now, because the
             * windows is to be destroyed and therefore the guest side becomes inactive.
             */
            int rc = VBoxClipboardWinOpen(hwnd);
            if (RT_SUCCESS(rc))
            {
                VBoxClipboardWinClear();
                VBoxClipboardWinClose();
            }
        } break;

        case VBOX_CLIPBOARD_WM_SET_FORMATS:
        {
            LogFunc(("VBOX_CLIPBOARD_WM_SET_FORMATS\n"));

            if (   pCtx->pClientData == NULL
                || pCtx->pClientData->State.fHostMsgFormats)
            {
                /* Host has pending formats message. Ignore the guest announcement,
                 * because host clipboard has more priority.
                 */
                LogFunc(("VBOX_CLIPBOARD_WM_SET_FORMATS ignored\n"));
                break;
            }

            /* Announce available formats. Do not insert data, they will be inserted in WM_RENDER*. */
            VBOXCLIPBOARDFORMATS fFormats = (uint32_t)lParam;

            int rc = VBoxClipboardWinOpen(hwnd);
            if (RT_SUCCESS(rc))
            {
                VBoxClipboardWinClear();

                rc = VBoxClipboardWinHandleWMSetFormats(pWinCtx, fFormats);
                if (rc == VERR_NOT_SUPPORTED)
                {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
                    SHAREDCLIPBOARDPROVIDERCREATIONCTX providerCtx;
                    RT_ZERO(providerCtx);
                    providerCtx.enmSource = SHAREDCLIPBOARDPROVIDERSOURCE_HOSTSERVICE;

                    rc = VBoxClipboardWinURIHandleWMSetFormats(pWinCtx, &pCtx->URI, &providerCtx, fFormats);
                    if (RT_SUCCESS(rc))
                        rc = vboxSvcClipboardReportMsg(pCtx->pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_HDR,
                                                       VBOX_SHARED_CLIPBOARD_FMT_URI_LIST);
#endif
                    if (rc == VERR_NOT_SUPPORTED)
                        LogRel(("Clipboard: Unsupported format(s) (0x%x), skipping\n", fFormats));
                }

                if (RT_FAILURE(rc))
                    LogFunc(("Failed with %Rrc\n", rc));

                VBoxClipboardWinClose();

                LogFunc(("VBOX_CLIPBOARD_WM_SET_FORMATS: fFormats=0x%x, lastErr=%ld\n", fFormats, GetLastError()));
            }
        } break;

        case WM_DESTROY:
        {
            /* MS recommends to remove from Clipboard chain in this callback. */
            VBoxClipboardWinRemoveFromCBChain(&pCtx->Win);
            if (pWinCtx->oldAPI.timerRefresh)
            {
                Assert(pWinCtx->hWnd);
                KillTimer(pWinCtx->hWnd, 0);
            }
            PostQuitMessage(0);
        } break;

        default:
        {
            LogFunc(("WM_ %p\n", msg));
            lresultRc = DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

    LogFunc(("WM_ rc %d\n", lresultRc));
    return lresultRc;
}

DECLCALLBACK(int) vboxClipboardWinThread(RTTHREAD hThreadSelf, void *pvUser)
{
    RT_NOREF(hThreadSelf, pvUser);

    /* Create a window and make it a clipboard viewer. */
    int rc = VINF_SUCCESS;

    LogFlowFuncEnter();

    const PVBOXCLIPBOARDCONTEXT pCtx    = &g_ctx;
    const PVBOXCLIPBOARDWINCTX  pWinCtx = &pCtx->Win;

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);

    /* Register the Window Class. */
    WNDCLASS wc;
    RT_ZERO(wc);

    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = vboxClipboardWinWndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszClassName = s_szClipWndClassName;

    ATOM atomWindowClass = RegisterClass(&wc);

    if (atomWindowClass == 0)
    {
        LogFunc(("Failed to register window class\n"));
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        /* Create the window. */
        pWinCtx->hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                       s_szClipWndClassName, s_szClipWndClassName,
                                       WS_POPUPWINDOW,
                                       -200, -200, 100, 100, NULL, NULL, hInstance, NULL);
        if (pWinCtx->hWnd == NULL)
        {
            LogFunc(("Failed to create window\n"));
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            SetWindowPos(pWinCtx->hWnd, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

            rc = VBoxClipboardWinAddToCBChain(&pCtx->Win);
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

                int rc2 = SharedClipboardURICtxInit(&g_ctx.URI);
                AssertRC(rc2);
            }
#endif
            MSG msg;
            BOOL msgret = 0;
            while ((msgret = GetMessage(&msg, NULL, 0, 0)) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            /*
            * Window procedure can return error,
            * but this is exceptional situation
            * that should be identified in testing
            */
            Assert(msgret >= 0);
            LogFunc(("Message loop finished. GetMessage returned %d, message id: %d \n", msgret, msg.message));

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
            OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
            OleUninitialize();

            SharedClipboardURICtxDestroy(&g_ctx.URI);
#endif
        }
    }

    pWinCtx->hWnd = NULL;

    if (atomWindowClass != 0)
    {
        UnregisterClass(s_szClipWndClassName, hInstance);
        atomWindowClass = 0;
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
static int vboxClipboardWinSyncInternal(PVBOXCLIPBOARDCONTEXT pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc;

    if (pCtx->pClientData)
    {
        uint32_t uFormats;
        rc = VBoxClipboardWinGetFormats(&pCtx->Win, &uFormats);
        if (RT_SUCCESS(rc))
            vboxSvcClipboardReportMsg(pCtx->pClientData, VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS, uFormats);
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
    RT_ZERO(g_ctx); /* Be careful not messing up non-POD types! */

    /* Check that new Clipboard API is available. */
    VBoxClipboardWinCheckAndInitNewAPI(&g_ctx.Win.newAPI);

    int rc = RTSemEventCreate(&g_ctx.hRenderEvent);
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadCreate(&g_ctx.hThread, vboxClipboardWinThread, NULL, _64K /* Stack size */,
                            RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");
    }

    if (RT_FAILURE(rc))
        RTSemEventDestroy(g_ctx.hRenderEvent);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void VBoxClipboardSvcImplDestroy(void)
{
    LogFlowFuncEnter();

    if (g_ctx.Win.hWnd)
    {
        PostMessage(g_ctx.Win.hWnd, WM_CLOSE, 0, 0);
    }

    int rc = RTSemEventDestroy(g_ctx.hRenderEvent);
    AssertRC(rc);

    /* Wait for the window thread to terminate. */
    rc = RTThreadWait(g_ctx.hThread, 30 * 1000 /* Timeout in ms */, NULL);
    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Waiting for window thread termination failed with rc=%Rrc\n", rc));

    g_ctx.hThread = NIL_RTTHREAD;
}

int VBoxClipboardSvcImplConnect(PVBOXCLIPBOARDCLIENTDATA pClientData, bool fHeadless)
{
    RT_NOREF(fHeadless);

    LogFlowFuncEnter();

    if (g_ctx.pClientData != NULL)
    {
        /* One client only. */
        return VERR_NOT_SUPPORTED;
    }

    pClientData->State.pCtx = &g_ctx;

    pClientData->State.pCtx->pClientData = pClientData;

    /* Sync the host clipboard content with the client. */
    VBoxClipboardSvcImplSync(pClientData);

    return VINF_SUCCESS;
}

int VBoxClipboardSvcImplSync(PVBOXCLIPBOARDCLIENTDATA pClientData)
{
    /* Sync the host clipboard content with the client. */
    return vboxClipboardWinSyncInternal(pClientData->State.pCtx);
}

int VBoxClipboardSvcImplDisconnect(PVBOXCLIPBOARDCLIENTDATA pClientData)
{
    RT_NOREF(pClientData);

    LogFlowFuncEnter();

    g_ctx.pClientData = NULL;

    return VINF_SUCCESS;
}

int VBoxClipboardSvcImplFormatAnnounce(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Formats)
{
    AssertPtrReturn(pClientData, VERR_INVALID_POINTER);
    AssertPtrReturn(pClientData->State.pCtx, VERR_INVALID_POINTER);

    /*
     * The guest announces formats. Forward to the window thread.
     */
    PostMessage(pClientData->State.pCtx->Win.hWnd, VBOX_CLIPBOARD_WM_SET_FORMATS, 0, u32Formats);

    return VINF_SUCCESS;
}

#ifdef VBOX_STRICT
static int vboxClipboardDbgDumpHtml(const char *pszSrc, size_t cb)
{
    size_t cchIgnored = 0;
    int rc = RTStrNLenEx(pszSrc, cb, &cchIgnored);
    if (RT_SUCCESS(rc))
    {
        char *pszBuf = (char *)RTMemAllocZ(cb + 1);
        if (pszBuf != NULL)
        {
            rc = RTStrCopy(pszBuf, cb + 1, (const char *)pszSrc);
            if (RT_SUCCESS(rc))
            {
                for (size_t i = 0; i < cb; ++i)
                    if (pszBuf[i] == '\n' || pszBuf[i] == '\r')
                        pszBuf[i] = ' ';
            }
            else
                LogFunc(("Error in copying string\n"));
            LogFunc(("Removed \\r\\n: %s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
        {
            rc = VERR_NO_MEMORY;
            LogFunc(("Not enough memory to allocate buffer\n"));
        }
    }
    return rc;
}
#endif

int VBoxClipboardSvcImplReadData(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual)
{
    AssertPtrReturn(pClientData,             VERR_INVALID_POINTER);
    AssertPtrReturn(pClientData->State.pCtx, VERR_INVALID_POINTER);

    LogFlowFunc(("u32Format=%02X\n", u32Format));

    HANDLE hClip = NULL;

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pClientData->State.pCtx->Win;

    /*
     * The guest wants to read data in the given format.
     */
    int rc = VBoxClipboardWinOpen(pWinCtx->hWnd);
    if (RT_SUCCESS(rc))
    {
        LogFunc(("Clipboard opened\n"));

        if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
        {
            hClip = GetClipboardData(CF_DIB);
            if (hClip != NULL)
            {
                LPVOID lp = GlobalLock(hClip);

                if (lp != NULL)
                {
                    LogFunc(("CF_DIB\n"));

                    vboxClipboardGetData(VBOX_SHARED_CLIPBOARD_FMT_BITMAP, lp, GlobalSize(hClip),
                                         pv, cb, pcbActual);

                    GlobalUnlock(hClip);
                }
                else
                {
                    hClip = NULL;
                }
            }
        }
        else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
        {
            hClip = GetClipboardData(CF_UNICODETEXT);
            if (hClip != NULL)
            {
                LPWSTR uniString = (LPWSTR)GlobalLock(hClip);

                if (uniString != NULL)
                {
                    LogFunc(("CF_UNICODETEXT\n"));

                    vboxClipboardGetData(VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT, uniString, (lstrlenW(uniString) + 1) * 2,
                                         pv, cb, pcbActual);

                    GlobalUnlock(hClip);
                }
                else
                {
                    hClip = NULL;
                }
            }
        }
        else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_HTML)
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
                                             pv, cb, pcbActual);
#ifdef VBOX_STRICT
                        LogFlowFunc(("Raw HTML clipboard data from host:"));
                        vboxClipboardDbgDumpHtml((char *)pv, cb);
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
        else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
        {
            hClip = GetClipboardData(CF_HDROP);
            if (hClip != NULL) /* Do we have data in CF_HDROP format? */
            {
                LPVOID lp = GlobalLock(hClip);
                if (lp)
                {
                    void *pvTemp;
                    size_t cbTemp;
                    rc = VBoxClipboardWinDropFilesToStringList((DROPFILES *)lp, (char **)&pvTemp, &cbTemp);
                    if (RT_SUCCESS(rc))
                    {
                        if (cbTemp > cb) /** @todo Add overflow handling! */
                        {
                            AssertMsgFailed(("More data buffer needed -- fix this\n"));
                            cbTemp = cb; /* Never copy more than the available buffer supplies. */
                        }

                        memcpy(pv, pvTemp, cbTemp);

                        RTMemFree(pvTemp);

                        *pcbActual = (uint32_t)cbTemp;
                    }

                    GlobalUnlock(hClip);
                }
            }
        }
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */
        VBoxClipboardWinClose();
    }

    if (hClip == NULL)
    {
        /* Reply with empty data. */
        vboxClipboardGetData(0, NULL, 0, pv, cb, pcbActual);
    }

    return VINF_SUCCESS; /** @todo r=andy Return rc here? */
}

int VBoxClipboardSvcImplWriteData(PVBOXCLIPBOARDCLIENTDATA pClientData, void *pv, uint32_t cb, uint32_t u32Format)
{
    LogFlowFuncEnter();

    /*
     * The guest returns data that was requested in the WM_RENDERFORMAT handler.
     */
    Assert(pClientData->State.data.pv == NULL && pClientData->State.data.cb == 0 && pClientData->State.data.u32Format == 0);

    vboxClipboardDump(pv, cb, u32Format);

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
                    pClientData->State.data.pv        = pszResult;
                    pClientData->State.data.cb        = cbResult;
                    pClientData->State.data.u32Format = u32Format;
                }
            }
        }
        else
        {
            pClientData->State.data.pv = RTMemDup(pv, cb);
            if (pClientData->State.data.pv)
            {
                pClientData->State.data.cb = cb;
                pClientData->State.data.u32Format = u32Format;
            }
        }
    }

    AssertPtr(pClientData->State.pCtx);
    int rc = RTSemEventSignal(pClientData->State.pCtx->hRenderEvent);
    AssertRC(rc);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardSvcImplURIReadDir(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClientData, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteDir(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pClientData, pDirData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileHdr(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClientData, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileHdr(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pClientData, pFileHdr);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIReadFileData(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClientData, pFileData);
    return VERR_NOT_IMPLEMENTED;
}

int VBoxClipboardSvcImplURIWriteFileData(PVBOXCLIPBOARDCLIENTDATA pClientData, PVBOXCLIPBOARDFILEDATA pFileData)
{
    RT_NOREF(pClientData, pFileData);
    return VERR_NOT_IMPLEMENTED;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

