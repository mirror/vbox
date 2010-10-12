/* $Id$ */
/** @file
 *
 * IO helper for IAppliance COM class implementations.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
 *   Header Files                                                             *
 ******************************************************************************/

#include "ProgressImpl.h"
#include "ApplianceImpl.h"
#include "ApplianceImplPrivate.h"

#include <iprt/tar.h>
#include <iprt/sha.h>
#include <iprt/path.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <VBox/VBoxHDD.h>

/******************************************************************************
 *   Structures and Typedefs                                                  *
 ******************************************************************************/

typedef struct RTFILESTORAGEINTERNAL
{
    /** File handle. */
    RTFILE         file;
    /** Completion callback. */
    PFNVDCOMPLETED pfnCompleted;
} RTFILESTORAGEINTERNAL, *PRTFILESTORAGEINTERNAL;

typedef struct RTTARSTORAGEINTERNAL
{
    /** Tar handle. */
    RTTARFILE      file;
    /** Completion callback. */
    PFNVDCOMPLETED pfnCompleted;
} RTTARSTORAGEINTERNAL, *PRTTARSTORAGEINTERNAL;

typedef struct SHA1STORAGEINTERNAL
{
    /** Completion callback. */
    PFNVDCOMPLETED pfnCompleted;
    /** Storage handle for the next callback in chain. */
    void *pvStorage;
    /** Memory buffer used for caching and SHA1 calculation. */
    char *pcBuf;
    /** Size of the memory buffer. */
    size_t cbBuf;
    /** Memory buffer for writing zeros. */
    void *pvZeroBuf;
    /** Size of the zero memory buffer. */
    size_t cbZeroBuf;
    /** Current position in the caching memory buffer. */
    size_t cbCurBuf;
    /** Current absolute position. */
    uint64_t cbCurAll;
    /** Current real position in the file. */
    uint64_t cbCurFile;
    /** Handle of the SHA1 worker thread. */
    RTTHREAD pMfThread;
    /** Status of the worker thread. */
    volatile uint32_t u32Status;
    /** Event for signaling a new status. */
    RTSEMEVENT newStatusEvent;
    /** Event for signaling a finished SHA1 calculation. */
    RTSEMEVENT calcFinishedEvent;
    /** SHA1 calculation context. */
    RTSHA1CONTEXT ctx;
} SHA1STORAGEINTERNAL, *PSHA1STORAGEINTERNAL;

/******************************************************************************
 *   Defined Constants And Macros                                             *
 ******************************************************************************/

#define STATUS_WAIT UINT32_C(0)
#define STATUS_CALC UINT32_C(1)
#define STATUS_END  UINT32_C(3)

/* Enable for getting some flow history. */
#if 0
# define DEBUG_PRINT_FLOW() RTPrintf("%s\n", __FUNCTION__)
#else
# define DEBUG_PRINT_FLOW() do {} while(0)
#endif

/******************************************************************************
 *   Internal Functions                                                       *
 ******************************************************************************/

/******************************************************************************
 *   Internal: RTFile interface
 ******************************************************************************/

static int rtFileOpenCallback(void * /* pvUser */, const char *pszLocation, uint32_t fOpen,
                              PFNVDCOMPLETED pfnCompleted, void **ppInt)
{
    /* Validate input. */
    AssertPtrReturn(ppInt, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnCompleted, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)RTMemAllocZ(sizeof(RTFILESTORAGEINTERNAL));
    if (!pInt)
        return VERR_NO_MEMORY;

    pInt->pfnCompleted = pfnCompleted;

    int rc = RTFileOpen(&pInt->file, pszLocation, fOpen);

    if (RT_FAILURE(rc))
        RTMemFree(pInt);
    else
        *ppInt = pInt;

    return rc;
}

static int rtFileCloseCallback(void * /* pvUser */, void *pvStorage)
{
    /* Validate input. */
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    int rc = RTFileClose(pInt->file);

    /* Cleanup */
    RTMemFree(pInt);

    return rc;
}

static int rtFileDeleteCallback(void * /* pvUser */, const char *pcszFilename)
{
    DEBUG_PRINT_FLOW();

    return RTFileDelete(pcszFilename);
}

static int rtFileMoveCallback(void * /* pvUser */, const char *pcszSrc, const char *pcszDst, unsigned fMove)
{
    DEBUG_PRINT_FLOW();

    return RTFileMove(pcszSrc, pcszDst, fMove);
}

static int rtFileGetFreeSpaceCallback(void * /* pvUser */, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    /* Validate input. */
    AssertPtrReturn(pcszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbFreeSpace, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

static int rtFileGetModificationTimeCallback(void * /* pvUser */, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    /* Validate input. */
    AssertPtrReturn(pcszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(pModificationTime, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

static int rtFileGetSizeCallback(void * /* pvUser */, void *pvStorage, uint64_t *pcbSize)
{
    /* Validate input. */
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)pvStorage;

//    DEBUG_PRINT_FLOW();

    return RTFileGetSize(pInt->file, pcbSize);
}

static int rtFileSetSizeCallback(void * /* pvUser */, void *pvStorage, uint64_t cbSize)
{
    /* Validate input. */
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    return RTFileSetSize(pInt->file, cbSize);
}

static int rtFileWriteSyncCallback(void * /* pvUser */, void *pvStorage, uint64_t uOffset,
                                   const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    /* Validate input. */
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)pvStorage;

    return RTFileWriteAt(pInt->file, uOffset, pvBuf, cbWrite, pcbWritten);
}

static int rtFileReadSyncCallback(void * /* pvUser */, void *pvStorage, uint64_t uOffset,
                                  void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    /* Validate input. */
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)pvStorage;

    return RTFileReadAt(pInt->file, uOffset, pvBuf, cbRead, pcbRead);
}

static int rtFileFlushSyncCallback(void * /* pvUser */, void *pvStorage)
{
    /* Validate input. */
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    PRTFILESTORAGEINTERNAL pInt = (PRTFILESTORAGEINTERNAL)pvStorage;

    return RTFileFlush(pInt->file);
}

/******************************************************************************
 *   Internal: RTTar interface
 ******************************************************************************/

static int rtTarOpenCallback(void *pvUser, const char *pszLocation, uint32_t fOpen,
                             PFNVDCOMPLETED pfnCompleted, void **ppInt)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(ppInt, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnCompleted, VERR_INVALID_PARAMETER);

    RTTAR tar = (RTTAR)pvUser;

    DEBUG_PRINT_FLOW();

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)RTMemAllocZ(sizeof(RTTARSTORAGEINTERNAL));
    if (!pInt)
        return VERR_NO_MEMORY;

    pInt->pfnCompleted = pfnCompleted;

    int rc = RTTarFileOpen(tar, &pInt->file, RTPathFilename(pszLocation), fOpen);

    if (RT_FAILURE(rc))
        RTMemFree(pInt);
    else
        *ppInt = pInt;

    return rc;
}

static int rtTarCloseCallback(void *pvUser, void *pvStorage)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    int rc = RTTarFileClose(pInt->file);

    /* Cleanup */
    RTMemFree(pInt);

    return rc;
}

static int rtTarDeleteCallback(void *pvUser, const char *pcszFilename)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszFilename, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

static int rtTarMoveCallback(void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned /* fMove */)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszSrc, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDst, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

static int rtTarGetFreeSpaceCallback(void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbFreeSpace, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

static int rtTarGetModificationTimeCallback(void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(pModificationTime, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

static int rtTarGetSizeCallback(void *pvUser, void *pvStorage, uint64_t *pcbSize)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)pvStorage;

//    DEBUG_PRINT_FLOW();

    return RTTarFileGetSize(pInt->file, pcbSize);
}

static int rtTarSetSizeCallback(void *pvUser, void *pvStorage, uint64_t cbSize)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    return RTTarFileSetSize(pInt->file, cbSize);
}

static int rtTarWriteSyncCallback(void *pvUser, void *pvStorage, uint64_t uOffset,
                                  const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    return RTTarFileWriteAt(pInt->file, uOffset, pvBuf, cbWrite, pcbWritten);
}

static int rtTarReadSyncCallback(void *pvUser, void *pvStorage, uint64_t uOffset,
                                 void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    return RTTarFileReadAt(pInt->file, uOffset, pvBuf, cbRead, pcbRead);
}

static int rtTarFlushSyncCallback(void *pvUser, void *pvStorage)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    DEBUG_PRINT_FLOW();

    return VERR_NOT_IMPLEMENTED;
}

/******************************************************************************
 *   Internal: Sha1 interface
 ******************************************************************************/

DECLCALLBACK(int) sha1CalcWorkerThread(RTTHREAD /* aThread */, void *pvUser)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvUser;

    int rc = VINF_SUCCESS;
    bool fLoop = true;
    while(fLoop)
    {
        /* What should we do next? */
        uint32_t u32Status = ASMAtomicReadU32(&pInt->u32Status);
        switch (u32Status)
        {
            case STATUS_WAIT:
            {
                /* Wait for new work. */
                rc = RTSemEventWait(pInt->newStatusEvent, 100);
                if (   RT_FAILURE(rc)
                    && rc != VERR_TIMEOUT)
                    fLoop = false;
                break;
            }
            case STATUS_CALC:
            {
                /* Update the SHA1 context with the next data block. */
                RTSha1Update(&pInt->ctx, pInt->pcBuf, pInt->cbCurBuf);
                /* Reset the thread status and signal the main thread that we
                   are finished. */
                ASMAtomicWriteU32(&pInt->u32Status, STATUS_WAIT);
                rc = RTSemEventSignal(pInt->calcFinishedEvent);
                break;
            }
            case STATUS_END:
            {
                /* End signaled */
                fLoop = false;
                break;
            }
        }
    }
    return rc;
}

DECLINLINE(int) sha1SignalManifestThread(PSHA1STORAGEINTERNAL pInt, uint32_t uStatus)
{
    ASMAtomicWriteU32(&pInt->u32Status, uStatus);
    return RTSemEventSignal(pInt->newStatusEvent);
}

DECLINLINE(int) sha1WaitForManifestThreadFinished(PSHA1STORAGEINTERNAL pInt)
{
//    RTPrintf("start\n");
    int rc = VINF_SUCCESS;
    for(;;)
    {
//        RTPrintf(" wait\n");
        if (ASMAtomicReadU32(&pInt->u32Status) != STATUS_CALC)
            break;
        rc = RTSemEventWait(pInt->calcFinishedEvent, 100);
    }
    if (rc == VERR_TIMEOUT)
        rc = VINF_SUCCESS;
    return rc;
}

DECLINLINE(int) sha1FlushCurBuf(PVDINTERFACE pIO, PVDINTERFACEIO pCallbacks, PSHA1STORAGEINTERNAL pInt, bool fCreateDigest)
{
    int rc = VINF_SUCCESS;
    if (fCreateDigest)
    {
        /* Let the sha1 worker thread start immediately. */
        rc = sha1SignalManifestThread(pInt, STATUS_CALC);
        if (RT_FAILURE(rc))
            return rc;
    }
    /* Write the buffer content to disk. */
    size_t cbAllWritten = 0;
    for(;;)
    {
        /* Finished? */
        if (cbAllWritten == pInt->cbCurBuf)
            break;
        size_t cbToWrite = pInt->cbCurBuf - cbAllWritten;
        size_t cbWritten = 0;
        rc = pCallbacks->pfnWriteSync(pIO->pvUser, pInt->pvStorage, pInt->cbCurFile, &pInt->pcBuf[cbAllWritten], cbToWrite, &cbWritten);
        if (RT_FAILURE(rc))
            return rc;
        pInt->cbCurFile += cbWritten;
        cbAllWritten += cbWritten;
    }
    if (fCreateDigest)
    {
        /* Wait until the sha1 worker thread has finished. */
        rc = sha1WaitForManifestThreadFinished(pInt);
    }
    if (RT_SUCCESS(rc))
        pInt->cbCurBuf = 0;

    return rc;
}

static int sha1OpenCallback(void *pvUser, const char *pszLocation, uint32_t fOpen,
                            PFNVDCOMPLETED pfnCompleted, void **ppInt)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszLocation, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnCompleted, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppInt, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)RTMemAllocZ(sizeof(SHA1STORAGEINTERNAL));
    if (!pInt)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    do
    {
        pInt->pfnCompleted = pfnCompleted;
        /* For caching reasons and to be able to calculate the sha1 sum of the
           data we need a memory buffer. */
        pInt->cbBuf = _1M;
        pInt->pcBuf = (char*)RTMemAlloc(pInt->cbBuf);
        if (!pInt->pcBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        /* The zero buffer is used for appending empty parts at the end of the
         * file (or our buffer) in setSize or when uOffset in writeSync is
         * increased in steps bigger than a byte. */
        pInt->cbZeroBuf = _1K;
        pInt->pvZeroBuf = RTMemAllocZ(pInt->cbZeroBuf);
        if (!pInt->pvZeroBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        if (pSha1Storage->fCreateDigest)
        {
            /* Create a sha1 context the sha1 worker thread will work with. */
            RTSha1Init(&pInt->ctx);
            /* Create an event semaphore to indicate a state change for the sha1
               worker thread. */
            rc = RTSemEventCreate(&pInt->newStatusEvent);
            if (RT_FAILURE(rc))
                break;
            /* Create an event semaphore to indicate a finished calculation of the
               sha1 worker thread. */
            rc = RTSemEventCreate(&pInt->calcFinishedEvent);
            if (RT_FAILURE(rc))
                break;
            /* Create the sha1 worker thread. */
            rc = RTThreadCreate(&pInt->pMfThread, sha1CalcWorkerThread, pInt, 0, RTTHREADTYPE_MAIN_HEAVY_WORKER, RTTHREADFLAGS_WAITABLE, "SHA1-Worker");
            if (RT_FAILURE(rc))
                break;
        }
        /* Open the file. */
        rc = pCallbacks->pfnOpen(pIO->pvUser, pszLocation,
                                 fOpen, pInt->pfnCompleted,
                                 &pInt->pvStorage);
    }
    while(0);

    if (RT_FAILURE(rc))
    {
        if (pSha1Storage->fCreateDigest)
        {
            if (pInt->pMfThread)
            {
                sha1SignalManifestThread(pInt, STATUS_END);
                RTThreadWait(pInt->pMfThread, RT_INDEFINITE_WAIT, 0);
            }
            if (pInt->calcFinishedEvent)
                RTSemEventDestroy(pInt->calcFinishedEvent);
            if (pInt->newStatusEvent)
                RTSemEventDestroy(pInt->newStatusEvent);
        }
        if (pInt->pvZeroBuf)
            RTMemFree(pInt->pvZeroBuf);
        if (pInt->pcBuf)
            RTMemFree(pInt->pcBuf);
        RTMemFree(pInt);
    }
    else
        *ppInt = pInt;

    return rc;
}

static int sha1CloseCallback(void *pvUser, void *pvStorage)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    int rc = VINF_SUCCESS;

    /* Make sure all pending writes are flushed */
    if (pInt->cbCurBuf > 0)
        rc = sha1FlushCurBuf(pIO, pCallbacks, pInt, pSha1Storage->fCreateDigest);

    if (pSha1Storage->fCreateDigest)
    {
        /* Signal the worker thread to end himself */
        rc = sha1SignalManifestThread(pInt, STATUS_END);
        /* Finally calculate & format the SHA1 sum */
        unsigned char auchDig[RTSHA1_HASH_SIZE];
        char *pszDigest;
        RTSha1Final(&pInt->ctx, auchDig);
        rc = RTStrAllocEx(&pszDigest, RTSHA1_DIGEST_LEN + 1);
        if (RT_SUCCESS(rc))
        {
            rc = RTSha1ToString(auchDig, pszDigest, RTSHA1_DIGEST_LEN + 1);
            if (RT_SUCCESS(rc))
                pSha1Storage->strDigest = pszDigest;
            RTStrFree(pszDigest);
        }
        /* Worker thread stopped? */
        rc = RTThreadWait(pInt->pMfThread, RT_INDEFINITE_WAIT, 0);
    }
    /* Close the file */
    rc = pCallbacks->pfnClose(pIO->pvUser, pInt->pvStorage);

    /* Cleanup */
    if (pSha1Storage->fCreateDigest)
    {
        if (pInt->calcFinishedEvent)
            RTSemEventDestroy(pInt->calcFinishedEvent);
        if (pInt->newStatusEvent)
            RTSemEventDestroy(pInt->newStatusEvent);
    }
    if (pInt->pvZeroBuf)
        RTMemFree(pInt->pvZeroBuf);
    if (pInt->pcBuf)
        RTMemFree(pInt->pcBuf);
    RTMemFree(pInt);

    return rc;
}

static int sha1DeleteCallback(void *pvUser, const char *pcszFilename)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    return pCallbacks->pfnDelete(pIO->pvUser, pcszFilename);
}

static int sha1MoveCallback(void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    return pCallbacks->pfnMove(pIO->pvUser, pcszSrc, pcszDst, fMove);
}

static int sha1GetFreeSpaceCallback(void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    return pCallbacks->pfnGetFreeSpace(pIO->pvUser, pcszFilename, pcbFreeSpace);
}

static int sha1GetModificationTimeCallback(void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    return pCallbacks->pfnGetModificationTime(pIO->pvUser, pcszFilename, pModificationTime);
}


static int sha1GetSizeCallback(void *pvUser, void *pvStorage, uint64_t *pcbSize)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvStorage;

//    DEBUG_PRINT_FLOW();

    uint64_t cbSize;
    int rc = pCallbacks->pfnGetSize(pIO->pvUser, pInt->pvStorage, &cbSize);
    if (RT_FAILURE(rc))
        return rc;

    *pcbSize = RT_MAX(pInt->cbCurAll, cbSize);
    return VINF_SUCCESS;
}

static int sha1SetSizeCallback(void *pvUser, void *pvStorage, uint64_t cbSize)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvStorage;

    DEBUG_PRINT_FLOW();

    return pCallbacks->pfnSetSize(pIO->pvUser, pInt->pvStorage, cbSize);
}

static int sha1WriteSyncCallback(void *pvUser, void *pvStorage, uint64_t uOffset,
                                 const void *pvBuf, size_t cbWrite, size_t *pcbWritten)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvStorage;

//    DEBUG_PRINT_FLOW();

    /* Check that the write is linear */
    AssertMsgReturn(pInt->cbCurAll <= uOffset, ("Backward seeking is not allowed (uOffset: %7lu cbCurAll: %7lu)!", uOffset, pInt->cbCurAll), VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    /* Check if we have to add some free space at the end, before we start the
     * real write. */
    if (pInt->cbCurAll < uOffset)
    {
        size_t cbSize = uOffset - pInt->cbCurAll;
        size_t cbAllWritten = 0;
        for(;;)
        {
            /* Finished? */
            if (cbAllWritten == cbSize)
                break;
            size_t cbToWrite = RT_MIN(pInt->cbZeroBuf, cbSize - cbAllWritten);
            size_t cbWritten = 0;
            rc = sha1WriteSyncCallback(pvUser, pvStorage, pInt->cbCurAll,
                                       pInt->pvZeroBuf, cbToWrite, &cbWritten);
            if (RT_FAILURE(rc))
                break;
            cbAllWritten += cbWritten;
        }
        if (RT_FAILURE(rc))
            return rc;
    }
//    RTPrintf("Write uOffset: %7lu cbWrite: %7lu = %7lu\n", uOffset, cbWrite, uOffset + cbWrite);

    size_t cbAllWritten = 0;
    for(;;)
    {
        /* Finished? */
        if (cbAllWritten == cbWrite)
            break;
        size_t cbToWrite = RT_MIN(pInt->cbBuf - pInt->cbCurBuf, cbWrite - cbAllWritten);
        memcpy(&pInt->pcBuf[pInt->cbCurBuf], &((char*)pvBuf)[cbAllWritten], cbToWrite);
        pInt->cbCurBuf += cbToWrite;
        pInt->cbCurAll += cbToWrite;
        cbAllWritten += cbToWrite;
        /* Need to start a real write? */
        if (pInt->cbCurBuf == pInt->cbBuf)
        {
            rc = sha1FlushCurBuf(pIO, pCallbacks, pInt, pSha1Storage->fCreateDigest);
            if (RT_FAILURE(rc))
                break;
        }
    }
    if (pcbWritten)
        *pcbWritten = cbAllWritten;

    return rc;
}

static int sha1ReadSyncCallback(void *pvUser, void *pvStorage, uint64_t uOffset,
                                void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvStorage;

    return pCallbacks->pfnReadSync(pIO->pvUser, pInt->pvStorage, uOffset, pvBuf, cbRead, pcbRead);
}

static int sha1FlushSyncCallback(void *pvUser, void *pvStorage)
{
    /* Validate input. */
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);
    AssertPtrReturn(pvStorage, VERR_INVALID_POINTER);

    PSHA1STORAGE pSha1Storage = (PSHA1STORAGE)pvUser;
    PVDINTERFACE pIO = VDInterfaceGet(pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    DEBUG_PRINT_FLOW();

    PSHA1STORAGEINTERNAL pInt = (PSHA1STORAGEINTERNAL)pvStorage;

    int rc = VINF_SUCCESS;

    /* Check if there is still something in the buffer. If yes, flush it. */
    if (pInt->cbCurBuf > 0)
    {
        rc = sha1FlushCurBuf(pIO, pCallbacks, pInt, pSha1Storage->fCreateDigest);
        if (RT_FAILURE(rc))
            return rc;
    }

    return pCallbacks->pfnFlushSync(pIO->pvUser, pInt->pvStorage);
}

/******************************************************************************
 *   Public Functions                                                         *
 ******************************************************************************/

PVDINTERFACEIO Sha1CreateInterface()
{
    PVDINTERFACEIO pCallbacks = (PVDINTERFACEIO)RTMemAllocZ(sizeof(VDINTERFACEIO));
    if (!pCallbacks)
        return NULL;

    pCallbacks->cbSize                 = sizeof(VDINTERFACEIO);
    pCallbacks->enmInterface           = VDINTERFACETYPE_IO;
    pCallbacks->pfnOpen                = sha1OpenCallback;
    pCallbacks->pfnClose               = sha1CloseCallback;
    pCallbacks->pfnDelete              = sha1DeleteCallback;
    pCallbacks->pfnMove                = sha1MoveCallback;
    pCallbacks->pfnGetFreeSpace        = sha1GetFreeSpaceCallback;
    pCallbacks->pfnGetModificationTime = sha1GetModificationTimeCallback;
    pCallbacks->pfnGetSize             = sha1GetSizeCallback;
    pCallbacks->pfnSetSize             = sha1SetSizeCallback;
    pCallbacks->pfnReadSync            = sha1ReadSyncCallback;
    pCallbacks->pfnWriteSync           = sha1WriteSyncCallback;
    pCallbacks->pfnFlushSync           = sha1FlushSyncCallback;

    return pCallbacks;
}

PVDINTERFACEIO RTFileCreateInterface()
{
    PVDINTERFACEIO pCallbacks = (PVDINTERFACEIO)RTMemAllocZ(sizeof(VDINTERFACEIO));
    if (!pCallbacks)
        return NULL;

    pCallbacks->cbSize                 = sizeof(VDINTERFACEIO);
    pCallbacks->enmInterface           = VDINTERFACETYPE_IO;
    pCallbacks->pfnOpen                = rtFileOpenCallback;
    pCallbacks->pfnClose               = rtFileCloseCallback;
    pCallbacks->pfnDelete              = rtFileDeleteCallback;
    pCallbacks->pfnMove                = rtFileMoveCallback;
    pCallbacks->pfnGetFreeSpace        = rtFileGetFreeSpaceCallback;
    pCallbacks->pfnGetModificationTime = rtFileGetModificationTimeCallback;
    pCallbacks->pfnGetSize             = rtFileGetSizeCallback;
    pCallbacks->pfnSetSize             = rtFileSetSizeCallback;
    pCallbacks->pfnReadSync            = rtFileReadSyncCallback;
    pCallbacks->pfnWriteSync           = rtFileWriteSyncCallback;
    pCallbacks->pfnFlushSync           = rtFileFlushSyncCallback;

    return pCallbacks;
}

PVDINTERFACEIO RTTarCreateInterface()
{
    PVDINTERFACEIO pCallbacks = (PVDINTERFACEIO)RTMemAllocZ(sizeof(VDINTERFACEIO));
    if (!pCallbacks)
        return NULL;

    pCallbacks->cbSize                 = sizeof(VDINTERFACEIO);
    pCallbacks->enmInterface           = VDINTERFACETYPE_IO;
    pCallbacks->pfnOpen                = rtTarOpenCallback;
    pCallbacks->pfnClose               = rtTarCloseCallback;
    pCallbacks->pfnDelete              = rtTarDeleteCallback;
    pCallbacks->pfnMove                = rtTarMoveCallback;
    pCallbacks->pfnGetFreeSpace        = rtTarGetFreeSpaceCallback;
    pCallbacks->pfnGetModificationTime = rtTarGetModificationTimeCallback;
    pCallbacks->pfnGetSize             = rtTarGetSizeCallback;
    pCallbacks->pfnSetSize             = rtTarSetSizeCallback;
    pCallbacks->pfnReadSync            = rtTarReadSyncCallback;
    pCallbacks->pfnWriteSync           = rtTarWriteSyncCallback;
    pCallbacks->pfnFlushSync           = rtTarFlushSyncCallback;

    return pCallbacks;
}

int Sha1WriteBuf(const char *pcszFilename, void *pvBuf, size_t cbSize, PVDINTERFACEIO pCallbacks, void *pvUser)
{
    /* Validate input. */
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    void *pvStorage;
    int rc = pCallbacks->pfnOpen(pvUser, pcszFilename,
                                 RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_ALL, 0,
                                 &pvStorage);
    if (RT_FAILURE(rc))
        return rc;

    size_t cbAllWritten = 0;
    for(;;)
    {
        if (cbAllWritten >= cbSize)
            break;
        size_t cbToWrite = cbSize - cbAllWritten;
        size_t cbWritten = 0;
        rc = pCallbacks->pfnWriteSync(pvUser, pvStorage, cbAllWritten, &((char*)pvBuf)[cbAllWritten], cbToWrite, &cbWritten);
        if (RT_FAILURE(rc))
            break;
        cbAllWritten += cbWritten;
    }

    pCallbacks->pfnClose(pvUser, pvStorage);

    return rc;
}

