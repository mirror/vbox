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
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/md5.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>


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
    /** The binary to run. */
    char                       *pszBinary;
    /** Arguments to run the binary with, terminated by a NULL entry. */
    char                      **papszArgs;
    /** Number of arguments. */
    uint32_t                    cArgs;
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
 * Fuzzing observer worker loop.
 *
 * @returns IPRT status code.
 * @param   hThread               The thread handle.
 * @param   pvUser              Opaque user data.
 */
static DECLCALLBACK(int) rtFuzzObsWorkerLoop(RTTHREAD hThread, void *pvUser)
{
    PRTFUZZOBSTHRD pObsThrd = (PRTFUZZOBSTHRD)pvUser;
    PRTFUZZOBSINT pThis = pObsThrd->pFuzzObs;
    char **papszArgs = NULL;
    RTHANDLE NilHandle;
    NilHandle.enmType = RTHANDLETYPE_PIPE;
    NilHandle.u.hPipe = NIL_RTPIPE;

    if (!(pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE))
    {
        papszArgs = (char **)RTMemAllocZ((pThis->cArgs + 1) * sizeof(char *));
        if (RT_LIKELY(papszArgs))
        {
            for (uint32_t i = 0; i < pThis->cArgs; i++)
                papszArgs[i] = pThis->papszArgs[i];
        }
    }
    else
        papszArgs = pThis->papszArgs;

    while (!pObsThrd->fShutdown)
    {
        /* Wait for work. */
        int rc = RTThreadUserWait(hThread, RT_INDEFINITE_WAIT);
        AssertRC(rc);

        if (pObsThrd->fShutdown)
            break;

        bool f = ASMAtomicXchgBool(&pObsThrd->fNewInput, false);
        if (!f)
            continue;

        AssertPtr(pObsThrd->hFuzzInput);

        /* Create the stdout/stderr pipe. */
        RTPIPE hPipeStdOutErrR;
        RTPIPE hPipeStdOutErrW;
        rc = RTPipeCreate(&hPipeStdOutErrR, &hPipeStdOutErrW, RTPIPE_C_INHERIT_WRITE);
        if (RT_SUCCESS(rc))
        {
            RTHANDLE HandleStdOutErr;
            HandleStdOutErr.enmType = RTHANDLETYPE_PIPE;
            HandleStdOutErr.u.hPipe = hPipeStdOutErrW;

            if (pThis->fFlags & RTFUZZ_OBS_BINARY_F_INPUT_FILE)
            {
                char szInput[RTPATH_MAX];
                char szDigest[RTMD5_STRING_LEN + 1];
                char szFilename[sizeof(szDigest) + 32];

                rc = RTFuzzInputQueryDigestString(pObsThrd->hFuzzInput, &szDigest[0], sizeof(szDigest));
                AssertRC(rc);

                ssize_t cbBuf = RTStrPrintf2(&szFilename[0], sizeof(szFilename), "%u-%s", pObsThrd->idObs, &szDigest[0]);
                Assert(cbBuf > 0); RT_NOREF(cbBuf);

                RT_ZERO(szInput);
                rc = RTPathJoin(szInput, sizeof(szInput), pThis->pszTmpDir, &szFilename[0]);
                AssertRC(rc);

                rc = RTFuzzInputWriteToFile(pObsThrd->hFuzzInput, &szInput[0]);
                if (RT_SUCCESS(rc))
                {
                    papszArgs[pThis->cArgs - 1] = &szInput[0];

                    RTPROCESS hProc = NIL_RTPROCESS;
                    rc = RTProcCreateEx(pThis->pszBinary, papszArgs, RTENV_DEFAULT, 0 /*fFlags*/, &NilHandle,
                                        NULL, NULL, NULL, NULL, &hProc);
                    if (RT_SUCCESS(rc))
                    {
                        RTPipeClose(hPipeStdOutErrW);
                        RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
                        rc = RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus);
                        if (RT_SUCCESS(rc))
                        {
                            if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL)
                            {
                                pObsThrd->fKeepInput = true;
                                RTPrintf("Abnormal exit detected!\n");
                            }
                            else
                            {
                                /*
                                 * Fuzzed inputs are only added if the client signalled a success in parsing the input.
                                 * Inputs which lead to errors are not added because it is assumed that the target
                                 * successfully verified the input data.
                                 */
                                //if (ProcStatus.iStatus == RTEXITCODE_SUCCESS)
                                //    pObsThrd->fKeepInput = true;
                            }
                        }
                        else
                            AssertFailed();
                    }
                    else
                        AssertFailed();

                    rc = RTFileDelete(&szInput[0]);
                    AssertRC(rc);
                }
                else
                    AssertFailed();
            }
            else
            {
                RTPIPE hPipeR;
                RTPIPE hPipeW;
                rc = RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_READ);
                if (RT_SUCCESS(rc))
                {
                    RTPROCESS hProc = NIL_RTPROCESS;
                    RTHANDLE Handle;
                    Handle.enmType = RTHANDLETYPE_PIPE;
                    Handle.u.hPipe = hPipeR;
                    rc = RTProcCreateEx(pThis->pszBinary, papszArgs, RTENV_DEFAULT, 0 /*fFlags*/, &Handle,
                                        &HandleStdOutErr, &HandleStdOutErr, NULL, NULL, &hProc);
                    if (RT_SUCCESS(rc))
                    {
                        /* Pump the input data. */
                        RTPipeClose(hPipeStdOutErrW);

                        uint8_t *pbInput;
                        size_t  cbInput;
                        rc = RTFuzzInputQueryData(pObsThrd->hFuzzInput, (void **)&pbInput, &cbInput);
                        if (RT_SUCCESS(rc))
                        {
                            rc = RTPipeWriteBlocking(hPipeW, pbInput, cbInput, NULL);
                            RTPipeClose(hPipeW);

                            RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
                            rc = RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus);
                            if (RT_SUCCESS(rc))
                            {
                                if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL)
                                {
                                    pObsThrd->fKeepInput = true;
                                    RTPrintf("Abnormal exit detected!\n");
                                }
                                else
                                {
                                    /*
                                     * Fuzzed inputs are only added if the client signalled a success in parsing the input.
                                     * Inputs which lead to errors are not added because it is assumed that the target
                                     * successfully verified the input data.
                                     */
                                    if (ProcStatus.iStatus == RTEXITCODE_SUCCESS)
                                        pObsThrd->fKeepInput = true;
                                }
                            }
                        }
                    }

                    RTPipeClose(hPipeW);
                }
            }

            RTPipeClose(hPipeStdOutErrR);
        }
        else
            AssertFailed();

        ASMAtomicBitSet(&pThis->bmEvt, pObsThrd->idObs);
        RTSemEventSignal(pThis->hEvtGlobal);
    }

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
            pThis->paObsThreads = paObsThreads;
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
        pThis->pszBinary   = NULL;
        pThis->papszArgs   = NULL;
        pThis->fFlags      = 0;
        pThis->hThreadGlobal = NIL_RTTHREAD;
        pThis->hEvtGlobal  = NIL_RTSEMEVENT;
        pThis->bmEvt       = 0;
        pThis->cThreads    = 0;
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

    /* Wait for the master thread to terminate. */
    if (pThis->hThreadGlobal != NIL_RTTHREAD)
    {
        ASMAtomicXchgBool(&pThis->fShutdown, true);
        RTThreadWait(pThis->hThreadGlobal, RT_INDEFINITE_WAIT, NULL);
    }

    /* Clean up the workers. */
    if (pThis->paObsThreads)
    {
        for (unsigned i = 0; i < pThis->cThreads; i++)
        {
            PRTFUZZOBSTHRD pThrd = &pThis->paObsThreads[i];
            ASMAtomicXchgBool(&pThrd->fShutdown, true);
            RTThreadWait(pThrd->hThread, RT_INDEFINITE_WAIT, NULL);
            if (pThrd->hFuzzInput)
                RTFuzzInputRelease(pThrd->hFuzzInput);
        }
        RTMemFree(pThis->paObsThreads);
        pThis->paObsThreads = NULL;
    }

    /* Clean up all acquired resources. */
    for (unsigned i = 0; i < pThis->cArgs; i++)
        RTStrFree(pThis->papszArgs[i]);

    RTMemFree(pThis->papszArgs);

    if (pThis->hEvtGlobal != NIL_RTSEMEVENT)
        RTSemEventDestroy(pThis->hEvtGlobal);

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
    RT_NOREF(hFuzzObs);
    return VERR_NOT_IMPLEMENTED;
}

