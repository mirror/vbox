/* $Id$ */
/** @file
 * VBoxInstallHelper - Various helper routines for Windows host installer.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_NETFLT
# include "VBox/VBoxNetCfg-win.h"
# include "VBox/VBoxDrvCfg-win.h"
#endif /* VBOX_WITH_NETFLT */

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

#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/utf16.h>

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


BOOL WINAPI DllMain(HANDLE hInst, ULONG uReason, LPVOID pReserved)
{
    RT_NOREF(hInst, pReserved);

    switch (uReason)
    {
        case DLL_PROCESS_ATTACH:
            RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return TRUE;
}

static int logStringF(MSIHANDLE hInstall, const char *pcszFmt, ...)
{
    PMSIHANDLE hMSI = MsiCreateRecord(2 /* cParms */);
    if (!hMSI)
        return VERR_ACCESS_DENIED;

    RTUTF16 wszBuf[_1K] = { 0 };

    va_list va;
    va_start(va, pcszFmt);
    ssize_t cwch = RTUtf16PrintfV(wszBuf, RT_ELEMENTS(wszBuf), pcszFmt, va);
    va_end(va);

    if (cwch <= 0)
        return VERR_BUFFER_OVERFLOW;

    MsiRecordSetStringW(hMSI, 0, wszBuf);
    MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_INFO), hMSI);
    MsiCloseHandle(hMSI);

    return VINF_SUCCESS;
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
 * Tries to retrieve the Python installation path on the system.
 *
 * @returns VBox status code.
 * @param   hModule             Windows installer module handle.
 * @param   hKeyRoot            Registry root key to use, e.g. HKEY_LOCAL_MACHINE.
 * @param   ppszPath            Where to store the allocated Python path on success.
 *                              Must be free'd by the caller using RTStrFree().
 */
static int getPythonPath(MSIHANDLE hModule, HKEY hKeyRoot, char **ppszPath)
{
    HKEY hkPythonCore = NULL;
    LSTATUS dwErr = RegOpenKeyExW(hKeyRoot, L"SOFTWARE\\Python\\PythonCore", 0, KEY_READ, &hkPythonCore);
    if (dwErr != ERROR_SUCCESS)
        return RTErrConvertFromWin32(dwErr);

    char *pszPythonPath = NULL;

    RTUTF16 wszKey [RTPATH_MAX] = { 0 };
    RTUTF16 wszKey2[RTPATH_MAX] = { 0 };
    RTUTF16 wszVal [RTPATH_MAX] = { 0 };

    int rc = VINF_SUCCESS;

    /* Note: The loop ASSUMES that later found versions are higher, e.g. newer Python versions.
     *       For now we always go by the newest version. */
    for (int i = 0;; ++i)
    {
        DWORD dwKey     = sizeof(wszKey);
        DWORD dwKeyType = REG_SZ;

        dwErr = RegEnumKeyExW(hkPythonCore, i, wszKey, &dwKey, NULL, NULL, NULL, NULL);
        if (dwErr != ERROR_SUCCESS || dwKey <= 0)
            break;
        AssertBreakStmt(dwKey <= sizeof(wszKey), VERR_BUFFER_OVERFLOW);

        if (RTUtf16Printf(wszKey2, sizeof(wszKey2), "%ls\\InstallPath", wszKey) <= 0)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        dwKey = sizeof(wszKey2); /* Re-initialize length. */

        HKEY hkPythonInstPath = NULL;
        dwErr = RegOpenKeyExW(hkPythonCore, wszKey2, 0, KEY_READ,  &hkPythonInstPath);
        if (dwErr != ERROR_SUCCESS)
            continue;

        dwErr = RegQueryValueExW(hkPythonInstPath, L"", NULL, &dwKeyType, (LPBYTE)wszVal, &dwKey);
        if (dwErr == ERROR_SUCCESS)
            logStringF(hModule, "InstallPythonAPI: Path \"%ls\" found.", wszVal);

        if (pszPythonPath) /* Free former path, if any. */
        {
            RTStrFree(pszPythonPath);
            pszPythonPath = NULL;
        }

        rc = RTUtf16ToUtf8(wszVal, &pszPythonPath);
        AssertRCBreak(rc);

        if (!RTPathExists(pszPythonPath))
        {
            logStringF(hModule, "InstallPythonAPI: Warning: Path \"%s\" does not exist, skipping.", wszVal);
            rc = VERR_PATH_NOT_FOUND;
        }

        RegCloseKey(hkPythonInstPath);
    }

    RegCloseKey(hkPythonCore);

    if (RT_FAILURE(rc))
    {
        RTStrFree(pszPythonPath);
    }
    else
        *ppszPath = pszPythonPath;

    return rc;
}

/**
 * Waits for a started process to terminate.
 *
 * @returns VBox status code.
 * @param   Process             Handle of process to wait for.
 * @param   msTimeout           Timeout (in ms) to wait for process to terminate.
 * @param   pProcSts            Pointer to process status on return.
 */
int procWait(RTPROCESS Process, RTMSINTERVAL msTimeout, PRTPROCSTATUS pProcSts)
{
    uint64_t tsStartMs = RTTimeMilliTS();

    while (RTTimeMilliTS() - tsStartMs <= msTimeout)
    {
        int rc = RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, pProcSts);
        if (rc == VERR_PROCESS_RUNNING)
            continue;
        else if (RT_FAILURE(rc))
            return rc;

        if (   pProcSts->iStatus   != 0
            || pProcSts->enmReason != RTPROCEXITREASON_NORMAL)
        {
            rc = VERR_GENERAL_FAILURE; /** @todo Fudge! */
        }

        return VINF_SUCCESS;
    }

    return VERR_TIMEOUT;
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
    char *pszPythonPath;
    int rc = getPythonPath(hModule, HKEY_LOCAL_MACHINE, &pszPythonPath);
    if (RT_FAILURE(rc))
        rc = getPythonPath(hModule, HKEY_CURRENT_USER, &pszPythonPath);

    if (RT_SUCCESS(rc))
    {
        logStringF(hModule, "IsPythonInstalled: Python installation found at \"%s\"", pszPythonPath);

        PRTUTF16 pwszPythonPath;
        rc = RTStrToUtf16(pszPythonPath, &pwszPythonPath);
        if (RT_SUCCESS(rc))
        {
            VBoxSetMsiProp(hModule, L"VBOX_PYTHON_PATH", pwszPythonPath);

            RTUtf16Free(pwszPythonPath);
        }
        else
            logStringF(hModule, "IsPythonInstalled: Error: Unable to convert path, rc=%Rrc", rc);

        RTStrFree(pszPythonPath);
    }
    else
        logStringF(hModule, "IsPythonInstalled: Error: No suitable Python installation found (%Rrc), skipping installation.", rc);

    VBoxSetMsiProp(hModule, L"VBOX_PYTHON_INSTALLED", RT_SUCCESS(rc) ? L"1" : L"0");

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
    logStringF(hModule, "InstallPythonAPI: Checking for installed Python environment(s) ...");

    char *pszPythonPath;
    int rc = getPythonPath(hModule, HKEY_LOCAL_MACHINE, &pszPythonPath);
    if (RT_FAILURE(rc))
        rc = getPythonPath(hModule, HKEY_CURRENT_USER, &pszPythonPath);

    if (RT_FAILURE(rc))
    {
        VBoxSetMsiProp(hModule, L"VBOX_API_INSTALLED", L"0");

        logStringF(hModule, "InstallPythonAPI: Python seems not to be installed (%Rrc); please download + install the Python Core package.", rc);
        return ERROR_SUCCESS;
    }

    logStringF(hModule, "InstallPythonAPI: Python installation found at \"%s\".", pszPythonPath);
    logStringF(hModule, "InstallPythonAPI: Checking for win32api extensions ...");

    uint32_t fProcess = 0;
#ifndef DEBUG
    uint32_t fProcess |= RTPROC_FLAGS_HIDDEN;
#endif

    char szPythonPath[RTPATH_MAX] = { 0 };
    rc = RTPathAppend(szPythonPath, sizeof(szPythonPath), pszPythonPath);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathAppend(szPythonPath, sizeof(szPythonPath), "python.exe");
        if (RT_SUCCESS(rc))
        {
            /*
             * Check if importing the win32api module works.
             * This is a prerequisite for setting up the VBox API.
             */
            RTPROCESS Process;
            RT_ZERO(Process);
            const char *papszArgs[4] = { szPythonPath, "-c", "import win32api", NULL};
            rc = RTProcCreate(szPythonPath, papszArgs, RTENV_DEFAULT, fProcess, &Process);
            if (RT_SUCCESS(rc))
            {
                RTPROCSTATUS ProcSts;
                rc = procWait(Process, RT_MS_30SEC, &ProcSts);
                if (RT_FAILURE(rc))
                    logStringF(hModule, "InstallPythonAPI: Importing the win32api failed with %d (exit reason: %d)\n",
                                   ProcSts.iStatus, ProcSts.enmReason);
            }
        }
    }

    /*
     * Set up the VBox API.
     */
    if (RT_SUCCESS(rc))
    {
        /* Get the VBox API setup string. */
        char *pszVBoxSDKPath;
        rc = VBoxGetMsiPropUtf8(hModule, "CustomActionData", &pszVBoxSDKPath);
        if (RT_SUCCESS(rc))
        {
            /* Make sure our current working directory is the VBox installation path. */
            rc = RTPathSetCurrent(pszVBoxSDKPath);
            if (RT_SUCCESS(rc))
            {
                /* Set required environment variables. */
                rc = RTEnvSet("VBOX_INSTALL_PATH", pszVBoxSDKPath);
                if (RT_SUCCESS(rc))
                {
                    RTPROCSTATUS ProcSts;
                    RT_ZERO(ProcSts);

                    logStringF(hModule, "InstallPythonAPI: Invoking vboxapisetup.py in \"%s\" ...\n", pszVBoxSDKPath);

                    RTPROCESS Process;
                    RT_ZERO(Process);
                    const char *papszArgs[4] = { szPythonPath, "vboxapisetup.py", "install", NULL};
                    rc = RTProcCreate(szPythonPath, papszArgs, RTENV_DEFAULT, fProcess, &Process);
                    if (RT_SUCCESS(rc))
                    {
                        rc = procWait(Process, RT_MS_30SEC, &ProcSts);
                        if (RT_SUCCESS(rc))
                            logStringF(hModule, "InstallPythonAPI: Installation of vboxapisetup.py successful\n");
                    }

                    if (RT_FAILURE(rc))
                        logStringF(hModule, "InstallPythonAPI: Calling vboxapisetup.py failed with %Rrc (process status: %d, exit reason: %d)\n",
                                   rc, ProcSts.iStatus, ProcSts.enmReason);
                }
                else
                    logStringF(hModule, "InstallPythonAPI: Could set environment variable VBOX_INSTALL_PATH, rc=%Rrc\n", rc);
            }
            else
                logStringF(hModule, "InstallPythonAPI: Could set working directory to \"%s\", rc=%Rrc\n", pszVBoxSDKPath, rc);

            RTStrFree(pszVBoxSDKPath);
        }
        else
            logStringF(hModule, "InstallPythonAPI: Unable to retrieve VBox installation directory, rc=%Rrc\n", rc);
    }

    /*
     * Do some sanity checking if the VBox API works.
     */
    if (RT_SUCCESS(rc))
    {
        logStringF(hModule, "InstallPythonAPI: Validating VBox API ...\n");

        RTPROCSTATUS ProcSts;
        RT_ZERO(ProcSts);

        RTPROCESS Process;
        RT_ZERO(Process);
        const char *papszArgs[4] = { szPythonPath, "-c", "from vboxapi import VirtualBoxManager", NULL};
        rc = RTProcCreate(szPythonPath, papszArgs, RTENV_DEFAULT, fProcess, &Process);
        if (RT_SUCCESS(rc))
        {
            rc = procWait(Process, RT_MS_30SEC, &ProcSts);
            if (RT_SUCCESS(rc))
                logStringF(hModule, "InstallPythonAPI: VBox API looks good.\n");
        }

        if (RT_FAILURE(rc))
            logStringF(hModule, "InstallPythonAPI: Validating VBox API failed with %Rrc (process status: %d, exit reason: %d)\n",
                       rc, ProcSts.iStatus, ProcSts.enmReason);
    }

    RTStrFree(pszPythonPath);

    VBoxSetMsiProp(hModule, L"VBOX_API_INSTALLED", RT_SUCCESS(rc) ? L"1" : L"0");

    if (RT_FAILURE(rc))
        logStringF(hModule, "InstallPythonAPI: Installation failed with %Rrc\n", rc);

    return ERROR_SUCCESS; /* Do not fail here. */
}

static LONG installBrandingValue(MSIHANDLE hModule,
                                 const WCHAR *pwszFileName,
                                 const WCHAR *pwszSection,
                                 const WCHAR *pwszValue)
{
    LONG rc;
    WCHAR wszValue[_MAX_PATH];
    if (GetPrivateProfileStringW(pwszSection, pwszValue, NULL,
                                 wszValue, sizeof(wszValue), pwszFileName) > 0)
    {
        HKEY hkBranding;
        WCHAR wszKey[_MAX_PATH];

        if (wcsicmp(L"General", pwszSection) != 0)
            swprintf_s(wszKey, RT_ELEMENTS(wszKey), L"SOFTWARE\\%S\\VirtualBox\\Branding\\%s", VBOX_VENDOR_SHORT, pwszSection);
        else
            swprintf_s(wszKey, RT_ELEMENTS(wszKey), L"SOFTWARE\\%S\\VirtualBox\\Branding", VBOX_VENDOR_SHORT);

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
                logStringF(hModule, "InstallBranding: Could not write value %s! Error %ld", pwszValue, rc);
            RegCloseKey (hkBranding);
        }
    }
    else
        rc = ERROR_NOT_FOUND;
    return rc;
}

UINT CopyDir(MSIHANDLE hModule, const WCHAR *pwszDestDir, const WCHAR *pwszSourceDir)
{
    UINT rc;
    WCHAR wszDest[_MAX_PATH + 1];
    WCHAR wszSource[_MAX_PATH + 1];

    swprintf_s(wszDest, RT_ELEMENTS(wszDest), L"%s%c", pwszDestDir, L'\0');
    swprintf_s(wszSource, RT_ELEMENTS(wszSource), L"%s%c", pwszSourceDir, L'\0');

    SHFILEOPSTRUCTW s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_COPY;
    s.pTo = wszDest;
    s.pFrom = wszSource;
    s.fFlags = FOF_SILENT |
               FOF_NOCONFIRMATION |
               FOF_NOCONFIRMMKDIR |
               FOF_NOERRORUI;

    logStringF(hModule, "CopyDir: DestDir=%s, SourceDir=%s", wszDest, wszSource);
    int r = SHFileOperationW(&s);
    if (r != 0)
    {
        logStringF(hModule, "CopyDir: Copy operation returned status 0x%x", r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT RemoveDir(MSIHANDLE hModule, const WCHAR *pwszDestDir)
{
    UINT rc;
    WCHAR wszDest[_MAX_PATH + 1];

    swprintf_s(wszDest, RT_ELEMENTS(wszDest), L"%s%c", pwszDestDir, L'\0');

    SHFILEOPSTRUCTW s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_DELETE;
    s.pFrom = wszDest;
    s.fFlags = FOF_SILENT
             | FOF_NOCONFIRMATION
             | FOF_NOCONFIRMMKDIR
             | FOF_NOERRORUI;

    logStringF(hModule, "RemoveDir: DestDir=%s", wszDest);
    int r = SHFileOperationW(&s);
    if (r != 0)
    {
        logStringF(hModule, "RemoveDir: Remove operation returned status 0x%x", r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT RenameDir(MSIHANDLE hModule, const WCHAR *pwszDestDir, const WCHAR *pwszSourceDir)
{
    UINT rc;
    WCHAR wszDest[_MAX_PATH + 1];
    WCHAR wszSource[_MAX_PATH + 1];

    swprintf_s(wszDest, RT_ELEMENTS(wszDest), L"%s%c", pwszDestDir, L'\0');
    swprintf_s(wszSource, RT_ELEMENTS(wszSource), L"%s%c", pwszSourceDir, L'\0');

    SHFILEOPSTRUCTW s = {0};
    s.hwnd = NULL;
    s.wFunc = FO_RENAME;
    s.pTo = wszDest;
    s.pFrom = wszSource;
    s.fFlags = FOF_SILENT
             | FOF_NOCONFIRMATION
             | FOF_NOCONFIRMMKDIR
             | FOF_NOERRORUI;

    logStringF(hModule, "RenameDir: DestDir=%s, SourceDir=%s", wszDest, wszSource);
    int r = SHFileOperationW(&s);
    if (r != 0)
    {
        logStringF(hModule, "RenameDir: Rename operation returned status 0x%x", r);
        rc = ERROR_GEN_FAILURE;
    }
    else
        rc = ERROR_SUCCESS;
    return rc;
}

UINT __stdcall UninstallBranding(MSIHANDLE hModule)
{
    UINT rc;
    logStringF(hModule, "UninstallBranding: Handling branding file ...");

    WCHAR wszPathTargetDir[_MAX_PATH];
    WCHAR wszPathDest[_MAX_PATH];

    rc = VBoxGetMsiProp(hModule, L"CustomActionData", wszPathTargetDir, sizeof(wszPathTargetDir));
    if (rc == ERROR_SUCCESS)
    {
        /** @todo Check trailing slash after %s. */
/** @todo r=bird: using swprintf_s for formatting paths without checking for
 * overflow not good.  You're dodging the buffer overflow issue only to end up
 * with incorrect behavior!  (Truncated file/dir names)
 *
 * This applies almost to all swprintf_s calls in this file!!
 */
        swprintf_s(wszPathDest, RT_ELEMENTS(wszPathDest), L"%scustom", wszPathTargetDir);
        rc = RemoveDir(hModule, wszPathDest);
        if (rc != ERROR_SUCCESS)
        {
            /* Check for hidden .custom directory and remove it. */
            swprintf_s(wszPathDest, RT_ELEMENTS(wszPathDest), L"%s.custom", wszPathTargetDir);
            rc = RemoveDir(hModule, wszPathDest);
        }
    }

    logStringF(hModule, "UninstallBranding: Handling done. (rc=%u (ignored))", rc);
    return ERROR_SUCCESS; /* Do not fail here. */
}

UINT __stdcall InstallBranding(MSIHANDLE hModule)
{
    UINT rc;
    logStringF(hModule, "InstallBranding: Handling branding file ...");

    WCHAR wszPathMSI[_MAX_PATH];
    rc = VBoxGetMsiProp(hModule, L"SOURCEDIR", wszPathMSI, sizeof(wszPathMSI));
    if (rc == ERROR_SUCCESS)
    {
        WCHAR wszPathTargetDir[_MAX_PATH];
        rc = VBoxGetMsiProp(hModule, L"CustomActionData", wszPathTargetDir, sizeof(wszPathTargetDir));
        if (rc == ERROR_SUCCESS)
        {
            /** @todo Check for trailing slash after %s. */
            WCHAR wszPathDest[_MAX_PATH];
            swprintf_s(wszPathDest, RT_ELEMENTS(wszPathDest), L"%s", wszPathTargetDir);

            WCHAR wszPathSource[_MAX_PATH];
            swprintf_s(wszPathSource, RT_ELEMENTS(wszPathSource), L"%s.custom", wszPathMSI);
            rc = CopyDir(hModule, wszPathDest, wszPathSource);
            if (rc == ERROR_SUCCESS)
            {
                swprintf_s(wszPathDest, RT_ELEMENTS(wszPathDest), L"%scustom", wszPathTargetDir);
                swprintf_s(wszPathSource, RT_ELEMENTS(wszPathSource), L"%s.custom", wszPathTargetDir);
                rc = RenameDir(hModule, wszPathDest, wszPathSource);
            }
        }
    }

    logStringF(hModule, "InstallBranding: Handling done. (rc=%u (ignored))", rc);
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
                logStringF(g_hCurrentModule, pszMsg);
            break;
        default:
            break;
    }
}

static DECLCALLBACK(void) netCfgLoggerCallback(const char *pszString)
{
    if (g_hCurrentModule)
        logStringF(g_hCurrentModule, pszString);
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
            logStringF(hModule, "Reboot required, setting REBOOT property to \"force\"");
            HRESULT hr2 = MsiSetPropertyW(hModule, L"REBOOT", L"Force");
            if (hr2 != ERROR_SUCCESS)
                logStringF(hModule, "Failed to set REBOOT property, error = 0x%x", hr2);
            uRet = ERROR_SUCCESS; /* Never fail here. */
            break;
        }

        default:
            logStringF(hModule, "Converting unhandled HRESULT (0x%x) to ERROR_GEN_FAILURE", hr);
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
            logStringF(hModule, "createNetCfgLockedMsgRecord: MsiRecordSetInteger failed, error = 0x%x", uErr);
            MsiCloseHandle(hRecord);
            hRecord = NULL;
        }
    }
    else
        logStringF(hModule, "createNetCfgLockedMsgRecord: Failed to create a record");

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
                logStringF(hModule, "doNetCfgInit: VBoxNetCfgWinQueryINetCfg failed, error = 0x%x", hr);
            uErr = errorConvertFromHResult(hModule, hr);
            break;
        }

        /* hr == NETCFG_E_NO_WRITE_LOCK */

        if (!lpszLockedBy)
        {
            logStringF(hModule, "doNetCfgInit: lpszLockedBy == NULL, breaking");
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
            logStringF(hModule, "doNetCfgInit: lpszLockedBy is 6to4svc.dll, retrying %d out of %d", cRetries, VBOX_NETCFG_MAX_RETRIES);
            MsgResult = IDRETRY;
        }
        else
        {
            if (!hMsg)
            {
                hMsg = createNetCfgLockedMsgRecord(hModule);
                if (!hMsg)
                {
                    logStringF(hModule, "doNetCfgInit: Failed to create a message record, breaking");
                    CoTaskMemFree(lpszLockedBy);
                    break;
                }
            }

            UINT rTmp = MsiRecordSetStringW(hMsg, 2, lpszLockedBy);
            NonStandardAssert(rTmp == ERROR_SUCCESS);
            if (rTmp != ERROR_SUCCESS)
            {
                logStringF(hModule, "doNetCfgInit: MsiRecordSetStringW failed, error = 0x%x", rTmp);
                CoTaskMemFree(lpszLockedBy);
                break;
            }

            MsgResult = MsiProcessMessage(hModule, (INSTALLMESSAGE)(INSTALLMESSAGE_USER | MB_RETRYCANCEL), hMsg);
            NonStandardAssert(MsgResult == IDRETRY || MsgResult == IDCANCEL);
            logStringF(hModule, "doNetCfgInit: MsiProcessMessage returned (0x%x)", MsgResult);
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
        logStringF(hModule, "vboxNetFltQueryInfArray: INF 1: %s", pwszPtInf);

        wcsncat(pwszMpInf, NETFLT_MP_INF_REL_PATH, sizeof(NETFLT_MP_INF_REL_PATH));
        logStringF(hModule, "vboxNetFltQueryInfArray: INF 2: %s", pwszMpInf);
    }
    else if (uErr != ERROR_SUCCESS)
        logStringF(hModule, "vboxNetFltQueryInfArray: MsiGetPropertyW failed, error = 0x%x", uErr);
    else
    {
        logStringF(hModule, "vboxNetFltQueryInfArray: Empty installation directory");
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
        logStringF(hModule, "Uninstalling NetFlt");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetFltUninstall(pNetCfg);
            if (hr != S_OK)
                logStringF(hModule, "UninstallNetFlt: VBoxNetCfgWinUninstallComponent failed, error = 0x%x", hr);

            uErr = errorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, "Uninstalling NetFlt done, error = 0x%x", uErr);
        }
        else
            logStringF(hModule, "UninstallNetFlt: doNetCfgInit failed, error = 0x%x", uErr);
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

        logStringF(hModule, "InstallNetFlt: Installing NetFlt");

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
                    logStringF(hModule, "InstallNetFlt: VBoxNetCfgWinNetFltInstall failed, error = 0x%x", hr);

                uErr = errorConvertFromHResult(hModule, hr);
            }
            else
                logStringF(hModule, "InstallNetFlt: vboxNetFltQueryInfArray failed, error = 0x%x", uErr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, "InstallNetFlt: Done");
        }
        else
            logStringF(hModule, "InstallNetFlt: doNetCfgInit failed, error = 0x%x", uErr);
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
        logStringF(hModule, "Uninstalling NetLwf");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetLwfUninstall(pNetCfg);
            if (hr != S_OK)
                logStringF(hModule, "UninstallNetLwf: VBoxNetCfgWinUninstallComponent failed, error = 0x%x", hr);

            uErr = errorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, "Uninstalling NetLwf done, error = 0x%x", uErr);
        }
        else
            logStringF(hModule, "UninstallNetLwf: doNetCfgInit failed, error = 0x%x", uErr);
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

        logStringF(hModule, "InstallNetLwf: Installing NetLwf");

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
                        logStringF(hModule, "InstallNetLwf: VBoxNetCfgWinNetLwfInstall failed, error = 0x%x", hr);

                    uErr = errorConvertFromHResult(hModule, hr);
                }
                else
                {
                    logStringF(hModule, "vboxNetFltQueryInfArray: Empty installation directory");
                    uErr = ERROR_GEN_FAILURE;
                }
            }
            else
                logStringF(hModule, "vboxNetFltQueryInfArray: MsiGetPropertyW failed, error = 0x%x", uErr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, "InstallNetLwf: Done");
        }
        else
            logStringF(hModule, "InstallNetLwf: doNetCfgInit failed, error = 0x%x", uErr);
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

    logStringF(hModule, "CreateHostOnlyInterface: Creating host-only interface");

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
            logStringF(hModule, "CreateHostOnlyInterface: NetAdpDir property = %s", wszMpInf);
            if (wszMpInf[cchMpInf - 1] != L'\\')
            {
                wszMpInf[cchMpInf++] = L'\\';
                wszMpInf[cchMpInf]   = L'\0';
            }

            wcscat(wszMpInf, pwszInfName);
            pwszInfPath = wszMpInf;
            fIsFile = true;

            logStringF(hModule, "CreateHostOnlyInterface: Resulting INF path = %s", pwszInfPath);
        }
        else
            logStringF(hModule, "CreateHostOnlyInterface: VBox installation path is empty");
    }
    else
        logStringF(hModule, "CreateHostOnlyInterface: Unable to retrieve VBox installation path, error = 0x%x", uErr);

    /* Make sure the inf file is installed. */
    if (pwszInfPath != NULL && fIsFile)
    {
        logStringF(hModule, "CreateHostOnlyInterface: Calling VBoxDrvCfgInfInstall(%s)", pwszInfPath);
        hr = VBoxDrvCfgInfInstall(pwszInfPath);
        logStringF(hModule, "CreateHostOnlyInterface: VBoxDrvCfgInfInstall returns 0x%x", hr);
        if (FAILED(hr))
            logStringF(hModule, "CreateHostOnlyInterface: Failed to install INF file, error = 0x%x", hr);
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
                logStringF(hModule, "CreateHostOnlyInterface: Reboot required for update, setting REBOOT property to force");
                HRESULT hr2 = MsiSetPropertyW(hModule, L"REBOOT", L"Force");
                if (hr2 != ERROR_SUCCESS)
                    logStringF(hModule, "CreateHostOnlyInterface: Failed to set REBOOT property for update, error = 0x%x", hr2);
            }
        }
        else
        {
            //in fail case call CreateHostOnlyInterface
            logStringF(hModule, "CreateHostOnlyInterface: VBoxNetCfgWinUpdateHostOnlyNetworkInterface failed, hr = 0x%x", hr);
            logStringF(hModule, "CreateHostOnlyInterface: calling VBoxNetCfgWinCreateHostOnlyNetworkInterface");
#ifdef VBOXNETCFG_DELAYEDRENAME
            BSTR devId;
            hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface(pwszInfPath, fIsFile, NULL, &guid, &devId, NULL);
#else /* !VBOXNETCFG_DELAYEDRENAME */
            hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface(pwszInfPath, fIsFile, NULL, &guid, NULL, NULL);
#endif /* !VBOXNETCFG_DELAYEDRENAME */
            logStringF(hModule, "CreateHostOnlyInterface: VBoxNetCfgWinCreateHostOnlyNetworkInterface returns 0x%x", hr);
            if (SUCCEEDED(hr))
            {
                ULONG ip = inet_addr("192.168.56.1");
                ULONG mask = inet_addr("255.255.255.0");
                logStringF(hModule, "CreateHostOnlyInterface: calling VBoxNetCfgWinEnableStaticIpConfig");
                hr = VBoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
                logStringF(hModule, "CreateHostOnlyInterface: VBoxNetCfgWinEnableStaticIpConfig returns 0x%x", hr);
                if (FAILED(hr))
                    logStringF(hModule, "CreateHostOnlyInterface: VBoxNetCfgWinEnableStaticIpConfig failed, error = 0x%x", hr);
#ifdef VBOXNETCFG_DELAYEDRENAME
                hr = VBoxNetCfgWinRenameHostOnlyConnection(&guid, devId, NULL);
                if (FAILED(hr))
                    logStringF(hModule, "CreateHostOnlyInterface: VBoxNetCfgWinRenameHostOnlyConnection failed, error = 0x%x", hr);
                SysFreeString(devId);
#endif /* VBOXNETCFG_DELAYEDRENAME */
            }
            else
                logStringF(hModule, "CreateHostOnlyInterface: VBoxNetCfgWinCreateHostOnlyNetworkInterface failed, error = 0x%x", hr);
        }
    }

    if (SUCCEEDED(hr))
        logStringF(hModule, "CreateHostOnlyInterface: Creating host-only interface done");

    /* Restore original setup mode. */
    logStringF(hModule, "CreateHostOnlyInterface: Almost done...");
    if (fSetupModeInteractive)
        SetupSetNonInteractiveMode(fSetupModeInteractive);

    netCfgLoggerDisable();

#endif /* VBOX_WITH_NETFLT */

    logStringF(hModule, "CreateHostOnlyInterface: Returns success (ignoring all failures)");
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

    logStringF(hModule, "RemoveHostOnlyInterfaces: Removing all host-only interfaces");

    BOOL fSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    HRESULT hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(pwszId);
    if (SUCCEEDED(hr))
    {
        hr = VBoxDrvCfgInfUninstallAllSetupDi(&GUID_DEVCLASS_NET, L"Net", pwszId, SUOI_FORCEDELETE/* could be SUOI_FORCEDELETE */);
        if (FAILED(hr))
        {
            logStringF(hModule, "RemoveHostOnlyInterfaces: NetAdp uninstalled successfully, but failed to remove INF files");
        }
        else
            logStringF(hModule, "RemoveHostOnlyInterfaces: NetAdp uninstalled successfully");

    }
    else
        logStringF(hModule, "RemoveHostOnlyInterfaces: NetAdp uninstall failed, hr = 0x%x", hr);

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

    logStringF(hModule, "StopHostOnlyInterfaces: Stopping all host-only interfaces");

    BOOL fSetupModeInteractive = SetupSetNonInteractiveMode(FALSE);

    HRESULT hr = VBoxNetCfgWinPropChangeAllNetDevicesOfId(pwszId, VBOXNECTFGWINPROPCHANGE_TYPE_DISABLE);
    if (SUCCEEDED(hr))
    {
        logStringF(hModule, "StopHostOnlyInterfaces: Disabling host interfaces was successful, hr = 0x%x", hr);
    }
    else
        logStringF(hModule, "StopHostOnlyInterfaces: Disabling host interfaces failed, hr = 0x%x", hr);

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

    logStringF(hModule, "UpdateHostOnlyInterfaces: Updating all host-only interfaces");

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
            logStringF(hModule, "UpdateHostOnlyInterfaces: NetAdpDir property = %s", wszMpInf);
            if (wszMpInf[cchMpInf - 1] != L'\\')
            {
                wszMpInf[cchMpInf++] = L'\\';
                wszMpInf[cchMpInf]   = L'\0';
            }

            wcscat(wszMpInf, pwszInfName);
            pwszInfPath = wszMpInf;
            fIsFile = true;

            logStringF(hModule, "UpdateHostOnlyInterfaces: Resulting INF path = %s", pwszInfPath);

            DWORD attrFile = GetFileAttributesW(pwszInfPath);
            if (attrFile == INVALID_FILE_ATTRIBUTES)
            {
                DWORD dwErr = GetLastError();
                logStringF(hModule, "UpdateHostOnlyInterfaces: File \"%s\" not found, dwErr=%ld",
                           pwszInfPath, dwErr);
            }
            else
            {
                logStringF(hModule, "UpdateHostOnlyInterfaces: File \"%s\" exists",
                           pwszInfPath);

                BOOL fRebootRequired = FALSE;
                HRESULT hr = VBoxNetCfgWinUpdateHostOnlyNetworkInterface(pwszInfPath, &fRebootRequired, pwszId);
                if (SUCCEEDED(hr))
                {
                    if (fRebootRequired)
                    {
                        logStringF(hModule, "UpdateHostOnlyInterfaces: Reboot required, setting REBOOT property to force");
                        HRESULT hr2 = MsiSetPropertyW(hModule, L"REBOOT", L"Force");
                        if (hr2 != ERROR_SUCCESS)
                            logStringF(hModule, "UpdateHostOnlyInterfaces: Failed to set REBOOT property, error = 0x%x", hr2);
                    }
                }
                else
                    logStringF(hModule, "UpdateHostOnlyInterfaces: VBoxNetCfgWinUpdateHostOnlyNetworkInterface failed, hr = 0x%x", hr);
            }
        }
        else
            logStringF(hModule, "UpdateHostOnlyInterfaces: VBox installation path is empty");
    }
    else
        logStringF(hModule, "UpdateHostOnlyInterfaces: Unable to retrieve VBox installation path, error = 0x%x", uErr);

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
        logStringF(hModule, "Uninstalling NetAdp");

        uErr = doNetCfgInit(hModule, &pNetCfg, TRUE);
        if (uErr == ERROR_SUCCESS)
        {
            HRESULT hr = VBoxNetCfgWinNetAdpUninstall(pNetCfg, pwszId);
            if (hr != S_OK)
                logStringF(hModule, "UninstallNetAdp: VBoxNetCfgWinUninstallComponent failed, error = 0x%x", hr);

            uErr = errorConvertFromHResult(hModule, hr);

            VBoxNetCfgWinReleaseINetCfg(pNetCfg, TRUE);

            logStringF(hModule, "Uninstalling NetAdp done, error = 0x%x", uErr);
        }
        else
            logStringF(hModule, "UninstallNetAdp: doNetCfgInit failed, error = 0x%x", uErr);
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
    do
    {
        WCHAR wszPnPInstanceId[512] = {0};

        /* We have to find the device instance ID through a registry search */

        HKEY hkeyNetwork = 0;
        HKEY hkeyConnection = 0;

        do /* break-loop */
        {
            WCHAR wszRegLocation[256];
            swprintf(wszRegLocation,
                     L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s",
                     pwszGUID);
            LONG lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, wszRegLocation, 0, KEY_READ, &hkeyNetwork);
            if ((lStatus != ERROR_SUCCESS) || !hkeyNetwork)
                SetErrBreak((hModule, "VBox HostInterfaces: Host interface network was not found in registry (%s)! [1]",
                             wszRegLocation));

            lStatus = RegOpenKeyExW(hkeyNetwork, L"Connection", 0, KEY_READ, &hkeyConnection);
            if ((lStatus != ERROR_SUCCESS) || !hkeyConnection)
                SetErrBreak((hModule, "VBox HostInterfaces: Host interface network was not found in registry (%s)! [2]",
                             wszRegLocation));

            DWORD len = sizeof(wszPnPInstanceId);
            DWORD dwKeyType;
            lStatus = RegQueryValueExW(hkeyConnection, L"PnPInstanceID", NULL,
                                       &dwKeyType, (LPBYTE)&wszPnPInstanceId[0], &len);
            if ((lStatus != ERROR_SUCCESS) || (dwKeyType != REG_SZ))
                SetErrBreak((hModule, "VBox HostInterfaces: Host interface network was not found in registry (%s)! [3]",
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

        do
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
                logStringF(hModule, "VBox HostInterfaces: SetupDiGetClassDevs failed (0x%08X)!", GetLastError());
                SetErrBreak((hModule, "VBox HostInterfaces: Uninstallation failed!"));
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
                    logStringF(hModule, "VBox HostInterfaces: SetupDiSetSelectedDevice failed (0x%08X)!", GetLastError());
                    SetErrBreak((hModule, "VBox HostInterfaces: Uninstallation failed!"));
                }

                fResult = SetupDiCallClassInstaller(DIF_REMOVE, hDeviceInfo, &DeviceInfoData);
                if (!fResult)
                {
                    logStringF(hModule, "VBox HostInterfaces: SetupDiCallClassInstaller (DIF_REMOVE) failed (0x%08X)!", GetLastError());
                    SetErrBreak((hModule, "VBox HostInterfaces: Uninstallation failed!"));
                }
            }
            else
                SetErrBreak((hModule, "VBox HostInterfaces: Host interface network device not found!"));
        } while (0);

        /* clean up the device info set */
        if (hDeviceInfo != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList(hDeviceInfo);
    } while (0);
    return rc;
}

UINT __stdcall UninstallTAPInstances(MSIHANDLE hModule)
{
    static const WCHAR *s_wszNetworkKey = L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
    HKEY hCtrlNet;

    LONG lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, s_wszNetworkKey, 0, KEY_READ, &hCtrlNet);
    if (lStatus == ERROR_SUCCESS)
    {
        logStringF(hModule, "VBox HostInterfaces: Enumerating interfaces ...");
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
                        logStringF(hModule, "VBox HostInterfaces: No interfaces found.");
                        break;
                    default:
                        logStringF(hModule, "VBox HostInterfaces: Enumeration failed: %ld", lStatus);
                        break;
                }
                break;
            }

            if (isTAPDevice(wszNetworkGUID))
            {
                logStringF(hModule, "VBox HostInterfaces: Removing interface \"%s\" ...", wszNetworkGUID);
                removeNetworkInterface(hModule, wszNetworkGUID);
                lStatus = RegDeleteKeyW(hCtrlNet, wszNetworkGUID);
            }
        }
        RegCloseKey(hCtrlNet);
        logStringF(hModule, "VBox HostInterfaces: Removing interfaces done.");
    }
    return ERROR_SUCCESS;
}

