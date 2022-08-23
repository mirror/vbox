/* $Id$ */
/** @file
 * VBoxInstallHelper - Various helper routines for Windows host installer.
 */

/*
 * Copyright (C) 2008-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_NETFLT
# include "VBox/VBoxNetCfg-win.h"
# include "VBox/VBoxDrvCfg-win.h"
#endif

#include <VBox/version.h>

#include <wchar.h>
#include <stdio.h>

#include <msi.h>
#include <msiquery.h>

#define _WIN32_DCOM
#include <iprt/win/windows.h>

#include <assert.h>
#include <shellapi.h>
#define INITGUID
#include <guiddef.h>
#include <cfgmgr32.h>
#include <devguid.h>

#include <iprt/alloca.h>
#include <iprt/string.h> /* RT_ZERO */
#include <iprt/path.h>   /* RTPATH_MAX, RTPATH_IS_SLASH */

#include <iprt/win/objbase.h>
#include <iprt/win/setupapi.h>
#include <iprt/win/shlobj.h>

#include "VBoxCommon.h"

#ifndef VBOX_OSE
# include "internal/VBoxSerial.h"
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifdef DEBUG
# define NonStandardAssert(_expr) assert(_expr)
#else
# define NonStandardAssert(_expr) do{ }while(0)
#endif

#define MY_WTEXT_HLP(a_str) L##a_str
#define MY_WTEXT(a_str)     MY_WTEXT_HLP(a_str)



/**
 * DLL entry point.
 */
BOOL WINAPI DllMain(HANDLE hInst, ULONG uReason, LPVOID pReserved)
{
    RT_NOREF(hInst, uReason, pReserved);
    return TRUE;
}

/**
 * Format and add message to the MSI log.
 *
 * UTF-16 strings are formatted using '%s' (lowercase).
 * ANSI strings are formatted using '%S' (uppercase).
 */
static UINT logStringF(MSIHANDLE hInstall, const wchar_t *pwszFmt, ...)
{
    PMSIHANDLE hMSI = MsiCreateRecord(2 /* cParms */);
    if (hMSI)
    {
        wchar_t wszBuf[RTPATH_MAX + 256];
        va_list va;
        va_start(va, pwszFmt);
        ssize_t cwc = _vsnwprintf(wszBuf, RT_ELEMENTS(wszBuf), pwszFmt, va);
        va_end(va);
        wszBuf[RT_ELEMENTS(wszBuf) - 1] = '\0';

        MsiRecordSetStringW(hMSI, 0, wszBuf);
        MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_INFO), hMSI);

        MsiCloseHandle(hMSI);
        return cwc < RT_ELEMENTS(wszBuf) ? ERROR_SUCCESS : ERROR_BUFFER_OVERFLOW;
    }
    return ERROR_ACCESS_DENIED;
}

UINT __stdcall IsSerialCheckNeeded(MSIHANDLE hModule)
{
#ifndef VBOX_OSE
    /*BOOL fRet =*/ serialCheckNeeded(hModule);
#else
    RT_NOREF(hModule);
#endif
    return ERROR_SUCCESS;
}

UINT __stdcall CheckSerial(MSIHANDLE hModule)
{
#ifndef VBOX_OSE
    /*BOOL bRet =*/ serialIsValid(hModule);
#else
    RT_NOREF(hModule);
#endif
    return ERROR_SUCCESS;
}

/**
 * Runs an executable on the OS.
 *
 * @returns Windows error code.
 * @param   hModule     Windows installer module handle.
 * @param   pwszImage   The executable to run.
 * @param   pwszArgs    The arguments (command line w/o executable).
 */
static UINT procRun(MSIHANDLE hModule, const wchar_t *pwszImage, wchar_t const *pwszArgs)
{
    /*
     * Construct a full command line.
     */
    size_t const cwcImage = wcslen(pwszImage);
    size_t const cwcArgs  = wcslen(pwszArgs);

    wchar_t *pwszCmdLine = (wchar_t *)alloca((1 + cwcImage + 1 + 1 + cwcArgs + 1) * sizeof(wchar_t));
    pwszCmdLine[0] = '"';
    memcpy(&pwszCmdLine[1], pwszImage, cwcImage * sizeof(wchar_t));
    pwszCmdLine[1 + cwcImage] = '"';
    pwszCmdLine[1 + cwcImage + 1] = ' ';
    memcpy(&pwszCmdLine[1 + cwcImage + 1 + 1], pwszArgs, (cwcArgs + 1) * sizeof(wchar_t));

    /*
     * Construct startup info.
     */
    STARTUPINFOW StartupInfo;
    RT_ZERO(StartupInfo);
    StartupInfo.cb          = sizeof(StartupInfo);
    StartupInfo.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
    StartupInfo.hStdOutput  = GetStdHandle(STD_OUTPUT_HANDLE);
    StartupInfo.hStdError   = GetStdHandle(STD_ERROR_HANDLE);
    StartupInfo.dwFlags     = STARTF_USESTDHANDLES;
#ifndef DEBUG
    StartupInfo.dwFlags    |= STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;
#endif

    /*
     * Start it.
     */
    UINT rcWin;
    PROCESS_INFORMATION ChildInfo = { NULL, NULL, 0, 0 };
    if (CreateProcessW(pwszImage, pwszCmdLine, NULL /*pProcessAttribs*/, NULL /*pThreadAttribs*/, TRUE /*fInheritHandles*/,
                       0 /*fFlags*/, NULL /*pwszEnv*/, NULL /*pwszCwd*/, &StartupInfo, &ChildInfo))
    {
        logStringF(hModule, L"procRun: Info: Started process %u: %s", ChildInfo.dwProcessId, pwszCmdLine);
        CloseHandle(ChildInfo.hThread);
        DWORD const dwWait = WaitForSingleObject(ChildInfo.hProcess, RT_MS_30SEC);
        DWORD dwExitCode = 0xf00dface;
        if (GetExitCodeProcess(ChildInfo.hProcess, &dwExitCode))
        {
            if (dwExitCode == 0)
            {
                logStringF(hModule, L"procRun: Info: Process '%s' terminated exit code zero", pwszCmdLine);
                rcWin = ERROR_SUCCESS;
            }
            else
            {
                logStringF(hModule, L"procRun: Process '%s' terminated with non-zero exit code: %u (%#x)",
                           pwszCmdLine, dwExitCode, dwExitCode);
                rcWin = ERROR_GEN_FAILURE;
            }
        }
        else
        {
            rcWin = GetLastError();
            logStringF(hModule, L"procRun: Process '%s' is probably still running: rcWin=%u dwWait=%u (%#x)",
                       pwszCmdLine, rcWin, dwWait, dwWait);
        }
    }
    else
    {
        rcWin = GetLastError();
        logStringF(hModule, L"procRun: Creating process '%s' failed: rcWin=%u\n", pwszCmdLine, rcWin);
    }
    return rcWin;
}

/**
 * Tries to retrieve the Python installation path on the system, extended version.
 *
 * @returns Windows error code.
 * @param   hModule         Windows installer module handle.
 * @param   hKeyRoot        Registry root key to use, e.g. HKEY_LOCAL_MACHINE.
 * @param   pwszPythonPath  Buffer to return the path for python.exe in.
 * @param   cwcPythonPath   Buffer size in UTF-16 units.
 * @param   fReturnExe      Return the path to python.exe if true, otherwise
 *                          just the python install directory.
 */
static UINT getPythonPathEx(MSIHANDLE hModule, HKEY hKeyRoot, wchar_t *pwszPythonPath, size_t cwcPythonPath, bool fReturnExe)
{
    *pwszPythonPath = '\0';

    /*
     * Enumerate the subkeys of python core installation key.
     *
     * Note: The loop ASSUMES that later found versions are higher, e.g. newer
     *       Python versions.  For now we always go by the newest version.
     */
    HKEY hKeyPythonCore = NULL;
    LSTATUS dwErr = RegOpenKeyExW(hKeyRoot, L"SOFTWARE\\Python\\PythonCore", 0, KEY_READ, &hKeyPythonCore);
    if (dwErr != ERROR_SUCCESS)
        return dwErr;

    UINT rcWinRet = ERROR_PATH_NOT_FOUND;
    for (DWORD i = 0; i < 16384; ++i)
    {
        static wchar_t const s_wszInstallPath[] = L"\\InstallPath";
        static wchar_t const s_wszPythonExe[]   = L"python.exe";

        /* Get key name: */
        wchar_t wszBuf[RTPATH_MAX + RT_MAX(RT_ELEMENTS(s_wszInstallPath), RT_ELEMENTS(s_wszPythonExe)) + 2];
        DWORD   cwcKeyNm  = RTPATH_MAX;
        DWORD   dwKeyType = REG_SZ;
        dwErr = RegEnumKeyExW(hKeyPythonCore, i, wszBuf, &cwcKeyNm, NULL, NULL, NULL, NULL);
        if (dwErr == ERROR_NO_MORE_ITEMS)
            break;
        if (dwErr != ERROR_SUCCESS)
            continue;
        if (dwKeyType != REG_SZ)
            continue;
        if (cwcKeyNm == 0)
            continue;
        NonStandardAssert(cwcKeyNm <= sizeof(wszBuf));

        /* Try Open the InstallPath subkey: */
        memcpy(&wszBuf[cwcKeyNm], s_wszInstallPath, sizeof(s_wszInstallPath));

        HKEY hKeyInstallPath = NULL;
        dwErr = RegOpenKeyExW(hKeyPythonCore, wszBuf, 0, KEY_READ, &hKeyInstallPath);
        if (dwErr != ERROR_SUCCESS)
            continue;

        /* Query the value.  We double buffer this so we don't overwrite an okay
           return value with this.  Use the smaller of cwcPythonPath and wszValue
           so RegQueryValueExW can do all the buffer overflow checking for us.
           For paranoid reasons, we reserve a space for a terminator as well as
           a slash. (ASSUMES reasonably sized output buffer.) */
        NonStandardAssert(cwcPythonPath > RT_ELEMENTS(s_wszPythonExe) + 16);
        DWORD cbValue = (DWORD)RT_MIN(  cwcPythonPath * sizeof(wchar_t)
                                      - (fReturnExe ? sizeof(s_wszInstallPath) - sizeof(wchar_t) * 2 : sizeof(wchar_t) * 2),
                                      RTPATH_MAX * sizeof(wchar_t));
        DWORD dwValueType = REG_SZ;
        dwErr = RegQueryValueExW(hKeyInstallPath, L"", NULL, &dwValueType, (LPBYTE)wszBuf, &cbValue);
        RegCloseKey(hKeyInstallPath);
        if (   dwErr       == ERROR_SUCCESS
            && dwValueType == REG_SZ
            && cbValue     >= sizeof(L"C:\\") - sizeof(L""))
        {
            /* Find length in wchar_t unit w/o terminator: */
            DWORD cwc = cbValue / sizeof(wchar_t);
            while (cwc > 0 && wszBuf[cwc - 1] == '\0')
                cwc--;
            wszBuf[cwc] = '\0';
            if (cwc > 2)
            {
                /* Check if the path leads to a directory with a python.exe file in it. */
                if (!RTPATH_IS_SLASH(wszBuf[cwc - 1]))
                    wszBuf[cwc++] = '\\';
                memcpy(&wszBuf[cwc], s_wszPythonExe, sizeof(s_wszPythonExe));
                DWORD const fAttribs = GetFileAttributesW(wszBuf);
                if (fAttribs != INVALID_FILE_ATTRIBUTES)
                {
                    if (!(fAttribs & FILE_ATTRIBUTE_DIRECTORY))
                    {
                        /* Okay, we found something that can be returned. */
                        if (fReturnExe)
                            cwc += RT_ELEMENTS(s_wszPythonExe) - 1;
                        wszBuf[cwc] = '\0';
                        logStringF(hModule, L"getPythonPath: Found: \"%s\"", wszBuf);

                        NonStandardAssert(cwcPythonPath > cwc);
                        memcpy(pwszPythonPath, wszBuf, cwc * sizeof(wchar_t));
                        pwszPythonPath[cwc] = '\0';
                        rcWinRet = ERROR_SUCCESS;
                    }
                    else
                        logStringF(hModule, L"getPythonPath: Warning: Skipping \"%s\": is a directory (%#x)", wszBuf, fAttribs);
                }
                else
                    logStringF(hModule, L"getPythonPath: Warning: Skipping \"%s\": Does not exist (%u)", wszBuf, GetLastError());
            }
        }
    }

    RegCloseKey(hKeyPythonCore);
    if (rcWinRet != ERROR_SUCCESS)
        logStringF(hModule, L"getPythonPath: Unable to find python");
    return rcWinRet;
}

/**
 * Retrieves the absolute path of the Python installation.
 *
 * @returns Windows error code.
 * @param   hModule         Windows installer module handle.
 * @param   pwszPythonPath  Buffer to return the path for python.exe in.
 * @param   cwcPythonPath   Buffer size in UTF-16 units.
 * @param   fReturnExe      Return the path to python.exe if true, otherwise
 *                          just the python install directory.
 */
static UINT getPythonPath(MSIHANDLE hModule, wchar_t *pwszPythonPath, size_t cwcPythonPath, bool fReturnExe = false)
{
    UINT rcWin = getPythonPathEx(hModule, HKEY_LOCAL_MACHINE, pwszPythonPath, cwcPythonPath, fReturnExe);
    if (rcWin != ERROR_SUCCESS)
        rcWin = getPythonPathEx(hModule, HKEY_CURRENT_USER, pwszPythonPath, cwcPythonPath, fReturnExe);
    return rcWin;
}

/**
 * Retrieves the absolute path of the Python executable.
 *
 * @returns Windows error code.
 * @param   hModule         Windows installer module handle.
 * @param   pwszPythonExe   Buffer to return the path for python.exe in.
 * @param   cwcPythonExe    Buffer size in UTF-16 units.
 */
static UINT getPythonExe(MSIHANDLE hModule, wchar_t *pwszPythonExe, size_t cwcPythonExe)
{
    return getPythonPath(hModule, pwszPythonExe, cwcPythonExe, true /*fReturnExe*/);
}

/**
 * Checks if all dependencies for running the VBox Python API bindings are met.
 *
 * @returns VBox status code, or error if depedencies are not met.
 * @param   hModule             Windows installer module handle.
 * @param   pwszPythonExe       Path to Python interpreter image (.exe).
 */
static int checkPythonDependencies(MSIHANDLE hModule, const wchar_t *pwszPythonExe)
{
    /*
     * Check if importing the win32api module works.
     * This is a prerequisite for setting up the VBox API.
     */
    logStringF(hModule, L"checkPythonDependencies: Checking for win32api extensions ...");

    UINT rcWin = procRun(hModule, pwszPythonExe, L"-c \"import win32api\"");
    if (rcWin == ERROR_SUCCESS)
        logStringF(hModule, L"checkPythonDependencies: win32api found\n");
    else
        logStringF(hModule, L"checkPythonDependencies: Importing win32api failed with %u (%#x)\n", rcWin, rcWin);

    return rcWin;
}

/**
 * Checks for a valid Python installation on the system.
 *
 * Called from the MSI installer as custom action.
 *
 * @returns Always ERROR_SUCCESS.
 *          Sets public property VBOX_PYTHON_INSTALLED to "0" (false) or "1" (success).
 *          Sets public property VBOX_PYTHON_PATH to the Python installation path (if found).
 *
 * @param   hModule             Windows installer module handle.
 */
UINT __stdcall IsPythonInstalled(MSIHANDLE hModule)
{
    wchar_t wszPythonPath[RTPATH_MAX];
    UINT rcWin = getPythonPath(hModule, wszPythonPath, RTPATH_MAX);
    if (rcWin == ERROR_SUCCESS)
    {
        logStringF(hModule, L"IsPythonInstalled: Python installation found at \"%s\"", wszPythonPath);
        VBoxSetMsiProp(hModule, L"VBOX_PYTHON_PATH", wszPythonPath);
        VBoxSetMsiProp(hModule, L"VBOX_PYTHON_INSTALLED", L"1");
    }
    else
    {
        logStringF(hModule, L"IsPythonInstalled: Error: No suitable Python installation found (%u), skipping installation.", rcWin);
        logStringF(hModule, L"IsPythonInstalled: Python seems not to be installed; please download + install the Python Core package.");
        VBoxSetMsiProp(hModule, L"VBOX_PYTHON_INSTALLED", L"0");
    }

    return ERROR_SUCCESS; /* Never return failure. */
}

/**
 * Checks if all dependencies for running the VBox Python API bindings are met.
 *
 * Called from the MSI installer as custom action.
 *
 * @returns Always ERROR_SUCCESS.
 *          Sets public property VBOX_PYTHON_DEPS_INSTALLED to "0" (false) or "1" (success).
 *
 * @param   hModule             Windows installer module handle.
 */
UINT __stdcall ArePythonAPIDepsInstalled(MSIHANDLE hModule)
{
    wchar_t wszPythonExe[RTPATH_MAX];
    UINT dwErr = getPythonExe(hModule, wszPythonExe, RTPATH_MAX);
    if (dwErr == ERROR_SUCCESS)
    {
        dwErr = checkPythonDependencies(hModule, wszPythonExe);
        if (dwErr == ERROR_SUCCESS)
            logStringF(hModule, L"ArePythonAPIDepsInstalled: Dependencies look good.");
    }

    if (dwErr != ERROR_SUCCESS)
        logStringF(hModule, L"ArePythonAPIDepsInstalled: Failed with dwErr=%u", dwErr);

    VBoxSetMsiProp(hModule, L"VBOX_PYTHON_DEPS_INSTALLED", dwErr == ERROR_SUCCESS ? L"1" : L"0");
    return ERROR_SUCCESS; /* Never return failure. */
}

/**
 * Checks if all required MS CRTs (Visual Studio Redistributable Package) are installed on the system.
 *
 * Called from the MSI installer as custom action.
 *
 * @returns Always ERROR_SUCCESS.
 *          Sets public property VBOX_MSCRT_INSTALLED to "" (false, to use "NOT" in WiX) or "1" (success).
 *
 *          Also exposes public properties VBOX_MSCRT_VER_MIN + VBOX_MSCRT_VER_MAJ strings
 *          with the most recent MSCRT version detected.
 *
 * @param   hModule             Windows installer module handle.
 *
 * @sa      https://docs.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170
 */
UINT __stdcall IsMSCRTInstalled(MSIHANDLE hModule)
{
    HKEY hKeyVS = NULL;
    LSTATUS dwErr = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                  L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X64",
                                  0, KEY_READ, &hKeyVS);
    if (dwErr == ERROR_SUCCESS)
    {
        DWORD dwVal = 0;
        DWORD cbVal = sizeof(dwVal);
        DWORD dwValueType = REG_DWORD;

        dwErr = RegQueryValueExW(hKeyVS, L"Installed", NULL, &dwValueType, (LPBYTE)&dwVal, &cbVal);
        if (dwErr == ERROR_SUCCESS)
        {
            if (dwVal >= 1)
            {
                DWORD dwMin, dwMaj;
                dwErr = RegQueryValueExW(hKeyVS, L"Major", NULL, &dwValueType, (LPBYTE)&dwMaj, &cbVal);
                if (dwErr == ERROR_SUCCESS)
                {
                    VBoxSetMsiPropDWORD(hModule, L"VBOX_MSCRT_VER_MAJ", dwMaj);

                    dwErr = RegQueryValueExW(hKeyVS, L"Minor", NULL, &dwValueType, (LPBYTE)&dwMin, &cbVal);
                    if (dwErr == ERROR_SUCCESS)
                    {
                        VBoxSetMsiPropDWORD(hModule, L"VBOX_MSCRT_VER_MIN", dwMin);

                        logStringF(hModule, L"IsMSCRTInstalled: Found v%ld.%ld\n", dwMaj, dwMin);

                        /* Check for at least 2019. */
                        if (dwMaj >= 14 && dwMin >= 20)
                            VBoxSetMsiProp(hModule, L"VBOX_MSCRT_INSTALLED", L"1");
                    }
                    else
                        logStringF(hModule, L"IsMSCRTInstalled: Found, but 'Minor' key not present");
                }
                else
                    logStringF(hModule, L"IsMSCRTInstalled: Found, but 'Major' key not present");
            }
            else
            {
                logStringF(hModule, L"IsMSCRTInstalled: Found, but not marked as installed");
                dwErr = ERROR_NOT_INSTALLED;
            }
        }
        else
            logStringF(hModule, L"IsMSCRTInstalled: Found, but 'Installed' key not present");
    }

    if (dwErr != ERROR_SUCCESS)
        logStringF(hModule, L"IsMSCRTInstalled: Failed with dwErr=%ld", dwErr);

    return ERROR_SUCCESS; /* Never return failure. */
}

/**
 * Checks if the running OS is (at least) Windows 10 (e.g. >= build 10000).
 *
 * Called from the MSI installer as custom action.
 *
 * @returns Always ERROR_SUCCESS.
 *          Sets public property VBOX_IS_WINDOWS_10 to "" (empty / false) or "1" (success).
 *
 * @param   hModule             Windows installer module handle.
 */
UINT __stdcall IsWindows10(MSIHANDLE hModule)
{
    /*
     * Note: We cannot use RtlGetVersion() / GetVersionExW() here, as the Windows Installer service
     *       all shims this, unfortunately. So we have to go another route by querying the major version
     *       number from the registry.
     */
    HKEY hKeyCurVer = NULL;
    LSTATUS dwErr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKeyCurVer);
    if (dwErr == ERROR_SUCCESS)
    {
        DWORD dwVal = 0;
        DWORD cbVal = sizeof(dwVal);
        DWORD dwValueType = REG_DWORD;
        dwErr = RegQueryValueExW(hKeyCurVer, L"CurrentMajorVersionNumber", NULL, &dwValueType, (LPBYTE)&dwVal, &cbVal);
        if (dwErr == ERROR_SUCCESS)
        {
            logStringF(hModule, L"IsWindows10/CurrentMajorVersionNumber: %ld", dwVal);

            VBoxSetMsiProp(hModule, L"VBOX_IS_WINDOWS_10", dwVal >= 10 ? L"1" : L"");
        }
        else
            logStringF(hModule, L"IsWindows10/RegOpenKeyExW: Error reading CurrentMajorVersionNumber (%ld)", dwErr);

        RegCloseKey(hKeyCurVer);
    }
    else
        logStringF(hModule, L"IsWindows10/RegOpenKeyExW: Error opening CurrentVersion key (%ld)", dwErr);

    return ERROR_SUCCESS; /* Never return failure. */
}

/**
 * Installs and compiles the VBox Python bindings.
 *
 * Called from the MSI installer as custom action.
 *
 * @returns Always ERROR_SUCCESS.
 *          Sets public property VBOX_API_INSTALLED to "0" (false) or "1" (success).
 *
 * @param   hModule             Windows installer module handle.
 */
UINT __stdcall InstallPythonAPI(MSIHANDLE hModule)
{
    logStringF(hModule, L"InstallPythonAPI: Checking for installed Python environment(s) ...");

    /** @todo r=bird: Can't we get the VBOX_PYTHON_PATH property here? */
    wchar_t wszPythonExe[RTPATH_MAX];
    UINT rcWin = getPythonExe(hModule, wszPythonExe, RTPATH_MAX);
    if (rcWin != ERROR_SUCCESS)
    {
        VBoxSetMsiProp(hModule, L"VBOX_API_INSTALLED", L"0");
        return ERROR_SUCCESS;
    }

    /*
     * Set up the VBox API.
     */
    /* Get the VBox API setup string. */
    WCHAR wszVBoxSDKPath[RTPATH_MAX];
    rcWin = VBoxGetMsiProp(hModule, L"CustomActionData", wszVBoxSDKPath, sizeof(wszVBoxSDKPath));
    if (rcWin == ERROR_SUCCESS)
    {
        /* Make sure our current working directory is the VBox installation path. */
        if (SetCurrentDirectoryW(wszVBoxSDKPath))
        {
            /* Set required environment variables. */
            if (SetEnvironmentVariableW(L"VBOX_INSTALL_PATH", wszVBoxSDKPath))
            {
                logStringF(hModule, L"InstallPythonAPI: Invoking vboxapisetup.py in \"%s\" ...", wszVBoxSDKPath);

                rcWin = procRun(hModule, wszPythonExe, L"vboxapisetup.py install");
                if (rcWin == ERROR_SUCCESS)
                {
                    logStringF(hModule, L"InstallPythonAPI: Installation of vboxapisetup.py successful");

                    /*
                     * Do some sanity checking if the VBox API works.
                     */
                    logStringF(hModule, L"InstallPythonAPI: Validating VBox API ...");

                    rcWin = procRun(hModule, wszPythonExe, L"-c \"from vboxapi import VirtualBoxManager\"");
                    if (rcWin == ERROR_SUCCESS)
                    {
                        logStringF(hModule, L"InstallPythonAPI: VBox API looks good.");
                        VBoxSetMsiProp(hModule, L"VBOX_API_INSTALLED", L"1");
                        return ERROR_SUCCESS;
                    }

                    /* failed */
                    logStringF(hModule, L"InstallPythonAPI: Validating VBox API failed with %u (%#x)", rcWin, rcWin);
                }
                else
                    logStringF(hModule, L"InstallPythonAPI: Calling vboxapisetup.py failed with %u (%#x)", rcWin, rcWin);
            }
            else
                logStringF(hModule, L"InstallPythonAPI: Could set environment variable VBOX_INSTALL_PATH: LastError=%u",
                           GetLastError());
        }
        else
            logStringF(hModule, L"InstallPythonAPI: Could set working directory to \"%s\": LastError=%u",
                       wszVBoxSDKPath, GetLastError());
    }
    else
        logStringF(hModule, L"InstallPythonAPI: Unable to retrieve VBox installation directory: rcWin=%u (%#x)", rcWin, rcWin);

    VBoxSetMsiProp(hModule, L"VBOX_API_INSTALLED", L"0");
    logStringF(hModule, L"InstallPythonAPI: Installation failed");
    return ERROR_SUCCESS; /* Do not fail here. */
}

static LONG installBrandingValue(MSIHANDLE hModule,
                                 const WCHAR *pwszFileName,
                                 const WCHAR *pwszSection,
                                 const WCHAR *pwszValue)
{
    LONG rc;
    WCHAR wszValue[_MAX_PATH];
    if (GetPrivateProfileStringW(pwszSection, pwszValue, NULL, wszValue, sizeof(wszValue), pwszFileName) > 0)
    {
        WCHAR wszKey[_MAX_PATH + 64];
        if (wcsicmp(L"General", pwszSection) != 0)
            swprintf_s(wszKey, RT_ELEMENTS(wszKey), L"SOFTWARE\\%S\\VirtualBox\\Branding\\%s", VBOX_VENDOR_SHORT, pwszSection);
        else
            swprintf_s(wszKey, RT_ELEMENTS(wszKey), L"SOFTWARE\\%S\\VirtualBox\\Branding", VBOX_VENDOR_SHORT);

        HKEY hkBranding = NULL;
        rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, wszKey, 0, KEY_WRITE, &hkBranding);
        if (rc == ERROR_SUCCESS)
        {
            rc = RegSetValueExW(hkBranding,
                                pwszValue,
                                NULL,
                                REG_SZ,
                                (BYTE *)wszValue,
                                (DWORD)wcslen(wszValue));
            if (rc != ERROR_SUCCESS)
                logStringF(hModule, L"InstallBranding: Could not write value %s! Error %d", pwszValue, rc);
            RegCloseKey(hkBranding);
        }
    }
    else
        rc = ERROR_NOT_FOUND;
    return rc;
}

/**
 * @note Both paths strings must have an extra terminator.
 */
static UINT CopyDir(MSIHANDLE hModule, const WCHAR *pwszzDstDir, const WCHAR *pwszzSrcDir)
{
    NonStandardAssert(pwszzDstDir[wcslen(pwszzDstDir) + 1] == '\0');
    NonStandardAssert(pwszzSrcDir[wcslen(pwszzSrcDir) + 1] == '\0');

    SHFILEOPSTRUCTW s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_COPY;
    s.pTo = pwszzDstDir;
    s.pFrom = pwszzSrcDir;
    s.fFlags = FOF_SILENT
             | FOF_NOCONFIRMATION
             | FOF_NOCONFIRMMKDIR
             | FOF_NOERRORUI;

    logStringF(hModule, L"CopyDir: pwszzDstDir=%s, pwszzSrcDir=%s", pwszzDstDir, pwszzSrcDir);
    int r = SHFileOperationW(&s);
    if (r == 0)
        return ERROR_SUCCESS;
    logStringF(hModule, L"CopyDir: Copy operation returned status %#x", r);
    return ERROR_GEN_FAILURE;
}

/**
 * @note The directory string must have two zero terminators!
 */
static UINT RemoveDir(MSIHANDLE hModule, const WCHAR *pwszzDstDir)
{
    NonStandardAssert(pwszzDstDir[wcslen(pwszzDstDir) + 1] == '\0');

    SHFILEOPSTRUCTW s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_DELETE;
    s.pFrom = pwszzDstDir;
    s.fFlags = FOF_SILENT
             | FOF_NOCONFIRMATION
             | FOF_NOCONFIRMMKDIR
             | FOF_NOERRORUI;

    logStringF(hModule, L"RemoveDir: pwszzDstDir=%s", pwszzDstDir);
    int r = SHFileOperationW(&s);
    if (r == 0)
        return ERROR_SUCCESS;
    logStringF(hModule, L"RemoveDir: Remove operation returned status %#x", r);
    return ERROR_GEN_FAILURE;
}

/**
 * @note Both paths strings must have an extra terminator.
 */
static UINT RenameDir(MSIHANDLE hModule, const WCHAR *pwszzDstDir, const WCHAR *pwszzSrcDir)
{
    NonStandardAssert(pwszzDstDir[wcslen(pwszzDstDir) + 1] == '\0');
    NonStandardAssert(pwszzSrcDir[wcslen(pwszzSrcDir) + 1] == '\0');

    SHFILEOPSTRUCTW s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_RENAME;
    s.pTo = pwszzDstDir;
    s.pFrom = pwszzSrcDir;
    s.fFlags = FOF_SILENT
             | FOF_NOCONFIRMATION
             | FOF_NOCONFIRMMKDIR
             | FOF_NOERRORUI;

    logStringF(hModule, L"RenameDir: pwszzDstDir=%s, pwszzSrcDir=%s", pwszzDstDir, pwszzSrcDir);
    int r = SHFileOperationW(&s);
    if (r == 0)
        return ERROR_SUCCESS;
    logStringF(hModule, L"RenameDir: Rename operation returned status %#x", r);
    return ERROR_GEN_FAILURE;
}

/** RTPathAppend-like function. */
static UINT AppendToPath(wchar_t *pwszPath, size_t cwcPath, wchar_t *pwszAppend, bool fDoubleTerm = false)
{
    size_t cwcCurPath = wcslen(pwszPath);
    size_t cwcSlash   = cwcCurPath > 1 && RTPATH_IS_SLASH(pwszPath[cwcCurPath - 1]) ? 0 : 1;
    while (RTPATH_IS_SLASH(*pwszAppend))
        pwszAppend++;
    size_t cwcAppend  = wcslen(pwszAppend);
    if (cwcCurPath + cwcCurPath + cwcAppend + fDoubleTerm < cwcPath)
    {
        if (cwcSlash)
            pwszPath[cwcCurPath++] = '\\';
        memcpy(&pwszPath[cwcCurPath], pwszAppend, (cwcAppend + 1) * sizeof(wchar_t));
        if (fDoubleTerm)
            pwszPath[cwcCurPath + cwcAppend + 1] = '\0';
        return ERROR_SUCCESS;
    }
    return ERROR_BUFFER_OVERFLOW;
}

/** RTPathJoin-like function. */
static UINT JoinPaths(wchar_t *pwszPath, size_t cwcPath, wchar_t *pwszPath1, wchar_t *pwszAppend, bool fDoubleTerm = false)
{
    size_t cwcCurPath = wcslen(pwszPath1);
    if (cwcCurPath < cwcPath)
    {
        memcpy(pwszPath, pwszPath1, (cwcCurPath + 1) * sizeof(wchar_t));
        return AppendToPath(pwszPath, cwcPath, pwszAppend, fDoubleTerm);
    }
    return ERROR_BUFFER_OVERFLOW;
}

UINT __stdcall UninstallBranding(MSIHANDLE hModule)
{
    logStringF(hModule, L"UninstallBranding: Handling branding file ...");

    WCHAR wszPath[RTPATH_MAX];
    UINT rc = VBoxGetMsiProp(hModule, L"CustomActionData", wszPath, sizeof(wszPath));
    if (rc == ERROR_SUCCESS)
    {
        size_t const cwcPath = wcslen(wszPath);
        rc = AppendToPath(wszPath, RTPATH_MAX, L"custom", true /*fDoubleTerm*/);
        if (rc == ERROR_SUCCESS)
            rc = RemoveDir(hModule, wszPath);

        /* Check for .custom directory from a failed install and remove it. */
        wszPath[cwcPath] = '\0';
        rc = AppendToPath(wszPath, RTPATH_MAX, L".custom", true /*fDoubleTerm*/);
        if (rc == ERROR_SUCCESS)
            rc = RemoveDir(hModule, wszPath);
    }

    logStringF(hModule, L"UninstallBranding: Handling done. (rc=%u (ignored))", rc);
    return ERROR_SUCCESS; /* Do not fail here. */
}

UINT __stdcall InstallBranding(MSIHANDLE hModule)
{
    logStringF(hModule, L"InstallBranding: Handling branding file ...");

    /*
     * Get the paths.
     */
    wchar_t wszSrcPath[RTPATH_MAX];
    UINT rc = VBoxGetMsiProp(hModule, L"SOURCEDIR", wszSrcPath, RT_ELEMENTS(wszSrcPath));
    if (rc == ERROR_SUCCESS)
    {
        wchar_t wszDstPath[RTPATH_MAX];
        rc = VBoxGetMsiProp(hModule, L"CustomActionData", wszDstPath, RT_ELEMENTS(wszDstPath) - 1);
        if (rc == ERROR_SUCCESS)
        {
            /*
             * First we copy the src\.custom dir to the target.
             */
            rc = AppendToPath(wszSrcPath, RT_ELEMENTS(wszSrcPath) - 1, L".custom", true /*fDoubleTerm*/);
            if (rc == ERROR_SUCCESS)
            {
                rc = CopyDir(hModule, wszDstPath, wszSrcPath);
                if (rc == ERROR_SUCCESS)
                {
                    /*
                     * The rename the '.custom' directory we now got in the target area to 'custom'.
                     */
                    rc = JoinPaths(wszSrcPath, RT_ELEMENTS(wszSrcPath), wszDstPath, L".custom", true /*fDoubleTerm*/);
                    if (rc == ERROR_SUCCESS)
                    {
                        rc = AppendToPath(wszDstPath, RT_ELEMENTS(wszDstPath), L"custom", true /*fDoubleTerm*/);
                        if (rc == ERROR_SUCCESS)
                            rc = RenameDir(hModule, wszDstPath, wszSrcPath);
                    }
                }
            }
        }
    }

    logStringF(hModule, L"InstallBranding: Handling done. (rc=%u (ignored))", rc);
    return ERROR_SUCCESS; /* Do not fail here. */
}

#ifdef VBOX_WITH_NETFLT

/** @todo should use some real VBox app name */
#define VBOX_NETCFG_APP_NAME L"VirtualBox Installer"
#define VBOX_NETCFG_MAX_RETRIES 10
#define NETFLT_PT_INF_REL_PATH L"VBoxNetFlt.inf"
#define NETFLT_MP_INF_REL_PATH L"VBoxNetFltM.inf"
#define NETFLT_ID  L"sun_VBoxNetFlt" /** @todo Needs to be changed (?). */
#define NETADP_ID  L"sun_VBoxNetAdp" /** @todo Needs to be changed (?). */

#define NETLWF_INF_NAME L"VBoxNetLwf.inf"

static MSIHANDLE g_hCurrentModule = NULL;

static UINT _uninstallNetFlt(MSIHANDLE hModule);
static UINT _uninstallNetLwf(MSIHANDLE hModule);

static VOID vboxDrvLoggerCallback(VBOXDRVCFG_LOG_SEVERITY enmSeverity, char *pszMsg, void *pvContext)
{
    RT_NOREF1(pvContext);
    switch (enmSeverity)
    {
        case VBOXDRVCFG_LOG_SEVERITY_FLOW:
        case VBOXDRVCFG_LOG_SEVERITY_REGULAR:
            break;
        case VBOXDRVCFG_LOG_SEVERITY_REL:
            if (g_hCurrentModule)
                logStringF(g_hCurrentModule, L"%S", pszMsg);
            break;
        default:
            break;
    }
}

static DECLCALLBACK(void) netCfgLoggerCallback(const char *pszString)
{
    if (g_hCurrentModule)
        logStringF(g_hCurrentModule, L"%S", pszString);
}

static VOID netCfgLoggerDisable()
{
    if (g_hCurrentModule)
    {
        VBoxNetCfgWinSetLogging(NULL);
        g_hCurrentModule = NULL;
    }
}

static VOID netCfgLoggerEnable(MSIHANDLE hModule)
{
    NonStandardAssert(hModule);

    if (g_hCurrentModule)
        netCfgLoggerDisable();

    g_hCurrentModule = hModule;

    VBoxNetCfgWinSetLogging(netCfgLoggerCallback);
    /* uncomment next line if you want to add logging information from VBoxDrvCfg.cpp */
//    VBoxDrvCfgLoggerSet(vboxDrvLoggerCallback, NULL);
}

static UINT errorConvertFromHResult(MSIHANDLE hModule, HRESULT hr)
{
    UINT uRet;
    switch (hr)
    {
        case S_OK:
            uRet = ERROR_SUCCESS;
            break;

        case NETCFG_S_REBOOT:
        {
            logStringF(hModule, L"Reboot required, setting REBOOT property to \"force\"");
            HRESULT hr2 = MsiSetPropertyW(hModule, L"REBOOT", L"Force");
            if (hr2 != ERROR_SUCCESS)
                logStringF(hModule, L"Failed to set REBOOT property, error = %#x", hr2);
            uRet = ERROR_SUCCESS; /* Never fail here. */
            break;
        }

        default:
            logStringF(hModule, L"Converting unhandled HRESULT (%#x) to ERROR_GEN_FAILURE", hr);
            uRet = ERROR_GEN_FAILURE;
    }
    return uRet;
}

static MSIHANDLE createNetCfgLockedMsgRecord(MSIHANDLE hModule)
{
    MSIHANDLE hRecord = MsiCreateRecord(2);
    if (hRecord)
    {
        UINT uErr = MsiRecordSetInteger(hRecord, 1, 25001);
        if (uErr != ERROR_SUCCESS)
        {
            logStringF(hModule, L"createNetCfgLockedMsgRecord: MsiRecordSetInteger failed, error = %#x", uErr);
            MsiCloseHandle(hRecord);
            hRecord = NULL;
        }
    }
    else
        logStringF(hModule, L"createNetCfgLockedMsgRecord: Failed to create a record");

    return hRecord;
}

static UINT doNetCfgInit(MSIHANDLE hModule, INetCfg **ppnc, BOOL bWrite)
{
    MSIHANDLE hMsg = NULL;
    UINT uErr = ERROR_GEN_FAILURE;
    int MsgResult;
    int cRetries = 0;

    do
    {
        LPWSTR lpszLockedBy;
        HRESULT hr = VBoxNetCfgWinQueryINetCfg(ppnc, bWrite, VBOX_NETCFG_APP_NAME, 10000, &lpszLockedBy);
        if (hr != NETCFG_E_NO_WRITE_LOCK)
        {
            if (FAILED(hr))
                logStringF(hModule, L"doNetCfgInit: VBoxNetCfgWinQueryINetCfg failed, error = %#x", hr);
            uErr = errorConvertFromHResult(hModule, hr);
            break;
        }

        /* hr == NETCFG_E_NO_WRITE_LOCK */

        if (!lpszLockedBy)
        {
            logStringF(hModule, L"doNetCfgInit: lpszLockedBy == NULL, breaking");
            break;
        }

        /* on vista the 6to4svc.dll periodically maintains the lock for some reason,
         * if this is the case, increase the wait period by retrying multiple times
         * NOTE: we could alternatively increase the wait timeout,
         * however it seems unneeded for most cases, e.g. in case some network connection property
         * dialog is opened, it would be better to post a notification to the user as soon as possible
         * rather than waiting for a longer period of time before displaying it */
        if (   cRetries < VBOX_NETCFG_MAX_RETRIES
            && !wcscmp(lpszLockedBy, L"6to4svc.dll"))
        {
            cRetries++;
            logStringF(hModule, L"doNetCfgInit: lpszLockedBy is 6to4svc.dll, retrying %d out of %d", cRetries, VBOX_NETCFG_MAX_RETRIES);
            MsgResult = IDRETRY;
        }
        else
        {
            if (!hMsg)
            {
                hMsg = createNetCfgLockedMsgRecord(hModule);
                if (!hMsg)
                {
                    logStringF(hModule, L"doNetCfgInit: Failed to create a message record, breaking");
                    CoTaskMemFree(lpszLockedBy);
                    break;
                }
            }

            UINT rTmp = MsiRecordSetStringW(hMsg, 2, lpszLockedBy);
            NonStandardAssert(rTmp == ERROR_SUCCESS);
            if (rTmp != ERROR_SUCCESS)
            {
                logStringF(hModule, L"doNetCfgInit: MsiRecordSetStringW failed, error = #%x", rTmp);
                CoTaskMemFree(lpszLockedBy);
                break;
            }

            MsgResult = MsiProcessMessage(hModule, (INSTALLMESSAGE)(INSTALLMESSAGE_USER | MB_RETRYCANCEL), hMsg);
            NonStandardAssert(MsgResult == IDRETRY || MsgResult == IDCANCEL);
            logStringF(hModule, L"doNetCfgInit: MsiProcessMessage returned (%#x)", MsgResult);
        }
        CoTaskMemFree(lpszLockedBy);
    } while(MsgResult == IDRETRY);

    if (hMsg)
        MsiCloseHandle(hMsg);

    return uErr;
}

static UINT vboxNetFltQueryInfArray(MSIHANDLE hModule, OUT LPWSTR pwszPtInf, OUT LPWSTR pwszMpInf, DWORD dwSize)
{
    DWORD dwBuf = dwSize - RT_MAX(sizeof(NETFLT_PT_INF_REL_PATH), sizeof(NETFLT_MP_INF_REL_PATH));
    UINT uErr = MsiGetPropertyW(hModule, L"CustomActionData", pwszPtInf, &dwBuf);
    if (   uErr == ERROR_SUCCESS
        && dwBuf)
    {
        wcscpy(pwszMpInf, pwszPtInf);

        wcsncat(pwszPtInf, NETFLT_PT_INF_REL_PATH, sizeof(NETFLT_PT_INF_REL_PATH));
        logStringF(hModule, L"vboxNetFltQueryInfArray: INF 1: %s", pwszPtInf);

        wcsncat(pwszMpInf, NETFLT_MP_INF_REL_PATH, sizeof(NETFLT_MP_INF_REL_PATH));
        logStringF(hModule, L"vboxNetFltQueryInfArray: INF 2: %s", pwszMpInf);
    }
    else if (uErr != ERROR_SUCCESS)
        logStringF(hModule, L"vboxNetFltQueryInfArray: MsiGetPropertyW failed, error = %#x", uErr);
    else
    {
        logStringF(hModule, L"vboxNetFltQueryInfArray: Empty installation directory");
        uErr = ERROR_GEN_FAILURE;
    }

    return uErr;
}

#endif /*VBOX_WITH_NETFLT*/

/*static*/ UINT _uninstallNetFlt(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    INetCfg *pNetCfg;
    UINT uErr;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {
        logStringF(hModule, L"Uninstalling NetFlt");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetFltUninstall(pNetCfg);
            if (hr != S_OK)
                logStringF(hModule, L"UninstallNetFlt: VBoxNetCfgWinUninstallComponent failed, error = %#x", hr);

            uErr = errorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, L"Uninstalling NetFlt done, error = %#x", uErr);
        }
        else
            logStringF(hModule, L"UninstallNetFlt: doNetCfgInit failed, error = %#x", uErr);
    }
    __finally
    {
        if (bOldIntMode)
        {
            /* The prev mode != FALSE, i.e. non-interactive. */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        netCfgLoggerDisable();
    }
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the uninstall even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall UninstallNetFlt(MSIHANDLE hModule)
{
    (void)_uninstallNetLwf(hModule);
    return _uninstallNetFlt(hModule);
}

static UINT _installNetFlt(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    UINT uErr;
    INetCfg *pNetCfg;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {

        logStringF(hModule, L"InstallNetFlt: Installing NetFlt");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            WCHAR wszPtInf[MAX_PATH];
            WCHAR wszMpInf[MAX_PATH];
            uErr = vboxNetFltQueryInfArray(hModule, wszPtInf, wszMpInf, sizeof(wszMpInf));
            if (uErr == ERROR_SUCCESS)
            {
                LPCWSTR const apwszInfs[] = { wszPtInf, wszMpInf };
                HRESULT hr = VBoxNetCfgWinNetFltInstall(pNetCfg, &apwszInfs[0], RT_ELEMENTS(apwszInfs));
                if (FAILED(hr))
                    logStringF(hModule, L"InstallNetFlt: VBoxNetCfgWinNetFltInstall failed, error = %#x", hr);

                uErr = errorConvertFromHResult(hModule, hr);
            }
            else
                logStringF(hModule, L"InstallNetFlt: vboxNetFltQueryInfArray failed, error = %#x", uErr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, L"InstallNetFlt: Done");
        }
        else
            logStringF(hModule, L"InstallNetFlt: doNetCfgInit failed, error = %#x", uErr);
    }
    __finally
    {
        if (bOldIntMode)
        {
            /* The prev mode != FALSE, i.e. non-interactive. */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        netCfgLoggerDisable();
    }
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the install even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall InstallNetFlt(MSIHANDLE hModule)
{
    (void)_uninstallNetLwf(hModule);
    return _installNetFlt(hModule);
}


/*static*/ UINT _uninstallNetLwf(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    INetCfg *pNetCfg;
    UINT uErr;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {
        logStringF(hModule, L"Uninstalling NetLwf");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetLwfUninstall(pNetCfg);
            if (hr != S_OK)
                logStringF(hModule, L"UninstallNetLwf: VBoxNetCfgWinUninstallComponent failed, error = %#x", hr);

            uErr = errorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, L"Uninstalling NetLwf done, error = %#x", uErr);
        }
        else
            logStringF(hModule, L"UninstallNetLwf: doNetCfgInit failed, error = %#x", uErr);
    }
    __finally
    {
        if (bOldIntMode)
        {
            /* The prev mode != FALSE, i.e. non-interactive. */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        netCfgLoggerDisable();
    }
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the uninstall even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall UninstallNetLwf(MSIHANDLE hModule)
{
    (void)_uninstallNetFlt(hModule);
    return _uninstallNetLwf(hModule);
}

static UINT _installNetLwf(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    UINT uErr;
    INetCfg *pNetCfg;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {

        logStringF(hModule, L"InstallNetLwf: Installing NetLwf");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            WCHAR wszInf[MAX_PATH];
            DWORD cwcInf = RT_ELEMENTS(wszInf) - sizeof(NETLWF_INF_NAME) - 1;
            uErr = MsiGetPropertyW(hModule, L"CustomActionData", wszInf, &cwcInf);
            if (uErr == ERROR_SUCCESS)
            {
                if (cwcInf)
                {
                    if (wszInf[cwcInf - 1] != L'\\')
                    {
                        wszInf[cwcInf++] = L'\\';
                        wszInf[cwcInf]   = L'\0';
                    }

                    wcscat(wszInf, NETLWF_INF_NAME);

                    HRESULT hr = VBoxNetCfgWinNetLwfInstall(pNetCfg, wszInf);
                    if (FAILED(hr))
                        logStringF(hModule, L"InstallNetLwf: VBoxNetCfgWinNetLwfInstall failed, error = %#x", hr);

                    uErr = errorConvertFromHResult(hModule, hr);
                }
                else
                {
                    logStringF(hModule, L"vboxNetFltQueryInfArray: Empty installation directory");
                    uErr = ERROR_GEN_FAILURE;
                }
            }
            else
                logStringF(hModule, L"vboxNetFltQueryInfArray: MsiGetPropertyW failed, error = %#x", uErr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, L"InstallNetLwf: Done");
        }
        else
            logStringF(hModule, L"InstallNetLwf: doNetCfgInit failed, error = %#x", uErr);
    }
    __finally
    {
        if (bOldIntMode)
        {
            /* The prev mode != FALSE, i.e. non-interactive. */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        netCfgLoggerDisable();
    }
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the install even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall InstallNetLwf(MSIHANDLE hModule)
{
    (void)_uninstallNetFlt(hModule);
    return _installNetLwf(hModule);
}


#if 0
static BOOL RenameHostOnlyConnectionsCallback(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext)
{
    WCHAR DevName[256];
    DWORD winEr;

    if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDev,
            SPDRP_FRIENDLYNAME , /* IN DWORD  Property,*/
              NULL, /*OUT PDWORD  PropertyRegDataType,  OPTIONAL*/
              (PBYTE)DevName, /*OUT PBYTE  PropertyBuffer,*/
              sizeof(DevName), /* IN DWORD  PropertyBufferSize,*/
              NULL /*OUT PDWORD  RequiredSize  OPTIONAL*/
            ))
    {
        HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, pDev,
                DICS_FLAG_GLOBAL, /* IN DWORD  Scope,*/
                0, /*IN DWORD  HwProfile, */
                DIREG_DRV, /* IN DWORD  KeyType, */
                KEY_READ /*IN REGSAM  samDesired*/
                );
        NonStandardAssert(hKey != INVALID_HANDLE_VALUE);
        if (hKey != INVALID_HANDLE_VALUE)
        {
            WCHAR guid[50];
            DWORD cbGuid=sizeof(guid);
            winEr = RegQueryValueExW(hKey,
              L"NetCfgInstanceId", /*__in_opt     LPCTSTR lpValueName,*/
              NULL, /*__reserved   LPDWORD lpReserved,*/
              NULL, /*__out_opt    LPDWORD lpType,*/
              (LPBYTE)guid, /*__out_opt    LPBYTE lpData,*/
              &cbGuid /*guid__inout_opt  LPDWORD lpcbData*/
            );
            NonStandardAssert(winEr == ERROR_SUCCESS);
            if (winEr == ERROR_SUCCESS)
            {
                WCHAR ConnectoinName[128];
                ULONG cbName = sizeof(ConnectoinName);

                HRESULT hr = VBoxNetCfgWinGenHostonlyConnectionName (DevName, ConnectoinName, &cbName);
                NonStandardAssert(hr == S_OK);
                if (SUCCEEDED(hr))
                {
                    hr = VBoxNetCfgWinRenameConnection(guid, ConnectoinName);
                    NonStandardAssert(hr == S_OK);
                }
            }
        }
        RegCloseKey(hKey);
    }
    else
    {
        NonStandardAssert(0);
    }

    return TRUE;
}
#endif

static UINT _createHostOnlyInterface(MSIHANDLE hModule, LPCWSTR pwszId, LPCWSTR pwszInfName)
{
#ifdef VBOX_WITH_NETFLT
    netCfgLoggerEnable(hModule);

    BOOL fSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    logStringF(hModule, L"CreateHostOnlyInterface: Creating host-only interface");

    HRESULT hr = E_FAIL;
    GUID guid;
    WCHAR wszMpInf[MAX_PATH];
    DWORD cchMpInf = RT_ELEMENTS(wszMpInf) - (DWORD)wcslen(pwszInfName) - 1 - 1;
    LPCWSTR pwszInfPath = NULL;
    bool fIsFile = false;
    UINT uErr = MsiGetPropertyW(hModule, L"CustomActionData", wszMpInf, &cchMpInf);
    if (uErr == ERROR_SUCCESS)
    {
        if (cchMpInf)
        {
            logStringF(hModule, L"CreateHostOnlyInterface: NetAdpDir property = %s", wszMpInf);
            if (wszMpInf[cchMpInf - 1] != L'\\')
            {
                wszMpInf[cchMpInf++] = L'\\';
                wszMpInf[cchMpInf]   = L'\0';
            }

            wcscat(wszMpInf, pwszInfName);
            pwszInfPath = wszMpInf;
            fIsFile = true;

            logStringF(hModule, L"CreateHostOnlyInterface: Resulting INF path = %s", pwszInfPath);
        }
        else
            logStringF(hModule, L"CreateHostOnlyInterface: VBox installation path is empty");
    }
    else
        logStringF(hModule, L"CreateHostOnlyInterface: Unable to retrieve VBox installation path, error = %#x", uErr);

    /* Make sure the inf file is installed. */
    if (pwszInfPath != NULL && fIsFile)
    {
        logStringF(hModule, L"CreateHostOnlyInterface: Calling VBoxDrvCfgInfInstall(%s)", pwszInfPath);
        hr = VBoxDrvCfgInfInstall(pwszInfPath);
        logStringF(hModule, L"CreateHostOnlyInterface: VBoxDrvCfgInfInstall returns %#x", hr);
        if (FAILED(hr))
            logStringF(hModule, L"CreateHostOnlyInterface: Failed to install INF file, error = %#x", hr);
    }

    if (SUCCEEDED(hr))
    {
        //first, try to update Host Only Network Interface
        BOOL fRebootRequired = FALSE;
        hr = VBoxNetCfgWinUpdateHostOnlyNetworkInterface(pwszInfPath, &fRebootRequired, pwszId);
        if (SUCCEEDED(hr))
        {
            if (fRebootRequired)
            {
                logStringF(hModule, L"CreateHostOnlyInterface: Reboot required for update, setting REBOOT property to force");
                HRESULT hr2 = MsiSetPropertyW(hModule, L"REBOOT", L"Force");
                if (hr2 != ERROR_SUCCESS)
                    logStringF(hModule, L"CreateHostOnlyInterface: Failed to set REBOOT property for update, error = %#x", hr2);
            }
        }
        else
        {
            //in fail case call CreateHostOnlyInterface
            logStringF(hModule, L"CreateHostOnlyInterface: VBoxNetCfgWinUpdateHostOnlyNetworkInterface failed, hr = %#x", hr);
            logStringF(hModule, L"CreateHostOnlyInterface: calling VBoxNetCfgWinCreateHostOnlyNetworkInterface");
#ifdef VBOXNETCFG_DELAYEDRENAME
            BSTR devId;
            hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface(pwszInfPath, fIsFile, NULL, &guid, &devId, NULL);
#else /* !VBOXNETCFG_DELAYEDRENAME */
            hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface(pwszInfPath, fIsFile, NULL, &guid, NULL, NULL);
#endif /* !VBOXNETCFG_DELAYEDRENAME */
            logStringF(hModule, L"CreateHostOnlyInterface: VBoxNetCfgWinCreateHostOnlyNetworkInterface returns %#x", hr);
            if (SUCCEEDED(hr))
            {
                ULONG ip = inet_addr("192.168.56.1");
                ULONG mask = inet_addr("255.255.255.0");
                logStringF(hModule, L"CreateHostOnlyInterface: calling VBoxNetCfgWinEnableStaticIpConfig");
                hr = VBoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
                logStringF(hModule, L"CreateHostOnlyInterface: VBoxNetCfgWinEnableStaticIpConfig returns %#x", hr);
                if (FAILED(hr))
                    logStringF(hModule, L"CreateHostOnlyInterface: VBoxNetCfgWinEnableStaticIpConfig failed, error = %#x", hr);
#ifdef VBOXNETCFG_DELAYEDRENAME
                hr = VBoxNetCfgWinRenameHostOnlyConnection(&guid, devId, NULL);
                if (FAILED(hr))
                    logStringF(hModule, L"CreateHostOnlyInterface: VBoxNetCfgWinRenameHostOnlyConnection failed, error = %#x", hr);
                SysFreeString(devId);
#endif /* VBOXNETCFG_DELAYEDRENAME */
            }
            else
                logStringF(hModule, L"CreateHostOnlyInterface: VBoxNetCfgWinCreateHostOnlyNetworkInterface failed, error = %#x", hr);
        }
    }

    if (SUCCEEDED(hr))
        logStringF(hModule, L"CreateHostOnlyInterface: Creating host-only interface done");

    /* Restore original setup mode. */
    logStringF(hModule, L"CreateHostOnlyInterface: Almost done...");
    if (fSetupModeInteractive)
        SetupSetNonInteractiveMode(fSetupModeInteractive);

    netCfgLoggerDisable();

#endif /* VBOX_WITH_NETFLT */

    logStringF(hModule, L"CreateHostOnlyInterface: Returns success (ignoring all failures)");
    /* Never fail the install even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall CreateHostOnlyInterface(MSIHANDLE hModule)
{
    return _createHostOnlyInterface(hModule, NETADP_ID, L"VBoxNetAdp.inf");
}

UINT __stdcall Ndis6CreateHostOnlyInterface(MSIHANDLE hModule)
{
    return _createHostOnlyInterface(hModule, NETADP_ID, L"VBoxNetAdp6.inf");
}

static UINT _removeHostOnlyInterfaces(MSIHANDLE hModule, LPCWSTR pwszId)
{
#ifdef VBOX_WITH_NETFLT
    netCfgLoggerEnable(hModule);

    logStringF(hModule, L"RemoveHostOnlyInterfaces: Removing all host-only interfaces");

    BOOL fSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    HRESULT hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(pwszId);
    if (SUCCEEDED(hr))
    {
        hr = VBoxDrvCfgInfUninstallAllSetupDi(&GUID_DEVCLASS_NET, L"Net", pwszId, SUOI_FORCEDELETE/* could be SUOI_FORCEDELETE */);
        if (FAILED(hr))
            logStringF(hModule, L"RemoveHostOnlyInterfaces: NetAdp uninstalled successfully, but failed to remove INF files");
        else
            logStringF(hModule, L"RemoveHostOnlyInterfaces: NetAdp uninstalled successfully");
    }
    else
        logStringF(hModule, L"RemoveHostOnlyInterfaces: NetAdp uninstall failed, hr = %#x", hr);

    /* Restore original setup mode. */
    if (fSetupModeInteractive)
        SetupSetNonInteractiveMode(fSetupModeInteractive);

    netCfgLoggerDisable();
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the uninstall even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall RemoveHostOnlyInterfaces(MSIHANDLE hModule)
{
    return _removeHostOnlyInterfaces(hModule, NETADP_ID);
}

static UINT _stopHostOnlyInterfaces(MSIHANDLE hModule, LPCWSTR pwszId)
{
#ifdef VBOX_WITH_NETFLT
    netCfgLoggerEnable(hModule);

    logStringF(hModule, L"StopHostOnlyInterfaces: Stopping all host-only interfaces");

    BOOL fSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    HRESULT hr = VBoxNetCfgWinPropChangeAllNetDevicesOfId(pwszId, VBOXNECTFGWINPROPCHANGE_TYPE_DISABLE);
    if (SUCCEEDED(hr))
        logStringF(hModule, L"StopHostOnlyInterfaces: Disabling host interfaces was successful, hr = %#x", hr);
    else
        logStringF(hModule, L"StopHostOnlyInterfaces: Disabling host interfaces failed, hr = %#x", hr);

    /* Restore original setup mode. */
    if (fSetupModeInteractive)
        SetupSetNonInteractiveMode(fSetupModeInteractive);

    netCfgLoggerDisable();
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the uninstall even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall StopHostOnlyInterfaces(MSIHANDLE hModule)
{
    return _stopHostOnlyInterfaces(hModule, NETADP_ID);
}

static UINT _updateHostOnlyInterfaces(MSIHANDLE hModule, LPCWSTR pwszInfName, LPCWSTR pwszId)
{
#ifdef VBOX_WITH_NETFLT
    netCfgLoggerEnable(hModule);

    logStringF(hModule, L"UpdateHostOnlyInterfaces: Updating all host-only interfaces");

    BOOL fSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    WCHAR wszMpInf[MAX_PATH];
    DWORD cchMpInf = RT_ELEMENTS(wszMpInf) - (DWORD)wcslen(pwszInfName) - 1 - 1;
    LPCWSTR pwszInfPath = NULL;
    bool fIsFile = false;
    UINT uErr = MsiGetPropertyW(hModule, L"CustomActionData", wszMpInf, &cchMpInf);
    if (uErr == ERROR_SUCCESS)
    {
        if (cchMpInf)
        {
            logStringF(hModule, L"UpdateHostOnlyInterfaces: NetAdpDir property = %s", wszMpInf);
            if (wszMpInf[cchMpInf - 1] != L'\\')
            {
                wszMpInf[cchMpInf++] = L'\\';
                wszMpInf[cchMpInf]   = L'\0';
            }

            wcscat(wszMpInf, pwszInfName);
            pwszInfPath = wszMpInf;
            fIsFile = true;

            logStringF(hModule, L"UpdateHostOnlyInterfaces: Resulting INF path = %s", pwszInfPath);

            DWORD attrFile = GetFileAttributesW(pwszInfPath);
            if (attrFile == INVALID_FILE_ATTRIBUTES)
            {
                DWORD dwErr = GetLastError();
                logStringF(hModule, L"UpdateHostOnlyInterfaces: File \"%s\" not found, dwErr=%ld", pwszInfPath, dwErr);
            }
            else
            {
                logStringF(hModule, L"UpdateHostOnlyInterfaces: File \"%s\" exists", pwszInfPath);

                BOOL fRebootRequired = FALSE;
                HRESULT hr = VBoxNetCfgWinUpdateHostOnlyNetworkInterface(pwszInfPath, &fRebootRequired, pwszId);
                if (SUCCEEDED(hr))
                {
                    if (fRebootRequired)
                    {
                        logStringF(hModule, L"UpdateHostOnlyInterfaces: Reboot required, setting REBOOT property to force");
                        HRESULT hr2 = MsiSetPropertyW(hModule, L"REBOOT", L"Force");
                        if (hr2 != ERROR_SUCCESS)
                            logStringF(hModule, L"UpdateHostOnlyInterfaces: Failed to set REBOOT property, error = %#x", hr2);
                    }
                }
                else
                    logStringF(hModule, L"UpdateHostOnlyInterfaces: VBoxNetCfgWinUpdateHostOnlyNetworkInterface failed, hr = %#x", hr);
            }
        }
        else
            logStringF(hModule, L"UpdateHostOnlyInterfaces: VBox installation path is empty");
    }
    else
        logStringF(hModule, L"UpdateHostOnlyInterfaces: Unable to retrieve VBox installation path, error = %#x", uErr);

    /* Restore original setup mode. */
    if (fSetupModeInteractive)
        SetupSetNonInteractiveMode(fSetupModeInteractive);

    netCfgLoggerDisable();
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the update even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall UpdateHostOnlyInterfaces(MSIHANDLE hModule)
{
    return _updateHostOnlyInterfaces(hModule, L"VBoxNetAdp.inf", NETADP_ID);
}

UINT __stdcall Ndis6UpdateHostOnlyInterfaces(MSIHANDLE hModule)
{
    return _updateHostOnlyInterfaces(hModule, L"VBoxNetAdp6.inf", NETADP_ID);
}

static UINT _uninstallNetAdp(MSIHANDLE hModule, LPCWSTR pwszId)
{
#ifdef VBOX_WITH_NETFLT
    INetCfg *pNetCfg;
    UINT uErr;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {
        logStringF(hModule, L"Uninstalling NetAdp");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetAdpUninstall(pNetCfg, pwszId);
            if (hr != S_OK)
                logStringF(hModule, L"UninstallNetAdp: VBoxNetCfgWinUninstallComponent failed, error = %#x", hr);

            uErr = errorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, L"Uninstalling NetAdp done, error = %#x", uErr);
        }
        else
            logStringF(hModule, L"UninstallNetAdp: doNetCfgInit failed, error = %#x", uErr);
    }
    __finally
    {
        if (bOldIntMode)
        {
            /* The prev mode != FALSE, i.e. non-interactive. */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        netCfgLoggerDisable();
    }
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the uninstall even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall UninstallNetAdp(MSIHANDLE hModule)
{
    return _uninstallNetAdp(hModule, NETADP_ID);
}

static bool isTAPDevice(const WCHAR *pwszGUID)
{
    HKEY hNetcard;
    bool bIsTapDevice = false;
    LONG lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                 L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}",
                                 0, KEY_READ, &hNetcard);
    if (lStatus != ERROR_SUCCESS)
        return false;

    int i = 0;
    for (;;)
    {
        WCHAR wszEnumName[256];
        WCHAR wszNetCfgInstanceId[256];
        DWORD dwKeyType;
        HKEY  hNetCardGUID;

        DWORD dwLen = sizeof(wszEnumName);
        lStatus = RegEnumKeyExW(hNetcard, i, wszEnumName, &dwLen, NULL, NULL, NULL, NULL);
        if (lStatus != ERROR_SUCCESS)
            break;

        lStatus = RegOpenKeyExW(hNetcard, wszEnumName, 0, KEY_READ, &hNetCardGUID);
        if (lStatus == ERROR_SUCCESS)
        {
            dwLen = sizeof(wszNetCfgInstanceId);
            lStatus = RegQueryValueExW(hNetCardGUID, L"NetCfgInstanceId", NULL, &dwKeyType, (LPBYTE)wszNetCfgInstanceId, &dwLen);
            if (   lStatus == ERROR_SUCCESS
                && dwKeyType == REG_SZ)
            {
                WCHAR wszNetProductName[256];
                WCHAR wszNetProviderName[256];

                wszNetProductName[0] = 0;
                dwLen = sizeof(wszNetProductName);
                lStatus = RegQueryValueExW(hNetCardGUID, L"ProductName", NULL, &dwKeyType, (LPBYTE)wszNetProductName, &dwLen);

                wszNetProviderName[0] = 0;
                dwLen = sizeof(wszNetProviderName);
                lStatus = RegQueryValueExW(hNetCardGUID, L"ProviderName", NULL, &dwKeyType, (LPBYTE)wszNetProviderName, &dwLen);

                if (   !wcscmp(wszNetCfgInstanceId, pwszGUID)
                    && !wcscmp(wszNetProductName, L"VirtualBox TAP Adapter")
                    && (   (!wcscmp(wszNetProviderName, L"innotek GmbH")) /* Legacy stuff. */
                        || (!wcscmp(wszNetProviderName, L"Sun Microsystems, Inc.")) /* Legacy stuff. */
                        || (!wcscmp(wszNetProviderName, MY_WTEXT(VBOX_VENDOR))) /* Reflects current vendor string. */
                       )
                   )
                {
                    bIsTapDevice = true;
                    RegCloseKey(hNetCardGUID);
                    break;
                }
            }
            RegCloseKey(hNetCardGUID);
        }
        ++i;
    }

    RegCloseKey(hNetcard);
    return bIsTapDevice;
}

/** @todo r=andy BUGBUG WTF! Why do we a) set the rc to 0 (success), and b) need this macro at all!? */
#define SetErrBreak(args) \
    if (1) { \
        rc = 0; \
        logStringF args; \
        break; \
    } else do {} while (0)

int removeNetworkInterface(MSIHANDLE hModule, const WCHAR *pwszGUID)
{
    int rc = 1;
    do /* break-loop */
    {
        WCHAR wszPnPInstanceId[512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do /* break-loop */
        {
            WCHAR wszRegLocation[256];
            swprintf_s(wszRegLocation, RT_ELEMENTS(wszRegLocation),
                       L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s", pwszGUID);
            LONG lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, wszRegLocation, 0, KEY_READ, &hkeyNetwork);
            if (lStatus != ERROR_SUCCESS || !hkeyNetwork)
                SetErrBreak((hModule, L"VBox HostInterfaces: Host interface network was not found in registry (%s)! [1]",
                             wszRegLocation));

            lStatus = RegOpenKeyExW(hkeyNetwork, L"Connection", 0, KEY_READ, &hkeyConnection);
            if (lStatus != ERROR_SUCCESS || !hkeyConnection)
                SetErrBreak((hModule, L"VBox HostInterfaces: Host interface network was not found in registry (%s)! [2]",
                             wszRegLocation));

            DWORD len = sizeof(wszPnPInstanceId);
            DWORD dwKeyType;
            lStatus = RegQueryValueExW(hkeyConnection, L"PnPInstanceID", NULL, &dwKeyType, (LPBYTE)&wszPnPInstanceId[0], &len);
            if (lStatus != ERROR_SUCCESS || (dwKeyType != REG_SZ))
                SetErrBreak((hModule, L"VBox HostInterfaces: Host interface network was not found in registry (%s)! [3]",
                             wszRegLocation));
        }
        while (0);

        if (hkeyConnection)
            RegCloseKey(hkeyConnection);
        if (hkeyNetwork)
            RegCloseKey(hkeyNetwork);

        /*
         * Now we are going to enumerate all network devices and
         * wait until we encounter the right device instance ID
         */

        HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;
        BOOL fResult;

        do /* break-loop */
        {
            GUID netGuid;
            SP_DEVINFO_DATA DeviceInfoData;
            DWORD index = 0;
            DWORD size = 0;

            /* initialize the structure size */
            DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            /* copy the net class GUID */
            memcpy(&netGuid, &GUID_DEVCLASS_NET, sizeof (GUID_DEVCLASS_NET));

            /* return a device info set contains all installed devices of the Net class */
            hDeviceInfo = SetupDiGetClassDevs(&netGuid, NULL, NULL, DIGCF_PRESENT);
            if (hDeviceInfo == INVALID_HANDLE_VALUE)
            {
                logStringF(hModule, L"VBox HostInterfaces: SetupDiGetClassDevs failed (0x%08X)!", GetLastError());
                SetErrBreak((hModule, L"VBox HostInterfaces: Uninstallation failed!"));
            }

            BOOL fFoundDevice = FALSE;

            /* enumerate the driver info list */
            while (TRUE)
            {
                WCHAR *pwszDeviceHwid;

                fResult = SetupDiEnumDeviceInfo(hDeviceInfo, index, &DeviceInfoData);
                if (!fResult)
                {
                    if (GetLastError() == ERROR_NO_MORE_ITEMS)
                        break;
                    else
                    {
                        index++;
                        continue;
                    }
                }

                /* try to get the hardware ID registry property */
                fResult = SetupDiGetDeviceRegistryProperty(hDeviceInfo,
                                                           &DeviceInfoData,
                                                           SPDRP_HARDWAREID,
                                                           NULL,
                                                           NULL,
                                                           0,
                                                           &size);
                if (!fResult)
                {
                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                    {
                        index++;
                        continue;
                    }

                    pwszDeviceHwid = (WCHAR *)malloc(size);
                    if (pwszDeviceHwid)
                    {
                        fResult = SetupDiGetDeviceRegistryProperty(hDeviceInfo,
                                                                   &DeviceInfoData,
                                                                   SPDRP_HARDWAREID,
                                                                   NULL,
                                                                   (PBYTE)pwszDeviceHwid,
                                                                   size,
                                                                   NULL);
                        if (!fResult)
                        {
                            free(pwszDeviceHwid);
                            pwszDeviceHwid = NULL;
                            index++;
                            continue;
                        }
                    }
                }
                else
                {
                    /* something is wrong.  This shouldn't have worked with a NULL buffer */
                    index++;
                    continue;
                }

                for (WCHAR *t = pwszDeviceHwid;
                     t && *t && t < &pwszDeviceHwid[size / sizeof(WCHAR)];
                     t += wcslen(t) + 1)
                {
                    if (!_wcsicmp(L"vboxtap", t))
                    {
                          /* get the device instance ID */
                          WCHAR wszDevID[MAX_DEVICE_ID_LEN];
                          if (CM_Get_Device_IDW(DeviceInfoData.DevInst,
                                                wszDevID, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
                          {
                              /* compare to what we determined before */
                              if (!wcscmp(wszDevID, wszPnPInstanceId))
                              {
                                  fFoundDevice = TRUE;
                                  break;
                              }
                          }
                    }
                }

                if (pwszDeviceHwid)
                {
                    free(pwszDeviceHwid);
                    pwszDeviceHwid = NULL;
                }

                if (fFoundDevice)
                    break;

                index++;
            }

            if (fFoundDevice)
            {
                fResult = SetupDiSetSelectedDevice(hDeviceInfo, &DeviceInfoData);
                if (!fResult)
                {
                    logStringF(hModule, L"VBox HostInterfaces: SetupDiSetSelectedDevice failed (0x%08X)!", GetLastError());
                    SetErrBreak((hModule, L"VBox HostInterfaces: Uninstallation failed!"));
                }

                fResult = SetupDiCallClassInstaller(DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
                if (!fResult)
                {
                    logStringF(hModule, L"VBox HostInterfaces: SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)!", GetLastError());
                    SetErrBreak((hModule, L"VBox HostInterfaces: Uninstallation failed!"));
                }
            }
            else
                SetErrBreak((hModule, L"VBox HostInterfaces: Host interface network device not found!"));
        } while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList(hDeviceInfo);
    } while (0);
    return rc;
}

UINT __stdcall UninstallTAPInstances(MSIHANDLE hModule)
{
    static const wchar_t s_wszNetworkKey[] = L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
    HKEY hCtrlNet;

    LONG lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, s_wszNetworkKey, 0, KEY_READ, &hCtrlNet);
    if (lStatus == ERROR_SUCCESS)
    {
        logStringF(hModule, L"VBox HostInterfaces: Enumerating interfaces ...");
        for (int i = 0; ; ++i)
        {
            WCHAR wszNetworkGUID[256] = { 0 };
            DWORD dwLen = (DWORD)sizeof(wszNetworkGUID);
            lStatus = RegEnumKeyExW(hCtrlNet, i, wszNetworkGUID, &dwLen, NULL, NULL, NULL, NULL);
            if (lStatus != ERROR_SUCCESS)
            {
                switch (lStatus)
                {
                    case ERROR_NO_MORE_ITEMS:
                        logStringF(hModule, L"VBox HostInterfaces: No interfaces found.");
                        break;
                    default:
                        logStringF(hModule, L"VBox HostInterfaces: Enumeration failed: %ld", lStatus);
                        break;
                }
                break;
            }

            if (isTAPDevice(wszNetworkGUID))
            {
                logStringF(hModule, L"VBox HostInterfaces: Removing interface \"%s\" ...", wszNetworkGUID);
                removeNetworkInterface(hModule, wszNetworkGUID);
                lStatus = RegDeleteKeyW(hCtrlNet, wszNetworkGUID);
            }
        }
        RegCloseKey(hCtrlNet);
        logStringF(hModule, L"VBox HostInterfaces: Removing interfaces done.");
    }
    return ERROR_SUCCESS;
}


/**
 * This is used to remove the old VBoxDrv service before installation.
 *
 * The current service name is VBoxSup but the INF file won't remove the old
 * one, so we do it manually to try prevent trouble as the device nodes are the
 * same and we would fail starting VBoxSup.sys if VBoxDrv.sys is still loading.
 *
 * Status code is ignored for now as a reboot should fix most potential trouble
 * here (and I don't want to break stuff too badly).
 *
 * @sa @bugref{10162}
 */
UINT __stdcall UninstallVBoxDrv(MSIHANDLE hModule)
{
    /*
     * Try open the service.
     */
    SC_HANDLE hSMgr = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (hSMgr)
    {
        SC_HANDLE hService = OpenServiceW(hSMgr, L"VBoxDrv", DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (hService)
        {
            /*
             * Try stop it before we delete it.
             */
            SERVICE_STATUS Status = { 0, 0, 0, 0, 0, 0, 0 };
            QueryServiceStatus(hService, &Status);
            if (Status.dwCurrentState == SERVICE_STOPPED)
                logStringF(hModule, L"VBoxDrv: The service old service was already stopped");
            else
            {
                logStringF(hModule, L"VBoxDrv: Stopping the service (state %u)", Status.dwCurrentState);
                if (ControlService(hService, SERVICE_CONTROL_STOP, &Status))
                {
                    /* waiting for it to stop: */
                    int iWait = 100;
                    while (Status.dwCurrentState == SERVICE_STOP_PENDING && iWait-- > 0)
                    {
                        Sleep(100);
                        QueryServiceStatus(hService, &Status);
                    }

                    if (Status.dwCurrentState == SERVICE_STOPPED)
                        logStringF(hModule, L"VBoxDrv: Stopped service");
                    else
                        logStringF(hModule, L"VBoxDrv: Failed to stop the service, status: %u", Status.dwCurrentState);
                }
                else
                {
                    DWORD const dwErr = GetLastError();
                    if (   Status.dwCurrentState == SERVICE_STOP_PENDING
                        && dwErr == ERROR_SERVICE_CANNOT_ACCEPT_CTRL)
                        logStringF(hModule, L"VBoxDrv: Failed to stop the service: stop pending, not accepting control messages");
                    else
                        logStringF(hModule, L"VBoxDrv: Failed to stop the service: dwErr=%u status=%u", dwErr, Status.dwCurrentState);
                }
            }

            /*
             * Delete the service, or at least mark it for deletion.
             */
            if (DeleteService(hService))
                logStringF(hModule, L"VBoxDrv: Successfully delete service");
            else
                logStringF(hModule, L"VBoxDrv: Failed to delete the service: %u", GetLastError());

            CloseServiceHandle(hService);
        }
        else
        {
            DWORD const dwErr = GetLastError();
            if (dwErr == ERROR_SERVICE_DOES_NOT_EXIST)
                logStringF(hModule, L"VBoxDrv: Nothing to do, the old service does not exist");
            else
                logStringF(hModule, L"VBoxDrv: Failed to open the service: %u", dwErr);
        }

        CloseServiceHandle(hSMgr);
    }
    else
        logStringF(hModule, L"VBoxDrv: Failed to open service manager (%u).", GetLastError());

    return ERROR_SUCCESS;
}

