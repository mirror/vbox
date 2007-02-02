/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Process, POSIX.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#if defined(__LINUX__) || defined(__OS2__)
# define HAVE_POSIX_SPAWN 1
#endif
#ifdef HAVE_POSIX_SPAWN
# include <spawn.h>
#endif
#ifdef __DARWIN__
# include <mach-o/dyld.h>
#endif

#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/process.h"



RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, const char * const *papszEnv, unsigned fFlags, PRTPROCESS pProcess)
{
    /*
     * Validate input.
     */
    if (!pszExec || !*pszExec)
    {
        AssertMsgFailed(("no exec\n"));
        return VERR_INVALID_PARAMETER;
    }
    if (fFlags)
    {
        AssertMsgFailed(("invalid flags!\n"));
        return VERR_INVALID_PARAMETER;
    }
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
                         papszEnv ? (char * const *)papszEnv : environ);
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
        if (papszEnv)
            rc = execve(pszExec, (char * const *)papszArgs, (char * const *)papszEnv);
        else
            rc = execv(pszExec, (char * const *)papszArgs);
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
#if defined(__LINUX__) || defined(__FREEBSD__)
# ifdef __LINUX__
    int cchLink = readlink("/proc/self/exe", pszExecName, cchExecName - 1);
# else    
    int cchLink = readlink("/proc/curproc/file", pszExecName, cchExecName - 1);
# endif    
    if (cchLink > 0 && (size_t)cchLink <= cchExecName - 1)
    {
        pszExecName[cchLink] = '\0';
        return pszExecName;
    }

#elif defined(__OS2__) || defined(__L4__)
    if (!_execname(pszExecName, cchExecName))
        return pszExecName;

#elif defined(__DARWIN__)
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

