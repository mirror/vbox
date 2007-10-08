/** @file
 *
 * VBox storage devices:
 * Host DVD block driver
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_DVD
#ifdef RT_OS_DARWIN
# include <mach/mach.h>
# include <Carbon/Carbon.h>
# include <IOKit/IOKitLib.h>
# include <IOKit/IOCFPlugIn.h>
# include <IOKit/scsi-commands/SCSITaskLib.h>
# include <IOKit/scsi-commands/SCSICommandOperationCodes.h>
# include <IOKit/storage/IOStorageDeviceCharacteristics.h>
# include <mach/mach_error.h>
# define USE_MEDIA_POLLING

#elif defined(RT_OS_L4)
/* nothing (yet). */

#elif defined RT_OS_LINUX
# include <sys/ioctl.h>
/* This is a hack to work around conflicts between these linux kernel headers
 * and the GLIBC tcpip headers. They have different declarations of the 4
 * standard byte order functions. */
# define _LINUX_BYTEORDER_GENERIC_H
/* This is another hack for not bothering with C++ unfriendly byteswap macros. */
# define _LINUX_BYTEORDER_SWAB_H
/* Those macros that are needed are defined in the header below */
# include "swab.h"
# include <linux/cdrom.h>
# include <sys/fcntl.h>
# include <errno.h>
# include <limits.h>
# define USE_MEDIA_POLLING

#elif defined(RT_OS_SOLARIS)
# include <stropts.h>
# include <fcntl.h>
# include <ctype.h>
# include <errno.h>
# include <pwd.h>
# include <unistd.h>
# include <auth_attr.h>
# include <sys/dkio.h>
# include <sys/sockio.h>
# include <sys/scsi/scsi.h>
# define USE_MEDIA_POLLING

#elif defined(RT_OS_WINDOWS)
# include <Windows.h>
# include <winioctl.h>
# include <ntddscsi.h>
# undef USE_MEDIA_POLLING

#else
# error "Unsupported Platform."
#endif

#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/critsect.h>
#include <VBox/scsi.h>

#include "Builtins.h"
#include "DrvHostBase.h"


/* Forward declarations. */

static DECLCALLBACK(int) drvHostDvdDoLock(PDRVHOSTBASE pThis, bool fLock);


/** @copydoc PDMIMOUNT::pfnUnmount */
static DECLCALLBACK(int) drvHostDvdUnmount(PPDMIMOUNT pInterface, bool fForce)
{
     PDRVHOSTBASE pThis = PDMIMOUNT_2_DRVHOSTBASE(pInterface);
     RTCritSectEnter(&pThis->CritSect);

     /*
      * Validate state.
      */
     int rc = VINF_SUCCESS;
     if (!pThis->fLocked || fForce)
     {
        /* Unlock drive if necessary. */
        if (pThis->fLocked)
            drvHostDvdDoLock(pThis, false);

         /*
          * Eject the disc.
          */
#ifdef RT_OS_DARWIN
         uint8_t abCmd[16] =
         {
             SCSI_START_STOP_UNIT, 0, 0, 0, 2 /*eject+stop*/, 0,
             0,0,0,0,0,0,0,0,0,0
         };
         rc = DRVHostBaseScsiCmd(pThis, abCmd, 6, PDMBLOCKTXDIR_NONE, NULL, NULL, NULL, 0, 0);

#elif defined(RT_OS_LINUX)
         rc = ioctl(pThis->FileDevice, CDROMEJECT, 0);
         if (rc < 0)
         {
             if (errno == EBUSY)
                 rc = VERR_PDM_MEDIA_LOCKED;
             else if (errno == ENOSYS)
                 rc = VERR_NOT_SUPPORTED;
             else
                 rc = RTErrConvertFromErrno(errno);
         }

#elif defined(RT_OS_SOLARIS)
        rc = ioctl(pThis->FileDevice, DKIOCEJECT, 0);
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

#elif defined(RT_OS_WINDOWS)
         RTFILE FileDevice = pThis->FileDevice;
         if (FileDevice == NIL_RTFILE) /* obsolete crap */
             rc = RTFileOpen(&FileDevice, pThis->pszDeviceOpen, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
         if (VBOX_SUCCESS(rc))
         {
             /* do ioctl */
             DWORD cbReturned;
             if (DeviceIoControl((HANDLE)FileDevice, IOCTL_STORAGE_EJECT_MEDIA,
                                 NULL, 0,
                                 NULL, 0, &cbReturned,
                                 NULL))
                 rc = VINF_SUCCESS;
             else
                 rc = RTErrConvertFromWin32(GetLastError());

             /* clean up handle */
             if (FileDevice != pThis->FileDevice)
                 RTFileClose(FileDevice);
         }
         else
             AssertMsgFailed(("Failed to open '%s' for ejecting this tray.\n",  rc));


#else
         AssertMsgFailed(("Eject is not implemented!\n"));
         rc = VINF_SUCCESS;
#endif

         /*
          * Media is no longer present.
          */
         DRVHostBaseMediaNotPresent(pThis);  /** @todo This isn't thread safe! */
     }
     else
     {
         Log(("drvHostDvdUnmount: Locked\n"));
         rc = VERR_PDM_MEDIA_LOCKED;
     }

     RTCritSectLeave(&pThis->CritSect);
     LogFlow(("drvHostDvdUnmount: returns %Vrc\n", rc));
     return rc;
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
#ifdef RT_OS_DARWIN
    uint8_t abCmd[16] =
    {
        SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL, 0, 0, 0, fLock, 0,
        0,0,0,0,0,0,0,0,0,0
    };
    int rc = DRVHostBaseScsiCmd(pThis, abCmd, 6, PDMBLOCKTXDIR_NONE, NULL, NULL, NULL, 0, 0);

#elif defined(RT_OS_LINUX)
    int rc = ioctl(pThis->FileDevice, CDROM_LOCKDOOR, (int)fLock);
    if (rc < 0)
    {
        if (errno == EBUSY)
            rc = VERR_ACCESS_DENIED;
        else if (errno == EDRIVE_CANT_DO_THIS)
            rc = VERR_NOT_SUPPORTED;
        else
            rc = RTErrConvertFromErrno(errno);
    }

#elif defined(RT_OS_SOLARIS)
    int rc = ioctl(pThis->FileDevice, fLock ? DKIOCLOCK : DKIOCUNLOCK, 0);
    if (rc < 0)
    {
        if (errno == EBUSY)
            rc = VERR_ACCESS_DENIED;
        else if (errno == ENOTSUP || errno == ENOSYS)
            rc = VERR_NOT_SUPPORTED;
        else
            rc = RTErrConvertFromErrno(errno);
    }

#elif defined(RT_OS_WINDOWS)

    PREVENT_MEDIA_REMOVAL PreventMediaRemoval = {fLock};
    DWORD cbReturned;
    int rc;
    if (DeviceIoControl((HANDLE)pThis->FileDevice, IOCTL_STORAGE_MEDIA_REMOVAL,
                        &PreventMediaRemoval, sizeof(PreventMediaRemoval),
                        NULL, 0, &cbReturned,
                        NULL))
        rc = VINF_SUCCESS;
    else
        /** @todo figure out the return codes for already locked. */
        rc = RTErrConvertFromWin32(GetLastError());

#else
    AssertMsgFailed(("Lock/Unlock is not implemented!\n"));
    int rc = VINF_SUCCESS;

#endif

    LogFlow(("drvHostDvdDoLock(, fLock=%RTbool): returns %Vrc\n", fLock, rc));
    return rc;
}



#ifdef RT_OS_LINUX
/**
 * Get the media size.
 *
 * @returns VBox status code.
 * @param   pThis   The instance data.
 * @param   pcb     Where to store the size.
 */
static int drvHostDvdGetMediaSize(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    /*
     * Query the media size.
     */
    /* Clear the media-changed-since-last-call-thingy just to be on the safe side. */
    ioctl(pThis->FileDevice, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
    return RTFileSeek(pThis->FileDevice, 0, RTFILE_SEEK_END, pcb);

}
#endif /* RT_OS_LINUX */


#ifdef USE_MEDIA_POLLING
/**
 * Do media change polling.
 */
DECLCALLBACK(int) drvHostDvdPoll(PDRVHOSTBASE pThis)
{
    /*
     * Poll for media change.
     */
#ifdef RT_OS_DARWIN
    AssertReturn(pThis->ppScsiTaskDI, VERR_INTERNAL_ERROR);

    /*
     * Issue a TEST UNIT READY request.
     */
    bool fMediaChanged = false;
    bool fMediaPresent = false;
    uint8_t abCmd[16] = { SCSI_TEST_UNIT_READY, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    uint8_t abSense[32];
    int rc2 = DRVHostBaseScsiCmd(pThis, abCmd, 6, PDMBLOCKTXDIR_NONE, NULL, NULL, abSense, sizeof(abSense), 0);
    if (VBOX_SUCCESS(rc2))
        fMediaPresent = true;
    else if (   rc2 == VERR_UNRESOLVED_ERROR
             && abSense[2] == 6 /* unit attention */
             && (   (abSense[12] == 0x29 && abSense[13] < 5 /* reset */)
                 || (abSense[12] == 0x2a && abSense[13] == 0 /* parameters changed */)                        //???
                 || (abSense[12] == 0x3f && abSense[13] == 0 /* target operating conditions have changed */)  //???
                 || (abSense[12] == 0x3f && abSense[13] == 2 /* changed operating definition */)              //???
                 || (abSense[12] == 0x3f && abSense[13] == 3 /* inquery parameters changed */)
                 || (abSense[12] == 0x3f && abSense[13] == 5 /* device identifier changed */)
                 )
            )
    {
        fMediaPresent = false;
        fMediaChanged = true;
        /** @todo check this media chance stuff on Darwin. */
    }

#elif defined(RT_OS_LINUX)
    bool fMediaPresent = ioctl(pThis->FileDevice, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK;

#elif defined(RT_OS_SOLARIS)
    bool fMediaPresent = false;
    bool fMediaChanged = false;

    /* Need to pass the previous state and DKIO_NONE for the first time. */
    static dkio_state DeviceState = DKIO_NONE;
    int rc2 = ioctl(pThis->FileDevice, DKIOCSTATE, &DeviceState);
    if (rc2 == 0)
    {
        fMediaPresent = DeviceState == DKIO_INSERTED;
        if (pThis->fMediaPresent != fMediaPresent || !fMediaPresent)
            fMediaChanged = true;   /** @todo find proper way to detect media change. */
    }

#else
# error "Unsupported platform."
#endif

    RTCritSectEnter(&pThis->CritSect);

    int rc = VINF_SUCCESS;
    if (pThis->fMediaPresent != fMediaPresent)
    {
        LogFlow(("drvHostDvdPoll: %d -> %d\n", pThis->fMediaPresent, fMediaPresent));
        pThis->fMediaPresent = false;
        if (fMediaPresent)
            rc = DRVHostBaseMediaPresent(pThis);
        else
            DRVHostBaseMediaNotPresent(pThis);
    }
    else if (fMediaPresent)
    {
        /*
         * Poll for media change.
         */
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
        /* taken care of above. */
#elif defined(RT_OS_LINUX)
        bool fMediaChanged = ioctl(pThis->FileDevice, CDROM_MEDIA_CHANGED, CDSL_CURRENT) == 1;
#else
# error "Unsupported platform."
#endif
        if (fMediaChanged)
        {
            LogFlow(("drvHostDVDMediaThread: Media changed!\n"));
            DRVHostBaseMediaNotPresent(pThis);
            rc = DRVHostBaseMediaPresent(pThis);
        }
    }

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}
#endif /* USE_MEDIA_POLLING */


/** @copydoc PDMIBLOCK::pfnSendCmd */
static int drvHostDvdSendCmd(PPDMIBLOCK pInterface, const uint8_t *pbCmd, PDMBLOCKTXDIR enmTxDir, void *pvBuf, size_t *pcbBuf,
                             uint8_t *pbStat, uint32_t cTimeoutMillies)
{
    PDRVHOSTBASE pThis = PDMIBLOCK_2_DRVHOSTBASE(pInterface);
    int rc;
    LogFlow(("%s: cmd[0]=%#04x txdir=%d pcbBuf=%d timeout=%d\n", __FUNCTION__, pbCmd[0], enmTxDir, *pcbBuf, cTimeoutMillies));

#ifdef RT_OS_DARWIN
    /*
     * Pass the request on to the internal scsi command interface.
     * The command seems to be 12 bytes long, the docs a bit copy&pasty on the command length point...
     */
    if (enmTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
        memset(pvBuf, '\0', *pcbBuf); /* we got read size, but zero it anyway. */
    uint8_t abSense[32];
    rc = DRVHostBaseScsiCmd(pThis, pbCmd, 12, PDMBLOCKTXDIR_FROM_DEVICE, pvBuf, pcbBuf, abSense, sizeof(abSense), cTimeoutMillies);
    if (rc == VERR_UNRESOLVED_ERROR)
    {
        *pbStat = abSense[2] & 0x0f;
        rc = VINF_SUCCESS;
    }

#elif defined(RT_OS_L4)
    /* Not really ported to L4 yet. */
    rc = VERR_INTERNAL_ERROR;

#elif defined(RT_OS_LINUX)
    int direction;
    struct cdrom_generic_command cgc;
    request_sense sense;

    switch (enmTxDir)
    {
        case PDMBLOCKTXDIR_NONE:
            Assert(*pcbBuf == 0);
            direction = CGC_DATA_NONE;
            break;
        case PDMBLOCKTXDIR_FROM_DEVICE:
            Assert(*pcbBuf != 0);
            /* Make sure that the buffer is clear for commands reading
             * data. The actually received data may be shorter than what
             * we expect, and due to the unreliable feedback about how much
             * data the ioctl actually transferred, it's impossible to
             * prevent that. Returning previous buffer contents may cause
             * security problems inside the guest OS, if users can issue
             * commands to the CDROM device. */
            memset(pvBuf, '\0', *pcbBuf);
            direction = CGC_DATA_READ;
            break;
        case PDMBLOCKTXDIR_TO_DEVICE:
            Assert(*pcbBuf != 0);
            direction = CGC_DATA_WRITE;
            break;
        default:
            AssertMsgFailed(("enmTxDir invalid!\n"));
            direction = CGC_DATA_NONE;
    }
    memset(&cgc, '\0', sizeof(cgc));
    memcpy(cgc.cmd, pbCmd, CDROM_PACKET_SIZE);
    cgc.buffer = (unsigned char *)pvBuf;
    cgc.buflen = *pcbBuf;
    cgc.stat = 0;
    cgc.sense = &sense;
    cgc.data_direction = direction;
    cgc.quiet = false;
    cgc.timeout = cTimeoutMillies;
    rc = ioctl(pThis->FileDevice, CDROM_SEND_PACKET, &cgc);
    if (rc < 0)
    {
        if (errno == EBUSY)
            rc = VERR_PDM_MEDIA_LOCKED;
        else if (errno == ENOSYS)
            rc = VERR_NOT_SUPPORTED;
        else
        {
            if (rc == VERR_ACCESS_DENIED && cgc.sense->sense_key == SCSI_SENSE_NONE)
                cgc.sense->sense_key = SCSI_SENSE_ILLEGAL_REQUEST;
            *pbStat = cgc.sense->sense_key;
            rc = RTErrConvertFromErrno(errno);
            Log2(("%s: error status %d, rc=%Vrc\n", __FUNCTION__, cgc.stat, rc));
        }
    }
    Log2(("%s: after ioctl: cgc.buflen=%d txlen=%d\n", __FUNCTION__, cgc.buflen, *pcbBuf));
    /* The value of cgc.buflen does not reliably reflect the actual amount
     * of data transferred (for packet commands with little data transfer
     * it's 0). So just assume that everything worked ok. */

#elif defined(RT_OS_SOLARIS)
    struct uscsi_cmd usc;
    union scsi_cdb scdb;
    memset(&usc, 0, sizeof(struct uscsi_cmd));
    memset(&scdb, 0, sizeof(scdb));

    switch (enmTxDir)
    {
        case PDMBLOCKTXDIR_NONE:
            Assert(*pcbBuf == 0);
            usc.uscsi_flags = USCSI_READ;
            /* nothing to do */
            break;

        case PDMBLOCKTXDIR_FROM_DEVICE:
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
        case PDMBLOCKTXDIR_TO_DEVICE:
            Assert(*pcbBuf != 0);
            usc.uscsi_flags = USCSI_WRITE;
            break;
        default:
            AssertMsgFailedReturn(("%d\n", enmTxDir), VERR_INTERNAL_ERROR);
    }
    char aSense[32];
    usc.uscsi_flags |= USCSI_RQENABLE;
    usc.uscsi_rqbuf = aSense;
    usc.uscsi_rqlen = 32;
    usc.uscsi_cdb = (caddr_t)&scdb;
    usc.uscsi_cdblen = 12;
    memcpy (usc.uscsi_cdb, pbCmd, usc.uscsi_cdblen);
    usc.uscsi_bufaddr = (caddr_t)pvBuf;
    usc.uscsi_buflen = *pcbBuf;
    usc.uscsi_timeout = (cTimeoutMillies + 999) / 1000;

    /* We need root privileges for user-SCSI under Solaris. */
    rc = ioctl(pThis->FileDevice, USCSICMD, &usc);
    if (rc < 0)
    {
        if (errno == EPERM)
            return VERR_PERMISSION_DENIED;
        if (usc.uscsi_status)
        {
            *pbStat = aSense[2] & 0x0f;
            rc = RTErrConvertFromErrno(errno);
            Log2(("%s: error status. rc=%Vrc\n", __FUNCTION__, rc));
        }
        else
            *pbStat = 0;
    }
    Log2(("%s: after ioctl: residual buflen=%d original buflen=%d\n", __FUNCTION__, usc.uscsi_resid, usc.uscsi_buflen));

#elif defined(RT_OS_WINDOWS)
    int direction;
    struct _REQ
    {
        SCSI_PASS_THROUGH_DIRECT spt;
        uint8_t aSense[18];
    } Req;
    DWORD cbReturned = 0;

    switch (enmTxDir)
    {
        case PDMBLOCKTXDIR_NONE:
            direction = SCSI_IOCTL_DATA_UNSPECIFIED;
            break;
        case PDMBLOCKTXDIR_FROM_DEVICE:
            Assert(*pcbBuf != 0);
            /* Make sure that the buffer is clear for commands reading
             * data. The actually received data may be shorter than what
             * we expect, and due to the unreliable feedback about how much
             * data the ioctl actually transferred, it's impossible to
             * prevent that. Returning previous buffer contents may cause
             * security problems inside the guest OS, if users can issue
             * commands to the CDROM device. */
            memset(pvBuf, '\0', *pcbBuf);
            direction = SCSI_IOCTL_DATA_IN;
            break;
        case PDMBLOCKTXDIR_TO_DEVICE:
            direction = SCSI_IOCTL_DATA_OUT;
            break;
        default:
            AssertMsgFailed(("enmTxDir invalid!\n"));
            direction = SCSI_IOCTL_DATA_UNSPECIFIED;
    }
    memset(&Req, '\0', sizeof(Req));
    Req.spt.Length = sizeof(Req.spt);
    Req.spt.CdbLength = 12;
    memcpy(Req.spt.Cdb, pbCmd, Req.spt.CdbLength);
    Req.spt.DataBuffer = pvBuf;
    Req.spt.DataTransferLength = *pcbBuf;
    Req.spt.DataIn = direction;
    Req.spt.TimeOutValue = (cTimeoutMillies + 999) / 1000; /* Convert to seconds */
    Req.spt.SenseInfoLength = sizeof(Req.aSense);
    Req.spt.SenseInfoOffset = RT_OFFSETOF(struct _REQ, aSense);
    if (DeviceIoControl((HANDLE)pThis->FileDevice, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                        &Req, sizeof(Req), &Req, sizeof(Req), &cbReturned, NULL))
    {
        if (cbReturned > RT_OFFSETOF(struct _REQ, aSense))
            *pbStat = Req.aSense[2] & 0x0f;
        else
            *pbStat = 0;
        /* Windows shares the property of not properly reflecting the actually
         * transferred data size. See above. Assume that everything worked ok. */
        rc = VINF_SUCCESS;
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
    Log2(("%s: scsistatus=%d bytes returned=%d tlength=%d\n", __FUNCTION__, Req.spt.ScsiStatus, cbReturned, Req.spt.DataTransferLength));

#else
# error "Unsupported platform."
#endif
    LogFlow(("%s: rc=%Vrc\n", __FUNCTION__, rc));
    return rc;
}

#if 0
/* These functions would have to go into a seperate solaris binary with
 * the setuid permission set, which would run the user-SCSI ioctl and
 * return the value. BUT... this might be prohibitively slow.
 */
#ifdef RT_OS_SOLARIS
/**
 * Checks if the current user is authorized using Solaris' role-based access control.
 * Made as a seperate function with so that it need not be invoked each time we need
 * to gain root access.
 *
 * @returns VBox error code.
 */
static int solarisCheckUserAuth()
{
    /* Uses Solaris' role-based access control (RBAC).*/
    struct passwd *pPass = getpwuid(getuid());
    if (pPass == NULL || chkauthattr("solaris.device.cdrw", pPass->pw_name) == 0)
        return VERR_PERMISSION_DENIED;

    return VINF_SUCCESS;
}

/**
 * Setuid wrapper to gain root access.
 *
 * @returns VBox error code.
 * @param   pUserID        Pointer to user ID.
 * @param   pEffUserID     Pointer to effective user ID.
 */
static int solarisEnterRootMode(uid_t *pUserID, uid_t *pEffUserID)
{
    /* Increase privilege if required */
    if (*pEffUserID == 0)
        return VINF_SUCCESS;
    if (seteuid(0) == 0)
    {
        *pEffUserID = 0;
        return VINF_SUCCESS;
    }
    return VERR_PERMISSION_DENIED;
}

/**
 * Setuid wrapper to relinquish root access.
 *
 * @returns VBox error code.
 * @param   pUserID        Pointer to user ID.
 * @param   pEffUserID     Pointer to effective user ID.
 */
static int solarisExitRootMode(uid_t *pUserID, uid_t *pEffUserID)
{
    /* Get back to user mode. */
    if (*pEffUserID == 0)
    {
        if (seteuid(*pUserID) == 0)
        {
            *pEffUserID = *pUserID;
            return VINF_SUCCESS;
        }
        return VERR_PERMISSION_DENIED;
    }
    return VINF_SUCCESS;
}
#endif   /* RT_OS_SOLARIS */
#endif

/* -=-=-=-=- driver interface -=-=-=-=- */


/**
 * Construct a host dvd drive driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvHostDvdConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVHOSTBASE pThis = PDMINS2DATA(pDrvIns, PDRVHOSTBASE);
    LogFlow(("drvHostDvdConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0Interval\0Locked\0BIOSVisible\0AttachFailError\0Passthrough\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;


    /*
     * Init instance data.
     */
    int rc = DRVHostBaseInitData(pDrvIns, pCfgHandle, PDMBLOCKTYPE_DVD);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Override stuff.
         */

#ifndef RT_OS_L4 /* Passthrough is not supported on L4 yet */
        bool fPassthrough;
        rc = CFGMR3QueryBool(pCfgHandle, "Passthrough", &fPassthrough);
        if (VBOX_SUCCESS(rc) && fPassthrough)
        {
            pThis->IBlock.pfnSendCmd = drvHostDvdSendCmd;
            /* Passthrough requires opening the device in R/W mode. */
            pThis->fReadOnlyConfig = false;
        }
#endif /* !RT_OS_L4 */

        pThis->IMount.pfnUnmount = drvHostDvdUnmount;
        pThis->pfnDoLock         = drvHostDvdDoLock;
#ifdef USE_MEDIA_POLLING
        if (!fPassthrough)
            pThis->pfnPoll       = drvHostDvdPoll;
        else
            pThis->pfnPoll       = NULL;
#endif
#ifdef RT_OS_LINUX
        pThis->pfnGetMediaSize   = drvHostDvdGetMediaSize;
#endif

        /*
         * 2nd init part.
         */
        rc = DRVHostBaseInitFinish(pThis);
    }

    if (VBOX_FAILURE(rc))
    {
        if (!pThis->fAttachFailError)
        {
            /* Suppressing the attach failure error must not affect the normal
             * DRVHostBaseDestruct, so reset this flag below before leaving. */
            pThis->fKeepInstance = true;
            rc = VINF_SUCCESS;
        }
        DRVHostBaseDestruct(pDrvIns);
        pThis->fKeepInstance = false;
    }

    LogFlow(("drvHostDvdConstruct: returns %Vrc\n", rc));
    return rc;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvHostDVD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostDVD",
    /* pszDescription */
    "Host DVD Block Driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_BLOCK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVHOSTBASE),
    /* pfnConstruct */
    drvHostDvdConstruct,
    /* pfnDestruct */
    DRVHostBaseDestruct,
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
    /* pfnDetach */
    NULL
};

