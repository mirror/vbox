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
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/avl.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/sg.h>

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * async I/O request.
 */
typedef struct DRVDISKAIOREQ
{
    /** Flag whether this is a read or write request. */
    bool      fRead;
    /** Start offset. */
    uint64_t  off;
    /** Transfer size. */
    size_t    cbTransfer;
    /** Segment array. */
    PCRTSGSEG paSeg;
    /** Number of array entries. */
    unsigned  cSeg;
    /** User argument */
    void     *pvUser;
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
    /** Numbner of entries in the I/O array. */
    unsigned         cIoLogEntries;
    /** Array of I/O log references. */
    PIOLOGENT        apIoLog[1];
} DRVDISKSEGMENT, *PDRVDISKSEGMENT;

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

    /** Pointer to the media async driver below us.
     * This is NULL if the media is not mounted. */
    PPDMIMEDIAASYNC         pDrvMediaAsync;
    /** Our media async interface */
    PDMIMEDIAASYNC          IMediaAsync;

    /** The async media port interface above. */
    PPDMIMEDIAASYNCPORT     pDrvMediaAsyncPort;
    /** Our media async port interface */
    PDMIMEDIAASYNCPORT      IMediaAsyncPort;

    /** AVL tree containing the disk blocks to check. */
    PAVLRFOFFTREE           pTreeSegments;
} DRVDISKINTEGRITY, *PDRVDISKINTEGRITY;


/**
 * Allocate a new I/O request.
 *
 * @returns New I/O request.
 * @param   fRead         Flag whether this is a read or a write.
 * @param   off           Start offset.
 * @param   paSeg         Segment array.
 * @param   cSeg          Number of segments.
 * @param   cbTransfer    Number of bytes to transfer.
 * @param   pvUser        User argument.
 */
static PDRVDISKAIOREQ drvdiskintIoReqAlloc(bool fRead, uint64_t off, PCRTSGSEG paSeg,
                                           unsigned cSeg, size_t cbTransfer, void *pvUser)
{
    PDRVDISKAIOREQ pIoReq = (PDRVDISKAIOREQ)RTMemAlloc(sizeof(DRVDISKAIOREQ));

    if (RT_LIKELY(pIoReq))
    {
        pIoReq->fRead      = fRead;
        pIoReq->off        = off;
        pIoReq->cbTransfer = cbTransfer;
        pIoReq->paSeg      = paSeg;
        pIoReq->cSeg       = cSeg;
        pIoReq->pvUser     = pvUser;
    }

    return pIoReq;
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
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    int rc = pThis->pDrvMedia->pfnRead(pThis->pDrvMedia, off, pvBuf, cbRead);
    if (RT_FAILURE(rc))
        return rc;

    /* Verify the read. */
    RTSGSEG Seg;
    Seg.cbSeg = cbRead;
    Seg.pvSeg = pvBuf;
    return drvdiskintReadVerify(pThis, &Seg, 1, off, cbRead);
}

/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvdiskintWrite(PPDMIMEDIA pInterface,
                                         uint64_t off, const void *pvBuf,
                                         size_t cbWrite)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    int rc = pThis->pDrvMedia->pfnWrite(pThis->pDrvMedia, off, pvBuf, cbWrite);
    if (RT_FAILURE(rc))
        return rc;

    /* Record the write. */
    RTSGSEG Seg;
    Seg.cbSeg = cbWrite;
    Seg.pvSeg = (void *)pvBuf;
    return drvdiskintWriteRecord(pThis, &Seg, 1, off, cbWrite);
}

static DECLCALLBACK(int) drvdiskintStartRead(PPDMIMEDIAASYNC pInterface, uint64_t uOffset,
                                             PCRTSGSEG paSeg, unsigned cSeg,
                                             size_t cbRead, void *pvUser)
{
     LogFlow(("%s: uOffset=%#llx paSeg=%#p cSeg=%u cbRead=%d\n pvUser=%#p", __FUNCTION__,
             uOffset, paSeg, cSeg, cbRead, pvUser));
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNC_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = drvdiskintIoReqAlloc(true, uOffset, paSeg, cSeg, cbRead, pvUser);
    AssertPtr(pIoReq);

    int rc = pThis->pDrvMediaAsync->pfnStartRead(pThis->pDrvMediaAsync, uOffset, paSeg, cSeg,
                                                 cbRead, pIoReq);
    if (rc == VINF_VD_ASYNC_IO_FINISHED)
    {
        /* Verify the read now. */
        int rc2 = drvdiskintReadVerify(pThis, paSeg, cSeg, uOffset, cbRead);
        AssertRC(rc2);
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
     LogFlow(("%s: uOffset=%#llx paSeg=%#p cSeg=%u cbWrite=%d\n pvUser=%#p", __FUNCTION__,
             uOffset, paSeg, cSeg, cbWrite, pvUser));
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNC_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = drvdiskintIoReqAlloc(false, uOffset, paSeg, cSeg, cbWrite, pvUser);
    AssertPtr(pIoReq);

    int rc = pThis->pDrvMediaAsync->pfnStartWrite(pThis->pDrvMediaAsync, uOffset, paSeg, cSeg,
                                                  cbWrite, pIoReq);
    if (rc == VINF_VD_ASYNC_IO_FINISHED)
    {
        /* Verify the read now. */
        int rc2 = drvdiskintWriteRecord(pThis, paSeg, cSeg, uOffset, cbWrite);
        AssertRC(rc2);
        RTMemFree(pIoReq);
    }
    else if (RT_FAILURE(rc) && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
        RTMemFree(pIoReq);

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvdiskintFlush(PPDMIMEDIA pInterface)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIA_2_DRVDISKINTEGRITY(pInterface);
    return pThis->pDrvMedia->pfnFlush(pThis->pDrvMedia);
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

/* -=-=-=-=- IMediaAsyncPort -=-=-=-=- */

/** Makes a PDRVBLOCKASYNC out of a PPDMIMEDIAASYNCPORT. */
#define PDMIMEDIAASYNCPORT_2_DRVDISKINTEGRITY(pInterface)    ( (PDRVDISKINTEGRITY((uintptr_t)pInterface - RT_OFFSETOF(DRVDISKINTEGRITY, IMediaAsyncPort))) )

static DECLCALLBACK(int) drvdiskintAsyncTransferCompleteNotify(PPDMIMEDIAASYNCPORT pInterface, void *pvUser, int rcReq)
{
    PDRVDISKINTEGRITY pThis = PDMIMEDIAASYNCPORT_2_DRVDISKINTEGRITY(pInterface);
    PDRVDISKAIOREQ pIoReq = (PDRVDISKAIOREQ)pvUser;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pIoReq=%#p\n", pIoReq));

    if (RT_SUCCESS(rcReq))
    {
        if (pIoReq->fRead)
            rc = drvdiskintReadVerify(pThis, pIoReq->paSeg, pIoReq->cSeg, pIoReq->off, pIoReq->cbTransfer);
        else
            rc = drvdiskintWriteRecord(pThis, pIoReq->paSeg, pIoReq->cSeg, pIoReq->off, pIoReq->cbTransfer);

        AssertRC(rc);
    }

    rc = pThis->pDrvMediaAsyncPort->pfnTransferCompleteNotify(pThis->pDrvMediaAsyncPort, pIoReq->pvUser, rcReq);
    RTMemFree(pIoReq);

    return rc;
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
}

/**
 * Construct a disk integrity driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvdiskintConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVDISKINTEGRITY pThis = PDMINS_2_DATA(pDrvIns, PDRVDISKINTEGRITY);
    LogFlow(("drvdiskintConstruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, ""))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

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

    /* IMediaAsyncPort. */
    pThis->IMediaAsyncPort.pfnTransferCompleteNotify  = drvdiskintAsyncTransferCompleteNotify;

    /*
     * Try attach driver below and query it's media interface.
     */
    PPDMIBASE pBase;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBase);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to attach driver below us! %Rrf"), rc);

    pThis->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIA);
    if (!pThis->pDrvMedia)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                N_("No media or async media interface below"));

    pThis->pDrvMediaAsync = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIAASYNC);

    /* Try to attach async media port interface above.*/
    pThis->pDrvMediaAsyncPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIMEDIAASYNCPORT);

    /* Create the AVL tree. */
    pThis->pTreeSegments = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
    if (!pThis->pTreeSegments)
        rc = VERR_NO_MEMORY;

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

