/* $Id$ */
/** @file
 * DrvHostBase - Host base drive access driver, Linux specifics.
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_BASE
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <linux/version.h>
/* All the following crap is apparently not necessary anymore since Linux
 * version 2.6.29. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
/* This is a hack to work around conflicts between these linux kernel headers
 * and the GLIBC tcpip headers. They have different declarations of the 4
 * standard byte order functions. */
# define _LINUX_BYTEORDER_GENERIC_H
/* This is another hack for not bothering with C++ unfriendly byteswap macros. */
/* Those macros that are needed are defined in the header below. */
# include "swab.h"
#endif
#include <linux/fd.h>
#include <linux/cdrom.h>
#include <limits.h>

#include <iprt/mem.h>
#include <iprt/file.h>
#include <VBox/scsi.h>

#include "DrvHostBase.h"

DECLHIDDEN(int) drvHostBaseScsiCmdOs(PDRVHOSTBASE pThis, const uint8_t *pbCmd, size_t cbCmd, PDMMEDIATXDIR enmTxDir,
                                     void *pvBuf, uint32_t *pcbBuf, uint8_t *pbSense, size_t cbSense, uint32_t cTimeoutMillies)
{
    /*
     * Minimal input validation.
     */
    Assert(enmTxDir == PDMMEDIATXDIR_NONE || enmTxDir == PDMMEDIATXDIR_FROM_DEVICE || enmTxDir == PDMMEDIATXDIR_TO_DEVICE);
    Assert(!pvBuf || pcbBuf);
    Assert(pvBuf || enmTxDir == PDMMEDIATXDIR_NONE);
    Assert(pbSense || !cbSense); RT_NOREF(cbSense);
    AssertPtr(pbCmd);
    Assert(cbCmd <= 16 && cbCmd >= 1);

    int rc = VERR_GENERAL_FAILURE;
    int direction;
    struct cdrom_generic_command cgc;

    switch (enmTxDir)
    {
        case PDMMEDIATXDIR_NONE:
            Assert(*pcbBuf == 0);
            direction = CGC_DATA_NONE;
            break;
        case PDMMEDIATXDIR_FROM_DEVICE:
            Assert(*pcbBuf != 0);
            Assert(*pcbBuf <= SCSI_MAX_BUFFER_SIZE);
            /* Make sure that the buffer is clear for commands reading
             * data. The actually received data may be shorter than what
             * we expect, and due to the unreliable feedback about how much
             * data the ioctl actually transferred, it's impossible to
             * prevent that. Returning previous buffer contents may cause
             * security problems inside the guest OS, if users can issue
             * commands to the CDROM device. */
            memset(pThis->pbDoubleBuffer, '\0', *pcbBuf);
            direction = CGC_DATA_READ;
            break;
        case PDMMEDIATXDIR_TO_DEVICE:
            Assert(*pcbBuf != 0);
            Assert(*pcbBuf <= SCSI_MAX_BUFFER_SIZE);
            memcpy(pThis->pbDoubleBuffer, pvBuf, *pcbBuf);
            direction = CGC_DATA_WRITE;
            break;
        default:
            AssertMsgFailed(("enmTxDir invalid!\n"));
            direction = CGC_DATA_NONE;
    }
    memset(&cgc, '\0', sizeof(cgc));
    memcpy(cgc.cmd, pbCmd, RT_MIN(CDROM_PACKET_SIZE, cbCmd));
    cgc.buffer = (unsigned char *)pThis->pbDoubleBuffer;
    cgc.buflen = *pcbBuf;
    cgc.stat = 0;
    Assert(cbSense >= sizeof(struct request_sense));
    cgc.sense = (struct request_sense *)pbSense;
    cgc.data_direction = direction;
    cgc.quiet = false;
    cgc.timeout = cTimeoutMillies;
    rc = ioctl(RTFileToNative(pThis->hFileDevice), CDROM_SEND_PACKET, &cgc);
    if (rc < 0)
    {
        if (errno == EBUSY)
            rc = VERR_PDM_MEDIA_LOCKED;
        else if (errno == ENOSYS)
            rc = VERR_NOT_SUPPORTED;
        else
        {
            rc = RTErrConvertFromErrno(errno);
            if (rc == VERR_ACCESS_DENIED && cgc.sense->sense_key == SCSI_SENSE_NONE)
                cgc.sense->sense_key = SCSI_SENSE_ILLEGAL_REQUEST;
            Log2(("%s: error status %d, rc=%Rrc\n", __FUNCTION__, cgc.stat, rc));
        }
    }
    switch (enmTxDir)
    {
        case PDMMEDIATXDIR_FROM_DEVICE:
            memcpy(pvBuf, pThis->pbDoubleBuffer, *pcbBuf);
            break;
        default:
            ;
    }
    Log2(("%s: after ioctl: cgc.buflen=%d txlen=%d\n", __FUNCTION__, cgc.buflen, *pcbBuf));
    /* The value of cgc.buflen does not reliably reflect the actual amount
     * of data transferred (for packet commands with little data transfer
     * it's 0). So just assume that everything worked ok. */

    return rc;
}

DECLHIDDEN(int) drvHostBaseGetMediaSizeOs(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    int rc = VERR_INVALID_STATE;

    if (PDMMEDIATYPE_IS_FLOPPY(pThis->enmType))
    {
        rc = ioctl(RTFileToNative(pThis->hFileDevice), FDFLUSH);
        if (rc)
        {
            rc = RTErrConvertFromErrno (errno);
            Log(("DrvHostFloppy: FDFLUSH ioctl(%s) failed, errno=%d rc=%Rrc\n", pThis->pszDevice, errno, rc));
            return rc;
        }

        floppy_drive_struct DrvStat;
        rc = ioctl(RTFileToNative(pThis->hFileDevice), FDGETDRVSTAT, &DrvStat);
        if (rc)
        {
            rc = RTErrConvertFromErrno(errno);
            Log(("DrvHostFloppy: FDGETDRVSTAT ioctl(%s) failed, errno=%d rc=%Rrc\n", pThis->pszDevice, errno, rc));
            return rc;
        }
        pThis->fReadOnly = !(DrvStat.flags & FD_DISK_WRITABLE);
        rc = RTFileSeek(pThis->hFileDevice, 0, RTFILE_SEEK_END, pcb);
    }
    else if (pThis->enmType == PDMMEDIATYPE_CDROM || pThis->enmType == PDMMEDIATYPE_DVD)
    {
        /* Clear the media-changed-since-last-call-thingy just to be on the safe side. */
        ioctl(RTFileToNative(pThis->hFileDevice), CDROM_MEDIA_CHANGED, CDSL_CURRENT);
        rc = RTFileSeek(pThis->hFileDevice, 0, RTFILE_SEEK_END, pcb);
    }

    return rc;
}


DECLHIDDEN(int) drvHostBaseReadOs(PDRVHOSTBASE pThis, uint64_t off, void *pvBuf, size_t cbRead)
{
    return RTFileReadAt(pThis->hFileDevice, off, pvBuf, cbRead, NULL);
}


DECLHIDDEN(int) drvHostBaseWriteOs(PDRVHOSTBASE pThis, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    return RTFileWriteAt(pThis->hFileDevice, off, pvBuf, cbWrite, NULL);
}


DECLHIDDEN(int) drvHostBaseFlushOs(PDRVHOSTBASE pThis)
{
    return RTFileFlush(pThis->hFileDevice);
}


DECLHIDDEN(int) drvHostBasePollerWakeupOs(PDRVHOSTBASE pThis)
{
    return RTSemEventSignal(pThis->EventPoller);
}


DECLHIDDEN(void) drvHostBaseDestructOs(PDRVHOSTBASE pThis)
{
    if (pThis->pbDoubleBuffer)
    {
        RTMemFree(pThis->pbDoubleBuffer);
        pThis->pbDoubleBuffer = NULL;
    }

    if (pThis->EventPoller != NULL)
    {
        RTSemEventDestroy(pThis->EventPoller);
        pThis->EventPoller = NULL;
    }

    if (pThis->hFileDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->hFileDevice);
        AssertRC(rc);
        pThis->hFileDevice = NIL_RTFILE;
    }
}

