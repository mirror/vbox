/* $Id$ */
/** @file
* VBoxInstallHelper - Various helper routines for Windows host installer.
*/

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
# include "VBox/WinNetConfig.h"
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

void LogString(MSIHANDLE hInstall, TCHAR* szString, ...)
{
    PMSIHANDLE newHandle = ::MsiCreateRecord(2);

    TCHAR szBuffer[1024] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsntprintf(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szString, pArgList);
    va_end(pArgList);

    MsiRecordSetString(newHandle, 0, szBuffer);
    MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_INFO), newHandle);
    MsiCloseHandle(newHandle);

#ifdef DEBUG
    _tprintf(_T("Debug: %s\n"), szBuffer);
#endif
}

static void LogStringW(MSIHANDLE hInstall, LPCWSTR szString, ...)
{
    PMSIHANDLE newHandle = ::MsiCreateRecord(2);

    TCHAR szBuffer[1024] = {0};
    va_list pArgList;
    va_start(pArgList, szString);
    _vsnwprintf(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szString, pArgList);
    va_end(pArgList);

    MsiRecordSetStringW(newHandle, 0, szBuffer);
    MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_INFO), newHandle);
    MsiCloseHandle(newHandle);
}

UINT __stdcall IsSerialCheckNeeded(MSIHANDLE a_hModule)
{
#ifndef VBOX_OSE
    /*BOOL bRet =*/ serialCheckNeeded(a_hModule);
#endif
    return ERROR_SUCCESS;
}

UINT __stdcall CheckSerial(MSIHANDLE a_hModule)
{
#ifndef VBOX_OSE
    /*BOOL bRet =*/ serialIsValid(a_hModule);
#endif
    return ERROR_SUCCESS;
}

DWORD Exec(MSIHANDLE hModule, TCHAR* szAppName, TCHAR* szCmdLine, TCHAR* szWorkDir, DWORD* dwExitCode)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD rc = ERROR_SUCCESS;

    ::ZeroMemory(&si, sizeof(si));
    si.dwFlags = STARTF_USESHOWWINDOW;
#ifdef UNICODE
    si.dwFlags |= CREATE_UNICODE_ENVIRONMENT;
#endif
    si.wShowWindow = SW_HIDE; /* For debugging: SW_SHOW; */
    si.cb = sizeof(si);
    ::ZeroMemory(&pi, sizeof(pi));

    LogString(hModule, TEXT("Executing command line: %s %s (Working Dir: %s)"), szAppName, szCmdLine, szWorkDir == NULL ? L"Current" : szWorkDir);

    ::SetLastError(0);
    if (!::CreateProcess(szAppName,    /* Module name. */
                         szCmdLine,    /* Command line. */
                         NULL,         /* Process handle not inheritable. */
                         NULL,         /* Thread handle not inheritable. */
                         FALSE,        /* Set handle inheritance to FALSE .*/
                         0,            /* No creation flags. */
                         NULL,         /* Use parent's environment block. */
                         szWorkDir,    /* Use parent's starting directory. */
                         &si,          /* Pointer to STARTUPINFO structure. */
                         &pi))         /* Pointer to PROCESS_INFORMATION structure. */
    {
        rc = ::GetLastError();
        LogString(hModule, TEXT("Executing command line: CreateProcess() failed! Error: %ld"), rc);
        return rc;
    }

    /* Wait until child process exits. */
    if (WAIT_FAILED == ::WaitForSingleObject(pi.hProcess, 30 * 1000 /* Wait 30 secs max. */))
    {
        rc = ::GetLastError();
        LogString(hModule, TEXT("Executing command line: WaitForSingleObject() failed! Error: %ld"), rc);
    }
    else
    {
        if (0 == ::GetExitCodeProcess(pi.hProcess, dwExitCode))
        {
            rc = ::GetLastError();
            LogString(hModule, TEXT("Executing command line: GetExitCodeProcess() failed! Error: %ld"), rc);
        }
    }

    /* Close process and thread handles. */
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);

    LogString(hModule, TEXT("Executing command returned: %ld (exit code %ld)"), rc, *dwExitCode);
    return rc;
}

UINT __stdcall InstallPythonAPI(MSIHANDLE hModule)
{
    LogString(hModule, TEXT("InstallPythonAPI: Checking for installed Python environment ..."));

    HKEY hkPythonCore = NULL;
    BOOL bInstalled = FALSE;
    LONG rc = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Python\\PythonCore", 0, KEY_READ, &hkPythonCore);
    if (rc != ERROR_SUCCESS)
    {
        LogString(hModule, TEXT("InstallPythonAPI: No environment seems to be installed."));
        return ERROR_SUCCESS;
    }

    TCHAR szPath[MAX_PATH] = { 0 };
    TCHAR szVal[MAX_PATH] = { 0 };

    for (int i = 0;; ++i)
    {
        TCHAR szRoot[MAX_PATH] = { 0 };
        DWORD dwLen = sizeof (szPath);
        DWORD dwKeyType = REG_SZ;

        rc = ::RegEnumKeyEx(hkPythonCore, i, szRoot, &dwLen, NULL, NULL, NULL, NULL);
        if (rc != ERROR_SUCCESS || dwLen <= 0)
            break;

        _stprintf_s(szPath, sizeof(szPath), L"%s\\InstallPath", szRoot);
        dwLen = sizeof(szVal);

        HKEY hkPythonInstPath = NULL;
        rc = ::RegOpenKeyEx(hkPythonCore, szPath, 0, KEY_READ,  &hkPythonInstPath);
        if (rc != ERROR_SUCCESS)
            continue;

        rc = ::RegQueryValueEx(hkPythonInstPath, L"", NULL, &dwKeyType, (LPBYTE)szVal, &dwLen);
        if(rc == ERROR_SUCCESS)
            LogString(hModule, TEXT("InstallPythonAPI: Path \"%s\" detected."), szVal);
    }

    ::RegCloseKey (hkPythonCore);

    /* Python path found? */
    TCHAR szExec[MAX_PATH] = { 0 };
    TCHAR szCmdLine[MAX_PATH] = { 0 };
    DWORD dwExitCode = 0;
    if (::_tcslen(szVal) > 0)
    {
        /* Cool, check for installed Win32 extensions. */
        LogString(hModule, TEXT("InstallPythonAPI: Python installed. Checking for Win32 extensions ..."));
        _stprintf_s(szExec, sizeof(szExec), L"%s\\python.exe", szVal);
        _stprintf_s(szCmdLine, sizeof(szCmdLine), L"%s\\python.exe -c \"import win32api\"", szVal);

        if (   (0 == Exec(hModule, szExec, szCmdLine, NULL, &dwExitCode))
            && (0 == dwExitCode))
        {
            /* Did we get the correct error level (=0)? */
            LogString(hModule, TEXT("InstallPythonAPI: Win32 extensions installed."));
            bInstalled = TRUE;
        }
        else LogString(hModule, TEXT("InstallPythonAPI: Win32 extensions not found."));
    }

    if (bInstalled) /* Is Python and all required stuff installed? */
    {
        /* Get the VBoxAPI setup string. */
        TCHAR szVBoxAPISetupPath[MAX_PATH] = {0};
        VBoxGetProperty(hModule, L"INSTALLDIR", szVBoxAPISetupPath, sizeof(szVBoxAPISetupPath));

        /* Set final path. */
        _stprintf_s(szPath, sizeof(szPath), L"%s\\sdk\\install", szVBoxAPISetupPath);

        /* Install our API module. */
        _stprintf_s(szCmdLine, sizeof(szCmdLine), L"%s\\python.exe vboxapisetup.py install", szVal);

        /* Set required environment variables. */
        if (!SetEnvironmentVariable(L"VBOX_INSTALL_PATH", szVBoxAPISetupPath))
        {
            LogString(hModule, TEXT("InstallPythonAPI: Cannot set environment variable VBOX_INSTALL_PATH!"));
            return FALSE;
        }
        else
        {
            if (   (0 == Exec(hModule, szExec, szCmdLine, szPath, &dwExitCode))
                && (0 == dwExitCode))
            {
                /* All done! */
                LogString(hModule, TEXT("InstallPythonAPI: VBoxAPI for Python successfully installed."));
                return ERROR_SUCCESS;
            }
            else LogString(hModule, TEXT("InstallPythonAPI: Error while installing VBoxAPI: %ld"), dwExitCode);
        }
    }

    LogString(hModule, TEXT("InstallPythonAPI: VBoxAPI not installed."));
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
            _stprintf_s(szKey, sizeof(szKey), L"SOFTWARE\\%s\\VirtualBox\\Branding\\", VBOX_VENDOR_SHORT, pszSection);
        else
            _stprintf_s(szKey, sizeof(szKey), L"SOFTWARE\\%s\\VirtualBox\\Branding", VBOX_VENDOR_SHORT);

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
                LogString(hModule, TEXT("InstallBranding: Could not write value %s! Error %ld"), pszValue, rc);
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

    _stprintf_s(szDest, sizeof(szDest), L"%s%c", pszDestDir, '\0');
    _stprintf_s(szSource, sizeof(szSource), L"%s%c", pszSourceDir, '\0');

    SHFILEOPSTRUCT s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_COPY;
    s.pTo = szDest;
    s.pFrom = szSource;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    LogString(hModule, TEXT("CopyDir: DestDir=%s, SourceDir=%s"),
              szDest, szSource);
    int r = SHFileOperation(&s);
    if (r != 0)
    {
        LogString(hModule, TEXT("CopyDir: Copy operation returned status 0x%x"), r);
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

    _stprintf_s(szDest, sizeof(szDest), L"%s%c", pszDestDir, '\0');

    SHFILEOPSTRUCT s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_DELETE;
    s.pFrom = szDest;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    LogString(hModule, TEXT("RemoveDir: DestDir=%s"), szDest);
    int r = SHFileOperation(&s);
    if (r != 0)
    {
        LogString(hModule, TEXT("RemoveDir: Remove operation returned status 0x%x"), r);
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

    _stprintf_s(szDest, sizeof(szDest), L"%s%c", pszDestDir, '\0');
    _stprintf_s(szSource, sizeof(szSource), L"%s%c", pszSourceDir, '\0');

    SHFILEOPSTRUCT s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_RENAME;
    s.pTo = szDest;
    s.pFrom = szSource;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    LogString(hModule, TEXT("RenameDir: DestDir=%s, SourceDir=%s"),
              szDest, szSource);
    int r = SHFileOperation(&s);
    if (r != 0)
    {
        LogString(hModule, TEXT("RenameDir: Rename operation returned status 0x%x"), r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT __stdcall UninstallBranding(MSIHANDLE hModule)
{
    UINT rc;
    LogString(hModule, TEXT("UninstallBranding: Handling branding file ..."));

    TCHAR szPathTargetDir[_MAX_PATH];
    TCHAR szPathDest[_MAX_PATH];

    rc = VBoxGetProperty(hModule, L"INSTALLDIR", szPathTargetDir, sizeof(szPathTargetDir));
    if (rc == ERROR_SUCCESS)
    {
        /** @todo Check trailing slash after %s. */
        _stprintf_s(szPathDest, sizeof(szPathDest), L"%scustom", szPathTargetDir);
        rc = RemoveDir(hModule, szPathDest);
        if (rc != ERROR_SUCCESS)
        {
            /* Check for hidden .custom directory and remove it. */
            _stprintf_s(szPathDest, sizeof(szPathDest), L"%s.custom", szPathTargetDir);
            rc = RemoveDir(hModule, szPathDest);
        }
    }

    LogString(hModule, TEXT("UninstallBranding: Handling done."));
    return ERROR_SUCCESS; /* Do not fail here. */
}

UINT __stdcall InstallBranding(MSIHANDLE hModule)
{
    UINT rc;
    LogString(hModule, TEXT("InstallBranding: Handling branding file ..."));

    TCHAR szPathMSI[_MAX_PATH];
    TCHAR szPathTargetDir[_MAX_PATH];

    TCHAR szPathSource[_MAX_PATH];
    TCHAR szPathDest[_MAX_PATH];

    rc = VBoxGetProperty(hModule, L"SOURCEDIR", szPathMSI, sizeof(szPathMSI));
    rc = VBoxGetProperty(hModule, L"INSTALLDIR", szPathTargetDir, sizeof(szPathTargetDir));
    if (rc == ERROR_SUCCESS)
    {
        /** @todo Check for trailing slash after %s. */
        _stprintf_s(szPathDest, sizeof(szPathDest), L"%s", szPathTargetDir);
        _stprintf_s(szPathSource, sizeof(szPathSource), L"%s.custom", szPathMSI);
        rc = CopyDir(hModule, szPathDest, szPathSource);
        if (rc == ERROR_SUCCESS)
        {
            _stprintf_s(szPathDest, sizeof(szPathDest), L"%scustom", szPathTargetDir);
            _stprintf_s(szPathSource, sizeof(szPathSource), L"%s.custom", szPathTargetDir);
            rc = RenameDir(hModule, szPathDest, szPathSource);
        }
    }

    LogString(hModule, TEXT("InstallBranding: Handling done."));
    return ERROR_SUCCESS; /* Do not fail here. */
}

#ifdef VBOX_WITH_NETFLT

/** @todo should use some real VBox app name */
#define VBOX_NETCFG_APP_NAME L"VirtualBox Installer"
#define VBOX_NETCFG_MAX_RETRIES 10
#define NETFLT_PT_INF_REL_PATH L"drivers\\network\\netflt\\VBoxNetFlt.inf"
#define NETFLT_MP_INF_REL_PATH L"drivers\\network\\netflt\\VBoxNetFlt_m.inf"
#define NETFLT_ID  L"sun_VBoxNetFlt" /** @todo Needs to be changed (?). */
#define NETADP_ID  L"sun_VBoxNetAdp" /** @todo Needs to be changed (?). */

static MSIHANDLE g_hCurrentModule = NULL;
static VOID winNetCfgLogger(LPCWSTR szString)
{
    Assert(g_hCurrentModule);
    if(g_hCurrentModule)
    {
        LogStringW(g_hCurrentModule, szString);
    }
}

static VOID inintWinNetCfgLogger(MSIHANDLE hModule)
{
    Assert(!g_hCurrentModule);
    Assert(hModule);

    g_hCurrentModule = hModule;

    VBoxNetCfgWinSetLogging((LOG_ROUTINE)winNetCfgLogger);
}

static VOID finiWinNetCfgLogger()
{
    Assert(g_hCurrentModule);

    VBoxNetCfgWinSetLogging((LOG_ROUTINE)NULL);

    g_hCurrentModule = NULL;
}

static UINT Hresult2Error(MSIHANDLE hModule, HRESULT hr)
{
    switch(hr)
    {
        case S_OK:
            return ERROR_SUCCESS;
        case NETCFG_S_REBOOT:
            LogString(hModule, TEXT("Reboot required, setting REBOOT property to Force"));
            if(MsiSetProperty(hModule, TEXT("REBOOT"), TEXT("Force")) != ERROR_SUCCESS)
            {
                LogString(hModule, TEXT("Failed to set REBOOT property"));
                return ERROR_GEN_FAILURE;
            }
            return ERROR_SUCCESS;
        default:
            LogString(hModule, TEXT("converting hresult (0x%x) to ERROR_GEN_FAILURE"), hr);
            return ERROR_GEN_FAILURE;
    }
}

static MSIHANDLE createNetCfgLockedMsgRecord(MSIHANDLE hModule)
{
    MSIHANDLE hRecord = MsiCreateRecord(2);
    Assert(hRecord);
    if(hRecord)
    {
        do
        {
            UINT r = MsiRecordSetInteger(hRecord, 1, 25001);
            Assert(r == ERROR_SUCCESS);
            if(r != ERROR_SUCCESS)
            {
                LogString(hModule, TEXT("createNetCfgLockedMsgRecord: MsiRecordSetInteger failed, r (0x%x)"), r);
                MsiCloseHandle(hRecord);
                hRecord = NULL;
                break;
            }
        }while(0);
    }
    else
    {
        LogString(hModule, TEXT("createNetCfgLockedMsgRecord: failed to create a record"));
    }

    return hRecord;
}

static UINT doNetCfgInit(MSIHANDLE hModule, INetCfg **ppnc, BOOL bWrite)
{
    MSIHANDLE hMsg = NULL;
    UINT r = ERROR_GEN_FAILURE;
    int MsgResult;
    int cRetries = 0;

    do
    {
        LPWSTR lpszLockedBy;
        HRESULT hr = VBoxNetCfgWinQueryINetCfg(bWrite, VBOX_NETCFG_APP_NAME, ppnc, &lpszLockedBy);
        if(hr != NETCFG_E_NO_WRITE_LOCK)
        {
            Assert(hr == S_OK);
            if(hr != S_OK)
            {
                LogString(hModule, TEXT("doNetCfgInit: VBoxNetCfgWinQueryINetCfg failed, hr (0x%x)"), hr);
            }
            r = Hresult2Error(hModule, hr);
            break;
        }

        /* hr == NETCFG_E_NO_WRITE_LOCK */

        Assert(lpszLockedBy);
        if(!lpszLockedBy)
        {
            LogString(hModule, TEXT("doNetCfgInit: lpszLockedBy == NULL, breaking"));
            break;
        }

        /* on vista the 6to4svc.dll periodically maintains the lock for some reason,
         * if this is the case, increase the wait period by retrying multiple times
         * NOTE: we could alternatively increase the wait timeout,
         * however it seems unneeded for most cases, e.g. in case some network connection property
         * dialog is opened, it would be better to post a notification to the user as soon as possible
         * rather than waiting for a longer period of time before displaying it */
        if(cRetries < VBOX_NETCFG_MAX_RETRIES
                && !wcscmp(lpszLockedBy, L"6to4svc.dll"))
        {
            cRetries++;
            LogString(hModule, TEXT("doNetCfgInit: lpszLockedBy is 6to4svc.dll, retrying %d out of %d"), cRetries, VBOX_NETCFG_MAX_RETRIES);
            MsgResult = IDRETRY;
        }
        else
        {
            if(!hMsg)
            {
                hMsg = createNetCfgLockedMsgRecord(hModule);
                if(!hMsg)
                {
                    LogString(hModule, TEXT("doNetCfgInit: failed to create a message record, breaking"));
                    CoTaskMemFree(lpszLockedBy);
                    break;
                }
            }

            UINT rTmp = MsiRecordSetStringW(hMsg, 2, lpszLockedBy);
            Assert(rTmp == ERROR_SUCCESS);
            if(rTmp != ERROR_SUCCESS)
            {
                LogString(hModule, TEXT("doNetCfgInit: MsiRecordSetStringW failed, r (0x%x)"), rTmp);
                CoTaskMemFree(lpszLockedBy);
                break;
            }

            MsgResult = MsiProcessMessage(hModule, (INSTALLMESSAGE)(INSTALLMESSAGE_USER | MB_RETRYCANCEL), hMsg);
            Assert(MsgResult == IDRETRY || MsgResult == IDCANCEL);
            LogString(hModule, TEXT("doNetCfgInit: MsiProcessMessage returned (0x%x)"), MsgResult);
        }
        CoTaskMemFree(lpszLockedBy);
    } while(MsgResult == IDRETRY);

    if(hMsg)
    {
        MsiCloseHandle(hMsg);
    }

    return r;
}

static UINT vboxNetFltQueryInfArray(MSIHANDLE hModule, OUT LPWSTR *apInfFullPaths, PUINT pcInfs, DWORD cSize)
{
    UINT r;
    Assert(*pcInfs >= 2);
    if(*pcInfs >= 2)
    {
        *pcInfs = 2;
        r = MsiGetPropertyW(hModule, L"CustomActionData", apInfFullPaths[0], &cSize);
        Assert(r == ERROR_SUCCESS);
        if(r == ERROR_SUCCESS)
        {
            wcscpy(apInfFullPaths[1], apInfFullPaths[0]);

            wcsncat(apInfFullPaths[0], NETFLT_PT_INF_REL_PATH, sizeof(NETFLT_PT_INF_REL_PATH));
            wcsncat(apInfFullPaths[1], NETFLT_MP_INF_REL_PATH, sizeof(NETFLT_MP_INF_REL_PATH));
        }
        else
        {
            LogString(hModule, TEXT("vboxNetFltQueryInfArray: MsiGetPropertyW failes, r (%d)"), r);
        }
    }
    else
    {
        r = ERROR_GEN_FAILURE;
        LogString(hModule, TEXT("vboxNetFltQueryInfArray: buffer array size is < 2 : (%d)"), *pcInfs);
        *pcInfs = 2;
    }

    return r;
}

#endif /*VBOX_WITH_NETFLT*/

UINT __stdcall UninstallNetFlt(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    INetCfg *pnc;
    UINT r;

    inintWinNetCfgLogger(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {
        LogString(hModule, TEXT("Uninstalling NetFlt"));

        r = doNetCfgInit(hModule, &pnc, TRUE);
        Assert(r == ERROR_SUCCESS);
        if(r == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetFltUninstall(pnc);
            Assert(hr == S_OK);
            if(hr != S_OK)
            {
                LogString(hModule, TEXT("UninstallNetFlt: VBoxNetCfgWinUninstallComponent failed, hr (0x%x)"), hr);
            }

            r = Hresult2Error(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pnc, TRUE);

            /* Never fail the uninstall */
            r = ERROR_SUCCESS;
        }
        else
        {
            LogString(hModule, TEXT("UninstallNetFlt: doNetCfgInit failed, r (0x%x)"), r);
        }

        LogString(hModule, TEXT("Uninstalling NetFlt done, r (0x%x)"), r);
    }
    __finally
    {
        if(bOldIntMode)
        {
            /* the prev mode != FALSE, i.e. non-interactive */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        finiWinNetCfgLogger();
    }

    return ERROR_SUCCESS;
#else /* not defined VBOX_WITH_NETFLT */
    return ERROR_SUCCESS;
#endif /* VBOX_WITH_NETFLT */
}

UINT __stdcall InstallNetFlt(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    UINT r;
    INetCfg *pnc;

    inintWinNetCfgLogger(hModule);

    BOOL bOldIntMode = SetupSetNonInteractiveMode(FALSE);

    __try
    {

        LogString(hModule, TEXT("Installing NetFlt"));

        r = doNetCfgInit(hModule, &pnc, TRUE);
        Assert(r == ERROR_SUCCESS);
        if(r == ERROR_SUCCESS)
        {
            WCHAR PtInf[MAX_PATH];
            WCHAR MpInf[MAX_PATH];
            DWORD sz = sizeof(PtInf);
            LPWSTR aInfs[] = {PtInf, MpInf};
            UINT cInfs = 2;

            r = vboxNetFltQueryInfArray(hModule, aInfs, &cInfs, sz);
            Assert(r == ERROR_SUCCESS);
            Assert(cInfs == 2);
            if(r == ERROR_SUCCESS)
            {
    //            HRESULT hr = VBoxNetCfgWinInstallSpecifiedComponent (
    //                                                        pnc,
    //                                                        (LPCWSTR*)aInfs,
    //                                                        cInfs,
    //                                                        NETFLT_ID,
    //                                                        &GUID_DEVCLASS_NETSERVICE,
    //                                                        true);
                HRESULT hr = VBoxNetCfgWinNetFltInstall(pnc, (LPCWSTR*)aInfs, cInfs);
                Assert(hr == S_OK);
                if(hr != S_OK)
                {
                    LogString(hModule, TEXT("InstallNetFlt: VBoxNetCfgWinInstallSpecifiedComponent failed, hr (0x%x)"), hr);
                }

                r = Hresult2Error(hModule, hr);
            }
            else
            {
                LogString(hModule, TEXT("InstallNetFlt: vboxNetFltQueryInfArray failed, r (0x%x)"), r);
            }

            VBoxNetCfgWinReleaseINetCfg(pnc, TRUE);

            /* Never fail the uninstall */
            r = ERROR_SUCCESS;
        }
        else
        {
            LogString(hModule, TEXT("InstallNetFlt: doNetCfgInit failed, r (0x%x)"), r);
        }

        LogString(hModule, TEXT("Installing NetFlt done, r (0x%x)"), r);
    }
    __finally
    {
        if(bOldIntMode)
        {
            /* the prev mode != FALSE, i.e. non-interactive */
            SetupSetNonInteractiveMode(bOldIntMode);
        }
        finiWinNetCfgLogger();
    }

    return ERROR_SUCCESS;
#else /* not defined VBOX_WITH_NETFLT */
    return ERROR_SUCCESS;
#endif /* VBOX_WITH_NETFLT */
}

#if 0
static BOOL RenameHostOnlyConnectionsCallback(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDev, PVOID pContext)
{
    WCHAR DevName[256];
    DWORD winEr;

    if(SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDev,
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
            if(winEr == ERROR_SUCCESS)
            {
                WCHAR ConnectoinName[128];
                ULONG cbName = sizeof(ConnectoinName);

                HRESULT hr = VBoxNetCfgWinGenHostonlyConnectionName (DevName, ConnectoinName, &cbName);
                Assert(hr == S_OK);
                if(hr == S_OK)
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
    inintWinNetCfgLogger(hModule);

    BOOL bPrevMode = SetupSetNonInteractiveMode(FALSE);
    bool bSetStaticIp = true;

    LogString(hModule, TEXT("Creating Host-Only Interface"));

    HRESULT hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(NETADP_ID);
    Assert(hr == S_OK);
    if(hr != S_OK)
    {
        LogString(hModule, TEXT("CreateHostOnlyInterface: VBoxNetCfgWinRemoveAllNetDevicesOfId failed, hr (0x%x)"), hr);
        bSetStaticIp = false;
    }

    GUID guid;
    WCHAR MpInf[MAX_PATH];
    DWORD cSize = sizeof(MpInf)/sizeof(MpInf[0]);
    LPCWSTR pInfPath = NULL;
    bool bIsFile = false;
    UINT r = MsiGetPropertyW(hModule, L"CustomActionData", MpInf, &cSize);
    Assert(r == ERROR_SUCCESS);
    if(r == ERROR_SUCCESS)
    {
        LogString(hModule, TEXT("NetAdpDir property: (%s)"), MpInf);
        if(cSize)
        {
            if(MpInf[cSize-1] != L'\\')
            {
                MpInf[cSize] = L'\\';
                ++cSize;
                MpInf[cSize] = L'\0';
            }
//          wcscat(MpInf, L"VBoxNetFlt.inf");
            wcscat(MpInf, L"drivers\\network\\netadp\\VBoxNetAdp.inf");
            pInfPath = MpInf;
            bIsFile = true;
            LogString(hModule, TEXT("Resulting inf path is: (%s)"), pInfPath);
        }
        else
        {
            LogString(hModule, TEXT("CreateHostOnlyInterface: NetAdpDir property value is empty"));
        }
    }
    else
    {
        LogString(hModule, TEXT("CreateHostOnlyInterface: failed to get NetAdpDir property, r(%d)"), r);
    }

    /* make sure the inf file is installed */
    if(!!pInfPath && bIsFile)
    {
        HRESULT tmpHr = VBoxNetCfgWinInstallInf(pInfPath);
        Assert(tmpHr == S_OK);
    }

    hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface (pInfPath, bIsFile, &guid, NULL, NULL);
    Assert(hr == S_OK);
    if(hr == S_OK)
    {
        ULONG ip = inet_addr("192.168.56.1");
        ULONG mask = inet_addr("255.255.255.0");

        hr = VBoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
        Assert(hr == S_OK);
        if(hr != S_OK)
        {
            LogString(hModule, TEXT("CreateHostOnlyInterface: VBoxNetCfgWinEnableStaticIpConfig failed, hr (0x%x)"), hr);
        }
    }
    else
    {
        LogString(hModule, TEXT("CreateHostOnlyInterface: VBoxNetCfgWinCreateHostOnlyNetworkInterface failed, hr (0x%x)"), hr);
    }

    if(bPrevMode)
    {
        /* the prev mode != FALSE, i.e. non-interactive */
        SetupSetNonInteractiveMode(bPrevMode);
    }

    finiWinNetCfgLogger();

    /* never fail the install even if we are not succeeded */
    return ERROR_SUCCESS;
#else /* not defined VBOX_WITH_NETFLT */
    return ERROR_SUCCESS;
#endif /* VBOX_WITH_NETFLT */
}

UINT __stdcall RemoveHostOnlyInterfaces(MSIHANDLE hModule)
{
#ifdef VBOX_WITH_NETFLT
    inintWinNetCfgLogger(hModule);

    LogString(hModule, TEXT("Removing All Host-Only Interface"));

    BOOL bPrevMode = SetupSetNonInteractiveMode(FALSE);

    HRESULT hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(NETADP_ID);
    if(hr == S_OK)
    {
        hr = VBoxNetCfgWinUninstallInfs(&GUID_DEVCLASS_NET, NETADP_ID, 0/* could be SUOI_FORCEDELETE */);
        if(hr != S_OK)
        {
            LogString(hModule, TEXT("NetAdp uninstalled successfully, but failed to remove infs\n"));
        }
    }
    else
    {
        LogString(hModule, TEXT("NetAdp uninstall failed, hr = 0x%x\n"), hr);
    }

    if(bPrevMode)
    {
        /* the prev mode != FALSE, i.e. non-interactive */
        SetupSetNonInteractiveMode(bPrevMode);
    }

    finiWinNetCfgLogger();

    return ERROR_SUCCESS;
#else /* not defined VBOX_WITH_NETFLT */
    return ERROR_SUCCESS;
#endif /* VBOX_WITH_NETFLT */
}

static bool IsTAPDevice (const TCHAR *a_pcGUID)
{
    HKEY hNetcard;
    LONG status;
    DWORD len;
    int i = 0;
    bool ret = false;

    status = RegOpenKeyEx (HKEY_LOCAL_MACHINE,
        TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"),
        0, KEY_READ, &hNetcard);

    if (status != ERROR_SUCCESS)
        return false;

    while(true)
    {
        TCHAR szEnumName[256];
        TCHAR szNetCfgInstanceId[256];
        DWORD dwKeyType;
        HKEY  hNetCardGUID;

        len = sizeof(szEnumName);
        status = RegEnumKeyEx (hNetcard, i, szEnumName, &len, NULL, NULL, NULL, NULL);
        if (status != ERROR_SUCCESS)
            break;

        status = RegOpenKeyEx (hNetcard, szEnumName, 0, KEY_READ, &hNetCardGUID);
        if (status == ERROR_SUCCESS)
        {
            len = sizeof (szNetCfgInstanceId);
            status = RegQueryValueEx (hNetCardGUID, TEXT("NetCfgInstanceId"), NULL, &dwKeyType, (LPBYTE)szNetCfgInstanceId, &len);
            if (status == ERROR_SUCCESS && dwKeyType == REG_SZ)
            {
                TCHAR szNetProductName[256];
                TCHAR szNetProviderName[256];

                szNetProductName[0] = 0;
                len = sizeof(szNetProductName);
                status = RegQueryValueEx (hNetCardGUID, TEXT("ProductName"), NULL, &dwKeyType, (LPBYTE)szNetProductName, &len);

                szNetProviderName[0] = 0;
                len = sizeof(szNetProviderName);
                status = RegQueryValueEx (hNetCardGUID, TEXT("ProviderName"), NULL, &dwKeyType, (LPBYTE)szNetProviderName, &len);

                if (   !wcscmp(szNetCfgInstanceId, a_pcGUID)
                    && !wcscmp(szNetProductName, TEXT("VirtualBox TAP Adapter"))
                    && (   (!wcscmp(szNetProviderName, TEXT("innotek GmbH"))) /* Legacy stuff. */
                        || (!wcscmp(szNetProviderName, TEXT("Sun Microsystems, Inc."))) /* Legacy stuff. */
                        || (!wcscmp(szNetProviderName, TEXT(VBOX_VENDOR))) /* Reflects current vendor string. */
                       )
                   )
                {
                    ret = true;
                    RegCloseKey(hNetCardGUID);
                    break;
                }
            }
            RegCloseKey(hNetCardGUID);
        }
        ++i;
    }

    RegCloseKey (hNetcard);
    return ret;
}

#define VBOX_TAP_HWID _T("vboxtap")

#define SetErrBreak(strAndArgs) \
    if (1) { \
        rc = 0; \
        LogString (a_hModule, strAndArgs); \
        break; \
    } else do {} while (0)

int removeNetworkInterface (MSIHANDLE a_hModule, const TCHAR* pcGUID)
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
            LONG status;
            status = RegOpenKeyEx (HKEY_LOCAL_MACHINE, strRegLocation, 0,
                                   KEY_READ, &hkeyNetwork);
            if ((status != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak ((TEXT("VBox HostInterfaces: Host interface network was not found in registry (%s)! [1]"), strRegLocation));

            status = RegOpenKeyExA (hkeyNetwork, "Connection", 0,
                                    KEY_READ, &hkeyConnection);
            if ((status != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak ((TEXT("VBox HostInterfaces: Host interface network was not found in registry (%s)! [2]"), strRegLocation));

            DWORD len = sizeof (lszPnPInstanceId);
            DWORD dwKeyType;
            status = RegQueryValueExW (hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE) lszPnPInstanceId, &len);
            if ((status != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak ((TEXT("VBox HostInterfaces: Host interface network was not found in registry (%s)! [3]"), strRegLocation));
        }
        while (0);

        if (hkeyConnection)
            RegCloseKey (hkeyConnection);
        if (hkeyNetwork)
            RegCloseKey (hkeyNetwork);

        /*
         * Now we are going to enumerate all network devices and
         * wait until we encounter the right device instance ID
         */

        HDEVINFO hDeviceInfo = INVALID_HANDLE_VALUE;

        do
        {
            BOOL ok;
            DWORD ret = 0;
            GUID netGuid;
            SP_DEVINFO_DATA DeviceInfoData;
            DWORD index = 0;
            BOOL found = FALSE;
            DWORD size = 0;

            /* initialize the structure size */
            DeviceInfoData.cbSize = sizeof (SP_DEVINFO_DATA);

            /* copy the net class GUID */
            memcpy (&netGuid, &GUID_DEVCLASS_NET, sizeof (GUID_DEVCLASS_NET));

            /* return a device info set contains all installed devices of the Net class */
            hDeviceInfo = SetupDiGetClassDevs (&netGuid, NULL, NULL, DIGCF_PRESENT);

            if (hDeviceInfo == INVALID_HANDLE_VALUE)
            {
                LogString (a_hModule, TEXT("VBox HostInterfaces: SetupDiGetClassDevs failed (0x%08X)!"), GetLastError());
                SetErrBreak (TEXT("VBox HostInterfaces: Uninstallation failed!"));
            }

            /* enumerate the driver info list */
            while (TRUE)
            {
                TCHAR *deviceHwid;

                ok = SetupDiEnumDeviceInfo (hDeviceInfo, index, &DeviceInfoData);

                if (!ok)
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
                ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                       &DeviceInfoData,
                                                       SPDRP_HARDWAREID,
                                                       NULL,
                                                       NULL,
                                                       0,
                                                       &size);
                if (!ok)
                {
                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                    {
                        index++;
                        continue;
                    }

                    deviceHwid = (TCHAR *) malloc (size);
                    ok = SetupDiGetDeviceRegistryProperty (hDeviceInfo,
                                                           &DeviceInfoData,
                                                           SPDRP_HARDWAREID,
                                                           NULL,
                                                           (PBYTE)deviceHwid,
                                                           size,
                                                           NULL);
                    if (!ok)
                    {
                        free (deviceHwid);
                        deviceHwid = NULL;
                        index++;
                        continue;
                    }
                }
                else
                {
                    /* something is wrong.  This shouldn't have worked with a NULL buffer */
                    index++;
                    continue;
                }

                for (TCHAR *t = deviceHwid;
                     t && *t && t < &deviceHwid[size / sizeof(TCHAR)];
                     t += _tcslen (t) + 1)
                {
                    if (!_tcsicmp (VBOX_TAP_HWID, t))
                    {
                          /* get the device instance ID */
                          TCHAR devID [MAX_DEVICE_ID_LEN];
                          if (CM_Get_Device_ID(DeviceInfoData.DevInst,
                                               devID, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
                          {
                              /* compare to what we determined before */
                              if (wcscmp(devID, lszPnPInstanceId) == 0)
                              {
                                  found = TRUE;
                                  break;
                              }
                          }
                    }
                }

                if (deviceHwid)
                {
                    free (deviceHwid);
                    deviceHwid = NULL;
                }

                if (found)
                    break;

                index++;
            }

            if (found == FALSE)
                SetErrBreak (TEXT("VBox HostInterfaces: Host Interface Network driver not found!"));

            ok = SetupDiSetSelectedDevice (hDeviceInfo, &DeviceInfoData);
            if (!ok)
            {
                LogString (a_hModule, TEXT("VBox HostInterfaces: SetupDiSetSelectedDevice failed (0x%08X)!"), GetLastError());
                SetErrBreak (TEXT("VBox HostInterfaces: Uninstallation failed!"));
            }

            ok = SetupDiCallClassInstaller (DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
            if (!ok)
            {
                LogString (a_hModule, TEXT("VBox HostInterfaces: SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)!"), GetLastError());
                SetErrBreak (TEXT("VBox HostInterfaces: Uninstallation failed!"));
            }
        }
        while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList (hDeviceInfo);
    }
    while (0);
    return rc;
}

UINT __stdcall UninstallTAPInstances (MSIHANDLE a_hModule)
{
    static const TCHAR *NetworkKey = TEXT("SYSTEM\\CurrentControlSet\\Control\\Network\\")
                                     TEXT("{4D36E972-E325-11CE-BFC1-08002BE10318}");
    HKEY hCtrlNet;
    LONG status = 0;
    DWORD len = 0;
    LONG cnt = 0;

    status = RegOpenKeyEx (HKEY_LOCAL_MACHINE, NetworkKey, 0, KEY_READ, &hCtrlNet);
    if (status == ERROR_SUCCESS)
    {
        LogString(a_hModule, TEXT("VBox HostInterfaces: Enumerating interfaces ..."));
        for (int i = 0;; ++ i)
        {
            TCHAR szNetworkGUID [256] = { 0 };
            TCHAR szNetworkConnection [256] = { 0 };

            len = sizeof (szNetworkGUID);
            status = RegEnumKeyEx (hCtrlNet, i, szNetworkGUID, &len, NULL, NULL, NULL, NULL);
            if (status != ERROR_SUCCESS)
            {
                switch (status)
                {
                case ERROR_NO_MORE_ITEMS:
                    LogString(a_hModule, TEXT("VBox HostInterfaces: No interfaces found."));
                    break;
                default:
                    LogString(a_hModule, TEXT("VBox HostInterfaces: Enumeration failed: %ld"), status);
                    break;
                };
                break;
            }

            if (IsTAPDevice(szNetworkGUID))
            {
                LogString(a_hModule, TEXT("VBox HostInterfaces: Removing interface \"%s\" ..."), szNetworkGUID);
                removeNetworkInterface (a_hModule, szNetworkGUID);
                status = RegDeleteKey (hCtrlNet, szNetworkGUID);
            }
        }
        RegCloseKey (hCtrlNet);
        LogString(a_hModule, TEXT("VBox HostInterfaces: Removing interfaces done."));
    }
    return ERROR_SUCCESS;
}
