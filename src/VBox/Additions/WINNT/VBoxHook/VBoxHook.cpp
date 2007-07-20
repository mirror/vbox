/** @file
 *
 * VBoxHook -- Global windows hook dll
 *
 * Copyright (C) 2006-2007 innotek GmbH
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
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <VBoxHook.h>
#include <VBoxDisplay.h>
#include <stdio.h>

#pragma data_seg("SHARED")
static HWINEVENTHOOK    hEventHook[2] = {0};
#pragma data_seg()
#pragma comment(linker, "/section:SHARED,RWS")

static void VBoxRecheckVisibleWindows();

#ifdef DEBUG
void WriteLog(char *String, ...);
#define dprintf(a) do { WriteLog a; } while (0)
#else
#define dprintf(a) do {} while (0)
#endif /* DEBUG */

typedef struct
{
    HDC     hdc;
    HRGN    hrgn;
    RECT    rect;
} VBOX_ENUM_PARAM, *PVBOX_ENUM_PARAM;


void CALLBACK VBoxHandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, 
                                 LONG idObject, LONG idChild, 
                                 DWORD dwEventThread, DWORD dwmsEventTime)
{
    DWORD dwStyle;
    if (    idObject != OBJID_WINDOW
        ||  !hwnd)
        return;

    dwStyle  = GetWindowLong(hwnd, GWL_STYLE);
    if (dwStyle & WS_CHILD)
        return;

    switch(event)
    {
    case EVENT_OBJECT_LOCATIONCHANGE:
        if (!(dwStyle & WS_VISIBLE))
            return;

    case EVENT_OBJECT_CREATE:
    case EVENT_OBJECT_DESTROY:
    case EVENT_OBJECT_HIDE:
    case EVENT_OBJECT_SHOW:
#ifdef DEBUG
        switch(event)
        {
        case EVENT_OBJECT_LOCATIONCHANGE:
            dprintf(("VBoxHandleWinEvent EVENT_OBJECT_LOCATIONCHANGE for window %x\n", hwnd));
            break;
        case EVENT_OBJECT_CREATE:
            dprintf(("VBoxHandleWinEvent EVENT_OBJECT_CREATE for window %x\n", hwnd));
            break;
        case EVENT_OBJECT_HIDE:
            dprintf(("VBoxHandleWinEvent EVENT_OBJECT_HIDE for window %x\n", hwnd));
            break;
        case EVENT_OBJECT_SHOW:
            dprintf(("VBoxHandleWinEvent EVENT_OBJECT_SHOW for window %x\n", hwnd));
            break;
        case EVENT_OBJECT_DESTROY:
            dprintf(("VBoxHandleWinEvent EVENT_OBJECT_DESTROY for window %x\n", hwnd));
            break;
        }
#endif
        VBoxRecheckVisibleWindows();
        break;
    }
}


BOOL CALLBACK VBoxEnumFunc(HWND hwnd, LPARAM lParam)
{
    PVBOX_ENUM_PARAM    lpParam = (PVBOX_ENUM_PARAM)lParam;
    DWORD               dwStyle, dwExStyle;
    RECT                rect, rectVisible;

    dwStyle   = GetWindowLong(hwnd, GWL_STYLE);
    dwExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (   !(dwStyle & WS_VISIBLE)
        ||  (dwStyle & WS_CHILD))
        return TRUE;

    dprintf(("VBoxEnumFunc %x\n", hwnd));
    /* Only visible windows that are present on the desktop are interesting here */
    if (    GetWindowRect(hwnd, &rect)
        &&  IntersectRect(&rectVisible, &lpParam->rect, &rect))
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
            dprintf(("Enum hwnd=%x rect (%d,%d) (%d,%d)\n", hwnd, rect.left, rect.top, rect.right, rect.bottom));
            dprintf(("title=%s style=%x\n", szWindowText, dwStyle));

            HRGN hrgn = CreateRectRgn(0,0,0,0);

            int ret = GetWindowRgn(hwnd, hrgn);

            if (ret == ERROR)
            {
                dprintf(("GetWindowRgn failed with rc=%d\n", GetLastError()));
                SetRectRgn(hrgn, rectVisible.left, rectVisible.top, rectVisible.right, rectVisible.bottom);
            }
            else
                /* this region is relative to the window origin instead of the desktop origin */
                OffsetRgn(hrgn, rectVisible.left, rectVisible.top);

            if (lpParam->hrgn)
            {
                /* create a union of the current visible region and the visible rectangle of this window. */
                CombineRgn(lpParam->hrgn, lpParam->hrgn, hrgn, RGN_OR);
                DeleteObject(hrgn);
            }
            else
                lpParam->hrgn = hrgn;
        }
    }
    return TRUE; /* continue enumeration */
}

void VBoxRecheckVisibleWindows()
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
            LPRGNDATA lpRgnData = (LPRGNDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbSize);

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
                    /* send to display driver */
                    ExtEscape(param.hdc, VBOXESC_SETVISIBLEREGION, cbSize, (LPCSTR)lpRgnData, 0, NULL);
                }
                HeapFree(GetProcessHeap(), 0, lpRgnData);
            }
        }

        DeleteObject(param.hrgn);
    }

    ReleaseDC(HWND_DESKTOP, param.hdc);
}


/* Install the global message hook */
BOOL VBoxInstallHook(HMODULE hDll)
{
    /* Check current visible region state */
    VBoxRecheckVisibleWindows();

    CoInitialize(NULL);
    hEventHook[0] = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
                                    hDll,
                                    VBoxHandleWinEvent,
                                    0, 0,
                                    WINEVENT_INCONTEXT | WINEVENT_SKIPOWNPROCESS);

    hEventHook[1] = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE,
                                    hDll,
                                    VBoxHandleWinEvent,
                                    0, 0,
                                    WINEVENT_INCONTEXT | WINEVENT_SKIPOWNPROCESS);
    return !!hEventHook;
}

/* Remove the global message hook */
BOOL VBoxRemoveHook()
{
    UnhookWinEvent(hEventHook[0]);
    UnhookWinEvent(hEventHook[1]);
    CoUninitialize();
    return true;
}


#ifdef DEBUG
#include <VBox/VBoxGuest.h>

static char LogBuffer[1024];
static HANDLE gVBoxDriver = INVALID_HANDLE_VALUE;

VBGLR3DECL(int) VbglR3GRPerform(VMMDevRequestHeader *pReq)
{
    DWORD cbReturned;
    DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, pReq, pReq->size,
                    pReq, pReq->size, &cbReturned, NULL);
    return VINF_SUCCESS;
}

void WriteLog(char *pszStr, ...)
{
    VMMDevReqLogString *pReq = (VMMDevReqLogString *)LogBuffer;
    int rc;

    /* open VBox guest driver */
    if (gVBoxDriver == INVALID_HANDLE_VALUE)
        gVBoxDriver = CreateFile(VBOXGUEST_DEVICE_NAME,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                             NULL);

    if (gVBoxDriver == INVALID_HANDLE_VALUE)
        return;

    va_list va;

    va_start(va, pszStr);

    vmmdevInitRequest(&pReq->header, VMMDevReq_LogString);
    vsprintf(pReq->szString, pszStr, va);
    pReq->header.size += strlen(pReq->szString);
    rc = VbglR3GRPerform(&pReq->header);

    va_end (va);
    return;
}

#endif
