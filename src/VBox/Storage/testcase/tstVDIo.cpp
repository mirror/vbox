/* $Id$ */
/** @file
 *
 * VBox HDD container test utility - I/O replay.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOGGROUP LOGGROUP_DEFAULT
#include <VBox/vd.h>
#include <VBox/vddbg.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/ctype.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/rand.h>
#include <iprt/critsect.h>

#include "VDMemDisk.h"
#include "VDIoBackendMem.h"
#include "VDIoRnd.h"

#include "VDScript.h"

/**
 * A virtual file backed by memory.
 */
typedef struct VDFILE
{
    /** Pointer to the next file. */
    RTLISTNODE     Node;
    /** Name of the file. */
    char          *pszName;
    /** Memory file baking the file. */
    PVDMEMDISK     pMemDisk;
    /** Flag whether the file is read locked. */
    bool           fReadLock;
    /** Flag whether the file is write locked. */
    bool           fWriteLock;
    /** Statistics: Number of reads. */
    unsigned       cReads;
    /** Statistics: Number of writes. */
    unsigned       cWrites;
    /** Statistics: Number of flushes. */
    unsigned       cFlushes;
    /** Statistics: Number of async reads. */
    unsigned       cAsyncReads;
    /** Statistics: Number of async writes. */
    unsigned       cAsyncWrites;
    /** Statistics: Number of async flushes. */
    unsigned       cAsyncFlushes;
} VDFILE, *PVDFILE;

/**
 * VD storage object.
 */
typedef struct VDSTORAGE
{
    /** Pointer to the file. */
    PVDFILE        pFile;
    /** Completion callback of the VD layer. */
    PFNVDCOMPLETED pfnComplete;
} VDSTORAGE, *PVDSTORAGE;

/**
 * A virtual disk.
 */
typedef struct VDDISK
{
    /** List node. */
    RTLISTNODE     ListNode;
    /** Name of the disk handle for identification. */
    char          *pszName;
    /** HDD handle to operate on. */
    PVBOXHDD       pVD;
    /** Memory disk used for data verification. */
    PVDMEMDISK     pMemDiskVerify;
    /** Critical section to serialize access to the memory disk. */
    RTCRITSECT     CritSectVerify;
    /** Physical CHS Geometry. */
    VDGEOMETRY     PhysGeom;
    /** Logical CHS geometry. */
    VDGEOMETRY     LogicalGeom;
} VDDISK, *PVDDISK;

/**
 * A data buffer with a pattern.
 */
typedef struct VDPATTERN
{
    /** List node. */
    RTLISTNODE     ListNode;
    /** Name of the pattern. */
    char          *pszName;
    /** Size of the pattern. */
    size_t         cbPattern;
    /** Pointer to the buffer containing the pattern. */
    void          *pvPattern;
} VDPATTERN, *PVDPATTERN;

/**
 * Global VD test state.
 */
typedef struct VDTESTGLOB
{
    /** List of active virtual disks. */
    RTLISTNODE       ListDisks;
    /** Head of the active file list. */
    RTLISTNODE       ListFiles;
    /** Head of the pattern list. */
    RTLISTNODE       ListPatterns;
    /** Memory I/O backend. */
    PVDIOBACKENDMEM  pIoBackend;
    /** Error interface. */
    VDINTERFACEERROR VDIfError;
    /** Pointer to the per disk interface list. */
    PVDINTERFACE     pInterfacesDisk;
    /** I/O interface. */
    VDINTERFACEIO    VDIfIo;
    /** Pointer to the per image interface list. */
    PVDINTERFACE     pInterfacesImages;
    /** I/O RNG handle. */
    PVDIORND         pIoRnd;
} VDTESTGLOB, *PVDTESTGLOB;

/**
 * Transfer direction.
 */
typedef enum VDIOREQTXDIR
{
    VDIOREQTXDIR_READ = 0,
    VDIOREQTXDIR_WRITE,
    VDIOREQTXDIR_FLUSH,
    VDIOREQTXDIR_DISCARD
} VDIOREQTXDIR;

/**
 * I/O request.
 */
typedef struct VDIOREQ
{
    /** Transfer type. */
    VDIOREQTXDIR  enmTxDir;
    /** slot index. */
    unsigned      idx;
    /** Start offset. */
    uint64_t      off;
    /** Size to transfer. */
    size_t        cbReq;
    /** S/G Buffer */
    RTSGBUF       SgBuf;
    /** Data segment */
    RTSGSEG       DataSeg;
    /** Flag whether the request is outstanding or not. */
    volatile bool fOutstanding;
    /** Buffer to use for reads. */
    void          *pvBufRead;
    /** Opaque user data. */
    void          *pvUser;
} VDIOREQ, *PVDIOREQ;

/**
 * I/O test data.
 */
typedef struct VDIOTEST
{
    /** Start offset. */
    uint64_t    offStart;
    /** End offset. */
    uint64_t    offEnd;
    /** Flag whether random or sequential access is wanted */
    bool        fRandomAccess;
    /** Block size. */
    size_t      cbBlkIo;
    /** Number of bytes to transfer. */
    uint64_t    cbIo;
    /** Chance in percent to get a write. */
    unsigned    uWriteChance;
    /** Pointer to the I/O data generator. */
    PVDIORND    pIoRnd;
    /** Pointer to the data pattern to use. */
    PVDPATTERN  pPattern;
    /** Data dependent on the I/O mode (sequential or random). */
    union
    {
        /** Next offset for sequential access. */
        uint64_t    offNext;
        /** Data for random acess. */
        struct
        {
            /** Number of valid entries in the bitmap. */
            uint32_t cBlocks;
            /** Pointer to the bitmap marking accessed blocks. */
            uint8_t *pbMapAccessed;
            /** Number of unaccessed blocks. */
            uint32_t cBlocksLeft;
        } Rnd;
    } u;
} VDIOTEST, *PVDIOTEST;

static DECLCALLBACK(int) vdScriptHandlerCreate(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerOpen(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIo(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerFlush(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerMerge(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerCompact(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerDiscard(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerCopy(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerClose(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerPrintFileSize(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIoLogReplay(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIoRngCreate(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIoRngDestroy(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIoPatternCreateFromNumber(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIoPatternCreateFromFile(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerIoPatternDestroy(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerSleep(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerDumpFile(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerCreateDisk(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerDestroyDisk(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerCompareDisks(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerDumpDiskInfo(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerPrintMsg(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerShowStatistics(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerResetStatistics(PVDSCRIPTARG paScriptArgs, void *pvUser);
static DECLCALLBACK(int) vdScriptHandlerResize(PVDSCRIPTARG paScriptArgs, void *pvUser);

/* create action */
const VDSCRIPTTYPE g_aArgCreate[] =
{
    VDSCRIPTTYPE_STRING,
    VDSCRIPTTYPE_STRING,
    VDSCRIPTTYPE_STRING,
    VDSCRIPTTYPE_STRING,
    VDSCRIPTTYPE_STRING,
    VDSCRIPTTYPE_UINT64,
    VDSCRIPTTYPE_BOOL
};

/* open action */
const VDSCRIPTTYPE g_aArgOpen[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_STRING, /* name */
    VDSCRIPTTYPE_STRING, /* backend */
    VDSCRIPTTYPE_BOOL,   /* async */
    VDSCRIPTTYPE_BOOL,   /* shareable */
    VDSCRIPTTYPE_BOOL,   /* readonly */
    VDSCRIPTTYPE_BOOL,   /* discard */
    VDSCRIPTTYPE_BOOL    /* ignoreflush */
};

/* I/O action */
const VDSCRIPTTYPE g_aArgIo[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_BOOL,   /* async */
    VDSCRIPTTYPE_UINT32, /* max-reqs */
    VDSCRIPTTYPE_STRING, /* mode */
    VDSCRIPTTYPE_UINT64, /* size */
    VDSCRIPTTYPE_UINT64, /* blocksize */
    VDSCRIPTTYPE_UINT64, /* offStart */
    VDSCRIPTTYPE_UINT64, /* offEnd */
    VDSCRIPTTYPE_UINT32, /* writes */
    VDSCRIPTTYPE_STRING  /* pattern */
};

/* flush action */
const VDSCRIPTTYPE g_aArgFlush[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_BOOL    /* async */
};

/* merge action */
const VDSCRIPTTYPE g_aArgMerge[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_UINT32, /* from */
    VDSCRIPTTYPE_UINT32  /* to */
};

/* Compact a disk */
const VDSCRIPTTYPE g_aArgCompact[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_UINT32  /* image */
};

/* Discard a part of a disk */
const VDSCRIPTTYPE g_aArgDiscard[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_BOOL,   /* async */
    VDSCRIPTTYPE_STRING  /* ranges */
};

/* Compact a disk */
const VDSCRIPTTYPE g_aArgCopy[] =
{
    VDSCRIPTTYPE_STRING, /* diskfrom */
    VDSCRIPTTYPE_STRING, /* diskto */
    VDSCRIPTTYPE_UINT32, /* imagefrom */
    VDSCRIPTTYPE_STRING, /* backend */
    VDSCRIPTTYPE_STRING, /* filename */
    VDSCRIPTTYPE_BOOL,   /* movebyrename */
    VDSCRIPTTYPE_UINT64, /* size */
    VDSCRIPTTYPE_UINT32, /* fromsame */
    VDSCRIPTTYPE_UINT32  /* tosame */
};

/* close action */
const VDSCRIPTTYPE g_aArgClose[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_STRING, /* mode */
    VDSCRIPTTYPE_BOOL    /* delete */
};

/* print file size action */
const VDSCRIPTTYPE g_aArgPrintFileSize[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_UINT32 /* image */
};

/* print file size action */
const VDSCRIPTTYPE g_aArgIoLogReplay[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_STRING  /* iolog */
};

/* I/O RNG create action */
const VDSCRIPTTYPE g_aArgIoRngCreate[] =
{
    VDSCRIPTTYPE_UINT32, /* size */
    VDSCRIPTTYPE_STRING, /* mode */
    VDSCRIPTTYPE_UINT32, /* seed */
};

/* I/O pattern create action */
const VDSCRIPTTYPE g_aArgIoPatternCreateFromNumber[] =
{
    VDSCRIPTTYPE_STRING, /* name */
    VDSCRIPTTYPE_UINT32, /* size */
    VDSCRIPTTYPE_UINT32  /* pattern */
};

/* I/O pattern create action */
const VDSCRIPTTYPE g_aArgIoPatternCreateFromFile[] =
{
    VDSCRIPTTYPE_STRING, /* name */
    VDSCRIPTTYPE_STRING  /* file */
};

/* I/O pattern destroy action */
const VDSCRIPTTYPE g_aArgIoPatternDestroy[] =
{
    VDSCRIPTTYPE_STRING  /* name */
};

/* Sleep */
const VDSCRIPTTYPE g_aArgSleep[] =
{
    VDSCRIPTTYPE_UINT32  /* time */
};

/* Dump memory file */
const VDSCRIPTTYPE g_aArgDumpFile[] =
{
    VDSCRIPTTYPE_STRING, /* file */
    VDSCRIPTTYPE_STRING  /* path */
};

/* Create virtual disk handle */
const VDSCRIPTTYPE g_aArgCreateDisk[] =
{
    VDSCRIPTTYPE_STRING, /* name */
    VDSCRIPTTYPE_BOOL    /* verify */
};

/* Create virtual disk handle */
const VDSCRIPTTYPE g_aArgDestroyDisk[] =
{
    VDSCRIPTTYPE_STRING  /* name */
};

/* Compare virtual disks */
const VDSCRIPTTYPE g_aArgCompareDisks[] =
{
    VDSCRIPTTYPE_STRING, /* disk1 */
    VDSCRIPTTYPE_STRING  /* disk2 */
};

/* Dump disk info */
const VDSCRIPTTYPE g_aArgDumpDiskInfo[] =
{
    VDSCRIPTTYPE_STRING  /* disk */
};

/* Print message */
const VDSCRIPTTYPE g_aArgPrintMsg[] =
{
    VDSCRIPTTYPE_STRING  /* msg */
};

/* Show statistics */
const VDSCRIPTTYPE g_aArgShowStatistics[] =
{
    VDSCRIPTTYPE_STRING  /* file */
};

/* Reset statistics */
const VDSCRIPTTYPE g_aArgResetStatistics[] =
{
    VDSCRIPTTYPE_STRING  /* file */
};

/* Resize disk. */
const VDSCRIPTTYPE g_aArgResize[] =
{
    VDSCRIPTTYPE_STRING, /* disk */
    VDSCRIPTTYPE_UINT64  /* size */
};


const VDSCRIPTCALLBACK g_aScriptActions[] =
{
    /* pcszFnName                  enmTypeReturn      paArgDesc                          cArgDescs                                      pfnHandler */
    {"create",                     VDSCRIPTTYPE_VOID, g_aArgCreate,                      RT_ELEMENTS(g_aArgCreate),                     vdScriptHandlerCreate},
    {"open",                       VDSCRIPTTYPE_VOID, g_aArgOpen,                        RT_ELEMENTS(g_aArgOpen),                       vdScriptHandlerOpen},
    {"io",                         VDSCRIPTTYPE_VOID, g_aArgIo,                          RT_ELEMENTS(g_aArgIo),                         vdScriptHandlerIo},
    {"flush",                      VDSCRIPTTYPE_VOID, g_aArgFlush,                       RT_ELEMENTS(g_aArgFlush),                      vdScriptHandlerFlush},
    {"close",                      VDSCRIPTTYPE_VOID, g_aArgClose,                       RT_ELEMENTS(g_aArgClose),                      vdScriptHandlerClose},
    {"printfilesize",              VDSCRIPTTYPE_VOID, g_aArgPrintFileSize,               RT_ELEMENTS(g_aArgPrintFileSize),              vdScriptHandlerPrintFileSize},
    {"ioreplay",                   VDSCRIPTTYPE_VOID, g_aArgIoLogReplay,                 RT_ELEMENTS(g_aArgIoLogReplay),                vdScriptHandlerIoLogReplay},
    {"merge",                      VDSCRIPTTYPE_VOID, g_aArgMerge,                       RT_ELEMENTS(g_aArgMerge),                      vdScriptHandlerMerge},
    {"compact",                    VDSCRIPTTYPE_VOID, g_aArgCompact,                     RT_ELEMENTS(g_aArgCompact),                    vdScriptHandlerCompact},
    {"discard",                    VDSCRIPTTYPE_VOID, g_aArgDiscard,                     RT_ELEMENTS(g_aArgDiscard),                    vdScriptHandlerDiscard},
    {"copy",                       VDSCRIPTTYPE_VOID, g_aArgCopy,                        RT_ELEMENTS(g_aArgCopy),                       vdScriptHandlerCopy},
    {"iorngcreate",                VDSCRIPTTYPE_VOID, g_aArgIoRngCreate,                 RT_ELEMENTS(g_aArgIoRngCreate),                vdScriptHandlerIoRngCreate},
    {"iorngdestroy",               VDSCRIPTTYPE_VOID, NULL,                              0,                                             vdScriptHandlerIoRngDestroy},
    {"iopatterncreatefromnumber",  VDSCRIPTTYPE_VOID, g_aArgIoPatternCreateFromNumber,   RT_ELEMENTS(g_aArgIoPatternCreateFromNumber),  vdScriptHandlerIoPatternCreateFromNumber},
    {"iopatterncreatefromfile",    VDSCRIPTTYPE_VOID, g_aArgIoPatternCreateFromFile,     RT_ELEMENTS(g_aArgIoPatternCreateFromFile),    vdScriptHandlerIoPatternCreateFromFile},
    {"iopatterndestroy",           VDSCRIPTTYPE_VOID, g_aArgIoPatternDestroy,            RT_ELEMENTS(g_aArgIoPatternDestroy),           vdScriptHandlerIoPatternDestroy},
    {"sleep",                      VDSCRIPTTYPE_VOID, g_aArgSleep,                       RT_ELEMENTS(g_aArgSleep),                      vdScriptHandlerSleep},
    {"dumpfile",                   VDSCRIPTTYPE_VOID, g_aArgDumpFile,                    RT_ELEMENTS(g_aArgDumpFile),                   vdScriptHandlerDumpFile},
    {"createdisk",                 VDSCRIPTTYPE_VOID, g_aArgCreateDisk,                  RT_ELEMENTS(g_aArgCreateDisk),                 vdScriptHandlerCreateDisk},
    {"destroydisk",                VDSCRIPTTYPE_VOID, g_aArgDestroyDisk,                 RT_ELEMENTS(g_aArgDestroyDisk),                vdScriptHandlerDestroyDisk},
    {"comparedisks",               VDSCRIPTTYPE_VOID, g_aArgCompareDisks,                RT_ELEMENTS(g_aArgCompareDisks),               vdScriptHandlerCompareDisks},
    {"dumpdiskinfo",               VDSCRIPTTYPE_VOID, g_aArgDumpDiskInfo,                RT_ELEMENTS(g_aArgDumpDiskInfo),               vdScriptHandlerDumpDiskInfo},
    {"print",                      VDSCRIPTTYPE_VOID, g_aArgPrintMsg,                    RT_ELEMENTS(g_aArgPrintMsg),                   vdScriptHandlerPrintMsg},
    {"showstatistics",             VDSCRIPTTYPE_VOID, g_aArgShowStatistics,              RT_ELEMENTS(g_aArgShowStatistics),             vdScriptHandlerShowStatistics},
    {"resetstatistics",            VDSCRIPTTYPE_VOID, g_aArgResetStatistics,             RT_ELEMENTS(g_aArgResetStatistics),            vdScriptHandlerResetStatistics},
    {"resize",                     VDSCRIPTTYPE_VOID, g_aArgResize,                      RT_ELEMENTS(g_aArgResize),                     vdScriptHandlerResize},
};

const unsigned g_cScriptActions = RT_ELEMENTS(g_aScriptActions);

static DECLCALLBACK(int) vdScriptCallbackPrint(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    RTPrintf(paScriptArgs[0].psz);
    return VINF_SUCCESS;
}

static void tstVDError(void *pvUser, int rc, RT_SRC_POS_DECL,
                       const char *pszFormat, va_list va)
{
    RTPrintf("tstVD: Error %Rrc at %s:%u (%s): ", rc, RT_SRC_POS_ARGS);
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
}

static int tstVDMessage(void *pvUser, const char *pszFormat, va_list va)
{
    RTPrintf("tstVD: ");
    RTPrintfV(pszFormat, va);
    return VINF_SUCCESS;
}

static int tstVDIoTestInit(PVDIOTEST pIoTest, PVDTESTGLOB pGlob, bool fRandomAcc, uint64_t cbIo,
                           size_t cbBlkSize, uint64_t offStart, uint64_t offEnd,
                           unsigned uWriteChance, PVDPATTERN pPattern);
static bool tstVDIoTestRunning(PVDIOTEST pIoTest);
static void tstVDIoTestDestroy(PVDIOTEST pIoTest);
static bool tstVDIoTestReqOutstanding(PVDIOREQ pIoReq);
static int  tstVDIoTestReqInit(PVDIOTEST pIoTest, PVDIOREQ pIoReq, void *pvUser);
static void tstVDIoTestReqComplete(void *pvUser1, void *pvUser2, int rcReq);

static PVDDISK tstVDIoGetDiskByName(PVDTESTGLOB pGlob, const char *pcszDisk);
static PVDPATTERN tstVDIoGetPatternByName(PVDTESTGLOB pGlob, const char *pcszName);
static PVDPATTERN tstVDIoPatternCreate(const char *pcszName, size_t cbPattern);
static int tstVDIoPatternGetBuffer(PVDPATTERN pPattern, void **ppv, size_t cb);

static DECLCALLBACK(int) vdScriptHandlerCreate(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    uint64_t cbSize = 0;
    const char *pcszBackend = NULL;
    const char *pcszImage = NULL;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    bool fBase = false;
    bool fDynamic = true;
    bool fIgnoreFlush = false;

    pcszDisk = paScriptArgs[0].psz;
    if (!RTStrICmp(paScriptArgs[1].psz, "base"))
        fBase = true;
    else if (!RTStrICmp(paScriptArgs[1].psz, "diff"))
        fBase = false;
    else
    {
        RTPrintf("Invalid image mode '%s' given\n", paScriptArgs[1].psz);
        rc = VERR_INVALID_PARAMETER;
    }
    pcszImage = paScriptArgs[2].psz;
    if (!RTStrICmp(paScriptArgs[3].psz, "fixed"))
        fDynamic = false;
    else if (!RTStrICmp(paScriptArgs[3].psz, "dynamic"))
        fDynamic = true;
    else
    {
        RTPrintf("Invalid image type '%s' given\n", paScriptArgs[3].psz);
        rc = VERR_INVALID_PARAMETER;
    }
    pcszBackend = paScriptArgs[4].psz;
    cbSize = paScriptArgs[5].u64;
    fIgnoreFlush = paScriptArgs[6].f;

    if (RT_SUCCESS(rc))
    {
        pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
        if (pDisk)
        {
            unsigned fOpenFlags = VD_OPEN_FLAGS_ASYNC_IO;
            unsigned fImageFlags = VD_IMAGE_FLAGS_NONE;

            if (!fDynamic)
                fImageFlags |= VD_IMAGE_FLAGS_FIXED;

            if (fIgnoreFlush)
                fOpenFlags |= VD_OPEN_FLAGS_IGNORE_FLUSH;

            if (fBase)
                rc = VDCreateBase(pDisk->pVD, pcszBackend, pcszImage, cbSize, fImageFlags, NULL,
                                  &pDisk->PhysGeom, &pDisk->LogicalGeom,
                                  NULL, fOpenFlags, pGlob->pInterfacesImages, NULL);
            else
                rc = VDCreateDiff(pDisk->pVD, pcszBackend, pcszImage, fImageFlags, NULL, NULL, NULL,
                                  fOpenFlags, pGlob->pInterfacesImages, NULL);
        }
        else
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerOpen(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszBackend = NULL;
    const char *pcszImage = NULL;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    bool fShareable = false;
    bool fReadonly = false;
    bool fAsyncIo  = true;
    bool fDiscard  = false;
    bool fIgnoreFlush = false;

    pcszDisk = paScriptArgs[0].psz;
    pcszImage = paScriptArgs[1].psz;
    pcszBackend = paScriptArgs[2].psz;
    fShareable = paScriptArgs[3].f;
    fReadonly = paScriptArgs[4].f;
    fAsyncIo = paScriptArgs[5].f;
    fDiscard = paScriptArgs[6].f;

    if (RT_SUCCESS(rc))
    {
        pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
        if (pDisk)
        {
            unsigned fOpenFlags = 0;

            if (fAsyncIo)
                fOpenFlags |= VD_OPEN_FLAGS_ASYNC_IO;
            if (fShareable)
                fOpenFlags |= VD_OPEN_FLAGS_SHAREABLE;
            if (fReadonly)
                fOpenFlags |= VD_OPEN_FLAGS_READONLY;
            if (fDiscard)
                fOpenFlags |= VD_OPEN_FLAGS_DISCARD;
            if (fIgnoreFlush)
                fOpenFlags |= VD_OPEN_FLAGS_IGNORE_FLUSH;

            rc = VDOpen(pDisk->pVD, pcszBackend, pcszImage, fOpenFlags, pGlob->pInterfacesImages);
        }
        else
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerIo(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fAsync = false;
    bool fRandomAcc = false;
    uint64_t cbIo = 0;
    uint64_t cbBlkSize = 0;
    bool fDataProviderRnd = false;
    bool fPrintStats = false;
    uint64_t offStart = 0;
    uint64_t offEnd = 0;
    unsigned cMaxReqs = 0;
    uint8_t uWriteChance = 0;
    const char *pcszDisk = NULL;
    const char *pcszPattern = NULL;
    PVDDISK pDisk = NULL;
    PVDPATTERN pPattern = NULL;

    pcszDisk = paScriptArgs[0].psz;
    fAsync   = paScriptArgs[1].f;
    cMaxReqs = paScriptArgs[2].u64;
    if (!RTStrICmp(paScriptArgs[3].psz, "seq"))
        fRandomAcc = false;
    else if (!RTStrICmp(paScriptArgs[3].psz, "rnd"))
        fRandomAcc = true;
    else
    {
        RTPrintf("Invalid access mode '%s'\n", paScriptArgs[3].psz);
        rc = VERR_INVALID_PARAMETER;
    }
    cbBlkSize = paScriptArgs[4].u64;
    offStart = paScriptArgs[5].u64;
    offEnd = paScriptArgs[6].u64;
    cbIo = paScriptArgs[7].u64;
    uWriteChance = (uint8_t)paScriptArgs[8].u64;
    pcszPattern = paScriptArgs[9].psz;

    if (   RT_SUCCESS(rc)
        && fAsync
        && !cMaxReqs)
        rc = VERR_INVALID_PARAMETER;

    if (RT_SUCCESS(rc))
    {
        pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
        if (!pDisk)
            rc = VERR_NOT_FOUND;
    }

    if (RT_SUCCESS(rc))
    {
        /* Set defaults if not set by the user. */
        if (offStart == 0 && offEnd == 0)
        {
            offEnd = VDGetSize(pDisk->pVD, VD_LAST_IMAGE);
            if (offEnd == 0)
                return VERR_INVALID_STATE;
        }

        if (!cbIo)
            cbIo = offEnd;
    }

    if (   RT_SUCCESS(rc)
        && RTStrCmp(pcszPattern, "none"))
    {
        pPattern = tstVDIoGetPatternByName(pGlob, pcszPattern);
        if (!pPattern)
            rc = VERR_NOT_FOUND;
    }

    if (RT_SUCCESS(rc))
    {
        VDIOTEST IoTest;

        rc = tstVDIoTestInit(&IoTest, pGlob, fRandomAcc, cbIo, cbBlkSize, offStart, offEnd, uWriteChance, pPattern);
        if (RT_SUCCESS(rc))
        {
            PVDIOREQ paIoReq = NULL;
            unsigned cMaxTasksOutstanding = fAsync ? cMaxReqs : 1;
            RTSEMEVENT EventSem;

            rc = RTSemEventCreate(&EventSem);
            paIoReq = (PVDIOREQ)RTMemAllocZ(cMaxTasksOutstanding * sizeof(VDIOREQ));
            if (paIoReq && RT_SUCCESS(rc))
            {
                uint64_t NanoTS = RTTimeNanoTS();

                /* Init requests. */
                for (unsigned i = 0; i < cMaxTasksOutstanding; i++)
                {
                    paIoReq[i].idx = i;
                    paIoReq[i].pvBufRead = RTMemAlloc(cbBlkSize);
                    if (!paIoReq[i].pvBufRead)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                }

                while (   tstVDIoTestRunning(&IoTest)
                       && RT_SUCCESS(rc))
                {
                    bool fTasksOutstanding = false;
                    unsigned idx = 0;

                    /* Submit all idling requests. */
                    while (   idx < cMaxTasksOutstanding
                           && tstVDIoTestRunning(&IoTest))
                    {
                        if (!tstVDIoTestReqOutstanding(&paIoReq[idx]))
                        {
                            rc = tstVDIoTestReqInit(&IoTest, &paIoReq[idx], pDisk);
                            AssertRC(rc);

                            if (RT_SUCCESS(rc))
                            {
                                if (!fAsync)
                                {
                                    switch (paIoReq[idx].enmTxDir)
                                    {
                                        case VDIOREQTXDIR_READ:
                                        {
                                            rc = VDRead(pDisk->pVD, paIoReq[idx].off, paIoReq[idx].DataSeg.pvSeg, paIoReq[idx].cbReq);

                                            if (RT_SUCCESS(rc)
                                                && pDisk->pMemDiskVerify)
                                            {
                                                RTSGBUF SgBuf;
                                                RTSgBufInit(&SgBuf, &paIoReq[idx].DataSeg, 1);

                                                if (VDMemDiskCmp(pDisk->pMemDiskVerify, paIoReq[idx].off, paIoReq[idx].cbReq, &SgBuf))
                                                {
                                                    RTPrintf("Corrupted disk at offset %llu!\n", paIoReq[idx].off);
                                                    rc = VERR_INVALID_STATE;
                                                }
                                            }
                                            break;
                                        }
                                        case VDIOREQTXDIR_WRITE:
                                        {
                                            rc = VDWrite(pDisk->pVD, paIoReq[idx].off, paIoReq[idx].DataSeg.pvSeg, paIoReq[idx].cbReq);

                                            if (RT_SUCCESS(rc)
                                                && pDisk->pMemDiskVerify)
                                            {
                                                RTSGBUF SgBuf;
                                                RTSgBufInit(&SgBuf, &paIoReq[idx].DataSeg, 1);
                                                rc = VDMemDiskWrite(pDisk->pMemDiskVerify, paIoReq[idx].off, paIoReq[idx].cbReq, &SgBuf);
                                            }
                                            break;
                                        }
                                        case VDIOREQTXDIR_FLUSH:
                                        {
                                            rc = VDFlush(pDisk->pVD);
                                            break;
                                        }
                                        case VDIOREQTXDIR_DISCARD:
                                            AssertMsgFailed(("Invalid\n"));
                                    }

                                    ASMAtomicXchgBool(&paIoReq[idx].fOutstanding, false);
                                    if (RT_SUCCESS(rc))
                                        idx++;
                                }
                                else
                                {
                                    LogFlow(("Queuing request %d\n", idx));
                                    switch (paIoReq[idx].enmTxDir)
                                    {
                                        case VDIOREQTXDIR_READ:
                                        {
                                            rc = VDAsyncRead(pDisk->pVD, paIoReq[idx].off, paIoReq[idx].cbReq, &paIoReq[idx].SgBuf,
                                                             tstVDIoTestReqComplete, &paIoReq[idx], EventSem);
                                            break;
                                        }
                                        case VDIOREQTXDIR_WRITE:
                                        {
                                            rc = VDAsyncWrite(pDisk->pVD, paIoReq[idx].off, paIoReq[idx].cbReq, &paIoReq[idx].SgBuf,
                                                              tstVDIoTestReqComplete, &paIoReq[idx], EventSem);
                                            break;
                                        }
                                        case VDIOREQTXDIR_FLUSH:
                                        {
                                            rc = VDAsyncFlush(pDisk->pVD, tstVDIoTestReqComplete, &paIoReq[idx], EventSem);
                                            break;
                                        }
                                        case VDIOREQTXDIR_DISCARD:
                                            AssertMsgFailed(("Invalid\n"));
                                    }

                                    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                                    {
                                        idx++;
                                        fTasksOutstanding = true;
                                        rc = VINF_SUCCESS;
                                    }
                                    else if (rc == VINF_VD_ASYNC_IO_FINISHED)
                                    {
                                        LogFlow(("Request %d completed\n", idx));
                                        switch (paIoReq[idx].enmTxDir)
                                        {
                                            case VDIOREQTXDIR_READ:
                                            {
                                                if (pDisk->pMemDiskVerify)
                                                {
                                                    RTCritSectEnter(&pDisk->CritSectVerify);
                                                    RTSgBufReset(&paIoReq[idx].SgBuf);

                                                    if (VDMemDiskCmp(pDisk->pMemDiskVerify, paIoReq[idx].off, paIoReq[idx].cbReq,
                                                                     &paIoReq[idx].SgBuf))
                                                    {
                                                        RTPrintf("Corrupted disk at offset %llu!\n", paIoReq[idx].off);
                                                        rc = VERR_INVALID_STATE;
                                                    }
                                                    RTCritSectLeave(&pDisk->CritSectVerify);
                                                }
                                                break;
                                            }
                                            case VDIOREQTXDIR_WRITE:
                                            {
                                                if (pDisk->pMemDiskVerify)
                                                {
                                                    RTCritSectEnter(&pDisk->CritSectVerify);
                                                    RTSgBufReset(&paIoReq[idx].SgBuf);

                                                    rc = VDMemDiskWrite(pDisk->pMemDiskVerify, paIoReq[idx].off, paIoReq[idx].cbReq,
                                                                        &paIoReq[idx].SgBuf);
                                                    RTCritSectLeave(&pDisk->CritSectVerify);
                                                }
                                                break;
                                            }
                                            case VDIOREQTXDIR_FLUSH:
                                                break;
                                            case VDIOREQTXDIR_DISCARD:
                                                AssertMsgFailed(("Invalid\n"));
                                        }

                                        ASMAtomicXchgBool(&paIoReq[idx].fOutstanding, false);
                                        if (rc != VERR_INVALID_STATE)
                                            rc = VINF_SUCCESS;
                                    }
                                }

                                if (RT_FAILURE(rc))
                                    RTPrintf("Error submitting task %u rc=%Rrc\n", paIoReq[idx].idx, rc);
                            }
                        }
                    }

                    /* Wait for a request to complete. */
                    if (   fAsync
                        && fTasksOutstanding)
                    {
                        rc = RTSemEventWait(EventSem, RT_INDEFINITE_WAIT);
                        AssertRC(rc);
                    }
                }

                /* Cleanup, wait for all tasks to complete. */
                while (fAsync)
                {
                    unsigned idx = 0;
                    bool fAllIdle = true;

                    while (idx < cMaxTasksOutstanding)
                    {
                        if (tstVDIoTestReqOutstanding(&paIoReq[idx]))
                        {
                            fAllIdle = false;
                            break;
                        }
                        idx++;
                    }

                    if (!fAllIdle)
                    {
                        rc = RTSemEventWait(EventSem, 100);
                        Assert(RT_SUCCESS(rc) || rc == VERR_TIMEOUT);
                    }
                    else
                        break;
                }

                NanoTS = RTTimeNanoTS() - NanoTS;
                uint64_t SpeedKBs = (uint64_t)(cbIo / (NanoTS / 1000000000.0) / 1024);
                RTPrintf("I/O Test: Throughput %lld kb/s\n", SpeedKBs);

                RTSemEventDestroy(EventSem);
                RTMemFree(paIoReq);
            }
            else
                rc = VERR_NO_MEMORY;

            tstVDIoTestDestroy(&IoTest);
        }
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerFlush(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fAsync = false;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;

    pcszDisk = paScriptArgs[0].psz;
    fAsync   = paScriptArgs[1].f;

    if (RT_SUCCESS(rc))
    {
        pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
        if (!pDisk)
            rc = VERR_NOT_FOUND;
        else if (fAsync)
        {
            VDIOREQ IoReq;
            RTSEMEVENT EventSem;

            rc = RTSemEventCreate(&EventSem);
            if (RT_SUCCESS(rc))
            {
                memset(&IoReq, 0, sizeof(VDIOREQ));
                IoReq.enmTxDir = VDIOREQTXDIR_FLUSH;
                IoReq.pvUser   = pDisk;
                IoReq.idx      = 0;
                rc = VDAsyncFlush(pDisk->pVD, tstVDIoTestReqComplete, &IoReq, EventSem);
                if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                {
                    rc = RTSemEventWait(EventSem, RT_INDEFINITE_WAIT);
                    AssertRC(rc);
                }
                else if (rc == VINF_VD_ASYNC_IO_FINISHED)
                    rc = VINF_SUCCESS;

                RTSemEventDestroy(EventSem);
            }
        }
        else
            rc = VDFlush(pDisk->pVD);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerMerge(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    unsigned nImageFrom = 0;
    unsigned nImageTo = 0;

    pcszDisk   = paScriptArgs[0].psz;
    nImageFrom = paScriptArgs[1].u32;
    nImageTo   = paScriptArgs[2].u32;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (!pDisk)
        rc = VERR_NOT_FOUND;
    else
    {
        /** @todo: Provide progress interface to test that cancelation
         * doesn't corrupt the data.
         */
        rc = VDMerge(pDisk->pVD, nImageFrom, nImageTo, NULL);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerCompact(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    unsigned nImage = 0;

    pcszDisk = paScriptArgs[0].psz;
    nImage   = paScriptArgs[1].u32;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (!pDisk)
        rc = VERR_NOT_FOUND;
    else
    {
        /** @todo: Provide progress interface to test that cancelation
         * doesn't corrupt the data.
         */
        rc = VDCompact(pDisk->pVD, nImage, NULL);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerDiscard(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    bool fAsync = false;
    const char *pcszRanges = NULL;

    pcszDisk   = paScriptArgs[0].psz;
    fAsync     = paScriptArgs[1].f;
    pcszRanges = paScriptArgs[2].psz;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (!pDisk)
        rc = VERR_NOT_FOUND;
    else
    {
        unsigned cRanges = 0;
        PRTRANGE paRanges = NULL;

        /*
         * Parse the range string which should look like this:
         * n,off1,cb1,off2,cb2,...
         *
         * <n> gives the number of ranges in the string and every off<i>,cb<i>
         * pair afterwards is a start offset + number of bytes to discard entry.
         */
        do
        {
            rc = RTStrToUInt32Ex(pcszRanges, (char **)&pcszRanges, 10, &cRanges);
            if (RT_FAILURE(rc) && (rc != VWRN_TRAILING_CHARS))
                break;

            if (!cRanges)
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            paRanges = (PRTRANGE)RTMemAllocZ(cRanges * sizeof(RTRANGE));
            if (!paRanges)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            if (*pcszRanges != ',')
            {
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            pcszRanges++;

            /* Retrieve each pair from the string. */
            for (unsigned i = 0; i < cRanges; i++)
            {
                uint64_t off;
                uint32_t cb;

                rc = RTStrToUInt64Ex(pcszRanges, (char **)&pcszRanges, 10, &off);
                if (RT_FAILURE(rc) && (rc != VWRN_TRAILING_CHARS))
                    break;

                if (*pcszRanges != ',')
                {
                    switch (*pcszRanges)
                    {
                        case 'k':
                        case 'K':
                        {
                            off *= _1K;
                            break;
                        }
                        case 'm':
                        case 'M':
                        {
                            off *= _1M;
                            break;
                        }
                        case 'g':
                        case 'G':
                        {
                            off *= _1G;
                            break;
                        }
                        default:
                        {
                            RTPrintf("Invalid size suffix '%s'\n", pcszRanges);
                            rc = VERR_INVALID_PARAMETER;
                        }
                    }
                    if (RT_SUCCESS(rc))
                        pcszRanges++;
                }

                if (*pcszRanges != ',')
                {
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                pcszRanges++;

                rc = RTStrToUInt32Ex(pcszRanges, (char **)&pcszRanges, 10, &cb);
                if (RT_FAILURE(rc) && (rc != VWRN_TRAILING_CHARS))
                    break;

                if (*pcszRanges != ',')
                {
                    switch (*pcszRanges)
                    {
                        case 'k':
                        case 'K':
                        {
                            cb *= _1K;
                            break;
                        }
                        case 'm':
                        case 'M':
                        {
                            cb *= _1M;
                            break;
                        }
                        case 'g':
                        case 'G':
                        {
                            cb *= _1G;
                            break;
                        }
                        default:
                        {
                            RTPrintf("Invalid size suffix '%s'\n", pcszRanges);
                            rc = VERR_INVALID_PARAMETER;
                        }
                    }
                    if (RT_SUCCESS(rc))
                        pcszRanges++;
                }

                if (   *pcszRanges != ','
                    && !(i == cRanges - 1 && *pcszRanges == '\0'))
                {
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                pcszRanges++;

                paRanges[i].offStart = off;
                paRanges[i].cbRange  = cb;
            }
        } while (0);

        if (RT_SUCCESS(rc))
        {
            if (!fAsync)
                rc = VDDiscardRanges(pDisk->pVD, paRanges, cRanges);
            else
            {
                VDIOREQ IoReq;
                RTSEMEVENT EventSem;

                rc = RTSemEventCreate(&EventSem);
                if (RT_SUCCESS(rc))
                {
                    memset(&IoReq, 0, sizeof(VDIOREQ));
                    IoReq.enmTxDir = VDIOREQTXDIR_FLUSH;
                    IoReq.pvUser   = pDisk;
                    IoReq.idx      = 0;
                    rc = VDAsyncDiscardRanges(pDisk->pVD, paRanges, cRanges, tstVDIoTestReqComplete, &IoReq, EventSem);
                    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                    {
                        rc = RTSemEventWait(EventSem, RT_INDEFINITE_WAIT);
                        AssertRC(rc);
                    }
                    else if (rc == VINF_VD_ASYNC_IO_FINISHED)
                        rc = VINF_SUCCESS;

                    RTSemEventDestroy(EventSem);
                }
            }

            if (   RT_SUCCESS(rc)
                && pDisk->pMemDiskVerify)
            {
                for (unsigned i = 0; i < cRanges; i++)
                {
                    void *pv = RTMemAllocZ(paRanges[i].cbRange);
                    if (pv)
                    {
                        RTSGSEG SgSeg;
                        RTSGBUF SgBuf;

                        SgSeg.pvSeg = pv;
                        SgSeg.cbSeg = paRanges[i].cbRange;
                        RTSgBufInit(&SgBuf, &SgSeg, 1);
                        rc = VDMemDiskWrite(pDisk->pMemDiskVerify, paRanges[i].offStart, paRanges[i].cbRange, &SgBuf);
                        RTMemFree(pv);
                    }
                    else
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                }
            }
        }

        if (paRanges)
            RTMemFree(paRanges);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerCopy(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDiskFrom = NULL;
    const char *pcszDiskTo = NULL;
    PVDDISK pDiskFrom = NULL;
    PVDDISK pDiskTo = NULL;
    unsigned nImageFrom = 0;
    const char *pcszBackend = NULL;
    const char *pcszFilename = NULL;
    bool fMoveByRename = false;
    uint64_t cbSize = 0;
    unsigned nImageFromSame = VD_IMAGE_CONTENT_UNKNOWN;
    unsigned nImageToSame = VD_IMAGE_CONTENT_UNKNOWN;

    pcszDiskFrom   = paScriptArgs[0].psz;
    pcszDiskTo     = paScriptArgs[1].psz;
    nImageFrom     = paScriptArgs[2].u32;
    pcszBackend    = paScriptArgs[3].psz;
    pcszFilename   = paScriptArgs[4].psz;
    fMoveByRename  = paScriptArgs[5].f;
    cbSize         = paScriptArgs[6].u64;
    nImageFromSame = paScriptArgs[7].u32;
    nImageToSame   = paScriptArgs[8].u32;

    pDiskFrom = tstVDIoGetDiskByName(pGlob, pcszDiskFrom);
    pDiskTo = tstVDIoGetDiskByName(pGlob, pcszDiskTo);
    if (!pDiskFrom || !pDiskTo)
        rc = VERR_NOT_FOUND;
    else
    {
        /** @todo: Provide progress interface to test that cancelation
         * works as intended.
         */
        rc = VDCopyEx(pDiskFrom->pVD, nImageFrom, pDiskTo->pVD, pcszBackend, pcszFilename,
                      fMoveByRename, cbSize, nImageFromSame, nImageToSame,
                      VD_IMAGE_FLAGS_NONE, NULL, VD_OPEN_FLAGS_ASYNC_IO,
                      NULL, pGlob->pInterfacesImages, NULL);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerClose(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fAll = false;
    bool fDelete = false;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;

    pcszDisk = paScriptArgs[0].psz;
    if (!RTStrICmp(paScriptArgs[1].psz, "all"))
        fAll = true;
    else if (!RTStrICmp(paScriptArgs[1].psz, "single"))
        fAll = false;
    else
    {
        RTPrintf("Invalid mode '%s' given\n", paScriptArgs[1].psz);
        rc = VERR_INVALID_PARAMETER;
    }
    fDelete = paScriptArgs[2].f;

    if (   fAll
        && fDelete)
    {
        RTPrintf("mode=all doesn't work with delete=yes\n");
        rc = VERR_INVALID_PARAMETER;
    }

    if (RT_SUCCESS(rc))
    {
        pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
        if (pDisk)
        {
            if (fAll)
                rc = VDCloseAll(pDisk->pVD);
            else
                rc = VDClose(pDisk->pVD, fDelete);
        }
        else
            rc = VERR_NOT_FOUND;
    }
    return rc;
}


static DECLCALLBACK(int) vdScriptHandlerPrintFileSize(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    unsigned nImage = 0;

    pcszDisk = paScriptArgs[0].psz;
    nImage   = paScriptArgs[1].u32;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (pDisk)
        RTPrintf("%s: size of image %u is %llu\n", pcszDisk, nImage, VDGetFileSize(pDisk->pVD, nImage));
    else
        rc = VERR_NOT_FOUND;

    return rc;
}


static DECLCALLBACK(int) vdScriptHandlerIoLogReplay(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    const char *pcszIoLog = NULL;

    pcszDisk  = paScriptArgs[0].psz;
    pcszIoLog = paScriptArgs[1].psz;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (pDisk)
    {
        VDIOLOGGER hIoLogger;

        rc = VDDbgIoLogOpen(&hIoLogger, pcszIoLog);
        if (RT_SUCCESS(rc))
        {
            uint32_t fIoLogFlags;
            VDIOLOGEVENT enmEvent;
            void *pvBuf = NULL;
            size_t cbBuf = 0;

            fIoLogFlags = VDDbgIoLogGetFlags(hIoLogger);

            /* Loop through events. */
            rc = VDDbgIoLogEventTypeGetNext(hIoLogger, &enmEvent);
            while (   RT_SUCCESS(rc)
                   && enmEvent != VDIOLOGEVENT_END)
            {
                VDDBGIOLOGREQ enmReq = VDDBGIOLOGREQ_INVALID;
                uint64_t idEvent = 0;
                bool fAsync = false;
                uint64_t off = 0;
                size_t cbIo = 0;
                Assert(enmEvent == VDIOLOGEVENT_START);

                rc = VDDbgIoLogReqTypeGetNext(hIoLogger, &enmReq);
                if (RT_FAILURE(rc))
                    break;

                switch (enmReq)
                {
                    case VDDBGIOLOGREQ_READ:
                    {
                        rc = VDDbgIoLogEventGetStart(hIoLogger, &idEvent, &fAsync,
                                                     &off, &cbIo, 0, NULL);
                        if (   RT_SUCCESS(rc)
                            && cbIo > cbBuf)
                        {
                            pvBuf = RTMemRealloc(pvBuf, cbIo);
                            if (pvBuf)
                                cbBuf = cbIo;
                            else
                                rc = VERR_NO_MEMORY;
                        }

                        if (   RT_SUCCESS(rc)
                            && !fAsync)
                            rc = VDRead(pDisk->pVD, off, pvBuf, cbIo);
                        else if (RT_SUCCESS(rc))
                            rc = VERR_NOT_SUPPORTED;
                        break;
                    }
                    case VDDBGIOLOGREQ_WRITE:
                    {
                        rc = VDDbgIoLogEventGetStart(hIoLogger, &idEvent, &fAsync,
                                                     &off, &cbIo, cbBuf, pvBuf);
                        if (rc == VERR_BUFFER_OVERFLOW)
                        {
                            pvBuf = RTMemRealloc(pvBuf, cbIo);
                            if (pvBuf)
                            {
                                cbBuf = cbIo;
                                rc = VDDbgIoLogEventGetStart(hIoLogger, &idEvent, &fAsync,
                                                             &off, &cbIo, cbBuf, pvBuf);
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }

                        if (   RT_SUCCESS(rc)
                            && !fAsync)
                            rc = VDWrite(pDisk->pVD, off, pvBuf, cbIo);
                        else if (RT_SUCCESS(rc))
                            rc = VERR_NOT_SUPPORTED;
                        break;
                    }
                    case VDDBGIOLOGREQ_FLUSH:
                    {
                        rc = VDDbgIoLogEventGetStart(hIoLogger, &idEvent, &fAsync,
                                                     &off, &cbIo, 0, NULL);
                        if (   RT_SUCCESS(rc)
                            && !fAsync)
                            rc = VDFlush(pDisk->pVD);
                        else if (RT_SUCCESS(rc))
                            rc = VERR_NOT_SUPPORTED;
                        break;
                    }
                    case VDDBGIOLOGREQ_DISCARD:
                    {
                        PRTRANGE paRanges = NULL;
                        unsigned cRanges = 0;

                        rc = VDDbgIoLogEventGetStartDiscard(hIoLogger, &idEvent, &fAsync,
                                                            &paRanges, &cRanges);
                        if (   RT_SUCCESS(rc)
                            && !fAsync)
                        {
                            rc = VDDiscardRanges(pDisk->pVD, paRanges, cRanges);
                            RTMemFree(paRanges);
                        }
                        else if (RT_SUCCESS(rc))
                            rc = VERR_NOT_SUPPORTED;
                        break;
                    }
                    default:
                        AssertMsgFailed(("Invalid request type %d\n", enmReq));
                }

                if (RT_SUCCESS(rc))
                {
                    /* Get matching complete event. */
                    rc = VDDbgIoLogEventTypeGetNext(hIoLogger, &enmEvent);
                    if (RT_SUCCESS(rc))
                    {
                        uint64_t idEvtComplete;
                        int rcReq;
                        uint64_t msDuration;

                        Assert(enmEvent == VDIOLOGEVENT_COMPLETE);
                        rc = VDDbgIoLogEventGetComplete(hIoLogger, &idEvtComplete, &rcReq,
                                                        &msDuration, &cbIo, cbBuf, pvBuf);
                        Assert(RT_FAILURE(rc) || idEvtComplete == idEvent);
                    }
                }

                if (RT_SUCCESS(rc))
                    rc = VDDbgIoLogEventTypeGetNext(hIoLogger, &enmEvent);
            }

            VDDbgIoLogDestroy(hIoLogger);
        }
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}


static DECLCALLBACK(int) vdScriptHandlerIoRngCreate(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    size_t cbPattern = 0;
    uint64_t uSeed = 0;
    const char *pcszSeeder = NULL;

    cbPattern  = paScriptArgs[0].u64;
    pcszSeeder = paScriptArgs[1].psz;
    uSeed      = paScriptArgs[2].u64;

    if (pGlob->pIoRnd)
    {
        RTPrintf("I/O RNG already exists\n");
        rc = VERR_INVALID_STATE;
    }
    else
    {
        uint64_t uSeedToUse = 0;

        if (!RTStrICmp(pcszSeeder, "manual"))
            uSeedToUse = uSeed;
        else if (!RTStrICmp(pcszSeeder, "time"))
            uSeedToUse = RTTimeSystemMilliTS();
        else if (!RTStrICmp(pcszSeeder, "system"))
        {
            RTRAND hRand;
            rc = RTRandAdvCreateSystemTruer(&hRand);
            if (RT_SUCCESS(rc))
            {
                RTRandAdvBytes(hRand, &uSeedToUse, sizeof(uSeedToUse));
                RTRandAdvDestroy(hRand);
            }
        }

        if (RT_SUCCESS(rc))
            rc = VDIoRndCreate(&pGlob->pIoRnd, cbPattern, uSeed);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerIoRngDestroy(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;

    if (pGlob->pIoRnd)
    {
        VDIoRndDestroy(pGlob->pIoRnd);
        pGlob->pIoRnd = NULL;
    }
    else
        RTPrintf("WARNING: No I/O RNG active, faulty script. Continuing\n");

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vdScriptHandlerIoPatternCreateFromNumber(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    size_t cbPattern = 0;
    const char *pcszName = NULL;
    uint64_t u64Pattern = 0;

    pcszName   = paScriptArgs[0].psz;
    cbPattern  = paScriptArgs[1].u64;
    u64Pattern = paScriptArgs[2].u64;

    PVDPATTERN pPattern = tstVDIoGetPatternByName(pGlob, pcszName);
    if (!pPattern)
    {
        pPattern = tstVDIoPatternCreate(pcszName, RT_ALIGN_Z(cbPattern, sizeof(uint64_t)));
        if (pPattern)
        {
            /* Fill the buffer. */
            void *pv = pPattern->pvPattern;

            while (pPattern->cbPattern > 0)
            {
                *((uint64_t*)pv)     = u64Pattern;
                pPattern->cbPattern -= sizeof(uint64_t);
                pv                   = (uint64_t *)pv + 1;
            }
            pPattern->cbPattern = cbPattern; /* Set to the desired size. (could be unaligned) */

            RTListAppend(&pGlob->ListPatterns, &pPattern->ListNode);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_ALREADY_EXISTS;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerIoPatternCreateFromFile(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszName = NULL;
    const char *pcszFile = NULL;

    pcszName = paScriptArgs[0].psz;
    pcszFile = paScriptArgs[1].psz;

    PVDPATTERN pPattern = tstVDIoGetPatternByName(pGlob, pcszName);
    if (!pPattern)
    {
        RTFILE hFile;
        uint64_t cbPattern = 0;

        rc = RTFileOpen(&hFile, pcszFile, RTFILE_O_DENY_NONE | RTFILE_O_OPEN | RTFILE_O_READ);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileGetSize(hFile, &cbPattern);
            if (RT_SUCCESS(rc))
            {
                pPattern = tstVDIoPatternCreate(pcszName, (size_t)cbPattern);
                if (pPattern)
                {
                    rc = RTFileRead(hFile, pPattern->pvPattern, (size_t)cbPattern, NULL);
                    if (RT_SUCCESS(rc))
                        RTListAppend(&pGlob->ListPatterns, &pPattern->ListNode);
                    else
                    {
                        RTMemFree(pPattern->pvPattern);
                        RTStrFree(pPattern->pszName);
                        RTMemFree(pPattern);
                    }
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            RTFileClose(hFile);
        }
    }
    else
        rc = VERR_ALREADY_EXISTS;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerIoPatternDestroy(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszName = NULL;

    pcszName = paScriptArgs[0].psz;

    PVDPATTERN pPattern = tstVDIoGetPatternByName(pGlob, pcszName);
    if (pPattern)
    {
        RTListNodeRemove(&pPattern->ListNode);
        RTMemFree(pPattern->pvPattern);
        RTStrFree(pPattern->pszName);
        RTMemFree(pPattern);
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerSleep(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    uint64_t cMillies = paScriptArgs[0].u64;

    rc = RTThreadSleep(cMillies);
    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerDumpFile(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszFile = NULL;
    const char *pcszPathToDump = NULL;

    pcszFile       = paScriptArgs[0].psz;
    pcszPathToDump = paScriptArgs[1].psz;

    /* Check for the file. */
    PVDFILE pIt = NULL;
    bool fFound = false;
    RTListForEach(&pGlob->ListFiles, pIt, VDFILE, Node)
    {
        if (!RTStrCmp(pIt->pszName, pcszFile))
        {
            fFound = true;
            break;
        }
    }

    if (fFound)
    {
        RTPrintf("Dumping memory file %s to %s, this might take some time\n", pcszFile, pcszPathToDump);
        rc = VDMemDiskWriteToFile(pIt->pMemDisk, pcszPathToDump);
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerCreateDisk(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;
    bool fVerify = false;

    pcszDisk = paScriptArgs[0].psz;
    fVerify  = paScriptArgs[1].f;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (pDisk)
        rc = VERR_ALREADY_EXISTS;
    else
    {
        pDisk = (PVDDISK)RTMemAllocZ(sizeof(VDDISK));
        if (pDisk)
        {
            pDisk->pszName = RTStrDup(pcszDisk);
            if (pDisk->pszName)
            {
                rc = VINF_SUCCESS;

                if (fVerify)
                {
                    rc = VDMemDiskCreate(&pDisk->pMemDiskVerify, 0 /* Growing */);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTCritSectInit(&pDisk->CritSectVerify);
                        if (RT_FAILURE(rc))
                            VDMemDiskDestroy(pDisk->pMemDiskVerify);
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    rc = VDCreate(pGlob->pInterfacesDisk, VDTYPE_HDD, &pDisk->pVD);

                    if (RT_SUCCESS(rc))
                        RTListAppend(&pGlob->ListDisks, &pDisk->ListNode);
                    else
                    {
                        if (fVerify)
                        {
                            RTCritSectDelete(&pDisk->CritSectVerify);
                            VDMemDiskDestroy(pDisk->pMemDiskVerify);
                        }
                        RTStrFree(pDisk->pszName);
                    }
                }
            }
            else
                rc = VERR_NO_MEMORY;

            if (RT_FAILURE(rc))
                RTMemFree(pDisk);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerDestroyDisk(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;

    pcszDisk = paScriptArgs[0].psz;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (pDisk)
    {
        RTListNodeRemove(&pDisk->ListNode);
        VDDestroy(pDisk->pVD);
        if (pDisk->pMemDiskVerify)
        {
            VDMemDiskDestroy(pDisk->pMemDiskVerify);
            RTCritSectDelete(&pDisk->CritSectVerify);
        }
        RTStrFree(pDisk->pszName);
        RTMemFree(pDisk);
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerCompareDisks(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk1 = NULL;
    PVDDISK pDisk1 = NULL;
    const char *pcszDisk2 = NULL;
    PVDDISK pDisk2 = NULL;

    pcszDisk1 = paScriptArgs[0].psz;
    pcszDisk2 = paScriptArgs[1].psz;

    pDisk1 = tstVDIoGetDiskByName(pGlob, pcszDisk1);
    pDisk2 = tstVDIoGetDiskByName(pGlob, pcszDisk2);

    if (pDisk1 && pDisk2)
    {
        uint8_t *pbBuf1 = (uint8_t *)RTMemAllocZ(16 * _1M);
        uint8_t *pbBuf2 = (uint8_t *)RTMemAllocZ(16 * _1M);
        if (pbBuf1 && pbBuf2)
        {
            uint64_t cbDisk1, cbDisk2;
            uint64_t uOffCur = 0;

            cbDisk1 = VDGetSize(pDisk1->pVD, VD_LAST_IMAGE);
            cbDisk2 = VDGetSize(pDisk2->pVD, VD_LAST_IMAGE);

            if (cbDisk1 != cbDisk2)
                RTPrintf("Disks differ in size %llu vs %llu\n", cbDisk1, cbDisk2);
            else
            {
                while (uOffCur < cbDisk1)
                {
                    size_t cbRead = RT_MIN(cbDisk1, 16 * _1M);

                    rc = VDRead(pDisk1->pVD, uOffCur, pbBuf1, cbRead);
                    if (RT_SUCCESS(rc))
                        rc = VDRead(pDisk2->pVD, uOffCur, pbBuf2, cbRead);

                    if (RT_SUCCESS(rc))
                    {
                        if (memcmp(pbBuf1, pbBuf2, cbRead))
                        {
                            RTPrintf("Disks differ at offset %llu\n", uOffCur);
                            rc = VERR_DEV_IO_ERROR;
                            break;
                        }
                    }
                    else
                    {
                        RTPrintf("Reading one disk at offset %llu failed\n", uOffCur);
                        break;
                    }

                    uOffCur += cbRead;
                    cbDisk1 -= cbRead;
                }
            }
            RTMemFree(pbBuf1);
            RTMemFree(pbBuf2);
        }
        else
        {
            if (pbBuf1)
                RTMemFree(pbBuf1);
            if (pbBuf2)
                RTMemFree(pbBuf2);
            rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerDumpDiskInfo(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = NULL;
    PVDDISK pDisk = NULL;

    pcszDisk = paScriptArgs[0].psz;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);

    if (pDisk)
        VDDumpImages(pDisk->pVD);
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerPrintMsg(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    RTPrintf("%s\n", paScriptArgs[0].psz);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vdScriptHandlerShowStatistics(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszFile = paScriptArgs[0].psz;

    /* Check for the file. */
    PVDFILE pIt = NULL;
    bool fFound = false;
    RTListForEach(&pGlob->ListFiles, pIt, VDFILE, Node)
    {
        if (!RTStrCmp(pIt->pszName, pcszFile))
        {
            fFound = true;
            break;
        }
    }

    if (fFound)
    {
        RTPrintf("Statistics %s: \n"
                 "               sync  reads=%u writes=%u flushes=%u\n"
                 "               async reads=%u writes=%u flushes=%u\n",
                 pcszFile,
                 pIt->cReads, pIt->cWrites, pIt->cFlushes,
                 pIt->cAsyncReads, pIt->cAsyncWrites, pIt->cAsyncFlushes);
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerResetStatistics(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszFile = paScriptArgs[0].psz;

    /* Check for the file. */
    PVDFILE pIt = NULL;
    bool fFound = false;
    RTListForEach(&pGlob->ListFiles, pIt, VDFILE, Node)
    {
        if (!RTStrCmp(pIt->pszName, pcszFile))
        {
            fFound = true;
            break;
        }
    }

    if (fFound)
    {
        pIt->cReads   = 0;
        pIt->cWrites  = 0;
        pIt->cFlushes = 0;

        pIt->cAsyncReads   = 0;
        pIt->cAsyncWrites  = 0;
        pIt->cAsyncFlushes = 0;
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerResize(PVDSCRIPTARG paScriptArgs, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    const char *pcszDisk = paScriptArgs[0].psz;
    uint64_t cbDiskNew = 0;
    PVDDISK pDisk = NULL;

    pDisk = tstVDIoGetDiskByName(pGlob, pcszDisk);
    if (pDisk)
    {
        rc = VDResize(pDisk->pVD, cbDiskNew, &pDisk->PhysGeom, &pDisk->LogicalGeom, NULL);
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileOpen(void *pvUser, const char *pszLocation,
                                         uint32_t fOpen,
                                         PFNVDCOMPLETED pfnCompleted,
                                         void **ppStorage)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fFound = false;

    /*
     * Some backends use ./ for paths, strip it.
     * @todo: Implement proper directory support for the
     * memory filesystem.
     */
    if (   strlen(pszLocation) >= 2
        && *pszLocation == '.'
        && pszLocation[1] == '/')
        pszLocation += 2;

    /* Check if the file exists. */
    PVDFILE pIt = NULL;
    RTListForEach(&pGlob->ListFiles, pIt, VDFILE, Node)
    {
        if (!RTStrCmp(pIt->pszName, pszLocation))
        {
            fFound = true;
            break;
        }
    }

    if ((fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_CREATE)
    {
        /* If the file exists delete the memory disk. */
        if (fFound)
            rc = VDMemDiskSetSize(pIt->pMemDisk, 0);
        else
        {
            /* Create completey new. */
            pIt = (PVDFILE)RTMemAllocZ(sizeof(VDFILE));
            if (pIt)
            {
                pIt->pszName = RTStrDup(pszLocation);

                if (pIt->pszName)
                {
                    rc = VDMemDiskCreate(&pIt->pMemDisk, 0);
                }
                else
                    rc = VERR_NO_MEMORY;

                if (RT_FAILURE(rc))
                {
                    if (pIt->pszName)
                        RTStrFree(pIt->pszName);
                    RTMemFree(pIt);
                }
            }
            else
                rc = VERR_NO_MEMORY;

            RTListAppend(&pGlob->ListFiles, &pIt->Node);
        }
    }
    else if ((fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_OPEN)
    {
        if (!fFound)
            rc = VERR_FILE_NOT_FOUND;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pIt);
        PVDSTORAGE pStorage = (PVDSTORAGE)RTMemAllocZ(sizeof(VDSTORAGE));
        if (!pStorage)
            rc = VERR_NO_MEMORY;

        pStorage->pFile = pIt;
        pStorage->pfnComplete = pfnCompleted;
        *ppStorage = pStorage;
    }

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileClose(void *pvUser, void *pStorage)
{
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    RTMemFree(pIoStorage);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstVDIoFileDelete(void *pvUser, const char *pcszFilename)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fFound = false;

    /*
     * Some backends use ./ for paths, strip it.
     * @todo: Implement proper directory support for the
     * memory filesystem.
     */
    if (   strlen(pcszFilename) >= 2
        && *pcszFilename == '.'
        && pcszFilename[1] == '/')
        pcszFilename += 2;

    /* Check if the file exists. */
    PVDFILE pIt = NULL;
    RTListForEach(&pGlob->ListFiles, pIt, VDFILE, Node)
    {
        if (!RTStrCmp(pIt->pszName, pcszFilename))
        {
            fFound = true;
            break;
        }
    }

    if (fFound)
    {
        RTListNodeRemove(&pIt->Node);
        VDMemDiskDestroy(pIt->pMemDisk);
        RTStrFree(pIt->pszName);
        RTMemFree(pIt);
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileMove(void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fFound = false;

    /* Check if the file exists. */
    PVDFILE pIt = NULL;
    RTListForEach(&pGlob->ListFiles, pIt, VDFILE, Node)
    {
        if (!RTStrCmp(pIt->pszName, pcszSrc))
        {
            fFound = true;
            break;
        }
    }

    if (fFound)
    {
        char *pszNew = RTStrDup(pcszDst);
        if (pszNew)
        {
            RTStrFree(pIt->pszName);
            pIt->pszName = pszNew;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileGetFreeSpace(void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    AssertPtrReturn(pcbFreeSpace, VERR_INVALID_POINTER);

    *pcbFreeSpace = ~0ULL; /** @todo: Implement */
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstVDIoFileGetModificationTime(void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    AssertPtrReturn(pModificationTime, VERR_INVALID_POINTER);

    /** @todo: Implement */
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstVDIoFileGetSize(void *pvUser, void *pStorage, uint64_t *pcbSize)
{
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    return VDMemDiskGetSize(pIoStorage->pFile->pMemDisk, pcbSize);
}

static DECLCALLBACK(int) tstVDIoFileSetSize(void *pvUser, void *pStorage, uint64_t cbSize)
{
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    return VDMemDiskSetSize(pIoStorage->pFile->pMemDisk, cbSize);
}

static DECLCALLBACK(int) tstVDIoFileWriteSync(void *pvUser, void *pStorage, uint64_t uOffset,
                                              const void *pvBuffer, size_t cbBuffer, size_t *pcbWritten)
{
    int rc = VINF_SUCCESS;
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    RTSGBUF SgBuf;
    RTSGSEG Seg;

    Seg.pvSeg = (void *)pvBuffer;
    Seg.cbSeg = cbBuffer;
    RTSgBufInit(&SgBuf, &Seg, 1);
    rc = VDMemDiskWrite(pIoStorage->pFile->pMemDisk, uOffset, cbBuffer, &SgBuf);
    if (RT_SUCCESS(rc))
    {
        pIoStorage->pFile->cWrites++;
        if (pcbWritten)
            *pcbWritten = cbBuffer;
    }

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileReadSync(void *pvUser, void *pStorage, uint64_t uOffset,
                                             void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    RTSGBUF SgBuf;
    RTSGSEG Seg;

    Seg.pvSeg = pvBuffer;
    Seg.cbSeg = cbBuffer;
    RTSgBufInit(&SgBuf, &Seg, 1);
    rc = VDMemDiskRead(pIoStorage->pFile->pMemDisk, uOffset, cbBuffer, &SgBuf);
    if (RT_SUCCESS(rc))
    {
        pIoStorage->pFile->cReads++;
        if (pcbRead)
            *pcbRead = cbBuffer;
    }

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileFlushSync(void *pvUser, void *pStorage)
{
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;
    pIoStorage->pFile->cFlushes++;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstVDIoFileReadAsync(void *pvUser, void *pStorage, uint64_t uOffset,
                                              PCRTSGSEG paSegments, size_t cSegments,
                                              size_t cbRead, void *pvCompletion,
                                              void **ppTask)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    rc = VDIoBackendMemTransfer(pGlob->pIoBackend, pIoStorage->pFile->pMemDisk, VDIOTXDIR_READ, uOffset,
                                cbRead, paSegments, cSegments, pIoStorage->pfnComplete, pvCompletion);
    if (RT_SUCCESS(rc))
    {
        pIoStorage->pFile->cAsyncReads++;
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
    }

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileWriteAsync(void *pvUser, void *pStorage, uint64_t uOffset,
                                               PCRTSGSEG paSegments, size_t cSegments,
                                               size_t cbWrite, void *pvCompletion,
                                               void **ppTask)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    rc = VDIoBackendMemTransfer(pGlob->pIoBackend, pIoStorage->pFile->pMemDisk, VDIOTXDIR_WRITE, uOffset,
                                cbWrite, paSegments, cSegments, pIoStorage->pfnComplete, pvCompletion);
    if (RT_SUCCESS(rc))
    {
        pIoStorage->pFile->cAsyncWrites++;
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
    }

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileFlushAsync(void *pvUser, void *pStorage, void *pvCompletion,
                                               void **ppTask)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    PVDSTORAGE pIoStorage = (PVDSTORAGE)pStorage;

    rc = VDIoBackendMemTransfer(pGlob->pIoBackend, pIoStorage->pFile->pMemDisk, VDIOTXDIR_FLUSH, 0,
                                0, NULL, 0, pIoStorage->pfnComplete, pvCompletion);
    if (RT_SUCCESS(rc))
    {
        pIoStorage->pFile->cAsyncFlushes++;
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
    }

    return rc;
}

static int tstVDIoTestInit(PVDIOTEST pIoTest, PVDTESTGLOB pGlob, bool fRandomAcc, uint64_t cbIo,
                           size_t cbBlkSize, uint64_t offStart, uint64_t offEnd,
                           unsigned uWriteChance, PVDPATTERN pPattern)
{
    int rc = VINF_SUCCESS;

    RT_ZERO(*pIoTest);
    pIoTest->fRandomAccess = fRandomAcc;
    pIoTest->cbIo          = cbIo;
    pIoTest->cbBlkIo       = cbBlkSize;
    pIoTest->offStart      = offStart;
    pIoTest->offEnd        = offEnd;
    pIoTest->uWriteChance  = uWriteChance;
    pIoTest->pIoRnd        = pGlob->pIoRnd;
    pIoTest->pPattern      = pPattern;

    if (fRandomAcc)
    {
        uint64_t cbRange = pIoTest->offEnd < pIoTest->offStart
                           ? pIoTest->offStart - pIoTest->offEnd
                           : pIoTest->offEnd - pIoTest->offStart;

        pIoTest->u.Rnd.cBlocks = cbRange / cbBlkSize + ((cbRange % cbBlkSize) ? 1 : 0);
        pIoTest->u.Rnd.cBlocksLeft = pIoTest->u.Rnd.cBlocks;
        pIoTest->u.Rnd.pbMapAccessed = (uint8_t *)RTMemAllocZ(pIoTest->u.Rnd.cBlocks / 8
                                                              + ((pIoTest->u.Rnd.cBlocks % 8)
                                                                ? 1
                                                                : 0));
        if (!pIoTest->u.Rnd.pbMapAccessed)
            rc = VERR_NO_MEMORY;
    }
    else
        pIoTest->u.offNext = pIoTest->offEnd < pIoTest->offStart ? pIoTest->offStart - cbBlkSize : offStart;

    return rc;
}

static void tstVDIoTestDestroy(PVDIOTEST pIoTest)
{
    if (pIoTest->fRandomAccess)
        RTMemFree(pIoTest->u.Rnd.pbMapAccessed);
}

static bool tstVDIoTestRunning(PVDIOTEST pIoTest)
{
    return pIoTest->cbIo > 0;
}

static bool tstVDIoTestReqOutstanding(PVDIOREQ pIoReq)
{
    return pIoReq->fOutstanding;
}

/**
 * Returns true with the given chance in percent.
 *
 * @returns true or false
 * @param   iPercentage   The percentage of the chance to return true.
 */
static bool tstVDIoTestIsTrue(PVDIOTEST pIoTest, int iPercentage)
{
    int uRnd = VDIoRndGetU32Ex(pIoTest->pIoRnd, 0, 100);

    return (uRnd < iPercentage); /* This should be enough for our purpose */
}

static int tstVDIoTestReqInit(PVDIOTEST pIoTest, PVDIOREQ pIoReq, void *pvUser)
{
    int rc = VINF_SUCCESS;

    if (pIoTest->cbIo)
    {
        /* Read or Write? */
        pIoReq->enmTxDir = tstVDIoTestIsTrue(pIoTest, pIoTest->uWriteChance) ? VDIOREQTXDIR_WRITE : VDIOREQTXDIR_READ;
        pIoReq->cbReq = RT_MIN(pIoTest->cbBlkIo, pIoTest->cbIo);
        pIoTest->cbIo -= pIoReq->cbReq;
        pIoReq->DataSeg.cbSeg = pIoReq->cbReq;

        if (pIoReq->enmTxDir == VDIOREQTXDIR_WRITE)
        {
            if (pIoTest->pPattern)
                rc = tstVDIoPatternGetBuffer(pIoTest->pPattern, &pIoReq->DataSeg.pvSeg, pIoReq->cbReq);
            else
                rc = VDIoRndGetBuffer(pIoTest->pIoRnd, &pIoReq->DataSeg.pvSeg, pIoReq->cbReq);
            AssertRC(rc);
        }
        else
        {
            /* Read */
            pIoReq->DataSeg.pvSeg = pIoReq->pvBufRead;
        }

        if (RT_SUCCESS(rc))
        {
            RTSgBufInit(&pIoReq->SgBuf, &pIoReq->DataSeg, 1);

            if (pIoTest->fRandomAccess)
            {
                int idx = -1;

                idx = ASMBitFirstClear(pIoTest->u.Rnd.pbMapAccessed, pIoTest->u.Rnd.cBlocks);

                /* In case this is the last request we don't need to search further. */
                if (pIoTest->u.Rnd.cBlocksLeft > 1)
                {
                    int idxIo;
                    idxIo = VDIoRndGetU32Ex(pIoTest->pIoRnd, idx, pIoTest->u.Rnd.cBlocks - 1);

                    /*
                     * If the bit is marked free use it, otherwise search for the next free bit
                     * and if that doesn't work use the first free bit.
                     */
                    if (ASMBitTest(pIoTest->u.Rnd.pbMapAccessed, idxIo))
                    {
                        idxIo = ASMBitNextClear(pIoTest->u.Rnd.pbMapAccessed, pIoTest->u.Rnd.cBlocks, idxIo);
                        if (idxIo != -1)
                            idx = idxIo;
                    }
                    else
                        idx = idxIo;
                }

                Assert(idx != -1);
                pIoReq->off = (uint64_t)idx * pIoTest->cbBlkIo;
                pIoTest->u.Rnd.cBlocksLeft--;
                if (!pIoTest->u.Rnd.cBlocksLeft)
                {
                    /* New round, clear everything. */
                    ASMBitClearRange(pIoTest->u.Rnd.pbMapAccessed, 0, pIoTest->u.Rnd.cBlocks);
                    pIoTest->u.Rnd.cBlocksLeft = pIoTest->u.Rnd.cBlocks;
                }
                else
                    ASMBitSet(pIoTest->u.Rnd.pbMapAccessed, idx);
            }
            else
            {
                pIoReq->off = pIoTest->u.offNext;
                if (pIoTest->offEnd < pIoTest->offStart)
                {
                    pIoTest->u.offNext = pIoTest->u.offNext == 0
                                         ? pIoTest->offEnd - pIoTest->cbBlkIo
                                         : RT_MAX(pIoTest->offEnd, pIoTest->u.offNext - pIoTest->cbBlkIo);
                }
                else
                {
                    pIoTest->u.offNext = pIoTest->u.offNext + pIoTest->cbBlkIo >= pIoTest->offEnd
                                         ? 0
                                         : RT_MIN(pIoTest->offEnd, pIoTest->u.offNext + pIoTest->cbBlkIo);
                }
            }
            pIoReq->pvUser = pvUser;
            pIoReq->fOutstanding = true;
        }
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}

static void tstVDIoTestReqComplete(void *pvUser1, void *pvUser2, int rcReq)
{
    PVDIOREQ pIoReq = (PVDIOREQ)pvUser1;
    RTSEMEVENT hEventSem = (RTSEMEVENT)pvUser2;
    PVDDISK pDisk = (PVDDISK)pIoReq->pvUser;

    LogFlow(("Request %d completed\n", pIoReq->idx));

    if (pDisk->pMemDiskVerify)
    {
        switch (pIoReq->enmTxDir)
        {
            case VDIOREQTXDIR_READ:
            {
                RTCritSectEnter(&pDisk->CritSectVerify);
                RTSgBufReset(&pIoReq->SgBuf);

                if (VDMemDiskCmp(pDisk->pMemDiskVerify, pIoReq->off, pIoReq->cbReq,
                                 &pIoReq->SgBuf))
                    RTPrintf("Corrupted disk at offset %llu!\n", pIoReq->off);
                RTCritSectLeave(&pDisk->CritSectVerify);
            }
            case VDIOREQTXDIR_WRITE:
            {
                RTCritSectEnter(&pDisk->CritSectVerify);
                RTSgBufReset(&pIoReq->SgBuf);

                int rc = VDMemDiskWrite(pDisk->pMemDiskVerify, pIoReq->off, pIoReq->cbReq,
                                        &pIoReq->SgBuf);
                AssertRC(rc);
                RTCritSectLeave(&pDisk->CritSectVerify);
                break;
            }
            case VDIOREQTXDIR_FLUSH:
            case VDIOREQTXDIR_DISCARD:
                break;
        }
    }

    ASMAtomicXchgBool(&pIoReq->fOutstanding, false);
    RTSemEventSignal(hEventSem);
    return;
}

/**
 * Returns the disk handle by name or NULL if not found
 *
 * @returns Disk handle or NULL if the disk could not be found.
 *
 * @param pGlob    Global test state.
 * @param pcszDisk Name of the disk to get.
 */
static PVDDISK tstVDIoGetDiskByName(PVDTESTGLOB pGlob, const char *pcszDisk)
{
    PVDDISK pIt = NULL;
    bool fFound = false;

    LogFlowFunc(("pGlob=%#p pcszDisk=%s\n", pGlob, pcszDisk));

    RTListForEach(&pGlob->ListDisks, pIt, VDDISK, ListNode)
    {
        if (!RTStrCmp(pIt->pszName, pcszDisk))
        {
            fFound = true;
            break;
        }
    }

    LogFlowFunc(("return %#p\n", fFound ? pIt : NULL));
    return fFound ? pIt : NULL;
}

/**
 * Returns the I/O pattern handle by name of NULL if not found.
 *
 * @returns I/O pattern handle or NULL if the pattern could not be found.
 *
 * @param pGlob    Global test state.
 * @param pcszName Name of the pattern.
 */
static PVDPATTERN tstVDIoGetPatternByName(PVDTESTGLOB pGlob, const char *pcszName)
{
    PVDPATTERN pIt = NULL;
    bool fFound = false;

    LogFlowFunc(("pGlob=%#p pcszName=%s\n", pGlob, pcszName));

    RTListForEach(&pGlob->ListPatterns, pIt, VDPATTERN, ListNode)
    {
        if (!RTStrCmp(pIt->pszName, pcszName))
        {
            fFound = true;
            break;
        }
    }

    LogFlowFunc(("return %#p\n", fFound ? pIt : NULL));
    return fFound ? pIt : NULL;
}

/**
 * Creates a new pattern with the given name and an
 * allocated pattern buffer.
 *
 * @returns Pointer to a new pattern buffer or NULL on failure.
 * @param   pcszName    Name of the pattern.
 * @param   cbPattern   Size of the pattern buffer.
 */
static PVDPATTERN tstVDIoPatternCreate(const char *pcszName, size_t cbPattern)
{
    PVDPATTERN pPattern  = (PVDPATTERN)RTMemAllocZ(sizeof(VDPATTERN));
    char      *pszName   = RTStrDup(pcszName);
    void      *pvPattern = RTMemAllocZ(cbPattern);

    if (pPattern && pszName && pvPattern)
    {
        pPattern->pszName   = pszName;
        pPattern->pvPattern = pvPattern;
        pPattern->cbPattern = cbPattern;
    }
    else
    {
        if (pPattern)
            RTMemFree(pPattern);
        if (pszName)
            RTStrFree(pszName);
        if (pvPattern)
            RTMemFree(pvPattern);

        pPattern  = NULL;
        pszName   = NULL;
        pvPattern = NULL;
    }

    return pPattern;
}

static int tstVDIoPatternGetBuffer(PVDPATTERN pPattern, void **ppv, size_t cb)
{
    AssertPtrReturn(pPattern, VERR_INVALID_POINTER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);

    if (cb > pPattern->cbPattern)
        return VERR_INVALID_PARAMETER;

    *ppv = pPattern->pvPattern;
    return VINF_SUCCESS;
}

/**
 * Executes the given I/O script using the new scripting engine.
 *
 * @returns nothing.
 *
 * @param pcszFilename    The script to execute.
 */
static void tstVDIoScriptRun(const char *pcszFilename)
{
    int rc = VINF_SUCCESS;
    VDTESTGLOB GlobTest;   /**< Global test data. */
    void *pvFile = NULL;
    size_t cbFile = 0;

    memset(&GlobTest, 0, sizeof(VDTESTGLOB));
    RTListInit(&GlobTest.ListFiles);
    RTListInit(&GlobTest.ListDisks);
    RTListInit(&GlobTest.ListPatterns);

    rc = RTFileReadAll(pcszFilename, &pvFile, &cbFile);
    if (RT_SUCCESS(rc))
    {
        char *pszScript = RTStrDupN((char *)pvFile, cbFile);
        RTFileReadAllFree(pvFile, cbFile);

        AssertPtr(pszScript);
        /* Init global test data. */
        GlobTest.VDIfError.pfnError     = tstVDError;
        GlobTest.VDIfError.pfnMessage   = tstVDMessage;

        rc = VDInterfaceAdd(&GlobTest.VDIfError.Core, "tstVDIo_VDIError", VDINTERFACETYPE_ERROR,
                            NULL, sizeof(VDINTERFACEERROR), &GlobTest.pInterfacesDisk);
        AssertRC(rc);

        GlobTest.VDIfIo.pfnOpen                = tstVDIoFileOpen;
        GlobTest.VDIfIo.pfnClose               = tstVDIoFileClose;
        GlobTest.VDIfIo.pfnDelete              = tstVDIoFileDelete;
        GlobTest.VDIfIo.pfnMove                = tstVDIoFileMove;
        GlobTest.VDIfIo.pfnGetFreeSpace        = tstVDIoFileGetFreeSpace;
        GlobTest.VDIfIo.pfnGetModificationTime = tstVDIoFileGetModificationTime;
        GlobTest.VDIfIo.pfnGetSize             = tstVDIoFileGetSize;
        GlobTest.VDIfIo.pfnSetSize             = tstVDIoFileSetSize;
        GlobTest.VDIfIo.pfnWriteSync           = tstVDIoFileWriteSync;
        GlobTest.VDIfIo.pfnReadSync            = tstVDIoFileReadSync;
        GlobTest.VDIfIo.pfnFlushSync           = tstVDIoFileFlushSync;
        GlobTest.VDIfIo.pfnReadAsync           = tstVDIoFileReadAsync;
        GlobTest.VDIfIo.pfnWriteAsync          = tstVDIoFileWriteAsync;
        GlobTest.VDIfIo.pfnFlushAsync          = tstVDIoFileFlushAsync;

        rc = VDInterfaceAdd(&GlobTest.VDIfIo.Core, "tstVDIo_VDIIo", VDINTERFACETYPE_IO,
                            &GlobTest, sizeof(VDINTERFACEIO), &GlobTest.pInterfacesImages);
        AssertRC(rc);

        /* Init I/O backend. */
        rc = VDIoBackendMemCreate(&GlobTest.pIoBackend);
        if (RT_SUCCESS(rc))
        {
            VDSCRIPTCTX hScriptCtx = NULL;
            rc = VDScriptCtxCreate(&hScriptCtx);
            if (RT_SUCCESS(rc))
            {
                rc = VDScriptCtxCallbacksRegister(hScriptCtx, g_aScriptActions, g_cScriptActions, &GlobTest);
                AssertRC(rc);

                rc = VDScriptCtxLoadScript(hScriptCtx, pszScript);
                if (RT_FAILURE(rc))
                {
                    RTPrintf("Loading the script failed rc=%Rrc\n", rc);
                }
                else
                    rc = VDScriptCtxCallFn(hScriptCtx, "main", NULL, 0);
                VDScriptCtxDestroy(hScriptCtx);
            }
            VDIoBackendMemDestroy(GlobTest.pIoBackend);
        }
        else
            RTPrintf("Creating the I/O backend failed rc=%Rrc\n");
    }
    else
        RTPrintf("Opening script failed rc=%Rrc\n", rc);
}

/**
 * Shows help message.
 */
static void printUsage(void)
{
    RTPrintf("Usage:\n"
             "--script <filename>    Script to execute\n");
}

static const RTGETOPTDEF g_aOptions[] =
{
    { "--script",   's', RTGETOPT_REQ_STRING }
};

int main(int argc, char *argv[])
{
    RTR3InitExe(argc, &argv, 0);
    int rc;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    char c;

    if (argc != 3)
    {
        printUsage();
        return RTEXITCODE_FAILURE;
    }

    rc = VDInit();
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    RTGetOptInit(&GetState, argc, argv, g_aOptions,
                 RT_ELEMENTS(g_aOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    while (   RT_SUCCESS(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 's':
                tstVDIoScriptRun(ValueUnion.psz);
                break;
            default:
                printUsage();
        }
    }

    rc = VDShutdown();
    if (RT_FAILURE(rc))
        RTPrintf("tstVDIo: unloading backends failed! rc=%Rrc\n", rc);

    return RTEXITCODE_SUCCESS;
}

