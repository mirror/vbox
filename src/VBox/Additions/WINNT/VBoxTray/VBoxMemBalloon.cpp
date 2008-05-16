/** @file
 *
 * VBoxMemBalloon - Memory balloon notification
 *
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <psapi.h>
#include "VBoxTray.h"
#include "VBoxMemBalloon.h"
#include <VBoxDisplay.h>
#include <VBox/VBoxDev.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>
#include "helpers.h"
#include <winternl.h>

typedef struct _VBOXMEMBALLOONCONTEXT
{
    const VBOXSERVICEENV *pEnv;
    uint32_t              uMemBalloonSize;
} VBOXMEMBALLOONCONTEXT;


static VBOXMEMBALLOONCONTEXT gCtx = {0};


int VBoxMemBalloonInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    HANDLE gVBoxDriver = pEnv->hDriver;
    DWORD  cbReturned;

    dprintf(("VBoxMemBalloonInit\n"));

    gCtx.pEnv                   = pEnv;
    gCtx.uMemBalloonSize        = 0;

    /* Check balloon size */
    DWORD dwMemBalloonSize;
    if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_CTL_CHECK_BALLOON, NULL, 0, &dwMemBalloonSize, sizeof(dwMemBalloonSize), &cbReturned, NULL))
    {
        dprintf(("VBoxMemBalloonInit: new balloon size %d MB\n", dwMemBalloonSize));
        gCtx.uMemBalloonSize = dwMemBalloonSize;
    }
    else
        dprintf(("VBoxMemBalloonInit: DeviceIoControl (balloon) failed with %d\n", GetLastError()));

    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxMemBalloonDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    dprintf(("VBoxMemBalloonDestroy\n"));
    return;
}

uint32_t VBoxMemBalloonQuerySize()
{
    return gCtx.uMemBalloonSize;
}

/**
 * Thread function to wait for and process seamless mode change
 * requests
 */
unsigned __stdcall VBoxMemBalloonThread(void *pInstance)
{
    VBOXMEMBALLOONCONTEXT *pCtx = (VBOXMEMBALLOONCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxMemBalloonThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxMemBalloonThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            dprintf(("VBoxMemBalloonThread: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            dprintf(("VBoxMemBalloonThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_BALLOON_CHANGE_REQUEST)
            {
                DWORD dwMemBalloonSize;
                if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_CTL_CHECK_BALLOON, NULL, 0, &dwMemBalloonSize, sizeof(dwMemBalloonSize), &cbReturned, NULL))
                {
                    dprintf(("VBoxMemBalloonThread: new balloon size % MB\n", dwMemBalloonSize));
                    pCtx->uMemBalloonSize = dwMemBalloonSize;
                }
                else
                    dprintf(("VBoxMemBalloonThread: DeviceIoControl (balloon) failed with %d\n", GetLastError()));
            }
        } 
        else
        {
            dprintf(("VBoxMemBalloonThread: error 0 from DeviceIoControl IOCTL_VBOXGUEST_WAITEVENT\n"));

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
    maskInfo.u32NotMask = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxMemBalloonThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxMemBalloonThread: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxMemBalloonThread: finished mem balloon change request thread\n"));
    return 0;
}


