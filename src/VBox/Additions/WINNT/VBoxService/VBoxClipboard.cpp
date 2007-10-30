/** @file
 *
 * VBoxClipboard - Shared clipboard
 *
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxService.h"
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include "helpers.h"
#include <iprt/asm.h>

typedef struct _VBOXCLIPBOARDCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    uint32_t u32ClientID;

    ATOM     atomWindowClass;

    HWND     hwnd;

    HWND     hwndNextInChain;

//    bool     fOperational;

//    uint32_t u32LastSentFormat;
//    uint64_t u64LastSentCRC64;
    
} VBOXCLIPBOARDCONTEXT;


static char gachWindowClassName[] = "VBoxSharedClipboardClass";


#define VBOX_INIT_CALL(__a, __b, __c) do {                                                             \
    (__a)->hdr.result      = VINF_SUCCESS;                                                             \
    (__a)->hdr.u32ClientID = (__c)->u32ClientID;                                                       \
    (__a)->hdr.u32Function = (__b);                                                                    \
    (__a)->hdr.cParms      = (sizeof (*(__a)) - sizeof ((__a)->hdr)) / sizeof (HGCMFunctionParameter); \
} while (0)


//static bool vboxClipboardIsSameAsLastSent (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format,
//                                           void *pv, uint32_t cb)
//{
//    uint64_t u64CRC = RTCrc64 (pv, cb);
//
//    if (   pCtx->u32LastSentFormat == u32Format
//        && pCtx->u64LastSentCRC64 == u64CRC)
//    {
//        return true;
//    }
//
//    pCtx->u64LastSentCRC64 = u64CRC;
//    pCtx->u32LastSentFormat = u32Format;
//
//    return false;
//}

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

static int vboxClipboardReportFormats (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Formats)
{
    VBoxClipboardFormats parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_FORMATS, pCtx);

    VBoxHGCMParmUInt32Set (&parms.formats, u32Formats);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;
    }

    return rc;
}

static int vboxClipboardReadData (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format, void *pv, uint32_t cb, uint32_t *pcbActual)
{
    VBoxClipboardReadData parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_READ_DATA, pCtx);

    VBoxHGCMParmUInt32Set (&parms.format, u32Format);
    VBoxHGCMParmPtrSet    (&parms.ptr, pv, cb);
    VBoxHGCMParmUInt32Set (&parms.size, 0);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;

        if (VBOX_SUCCESS (rc))
        {
            uint32_t u32Size;

            rc = VBoxHGCMParmUInt32Get (&parms.size, &u32Size);

            dprintf (("vboxClipboardReadData: actual size = %d, rc = %d\n", u32Size, rc));

            if (VBOX_SUCCESS (rc))
            {
                if (pcbActual)
                {
                    *pcbActual = u32Size;
                }
            }
        }
    }

    return rc;
}

static int vboxClipboardWriteData (VBOXCLIPBOARDCONTEXT *pCtx, uint32_t u32Format,
                                  void *pv, uint32_t cb)
{
//    if (vboxClipboardIsSameAsLastSent (pCtx, u32Format, pv, cb))
//    {
//        dprintf (("vboxClipboardWriteData: The data to be sent are the same as the last sent.\n"));
//        return VINF_SUCCESS;
//    }

    VBoxClipboardWriteData parms;

    VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA, pCtx);

    VBoxHGCMParmUInt32Set (&parms.format, u32Format);
    VBoxHGCMParmPtrSet (&parms.ptr, pv, cb);

    int rc = vboxCall (pCtx->pEnv->hDriver, &parms, sizeof (parms));

    if (VBOX_SUCCESS (rc))
    {
        rc = parms.hdr.result;
    }

    return rc;
}

static void vboxClipboardChanged (VBOXCLIPBOARDCONTEXT *pCtx)
{
    /* Query list of available formats and report to host. */
    if (OpenClipboard (pCtx->hwnd))
    {
        uint32_t u32Formats = 0;

        UINT format = 0;

        while ((format = EnumClipboardFormats (format)) != 0)
        {
            dprintf (("vboxClipboardChanged: format 0x%08X\n", format));
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
        
        vboxClipboardReportFormats (pCtx, u32Formats);
    }
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

            if (GetClipboardOwner () != hwnd)
            {
                /* Clipboard was updated by another application. */
                vboxClipboardChanged (pCtx);
            }

            /* Pass the message to next windows in the clipboard chain. */
            rc = SendMessage (pCtx->hwndNextInChain, msg, wParam, lParam);
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

            dprintf (("vboxClipboardProcessMsg: WM_RENDERFORMAT, format %x\n", format));
            
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
                EmptyClipboard ();
            }
            else
            {
                const uint32_t cbPrealloc = 4096;
                uint32_t cb = 0;
                
                /* Preallocate a buffer, most of small text transfers will fit into it. */
                HANDLE hMem = GlobalAlloc (GMEM_DDESHARE | GMEM_MOVEABLE, cbPrealloc);
                dprintf(("hMem %p\n", hMem));

                if (hMem)
                {
                    void *pMem = GlobalLock (hMem);
                    dprintf(("pMem %p, GlobalSize %d\n", pMem, GlobalSize (hMem)));

                    if (pMem)
                    {
                        /* Read the host data to the preallocated buffer. */
                        int vboxrc = vboxClipboardReadData (pCtx, u32Format, pMem, cbPrealloc, &cb);
                        dprintf(("vboxClipboardReadData vboxrc %d\n",  vboxrc));

                        if (VBOX_SUCCESS (rc))
                        {
                            if (cb > cbPrealloc)
                            {
                                GlobalUnlock (hMem);
                                
                                /* The preallocated buffer is too small, adjust the size. */
                                hMem = GlobalReAlloc (hMem, cb, 0);
                                dprintf(("hMem %p\n", hMem));
                                
                                if (hMem)
                                {
                                    pMem = GlobalLock (hMem);
                                    dprintf(("pMem %p, GlobalSize %d\n", pMem, GlobalSize (hMem)));

                                    if (pMem)
                                    {
                                        /* Read the host data to the preallocated buffer. */
                                        uint32_t cbNew = 0;
                                        vboxrc = vboxClipboardReadData (pCtx, u32Format, pMem, cb, &cbNew);
                                        dprintf(("vboxClipboardReadData vboxrc %d, cb = %d, cbNew = %d\n", vboxrc, cb, cbNew));

                                        if (VBOX_SUCCESS (vboxrc) && cbNew <= cb)
                                        {
                                            cb = cbNew;
                                        }
                                        else
                                        {
                                            GlobalUnlock (pMem);
                                            GlobalFree (hMem);
                                            hMem = NULL;
                                        }
                                    }
                                    else
                                    {
                                        GlobalFree (hMem);
                                        hMem = NULL;
                                    }
                                }
                            }
                            
                            if (hMem)
                            {
                                /* pMem is the address of the data. cb is the size of returned data. */
                                /* Verify the size of returned text. */
                                if (u32Format == VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
                                {
                                    cb = (lstrlenW((LPWSTR)pMem) + 1) * 2;
                                }
                                
                                GlobalUnlock (hMem);
                                
                                hMem = GlobalReAlloc (hMem, cb, 0);
                                dprintf(("hMem %p\n", hMem));
                                
                                if (hMem)
                                {
                                    /* 'hMem' contains the host clipboard data.
                                     * size is 'cb' and format is 'format'.
                                     */
                                    HANDLE hClip = SetClipboardData (format, hMem);
                                    dprintf(("WM_RENDERFORMAT hClip %p\n", hClip));

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
                        {
                            GlobalUnlock (hMem);
                        }
                    }
                    
                    if (hMem)
                    {
                        GlobalFree (hMem);
                    }
                }
                
                /* Something went wrong. */
                EmptyClipboard ();
            }
        } break;

        case WM_RENDERALLFORMATS:
        {
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
            /* Announce available formats. Do not insert data, they will be inserted in WM_RENDER*. */
            uint32_t u32Formats = (uint32_t)lParam;
            
            if (OpenClipboard (hwnd))
            {
                EmptyClipboard();
                
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

        case WM_USER + 1:
        {
            /* Send data in the specified format to the host. */
            uint32_t u32Formats = (uint32_t)lParam;

            HANDLE hClip = NULL;

            if (OpenClipboard (hwnd))
            {
                if (u32Formats & VBOX_SHARED_CLIPBOARD_FMT_BITMAP)
                {
                    hClip = GetClipboardData (CF_DIB);

                    if (hClip != NULL)
                    {
                        LPVOID lp = GlobalLock (hClip);

                        if (lp != NULL)
                        {
                            dprintf(("CF_DIB\n"));

                            vboxClipboardWriteData (pCtx, VBOX_SHARED_CLIPBOARD_FMT_BITMAP,
                                                    lp, GlobalSize (hClip));

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
                        LPWSTR uniString = (LPWSTR)GlobalLock (hClip);

                        if (uniString != NULL)
                        {
                            dprintf(("CF_UNICODETEXT\n"));

                            vboxClipboardWriteData (pCtx, VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT,
                                                    uniString, (lstrlenW (uniString) + 1) * 2);

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
                        hClip = GetClipboardData (format);

                        if (hClip != NULL)
                        {
                            LPVOID lp = GlobalLock (hClip);

                            if (lp != NULL)
                            {
                                dprintf(("CF_HTML\n"));

                                vboxClipboardWriteData (pCtx, VBOX_SHARED_CLIPBOARD_FMT_HTML,
                                                        lp, GlobalSize (hClip));

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
            
            if (hClip == NULL)
            {
                /* Requested clipboard format is not available, send empty data. */
                vboxClipboardWriteData (pCtx, 0, NULL, 0);
            }
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

    if (gCtx.pEnv)
    {
        /* Clipboard was already initialized. 2 or more instances are not supported. */
        return VERR_NOT_SUPPORTED;
    }

    memset (&gCtx, 0, sizeof (gCtx));

    gCtx.pEnv = pEnv;

    rc = vboxClipboardConnect (&gCtx);

    if (VBOX_SUCCESS (rc))
    {
        rc = vboxClipboardInit (&gCtx);

        dprintf (("vboxClipboardInit: rc = %d\n", rc));

        if (VBOX_SUCCESS (rc))
        {
            /* Always start the thread for host messages. */
            *pfStartThread = true;
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
    
    /* Open the new driver instance to not interfere with other callers. */
    HANDLE hDriver = CreateFile(VBOXGUEST_DEVICE_NAME,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
                                
    /* The thread waits for incoming messages from the host. */
    for (;;)
    {
        VBoxClipboardGetHostMsg parms;

        VBOX_INIT_CALL(&parms, VBOX_SHARED_CLIPBOARD_FN_GET_HOST_MSG, pCtx);

        VBoxHGCMParmUInt32Set (&parms.msg, 0);
        VBoxHGCMParmUInt32Set (&parms.formats, 0);

        DWORD cbReturned;

        if (!DeviceIoControl (hDriver,
                              IOCTL_VBOXGUEST_HGCM_CALL,
                              &parms, sizeof (parms),
                              &parms, sizeof (parms),
                              &cbReturned,
                              NULL))
        {
            dprintf(("Failed to call the driver for host message.\n"));

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
            uint32_t u32Msg;
            uint32_t u32Formats;

            rc = VBoxHGCMParmUInt32Get (&parms.msg, &u32Msg);

            if (VBOX_SUCCESS (rc))
            {
                rc = VBoxHGCMParmUInt32Get (&parms.formats, &u32Formats);

                if (VBOX_SUCCESS (rc))
                {
                    dprintf(("vboxClipboardHostEvent u32Msg %d, u32Formats %d\n", u32Msg, u32Formats));
    
                    switch (u32Msg)
                    {
                        case VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS:
                        {
                            /* The host has announced available clipboard formats. 
                             * Forward the information to the window, so it can later
                             * respond to WM_RENDERFORMAT message.
                             */
                            ::PostMessage (pCtx->hwnd, WM_USER, 0, u32Formats);
                        } break;
                        case VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA:
                        {
                            /* The host needs data in the specified format.
                             */
                            ::PostMessage (pCtx->hwnd, WM_USER + 1, 0, u32Formats);
                        } break;
                        case VBOX_SHARED_CLIPBOARD_HOST_MSG_QUIT:
                        {
                            /* The host is terminating.
                             */
                            rc = VERR_INTERRUPTED;
                        } break;
                        default:
                        {
                            dprintf(("Unsupported message from host!!!"));
                        }
                    }
                }
            }
        }
        
        if (rc == VERR_INTERRUPTED)
        {
            /* Wait for termination event. */
            WaitForSingleObject(pCtx->pEnv->hStopEvent, INFINITE);
            break;
        }
        
        if (VBOX_FAILURE (rc))
        {
            /* Wait a bit before retrying. */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
            {
                break;
            }

            continue;
        }

        dprintf(("processed host event rc = %d\n", rc));
    }
    
    CloseHandle (hDriver);

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
