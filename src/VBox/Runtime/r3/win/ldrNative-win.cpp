/* $Id$ */
/** @file
 * IPRT - Binary Image Loader, Win32 native.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_LDR
#include <Windows.h>

#include <iprt/ldr.h>
#include "internal/iprt.h"

#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <iprt/once.h>
#include <iprt/string.h>
#include "internal/ldr.h"

#ifndef LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
# define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR       UINT32_C(0x100)
# define LOAD_LIBRARY_SEARCH_APPLICATION_DIR    UINT32_C(0x200)
# define LOAD_LIBRARY_SEARCH_SYSTEM32           UINT32_C(0x800)
#endif


int rtldrNativeLoad(const char *pszFilename, uintptr_t *phHandle, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    Assert(sizeof(*phHandle) >= sizeof(HMODULE));
    AssertReturn(!(fFlags & RTLDRLOAD_FLAGS_GLOBAL), VERR_INVALID_FLAGS);
    AssertLogRelMsgReturn(RTPathStartsWithRoot(pszFilename),  /* Relative names will still be applied to the search path. */
                          ("pszFilename='%s'\n", pszFilename),
                          VERR_INTERNAL_ERROR_2);

    /*
     * Convert to UTF-16 and make sure it got a .DLL suffix.
     */
    int rc;
    RTUTF16 *pwszNative = NULL;
    if (RTPathHasSuffix(pszFilename))
        rc = RTStrToUtf16(pszFilename, &pwszNative);
    else
    {
        size_t cwcAlloc;
        rc = RTStrCalcUtf16LenEx(pszFilename, RTSTR_MAX, &cwcAlloc);
        if (RT_SUCCESS(rc))
        {
            cwcAlloc += sizeof(".DLL");
            pwszNative = RTUtf16Alloc(cwcAlloc * sizeof(RTUTF16));
            if (pwszNative)
            {
                size_t cwcNative;
                rc = RTStrToUtf16Ex(pszFilename, RTSTR_MAX, &pwszNative, cwcAlloc, &cwcNative);
                if (RT_SUCCESS(rc))
                    rc = RTUtf16CopyAscii(&pwszNative[cwcNative], cwcAlloc - cwcNative, ".DLL");
            }
            else
                rc = VERR_NO_UTF16_MEMORY;
        }
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Attempt load.
         */
        HMODULE     hmod;
        static int  s_iSearchDllLoadDirSupported = 0;
        if (   !(fFlags & RTLDRLOAD_FLAGS_NT_SEARCH_DLL_LOAD_DIR)
            || s_iSearchDllLoadDirSupported < 0)
            hmod = LoadLibraryExW(pwszNative, NULL, 0);
        else
        {
            hmod = LoadLibraryExW(pwszNative, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32
                                  | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
            if (s_iSearchDllLoadDirSupported == 0)
            {
                if (hmod != NULL || GetLastError() != ERROR_INVALID_PARAMETER)
                    s_iSearchDllLoadDirSupported = 1;
                else
                {
                    s_iSearchDllLoadDirSupported = -1;
                    hmod = LoadLibraryExW(pwszNative, NULL, 0);
                }
            }
        }
        if (hmod)
        {
            *phHandle = (uintptr_t)hmod;
            RTUtf16Free(pwszNative);
            return VINF_SUCCESS;
        }

        /*
         * Try figure why it failed to load.
         */
        DWORD dwErr = GetLastError();
        rc = RTErrConvertFromWin32(dwErr);
        rc = RTErrInfoSetF(pErrInfo, rc, "GetLastError=%u", dwErr);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, rc, "Error converting UTF-8 to UTF-16 string.");
    RTUtf16Free(pwszNative);
    return rc;
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
    if (   (pModNative->fFlags & RTLDRLOAD_FLAGS_NO_UNLOAD)
        || FreeLibrary((HMODULE)pModNative->hNative))
    {
        pModNative->hNative = (uintptr_t)INVALID_HANDLE_VALUE;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromWin32(GetLastError());
}


int rtldrNativeLoadSystem(const char *pszFilename, const char *pszExt, uint32_t fFlags, PRTLDRMOD phLdrMod)
{
    /*
     * We only try the System32 directory.
     */
    WCHAR wszSysDir[MAX_PATH];
    UINT cwcSysDir = GetSystemDirectoryW(wszSysDir, MAX_PATH);
    if (cwcSysDir >= MAX_PATH)
        return VERR_FILENAME_TOO_LONG;

    char szPath[RTPATH_MAX];
    char *pszPath = szPath;
    int rc = RTUtf16ToUtf8Ex(wszSysDir, RTSTR_MAX, &pszPath, sizeof(szPath), NULL);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathAppend(szPath, sizeof(szPath), pszFilename);
        if (pszExt && RT_SUCCESS(rc))
            rc = RTStrCat(szPath, sizeof(szPath), pszExt);
        if (RT_SUCCESS(rc))
        {
            if (RTFileExists(szPath))
                rc = RTLdrLoadEx(szPath, phLdrMod, fFlags, NULL);
            else
                rc = VERR_MODULE_NOT_FOUND;
        }
    }

    return rc;
}

