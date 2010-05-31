/* $Id$ */
/** @file
 * IPRT - Process, Windows.
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
#define LOG_GROUP RTLOGGROUP_PROCESS

#include <Userenv.h>
#include <Windows.h>
#include <tlhelp32.h>
#include <process.h>
#include <errno.h>

#include <iprt/process.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/socket.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef WINADVAPI BOOL WINAPI FNCREATEPROCESSWITHLOGON(LPCWSTR,
                                                       LPCWSTR,
                                                       LPCWSTR,
                                                       DWORD,
                                                       LPCWSTR,
                                                       LPWSTR,
                                                       DWORD,
                                                       LPVOID,
                                                       LPCWSTR,
                                                       LPSTARTUPINFOW,
                                                       LPPROCESS_INFORMATION);
typedef FNCREATEPROCESSWITHLOGON *PFNCREATEPROCESSWITHLOGON;

typedef DWORD WINAPI FNWTSGETACTIVECONSOLESESSIONID();
typedef FNWTSGETACTIVECONSOLESESSIONID *PFNWTSGETACTIVECONSOLESESSIONID;

typedef HANDLE WINAPI FNCREATETOOLHELP32SNAPSHOT(DWORD, DWORD);
typedef FNCREATETOOLHELP32SNAPSHOT *PFNCREATETOOLHELP32SNAPSHOT;

typedef BOOL WINAPI FNPROCESS32FIRST(HANDLE, LPPROCESSENTRY32);
typedef FNPROCESS32FIRST *PFNPROCESS32FIRST;

typedef BOOL WINAPI FNPROCESS32NEXT(HANDLE, LPPROCESSENTRY32);
typedef FNPROCESS32NEXT *PFNPROCESS32NEXT;

typedef BOOL WINAPI FNENUMPROCESSES(DWORD*, DWORD, DWORD*);
typedef FNENUMPROCESSES *PFNENUMPROCESSES;

typedef DWORD FNGETMODULEBASENAME(HANDLE, HMODULE, LPTSTR, DWORD);
typedef FNGETMODULEBASENAME *PFNGETMODULEBASENAME;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
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
 * @param   pvUser1             Ignored.
 * @param   pvUser2             Ignored.
 */
static DECLCALLBACK(int32_t) rtProcWinInitOnce(void *pvUser1, void *pvUser2)
{
    NOREF(pvUser1); NOREF(pvUser2);

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
 * Removes a process from g_paProcesses.
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
            g_cProcesses--;
            uint32_t cToMove = g_cProcesses - i;
            if (cToMove)
                memmove(&g_paProcesses[i], &g_paProcesses[i + 1], cToMove * sizeof(g_paProcesses[0]));
            break;
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


RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess)
{
    return RTProcCreateEx(pszExec, papszArgs, Env, fFlags,
                          NULL, NULL, NULL,  /* standard handles */
                          NULL /*pszAsUser*/, NULL /* pszPassword*/,
                          pProcess);
}


static int rtProcGetProcessHandle(DWORD dwPID, PSID pSID, PHANDLE phToken)
{
    AssertPtr(pSID);
    AssertPtr(phToken);

    DWORD dwErr;
    BOOL fRc;
    BOOL fFound = FALSE;
    HANDLE hProc = OpenProcess(MAXIMUM_ALLOWED, TRUE, dwPID);
    if (hProc != NULL)
    {
        HANDLE hTokenProc;
        fRc = OpenProcessToken(hProc,
                               TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE |
                               TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID | TOKEN_READ | TOKEN_WRITE,
                               &hTokenProc);
        if (fRc)
        {
            DWORD dwSize = 0;
            fRc = GetTokenInformation(hTokenProc, TokenUser, NULL, 0, &dwSize);
            if (!fRc)
                dwErr = GetLastError();
            if (   !fRc
                && dwErr == ERROR_INSUFFICIENT_BUFFER
                && dwSize > 0)
            {
                PTOKEN_USER pTokenUser = (PTOKEN_USER)RTMemAlloc(dwSize);
                AssertPtrReturn(pTokenUser, VERR_NO_MEMORY);
                RT_ZERO(*pTokenUser);
                if (   GetTokenInformation(hTokenProc,
                                           TokenUser,
                                           (LPVOID)pTokenUser,
                                           dwSize,
                                           &dwSize))
                {
                    if (   IsValidSid(pTokenUser->User.Sid)
                        && EqualSid(pTokenUser->User.Sid, pSID))
                    {
                        if (DuplicateTokenEx(hTokenProc, MAXIMUM_ALLOWED,
                                             NULL, SecurityIdentification, TokenPrimary, phToken))
                        {
                            /*
                             * So we found the process instance which belongs to the user we want to
                             * to run our new process under. This duplicated token will be used for
                             * the actual CreateProcessAsUserW() call then.
                             */
                            fFound = TRUE;
                        }
                        else
                            dwErr = GetLastError();
                    }
                }
                else
                    dwErr = GetLastError();
                RTMemFree(pTokenUser);
            }
            else
                dwErr = GetLastError();
            CloseHandle(hTokenProc);
        }
        else
            dwErr = GetLastError();
        CloseHandle(hProc);
    }
    else
        dwErr = GetLastError();
    if (fFound)
        return VINF_SUCCESS;
    if (dwErr != NO_ERROR)
        return RTErrConvertFromWin32(dwErr);
    return VERR_NOT_FOUND; /* No error occured, but we didn't find the right process. */
}


static BOOL rtProcFindProcessByName(const char *pszName, PSID pSID, PHANDLE phToken)
{
    AssertPtr(pszName);
    AssertPtr(pSID);
    AssertPtr(phToken);

    DWORD dwErr = NO_ERROR;
    BOOL fFound = FALSE;

    /*
     * On modern systems (W2K+) try the Toolhelp32 API first; this is more stable
     * and reliable. Fallback to EnumProcess on NT4.
     */
    RTLDRMOD hKernel32;
    int rc = RTLdrLoad("Kernel32.dll", &hKernel32);
    if (RT_SUCCESS(rc))
    {
        PFNCREATETOOLHELP32SNAPSHOT pfnCreateToolhelp32Snapshot;
        rc = RTLdrGetSymbol(hKernel32, "CreateToolhelp32Snapshot", (void**)&pfnCreateToolhelp32Snapshot);
        if (RT_SUCCESS(rc))
        {
            PFNPROCESS32FIRST pfnProcess32First;
            rc = RTLdrGetSymbol(hKernel32, "Process32First", (void**)&pfnProcess32First);
            if (RT_SUCCESS(rc))
            {
                PFNPROCESS32NEXT pfnProcess32Next;
                rc = RTLdrGetSymbol(hKernel32, "Process32Next", (void**)&pfnProcess32Next);
                if (RT_SUCCESS(rc))
                {
                    HANDLE hSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                    if (hSnap != INVALID_HANDLE_VALUE)
                    {
                        PROCESSENTRY32 procEntry;
                        procEntry.dwSize = sizeof(PROCESSENTRY32);
                        if (pfnProcess32First(hSnap, &procEntry))
                        {
                            do
                            {
                                if (   _stricmp(procEntry.szExeFile, pszName) == 0
                                    && RT_SUCCESS(rtProcGetProcessHandle(procEntry.th32ProcessID, pSID, phToken)))
                                {
                                    fFound = TRUE;
                                }
                            } while (pfnProcess32Next(hSnap, &procEntry) && !fFound);
                        }
                        else /* Process32First */
                            dwErr = GetLastError();
                        CloseHandle(hSnap);
                    }
                    else /* hSnap =! INVALID_HANDLE_VALUE */
                        dwErr = GetLastError();
                }
            }
        }
        else /* CreateToolhelp32Snapshot / Toolhelp32 API not available. */
        {
            /*
             * NT4 needs a copy of "PSAPI.dll" (redistributed by Microsoft and not
             * part of the OS) in order to get a lookup. If we don't have this DLL
             * we are not able to get a token and therefore no UI will be visible.
             */
            RTLDRMOD hPSAPI;
            int rc = RTLdrLoad("PSAPI.dll", &hPSAPI);
            if (RT_SUCCESS(rc))
            {
                PFNENUMPROCESSES pfnEnumProcesses;
                rc = RTLdrGetSymbol(hPSAPI, "EnumProcesses", (void**)&pfnEnumProcesses);
                if (RT_SUCCESS(rc))
                {
                    PFNGETMODULEBASENAME pfnGetModuleBaseName;
                    rc = RTLdrGetSymbol(hPSAPI, "GetModuleBaseName", (void**)&pfnGetModuleBaseName);
                    if (RT_SUCCESS(rc))
                    {
                        /** @todo Retry if pBytesReturned equals cbBytes! */
                        DWORD dwPIDs[4096]; /* Should be sufficient for now. */
                        DWORD cbBytes = 0;
                        if (pfnEnumProcesses(dwPIDs, sizeof(dwPIDs), &cbBytes))
                        {
                            for (DWORD dwIdx = 0; dwIdx < cbBytes/sizeof(DWORD) && !fFound; dwIdx++)
                            {
                                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                                           FALSE, dwPIDs[dwIdx]);
                                if (hProc)
                                {
                                    char *pszProcName = NULL;
                                    DWORD dwSize = 128;
                                    do
                                    {
                                        RTMemRealloc(pszProcName, dwSize);
                                        if (pfnGetModuleBaseName(hProc, 0, pszProcName, dwSize) == dwSize)
                                            dwSize += 128;
                                    } while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);

                                    if (pszProcName)
                                    {
                                        if (   _stricmp(pszProcName, pszName) == 0
                                            && RT_SUCCESS(rtProcGetProcessHandle(dwPIDs[dwIdx], pSID, phToken)))
                                        {
                                            fFound = TRUE;
                                        }
                                    }
                                    if (pszProcName)
                                        RTStrFree(pszProcName);
                                    CloseHandle(hProc);
                                }
                            }
                        }
                        else
                            dwErr = GetLastError();
                    }
                }
            }
        }
        RTLdrClose(hKernel32);
    }
    Assert(dwErr == NO_ERROR);
    return fFound;
}


static int rtProcCreateAsUserHlp(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszExec, PRTUTF16 pwszCmdLine,
                                 PRTUTF16 pwszzBlock, DWORD dwCreationFlags,
                                 STARTUPINFOW *pStartupInfo, PROCESS_INFORMATION *pProcInfo, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    BOOL fRc = FALSE;
    DWORD dwErr = NO_ERROR;

    /*
     * If we run as a service CreateProcessWithLogon will fail,
     * so don't even try it (because of Local System context).
     */
    if (!(fFlags & RTPROC_FLAGS_SERVICE))
    {
        RTLDRMOD hAdvAPI32;
        rc = RTLdrLoad("Advapi32.dll", &hAdvAPI32);
        if (RT_SUCCESS(rc))
        {
            /*
             * This may fail on too old (NT4) platforms or if the calling process
             * is running on a SYSTEM account (like a service, ERROR_ACCESS_DENIED) on newer
             * platforms (however, this works on W2K!).
             */
            PFNCREATEPROCESSWITHLOGON pfnCreateProcessWithLogonW;
            rc = RTLdrGetSymbol(hAdvAPI32, "CreateProcessWithLogonW", (void**)&pfnCreateProcessWithLogonW);
            if (RT_SUCCESS(rc))
            {
                fRc = pfnCreateProcessWithLogonW(pwszUser,
                                                 NULL,                       /* lpDomain*/
                                                 pwszPassword,
                                                 1 /*LOGON_WITH_PROFILE*/,   /* dwLogonFlags */
                                                 pwszExec,
                                                 pwszCmdLine,
                                                 dwCreationFlags,
                                                 pwszzBlock,
                                                 NULL,                       /* pCurrentDirectory */
                                                 pStartupInfo,
                                                 pProcInfo);
                if (!fRc)
                    dwErr = GetLastError();
            }
            RTLdrClose(hAdvAPI32);
        }
    }

    /*
     * Did the API call above fail because we're running on a too old OS (NT4) or
     * we're running as a Windows service?
     */
    if (   RT_FAILURE(rc)
        || (fFlags & RTPROC_FLAGS_SERVICE))
    {
        /*
         * So if we want to start a process from a service (RTPROC_FLAGS_SERVICE),
         * we have to do the following:
         * - Check the credentials supplied and get the user SID.
         * - If valid get the correct Explorer/VBoxTray instance corresponding to that
         *   user. This of course is only possible if that user is logged in (over
         *   physical console or terminal services).
         * - If we found the user's Explorer/VBoxTray app, use and modify the token to
         *   use it in order to allow the newly started process acess the user's
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
        PHANDLE phToken = NULL;
        HANDLE hTokenLogon = INVALID_HANDLE_VALUE;
        fRc = LogonUserW(pwszUser,
                         /* 
                          * Because we have to deal with http://support.microsoft.com/kb/245683
                          * for NULL domain names when running on NT4 here, pass an empty string if so.
                          * However, passing FQDNs should work! 
                          */
                         ((DWORD)(LOBYTE(LOWORD(GetVersion()))) < 5)  /* < Windows 2000. */
                         ? L""   /* NT4 and older. */
                         : NULL, /* Windows 2000 and up. */
                         pwszPassword,
                         LOGON32_LOGON_INTERACTIVE,
                         LOGON32_PROVIDER_DEFAULT,
                         &hTokenLogon);

        BOOL fFound = FALSE;
        HANDLE hTokenUserDesktop = INVALID_HANDLE_VALUE;
        if (fRc)
        {
            if (fFlags & RTPROC_FLAGS_SERVICE)
            {
                DWORD cbName = 0; /* Must be zero to query size! */
                DWORD cbDomain = 0;
                SID_NAME_USE sidNameUse = SidTypeUser;
                fRc = LookupAccountNameW(NULL,
                                         pwszUser,
                                         NULL,
                                         &cbName,
                                         NULL,
                                         &cbDomain,
                                         &sidNameUse);
                if (!fRc)
                    dwErr = GetLastError();
                if (   !fRc
                    && dwErr == ERROR_INSUFFICIENT_BUFFER
                    && cbName > 0)
                {
                    dwErr = NO_ERROR;

                    PSID pSID = (PSID)RTMemAlloc(cbName * sizeof(wchar_t));
                    AssertPtrReturn(pSID, VERR_NO_MEMORY);

                    /** @todo No way to allocate a PRTUTF16 directly? */
                    PRTUTF16 pwszDomain = NULL;
                    if (cbDomain > 0)
                    {
                        pwszDomain = (PRTUTF16)RTMemAlloc(cbDomain * sizeof(RTUTF16));
                        AssertPtrReturn(pwszDomain, VERR_NO_MEMORY);
                    }

                    /* Note: Also supports FQDNs! */
                    if (   LookupAccountNameW(NULL,            /* lpSystemName */
                                              pwszUser,
                                              pSID,
                                              &cbName,
                                              pwszDomain,
                                              &cbDomain,
                                              &sidNameUse)
                        && IsValidSid(pSID))
                    {
                        fFound = rtProcFindProcessByName(
#ifdef VBOX
                                                         "VBoxTray.exe",
#else
                                                         "explorer.exe"
#endif
                                                         pSID, &hTokenUserDesktop);
                    }
                    else
                        dwErr = GetLastError(); /* LookupAccountNameW() failed. */
                    RTMemFree(pSID);
                    if (pwszDomain != NULL)
                        RTUtf16Free(pwszDomain);
                }
            }
            else /* !RTPROC_FLAGS_SERVICE */
            {
                /* Nothing to do here right now. */
            }

            /*
             * If we didn't find a matching VBoxTray, just use the token we got
             * above from LogonUserW(). This enables us to at least run processes with
             * desktop interaction without UI.
             */
            phToken = fFound ? &hTokenUserDesktop : &hTokenLogon;

            /*
             * Useful KB articles:
             * http://support.microsoft.com/kb/165194/
             * http://support.microsoft.com/kb/184802/
             * http://support.microsoft.com/kb/327618/
             */
            fRc = CreateProcessAsUserW(*phToken,
                                       pwszExec,
                                       pwszCmdLine,
                                       NULL,         /* pProcessAttributes */
                                       NULL,         /* pThreadAttributes */
                                       TRUE,         /* fInheritHandles */
                                       dwCreationFlags,
                                       pwszzBlock,
                                       NULL,         /* pCurrentDirectory */
                                       pStartupInfo,
                                       pProcInfo);
            if (fRc)
                dwErr = NO_ERROR;
            else
                dwErr = GetLastError(); /* CreateProcessAsUserW() failed. */

            if (hTokenUserDesktop != INVALID_HANDLE_VALUE)
                CloseHandle(hTokenUserDesktop);
            CloseHandle(hTokenLogon);
        }
        else
            dwErr = GetLastError(); /* LogonUserW() failed. */
    }

    if (dwErr != NO_ERROR)
    {
        /*
         * Map some important or much used Windows error codes
         * to our error codes.
         */
        switch (dwErr)
        {
            case ERROR_NOACCESS:
            case ERROR_PRIVILEGE_NOT_HELD:
                rc = VERR_PERMISSION_DENIED;
                break;

            case ERROR_PASSWORD_EXPIRED:            
            case ERROR_ACCOUNT_RESTRICTION: /* See: http://support.microsoft.com/kb/303846/ */
                rc = VERR_LOGON_FAILURE;
                break;

            default:
                /* Could trigger a debug assertion! */
                rc = RTErrConvertFromWin32(dwErr);
                break;
        }
    }
    else
        rc = VINF_SUCCESS;
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
    AssertReturn(!(fFlags & ~(RTPROC_FLAGS_DAEMONIZE_DEPRECATED | RTPROC_FLAGS_DETACHED | RTPROC_FLAGS_SERVICE)), VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & RTPROC_FLAGS_DETACHED) || !phProcess, VERR_INVALID_PARAMETER);
    AssertReturn(hEnv != NIL_RTENV, VERR_INVALID_PARAMETER);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszAsUser, VERR_INVALID_POINTER);
    AssertReturn(!pszAsUser || *pszAsUser, VERR_INVALID_PARAMETER);
    AssertReturn(!pszPassword || pszAsUser, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszPassword, VERR_INVALID_POINTER);
    /** @todo search the PATH (add flag for this). */

    /*
     * Initialize the globals.
     */
    int rc = RTOnce(&g_rtProcWinInitOnce, rtProcWinInitOnce, NULL, NULL);
    AssertRCReturn(rc, rc);

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
     * Create the environment block, command line and convert the executable
     * name.
     */
    PRTUTF16 pwszzBlock;
    if (RT_SUCCESS(rc))
        rc = RTEnvQueryUtf16Block(hEnv, &pwszzBlock);
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszCmdLine;
        rc = RTGetOptArgvToUtf16String(&pwszCmdLine, papszArgs, RTGETOPTARGV_CNV_QUOTE_MS_CRT);
        if (RT_SUCCESS(rc))
        {
            PRTUTF16 pwszExec;
            rc = RTStrToUtf16(pszExec, &pwszExec);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Get going...
                 */
                DWORD               dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
                if (fFlags & RTPROC_FLAGS_DETACHED)
                    dwCreationFlags |= DETACHED_PROCESS;

                PROCESS_INFORMATION ProcInfo;
                RT_ZERO(ProcInfo);

                /*
                 * Only use the normal CreateProcess stuff if we have no user name
                 * and we are not running from a (Windows) service. Otherwise use
                 * the more advanced version in rtProcCreateAsUserHlp().
                 */
                if (   pszAsUser == NULL
                    && !(fFlags & RTPROC_FLAGS_SERVICE))
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
                            rc = rtProcCreateAsUserHlp(pwszUser, pwszPassword,
                                                       pwszExec, pwszCmdLine, pwszzBlock, dwCreationFlags,
                                                       &StartupInfo, &ProcInfo, fFlags);

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
        RTEnvFreeUtf16Block(pwszzBlock);
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
    int rc = RTOnce(&g_rtProcWinInitOnce, rtProcWinInitOnce, NULL, NULL);
    AssertRCReturn(rc, rc);

    /*
     * Try find the process among the ones we've spawned, otherwise, attempt
     * opening the specified process.
     */
    HANDLE hProcess = rtProcWinFindPid(Process);
    if (hProcess == NULL)
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, Process);
    if (hProcess != NULL)
    {
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
                    rtProcWinRemovePid(Process);
                    return VINF_SUCCESS;
                }
                break;
            }

            /*
             * It hasn't terminated just yet.
             */
            case WAIT_TIMEOUT:
                return VERR_PROCESS_RUNNING;

            /*
             * Something went wrong...
             */
            case WAIT_FAILED:
                break;
            case WAIT_ABANDONED:
                AssertFailed();
                return VERR_GENERAL_FAILURE;
            default:
                AssertMsgFailed(("WaitRc=%RU32\n", WaitRc));
                return VERR_GENERAL_FAILURE;
        }
    }
    DWORD dwErr = GetLastError();
    if (dwErr == ERROR_INVALID_PARAMETER)
        return VERR_PROCESS_NOT_FOUND;
    return RTErrConvertFromWin32(dwErr);
}


RTR3DECL(int) RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    /** @todo this isn't quite right. */
    return RTProcWait(Process, fFlags, pProcStatus);
}


RTR3DECL(int) RTProcTerminate(RTPROCESS Process)
{
    int     rc       = VINF_SUCCESS;
    HANDLE  hProcess = rtProcWinFindPid(Process);
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

