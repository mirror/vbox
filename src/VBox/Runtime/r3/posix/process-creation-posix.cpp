/* $Id$ */
/** @file
 * IPRT - Process Creation, POSIX.
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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
# include <crypt.h>
# include <pwd.h>
# include <shadow.h>
#endif
#if defined(RT_OS_LINUX) || defined(RT_OS_OS2)
/* While Solaris has posix_spawn() of course we don't want to use it as
 * we need to have the child in a different process contract, no matter
 * whether it is started detached or not. */
# define HAVE_POSIX_SPAWN 1
#endif
#ifdef HAVE_POSIX_SPAWN
# include <spawn.h>
#endif
#ifdef RT_OS_DARWIN
# include <mach-o/dyld.h>
#endif
#ifdef RT_OS_SOLARIS
# include <limits.h>
# include <sys/ctfs.h>
# include <sys/contract/process.h>
# include <libcontract.h>
#endif

#include <iprt/process.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/pipe.h>
#include <iprt/socket.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include "internal/process.h"


/**
 * Check the credentials and return the gid/uid of user.
 *
 * @param    pszUser     username
 * @param    pszPasswd   password
 * @param    gid         where to store the GID of the user
 * @param    uid         where to store the UID of the user
 * @returns IPRT status code
 */
static int rtCheckCredentials(const char *pszUser, const char *pszPasswd, gid_t *pGid, uid_t *pUid)
{
#if defined(RT_OS_LINUX)
    struct passwd *pw;

    pw = getpwnam(pszUser);
    if (!pw)
        return VERR_PERMISSION_DENIED;

    if (!pszPasswd)
        pszPasswd = "";

    struct spwd *spwd;
    /* works only if /etc/shadow is accessible */
    spwd = getspnam(pszUser);
    if (spwd)
        pw->pw_passwd = spwd->sp_pwdp;

    /* be reentrant */
    struct crypt_data *data = (struct crypt_data*)RTMemTmpAllocZ(sizeof(*data));
    char *pszEncPasswd = crypt_r(pszPasswd, pw->pw_passwd, data);
    int fCorrect = !strcmp(pszEncPasswd, pw->pw_passwd);
    RTMemTmpFree(data);
    if (!fCorrect)
        return VERR_PERMISSION_DENIED;

    *pGid = pw->pw_gid;
    *pUid = pw->pw_uid;
    return VINF_SUCCESS;

#elif defined(RT_OS_SOLARIS)
    struct passwd *ppw, pw;
    char szBuf[1024];

    if (getpwnam_r(pszUser, &pw, szBuf, sizeof(szBuf), &ppw) != 0 || ppw == NULL)
        return VERR_PERMISSION_DENIED;

    if (!pszPasswd)
        pszPasswd = "";

    struct spwd spwd;
    char szPwdBuf[1024];
    /* works only if /etc/shadow is accessible */
    if (getspnam_r(pszUser, &spwd, szPwdBuf, sizeof(szPwdBuf)) != NULL)
        ppw->pw_passwd = spwd.sp_pwdp;

    char *pszEncPasswd = crypt(pszPasswd, ppw->pw_passwd);
    if (strcmp(pszEncPasswd, ppw->pw_passwd))
        return VERR_PERMISSION_DENIED;

    *pGid = ppw->pw_gid;
    *pUid = ppw->pw_uid;
    return VINF_SUCCESS;

#else
    NOREF(pszUser); NOREF(pszPasswd); NOREF(pGid); NOREF(pUid);
    return VERR_PERMISSION_DENIED;
#endif
}


#ifdef RT_OS_SOLARIS
/** @todo the error reporting of the Solaris process contract code could be
 * a lot better, but essentially it is not meant to run into errors after
 * the debugging phase. */
static int rtSolarisContractPreFork(void)
{
    int templateFd = open64(CTFS_ROOT "/process/template", O_RDWR);
    if (templateFd < 0)
        return -1;

    /* Set template parameters and event sets. */
    if (ct_pr_tmpl_set_param(templateFd, CT_PR_PGRPONLY))
    {
        close(templateFd);
        return -1;
    }
    if (ct_pr_tmpl_set_fatal(templateFd, CT_PR_EV_HWERR))
    {
        close(templateFd);
        return -1;
    }
    if (ct_tmpl_set_critical(templateFd, 0))
    {
        close(templateFd);
        return -1;
    }
    if (ct_tmpl_set_informative(templateFd, CT_PR_EV_HWERR))
    {
        close(templateFd);
        return -1;
    }

    /* Make this the active template for the process. */
    if (ct_tmpl_activate(templateFd))
    {
        close(templateFd);
        return -1;
    }

    return templateFd;
}

static void rtSolarisContractPostForkChild(int templateFd)
{
    if (templateFd == -1)
        return;

    /* Clear the active template. */
    ct_tmpl_clear(templateFd);
    close(templateFd);
}

static void rtSolarisContractPostForkParent(int templateFd, pid_t pid)
{
    if (templateFd == -1)
        return;

    /* Clear the active template. */
    int cleared = ct_tmpl_clear(templateFd);
    close(templateFd);

    /* If the clearing failed or the fork failed there's nothing more to do. */
    if (cleared || pid <= 0)
        return;

    /* Look up the contract which was created by this thread. */
    int statFd = open64(CTFS_ROOT "/process/latest", O_RDONLY);
    if (statFd == -1)
        return;
    ct_stathdl_t statHdl;
    if (ct_status_read(statFd, CTD_COMMON, &statHdl))
    {
        close(statFd);
        return;
    }
    ctid_t ctId = ct_status_get_id(statHdl);
    ct_status_free(statHdl);
    close(statFd);
    if (ctId < 0)
        return;

    /* Abandon this contract we just created. */
    char ctlPath[PATH_MAX];
    size_t len = snprintf(ctlPath, sizeof(ctlPath),
                          CTFS_ROOT "/process/%d/ctl", ctId);
    if (len >= sizeof(ctlPath))
        return;
    int ctlFd = open64(ctlPath, O_WRONLY);
    if (statFd == -1)
        return;
    if (ct_ctl_abandon(ctlFd) < 0)
    {
        close(ctlFd);
        return;
    }
    close(ctlFd);
}

#endif /* RT_OS_SOLARIS */


RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, RTENV Env, unsigned fFlags, PRTPROCESS pProcess)
{
    return RTProcCreateEx(pszExec, papszArgs, Env, fFlags,
                          NULL, NULL, NULL,  /* standard handles */
                          NULL /*pszAsUser*/, NULL /* pszPassword*/,
                          pProcess);
}


RTR3DECL(int)   RTProcCreateEx(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                               PCRTHANDLE phStdIn, PCRTHANDLE phStdOut, PCRTHANDLE phStdErr, const char *pszAsUser,
                               const char *pszPassword, PRTPROCESS phProcess)
{
    int rc;

    /*
     * Input validation
     */
    AssertPtrReturn(pszExec, VERR_INVALID_POINTER);
    AssertReturn(*pszExec, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~(RTPROC_FLAGS_DETACHED | RTPROC_FLAGS_HIDDEN | RTPROC_FLAGS_SERVICE | RTPROC_FLAGS_SAME_CONTRACT | RTPROC_FLAGS_NO_PROFILE)), VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & RTPROC_FLAGS_DETACHED) || !phProcess, VERR_INVALID_PARAMETER);
    AssertReturn(hEnv != NIL_RTENV, VERR_INVALID_PARAMETER);
    const char * const *papszEnv = RTEnvGetExecEnvP(hEnv);
    AssertPtrReturn(papszEnv, VERR_INVALID_HANDLE);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    /** @todo search the PATH (add flag for this). */
    AssertPtrNullReturn(pszAsUser, VERR_INVALID_POINTER);
    AssertReturn(!pszAsUser || *pszAsUser, VERR_INVALID_PARAMETER);
    AssertReturn(!pszPassword || pszAsUser, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pszPassword, VERR_INVALID_POINTER);

    /*
     * Get the file descriptors for the handles we've been passed.
     */
    PCRTHANDLE  paHandles[3] = { phStdIn, phStdOut, phStdErr };
    int         aStdFds[3]   = {      -1,       -1,       -1 };
    for (int i = 0; i < 3; i++)
    {
        if (paHandles[i])
        {
            AssertPtrReturn(paHandles[i], VERR_INVALID_POINTER);
            switch (paHandles[i]->enmType)
            {
                case RTHANDLETYPE_FILE:
                    aStdFds[i] = paHandles[i]->u.hFile != NIL_RTFILE
                               ? (int)RTFileToNative(paHandles[i]->u.hFile)
                               : -2 /* close it */;
                    break;

                case RTHANDLETYPE_PIPE:
                    aStdFds[i] = paHandles[i]->u.hPipe != NIL_RTPIPE
                               ? (int)RTPipeToNative(paHandles[i]->u.hPipe)
                               : -2 /* close it */;
                    break;

                case RTHANDLETYPE_SOCKET:
                    aStdFds[i] = paHandles[i]->u.hSocket != NIL_RTSOCKET
                               ? (int)RTSocketToNative(paHandles[i]->u.hSocket)
                               : -2 /* close it */;
                    break;

                default:
                    AssertMsgFailedReturn(("%d: %d\n", i, paHandles[i]->enmType), VERR_INVALID_PARAMETER);
            }
            /** @todo check the close-on-execness of these handles?  */
        }
    }

    for (int i = 0; i < 3; i++)
        if (aStdFds[i] == i)
            aStdFds[i] = -1;

    for (int i = 0; i < 3; i++)
        AssertMsgReturn(aStdFds[i] < 0 || aStdFds[i] > i,
                        ("%i := %i not possible because we're lazy\n", i, aStdFds[i]),
                        VERR_NOT_SUPPORTED);

    /*
     * Resolve the user id if specified.
     */
    uid_t uid = ~(uid_t)0;
    gid_t gid = ~(gid_t)0;
    if (pszAsUser)
    {
        rc = rtCheckCredentials(pszAsUser, pszPassword, &gid, &uid);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Check for execute access to the file.
     */
    if (access(pszExec, X_OK))
        return RTErrConvertFromErrno(errno);

    pid_t pid = -1;

    /*
     * Take care of detaching the process.
     *
     * HACK ALERT! Put the process into a new process group with pgid = pid
     * to make sure it differs from that of the parent process to ensure that
     * the IPRT waitpid call doesn't race anyone (read XPCOM) doing group wide
     * waits. setsid() includes the setpgid() functionality.
     * 2010-10-11 XPCOM no longer waits for anything, but it cannot hurt.
     */
#ifndef RT_OS_OS2
    if (fFlags & RTPROC_FLAGS_DETACHED)
    {
# ifdef RT_OS_SOLARIS
        int templateFd = -1;
        if (!(fFlags & RTPROC_FLAGS_SAME_CONTRACT))
        {
            templateFd = rtSolarisContractPreFork();
            if (templateFd == -1)
                return VERR_OPEN_FAILED;
        }
# endif /* RT_OS_SOLARIS */
        pid = fork();
        if (!pid)
        {
# ifdef RT_OS_SOLARIS
            if (!(fFlags & RTPROC_FLAGS_SAME_CONTRACT))
                rtSolarisContractPostForkChild(templateFd);
# endif /* RT_OS_SOLARIS */
            setsid(); /* see comment above */

            pid = -1;
            /* Child falls through to the actual spawn code below. */
        }
        else
        {
#ifdef RT_OS_SOLARIS
            if (!(fFlags & RTPROC_FLAGS_SAME_CONTRACT))
                rtSolarisContractPostForkParent(templateFd, pid);
#endif /* RT_OS_SOLARIS */
            if (pid > 0)
            {
                /* Must wait for the temporary process to avoid a zombie. */
                int status = 0;
                pid_t pidChild = 0;

                /* Restart if we get interrupted. */
                do
                {
                    pidChild = waitpid(pid, &status, 0);
                } while (   pidChild == -1
                         && errno == EINTR);

                /* Assume that something wasn't found. No detailed info. */
                if (status)
                    return VERR_PROCESS_NOT_FOUND;
                if (phProcess)
                    *phProcess = 0;
                return VINF_SUCCESS;
            }
            return RTErrConvertFromErrno(errno);
        }
    }
#endif

    /*
     * Spawn the child.
     *
     * Any spawn code MUST not execute any atexit functions if it is for a
     * detached process. It would lead to running the atexit functions which
     * make only sense for the parent. libORBit e.g. gets confused by multiple
     * execution. Remember, there was only a fork() so far, and until exec()
     * is successfully run there is nothing which would prevent doing anything
     * silly with the (duplicated) file descriptors.
     */
#ifdef HAVE_POSIX_SPAWN
    /** @todo OS/2: implement DETACHED (BACKGROUND stuff), see VbglR3Daemonize.  */
    if (   uid == ~(uid_t)0
        && gid == ~(gid_t)0)
    {
        /* Spawn attributes. */
        posix_spawnattr_t Attr;
        rc = posix_spawnattr_init(&Attr);
        if (!rc)
        {
# ifndef RT_OS_OS2 /* We don't need this on OS/2 and I don't recall if it's actually implemented. */
            rc = posix_spawnattr_setflags(&Attr, POSIX_SPAWN_SETPGROUP);
            Assert(rc == 0);
            if (!rc)
            {
                rc = posix_spawnattr_setpgroup(&Attr, 0 /* pg == child pid */);
                Assert(rc == 0);
            }
# endif

            /* File changes. */
            posix_spawn_file_actions_t  FileActions;
            posix_spawn_file_actions_t *pFileActions = NULL;
            if (aStdFds[0] != -1 || aStdFds[1] != -1 || aStdFds[2] != -1)
            {
                rc = posix_spawn_file_actions_init(&FileActions);
                if (!rc)
                {
                    pFileActions = &FileActions;
                    for (int i = 0; i < 3; i++)
                    {
                        int fd = aStdFds[i];
                        if (fd == -2)
                            rc = posix_spawn_file_actions_addclose(&FileActions, i);
                        else if (fd >= 0 && fd != i)
                        {
                            rc = posix_spawn_file_actions_adddup2(&FileActions, fd, i);
                            if (!rc)
                            {
                                for (int j = i + 1; j < 3; j++)
                                    if (aStdFds[j] == fd)
                                    {
                                        fd = -1;
                                        break;
                                    }
                                if (fd >= 0)
                                    rc = posix_spawn_file_actions_addclose(&FileActions, fd);
                            }
                        }
                        if (rc)
                            break;
                    }
                }
            }

            if (!rc)
                rc = posix_spawn(&pid, pszExec, pFileActions, &Attr, (char * const *)papszArgs,
                                 (char * const *)papszEnv);

            /* cleanup */
            int rc2 = posix_spawnattr_destroy(&Attr); Assert(rc2 == 0); NOREF(rc2);
            if (pFileActions)
            {
                rc2 = posix_spawn_file_actions_destroy(pFileActions);
                Assert(rc2 == 0);
            }

            /* return on success.*/
            if (!rc)
            {
                /* For a detached process this happens in the temp process, so
                 * it's not worth doing anything as this process must exit. */
                if (fFlags & RTPROC_FLAGS_DETACHED)
                _Exit(0);
                if (phProcess)
                    *phProcess = pid;
                return VINF_SUCCESS;
            }
        }
        /* For a detached process this happens in the temp process, so
         * it's not worth doing anything as this process must exit. */
        if (fFlags & RTPROC_FLAGS_DETACHED)
            _Exit(124);
    }
    else
#endif
    {
#ifdef RT_OS_SOLARIS
        int templateFd = rtSolarisContractPreFork();
        if (templateFd == -1)
            return VERR_OPEN_FAILED;
#endif /* RT_OS_SOLARIS */
        pid = fork();
        if (!pid)
        {
#ifdef RT_OS_SOLARIS
            rtSolarisContractPostForkChild(templateFd);
#endif /* RT_OS_SOLARIS */
            if (!(fFlags & RTPROC_FLAGS_DETACHED))
                setpgid(0, 0); /* see comment above */

            /*
             * Change group and user if requested.
             */
#if 1 /** @todo This needs more work, see suplib/hardening. */
            if (gid != ~(gid_t)0)
            {
                if (setgid(gid))
                {
                    if (fFlags & RTPROC_FLAGS_DETACHED)
                        _Exit(126);
                    else
                        exit(126);
                }
            }

            if (uid != ~(uid_t)0)
            {
                if (setuid(uid))
                {
                    if (fFlags & RTPROC_FLAGS_DETACHED)
                        _Exit(126);
                    else
                        exit(126);
                }
            }
#endif

            /*
             * Apply changes to the standard file descriptor and stuff.
             */
            for (int i = 0; i < 3; i++)
            {
                int fd = aStdFds[i];
                if (fd == -2)
                    close(i);
                else if (fd >= 0)
                {
                    int rc2 = dup2(fd, i);
                    if (rc2 != i)
                    {
                        if (fFlags & RTPROC_FLAGS_DETACHED)
                            _Exit(125);
                        else
                            exit(125);
                    }
                    for (int j = i + 1; j < 3; j++)
                        if (aStdFds[j] == fd)
                        {
                            fd = -1;
                            break;
                        }
                    if (fd >= 0)
                        close(fd);
                }
            }

            /*
             * Finally, execute the requested program.
             */
            rc = execve(pszExec, (char * const *)papszArgs, (char * const *)papszEnv);
            if (errno == ENOEXEC)
            {
                /* This can happen when trying to start a shell script without the magic #!/bin/sh */
                RTAssertMsg2Weak("Cannot execute this binary format!\n");
            }
            else
                RTAssertMsg2Weak("execve returns %d errno=%d\n", rc, errno);
            RTAssertReleasePanic();
            if (fFlags & RTPROC_FLAGS_DETACHED)
                _Exit(127);
            else
                exit(127);
        }
#ifdef RT_OS_SOLARIS
        rtSolarisContractPostForkParent(templateFd, pid);
#endif /* RT_OS_SOLARIS */
        if (pid > 0)
        {
            /* For a detached process this happens in the temp process, so
             * it's not worth doing anything as this process must exit. */
            if (fFlags & RTPROC_FLAGS_DETACHED)
                _Exit(0);
            if (phProcess)
                *phProcess = pid;
            return VINF_SUCCESS;
        }
        /* For a detached process this happens in the temp process, so
         * it's not worth doing anything as this process must exit. */
        if (fFlags & RTPROC_FLAGS_DETACHED)
            _Exit(124);
        return RTErrConvertFromErrno(errno);
    }

    return VERR_NOT_IMPLEMENTED;
}


RTR3DECL(int)   RTProcDaemonizeUsingFork(bool fNoChDir, bool fNoClose, const char *pszPidfile)
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
    {
        if (fdPidfile != -1)
            close(fdPidfile);
        return RTErrConvertFromErrno(errno);
    }
    if (pid != 0)
    {
        /* Parent exits, no longer necessary. The child gets reparented
         * to the init process. */
        exit(0);
    }

    /* Create new session, fix up the standard file descriptors and the
     * current working directory. */
    /** @todo r=klaus the webservice uses this function and assumes that the
     * contract id of the daemon is the same as that of the original process.
     * Whenever this code is changed this must still remain possible. */
    pid_t newpgid = setsid();
    int SavedErrno = errno;
    if (rcSigAct != -1)
        sigaction(SIGHUP, &OldSigAct, NULL);
    if (newpgid == -1)
    {
        if (fdPidfile != -1)
            close(fdPidfile);
        return RTErrConvertFromErrno(SavedErrno);
    }

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
    {
        int rcIgnored = chdir("/");
        NOREF(rcIgnored);
    }

    /* Second fork to lose session leader status. */
    pid = fork();
    if (pid == -1)
    {
        if (fdPidfile != -1)
            close(fdPidfile);
        return RTErrConvertFromErrno(errno);
    }

    if (pid != 0)
    {
        /* Write the pid file, this is done in the parent, before exiting. */
        if (fdPidfile != -1)
        {
            char szBuf[256];
            size_t cbPid = RTStrPrintf(szBuf, sizeof(szBuf), "%d\n", pid);
            ssize_t cbIgnored = write(fdPidfile, szBuf, cbPid); NOREF(cbIgnored);
            close(fdPidfile);
        }
        exit(0);
    }

    if (fdPidfile != -1)
        close(fdPidfile);

    return VINF_SUCCESS;
}

