/** @file
 *
 * VBoxRestore - Restore notification
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
#include "VBoxTray.h"
#include "VBoxRestore.h"
#include <VBoxDisplay.h>
#include <VBox/VBoxDev.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>
#include "helpers.h"

typedef struct _VBOXRESTORECONTEXT
{
    const VBOXSERVICEENV *pEnv;

    BOOL  fRDPState;
} VBOXRESTORECONTEXT;


static VBOXRESTORECONTEXT gCtx = {0};


int VBoxRestoreInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    dprintf(("VBoxRestoreInit\n"));

    gCtx.pEnv      = pEnv;
    gCtx.fRDPState = FALSE;

    VBoxRestoreCheckVRDP();

    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxRestoreDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    dprintf(("VBoxRestoreDestroy\n"));
    return;
}

void VBoxRestoreSession()
{
    VBoxRestoreCheckVRDP();
}

void VBoxRestoreCheckVRDP()
{
    HDC  hdc;
    BOOL ret;
    
    /* Check VRDP activity */
    hdc = GetDC(HWND_DESKTOP);

    /* send to display driver */
    ret = ExtEscape(hdc, VBOXESC_ISVRDPACTIVE, 0, NULL, 0, NULL);
    dprintf(("VBoxRestoreSession -> VRDP activate state = %d\n", ret));
    ReleaseDC(HWND_DESKTOP, hdc);

    if (ret != gCtx.fRDPState)
    {
        DWORD cbReturned;

        if (!DeviceIoControl (gCtx.pEnv->hDriver, (ret) ? IOCTL_VBOXGUEST_ENABLE_VRDP_SESSION : IOCTL_VBOXGUEST_DISABLE_VRDP_SESSION, NULL, 0, NULL, 0, &cbReturned, NULL))
        {
            dprintf(("VBoxRestoreThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        }
        gCtx.fRDPState = ret;
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
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxRestoreThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxRestoreThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_RESTORED;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            dprintf(("VBoxRestoreThread: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            dprintf(("VBoxRestoreThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_RESTORED)
                PostMessage(gToolWindow, WM_VBOX_RESTORED, 0, 0);
            else
                /** @todo Don't poll, but wait for connect/disconnect events */
                PostMessage(gToolWindow, WM_VBOX_CHECK_VRDP, 0, 0);
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
    maskInfo.u32NotMask = VMMDEV_EVENT_RESTORED;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxRestoreThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxRestoreThread: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxRestoreThread: finished seamless change request thread\n"));
    return 0;
}


