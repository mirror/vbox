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

#include "VBoxService.h"
#include <VBoxHook.h>
#include <VBox/VBoxDev.h>
#include <iprt/assert.h>
#include "helpers.h"

typedef struct _VBOXSEAMLESSCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    HMODULE hModule;

    BOOL (* pfnVBoxInstallHook)(HMODULE hDll);
    BOOL (* pfnVBoxRemoveHook)();

} VBOXSEAMLESSCONTEXT;

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
        waitEvent.u32TimeoutIn = 1000;
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
                            if (pCtx->pfnVBoxRemoveHook)
                                pCtx->pfnVBoxRemoveHook();
                            break;

                        case VMMDev_Seamless_Visible_Region:
                            if (pCtx->pfnVBoxInstallHook)
                                pCtx->pfnVBoxInstallHook(pCtx->hModule);
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
