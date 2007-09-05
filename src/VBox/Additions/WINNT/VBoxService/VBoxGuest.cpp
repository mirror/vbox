/** @file
 *
 * VBoxGuest - Guest management notification
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
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include "VBoxService.h"
#include "VBoxGuest.h"
#include <VBoxDisplay.h>
#include <VBox/VBoxDev.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>
#include "helpers.h"
#include <winternl.h>

typedef struct _VBOXGUESTCONTEXT
{
    const VBOXSERVICEENV *pEnv;
    uint32_t              uStatInterval;

    NTSTATUS (WINAPI *pfnNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
} VBOXGUESTCONTEXT;


static VBOXGUESTCONTEXT gCtx = {0};


int VBoxGuestInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    HANDLE gVBoxDriver = pEnv->hDriver;
    DWORD  cbReturned;

    dprintf(("VBoxGuestInit\n"));

    gCtx.pEnv          = pEnv;
    gCtx.uStatInterval = 0;     /* default */

    VMMDevGetStatisticsChangeRequest req;
    vmmdevInitRequest(&req.header, VMMDevReq_GetStatisticsChangeRequest);
    req.eventAck = 0;

    if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &req, req.header.size, &req, req.header.size, &cbReturned, NULL))
    {
        dprintf(("VBoxGuestThread: new statistics interval %d seconds\n", req.u32StatInterval));
        gCtx.uStatInterval = req.u32StatInterval * 1000;
    }
    else
        dprintf(("VBoxGuestThread: DeviceIoControl failed with %d\n", GetLastError()));

    HMODULE hMod = LoadLibrary("NTDLL.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnNtQuerySystemInformation = (uintptr_t)GetProcAddress(hMod, "NtQuerySystemInformation");
        if (gCtx.pfnNtQuerySystemInformation)
            dprintf(("gCtx.pfnNtQuerySystemInformation = %x\n", gCtx.pfnNtQuerySystemInformation));
        else
            dprintf(("NTDLL.NtQuerySystemInformation not found!!\n"));
    }

    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxGuestDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    dprintf(("VBoxGuestDestroy\n"));
    return;
}

/**
 * Thread function to wait for and process seamless mode change
 * requests
 */
unsigned __stdcall VBoxGuestThread(void *pInstance)
{
    VBOXGUESTCONTEXT *pCtx = (VBOXGUESTCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxGuestThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxGuestThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = (pCtx->uStatInterval) ? pCtx->uStatInterval : 1000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST | VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            dprintf(("VBoxGuestThread: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            dprintf(("VBoxGuestThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_BALLOON_CHANGE_REQUEST)
            {
                DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_CTL_CHECK_BALLOON, NULL, 0, NULL, 0, NULL, NULL);
            }
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST)
            {
                VMMDevGetStatisticsChangeRequest req;
                vmmdevInitRequest(&req.header, VMMDevReq_GetStatisticsChangeRequest);
                req.eventAck = VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;

                if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &req, req.header.size, &req, req.header.size, &cbReturned, NULL))
                {
                    dprintf(("VBoxGuestThread: new statistics interval %d seconds\n", req.u32StatInterval));
                    pCtx->uStatInterval = req.u32StatInterval * 1000;
                }
                else
                    dprintf(("VBoxGuestThread: DeviceIoControl failed with %d\n", GetLastError()));

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
        if (    gCtx.uStatInterval 
            &&  gCtx.pfnNtQuerySystemInformation)
        {
            SYSTEM_INFO systemInfo;

            /* Query and report guest statistics */
            GetSystemInfo(&systemInfo);

            //gCtx.pfnNtQuerySystemInformation(
        }
    } 
    while (!fTerminate);

    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST | VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxGuestThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxGuestThread: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxGuestThread: finished seamless change request thread\n"));
    return 0;
}


