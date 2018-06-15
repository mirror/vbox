/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, observer.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/fuzz.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/md5.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>


/** Poll ID for the reading end of the stdout pipe from the client process. */
#define RTFUZZOBS_EXEC_CTX_POLL_ID_STDOUT 0
/** Poll ID for the reading end of the stderr pipe from the client process. */
#define RTFUZZOBS_EXEC_CTX_POLL_ID_STDERR 1
/** Poll ID for the writing end of the stdin pipe to the client process. */
#define RTFUZZOBS_EXEC_CTX_POLL_ID_STDIN  2


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the internal fuzzing observer state. */
typedef struct RTFUZZOBSINT *PRTFUZZOBSINT;


/**
 * Observer thread state for one process.
 */
typedef struct RTFUZZOBSTHRD
{
    /** The thread handle. */
    RTTHREAD                    hThread;
    /** The observer ID. */
    uint32_t                    idObs;
    /** Flag whether to shutdown. */
    volatile bool               fShutdown;
    /** Pointer to te global observer state. */
    PRTFUZZOBSINT               pFuzzObs;
    /** Current fuzzer input. */
    RTFUZZINPUT                 hFuzzInput;
    /** Flag whether to keep the input. */
    bool                        fKeepInput;
    /** Flag whether a new input is waiting. */
    volatile bool               fNewInput;
} RTFUZZOBSTHRD;
/** Pointer to an observer thread state. */
typedef RTFUZZOBSTHRD *PRTFUZZOBSTHRD;


/**
 * Internal fuzzing observer state.
 */
typedef struct RTFUZZOBSINT
{
    /** The fuzzing context used for this observer. */
    RTFUZZCTX                   hFuzzCtx;
    /** Temp directory for input files. */
    char                       *pszTmpDir;
    /** Results directory. */
    char                       *pszResultsDir;
    /** The binary to run. */
    char                       *pszBinary;
    /** Arguments to run the binary with, terminated by a NULL entry. */
    char                      **papszArgs;
    /** Number of arguments. */
    uint32_t                    cArgs;
    /** Maximum time to wait for the client to terminate until it is considered hung and killed. */
    RTMSINTERVAL                msWaitMax;
    /** Flags controlling how the binary is executed. */
    uint32_t                    fFlags;
    /** Flag whether to shutdown the master and all workers. */
    volatile bool               fShutdown;
    /** Global observer thread handle. */
    RTTHREAD                    hThreadGlobal;
    /** The event semaphore handle for the global observer thread. */
    RTSEMEVENT                  hEvtGlobal;
    /** Notification event bitmap. */
    volatile uint64_t           bmEvt;
    /** Number of threads created - one for each process. */
    uint32_t                    cThreads;
    /** Pointer to the array of observer thread states. */
    PRTFUZZOBSTHRD              paObsThreads;
} RTFUZZOBSINT;


/**
 * Stdout/Stderr buffer.
 */
typedef struct RTFUZZOBSSTDOUTERRBUF
{
    /** Current amount buffered. */
    size_t                      cbBuf;
    /** Maxmium amount to buffer. */
    size_t                      cbBufMax;
    /** Base pointer to the data buffer. */
    uint8_t                     *pbBase;
} RTFUZZOBSSTDOUTERRBUF;
/** Pointer to a stdout/stderr buffer. */
typedef RTFUZZOBSSTDOUTERRBUF *PRTFUZZOBSSTDOUTERRBUF;


/**
 * Worker execution context.
 */
typedef struct RTFUZZOBSEXECCTX
{
    /** The stdout pipe handle - reading end. */
    RTPIPE                      hPipeStdoutR;
    /** The stdout pipe handle - writing end. */
    RTPIPE                      hPipeStdoutW;
    /** The stderr pipe handle - reading end. */
    RTPIPE                      hPipeStderrR;
    /** The stderr pipe handle - writing end. */
    RTPIPE                      hPipeStderrW;
    /** The stdin pipe handle - reading end. */
    RTPIPE                      hPipeStdinR;
    /** The stind pipe handle - writing end. */
    RTPIPE                      hPipeStdinW;
    /** The stdout handle. */
    RTHANDLE                    StdoutHandle;
    /** The stderr handle. */
    RTHANDLE                    StderrHandle;
    /** The stdin handle. */
    RTHANDLE                    StdinHandle;
    /** The pollset to monitor. */
    RTPOLLSET                   hPollSet;
    /** The process to monitor. */
    RTPROCESS                   hProc;
    /** Execution time of the process. */
    RTMSINTERVAL                msExec;
    /** Current input data pointer. */
    uint8_t                     *pbInputCur;
    /** Number of bytes left for the input. */
    size_t                      cbInputLeft;
    /** The stdout data buffer. */
    RTFUZZOBSSTDOUTERRBUF       StdOutBuf;
    /** The stderr data buffer. */
    RTFUZZOBSSTDOUTERRBUF       StdErrBuf;
    /** Modified arguments vector - variable in size. */
    char                        *apszArgs[1];
} RTFUZZOBSEXECCTX;
/** Pointer to an execution context. */
typedef RTFUZZOBSEXECCTX *PRTFUZZOBSEXECCTX;
/** Pointer to an execution context pointer. */
typedef PRTFUZZOBSEXECCTX *PPRTFUZZOBSEXECCTX;


/**
 * A variable descriptor.
 */
typedef struct RTFUZZOBSVARIABLE
{
    /** The variable. */
    const char                  *pszVar;
    /** Length of the variable in characters - excluding the terminator. */
    uint32_t                    cchVar;
    /** The replacement value. */
    const char                  *pszVal;
} RTFUZZOBSVARIABLE;
/** Pointer to a variable descriptor. */
typedef RTFUZZOBSVARIABLE *PRTFUZZOBSVARIABLE;


/**
 * Initializes the given stdout/stderr buffer.
 *
 * @returns nothing.
 * @param   pBuf                The buffer to initialize.
 */
static void rtFuzzObsStdOutErrBufInit(PRTFUZZOBSSTDOUTERRBUF pBuf)
{
    pBuf->cbBuf    = 0;
    pBuf->cbBufMax = 0;
    pBuf->pbBase   = NULL;
}


/**
 * Frees all allocated resources in the given stdout/stderr buffer.
 *
 * @returns nothing.
 * @param   pBuf                The buffer to free.
 */
static void rtFuzzObsStdOutErrBufFree(PRTFUZZOBSSTDOUTERRBUF pBuf)
{
    if (pBuf->pbBase)
        RTMemFree(pBuf->pbBase);
}


/**
 * Clears the given stdout/stderr buffer.
 *
 * @returns nothing.
 * @param   pBuf                The buffer to clear.
 */
static void rtFuzzObsStdOutErrBufClear(PRTFUZZOBSSTDOUTERRBUF pBuf)
{
    pBuf->cbBuf = 0;
}


/**
 * Fills the given stdout/stderr buffer from the given pipe.
 *
 * @returns IPRT status code.
 * @param   pBuf                The buffer to fill.
 * @param   hPipeRead           The pipe to read from.
 */
static int rtFuzzObsStdOutErrBufFill(PRTFUZZOBSSTDOUTERRBUF pBuf, RTPIPE hPipeRead)
{
    int rc = VINF_SUCCESS;

    size_t cbRead = 0;
    size_t cbThisRead = 0;
    do
    {
        cbThisRead = pBuf->cbBufMax - pBuf->cbBuf;
        if (!cbThisRead)
        {
            /* Try to increase the buffer. */
            uint8_t *pbNew = (uint8_t *)RTMemRealloc(pBuf->pbBase, pBuf->cbBufMax + _4K);
            if (RT_LIKELY(pbNew))
            {
                pBuf->cbBufMax += _4K;
                pBuf->pbBase   = pbNew;
            }
            cbThisRead = pBuf->cbBufMax - pBuf->cbBuf;
        }

        if (cbThisRead)
        {
            rc = RTPipeRead(hPipeRead, pBuf->pbBase + pBuf->cbBuf, cbThisRead, &cbRead);
            if (RT_SUCCESS(rc))
                pBuf->cbBuf += cbRead;
        }
        else
            rc = VERR_NO_MEMORY;
    } while (   RT_SUCCESS(rc)
             && cbRead == cbThisRead);

    return rc;
}


/**
 * Writes the given stdout/stderr buffer to the given filename.
 *
 * @returns IPRT status code.
 * @param   pBuf                The buffer to write.
 * @param   pszFilename         The filename to write the buffer to.
 */
static int rtFuzzStdOutErrBufWriteToFile(PRTFUZZOBSSTDOUTERRBUF pBuf, const char *pszFilename)
{
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(hFile, pBuf->pbBase, pBuf->cbBuf, NULL);
        AssertRC(rc);
        RTFileClose(hFile);

        if (RT_FAILURE(rc))
            RTFileDelete(pszFilename);
    }

    return rc;
}


/**
 * Replaces a variable with its value.
 *
 * @returns VINF_SUCCESS or VERR_NO_STR_MEMORY.
 * @param   ppszNew             In/Out.
 * @param   pcchNew             In/Out. (Messed up on failure.)
 * @param   offVar              Variable offset.
 * @param   cchVar              Variable length.
 * @param   pszValue            The value.
 * @param   cchValue            Value length.
 */
static int rtFuzzObsReplaceStringVariable(char **ppszNew, size_t *pcchNew, size_t offVar, size_t cchVar,
                                          const char *pszValue, size_t cchValue)
{
    size_t const cchAfter = *pcchNew - offVar - cchVar;
    if (cchVar < cchValue)
    {
        *pcchNew += cchValue - cchVar;
        int rc = RTStrRealloc(ppszNew, *pcchNew + 1);
        if (RT_FAILURE(rc))
            return rc;
    }

    char *pszNew = *ppszNew;
    memmove(&pszNew[offVar + cchValue], &pszNew[offVar + cchVar], cchAfter + 1);
    memcpy(&pszNew[offVar], pszValue, cchValue);
    return VINF_SUCCESS;
}


/**
 * Replace the variables found in the source string, returning a new string that
 * lives on the string heap.
 *
 * @returns IPRT status code.
 * @param   pszSrc              The source string.
 * @param   paVars              Pointer to the array of known variables.
 * @param   ppszNew             Where to return the new string.
 */
static int rtFuzzObsReplaceStringVariables(const char *pszSrc, PRTFUZZOBSVARIABLE paVars, char **ppszNew)
{
    /* Lazy approach that employs memmove.  */
    int     rc        = VINF_SUCCESS;
    size_t  cchNew    = strlen(pszSrc);
    char   *pszNew    = RTStrDup(pszSrc);

    if (paVars)
    {
        char   *pszDollar = pszNew;
        while ((pszDollar = strchr(pszDollar, '$')) != NULL)
        {
            if (pszDollar[1] == '{')
            {
                const char *pszEnd = strchr(&pszDollar[2], '}');
                if (pszEnd)
                {
                    size_t const cchVar    = pszEnd - pszDollar + 1; /* includes "${}" */
                    size_t       offDollar = pszDollar - pszNew;
                    PRTFUZZOBSVARIABLE pVar = paVars;
                    while (pVar->pszVar != NULL)
                    {
                        if (   cchVar == pVar->cchVar
                            && !memcmp(pszDollar, pVar->pszVar, cchVar))
                        {
                            size_t const cchValue = strlen(pVar->pszVal);
                            rc = rtFuzzObsReplaceStringVariable(&pszNew, &cchNew, offDollar,
                                                                cchVar, pVar->pszVal, cchValue);
                            offDollar += cchValue;
                            break;
                        }

                        pVar++;
                    }

                    pszDollar = &pszNew[offDollar];

                    if (RT_FAILURE(rc))
                    {
                        RTStrFree(pszNew);
                        *ppszNew = NULL;
                        return rc;
                    }
                }
            }
        }
    }

    *ppszNew = pszNew;
    return rc;
}

/**
 * Prepares the argument vector for the child process.
 *
 * @returns IPRT status code.
 * @param   pThis               The internal fuzzing observer state.
 * @param   pExecCtx            The execution context to prepare the argument vector for.
 * @param   paVars              Pointer to the array of known variables.
 */
static int rtFuzzObsExecCtxArgvPrepare(PRTFUZZOBSINT pThis, PRTFUZZOBSEXECCTX pExecCtx, PRTFUZZOBSVARIABLE paVars)
{
    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < pThis->cArgs && RT_SUCCESS(rc); i++)
        rc = rtFuzzObsReplaceStringVariables(pThis->papszArgs[i], paVars, &pExecCtx->apszArgs[i]);

    return rc;
}


/**
 * Creates a new execution context.
 *
 * @returns IPRT status code.
 * @param   ppExecCtx           Where to store the pointer to the execution context on success.
 * @param   pThis               The internal fuzzing observer state.
 */
static int rtFuzzObsExecCtxCreate(PPRTFUZZOBSEXECCTX ppExecCtx, PRTFUZZOBSINT pThis)
{
    int rc = VINF_SUCCESS;
    PRTFUZZOBSEXECCTX pExecCtx = (PRTFUZZOBSEXECCTX)RTMemAllocZ(RT_OFFSETOF(RTFUZZOBSEXECCTX, apszArgs[pThis->cArgs + 1]));
    if (RT_LIKELY(pExecCtx))
    {
        pExecCtx->hPipeStdoutR     = NIL_RTPIPE;
        pExecCtx->hPipeStdoutW     = NIL_RTPIPE;
        pExecCtx->hPipeStderrR     = NIL_RTPIPE;
        pExecCtx->hPipeStderrW     = NIL_RTPIPE;
        pExecCtx->hPipeStdinR      = NIL_RTPIPE;
        pExecCtx->hPipeStdinW      = NIL_RTPIPE;
        pExecCtx->hPollSet         = NIL_RTPOLLSET;
        pExecCtx->hProc            = NIL_RTPROCESS;
        pExecCtx->msExec           = 0;
        rtFuzzObsStdOutErrBufInit(&pExecCtx->StdOutBuf);
        rtFuzzObsStdOutErrBufInit(&pExecCtx->StdErrBuf);

        rc = RTPollSetCreate(&pExecCtx->hPollSet);
        if (RT_SUCCESS(rc))
        {
            rc = RTPipeCreate(&pExecCtx->hPipeStdoutR, &pExecCtx->hPipeStdoutW, RTPIPE_C_INHERIT_WRITE);
            if (RT_SUCCESS(rc))
            {
                RTHANDLE Handle;
                Handle.enmType = RTHANDLETYPE_PIPE;
                Handle.u.hPipe = pExecCtx->hPipeStdoutR;
                rc = RTPollSetAdd(pExecCtx->hPollSet, &Handle, RTPOLL_EVT_READ, RTFUZZOBS_EXEC_CTX_POLL_ID_STDOUT);
                AssertRC(rc);

                rc = RTPipeCreate(&pExecCtx->hPipeStderrR, &pExecCtx->hPipeStderrW, RTPIPE_C_INHERIT_WRITE);
                if (RT_SUCCESS(rc))
                {
                    Handle.u.hPipe = pExecCtx->hPipeStderrR;
                    rc = RTPollSetAdd(pExecCtx->hPollSet, &Handle, RTPOLL_EVT_READ, RTFUZZOBS_EXEC_CTX_POLL_ID_STDERR);
                    AssertRC(rc);

                    /* Create the stdin pipe handles if not a file input. */
                    if (!(pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE))
                    {
                        rc = RTPipeCreate(&pExecCtx->hPipeStdinR, &pExecCtx->hPipeStdinW, RTPIPE_C_INHERIT_READ);
                        if (RT_SUCCESS(rc))
                        {
                            pExecCtx->StdinHandle.enmType = RTHANDLETYPE_PIPE;
                            pExecCtx->StdinHandle.u.hPipe = pExecCtx->hPipeStdinR;

                            Handle.u.hPipe = pExecCtx->hPipeStdinW;
                            rc = RTPollSetAdd(pExecCtx->hPollSet, &Handle, RTPOLL_EVT_WRITE, RTFUZZOBS_EXEC_CTX_POLL_ID_STDIN);
                            AssertRC(rc);
                        }
                    }
                    else
                    {
                        pExecCtx->StdinHandle.enmType = RTHANDLETYPE_PIPE;
                        pExecCtx->StdinHandle.u.hPipe = NIL_RTPIPE;
                    }

                    if (RT_SUCCESS(rc))
                    {
                        pExecCtx->StdoutHandle.enmType = RTHANDLETYPE_PIPE;
                        pExecCtx->StdoutHandle.u.hPipe = pExecCtx->hPipeStdoutW;
                        pExecCtx->StderrHandle.enmType = RTHANDLETYPE_PIPE;
                        pExecCtx->StderrHandle.u.hPipe = pExecCtx->hPipeStderrW;
                        *ppExecCtx = pExecCtx;
                        return VINF_SUCCESS;
                    }

                    RTPipeClose(pExecCtx->hPipeStderrR);
                    RTPipeClose(pExecCtx->hPipeStderrW);
                }

                RTPipeClose(pExecCtx->hPipeStdoutR);
                RTPipeClose(pExecCtx->hPipeStdoutW);
            }

            RTPollSetDestroy(pExecCtx->hPollSet);
        }

        RTMemFree(pExecCtx);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Destroys the given execution context.
 *
 * @returns nothing.
 * @param   pThis               The internal fuzzing observer state.
 * @param   pExecCtx            The execution context to destroy.
 */
static void rtFuzzObsExecCtxDestroy(PRTFUZZOBSINT pThis, PRTFUZZOBSEXECCTX pExecCtx)
{
    RTPipeClose(pExecCtx->hPipeStdoutR);
    RTPipeClose(pExecCtx->hPipeStdoutW);
    RTPipeClose(pExecCtx->hPipeStderrR);
    RTPipeClose(pExecCtx->hPipeStderrW);

    if (!(pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE))
    {
        RTPipeClose(pExecCtx->hPipeStdinR);
        RTPipeClose(pExecCtx->hPipeStdinW);
    }

    RTPollSetDestroy(pExecCtx->hPollSet);
    char **ppszArg = &pExecCtx->apszArgs[0];
    while (*ppszArg != NULL)
    {
        RTStrFree(*ppszArg);
        ppszArg++;
    }

    rtFuzzObsStdOutErrBufFree(&pExecCtx->StdOutBuf);
    rtFuzzObsStdOutErrBufFree(&pExecCtx->StdErrBuf);
    RTMemFree(pExecCtx);
}


/**
 * Runs the client binary pumping all data back and forth waiting for the client to finish.
 *
 * @returns IPRT status code.
 * @retval  VERR_TIMEOUT if the client didn't finish in the given deadline and was killed.
 * @param   pThis               The internal fuzzing observer state.
 * @param   pExecCtx            The execution context.
 * @param   pProcStat           Where to store the process exit status on success.
 */
static int rtFuzzObsExecCtxClientRun(PRTFUZZOBSINT pThis, PRTFUZZOBSEXECCTX pExecCtx, PRTPROCSTATUS pProcStat)
{
    rtFuzzObsStdOutErrBufClear(&pExecCtx->StdOutBuf);
    rtFuzzObsStdOutErrBufClear(&pExecCtx->StdErrBuf);

    int rc = RTProcCreateEx(pThis->pszBinary, &pExecCtx->apszArgs[0], RTENV_DEFAULT, 0 /*fFlags*/, &pExecCtx->StdinHandle,
                            &pExecCtx->StdoutHandle, &pExecCtx->StderrHandle, NULL, NULL, &pExecCtx->hProc);
    if (RT_SUCCESS(rc))
    {
        uint64_t tsMilliesStart = RTTimeSystemMilliTS();
        for (;;)
        {
            /* Wait a bit for something to happen on one of the pipes. */
            uint32_t fEvtsRecv = 0;
            uint32_t idEvt = 0;
            rc = RTPoll(pExecCtx->hPollSet, 10 /*cMillies*/, &fEvtsRecv, &idEvt);
            if (RT_SUCCESS(rc))
            {
                if (idEvt == RTFUZZOBS_EXEC_CTX_POLL_ID_STDOUT)
                {
                    Assert(fEvtsRecv & RTPOLL_EVT_READ);
                    rc = rtFuzzObsStdOutErrBufFill(&pExecCtx->StdOutBuf, pExecCtx->hPipeStdoutR);
                    AssertRC(rc);
                }
                else if (idEvt == RTFUZZOBS_EXEC_CTX_POLL_ID_STDERR)
                {
                    Assert(fEvtsRecv & RTPOLL_EVT_READ);
                    rc = rtFuzzObsStdOutErrBufFill(&pExecCtx->StdErrBuf, pExecCtx->hPipeStderrR);
                    AssertRC(rc);
                }
                else if (idEvt == RTFUZZOBS_EXEC_CTX_POLL_ID_STDIN)
                {
                    /* Feed the next input. */
                    Assert(fEvtsRecv & RTPOLL_EVT_WRITE);
                    size_t cbWritten = 0;
                    rc = RTPipeWrite(pExecCtx->hPipeStdinW, pExecCtx->pbInputCur, pExecCtx->cbInputLeft, &cbWritten);
                    if (RT_SUCCESS(rc))
                    {
                        pExecCtx->cbInputLeft -= cbWritten;
                        if (!pExecCtx->cbInputLeft)
                        {
                            /* Close stdin pipe. */
                            rc = RTPollSetRemove(pExecCtx->hPollSet, RTFUZZOBS_EXEC_CTX_POLL_ID_STDIN);
                            AssertRC(rc);
                            RTPipeClose(pExecCtx->hPipeStdinW);
                        }
                    }
                }
                else
                    AssertMsgFailed(("Invalid poll ID returned: %u!\n", idEvt));
            }
            else
                Assert(rc == VERR_TIMEOUT);

            /* Check the process status. */
            rc = RTProcWait(pExecCtx->hProc, RTPROCWAIT_FLAGS_NOBLOCK, pProcStat);
            if (RT_SUCCESS(rc))
                break;
            else
            {
                Assert(rc == VERR_PROCESS_RUNNING);
                /* Check whether we reached the limit. */
                if (RTTimeSystemMilliTS() - tsMilliesStart > pThis->msWaitMax)
                {
                    rc = VERR_TIMEOUT;
                    break;
                }
            }
        } /* for (;;) */

        /* Kill the process on a timeout. */
        if (rc == VERR_TIMEOUT)
        {
            int rc2 = RTProcTerminate(pExecCtx->hProc);
            AssertRC(rc2);
        }
    }

    return rc;
}


/**
 * Adds the input to the results directory.
 *
 * @returns IPRT status code.
 * @param   pThis               The internal fuzzing observer state.
 * @param   hFuzzInput          Fuzzing input handle to write.
 * @param   pExecCtx            Execution context.
 */
static int rtFuzzObsAddInputToResults(PRTFUZZOBSINT pThis, RTFUZZINPUT hFuzzInput, PRTFUZZOBSEXECCTX pExecCtx)
{
    char aszDigest[RTMD5_STRING_LEN + 1];
    int rc = RTFuzzInputQueryDigestString(hFuzzInput, &aszDigest[0], sizeof(aszDigest));
    if (RT_SUCCESS(rc))
    {
        /* Create a directory. */
        char szPath[RTPATH_MAX];
        rc = RTPathJoin(szPath, sizeof(szPath), pThis->pszResultsDir, &aszDigest[0]);
        AssertRC(rc);

        rc = RTDirCreate(&szPath[0], 0700, 0 /*fCreate*/);
        if (RT_SUCCESS(rc))
        {
            /* Write the input. */
            char szTmp[RTPATH_MAX];
            rc = RTPathJoin(szTmp, sizeof(szTmp), &szPath[0], "input");
            AssertRC(rc);

            rc = RTFuzzInputWriteToFile(hFuzzInput, &szTmp[0]);
            if (RT_SUCCESS(rc))
            {
                /* Stdout and Stderr. */
                rc = RTPathJoin(szTmp, sizeof(szTmp), &szPath[0], "stdout");
                AssertRC(rc);
                rc = rtFuzzStdOutErrBufWriteToFile(&pExecCtx->StdOutBuf, &szTmp[0]);
                if (RT_SUCCESS(rc))
                {
                    rc = RTPathJoin(szTmp, sizeof(szTmp), &szPath[0], "stderr");
                    AssertRC(rc);
                    rc = rtFuzzStdOutErrBufWriteToFile(&pExecCtx->StdOutBuf, &szTmp[0]);
                }
            }
        }
    }

    return rc;
}


/**
 * Fuzzing observer worker loop.
 *
 * @returns IPRT status code.
 * @param   hThrd               The thread handle.
 * @param   pvUser              Opaque user data.
 */
static DECLCALLBACK(int) rtFuzzObsWorkerLoop(RTTHREAD hThrd, void *pvUser)
{
    PRTFUZZOBSTHRD pObsThrd = (PRTFUZZOBSTHRD)pvUser;
    PRTFUZZOBSINT pThis = pObsThrd->pFuzzObs;
    PRTFUZZOBSEXECCTX pExecCtx = NULL;

    int rc = rtFuzzObsExecCtxCreate(&pExecCtx, pThis);
    if (RT_FAILURE(rc))
        return rc;

    while (!pObsThrd->fShutdown)
    {
        char szInput[RTPATH_MAX];

        /* Wait for work. */
        rc = RTThreadUserWait(hThrd, RT_INDEFINITE_WAIT);
        AssertRC(rc);

        if (pObsThrd->fShutdown)
            break;

        if (!ASMAtomicXchgBool(&pObsThrd->fNewInput, false))
            continue;

        AssertPtr(pObsThrd->hFuzzInput);

        if (pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE)
        {
            char szFilename[32];

            ssize_t cbBuf = RTStrPrintf2(&szFilename[0], sizeof(szFilename), "%u", pObsThrd->idObs);
            Assert(cbBuf > 0); RT_NOREF(cbBuf);

            RT_ZERO(szInput);
            rc = RTPathJoin(szInput, sizeof(szInput), pThis->pszTmpDir, &szFilename[0]);
            AssertRC(rc);

            rc = RTFuzzInputWriteToFile(pObsThrd->hFuzzInput, &szInput[0]);
            if (RT_SUCCESS(rc))
            {
                RTFUZZOBSVARIABLE aVar[2] = {
                    { "${INPUT}", sizeof("${INPUT}") - 1, &szInput[0] },
                    { NULL,       0,                      NULL        }
                };
                rc = rtFuzzObsExecCtxArgvPrepare(pThis, pExecCtx, &aVar[0]);
            }
        }
        else
        {
            rc = RTFuzzInputQueryData(pObsThrd->hFuzzInput, (void **)&pExecCtx->pbInputCur, &pExecCtx->cbInputLeft);
            if (RT_SUCCESS(rc))
                rc = rtFuzzObsExecCtxArgvPrepare(pThis, pExecCtx, NULL);
        }

        if (RT_SUCCESS(rc))
        {
            RTPROCSTATUS ProcSts;
            rc = rtFuzzObsExecCtxClientRun(pThis, pExecCtx, &ProcSts);
            if (RT_SUCCESS(rc))
            {
                if (ProcSts.enmReason != RTPROCEXITREASON_NORMAL)
                    rc = rtFuzzObsAddInputToResults(pThis, pObsThrd->hFuzzInput, pExecCtx);
            }
            else if (rc == VERR_TIMEOUT)
                rc = rtFuzzObsAddInputToResults(pThis, pObsThrd->hFuzzInput, pExecCtx);
            else
                AssertFailed();

            if (pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE)
                RTFileDelete(&szInput[0]);
        }

        ASMAtomicBitSet(&pThis->bmEvt, pObsThrd->idObs);
        RTSemEventSignal(pThis->hEvtGlobal);
    }

    rtFuzzObsExecCtxDestroy(pThis, pExecCtx);
    return VINF_SUCCESS;
}


/**
 * Fuzzing observer master worker loop.
 *
 * @returns IPRT status code.
 * @param   hThread               The thread handle.
 * @param   pvUser              Opaque user data.
 */
static DECLCALLBACK(int) rtFuzzObsMasterLoop(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);
    int rc = VINF_SUCCESS;
    PRTFUZZOBSINT pThis = (PRTFUZZOBSINT)pvUser;

    RTThreadUserSignal(hThread);

    while (   !pThis->fShutdown
           && RT_SUCCESS(rc))
    {
        uint64_t bmEvt = ASMAtomicXchgU64(&pThis->bmEvt, 0);
        uint32_t idxObs = 0;
        while (bmEvt != 0)
        {
            if (bmEvt & 0x1)
            {
                /* Create a new input for this observer and kick it. */
                PRTFUZZOBSTHRD pObsThrd = &pThis->paObsThreads[idxObs];

                /* Release the old input. */
                if (pObsThrd->hFuzzInput)
                {
                    if (pObsThrd->fKeepInput)
                    {
                        int rc2 = RTFuzzInputAddToCtxCorpus(pObsThrd->hFuzzInput);
                        Assert(RT_SUCCESS(rc2) || rc2 == VERR_ALREADY_EXISTS); RT_NOREF(rc2);
                        pObsThrd->fKeepInput= false;
                    }
                    RTFuzzInputRelease(pObsThrd->hFuzzInput);
                }

                rc = RTFuzzCtxInputGenerate(pThis->hFuzzCtx, &pObsThrd->hFuzzInput);
                if (RT_SUCCESS(rc))
                {
                    ASMAtomicWriteBool(&pObsThrd->fNewInput, true);
                    RTThreadUserSignal(pObsThrd->hThread);
                }
            }

            idxObs++;
            bmEvt >>= 1;
        }

        rc = RTSemEventWait(pThis->hEvtGlobal, RT_INDEFINITE_WAIT);
    }

    return VINF_SUCCESS;
}


/**
 * Initializes the given worker thread structure.
 *
 * @returns IPRT status code.
 * @param   pThis               The internal fuzzing observer state.
 * @param   iObs                Observer ID.
 * @param   pObsThrd            The observer thread structure.
 */
static int rtFuzzObsWorkerThreadInit(PRTFUZZOBSINT pThis, uint32_t idObs, PRTFUZZOBSTHRD pObsThrd)
{
    pObsThrd->pFuzzObs   = pThis;
    pObsThrd->hFuzzInput = NULL;
    pObsThrd->idObs      = idObs;
    pObsThrd->fShutdown  = false;

    ASMAtomicBitSet(&pThis->bmEvt, idObs);
    return RTThreadCreate(&pObsThrd->hThread, rtFuzzObsWorkerLoop, pObsThrd, 0, RTTHREADTYPE_IO,
                          RTTHREADFLAGS_WAITABLE, "Fuzz-Worker");
}


/**
 * Creates the given amount of worker threads and puts them into waiting state.
 *
 * @returns IPRT status code.
 * @param   pThis               The internal fuzzing observer state.
 * @param   cThreads            Number of worker threads to create.
 */
static int rtFuzzObsWorkersCreate(PRTFUZZOBSINT pThis, uint32_t cThreads)
{
    int rc = VINF_SUCCESS;
    PRTFUZZOBSTHRD paObsThreads = (PRTFUZZOBSTHRD)RTMemAllocZ(cThreads * sizeof(RTFUZZOBSTHRD));
    if (RT_LIKELY(paObsThreads))
    {
        for (unsigned i = 0; i < cThreads && RT_SUCCESS(rc); i++)
        {
            rc = rtFuzzObsWorkerThreadInit(pThis, i, &paObsThreads[i]);
            if (RT_FAILURE(rc))
            {
                /* Rollback. */

            }
        }

        if (RT_SUCCESS(rc))
        {
            pThis->paObsThreads = paObsThreads;
            pThis->cThreads     = cThreads;
        }
        else
            RTMemFree(paObsThreads);
    }

    return rc;
}


/**
 * Creates the global worker thread managing the input creation and other worker threads.
 *
 * @returns IPRT status code.
 * @param   pThis               The internal fuzzing observer state.
 */
static int rtFuzzObsMasterCreate(PRTFUZZOBSINT pThis)
{
    pThis->fShutdown = false;

    int rc = RTSemEventCreate(&pThis->hEvtGlobal);
    if (RT_SUCCESS(rc))
    {
        rc = RTThreadCreate(&pThis->hThreadGlobal, rtFuzzObsMasterLoop, pThis, 0, RTTHREADTYPE_IO,
                            RTTHREADFLAGS_WAITABLE, "Fuzz-Master");
        if (RT_SUCCESS(rc))
        {
            RTThreadUserWait(pThis->hThreadGlobal, RT_INDEFINITE_WAIT);
        }
        else
        {
            RTSemEventDestroy(pThis->hEvtGlobal);
            pThis->hEvtGlobal = NIL_RTSEMEVENT;
        }
    }

    return rc;
}


RTDECL(int) RTFuzzObsCreate(PRTFUZZOBS phFuzzObs)
{
    AssertPtrReturn(phFuzzObs, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    PRTFUZZOBSINT pThis = (PRTFUZZOBSINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_LIKELY(pThis))
    {
        pThis->pszBinary     = NULL;
        pThis->papszArgs     = NULL;
        pThis->fFlags        = 0;
        pThis->msWaitMax     = 1000;
        pThis->hThreadGlobal = NIL_RTTHREAD;
        pThis->hEvtGlobal    = NIL_RTSEMEVENT;
        pThis->bmEvt         = 0;
        pThis->cThreads      = 0;
        pThis->paObsThreads  = NULL;
        rc = RTFuzzCtxCreate(&pThis->hFuzzCtx);
        if (RT_SUCCESS(rc))
        {
            *phFuzzObs = pThis;
            return VINF_SUCCESS;
        }

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int) RTFuzzObsDestroy(RTFUZZOBS hFuzzObs)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    RTFuzzObsExecStop(hFuzzObs);

    /* Clean up all acquired resources. */
    for (unsigned i = 0; i < pThis->cArgs; i++)
        RTStrFree(pThis->papszArgs[i]);

    RTMemFree(pThis->papszArgs);

    if (pThis->hEvtGlobal != NIL_RTSEMEVENT)
        RTSemEventDestroy(pThis->hEvtGlobal);

    if (pThis->pszResultsDir)
        RTStrFree(pThis->pszResultsDir);
    if (pThis->pszTmpDir)
        RTStrFree(pThis->pszTmpDir);
    if (pThis->pszBinary)
        RTStrFree(pThis->pszBinary);
    RTFuzzCtxRelease(pThis->hFuzzCtx);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(int) RTFuzzObsQueryCtx(RTFUZZOBS hFuzzObs, PRTFUZZCTX phFuzzCtx)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(phFuzzCtx, VERR_INVALID_POINTER);

    RTFuzzCtxRetain(pThis->hFuzzCtx);
    *phFuzzCtx = pThis->hFuzzCtx;
    return VINF_SUCCESS;
}


RTDECL(int) RTFuzzObsSetTmpDirectory(RTFUZZOBS hFuzzObs, const char *pszTmp)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszTmp, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    pThis->pszTmpDir = RTStrDup(pszTmp);
    if (!pThis->pszTmpDir)
        rc = VERR_NO_STR_MEMORY;
    return rc;
}


RTDECL(int) RTFuzzObsSetResultDirectory(RTFUZZOBS hFuzzObs, const char *pszResults)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszResults, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    pThis->pszResultsDir = RTStrDup(pszResults);
    if (!pThis->pszResultsDir)
        rc = VERR_NO_STR_MEMORY;
    return rc;
}


RTDECL(int) RTFuzzObsSetTestBinary(RTFUZZOBS hFuzzObs, const char *pszBinary, uint32_t fFlags)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszBinary, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    pThis->fFlags    = fFlags;
    pThis->pszBinary = RTStrDup(pszBinary);
    if (RT_UNLIKELY(!pThis->pszBinary))
        rc = VERR_NO_STR_MEMORY;
    return rc;
}


RTDECL(int) RTFuzzObsSetTestBinaryArgs(RTFUZZOBS hFuzzObs, const char * const *papszArgs, unsigned cArgs)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    int rc = VINF_SUCCESS;
    char **papszArgsOld = pThis->papszArgs;
    if (papszArgs)
    {
        pThis->papszArgs = (char **)RTMemAllocZ(sizeof(char **) * (cArgs + 1));
        if (RT_LIKELY(pThis->papszArgs))
        {
            char **ppszOwn = pThis->papszArgs;
            const char * const *ppsz = papszArgs;
            while (   *ppsz != NULL
                   && RT_SUCCESS(rc))
            {
                *ppszOwn = RTStrDup(*ppsz);
                if (RT_UNLIKELY(!*ppszOwn))
                {
                    while (ppszOwn > pThis->papszArgs)
                    {
                        ppszOwn--;
                        RTStrFree(*ppszOwn);
                    }
                    break;
                }

                ppszOwn++;
                ppsz++;
            }

            if (RT_FAILURE(rc))
                RTMemFree(pThis->papszArgs);
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_FAILURE(rc))
            pThis->papszArgs = papszArgsOld;
        else
            pThis->cArgs = cArgs;
    }
    else
    {
        pThis->papszArgs = NULL;
        pThis->cArgs = 0;
        if (papszArgsOld)
        {
            char **ppsz = papszArgsOld;
            while (*ppsz != NULL)
            {
                RTStrFree(*ppsz);
                ppsz++;
            }
            RTMemFree(papszArgsOld);
        }
    }

    return rc;
}


RTDECL(int) RTFuzzObsExecStart(RTFUZZOBS hFuzzObs, uint32_t cProcs)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(cProcs <= sizeof(uint64_t) * 8, VERR_INVALID_PARAMETER);
    AssertReturn(   (pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE)
                 || pThis->pszTmpDir != NULL,
                 VERR_INVALID_STATE);

    int rc = VINF_SUCCESS;
    if (!cProcs)
        cProcs = RT_MIN(RTMpGetPresentCoreCount(), sizeof(uint64_t) * 8);

    /* Spin up the worker threads first. */
    rc = rtFuzzObsWorkersCreate(pThis, cProcs);
    if (RT_SUCCESS(rc))
    {
        /* Spin up the global thread. */
        rc = rtFuzzObsMasterCreate(pThis);
    }

    return rc;
}


RTDECL(int) RTFuzzObsExecStop(RTFUZZOBS hFuzzObs)
{
    PRTFUZZOBSINT pThis = hFuzzObs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    /* Wait for the master thread to terminate. */
    if (pThis->hThreadGlobal != NIL_RTTHREAD)
    {
        ASMAtomicXchgBool(&pThis->fShutdown, true);
        RTSemEventSignal(pThis->hEvtGlobal);
        RTThreadWait(pThis->hThreadGlobal, RT_INDEFINITE_WAIT, NULL);
        pThis->hThreadGlobal = NIL_RTTHREAD;
    }

    /* Destroy the workers. */
    if (pThis->paObsThreads)
    {
        for (unsigned i = 0; i < pThis->cThreads; i++)
        {
            PRTFUZZOBSTHRD pThrd = &pThis->paObsThreads[i];
            ASMAtomicXchgBool(&pThrd->fShutdown, true);
            RTThreadUserSignal(pThrd->hThread);
            RTThreadWait(pThrd->hThread, RT_INDEFINITE_WAIT, NULL);
            if (pThrd->hFuzzInput)
                RTFuzzInputRelease(pThrd->hFuzzInput);
        }

        RTMemFree(pThis->paObsThreads);
        pThis->paObsThreads = NULL;
        pThis->cThreads     = 0;
    }

    RTSemEventDestroy(pThis->hEvtGlobal);
    pThis->hEvtGlobal = NIL_RTSEMEVENT;
    return VINF_SUCCESS;
}

