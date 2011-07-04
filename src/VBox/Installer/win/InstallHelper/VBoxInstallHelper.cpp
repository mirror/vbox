/* $Id$ */
/** @file
 * VBoxInstallHelper - Various helper routines for Windows host installer.
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_NETFLT
# include "VBox/VBoxNetCfg-win.h"
# include "VBox/VBoxDrvCfg-win.h"
#endif /* VBOX_WITH_NETFLT */

#include <VBox/version.h>

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <msi.h>
#include <msiquery.h>

#define _WIN32_DCOM
#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#define INITGUID
#include <guiddef.h>
#include <devguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <shlobj.h>
#include <cfgmgr32.h>

#include "VBoxCommon.h"

#ifndef VBOX_OSE
# include "internal/VBoxSerial.h"
#endif

#ifdef DEBUG
# define Assert(_expr) assert(_expr)
#else
# define Assert(_expr) do{ }while(0)
#endif

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    return TRUE;
}

static void LogString(MSIHANDLE hInstall, LPCSTR szString, ...)
{
    PMSIHANDLE newHandle = MsiCreateRecord(2);

    char szBuffer[1024] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnprintf(szBuffer, sizeof(szBuffer) / sizeof(char), szString, pArgList);
    va_end(pArgList);

    MsiRecordSetStringA(newHandle, 0, szBuffer);
    MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_INFO), newHandle);
    MsiCloseHandle(newHandle);

#ifdef DEBUG
    _tprintf(_T("Debug: %s\n"), szBuffer);
#endif
}

static void LogStringW(MSIHANDLE hInstall, LPCWSTR szString, ...)
{
    PMSIHANDLE newHandle = MsiCreateRecord(2);

    TCHAR szBuffer[1024] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnwprintf(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szString, pArgList);
    va_end(pArgList);

    MsiRecordSetStringW(newHandle, 0, szBuffer);
    MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_INFO), newHandle);
    MsiCloseHandle(newHandle);
}

UINT __stdcall IsSerialCheckNeeded(MSIHANDLE hModule)
{
#ifndef VBOX_OSE
    /*BOOL bRet =*/ serialCheckNeeded(hModule);
#endif
    return ERROR_SUCCESS;
}

UINT __stdcall CheckSerial(MSIHANDLE hModule)
{
#ifndef VBOX_OSE
    /*BOOL bRet =*/ serialIsValid(hModule);
#endif
    return ERROR_SUCCESS;
}

DWORD Exec(MSIHANDLE hModule,
           const TCHAR *pszAppName, TCHAR *pszCmdLine, const TCHAR *pszWorkDir,
           DWORD *pdwExitCode)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD rc = ERROR_SUCCESS;

    ZeroMemory(&si, sizeof(si));
    si.dwFlags = STARTF_USESHOWWINDOW;
#ifdef UNICODE
    si.dwFlags |= CREATE_UNICODE_ENVIRONMENT;
#endif
    si.wShowWindow = SW_HIDE; /* For debugging: SW_SHOW; */
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    LogStringW(hModule, TEXT("Executing command line: %s %s (Working Dir: %s)"),
               pszAppName, pszCmdLine, pszWorkDir == NULL ? L"Current" : pszWorkDir);

    SetLastError(0);
    if (!CreateProcess(pszAppName,    /* Module name. */
                       pszCmdLine,    /* Command line. */
                       NULL,         /* Process handle not inheritable. */
                       NULL,         /* Thread handle not inheritable. */
                       FALSE,        /* Set handle inheritance to FALSE .*/
                       0,            /* No creation flags. */
                       NULL,         /* Use parent's environment block. */
                       pszWorkDir,    /* Use parent's starting directory. */
                       &si,          /* Pointer to STARTUPINFO structure. */
                       &pi))         /* Pointer to PROCESS_INFORMATION structure. */
    {
        rc = GetLastError();
        LogStringW(hModule, TEXT("Executing command line: CreateProcess() failed! Error: %ld"), rc);
        return rc;
    }

    /* Wait until child process exits. */
    if (WAIT_FAILED == WaitForSingleObject(pi.hProcess, 30 * 1000 /* Wait 30 secs max. */))
    {
        rc = GetLastError();
        LogStringW(hModule, TEXT("Executing command line: WaitForSingleObject() failed! Error: %ld"), rc);
    }
    else
    {
        if (!GetExitCodeProcess(pi.hProcess, pdwExitCode))
        {
            rc = GetLastError();
            LogStringW(hModule, TEXT("Executing command line: GetExitCodeProcess() failed! Error: %ld"), rc);
        }
    }

    /* Close process and thread handles. */
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    LogStringW(hModule, TEXT("Executing command returned: %ld (exit code %ld)"), rc, *pdwExitCode);
    return rc;
}

UINT __stdcall InstallPythonAPI(MSIHANDLE hModule)
{
    LogStringW(hModule, TEXT("InstallPythonAPI: Checking for installed Python environment ..."));

    HKEY hkPythonCore = NULL;
    BOOL bFound = FALSE;
    LONG rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Python\\PythonCore", 0, KEY_READ, &hkPythonCore);
    if (rc != ERROR_SUCCESS)
    {
        LogStringW(hModule, TEXT("InstallPythonAPI: Python seems not to be installed."));
        return ERROR_SUCCESS;
    }

    TCHAR szPath[MAX_PATH] = { 0 };
    TCHAR szVal[MAX_PATH] = { 0 };

    for (int i = 0;; ++i)
    {
        TCHAR szRoot[MAX_PATH] = { 0 };
        DWORD dwLen = sizeof (szPath);
        DWORD dwKeyType = REG_SZ;

        rc = RegEnumKeyEx(hkPythonCore, i, szRoot, &dwLen, NULL, NULL, NULL, NULL);
        if (rc != ERROR_SUCCESS || dwLen <= 0)
            break;

        _stprintf_s(szPath, sizeof(szPath) / sizeof(TCHAR), L"%s\\InstallPath", szRoot);
        dwLen = sizeof(szVal);

        HKEY hkPythonInstPath = NULL;
        rc = RegOpenKeyEx(hkPythonCore, szPath, 0, KEY_READ,  &hkPythonInstPath);
        if (rc != ERROR_SUCCESS)
            continue;

        rc = RegQueryValueEx(hkPythonInstPath, L"", NULL, &dwKeyType, (LPBYTE)szVal, &dwLen);
        if (rc == ERROR_SUCCESS)
            LogStringW(hModule, TEXT("InstallPythonAPI: Path \"%s\" detected."), szVal);

        RegCloseKey(hkPythonInstPath);
    }
    RegCloseKey(hkPythonCore);

    /* Python path found? */
    TCHAR szExec[MAX_PATH] = { 0 };
    TCHAR szCmdLine[MAX_PATH] = { 0 };
    DWORD dwExitCode = 0;
    if (_tcslen(szVal) > 0)
    {
        /* Cool, check for installed Win32 extensions. */
        LogStringW(hModule, TEXT("InstallPythonAPI: Python installed. Checking for Win32 extensions ..."));
        _stprintf_s(szExec, sizeof(szExec) / sizeof(TCHAR), L"%s\\python.exe", szVal);
        _stprintf_s(szCmdLine, sizeof(szCmdLine) / sizeof(TCHAR), L"%s\\python.exe -c \"import win32api\"", szVal);

        DWORD dwRetExec = Exec(hModule, szExec, szCmdLine, NULL, &dwExitCode);
        if (   (ERROR_SUCCESS == dwRetExec)
            && (            0 == dwExitCode))
        {
            /* Did we get the correct error level (=0)? */
            LogStringW(hModule, TEXT("InstallPythonAPI: Win32 extensions installed."));
            bFound = TRUE;
        }
        else
            LogStringW(hModule, TEXT("InstallPythonAPI: Win32 extensions not found."));
    }

    BOOL bInstalled = FALSE;
    if (bFound) /* Is Python and all required stuff installed? */
    {
        /* Get the VBoxAPI setup string. */
        TCHAR szPathTargetDir[MAX_PATH] = {0};
        VBoxGetProperty(hModule, L"CustomActionData", szPathTargetDir, sizeof(szPathTargetDir));
        if (_tcslen(szPathTargetDir))
        {

            /* Set final path. */
            _stprintf_s(szPath, sizeof(szPath) / sizeof(TCHAR), L"%s\\sdk\\install", szPathTargetDir);

            /* Install our API module. */
            _stprintf_s(szCmdLine, sizeof(szCmdLine) / sizeof(TCHAR), L"%s\\python.exe vboxapisetup.py install", szVal);

            /* Set required environment variables. */
            if (!SetEnvironmentVariable(L"VBOX_INSTALL_PATH", szPathTargetDir))
            {
                LogStringW(hModule, TEXT("InstallPythonAPI: Could set environment variable VBOX_INSTALL_PATH!"));
            }
            else
            {
                DWORD dwRetExec = Exec(hModule, szExec, szCmdLine, szPath, &dwExitCode);
                if (   (ERROR_SUCCESS == dwRetExec)
                    && (            0 == dwExitCode))
                {
                    /* All done! */
                    LogStringW(hModule, TEXT("InstallPythonAPI: VBoxAPI for Python successfully installed."));
                    bInstalled = TRUE;
                }
                else
                {
                    if (dwRetExec)
                        LogStringW(hModule, TEXT("InstallPythonAPI: Error while executing installation of VBox API: %ld"), dwRetExec);
                    else
                        LogStringW(hModule, TEXT("InstallPythonAPI: Python reported an error while installing VBox API: %ld"), dwExitCode);
                }
            }
        }
        else
            LogStringW(hModule, TEXT("InstallPythonAPI: Unable to retrieve VBox installation path!"));
    }

    VBoxSetProperty(hModule, L"PYTHON_INSTALLED", bInstalled ? L"1" : L"0");

    if (!bInstalled)
        LogStringW(hModule, TEXT("InstallPythonAPI: VBox API not installed."));
    return ERROR_SUCCESS; /* Do not fail here. */
}

static LONG InstallBrandingValue(MSIHANDLE hModule,
                                 const TCHAR* pszFileName,
                                 const TCHAR* pszSection,
                                 const TCHAR* pszValue)
{
    LONG rc;
    TCHAR szValue[_MAX_PATH];
    if (GetPrivateProfileString(pszSection, pszValue, NULL,
                                szValue, sizeof(szValue), pszFileName) > 0)
    {
        HKEY hkBranding;
        TCHAR szKey[_MAX_PATH];

        if (wcsicmp(L"General", pszSection) != 0)
            _stprintf_s(szKey, sizeof(szKey) / sizeof(TCHAR), L"SOFTWARE\\%s\\VirtualBox\\Branding\\", VBOX_VENDOR_SHORT, pszSection);
        else
            _stprintf_s(szKey, sizeof(szKey) / sizeof(TCHAR), L"SOFTWARE\\%s\\VirtualBox\\Branding", VBOX_VENDOR_SHORT);

        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey,
                          0, KEY_WRITE, &hkBranding);
        if (rc == ERROR_SUCCESS)
        {
            rc = RegSetValueEx(hkBranding,
                               pszValue,
                               NULL,
                               REG_SZ,
                               (BYTE*)szValue,
                               (DWORD)wcslen(szValue));
            if (rc != ERROR_SUCCESS)
                LogStringW(hModule, TEXT("InstallBranding: Could not write value %s! Error %ld"), pszValue, rc);
            RegCloseKey (hkBranding);
        }
    }
    else
        rc = ERROR_NOT_FOUND;
    return rc;
}

UINT CopyDir(MSIHANDLE hModule, const TCHAR *pszDestDir, const TCHAR *pszSourceDir)
{
    UINT rc;
    TCHAR szDest[_MAX_PATH + 1];
    TCHAR szSource[_MAX_PATH + 1];

    _stprintf_s(szDest, sizeof(szDest) / sizeof(TCHAR), L"%s%c", pszDestDir, '\0');
    _stprintf_s(szSource, sizeof(szSource) / sizeof(TCHAR), L"%s%c", pszSourceDir, '\0');

    SHFILEOPSTRUCT s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_COPY;
    s.pTo = szDest;
    s.pFrom = szSource;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    LogStringW(hModule, TEXT("CopyDir: DestDir=%s, SourceDir=%s"),
              szDest, szSource);
    int r = SHFileOperation(&s);
    if (r != 0)
    {
        LogStringW(hModule, TEXT("CopyDir: Copy operation returned status 0x%x"), r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT RemoveDir(MSIHANDLE hModule, const TCHAR *pszDestDir)
{
    UINT rc;
    TCHAR szDest[_MAX_PATH + 1];

    _stprintf_s(szDest, sizeof(szDest) / sizeof(TCHAR), L"%s%c", pszDestDir, '\0');

    SHFILEOPSTRUCT s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_DELETE;
    s.pFrom = szDest;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    LogStringW(hModule, TEXT("RemoveDir: DestDir=%s"), szDest);
    int r = SHFileOperation(&s);
    if (r != 0)
    {
        LogStringW(hModule, TEXT("RemoveDir: Remove operation returned status 0x%x"), r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT RenameDir(MSIHANDLE hModule, const TCHAR *pszDestDir, const TCHAR *pszSourceDir)
{
    UINT rc;
    TCHAR szDest[_MAX_PATH + 1];
    TCHAR szSource[_MAX_PATH + 1];

    _stprintf_s(szDest, sizeof(szDest) / sizeof(TCHAR), L"%s%c", pszDestDir, '\0');
    _stprintf_s(szSource, sizeof(szSource) / sizeof(TCHAR), L"%s%c", pszSourceDir, '\0');

    SHFILEOPSTRUCT s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_RENAME;
    s.pTo = szDest;
    s.pFrom = szSource;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    LogStringW(hModule, TEXT("RenameDir: DestDir=%s, SourceDir=%s"),
              szDest, szSource);
    int r = SHFileOperation(&s);
    if (r != 0)
    {
        LogStringW(hModule, TEXT("RenameDir: Rename operation returned status 0x%x"), r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT __stdcall UninstallBranding(MSIHANDLE hModule)
{
    UINT rc;
    LogStringW(hModule, TEXT("UninstallBranding: Handling branding file ..."));

    TCHAR szPathTargetDir[_MAX_PATH];
    TCHAR szPathDest[_MAX_PATH];

    rc = VBoxGetProperty(hModule, L"CustomActionData", szPathTargetDir, sizeof(szPathTargetDir));
    if (rc == ERROR_SUCCESS)
    {
        /** @todo Check trailing slash after %s. */
        _stprintf_s(szPathDest, sizeof(szPathDest) / sizeof(TCHAR), L"%scustom", szPathTargetDir);
        rc = RemoveDir(hModule, szPathDest);
        if (rc != ERROR_SUCCESS)
        {
            /* Check for hidden .custom directory and remove it. */
            _stprintf_s(szPathDest, sizeof(szPathDest) / sizeof(TCHAR), L"%s.custom", szPathTargetDir);
            rc = RemoveDir(hModule, szPathDest);
        }
    }

    LogStringW(hModule, TEXT("UninstallBranding: Handling done."));
    return ERROR_SUCCESS; /* Do not fail here. */
}

UINT __stdcall InstallBranding(MSIHANDLE hModule)
{
    UINT rc;
    LogStringW(hModule, TEXT("InstallBranding: Handling branding file ..."));

    TCHAR szPathMSI[_MAX_PATH];
    TCHAR szPathTargetDir[_MAX_PATH];

    TCHAR szPathSource[_MAX_PATH];
    TCHAR szPathDest[_MAX_PATH];

    rc = VBoxGetProperty(hModule, L"SOURCEDIR", szPathMSI, sizeof(szPathMSI));
    if (rc == ERROR_SUCCESS)
    {
        rc = VBoxGetProperty(hModule, L"CustomActionData", szPathTargetDir, sizeof(szPathTargetDir));
        if (rc == ERROR_SUCCESS)
        {
            /** @todo Check for trailing slash after %s. */
            _stprintf_s(szPathDest, sizeof(szPathDest) / sizeof(TCHAR), L"%s", szPathTargetDir);
            _stprintf_s(szPathSource, sizeof(szPathSource) / sizeof(TCHAR), L"%s.custom", szPathMSI);
            rc = CopyDir(hModule, szPathDest, szPathSource);
            if (rc == ERROR_SUCCESS)
            {
                _stprintf_s(szPathDest, sizeof(szPathDest) / sizeof(TCHAR), L"%scustom", szPathTargetDir);
                _stprintf_s(szPathSource, sizeof(szPathSource) / sizeof(TCHAR), L"%s.custom", szPathTargetDir);
                rc = RenameDir(hModule, szPathDest, szPathSource);
            }
        }
    }

    LogStringW(hModule, TEXT("InstallBranding: Handling done."));
    return ERROR_SUCCESS; /* Do not fail here. */
}

#ifdef VBOX_WITH_NETFLT

/** @todo should use some real VBox app name */
#define VBOX_NETCFG_APP_NAME L"VirtualBox Installer"
#define VBOX_NETCFG_MAX_RETRIES 10
#define NETFLT_PT_INF_REL_PATH L"drivers\\network\\netflt\\VBoxNetFlt.inf"
#define NETFLT_MP_INF_REL_PATH L"drivers\\network\\netflt\\VBoxNetFltM.inf"
#define NETFLT_ID  L"sun_VBoxNetFlt" /** @todo Needs to be changed (?). */
#define NETADP_ID  L"sun_VBoxNetAdp" /** @todo Needs to be changed (?). */

static MSIHANDLE g_hCurrentModule = NULL;

static VOID netCfgLoggerCallback(LPCSTR szString)
{
    if (g_hCurrentModule)
        LogString(g_hCurrentModule, szString);
}

static VOID netCfgLoggerDisable()
{
    if (g_hCurrentModule)
    {
        VBoxNetCfgWinSetLogging((LOG_ROUTINE)NULL);
        g_hCurrentModule = NULL;
    }
}

static VOID netCfgLoggerEnable(MSIHANDLE hModule)
{
    Assert(hModule);

    if (g_hCurrentModule)
        netCfgLoggerDisable();

    g_hCurrentModule = hModule;

    VBoxNetCfgWinSetLogging((LOG_ROUTINE)netCfgLoggerCallback);
}

static UINT ErrorConvertFromHResult(MSIHANDLE hModule, HRESULT hr)
{
    UINT uRet;
    switch (hr)
    {
        case S_OK:
            uRet = ERROR_SUCCESS;
            break;

        case NETCFG_S_REBOOT:
        {
            LogStringW(hModule, TEXT("Reboot required, setting REBOOT property to Force"));
            HRESULT hr2 = MsiSetProperty(hModule, TEXT("REBOOT"), TEXT("Force"));
            if (hr2 != ERROR_SUCCESS)
                LogStringW(hModule, TEXT("Failed to set REBOOT property, error = 0x%x"), hr2);
            uRet = ERROR_SUCCESS; /* Never fail here. */
            break;
        }

        default:
            LogStringW(hModule, TEXT("Converting unhandled HRESULT (0x%x) to ERROR_GEN_FAILURE"), hr);
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
            LogStringW(hModule, TEXT("createNetCfgLockedMsgRecord: MsiRecordSetInteger failed, error = 0x%x"), uErr);
            MsiCloseHandle(hRecord);
            hRecord = NULL;
        }
    }
    else
        LogStringW(hModule, TEXT("createNetCfgLockedMsgRecord: Failed to create a record"));

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
                LogStringW(hModule, TEXT("doNetCfgInit: VBoxNetCfgWinQueryINetCfg failed, error = 0x%x"), hr);
            uErr = ErrorConvertFromHResult(hModule, hr);
            break;
        }

        /* hr == NETCFG_E_NO_WRITE_LOCK */

        if (!lpszLockedBy)
        {
            LogStringW(hModule, TEXT("doNetCfgInit: lpszLockedBy == NULL, breaking"));
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
            LogStringW(hModule, TEXT("doNetCfgInit: lpszLockedBy is 6to4svc.dll, retrying %d out of %d"), cRetries, VBOX_NETCFG_MAX_RETRIES);
            MsgResult = IDRETRY;
        }
        else
        {
            if (!hMsg)
            {
                hMsg = createNetCfgLockedMsgRecord(hModule);
                if (!hMsg)
                {
                    LogStringW(hModule, TEXT("doNetCfgInit: Failed to create a message record, breaking"));
                    CoTaskMemFree(lpszLockedBy);
                    break;
                }
            }

            UINT rTmp = MsiRecordSetStringW(hMsg, 2, lpszLockedBy);
            Assert(rTmp == ERROR_SUCCESS);
            if (rTmp != ERROR_SUCCESS)
            {
                LogStringW(hModule, TEXT("doNetCfgInit: MsiRecordSetStringW failed, error = 0x%x"), rTmp);
                CoTaskMemFree(lpszLockedBy);
                break;
            }

            MsgResult = MsiProcessMessage(hModule, (INSTALLMESSAGE)(INSTALLMESSAGE_USER | MB_RETRYCANCEL), hMsg);
            Assert(MsgResult == IDRETRY || MsgResult == IDCANCEL);
            LogStringW(hModule, TEXT("doNetCfgInit: MsiProcessMessage returned (0x%x)"), MsgResult);
        }
        CoTaskMemFree(lpszLockedBy);
    } while(MsgResult == IDRETRY);

    if (hMsg)
        MsiCloseHandle(hMsg);

    return uErr;
}

static UINT vboxNetFltQueryInfArray(MSIHANDLE hModule, OUT LPWSTR *apInfFullPaths, PUINT pcInfs, DWORD dwSize)
{
    UINT uErr;
    if (*pcInfs >= 2)
    {
        *pcInfs = 2;

        DWORD dwBuf = dwSize;
        uErr = MsiGetPropertyW(hModule, L"CustomActionData", apInfFullPaths[0], &dwBuf);
        if (   uErr == ERROR_SUCCESS
            && dwBuf)
        {
            /** @todo r=andy Avoid wcscpy and wcsncat, can cause buffer overruns! */
            wcscpy(apInfFullPaths[1], apInfFullPaths[0]);

            wcsncat(apInfFullPaths[0], NETFLT_PT_INF_REL_PATH, sizeof(NETFLT_PT_INF_REL_PATH));
            LogStringW(hModule, TEXT("vboxNetFltQueryInfArray: INF 1: %s"), apInfFullPaths[0]);

            wcsncat(apInfFullPaths[1], NETFLT_MP_INF_REL_PATH, sizeof(NETFLT_MP_INF_REL_PATH));
            LogStringW(hModule, TEXT("vboxNetFltQueryInfArray: INF 2: %s"), apInfFullPaths[1]);
        }
        else
        {
            if (uErr != ERROR_SUCCESS)
                LogStringW(hModule, TEXT("vboxNetFltQueryInfArray: MsiGetPropertyW failed, error = 0x%x"), uErr);
            else
                LogStringW(hModule, TEXT("vboxNetFltQueryInfArray: Empty installation directory"));
        }
    }
    else
    {
        uErr = ERROR_BUFFER_OVERFLOW;
        LogStringW(hModule, TEXT("vboxNetFltQueryInfArray: Buffer array size is < 2 (%u)"), *pcInfs);
    }

    return uErr;
}

#endif /*VBOX_WITH_NETFLT*/

UINT __stdcall UninstallNetFlt(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    INetCfg *pNetCfg;
    UINT uErr;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {
        LogStringW(hModule, TEXT("Uninstalling NetFlt"));

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetFltUninstall(pNetCfg);
            if (hr != S_OK)
                LogStringW(hModule, TEXT("UninstallNetFlt: VBoxNetCfgWinUninstallComponent failed, error = 0x%x"), hr);

            uErr = ErrorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            LogStringW(hModule, TEXT("Uninstalling NetFlt done, error = 0x%x"), uErr);

            /* Never fail on uninstall. */
            uErr = ERROR_SUCCESS;
        }
        else
            LogStringW(hModule, TEXT("UninstallNetFlt: doNetCfgInit failed, error = 0x%x"), uErr);
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
#ifdef VBOX_WITH_NETFLT
    UINT uErr;
    INetCfg *pNetCfg;

    netCfgLoggerEnable(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {

        LogStringW(hModule, TEXT("Installing NetFlt"));

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            WCHAR PtInf[MAX_PATH];
            WCHAR MpInf[MAX_PATH];
            DWORD sz = sizeof(PtInf);
            LPWSTR aInfs[] = {PtInf, MpInf};
            UINT cInfs = 2;
            uErr = vboxNetFltQueryInfArray(hModule, aInfs, &cInfs, sz);
            if (uErr == ERROR_SUCCESS)
            {
                HRESULT hr = VBoxNetCfgWinNetFltInstall(pNetCfg, (LPCWSTR*)aInfs, cInfs);
                if (FAILED(hr))
                    LogStringW(hModule, TEXT("InstallNetFlt: VBoxNetCfgWinNetFltInstall failed, error = 0x%x"), hr);

                uErr = ErrorConvertFromHResult(hModule, hr);
            }
            else
                LogStringW(hModule, TEXT("InstallNetFlt: vboxNetFltQueryInfArray failed, error = 0x%x"), uErr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            LogStringW(hModule, TEXT("Installing NetFlt done"));
        }
        else
            LogStringW(hModule, TEXT("InstallNetFlt: doNetCfgInit failed, error = 0x%x"), uErr);
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
        Assert(hKey != INVALID_HANDLE_VALUE);
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
            Assert(winEr == ERROR_SUCCESS);
            if (winEr == ERROR_SUCCESS)
            {
                WCHAR ConnectoinName[128];
                ULONG cbName = sizeof(ConnectoinName);

                HRESULT hr = VBoxNetCfgWinGenHostonlyConnectionName (DevName, ConnectoinName, &cbName);
                Assert(hr == S_OK);
                if (SUCCEEDED(hr))
                {
                    hr = VBoxNetCfgWinRenameConnection (guid, ConnectoinName);
                    Assert(hr == S_OK);
                }
            }
        }
        RegCloseKey(hKey);
    }
    else
    {
        Assert(0);
    }

    return TRUE;
}
#endif

UINT __stdcall CreateHostOnlyInterface(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    netCfgLoggerEnable(hModule);

    BOOL bSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);
    bool bSetStaticIp = true;

    LogStringW(hModule, TEXT("Creating host-only interface"));

    HRESULT hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(NETADP_ID);
    if (FAILED(hr))
    {
        LogStringW(hModule, TEXT("CreateHostOnlyInterface: VBoxNetCfgWinRemoveAllNetDevicesOfId failed, error = 0x%x"), hr);
        bSetStaticIp = false;
    }

    GUID guid;
    WCHAR MpInf[MAX_PATH];
    DWORD cSize = sizeof(MpInf)/sizeof(MpInf[0]);
    LPCWSTR pInfPath = NULL;
    bool bIsFile = false;
    UINT uErr = MsiGetPropertyW(hModule, L"CustomActionData", MpInf, &cSize);
    if (uErr == ERROR_SUCCESS)
    {
        if (cSize)
        {
            LogStringW(hModule, TEXT("CreateHostOnlyInterface: NetAdpDir property = %s"), MpInf);
            if (MpInf[cSize-1] != L'\\')
            {
                MpInf[cSize] = L'\\';
                ++cSize;
                MpInf[cSize] = L'\0';
            }

            /** @todo r=andy Avoid wcscat, can cause buffer overruns! */
            wcscat(MpInf, L"drivers\\network\\netadp\\VBoxNetAdp.inf");
            pInfPath = MpInf;
            bIsFile = true;

            LogStringW(hModule, TEXT("CreateHostOnlyInterface: Resulting INF path = %s"), pInfPath);
        }
        else
            LogStringW(hModule, TEXT("CreateHostOnlyInterface: NetAdpDir property value is empty"));
    }
    else
        LogStringW(hModule, TEXT("CreateHostOnlyInterface: Failed to get NetAdpDir property, error = 0x%x"), uErr);

    /* Make sure the inf file is installed. */
    if (!!pInfPath && bIsFile)
    {
        hr = VBoxDrvCfgInfInstall(pInfPath);
        if (FAILED(hr))
            LogStringW(hModule, TEXT("CreateHostOnlyInterface: Failed to install INF file, error = 0x%x"), hr);
    }

    if (SUCCEEDED(hr))
    {
        hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface(pInfPath, bIsFile, &guid, NULL, NULL);
        if (SUCCEEDED(hr))
        {
            ULONG ip = inet_addr("192.168.56.1");
            ULONG mask = inet_addr("255.255.255.0");
            hr = VBoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
            if (FAILED(hr))
                LogStringW(hModule, TEXT("CreateHostOnlyInterface: VBoxNetCfgWinEnableStaticIpConfig failed, error = 0x%x"), hr);
        }
        else
            LogStringW(hModule, TEXT("CreateHostOnlyInterface: VBoxNetCfgWinCreateHostOnlyNetworkInterface failed, error = 0x%x"), hr);
    }

    if (SUCCEEDED(hr))
        LogStringW(hModule, TEXT("Creating host-only interface done"));

    /* Restore original setup mode. */
    if (bSetupModeInteractive)
        SetupSetNonInteractiveMode(bSetupModeInteractive);

    netCfgLoggerDisable();

#endif /* VBOX_WITH_NETFLT */

    /* Never fail the install even if we did not succeed. */
    return ERROR_SUCCESS;
}

UINT __stdcall RemoveHostOnlyInterfaces(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    netCfgLoggerEnable(hModule);

    LogStringW(hModule, TEXT("RemoveHostOnlyInterfaces: Removing All Host-Only Interface"));

    BOOL bSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    HRESULT hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(NETADP_ID);
    if (SUCCEEDED(hr))
    {
        hr = VBoxDrvCfgInfUninstallAllSetupDi(&GUID_DEVCLASS_NET, NETADP_ID, L"Net", 0/* could be SUOI_FORCEDELETE */);
        if (FAILED(hr))
        {
            LogStringW(hModule, TEXT("RemoveHostOnlyInterfaces: NetAdp uninstalled successfully, but failed to remove infs\n"));
        }
    }
    else
        LogStringW(hModule, TEXT("RemoveHostOnlyInterfaces: NetAdp uninstall failed, hr = 0x%x\n"), hr);

    /* Restore original setup mode. */
    if (bSetupModeInteractive)
        SetupSetNonInteractiveMode(bSetupModeInteractive);

    netCfgLoggerDisable();
#endif /* VBOX_WITH_NETFLT */

    /* Never fail the install even if we did not succeed. */
    return ERROR_SUCCESS;
}

static bool IsTAPDevice(const TCHAR *pcGUID)
{
    HKEY hNetcard;
    bool bIsTapDevice = false;
    LONG lStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"),
                                0, KEY_READ, &hNetcard);
    if (lStatus != ERROR_SUCCESS)
        return false;

    int i = 0;
    while (true)
    {
        TCHAR szEnumName[256];
        TCHAR szNetCfgInstanceId[256];
        DWORD dwKeyType;
        HKEY  hNetCardGUID;

        DWORD dwLen = sizeof(szEnumName);
        lStatus = RegEnumKeyEx(hNetcard, i, szEnumName, &dwLen, NULL, NULL, NULL, NULL);
        if (lStatus != ERROR_SUCCESS)
            break;

        lStatus = RegOpenKeyEx(hNetcard, szEnumName, 0, KEY_READ, &hNetCardGUID);
        if (lStatus == ERROR_SUCCESS)
        {
            dwLen = sizeof(szNetCfgInstanceId);
            lStatus = RegQueryValueEx(hNetCardGUID, TEXT("NetCfgInstanceId"), NULL, &dwKeyType, (LPBYTE)szNetCfgInstanceId, &dwLen);
            if (   lStatus == ERROR_SUCCESS
                && dwKeyType == REG_SZ)
            {
                TCHAR szNetProductName[256];
                TCHAR szNetProviderName[256];

                szNetProductName[0] = 0;
                dwLen = sizeof(szNetProductName);
                lStatus = RegQueryValueEx(hNetCardGUID, TEXT("ProductName"), NULL, &dwKeyType, (LPBYTE)szNetProductName, &dwLen);

                szNetProviderName[0] = 0;
                dwLen = sizeof(szNetProviderName);
                lStatus = RegQueryValueEx(hNetCardGUID, TEXT("ProviderName"), NULL, &dwKeyType, (LPBYTE)szNetProviderName, &dwLen);

                if (   !wcscmp(szNetCfgInstanceId, pcGUID)
                    && !wcscmp(szNetProductName, TEXT("VirtualBox TAP Adapter"))
                    && (   (!wcscmp(szNetProviderName, TEXT("innotek GmbH"))) /* Legacy stuff. */
                        || (!wcscmp(szNetProviderName, TEXT("Sun Microsystems, Inc."))) /* Legacy stuff. */
                        || (!wcscmp(szNetProviderName, TEXT(VBOX_VENDOR))) /* Reflects current vendor string. */
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

    RegCloseKey (hNetcard);
    return bIsTapDevice;
}

#define VBOX_TAP_HWID _T("vboxtap")

#define SetErrBreak(strAndArgs) \
    if (1) { \
        rc = 0; \
        LogStringW(hModule, strAndArgs); \
        break; \
    } else do {} while (0)

int removeNetworkInterface(MSIHANDLE hModule, const TCHAR* pcGUID)
{
    int rc = 1;
    do
    {
        TCHAR lszPnPInstanceId [512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do
        {
            TCHAR strRegLocation [256];
            swprintf (strRegLocation,
                      TEXT("SYSTEM\\CurrentControlSet\\Control\\Network\\")
                      TEXT("{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s"),
                      pcGUID);
            LONG lStatus;
            lStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strRegLocation, 0,
                                   KEY_READ, &hkeyNetwork);
            if ((lStatus != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak((TEXT("VBox HostInterfaces: Host interface network was not found in registry (%s)! [1]"), strRegLocation));

            lStatus = RegOpenKeyExA(hkeyNetwork, "Connection", 0,
                                    KEY_READ, &hkeyConnection);
            if ((lStatus != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak((TEXT("VBox HostInterfaces: Host interface network was not found in registry (%s)! [2]"), strRegLocation));

            DWORD len = sizeof (lszPnPInstanceId);
            DWORD dwKeyType;
            lStatus = RegQueryValueExW(hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE) lszPnPInstanceId, &len);
            if ((lStatus != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak((TEXT("VBox HostInterfaces: Host interface network was not found in registry (%s)! [3]"), strRegLocation));
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

        do
        {
            DWORD ret = 0;
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
                LogStringW(hModule, TEXT("VBox HostInterfaces: SetupDiGetClassDevs failed (0x%08X)!"), GetLastError());
                SetErrBreak(TEXT("VBox HostInterfaces: Uninstallation failed!"));
            }

            BOOL fFoundDevice = FALSE;

            /* enumerate the driver info list */
            while (TRUE)
            {
                TCHAR *pszDeviceHwid;

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

                    pszDeviceHwid = (TCHAR *)malloc(size);
                    if (pszDeviceHwid)
                    {
                        fResult = SetupDiGetDeviceRegistryProperty(hDeviceInfo,
                                                                   &DeviceInfoData,
                                                                   SPDRP_HARDWAREID,
                                                                   NULL,
                                                                   (PBYTE)pszDeviceHwid,
                                                                   size,
                                                                   NULL);
                        if (!fResult)
                        {
                            free(pszDeviceHwid);
                            pszDeviceHwid = NULL;
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

                for (TCHAR *t = pszDeviceHwid;
                     t && *t && t < &pszDeviceHwid[size / sizeof(TCHAR)];
                     t += _tcslen (t) + 1)
                {
                    if (!_tcsicmp(VBOX_TAP_HWID, t))
                    {
                          /* get the device instance ID */
                          TCHAR devID [MAX_DEVICE_ID_LEN];
                          if (CM_Get_Device_ID(DeviceInfoData.DevInst,
                                               devID, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
                          {
                              /* compare to what we determined before */
                              if (!wcscmp(devID, lszPnPInstanceId))
                              {
                                  fFoundDevice = TRUE;
                                  break;
                              }
                          }
                    }
                }

                if (pszDeviceHwid)
                {
                    free(pszDeviceHwid);
                    pszDeviceHwid = NULL;
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
                    LogStringW(hModule, TEXT("VBox HostInterfaces: SetupDiSetSelectedDevice failed (0x%08X)!"), GetLastError());
                    SetErrBreak(TEXT("VBox HostInterfaces: Uninstallation failed!"));
                }

                fResult = SetupDiCallClassInstaller(DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
                if (!fResult)
                {
                    LogStringW(hModule, TEXT("VBox HostInterfaces: SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)!"), GetLastError());
                    SetErrBreak(TEXT("VBox HostInterfaces: Uninstallation failed!"));
                }
            }
            else
                SetErrBreak(TEXT("VBox HostInterfaces: Host interface network device not found!"));
        }
        while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList(hDeviceInfo);
    }
    while (0);
    return rc;
}

UINT __stdcall UninstallTAPInstances(MSIHANDLE hModule)
{
    static const TCHAR *NetworkKey = TEXT("SYSTEM\\CurrentControlSet\\Control\\Network\\")
                                     TEXT("{4D36E972-E325-11CE-BFC1-08002BE10318}");
    HKEY hCtrlNet;

    LONG lStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NetworkKey, 0, KEY_READ, &hCtrlNet);
    if (lStatus == ERROR_SUCCESS)
    {
        LogStringW(hModule, TEXT("VBox HostInterfaces: Enumerating interfaces ..."));
        for (int i = 0;; ++ i)
        {
            TCHAR szNetworkGUID [256] = { 0 };
            TCHAR szNetworkConnection [256] = { 0 };

            DWORD dwLen = (DWORD)sizeof(szNetworkGUID);
            lStatus = RegEnumKeyEx(hCtrlNet, i, szNetworkGUID, &dwLen, NULL, NULL, NULL, NULL);
            if (lStatus != ERROR_SUCCESS)
            {
                switch (lStatus)
                {
                case ERROR_NO_MORE_ITEMS:
                    LogStringW(hModule, TEXT("VBox HostInterfaces: No interfaces found."));
                    break;
                default:
                    LogStringW(hModule, TEXT("VBox HostInterfaces: Enumeration failed: %ld"), lStatus);
                    break;
                };
                break;
            }

            if (IsTAPDevice(szNetworkGUID))
            {
                LogStringW(hModule, TEXT("VBox HostInterfaces: Removing interface \"%s\" ..."), szNetworkGUID);
                removeNetworkInterface (hModule, szNetworkGUID);
                lStatus = RegDeleteKey (hCtrlNet, szNetworkGUID);
            }
        }
        RegCloseKey (hCtrlNet);
        LogStringW(hModule, TEXT("VBox HostInterfaces: Removing interfaces done."));
    }
    return ERROR_SUCCESS;
}

