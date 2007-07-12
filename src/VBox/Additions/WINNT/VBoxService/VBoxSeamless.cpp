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
#include "helpers.h"

static HMODULE hModule = 0;
/** @todo statically link in the future; dynamic linking is just testing purposes */

BOOL (* APIENTRY pfnVBoxInstallHook)(HMODULE hDll) = NULL;
BOOL (* APIENTRY pfnVBoxRemoveHook)() = NULL;


void VBoxLogString(HANDLE hDriver, char *pszStr);

int VBoxSeamlessInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    *pfStartThread = false;
    hModule = LoadLibrary(VBOXHOOK_DLL_NAME);
    if (hModule)
    {
        *(uintptr_t *)&pfnVBoxInstallHook = (uintptr_t)GetProcAddress(hModule, "VBoxInstallHook");
        *(uintptr_t *)&pfnVBoxRemoveHook  = (uintptr_t)GetProcAddress(hModule, "VBoxRemoveHook");
    }
    if (pfnVBoxInstallHook)
    {
        /* inform the host that we support the seamless window mode */
        VMMDevReqGuestCapabilities vmmreqGuestCaps = {0};
        vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqGuestCaps, VMMDevReq_ReportGuestCapabilities);
        vmmreqGuestCaps.caps = VMMDEV_GUEST_SUPPORTS_SEAMLESS;

        DWORD cbReturned;
        if (!DeviceIoControl(pEnv->hDriver, IOCTL_VBOXGUEST_VMMREQUEST, &vmmreqGuestCaps, sizeof(vmmreqGuestCaps),
                             &vmmreqGuestCaps, sizeof(vmmreqGuestCaps), &cbReturned, NULL))
        {
            SvcDebugOut2("VMMDevReq_ReportGuestCapabilities: error doing IOCTL, last error: %d\n", GetLastError());
            return VERR_INVALID_PARAMETER;
        }

        pfnVBoxInstallHook(hModule);
    }
    return VINF_SUCCESS;
}


void VBoxSeamlessDestroy  (const VBOXSERVICEENV *pEnv, void *pInstance)
{
    /* inform the host that we no longer support the seamless window mode */
    VMMDevReqGuestCapabilities vmmreqGuestCaps = {0};
    vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqGuestCaps, VMMDevReq_ReportGuestCapabilities);
    vmmreqGuestCaps.caps = 0;

    DWORD cbReturned;
    if (!DeviceIoControl(pEnv->hDriver, IOCTL_VBOXGUEST_VMMREQUEST, &vmmreqGuestCaps, sizeof(vmmreqGuestCaps),
                         &vmmreqGuestCaps, sizeof(vmmreqGuestCaps), &cbReturned, NULL))
    {
        SvcDebugOut2("VMMDevReq_ReportGuestCapabilities: error doing IOCTL, last error: %d\n", GetLastError());
    }
    if (pfnVBoxRemoveHook)
        pfnVBoxRemoveHook();

    FreeLibrary(hModule);
    return;
}

#if 0

static char LogBuffer[1024];


VBGLR3DECL(int) VbglR3GRPerform(HANDLE hDriver, VMMDevRequestHeader *pReq)
{
    DWORD cbReturned;
    if (!DeviceIoControl(hDriver, IOCTL_VBOXGUEST_VMMREQUEST, pReq, pReq->size,
                         pReq, pReq->size, &cbReturned, NULL))
    {
        SvcDebugOut2("error doing IOCTL, last error: %d\n", GetLastError());
    }
    return VINF_SUCCESS;
}

void VBoxLogString(HANDLE hDriver, char *pszStr)
{
    VMMDevReqLogString *pReq = (VMMDevReqLogString *)LogBuffer;
    int rc;

    vmmdevInitRequest(&pReq->header, VMMDevReq_LogString);
    strcpy(pReq->szString, pszStr);
    pReq->header.size += strlen(pszStr);
    rc = VbglR3GRPerform(hDriver, &pReq->header);
    return;
}

#endif
