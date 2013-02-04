/* $Id$ */
/** @file
 * VBoxRestore - Restore notification.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxRestore.h"
#include <VBoxDisplay.h>
#include <VBox/VMMDev.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>


typedef struct _VBOXRESTORECONTEXT
{
    const VBOXSERVICEENV *pEnv;

    DWORD  fRDPState;
} VBOXRESTORECONTEXT;


static VBOXRESTORECONTEXT gCtx = {0};


int VBoxRestoreInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    Log(("VBoxTray: VBoxRestoreInit\n"));

    gCtx.pEnv      = pEnv;
    gCtx.fRDPState = ERROR_NOT_SUPPORTED;

    VBoxRestoreCheckVRDP();

    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxRestoreDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    Log(("VBoxTray: VBoxRestoreDestroy\n"));
    return;
}

void VBoxRestoreSession()
{
    VBoxRestoreCheckVRDP();
}

void VBoxRestoreCheckVRDP()
{
    VBOXDISPIFESCAPE escape = {0};
    escape.escapeCode = VBOXESC_ISVRDPACTIVE;
    /* Check VRDP activity. */

    /* Send to display driver. */
    DWORD dwRet = VBoxDispIfEscape(&gCtx.pEnv->dispIf, &escape, 0);
    Log(("VBoxTray: VBoxRestoreCheckVRDP -> VRDP activate state = %d\n", dwRet));

    if (dwRet != gCtx.fRDPState)
    {
        DWORD cbReturnIgnored;
        if (!DeviceIoControl(gCtx.pEnv->hDriver,
                             dwRet == NO_ERROR
                             ? VBOXGUEST_IOCTL_ENABLE_VRDP_SESSION
                             : VBOXGUEST_IOCTL_DISABLE_VRDP_SESSION,
                             NULL, 0, NULL, 0, &cbReturnIgnored, NULL))
        {
            Log(("VBoxTray: VBoxRestoreCheckVRDP: DeviceIOControl failed, error = %ld\n", GetLastError()));
        }
        gCtx.fRDPState = dwRet;
    }
}

/**
 * Thread function to wait for and process seamless mode change
 * requests
 */
unsigned __stdcall VBoxRestoreThread(void *pInstance)
{
    VBOXRESTORECONTEXT *pCtx = (VBOXRESTORECONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_RESTORED;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxTray: VBoxRestoreThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        Log(("VBoxTray: VBoxRestoreThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_RESTORED;
        if (DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            Log(("VBoxTray: VBoxRestoreThread: DeviceIOControl succeeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            Log(("VBoxTray: VBoxRestoreThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_RESTORED)
                PostMessage(ghwndToolWindow, WM_VBOXTRAY_VM_RESTORED, 0, 0);
            else
                /** @todo Don't poll, but wait for connect/disconnect events */
                PostMessage(ghwndToolWindow, WM_VBOXTRAY_VRDP_CHECK, 0, 0);
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
    maskInfo.u32NotMask = VMMDEV_EVENT_RESTORED;
    if (DeviceIoControl (gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxTray: VBoxRestoreThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        Log(("VBoxTray: VBoxRestoreThread: DeviceIOControl(CtlMask) failed\n"));
    }

    Log(("VBoxTray: VBoxRestoreThread: finished seamless change request thread\n"));
    return 0;
}

