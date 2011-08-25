/* $Id$ */
/** @file
 * Virtual SCSI driver: SBC LUN implementation (hard disks)
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
#define LOG_GROUP LOG_GROUP_VSCSI
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/types.h>
#include <VBox/vscsi.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include "VSCSIInternal.h"

/**
 * SBC LUN instance
 */
typedef struct VSCSILUNSBC
{
    /** Core LUN structure */
    VSCSILUNINT    Core;
    /** Size of the virtual disk. */
    uint64_t       cSectors;
} VSCSILUNSBC;
/** Pointer to a SBC LUN instance */
typedef VSCSILUNSBC *PVSCSILUNSBC;

static int vscsiLunSbcInit(PVSCSILUNINT pVScsiLun)
{
    PVSCSILUNSBC pVScsiLunSbc = (PVSCSILUNSBC)pVScsiLun;
    uint64_t cbDisk = 0;
    int rc = VINF_SUCCESS;

    rc = vscsiLunMediumGetSize(pVScsiLun, &cbDisk);
    if (RT_SUCCESS(rc))
        pVScsiLunSbc->cSectors = cbDisk / 512; /* Fixed sector size */

    return rc;
}

static int vscsiLunSbcDestroy(PVSCSILUNINT pVScsiLun)
{
    PVSCSILUNSBC pVScsiLunSbc = (PVSCSILUNSBC)pVScsiLun;

    return VINF_SUCCESS;
}

static int vscsiLunSbcReqProcess(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq)
{
    PVSCSILUNSBC pVScsiLunSbc = (PVSCSILUNSBC)pVScsiLun;
    int rc = VINF_SUCCESS;
    int rcReq = SCSI_STATUS_OK;
    uint64_t uLbaStart = 0;
    uint32_t cSectorTransfer = 0;
    VSCSIIOREQTXDIR enmTxDir = VSCSIIOREQTXDIR_INVALID;

    switch(pVScsiReq->pbCDB[0])
    {
        case SCSI_INQUIRY:
        {
            SCSIINQUIRYDATA ScsiInquiryReply;

            memset(&ScsiInquiryReply, 0, sizeof(ScsiInquiryReply));

            ScsiInquiryReply.cbAdditional           = 31;
            ScsiInquiryReply.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS;
            ScsiInquiryReply.u3PeripheralQualifier  = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_CONNECTED;
            ScsiInquiryReply.u3AnsiVersion          = 0x05; /* SPC-4 compliant */
            ScsiInquiryReply.fCmdQue                = 1;    /* Command queuing supported. */
            ScsiInquiryReply.fWBus16                = 1;
            vscsiPadStr(ScsiInquiryReply.achVendorId, "VBOX", 8);
            vscsiPadStr(ScsiInquiryReply.achProductId, "HARDDISK", 16);
            vscsiPadStr(ScsiInquiryReply.achProductLevel, "1.0", 4);

            vscsiCopyToIoMemCtx(&pVScsiReq->IoMemCtx, (uint8_t *)&ScsiInquiryReply, sizeof(SCSIINQUIRYDATA));
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            break;
        }
        case SCSI_READ_CAPACITY:
        {
            uint8_t aReply[8];
            memset(aReply, 0, sizeof(aReply));

            /*
             * If sector size exceeds the maximum value that is
             * able to be stored in 4 bytes return 0xffffffff in this field
             */
            if (pVScsiLunSbc->cSectors > UINT32_C(0xffffffff))
                vscsiH2BEU32(aReply, UINT32_C(0xffffffff));
            else
                vscsiH2BEU32(aReply, pVScsiLunSbc->cSectors - 1);
            vscsiH2BEU32(&aReply[4], 512);
            vscsiCopyToIoMemCtx(&pVScsiReq->IoMemCtx, aReply, sizeof(aReply));
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            break;
        }
        case SCSI_MODE_SENSE_6:
        {
            uint8_t uModePage = pVScsiReq->pbCDB[2] & 0x3f;
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

            vscsiCopyToIoMemCtx(&pVScsiReq->IoMemCtx, aReply, sizeof(aReply));
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            break;
        }
        case SCSI_READ_6:
        {
            enmTxDir       = VSCSIIOREQTXDIR_READ;
            uLbaStart      = ((uint64_t)    pVScsiReq->pbCDB[3]
                                        |  (pVScsiReq->pbCDB[2] <<  8)
                                        | ((pVScsiReq->pbCDB[1] & 0x1f) << 16));
            cSectorTransfer = pVScsiReq->pbCDB[4];
            break;
        }
        case SCSI_READ_10:
        {
            enmTxDir        = VSCSIIOREQTXDIR_READ;
            uLbaStart       = vscsiBE2HU32(&pVScsiReq->pbCDB[2]);
            cSectorTransfer = vscsiBE2HU16(&pVScsiReq->pbCDB[7]);
            break;
        }
        case SCSI_READ_12:
        {
            enmTxDir        = VSCSIIOREQTXDIR_READ;
            uLbaStart       = vscsiBE2HU32(&pVScsiReq->pbCDB[2]);
            cSectorTransfer = vscsiBE2HU32(&pVScsiReq->pbCDB[6]);
            break;
        }
        case SCSI_READ_16:
        {
            enmTxDir        = VSCSIIOREQTXDIR_READ;
            uLbaStart       = vscsiBE2HU64(&pVScsiReq->pbCDB[2]);
            cSectorTransfer = vscsiBE2HU32(&pVScsiReq->pbCDB[10]);
            break;
        }
        case SCSI_WRITE_6:
        {
            enmTxDir        = VSCSIIOREQTXDIR_WRITE;
            uLbaStart       = ((uint64_t)  pVScsiReq->pbCDB[3]
                                        | (pVScsiReq->pbCDB[2] <<  8)
                                        | ((pVScsiReq->pbCDB[1] & 0x1f) << 16));
            cSectorTransfer = pVScsiReq->pbCDB[4];
            break;
        }
        case SCSI_WRITE_10:
        {
            enmTxDir        = VSCSIIOREQTXDIR_WRITE;
            uLbaStart       = vscsiBE2HU32(&pVScsiReq->pbCDB[2]);
            cSectorTransfer = vscsiBE2HU16(&pVScsiReq->pbCDB[7]);
            break;
        }
        case SCSI_WRITE_12:
        {
            enmTxDir        = VSCSIIOREQTXDIR_WRITE;
            uLbaStart       = vscsiBE2HU32(&pVScsiReq->pbCDB[2]);
            cSectorTransfer = vscsiBE2HU32(&pVScsiReq->pbCDB[6]);
            break;
        }
        case SCSI_WRITE_16:
        {
            enmTxDir        = VSCSIIOREQTXDIR_WRITE;
            uLbaStart       = vscsiBE2HU64(&pVScsiReq->pbCDB[2]);
            cSectorTransfer = vscsiBE2HU32(&pVScsiReq->pbCDB[10]);
            break;
        }
        case SCSI_SYNCHRONIZE_CACHE:
        {
            break; /* Handled below */
        }
        case SCSI_READ_BUFFER:
        {
            uint8_t uDataMode = pVScsiReq->pbCDB[1] & 0x1f;

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

                    vscsiCopyToIoMemCtx(&pVScsiReq->IoMemCtx, aReply, sizeof(aReply));
                    rcReq =  vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
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
        case SCSI_VERIFY_10:
        case SCSI_START_STOP_UNIT:
        {
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            break;
        }
        case SCSI_LOG_SENSE:
        {
            uint16_t cbMax = vscsiBE2HU16(&pVScsiReq->pbCDB[7]);
            uint8_t uPageCode = pVScsiReq->pbCDB[2] & 0x3f;
            uint8_t uSubPageCode = pVScsiReq->pbCDB[3];

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
                        vscsiCopyToIoMemCtx(&pVScsiReq->IoMemCtx, aReply, sizeof(aReply));
                        rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
                        break;
                    }
                }
                default:
                    rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
            }
            break;
        }
        case SCSI_SERVICE_ACTION_IN_16:
        {
            switch (pVScsiReq->pbCDB[1] & 0x1f)
            {
                case SCSI_SVC_ACTION_IN_READ_CAPACITY_16:
                {
                    uint8_t aReply[32];

                    memset(aReply, 0, sizeof(aReply));
                    vscsiH2BEU64(aReply, pVScsiLunSbc->cSectors - 1);
                    vscsiH2BEU32(&aReply[8], 512);
                    /* Leave the rest 0 */

                    vscsiCopyToIoMemCtx(&pVScsiReq->IoMemCtx, aReply, sizeof(aReply));
                    rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
                    break;
                }
                default:
                    rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET); /* Don't know if this is correct */
            }
            break;
        }
        default:
            //AssertMsgFailed(("Command %#x [%s] not implemented\n", pRequest->pbCDB[0], SCSICmdText(pRequest->pbCDB[0])));
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
    }

    if (enmTxDir != VSCSIIOREQTXDIR_INVALID)
    {
        LogFlow(("%s: uLbaStart=%llu cSectorTransfer=%u\n",
                 __FUNCTION__, uLbaStart, cSectorTransfer));

        if (RT_UNLIKELY(uLbaStart + cSectorTransfer > pVScsiLunSbc->cSectors))
        {
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
            vscsiDeviceReqComplete(pVScsiLun->pVScsiDevice, pVScsiReq, rcReq, false, VINF_SUCCESS);
        }
        else
        {
            /* Enqueue new I/O request */
            rc = vscsiIoReqTransferEnqueue(pVScsiLun, pVScsiReq, enmTxDir,
                                           uLbaStart * 512, cSectorTransfer * 512);
        }
    }
    else if (pVScsiReq->pbCDB[0] ==  SCSI_SYNCHRONIZE_CACHE)
    {
        /* Enqueue flush */
        rc = vscsiIoReqFlushEnqueue(pVScsiLun, pVScsiReq);
    }
    else /* Request completed */
        vscsiDeviceReqComplete(pVScsiLun->pVScsiDevice, pVScsiReq, rcReq, false, VINF_SUCCESS);

    return rc;
}

VSCSILUNDESC g_VScsiLunTypeSbc =
{
    /** enmLunType */
    VSCSILUNTYPE_SBC,
    /** pcszDescName */
    "SBC",
    /** cbLun */
    sizeof(VSCSILUNSBC),
    /** pfnVScsiLunInit */
    vscsiLunSbcInit,
    /** pfnVScsiLunDestroy */
    vscsiLunSbcDestroy,
    /** pfnVScsiLunReqProcess */
    vscsiLunSbcReqProcess
};

