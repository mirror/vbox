/* $Id$ */
/** @file
 * FsPerf - File System (Shared Folders) Performance Benchmark.
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
#ifdef RT_OS_OS2
#  define INCL_BASE
#  include <os2.h>
#  undef RT_MAX
#endif
#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#ifdef RT_OS_LINUX
# include <iprt/pipe.h>
#endif
#include <iprt/process.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/system.h>
#include <iprt/tcp.h>
#include <iprt/test.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/zero.h>

#ifdef RT_OS_WINDOWS
# include <iprt/nt/nt-and-windows.h>
#else
# include <errno.h>
# include <unistd.h>
# include <limits.h>
# include <sys/types.h>
# include <sys/fcntl.h>
# ifndef RT_OS_OS2
#  include <sys/mman.h>
#  include <sys/uio.h>
# endif
# include <sys/socket.h>
# include <signal.h>
# ifdef RT_OS_LINUX
#  include <sys/sendfile.h>
#  include <sys/syscall.h>
# endif
# ifdef RT_OS_DARWIN
#  include <sys/uio.h>
# endif
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def FSPERF_TEST_SENDFILE
 * Whether to enable the sendfile() tests. */
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN)
# define FSPERF_TEST_SENDFILE
#endif

/**
 * Macro for profiling @a a_fnCall (typically forced inline) for about @a a_cNsTarget ns.
 *
 * Always does an even number of iterations.
 */
#define PROFILE_FN(a_fnCall, a_cNsTarget, a_szDesc) \
    do { \
        /* Estimate how many iterations we need to fill up the given timeslot: */ \
        fsPerfYield(); \
        uint64_t nsStart = RTTimeNanoTS(); \
        uint64_t nsPrf; \
        do \
            nsPrf = RTTimeNanoTS(); \
        while (nsPrf == nsStart); \
        nsStart = nsPrf; \
        \
        uint64_t iIteration = 0; \
        do \
        { \
            RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
            iIteration++; \
            nsPrf = RTTimeNanoTS() - nsStart; \
        } while (nsPrf < RT_NS_10MS || (iIteration & 1)); \
        nsPrf /= iIteration; \
        if (nsPrf > g_nsPerNanoTSCall + 32) \
            nsPrf -= g_nsPerNanoTSCall; \
        \
        uint64_t cIterations = (a_cNsTarget) / nsPrf; \
        if (cIterations <= 1) \
            cIterations = 2; \
        else if (cIterations & 1) \
            cIterations++; \
        \
        /* Do the actual profiling: */ \
        fsPerfYield(); \
        iIteration = 0; \
        nsStart = RTTimeNanoTS(); \
        for (; iIteration < cIterations; iIteration++) \
            RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
        nsPrf = RTTimeNanoTS() - nsStart; \
        RTTestIValue(a_szDesc, nsPrf / cIterations, RTTESTUNIT_NS_PER_OCCURRENCE); \
        if (g_fShowDuration) \
            RTTestIValueF(nsPrf, RTTESTUNIT_NS, "%s duration", a_szDesc); \
        if (g_fShowIterations) \
            RTTestIValueF(iIteration, RTTESTUNIT_OCCURRENCES, "%s iterations", a_szDesc); \
    } while (0)


/**
 * Macro for profiling an operation on each file in the manytree directory tree.
 *
 * Always does an even number of tree iterations.
 */
#define PROFILE_MANYTREE_FN(a_szPath, a_fnCall, a_cEstimationIterations, a_cNsTarget, a_szDesc) \
    do { \
        if (!g_fManyFiles) \
            break; \
        \
        /* Estimate how many iterations we need to fill up the given timeslot: */ \
        fsPerfYield(); \
        uint64_t nsStart = RTTimeNanoTS(); \
        uint64_t ns; \
        do \
            ns = RTTimeNanoTS(); \
        while (ns == nsStart); \
        nsStart = ns; \
        \
        PFSPERFNAMEENTRY pCur; \
        uint64_t iIteration = 0; \
        do \
        { \
            RTListForEach(&g_ManyTreeHead, pCur, FSPERFNAMEENTRY, Entry) \
            { \
                memcpy(a_szPath, pCur->szName, pCur->cchName); \
                for (uint32_t i = 0; i < g_cManyTreeFilesPerDir; i++) \
                { \
                    RTStrFormatU32(&a_szPath[pCur->cchName], sizeof(a_szPath) - pCur->cchName, i, 10, 5, 5, RTSTR_F_ZEROPAD); \
                    RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
                } \
            } \
            iIteration++; \
            ns = RTTimeNanoTS() - nsStart; \
        } while (ns < RT_NS_10MS || (iIteration & 1)); \
        ns /= iIteration; \
        if (ns > g_nsPerNanoTSCall + 32) \
            ns -= g_nsPerNanoTSCall; \
        \
        uint32_t cIterations = (a_cNsTarget) / ns; \
        if (cIterations <= 1) \
            cIterations = 2; \
        else if (cIterations & 1) \
            cIterations++; \
        \
        /* Do the actual profiling: */ \
        fsPerfYield(); \
        uint32_t cCalls = 0; \
        nsStart = RTTimeNanoTS(); \
        for (iIteration = 0; iIteration < cIterations; iIteration++) \
        { \
            RTListForEach(&g_ManyTreeHead, pCur, FSPERFNAMEENTRY, Entry) \
            { \
                memcpy(a_szPath, pCur->szName, pCur->cchName); \
                for (uint32_t i = 0; i < g_cManyTreeFilesPerDir; i++) \
                { \
                    RTStrFormatU32(&a_szPath[pCur->cchName], sizeof(a_szPath) - pCur->cchName, i, 10, 5, 5, RTSTR_F_ZEROPAD); \
                    RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
                    cCalls++; \
                } \
            } \
        } \
        ns = RTTimeNanoTS() - nsStart; \
        RTTestIValueF(ns / cCalls, RTTESTUNIT_NS_PER_OCCURRENCE, a_szDesc); \
        if (g_fShowDuration) \
            RTTestIValueF(ns, RTTESTUNIT_NS, "%s duration", a_szDesc); \
        if (g_fShowIterations) \
            RTTestIValueF(iIteration, RTTESTUNIT_OCCURRENCES, "%s iterations", a_szDesc); \
    } while (0)


/**
 * Execute a_fnCall for each file in the manytree.
 */
#define DO_MANYTREE_FN(a_szPath, a_fnCall) \
    do { \
        PFSPERFNAMEENTRY pCur; \
        RTListForEach(&g_ManyTreeHead, pCur, FSPERFNAMEENTRY, Entry) \
        { \
            memcpy(a_szPath, pCur->szName, pCur->cchName); \
            for (uint32_t i = 0; i < g_cManyTreeFilesPerDir; i++) \
            { \
                RTStrFormatU32(&a_szPath[pCur->cchName], sizeof(a_szPath) - pCur->cchName, i, 10, 5, 5, RTSTR_F_ZEROPAD); \
                a_fnCall; \
            } \
        } \
    } while (0)


/** @def FSPERF_VERR_PATH_NOT_FOUND
 * Hides the fact that we only get VERR_PATH_NOT_FOUND on non-unix systems.  */
#if defined(RT_OS_WINDOWS) //|| defined(RT_OS_OS2) - using posix APIs IIRC, so lost in translation.
# define FSPERF_VERR_PATH_NOT_FOUND     VERR_PATH_NOT_FOUND
#else
# define FSPERF_VERR_PATH_NOT_FOUND     VERR_FILE_NOT_FOUND
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct FSPERFNAMEENTRY
{
    RTLISTNODE  Entry;
    uint16_t    cchName;
    char        szName[RT_FLEXIBLE_ARRAY];
} FSPERFNAMEENTRY;
typedef FSPERFNAMEENTRY *PFSPERFNAMEENTRY;


enum
{
    kCmdOpt_First = 128,

    kCmdOpt_ManyFiles = kCmdOpt_First,
    kCmdOpt_NoManyFiles,
    kCmdOpt_Open,
    kCmdOpt_NoOpen,
    kCmdOpt_FStat,
    kCmdOpt_NoFStat,
    kCmdOpt_FChMod,
    kCmdOpt_NoFChMod,
    kCmdOpt_FUtimes,
    kCmdOpt_NoFUtimes,
    kCmdOpt_Stat,
    kCmdOpt_NoStat,
    kCmdOpt_ChMod,
    kCmdOpt_NoChMod,
    kCmdOpt_Utimes,
    kCmdOpt_NoUtimes,
    kCmdOpt_Rename,
    kCmdOpt_NoRename,
    kCmdOpt_DirOpen,
    kCmdOpt_NoDirOpen,
    kCmdOpt_DirEnum,
    kCmdOpt_NoDirEnum,
    kCmdOpt_MkRmDir,
    kCmdOpt_NoMkRmDir,
    kCmdOpt_StatVfs,
    kCmdOpt_NoStatVfs,
    kCmdOpt_Rm,
    kCmdOpt_NoRm,
    kCmdOpt_ChSize,
    kCmdOpt_NoChSize,
    kCmdOpt_ReadPerf,
    kCmdOpt_NoReadPerf,
    kCmdOpt_ReadTests,
    kCmdOpt_NoReadTests,
    kCmdOpt_SendFile,
    kCmdOpt_NoSendFile,
#ifdef RT_OS_LINUX
    kCmdOpt_Splice,
    kCmdOpt_NoSplice,
#endif
    kCmdOpt_WritePerf,
    kCmdOpt_NoWritePerf,
    kCmdOpt_WriteTests,
    kCmdOpt_NoWriteTests,
    kCmdOpt_Seek,
    kCmdOpt_NoSeek,
    kCmdOpt_FSync,
    kCmdOpt_NoFSync,
    kCmdOpt_MMap,
    kCmdOpt_NoMMap,
    kCmdOpt_IgnoreNoCache,
    kCmdOpt_NoIgnoreNoCache,
    kCmdOpt_IoFileSize,
    kCmdOpt_SetBlockSize,
    kCmdOpt_AddBlockSize,
    kCmdOpt_Copy,
    kCmdOpt_NoCopy,

    kCmdOpt_ShowDuration,
    kCmdOpt_NoShowDuration,
    kCmdOpt_ShowIterations,
    kCmdOpt_NoShowIterations,

    kCmdOpt_ManyTreeFilesPerDir,
    kCmdOpt_ManyTreeSubdirsPerDir,
    kCmdOpt_ManyTreeDepth,

    kCmdOpt_End
};


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    { "--dir",              'd', RTGETOPT_REQ_STRING  },
    { "--seconds",          's', RTGETOPT_REQ_UINT32  },
    { "--milliseconds",     'm', RTGETOPT_REQ_UINT64  },

    { "--enable-all",       'e', RTGETOPT_REQ_NOTHING },
    { "--disable-all",      'z', RTGETOPT_REQ_NOTHING },

    { "--many-files",       kCmdOpt_ManyFiles,              RTGETOPT_REQ_UINT32 },
    { "--no-many-files",    kCmdOpt_NoManyFiles,            RTGETOPT_REQ_NOTHING },
    { "--files-per-dir",    kCmdOpt_ManyTreeFilesPerDir,    RTGETOPT_REQ_UINT32 },
    { "--subdirs-per-dir",  kCmdOpt_ManyTreeSubdirsPerDir,  RTGETOPT_REQ_UINT32 },
    { "--tree-depth",       kCmdOpt_ManyTreeDepth,          RTGETOPT_REQ_UINT32 },

    { "--open",             kCmdOpt_Open,           RTGETOPT_REQ_NOTHING },
    { "--no-open",          kCmdOpt_NoOpen,         RTGETOPT_REQ_NOTHING },
    { "--fstat",            kCmdOpt_FStat,          RTGETOPT_REQ_NOTHING },
    { "--no-fstat",         kCmdOpt_NoFStat,        RTGETOPT_REQ_NOTHING },
    { "--fchmod",           kCmdOpt_FChMod,         RTGETOPT_REQ_NOTHING },
    { "--no-fchmod",        kCmdOpt_NoFChMod,       RTGETOPT_REQ_NOTHING },
    { "--futimes",          kCmdOpt_FUtimes,        RTGETOPT_REQ_NOTHING },
    { "--no-futimes",       kCmdOpt_NoFUtimes,      RTGETOPT_REQ_NOTHING },
    { "--stat",             kCmdOpt_Stat,           RTGETOPT_REQ_NOTHING },
    { "--no-stat",          kCmdOpt_NoStat,         RTGETOPT_REQ_NOTHING },
    { "--chmod",            kCmdOpt_ChMod,          RTGETOPT_REQ_NOTHING },
    { "--no-chmod",         kCmdOpt_NoChMod,        RTGETOPT_REQ_NOTHING },
    { "--utimes",           kCmdOpt_Utimes,         RTGETOPT_REQ_NOTHING },
    { "--no-utimes",        kCmdOpt_NoUtimes,       RTGETOPT_REQ_NOTHING },
    { "--rename",           kCmdOpt_Rename,         RTGETOPT_REQ_NOTHING },
    { "--no-rename",        kCmdOpt_NoRename,       RTGETOPT_REQ_NOTHING },
    { "--dir-open",         kCmdOpt_DirOpen,        RTGETOPT_REQ_NOTHING },
    { "--no-dir-open",      kCmdOpt_NoDirOpen,      RTGETOPT_REQ_NOTHING },
    { "--dir-enum",         kCmdOpt_DirEnum,        RTGETOPT_REQ_NOTHING },
    { "--no-dir-enum",      kCmdOpt_NoDirEnum,      RTGETOPT_REQ_NOTHING },
    { "--mk-rm-dir",        kCmdOpt_MkRmDir,        RTGETOPT_REQ_NOTHING },
    { "--no-mk-rm-dir",     kCmdOpt_NoMkRmDir,      RTGETOPT_REQ_NOTHING },
    { "--stat-vfs",         kCmdOpt_StatVfs,        RTGETOPT_REQ_NOTHING },
    { "--no-stat-vfs",      kCmdOpt_NoStatVfs,      RTGETOPT_REQ_NOTHING },
    { "--rm",               kCmdOpt_Rm,             RTGETOPT_REQ_NOTHING },
    { "--no-rm",            kCmdOpt_NoRm,           RTGETOPT_REQ_NOTHING },
    { "--chsize",           kCmdOpt_ChSize,         RTGETOPT_REQ_NOTHING },
    { "--no-chsize",        kCmdOpt_NoChSize,       RTGETOPT_REQ_NOTHING },
    { "--read-tests",       kCmdOpt_ReadTests,      RTGETOPT_REQ_NOTHING },
    { "--no-read-tests",    kCmdOpt_NoReadTests,    RTGETOPT_REQ_NOTHING },
    { "--read-perf",        kCmdOpt_ReadPerf,       RTGETOPT_REQ_NOTHING },
    { "--no-read-perf",     kCmdOpt_NoReadPerf,     RTGETOPT_REQ_NOTHING },
    { "--sendfile",         kCmdOpt_SendFile,       RTGETOPT_REQ_NOTHING },
    { "--no-sendfile",      kCmdOpt_NoSendFile,     RTGETOPT_REQ_NOTHING },
#ifdef RT_OS_LINUX
    { "--splice",           kCmdOpt_Splice,         RTGETOPT_REQ_NOTHING },
    { "--no-splice",        kCmdOpt_NoSplice,       RTGETOPT_REQ_NOTHING },
#endif
    { "--write-tests",      kCmdOpt_WriteTests,     RTGETOPT_REQ_NOTHING },
    { "--no-write-tests",   kCmdOpt_NoWriteTests,   RTGETOPT_REQ_NOTHING },
    { "--write-perf",       kCmdOpt_WritePerf,      RTGETOPT_REQ_NOTHING },
    { "--no-write-perf",    kCmdOpt_NoWritePerf,    RTGETOPT_REQ_NOTHING },
    { "--seek",             kCmdOpt_Seek,           RTGETOPT_REQ_NOTHING },
    { "--no-seek",          kCmdOpt_NoSeek,         RTGETOPT_REQ_NOTHING },
    { "--fsync",            kCmdOpt_FSync,          RTGETOPT_REQ_NOTHING },
    { "--no-fsync",         kCmdOpt_NoFSync,        RTGETOPT_REQ_NOTHING },
    { "--mmap",             kCmdOpt_MMap,           RTGETOPT_REQ_NOTHING },
    { "--no-mmap",          kCmdOpt_NoMMap,         RTGETOPT_REQ_NOTHING },
    { "--ignore-no-cache",  kCmdOpt_IgnoreNoCache,  RTGETOPT_REQ_NOTHING },
    { "--no-ignore-no-cache",  kCmdOpt_NoIgnoreNoCache,  RTGETOPT_REQ_NOTHING },
    { "--io-file-size",     kCmdOpt_IoFileSize,     RTGETOPT_REQ_UINT64 },
    { "--set-block-size",   kCmdOpt_SetBlockSize,   RTGETOPT_REQ_UINT32 },
    { "--add-block-size",   kCmdOpt_AddBlockSize,   RTGETOPT_REQ_UINT32 },
    { "--copy",             kCmdOpt_Copy,           RTGETOPT_REQ_NOTHING },
    { "--no-copy",          kCmdOpt_NoCopy,         RTGETOPT_REQ_NOTHING },

    { "--show-duration",        kCmdOpt_ShowDuration,       RTGETOPT_REQ_NOTHING },
    { "--no-show-duration",     kCmdOpt_NoShowDuration,     RTGETOPT_REQ_NOTHING },
    { "--show-iterations",      kCmdOpt_ShowIterations,     RTGETOPT_REQ_NOTHING },
    { "--no-show-iterations",   kCmdOpt_NoShowIterations,   RTGETOPT_REQ_NOTHING },

    { "--quiet",                'q', RTGETOPT_REQ_NOTHING },
    { "--verbose",              'v', RTGETOPT_REQ_NOTHING },
    { "--version",              'V', RTGETOPT_REQ_NOTHING },
    { "--help",                 'h', RTGETOPT_REQ_NOTHING } /* for Usage() */
};

/** The test handle. */
static RTTEST       g_hTest;
/** The number of nanoseconds a RTTimeNanoTS call takes.
 * This is used for adjusting loop count estimates.  */
static uint64_t     g_nsPerNanoTSCall = 1;
/** Whether or not to display the duration of each profile run.
 * This is chiefly for verify the estimate phase.  */
static bool         g_fShowDuration = false;
/** Whether or not to display the iteration count for each profile run.
 * This is chiefly for verify the estimate phase.  */
static bool         g_fShowIterations = false;
/** Verbosity level. */
static uint32_t     g_uVerbosity = 0;

/** @name Selected subtest
 * @{ */
static bool         g_fManyFiles = true;
static bool         g_fOpen      = true;
static bool         g_fFStat     = true;
static bool         g_fFChMod    = true;
static bool         g_fFUtimes   = true;
static bool         g_fStat      = true;
static bool         g_fChMod     = true;
static bool         g_fUtimes    = true;
static bool         g_fRename    = true;
static bool         g_fDirOpen   = true;
static bool         g_fDirEnum   = true;
static bool         g_fMkRmDir   = true;
static bool         g_fStatVfs   = true;
static bool         g_fRm        = true;
static bool         g_fChSize    = true;
static bool         g_fReadTests = true;
static bool         g_fReadPerf  = true;
static bool         g_fSendFile  = true;
#ifdef RT_OS_LINUX
static bool         g_fSplice    = true;
#endif
static bool         g_fWriteTests= true;
static bool         g_fWritePerf = true;
static bool         g_fSeek      = true;
static bool         g_fFSync     = true;
static bool         g_fMMap      = true;
static bool         g_fCopy      = true;
/** @} */

/** The length of each test run. */
static uint64_t     g_nsTestRun                 = RT_NS_1SEC_64 * 10;

/** For the 'manyfiles' subdir.  */
static uint32_t     g_cManyFiles                = 10000;

/** Number of files in the 'manytree' directory tree.  */
static uint32_t     g_cManyTreeFiles            = 640 + 16*640 /*10880*/;
/** Number of files per directory in the 'manytree' construct. */
static uint32_t     g_cManyTreeFilesPerDir      = 640;
/** Number of subdirs per directory in the 'manytree' construct. */
static uint32_t     g_cManyTreeSubdirsPerDir    = 16;
/** The depth of the 'manytree' directory tree.  */
static uint32_t     g_cManyTreeDepth            = 1;
/** List of directories in the many tree, creation order. */
static RTLISTANCHOR g_ManyTreeHead;

/** Number of configured I/O block sizes. */
static uint32_t     g_cIoBlocks                 = 8;
/** Configured I/O block sizes. */
static uint32_t     g_acbIoBlocks[16]           = { 1, 512, 4096, 16384, 65536, _1M, _32M, _128M };
/** The desired size of the test file we use for I/O. */
static uint64_t     g_cbIoFile                  = _512M;
/** Whether to be less strict with non-cache file handle. */
static bool         g_fIgnoreNoCache            = false;

/** The length of g_szDir. */
static size_t       g_cchDir;
/** The length of g_szEmptyDir. */
static size_t       g_cchEmptyDir;
/** The length of g_szDeepDir. */
static size_t       g_cchDeepDir;

/** The test directory (absolute).  This will always have a trailing slash. */
static char         g_szDir[RTPATH_MAX];
/** The test directory (absolute), 2nd copy for use with InDir2().  */
static char         g_szDir2[RTPATH_MAX];
/** The empty test directory (absolute). This will always have a trailing slash. */
static char         g_szEmptyDir[RTPATH_MAX];
/** The deep test directory (absolute). This will always have a trailing slash. */
static char         g_szDeepDir[RTPATH_MAX];


/**
 * Yield the CPU and stuff before starting a test run.
 */
DECLINLINE(void) fsPerfYield(void)
{
    RTThreadYield();
    RTThreadYield();
}


/**
 * Profiles the RTTimeNanoTS call, setting g_nsPerNanoTSCall.
 */
static void fsPerfNanoTS(void)
{
    fsPerfYield();

    /* Make sure we start off on a changing timestamp on platforms will low time resoultion. */
    uint64_t nsStart = RTTimeNanoTS();
    uint64_t ns;
    do
        ns = RTTimeNanoTS();
    while (ns == nsStart);
    nsStart = ns;

    /* Call it for 10 ms. */
    uint32_t i = 0;
    do
    {
        i++;
        ns = RTTimeNanoTS();
    }
    while (ns - nsStart < RT_NS_10MS);

    g_nsPerNanoTSCall = (ns - nsStart) / i;
}


/**
 * Construct a path relative to the base test directory.
 *
 * @returns g_szDir.
 * @param   pszAppend           What to append.
 * @param   cchAppend           How much to append.
 */
DECLINLINE(char *) InDir(const char *pszAppend, size_t cchAppend)
{
    Assert(g_szDir[g_cchDir - 1] == RTPATH_SLASH);
    memcpy(&g_szDir[g_cchDir], pszAppend, cchAppend);
    g_szDir[g_cchDir + cchAppend] = '\0';
    return &g_szDir[0];
}


/**
 * Construct a path relative to the base test directory, 2nd copy.
 *
 * @returns g_szDir2.
 * @param   pszAppend           What to append.
 * @param   cchAppend           How much to append.
 */
DECLINLINE(char *) InDir2(const char *pszAppend, size_t cchAppend)
{
    Assert(g_szDir[g_cchDir - 1] == RTPATH_SLASH);
    memcpy(g_szDir2, g_szDir, g_cchDir);
    memcpy(&g_szDir2[g_cchDir], pszAppend, cchAppend);
    g_szDir2[g_cchDir + cchAppend] = '\0';
    return &g_szDir2[0];
}


/**
 * Construct a path relative to the empty directory.
 *
 * @returns g_szEmptyDir.
 * @param   pszAppend           What to append.
 * @param   cchAppend           How much to append.
 */
DECLINLINE(char *) InEmptyDir(const char *pszAppend, size_t cchAppend)
{
    Assert(g_szEmptyDir[g_cchEmptyDir - 1] == RTPATH_SLASH);
    memcpy(&g_szEmptyDir[g_cchEmptyDir], pszAppend, cchAppend);
    g_szEmptyDir[g_cchEmptyDir + cchAppend] = '\0';
    return &g_szEmptyDir[0];
}


/**
 * Construct a path relative to the deep test directory.
 *
 * @returns g_szDeepDir.
 * @param   pszAppend           What to append.
 * @param   cchAppend           How much to append.
 */
DECLINLINE(char *) InDeepDir(const char *pszAppend, size_t cchAppend)
{
    Assert(g_szDeepDir[g_cchDeepDir - 1] == RTPATH_SLASH);
    memcpy(&g_szDeepDir[g_cchDeepDir], pszAppend, cchAppend);
    g_szDeepDir[g_cchDeepDir + cchAppend] = '\0';
    return &g_szDeepDir[0];
}


/**
 * Prepares the test area.
 * @returns VBox status code.
 */
static int fsPrepTestArea(void)
{
    /* The empty subdir and associated globals: */
    static char s_szEmpty[] = "empty";
    memcpy(g_szEmptyDir, g_szDir, g_cchDir);
    memcpy(&g_szEmptyDir[g_cchDir], s_szEmpty, sizeof(s_szEmpty));
    g_cchEmptyDir = g_cchDir + sizeof(s_szEmpty) - 1;
    RTTESTI_CHECK_RC_RET(RTDirCreate(g_szEmptyDir, 0755, 0), VINF_SUCCESS, rcCheck);
    g_szEmptyDir[g_cchEmptyDir++] = RTPATH_SLASH;
    g_szEmptyDir[g_cchEmptyDir]   = '\0';
    RTTestIPrintf(RTTESTLVL_ALWAYS, "Empty dir: %s\n", g_szEmptyDir);

    /* Deep directory: */
    memcpy(g_szDeepDir, g_szDir, g_cchDir);
    g_cchDeepDir = g_cchDir;
    do
    {
        static char const s_szSub[] = "d" RTPATH_SLASH_STR;
        memcpy(&g_szDeepDir[g_cchDeepDir], s_szSub, sizeof(s_szSub));
        g_cchDeepDir += sizeof(s_szSub) - 1;
        RTTESTI_CHECK_RC_RET( RTDirCreate(g_szDeepDir, 0755, 0), VINF_SUCCESS, rcCheck);
    } while (g_cchDeepDir < 176);
    RTTestIPrintf(RTTESTLVL_ALWAYS, "Deep  dir: %s\n", g_szDeepDir);

    /* Create known file in both deep and shallow dirs: */
    RTFILE hKnownFile;
    RTTESTI_CHECK_RC_RET(RTFileOpen(&hKnownFile, InDir(RT_STR_TUPLE("known-file")),
                                    RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE),
                         VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC_RET(RTFileClose(hKnownFile), VINF_SUCCESS, rcCheck);

    RTTESTI_CHECK_RC_RET(RTFileOpen(&hKnownFile, InDeepDir(RT_STR_TUPLE("known-file")),
                                    RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE),
                         VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC_RET(RTFileClose(hKnownFile), VINF_SUCCESS, rcCheck);

    return VINF_SUCCESS;
}


/**
 * Create a name list entry.
 * @returns Pointer to the entry, NULL if out of memory.
 * @param   pchName             The name.
 * @param   cchName             The name length.
 */
PFSPERFNAMEENTRY fsPerfCreateNameEntry(const char *pchName, size_t cchName)
{
    PFSPERFNAMEENTRY pEntry = (PFSPERFNAMEENTRY)RTMemAllocVar(RT_UOFFSETOF_DYN(FSPERFNAMEENTRY, szName[cchName + 1]));
    if (pEntry)
    {
        RTListInit(&pEntry->Entry);
        pEntry->cchName         = (uint16_t)cchName;
        memcpy(pEntry->szName, pchName, cchName);
        pEntry->szName[cchName] = '\0';
    }
    return pEntry;
}


static int fsPerfManyTreeRecursiveDirCreator(size_t cchDir, uint32_t iDepth)
{
    PFSPERFNAMEENTRY pEntry = fsPerfCreateNameEntry(g_szDir, cchDir);
    RTTESTI_CHECK_RET(pEntry, VERR_NO_MEMORY);
    RTListAppend(&g_ManyTreeHead, &pEntry->Entry);

    RTTESTI_CHECK_RC_RET(RTDirCreate(g_szDir, 0755, RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_DONT_SET | RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_NOT_CRITICAL),
                         VINF_SUCCESS, rcCheck);

    if (iDepth < g_cManyTreeDepth)
        for (uint32_t i = 0; i < g_cManyTreeSubdirsPerDir; i++)
        {
            size_t cchSubDir = RTStrPrintf(&g_szDir[cchDir], sizeof(g_szDir) - cchDir, "d%02u" RTPATH_SLASH_STR, i);
            RTTESTI_CHECK_RC_RET(fsPerfManyTreeRecursiveDirCreator(cchDir + cchSubDir, iDepth + 1), VINF_SUCCESS, rcCheck);
        }

    return VINF_SUCCESS;
}


void fsPerfManyFiles(void)
{
    RTTestISub("manyfiles");

    /*
     * Create a sub-directory with like 10000 files in it.
     *
     * This does push the directory organization of the underlying file system,
     * which is something we might not want to profile with shared folders.  It
     * is however useful for directory enumeration.
     */
    RTTESTI_CHECK_RC_RETV(RTDirCreate(InDir(RT_STR_TUPLE("manyfiles")), 0755,
                                      RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_DONT_SET | RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_NOT_CRITICAL),
                          VINF_SUCCESS);

    size_t offFilename = strlen(g_szDir);
    g_szDir[offFilename++] = RTPATH_SLASH;

    fsPerfYield();
    RTFILE hFile;
    uint64_t const nsStart = RTTimeNanoTS();
    for (uint32_t i = 0; i < g_cManyFiles; i++)
    {
        RTStrFormatU32(&g_szDir[offFilename], sizeof(g_szDir) - offFilename, i, 10, 5, 5, RTSTR_F_ZEROPAD);
        RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile, g_szDir, RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
    }
    uint64_t const cNsElapsed = RTTimeNanoTS() - nsStart;
    RTTestIValueF(cNsElapsed, RTTESTUNIT_NS, "Creating %u empty files in single directory", g_cManyFiles);
    RTTestIValueF(cNsElapsed / g_cManyFiles, RTTESTUNIT_NS_PER_OCCURRENCE, "Create empty file (single dir)");

    /*
     * Create a bunch of directories with exacly 32 files in each, hoping to
     * avoid any directory organization artifacts.
     */
    /* Create the directories first, building a list of them for simplifying iteration: */
    RTListInit(&g_ManyTreeHead);
    InDir(RT_STR_TUPLE("manytree" RTPATH_SLASH_STR));
    RTTESTI_CHECK_RC_RETV(fsPerfManyTreeRecursiveDirCreator(strlen(g_szDir), 0), VINF_SUCCESS);

    /* Create the zero byte files: */
    fsPerfYield();
    uint64_t const nsStart2 = RTTimeNanoTS();
    uint32_t cFiles = 0;
    PFSPERFNAMEENTRY pCur;
    RTListForEach(&g_ManyTreeHead, pCur, FSPERFNAMEENTRY, Entry)
    {
        char szPath[RTPATH_MAX];
        memcpy(szPath, pCur->szName, pCur->cchName);
        for (uint32_t i = 0; i < g_cManyTreeFilesPerDir; i++)
        {
            RTStrFormatU32(&szPath[pCur->cchName], sizeof(szPath) - pCur->cchName, i, 10, 5, 5, RTSTR_F_ZEROPAD);
            RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile, szPath, RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
            cFiles++;
        }
    }
    uint64_t const cNsElapsed2 = RTTimeNanoTS() - nsStart2;
    RTTestIValueF(cNsElapsed2, RTTESTUNIT_NS, "Creating %u empty files in tree", cFiles);
    RTTestIValueF(cNsElapsed2 / cFiles, RTTESTUNIT_NS_PER_OCCURRENCE, "Create empty file (tree)");
    RTTESTI_CHECK(g_cManyTreeFiles == cFiles);
}


DECL_FORCE_INLINE(int) fsPerfOpenExistingOnceReadonly(const char *pszFile)
{
    RTFILE hFile;
    RTTESTI_CHECK_RC_RET(RTFileOpen(&hFile, pszFile, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
    return VINF_SUCCESS;
}


DECL_FORCE_INLINE(int) fsPerfOpenExistingOnceWriteonly(const char *pszFile)
{
    RTFILE hFile;
    RTTESTI_CHECK_RC_RET(RTFileOpen(&hFile, pszFile, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
    return VINF_SUCCESS;
}


/** @note tstRTFileOpenEx-1.cpp has a copy of this code.   */
static void tstOpenExTest(unsigned uLine, int cbExist, int cbNext, const char *pszFilename, uint64_t fAction,
                          int rcExpect, RTFILEACTION enmActionExpected)
{
    uint64_t const  fCreateMode = (0644 << RTFILE_O_CREATE_MODE_SHIFT);
    RTFILE          hFile;
    int             rc;

    /*
     * File existence and size.
     */
    bool fOkay = false;
    RTFSOBJINFO ObjInfo;
    rc = RTPathQueryInfoEx(pszFilename, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
    if (RT_SUCCESS(rc))
        fOkay = cbExist == (int64_t)ObjInfo.cbObject;
    else
        fOkay = rc == VERR_FILE_NOT_FOUND && cbExist < 0;
    if (!fOkay)
    {
        if (cbExist >= 0)
        {
            rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | fCreateMode);
            if (RT_SUCCESS(rc))
            {
                while (cbExist > 0)
                {
                    int cbToWrite = (int)strlen(pszFilename);
                    if (cbToWrite > cbExist)
                        cbToWrite = cbExist;
                    rc = RTFileWrite(hFile, pszFilename, cbToWrite, NULL);
                    if (RT_FAILURE(rc))
                    {
                        RTTestIFailed("%u: RTFileWrite(%s,%#x) -> %Rrc\n", uLine, pszFilename, cbToWrite, rc);
                        break;
                    }
                    cbExist -= cbToWrite;
                }

                RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
            }
            else
                RTTestIFailed("%u: RTFileDelete(%s) -> %Rrc\n", uLine, pszFilename, rc);

        }
        else
        {
            rc = RTFileDelete(pszFilename);
            if (rc != VINF_SUCCESS && rc != VERR_FILE_NOT_FOUND)
                RTTestIFailed("%u: RTFileDelete(%s) -> %Rrc\n", uLine, pszFilename, rc);
        }
    }

    /*
     * The actual test.
     */
    RTFILEACTION enmActuallyTaken = RTFILEACTION_END;
    hFile = NIL_RTFILE;
    rc = RTFileOpenEx(pszFilename, fAction | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | fCreateMode, &hFile, &enmActuallyTaken);
    if (   rc != rcExpect
        || enmActuallyTaken != enmActionExpected
        || (RT_SUCCESS(rc) ? hFile == NIL_RTFILE : hFile != NIL_RTFILE))
        RTTestIFailed("%u: RTFileOpenEx(%s, %#llx) -> %Rrc + %d  (hFile=%p), expected %Rrc + %d\n",
                      uLine, pszFilename, fAction, rc, enmActuallyTaken, hFile, rcExpect, enmActionExpected);
    if (RT_SUCCESS(rc))
    {
        if (   enmActionExpected == RTFILEACTION_REPLACED
            || enmActionExpected == RTFILEACTION_TRUNCATED)
        {
            uint8_t abBuf[16];
            rc = RTFileRead(hFile, abBuf, 1, NULL);
            if (rc != VERR_EOF)
                RTTestIFailed("%u: RTFileRead(%s,,1,) -> %Rrc, expected VERR_EOF\n", uLine, pszFilename, rc);
        }

        while (cbNext > 0)
        {
            int cbToWrite = (int)strlen(pszFilename);
            if (cbToWrite > cbNext)
                cbToWrite = cbNext;
            rc = RTFileWrite(hFile, pszFilename, cbToWrite, NULL);
            if (RT_FAILURE(rc))
            {
                RTTestIFailed("%u: RTFileWrite(%s,%#x) -> %Rrc\n", uLine, pszFilename, cbToWrite, rc);
                break;
            }
            cbNext -= cbToWrite;
        }

        rc = RTFileClose(hFile);
        if (RT_FAILURE(rc))
            RTTestIFailed("%u: RTFileClose(%p) -> %Rrc\n", uLine, hFile, rc);
    }
}


void fsPerfOpen(void)
{
    RTTestISub("open");

    /* Opening non-existing files. */
    RTFILE hFile;
    RTTESTI_CHECK_RC(RTFileOpen(&hFile, InEmptyDir(RT_STR_TUPLE("no-such-file")),
                                RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileOpen(&hFile, InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")),
                                RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileOpen(&hFile, InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")),
                                RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VERR_PATH_NOT_FOUND);

    /*
     * The following is copied from tstRTFileOpenEx-1.cpp:
     */
    InDir(RT_STR_TUPLE("file1"));
    tstOpenExTest(__LINE__, -1, -1, g_szDir, RTFILE_O_OPEN,                        VERR_FILE_NOT_FOUND, RTFILEACTION_INVALID);
    tstOpenExTest(__LINE__, -1, -1, g_szDir, RTFILE_O_OPEN_CREATE,                        VINF_SUCCESS, RTFILEACTION_CREATED);
    tstOpenExTest(__LINE__,  0,  0, g_szDir, RTFILE_O_OPEN_CREATE,                        VINF_SUCCESS, RTFILEACTION_OPENED);
    tstOpenExTest(__LINE__,  0,  0, g_szDir, RTFILE_O_OPEN,                               VINF_SUCCESS, RTFILEACTION_OPENED);

    tstOpenExTest(__LINE__,  0,  0, g_szDir, RTFILE_O_OPEN | RTFILE_O_TRUNCATE,           VINF_SUCCESS, RTFILEACTION_TRUNCATED);
    tstOpenExTest(__LINE__,  0, 10, g_szDir, RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE,    VINF_SUCCESS, RTFILEACTION_TRUNCATED);
    tstOpenExTest(__LINE__, 10, 10, g_szDir, RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE,    VINF_SUCCESS, RTFILEACTION_TRUNCATED);
    tstOpenExTest(__LINE__, 10, -1, g_szDir, RTFILE_O_OPEN | RTFILE_O_TRUNCATE,           VINF_SUCCESS, RTFILEACTION_TRUNCATED);
    tstOpenExTest(__LINE__, -1, -1, g_szDir, RTFILE_O_OPEN | RTFILE_O_TRUNCATE,    VERR_FILE_NOT_FOUND, RTFILEACTION_INVALID);
    tstOpenExTest(__LINE__, -1,  0, g_szDir, RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE,    VINF_SUCCESS, RTFILEACTION_CREATED);

    tstOpenExTest(__LINE__,  0, -1, g_szDir, RTFILE_O_CREATE_REPLACE,                     VINF_SUCCESS, RTFILEACTION_REPLACED);
    tstOpenExTest(__LINE__, -1,  0, g_szDir, RTFILE_O_CREATE_REPLACE,                     VINF_SUCCESS, RTFILEACTION_CREATED);
    tstOpenExTest(__LINE__,  0, -1, g_szDir, RTFILE_O_CREATE,                      VERR_ALREADY_EXISTS, RTFILEACTION_ALREADY_EXISTS);
    tstOpenExTest(__LINE__, -1, -1, g_szDir, RTFILE_O_CREATE,                             VINF_SUCCESS, RTFILEACTION_CREATED);

    tstOpenExTest(__LINE__, -1, 10, g_szDir, RTFILE_O_CREATE | RTFILE_O_TRUNCATE,         VINF_SUCCESS, RTFILEACTION_CREATED);
    tstOpenExTest(__LINE__, 10, 10, g_szDir, RTFILE_O_CREATE | RTFILE_O_TRUNCATE,  VERR_ALREADY_EXISTS, RTFILEACTION_ALREADY_EXISTS);
    tstOpenExTest(__LINE__, 10, -1, g_szDir, RTFILE_O_CREATE_REPLACE | RTFILE_O_TRUNCATE, VINF_SUCCESS, RTFILEACTION_REPLACED);
    tstOpenExTest(__LINE__, -1, -1, g_szDir, RTFILE_O_CREATE_REPLACE | RTFILE_O_TRUNCATE, VINF_SUCCESS, RTFILEACTION_CREATED);

    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VINF_SUCCESS);

    /*
     * Create file1 and then try exclusivly creating it again.
     * Then profile opening it for reading.
     */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file1")),
                                     RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileOpen(&hFile, g_szDir, RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VERR_ALREADY_EXISTS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    PROFILE_FN(fsPerfOpenExistingOnceReadonly(g_szDir),  g_nsTestRun, "RTFileOpen/Close/Readonly");
    PROFILE_FN(fsPerfOpenExistingOnceWriteonly(g_szDir), g_nsTestRun, "RTFileOpen/Close/Writeonly");

    /*
     * Profile opening in the deep directory too.
     */
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDeepDir(RT_STR_TUPLE("file1")),
                                     RTFILE_O_CREATE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    PROFILE_FN(fsPerfOpenExistingOnceReadonly(g_szDeepDir),  g_nsTestRun, "RTFileOpen/Close/deep/readonly");
    PROFILE_FN(fsPerfOpenExistingOnceWriteonly(g_szDeepDir), g_nsTestRun, "RTFileOpen/Close/deep/writeonly");

    /* Manytree: */
    char szPath[RTPATH_MAX];
    PROFILE_MANYTREE_FN(szPath, fsPerfOpenExistingOnceReadonly(szPath), 1, g_nsTestRun, "RTFileOpen/Close/manytree/readonly");
}


void fsPerfFStat(void)
{
    RTTestISub("fstat");
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file2")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo = {0};
    PROFILE_FN(RTFileQueryInfo(hFile1, &ObjInfo,  RTFSOBJATTRADD_NOTHING), g_nsTestRun, "RTFileQueryInfo/NOTHING");
    PROFILE_FN(RTFileQueryInfo(hFile1, &ObjInfo,  RTFSOBJATTRADD_UNIX),    g_nsTestRun, "RTFileQueryInfo/UNIX");

    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
}


void fsPerfFChMod(void)
{
    RTTestISub("fchmod");
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file4")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo = {0};
    RTTESTI_CHECK_RC(RTFileQueryInfo(hFile1, &ObjInfo,  RTFSOBJATTRADD_NOTHING), VINF_SUCCESS);
    RTFMODE const fEvenMode = (ObjInfo.Attr.fMode & ~RTFS_UNIX_ALL_ACCESS_PERMS) | RTFS_DOS_READONLY   | 0400;
    RTFMODE const fOddMode  = (ObjInfo.Attr.fMode & ~(RTFS_UNIX_ALL_ACCESS_PERMS | RTFS_DOS_READONLY)) | 0640;
    PROFILE_FN(RTFileSetMode(hFile1, iIteration & 1 ? fOddMode : fEvenMode), g_nsTestRun, "RTFileSetMode");

    RTFileSetMode(hFile1, ObjInfo.Attr.fMode);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
}


void fsPerfFUtimes(void)
{
    RTTestISub("futimes");
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file5")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTIMESPEC Time1;
    RTTimeNow(&Time1);
    RTTIMESPEC Time2 = Time1;
    RTTimeSpecSubSeconds(&Time2, 3636);

    RTFSOBJINFO ObjInfo0 = {0};
    RTTESTI_CHECK_RC(RTFileQueryInfo(hFile1, &ObjInfo0,  RTFSOBJATTRADD_NOTHING), VINF_SUCCESS);

    /* Modify modification time: */
    RTTESTI_CHECK_RC(RTFileSetTimes(hFile1, NULL, &Time2, NULL, NULL), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo1 = {0};
    RTTESTI_CHECK_RC(RTFileQueryInfo(hFile1, &ObjInfo1,  RTFSOBJATTRADD_NOTHING), VINF_SUCCESS);
    RTTESTI_CHECK((RTTimeSpecGetSeconds(&ObjInfo1.ModificationTime) >> 2) == (RTTimeSpecGetSeconds(&Time2) >> 2));
    char sz1[RTTIME_STR_LEN], sz2[RTTIME_STR_LEN]; /* Div by 1000 here for posix impl. using timeval. */
    RTTESTI_CHECK_MSG(RTTimeSpecGetNano(&ObjInfo1.AccessTime) / 1000 == RTTimeSpecGetNano(&ObjInfo0.AccessTime) / 1000,
                      ("%s, expected %s", RTTimeSpecToString(&ObjInfo1.AccessTime, sz1, sizeof(sz1)),
                       RTTimeSpecToString(&ObjInfo0.AccessTime, sz2, sizeof(sz2))));

    /* Modify access time: */
    RTTESTI_CHECK_RC(RTFileSetTimes(hFile1, &Time1, NULL, NULL, NULL), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo2 = {0};
    RTTESTI_CHECK_RC(RTFileQueryInfo(hFile1, &ObjInfo2,  RTFSOBJATTRADD_NOTHING), VINF_SUCCESS);
    RTTESTI_CHECK((RTTimeSpecGetSeconds(&ObjInfo2.AccessTime) >> 2) == (RTTimeSpecGetSeconds(&Time1) >> 2));
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo2.ModificationTime) / 1000 == RTTimeSpecGetNano(&ObjInfo1.ModificationTime) / 1000);

    /* Benchmark it: */
    PROFILE_FN(RTFileSetTimes(hFile1, NULL, iIteration & 1 ? &Time1 : &Time2, NULL, NULL), g_nsTestRun, "RTFileSetTimes");

    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
}


void fsPerfStat(void)
{
    RTTestISub("stat");
    RTFSOBJINFO ObjInfo;

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(InEmptyDir(RT_STR_TUPLE("no-such-file")),
                                       &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")),
                                       &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")),
                                       &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK), VERR_PATH_NOT_FOUND);

    /* Shallow: */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file3")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    PROFILE_FN(RTPathQueryInfoEx(g_szDir, &ObjInfo,  RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK), g_nsTestRun,
               "RTPathQueryInfoEx/NOTHING");
    PROFILE_FN(RTPathQueryInfoEx(g_szDir, &ObjInfo,  RTFSOBJATTRADD_UNIX,     RTPATH_F_ON_LINK), g_nsTestRun,
               "RTPathQueryInfoEx/UNIX");


    /* Deep: */
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDeepDir(RT_STR_TUPLE("file3")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    PROFILE_FN(RTPathQueryInfoEx(g_szDeepDir, &ObjInfo,  RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK), g_nsTestRun,
               "RTPathQueryInfoEx/deep/NOTHING");
    PROFILE_FN(RTPathQueryInfoEx(g_szDeepDir, &ObjInfo,  RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK), g_nsTestRun,
               "RTPathQueryInfoEx/deep/UNIX");

    /* Manytree: */
    char szPath[RTPATH_MAX];
    PROFILE_MANYTREE_FN(szPath, RTPathQueryInfoEx(szPath, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK),
                        1, g_nsTestRun, "RTPathQueryInfoEx/manytree/NOTHING");
    PROFILE_MANYTREE_FN(szPath, RTPathQueryInfoEx(szPath, &ObjInfo, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK),
                        1, g_nsTestRun, "RTPathQueryInfoEx/manytree/UNIX");
}


void fsPerfChmod(void)
{
    RTTestISub("chmod");

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTPathSetMode(InEmptyDir(RT_STR_TUPLE("no-such-file")), 0665),
                     VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathSetMode(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")), 0665),
                     FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathSetMode(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")), 0665), VERR_PATH_NOT_FOUND);

    /* Shallow: */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file14")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    RTFSOBJINFO ObjInfo;
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(g_szDir, &ObjInfo,  RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTFMODE const fEvenMode = (ObjInfo.Attr.fMode & ~RTFS_UNIX_ALL_ACCESS_PERMS) | RTFS_DOS_READONLY   | 0400;
    RTFMODE const fOddMode  = (ObjInfo.Attr.fMode & ~(RTFS_UNIX_ALL_ACCESS_PERMS | RTFS_DOS_READONLY)) | 0640;
    PROFILE_FN(RTPathSetMode(g_szDir, iIteration & 1 ? fOddMode : fEvenMode), g_nsTestRun, "RTPathSetMode");
    RTPathSetMode(g_szDir, ObjInfo.Attr.fMode);

    /* Deep: */
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDeepDir(RT_STR_TUPLE("file14")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    PROFILE_FN(RTPathSetMode(g_szDeepDir, iIteration & 1 ? fOddMode : fEvenMode), g_nsTestRun, "RTPathSetMode/deep");
    RTPathSetMode(g_szDeepDir, ObjInfo.Attr.fMode);

    /* Manytree: */
    char szPath[RTPATH_MAX];
    PROFILE_MANYTREE_FN(szPath, RTPathSetMode(szPath, iIteration & 1 ? fOddMode : fEvenMode), 1, g_nsTestRun,
                        "RTPathSetMode/manytree");
    DO_MANYTREE_FN(szPath, RTPathSetMode(szPath, ObjInfo.Attr.fMode));
}


void fsPerfUtimes(void)
{
    RTTestISub("utimes");

    RTTIMESPEC Time1;
    RTTimeNow(&Time1);
    RTTIMESPEC Time2 = Time1;
    RTTimeSpecSubSeconds(&Time2, 3636);

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTPathSetTimesEx(InEmptyDir(RT_STR_TUPLE("no-such-file")), NULL, &Time1, NULL, NULL, RTPATH_F_ON_LINK),
                     VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathSetTimesEx(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")),
                                      NULL, &Time1, NULL, NULL, RTPATH_F_ON_LINK),
                     FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathSetTimesEx(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")),
                                      NULL, &Time1, NULL, NULL, RTPATH_F_ON_LINK),
                     VERR_PATH_NOT_FOUND);

    /* Shallow: */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file15")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    RTFSOBJINFO ObjInfo0 = {0};
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(g_szDir, &ObjInfo0,  RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK), VINF_SUCCESS);

    /* Modify modification time: */
    RTTESTI_CHECK_RC(RTPathSetTimesEx(g_szDir, NULL, &Time2, NULL, NULL, RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo1;
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(g_szDir, &ObjInfo1,  RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTTESTI_CHECK((RTTimeSpecGetSeconds(&ObjInfo1.ModificationTime) >> 2) == (RTTimeSpecGetSeconds(&Time2) >> 2));
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo1.AccessTime) / 1000 == RTTimeSpecGetNano(&ObjInfo0.AccessTime) / 1000 /* posix timeval */);

    /* Modify access time: */
    RTTESTI_CHECK_RC(RTPathSetTimesEx(g_szDir, &Time1, NULL, NULL, NULL, RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo2 = {0};
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(g_szDir, &ObjInfo2,  RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTTESTI_CHECK((RTTimeSpecGetSeconds(&ObjInfo2.AccessTime) >> 2) == (RTTimeSpecGetSeconds(&Time1) >> 2));
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo2.ModificationTime) / 1000 == RTTimeSpecGetNano(&ObjInfo1.ModificationTime) / 1000 /* posix timeval */);

    /* Profile shallow: */
    PROFILE_FN(RTPathSetTimesEx(g_szDir, iIteration & 1 ? &Time1 : &Time2, iIteration & 1 ? &Time2 : &Time1,
                                NULL, NULL, RTPATH_F_ON_LINK),
               g_nsTestRun, "RTPathSetTimesEx");

    /* Deep: */
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDeepDir(RT_STR_TUPLE("file15")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    PROFILE_FN(RTPathSetTimesEx(g_szDeepDir, iIteration & 1 ? &Time1 : &Time2, iIteration & 1 ? &Time2 : &Time1,
                                NULL, NULL, RTPATH_F_ON_LINK),
               g_nsTestRun, "RTPathSetTimesEx/deep");

    /* Manytree: */
    char szPath[RTPATH_MAX];
    PROFILE_MANYTREE_FN(szPath, RTPathSetTimesEx(szPath, iIteration & 1 ? &Time1 : &Time2, iIteration & 1 ? &Time2 : &Time1,
                                                 NULL, NULL, RTPATH_F_ON_LINK),
                        1, g_nsTestRun, "RTPathSetTimesEx/manytree");
}


DECL_FORCE_INLINE(int) fsPerfRenameMany(const char *pszFile, uint32_t iIteration)
{
    char szRenamed[RTPATH_MAX];
    strcat(strcpy(szRenamed, pszFile), "-renamed");
    if (!(iIteration & 1))
        return RTPathRename(pszFile, szRenamed, 0);
    return RTPathRename(szRenamed, pszFile, 0);
}


void fsPerfRename(void)
{
    RTTestISub("rename");
    char szPath[RTPATH_MAX];

/** @todo rename directories too!   */
/** @todo check overwriting files and directoris (empty ones should work on
 *        unix). */

    /* Non-existing files. */
    strcpy(szPath, InEmptyDir(RT_STR_TUPLE("other-no-such-file")));
    RTTESTI_CHECK_RC(RTPathRename(InEmptyDir(RT_STR_TUPLE("no-such-file")), szPath, 0), VERR_FILE_NOT_FOUND);
    strcpy(szPath, InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "other-no-such-file")));
    RTTESTI_CHECK_RC(RTPathRename(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")), szPath, 0),
                     FSPERF_VERR_PATH_NOT_FOUND);
    strcpy(szPath, InEmptyDir(RT_STR_TUPLE("other-no-such-file")));
    RTTESTI_CHECK_RC(RTPathRename(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")), szPath, 0), VERR_PATH_NOT_FOUND);

    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file16")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    strcat(strcpy(szPath, g_szDir), "-no-such-dir" RTPATH_SLASH_STR "file16");
    RTTESTI_CHECK_RC(RTPathRename(szPath, g_szDir, 0), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathRename(g_szDir, szPath, 0), FSPERF_VERR_PATH_NOT_FOUND);

    /* Shallow: */
    strcat(strcpy(szPath, g_szDir), "-other");
    PROFILE_FN(RTPathRename(iIteration & 1 ? szPath : g_szDir, iIteration & 1 ? g_szDir : szPath, 0), g_nsTestRun, "RTPathRename");

    /* Deep: */
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDeepDir(RT_STR_TUPLE("file15")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

    strcat(strcpy(szPath, g_szDeepDir), "-other");
    PROFILE_FN(RTPathRename(iIteration & 1 ? szPath : g_szDeepDir, iIteration & 1 ? g_szDeepDir : szPath, 0),
               g_nsTestRun, "RTPathRename/deep");

    /* Manytree: */
    PROFILE_MANYTREE_FN(szPath, fsPerfRenameMany(szPath, iIteration), 2, g_nsTestRun, "RTPathRename/manytree");
}


DECL_FORCE_INLINE(int) fsPerfOpenClose(const char *pszDir)
{
    RTDIR hDir;
    RTTESTI_CHECK_RC_RET(RTDirOpen(&hDir, pszDir), VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC(RTDirClose(hDir), VINF_SUCCESS);
    return VINF_SUCCESS;
}


void vsPerfDirOpen(void)
{
    RTTestISub("dir open");
    RTDIR hDir;

    /*
     * Non-existing files.
     */
    RTTESTI_CHECK_RC(RTDirOpen(&hDir, InEmptyDir(RT_STR_TUPLE("no-such-file"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirOpen(&hDir, InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirOpen(&hDir, InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);

    /*
     * Check that open + close works.
     */
    g_szEmptyDir[g_cchEmptyDir] = '\0';
    RTTESTI_CHECK_RC_RETV(RTDirOpen(&hDir, g_szEmptyDir), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTDirClose(hDir), VINF_SUCCESS);


    /*
     * Profile empty dir and dir with many files.
     */
    g_szEmptyDir[g_cchEmptyDir] = '\0';
    PROFILE_FN(fsPerfOpenClose(g_szEmptyDir), g_nsTestRun, "RTDirOpen/Close empty");
    if (g_fManyFiles)
    {
        InDir(RT_STR_TUPLE("manyfiles"));
        PROFILE_FN(fsPerfOpenClose(g_szDir), g_nsTestRun, "RTDirOpen/Close manyfiles");
    }
}


DECL_FORCE_INLINE(int) fsPerfEnumEmpty(void)
{
    RTDIR hDir;
    g_szEmptyDir[g_cchEmptyDir] = '\0';
    RTTESTI_CHECK_RC_RET(RTDirOpen(&hDir, g_szEmptyDir), VINF_SUCCESS, rcCheck);

    RTDIRENTRY Entry;
    RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VERR_NO_MORE_FILES);

    RTTESTI_CHECK_RC(RTDirClose(hDir), VINF_SUCCESS);
    return VINF_SUCCESS;
}


DECL_FORCE_INLINE(int) fsPerfEnumManyFiles(void)
{
    RTDIR hDir;
    RTTESTI_CHECK_RC_RET(RTDirOpen(&hDir, InDir(RT_STR_TUPLE("manyfiles"))), VINF_SUCCESS, rcCheck);
    uint32_t cLeft = g_cManyFiles + 2;
    for (;;)
    {
        RTDIRENTRY Entry;
        if (cLeft > 0)
            RTTESTI_CHECK_RC_BREAK(RTDirRead(hDir, &Entry, NULL), VINF_SUCCESS);
        else
        {
            RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VERR_NO_MORE_FILES);
            break;
        }
        cLeft--;
    }
    RTTESTI_CHECK_RC(RTDirClose(hDir), VINF_SUCCESS);
    return VINF_SUCCESS;
}


void vsPerfDirEnum(void)
{
    RTTestISub("dir enum");
    RTDIR hDir;

    /*
     * The empty directory.
     */
    g_szEmptyDir[g_cchEmptyDir] = '\0';
    RTTESTI_CHECK_RC_RETV(RTDirOpen(&hDir, g_szEmptyDir), VINF_SUCCESS);

    uint32_t   fDots = 0;
    RTDIRENTRY Entry;
    RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VINF_SUCCESS);
    RTTESTI_CHECK(RTDirEntryIsStdDotLink(&Entry));
    fDots |= RT_BIT_32(Entry.cbName - 1);

    RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VINF_SUCCESS);
    RTTESTI_CHECK(RTDirEntryIsStdDotLink(&Entry));
    fDots |= RT_BIT_32(Entry.cbName - 1);
    RTTESTI_CHECK(fDots == 3);

    RTTESTI_CHECK_RC(RTDirRead(hDir, &Entry, NULL), VERR_NO_MORE_FILES);

    RTTESTI_CHECK_RC(RTDirClose(hDir), VINF_SUCCESS);

    /*
     * The directory with many files in it.
     */
    if (g_fManyFiles)
    {
        fDots = 0;
        uint32_t const cBitmap  = RT_ALIGN_32(g_cManyFiles, 64);
        void          *pvBitmap = alloca(cBitmap / 8);
        RT_BZERO(pvBitmap, cBitmap / 8);
        for (uint32_t i = g_cManyFiles; i < cBitmap; i++)
            ASMBitSet(pvBitmap, i);

        uint32_t cFiles = 0;
        RTTESTI_CHECK_RC_RETV(RTDirOpen(&hDir, InDir(RT_STR_TUPLE("manyfiles"))), VINF_SUCCESS);
        for (;;)
        {
            int rc = RTDirRead(hDir, &Entry, NULL);
            if (rc == VINF_SUCCESS)
            {
                if (Entry.szName[0] == '.')
                {
                    if (Entry.szName[1] == '.')
                    {
                        RTTESTI_CHECK(!(fDots & 2));
                        fDots |= 2;
                    }
                    else
                    {
                        RTTESTI_CHECK(Entry.szName[1] == '\0');
                        RTTESTI_CHECK(!(fDots & 1));
                        fDots |= 1;
                    }
                }
                else
                {
                    uint32_t iFile = UINT32_MAX;
                    RTTESTI_CHECK_RC(RTStrToUInt32Full(Entry.szName, 10, &iFile), VINF_SUCCESS);
                    if (   iFile < g_cManyFiles
                        && !ASMBitTest(pvBitmap, iFile))
                    {
                        ASMBitSet(pvBitmap, iFile);
                        cFiles++;
                    }
                    else
                        RTTestFailed(g_hTest, "line %u: iFile=%u g_cManyFiles=%u\n", __LINE__, iFile, g_cManyFiles);
                }
            }
            else if (rc == VERR_NO_MORE_FILES)
                break;
            else
            {
                RTTestFailed(g_hTest, "RTDirRead failed enumerating manyfiles: %Rrc\n", rc);
                RTDirClose(hDir);
                return;
            }
        }
        RTTESTI_CHECK_RC(RTDirClose(hDir), VINF_SUCCESS);
        RTTESTI_CHECK(fDots == 3);
        RTTESTI_CHECK(cFiles == g_cManyFiles);
        RTTESTI_CHECK(ASMMemIsAllU8(pvBitmap, cBitmap / 8, 0xff));
    }

    /*
     * Profile.
     */
    PROFILE_FN(fsPerfEnumEmpty(),g_nsTestRun, "RTDirOpen/Read/Close empty");
    if (g_fManyFiles)
        PROFILE_FN(fsPerfEnumManyFiles(), g_nsTestRun, "RTDirOpen/Read/Close manyfiles");
}


void fsPerfMkRmDir(void)
{
    RTTestISub("mkdir/rmdir");

    /* Non-existing directories: */
    RTTESTI_CHECK_RC(RTDirRemove(InEmptyDir(RT_STR_TUPLE("no-such-dir"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirRemove(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirCreate(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")), 0755, 0), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirCreate(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")), 0755, 0), VERR_PATH_NOT_FOUND);

    /** @todo check what happens if non-final path component isn't a directory. unix
     *        should return ENOTDIR and IPRT translates that to VERR_PATH_NOT_FOUND.
     *        Curious what happens on windows. */

    /* Already existing directories and files: */
    RTTESTI_CHECK_RC(RTDirCreate(InEmptyDir(RT_STR_TUPLE(".")), 0755, 0), VERR_ALREADY_EXISTS);
    RTTESTI_CHECK_RC(RTDirCreate(InEmptyDir(RT_STR_TUPLE("..")), 0755, 0), VERR_ALREADY_EXISTS);

    /* Remove directory with subdirectories: */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE("."))), VERR_DIR_NOT_EMPTY);
#else
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE("."))), VERR_INVALID_PARAMETER); /* EINVAL for '.' */
#endif
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    int rc = RTDirRemove(InDir(RT_STR_TUPLE("..")));
# ifdef RT_OS_WINDOWS
    if (rc != VERR_DIR_NOT_EMPTY /*ntfs root*/ && rc != VERR_SHARING_VIOLATION /*ntfs weird*/)
        RTTestIFailed("RTDirRemove(%s) -> %Rrc, expected VERR_DIR_NOT_EMPTY or VERR_SHARING_VIOLATION",  g_szDir, rc);
# else
    if (rc != VERR_DIR_NOT_EMPTY && rc != VERR_RESOURCE_BUSY /*IPRT/kLIBC fun*/)
        RTTestIFailed("RTDirRemove(%s) -> %Rrc, expected VERR_DIR_NOT_EMPTY or VERR_RESOURCE_BUSY",  g_szDir, rc);

    APIRET orc;
    RTTESTI_CHECK_MSG((orc = DosDelete((PCSZ)InEmptyDir(RT_STR_TUPLE(".")))) == ERROR_ACCESS_DENIED,
                      ("DosDelete(%s) -> %u, expected %u\n", g_szEmptyDir, orc, ERROR_ACCESS_DENIED));
    RTTESTI_CHECK_MSG((orc = DosDelete((PCSZ)InEmptyDir(RT_STR_TUPLE("..")))) == ERROR_ACCESS_DENIED,
                      ("DosDelete(%s) -> %u, expected %u\n", g_szEmptyDir, orc, ERROR_ACCESS_DENIED));
    RTTESTI_CHECK_MSG((orc = DosDelete((PCSZ)InEmptyDir(RT_STR_TUPLE("")))) == ERROR_PATH_NOT_FOUND, /* a little weird (fsrouter) */
                      ("DosDelete(%s) -> %u, expected %u\n", g_szEmptyDir, orc, ERROR_PATH_NOT_FOUND));

# endif
#else
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE(".."))), VERR_DIR_NOT_EMPTY);
#endif
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE(""))), VERR_DIR_NOT_EMPTY);

    /* Create a directory and remove it: */
    RTTESTI_CHECK_RC(RTDirCreate(InDir(RT_STR_TUPLE("subdir-1")), 0755, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTDirRemove(g_szDir), VINF_SUCCESS);

    /* Create a file and try remove it or create a directory with the same name: */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file18")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTDirRemove(g_szDir), VERR_NOT_A_DIRECTORY);
    RTTESTI_CHECK_RC(RTDirCreate(g_szDir, 0755, 0), VERR_ALREADY_EXISTS);
    RTTESTI_CHECK_RC(RTDirCreate(InDir(RT_STR_TUPLE("file18" RTPATH_SLASH_STR "subdir")), 0755, 0), VERR_PATH_NOT_FOUND);

    /*
     * Profile alternately creating and removing a bunch of directories.
     */
    RTTESTI_CHECK_RC_RETV(RTDirCreate(InDir(RT_STR_TUPLE("subdir-2")), 0755, 0), VINF_SUCCESS);
    size_t cchDir = strlen(g_szDir);
    g_szDir[cchDir++] = RTPATH_SLASH;
    g_szDir[cchDir++] = 's';

    uint32_t cCreated = 0;
    uint64_t nsCreate = 0;
    uint64_t nsRemove = 0;
    for (;;)
    {
        /* Create a bunch: */
        uint64_t nsStart = RTTimeNanoTS();
        for (uint32_t i = 0; i < 998; i++)
        {
            RTStrFormatU32(&g_szDir[cchDir], sizeof(g_szDir) - cchDir, i, 10, 3, 3, RTSTR_F_ZEROPAD);
            RTTESTI_CHECK_RC_RETV(RTDirCreate(g_szDir, 0755, 0), VINF_SUCCESS);
        }
        nsCreate += RTTimeNanoTS() - nsStart;
        cCreated += 998;

        /* Remove the bunch: */
        nsStart = RTTimeNanoTS();
        for (uint32_t i = 0; i < 998; i++)
        {
            RTStrFormatU32(&g_szDir[cchDir], sizeof(g_szDir) - cchDir, i, 10, 3, 3, RTSTR_F_ZEROPAD);
            RTTESTI_CHECK_RC_RETV(RTDirRemove(g_szDir), VINF_SUCCESS);
        }
        nsRemove = RTTimeNanoTS() - nsStart;

        /* Check if we got time for another round: */
        if (   (   nsRemove >= g_nsTestRun
                && nsCreate >= g_nsTestRun)
            || nsCreate + nsRemove >= g_nsTestRun * 3)
            break;
    }
    RTTestIValue("RTDirCreate", nsCreate / cCreated, RTTESTUNIT_NS_PER_OCCURRENCE);
    RTTestIValue("RTDirRemove", nsRemove / cCreated, RTTESTUNIT_NS_PER_OCCURRENCE);
}


void fsPerfStatVfs(void)
{
    RTTestISub("statvfs");

    g_szEmptyDir[g_cchEmptyDir] = '\0';
    RTFOFF   cbTotal;
    RTFOFF   cbFree;
    uint32_t cbBlock;
    uint32_t cbSector;
    RTTESTI_CHECK_RC(RTFsQuerySizes(g_szEmptyDir, &cbTotal, &cbFree, &cbBlock, &cbSector), VINF_SUCCESS);

    uint32_t uSerial;
    RTTESTI_CHECK_RC(RTFsQuerySerial(g_szEmptyDir, &uSerial), VINF_SUCCESS);

    RTFSPROPERTIES Props;
    RTTESTI_CHECK_RC(RTFsQueryProperties(g_szEmptyDir, &Props), VINF_SUCCESS);

    RTFSTYPE enmType;
    RTTESTI_CHECK_RC(RTFsQueryType(g_szEmptyDir, &enmType), VINF_SUCCESS);

    g_szDeepDir[g_cchDeepDir] = '\0';
    PROFILE_FN(RTFsQuerySizes(g_szEmptyDir, &cbTotal, &cbFree, &cbBlock, &cbSector), g_nsTestRun, "RTFsQuerySize/empty");
    PROFILE_FN(RTFsQuerySizes(g_szDeepDir, &cbTotal, &cbFree, &cbBlock, &cbSector), g_nsTestRun, "RTFsQuerySize/deep");
}


void fsPerfRm(void)
{
    RTTestISub("rm");

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("no-such-file"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileDelete(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);

    /* Directories: */
#if defined(RT_OS_WINDOWS)
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("."))),  VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(".."))), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(""))),   VERR_ACCESS_DENIED);
#elif defined(RT_OS_DARWIN) /* unlink() on xnu 16.7.0 is behaviour totally werid: */
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("."))),  VERR_INVALID_PARAMETER);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(".."))), VINF_SUCCESS /*WTF?!?*/);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(""))),   VERR_ACCESS_DENIED);
#elif defined(RT_OS_OS2) /* OS/2 has a busted unlink, it think it should remove directories too. */
    RTTESTI_CHECK_RC(RTFileDelete(InDir(RT_STR_TUPLE("."))),       VERR_DIR_NOT_EMPTY);
    int rc = RTFileDelete(InDir(RT_STR_TUPLE("..")));
    if (rc != VERR_DIR_NOT_EMPTY && rc != VERR_FILE_NOT_FOUND && rc != VERR_RESOURCE_BUSY)
        RTTestIFailed("RTFileDelete(%s) -> %Rrc, expected VERR_DIR_NOT_EMPTY or VERR_FILE_NOT_FOUND or VERR_RESOURCE_BUSY", g_szDir, rc);
    RTTESTI_CHECK_RC(RTFileDelete(InDir(RT_STR_TUPLE(""))),        VERR_DIR_NOT_EMPTY);
    APIRET orc;
    RTTESTI_CHECK_MSG((orc = DosDelete((PCSZ)InEmptyDir(RT_STR_TUPLE(".")))) == ERROR_ACCESS_DENIED,
                      ("DosDelete(%s) -> %u, expected %u\n", g_szEmptyDir, orc, ERROR_ACCESS_DENIED));
    RTTESTI_CHECK_MSG((orc = DosDelete((PCSZ)InEmptyDir(RT_STR_TUPLE("..")))) == ERROR_ACCESS_DENIED,
                      ("DosDelete(%s) -> %u, expected %u\n", g_szEmptyDir, orc, ERROR_ACCESS_DENIED));
    RTTESTI_CHECK_MSG((orc = DosDelete((PCSZ)InEmptyDir(RT_STR_TUPLE("")))) == ERROR_PATH_NOT_FOUND,
                      ("DosDelete(%s) -> %u, expected %u\n", g_szEmptyDir, orc, ERROR_PATH_NOT_FOUND)); /* hpfs+jfs; weird.  */

#else
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("."))),  VERR_IS_A_DIRECTORY);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(".."))), VERR_IS_A_DIRECTORY);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(""))),   VERR_IS_A_DIRECTORY);
#endif

    /* Shallow: */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file19")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VERR_FILE_NOT_FOUND);

    if (g_fManyFiles)
    {
        /*
         * Profile the deletion of the manyfiles content.
         */
        {
            InDir(RT_STR_TUPLE("manyfiles" RTPATH_SLASH_STR));
            size_t const offFilename = strlen(g_szDir);
            fsPerfYield();
            uint64_t const nsStart = RTTimeNanoTS();
            for (uint32_t i = 0; i < g_cManyFiles; i++)
            {
                RTStrFormatU32(&g_szDir[offFilename], sizeof(g_szDir) - offFilename, i, 10, 5, 5, RTSTR_F_ZEROPAD);
                RTTESTI_CHECK_RC_RETV(RTFileDelete(g_szDir), VINF_SUCCESS);
            }
            uint64_t const cNsElapsed = RTTimeNanoTS() - nsStart;
            RTTestIValueF(cNsElapsed, RTTESTUNIT_NS, "Deleted %u empty files from a single directory", g_cManyFiles);
            RTTestIValueF(cNsElapsed / g_cManyFiles, RTTESTUNIT_NS_PER_OCCURRENCE, "Delete file (single dir)");
        }

        /*
         * Ditto for the manytree.
         */
        {
            char szPath[RTPATH_MAX];
            uint64_t const nsStart = RTTimeNanoTS();
            DO_MANYTREE_FN(szPath, RTTESTI_CHECK_RC_RETV(RTFileDelete(szPath), VINF_SUCCESS));
            uint64_t const cNsElapsed = RTTimeNanoTS() - nsStart;
            RTTestIValueF(cNsElapsed, RTTESTUNIT_NS, "Deleted %u empty files in tree", g_cManyTreeFiles);
            RTTestIValueF(cNsElapsed / g_cManyTreeFiles, RTTESTUNIT_NS_PER_OCCURRENCE, "Delete file (tree)");
        }
    }
}


void fsPerfChSize(void)
{
    RTTestISub("chsize");

    /*
     * We need some free space to perform this test.
     */
    g_szDir[g_cchDir] = '\0';
    RTFOFF cbFree = 0;
    RTTESTI_CHECK_RC_RETV(RTFsQuerySizes(g_szDir, NULL, &cbFree, NULL, NULL), VINF_SUCCESS);
    if (cbFree < _1M)
    {
        RTTestSkipped(g_hTest, "Insufficent free space: %'RU64 bytes, requires >= 1MB", cbFree);
        return;
    }

    /*
     * Create a file and play around with it's size.
     * We let the current file position follow the end position as we make changes.
     */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file20")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE), VINF_SUCCESS);
    uint64_t cbFile = UINT64_MAX;
    RTTESTI_CHECK_RC(RTFileGetSize(hFile1, &cbFile), VINF_SUCCESS);
    RTTESTI_CHECK(cbFile == 0);

    uint8_t abBuf[4096];
    static uint64_t const s_acbChanges[] =
    {
        1023, 1024, 1024, 1025, 8192, 11111, _1M, _8M, _8M,
        _4M, _2M + 1, _1M - 1, 65537, 65536, 32768, 8000, 7999, 7998, 1024, 1, 0
    };
    uint64_t cbOld = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(s_acbChanges); i++)
    {
        uint64_t cbNew = s_acbChanges[i];
        if (cbNew + _64K >= (uint64_t)cbFree)
            continue;

        RTTESTI_CHECK_RC(RTFileSetSize(hFile1, cbNew), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileGetSize(hFile1, &cbFile), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(cbFile == cbNew, ("cbFile=%#RX64 cbNew=%#RX64\n", cbFile, cbNew));

        if (cbNew > cbOld)
        {
            /* Check that the extension is all zeroed: */
            uint64_t cbLeft = cbNew - cbOld;
            while (cbLeft > 0)
            {
                memset(abBuf, 0xff, sizeof(abBuf));
                size_t cbToRead = sizeof(abBuf);
                if (cbToRead > cbLeft)
                    cbToRead = (size_t)cbLeft;
                RTTESTI_CHECK_RC(RTFileRead(hFile1, abBuf, cbToRead, NULL), VINF_SUCCESS);
                RTTESTI_CHECK(ASMMemIsZero(abBuf, cbToRead));
                cbLeft -= cbToRead;
            }
        }
        else
        {
            /* Check that reading fails with EOF because current position is now beyond the end: */
            RTTESTI_CHECK_RC(RTFileRead(hFile1, abBuf, 1, NULL), VERR_EOF);

            /* Keep current position at the end of the file: */
            RTTESTI_CHECK_RC(RTFileSeek(hFile1, cbNew, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
        }
        cbOld = cbNew;
    }

    /*
     * Profile just the file setting operation itself, keeping the changes within
     * an allocation unit to avoid needing to adjust the actual (host) FS allocation.
     * ASSUMES allocation unit >= 512 and power of two.
     */
    RTTESTI_CHECK_RC(RTFileSetSize(hFile1, _64K), VINF_SUCCESS);
    PROFILE_FN(RTFileSetSize(hFile1, _64K - (iIteration & 255) - 128), g_nsTestRun, "RTFileSetSize/noalloc");

    RTTESTI_CHECK_RC(RTFileSetSize(hFile1, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VINF_SUCCESS);
}


int fsPerfIoPrepFile(RTFILE hFile1, uint64_t cbFile, uint8_t **ppbFree)
{
    /*
     * Seek to the end - 4K and write the last 4K.
     * This should have the effect of filling the whole file with zeros.
     */
    RTTESTI_CHECK_RC_RET(RTFileSeek(hFile1, cbFile - _4K, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC_RET(RTFileWrite(hFile1, g_abRTZero4K, _4K, NULL), VINF_SUCCESS, rcCheck);

    /*
     * Check that the space we searched across actually is zero filled.
     */
    RTTESTI_CHECK_RC_RET(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS, rcCheck);
    size_t   cbBuf = _1M;
    uint8_t *pbBuf = *ppbFree = (uint8_t *)RTMemAlloc(cbBuf);
    RTTESTI_CHECK_RET(pbBuf != NULL, VERR_NO_MEMORY);
    uint64_t cbLeft = cbFile;
    while (cbLeft > 0)
    {
        size_t cbToRead = cbBuf;
        if (cbToRead > cbLeft)
            cbToRead = (size_t)cbLeft;
        pbBuf[cbToRead] = 0xff;

        RTTESTI_CHECK_RC_RET(RTFileRead(hFile1, pbBuf, cbToRead, NULL), VINF_SUCCESS, rcCheck);
        RTTESTI_CHECK_RET(ASMMemIsZero(pbBuf, cbToRead), VERR_MISMATCH);

        cbLeft -= cbToRead;
    }

    /*
     * Fill the file with 0xf6 and insert offset markers with 1KB intervals.
     */
    RTTESTI_CHECK_RC_RET(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS, rcCheck);
    memset(pbBuf, 0xf6, cbBuf);
    cbLeft = cbFile;
    uint64_t off = 0;
    while (cbLeft > 0)
    {
        Assert(!(off   & (_1K - 1)));
        Assert(!(cbBuf & (_1K - 1)));
        for (size_t offBuf = 0; offBuf < cbBuf; offBuf += _1K, off += _1K)
            *(uint64_t *)&pbBuf[offBuf] = off;

        size_t cbToWrite = cbBuf;
        if (cbToWrite > cbLeft)
            cbToWrite = (size_t)cbLeft;

        RTTESTI_CHECK_RC_RET(RTFileWrite(hFile1, pbBuf, cbToWrite, NULL), VINF_SUCCESS, rcCheck);

        cbLeft -= cbToWrite;
    }

    return VINF_SUCCESS;
}


/**
 * Checks the content read from the file fsPerfIoPrepFile() prepared.
 */
bool fsPerfCheckReadBuf(unsigned uLineNo, uint64_t off, uint8_t const *pbBuf, size_t cbBuf, uint8_t bFiller = 0xf6)
{
    uint32_t cMismatches = 0;
    size_t   offBuf      = 0;
    uint32_t offBlock    = (uint32_t)(off & (_1K - 1));
    while (offBuf < cbBuf)
    {
        /*
         * Check the offset marker:
         */
        if (offBlock < sizeof(uint64_t))
        {
            RTUINT64U uMarker;
            uMarker.u = off + offBuf - offBlock;
            unsigned offMarker = offBlock & (sizeof(uint64_t) - 1);
            while (offMarker < sizeof(uint64_t) && offBuf < cbBuf)
            {
                if (uMarker.au8[offMarker] != pbBuf[offBuf])
                {
                    RTTestIFailed("%u: Mismatch at buffer/file offset %#zx/%#RX64: %#x, expected %#x",
                                  uLineNo, offBuf, off + offBuf, pbBuf[offBuf], uMarker.au8[offMarker]);
                    if (cMismatches++ > 32)
                        return false;
                }
                offMarker++;
                offBuf++;
            }
            offBlock = sizeof(uint64_t);
        }

        /*
         * Check the filling:
         */
        size_t cbFilling = RT_MIN(_1K - offBlock, cbBuf - offBuf);
        if (   cbFilling == 0
            || ASMMemIsAllU8(&pbBuf[offBuf], cbFilling, bFiller))
            offBuf += cbFilling;
        else
        {
            /* Some mismatch, locate it/them: */
            while (cbFilling > 0 && offBuf < cbBuf)
            {
                if (pbBuf[offBuf] != bFiller)
                {
                    RTTestIFailed("%u: Mismatch at buffer/file offset %#zx/%#RX64: %#x, expected %#04x",
                                  uLineNo, offBuf, off + offBuf, pbBuf[offBuf], bFiller);
                    if (cMismatches++ > 32)
                        return false;
                }
                offBuf++;
                cbFilling--;
            }
        }
        offBlock = 0;
    }
    return cMismatches == 0;
}


/**
 * Sets up write buffer with offset markers and fillers.
 */
void fsPerfFillWriteBuf(uint64_t off, uint8_t *pbBuf, size_t cbBuf, uint8_t bFiller = 0xf6)
{
    uint32_t offBlock = (uint32_t)(off & (_1K - 1));
    while (cbBuf > 0)
    {
        /* The marker. */
        if (offBlock < sizeof(uint64_t))
        {
            RTUINT64U uMarker;
            uMarker.u = off + offBlock;
            if (cbBuf > sizeof(uMarker) - offBlock)
            {
                memcpy(pbBuf, &uMarker.au8[offBlock], sizeof(uMarker) - offBlock);
                pbBuf += sizeof(uMarker) - offBlock;
                cbBuf -= sizeof(uMarker) - offBlock;
                off   += sizeof(uMarker) - offBlock;
            }
            else
            {
                memcpy(pbBuf, &uMarker.au8[offBlock], cbBuf);
                return;
            }
            offBlock = sizeof(uint64_t);
        }

        /* Do the filling. */
        size_t cbFilling = RT_MIN(_1K - offBlock, cbBuf);
        memset(pbBuf, bFiller, cbFilling);
        pbBuf += cbFilling;
        cbBuf -= cbFilling;
        off   += cbFilling;

        offBlock = 0;
    }
}



void fsPerfIoSeek(RTFILE hFile1, uint64_t cbFile)
{
    /*
     * Do a bunch of search tests, most which are random.
     */
    struct
    {
        int         rc;
        uint32_t    uMethod;
        int64_t     offSeek;
        uint64_t    offActual;

    } aSeeks[9 + 64] =
    {
        { VINF_SUCCESS,         RTFILE_SEEK_BEGIN,    0,                        0 },
        { VINF_SUCCESS,         RTFILE_SEEK_CURRENT,  0,                        0 },
        { VINF_SUCCESS,         RTFILE_SEEK_END,      0,                        cbFile },
        { VINF_SUCCESS,         RTFILE_SEEK_CURRENT,  -4096,                    cbFile - 4096 },
        { VINF_SUCCESS,         RTFILE_SEEK_CURRENT,  4096 - (int64_t)cbFile,   0 },
        { VINF_SUCCESS,         RTFILE_SEEK_END,      -(int64_t)cbFile/2,       cbFile / 2 + (cbFile & 1) },
        { VINF_SUCCESS,         RTFILE_SEEK_CURRENT,  -(int64_t)cbFile/2,       0 },
#if defined(RT_OS_WINDOWS)
        { VERR_NEGATIVE_SEEK,   RTFILE_SEEK_CURRENT,  -1,                       0 },
#else
        { VERR_INVALID_PARAMETER, RTFILE_SEEK_CURRENT, -1,                      0 },
#endif
        { VINF_SUCCESS,         RTFILE_SEEK_CURRENT,  0,                        0 },
    };

    uint64_t offActual = 0;
    for (unsigned i = 9; i < RT_ELEMENTS(aSeeks); i++)
    {
        switch (RTRandU32Ex(RTFILE_SEEK_BEGIN, RTFILE_SEEK_END))
        {
            default: AssertFailedBreak();
            case RTFILE_SEEK_BEGIN:
                aSeeks[i].uMethod   = RTFILE_SEEK_BEGIN;
                aSeeks[i].rc        = VINF_SUCCESS;
                aSeeks[i].offSeek   = RTRandU64Ex(0, cbFile + cbFile / 8);
                aSeeks[i].offActual = offActual = aSeeks[i].offSeek;
                break;

            case RTFILE_SEEK_CURRENT:
                aSeeks[i].uMethod   = RTFILE_SEEK_CURRENT;
                aSeeks[i].rc        = VINF_SUCCESS;
                aSeeks[i].offSeek   = (int64_t)RTRandU64Ex(0, cbFile + cbFile / 8) - (int64_t)offActual;
                aSeeks[i].offActual = offActual += aSeeks[i].offSeek;
                break;

            case RTFILE_SEEK_END:
                aSeeks[i].uMethod   = RTFILE_SEEK_END;
                aSeeks[i].rc        = VINF_SUCCESS;
                aSeeks[i].offSeek   = -(int64_t)RTRandU64Ex(0, cbFile);
                aSeeks[i].offActual = offActual = cbFile + aSeeks[i].offSeek;
                break;
        }
    }

    for (unsigned iDoReadCheck = 0; iDoReadCheck < 2; iDoReadCheck++)
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(aSeeks); i++)
        {
            offActual = UINT64_MAX;
            int rc = RTFileSeek(hFile1, aSeeks[i].offSeek, aSeeks[i].uMethod, &offActual);
            if (rc != aSeeks[i].rc)
                RTTestIFailed("Seek #%u: Expected %Rrc, got %Rrc", i, aSeeks[i].rc, rc);
            if (RT_SUCCESS(rc) && offActual != aSeeks[i].offActual)
                RTTestIFailed("Seek #%u: offActual %#RX64, expected %#RX64", i, offActual, aSeeks[i].offActual);
            if (RT_SUCCESS(rc))
            {
                uint64_t offTell = RTFileTell(hFile1);
                if (offTell != offActual)
                    RTTestIFailed("Seek #%u: offActual %#RX64, RTFileTell %#RX64", i, offActual, offTell);
            }

            if (RT_SUCCESS(rc) && offActual + _2K <= cbFile && iDoReadCheck)
            {
                uint8_t abBuf[_2K];
                RTTESTI_CHECK_RC(rc = RTFileRead(hFile1, abBuf, sizeof(abBuf), NULL), VINF_SUCCESS);
                if (RT_SUCCESS(rc))
                {
                    size_t offMarker = (size_t)(RT_ALIGN_64(offActual, _1K) - offActual);
                    uint64_t uMarker = *(uint64_t *)&abBuf[offMarker]; /** @todo potentially unaligned access */
                    if (uMarker != offActual + offMarker)
                        RTTestIFailed("Seek #%u: Invalid marker value (@ %#RX64): %#RX64, expected %#RX64",
                                      i, offActual, uMarker, offActual + offMarker);

                    RTTESTI_CHECK_RC(RTFileSeek(hFile1, -(int64_t)sizeof(abBuf), RTFILE_SEEK_CURRENT, NULL), VINF_SUCCESS);
                }
            }
        }
    }


    /*
     * Profile seeking relative to the beginning of the file and relative
     * to the end.  The latter might be more expensive in a SF context.
     */
    PROFILE_FN(RTFileSeek(hFile1, iIteration < cbFile ? iIteration : iIteration % cbFile, RTFILE_SEEK_BEGIN, NULL),
               g_nsTestRun, "RTFileSeek/BEGIN");
    PROFILE_FN(RTFileSeek(hFile1, iIteration < cbFile ? -(int64_t)iIteration : -(int64_t)(iIteration % cbFile), RTFILE_SEEK_END, NULL),
               g_nsTestRun, "RTFileSeek/END");

}

#ifdef FSPERF_TEST_SENDFILE

/**
 * Send file thread arguments.
 */
typedef struct FSPERFSENDFILEARGS
{
    uint64_t            offFile;
    size_t              cbSend;
    uint64_t            cbSent;
    size_t              cbBuf;
    uint8_t            *pbBuf;
    uint8_t             bFiller;
    bool                fCheckBuf;
    RTSOCKET            hSocket;
    uint64_t volatile   tsThreadDone;
} FSPERFSENDFILEARGS;

/** Thread receiving the bytes from a sendfile() call. */
static DECLCALLBACK(int) fsPerfSendFileThread(RTTHREAD hSelf, void *pvUser)
{
    FSPERFSENDFILEARGS *pArgs = (FSPERFSENDFILEARGS *)pvUser;
    int                 rc    = VINF_SUCCESS;

    if (pArgs->fCheckBuf)
        RTTestSetDefault(g_hTest, NULL);

    uint64_t cbReceived = 0;
    while (cbReceived < pArgs->cbSent)
    {
        size_t const cbToRead = RT_MIN(pArgs->cbBuf, pArgs->cbSent - cbReceived);
        size_t       cbActual = 0;
        RTTEST_CHECK_RC_BREAK(g_hTest, rc = RTTcpRead(pArgs->hSocket, pArgs->pbBuf, cbToRead, &cbActual), VINF_SUCCESS);
        RTTEST_CHECK_BREAK(g_hTest, cbActual != 0);
        RTTEST_CHECK(g_hTest, cbActual <= cbToRead);
        if (pArgs->fCheckBuf)
            fsPerfCheckReadBuf(__LINE__, pArgs->offFile + cbReceived, pArgs->pbBuf, cbActual, pArgs->bFiller);
        cbReceived += cbActual;
    }

    pArgs->tsThreadDone = RTTimeNanoTS();

    if (cbReceived == pArgs->cbSent && RT_SUCCESS(rc))
    {
        size_t cbActual = 0;
        rc = RTSocketReadNB(pArgs->hSocket, pArgs->pbBuf, 1, &cbActual);
        if (rc != VINF_SUCCESS && rc != VINF_TRY_AGAIN)
            RTTestFailed(g_hTest, "RTSocketReadNB(sendfile client socket) -> %Rrc; expected VINF_SUCCESS or VINF_TRY_AGAIN\n", rc);
        else if (cbActual != 0)
            RTTestFailed(g_hTest, "sendfile client socket still contains data when done!\n");
    }

    RTTEST_CHECK_RC(g_hTest, RTSocketClose(pArgs->hSocket), VINF_SUCCESS);
    pArgs->hSocket = NIL_RTSOCKET;

    RT_NOREF(hSelf);
    return rc;
}


static uint64_t fsPerfSendFileOne(FSPERFSENDFILEARGS *pArgs, RTFILE hFile1, uint64_t offFile,
                                  size_t cbSend, uint64_t cbSent, uint8_t bFiller, bool fCheckBuf, unsigned iLine)
{
    /* Copy parameters to the argument structure: */
    pArgs->offFile   = offFile;
    pArgs->cbSend    = cbSend;
    pArgs->cbSent    = cbSent;
    pArgs->bFiller   = bFiller;
    pArgs->fCheckBuf = fCheckBuf;

    /* Create a socket pair. */
    pArgs->hSocket   = NIL_RTSOCKET;
    RTSOCKET hServer = NIL_RTSOCKET;
    RTTESTI_CHECK_RC_RET(RTTcpCreatePair(&hServer, &pArgs->hSocket, 0), VINF_SUCCESS, 0);

    /* Create the receiving thread: */
    int rc;
    RTTHREAD hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(rc = RTThreadCreate(&hThread, fsPerfSendFileThread, pArgs, 0,
                                         RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "sendfile"), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        uint64_t const tsStart = RTTimeNanoTS();

# if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
        /* SystemV sendfile: */
        loff_t offFileSf = pArgs->offFile;
        ssize_t cbActual = sendfile((int)RTSocketToNative(hServer), (int)RTFileToNative(hFile1), &offFileSf, pArgs->cbSend);
        int const iErr = errno;
        if (cbActual < 0)
            RTTestIFailed("%u: sendfile(socket, file, &%#X64, %#zx) failed (%zd): %d (%Rrc), offFileSf=%#RX64\n",
                          iLine, pArgs->offFile, pArgs->cbSend, cbActual, iErr, RTErrConvertFromErrno(iErr), (uint64_t)offFileSf);
        else if ((uint64_t)cbActual != pArgs->cbSent)
            RTTestIFailed("%u: sendfile(socket, file, &%#RX64, %#zx): %#zx, expected %#RX64 (offFileSf=%#RX64)\n",
                          iLine, pArgs->offFile, pArgs->cbSend, cbActual, pArgs->cbSent, (uint64_t)offFileSf);
        else if ((uint64_t)offFileSf != pArgs->offFile + pArgs->cbSent)
            RTTestIFailed("%u: sendfile(socket, file, &%#RX64, %#zx): %#zx; offFileSf=%#RX64, expected %#RX64\n",
                          iLine, pArgs->offFile, pArgs->cbSend, cbActual, (uint64_t)offFileSf, pArgs->offFile + pArgs->cbSent);
#else
        /* BSD sendfile: */
#  ifdef SF_SYNC
        int fSfFlags = SF_SYNC;
#  else
        int fSfFlags = 0;
#  endif
        off_t cbActual = pArgs->cbSend;
        rc = sendfile((int)RTFileToNative(hFile1), (int)RTSocketToNative(hServer),
#  ifdef RT_OS_DARWIN
                      pArgs->offFile, &cbActual, NULL, fSfFlags);
#  else
                      pArgs->offFile, cbActual, NULL,  &cbActual, fSfFlags);
#  endif
        int const iErr = errno;
        if (rc != 0)
            RTTestIFailed("%u: sendfile(file, socket, %#RX64, %#zx, NULL,, %#x) failed (%d): %d (%Rrc), cbActual=%#RX64\n",
                          iLine, pArgs->offFile, (size_t)pArgs->cbSend, rc, iErr, RTErrConvertFromErrno(iErr), (uint64_t)cbActual);
        if ((uint64_t)cbActual != pArgs->cbSent)
            RTTestIFailed("%u: sendfile(file, socket, %#RX64, %#zx, NULL,, %#x): cbActual=%#RX64, expected %#RX64 (rc=%d, errno=%d)\n",
                          iLine, pArgs->offFile, (size_t)pArgs->cbSend, (uint64_t)cbActual, pArgs->cbSent, rc, iErr);
# endif
        RTTESTI_CHECK_RC(RTSocketClose(hServer), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTThreadWait(hThread, 30 * RT_NS_1SEC, NULL), VINF_SUCCESS);

        if (pArgs->tsThreadDone >= tsStart)
            return RT_MAX(pArgs->tsThreadDone - tsStart, 1);
    }
    return 0;
}


static void fsPerfSendFile(RTFILE hFile1, uint64_t cbFile)
{
    RTTestISub("sendfile");
# ifdef RT_OS_LINUX
    uint64_t const cbFileMax = RT_MIN(cbFile, UINT32_MAX - PAGE_OFFSET_MASK);
# else
    uint64_t const cbFileMax = RT_MIN(cbFile, SSIZE_MAX - PAGE_OFFSET_MASK);
# endif
    signal(SIGPIPE, SIG_IGN);

    /*
     * Allocate a buffer.
     */
    FSPERFSENDFILEARGS Args;
    Args.cbBuf = RT_MIN(cbFileMax, _16M);
    Args.pbBuf = (uint8_t *)RTMemAlloc(Args.cbBuf);
    while (!Args.pbBuf)
    {
        Args.cbBuf /= 8;
        RTTESTI_CHECK_RETV(Args.cbBuf >= _64K);
        Args.pbBuf = (uint8_t *)RTMemAlloc(Args.cbBuf);
    }

    /*
     * First iteration with default buffer content.
     */
    fsPerfSendFileOne(&Args, hFile1, 0, cbFileMax, cbFileMax, 0xf6, true /*fCheckBuf*/, __LINE__);
    if (cbFileMax == cbFile)
        fsPerfSendFileOne(&Args, hFile1, 63, cbFileMax, cbFileMax - 63, 0xf6, true /*fCheckBuf*/, __LINE__);
    else
        fsPerfSendFileOne(&Args, hFile1, 63, cbFileMax - 63, cbFileMax - 63, 0xf6, true /*fCheckBuf*/, __LINE__);

    /*
     * Write a block using the regular API and then send it, checking that
     * the any caching that sendfile does is correctly updated.
     */
    uint8_t bFiller = 0xf6;
    size_t cbToSend = RT_MIN(cbFileMax, Args.cbBuf);
    do
    {
        fsPerfSendFileOne(&Args, hFile1, 0, cbToSend, cbToSend, bFiller, true /*fCheckBuf*/, __LINE__); /* prime cache */

        bFiller += 1;
        fsPerfFillWriteBuf(0, Args.pbBuf, cbToSend, bFiller);
        RTTESTI_CHECK_RC(RTFileWriteAt(hFile1, 0, Args.pbBuf, cbToSend, NULL), VINF_SUCCESS);

        fsPerfSendFileOne(&Args, hFile1, 0, cbToSend, cbToSend, bFiller, true /*fCheckBuf*/, __LINE__);

        cbToSend /= 2;
    } while (cbToSend >= PAGE_SIZE && ((unsigned)bFiller - 0xf7U) < 64);

    /*
     * Restore buffer content
     */
    bFiller = 0xf6;
    fsPerfFillWriteBuf(0, Args.pbBuf, Args.cbBuf, bFiller);
    RTTESTI_CHECK_RC(RTFileWriteAt(hFile1, 0, Args.pbBuf, Args.cbBuf, NULL), VINF_SUCCESS);

    /*
     * Do 128 random sends.
     */
    uint64_t const cbSmall = RT_MIN(_256K, cbFileMax / 16);
    for (uint32_t iTest = 0; iTest < 128; iTest++)
    {
        cbToSend                     = (size_t)RTRandU64Ex(1, iTest < 64 ? cbSmall : cbFileMax);
        uint64_t const offToSendFrom = RTRandU64Ex(0, cbFile - 1);
        uint64_t const cbSent        = offToSendFrom + cbToSend <= cbFile ? cbToSend : cbFile - offToSendFrom;

        fsPerfSendFileOne(&Args, hFile1, offToSendFrom, cbToSend, cbSent, bFiller, true /*fCheckBuf*/, __LINE__);
    }

    /*
     * Benchmark it.
     */
    uint32_t cIterations = 0;
    uint64_t nsElapsed   = 0;
    for (;;)
    {
        uint64_t cNsThis = fsPerfSendFileOne(&Args, hFile1, 0, cbFileMax, cbFileMax, 0xf6, false /*fCheckBuf*/, __LINE__);
        nsElapsed += cNsThis;
        cIterations++;
        if (!cNsThis || nsElapsed >= g_nsTestRun)
            break;
    }
    uint64_t cbTotal = cbFileMax * cIterations;
    RTTestIValue("latency",    nsElapsed / cIterations,                                 RTTESTUNIT_NS_PER_CALL);
    RTTestIValue("throughput", (uint64_t)(cbTotal / ((double)nsElapsed / RT_NS_1SEC)),  RTTESTUNIT_BYTES_PER_SEC);
    RTTestIValue("calls",      cIterations,                                             RTTESTUNIT_CALLS);
    RTTestIValue("bytes",      cbTotal,                                                 RTTESTUNIT_BYTES);
    if (g_fShowDuration)
        RTTestIValue("duration", nsElapsed,                                             RTTESTUNIT_NS);

    /*
     * Cleanup.
     */
    RTMemFree(Args.pbBuf);
}

#endif /* FSPERF_TEST_SENDFILE */
#ifdef RT_OS_LINUX

/**
 * Send file thread arguments.
 */
typedef struct FSPERFSPLICEARGS
{
    uint64_t            offFile;
    size_t              cbSend;
    uint64_t            cbSent;
    size_t              cbBuf;
    uint8_t            *pbBuf;
    uint8_t             bFiller;
    bool                fCheckBuf;
    uint32_t            cCalls;
    RTPIPE              hPipe;
    uint64_t volatile   tsThreadDone;
} FSPERFSPLICEARGS;


/** Thread receiving the bytes from a splice() call. */
static DECLCALLBACK(int) fsPerfSpliceRecvThread(RTTHREAD hSelf, void *pvUser)
{
    FSPERFSPLICEARGS *pArgs = (FSPERFSPLICEARGS *)pvUser;
    int               rc    = VINF_SUCCESS;

    if (pArgs->fCheckBuf)
        RTTestSetDefault(g_hTest, NULL);

    uint64_t cbReceived = 0;
    while (cbReceived < pArgs->cbSent)
    {
        size_t const cbToRead = RT_MIN(pArgs->cbBuf, pArgs->cbSent - cbReceived);
        size_t       cbActual = 0;
        RTTEST_CHECK_RC_BREAK(g_hTest, rc = RTPipeReadBlocking(pArgs->hPipe, pArgs->pbBuf, cbToRead, &cbActual), VINF_SUCCESS);
        RTTEST_CHECK_BREAK(g_hTest, cbActual != 0);
        RTTEST_CHECK(g_hTest, cbActual <= cbToRead);
        if (pArgs->fCheckBuf)
            fsPerfCheckReadBuf(__LINE__, pArgs->offFile + cbReceived, pArgs->pbBuf, cbActual, pArgs->bFiller);
        cbReceived += cbActual;
    }

    pArgs->tsThreadDone = RTTimeNanoTS();

    if (cbReceived == pArgs->cbSent && RT_SUCCESS(rc))
    {
        size_t cbActual = 0;
        rc = RTPipeRead(pArgs->hPipe, pArgs->pbBuf, 1, &cbActual);
        if (rc != VINF_SUCCESS && rc != VINF_TRY_AGAIN && rc != VERR_BROKEN_PIPE)
            RTTestFailed(g_hTest, "RTPipeReadBlocking() -> %Rrc; expected VINF_SUCCESS or VINF_TRY_AGAIN\n", rc);
        else if (cbActual != 0)
            RTTestFailed(g_hTest, "splice read pipe still contains data when done!\n");
    }

    RTTEST_CHECK_RC(g_hTest, RTPipeClose(pArgs->hPipe), VINF_SUCCESS);
    pArgs->hPipe = NIL_RTPIPE;

    RT_NOREF(hSelf);
    return rc;
}


/** Sends hFile1 to a pipe via the Linux-specific splice() syscall. */
static uint64_t fsPerfSpliceSendFile(FSPERFSPLICEARGS *pArgs, RTFILE hFile1, uint64_t offFile,
                                     size_t cbSend, uint64_t cbSent, uint8_t bFiller, bool fCheckBuf, unsigned iLine)
{
    /* Copy parameters to the argument structure: */
    pArgs->offFile   = offFile;
    pArgs->cbSend    = cbSend;
    pArgs->cbSent    = cbSent;
    pArgs->bFiller   = bFiller;
    pArgs->fCheckBuf = fCheckBuf;

    /* Create a socket pair. */
    pArgs->hPipe     = NIL_RTPIPE;
    RTPIPE hPipeW    = NIL_RTPIPE;
    RTTESTI_CHECK_RC_RET(RTPipeCreate(&pArgs->hPipe, &hPipeW, 0 /*fFlags*/), VINF_SUCCESS, 0);

    /* Create the receiving thread: */
    int rc;
    RTTHREAD hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(rc = RTThreadCreate(&hThread, fsPerfSpliceRecvThread, pArgs, 0,
                                         RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "splicerecv"), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        uint64_t const  tsStart   = RTTimeNanoTS();
        size_t          cbLeft    = cbSend;
        size_t          cbTotal   = 0;
        do
        {
            loff_t offFileIn = offFile;
            ssize_t cbActual = splice((int)RTFileToNative(hFile1), &offFileIn, (int)RTPipeToNative(hPipeW), NULL,
                                      cbLeft, 0 /*fFlags*/);
            int const iErr = errno;
            if (RT_UNLIKELY(cbActual < 0))
            {
                RTTestIFailed("%u: splice(file, &%#RX64, pipe, NULL, %#zx, 0) failed (%zd): %d (%Rrc), offFileIn=%#RX64\n",
                              iLine, offFile, cbLeft, cbActual, iErr, RTErrConvertFromErrno(iErr), (uint64_t)offFileIn);
                break;
            }
            RTTESTI_CHECK_BREAK((uint64_t)cbActual <= cbLeft);
            if ((uint64_t)offFileIn != offFile + (uint64_t)cbActual)
            {
                RTTestIFailed("%u: splice(file, &%#RX64, pipe, NULL, %#zx, 0): %#zx; offFileIn=%#RX64, expected %#RX64\n",
                              iLine, offFile, cbLeft, cbActual, (uint64_t)offFileIn, offFile + (uint64_t)cbActual);
                break;
            }
            if (cbActual > 0)
            {
                pArgs->cCalls++;
                offFile += (size_t)cbActual;
                cbTotal += (size_t)cbActual;
                cbLeft  -= (size_t)cbActual;
            }
            else
                break;
        } while (cbLeft > 0);

        if (cbTotal != pArgs->cbSent)
            RTTestIFailed("%u: spliced a total of %#zx bytes, expected %#zx!\n", iLine, cbTotal, pArgs->cbSent);

        RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTThreadWait(hThread, 30 * RT_NS_1SEC, NULL), VINF_SUCCESS);

        if (pArgs->tsThreadDone >= tsStart)
            return RT_MAX(pArgs->tsThreadDone - tsStart, 1);
    }
    return 0;
}


static void fsPerfSpliceToPipe(RTFILE hFile1, uint64_t cbFile)
{
    RTTestISub("splice/to-pipe");

    /*
     * splice was introduced in 2.6.17 according to the man-page.
     */
    char szRelease[64];
    RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szRelease, sizeof(szRelease));
    if (RTStrVersionCompare(szRelease, "2.6.17") < 0)
    {
        RTTestPassed(g_hTest, "too old kernel (%s)", szRelease);
        return;
    }

    uint64_t const cbFileMax = RT_MIN(cbFile, UINT32_MAX - PAGE_OFFSET_MASK);
    signal(SIGPIPE, SIG_IGN);

    /*
     * Allocate a buffer.
     */
    FSPERFSPLICEARGS Args;
    Args.cbBuf = RT_MIN(cbFileMax, _16M);
    Args.pbBuf = (uint8_t *)RTMemAlloc(Args.cbBuf);
    while (!Args.pbBuf)
    {
        Args.cbBuf /= 8;
        RTTESTI_CHECK_RETV(Args.cbBuf >= _64K);
        Args.pbBuf = (uint8_t *)RTMemAlloc(Args.cbBuf);
    }

    /*
     * First iteration with default buffer content.
     */
    fsPerfSpliceSendFile(&Args, hFile1, 0, cbFileMax, cbFileMax, 0xf6, true /*fCheckBuf*/, __LINE__);
    if (cbFileMax == cbFile)
        fsPerfSpliceSendFile(&Args, hFile1, 63, cbFileMax, cbFileMax - 63, 0xf6, true /*fCheckBuf*/, __LINE__);
    else
        fsPerfSpliceSendFile(&Args, hFile1, 63, cbFileMax - 63, cbFileMax - 63, 0xf6, true /*fCheckBuf*/, __LINE__);

    /*
     * Write a block using the regular API and then send it, checking that
     * the any caching that sendfile does is correctly updated.
     */
    uint8_t bFiller = 0xf6;
    size_t cbToSend = RT_MIN(cbFileMax, Args.cbBuf);
    do
    {
        fsPerfSpliceSendFile(&Args, hFile1, 0, cbToSend, cbToSend, bFiller, true /*fCheckBuf*/, __LINE__); /* prime cache */

        bFiller += 1;
        fsPerfFillWriteBuf(0, Args.pbBuf, cbToSend, bFiller);
        RTTESTI_CHECK_RC(RTFileWriteAt(hFile1, 0, Args.pbBuf, cbToSend, NULL), VINF_SUCCESS);

        fsPerfSpliceSendFile(&Args, hFile1, 0, cbToSend, cbToSend, bFiller, true /*fCheckBuf*/, __LINE__);

        cbToSend /= 2;
    } while (cbToSend >= PAGE_SIZE && ((unsigned)bFiller - 0xf7U) < 64);

    /*
     * Restore buffer content
     */
    bFiller = 0xf6;
    fsPerfFillWriteBuf(0, Args.pbBuf, Args.cbBuf, bFiller);
    RTTESTI_CHECK_RC(RTFileWriteAt(hFile1, 0, Args.pbBuf, Args.cbBuf, NULL), VINF_SUCCESS);

    /*
     * Do 128 random sends.
     */
    uint64_t const cbSmall = RT_MIN(_256K, cbFileMax / 16);
    for (uint32_t iTest = 0; iTest < 128; iTest++)
    {
        cbToSend                     = (size_t)RTRandU64Ex(1, iTest < 64 ? cbSmall : cbFileMax);
        uint64_t const offToSendFrom = RTRandU64Ex(0, cbFile - 1);
        uint64_t const cbSent        = offToSendFrom + cbToSend <= cbFile ? cbToSend : cbFile - offToSendFrom;

        fsPerfSpliceSendFile(&Args, hFile1, offToSendFrom, cbToSend, cbSent, bFiller, true /*fCheckBuf*/, __LINE__);
    }

    /*
     * Benchmark it.
     */
    Args.cCalls          = 0;
    uint32_t cIterations = 0;
    uint64_t nsElapsed   = 0;
    for (;;)
    {
        uint64_t cNsThis = fsPerfSpliceSendFile(&Args, hFile1, 0, cbFileMax, cbFileMax, 0xf6, false /*fCheckBuf*/, __LINE__);
        nsElapsed += cNsThis;
        cIterations++;
        if (!cNsThis || nsElapsed >= g_nsTestRun)
            break;
    }
    uint64_t cbTotal = cbFileMax * cIterations;
    RTTestIValue("latency",    nsElapsed / Args.cCalls,                                 RTTESTUNIT_NS_PER_CALL);
    RTTestIValue("throughput", (uint64_t)(cbTotal / ((double)nsElapsed / RT_NS_1SEC)),  RTTESTUNIT_BYTES_PER_SEC);
    RTTestIValue("calls",      Args.cCalls,                                             RTTESTUNIT_CALLS);
    RTTestIValue("bytes/call", cbTotal / Args.cCalls,                                   RTTESTUNIT_BYTES);
    RTTestIValue("iterations", cIterations,                                             RTTESTUNIT_NONE);
    RTTestIValue("bytes",      cbTotal,                                                 RTTESTUNIT_BYTES);
    if (g_fShowDuration)
        RTTestIValue("duration", nsElapsed,                                             RTTESTUNIT_NS);

    /*
     * Cleanup.
     */
    RTMemFree(Args.pbBuf);
}


/** Thread sending the bytes to a splice() call. */
static DECLCALLBACK(int) fsPerfSpliceSendThread(RTTHREAD hSelf, void *pvUser)
{
    FSPERFSPLICEARGS *pArgs = (FSPERFSPLICEARGS *)pvUser;
    int               rc    = VINF_SUCCESS;

    uint64_t offFile     = pArgs->offFile;
    uint64_t cbTotalSent = 0;
    while (cbTotalSent < pArgs->cbSent)
    {
        size_t const cbToSend = RT_MIN(pArgs->cbBuf, pArgs->cbSent - cbTotalSent);
        fsPerfFillWriteBuf(offFile, pArgs->pbBuf, cbToSend, pArgs->bFiller);
        RTTEST_CHECK_RC_BREAK(g_hTest, rc = RTPipeWriteBlocking(pArgs->hPipe, pArgs->pbBuf, cbToSend, NULL), VINF_SUCCESS);
        offFile     += cbToSend;
        cbTotalSent += cbToSend;
    }

    pArgs->tsThreadDone = RTTimeNanoTS();

    RTTEST_CHECK_RC(g_hTest, RTPipeClose(pArgs->hPipe), VINF_SUCCESS);
    pArgs->hPipe = NIL_RTPIPE;

    RT_NOREF(hSelf);
    return rc;
}


/** Fill hFile1 via a pipe and the Linux-specific splice() syscall. */
static uint64_t fsPerfSpliceWriteFile(FSPERFSPLICEARGS *pArgs, RTFILE hFile1, uint64_t offFile,
                                      size_t cbSend, uint64_t cbSent, uint8_t bFiller, bool fCheckFile, unsigned iLine)
{
    /* Copy parameters to the argument structure: */
    pArgs->offFile   = offFile;
    pArgs->cbSend    = cbSend;
    pArgs->cbSent    = cbSent;
    pArgs->bFiller   = bFiller;
    pArgs->fCheckBuf = false;

    /* Create a socket pair. */
    pArgs->hPipe     = NIL_RTPIPE;
    RTPIPE hPipeR    = NIL_RTPIPE;
    RTTESTI_CHECK_RC_RET(RTPipeCreate(&hPipeR, &pArgs->hPipe, 0 /*fFlags*/), VINF_SUCCESS, 0);

    /* Create the receiving thread: */
    int rc;
    RTTHREAD hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(rc = RTThreadCreate(&hThread, fsPerfSpliceSendThread, pArgs, 0,
                                         RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "splicerecv"), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        /*
         * Do the splicing.
         */
        uint64_t const  tsStart   = RTTimeNanoTS();
        size_t          cbLeft    = cbSend;
        size_t          cbTotal   = 0;
        do
        {
            loff_t offFileOut = offFile;
            ssize_t cbActual  = splice((int)RTPipeToNative(hPipeR), NULL, (int)RTFileToNative(hFile1), &offFileOut,
                                       cbLeft, 0 /*fFlags*/);
            int const iErr = errno;
            if (RT_UNLIKELY(cbActual < 0))
            {
                RTTestIFailed("%u: splice(pipe, NULL, file, &%#RX64, %#zx, 0) failed (%zd): %d (%Rrc), offFileOut=%#RX64\n",
                              iLine, offFile, cbLeft, cbActual, iErr, RTErrConvertFromErrno(iErr), (uint64_t)offFileOut);
                break;
            }
            RTTESTI_CHECK_BREAK((uint64_t)cbActual <= cbLeft);
            if ((uint64_t)offFileOut != offFile + (uint64_t)cbActual)
            {
                RTTestIFailed("%u: splice(pipe, NULL, file, &%#RX64, %#zx, 0): %#zx; offFileOut=%#RX64, expected %#RX64\n",
                              iLine, offFile, cbLeft, cbActual, (uint64_t)offFileOut, offFile + (uint64_t)cbActual);
                break;
            }
            if (cbActual > 0)
            {
                pArgs->cCalls++;
                offFile += (size_t)cbActual;
                cbTotal += (size_t)cbActual;
                cbLeft  -= (size_t)cbActual;
            }
            else
                break;
        } while (cbLeft > 0);
        uint64_t const nsElapsed = RTTimeNanoTS() - tsStart;

        if (cbTotal != pArgs->cbSent)
            RTTestIFailed("%u: spliced a total of %#zx bytes, expected %#zx!\n", iLine, cbTotal, pArgs->cbSent);

        RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTThreadWait(hThread, 30 * RT_NS_1SEC, NULL), VINF_SUCCESS);

        /* Check the file content. */
        if (fCheckFile && cbTotal == pArgs->cbSent)
        {
            offFile = pArgs->offFile;
            cbLeft  = cbSent;
            while (cbLeft > 0)
            {
                size_t cbToRead = RT_MIN(cbLeft, pArgs->cbBuf);
                RTTESTI_CHECK_RC_BREAK(RTFileReadAt(hFile1, offFile, pArgs->pbBuf, cbToRead, NULL), VINF_SUCCESS);
                if (!fsPerfCheckReadBuf(iLine, offFile, pArgs->pbBuf, cbToRead, pArgs->bFiller))
                    break;
                offFile += cbToRead;
                cbLeft  -= cbToRead;
            }
        }
        return nsElapsed;
    }
    return 0;
}


static void fsPerfSpliceToFile(RTFILE hFile1, uint64_t cbFile)
{
    RTTestISub("splice/to-file");

    /*
     * splice was introduced in 2.6.17 according to the man-page.
     */
    char szRelease[64];
    RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szRelease, sizeof(szRelease));
    if (RTStrVersionCompare(szRelease, "2.6.17") < 0)
    {
        RTTestPassed(g_hTest, "too old kernel (%s)", szRelease);
        return;
    }

    uint64_t const cbFileMax = RT_MIN(cbFile, UINT32_MAX - PAGE_OFFSET_MASK);
    signal(SIGPIPE, SIG_IGN);

    /*
     * Allocate a buffer.
     */
    FSPERFSPLICEARGS Args;
    Args.cbBuf = RT_MIN(cbFileMax, _16M);
    Args.pbBuf = (uint8_t *)RTMemAlloc(Args.cbBuf);
    while (!Args.pbBuf)
    {
        Args.cbBuf /= 8;
        RTTESTI_CHECK_RETV(Args.cbBuf >= _64K);
        Args.pbBuf = (uint8_t *)RTMemAlloc(Args.cbBuf);
    }

    /*
     * Do the whole file.
     */
    uint8_t bFiller = 0x76;
    fsPerfSpliceWriteFile(&Args, hFile1, 0, cbFileMax, cbFileMax, bFiller, true /*fCheckFile*/, __LINE__);

    /*
     * Do 64 random chunks (this is slower).
     */
    uint64_t const cbSmall = RT_MIN(_256K, cbFileMax / 16);
    for (uint32_t iTest = 0; iTest < 64; iTest++)
    {
        size_t const   cbToWrite    = (size_t)RTRandU64Ex(1, iTest < 24 ? cbSmall : cbFileMax);
        uint64_t const offToWriteAt = RTRandU64Ex(0, cbFile - cbToWrite);
        uint64_t const cbTryRead    = cbToWrite + (iTest & 1 ? RTRandU32Ex(0, _64K) : 0);

        bFiller++;
        fsPerfSpliceWriteFile(&Args, hFile1, offToWriteAt, cbTryRead, cbToWrite, bFiller, true /*fCheckFile*/, __LINE__);
    }

    /*
     * Benchmark it.
     */
    Args.cCalls          = 0;
    uint32_t cIterations = 0;
    uint64_t nsElapsed   = 0;
    for (;;)
    {
        uint64_t cNsThis = fsPerfSpliceWriteFile(&Args, hFile1, 0, cbFileMax, cbFileMax, 0xf6, false /*fCheckBuf*/, __LINE__);
        nsElapsed += cNsThis;
        cIterations++;
        if (!cNsThis || nsElapsed >= g_nsTestRun)
            break;
    }
    uint64_t cbTotal = cbFileMax * cIterations;
    RTTestIValue("latency",    nsElapsed / Args.cCalls,                                 RTTESTUNIT_NS_PER_CALL);
    RTTestIValue("throughput", (uint64_t)(cbTotal / ((double)nsElapsed / RT_NS_1SEC)),  RTTESTUNIT_BYTES_PER_SEC);
    RTTestIValue("calls",      Args.cCalls,                                             RTTESTUNIT_CALLS);
    RTTestIValue("bytes/call", cbTotal / Args.cCalls,                                   RTTESTUNIT_BYTES);
    RTTestIValue("iterations", cIterations,                                             RTTESTUNIT_NONE);
    RTTestIValue("bytes",      cbTotal,                                                 RTTESTUNIT_BYTES);
    if (g_fShowDuration)
        RTTestIValue("duration", nsElapsed,                                             RTTESTUNIT_NS);

    /*
     * Cleanup.
     */
    RTMemFree(Args.pbBuf);
}

#endif /* RT_OS_LINUX */

/** For fsPerfIoRead and fsPerfIoWrite. */
#define PROFILE_IO_FN(a_szOperation, a_fnCall) \
    do \
    { \
        RTTESTI_CHECK_RC_RETV(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS); \
        uint64_t offActual = 0; \
        uint32_t cSeeks    = 0; \
        \
        /* Estimate how many iterations we need to fill up the given timeslot: */ \
        fsPerfYield(); \
        uint64_t nsStart = RTTimeNanoTS(); \
        uint64_t ns; \
        do \
            ns = RTTimeNanoTS(); \
        while (ns == nsStart); \
        nsStart = ns; \
        \
        uint64_t iIteration = 0; \
        do \
        { \
            RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
            iIteration++; \
            ns = RTTimeNanoTS() - nsStart; \
        } while (ns < RT_NS_10MS); \
        ns /= iIteration; \
        if (ns > g_nsPerNanoTSCall + 32) \
            ns -= g_nsPerNanoTSCall; \
        uint64_t cIterations = g_nsTestRun / ns; \
        if (cIterations < 2) \
            cIterations = 2; \
        else if (cIterations & 1) \
            cIterations++; \
        \
        /* Do the actual profiling: */ \
        cSeeks = 0; \
        iIteration = 0; \
        fsPerfYield(); \
        nsStart = RTTimeNanoTS(); \
        for (uint32_t iAdjust = 0; iAdjust < 4; iAdjust++) \
        { \
            for (; iIteration < cIterations; iIteration++)\
                RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
            ns = RTTimeNanoTS() - nsStart;\
            if (ns >= g_nsTestRun - (g_nsTestRun / 10)) \
                break; \
            cIterations += cIterations / 4; \
            if (cIterations & 1) \
                cIterations++; \
            nsStart += g_nsPerNanoTSCall; \
        } \
        RTTestIValueF(ns / iIteration, \
                      RTTESTUNIT_NS_PER_OCCURRENCE, a_szOperation "/seq/%RU32 latency", cbBlock); \
        RTTestIValueF((uint64_t)((uint64_t)iIteration * cbBlock / ((double)ns / RT_NS_1SEC)), \
                      RTTESTUNIT_BYTES_PER_SEC,     a_szOperation "/seq/%RU32 throughput", cbBlock); \
        RTTestIValueF(iIteration, \
                      RTTESTUNIT_CALLS,             a_szOperation "/seq/%RU32 calls", cbBlock); \
        RTTestIValueF((uint64_t)iIteration * cbBlock, \
                      RTTESTUNIT_BYTES,             a_szOperation "/seq/%RU32 bytes", cbBlock); \
        RTTestIValueF(cSeeks, \
                      RTTESTUNIT_OCCURRENCES,       a_szOperation "/seq/%RU32 seeks", cbBlock); \
        if (g_fShowDuration) \
            RTTestIValueF(ns, RTTESTUNIT_NS,        a_szOperation "/seq/%RU32 duration", cbBlock); \
    } while (0)


/**
 * One RTFileRead profiling iteration.
 */
DECL_FORCE_INLINE(int) fsPerfIoReadWorker(RTFILE hFile1, uint64_t cbFile, uint32_t cbBlock, uint8_t *pbBlock,
                                          uint64_t *poffActual, uint32_t *pcSeeks)
{
    /* Do we need to seek back to the start? */
    if (*poffActual + cbBlock <= cbFile)
    { /* likely */ }
    else
    {
        RTTESTI_CHECK_RC_RET(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS, rcCheck);
        *pcSeeks += 1;
        *poffActual = 0;
    }

    size_t cbActuallyRead = 0;
    RTTESTI_CHECK_RC_RET(RTFileRead(hFile1, pbBlock, cbBlock, &cbActuallyRead), VINF_SUCCESS, rcCheck);
    if (cbActuallyRead == cbBlock)
    {
        *poffActual += cbActuallyRead;
        return VINF_SUCCESS;
    }
    RTTestIFailed("RTFileRead at %#RX64 returned just %#x bytes, expected %#x", *poffActual, cbActuallyRead, cbBlock);
    *poffActual += cbActuallyRead;
    return VERR_READ_ERROR;
}


void fsPerfIoReadBlockSize(RTFILE hFile1, uint64_t cbFile, uint32_t cbBlock)
{
    RTTestISubF("IO - Sequential read %RU32", cbBlock);

    uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBlock);
    if (pbBuf)
    {
        memset(pbBuf, 0xf7, cbBlock);
        PROFILE_IO_FN("RTFileRead", fsPerfIoReadWorker(hFile1, cbFile, cbBlock, pbBuf, &offActual, &cSeeks));
        RTMemPageFree(pbBuf, cbBlock);
    }
    else
        RTTestSkipped(g_hTest, "insufficient (virtual) memory available");
}


/** preadv is too new to be useful, so we use the readv api via this wrapper. */
DECLINLINE(int) myFileSgReadAt(RTFILE hFile, RTFOFF off, PRTSGBUF pSgBuf, size_t cbToRead, size_t *pcbRead)
{
    int rc = RTFileSeek(hFile, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
        rc = RTFileSgRead(hFile, pSgBuf, cbToRead, pcbRead);
    return rc;
}


void fsPerfRead(RTFILE hFile1, RTFILE hFileNoCache, uint64_t cbFile)
{
    RTTestISubF("IO - RTFileRead");

    /*
     * Allocate a big buffer we can play around with.  Min size is 1MB.
     */
    size_t   cbBuf = cbFile < _64M ? (size_t)cbFile : _64M;
    uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
    while (!pbBuf)
    {
        cbBuf /= 2;
        RTTESTI_CHECK_RETV(cbBuf >= _1M);
        pbBuf = (uint8_t *)RTMemPageAlloc(_32M);
    }

#if 1
    /*
     * Start at the beginning and read the full buffer in random small chunks, thereby
     * checking that unaligned buffer addresses, size and file offsets work fine.
     */
    struct
    {
        uint64_t offFile;
        uint32_t cbMax;
    } aRuns[] = { { 0, 127 }, { cbFile - cbBuf, UINT32_MAX }, { 0, UINT32_MAX -1 }};
    for (uint32_t i = 0; i < RT_ELEMENTS(aRuns); i++)
    {
        memset(pbBuf, 0x55, cbBuf);
        RTTESTI_CHECK_RC(RTFileSeek(hFile1, aRuns[i].offFile, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
        for (size_t offBuf = 0; offBuf < cbBuf; )
        {
            uint32_t const cbLeft   = (uint32_t)(cbBuf - offBuf);
            uint32_t const cbToRead = aRuns[i].cbMax < UINT32_MAX / 2 ? RTRandU32Ex(1, RT_MIN(aRuns[i].cbMax, cbLeft))
                                    : aRuns[i].cbMax == UINT32_MAX    ? RTRandU32Ex(RT_MAX(cbLeft / 4, 1), cbLeft)
                                    :                                   RTRandU32Ex(cbLeft >= _8K ? _8K : 1, RT_MIN(_1M, cbLeft));
            size_t cbActual = 0;
            RTTESTI_CHECK_RC(RTFileRead(hFile1, &pbBuf[offBuf], cbToRead, &cbActual), VINF_SUCCESS);
            if (cbActual == cbToRead)
            {
                offBuf += cbActual;
                RTTESTI_CHECK_MSG(RTFileTell(hFile1) == aRuns[i].offFile + offBuf,
                                  ("%#RX64, expected %#RX64\n", RTFileTell(hFile1), aRuns[i].offFile + offBuf));
            }
            else
            {
                RTTestIFailed("Attempting to read %#x bytes at %#zx, only got %#x bytes back! (cbLeft=%#x cbBuf=%#zx)\n",
                              cbToRead, offBuf, cbActual, cbLeft, cbBuf);
                if (cbActual)
                    offBuf += cbActual;
                else
                    pbBuf[offBuf++] = 0x11;
            }
        }
        fsPerfCheckReadBuf(__LINE__, aRuns[i].offFile, pbBuf, cbBuf);
    }

    /*
     * Test reading beyond the end of the file.
     */
    size_t   const acbMax[] = { cbBuf, _64K, _16K, _4K, 256 };
    uint32_t const aoffFromEos[] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 63, 64, 127, 128, 255, 254, 256, 1023, 1024, 2048,
        4092, 4093, 4094, 4095, 4096, 4097, 4098, 4099, 4100, 8192, 16384, 32767, 32768, 32769, 65535, 65536, _1M - 1
    };
    for (unsigned iMax = 0; iMax < RT_ELEMENTS(acbMax); iMax++)
    {
        size_t const cbMaxRead = acbMax[iMax];
        for (uint32_t iOffFromEos = 0; iOffFromEos < RT_ELEMENTS(aoffFromEos); iOffFromEos++)
        {
            uint32_t off = aoffFromEos[iOffFromEos];
            if (off >= cbMaxRead)
                continue;
            RTTESTI_CHECK_RC(RTFileSeek(hFile1, cbFile - off, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
            size_t cbActual = ~(size_t)0;
            RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, cbMaxRead, &cbActual), VINF_SUCCESS);
            RTTESTI_CHECK(cbActual == off);

            RTTESTI_CHECK_RC(RTFileSeek(hFile1, cbFile - off, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
            cbActual = ~(size_t)0;
            RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, off, &cbActual), VINF_SUCCESS);
            RTTESTI_CHECK_MSG(cbActual == off, ("%#zx vs %#zx\n", cbActual, off));

            cbActual = ~(size_t)0;
            RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, 1, &cbActual), VINF_SUCCESS);
            RTTESTI_CHECK_MSG(cbActual == 0, ("cbActual=%zu\n", cbActual));

            RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, cbMaxRead, NULL), VERR_EOF);

            /* Repeat using native APIs in case IPRT or other layers hid status codes: */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
            RTTESTI_CHECK_RC(RTFileSeek(hFile1, cbFile - off, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
# ifdef RT_OS_OS2
            ULONG cbActual2 = ~(ULONG)0;
            APIRET orc = DosRead((HFILE)RTFileToNative(hFile1), pbBuf, cbMaxRead, &cbActual2);
            RTTESTI_CHECK_MSG(orc == NO_ERROR, ("orc=%u, expected 0\n", orc));
            RTTESTI_CHECK_MSG(cbActual2 == off, ("%#x vs %#x\n", cbActual2, off));
# else
            IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            NTSTATUS rcNt = NtReadFile((HANDLE)RTFileToNative(hFile1), NULL /*hEvent*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                                       &Ios, pbBuf, (ULONG)cbMaxRead, NULL /*poffFile*/, NULL /*Key*/);
            if (off == 0)
                RTTESTI_CHECK_MSG(rcNt == STATUS_END_OF_FILE, ("rcNt=%#x, expected %#x\n", rcNt, STATUS_END_OF_FILE));
            else
                RTTESTI_CHECK_MSG(rcNt == STATUS_SUCCESS, ("rcNt=%#x, expected 0 (off=%#x cbMaxRead=%#zx)\n", rcNt, off, cbMaxRead));
            RTTESTI_CHECK_MSG(Ios.Information == off, ("%#zx vs %#x\n", Ios.Information, off));
# endif

# ifdef RT_OS_OS2
            cbActual2 = ~(ULONG)0;
            orc = DosRead((HFILE)RTFileToNative(hFile1), pbBuf, 1, &cbActual2);
            RTTESTI_CHECK_MSG(orc == NO_ERROR, ("orc=%u, expected 0\n", orc));
            RTTESTI_CHECK_MSG(cbActual2 == 0, ("cbActual2=%u\n", cbActual2));
# else
            RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
            rcNt = NtReadFile((HANDLE)RTFileToNative(hFile1), NULL /*hEvent*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                              &Ios, pbBuf, 1, NULL /*poffFile*/, NULL /*Key*/);
            RTTESTI_CHECK_MSG(rcNt == STATUS_END_OF_FILE, ("rcNt=%#x, expected %#x\n", rcNt, STATUS_END_OF_FILE));
# endif

#endif
        }
    }

    /*
     * Test reading beyond end of the file.
     */
    for (unsigned iMax = 0; iMax < RT_ELEMENTS(acbMax); iMax++)
    {
        size_t const cbMaxRead = acbMax[iMax];
        for (uint32_t off = 0; off < 256; off++)
        {
            RTTESTI_CHECK_RC(RTFileSeek(hFile1, cbFile + off, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
            size_t cbActual = ~(size_t)0;
            RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, cbMaxRead, &cbActual), VINF_SUCCESS);
            RTTESTI_CHECK(cbActual == 0);

            RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, cbMaxRead, NULL), VERR_EOF);

            /* Repeat using native APIs in case IPRT or other layers hid status codes: */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
            RTTESTI_CHECK_RC(RTFileSeek(hFile1, cbFile + off, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
# ifdef RT_OS_OS2
            ULONG cbActual2 = ~(ULONG)0;
            APIRET orc = DosRead((HFILE)RTFileToNative(hFile1), pbBuf, cbMaxRead, &cbActual2);
            RTTESTI_CHECK_MSG(orc == NO_ERROR, ("orc=%u, expected 0\n", orc));
            RTTESTI_CHECK_MSG(cbActual2 == 0, ("%#x vs %#x\n", cbActual2, off));
# else
            IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            NTSTATUS rcNt = NtReadFile((HANDLE)RTFileToNative(hFile1), NULL /*hEvent*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                                       &Ios, pbBuf, (ULONG)cbMaxRead, NULL /*poffFile*/, NULL /*Key*/);
            RTTESTI_CHECK_MSG(rcNt == STATUS_END_OF_FILE, ("rcNt=%#x, expected %#x\n", rcNt, STATUS_END_OF_FILE));
# endif
#endif
        }
    }

    /*
     * Do uncached access, must be page aligned.
     */
    uint32_t cbPage = PAGE_SIZE;
    memset(pbBuf, 0x66, cbBuf);
    if (!g_fIgnoreNoCache || hFileNoCache != NIL_RTFILE)
    {
        RTTESTI_CHECK_RC(RTFileSeek(hFileNoCache, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
        for (size_t offBuf = 0; offBuf < cbBuf; )
        {
            uint32_t const cPagesLeft   = (uint32_t)((cbBuf - offBuf) / cbPage);
            uint32_t const cPagesToRead = RTRandU32Ex(1, cPagesLeft);
            size_t const   cbToRead     = cPagesToRead * (size_t)cbPage;
            size_t cbActual = 0;
            RTTESTI_CHECK_RC(RTFileRead(hFileNoCache, &pbBuf[offBuf], cbToRead, &cbActual), VINF_SUCCESS);
            if (cbActual == cbToRead)
                offBuf += cbActual;
            else
            {
                RTTestIFailed("Attempting to read %#zx bytes at %#zx, only got %#x bytes back!\n", cbToRead, offBuf, cbActual);
                if (cbActual)
                    offBuf += cbActual;
                else
                {
                    memset(&pbBuf[offBuf], 0x11, cbPage);
                    offBuf += cbPage;
                }
            }
        }
        fsPerfCheckReadBuf(__LINE__, 0, pbBuf, cbBuf);
    }

    /*
     * Check reading zero bytes at the end of the file.
     * Requires native call because RTFileWrite doesn't call kernel on zero byte reads.
     */
    RTTESTI_CHECK_RC(RTFileSeek(hFile1, 0, RTFILE_SEEK_END, NULL), VINF_SUCCESS);
# ifdef RT_OS_WINDOWS
    IO_STATUS_BLOCK Ios       = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    NTSTATUS rcNt = NtReadFile((HANDLE)RTFileToNative(hFile1), NULL, NULL, NULL, &Ios, pbBuf, 0, NULL, NULL);
    RTTESTI_CHECK_MSG(rcNt == STATUS_SUCCESS, ("rcNt=%#x", rcNt));
    RTTESTI_CHECK(Ios.Status == STATUS_SUCCESS);
    RTTESTI_CHECK(Ios.Information == 0);

    RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
    rcNt = NtReadFile((HANDLE)RTFileToNative(hFile1), NULL, NULL, NULL, &Ios, pbBuf, 1, NULL, NULL);
    RTTESTI_CHECK_MSG(rcNt == STATUS_END_OF_FILE, ("rcNt=%#x", rcNt));
    RTTESTI_CHECK(Ios.Status == STATUS_END_OF_FILE);
    RTTESTI_CHECK(Ios.Information == 0);
# else
    ssize_t cbRead = read((int)RTFileToNative(hFile1), pbBuf, 0);
    RTTESTI_CHECK(cbRead == 0);
# endif

#else
    RT_NOREF(hFileNoCache);
#endif

    /*
     * Scatter read function operation.
     */
#ifdef RT_OS_WINDOWS
    /** @todo RTFileSgReadAt is just a RTFileReadAt loop for windows NT.  Need
     *        to use ReadFileScatter (nocache + page aligned). */
#elif !defined(RT_OS_OS2) /** @todo implement RTFileSg using list i/o */

# ifdef UIO_MAXIOV
    RTSGSEG aSegs[UIO_MAXIOV];
# else
    RTSGSEG aSegs[512];
# endif
    RTSGBUF SgBuf;
    uint32_t cIncr = 1;
    for (uint32_t cSegs = 1; cSegs <= RT_ELEMENTS(aSegs); cSegs += cIncr)
    {
        size_t const cbSeg    = cbBuf / cSegs;
        size_t const cbToRead = cbSeg * cSegs;
        for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
        {
            aSegs[iSeg].cbSeg = cbSeg;
            aSegs[iSeg].pvSeg = &pbBuf[cbToRead - (iSeg + 1) * cbSeg];
        }
        RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
        int rc = myFileSgReadAt(hFile1, 0, &SgBuf, cbToRead, NULL);
        if (RT_SUCCESS(rc))
            for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
            {
                if (!fsPerfCheckReadBuf(__LINE__, iSeg * cbSeg, &pbBuf[cbToRead - (iSeg + 1) * cbSeg], cbSeg))
                {
                    cSegs = RT_ELEMENTS(aSegs);
                    break;
                }
            }
        else
        {
            RTTestIFailed("myFileSgReadAt failed: %Rrc - cSegs=%u cbSegs=%#zx cbToRead=%#zx", rc, cSegs, cbSeg, cbToRead);
            break;
        }
        if (cSegs == 16)
            cIncr = 7;
        else if (cSegs == 16 * 7 + 16 /*= 128*/)
            cIncr = 64;
    }

    for (uint32_t iTest = 0; iTest < 128; iTest++)
    {
        uint32_t cSegs     = RTRandU32Ex(1, RT_ELEMENTS(aSegs));
        uint32_t iZeroSeg  = cSegs > 10 ? RTRandU32Ex(0, cSegs - 1)                    : UINT32_MAX / 2;
        uint32_t cZeroSegs = cSegs > 10 ? RTRandU32Ex(1, RT_MIN(cSegs - iZeroSeg, 25)) : 0;
        size_t   cbToRead  = 0;
        size_t   cbLeft    = cbBuf;
        uint8_t *pbCur     = &pbBuf[cbBuf];
        for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
        {
            uint32_t iAlign = RTRandU32Ex(0, 3);
            if (iAlign & 2) /* end is page aligned */
            {
                cbLeft -= (uintptr_t)pbCur & PAGE_OFFSET_MASK;
                pbCur  -= (uintptr_t)pbCur & PAGE_OFFSET_MASK;
            }

            size_t cbSegOthers = (cSegs - iSeg) * _8K;
            size_t cbSegMax    = cbLeft > cbSegOthers ? cbLeft - cbSegOthers
                               : cbLeft > cSegs       ? cbLeft - cSegs
                               : cbLeft;
            size_t cbSeg       = cbLeft != 0 ? RTRandU32Ex(0, cbSegMax) : 0;
            if (iAlign & 1) /* start is page aligned */
                cbSeg += ((uintptr_t)pbCur - cbSeg) & PAGE_OFFSET_MASK;

            if (iSeg - iZeroSeg < cZeroSegs)
                cbSeg = 0;

            cbToRead += cbSeg;
            cbLeft   -= cbSeg;
            pbCur    -= cbSeg;
            aSegs[iSeg].cbSeg = cbSeg;
            aSegs[iSeg].pvSeg = pbCur;
        }

        uint64_t offFile = cbToRead < cbFile ? RTRandU64Ex(0, cbFile - cbToRead) : 0;
        RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
        int rc = myFileSgReadAt(hFile1, offFile, &SgBuf, cbToRead, NULL);
        if (RT_SUCCESS(rc))
            for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
            {
                if (!fsPerfCheckReadBuf(__LINE__, offFile, (uint8_t *)aSegs[iSeg].pvSeg, aSegs[iSeg].cbSeg))
                {
                    RTTestIFailureDetails("iSeg=%#x cSegs=%#x cbSeg=%#zx cbToRead=%#zx\n", iSeg, cSegs, aSegs[iSeg].cbSeg, cbToRead);
                    iTest = _16K;
                    break;
                }
                offFile += aSegs[iSeg].cbSeg;
            }
        else
        {
            RTTestIFailed("myFileSgReadAt failed: %Rrc - cSegs=%#x cbToRead=%#zx", rc, cSegs, cbToRead);
            for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
                RTTestIFailureDetails("aSeg[%u] = %p LB %#zx (last %p)\n", iSeg, aSegs[iSeg].pvSeg, aSegs[iSeg].cbSeg,
                                      (uint8_t *)aSegs[iSeg].pvSeg + aSegs[iSeg].cbSeg - 1);
            break;
        }
    }

    /* reading beyond the end of the file */
    for (uint32_t cSegs = 1; cSegs < 6; cSegs++)
        for (uint32_t iTest = 0; iTest < 128; iTest++)
        {
            uint32_t const cbToRead  = RTRandU32Ex(0, cbBuf);
            uint32_t const cbBeyond  = cbToRead ? RTRandU32Ex(0, cbToRead) : 0;
            uint32_t const cbSeg     = cbToRead / cSegs;
            uint32_t       cbLeft    = cbToRead;
            uint8_t       *pbCur     = &pbBuf[cbToRead];
            for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
            {
                aSegs[iSeg].cbSeg = iSeg + 1 < cSegs ? cbSeg : cbLeft;
                aSegs[iSeg].pvSeg = pbCur -= aSegs[iSeg].cbSeg;
                cbLeft -= aSegs[iSeg].cbSeg;
            }
            Assert(pbCur == pbBuf);

            uint64_t offFile = cbFile + cbBeyond - cbToRead;
            RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
            int rcExpect = cbBeyond == 0 || cbToRead == 0 ? VINF_SUCCESS : VERR_EOF;
            int rc = myFileSgReadAt(hFile1, offFile, &SgBuf, cbToRead, NULL);
            if (rc != rcExpect)
            {
                RTTestIFailed("myFileSgReadAt failed: %Rrc - cSegs=%#x cbToRead=%#zx cbBeyond=%#zx\n", rc, cSegs, cbToRead, cbBeyond);
                for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
                    RTTestIFailureDetails("aSeg[%u] = %p LB %#zx (last %p)\n", iSeg, aSegs[iSeg].pvSeg, aSegs[iSeg].cbSeg,
                                          (uint8_t *)aSegs[iSeg].pvSeg + aSegs[iSeg].cbSeg - 1);
            }

            RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
            size_t cbActual = 0;
            rc = myFileSgReadAt(hFile1, offFile, &SgBuf, cbToRead, &cbActual);
            if (rc != VINF_SUCCESS || cbActual != cbToRead - cbBeyond)
                RTTestIFailed("myFileSgReadAt failed: %Rrc cbActual=%#zu - cSegs=%#x cbToRead=%#zx cbBeyond=%#zx expected %#zx\n",
                              rc, cbActual, cSegs, cbToRead, cbBeyond, cbToRead - cbBeyond);
            if (RT_SUCCESS(rc) && cbActual > 0)
                for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
                {
                    if (!fsPerfCheckReadBuf(__LINE__, offFile, (uint8_t *)aSegs[iSeg].pvSeg, RT_MIN(cbActual, aSegs[iSeg].cbSeg)))
                    {
                        RTTestIFailureDetails("iSeg=%#x cSegs=%#x cbSeg=%#zx cbActual%#zx cbToRead=%#zx cbBeyond=%#zx\n",
                                              iSeg, cSegs, aSegs[iSeg].cbSeg, cbActual, cbToRead, cbBeyond);
                        iTest = _16K;
                        break;
                    }
                    if (cbActual <= aSegs[iSeg].cbSeg)
                        break;
                    cbActual -= aSegs[iSeg].cbSeg;
                    offFile  += aSegs[iSeg].cbSeg;
                }
        }

#endif

    /*
     * Other OS specific stuff.
     */
#ifdef RT_OS_WINDOWS
    /* Check that reading at an offset modifies the position: */
    RTTESTI_CHECK_RC(RTFileSeek(hFile1, 0, RTFILE_SEEK_END, NULL), VINF_SUCCESS);
    RTTESTI_CHECK(RTFileTell(hFile1) == cbFile);

    RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
    LARGE_INTEGER offNt;
    offNt.QuadPart = cbFile / 2;
    rcNt = NtReadFile((HANDLE)RTFileToNative(hFile1), NULL, NULL, NULL, &Ios, pbBuf, _4K, &offNt, NULL);
    RTTESTI_CHECK_MSG(rcNt == STATUS_SUCCESS, ("rcNt=%#x", rcNt));
    RTTESTI_CHECK(Ios.Status == STATUS_SUCCESS);
    RTTESTI_CHECK(Ios.Information == _4K);
    RTTESTI_CHECK(RTFileTell(hFile1) == cbFile / 2 + _4K);
    fsPerfCheckReadBuf(__LINE__, cbFile / 2, pbBuf, _4K);
#endif


    RTMemPageFree(pbBuf, cbBuf);
}


/**
 * One RTFileWrite profiling iteration.
 */
DECL_FORCE_INLINE(int) fsPerfIoWriteWorker(RTFILE hFile1, uint64_t cbFile, uint32_t cbBlock, uint8_t *pbBlock,
                                           uint64_t *poffActual, uint32_t *pcSeeks)
{
    /* Do we need to seek back to the start? */
    if (*poffActual + cbBlock <= cbFile)
    { /* likely */ }
    else
    {
        RTTESTI_CHECK_RC_RET(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS, rcCheck);
        *pcSeeks += 1;
        *poffActual = 0;
    }

    size_t cbActuallyWritten = 0;
    RTTESTI_CHECK_RC_RET(RTFileWrite(hFile1, pbBlock, cbBlock, &cbActuallyWritten), VINF_SUCCESS, rcCheck);
    if (cbActuallyWritten == cbBlock)
    {
        *poffActual += cbActuallyWritten;
        return VINF_SUCCESS;
    }
    RTTestIFailed("RTFileWrite at %#RX64 returned just %#x bytes, expected %#x", *poffActual, cbActuallyWritten, cbBlock);
    *poffActual += cbActuallyWritten;
    return VERR_WRITE_ERROR;
}


void fsPerfIoWriteBlockSize(RTFILE hFile1, uint64_t cbFile, uint32_t cbBlock)
{
    RTTestISubF("IO - Sequential write %RU32", cbBlock);

    uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBlock);
    if (pbBuf)
    {
        memset(pbBuf, 0xf7, cbBlock);
        PROFILE_IO_FN("RTFileWrite", fsPerfIoWriteWorker(hFile1, cbFile, cbBlock, pbBuf, &offActual, &cSeeks));
        RTMemPageFree(pbBuf, cbBlock);
    }
    else
        RTTestSkipped(g_hTest, "insufficient (virtual) memory available");
}


/** pwritev is too new to be useful, so we use the writev api via this wrapper. */
DECLINLINE(int) myFileSgWriteAt(RTFILE hFile, RTFOFF off, PRTSGBUF pSgBuf, size_t cbToWrite, size_t *pcbWritten)
{
    int rc = RTFileSeek(hFile, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
        rc = RTFileSgWrite(hFile, pSgBuf, cbToWrite, pcbWritten);
    return rc;
}


void fsPerfWrite(RTFILE hFile1, RTFILE hFileNoCache, RTFILE hFileWriteThru, uint64_t cbFile)
{
    RTTestISubF("IO - RTFileWrite");

    /*
     * Allocate a big buffer we can play around with.  Min size is 1MB.
     */
    size_t   cbBuf = cbFile < _64M ? (size_t)cbFile : _64M;
    uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
    while (!pbBuf)
    {
        cbBuf /= 2;
        RTTESTI_CHECK_RETV(cbBuf >= _1M);
        pbBuf = (uint8_t *)RTMemPageAlloc(_32M);
    }

    uint8_t bFiller = 0x88;

#if 1
    /*
     * Start at the beginning and write out the full buffer in random small chunks, thereby
     * checking that unaligned buffer addresses, size and file offsets work fine.
     */
    struct
    {
        uint64_t offFile;
        uint32_t cbMax;
    } aRuns[] = { { 0, 127 }, { cbFile - cbBuf, UINT32_MAX }, { 0, UINT32_MAX -1 }};
    for (uint32_t i = 0; i < RT_ELEMENTS(aRuns); i++, bFiller)
    {
        fsPerfFillWriteBuf(aRuns[i].offFile, pbBuf, cbBuf, bFiller);
        fsPerfCheckReadBuf(__LINE__, aRuns[i].offFile, pbBuf, cbBuf, bFiller);

        RTTESTI_CHECK_RC(RTFileSeek(hFile1, aRuns[i].offFile, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
        for (size_t offBuf = 0; offBuf < cbBuf; )
        {
            uint32_t const cbLeft    = (uint32_t)(cbBuf - offBuf);
            uint32_t const cbToWrite = aRuns[i].cbMax < UINT32_MAX / 2 ? RTRandU32Ex(1, RT_MIN(aRuns[i].cbMax, cbLeft))
                                     : aRuns[i].cbMax == UINT32_MAX    ? RTRandU32Ex(RT_MAX(cbLeft / 4, 1), cbLeft)
                                     : RTRandU32Ex(cbLeft >= _8K ? _8K : 1, RT_MIN(_1M, cbLeft));
            size_t cbActual = 0;
            RTTESTI_CHECK_RC(RTFileWrite(hFile1, &pbBuf[offBuf], cbToWrite, &cbActual), VINF_SUCCESS);
            if (cbActual == cbToWrite)
            {
                offBuf += cbActual;
                RTTESTI_CHECK_MSG(RTFileTell(hFile1) == aRuns[i].offFile + offBuf,
                                  ("%#RX64, expected %#RX64\n", RTFileTell(hFile1), aRuns[i].offFile + offBuf));
            }
            else
            {
                RTTestIFailed("Attempting to write %#x bytes at %#zx (%#x left), only got %#x written!\n",
                              cbToWrite, offBuf, cbLeft, cbActual);
                if (cbActual)
                    offBuf += cbActual;
                else
                    pbBuf[offBuf++] = 0x11;
            }
        }

        RTTESTI_CHECK_RC(RTFileReadAt(hFile1, aRuns[i].offFile, pbBuf, cbBuf, NULL), VINF_SUCCESS);
        fsPerfCheckReadBuf(__LINE__, aRuns[i].offFile, pbBuf, cbBuf, bFiller);
    }


    /*
     * Do uncached and write-thru accesses, must be page aligned.
     */
    RTFILE ahFiles[2] = { hFileWriteThru, hFileNoCache };
    for (unsigned iFile = 0; iFile < RT_ELEMENTS(ahFiles); iFile++, bFiller++)
    {
        if (g_fIgnoreNoCache && ahFiles[iFile] == NIL_RTFILE)
            continue;

        fsPerfFillWriteBuf(0, pbBuf, cbBuf, bFiller);
        fsPerfCheckReadBuf(__LINE__, 0, pbBuf, cbBuf, bFiller);
        RTTESTI_CHECK_RC(RTFileSeek(ahFiles[iFile], 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);

        uint32_t cbPage = PAGE_SIZE;
        for (size_t offBuf = 0; offBuf < cbBuf; )
        {
            uint32_t const cPagesLeft    = (uint32_t)((cbBuf - offBuf) / cbPage);
            uint32_t const cPagesToWrite = RTRandU32Ex(1, cPagesLeft);
            size_t const   cbToWrite     = cPagesToWrite * (size_t)cbPage;
            size_t cbActual = 0;
            RTTESTI_CHECK_RC(RTFileWrite(ahFiles[iFile], &pbBuf[offBuf], cbToWrite, &cbActual), VINF_SUCCESS);
            if (cbActual == cbToWrite)
            {
                RTTESTI_CHECK_RC(RTFileReadAt(hFile1, offBuf, pbBuf, cbToWrite, NULL), VINF_SUCCESS);
                fsPerfCheckReadBuf(__LINE__, offBuf, pbBuf, cbToWrite, bFiller);
                offBuf += cbActual;
            }
            else
            {
                RTTestIFailed("Attempting to read %#zx bytes at %#zx, only got %#x written!\n", cbToWrite, offBuf, cbActual);
                if (cbActual)
                    offBuf += cbActual;
                else
                {
                    memset(&pbBuf[offBuf], 0x11, cbPage);
                    offBuf += cbPage;
                }
            }
        }

        RTTESTI_CHECK_RC(RTFileReadAt(ahFiles[iFile], 0, pbBuf, cbBuf, NULL), VINF_SUCCESS);
        fsPerfCheckReadBuf(__LINE__, 0, pbBuf, cbBuf, bFiller);
    }

    /*
     * Check the behavior of writing zero bytes to the file _4K from the end
     * using native API.  In the olden days zero sized write have been known
     * to be used to truncate a file.
     */
    RTTESTI_CHECK_RC(RTFileSeek(hFile1, -_4K, RTFILE_SEEK_END, NULL), VINF_SUCCESS);
# ifdef RT_OS_WINDOWS
    IO_STATUS_BLOCK Ios        = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    NTSTATUS rcNt = NtWriteFile((HANDLE)RTFileToNative(hFile1), NULL, NULL, NULL, &Ios, pbBuf, 0, NULL, NULL);
    RTTESTI_CHECK_MSG(rcNt == STATUS_SUCCESS, ("rcNt=%#x", rcNt));
    RTTESTI_CHECK(Ios.Status == STATUS_SUCCESS);
    RTTESTI_CHECK(Ios.Information == 0);
# else
    ssize_t cbWritten = write((int)RTFileToNative(hFile1), pbBuf, 0);
    RTTESTI_CHECK(cbWritten == 0);
# endif
    RTTESTI_CHECK_RC(RTFileRead(hFile1, pbBuf, _4K, NULL), VINF_SUCCESS);
    fsPerfCheckReadBuf(__LINE__, cbFile - _4K, pbBuf, _4K, pbBuf[0x8]);

#else
    RT_NOREF(hFileNoCache, hFileWriteThru);
#endif

    /*
     * Gather write function operation.
     */
#ifdef RT_OS_WINDOWS
    /** @todo RTFileSgWriteAt is just a RTFileWriteAt loop for windows NT.  Need
     *        to use WriteFileGather (nocache + page aligned). */
#elif !defined(RT_OS_OS2) /** @todo implement RTFileSg using list i/o */

# ifdef UIO_MAXIOV
    RTSGSEG aSegs[UIO_MAXIOV];
# else
    RTSGSEG aSegs[512];
# endif
    RTSGBUF SgBuf;
    uint32_t cIncr = 1;
    for (uint32_t cSegs = 1; cSegs <= RT_ELEMENTS(aSegs); cSegs += cIncr, bFiller++)
    {
        size_t const cbSeg     = cbBuf / cSegs;
        size_t const cbToWrite = cbSeg * cSegs;
        for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
        {
            aSegs[iSeg].cbSeg = cbSeg;
            aSegs[iSeg].pvSeg = &pbBuf[cbToWrite - (iSeg + 1) * cbSeg];
            fsPerfFillWriteBuf(iSeg * cbSeg, (uint8_t *)aSegs[iSeg].pvSeg, cbSeg, bFiller);
        }
        RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
        int rc = myFileSgWriteAt(hFile1, 0, &SgBuf, cbToWrite, NULL);
        if (RT_SUCCESS(rc))
        {
            RTTESTI_CHECK_RC(RTFileReadAt(hFile1, 0, pbBuf, cbToWrite, NULL), VINF_SUCCESS);
            fsPerfCheckReadBuf(__LINE__, 0, pbBuf, cbToWrite, bFiller);
        }
        else
        {
            RTTestIFailed("myFileSgWriteAt failed: %Rrc - cSegs=%u cbSegs=%#zx cbToWrite=%#zx", rc, cSegs, cbSeg, cbToWrite);
            break;
        }
        if (cSegs == 16)
            cIncr = 7;
        else if (cSegs == 16 * 7 + 16 /*= 128*/)
            cIncr = 64;
    }

    /* random stuff, including zero segments.  */
    for (uint32_t iTest = 0; iTest < 128; iTest++, bFiller++)
    {
        uint32_t cSegs     = RTRandU32Ex(1, RT_ELEMENTS(aSegs));
        uint32_t iZeroSeg  = cSegs > 10 ? RTRandU32Ex(0, cSegs - 1)                    : UINT32_MAX / 2;
        uint32_t cZeroSegs = cSegs > 10 ? RTRandU32Ex(1, RT_MIN(cSegs - iZeroSeg, 25)) : 0;
        size_t   cbToWrite = 0;
        size_t   cbLeft    = cbBuf;
        uint8_t *pbCur     = &pbBuf[cbBuf];
        for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
        {
            uint32_t iAlign = RTRandU32Ex(0, 3);
            if (iAlign & 2) /* end is page aligned */
            {
                cbLeft -= (uintptr_t)pbCur & PAGE_OFFSET_MASK;
                pbCur  -= (uintptr_t)pbCur & PAGE_OFFSET_MASK;
            }

            size_t cbSegOthers = (cSegs - iSeg) * _8K;
            size_t cbSegMax    = cbLeft > cbSegOthers ? cbLeft - cbSegOthers
                               : cbLeft > cSegs       ? cbLeft - cSegs
                               : cbLeft;
            size_t cbSeg       = cbLeft != 0 ? RTRandU32Ex(0, cbSegMax) : 0;
            if (iAlign & 1) /* start is page aligned */
                cbSeg += ((uintptr_t)pbCur - cbSeg) & PAGE_OFFSET_MASK;

            if (iSeg - iZeroSeg < cZeroSegs)
                cbSeg = 0;

            cbToWrite += cbSeg;
            cbLeft    -= cbSeg;
            pbCur     -= cbSeg;
            aSegs[iSeg].cbSeg = cbSeg;
            aSegs[iSeg].pvSeg = pbCur;
        }

        uint64_t const offFile = cbToWrite < cbFile ? RTRandU64Ex(0, cbFile - cbToWrite) : 0;
        uint64_t       offFill = offFile;
        for (uint32_t iSeg = 0; iSeg < cSegs; iSeg++)
            if (aSegs[iSeg].cbSeg)
            {
                fsPerfFillWriteBuf(offFill, (uint8_t *)aSegs[iSeg].pvSeg, aSegs[iSeg].cbSeg, bFiller);
                offFill += aSegs[iSeg].cbSeg;
            }

        RTSgBufInit(&SgBuf, &aSegs[0], cSegs);
        int rc = myFileSgWriteAt(hFile1, offFile, &SgBuf, cbToWrite, NULL);
        if (RT_SUCCESS(rc))
        {
            RTTESTI_CHECK_RC(RTFileReadAt(hFile1, offFile, pbBuf, cbToWrite, NULL), VINF_SUCCESS);
            fsPerfCheckReadBuf(__LINE__, offFile, pbBuf, cbToWrite, bFiller);
        }
        else
        {
            RTTestIFailed("myFileSgWriteAt failed: %Rrc - cSegs=%#x cbToWrite=%#zx", rc, cSegs, cbToWrite);
            break;
        }
    }

#endif

    /*
     * Other OS specific stuff.
     */
#ifdef RT_OS_WINDOWS
    /* Check that reading at an offset modifies the position: */
    RTTESTI_CHECK_RC(RTFileReadAt(hFile1, cbFile / 2, pbBuf, _4K, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileSeek(hFile1, 0, RTFILE_SEEK_END, NULL), VINF_SUCCESS);
    RTTESTI_CHECK(RTFileTell(hFile1) == cbFile);

    RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
    LARGE_INTEGER offNt;
    offNt.QuadPart = cbFile / 2;
    rcNt = NtWriteFile((HANDLE)RTFileToNative(hFile1), NULL, NULL, NULL, &Ios, pbBuf, _4K, &offNt, NULL);
    RTTESTI_CHECK_MSG(rcNt == STATUS_SUCCESS, ("rcNt=%#x", rcNt));
    RTTESTI_CHECK(Ios.Status == STATUS_SUCCESS);
    RTTESTI_CHECK(Ios.Information == _4K);
    RTTESTI_CHECK(RTFileTell(hFile1) == cbFile / 2 + _4K);
#endif

    RTMemPageFree(pbBuf, cbBuf);
}


/**
 * Worker for testing RTFileFlush.
 */
DECL_FORCE_INLINE(int) fsPerfFSyncWorker(RTFILE hFile1, uint64_t cbFile, uint8_t *pbBuf, size_t cbBuf, uint64_t *poffFile)
{
    if (*poffFile + cbBuf <= cbFile)
    { /* likely */ }
    else
    {
        RTTESTI_CHECK_RC(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
        *poffFile = 0;
    }

    RTTESTI_CHECK_RC_RET(RTFileWrite(hFile1, pbBuf, cbBuf, NULL), VINF_SUCCESS, rcCheck);
    RTTESTI_CHECK_RC_RET(RTFileFlush(hFile1), VINF_SUCCESS, rcCheck);

    *poffFile += cbBuf;
    return VINF_SUCCESS;
}


void fsPerfFSync(RTFILE hFile1, uint64_t cbFile)
{
    RTTestISub("fsync");

    RTTESTI_CHECK_RC(RTFileFlush(hFile1), VINF_SUCCESS);

    PROFILE_FN(RTFileFlush(hFile1), g_nsTestRun, "RTFileFlush");

    size_t   cbBuf = PAGE_SIZE;
    uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
    RTTESTI_CHECK_RETV(pbBuf != NULL);
    memset(pbBuf, 0xf4, cbBuf);

    RTTESTI_CHECK_RC(RTFileSeek(hFile1, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
    uint64_t offFile = 0;
    PROFILE_FN(fsPerfFSyncWorker(hFile1, cbFile, pbBuf, cbBuf, &offFile), g_nsTestRun, "RTFileWrite[Page]/RTFileFlush");

    RTMemPageFree(pbBuf, cbBuf);
}


#ifndef RT_OS_OS2
/**
 * Worker for profiling msync.
 */
DECL_FORCE_INLINE(int) fsPerfMSyncWorker(uint8_t *pbMapping, size_t offMapping, size_t cbFlush, size_t *pcbFlushed)
{
    uint8_t *pbCur = &pbMapping[offMapping];
    for (size_t offFlush = 0; offFlush < cbFlush; offFlush += PAGE_SIZE)
        *(size_t volatile *)&pbCur[offFlush + 8] = cbFlush;
# ifdef RT_OS_WINDOWS
    RTTESTI_CHECK(FlushViewOfFile(pbCur, cbFlush));
# else
    RTTESTI_CHECK(msync(pbCur, cbFlush, MS_SYNC) == 0);
# endif
    if (*pcbFlushed < offMapping + cbFlush)
        *pcbFlushed = offMapping + cbFlush;
    return VINF_SUCCESS;
}
#endif /* !RT_OS_OS2 */


void fsPerfMMap(RTFILE hFile1, RTFILE hFileNoCache, uint64_t cbFile)
{
    RTTestISub("mmap");
#if !defined(RT_OS_OS2)
    static const char * const s_apszStates[] = { "readonly", "writecopy", "readwrite" };
    enum { kMMap_ReadOnly = 0, kMMap_WriteCopy, kMMap_ReadWrite, kMMap_End };
    for (int enmState = kMMap_ReadOnly; enmState < kMMap_End; enmState++)
    {
        /*
         * Do the mapping.
         */
        size_t cbMapping = (size_t)cbFile;
        if (cbMapping != cbFile)
            cbMapping = _256M;
        uint8_t *pbMapping;

# ifdef RT_OS_WINDOWS
        HANDLE hSection;
        pbMapping = NULL;
        for (;; cbMapping /= 2)
        {
            hSection = CreateFileMapping((HANDLE)RTFileToNative(hFile1), NULL,
                                         enmState == kMMap_ReadOnly    ? PAGE_READONLY
                                         : enmState == kMMap_WriteCopy ? PAGE_WRITECOPY : PAGE_READWRITE,
                                         (uint32_t)((uint64_t)cbMapping >> 32), (uint32_t)cbMapping, NULL);
            DWORD dwErr1 = GetLastError();
            DWORD dwErr2 = 0;
            if (hSection != NULL)
            {
                pbMapping = (uint8_t *)MapViewOfFile(hSection,
                                                     enmState == kMMap_ReadOnly    ? FILE_MAP_READ
                                                     : enmState == kMMap_WriteCopy ? FILE_MAP_COPY
                                                     :                               FILE_MAP_WRITE,
                                                     0, 0, cbMapping);
                if (pbMapping)
                    break;
                dwErr2 = GetLastError();
                CloseHandle(hSection);
            }
            if (cbMapping <= _2M)
            {
                RTTestIFailed("%u/%s: CreateFileMapping or MapViewOfFile failed: %u, %u",
                              enmState, s_apszStates[enmState], dwErr1, dwErr2);
                break;
            }
        }
# else
        for (;; cbMapping /= 2)
        {
            pbMapping = (uint8_t *)mmap(NULL, cbMapping,
                                        enmState == kMMap_ReadOnly  ? PROT_READ   : PROT_READ | PROT_WRITE,
                                        enmState == kMMap_WriteCopy ? MAP_PRIVATE : MAP_SHARED,
                                        (int)RTFileToNative(hFile1), 0);
            if ((void *)pbMapping != MAP_FAILED)
                break;
            if (cbMapping <= _2M)
            {
                RTTestIFailed("%u/%s: mmap failed: %s (%u)", enmState, s_apszStates[enmState], strerror(errno), errno);
                break;
            }
        }
# endif
        if (cbMapping <= _2M)
            continue;

        /*
         * Time page-ins just for fun.
         */
        size_t const cPages = cbMapping >> PAGE_SHIFT;
        size_t uDummy = 0;
        uint64_t ns = RTTimeNanoTS();
        for (size_t iPage = 0; iPage < cPages; iPage++)
            uDummy += ASMAtomicReadU8(&pbMapping[iPage << PAGE_SHIFT]);
        ns = RTTimeNanoTS() - ns;
        RTTestIValueF(ns / cPages, RTTESTUNIT_NS_PER_OCCURRENCE,  "page-in %s", s_apszStates[enmState]);

        /* Check the content. */
        fsPerfCheckReadBuf(__LINE__, 0, pbMapping, cbMapping);

        if (enmState != kMMap_ReadOnly)
        {
            /* Write stuff to the first two megabytes.  In the COW case, we'll detect
               corruption of shared data during content checking of the RW iterations. */
            fsPerfFillWriteBuf(0, pbMapping, _2M, 0xf7);
            if (enmState == kMMap_ReadWrite)
            {
                /* For RW we can try read back from the file handle and check if we get
                   a match there first.  */
                uint8_t abBuf[_4K];
                for (uint32_t off = 0; off < _2M; off += sizeof(abBuf))
                {
                    RTTESTI_CHECK_RC(RTFileReadAt(hFile1, off, abBuf, sizeof(abBuf), NULL), VINF_SUCCESS);
                    fsPerfCheckReadBuf(__LINE__, off, abBuf, sizeof(abBuf), 0xf7);
                }
# ifdef RT_OS_WINDOWS
                RTTESTI_CHECK(FlushViewOfFile(pbMapping, _2M));
# else
                RTTESTI_CHECK(msync(pbMapping, _2M, MS_SYNC) == 0);
# endif

                /*
                 * Time modifying and flushing a few different number of pages.
                 */
                static size_t const s_acbFlush[] = { PAGE_SIZE, PAGE_SIZE * 2, PAGE_SIZE * 3, PAGE_SIZE * 8, PAGE_SIZE * 16, _2M };
                for (unsigned iFlushSize = 0 ; iFlushSize < RT_ELEMENTS(s_acbFlush); iFlushSize++)
                {
                    size_t const cbFlush = s_acbFlush[iFlushSize];
                    if (cbFlush > cbMapping)
                        continue;

                    char szDesc[80];
                    RTStrPrintf(szDesc, sizeof(szDesc), "touch/flush/%zu", cbFlush);
                    size_t const cFlushes      = cbMapping / cbFlush;
                    size_t const cbMappingUsed = cFlushes * cbFlush;
                    size_t       cbFlushed     = 0;
                    PROFILE_FN(fsPerfMSyncWorker(pbMapping, (iIteration * cbFlush) % cbMappingUsed, cbFlush, &cbFlushed),
                               g_nsTestRun, szDesc);

                    /*
                     * Check that all the changes made it thru to the file:
                     */
                    if (!g_fIgnoreNoCache || hFileNoCache != NIL_RTFILE)
                    {
                        size_t   cbBuf = _2M;
                        uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
                        if (!pbBuf)
                        {
                            cbBuf = _4K;
                            pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
                        }
                        RTTESTI_CHECK(pbBuf != NULL);
                        if (pbBuf)
                        {
                            RTTESTI_CHECK_RC(RTFileSeek(hFileNoCache, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
                            size_t const cbToCheck = RT_MIN(cFlushes * cbFlush, cbFlushed);
                            unsigned     cErrors   = 0;
                            for (size_t offBuf = 0; cErrors < 32 && offBuf < cbToCheck; offBuf += cbBuf)
                            {
                                size_t cbToRead = RT_MIN(cbBuf, cbToCheck - offBuf);
                                RTTESTI_CHECK_RC(RTFileRead(hFileNoCache, pbBuf, cbToRead, NULL), VINF_SUCCESS);

                                for (size_t offFlush = 0; offFlush < cbToRead; offFlush += PAGE_SIZE)
                                    if (*(size_t volatile *)&pbBuf[offFlush + 8] != cbFlush)
                                    {
                                        RTTestIFailed("Flush issue at offset #%zx: %#zx, expected %#zx (cbFlush=%#zx, %#RX64)",
                                                      offBuf + offFlush + 8, *(size_t volatile *)&pbBuf[offFlush + 8],
                                                      cbFlush, cbFlush, *(uint64_t volatile *)&pbBuf[offFlush]);
                                        if (++cErrors > 32)
                                            break;
                                    }
                            }
                            RTMemPageFree(pbBuf, cbBuf);
                        }
                    }
                }

# if 0 /* not needed, very very slow */
                /*
                 * Restore the file to 0xf6 state for the next test.
                 */
                RTTestIPrintf(RTTESTLVL_ALWAYS, "Restoring content...\n");
                fsPerfFillWriteBuf(0, pbMapping, cbMapping, 0xf6);
#  ifdef RT_OS_WINDOWS
                RTTESTI_CHECK(FlushViewOfFile(pbMapping, cbMapping));
#  else
                RTTESTI_CHECK(msync(pbMapping, cbMapping, MS_SYNC) == 0);
#  endif
                RTTestIPrintf(RTTESTLVL_ALWAYS, "... done\n");
# endif
            }
        }

        /*
         * Observe how regular writes affects a read-only or readwrite mapping.
         * These should ideally be immediately visible in the mapping, at least
         * when not performed thru an no-cache handle.
         */
        if (enmState == kMMap_ReadOnly || enmState == kMMap_ReadWrite)
        {
            size_t   cbBuf = RT_MIN(_2M, cbMapping / 2);
            uint8_t *pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
            if (!pbBuf)
            {
                cbBuf = _4K;
                pbBuf = (uint8_t *)RTMemPageAlloc(cbBuf);
            }
            RTTESTI_CHECK(pbBuf != NULL);
            if (pbBuf)
            {
                /* Do a number of random writes to the file (using hFile1).
                   Immediately undoing them. */
                for (uint32_t i = 0; i < 128; i++)
                {
                    /* Generate a randomly sized write at a random location, making
                       sure it differs from whatever is there already before writing. */
                    uint32_t const cbToWrite  = RTRandU32Ex(1, (uint32_t)cbBuf);
                    uint64_t const offToWrite = RTRandU64Ex(0, cbMapping - cbToWrite);

                    fsPerfFillWriteBuf(offToWrite, pbBuf, cbToWrite, 0xf8);
                    pbBuf[0] = ~pbBuf[0];
                    if (cbToWrite > 1)
                        pbBuf[cbToWrite - 1] = ~pbBuf[cbToWrite - 1];
                    RTTESTI_CHECK_RC(RTFileWriteAt(hFile1, offToWrite, pbBuf, cbToWrite, NULL), VINF_SUCCESS);

                    /* Check the mapping. */
                    if (memcmp(&pbMapping[(size_t)offToWrite], pbBuf, cbToWrite) != 0)
                    {
                        RTTestIFailed("Write #%u @ %#RX64 LB %#x was not reflected in the mapping!\n", i, offToWrite, cbToWrite);
                    }

                    /* Restore */
                    fsPerfFillWriteBuf(offToWrite, pbBuf, cbToWrite, 0xf6);
                    RTTESTI_CHECK_RC(RTFileWriteAt(hFile1, offToWrite, pbBuf, cbToWrite, NULL), VINF_SUCCESS);
                }

                RTMemPageFree(pbBuf, cbBuf);
            }
        }

        /*
         * Unmap it.
         */
# ifdef RT_OS_WINDOWS
        RTTESTI_CHECK(UnmapViewOfFile(pbMapping));
        RTTESTI_CHECK(CloseHandle(hSection));
# else
        RTTESTI_CHECK(munmap(pbMapping, cbMapping) == 0);
# endif
    }

    /*
     * Memory mappings without open handles (pretty common).
     */
    for (uint32_t i = 0; i < 32; i++)
    {
        /* Create a new file, 256 KB in size, and fill it with random bytes.
           Try uncached access if we can to force the page-in to do actual reads. */
        char szFile2[RTPATH_MAX + 32];
        memcpy(szFile2, g_szDir, g_cchDir);
        RTStrPrintf(&szFile2[g_cchDir], sizeof(szFile2) - g_cchDir, "mmap-%u.noh", i);
        RTFILE hFile2 = NIL_RTFILE;
        int rc = (i & 3) == 3 ? VERR_TRY_AGAIN
               : RTFileOpen(&hFile2, szFile2, RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_NO_CACHE);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC_BREAK(RTFileOpen(&hFile2, szFile2, RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE),
                                   VINF_SUCCESS);
        }

        static char s_abContent[256*1024];
        RTRandBytes(s_abContent, sizeof(s_abContent));
        RTTESTI_CHECK_RC(RTFileWrite(hFile2, s_abContent, sizeof(s_abContent), NULL), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);

        /* Reopen the file with normal caching.  Every second time, we also
           does a read-only open of it to confuse matters. */
        RTFILE hFile3 = NIL_RTFILE;
        if ((i & 3) == 3)
            RTTESTI_CHECK_RC(RTFileOpen(&hFile3, szFile2, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE), VINF_SUCCESS);
        hFile2 = NIL_RTFILE;
        RTTESTI_CHECK_RC_BREAK(RTFileOpen(&hFile2, szFile2, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE),
                               VINF_SUCCESS);
        if ((i & 3) == 1)
            RTTESTI_CHECK_RC(RTFileOpen(&hFile3, szFile2, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE), VINF_SUCCESS);

        /* Memory map it read-write (no COW). */
#ifdef RT_OS_WINDOWS
        HANDLE hSection = CreateFileMapping((HANDLE)RTFileToNative(hFile2), NULL, PAGE_READWRITE, 0, sizeof(s_abContent), NULL);
        RTTESTI_CHECK_MSG(hSection  != NULL, ("last error %u\n", GetLastError));
        uint8_t *pbMapping = (uint8_t *)MapViewOfFile(hSection, FILE_MAP_WRITE, 0, 0, sizeof(s_abContent));
        RTTESTI_CHECK_MSG(pbMapping != NULL, ("last error %u\n", GetLastError));
        RTTESTI_CHECK_MSG(CloseHandle(hSection), ("last error %u\n", GetLastError));
# else
        uint8_t *pbMapping = (uint8_t *)mmap(NULL, sizeof(s_abContent), PROT_READ | PROT_WRITE, MAP_SHARED,
                                             (int)RTFileToNative(hFile2), 0);
        if ((void *)pbMapping == MAP_FAILED)
            pbMapping = NULL;
        RTTESTI_CHECK_MSG(pbMapping != NULL, ("errno=%s (%d)\n", strerror(errno), errno));
# endif

        /* Close the file handles. */
        if ((i & 7) == 7)
        {
            RTTESTI_CHECK_RC(RTFileClose(hFile3), VINF_SUCCESS);
            hFile3 = NIL_RTFILE;
        }
        RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
        if ((i & 7) == 5)
        {
            RTTESTI_CHECK_RC(RTFileClose(hFile3), VINF_SUCCESS);
            hFile3 = NIL_RTFILE;
        }
        if (pbMapping)
        {
            RTThreadSleep(2); /* fudge for cleanup/whatever */

            /* Page in the mapping by comparing with the content we wrote above. */
            RTTESTI_CHECK(memcmp(pbMapping, s_abContent, sizeof(s_abContent)) == 0);

            /* Now dirty everything by inverting everything. */
            size_t *puCur = (size_t *)pbMapping;
            size_t  cLeft = sizeof(s_abContent) / sizeof(*puCur);
            while (cLeft-- > 0)
            {
                *puCur = ~*puCur;
                puCur++;
            }

            /* Sync it all. */
#  ifdef RT_OS_WINDOWS
            RTTESTI_CHECK(FlushViewOfFile(pbMapping, sizeof(s_abContent)));
#  else
            RTTESTI_CHECK(msync(pbMapping, sizeof(s_abContent), MS_SYNC) == 0);
#  endif

            /* Unmap it. */
# ifdef RT_OS_WINDOWS
            RTTESTI_CHECK(UnmapViewOfFile(pbMapping));
# else
            RTTESTI_CHECK(munmap(pbMapping, sizeof(s_abContent)) == 0);
# endif
        }

        if (hFile3 != NIL_RTFILE)
            RTTESTI_CHECK_RC(RTFileClose(hFile3), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileDelete(szFile2), VINF_SUCCESS);
    }


#else
    RTTestSkipped(g_hTest,  "not supported/implemented");
    RT_NOREF(hFile1, hFileNoCache, cbFile);
#endif
}


/**
 * This does the read, write and seek tests.
 */
void fsPerfIo(void)
{
    RTTestISub("I/O");

    /*
     * Determin the size of the test file.
     */
    g_szDir[g_cchDir] = '\0';
    RTFOFF cbFree = 0;
    RTTESTI_CHECK_RC_RETV(RTFsQuerySizes(g_szDir, NULL, &cbFree, NULL, NULL), VINF_SUCCESS);
    uint64_t cbFile = g_cbIoFile;
    if (cbFile + _16M < (uint64_t)cbFree)
        cbFile = RT_ALIGN_64(cbFile, _64K);
    else if (cbFree < _32M)
    {
        RTTestSkipped(g_hTest, "Insufficent free space: %'RU64 bytes, requires >= 32MB", cbFree);
        return;
    }
    else
    {
        cbFile = cbFree - (cbFree > _128M ? _64M : _16M);
        cbFile = RT_ALIGN_64(cbFile, _64K);
        RTTestIPrintf(RTTESTLVL_ALWAYS,  "Adjusted file size to %'RU64 bytes, due to %'RU64 bytes free.\n", cbFile, cbFree);
    }
    if (cbFile < _64K)
    {
        RTTestSkipped(g_hTest, "Specified test file size too small: %'RU64 bytes, requires >= 64KB", cbFile);
        return;
    }

    /*
     * Create a cbFile sized test file.
     */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file21")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE), VINF_SUCCESS);
    RTFILE hFileNoCache;
    if (!g_fIgnoreNoCache)
        RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFileNoCache, g_szDir,
                                         RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE | RTFILE_O_NO_CACHE),
                              VINF_SUCCESS);
    else
    {
        int rc = RTFileOpen(&hFileNoCache, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE | RTFILE_O_NO_CACHE);
        if (RT_FAILURE(rc))
        {
            RTTestIPrintf(RTTESTLVL_ALWAYS, "Unable to open I/O file with non-cache flag (%Rrc), skipping related tests.\n", rc);
            hFileNoCache = NIL_RTFILE;
        }
    }
    RTFILE hFileWriteThru;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFileWriteThru, g_szDir,
                                     RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE | RTFILE_O_WRITE_THROUGH),
                          VINF_SUCCESS);

    uint8_t *pbFree = NULL;
    int rc = fsPerfIoPrepFile(hFile1, cbFile, &pbFree);
    RTMemFree(pbFree);
    if (RT_SUCCESS(rc))
    {
        /*
         * Do the testing & profiling.
         */
        if (g_fSeek)
            fsPerfIoSeek(hFile1, cbFile);

        if (g_fReadTests)
            fsPerfRead(hFile1, hFileNoCache, cbFile);
        if (g_fReadPerf)
            for (unsigned i = 0; i < g_cIoBlocks; i++)
                fsPerfIoReadBlockSize(hFile1, cbFile, g_acbIoBlocks[i]);
#ifdef FSPERF_TEST_SENDFILE
        if (g_fSendFile)
            fsPerfSendFile(hFile1, cbFile);
#endif
#ifdef RT_OS_LINUX
        if (g_fSplice)
            fsPerfSpliceToPipe(hFile1, cbFile);
#endif
        if (g_fMMap)
            fsPerfMMap(hFile1, hFileNoCache, cbFile);

        /* This is destructive to the file content. */
        if (g_fWriteTests)
            fsPerfWrite(hFile1, hFileNoCache, hFileWriteThru, cbFile);
        if (g_fWritePerf)
            for (unsigned i = 0; i < g_cIoBlocks; i++)
                fsPerfIoWriteBlockSize(hFile1, cbFile, g_acbIoBlocks[i]);
#ifdef RT_OS_LINUX
        if (g_fSplice)
            fsPerfSpliceToFile(hFile1, cbFile);
#endif
        if (g_fFSync)
            fsPerfFSync(hFile1, cbFile);
    }

    RTTESTI_CHECK_RC(RTFileSetSize(hFile1, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    if (hFileNoCache != NIL_RTFILE || !g_fIgnoreNoCache)
        RTTESTI_CHECK_RC(RTFileClose(hFileNoCache), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFileWriteThru), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VINF_SUCCESS);
}


DECL_FORCE_INLINE(int) fsPerfCopyWorker1(const char *pszSrc, const char *pszDst)
{
    RTFileDelete(pszDst);
    return RTFileCopy(pszSrc, pszDst);
}


#ifdef RT_OS_LINUX
DECL_FORCE_INLINE(int) fsPerfCopyWorkerSendFile(RTFILE hFile1, RTFILE hFile2, size_t cbFile)
{
    RTTESTI_CHECK_RC_RET(RTFileSeek(hFile2, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS, rcCheck);

    loff_t off = 0;
    ssize_t cbSent = sendfile((int)RTFileToNative(hFile2), (int)RTFileToNative(hFile1), &off, cbFile);
    if (cbSent > 0 && (size_t)cbSent == cbFile)
        return 0;

    int rc = VERR_GENERAL_FAILURE;
    if (cbSent < 0)
    {
        rc = RTErrConvertFromErrno(errno);
        RTTestIFailed("sendfile(file,file,NULL,%#zx) failed (%zd): %d (%Rrc)", cbFile, cbSent, errno, rc);
    }
    else
        RTTestIFailed("sendfile(file,file,NULL,%#zx) returned %#zx, expected %#zx (diff %zd)",
                      cbFile, cbSent, cbFile, cbSent - cbFile);
    return rc;
}
#endif /* RT_OS_LINUX */


static void fsPerfCopy(void)
{
    RTTestISub("copy");

    /*
     * Non-existing files.
     */
    RTTESTI_CHECK_RC(RTFileCopy(InEmptyDir(RT_STR_TUPLE("no-such-file")),
                                InDir2(RT_STR_TUPLE("whatever"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileCopy(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")),
                                InDir2(RT_STR_TUPLE("no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileCopy(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file")),
                                InDir2(RT_STR_TUPLE("whatever"))), VERR_PATH_NOT_FOUND);

    RTTESTI_CHECK_RC(RTFileCopy(InDir(RT_STR_TUPLE("known-file")),
                                InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileCopy(InDir(RT_STR_TUPLE("known-file")),
                                InDir2(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);

    /*
     * Determin the size of the test file.
     * We want to be able to make 1 copy of it.
     */
    g_szDir[g_cchDir] = '\0';
    RTFOFF cbFree = 0;
    RTTESTI_CHECK_RC_RETV(RTFsQuerySizes(g_szDir, NULL, &cbFree, NULL, NULL), VINF_SUCCESS);
    uint64_t cbFile = g_cbIoFile;
    if (cbFile + _16M < (uint64_t)cbFree)
        cbFile = RT_ALIGN_64(cbFile, _64K);
    else if (cbFree < _32M)
    {
        RTTestSkipped(g_hTest, "Insufficent free space: %'RU64 bytes, requires >= 32MB", cbFree);
        return;
    }
    else
    {
        cbFile = cbFree - (cbFree > _128M ? _64M : _16M);
        cbFile = RT_ALIGN_64(cbFile, _64K);
        RTTestIPrintf(RTTESTLVL_ALWAYS,  "Adjusted file size to %'RU64 bytes, due to %'RU64 bytes free.\n", cbFile, cbFree);
    }
    if (cbFile < _512K * 2)
    {
        RTTestSkipped(g_hTest, "Specified test file size too small: %'RU64 bytes, requires >= 1MB", cbFile);
        return;
    }
    cbFile /= 2;

    /*
     * Create a cbFile sized test file.
     */
    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file22")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_READWRITE), VINF_SUCCESS);
    uint8_t *pbFree = NULL;
    int rc = fsPerfIoPrepFile(hFile1, cbFile, &pbFree);
    RTMemFree(pbFree);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        /*
         * Make copies.
         */
        /* plain */
        RTFileDelete(InDir2(RT_STR_TUPLE("file23")));
        RTTESTI_CHECK_RC(RTFileCopy(g_szDir, g_szDir2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileCopy(g_szDir, g_szDir2), VERR_ALREADY_EXISTS);
        RTTESTI_CHECK_RC(RTFileCompare(g_szDir, g_szDir2), VINF_SUCCESS);

        /* by handle */
        hFile1 = NIL_RTFILE;
        RTTESTI_CHECK_RC(RTFileOpen(&hFile1, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS);
        RTFILE hFile2 = NIL_RTFILE;
        RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileCopyByHandles(hFile1, hFile2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileCompare(g_szDir, g_szDir2), VINF_SUCCESS);

        /* copy part */
        hFile1 = NIL_RTFILE;
        RTTESTI_CHECK_RC(RTFileOpen(&hFile1, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS);
        hFile2 = NIL_RTFILE;
        RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileCopyPart(hFile1, 0, hFile2, 0, cbFile / 2, 0, NULL), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileCopyPart(hFile1, cbFile / 2, hFile2, cbFile / 2, cbFile - cbFile / 2, 0, NULL), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileCompare(g_szDir, g_szDir2), VINF_SUCCESS);

#ifdef RT_OS_LINUX
        /*
         * On linux we can also use sendfile between two files, except for 2.5.x to 2.6.33.
         */
        uint64_t const cbFileMax = RT_MIN(cbFile, UINT32_C(0x7ffff000));
        char szRelease[64];
        RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szRelease, sizeof(szRelease));
        bool const fSendFileBetweenFiles = RTStrVersionCompare(szRelease, "2.5.0") < 0
                                        || RTStrVersionCompare(szRelease, "2.6.33") >= 0;
        if (fSendFileBetweenFiles)
        {
            /* Copy the whole file: */
            hFile1 = NIL_RTFILE;
            RTTESTI_CHECK_RC(RTFileOpen(&hFile1, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS);
            RTFileDelete(g_szDir2);
            hFile2 = NIL_RTFILE;
            RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
            ssize_t cbSent = sendfile((int)RTFileToNative(hFile2), (int)RTFileToNative(hFile1), NULL, cbFile);
            if (cbSent < 0)
                RTTestIFailed("sendfile(file,file,NULL,%#zx) failed (%zd): %d (%Rrc)",
                              cbFile, cbSent, errno, RTErrConvertFromErrno(errno));
            else if ((size_t)cbSent != cbFileMax)
                RTTestIFailed("sendfile(file,file,NULL,%#zx) returned %#zx, expected %#zx (diff %zd)",
                              cbFile, cbSent, cbFileMax, cbSent - cbFileMax);
            RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTFileCompare(g_szDir, g_szDir2), VINF_SUCCESS);

            /* Try copy a little bit too much: */
            if (cbFile == cbFileMax)
            {
                hFile1 = NIL_RTFILE;
                RTTESTI_CHECK_RC(RTFileOpen(&hFile1, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS);
                RTFileDelete(g_szDir2);
                hFile2 = NIL_RTFILE;
                RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
                size_t cbToCopy = cbFile + RTRandU32Ex(1, _64M);
                cbSent = sendfile((int)RTFileToNative(hFile2), (int)RTFileToNative(hFile1), NULL, cbToCopy);
                if (cbSent < 0)
                    RTTestIFailed("sendfile(file,file,NULL,%#zx) failed (%zd): %d (%Rrc)",
                                  cbToCopy, cbSent, errno, RTErrConvertFromErrno(errno));
                else if ((size_t)cbSent != cbFile)
                    RTTestIFailed("sendfile(file,file,NULL,%#zx) returned %#zx, expected %#zx (diff %zd)",
                                  cbToCopy, cbSent, cbFile, cbSent - cbFile);
                RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTFileCompare(g_szDir, g_szDir2), VINF_SUCCESS);
            }

            /* Do partial copy: */
            hFile2 = NIL_RTFILE;
            RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
            for (uint32_t i = 0; i < 64; i++)
            {
                size_t cbToCopy = RTRandU32Ex(0, cbFileMax - 1);
                uint32_t const offFile  = RTRandU32Ex(1, (uint64_t)RT_MIN(cbFileMax - cbToCopy, UINT32_MAX));
                RTTESTI_CHECK_RC_BREAK(RTFileSeek(hFile2, offFile, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
                loff_t offFile2 = offFile;
                cbSent = sendfile((int)RTFileToNative(hFile2), (int)RTFileToNative(hFile1), &offFile2, cbToCopy);
                if (cbSent < 0)
                    RTTestIFailed("sendfile(file,file,%#x,%#zx) failed (%zd): %d (%Rrc)",
                                  offFile, cbToCopy, cbSent, errno, RTErrConvertFromErrno(errno));
                else if ((size_t)cbSent != cbToCopy)
                    RTTestIFailed("sendfile(file,file,%#x,%#zx) returned %#zx, expected %#zx (diff %zd)",
                                  offFile, cbToCopy, cbSent, cbToCopy, cbSent - cbToCopy);
                else if (offFile2 != (loff_t)(offFile + cbToCopy))
                    RTTestIFailed("sendfile(file,file,%#x,%#zx) returned %#zx + off=%#RX64, expected off %#x",
                                  offFile, cbToCopy, cbSent, offFile2, offFile + cbToCopy);
            }
            RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTFileCompare(g_szDir, g_szDir2), VINF_SUCCESS);
        }
#endif

        /*
         * Do some benchmarking.
         */
#define PROFILE_COPY_FN(a_szOperation, a_fnCall) \
            do \
            { \
                /* Estimate how many iterations we need to fill up the given timeslot: */ \
                fsPerfYield(); \
                uint64_t nsStart = RTTimeNanoTS(); \
                uint64_t ns; \
                do \
                    ns = RTTimeNanoTS(); \
                while (ns == nsStart); \
                nsStart = ns; \
                \
                uint64_t iIteration = 0; \
                do \
                { \
                    RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
                    iIteration++; \
                    ns = RTTimeNanoTS() - nsStart; \
                } while (ns < RT_NS_10MS); \
                ns /= iIteration; \
                if (ns > g_nsPerNanoTSCall + 32) \
                    ns -= g_nsPerNanoTSCall; \
                uint64_t cIterations = g_nsTestRun / ns; \
                if (cIterations < 2) \
                    cIterations = 2; \
                else if (cIterations & 1) \
                    cIterations++; \
                \
                /* Do the actual profiling: */ \
                iIteration = 0; \
                fsPerfYield(); \
                nsStart = RTTimeNanoTS(); \
                for (uint32_t iAdjust = 0; iAdjust < 4; iAdjust++) \
                { \
                    for (; iIteration < cIterations; iIteration++)\
                        RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
                    ns = RTTimeNanoTS() - nsStart;\
                    if (ns >= g_nsTestRun - (g_nsTestRun / 10)) \
                        break; \
                    cIterations += cIterations / 4; \
                    if (cIterations & 1) \
                        cIterations++; \
                    nsStart += g_nsPerNanoTSCall; \
                } \
                RTTestIValueF(ns / iIteration, \
                              RTTESTUNIT_NS_PER_OCCURRENCE, a_szOperation " latency"); \
                RTTestIValueF((uint64_t)((uint64_t)iIteration * cbFile / ((double)ns / RT_NS_1SEC)), \
                              RTTESTUNIT_BYTES_PER_SEC,     a_szOperation " throughput"); \
                RTTestIValueF((uint64_t)iIteration * cbFile, \
                              RTTESTUNIT_BYTES,             a_szOperation " bytes"); \
                RTTestIValueF(iIteration, \
                              RTTESTUNIT_OCCURRENCES,       a_szOperation " iterations"); \
                if (g_fShowDuration) \
                    RTTestIValueF(ns, RTTESTUNIT_NS,        a_szOperation " duration"); \
            } while (0)

        PROFILE_COPY_FN("RTFileCopy/Replace",           fsPerfCopyWorker1(g_szDir, g_szDir2));

        hFile1 = NIL_RTFILE;
        RTTESTI_CHECK_RC(RTFileOpen(&hFile1, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS);
        RTFileDelete(g_szDir2);
        hFile2 = NIL_RTFILE;
        RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
        PROFILE_COPY_FN("RTFileCopyByHandles/Overwrite", RTFileCopyByHandles(hFile1, hFile2));
        RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);

        /* We could benchmark RTFileCopyPart with various block sizes and whatnot...
           But it's currently well covered by the two previous operations. */

#ifdef RT_OS_LINUX
        if (fSendFileBetweenFiles)
        {
            hFile1 = NIL_RTFILE;
            RTTESTI_CHECK_RC(RTFileOpen(&hFile1, g_szDir, RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VINF_SUCCESS);
            RTFileDelete(g_szDir2);
            hFile2 = NIL_RTFILE;
            RTTESTI_CHECK_RC(RTFileOpen(&hFile2, g_szDir2, RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
            PROFILE_COPY_FN("sendfile/overwrite", fsPerfCopyWorkerSendFile(hFile1, hFile2, cbFileMax));
            RTTESTI_CHECK_RC(RTFileClose(hFile2), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
        }
#endif
    }

    /*
     * Clean up.
     */
    RTFileDelete(InDir2(RT_STR_TUPLE("file22c1")));
    RTFileDelete(InDir2(RT_STR_TUPLE("file22c2")));
    RTFileDelete(InDir2(RT_STR_TUPLE("file22c3")));
    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VINF_SUCCESS);
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
            case 'e':                           pszHelp = "Enables all tests.                           default: -e"; break;
            case 'z':                           pszHelp = "Disables all tests.                          default: -e"; break;
            case 's':                           pszHelp = "Set benchmark duration in seconds.           default: 10 sec"; break;
            case 'm':                           pszHelp = "Set benchmark duration in milliseconds.      default: 10000 ms"; break;
            case 'v':                           pszHelp = "More verbose execution."; break;
            case 'q':                           pszHelp = "Quiet execution."; break;
            case 'h':                           pszHelp = "Displays this help and exit"; break;
            case 'V':                           pszHelp = "Displays the program revision"; break;
            case kCmdOpt_ShowDuration:          pszHelp = "Show duration of profile runs.               default: --no-show-duration"; break;
            case kCmdOpt_NoShowDuration:        pszHelp = "Hide duration of profile runs.               default: --no-show-duration"; break;
            case kCmdOpt_ShowIterations:        pszHelp = "Show iteration count for profile runs.       default: --no-show-iterations"; break;
            case kCmdOpt_NoShowIterations:      pszHelp = "Hide iteration count for profile runs.       default: --no-show-iterations"; break;
            case kCmdOpt_ManyFiles:             pszHelp = "Count of files in big test dir.              default: --many-files 10000"; break;
            case kCmdOpt_NoManyFiles:           pszHelp = "Skip big test dir with many files.           default: --many-files 10000"; break;
            case kCmdOpt_ManyTreeFilesPerDir:   pszHelp = "Count of files per directory in test tree.   default: 640"; break;
            case kCmdOpt_ManyTreeSubdirsPerDir: pszHelp = "Count of subdirs per directory in test tree. default: 16"; break;
            case kCmdOpt_ManyTreeDepth:         pszHelp = "Depth of test tree (not counting root).      default: 1"; break;
            case kCmdOpt_IgnoreNoCache:         pszHelp = "Ignore error wrt no-cache handle.            default: --no-ignore-no-cache"; break;
            case kCmdOpt_NoIgnoreNoCache:       pszHelp = "Do not ignore error wrt no-cache handle.     default: --no-ignore-no-cache"; break;
            case kCmdOpt_IoFileSize:            pszHelp = "Size of file used for I/O tests.             default: 512 MB"; break;
            case kCmdOpt_SetBlockSize:          pszHelp = "Sets single I/O block size (in bytes)."; break;
            case kCmdOpt_AddBlockSize:          pszHelp = "Adds an I/O block size (in bytes)."; break;
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


static uint32_t fsPerfCalcManyTreeFiles(void)
{
    uint32_t cDirs = 1;
    for (uint32_t i = 0, cDirsAtLevel = 1; i < g_cManyTreeDepth; i++)
    {
        cDirs += cDirsAtLevel * g_cManyTreeSubdirsPerDir;
        cDirsAtLevel *= g_cManyTreeSubdirsPerDir;
    }
    return g_cManyTreeFilesPerDir * cDirs;
}


int main(int argc, char *argv[])
{
    /*
     * Init IPRT and globals.
     */
    int rc = RTTestInitAndCreate("FsPerf", &g_hTest);
    if (rc)
        return rc;
    RTListInit(&g_ManyTreeHead);

    /*
     * Default values.
     */
    rc = RTPathGetCurrent(g_szDir, sizeof(g_szDir) / 2);
    if (RT_SUCCESS(rc))
        rc = RTPathAppend(g_szDir, sizeof(g_szDir) / 2, "fstestdir-");
    if (RT_SUCCESS(rc))
    {
        g_cchDir = strlen(g_szDir);
        g_cchDir += RTStrPrintf(&g_szDir[g_cchDir], sizeof(g_szDir) - g_cchDir, "%u" RTPATH_SLASH_STR, RTProcSelf());
    }
    else
    {
        RTTestFailed(g_hTest, "RTPathGetCurrent (or RTPathAppend) failed: %Rrc\n", rc);
        return RTTestSummaryAndDestroy(g_hTest);
    }

    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, g_aCmdOptions, RT_ELEMENTS(g_aCmdOptions), 1, 0 /* fFlags */);
    while ((rc = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'd':
                rc = RTPathAbs(ValueUnion.psz, g_szDir, sizeof(g_szDir) / 2);
                if (RT_SUCCESS(rc))
                {
                    RTPathEnsureTrailingSeparator(g_szDir, sizeof(g_szDir));
                    g_cchDir = strlen(g_szDir);
                    break;
                }
                RTTestFailed(g_hTest, "RTPathAbs(%s) failed: %Rrc\n", ValueUnion.psz, rc);
                return RTTestSummaryAndDestroy(g_hTest);

            case 's':
                if (ValueUnion.u32 == 0)
                    g_nsTestRun = RT_NS_1SEC_64 * 10;
                else
                    g_nsTestRun = ValueUnion.u32 * RT_NS_1SEC_64;
                break;

            case 'm':
                if (ValueUnion.u64 == 0)
                    g_nsTestRun = RT_NS_1SEC_64 * 10;
                else
                    g_nsTestRun = ValueUnion.u64 * RT_NS_1MS;
                break;

            case 'e':
                g_fManyFiles = true;
                g_fOpen      = true;
                g_fFStat     = true;
                g_fFChMod    = true;
                g_fFUtimes   = true;
                g_fStat      = true;
                g_fChMod     = true;
                g_fUtimes    = true;
                g_fRename    = true;
                g_fDirOpen   = true;
                g_fDirEnum   = true;
                g_fMkRmDir   = true;
                g_fStatVfs   = true;
                g_fRm        = true;
                g_fChSize    = true;
                g_fReadTests = true;
                g_fReadPerf  = true;
                g_fSendFile  = true;
#ifdef RT_OS_LINUX
                g_fSplice    = true;
#endif
                g_fWriteTests= true;
                g_fWritePerf = true;
                g_fSeek      = true;
                g_fFSync     = true;
                g_fMMap      = true;
                g_fCopy      = true;
                break;

            case 'z':
                g_fManyFiles = false;
                g_fOpen      = false;
                g_fFStat     = false;
                g_fFChMod    = false;
                g_fFUtimes   = false;
                g_fStat      = false;
                g_fChMod     = false;
                g_fUtimes    = false;
                g_fRename    = false;
                g_fDirOpen   = false;
                g_fDirEnum   = false;
                g_fMkRmDir   = false;
                g_fStatVfs   = false;
                g_fRm        = false;
                g_fChSize    = false;
                g_fReadTests = false;
                g_fReadPerf  = false;
                g_fSendFile  = false;
#ifdef RT_OS_LINUX
                g_fSplice    = false;
#endif
                g_fWriteTests= false;
                g_fWritePerf = false;
                g_fSeek      = false;
                g_fFSync     = false;
                g_fMMap      = false;
                g_fCopy      = false;
                break;

#define CASE_OPT(a_Stem) \
            case RT_CONCAT(kCmdOpt_,a_Stem):   RT_CONCAT(g_f,a_Stem) = true; break; \
            case RT_CONCAT(kCmdOpt_No,a_Stem): RT_CONCAT(g_f,a_Stem) = false; break
            CASE_OPT(Open);
            CASE_OPT(FStat);
            CASE_OPT(FChMod);
            CASE_OPT(FUtimes);
            CASE_OPT(Stat);
            CASE_OPT(ChMod);
            CASE_OPT(Utimes);
            CASE_OPT(Rename);
            CASE_OPT(DirOpen);
            CASE_OPT(DirEnum);
            CASE_OPT(MkRmDir);
            CASE_OPT(StatVfs);
            CASE_OPT(Rm);
            CASE_OPT(ChSize);
            CASE_OPT(ReadTests);
            CASE_OPT(ReadPerf);
            CASE_OPT(SendFile);
#ifdef RT_OS_LINUX
            CASE_OPT(Splice);
#endif
            CASE_OPT(WriteTests);
            CASE_OPT(WritePerf);
            CASE_OPT(Seek);
            CASE_OPT(FSync);
            CASE_OPT(MMap);
            CASE_OPT(IgnoreNoCache);
            CASE_OPT(Copy);

            CASE_OPT(ShowDuration);
            CASE_OPT(ShowIterations);
#undef CASE_OPT

            case kCmdOpt_ManyFiles:
                g_fManyFiles = ValueUnion.u32 > 0;
                g_cManyFiles = ValueUnion.u32;
                break;

            case kCmdOpt_NoManyFiles:
                g_fManyFiles = false;
                break;

            case kCmdOpt_ManyTreeFilesPerDir:
                if (ValueUnion.u32 > 0 && ValueUnion.u32 <= _64M)
                {
                    g_cManyTreeFilesPerDir = ValueUnion.u32;
                    g_cManyTreeFiles = fsPerfCalcManyTreeFiles();
                    break;
                }
                RTTestFailed(g_hTest, "Out of range --files-per-dir value: %u (%#x)\n", ValueUnion.u32, ValueUnion.u32);
                return RTTestSummaryAndDestroy(g_hTest);

            case kCmdOpt_ManyTreeSubdirsPerDir:
                if (ValueUnion.u32 > 0 && ValueUnion.u32 <= 1024)
                {
                    g_cManyTreeSubdirsPerDir = ValueUnion.u32;
                    g_cManyTreeFiles = fsPerfCalcManyTreeFiles();
                    break;
                }
                RTTestFailed(g_hTest, "Out of range --subdirs-per-dir value: %u (%#x)\n", ValueUnion.u32, ValueUnion.u32);
                return RTTestSummaryAndDestroy(g_hTest);

            case kCmdOpt_ManyTreeDepth:
                if (ValueUnion.u32 <= 8)
                {
                    g_cManyTreeDepth = ValueUnion.u32;
                    g_cManyTreeFiles = fsPerfCalcManyTreeFiles();
                    break;
                }
                RTTestFailed(g_hTest, "Out of range --tree-depth value: %u (%#x)\n", ValueUnion.u32, ValueUnion.u32);
                return RTTestSummaryAndDestroy(g_hTest);

            case kCmdOpt_IoFileSize:
                if (ValueUnion.u64 == 0)
                    g_cbIoFile = _512M;
                else
                    g_cbIoFile = ValueUnion.u64;
                break;

            case kCmdOpt_SetBlockSize:
                if (ValueUnion.u32 > 0)
                {
                    g_cIoBlocks = 1;
                    g_acbIoBlocks[0] = ValueUnion.u32;
                }
                else
                {
                    RTTestFailed(g_hTest, "Invalid I/O block size: %u (%#x)\n", ValueUnion.u32, ValueUnion.u32);
                    return RTTestSummaryAndDestroy(g_hTest);
                }
                break;

            case kCmdOpt_AddBlockSize:
                if (g_cIoBlocks >= RT_ELEMENTS(g_acbIoBlocks))
                    RTTestFailed(g_hTest, "Too many I/O block sizes: max %u\n", RT_ELEMENTS(g_acbIoBlocks));
                else if (ValueUnion.u32 == 0)
                    RTTestFailed(g_hTest, "Invalid I/O block size: %u (%#x)\n", ValueUnion.u32, ValueUnion.u32);
                else
                {
                    g_acbIoBlocks[g_cIoBlocks++] = ValueUnion.u32;
                    break;
                }
                return RTTestSummaryAndDestroy(g_hTest);

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
            rc = fsPrepTestArea();
            if (RT_SUCCESS(rc))
            {
                /* Profile RTTimeNanoTS(). */
                fsPerfNanoTS();

                /* Do tests: */
                if (g_fManyFiles)
                    fsPerfManyFiles();
                if (g_fOpen)
                    fsPerfOpen();
                if (g_fFStat)
                    fsPerfFStat();
                if (g_fFChMod)
                    fsPerfFChMod();
                if (g_fFUtimes)
                    fsPerfFUtimes();
                if (g_fStat)
                    fsPerfStat();
                if (g_fChMod)
                    fsPerfChmod();
                if (g_fUtimes)
                    fsPerfUtimes();
                if (g_fRename)
                    fsPerfRename();
                if (g_fDirOpen)
                    vsPerfDirOpen();
                if (g_fDirEnum)
                    vsPerfDirEnum();
                if (g_fMkRmDir)
                    fsPerfMkRmDir();
                if (g_fStatVfs)
                    fsPerfStatVfs();
                if (g_fRm || g_fManyFiles)
                    fsPerfRm(); /* deletes manyfiles and manytree */
                if (g_fChSize)
                    fsPerfChSize();
                if (   g_fReadPerf || g_fReadTests || g_fSendFile || g_fWritePerf || g_fWriteTests
#ifdef RT_OS_LINUX
                    || g_fSplice
#endif
                    || g_fSeek || g_fFSync || g_fMMap)
                    fsPerfIo();
                if (g_fCopy)
                    fsPerfCopy();
            }

            /* Cleanup: */
            g_szDir[g_cchDir] = '\0';
            rc = RTDirRemoveRecursive(g_szDir, RTDIRRMREC_F_CONTENT_AND_DIR);
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

