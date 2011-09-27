/* $Id$ */
/** @file
 * VBox storage devices: Disk integrity check.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_DISK_INTEGRITY
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vddbg.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/avl.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/sg.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/asm.h>

#include "VBoxDD.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Transfer direction.
 */
typedef enum DRVDISKAIOTXDIR
{
    /** Read */
    DRVDISKAIOTXDIR_READ = 0,
    /** Write */
    DRVDISKAIOTXDIR_WRITE,
    /** Flush */
    DRVDISKAIOTXDIR_FLUSH,
    /** Discard */
    DRVDISKAIOTXDIR_DISCARD
} DRVDISKAIOTXDIR;

/**
 * async I/O request.
 */
typedef struct DRVDISKAIOREQ
{
    /** Transfer direction. */
    DRVDISKAIOTXDIR enmTxDir;
    /** Start offset. */
    uint64_t        off;
    /** Transfer size. */
    size_t          cbTransfer;
    /** Segment array. */
    PCRTSGSEG       paSeg;
    /** Number of array entries. */
    unsigned        cSeg;
    /** User argument */
    void           *pvUser;
    /** Slot in the array. */
    unsigned        iSlot;
    /** Start timestamp */
    uint64_t        tsStart;
    /** Completion timestamp. */
    uint64_t        tsComplete;
    /** I/O log entry if configured. */
    VDIOLOGENT      hIoLogEntry;
    /** Ranges to discard. */
    PCRTRANGE       paRanges;
    /** Number of ranges. */
    unsigned        cRanges;
} DRVDISKAIOREQ, *PDRVDISKAIOREQ;

/**
 * I/O log entry.
 */
typedef struct IOLOGENT
{
    /** Start offset */
    uint64_t         off;
    /** Write size */
    size_t           cbWrite;
    /** Number of references to this entry. */
    unsigned         cRefs;
} IOLOGENT, *PIOLOGENT;

/**
 * Disk segment.
 */
typedef struct DRVDISKSEGMENT
{
    /** AVL core. */
    AVLRFOFFNODECORE Core;
    /** Size of the segment */
    size_t           cbSeg;
    /** Data for this segment */
    uint8_t         *pbSeg;
    /** Number of entries in the I/O array. */
    unsigned         cIoLogEntries;
    /** Array of I/O log references. */
    PIOLOGENT        apIoLog[1];
} DRVDISKSEGMENT, *PDRVDISKSEGMENT;

/**
 * Active requests list entry.
 */
typedef struct DRVDISKAIOREQACTIVE
{
    /** Pointer to the request. */
    volatile PDRVDISKAIOREQ pIoReq;
    /** Start timestamp. */
    uint64_t  tsStart;
} DRVDISKAIOREQACTIVE, *PDRVDISKAIOREQACTIVE;

/**
 * Disk integrity driver instance data.
 *
 * @implements  PDMIMEDIA
 */
typedef struct DRVDISKINTEGRITY
{
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to the media driver below us.
     * This is NULL if the media is not mounted. */
    PPDMIMEDIA              pDrvMedia;
    /** Our media interface */
    PDMIMEDIA               IMedia;

    /** The media port interface above. */
    PPDMIMEDIAPORT          pDrvMediaPort;
    /** Media port interface */
    PDMIMEDIAPORT           IMediaPort;

    /** Pointer to the media async driver below us.
     * This is NULL if the media is not mounted. */
    PPDMIMEDIAASYNC         pDrvMediaAsync;
    /** Our media async interface */
    PDMIMEDIAASYNC          IMediaAsync;

    /** The async media port interface above. */
    PPDMIMEDIAASYNCPORT     pDrvMediaAsyncPort;
    /** Our media async port interface */
    PDMIMEDIAASYNCPORT      IMediaAsyncPort;

    /** Flag whether consistency checks are enabled. */
    bool                    fCheckConsistency;
    /** AVL tree containing the disk blocks to check. */
    PAVLRFOFFTREE           pTreeSegments;

    /** Flag whether async request tracing is enabled. */
    bool                    fTraceRequests;
    /** Interval the thread should check for expired requests (milliseconds). */
    uint32_t                uCheckIntervalMs;
    /** Expire timeout for a request (milliseconds). */
    uint32_t                uExpireIntervalMs;
    /** Thread which checks for lost requests. */
    RTTHREAD                hThread;
    /** Event semaphore */
    RTSEMEVENT              SemEvent;
    /** Flag whether the thread should run. */
    bool                    fRunning;
    /** Array containing active requests. */
    DRVDISKAIOREQACTIVE     apReqActive[128];
    /** Next free slot in the array */
    volatile unsigned       iNextFreeSlot;

    /** Flag whether we check for requests completing twice. */
    bool                    fCheckDoubleCompletion;
    /** Number of requests we go back. */
    unsigned                cEntries;
    /** Array of completed but still observed requests. */
    PDRVDISKAIOREQ          *papIoReq;
    /** Current entry in the array. */
    unsigned                iEntry;

    /** I/O logger to use if enabled. */
    VDIOLOGGER              hIoLogger;
} DRVDISKINTEGRITY, *PDRVDISKINTEGRITY;


/**
 * Allocate a new I/O request.
 *
 * @returns New I/O request.
 * @param   enmTxDir      Transfer direction.
 * @param   off           Start offset.
 * @param   paSeg         Segment array.
 * @param   cSeg          Number of segments.
 * @param   cbTransfer    Number of bytes to transfer.
 * @param   pvUser        User argument.
 */
static PDRVDISKAIOREQ drvdiskintIoReqAlloc(DRVDISKAIOTXDIR enmTxDir, uint64_t off, PCRTSGSEG paSeg,
                                           unsigned cSeg, size_t cbTransfer, void *pvUser)
{
    PDRVDISKAIOREQ pIoReq = (PDRVDISKAIOREQ)RTMemAlloc(sizeof(DRVDISKAIOREQ));

    if (RT_LIKELY(pIoReq))
    {
        pIoReq->enmTxDir    = enmTxDir;
        pIoReq->off         = off;
        pIoReq->cbTransfer  = cbTransfer;
        pIoReq->paSeg       = paSeg;
        pIoReq->cSeg        = cSeg;
        pIoReq->pvUser      = pvUser;
        pIoReq->iSlot       = 0;
        pIoReq->tsStart     = RTTimeSystemMilliTS();
        pIoReq->tsComplete  = 0;
        pIoReq->hIoLogEntry = NULL;
    }

    return pIoReq;
}

/**
 * Free a async I/O request.
 *
 * @returns nothing.
 * @param   pThis     Disk driver.
 * @param   pIoReq    The I/O request to free.
 */
static void drvdiskintIoReqFree(PDRVDISKINTEGRITY pThis, PDRVDISKAIOREQ pIoReq)
{
    if (pThis->fCheckDoubleCompletion)
    {
        /* Search if the I/O request completed already. */
        for (unsigned i = 0; i < pThis->cEntries; i++)
        {
            if (RT_UNLIKELY(pThis->papIoReq[i] == pIoReq))
            {
                RTMsgError("Request %#p completed already!\n", pIoReq);
                RTMsgError("Start timestamp %llu Completion timestamp %llu (completed after %llu ms)\n",
                           pIoReq->tsStart, pIoReq->tsComplete, pIoReq->tsComplete - pIoReq->tsStart);
                RTAssertDebugBreak();
            }
        }

        pIoReq->tsComplete = RTTimeSystemMilliTS();
        Assert(!pThis->papIoReq[pThis->iEntry]);
        pThis->papIoReq[pThis->iEntry] = pIoReq;

        pThis->iEntry = (pThis->iEntry+1) % pThis->cEntries;
        if (pThis->papIoReq[pThis->iEntry])
        {
            RTMemFree(pThis->papIoReq[pThis->iEntry]);
            pThis->papIoReq[pThis->iEntry] = NULL;
        }
    }
    else
        RTMemFree(pIoReq);
}

static void drvdiskintIoLogEntryRelease(PIOLOGENT pIoLogEnt)
{
    pIoLogEnt->cRefs--;
    if (!pIoLogEnt->cRefs)
        RTMemFree(pIoLogEnt);
}

/**
 * Record a successful write to the virtual disk.
 *
 * @returns VBox status code.
 * @param   pThis    Disk integrity driver instance data.
 * @param   paSeg    Segment array of the write to record.
 * @param   cSeg     Number of segments.
 * @param   off      Start offset.
 * @param   cbWrite  Number of bytes to record.
 */
static int drvdiskintWriteRecord(PDRVDISKINTEGRITY pThis, PCRTSGSEG paSeg, unsigned cSeg,
                                 uint64_t off, size_t cbWrite)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pThis=%#p paSeg=%#p cSeg=%u off=%llx cbWrite=%u\n",
                 pThis, paSeg, cSeg, off, cbWrite));

    /* Update the segments */
    size_t cbLeft   = cbWrite;
    RTFOFF offCurr  = (RTFOFF)off;
    RTSGBUF SgBuf;
    PIOLOGENT pIoLogEnt = (PIOLOGENT)RTMemAllocZ(sizeof(IOLOGENT));
    if (!pIoLogEnt)
        return VERR_NO_MEMORY;

    pIoLogEnt->off     = off;
    pIoLogEnt->cbWrite = cbWrite;
    pIoLogEnt->cRefs   = 0;

    RTSgBufInit(&SgBuf, paSeg, cSeg);

    while (cbLeft)
    {
        PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetRangeGet(pThis->pTreeSegments, offCurr);
        size_t cbRange  = 0;
        bool fSet       = false;
        unsigned offSeg = 0;

        if (!pSeg)
        {
            /* Get next segment */
            pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetGetBestFit(pThis->pTreeSegments, offCurr, true);
            if (   !pSeg
                || offCurr + (RTFOFF)cbLeft <= pSeg->Core.Key)
                cbRange = cbLeft;
            else
                cbRange = pSeg->Core.Key - offCurr;

            Assert(cbRange % 512 == 0);

            /* Create new segment */
            pSeg = (PDRVDISKSEGMENT)RTMemAllocZ(RT_OFFSETOF(DRVDISKSEGMENT, apIoLog[cbRange / 512]));
            if (pSeg)
            {
                pSeg->Core.Key      = offCurr;
                pSeg->Core.KeyLast  = offCurr + (RTFOFF)cbRange - 1;
                pSeg->cbSeg         = cbRange;
                pSeg->pbSeg         = (uint8_t *)RTMemAllocZ(cbRange);
                pSeg->cIoLogEntries = cbRange / 512;
                if (!pSeg->pbSeg)
                    RTMemFree(pSeg);
                else
                {
                    bool fInserted = RTAvlrFileOffsetInsert(pThis->pTreeSegments, &pSeg->Core);
                    AssertMsg(fInserted, ("Bug!\n"));
                    fSet = true;
                }
            }
        }
        else
        {
            fSet    = true;
            offSeg  = offCurr - pSeg->Core.Key;
            cbRange = RT_MIN(cbLeft, (size_t)(pSeg->Core.KeyLast + 1 - offCurr));
        }

        if (fSet)
        {
            AssertPtr(pSeg);
            size_t cbCopied = RTSgBufCopyToBuf(&SgBuf, pSeg->pbSeg + offSeg, cbRange);
            Assert(cbCopied == cbRange);

            /* Update the I/O log pointers */
            Assert(offSeg % 512 == 0);
            Assert(cbRange % 512 == 0);
            while (offSeg < cbRange)
            {
                uint32_t uSector = offSeg / 512;
                PIOLOGENT pIoLogOld = NULL;

                AssertMsg(uSector < pSeg->cIoLogEntries, ("Internal bug!\n"));

                pIoLogOld = pSeg->apIoLog[uSector];
                if (pIoLogOld)
                {
                    pIoLogOld->cRefs--;
                    if (!pIoLogOld->cRefs)
                        RTMemFree(pIoLogOld);
                }

                pSeg->apIoLog[uSector] = pIoLogEnt;
                pIoLogEnt->cRefs++;

                offSeg += 512;
            }
        }
        else
            RTSgBufAdvance(&SgBuf, cbRange);

        offCurr += cbRange;
        cbLeft  -= cbRange;
    }

    return rc;
}

/**
 * Verifies a read request.
 *
 * @returns VBox status code.
 * @param   pThis    Disk integrity driver instance data.
 * @param   paSeg    Segment array of the containing the data buffers to verify.
 * @param   cSeg     Number of segments.
 * @param   off      Start offset.
 * @param   cbWrite  Number of bytes to verify.
 */
static int drvdiskintReadVerify(PDRVDISKINTEGRITY pThis, PCRTSGSEG paSeg, unsigned cSeg,
                                uint64_t off, size_t cbRead)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pThis=%#p paSeg=%#p cSeg=%u off=%llx cbRead=%u\n",
                 pThis, paSeg, cSeg, off, cbRead));

    Assert(off % 512 == 0);
    Assert(cbRead % 512 == 0);

    /* Compare read data */
    size_t cbLeft   = cbRead;
    RTFOFF offCurr  = (RTFOFF)off;
    RTSGBUF SgBuf;

    RTSgBufInit(&SgBuf, paSeg, cSeg);

    while (cbLeft)
    {
        PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetRangeGet(pThis->pTreeSegments, offCurr);
        size_t cbRange  = 0;
        bool fCmp       = false;
        unsigned offSeg = 0;

        if (!pSeg)
        {
            /* Get next segment */
            pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetGetBestFit(pThis->pTreeSegments, offCurr, true);
            if (!pSeg)
            {
                /* No data in the tree for this read. Assume everything is ok. */
                cbRange = cbLeft;
            }
            else if (offCurr + (RTFOFF)cbLeft <= pSeg->Core.Key)
                cbRange = cbLeft;
            else
                cbRange = pSeg->Core.Key - offCurr;
        }
        else
        {
            fCmp    = true;
            offSeg  = offCurr - pSeg->Core.Key;
            cbRange = RT_MIN(cbLeft, (size_t)(pSeg->Core.KeyLast + 1 - offCurr));
        }

        if (fCmp)
        {
            RTSGSEG Seg;
            RTSGBUF SgBufCmp;
            size_t cbOff = 0;

            Seg.cbSeg = cbRange;
            Seg.pvSeg = pSeg->pbSeg + offSeg;

            RTSgBufInit(&SgBufCmp, &Seg, 1);
            if (RTSgBufCmpEx(&SgBuf, &SgBufCmp, cbRange, &cbOff, true))
            {
                /* Corrupted disk, print I/O log entry of the last write which accessed this range. */
                uint32_t cSector = (offSeg + cbOff) / 512;
                AssertMsg(cSector < pSeg->cIoLogEntries, ("Internal bug!\n"));

                RTMsgError("Corrupted disk at offset %llu (%u bytes in the current read buffer)!\n",
                           offCurr + cbOff, cbOff);
                RTMsgError("Last write to this sector started at offset %llu with %u bytes (%u references to this log entry)\n",
                           pSeg->apIoLog[cSector]->off,
                           pSeg->apIoLog[cSector]->cbWrite,
                           pSeg->apIoLog[cSector]->cRefs);
                RTAssertDebugBreak();
            }
        }
        else
            RTSgBufAdvance(&SgBuf, cbRange);

        offCurr += cbRange;
        cbLeft  -= cbRange;
    }

    return rc;
}

/**
 * Discards the given ranges from the disk.
 *
 * @returns VBox status code.
 * @param   pThis    Disk integrity driver instance data.
 * @param   paRanges Array of ranges to discard.
 * @param   cRanges  Number of ranges in the array.
 */
static int drvdiskintDiscardRecords(PDRVDISKINTEGRITY pThis, PPDMRANGE paRanges, unsigned cRanges)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pThis=%#p paRanges=%#p cRanges=%u\n", pThis, paRanges, cRanges));

    for (unsigned i = 0; i < cRanges; i++)
    {
        uint64_t offStart = paRanges[i].offStart;
        size_t cbLeft = paRanges[i].cbRange;

        LogFlowFunc(("Discarding off=%llu cbRange=%zu\n", offStart, cbLeft));

        while (cbLeft)
        {
            size_t cbRange;
            PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetRangeGet(pThis->pTreeSegments, offStart);

            if (!pSeg)
            {
                /* Get next segment */
                pSeg = (PDRVDISKSEGMENT)RTAvlrFileOffsetGetBestFit(pThis->pTreeSegments, offStart, true);
                if (   !pSeg
                    || (RTFOFF)offStart + (RTFOFF)cbLeft <= pSeg->Core.Key)
                    cbRange = cbLeft;
                else
                    cbRange = pSeg->Core.Key - offStart;

                Assert(!(cbRange % 512));
            }
            else
            {
                size_t cbPreLeft, cbPostLeft;

                cbRange    = RT_MIN(cbRange, pSeg->Core.KeyLast - offStart + 1);
                cbPreLeft  = offStart - pSeg->Core.Key;
                cbPostLeft = pSeg->cbSeg - cbRange - cbPreLeft;

                Assert(!(cbRange % 512));
                Assert(!(cbPreLeft % 512));
                Assert(!(cbPostLeft % 512));

                LogFlowFunc(("cbRange=%zu cbPreLeft=%zu cbPostLeft=%zu\n",
                             cbRange, cbPreLeft, cbPostLeft));

                RTAvlrFileOffsetRemove(pThis->pTreeSegments, pSeg->Core.Key);

                if (!cbPreLeft && !cbPostLeft)
                {
                    /* Just free the whole segment. */
                    LogFlowFunc(("Freeing whole segment pSeg=%#p\n", pSeg));
                    RTMemFree(pSeg->pbSeg);
                    for (unsigned idx = 0; idx < pSeg->cIoLogEntries; idx++)
                        drvdiskintIoLogEntryRelease(pSeg->apIoLog[idx]);
                    RTMemFree(pSeg);
                }
                else if (cbPreLeft && !cbPostLeft)
                {
                    /* Realloc to new size and insert. */
                    LogFlowFunc(("Realloc segment pSeg=%#p\n", pSeg));
                    pSeg->pbSeg = (uint8_t *)RTMemRealloc(pSeg->pbSeg, cbPreLeft);
                    for (unsigned idx = cbPreLeft / 512; idx < pSeg->cIoLogEntries; idx++)
                        drvdiskintIoLogEntryRelease(pSeg->apIoLog[idx]);
                    pSeg = (PDRVDISKSEGMENT)RTMemRealloc(pSeg, RT_OFFSETOF(DRVDISKSEGMENT, apIoLog[cbPreLeft / 512]));
                    pSeg->Core.KeyLast = pSeg->Core.Key + cbPreLeft - 1;
                    pSeg->cbSeg = cbPreLeft;
                    pSeg->cIoLogEntries = cbPreLeft / 512;
                    bool fInserted = RTAvlrFileOffsetInsert(pThis->pTreeSegments, &pSeg->Core);
                    Assert(fInserted);
                }
                else if (!cbPreLeft && cbPostLeft)
                {
                    /* Move data to the front and realloc. */
                    LogFlowFunc(("Move data and realloc segment pSeg=%#p\n", pSeg));
                    memmove(pSeg->pbSeg, pSeg->pbSeg + cbRange, cbPostLeft);
                    for (unsigned idx = 0; idx < cbRange / 512; idx++)
                        drvdiskintIoLogEntryRelease(pSeg->apIoLog[idx]);
                    for (unsigned idx = 0; idx < cbPostLeft /512; idx++)
                        pSeg->apIoLog[idx] = pSeg->apIoLog[(cbRange / 512) + idx];
                    pSeg = (PDRVDISKSEGMENT)RTMemRealloc(pSeg, RT_OFFSETOF(DRVDISKSEGMENT, apIoLog[cbPostLeft / 512]));
                    pSeg->pbSeg = (uint8_t *)RTMemRealloc(pSeg->pbSeg, cbPostLeft);
                    pSeg->Core.Key += cbRange;
                    pSeg->cbSeg = cbPostLeft;
                    pSeg->cIoLogEntries = cbPostLeft / 512;
                    bool fInserted = RTAvlrFileOffsetInsert(pThis->pTreeSegments, &pSeg->Core);
                    Assert(fInserted);
                }
                else
                {
                    /* Split the segment into 2 new segments. */
                    LogFlowFunc(("Split segment pSeg=%#p\n", pSeg));
                    PDRVDISKSEGMENT pSegPost = (PDRVDISKSEGMENT)RTMemAllocZ(RT_OFFSETOF(DRVDISKSEGMENT, apIoLog[cbPostLeft / 512]));
                    if (pSegPost)
                    {
                        pSegPost->Core.Key      = pSeg->Core.Key + cbPreLeft + cbRange;
                        pSegPost->Core.KeyLast  = pSeg->Core.KeyLast;
                        pSegPost->cbSeg         = cbPostLeft;
                        pSegPost->pbSeg         = (uint8_t *)RTMemAllocZ(cbPostLeft);
                        pSegPost->cIoLogEntries = cbPostLeft / 512;
                        if (!pSegPost->pbSeg)
                            RTMemFree(pSegPost);
                        else
                        {
                            memcpy(pSegPost->pbSeg, pSeg->pbSeg + cbPreLeft + cbRange, cbPostLeft);
                            for (unsigned idx = 0; idx < cbPostLeft / 512; idx++)
                                pSegPost->apIoLog[idx] = pSeg->apIoLog[((cbPreLeft + cbRange) / 512) + idx];

                            bool fInserted = RTAvlrFileOffsetInsert(pThis->pTreeSegments, &pSegPost->Core);
                            Assert(fInserted);
                        }
                    }

                    /* Shrink the current segment. */
                    pSeg->pbSeg = (uint8_t *)RTMemRealloc(pSeg->pbSeg, cbPreLeft);
                    for (unsigned idx = cbPreLeft / 512; idx < (cbPreLeft + cbRange) / 512; idx++)
                        drvdiskintIoLogEntryRelease(pSeg->apIoLog[idx]);
                    pSeg = (PDRVDISKSEGMENT)RTMemRealloc(pSeg, RT_OFFSETOF(DRVDISKSEGMENT, apIoLog[cbPreLeft / 512]));
                    pSeg->Core.KeyLast = pSeg->Core.Key + cbPreLeft - 1;
                    pSeg->cbSeg = cbPreLeft;
                    pSeg->cIoLogEntries = cbPreLeft / 512;
                    bool fInserted = RTAvlrFileOffsetInsert(pThis->pTreeSegments, &pSeg->Core);
                    Assert(fInserted);
                } /* if (cbPreLeft && cbPostLeft) */
            }

            offStart += cbRange;
            cbLeft   -= cbRange;
        }
    }

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/**
 * Adds a request to the active list.
 *
 * @returns nothing.
 * @param   pThis    The driver instance data.
 * @param   pIoReq   The request to add.
 */
static void drvdiskintIoReqAdd(PDRVDISKINTEGRITY pThis, PDRVDISKAIOREQ pIoReq)
{
    PDRVDISKAIOREQACTIVE pReqActive = &pThis->apReqActive[pThis->iNextFreeSlot];

    Assert(!pReqActive->pIoReq);
    pReqActive->tsStart = pIoReq->tsStart;
    pReqActive->pIoReq  = pIoReq;
    pIoReq->iSlot = pThis->iNextFreeSlot;

    /* Search for the next one. */
    while (pThis->apReqActive[pThis->iNextFreeSlot].pIoReq)
        pThis->iNextFreeSlot = (pThis->iNextFreeSlot+1) % RT_ELEMENTS(pThis->apReqActive);
}

/**
 * Removes a request from the active list.
 *
 * @returns nothing.
 * @param   pThis    The driver instance data.
 * @param   pIoReq   The request to remove.
 */
static void drvdiskintIoReqRemove(PDRVDISKINTEGRITY pThis, PDRVDISKAIOREQ pIoReq)
{
    PDRVDISKAIOREQACTIVE pReqActive = &pThis->apReqActive[pIoReq->iSlot];

    Assert(pReqActive->pIoReq == pIoReq);

    ASMAtomicWriteNullPtr(&pReqActive->pIoReq);
}

/**
 * Thread checking for expired requests.
 *
 * @returns IPRT status code.
 * @param   pThread    Thread handle.
 * @param   pvUser     Opaque user data.
 */
static int drvdiskIntIoReqExpiredCheck(RTTHREAD pThread, void *pvUser)
{
    PDRVDISKINTEGRITY pThis = (PDRVDISKINTEGRITY)pvUser;

    while (pThis->fRunning)
    {
        int rc = RTSemEventWait(pThis->SemEvent, pThis->uCheckIntervalMs);

        if (!pThis->fRunning)
            break;

        Assert(rc == VERR_TIMEOUT);

        /* Get current timestamp for comparison. */
        uint64_t tsCurr = RTTimeSystemMilliTS();

        /* Go through the array and check for expired requests. */
        for (unsigned i = 0; i < RT_ELEMENTS(pThis->apReqActive); i++)
        {
            PDRVDISKAIOREQACTIVE pReqActive = &pThis->apReqActive[i];
            PDRVDISKAIOREQ pIoReq = ASMAtomicReadPtrT(&pReqActive->pIoReq, PDRVDISKAIOREQ);

            if (   pIoReq
                && (tsCurr > pReqActive->tsStart)
                && (tsCurr - pReqActive->tsStart) >= pThis->uExpireIntervalMs)
            {
                RTMsgError("Request %#p expired (active for %llu ms already)\n",
                           pIoReq, tsCurr - pReqActive->tsStart);
                RTAssertDebugBreak();
            }
        }
    }

    return VINF_SUCCESS;
}

/* -=-=-=-=- IMedia -=-=-=-=- */

/** Makes a PDRVDISKINTEGRITY out of a PPDMIMEDIA. */
#define PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface)        ( (PDRVDISKINTEGRITY)((uintptr_t)pInterface - RT_OFFSETOF(DRVDISKINTEGRITY, IMedia)) )
/** Makes a PDRVDISKINTEGRITY out of a PPDMIMEDIAASYNC. */
#define PDMIMEDIAASYNC_2_DRVDISKINTEGRITY(pInterface)   ( (PDRVDISKINTEGRITY)((uintptr_t)pInterface - RT_OFFSETOF(DRVDISKINTEGRITY, IMediaAsync)) )

/*******************************************************************************
*   Media interface methods                                                    *
*******************************************************************************/

/** @copydoc PDMIMEDIA::pfnRead */
static DECLCALLBACK(int) drvdiskintRead(PPDMIMEDIA pInterface,
                                        uint64_t off, void *pvBuf, size_t cbRead)
{
    int rc = VINF_SUCCESS;
    VDIOLOGENT hIoLogEntry;
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);

    if (pThis->hIoLogger)
    {
        rc = VDDbgIoLogStart(pThis->hIoLogger, false, VDDBGIOLOGREQ_READ, off,
                             cbRead, NULL, &hIoLogEntry);
        AssertRC(rc);
    }

    rc = pThis->pDrvMedia->pfnRead(pThis->pDrvMedia, off, pvBuf, cbRead);

    if (pThis->hIoLogger)
    {
        RTSGSEG Seg;
        RTSGBUF SgBuf;

        Seg.pvSeg = pvBuf;
        Seg.cbSeg = cbRead;
        RTSgBufInit(&SgBuf, &Seg, 1);

        int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, hIoLogEntry, rc, &SgBuf);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fCheckConsistency)
    {
        /* Verify the read. */
        RTSGSEG Seg;
        Seg.cbSeg = cbRead;
        Seg.pvSeg = pvBuf;
        rc = drvdiskintReadVerify(pThis, &Seg, 1, off, cbRead);
    }

    return rc;
}

/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvdiskintWrite(PPDMIMEDIA pInterface,
                                         uint64_t off, const void *pvBuf,
                                         size_t cbWrite)
{
    int rc = VINF_SUCCESS;
    VDIOLOGENT hIoLogEntry;
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);

    if (pThis->hIoLogger)
    {
        RTSGSEG Seg;
        RTSGBUF SgBuf;

        Seg.pvSeg = (void *)pvBuf;
        Seg.cbSeg = cbWrite;
        RTSgBufInit(&SgBuf, &Seg, 1);

        rc = VDDbgIoLogStart(pThis->hIoLogger, false, VDDBGIOLOGREQ_WRITE, off,
                             cbWrite, &SgBuf, &hIoLogEntry);
        AssertRC(rc);
    }

    rc = pThis->pDrvMedia->pfnWrite(pThis->pDrvMedia, off, pvBuf, cbWrite);

    if (pThis->hIoLogger)
    {
        int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, hIoLogEntry, rc, NULL);
        AssertRC(rc2);
    }

    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fCheckConsistency)
    {
        /* Record the write. */
        RTSGSEG Seg;
        Seg.cbSeg = cbWrite;
        Seg.pvSeg = (void *)pvBuf;
        rc = drvdiskintWriteRecord(pThis, &Seg, 1, off, cbWrite);
    }

    return rc;
}

static DECLCALLBACK(int) drvdiskintStartRead(PPDMIMEDIAASYNC pInterface, uint64_t uOffset,
                                             PCRTSGSEG paSeg, unsigned cSeg,
                                             size_t cbRead, void *pvUser)
{
     LogFlow(("%s: uOffset=%#llx paSeg=%#p cSeg=%u cbRead=%d pvUser=%#p\n", __FUNCTION__,
             uOffset, paSeg, cSeg, cbRead, pvUser));
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNC_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = drvdiskintIoReqAlloc(DRVDISKAIOTXDIR_READ, uOffset, paSeg, cSeg, cbRead, pvUser);
    AssertPtr(pIoReq);

    if (pThis->fTraceRequests)
        drvdiskintIoReqAdd(pThis, pIoReq);

    if (pThis->hIoLogger)
    {
        int rc2 = VDDbgIoLogStart(pThis->hIoLogger, true, VDDBGIOLOGREQ_READ, uOffset,
                                  cbRead, NULL, &pIoReq->hIoLogEntry);
        AssertRC(rc2);
    }

    int rc = pThis->pDrvMediaAsync->pfnStartRead(pThis->pDrvMediaAsync, uOffset, paSeg, cSeg,
                                                 cbRead, pIoReq);
    if (rc == VINF_VD_ASYNC_IO_FINISHED)
    {
        /* Verify the read now. */
        if (pThis->fCheckConsistency)
        {
            int rc2 = drvdiskintReadVerify(pThis, paSeg, cSeg, uOffset, cbRead);
            AssertRC(rc2);
        }

        if (pThis->hIoLogger)
        {
            RTSGBUF SgBuf;

            RTSgBufInit(&SgBuf, paSeg, cSeg);

            int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, pIoReq->hIoLogEntry, VINF_SUCCESS, &SgBuf);
            AssertRC(rc2);
        }

        if (pThis->fTraceRequests)
            drvdiskintIoReqRemove(pThis, pIoReq);
        RTMemFree(pIoReq);
    }
    else if (RT_FAILURE(rc) && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        RTMemFree(pIoReq);

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static DECLCALLBACK(int) drvdiskintStartWrite(PPDMIMEDIAASYNC pInterface, uint64_t uOffset,
                                              PCRTSGSEG paSeg, unsigned cSeg,
                                              size_t cbWrite, void *pvUser)
{
     LogFlow(("%s: uOffset=%#llx paSeg=%#p cSeg=%u cbWrite=%d pvUser=%#p\n", __FUNCTION__,
             uOffset, paSeg, cSeg, cbWrite, pvUser));
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNC_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = drvdiskintIoReqAlloc(DRVDISKAIOTXDIR_WRITE, uOffset, paSeg, cSeg, cbWrite, pvUser);
    AssertPtr(pIoReq);

    if (pThis->fTraceRequests)
        drvdiskintIoReqAdd(pThis, pIoReq);

    if (pThis->hIoLogger)
    {
        RTSGBUF SgBuf;

        RTSgBufInit(&SgBuf, paSeg, cSeg);

        int rc2 = VDDbgIoLogStart(pThis->hIoLogger, true, VDDBGIOLOGREQ_WRITE, uOffset,
                                  cbWrite, &SgBuf, &pIoReq->hIoLogEntry);
        AssertRC(rc2);
    }

    int rc = pThis->pDrvMediaAsync->pfnStartWrite(pThis->pDrvMediaAsync, uOffset, paSeg, cSeg,
                                                  cbWrite, pIoReq);
    if (rc == VINF_VD_ASYNC_IO_FINISHED)
    {
        /* Record the write. */
        if  (pThis->fCheckConsistency)
        {
            int rc2 = drvdiskintWriteRecord(pThis, paSeg, cSeg, uOffset, cbWrite);
            AssertRC(rc2);
        }

        if (pThis->hIoLogger)
        {
            int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, pIoReq->hIoLogEntry, VINF_SUCCESS, NULL);
            AssertRC(rc2);
        }

        if (pThis->fTraceRequests)
            drvdiskintIoReqRemove(pThis, pIoReq);

        RTMemFree(pIoReq);
    }
    else if (RT_FAILURE(rc) && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        RTMemFree(pIoReq);

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIAASYNC::pfnStartFlush */
static DECLCALLBACK(int) drvdiskintStartFlush(PPDMIMEDIAASYNC pInterface, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNC_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = drvdiskintIoReqAlloc(DRVDISKAIOTXDIR_FLUSH, 0, NULL, 0, 0, pvUser);
    AssertPtr(pIoReq);

    if (pThis->fTraceRequests)
        drvdiskintIoReqAdd(pThis, pIoReq);

    if (pThis->hIoLogger)
    {
        rc = VDDbgIoLogStart(pThis->hIoLogger, true, VDDBGIOLOGREQ_FLUSH, 0,
                             0, NULL, &pIoReq->hIoLogEntry);
        AssertRC(rc);
    }

    rc = pThis->pDrvMediaAsync->pfnStartFlush(pThis->pDrvMediaAsync, pIoReq);

    if (rc == VINF_VD_ASYNC_IO_FINISHED)
    {
        if (pThis->hIoLogger)
        {
            int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, pIoReq->hIoLogEntry, VINF_SUCCESS, NULL);
            AssertRC(rc2);
        }

        RTMemFree(pIoReq);
    }
    else if (RT_FAILURE(rc) && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        RTMemFree(pIoReq);

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIAASYNC::pfnStartDiscard */
static DECLCALLBACK(int) drvdiskintStartDiscard(PPDMIMEDIA pInterface, PPDMRANGE paRanges, unsigned cRanges, void *pvUser)
{
    int rc = VINF_SUCCESS;
    VDIOLOGENT hIoLogEntry;
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = drvdiskintIoReqAlloc(DRVDISKAIOTXDIR_DISCARD, 0, NULL, 0, 0, pvUser);
    AssertPtr(pIoReq);

    pIoReq->paRanges = paRanges;
    pIoReq->cRanges  = cRanges;

    if (pThis->hIoLogger)
    {
        rc = VDDbgIoLogStartDiscard(pThis->hIoLogger, true, (PVDRANGE)paRanges, cRanges, &hIoLogEntry);
        AssertRC(rc);
    }

    rc = pThis->pDrvMedia->pfnStartDiscard(pThis->pDrvMedia, paRanges, cRanges, pIoReq);

    if (rc == VINF_VD_ASYNC_IO_FINISHED)
    {
        if (pThis->hIoLogger)
        {
            int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, pIoReq->hIoLogEntry, VINF_SUCCESS, NULL);
            AssertRC(rc2);
        }

        RTMemFree(pIoReq);
    }
    else if (RT_FAILURE(rc) && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        RTMemFree(pIoReq);

    return rc;
}

/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvdiskintFlush(PPDMIMEDIA pInterface)
{
    int rc = VINF_SUCCESS;
    VDIOLOGENT hIoLogEntry;
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);

    if (pThis->hIoLogger)
    {
        rc = VDDbgIoLogStart(pThis->hIoLogger, false, VDDBGIOLOGREQ_FLUSH, 0,
                             0, NULL, &hIoLogEntry);
        AssertRC(rc);
    }

    rc = pThis->pDrvMedia->pfnFlush(pThis->pDrvMedia);

    if (pThis->hIoLogger)
    {
        int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, hIoLogEntry, rc, NULL);
        AssertRC(rc2);
    }

    return rc;
}

/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvdiskintGetSize(PPDMIMEDIA pInterface)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnGetSize(pThis->pDrvMedia);
}

/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvdiskintIsReadOnly(PPDMIMEDIA pInterface)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnIsReadOnly(pThis->pDrvMedia);
}

/** @copydoc PDMIMEDIA::pfnBiosGetPCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosGetPCHSGeometry(PPDMIMEDIA pInterface,
                                                       PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosGetPCHSGeometry(pThis->pDrvMedia, pPCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnBiosSetPCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosSetPCHSGeometry(PPDMIMEDIA pInterface,
                                                       PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosSetPCHSGeometry(pThis->pDrvMedia, pPCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnBiosGetLCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosGetLCHSGeometry(PPDMIMEDIA pInterface,
                                                       PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosGetLCHSGeometry(pThis->pDrvMedia, pLCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnBiosSetLCHSGeometry */
static DECLCALLBACK(int) drvdiskintBiosSetLCHSGeometry(PPDMIMEDIA pInterface,
                                                  PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnBiosSetLCHSGeometry(pThis->pDrvMedia, pLCHSGeometry);
}

/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) drvdiskintGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnGetUuid(pThis->pDrvMedia, pUuid);
}

/** @copydoc PDMIMEDIA::pfnDiscard */
static DECLCALLBACK(int) drvdiskintDiscard(PPDMIMEDIA pInterface, PPDMRANGE paRanges, unsigned cRanges)
{
    int rc = VINF_SUCCESS;
    VDIOLOGENT hIoLogEntry;
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);

    if (pThis->hIoLogger)
    {
        rc = VDDbgIoLogStartDiscard(pThis->hIoLogger, false, (PVDRANGE)paRanges, cRanges, &hIoLogEntry);
        AssertRC(rc);
    }

    rc = pThis->pDrvMedia->pfnDiscard(pThis->pDrvMedia, paRanges, cRanges);

    if (pThis->hIoLogger)
    {
        int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, hIoLogEntry, rc, NULL);
        AssertRC(rc2);
    }

    if (pThis->fCheckConsistency)
        rc = drvdiskintDiscardRecords(pThis, paRanges, cRanges);

    return rc;
}

/* -=-=-=-=- IMediaAsyncPort -=-=-=-=- */

/** Makes a PDRVBLOCKASYNC out of a PPDMIMEDIAASYNCPORT. */
#define PDMIMEDIAASYNCPORT_2_DRVDISKINTEGRITY(pInterface)    ( (PDRVDISKINTEGRITY((uintptr_t)pInterface - RT_OFFSETOF(DRVDISKINTEGRITY, IMediaAsyncPort))) )

static DECLCALLBACK(int) drvdiskintAsyncTransferCompleteNotify(PPDMIMEDIAASYNCPORT pInterface, void *pvUser, int rcReq)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNCPORT_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = (PDRVDISKAIOREQ)pvUser;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pIoReq=%#p\n", pIoReq));

    /* Remove from the active list. */
    if (pThis->fTraceRequests)
        drvdiskintIoReqRemove(pThis, pIoReq);

    if (RT_SUCCESS(rcReq) && pThis->fCheckConsistency)
    {
        if (pIoReq->enmTxDir == DRVDISKAIOTXDIR_READ)
            rc = drvdiskintReadVerify(pThis, pIoReq->paSeg, pIoReq->cSeg, pIoReq->off, pIoReq->cbTransfer);
        else if (pIoReq->enmTxDir == DRVDISKAIOTXDIR_WRITE)
            rc = drvdiskintWriteRecord(pThis, pIoReq->paSeg, pIoReq->cSeg, pIoReq->off, pIoReq->cbTransfer);
        else if (pIoReq->enmTxDir == DRVDISKAIOTXDIR_DISCARD)
            rc = drvdiskintDiscardRecords(pThis, paRanges, cRanges);
        else
            AssertMsg(pIoReq->enmTxDir == DRVDISKAIOTXDIR_FLUSH, ("Huh?\n"));

        AssertRC(rc);
    }

    if (pThis->hIoLogger)
    {
        RTSGBUF SgBuf;

        if (pIoReq->enmTxDir == DRVDISKAIOTXDIR_READ)
            RTSgBufInit(&SgBuf, pIoReq->paSeg, pIoReq->cSeg);

        int rc2 = VDDbgIoLogComplete(pThis->hIoLogger, pIoReq->hIoLogEntry, rc, &SgBuf);
        AssertRC(rc2);
    }

    void *pvUserComplete = pIoReq->pvUser;

    drvdiskintIoReqFree(pThis, pIoReq);

    rc = pThis->pDrvMediaAsyncPort->pfnTransferCompleteNotify(pThis->pDrvMediaAsyncPort, pvUserComplete, rcReq);

    return rc;
}

/* -=-=-=-=- IMediaPort -=-=-=-=- */

/** Makes a PDRVBLOCK out of a PPDMIMEDIAPORT. */
#define PDMIMEDIAPORT_2_DRVDISKINTEGRITY(pInterface)    ( (PDRVDISKINTEGRITY((uintptr_t)pInterface - RT_OFFSETOF(DRVDISKINTEGRITY, IMediaPort))) )

/**
 * @interface_method_impl{PDMIMEDIAPORT,pfnQueryDeviceLocation}
 */
static DECLCALLBACK(int) drvdiskintQueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIAPORT_2_DRVDISKINTEGRITY(pInterface);

    return pThis->pDrvMediaPort->pfnQueryDeviceLocation(pThis->pDrvMediaPort, ppcszController,
                                                        piInstance, piLUN);
}

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvdiskintQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIA, &pThis->IMedia);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAASYNC, pThis->pDrvMediaAsync ? &pThis->IMediaAsync : NULL);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAASYNCPORT, &pThis->IMediaAsyncPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT, &pThis->IMediaPort);
    return NULL;
}


/* -=-=-=-=- driver interface -=-=-=-=- */

static int drvdiskintTreeDestroy(PAVLRFOFFNODECORE pNode, void *pvUser)
{
    PDRVDISKSEGMENT pSeg = (PDRVDISKSEGMENT)pNode;

    RTMemFree(pSeg->pbSeg);
    RTMemFree(pSeg);
    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDRVDESTRUCT
 */
static DECLCALLBACK(void) drvdiskintDestruct(PPDMDRVINS pDrvIns)
{
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);

    if (pThis->pTreeSegments)
    {
        RTAvlrFileOffsetDestroy(pThis->pTreeSegments, drvdiskintTreeDestroy, NULL);
        RTMemFree(pThis->pTreeSegments);
    }

    if (pThis->fTraceRequests)
    {
        pThis->fRunning = false;
        RTSemEventSignal(pThis->SemEvent);
        RTSemEventDestroy(pThis->SemEvent);
    }

    if (pThis->fCheckDoubleCompletion)
    {
        /* Free all requests */
        while (pThis->papIoReq[pThis->iEntry])
        {
            RTMemFree(pThis->papIoReq[pThis->iEntry]);
            pThis->papIoReq[pThis->iEntry] = NULL;
            pThis->iEntry = (pThis->iEntry+1) % pThis->cEntries;
        }
    }

    if (pThis->hIoLogger)
        VDDbgIoLogDestroy(pThis->hIoLogger);
}

/**
 * Construct a disk integrity driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvdiskintConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);
    LogFlow(("drvdiskintConstruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "CheckConsistency\0"
                                    "TraceRequests\0"
                                    "CheckIntervalMs\0"
                                    "ExpireIntervalMs\0"
                                    "CheckDoubleCompletions\0"
                                    "HistorySize\0"
                                    "IoLog\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBoolDef(pCfg, "CheckConsistency", &pThis->fCheckConsistency, false);
    AssertRC(rc);
    rc = CFGMR3QueryBoolDef(pCfg, "TraceRequests", &pThis->fTraceRequests, false);
    AssertRC(rc);
    rc = CFGMR3QueryU32Def(pCfg, "CheckIntervalMs", &pThis->uCheckIntervalMs, 5000); /* 5 seconds */
    AssertRC(rc);
    rc = CFGMR3QueryU32Def(pCfg, "ExpireIntervalMs", &pThis->uExpireIntervalMs, 20000); /* 20 seconds */
    AssertRC(rc);
    rc = CFGMR3QueryBoolDef(pCfg, "CheckDoubleCompletions", &pThis->fCheckDoubleCompletion, false);
    AssertRC(rc);
    rc = CFGMR3QueryU32Def(pCfg, "HistorySize", &pThis->cEntries, 512);
    AssertRC(rc);

    char *pszIoLogFilename = NULL;
    rc = CFGMR3QueryStringAlloc(pCfg, "IoLog", &pszIoLogFilename);
    Assert(RT_SUCCESS(rc) || rc == VERR_CFGM_VALUE_NOT_FOUND);

    /*
     * Initialize most of the data members.
     */
    pThis->pDrvIns                       = pDrvIns;

    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface     = drvdiskintQueryInterface;

    /* IMedia */
    pThis->IMedia.pfnRead                = drvdiskintRead;
    pThis->IMedia.pfnWrite               = drvdiskintWrite;
    pThis->IMedia.pfnFlush               = drvdiskintFlush;
    pThis->IMedia.pfnGetSize             = drvdiskintGetSize;
    pThis->IMedia.pfnIsReadOnly          = drvdiskintIsReadOnly;
    pThis->IMedia.pfnBiosGetPCHSGeometry = drvdiskintBiosGetPCHSGeometry;
    pThis->IMedia.pfnBiosSetPCHSGeometry = drvdiskintBiosSetPCHSGeometry;
    pThis->IMedia.pfnBiosGetLCHSGeometry = drvdiskintBiosGetLCHSGeometry;
    pThis->IMedia.pfnBiosSetLCHSGeometry = drvdiskintBiosSetLCHSGeometry;
    pThis->IMedia.pfnGetUuid             = drvdiskintGetUuid;

    /* IMediaAsync */
    pThis->IMediaAsync.pfnStartRead      = drvdiskintStartRead;
    pThis->IMediaAsync.pfnStartWrite     = drvdiskintStartWrite;
    pThis->IMediaAsync.pfnStartFlush     = drvdiskintStartFlush;

    /* IMediaAsyncPort. */
    pThis->IMediaAsyncPort.pfnTransferCompleteNotify  = drvdiskintAsyncTransferCompleteNotify;

    /* IMediaPort. */
    pThis->IMediaPort.pfnQueryDeviceLocation = drvdiskintQueryDeviceLocation;

    /* Query the media port interface above us. */
    pThis->pDrvMediaPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIMEDIAPORT);
    if (!pThis->pDrvMediaPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                N_("No media port inrerface above"));

    /* Try to attach async media port interface above.*/
    pThis->pDrvMediaAsyncPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIMEDIAASYNCPORT);

    /*
     * Try attach driver below and query it's media interface.
     */
    PPDMIBASE pBase;
    rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBase);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to attach driver below us! %Rrc"), rc);

    pThis->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIA);
    if (!pThis->pDrvMedia)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                N_("No media or async media interface below"));

    pThis->pDrvMediaAsync = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIAASYNC);

    if (pThis->pDrvMedia->pfnDiscard)
        pThis->IMedia.pfnDiscard = drvdiskintDiscard;
    if (   pThis->pDrvMediaAsync
        && pThis->pDrvMediaAsync->pfnStartDiscard)
        pThis->IMediaAsync.pfnStartDiscard = drvdiskintStartDiscard;

    if (pThis->fCheckConsistency)
    {
        /* Create the AVL tree. */
        pThis->pTreeSegments = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
        if (!pThis->pTreeSegments)
            rc = VERR_NO_MEMORY;
    }

    if (pThis->fTraceRequests)
    {
        for (unsigned i = 0; i < RT_ELEMENTS(pThis->apReqActive); i++)
        {
            pThis->apReqActive[i].pIoReq  = NULL;
            pThis->apReqActive[i].tsStart = 0;
        }

        pThis->iNextFreeSlot = 0;

        /* Init event semaphore. */
        rc = RTSemEventCreate(&pThis->SemEvent);
        AssertRC(rc);
        pThis->fRunning = true;
        rc = RTThreadCreate(&pThis->hThread, drvdiskIntIoReqExpiredCheck, pThis,
                            0, RTTHREADTYPE_INFREQUENT_POLLER, 0, "DiskIntegrity");
        AssertRC(rc);
    }

    if (pThis->fCheckDoubleCompletion)
    {
        pThis->iEntry = 0;
        pThis->papIoReq = (PDRVDISKAIOREQ *)RTMemAllocZ(pThis->cEntries * sizeof(PDRVDISKAIOREQ));
        AssertPtr(pThis->papIoReq);
    }

    if (pszIoLogFilename)
    {
        rc = VDDbgIoLogCreate(&pThis->hIoLogger, pszIoLogFilename, VDDBG_IOLOG_LOG_DATA);
        MMR3HeapFree(pszIoLogFilename);
    }

    return rc;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvDiskIntegrity =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "DiskIntegrity",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Disk integrity driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_BLOCK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVDISKINTEGRITY),
    /* pfnConstruct */
    drvdiskintConstruct,
    /* pfnDestruct */
    drvdiskintDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

