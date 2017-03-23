/* $Id$ */
/** @file
 * IPRT - Init Ring-3, Windows Specific Code.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <iprt/win/windows.h>
#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
# define LOAD_LIBRARY_SEARCH_APPLICATION_DIR    0x200
# define LOAD_LIBRARY_SEARCH_SYSTEM32           0x800
#endif

#include "internal-r3-win.h"
#include <iprt/initterm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "../init.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Windows DLL loader protection level. */
DECLHIDDEN(RTR3WINLDRPROT)      g_enmWinLdrProt = RTR3WINLDRPROT_NONE;
/** Our simplified windows version.    */
DECLHIDDEN(RTWINOSTYPE)         g_enmWinVer = kRTWinOSType_UNKNOWN;
/** Extended windows version information. */
DECLHIDDEN(OSVERSIONINFOEXW)    g_WinOsInfoEx;
/** The native kernel32.dll handle. */
DECLHIDDEN(HMODULE)             g_hModKernel32 = NULL;
/** The native ntdll.dll handle. */
DECLHIDDEN(HMODULE)             g_hModNtDll = NULL;
/** GetSystemWindowsDirectoryW or GetWindowsDirectoryW (NT4). */
DECLHIDDEN(PFNGETWINSYSDIR)     g_pfnGetSystemWindowsDirectoryW = NULL;



/**
 * Translates OSVERSIONINOFEX into a Windows OS type.
 *
 * @returns The Windows OS type.
 * @param   pOSInfoEx       The OS info returned by Windows.
 *
 * @remarks This table has been assembled from Usenet postings, personal
 *          observations, and reading other people's code.  Please feel
 *          free to add to it or correct it.
 * <pre>
         dwPlatFormID  dwMajorVersion  dwMinorVersion  dwBuildNumber
95             1              4               0             950
95 SP1         1              4               0        >950 && <=1080
95 OSR2        1              4             <10           >1080
98             1              4              10            1998
98 SP1         1              4              10       >1998 && <2183
98 SE          1              4              10          >=2183
ME             1              4              90            3000

NT 3.51        2              3              51            1057
NT 4           2              4               0            1381
2000           2              5               0            2195
XP             2              5               1            2600
2003           2              5               2            3790
Vista          2              6               0

CE 1.0         3              1               0
CE 2.0         3              2               0
CE 2.1         3              2               1
CE 3.0         3              3               0
</pre>
 */
static RTWINOSTYPE rtR3InitWinSimplifiedVersion(OSVERSIONINFOEXW const *pOSInfoEx)
{
    RTWINOSTYPE enmVer         = kRTWinOSType_UNKNOWN;
    BYTE  const bProductType   = pOSInfoEx->wProductType;
    DWORD const dwPlatformId   = pOSInfoEx->dwPlatformId;
    DWORD const dwMinorVersion = pOSInfoEx->dwMinorVersion;
    DWORD const dwMajorVersion = pOSInfoEx->dwMajorVersion;
    DWORD const dwBuildNumber  = pOSInfoEx->dwBuildNumber & 0xFFFF;   /* Win 9x needs this. */

    if (    dwPlatformId == VER_PLATFORM_WIN32_WINDOWS
        &&  dwMajorVersion == 4)
    {
        if (        dwMinorVersion < 10
                 && dwBuildNumber == 950)
            enmVer = kRTWinOSType_95;
        else if (   dwMinorVersion < 10
                 && dwBuildNumber > 950
                 && dwBuildNumber <= 1080)
            enmVer = kRTWinOSType_95SP1;
        else if (   dwMinorVersion < 10
                 && dwBuildNumber > 1080)
            enmVer = kRTWinOSType_95OSR2;
        else if (   dwMinorVersion == 10
                 && dwBuildNumber == 1998)
            enmVer = kRTWinOSType_98;
        else if (   dwMinorVersion == 10
                 && dwBuildNumber > 1998
                 && dwBuildNumber < 2183)
            enmVer = kRTWinOSType_98SP1;
        else if (   dwMinorVersion == 10
                 && dwBuildNumber >= 2183)
            enmVer = kRTWinOSType_98SE;
        else if (dwMinorVersion == 90)
            enmVer = kRTWinOSType_ME;
    }
    else if (dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        if (        dwMajorVersion == 3
                 && dwMinorVersion == 51)
            enmVer = kRTWinOSType_NT351;
        else if (   dwMajorVersion == 4
                 && dwMinorVersion == 0)
            enmVer = kRTWinOSType_NT4;
        else if (   dwMajorVersion == 5
                 && dwMinorVersion == 0)
            enmVer = kRTWinOSType_2K;
        else if (   dwMajorVersion == 5
                 && dwMinorVersion == 1)
            enmVer = kRTWinOSType_XP;
        else if (   dwMajorVersion == 5
                 && dwMinorVersion == 2)
            enmVer = kRTWinOSType_2003;
        else if (   dwMajorVersion == 6
                 && dwMinorVersion == 0)
        {
            if (bProductType != VER_NT_WORKSTATION)
                enmVer = kRTWinOSType_2008;
            else
                enmVer = kRTWinOSType_VISTA;
        }
        else if (   dwMajorVersion == 6
                 && dwMinorVersion == 1)
        {
            if (bProductType != VER_NT_WORKSTATION)
                enmVer = kRTWinOSType_2008R2;
            else
                enmVer = kRTWinOSType_7;
        }
        else if (   dwMajorVersion == 6
                 && dwMinorVersion == 2)
        {
            if (bProductType != VER_NT_WORKSTATION)
                enmVer = kRTWinOSType_2012;
            else
                enmVer = kRTWinOSType_8;
        }
        else if (   dwMajorVersion == 6
                 && dwMinorVersion == 3)
        {
            if (bProductType != VER_NT_WORKSTATION)
                enmVer = kRTWinOSType_2012R2;
            else
                enmVer = kRTWinOSType_81;
        }
        else if (   (   dwMajorVersion == 6
                     && dwMinorVersion == 4)
                 || (   dwMajorVersion == 10
                     && dwMinorVersion == 0))
        {
            if (bProductType != VER_NT_WORKSTATION)
                enmVer = kRTWinOSType_2016;
            else
                enmVer = kRTWinOSType_10;
        }
        else
            enmVer = kRTWinOSType_NT_UNKNOWN;
    }

    return enmVer;
}


/**
 * Initializes the global variables related to windows version.
 */
static void rtR3InitWindowsVersion(void)
{
    Assert(g_hModNtDll != NULL);

    /*
     * ASSUMES OSVERSIONINFOEX starts with the exact same layout as OSVERSIONINFO (safe).
     */
    AssertCompileMembersSameSizeAndOffset(OSVERSIONINFOEX, szCSDVersion, OSVERSIONINFO, szCSDVersion);
    AssertCompileMemberOffset(OSVERSIONINFOEX, wServicePackMajor, sizeof(OSVERSIONINFO));

    /*
     * Use the NT version of GetVersionExW so we don't get fooled by
     * compatability shims.
     */
    RT_ZERO(g_WinOsInfoEx);
    g_WinOsInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

    LONG (__stdcall *pfnRtlGetVersion)(OSVERSIONINFOEXW *);
    *(FARPROC *)&pfnRtlGetVersion = GetProcAddress(g_hModNtDll, "RtlGetVersion");
    LONG rcNt = -1;
    if (pfnRtlGetVersion)
        rcNt = pfnRtlGetVersion(&g_WinOsInfoEx);
    if (rcNt != 0)
    {
        /*
         * Couldn't find it or it failed, try the windows version of the API.
         */
        RT_ZERO(g_WinOsInfoEx);
        g_WinOsInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
        if (!GetVersionExW((POSVERSIONINFOW)&g_WinOsInfoEx))
        {
            /*
             * If that didn't work either, just get the basic version bits.
             */
            RT_ZERO(g_WinOsInfoEx);
            g_WinOsInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
            if (GetVersionExW((POSVERSIONINFOW)&g_WinOsInfoEx))
                Assert(g_WinOsInfoEx.dwPlatformId != VER_PLATFORM_WIN32_NT || g_WinOsInfoEx.dwMajorVersion < 5);
            else
            {
                AssertBreakpoint();
                RT_ZERO(g_WinOsInfoEx);
            }
        }
    }

    if (g_WinOsInfoEx.dwOSVersionInfoSize)
        g_enmWinVer = rtR3InitWinSimplifiedVersion(&g_WinOsInfoEx);
}


static int rtR3InitNativeObtrusiveWorker(uint32_t fFlags)
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
     *    searching to the application directory (except when
     *    RTR3INIT_FLAGS_STANDALONE_APP is given) and the System32 directory.
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

    /** @bugref{6861} Observed GUI issues on Vista (32-bit and 64-bit) when using
     *                SetDefaultDllDirectories.
     *  @bugref{8194} Try use SetDefaultDllDirectories on Vista for standalone apps
     *                despite potential GUI issues. */
    if (   g_enmWinVer > kRTWinOSType_VISTA
        || (fFlags & RTR3INIT_FLAGS_STANDALONE_APP))
    {
        typedef BOOL(WINAPI *PFNSETDEFAULTDLLDIRECTORIES)(DWORD);
        PFNSETDEFAULTDLLDIRECTORIES pfnSetDefDllDirs;
        pfnSetDefDllDirs = (PFNSETDEFAULTDLLDIRECTORIES)GetProcAddress(g_hModKernel32, "SetDefaultDllDirectories");
        if (pfnSetDefDllDirs)
        {
            DWORD fDllDirs = LOAD_LIBRARY_SEARCH_SYSTEM32;
            if (!(fFlags & RTR3INIT_FLAGS_STANDALONE_APP))
                fDllDirs |= LOAD_LIBRARY_SEARCH_APPLICATION_DIR;
            if (pfnSetDefDllDirs(fDllDirs))
                g_enmWinLdrProt = fDllDirs & LOAD_LIBRARY_SEARCH_APPLICATION_DIR ? RTR3WINLDRPROT_SAFE : RTR3WINLDRPROT_SAFER;
            else if (RT_SUCCESS(rc))
                rc = VERR_INTERNAL_ERROR_4;
        }
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

    rtR3InitWindowsVersion();

    int rc = VINF_SUCCESS;
    if (!(fFlags & RTR3INIT_FLAGS_UNOBTRUSIVE))
        rc = rtR3InitNativeObtrusiveWorker(fFlags);

    /*
     * Resolve some kernel32.dll APIs we may need but aren't necessarily
     * present in older windows versions.
     */
    g_pfnGetSystemWindowsDirectoryW = (PFNGETWINSYSDIR)GetProcAddress(g_hModKernel32, "GetSystemWindowsDirectoryW");
    if (g_pfnGetSystemWindowsDirectoryW)
        g_pfnGetSystemWindowsDirectoryW = (PFNGETWINSYSDIR)GetProcAddress(g_hModKernel32, "GetWindowsDirectoryW");

    return rc;
}


DECLHIDDEN(void) rtR3InitNativeObtrusive(uint32_t fFlags)
{
    rtR3InitNativeObtrusiveWorker(fFlags);
}


DECLHIDDEN(int) rtR3InitNativeFinal(uint32_t fFlags)
{
    /* Nothing to do here. */
    RT_NOREF_PV(fFlags);
    return VINF_SUCCESS;
}

