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

/**
 * Host DVD driver instance data.
 */
typedef struct DRVHOSTDVD
{
    /** Base drivr data. */
    DRVHOSTBASE             Core;
    /** The current tracklist of the loaded medium if passthrough is used. */
    PTRACKLIST              pTrackList;
} DRVHOSTDVD;
/** Pointer to the host DVD driver instance data. */
typedef DRVHOSTDVD *PDRVHOSTDVD;

/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


static PDMMEDIATXDIR drvHostDvdGetTxDirFromCmd(uint8_t bCdb)
{
    PDMMEDIATXDIR enmTxDir = PDMMEDIATXDIR_NONE;

    switch (bCdb)
    {
        case SCSI_BLANK:
        case SCSI_CLOSE_TRACK_SESSION:
        case SCSI_LOAD_UNLOAD_MEDIUM:
        case SCSI_PAUSE_RESUME:
        case SCSI_PLAY_AUDIO_10:
        case SCSI_PLAY_AUDIO_12:
        case SCSI_PLAY_AUDIO_MSF:
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
        case SCSI_RESERVE_TRACK:
        case SCSI_SCAN:
        case SCSI_SEEK_10:
        case SCSI_REPAIR_TRACK:
        case SCSI_SET_CD_SPEED:
        case SCSI_SET_READ_AHEAD:
        case SCSI_START_STOP_UNIT:
        case SCSI_STOP_PLAY_SCAN:
        case SCSI_SYNCHRONIZE_CACHE:
        case SCSI_TEST_UNIT_READY:
        case SCSI_VERIFY_10:
            enmTxDir = PDMMEDIATXDIR_NONE;
            break;
        case SCSI_SET_STREAMING:
        case SCSI_ERASE_10:
        case SCSI_FORMAT_UNIT:
        case SCSI_MODE_SELECT_10:
        case SCSI_SEND_CUE_SHEET:
        case SCSI_SEND_DVD_STRUCTURE:
        case SCSI_SEND_EVENT:
        case SCSI_SEND_KEY:
        case SCSI_SEND_OPC_INFORMATION:
        case SCSI_WRITE_10:
        case SCSI_WRITE_AND_VERIFY_10:
        case SCSI_WRITE_12:
            enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
            break;
        case SCSI_GET_CONFIGURATION:
        case SCSI_GET_EVENT_STATUS_NOTIFICATION:
        case SCSI_GET_PERFORMANCE:
        case SCSI_INQUIRY:
        case SCSI_MECHANISM_STATUS:
        case SCSI_MODE_SENSE_10:
        case SCSI_READ_10:
        case SCSI_READ_12:
        case SCSI_READ_BUFFER:
        case SCSI_READ_BUFFER_CAPACITY:
        case SCSI_READ_CAPACITY:
        case SCSI_READ_CD:
        case SCSI_READ_CD_MSF:
        case SCSI_READ_DISC_INFORMATION:
        case SCSI_READ_DVD_STRUCTURE:
        case SCSI_READ_FORMAT_CAPACITIES:
        case SCSI_READ_SUBCHANNEL:
        case SCSI_READ_TOC_PMA_ATIP:
        case SCSI_READ_TRACK_INFORMATION:
        case SCSI_REPORT_KEY:
        case SCSI_REQUEST_SENSE:
        case SCSI_REPORT_LUNS: /* Not part of MMC-3, but used by Windows. */
            enmTxDir = PDMMEDIATXDIR_FROM_DEVICE;
            break;

        case SCSI_WRITE_BUFFER:
#if 0
            switch (pbPacket[1] & 0x1f)
            {
                case 0x04: /* download microcode */
                case 0x05: /* download microcode and save */
                case 0x06: /* download microcode with offsets */
                case 0x07: /* download microcode with offsets and save */
                case 0x0e: /* download microcode with offsets and defer activation */
                case 0x0f: /* activate deferred microcode */
                    LogRel(("PIIX3 ATA: LUN#%d: CD-ROM passthrough command attempted to update firmware, blocked\n", s->iLUN));
                    atapiR3CmdErrorSimple(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                    break;
                default:
                    enmTxDir = PDMMEDIATXDIR_TO_DEVICE;
                    break;
            }
#endif
            break;
        case SCSI_REZERO_UNIT:
            /* Obsolete command used by cdrecord. What else would one expect?
             * This command is not sent to the drive, it is handled internally,
             * as the Linux kernel doesn't like it (message "scsi: unknown
             * opcode 0x01" in syslog) and replies with a sense code of 0,
             * which sends cdrecord to an endless loop. */
            //atapiR3CmdErrorSimple(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
        default:
            LogRel(("HostDVD: passthrough unimplemented for command %#x\n", bCdb));
            //atapiR3CmdErrorSimple(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
    }

    return enmTxDir;
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

    PDRVHOSTBASE pThis = RT_FROM_MEMBER(pInterface, DRVHOSTBASE, IMediaEx);
    PDRVHOSTBASEREQ pReq = (PDRVHOSTBASEREQ)hIoReq;
    int rc = VINF_SUCCESS;
    void *pvBuf = NULL;

    LogFlow(("%s: pbCdb[0]=%#04x enmTxDir=%d cbBuf=%zu timeout=%u\n", __FUNCTION__, pbCdb[0], enmTxDir, cbBuf, cTimeoutMillies));

    RTCritSectEnter(&pThis->CritSect);
    if (cbBuf)
    {
        bool fWrite = (enmTxDir == PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN || enmTxDir == PDMMEDIAEXIOREQSCSITXDIR_TO_DEVICE);
        rc = drvHostBaseBufferRetain(pThis, pReq, cbBuf, fWrite, &pvBuf);
    }

    if (RT_SUCCESS(rc))
    {
        uint32_t cbBufTmp = (uint32_t)cbBuf;
        PDMMEDIATXDIR enmMediaTxDir = PDMMEDIATXDIR_NONE;
        /*
         * Pass the request on to the internal scsi command interface.
         * The command seems to be 12 bytes long, the docs a bit copy&pasty on the command length point...
         */
        if (enmTxDir == PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN || enmTxDir == PDMMEDIAEXIOREQSCSITXDIR_FROM_DEVICE)
            memset(pvBuf, '\0', cbBuf); /* we got read size, but zero it anyway. */

        if (cbBuf)
        {
            switch (enmTxDir)
            {
                case PDMMEDIAEXIOREQSCSITXDIR_FROM_DEVICE:
                    enmMediaTxDir = PDMMEDIATXDIR_FROM_DEVICE;
                    break;
                case PDMMEDIAEXIOREQSCSITXDIR_TO_DEVICE:
                    enmMediaTxDir = PDMMEDIATXDIR_TO_DEVICE;
                    break;
                case PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN:
                    enmMediaTxDir = drvHostDvdGetTxDirFromCmd(pbCdb[0]);
                    break;
                case PDMMEDIAEXIOREQSCSITXDIR_NONE:
                default:
                    enmMediaTxDir = PDMMEDIATXDIR_NONE;
                    break;
            }
        }

        rc = drvHostBaseScsiCmdOs(pThis, pbCdb, cbCdb, enmMediaTxDir, pvBuf, &cbBufTmp, pabSense, cbSense, cTimeoutMillies);
        if (rc == VERR_UNRESOLVED_ERROR)
        {
            /* sense information set */
            rc = VERR_DEV_IO_ERROR;
            *pu8ScsiSts = SCSI_STATUS_CHECK_CONDITION;
        }

        if (pbCdb[0] == SCSI_GET_EVENT_STATUS_NOTIFICATION)
        {
            uint8_t *pbBuf = (uint8_t*)pvBuf;
            Log2(("Event Status Notification class=%#02x supported classes=%#02x\n", pbBuf[2], pbBuf[3]));
            if (RT_BE2H_U16(*(uint16_t*)pbBuf) >= 6)
                Log2(("  event %#02x %#02x %#02x %#02x\n", pbBuf[4], pbBuf[5], pbBuf[6], pbBuf[7]));
        }

        if (cbBuf)
        {
            bool fWrite = (enmTxDir == PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN || enmTxDir == PDMMEDIAEXIOREQSCSITXDIR_TO_DEVICE);
            rc = drvHostBaseBufferRelease(pThis, pReq, cbBuf, fWrite, pvBuf);
        }
    }
    RTCritSectLeave(&pThis->CritSect);

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

