/* $Id$ */
/** @file
 * VBox storage drivers: Generic SCSI command parser and execution driver
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DRV_SCSI
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmifs.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/pdmthread.h>
#include <VBox/vscsi.h>
#include <VBox/scsi.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/req.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

/** The maximum number of release log entries per device. */
#define MAX_LOG_REL_ERRORS  1024

/**
 * SCSI driver instance data.
 *
 * @implements  PDMISCSICONNECTOR
 * @implements  PDMIMEDIAEXPORT
 * @implements  PDMIMOUNTNOTIFY
 */
typedef struct DRVSCSI
{
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;

    /** Pointer to the attached driver's base interface. */
    PPDMIBASE               pDrvBase;
    /** Pointer to the attached driver's block interface. */
    PPDMIMEDIA              pDrvMedia;
    /** Pointer to the attached driver's extended media interface. */
    PPDMIMEDIAEX            pDrvMediaEx;
    /** Pointer to the attached driver's mount interface. */
    PPDMIMOUNT              pDrvMount;
    /** Pointer to the SCSI port interface of the device above. */
    PPDMISCSIPORT           pDevScsiPort;
    /** pointer to the Led port interface of the dveice above. */
    PPDMILEDPORTS           pLedPort;
    /** The scsi connector interface .*/
    PDMISCSICONNECTOR       ISCSIConnector;
    /** The media port interface. */
    PDMIMEDIAPORT           IPort;
    /** The optional extended media port interface. */
    PDMIMEDIAEXPORT         IPortEx;
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY         IMountNotify;
    /** Fallback status LED state for this drive.
     * This is used in case the device doesn't has a LED interface. */
    PDMLED                  Led;
    /** Pointer to the status LED for this drive. */
    PPDMLED                 pLed;

    /** VSCSI device handle. */
    VSCSIDEVICE             hVScsiDevice;
    /** VSCSI LUN handle. */
    VSCSILUN                hVScsiLun;
    /** I/O callbacks. */
    VSCSILUNIOCALLBACKS     VScsiIoCallbacks;

    /** Indicates whether PDMDrvHlpAsyncNotificationCompleted should be called by
     * any of the dummy functions. */
    bool volatile           fDummySignal;
    /** Release statistics: number of bytes written. */
    STAMCOUNTER             StatBytesWritten;
    /** Release statistics: number of bytes read. */
    STAMCOUNTER             StatBytesRead;
    /** Release statistics: Current I/O depth. */
    volatile uint32_t       StatIoDepth;
    /** Errors printed in the release log. */
    unsigned                cErrors;
    /** Mark the drive as having a non-rotational medium (i.e. as a SSD). */
    bool                    fNonRotational;
    /** Medium is readonly */
    bool                    fReadonly;
} DRVSCSI, *PDRVSCSI;

/** Convert a VSCSI I/O request handle to the associated PDMIMEDIAEX I/O request. */
#define DRVSCSI_VSCSIIOREQ_2_PDMMEDIAEXIOREQ(a_hVScsiIoReq) (*(PPDMMEDIAEXIOREQ)((uint8_t *)(a_hVScsiIoReq) - sizeof(PDMMEDIAEXIOREQ)))
/** Convert a PDMIMEDIAEX I/O additional request memory to a VSCSI I/O request. */
#define DRVSCSI_PDMMEDIAEXIOREQ_2_VSCSIIOREQ(a_pvIoReqAlloc) ((VSCSIIOREQ)((uint8_t *)(a_pvIoReqAlloc) + sizeof(PDMMEDIAEXIOREQ)))

/**
 * Returns whether the given status code indicates a non fatal error.
 *
 * @returns True if the error can be fixed by the user after the VM was suspended.
 *          False if not and the error should be reported to the guest.
 * @param   rc          The status code to check.
 */
DECLINLINE(bool) drvscsiIsRedoPossible(int rc)
{
    if (   rc == VERR_DISK_FULL
        || rc == VERR_FILE_TOO_BIG
        || rc == VERR_BROKEN_PIPE
        || rc == VERR_NET_CONNECTION_REFUSED
        || rc == VERR_VD_DEK_MISSING)
        return true;

    return false;
}

/* -=-=-=-=- VScsiIoCallbacks -=-=-=-=- */

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunReqAllocSizeSet}
 */
static DECLCALLBACK(int) drvscsiReqAllocSizeSet(VSCSILUN hVScsiLun, void *pvScsiLunUser, size_t cbVScsiIoReqAlloc)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    /* We need to store the I/O request handle so we can get it when VSCSI queues an I/O request. */
    return pThis->pDrvMediaEx->pfnIoReqAllocSizeSet(pThis->pDrvMediaEx, cbVScsiIoReqAlloc + sizeof(PDMMEDIAEXIOREQ));
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunReqAlloc}
 */
static DECLCALLBACK(int) drvscsiReqAlloc(VSCSILUN hVScsiLun, void *pvScsiLunUser,
                                         uint64_t u64Tag, PVSCSIIOREQ phVScsiIoReq)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;
    PDMMEDIAEXIOREQ hIoReq;
    void *pvIoReqAlloc;
    int rc = pThis->pDrvMediaEx->pfnIoReqAlloc(pThis->pDrvMediaEx, &hIoReq, &pvIoReqAlloc, u64Tag,
                                               PDMIMEDIAEX_F_DEFAULT);
    if (RT_SUCCESS(rc))
    {
        PPDMMEDIAEXIOREQ phIoReq = (PPDMMEDIAEXIOREQ)pvIoReqAlloc;

        *phIoReq = hIoReq;
        *phVScsiIoReq = (VSCSIIOREQ)(phIoReq + 1);
    }

    return rc;
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunReqFree}
 */
static DECLCALLBACK(int) drvscsiReqFree(VSCSILUN hVScsiLun, void *pvScsiLunUser, VSCSIIOREQ hVScsiIoReq)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;
    PDMMEDIAEXIOREQ hIoReq = DRVSCSI_VSCSIIOREQ_2_PDMMEDIAEXIOREQ(hVScsiIoReq);

    return pThis->pDrvMediaEx->pfnIoReqFree(pThis->pDrvMediaEx, hIoReq);
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunMediumGetSize}
 */
static DECLCALLBACK(int) drvscsiGetSize(VSCSILUN hVScsiLun, void *pvScsiLunUser, uint64_t *pcbSize)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    *pcbSize = pThis->pDrvMedia->pfnGetSize(pThis->pDrvMedia);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunMediumGetSectorSize}
 */
static DECLCALLBACK(int) drvscsiGetSectorSize(VSCSILUN hVScsiLun, void *pvScsiLunUser, uint32_t *pcbSectorSize)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    *pcbSectorSize = pThis->pDrvMedia->pfnGetSectorSize(pThis->pDrvMedia);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunMediumSetLock}
 */
static DECLCALLBACK(int) drvscsiSetLock(VSCSILUN hVScsiLun, void *pvScsiLunUser, bool fLocked)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    if (fLocked)
        pThis->pDrvMount->pfnLock(pThis->pDrvMount);
    else
        pThis->pDrvMount->pfnUnlock(pThis->pDrvMount);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunReqTransferEnqueue}
 */
static DECLCALLBACK(int) drvscsiReqTransferEnqueue(VSCSILUN hVScsiLun, void *pvScsiLunUser, VSCSIIOREQ hVScsiIoReq)
{
    RT_NOREF(hVScsiLun);
    int rc = VINF_SUCCESS;
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;
    PDMMEDIAEXIOREQ hIoReq = DRVSCSI_VSCSIIOREQ_2_PDMMEDIAEXIOREQ(hVScsiIoReq);

    LogFlowFunc(("Enqueuing hVScsiIoReq=%#p\n", hVScsiIoReq));

    VSCSIIOREQTXDIR enmTxDir = VSCSIIoReqTxDirGet(hVScsiIoReq);
    switch (enmTxDir)
    {
        case VSCSIIOREQTXDIR_FLUSH:
        {
            rc = pThis->pDrvMediaEx->pfnIoReqFlush(pThis->pDrvMediaEx, hIoReq);
            if (   RT_FAILURE(rc)
                && pThis->cErrors++ < MAX_LOG_REL_ERRORS)
                LogRel(("SCSI#%u: Flush returned rc=%Rrc\n",
                        pThis->pDrvIns->iInstance, rc));
            break;
        }
        case VSCSIIOREQTXDIR_UNMAP:
        {
            PCRTRANGE paRanges;
            unsigned cRanges;

            rc = VSCSIIoReqUnmapParamsGet(hVScsiIoReq, &paRanges, &cRanges);
            AssertRC(rc);

            pThis->pLed->Asserted.s.fWriting = pThis->pLed->Actual.s.fWriting = 1;
            rc = pThis->pDrvMediaEx->pfnIoReqDiscard(pThis->pDrvMediaEx, hIoReq, paRanges, cRanges);
            if (   RT_FAILURE(rc)
                && pThis->cErrors++ < MAX_LOG_REL_ERRORS)
                LogRel(("SCSI#%u: Discard returned rc=%Rrc\n",
                        pThis->pDrvIns->iInstance, rc));
            break;
        }
        case VSCSIIOREQTXDIR_READ:
        case VSCSIIOREQTXDIR_WRITE:
        {
            uint64_t  uOffset    = 0;
            size_t    cbTransfer = 0;
            size_t    cbSeg      = 0;
            PCRTSGSEG paSeg      = NULL;
            unsigned  cSeg       = 0;

            rc = VSCSIIoReqParamsGet(hVScsiIoReq, &uOffset, &cbTransfer,
                                     &cSeg, &cbSeg, &paSeg);
            AssertRC(rc);

            if (enmTxDir == VSCSIIOREQTXDIR_READ)
            {
                pThis->pLed->Asserted.s.fReading = pThis->pLed->Actual.s.fReading = 1;
                rc = pThis->pDrvMediaEx->pfnIoReqRead(pThis->pDrvMediaEx, hIoReq, uOffset, cbTransfer);
                STAM_REL_COUNTER_ADD(&pThis->StatBytesRead, cbTransfer);
            }
            else
            {
                pThis->pLed->Asserted.s.fWriting = pThis->pLed->Actual.s.fWriting = 1;
                rc = pThis->pDrvMediaEx->pfnIoReqWrite(pThis->pDrvMediaEx, hIoReq, uOffset, cbTransfer);
                STAM_REL_COUNTER_ADD(&pThis->StatBytesWritten, cbTransfer);
            }

            if (   RT_FAILURE(rc)
                && pThis->cErrors++ < MAX_LOG_REL_ERRORS)
                LogRel(("SCSI#%u: %s at offset %llu (%u bytes left) returned rc=%Rrc\n",
                        pThis->pDrvIns->iInstance,
                        enmTxDir == VSCSIIOREQTXDIR_READ
                        ? "Read"
                        : "Write",
                        uOffset,
                        cbTransfer, rc));
            break;
        }
        default:
            AssertMsgFailed(("Invalid transfer direction %u\n", enmTxDir));
    }

    if (rc == VINF_SUCCESS)
    {
        if (enmTxDir == VSCSIIOREQTXDIR_READ)
            pThis->pLed->Actual.s.fReading = 0;
        else if (enmTxDir == VSCSIIOREQTXDIR_WRITE)
            pThis->pLed->Actual.s.fWriting = 0;
        else
            AssertMsg(enmTxDir == VSCSIIOREQTXDIR_FLUSH, ("Invalid transfer direction %u\n", enmTxDir));

        VSCSIIoReqCompleted(hVScsiIoReq, VINF_SUCCESS, false);
        rc = VINF_SUCCESS;
    }
    else if (rc == VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS)
        rc = VINF_SUCCESS;
    else if (RT_FAILURE(rc))
    {
        if (enmTxDir == VSCSIIOREQTXDIR_READ)
            pThis->pLed->Actual.s.fReading = 0;
        else if (enmTxDir == VSCSIIOREQTXDIR_WRITE)
            pThis->pLed->Actual.s.fWriting = 0;
        else
            AssertMsg(enmTxDir == VSCSIIOREQTXDIR_FLUSH, ("Invalid transfer direction %u\n", enmTxDir));

        VSCSIIoReqCompleted(hVScsiIoReq, rc, drvscsiIsRedoPossible(rc));
        rc = VINF_SUCCESS;
    }
    else
        AssertMsgFailed(("Invalid return code rc=%Rrc\n", rc));

    return rc;
}

/**
 * @interface_method_impl{VSCSILUNIOCALLBACKS,pfnVScsiLunGetFeatureFlags}
 */
static DECLCALLBACK(int) drvscsiGetFeatureFlags(VSCSILUN hVScsiLun, void *pvScsiLunUser, uint64_t *pfFeatures)
{
    RT_NOREF(hVScsiLun);
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    *pfFeatures = 0;

    uint32_t fFeatures = 0;
    int rc = pThis->pDrvMediaEx->pfnQueryFeatures(pThis->pDrvMediaEx, &fFeatures);
    if (RT_SUCCESS(rc) && (fFeatures & PDMIMEDIAEX_FEATURE_F_DISCARD))
        *pfFeatures |= VSCSI_LUN_FEATURE_UNMAP;

    if (pThis->fNonRotational)
        *pfFeatures |= VSCSI_LUN_FEATURE_NON_ROTATIONAL;

    if (pThis->fReadonly)
        *pfFeatures |= VSCSI_LUN_FEATURE_READONLY;

    return VINF_SUCCESS;
}


/* -=-=-=-=- IPortEx -=-=-=-=- */

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCompleteNotify}
 */
static DECLCALLBACK(int) drvscsiIoReqCompleteNotify(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                    void *pvIoReqAlloc, int rcReq)
{
    RT_NOREF1(hIoReq);

    PDRVSCSI pThis = RT_FROM_MEMBER(pInterface, DRVSCSI, IPortEx);
    VSCSIIOREQ hVScsiIoReq = (VSCSIIOREQ)((uint8_t *)pvIoReqAlloc + sizeof(PDMMEDIAEXIOREQ));
    VSCSIIOREQTXDIR enmTxDir = VSCSIIoReqTxDirGet(hVScsiIoReq);

    LogFlowFunc(("Request hVScsiIoReq=%#p completed\n", hVScsiIoReq));

    if (enmTxDir == VSCSIIOREQTXDIR_READ)
        pThis->pLed->Actual.s.fReading = 0;
    else if (   enmTxDir == VSCSIIOREQTXDIR_WRITE
             || enmTxDir == VSCSIIOREQTXDIR_UNMAP)
        pThis->pLed->Actual.s.fWriting = 0;
    else
        AssertMsg(enmTxDir == VSCSIIOREQTXDIR_FLUSH, ("Invalid transfer direction %u\n", enmTxDir));

    if (RT_SUCCESS(rcReq))
        VSCSIIoReqCompleted(hVScsiIoReq, rcReq, false /* fRedoPossible */);
    else
    {
        pThis->cErrors++;
        if (pThis->cErrors < MAX_LOG_REL_ERRORS)
        {
            if (enmTxDir == VSCSIIOREQTXDIR_FLUSH)
                LogRel(("SCSI#%u: Flush returned rc=%Rrc\n",
                        pThis->pDrvIns->iInstance, rcReq));
            else if (enmTxDir == VSCSIIOREQTXDIR_UNMAP)
                LogRel(("SCSI#%u: Unmap returned rc=%Rrc\n",
                        pThis->pDrvIns->iInstance, rcReq));
            else
            {
                uint64_t  uOffset    = 0;
                size_t    cbTransfer = 0;
                size_t    cbSeg      = 0;
                PCRTSGSEG paSeg      = NULL;
                unsigned  cSeg       = 0;

                VSCSIIoReqParamsGet(hVScsiIoReq, &uOffset, &cbTransfer,
                                    &cSeg, &cbSeg, &paSeg);

                LogRel(("SCSI#%u: %s at offset %llu (%u bytes left) returned rc=%Rrc\n",
                        pThis->pDrvIns->iInstance,
                        enmTxDir == VSCSIIOREQTXDIR_READ
                        ? "Read"
                        : "Write",
                        uOffset,
                        cbTransfer, rcReq));
            }
        }

        VSCSIIoReqCompleted(hVScsiIoReq, rcReq, drvscsiIsRedoPossible(rcReq));
    }

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyFromBuf}
 */
static DECLCALLBACK(int) drvscsiIoReqCopyFromBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                 void *pvIoReqAlloc, uint32_t offDst, PRTSGBUF pSgBuf,
                                                 size_t cbCopy)
{
    RT_NOREF2(pInterface, hIoReq);

    VSCSIIOREQ hVScsiIoReq = DRVSCSI_PDMMEDIAEXIOREQ_2_VSCSIIOREQ(pvIoReqAlloc);
    uint64_t  uOffset    = 0;
    size_t    cbTransfer = 0;
    size_t    cbSeg      = 0;
    PCRTSGSEG paSeg      = NULL;
    unsigned  cSeg       = 0;
    size_t    cbCopied   = 0;

    int rc = VSCSIIoReqParamsGet(hVScsiIoReq, &uOffset, &cbTransfer, &cSeg, &cbSeg, &paSeg);
    if (RT_SUCCESS(rc))
    {
        RTSGBUF SgBuf;
        RTSgBufInit(&SgBuf, paSeg, cSeg);

        RTSgBufAdvance(&SgBuf, offDst);
        cbCopied = RTSgBufCopy(&SgBuf, pSgBuf, cbCopy);
    }

    return cbCopied == cbCopy ? VINF_SUCCESS : VERR_PDM_MEDIAEX_IOBUF_OVERFLOW;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyToBuf}
 */
static DECLCALLBACK(int) drvscsiIoReqCopyToBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                               void *pvIoReqAlloc, uint32_t offSrc, PRTSGBUF pSgBuf,
                                               size_t cbCopy)
{
    RT_NOREF2(pInterface, hIoReq);

    VSCSIIOREQ hVScsiIoReq = DRVSCSI_PDMMEDIAEXIOREQ_2_VSCSIIOREQ(pvIoReqAlloc);
    uint64_t  uOffset    = 0;
    size_t    cbTransfer = 0;
    size_t    cbSeg      = 0;
    PCRTSGSEG paSeg      = NULL;
    unsigned  cSeg       = 0;
    size_t    cbCopied   = 0;

    int rc = VSCSIIoReqParamsGet(hVScsiIoReq, &uOffset, &cbTransfer, &cSeg, &cbSeg, &paSeg);
    if (RT_SUCCESS(rc))
    {
        RTSGBUF SgBuf;
        RTSgBufInit(&SgBuf, paSeg, cSeg);

        RTSgBufAdvance(&SgBuf, offSrc);
        cbCopied = RTSgBufCopy(pSgBuf, &SgBuf, cbCopy);
    }

    return cbCopied == cbCopy ? VINF_SUCCESS : VERR_PDM_MEDIAEX_IOBUF_UNDERRUN;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqStateChanged}
 */
static DECLCALLBACK(void) drvscsiIoReqStateChanged(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                   void *pvIoReqAlloc, PDMMEDIAEXIOREQSTATE enmState)
{
    RT_NOREF4(pInterface, hIoReq, pvIoReqAlloc, enmState);
    AssertLogRelMsgFailed(("This should not be hit because I/O requests should not be suspended\n"));
}

static DECLCALLBACK(void) drvscsiVScsiReqCompleted(VSCSIDEVICE hVScsiDevice, void *pVScsiDeviceUser,
                                                   void *pVScsiReqUser, int rcScsiCode, bool fRedoPossible, int rcReq)
{
    RT_NOREF(hVScsiDevice);
    PDRVSCSI pThis = (PDRVSCSI)pVScsiDeviceUser;

    ASMAtomicDecU32(&pThis->StatIoDepth);

    pThis->pDevScsiPort->pfnSCSIRequestCompleted(pThis->pDevScsiPort, (PPDMSCSIREQUEST)pVScsiReqUser,
                                                 rcScsiCode, fRedoPossible, rcReq);

    if (RT_UNLIKELY(pThis->fDummySignal) && !pThis->StatIoDepth)
        PDMDrvHlpAsyncNotificationCompleted(pThis->pDrvIns);
}

/* -=-=-=-=- ISCSIConnector -=-=-=-=- */

#ifdef DEBUG
/**
 * Dumps a SCSI request structure for debugging purposes.
 *
 * @returns nothing.
 * @param   pRequest    Pointer to the request to dump.
 */
static void drvscsiDumpScsiRequest(PPDMSCSIREQUEST pRequest)
{
    Log(("Dump for pRequest=%#p Command: %s\n", pRequest, SCSICmdText(pRequest->pbCDB[0])));
    Log(("cbCDB=%u\n", pRequest->cbCDB));
    for (uint32_t i = 0; i < pRequest->cbCDB; i++)
        Log(("pbCDB[%u]=%#x\n", i, pRequest->pbCDB[i]));
    Log(("cbScatterGather=%u\n", pRequest->cbScatterGather));
    Log(("cScatterGatherEntries=%u\n", pRequest->cScatterGatherEntries));
    /* Print all scatter gather entries. */
    for (uint32_t i = 0; i < pRequest->cScatterGatherEntries; i++)
    {
        Log(("ScatterGatherEntry[%u].cbSeg=%u\n", i, pRequest->paScatterGatherHead[i].cbSeg));
        Log(("ScatterGatherEntry[%u].pvSeg=%#p\n", i, pRequest->paScatterGatherHead[i].pvSeg));
    }
    Log(("pvUser=%#p\n", pRequest->pvUser));
}
#endif

/** @interface_method_impl{PDMISCSICONNECTOR,pfnSCSIRequestSend} */
static DECLCALLBACK(int) drvscsiRequestSend(PPDMISCSICONNECTOR pInterface, PPDMSCSIREQUEST pSCSIRequest)
{
    int rc;
    PDRVSCSI pThis = RT_FROM_MEMBER(pInterface, DRVSCSI, ISCSIConnector);
    VSCSIREQ hVScsiReq;

#ifdef DEBUG
    drvscsiDumpScsiRequest(pSCSIRequest);
#endif

    rc = VSCSIDeviceReqCreate(pThis->hVScsiDevice, &hVScsiReq,
                              pSCSIRequest->uLogicalUnit,
                              pSCSIRequest->pbCDB,
                              pSCSIRequest->cbCDB,
                              pSCSIRequest->cbScatterGather,
                              pSCSIRequest->cScatterGatherEntries,
                              pSCSIRequest->paScatterGatherHead,
                              pSCSIRequest->pbSenseBuffer,
                              pSCSIRequest->cbSenseBuffer,
                              pSCSIRequest);
    if (RT_FAILURE(rc))
        return rc;

    ASMAtomicIncU32(&pThis->StatIoDepth);
    rc = VSCSIDeviceReqEnqueue(pThis->hVScsiDevice, hVScsiReq);

    return rc;
}

/** @interface_method_impl{PDMISCSICONNECTOR,pfnQueryLUNType} */
static DECLCALLBACK(int) drvscsiQueryLUNType(PPDMISCSICONNECTOR pInterface, uint32_t iLun, PPDMSCSILUNTYPE pLunType)
{
    PDRVSCSI pThis = RT_FROM_MEMBER(pInterface, DRVSCSI, ISCSIConnector);
    VSCSILUNTYPE enmLunType;

    int rc = VSCSIDeviceLunQueryType(pThis->hVScsiDevice, iLun, &enmLunType);
    if (RT_FAILURE(rc))
        return rc;

    switch (enmLunType)
    {
    case VSCSILUNTYPE_SBC:  *pLunType = PDMSCSILUNTYPE_SBC; break;
    case VSCSILUNTYPE_MMC:  *pLunType = PDMSCSILUNTYPE_MMC; break;
    case VSCSILUNTYPE_SSC:  *pLunType = PDMSCSILUNTYPE_SSC; break;
    default:                *pLunType = PDMSCSILUNTYPE_INVALID;
    }

    return rc;
}

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvscsiQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVSCSI    pThis   = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUNT, pThis->pDrvMount);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISCSICONNECTOR, &pThis->ISCSIConnector);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT, &pThis->IPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUNTNOTIFY, &pThis->IMountNotify);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAEXPORT, &pThis->IPortEx);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIA, pThis->pDrvMedia);
    return NULL;
}

static DECLCALLBACK(int) drvscsiQueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                    uint32_t *piInstance, uint32_t *piLUN)
{
    PDRVSCSI pThis = RT_FROM_MEMBER(pInterface, DRVSCSI, IPort);

    return pThis->pDevScsiPort->pfnQueryDeviceLocation(pThis->pDevScsiPort, ppcszController,
                                                       piInstance, piLUN);
}

/**
 * Called when media is mounted.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) drvscsiMountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    PDRVSCSI pThis = RT_FROM_MEMBER(pInterface, DRVSCSI, IMountNotify);
    LogFlowFunc(("mounting LUN#%p\n", pThis->hVScsiLun));

    /* Ignore the call if we're called while being attached. */
    if (!pThis->pDrvMedia)
        return;

    /* Let the LUN know that a medium was mounted. */
    VSCSILunMountNotify(pThis->hVScsiLun);
}

/**
 * Called when media is unmounted
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) drvscsiUnmountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    PDRVSCSI pThis = RT_FROM_MEMBER(pInterface, DRVSCSI, IMountNotify);
    LogFlowFunc(("unmounting LUN#%p\n", pThis->hVScsiLun));

    /* Let the LUN know that the medium was unmounted. */
    VSCSILunUnmountNotify(pThis->hVScsiLun);
}

/**
 * Worker for drvscsiReset, drvscsiSuspend and drvscsiPowerOff.
 *
 * @param   pDrvIns         The driver instance.
 * @param   pfnAsyncNotify  The async callback.
 */
static void drvscsiR3ResetOrSuspendOrPowerOff(PPDMDRVINS pDrvIns, PFNPDMDRVASYNCNOTIFY pfnAsyncNotify)
{
    RT_NOREF1(pfnAsyncNotify);

    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    if (pThis->StatIoDepth > 0)
        ASMAtomicWriteBool(&pThis->fDummySignal, true);
}

/**
 * Callback employed by drvscsiSuspend and drvscsiPowerOff.
 *
 * @returns true if we've quiesced, false if we're still working.
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(bool) drvscsiIsAsyncSuspendOrPowerOffDone(PPDMDRVINS pDrvIns)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    if (pThis->StatIoDepth > 0)
        return false;
    else
        return true;
}

/**
 * @copydoc FNPDMDRVPOWEROFF
 */
static DECLCALLBACK(void) drvscsiPowerOff(PPDMDRVINS pDrvIns)
{
    drvscsiR3ResetOrSuspendOrPowerOff(pDrvIns, drvscsiIsAsyncSuspendOrPowerOffDone);
}

/**
 * @copydoc FNPDMDRVSUSPEND
 */
static DECLCALLBACK(void) drvscsiSuspend(PPDMDRVINS pDrvIns)
{
    drvscsiR3ResetOrSuspendOrPowerOff(pDrvIns, drvscsiIsAsyncSuspendOrPowerOffDone);
}

/**
 * Callback employed by drvscsiReset.
 *
 * @returns true if we've quiesced, false if we're still working.
 * @param   pDrvIns     The driver instance.
 */
static DECLCALLBACK(bool) drvscsiIsAsyncResetDone(PPDMDRVINS pDrvIns)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    if (pThis->StatIoDepth > 0)
        return false;
    else
        return true;
}

/**
 * @copydoc FNPDMDRVRESET
 */
static DECLCALLBACK(void) drvscsiReset(PPDMDRVINS pDrvIns)
{
    drvscsiR3ResetOrSuspendOrPowerOff(pDrvIns, drvscsiIsAsyncResetDone);
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvscsiDestruct(PPDMDRVINS pDrvIns)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    /* Free the VSCSI device and LUN handle. */
    if (pThis->hVScsiDevice)
    {
        VSCSILUN hVScsiLun;
        int rc = VSCSIDeviceLunDetach(pThis->hVScsiDevice, 0, &hVScsiLun);
        AssertRC(rc);

        Assert(hVScsiLun == pThis->hVScsiLun);
        rc = VSCSILunDestroy(hVScsiLun);
        AssertRC(rc);
        rc = VSCSIDeviceDestroy(pThis->hVScsiDevice);
        AssertRC(rc);

        pThis->hVScsiDevice = NULL;
        pThis->hVScsiLun    = NULL;
    }

    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->StatBytesRead);
    PDMDrvHlpSTAMDeregister(pDrvIns, &pThis->StatBytesWritten);
    PDMDrvHlpSTAMDeregister(pDrvIns, (void *)&pThis->StatIoDepth);
}

/**
 * Construct a block driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvscsiConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);
    LogFlowFunc(("pDrvIns=%#p pCfg=%#p\n", pDrvIns, pCfg));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Initialize the instance data.
     */
    pThis->pDrvIns                              = pDrvIns;
    pThis->ISCSIConnector.pfnSCSIRequestSend    = drvscsiRequestSend;
    pThis->ISCSIConnector.pfnQueryLUNType       = drvscsiQueryLUNType;

    pDrvIns->IBase.pfnQueryInterface            = drvscsiQueryInterface;

    pThis->IMountNotify.pfnMountNotify          = drvscsiMountNotify;
    pThis->IMountNotify.pfnUnmountNotify        = drvscsiUnmountNotify;
    pThis->IPort.pfnQueryDeviceLocation         = drvscsiQueryDeviceLocation;
    pThis->IPortEx.pfnIoReqCompleteNotify       = drvscsiIoReqCompleteNotify;
    pThis->IPortEx.pfnIoReqCopyFromBuf          = drvscsiIoReqCopyFromBuf;
    pThis->IPortEx.pfnIoReqCopyToBuf            = drvscsiIoReqCopyToBuf;
    pThis->IPortEx.pfnIoReqStateChanged         = drvscsiIoReqStateChanged;

    /* Query the SCSI port interface above. */
    pThis->pDevScsiPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMISCSIPORT);
    AssertMsgReturn(pThis->pDevScsiPort, ("Missing SCSI port interface above\n"), VERR_PDM_MISSING_INTERFACE);

    /* Query the optional LED interface above. */
    pThis->pLedPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMILEDPORTS);
    if (pThis->pLedPort != NULL)
    {
        /* Get The Led. */
        rc = pThis->pLedPort->pfnQueryStatusLed(pThis->pLedPort, 0, &pThis->pLed);
        if (RT_FAILURE(rc))
            pThis->pLed = &pThis->Led;
    }
    else
        pThis->pLed = &pThis->Led;

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "NonRotationalMedium\0Readonly\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("SCSI configuration error: unknown option specified"));

    rc = CFGMR3QueryBoolDef(pCfg, "NonRotationalMedium", &pThis->fNonRotational, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                    N_("SCSI configuration error: failed to read \"NonRotationalMedium\" as boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "Readonly", &pThis->fReadonly, false);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("SCSI configuration error: failed to read \"Readonly\" as boolean"));

    /*
     * Try attach driver below and query it's block interface.
     */
    rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pThis->pDrvBase);
    AssertMsgReturn(RT_SUCCESS(rc), ("Attaching driver below failed rc=%Rrc\n", rc), rc);

    /*
     * Query the block and blockbios interfaces.
     */
    pThis->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIMEDIA);
    AssertMsgReturn(VALID_PTR(pThis->pDrvMedia), ("VSCSI configuration error: No media interface!\n"),
                    VERR_PDM_MISSING_INTERFACE);

    /* Query the extended media interface. */
    pThis->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIMEDIAEX);
    AssertMsgReturn(VALID_PTR(pThis->pDrvMediaEx), ("VSCSI configuration error: No extended media interface!\n"),
                    VERR_PDM_MISSING_INTERFACE);

    pThis->pDrvMount = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIMOUNT);

    PDMMEDIATYPE enmType = pThis->pDrvMedia->pfnGetType(pThis->pDrvMedia);
    VSCSILUNTYPE enmLunType;
    switch (enmType)
    {
    case PDMMEDIATYPE_HARD_DISK:
        enmLunType = VSCSILUNTYPE_SBC;
        break;
    case PDMMEDIATYPE_CDROM:
    case PDMMEDIATYPE_DVD:
        enmLunType = VSCSILUNTYPE_MMC;
        break;
    default:
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_UNSUPPORTED_BLOCK_TYPE, RT_SRC_POS,
                                   N_("Only hard disks and CD/DVD-ROMs are currently supported as SCSI devices (enmType=%d)"),
                                   enmType);
    }
    if (    (   enmType == PDMMEDIATYPE_DVD
             || enmType == PDMMEDIATYPE_CDROM)
        &&  !pThis->pDrvMount)
    {
        AssertMsgFailed(("Internal error: cdrom without a mountable interface\n"));
        return VERR_INTERNAL_ERROR;
    }

    /* Create VSCSI device and LUN. */
    pThis->VScsiIoCallbacks.pfnVScsiLunReqAllocSizeSet     = drvscsiReqAllocSizeSet;
    pThis->VScsiIoCallbacks.pfnVScsiLunReqAlloc            = drvscsiReqAlloc;
    pThis->VScsiIoCallbacks.pfnVScsiLunReqFree             = drvscsiReqFree;
    pThis->VScsiIoCallbacks.pfnVScsiLunMediumGetSize       = drvscsiGetSize;
    pThis->VScsiIoCallbacks.pfnVScsiLunMediumGetSectorSize = drvscsiGetSectorSize;
    pThis->VScsiIoCallbacks.pfnVScsiLunReqTransferEnqueue  = drvscsiReqTransferEnqueue;
    pThis->VScsiIoCallbacks.pfnVScsiLunGetFeatureFlags     = drvscsiGetFeatureFlags;
    pThis->VScsiIoCallbacks.pfnVScsiLunMediumSetLock       = drvscsiSetLock;

    rc = VSCSIDeviceCreate(&pThis->hVScsiDevice, drvscsiVScsiReqCompleted, pThis);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create VSCSI device rc=%Rrc\n", rc), rc);
    rc = VSCSILunCreate(&pThis->hVScsiLun, enmLunType, &pThis->VScsiIoCallbacks,
                        pThis);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create VSCSI LUN rc=%Rrc\n", rc), rc);
    rc = VSCSIDeviceLunAttach(pThis->hVScsiDevice, pThis->hVScsiLun, 0);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to attached the LUN to the SCSI device\n"), rc);

    /// @todo This is a very hacky way of telling the LUN whether a medium was mounted.
    // The mount/unmount interface doesn't work in a very sensible manner!
    if (pThis->pDrvMount)
    {
        if (pThis->pDrvMedia->pfnGetSize(pThis->pDrvMedia))
        {
            rc = VINF_SUCCESS; VSCSILunMountNotify(pThis->hVScsiLun);
            AssertMsgReturn(RT_SUCCESS(rc), ("Failed to notify the LUN of media being mounted\n"), rc);
        }
        else
        {
            rc = VINF_SUCCESS; VSCSILunUnmountNotify(pThis->hVScsiLun);
            AssertMsgReturn(RT_SUCCESS(rc), ("Failed to notify the LUN of media being unmounted\n"), rc);
        }
    }

    const char *pszCtrl = NULL;
    uint32_t iCtrlInstance = 0;
    uint32_t iCtrlLun = 0;

    rc = pThis->pDevScsiPort->pfnQueryDeviceLocation(pThis->pDevScsiPort, &pszCtrl, &iCtrlInstance, &iCtrlLun);
    if (RT_SUCCESS(rc))
    {
        const char *pszCtrlId =   strcmp(pszCtrl, "Msd") == 0 ? "USB"
                                : strcmp(pszCtrl, "lsilogicsas") == 0 ? "SAS"
                                : "SCSI";
        /* Register statistics counter. */
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesRead, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                                "Amount of data read.", "/Devices/%s%u/%u/ReadBytes", pszCtrlId, iCtrlInstance, iCtrlLun);
        PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                                "Amount of data written.", "/Devices/%s%u/%u/WrittenBytes", pszCtrlId, iCtrlInstance, iCtrlLun);
        PDMDrvHlpSTAMRegisterF(pDrvIns, (void *)&pThis->StatIoDepth, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                                "Number of active tasks.", "/Devices/%s%u/%u/IoDepth", pszCtrlId, iCtrlInstance, iCtrlLun);
    }

    pThis->StatIoDepth = 0;

    uint32_t fFeatures = 0;
    rc = pThis->pDrvMediaEx->pfnQueryFeatures(pThis->pDrvMediaEx, &fFeatures);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("VSCSI configuration error: Failed to query features of device"));
    if (fFeatures & PDMIMEDIAEX_FEATURE_F_DISCARD)
        LogRel(("SCSI#%d: Enabled UNMAP support\n", pDrvIns->iInstance));

    return VINF_SUCCESS;
}

/**
 * SCSI driver registration record.
 */
const PDMDRVREG g_DrvSCSI =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "SCSI",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Generic SCSI driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_SCSI,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVSCSI),
    /* pfnConstruct */
    drvscsiConstruct,
    /* pfnDestruct */
    drvscsiDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    drvscsiReset,
    /* pfnSuspend */
    drvscsiSuspend,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvscsiPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
