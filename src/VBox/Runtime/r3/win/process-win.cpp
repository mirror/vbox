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
#include <iprt/asm.h> /* hack */

#include <Userenv.h>
#include <Windows.h>
#include <tlhelp32.h>
#include <process.h>
#include <errno.h>
#include <Strsafe.h>

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

typedef BOOL WINAPI FNCREATEENVIRONMENTBLOCK(LPVOID *, HANDLE, BOOL);
typedef FNCREATEENVIRONMENTBLOCK *PFNCREATEENVIRONMENTBLOCK;

typedef BOOL WINAPI FNPFNDESTROYENVIRONMENTBLOCK(LPVOID);
typedef FNPFNDESTROYENVIRONMENTBLOCK *PFNPFNDESTROYENVIRONMENTBLOCK;

typedef BOOL WINAPI FNLOADUSERPROFILEW(HANDLE, LPPROFILEINFOW);
typedef FNLOADUSERPROFILEW *PFNLOADUSERPROFILEW;

typedef BOOL WINAPI FNUNLOADUSERPROFILE(HANDLE, HANDLE);
typedef FNUNLOADUSERPROFILE *PFNUNLOADUSERPROFILE;


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


RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess)
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
static int rtProcMapErrorCodes(DWORD dwError)
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
            rc = VERR_AUTHENTICATION_FAILURE;
            break;

        default:
            /* Could trigger a debug assertion! */
            rc = RTErrConvertFromWin32(dwError);
            break;
    }
    return rc;
}


/**
 * Get the process token (not the process handle like the name might indicate)
 * of the process indicated by @a dwPID if the @a pSID matches.
 *
 * @returns IPRT status code.
 *
 * @param   dwPID           The process identifier.
 * @param   pSID            The secure identifier of the user.
 * @param   phToken         Where to return the token handle - duplicate,
 *                          caller closes it on success.
 */
static int rtProcGetProcessHandle(DWORD dwPID, PSID pSID, PHANDLE phToken)
{
    AssertPtr(pSID);
    AssertPtr(phToken);

    DWORD dwErr;
    BOOL fRc;
    bool fFound = false;
    HANDLE hProc = OpenProcess(MAXIMUM_ALLOWED, TRUE, dwPID);
    if (hProc != NULL)
    {
        HANDLE hTokenProc;
        fRc = OpenProcessToken(hProc,
                               TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE
                               | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_SESSIONID | TOKEN_READ | TOKEN_WRITE,
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
                AssertPtrReturn(pTokenUser, VERR_NO_MEMORY); /** @todo r=bird: Leaking handles when we're out of memory...  */
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
                            fFound = true;
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
    return VERR_NOT_FOUND; /* No error occurred, but we didn't find the right process. */
}


/**
 * Finds a one of the processes in @a papszNames running with user @a pSID and
 * returns a duplicate handle to its token.
 *
 * @returns Success indicator.
 * @param   papszNames      The process candidates, in prioritized order.
 * @param   pSid            The secure identifier of the user.
 * @param   phToken         Where to return the token handle - duplicate,
 *                          caller closes it on success.
 */
static bool rtProcFindProcessByName(const char * const *papszNames, PSID pSid, PHANDLE phToken)
{
    AssertPtr(papszNames);
    AssertPtr(pSid);
    AssertPtr(phToken);

    DWORD dwErr = NO_ERROR;
    bool fFound = false;

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
                        for (size_t i = 0; papszNames[i] && !fFound; i++)
                        {
                            PROCESSENTRY32 procEntry;
                            procEntry.dwSize = sizeof(PROCESSENTRY32);
                            if (pfnProcess32First(hSnap, &procEntry))
                            {
                                do
                                {
                                    if (   _stricmp(procEntry.szExeFile, papszNames[i]) == 0
                                        && RT_SUCCESS(rtProcGetProcessHandle(procEntry.th32ProcessID, pSid, phToken)))
                                    {
                                        fFound = true;
                                        break;
                                    }
                                } while (pfnProcess32Next(hSnap, &procEntry));
                            }
                            else /* Process32First */
                                dwErr = GetLastError();
                            if (FAILED(dwErr))
                                break;
                        }
                        CloseHandle(hSnap);
                    }
                    else /* hSnap == INVALID_HANDLE_VALUE */
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
                        DWORD adwPIDs[4096]; /* Should be sufficient for now. */
                        DWORD cbBytes = 0;
                        if (pfnEnumProcesses(adwPIDs, sizeof(adwPIDs), &cbBytes))
                        {
                            for (size_t i = 0; papszNames[i] && !fFound; i++)
                            {
                                for (DWORD dwIdx = 0; dwIdx < cbBytes/sizeof(DWORD) && !fFound; dwIdx++)
                                {
                                    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                                               FALSE, adwPIDs[dwIdx]);
                                    if (hProc)
                                    {
                                        char *pszProcName = NULL;
                                        DWORD dwSize = 128;
                                        do
                                        {
                                            RTMemRealloc(pszProcName, dwSize);
                                            if (pfnGetModuleBaseName(hProc, 0, pszProcName, dwSize) == dwSize)
                                                dwSize += 128;
                                            if (dwSize > _4K) /* Play safe. */
                                                break;
                                        } while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);

                                        if (pszProcName)
                                        {
                                            if (   _stricmp(pszProcName, papszNames[i]) == 0
                                                && RT_SUCCESS(rtProcGetProcessHandle(adwPIDs[dwIdx], pSid, phToken)))
                                            {
                                                fFound = true;
                                            }
                                        }
                                        if (pszProcName)
                                            RTStrFree(pszProcName);
                                        CloseHandle(hProc);
                                    }
                                }
                            }
                        }
                        else
                            dwErr = GetLastError();
                    }
                }
                RTLdrClose(hPSAPI);
            }
        }
        RTLdrClose(hKernel32);
    }
    Assert(dwErr == NO_ERROR);
    return fFound;
}


/**
 * Logs on a specified user and returns its primary token.
 *
 * @return  int
 *
 * @param   pwszUser            User name.
 * @param   pwszPassword        Password.
 * @param   pwszDomain          Domain (not used at the moment).
 * @param   phToken             Pointer to store the logon token.
 */
static int rtProcUserLogon(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszDomain, HANDLE *phToken)
{
    /** @todo Add domain support! */
    BOOL fRc = LogonUserW(pwszUser,
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
                          phToken);
    if (!fRc)
        return rtProcMapErrorCodes(GetLastError());
    return VINF_SUCCESS;
}


/**
 * Logs off a user, specified by the given token.
 *
 * @param   hToken      The token (=user) to log off.
 */
static void rtProcUserLogoff(HANDLE hToken)
{
    CloseHandle(hToken);
}


/**
 * Creates an environment block out of a handed in Unicode and RTENV block.
 * The RTENV block can overwrite entries already present in the Unicode block.
 *
 * @return  IPRT status code.
 *
 * @param   pvBlock         Unicode block (array) of environment entries; name=value
 * @param   hEnv            Handle of an existing RTENV block to use.
 * @param   ppwszBlock      Pointer to the final output.
 */
static int rtProcEnvironmentCreateInternal(VOID *pvBlock, RTENV hEnv, PRTUTF16 *ppwszBlock)
{
    int rc = VINF_SUCCESS;
    RTENV hEnvTemp;
    rc = RTEnvClone(&hEnvTemp, hEnv);
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pBlock = (PRTUTF16)pvBlock;
        while (   pBlock
               && pBlock != '\0'
               && RT_SUCCESS(rc))
        {
            char *pszEntry;
            rc = RTUtf16ToUtf8(pBlock, &pszEntry);
            if (RT_SUCCESS(rc))
            {
                /* Don't overwrite values which we already have set to a custom value
                 * specified in hEnv ... */
                if (!RTEnvExistEx(hEnv, pszEntry))
                    rc = RTEnvPutEx(hEnvTemp, pszEntry);
                RTStrFree(pszEntry);
            }

            size_t l;
            /* 32k should be the maximum the environment block can have on Windows. */
            if (FAILED(StringCbLengthW((LPCWSTR)pBlock, _32K * sizeof(RTUTF16), &l)))
                break;
            pBlock += l / sizeof(RTUTF16);
            if (pBlock[1] == '\0') /* Did we reach the double zero termination (\0\0)? */
                break;
            pBlock++; /* Skip zero termination of current string and advance to next string ... */
        }

        if (RT_SUCCESS(rc))
            rc = RTEnvQueryUtf16Block(hEnvTemp, ppwszBlock);
        RTEnvDestroy(hEnvTemp);
    }
    return rc;
}


/**
 * Builds up the environment block for a specified user (identified by a token),
 * whereas hEnv is an additional set of environment variables which overwrite existing
 * values of the user profile.  ppwszBlock needs to be destroyed after usage
 * calling rtProcEnvironmentDestroy().
 *
 * @return  IPRT status code.
 *
 * @param   hToken          Token of the user to use.
 * @param   hEnv            Own environment block to extend/overwrite the profile's data with.
 * @param   ppwszBlock      Pointer to a pointer of the final UTF16 environment block.
 */
static int rtProcEnvironmentCreateFromToken(HANDLE hToken, RTENV hEnv, PRTUTF16 *ppwszBlock)
{
    RTLDRMOD hUserenv;
    int rc = RTLdrLoad("Userenv.dll", &hUserenv);
    if (RT_SUCCESS(rc))
    {
        PFNCREATEENVIRONMENTBLOCK pfnCreateEnvironmentBlock;
        rc = RTLdrGetSymbol(hUserenv, "CreateEnvironmentBlock", (void**)&pfnCreateEnvironmentBlock);
        if (RT_SUCCESS(rc))
        {
            PFNPFNDESTROYENVIRONMENTBLOCK pfnDestroyEnvironmentBlock;
            rc = RTLdrGetSymbol(hUserenv, "DestroyEnvironmentBlock", (void**)&pfnDestroyEnvironmentBlock);
            if (RT_SUCCESS(rc))
            {
                LPVOID pEnvBlockProfile = NULL;
                if (pfnCreateEnvironmentBlock(&pEnvBlockProfile, hToken, FALSE /* Don't inherit from parent. */))
                {
                    rc = rtProcEnvironmentCreateInternal(pEnvBlockProfile, hEnv, ppwszBlock);
                    pfnDestroyEnvironmentBlock(pEnvBlockProfile);
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }
        }
        RTLdrClose(hUserenv);
    }
    /* If we don't have the Userenv-API for whatever reason or something with the
     * native environment block failed, try to return at least our own environment block. */
    if (RT_FAILURE(rc))
        rc = RTEnvQueryUtf16Block(hEnv, ppwszBlock);
    return rc;
}


/**
 * Builds up the environment block for a specified user (identified by user name, password
 * and domain), whereas hEnv is an additional set of environment variables which overwrite
 * existing values of the user profile.  ppwszBlock needs to be destroyed after usage
 * calling rtProcEnvironmentDestroy().
 *
 * @return  IPRT status code.
 *
 * @param   pwszUser        User name.
 * @param   pwszPassword    Password.
 * @param   pwszDomain      Domain.
 * @param   hEnv            Own environment block to extend/overwrite the profile's data with.
 * @param   ppwszBlock      Pointer to a pointer of the final UTF16 environment block.
 */
static int rtProcEnvironmentCreateFromAccount(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszDomain,
                                              RTENV hEnv, PRTUTF16 *ppwszBlock)
{
    HANDLE hToken;
    int rc = rtProcUserLogon(pwszUser, pwszPassword, pwszDomain, &hToken);
    if (RT_SUCCESS(rc))
    {
        rc = rtProcEnvironmentCreateFromToken(hToken, hEnv, ppwszBlock);
        rtProcUserLogoff(hToken);
    }
    return rc;
}


/**
 * Destroys an environment block formerly created by rtProcEnvironmentCreateInternal(),
 * rtProcEnvironmentCreateFromToken() or rtProcEnvironmentCreateFromAccount().
 *
 * @param   ppwszBlock      Environment block to destroy.
 */
static void rtProcEnvironmentDestroy(PRTUTF16 ppwszBlock)
{
    RTEnvFreeUtf16Block(ppwszBlock);
}


static int rtProcCreateAsUserHlp(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszExec, PRTUTF16 pwszCmdLine,
                                 RTENV hEnv, DWORD dwCreationFlags,
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
                PRTUTF16 pwszzBlock;
                rc = rtProcEnvironmentCreateFromAccount(pwszUser, pwszPassword, NULL /* Domain */,
                                                        hEnv, &pwszzBlock);
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
                        rc = rtProcMapErrorCodes(GetLastError());
                    rtProcEnvironmentDestroy(pwszzBlock);
                }
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
        PHANDLE phToken = NULL;
        HANDLE hTokenLogon = INVALID_HANDLE_VALUE;
        rc = rtProcUserLogon(pwszUser, pwszPassword, NULL /* Domain */, &hTokenLogon);
        if (RT_SUCCESS(rc))
        {
            bool fFound = false;
            HANDLE hTokenUserDesktop = INVALID_HANDLE_VALUE;

            if (fFlags & RTPROC_FLAGS_SERVICE)
            {
                DWORD cbSid = 0; /* Must be zero to query size! */
                DWORD cchDomain = 0;
                SID_NAME_USE sidNameUse = SidTypeUser;
                fRc = LookupAccountNameW(NULL,
                                         pwszUser,
                                         NULL,
                                         &cbSid,
                                         NULL,
                                         &cchDomain,
                                         &sidNameUse);
                if (!fRc)
                    dwErr = GetLastError();
                if (   !fRc
                    && dwErr == ERROR_INSUFFICIENT_BUFFER
                    && cbSid > 0)
                {
                    dwErr = NO_ERROR;

                    PSID pSid = (PSID)RTMemAlloc(cbSid * sizeof(wchar_t)); /** @todo r=bird: What's the relationship between wchar_t and PSID? */
                    AssertPtrReturn(pSid, VERR_NO_MEMORY); /** @todo r=bird: Leaking token handles when we're out of memory...  */

                    PRTUTF16 pwszDomain = NULL;
                    if (cchDomain > 0)
                    {
                        pwszDomain = (PRTUTF16)RTMemAlloc(cchDomain * sizeof(RTUTF16));
                        AssertPtrReturn(pwszDomain, VERR_NO_MEMORY); /** @todo r=bird: Leaking token handles when we're out of memory...  */
                    }

                    /* Note: Also supports FQDNs! */
                    if (   LookupAccountNameW(NULL,            /* lpSystemName */
                                              pwszUser,
                                              pSid,
                                              &cbSid,
                                              pwszDomain,
                                              &cchDomain,
                                              &sidNameUse)
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
                        fFound = rtProcFindProcessByName(s_papszProcNames, pSid, &hTokenUserDesktop);
                    }
                    else
                        dwErr = GetLastError(); /* LookupAccountNameW() failed. */
                    RTMemFree(pSid);
                    RTMemFree(pwszDomain);
                }
            }
            else /* !RTPROC_FLAGS_SERVICE */
            {
                /* Nothing to do here right now. */
            }

            /** @todo Hmm, this function already is too big! We need to split
             *        it up into several small parts. */

            /* If we got an error due to account lookup/loading above, don't
             * continue here. */
            if (dwErr == NO_ERROR)
            {
                /*
                 * If we didn't find a matching VBoxTray, just use the token we got
                 * above from LogonUserW(). This enables us to at least run processes with
                 * desktop interaction without UI.
                 */
                phToken = fFound ? &hTokenUserDesktop : &hTokenLogon;
                RTLDRMOD hUserenv;
                int rc = RTLdrLoad("Userenv.dll", &hUserenv);
                if (RT_SUCCESS(rc))
                {
                    PFNLOADUSERPROFILEW pfnLoadUserProfileW;
                    rc = RTLdrGetSymbol(hUserenv, "LoadUserProfileW", (void**)&pfnLoadUserProfileW);
                    if (RT_SUCCESS(rc))
                    {
                        PFNUNLOADUSERPROFILE pfnUnloadUserProfile;
                        rc = RTLdrGetSymbol(hUserenv, "UnloadUserProfile", (void**)&pfnUnloadUserProfile);
                        if (RT_SUCCESS(rc))
                        {
                            PROFILEINFOW profileInfo;
                            if (!(fFlags & RTPROC_FLAGS_NO_PROFILE))
                            {
                                RT_ZERO(profileInfo);
                                profileInfo.dwSize = sizeof(profileInfo);
                                profileInfo.lpUserName = pwszUser;
                                profileInfo.dwFlags = PI_NOUI; /* Prevents the display of profile error messages. */

                                if (!pfnLoadUserProfileW(*phToken, &profileInfo))
                                    dwErr = GetLastError();
                            }

                            if (dwErr == NO_ERROR)
                            {
                                PRTUTF16 pwszzBlock;
                                rc = rtProcEnvironmentCreateFromToken(*phToken, hEnv, &pwszzBlock);
                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                     * Useful KB articles:
                                     *      http://support.microsoft.com/kb/165194/
                                     *      http://support.microsoft.com/kb/184802/
                                     *      http://support.microsoft.com/kb/327618/
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
                                    rtProcEnvironmentDestroy(pwszzBlock);
                                }

                                if (!(fFlags & RTPROC_FLAGS_NO_PROFILE))
                                {
                                    fRc = pfnUnloadUserProfile(*phToken, profileInfo.hProfile);
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
                    }
                    RTLdrClose(hUserenv);
                } /* Userenv.dll found/loaded? */
            } /* Account lookup succeeded? */
            if (hTokenUserDesktop != INVALID_HANDLE_VALUE)
                CloseHandle(hTokenUserDesktop);
            rtProcUserLogoff(hTokenLogon);
        }
    }

    if (   RT_SUCCESS(rc)
        && dwErr != NO_ERROR)
        rc = rtProcMapErrorCodes(dwErr);

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
    AssertReturn(!(fFlags & ~(RTPROC_FLAGS_DETACHED | RTPROC_FLAGS_HIDDEN | RTPROC_FLAGS_SERVICE | RTPROC_FLAGS_SAME_CONTRACT | RTPROC_FLAGS_NO_PROFILE | RTPROC_FLAGS_NO_WINDOW)), VERR_INVALID_PARAMETER);
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
                PROCESS_INFORMATION ProcInfo;
                RT_ZERO(ProcInfo);
                DWORD               dwCreationFlags = CREATE_UNICODE_ENVIRONMENT;
                if (fFlags & RTPROC_FLAGS_DETACHED)
                    dwCreationFlags |= DETACHED_PROCESS;
                if (fFlags & RTPROC_FLAGS_NO_WINDOW)
                    dwCreationFlags |= CREATE_NO_WINDOW;

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
                                                       pwszExec, pwszCmdLine, hEnv, dwCreationFlags,
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

    int rc = RTOnce(&g_rtProcWinInitOnce, rtProcWinInitOnce, NULL, NULL);
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

