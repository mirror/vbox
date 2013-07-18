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
#include <iprt/ldr.h>

struct VBOXMMRCONTEXT
{
    RTLDRMOD  hModHook;
    HHOOK     hHook;
};

static VBOXMMRCONTEXT gCtx = {0};

static const char *g_pszMMRDLL  = "VBoxMMRHook";
static const char *g_pszMMRPROC = "CBTProc";

void VBoxMMRCleanup(VBOXMMRCONTEXT *pCtx)
{
    if (pCtx->hHook)
    {
        UnhookWindowsHookEx(pCtx->hHook);
        pCtx->hHook = NULL;
    }

    if (pCtx->hModHook != NIL_RTLDRMOD)
    {
        RTLdrClose(pCtx->hModHook);
        pCtx->hModHook = NIL_RTLDRMOD;
    }
}

int VBoxMMRInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    LogRel2(("VBoxMMR: Initializing\n"));

    int rc = RTLdrLoadAppPriv(g_pszMMRDLL, &gCtx.hModHook);
    if (RT_SUCCESS(rc))
    {
        HOOKPROC pHook = (HOOKPROC)RTLdrGetFunction(gCtx.hModHook, g_pszMMRPROC);
        if (pHook)
        {
            HMODULE hMod = (HMODULE)RTLdrGetNativeHandle(gCtx.hModHook);
            Assert(hMod != (HMODULE)~(uintptr_t)0);
            gCtx.hHook = SetWindowsHookEx(WH_CBT, pHook, hMod, 0);
            if (gCtx.hHook)
            {
                *ppInstance = &gCtx;
                return VINF_SUCCESS;
            }

            rc = RTErrConvertFromWin32(GetLastError());
            LogRel2(("VBoxMMR: Error installing hooking proc: %Rrc\n", rc));
        }
        else
        {
            LogRel2(("VBoxMMR: Hooking proc not found\n"));
            rc = VERR_NOT_FOUND;
        }

        RTLdrClose(gCtx.hModHook);
        gCtx.hModHook = NIL_RTLDRMOD;
    }
    else
        LogRel2(("VBoxMMR: Hooking library not found (%Rrc)\n", rc));

    return rc;
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

