/* $Id$ */
/** @file
 * VBoxSeamless - Seamless windows
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

#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxSeamless.h"
#include <VBoxHook.h>
#include <VBoxDisplay.h>
#include <VBox/VMMDev.h>
#include <iprt/assert.h>
#include <VBoxGuestInternal.h>

typedef struct _VBOXSEAMLESSCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    HMODULE    hModule;

    BOOL    (* pfnVBoxInstallHook)(HMODULE hDll);
    BOOL    (* pfnVBoxRemoveHook)();

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
        gCtx.hModule = LoadLibrary(VBOXHOOK_DLL_NAME);
        if (gCtx.hModule)
        {
            *(uintptr_t *)&gCtx.pfnVBoxInstallHook = (uintptr_t)GetProcAddress(gCtx.hModule, "VBoxInstallHook");
            *(uintptr_t *)&gCtx.pfnVBoxRemoveHook  = (uintptr_t)GetProcAddress(gCtx.hModule, "VBoxRemoveHook");

            /* Inform the host that we support the seamless window mode. */
            rc = VbglR3SetGuestCaps(VMMDEV_GUEST_SUPPORTS_SEAMLESS, 0);
            if (RT_SUCCESS(rc))
            {
                *pfStartThread = true;
                *ppInstance = &gCtx;
            }
            else
                Log(("VBoxTray: VBoxSeamlessInit: Failed to set seamless capability\n"));
        }
        else
        {
            rc = RTErrConvertFromWin32(GetLastError());
            Log(("VBoxTray: VBoxSeamlessInit: LoadLibrary of \"%s\" failed with rc=%Rrc\n", VBOXHOOK_DLL_NAME, rc));
        }
    }

    return rc;
}


void VBoxSeamlessDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    Log(("VBoxTray: VBoxSeamlessDestroy\n"));

    /* Inform the host that we no longer support the seamless window mode. */
    int rc = VbglR3SetGuestCaps(0, VMMDEV_GUEST_SUPPORTS_SEAMLESS);
    if (RT_FAILURE(rc))
        Log(("VBoxTray: VBoxSeamlessDestroy: Failed to unset seamless capability, rc=%Rrc\n", rc));

    if (gCtx.pfnVBoxRemoveHook)
        gCtx.pfnVBoxRemoveHook();
    if (gCtx.hModule)
        FreeLibrary(gCtx.hModule);
    gCtx.hModule = 0;
    return;
}

void VBoxSeamlessInstallHook()
{
    if (gCtx.pfnVBoxInstallHook)
    {
        /* Check current visible region state */
        VBoxSeamlessCheckWindows();

        gCtx.pfnVBoxInstallHook(gCtx.hModule);
    }
}

void VBoxSeamlessRemoveHook()
{
    if (gCtx.pfnVBoxRemoveHook)
        gCtx.pfnVBoxRemoveHook();

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
        rectVisible = rectWindow;

        char szWindowText[256];
        szWindowText[0] = 0;
        GetWindowText(hwnd, szWindowText, sizeof(szWindowText));

        /* Filter out Windows XP shadow windows */
        /** @todo still shows inside the guest */
        if (   szWindowText[0] == 0
            && dwStyle == (WS_POPUP|WS_VISIBLE|WS_CLIPSIBLINGS)
            && dwExStyle == (WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT|WS_EX_TOPMOST))
        {
            Log(("VBoxTray: Filter out shadow window style=%x exstyle=%x\n", dwStyle, dwExStyle));
            return TRUE;
        }

        /** @todo will this suffice? The Program Manager window covers the whole screen */
        if (strcmp(szWindowText, "Program Manager"))
        {
            Log(("VBoxTray: Enum hwnd=%x rect (%d,%d) (%d,%d)\n", hwnd, rectWindow.left, rectWindow.top, rectWindow.right, rectWindow.bottom));
            Log(("VBoxTray: title=%s style=%x exStyle=%x\n", szWindowText, dwStyle, dwExStyle));

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
                            PostMessage(ghwndToolWindow, WM_VBOX_REMOVE_SEAMLESS_HOOK, 0, 0);
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
                            PostMessage(ghwndToolWindow, WM_VBOX_INSTALL_SEAMLESS_HOOK, 0, 0);
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

