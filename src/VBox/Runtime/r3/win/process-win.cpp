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

#include <Windows.h>
#include <process.h>
#include <errno.h>

#include <iprt/process.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/getopt.h>
#include <iprt/pipe.h>
#include <iprt/string.h>


/*
 * This is from Winternl.h. It has been copied here
 * because the header does not define a calling convention for
 * its prototypes and just assumes that _stdcall is the standard
 * calling convention.
 */
typedef struct _PEB
{
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[229];
    PVOID Reserved3[59];
    ULONG SessionId;
} PEB, *PPEB;

typedef struct _PROCESS_BASIC_INFORMATION
{
    PVOID Reserved1;
    PPEB PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;

typedef enum _PROCESSINFOCLASS
{
    ProcessBasicInformation = 0,
    ProcessWow64Information = 26
} PROCESSINFOCLASS;

extern "C" LONG WINAPI
NtQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

/** @todo r=michael This function currently does not work correctly if the arguments
                    contain spaces. */
RTR3DECL(int) RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszExec, VERR_INVALID_POINTER);
    AssertReturn(*pszExec, VERR_INVALID_PARAMETER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(Env != NIL_RTENV, VERR_INVALID_PARAMETER);
    const char * const *papszEnv = RTEnvGetExecEnvP(Env);
    AssertPtrReturn(papszEnv, VERR_INVALID_HANDLE);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    AssertPtrReturn(*papszArgs, VERR_INVALID_PARAMETER);
    /* later: path searching. */

    /*
     * Spawn the child.
     */
    /** @todo utf-8 considerations! */
    HANDLE hProcess = (HANDLE)_spawnve(_P_NOWAITO, pszExec, papszArgs, papszEnv);
    if (hProcess != 0 && hProcess != INVALID_HANDLE_VALUE)
    {
        if (pProcess)
        {
            /*
             * GetProcessId requires XP SP1 or later
             */
#if defined(RT_ARCH_AMD64)
            *pProcess = GetProcessId(hProcess);
#else /* !RT_ARCH_AMD64 */
            static bool           fInitialized = false;
            static DWORD (WINAPI *pfnGetProcessId)(HANDLE Thread) = NULL;
            if (!fInitialized)
            {
                HMODULE hmodKernel32 = GetModuleHandle("KERNEL32.DLL");
                if (hmodKernel32)
                    pfnGetProcessId = (DWORD (WINAPI*)(HANDLE))GetProcAddress(hmodKernel32, "GetProcessId");
                fInitialized = true;
            }
            if (pfnGetProcessId)
            {
                *pProcess = pfnGetProcessId(hProcess);
                if (!*pProcess)
                {
                    int rc = RTErrConvertFromWin32(GetLastError());
                    AssertMsgFailed(("failed to get pid from hProcess=%#x rc=%Rrc\n", hProcess, rc));
                    return rc;
                }
            }
            else
            {
                /*
                 * Fall back to the NT api for older versions.
                 */
                PROCESS_BASIC_INFORMATION ProcInfo = {0};
                ULONG Status = NtQueryInformationProcess(hProcess, ProcessBasicInformation,
                                                         &ProcInfo, sizeof(ProcInfo), NULL);
                if (Status != 0)
                {
                    int rc = ERROR_INTERNAL_ERROR; /* (we don't have a valid conversion here, but this shouldn't happen anyway.) */
                    AssertMsgFailed(("failed to get pid from hProcess=%#x rc=%Rrc Status=%#x\n", hProcess, rc, Status));
                    return rc;
                }
                *pProcess = ProcInfo.UniqueProcessId;
            }
#endif  /* !RT_ARCH_AMD64 */
        }
        return VINF_SUCCESS;
    }

    int rc = RTErrConvertFromErrno(errno);
    AssertMsgFailed(("spawn/exec failed rc=%Rrc\n", rc)); /* this migth be annoying... */
    return rc;
}


RTR3DECL(int)   RTProcCreateEx(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                               PCRTHANDLE phStdIn, PCRTHANDLE phStdOut, PCRTHANDLE phStdErr, const char *pszAsUser,
                               PRTPROCESS phProcess)
{
#if 1 /* needs more work... dinner time. */
    int rc;

    /*
     * Input validation
     */
    AssertPtrReturn(pszExec, VERR_INVALID_POINTER);
    AssertReturn(*pszExec, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~RTPROC_FLAGS_DAEMONIZE), VERR_INVALID_PARAMETER);
    AssertReturn(hEnv != NIL_RTENV, VERR_INVALID_PARAMETER);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    /** @todo search the PATH (add flag for this). */
    AssertPtrNullReturn(pszAsUser, VERR_INVALID_POINTER);

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

                //case RTHANDLETYPE_SOCKET:
                //    *aphStds[i] = paHandles[i]->u.hSocket != NIL_RTSOCKET
                //                ? (HANDLE)RTTcpToNative(paHandles[i]->u.hSocket)
                //                : INVALID_HANDLE_VALUE;
                //    break;

                default:
                    AssertMsgFailedReturn(("%d: %d\n", i, paHandles[i]->enmType), VERR_INVALID_PARAMETER);
            }
        }
    }

    /*
     * Create the environment block, command line and convert the executable
     * name.
     */
    PRTUTF16 pwszzBlock;
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
                HANDLE hToken = INVALID_HANDLE_VALUE;
                if (pszAsUser)
                {
                    /** @todo - Maybe use CreateProcessWithLoginW? That'll require a password, but
                     *        we may need that anyway because it looks like LogonUserW is the only
                     *        way to get a hToken.  FIXME */
                    rc = VERR_NOT_IMPLEMENTED;
                }
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Get going...
                     */
                    PROCESS_INFORMATION ProcInfo;
                    RT_ZERO(ProcInfo);
                    BOOL fRc;
                    if (hToken == INVALID_HANDLE_VALUE)
                        fRc = CreateProcessW(pwszExec,
                                             pwszCmdLine,
                                             NULL,         /* pProcessAttributes */
                                             NULL,         /* pThreadAttributes */
                                             TRUE,         /* fInheritHandles */
                                             CREATE_UNICODE_ENVIRONMENT, /* dwCreationFlags */
                                             pwszzBlock,
                                             NULL,          /* pCurrentDirectory */
                                             &StartupInfo,
                                             &ProcInfo);
                    else
                        fRc = CreateProcessAsUserW(hToken,
                                                   pwszExec,
                                                   pwszCmdLine,
                                                   NULL,         /* pProcessAttributes */
                                                   NULL,         /* pThreadAttributes */
                                                   TRUE,         /* fInheritHandles */
                                                   CREATE_UNICODE_ENVIRONMENT, /* dwCreationFlags */
                                                   pwszzBlock,
                                                   NULL,          /* pCurrentDirectory */
                                                   &StartupInfo,
                                                   &ProcInfo);

                    if (fRc)
                    {
                        CloseHandle(ProcInfo.hThread);
                        CloseHandle(ProcInfo.hProcess);
                        if (phProcess)
                            *phProcess = ProcInfo.dwProcessId;
                        rc = VINF_SUCCESS;
                    }
                    else
                        rc = RTErrConvertFromWin32(GetLastError());

                    if (hToken == INVALID_HANDLE_VALUE)
                        CloseHandle(hToken);
                }
                RTUtf16Free(pwszExec);
            }
            RTUtf16Free(pwszCmdLine);
        }
        RTEnvFreeUtf16Block(pwszzBlock);
    }

    return rc;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}



RTR3DECL(int) RTProcWait(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    AssertReturn(!(fFlags & ~(RTPROCWAIT_FLAGS_BLOCK | RTPROCWAIT_FLAGS_NOBLOCK)), VERR_INVALID_PARAMETER);

    /*
     * Open the process.
     */
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, Process);
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
    return RTErrConvertFromWin32(dwErr);
}


RTR3DECL(int) RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    /** @todo this isn't quite right. */
    return RTProcWait(Process, fFlags, pProcStatus);
}


RTR3DECL(int) RTProcTerminate(RTPROCESS Process)
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, Process);
    if (hProcess != NULL)
    {
        BOOL fRc = TerminateProcess(hProcess, 127);
        CloseHandle(hProcess);
        if (fRc)
            return VINF_SUCCESS;
    }
    DWORD dwErr = GetLastError();
    return RTErrConvertFromWin32(dwErr);
}


RTR3DECL(uint64_t) RTProcGetAffinityMask(void)
{
    DWORD_PTR dwProcessAffinityMask = 0xffffffff;
    DWORD_PTR dwSystemAffinityMask;

    BOOL fRc = GetProcessAffinityMask(GetCurrentProcess(), &dwProcessAffinityMask, &dwSystemAffinityMask);
    Assert(fRc);

    return dwProcessAffinityMask;
}

