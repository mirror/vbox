/* $Id$ */
/** @file
 * IPRT - Process, Windows.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_PROCESS

#include <Userenv.h>
#include <Windows.h>
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


static int rtProcCreateAsUserHlp(PRTUTF16 pwszUser, PRTUTF16 pwszPassword, PRTUTF16 pwszExec, PRTUTF16 pwszCmdLine,
                                 PRTUTF16 pwszzBlock, DWORD dwCreationFlags,
                                 STARTUPINFOW *pStartupInfo, PROCESS_INFORMATION *pProcInfo)
{
    /** @todo On NT4 we need to enable the SeTcbPrivilege to act as part of the operating system. Otherwise
      *       we will get error 1314 (priviledge not held) as a response. */

    /*
     * The following rights are needed in order to use LogonUserW and
     * CreateProcessAsUserW, so the local policy has to be modified to:
     *  - SE_TCB_NAME = Act as part of the operating system
     *  - SE_ASSIGNPRIMARYTOKEN_NAME = Create/replace a token object
     *  - SE_INCREASE_QUOTA_NAME
     *
     * We may fail here with ERROR_PRIVILEGE_NOT_HELD.
     */
    int    rc;
    DWORD  dwErr  = NO_ERROR;
    HANDLE hToken = INVALID_HANDLE_VALUE;
    BOOL   fRc    = LogonUserW(pwszUser,
                               NULL,      /* lpDomain */
                               pwszPassword,
                               LOGON32_LOGON_INTERACTIVE,
                               LOGON32_PROVIDER_DEFAULT,
                               &hToken);
    if (fRc)
    {
        fRc = CreateProcessAsUserW(hToken,
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
        if (!fRc)
            dwErr = GetLastError();
        CloseHandle(hToken);
    }
    else
        dwErr = GetLastError();

#ifndef IPRT_TARGET_NT4
    /*
     * If we don't hold enough priviledges to spawn a new process with
     * different credentials we have to use CreateProcessWithLogonW here.  This
     * API is W2K+ and uses a system service for spawning the process.
     */
    /** @todo Use fFlags to either use this feature or just fail. */
    if (dwErr == ERROR_PRIVILEGE_NOT_HELD)
    {
        RTLDRMOD hAdvAPI32;
        rc = RTLdrLoad("Advapi32.dll", &hAdvAPI32);
        if (RT_SUCCESS(rc))
        {
            /* This may fail on too old (NT4) platforms. */
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
                if (fRc)
                    dwErr = NO_ERROR;
                else
                    dwErr = GetLastError();
            }
            RTLdrClose(hAdvAPI32);
        }
    }
#endif

    if (dwErr != NO_ERROR)
        rc = RTErrConvertFromWin32(dwErr);
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
    AssertReturn(!(fFlags & ~(RTPROC_FLAGS_DAEMONIZE_DEPRECATED | RTPROC_FLAGS_DETACHED)), VERR_INVALID_PARAMETER);
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
                if (pszAsUser == NULL)
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
                                                       pwszExec, pwszCmdLine, pwszzBlock, dwCreationFlags
                                                       &StartupInfo, &ProcInfo);

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

