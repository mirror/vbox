/* $Id$ */
/** @file
 * IPRT - Init Ring-3, Windows Specific Code.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <Windows.h>
#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
# define LOAD_LIBRARY_SEARCH_APPLICATION_DIR    0x200
# define LOAD_LIBRARY_SEARCH_SYSTEM32           0x800
#endif

#include "internal/iprt.h"
#include <iprt/initterm.h>
#include <iprt/err.h>
#include "../init.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The native kernel32.dll handle. */
DECLHIDDEN(HMODULE)         g_hModKernel32 = NULL;
/** The native ntdll.dll handle. */
DECLHIDDEN(HMODULE)         g_hModNtDll = NULL;
/** Windows DLL loader protection level. */
DECLHIDDEN(RTR3WINLDRPROT)  g_enmWinLdrProt = RTR3WINLDRPROT_NONE;


DECLHIDDEN(int) rtR3InitNativeObtrusiveWorker(void)
{
    /*
     * Disable error popups.
     */
    UINT fOldErrMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | fOldErrMode);

    /*
     * Restrict DLL searching for the process on windows versions which allow
     * us to do so.
     *  - The first trick works on XP SP1+ and disables the searching of the
     *    current directory.
     *  - The second trick is W7 w/ KB2533623 and W8+, it restrict the DLL
     *    searching to the application directory and the System32 directory.
     */
    int rc = VINF_SUCCESS;

    typedef BOOL (WINAPI *PFNSETDLLDIRECTORY)(LPCWSTR);
    PFNSETDLLDIRECTORY pfnSetDllDir = (PFNSETDLLDIRECTORY)GetProcAddress(g_hModKernel32, "SetDllDirectoryW");
    if (pfnSetDllDir)
    {
        if (pfnSetDllDir(L""))
            g_enmWinLdrProt = RTR3WINLDRPROT_NO_CWD;
        else
            rc = VERR_INTERNAL_ERROR_3;
    }

    typedef BOOL (WINAPI *PFNSETDEFAULTDLLDIRECTORIES)(DWORD);
    PFNSETDEFAULTDLLDIRECTORIES pfnSetDefDllDirs;
    pfnSetDefDllDirs = (PFNSETDEFAULTDLLDIRECTORIES)GetProcAddress(g_hModKernel32, "SetDefaultDllDirectories");
    if (pfnSetDefDllDirs)
    {
        if (pfnSetDefDllDirs(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32))
            g_enmWinLdrProt = RTR3WINLDRPROT_SAFE;
        else if (RT_SUCCESS(rc))
            rc = VERR_INTERNAL_ERROR_4;
    }

    return rc;
}


DECLHIDDEN(int) rtR3InitNativeFirst(uint32_t fFlags)
{
    /*
     * Make sure we've got the handles of the two main Windows NT dlls.
     */
    g_hModKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (g_hModKernel32 == NULL)
        return VERR_INTERNAL_ERROR_2;
    g_hModNtDll    = GetModuleHandleW(L"ntdll.dll");
    if (g_hModNtDll == NULL)
        return VERR_INTERNAL_ERROR_2;

    int rc = VINF_SUCCESS;
    if (!(fFlags & RTR3INIT_FLAGS_UNOBTRUSIVE))
        rc = rtR3InitNativeObtrusiveWorker();

    return rc;
}


DECLHIDDEN(void) rtR3InitNativeObtrusive(void)
{
    rtR3InitNativeObtrusiveWorker();
}


DECLHIDDEN(int) rtR3InitNativeFinal(uint32_t fFlags)
{
    /* Nothing to do here. */
    return VINF_SUCCESS;
}

