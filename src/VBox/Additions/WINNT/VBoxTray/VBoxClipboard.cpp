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
# include <VBox/GuestHost/SharedClipboard-uri.h>
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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    /** Associated transfer data. */
    SHCLURICTX            URI;
#endif
} SHCLCONTEXT, *PSHCLCONTEXT;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
typedef struct _SHCLURIREADTHREADCTX
{
    PSHCLCONTEXT     pClipboardCtx;
    PSHCLURITRANSFER pTransfer;
} SHCLURIREADTHREADCTX, *PSHCLURIREADTHREADCTX;

typedef struct _SHCLURIWRITETHREADCTX
{
    PSHCLCONTEXT     pClipboardCtx;
    PSHCLURITRANSFER pTransfer;
} SHCLURIWRITETHREADCTX, *PSHCLURIWRITETHREADCTX;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */


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
static DECLCALLBACK(void) vboxClipboardURITransferCompleteCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc);
static DECLCALLBACK(void) vboxClipboardURITransferErrorCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc);
#endif


#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
#if 0
static DECLCALLBACK(int) vboxClipboardURIWriteThread(RTTHREAD ThreadSelf, void *pvUser)
{
    RT_NOREF(ThreadSelf);

    LogFlowFuncEnter();

    PSHCLURIWRITETHREADCTX pCtx = (PSHCLURIWRITETHREADCTX)pvUser;
    AssertPtr(pCtx);

    PSHCLURITRANSFER pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    pTransfer->Thread.fStarted = true;

    RTThreadUserSignal(RTThreadSelf());

    VBGLR3SHCLCMDCTX cmdCtx;
    int rc = VbglR3ClipboardConnectEx(&cmdCtx);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3ClipboardTransferSendStatus(&cmdCtx, pTransfer, SHCLURITRANSFERSTATUS_RUNNING);
        if (RT_SUCCESS(rc))
        {
            bool     fTerminate = false;
            unsigned cErrors    = 0;

            for (;;)
            {
                PVBGLR3CLIPBOARDEVENT pEvent = NULL;
                rc = VbglR3ClipboardEventGetNext(&cmdCtx, &pEvent);
                if (RT_SUCCESS(rc))
                {
                    /* Nothing to do in here right now. */

                    VbglR3ClipboardEventFree(pEvent);
                }

                if (fTerminate)
                    break;

                if (RT_FAILURE(rc))
                {
                    if (cErrors++ >= 3)
                        break;
                    RTThreadSleep(1000);
                }
            }
        }

        VbglR3ClipboardDisconnectEx(&cmdCtx);
    }

    RTMemFree(pCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif

static void vboxClipboardURITransferCallbackCleanup(PSHCLURITRANSFERCALLBACKDATA pData)
{
    PSHCLURICTX pCtx = (PSHCLURICTX)pData->pvUser;
    AssertPtr(pCtx);

    PSHCLURITRANSFER pTransfer = pData->pTransfer;
    AssertPtr(pTransfer);

    if (pTransfer->pvUser) /* SharedClipboardWinURITransferCtx */
    {
        delete pTransfer->pvUser;
        pTransfer->pvUser = NULL;
    }

    int rc2 = SharedClipboardURICtxTransferUnregister(pCtx, pTransfer->State.uID);
    AssertRC(rc2);

    SharedClipboardURITransferDestroy(pTransfer);

    RTMemFree(pTransfer);
    pTransfer = NULL;
}

static DECLCALLBACK(void) vboxClipboardURITransferCompleteCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc)
{
    RT_NOREF(rc);

    LogFlowFunc(("pData=%p, rc=%Rrc\n", pData, rc));

    LogRel2(("Shared Clipboard: Transfer to destination complete\n"));

    vboxClipboardURITransferCallbackCleanup(pData);
}

static DECLCALLBACK(void) vboxClipboardURITransferErrorCallback(PSHCLURITRANSFERCALLBACKDATA pData, int rc)
{
    RT_NOREF(rc);

    LogFlowFunc(("pData=%p, rc=%Rrc\n", pData, rc));

    LogRel(("Shared Clipboard: Transfer to destination failed with %Rrc\n", rc));

    vboxClipboardURITransferCallbackCleanup(pData);
}

static int vboxClipboardURITransferOpen(PSHCLPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

static int vboxClipboardURITransferClose(PSHCLPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

static int vboxClipboardURIListOpen(PSHCLPROVIDERCTX pCtx, PSHCLLISTOPENPARMS pOpenParms,
                                    PSHCLLISTHANDLE phList)
{
    RT_NOREF(pCtx, pOpenParms, phList);

    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    RT_NOREF(pThisCtx);

    int rc = 0; // VbglR3ClipboardRecvListOpen(pThisCtx->u32ClientID, pListHdr, phList);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIListClose(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList)
{
    RT_NOREF(pCtx, hList);

    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    RT_NOREF(pThisCtx);

    int rc = 0; // VbglR3ClipboardRecvListOpen(pThisCtx->u32ClientID, pListHdr, phList);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIGetRoots(PSHCLPROVIDERCTX pCtx, PSHCLROOTLIST *ppRootList)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardRootListRead(&pThisCtx->CmdCtx, ppRootList);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIListHdrRead(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                       PSHCLLISTHDR pListHdr)
{
    RT_NOREF(hList);

    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    RT_NOREF(pThisCtx);

    int rc = SharedClipboardURIListHdrInit(pListHdr);
    if (RT_SUCCESS(rc))
    {
        if (RT_SUCCESS(rc))
        {
            //rc = VbglR3ClipboardListHdrReadRecv(pThisCtx->u32ClientID, hList, pListHdr);
        }
        else
            SharedClipboardURIListHdrDestroy(pListHdr);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
static int vboxClipboardURIListHdrWrite(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                        PSHCLLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardListHdrWrite(pThisCtx->u32ClientID, hList, pListHdr);

    LogFlowFuncLeaveRC(rc);
    return rc;
}*/

static int vboxClipboardURIListEntryRead(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                         PSHCLLISTENTRY pListEntry)
{
    RT_NOREF(hList);

    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    RT_NOREF(pThisCtx);

    RT_NOREF(pListEntry);
    int rc = 0; // VbglR3ClipboardListEntryRead(pThisCtx->u32ClientID, pListEntry);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
static int vboxClipboardURIListEntryWrite(PSHCLPROVIDERCTX pCtx, SHCLLISTHANDLE hList,
                                          PSHCLLISTENTRY pListEntry)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardListEntryWrite(pThisCtx->u32ClientID, hList, pListEntry);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
*/

static int vboxClipboardURIObjOpen(PSHCLPROVIDERCTX pCtx,
                                   PSHCLOBJOPENCREATEPARMS pCreateParms, PSHCLOBJHANDLE phObj)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjOpenSend(&pThisCtx->CmdCtx, pCreateParms, phObj);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIObjClose(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj)
{
    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjCloseSend(&pThisCtx->CmdCtx, hObj);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIObjRead(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                   void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjRead(&pThisCtx->CmdCtx, hObj, pvData, cbData, pcbRead);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIObjWrite(PSHCLPROVIDERCTX pCtx, SHCLOBJHANDLE hObj,
                                    void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    PSHCLCONTEXT pThisCtx = (PSHCLCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjWrite(&pThisCtx->CmdCtx, hObj, pvData, cbData, pcbWritten);

    LogFlowFuncLeaveRC(rc);
    return rc;
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
           const HWND hWndClipboardOwner = GetClipboardOwner();
           if (pWinCtx->hWndClipboardOwnerUs != hWndClipboardOwner)
           {
               LogFunc(("WM_CLIPBOARDUPDATE: hWndOldClipboardOwner=%p, hWndNewClipboardOwner=%p\n",
                        pWinCtx->hWndClipboardOwnerUs, hWndClipboardOwner));

               /* Clipboard was updated by another application.
                * Report available formats to the host. */
               SHCLFORMATDATA Formats;
               int rc = SharedClipboardWinGetFormats(&pCtx->Win, &Formats);
               if (RT_SUCCESS(rc))
               {
                   LogFunc(("WM_CLIPBOARDUPDATE: Reporting formats 0x%x\n", Formats.uFormats));
                   rc = VbglR3ClipboardFormatsSend(&pCtx->CmdCtx, &Formats);
               }
           }

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
           LogFlowFunc(("WM_DRAWCLIPBOARD, hwnd %p\n", pWinCtx->hWnd));

           if (GetClipboardOwner() != hwnd)
           {
               /* Clipboard was updated by another application. */
               /* WM_DRAWCLIPBOARD always expects a return code of 0, so don't change "rc" here. */
               SHCLFORMATDATA Formats;
               int rc = SharedClipboardWinGetFormats(pWinCtx, &Formats);
               if (RT_SUCCESS(rc))
                   rc = VbglR3ClipboardFormatsSend(&pCtx->CmdCtx, &Formats);
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

           const SHCLFORMATS fFormats =  pEvent->u.ReportedFormats.uFormats;

           if (fFormats != VBOX_SHCL_FMT_NONE) /* Could arrive with some older GA versions. */
           {
               int rc = SharedClipboardWinOpen(hwnd);
               if (RT_SUCCESS(rc))
               {
                   SharedClipboardWinClear();

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                    if (fFormats & VBOX_SHCL_FMT_URI_LIST)
                    {
                        LogFunc(("VBOX_SHCL_FMT_URI_LIST\n"));

                        PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(&pCtx->URI, 0); /** @todo FIX !!! */
                        if (pTransfer)
                        {
                            rc = SharedClipboardWinURITransferCreate(pWinCtx, pTransfer);

                            /* Note: The actual requesting + retrieving of data will be done in the IDataObject implementation
                                     (ClipboardDataObjectImpl::GetData()). */
                        }
                        else
                            AssertFailedStmt(rc = VERR_NOT_FOUND);

                        /* Note: SharedClipboardWinURITransferCreate() takes care of closing the clipboard. */
                    }
                    else
                    {
#endif
                        rc = SharedClipboardWinAnnounceFormats(pWinCtx, fFormats);

                        SharedClipboardWinClose();
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                    }
#endif
               }
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
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
               else if (uFormat == VBOX_SHCL_FMT_URI_LIST)
               {
                   LogFunc(("cTransfersRunning=%RU32\n", SharedClipboardURICtxGetRunningTransfers(&pCtx->URI)));

                   int rc = SharedClipboardWinOpen(hwnd);
                   if (RT_SUCCESS(rc))
                   {
                       PSHCLURITRANSFER pTransfer;
                       rc = SharedClipboardURITransferCreate(&pTransfer);
                       if (RT_SUCCESS(rc))
                           rc = SharedClipboardURITransferInit(pTransfer, 0 /* uID */,
                                                               SHCLURITRANSFERDIR_WRITE, SHCLSOURCE_LOCAL);
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
                                   char    *papszList;
                                   uint32_t cbList;
                                   rc = SharedClipboardWinDropFilesToStringList((DROPFILES *)hDrop, &papszList, &cbList);

                                   GlobalUnlock(hClip);

                                   if (RT_SUCCESS(rc))
                                   {
                                       rc = SharedClipboardURILTransferSetRoots(pTransfer,
                                                                                papszList, cbList + 1 /* Include termination */);
                                       if (RT_SUCCESS(rc))
                                       {
                                           PSHCLURIWRITETHREADCTX pThreadCtx
                                               = (PSHCLURIWRITETHREADCTX)RTMemAllocZ(sizeof(SHCLURIWRITETHREADCTX));
                                           if (pThreadCtx)
                                           {
                                               pThreadCtx->pClipboardCtx = pCtx;
                                               pThreadCtx->pTransfer     = pTransfer;

                                               rc = SharedClipboardURITransferPrepare(pTransfer);
                                               if (RT_SUCCESS(rc))
                                               {
                                                   rc = SharedClipboardURICtxTransferRegister(&pCtx->URI, pTransfer,
                                                                                              NULL /* puTransferID */);
                                            #if 0
                                                   if (RT_SUCCESS(rc))
                                                   {
                                                       rc = SharedClipboardURITransferRun(pTransfer, vboxClipboardURIWriteThread,
                                                                                          pThreadCtx /* pvUser */);
                                                       /* pThreadCtx now is owned by vboxClipboardURIWriteThread(). */
                                                   }
                                            #endif
                                               }
                                           }
                                           else
                                               rc = VERR_NO_MEMORY;
                                       }

                                       if (papszList)
                                           RTStrFree(papszList);
                                   }
                               }
                               else
                               {
                                   hClip = NULL;
                               }
                           }
                       }

                       SharedClipboardWinClose();
                   }

                   if (RT_FAILURE(rc))
                       LogFunc(("SHCL_WIN_WM_READ_DATA: Failed with rc=%Rrc\n", rc));
               }
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
       case SHCL_WIN_WM_URI_TRANSFER_STATUS:
       {
           LogFunc(("SHCL_WIN_WM_URI_TRANSFER_STATUS\n"));

           break;
       }
#endif

#if 0
       /* The host wants to read URI data. */
       case VBOX_CLIPBOARD_WM_URI_START_READ:
       {
           LogFunc(("VBOX_CLIPBOARD_WM_URI_START_READ: cTransfersRunning=%RU32\n",
                    SharedClipboardURICtxGetRunningTransfers(&pCtx->URI)));

           int rc = SharedClipboardWinOpen(hwnd);
           if (RT_SUCCESS(rc))
           {
               PSHCLURITRANSFER pTransfer;
               rc = SharedClipboardURITransferCreate(SHCLURITRANSFERDIR_WRITE,
                                                     SHCLSOURCE_LOCAL,
                                                     &pTransfer);
               if (RT_SUCCESS(rc))
               {
                   rc = SharedClipboardURICtxTransferAdd(&pCtx->URI, pTransfer);
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
                               char    *papszList;
                               uint32_t cbList;
                               rc = SharedClipboardWinDropFilesToStringList((DROPFILES *)hDrop, &papszList, &cbList);

                               GlobalUnlock(hClip);

                               if (RT_SUCCESS(rc))
                               {
                                   rc = SharedClipboardURILTransferSetRoots(pTransfer,
                                                                            papszList, cbList + 1 /* Include termination */);
                                   if (RT_SUCCESS(rc))
                                   {
                                       PSHCLURIWRITETHREADCTX pThreadCtx
                                           = (PSHCLURIWRITETHREADCTX)RTMemAllocZ(sizeof(SHCLURIWRITETHREADCTX));
                                       if (pThreadCtx)
                                       {
                                           pThreadCtx->pClipboardCtx = pCtx;
                                           pThreadCtx->pTransfer     = pTransfer;

                                           rc = SharedClipboardURITransferPrepare(pTransfer);
                                           if (RT_SUCCESS(rc))
                                           {
                                               rc = SharedClipboardURITransferRun(pTransfer, vboxClipboardURIWriteThread,
                                                                                  pThreadCtx /* pvUser */);
                                               /* pThreadCtx now is owned by vboxClipboardURIWriteThread(). */
                                           }
                                       }
                                       else
                                           rc = VERR_NO_MEMORY;
                                   }

                                   if (papszList)
                                       RTStrFree(papszList);
                               }
                           }
                           else
                           {
                               hClip = NULL;
                           }
                       }
                   }
               }

               SharedClipboardWinClose();
           }

           if (RT_FAILURE(rc))
               LogFunc(("VBOX_CLIPBOARD_WM_URI_START_READ: Failed with rc=%Rrc\n", rc));
           break;
       }

       /* The host wants to write URI data. */
       case VBOX_CLIPBOARD_WM_URI_START_WRITE:
       {
           LogFunc(("VBOX_CLIPBOARD_WM_URI_START_WRITE: cTransfersRunning=%RU32\n",
                    SharedClipboardURICtxGetRunningTransfers(&pCtx->URI)));

           PSHCLURITRANSFER pTransfer;
           int rc = SharedClipboardURITransferCreate(SHCLURITRANSFERDIR_READ,
                                                     SHCLSOURCE_LOCAL,
                                                     &pTransfer);
           if (RT_SUCCESS(rc))
           {
               SHCLURITRANSFERCALLBACKS TransferCallbacks;
               RT_ZERO(TransferCallbacks);

               TransferCallbacks.pvUser              = &pCtx->URI;
               TransferCallbacks.pfnTransferComplete = vboxClipboardURITransferCompleteCallback;
               TransferCallbacks.pfnTransferError    = vboxClipboardURITransferErrorCallback;

               SharedClipboardURITransferSetCallbacks(pTransfer, &TransferCallbacks);

               SHCLPROVIDERCREATIONCTX creationCtx;
               RT_ZERO(creationCtx);

               creationCtx.enmSource = SHCLSOURCE_REMOTE;

               creationCtx.Interface.pfnGetRoots      = vboxClipboardURIGetRoots;
               creationCtx.Interface.pfnListOpen      = vboxClipboardURIListOpen;
               creationCtx.Interface.pfnListClose     = vboxClipboardURIListClose;
               creationCtx.Interface.pfnListHdrRead   = vboxClipboardURIListHdrRead;
               creationCtx.Interface.pfnListEntryRead = vboxClipboardURIListEntryRead;
               creationCtx.Interface.pfnObjOpen       = vboxClipboardURIObjOpen;
               creationCtx.Interface.pfnObjClose      = vboxClipboardURIObjClose;
               creationCtx.Interface.pfnObjRead       = vboxClipboardURIObjRead;

               creationCtx.pvUser = pCtx;

               rc = SharedClipboardURITransferSetInterface(pTransfer, &creationCtx);
               if (RT_SUCCESS(rc))
               {
                   rc = SharedClipboardURICtxTransferAdd(&pCtx->URI, pTransfer);
                   if (RT_SUCCESS(rc))
                   {
                       rc = SharedClipboardURITransferPrepare(pTransfer);
                       if (RT_SUCCESS(rc))
                       {
                           rc = SharedClipboardWinURITransferCreate(pWinCtx, pTransfer);
                       }
                   }
               }
           }

           break;
       }
#endif /* 0 */

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

            SharedClipboardWinChainAdd(pWinCtx);
            if (!SharedClipboardWinIsNewAPI(&pWinCtx->newAPI))
                pWinCtx->oldAPI.timerRefresh = SetTimer(pWinCtx->hWnd, 0, 10 * 1000 /* 10s */, NULL);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static void vboxClipboardDestroy(PSHCLCONTEXT pCtx)
{
    AssertPtrReturnVoid(pCtx);

    const PSHCLWINCTX pWinCtx = &pCtx->Win;

    if (pWinCtx->hWnd)
    {
        DestroyWindow(pWinCtx->hWnd);
        pWinCtx->hWnd = NULL;
    }

    UnregisterClass(s_szClipWndClassName, pCtx->pEnv->hInstance);

    SharedClipboardWinCtxDestroy(&pCtx->Win);
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

    pCtx->pEnv = pEnv;

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
        LogRel(("Shared Clipboard: Initializing OLE failed (%Rhrc) -- file transfers unavailable\n"));
        /* Not critical, the rest of the clipboard might work. */
    }
    else
        LogRel(("Shared Clipboard: Initialized OLE\n"));
#endif

    if (RT_SUCCESS(rc))
    {
        /* Check if new Clipboard API is available. */
        rc = SharedClipboardWinCtxInit(&pCtx->Win);
        if (RT_SUCCESS(rc))
            rc = VbglR3ClipboardConnectEx(&pCtx->CmdCtx);
        if (RT_SUCCESS(rc))
        {
            rc = vboxClipboardCreateWindow(pCtx);
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
                rc = SharedClipboardURICtxInit(&pCtx->URI);
                if (RT_SUCCESS(rc))
#endif
                    *ppInstance = pCtx;
            }
            else
            {
                VbglR3ClipboardDisconnectEx(&pCtx->CmdCtx);
            }
        }
    }

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

    int rc;

    LogFlowFunc(("Using protocol %RU32\n", pCtx->CmdCtx.uProtocolVer));

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
                rc = VbglR3ClipboardEventGetNextEx(uMsg, cParms, &pCtx->CmdCtx, &pCtx->URI, pEvent);
#else
                rc = VbglR3ClipboardEventGetNext(uMsg, cParms, &pCtx->CmdCtx, pEvent);
#endif
            }
        }

        if (RT_FAILURE(rc))
        {
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
                   break;
               }

               case VBGLR3CLIPBOARDEVENTTYPE_READ_DATA:
               {
                   /* The host needs data in the specified format. */
                   ::PostMessage(pWinCtx->hWnd, SHCL_WIN_WM_READ_DATA,
                                 0 /* wParam */, (LPARAM)pEvent /* lParam */);
                   break;
               }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
               case VBGLR3CLIPBOARDEVENTTYPE_URI_TRANSFER_STATUS:
               {
                   ::PostMessage(pWinCtx->hWnd, SHCL_WIN_WM_URI_TRANSFER_STATUS,
                                 0 /* wParm */, (LPARAM)pEvent /* lParm */);
                   break;
               }
#endif
               case VBGLR3CLIPBOARDEVENTTYPE_QUIT:
               {
                   /* The host is terminating. */
                   LogRel(("Shared Clipboard: Terminating ...\n"));
                   ASMAtomicXchgBool(pfShutdown, true);
                   break;
               }

               default:
               {
                   /* Wait a bit before retrying. */
                   RTThreadSleep(1000);
                   break;
               }
            }

            if (RT_FAILURE(rc))
                VbglR3ClipboardEventFree(pEvent);
        }

        if (*pfShutdown)
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) VBoxShClStop(void *pInstance)
{
    AssertPtrReturn(pInstance, VERR_INVALID_POINTER);

    LogFunc(("Stopping pInstance=%p\n", pInstance));

    PSHCLCONTEXT pCtx = (PSHCLCONTEXT)pInstance;
    AssertPtr(pCtx);

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
    OleSetClipboard(NULL); /* Make sure to flush the clipboard on destruction. */
    OleUninitialize();

    SharedClipboardURICtxDestroy(&pCtx->URI);
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

