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
#include <VBox/log.h>

#include "VBoxTray.h"
#include "VBoxHelpers.h"

#include <iprt/asm.h>
#include <iprt/errcore.h>
#include <iprt/mem.h>
#include <iprt/ldr.h>


#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-win.h>
#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/HostServices/VBoxClipboardSvc.h> /* Temp, remove. */
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include <VBox/GuestHost/SharedClipboard-transfers.h>
#endif

#include <strsafe.h>

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
# include <iprt/win/shlobj.h>
# include <iprt/win/shlwapi.h>
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

typedef struct _SHCLCONTEXT
{
    /** Pointer to the VBoxClient service environment. */
    const VBOXSERVICEENV *pEnv;
    /** Command context. */
    VBGLR3SHCLCMDCTX      CmdCtx;
    /** Windows-specific context data. */
    SHCLWINCTX            Win;
    /** Thread handle for window thread. */
    RTTHREAD              hThread;
    /** Start indicator flag. */
    bool                  fStarted;
    /** Shutdown indicator flag. */
    bool                  fShutdown;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /** Associated transfer data. */
    SHCLTRANSFERCTX       TransferCtx;
#endif
} SHCLCONTEXT, *PSHCLCONTEXT;


/*********************************************************************************************************************************
*   Static variables                                                                                                             *
*********************************************************************************************************************************/
/** Static clipboard context (since it is the single instance). Directly used in the windows proc. */
static SHCLCONTEXT g_Ctx = { NULL };
/** Static window class name. */
static char s_szClipWndClassName[] = SHCL_WIN_WNDCLASS_NAME;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
static DECLCALLBACK(int)  vboxClipboardOnTransferInitCallback(PSHCLTRANSFERCALLBACKDATA pData);
static DECLCALLBACK(int)  vboxClipboardOnTransferStartCallback(PSHCLTRANSFERCALLBACKDATA pData);
static DECLCALLBACK(void) vboxClipboardOnTransferCompleteCallback(PSHCLTRANSFERCALLBACKDATA pData, int rc);
static DECLCALLBACK(void) vboxClipboardOnTransferErrorCallback(PSHCLTRANSFERCALLBACKDATA pData, int rc);
#endif


#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/**
 * Cleanup helper function for transfer callbacks.
 *
 * @param   pData               Callback data to cleanup.
 */
static void vboxClipboardTransferCallbackCleanup(PSHCLTRANSFERCALLBACKDATA pData)
{
    LogFlowFuncEnter();

    PSHCLTRANSFERCTX pCtx = (PSHCLTRANSFERCTX)pData->pvUser;
    AssertPtr(pCtx);

    PSHCLTRANSFER pTransfer = pData->pTransfer;
    AssertPtr(pTransfer);

    if (pTransfer->pvUser) /* SharedClipboardWinTransferCtx */
    {
        delete pTransfer->pvUser;
        pTransfer->pvUser = NULL;
    }

    int rc2 = SharedClipboardTransferCtxTransferUnregister(pCtx, pTransfer->State.uID);
    AssertRC(rc2);

    SharedClipboardTransferDestroy(pTransfer);

    RTMemFree(pTransfer);
    pTransfer = NULL;
}

static DECLCALLBACK(int)  vboxClipboardOnTransferInitCallback(PSHCLTRANSFERCALLBACKDATA pData)
{
    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pData->pvUser;
    AssertPtr(pCtx);

    LogFlowFunc(("pCtx=%p\n", pCtx));

    RT_NOREF(pData, pCtx);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vboxClipboardOnTransferStartCallback(PSHCLTRANSFERCALLBACKDATA pData)
{
    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pData->pvUser;
    AssertPtr(pCtx);
    Assert(pData->cbUser == sizeof(SHCLCONTEXT));

    PSHCLTRANSFER pTransfer = pData->pTransfer;
    AssertPtr(pTransfer);

    const SHCLTRANSFERDIR enmDir = SharedClipboardTransferGetDir(pTransfer);

    LogFlowFunc(("pCtx=%p, idTransfer=%RU16, enmDir=%RU32\n", pCtx, SharedClipboardTransferGetID(pTransfer), enmDir));

    int rc;

    /* The guest wants to write local data to the host? */
    if (enmDir == SHCLTRANSFERDIR_WRITE)
    {
        Assert(SharedClipboardTransferGetSource(pTransfer) == SHCLSOURCE_LOCAL); /* Sanity. */

        rc = SharedClipboardWinOpen(pCtx->Win.hWnd);
        if (RT_SUCCESS(rc))
        {
            /* The data data in CF_HDROP format, as the files are locally present and don't need to be
             * presented as a IDataObject or IStream. */
            HANDLE hClip = hClip = GetClipboardData(CF_HDROP);
            if (hClip)
            {
                HDROP hDrop = (HDROP)GlobalLock(hClip);
                if (hDrop)
                {
                    char    *papszList = NULL;
                    uint32_t cbList;
                    rc = SharedClipboardWinDropFilesToStringList((DROPFILES *)hDrop, &papszList, &cbList);

                    GlobalUnlock(hClip);

                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardTransferRootsSet(pTransfer,
                                                             papszList, cbList + 1 /* Include termination */);
                        RTStrFree(papszList);
                    }
                }
                else
                    LogRel(("Shared Clipboard: Unable to lock clipboard data, last error: %ld\n", GetLastError()));
            }
            else
                LogRel(("Shared Clipboard: Unable to retrieve clipboard data from clipboard (CF_HDROP), last error: %ld\n",
                        GetLastError()));

            SharedClipboardWinClose();
        }
    }
    /* The guest wants to read data from a remote source. */
    else if (enmDir == SHCLTRANSFERDIR_READ)
    {
        Assert(SharedClipboardTransferGetSource(pTransfer) == SHCLSOURCE_REMOTE); /* Sanity. */

        rc = SharedClipboardWinTransferCreate(&pCtx->Win, pTransfer);
    }
    else
        AssertFailedStmt(rc = VERR_NOT_SUPPORTED);

    LogFlowFunc(("LEAVE: idTransfer=%RU16, rc=%Rrc\n", SharedClipboardTransferGetID(pTransfer), rc));
    return rc;
}

static DECLCALLBACK(void) vboxClipboardOnTransferCompleteCallback(PSHCLTRANSFERCALLBACKDATA pData, int rc)
{
    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pData->pvUser;
    AssertPtr(pCtx);

    RT_NOREF(pCtx, rc);

    LogFlowFunc(("pCtx=%p, rc=%Rrc\n", pCtx, rc));

    LogRel2(("Shared Clipboard: Transfer to destination complete\n"));

    vboxClipboardTransferCallbackCleanup(pData);
}

static DECLCALLBACK(void) vboxClipboardOnTransferErrorCallback(PSHCLTRANSFERCALLBACKDATA pData, int rc)
{
    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pData->pvUser;
    AssertPtr(pCtx);

    RT_NOREF(pCtx, rc);

    LogFlowFunc(("pCtx=%p, rc=%Rrc\n", pCtx, rc));

    LogRel(("Shared Clipboard: Transfer to destination failed with %Rrc\n", rc));

    vboxClipboardTransferCallbackCleanup(pData);
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

static LRESULT vboxClipboardWinProcessMsg(PSHCLCONTEXT pCtx, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AssertPtr(pCtx);

    const PSHCLWINCTX pWinCtx = &pCtx->Win;

    LRESULT lresultRc = 0;

    switch (msg)
    {
        case WM_CLIPBOARDUPDATE:
        {
            LogFunc(("WM_CLIPBOARDUPDATE: pWinCtx=%p\n", pWinCtx));

            if (pCtx->fShutdown) /* If we're about to shut down, skip handling stuff here. */
                break;

            int rc = RTCritSectEnter(&pWinCtx->CritSect);
            if (RT_SUCCESS(rc))
            {
                const HWND hWndClipboardOwner = GetClipboardOwner();

                LogFunc(("WM_CLIPBOARDUPDATE: hWndOldClipboardOwner=%p, hWndNewClipboardOwner=%p\n",
                         pWinCtx->hWndClipboardOwnerUs, hWndClipboardOwner));

                if (pWinCtx->hWndClipboardOwnerUs != hWndClipboardOwner)
                {
                    int rc2 = RTCritSectLeave(&pWinCtx->CritSect);
                    AssertRC(rc2);

                    /* Clipboard was updated by another application.
                     * Report available formats to the host. */
                    SHCLFORMATDATA Formats;
                    int rc = SharedClipboardWinGetFormats(pWinCtx, &Formats);
                    if (RT_SUCCESS(rc))
                    {
                        LogFunc(("WM_CLIPBOARDUPDATE: Reporting formats 0x%x\n", Formats.uFormats));
                        rc = VbglR3ClipboardFormatsReportEx(&pCtx->CmdCtx, &Formats);
                    }
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
            lresultRc = SharedClipboardWinHandleWMChangeCBChain(pWinCtx, hwnd, msg, wParam, lParam);
            break;
        }

        case WM_DRAWCLIPBOARD:
        {
            LogFlowFunc(("WM_DRAWCLIPBOARD: pWinCtx=%p\n", pWinCtx));

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

                    /* Clipboard was updated by another application. */
                    /* WM_DRAWCLIPBOARD always expects a return code of 0, so don't change "rc" here. */
                    SHCLFORMATDATA Formats;
                    rc = SharedClipboardWinGetFormats(pWinCtx, &Formats);
                    if (RT_SUCCESS(rc)
                        && Formats.uFormats != VBOX_SHCL_FMT_NONE)
                    {
                        rc = VbglR3ClipboardFormatsReportEx(&pCtx->CmdCtx, &Formats);
                    }
                }
                else
                {
                    int rc2 = RTCritSectLeave(&pWinCtx->CritSect);
                    AssertRC(rc2);
                }
            }

            lresultRc = SharedClipboardWinChainPassToNext(pWinCtx, msg, wParam, lParam);
            break;
        }

        case WM_TIMER:
        {
            int rc = SharedClipboardWinHandleWMTimer(pWinCtx);
            AssertRC(rc);

            break;
        }

        case WM_CLOSE:
        {
            /* Do nothing. Ignore the message. */
            break;
        }

        case WM_RENDERFORMAT:
        {
            LogFunc(("WM_RENDERFORMAT\n"));

            /* Insert the requested clipboard format data into the clipboard. */
            const UINT cfFormat = (UINT)wParam;

            const SHCLFORMAT fFormat = SharedClipboardWinClipboardFormatToVBox(cfFormat);

            LogFunc(("WM_RENDERFORMAT: cfFormat=%u -> fFormat=0x%x\n", cfFormat, fFormat));

            if (fFormat == VBOX_SHCL_FMT_NONE)
            {
                LogFunc(("WM_RENDERFORMAT: Unsupported format requested\n"));
                SharedClipboardWinClear();
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
                        int rc = VbglR3ClipboardReadData(pCtx->CmdCtx.uClientID, fFormat, pMem, cbPrealloc, &cb);
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
                                        rc = VbglR3ClipboardReadData(pCtx->CmdCtx.uClientID, fFormat, pMem, cb, &cbNew);
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
                                if (fFormat == VBOX_SHCL_FMT_UNICODETEXT)
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

            break;
        }

        case WM_RENDERALLFORMATS:
        {
            LogFunc(("WM_RENDERALLFORMATS\n"));

            int rc = SharedClipboardWinHandleWMRenderAllFormats(pWinCtx, hwnd);
            AssertRC(rc);

            break;
        }

        case SHCL_WIN_WM_REPORT_FORMATS:
        {
            LogFunc(("SHCL_WIN_WM_REPORT_FORMATS\n"));

            /* Announce available formats. Do not insert data -- will be inserted in WM_RENDERFORMAT. */
            PVBGLR3CLIPBOARDEVENT pEvent = (PVBGLR3CLIPBOARDEVENT)lParam;
            AssertPtr(pEvent);
            Assert(pEvent->enmType == VBGLR3CLIPBOARDEVENTTYPE_REPORT_FORMATS);

            const SHCLFORMATS fFormats = pEvent->u.ReportedFormats.uFormats;

            if (fFormats != VBOX_SHCL_FMT_NONE) /* Could arrive with some older GA versions. */
            {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                if (fFormats & VBOX_SHCL_FMT_URI_LIST)
                {
                    LogFunc(("VBOX_SHCL_FMT_URI_LIST\n"));

                    /*
                     * Creating and starting the actual transfer will be done in vbglR3ClipboardTransferStart() as soon
                     * as the host announces the start of the transfer via a VBOX_SHCL_HOST_MSG_TRANSFER_STATUS message.
                     * Transfers always are controlled and initiated on the host side!
                     *
                     * So don't announce the transfer to the OS here yet. Don't touch the clipboard in any here; otherwise
                     * this will trigger a WM_DRAWCLIPBOARD or friends, which will result in fun bugs coming up.
                     */
                }
                else
                {
#endif
                    int rc = SharedClipboardWinOpen(hwnd);
                    if (RT_SUCCESS(rc))
                    {
                        SharedClipboardWinClear();

                        rc = SharedClipboardWinAnnounceFormats(pWinCtx, fFormats);
                    }

                    SharedClipboardWinClose();

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                }
#endif
            }

            LogFunc(("SHCL_WIN_WM_REPORT_FORMATS: fFormats=0x%x, lastErr=%ld\n", fFormats, GetLastError()));
            break;
        }

        case SHCL_WIN_WM_READ_DATA:
        {
            /* Send data in the specified format to the host. */
            PVBGLR3CLIPBOARDEVENT pEvent = (PVBGLR3CLIPBOARDEVENT)lParam;
            AssertPtr(pEvent);
            Assert(pEvent->enmType == VBGLR3CLIPBOARDEVENTTYPE_READ_DATA);

            const SHCLFORMAT uFormat = (uint32_t)pEvent->u.ReadData.uFmt;

            HANDLE hClip = NULL;

            LogFlowFunc(("SHCL_WIN_WM_READ_DATA: uFormat=0x%x\n", uFormat));

            int rc = SharedClipboardWinOpen(hwnd);
            if (RT_SUCCESS(rc))
            {
                if (uFormat == VBOX_SHCL_FMT_BITMAP)
                {
                    hClip = GetClipboardData(CF_DIB);
                    if (hClip != NULL)
                    {
                        LPVOID lp = GlobalLock(hClip);
                        if (lp != NULL)
                        {
                            SHCLDATABLOCK dataBlock = { uFormat, lp, (uint32_t)GlobalSize(hClip) };

                            rc = VbglR3ClipboardWriteDataEx(&pEvent->cmdCtx, &dataBlock);

                            GlobalUnlock(hClip);
                        }
                        else
                        {
                            hClip = NULL;
                        }
                    }
                }
                else if (uFormat == VBOX_SHCL_FMT_UNICODETEXT)
                {
                    hClip = GetClipboardData(CF_UNICODETEXT);
                    if (hClip != NULL)
                    {
                        LPWSTR uniString = (LPWSTR)GlobalLock(hClip);
                        if (uniString != NULL)
                        {
                            SHCLDATABLOCK dataBlock = { uFormat, uniString, ((uint32_t)lstrlenW(uniString) + 1) * 2 };

                            rc = VbglR3ClipboardWriteDataEx(&pEvent->cmdCtx, &dataBlock);

                            GlobalUnlock(hClip);
                        }
                        else
                        {
                            hClip = NULL;
                        }
                    }
                }
                else if (uFormat == VBOX_SHCL_FMT_HTML)
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
                                SHCLDATABLOCK dataBlock = { uFormat, lp, (uint32_t)GlobalSize(hClip) };

                                rc = VbglR3ClipboardWriteDataEx(&pEvent->cmdCtx, &dataBlock);

                                GlobalUnlock(hClip);
                            }
                            else
                            {
                                hClip = NULL;
                            }
                        }
                    }
                }
                if (hClip == NULL)
                {
                    LogFunc(("SHCL_WIN_WM_READ_DATA: hClip=NULL, lastError=%ld\n", GetLastError()));

                    /* Requested clipboard format is not available, send empty data. */
                    VbglR3ClipboardWriteData(pCtx->CmdCtx.uClientID, VBOX_SHCL_FMT_NONE, NULL, 0);
#ifdef DEBUG_andy
                    AssertFailed();
#endif
                }

                SharedClipboardWinClose();
            }
            break;
        }

#if 0
        case SHCL_WIN_WM_TRANSFER_START:
        {
            LogFunc(("SHCL_WIN_WM_TRANSFER_START\n"));

            PSHCLTRANSFER pTransfer = (PSHCLTRANSFER)lParam;
            AssertPtr(pTransfer);

            Assert(SharedClipboardTransferGetSource(pTransfer) == SHCLSOURCE_REMOTE); /* Sanity. */

            int rc2 = SharedClipboardWinTransferCreate(pWinCtx, pTransfer);
            AssertRC(rc2);
            break;
        }
#endif

        case WM_DESTROY:
        {
            LogFunc(("WM_DESTROY\n"));

            int rc = SharedClipboardWinHandleWMDestroy(pWinCtx);
            AssertRC(rc);

            /*
             * Don't need to call PostQuitMessage cause
             * the VBoxTray already finished a message loop.
             */

            break;
        }

        default:
        {
            LogFunc(("WM_ %p\n", msg));
            lresultRc = DefWindowProc(hwnd, msg, wParam, lParam);
            break;
        }
    }

    LogFunc(("WM_ rc %d\n", lresultRc));
    return lresultRc;
}

static LRESULT CALLBACK vboxClipboardWinWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int vboxClipboardCreateWindow(PSHCLCONTEXT pCtx)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    AssertPtr(pCtx->pEnv);
    HINSTANCE hInstance = pCtx->pEnv->hInstance;
    Assert(hInstance != 0);

    /* Register the Window Class. */
    WNDCLASSEX wc;
    RT_ZERO(wc);

    wc.cbSize = sizeof(WNDCLASSEX);

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
        const PSHCLWINCTX pWinCtx = &pCtx->Win;

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

            rc = SharedClipboardWinChainAdd(pWinCtx);
            if (RT_SUCCESS(rc))
            {
                if (!SharedClipboardWinIsNewAPI(&pWinCtx->newAPI))
                    pWinCtx->oldAPI.timerRefresh = SetTimer(pWinCtx->hWnd, 0, 10 * 1000 /* 10s */, NULL);
            }
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardWindowThread(RTTHREAD hThread, void *pvUser)
{
    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pvUser;
    AssertPtr(pCtx);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
        LogRel(("Shared Clipboard: Initializing OLE in window thread failed (%Rhrc) -- file transfers unavailable\n", hr));
        /* Not critical, the rest of the clipboard might work. */
    }
    else
        LogRel(("Shared Clipboard: Initialized OLE in window thread\n"));
#endif

    int rc = vboxClipboardCreateWindow(pCtx);
    if (RT_FAILURE(rc))
    {
        LogRel(("Shared Clipboard: Unable to create window, rc=%Rrc\n", rc));
        return rc;
    }

    pCtx->fStarted = true; /* Set started indicator. */

    int rc2 = RTThreadUserSignal(hThread);
    bool fSignalled = RT_SUCCESS(rc2);

    LogRel2(("Shared Clipboard: Window thread running\n"));

    if (RT_SUCCESS(rc))
    {
        for (;;)
        {
            MSG uMsg;
            BOOL fRet;
            while ((fRet = GetMessage(&uMsg, 0, 0, 0)) > 0)
            {
                TranslateMessage(&uMsg);
                DispatchMessage(&uMsg);
            }
            Assert(fRet >= 0);

            if (ASMAtomicReadBool(&pCtx->fShutdown))
                break;

            /** @todo Immediately drop on failure? */
        }
    }

    if (!fSignalled)
    {
        rc2 = RTThreadUserSignal(hThread);
        AssertRC(rc2);
    }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
    OleUninitialize();
#endif

    LogRel(("Shared Clipboard: Window thread ended\n"));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static void vboxClipboardDestroy(PSHCLCONTEXT pCtx)
{
    AssertPtrReturnVoid(pCtx);

    LogFlowFunc(("pCtx=%p\n", pCtx));

    LogRel2(("Shared Clipboard: Destroying ...\n"));

    const PSHCLWINCTX pWinCtx = &pCtx->Win;

    if (pCtx->hThread != NIL_RTTHREAD)
    {
        int rcThread = VERR_WRONG_ORDER;
        int rc = RTThreadWait(pCtx->hThread, 60 * 1000 /* Timeout in ms */, &rcThread);
        LogFlowFunc(("Waiting for thread resulted in %Rrc (thread exited with %Rrc)\n",
                     rc, rcThread));
        RT_NOREF(rc);
    }

    if (pWinCtx->hWnd)
    {
        DestroyWindow(pWinCtx->hWnd);
        pWinCtx->hWnd = NULL;
    }

    UnregisterClass(s_szClipWndClassName, pCtx->pEnv->hInstance);

    SharedClipboardWinCtxDestroy(&pCtx->Win);

    LogRel2(("Shared Clipboard: Destroyed\n"));
}

static LRESULT CALLBACK vboxClipboardWinWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PSHCLCONTEXT pCtx = &g_Ctx; /** @todo r=andy Make pCtx available through SetWindowLongPtr() / GWL_USERDATA. */
    AssertPtr(pCtx);

    /* Forward with proper context. */
    return vboxClipboardWinProcessMsg(pCtx, hWnd, uMsg, wParam, lParam);
}

DECLCALLBACK(int) VBoxShClInit(const PVBOXSERVICEENV pEnv, void **ppInstance)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pCtx = &g_Ctx; /* Only one instance for now. */
    AssertPtr(pCtx);

    if (pCtx->pEnv)
    {
        /* Clipboard was already initialized. 2 or more instances are not supported. */
        return VERR_NOT_SUPPORTED;
    }

    if (VbglR3AutoLogonIsRemoteSession())
    {
        /* Do not use clipboard for remote sessions. */
        LogRel(("Shared Clipboard: Clipboard has been disabled for a remote session\n"));
        return VERR_NOT_SUPPORTED;
    }

    pCtx->pEnv      = pEnv;
    pCtx->hThread   = NIL_RTTHREAD;
    pCtx->fStarted  = false;
    pCtx->fShutdown = false;

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /* Install callbacks. */
    RT_ZERO(pCtx->CmdCtx.Transfers.Callbacks);

    pCtx->CmdCtx.Transfers.Callbacks.pvUser = pCtx; /* Assign context as user-provided callback data. */
    pCtx->CmdCtx.Transfers.Callbacks.cbUser = sizeof(SHCLCONTEXT);

    pCtx->CmdCtx.Transfers.Callbacks.pfnTransferInitialize = vboxClipboardOnTransferInitCallback;
    pCtx->CmdCtx.Transfers.Callbacks.pfnTransferStart      = vboxClipboardOnTransferStartCallback;
    pCtx->CmdCtx.Transfers.Callbacks.pfnTransferComplete   = vboxClipboardOnTransferCompleteCallback;
    pCtx->CmdCtx.Transfers.Callbacks.pfnTransferError      = vboxClipboardOnTransferErrorCallback;
#endif

    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardWinCtxInit(&pCtx->Win);
        if (RT_SUCCESS(rc))
            rc = VbglR3ClipboardConnectEx(&pCtx->CmdCtx);
        if (RT_SUCCESS(rc))
        {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
            rc = SharedClipboardTransferCtxInit(&pCtx->TransferCtx);
#endif
            if (RT_SUCCESS(rc))
            {
                /* Message pump thread for our proxy window. */
                rc = RTThreadCreate(&pCtx->hThread, vboxClipboardWindowThread, pCtx /* pvUser */,
                                    0, RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                                    "shclwnd");
                if (RT_SUCCESS(rc))
                {
                    int rc2 = RTThreadUserWait(pCtx->hThread, 30 * 1000 /* Timeout in ms */);
                    AssertRC(rc2);

                    if (!pCtx->fStarted) /* Did the thread fail to start? */
                        rc = VERR_NOT_SUPPORTED; /* Report back Shared Clipboard as not being supported. */
                }
            }

            if (RT_SUCCESS(rc))
            {
                *ppInstance = pCtx;
            }
            else
                VbglR3ClipboardDisconnectEx(&pCtx->CmdCtx);
        }
    }

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Unable to initialize, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) VBoxShClWorker(void *pInstance, bool volatile *pfShutdown)
{
    AssertPtr(pInstance);
    LogFlowFunc(("pInstance=%p\n", pInstance));

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    const PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pInstance;
    AssertPtr(pCtx);

    const PSHCLWINCTX pWinCtx = &pCtx->Win;

    LogRel2(("Shared Clipboard: Worker loop running, using protocol v%RU32\n", pCtx->CmdCtx.uProtocolVer));

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
        LogRel(("Shared Clipboard: Initializing OLE in worker thread failed (%Rhrc) -- file transfers unavailable\n", hr));
        /* Not critical, the rest of the clipboard might work. */
    }
    else
        LogRel(("Shared Clipboard: Initialized OLE in worker thraed\n"));
#endif

    int rc;

    LogFlowFunc(("uProtocolVer=%RU32\n", pCtx->CmdCtx.uProtocolVer));

    uint32_t uMsg;
    uint32_t uFormats;

    /* The thread waits for incoming messages from the host. */
    for (;;)
    {
        PVBGLR3CLIPBOARDEVENT pEvent = NULL;

        LogFlowFunc(("Waiting for host message (protocol v%RU32) ...\n", pCtx->CmdCtx.uProtocolVer));

        if (pCtx->CmdCtx.uProtocolVer == 0) /* Legacy protocol */
        {
            rc = VbglR3ClipboardGetHostMsgOld(pCtx->CmdCtx.uClientID, &uMsg, &uFormats);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_INTERRUPTED)
                    break;

                LogFunc(("Error getting host message, rc=%Rrc\n", rc));
            }
            else
            {
                pEvent = (PVBGLR3CLIPBOARDEVENT)RTMemAllocZ(sizeof(VBGLR3CLIPBOARDEVENT));
                AssertPtrBreakStmt(pEvent, rc = VERR_NO_MEMORY);

                switch (uMsg)
                {
                    case VBOX_SHCL_HOST_MSG_FORMATS_REPORT:
                    {
                        pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_REPORT_FORMATS;
                        pEvent->u.ReportedFormats.uFormats = uFormats;
                        break;
                    }

                    case VBOX_SHCL_HOST_MSG_READ_DATA:
                    {
                        pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_READ_DATA;
                        pEvent->u.ReadData.uFmt = uFormats;
                        break;
                    }

                    case VBOX_SHCL_HOST_MSG_QUIT:
                    {
                        pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_QUIT;
                        break;
                    }

                    default:
                        rc = VERR_NOT_SUPPORTED;
                        break;
                }
            }
        }
        else /* Protocol >= v1. */
        {
            pEvent = (PVBGLR3CLIPBOARDEVENT)RTMemAllocZ(sizeof(VBGLR3CLIPBOARDEVENT));
            AssertPtrBreakStmt(pEvent, rc = VERR_NO_MEMORY);

            uint32_t uMsg   = 0;
            uint32_t cParms = 0;
            rc = VbglR3ClipboardMsgPeekWait(&pCtx->CmdCtx, &uMsg, &cParms, NULL /* pidRestoreCheck */);
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                rc = VbglR3ClipboardEventGetNextEx(uMsg, cParms, &pCtx->CmdCtx, &pCtx->TransferCtx, pEvent);
#else
                rc = VbglR3ClipboardEventGetNext(uMsg, cParms, &pCtx->CmdCtx, pEvent);
#endif
            }
        }

        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Getting next event failed with %Rrc\n", rc));

            VbglR3ClipboardEventFree(pEvent);
            pEvent = NULL;

            if (*pfShutdown)
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
                    /* The host has announced available clipboard formats.
                     * Forward the information to the window, so it can later
                     * respond to WM_RENDERFORMAT message. */
                    ::PostMessage(pWinCtx->hWnd, SHCL_WIN_WM_REPORT_FORMATS,
                                  0 /* wParam */, (LPARAM)pEvent /* lParam */);

                    pEvent = NULL; /* Consume pointer. */
                    break;
                }

                case VBGLR3CLIPBOARDEVENTTYPE_READ_DATA:
                {
                    /* The host needs data in the specified format. */
                    ::PostMessage(pWinCtx->hWnd, SHCL_WIN_WM_READ_DATA,
                                  0 /* wParam */, (LPARAM)pEvent /* lParam */);

                    pEvent = NULL; /* Consume pointer. */
                    break;
                }

                case VBGLR3CLIPBOARDEVENTTYPE_QUIT:
                {
                    LogRel2(("Shared Clipboard: Host requested termination\n"));
                    ASMAtomicXchgBool(pfShutdown, true);
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

        if (*pfShutdown)
            break;
    }

    LogRel(("Shared Clipboard: Worker loop ended\n"));

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
    OleUninitialize();
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) VBoxShClStop(void *pInstance)
{
    AssertPtrReturn(pInstance, VERR_INVALID_POINTER);

    LogFunc(("Stopping pInstance=%p\n", pInstance));

    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pInstance;
    AssertPtr(pCtx);

    /* Set shutdown indicator. */
    ASMAtomicWriteBool(&pCtx->fShutdown, true);

    /* Let our clipboard know that we're going to shut down. */
    PostMessage(pCtx->Win.hWnd, WM_QUIT, 0, 0);

    /* Disconnect from the host service.
    /* This will also send a VBOX_SHCL_HOST_MSG_QUIT from the host so that we can break out from our message worker. */
    VbglR3ClipboardDisconnect(pCtx->CmdCtx.uClientID);
    pCtx->CmdCtx.uClientID = 0;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

DECLCALLBACK(void) VBoxShClDestroy(void *pInstance)
{
    AssertPtrReturnVoid(pInstance);

    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pInstance;
    AssertPtr(pCtx);

    /* Make sure that we are disconnected. */
    Assert(pCtx->CmdCtx.uClientID == 0);

    vboxClipboardDestroy(pCtx);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    SharedClipboardTransferCtxDestroy(&pCtx->TransferCtx);
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
    VBoxShClInit,
    VBoxShClWorker,
    VBoxShClStop,
    VBoxShClDestroy
};
