/* $Id$ */
/** @file
 * IPRT - Process, POSIX.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
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
    int rc;

    /*
     * Validate input.
     */
    AssertPtrReturn(pszExec, VERR_INVALID_POINTER);
    AssertReturn(*pszExec, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~RTPROC_FLAGS_DAEMONIZE), VERR_INVALID_PARAMETER);
    AssertReturn(Env != NIL_RTENV, VERR_INVALID_PARAMETER);
    const char * const *papszEnv = RTEnvGetExecEnvP(Env);
    AssertPtrReturn(papszEnv, VERR_INVALID_HANDLE);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    /* later: path searching. */


    /*
     * Check for execute access to the file.
     */
    if (access(pszExec, X_OK))
    {
        rc = RTErrConvertFromErrno(errno);
        AssertMsgFailed(("'%s' %Rrc!\n", pszExec, rc));
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
    if (!(fFlags & RTPROC_FLAGS_DAEMONIZE))
    {
        /** @todo check if it requires any of those two attributes, don't remember atm. */
        rc = posix_spawn(&pid, pszExec, NULL, NULL, (char * const *)papszArgs,
                         (char * const *)papszEnv);
        if (!rc)
        {
            if (pProcess)
                *pProcess = pid;
            return VINF_SUCCESS;
        }
    }
    else
#endif
    {
        pid = fork();
        if (!pid)
        {
            if (fFlags & RTPROC_FLAGS_DAEMONIZE)
            {
                rc = RTProcDaemonize(true /* fNoChDir */, false /* fNoClose */, NULL /* pszPidFile */);
                AssertReleaseMsgFailed(("RTProcDaemonize returns %Rrc errno=%d\n", rc, errno));
                exit(127);
            }
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
        rc = errno;
    }

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


/**
 * Daemonize the current process, making it a background process. The current
 * process will exit if daemonizing is successful.
 *
 * @returns iprt status code.
 * @param   fNoChDir    Pass false to change working directory to "/".
 * @param   fNoClose    Pass false to redirect standard file streams to the null device.
 * @param   pszPidfile  Path to a file to write the process id of the daemon
 *                      process to. Daemonizing will fail if this file already
 *                      exists or cannot be written. May be NULL.
 */
RTR3DECL(int)   RTProcDaemonize(bool fNoChDir, bool fNoClose, const char *pszPidfile)
{
    /*
     * Fork the child process in a new session and quit the parent.
     *
     * - fork once and create a new session (setsid). This will detach us
     *   from the controlling tty meaning that we won't receive the SIGHUP
     *   (or any other signal) sent to that session.
     * - The SIGHUP signal is ignored because the session/parent may throw
     *   us one before we get to the setsid.
     * - When the parent exit(0) we will become an orphan and re-parented to
     *   the init process.
     * - Because of the sometimes unexpected semantics of assigning the
     *   controlling tty automagically when a session leader first opens a tty,
     *   we will fork() once more to get rid of the session leadership role.
     */

    /* We start off by opening the pidfile, so that we can fail straight away
     * if it already exists. */
    int fdPidfile = -1;
    if (pszPidfile != NULL)
    {
        /* @note the exclusive create is not guaranteed on all file
         * systems (e.g. NFSv2) */
        if ((fdPidfile = open(pszPidfile, O_RDWR | O_CREAT | O_EXCL, 0644)) == -1)
            return RTErrConvertFromErrno(errno);
    }

    /* Ignore SIGHUP straight away. */
    struct sigaction OldSigAct;
    struct sigaction SigAct;
    memset(&SigAct, 0, sizeof(SigAct));
    SigAct.sa_handler = SIG_IGN;
    int rcSigAct = sigaction(SIGHUP, &SigAct, &OldSigAct);

    /* First fork, to become independent process. */
    pid_t pid = fork();
    if (pid == -1)
        return RTErrConvertFromErrno(errno);
    if (pid != 0)
    {
        /* Parent exits, no longer necessary. Child creates gets reparented
         * to the init process. */
        exit(0);
    }

    /* Create new session, fix up the standard file descriptors and the
     * current working directory. */
    pid_t newpgid = setsid();
    int SavedErrno = errno;
    if (rcSigAct != -1)
        sigaction(SIGHUP, &OldSigAct, NULL);
    if (newpgid == -1)
        return RTErrConvertFromErrno(SavedErrno);

    if (!fNoClose)
    {
        /* Open stdin(0), stdout(1) and stderr(2) as /dev/null. */
        int fd = open("/dev/null", O_RDWR);
        if (fd == -1) /* paranoia */
        {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            fd = open("/dev/null", O_RDWR);
        }
        if (fd != -1)
        {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2)
                close(fd);
        }
    }

    if (!fNoChDir)
        chdir("/");

    /* Second fork to lose session leader status. */
    pid = fork();
    if (pid == -1)
        return RTErrConvertFromErrno(errno);
    if (pid != 0)
    {
        /* Write the pid file, this is done in the parent, before exiting. */
        if (fdPidfile != -1)
        {
            char szBuf[256];
            size_t cbPid = RTStrPrintf(szBuf, sizeof(szBuf), "%d\n", pid);
            write(fdPidfile, szBuf, cbPid);
            close(fdPidfile);
        }
        exit(0);
    }

    return VINF_SUCCESS;
}

