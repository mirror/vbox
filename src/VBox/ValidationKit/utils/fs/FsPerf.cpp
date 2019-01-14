/* $Id$ */
/** @file
 * NetPerf - File System Performance Benchmark.
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
        uint64_t const nsStartEstimation = RTTimeNanoTS(); \
        for (uint32_t iIteration = 0; iIteration < 64; iIteration++) \
        { \
            RTTESTI_CHECK_RC(a_fnCall, VINF_SUCCESS); \
        } \
        uint64_t cNs = RTTimeNanoTS() - nsStartEstimation; \
        cNs /= 64; \
        uint32_t const cIterations = (((a_cNsTarget) / cNs) + 255) / 256 * 256; \
        \
        /* Do the actual profiling: */ \
        fsPerfYield(); \
        uint64_t const nsStart = RTTimeNanoTS(); \
        for (uint32_t iIteration = 0; iIteration < cIterations; iIteration++) \
        { \
            a_fnCall; \
        } \
        uint64_t const cNsElapsed = RTTimeNanoTS() - nsStart; \
        RTTestIValueF(cNsElapsed / cIterations, RTTESTUNIT_NS_PER_OCCURRENCE, a_szDesc); \
    } while (0)


/**
 * Macro for profiling an operation on each file in the manytree directory tree.
 *
 * Always does an even number of tree iterations.
 */
#define PROFILE_MANYTREE_FN(a_szPath, a_fnCall, a_cEstimationIterations, a_cNsTarget, a_szDesc1, a_szDesc2) \
    do { \
        /* Estimate how many iterations we need to fill up the given timeslot: */ \
        uint64_t const nsStartEstimation = RTTimeNanoTS(); \
        PFSPERFNAMEENTRY pCur; \
        for (uint32_t iIteration = 0; iIteration < (a_cEstimationIterations); iIteration++) \
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
        } \
        uint64_t cNs = RTTimeNanoTS() - nsStartEstimation; \
        uint32_t const cIterations = RT_ALIGN_32(((a_cNsTarget) + cNs - 1) / (cNs / 2), 2); \
        /* Do the actual profiling: */ \
        fsPerfYield(); \
        uint32_t       cCalls = 0; \
        uint64_t const nsStart = RTTimeNanoTS(); \
        for (uint32_t iIteration = 0; iIteration < cIterations; iIteration++) \
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
        uint64_t const cNsElapsed = RTTimeNanoTS() - nsStart; \
        if (a_szDesc1 != NULL) \
            RTTestIValueF(cNsElapsed, RTTESTUNIT_NS, a_szDesc1, cCalls); \
        RTTestIValueF(cNsElapsed / cCalls, RTTESTUNIT_NS_PER_OCCURRENCE, a_szDesc2); \
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


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Command line parameters */
static const RTGETOPTDEF g_aCmdOptions[] =
{
    { "--dir",              'd', RTGETOPT_REQ_STRING  },
    { "--seconds",          's', RTGETOPT_REQ_UINT32  },
    { "--verbose",          'v', RTGETOPT_REQ_NOTHING },
    { "--version",          'V', RTGETOPT_REQ_NOTHING },
    { "--help",             'h', RTGETOPT_REQ_NOTHING } /* for Usage() */
};

/** The test handle. */
static RTTEST       g_hTest;
/** Verbosity level. */
static uint32_t     g_uVerbosity = 0;

/** @name Selected subtest
 * @{ */
static bool         g_fManyFiles = true;
/** @} */

/** The length of each test run. */
static uint64_t     g_nsTestRun  = RT_NS_1SEC_64 * 10;

/** For the 'manyfiles' subdir.  */
static uint32_t     g_cManyFiles = 10000;

/** Number of files in the 'manytree' directory tree.  */
static uint32_t     g_cManyTreeFiles  = 640 + 16*640 /*10880*/;
/** Number of files per directory in the 'manytree' construct. */
static uint32_t     g_cManyTreeFilesPerDir = 640;
/* Number of subdirs per directory in the 'manytree' construct. */
static uint32_t     g_cManyTreeSubdirsPerDir = 16;
/** The depth of the 'manytree' directory tree.  */
static uint32_t     g_cManyTreeDepth  = 1;
/** List of directories in the many tree, creation order. */
static RTLISTANCHOR g_ManyTreeHead;

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
 * Construct a path relative to the base test directory.
 *
 * @returns g_szDir.
 * @param   pszAppend           What to append.
 * @param   cchAppend           How much to append.
 */
DECLINLINE(char *) InDir(const char *pszAppend, size_t cchAppend)
{
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
    memcpy(&g_szDeepDir[g_cchDeepDir], pszAppend, cchAppend);
    g_szDeepDir[g_cchDeepDir + cchAppend] = '\0';
    return &g_szDeepDir[0];
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


void fsPerfOpen(void)
{
    RTTestISub("open");

    /* Opening non-existing files. */
    RTFILE hFile;
    RTTESTI_CHECK_RC(RTFileOpen(&hFile, InEmptyDir(RT_STR_TUPLE("no-such-file")),
                                RTFILE_O_OPEN | RTFILE_O_DENY_NONE | RTFILE_O_READ), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileOpen(&hFile, InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")),
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
    PROFILE_MANYTREE_FN(szPath, fsPerfOpenExistingOnceReadonly(szPath), 1, g_nsTestRun, NULL, "RTFileOpen/Close/manytree/readonly");
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
                        1, g_nsTestRun, NULL, "RTPathQueryInfoEx/manytree/NOTHING");
    PROFILE_MANYTREE_FN(szPath, RTPathQueryInfoEx(szPath, &ObjInfo, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK),
                        1, g_nsTestRun, NULL, "RTPathQueryInfoEx/manytree/UNIX");
}


void fsPerfChmod(void)
{
    RTTestISub("chmod");

    /* Non-existing files. */
    RTTESTI_CHECK_RC(RTPathSetMode(InEmptyDir(RT_STR_TUPLE("no-such-file")), 0665),
                     VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathSetMode(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file")), 0665),
                     VERR_PATH_NOT_FOUND);

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
                        NULL, "RTPathSetMode/manytree");
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
                        1, g_nsTestRun, NULL, "RTPathSetTimesEx/manytree");
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
                     VERR_PATH_NOT_FOUND);

    RTFILE hFile1;
    RTTESTI_CHECK_RC_RETV(RTFileOpen(&hFile1, InDir(RT_STR_TUPLE("file16")),
                                     RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTFileClose(hFile1), VINF_SUCCESS);
    strcat(strcpy(szPath, g_szDir), "-no-such-dir" RTPATH_SLASH_STR "file16");
    RTTESTI_CHECK_RC(RTPathRename(szPath, g_szDir, 0), VERR_PATH_NOT_FOUND);
    RTTESTI_CHECK_RC(RTPathRename(g_szDir, szPath, 0), VERR_PATH_NOT_FOUND);

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
    PROFILE_MANYTREE_FN(szPath, fsPerfRenameMany(szPath, iIteration), 2, g_nsTestRun, NULL, "RTPathRename/manytree");
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

    /*
     * The empty directory.
     */
    RTDIR hDir;
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

    /*
     * Profile.
     */
    PROFILE_FN(fsPerfEnumEmpty(), g_nsTestRun, "RTDirOpen/Read/Close empty");
    PROFILE_FN(fsPerfEnumManyFiles(), g_nsTestRun, "RTDirOpen/Read/Close manyfiles");
}


void fsPerfRm(void)
{
    RTTestISub("rm");

    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("no-such-file"))), VERR_FILE_NOT_FOUND);
    RTTESTI_CHECK_RC(RTFileDelete(InEmptyDir(RT_STR_TUPLE("no-such-dir" RTPATH_SLASH_STR "no-such-file"))), VERR_PATH_NOT_FOUND);

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


static void Usage(PRTSTREAM pStrm)
{
    char szExec[RTPATH_MAX];
    RTStrmPrintf(pStrm, "usage: %s <-d <testdir>> [options]\n",
                 RTPathFilename(RTProcGetExecutablePath(szExec, sizeof(szExec))));
    RTStrmPrintf(pStrm, "\n");
    RTStrmPrintf(pStrm, "options: \n");


    for (unsigned i = 0; i < RT_ELEMENTS(g_aCmdOptions); i++)
    {
        const char *pszHelp;
        switch (g_aCmdOptions[i].iShort)
        {
            case 'd':   pszHelp = "The directory to use for testing.  Default: CWD/fstestdir"; break;
            case 's':   pszHelp = "Benchmark duration.  Default: 10 sec"; break;
            case 'v':   pszHelp = "More verbose execution."; break;
            case 'q':   pszHelp = "Quiet execution."; break;
            case 'h':   pszHelp = "Displays this help and exit"; break;
            case 'V':   pszHelp = "Displays the program revision"; break;
            default:
                pszHelp = "Option undocumented";
                break;
        }
        char szOpt[256];
        RTStrPrintf(szOpt, sizeof(szOpt), "%s, -%c", g_aCmdOptions[i].pszLong, g_aCmdOptions[i].iShort);
        RTStrmPrintf(pStrm, "  %-20s%s\n", szOpt, pszHelp);
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

            /* The empty subdir and associated globals: */
            static char s_szEmpty[] = "empty";
            memcpy(g_szEmptyDir, g_szDir, g_cchDir);
            memcpy(&g_szEmptyDir[g_cchDir], s_szEmpty, sizeof(s_szEmpty));
            g_cchEmptyDir = g_cchDir + sizeof(s_szEmpty) - 1;
            rc = RTDirCreate(g_szEmptyDir, 0755, 0);
            if (RT_SUCCESS(rc))
            {
                RTTestIPrintf(RTTESTLVL_ALWAYS, "Empty dir: %s\n", g_szEmptyDir);

                /* Deep directory: */
                memcpy(g_szDeepDir, g_szDir, g_cchDir);
                g_cchDeepDir = g_cchDir;
                do
                {
                    static char const s_szSub[] = "d" RTPATH_SLASH_STR;
                    memcpy(&g_szDeepDir[g_cchDeepDir], s_szSub, sizeof(s_szSub));
                    g_cchDeepDir += sizeof(s_szSub) - 1;
                    RTTESTI_CHECK_RC(rc = RTDirCreate(g_szDeepDir, 0755, 0), VINF_SUCCESS);
                } while (RT_SUCCESS(rc) && g_cchDeepDir < 176);
                RTTestIPrintf(RTTESTLVL_ALWAYS, "Deep  dir: %s\n", g_szDeepDir);
                if (RT_SUCCESS(rc))
                {
                    /* Do tests: */
                    if (g_fManyFiles)
                        fsPerfManyFiles();
#if 1
                    fsPerfOpen();
                    fsPerfFStat();
                    fsPerfFChMod();
                    fsPerfFUtimes();
                    fsPerfStat();
                    fsPerfChmod();
                    fsPerfUtimes();
                    fsPerfRename();
                    vsPerfDirEnum();
                    /// @todo fsPerfMkDir
                    /// @todo fsPerfRmDir
                    /// @todo fsPerfRead
                    /// @todo fsPerfWrite
                    /// @todo fsPerfSeek
                    /// @todo fsPerfChSize
#endif
                    fsPerfRm(); /* must come last as it deletes manyfiles and manytree */
                }
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
