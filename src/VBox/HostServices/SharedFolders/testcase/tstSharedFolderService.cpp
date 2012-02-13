/* $Id$ */
/** @file
 *
 * Testcase for the shared folder service vbsf API.
 *
 * Note that this is still very threadbare (there is an awful lot which should
 * really be tested, but it already took too long to produce this much).  The
 * idea is that anyone who makes changes to the shared folders service and who
 * cares about unit testing them should add tests to the skeleton framework to
 * exercise the bits they change before and after changing them.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/******************************************************************************
*   Header Files                                                              *
******************************************************************************/

#include "tstSharedFolderService.h"
#include "vbsf.h"

#include <iprt/fs.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/symlink.h>
#include <iprt/stream.h>
#include <iprt/test.h>

#include "teststubs.h"

/******************************************************************************
*   Global Variables                                                          *
******************************************************************************/
static RTTEST g_hTest = NIL_RTTEST;

/******************************************************************************
*   Declarations                                                              *
******************************************************************************/

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *ptable);

/******************************************************************************
*   Helpers                                                                   *
******************************************************************************/

/** Simple call handle structure for the guest call completion callback */
struct VBOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code */
    int32_t rc;
};

/** Call completion callback for guest calls. */
static void callComplete(VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
    callHandle->rc = rc;
}

/**
 * Initialise the HGCM service table as much as we need to start the
 * service
 * @param  pTable the table to initialise
 */
void initTable(VBOXHGCMSVCFNTABLE *pTable, VBOXHGCMSVCHELPERS *pHelpers)
{
    pTable->cbSize              = sizeof (VBOXHGCMSVCFNTABLE);
    pTable->u32Version          = VBOX_HGCM_SVC_VERSION;
    pHelpers->pfnCallComplete   = callComplete;
    pTable->pHelpers            = pHelpers;
}

#define LLUIFY(a) ((unsigned long long)(a))

static void bufferFromString(void *pvDest, size_t cb, const char *pcszSrc)
{
    char *pchDest = (char *)pvDest;

    Assert((cb) > 0);
    strncpy((pchDest), (pcszSrc), (cb) - 1);
    (pchDest)[(cb) - 1] = 0;
}

static void bufferFromPath(void *pvDest, size_t cb, const char *pcszSrc)
{
    char *psz;

    bufferFromString(pvDest, cb, pcszSrc);
    for (psz = (char *)pvDest; psz && psz < (char *)pvDest + cb; ++psz)
        if (*psz == '\\')
            *psz = '/';
}

#define ARRAY_FROM_PATH(a, b) \
do { \
    Assert((a) == (a)); /* Constant parameter */ \
    Assert(sizeof((a)) > 0); \
    bufferFromPath(a, sizeof(a), b); \
} while(0)

/******************************************************************************
*   Stub functions                                                            *
******************************************************************************/

static PRTDIR testRTDirClosepDir;

extern int testRTDirClose(PRTDIR pDir)
{
 /* RTPrintf("%s: pDir=%p\n", __PRETTY_FUNCTION__, pDir); */
    testRTDirClosepDir = pDir;
    return VINF_SUCCESS;
}

static char testRTDirCreatePath[256];
static RTFMODE testRTDirCreateMode;

extern int testRTDirCreate(const char *pszPath, RTFMODE fMode, uint32_t fCreate)
{
 /* RTPrintf("%s: pszPath=%s, fMode=0x%llx\n", __PRETTY_FUNCTION__, pszPath,
             LLUIFY(fMode)); */
    ARRAY_FROM_PATH(testRTDirCreatePath, pszPath);
    return 0;
}

static char testRTDirOpenName[256];
static PRTDIR testRTDirOpenpDir;

extern int testRTDirOpen(PRTDIR *ppDir, const char *pszPath)
{
 /* RTPrintf("%s: pszPath=%s\n", __PRETTY_FUNCTION__, pszPath); */
    ARRAY_FROM_PATH(testRTDirOpenName, pszPath);
    *ppDir = testRTDirOpenpDir;
    testRTDirOpenpDir = 0;
    return VINF_SUCCESS;
}

/** @todo Do something useful with the last two arguments. */
extern int testRTDirOpenFiltered(PRTDIR *ppDir, const char *pszPath, RTDIRFILTER, uint32_t)
{
 /* RTPrintf("%s: pszPath=%s\n", __PRETTY_FUNCTION__, pszPath); */
    ARRAY_FROM_PATH(testRTDirOpenName, pszPath);
    *ppDir = testRTDirOpenpDir;
    testRTDirOpenpDir = 0;
    return VINF_SUCCESS;
}

static PRTDIR testRTDirQueryInfoDir;
static RTTIMESPEC testRTDirQueryInfoATime;

extern int testRTDirQueryInfo(PRTDIR pDir, PRTFSOBJINFO pObjInfo,
                               RTFSOBJATTRADD enmAdditionalAttribs)
{
 /* RTPrintf("%s: pDir=%p, enmAdditionalAttribs=0x%llx\n", __PRETTY_FUNCTION__,
             pDir, LLUIFY(enmAdditionalAttribs)); */
    testRTDirQueryInfoDir = pDir;
    RT_ZERO(*pObjInfo);
    pObjInfo->AccessTime = testRTDirQueryInfoATime;
    RT_ZERO(testRTDirQueryInfoATime);
    return VINF_SUCCESS;
}

extern int testRTDirRemove(const char *pszPath) { RTPrintf("%s\n", __PRETTY_FUNCTION__); return 0; }

static PRTDIR testRTDirReadExDir;

extern int testRTDirReadEx(PRTDIR pDir, PRTDIRENTRYEX pDirEntry,
                            size_t *pcbDirEntry,
                            RTFSOBJATTRADD enmAdditionalAttribs,
                            uint32_t fFlags)
{
 /* RTPrintf("%s: pDir=%p, pcbDirEntry=%d, enmAdditionalAttribs=%llu, fFlags=0x%llx\n",
             __PRETTY_FUNCTION__, pDir, pcbDirEntry ? (int) *pcbDirEntry : -1,
             LLUIFY(enmAdditionalAttribs), LLUIFY(fFlags)); */
    testRTDirReadExDir = pDir;
    return VERR_NO_MORE_FILES;
}

static RTTIMESPEC testRTDirSetTimesATime;

extern int testRTDirSetTimes(PRTDIR pDir, PCRTTIMESPEC pAccessTime,
                                PCRTTIMESPEC pModificationTime,
                                PCRTTIMESPEC pChangeTime,
                                PCRTTIMESPEC pBirthTime)
{
 /* RTPrintf("%s: pDir=%p, *pAccessTime=%lli, *pModificationTime=%lli, *pChangeTime=%lli, *pBirthTime=%lli\n",
             __PRETTY_FUNCTION__, pDir,
             pAccessTime ? (long long)RTTimeSpecGetNano(pAccessTime) : -1,
               pModificationTime
             ? (long long)RTTimeSpecGetNano(pModificationTime) : -1,
             pChangeTime ? (long long)RTTimeSpecGetNano(pChangeTime) : -1,
             pBirthTime ? (long long)RTTimeSpecGetNano(pBirthTime) : -1); */
    if (pAccessTime)
        testRTDirSetTimesATime = *pAccessTime;
    else
        RT_ZERO(testRTDirSetTimesATime);
    return VINF_SUCCESS;
}

static RTFILE testRTFileCloseFile;

extern int  testRTFileClose(RTFILE File)
{
 /* RTPrintf("%s: File=%p\n", __PRETTY_FUNCTION__, File); */
    testRTFileCloseFile = File;
    return 0;
}

extern int  testRTFileDelete(const char *pszFilename) { RTPrintf("%s\n", __PRETTY_FUNCTION__); return 0; }

static RTFILE testRTFileFlushFile;

extern int  testRTFileFlush(RTFILE File)
{
 /* RTPrintf("%s: File=%p\n", __PRETTY_FUNCTION__, File); */
    testRTFileFlushFile = File;
    return VINF_SUCCESS;
}

static RTFILE testRTFileLockFile;
static unsigned testRTFileLockfLock;
static int64_t testRTFileLockOffset;
static uint64_t testRTFileLockSize;

extern int  testRTFileLock(RTFILE hFile, unsigned fLock, int64_t offLock,
                            uint64_t cbLock)
{
 /* RTPrintf("%s: hFile=%p, fLock=%u, offLock=%lli, cbLock=%llu\n",
             __PRETTY_FUNCTION__, hFile, fLock, (long long) offLock,
             LLUIFY(cbLock)); */
    testRTFileLockFile = hFile;
    testRTFileLockfLock = fLock;
    testRTFileLockOffset = offLock;
    testRTFileLockSize = cbLock;
    return VINF_SUCCESS;
}

static char testRTFileOpenName[256];
static uint64_t testRTFileOpenFlags;
static RTFILE testRTFileOpenpFile;

extern int  testRTFileOpen(PRTFILE pFile, const char *pszFilename,
                            uint64_t fOpen)
{
 /* RTPrintf("%s, pszFilename=%s, fOpen=0x%llx\n", __PRETTY_FUNCTION__,
             pszFilename, LLUIFY(fOpen)); */
    ARRAY_FROM_PATH(testRTFileOpenName, pszFilename);
    testRTFileOpenFlags = fOpen;
    *pFile = testRTFileOpenpFile;
    testRTFileOpenpFile = 0;
    return VINF_SUCCESS;
}

static RTFILE testRTFileQueryInfoFile;
static RTTIMESPEC testRTFileQueryInfoATime;
static uint32_t testRTFileQueryInfoFMode;

extern int  testRTFileQueryInfo(RTFILE hFile, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
 /* RTPrintf("%s, hFile=%p, enmAdditionalAttribs=0x%llx\n",
             __PRETTY_FUNCTION__, hFile, LLUIFY(enmAdditionalAttribs)); */
    testRTFileQueryInfoFile = hFile;
    RT_ZERO(*pObjInfo);
    pObjInfo->AccessTime = testRTFileQueryInfoATime;
    RT_ZERO(testRTDirQueryInfoATime);
    pObjInfo->Attr.fMode = testRTFileQueryInfoFMode;
    testRTFileQueryInfoFMode = 0;
    return VINF_SUCCESS;
}

static const char *testRTFileReadData;

extern int  testRTFileRead(RTFILE File, void *pvBuf, size_t cbToRead,
                            size_t *pcbRead)
{
 /* RTPrintf("%s : File=%p, cbToRead=%llu\n", __PRETTY_FUNCTION__, File,
             LLUIFY(cbToRead)); */
    bufferFromPath(pvBuf, cbToRead, testRTFileReadData);
    if (pcbRead)
        *pcbRead = RT_MIN(cbToRead, strlen(testRTFileReadData) + 1);
    testRTFileReadData = 0;
    return VINF_SUCCESS;
}

extern int testRTFileSeek(RTFILE hFile, int64_t offSeek, unsigned uMethod,
                           uint64_t *poffActual)
{
 /* RTPrintf("%s : hFile=%p, offSeek=%llu, uMethod=%u\n", __PRETTY_FUNCTION__,
             hFile, LLUIFY(offSeek), uMethod); */
    if (poffActual)
        *poffActual = 0;
    return VINF_SUCCESS;
}

static uint64_t testRTFileSetFMode;

extern int testRTFileSetMode(RTFILE File, RTFMODE fMode)
{
 /* RTPrintf("%s: fMode=%llu\n", __PRETTY_FUNCTION__, LLUIFY(fMode)); */
    testRTFileSetFMode = fMode; 
    return VINF_SUCCESS;
}

static RTFILE testRTFileSetSizeFile;
static RTFOFF testRTFileSetSizeSize;

extern int  testRTFileSetSize(RTFILE File, uint64_t cbSize)
{
 /* RTPrintf("%s: File=%llu, cbSize=%llu\n", __PRETTY_FUNCTION__, LLUIFY(File),
             LLUIFY(cbSize)); */
    testRTFileSetSizeFile = File;
    testRTFileSetSizeSize = (RTFOFF) cbSize; /* Why was this signed before? */
    return VINF_SUCCESS;
}

static RTTIMESPEC testRTFileSetTimesATime;

extern int testRTFileSetTimes(RTFILE File, PCRTTIMESPEC pAccessTime,
                               PCRTTIMESPEC pModificationTime,
                               PCRTTIMESPEC pChangeTime,
                               PCRTTIMESPEC pBirthTime)
{
 /* RTPrintf("%s: pFile=%p, *pAccessTime=%lli, *pModificationTime=%lli, *pChangeTime=%lli, *pBirthTime=%lli\n",
             __PRETTY_FUNCTION__,
             pAccessTime ? (long long)RTTimeSpecGetNano(pAccessTime) : -1,
               pModificationTime
             ? (long long)RTTimeSpecGetNano(pModificationTime) : -1,
             pChangeTime ? (long long)RTTimeSpecGetNano(pChangeTime) : -1,
             pBirthTime ? (long long)RTTimeSpecGetNano(pBirthTime) : -1); */
    if (pAccessTime)
        testRTFileSetTimesATime = *pAccessTime;
    else
        RT_ZERO(testRTFileSetTimesATime);
    return VINF_SUCCESS;
}

static RTFILE testRTFileUnlockFile;
static int64_t testRTFileUnlockOffset;
static uint64_t testRTFileUnlockSize;

extern int  testRTFileUnlock(RTFILE File, int64_t offLock, uint64_t cbLock)
{
 /* RTPrintf("%s: hFile=%p, ofLock=%lli, cbLock=%llu\n", __PRETTY_FUNCTION__,
             File, (long long) offLock, LLUIFY(cbLock)); */
    testRTFileUnlockFile = File;
    testRTFileUnlockOffset = offLock;
    testRTFileUnlockSize = cbLock;
    return VINF_SUCCESS;
}

static char testRTFileWriteData[256];

extern int  testRTFileWrite(RTFILE File, const void *pvBuf, size_t cbToWrite,
                             size_t *pcbWritten)
{
 /* RTPrintf("%s: File=%p, pvBuf=%.*s, cbToWrite=%llu\n", __PRETTY_FUNCTION__,
             File, cbToWrite, (const char *)pvBuf, LLUIFY(cbToWrite)); */
    ARRAY_FROM_PATH(testRTFileWriteData, (const char *)pvBuf);
    if (pcbWritten)
        *pcbWritten = strlen(testRTFileWriteData) + 1;
    return VINF_SUCCESS;
}
                             
extern int testRTFsQueryProperties(const char *pszFsPath,
                                      PRTFSPROPERTIES pProperties)
{
 /* RTPrintf("%s, pszFsPath=%s\n", __PRETTY_FUNCTION__, pszFsPath);
    RT_ZERO(*pProperties); */
    pProperties->cbMaxComponent = 256;
    pProperties->fCaseSensitive = true;
    return VINF_SUCCESS;
}

extern int testRTFsQuerySerial(const char *pszFsPath, uint32_t *pu32Serial)
{ RTPrintf("%s\n", __PRETTY_FUNCTION__); return 0; }
extern int testRTFsQuerySizes(const char *pszFsPath, PRTFOFF pcbTotal,
                                 RTFOFF *pcbFree, uint32_t *pcbBlock, 
                                 uint32_t *pcbSector) { RTPrintf("%s\n", __PRETTY_FUNCTION__); return 0; }

extern int testRTPathQueryInfoEx(const char *pszPath,
                                    PRTFSOBJINFO pObjInfo,
                                    RTFSOBJATTRADD enmAdditionalAttribs,
                                    uint32_t fFlags)
{
 /* RTPrintf("%s: pszPath=%s, enmAdditionalAttribs=0x%x, fFlags=0x%x\n",
             __PRETTY_FUNCTION__, pszPath, (unsigned) enmAdditionalAttribs,
             (unsigned) fFlags); */
    RT_ZERO(*pObjInfo);
    return VINF_SUCCESS;
}

extern int testRTSymlinkDelete(const char *pszSymlink, uint32_t fDelete) 
{ RTPrintf("%s\n", __PRETTY_FUNCTION__); return 0; }
extern int testRTSymlinkRead(const char *pszSymlink, char *pszTarget,
                              size_t cbTarget, uint32_t fRead)
{ RTPrintf("%s\n", __PRETTY_FUNCTION__); return 0; }

/******************************************************************************
*   Tests                                                                     *
******************************************************************************/

/* Sub-tests for testMappingsQuery(). */
void testMappingsQuerySimple(RTTEST hTest) {}
void testMappingsQueryTooFewBuffers(RTTEST hTest) {}
void testMappingsQueryAutoMount(RTTEST hTest) {}
void testMappingsQueryArrayWrongSize(RTTEST hTest) {}

/* Sub-tests for testMappingsQueryName(). */
void testMappingsQueryNameValid(RTTEST hTest) {}
void testMappingsQueryNameInvalid(RTTEST hTest) {}
void testMappingsQueryNameBadBuffer(RTTEST hTest) {}

/* Sub-tests for testMapFolder(). */
void testMapFolderValid(RTTEST hTest) {}
void testMapFolderInvalid(RTTEST hTest) {}
void testMapFolderTwice(RTTEST hTest) {}
void testMapFolderDelimiter(RTTEST hTest) {}
void testMapFolderCaseSensitive(RTTEST hTest) {}
void testMapFolderCaseInsensitive(RTTEST hTest) {}
void testMapFolderBadParameters(RTTEST hTest) {}

/* Sub-tests for testUnmapFolder(). */
void testUnmapFolderValid(RTTEST hTest) {}
void testUnmapFolderInvalid(RTTEST hTest) {}
void testUnmapFolderBadParameters(RTTEST hTest) {}

/* Sub-tests for testCreate(). */
void testCreateBadParameters(RTTEST hTest) {}

/* Sub-tests for testClose(). */
void testCloseBadParameters(RTTEST hTest) {}

/* Sub-tests for testRead(). */
void testReadBadParameters(RTTEST hTest) {}

/* Sub-tests for testWrite(). */
void testWriteBadParameters(RTTEST hTest) {}

/* Sub-tests for testLock(). */
void testLockBadParameters(RTTEST hTest) {}

/* Sub-tests for testFlush(). */
void testFlushBadParameters(RTTEST hTest) {}

/* Sub-tests for testDirList(). */
void testDirListBadParameters(RTTEST hTest) {}

/* Sub-tests for testReadLink(). */
void testReadLinkBadParameters(RTTEST hTest) {}

/* Sub-tests for testFSInfo(). */
void testFSInfoBadParameters(RTTEST hTest) {}

/* Sub-tests for testRemove(). */
void testRemoveBadParameters(RTTEST hTest) {}

/* Sub-tests for testRename(). */
void testRenameBadParameters(RTTEST hTest) {}

/* Sub-tests for testSymlink(). */
void testSymlinkBadParameters(RTTEST hTest) {}

/* Sub-tests for testMappingsAdd(). */
void testMappingsAddBadParameters(RTTEST hTest) {}

/* Sub-tests for testMappingsRemove(). */
void testMappingsRemoveBadParameters(RTTEST hTest) {}

struct TESTSHFLSTRING
{
    SHFLSTRING string;
    char acData[256];
};

static void fillTestShflString(struct TESTSHFLSTRING *pDest,
                               const char *pcszSource)
{
    unsigned i;
    
    AssertRelease(  strlen(pcszSource) * 2 + 2 < sizeof(*pDest)
                  - RT_UOFFSETOF(SHFLSTRING, String));
    pDest->string.u16Size = (uint16_t)strlen(pcszSource) * 2 + 2;
    pDest->string.u16Length = (uint16_t)strlen(pcszSource);
    for (i = 0; i < strlen(pcszSource) + 1; ++i)
        pDest->string.String.ucs2[i] = (uint16_t)pcszSource[i];
}

static SHFLROOT initWithWritableMapping(RTTEST hTest,
                                        VBOXHGCMSVCFNTABLE *psvcTable,
                                        VBOXHGCMSVCHELPERS *psvcHelpers,
                                        const char *pcszFolderName,
                                        const char *pcszMapping)
{
    VBOXHGCMSVCPARM aParms[RT_MAX(SHFL_CPARMS_ADD_MAPPING,
                                  SHFL_CPARMS_MAP_FOLDER)];
    struct TESTSHFLSTRING FolderName;
    struct TESTSHFLSTRING Mapping;
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    int rc;

    initTable(psvcTable, psvcHelpers);
    AssertReleaseRC(VBoxHGCMSvcLoad(psvcTable));
    AssertRelease(  psvcTable->pvService
                  = RTTestGuardedAllocTail(hTest, psvcTable->cbClient));
    fillTestShflString(&FolderName, pcszFolderName);
    fillTestShflString(&Mapping, pcszMapping);
    aParms[0].setPointer(&FolderName,   RT_UOFFSETOF(SHFLSTRING, String)
                                      + FolderName.string.u16Size);
    aParms[1].setPointer(&Mapping,   RT_UOFFSETOF(SHFLSTRING, String)
                                   + Mapping.string.u16Size);
    aParms[2].setUInt32(1);
    rc = psvcTable->pfnHostCall(psvcTable->pvService, SHFL_FN_ADD_MAPPING,
                                SHFL_CPARMS_ADD_MAPPING, aParms);
    AssertReleaseRC(rc);
    aParms[0].setPointer(&Mapping,   RT_UOFFSETOF(SHFLSTRING, String)
                                   + Mapping.string.u16Size);
    aParms[1].setUInt32(0);  /* root */
    aParms[2].setUInt32('/');  /* delimiter */
    aParms[3].setUInt32(1);  /* case sensitive */
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_MAP_FOLDER,
                       SHFL_CPARMS_MAP_FOLDER, aParms);
    AssertReleaseRC(callHandle.rc);
    return aParms[1].u.uint32;
}

/** @todo Mappings should be automatically removed by unloading the service,
 *        but unloading is currently a no-op! */
static void unmapAndRemoveMapping(RTTEST hTest, VBOXHGCMSVCFNTABLE *psvcTable,
                                  SHFLROOT root, const char *pcszFolderName)
{
    VBOXHGCMSVCPARM aParms[RT_MAX(SHFL_CPARMS_UNMAP_FOLDER,
                                  SHFL_CPARMS_REMOVE_MAPPING)];
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    struct TESTSHFLSTRING FolderName;
    int rc;

    aParms[0].setUInt32(root);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_UNMAP_FOLDER,
                       SHFL_CPARMS_UNMAP_FOLDER, aParms);
    AssertReleaseRC(callHandle.rc);
    fillTestShflString(&FolderName, pcszFolderName);
    aParms[0].setPointer(&FolderName,   RT_UOFFSETOF(SHFLSTRING, String)
                                      + FolderName.string.u16Size);
    rc = psvcTable->pfnHostCall(psvcTable->pvService, SHFL_FN_REMOVE_MAPPING,
                                SHFL_CPARMS_REMOVE_MAPPING, aParms);
    AssertReleaseRC(rc);
}

static int createFile(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT Root,
                      const char *pcszFilename, uint32_t fCreateFlags,
                      SHFLHANDLE *pHandle, SHFLCREATERESULT *pResult)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_CREATE];
    struct TESTSHFLSTRING Path;
    SHFLCREATEPARMS CreateParms;
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    fillTestShflString(&Path, pcszFilename);
    RT_ZERO(CreateParms);
    CreateParms.CreateFlags = fCreateFlags;
    aParms[0].setUInt32(Root);
    aParms[1].setPointer(&Path,   RT_UOFFSETOF(SHFLSTRING, String)
                                + Path.string.u16Size);
    aParms[2].setPointer(&CreateParms, sizeof(CreateParms));
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_CREATE,
                       RT_ELEMENTS(aParms), aParms);
    if (RT_FAILURE(callHandle.rc))
        return callHandle.rc;
    if (pHandle)
        *pHandle = CreateParms.Handle;
    if (pResult)
        *pResult = CreateParms.Result;
    return VINF_SUCCESS;
}

static int readFile(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT Root,
                    SHFLHANDLE hFile, uint64_t offSeek, uint32_t cbRead,
                    uint32_t *pcbRead, void *pvBuf, uint32_t cbBuf)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_READ];
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    aParms[0].setUInt32(Root);
    aParms[1].setUInt64((uint64_t) hFile);
    aParms[2].setUInt64(offSeek);
    aParms[3].setUInt32(cbRead);
    aParms[4].setPointer(pvBuf, cbBuf);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_READ,
                       RT_ELEMENTS(aParms), aParms);
    if (pcbRead)
        *pcbRead = aParms[3].u.uint32;
    return callHandle.rc;
}

static int writeFile(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT Root,
                     SHFLHANDLE hFile, uint64_t offSeek, uint32_t cbWrite,
                     uint32_t *pcbWritten, const void *pvBuf, uint32_t cbBuf)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_WRITE];
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    aParms[0].setUInt32(Root);
    aParms[1].setUInt64((uint64_t) hFile);
    aParms[2].setUInt64(offSeek);
    aParms[3].setUInt32(cbWrite);
    aParms[4].setPointer((void *)pvBuf, cbBuf);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_WRITE,
                       RT_ELEMENTS(aParms), aParms);
    if (pcbWritten)
        *pcbWritten = aParms[3].u.uint32;
    return callHandle.rc;
}

static int flushFile(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT root,
                     SHFLHANDLE handle)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_FLUSH];
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    aParms[0].setUInt32(root);
    aParms[1].setUInt64(handle);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_FLUSH,
                       SHFL_CPARMS_FLUSH, aParms);
    return callHandle.rc;
}

static int listDir(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT root,
                   SHFLHANDLE handle, uint32_t fFlags, uint32_t cb,
                   const char *pcszPath, void *pvBuf, uint32_t cbBuf,
                   uint32_t resumePoint, uint32_t *pcFiles)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_LIST];
    struct TESTSHFLSTRING Path;
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    aParms[0].setUInt32(root);
    aParms[1].setUInt64(handle);
    aParms[2].setUInt32(fFlags);
    aParms[3].setUInt32(cb);
    if (pcszPath)
    {
        fillTestShflString(&Path, pcszPath);
        aParms[4].setPointer(&Path,   RT_UOFFSETOF(SHFLSTRING, String)
                                    + Path.string.u16Size);
    }
    else
        aParms[4].setPointer(NULL, 0);
    aParms[5].setPointer(pvBuf, cbBuf);
    aParms[6].setUInt32(resumePoint);
    aParms[7].setUInt32(0);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_LIST,
                       RT_ELEMENTS(aParms), aParms);
    if (pcFiles)
        *pcFiles = aParms[7].u.uint32;
    return callHandle.rc;
}

static int sfInformation(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT root,
                         SHFLHANDLE handle, uint32_t fFlags, uint32_t cb,
                         SHFLFSOBJINFO *pInfo)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_INFORMATION];
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    aParms[0].setUInt32(root);
    aParms[1].setUInt64(handle);
    aParms[2].setUInt32(fFlags);
    aParms[3].setUInt32(cb);
    aParms[4].setPointer(pInfo, cb);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_INFORMATION,
                       RT_ELEMENTS(aParms), aParms);
    return callHandle.rc;
}

static int lockFile(VBOXHGCMSVCFNTABLE *psvcTable, SHFLROOT root,
                    SHFLHANDLE handle, int64_t offLock, uint64_t cbLock,
                    uint32_t fFlags)
{
    VBOXHGCMSVCPARM aParms[SHFL_CPARMS_LOCK];
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };

    aParms[0].setUInt32(root);
    aParms[1].setUInt64(handle);
    aParms[2].setUInt64(offLock);
    aParms[3].setUInt64(cbLock);
    aParms[4].setUInt32(fFlags);
    psvcTable->pfnCall(psvcTable->pvService, &callHandle, 0,
                       psvcTable->pvService, SHFL_FN_LOCK,
                       RT_ELEMENTS(aParms), aParms);
    return callHandle.rc;
}

void testCreateFileSimple(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    SHFLCREATERESULT Result;
    int rc;

    RTTestSub(hTest, "Create file simple");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ, NULL,
                    &Result);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest,
                     !strcmp(testRTFileOpenName, "/test/mapping/test/file"),
                     (hTest, "pszFilename=%s\n", testRTFileOpenName));
    RTTEST_CHECK_MSG(hTest, testRTFileOpenFlags == 0x181,
                     (hTest, "fOpen=%llu\n", LLUIFY(testRTFileOpenFlags)));
    RTTEST_CHECK_MSG(hTest, Result == SHFL_FILE_CREATED,
                     (hTest, "Result=%d\n", (int) Result));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
}

void testCreateDirSimple(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    PRTDIR pcDir = (PRTDIR)0x10000;
    SHFLCREATERESULT Result;
    int rc;

    RTTestSub(hTest, "Create directory simple");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTDirOpenpDir = pcDir;
    rc = createFile(&svcTable, Root, "test/dir",
                    SHFL_CF_DIRECTORY | SHFL_CF_ACCESS_READ, NULL, &Result);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest,
                     !strcmp(testRTDirCreatePath, "/test/mapping/test/dir"),
                     (hTest, "pszPath=%s\n", testRTDirCreatePath));
    RTTEST_CHECK_MSG(hTest,
                     !strcmp(testRTDirOpenName, "/test/mapping/test/dir"),
                     (hTest, "pszFilename=%s\n", testRTDirOpenName));
    RTTEST_CHECK_MSG(hTest, Result == SHFL_FILE_CREATED,
                     (hTest, "Result=%d\n", (int) Result));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest,
                     testRTDirClosepDir == pcDir,
                     (hTest, "pDir=%llu\n", LLUIFY(testRTDirClosepDir)));
}

void testReadFileSimple(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    SHFLHANDLE Handle;
    const char *pcszReadData = "Data to read";
    char acBuf[sizeof(pcszReadData) + 10];
    uint32_t cbRead;
    int rc;

    RTTestSub(hTest, "Read file simple");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    testRTFileReadData = pcszReadData;
    rc = readFile(&svcTable, Root, Handle, 0, strlen(pcszReadData) + 1,
                  &cbRead, acBuf, (uint32_t)sizeof(acBuf));
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest,
                     !strncmp(acBuf, pcszReadData, sizeof(acBuf)),
                     (hTest, "pvBuf=%.*s\n", sizeof(acBuf), acBuf));
    RTTEST_CHECK_MSG(hTest, cbRead == strlen(pcszReadData) + 1,
                     (hTest, "cbRead=%llu\n", LLUIFY(cbRead)));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
}

void testWriteFileSimple(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    SHFLHANDLE Handle;
    const char *pcszWrittenData = "Data to write";
    uint32_t cbToWrite = (uint32_t)strlen(pcszWrittenData) + 1;
    uint32_t cbWritten;
    int rc;

    RTTestSub(hTest, "Write file simple");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    rc = writeFile(&svcTable, Root, Handle, 0, cbToWrite, &cbWritten,
                   pcszWrittenData, cbToWrite);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest,
                     !strcmp(testRTFileWriteData, pcszWrittenData),
                     (hTest, "pvBuf=%s\n", testRTFileWriteData));
    RTTEST_CHECK_MSG(hTest, cbWritten == cbToWrite,
                     (hTest, "cbWritten=%llu\n", LLUIFY(cbWritten)));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
}

void testFlushFileSimple(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    SHFLHANDLE Handle;
    int rc;

    RTTestSub(hTest, "Flush file simple");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    rc = flushFile(&svcTable, Root, Handle);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest, testRTFileFlushFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileFlushFile)));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
}

void testDirListEmpty(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    PRTDIR pcDir = (PRTDIR)0x10000;
    SHFLHANDLE Handle;
    SHFLDIRINFO DirInfo;
    uint32_t cFiles;
    int rc;

    RTTestSub(hTest, "List empty directory");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTDirOpenpDir = pcDir;
    rc = createFile(&svcTable, Root, "test/dir",
                    SHFL_CF_DIRECTORY | SHFL_CF_ACCESS_READ, &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    rc = listDir(&svcTable, Root, Handle, 0, sizeof (SHFLDIRINFO), NULL,
                 &DirInfo, sizeof(DirInfo), 0, &cFiles);
    RTTEST_CHECK_RC(hTest, rc, VERR_NO_MORE_FILES);
    RTTEST_CHECK_MSG(hTest, testRTDirReadExDir == pcDir,
                     (hTest, "Dir=%llu\n", LLUIFY(testRTDirReadExDir)));
    RTTEST_CHECK_MSG(hTest, cFiles == 0,
                     (hTest, "cFiles=%llu\n", LLUIFY(cFiles)));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest,
                     testRTDirClosepDir == pcDir,
                     (hTest, "pDir=%llu\n", LLUIFY(testRTDirClosepDir)));
}

void testFSInfoQuerySetFMode(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    const uint32_t fMode = 0660;
    SHFLFSOBJINFO Info;
    SHFLHANDLE Handle;
    int rc;

    RTTestSub(hTest, "Query and set file size");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RT_ZERO(Info);
    testRTFileQueryInfoFMode = fMode;
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_FILE, sizeof(Info),
                       &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest, testRTFileQueryInfoFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileQueryInfoFile)));
    RTTEST_CHECK_MSG(hTest, Info.Attr.fMode == fMode,
                     (hTest, "cbObject=%llu\n", LLUIFY(Info.cbObject)));
    RT_ZERO(Info);
    Info.Attr.fMode = fMode;
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_SET | SHFL_INFO_FILE,
                       sizeof(Info), &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest, testRTFileSetFMode == fMode,
                     (hTest, "Size=%llu\n", LLUIFY(testRTFileSetFMode)));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
}

void testFSInfoQuerySetDirATime(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const PRTDIR pcDir = (PRTDIR) 0x10000;
    const int64_t ccAtimeNano = 100000;
    SHFLFSOBJINFO Info;
    SHFLHANDLE Handle;
    int rc;

    RTTestSub(hTest, "Query and set directory atime");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTDirOpenpDir = pcDir;
    rc = createFile(&svcTable, Root, "test/dir",
                    SHFL_CF_DIRECTORY | SHFL_CF_ACCESS_READ, &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RT_ZERO(Info);
    RTTimeSpecSetNano(&testRTDirQueryInfoATime, ccAtimeNano);
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_FILE, sizeof(Info),
                       &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest, testRTDirQueryInfoDir == pcDir,
                     (hTest, "Dir=%llu\n", LLUIFY(testRTDirQueryInfoDir)));
    RTTEST_CHECK_MSG(hTest, RTTimeSpecGetNano(&Info.AccessTime) == ccAtimeNano,
                     (hTest, "ATime=%llu\n",
                      LLUIFY(RTTimeSpecGetNano(&Info.AccessTime))));
    RT_ZERO(Info);
    RTTimeSpecSetNano(&Info.AccessTime, ccAtimeNano);
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_SET | SHFL_INFO_FILE,
                       sizeof(Info), &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest,    RTTimeSpecGetNano(&testRTDirSetTimesATime)
                            == ccAtimeNano,
                     (hTest, "ATime=%llu\n",
                      LLUIFY(RTTimeSpecGetNano(&testRTDirSetTimesATime))));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTDirClosepDir == pcDir,
                     (hTest, "File=%llu\n", LLUIFY(testRTDirClosepDir)));
}

void testFSInfoQuerySetFileATime(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    const int64_t ccAtimeNano = 100000;
    SHFLFSOBJINFO Info;
    SHFLHANDLE Handle;
    int rc;

    RTTestSub(hTest, "Query and set file atime");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RT_ZERO(Info);
    RTTimeSpecSetNano(&testRTFileQueryInfoATime, ccAtimeNano);
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_FILE, sizeof(Info),
                       &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest, testRTFileQueryInfoFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileQueryInfoFile)));
    RTTEST_CHECK_MSG(hTest, RTTimeSpecGetNano(&Info.AccessTime) == ccAtimeNano,
                     (hTest, "ATime=%llu\n",
                      LLUIFY(RTTimeSpecGetNano(&Info.AccessTime))));
    RT_ZERO(Info);
    RTTimeSpecSetNano(&Info.AccessTime, ccAtimeNano);
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_SET | SHFL_INFO_FILE,
                       sizeof(Info), &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest,    RTTimeSpecGetNano(&testRTFileSetTimesATime)
                            == ccAtimeNano,
                     (hTest, "ATime=%llu\n",
                      LLUIFY(RTTimeSpecGetNano(&testRTFileSetTimesATime))));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
}

void testFSInfoQuerySetEndOfFile(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    const RTFOFF cbNew = 50000;
    SHFLFSOBJINFO Info;
    SHFLHANDLE Handle;
    int rc;

    RTTestSub(hTest, "Set end of file position");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RT_ZERO(Info);
    Info.cbObject = cbNew;
    rc = sfInformation(&svcTable, Root, Handle, SHFL_INFO_SET | SHFL_INFO_SIZE,
                       sizeof(Info), &Info);
    RTTEST_CHECK_RC_OK(hTest, rc);
    RTTEST_CHECK_MSG(hTest, testRTFileSetSizeFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileSetSizeFile)));
    RTTEST_CHECK_MSG(hTest, testRTFileSetSizeSize == cbNew,
                     (hTest, "Size=%llu\n", LLUIFY(testRTFileSetSizeSize)));
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
}

void testLockFileSimple(RTTEST hTest)
{
    VBOXHGCMSVCFNTABLE  svcTable;
    VBOXHGCMSVCHELPERS  svcHelpers;
    SHFLROOT Root;
    const RTFILE hcFile = (RTFILE) 0x10000;
    const int64_t offLock = 50000;
    const uint64_t cbLock = 4000;
    SHFLHANDLE Handle;
    int rc;

    RTTestSub(hTest, "Simple file lock and unlock");
    Root = initWithWritableMapping(hTest, &svcTable, &svcHelpers,
                                   "/test/mapping", "testname");
    testRTFileOpenpFile = hcFile;
    rc = createFile(&svcTable, Root, "/test/file", SHFL_CF_ACCESS_READ,
                    &Handle, NULL);
    RTTEST_CHECK_RC_OK(hTest, rc);
    rc = lockFile(&svcTable, Root, Handle, offLock, cbLock, SHFL_LOCK_SHARED);
    RTTEST_CHECK_RC_OK(hTest, rc);
#ifdef RT_OS_WINDOWS  /* Locking is a no-op elsewhere. */
    RTTEST_CHECK_MSG(hTest, testRTFileLockFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileLockFile)));
    RTTEST_CHECK_MSG(hTest, testRTFileLockfLock == 0,
                     (hTest, "fLock=%u\n", testRTFileLockfLock));
    RTTEST_CHECK_MSG(hTest, testRTFileLockOffset == offLock,
                     (hTest, "Offs=%llu\n", (long long) testRTFileLockOffset));
    RTTEST_CHECK_MSG(hTest, testRTFileLockSize == cbLock,
                     (hTest, "Size=%llu\n", LLUIFY(testRTFileLockSize)));
#endif
    rc = lockFile(&svcTable, Root, Handle, offLock, cbLock, SHFL_LOCK_CANCEL);
    RTTEST_CHECK_RC_OK(hTest, rc);
#ifdef RT_OS_WINDOWS
    RTTEST_CHECK_MSG(hTest, testRTFileUnlockFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileUnlockFile)));
    RTTEST_CHECK_MSG(hTest, testRTFileUnlockOffset == offLock,
                     (hTest, "Offs=%llu\n",
                      (long long) testRTFileUnlockOffset));
    RTTEST_CHECK_MSG(hTest, testRTFileUnlockSize == cbLock,
                     (hTest, "Size=%llu\n", LLUIFY(testRTFileUnlockSize)));
#endif
    unmapAndRemoveMapping(hTest, &svcTable, Root, "testname");
    AssertReleaseRC(svcTable.pfnDisconnect(NULL, 0, svcTable.pvService));
    RTTestGuardedFree(hTest, svcTable.pvService);
    RTTEST_CHECK_MSG(hTest, testRTFileCloseFile == hcFile,
                     (hTest, "File=%llu\n", LLUIFY(testRTFileCloseFile)));
}

/******************************************************************************
*   Main code                                                                 *
******************************************************************************/

static void testAPI(RTTEST hTest)
{
    testMappingsQuery(hTest);
    testMappingsQueryName(hTest);
    testMapFolder(hTest);
    testUnmapFolder(hTest);
    testCreate(hTest);
    testClose(hTest);
    testRead(hTest);
    testWrite(hTest);
    testLock(hTest);
    testFlush(hTest);
    testDirList(hTest);
    testReadLink(hTest);
    testFSInfo(hTest);
    testRemove(hTest);
    testRename(hTest);
    testSymlink(hTest);
    testMappingsAdd(hTest);
    testMappingsRemove(hTest);
    /* testSetStatusLed(hTest); */
}

int main(int argc, char **argv)
{
    RTEXITCODE rcExit = RTTestInitAndCreate(RTPathFilename(argv[0]),
                                            &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);
    testAPI(g_hTest);
    return RTTestSummaryAndDestroy(g_hTest);
}
