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
    /** Test dependent data. */
    union
    {
        uint32_t                uDummy;
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
    { "--enable-all",               'e',                            RTGETOPT_REQ_NOTHING },
    { "--disable-all",              'z',                            RTGETOPT_REQ_NOTHING },

    { "--jobs",                     'j',                            RTGETOPT_REQ_UINT32  },
    { "--io-engine",                'i',                            RTGETOPT_REQ_STRING  },
    { "--test-set-size",            's',                            RTGETOPT_REQ_UINT64  },
    { "--block-size",               'b',                            RTGETOPT_REQ_UINT32  },

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
static IOPERFTEST ioPerfTestSelectNext()
{
    AssertReturn(g_idxTest < RT_ELEMENTS(g_aenmTests), IOPERFTEST_SHUTDOWN);

    while (   g_idxTest < RT_ELEMENTS(g_aenmTests)
           && g_aenmTests[g_idxTest] != IOPERFTEST_DISABLED)
        g_idxTest++;

    AssertReturn(g_idxTest < RT_ELEMENTS(g_aenmTests), IOPERFTEST_SHUTDOWN);

    return g_aenmTests[g_idxTest];
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
        /* Enter the rendevouzs semaphore. */
        int rc = VINF_SUCCESS;

        return rc;
    }

    /* Single threaded run, collect the results from our current test and select the next test. */
    /** @todo Results and statistics collection. */
    pJob->enmTest = ioPerfTestSelectNext();
    return VINF_SUCCESS;
}


/**
 * Sequential read test.
 *
 * @returns IPRT status code.
 * @param   pJob                The job data for the current worker.
 */
static int ioPerfTestSeqRead(PIOPERFJOB pJob)
{
    RT_NOREF(pJob);
    return VERR_NOT_IMPLEMENTED;
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

        switch (pJob->enmTest)
        {
            case IOPERFTEST_FIRST_WRITE:
                break;
            case IOPERFTEST_SEQ_READ:
                rc = ioPerfTestSeqRead(pJob);
                break;
            case IOPERFTEST_SEQ_WRITE:
                break;
            case IOPERFTEST_RND_READ:
                break;
            case IOPERFTEST_RND_WRITE:
                break;
            case IOPERFTEST_REV_READ:
                break;
            case IOPERFTEST_REV_WRITE:
                break;
            case IOPERFTEST_SEQ_READWRITE:
                break;
            case IOPERFTEST_RND_READWRITE:
                break;
            case IOPERFTEST_SHUTDOWN:
                fShutdown = true;
                break;
            default:
                AssertMsgFailed(("Invalid job: %d\n", pJob->enmTest));
        }
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
    pJob->pMaster     = pMaster;
    pJob->idJob       = idJob;
    pJob->enmTest     = IOPERFTEST_INVALID;
    pJob->hThread     = NIL_RTTHREAD;
    pJob->Hnd.enmType = RTHANDLETYPE_FILE;
    pJob->cbTestSet   = cbTestSet;
    pJob->cbIoBlock   = cbIoBlock;
    pJob->cReqsMax    = cReqsMax;

    /* Create the file. */
    int rc = VINF_SUCCESS;
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
                /* Create I/O queue. */
                PCRTIOQUEUEPROVVTABLE pIoQProv = NULL;
                if (pszIoEngine)
                    pIoQProv = RTIoQueueProviderGetBestForHndType(RTHANDLETYPE_FILE);
                else
                    pIoQProv = RTIoQueueProviderGetById(pszIoEngine);

                if (RT_LIKELY(pIoQProv))
                {
                    rc = RTIoQueueCreate(&pJob->hIoQueue, pIoQProv, 0 /*fFlags*/, cReqsMax, cReqsMax);
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
                else
                    rc = VERR_NOT_SUPPORTED;
            }

            RTFileClose(pJob->Hnd.u.hFile);
            RTFileDelete(pJob->pszFilename);
        }

        RTStrFree(pJob->pszFilename);
    }
    else
        rc = VERR_NO_STR_MEMORY;

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
    int rc = RTThreadWait(pJob->hThread, RT_INDEFINITE_WAIT, NULL); AssertRC(rc); RT_NOREF(rc);

    RTIoQueueDestroy(pJob->hIoQueue);
    RTFileClose(pJob->Hnd.u.hFile);
    RTFileDelete(pJob->pszFilename);
    RTStrFree(pJob->pszFilename);
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
                           g_szDir, IOPERFTESTSETPREP_SET_ALLOC_SZ,
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
            case 'e':                           pszHelp = "Enables all tests.                           default: -e"; break;
            case 'z':                           pszHelp = "Disables all tests.                          default: -e"; break;
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

