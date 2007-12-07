/** @file
 *
 * VBoxHook -- Global windows hook dll
 *
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
#include <windows.h>
#include <VBoxHook.h>
#include <stdio.h>

#pragma data_seg("SHARED") 
static HWINEVENTHOOK    hEventHook[2]    = {0}; 
#pragma data_seg() 
#pragma comment(linker, "/section:SHARED,RWS") 

static HANDLE   hNotifyEvent = 0;

#ifdef DEBUG
void WriteLog(char *String, ...);
#define dprintf(a) do { WriteLog a; } while (0)
#else
#define dprintf(a) do {} while (0)
#endif /* DEBUG */


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
        if (!hNotifyEvent)
        {
            hNotifyEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, VBOXHOOK_GLOBAL_EVENT_NAME);
            dprintf(("OpenEvent returned %x (last err=%x)\n", hNotifyEvent, GetLastError()));
        }
        BOOL ret = SetEvent(hNotifyEvent);
        dprintf(("SetEvent %x returned %d (last error %x)\n", hNotifyEvent, ret, GetLastError()));
        break;
    }
}


/* Install the global message hook */
BOOL VBoxInstallHook(HMODULE hDll)
{
    if (hEventHook[0] || hEventHook[1])
        return TRUE;

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
    return !!hEventHook[0];
}

/* Remove the global message hook */
BOOL VBoxRemoveHook()
{
    if (hEventHook[0] && hEventHook[1])
    {
        UnhookWinEvent(hEventHook[0]);
        UnhookWinEvent(hEventHook[1]);
        CoUninitialize();
    }
    hEventHook[0]  = hEventHook[1] = 0;
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
