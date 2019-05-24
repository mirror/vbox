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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif
#include <strsafe.h>

#include <VBox/log.h>


#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/** !!! HACK ALERT !!! Dynamically resolve functions! */
# ifdef _WIN32_IE
#  undef _WIN32_IE
#  define _WIN32_IE 0x0501
# endif
# include <iprt/win/shlobj.h>
# include <iprt/win/shlwapi.h>
#endif


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



static LRESULT vboxClipboardWinProcessMsg(PVBOXCLIPBOARDCONTEXT pCtx, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AssertPtr(pCtx);

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

    LRESULT lresultRc = 0;

    switch (msg)
    {
       case WM_CLIPBOARDUPDATE:
       {
           if (GetClipboardOwner() != hwnd)
           {
               /* Clipboard was updated by another application.
                * Report available formats to the host. */
               VBOXCLIPBOARDFORMATS fFormats;
               int rc = VBoxClipboardWinGetFormats(&pCtx->Win, &fFormats);
               if (RT_SUCCESS(rc))
                   rc = VbglR3ClipboardReportFormats(pCtx->u32ClientID, fFormats);

               LogFunc(("WM_CLIPBOARDUPDATE: rc=%Rrc, fFormats=0x%x\n", rc, fFormats));
           }
           else
               LogFlowFunc(("WM_CLIPBOARDUPDATE: No change (VBoxTray is owner)\n"));
       }
       break;

       case WM_CHANGECBCHAIN:
       {
           if (VBoxClipboardWinIsNewAPI(&pWinCtx->newAPI))
           {
               lresultRc = DefWindowProc(hwnd, msg, wParam, lParam);
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
                   lresultRc = SendMessageTimeout(pWinCtx->hWndNextInChain, WM_CHANGECBCHAIN, wParam, lParam, 0,
                                                  VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS, &dwResult);
                   if (!lresultRc)
                       lresultRc = (LRESULT)dwResult;
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
               VBOXCLIPBOARDFORMATS fFormats;
               int rc = VBoxClipboardWinGetFormats(pWinCtx, &fFormats);
               if (RT_SUCCESS(rc))
                   rc = VbglR3ClipboardReportFormats(pCtx->u32ClientID, fFormats);
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
           LogFunc(("WM_RENDERFORMAT\n"));

           /* Insert the requested clipboard format data into the clipboard. */
           const UINT cfFormat = (UINT)wParam;

           const VBOXCLIPBOARDFORMAT fFormat = VBoxClipboardWinClipboardFormatToVBox(cfFormat);

           LogFunc(("WM_RENDERFORMAT: cfFormat=%u -> fFormat=0x%x\n", cfFormat, fFormat));

           if (fFormat == VBOX_SHARED_CLIPBOARD_FMT_NONE)
           {
               LogFunc(("WM_RENDERFORMAT: Unsupported format requested\n"));
               VBoxClipboardWinClear();
           }
           else
           {
               const uint32_t cbPrealloc = _4K;
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
                       int rc = VbglR3ClipboardReadData(pCtx->u32ClientID, fFormat, pMem, cbPrealloc, &cb);
                       LogFlowFunc(("VbglR3ClipboardReadData returned with rc = %Rrc\n",  rc));

                       if (RT_SUCCESS(rc))
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
                                       rc = VbglR3ClipboardReadData(pCtx->u32ClientID, fFormat, pMem, cb, &cbNew);
                                       LogFlowFunc(("VbglR3ClipboardReadData returned with rc = %Rrc, cb = %d, cbNew = %d\n",
                                                    rc, cb, cbNew));

                                       if (RT_SUCCESS(rc)
                                           && cbNew <= cb)
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
                               if (fFormat == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
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
                                   HANDLE hClip = SetClipboardData(cfFormat, hMem);
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
           }
       }
       break;

       case WM_RENDERALLFORMATS:
       {
           /* Do nothing. The clipboard formats will be unavailable now, because the
           * windows is to be destroyed and therefore the guest side becomes inactive.
           */
           int rc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(rc))
           {
               VBoxClipboardWinClear();
               VBoxClipboardWinClose();
           }
       }
       break;

       case VBOX_CLIPBOARD_WM_SET_FORMATS:
       {
           /* Announce available formats. Do not insert data, they will be inserted in WM_RENDER*. */
           VBOXCLIPBOARDFORMATS fFormats = (uint32_t)lParam;

           LogFunc(("VBOX_WM_SHCLPB_SET_FORMATS: fFormats=0x%x\n", fFormats));

           int rc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(rc))
           {
               VBoxClipboardWinClear();

               HANDLE hClip    = NULL;
               UINT   cfFormat = 0;

               /** @todo r=andy Only one clipboard format can be set at once, at least on Windows. */

               if (fFormats & VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
               {
                   LogFunc(("VBOX_WM_SHCLPB_SET_FORMATS: CF_UNICODETEXT\n"));
                   hClip = SetClipboardData(CF_UNICODETEXT, NULL);
               }
               else if (fFormats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
               {
                   LogFunc(("VBOX_WM_SHCLPB_SET_FORMATS: CF_DIB\n"));
                   hClip = SetClipboardData(CF_DIB, NULL);
               }
               else if (fFormats & VBOX_SHARED_CLIPBOARD_FMT_HTML)
               {
                   LogFunc(("VBOX_WM_SHCLPB_SET_FORMATS: VBOX_CLIPBOARD_WIN_REGFMT_HTML\n"));
                   cfFormat = RegisterClipboardFormat(VBOX_CLIPBOARD_WIN_REGFMT_HTML);
                   if (cfFormat != 0)
                       hClip = SetClipboardData(cfFormat, NULL);
               }
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
               else if (fFormats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
               {
                   LogFunc(("VBOX_WM_SHCLPB_SET_FORMATS: VBOX_CLIPBOARD_WIN_REGFMT_URI_LIST cTransfers=%RU32\n",
                            pWinCtx->URI.cTransfers));
                   if (pWinCtx->URI.cTransfers == 0) /* Only allow one transfer at a time for now. */
                   {
                       pWinCtx->URI.Transfer.pDataObj = new VBoxClipboardWinDataObject(&pWinCtx->URI.Transfer.Provider);
                       if (pWinCtx->URI.Transfer.pDataObj)
                       {
                           rc = pWinCtx->URI.Transfer.pDataObj->Init();
                           if (RT_SUCCESS(rc))
                           {
                               VBoxClipboardWinClose();
                               /* Note: Clipboard must be closed first before calling OleSetClipboard(). */

                               /** @todo There is a potential race between VBoxClipboardWinClose() and OleSetClipboard(),
                                *        where another application could own the clipboard (open), and thus the call to
                                *        OleSetClipboard() will fail. Needs fixing. */

                               HRESULT hr = OleSetClipboard(pWinCtx->URI.Transfer.pDataObj);
                               if (SUCCEEDED(hr))
                               {
                                   pWinCtx->URI.cTransfers++;
                               }
                               else
                                   LogRel(("Clipboard: Failed with %Rhrc when setting data object to clipboard\n", hr));
                           }
                       }
                   }
               }
#endif
               else
                   LogRel(("Clipboard: Unsupported format(s) (0x%x), skipping\n", fFormats));

               /** @todo Implement more flexible clipboard precedence for supported formats. */

               VBoxClipboardWinClose();
               /* Note: Clipboard must be closed first before calling OleSetClipboard(). */

               LogFunc(("VBOX_WM_SHCLPB_SET_FORMATS: cfFormat=%u, lastErr=%ld\n", cfFormat, GetLastError()));
           }
       }
       break;

       case VBOX_CLIPBOARD_WM_READ_DATA:
       {
           /* Send data in the specified format to the host. */
           VBOXCLIPBOARDFORMAT uFormat = (uint32_t)lParam;
           HANDLE hClip = NULL;

           LogFlowFunc(("VBOX_WM_SHCLPB_READ_DATA: uFormat=0x%x\n", uFormat));

           int rc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(rc))
           {
               if (uFormat == VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
               {
                   hClip = GetClipboardData(CF_DIB);
                   if (hClip != NULL)
                   {
                       LPVOID lp = GlobalLock(hClip);
                       if (lp != NULL)
                       {
                           rc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_BITMAP,
                                                         lp, GlobalSize(hClip));
                           GlobalUnlock(hClip);
                       }
                       else
                       {
                           hClip = NULL;
                       }
                   }
               }
               else if (uFormat == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
               {
                   hClip = GetClipboardData(CF_UNICODETEXT);
                   if (hClip != NULL)
                   {
                       LPWSTR uniString = (LPWSTR)GlobalLock(hClip);
                       if (uniString != NULL)
                       {
                           rc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                                         uniString, (lstrlenW(uniString) + 1) * 2);
                           GlobalUnlock(hClip);
                       }
                       else
                       {
                           hClip = NULL;
                       }
                   }
               }
               else if (uFormat == VBOX_SHARED_CLIPBOARD_FMT_HTML)
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
                               rc = VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_HTML,
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
               else if (uFormat == VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
               {
                   /* The data data in CF_HDROP format, as the files are locally present and don't need to be
                    * presented as a IDataObject or IStream. */
                   hClip = GetClipboardData(CF_HDROP);
                   if (hClip)
                   {
                       HDROP hDrop = (HDROP)GlobalLock(hClip);
                       if (hDrop)
                       {
                           char *pszList;
                           size_t cbList;
                           rc = VBoxClipboardWinDropFilesToStringList((DROPFILES *)hDrop, &pszList, &cbList);
                           if (RT_SUCCESS(rc))
                           {
                               rc = VbglR3ClipboardWriteData(pCtx->u32ClientID, uFormat, pszList, (uint32_t)cbList);
                               RTMemFree(pszList);
                           }

                           GlobalUnlock(hClip);
                       }
                       else
                       {
                           hClip = NULL;
                       }
                   }
               }
#endif
               if (hClip == NULL)
               {
                   LogFunc(("VBOX_WM_SHCLPB_READ_DATA: hClip=NULL, lastError=%ld\n", GetLastError()));

                   /* Requested clipboard format is not available, send empty data. */
                   VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_NONE, NULL, 0);
               }

               VBoxClipboardWinClose();
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
           lresultRc = DefWindowProc(hwnd, msg, wParam, lParam);
       }
       break;
    }

#ifndef DEBUG_andy
    LogFlowFunc(("vboxClipboardProcessMsg returned with lresultRc=%ld\n", lresultRc));
#endif
    return lresultRc;
}

static LRESULT CALLBACK vboxClipboardWinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
        wc.lpfnWndProc   = vboxClipboardWinWndProc;
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

static LRESULT CALLBACK vboxClipboardWinWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PVBOXCLIPBOARDCONTEXT pCtx = &g_Ctx; /** @todo r=andy Make pCtx available through SetWindowLongPtr() / GWL_USERDATA. */
    AssertPtr(pCtx);

    /* Forward with proper context. */
    return vboxClipboardWinProcessMsg(pCtx, hWnd, uMsg, wParam, lParam);
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

    pCtx->pEnv = pEnv;

    if (VbglR3AutoLogonIsRemoteSession())
    {
        /* Do not use clipboard for remote sessions. */
        LogRel(("Clipboard: Clipboard has been disabled for a remote session\n"));
        return VERR_NOT_SUPPORTED;
    }

    int rc;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
        LogRel(("Clipboard: Initializing OLE failed (%Rhrc) -- file transfers unavailable\n"));
        /* Not critical, the rest of the clipboard might work. */
    }
    else
        LogRel(("Clipboard: Initialized OLE\n"));

    rc = pCtx->Win.URI.Transfer.Provider.SetSource(SharedClipboardProvider::SourceType_VbglR3);
    AssertRC(rc);
#endif

    /* Check that new Clipboard API is available */
    VBoxClipboardWinCheckAndInitNewAPI(&pCtx->Win.newAPI);

    rc = VbglR3ClipboardConnect(&pCtx->u32ClientID);
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

            LogFunc(("Error getting host message, rc=%Rrc\n", rc));

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
               case VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS:
               {
                   /* The host has announced available clipboard formats.
                    * Forward the information to the window, so it can later
                    * respond to WM_RENDERFORMAT message. */
                   ::PostMessage(pWinCtx->hWnd, VBOX_CLIPBOARD_WM_SET_FORMATS, 0, u32Formats);
                   break;
               }

               case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
               {
                   /* The host needs data in the specified format. */
                   ::PostMessage(pWinCtx->hWnd, VBOX_CLIPBOARD_WM_READ_DATA, 0, u32Formats);
                   break;
               }

               case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
               {
                   /* The host is terminating. */
                   LogRel(("Clipboard: Terminating ...\n"));
                   ASMAtomicXchgBool(pfShutdown, true);
                   break;
               }

               default:
               {
                   LogFlowFunc(("Unsupported message from host, message=%RU32\n", u32Msg));

                   /* Wait a bit before retrying. */
                   RTThreadSleep(1000);
                   break;
               }
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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST_ASF
    OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
    OleUninitialize();
#endif

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

