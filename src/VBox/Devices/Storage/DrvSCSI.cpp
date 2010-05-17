/* $Id$ */
/** @file
 * VBox storage drivers: Generic SCSI command parser and execution driver
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
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DRV_SCSI
#include <VBox/pdmdrv.h>
#include <VBox/pdmifs.h>
#include <VBox/pdmthread.h>
#include <VBox/vscsi.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/req.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "Builtins.h"

/** The maximum number of release log entries per device. */
#define MAX_LOG_REL_ERRORS  1024

/**
 * SCSI driver instance data.
 *
 * @implements  PDMISCSICONNECTOR
 * @implements  PDMIBLOCKASYNCPORT
 * @implements  PDMIMOUNTNOTIFY
 */
typedef struct DRVSCSI
{
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;

    /** Pointer to the attached driver's base interface. */
    PPDMIBASE               pDrvBase;
    /** Pointer to the attached driver's block interface. */
    PPDMIBLOCK              pDrvBlock;
    /** Pointer to the attached driver's async block interface. */
    PPDMIBLOCKASYNC         pDrvBlockAsync;
    /** Pointer to the attached driver's block bios interface. */
    PPDMIBLOCKBIOS          pDrvBlockBios;
    /** Pointer to the attached driver's mount interface. */
    PPDMIMOUNT              pDrvMount;
    /** Pointer to the SCSI port interface of the device above. */
    PPDMISCSIPORT           pDevScsiPort;
    /** pointer to the Led port interface of the dveice above. */
    PPDMILEDPORTS           pLedPort;
    /** The scsi connector interface .*/
    PDMISCSICONNECTOR       ISCSIConnector;
    /** The block port interface. */
    PDMIBLOCKPORT           IPort;
    /** The optional block async port interface. */
    PDMIBLOCKASYNCPORT      IPortAsync;
#if 0 /* these interfaces aren't implemented */
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY         IMountNotify;
#endif
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

    /** The dedicated I/O thread for the non async approach. */
    PPDMTHREAD              pAsyncIOThread;
    /** Queue for passing the requests to the thread. */
    PRTREQQUEUE             pQueueRequests;
    /** Request that we've left pending on wakeup or reset. */
    PRTREQ                  pPendingDummyReq;
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
} DRVSCSI, *PDRVSCSI;

/** Converts a pointer to DRVSCSI::ISCSIConnector to a PDRVSCSI. */
#define PDMISCSICONNECTOR_2_DRVSCSI(pInterface) ( (PDRVSCSI)((uintptr_t)pInterface - RT_OFFSETOF(DRVSCSI, ISCSIConnector)) )
/** Converts a pointer to DRVSCSI::IPortAsync to a PDRVSCSI. */
#define PDMIBLOCKASYNCPORT_2_DRVSCSI(pInterface) ( (PDRVSCSI)((uintptr_t)pInterface - RT_OFFSETOF(DRVSCSI, IPortAsync)) )

static int drvscsiProcessRequestOne(PDRVSCSI pThis, VSCSIIOREQ hVScsiIoReq)
{
    int rc = VINF_SUCCESS;
    VSCSIIOREQTXDIR enmTxDir;

    enmTxDir = VSCSIIoReqTxDirGet(hVScsiIoReq);

    switch (enmTxDir)
    {
        case VSCSIIOREQTXDIR_FLUSH:
        {
            rc = pThis->pDrvBlock->pfnFlush(pThis->pDrvBlock);
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

            rc = VSCSIIoReqParamsGet(hVScsiIoReq, &uOffset, &cbTransfer, &cSeg, &cbSeg,
                                     &paSeg);
            AssertRC(rc);

            while (cbTransfer && cSeg)
            {
                size_t cbProcess = (cbTransfer < paSeg->cbSeg) ? cbTransfer : paSeg->cbSeg;

                Log(("%s: uOffset=%llu cbProcess=%u\n", __FUNCTION__, uOffset, cbProcess));

                if (enmTxDir == VSCSIIOREQTXDIR_READ)
                {
                    pThis->pLed->Asserted.s.fReading = pThis->pLed->Actual.s.fReading = 1;
                    rc = pThis->pDrvBlock->pfnRead(pThis->pDrvBlock, uOffset,
                                                    paSeg->pvSeg, cbProcess);
                    pThis->pLed->Actual.s.fReading = 0;
                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("%s: Failed to read data %Rrc\n", __FUNCTION__, rc));
                    STAM_REL_COUNTER_ADD(&pThis->StatBytesRead, cbProcess);
                }
                else
                {
                    pThis->pLed->Asserted.s.fWriting = pThis->pLed->Actual.s.fWriting = 1;
                    rc = pThis->pDrvBlock->pfnWrite(pThis->pDrvBlock, uOffset,
                                                    paSeg->pvSeg, cbProcess);
                    pThis->pLed->Actual.s.fWriting = 0;
                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("%s: Failed to write data %Rrc\n", __FUNCTION__, rc));
                    STAM_REL_COUNTER_ADD(&pThis->StatBytesWritten, cbProcess);
                }

                /* Go to the next entry. */
                uOffset     += cbProcess;
                cbTransfer -= cbProcess;
                paSeg++;
                cSeg--;
            }

            break;
        }
        default:
            AssertMsgFailed(("Invalid transfer direction %d\n", enmTxDir));
    }

    VSCSIIoReqCompleted(hVScsiIoReq, rc);

    return VINF_SUCCESS;
}

static int drvscsiGetSize(VSCSILUN hVScsiLun, void *pvScsiLunUser, uint64_t *pcbSize)
{
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    *pcbSize = pThis->pDrvBlock->pfnGetSize(pThis->pDrvBlock);

    return VINF_SUCCESS;
}

static int drvscsiTransferCompleteNotify(PPDMIBLOCKASYNCPORT pInterface, void *pvUser, int rc)
{
    PDRVSCSI pThis = PDMIBLOCKASYNCPORT_2_DRVSCSI(pInterface);
    VSCSIIOREQ hVScsiIoReq = (VSCSIIOREQ)pvUser;
    VSCSIIOREQTXDIR enmTxDir = VSCSIIoReqTxDirGet(hVScsiIoReq);

    LogFlowFunc(("Request hVScsiIoReq=%#p completed\n", hVScsiIoReq));

    if (enmTxDir == VSCSIIOREQTXDIR_READ)
        pThis->pLed->Actual.s.fReading = 0;
    else if (enmTxDir == VSCSIIOREQTXDIR_WRITE)
        pThis->pLed->Actual.s.fWriting = 0;
    else
        AssertMsg(enmTxDir == VSCSIIOREQTXDIR_FLUSH, ("Invalid transfer direction %u\n", enmTxDir));

    VSCSIIoReqCompleted(hVScsiIoReq, rc);

    return VINF_SUCCESS;
}

static int drvscsiReqTransferEnqueue(VSCSILUN hVScsiLun,
                                     void *pvScsiLunUser,
                                     VSCSIIOREQ hVScsiIoReq)
{
    int rc = VINF_SUCCESS;
    PDRVSCSI pThis = (PDRVSCSI)pvScsiLunUser;

    if (pThis->pDrvBlockAsync)
    {
        /* async I/O path. */
        VSCSIIOREQTXDIR enmTxDir;

        LogFlowFunc(("Enqueuing hVScsiIoReq=%#p\n", hVScsiIoReq));

        enmTxDir = VSCSIIoReqTxDirGet(hVScsiIoReq);

        switch (enmTxDir)
        {
            case VSCSIIOREQTXDIR_FLUSH:
            {
                rc = pThis->pDrvBlockAsync->pfnStartFlush(pThis->pDrvBlockAsync, hVScsiIoReq);
                if (   RT_FAILURE(rc)
                    && rc != VERR_VD_ASYNC_IO_IN_PROGRESS
                    && pThis->cErrors++ < MAX_LOG_REL_ERRORS)
                    LogRel(("SCSI#%u: Flush returned rc=%Rrc\n",
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
                    rc = pThis->pDrvBlockAsync->pfnStartRead(pThis->pDrvBlockAsync, uOffset,
                                                             paSeg, cSeg, cbTransfer,
                                                             hVScsiIoReq);
                    STAM_REL_COUNTER_ADD(&pThis->StatBytesRead, cbTransfer);
                }
                else
                {
                    pThis->pLed->Asserted.s.fWriting = pThis->pLed->Actual.s.fWriting = 1;
                    rc = pThis->pDrvBlockAsync->pfnStartWrite(pThis->pDrvBlockAsync, uOffset,
                                                              paSeg, cSeg, cbTransfer,
                                                              hVScsiIoReq);
                    STAM_REL_COUNTER_ADD(&pThis->StatBytesWritten, cbTransfer);
                }

                if (   RT_FAILURE(rc)
                    && rc != VERR_VD_ASYNC_IO_IN_PROGRESS
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

        if (rc == VINF_VD_ASYNC_IO_FINISHED)
        {
            if (enmTxDir == VSCSIIOREQTXDIR_READ)
                pThis->pLed->Actual.s.fReading = 0;
            else if (enmTxDir == VSCSIIOREQTXDIR_WRITE)
                pThis->pLed->Actual.s.fWriting = 0;
            else
                AssertMsg(enmTxDir == VSCSIIOREQTXDIR_FLUSH, ("Invalid transfer direction %u\n", enmTxDir));

            VSCSIIoReqCompleted(hVScsiIoReq, VINF_SUCCESS);
        }
        else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
            rc = VINF_SUCCESS;
        else if (RT_FAILURE(rc))
        {
            if (enmTxDir == VSCSIIOREQTXDIR_READ)
                pThis->pLed->Actual.s.fReading = 0;
            else if (enmTxDir == VSCSIIOREQTXDIR_WRITE)
                pThis->pLed->Actual.s.fWriting = 0;
            else
                AssertMsg(enmTxDir == VSCSIIOREQTXDIR_FLUSH, ("Invalid transfer direction %u\n", enmTxDir));

            VSCSIIoReqCompleted(hVScsiIoReq, rc);
            rc = VINF_SUCCESS;
        }
        else
            AssertMsgFailed(("Invalid return code rc=%Rrc\n", rc));
    }
    else
    {
        /* I/O thread. */
        rc = RTReqCallEx(pThis->pQueueRequests, NULL, 0, RTREQFLAGS_NO_WAIT,
                         (PFNRT)drvscsiProcessRequestOne, 2, pThis, hVScsiIoReq);
    }

    return rc;
}

static void drvscsiVScsiReqCompleted(VSCSIDEVICE hVScsiDevice, void *pVScsiDeviceUser,
                                     void *pVScsiReqUser, int rcReq)
{
    PDRVSCSI pThis = (PDRVSCSI)pVScsiDeviceUser;

    ASMAtomicDecU32(&pThis->StatIoDepth);

    pThis->pDevScsiPort->pfnSCSIRequestCompleted(pThis->pDevScsiPort, (PPDMSCSIREQUEST)pVScsiReqUser,
                                                 rcReq);

    if (RT_UNLIKELY(pThis->fDummySignal) && !pThis->StatIoDepth)
        PDMDrvHlpAsyncNotificationCompleted(pThis->pDrvIns);
}

/**
 * Dummy request function used by drvscsiReset to wait for all pending requests
 * to complete prior to the device reset.
 *
 * @param   pThis           Pointer to the instace data.
 * @returns VINF_SUCCESS.
 */
static int drvscsiAsyncIOLoopSyncCallback(PDRVSCSI pThis)
{
    if (pThis->fDummySignal)
        PDMDrvHlpAsyncNotificationCompleted(pThis->pDrvIns);
    return VINF_SUCCESS;
}

/**
 * Request function to wakeup the thread.
 *
 * @param   pThis           Pointer to the instace data.
 * @returns VWRN_STATE_CHANGED.
 */
static int drvscsiAsyncIOLoopWakeupFunc(PDRVSCSI pThis)
{
    if (pThis->fDummySignal)
        PDMDrvHlpAsyncNotificationCompleted(pThis->pDrvIns);
    return VWRN_STATE_CHANGED;
}

/**
 * The thread function which processes the requests asynchronously.
 *
 * @returns VBox status code.
 * @param   pDrvIns    Pointer to the driver instance data.
 * @param   pThread    Pointer to the thread instance data.
 */
static int drvscsiAsyncIOLoop(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    int rc = VINF_SUCCESS;
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    LogFlowFunc(("Entering async IO loop.\n"));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        rc = RTReqProcess(pThis->pQueueRequests, RT_INDEFINITE_WAIT);
        AssertMsg(rc == VWRN_STATE_CHANGED, ("Left RTReqProcess and error code is not VWRN_STATE_CHANGED rc=%Rrc\n", rc));
    }

    return VINF_SUCCESS;
}

/**
 * Deals with any pending dummy request
 *
 * @returns true if no pending dummy request, false if still pending.
 * @param   pThis               The instance data.
 * @param   cMillies            The number of milliseconds to wait for any
 *                              pending request to finish.
 */
static bool drvscsiAsyncIOLoopNoPendingDummy(PDRVSCSI pThis, uint32_t cMillies)
{
    if (!pThis->pPendingDummyReq)
        return true;
    int rc = RTReqWait(pThis->pPendingDummyReq, cMillies);
    if (RT_FAILURE(rc))
        return false;
    RTReqFree(pThis->pPendingDummyReq);
    pThis->pPendingDummyReq = NULL;
    return true;
}

static int drvscsiAsyncIOLoopWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);
    PRTREQ pReq;
    int rc;

    AssertMsgReturn(pThis->pQueueRequests, ("pQueueRequests is NULL\n"), VERR_INVALID_STATE);

    if (!drvscsiAsyncIOLoopNoPendingDummy(pThis, 10000 /* 10 sec */))
    {
        LogRel(("drvscsiAsyncIOLoopWakeup#%u: previous dummy request is still pending\n", pDrvIns->iInstance));
        return VERR_TIMEOUT;
    }

    rc = RTReqCall(pThis->pQueueRequests, &pReq, 10000 /* 10 sec. */, (PFNRT)drvscsiAsyncIOLoopWakeupFunc, 1, pThis);
    if (RT_SUCCESS(rc))
        RTReqFree(pReq);
    else
    {
        pThis->pPendingDummyReq = pReq;
        LogRel(("drvscsiAsyncIOLoopWakeup#%u: %Rrc pReq=%p\n", pDrvIns->iInstance, rc, pReq));
    }

    return rc;
}

/* -=-=-=-=- ISCSIConnector -=-=-=-=- */

/** @copydoc PDMISCSICONNECTOR::pfnSCSIRequestSend. */
static DECLCALLBACK(int) drvscsiRequestSend(PPDMISCSICONNECTOR pInterface, PPDMSCSIREQUEST pSCSIRequest)
{
    int rc;
    PDRVSCSI pThis = PDMISCSICONNECTOR_2_DRVSCSI(pInterface);
    VSCSIREQ hVScsiReq;

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

/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvscsiQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVSCSI    pThis   = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISCSICONNECTOR, &pThis->ISCSIConnector);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBLOCKPORT, &pThis->IPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBLOCKASYNCPORT, &pThis->IPortAsync);
    return NULL;
}

/**
 * Worker for drvscsiReset, drvscsiSuspend and drvscsiPowerOff.
 *
 * @param   pDrvIns         The driver instance.
 * @param   pfnAsyncNotify  The async callback.
 */
static void drvscsiR3ResetOrSuspendOrPowerOff(PPDMDRVINS pDrvIns, PFNPDMDRVASYNCNOTIFY pfnAsyncNotify)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    if (!pThis->pDrvBlockAsync)
    {
        if (!pThis->pQueueRequests)
            return;

        ASMAtomicWriteBool(&pThis->fDummySignal, true);
        if (drvscsiAsyncIOLoopNoPendingDummy(pThis, 0 /*ms*/))
        {
            if (!RTReqIsBusy(pThis->pQueueRequests))
            {
                ASMAtomicWriteBool(&pThis->fDummySignal, false);
                return;
            }

            PRTREQ pReq;
            int rc = RTReqCall(pThis->pQueueRequests, &pReq, 0 /*ms*/, (PFNRT)drvscsiAsyncIOLoopSyncCallback, 1, pThis);
            if (RT_SUCCESS(rc))
            {
                ASMAtomicWriteBool(&pThis->fDummySignal, false);
                RTReqFree(pReq);
                return;
            }

            pThis->pPendingDummyReq = pReq;
        }
    }
    else
    {
        if (pThis->StatIoDepth > 0)
        {
            ASMAtomicWriteBool(&pThis->fDummySignal, true);
        }
        return;
    }

    PDMDrvHlpSetAsyncNotification(pDrvIns, pfnAsyncNotify);
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

    if (pThis->pDrvBlockAsync)
    {
        if (pThis->StatIoDepth > 0)
            return false;
        else
            return true;
    }
    else
    {
        if (!drvscsiAsyncIOLoopNoPendingDummy(pThis, 0 /*ms*/))
            return false;
        ASMAtomicWriteBool(&pThis->fDummySignal, false);
        PDMR3ThreadSuspend(pThis->pAsyncIOThread);
        return true;
    }
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

    if (pThis->pDrvBlockAsync)
    {
        if (pThis->StatIoDepth > 0)
            return false;
        else
            return true;
    }
    else
    {
        if (!drvscsiAsyncIOLoopNoPendingDummy(pThis, 0 /*ms*/))
            return false;
        ASMAtomicWriteBool(&pThis->fDummySignal, false);
        return true;
    }
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

    if (pThis->pQueueRequests)
    {
        if (!drvscsiAsyncIOLoopNoPendingDummy(pThis, 100 /*ms*/))
            LogRel(("drvscsiDestruct#%u: previous dummy request is still pending\n", pDrvIns->iInstance));

        int rc = RTReqDestroyQueue(pThis->pQueueRequests);
        AssertMsgRC(rc, ("Failed to destroy queue rc=%Rrc\n", rc));
    }

    /* Free the VSCSI device and LUN handle. */
    VSCSILUN hVScsiLun;
    int rc = VSCSIDeviceLunDetach(pThis->hVScsiDevice, 0, &hVScsiLun);
    AssertRC(rc);

    Assert(hVScsiLun == pThis->hVScsiLun);
    rc = VSCSILunDestroy(hVScsiLun);
    AssertRC(rc);
    rc = VSCSIDeviceDestroy(pThis->hVScsiDevice);
    AssertRC(rc);
}

/**
 * Construct a block driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvscsiConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);
    LogFlowFunc(("pDrvIns=%#p pCfg=%#p\n", pDrvIns, pCfg));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Initialize the instance data.
     */
    pThis->pDrvIns                           = pDrvIns;
    pThis->ISCSIConnector.pfnSCSIRequestSend = drvscsiRequestSend;

    pDrvIns->IBase.pfnQueryInterface         = drvscsiQueryInterface;

    pThis->IPortAsync.pfnTransferCompleteNotify = drvscsiTransferCompleteNotify;

    /*
     * Try attach driver below and query it's block interface.
     */
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pThis->pDrvBase);
    AssertMsgReturn(RT_SUCCESS(rc), ("Attaching driver below failed rc=%Rrc\n", rc), rc);

    /*
     * Query the block and blockbios interfaces.
     */
    pThis->pDrvBlock = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBLOCK);
    if (!pThis->pDrvBlock)
    {
        AssertMsgFailed(("Configuration error: No block interface!\n"));
        return VERR_PDM_MISSING_INTERFACE;
    }
    pThis->pDrvBlockBios = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBLOCKBIOS);
    if (!pThis->pDrvBlockBios)
    {
        AssertMsgFailed(("Configuration error: No block BIOS interface!\n"));
        return VERR_PDM_MISSING_INTERFACE;
    }

    /* Query the SCSI port interface above. */
    pThis->pDevScsiPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMISCSIPORT);
    AssertMsgReturn(pThis->pDevScsiPort, ("Missing SCSI port interface above\n"), VERR_PDM_MISSING_INTERFACE);

    pThis->pDrvMount = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIMOUNT);

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

    /* Try to get the optional async block interface. */
    pThis->pDrvBlockAsync = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIBLOCKASYNC);

    PDMBLOCKTYPE enmType = pThis->pDrvBlock->pfnGetType(pThis->pDrvBlock);
    if (enmType != PDMBLOCKTYPE_HARD_DISK)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_UNSUPPORTED_BLOCK_TYPE, RT_SRC_POS,
                                   N_("Only hard disks are currently supported as SCSI devices (enmType=%d)"),
                                   enmType);

    /* Create VSCSI device and LUN. */
    pThis->VScsiIoCallbacks.pfnVScsiLunMediumGetSize      = drvscsiGetSize;
    pThis->VScsiIoCallbacks.pfnVScsiLunReqTransferEnqueue = drvscsiReqTransferEnqueue;

    rc = VSCSIDeviceCreate(&pThis->hVScsiDevice, drvscsiVScsiReqCompleted, pThis);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create VSCSI device rc=%Rrc\n"), rc);
    rc = VSCSILunCreate(&pThis->hVScsiLun, VSCSILUNTYPE_SBC, &pThis->VScsiIoCallbacks,
                        pThis);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create VSCSI LUN rc=%Rrc\n"), rc);
    rc = VSCSIDeviceLunAttach(pThis->hVScsiDevice, pThis->hVScsiLun, 0);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to attached the LUN to the SCSI device\n"), rc);

    /* Register statistics counter. */
    /** @todo aeichner: Find a way to put the instance number of the attached
     * controller device when we support more than one controller of the same type.
     * At the moment we have the 0 hardcoded. */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesRead, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                            "Amount of data read.", "/Devices/SCSI0/%d/ReadBytes", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                            "Amount of data written.", "/Devices/SCSI0/%d/WrittenBytes", pDrvIns->iInstance);

    pThis->StatIoDepth = 0;

    PDMDrvHlpSTAMRegisterF(pDrvIns, (void *)&pThis->StatIoDepth, STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                            "Number of active tasks.", "/Devices/SCSI0/%d/IoDepth", pDrvIns->iInstance);

    if (!pThis->pDrvBlockAsync)
    {
        /* Create request queue. */
        rc = RTReqCreateQueue(&pThis->pQueueRequests);
        AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create request queue rc=%Rrc\n"), rc);
        /* Create I/O thread. */
        rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pAsyncIOThread, pThis, drvscsiAsyncIOLoop,
                                   drvscsiAsyncIOLoopWakeup, 0, RTTHREADTYPE_IO, "SCSI async IO");
        AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create async I/O thread rc=%Rrc\n"), rc);
    }

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
    ~0,
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

