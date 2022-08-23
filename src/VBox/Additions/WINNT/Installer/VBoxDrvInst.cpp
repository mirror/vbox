/* $Id$ */
/** @file
 * VBoxDrvInst - Driver and service installation helper for Windows guests.
 */

/*
 * Copyright (C) 2011-2022 Oracle and/or its affiliates.
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
#ifndef UNICODE
# define UNICODE
#endif

#include <iprt/alloca.h>
#include <VBox/version.h>

#include <iprt/win/windows.h>
#include <iprt/win/setupapi.h>

#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/path.h>      /* RTPATH_IS_SEP */
#include <iprt/string.h>
#include <iprt/utf16.h>


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
/* Exit codes */
#define EXIT_OK      (0)
#define EXIT_REBOOT  (1)
#define EXIT_FAIL    (2)
#define EXIT_USAGE   (3)

/* Defines */
#define DRIVER_PACKAGE_REPAIR                   0x00000001
#define DRIVER_PACKAGE_SILENT                   0x00000002
#define DRIVER_PACKAGE_FORCE                    0x00000004
#define DRIVER_PACKAGE_ONLY_IF_DEVICE_PRESENT   0x00000008
#define DRIVER_PACKAGE_LEGACY_MODE              0x00000010
#define DRIVER_PACKAGE_DELETE_FILES             0x00000020

/* DIFx error codes */
/** @todo any reason why we're not using difxapi.h instead of these redefinitions? */
#ifndef ERROR_DRIVER_STORE_ADD_FAILED
# define ERROR_DRIVER_STORE_ADD_FAILED          (APPLICATION_ERROR_MASK | ERROR_SEVERITY_ERROR | 0x0247L)
#endif
#define ERROR_DEPENDENT_APPLICATIONS_EXIST      (APPLICATION_ERROR_MASK | ERROR_SEVERITY_ERROR|0x300)
#define ERROR_DRIVER_PACKAGE_NOT_IN_STORE       (APPLICATION_ERROR_MASK | ERROR_SEVERITY_ERROR | 0x302)

/* Registry string list flags */
#define VBOX_REG_STRINGLIST_NONE                0x00000000        /**< No flags set. */
#define VBOX_REG_STRINGLIST_ALLOW_DUPLICATES    0x00000001        /**< Allows duplicates in list when adding a value. */

#ifdef DEBUG
# define VBOX_DRVINST_LOGFILE                 "C:\\Temp\\VBoxDrvInstDIFx.log"
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct
{
    PWSTR pApplicationId;
    PWSTR pDisplayName;
    PWSTR pProductName;
    PWSTR pMfgName;
} INSTALLERINFO, *PINSTALLERINFO;
typedef const PINSTALLERINFO PCINSTALLERINFO;

typedef enum
{
    DIFXAPI_SUCCESS,
    DIFXAPI_INFO,
    DIFXAPI_WARNING,
    DIFXAPI_ERROR
} DIFXAPI_LOG;

typedef void (__cdecl * DIFXAPILOGCALLBACK_W)(DIFXAPI_LOG Event, DWORD Error, PCWSTR EventDescription, PVOID CallbackContext);

typedef DWORD (WINAPI *fnDriverPackageInstall)(PCTSTR DriverPackageInfPath, DWORD Flags, PCINSTALLERINFO pInstallerInfo, BOOL *pNeedReboot);
fnDriverPackageInstall g_pfnDriverPackageInstall = NULL;

typedef DWORD (WINAPI *fnDriverPackageUninstall)(PCTSTR DriverPackageInfPath, DWORD Flags, PCINSTALLERINFO pInstallerInfo, BOOL *pNeedReboot);
fnDriverPackageUninstall g_pfnDriverPackageUninstall = NULL;

typedef VOID (WINAPI *fnDIFXAPISetLogCallback)(DIFXAPILOGCALLBACK_W LogCallback, PVOID CallbackContext);
fnDIFXAPISetLogCallback g_pfnDIFXAPISetLogCallback = NULL;


/** @name Output helpers
 *
 * The general ASSUMPTION here is that all strings are restricted to 7-bit
 * ASCII, with the exception of wchar_t ones.
 *
 * @note We don't use printf, RTPrintf or similar not for masochistic reasons
 *       but to keep the binary small and make it easier to switch between CRT
 *       and IPRT w/ no-CRT.
 *
 * @{
 */

static void OutputWStr(HANDLE hDst, const wchar_t *pwszStr)
{
    DWORD cbIgn;
    if (GetConsoleMode(hDst, &cbIgn))
        WriteConsoleW(hDst, pwszStr, (DWORD)RTUtf16Len(pwszStr), &cbIgn, NULL);
    else
    {
        char *pszTmp;
        int rc = RTUtf16ToUtf8(pwszStr, &pszTmp);
        if (RT_SUCCESS(rc))
        {
            char *pszInCodepage;
            rc = RTStrUtf8ToCurrentCP(&pszInCodepage, pszTmp);
            if (RT_SUCCESS(rc))
            {
                WriteFile(hDst, pszInCodepage, (DWORD)strlen(pszInCodepage), &cbIgn, NULL);
                RTStrFree(pszInCodepage);
            }
            else
                WriteFile(hDst, RT_STR_TUPLE("<RTStrUtf8ToCurrentCP error>"), &cbIgn, NULL);
            RTStrFree(pszTmp);
        }
        else
            WriteFile(hDst, RT_STR_TUPLE("<RTUtf16ToUtf8 error>"), &cbIgn, NULL);
    }
}


static void ErrorMsgBegin(const char *pszMsg)
{
    HANDLE const hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD        cbIgn;
    WriteFile(hStdErr, RT_STR_TUPLE("error: "), &cbIgn, NULL);
    WriteFile(hStdErr, pszMsg, (DWORD)strlen(pszMsg), &cbIgn, NULL);
}


static void ErrorMsgStr(const char *pszMsg)
{
    HANDLE const hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD cbIgn;
    WriteFile(hStdErr, pszMsg, (DWORD)strlen(pszMsg), &cbIgn, NULL);
}


static void ErrorMsgWStr(const wchar_t *pwszStr)
{
    OutputWStr(GetStdHandle(STD_ERROR_HANDLE), pwszStr);
}


static int ErrorMsgEnd(const char *pszMsg)
{
    HANDLE const hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD        cbIgn;
    if (pszMsg)
        WriteFile(hStdErr, pszMsg, (DWORD)strlen(pszMsg), &cbIgn, NULL);
    WriteFile(hStdErr, RT_STR_TUPLE("\r\n"), &cbIgn, NULL);
    return EXIT_FAIL;
}


static void ErrorMsgU64(uint64_t uValue, bool fSigned = false)
{
    char szVal[128];
    RTStrFormatU64(szVal, sizeof(szVal), uValue, 10, 0, 0, fSigned ? RTSTR_F_VALSIGNED : 0);
    ErrorMsgStr(szVal);
}


static int ErrorMsg(const char *pszMsg)
{
    ErrorMsgBegin(pszMsg);
    return ErrorMsgEnd(NULL);
}


static int ErrorMsgSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3)
{
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    return ErrorMsgEnd(pszMsg3);
}


static int ErrorMsgSWSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3,
                         const wchar_t *pwszMsg4, const char *pszMsg5)
{
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgWStr(pwszMsg4);
    return ErrorMsgEnd(pszMsg5);
}


static void ErrorMsgErrVal(uint32_t uErrVal, bool fSigned)
{
    char    szVal[128];
    ssize_t cchVal = RTStrFormatU32(szVal, sizeof(szVal) - 1, uErrVal, 10, 0, 0, fSigned ? RTSTR_F_VALSIGNED : 0);
    szVal[cchVal++] = '/';
    szVal[cchVal]   = '\0';
    ErrorMsgStr(szVal);

    RTStrFormatU32(szVal, sizeof(szVal) - 1, uErrVal, 16, 0, 0, RTSTR_F_SPECIAL);
    ErrorMsgStr(szVal);
}


static int ErrorMsgErr(const char *pszMsg, uint32_t uErrVal, const char *pszErrIntro, bool fSigned)
{
    ErrorMsgBegin(pszMsg);
    ErrorMsgStr(pszErrIntro);
    ErrorMsgErrVal(uErrVal, fSigned);
    return ErrorMsgEnd(")");
}


static int ErrorMsgVBoxErr(const char *pszMsg, int rc)
{
    return ErrorMsgErr(pszMsg, rc, " (", true);
}


static int ErrorMsgLastErr(const char *pszMsg)
{
    return ErrorMsgErr(pszMsg, GetLastError(), " (last error ", false);
}


static int ErrorMsgLastErrSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3)
{
    DWORD dwErr = GetLastError();
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgStr(" (last error ");
    ErrorMsgErrVal(dwErr, false);
    return ErrorMsgEnd(")");
}

static int ErrorMsgLastErrSWSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3,
                                const wchar_t *pwszMsg4, const char *pszMsg5)
{
    DWORD dwErr = GetLastError();
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgWStr(pwszMsg4);
    ErrorMsgStr(pszMsg5);
    ErrorMsgStr(" (last error ");
    ErrorMsgErrVal(dwErr, false);
    return ErrorMsgEnd(")");
}


static int ErrorMsgLastErrSWSRSUS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3, const char *pszMsg4,
                                  uint64_t uValue, const char *pszMsg5)
{
    DWORD dwErr = GetLastError();
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgStr(" (last error ");
    ErrorMsgErrVal(dwErr, false);
    ErrorMsgStr(")");
    ErrorMsgStr(pszMsg4);
    ErrorMsgU64(uValue);
    return ErrorMsgEnd(pszMsg5);
}


static int ErrorMsgLastErrSSS(const char *pszMsg1, const char *pszMsg2, const char *pszMsg3)
{
    DWORD dwErr = GetLastError();
    ErrorMsgBegin(pszMsg1);
    ErrorMsgStr(pszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgStr(" (last error ");
    ErrorMsgErrVal(dwErr, false);
    return ErrorMsgEnd(")");
}


static int ErrorMsgLStatus(const char *pszMsg, LSTATUS lrc)
{
    return ErrorMsgErr(pszMsg, (DWORD)lrc, " (", true);
}


static int ErrorMsgLStatusSWSRS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3,
                                LSTATUS lrc, const char *pszMsg4)
{
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgErrVal((DWORD)lrc, true);
    return ErrorMsgEnd(pszMsg4);
}


static int ErrorMsgLStatusSWSWSRS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3, const wchar_t *pwszMsg4,
                                  const char *pszMsg5, LSTATUS lrc, const char *pszMsg6)
{
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgWStr(pwszMsg4);
    ErrorMsgStr(pszMsg5);
    ErrorMsgErrVal((DWORD)lrc, true);
    return ErrorMsgEnd(pszMsg6);
}


static int ErrorMsgLStatusSWSWSWSRS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3, const wchar_t *pwszMsg4,
                                    const char *pszMsg5, const wchar_t *pwszMsg6, const char *pszMsg7,
                                    LSTATUS lrc, const char *pszMsg8)
{
    ErrorMsgBegin(pszMsg1);
    ErrorMsgWStr(pwszMsg2);
    ErrorMsgStr(pszMsg3);
    ErrorMsgWStr(pwszMsg4);
    ErrorMsgStr(pszMsg5);
    ErrorMsgWStr(pwszMsg6);
    ErrorMsgStr(pszMsg7);
    ErrorMsgErrVal((DWORD)lrc, true);
    return ErrorMsgEnd(pszMsg8);
}


static int ErrorBadArg(const char *pszName, wchar_t const *pwszArg, const char *pszValues = NULL)
{
    ErrorMsgBegin("Bad argument '");
    ErrorMsgStr(pszName);
    ErrorMsgStr("': ");
    ErrorMsgWStr(pwszArg);
    if (pszValues)
        ErrorMsgStr(", expected: ");
    return ErrorMsgEnd(pszValues);
}


/** Simple fputs(stdout) replacement. */
static void PrintStr(const char *pszMsg)
{
    HANDLE const hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD        cbIgn;
    WriteFile(hStdOut, pszMsg, (DWORD)strlen(pszMsg), &cbIgn, NULL);
}


/** Simple fputs(stdout) replacement. */
static void PrintWStr(const wchar_t *pwszStr)
{
    OutputWStr(GetStdHandle(STD_OUTPUT_HANDLE), pwszStr);
}


static void PrintX64(uint64_t uValue)
{
    char szVal[128];
    RTStrFormatU64(szVal, sizeof(szVal), uValue, 16, 0, 0, RTSTR_F_64BIT | RTSTR_F_SPECIAL);
    PrintStr(szVal);
}


static void PrintSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3)
{
    PrintStr(pszMsg1);
    PrintWStr(pwszMsg2);
    PrintStr(pszMsg3);
}


static void PrintSWSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3,
                       const wchar_t *pwszMsg4, const char *pszMsg5)
{
    PrintStr(pszMsg1);
    PrintWStr(pwszMsg2);
    PrintStr(pszMsg3);
    PrintWStr(pwszMsg4);
    PrintStr(pszMsg5);
}


static void PrintSWSWSWS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3, const wchar_t *pwszMsg4,
                         const char *pszMsg5, const wchar_t *pwszMsg6, const char *pszMsg7)
{
    PrintStr(pszMsg1);
    PrintWStr(pwszMsg2);
    PrintStr(pszMsg3);
    PrintWStr(pwszMsg4);
    PrintStr(pszMsg5);
    PrintWStr(pwszMsg6);
    PrintStr(pszMsg7);
}


static void PrintSXS(const char *pszMsg1, uint64_t uValue, const char *pszMsg2)
{
    PrintStr(pszMsg1);
    PrintX64(uValue);
    PrintStr(pszMsg2);
}


static void PrintSWSWSWSXS(const char *pszMsg1, const wchar_t *pwszMsg2, const char *pszMsg3, const wchar_t *pwszMsg4,
                           const char *pszMsg5, const wchar_t *pwszMsg6, const char *pszMsg7, uint64_t uValue, const char *pszMsg8)
{
    PrintStr(pszMsg1);
    PrintWStr(pwszMsg2);
    PrintStr(pszMsg3);
    PrintWStr(pwszMsg4);
    PrintStr(pszMsg5);
    PrintWStr(pwszMsg6);
    PrintStr(pszMsg7);
    PrintX64(uValue);
    PrintStr(pszMsg8);
}

/** @} */

static char *ArgToUtf8(wchar_t const *pwszString, const char *pszArgName)
{
    char *pszUtf8 = NULL;
    int rc = RTUtf16ToUtf8(pwszString, &pszUtf8);
    if (RT_SUCCESS(rc))
        return pszUtf8;
    ErrorMsgBegin("RTUtf16ToUtf8 failed on '");
    ErrorMsgStr(pszArgName);
    ErrorMsgStr("': ");
    ErrorMsgErrVal(rc, true);
    ErrorMsgEnd(NULL);
    return NULL;
}

/**
 * @returns false.
 * @note Frees pszValue
 */
static bool ErrorArtToNum(int rc, const char *pszArgName, char *pszValue)
{
    ErrorMsgBegin("Failed to convert the '");
    ErrorMsgStr(pszArgName);
    ErrorMsgStr("' value '");
    ErrorMsgStr(pszValue);
    ErrorMsgStr("' to a number: ");
    ErrorMsgErrVal(rc, true);
    ErrorMsgEnd(NULL);
    return false;
}


static bool ArgToUInt32Full(wchar_t const *pwszString, const char *pszArgName, uint32_t *puValue)
{
    char *pszValue = ArgToUtf8(pwszString, pszArgName);
    if (!pszValue)
        return false;
    int rc = RTStrToUInt32Full(pszValue, 0, puValue);
    if (RT_FAILURE(rc))
        return ErrorArtToNum(rc, pszArgName, pszValue);
    RTStrFree(pszValue);
    return true;
}


static bool ArgToUInt64Full(wchar_t const *pwszString, const char *pszArgName, uint64_t *puValue)
{
    char *pszValue = ArgToUtf8(pwszString, pszArgName);
    if (!pszValue)
        return false;
    int rc = RTStrToUInt64Full(pszValue, 0, puValue);
    if (rc != VINF_SUCCESS)
        return ErrorArtToNum(rc, pszArgName, pszValue);
    RTStrFree(pszValue);
    return true;
}



static bool GetErrorMsg(DWORD dwLastError, wchar_t *pwszMsg, DWORD cwcMsg)
{
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwLastError, 0, pwszMsg, cwcMsg, NULL) == 0)
        return false;
    wchar_t *pwc = RTUtf16Chr(pwszMsg, '\r');
    if (pwc)
        *pwc = '\0';
    return true;
}


/**
 * Log callback for DIFxAPI calls.
 *
 * @param   enmEvent        Event logging level.
 * @param   dwError         Event error number.
 * @param   pwszEventDesc   Event description text.
 * @param   pvCtx           Log file handle, if we've got one.
 */
static void __cdecl VBoxDIFxLogCallback(DIFXAPI_LOG enmEvent, DWORD dwError, PCWSTR pwszEventDesc, PVOID pvCtx)
{
    const char *pszEvent;
    switch (enmEvent)
    {
        case DIFXAPI_SUCCESS:   pszEvent  =  "DIFXAPI_SUCCESS"; break;
        case DIFXAPI_INFO:      pszEvent  =  "DIFXAPI_INFO";    break;
        case DIFXAPI_WARNING:   pszEvent  =  "DIFXAPI_WARNING"; break;
        case DIFXAPI_ERROR:     pszEvent  =  "DIFXAPI_ERROR";   break;
        default:                pszEvent  =  "DIFXAPI_<unknown>"; break;
    }

    /*
     * Log to standard output:
     */
    PrintStr(pszEvent);
    if (dwError == 0)
        PrintStr(": ");
    else
    {
        PrintStr(": ERROR: ");
        PrintX64(dwError);
        PrintStr(" - ");
    }
    PrintWStr(pwszEventDesc);
    PrintStr("\r\n");

    /*
     * Write to the log file if we have one (wide char format).
     */
    HANDLE const hLogFile = (HANDLE)pvCtx;
    if (hLogFile != INVALID_HANDLE_VALUE)
    {
        /* "event: err - desc\r\n" */
        wchar_t wszBuf[168];
        RTUtf16CopyAscii(wszBuf, RT_ELEMENTS(wszBuf), pszEvent);
        RTUtf16CatAscii(wszBuf, RT_ELEMENTS(wszBuf), ": ");
        char szVal[128];
        RTStrFormatU32(szVal, sizeof(szVal), dwError, 10, 0, 0, 0);
        RTUtf16CatAscii(wszBuf, RT_ELEMENTS(wszBuf), szVal);
        RTUtf16CatAscii(wszBuf, RT_ELEMENTS(wszBuf), " - ");

        DWORD dwIgn;
        WriteFile(hLogFile, wszBuf,        (DWORD)(RTUtf16Len(wszBuf)        * sizeof(wchar_t)), &dwIgn, NULL);
        WriteFile(hLogFile, pwszEventDesc, (DWORD)(RTUtf16Len(pwszEventDesc) * sizeof(wchar_t)), &dwIgn, NULL);
        WriteFile(hLogFile, L"\r\n", 2 * sizeof(wchar_t), &dwIgn, NULL);
    }
}


/**
 * Loads a DLL from the same directory as the installer.
 *
 * @returns Module handle, NULL on failure (fully messaged).
 * @param   pwszName            The DLL name.
 */
static HMODULE LoadAppDll(const wchar_t *pwszName)
{
    /* Get the process image path. */
    WCHAR  wszPath[MAX_PATH];
    UINT   cwcPath = GetModuleFileNameW(NULL, wszPath, MAX_PATH);
    if (!cwcPath || cwcPath >= MAX_PATH)
    {
        ErrorMsgLastErr("LoadAppDll: GetModuleFileNameW failed");
        return NULL;
    }

    /* Drop the image filename. */
    do
    {
        cwcPath--;
        if (RTPATH_IS_SEP(wszPath[cwcPath]))
        {
            cwcPath++;
            wszPath[cwcPath] = '\0';
            break;
        }
    } while (cwcPath > 0);

    if (!cwcPath) /* This should be impossible */
    {
        ErrorMsg("LoadAppDll: GetModuleFileNameW returned no path!");
        return NULL;
    }

    /* Append the dll name if we can. */
    size_t const cwcName = RTUtf16Len(pwszName);
    if (cwcPath + cwcName >= RT_ELEMENTS(wszPath))
    {
        ErrorMsgSWSWS("LoadAppDll: Path '", wszPath, "' too long when adding '", pwszName, "'");
        return NULL;
    }
    memcpy(&wszPath[cwcPath], pwszName, (cwcName + 1) * sizeof(wszPath[0]));

    /* Try load the module.  We will try restrict the library search to the
       system32 directory if supported by the OS. Older OSes doesn't support
       this, so we fall back on full search in that case. */
    HMODULE hMod = LoadLibraryExW(wszPath, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hMod == NULL && GetLastError() == ERROR_INVALID_PARAMETER)
        hMod = LoadLibraryExW(wszPath, NULL, 0);
    if (!hMod)
        ErrorMsgLastErrSWS("LoadAppDll: LoadLibraryExW failed on '", wszPath, "'");
    return hMod;
}


/**
 * Installs or uninstalls a driver.
 *
 * @return  Exit code (EXIT_OK, EXIT_FAIL)
 * @param   fInstall            Set to @c true for installation, and @c false
 *                              for uninstallation.
 * @param   pwszDriverPath      Path to the driver's .INF file.
 * @param   fSilent             Set to @c true for silent installation.
 * @param   pwszLogFile         Pointer to full qualified path to log file to be
 *                              written during installation. Optional.
 */
static int VBoxInstallDriver(const BOOL fInstall, const wchar_t *pwszDriverPath, bool fSilent, const wchar_t *pwszLogFile)
{
    /*
     * Windows 2000 and later.
     */
    OSVERSIONINFO VerInfo = { sizeof(VerInfo) };
    GetVersionEx(&VerInfo);
    if (VerInfo.dwPlatformId != VER_PLATFORM_WIN32_NT)
        return ErrorMsg("Platform not supported for driver (un)installation!");
    if (VerInfo.dwMajorVersion < 5)
        return ErrorMsg("Platform too old to be supported for driver (un)installation!");

    /*
     * Get the full path to the INF file.
     */
    wchar_t wszFullDriverInf[MAX_PATH];
    if (GetFullPathNameW(pwszDriverPath, MAX_PATH, wszFullDriverInf, NULL) ==0 )
        return ErrorMsgLastErrSWS("GetFullPathNameW failed on '", pwszDriverPath, "'");

    /*
     * Load DIFxAPI.dll from our application directory and resolve the symbols we need
     * from it.  We always resolve all for reasons of simplicity and general paranoia.
     */
    HMODULE hModDifXApi = LoadAppDll(L"DIFxAPI.dll");
    if (!hModDifXApi)
        return EXIT_FAIL;

    static struct { FARPROC *ppfn; const char *pszName; } const s_aFunctions[] =
    {
        { (FARPROC *)&g_pfnDriverPackageInstall,   "DriverPackageInstallW" },
        { (FARPROC *)&g_pfnDriverPackageUninstall, "DriverPackageUninstallW" },
        { (FARPROC *)&g_pfnDIFXAPISetLogCallback,  "DIFXAPISetLogCallbackW" },
    };
    for (size_t i = 0; i < RT_ELEMENTS(s_aFunctions); i++)
    {
        FARPROC pfn = *s_aFunctions[i].ppfn = GetProcAddress(hModDifXApi, s_aFunctions[i].pszName);
        if (!pfn)
            return ErrorMsgLastErrSSS("Failed to find symbol '", s_aFunctions[i].pszName, "' in DIFxAPI.dll");
    }

    /*
     * Try open the log file and register a logger callback with DIFx.
     * Failures here are non-fatal.
     */
    HANDLE hLogFile = INVALID_HANDLE_VALUE;
    if (pwszLogFile)
    {
        hLogFile = CreateFileW(pwszLogFile, FILE_GENERIC_WRITE & ~FILE_WRITE_DATA /* append mode */, FILE_SHARE_READ,
                               NULL /*pSecAttr*/, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL /*hTemplateFile*/);
        if (hLogFile == INVALID_HANDLE_VALUE)
            ErrorMsgLastErrSWS("Failed to open/create log file '", pwszLogFile, "'");
        g_pfnDIFXAPISetLogCallback(VBoxDIFxLogCallback, (void *)hLogFile);
    }

    PrintStr(fInstall ? "Installing driver ...\r\n" : "Uninstalling driver ...\r\n");
    PrintSWS("INF-File: '", wszFullDriverInf, "'\r\n");

    INSTALLERINFO InstInfo =
    {
        L"{7d2c708d-c202-40ab-b3e8-de21da1dc629}", /* Our GUID for representing this installation tool. */
        L"VirtualBox Guest Additions Install Helper",
        L"VirtualBox Guest Additions", /** @todo Add version! */
        L"Oracle Corporation"
    };

    /* Flags */
    DWORD dwFlags = DRIVER_PACKAGE_FORCE;
    if (!fInstall)
        dwFlags |= DRIVER_PACKAGE_DELETE_FILES;
    if (VerInfo.dwMajorVersion < 6 && fInstall)
    {
        PrintStr("Using legacy mode for install ...\r\n");
        dwFlags |= DRIVER_PACKAGE_LEGACY_MODE;
    }
    if (fSilent)
    {
        /* Don't add DRIVER_PACKAGE_SILENT to dwFlags here, otherwise the
           installation will fail because we don't have WHQL certified drivers.
           See CERT_E_WRONG_USAGE on MSDN for more information. */
        PrintStr("Installation is silent ...\r\n");
    }

    /* Do the install/uninstall: */
    BOOL  fReboot = FALSE;
    DWORD dwErr;
    if (fInstall)
        dwErr = g_pfnDriverPackageInstall(wszFullDriverInf, dwFlags, &InstInfo, &fReboot);
    else
        dwErr = g_pfnDriverPackageUninstall(wszFullDriverInf, dwFlags, &InstInfo, &fReboot);

    /*
     * Report error
     */
    int         rcExit = EXIT_FAIL;
    const char *psz    = NULL;
    switch (dwErr)
    {
        case ERROR_SUCCESS:
            rcExit = EXIT_OK;
            break;

        case CRYPT_E_FILE_ERROR:
            psz = "The catalog file for the specified driver package was not found!";
            break;
        case ERROR_ACCESS_DENIED:
            psz = fInstall ? "Caller is not in Administrators group to install this driver package!"
                           : "Caller is not in Administrators group to uninstall this driver package!";
            break;
        case ERROR_BAD_ENVIRONMENT:
            psz = "The current Microsoft Windows version does not support this operation!";
            break;
        case ERROR_CANT_ACCESS_FILE:
            psz = "The driver package files could not be accessed!";
            break;
        case ERROR_DEPENDENT_APPLICATIONS_EXIST:
            psz = "DriverPackageUninstall removed an association between the driver package and the specified application but the function did not uninstall the driver package because other applications are associated with the driver package!";
            break;
        case ERROR_DRIVER_PACKAGE_NOT_IN_STORE:
            psz = fInstall ? "There is no INF file in the DIFx driver store that corresponds to the INF file being installed!"
                           : "There is no INF file in the DIFx driver store that corresponds to the INF file being uninstalled!";
            break;
        case ERROR_FILE_NOT_FOUND:
            psz = "INF-file not found!";
            break;
        case ERROR_IN_WOW64:
            psz = "The calling application is a 32-bit application attempting to execute in a 64-bit environment, which is not allowed!";
            break;
        case ERROR_INVALID_FLAGS:
            psz = "The flags specified are invalid!";
            break;
        case ERROR_INSTALL_FAILURE:
            psz = fInstall ? "The install operation failed! Consult the Setup API logs for more information."
                           : "The uninstall operation failed! Consult the Setup API logs for more information.";
            break;
        case ERROR_NO_MORE_ITEMS:
            psz = "The function found a match for the HardwareId value, but the specified driver was not a better match than the current driver and the caller did not specify the INSTALLFLAG_FORCE flag!";
            break;
        case ERROR_NO_DRIVER_SELECTED:
            psz = "No driver in .INF-file selected!";
            break;
        case ERROR_SECTION_NOT_FOUND:
            psz = "Section in .INF-file was not found!";
            break;
        case ERROR_SHARING_VIOLATION:
            psz = "A component of the driver package in the DIFx driver store is locked by a thread or process!";
            break;

        /*
         * !    sig:           Verifying file against specific Authenticode(tm) catalog failed! (0x800b0109)
         * !    sig:           Error 0x800b0109: A certificate chain processed, but terminated in a root certificate which is not trusted by the trust provider.
         * !!!  sto:           No error message will be displayed as client is running in non-interactive mode.
         * !!!  ndv:           Driver package failed signature validation. Error = 0xE0000247
         */
        case ERROR_DRIVER_STORE_ADD_FAILED:
            psz = "Adding driver to the driver store failed!!";
            break;
        case ERROR_UNSUPPORTED_TYPE:
            psz = "The driver package type is not supported of INF-file!";
            break;
        case ERROR_NO_SUCH_DEVINST:
            psz = "The driver package was installed but no matching devices found in the device tree (ERROR_NO_SUCH_DEVINST).";
            /* GA installer should ignore this error code and continue */
            rcExit = EXIT_OK;
            break;

        default:
        {
            /* Try error lookup with GetErrorMsg(). */
            ErrorMsgSWS(fInstall ? "Installation of '" : "Uninstallation of '", wszFullDriverInf, "' failed!");
            ErrorMsgBegin("dwErr=");
            ErrorMsgErrVal(dwErr, false);
            WCHAR wszErrMsg[1024];
            if (GetErrorMsg(dwErr, wszErrMsg, RT_ELEMENTS(wszErrMsg)))
            {
                ErrorMsgStr(": ");
                ErrorMsgWStr(wszErrMsg);
            }
            ErrorMsgEnd(NULL);
            break;
        }
    }
    if (psz)
    {
        ErrorMsgSWS(fInstall ? "Installation of '" : "Uninstallation of '", wszFullDriverInf, "' failed!");
        ErrorMsgBegin("dwErr=");
        ErrorMsgErrVal(dwErr, false);
        ErrorMsgStr(": ");
        ErrorMsgEnd(psz);
    }

    /* Close the log file. */
    if (pwszLogFile)
    {
        g_pfnDIFXAPISetLogCallback(NULL, NULL);
        if (hLogFile != INVALID_HANDLE_VALUE)
            CloseHandle(hLogFile);
    }
    if (rcExit == EXIT_OK)
    {
        PrintStr(fInstall ? "Driver was installed successfully!\r\n"
                          : "Driver was uninstalled successfully!\r\n");
        if (fReboot)
        {
            PrintStr(fInstall ? "A reboot is needed to complete the driver installation!\r\n"
                              : "A reboot is needed to complete the driver uninstallation!\r\n");
            /** @todo r=bird: We don't set EXIT_REBOOT here for some reason... The
             *        ExecuteInf didn't use EXIT_REBOOT either untill the no-CRT rewrite,
             *        so perhaps the EXIT_REBOOT stuff can be removed? */
        }
    }

    return rcExit;
}


/** Handles 'driver install'. */
static int handleDriverInstall(unsigned cArgs, wchar_t **papwszArgs)
{
    return VBoxInstallDriver(true /*fInstall*/, papwszArgs[1], false /*fSilent*/,
                             cArgs > 1 && papwszArgs[1][0] ? papwszArgs[1] : NULL /* pwszLogFile*/);
}


/** Handles 'driver uninstall'. */
static int handleDriverUninstall(unsigned cArgs, wchar_t **papwszArgs)
{
    return VBoxInstallDriver(false /*fInstall*/, papwszArgs[1], false /*fSilent*/,
                             cArgs > 1 && papwszArgs[1][0] ? papwszArgs[1] : NULL /* pwszLogFile*/);
}


/**
 * Implementes PSP_FILE_CALLBACK_W, used by ExecuteInfFile.
 */
static UINT CALLBACK
vboxDrvInstExecuteInfFileCallback(PVOID pvContext, UINT uNotification, UINT_PTR uParam1, UINT_PTR uParam2) RT_NOTHROW_DEF
{
#ifdef DEBUG
    PrintSXS("Got installation notification ", uNotification, "\r\n");
#endif

    switch (uNotification)
    {
        case SPFILENOTIFY_NEEDMEDIA:
            PrintStr("Requesting installation media ...\r\n");
            break;

        case SPFILENOTIFY_STARTCOPY:
            PrintStr("Copying driver files to destination ...\r\n");
            break;

        case SPFILENOTIFY_TARGETNEWER:
        case SPFILENOTIFY_TARGETEXISTS:
            return TRUE;
    }

    return SetupDefaultQueueCallbackW(pvContext, uNotification, uParam1, uParam2);
}


/**
 * Executes a specific .INF section to install/uninstall drivers and/or
 * services.
 *
 * @return  Exit code (EXIT_OK, EXIT_FAIL, EXIT_REBOOT)
 * @param   pwszSection Section to execute; usually it's L"DefaultInstall".
 * @param   pwszInf     Path of the .INF file to use.
 */
static int ExecuteInfFile(const wchar_t *pwszSection, const wchar_t *pwszInf)
{
    PrintSWSWS("Installing from INF-File: '", pwszInf, "', Section: '", pwszSection, "' ...\r\n");

    UINT uErrorLine = 0;
    HINF hInf = SetupOpenInfFileW(pwszInf, NULL, INF_STYLE_WIN4, &uErrorLine);
    if (hInf == INVALID_HANDLE_VALUE)
        return ErrorMsgLastErrSWSRSUS("SetupOpenInfFileW failed to open '", pwszInf, "' ", ", error line ", uErrorLine, NULL);

    int   rcExit  = EXIT_FAIL;
    PVOID pvQueue = SetupInitDefaultQueueCallback(NULL);
    if (pvQueue)
    {
        if (SetupInstallFromInfSectionW(NULL /*hWndOwner*/, hInf, pwszSection, SPINST_ALL, HKEY_LOCAL_MACHINE,
                                        NULL /*pwszSrcRootPath*/, SP_COPY_NEWER_OR_SAME | SP_COPY_NOSKIP,
                                        vboxDrvInstExecuteInfFileCallback, pvQueue, NULL /*hDevInfoSet*/, NULL /*pDevInfoData*/))
        {
            PrintStr("File installation stage successful\r\n");

            if (SetupInstallServicesFromInfSectionW(hInf, L"DefaultInstall.Services", 0 /* Flags */))
            {
                PrintStr("Service installation stage successful. Installation completed.\r\n");
                rcExit = EXIT_OK;
            }
            else if (GetLastError() == ERROR_SUCCESS_REBOOT_REQUIRED)
            {
                PrintStr("A reboot is required to complete the installation\r\n");
                rcExit = EXIT_REBOOT;
            }
            else
                ErrorMsgLastErrSWSWS("SetupInstallServicesFromInfSectionW failed on '", pwszSection, "' in '", pwszInf, "'");
        }
        SetupTermDefaultQueueCallback(pvQueue);
    }
    else
        ErrorMsgLastErr("SetupInitDefaultQueueCallback failed");
    SetupCloseInfFile(hInf);
    return rcExit;
}


/** Handles 'driver executeinf'. */
static int handleDriverExecuteInf(unsigned cArgs, wchar_t **papwszArgs)
{
    RT_NOREF(cArgs);
    return ExecuteInfFile(L"DefaultInstall", papwszArgs[0]);
}


/**
 * Checks if a string is a substring of another one.
 *
 * Used by the RegistryAddStringToMultiSZ & RegistryRemoveStringToMultiSZ
 * routines.
 */
static bool IsSubStringOf(wchar_t volatile const *pwszStr, size_t cwcStr, wchar_t const *pwszSubStr, size_t cwcSubStr)
{
    if (cwcStr >= cwcSubStr && cwcSubStr > 0)
    {
        wchar_t const wcFirst = *pwszSubStr;
        cwcStr -= cwcSubStr;
        do
        {
            /* Could've used wmemchr here, but it isn't implemented in noCRT yet. */
            if (   *pwszStr == wcFirst
                && memcmp((void const *)pwszStr, pwszSubStr, cwcSubStr * sizeof(wchar_t)) == 0)
                return true;
            pwszStr++;
        } while (cwcStr-- > 0);
    }
    return false;
}


/**
 * Adds a string entry to a MULTI_SZ registry list.
 *
 * @return  Exit code (EXIT_OK, EXIT_FAIL)
 * @param   pwszSubKey      Sub key containing the list.
 * @param   pwszValueName   The actual key name of the list.
 * @param   pwszItemToAdd   The item to add to the list.
 * @param   uPosition       Position (zero-based) of where to add the
 *                          value to the list.
 */
static int RegistryAddStringToMultiSZ(const wchar_t *pwszSubKey, const wchar_t *pwszValueName,
                                      const wchar_t *pwszItemToAdd, uint32_t uPosition)
{
    size_t const cwcItemToAdd = RTUtf16Len(pwszItemToAdd);
    size_t const cbItemToAdd  = (cwcItemToAdd + 1) * sizeof(wchar_t);
#ifdef DEBUG
    PrintSWSWSWSXS("AddStringToMultiSZ: Adding MULTI_SZ item '", pwszItemToAdd,
                   "' to HKLM/'", pwszSubKey, "'/'", pwszValueName, "' at position ", uPosition, "\r\n");
#endif

    /*
     * Open/create the key.
     */
    HKEY    hKey   = NULL;
    DWORD   dwDisp = 0;
    LSTATUS lrc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, pwszSubKey, 0 /*Reserved*/, NULL /*pClass*/, REG_OPTION_NON_VOLATILE,
                                 KEY_READ | KEY_WRITE, NULL /*pSecAttr*/, &hKey, &dwDisp);
    if (lrc != ERROR_SUCCESS)
        return ErrorMsgLStatusSWSRS("RegistryAddStringToList: RegCreateKeyEx HKLM/'", pwszSubKey, "' failed: ", lrc, NULL);

    /*
     * Query the current value, first query just gets the buffer size the 2nd does the actual query.
     * We make sure the buffer is large enough to contain the new item we're supposed to add.
     */
    int   rcExit  = EXIT_FAIL;
    PBYTE pbBuf   = NULL;
    DWORD cbValue = 0;
    DWORD dwType  = 0;
    lrc = RegQueryValueEx(hKey, pwszValueName, NULL, &dwType, NULL, &cbValue);
    if (lrc == ERROR_SUCCESS || lrc == ERROR_MORE_DATA)
    {
        cbValue = cbValue + _1K - sizeof(wchar_t)*2; /* 1KB of paranoia fudge, even if we ASSUME no races. */
        pbBuf = (PBYTE)RTMemAllocZ(cbValue + sizeof(wchar_t)*2 /* Two extra wchar_t's for proper zero termination. */
                                   + cbItemToAdd);
        if (!pbBuf)
            lrc = ERROR_OUTOFMEMORY;
        lrc = RegQueryValueEx(hKey, pwszValueName, NULL, &dwType, pbBuf, &cbValue);
    }
    if (lrc == ERROR_FILE_NOT_FOUND)
    {
        PrintStr("RegistryAddStringToList: Value not found, creating a new one...\r\n");
        pbBuf = (PBYTE)RTMemAllocZ(cbItemToAdd + sizeof(wchar_t)*8);
        if (pbBuf)
        {
            cbValue = sizeof(wchar_t);
            dwType  = REG_MULTI_SZ;
            lrc     = ERROR_SUCCESS;
        }
        else
            lrc     = ERROR_OUTOFMEMORY;
    }
    if (   lrc == ERROR_SUCCESS
        && dwType == REG_MULTI_SZ)
    {
#ifdef DEBUG
        PrintSXS("RegistryAddStringToList: Current value length: ", cbValue, "\r\n");
#endif

        /*
         * Scan the strings in the buffer, inserting the new item and removing any
         * existing duplicates.  We do this in place.
         *
         * We have made sure above that the buffer is both properly zero terminated
         * and large enough to contain the new item, so we need do no buffer size
         * checking here.
         */
        wchar_t volatile *pwszSrc   = (wchar_t volatile *)pbBuf;
        wchar_t volatile *pwszDst   = (wchar_t volatile *)pbBuf;
        size_t            cbLeft    = cbValue;
        for (uint32_t uCurPos = 0; ; uCurPos++)
        {
            size_t const cwcSrc  = RTUtf16Len((wchar_t const *)pwszSrc);
            size_t const cbSrc   = (cwcSrc + 1) * sizeof(wchar_t);
            bool const   fTheEnd = !cwcSrc && cbSrc >= cbLeft;

            /* Insert the item if we're in the right position now, or if we're
               at the last string and still haven't reached it. */
            if (uCurPos == uPosition || (fTheEnd && uCurPos < uPosition))
            {
                pwszSrc = (wchar_t volatile *)memmove((PBYTE)pwszSrc + cbItemToAdd, (wchar_t const *)pwszSrc, cbLeft);
                memcpy((void *)pwszDst, pwszItemToAdd, cbItemToAdd);
                pwszDst += cwcItemToAdd + 1;
                uCurPos++;
            }
            if (fTheEnd)
                break;

            /* We do not add empty strings nor strings matching the one we're adding. */
            if (!cwcSrc || IsSubStringOf(pwszSrc, cwcSrc, pwszItemToAdd, cwcItemToAdd))
                uCurPos--;
            else
            {
                if (pwszDst != pwszSrc)
                    memmove((void *)pwszDst, (void const *)pwszSrc, cbSrc);
                pwszDst += cwcSrc + 1;
            }
            pwszSrc += cwcSrc + 1;
            cbLeft  -= cbSrc;
        }
        *pwszDst = '\0';
        DWORD const cbNewValue = (DWORD)((PBYTE)(pwszDst + 1) - pbBuf);
#ifdef DEBUG
        PrintSXS("RegistryAddStringToList: New value length: ", cbNewValue, "\r\n");
#endif

        /*
         * Always write the value since we cannot tell whether it changed or
         * not without adding a bunch extra code above.
         */
        lrc = RegSetValueExW(hKey, pwszValueName, 0, REG_MULTI_SZ, pbBuf, cbNewValue);
        if (lrc == ERROR_SUCCESS)
        {
#ifdef DEBUG
            PrintSWSWS("RegistryAddStringToList: The item '", pwszItemToAdd, "' was added successfully to '",
                       pwszValueName, "'.\r\n");
#endif
            rcExit = EXIT_OK;
        }
        else
            ErrorMsgLStatusSWSWSRS("RegistryAddStringToList: RegSetValueExW HKLM/'",
                                   pwszSubKey, "'/'", pwszValueName, "' failed: ", lrc, NULL);
    }
    else if (lrc != ERROR_SUCCESS)
        ErrorMsgLStatusSWSWSRS("RemoveStringFromMultiSZ: RegQueryValueEx HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "' failed: ", lrc, NULL);
    else
        ErrorMsgLStatusSWSWSRS("RemoveStringFromMultiSZ: Unexpected value type for HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "': ", (LSTATUS)dwType, ", expected REG_SZ (1)");
    return rcExit;
}


/** Handles 'registry addmultisz'. */
static int handleRegistryAddMultiSz(unsigned cArgs, wchar_t **papwszArgs)
{
    RT_NOREF(cArgs);

    uint32_t uPosition;
    if (!ArgToUInt32Full(papwszArgs[3], "position", &uPosition))
        return EXIT_USAGE;

    return RegistryAddStringToMultiSZ(papwszArgs[0], papwszArgs[1], papwszArgs[2], uPosition);
}


/**
 * Removes a item from a MULTI_SZ registry list.
 *
 * @return  Exit code (EXIT_OK, EXIT_FAIL)
 * @param   pwszSubKey          Sub key containing the list.
 * @param   pwszValueName       The actual key name of the list.
 * @param   pwszItemToRemove    The item to remove from the list.  Actually, we
 *                              only do a substring match on this, so any item
 *                              containing this string will be removed.
 */
static int RegistryRemoveStringFromMultiSZ(const wchar_t *pwszSubKey, const wchar_t *pwszValueName,
                                           const wchar_t *pwszItemToRemove)
{
#ifdef DEBUG
    PrintSWSWSWS("RemoveStringFromMultiSZ: Removing MULTI_SZ string '", pwszItemToRemove,
                 "' from HKLM/'", pwszSubKey, "'/'", pwszValueName, "'\r\n");
#endif

    /*
     * Open the specified key.
     */
    HKEY hKey = NULL;
    LSTATUS lrc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, pwszSubKey, 0 /*dwOptions*/, KEY_READ | KEY_WRITE, &hKey);
    if (lrc != ERROR_SUCCESS)
        return ErrorMsgLStatusSWSRS("RemoveStringFromMultiSZ: RegOpenKeyExW HKLM/'", pwszSubKey, "' failed: ", lrc, NULL);

    /*
     * Query the current value, first query just gets the buffer size the 2nd does the actual query.
     */
    int   rcExit  = EXIT_FAIL;
    PBYTE pbBuf   = NULL;
    DWORD cbValue = 0;
    DWORD dwType  = 0;
    lrc = RegQueryValueEx(hKey, pwszValueName, NULL, &dwType, NULL, &cbValue);
    if (lrc == ERROR_SUCCESS || lrc  == ERROR_MORE_DATA)
    {
        cbValue = cbValue + _1K - sizeof(wchar_t)*2; /* 1KB of paranoia fudge, even if we ASSUME no races. */
        pbBuf = (PBYTE)RTMemAllocZ(cbValue + sizeof(wchar_t)*2); /* Two extra wchar_t's for proper zero termination, see docs. */
        if (!pbBuf)
            lrc = ERROR_OUTOFMEMORY;
        lrc = RegQueryValueEx(hKey, pwszValueName, NULL, &dwType, pbBuf, &cbValue);
    }
    if (   lrc == ERROR_SUCCESS
        && dwType == REG_MULTI_SZ)
    {
#ifdef DEBUG
        PrintSXS("RemoveStringFromMultiSZ: Current value length: ", cbValue, "\r\n");
#endif
        /*
         * Scan the buffer and remove all strings containing the pwszItemToRemove
         * as a substring.
         */
        size_t const      cwcValueToRemove = RTUtf16Len(pwszItemToRemove);
        wchar_t volatile *pwszSrc          = (wchar_t volatile *)pbBuf;
        wchar_t volatile *pwszDst          = (wchar_t volatile *)pbBuf;
        size_t            cbLeft           = cbValue;
        for (;;)
        {
            /* Find the length for the current string.  We can safely use RTUtf16Len
               here because of a zero terminated buffer with two extra terminator chars. */
            size_t const cwcSrc = RTUtf16Len((wchar_t const *)pwszSrc);
            size_t const cbSrc  = (cwcSrc + 1) * sizeof(wchar_t);
            if (!IsSubStringOf(pwszSrc, cwcSrc, pwszItemToRemove, cwcValueToRemove))
            {
                if (pwszDst != pwszSrc)
                    memmove((void *)pwszDst, (void const *)pwszSrc, cbSrc);
                pwszDst += cwcSrc + 1;
            }

            /* Advance. */
            if (cbLeft < cbSrc)
                break;
            cbLeft  -= cbSrc;
            pwszSrc += cwcSrc + 1;
        }
        *pwszDst = '\0';
        DWORD const cbNewValue = (DWORD)((PBYTE)(pwszDst + 1) - pbBuf);
#ifdef DEBUG
        PrintSXS("RemoveStringFromMultiSZ: New value length: ", cbNewValue, "\r\n");
#endif

        /*
         * Update the value if we made any change.
         */
        if (cbNewValue == cbValue)
        {
#ifdef DEBUG
            PrintSWSWS("RemoveStringFromMultiSZ: The item '", pwszItemToRemove, "' was not part of '",
                       pwszValueName, "', so nothing needed doing.\r\n");
#endif
            rcExit = EXIT_OK;
        }
        else
        {
            lrc = RegSetValueExW(hKey, pwszValueName, 0, REG_MULTI_SZ, pbBuf, cbNewValue);
            if (lrc == ERROR_SUCCESS)
            {
#ifdef DEBUG
                PrintSWSWS("RemoveStringFromMultiSZ: The item '", pwszItemToRemove, "' was removed successfully from '",
                           pwszValueName, "'.\r\n");
#endif
                rcExit = EXIT_OK;
            }
            else
                ErrorMsgLStatusSWSWSRS("RegistryAddStringToList: RegSetValueExW HKLM/'",
                                       pwszSubKey, "'/'", pwszValueName, "' failed: ", lrc, NULL);
        }
    }
    else if (lrc == ERROR_FILE_NOT_FOUND)
    {
#ifdef DEBUG
        PrintStr("RemoveStringFromMultiSZ: value not present in registry\r\n");
#endif
        rcExit = EXIT_OK;
    }
    else if (lrc != ERROR_SUCCESS)
        ErrorMsgLStatusSWSWSRS("RemoveStringFromMultiSZ: RegQueryValueEx HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "' failed: ", lrc, NULL);
    else
        ErrorMsgLStatusSWSWSRS("RemoveStringFromMultiSZ: Unexpected value type for HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "': ", (LSTATUS)dwType, ", expected REG_SZ (1)");
    RegCloseKey(hKey);
    RTMemFree(pbBuf);
    return rcExit;
}


/** Handles 'registry delmultisz'. */
static int handleRegistryDelMultiSz(unsigned cArgs, wchar_t **papwszArgs)
{
    RT_NOREF(cArgs);
    return RegistryRemoveStringFromMultiSZ(papwszArgs[0], papwszArgs[1], papwszArgs[2]);
}


/**
 * Compare the current list item with the one to add/remove.
 *
 * Used by RegistryAddStringToList and RegistryRemoveStringFromList.
 */
static bool IsStringListItemMatch(wchar_t volatile *pwszItem1, size_t cwcItem1,
                                  wchar_t const *pwszItem2, size_t cwcItem2)
{
    if (cwcItem1 == cwcItem2)
    {
#if 0 /* 94720 bytes */
        if (RTUtf16NICmp((wchar_t const *)pwszItem1, pwszItem2, cwcItem1) == 0)
            return true;
#else /* vs 62464 bytes */
        /* Temporarily zero termination of item 1 as it's easier, and therefore
           safer, to use lstrcmpiW than CompareStringW or CompareStringExW.  The
           latter is Vista and later, the former has a big fat warning on it.  */
        wchar_t const wcEnd = pwszItem1[cwcItem1];
        int const iDiff = lstrcmpiW((wchar_t const *)pwszItem1, pwszItem2);
        pwszItem1[cwcItem1] = wcEnd;
        return iDiff == 0;
#endif
    }
    return false;
}


/**
 * Adds an item to a comma separated registry string list (REG_SZ).
 *
 * Only operates in HKLM for now, if needed it can be extended later for use
 * with other hives.
 *
 * @return  Exit code (EXIT_OK, EXIT_FAIL)
 * @param   pwszSubKey          Sub key containing the list value.
 * @param   pwszValueName       The name of the value holding the list.
 * @param   pwszItemToAdd       The value to add to the list.
 * @param   uPosition           Position (zero-based) of where to insert the
 *                              value into the list.
 * @param   fFlags              VBOX_REG_STRINGLIST_ALLOW_DUPLICATES or 0.
 */
static int RegistryAddStringToList(const wchar_t *pwszSubKey, const wchar_t *pwszValueName, const wchar_t *pwszItemToAdd,
                                   uint32_t uPosition, uint32_t fFlags)
{
    /* Overflow precaution - see comment below. */
    size_t const cwcItemToAdd = RTUtf16Len(pwszItemToAdd);
    if (cwcItemToAdd >= 256 /* see wszNewValue size below */)
        return ErrorMsg("RegistryAddStringToList: The value to add is too long! Max 256 chars.");

    /*
     * Open/create the key.
     */
    HKEY    hKey   = NULL;
    DWORD   dwDisp = 0;
    LSTATUS lrc = RegCreateKeyEx(HKEY_LOCAL_MACHINE, pwszSubKey, 0 /*Reserved*/, NULL /*pClass*/, REG_OPTION_NON_VOLATILE,
                                 KEY_READ | KEY_WRITE, NULL /*pSecAttr*/, &hKey, &dwDisp);
    if (lrc != ERROR_SUCCESS)
        return ErrorMsgLStatusSWSRS("RegistryAddStringToList: RegCreateKeyEx HKLM/'", pwszSubKey, "' failed: ", lrc, NULL);

    /*
     * Query the current value.
     */
    int     rcExit         = EXIT_FAIL;
    wchar_t wszValue[1024] = { 0 };
    DWORD   cbValue        = sizeof(wszValue) - sizeof(wchar_t);
    DWORD   dwType         = 0;
    lrc = RegQueryValueEx(hKey, pwszValueName, NULL /*pReserved*/, &dwType, (LPBYTE)wszValue, &cbValue);
    if (lrc == ERROR_FILE_NOT_FOUND)
    {
        PrintStr("RegistryAddStringToList: Value not found, creating a new one...\r\n");
        wszValue[0] = '\0';
        cbValue     = sizeof(wchar_t);
        dwType      = REG_SZ;
        lrc = ERROR_SUCCESS;
    }
    if (lrc == ERROR_SUCCESS && dwType == REG_SZ)
    {
#ifdef DEBUG
        PrintSWS("RegistryAddStringToList: Value string: '", wszValue, "'\r\n");
#endif

        /*
         * Scan the list and make a new copy of it with the new item added
         * in the specified place.
         *
         * Having checked that what we're adding isn't more than 256 + 1 chars long
         * above, we can avoid tedious overflow checking here the simple expedient of
         * using an output buffer that's at least 256 + 1 chars bigger than the source.
         */
        wchar_t  wszNewValue[RT_ELEMENTS(wszValue) + 256 + 4] = { 0 };
        wchar_t *pwszDst = wszNewValue;
        wchar_t *pwszSrc = wszValue;
        unsigned uCurPos = 0;
        for (;;)
        {
            /* Skip leading commas: */
            wchar_t wc            = *pwszSrc;
            bool    fLeadingComma = wc == ',';
            if (fLeadingComma)
                do
                    wc = *++pwszSrc;
                while (wc == ',');

            /* Insert the new item if we're at the right position or have reached
               the end of the list and have yet done so. */
            if (uCurPos == uPosition || (!wc && uCurPos < uPosition))
            {
                if (fLeadingComma)
                    *pwszDst++ = ',';
                memcpy(pwszDst, pwszItemToAdd, cwcItemToAdd * sizeof(wchar_t));
                pwszDst += cwcItemToAdd;
                fLeadingComma = true;
            }

            /* Get out of the loop if we're at the end of the input. */
            if (!wc)
                break; /* don't preserve trailing commas? Old code didn't (see strtok_r code). */

            /* Start of a new 'value', so, find the end of it. */
            wchar_t *pwszSrcEnd = pwszSrc + 1;
            do
                wc = *++pwszSrcEnd;
            while (wc != '\0' && wc != ',');
            size_t const cwcItem = (size_t)(pwszSrcEnd - pwszSrc);

            /* If it matches pwszItemToRemove and the VBOX_REG_STRINGLIST_ALLOW_DUPLICATES
               wasn't specified, we'll skip this value. */
            ASMCompilerBarrier(); /* Paranoia ^ 2*/
            if (   !(fFlags & VBOX_REG_STRINGLIST_ALLOW_DUPLICATES)
                && IsStringListItemMatch(pwszSrc, cwcItem, pwszItemToAdd, cwcItemToAdd))
                pwszSrc = pwszSrcEnd;
            else
            {
                if (fLeadingComma)
                    *pwszDst++ = ',';
                memmove(pwszDst, pwszSrc, cwcItem * sizeof(*pwszDst));
                pwszDst += cwcItem;
                pwszSrc  = pwszSrcEnd;
                ASMCompilerBarrier(); /* Paranoia ^ 3 */
            }

            /* pwszSrc should not point at a comma or a zero terminator. */
        }
        *pwszDst = '\0';
        DWORD const cbNewValue = (DWORD)((pwszDst + 1 - &wszNewValue[0]) * sizeof(wchar_t));

#ifdef DEBUG
        PrintSWS("RegistryAddStringToList: New value:    '", wszNewValue, "'\r\n");
#endif

        /*
         * Add the value if changed.
         */
        if (   cbNewValue == cbValue
            && memcmp(wszNewValue, wszValue, cbNewValue) == 0)
            rcExit = EXIT_OK;
        else
        {
            lrc = RegSetValueExW(hKey, pwszValueName, 0, REG_SZ, (LPBYTE)wszNewValue, cbNewValue);
            if (lrc == ERROR_SUCCESS)
                rcExit = EXIT_OK;
            else
                ErrorMsgLStatusSWSWSWSRS("RegistryAddStringToList: RegSetValueExW HKLM/'",
                                         pwszSubKey, "'/'", pwszValueName, "' = '", wszNewValue, "' failed: ", lrc, NULL);
        }
    }
    else if (lrc != ERROR_SUCCESS)
        ErrorMsgLStatusSWSWSRS("RegistryAddStringToList: RegQueryValueEx HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "' failed: ", lrc, NULL);
    else
        ErrorMsgLStatusSWSWSRS("RegistryAddStringToList: Unexpected value type for HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "': ", (LSTATUS)dwType, ", expected REG_SZ (1)");

    RegCloseKey(hKey);
    return rcExit;
}


/**
 * Handles 'netprovider add'.
 */
static int handleNetProviderAdd(unsigned cArgs, wchar_t **papwszArgs)
{
    const wchar_t * const pwszProvider = papwszArgs[0];
    wchar_t const * const pwszPosition = cArgs > 1 ? papwszArgs[1] : L"0";
    uint32_t              uPosition    = 0;
    if (cArgs > 1 && !ArgToUInt32Full(pwszPosition, "position", &uPosition))
        return EXIT_USAGE;

    PrintSWSWS("Adding network provider '", pwszProvider, "' (Position = ", pwszPosition, ") ...\r\n");
    int rcExit = RegistryAddStringToList(L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order",
                                         L"ProviderOrder",
                                         pwszProvider, uPosition, VBOX_REG_STRINGLIST_NONE);
    if (rcExit == EXIT_OK)
        PrintStr("Network provider successfully added!\r\n");

    return rcExit;
}


/**
 * Removes an item from a comma separated registry string (REG_SZ).
 *
 * Only operates in HKLM for now, if needed it can be extended later for use
 * with other hives.
 *
 * @return  Exit code (EXIT_OK, EXIT_FAIL)
 * @param   pwszSubKey          Subkey containing the list value.
 * @param   pwszValueName       The value name.
 * @param   pwszItemToRemove    The item to remove from the list.
 */
static int RegistryRemoveStringFromList(const wchar_t *pwszSubKey, const wchar_t *pwszValueName, const wchar_t *pwszItemToRemove)
{
    /*
     * Open the specified key.
     */
    HKEY hKey = NULL;
    LSTATUS lrc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, pwszSubKey, 0 /*dwOptions*/, KEY_READ | KEY_WRITE, &hKey);
    if (lrc != ERROR_SUCCESS)
        return ErrorMsgLStatusSWSRS("RegistryRemoveStringFromList: RegOpenKeyExW HKLM/'", pwszSubKey, "' failed: ", lrc, NULL);

    /*
     * Query the specified value.
     */
    int     rcExit         = EXIT_FAIL;
    wchar_t wszValue[1296] = { 0 };
    DWORD   cbValue        = sizeof(wszValue) - sizeof(wchar_t);
    DWORD   dwType         = 0;
    lrc = RegQueryValueEx(hKey, pwszValueName, NULL /*pReserved*/, &dwType, (LPBYTE)wszValue, &cbValue);
    if (lrc == ERROR_SUCCESS && dwType == REG_SZ)
    {
#ifdef DEBUG
        PrintSWS("RegistryRemoveStringFromList: Value string: '", wszValue, "'\r\n");
#endif

        /*
         * Scan for item, shifting the query result as we scan.
         */
        size_t const      cwcItemToRemove = RTUtf16Len(pwszItemToRemove);
        wchar_t volatile *pwszSrc         = wszValue;
        wchar_t volatile *pwszDst         = wszValue;
        for (;;)
        {
            /* Skip leading commas: */
            wchar_t    wc            = *pwszSrc;
            bool const fLeadingComma = wc == ',';
            if (fLeadingComma)
                do
                    wc = *++pwszSrc;
                while (wc == ',');
            if (!wc)
                break; /* don't preserve trailing commas? Old code didn't (see strtok_r code). */

            /* Start of a new 'value', so, find the end of it. */
            wchar_t volatile *pwszSrcEnd = pwszSrc + 1;
            do
                wc = *++pwszSrcEnd;
            while (wc != '\0' && wc != ',');
            size_t const cwcItem = (size_t)(pwszSrcEnd - pwszSrc);

            /* If it matches pwszItemToRemove, do not copy it. */
            ASMCompilerBarrier(); /* Paranoia ^ 2 */
            if (IsStringListItemMatch(pwszSrc, cwcItem, pwszItemToRemove, cwcItemToRemove))
                pwszSrc = pwszSrcEnd;
            else
            {
                if (fLeadingComma)
                    *pwszDst++ = ',';
                memmove((void *)pwszDst, (void const *)pwszSrc, cwcItem * sizeof(*pwszDst));
                pwszDst += cwcItem;
                pwszSrc  = pwszSrcEnd;
                ASMCompilerBarrier(); /* paranoia ^ 3 */
            }

            /* pwszSrc should not point at a comma or a zero terminator. */
        }
        *pwszDst = '\0';
#ifdef DEBUG
        PrintSWS("RegistryRemoveStringFromList: New value:    '", wszValue, "'\r\n");
#endif

        /*
         * Save the new value if we've made any changes.
         */
        if (pwszDst == pwszSrc)
            rcExit = EXIT_OK;
        else
        {
            cbValue = (DWORD)((pwszDst + 1 - &wszValue[0]) * sizeof(wchar_t));
            lrc = RegSetValueExW(hKey, pwszValueName, 0, REG_SZ, (LPBYTE)wszValue, cbValue);
            if (lrc == ERROR_SUCCESS)
                rcExit = EXIT_OK;
            else
                ErrorMsgLStatusSWSWSWSRS("RegistryRemoveStringFromList: RegSetValueExW HKLM/'",
                                         pwszSubKey, "'/'", pwszValueName, "' = '", wszValue, "' failed: ", lrc, NULL);
        }
    }
    else if (lrc == ERROR_FILE_NOT_FOUND)
    {
#ifdef DEBUG
        PrintStr("RegistryRemoveStringFromList: Value not present in registry\r\n");
#endif
        rcExit = EXIT_OK;
    }
    else if (lrc != ERROR_SUCCESS)
        ErrorMsgLStatusSWSWSRS("RegistryRemoveStringFromList: RegQueryValueEx HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "' failed: ", lrc, NULL);
    else
        ErrorMsgLStatusSWSWSRS("RegistryRemoveStringFromList: Unexpected value type for HKLM/'",
                               pwszSubKey, "'/'", pwszValueName, "': ", (LSTATUS)dwType, ", expected REG_SZ (1)");
    RegCloseKey(hKey);
    return rcExit;
}


/**
 * Handles 'netprovider remove'.
 */
static int handleNetProviderRemove(unsigned cArgs, wchar_t **papwszArgs)
{
    const wchar_t * const pwszProvider = papwszArgs[0];
    PrintSWS("Removing network provider '", pwszProvider, "' ...\r\n");

    int rcExit = RegistryRemoveStringFromList(L"System\\CurrentControlSet\\Control\\NetworkProvider\\Order",
                                              L"ProviderOrder",
                                              pwszProvider);
    if (rcExit == EXIT_OK)
        PrintStr("Network provider successfully removed!\r\n");

    RT_NOREF(cArgs);
    return rcExit;
}


/**
 * Worker for the 'service create' handler.
 */
static int CreateService(const wchar_t *pwszService,
                         const wchar_t *pwszDisplayName,
                         uint32_t       uServiceType,
                         uint32_t       uStartType,
                         const wchar_t *pwszBinPath,
                         const wchar_t *pwszLoadOrderGroup,
                         const wchar_t *pwszDependencies,
                         const wchar_t *pwszLogonUser,
                         const wchar_t *pwszLogonPassword)
{
    PrintSWSWS("Installing service '", pwszService, "' ('", pwszDisplayName, ") ...\r\n");

    /*
     * Transform the dependency list to a REG_MULTI_SZ.
     */
    if (pwszDependencies != NULL)
    {
        /* Copy it into alloca() buffer so we can modify it. */
        size_t cwc = RTUtf16Len(pwszDependencies);
        wchar_t *pwszDup = (wchar_t *)alloca((cwc + 2) * sizeof(wchar_t));
        memcpy(pwszDup, pwszDependencies, cwc * sizeof(wchar_t));
        pwszDup[cwc]     = L'\0';
        pwszDup[cwc + 1] = L'\0';   /* double termination */

        /* Perform: s/,/\0/g */
        while (cwc-- > 0 )
            if (pwszDup[cwc] == L',')
                pwszDup[cwc] = L'\0';

        pwszDependencies = pwszDup;
    }

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
        return ErrorMsgLastErr("OpenSCManagerW failed");

    int rcExit = EXIT_FAIL;
    DWORD dwTag = 0xDEADBEAF;
    SC_HANDLE hService = CreateServiceW(hSCManager, pwszService, pwszDisplayName, SERVICE_ALL_ACCESS, uServiceType, uStartType,
                                        SERVICE_ERROR_NORMAL, pwszBinPath, pwszLoadOrderGroup, pwszLoadOrderGroup ? &dwTag : NULL,
                                        pwszDependencies, pwszLogonUser, pwszLogonPassword);
    if (hService != NULL)
    {
        CloseServiceHandle(hService);
        PrintStr("Installation of service successful!\r\n");
        rcExit = EXIT_OK;
    }
    else
    {
        DWORD dwErr = GetLastError();
        if (dwErr == ERROR_SERVICE_EXISTS)
        {
            PrintStr("Service already exists. Updating the service config ...\r\n");
            hService = OpenServiceW(hSCManager, pwszService, SERVICE_ALL_ACCESS);
            if (hService != NULL)
            {
                if (ChangeServiceConfigW(hService, uServiceType, uStartType, SERVICE_ERROR_NORMAL, pwszBinPath,
                                         pwszLoadOrderGroup, pwszLoadOrderGroup ? &dwTag : NULL, pwszDependencies,
                                         pwszLogonUser, pwszLogonPassword, pwszDisplayName))
                {
                    PrintStr("The service config has been successfully updated.\r\n");
                    rcExit = EXIT_OK;
                }
                else
                    rcExit = ErrorMsgLastErrSWS("ChangeServiceConfigW failed on '", pwszService, "'!");
                CloseServiceHandle(hService);
            }
            else
                rcExit = ErrorMsgLastErrSWS("OpenSCManagerW failed on '", pwszService, "'!");

            /*
             * This branch does not return an error to avoid installations failures,
             * if updating service parameters. Better to have a running system with old
             * parameters and the failure information in the installation log.
             */
            rcExit = EXIT_OK;
        }
        else
            rcExit = ErrorMsgLastErrSWS("CreateServiceW for '", pwszService, "'!");
    }

    CloseServiceHandle(hSCManager);
    return rcExit;
}


/** Handles 'service create'. */
static int handleServiceCreate(unsigned cArgs, wchar_t **papwszArgs)
{
    uint32_t uServiceType;
    if (!ArgToUInt32Full(papwszArgs[2], "service-type", &uServiceType))
        return EXIT_USAGE;

    uint32_t uStartType;
    if (!ArgToUInt32Full(papwszArgs[3], "start-type", &uStartType))
        return EXIT_USAGE;

    return CreateService(papwszArgs[0], papwszArgs[1], uServiceType, uStartType, papwszArgs[4],
                         cArgs > 5 ? papwszArgs[5] : NULL,
                         cArgs > 6 ? papwszArgs[6] : NULL,
                         cArgs > 7 ? papwszArgs[7] : NULL,
                         cArgs > 8 ? papwszArgs[8] : NULL);
}


/**
 * Worker for the 'service delete' handler.
 */
static int DelService(const wchar_t *pwszService)
{
    PrintSWS("Removing service '", pwszService, "' ...\r\n");

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
        return ErrorMsgLastErr("OpenSCManagerW failed");

    int rcExit = EXIT_FAIL;
    SC_HANDLE hService = NULL;
    hService = OpenServiceW(hSCManager, pwszService, SERVICE_ALL_ACCESS);
    if (hService)
    {
        SC_LOCK hSCLock = LockServiceDatabase(hSCManager);
        if (hSCLock != NULL)
        {
            if (DeleteService(hService))
            {
                PrintSWS("Service '", pwszService, "' successfully deleted.\r\n");
                rcExit = EXIT_OK;
            }
            else
            {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_SERVICE_MARKED_FOR_DELETE)
                {
                    PrintSWS("Service '", pwszService, "' already marked for deletion.\r\n");
                    rcExit = EXIT_OK;
                }
                else
                    rcExit = ErrorMsgLastErrSWS("Failed to delete service'", pwszService, "'!");
            }
            UnlockServiceDatabase(hSCLock);
        }
        else
            ErrorMsgLastErr("LockServiceDatabase failed");
        CloseServiceHandle(hService);
    }
    else
        rcExit = ErrorMsgLastErrSWS("Failed to open service'", pwszService, "'!");
    CloseServiceHandle(hSCManager);
    return rcExit;
}


/** Handles 'service delete' */
static int handleServiceDelete(unsigned cArgs, wchar_t **papwszArgs)
{
    RT_NOREF(cArgs);
    return DelService(papwszArgs[0]);
}


static HKEY ArgToRegistryRoot(const wchar_t *pwszRoot)
{
    HKEY hRootKey = NULL;
    if (RTUtf16ICmpAscii(pwszRoot, "hklm") == 0)
        hRootKey = HKEY_LOCAL_MACHINE;
    else if (RTUtf16ICmpAscii(pwszRoot, "hkcu") == 0)
        hRootKey = HKEY_CURRENT_USER;
    else if (RTUtf16ICmpAscii(pwszRoot, "hkcr") == 0)
        hRootKey = HKEY_CLASSES_ROOT;
    else if (RTUtf16ICmpAscii(pwszRoot, "hku") == 0)
        hRootKey = HKEY_USERS;
    else if (RTUtf16ICmpAscii(pwszRoot, "hkcc") == 0)
        hRootKey = HKEY_CURRENT_CONFIG;
    else
        ErrorBadArg("root", pwszRoot, "hklm, hkcu, hkcr, hku or hkcc");
    return hRootKey;
}


/**
 * Handles 'registry write'.
 */
static int handleRegistryWrite(unsigned cArgs, wchar_t **papwszArgs)
{
    /*
     * Mandatory parameters.
     */
    wchar_t const * const pwszRoot      = papwszArgs[0];
    wchar_t const * const pwszSubKey    = papwszArgs[1];
    wchar_t const * const pwszValueName = papwszArgs[2];
    wchar_t const * const pwszType      = papwszArgs[3];
    wchar_t const * const pwszValue     = papwszArgs[4];

    /*
     * Root key:
     */
    HKEY hRootKey = ArgToRegistryRoot(papwszArgs[0]);
    if (hRootKey == NULL)
        return EXIT_USAGE;

    /*
     * Type and value with default length.
     */
    union
    {
        uint32_t    dw;
        uint64_t    qw;
    } uValue;
    DWORD           dwType;
    DWORD           cbValue;
    BYTE const     *pbValue;
    if (   RTUtf16ICmpAscii(pwszType, "REG_BINARY") == 0
        || RTUtf16ICmpAscii(pwszType, "REG_BIN") == 0
        || RTUtf16ICmpAscii(pwszType, "BINARY") == 0)
    {
        dwType  = REG_BINARY;
        cbValue = (DWORD)(RTUtf16Len(pwszValue) + 1) * sizeof(wchar_t);
        pbValue = (BYTE const *)pwszValue;
    }
    else if (   RTUtf16ICmpAscii(pwszType, "REG_DWORD") == 0
             || RTUtf16ICmpAscii(pwszType, "DWORD") == 0)
    {
        if (!ArgToUInt32Full(pwszValue, "dword value", &uValue.dw))
            return EXIT_USAGE;
        dwType  = REG_DWORD;
        pbValue = (BYTE const *)&uValue.dw;
        cbValue = sizeof(uValue.dw);
    }
    else if (   RTUtf16ICmpAscii(pwszType, "REG_QWORD") == 0
             || RTUtf16ICmpAscii(pwszType, "QWORD") == 0)
    {
        if (!ArgToUInt64Full(pwszValue, "qword value", &uValue.qw))
            return EXIT_USAGE;
        dwType  = REG_QWORD;
        pbValue = (BYTE const *)&uValue.qw;
        cbValue = sizeof(uValue.qw);
    }
    else if (   RTUtf16ICmpAscii(pwszType, "REG_SZ") == 0
             || RTUtf16ICmpAscii(pwszType, "SZ") == 0)
    {
        dwType  = REG_SZ;
        cbValue = (DWORD)((RTUtf16Len(pwszValue) + 1) * sizeof(wchar_t));
        pbValue = (BYTE const *)pwszValue;
    }
    else
        return ErrorBadArg("type", pwszType, "");

    /*
     * Binary only: Reinterpret the input as - optional.
     */
    if (cArgs > 5)
    {
        if (dwType != REG_BINARY)
            return ErrorMsg("The 'binary-conversion' argument is currently only supported for REG_BINARY type values!");
        if (RTUtf16ICmpAscii(papwszArgs[5], "dword") == 0)
        {
            if (!ArgToUInt32Full(pwszValue, "dword(/binary) value", &uValue.dw))
                return EXIT_USAGE;
            pbValue = (BYTE const *)&uValue.dw;
            cbValue = sizeof(uValue.dw);
        }
        else if (RTUtf16ICmpAscii(papwszArgs[5], "qword") == 0)
        {
            if (!ArgToUInt64Full(pwszValue, "qword(/binary) value", &uValue.qw))
                return EXIT_USAGE;
            pbValue = (BYTE const *)&uValue.qw;
            cbValue = sizeof(uValue.qw);
        }
        else
            return ErrorBadArg("binary-conversion", papwszArgs[0], "dword");
    }

    /*
     * Binary only: Max length to write - optional.
     */
    if (cArgs> 6)
    {
        if (dwType != REG_BINARY)
            return ErrorMsg("The 'max-size' argument is currently only supported for REG_BINARY type values!");
        uint32_t cbMaxValue;
        if (!ArgToUInt32Full(papwszArgs[6], "max-size", &cbMaxValue))
            return EXIT_USAGE;
        if (cbValue > cbMaxValue)
            cbValue = cbMaxValue;
    }

    /*
     * Do the writing.
     */
    HKEY    hKey = NULL;
    LSTATUS lrc  = RegCreateKeyExW(hRootKey, pwszSubKey, 0 /*Reserved*/, NULL /*pwszClass*/, 0 /*dwOptions*/,
                                   KEY_WRITE, NULL /*pSecAttr*/, &hKey, NULL /*pdwDisposition*/);
    if (lrc != ERROR_SUCCESS)
        return ErrorMsgLStatusSWSWSRS("RegCreateKeyExW ", pwszRoot, "/'", pwszSubKey, "' failed: ", lrc, NULL);

    lrc = RegSetValueExW(hKey, pwszValueName, 0, dwType, pbValue, cbValue);
    RegCloseKey(hKey);
    if (lrc != ERROR_SUCCESS)
        return ErrorMsgLStatusSWSWSWSRS("RegSetValueExW ", pwszRoot, "/'", pwszSubKey, "'/'",
                                        pwszValueName, "' failed: ",  lrc, NULL);
    return EXIT_OK;
}


/**
 * Handles 'registry delete'.
 */
static int handleRegistryDelete(unsigned cArgs, wchar_t **papwszArgs)
{
    /*
     * Parameters.
     */
    RT_NOREF(cArgs);
    wchar_t const * const pwszRoot      = papwszArgs[0];
    wchar_t const * const pwszSubKey    = papwszArgs[1];
    wchar_t const * const pwszValueName = papwszArgs[2];

    HKEY const hRootKey = ArgToRegistryRoot(pwszRoot);
    if (hRootKey == NULL)
        return EXIT_USAGE;

    /*
     * Do the deleting.
     */
    HKEY    hKey = NULL;
    LSTATUS lrc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, papwszArgs[1] /*pwszSubKey*/, 0 /*dwOptions*/, KEY_READ | KEY_WRITE, &hKey);
    if (lrc != ERROR_FILE_NOT_FOUND)
    {
        if (lrc != ERROR_SUCCESS)
            return ErrorMsgLStatusSWSWSRS("RegOpenKeyExW ", pwszRoot, "/'", pwszSubKey, "' failed: ", lrc, NULL);

        lrc = RegDeleteValueW(hKey, pwszValueName);
        RegCloseKey(hKey);
        if (lrc != ERROR_SUCCESS && lrc != ERROR_FILE_NOT_FOUND)
            return ErrorMsgLStatusSWSWSWSRS("RegDeleteValueW ", pwszRoot, "/'", pwszSubKey, "'/'",
                                            pwszValueName, "' failed: ",  lrc, NULL);
    }
    return EXIT_OK;
}


/** Handles 'version' and its aliases. */
static int handleVersion(unsigned cArgs, wchar_t **papwszArgs)
{
    PrintStr(RT_XSTR(VBOX_VERSION_MAJOR) "." RT_XSTR(VBOX_VERSION_MINOR) "." RT_XSTR(VBOX_VERSION_BUILD) "r" RT_XSTR(VBOX_SVN_REV) "\r\n");
    RT_NOREF(cArgs, papwszArgs);
    return EXIT_OK;
}


/** Handles 'help' and all its aliases. */
static int handleHelp(unsigned cArgs, wchar_t **papwszArgs)
{
    /*       "0         1         2         3         4         5         6         7         8 */
    /*       "012345678901234567890123456789012345678901234567890123456789012345678901234567890 */
    PrintStr("VirtualBox Guest Additions Installation Helper for Windows\r\n"
             "Version: " RT_XSTR(VBOX_VERSION_MAJOR) "." RT_XSTR(VBOX_VERSION_MINOR) "." RT_XSTR(VBOX_VERSION_BUILD) "r" RT_XSTR(VBOX_SVN_REV) "\r\n"
             "\r\n"
             "Syntax: VBoxDrvInst <command> <subcommand>\r\n"
             "\r\n"
             "Drivers:\r\n"
             "    VBoxDrvInst driver install <inf-file> [log-file]\r\n"
             "    VBoxDrvInst driver uninstall <inf-file> [log-file]\r\n"
             "    VBoxDrvInst driver executeinf <inf-file>\r\n"
             "\r\n"
             "Service:\r\n"
             "    VBoxDrvInst service create <name> <display-name> <service-type>\r\n"
             "        <start-type> <binary-path> [load-order] [deps] [user] [password]\r\n"
             "    VBoxDrvInst service delete <name>\r\n"
             "\r\n"
             "Network Provider:\r\n"
             "    VBoxDrvInst netprovider add <name> <position>\r\n"
             "    VBoxDrvInst netprovider remove <name>\r\n"
             "\r\n"
             "Registry:\r\n"
             "    VBoxDrvInst registry write <root> <sub-key> <value-name> <type> <value>\r\n"
             "        [binary-conversion] [max-size]\r\n"
             "    VBoxDrvInst registry delete <root> <sub-key> <value-name>\r\n"
             /** @todo Add roots for these two. */
             "    VBoxDrvInst registry addmultisz <sub-key> <value-name> <to-add> <position>\r\n"
             "    VBoxDrvInst registry delmultisz <sub-key> <value-name> <to-remove>\r\n"
             "\r\n"
             "Standard options:\r\n"
             "    VBoxDrvInst [help|--help|/help|-h|/h|-?|/h] [...]\r\n"
             "    VBoxDrvInst [version|--version|-V]\r\n"
             );
    RT_NOREF(cArgs, papwszArgs);
    return EXIT_OK;
}


int wmain(int argc, wchar_t **argv)
{
    /* Not initializing IPRT here, ASSUMING the little bit we use of it does
       not need any initialization.  Reduces the binary size a little. */

    static struct
    {
        const char *pszCmd;
        const char *pszSubCmd;
        unsigned    cMin, cMax;
        int       (*pfnHandler)(unsigned cArgs, wchar_t **papwszArgs);
    } s_aActions[] =
    {
        { "driver",         "install",      1,  2, handleDriverInstall },
        { "driver",         "uninstall",    1,  2, handleDriverUninstall },
        { "driver",         "executeinf",   1,  1, handleDriverExecuteInf },
        { "netprovider",    "add",          1,  2, handleNetProviderAdd },
        { "netprovider",    "remove",       1,  2, handleNetProviderRemove },
        { "service",        "create",       5,  9, handleServiceCreate },
        { "service",        "delete",       1,  1, handleServiceDelete },
        { "registry",       "addmultisz",   4,  4, handleRegistryAddMultiSz },
        { "registry",       "delmultisz",   3,  3, handleRegistryDelMultiSz },
        { "registry",       "write",        5,  7, handleRegistryWrite },
        { "registry",       "delete",       3,  3, handleRegistryDelete },

        { "help",           NULL,           0, ~0U, handleHelp },
        { "--help",         NULL,           0, ~0U, handleHelp },
        { "/help",          NULL,           0, ~0U, handleHelp },
        { "-h",             NULL,           0, ~0U, handleHelp },
        { "/h",             NULL,           0, ~0U, handleHelp },
        { "-?",             NULL,           0, ~0U, handleHelp },
        { "/?",             NULL,           0, ~0U, handleHelp },
        { "version",        NULL,           0, ~0U, handleVersion },
        { "--version",      NULL,           0, ~0U, handleVersion },
        { "-V",             NULL,           0, ~0U, handleVersion },
    };

    /*
     * Lookup the action handler.
     */
    int rcExit = EXIT_USAGE;
    if (argc >= 2)
    {
        const wchar_t * const pwszCmd    = argv[1];
        const wchar_t * const pwszSubCmd = argc > 2 ? argv[2] : NULL;
        unsigned              i          = 0;
        for (i = 0; i < RT_ELEMENTS(s_aActions); i++)
            if (   RTUtf16ICmpAscii(pwszCmd, s_aActions[i].pszCmd) == 0
                && (   !s_aActions[i].pszSubCmd
                    || RTUtf16ICmpAscii(pwszSubCmd, s_aActions[i].pszSubCmd) == 0))
            {
                unsigned   const cArgs      = (unsigned)argc - (s_aActions[i].pszSubCmd ? 3 : 2);
                wchar_t ** const papwszArgs = &argv[s_aActions[i].pszSubCmd ? 3 : 2];
                if (cArgs >= s_aActions[i].cMin && cArgs <= s_aActions[i].cMax)
                    rcExit = s_aActions[i].pfnHandler(cArgs, papwszArgs);
                else
                {
                    bool const fTooFew = cArgs < s_aActions[i].cMin;
                    ErrorMsgBegin(fTooFew ? "Too few parameters for '" : "Too many parameters for '");
                    ErrorMsgStr(s_aActions[i].pszCmd);
                    if (s_aActions[i].pszSubCmd)
                    {
                        ErrorMsgStr(" ");
                        ErrorMsgStr(s_aActions[i].pszSubCmd);
                    }
                    ErrorMsgStr("'! Got ");
                    ErrorMsgU64(cArgs);
                    ErrorMsgStr(fTooFew ? ", expected at least " : ", expected at most ");;
                    ErrorMsgU64(fTooFew ? s_aActions[i].cMin : s_aActions[i].cMax);
                    ErrorMsgEnd(".");
                }
                break;
            }
        if (i >= RT_ELEMENTS(s_aActions))
        {
            ErrorMsgBegin("Unknown action '");
            ErrorMsgWStr(pwszCmd);
            if (pwszSubCmd)
            {
                ErrorMsgBegin(" ");
                ErrorMsgWStr(pwszSubCmd);
            }
            ErrorMsgEnd("'! Please consult \"--help\" for more information.\r\n");
        }
    }
    else
        ErrorMsg("No parameters given. Please consult \"--help\" for more information.\r\n");
    return rcExit;
}


#ifdef IPRT_NO_CRT
int main(int argc, char **argv)
{
    /*
     * Convert the arguments to UTF16 and call wmain.  We don't bother freeing
     * any of these strings as the process is exiting and it's a waste of time.
     */
    wchar_t **papwszArgs = (wchar_t **)alloca((argc + 1) * sizeof(wchar_t *));
    int i = 0;
    while (i < argc)
    {
        papwszArgs[i] = NULL;
        int rc = RTStrToUtf16(argv[i], &papwszArgs[i]);
        if (RT_SUCCESS(rc))
            i++;
        else
            return ErrorMsg("Failed to convert command line arguments to UTF16!!");
    }
    papwszArgs[i] = NULL;
    return wmain(argc, papwszArgs);
}
#endif

