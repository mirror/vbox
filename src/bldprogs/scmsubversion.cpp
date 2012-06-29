/* $Id$ */
/** @file
 * IPRT Testcase / Tool - Source Code Massager, Subversion Access.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define SCM_WITHOUT_LIBSVN

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scm.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static char g_szSvnPath[RTPATH_MAX];
static enum
{
    kScmSvnVersion_Ancient = 1,
    kScmSvnVersion_1_6,
    kScmSvnVersion_1_7,
    kScmSvnVersion_End
}           g_enmSvnVersion = kScmSvnVersion_Ancient;


#ifdef SCM_WITHOUT_LIBSVN

/**
 * Callback that is call for each path to search.
 */
static DECLCALLBACK(int) scmSvnFindSvnBinaryCallback(char const *pchPath, size_t cchPath, void *pvUser1, void *pvUser2)
{
    char   *pszDst = (char *)pvUser1;
    size_t  cchDst = (size_t)pvUser2;
    if (cchDst > cchPath)
    {
        memcpy(pszDst, pchPath, cchPath);
        pszDst[cchPath] = '\0';
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        int rc = RTPathAppend(pszDst, cchDst, "svn.exe");
#else
        int rc = RTPathAppend(pszDst, cchDst, "svn");
#endif
        if (   RT_SUCCESS(rc)
            && RTFileExists(pszDst))
            return VINF_SUCCESS;
    }
    return VERR_TRY_AGAIN;
}

#include <iprt/handle.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>

/**
 * Reads from a pipe.
 *
 * @returns @a rc or other status code.
 * @param   rc              The current status of the operation.  Error status
 *                          are preserved and returned.
 * @param   phPipeR         Pointer to the pipe handle.
 * @param   pcbAllocated    Pointer to the buffer size variable.
 * @param   poffCur         Pointer to the buffer offset variable.
 * @param   ppszBuffer      Pointer to the buffer pointer variable.
 */
static int rtProcProcessOutput(int rc, PRTPIPE phPipeR, size_t *pcbAllocated, size_t *poffCur, char **ppszBuffer,
                               RTPOLLSET hPollSet, uint32_t idPollSet)
{
    size_t  cbRead;
    char    szTmp[_4K - 1];
    for (;;)
    {
        int rc2 = RTPipeRead(*phPipeR, szTmp, sizeof(szTmp), &cbRead);
        if (RT_SUCCESS(rc2) && cbRead)
        {
            /* Resize the buffer. */
            if (*poffCur + cbRead >= *pcbAllocated)
            {
                if (*pcbAllocated >= _1G)
                {
                    RTPollSetRemove(hPollSet, idPollSet);
                    rc2 = RTPipeClose(*phPipeR); AssertRC(rc2);
                    *phPipeR = NIL_RTPIPE;
                    return RT_SUCCESS(rc) ? VERR_TOO_MUCH_DATA : rc;
                }

                size_t cbNew = *pcbAllocated ? *pcbAllocated * 2 : sizeof(szTmp) + 1;
                Assert(*poffCur + cbRead < cbNew);
                rc2 = RTStrRealloc(ppszBuffer, cbNew);
                if (RT_FAILURE(rc2))
                {
                    RTPollSetRemove(hPollSet, idPollSet);
                    rc2 = RTPipeClose(*phPipeR); AssertRC(rc2);
                    *phPipeR = NIL_RTPIPE;
                    return RT_SUCCESS(rc) ? rc2 : rc;
                }
                *pcbAllocated = cbNew;
            }

            /* Append the new data, terminating it. */
            memcpy(*ppszBuffer + *poffCur, szTmp, cbRead);
            *poffCur += cbRead;
            (*ppszBuffer)[*poffCur] = '\0';

            /* Check for null terminators in the string. */
            if (RT_SUCCESS(rc) && memchr(szTmp, '\0', cbRead))
                rc = VERR_NO_TRANSLATION;

            /* If we read a full buffer, try read some more. */
            if (RT_SUCCESS(rc) && cbRead == sizeof(szTmp))
                continue;
        }
        else if (rc2 != VINF_TRY_AGAIN)
        {
            if (RT_FAILURE(rc) && rc2 != VERR_BROKEN_PIPE)
                rc = rc2;
            RTPollSetRemove(hPollSet, idPollSet);
            rc2 = RTPipeClose(*phPipeR); AssertRC(rc2);
            *phPipeR = NIL_RTPIPE;
        }
        return rc;
    }
}

/** @name RTPROCEXEC_FLAGS_XXX - flags for RTProcExec and RTProcExecToString.
 * @{ */
/** Redirect /dev/null to standard input. */
#define RTPROCEXEC_FLAGS_STDIN_NULL             RT_BIT_32(0)
/** Redirect standard output to /dev/null. */
#define RTPROCEXEC_FLAGS_STDOUT_NULL            RT_BIT_32(1)
/** Redirect standard error to /dev/null. */
#define RTPROCEXEC_FLAGS_STDERR_NULL            RT_BIT_32(2)
/** Redirect all standard output to /dev/null as well as directing /dev/null
 * to standard input. */
#define RTPROCEXEC_FLAGS_STD_NULL               (  RTPROCEXEC_FLAGS_STDIN_NULL \
                                                 | RTPROCEXEC_FLAGS_STDOUT_NULL \
                                                 | RTPROCEXEC_FLAGS_STDERR_NULL)
/** Mask containing the valid flags. */
#define RTPROCEXEC_FLAGS_VALID_MASK             UINT32_C(0x00000007)
/** @} */

/**
 * Runs a process, collecting the standard output and/or standard error.
 *
 *
 * @returns IPRT status code
 * @retval  VERR_NO_TRANSLATION if the output of the program isn't valid UTF-8
 *          or contains a nul character.
 * @retval  VERR_TOO_MUCH_DATA if the process produced too much data.
 *
 * @param   pszExec     Executable image to use to create the child process.
 * @param   papszArgs   Pointer to an array of arguments to the child.  The
 *                      array terminated by an entry containing NULL.
 * @param   hEnv        Handle to the environment block for the child.
 * @param   fFlags      A combination of RTPROCEXEC_FLAGS_XXX.  The @a
 *                      ppszStdOut and @a ppszStdErr parameters takes precedence
 *                      over redirection flags.
 * @param   pStatus     Where to return the status on success.
 * @param   ppszStdOut  Where to return the text written to standard output. If
 *                      NULL then standard output will not be collected and go
 *                      to the standard output handle of the process.
 *                      Free with RTStrFree, regardless of return status.
 * @param   ppszStdErr  Where to return the text written to standard error. If
 *                      NULL then standard output will not be collected and go
 *                      to the standard error handle of the process.
 *                      Free with RTStrFree, regardless of return status.
 */
int RTProcExecToString(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                       PRTPROCSTATUS pStatus, char **ppszStdOut, char **ppszStdErr)
{
    int rc2;

    /*
     * Clear output arguments (no returning failure here, simply crash!).
     */
    AssertPtr(pStatus);
    pStatus->enmReason = RTPROCEXITREASON_ABEND;
    pStatus->iStatus   = RTEXITCODE_FAILURE;
    AssertPtrNull(ppszStdOut);
    if (ppszStdOut)
        *ppszStdOut = NULL;
    AssertPtrNull(ppszStdOut);
    if (ppszStdErr)
        *ppszStdErr = NULL;

    /*
     * Check input arguments.
     */
    AssertReturn(!(fFlags & ~RTPROCEXEC_FLAGS_VALID_MASK), VERR_INVALID_PARAMETER);

    /*
     * Do we need a standard input bitbucket?
     */
    int         rc = VINF_SUCCESS;
    PRTHANDLE   phChildStdIn = NULL;
    RTHANDLE    hChildStdIn;
    hChildStdIn.enmType = RTHANDLETYPE_FILE;
    hChildStdIn.u.hFile = NIL_RTFILE;
    if ((fFlags & RTPROCEXEC_FLAGS_STDIN_NULL) && RT_SUCCESS(rc))
    {
        phChildStdIn = &hChildStdIn;
        rc = RTFileOpenBitBucket(&hChildStdIn.u.hFile, RTFILE_O_READ);
    }

    /*
     * Create the output pipes / bitbuckets.
     */
    RTPIPE      hPipeStdOutR  = NIL_RTPIPE;
    PRTHANDLE   phChildStdOut = NULL;
    RTHANDLE    hChildStdOut;
    hChildStdOut.enmType = RTHANDLETYPE_PIPE;
    hChildStdOut.u.hPipe = NIL_RTPIPE;
    if (ppszStdOut && RT_SUCCESS(rc))
    {
        phChildStdOut = &hChildStdOut;
        rc = RTPipeCreate(&hPipeStdOutR, &hChildStdOut.u.hPipe, 0 /*fFlags*/);
    }
    else if ((fFlags & RTPROCEXEC_FLAGS_STDOUT_NULL) && RT_SUCCESS(rc))
    {
        phChildStdOut = &hChildStdOut;
        hChildStdOut.enmType = RTHANDLETYPE_FILE;
        hChildStdOut.u.hFile = NIL_RTFILE;
        rc = RTFileOpenBitBucket(&hChildStdOut.u.hFile, RTFILE_O_WRITE);
    }

    RTPIPE      hPipeStdErrR  = NIL_RTPIPE;
    PRTHANDLE   phChildStdErr = NULL;
    RTHANDLE    hChildStdErr;
    hChildStdErr.enmType = RTHANDLETYPE_PIPE;
    hChildStdErr.u.hPipe = NIL_RTPIPE;
    if (ppszStdErr && RT_SUCCESS(rc))
    {
        phChildStdErr = &hChildStdErr;
        rc = RTPipeCreate(&hPipeStdErrR, &hChildStdErr.u.hPipe, 0 /*fFlags*/);
    }
    else if ((fFlags & RTPROCEXEC_FLAGS_STDERR_NULL) && RT_SUCCESS(rc))
    {
        phChildStdErr = &hChildStdErr;
        hChildStdErr.enmType = RTHANDLETYPE_FILE;
        hChildStdErr.u.hFile = NIL_RTFILE;
        rc = RTFileOpenBitBucket(&hChildStdErr.u.hFile, RTFILE_O_WRITE);
    }

    if (RT_SUCCESS(rc))
    {
        RTPOLLSET hPollSet;
        rc = RTPollSetCreate(&hPollSet);
        if (RT_SUCCESS(rc))
        {
            if (hPipeStdOutR != NIL_RTPIPE && RT_SUCCESS(rc))
                rc = RTPollSetAddPipe(hPollSet, hPipeStdOutR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 1);
            if (hPipeStdErrR != NIL_RTPIPE)
                rc = RTPollSetAddPipe(hPollSet, hPipeStdErrR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 2);
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Create the process.
             */
            RTPROCESS hProc;
            rc = RTProcCreateEx(g_szSvnPath,
                                papszArgs,
                                RTENV_DEFAULT,
                                0 /*fFlags*/,
                                NULL /*phStdIn*/,
                                phChildStdOut,
                                phChildStdErr,
                                NULL /*pszAsUser*/,
                                NULL /*pszPassword*/,
                                &hProc);
            rc2 = RTHandleClose(&hChildStdErr); AssertRC(rc2);
            rc2 = RTHandleClose(&hChildStdOut); AssertRC(rc2);

            if (RT_SUCCESS(rc))
            {
                /*
                 * Process output and wait for the process to finish.
                 */
                size_t cbStdOut  = 0;
                size_t offStdOut = 0;
                size_t cbStdErr  = 0;
                size_t offStdErr = 0;
                for (;;)
                {
                    if (hPipeStdOutR != NIL_RTPIPE)
                        rc = rtProcProcessOutput(rc, &hPipeStdOutR, &cbStdOut, &offStdOut, ppszStdOut, hPollSet, 1);
                    if (hPipeStdErrR != NIL_RTPIPE)
                        rc = rtProcProcessOutput(rc, &hPipeStdErrR, &cbStdErr, &offStdErr, ppszStdErr, hPollSet, 2);
                    if (hPipeStdOutR == NIL_RTPIPE && hPipeStdErrR == NIL_RTPIPE)
                        break;

                    if (hProc != NIL_RTPROCESS)
                    {
                        rc2 = RTProcWait(hProc, RTPROCWAIT_FLAGS_NOBLOCK, pStatus);
                        if (rc2 != VERR_PROCESS_RUNNING)
                        {
                            if (RT_FAILURE(rc2))
                                rc = rc2;
                            hProc = NIL_RTPROCESS;
                        }
                    }

                    rc2 = RTPoll(hPollSet, 10000, NULL, NULL);
                    Assert(RT_SUCCESS(rc2) || rc2 == VERR_TIMEOUT);
                }

                if (RT_SUCCESS(rc))
                {
                    if (   (ppszStdOut && *ppszStdOut && !RTStrIsValidEncoding(*ppszStdOut))
                        || (ppszStdErr && *ppszStdErr && !RTStrIsValidEncoding(*ppszStdErr)) )
                        rc = VERR_NO_TRANSLATION;
                }

                /*
                 * No more output, just wait for it to finish.
                 */
                if (hProc != NIL_RTPROCESS)
                {
                    rc2 = RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, pStatus);
                    if (RT_FAILURE(rc2))
                        rc = rc2;
                }
            }
            RTPollSetDestroy(hPollSet);
        }
    }

    rc2 = RTHandleClose(&hChildStdErr); AssertRC(rc2);
    rc2 = RTHandleClose(&hChildStdOut); AssertRC(rc2);
    rc2 = RTHandleClose(&hChildStdIn);  AssertRC(rc2);
    rc2 = RTPipeClose(hPipeStdErrR);    AssertRC(rc2);
    rc2 = RTPipeClose(hPipeStdOutR);    AssertRC(rc2);
    return rc;
}


/**
 * Runs a process, waiting for it to complete.
 *
 * @returns IPRT status code
 *
 * @param   pszExec     Executable image to use to create the child process.
 * @param   papszArgs   Pointer to an array of arguments to the child.  The
 *                      array terminated by an entry containing NULL.
 * @param   hEnv        Handle to the environment block for the child.
 * @param   fFlags      A combination of RTPROCEXEC_FLAGS_XXX.
 * @param   pStatus     Where to return the status on success.
 */
int RTProcExec(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
               PRTPROCSTATUS pStatus)
{
    int rc;

    /*
     * Clear output argument (no returning failure here, simply crash!).
     */
    AssertPtr(pStatus);
    pStatus->enmReason = RTPROCEXITREASON_ABEND;
    pStatus->iStatus   = RTEXITCODE_FAILURE;

    /*
     * Check input arguments.
     */
    AssertReturn(!(fFlags & ~RTPROCEXEC_FLAGS_VALID_MASK), VERR_INVALID_PARAMETER);

    /*
     * Set up /dev/null redirections.
     */
    PRTHANDLE   aph[3] = { NULL, NULL, NULL };
    RTHANDLE    ah[3];
    for (uint32_t i = 0; i < 3; i++)
    {
        ah[i].enmType = RTHANDLETYPE_FILE;
        ah[i].u.hFile = NIL_RTFILE;
    }
    rc = VINF_SUCCESS;
    if ((fFlags & RTPROCEXEC_FLAGS_STDIN_NULL) && RT_SUCCESS(rc))
    {
        aph[0] = &ah[0];
        rc = RTFileOpenBitBucket(&ah[0].u.hFile, RTFILE_O_READ);
    }
    if ((fFlags & RTPROCEXEC_FLAGS_STDOUT_NULL) && RT_SUCCESS(rc))
    {
        aph[1] = &ah[1];
        rc = RTFileOpenBitBucket(&ah[1].u.hFile, RTFILE_O_WRITE);
    }
    if ((fFlags & RTPROCEXEC_FLAGS_STDERR_NULL) && RT_SUCCESS(rc))
    {
        aph[2] = &ah[2];
        rc = RTFileOpenBitBucket(&ah[2].u.hFile, RTFILE_O_WRITE);
    }

    /*
     * Create the process.
     */
    RTPROCESS hProc;
    if (RT_SUCCESS(rc))
        rc = RTProcCreateEx(g_szSvnPath,
                            papszArgs,
                            RTENV_DEFAULT,
                            0 /*fFlags*/,
                            aph[0],
                            aph[1],
                            aph[2],
                            NULL /*pszAsUser*/,
                            NULL /*pszPassword*/,
                            &hProc);

    for (uint32_t i = 0; i < 3; i++)
        RTFileClose(ah[i].u.hFile);

    if (RT_SUCCESS(rc))
        rc = RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, pStatus);
    return rc;
}



/**
 * Executes SVN and gets the output.
 *
 * Standard error is suppressed.
 *
 * @returns VINF_SUCCESS if the command executed successfully.
 * @param   pState              The rewrite state to work on.  Can be NULL.
 * @param   papszArgs           The SVN argument.
 * @param   fNormalFailureOk    Whether normal failure is ok.
 * @param   ppszStdOut          Where to return the output on success.
 */
static int scmSvnRunAndGetOutput(PSCMRWSTATE pState, const char **papszArgs, bool fNormalFailureOk, char **ppszStdOut)
{
    *ppszStdOut = NULL;

    char *pszCmdLine = NULL;
    int rc = RTGetOptArgvToString(&pszCmdLine, papszArgs, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH);
    if (RT_FAILURE(rc))
        return rc;
    ScmVerbose(pState, 2, "executing: %s\n", pszCmdLine);

    RTPROCSTATUS Status;
    rc = RTProcExecToString(g_szSvnPath, papszArgs, RTENV_DEFAULT,
                            RTPROCEXEC_FLAGS_STD_NULL, &Status, ppszStdOut, NULL);

    if (    RT_SUCCESS(rc)
        &&  (   Status.enmReason != RTPROCEXITREASON_NORMAL
             || Status.iStatus != 0) )
    {
        if (fNormalFailureOk || Status.enmReason != RTPROCEXITREASON_NORMAL)
            RTMsgError("%s: %s -> %s %u\n",
                       pszCmdLine,
                       Status.enmReason == RTPROCEXITREASON_NORMAL   ? "exit code"
                       : Status.enmReason == RTPROCEXITREASON_SIGNAL ? "signal"
                       : Status.enmReason == RTPROCEXITREASON_ABEND  ? "abnormal end"
                       : "abducted by alien",
                       Status.iStatus);
        rc = VERR_GENERAL_FAILURE;
    }
    else if (RT_FAILURE(rc))
    {
        if (pState)
            RTMsgError("%s: executing: %s => %Rrc\n", pState->pszFilename, pszCmdLine, rc);
        else
            RTMsgError("executing: %s => %Rrc\n", pszCmdLine, rc);
    }

    if (RT_FAILURE(rc))
    {
        RTStrFree(*ppszStdOut);
        *ppszStdOut = NULL;
    }
    RTStrFree(pszCmdLine);
    return rc;
}


/**
 * Executes SVN.
 *
 * Standard error and standard output is suppressed.
 *
 * @returns VINF_SUCCESS if the command executed successfully.
 * @param   pState              The rewrite state to work on.
 * @param   papszArgs           The SVN argument.
 * @param   fNormalFailureOk    Whether normal failure is ok.
 */
static int scmSvnRun(PSCMRWSTATE pState, const char **papszArgs, bool fNormalFailureOk)
{
    char *pszCmdLine = NULL;
    int rc = RTGetOptArgvToString(&pszCmdLine, papszArgs, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH);
    if (RT_FAILURE(rc))
        return rc;
    ScmVerbose(pState, 2, "executing: %s\n", pszCmdLine);

    /* Lazy bird uses RTProcExecToString. */
    RTPROCSTATUS Status;
    rc = RTProcExec(g_szSvnPath, papszArgs, RTENV_DEFAULT, RTPROCEXEC_FLAGS_STD_NULL, &Status);

    if (    RT_SUCCESS(rc)
        &&  (   Status.enmReason != RTPROCEXITREASON_NORMAL
             || Status.iStatus != 0) )
    {
        if (fNormalFailureOk || Status.enmReason != RTPROCEXITREASON_NORMAL)
            RTMsgError("%s: %s -> %s %u\n",
                       pState->pszFilename,
                       pszCmdLine,
                       Status.enmReason == RTPROCEXITREASON_NORMAL   ? "exit code"
                       : Status.enmReason == RTPROCEXITREASON_SIGNAL ? "signal"
                       : Status.enmReason == RTPROCEXITREASON_ABEND  ? "abnormal end"
                       : "abducted by alien",
                       Status.iStatus);
        rc = VERR_GENERAL_FAILURE;
    }
    else if (RT_FAILURE(rc))
        RTMsgError("%s: %s -> %Rrc\n", pState->pszFilename, pszCmdLine, rc);

    RTStrFree(pszCmdLine);
    return rc;
}


/**
 * Finds the svn binary, updating g_szSvnPath and g_enmSvnVersion.
 */
static void scmSvnFindSvnBinary(PSCMRWSTATE pState)
{
    /* Already been called? */
    if (g_szSvnPath[0] != '\0')
        return;

    /*
     * Locate it.
     */
    /** @todo code page fun... */
#ifdef RT_OS_WINDOWS
    const char *pszEnvVar = RTEnvGet("Path");
#else
    const char *pszEnvVar = RTEnvGet("PATH");
#endif
    if (pszEnvVar)
    {
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        int rc = RTPathTraverseList(pszEnvVar, ';', scmSvnFindSvnBinaryCallback, g_szSvnPath, (void *)sizeof(g_szSvnPath));
#else
        int rc = RTPathTraverseList(pszEnvVar, ':', scmSvnFindSvnBinaryCallback, g_szSvnPath, (void *)sizeof(g_szSvnPath));
#endif
        if (RT_FAILURE(rc))
            strcpy(g_szSvnPath, "svn");
    }
    else
        strcpy(g_szSvnPath, "svn");

    /*
     * Check the version.
     */
    const char *apszArgs[] = { g_szSvnPath, "--version", "--quiet", NULL };
    char *pszVersion;
    int rc = scmSvnRunAndGetOutput(pState, apszArgs, false, &pszVersion);
    if (RT_SUCCESS(rc))
    {
        char *pszStripped = RTStrStrip(pszVersion);
        if (RTStrVersionCompare(pszVersion, "1.7") >= 0)
            g_enmSvnVersion = kScmSvnVersion_1_7;
        else if (RTStrVersionCompare(pszVersion, "1.6") >= 0)
            g_enmSvnVersion = kScmSvnVersion_1_6;
        else
            g_enmSvnVersion = kScmSvnVersion_Ancient;
        RTStrFree(pszVersion);
    }
    else
        g_enmSvnVersion = kScmSvnVersion_Ancient;
}


/**
 * Construct a dot svn filename for the file being rewritten.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state (for the name).
 * @param   pszDir              The directory, including ".svn/".
 * @param   pszSuff             The filename suffix.
 * @param   pszDst              The output buffer.  RTPATH_MAX in size.
 */
static int scmSvnConstructName(PSCMRWSTATE pState, const char *pszDir, const char *pszSuff, char *pszDst)
{
    strcpy(pszDst, pState->pszFilename); /* ASSUMES sizeof(szBuf) <= sizeof(szPath) */
    RTPathStripFilename(pszDst);

    int rc = RTPathAppend(pszDst, RTPATH_MAX, pszDir);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathAppend(pszDst, RTPATH_MAX, RTPathFilename(pState->pszFilename));
        if (RT_SUCCESS(rc))
        {
            size_t cchDst  = strlen(pszDst);
            size_t cchSuff = strlen(pszSuff);
            if (cchDst + cchSuff < RTPATH_MAX)
            {
                memcpy(&pszDst[cchDst], pszSuff, cchSuff + 1);
                return VINF_SUCCESS;
            }
            else
                rc = VERR_BUFFER_OVERFLOW;
        }
    }
    return rc;
}

/**
 * Interprets the specified string as decimal numbers.
 *
 * @returns true if parsed successfully, false if not.
 * @param   pch                 The string (not terminated).
 * @param   cch                 The string length.
 * @param   pu                  Where to return the value.
 */
static bool scmSvnReadNumber(const char *pch, size_t cch, size_t *pu)
{
    size_t u = 0;
    while (cch-- > 0)
    {
        char ch = *pch++;
        if (ch < '0' || ch > '9')
            return false;
        u *= 10;
        u += ch - '0';
    }
    *pu = u;
    return true;
}

#endif /* SCM_WITHOUT_LIBSVN */

/**
 * Checks if the file we're operating on is part of a SVN working copy.
 *
 * @returns true if it is, false if it isn't or we cannot tell.
 * @param   pState              The rewrite state to work on.
 */
bool ScmSvnIsInWorkingCopy(PSCMRWSTATE pState)
{
#ifdef SCM_WITHOUT_LIBSVN
    scmSvnFindSvnBinary(pState);
    if (g_enmSvnVersion < kScmSvnVersion_1_7)
    {
        /*
         * Hack: check if the .svn/text-base/<file>.svn-base file exists.
         */
        char szPath[RTPATH_MAX];
        int rc = scmSvnConstructName(pState, ".svn/text-base/", ".svn-base", szPath);
        if (RT_SUCCESS(rc))
            return RTFileExists(szPath);
    }
    else
    {
        const char *apszArgs[] = { g_szSvnPath, "propget", "svn:no-such-property", pState->pszFilename, NULL };
        char       *pszValue;
        int rc = scmSvnRunAndGetOutput(pState, apszArgs, true, &pszValue);
        if (RT_SUCCESS(rc))
        {
            RTStrFree(pszValue);
            return true;
        }
    }

#else
    NOREF(pState);
#endif
    return false;
}

/**
 * Checks if the specified directory is part of a SVN working copy.
 *
 * @returns true if it is, false if it isn't or we cannot tell.
 * @param   pszDir              The directory in question.
 */
bool ScmSvnIsDirInWorkingCopy(const char *pszDir)
{
#ifdef SCM_WITHOUT_LIBSVN
    scmSvnFindSvnBinary(NULL);
    if (g_enmSvnVersion < kScmSvnVersion_1_7)
    {
        /*
         * Hack: check if the .svn/ dir exists.
         */
        char szPath[RTPATH_MAX];
        int rc = RTPathJoin(szPath, sizeof(szPath), pszDir, ".svn");
        if (RT_SUCCESS(rc))
            return RTDirExists(szPath);
    }
    else
    {
        const char *apszArgs[] = { g_szSvnPath, "propget", "svn:no-such-property", pszDir, NULL };
        char       *pszValue;
        int rc = scmSvnRunAndGetOutput(NULL, apszArgs, true, &pszValue);
        if (RT_SUCCESS(rc))
        {
            RTStrFree(pszValue);
            return true;
        }
    }

#else
    NOREF(pState);
#endif
    return false;
}

/**
 * Queries the value of an SVN property.
 *
 * This will automatically adjust for scheduled changes.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_STATE if not a SVN WC file.
 * @retval  VERR_NOT_FOUND if the property wasn't found.
 * @param   pState              The rewrite state to work on.
 * @param   pszName             The property name.
 * @param   ppszValue           Where to return the property value.  Free this
 *                              using RTStrFree.  Optional.
 */
int ScmSvnQueryProperty(PSCMRWSTATE pState, const char *pszName, char **ppszValue)
{
    /*
     * Look it up in the scheduled changes.
     */
    size_t i = pState->cSvnPropChanges;
    while (i-- > 0)
        if (!strcmp(pState->paSvnPropChanges[i].pszName, pszName))
        {
            const char *pszValue = pState->paSvnPropChanges[i].pszValue;
            if (!pszValue)
                return VERR_NOT_FOUND;
            if (ppszValue)
                return RTStrDupEx(ppszValue, pszValue);
            return VINF_SUCCESS;
        }

#ifdef SCM_WITHOUT_LIBSVN
    int rc;
    scmSvnFindSvnBinary(pState);
    if (g_enmSvnVersion < kScmSvnVersion_1_7)
    {
        /*
         * Hack: Read the .svn/props/<file>.svn-work file exists.
         */
        char szPath[RTPATH_MAX];
        rc = scmSvnConstructName(pState, ".svn/props/", ".svn-work", szPath);
        if (RT_SUCCESS(rc) && !RTFileExists(szPath))
            rc = scmSvnConstructName(pState, ".svn/prop-base/", ".svn-base", szPath);
        if (RT_SUCCESS(rc))
        {
            SCMSTREAM Stream;
            rc = ScmStreamInitForReading(&Stream, szPath);
            if (RT_SUCCESS(rc))
            {
                /*
                 * The current format is K len\n<name>\nV len\n<value>\n" ... END.
                 */
                rc = VERR_NOT_FOUND;
                size_t const    cchName = strlen(pszName);
                SCMEOL          enmEol;
                size_t          cchLine;
                const char     *pchLine;
                while ((pchLine = ScmStreamGetLine(&Stream, &cchLine, &enmEol)) != NULL)
                {
                    /*
                     * Parse the 'K num' / 'END' line.
                     */
                    if (   cchLine == 3
                        && !memcmp(pchLine, "END", 3))
                        break;
                    size_t cchKey;
                    if (   cchLine < 3
                        || pchLine[0] != 'K'
                        || pchLine[1] != ' '
                        || !scmSvnReadNumber(&pchLine[2], cchLine - 2, &cchKey)
                        || cchKey == 0
                        || cchKey > 4096)
                    {
                        RTMsgError("%s:%u: Unexpected data '%.*s'\n", szPath, ScmStreamTellLine(&Stream), cchLine, pchLine);
                        rc = VERR_PARSE_ERROR;
                        break;
                    }

                    /*
                     * Match the key and skip to the value line.  Don't bother with
                     * names containing EOL markers.
                     */
                    size_t const offKey = ScmStreamTell(&Stream);
                    bool fMatch = cchName == cchKey;
                    if (fMatch)
                    {
                        pchLine = ScmStreamGetLine(&Stream, &cchLine, &enmEol);
                        if (!pchLine)
                            break;
                        fMatch = cchLine == cchName
                              && !memcmp(pchLine, pszName, cchName);
                    }

                    if (RT_FAILURE(ScmStreamSeekAbsolute(&Stream, offKey + cchKey)))
                        break;
                    if (RT_FAILURE(ScmStreamSeekByLine(&Stream, ScmStreamTellLine(&Stream) + 1)))
                        break;

                    /*
                     * Read and Parse the 'V num' line.
                     */
                    pchLine = ScmStreamGetLine(&Stream, &cchLine, &enmEol);
                    if (!pchLine)
                        break;
                    size_t cchValue;
                    if (   cchLine < 3
                        || pchLine[0] != 'V'
                        || pchLine[1] != ' '
                        || !scmSvnReadNumber(&pchLine[2], cchLine - 2, &cchValue)
                        || cchValue > _1M)
                    {
                        RTMsgError("%s:%u: Unexpected data '%.*s'\n", szPath, ScmStreamTellLine(&Stream), cchLine, pchLine);
                        rc = VERR_PARSE_ERROR;
                        break;
                    }

                    /*
                     * If we have a match, allocate a return buffer and read the
                     * value into it.  Otherwise skip this value and continue
                     * searching.
                     */
                    if (fMatch)
                    {
                        if (!ppszValue)
                            rc = VINF_SUCCESS;
                        else
                        {
                            char *pszValue;
                            rc = RTStrAllocEx(&pszValue, cchValue + 1);
                            if (RT_SUCCESS(rc))
                            {
                                rc = ScmStreamRead(&Stream, pszValue, cchValue);
                                if (RT_SUCCESS(rc))
                                    *ppszValue = pszValue;
                                else
                                    RTStrFree(pszValue);
                            }
                        }
                        break;
                    }

                    if (RT_FAILURE(ScmStreamSeekRelative(&Stream, cchValue)))
                        break;
                    if (RT_FAILURE(ScmStreamSeekByLine(&Stream, ScmStreamTellLine(&Stream) + 1)))
                        break;
                }

                if (RT_FAILURE(ScmStreamGetStatus(&Stream)))
                {
                    rc = ScmStreamGetStatus(&Stream);
                    RTMsgError("%s: stream error %Rrc\n", szPath, rc);
                }
                ScmStreamDelete(&Stream);
            }
        }

        if (rc == VERR_FILE_NOT_FOUND)
            rc = VERR_NOT_FOUND;
    }
    else
    {
        const char *apszArgs[] = { g_szSvnPath, "propget", "--strict", pszName, pState->pszFilename, NULL };
        char       *pszValue;
        rc = scmSvnRunAndGetOutput(pState, apszArgs, false, &pszValue);
        if (RT_SUCCESS(rc))
        {
            if (pszValue && *pszValue)
            {
                if (ppszValue)
                {
                    *ppszValue = pszValue;
                    pszValue = NULL;
                }
            }
            else
                rc = VERR_NOT_FOUND;
            RTStrFree(pszValue);
        }
    }
    return rc;

#else
    NOREF(pState);
#endif
    return VERR_NOT_FOUND;
}


/**
 * Schedules the setting of a property.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_STATE if not a SVN WC file.
 * @param   pState              The rewrite state to work on.
 * @param   pszName             The name of the property to set.
 * @param   pszValue            The value.  NULL means deleting it.
 */
int ScmSvnSetProperty(PSCMRWSTATE pState, const char *pszName, const char *pszValue)
{
    /*
     * Update any existing entry first.
     */
    size_t i = pState->cSvnPropChanges;
    while (i-- > 0)
        if (!strcmp(pState->paSvnPropChanges[i].pszName,  pszName))
        {
            if (!pszValue)
            {
                RTStrFree(pState->paSvnPropChanges[i].pszValue);
                pState->paSvnPropChanges[i].pszValue = NULL;
            }
            else
            {
                char *pszCopy;
                int rc = RTStrDupEx(&pszCopy, pszValue);
                if (RT_FAILURE(rc))
                    return rc;
                pState->paSvnPropChanges[i].pszValue = pszCopy;
            }
            return VINF_SUCCESS;
        }

    /*
     * Insert a new entry.
     */
    i = pState->cSvnPropChanges;
    if ((i % 32) == 0)
    {
        void *pvNew = RTMemRealloc(pState->paSvnPropChanges, (i + 32) * sizeof(SCMSVNPROP));
        if (!pvNew)
            return VERR_NO_MEMORY;
        pState->paSvnPropChanges = (PSCMSVNPROP)pvNew;
    }

    pState->paSvnPropChanges[i].pszName  = RTStrDup(pszName);
    pState->paSvnPropChanges[i].pszValue = pszValue ? RTStrDup(pszValue) : NULL;
    if (   pState->paSvnPropChanges[i].pszName
        && (pState->paSvnPropChanges[i].pszValue || !pszValue) )
        pState->cSvnPropChanges = i + 1;
    else
    {
        RTStrFree(pState->paSvnPropChanges[i].pszName);
        pState->paSvnPropChanges[i].pszName = NULL;
        RTStrFree(pState->paSvnPropChanges[i].pszValue);
        pState->paSvnPropChanges[i].pszValue = NULL;
        return VERR_NO_MEMORY;
    }
    return VINF_SUCCESS;
}


/**
 * Schedules a property deletion.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state to work on.
 * @param   pszName             The name of the property to delete.
 */
int ScmSvnDelProperty(PSCMRWSTATE pState, const char *pszName)
{
    return ScmSvnSetProperty(pState, pszName, NULL);
}


/**
 * Applies any SVN property changes to the work copy of the file.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state which SVN property changes
 *                              should be applied.
 */
int ScmSvnDisplayChanges(PSCMRWSTATE pState)
{
    size_t i = pState->cSvnPropChanges;
    while (i-- > 0)
    {
        const char *pszName  = pState->paSvnPropChanges[i].pszName;
        const char *pszValue = pState->paSvnPropChanges[i].pszValue;
        if (pszValue)
            ScmVerbose(pState, 0, "svn propset '%s' '%s' %s\n", pszName, pszValue, pState->pszFilename);
        else
            ScmVerbose(pState, 0, "svn propdel '%s' %s\n", pszName, pState->pszFilename);
    }

    return VINF_SUCCESS;
}

/**
 * Applies any SVN property changes to the work copy of the file.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state which SVN property changes
 *                              should be applied.
 */
int ScmSvnApplyChanges(PSCMRWSTATE pState)
{
#ifdef SCM_WITHOUT_LIBSVN
    scmSvnFindSvnBinary(pState);

    /*
     * Iterate thru the changes and apply them by starting the svn client.
     */
    for (size_t i = 0; i < pState->cSvnPropChanges; i++)
    {
        const char *apszArgv[6];
        apszArgv[0] = g_szSvnPath;
        apszArgv[1] = pState->paSvnPropChanges[i].pszValue ? "propset" : "propdel";
        apszArgv[2] = pState->paSvnPropChanges[i].pszName;
        int iArg = 3;
        if (pState->paSvnPropChanges[i].pszValue)
            apszArgv[iArg++] = pState->paSvnPropChanges[i].pszValue;
        apszArgv[iArg++] = pState->pszFilename;
        apszArgv[iArg++] = NULL;

        int rc = scmSvnRun(pState, apszArgv, false);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}



