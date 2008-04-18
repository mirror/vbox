/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Process, POSIX.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#if defined(RT_OS_LINUX) || defined(RT_OS_OS2)
# define HAVE_POSIX_SPAWN 1
#endif
#ifdef HAVE_POSIX_SPAWN
# include <spawn.h>
#endif
#ifdef RT_OS_DARWIN
# include <mach-o/dyld.h>
#endif

#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include "internal/process.h"



RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess)
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
    /* later: path searching. */


    /*
     * Check for execute access to the file.
     */
    if (access(pszExec, X_OK))
    {
        int rc = RTErrConvertFromErrno(errno);
        AssertMsgFailed(("'%s' %Vrc!\n", pszExec, rc));
        return rc;
    }

#if 0
    /*
     * Squeeze gdb --args in front of what's being spawned.
     */
    unsigned cArgs = 0;
    while (papszArgs[cArgs])
        cArgs++;
    cArgs += 3;
    const char **papszArgsTmp = (const char **)alloca(cArgs * sizeof(char *));
    papszArgsTmp[0] = "/usr/bin/gdb";
    papszArgsTmp[1] = "--args";
    papszArgsTmp[2] = pszExec;
    for (unsigned i = 1; papszArgs[i]; i++)
        papszArgsTmp[i + 2] = papszArgs[i];
    papszArgsTmp[cArgs - 1] = NULL;
    pszExec = papszArgsTmp[0];
    papszArgs = papszArgsTmp;
#endif

    /*
     * Spawn the child.
     */
    pid_t pid;
#ifdef HAVE_POSIX_SPAWN
    /** @todo check if it requires any of those two attributes, don't remember atm. */
    int rc = posix_spawn(&pid, pszExec, NULL, NULL, (char * const *)papszArgs,
                         (char * const *)papszEnv);
    if (!rc)
    {
        if (pProcess)
            *pProcess = pid;
        return VINF_SUCCESS;
    }

#else

    pid = fork();
    if (!pid)
    {
        int rc;
        rc = execve(pszExec, (char * const *)papszArgs, (char * const *)papszEnv);
        AssertReleaseMsgFailed(("execve returns %d errno=%d\n", rc, errno));
        exit(127);
    }
    if (pid > 0)
    {
        if (pProcess)
            *pProcess = pid;
        return VINF_SUCCESS;
    }
    int rc = errno;
#endif

    /* failure, errno value in rc. */
    AssertMsgFailed(("spawn/exec failed rc=%d\n", rc)); /* this migth be annoying... */
    return RTErrConvertFromErrno(rc);
}


RTR3DECL(int)   RTProcWait(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    int rc;
    do rc = RTProcWaitNoResume(Process, fFlags, pProcStatus);
    while (rc == VERR_INTERRUPTED);
    return rc;
}

RTR3DECL(int)   RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    /*
     * Validate input.
     */
    if (Process <= 0)
    {
        AssertMsgFailed(("Invalid Process=%d\n", Process));
        return VERR_INVALID_PARAMETER;
    }
    if (fFlags & ~(RTPROCWAIT_FLAGS_NOBLOCK | RTPROCWAIT_FLAGS_BLOCK))
    {
        AssertMsgFailed(("Invalid flags %#x\n", fFlags));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Performe the wait.
     */
    int iStatus = 0;
    int rc = waitpid(Process, &iStatus, fFlags & RTPROCWAIT_FLAGS_NOBLOCK ? WNOHANG : 0);
    if (rc > 0)
    {
        /*
         * Fill in the status structure.
         */
        if (pProcStatus)
        {
            if (WIFEXITED(iStatus))
            {
                pProcStatus->enmReason = RTPROCEXITREASON_NORMAL;
                pProcStatus->iStatus = WEXITSTATUS(iStatus);
            }
            else if (WIFSIGNALED(iStatus))
            {
                pProcStatus->enmReason = RTPROCEXITREASON_SIGNAL;
                pProcStatus->iStatus = WTERMSIG(iStatus);
            }
            else
            {
                Assert(!WIFSTOPPED(iStatus));
                pProcStatus->enmReason = RTPROCEXITREASON_ABEND;
                pProcStatus->iStatus = iStatus;
            }
        }
        return VINF_SUCCESS;
    }

    /*
     * Child running?
     */
    if (!rc)
    {
        Assert(fFlags & RTPROCWAIT_FLAGS_NOBLOCK);
        return VERR_PROCESS_RUNNING;
    }

    /*
     * Figure out which error to return.
     */
    int iErr = errno;
    if (iErr == ECHILD)
        return VERR_PROCESS_NOT_FOUND;
    return RTErrConvertFromErrno(iErr);
}


RTR3DECL(int) RTProcTerminate(RTPROCESS Process)
{
    if (!kill(Process, SIGKILL))
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(uint64_t) RTProcGetAffinityMask()
{
    // @todo
    return 1;
}


RTR3DECL(char *) RTProcGetExecutableName(char *pszExecName, size_t cchExecName)
{
    /*
     * I don't think there is a posix API for this, but
     * because I'm lazy I'm not creating OS specific code
     * files and code for this.
     */
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD) || defined(RT_OS_SOLARIS)
# ifdef RT_OS_LINUX
    int cchLink = readlink("/proc/self/exe", pszExecName, cchExecName - 1);
# elif defined(RT_OS_SOLARIS)
    char szFileBuf[80];
    RTStrPrintf(szFileBuf, sizeof(szFileBuf), "/proc/%ld/path/a.out", (long)getpid());
    int cchLink = readlink(szFileBuf, pszExecName, cchExecName - 1);
# else
    int cchLink = readlink("/proc/curproc/file", pszExecName, cchExecName - 1);
# endif
    if (cchLink > 0 && (size_t)cchLink <= cchExecName - 1)
    {
        pszExecName[cchLink] = '\0';
        return pszExecName;
    }

#elif defined(RT_OS_OS2) || defined(RT_OS_L4)
    if (!_execname(pszExecName, cchExecName))
        return pszExecName;

#elif defined(RT_OS_DARWIN)
    const char *pszImageName = _dyld_get_image_name(0);
    if (pszImageName)
    {
        size_t cchImageName = strlen(pszImageName);
        if (cchImageName < cchExecName)
            return (char *)memcpy(pszExecName, pszImageName, cchImageName + 1);
    }

#else
#   error "Port me!"
#endif
    return NULL;
}

