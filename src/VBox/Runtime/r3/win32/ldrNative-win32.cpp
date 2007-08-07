/* $Id$ */
/** @file
 * innotek Portable Runtime - Binary Image Loader, Win32 native.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <Windows.h>

#include <iprt/ldr.h>
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/err.h>
#include <iprt/alloca.h>
#include "internal/ldr.h"


int rtldrNativeLoad(const char *pszFilename, uintptr_t *phHandle)
{
    Assert(sizeof(*phHandle) >= sizeof(HMODULE));

    /*
     * Do we need to add an extension?
     */
    if (!RTPathHaveExt(pszFilename))
    {
        size_t cch = strlen(pszFilename);
        char *psz = (char *)alloca(cch + sizeof(".DLL"));
        if (!psz)
            return VERR_NO_MEMORY;
        memcpy(psz, pszFilename, cch);
        memcpy(psz + cch, ".DLL", sizeof(".DLL"));
        pszFilename = psz;
    }

    /*
     * Attempt load.
     */
    HMODULE hmod = LoadLibrary(pszFilename);
    if (hmod)
    {
        *phHandle = (uintptr_t)hmod;
        return VINF_SUCCESS;
    }

    return RTErrConvertFromWin32(GetLastError());
}


DECLCALLBACK(int) rtldrNativeGetSymbol(PRTLDRMODINTERNAL pMod, const char *pszSymbol, void **ppvValue)
{
    PRTLDRMODNATIVE pModNative = (PRTLDRMODNATIVE)pMod;
    FARPROC pfn = GetProcAddress((HMODULE)pModNative->hNative, pszSymbol);
    if (pfn)
    {
        *ppvValue = (void *)pfn;
        return VINF_SUCCESS;
    }
    *ppvValue = NULL;
    return RTErrConvertFromWin32(GetLastError());
}


DECLCALLBACK(int) rtldrNativeClose(PRTLDRMODINTERNAL pMod)
{
    PRTLDRMODNATIVE pModNative = (PRTLDRMODNATIVE)pMod;
    if (FreeLibrary((HMODULE)pModNative->hNative))
    {
        pModNative->hNative = (uintptr_t)INVALID_HANDLE_VALUE;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromWin32(GetLastError());
}

