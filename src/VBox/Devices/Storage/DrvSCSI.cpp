/* $Id$ */
/** @file
 *
 * VBox storage drivers:
 * Generic SCSI command parser and execution driver
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DRV_SCSI
#include <VBox/pdmdrv.h>
#include <VBox/pdmifs.h>
#include <VBox/pdmthread.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/req.h>
#include <iprt/semaphore.h>

#include "Builtins.h"

/**
 * SCSI driver instance data.
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
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY         IMountNotify;
    /** The status LED state for this drive.
     *  used in case the device doesn't has a Led interface
     *  so we can use this to avoid if checks later on. */
    PDMLED                  Led;
    /** pointer to the Led to use. */
    PPDMLED                 pLed;

    /** Device type. */
    PDMBLOCKTYPE            enmType;
    /** BIOS PCHS Geometry. */
    PDMMEDIAGEOMETRY        PCHSGeometry;
    /** BIOS LCHS Geometry. */
    PDMMEDIAGEOMETRY        LCHSGeometry;
    /** Number of sectors this device has. */
    uint64_t                cSectors;

    /** The dedicated I/O thread for the non async approach. */
    PPDMTHREAD              pAsyncIOThread;
    /** Queue for passing the requests to the thread. */
    PRTREQQUEUE             pQueueRequests;
    /** Release statistics: number of bytes written. */
    STAMCOUNTER             StatBytesWritten;
    /** Release statistics: number of bytes read. */
    STAMCOUNTER             StatBytesRead;
} DRVSCSI, *PDRVSCSI;

/** Converts a pointer to DRVSCSI::ISCSIConnecotr to a PDRVSCSI. */
#define PDMISCSICONNECTOR_2_DRVSCSI(pInterface) ( (PDRVSCSI)((uintptr_t)pInterface - RT_OFFSETOF(DRVSCSI, ISCSIConnector)) )

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

/**
 * Copy the content of a buffer to a scatter gather list only
 * copying only the amount of data which fits into the
 * scatter gather list.
 *
 * @returns VBox status code.
 * @param   pRequest    Pointer to the request which contains the S/G list entries.
 * @param   pvBuf       Pointer to the buffer which should be copied.
 * @param   cbBuf       Size of the buffer.
 */
static int drvscsiScatterGatherListCopyFromBuffer(PPDMSCSIREQUEST pRequest, void *pvBuf, size_t cbBuf)
{
    unsigned cSGEntry = 0;
    PPDMDATASEG pSGEntry = &pRequest->paScatterGatherHead[cSGEntry];
    uint8_t *pu8Buf = (uint8_t *)pvBuf;

    LogFlowFunc(("pRequest=%#p pvBuf=%#p cbBuf=%u\n", pRequest, pvBuf, cbBuf));

#ifdef DEBUG
    for (unsigned i = 0; i < cbBuf; i++)
        Log(("%s: pvBuf[%u]=%#x\n", __FUNCTION__, i, pu8Buf[i]));
#endif

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

static void drvscsiPadStr(int8_t *pbDst, const char *pbSrc, uint32_t cbSize)
{
    for (uint32_t i = 0; i < cbSize; i++)
    {
        if (*pbSrc)
            pbDst[i] = *pbSrc++;
        else
            pbDst[i] = ' ';
    }
}

/**
 * Set the sense and advanced sense key in the buffer for error conditions.
 *
 * @returns SCSI status code.
 * @param   pRequest      Pointer to the request which contains the sense buffer.
 * @param   uSCSISenseKey The sense key to set.
 * @param   uSCSIASC      The advanced sense key to set.
 */
DECLINLINE(int) drvscsiCmdError(PPDMSCSIREQUEST pRequest, uint8_t uSCSISenseKey, uint8_t uSCSIASC)
{
    AssertMsgReturn(pRequest->cbSenseBuffer >= 18, ("Sense buffer is not big enough\n"), SCSI_STATUS_OK);
    AssertMsgReturn(pRequest->pbSenseBuffer, ("Sense buffer pointer is NULL\n"), SCSI_STATUS_OK);
    memset(pRequest->pbSenseBuffer, 0, pRequest->cbSenseBuffer);
    pRequest->pbSenseBuffer[0] = (1 << 7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED; /* Fixed format */
    pRequest->pbSenseBuffer[2] = uSCSISenseKey;
    pRequest->pbSenseBuffer[7]  = 10;
    pRequest->pbSenseBuffer[12] = uSCSIASC;
    pRequest->pbSenseBuffer[13] = 0x00; /** @todo: Provide more info. */
    return SCSI_STATUS_CHECK_CONDITION;
}

/**
 * Sets the sense key for a status good condition.
 *
 * @returns SCSI status code.
 * @param   pRequest    Pointer to the request which contains the sense buffer.
 */
DECLINLINE(int) drvscsiCmdOk(PPDMSCSIREQUEST pRequest)
{
    AssertMsgReturn(pRequest->cbSenseBuffer >= 18, ("Sense buffer is not big enough\n"), SCSI_STATUS_OK);
    AssertMsgReturn(pRequest->pbSenseBuffer, ("Sense buffer pointer is NULL\n"), SCSI_STATUS_OK);
    memset(pRequest->pbSenseBuffer, 0, pRequest->cbSenseBuffer);
    /*
     * Setting this breaks Linux guests on the BusLogic controller.
     * According to the SCSI SPC spec sense data is returned after a
     * CHECK CONDITION status or a REQUEST SENSE command.
     * Both SCSI controllers have a feature called Auto Sense which
     * fetches the sense data automatically from the device
     * with REQUEST SENSE. So the SCSI subsystem in Linux should
     * find this sense data even if the command finishes successfully
     * but if it finds valid sense data it will let the command fail
     * and it doesn't detect attached disks anymore.
     * Disabling makes it work again and no other guest shows errors
     * so I will leave it disabled for now.
     *
     * On the other hand it is possible that the devices fetch the sense data
     * only after a command failed so the content is really invalid if
     * the command succeeds.
     */
#if 0
    pRequest->pbSenseBuffer[0]  = (1 << 7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED; /* Fixed format */
    pRequest->pbSenseBuffer[2]  = SCSI_SENSE_NONE;
    pRequest->pbSenseBuffer[7]  = 10;
    pRequest->pbSenseBuffer[12] = SCSI_ASC_NONE;
    pRequest->pbSenseBuffer[13] = SCSI_ASC_NONE; /* Should be ASCQ but it has the same value for success. */
#endif
    return SCSI_STATUS_OK;
}

DECLINLINE(void) drvscsiH2BE_U16(uint8_t *pbBuf, uint16_t val)
{
    pbBuf[0] = val >> 8;
    pbBuf[1] = val;
}


DECLINLINE(void) drvscsiH2BE_U24(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 16;
    pbBuf[1] = val >> 8;
    pbBuf[2] = val;
}


DECLINLINE(void) drvscsiH2BE_U32(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 24;
    pbBuf[1] = val >> 16;
    pbBuf[2] = val >> 8;
    pbBuf[3] = val;
}

DECLINLINE(void) drvscsiH2BE_U64(uint8_t *pbBuf, uint64_t val)
{
    pbBuf[0] = val >> 56;
    pbBuf[1] = val >> 48;
    pbBuf[2] = val >> 40;
    pbBuf[3] = val >> 32;
    pbBuf[4] = val >> 24;
    pbBuf[5] = val >> 16;
    pbBuf[6] = val >> 8;
    pbBuf[7] = val;
}

DECLINLINE(uint16_t) drvscsiBE2H_U16(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 8) | pbBuf[1];
}


DECLINLINE(uint32_t) drvscsiBE2H_U24(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 16) | (pbBuf[1] << 8) | pbBuf[2];
}


DECLINLINE(uint32_t) drvscsiBE2H_U32(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 24) | (pbBuf[1] << 16) | (pbBuf[2] << 8) | pbBuf[3];
}

DECLINLINE(uint64_t) drvscsiBE2H_U64(const uint8_t *pbBuf)
{
    return   ((uint64_t)pbBuf[0] << 56)
           | ((uint64_t)pbBuf[1] << 48)
           | ((uint64_t)pbBuf[2] << 40)
           | ((uint64_t)pbBuf[3] << 32)
           | ((uint64_t)pbBuf[4] << 24)
           | ((uint64_t)pbBuf[5] << 16)
           | ((uint64_t)pbBuf[6] << 8)
           | (uint64_t)pbBuf[7];
}

/**
 * Parses the CDB of a request and acts accordingly.
 *
 * @returns transfer direction type.
 * @param   pThis    Pointer to the SCSI driver instance data.
 * @param   pRequest Pointer to the request to process.
 * @param   puOffset Where to store the start offset to start data transfer from.
 * @param   pcbToTransfer Where to store the number of bytes to transfer.
 * @param   piTxDir       Where to store the data transfer direction.
 */
static int drvscsiProcessCDB(PDRVSCSI pThis, PPDMSCSIREQUEST pRequest, uint64_t *puOffset, uint32_t *pcbToTransfer, int *piTxDir)
{
    int iTxDir = PDMBLOCKTXDIR_NONE;
    int rc = SCSI_STATUS_OK;

    /* We check for a command which needs to be handled even for non existant LUNs. */
    switch (pRequest->pbCDB[0])
    {
        case SCSI_INQUIRY:
        {
            SCSIINQUIRYDATA ScsiInquiryReply;

            memset(&ScsiInquiryReply, 0, sizeof(ScsiInquiryReply));

            ScsiInquiryReply.cbAdditional = 31;

            /* We support only one attached device at LUN0 at the moment. */
            if (pRequest->uLogicalUnit != 0)
            {
                ScsiInquiryReply.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_UNKNOWN;
                ScsiInquiryReply.u3PeripheralQualifier = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_NOT_CONNECTED_NOT_SUPPORTED;
            }
            else
            {
                switch (pThis->enmType)
                {
                    case PDMBLOCKTYPE_HARD_DISK:
                        ScsiInquiryReply.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS;
                        break;
                    default:
                        AssertMsgFailed(("Device type %u not supported\n", pThis->enmType));
                }

                ScsiInquiryReply.u3PeripheralQualifier = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_CONNECTED;
                ScsiInquiryReply.u3AnsiVersion = 0x05; /* SPC-4 compliant */
                drvscsiPadStr(ScsiInquiryReply.achVendorId, "VBOX", 8);
                drvscsiPadStr(ScsiInquiryReply.achProductId, "HARDDISK", 16);
                drvscsiPadStr(ScsiInquiryReply.achProductLevel, "1.0", 4);
            }

            drvscsiScatterGatherListCopyFromBuffer(pRequest, &ScsiInquiryReply, sizeof(SCSIINQUIRYDATA));
            rc = drvscsiCmdOk(pRequest);
            break;
        }
        case SCSI_REPORT_LUNS:
        {
            /*
             * If allocation length is less than 16 bytes SPC compliant devices have
             * to return an error.
             */
            if (drvscsiBE2H_U32(&pRequest->pbCDB[6]) < 16)
                rc = drvscsiCmdError(pRequest, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
            else
            {
                uint8_t aReply[16]; /* We report only one LUN. */

                memset(aReply, 0, sizeof(aReply));
                drvscsiH2BE_U32(&aReply[0], 8); /* List length starts at position 0. */
                drvscsiScatterGatherListCopyFromBuffer(pRequest, aReply, sizeof(aReply));
                rc = drvscsiCmdOk(pRequest);
            }
            break;
        }
        case SCSI_TEST_UNIT_READY:
        {
            rc = drvscsiCmdOk(pRequest);
            break;
        }
        default:
        {
            /* Now for commands which are only implemented for existant LUNs. */
            if (RT_LIKELY(pRequest->uLogicalUnit == 0))
            {
                switch(pRequest->pbCDB[0])
                {
                    case SCSI_READ_CAPACITY:
                    {
                        uint8_t aReply[8];
                        memset(aReply, 0, sizeof(aReply));

                        /*
                         * If sector size exceeds the maximum value that is
                         * able to be stored in 4 bytes return 0xffffffff in this field
                         */
                        if (pThis->cSectors > UINT32_C(0xffffffff))
                            drvscsiH2BE_U32(aReply, UINT32_C(0xffffffff));
                        else
                            drvscsiH2BE_U32(aReply, pThis->cSectors - 1);
                        drvscsiH2BE_U32(&aReply[4], 512);
                        drvscsiScatterGatherListCopyFromBuffer(pRequest, aReply, sizeof(aReply));
                        rc =  drvscsiCmdOk(pRequest);
                        break;
                    }
                    case SCSI_MODE_SENSE_6:
                    {
                        uint8_t uModePage = pRequest->pbCDB[2] & 0x3f;
                        uint8_t aReply[24];
                        uint8_t *pu8ReplyPos;

                        memset(aReply, 0, sizeof(aReply));
                        aReply[0] = 4; /* Reply length 4. */
                        aReply[1] = 0; /* Default media type. */
                        aReply[2] = RT_BIT(4); /* Caching supported. */
                        aReply[3] = 0; /* Block descriptor length. */

                        pu8ReplyPos = aReply + 4;

                        if ((uModePage == 0x08) || (uModePage == 0x3f))
                        {
                            memset(pu8ReplyPos, 0, 20);
                            *pu8ReplyPos++ = 0x08; /* Page code. */
                            *pu8ReplyPos++ = 0x12; /* Size of the page. */
                            *pu8ReplyPos++ = 0x4;  /* Write cache enabled. */
                        }

                        drvscsiScatterGatherListCopyFromBuffer(pRequest, aReply, sizeof(aReply));
                        rc =  drvscsiCmdOk(pRequest);
                        break;
                    }
                    case SCSI_READ_6:
                    {
                        iTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
                        *puOffset      = ((uint64_t)  pRequest->pbCDB[3]
                                                    | (pRequest->pbCDB[2] <<  8)
                                                    | ((pRequest->pbCDB[1] & 0x1f) << 16)) * 512;
                        *pcbToTransfer = ((uint32_t)pRequest->pbCDB[4]) * 512;
                        break;
                    }
                    case SCSI_READ_10:
                    {
                        iTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
                        *puOffset = ((uint64_t)drvscsiBE2H_U32(&pRequest->pbCDB[2])) * 512;
                        *pcbToTransfer = ((uint32_t)drvscsiBE2H_U16(&pRequest->pbCDB[7])) * 512;
                        break;
                    }
                    case SCSI_READ_12:
                    {
                        iTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
                        *puOffset = ((uint64_t)drvscsiBE2H_U32(&pRequest->pbCDB[2])) * 512;
                        *pcbToTransfer = ((uint32_t)drvscsiBE2H_U32(&pRequest->pbCDB[6])) * 512;
                        break;
                    }
                    case SCSI_READ_16:
                    {
                        iTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
                        *puOffset = drvscsiBE2H_U64(&pRequest->pbCDB[2]) * 512;
                        *pcbToTransfer = ((uint32_t)drvscsiBE2H_U32(&pRequest->pbCDB[10])) * 512;
                        break;
                    }
                    case SCSI_WRITE_6:
                    {
                        iTxDir = PDMBLOCKTXDIR_TO_DEVICE;
                        *puOffset      = ((uint64_t)  pRequest->pbCDB[3]
                                                    | (pRequest->pbCDB[2] <<  8)
                                                    | ((pRequest->pbCDB[1] & 0x1f) << 16)) * 512;
                        *pcbToTransfer = ((uint32_t)pRequest->pbCDB[4]) * 512;
                        break;
                    }
                    case SCSI_WRITE_10:
                    {
                        iTxDir = PDMBLOCKTXDIR_TO_DEVICE;
                        *puOffset = ((uint64_t)drvscsiBE2H_U32(&pRequest->pbCDB[2])) * 512;
                        *pcbToTransfer = ((uint32_t)drvscsiBE2H_U16(&pRequest->pbCDB[7])) * 512;
                        break;
                    }
                    case SCSI_WRITE_12:
                    {
                        iTxDir = PDMBLOCKTXDIR_TO_DEVICE;
                        *puOffset = ((uint64_t)drvscsiBE2H_U32(&pRequest->pbCDB[2])) * 512;
                        *pcbToTransfer = ((uint32_t)drvscsiBE2H_U32(&pRequest->pbCDB[6])) * 512;
                        break;
                    }
                    case SCSI_WRITE_16:
                    {
                        iTxDir = PDMBLOCKTXDIR_TO_DEVICE;
                        *puOffset = drvscsiBE2H_U64(&pRequest->pbCDB[2]) * 512;
                        *pcbToTransfer = ((uint32_t)drvscsiBE2H_U32(&pRequest->pbCDB[10])) * 512;
                        break;
                    }
                    case SCSI_SYNCHRONIZE_CACHE:
                    {
                        /* @todo When async mode implemented we have to move this out here. */
                        int rc2 = pThis->pDrvBlock->pfnFlush(pThis->pDrvBlock);
                        AssertMsgRC(rc2, ("Flushing data failed rc=%Rrc\n", rc2));
                        break;
                    }
                    case SCSI_READ_BUFFER:
                    {
                        uint8_t uDataMode = pRequest->pbCDB[1] & 0x1f;

                        switch (uDataMode)
                        {
                            case 0x00:
                            case 0x01:
                            case 0x02:
                            case 0x03:
                            case 0x0a:
                                break;
                            case 0x0b:
                            {
                                uint8_t aReply[4];

                                /* We do not implement an echo buffer. */
                                memset(aReply, 0, sizeof(aReply));

                                drvscsiScatterGatherListCopyFromBuffer(pRequest, aReply, sizeof(aReply));
                                rc =  drvscsiCmdOk(pRequest);
                                break;
                            }
                            case 0x1a:
                            case 0x1c:
                                break;
                            default:
                                AssertMsgFailed(("Invalid data mode\n"));
                        }
                        break;
                    }
                    case SCSI_START_STOP_UNIT:
                    {
                        /* Nothing to do. */
                        break;
                    }
                    case SCSI_LOG_SENSE:
                    {
                        uint16_t cbMax = drvscsiBE2H_U16(&pRequest->pbCDB[7]);
                        uint8_t uPageCode = pRequest->pbCDB[2] & 0x3f;
                        uint8_t uSubPageCode = pRequest->pbCDB[3];

                        switch (uPageCode)
                        {
                            case 0x00:
                            {
                                if (uSubPageCode == 0)
                                {
                                    uint8_t aReply[4];

                                    aReply[0] = 0;
                                    aReply[1] = 0;
                                    aReply[2] = 0;
                                    aReply[3] = 0;
                                    drvscsiScatterGatherListCopyFromBuffer(pRequest, aReply, sizeof(aReply));
                                    rc =  drvscsiCmdOk(pRequest);
                                    break;
                                }
                            }
                            default:
                                rc = drvscsiCmdError(pRequest, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                        }
                        break;
                    }
                    case SCSI_SERVICE_ACTION_IN_16:
                    {
                        switch (pRequest->pbCDB[1] & 0x1f)
                        {
                            case SCSI_SVC_ACTION_IN_READ_CAPACITY_16:
                            {
                                uint8_t aReply[32];

                                memset(aReply, 0, sizeof(aReply));
                                drvscsiH2BE_U64(aReply, pThis->cSectors - 1);
                                drvscsiH2BE_U32(&aReply[8], 512);
                                /* Leave the rest 0 */

                                drvscsiScatterGatherListCopyFromBuffer(pRequest, aReply, sizeof(aReply));
                                rc =  drvscsiCmdOk(pRequest);
                                break;
                            }
                            default:
                                rc = drvscsiCmdError(pRequest, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET); /* Don't know if this is correct */
                        }
                        break;
                    }
                    default:
                        //AssertMsgFailed(("Command %#x [%s] not implemented\n", pRequest->pbCDB[0], SCSICmdText(pRequest->pbCDB[0])));
                        rc = drvscsiCmdError(pRequest, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
                }
            }
            else
            {
                /* Report an error. */
                rc = drvscsiCmdError(pRequest, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_UNIT_DOES_NOT_RESPOND_TO_SELECTION);
            }
            break;
        }
    }

    *piTxDir = iTxDir;

    return rc;
}

static int drvscsiProcessRequestOne(PDRVSCSI pThis, PPDMSCSIREQUEST pRequest)
{
    int rc = VINF_SUCCESS;
    int iTxDir;
    int rcCompletion;
    uint64_t uOffset;
    uint32_t cbToTransfer;
    uint32_t cSegmentsLeft;

    LogFlowFunc(("Entered\n"));

#ifdef DEBUG
    drvscsiDumpScsiRequest(pRequest);
#endif
    rcCompletion = drvscsiProcessCDB(pThis, pRequest, &uOffset, &cbToTransfer, &iTxDir);
    if ((rcCompletion == SCSI_STATUS_OK) && (iTxDir != PDMBLOCKTXDIR_NONE))
    {
        PPDMDATASEG pSegActual;

        pSegActual = &pRequest->paScatterGatherHead[0];
        cSegmentsLeft = pRequest->cScatterGatherEntries;

        while(cbToTransfer && cSegmentsLeft)
        {
            uint32_t cbProcess = (cbToTransfer < pSegActual->cbSeg) ? cbToTransfer : (uint32_t)pSegActual->cbSeg;

            Log(("%s: uOffset=%llu cbToTransfer=%u\n", __FUNCTION__, uOffset, cbToTransfer));

            if (iTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
            {
                pThis->pLed->Asserted.s.fReading = pThis->pLed->Actual.s.fReading = 1;
                rc = pThis->pDrvBlock->pfnRead(pThis->pDrvBlock, uOffset,
                                                pSegActual->pvSeg, cbProcess);
                pThis->pLed->Actual.s.fReading = 0;
                if (RT_FAILURE(rc))
                    AssertMsgFailed(("%s: Failed to read data %Rrc\n", __FUNCTION__, rc));
                STAM_REL_COUNTER_ADD(&pThis->StatBytesRead, cbProcess);
            }
            else
            {
                pThis->pLed->Asserted.s.fWriting = pThis->pLed->Actual.s.fWriting = 1;
                rc = pThis->pDrvBlock->pfnWrite(pThis->pDrvBlock, uOffset,
                                                pSegActual->pvSeg, cbProcess);
                pThis->pLed->Actual.s.fWriting = 0;
                if (RT_FAILURE(rc))
                    AssertMsgFailed(("%s: Failed to write data %Rrc\n", __FUNCTION__, rc));
                STAM_REL_COUNTER_ADD(&pThis->StatBytesWritten, cbProcess);
            }

            /* Go to the next entry. */
            uOffset += cbProcess;
            cbToTransfer -= cbProcess;
            pSegActual++;
            cSegmentsLeft--;
        }
        AssertMsg(!cbToTransfer && !cSegmentsLeft,
                  ("Transfer incomplete cbToTransfer=%u cSegmentsLeft=%u\n", cbToTransfer, cSegmentsLeft));
        drvscsiCmdOk(pRequest);
    }

    /* Notify device. */
    rc = pThis->pDevScsiPort->pfnSCSIRequestCompleted(pThis->pDevScsiPort, pRequest, rcCompletion);
    AssertMsgRC(rc, ("Error while notifying device rc=%Rrc\n", rc));

    return rc;
}

/**
 * Request function to wakeup the thread.
 *
 * @returns VWRN_STATE_CHANGED.
 */
static int drvscsiAsyncIOLoopWakeupFunc(void)
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

static int drvscsiAsyncIOLoopWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    int rc;
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);
    PRTREQ pReq;

    AssertMsgReturn(pThis->pQueueRequests, ("pQueueRequests is NULL\n"), VERR_INVALID_STATE);

    rc = RTReqCall(pThis->pQueueRequests, &pReq, 10000 /* 10 sec. */, (PFNRT)drvscsiAsyncIOLoopWakeupFunc, 0);
    AssertMsgRC(rc, ("Inserting request into queue failed rc=%Rrc\n", rc));

    return rc;
}

/* -=-=-=-=- ISCSIConnector -=-=-=-=- */

/** @copydoc PDMISCSICONNECTOR::pfnSCSIRequestSend. */
static DECLCALLBACK(int) drvscsiRequestSend(PPDMISCSICONNECTOR pInterface, PPDMSCSIREQUEST pSCSIRequest)
{
    int rc;
    PDRVSCSI pThis = PDMISCSICONNECTOR_2_DRVSCSI(pInterface);
    PRTREQ pReq;

    AssertMsgReturn(pThis->pQueueRequests, ("pQueueRequests is NULL\n"), VERR_INVALID_STATE);

    rc = RTReqCallEx(pThis->pQueueRequests, &pReq, 0, RTREQFLAGS_NO_WAIT, (PFNRT)drvscsiProcessRequestOne, 2, pThis, pSCSIRequest);
    AssertMsgReturn(RT_SUCCESS(rc), ("Inserting request into queue failed rc=%Rrc\n", rc), rc);

    return VINF_SUCCESS;
}

/* -=-=-=-=- IBase -=-=-=-=- */

/** @copydoc PDMIBASE::pfnQueryInterface. */
static DECLCALLBACK(void *)  drvscsiQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVSCSI    pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_SCSI_CONNECTOR:
            return &pThis->ISCSIConnector;
        case PDMINTERFACE_BLOCK_PORT:
            return &pThis->IPort;
        default:
            return NULL;
    }
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
    int rc;
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    if (pThis->pQueueRequests)
    {
        rc = RTReqDestroyQueue(pThis->pQueueRequests);
        AssertMsgRC(rc, ("Failed to destroy queue rc=%Rrc\n", rc));
    }

}

/**
 * Construct a block driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvscsiConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVSCSI pThis = PDMINS_2_DATA(pDrvIns, PDRVSCSI);

    LogFlowFunc(("pDrvIns=%#p pCfgHandle=%#p\n", pDrvIns, pCfgHandle));

    /*
     * Initialize interfaces.
     */
    pDrvIns->IBase.pfnQueryInterface                    = drvscsiQueryInterface;
    pThis->ISCSIConnector.pfnSCSIRequestSend            = drvscsiRequestSend;

    /*
     * Try attach driver below and query it's block interface.
     */
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pThis->pDrvBase);
    AssertMsgReturn(RT_SUCCESS(rc), ("Attaching driver below failed rc=%Rrc\n", rc), rc);

    /*
     * Query the block and blockbios interfaces.
     */
    pThis->pDrvBlock = (PDMIBLOCK *)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_BLOCK);
    if (!pThis->pDrvBlock)
    {
        AssertMsgFailed(("Configuration error: No block interface!\n"));
        return VERR_PDM_MISSING_INTERFACE;
    }
    pThis->pDrvBlockBios = (PDMIBLOCKBIOS *)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_BLOCK_BIOS);
    if (!pThis->pDrvBlockBios)
    {
        AssertMsgFailed(("Configuration error: No block BIOS interface!\n"));
        return VERR_PDM_MISSING_INTERFACE;
    }

    /* Query the SCSI port interface above. */
    pThis->pDevScsiPort = (PPDMISCSIPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_SCSI_PORT);
    AssertMsgReturn(pThis->pDevScsiPort, ("Missing SCSI port interface above\n"), VERR_PDM_MISSING_INTERFACE);

    pThis->pDrvMount = (PDMIMOUNT *)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_MOUNT);

    /* Query the optional LED interface above. */
    pThis->pLedPort = (PPDMILEDPORTS)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_LED_PORTS);
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
    pThis->pDrvBlockAsync = (PDMIBLOCKASYNC *)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_BLOCK_ASYNC);

    PDMBLOCKTYPE enmType = pThis->pDrvBlock->pfnGetType(pThis->pDrvBlock);
    if (enmType != PDMBLOCKTYPE_HARD_DISK)
    {
        AssertMsgFailed(("Configuration error: Not a disk or cd/dvd-rom. enmType=%d\n", enmType));
        return VERR_PDM_UNSUPPORTED_BLOCK_TYPE;
    }
    pThis->enmType = enmType;
    pThis->cSectors = pThis->pDrvBlock->pfnGetSize(pThis->pDrvBlock) / 512;

    /* Create request queue. */
    rc = RTReqCreateQueue(&pThis->pQueueRequests);
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create request queue rc=%Rrc\n"), rc);

    /* Register statistics counter. */
    /** @odo r=aeichner: Find a way to put the instance number of the attached controller device
     * when we support more than one controller of the same type. At the moment we have the
     * 0 hardcoded. */
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesRead, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                            "Amount of data read.", "/Devices/SCSI0/%d/ReadBytes", pDrvIns->iInstance);
    PDMDrvHlpSTAMRegisterF(pDrvIns, &pThis->StatBytesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                            "Amount of data written.", "/Devices/SCSI0/%d/WrittenBytes", pDrvIns->iInstance);

    /* Create I/O thread. */
    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pAsyncIOThread, pThis, drvscsiAsyncIOLoop,
                                  drvscsiAsyncIOLoopWakeup, 0, RTTHREADTYPE_IO, "SCSI async IO");
    AssertMsgReturn(RT_SUCCESS(rc), ("Failed to create async I/O thread rc=%Rrc\n"), rc);

    return VINF_SUCCESS;
}

/**
 * SCSI driver registration record.
 */
const PDMDRVREG g_DrvSCSI =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "SCSI",
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
