/** @file
 *
 * VBoxClipboard - Shared clipboard
 *
 * Copyright (C) 2006-2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 */

#include "VBoxService.h"
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include <iprt/crc64.h>

// #define LOG_ENABLED

#ifdef LOG_ENABLED
#include <stdio.h>

static void _dprintf(LPSTR String, ...)
{
   va_list va;

   va_start(va, String);

   CHAR  Buffer[1024];
   if (strlen(String) < 1000)
   {
      _vsntprintf (Buffer, sizeof(Buffer), String, va);

      FILE *f = fopen ("c:\\inst\\clip.log", "ab");
      if (f)
      {
          fprintf (f, "%s", Buffer);
          fclose (f);
      }
   }

   va_end (va);
}
#define dprintf(a) _dprintf a
#else
#define dprintf(a) do {} while (0)
#endif /* LOG_ENABLED */

typedef struct _VBOXCLIPBOARDCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    uint32_t u32ClientID;

    uint32_t u32Mode;

    ATOM     atomWindowClass;

    HWND     hwnd;

    HWND     hwndNextInChain;

    HANDLE   hHostEvent;

    bool     fOperational;

    uint32_t u32LastSentFormat;
    uint64_t u64LastSentCRC64;

} VBOXCLIPBOARDCONTEXT;

static char gachWindowClassName[] = "VBoxSharedClipboardClass";

#define VBOX_INIT_CALL(a, b) do {                                                             \
    (a)->hdr.result      = VINF_SUCCESS;                                                      \
    (a)->hdr.u32ClientID = pCtx->u32ClientID;                                                 \
    (a)->hdr.u32Function = b;                                                                 \
    (a)->hdr.cParms      = (sizeof (*a) - sizeof ((a)->hdr)) / sizeof (HGCMFunctionParameter); \
} while (0)

static bool vboxClipboardIsSameAsLastSent (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format,
                                          void *pv, uint32_t cb)
{
    uint64_t u64CRC = RTCrc64 (pv, cb);

    if (   pCtx->u32LastSentFormat == u32Format
        && pCtx->u64LastSentCRC64 == u64CRC)
    {
        return true;
    }

    pCtx->u64LastSentCRC64 = u64CRC;
    pCtx->u32LastSentFormat = u32Format;

    return false;
}

static int vboxClipboardConnect (VBOXCLIPBOARDCONTEXT *pCtx)
{
    VBoxGuestHGCMConnectInfo info;

    memset (&info, 0, sizeof (info));

    info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;

    strcpy (info.Loc.u.host.achName, "VBoxSharedClipboard");

    DWORD cbReturned;

    if (DeviceIoControl (pCtx->pEnv->hDriver,
                         IOCTL_VBOXGUEST_HGCM_CONNECT,
                         &info, sizeof (info),
                         &info, sizeof (info),
                         &cbReturned,
                         NULL))
    {
        if (info.result == VINF_SUCCESS)
        {
            pCtx->u32ClientID = info.u32ClientID;
        }
    }
    else
    {
        info.result = VERR_NOT_SUPPORTED;
    }

    return info.result;
}

static void vboxClipboardDisconnect (VBOXCLIPBOARDCONTEXT *pCtx)
{
    if (pCtx->u32ClientID == 0)
    {
        return;
    }

    VBoxGuestHGCMDisconnectInfo info;

    memset (&info, 0, sizeof (info));

    info.u32ClientID = pCtx->u32ClientID;

    DWORD cbReturned;

    DeviceIoControl (pCtx->pEnv->hDriver,
                     IOCTL_VBOXGUEST_HGCM_DISCONNECT,
                     &info, sizeof (info),
                     &info, sizeof (info),
                     &cbReturned,
                     NULL);

    return;
}


static void VBoxHGCMParmUInt32Set (HGCMFunctionParameter *pParm, uint32_t u32)
{
    pParm->type = VMMDevHGCMParmType_32bit;
    pParm->u.value32 = u32;
}

static int VBoxHGCMParmUInt32Get (HGCMFunctionParameter *pParm, uint32_t *pu32)
{
    if (pParm->type == VMMDevHGCMParmType_32bit)
    {
        *pu32 = pParm->u.value32;
        return VINF_SUCCESS;
    }

    return VERR_INVALID_PARAMETER;
}

static void VBoxHGCMParmPtrSet (HGCMFunctionParameter *pParm, void *pv, uint32_t cb)
{
    pParm->type                    = VMMDevHGCMParmType_LinAddr;
    pParm->u.Pointer.size          = cb;
    pParm->u.Pointer.u.linearAddr  = (VBOXGCPTR)pv;
}


static int vboxCall (HANDLE hDriver, void *pvData, unsigned cbData)
{
    DWORD cbReturned;

    if (DeviceIoControl (hDriver,
                         IOCTL_VBOXGUEST_HGCM_CALL,
                         pvData, cbData,
                         pvData, cbData,
                         &cbReturned,
                         NULL))
    {
        return VINF_SUCCESS;
    }

    return VERR_NOT_SUPPORTED;
}

static int vboxClipboardQueryMode (VBOXCLIPBOARDCONTEXT *pCtx)
{
    VBoxClipboardQueryMode parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_QUERY_MODE);

    VBoxHGCMParmUInt32Set (&parms.mode, 0);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;

        if (VBOX_SUCCESS (rc))
        {
            rc = VBoxHGCMParmUInt32Get (&parms.mode, &pCtx->u32Mode);

            dprintf (("vboxClipboardQueryMode: Mode = %d, rc = %d\n", pCtx->u32Mode, rc));
        }
    }

    return rc;
}

static int vboxClipboardHostEventCancel (VBOXCLIPBOARDCONTEXT *pCtx)
{
    VBoxClipboardHostEventCancel parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_HOST_EVENT_CANCEL);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;
    }

    return rc;
}

static int vboxClipboardHostEventRead (VBOXCLIPBOARDCONTEXT *pCtx, void *pv, uint32_t cb)
{
    if (!VirtualLock (pv, cb))
    {
        dprintf (("vboxClipboardHostEventRead: VirtualLock failed\n"));
        return VERR_NOT_SUPPORTED;
    }

    VBoxClipboardHostEventRead parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_HOST_EVENT_READ);

    VBoxHGCMParmPtrSet (&parms.ptr, pv, cb);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;
    }

    VirtualUnlock (pv, cb);

    return rc;
}

static int vboxClipboardSendData (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format,
                                  void *pv, uint32_t cb)
{
    if (vboxClipboardIsSameAsLastSent (pCtx, u32Format, pv, cb))
    {
        dprintf (("vboxClipboardSendData: The data to be sent are the same as the last sent.\n"));
        return VINF_SUCCESS;
    }

    if (!VirtualLock (pv, cb))
    {
        dprintf (("vboxClipboardSendData: VirtualLock failed\n"));
        return VERR_NOT_SUPPORTED;
    }

    VBoxClipboardSendData parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_SEND_DATA);

    VBoxHGCMParmUInt32Set (&parms.format, u32Format);
    VBoxHGCMParmPtrSet (&parms.ptr, pv, cb);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;
    }

    VirtualUnlock (pv, cb);

    return rc;
}


static void vboxClipboardSendDIB (VBOXCLIPBOARDCONTEXT *pCtx)
{
    HANDLE hClip = GetClipboardData (CF_DIB);

    if (hClip != NULL)
    {
        LPVOID lp = GlobalLock (hClip);

        if (lp != NULL)
        {
            vboxClipboardSendData (pCtx, VBOX_SHARED_CLIPBOARD_FMT_BITMAP,
                                   lp, GlobalSize (hClip));

            GlobalUnlock(hClip);
        }
    }
}

static void vboxClipboardSendBitmap (VBOXCLIPBOARDCONTEXT *pCtx)
{
    /* Request DIB format anyway. The system will convert the available CF_BITMAP to CF_DIB. */
    vboxClipboardSendDIB (pCtx);
}

static void vboxClipboardSendUnicodeText (VBOXCLIPBOARDCONTEXT *pCtx)
{
    HANDLE hClip = GetClipboardData(CF_UNICODETEXT);

    if (hClip != NULL)
    {
        LPWSTR uniString = (LPWSTR)GlobalLock (hClip);

        if (uniString != NULL)
        {
            vboxClipboardSendData (pCtx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                   uniString, (lstrlenW (uniString) + 1) * 2);

            GlobalUnlock(hClip);
        }
    }
}

static void vboxClipboardSendText (VBOXCLIPBOARDCONTEXT *pCtx)
{
    /* Request UNICODE format anyway. The system will convert the available CF_TEXT to CF_UNICODE. */
    vboxClipboardSendUnicodeText (pCtx);
}

static void vboxClipboardGuestEvent (VBOXCLIPBOARDCONTEXT *pCtx)
{
    if (   pCtx->u32Mode != VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
        && pCtx->u32Mode != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
    {
        /* The host will not accept the clipboard data anyway. */
        return;
    }

    if (OpenClipboard (pCtx->hwnd))
    {
        int cFormats = CountClipboardFormats ();

        if (cFormats > 0)
        {
            dprintf (("vboxClipboardGuestEvent: cFormats = %d\n", cFormats));

            static UINT aFormatPriorityList[] =
            {
                CF_UNICODETEXT,
                CF_TEXT,
                CF_DIB,
                CF_BITMAP
            };

            int iFormat = GetPriorityClipboardFormat (aFormatPriorityList, ELEMENTS (aFormatPriorityList));

            switch (iFormat)
            {
                case CF_TEXT:
                {
                    dprintf(("CF_TEXT\n"));

                    vboxClipboardSendText (pCtx);
                } break;

                case CF_UNICODETEXT:
                {
                    dprintf(("CF_UNICODETEXT\n"));

                    vboxClipboardSendUnicodeText (pCtx);
                } break;

                case CF_BITMAP:
                {
                    dprintf(("CF_BITMAP\n"));

                    vboxClipboardSendBitmap (pCtx);
                } break;

                case CF_DIB:
                {
                    dprintf(("CF_DIB\n"));

                    vboxClipboardSendDIB (pCtx);
                } break;

                default:
                {
                    dprintf(("not supported %d\n", iFormat));
                }
            }
        }

        CloseClipboard ();
    }
}

static void vboxClipboardHostEvent (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format, uint32_t u32Size)
{
    if (   pCtx->u32Mode != VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
        && pCtx->u32Mode != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
    {
        /* The guest should not accept the clipboard data anyway. */
        vboxClipboardHostEventCancel (pCtx);

        return;
    }

    dprintf(("vboxClipboardHostEvent u32Size %d\n", u32Size));

    if (u32Size > 0)
    {
        HANDLE hMem = GlobalAlloc (GMEM_DDESHARE | GMEM_MOVEABLE, u32Size);

        dprintf(("vboxClipboardHostEvent hMem %p\n", hMem));

        if (hMem)
        {
            void *pMem = GlobalLock (hMem);

            dprintf(("vboxClipboardHostEvent pMem %p, GlobalSize %d\n", pMem, GlobalSize (hMem)));

            if (pMem)
            {
                /* Read the host data and cancel the transaction. */
                int rc = vboxClipboardHostEventRead (pCtx, pMem, u32Size);

                dprintf(("vboxClipboardHostEvent read %d\n", rc));

                if (VBOX_SUCCESS (rc))
                {
                    /* Data was read from the host and transaction was cancelled.
                     * Return from the code branch without calling cancel event.
                     */
                    HANDLE hClip = 0;

                    /* Remember the last data that goes to the clipboard.
                     * That will prevent sending the same data back to the client.
                     */
                    vboxClipboardIsSameAsLastSent (pCtx, u32Format, pMem, u32Size);

                    /* The memory must be unlocked before the Clipboard is closed. */
                    GlobalUnlock (hMem);

                    /* 'pMem' contains the host clipboard data.
                     * size is 'u32Size' and format is 'u32Format'.
                     */
                    if (OpenClipboard (pCtx->hwnd))
                    {
                        EmptyClipboard();

                        if (u32Format == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                        {
                            dprintf(("vboxClipboardHostEvent VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT\n"));
                            hClip = SetClipboardData (CF_UNICODETEXT, hMem);
                        }
                        else if (u32Format == VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
                        {
                            dprintf(("vboxClipboardHostEvent VBOX_SHARED_CLIPBOARD_FMT_BiTMAP\n"));
                            hClip = SetClipboardData (CF_DIB, hMem);
                        }

                        CloseClipboard();

                        dprintf(("vboxClipboardHostEvent hClip %p\n", hClip));
                    }
                    else
                    {
                        dprintf(("vboxClipboardHostEvent failed to open clipboard\n"));
                    }

                    if (!hClip)
                    {
                        /* Failed to set the clipboard data. */
                        GlobalFree (hMem);
                    }
                    else
                    {
                        /* The hMem ownership has gone to the system. Nothing to do. */
                    }

                    return;
                }
                else
                {
                    GlobalUnlock (hMem);
                }
            }

            GlobalFree (hMem);
        }
    }
    else
    {
        if (OpenClipboard (pCtx->hwnd))
        {
            EmptyClipboard();
            CloseClipboard();
        }
    }

    vboxClipboardHostEventCancel (pCtx);

    return;
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

            dprintf (("vboxClipboardProcessMsg: WM_CHANGECBCHAIN: hwndRemoved %p, hwndNext %p, hwnd %p\n", hwndRemoved, hwndNext, pCtx->hwnd));

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
                    rc = SendMessage (pCtx->hwndNextInChain, WM_CHANGECBCHAIN, wParam, lParam);
                }
            }
        } break;

        case WM_DRAWCLIPBOARD:
        {
            dprintf (("vboxClipboardProcessMsg: WM_DRAWCLIPBOARD, hwnd %p\n", pCtx->hwnd));

            vboxClipboardGuestEvent (pCtx);

            /* Pass the message to next windows in the clipboard chain. */
            rc = SendMessage (pCtx->hwndNextInChain, msg, wParam, lParam);
        } break;

        case WM_CLOSE:
        {
            /* Do nothing. Ignore the message. */
        } break;

        default:
        {
            rc = DefWindowProc (hwnd, msg, wParam, lParam);
        }
    }

    return rc;
}

static LRESULT CALLBACK vboxClipboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int vboxClipboardInit (VBOXCLIPBOARDCONTEXT *pCtx)
{
    int rc = VINF_SUCCESS;

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

            pCtx->hwndNextInChain = SetClipboardViewer (pCtx->hwnd);
        }
    }

    return rc;
}

static void vboxClipboardDestroy (VBOXCLIPBOARDCONTEXT *pCtx)
{
    if (pCtx->hwnd)
    {
        ChangeClipboardChain (pCtx->hwnd, pCtx->hwndNextInChain);
        pCtx->hwndNextInChain = NULL;

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
    return vboxClipboardProcessMsg (&gCtx, hwnd, msg, wParam, lParam);
}

int VBoxClipboardInit (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    int rc = VINF_SUCCESS;

    dprintf (("VboxClipboardInit\n"));

    if (!gCtx.pEnv)
    {
        memset (&gCtx, 0, sizeof (gCtx));

        gCtx.pEnv = pEnv;

        rc = vboxClipboardConnect (&gCtx);

        if (VBOX_SUCCESS (rc))
        {
            rc = vboxClipboardQueryMode (&gCtx);

            if (VBOX_SUCCESS (rc))
            {
                rc = vboxClipboardInit (&gCtx);

                dprintf (("vboxClipboardInit: rc = %d\n", rc));

                if (VBOX_SUCCESS (rc))
                {
                    /* Host to Guest direction requires the thread. */
                    if (   gCtx.u32Mode == VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                        || gCtx.u32Mode == VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
                    {
                        *pfStartThread = true;
                    }
                }
            }
        }
    }

    if (VBOX_SUCCESS (rc))
    {
        *ppInstance = &gCtx;
    }
    else
    {
        vboxClipboardDisconnect (&gCtx);
    }

    return rc;
}

unsigned __stdcall VBoxClipboardThread (void *pInstance)
{
    VBOXCLIPBOARDCONTEXT *pCtx = (VBOXCLIPBOARDCONTEXT *)pInstance;

    dprintf(("VBoxClipboardThread\n"));

    HANDLE hHostEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    /* The thread waits for incoming data from the host.
     * Register an event notification.
     */
    for (;;)
    {
        uint32_t u32Format;
        uint32_t u32Size;

        VBoxClipboardHostEventQuery parms;

        VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_HOST_EVENT_QUERY);

        VBoxHGCMParmUInt32Set (&parms.format, 0);
        VBoxHGCMParmUInt32Set (&parms.size, 0);

        DWORD cbReturned;

        if (!DeviceIoControl (pCtx->pEnv->hDriver,
                              IOCTL_VBOXGUEST_HGCM_CALL,
                              &parms, sizeof (parms),
                              &parms, sizeof (parms),
                              &cbReturned,
                              NULL))
        {
            dprintf(("Failed to call the driver for host event\n"));

            /* Wait a bit before retrying. */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
            {
                break;
            }

            continue;
        }

        int rc = parms.hdr.result;

        if (VBOX_SUCCESS (rc))
        {
            rc = VBoxHGCMParmUInt32Get (&parms.format, &u32Format);

            if (VBOX_SUCCESS (rc))
            {
                rc = VBoxHGCMParmUInt32Get (&parms.size, &u32Size);

                if (VBOX_SUCCESS (rc))
                {
                    vboxClipboardHostEvent (pCtx, u32Format, u32Size);
                }
            }
        }

        dprintf(("processed host event rc = %d\n", rc));
    }

    return 0;
}

void VBoxClipboardDestroy (const VBOXSERVICEENV *pEnv, void *pInstance)
{
    VBOXCLIPBOARDCONTEXT *pCtx = (VBOXCLIPBOARDCONTEXT *)pInstance;

    if (pCtx != &gCtx)
    {
        dprintf(("VBoxClipboardDestroy: invalid instance %p (our %p)!!!\n", pCtx, &gCtx));

        pCtx = &gCtx;
    }

    vboxClipboardDestroy (pCtx);

    vboxClipboardDisconnect (pCtx);

    memset (pCtx, 0, sizeof (*pCtx));

    return;
}
