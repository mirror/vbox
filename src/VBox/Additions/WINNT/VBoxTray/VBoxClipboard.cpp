/* $Id$ */
/** @file
 * VBoxClipboard - Shared clipboard, Windows Guest Implementation.
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
#include "VBoxTray.h"
#include "VBoxHelpers.h"

#include <iprt/asm.h>
#include <iprt/ldr.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-win.h>
#include <strsafe.h>

#include <VBox/log.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

struct _VBOXCLIPBOARDCONTEXT
{
    /** Pointer to the VBoxClient service environment. */
    const VBOXSERVICEENV    *pEnv;
    /** Client ID the service is connected to the HGCM service with. */
    uint32_t                 u32ClientID;
    /** Windows-specific context data. */
    VBOXCLIPBOARDWINCTX      Win;
};


/*********************************************************************************************************************************
*   Static variables                                                                                                             *
*********************************************************************************************************************************/
/** Static clipboard context (since it is the single instance). Directly used in the windows proc. */
static VBOXCLIPBOARDCONTEXT g_Ctx = { NULL };
/** Static window class name. */
static char s_szClipWndClassName[] = VBOX_CLIPBOARD_WNDCLASS_NAME;


static LRESULT vboxClipboardProcessMsg(PVBOXCLIPBOARDCONTEXT pCtx, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AssertPtr(pCtx);

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

    LRESULT rc = 0;

    switch (msg)
    {
        case WM_CLIPBOARDUPDATE:
        {
           Log(("WM_CLIPBOARDUPDATE\n"));

           if (GetClipboardOwner() != hwnd)
           {
               /* Clipboard was updated by another application. */
               uint32_t uFormats;
               int vboxrc = VBoxClipboardWinGetFormats(&pCtx->Win, &uFormats);
               if (RT_SUCCESS(vboxrc))
                   vboxrc = VbglR3ClipboardReportFormats(pCtx->u32ClientID, uFormats);
           }
        }
        break;

        case WM_CHANGECBCHAIN:
        {
           if (VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
           {
               rc = DefWindowProc(hwnd, msg, wParam, lParam);
               break;
           }

           HWND hWndRemoved = (HWND)wParam;
           HWND hWndNext    = (HWND)lParam;

           LogFlowFunc(("WM_CHANGECBCHAIN: hWndRemoved=%p, hWndNext=%p, hWnd=%p\n", hWndRemoved, hWndNext, pWinCtx->hWnd));

           if (hWndRemoved == pWinCtx->hWndNextInChain)
           {
               /* The window that was next to our in the chain is being removed.
                * Relink to the new next window. */
               pWinCtx->hWndNextInChain = hWndNext;
           }
           else
           {
               if (pWinCtx->hWndNextInChain)
               {
                   /* Pass the message further. */
                   DWORD_PTR dwResult;
                   rc = SendMessageTimeout(pWinCtx->hWndNextInChain, WM_CHANGECBCHAIN, wParam, lParam, 0,
                                           VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS, &dwResult);
                   if (!rc)
                       rc = (LRESULT)dwResult;
               }
           }
        }
        break;

        case WM_DRAWCLIPBOARD:
        {
           LogFlowFunc(("WM_DRAWCLIPBOARD, hwnd %p\n", pWinCtx->hWnd));

           if (GetClipboardOwner() != hwnd)
           {
               /* Clipboard was updated by another application. */
               /* WM_DRAWCLIPBOARD always expects a return code of 0, so don't change "rc" here. */
               uint32_t uFormats;
               int vboxrc = VBoxClipboardWinGetFormats(pWinCtx, &uFormats);
               if (RT_SUCCESS(vboxrc))
                   vboxrc = VbglR3ClipboardReportFormats(pCtx->u32ClientID, uFormats);
           }

           if (pWinCtx->hWndNextInChain)
           {
               /* Pass the message to next windows in the clipboard chain. */
               SendMessageTimeout(pWinCtx->hWndNextInChain, msg, wParam, lParam, 0, VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS, NULL);
           }
        }
        break;

        case WM_TIMER:
        {
           if (VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
               break;

           HWND hViewer = GetClipboardViewer();

           /* Re-register ourselves in the clipboard chain if our last ping
            * timed out or there seems to be no valid chain. */
           if (!hViewer || pWinCtx->oldAPI.fCBChainPingInProcess)
           {
               VBoxClipboardWinRemoveFromCBChain(pWinCtx);
               VBoxClipboardWinAddToCBChain(pWinCtx);
           }

           /* Start a new ping by passing a dummy WM_CHANGECBCHAIN to be
            * processed by ourselves to the chain. */
           pWinCtx->oldAPI.fCBChainPingInProcess = TRUE;

           hViewer = GetClipboardViewer();
           if (hViewer)
               SendMessageCallback(hViewer, WM_CHANGECBCHAIN, (WPARAM)pWinCtx->hWndNextInChain, (LPARAM)pWinCtx->hWndNextInChain,
                                   VBoxClipboardWinChainPingProc, (ULONG_PTR)pWinCtx);
        }
        break;

        case WM_CLOSE:
        {
           /* Do nothing. Ignore the message. */
        }
        break;

        case WM_RENDERFORMAT:
        {
           /* Insert the requested clipboard format data into the clipboard. */
           uint32_t u32Format = 0;
           UINT format = (UINT)wParam;

           LogFlowFunc(("WM_RENDERFORMAT, format = %x\n", format));
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

                      int cActual = GetClipboardFormatName(format, szFormatName, sizeof(szFormatName) / sizeof(TCHAR));
                      if (cActual)
                      {
                          if (strcmp(szFormatName, "HTML Format") == 0)
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
               LogFlowFunc(("Unsupported clipboard format requested: %ld\n", u32Format));
               VBoxClipboardWinClear();
           }
           else
           {
               const uint32_t cbPrealloc = 4096; /** @todo r=andy Make it dynamic for supporting larger text buffers! */
               uint32_t cb = 0;

               /* Preallocate a buffer, most of small text transfers will fit into it. */
               HANDLE hMem = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, cbPrealloc);
               LogFlowFunc(("Preallocated handle hMem = %p\n", hMem));

               if (hMem)
               {
                   void *pMem = GlobalLock(hMem);
                   LogFlowFunc(("Locked pMem = %p, GlobalSize = %ld\n", pMem, GlobalSize(hMem)));

                   if (pMem)
                   {
                       /* Read the host data to the preallocated buffer. */
                       int vboxrc = VbglR3ClipboardReadData(pCtx->u32ClientID, u32Format, pMem, cbPrealloc, &cb);
                       LogFlowFunc(("VbglR3ClipboardReadData returned with rc = %Rrc\n",  vboxrc));

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
                               LogFlowFunc(("Reallocated hMem = %p\n", hMem));

                               if (hMem)
                               {
                                   pMem = GlobalLock(hMem);
                                   LogFlowFunc(("Locked pMem = %p, GlobalSize = %ld\n", pMem, GlobalSize(hMem)));

                                   if (pMem)
                                   {
                                       /* Read the host data to the preallocated buffer. */
                                       uint32_t cbNew = 0;
                                       vboxrc = VbglR3ClipboardReadData(pCtx->u32ClientID, u32Format, pMem, cb, &cbNew);
                                       LogFlowFunc(("VbglR3ClipboardReadData returned with rc = %Rrc, cb = %d, cbNew = %d\n", vboxrc, cb, cbNew));

                                       if (RT_SUCCESS(vboxrc) && cbNew <= cb)
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
                                   if (FAILED(hrc))
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
                               LogFlowFunc(("Reallocated hMem = %p\n", hMem));

                               if (hMem)
                               {
                                   /* 'hMem' contains the host clipboard data.
                                    * size is 'cb' and format is 'format'. */
                                   HANDLE hClip = SetClipboardData(format, hMem);
                                   LogFlowFunc(("WM_RENDERFORMAT hClip = %p\n", hClip));

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
               VBoxClipboardWinClear();
           }
        }
        break;

        case WM_RENDERALLFORMATS:
        {
           /* Do nothing. The clipboard formats will be unavailable now, because the
            * windows is to be destroyed and therefore the guest side becomes inactive.
            */
           int vboxrc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(vboxrc))
           {
               VBoxClipboardWinClear();
               VBoxClipboardWinClose();
           }
        }
        break;

        case VBOX_CLIPBOARD_WM_SET_FORMATS:
        {
           /* Announce available formats. Do not insert data, they will be inserted in WM_RENDER*. */
           uint32_t u32Formats = (uint32_t)lParam;

           LogFlowFunc(("VBOX_WM_SHCLPB_SET_FORMATS: u32Formats=0x%x\n", u32Formats));

           int vboxrc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(vboxrc))
           {
               VBoxClipboardWinClear();

               HANDLE hClip = NULL;

               if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                   hClip = SetClipboardData(CF_UNICODETEXT, NULL);

               if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
                   hClip = SetClipboardData(CF_DIB, NULL);

               if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_HTML)
               {
                   UINT format = RegisterClipboardFormat("HTML Format");
                   if (format != 0)
                       hClip = SetClipboardData(format, NULL);
               }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
               if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
                   hClip = SetClipboardData(CF_HDROP, NULL);
#endif
               VBoxClipboardWinClose();

               LogFlowFunc(("VBOX_WM_SHCLPB_SET_FORMATS: hClip=%p, lastErr=%ld\n", hClip, GetLastError()));
           }
        }
        break;

        case VBOX_CLIPBOARD_WM_READ_DATA:
        {
           /* Send data in the specified format to the host. */
           uint32_t u32Formats = (uint32_t)lParam;
           HANDLE hClip = NULL;

           LogFlowFunc(("VBOX_WM_SHCLPB_READ_DATA: u32Formats=0x%x\n", u32Formats));

           int vboxrc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(vboxrc))
           {
               if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
               {
                   hClip = GetClipboardData(CF_DIB);

                   if (hClip != NULL)
                   {
                       LPVOID lp = GlobalLock(hClip);
                       if (lp != NULL)
                       {
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
                   UINT format = RegisterClipboardFormat("HTML Format");
                   if (format != 0)
                   {
                       hClip = GetClipboardData(format);
                       if (hClip != NULL)
                       {
                           LPVOID lp = GlobalLock(hClip);

                           if (lp != NULL)
                           {
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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
               else if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
               {
                   hClip = GetClipboardData(CF_HDROP);
                   if (hClip)
                   {
                       HDROP hDrop = (HDROP)GlobalLock(hClip);
                       if (hDrop)
                       {
                           vboxrc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_URI_LIST,
                                                             );
                           GlobalUnlock(hClip);
                       }
                       else
                       {
                           hClip = NULL;
                       }
                   }
               }
#endif
               VBoxClipboardWinClose();
           }

           if (hClip == NULL)
           {
               /* Requested clipboard format is not available, send empty data. */
               VbglR3ClipboardWriteData(pCtx->u32ClientID, 0, NULL, 0);
           }
        }
        break;

        case WM_DESTROY:
        {
           VBoxClipboardWinRemoveFromCBChain(pWinCtx);
           if (pWinCtx->oldAPI.timerRefresh)
               KillTimer(pWinCtx->hWnd, 0);
           /*
            * don't need to call PostQuitMessage cause
            * the VBoxTray already finished a message loop
            */
        }
        break;

        default:
        {
           rc = DefWindowProc(hwnd, msg, wParam, lParam);
        }
        break;
    }

#ifndef DEBUG_andy
    LogFlowFunc(("vboxClipboardProcessMsg returned with rc = %ld\n", rc));
#endif
    return rc;
}

static LRESULT CALLBACK vboxClipboardWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int vboxClipboardCreateWindow(PVBOXCLIPBOARDCONTEXT pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    AssertPtr(pCtx->pEnv);
    HINSTANCE hInstance = pCtx->pEnv->hInstance;
    Assert(hInstance != 0);

    /* Register the Window Class. */
    WNDCLASSEX wc = { 0 };
    wc.cbSize     = sizeof(WNDCLASSEX);

    if (!GetClassInfoEx(hInstance, s_szClipWndClassName, &wc))
    {
        wc.style         = CS_NOCLOSE;
        wc.lpfnWndProc   = vboxClipboardWndProc;
        wc.hInstance     = pCtx->pEnv->hInstance;
        wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
        wc.lpszClassName = s_szClipWndClassName;

        ATOM wndClass = RegisterClassEx(&wc);
        if (wndClass == 0)
            rc = RTErrConvertFromWin32(GetLastError());
    }

    if (RT_SUCCESS(rc))
    {
        const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

        /* Create the window. */
        pWinCtx->hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                       s_szClipWndClassName, s_szClipWndClassName,
                                       WS_POPUPWINDOW,
                                       -200, -200, 100, 100, NULL, NULL, hInstance, NULL);
        if (pWinCtx->hWnd == NULL)
        {
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            SetWindowPos(pWinCtx->hWnd, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

            VBoxClipboardWinAddToCBChain(pWinCtx);
            if (!VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
                pWinCtx->oldAPI.timerRefresh = SetTimer(pWinCtx->hWnd, 0, 10 * 1000 /* 10s */, NULL);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static void vboxClipboardDestroy(PVBOXCLIPBOARDCONTEXT pCtx)
{
    AssertPtrReturnVoid(pCtx);

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

    if (pWinCtx->hWnd)
    {
        DestroyWindow(pWinCtx->hWnd);
        pWinCtx->hWnd = NULL;
    }

    UnregisterClass(s_szClipWndClassName, pCtx->pEnv->hInstance);
}

static LRESULT CALLBACK vboxClipboardWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PVBOXCLIPBOARDCONTEXT pCtx = &g_Ctx; /** @todo r=andy Make pCtx available through SetWindowLongPtr() / GWL_USERDATA. */
    AssertPtr(pCtx);

    /* Forward with proper context. */
    return vboxClipboardProcessMsg(pCtx, hWnd, uMsg, wParam, lParam);
}

DECLCALLBACK(int) VBoxClipboardInit(const PVBOXSERVICEENV pEnv, void **ppInstance)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pCtx = &g_Ctx; /* Only one instance for now. */
    AssertPtr(pCtx);

    if (pCtx->pEnv)
    {
        /* Clipboard was already initialized. 2 or more instances are not supported. */
        return VERR_NOT_SUPPORTED;
    }

    if (VbglR3AutoLogonIsRemoteSession())
    {
        /* Do not use clipboard for remote sessions. */
        LogRel(("Clipboard: Clipboard has been disabled for a remote session\n"));
        return VERR_NOT_SUPPORTED;
    }

    RT_BZERO(pCtx, sizeof(VBOXCLIPBOARDCONTEXT));

    pCtx->pEnv = pEnv;

    /* Check that new Clipboard API is available */
    VBoxClipboardWinCheckAndInitNewAPI(&pCtx->Win.newAPI);

    int rc = VbglR3ClipboardConnect(&pCtx->u32ClientID);
    if (RT_SUCCESS(rc))
    {
        rc = vboxClipboardCreateWindow(pCtx);
        if (RT_SUCCESS(rc))
        {
            *ppInstance = pCtx;
        }
        else
        {
            VbglR3ClipboardDisconnect(pCtx->u32ClientID);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) VBoxClipboardWorker(void *pInstance, bool volatile *pfShutdown)
{
    AssertPtr(pInstance);
    LogFlowFunc(("pInstance=%p\n", pInstance));

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    const PVBOXCLIPBOARDCONTEXT pCtx = (PVBOXCLIPBOARDCONTEXT)pInstance;
    AssertPtr(pCtx);

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

    int rc;

    /* The thread waits for incoming messages from the host. */
    for (;;)
    {
        uint32_t u32Msg;
        uint32_t u32Formats;
        rc = VbglR3ClipboardGetHostMsg(pCtx->u32ClientID, &u32Msg, &u32Formats);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_INTERRUPTED)
                break;

            LogFlowFunc(("Error getting host message, rc=%Rrc\n", rc));

            if (*pfShutdown)
                break;

            /* Wait a bit before retrying. */
            RTThreadSleep(1000);
            continue;
        }
        else
        {
            LogFlowFunc(("u32Msg=%RU32, u32Formats=0x%x\n", u32Msg, u32Formats));
            switch (u32Msg)
            {
               case VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS:
               {
                   /* The host has announced available clipboard formats.
                    * Forward the information to the window, so it can later
                    * respond to WM_RENDERFORMAT message. */
                   ::PostMessage(pWinCtx->hWnd, VBOX_CLIPBOARD_WM_SET_FORMATS, 0, u32Formats);
               }
                   break;

               case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
               {
                   /* The host needs data in the specified format. */
                   ::PostMessage(pWinCtx->hWnd, VBOX_CLIPBOARD_WM_READ_DATA, 0, u32Formats);
               }
                   break;

               case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
               {
                   /* The host is terminating. */
                   LogRel(("Clipboard: Terminating ...\n"));
                   ASMAtomicXchgBool(pfShutdown, true);
               }
                   break;

               default:
               {
                   LogFlowFunc(("Unsupported message from host, message=%RU32\n", u32Msg));

                   /* Wait a bit before retrying. */
                   RTThreadSleep(1000);
               }
                   break;
            }
        }

        if (*pfShutdown)
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) VBoxClipboardStop(void *pInstance)
{
    AssertPtrReturn(pInstance, VERR_INVALID_POINTER);

    LogFunc(("Stopping pInstance=%p\n", pInstance));

    PVBOXCLIPBOARDCONTEXT pCtx = (PVBOXCLIPBOARDCONTEXT)pInstance;
    AssertPtr(pCtx);

    VbglR3ClipboardDisconnect(pCtx->u32ClientID);
    pCtx->u32ClientID = 0;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

DECLCALLBACK(void) VBoxClipboardDestroy(void *pInstance)
{
    AssertPtrReturnVoid(pInstance);

    PVBOXCLIPBOARDCONTEXT pCtx = (PVBOXCLIPBOARDCONTEXT)pInstance;
    AssertPtr(pCtx);

    /* Make sure that we are disconnected. */
    Assert(pCtx->u32ClientID == 0);

    vboxClipboardDestroy(pCtx);
    RT_BZERO(pCtx, sizeof(VBOXCLIPBOARDCONTEXT));

    return;
}

/**
 * The service description.
 */
VBOXSERVICEDESC g_SvcDescClipboard =
{
    /* pszName. */
    "clipboard",
    /* pszDescription. */
    "Shared Clipboard",
    /* methods */
    VBoxClipboardInit,
    VBoxClipboardWorker,
    VBoxClipboardStop,
    VBoxClipboardDestroy
};

