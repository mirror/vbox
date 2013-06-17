/* $Id$ */
/** @file
 * VBoxSeamless - Seamless windows
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxSeamless.h"
#include <VBoxHook.h>
#include <VBoxDisplay.h>
#include <VBox/VMMDev.h>
#include <iprt/assert.h>
#include <iprt/ldr.h>
#include <VBoxGuestInternal.h>

typedef struct _VBOXSEAMLESSCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    RTLDRMOD hModHook;

    BOOL    (* pfnVBoxHookInstallWindowTracker)(HMODULE hDll);
    BOOL    (* pfnVBoxHookRemoveWindowTracker)();

    PVBOXDISPIFESCAPE lpEscapeData;
} VBOXSEAMLESSCONTEXT;

typedef struct
{
    HDC     hdc;
    HRGN    hrgn;
} VBOX_ENUM_PARAM, *PVBOX_ENUM_PARAM;

static VBOXSEAMLESSCONTEXT gCtx = {0};

void VBoxLogString(HANDLE hDriver, char *pszStr);

int VBoxSeamlessInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    Log(("VBoxTray: VBoxSeamlessInit\n"));

    *pfStartThread = false;
    gCtx.pEnv = pEnv;
    gCtx.hModHook = NIL_RTLDRMOD;

    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
    GetVersionEx (&OSinfo);

    int rc = VINF_SUCCESS;

    /* We have to jump out here when using NT4, otherwise it complains about
       a missing API function "UnhookWinEvent" used by the dynamically loaded VBoxHook.dll below */
    if (OSinfo.dwMajorVersion <= 4)         /* Windows NT 4.0 or older */
    {
        Log(("VBoxTray: VBoxSeamlessInit: Windows NT 4.0 or older not supported!\n"));
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        /* Will fail if SetWinEventHook is not present (version < NT4 SP6 apparently) */
        rc = RTLdrLoadAppPriv(VBOXHOOK_DLL_NAME, &gCtx.hModHook);
        if (RT_SUCCESS(rc))
        {
            *(PFNRT *)&gCtx.pfnVBoxHookInstallWindowTracker = RTLdrGetFunction(gCtx.hModHook, "VBoxHookInstallWindowTracker");
            *(PFNRT *)&gCtx.pfnVBoxHookRemoveWindowTracker  = RTLdrGetFunction(gCtx.hModHook, "VBoxHookRemoveWindowTracker");

            /* rc should contain success status */
            AssertRC(rc);

            VBoxSeamlessSetSupported(TRUE);

//            if (RT_SUCCESS(rc))
            {
                *pfStartThread = true;
                *ppInstance = &gCtx;
            }
        }
        else
            Log(("VBoxTray: VBoxSeamlessInit: LoadLibrary of \"%s\" failed with rc=%Rrc\n", VBOXHOOK_DLL_NAME, rc));
    }

    return rc;
}


void VBoxSeamlessDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    Log(("VBoxTray: VBoxSeamlessDestroy\n"));

    VBoxSeamlessSetSupported(FALSE);

    /* Inform the host that we no longer support the seamless window mode. */
    if (gCtx.pfnVBoxHookRemoveWindowTracker)
        gCtx.pfnVBoxHookRemoveWindowTracker();
    if (gCtx.hModHook != NIL_RTLDRMOD)
    {
        RTLdrClose(gCtx.hModHook);
        gCtx.hModHook = NIL_RTLDRMOD;
    }
    return;
}

void VBoxSeamlessInstallHook()
{
    if (gCtx.pfnVBoxHookInstallWindowTracker)
    {
        /* Check current visible region state */
        VBoxSeamlessCheckWindows();

        HMODULE hMod = (HMODULE)RTLdrGetNativeHandle(gCtx.hModHook);
        Assert(hMod != (HMODULE)~(uintptr_t)0);
        gCtx.pfnVBoxHookInstallWindowTracker(hMod);
    }
}

void VBoxSeamlessRemoveHook()
{
    if (gCtx.pfnVBoxHookRemoveWindowTracker)
        gCtx.pfnVBoxHookRemoveWindowTracker();

    if (gCtx.lpEscapeData)
    {
        free(gCtx.lpEscapeData);
        gCtx.lpEscapeData = NULL;
    }
}

BOOL CALLBACK VBoxEnumFunc(HWND hwnd, LPARAM lParam)
{
    PVBOX_ENUM_PARAM    lpParam = (PVBOX_ENUM_PARAM)lParam;
    DWORD               dwStyle, dwExStyle;
    RECT                rectWindow, rectVisible;

    dwStyle   = GetWindowLong(hwnd, GWL_STYLE);
    dwExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (   !(dwStyle & WS_VISIBLE)
        ||  (dwStyle & WS_CHILD))
        return TRUE;

    Log(("VBoxTray: VBoxEnumFunc %x\n", hwnd));
    /* Only visible windows that are present on the desktop are interesting here */
    if (GetWindowRect(hwnd, &rectWindow))
    {
        char szWindowText[256];
        szWindowText[0] = 0;
        OSVERSIONINFO OSinfo;
        HWND hStart = NULL;
        GetWindowText(hwnd, szWindowText, sizeof(szWindowText));
        OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
        GetVersionEx (&OSinfo);
        if (OSinfo.dwMajorVersion >= 6)
        {
            hStart = ::FindWindowEx(GetDesktopWindow(), NULL, "Button", "Start");
            if (  hwnd == hStart && szWindowText != NULL
                && !(strcmp(szWindowText, "Start"))
               )
            {
                /* for vista and above. To solve the issue of small bar above
                 * the Start button when mouse is hovered over the start button in seamless mode.
                 * Difference of 7 is observed in Win 7 platform between the dimensionsof rectangle with Start title and its shadow.
                 */
                rectWindow.top += 7;
                rectWindow.bottom -=7;
            }
        }
        rectVisible = rectWindow;

#ifdef LOG_ENABLED
        DWORD pid = 0;
        DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
#endif

        /* Filter out Windows XP shadow windows */
        /** @todo still shows inside the guest */
        if (   szWindowText[0] == 0
            && (
                    (dwStyle == (WS_POPUP|WS_VISIBLE|WS_CLIPSIBLINGS)
                            && dwExStyle == (WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT|WS_EX_TOPMOST))
                 || (dwStyle == (WS_POPUP|WS_VISIBLE|WS_DISABLED|WS_CLIPSIBLINGS|WS_CLIPCHILDREN)
                            && dwExStyle == (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE))
                 || (dwStyle == (WS_POPUP|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN)
                                       && dwExStyle == (WS_EX_TOOLWINDOW))
                            ))
        {
            Log(("VBoxTray: Filter out shadow window style=%x exstyle=%x\n", dwStyle, dwExStyle));
            Log(("VBoxTray: Enum hwnd=%x rect (%d,%d) (%d,%d) (filtered)\n", hwnd, rectWindow.left, rectWindow.top, rectWindow.right, rectWindow.bottom));
            Log(("VBoxTray: title=%s style=%x exStyle=%x\n", szWindowText, dwStyle, dwExStyle));
            Log(("VBoxTray: pid=%d tid=%d\n", pid, tid));
            return TRUE;
        }

        /** @todo will this suffice? The Program Manager window covers the whole screen */
        if (strcmp(szWindowText, "Program Manager"))
        {
            Log(("VBoxTray: Enum hwnd=%x rect (%d,%d) (%d,%d) (applying)\n", hwnd, rectWindow.left, rectWindow.top, rectWindow.right, rectWindow.bottom));
            Log(("VBoxTray: title=%s style=%x exStyle=%x\n", szWindowText, dwStyle, dwExStyle));
            Log(("VBoxTray: pid=%d tid=%d\n", pid, tid));

            HRGN hrgn = CreateRectRgn(0,0,0,0);

            int ret = GetWindowRgn(hwnd, hrgn);

            if (ret == ERROR)
            {
                Log(("VBoxTray: GetWindowRgn failed with rc=%d\n", GetLastError()));
                SetRectRgn(hrgn, rectVisible.left, rectVisible.top, rectVisible.right, rectVisible.bottom);
            }
            else
            {
                /* this region is relative to the window origin instead of the desktop origin */
                OffsetRgn(hrgn, rectWindow.left, rectWindow.top);
            }
            if (lpParam->hrgn)
            {
                /* create a union of the current visible region and the visible rectangle of this window. */
                CombineRgn(lpParam->hrgn, lpParam->hrgn, hrgn, RGN_OR);
                DeleteObject(hrgn);
            }
            else
                lpParam->hrgn = hrgn;
        }
        else
        {
            Log(("VBoxTray: Enum hwnd=%x rect (%d,%d) (%d,%d) (ignored)\n", hwnd, rectWindow.left, rectWindow.top, rectWindow.right, rectWindow.bottom));
            Log(("VBoxTray: title=%s style=%x\n", szWindowText, dwStyle));
            Log(("VBoxTray: pid=%d tid=%d\n", pid, tid));
        }
    }
    return TRUE; /* continue enumeration */
}

void VBoxSeamlessCheckWindows()
{
    VBOX_ENUM_PARAM param;

    param.hdc       = GetDC(HWND_DESKTOP);
    param.hrgn      = 0;

    EnumWindows(VBoxEnumFunc, (LPARAM)&param);

    if (param.hrgn)
    {
        DWORD cbSize;

        cbSize = GetRegionData(param.hrgn, 0, NULL);
        if (cbSize)
        {
            PVBOXDISPIFESCAPE lpEscapeData = (PVBOXDISPIFESCAPE)malloc(VBOXDISPIFESCAPE_SIZE(cbSize));
            if (lpEscapeData)
            {
                lpEscapeData->escapeCode = VBOXESC_SETVISIBLEREGION;
                LPRGNDATA lpRgnData = VBOXDISPIFESCAPE_DATA(lpEscapeData, RGNDATA);
                memset(lpRgnData, 0, cbSize);
                cbSize = GetRegionData(param.hrgn, cbSize, lpRgnData);
                if (cbSize)
                {
#ifdef DEBUG
                    RECT *lpRect = (RECT *)&lpRgnData->Buffer[0];
                    Log(("VBoxTray: New visible region: \n"));

                    for (DWORD i=0;i<lpRgnData->rdh.nCount;i++)
                    {
                        Log(("VBoxTray: visible rect (%d,%d)(%d,%d)\n", lpRect[i].left, lpRect[i].top, lpRect[i].right, lpRect[i].bottom));
                    }
#endif
                    LPRGNDATA lpCtxRgnData = VBOXDISPIFESCAPE_DATA(gCtx.lpEscapeData, RGNDATA);
                    if (    !gCtx.lpEscapeData
                        ||  (lpCtxRgnData->rdh.dwSize + lpCtxRgnData->rdh.nRgnSize != cbSize)
                        ||  memcmp(lpCtxRgnData, lpRgnData, cbSize))
                    {
                        /* send to display driver */
                        VBoxDispIfEscape(&gCtx.pEnv->dispIf, lpEscapeData, cbSize);

                        if (gCtx.lpEscapeData)
                            free(gCtx.lpEscapeData);
                        gCtx.lpEscapeData = lpEscapeData;
                    }
                    else
                        Log(("VBoxTray: Visible rectangles haven't changed; ignore\n"));
                }
                if (lpEscapeData != gCtx.lpEscapeData)
                    free(lpEscapeData);
            }
        }

        DeleteObject(param.hrgn);
    }

    ReleaseDC(HWND_DESKTOP, param.hdc);
}

/**
 * Thread function to wait for and process seamless mode change
 * requests
 */
unsigned __stdcall VBoxSeamlessThread(void *pInstance)
{
    VBOXSEAMLESSCONTEXT *pCtx = (VBOXSEAMLESSCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;
    BOOL fWasScreenSaverActive = FALSE, ret;

    maskInfo.u32OrMask = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxTray: VBoxSeamlessThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        Log(("VBoxTray: VBoxSeamlessThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
        if (DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            Log(("VBoxTray: VBoxSeamlessThread: DeviceIOControl succeeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            Log(("VBoxTray: VBoxSeamlessThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST)
            {
                Log(("VBoxTray: VBoxTray: going to get seamless change information\n"));

                /* We got at least one event. Read the requested resolution
                 * and try to set it until success. New events will not be seen
                 * but a new resolution will be read in this poll loop.
                 */
                for (;;)
                {
                    /* get the seamless change request */
                    VMMDevSeamlessChangeRequest seamlessChangeRequest = {0};
                    vmmdevInitRequest((VMMDevRequestHeader*)&seamlessChangeRequest, VMMDevReq_GetSeamlessChangeRequest);
                    seamlessChangeRequest.eventAck = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;

                    BOOL fSeamlessChangeQueried = DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(seamlessChangeRequest)), &seamlessChangeRequest, sizeof(seamlessChangeRequest),
                                                                 &seamlessChangeRequest, sizeof(seamlessChangeRequest), &cbReturned, NULL);
                    if (fSeamlessChangeQueried)
                    {
                        Log(("VBoxTray: VBoxSeamlessThread: mode change to %d\n", seamlessChangeRequest.mode));

                        switch(seamlessChangeRequest.mode)
                        {
                        case VMMDev_Seamless_Disabled:
                            if (fWasScreenSaverActive)
                            {
                                Log(("VBoxTray: Re-enabling the screensaver\n"));
                                ret = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, NULL, 0);
                                if (!ret)
                                    Log(("VBoxTray: SystemParametersInfo SPI_SETSCREENSAVEACTIVE failed with %d\n", GetLastError()));
                            }
                            PostMessage(ghwndToolWindow, WM_VBOX_SEAMLESS_DISABLE, 0, 0);
                            break;

                        case VMMDev_Seamless_Visible_Region:
                            ret = SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &fWasScreenSaverActive, 0);
                            if (!ret)
                                Log(("VBoxTray: SystemParametersInfo SPI_GETSCREENSAVEACTIVE failed with %d\n", GetLastError()));

                            if (fWasScreenSaverActive)
                                Log(("VBoxTray: Disabling the screensaver\n"));

                            ret = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
                            if (!ret)
                                Log(("VBoxTray: SystemParametersInfo SPI_SETSCREENSAVEACTIVE failed with %d\n", GetLastError()));
                            PostMessage(ghwndToolWindow, WM_VBOX_SEAMLESS_ENABLE, 0, 0);
                            break;

                        case VMMDev_Seamless_Host_Window:
                            break;

                        default:
                            AssertFailed();
                            break;
                        }
                        break;
                    }
                    else
                    {
                        Log(("VBoxTray: VBoxSeamlessThread: error from DeviceIoControl VBOXGUEST_IOCTL_VMMREQUEST\n"));
                    }
                    /* sleep a bit to not eat too much CPU while retrying */
                    /* are we supposed to stop? */
                    if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 50) == WAIT_OBJECT_0)
                    {
                        fTerminate = true;
                        break;
                    }
                }
            }
        }
        else
        {
            Log(("VBoxTray: VBoxTray: error 0 from DeviceIoControl VBOXGUEST_IOCTL_WAITEVENT\n"));
            /* sleep a bit to not eat too much CPU in case the above call always fails */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 10) == WAIT_OBJECT_0)
            {
                fTerminate = true;
                break;
            }
        }
    }
    while (!fTerminate);

    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
    if (DeviceIoControl (gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxTray: VBoxSeamlessThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        Log(("VBoxTray: VBoxSeamlessThread: DeviceIOControl(CtlMask) failed\n"));
    }

    Log(("VBoxTray: VBoxSeamlessThread: finished seamless change request thread\n"));
    return 0;
}

