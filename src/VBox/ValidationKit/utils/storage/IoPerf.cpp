/* $Id$ */
/** @file
 * IoPerf - Storage I/O Performance Benchmark.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/ioqueue.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/system.h>
#include <iprt/test.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/zero.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** Size multiplier for the random data buffer to seek around. */
#define IOPERF_RAND_DATA_BUF_FACTOR 3


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** Forward declaration of the master. */
typedef struct IOPERFMASTER *PIOPERFMASTER;

/**
 * I/O perf supported tests.
 */
typedef enum IOPERFTEST
{
    /** Invalid test handle. */
    IOPERFTEST_INVALID = 0,
    /** The test was disabled. */
    IOPERFTEST_DISABLED,
    IOPERFTEST_FIRST_WRITE,
    IOPERFTEST_SEQ_READ,
    IOPERFTEST_SEQ_WRITE,
    IOPERFTEST_RND_READ,
    IOPERFTEST_RND_WRITE,
    IOPERFTEST_REV_READ,
    IOPERFTEST_REV_WRITE,
    IOPERFTEST_SEQ_READWRITE,
    IOPERFTEST_RND_READWRITE,
    /** Special shutdown test which lets the workers exit, must be LAST. */
    IOPERFTEST_SHUTDOWN,
    IOPERFTEST_32BIT_HACK = 0x7fffffff
} IOPERFTEST;


/**
 * I/O perf test set preparation method.
 */
typedef enum IOPERFTESTSETPREP
{
    IOPERFTESTSETPREP_INVALID = 0,
    /** Just create the file and don't set any sizes. */
    IOPERFTESTSETPREP_JUST_CREATE,
    /** Standard RTFileSetSize() call which might create a sparse file. */
    IOPERFTESTSETPREP_SET_SZ,
    /** Uses RTFileSetAllocationSize() to ensure storage is allocated for the file. */
    IOPERFTESTSETPREP_SET_ALLOC_SZ,
    /** 32bit hack. */
    IOPERFTESTSETPREP_32BIT_HACK = 0x7fffffff
} IOPERFTESTSETPREP;


/**
 * I/O perf request.
 */
typedef struct IOPERFREQ
{
    /** Start timestamp. */
    uint64_t                    tsStart;
    /** Request operation code. */
    RTIOQUEUEOP                 enmOp;
    /** Start offset. */
    uint64_t                    offXfer;
    /** Transfer size for the request. */
    size_t                      cbXfer;
    /** The buffer used for the transfer. */
    void                        *pvXfer;
    /** This is the statically assigned destination buffer for read requests for this request. */
    void                        *pvXferRead;
    /** Size of the read buffer. */
    size_t                      cbXferRead;
} IOPERFREQ;
/** Pointer to an I/O perf request. */
typedef IOPERFREQ *PIOPERFREQ;
/** Pointer to a constant I/O perf request. */
typedef const IOPERFREQ *PCIOPERFREQ;


/**
 * I/O perf job data.
 */
typedef struct IOPERFJOB
{
    /** Pointer to the master if multiple jobs are running. */
    PIOPERFMASTER               pMaster;
    /** Job ID. */
    uint32_t                    idJob;
    /** The test this job is executing. */
    volatile IOPERFTEST         enmTest;
    /** The thread executing the job. */
    RTTHREAD                    hThread;
    /** The I/O queue for the job. */
    RTIOQUEUE                   hIoQueue;
    /** The file path used. */
    char                        *pszFilename;
    /** The handle to use for the I/O queue. */
    RTHANDLE                    Hnd;
    /** Multi event semaphore to synchronise with other jobs. */
    RTSEMEVENTMULTI             hSemEvtMultiRendezvous;
    /** The test set size. */
    uint64_t                    cbTestSet;
    /** Size of one I/O block. */
    size_t                      cbIoBlock;
    /** Maximum number of requests to queue. */
    uint32_t                    cReqsMax;
    /** Pointer to the array of request specific data. */
    PIOPERFREQ                  paIoReqs;
    /** Page aligned chunk of memory assigned as read buffers for the individual requests. */
    void                        *pvIoReqReadBuf;
    /** Size of the read memory buffer. */
    size_t                      cbIoReqReadBuf;
    /** Random number generator used. */
    RTRAND                      hRand;
    /** The random data buffer used for writes. */
    uint8_t                     *pbRandWrite;
    /** Size of the random write buffer in 512 byte blocks. */
    uint32_t                    cRandWriteBlocks512B;
    /** Test dependent data. */
    union
    {
        /** Sequential read write. */
        uint64_t                offNextSeq;
    } Tst;
} IOPERFJOB;
/** Pointer to an I/O Perf job. */
typedef IOPERFJOB *PIOPERFJOB;


/**
 * I/O perf master instance coordinating the job execution.
 */
typedef struct IOPERFMASTER
{
    /** Event semaphore. */
    /** Number of jobs. */
    uint32_t                    cJobs;
    /** Job instances, variable in size. */
    IOPERFJOB                   aJobs[1];
} IOPERFMASTER;


enum
{
    kCmdOpt_First = 128,

    kCmdOpt_FirstWrite = kCmdOpt_First,
    kCmdOpt_NoFirstWrite,
    kCmdOpt_SeqRead,
    kCmdOpt_NoSeqRead,
    kCmdOpt_SeqWrite,
    kCmdOpt_NoSeqWrite,
    kCmdOpt_RndRead,
    kCmdOpt_NoRndRead,
    kCmdOpt_RndWrite,
    kCmdOpt_NoRndWrite,
    kCmdOpt_RevRead,
    kCmdOpt_NoRevRead,
    kCmdOpt_RevWrite,
    kCmdOpt_NoRevWrite,
    kCmdOpt_SeqReadWrite,
    kCmdOpt_NoSeqReadWrite,
    kCmdOpt_RndReadWrite,
    kCmdOpt_NoRndReadWrite,

    kCmdOpt_End
};


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    { "--dir",                      'd',                            RTGETOPT_REQ_STRING  },
    { "--relative-dir",             'r',                            RTGETOPT_REQ_NOTHING },

    { "--jobs",                     'j',                            RTGETOPT_REQ_UINT32  },
    { "--io-engine",                'i',                            RTGETOPT_REQ_STRING  },
    { "--test-set-size",            's',                            RTGETOPT_REQ_UINT64  },
    { "--block-size",               'b',                            RTGETOPT_REQ_UINT32  },
    { "--maximum-requests",         'm',                            RTGETOPT_REQ_UINT32  },

    { "--first-write",              kCmdOpt_FirstWrite,             RTGETOPT_REQ_NOTHING },
    { "--no-first-write",           kCmdOpt_NoFirstWrite,           RTGETOPT_REQ_NOTHING },
    { "--seq-read",                 kCmdOpt_SeqRead,                RTGETOPT_REQ_NOTHING },
    { "--no-seq-read",              kCmdOpt_NoSeqRead,              RTGETOPT_REQ_NOTHING },
    { "--seq-write",                kCmdOpt_SeqWrite,               RTGETOPT_REQ_NOTHING },
    { "--no-seq-write",             kCmdOpt_NoSeqWrite,             RTGETOPT_REQ_NOTHING },
    { "--rnd-read",                 kCmdOpt_RndRead,                RTGETOPT_REQ_NOTHING },
    { "--no-rnd-read",              kCmdOpt_NoRndRead,              RTGETOPT_REQ_NOTHING },
    { "--rnd-write",                kCmdOpt_RndWrite,               RTGETOPT_REQ_NOTHING },
    { "--no-rnd-write",             kCmdOpt_NoRndWrite,             RTGETOPT_REQ_NOTHING },
    { "--rev-read",                 kCmdOpt_RevRead,                RTGETOPT_REQ_NOTHING },
    { "--no-rev-read",              kCmdOpt_NoRevRead,              RTGETOPT_REQ_NOTHING },
    { "--rev-write",                kCmdOpt_RevWrite,               RTGETOPT_REQ_NOTHING },
    { "--no-rev-write",             kCmdOpt_NoRevWrite,             RTGETOPT_REQ_NOTHING },
    { "--seq-read-write",           kCmdOpt_SeqReadWrite,           RTGETOPT_REQ_NOTHING },
    { "--no-seq-read-write",        kCmdOpt_NoSeqReadWrite,         RTGETOPT_REQ_NOTHING },
    { "--rnd-read-write",           kCmdOpt_RndReadWrite,           RTGETOPT_REQ_NOTHING },
    { "--no-rnd-read-write",        kCmdOpt_NoRndReadWrite,         RTGETOPT_REQ_NOTHING },

    { "--quiet",                    'q',                            RTGETOPT_REQ_NOTHING },
    { "--verbose",                  'v',                            RTGETOPT_REQ_NOTHING },
    { "--version",                  'V',                            RTGETOPT_REQ_NOTHING },
    { "--help",                     'h',                            RTGETOPT_REQ_NOTHING } /* for Usage() */
};

/** The test handle. */
static RTTEST       g_hTest;
/** Verbosity level. */
static uint32_t     g_uVerbosity   = 0;
/** Selected I/O engine for the tests, NULL means pick best default one. */
static const char   *g_pszIoEngine = NULL;
/** Number of jobs to run concurrently. */
static uint32_t     g_cJobs        = 1;
/** Size of each test set (file) in bytes. */
static uint64_t     g_cbTestSet    = _2G;
/** Block size for each request. */
static size_t       g_cbIoBlock    = _4K;
/** Maximum number of concurrent requests for each job. */
static uint32_t     g_cReqsMax     = 16;


/** @name Configured tests, this must match the IOPERFTEST order.
 * @{ */
static IOPERFTEST   g_aenmTests[] =
{
    IOPERFTEST_DISABLED, /** @< The invalid test value is disabled of course. */
    IOPERFTEST_DISABLED,
    IOPERFTEST_FIRST_WRITE,
    IOPERFTEST_SEQ_READ,
    IOPERFTEST_SEQ_WRITE,
    IOPERFTEST_RND_READ,
    IOPERFTEST_RND_WRITE,
    IOPERFTEST_REV_READ,
    IOPERFTEST_REV_WRITE,
    IOPERFTEST_SEQ_READWRITE,
    IOPERFTEST_RND_READWRITE,
    IOPERFTEST_SHUTDOWN
};
/** The test index being selected next. */
static uint32_t     g_idxTest      = 2;
/** @} */

/** Set if g_szDir and friends are path relative to CWD rather than absolute. */
static bool         g_fRelativeDir = false;
/** The length of g_szDir. */
static size_t       g_cchDir;

/** The test directory (absolute).  This will always have a trailing slash. */
static char         g_szDir[RTPATH_BIG_MAX];


/*********************************************************************************************************************************
*   Tests                                                                                                                        *
*********************************************************************************************************************************/


/**
 * Selects the next test to run.
 *
 * @return Next test to run.
 */
static IOPERFTEST ioPerfJobTestSelectNext()
{
    AssertReturn(g_idxTest < RT_ELEMENTS(g_aenmTests), IOPERFTEST_SHUTDOWN);

    while (   g_idxTest < RT_ELEMENTS(g_aenmTests)
           && g_aenmTests[g_idxTest] == IOPERFTEST_DISABLED)
        g_idxTest++;

    AssertReturn(g_idxTest < RT_ELEMENTS(g_aenmTests), IOPERFTEST_SHUTDOWN);

    return g_aenmTests[g_idxTest];
}


/**
 * Returns the I/O queue operation for the next request.
 *
 * @returns I/O queue operation enum.
 * @param   pJob                The job data for the current worker.
 */
static RTIOQUEUEOP ioPerfJobTestGetIoQOp(PIOPERFJOB pJob)
{
    switch (pJob->enmTest)
    {
        case IOPERFTEST_FIRST_WRITE:
        case IOPERFTEST_SEQ_WRITE:
        case IOPERFTEST_SEQ_READ:
        case IOPERFTEST_REV_WRITE:
        case IOPERFTEST_RND_WRITE:
            return RTIOQUEUEOP_WRITE;

        case IOPERFTEST_RND_READ:
        case IOPERFTEST_REV_READ:
            return RTIOQUEUEOP_READ;
        case IOPERFTEST_SEQ_READWRITE:
        case IOPERFTEST_RND_READWRITE:
            AssertMsgFailed(("Not implemented!\n"));
            break;
        default:
            AssertMsgFailed(("Invalid/unknown test selected: %d\n", pJob->enmTest));
            break;
    }

    return RTIOQUEUEOP_INVALID;
}


/**
 * Returns the offset to use for the next request.
 *
 * @returns Offset to use.
 * @param   pJob                The job data for the current worker.
 */
static uint64_t ioPerfJobTestGetOffsetNext(PIOPERFJOB pJob)
{
    uint64_t offNext = 0;

    switch (pJob->enmTest)
    {
        case IOPERFTEST_FIRST_WRITE:
        case IOPERFTEST_SEQ_WRITE:
        case IOPERFTEST_SEQ_READ:
            offNext = pJob->Tst.offNextSeq;
            pJob->Tst.offNextSeq += pJob->cbIoBlock;
            break;

        case IOPERFTEST_REV_WRITE:
        case IOPERFTEST_REV_READ:
        case IOPERFTEST_RND_WRITE:
        case IOPERFTEST_RND_READ:
        case IOPERFTEST_SEQ_READWRITE:
        case IOPERFTEST_RND_READWRITE:
            AssertMsgFailed(("Not implemented!\n"));
            break;
        default:
            AssertMsgFailed(("Invalid/unknown test selected: %d\n", pJob->enmTest));
            break;
    }

    return offNext;
}


/**
 * Returns a pointer to the write buffer with random data for the given offset which
 * is predictable for data verification.
 *
 * @returns Pointer to I/O block sized data buffer with random data.
 * @param   pJob                The job data for the current worker.
 * @param   off                 The offset to get the buffer for.
 */
static void *ioPerfJobTestGetWriteBufForOffset(PIOPERFJOB pJob, uint64_t off)
{
    /*
     * Dividing the file into 512 byte blocks so buffer pointers are at least
     * 512 byte aligned to work with async I/O on some platforms (Linux and O_DIRECT for example).
     */
    uint64_t uBlock = off / 512;
    uint32_t idxBuf = uBlock % pJob->cRandWriteBlocks512B;
    return pJob->pbRandWrite + idxBuf * 512;
}


/**
 * Initialize the given request for submission.
 *
 * @returns nothing.
 * @param   pJob                The job data for the current worker.
 * @param   pIoReq              The request to initialize.
 */
static void ioPerfJobTestReqInit(PIOPERFJOB pJob, PIOPERFREQ pIoReq)
{
    pIoReq->enmOp   = ioPerfJobTestGetIoQOp(pJob);
    pIoReq->offXfer = ioPerfJobTestGetOffsetNext(pJob);
    pIoReq->cbXfer  = pJob->cbIoBlock;
    if (pIoReq->enmOp == RTIOQUEUEOP_READ)
        pIoReq->pvXfer = pIoReq->pvXferRead;
    else if (pIoReq->enmOp == RTIOQUEUEOP_WRITE)
        pIoReq->pvXfer = ioPerfJobTestGetWriteBufForOffset(pJob, pIoReq->offXfer);
    else /* Flush */
        pIoReq->pvXfer = NULL;

    pIoReq->tsStart = RTTimeNanoTS();
}


/**
 * Initializes the test state for the current test.
 *
 * @returns IPRT status code.
 * @param   pJob                The job data for the current worker.
 */
static int ioPerfJobTestInit(PIOPERFJOB pJob)
{
    switch (pJob->enmTest)
    {
        case IOPERFTEST_FIRST_WRITE:
        case IOPERFTEST_SEQ_WRITE:
        case IOPERFTEST_SEQ_READ:
            pJob->Tst.offNextSeq = 0;
            break;

        case IOPERFTEST_REV_WRITE:
        case IOPERFTEST_REV_READ:
        case IOPERFTEST_RND_WRITE:
        case IOPERFTEST_RND_READ:
        case IOPERFTEST_SEQ_READWRITE:
        case IOPERFTEST_RND_READWRITE:
            AssertMsgFailed(("Not implemented!\n"));
            break;
        default:
            AssertMsgFailed(("Invalid/unknown test selected: %d\n", pJob->enmTest));
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Frees allocated resources specific for the current test.
 *
 * @returns nothing.
 * @param   pJob                The job data for the current worker.
 */
static void ioPerfJobTestFinish(PIOPERFJOB pJob)
{
    switch (pJob->enmTest)
    {
        case IOPERFTEST_FIRST_WRITE:
        case IOPERFTEST_SEQ_WRITE:
        case IOPERFTEST_SEQ_READ:
            break; /* Nothing to do. */

        case IOPERFTEST_REV_WRITE:
        case IOPERFTEST_REV_READ:
        case IOPERFTEST_RND_WRITE:
        case IOPERFTEST_RND_READ:
        case IOPERFTEST_SEQ_READWRITE:
        case IOPERFTEST_RND_READWRITE:
            AssertMsgFailed(("Not implemented!\n"));
            break;
        default:
            AssertMsgFailed(("Invalid/unknown test selected: %d\n", pJob->enmTest));
            break;
    }
}


/**
 * Returns whether the current test is done with submitting new requests (reached test set size).
 *
 * @returns True when the test has submitted all required requests, false if there are still requests required
 */
static bool ioPerfJobTestIsDone(PIOPERFJOB pJob)
{
    switch (pJob->enmTest)
    {
        case IOPERFTEST_FIRST_WRITE:
        case IOPERFTEST_SEQ_WRITE:
        case IOPERFTEST_SEQ_READ:
            return pJob->Tst.offNextSeq == pJob->cbTestSet;

        case IOPERFTEST_REV_WRITE:
        case IOPERFTEST_REV_READ:
        case IOPERFTEST_RND_WRITE:
        case IOPERFTEST_RND_READ:
        case IOPERFTEST_SEQ_READWRITE:
        case IOPERFTEST_RND_READWRITE:
            AssertMsgFailed(("Not implemented!\n"));
            break;
        default:
            AssertMsgFailed(("Invalid/unknown test selected: %d\n", pJob->enmTest));
            break;
    }

    return true;
}


/**
 * The test I/O loop pumping I/O.
 *
 * @returns IPRT status code.
 * @param   pJob                The job data for the current worker.
 */
static int ioPerfJobTestIoLoop(PIOPERFJOB pJob)
{
    int rc = ioPerfJobTestInit(pJob);
    if (RT_SUCCESS(rc))
    {
        /* Allocate the completion event array. */
        uint32_t cReqsQueued = 0;
        PRTIOQUEUECEVT paIoQCEvt = (PRTIOQUEUECEVT)RTMemAllocZ(pJob->cReqsMax * sizeof(RTIOQUEUECEVT));
        if (RT_LIKELY(paIoQCEvt))
        {
            /* Queue requests up to the maximum. */
            while (   (cReqsQueued < pJob->cReqsMax)
                   && !ioPerfJobTestIsDone(pJob)
                   && RT_SUCCESS(rc))
            {
                PIOPERFREQ pReq = &pJob->paIoReqs[cReqsQueued];
                ioPerfJobTestReqInit(pJob, pReq);
                rc = RTIoQueueRequestPrepare(pJob->hIoQueue, &pJob->Hnd, pReq->enmOp,
                                             pReq->offXfer, pReq->pvXfer, pReq->cbXfer, 0 /*fReqFlags*/,
                                             pReq);
                cReqsQueued++;
            }

            /* Commit the prepared requests. */
            if (   RT_SUCCESS(rc)
                && cReqsQueued)
                rc = RTIoQueueCommit(pJob->hIoQueue);

            /* Enter wait loop and process completed requests. */
            while (   RT_SUCCESS(rc)
                   && cReqsQueued)
            {
                uint32_t cCEvtCompleted = 0;

                rc = RTIoQueueEvtWait(pJob->hIoQueue, paIoQCEvt, pJob->cReqsMax, 1 /*cMinWait*/,
                                      &cCEvtCompleted, 0 /*fFlags*/);
                if (RT_SUCCESS(rc))
                {
                    uint32_t cReqsThisQueued = 0;

                    /* Process any completed event and continue to fill the queue as long as there is stuff to do. */
                    for (uint32_t i = 0; i < cCEvtCompleted; i++)
                    {
                        PIOPERFREQ pReq = (PIOPERFREQ)paIoQCEvt[i].pvUser;

                        if (RT_SUCCESS(paIoQCEvt[i].rcReq))
                        {
                            Assert(paIoQCEvt[i].cbXfered == pReq->cbXfer);

                            /** @todo Statistics collection. */
                            if (!ioPerfJobTestIsDone(pJob))
                            {
                                ioPerfJobTestReqInit(pJob, pReq);
                                rc = RTIoQueueRequestPrepare(pJob->hIoQueue, &pJob->Hnd, pReq->enmOp,
                                                             pReq->offXfer, pReq->pvXfer, pReq->cbXfer, 0 /*fReqFlags*/,
                                                             pReq);
                                cReqsThisQueued++;
                            }
                            else
                                cReqsQueued--;
                        }
                    }

                    if (   cReqsThisQueued
                        && RT_SUCCESS(rc))
                        rc = RTIoQueueCommit(pJob->hIoQueue);
                }
            }

            RTMemFree(paIoQCEvt);
        }

        ioPerfJobTestFinish(pJob);
    }

    return rc;
}


/**
 * Synchronizes with the other jobs and waits for the current test to execute.
 *
 * @returns IPRT status.
 * @param   pJob                The job data for the current worker.
 */
static int ioPerfJobSync(PIOPERFJOB pJob)
{
    if (pJob->pMaster)
    {
        /* Enter the rendezvous semaphore. */
        int rc = VINF_SUCCESS;

        return rc;
    }

    /* Single threaded run, collect the results from our current test and select the next test. */
    /** @todo Results and statistics collection. */
    pJob->enmTest = ioPerfJobTestSelectNext();
    return VINF_SUCCESS;
}


/**
 * I/O perf job main work loop.
 *
 * @returns IPRT status code.
 * @param   pJob                The job data for the current worker.
 */
static int ioPerfJobWorkLoop(PIOPERFJOB pJob)
{
    int rc = VINF_SUCCESS;
    bool fShutdown = false;

    do
    {
        /* Synchronize with the other jobs and the master. */
        rc = ioPerfJobSync(pJob);
        if (RT_FAILURE(rc))
            break;

        rc = ioPerfJobTestIoLoop(pJob);
    } while (   RT_SUCCESS(rc)
             && !fShutdown);

    return rc;
}


/**
 * Job thread entry point.
 */
static DECLCALLBACK(int) ioPerfJobThread(RTTHREAD hThrdSelf, void *pvUser)
{
    RT_NOREF(hThrdSelf);

    PIOPERFJOB pJob = (PIOPERFJOB)pvUser;
    return ioPerfJobWorkLoop(pJob);
}


/**
 * Prepares the test set by laying out the files and filling them with data.
 *
 * @returns IPRT status code.
 * @param   pJob                The job to initialize.
 */
static int ioPerfJobTestSetPrep(PIOPERFJOB pJob)
{
    int rc = RTRandAdvCreateParkMiller(&pJob->hRand);
    if (RT_SUCCESS(rc))
    {
        rc = RTRandAdvSeed(pJob->hRand, RTTimeNanoTS());
        if (RT_SUCCESS(rc))
        {
            /*
             * Create a random data buffer for writes, we'll use multiple of the I/O block size to
             * be able to seek in the buffer quite a bit to make the file content as random as possible
             * to avoid mechanisms like compression or deduplication for now which can influence storage
             * benchmarking unpredictably.
             */
            pJob->cRandWriteBlocks512B = ((IOPERF_RAND_DATA_BUF_FACTOR - 1) * pJob->cbIoBlock) / 512;
            pJob->pbRandWrite = (uint8_t *)RTMemPageAllocZ(IOPERF_RAND_DATA_BUF_FACTOR * pJob->cbIoBlock);
            if (RT_LIKELY(pJob->pbRandWrite))
            {
                RTRandAdvBytes(pJob->hRand, pJob->pbRandWrite, IOPERF_RAND_DATA_BUF_FACTOR * pJob->cbIoBlock);

                /* Write the content here if the first write test is disabled. */
                if (g_aenmTests[IOPERFTEST_FIRST_WRITE] == IOPERFTEST_DISABLED)
                {
                    for (uint64_t off = 0; off < pJob->cbTestSet && RT_SUCCESS(rc); off += pJob->cbIoBlock)
                    {
                        void *pvWrite = ioPerfJobTestGetWriteBufForOffset(pJob, off);
                        rc = RTFileWriteAt(pJob->Hnd.u.hFile, off, pvWrite, pJob->cbIoBlock, NULL);
                    }
                }
            }
        }
        RTRandAdvDestroy(pJob->hRand);
    }

    return rc;
}


/**
 * Initializes the given job instance.
 *
 * @returns IPRT status code.
 * @param   pJob                The job to initialize.
 * @param   pMaster             The coordination master if any.
 * @param   idJob               ID of the job.
 * @param   pszIoEngine         I/O queue engine for this job, NULL for best default.
 * @param   pszTestDir          The test directory to create the file in - requires a slash a the end.
 * @param   enmPrepMethod       Test set preparation method to use.
 * @param   cbTestSet           Size of the test set ofr this job.
 * @param   cbIoBlock           I/O block size for the given job.
 * @param   cReqsMax            Maximum number of concurrent requests for this job.
 */
static int ioPerfJobInit(PIOPERFJOB pJob, PIOPERFMASTER pMaster, uint32_t idJob,
                         const char *pszIoEngine, const char *pszTestDir,
                         IOPERFTESTSETPREP enmPrepMethod,
                         uint64_t cbTestSet, size_t cbIoBlock, uint32_t cReqsMax)
{
    pJob->pMaster        = pMaster;
    pJob->idJob          = idJob;
    pJob->enmTest        = IOPERFTEST_INVALID;
    pJob->hThread        = NIL_RTTHREAD;
    pJob->Hnd.enmType    = RTHANDLETYPE_FILE;
    pJob->cbTestSet      = cbTestSet;
    pJob->cbIoBlock      = cbIoBlock;
    pJob->cReqsMax       = cReqsMax;
    pJob->cbIoReqReadBuf = cReqsMax * cbIoBlock;

    int rc = VINF_SUCCESS;
    pJob->paIoReqs = (PIOPERFREQ)RTMemAllocZ(cReqsMax * sizeof(IOPERFREQ));
    if (RT_LIKELY(pJob->paIoReqs))
    {
        pJob->pvIoReqReadBuf = RTMemPageAlloc(pJob->cbIoReqReadBuf);
        if (RT_LIKELY(pJob->pvIoReqReadBuf))
        {
            uint8_t *pbReadBuf = (uint8_t *)pJob->pvIoReqReadBuf;

            for (uint32_t i = 0; i < cReqsMax; i++)
            {
                pJob->paIoReqs[i].pvXferRead = pbReadBuf;
                pJob->paIoReqs[i].cbXferRead = cbIoBlock;
                pbReadBuf += cbIoBlock;
            }

            /* Create the file. */
            pJob->pszFilename = RTStrAPrintf2("%sioperf-%u.file", pszTestDir, idJob);
            if (RT_LIKELY(pJob->pszFilename))
            {
                rc = RTFileOpen(&pJob->Hnd.u.hFile, pJob->pszFilename, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE);
                if (RT_SUCCESS(rc))
                {
                    switch (enmPrepMethod)
                    {
                        case IOPERFTESTSETPREP_JUST_CREATE:
                            break;
                        case IOPERFTESTSETPREP_SET_SZ:
                            rc = RTFileSetSize(pJob->Hnd.u.hFile, pJob->cbTestSet);
                            break;
                        case IOPERFTESTSETPREP_SET_ALLOC_SZ:
                            rc = RTFileSetAllocationSize(pJob->Hnd.u.hFile, pJob->cbTestSet, RTFILE_ALLOC_SIZE_F_DEFAULT);
                            break;
                        default:
                            AssertMsgFailed(("Invalid file preparation method: %d\n", enmPrepMethod));
                    }

                    if (RT_SUCCESS(rc))
                    {
                        rc = ioPerfJobTestSetPrep(pJob);
                        if (RT_SUCCESS(rc))
                        {
                            /* Create I/O queue. */
                            PCRTIOQUEUEPROVVTABLE pIoQProv = NULL;
                            if (!pszIoEngine)
                                pIoQProv = RTIoQueueProviderGetBestForHndType(RTHANDLETYPE_FILE);
                            else
                                pIoQProv = RTIoQueueProviderGetById(pszIoEngine);

                            if (RT_LIKELY(pIoQProv))
                            {
                                rc = RTIoQueueCreate(&pJob->hIoQueue, pIoQProv, 0 /*fFlags*/, cReqsMax, cReqsMax);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = RTIoQueueHandleRegister(pJob->hIoQueue, &pJob->Hnd);
                                    if (RT_SUCCESS(rc))
                                    {
                                        /* Spin up the worker thread. */
                                        if (pMaster)
                                            rc = RTThreadCreateF(&pJob->hThread, ioPerfJobThread, pJob, 0,
                                                                 RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "ioperf-%u", idJob);

                                        if (RT_SUCCESS(rc))
                                            return VINF_SUCCESS;
                                    }
                                }
                            }
                            else
                                rc = VERR_NOT_SUPPORTED;
                        }

                        RTRandAdvDestroy(pJob->hRand);
                    }

                    RTFileClose(pJob->Hnd.u.hFile);
                    RTFileDelete(pJob->pszFilename);
                }

                RTStrFree(pJob->pszFilename);
            }

            RTMemPageFree(pJob->pvIoReqReadBuf, pJob->cbIoReqReadBuf);
        }
        else
            rc = VERR_NO_STR_MEMORY;
    }

    return rc;
}


/**
 * Teardown a job instance and free all associated resources.
 *
 * @returns IPRT status code.
 * @param   pJob                The job to teardown.
 */
static int ioPerfJobTeardown(PIOPERFJOB pJob)
{
    if (pJob->pMaster)
    {
        int rc = RTThreadWait(pJob->hThread, RT_INDEFINITE_WAIT, NULL);
        AssertRC(rc); RT_NOREF(rc);
    }

    RTIoQueueHandleDeregister(pJob->hIoQueue, &pJob->Hnd);
    RTIoQueueDestroy(pJob->hIoQueue);
    RTRandAdvDestroy(pJob->hRand);
    RTMemPageFree(pJob->pbRandWrite, IOPERF_RAND_DATA_BUF_FACTOR * pJob->cbIoBlock);
    RTFileClose(pJob->Hnd.u.hFile);
    RTFileDelete(pJob->pszFilename);
    RTStrFree(pJob->pszFilename);
    RTMemPageFree(pJob->pvIoReqReadBuf, pJob->cbIoReqReadBuf);
    RTMemFree(pJob->paIoReqs);
    return VINF_SUCCESS;
}


/**
 * Single job testing entry point.
 *
 * @returns IPRT status code.
 */
static int ioPerfDoTestSingle(void)
{
    IOPERFJOB Job;

    int rc = ioPerfJobInit(&Job, NULL, 0, g_pszIoEngine,
                           g_szDir, IOPERFTESTSETPREP_SET_SZ,
                           g_cbTestSet, g_cbIoBlock, g_cReqsMax);
    if (RT_SUCCESS(rc))
    {
        rc = ioPerfJobWorkLoop(&Job);
        if (RT_SUCCESS(rc))
        {
            rc = ioPerfJobTeardown(&Job);
            AssertRC(rc); RT_NOREF(rc);
        }
    }

    return rc;
}


/**
 * Multi job testing entry point.
 *
 * @returns IPRT status code.
 */
static int ioPerfDoTestMulti(void)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Display the usage to @a pStrm.
 */
static void Usage(PRTSTREAM pStrm)
{
    char szExec[RTPATH_MAX];
    RTStrmPrintf(pStrm, "usage: %s <-d <testdir>> [options]\n",
                 RTPathFilename(RTProcGetExecutablePath(szExec, sizeof(szExec))));
    RTStrmPrintf(pStrm, "\n");
    RTStrmPrintf(pStrm, "options: \n");

    for (unsigned i = 0; i < RT_ELEMENTS(g_aCmdOptions); i++)
    {
        char szHelp[80];
        const char *pszHelp;
        switch (g_aCmdOptions[i].iShort)
        {
            case 'd':                           pszHelp = "The directory to use for testing.            default: CWD/fstestdir"; break;
            case 'r':                           pszHelp = "Don't abspath test dir (good for deep dirs). default: disabled"; break;
            case 'v':                           pszHelp = "More verbose execution."; break;
            case 'q':                           pszHelp = "Quiet execution."; break;
            case 'h':                           pszHelp = "Displays this help and exit"; break;
            case 'V':                           pszHelp = "Displays the program revision"; break;
            default:
                if (g_aCmdOptions[i].iShort >= kCmdOpt_First)
                {
                    if (RTStrStartsWith(g_aCmdOptions[i].pszLong, "--no-"))
                        RTStrPrintf(szHelp, sizeof(szHelp), "Disables the '%s' test.", g_aCmdOptions[i].pszLong + 5);
                    else
                        RTStrPrintf(szHelp, sizeof(szHelp), "Enables  the '%s' test.", g_aCmdOptions[i].pszLong + 2);
                    pszHelp = szHelp;
                }
                else
                    pszHelp = "Option undocumented";
                break;
        }
        if ((unsigned)g_aCmdOptions[i].iShort < 127U)
        {
            char szOpt[64];
            RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdOptions[i].pszLong, g_aCmdOptions[i].iShort);
            RTStrmPrintf(pStrm, "  %-19s %s\n", szOpt, pszHelp);
        }
        else
            RTStrmPrintf(pStrm, "  %-19s %s\n", g_aCmdOptions[i].pszLong, pszHelp);
    }
}


int main(int argc, char *argv[])
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("IoPerf", &g_hTest);
    if (rc)
        return rc;

    /*
     * Default values.
     */
    char szDefaultDir[32];
    const char *pszDir = szDefaultDir;
    RTStrPrintf(szDefaultDir, sizeof(szDefaultDir), "ioperfdir-%u" RTPATH_SLASH_STR, RTProcSelf());

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdOptions, RT_ELEMENTS(g_aCmdOptions), 1, 0 /* fFlags */);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'd':
                pszDir = ValueUnion.psz;
                break;

            case 'r':
                g_fRelativeDir = true;
                break;

            case 'i':
                g_pszIoEngine = ValueUnion.psz;
                break;

            case 's':
                g_cbTestSet = ValueUnion.u64;
                break;

            case 'b':
                g_cbIoBlock = ValueUnion.u32;
                break;

            case 'm':
                g_cReqsMax = ValueUnion.u32;
                break;

            case 'q':
                g_uVerbosity = 0;
                break;

            case 'v':
                g_uVerbosity++;
                break;

            case 'h':
                Usage(g_pStdOut);
                return RTEXITCODE_SUCCESS;

            case 'V':
            {
                char szRev[] = "$Revision$";
                szRev[RT_ELEMENTS(szRev) - 2] = '\0';
                RTPrintf(RTStrStrip(strchr(szRev, ':') + 1));
                return RTEXITCODE_SUCCESS;
            }

            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Populate g_szDir.
     */
    if (!g_fRelativeDir)
        rc = RTPathAbs(pszDir, g_szDir, sizeof(g_szDir));
    else
        rc = RTStrCopy(g_szDir, sizeof(g_szDir), pszDir);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(g_hTest, "%s(%s) failed: %Rrc\n", g_fRelativeDir ? "RTStrCopy" : "RTAbsPath", pszDir, rc);
        return RTTestSummaryAndDestroy(g_hTest);
    }
    RTPathEnsureTrailingSeparator(g_szDir, sizeof(g_szDir));
    g_cchDir = strlen(g_szDir);

    /*
     * Create the test directory with an 'empty' subdirectory under it,
     * execute the tests, and remove directory when done.
     */
    RTTestBanner(g_hTest);
    if (!RTPathExists(g_szDir))
    {
        /* The base dir: */
        rc = RTDirCreate(g_szDir, 0755,
                         RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_DONT_SET | RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_NOT_CRITICAL);
        if (RT_SUCCESS(rc))
        {
            RTTestIPrintf(RTTESTLVL_ALWAYS, "Test  dir: %s\n", g_szDir);

            if (g_cJobs == 1)
                rc = ioPerfDoTestSingle();
            else
                rc = ioPerfDoTestMulti();

            g_szDir[g_cchDir] = '\0';
            rc = RTDirRemoveRecursive(g_szDir, RTDIRRMREC_F_CONTENT_AND_DIR | (g_fRelativeDir ? RTDIRRMREC_F_NO_ABS_PATH : 0));
            if (RT_FAILURE(rc))
                RTTestFailed(g_hTest, "RTDirRemoveRecursive(%s,) -> %Rrc\n", g_szDir, rc);
        }
        else
            RTTestFailed(g_hTest, "RTDirCreate(%s) -> %Rrc\n", g_szDir, rc);
    }
    else
        RTTestFailed(g_hTest, "Test directory already exists: %s\n", g_szDir);

    return RTTestSummaryAndDestroy(g_hTest);
}

