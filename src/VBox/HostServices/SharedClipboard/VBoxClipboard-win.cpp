/** @file
 * Shared Clipboard: Win32 host.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <windows.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <process.h>

#include "VBoxClipboard.h"

#define dprintf Log

static char gachWindowClassName[] = "VBoxSharedClipboardClass";

enum { CBCHAIN_TIMEOUT = 5000 /* ms */ };

struct _VBOXCLIPBOARDCONTEXT
{
    HWND    hwnd;
    HWND    hwndNextInChain;

    UINT     timerRefresh;

    bool     fCBChainPingInProcess;

    RTTHREAD thread;
    bool volatile fTerminate;

    HANDLE hRenderEvent;

    VBOXCLIPBOARDCLIENTDATA *pClient;
};

/* Only one client is supported. There seems to be no need for more clients. */
static VBOXCLIPBOARDCONTEXT g_ctx;


#ifdef LOG_ENABLED
void vboxClipboardDump(const void *pv, size_t cb, uint32_t u32Format)
{
    if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
    {
        Log(("DUMP: VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT:\n"));
        if (pv && cb)
        {
            Log(("%ls\n", pv));
        }
        else
        {
            Log(("%p %d\n", pv, cb));
        }
    }
    else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
    {
        dprintf(("DUMP: VBOX_SHARED_CLIPBOARD_FMT_BITMAP\n"));
    }
    else if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_HTML)
    {
        Log(("DUMP: VBOX_SHARED_CLIPBOARD_FMT_HTML:\n"));
        if (pv && cb)
        {
            Log(("%s\n", pv));
        }
        else
        {
            Log(("%p %d\n", pv, cb));
        }
    }
    else
    {
        dprintf(("DUMP: invalid format %02X\n", u32Format));
    }
}
#else
#define vboxClipboardDump(__pv, __cb, __format) do { NOREF(__pv); NOREF(__cb); NOREF(__format); } while (0)
#endif /* LOG_ENABLED */

static void vboxClipboardGetData (uint32_t u32Format, const void *pvSrc, uint32_t cbSrc,
                                  void *pvDst, uint32_t cbDst, uint32_t *pcbActualDst)
{
    dprintf (("vboxClipboardGetData.\n"));

    *pcbActualDst = cbSrc;

    LogFlow(("vboxClipboardGetData cbSrc = %d, cbDst = %d\n", cbSrc, cbDst));

    if (cbSrc > cbDst)
    {
        /* Do not copy data. The dst buffer is not enough. */
        return;
    }

    memcpy (pvDst, pvSrc, cbSrc);

    vboxClipboardDump(pvDst, cbSrc, u32Format);

    return;
}

static int vboxClipboardReadDataFromClient (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format)
{
    Assert(pCtx->pClient);
    Assert(pCtx->pClient->data.pv == NULL && pCtx->pClient->data.cb == 0 && pCtx->pClient->data.u32Format == 0);

    LogFlow(("vboxClipboardReadDataFromClient u32Format = %02X\n", u32Format));

    ResetEvent (pCtx->hRenderEvent);

    vboxSvcClipboardReportMsg (pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA, u32Format);

    WaitForSingleObject(pCtx->hRenderEvent, INFINITE);

    LogFlow(("vboxClipboardReadDataFromClient wait completed\n"));

    return VINF_SUCCESS;
}

static void vboxClipboardChanged (VBOXCLIPBOARDCONTEXT *pCtx)
{
    LogFlow(("vboxClipboardChanged\n"));

    if (pCtx->pClient == NULL)
    {
        return;
    }

    /* Query list of available formats and report to host. */
    if (OpenClipboard (pCtx->hwnd))
    {
        uint32_t u32Formats = 0;

        UINT format = 0;

        while ((format = EnumClipboardFormats (format)) != 0)
        {
            LogFlow(("vboxClipboardChanged format %#x\n", format));
            switch (format)
            {
                case CF_UNICODETEXT:
                case CF_TEXT:
                    u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
                    break;

                case CF_DIB:
                case CF_BITMAP:
                    u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_BITMAP;
                    break;

                default:
                    if (format >= 0xC000)
                    {
                        TCHAR szFormatName[256];

                        int cActual = GetClipboardFormatName(format, szFormatName, sizeof(szFormatName)/sizeof (TCHAR));

                        if (cActual)
                        {
                            if (strcmp (szFormatName, "HTML Format") == 0)
                            {
                                u32Formats |= VBOX_SHARED_CLIPBOARD_FMT_HTML;
                            }
                        }
                    }
                    break;
            }
        }

        CloseClipboard ();

        LogFlow(("vboxClipboardChanged u32Formats %02X\n", u32Formats));

        vboxSvcClipboardReportMsg (pCtx->pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS, u32Formats);
    }
}

/* Add ourselves into the chain of cliboard listeners */
static void addToCBChain (VBOXCLIPBOARDCONTEXT *pCtx)
{
    pCtx->hwndNextInChain = SetClipboardViewer (pCtx->hwnd);
}

/* Remove ourselves from the chain of cliboard listeners */
static void removeFromCBChain (VBOXCLIPBOARDCONTEXT *pCtx)
{
    ChangeClipboardChain (pCtx->hwnd, pCtx->hwndNextInChain);
    pCtx->hwndNextInChain = NULL;
}

/* Callback which is invoked when we have successfully pinged ourselves down the
 * clipboard chain.  We simply unset a boolean flag to say that we are responding.
 * There is a race if a ping returns after the next one is initiated, but nothing
 * very bad is likely to happen. */
VOID CALLBACK CBChainPingProc(HWND hwnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult)
{
    (void) hwnd;
    (void) uMsg;
    (void) lResult;
    VBOXCLIPBOARDCONTEXT *pCtx = (VBOXCLIPBOARDCONTEXT *)dwData;
    pCtx->fCBChainPingInProcess = FALSE;
}

static LRESULT CALLBACK vboxClipboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT rc = 0;

    VBOXCLIPBOARDCONTEXT *pCtx = &g_ctx;

    switch (msg)
    {
        case WM_CHANGECBCHAIN:
        {
            Log(("WM_CHANGECBCHAIN\n"));

            HWND hwndRemoved = (HWND)wParam;
            HWND hwndNext    = (HWND)lParam;

            if (hwndRemoved == pCtx->hwndNextInChain)
            {
                /* The window that was next to our in the chain is being removed.
                 * Relink to the new next window.
                 */
                pCtx->hwndNextInChain = hwndNext;
            }
            else
            {
                if (pCtx->hwndNextInChain)
                {
                    /* Pass the message further. */
                    DWORD_PTR dwResult;
                    rc = SendMessageTimeout(pCtx->hwndNextInChain, WM_CHANGECBCHAIN, wParam, lParam, 0, CBCHAIN_TIMEOUT, &dwResult);
                    if (!rc)
                        rc = (LRESULT)dwResult;
                }
            }
        } break;

        case WM_DRAWCLIPBOARD:
        {
            Log(("WM_DRAWCLIPBOARD next %p\n", pCtx->hwndNextInChain));

            if (GetClipboardOwner () != hwnd)
            {
                /* Clipboard was updated by another application. */
                vboxClipboardChanged (pCtx);
            }

            if (pCtx->hwndNextInChain)
            {
                /* Pass the message to next windows in the clipboard chain. */
                DWORD_PTR dwResult;
                rc = SendMessageTimeout(pCtx->hwndNextInChain, msg, wParam, lParam, 0, CBCHAIN_TIMEOUT, &dwResult);
                if (!rc)
                    rc = dwResult;
            }
        } break;

        case WM_TIMER:
        {
            HWND hViewer = GetClipboardViewer();

            /* Re-register ourselves in the clipboard chain if our last ping
             * timed out or there seems to be no valid chain. */
            if (!hViewer || pCtx->fCBChainPingInProcess)
            {
                removeFromCBChain(pCtx);
                addToCBChain(pCtx);
            }
            /* Start a new ping by passing a dummy WM_CHANGECBCHAIN to be
             * processed by ourselves to the chain. */
            pCtx->fCBChainPingInProcess = TRUE;
            hViewer = GetClipboardViewer();
            if (hViewer)
                SendMessageCallback(hViewer, WM_CHANGECBCHAIN, (WPARAM)pCtx->hwndNextInChain, (LPARAM)pCtx->hwndNextInChain, CBChainPingProc, (ULONG_PTR) pCtx);
        } break;

        case WM_CLOSE:
        {
            /* Do nothing. Ignore the message. */
        } break;

        case WM_RENDERFORMAT:
        {
            /* Insert the requested clipboard format data into the clipboard. */
            uint32_t u32Format = 0;

            UINT format = (UINT)wParam;

            Log(("WM_RENDERFORMAT %d\n", format));

            switch (format)
            {
                case CF_UNICODETEXT:
                    u32Format |= VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT;
                    break;

                case CF_DIB:
                    u32Format |= VBOX_SHARED_CLIPBOARD_FMT_BITMAP;
                    break;

                default:
                    if (format >= 0xC000)
                    {
                        TCHAR szFormatName[256];

                        int cActual = GetClipboardFormatName(format, szFormatName, sizeof(szFormatName)/sizeof (TCHAR));

                        if (cActual)
                        {
                            if (strcmp (szFormatName, "HTML Format") == 0)
                            {
                                u32Format |= VBOX_SHARED_CLIPBOARD_FMT_HTML;
                            }
                        }
                    }
                    break;
            }

            if (u32Format == 0 || pCtx->pClient == NULL)
            {
                /* Unsupported clipboard format is requested. */
                Log(("WM_RENDERFORMAT unsupported format requested or client is not active.\n"));
                EmptyClipboard ();
            }
            else
            {
                int vboxrc = vboxClipboardReadDataFromClient (pCtx, u32Format);

                dprintf(("vboxClipboardReadDataFromClient vboxrc = %d\n", vboxrc));

                if (   RT_SUCCESS (vboxrc)
                    && pCtx->pClient->data.pv != NULL
                    && pCtx->pClient->data.cb > 0
                    && pCtx->pClient->data.u32Format == u32Format)
                {
                    HANDLE hMem = GlobalAlloc (GMEM_DDESHARE | GMEM_MOVEABLE, pCtx->pClient->data.cb);

                    dprintf(("hMem %p\n", hMem));

                    if (hMem)
                    {
                        void *pMem = GlobalLock (hMem);

                        dprintf(("pMem %p, GlobalSize %d\n", pMem, GlobalSize (hMem)));

                        if (pMem)
                        {
                            Log(("WM_RENDERFORMAT setting data\n"));

                            if (pCtx->pClient->data.pv)
                            {
                                memcpy (pMem, pCtx->pClient->data.pv, pCtx->pClient->data.cb);

                                RTMemFree (pCtx->pClient->data.pv);
                                pCtx->pClient->data.pv        = NULL;
                            }

                            pCtx->pClient->data.cb        = 0;
                            pCtx->pClient->data.u32Format = 0;

                            /* The memory must be unlocked before inserting to the Clipboard. */
                            GlobalUnlock (hMem);

                            /* 'hMem' contains the host clipboard data.
                             * size is 'cb' and format is 'format'.
                             */
                            HANDLE hClip = SetClipboardData (format, hMem);

                            dprintf(("vboxClipboardHostEvent hClip %p\n", hClip));

                            if (hClip)
                            {
                                /* The hMem ownership has gone to the system. Nothing to do. */
                                break;
                            }
                        }

                        GlobalFree (hMem);
                    }
                }

                RTMemFree (pCtx->pClient->data.pv);
                pCtx->pClient->data.pv        = NULL;
                pCtx->pClient->data.cb        = 0;
                pCtx->pClient->data.u32Format = 0;

                /* Something went wrong. */
                EmptyClipboard ();
            }
        } break;

        case WM_RENDERALLFORMATS:
        {
            Log(("WM_RENDERALLFORMATS\n"));

            /* Do nothing. The clipboard formats will be unavailable now, because the
             * windows is to be destroyed and therefore the guest side becomes inactive.
             */
            if (OpenClipboard (hwnd))
            {
                EmptyClipboard();

                CloseClipboard();
            }
        } break;

        case WM_USER:
        {
            if (pCtx->pClient == NULL || pCtx->pClient->fMsgFormats)
            {
                /* Host has pending formats message. Ignore the guest announcement,
                 * because host clipboard has more priority.
                 */
                break;
            }

            /* Announce available formats. Do not insert data, they will be inserted in WM_RENDER*. */
            uint32_t u32Formats = (uint32_t)lParam;

            Log(("WM_USER u32Formats = %02X\n", u32Formats));

            if (OpenClipboard (hwnd))
            {
                EmptyClipboard();

                Log(("WM_USER emptied clipboard\n"));

                HANDLE hClip = NULL;

                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                {
                    dprintf(("window proc WM_USER: VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT\n"));

                    hClip = SetClipboardData (CF_UNICODETEXT, NULL);
                }

                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
                {
                    dprintf(("window proc WM_USER: VBOX_SHARED_CLIPBOARD_FMT_BITMAP\n"));

                    hClip = SetClipboardData (CF_DIB, NULL);
                }

                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_HTML)
                {
                    UINT format = RegisterClipboardFormat ("HTML Format");
                    dprintf(("window proc WM_USER: VBOX_SHARED_CLIPBOARD_FMT_HTML 0x%04X\n", format));
                    if (format != 0)
                    {
                        hClip = SetClipboardData (format, NULL);
                    }
                }

                CloseClipboard();

                dprintf(("window proc WM_USER: hClip %p, err %d\n", hClip, GetLastError ()));
            }
            else
            {
                dprintf(("window proc WM_USER: failed to open clipboard\n"));
            }
        } break;

        default:
        {
            Log(("WM_ %p\n", msg));
            rc = DefWindowProc (hwnd, msg, wParam, lParam);
        }
    }

    Log(("WM_ rc %d\n", rc));
    return rc;
}

DECLCALLBACK(int) VBoxClipboardThread (RTTHREAD ThreadSelf, void *pInstance)
{
    /* Create a window and make it a clipboard viewer. */
    int rc = VINF_SUCCESS;

    LogFlow(("VBoxClipboardThread\n"));

    VBOXCLIPBOARDCONTEXT *pCtx = &g_ctx;

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle (NULL);

    /* Register the Window Class. */
    WNDCLASS wc;

    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = vboxClipboardWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = gachWindowClassName;

    ATOM atomWindowClass = RegisterClass (&wc);

    if (atomWindowClass == 0)
    {
        Log(("Failed to register window class\n"));
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        /* Create the window. */
        pCtx->hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                     gachWindowClassName, gachWindowClassName,
                                     WS_POPUPWINDOW,
                                     -200, -200, 100, 100, NULL, NULL, hInstance, NULL);

        if (pCtx->hwnd == NULL)
        {
            Log(("Failed to create window\n"));
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            SetWindowPos(pCtx->hwnd, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

            addToCBChain(pCtx);
            pCtx->timerRefresh = SetTimer(pCtx->hwnd, 0, 10 * 1000, NULL);

            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0) && !pCtx->fTerminate)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    if (pCtx->hwnd)
    {
        removeFromCBChain(pCtx);
        if (pCtx->timerRefresh)
            KillTimer(pCtx->hwnd, 0);

        DestroyWindow (pCtx->hwnd);
        pCtx->hwnd = NULL;
    }

    if (atomWindowClass != 0)
    {
        UnregisterClass (gachWindowClassName, hInstance);
        atomWindowClass = 0;
    }

    return 0;
}

/*
 * Public platform dependent functions.
 */
int vboxClipboardInit (void)
{
    int rc = VINF_SUCCESS;

    g_ctx.hRenderEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    rc = RTThreadCreate (&g_ctx.thread, VBoxClipboardThread, NULL, 65536,
                         RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "SHCLIP");

    if (RT_FAILURE (rc))
    {
        CloseHandle (g_ctx.hRenderEvent);
    }

    return rc;
}

void vboxClipboardDestroy (void)
{
    Log(("vboxClipboardDestroy\n"));

    /* Set the termination flag and ping the window thread. */
    ASMAtomicWriteBool (&g_ctx.fTerminate, true);

    if (g_ctx.hwnd)
    {
        PostMessage (g_ctx.hwnd, WM_CLOSE, 0, 0);
    }

    CloseHandle (g_ctx.hRenderEvent);

    /* Wait for the window thread to terminate. */
    RTThreadWait (g_ctx.thread, RT_INDEFINITE_WAIT, NULL);

    g_ctx.thread = NIL_RTTHREAD;
}

int vboxClipboardConnect (VBOXCLIPBOARDCLIENTDATA *pClient, bool)
{
    Log(("vboxClipboardConnect\n"));

    if (g_ctx.pClient != NULL)
    {
        /* One client only. */
        return VERR_NOT_SUPPORTED;
    }

    pClient->pCtx = &g_ctx;

    pClient->pCtx->pClient = pClient;

    /* Sync the host clipboard content with the client. */
    vboxClipboardSync (pClient);

    return VINF_SUCCESS;
}

int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    /* Sync the host clipboard content with the client. */
    vboxClipboardChanged (pClient->pCtx);

    return VINF_SUCCESS;
}

void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
    Log(("vboxClipboardDisconnect\n"));

    g_ctx.pClient = NULL;
}

void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Formats)
{
    /*
     * The guest announces formats. Forward to the window thread.
     */
    PostMessage (pClient->pCtx->hwnd, WM_USER, 0, u32Formats);
}

int vboxClipboardReadData (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual)
{
    LogFlow(("vboxClipboardReadData: u32Format = %02X\n", u32Format));

    HANDLE hClip = NULL;

    /*
     * The guest wants to read data in the given format.
     */
    if (OpenClipboard (pClient->pCtx->hwnd))
    {
        dprintf(("Clipboard opened.\n"));

        if (u32Format & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
        {
            hClip = GetClipboardData (CF_DIB);

            if (hClip != NULL)
            {
                LPVOID lp = GlobalLock (hClip);

                if (lp != NULL)
                {
                    dprintf(("CF_DIB\n"));

                    vboxClipboardGetData (VBOX_SHARED_CLIPBOARD_FMT_BITMAP, lp, GlobalSize (hClip),
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
                LPWSTR uniString = (LPWSTR)GlobalLock (hClip);

                if (uniString != NULL)
                {
                    dprintf(("CF_UNICODETEXT\n"));

                    vboxClipboardGetData (VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT, uniString, (lstrlenW (uniString) + 1) * 2,
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
            UINT format = RegisterClipboardFormat ("HTML Format");

            if (format != 0)
            {
                hClip = GetClipboardData (format);

                if (hClip != NULL)
                {
                    LPVOID lp = GlobalLock (hClip);

                    if (lp != NULL)
                    {
                        dprintf(("CF_HTML\n"));

                        vboxClipboardGetData (VBOX_SHARED_CLIPBOARD_FMT_HTML, lp, GlobalSize (hClip),
                                              pv, cb, pcbActual);

                        GlobalUnlock(hClip);
                    }
                    else
                    {
                        hClip = NULL;
                    }
                }
            }
        }

        CloseClipboard ();
    }
    else
    {
        dprintf(("failed to open clipboard\n"));
    }

    if (hClip == NULL)
    {
        /* Reply with empty data. */
        vboxClipboardGetData (0, NULL, 0,
                              pv, cb, pcbActual);
    }

    return VINF_SUCCESS;
}

void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv, uint32_t cb, uint32_t u32Format)
{
    LogFlow(("vboxClipboardWriteData\n"));

    /*
     * The guest returns data that was requested in the WM_RENDERFORMAT handler.
     */
    Assert(pClient->data.pv == NULL && pClient->data.cb == 0 && pClient->data.u32Format == 0);

    vboxClipboardDump(pv, cb, u32Format);

    if (cb > 0)
    {
        pClient->data.pv = RTMemAlloc (cb);

        if (pClient->data.pv)
        {
            memcpy (pClient->data.pv, pv, cb);
            pClient->data.cb = cb;
            pClient->data.u32Format = u32Format;
        }
    }

    SetEvent(pClient->pCtx->hRenderEvent);
}
