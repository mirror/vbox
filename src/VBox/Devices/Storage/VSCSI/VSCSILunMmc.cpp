/* $Id$ */
/** @file
 * Virtual SCSI driver: MMC LUN implementation (CD/DVD-ROM)
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
 * MMC LUN instance
 */
typedef struct VSCSILUNMMC
{
    /** Core LUN structure */
    VSCSILUNINT     Core;
    /** Size of the virtual disk. */
    uint64_t        cSectors;
    /** Sector size. */
    uint32_t        cbSector;
    /** Medium locked indicator. */
    bool            fLocked;
} VSCSILUNMMC, *PVSCSILUNMMC;


DECLINLINE(void) mmcLBA2MSF(uint8_t *pbBuf, uint32_t iLBA)
{
    iLBA += 150;
    pbBuf[0] = (iLBA / 75) / 60;
    pbBuf[1] = (iLBA / 75) % 60;
    pbBuf[2] = iLBA % 75;
}

#if 0 /* unused */
DECLINLINE(uint32_t) mmcMSF2LBA(const uint8_t *pbBuf)
{
    return (pbBuf[0] * 60 + pbBuf[1]) * 75 + pbBuf[2];
}
#endif


/* Fabricate normal TOC information. */
static int mmcReadTOCNormal(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq, uint16_t cbMaxTransfer, bool fMSF)
{
    PVSCSILUNMMC    pVScsiLunMmc = (PVSCSILUNMMC)pVScsiLun;
    uint8_t         aReply[32];
    uint8_t         *pbBuf = aReply;
    uint8_t         *q;
    uint8_t         iStartTrack;
    uint32_t        cbSize;

    iStartTrack = pVScsiReq->pbCDB[6];
    if (iStartTrack > 1 && iStartTrack != 0xaa)
    {
        return vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET, 0x00);
    }
    q = pbBuf + 2;
    *q++ = 1;   /* first session */
    *q++ = 1;   /* last session */
    if (iStartTrack <= 1)
    {
        *q++ = 0;       /* reserved */
        *q++ = 0x14;    /* ADR, CONTROL */
        *q++ = 1;       /* track number */
        *q++ = 0;       /* reserved */
        if (fMSF)
        {
            *q++ = 0;   /* reserved */
            mmcLBA2MSF(q, 0);
            q += 3;
        }
        else
        {
            /* sector 0 */
            vscsiH2BEU32(q, 0);
            q += 4;
        }
    }
    /* lead out track */
    *q++ = 0;       /* reserved */
    *q++ = 0x14;    /* ADR, CONTROL */
    *q++ = 0xaa;    /* track number */
    *q++ = 0;       /* reserved */
    if (fMSF)
    {
        *q++ = 0;   /* reserved */
        mmcLBA2MSF(q, pVScsiLunMmc->cSectors);
        q += 3;
    }
    else
    {
        vscsiH2BEU32(q, pVScsiLunMmc->cSectors);
        q += 4;
    }
    cbSize = q - pbBuf;
    Assert(cbSize <= sizeof(aReply));
    vscsiH2BEU16(pbBuf, cbSize - 2);
    if (cbSize < cbMaxTransfer)
        cbMaxTransfer = cbSize;

    RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, cbMaxTransfer);

    return vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
}

/* Fabricate session information. */
static int mmcReadTOCMulti(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq, uint16_t cbMaxTransfer, bool fMSF)
{
    RT_NOREF1(cbMaxTransfer);
    uint8_t         aReply[32];
    uint8_t         *pbBuf = aReply;

    /* multi session: only a single session defined */
    memset(pbBuf, 0, 12);
    pbBuf[1] = 0x0a;
    pbBuf[2] = 0x01;    /* first complete session number */
    pbBuf[3] = 0x01;    /* last complete session number */
    pbBuf[5] = 0x14;    /* ADR, CONTROL */
    pbBuf[6] = 1;       /* first track in last complete session */

    if (fMSF)
    {
        pbBuf[8] = 0;   /* reserved */
        mmcLBA2MSF(pbBuf + 8, 0);
    }
    else
    {
        /* sector 0 */
        vscsiH2BEU32(pbBuf + 8, 0);
    }

    RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, 12);

    return vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
}

static DECLCALLBACK(int) vscsiLunMmcInit(PVSCSILUNINT pVScsiLun)
{
    PVSCSILUNMMC    pVScsiLunMmc = (PVSCSILUNMMC)pVScsiLun;
    uint64_t        cbDisk = 0;
    int             rc = VINF_SUCCESS;

    pVScsiLunMmc->cbSector = 2048;  /* Default to 2K sectors. */
    rc = vscsiLunMediumGetSize(pVScsiLun, &cbDisk);
    if (RT_SUCCESS(rc))
        pVScsiLunMmc->cSectors = cbDisk / pVScsiLunMmc->cbSector;

    return rc;
}

static DECLCALLBACK(int) vscsiLunMmcDestroy(PVSCSILUNINT pVScsiLun)
{
    RT_NOREF1(pVScsiLun);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vscsiLunMmcReqProcess(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq)
{
    PVSCSILUNMMC    pVScsiLunMmc = (PVSCSILUNMMC)pVScsiLun;
    VSCSIIOREQTXDIR enmTxDir = VSCSIIOREQTXDIR_INVALID;
    uint64_t        uLbaStart = 0;
    uint32_t        cSectorTransfer = 0;
    int             rc = VINF_SUCCESS;
    int             rcReq = SCSI_STATUS_OK;
    unsigned        uCmd = pVScsiReq->pbCDB[0];

    /*
     * GET CONFIGURATION, GET EVENT/STATUS NOTIFICATION, INQUIRY, and REQUEST SENSE commands
     * operate even when a unit attention condition exists for initiator; every other command
     * needs to report CHECK CONDITION in that case.
     */
    if (!pVScsiLunMmc->Core.fReady && uCmd != SCSI_INQUIRY)
    {
        /*
         * A note on media changes: As long as a medium is not present, the unit remains in
         * the 'not ready' state. Technically the unit becomes 'ready' soon after a medium
         * is inserted; however, we internally keep the 'not ready' state until we've had
         * a chance to report the UNIT ATTENTION status indicating a media change.
         */
        if (pVScsiLunMmc->Core.fMediaPresent)
        {
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_UNIT_ATTENTION,
                                             SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED, 0x00);
            pVScsiLunMmc->Core.fReady = true;
        }
        else
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_NOT_READY,
                                             SCSI_ASC_MEDIUM_NOT_PRESENT, 0x00);
    }
    else
    {
        switch (uCmd)
        {
        case SCSI_TEST_UNIT_READY:
            Assert(!pVScsiLunMmc->Core.fReady); /* Only should get here if LUN isn't ready. */
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT, 0x00);
            break;

        case SCSI_INQUIRY:
        {
            SCSIINQUIRYDATA ScsiInquiryReply;

            memset(&ScsiInquiryReply, 0, sizeof(ScsiInquiryReply));

            ScsiInquiryReply.cbAdditional           = 31;
            ScsiInquiryReply.fRMB                   = 1;    /* Removable. */
            ScsiInquiryReply.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_CD_DVD;
            ScsiInquiryReply.u3PeripheralQualifier  = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_CONNECTED;
            ScsiInquiryReply.u3AnsiVersion          = 0x05; /* MMC-?? compliant */
            ScsiInquiryReply.fCmdQue                = 1;    /* Command queuing supported. */
            ScsiInquiryReply.fWBus16                = 1;
            vscsiPadStr(ScsiInquiryReply.achVendorId, "VBOX", 8);
            vscsiPadStr(ScsiInquiryReply.achProductId, "CD-ROM", 16);
            vscsiPadStr(ScsiInquiryReply.achProductLevel, "1.0", 4);

            RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, (uint8_t *)&ScsiInquiryReply, sizeof(SCSIINQUIRYDATA));
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
            if (pVScsiLunMmc->cSectors > UINT32_C(0xffffffff))
                vscsiH2BEU32(aReply, UINT32_C(0xffffffff));
            else
                vscsiH2BEU32(aReply, pVScsiLunMmc->cSectors - 1);
            vscsiH2BEU32(&aReply[4], pVScsiLunMmc->cbSector);
            RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, sizeof(aReply));
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            break;
        }
        case SCSI_MODE_SENSE_6:
        {
            uint8_t uModePage = pVScsiReq->pbCDB[2] & 0x3f;
            uint8_t aReply[24];
            uint8_t *pu8ReplyPos;
            bool    fValid = false;

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
                fValid = true;
            } else if (uModePage == 0) {
                fValid = true;
            }

            /* Querying unknown pages must fail. */
            if (fValid) {
                RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, sizeof(aReply));
                rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            } else {
                rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET, 0x00);
            }
            break;
        }
        case SCSI_MODE_SELECT_6:
        {
            /* @todo: implement!! */
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

                    RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, sizeof(aReply));
                    rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
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

                        RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, sizeof(aReply));
                        rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
                        break;
                    }
                }
                default:
                    rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET, 0x00);
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
                    vscsiH2BEU64(aReply, pVScsiLunMmc->cSectors - 1);
                    vscsiH2BEU32(&aReply[8], pVScsiLunMmc->cbSector);
                    /* Leave the rest 0 */
                    RTSgBufCopyFromBuf(&pVScsiReq->SgBuf, aReply, sizeof(aReply));
                    rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
                    break;
                }
                default:
                    rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET, 0x00); /* Don't know if this is correct */
            }
            break;
        }
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
        {
            pVScsiLunMmc->fLocked = pVScsiReq->pbCDB[4] & 1;
            vscsiLunMediumSetLock(pVScsiLun, pVScsiLunMmc->fLocked);
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            break;
        }
        case SCSI_READ_TOC_PMA_ATIP:
        {
            uint8_t     format;
            uint16_t    cbMax;
            bool        fMSF;

            format = pVScsiReq->pbCDB[2] & 0x0f;
            cbMax  = vscsiBE2HU16(&pVScsiReq->pbCDB[7]);
            fMSF   = (pVScsiReq->pbCDB[1] >> 1) & 1;
            switch (format)
            {
                case 0x00:
                    mmcReadTOCNormal(pVScsiLun, pVScsiReq, cbMax, fMSF);
                    break;
                case 0x01:
                    mmcReadTOCMulti(pVScsiLun, pVScsiReq, cbMax, fMSF);
                    break;
                default:
                    rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET, 0x00);
            }
            break;
        }

        default:
            //AssertMsgFailed(("Command %#x [%s] not implemented\n", pVScsiReq->pbCDB[0], SCSICmdText(pVScsiReq->pbCDB[0])));
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE, 0x00);
        }
    }

    if (enmTxDir != VSCSIIOREQTXDIR_INVALID)
    {
        LogFlow(("%s: uLbaStart=%llu cSectorTransfer=%u\n",
                 __FUNCTION__, uLbaStart, cSectorTransfer));

        if (RT_UNLIKELY(uLbaStart + cSectorTransfer > pVScsiLunMmc->cSectors))
        {
            rcReq = vscsiLunReqSenseErrorSet(pVScsiLun, pVScsiReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR, 0x00);
            vscsiDeviceReqComplete(pVScsiLun->pVScsiDevice, pVScsiReq, rcReq, false, VINF_SUCCESS);
        }
        else if (!cSectorTransfer)
        {
            /* A 0 transfer length is not an error. */
            rcReq = vscsiLunReqSenseOkSet(pVScsiLun, pVScsiReq);
            vscsiDeviceReqComplete(pVScsiLun->pVScsiDevice, pVScsiReq, rcReq, false, VINF_SUCCESS);
        }
        else
        {
            /* Enqueue new I/O request */
            rc = vscsiIoReqTransferEnqueue(pVScsiLun, pVScsiReq, enmTxDir,
                                           uLbaStart * pVScsiLunMmc->cbSector,
                                           cSectorTransfer * pVScsiLunMmc->cbSector);
        }
    }
    else /* Request completed */
        vscsiDeviceReqComplete(pVScsiLun->pVScsiDevice, pVScsiReq, rcReq, false, VINF_SUCCESS);

    return rc;
}

VSCSILUNDESC g_VScsiLunTypeMmc =
{
    /** enmLunType */
    VSCSILUNTYPE_MMC,
    /** pcszDescName */
    "MMC",
    /** cbLun */
    sizeof(VSCSILUNMMC),
    /** pfnVScsiLunInit */
    vscsiLunMmcInit,
    /** pfnVScsiLunDestroy */
    vscsiLunMmcDestroy,
    /** pfnVScsiLunReqProcess */
    vscsiLunMmcReqProcess
};
