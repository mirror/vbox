/* $Id$ */
/** @file
 * IPRT - Process, Windows.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_PROCESS
#include <iprt/asm.h> /* hack */

#include <Userenv.h>
#include <Windows.h>
#include <tlhelp32.h>
#include <process.h>
#include <errno.h>
#include <Strsafe.h>
#include <Lmcons.h>

#include <iprt/process.h>
#include "internal-r3-win.h"

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/socket.h>



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/* kernel32.dll: */
//typedef DWORD   (WINAPI *PFNWTSGETACTIVECONSOLESESSIONID)(VOID);
typedef HANDLE  (WINAPI *PFNCREATETOOLHELP32SNAPSHOT)(DWORD, DWORD);
typedef BOOL    (WINAPI *PFNPROCESS32FIRST)(HANDLE, LPPROCESSENTRY32);
typedef BOOL    (WINAPI *PFNPROCESS32FIRSTW)(HANDLE, LPPROCESSENTRY32W);
typedef BOOL    (WINAPI *PFNPROCESS32NEXT)(HANDLE, LPPROCESSENTRY32);
typedef BOOL    (WINAPI *PFNPROCESS32NEXTW)(HANDLE, LPPROCESSENTRY32W);

/* psapi.dll: */
typedef BOOL    (WINAPI *PFNENUMPROCESSES)(LPDWORD, DWORD, LPDWORD);
typedef DWORD   (WINAPI *PFNGETMODULEBASENAME)(HANDLE, HMODULE, LPTSTR, DWORD);

/* advapi32.dll: */
typedef BOOL    (WINAPI *PFNCREATEPROCESSWITHLOGON)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPCWSTR, LPWSTR, DWORD,
                                                    LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);

/* userenv.dll: */
typedef BOOL    (WINAPI *PFNCREATEENVIRONMENTBLOCK)(LPVOID *, HANDLE, BOOL);
typedef BOOL    (WINAPI *PFNPFNDESTROYENVIRONMENTBLOCK)(LPVOID);
typedef BOOL    (WINAPI *PFNLOADUSERPROFILEW)(HANDLE, LPPROFILEINFOW);
typedef BOOL    (WINAPI *PFNUNLOADUSERPROFILE)(HANDLE, HANDLE);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Init once structure. */
static RTONCE       g_rtProcWinInitOnce = RTONCE_INITIALIZER;
/** Critical section protecting the process array. */
static RTCRITSECT   g_CritSect;
/** The number of processes in the array. */
static uint32_t     g_cProcesses;
/** The current allocation size. */
static uint32_t     g_cProcessesAlloc;
/** Array containing the live or non-reaped child processes. */
static struct RTPROCWINENTRY
{
    /** The process ID. */
    ULONG_PTR       pid;
    /** The process handle. */
    HANDLE          hProcess;
}                  *g_paProcesses;

/** @name userenv.dll imports (we don't unload it).
 * They're all optional. So in addition to using g_rtProcWinResolveOnce, the
 * caller must also check if any of the necessary APIs are NULL pointers.
 * @{ */
/** Init once structure for run-as-user functions we need.. */
static RTONCE                           g_rtProcWinResolveOnce          = RTONCE_INITIALIZER;
/* kernel32.dll: */
static PFNCREATETOOLHELP32SNAPSHOT      g_pfnCreateToolhelp32Snapshot   = NULL;
static PFNPROCESS32FIRST                g_pfnProcess32First             = NULL;
static PFNPROCESS32NEXT                 g_pfnProcess32Next              = NULL;
static PFNPROCESS32FIRSTW               g_pfnProcess32FirstW            = NULL;
static PFNPROCESS32NEXTW                g_pfnProcess32NextW             = NULL;
/* psapi.dll: */
static PFNGETMODULEBASENAME             g_pfnGetModuleBaseName          = NULL;
static PFNENUMPROCESSES                 g_pfnEnumProcesses              = NULL;
/* advapi32.dll: */
static PFNCREATEPROCESSWITHLOGON        g_pfnCreateProcessWithLogonW    = NULL;
/* userenv.dll: */
static PFNCREATEENVIRONMENTBLOCK        g_pfnCreateEnvironmentBlock     = NULL;
static PFNPFNDESTROYENVIRONMENTBLOCK    g_pfnDestroyEnvironmentBlock    = NULL;
static PFNLOADUSERPROFILEW              g_pfnLoadUserProfileW           = NULL;
static PFNUNLOADUSERPROFILE             g_pfnUnloadUserProfile          = NULL;
/** @} */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int rtProcWinFindExe(uint32_t fFlags, RTENV hEnv, const char *pszExec, PRTUTF16 *ppwszExec);


/**
 * Clean up the globals.
 *
 * @param   enmReason           Ignored.
 * @param   iStatus             Ignored.
 * @param   pvUser              Ignored.
 */
static DECLCALLBACK(void) rtProcWinTerm(RTTERMREASON enmReason, int32_t iStatus, void *pvUser)
{
    NOREF(pvUser); NOREF(iStatus); NOREF(enmReason);

    RTCritSectDelete(&g_CritSect);

    size_t i = g_cProcesses;
    while (i-- > 0)
    {
        CloseHandle(g_paProcesses[i].hProcess);
        g_paProcesses[i].hProcess = NULL;
    }
    RTMemFree(g_paProcesses);

    g_paProcesses     = NULL;
    g_cProcesses      = 0;
    g_cProcessesAlloc = 0;
}


/**
 * Initialize the globals.
 *
 * @returns IPRT status code.
 * @param   pvUser              Ignored.
 */
static DECLCALLBACK(int32_t) rtProcWinInitOnce(void *pvUser)
{
    NOREF(pvUser);

    g_cProcesses        = 0;
    g_cProcessesAlloc   = 0;
    g_paProcesses       = NULL;
    int rc = RTCritSectInit(&g_CritSect);
    if (RT_SUCCESS(rc))
    {
        /** @todo init once, terminate once - this is a generic thing which should
         *        have some kind of static and simpler setup!  */
        rc = RTTermRegisterCallback(rtProcWinTerm, NULL);
        if (RT_SUCCESS(rc))
            return rc;
        RTCritSectDelete(&g_CritSect);
    }
    return rc;
}


/**
 * Gets the process handle for a process from g_paProcesses.
 *
 * @returns Process handle if found, NULL if not.
 * @param   pid                 The process to remove (pid).
 */
static HANDLE rtProcWinFindPid(RTPROCESS pid)
{
    HANDLE hProcess = NULL;

    RTCritSectEnter(&g_CritSect);
    uint32_t i = g_cProcesses;
    while (i-- > 0)
        if (g_paProcesses[i].pid == pid)
        {
            hProcess = g_paProcesses[i].hProcess;
            break;
        }
    RTCritSectLeave(&g_CritSect);

    return hProcess;
}


/**
 * Removes a process from g_paProcesses and closes the process handle.
 *
 * @param   pid                 The process to remove (pid).
 */
static void rtProcWinRemovePid(RTPROCESS pid)
{
    RTCritSectEnter(&g_CritSect);
    uint32_t i = g_cProcesses;
    while (i-- > 0)
        if (g_paProcesses[i].pid == pid)
        {
            HANDLE hProcess = g_paProcesses[i].hProcess;

            g_cProcesses--;
            uint32_t cToMove = g_cProcesses - i;
            if (cToMove)
                memmove(&g_paProcesses[i], &g_paProcesses[i + 1], cToMove * sizeof(g_paProcesses[0]));

            RTCritSectLeave(&g_CritSect);
            CloseHandle(hProcess);
            return;
        }
    RTCritSectLeave(&g_CritSect);
}


/**
 * Adds a process to g_paProcesses.
 *
 * @returns IPRT status code.
 * @param   pid                 The process id.
 * @param   hProcess            The process handle.
 */
static int rtProcWinAddPid(RTPROCESS pid, HANDLE hProcess)
{
    RTCritSectEnter(&g_CritSect);

    uint32_t i = g_cProcesses;
    if (i >= g_cProcessesAlloc)
    {
        void *pvNew = RTMemRealloc(g_paProcesses, (i + 16) * sizeof(g_paProcesses[0]));
        if (RT_UNLIKELY(!pvNew))
        {
            RTCritSectLeave(&g_CritSect);
            return VERR_NO_MEMORY;
        }
        g_paProcesses     = (struct RTPROCWINENTRY *)pvNew;
        g_cProcessesAlloc = i + 16;
    }

    g_paProcesses[i].pid      = pid;
    g_paProcesses[i].hProcess = hProcess;
    g_cProcesses = i + 1;

    RTCritSectLeave(&g_CritSect);
    return VINF_SUCCESS;
}


/**
 * Initialize the import APIs for run-as-user and special environment support.
 *
 * @returns IPRT status code.
 * @param   pvUser              Ignored.
 */
static DECLCALLBACK(int) rtProcWinResolveOnce(void *pvUser)
{
    int      rc;
    RTLDRMOD hMod;

    /*
     * kernel32.dll APIs introduced after NT4.
     */
    g_pfnCreateToolhelp32Snapshot   = (PFNCREATETOOLHELP32SNAPSHOT)GetProcAddress(g_hModKernel32, "CreateToolhelp32Snapshot");
    g_pfnProcess32First             = (PFNPROCESS32FIRST          )GetProcAddress(g_hModKernel32, "Process32First");
    g_pfnProcess32FirstW            = (PFNPROCESS32FIRSTW         )GetProcAddress(g_hModKernel32, "Process32FirstW");
    g_pfnProcess32Next              = (PFNPROCESS32NEXT           )GetProcAddress(g_hModKernel32, "Process32Next");
    g_pfnProcess32NextW             = (PFNPROCESS32NEXTW          )GetProcAddress(g_hModKernel32, "Process32NextW");

    /*
     * psapi.dll APIs, if none of the above are available.
     */
    if (   !g_pfnCreateToolhelp32Snapshot
        || !g_pfnProcess32First
        || !g_pfnProcess32Next)
    {
        Assert(!g_pfnCreateToolhelp32Snapshot && !g_pfnProcess32First && !g_pfnProcess32Next);

        rc = RTLdrLoadSystem("psapi.dll", true /*fNoUnload*/, &hMod);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(hMod, "GetModuleBaseName", (void **)&g_pfnGetModuleBaseName);
            AssertStmt(RT_SUCCESS(rc), g_pfnGetModuleBaseName = NULL);

            rc = RTLdrGetSymbol(hMod, "EnumProcesses", (void **)&g_pfnEnumProcesses);
            AssertStmt(RT_SUCCESS(rc), g_pfnEnumProcesses = NULL);

            RTLdrClose(hMod);
        }
    }

    /*
     * advapi32.dll APIs.
     */
    g_pfnCreateProcessWithLogonW    = (PFNCREATEPROCESSWITHLOGON)RTLdrGetSystemSymbol("advapi32.dll", "CreateProcessWithLogonW");

    /*
     * userenv.dll APIs.
     */
    rc = RTLdrLoadSystem("userenv.dll", true /*fNoUnload*/, &hMod);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(hMod, "LoadUserProfileW", (void **)&g_pfnLoadUserProfileW);
        AssertStmt(RT_SUCCESS(rc), g_pfnLoadUserProfileW = NULL);

        rc = RTLdrGetSymbol(hMod, "UnloadUserProfile", (void **)&g_pfnUnloadUserProfile);
        AssertStmt(RT_SUCCESS(rc), g_pfnUnloadUserProfile = NULL);

        rc = RTLdrGetSymbol(hMod, "CreateEnvironmentBlock", (void **)&g_pfnCreateEnvironmentBlock);
        AssertStmt(RT_SUCCESS(rc), g_pfnCreateEnvironmentBlock = NULL);

        rc = RTLdrGetSymbol(hMod, "DestroyEnvironmentBlock", (void **)&g_pfnDestroyEnvironmentBlock);
        AssertStmt(RT_SUCCESS(rc), g_pfnDestroyEnvironmentBlock = NULL);

        RTLdrClose(hMod);
    }

    return VINF_SUCCESS;
}


RTR3DECL(int) RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess)
{
    return RTProcCreateEx(pszExec, papszArgs, Env, fFlags,
                          NULL, NULL, NULL,  /* standard handles */
                          NULL /*pszAsUser*/, NULL /* pszPassword*/,
                          pProcess);
}


/**
 * Map some important or much used Windows error codes
 * to our error codes.
 *
 * @return  Mapped IPRT status code.
 * @param   dwError                         Windows error code to map to IPRT code.
 */
static int rtProcWinMapErrorCodes(DWORD dwError)
{
    int rc;
    switch (dwError)
    {
        case ERROR_NOACCESS:
        case ERROR_PRIVILEGE_NOT_HELD:
            rc = VERR_PERMISSION_DENIED;
            break;

        case ERROR_PASSWORD_EXPIRED:
        case ERROR_ACCOUNT_RESTRICTION: /* See: http://support.microsoft.com/kb/303846/ */
        case ERROR_PASSWORD_RESTRICTION:
        case ERROR_ACCOUNT_DISABLED:    /* See: http://support.microsoft.com/kb/263936 */
            rc = VERR_ACCOUNT_RESTRICTED;
            break;

        case ERROR_FILE_CORRUPT:
            rc = VERR_BAD_EXE_FORMAT;
            break;

        case ERROR_BAD_DEVICE: /* Can happen when opening funny things like "CON". */
            rc = VERR_INVALID_NAME;
            break;

        default:
            /* Could trigger a debug assertion! */
            rc = RTErrConvertFromWin32(dwError);
            break;
    }
    return rc;
}


/**
 * Get the process token of the process indicated by @a dwPID if the @a pSid
 * matches.
 *
 * @returns IPRT status code.
 * @param   dwPid           The process identifier.
 * @param   pSid            The secure identifier of the user.
 * @param   phToken         Where to return the a duplicate of the process token
 *                          handle on success. (The caller closes it.)
 */
static int rtProcWinGetProcessTokenHandle(DWORD dwPid, PSID pSid, PHANDLE phToken)
{
    AssertPtr(pSid);
    AssertPtr(phToken);

    int     rc;
    HANDLE  hProc = OpenProcess(MAXIMUM_ALLOWED, TRUE, dwPid);
    if (hProc != NULL)
    {
        HANDLE hTokenProc;
        if (OpenProcessToken(hProc,
                             TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE
                             | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID | TOKEN_READ | TOKEN_WRITE,
                             &hTokenProc))
        {
            SetLastError(NO_ERROR);
            DWORD   dwSize = 0;
            BOOL    fRc    = GetTokenInformation(hTokenProc, TokenUser, NULL, 0, &dwSize);
            DWORD   dwErr  = GetLastError();
            if (   !fRc
                && dwErr == ERROR_INSUFFICIENT_BUFFER
                && dwSize > 0)
            {
                PTOKEN_USER pTokenUser = (PTOKEN_USER)RTMemTmpAllocZ(dwSize);
                if (pTokenUser)
                {
                    if (GetTokenInformation(hTokenProc,
                                            TokenUser,
                                            pTokenUser,
                                            dwSize,
                                            &dwSize))
                    {
                        if (   IsValidSid(pTokenUser->User.Sid)
                            && EqualSid(pTokenUser->User.Sid, pSid))
                        {
                            if (DuplicateTokenEx(hTokenProc, MAXIMUM_ALLOWED,
                                                 NULL, SecurityIdentification, TokenPrimary, phToken))
                            {
                                /*
                                 * So we found the process instance which belongs to the user we want to
                                 * to run our new process under. This duplicated token will be used for
                                 * the actual CreateProcessAsUserW() call then.
                                 */
                                rc = VINF_SUCCESS;
                            }
                            else
                                rc = rtProcWinMapErrorCodes(GetLastError());
                        }
                        else
                            rc = VERR_NOT_FOUND;
                    }
                    else
                        rc = rtProcWinMapErrorCodes(GetLastError());
                    RTMemTmpFree(pTokenUser);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else if (fRc || dwErr == NO_ERROR)
                rc = VERR_IPE_UNEXPECTED_STATUS;
            else
                rc = rtProcWinMapErrorCodes(dwErr);
            CloseHandle(hTokenProc);
        }
        else
            rc = rtProcWinMapErrorCodes(GetLastError());
        CloseHandle(hProc);
    }
    else
        rc = rtProcWinMapErrorCodes(GetLastError());
    return rc;
}


/**
 * Fallback method for rtProcWinFindTokenByProcess that uses the older NT4
 * PSAPI.DLL API.
 *
 * @returns Success indicator.
 * @param   papszNames      The process candidates, in prioritized order.
 * @param   pSid            The secure identifier of the user.
 * @param   phToken         Where to return the token handle - duplicate,
 *                          caller closes it on success.
 *
 * @remarks NT4 needs a copy of "PSAPI.dll" (redistributed by Microsoft and not
 *          part of the OS) in order to get a lookup.  If we don't have this DLL
 *          we are not able to get a token and therefore no UI will be visible.
 */
static bool rtProcWinFindTokenByProcessAndPsApi(const char * const *papszNames, PSID pSid, PHANDLE phToken)
{
    /*
     * Load PSAPI.DLL and resolve the two symbols we need.
     */
    if (   !g_pfnGetModuleBaseName
        || !g_pfnEnumProcesses)
        return false;

    /*
     * Get a list of PID.  We retry if it looks like there are more PIDs
     * to be returned than what we supplied buffer space for.
     */
    bool   fFound          = false;
    int    rc              = VINF_SUCCESS;
    DWORD  cbPidsAllocated = 4096;
    DWORD  cbPidsReturned;
    DWORD *paPids;
    for (;;)
    {
        paPids = (DWORD *)RTMemTmpAlloc(cbPidsAllocated);
        AssertBreakStmt(paPids, rc = VERR_NO_TMP_MEMORY);
        cbPidsReturned = 0;
        if (!g_pfnEnumProcesses(paPids, cbPidsAllocated, &cbPidsReturned))
        {
            rc = RTErrConvertFromWin32(GetLastError());
            AssertMsgFailedBreak(("%Rrc\n", rc));
        }
        if (   cbPidsReturned < cbPidsAllocated
            || cbPidsAllocated >= _512K)
            break;
        RTMemTmpFree(paPids);
        cbPidsAllocated *= 2;
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Search for the process.
         *
         * We ASSUME that the caller won't be specifying any names longer
         * than RTPATH_MAX.
         */
        DWORD cbProcName  = RTPATH_MAX;
        char *pszProcName = (char *)RTMemTmpAlloc(RTPATH_MAX);
        if (pszProcName)
        {
            for (size_t i = 0; papszNames[i] && !fFound; i++)
            {
                const DWORD cPids = cbPidsReturned / sizeof(DWORD);
                for (DWORD iPid = 0; iPid < cPids && !fFound; iPid++)
                {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, paPids[iPid]);
                    if (hProc)
                    {
                        *pszProcName = '\0';
                        DWORD cbRet = g_pfnGetModuleBaseName(hProc, 0 /*hModule = exe */, pszProcName, cbProcName);
                        if (   cbRet > 0
                            && _stricmp(pszProcName, papszNames[i]) == 0
                            && RT_SUCCESS(rtProcWinGetProcessTokenHandle(paPids[iPid], pSid, phToken)))
                            fFound = true;
                        CloseHandle(hProc);
                    }
                }
            }
            RTMemTmpFree(pszProcName);
        }
        else
            rc = VERR_NO_TMP_MEMORY;
    }
    RTMemTmpFree(paPids);

    return fFound;
}


/**
 * Finds a one of the processes in @a papszNames running with user @a pSid and
 * returns a duplicate handle to its token.
 *
 * @returns Success indicator.
 * @param   papszNames      The process candidates, in prioritized order.
 * @param   pSid            The secure identifier of the user.
 * @param   phToken         Where to return the token handle - duplicate,
 *                          caller closes it on success.
 */
static bool rtProcWinFindTokenByProcess(const char * const *papszNames, PSID pSid, PHANDLE phToken)
{
    AssertPtr(papszNames);
    AssertPtr(pSid);
    AssertPtr(phToken);

    bool fFound = false;

    /*
     * On modern systems (W2K+) try the Toolhelp32 API first; this is more stable
     * and reliable.  Fallback to EnumProcess on NT4.
     */
    bool fFallback = true;
    if (g_pfnProcess32Next && g_pfnProcess32First && g_pfnCreateToolhelp32Snapshot)
    {
        HANDLE hSnap = g_pfnCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        Assert(hSnap != INVALID_HANDLE_VALUE);
        if (hSnap != INVALID_HANDLE_VALUE)
        {
            fFallback = false;
            for (size_t i = 0; papszNames[i] && !fFound; i++)
            {
                PROCESSENTRY32 ProcEntry;
                ProcEntry.dwSize = sizeof(PROCESSENTRY32);
/** @todo use W APIs here.   */
                if (g_pfnProcess32First(hSnap, &ProcEntry))
                {
                    do
                    {
                        if (_stricmp(ProcEntry.szExeFile, papszNames[i]) == 0)
                        {
                            int rc = rtProcWinGetProcessTokenHandle(ProcEntry.th32ProcessID, pSid, phToken);
                            if (RT_SUCCESS(rc))
                            {
                                fFound = true;
                                break;
                            }
                        }
                    } while (g_pfnProcess32Next(hSnap, &ProcEntry));
                }
#ifdef RT_STRICT
                else
                {
                    DWORD dwErr = GetLastError();
                    AssertMsgFailed(("dwErr=%u (%x)\n", dwErr, dwErr));
                }
#endif
            }
            CloseHandle(hSnap);
        }
    }

    /* If we couldn't take a process snapshot for some reason or another, fall
       back on the NT4 compatible API. */
    if (fFallback)
        fFound = rtProcWinFindTokenByProcessAndPsApi(papszNames, pSid, phToken);
    return fFound;
}


/**
 * Logs on a specified user and returns its primary token.
 *
 * @returns IPRT status code.
 * @param   pwszUser            User name.
 * @param   pwszPassword        Password.
 * @param   pwszDomain          Domain (not used at the moment).
 * @param   phToken             Pointer to store the logon token.
 */
static int rtProcWinUserLogon(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszDomain, HANDLE *phToken)
{
    AssertPtrReturn(pwszUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pwszPassword, VERR_INVALID_POINTER);
    NOREF(pwszDomain); /** @todo Add domain support! */

    /*
     * Because we have to deal with http://support.microsoft.com/kb/245683
     * for NULL domain names when running on NT4 here, pass an empty string if so.
     * However, passing FQDNs should work!
     */
    BOOL fRc = LogonUserW(pwszUser,
                          g_enmWinVer < kRTWinOSType_2K ? L"" /* NT4 and older */ : NULL /* Windows 2000 and up */,
                          pwszPassword,
                          LOGON32_LOGON_INTERACTIVE,
                          LOGON32_PROVIDER_DEFAULT,
                          phToken);
    if (fRc)
        return VINF_SUCCESS;

    DWORD dwErr = GetLastError();
    int rc = rtProcWinMapErrorCodes(dwErr);
    if (rc == VERR_UNRESOLVED_ERROR)
        LogRelFunc(("dwErr=%u (%#x), rc=%Rrc\n", dwErr, dwErr, rc));
    return rc;
}


/**
 * Returns the environment to use for the child process.
 *
 * This implements the RTPROC_FLAGS_ENV_CHANGE_RECORD and environment related
 * parts of RTPROC_FLAGS_PROFILE.
 *
 * @returns IPRT status code.
 * @param   hToken      The user token to use if RTPROC_FLAGS_PROFILE is given.
 *                      The caller must have loaded profile for this.
 * @param   hEnv        The environment passed in by the RTProcCreateEx caller.
 * @param   fFlags      The process creation flags passed in by the
 *                      RTProcCreateEx caller (RTPROC_FLAGS_XXX).
 * @param   phEnv       Where to return the environment to use.  This can either
 *                      be a newly created environment block or @a hEnv.  In the
 *                      former case, the caller must destroy it.
 */
static int rtProcWinCreateEnvFromToken(HANDLE hToken, RTENV hEnv, uint32_t fFlags, PRTENV phEnv)
{
    int rc;

    /*
     * Query the environment from the user profile associated with the token if
     * the caller has specified it directly or indirectly.
     */
    if (   (fFlags & RTPROC_FLAGS_PROFILE)
        && (   hEnv == RTENV_DEFAULT
            || (fFlags & RTPROC_FLAGS_ENV_CHANGE_RECORD)) )
    {
        if (g_pfnCreateEnvironmentBlock && g_pfnDestroyEnvironmentBlock)
        {
            LPVOID pvEnvBlockProfile = NULL;
            if (g_pfnCreateEnvironmentBlock(&pvEnvBlockProfile, hToken, FALSE /* Don't inherit from parent. */))
            {
                rc = RTEnvCloneUtf16Block(phEnv, (PCRTUTF16)pvEnvBlockProfile, 0 /*fFlags*/);
                if (   (fFlags & RTPROC_FLAGS_ENV_CHANGE_RECORD)
                    && RT_SUCCESS(rc)
                    && hEnv != RTENV_DEFAULT)
                {
                    rc = RTEnvApplyChanges(*phEnv, hEnv);
                    if (RT_FAILURE(rc))
                        RTEnvDestroy(*phEnv);
                }
                g_pfnDestroyEnvironmentBlock(pvEnvBlockProfile);
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
        }
        else
            rc = VERR_SYMBOL_NOT_FOUND;
    }
    /*
     * We we've got an incoming change record, combine it with the default environment.
     */
    else if (hEnv != RTENV_DEFAULT && (fFlags & RTPROC_FLAGS_ENV_CHANGE_RECORD))
    {
        rc = RTEnvClone(phEnv, RTENV_DEFAULT);
        if (RT_SUCCESS(rc))
        {
            rc = RTEnvApplyChanges(*phEnv, hEnv);
            if (RT_FAILURE(rc))
                RTEnvDestroy(*phEnv);
        }
    }
    /*
     * Otherwise we can return the incoming environment directly.
     */
    else
    {
        *phEnv = hEnv;
        rc = VINF_SUCCESS;
    }

    return rc;
}


/**
 * Method \#2.
 */
static int rtProcWinCreateAsUser2(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 *ppwszExec, PRTUTF16 pwszCmdLine,
                                  RTENV hEnv, DWORD dwCreationFlags,
                                  STARTUPINFOW *pStartupInfo, PROCESS_INFORMATION *pProcInfo,
                                  uint32_t fFlags, const char *pszExec)
{
    /*
     * So if we want to start a process from a service (RTPROC_FLAGS_SERVICE),
     * we have to do the following:
     * - Check the credentials supplied and get the user SID.
     * - If valid get the correct Explorer/VBoxTray instance corresponding to that
     *   user. This of course is only possible if that user is logged in (over
     *   physical console or terminal services).
     * - If we found the user's Explorer/VBoxTray app, use and modify the token to
     *   use it in order to allow the newly started process to access the user's
     *   desktop. If there's no Explorer/VBoxTray app we cannot display the started
     *   process (but run it without UI).
     *
     * The following restrictions apply:
     * - A process only can show its UI when the user the process should run
     *   under is logged in (has a desktop).
     * - We do not want to display a process of user A run on the desktop
     *   of user B on multi session systems.
     *
     * The following rights are needed in order to use LogonUserW and
     * CreateProcessAsUserW, so the local policy has to be modified to:
     *  - SE_TCB_NAME = Act as part of the operating system
     *  - SE_ASSIGNPRIMARYTOKEN_NAME = Create/replace a token object
     *  - SE_INCREASE_QUOTA_NAME
     *
     * We may fail here with ERROR_PRIVILEGE_NOT_HELD.
     */
/** @todo r=bird: Both methods starts with rtProcWinUserLogon, so we could probably do that once in the parent function, right... */
    DWORD   dwErr       = NO_ERROR;
    HANDLE  hTokenLogon = INVALID_HANDLE_VALUE;
    int rc = rtProcWinUserLogon(pwszUser, pwszPassword, NULL /* Domain */, &hTokenLogon);
    if (RT_SUCCESS(rc))
    {
        DWORD  fRc;
        bool   fFound = false;
        HANDLE hTokenUserDesktop = INVALID_HANDLE_VALUE;

        if (fFlags & RTPROC_FLAGS_SERVICE)
        {
            /* Try query the SID and domain sizes first. */
            DWORD           cbSid      = 0; /* Must be zero to query size! */
            DWORD           cwcDomain  = 0;
            SID_NAME_USE    SidNameUse = SidTypeUser;
            fRc = LookupAccountNameW(NULL, pwszUser, NULL, &cbSid, NULL, &cwcDomain, &SidNameUse);

            /* Allocate memory for the LookupAccountNameW output buffers and do it for real. */
            cbSid = fRc && cbSid != 0 ? cbSid + 16 : _1K;
            PSID pSid = (PSID)RTMemAllocZ(cbSid);
            if (pSid)
            {
                cwcDomain = fRc ? cwcDomain + 2 : _512K;
                PRTUTF16 pwszDomain = (PRTUTF16)RTMemAllocZ(cwcDomain * sizeof(RTUTF16));
                if (pwszDomain)
                {
                    /* Note: Also supports FQDNs! */
                    if (   LookupAccountNameW(NULL /*lpSystemName*/, pwszUser, pSid, &cbSid, pwszDomain, &cwcDomain, &SidNameUse)
                        && IsValidSid(pSid))
                    {
                        /* Array of process names we want to look for. */
                        static const char * const s_papszProcNames[] =
                        {
#ifdef VBOX                 /* The explorer entry is a fallback in case GA aren't installed. */
                            { "VBoxTray.exe" },
#endif
                            { "explorer.exe" },
                            NULL
                        };
                        fFound = rtProcWinFindTokenByProcess(s_papszProcNames, pSid, &hTokenUserDesktop);
                    }
                    else
                        dwErr = GetLastError() != NO_ERROR ? GetLastError() : ERROR_INTERNAL_ERROR;
                    RTMemFree(pwszDomain);
                }
                RTMemFree(pSid);
            }
        }
        /* else: !RTPROC_FLAGS_SERVICE: Nothing to do here right now. */

        /** @todo Hmm, this function already is too big! We need to split
         *        it up into several small parts. */

        /* If we got an error due to account lookup/loading above, don't
         * continue here. */
        if (dwErr == NO_ERROR)
        {
            /*
             * If we didn't find a matching VBoxTray, just use the token we got
             * above from LogonUserW().  This enables us to at least run processes
             * with desktop interaction without UI.
             */
            HANDLE hTokenToUse = fFound ? hTokenUserDesktop : hTokenLogon;
            if (   !(fFlags & RTPROC_FLAGS_PROFILE)
                || (g_pfnUnloadUserProfile && g_pfnLoadUserProfileW) )
            {
                /*
                 * Load the profile, if requested.  (Must be done prior to
                 * creating the enviornment.)
                 */
                PROFILEINFOW ProfileInfo;
                RT_ZERO(ProfileInfo);
                if (fFlags & RTPROC_FLAGS_PROFILE)
                {
                    ProfileInfo.dwSize     = sizeof(ProfileInfo);
                    ProfileInfo.lpUserName = pwszUser;
                    ProfileInfo.dwFlags    = PI_NOUI; /* Prevents the display of profile error messages. */

                    if (!g_pfnLoadUserProfileW(hTokenToUse, &ProfileInfo))
                        dwErr = GetLastError();
                }
                if (dwErr == NO_ERROR)
                {
                    /*
                     * Create the environment.
                     */
                    RTENV hEnvFinal;
                    rc = rtProcWinCreateEnvFromToken(hTokenToUse, hEnv, fFlags, &hEnvFinal);
                    if (RT_SUCCESS(rc))
                    {
                        PRTUTF16 pwszzBlock;
                        rc = RTEnvQueryUtf16Block(hEnvFinal, &pwszzBlock);
                        if (RT_SUCCESS(rc))
                        {
                            rc = rtProcWinFindExe(fFlags, hEnv, pszExec, ppwszExec);
                            if (RT_SUCCESS(rc))
                            {
                                /*
                                 * Useful KB articles:
                                 *      http://support.microsoft.com/kb/165194/
                                 *      http://support.microsoft.com/kb/184802/
                                 *      http://support.microsoft.com/kb/327618/
                                 */
                                fRc = CreateProcessAsUserW(hTokenToUse,
                                                           *ppwszExec,
                                                           pwszCmdLine,
                                                           NULL,         /* pProcessAttributes */
                                                           NULL,         /* pThreadAttributes */
                                                           TRUE,         /* fInheritHandles */
                                                           dwCreationFlags,
                                                           /** @todo Warn about exceeding 8192 bytes
                                                            *        on XP and up. */
                                                           pwszzBlock,   /* lpEnvironment */
                                                           NULL,         /* pCurrentDirectory */
                                                           pStartupInfo,
                                                           pProcInfo);
                                if (fRc)
                                    dwErr = NO_ERROR;
                                else
                                    dwErr = GetLastError(); /* CreateProcessAsUserW() failed. */
                            }
                            RTEnvFreeUtf16Block(pwszzBlock);
                        }

                        if (hEnvFinal != hEnv)
                            RTEnvDestroy(hEnvFinal);
                    }

                    if ((fFlags & RTPROC_FLAGS_PROFILE) && ProfileInfo.hProfile)
                    {
                        fRc = g_pfnUnloadUserProfile(hTokenToUse, ProfileInfo.hProfile);
#ifdef RT_STRICT
                        if (!fRc)
                        {
                            DWORD dwErr2 = GetLastError();
                            AssertMsgFailed(("Unloading user profile failed with error %u (%#x) - Are all handles closed? (dwErr=%u)",
                                             dwErr2, dwErr2, dwErr));
                        }
#endif
                    }
                }
            }
            else
                rc = VERR_SYMBOL_NOT_FOUND;
        } /* Account lookup succeeded? */

        if (hTokenUserDesktop != INVALID_HANDLE_VALUE)
            CloseHandle(hTokenUserDesktop);
        CloseHandle(hTokenLogon);

        /*
         * Do error conversion.
         */
        if (   RT_SUCCESS(rc)
            && dwErr != NO_ERROR)
        {
            rc = rtProcWinMapErrorCodes(dwErr);
            if (rc == VERR_UNRESOLVED_ERROR)
                LogRelFunc(("dwErr=%u (%#x), rc=%Rrc\n", dwErr, dwErr, rc));
        }
    }
    return rc;
}


/**
 * Method \#1.
 *
 * This may fail on too old (NT4) platforms or if the calling process
 * is running on a SYSTEM account (like a service, ERROR_ACCESS_DENIED) on newer
 * platforms (however, this works on W2K!).
 */
static int rtProcWinCreateAsUser1(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 *ppwszExec, PRTUTF16 pwszCmdLine,
                                  RTENV hEnv, DWORD dwCreationFlags,
                                  STARTUPINFOW *pStartupInfo, PROCESS_INFORMATION *pProcInfo,
                                  uint32_t fFlags, const char *pszExec)
{
    if (!g_pfnCreateProcessWithLogonW)
        return VERR_SYMBOL_NOT_FOUND;

    RTENV  hEnvToUse = NIL_RTENV;
    HANDLE hToken;
    int rc = rtProcWinUserLogon(pwszUser, pwszPassword, NULL /* Domain */, &hToken);
    if (RT_SUCCESS(rc))
    {
/** @todo r=bird: Why didn't we load the environment here?  The
 *       CreateEnvironmentBlock docs indicate that USERPROFILE isn't set
 *       unless we call LoadUserProfile first.  However, experiments here on W10
 *       shows it isn't really needed though. Weird. */
#if 0
        if (fFlags & RTPROC_FLAGS_PROFILE)
        {
            PROFILEINFOW ProfileInfo;
            RT_ZERO(ProfileInfo);
            ProfileInfo.dwSize     = sizeof(ProfileInfo);
            ProfileInfo.lpUserName = pwszUser;
            ProfileInfo.dwFlags    = PI_NOUI; /* Prevents the display of profile error messages. */

            if (g_pfnLoadUserProfileW(hToken, &ProfileInfo))
            {
                rc = rtProcWinCreateEnvFromToken(hToken, hEnv, fFlags, &hEnvToUse);

                if (!g_pfnUnloadUserProfile(hToken, ProfileInfo.hProfile))
                    AssertFailed();
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
        }
        else
#endif
            rc = rtProcWinCreateEnvFromToken(hToken, hEnv, fFlags, &hEnvToUse);
        CloseHandle(hToken);
    }
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszzBlock;
        rc = RTEnvQueryUtf16Block(hEnvToUse, &pwszzBlock);
        if (RT_SUCCESS(rc))
        {
            rc = rtProcWinFindExe(fFlags, hEnv, pszExec, ppwszExec);
            if (RT_SUCCESS(rc))
            {
                BOOL fRc = g_pfnCreateProcessWithLogonW(pwszUser,
                                                        NULL,                       /* lpDomain*/
                                                        pwszPassword,
/** @todo r=bird: Not respecting the RTPROF_FLAGS_PROFILE flag here.  */
                                                        1 /*LOGON_WITH_PROFILE*/,   /* dwLogonFlags */
                                                        *ppwszExec,
                                                        pwszCmdLine,
                                                        dwCreationFlags,
                                                        pwszzBlock,
                                                        NULL,                       /* pCurrentDirectory */
                                                        pStartupInfo,
                                                        pProcInfo);
                if (fRc)
                    rc = VINF_SUCCESS;
                else
                {
                    DWORD dwErr = GetLastError();
                    rc = rtProcWinMapErrorCodes(dwErr);
                    if (rc == VERR_UNRESOLVED_ERROR)
                        LogRelFunc(("g_pfnCreateProcessWithLogonW (%p) failed: dwErr=%u (%#x), rc=%Rrc\n",
                                    g_pfnCreateProcessWithLogonW, dwErr, dwErr, rc));
                }
            }
            RTEnvFreeUtf16Block(pwszzBlock);
        }
        if (hEnvToUse != hEnv)
            RTEnvDestroy(hEnvToUse);
    }
    return rc;
}


static int rtProcWinCreateAsUser(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 *ppwszExec, PRTUTF16 pwszCmdLine,
                                 RTENV hEnv, DWORD dwCreationFlags,
                                 STARTUPINFOW *pStartupInfo, PROCESS_INFORMATION *pProcInfo,
                                 uint32_t fFlags, const char *pszExec)
{
    /*
     * If we run as a service CreateProcessWithLogon will fail, so don't even
     * try it (because of Local System context).   This method may not work on
     * older OSes, it will fail and we try the next alternative.
     */
    if (!(fFlags & RTPROC_FLAGS_SERVICE))
    {
        int rc = rtProcWinCreateAsUser1(pwszUser, pwszPassword, ppwszExec, pwszCmdLine,
                                        hEnv, dwCreationFlags, pStartupInfo, pProcInfo, fFlags, pszExec);
        if (RT_SUCCESS(rc))
            return rc;
    }
    return rtProcWinCreateAsUser2(pwszUser, pwszPassword, ppwszExec, pwszCmdLine,
                                  hEnv, dwCreationFlags, pStartupInfo, pProcInfo, fFlags, pszExec);
}


/**
 * RTPathTraverseList callback used by rtProcWinFindExe to locate the
 * executable.
 */
static DECLCALLBACK(int) rtPathFindExec(char const *pchPath, size_t cchPath, void *pvUser1, void *pvUser2)
{
    const char *pszExec     = (const char *)pvUser1;
    char       *pszRealExec = (char *)pvUser2;
    int rc = RTPathJoinEx(pszRealExec, RTPATH_MAX, pchPath, cchPath, pszExec, RTSTR_MAX);
    if (RT_FAILURE(rc))
        return rc;
    if (RTFileExists(pszRealExec))
        return VINF_SUCCESS;
    return VERR_TRY_AGAIN;
}


/**
 * Locate the executable file if necessary.
 *
 * @returns IPRT status code.
 * @param   pszExec         The UTF-8 executable string passed in by the user.
 * @param   fFlags          The process creation flags pass in by the user.
 * @param   hEnv            The environment to get the path variabel from.
 * @param   ppwszExec       Pointer to the variable pointing to the UTF-16
 *                          converted string.  If we find something, the current
 *                          pointer will be free (RTUtf16Free) and
 *                          replaced by a new one.
 */
static int rtProcWinFindExe(uint32_t fFlags, RTENV hEnv, const char *pszExec, PRTUTF16 *ppwszExec)
{
    /*
     * Return immediately if we're not asked to search, or if the file has a
     * path already or if it actually exists in the current directory.
     */
    if (   !(fFlags & RTPROC_FLAGS_SEARCH_PATH)
        || RTPathHavePath(pszExec)
        || RTPathExists(pszExec) )
        return VINF_SUCCESS;

    /*
     * Search the Path or PATH variable for the file.
     */
    char *pszPath;
    if (RTEnvExistEx(hEnv, "PATH"))
        pszPath = RTEnvDupEx(hEnv, "PATH");
    else if (RTEnvExistEx(hEnv, "Path"))
        pszPath = RTEnvDupEx(hEnv, "Path");
    else
        return VERR_FILE_NOT_FOUND;

    char szRealExec[RTPATH_MAX];
    int rc = RTPathTraverseList(pszPath, ';', rtPathFindExec, (void *)pszExec, &szRealExec[0]);
    RTStrFree(pszPath);
    if (RT_SUCCESS(rc))
    {
        /*
         * Replace the executable string.
         */
        RTUtf16Free(*ppwszExec);
        *ppwszExec = NULL;
        rc = RTStrToUtf16(szRealExec, ppwszExec);
    }
    else if (rc == VERR_END_OF_STRING)
        rc = VERR_FILE_NOT_FOUND;
    return rc;
}


/**
 * Creates the UTF-16 environment block and, if necessary, find the executable.
 *
 * @returns IPRT status code.
 * @param   fFlags          The process creation flags pass in by the user.
 * @param   hEnv            The environment handle passed by the user.
 * @param   pszExec         See rtProcWinFindExe.
 * @param   ppwszzBlock     Where RTEnvQueryUtf16Block returns the block.
 * @param   ppwszExec       See rtProcWinFindExe.
 */
static int rtProcWinCreateEnvBlockAndFindExe(uint32_t fFlags, RTENV hEnv, const char *pszExec,
                                             PRTUTF16 *ppwszzBlock, PRTUTF16 *ppwszExec)
{
    int rc;

    /*
     * In most cases, we just need to convert the incoming enviornment to a
     * UTF-16 environment block.
     */
    RTENV hEnvToUse;
    if (   !(fFlags & (RTPROC_FLAGS_PROFILE | RTPROC_FLAGS_ENV_CHANGE_RECORD))
        || (hEnv == RTENV_DEFAULT && !(fFlags & RTPROC_FLAGS_PROFILE))
        || (hEnv != RTENV_DEFAULT && !(fFlags & RTPROC_FLAGS_ENV_CHANGE_RECORD)) )
        hEnvToUse = hEnv;
    else if (fFlags & RTPROC_FLAGS_PROFILE)
    {
        /*
         * We need to get the profile environment for the current user.
         */
        Assert((fFlags & RTPROC_FLAGS_ENV_CHANGE_RECORD) || hEnv == RTENV_DEFAULT);
        AssertReturn(g_pfnCreateEnvironmentBlock && g_pfnDestroyEnvironmentBlock, VERR_SYMBOL_NOT_FOUND);
        AssertReturn(g_pfnLoadUserProfileW && g_pfnUnloadUserProfile, VERR_SYMBOL_NOT_FOUND);
        HANDLE hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &hToken))
        {
            rc = rtProcWinCreateEnvFromToken(hToken, hEnv, fFlags, &hEnvToUse);
            CloseHandle(hToken);
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else
    {
        /*
         * Apply hEnv as a change record on top of the default environment.
         */
        Assert(fFlags & RTPROC_FLAGS_ENV_CHANGE_RECORD);
        rc = RTEnvClone(&hEnvToUse, RTENV_DEFAULT);
        if (RT_SUCCESS(rc))
        {
            rc = RTEnvApplyChanges(hEnvToUse, hEnv);
            if (RT_FAILURE(rc))
                RTEnvDestroy(hEnvToUse);
        }
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Query the UTF-16 environment block and locate the executable (if needed).
         */
        rc = RTEnvQueryUtf16Block(hEnvToUse, ppwszzBlock);
        if (RT_SUCCESS(rc))
            rc = rtProcWinFindExe(fFlags, hEnvToUse, pszExec, ppwszExec);

        if (hEnvToUse != hEnv)
            RTEnvDestroy(hEnvToUse);
    }

    return rc;
}


RTR3DECL(int)   RTProcCreateEx(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                               PCRTHANDLE phStdIn, PCRTHANDLE phStdOut, PCRTHANDLE phStdErr, const char *pszAsUser,
                               const char *pszPassword, PRTPROCESS phProcess)
{
    /*
     * Input validation
     */
    AssertPtrReturn(pszExec, VERR_INVALID_POINTER);
    AssertReturn(*pszExec, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~RTPROC_FLAGS_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & RTPROC_FLAGS_DETACHED) || !phProcess, VERR_INVALID_PARAMETER);
    AssertReturn(hEnv != NIL_RTENV, VERR_INVALID_PARAMETER);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszAsUser, VERR_INVALID_POINTER);
    AssertReturn(!pszAsUser || *pszAsUser, VERR_INVALID_PARAMETER);
    AssertReturn(!pszPassword || pszAsUser, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszPassword, VERR_INVALID_POINTER);

    /*
     * Initialize the globals.
     */
    int rc = RTOnce(&g_rtProcWinInitOnce, rtProcWinInitOnce, NULL);
    AssertRCReturn(rc, rc);
    if (pszAsUser || (fFlags & (RTPROC_FLAGS_PROFILE | RTPROC_FLAGS_SERVICE)))
    {
        rc = RTOnce(&g_rtProcWinResolveOnce, rtProcWinResolveOnce, NULL);
        AssertRCReturn(rc, rc);
    }

    /*
     * Get the file descriptors for the handles we've been passed.
     *
     * It seems there is no point in trying to convince a child process's CRT
     * that any of the standard file handles is non-TEXT.  So, we don't...
     */
    STARTUPINFOW StartupInfo;
    RT_ZERO(StartupInfo);
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags   = STARTF_USESTDHANDLES;
#if 1 /* The CRT should keep the standard handles up to date. */
    StartupInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    StartupInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
#else
    StartupInfo.hStdInput  = _get_osfhandle(0);
    StartupInfo.hStdOutput = _get_osfhandle(1);
    StartupInfo.hStdError  = _get_osfhandle(2);
#endif
    /* If we want to have a hidden process (e.g. not visible to
     * to the user) use the STARTUPINFO flags. */
    if (fFlags & RTPROC_FLAGS_HIDDEN)
    {
        StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
        StartupInfo.wShowWindow = SW_HIDE;
    }

    PCRTHANDLE  paHandles[3] = { phStdIn, phStdOut, phStdErr };
    HANDLE     *aphStds[3]   = { &StartupInfo.hStdInput, &StartupInfo.hStdOutput, &StartupInfo.hStdError };
    DWORD       afInhStds[3] = { 0xffffffff, 0xffffffff, 0xffffffff };
    for (int i = 0; i < 3; i++)
    {
        if (paHandles[i])
        {
            AssertPtrReturn(paHandles[i], VERR_INVALID_POINTER);
            switch (paHandles[i]->enmType)
            {
                case RTHANDLETYPE_FILE:
                    *aphStds[i] = paHandles[i]->u.hFile != NIL_RTFILE
                                ? (HANDLE)RTFileToNative(paHandles[i]->u.hFile)
                                : INVALID_HANDLE_VALUE;
                    break;

                case RTHANDLETYPE_PIPE:
                    *aphStds[i] = paHandles[i]->u.hPipe != NIL_RTPIPE
                                ? (HANDLE)RTPipeToNative(paHandles[i]->u.hPipe)
                                : INVALID_HANDLE_VALUE;
                    break;

                case RTHANDLETYPE_SOCKET:
                    *aphStds[i] = paHandles[i]->u.hSocket != NIL_RTSOCKET
                                ? (HANDLE)RTSocketToNative(paHandles[i]->u.hSocket)
                                : INVALID_HANDLE_VALUE;
                    break;

                default:
                    AssertMsgFailedReturn(("%d: %d\n", i, paHandles[i]->enmType), VERR_INVALID_PARAMETER);
            }

            /* Get the inheritability of the handle. */
            if (*aphStds[i] != INVALID_HANDLE_VALUE)
            {
                if (!GetHandleInformation(*aphStds[i], &afInhStds[i]))
                {
                    rc = RTErrConvertFromWin32(GetLastError());
                    AssertMsgFailedReturn(("%Rrc %p\n", rc, *aphStds[i]), rc);
                }
            }
        }
    }

    /*
     * Set the inheritability any handles we're handing the child.
     */
    rc = VINF_SUCCESS;
    for (int i = 0; i < 3; i++)
        if (    (afInhStds[i] != 0xffffffff)
            &&  !(afInhStds[i] & HANDLE_FLAG_INHERIT))
        {
            if (!SetHandleInformation(*aphStds[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
            {
                rc = RTErrConvertFromWin32(GetLastError());
                AssertMsgFailedBreak(("%Rrc %p\n", rc, *aphStds[i]));
            }
        }

    /*
     * Create the command line and convert the executable name.
     */
    PRTUTF16 pwszCmdLine;
    if (RT_SUCCESS(rc))
        rc = RTGetOptArgvToUtf16String(&pwszCmdLine, papszArgs,
                                       !(fFlags & RTPROC_FLAGS_UNQUOTED_ARGS)
                                       ? RTGETOPTARGV_CNV_QUOTE_MS_CRT : RTGETOPTARGV_CNV_UNQUOTED);
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszExec;
        rc = RTStrToUtf16(pszExec, &pwszExec);
        if (RT_SUCCESS(rc))
        {
            /*
             * Get going...
             */
            PROCESS_INFORMATION ProcInfo;
            RT_ZERO(ProcInfo);
            DWORD dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
            if (fFlags & RTPROC_FLAGS_DETACHED)
                dwCreationFlags |= DETACHED_PROCESS;
            if (fFlags & RTPROC_FLAGS_NO_WINDOW)
                dwCreationFlags |= CREATE_NO_WINDOW;

            /*
             * Only use the normal CreateProcess stuff if we have no user name
             * and we are not running from a (Windows) service. Otherwise use
             * the more advanced version in rtProcWinCreateAsUser().
             */
            if (   pszAsUser == NULL
                && !(fFlags & RTPROC_FLAGS_SERVICE))
            {
                /* Create the environment block first. */
                PRTUTF16 pwszzBlock;
                rc = rtProcWinCreateEnvBlockAndFindExe(fFlags, hEnv, pszExec, &pwszzBlock, &pwszExec);
                if (RT_SUCCESS(rc))
                {
                    if (CreateProcessW(pwszExec,
                                       pwszCmdLine,
                                       NULL,         /* pProcessAttributes */
                                       NULL,         /* pThreadAttributes */
                                       TRUE,         /* fInheritHandles */
                                       dwCreationFlags,
                                       pwszzBlock,
                                       NULL,          /* pCurrentDirectory */
                                       &StartupInfo,
                                       &ProcInfo))
                        rc = VINF_SUCCESS;
                    else
                        rc = RTErrConvertFromWin32(GetLastError());
                    RTEnvFreeUtf16Block(pwszzBlock);
                }
            }
            else
            {
                /*
                 * Convert the additional parameters and use a helper
                 * function to do the actual work.
                 */
                PRTUTF16 pwszUser;
                rc = RTStrToUtf16(pszAsUser, &pwszUser);
                if (RT_SUCCESS(rc))
                {
                    PRTUTF16 pwszPassword;
                    rc = RTStrToUtf16(pszPassword ? pszPassword : "", &pwszPassword);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rtProcWinCreateAsUser(pwszUser, pwszPassword,
                                                   &pwszExec, pwszCmdLine, hEnv, dwCreationFlags,
                                                   &StartupInfo, &ProcInfo, fFlags, pszExec);

                        if (pwszPassword && *pwszPassword)
                            RTMemWipeThoroughly(pwszPassword, RTUtf16Len(pwszPassword), 5);
                        RTUtf16Free(pwszPassword);
                    }
                    RTUtf16Free(pwszUser);
                }
            }
            if (RT_SUCCESS(rc))
            {
                CloseHandle(ProcInfo.hThread);
                if (phProcess)
                {
                    /*
                     * Add the process to the child process list so
                     * RTProcWait can reuse and close the process handle.
                     */
                    rtProcWinAddPid(ProcInfo.dwProcessId, ProcInfo.hProcess);
                    *phProcess = ProcInfo.dwProcessId;
                }
                else
                    CloseHandle(ProcInfo.hProcess);
                rc = VINF_SUCCESS;
            }
            RTUtf16Free(pwszExec);
        }
        RTUtf16Free(pwszCmdLine);
    }

    /* Undo any handle inherit changes. */
    for (int i = 0; i < 3; i++)
        if (    (afInhStds[i] != 0xffffffff)
            &&  !(afInhStds[i] & HANDLE_FLAG_INHERIT))
        {
            if (!SetHandleInformation(*aphStds[i], HANDLE_FLAG_INHERIT, 0))
                AssertMsgFailed(("%Rrc %p\n", RTErrConvertFromWin32(GetLastError()), *aphStds[i]));
        }

    return rc;
}



RTR3DECL(int) RTProcWait(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    AssertReturn(!(fFlags & ~(RTPROCWAIT_FLAGS_BLOCK | RTPROCWAIT_FLAGS_NOBLOCK)), VERR_INVALID_PARAMETER);
    int rc = RTOnce(&g_rtProcWinInitOnce, rtProcWinInitOnce, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Try find the process among the ones we've spawned, otherwise, attempt
     * opening the specified process.
     */
    HANDLE hOpenedProc = NULL;
    HANDLE hProcess = rtProcWinFindPid(Process);
    if (hProcess == NULL)
    {
        hProcess = hOpenedProc = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, Process);
        if (hProcess == NULL)
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_INVALID_PARAMETER)
                return VERR_PROCESS_NOT_FOUND;
            return RTErrConvertFromWin32(dwErr);
        }
    }

    /*
     * Wait for it to terminate.
     */
    DWORD Millies = fFlags == RTPROCWAIT_FLAGS_BLOCK ? INFINITE : 0;
    DWORD WaitRc = WaitForSingleObjectEx(hProcess, Millies, TRUE);
    while (WaitRc == WAIT_IO_COMPLETION)
        WaitRc = WaitForSingleObjectEx(hProcess, Millies, TRUE);
    switch (WaitRc)
    {
        /*
         * It has terminated.
         */
        case WAIT_OBJECT_0:
        {
            DWORD dwExitCode;
            if (GetExitCodeProcess(hProcess, &dwExitCode))
            {
                /** @todo the exit code can be special statuses. */
                if (pProcStatus)
                {
                    pProcStatus->enmReason = RTPROCEXITREASON_NORMAL;
                    pProcStatus->iStatus = (int)dwExitCode;
                }
                if (hOpenedProc == NULL)
                    rtProcWinRemovePid(Process);
                rc = VINF_SUCCESS;
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());
            break;
        }

        /*
         * It hasn't terminated just yet.
         */
        case WAIT_TIMEOUT:
            rc = VERR_PROCESS_RUNNING;
            break;

        /*
         * Something went wrong...
         */
        case WAIT_FAILED:
            rc = RTErrConvertFromWin32(GetLastError());
            break;

        case WAIT_ABANDONED:
            AssertFailed();
            rc = VERR_GENERAL_FAILURE;
            break;

        default:
            AssertMsgFailed(("WaitRc=%RU32\n", WaitRc));
            rc = VERR_GENERAL_FAILURE;
            break;
    }

    if (hOpenedProc != NULL)
        CloseHandle(hOpenedProc);
    return rc;
}


RTR3DECL(int) RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    /** @todo this isn't quite right. */
    return RTProcWait(Process, fFlags, pProcStatus);
}


RTR3DECL(int) RTProcTerminate(RTPROCESS Process)
{
    if (Process == NIL_RTPROCESS)
        return VINF_SUCCESS;

    int rc = RTOnce(&g_rtProcWinInitOnce, rtProcWinInitOnce, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Try find the process among the ones we've spawned, otherwise, attempt
     * opening the specified process.
     */
    HANDLE hProcess = rtProcWinFindPid(Process);
    if (hProcess != NULL)
    {
        if (!TerminateProcess(hProcess, 127))
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else
    {
        hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, Process);
        if (hProcess != NULL)
        {
            BOOL  fRc   = TerminateProcess(hProcess, 127);
            DWORD dwErr = GetLastError();
            CloseHandle(hProcess);
            if (!fRc)
                rc = RTErrConvertFromWin32(dwErr);
        }
    }
    return rc;
}


RTR3DECL(uint64_t) RTProcGetAffinityMask(void)
{
    DWORD_PTR dwProcessAffinityMask = 0xffffffff;
    DWORD_PTR dwSystemAffinityMask;

    BOOL fRc = GetProcessAffinityMask(GetCurrentProcess(), &dwProcessAffinityMask, &dwSystemAffinityMask);
    Assert(fRc);

    return dwProcessAffinityMask;
}


RTR3DECL(int) RTProcQueryUsername(RTPROCESS hProcess, char *pszUser, size_t cbUser,
                                  size_t *pcbUser)
{
    AssertReturn(   (pszUser && cbUser > 0)
                 || (!pszUser && !cbUser), VERR_INVALID_PARAMETER);

    if (hProcess != RTProcSelf())
        return VERR_NOT_SUPPORTED;

    RTUTF16 awszUserName[UNLEN + 1];
    DWORD   cchUserName = UNLEN + 1;

    if (!GetUserNameW(&awszUserName[0], &cchUserName))
        return RTErrConvertFromWin32(GetLastError());

    char *pszUserName = NULL;
    int rc = RTUtf16ToUtf8(awszUserName, &pszUserName);
    if (RT_SUCCESS(rc))
    {
        size_t cbUserName = strlen(pszUserName) + 1;

        if (pcbUser)
            *pcbUser = cbUserName;

        if (cbUserName > cbUser)
            rc = VERR_BUFFER_OVERFLOW;
        else
        {
            memcpy(pszUser, pszUserName, cbUserName);
            rc = VINF_SUCCESS;
        }

        RTStrFree(pszUserName);
    }

    return rc;
}

