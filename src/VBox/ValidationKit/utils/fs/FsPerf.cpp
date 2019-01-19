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
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/test.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/zero.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
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
        } while (ns < RT_NS_10MS || (iIteration & 1)); \
        ns /= iIteration; \
        if (ns > g_nsPerNanoTSCall + 32) \
            ns -= g_nsPerNanoTSCall; \
        \
        uint64_t cIterations = (a_cNsTarget) / ns; \
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
        ns = RTTimeNanoTS() - nsStart; \
        RTTestIValueF(ns / cIterations, RTTESTUNIT_NS_PER_OCCURRENCE, a_szDesc); \
        if (g_fShowDuration) \
            RTTestIValueF(ns, RTTESTUNIT_NS, "%s duration", a_szDesc); \
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
    kCmdOpt_Read,
    kCmdOpt_NoRead,
    kCmdOpt_Write,
    kCmdOpt_NoWrite,
    kCmdOpt_Seek,
    kCmdOpt_NoSeek,

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

    { "--many-files",       kCmdOpt_ManyFiles,      RTGETOPT_REQ_NOTHING },
    { "--no-many-files",    kCmdOpt_NoManyFiles,    RTGETOPT_REQ_NOTHING },

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
    { "--read",             kCmdOpt_Read,           RTGETOPT_REQ_NOTHING },
    { "--no-read",          kCmdOpt_NoRead,         RTGETOPT_REQ_NOTHING },
    { "--write",            kCmdOpt_Write,          RTGETOPT_REQ_NOTHING },
    { "--no-write",         kCmdOpt_NoWrite,        RTGETOPT_REQ_NOTHING },
    { "--seek",             kCmdOpt_Seek,           RTGETOPT_REQ_NOTHING },
    { "--no-seek",          kCmdOpt_NoSeek,         RTGETOPT_REQ_NOTHING },

    { "--quiet",            'q', RTGETOPT_REQ_NOTHING },
    { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
    { "--version",          'V', RTGETOPT_REQ_NOTHING },
    { "--help",             'h', RTGETOPT_REQ_NOTHING } /* for Usage() */
};

/** The test handle. */
static RTTEST       g_hTest;
/** The number of nanoseconds a RTTimeNanoTS call takes.
 * This is used for adjusting loop count estimates.  */
static uint64_t     g_nsPerNanoTSCall = 1;
/** Whether or not to display the duration of each profile run.
 * This is chiefly for verify the estimate phase.  */
static bool         g_fShowDuration = true;
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
static bool         g_fDirEnum   = true;
static bool         g_fMkRmDir   = true;
static bool         g_fStatVfs   = true;
static bool         g_fRm        = true;
static bool         g_fChSize    = true;
static bool         g_fSeek      = true;
static bool         g_fRead      = true;
static bool         g_fWrite     = true;
/** @} */

/** The length of each test run. */
static uint64_t     g_nsTestRun                 = RT_NS_1SEC_64 * 10;

/** For the 'manyfiles' subdir.  */
static uint32_t     g_cManyFiles                = 10000;

/** Number of files in the 'manytree' directory tree.  */
static uint32_t     g_cManyTreeFiles            = 640 + 16*640 /*10880*/;
/** Number of files per directory in the 'manytree' construct. */
static uint32_t     g_cManyTreeFilesPerDir      = 640;
/* Number of subdirs per directory in the 'manytree' construct. */
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

/** The length of g_szDir. */
static size_t       g_cchDir;
/** The length of g_szEmptyDir. */
static size_t       g_cchEmptyDir;
/** The length of g_szDeepDir. */
static size_t       g_cchDeepDir;

/** The test directory (absolute).  This will always have a trailing slash. */
static char         g_szDir[RTPATH_MAX];
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
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo1.AccessTime) == RTTimeSpecGetNano(&ObjInfo0.AccessTime));

    /* Modify access time: */
    RTTESTI_CHECK_RC(RTFileSetTimes(hFile1, &Time1, NULL, NULL, NULL), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo2 = {0};
    RTTESTI_CHECK_RC(RTFileQueryInfo(hFile1, &ObjInfo2,  RTFSOBJATTRADD_NOTHING), VINF_SUCCESS);
    RTTESTI_CHECK((RTTimeSpecGetSeconds(&ObjInfo2.AccessTime) >> 2) == (RTTimeSpecGetSeconds(&Time1) >> 2));
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo2.ModificationTime) == RTTimeSpecGetNano(&ObjInfo1.ModificationTime));

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
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo1.AccessTime) == RTTimeSpecGetNano(&ObjInfo0.AccessTime));

    /* Modify access time: */
    RTTESTI_CHECK_RC(RTPathSetTimesEx(g_szDir, &Time1, NULL, NULL, NULL, RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTFSOBJINFO ObjInfo2 = {0};
    RTTESTI_CHECK_RC(RTPathQueryInfoEx(g_szDir, &ObjInfo2,  RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK), VINF_SUCCESS);
    RTTESTI_CHECK((RTTimeSpecGetSeconds(&ObjInfo2.AccessTime) >> 2) == (RTTimeSpecGetSeconds(&Time1) >> 2));
    RTTESTI_CHECK(RTTimeSpecGetNano(&ObjInfo2.ModificationTime) == RTTimeSpecGetNano(&ObjInfo1.ModificationTime));

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

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTDirOpen(&hDir, InEmptyDir(RT_STR_TUPLE("no-such-file"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirOpen(&hDir, InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTDirOpen(&hDir, InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);

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
#if defined(RT_OS_WINDOWS)
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE("."))), VERR_DIR_NOT_EMPTY);
#else
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE("."))), VERR_INVALID_PARAMETER); /* EINVAL for '.' */
#endif
#if defined(RT_OS_WINDOWS)
    RTTESTI_CHECK_RC(RTDirRemove(InDir(RT_STR_TUPLE(".."))), VERR_SHARING_VIOLATION); /* weird */
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

}


void fsPerfRm(void)
{
    RTTestISub("rm");

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("no-such-file"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), FSPERF_VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileDelete(InDir(RT_STR_TUPLE("known-file" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);

    /* Directories: */
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("."))), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(".."))), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE(""))), VERR_ACCESS_DENIED);

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
        RTTestIValueF((uint64_t)iIteration * cbBlock / ((double)ns / RT_NS_1SEC), \
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
    RTTestISubF("Sequential read %RU32", cbBlock);

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
    RTTestISubF("Sequential write %RU32", cbBlock);

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
    else
    {
        if (cbFree < _32M)
        {
            RTTestSkipped(g_hTest, "Insufficent free space: %'RU64 bytes, requires >= 32MB", cbFree);
            return;
        }
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
        if (g_fRead)
        {
            for (unsigned i = 0; i < g_cIoBlocks; i++)
                fsPerfIoReadBlockSize(hFile1, cbFile, g_acbIoBlocks[i]);
        }
        if (g_fWrite)
        {
            for (unsigned i = 0; i < g_cIoBlocks; i++)
                fsPerfIoWriteBlockSize(hFile1, cbFile, g_acbIoBlocks[i]);
        }
    }

    RTTESTI_CHECK_RC(RTFileSetSize(hFile1, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileDelete(g_szDir), VINF_SUCCESS);
}


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
            case 'd':   pszHelp = "The directory to use for testing.  Default: CWD/fstestdir"; break;
            case 'e':   pszHelp = "Enables all tests.   Default: -e"; break;
            case 'z':   pszHelp = "Disables all tests.  Default: -e"; break;
            case 's':   pszHelp = "Set benchmark duration in seconds.  Default: 10 sec"; break;
            case 'm':   pszHelp = "Set benchmark duration in milliseconds.  Default: 10000 ms"; break;
            case 'v':   pszHelp = "More verbose execution."; break;
            case 'q':   pszHelp = "Quiet execution."; break;
            case 'h':   pszHelp = "Displays this help and exit"; break;
            case 'V':   pszHelp = "Displays the program revision"; break;
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
            RTStrmPrintf(pStrm, "  %-20s%s\n", szOpt, pszHelp);
        }
        else
            RTStrmPrintf(pStrm, "  %-20s%s\n", g_aCmdOptions[i].pszLong, pszHelp);
    }
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
                    break;
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
                g_fDirEnum   = true;
                g_fMkRmDir   = true;
                g_fStatVfs   = true;
                g_fRm        = true;
                g_fChSize    = true;
                g_fRead      = true;
                g_fWrite     = true;
                g_fSeek      = true;
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
                g_fDirEnum   = false;
                g_fMkRmDir   = false;
                g_fStatVfs   = false;
                g_fRm        = false;
                g_fChSize    = false;
                g_fRead      = false;
                g_fWrite     = false;
                g_fSeek      = false;
                break;

#define CASE_OPT(a_Stem) \
            case RT_CONCAT(kCmdOpt_,a_Stem):   RT_CONCAT(g_f,a_Stem) = true; break; \
            case RT_CONCAT(kCmdOpt_No,a_Stem): RT_CONCAT(g_f,a_Stem) = false; break
            CASE_OPT(ManyFiles);
            CASE_OPT(Open);
            CASE_OPT(FStat);
            CASE_OPT(FChMod);
            CASE_OPT(FUtimes);
            CASE_OPT(Stat);
            CASE_OPT(ChMod);
            CASE_OPT(Utimes);
            CASE_OPT(Rename);
            CASE_OPT(DirEnum);
            CASE_OPT(MkRmDir);
            CASE_OPT(StatVfs);
            CASE_OPT(Rm);
            CASE_OPT(ChSize);
            CASE_OPT(Seek);
            CASE_OPT(Read);
            CASE_OPT(Write);
#undef CASE_OPT

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
                if (g_fRead || g_fWrite || g_fSeek)
                    fsPerfIo();
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
