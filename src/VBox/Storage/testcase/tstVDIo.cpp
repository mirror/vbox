/** @file
 *
 * VBox HDD container test utility - I/O replay.
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
#define LOGGROUP LOGGROUP_DEFAULT
#include <VBox/vd.h>
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

#include "VDMemDisk.h"
#include "VDIoBackendMem.h"
#include "VDIoRnd.h"

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
    /** Completion callback of the VD layer. */
    PFNVDCOMPLETED pfnComplete;
} VDFILE, *PVDFILE;

/**
 * Global VD test state.
 */
typedef struct VDTESTGLOB
{
    /** HDD handle to operate on. */
    PVBOXHDD         pVD;
    /** Head of the active file list. */
    RTLISTNODE       ListFiles;
    /** Memory I/O backend. */
    PVDIOBACKENDMEM  pIoBackend;
    /** Error interface. */
    VDINTERFACE      VDIError;
    /** Error interface callbacks. */
    VDINTERFACEERROR VDIErrorCallbacks;
    /** Pointer to the per disk interface list. */
    PVDINTERFACE     pInterfacesDisk;
    /** I/O interface. */
    VDINTERFACE      VDIIo;
    /** I/O interface callbacks. */
    VDINTERFACEIO    VDIIoCallbacks;
    /** Pointer to the per image interface list. */
    PVDINTERFACE     pInterfacesImages;
    /** Physical CHS Geometry. */
    VDGEOMETRY       PhysGeom;
    /** Logical CHS geometry. */
    VDGEOMETRY       LogicalGeom;
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
    VDIOREQTXDIR_FLUSH
} VDIOREQTXDIR;

/**
 * I/O request.
 */
typedef struct VDIOREQ
{
    /** Transfer type. */
    VDIOREQTXDIR enmTxDir;
    /** slot index. */
    unsigned  idx;
    /** Start offset. */
    uint64_t  off;
    /** Size to transfer. */
    size_t    cbReq;
    /** S/G Buffer */
    RTSGBUF   SgBuf;
    /** Data segment */
    RTSGSEG   DataSeg;
    /** Flag whether the request is outstanding or not. */
    volatile bool fOutstanding;
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
    size_t      cbIo;
    unsigned    uWriteChance;
    PVDIORND    pIoRnd;
    uint64_t    offNext;
} VDIOTEST, *PVDIOTEST;

/**
 * Argument types.
 */
typedef enum VDSCRIPTARGTYPE
{
    /** Argument is a string. */
    VDSCRIPTARGTYPE_STRING = 0,
    /** Argument is a 64bit unsigned number. */
    VDSCRIPTARGTYPE_UNSIGNED_NUMBER,
    /** Argument is a 64bit signed number. */
    VDSCRIPTARGTYPE_SIGNED_NUMBER,
    /** Arugment is a unsigned 64bit range */
    VDSCRIPTARGTYPE_UNSIGNED_RANGE,
    /** Arugment is a boolean. */
    VDSCRIPTARGTYPE_BOOL
} VDSCRIPTARGTYPE;

/**
 * Script argument.
 */
typedef struct VDSCRIPTARG
{
    /** Argument identifier. */
    char            chId;
    /** Type of the argument. */
    VDSCRIPTARGTYPE enmType;
    /** Type depndent data. */
    union
    {
        /** String. */
        const char *pcszString;
        /** Bool. */
        bool        fFlag;
        /** unsigned number. */
        uint64_t    u64;
        /** Signed number. */
        int64_t     i64;
        /** Unsigned range. */
        struct
        {
            uint64_t Start;
            uint64_t End;
        } Range;
    } u;
} VDSCRIPTARG, *PVDSCRIPTARG;

/** Script action handler. */
typedef DECLCALLBACK(int) FNVDSCRIPTACTION(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
/** Pointer to a script action handler. */
typedef FNVDSCRIPTACTION *PFNVDSCRIPTACTION;

/**
 * Script argument descriptor.
 */
typedef struct VDSCRIPTARGDESC
{
    /** Name of the arugment. */
    const char      *pcszName;
    /** Identifier for the argument. */
    char            chId;
    /** Type of the argument. */
    VDSCRIPTARGTYPE enmType;
    /** Flags  */
    uint32_t        fFlags;
} VDSCRIPTARGDESC, *PVDSCRIPTARGDESC;
/** Pointer to a const script argument descriptor. */
typedef const VDSCRIPTARGDESC *PCVDSCRIPTARGDESC;

/** Flag whether the argument is mandatory. */
#define VDSCRIPTARGDESC_FLAG_MANDATORY   RT_BIT(0)
/** Flag whether the number can have a size suffix (K|M|G) */
#define VDSCRIPTARGDESC_FLAG_SIZE_SUFFIX RT_BIT(1)

/**
 * Script action.
 */
typedef struct VDSCRIPTACTION
{
    /** Action name. */
    const char              *pcszAction;
    /** Pointer to the arguments. */
    const PCVDSCRIPTARGDESC paArgDesc;
    /** Number of arugments in the array. */
    unsigned                cArgDescs;
    /** Pointer to the action handler. */
    PFNVDSCRIPTACTION       pfnHandler;
} VDSCRIPTACTION, *PVDSCRIPTACTION;

typedef const VDSCRIPTACTION *PCVDSCRIPTACTION;

static DECLCALLBACK(int) vdScriptHandlerCreate(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerOpen(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerIo(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerFlush(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerMerge(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerClose(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerIoRngCreate(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerIoRngDestroy(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);
static DECLCALLBACK(int) vdScriptHandlerSleep(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs);

/* create action */
const VDSCRIPTARGDESC g_aArgCreate[] =
{
    /* pcszName chId enmType                          fFlags */
    {"mode",    'm', VDSCRIPTARGTYPE_STRING,          VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"name",    'n', VDSCRIPTARGTYPE_STRING,          VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"backend", 'b', VDSCRIPTARGTYPE_STRING,          VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"size",    's', VDSCRIPTARGTYPE_UNSIGNED_NUMBER, VDSCRIPTARGDESC_FLAG_MANDATORY | VDSCRIPTARGDESC_FLAG_SIZE_SUFFIX},
};

/* open action */
const VDSCRIPTARGDESC g_aArgOpen[] =
{
    /* pcszName chId enmType                             fFlags */
    {"name",    'n', VDSCRIPTARGTYPE_STRING,             VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"backend", 'b', VDSCRIPTARGTYPE_STRING,             VDSCRIPTARGDESC_FLAG_MANDATORY}
};

/* write action */
const VDSCRIPTARGDESC g_aArgIo[] =
{
    /* pcszName    chId enmType                          fFlags */
    {"async",      'a', VDSCRIPTARGTYPE_BOOL,            0},
    {"max-reqs",   'l', VDSCRIPTARGTYPE_UNSIGNED_NUMBER, 0},
    {"mode",       'm', VDSCRIPTARGTYPE_STRING,          VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"size",       's', VDSCRIPTARGTYPE_UNSIGNED_NUMBER, VDSCRIPTARGDESC_FLAG_SIZE_SUFFIX},
    {"blocksize",  'b', VDSCRIPTARGTYPE_UNSIGNED_NUMBER, VDSCRIPTARGDESC_FLAG_MANDATORY | VDSCRIPTARGDESC_FLAG_SIZE_SUFFIX},
    {"off",        'o', VDSCRIPTARGTYPE_UNSIGNED_RANGE,  VDSCRIPTARGDESC_FLAG_SIZE_SUFFIX},
    {"reads",      'r', VDSCRIPTARGTYPE_UNSIGNED_NUMBER, VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"writes",     'w', VDSCRIPTARGTYPE_UNSIGNED_NUMBER, VDSCRIPTARGDESC_FLAG_MANDATORY}
};

/* flush action */
const VDSCRIPTARGDESC g_aArgFlush[] =
{
    /* pcszName  chId enmType                            fFlags */
    {"async",    'a', VDSCRIPTARGTYPE_BOOL,              0}
};

/* merge action */
const VDSCRIPTARGDESC g_aArgMerge[] =
{
    /* pcszName  chId enmType                            fFlags */
    {"forward",  'f', VDSCRIPTARGTYPE_BOOL,              VDSCRIPTARGDESC_FLAG_MANDATORY}
};

/* close action */
const VDSCRIPTARGDESC g_aArgClose[] =
{
    /* pcszName  chId enmType                            fFlags */
    {"mode",     'm', VDSCRIPTARGTYPE_STRING,            VDSCRIPTARGDESC_FLAG_MANDATORY},
    {"delete",   'd', VDSCRIPTARGTYPE_BOOL,              VDSCRIPTARGDESC_FLAG_MANDATORY}
};

/* I/O RNG create action */
const VDSCRIPTARGDESC g_aArgIoRngCreate[] =
{
    /* pcszName  chId enmType                            fFlags */
    {"size",     'd', VDSCRIPTARGTYPE_UNSIGNED_NUMBER,   VDSCRIPTARGDESC_FLAG_MANDATORY | VDSCRIPTARGDESC_FLAG_SIZE_SUFFIX},
    {"seed",     's', VDSCRIPTARGTYPE_UNSIGNED_NUMBER,   VDSCRIPTARGDESC_FLAG_MANDATORY}
};

/* Sleep */
const VDSCRIPTARGDESC g_aArgSleep[] =
{
    /* pcszName  chId enmType                            fFlags */
    {"time",     't', VDSCRIPTARGTYPE_UNSIGNED_NUMBER,   VDSCRIPTARGDESC_FLAG_MANDATORY},
};

const VDSCRIPTACTION g_aScriptActions[] =
{
    /* pcszAction    paArgDesc            cArgDescs                        pfnHandler */
    {"create",       g_aArgCreate,        RT_ELEMENTS(g_aArgCreate),       vdScriptHandlerCreate},
    {"open",         g_aArgOpen,          RT_ELEMENTS(g_aArgOpen),         vdScriptHandlerOpen},
    {"io",           g_aArgIo,            RT_ELEMENTS(g_aArgIo),           vdScriptHandlerIo},
    {"flush",        g_aArgFlush,         RT_ELEMENTS(g_aArgFlush),        vdScriptHandlerFlush},
    {"close",        g_aArgClose,         RT_ELEMENTS(g_aArgClose),        vdScriptHandlerClose},
    {"merge",        g_aArgMerge,         RT_ELEMENTS(g_aArgMerge),        vdScriptHandlerMerge},
    {"iorngcreate",  g_aArgIoRngCreate,   RT_ELEMENTS(g_aArgIoRngCreate),  vdScriptHandlerIoRngCreate},
    {"iorngdestroy", NULL,                0,                               vdScriptHandlerIoRngDestroy},
    {"sleep",        g_aArgSleep,         RT_ELEMENTS(g_aArgSleep),        vdScriptHandlerSleep},
};

const unsigned g_cScriptActions = RT_ELEMENTS(g_aScriptActions);

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

static int tstVDIoTestInit(PVDIOTEST pIoTest, PVDTESTGLOB pGlob, bool fRandomAcc, size_t cbIo,
                           size_t cbBlkSize, uint64_t offStart, uint64_t offEnd,
                           unsigned uWriteChance, unsigned uReadChance);
static bool tstVDIoTestRunning(PVDIOTEST pIoTest);
static bool tstVDIoTestReqOutstanding(PVDIOREQ pIoReq);
static int  tstVDIoTestReqInit(PVDIOTEST pIoTest, PVDIOREQ pIoReq);
static void tstVDIoTestReqComplete(void *pvUser1, void *pvUser2, int rcReq);

static DECLCALLBACK(int) vdScriptHandlerCreate(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
    uint64_t cbSize = 0;
    const char *pcszBackend = NULL;
    const char *pcszImage = NULL;
    bool fBase = false;

    for (unsigned i = 0; i < cScriptArgs; i++)
    {
        switch (paScriptArgs[i].chId)
        {
            case 'm':
            {
                if (!RTStrICmp(paScriptArgs[i].u.pcszString, "base"))
                    fBase = true;
                else if (!RTStrICmp(paScriptArgs[i].u.pcszString, "diff"))
                    fBase = false;
                else
                {
                    RTPrintf("Invalid image mode '%s' given\n", paScriptArgs[i].u.pcszString);
                    rc = VERR_INVALID_PARAMETER;
                }
                break;
            }
            case 'n':
            {
                pcszImage = paScriptArgs[i].u.pcszString;
                break;
            }
            case 'b':
            {
                pcszBackend = paScriptArgs[i].u.pcszString;
                break;
            }
            case 's':
            {
                cbSize = paScriptArgs[i].u.u64;
                break;
            }
            default:
                AssertMsgFailed(("Invalid argument given!\n"));
        }

        if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (fBase)
            rc = VDCreateBase(pGlob->pVD, pcszBackend, pcszImage, cbSize, 0, NULL,
                              &pGlob->PhysGeom, &pGlob->LogicalGeom,
                              NULL, VD_OPEN_FLAGS_ASYNC_IO, pGlob->pInterfacesImages, NULL);
        else
            rc = VDCreateDiff(pGlob->pVD, pcszBackend, pcszImage, 0, NULL, NULL, NULL, VD_OPEN_FLAGS_ASYNC_IO,
                              pGlob->pInterfacesImages, NULL);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerOpen(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
    const char *pcszBackend = NULL;
    const char *pcszImage = NULL;

    for (unsigned i = 0; i < cScriptArgs; i++)
    {
        switch (paScriptArgs[i].chId)
        {
            case 'n':
            {
                pcszImage = paScriptArgs[i].u.pcszString;
                break;
            }
            case 'b':
            {
                pcszBackend = paScriptArgs[i].u.pcszString;
                break;
            }
            default:
                AssertMsgFailed(("Invalid argument given!\n"));
        }

        if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        rc = VDOpen(pGlob->pVD, pcszBackend, pcszImage, VD_OPEN_FLAGS_ASYNC_IO, pGlob->pInterfacesImages);
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerIo(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
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
    uint8_t uReadChance = 0;

    offEnd = VDGetSize(pGlob->pVD, VD_LAST_IMAGE);
    if (offEnd == 0)
        return VERR_INVALID_STATE;
    cbIo = offEnd;

    for (unsigned i = 0; i < cScriptArgs; i++)
    {
        switch (paScriptArgs[i].chId)
        {
            case 'a':
            {
                fAsync = paScriptArgs[i].u.fFlag;
                break;
            }
            case 'l':
            {
                cMaxReqs = paScriptArgs[i].u.u64;
                break;
            }
            case 'm':
            {
                if (!RTStrICmp(paScriptArgs[i].u.pcszString, "seq"))
                    fRandomAcc = false;
                else if (!RTStrICmp(paScriptArgs[i].u.pcszString, "rnd"))
                    fRandomAcc = true;
                else
                {
                    RTPrintf("Invalid access mode '%s'\n", paScriptArgs[i].u.pcszString);
                    rc = VERR_INVALID_PARAMETER;
                }
                break;
            }
            case 's':
            {
                cbIo = paScriptArgs[i].u.u64;
                break;
            }
            case 'b':
            {
                cbBlkSize = paScriptArgs[i].u.u64;
                break;
            }
            case 'o':
            {
                offStart = paScriptArgs[i].u.Range.Start;
                offEnd = paScriptArgs[i].u.Range.End;
                break;
            }
            case 'r':
            {
                uReadChance = (uint8_t)paScriptArgs[i].u.u64;
                break;
            }
            case 'w':
            {
                uWriteChance = (uint8_t)paScriptArgs[i].u.u64;
                break;
            }
            default:
                AssertMsgFailed(("Invalid argument given!\n"));
        }

        if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        VDIOTEST IoTest;

        rc = tstVDIoTestInit(&IoTest, pGlob, fRandomAcc, cbIo, cbBlkSize, offStart, offEnd, uWriteChance, uReadChance);
        if (RT_SUCCESS(rc))
        {
            PVDIOREQ paIoReq = NULL;
            unsigned cMaxTasksOutstanding = fAsync ? cMaxReqs : 1;
            RTSEMEVENT EventSem;

            rc = RTSemEventCreate(&EventSem);
            paIoReq = (PVDIOREQ)RTMemAllocZ(cMaxTasksOutstanding * sizeof(VDIOREQ));
            if (paIoReq && RT_SUCCESS(rc))
            {
                for (unsigned i = 0; i < cMaxTasksOutstanding; i++)
                    paIoReq[i].idx = i;

                while (tstVDIoTestRunning(&IoTest))
                {
                    bool fTasksOutstanding = false;
                    unsigned idx = 0;

                    /* Submit all idling requests. */
                    while (   idx < cMaxTasksOutstanding
                           && tstVDIoTestRunning(&IoTest))
                    {
                        if (!tstVDIoTestReqOutstanding(&paIoReq[idx]))
                        {
                            rc = tstVDIoTestReqInit(&IoTest, &paIoReq[idx]);
                            AssertRC(rc);

                            if (RT_SUCCESS(rc))
                            {
                                if (!fAsync)
                                {
                                    switch (paIoReq[idx].enmTxDir)
                                    {
                                        case VDIOREQTXDIR_READ:
                                        {
                                            rc = VDRead(pGlob->pVD, paIoReq[idx].off, paIoReq[idx].DataSeg.pvSeg, paIoReq[idx].cbReq);
                                            RTMemFree(paIoReq[idx].DataSeg.pvSeg);
                                            break;
                                        }
                                        case VDIOREQTXDIR_WRITE:
                                        {
                                            rc = VDWrite(pGlob->pVD, paIoReq[idx].off, paIoReq[idx].DataSeg.pvSeg, paIoReq[idx].cbReq);
                                            break;
                                        }
                                        case VDIOREQTXDIR_FLUSH:
                                        {
                                            rc = VDFlush(pGlob->pVD);
                                            break;
                                        }
                                    }
                                    if (RT_SUCCESS(rc))
                                        idx++;
                                }
                                else
                                {
                                    switch (paIoReq[idx].enmTxDir)
                                    {
                                        case VDIOREQTXDIR_READ:
                                        {
                                            rc = VDAsyncRead(pGlob->pVD, paIoReq[idx].off, paIoReq[idx].cbReq, &paIoReq[idx].SgBuf,
                                                             tstVDIoTestReqComplete, &paIoReq[idx], EventSem);
                                            if (rc == VINF_VD_ASYNC_IO_FINISHED)
                                                RTMemFree(paIoReq[idx].DataSeg.pvSeg);
                                            break;
                                        }
                                        case VDIOREQTXDIR_WRITE:
                                        {
                                            rc = VDAsyncWrite(pGlob->pVD, paIoReq[idx].off, paIoReq[idx].cbReq, &paIoReq[idx].SgBuf,
                                                              tstVDIoTestReqComplete, &paIoReq[idx], EventSem);
                                            break;
                                        }
                                        case VDIOREQTXDIR_FLUSH:
                                        {
                                            rc = VDAsyncFlush(pGlob->pVD, tstVDIoTestReqComplete, &paIoReq[idx], EventSem);
                                            break;
                                        }
                                    }

                                    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                                    {
                                        idx++;
                                        fTasksOutstanding = true;
                                        rc = VINF_SUCCESS;
                                    }
                                    else if (rc == VINF_VD_ASYNC_IO_FINISHED)
                                    {
                                        ASMAtomicXchgBool(&paIoReq[idx].fOutstanding, false);
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

                RTSemEventDestroy(EventSem);
                RTMemFree(paIoReq);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerFlush(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
    bool fAsync = false;

    if (cScriptArgs == 1 && paScriptArgs[0].chId == 'a')
        fAsync = paScriptArgs[0].u.fFlag;

    if (fAsync)
    {
        /** @todo  */
        rc = VERR_NOT_IMPLEMENTED;
    }
    else
        rc = VDFlush(pGlob->pVD);

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerMerge(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) vdScriptHandlerClose(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
    bool fAll = false;
    bool fDelete = false;

    for (unsigned i = 0; i < cScriptArgs; i++)
    {
        switch (paScriptArgs[i].chId)
        {
            case 'm':
            {
                if (!RTStrICmp(paScriptArgs[i].u.pcszString, "all"))
                    fAll = true;
                else if (!RTStrICmp(paScriptArgs[i].u.pcszString, "single"))
                    fAll = false;
                else
                {
                    RTPrintf("Invalid mode '%s' given\n", paScriptArgs[i].u.pcszString);
                    rc = VERR_INVALID_PARAMETER;
                }
                break;
            }
            case 'd':
            {
                fDelete = paScriptArgs[i].u.fFlag;
                break;
            }
            default:
                AssertMsgFailed(("Invalid argument given!\n"));
        }

        if (RT_FAILURE(rc))
            break;
    }

    if (   RT_SUCCESS(rc)
        && fAll
        && fDelete)
    {
        RTPrintf("mode=all doesn't work with delete=yes\n");
        rc = VERR_INVALID_PARAMETER;
    }

    if (RT_SUCCESS(rc))
    {
        if (fAll)
            rc = VDCloseAll(pGlob->pVD);
        else
            rc = VDClose(pGlob->pVD, fDelete);
    }
    return rc;
}


static DECLCALLBACK(int) vdScriptHandlerIoRngCreate(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
    size_t cbPattern = 0;
    uint64_t uSeed = 0;

    for (unsigned i = 0; i < cScriptArgs; i++)
    {
        switch (paScriptArgs[i].chId)
        {
            case 'd':
            {
                cbPattern = paScriptArgs[i].u.u64;
                break;
            }
            case 's':
            {
                uSeed = paScriptArgs[i].u.u64;
                break;
            }
            default:
                AssertMsgFailed(("Invalid argument given!\n"));
        }
    }

    if (pGlob->pIoRnd)
    {
        RTPrintf("I/O RNG already exists\n");
        rc = VERR_INVALID_STATE;
    }
    else
        rc = VDIoRndCreate(&pGlob->pIoRnd, cbPattern, uSeed);

    return rc;
}

static DECLCALLBACK(int) vdScriptHandlerIoRngDestroy(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    if (pGlob->pIoRnd)
    {
        VDIoRndDestroy(pGlob->pIoRnd);
        pGlob->pIoRnd = NULL;
    }
    else
        RTPrintf("WARNING: No I/O RNG active, faulty script. Continuing\n");

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vdScriptHandlerSleep(PVDTESTGLOB pGlob, PVDSCRIPTARG paScriptArgs, unsigned cScriptArgs)
{
    int rc = VINF_SUCCESS;
    uint64_t cMillies = 0;

    for (unsigned i = 0; i < cScriptArgs; i++)
    {
        switch (paScriptArgs[i].chId)
        {
            case 't':
            {
                cMillies = paScriptArgs[i].u.u64;
                break;
            }
            default:
                AssertMsgFailed(("Invalid argument given!\n"));
        }
    }

    rc = RTThreadSleep(cMillies);
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

    if (fFound && pIt->pfnComplete)
        rc = VERR_FILE_LOCK_FAILED;
    else if ((fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_CREATE)
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
                pIt->pfnComplete = pfnCompleted;
                pIt->pszName     = RTStrDup(pszLocation);

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
        else
            pIt->pfnComplete = pfnCompleted;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pIt);
        *ppStorage = pIt;
    }

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileClose(void *pvUser, void *pStorage)
{
    PVDFILE pFile = (PVDFILE)pStorage;

    /* Mark as not busy. */
    pFile->pfnComplete = NULL;
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstVDIoFileDelete(void *pvUser, const char *pcszFilename)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    bool fFound = false;

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
    PVDFILE pFile = (PVDFILE)pStorage;

    return VDMemDiskGetSize(pFile->pMemDisk, pcbSize);
}

static DECLCALLBACK(int) tstVDIoFileSetSize(void *pvUser, void *pStorage, uint64_t cbSize)
{
    PVDFILE pFile = (PVDFILE)pStorage;

    return VDMemDiskSetSize(pFile->pMemDisk, cbSize);
}

static DECLCALLBACK(int) tstVDIoFileWriteSync(void *pvUser, void *pStorage, uint64_t uOffset,
                                              const void *pvBuffer, size_t cbBuffer, size_t *pcbWritten)
{
    int rc = VINF_SUCCESS;
    PVDFILE pFile = (PVDFILE)pStorage;

    RTSGBUF SgBuf;
    RTSGSEG Seg;

    Seg.pvSeg = (void *)pvBuffer;
    Seg.cbSeg = cbBuffer;
    RTSgBufInit(&SgBuf, &Seg, 1);
    rc = VDMemDiskWrite(pFile->pMemDisk, uOffset, cbBuffer, &SgBuf);
    if (RT_SUCCESS(rc) && pcbWritten)
        *pcbWritten = cbBuffer;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileReadSync(void *pvUser, void *pStorage, uint64_t uOffset,
                                             void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    PVDFILE pFile = (PVDFILE)pStorage;

    RTSGBUF SgBuf;
    RTSGSEG Seg;

    Seg.pvSeg = pvBuffer;
    Seg.cbSeg = cbBuffer;
    RTSgBufInit(&SgBuf, &Seg, 1);
    rc = VDMemDiskRead(pFile->pMemDisk, uOffset, cbBuffer, &SgBuf);
    if (RT_SUCCESS(rc) && pcbRead)
        *pcbRead = cbBuffer;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileFlushSync(void *pvUser, void *pStorage)
{
    /* nothing to do. */
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) tstVDIoFileReadAsync(void *pvUser, void *pStorage, uint64_t uOffset,
                                              PCRTSGSEG paSegments, size_t cSegments,
                                              size_t cbRead, void *pvCompletion,
                                              void **ppTask)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    PVDFILE pFile = (PVDFILE)pStorage;

    rc = VDIoBackendMemTransfer(pGlob->pIoBackend, pFile->pMemDisk, VDIOTXDIR_READ, uOffset,
                                cbRead, paSegments, cSegments, pFile->pfnComplete, pvCompletion);
    if (RT_SUCCESS(rc))
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileWriteAsync(void *pvUser, void *pStorage, uint64_t uOffset,
                                               PCRTSGSEG paSegments, size_t cSegments,
                                               size_t cbWrite, void *pvCompletion,
                                               void **ppTask)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    PVDFILE pFile = (PVDFILE)pStorage;

    rc = VDIoBackendMemTransfer(pGlob->pIoBackend, pFile->pMemDisk, VDIOTXDIR_WRITE, uOffset,
                                cbWrite, paSegments, cSegments, pFile->pfnComplete, pvCompletion);
    if (RT_SUCCESS(rc))
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;

    return rc;
}

static DECLCALLBACK(int) tstVDIoFileFlushAsync(void *pvUser, void *pStorage, void *pvCompletion,
                                               void **ppTask)
{
    int rc = VINF_SUCCESS;
    PVDTESTGLOB pGlob = (PVDTESTGLOB)pvUser;
    PVDFILE pFile = (PVDFILE)pStorage;

    rc = VDIoBackendMemTransfer(pGlob->pIoBackend, pFile->pMemDisk, VDIOTXDIR_FLUSH, 0,
                                0, NULL, 0, pFile->pfnComplete, pvCompletion);
    if (RT_SUCCESS(rc))
        rc = VERR_VD_ASYNC_IO_IN_PROGRESS;

    return rc;
}

static int tstVDIoTestInit(PVDIOTEST pIoTest, PVDTESTGLOB pGlob, bool fRandomAcc, size_t cbIo,
                           size_t cbBlkSize, uint64_t offStart, uint64_t offEnd,
                           unsigned uWriteChance, unsigned uReadChance)
{
    pIoTest->fRandomAccess = fRandomAcc;
    pIoTest->cbIo          = cbIo;
    pIoTest->cbBlkIo       = cbBlkSize;
    pIoTest->offStart      = offStart;
    pIoTest->offEnd        = offEnd;
    pIoTest->uWriteChance  = uWriteChance;
    pIoTest->pIoRnd        = pGlob->pIoRnd;
    pIoTest->offNext       = pIoTest->offEnd < pIoTest->offStart ? pIoTest->offEnd - cbBlkSize : 0;
    return VINF_SUCCESS;
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

    return (uRnd <= iPercentage); /* This should be enough for our purpose */
}

static int tstVDIoTestReqInit(PVDIOTEST pIoTest, PVDIOREQ pIoReq)
{
    int rc = VINF_SUCCESS;

    if (pIoTest->cbIo)
    {
        /* Read or Write? */
        pIoReq->enmTxDir = tstVDIoTestIsTrue(pIoTest, pIoTest->uWriteChance) ? VDIOREQTXDIR_WRITE : VDIOREQTXDIR_READ;
        pIoReq->cbReq = RT_MIN(pIoTest->cbBlkIo, pIoTest->cbIo);
        pIoTest->cbIo -= pIoReq->cbReq;
        pIoReq->DataSeg.cbSeg = pIoReq->cbReq;
        pIoReq->off           = pIoTest->offNext;

        if (pIoReq->enmTxDir == VDIOREQTXDIR_WRITE)
        {
            rc = VDIoRndGetBuffer(pIoTest->pIoRnd, &pIoReq->DataSeg.pvSeg, pIoReq->cbReq);
            AssertRC(rc);
        }
        else
        {
            /* Read */
            pIoReq->DataSeg.pvSeg = RTMemAlloc(pIoReq->cbReq);
            if (!pIoReq->DataSeg.pvSeg)
                rc = VERR_NO_MEMORY;
        }

        if (RT_SUCCESS(rc))
        {
            RTSgBufInit(&pIoReq->SgBuf, &pIoReq->DataSeg, 1);

            if (pIoTest->fRandomAccess)
            {
                /** @todo */
            }
            else
            {
                pIoTest->offNext = pIoTest->offEnd < pIoTest->offStart
                                   ? RT_MAX(pIoTest->offEnd, pIoTest->offNext - pIoTest->cbBlkIo)
                                   : RT_MIN(pIoTest->offEnd, pIoTest->offNext + pIoTest->cbBlkIo);
            }
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

    ASMAtomicXchgBool(&pIoReq->fOutstanding, false);
    RTSemEventSignal(hEventSem);
    return;
}

/**
 * Skips the characters until the given character is reached.
 *
 * @returns Start of the string with the given character
 *          or NULL if the string ended before.
 *
 * @param psz The string to skip.
 * @param ch  The character.
 */
static char *tstVDIoScriptSkipUntil(char *psz, char ch)
{
    while (   *psz != '\0'
           && *psz != ch)
        psz++;

    return psz;
}

/**
 * Skips the spaces of the current string.
 *
 * @returns Start of the string with a non space character
 *          or NULL if the string ended before.
 *
 * @param psz The string to skip.
 */
static char *tstVDIoScriptSkipSpace(char *psz)
{
    while (   *psz != '\0'
           && RT_C_IS_SPACE(*psz))
        psz++;

    return psz;
}

/**
 * Skips all characters until a space is reached of the current
 * string.
 *
 * @returns Start of the string with a space character
 *          or NULL if the string ended before.
 *
 * @param psz The string to skip.
 */
static char *tstVDIoScriptSkipNonSpace(char *psz)
{
    while (   *psz != '\0'
           && !RT_C_IS_SPACE(*psz))
        psz++;

    return psz;
}

/**
 * Parses one argument name, value pair.
 *
 * @returns IPRT status code.
 *
 * @param pVDScriptAction    Script action.
 * @param pcszName           Argument name.
 * @param pcszValue          Argument value.
 * @param pScriptArg         Where to fill in the parsed
 *                           argument.
 * @param pfMandatory        Where to store whether the argument
 *                           is mandatory.
 */
static int tstVDIoScriptArgumentParse(PCVDSCRIPTACTION pVDScriptAction, const char *pcszName,
                                      const char *pcszValue, PVDSCRIPTARG pScriptArg, bool *pfMandatory)
{
    int rc = VERR_NOT_FOUND;

    for (unsigned i = 0; i < pVDScriptAction->cArgDescs; i++)
    {
        if (!RTStrCmp(pVDScriptAction->paArgDesc[i].pcszName, pcszName))
        {
            rc = VINF_SUCCESS;

            switch (pVDScriptAction->paArgDesc[i].enmType)
            {
                case VDSCRIPTARGTYPE_BOOL:
                {
                    pScriptArg->enmType = VDSCRIPTARGTYPE_BOOL;
                    if (!RTStrICmp(pcszValue, "yes") || !RTStrICmp(pcszValue, "on"))
                        pScriptArg->u.fFlag = true;
                    else if (!RTStrICmp(pcszValue, "no") || !RTStrICmp(pcszValue, "off"))
                        pScriptArg->u.fFlag = false;
                    else
                    {
                        RTPrintf("Boolean argument malformed '%s'\n", pcszValue);
                        rc = VERR_INVALID_PARAMETER;
                    }
                    break;
                }
                case VDSCRIPTARGTYPE_SIGNED_NUMBER:
                {
                    pScriptArg->enmType = VDSCRIPTARGTYPE_SIGNED_NUMBER;
                    AssertMsgFailed(("todo\n"));
                    break;
                }
                case VDSCRIPTARGTYPE_STRING:
                {
                    pScriptArg->enmType = VDSCRIPTARGTYPE_STRING;
                    pScriptArg->u.pcszString = pcszValue;
                    break;
                }
                case VDSCRIPTARGTYPE_UNSIGNED_NUMBER:
                {
                    char *pszSuffix = NULL;

                    pScriptArg->enmType = VDSCRIPTARGTYPE_UNSIGNED_NUMBER;
                    rc = RTStrToUInt64Ex(pcszValue, &pszSuffix, 10, &pScriptArg->u.u64);
                    if (rc == VWRN_TRAILING_CHARS)
                    {
                        switch (*pszSuffix)
                        {
                            case 'k':
                            case 'K':
                            {
                                pScriptArg->u.u64 *= _1K;
                                break;
                            }
                            case 'm':
                            case 'M':
                            {
                                pScriptArg->u.u64 *= _1M;
                                break;
                            }
                            case 'g':
                            case 'G':
                            {
                                pScriptArg->u.u64 *= _1G;
                                break;
                            }
                            default:
                            {
                                RTPrintf("Invalid size suffix '%s'\n", pszSuffix);
                                rc = VERR_INVALID_PARAMETER;
                            }
                        }
                        if (rc != VERR_INVALID_PARAMETER)
                            rc = VINF_SUCCESS;
                    }

                    break;
                }
                case VDSCRIPTARGTYPE_UNSIGNED_RANGE:
                {
                    char *pszSuffix = NULL;

                    pScriptArg->enmType = VDSCRIPTARGTYPE_UNSIGNED_RANGE;
                    rc = RTStrToUInt64Ex(pcszValue, &pszSuffix, 10, &pScriptArg->u.Range.Start);
                    if (rc == VWRN_TRAILING_CHARS)
                    {
                        if (*pszSuffix != '-')
                        {
                            switch (*pszSuffix)
                            {
                                case 'k':
                                case 'K':
                                {
                                    pScriptArg->u.u64 *= _1K;
                                    break;
                                }
                                case 'm':
                                case 'M':
                                {
                                    pScriptArg->u.u64 *= _1M;
                                    break;
                                }
                                case 'g':
                                case 'G':
                                {
                                    pScriptArg->u.u64 *= _1G;
                                    break;
                                }
                                default:
                                {
                                    RTPrintf("Invalid size suffix '%s'\n", pszSuffix);
                                    rc = VERR_INVALID_PARAMETER;
                                }
                            }
                        }

                        if (*pszSuffix == '-')
                        {
                            pszSuffix++;
                            rc = RTStrToUInt64Ex(pszSuffix, &pszSuffix, 10, &pScriptArg->u.Range.End);
                            if (rc == VWRN_TRAILING_CHARS)
                            {
                                switch (*pszSuffix)
                                {
                                    case 'k':
                                    case 'K':
                                    {
                                        pScriptArg->u.Range.End *= _1K;
                                        break;
                                    }
                                    case 'm':
                                    case 'M':
                                    {
                                        pScriptArg->u.Range.End *= _1M;
                                        break;
                                    }
                                    case 'g':
                                    case 'G':
                                    {
                                        pScriptArg->u.Range.End *= _1G;
                                        break;
                                    }
                                    default:
                                    {
                                        RTPrintf("Invalid size suffix '%s'\n", pszSuffix);
                                        rc = VERR_INVALID_PARAMETER;
                                    }
                                }
                            }
                        }
                        else
                            rc = VERR_INVALID_PARAMETER;
                    }
                    else
                        rc = VERR_INVALID_PARAMETER;

                    if (rc == VERR_INVALID_PARAMETER)
                        RTPrintf("Invalid range format\n");
                    break;
                }
                default:
                    AssertMsgFailed(("Invalid script argument type\n"));
            }

            if (RT_SUCCESS(rc))
            {
                pScriptArg->chId = pVDScriptAction->paArgDesc[i].chId;
                *pfMandatory = !!(pVDScriptAction->paArgDesc[i].fFlags & VDSCRIPTARGDESC_FLAG_MANDATORY);
            }
            break;
        }
    }

    if (rc == VERR_NOT_FOUND)
        RTPrintf("Argument '%s' not found\n", pcszName);

    return rc;
}

/**
 * Parses the arguments of a action in the script.
 *
 * @returns IPRT status code.
 *
 * @param psz                Argument string.
 * @param pVDScriptAction    The script action to parses
 *                           arguments for.
 * @param paScriptArgs       Where to store the arguments.
 * @param pcScriptArgs       Where to store the actual number of
 *                           arguments parsed.
 */
static int tstVDIoScriptArgumentListParse(char *psz, PCVDSCRIPTACTION pVDScriptAction, PVDSCRIPTARG paScriptArgs, unsigned *pcScriptArgs)
{
    int rc = VINF_SUCCESS;
    unsigned cMandatoryArgsReq = 0;
    unsigned cScriptArgs = 0;

    /* Count the number of mandatory arguments first. */
    for (unsigned i = 0; i < pVDScriptAction->cArgDescs; i++)
        if (pVDScriptAction->paArgDesc[i].fFlags & VDSCRIPTARGDESC_FLAG_MANDATORY)
            cMandatoryArgsReq++;

    /* One argument is given in the form name=value. */
    *pcScriptArgs = 0;

    while (    psz
           && *psz != '\0')
    {
        const char *pcszName = psz;

        psz = tstVDIoScriptSkipUntil(psz, '=');
        if (psz != '\0')
        {
            *psz = '\0'; /* Overwrite */
            psz++;
            const char *pcszValue = psz;

            psz = tstVDIoScriptSkipNonSpace(psz);
            if (psz != '\0')
            {
                *psz = '\0'; /* Overwrite */
                psz++;
                psz = tstVDIoScriptSkipSpace(psz);

                /* We have the name and value pair now. */
                bool fMandatory;
                rc = tstVDIoScriptArgumentParse(pVDScriptAction, pcszName, pcszValue, &paScriptArgs[cScriptArgs], &fMandatory);
                if (RT_SUCCESS(rc))
                {
                    if (fMandatory)
                        cMandatoryArgsReq--;
                    cScriptArgs++;
                }
            }
            else
            {
                RTPrintf("Value missing for argument '%s'\n", pcszName);
                rc = VERR_INVALID_STATE;
                break;
            }
        }
        else
        {
            RTPrintf("Argument in invalid form\n");
            rc = VERR_INVALID_STATE;
            break;
        }
    }

    if (   RT_SUCCESS(rc)
        && cMandatoryArgsReq)
    {
        /* No arguments anymore but there are still mandatory arguments left. */
        RTPrintf("There are %u arguments missing for script action '%s\n", pVDScriptAction->pcszAction);
        rc = VERR_INVALID_STATE;
    }

    if (RT_SUCCESS(rc))
        *pcScriptArgs = cScriptArgs;

    return rc;
}

/**
 * Executes the script pointed to by the given stream.
 *
 * @returns IPRT status code.
 *
 * @param pStrm    The stream handle of the script.
 * @param pGlob    Global test data.
 */
static int tstVDIoScriptExecute(PRTSTREAM pStrm, PVDTESTGLOB pGlob)
{
    int rc = VINF_SUCCESS;
    char abBuffer[0x1000]; /* Current assumption that a line is never longer than 4096 bytes. */
    PVDSCRIPTARG paScriptArgs = NULL;
    unsigned cScriptArgsMax = 0;

    do
    {
        memset(abBuffer, 0, sizeof(abBuffer));
        rc = RTStrmGetLine(pStrm, abBuffer, sizeof(abBuffer));
        if (RT_SUCCESS(rc))
        {
            const char *pcszAction = NULL;
            char *psz = abBuffer;

            /* Skip space */
            psz = tstVDIoScriptSkipSpace(psz);
            if (psz != '\0')
            {
                PCVDSCRIPTACTION pVDScriptAction = NULL;

                /* Get the action name. */
                pcszAction = psz;

                psz = tstVDIoScriptSkipNonSpace(psz);
                if (psz != '\0')
                {
                    Assert(RT_C_IS_SPACE(*psz));
                    *psz++ = '\0';
                }

                /* Find the action. */
                for (unsigned i = 0; i < g_cScriptActions; i++)
                {
                    if (!RTStrCmp(pcszAction, g_aScriptActions[i].pcszAction))
                    {
                        pVDScriptAction = &g_aScriptActions[i];
                        break;
                    }
                }

                if (pVDScriptAction)
                {
                    /* Parse arguments. */
                    if (cScriptArgsMax < pVDScriptAction->cArgDescs)
                    {
                        /* Increase arguments array. */
                        if (paScriptArgs)
                            RTMemFree(paScriptArgs);

                        cScriptArgsMax = pVDScriptAction->cArgDescs;
                        paScriptArgs = (PVDSCRIPTARG)RTMemAllocZ(cScriptArgsMax * sizeof(VDSCRIPTARG));
                    }

                    if (paScriptArgs)
                    {
                        unsigned cScriptArgs;

                        rc = tstVDIoScriptArgumentListParse(psz, pVDScriptAction, paScriptArgs, &cScriptArgs);
                        if (RT_SUCCESS(rc))
                        {
                            /* Execute the handler. */
                            rc = pVDScriptAction->pfnHandler(pGlob, paScriptArgs, cScriptArgs);
                        }
                    }
                    else
                    {
                        RTPrintf("Out of memory while allocating argument array for script action %s\n", pcszAction);
                        rc = VERR_NO_MEMORY;
                    }
                }
                else
                {
                    RTPrintf("Script action %s is not known\n", pcszAction);
                    rc = VERR_NOT_FOUND;
                }
            }
            else
            {
                RTPrintf("Missing action name\n");
                rc = VERR_INVALID_STATE;
            }
        }
    } while(RT_SUCCESS(rc));

    if (rc == VERR_EOF)
    {
        RTPrintf("Successfully executed I/O script\n");
        rc = VINF_SUCCESS;
    }
    return rc;
}

/**
 * Executes the given I/O script.
 *
 * @returns nothing.
 *
 * @param pcszFilename    The script to execute.
 */
static void tstVDIoScriptRun(const char *pcszFilename)
{
    int rc = VINF_SUCCESS;
    PRTSTREAM pScriptStrm; /**< Stream of the script file. */
    VDTESTGLOB GlobTest;   /**< Global test data. */

    memset(&GlobTest, 0, sizeof(VDTESTGLOB));
    RTListInit(&GlobTest.ListFiles);

    rc = RTStrmOpen(pcszFilename, "r", &pScriptStrm);
    if (RT_SUCCESS(rc))
    {
        /* Init global test data. */
        GlobTest.VDIErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
        GlobTest.VDIErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
        GlobTest.VDIErrorCallbacks.pfnError     = tstVDError;
        GlobTest.VDIErrorCallbacks.pfnMessage   = tstVDMessage;

        rc = VDInterfaceAdd(&GlobTest.VDIError, "tstVDIo_VDIError", VDINTERFACETYPE_ERROR,
                            &GlobTest.VDIErrorCallbacks, NULL, &GlobTest.pInterfacesDisk);
        AssertRC(rc);

        GlobTest.VDIIoCallbacks.cbSize                 = sizeof(VDINTERFACEIO);
        GlobTest.VDIIoCallbacks.enmInterface           = VDINTERFACETYPE_IO;
        GlobTest.VDIIoCallbacks.pfnOpen                = tstVDIoFileOpen;
        GlobTest.VDIIoCallbacks.pfnClose               = tstVDIoFileClose;
        GlobTest.VDIIoCallbacks.pfnDelete              = tstVDIoFileDelete;
        GlobTest.VDIIoCallbacks.pfnMove                = tstVDIoFileMove;
        GlobTest.VDIIoCallbacks.pfnGetFreeSpace        = tstVDIoFileGetFreeSpace;
        GlobTest.VDIIoCallbacks.pfnGetModificationTime = tstVDIoFileGetModificationTime;
        GlobTest.VDIIoCallbacks.pfnGetSize             = tstVDIoFileGetSize;
        GlobTest.VDIIoCallbacks.pfnSetSize             = tstVDIoFileSetSize;
        GlobTest.VDIIoCallbacks.pfnWriteSync           = tstVDIoFileWriteSync;
        GlobTest.VDIIoCallbacks.pfnReadSync            = tstVDIoFileReadSync;
        GlobTest.VDIIoCallbacks.pfnFlushSync           = tstVDIoFileFlushSync;
        GlobTest.VDIIoCallbacks.pfnReadAsync           = tstVDIoFileReadAsync;
        GlobTest.VDIIoCallbacks.pfnWriteAsync          = tstVDIoFileWriteAsync;
        GlobTest.VDIIoCallbacks.pfnFlushAsync          = tstVDIoFileFlushAsync;

        rc = VDInterfaceAdd(&GlobTest.VDIIo, "tstVDIo_VDIIo", VDINTERFACETYPE_IO,
                            &GlobTest.VDIIoCallbacks, &GlobTest, &GlobTest.pInterfacesImages);
        AssertRC(rc);

        /* Init I/O backend. */
        rc = VDIoBackendMemCreate(&GlobTest.pIoBackend);
        if (RT_SUCCESS(rc))
        {
            rc = VDCreate(GlobTest.pInterfacesDisk, VDTYPE_HDD, &GlobTest.pVD);
            if (RT_SUCCESS(rc))
            {
                /* Execute the script. */
                rc = tstVDIoScriptExecute(pScriptStrm, &GlobTest);
                if (RT_FAILURE(rc))
                {
                    RTPrintf("Executing the script stream failed rc=%Rrc\n", rc);
                }
            }
            else
                RTPrintf("Failed to create disk container rc=%Rrc\n", rc);
            VDIoBackendMemDestroy(GlobTest.pIoBackend);
        }
        else
            RTPrintf("Creating the I/O backend failed rc=%Rrc\n");

        RTStrmClose(pScriptStrm);
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
             "--script <filename>    Script to execute\n"
             "--replay <filename>    Log to replay (not implemented yet)\n");
}

static const RTGETOPTDEF g_aOptions[] =
{
    { "--script",   's', RTGETOPT_REQ_STRING },
    { "--replay",   'r', RTGETOPT_REQ_STRING },
};

int main(int argc, char *argv[])
{
    RTR3Init();
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
            case 'r':
                RTPrintf("Replaying I/O logs is not implemented yet\n");
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

