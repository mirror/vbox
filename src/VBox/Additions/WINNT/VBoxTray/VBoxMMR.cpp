/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxTray.h"
#include "VBoxMMR.h"

struct VBOXMMRCONTEXT
{
    HINSTANCE hMod;
    HHOOK     hHook;
};

static VBOXMMRCONTEXT gCtx = {0};

static const char *g_pszMMRDLL  = "VBoxMMRHook";
static const char *g_pszMMRPROC = "CBTProc";

void VBoxMMRCleanup(VBOXMMRCONTEXT *pCtx)
{
    if (NULL != pCtx->hHook)
    {
        UnhookWindowsHookEx(pCtx->hHook);
    }

    if (pCtx->hMod)
    {
        FreeLibrary(pCtx->hMod);
    }

    return;
}

int VBoxMMRInit(
    const VBOXSERVICEENV *pEnv,
    void **ppInstance,
    bool *pfStartThread)
{
    HOOKPROC pHook = NULL;

    LogRel2(("VBoxMMR: Initializing\n"));

    gCtx.hMod = LoadLibraryA(g_pszMMRDLL);
    if (NULL == gCtx.hMod)
    {
        LogRel2(("VBoxMMR: Hooking library not found\n"));
        VBoxMMRCleanup(&gCtx);
        return VERR_NOT_FOUND;
    }

    pHook = (HOOKPROC) GetProcAddress(gCtx.hMod, g_pszMMRPROC);
    if (NULL == pHook)
    {
        LogRel2(("VBoxMMR: Hooking proc not found\n"));
        VBoxMMRCleanup(&gCtx);
        return VERR_NOT_FOUND;
    }

    gCtx.hHook = SetWindowsHookEx(WH_CBT, pHook, gCtx.hMod, 0);
    if (NULL == gCtx.hHook)
    {
        int rc = RTErrConvertFromWin32(GetLastError());
        LogRel2(("VBoxMMR: Error installing hooking proc: %d\n", rc));
        VBoxMMRCleanup(&gCtx);
        return rc;
    }

    *ppInstance = &gCtx;

    return VINF_SUCCESS;
}

void VBoxMMRDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    VBOXMMRCONTEXT *pCtx = (VBOXMMRCONTEXT *) pInstance;

    VBoxMMRCleanup(pCtx);
}

unsigned __stdcall VBoxMMRThread(void *pInstance)
{
    return 0;
}
