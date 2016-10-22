/* $Id$ */
/** @file
 * DrvHostDVD - Host DVD block driver.
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_DVD
#include <iprt/asm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/critsect.h>
#include <VBox/scsi.h>

#include "VBoxDD.h"
#include "DrvHostBase.h"
#include "ATAPIPassthrough.h"

/** ATAPI sense info size. */
#define ATAPI_SENSE_SIZE  64
/** Size of an ATAPI packet. */
#define ATAPI_PACKET_SIZE 12

/**
 * Host DVD driver instance data.
 */
typedef struct DRVHOSTDVD
{
    /** Base drivr data. */
    DRVHOSTBASE             Core;
    /** The current tracklist of the loaded medium if passthrough is used. */
    PTRACKLIST              pTrackList;
    /** ATAPI sense data. */
    uint8_t                 abATAPISense[ATAPI_SENSE_SIZE];
} DRVHOSTDVD;
/** Pointer to the host DVD driver instance data. */
typedef DRVHOSTDVD *PDRVHOSTDVD;

/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

DECLINLINE(void) drvHostDvdH2BE_U16(uint8_t *pbBuf, uint16_t val)
{
    pbBuf[0] = val >> 8;
    pbBuf[1] = val;
}


DECLINLINE(void) drvHostDvdH2BE_U24(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 16;
    pbBuf[1] = val >> 8;
    pbBuf[2] = val;
}


DECLINLINE(void) drvHostDvdH2BE_U32(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 24;
    pbBuf[1] = val >> 16;
    pbBuf[2] = val >> 8;
    pbBuf[3] = val;
}


DECLINLINE(uint16_t) drvHostDvdBE2H_U16(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 8) | pbBuf[1];
}


DECLINLINE(uint32_t) drvHostDvdBE2H_U24(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 16) | (pbBuf[1] << 8) | pbBuf[2];
}


DECLINLINE(uint32_t) drvHostDvdBE2H_U32(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 24) | (pbBuf[1] << 16) | (pbBuf[2] << 8) | pbBuf[3];
}


DECLINLINE(void) drvHostDvdLBA2MSF(uint8_t *pbBuf, uint32_t iATAPILBA)
{
    iATAPILBA += 150;
    pbBuf[0] = (iATAPILBA / 75) / 60;
    pbBuf[1] = (iATAPILBA / 75) % 60;
    pbBuf[2] = iATAPILBA % 75;
}


DECLINLINE(uint32_t) drvHostDvdMSF2LBA(const uint8_t *pbBuf)
{
    return (pbBuf[0] * 60 + pbBuf[1]) * 75 + pbBuf[2];
}

static void drvHostDvdCmdOK(PDRVHOSTDVD pThis)
{
    memset(pThis->abATAPISense, '\0', sizeof(pThis->abATAPISense));
    pThis->abATAPISense[0] = 0x70;
    pThis->abATAPISense[7] = 10;
}

static void drvHostDvdCmdError(PDRVHOSTDVD pThis, const uint8_t *pabATAPISense, size_t cbATAPISense)
{
    Log(("%s: sense=%#x (%s) asc=%#x ascq=%#x (%s)\n", __FUNCTION__, pabATAPISense[2] & 0x0f, SCSISenseText(pabATAPISense[2] & 0x0f),
             pabATAPISense[12], pabATAPISense[13], SCSISenseExtText(pabATAPISense[12], pabATAPISense[13])));
    memset(pThis->abATAPISense, '\0', sizeof(pThis->abATAPISense));
    memcpy(pThis->abATAPISense, pabATAPISense, RT_MIN(cbATAPISense, sizeof(pThis->abATAPISense)));
}

/** @todo deprecated function - doesn't provide enough info. Replace by direct
 * calls to drvHostDvdCmdError()  with full data. */
static void drvHostDvdCmdErrorSimple(PDRVHOSTDVD pThis, uint8_t uATAPISenseKey, uint8_t uATAPIASC)
{
    uint8_t abATAPISense[ATAPI_SENSE_SIZE];
    memset(abATAPISense, '\0', sizeof(abATAPISense));
    abATAPISense[0] = 0x70 | (1 << 7);
    abATAPISense[2] = uATAPISenseKey & 0x0f;
    abATAPISense[7] = 10;
    abATAPISense[12] = uATAPIASC;
    drvHostDvdCmdError(pThis, abATAPISense, sizeof(abATAPISense));
}

static void drvHostDvdSCSIPadStr(uint8_t *pbDst, const char *pbSrc, uint32_t cbSize)
{
    for (uint32_t i = 0; i < cbSize; i++)
    {
        if (*pbSrc)
            pbDst[i] = *pbSrc++;
        else
            pbDst[i] = ' ';
    }
}


static bool drvHostDvdParseCdb(PDRVHOSTDVD pThis, PDRVHOSTBASEREQ pReq,
                               const uint8_t *pbCdb, size_t cbCdb, size_t cbBuf,
                               PDMMEDIATXDIR *penmTxDir, size_t *pcbXfer,
                               size_t *pcbSector, uint8_t *pu8ScsiSts)
{
    uint32_t uLba = 0;
    uint32_t cSectors = 0;
    size_t cbSector = 0;
    size_t cbXfer = 0;
    bool fPassthrough = false;
    PDMMEDIATXDIR enmTxDir = PDMMEDIATXDIR_NONE;

    RT_NOREF(cbCdb);

    switch (pbCdb[0])
    {
        /* First the commands we can pass through without further processing. */
        case SCSI_BLANK:
        case SCSI_CLOSE_TRACK_SESSION:
        case SCSI_LOAD_UNLOAD_MEDIUM:
        case SCSI_PAUSE_RESUME:
        case SCSI_PLAY_AUDIO_10:
        case SCSI_PLAY_AUDIO_12:
        case SCSI_PLAY_AUDIO_MSF:
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
        case SCSI_REPAIR_TRACK:
        case SCSI_RESERVE_TRACK:
        case SCSI_SCAN:
        case SCSI_SEEK_10:
        case SCSI_SET_CD_SPEED:
        case SCSI_SET_READ_AHEAD:
        case SCSI_START_STOP_UNIT:
        case SCSI_STOP_PLAY_SCAN:
        case SCSI_SYNCHRONIZE_CACHE:
        case SCSI_TEST_UNIT_READY:
        case SCSI_VERIFY_10:
            fPassthrough = true;
            break;
        case SCSI_ERASE_10:
            uLba = drvHostDvdBE2H_U32(pbCdb + 2);
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_FORMAT_UNIT:
            cbXfer = cbBuf;
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_GET_CONFIGURATION:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_GET_EVENT_STATUS_NOTIFICATION:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_GET_PERFORMANCE:
            cbXfer = cbBuf;
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_INQUIRY:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 3);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_MECHANISM_STATUS:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 8);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_MODE_SELECT_10:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_MODE_SENSE_10:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_10:
            uLba = drvHostDvdBE2H_U32(pbCdb + 2);
            cSectors = drvHostDvdBE2H_U16(pbCdb + 7);
            cbSector = 2048;
            cbXfer = cSectors * cbSector;
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_12:
            uLba = drvHostDvdBE2H_U32(pbCdb + 2);
            cSectors = drvHostDvdBE2H_U32(pbCdb + 6);
            cbSector = 2048;
            cbXfer = cSectors * cbSector;
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_BUFFER:
            cbXfer = drvHostDvdBE2H_U24(pbCdb + 6);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_BUFFER_CAPACITY:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_CAPACITY:
            cbXfer = 8;
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_CD:
        case SCSI_READ_CD_MSF:
        {
            /* Get sector size based on the expected sector type field. */
            switch ((pbCdb[1] >> 2) & 0x7)
            {
                case 0x0: /* All types. */
                {
                    uint32_t iLbaStart;

                    if (pbCdb[0] == SCSI_READ_CD)
                        iLbaStart = drvHostDvdBE2H_U32(&pbCdb[2]);
                    else
                        iLbaStart = drvHostDvdMSF2LBA(&pbCdb[3]);

                    if (pThis->pTrackList)
                        cbSector = ATAPIPassthroughTrackListGetSectorSizeFromLba(pThis->pTrackList, iLbaStart);
                    else
                        cbSector = 2048; /* Might be incorrect if we couldn't determine the type. */
                    break;
                }
                case 0x1: /* CD-DA */
                    cbSector = 2352;
                    break;
                case 0x2: /* Mode 1 */
                    cbSector = 2048;
                    break;
                case 0x3: /* Mode 2 formless */
                    cbSector = 2336;
                    break;
                case 0x4: /* Mode 2 form 1 */
                    cbSector = 2048;
                    break;
                case 0x5: /* Mode 2 form 2 */
                    cbSector = 2324;
                    break;
                default: /* Reserved */
                    AssertMsgFailed(("Unknown sector type\n"));
                    cbSector = 0; /** @todo we should probably fail the command here already. */
            }

            if (pbCdb[0] == SCSI_READ_CD)
                cbXfer = drvHostDvdBE2H_U24(pbCdb + 6) * cbSector;
            else /* SCSI_READ_MSF */
            {
                cSectors = drvHostDvdMSF2LBA(pbCdb + 6) - drvHostDvdMSF2LBA(pbCdb + 3);
                if (cSectors > 32)
                    cSectors = 32; /* Limit transfer size to 64~74K. Safety first. In any case this can only harm software doing CDDA extraction. */
                cbXfer = cSectors * cbSector;
            }
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        }
        case SCSI_READ_DISC_INFORMATION:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_DVD_STRUCTURE:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 8);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_FORMAT_CAPACITIES:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_SUBCHANNEL:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_TOC_PMA_ATIP:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_READ_TRACK_INFORMATION:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_REPORT_KEY:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 8);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_REQUEST_SENSE:
            cbXfer = pbCdb[4];
            if ((pThis->abATAPISense[2] & 0x0f) != SCSI_SENSE_NONE)
            {
                /* Copy sense data over. */
                void *pvBuf = NULL;
                int rc = drvHostBaseBufferRetain(&pThis->Core, pReq, cbBuf, false /*fWrite*/, &pvBuf);
                if (RT_SUCCESS(rc))
                {
                    memcpy(pvBuf, &pThis->abATAPISense[0], RT_MIN(sizeof(pThis->abATAPISense), cbBuf));
                    rc = drvHostBaseBufferRelease(&pThis->Core, pReq, cbBuf, false /* fWrite */, pvBuf);
                    AssertRC(rc);
                    drvHostDvdCmdOK(pThis);
                    *pu8ScsiSts = SCSI_STATUS_OK;
                }
                break;
            }
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_SEND_CUE_SHEET:
            cbXfer = drvHostDvdBE2H_U24(pbCdb + 6);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_SEND_DVD_STRUCTURE:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 8);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_SEND_EVENT:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 8);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_SEND_KEY:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 8);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_SEND_OPC_INFORMATION:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 7);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_SET_STREAMING:
            cbXfer = drvHostDvdBE2H_U16(pbCdb + 9);
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_WRITE_10:
        case SCSI_WRITE_AND_VERIFY_10:
            uLba = drvHostDvdBE2H_U32(pbCdb + 2);
            cSectors = drvHostDvdBE2H_U16(pbCdb + 7);
            if (pThis->pTrackList)
                cbSector = ATAPIPassthroughTrackListGetSectorSizeFromLba(pThis->pTrackList, uLba);
            else
                cbSector = 2048;
            cbXfer = cSectors * cbSector;
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_WRITE_12:
            uLba = drvHostDvdBE2H_U32(pbCdb + 2);
            cSectors = drvHostDvdBE2H_U32(pbCdb + 6);
            if (pThis->pTrackList)
                cbSector = ATAPIPassthroughTrackListGetSectorSizeFromLba(pThis->pTrackList, uLba);
            else
                cbSector = 2048;
            cbXfer = cSectors * cbSector;
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_WRITE_BUFFER:
            switch (pbCdb[1] & 0x1f)
            {
                case 0x04: /* download microcode */
                case 0x05: /* download microcode and save */
                case 0x06: /* download microcode with offsets */
                case 0x07: /* download microcode with offsets and save */
                case 0x0e: /* download microcode with offsets and defer activation */
                case 0x0f: /* activate deferred microcode */
                    LogRel(("HostDVD#%u: CD-ROM passthrough command attempted to update firmware, blocked\n", pThis->Core.pDrvIns->iInstance));
                    drvHostDvdCmdErrorSimple(pThis, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                    *pu8ScsiSts = SCSI_STATUS_CHECK_CONDITION;
                    break;
                default:
                    cbXfer = drvHostDvdBE2H_U16(pbCdb + 6);
                    enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
                    fPassthrough = true;
                    break;
            }
            break;
        case SCSI_REPORT_LUNS: /* Not part of MMC-3, but used by Windows. */
            cbXfer = drvHostDvdBE2H_U32(pbCdb + 6);
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            fPassthrough = true;
            break;
        case SCSI_REZERO_UNIT:
            /* Obsolete command used by cdrecord. What else would one expect?
             * This command is not sent to the drive, it is handled internally,
             * as the Linux kernel doesn't like it (message "scsi: unknown
             * opcode 0x01" in syslog) and replies with a sense code of 0,
             * which sends cdrecord to an endless loop. */
            drvHostDvdCmdErrorSimple(pThis, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            *pu8ScsiSts = SCSI_STATUS_CHECK_CONDITION;
            break;
        default:
            LogRel(("HostDVD#%u: Passthrough unimplemented for command %#x\n", pThis->Core.pDrvIns->iInstance, pbCdb[0]));
            drvHostDvdCmdErrorSimple(pThis, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            *pu8ScsiSts = SCSI_STATUS_CHECK_CONDITION;
            break;
    }

    if (fPassthrough)
    {
        *penmTxDir = enmTxDir;
        *pcbXfer   = cbXfer;
        *pcbSector = cbSector;
    }

    return fPassthrough;
}

/**
 * Locks or unlocks the drive.
 *
 * @returns VBox status code.
 * @param   pThis       The instance data.
 * @param   fLock       True if the request is to lock the drive, false if to unlock.
 */
static DECLCALLBACK(int) drvHostDvdDoLock(PDRVHOSTBASE pThis, bool fLock)
{
    int rc = drvHostBaseDoLockOs(pThis, fLock);

    LogFlow(("drvHostDvdDoLock(, fLock=%RTbool): returns %Rrc\n", fLock, rc));
    return rc;
}


/** @interface_method_impl{PDMIMEDIA,pfnSendCmd} */
static DECLCALLBACK(int) drvHostDvdSendCmd(PPDMIMEDIA pInterface, const uint8_t *pbCmd,
                                           PDMMEDIATXDIR enmTxDir, void *pvBuf, uint32_t *pcbBuf,
                                           uint8_t *pabSense, size_t cbSense, uint32_t cTimeoutMillies)
{
    PDRVHOSTBASE pThis = RT_FROM_MEMBER(pInterface, DRVHOSTBASE, IMedia);
    int rc;
    LogFlow(("%s: cmd[0]=%#04x txdir=%d pcbBuf=%d timeout=%d\n", __FUNCTION__, pbCmd[0], enmTxDir, *pcbBuf, cTimeoutMillies));

    RTCritSectEnter(&pThis->CritSect);
    /*
     * Pass the request on to the internal scsi command interface.
     * The command seems to be 12 bytes long, the docs a bit copy&pasty on the command length point...
     */
    if (enmTxDir == PDMMEDIATXDIR_FROM_DEVICE)
        memset(pvBuf, '\0', *pcbBuf); /* we got read size, but zero it anyway. */
    rc = drvHostBaseScsiCmdOs(pThis, pbCmd, 12, enmTxDir, pvBuf, pcbBuf, pabSense, cbSense, cTimeoutMillies);
    if (rc == VERR_UNRESOLVED_ERROR)
        /* sense information set */
        rc = VERR_DEV_IO_ERROR;

    if (pbCmd[0] == SCSI_GET_EVENT_STATUS_NOTIFICATION)
    {
        uint8_t *pbBuf = (uint8_t*)pvBuf;
        Log2(("Event Status Notification class=%#02x supported classes=%#02x\n", pbBuf[2], pbBuf[3]));
        if (RT_BE2H_U16(*(uint16_t*)pbBuf) >= 6)
            Log2(("  event %#02x %#02x %#02x %#02x\n", pbBuf[4], pbBuf[5], pbBuf[6], pbBuf[7]));
    }
    RTCritSectLeave(&pThis->CritSect);

    LogFlow(("%s: rc=%Rrc\n", __FUNCTION__, rc));
    return rc;
}


/** @interface_method_impl{PDMIMEDIAEX,pfnIoReqSendScsiCmd} */
static DECLCALLBACK(int) drvHostDvdIoReqSendScsiCmd(PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq, uint32_t uLun,
                                                    const uint8_t *pbCdb, size_t cbCdb, PDMMEDIAEXIOREQSCSITXDIR enmTxDir,
                                                    size_t cbBuf, uint8_t *pabSense, size_t cbSense, uint8_t *pu8ScsiSts,
                                                    uint32_t cTimeoutMillies)
{
    RT_NOREF2(uLun, cTimeoutMillies);

    PDRVHOSTDVD pThis = RT_FROM_MEMBER(pInterface, DRVHOSTDVD, Core.IMediaEx);
    PDRVHOSTBASEREQ pReq = (PDRVHOSTBASEREQ)hIoReq;
    int rc = VINF_SUCCESS;

    LogFlow(("%s: pbCdb[0]=%#04x{%s} enmTxDir=%d cbBuf=%zu timeout=%u\n",
             __FUNCTION__, pbCdb[0], SCSICmdText(pbCdb[0]), enmTxDir, cbBuf, cTimeoutMillies));

    RTCritSectEnter(&pThis->Core.CritSect);

    /*
     * Parse the command first to fend off any illegal or dangeroups commands we don't want the guest
     * to execute on the host drive.
     */
    PDMMEDIATXDIR enmXferDir = PDMMEDIATXDIR_NONE;
    size_t cbXfer = 0;
    size_t cbSector = 0;
    bool fPassthrough = drvHostDvdParseCdb(pThis, pReq, pbCdb, cbCdb, cbBuf,
                                           &enmXferDir, &cbXfer, &cbSector, pu8ScsiSts);
    if (fPassthrough)
    {
        void *pvBuf = NULL;
        size_t cbXferCur = cbXfer;

        if (cbXfer)
            rc = drvHostBaseBufferRetain(&pThis->Core, pReq, cbXfer, enmXferDir == PDMMEDIATXDIR_TO_DEVICE, &pvBuf);

        if (cbXfer > SCSI_MAX_BUFFER_SIZE)
        {
            /* Linux accepts commands with up to 100KB of data, but expects
             * us to handle commands with up to 128KB of data. The usual
             * imbalance of powers. */
            uint8_t aATAPICmd[ATAPI_PACKET_SIZE];
            uint32_t iATAPILBA, cSectors;
            uint8_t *pbBuf = (uint8_t *)pvBuf;

            switch (pbCdb[0])
            {
                case SCSI_READ_10:
                case SCSI_WRITE_10:
                case SCSI_WRITE_AND_VERIFY_10:
                    iATAPILBA = drvHostDvdBE2H_U32(pbCdb + 2);
                    cSectors = drvHostDvdBE2H_U16(pbCdb + 7);
                    break;
                case SCSI_READ_12:
                case SCSI_WRITE_12:
                    iATAPILBA = drvHostDvdBE2H_U32(pbCdb + 2);
                    cSectors = drvHostDvdBE2H_U32(pbCdb + 6);
                    break;
                case SCSI_READ_CD:
                    iATAPILBA = drvHostDvdBE2H_U32(pbCdb + 2);
                    cSectors = drvHostDvdBE2H_U24(pbCdb + 6);
                    break;
                case SCSI_READ_CD_MSF:
                    iATAPILBA = drvHostDvdMSF2LBA(pbCdb + 3);
                    cSectors = drvHostDvdMSF2LBA(pbCdb + 6) - iATAPILBA;
                    break;
                default:
                    AssertMsgFailed(("Don't know how to split command %#04x\n", pbCdb[0]));
                    LogRelMax(10, ("HostDVD#%u: CD-ROM passthrough split error\n", pThis->Core.pDrvIns->iInstance));
                    drvHostDvdCmdErrorSimple(pThis, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
                    *pu8ScsiSts = SCSI_STATUS_CHECK_CONDITION;
                    rc = drvHostBaseBufferRelease(&pThis->Core, pReq, cbBuf, enmXferDir == PDMMEDIATXDIR_TO_DEVICE, pvBuf);
                    RTCritSectLeave(&pThis->Core.CritSect);
                    return VINF_SUCCESS;
            }
            memcpy(aATAPICmd, pbCdb, RT_MIN(cbCdb, ATAPI_PACKET_SIZE));
            uint32_t cReqSectors = 0;
            for (uint32_t i = cSectors; i > 0; i -= cReqSectors)
            {
                if (i * cbSector > SCSI_MAX_BUFFER_SIZE)
                    cReqSectors = SCSI_MAX_BUFFER_SIZE / (uint32_t)cbSector;
                else
                    cReqSectors = i;
                uint32_t cbCurrTX = (uint32_t)cbSector * cReqSectors;
                switch (pbCdb[0])
                {
                    case SCSI_READ_10:
                    case SCSI_WRITE_10:
                    case SCSI_WRITE_AND_VERIFY_10:
                        drvHostDvdH2BE_U32(aATAPICmd + 2, iATAPILBA);
                        drvHostDvdH2BE_U16(aATAPICmd + 7, cReqSectors);
                        break;
                    case SCSI_READ_12:
                    case SCSI_WRITE_12:
                        drvHostDvdH2BE_U32(aATAPICmd + 2, iATAPILBA);
                        drvHostDvdH2BE_U32(aATAPICmd + 6, cReqSectors);
                        break;
                    case SCSI_READ_CD:
                        drvHostDvdH2BE_U32(aATAPICmd + 2, iATAPILBA);
                        drvHostDvdH2BE_U24(aATAPICmd + 6, cReqSectors);
                        break;
                    case SCSI_READ_CD_MSF:
                        drvHostDvdLBA2MSF(aATAPICmd + 3, iATAPILBA);
                        drvHostDvdLBA2MSF(aATAPICmd + 6, iATAPILBA + cReqSectors);
                        break;
                }
                rc = drvHostBaseScsiCmdOs(&pThis->Core, aATAPICmd, sizeof(aATAPICmd),
                                          enmXferDir, pbBuf, &cbCurrTX,
                                          &pThis->abATAPISense[0], sizeof(pThis->abATAPISense),
                                          cTimeoutMillies /**< @todo timeout */);
                if (rc != VINF_SUCCESS)
                    break;
                iATAPILBA += cReqSectors;
                pbBuf += cbSector * cReqSectors;
            }
        }
        else
        {
            uint32_t cbXferTmp = (uint32_t)cbXferCur;
            rc = drvHostBaseScsiCmdOs(&pThis->Core, pbCdb, cbCdb, enmXferDir, pvBuf, &cbXferTmp,
                                      &pThis->abATAPISense[0], sizeof(pThis->abATAPISense), cTimeoutMillies);
        }

        if (RT_SUCCESS(rc))
        {
            /* Do post processing for certain commands. */
            switch (pbCdb[0])
            {
                case SCSI_SEND_CUE_SHEET:
                case SCSI_READ_TOC_PMA_ATIP:
                {
                    if (!pThis->pTrackList)
                        rc = ATAPIPassthroughTrackListCreateEmpty(&pThis->pTrackList);

                    if (RT_SUCCESS(rc))
                        rc = ATAPIPassthroughTrackListUpdate(pThis->pTrackList, pbCdb, pvBuf);

                    if (RT_FAILURE(rc))
                        LogRelMax(10, ("HostDVD#%u: Error (%Rrc) while updating the tracklist during %s, burning the disc might fail\n",
                                  pThis->Core.pDrvIns->iInstance, rc,
                                  pbCdb[0] == SCSI_SEND_CUE_SHEET ? "SEND CUE SHEET" : "READ TOC/PMA/ATIP"));
                    break;
                }
                case SCSI_SYNCHRONIZE_CACHE:
                {
                    if (pThis->pTrackList)
                        ATAPIPassthroughTrackListClear(pThis->pTrackList);
                    break;
                }
            }

            if (enmXferDir == PDMMEDIATXDIR_FROM_DEVICE)
            {
               Assert(cbXferCur <= cbXfer);

                if (pbCdb[0] == SCSI_INQUIRY)
                {
                    /* Make sure that the real drive cannot be identified.
                     * Motivation: changing the VM configuration should be as
                     *             invisible as possible to the guest. */
                    if (cbXferCur >= 8 + 8)
                        drvHostDvdSCSIPadStr((uint8_t *)pvBuf + 8, "VBOX", 8);
                    if (cbXferCur >= 16 + 16)
                        drvHostDvdSCSIPadStr((uint8_t *)pvBuf + 16, "CD-ROM", 16);
                    if (cbXferCur >= 32 + 4)
                        drvHostDvdSCSIPadStr((uint8_t *)pvBuf + 32, "1.0", 4);
                }

                if (cbXferCur)
                    Log3(("ATAPI PT data read (%d): %.*Rhxs\n", cbXferCur, cbXferCur, (uint8_t *)pvBuf));
            }

            drvHostDvdCmdOK(pThis);
            *pu8ScsiSts = SCSI_STATUS_OK;
        }
        else
        {
            do
            {
                /* don't log superfluous errors */
                if (    rc == VERR_DEV_IO_ERROR
                    && (   pbCdb[0] == SCSI_TEST_UNIT_READY
                        || pbCdb[0] == SCSI_READ_CAPACITY
                        || pbCdb[0] == SCSI_READ_DVD_STRUCTURE
                        || pbCdb[0] == SCSI_READ_TOC_PMA_ATIP))
                    break;
                LogRelMax(10, ("HostDVD#%u: CD-ROM passthrough cmd=%#04x sense=%d ASC=%#02x ASCQ=%#02x %Rrc\n",
                          pThis->Core.pDrvIns->iInstance, pbCdb[0], pThis->abATAPISense[2] & 0x0f,
                          pThis->abATAPISense[12], pThis->abATAPISense[13], rc));
            } while (0);
            drvHostDvdCmdError(pThis, &pThis->abATAPISense[0], sizeof(pThis->abATAPISense));
            *pu8ScsiSts = SCSI_STATUS_CHECK_CONDITION;
            rc = VINF_SUCCESS;
        }

        if (cbXfer)
            rc = drvHostBaseBufferRelease(&pThis->Core, pReq, cbXfer, enmXferDir == PDMMEDIATXDIR_TO_DEVICE, pvBuf);
    }

    /*
     * We handled the command, check the status code and copy over the sense data if
     * it is CHECK CONDITION.
     */
    if (   *pu8ScsiSts == SCSI_STATUS_CHECK_CONDITION
        && VALID_PTR(pabSense)
        && cbSense > 0)
        memcpy(pabSense, &pThis->abATAPISense[0], RT_MIN(cbSense, sizeof(pThis->abATAPISense)));

    RTCritSectLeave(&pThis->Core.CritSect);

    LogFlow(("%s: rc=%Rrc\n", __FUNCTION__, rc));
    return rc;
}


/* -=-=-=-=- driver interface -=-=-=-=- */


/** @interface_method_impl{PDMDRVREG,pfnDestruct} */
static DECLCALLBACK(void) drvHostDvdDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTDVD pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDVD);

    if (pThis->pTrackList)
    {
        ATAPIPassthroughTrackListDestroy(pThis->pTrackList);
        pThis->pTrackList = NULL;
    }

    DRVHostBaseDestruct(pDrvIns);
}

/**
 * Construct a host dvd drive driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostDvdConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDRVHOSTDVD pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDVD);
    LogFlow(("drvHostDvdConstruct: iInstance=%d\n", pDrvIns->iInstance));

    bool fPassthrough;
    int rc = CFGMR3QueryBool(pCfg, "Passthrough", &fPassthrough);
    if (RT_SUCCESS(rc) && fPassthrough)
    {
        pThis->Core.IMedia.pfnSendCmd            = drvHostDvdSendCmd;
        pThis->Core.IMediaEx.pfnIoReqSendScsiCmd = drvHostDvdIoReqSendScsiCmd;
        /* Passthrough requires opening the device in R/W mode. */
        pThis->Core.fReadOnlyConfig = false;
    }

    pThis->Core.pfnDoLock = drvHostDvdDoLock;

    /*
     * Init instance data.
     */
    rc = DRVHostBaseInit(pDrvIns, pCfg, "Path\0Interval\0Locked\0BIOSVisible\0AttachFailError\0Passthrough\0",
                         PDMMEDIATYPE_DVD);
    LogFlow(("drvHostDvdConstruct: returns %Rrc\n", rc));
    return rc;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvHostDVD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "HostDVD",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Host DVD Block Driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_BLOCK,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTDVD),
    /* pfnConstruct */
    drvHostDvdConstruct,
    /* pfnDestruct */
    drvHostDvdDestruct,
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

