/** $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, daemonize a process.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#if defined(RT_OS_DARWIN)
# error "PORTME"

#elif defined(RT_OS_OS2)
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>

# include <iprt/alloca.h>
# include <iprt/string.h>

#elif defined(RT_OS_WINDOWS)
# error "PORTME"

#else /* the unices */
# include <sys/types.h>
# include <sys/stat.h>
# include <stdio.h>
# include <fcntl.h>
# include <stdlib.h>
# include <unistd.h>
# include <signal.h>
# include <errno.h>
#endif

#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/process.h>
#include <VBox/VBoxGuest.h>


/**
 * Daemonize the process for running in the background.
 *
 * This is supposed to do the same job as the BSD daemon() call.
 *
 * @returns 0 on success
 *
 * @param   fNoChDir    Pass false to change working directory to root.
 * @param   fNoClose    Pass false to redirect standard file streams to /dev/null.
 */
VBGLR3DECL(int) VbglR3Daemonize(bool fNoChDir, bool fNoClose)
{
#if defined(RT_OS_DARWIN)
# error "PORTME"

#elif defined(RT_OS_OS2)
    PPIB pPib;
    PTIB pTib;
    DosGetInfoBlocks(&pTib, &pPib);

    /* Get the full path to the executable. */
    char szExe[CCHMAXPATH];
    APIRET rc = DosQueryModuleName(pPib->pib_hmte, sizeof(szExe), szExe);
    if (rc)
        return RTErrConvertFromOS2(rc);

    /* calc the length of the command line. */
    char *pch = pPib->pib_pchcmd;
    size_t cch0 = strlen(pch);
    pch += cch0 + 1;
    size_t cch1 = strlen(pch);
    pch += cch1 + 1;
    char *pchArgs;
    if (cch1 && *pch)
    {
        do  pch = strchr(pch, '\0') + 1;
        while (*pch);

        size_t cchTotal = pch - pPib->pib_pchcmd;
        pchArgs = (char *)alloca(cchTotal + sizeof("--daemonized\0\0"));
        memcpy(pchArgs, pPib->pib_pchcmd, cchTotal - 1);
        memcpy(pchArgs + cchTotal - 1, "--daemonized\0\0", sizeof("--daemonized\0\0"));
    }
    else
    {
        size_t cchTotal = pch - pPib->pib_pchcmd + 1;
        pchArgs = (char *)alloca(cchTotal + sizeof(" --daemonized "));
        memcpy(pchArgs, pPib->pib_pchcmd, cch0 + 1);
        pch = pchArgs + cch0 + 1;
        memcpy(pch, " --daemonized ", sizeof(" --daemonized ") - 1);
        pch += sizeof(" --daemonized ") - 1;
        if (cch1)
            memcpy(pch, pPib->pib_pchcmd + cch0 + 1, cch1 + 2);
        else
            pch[0] = pch[1] = '\0';
    }

    /* spawn a detach process  */
    char szObj[128];
    RESULTCODES ResCodes = { 0, 0 };
    szObj[0] = '\0';
    rc = DosExecPgm(szObj, sizeof(szObj), EXEC_BACKGROUND, (PCSZ)pchArgs, NULL, &ResCodes, (PCSZ)szExe);
    if (rc)
    {
        /** @todo Change this to some standard log/print error?? */
        /* VBoxServiceError("DosExecPgm failed with rc=%d and szObj='%s'\n", rc, szObj); */
        return RTErrConvertFromOS2(rc);
    }
    DosExit(EXIT_PROCESS, 0);
    return VERR_GENERAL_FAILURE;

#elif defined(RT_OS_WINDOWS)
# error "PORTME"

#else /* the unices */
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
     * - Because of the Linux / System V sematics of assigning the controlling
     *   tty automagically when a session leader first opens a tty, we will
     *   fork() once more on Linux to get rid of the session leadership role.
     */

    struct sigaction OldSigAct;
    struct sigaction SigAct;
    memset(&SigAct, 0, sizeof(SigAct));
    SigAct.sa_handler = SIG_IGN;
    int rcSigAct = sigaction(SIGHUP, &SigAct, &OldSigAct);

    pid_t pid = fork();
    if (pid == -1)
        return RTErrConvertFromErrno(errno);
    if (pid != 0)
        exit(0);

    /*
     * The orphaned child becomes is reparented to the init process.
     * We create a new session for it (setsid), point the standard
     * file descriptors to /dev/null, and change to the root directory.
     */
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

    /*
     * Change the umask - this is non-standard daemon() behavior.
     */
    umask(027);

# ifdef RT_OS_LINUX
    /*
     * And fork again to lose session leader status (non-standard daemon()
     * behaviour).
     */
    pid = fork();
    if (pid == -1)
        return RTErrConvertFromErrno(errno);
    if (pid != 0)
        exit(0);
# endif /* RT_OS_LINUX */

    return VINF_SUCCESS;
#endif
}

/**
 * Creates a pidfile and returns the open file descriptor.
 * On Unix-y systems, this call also places an advisory lock on the open
 * file.  It will overwrite any existing pidfiles without a lock on them,
 * on the assumption that they are stale files which an old process did not
 * properly clean up.
 *
 * @returns iprt status code
 * @param   pszPath  the path and filename to create the pidfile under
 * @param   pFile    where to store the file descriptor of the open
 *                   (and locked on Unix-y systems) pidfile.
 *                   On failure, or if another process owns the pidfile,
 *                   this will be set to NIL_RTFILE.
 */
VBGLR3DECL(int) VbglR3PidFile(const char *pszPath, PRTFILE pFile)
{
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
    RTFILE hPidFile = NIL_RTFILE;
    int rc = RTFileOpen(&hPidFile, pszPath,
                          RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE
                        | (0644 << RTFILE_O_CREATE_MODE_SHIFT));
    if (RT_SUCCESS(rc))
        /** @todo using size 0 for locking means lock all on Posix.
         * We should adopt this as our convention too, or something
         * similar. */
        rc = RTFileLock(hPidFile, RTFILE_LOCK_WRITE, 0, 0);
    if (RT_SUCCESS(rc))
    {
        char szBuf[256];
        size_t cbPid = RTStrPrintf(szBuf, sizeof(szBuf), "%d\n",
                                   RTProcSelf());
        RTFileWrite(hPidFile, szBuf, cbPid, NULL);
    }
    *pFile = hPidFile;
    return rc;
#else  /* !RT_OS_LINUX and !RT_OS_SOLARIS */
# error portme
#endif  /* !RT_OS_LINUX and !RT_OS_SOLARIS */
}

/**
 * Close and remove an open pidfile.
 * @param  pszPath  the path to the pidfile
 * @param  File     the open file descriptor
 */
VBGLR3DECL(void) VbglR3ClosePidFile(const char *pszPath, RTFILE File)
{
    AssertPtrReturnVoid(pszPath);
    AssertReturnVoid(File != NIL_RTFILE);
#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
    RTFileDelete(pszPath);
    RTFileClose(File);
#else  /* !RT_OS_LINUX and !RT_OS_SOLARIS */
# error portme
#endif  /* !RT_OS_LINUX and !RT_OS_SOLARIS */
}
