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
#include <iprt/ldr.h>

#include <iprt/errcore.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/HostServices/VBoxClipboardSvc.h> /* Temp, remove. */
#include <VBox/GuestHost/SharedClipboard-win.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
#   include <VBox/GuestHost/SharedClipboard-uri.h>
#endif

#include <strsafe.h>

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/** !!! HACK ALERT !!! Dynamically resolve functions! */
#   ifdef _WIN32_IE
#       undef _WIN32_IE
#       define _WIN32_IE 0x0501
#   endif
#   include <iprt/win/shlobj.h>
#   include <iprt/win/shlwapi.h>
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

typedef struct _VBOXCLIPBOARDCONTEXT
{
    /** Pointer to the VBoxClient service environment. */
    const VBOXSERVICEENV    *pEnv;
    /** Client ID the service is connected to the HGCM service with. */
    uint32_t                 u32ClientID;
    /** Windows-specific context data. */
    VBOXCLIPBOARDWINCTX      Win;
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    /** URI transfer data. */
    SHAREDCLIPBOARDURICTX    URI;
#endif
} VBOXCLIPBOARDCONTEXT, *PVBOXCLIPBOARDCONTEXT;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
typedef struct _VBOXCLIPBOARDURIREADTHREADCTX
{
    PVBOXCLIPBOARDCONTEXT       pClipboardCtx;
    PSHAREDCLIPBOARDURITRANSFER pTransfer;
} VBOXCLIPBOARDURIREADTHREADCTX, *PVBOXCLIPBOARDURIREADTHREADCTX;

typedef struct _VBOXCLIPBOARDURIWRITETHREADCTX
{
    PVBOXCLIPBOARDCONTEXT       pClipboardCtx;
    PSHAREDCLIPBOARDURITRANSFER pTransfer;
} VBOXCLIPBOARDURIWRITETHREADCTX, *PVBOXCLIPBOARDURIWRITETHREADCTX;
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */


/*********************************************************************************************************************************
*   Static variables                                                                                                             *
*********************************************************************************************************************************/
/** Static clipboard context (since it is the single instance). Directly used in the windows proc. */
static VBOXCLIPBOARDCONTEXT g_Ctx = { NULL };
/** Static window class name. */
static char s_szClipWndClassName[] = VBOX_CLIPBOARD_WNDCLASS_NAME;


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static DECLCALLBACK(void) vboxClipboardURITransferCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);
static DECLCALLBACK(void) vboxClipboardURITransferErrorCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);
#endif


#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static DECLCALLBACK(int) vboxClipboardURIWriteThread(RTTHREAD ThreadSelf, void *pvUser)
{
    RT_NOREF(ThreadSelf);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDURIWRITETHREADCTX pCtx = (PVBOXCLIPBOARDURIWRITETHREADCTX)pvUser;
    AssertPtr(pCtx);

    PSHAREDCLIPBOARDURITRANSFER pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    pTransfer->Thread.fStarted = true;

    RTThreadUserSignal(RTThreadSelf());

    uint32_t uClientID;
    int rc = VbglR3ClipboardConnect(&uClientID);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3ClipboardTransferSendStatus(uClientID, SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING);
        if (RT_SUCCESS(rc))
        {
            bool     fTerminate = false;
            unsigned cErrors    = 0;

            for (;;)
            {
                PVBGLR3CLIPBOARDEVENT pEvent = NULL;
                rc = VbglR3ClipboardEventGetNext(uClientID, pTransfer, &pEvent);
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

        VbglR3ClipboardDisconnect(uClientID);
    }

    RTMemFree(pCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) vboxClipboardURITransferCompleteCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc)
{
    RT_NOREF(rc);

    LogFlowFunc(("pData=%p, rc=%Rrc\n", pData, rc));

    LogRel2(("Shared Clipboard: Transfer to destination complete\n"));

    PSHAREDCLIPBOARDURICTX pCtx = (PSHAREDCLIPBOARDURICTX)pData->pvUser;
    AssertPtr(pCtx);

    PSHAREDCLIPBOARDURITRANSFER pTransfer = pData->pTransfer;
    AssertPtr(pTransfer);

    if (pTransfer->pvUser) /* SharedClipboardWinURITransferCtx */
    {
        delete pTransfer->pvUser;
        pTransfer->pvUser = NULL;
    }

    int rc2 = SharedClipboardURICtxTransferRemove(pCtx, pTransfer);
    AssertRC(rc2);
}

static DECLCALLBACK(void) vboxClipboardURITransferErrorCallback(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc)
{
    RT_NOREF(rc);

    LogFlowFunc(("pData=%p, rc=%Rrc\n", pData, rc));

    LogRel(("Shared Clipboard: Transfer to destination failed with %Rrc\n", rc));

    PSHAREDCLIPBOARDURICTX pCtx = (PSHAREDCLIPBOARDURICTX)pData->pvUser;
    AssertPtr(pCtx);

    PSHAREDCLIPBOARDURITRANSFER pTransfer = pData->pTransfer;
    AssertPtr(pTransfer);

    if (pTransfer->pvUser) /* SharedClipboardWinURITransferCtx */
    {
        delete pTransfer->pvUser;
        pTransfer->pvUser = NULL;
    }

    int rc2 = SharedClipboardURICtxTransferRemove(pCtx, pTransfer);
    AssertRC(rc2);
}

static int vboxClipboardURITransferOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

static int vboxClipboardURITransferClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx)
{
    RT_NOREF(pCtx);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

static int vboxClipboardURIListOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx, PVBOXCLIPBOARDLISTHDR pListHdr,
                                    PSHAREDCLIPBOARDLISTHANDLE phList)
{
    RT_NOREF(pCtx, pListHdr, phList);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = 0; // VbglR3ClipboardRecvListOpen(pThisCtx->u32ClientID, pListHdr, phList);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIListHdrRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDLISTHANDLE hList,
                                       PVBOXCLIPBOARDLISTHDR pListHdr)
{
    RT_NOREF(hList);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    LogFlowFunc(("c=%RU32\n", pThisCtx->u32ClientID));

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
static int vboxClipboardURIListHdrWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDLISTHANDLE hList,
                                        PVBOXCLIPBOARDLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardListHdrWrite(pThisCtx->u32ClientID, hList, pListHdr);

    LogFlowFuncLeaveRC(rc);
    return rc;
}*/

static int vboxClipboardURIListEntryRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDLISTHANDLE hList,
                                         PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    RT_NOREF(hList);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    RT_NOREF(pListEntry);
    int rc = 0; // VbglR3ClipboardListEntryRead(pThisCtx->u32ClientID, pListEntry);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
static int vboxClipboardURIListEntryWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDLISTHANDLE hList,
                                          PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardListEntryWrite(pThisCtx->u32ClientID, hList, pListEntry);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
*/

static int vboxClipboardURIObjOpen(PSHAREDCLIPBOARDPROVIDERCTX pCtx, const char *pszPath,
                                   PVBOXCLIPBOARDCREATEPARMS pCreateParms, PSHAREDCLIPBOARDOBJHANDLE phObj)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjOpen(pThisCtx->u32ClientID, pszPath, pCreateParms, phObj);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIObjClose(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj)
{
    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjClose(pThisCtx->u32ClientID, hObj);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIObjRead(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj,
                                   void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbRead)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjRead(pThisCtx->u32ClientID, hObj, pvData, cbData, pcbRead);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vboxClipboardURIObjWrite(PSHAREDCLIPBOARDPROVIDERCTX pCtx, SHAREDCLIPBOARDOBJHANDLE hObj,
                                    void *pvData, uint32_t cbData, uint32_t fFlags, uint32_t *pcbWritten)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDCONTEXT pThisCtx = (PVBOXCLIPBOARDCONTEXT)pCtx->pvUser;
    AssertPtr(pThisCtx);

    int rc = VbglR3ClipboardObjWrite(pThisCtx->u32ClientID, hObj, pvData, cbData, pcbWritten);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

static LRESULT vboxClipboardWinProcessMsg(PVBOXCLIPBOARDCONTEXT pCtx, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AssertPtr(pCtx);

    const PVBOXCLIPBOARDWINCTX pWinCtx = &pCtx->Win;

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
               VBOXCLIPBOARDFORMATS fFormats;
               int rc = VBoxClipboardWinGetFormats(&pCtx->Win, &fFormats);
               if (RT_SUCCESS(rc))
               {
                   LogFunc(("WM_CLIPBOARDUPDATE: Reporting formats 0x%x\n", fFormats));
                   rc = VbglR3ClipboardReportFormats(pCtx->u32ClientID, fFormats);
               }
           }
       }
           break;

       case WM_CHANGECBCHAIN:
       {
           LogFunc(("WM_CHANGECBCHAIN\n"));
           lresultRc = VBoxClipboardWinHandleWMChangeCBChain(pWinCtx, hwnd, msg, wParam, lParam);
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

           lresultRc = VBoxClipboardWinChainPassToNext(pWinCtx, msg, wParam, lParam);
       }
           break;

       case WM_TIMER:
       {
           int rc = VBoxClipboardWinHandleWMTimer(pWinCtx);
           AssertRC(rc);
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
           LogFunc(("WM_RENDERALLFORMATS\n"));

           int rc = VBoxClipboardWinHandleWMRenderAllFormats(pWinCtx, hwnd);
           AssertRC(rc);
       }
           break;

       case VBOX_CLIPBOARD_WM_SET_FORMATS:
       {
           LogFunc(("VBOX_CLIPBOARD_WM_SET_FORMATS\n"));

           /* Announce available formats. Do not insert data -- will be inserted in WM_RENDERFORMAT. */
           VBOXCLIPBOARDFORMATS fFormats = (uint32_t)lParam;
           if (fFormats != VBOX_SHARED_CLIPBOARD_FMT_NONE) /* Could arrive with some older GA versions. */
           {
               int rc = VBoxClipboardWinOpen(hwnd);
               if (RT_SUCCESS(rc))
               {
                   VBoxClipboardWinClear();

#if 0
                   if (fFormats & VBOX_SHARED_CLIPBOARD_FMT_URI_LIST)
                   {
                       LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_URI_LIST\n"));

                       PSHAREDCLIPBOARDURITRANSFER pTransfer;
                       rc = SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR_READ,
                                                             SHAREDCLIPBOARDSOURCE_REMOTE,
                                                             &pTransfer);
                       if (RT_SUCCESS(rc))
                       {
                           SHAREDCLIPBOARDURITRANSFERCALLBACKS TransferCallbacks;
                           RT_ZERO(TransferCallbacks);

                           TransferCallbacks.pvUser              = &pCtx->URI;
                           TransferCallbacks.pfnTransferComplete = vboxClipboardURITransferCompleteCallback;
                           TransferCallbacks.pfnTransferError    = vboxClipboardURITransferErrorCallback;

                           SharedClipboardURITransferSetCallbacks(pTransfer, &TransferCallbacks);

                           SHAREDCLIPBOARDPROVIDERCREATIONCTX creationCtx;
                           RT_ZERO(creationCtx);
                           creationCtx.enmSource = SHAREDCLIPBOARDSOURCE_REMOTE;

                           RT_ZERO(creationCtx.Interface);
                           creationCtx.Interface.pfnTransferOpen  = vboxClipboardURITransferOpen;
                           creationCtx.Interface.pfnTransferClose = vboxClipboardURITransferClose;
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
                                   rc = VBoxClipboardWinURITransferCreate(pWinCtx, pTransfer);
                           }

                           /* Note: VBoxClipboardWinURITransferCreate() takes care of closing the clipboard. */

                           LogFunc(("VBOX_SHARED_CLIPBOARD_FMT_URI_LIST: rc=%Rrc\n", rc));
                       }
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
           }
           LogFunc(("VBOX_CLIPBOARD_WM_SET_FORMATS: fFormats=0x%x, lastErr=%ld\n", fFormats, GetLastError()));
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

               if (hClip == NULL)
               {
                   LogFunc(("VBOX_WM_SHCLPB_READ_DATA: hClip=NULL, lastError=%ld\n", GetLastError()));

                   /* Requested clipboard format is not available, send empty data. */
                   VbglR3ClipboardWriteData(pCtx->u32ClientID, VBOX_SHARED_CLIPBOARD_FMT_NONE, NULL, 0);
#ifdef DEBUG_andy
                   AssertFailed();
#endif
               }

               VBoxClipboardWinClose();
           }
           break;
       }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
       /* The host wants to read URI data. */
       case VBOX_CLIPBOARD_WM_URI_START_READ:
       {
           LogFunc(("VBOX_CLIPBOARD_WM_URI_START_READ: cTransfersRunning=%RU32\n",
                    SharedClipboardURICtxGetRunningTransfers(&pCtx->URI)));

           int rc = VBoxClipboardWinOpen(hwnd);
           if (RT_SUCCESS(rc))
           {
               PSHAREDCLIPBOARDURITRANSFER pTransfer;
               rc = SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR_WRITE,
                                                     SHAREDCLIPBOARDSOURCE_LOCAL,
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
                               rc = VBoxClipboardWinDropFilesToStringList((DROPFILES *)hDrop, &papszList, &cbList);

                               GlobalUnlock(hClip);

                               if (RT_SUCCESS(rc))
                               {
                                   rc = SharedClipboardURILTransferSetRoots(pTransfer, papszList, cbList);
                                   if (RT_SUCCESS(rc))
                                   {
                                       PVBOXCLIPBOARDURIWRITETHREADCTX pThreadCtx
                                           = (PVBOXCLIPBOARDURIWRITETHREADCTX)RTMemAllocZ(sizeof(VBOXCLIPBOARDURIWRITETHREADCTX));
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

               VBoxClipboardWinClose();
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

           PSHAREDCLIPBOARDURITRANSFER pTransfer;
           int rc = SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR_READ,
                                                     SHAREDCLIPBOARDSOURCE_LOCAL,
                                                     &pTransfer);
           if (RT_SUCCESS(rc))
           {
               SHAREDCLIPBOARDURITRANSFERCALLBACKS TransferCallbacks;
               RT_ZERO(TransferCallbacks);

               TransferCallbacks.pvUser              = &pCtx->URI;
               TransferCallbacks.pfnTransferComplete = vboxClipboardURITransferCompleteCallback;
               TransferCallbacks.pfnTransferError    = vboxClipboardURITransferErrorCallback;

               SharedClipboardURITransferSetCallbacks(pTransfer, &TransferCallbacks);

               SHAREDCLIPBOARDPROVIDERCREATIONCTX creationCtx;
               RT_ZERO(creationCtx);
               creationCtx.enmSource = SHAREDCLIPBOARDSOURCE_REMOTE;

               RT_ZERO(creationCtx.Interface);
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
                       if (RT_SUCCESS(rc))
                       {
                           rc = SharedClipboardURITransferPrepare(pTransfer);
                           if (RT_SUCCESS(rc))
                           {
                               rc = VBoxClipboardWinURITransferCreate(pWinCtx, pTransfer);
                           }
                       }
                   }
               }
           }

           break;
       }
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

       case WM_DESTROY:
       {
           LogFunc(("WM_DESTROY\n"));

           int rc = VBoxClipboardWinHandleWMDestroy(pWinCtx);
           AssertRC(rc);

           /*
            * Don't need to call PostQuitMessage cause
            * the VBoxTray already finished a message loop.
            */
       }
           break;

       default:
       {
           LogFunc(("WM_ %p\n", msg));
           lresultRc = DefWindowProc(hwnd, msg, wParam, lParam);
       }
           break;
    }

    LogFunc(("WM_ rc %d\n", lresultRc));
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

            VBoxClipboardWinChainAdd(pWinCtx);
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

    if (VbglR3AutoLogonIsRemoteSession())
    {
        /* Do not use clipboard for remote sessions. */
        LogRel(("Clipboard: Clipboard has been disabled for a remote session\n"));
        return VERR_NOT_SUPPORTED;
    }

    pCtx->pEnv = pEnv;

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr))
    {
        LogRel(("Clipboard: Initializing OLE failed (%Rhrc) -- file transfers unavailable\n"));
        /* Not critical, the rest of the clipboard might work. */
    }
    else
        LogRel(("Clipboard: Initialized OLE\n"));
#endif

    if (RT_SUCCESS(rc))
    {
        /* Check if new Clipboard API is available. */
        /* ignore rc */ VBoxClipboardWinCheckAndInitNewAPI(&pCtx->Win.newAPI);

        rc = VbglR3ClipboardConnect(&pCtx->u32ClientID);
        if (RT_SUCCESS(rc))
        {
            rc = vboxClipboardCreateWindow(pCtx);
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
                rc = SharedClipboardURICtxInit(&pCtx->URI);
                if (RT_SUCCESS(rc))
#endif
                    *ppInstance = pCtx;
            }
            else
            {
                VbglR3ClipboardDisconnect(pCtx->u32ClientID);
            }
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
        LogFlowFunc(("Waiting for host message ...\n"));

        uint32_t u32Msg;
        uint32_t u32Formats;
        rc = VbglR3ClipboardGetHostMsgOld(pCtx->u32ClientID, &u32Msg, &u32Formats);
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
                   LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_REPORT_FORMATS\n"));

                   /* The host has announced available clipboard formats.
                    * Forward the information to the window, so it can later
                    * respond to WM_RENDERFORMAT message. */
                   ::PostMessage(pWinCtx->hWnd, VBOX_CLIPBOARD_WM_SET_FORMATS, 0, u32Formats);
                   break;
               }

               case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
               {
                   LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA\n"));

                   /* The host needs data in the specified format. */
                   ::PostMessage(pWinCtx->hWnd, VBOX_CLIPBOARD_WM_READ_DATA, 0, u32Formats);
                   break;
               }

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
               case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START:
               {
                   LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START\n"));

                   const UINT uMsg = u32Formats == 0 ?
                                     VBOX_CLIPBOARD_WM_URI_START_READ : VBOX_CLIPBOARD_WM_URI_START_WRITE;

                   ::PostMessage(pWinCtx->hWnd, uMsg, 0 /* wParm */, 0 /* lParm */);
                   break;
               }
#endif
               case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
               {
                   LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT\n"));

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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
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
    VBoxClipboardInit,
    VBoxClipboardWorker,
    VBoxClipboardStop,
    VBoxClipboardDestroy
};

