/** @file
 *
 * VBoxClipboard - Shared clipboard
 *
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxTray.h"
#include "VBoxHelpers.h"

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <strsafe.h>

typedef struct _VBOXCLIPBOARDCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    uint32_t u32ClientID;

    ATOM     atomWindowClass;

    HWND     hwnd;

    HWND     hwndNextInChain;

    UINT     timerRefresh;

    bool     fCBChainPingInProcess;

//    bool     fOperational;

//    uint32_t u32LastSentFormat;
//    uint64_t u64LastSentCRC64;

} VBOXCLIPBOARDCONTEXT;

static char gachWindowClassName[] = "VBoxSharedClipboardClass";

enum { CBCHAIN_TIMEOUT = 5000 /* ms */ };

static int vboxClipboardChanged(VBOXCLIPBOARDCONTEXT *pCtx)
{
    AssertPtr(pCtx);

    /* Query list of available formats and report to host. */
    int rc = VINF_SUCCESS;
    if (FALSE == OpenClipboard(pCtx->hwnd))
    {
        rc = RTErrConvertFromWin32(GetLastError());
    }
    else
    {
        uint32_t u32Formats = 0;
        UINT format = 0;

        while ((format = EnumClipboardFormats (format)) != 0)
        {
            Log(("VBoxTray: vboxClipboardChanged: format = 0x%08X\n", format));
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
        rc = VbglR3ClipboardReportFormats(pCtx->u32ClientID, u32Formats);
    }
    return rc;
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

static LRESULT vboxClipboardProcessMsg(VBOXCLIPBOARDCONTEXT *pCtx, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT rc = 0;

    switch (msg)
    {
        case WM_CHANGECBCHAIN:
        {
            HWND hwndRemoved = (HWND)wParam;
            HWND hwndNext    = (HWND)lParam;

            Log(("VBoxTray: vboxClipboardProcessMsg: WM_CHANGECBCHAIN: hwndRemoved %p, hwndNext %p, hwnd %p\n", hwndRemoved, hwndNext, pCtx->hwnd));

            if (hwndRemoved == pCtx->hwndNextInChain)
            {
                /* The window that was next to our in the chain is being removed.
                 * Relink to the new next window. */
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
                        rc = (LRESULT) dwResult;
                }
            }
        } break;

        case WM_DRAWCLIPBOARD:
        {
            Log(("VBoxTray: vboxClipboardProcessMsg: WM_DRAWCLIPBOARD, hwnd %p\n", pCtx->hwnd));

            if (GetClipboardOwner () != hwnd)
            {
                /* Clipboard was updated by another application. */
                /* WM_DRAWCLIPBOARD always expects a return code of 0, so don't change "rc" here. */
                int vboxrc = vboxClipboardChanged(pCtx);
                if (RT_FAILURE(vboxrc))
                    Log(("VBoxTray: vboxClipboardProcessMsg: vboxClipboardChanged failed, rc = %Rrc\n", vboxrc));
            }

            /* Pass the message to next windows in the clipboard chain. */
            SendMessageTimeout(pCtx->hwndNextInChain, msg, wParam, lParam, 0, CBCHAIN_TIMEOUT, NULL);
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

            Log(("VBoxTray: vboxClipboardProcessMsg: WM_RENDERFORMAT, format = %x\n", format));
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

            if (u32Format == 0)
            {
                /* Unsupported clipboard format is requested. */
                Log(("VBoxTray: vboxClipboardProcessMsg: Unsupported clipboard format requested: %ld\n", u32Format));
                EmptyClipboard();
            }
            else
            {
                const uint32_t cbPrealloc = 4096; /* @todo r=andy Make it dynamic for supporting larger text buffers! */
                uint32_t cb = 0;

                /* Preallocate a buffer, most of small text transfers will fit into it. */
                HANDLE hMem = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, cbPrealloc);
                Log(("VBoxTray: vboxClipboardProcessMsg: Preallocated handle hMem = %p\n", hMem));

                if (hMem)
                {
                    void *pMem = GlobalLock(hMem);
                    Log(("VBoxTray: vboxClipboardProcessMsg: Locked pMem = %p, GlobalSize = %ld\n", pMem, GlobalSize(hMem)));

                    if (pMem)
                    {
                        /* Read the host data to the preallocated buffer. */
                        int vboxrc = VbglR3ClipboardReadData(pCtx->u32ClientID, u32Format, pMem, cbPrealloc, &cb);
                        Log(("VBoxTray: vboxClipboardProcessMsg: VbglR3ClipboardReadData returned with rc = %Rrc\n",  vboxrc));

                        if (RT_SUCCESS(vboxrc))
                        {
                            if (cb == 0)
                            {
                                /* 0 bytes returned means the clipboard is empty.
                                 * Deallocate the memory and set hMem to NULL to get to
                                 * the clipboard empty code path. */
                                GlobalUnlock(hMem);
                                GlobalFree(hMem);
                                hMem = NULL;
                            }
                            else if (cb > cbPrealloc)
                            {
                                GlobalUnlock(hMem);

                                /* The preallocated buffer is too small, adjust the size. */
                                hMem = GlobalReAlloc(hMem, cb, 0);
                                Log(("VBoxTray: vboxClipboardProcessMsg: Reallocated hMem = %p\n", hMem));

                                if (hMem)
                                {
                                    pMem = GlobalLock(hMem);
                                    Log(("VBoxTray: vboxClipboardProcessMsg: Locked pMem = %p, GlobalSize = %ld\n", pMem, GlobalSize(hMem)));

                                    if (pMem)
                                    {
                                        /* Read the host data to the preallocated buffer. */
                                        uint32_t cbNew = 0;
                                        vboxrc = VbglR3ClipboardReadData(pCtx->u32ClientID, u32Format, pMem, cb, &cbNew);
                                        Log(("VBoxTray: VbglR3ClipboardReadData returned with rc = %Rrc, cb = %d, cbNew = %d\n", vboxrc, cb, cbNew));

                                        if (RT_SUCCESS (vboxrc) && cbNew <= cb)
                                        {
                                            cb = cbNew;
                                        }
                                        else
                                        {
                                            GlobalUnlock(hMem);
                                            GlobalFree(hMem);
                                            hMem = NULL;
                                        }
                                    }
                                    else
                                    {
                                        GlobalFree(hMem);
                                        hMem = NULL;
                                    }
                                }
                            }

                            if (hMem)
                            {
                                /* pMem is the address of the data. cb is the size of returned data. */
                                /* Verify the size of returned text, the memory block for clipboard
                                 * must have the exact string size.
                                 */
                                if (u32Format == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                                {
                                    size_t cbActual = 0;
                                    HRESULT hrc = StringCbLengthW((LPWSTR)pMem, cb, &cbActual);
                                    if (FAILED (hrc))
                                    {
                                        /* Discard invalid data. */
                                        GlobalUnlock(hMem);
                                        GlobalFree(hMem);
                                        hMem = NULL;
                                    }
                                    else
                                    {
                                        /* cbActual is the number of bytes, excluding those used
                                         * for the terminating null character.
                                         */
                                        cb = (uint32_t)(cbActual + 2);
                                    }
                                }
                            }

                            if (hMem)
                            {
                                GlobalUnlock(hMem);

                                hMem = GlobalReAlloc(hMem, cb, 0);
                                Log(("VBoxTray: vboxClipboardProcessMsg: Reallocated hMem = %p\n", hMem));

                                if (hMem)
                                {
                                    /* 'hMem' contains the host clipboard data.
                                     * size is 'cb' and format is 'format'. */
                                    HANDLE hClip = SetClipboardData(format, hMem);
                                    Log(("VBoxTray: vboxClipboardProcessMsg: WM_RENDERFORMAT hClip = %p\n", hClip));

                                    if (hClip)
                                    {
                                        /* The hMem ownership has gone to the system. Finish the processing. */
                                        break;
                                    }

                                    /* Cleanup follows. */
                                }
                            }
                        }
                        if (hMem)
                            GlobalUnlock(hMem);
                    }
                    if (hMem)
                        GlobalFree(hMem);
                }

                /* Something went wrong. */
                EmptyClipboard();
            }
        } break;

        case WM_RENDERALLFORMATS:
        {
            /* Do nothing. The clipboard formats will be unavailable now, because the
             * windows is to be destroyed and therefore the guest side becomes inactive.
             */
            if (OpenClipboard(hwnd))
            {
                EmptyClipboard();
                CloseClipboard();
            }
        } break;

        case WM_USER:
        {
            /* Announce available formats. Do not insert data, they will be inserted in WM_RENDER*. */
            uint32_t u32Formats = (uint32_t)lParam;

            if (FALSE == OpenClipboard(hwnd))
            {
                Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER: Failed to open clipboard! Last error = %ld\n", GetLastError()));
            }
            else
            {
                EmptyClipboard();

                HANDLE hClip = NULL;

                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                {
                    Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER: VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT\n"));
                    hClip = SetClipboardData(CF_UNICODETEXT, NULL);
                }

                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
                {
                    Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER: VBOX_SHARED_CLIPBOARD_FMT_BITMAP\n"));
                    hClip = SetClipboardData(CF_DIB, NULL);
                }

                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_HTML)
                {
                    UINT format = RegisterClipboardFormat ("HTML Format");
                    Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER: VBOX_SHARED_CLIPBOARD_FMT_HTML 0x%04X\n", format));
                    if (format != 0)
                    {
                        hClip = SetClipboardData(format, NULL);
                    }
                }

                CloseClipboard();
                Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER: hClip = %p, err = %ld\n", hClip, GetLastError ()));
            }
        } break;

        case WM_USER + 1:
        {
            /* Send data in the specified format to the host. */
            uint32_t u32Formats = (uint32_t)lParam;
            HANDLE hClip = NULL;

            if (FALSE == OpenClipboard(hwnd))
            {
                Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER: Failed to open clipboard! Last error = %ld\n", GetLastError()));
            }
            else
            {
                int vboxrc;
                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
                {
                    hClip = GetClipboardData(CF_DIB);

                    if (hClip != NULL)
                    {
                        LPVOID lp = GlobalLock(hClip);
                        if (lp != NULL)
                        {
                            Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER + 1: CF_DIB\n"));
                            vboxrc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_BITMAP,
                                                              lp, GlobalSize(hClip));
                            GlobalUnlock(hClip);
                        }
                        else
                        {
                            hClip = NULL;
                        }
                    }
                }
                else if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                {
                    hClip = GetClipboardData(CF_UNICODETEXT);

                    if (hClip != NULL)
                    {
                        LPWSTR uniString = (LPWSTR)GlobalLock(hClip);

                        if (uniString != NULL)
                        {
                            Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER + 1: CF_UNICODETEXT\n"));
                            vboxrc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                                              uniString, (lstrlenW(uniString) + 1) * 2);
                            GlobalUnlock(hClip);
                        }
                        else
                        {
                            hClip = NULL;
                        }
                    }
                }
                else if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_HTML)
                {
                    UINT format = RegisterClipboardFormat ("HTML Format");
                    if (format != 0)
                    {
                        hClip = GetClipboardData(format);
                        if (hClip != NULL)
                        {
                            LPVOID lp = GlobalLock(hClip);

                            if (lp != NULL)
                            {
                                Log(("VBoxTray: vboxClipboardProcessMsg: WM_USER + 1: CF_HTML\n"));
                                vboxrc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_HTML,
                                                                  lp, GlobalSize(hClip));
                                GlobalUnlock(hClip);
                            }
                            else
                            {
                                hClip = NULL;
                            }
                        }
                    }
                }

                CloseClipboard();
            }

            if (hClip == NULL)
            {
                /* Requested clipboard format is not available, send empty data. */
                VbglR3ClipboardWriteData(pCtx->u32ClientID, 0, NULL, 0);
            }
        } break;

        default:
        {
            rc = DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

    Log(("VBoxTray: vboxClipboardProcessMsg returned with rc = %ld\n", rc));
    return rc;
}

static LRESULT CALLBACK vboxClipboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int vboxClipboardInit (VBOXCLIPBOARDCONTEXT *pCtx)
{
    /* Register the Window Class. */
    WNDCLASS wc;

    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = vboxClipboardWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = pCtx->pEnv->hInstance;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = gachWindowClassName;

    pCtx->atomWindowClass = RegisterClass (&wc);

    int rc = VINF_SUCCESS;
    if (pCtx->atomWindowClass == 0)
    {
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        /* Create the window. */
        pCtx->hwnd = CreateWindowEx (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                     gachWindowClassName, gachWindowClassName,
                                     WS_POPUPWINDOW,
                                     -200, -200, 100, 100, NULL, NULL, pCtx->pEnv->hInstance, NULL);

        if (pCtx->hwnd == NULL)
        {
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            SetWindowPos(pCtx->hwnd, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

            addToCBChain(pCtx);
            pCtx->timerRefresh = SetTimer(pCtx->hwnd, 0, 10 * 1000, NULL);
        }
    }

    Log(("VBoxTray: vboxClipboardInit returned with rc = %Rrc\n", rc));
    return rc;
}

static void vboxClipboardDestroy(VBOXCLIPBOARDCONTEXT *pCtx)
{
    if (pCtx->hwnd)
    {
        removeFromCBChain(pCtx);
        if (pCtx->timerRefresh)
            KillTimer(pCtx->hwnd, 0);

        DestroyWindow (pCtx->hwnd);
        pCtx->hwnd = NULL;
    }

    if (pCtx->atomWindowClass != 0)
    {
        UnregisterClass(gachWindowClassName, pCtx->pEnv->hInstance);
        pCtx->atomWindowClass = 0;
    }
}

/* Static since it is the single instance. Directly used in the windows proc. */
static VBOXCLIPBOARDCONTEXT gCtx = { NULL };

static LRESULT CALLBACK vboxClipboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* Forward with proper context. */
    return vboxClipboardProcessMsg(&gCtx, hwnd, msg, wParam, lParam);
}

int VBoxClipboardInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    Log(("VBoxTray: VboxClipboardInit\n"));
    if (gCtx.pEnv)
    {
        /* Clipboard was already initialized. 2 or more instances are not supported. */
        return VERR_NOT_SUPPORTED;
    }

    if (VbglR3AutoLogonIsRemoteSession())
    {
        /* Do not use clipboard for remote sessions. */
        LogRel(("VBoxTray: clipboard has been disabled for a remote session.\n"));
        return VERR_NOT_SUPPORTED;
    }

    RT_ZERO (gCtx);
    gCtx.pEnv = pEnv;

    int rc = VbglR3ClipboardConnect(&gCtx.u32ClientID);
    if (RT_SUCCESS (rc))
    {
        rc = vboxClipboardInit(&gCtx);
        if (RT_SUCCESS (rc))
        {
            /* Always start the thread for host messages. */
            *pfStartThread = true;
        }
        else
        {
            VbglR3ClipboardDisconnect(gCtx.u32ClientID);
        }
    }

    if (RT_SUCCESS(rc))
        *ppInstance = &gCtx;
    return rc;
}

unsigned __stdcall VBoxClipboardThread(void *pInstance)
{
    Log(("VBoxTray: VBoxClipboardThread\n"));

    VBOXCLIPBOARDCONTEXT *pCtx = (VBOXCLIPBOARDCONTEXT *)pInstance;
    AssertPtr(pCtx);

    /* The thread waits for incoming messages from the host. */
    for (;;)
    {
        uint32_t u32Msg;
        uint32_t u32Formats;
        int rc = VbglR3ClipboardGetHostMsg(pCtx->u32ClientID, &u32Msg, &u32Formats);
        if (RT_FAILURE(rc))
        {
            Log(("VBoxTray: VBoxClipboardThread: Failed to call the driver for host message! rc = %Rrc\n", rc));
            if (rc == VERR_INTERRUPTED)
            {
                /* Wait for termination event. */
                WaitForSingleObject(pCtx->pEnv->hStopEvent, INFINITE);
                break;
            }
            /* Wait a bit before retrying. */
            AssertPtr(pCtx->pEnv);
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
            {
                break;
            }
            continue;
       }
        else
        {
            Log(("VBoxTray: VBoxClipboardThread: VbglR3ClipboardGetHostMsg u32Msg = %ld, u32Formats = %ld\n", u32Msg, u32Formats));
            switch (u32Msg)
            {
                case VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS:
                {
                    /* The host has announced available clipboard formats.
                     * Forward the information to the window, so it can later
                     * respond to WM_RENDERFORMAT message. */
                    ::PostMessage (pCtx->hwnd, WM_USER, 0, u32Formats);
                } break;

                case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
                {
                    /* The host needs data in the specified format. */
                    ::PostMessage (pCtx->hwnd, WM_USER + 1, 0, u32Formats);
                } break;

                case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
                {
                    /* The host is terminating. */
                    rc = VERR_INTERRUPTED;
                } break;

                default:
                {
                    Log(("VBoxTray: VBoxClipboardThread: Unsupported message from host! Message = %ld\n", u32Msg));
                }
            }
        }
    }
    return 0;
}

void VBoxClipboardDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    VBOXCLIPBOARDCONTEXT *pCtx = (VBOXCLIPBOARDCONTEXT *)pInstance;
    if (pCtx != &gCtx)
    {
        Log(("VBoxTray: VBoxClipboardDestroy: invalid instance %p (our = %p)!\n", pCtx, &gCtx));
        pCtx = &gCtx;
    }

    vboxClipboardDestroy (pCtx);
    VbglR3ClipboardDisconnect(pCtx->u32ClientID);
    memset (pCtx, 0, sizeof (*pCtx));
    return;
}

