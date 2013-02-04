/* $Id$ */
/** @file
 * VBox storage drivers: Host SCSI access driver.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_SCSIHOST
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmifs.h>
#include <VBox/vmm/pdmthread.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/req.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#if defined(RT_OS_LINUX)
# include <limits.h>
# include <scsi/sg.h>
# include <sys/ioctl.h>
#endif

#include "VBoxDD.h"

/**
 * SCSI driver instance data.
 *
 * @implements  PDMISCSICONNECTOR
 */
typedef struct DRVSCSIHOST
{
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;

    /** Pointer to the SCSI port interface of the device above. */
    PPDMISCSIPORT           pDevScsiPort;
    /** The SCSI connector interface .   */
    PDMISCSICONNECTOR       ISCSIConnector;

    /** Path to the device file. */
    char                   *pszDevicePath;
    /** Handle to the device. */
    RTFILE                  hDeviceFile;

    /** The dedicated I/O thread. */
    PPDMTHREAD              pAsyncIOThread;
    /** Queue for passing the requests to the thread. */
    RTREQQUEUE              hQueueRequests;
} DRVSCSIHOST, *PDRVSCSIHOST;

/** Converts a pointer to DRVSCSIHOST::ISCSIConnecotr to a PDRVSCSIHOST. */
#define PDMISCSICONNECTOR_2_DRVSCSIHOST(pInterface) ( (PDRVSCSIHOST)((uintptr_t)pInterface - RT_OFFSETOF(DRVSCSIHOST, ISCSIConnector)) )

#ifdef DEBUG
/**
 * Dumps a SCSI request structure for debugging purposes.
 *
 * @returns nothing.
 * @param   pRequest    Pointer to the request to dump.
 */
static void drvscsihostDumpScsiRequest(PPDMSCSIREQUEST pRequest)
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

/**
 * Copy the content of a buffer to a scatter gather list
 * copying only the amount of data which fits into the
 * scatter gather list.
 *
 * @returns VBox status code.
 * @param   pRequest    Pointer to the request which contains the S/G list entries.
 * @param   pvBuf       Pointer to the buffer which should be copied.
 * @param   cbBuf       Size of the buffer.
 */
static int drvscsihostScatterGatherListCopyFromBuffer(PPDMSCSIREQUEST pRequest, void *pvBuf, size_t cbBuf)
{
    unsigned cSGEntry = 0;
    PRTSGSEG pSGEntry = &pRequest->paScatterGatherHead[cSGEntry];
    uint8_t *pu8Buf = (uint8_t *)pvBuf;

    while (cSGEntry < pRequest->cScatterGatherEntries)
    {
        size_t cbToCopy = (cbBuf < pSGEntry->cbSeg) ? cbBuf : pSGEntry->cbSeg;

        memcpy(pSGEntry->pvSeg, pu8Buf, cbToCopy);

        cbBuf -= cbToCopy;
        /* We finished. */
        if (!cbBuf)
            break;

        /* Advance the buffer. */
        pu8Buf += cbToCopy;

        /* Go to the next entry in the list. */
        pSGEntry++;
        cSGEntry++;
    }

    return VINF_SUCCESS;
}

/**
 * Set the sense and advanced sense key in the buffer for error conditions.
 *
 * @returns nothing.
 * @param   pRequest      Pointer to the request which contains the sense buffer.
 * @param   uSCSISenseKey The sense key to set.
 * @param   uSCSIASC      The advanced sense key to set.
 */
DECLINLINE(void) drvscsiCmdError(PPDMSCSIREQUEST pRequest, uint8_t uSCSISenseKey, uint8_t uSCSIASC)
{
    AssertMsg(pRequest->cbSenseBuffer >= 2, ("Sense buffer is not big enough\n"));
    AssertMsg(pRequest->pbSenseBuffer, ("Sense buffer pointer is NULL\n"));
    pRequest->pbSenseBuffer[0] = uSCSISenseKey;
    pRequest->pbSenseBuffer[1] = uSCSIASC;
}

/**
 * Sets the sense key for a status good condition.
 *
 * @returns nothing.
 * @param   pRequest    Pointer to the request which contains the sense buffer.
 */
DECLINLINE(void) drvscsihostCmdOk(PPDMSCSIREQUEST pRequest)
{
    AssertMsg(pRequest->cbSenseBuffer >= 2, ("Sense buffer is not big enough\n"));
    AssertMsg(pRequest->pbSenseBuffer, ("Sense buffer pointer is NULL\n"));
    pRequest->pbSenseBuffer[0] = SCSI_SENSE_NONE;
    pRequest->pbSenseBuffer[1] = SCSI_ASC_NONE;
}

/**
 * Returns the transfer direction of the given command
 * in case the device does not provide this info.
 *
 * @returns transfer direction of the command.
 *          SCSIHOSTTXDIR_NONE if no data is transferred.
 *          SCSIHOSTTXDIR_FROM_DEVICE if the data is read from the device.
 *          SCSIHOSTTXDIR_TO_DEVICE   if the data is written to the device.
 * @param   uCommand The command byte.
 */
static unsigned drvscsihostGetTransferDirectionFromCommand(uint8_t uCommand)
{
    switch (uCommand)
    {
        case SCSI_INQUIRY:
        case SCSI_REPORT_LUNS:
        case SCSI_MODE_SENSE_6:
        case SCSI_READ_TOC_PMA_ATIP:
        case SCSI_READ_CAPACITY:
        case SCSI_MODE_SENSE_10:
        case SCSI_GET_EVENT_STATUS_NOTIFICATION:
        case SCSI_GET_CONFIGURATION:
        case SCSI_READ_10:
        case SCSI_READ_12:
        case SCSI_READ_BUFFER:
        case SCSI_READ_BUFFER_CAPACITY:
        case SCSI_READ_DISC_INFORMATION:
        case SCSI_READ_DVD_STRUCTURE:
        case SCSI_READ_FORMAT_CAPACITIES:
        case SCSI_READ_SUBCHANNEL:
        case SCSI_READ_TRACK_INFORMATION:
        case SCSI_READ_CD:
        case SCSI_READ_CD_MSF:
            return PDMSCSIREQUESTTXDIR_FROM_DEVICE;
        case SCSI_TEST_UNIT_READY:
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
        case SCSI_START_STOP_UNIT:
            return PDMSCSIREQUESTTXDIR_NONE;
        case SCSI_WRITE_10:
        case SCSI_WRITE_12:
        case SCSI_WRITE_BUFFER:
            return PDMSCSIREQUESTTXDIR_TO_DEVICE;
        default:
            AssertMsgFailed(("Command not known %#x\n", uCommand));
    }

    /* We should never get here in debug mode. */
    AssertMsgFailed(("Impossible to get here!!!\n"));
    return PDMSCSIREQUESTTXDIR_NONE; /* to make compilers happy. */
}

static int drvscsihostProcessRequestOne(PDRVSCSIHOST pThis, PPDMSCSIREQUEST pRequest)
{
    int rc = VINF_SUCCESS;
    unsigned uTxDir;

    LogFlowFunc(("Entered\n"));

#ifdef DEBUG
    drvscsihostDumpScsiRequest(pRequest);
#endif

    /* We implement only one device. */
    if (pRequest->uLogicalUnit != 0)
    {
        switch (pRequest->pbCDB[0])
        {
            case SCSI_INQUIRY:
            {
                SCSIINQUIRYDATA ScsiInquiryReply;

                memset(&ScsiInquiryReply, 0, sizeof(ScsiInquiryReply));

                ScsiInquiryReply.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_UNKNOWN;
                ScsiInquiryReply.u3PeripheralQualifier = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_NOT_CONNECTED_NOT_SUPPORTED;
                drvscsihostScatterGatherListCopyFromBuffer(pRequest, &ScsiInquiryReply, sizeof(SCSIINQUIRYDATA));
                drvscsihostCmdOk(pRequest);
                break;
            }
            default:
                AssertMsgFailed(("Command not implemented for attached device\n"));
                drvscsiCmdError(pRequest, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_NONE);
        }
    }
    else
    {
#if defined(RT_OS_LINUX)
        sg_io_hdr_t ScsiIoReq;
        sg_iovec_t  *paSG = NULL;

        /* Setup SCSI request. */
        memset(&ScsiIoReq, 0, sizeof(sg_io_hdr_t));
        ScsiIoReq.interface_id = 'S';

        if (pRequest->uDataDirection == PDMSCSIREQUESTTXDIR_UNKNOWN)
            uTxDir = drvscsihostGetTransferDirectionFromCommand(pRequest->pbCDB[0]);
        else
            uTxDir = pRequest->uDataDirection;

        if (uTxDir == PDMSCSIREQUESTTXDIR_NONE)
            ScsiIoReq.dxfer_direction = SG_DXFER_NONE;
        else if (uTxDir == PDMSCSIREQUESTTXDIR_TO_DEVICE)
            ScsiIoReq.dxfer_direction = SG_DXFER_TO_DEV;
        else if (uTxDir == PDMSCSIREQUESTTXDIR_FROM_DEVICE)
            ScsiIoReq.dxfer_direction = SG_DXFER_FROM_DEV;
        else
            AssertMsgFailed(("Invalid transfer direction %u\n", uTxDir));

        ScsiIoReq.cmd_len     = pRequest->cbCDB;
        ScsiIoReq.mx_sb_len   = pRequest->cbSenseBuffer;
        ScsiIoReq.dxfer_len   = pRequest->cbScatterGather;

        if (pRequest->cScatterGatherEntries > 0)
        {
            if (pRequest->cScatterGatherEntries == 1)
            {
                ScsiIoReq.iovec_count = 0;
                ScsiIoReq.dxferp      = pRequest->paScatterGatherHead[0].pvSeg;
            }
            else
            {
                ScsiIoReq.iovec_count = pRequest->cScatterGatherEntries;

                paSG = (sg_iovec_t *)RTMemAllocZ(pRequest->cScatterGatherEntries * sizeof(sg_iovec_t));
                AssertPtrReturn(paSG, VERR_NO_MEMORY);

                for (unsigned i = 0; i < pRequest->cScatterGatherEntries; i++)
                {
                    paSG[i].iov_base = pRequest->paScatterGatherHead[i].pvSeg;
                    paSG[i].iov_len  = pRequest->paScatterGatherHead[i].cbSeg;
                }
                ScsiIoReq.dxferp = paSG;
            }
        }

        ScsiIoReq.cmdp    = pRequest->pbCDB;
        ScsiIoReq.sbp     = pRequest->pbSenseBuffer;
        ScsiIoReq.timeout = UINT_MAX;
        ScsiIoReq.flags  |= SG_FLAG_DIRECT_IO;

        /* Issue command. */
        rc = ioctl(RTFileToNative(pThis->hDeviceFile), SG_IO, &ScsiIoReq);
        if (rc < 0)
        {
            AssertMsgFailed(("Ioctl failed with rc=%d\n", rc));
        }

        /* Request processed successfully. */
        Log(("Command successfully processed\n"));
        if (ScsiIoReq.iovec_count > 0)
            RTMemFree(paSG);
#endif
    }
    /* Notify device that request finished. */
    rc = pThis->pDevScsiPort->pfnSCSIRequestCompleted(pThis->pDevScsiPort, pRequest, SCSI_STATUS_OK, false, VINF_SUCCESS);
    AssertMsgRC(rc, ("Notifying device above failed rc=%Rrc\n", rc));

    return rc;

}

/**
 * Request function to wakeup the thread.
 *
 * @returns VWRN_STATE_CHANGED.
 */
static int drvscsihostAsyncIOLoopWakeupFunc(void)
{
    return VWRN_STATE_CHANGED;
}

/**
 * The thread function which processes the requests asynchronously.
 *
 * @returns VBox status code.
 * @param   pDrvIns    Pointer to the device instance data.
 * @param   pThread    Pointer to the thread instance data.
 */
static int drvscsihostAsyncIOLoop(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    int rc = VINF_SUCCESS;
    PDRVSCSIHOST pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSIHOST);

    LogFlowFunc(("Entering async IO loop.\n"));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        rc = RTReqQueueProcess(pThis->hQueueRequests, RT_INDEFINITE_WAIT);
        AssertMsg(rc == VWRN_STATE_CHANGED, ("Left RTReqProcess and error code is not VWRN_STATE_CHANGED rc=%Rrc\n", rc));
    }

    return VINF_SUCCESS;
}

static int drvscsihostAsyncIOLoopWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    int rc;
    PDRVSCSIHOST pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSIHOST);
    PRTREQ pReq;

    AssertReturn(pThis->hQueueRequests != NIL_RTREQQUEUE, VERR_INVALID_STATE);

    rc = RTReqQueueCall(pThis->hQueueRequests, &pReq, 10000 /* 10 sec. */, (PFNRT)drvscsihostAsyncIOLoopWakeupFunc, 0);
    AssertMsgRC(rc, ("Inserting request into queue failed rc=%Rrc\n"));

    return rc;
}

/* -=-=-=-=- ISCSIConnector -=-=-=-=- */

/** @copydoc PDMISCSICONNECTOR::pfnSCSIRequestSend. */
static DECLCALLBACK(int) drvscsihostRequestSend(PPDMISCSICONNECTOR pInterface, PPDMSCSIREQUEST pSCSIRequest)
{
    int rc;
    PDRVSCSIHOST pThis = PDMISCSICONNECTOR_2_DRVSCSIHOST(pInterface);
    PRTREQ pReq;

    AssertReturn(pThis->hQueueRequests != NIL_RTREQQUEUE, VERR_INVALID_STATE);

    rc = RTReqQueueCallEx(pThis->hQueueRequests, &pReq, 0, RTREQFLAGS_NO_WAIT, (PFNRT)drvscsihostProcessRequestOne, 2, pThis, pSCSIRequest);
    AssertMsgReturn(RT_SUCCESS(rc), ("Inserting request into queue failed rc=%Rrc\n", rc), rc);

    return VINF_SUCCESS;
}

/* -=-=-=-=- PDMIBASE -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvscsihostQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS   pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVSCSIHOST pThis   = PDMINS_2_DATA(pDrvIns, PDRVSCSIHOST);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISCSICONNECTOR, &pThis->ISCSIConnector);
    return NULL;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvscsihostDestruct(PPDMDRVINS pDrvIns)
{
    PDRVSCSIHOST pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSIHOST);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    RTFileClose(pThis->hDeviceFile);
    pThis->hDeviceFile = NIL_RTFILE;

    if (pThis->pszDevicePath)
    {
        MMR3HeapFree(pThis->pszDevicePath);
        pThis->pszDevicePath = NULL;
    }

    if (pThis->hQueueRequests != NIL_RTREQQUEUE)
    {
        int rc = RTReqQueueDestroy(pThis->hQueueRequests);
        AssertMsgRC(rc, ("Failed to destroy queue rc=%Rrc\n", rc));
        pThis->hQueueRequests = NIL_RTREQQUEUE;
    }

}

/**
 * Construct a block driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvscsihostConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVSCSIHOST pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSIHOST);
    LogFlowFunc(("pDrvIns=%#p pCfg=%#p\n", pDrvIns, pCfg));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Initialize the instance data first because of the destructor.
     */
    pDrvIns->IBase.pfnQueryInterface            = drvscsihostQueryInterface;
    pThis->ISCSIConnector.pfnSCSIRequestSend    = drvscsihostRequestSend;
    pThis->pDrvIns                              = pDrvIns;
    pThis->hDeviceFile                          = NIL_RTFILE;
    pThis->hQueueRequests                       = NIL_RTREQQUEUE;

    /*
     * Read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "DevicePath\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuration for host scsi access driver"));


    /* Query the SCSI port interface above. */
    pThis->pDevScsiPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMISCSIPORT);
    AssertMsgReturn(pThis->pDevScsiPort, ("Missing SCSI port interface above\n"), VERR_PDM_MISSING_INTERFACE);

    /* Create request queue. */
    int rc = RTReqQueueCreate(&pThis->hQueueRequests);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create request queue rc=%Rrc\n"), rc);

    /* Open the device. */
    rc = CFGMR3QueryStringAlloc(pCfg, "DevicePath", &pThis->pszDevicePath);
    if (RT_FAILURE(rc))
        return PDMDRV_SET_ERROR(pDrvIns, rc,
                                N_("Configuration error: Failed to get the \"DevicePath\" value"));

    rc = RTFileOpen(&pThis->hDeviceFile, pThis->pszDevicePath, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvSCSIHost#%d: Failed to open device '%s'"), pDrvIns->iInstance, pThis->pszDevicePath);

    /* Create I/O thread. */
    rc = PDMDrvHlpThreadCreate(pDrvIns, &pThis->pAsyncIOThread, pThis, drvscsihostAsyncIOLoop,
                               drvscsihostAsyncIOLoopWakeup, 0, RTTHREADTYPE_IO, "SCSI async IO");
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create async I/O thread rc=%Rrc\n"), rc);

    return VINF_SUCCESS;
}

/**
 * SCSI driver registration record.
 */
const PDMDRVREG g_DrvSCSIHost =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "SCSIHost",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Host SCSI driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_SCSI,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVSCSIHOST),
    /* pfnConstruct */
    drvscsihostConstruct,
    /* pfnDestruct */
    drvscsihostDestruct,
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

