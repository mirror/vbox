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
#include <iprt/stream.h>
#include <iprt/circbuf.h>
#include <VBox/vd.h>

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
    /** Current file open mode. */
    uint32_t fOpenMode;
    /** Our own storage handle. */
    PSHA1STORAGE pSha1Storage;
    /** Circular buffer used for transferring data from/to the worker thread. */
    PRTCIRCBUF pCircBuf;
    /** Current absolute position (regardless of the real read/written data). */
    uint64_t cbCurAll;
    /** Current real position in the file. */
    uint64_t cbCurFile;
    /** Handle of the worker thread. */
    RTTHREAD pWorkerThread;
    /** Status of the worker thread. */
    volatile uint32_t u32Status;
    /** Event for signaling a new status. */
    RTSEMEVENT newStatusEvent;
    /** Event for signaling a finished task of the worker thread. */
    RTSEMEVENT workFinishedEvent;
    /** SHA1 calculation context. */
    RTSHA1CONTEXT ctx;
    /** Write mode only: Memory buffer for writing zeros. */
    void *pvZeroBuf;
    /** Write mode only: Size of the zero memory buffer. */
    size_t cbZeroBuf;
    /** Read mode only: Indicate if we reached end of file. */
    volatile bool fEOF;
//    uint64_t calls;
//    uint64_t waits;
} SHA1STORAGEINTERNAL, *PSHA1STORAGEINTERNAL;

/******************************************************************************
 *   Defined Constants And Macros                                             *
 ******************************************************************************/

#define STATUS_WAIT  UINT32_C(0)
#define STATUS_WRITE UINT32_C(1)
#define STATUS_READ  UINT32_C(2)
#define STATUS_END   UINT32_C(3)

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

    DEBUG_PRINT_FLOW();

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

//    DEBUG_PRINT_FLOW();

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
//    AssertReturn(!(fOpen & RTFILE_O_READWRITE), VERR_INVALID_PARAMETER);

    RTTAR tar = (RTTAR)pvUser;

    DEBUG_PRINT_FLOW();

    PRTTARSTORAGEINTERNAL pInt = (PRTTARSTORAGEINTERNAL)RTMemAllocZ(sizeof(RTTARSTORAGEINTERNAL));
    if (!pInt)
        return VERR_NO_MEMORY;

    pInt->pfnCompleted = pfnCompleted;

    int rc = VINF_SUCCESS;

    if (   fOpen & RTFILE_O_READ
        && !(fOpen & RTFILE_O_WRITE))
    {
        /* Read only is a little bit more complicated than writing, cause we
         * need streaming functionality. First try to open the file on the
         * current file position. If this is the file the caller requested, we
         * are fine. If not seek to the next file in the stream and check
         * again. This is repeated until EOF of the OVA. */
        /*
         *
         *
         *  TODO: recheck this with more VDMKs (or what else) in an test OVA.
         *
         *
         */
        bool fFound = false;
        for(;;)
        {
            char *pszFilename = 0;
            rc = RTTarCurrentFile(tar, &pszFilename);
            if (RT_SUCCESS(rc))
            {
                fFound = !strcmp(pszFilename, RTPathFilename(pszLocation));
                RTStrFree(pszFilename);
                if (fFound)
                    break;
                else
                {
                    rc = RTTarSeekNextFile(tar);
                    if (RT_FAILURE(rc))
                        break;
                }
            }else
                break;
        }
        if (fFound)
            rc = RTTarFileOpenCurrentFile(tar, &pInt->file, 0, fOpen);
    }
    else
        rc = RTTarFileOpen(tar, &pInt->file, RTPathFilename(pszLocation), fOpen);

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

    DEBUG_PRINT_FLOW();

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

    PVDINTERFACE pIO = VDInterfaceGet(pInt->pSha1Storage->pVDImageIfaces, VDINTERFACETYPE_IO);
    AssertPtrReturn(pIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIO pCallbacks = VDGetInterfaceIO(pIO);
    AssertPtrReturn(pCallbacks, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    bool fLoop = true;
    while(fLoop)
    {
        /* What should we do next? */
        uint32_t u32Status = ASMAtomicReadU32(&pInt->u32Status);
//        RTPrintf("status: %d\n", u32Status);
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
            case STATUS_WRITE:
            {
                size_t cbAvail = RTCircBufUsed(pInt->pCircBuf);
                size_t cbMemAllRead = 0;
                bool fStop = false;
                bool fEOF = false;
                /* First loop over all the free memory in the circular
                 * memory buffer (could be turn around at the end). */
                for(;;)
                {
                    if (   cbMemAllRead == cbAvail
                        || fStop == true)
                        break;
                    char *pcBuf;
                    size_t cbMemToRead = cbAvail - cbMemAllRead;
                    size_t cbMemRead = 0;
                    /* Try to acquire all the used space of the circular buffer. */
                    RTCircBufAcquireReadBlock(pInt->pCircBuf, cbMemToRead, (void**)&pcBuf, &cbMemRead);
                    size_t cbAllWritten = 0;
                    /* Second, write as long as used memory is there. The write
                     * method could also split the writes up into to smaller
                     * parts. */
                    for(;;)
                    {
                        if (cbAllWritten == cbMemRead)
                            break;
                        size_t cbToWrite = cbMemRead - cbAllWritten;
                        size_t cbWritten = 0;
                        rc = pCallbacks->pfnWriteSync(pIO->pvUser, pInt->pvStorage, pInt->cbCurFile, &pcBuf[cbAllWritten], cbToWrite, &cbWritten);
//                        RTPrintf ("%lu %lu %lu %Rrc\n", pInt->cbCurFile, cbToRead, cbRead, rc);
                        if (RT_FAILURE(rc))
                        {
                            fStop = true;
                            fLoop = false;
                            break;
                        }
                        if (cbWritten == 0)
                        {
                            fStop = true;
                            fLoop = false;
                            fEOF = true;
//                            RTPrintf("EOF\n");
                            break;
                        }
                        cbAllWritten += cbWritten;
                        pInt->cbCurFile += cbWritten;
                    }
                    /* Update the SHA1 context with the next data block. */
                    if (pInt->pSha1Storage->fCreateDigest)
                        RTSha1Update(&pInt->ctx, pcBuf, cbAllWritten);
                    /* Mark the block as empty. */
                    RTCircBufReleaseReadBlock(pInt->pCircBuf, cbAllWritten);
                    cbMemAllRead += cbAllWritten;
                }
                /* Reset the thread status and signal the main thread that we
                 * are finished. Use CmpXchg, so we not overwrite other states
                 * which could be signaled in the meantime. */
                ASMAtomicCmpXchgU32(&pInt->u32Status, STATUS_WAIT, STATUS_WRITE);
                rc = RTSemEventSignal(pInt->workFinishedEvent);
                break;
            }
            case STATUS_READ:
            {
                size_t cbAvail = RTCircBufFree(pInt->pCircBuf);
                size_t cbMemAllWrite = 0;
                bool fStop = false;
                bool fEOF = false;
                /* First loop over all the available memory in the circular
                 * memory buffer (could be turn around at the end). */
                for(;;)
                {
                    if (   cbMemAllWrite == cbAvail
                        || fStop == true)
                        break;
                    char *pcBuf;
                    size_t cbMemToWrite = cbAvail - cbMemAllWrite;
                    size_t cbMemWrite = 0;
                    /* Try to acquire all the free space of the circular buffer. */
                    RTCircBufAcquireWriteBlock(pInt->pCircBuf, cbMemToWrite, (void**)&pcBuf, &cbMemWrite);
                    /* Second, read as long as we filled all the memory. The
                     * read method could also split the reads up into to
                     * smaller parts. */
                    size_t cbAllRead = 0;
                    for(;;)
                    {
                        if (cbAllRead == cbMemWrite)
                            break;
                        size_t cbToRead = cbMemWrite - cbAllRead;
                        size_t cbRead = 0;
                        rc = pCallbacks->pfnReadSync(pIO->pvUser, pInt->pvStorage, pInt->cbCurFile, &pcBuf[cbAllRead], cbToRead, &cbRead);
//                        RTPrintf ("%lu %lu %lu %Rrc\n", pInt->cbCurFile, cbToRead, cbRead, rc);
                        if (RT_FAILURE(rc))
                        {
                            fStop = true;
                            fLoop = false;
                            break;
                        }
                        if (cbRead == 0)
                        {
                            fStop = true;
                            fLoop = false;
                            fEOF = true;
//                            RTPrintf("EOF\n");
                            break;
                        }
                        cbAllRead += cbRead;
                        pInt->cbCurFile += cbRead;
                    }
                    /* Update the SHA1 context with the next data block. */
                    if (pInt->pSha1Storage->fCreateDigest)
                        RTSha1Update(&pInt->ctx, pcBuf, cbAllRead);
                    /* Mark the block as full. */
                    RTCircBufReleaseWriteBlock(pInt->pCircBuf, cbAllRead);
                    cbMemAllWrite += cbAllRead;
                }
                if (fEOF)
                    ASMAtomicWriteBool(&pInt->fEOF, true);
                /* Reset the thread status and signal the main thread that we
                 * are finished. Use CmpXchg, so we not overwrite other states
                 * which could be signaled in the meantime. */
                ASMAtomicCmpXchgU32(&pInt->u32Status, STATUS_WAIT, STATUS_READ);
                rc = RTSemEventSignal(pInt->workFinishedEvent);
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
        if (!(   ASMAtomicReadU32(&pInt->u32Status) == STATUS_WRITE
              || ASMAtomicReadU32(&pInt->u32Status) == STATUS_READ))
            break;
        rc = RTSemEventWait(pInt->workFinishedEvent, 100);
    }
    if (rc == VERR_TIMEOUT)
        rc = VINF_SUCCESS;
    return rc;
}

DECLINLINE(int) sha1FlushCurBuf(PSHA1STORAGEINTERNAL pInt)
{
    int rc = VINF_SUCCESS;
    if (pInt->fOpenMode & RTFILE_O_WRITE)
    {
        /* Let the write worker thread start immediately. */
        rc = sha1SignalManifestThread(pInt, STATUS_WRITE);
        if (RT_FAILURE(rc))
            return rc;

        /* Wait until the write worker thread has finished. */
        rc = sha1WaitForManifestThreadFinished(pInt);
    }

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
    AssertReturn((fOpen & RTFILE_O_READWRITE) != RTFILE_O_READWRITE, VERR_INVALID_PARAMETER); /* No read/write allowed */

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
        pInt->pSha1Storage = pSha1Storage;
        pInt->fEOF         = false;
        pInt->fOpenMode    = fOpen;

        /* Circular buffer in the read case. */
        rc = RTCircBufCreate(&pInt->pCircBuf, _1M * 2);
        if (RT_FAILURE(rc))
            break;

        if (fOpen & RTFILE_O_WRITE)
        {
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
        }

        /* Create an event semaphore to indicate a state change for the worker
         * thread. */
        rc = RTSemEventCreate(&pInt->newStatusEvent);
        if (RT_FAILURE(rc))
            break;
        /* Create an event semaphore to indicate a finished calculation of the
           worker thread. */
        rc = RTSemEventCreate(&pInt->workFinishedEvent);
        if (RT_FAILURE(rc))
            break;
        /* Create the worker thread. */
        rc = RTThreadCreate(&pInt->pWorkerThread, sha1CalcWorkerThread, pInt, 0, RTTHREADTYPE_MAIN_HEAVY_WORKER, RTTHREADFLAGS_WAITABLE, "SHA1-Worker");
        if (RT_FAILURE(rc))
            break;

        if (pSha1Storage->fCreateDigest)
            /* Create a sha1 context the worker thread will work with. */
            RTSha1Init(&pInt->ctx);

        /* Open the file. */
        rc = pCallbacks->pfnOpen(pIO->pvUser, pszLocation,
                                 fOpen, pInt->pfnCompleted,
                                 &pInt->pvStorage);
        if (RT_FAILURE(rc))
            break;

        if (fOpen & RTFILE_O_READ)
        {
            /* Immediately let the worker thread start the reading. */
            rc = sha1SignalManifestThread(pInt, STATUS_READ);
        }
    }
    while(0);

    if (RT_FAILURE(rc))
    {
        if (pInt->pWorkerThread)
        {
            sha1SignalManifestThread(pInt, STATUS_END);
            RTThreadWait(pInt->pWorkerThread, RT_INDEFINITE_WAIT, 0);
        }
        if (pInt->workFinishedEvent)
            RTSemEventDestroy(pInt->workFinishedEvent);
        if (pInt->newStatusEvent)
            RTSemEventDestroy(pInt->newStatusEvent);
        if (pInt->pCircBuf)
            RTCircBufDestroy(pInt->pCircBuf);
        if (pInt->pvZeroBuf)
            RTMemFree(pInt->pvZeroBuf);
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
    rc = sha1FlushCurBuf(pInt);

    if (pInt->pWorkerThread)
    {
        /* Signal the worker thread to end himself */
        rc = sha1SignalManifestThread(pInt, STATUS_END);
        /* Worker thread stopped? */
        rc = RTThreadWait(pInt->pWorkerThread, RT_INDEFINITE_WAIT, 0);
    }

    if (   RT_SUCCESS(rc)
        && pSha1Storage->fCreateDigest)
    {
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
    }

    /* Close the file */
    rc = pCallbacks->pfnClose(pIO->pvUser, pInt->pvStorage);

//    RTPrintf("%lu %lu\n", pInt->calls, pInt->waits);

    /* Cleanup */
    if (pInt->workFinishedEvent)
        RTSemEventDestroy(pInt->workFinishedEvent);
    if (pInt->newStatusEvent)
        RTSemEventDestroy(pInt->newStatusEvent);
    if (pInt->pCircBuf)
        RTCircBufDestroy(pInt->pCircBuf);
    if (pInt->pvZeroBuf)
        RTMemFree(pInt->pvZeroBuf);
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

    DEBUG_PRINT_FLOW();

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

    DEBUG_PRINT_FLOW();

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
        size_t cbAvail = RTCircBufFree(pInt->pCircBuf);
        if (   cbAvail == 0
            && pInt->fEOF)
            return VERR_EOF;
        /* If there isn't enough free space make sure the worker thread is
         * writing some data. */
        if ((cbWrite - cbAllWritten) > cbAvail)
        {
            rc = sha1SignalManifestThread(pInt, STATUS_WRITE);
            if(RT_FAILURE(rc))
                break;
            /* If there is _no_ free space available, we have to wait until it is. */
            if (cbAvail == 0)
            {
                rc = sha1WaitForManifestThreadFinished(pInt);
                if (RT_FAILURE(rc))
                    break;
                cbAvail = RTCircBufFree(pInt->pCircBuf);
//                RTPrintf("############## wait %lu %lu %lu \n", cbRead, cbAllRead, cbAvail);
//                pInt->waits++;
            }
        }
        size_t cbToWrite = RT_MIN(cbWrite - cbAllWritten, cbAvail);
        char *pcBuf;
        size_t cbMemWritten = 0;
        /* Acquire a block for writing from our circular buffer. */
        RTCircBufAcquireWriteBlock(pInt->pCircBuf, cbToWrite, (void**)&pcBuf, &cbMemWritten);
        memcpy(pcBuf, &((char*)pvBuf)[cbAllWritten], cbMemWritten);
        /* Mark the block full. */
        RTCircBufReleaseWriteBlock(pInt->pCircBuf, cbMemWritten);
        cbAllWritten += cbMemWritten;
        pInt->cbCurAll += cbMemWritten;
    }

    if (pcbWritten)
        *pcbWritten = cbAllWritten;

    /* Signal the thread to write more data in the mean time. */
    if (   RT_SUCCESS(rc)
           && RTCircBufUsed(pInt->pCircBuf) >= (RTCircBufSize(pInt->pCircBuf) / 2))
        rc = sha1SignalManifestThread(pInt, STATUS_WRITE);

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

    int rc = VINF_SUCCESS;

//    pInt->calls++;
//    RTPrintf("Read uOffset: %7lu cbRead: %7lu = %7lu\n", uOffset, cbRead, uOffset + cbRead);

    /* Check if we jump forward in the file. If so we have to read the
     * remaining stuff in the gap anyway (SHA1; streaming). */
    if (pInt->cbCurAll < uOffset)
    {
        rc = sha1ReadSyncCallback(pvUser, pvStorage, pInt->cbCurAll, 0, uOffset - pInt->cbCurAll, 0);
        if (RT_FAILURE(rc))
            return rc;
    }

    size_t cbAllRead = 0;
    for(;;)
    {
        /* Finished? */
        if (cbAllRead == cbRead)
            break;
        size_t cbAvail = RTCircBufUsed(pInt->pCircBuf);
        if (   cbAvail == 0
            && pInt->fEOF)
            return VERR_EOF;
        /* If there isn't enough data make sure the worker thread is fetching
         * more. */
        if ((cbRead - cbAllRead) > cbAvail)
        {
            rc = sha1SignalManifestThread(pInt, STATUS_READ);
            if(RT_FAILURE(rc))
                break;
            /* If there is _no_ data available, we have to wait until it is. */
            if (cbAvail == 0)
            {
                rc = sha1WaitForManifestThreadFinished(pInt);
                if (RT_FAILURE(rc))
                    break;
                cbAvail = RTCircBufUsed(pInt->pCircBuf);
//                RTPrintf("############## wait %lu %lu %lu \n", cbRead, cbAllRead, cbAvail);
//                pInt->waits++;
            }
        }
        size_t cbToRead = RT_MIN(cbRead - cbAllRead, cbAvail);
        char *pcBuf;
        size_t cbMemRead = 0;
        /* Acquire a block for reading from our circular buffer. */
        RTCircBufAcquireReadBlock(pInt->pCircBuf, cbToRead, (void**)&pcBuf, &cbMemRead);
        if (pvBuf) /* Make it possible to blind read data (for skipping) */
            memcpy(&((char*)pvBuf)[cbAllRead], pcBuf, cbMemRead);
        /* Mark the block as empty again. */
        RTCircBufReleaseReadBlock(pInt->pCircBuf, cbMemRead);
        cbAllRead += cbMemRead;
        pInt->cbCurAll += cbMemRead;
    }

    if (pcbRead)
        *pcbRead = cbAllRead;

    if (rc == VERR_EOF)
        rc = VINF_SUCCESS;

    /* Signal the thread to read more data in the mean time. */
    if (   RT_SUCCESS(rc)
        && RTCircBufFree(pInt->pCircBuf) >= (RTCircBufSize(pInt->pCircBuf) / 2))
        rc = sha1SignalManifestThread(pInt, STATUS_READ);

    return rc;
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

    /* Check if there is still something in the buffer. If yes, flush it. */
    int rc = sha1FlushCurBuf(pInt);
    if (RT_FAILURE(rc))
        return rc;

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

int Sha1ReadBuf(const char *pcszFilename, void **ppvBuf, size_t *pcbSize, PVDINTERFACEIO pCallbacks, void *pvUser)
{
    /* Validate input. */
    AssertPtrReturn(ppvBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbSize, VERR_INVALID_POINTER);
    AssertPtrReturn(pCallbacks, VERR_INVALID_POINTER);

    void *pvStorage;
    int rc = pCallbacks->pfnOpen(pvUser, pcszFilename,
                                 RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE, 0,
                                 &pvStorage);
    if (RT_FAILURE(rc))
        return rc;

    void *pvBuf = 0;
    uint64_t cbSize = 0;
    do
    {
        rc = pCallbacks->pfnGetSize(pvUser, pvStorage, &cbSize);
        if (RT_FAILURE(rc))
            break;

        pvBuf = RTMemAlloc(cbSize);
        if (!pvBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        size_t cbAllRead = 0;
        for(;;)
        {
            if (cbAllRead == cbSize)
                break;
            size_t cbToRead = cbSize - cbAllRead;
            size_t cbRead = 0;
            rc = pCallbacks->pfnReadSync(pvUser, pvStorage, cbAllRead, &((char*)pvBuf)[cbAllRead], cbToRead, &cbRead);
            if (RT_FAILURE(rc))
                break;
            cbAllRead += cbRead;
        }
    }while(0);

    pCallbacks->pfnClose(pvUser, pvStorage);

    if (RT_SUCCESS(rc))
    {
        *ppvBuf = pvBuf;
        *pcbSize = cbSize;
    }else
    {
        if (pvBuf)
            RTMemFree(pvBuf);
    }

    return rc;
}

int Sha1WriteBuf(const char *pcszFilename, void *pvBuf, size_t cbSize, PVDINTERFACEIO pCallbacks, void *pvUser)
{
    /* Validate input. */
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbSize, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pCallbacks, VERR_INVALID_POINTER);

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

