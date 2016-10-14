/* $Id$ */
/** @file
 * DrvHostBase - Host base drive access driver, Solaris specifics.
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
#include <fcntl.h>
#include <errno.h>
#include <stropts.h>
#include <malloc.h>
#include <sys/dkio.h>
#include <pwd.h>
#include <unistd.h>
#include <syslog.h>
#ifdef VBOX_WITH_SUID_WRAPPER
# include <auth_attr.h>
#endif
#include <sys/sockio.h>
#include <sys/scsi/scsi.h>

extern "C" char *getfullblkname(char *);

#include <iprt/file.h>
#include "DrvHostBase.h"

#ifdef VBOX_WITH_SUID_WRAPPER
/* These functions would have to go into a separate solaris binary with
 * the setuid permission set, which would run the user-SCSI ioctl and
 * return the value. BUT... this might be prohibitively slow.
 */

/**
 * Setuid wrapper to gain root access.
 *
 * @returns VBox error code.
 * @param   pEffUserID     Pointer to effective user ID.
 */
static int solarisEnterRootMode(uid_t *pEffUserID)
{
    /* Increase privilege if required */
    if (*pEffUserID != 0)
    {
        if (seteuid(0) == 0)
        {
            *pEffUserID = 0;
            return VINF_SUCCESS;
        }
        return VERR_PERMISSION_DENIED;
    }
    return VINF_SUCCESS;
}


/**
 * Setuid wrapper to relinquish root access.
 *
 * @returns VBox error code.
 * @param   pEffUserID     Pointer to effective user ID.
 */
static int solarisExitRootMode(uid_t *pEffUserID)
{
    /* Get back to user mode. */
    if (*pEffUserID == 0)
    {
        uid_t realID = getuid();
        if (seteuid(realID) == 0)
        {
            *pEffUserID = realID;
            return VINF_SUCCESS;
        }
        return VERR_PERMISSION_DENIED;
    }
    return VINF_SUCCESS;
}

#endif /* VBOX_WITH_SUID_WRAPPER */

DECLHIDDEN(int) drvHostBaseScsiCmdOs(PDRVHOSTBASE pThis, const uint8_t *pbCmd, size_t cbCmd, PDMMEDIATXDIR enmTxDir,
                                     void *pvBuf, uint32_t *pcbBuf, uint8_t *pbSense, size_t cbSense, uint32_t cTimeoutMillies)
{
    /*
     * Minimal input validation.
     */
    Assert(enmTxDir == PDMMEDIATXDIR_NONE || enmTxDir == PDMMEDIATXDIR_FROM_DEVICE || enmTxDir == PDMMEDIATXDIR_TO_DEVICE);
    Assert(!pvBuf || pcbBuf);
    Assert(pvBuf || enmTxDir == PDMMEDIATXDIR_NONE);
    Assert(pbSense || !cbSense);
    AssertPtr(pbCmd);
    Assert(cbCmd <= 16 && cbCmd >= 1);

    int rc = VERR_GENERAL_FAILURE;
    struct uscsi_cmd usc;
    union scsi_cdb scdb;
    memset(&usc, 0, sizeof(struct uscsi_cmd));
    memset(&scdb, 0, sizeof(scdb));

    switch (enmTxDir)
    {
        case PDMMEDIATXDIR_NONE:
            Assert(*pcbBuf == 0);
            usc.uscsi_flags = USCSI_READ;
            /* nothing to do */
            break;

        case PDMMEDIATXDIR_FROM_DEVICE:
            Assert(*pcbBuf != 0);
            /* Make sure that the buffer is clear for commands reading
             * data. The actually received data may be shorter than what
             * we expect, and due to the unreliable feedback about how much
             * data the ioctl actually transferred, it's impossible to
             * prevent that. Returning previous buffer contents may cause
             * security problems inside the guest OS, if users can issue
             * commands to the CDROM device. */
            memset(pvBuf, '\0', *pcbBuf);
            usc.uscsi_flags = USCSI_READ;
            break;
        case PDMMEDIATXDIR_TO_DEVICE:
            Assert(*pcbBuf != 0);
            usc.uscsi_flags = USCSI_WRITE;
            break;
        default:
            AssertMsgFailedReturn(("%d\n", enmTxDir), VERR_INTERNAL_ERROR);
    }
    usc.uscsi_flags |= USCSI_RQENABLE;
    usc.uscsi_rqbuf = (char *)pbSense;
    usc.uscsi_rqlen = cbSense;
    usc.uscsi_cdb = (caddr_t)&scdb;
    usc.uscsi_cdblen = 12;
    memcpy (usc.uscsi_cdb, pbCmd, usc.uscsi_cdblen);
    usc.uscsi_bufaddr = (caddr_t)pvBuf;
    usc.uscsi_buflen = *pcbBuf;
    usc.uscsi_timeout = (cTimeoutMillies + 999) / 1000;

    /* We need root privileges for user-SCSI under Solaris. */
#ifdef VBOX_WITH_SUID_WRAPPER
    uid_t effUserID = geteuid();
    solarisEnterRootMode(&effUserID); /** @todo check return code when this really works. */
#endif
    rc = ioctl(RTFileToNative(pThis->hFileRawDevice), USCSICMD, &usc);
#ifdef VBOX_WITH_SUID_WRAPPER
    solarisExitRootMode(&effUserID);
#endif
    if (rc < 0)
    {
        if (errno == EPERM)
            return VERR_PERMISSION_DENIED;
        if (usc.uscsi_status)
        {
            rc = RTErrConvertFromErrno(errno);
            Log2(("%s: error status. rc=%Rrc\n", __FUNCTION__, rc));
        }
    }
    Log2(("%s: after ioctl: residual buflen=%d original buflen=%d\n", __FUNCTION__, usc.uscsi_resid, usc.uscsi_buflen));

    return rc;
}

DECLHIDDEN(int) drvHostBaseGetMediaSizeOs(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    /*
     * Sun docs suggests using DKIOCGGEOM instead of DKIOCGMEDIAINFO, but
     * Sun themselves use DKIOCGMEDIAINFO for DVDs/CDs, and use DKIOCGGEOM
     * for secondary storage devices.
     */
    struct dk_minfo MediaInfo;
    if (ioctl(RTFileToNative(pThis->hFileRawDevice), DKIOCGMEDIAINFO, &MediaInfo) == 0)
    {
        *pcb = MediaInfo.dki_capacity * (uint64_t)MediaInfo.dki_lbsize;
        return VINF_SUCCESS;
    }
    return RTFileSeek(pThis->hFileDevice, 0, RTFILE_SEEK_END, pcb);
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


DECLHIDDEN(int) drvHostBaseDoLockOs(PDRVHOSTBASE pThis, bool fLock)
{
    int rc = ioctl(RTFileToNative(pThis->hFileRawDevice), fLock ? DKIOCLOCK : DKIOCUNLOCK, 0);
    if (rc < 0)
    {
        if (errno == EBUSY)
            rc = VERR_ACCESS_DENIED;
        else if (errno == ENOTSUP || errno == ENOSYS)
            rc = VERR_NOT_SUPPORTED;
        else
            rc = RTErrConvertFromErrno(errno);
    }

    return rc;
}


DECLHIDDEN(int) drvHostBaseEjectOs(PDRVHOSTBASE pThis)
{
    int rc = ioctl(RTFileToNative(pThis->hFileRawDevice), DKIOCEJECT, 0);
    if (rc < 0)
    {
        if (errno == EBUSY)
            rc = VERR_PDM_MEDIA_LOCKED;
        else if (errno == ENOSYS || errno == ENOTSUP)
            rc = VERR_NOT_SUPPORTED;
        else if (errno == ENODEV)
            rc = VERR_PDM_MEDIA_NOT_MOUNTED;
        else
            rc = RTErrConvertFromErrno(errno);
    }

    return rc;
}


DECLHIDDEN(int) drvHostBaseQueryMediaStatusOs(PDRVHOSTBASE pThis, bool *pfMediaChanged, bool *pfMediaPresent)
{
    *pfMediaPresent = false;
    *pfMediaChanged = false;

    /* Need to pass the previous state and DKIO_NONE for the first time. */
    static dkio_state s_DeviceState = DKIO_NONE;
    dkio_state PreviousState = s_DeviceState;
    int rc = ioctl(RTFileToNative(pThis->hFileRawDevice), DKIOCSTATE, &s_DeviceState);
    if (rc == 0)
    {
        *pfMediaPresent = (s_DeviceState == DKIO_INSERTED);
        if (PreviousState != s_DeviceState)
            *pfMediaChanged = true;
    }

    return VINF_SUCCESS;
}


DECLHIDDEN(int) drvHostBasePollerWakeupOs(PDRVHOSTBASE pThis)
{
    return RTSemEventSignal(pThis->EventPoller);
}


DECLHIDDEN(void) drvHostBaseDestructOs(PDRVHOSTBASE pThis)
{
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

    if (pThis->hFileRawDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->hFileRawDevice);
        AssertRC(rc);
        pThis->hFileRawDevice = NIL_RTFILE;
    }

    if (pThis->pszRawDeviceOpen)
    {
        RTStrFree(pThis->pszRawDeviceOpen);
        pThis->pszRawDeviceOpen = NULL;
    }
}

