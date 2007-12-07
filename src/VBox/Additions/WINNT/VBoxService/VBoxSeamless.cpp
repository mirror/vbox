/** @file
 *
 * VBoxSeamless - Seamless windows
 *
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <windows.h>
#include "VBoxService.h"
#include "VBoxSeamless.h"
#include <VBoxHook.h>
#include <VBoxDisplay.h>
#include <VBox/VBoxDev.h>
#include <iprt/assert.h>
#include "helpers.h"

typedef struct _VBOXSEAMLESSCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    HMODULE    hModule;

    BOOL    (* pfnVBoxInstallHook)(HMODULE hDll);
    BOOL    (* pfnVBoxRemoveHook)();

    LPRGNDATA lpRgnData;
} VBOXSEAMLESSCONTEXT;

typedef struct
{
    HDC     hdc;
    HRGN    hrgn;
    RECT    rect;
} VBOX_ENUM_PARAM, *PVBOX_ENUM_PARAM;

static VBOXSEAMLESSCONTEXT gCtx = {0};

void VBoxLogString(HANDLE hDriver, char *pszStr);

int VBoxSeamlessInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    dprintf(("VBoxSeamlessInit\n"));

    *pfStartThread = false;
    gCtx.pEnv = pEnv;

    /* Will fail if SetWinEventHook is not present (version < NT4 SP6 apparently) */
    gCtx.hModule = LoadLibrary(VBOXHOOK_DLL_NAME);
    if (gCtx.hModule)
    {
        *(uintptr_t *)&gCtx.pfnVBoxInstallHook = (uintptr_t)GetProcAddress(gCtx.hModule, "VBoxInstallHook");
        *(uintptr_t *)&gCtx.pfnVBoxRemoveHook  = (uintptr_t)GetProcAddress(gCtx.hModule, "VBoxRemoveHook");

        /* inform the host that we support the seamless window mode */
        VMMDevReqGuestCapabilities vmmreqGuestCaps = {0};
        vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqGuestCaps, VMMDevReq_ReportGuestCapabilities);
        vmmreqGuestCaps.caps = VMMDEV_GUEST_SUPPORTS_SEAMLESS;

        DWORD cbReturned;
        if (!DeviceIoControl(pEnv->hDriver, IOCTL_VBOXGUEST_VMMREQUEST, &vmmreqGuestCaps, sizeof(vmmreqGuestCaps),
                             &vmmreqGuestCaps, sizeof(vmmreqGuestCaps), &cbReturned, NULL))
        {
            dprintf(("VMMDevReq_ReportGuestCapabilities: error doing IOCTL, last error: %d\n", GetLastError()));
            return VERR_INVALID_PARAMETER;
        }

        *pfStartThread = true;
        *ppInstance = &gCtx;
        return VINF_SUCCESS;
    }
    else
    {
        dprintf(("VBoxSeamlessInit LoadLibrary failed with %d\n", GetLastError()));
        return VERR_INVALID_PARAMETER;
    }

    return VINF_SUCCESS;
}


void VBoxSeamlessDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    dprintf(("VBoxSeamlessDestroy\n"));
    /* inform the host that we no longer support the seamless window mode */
    VMMDevReqGuestCapabilities vmmreqGuestCaps = {0};
    vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqGuestCaps, VMMDevReq_ReportGuestCapabilities);
    vmmreqGuestCaps.caps = 0;

    DWORD cbReturned;
    if (!DeviceIoControl(pEnv->hDriver, IOCTL_VBOXGUEST_VMMREQUEST, &vmmreqGuestCaps, sizeof(vmmreqGuestCaps),
                         &vmmreqGuestCaps, sizeof(vmmreqGuestCaps), &cbReturned, NULL))
    {
        dprintf(("VMMDevReq_ReportGuestCapabilities: error doing IOCTL, last error: %d\n", GetLastError()));
    }

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

    if (gCtx.lpRgnData)
    {
        free(gCtx.lpRgnData);
        gCtx.lpRgnData = NULL;
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

    dprintf(("VBoxEnumFunc %x\n", hwnd));
    /* Only visible windows that are present on the desktop are interesting here */
    if (    GetWindowRect(hwnd, &rectWindow)
        &&  IntersectRect(&rectVisible, &lpParam->rect, &rectWindow))
    {
        char szWindowText[256];
        szWindowText[0] = 0;
        GetWindowText(hwnd, szWindowText, sizeof(szWindowText));

        /* Filter out Windows XP shadow windows */
        /** @todo still shows inside the guest */
        if (   szWindowText[0] == 0
            && dwStyle == (WS_POPUP|WS_VISIBLE|WS_CLIPSIBLINGS)
            && dwExStyle == (WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT|WS_EX_TOPMOST))
        {
            dprintf(("Filter out shadow window style=%x exstyle=%x\n", dwStyle, dwExStyle));
            return TRUE;
        }

        /** @todo will this suffice? The Program Manager window covers the whole screen */
        if (strcmp(szWindowText, "Program Manager"))
        {
            dprintf(("Enum hwnd=%x rect (%d,%d) (%d,%d)\n", hwnd, rectWindow.left, rectWindow.top, rectWindow.right, rectWindow.bottom));
            dprintf(("title=%s style=%x exStyle=%x\n", szWindowText, dwStyle, dwExStyle));

            HRGN hrgn = CreateRectRgn(0,0,0,0);

            int ret = GetWindowRgn(hwnd, hrgn);

            if (ret == ERROR)
            {
                dprintf(("GetWindowRgn failed with rc=%d\n", GetLastError()));
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
            dprintf(("Enum hwnd=%x rect (%d,%d) (%d,%d) (ignored)\n", hwnd, rectWindow.left, rectWindow.top, rectWindow.right, rectWindow.bottom));
            dprintf(("title=%s style=%x\n", szWindowText, dwStyle));
        }
    }
    return TRUE; /* continue enumeration */
}

void VBoxSeamlessCheckWindows()
{
    VBOX_ENUM_PARAM param;

    param.hdc       = GetDC(HWND_DESKTOP);
    param.hrgn      = 0;

    GetWindowRect(GetDesktopWindow(), &param.rect);
    dprintf(("VBoxRecheckVisibleWindows desktop=%x rect (%d,%d) (%d,%d)\n", GetDesktopWindow(), param.rect.left, param.rect.top, param.rect.right, param.rect.bottom));
    EnumWindows(VBoxEnumFunc, (LPARAM)&param);

    if (param.hrgn)
    {
        DWORD cbSize;

        cbSize = GetRegionData(param.hrgn, 0, NULL);
        if (cbSize)
        {
            LPRGNDATA lpRgnData = (LPRGNDATA)malloc(cbSize);
            memset(lpRgnData, 0, cbSize);
            if (lpRgnData)
            {
                cbSize = GetRegionData(param.hrgn, cbSize, lpRgnData);
                if (cbSize)
                {
#ifdef DEBUG
                    RECT *lpRect = (RECT *)&lpRgnData->Buffer[0];
                    dprintf(("New visible region: \n"));

                    for (DWORD i=0;i<lpRgnData->rdh.nCount;i++)
                    {
                        dprintf(("visible rect (%d,%d)(%d,%d)\n", lpRect[i].left, lpRect[i].top, lpRect[i].right, lpRect[i].bottom));
                    }
#endif
                    if (    !gCtx.lpRgnData 
                        ||  (gCtx.lpRgnData->rdh.dwSize + gCtx.lpRgnData->rdh.nRgnSize != cbSize)
                        ||  memcmp(gCtx.lpRgnData, lpRgnData, cbSize))
                    {
                        /* send to display driver */
                        ExtEscape(param.hdc, VBOXESC_SETVISIBLEREGION, cbSize, (LPCSTR)lpRgnData, 0, NULL);

                        if (gCtx.lpRgnData)
                            free(gCtx.lpRgnData);
                        gCtx.lpRgnData = lpRgnData;
                    }
                    else
                        dprintf(("Visible rectangles haven't changed; ignore\n"));
                }
                if (lpRgnData != gCtx.lpRgnData)
                    free(lpRgnData);
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
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxSeamlessThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxSeamlessThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            dprintf(("VBoxSeamlessThread: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            dprintf(("VBoxSeamlessThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST)
            {
                dprintf(("VBoxService: going to get seamless change information.\n"));

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

                    BOOL fSeamlessChangeQueried = DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &seamlessChangeRequest, sizeof(seamlessChangeRequest),
                                                                 &seamlessChangeRequest, sizeof(seamlessChangeRequest), &cbReturned, NULL);
                    if (fSeamlessChangeQueried)
                    {
                        dprintf(("VBoxSeamlessThread: mode change to %d\n", seamlessChangeRequest.mode));

                        switch(seamlessChangeRequest.mode)
                        {
                        case VMMDev_Seamless_Disabled:
                            if (fWasScreenSaverActive)
                            {
                                dprintf(("Re-enabling the screensaver\n"));
                                ret = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE, NULL, 0);
                                if (!ret)
                                    dprintf(("SystemParametersInfo SPI_SETSCREENSAVEACTIVE failed with %d\n", GetLastError()));
                            }
                            PostMessage(gToolWindow, WM_VBOX_REMOVE_SEAMLESS_HOOK, 0, 0);
                            break;

                        case VMMDev_Seamless_Visible_Region:
                            ret = SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &fWasScreenSaverActive, 0);
                            if (!ret)
                                dprintf(("SystemParametersInfo SPI_GETSCREENSAVEACTIVE failed with %d\n", GetLastError()));

                            if (fWasScreenSaverActive)
                                dprintf(("Disabling the screensaver\n"));

                            ret = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);
                            if (!ret)
                                dprintf(("SystemParametersInfo SPI_SETSCREENSAVEACTIVE failed with %d\n", GetLastError()));
                            PostMessage(gToolWindow, WM_VBOX_INSTALL_SEAMLESS_HOOK, 0, 0);
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
                        dprintf(("VBoxSeamlessThread: error from DeviceIoControl IOCTL_VBOXGUEST_VMMREQUEST\n"));
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
            dprintf(("VBoxService: error 0 from DeviceIoControl IOCTL_VBOXGUEST_WAITEVENT\n"));
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
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxSeamlessThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxSeamlessThread: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxSeamlessThread: finished seamless change request thread\n"));
    return 0;
}


