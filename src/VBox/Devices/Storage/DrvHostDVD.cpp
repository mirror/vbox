/** @file
 *
 * VBox storage devices:
 * Host DVD block driver
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_DVD
#ifdef __LINUX__
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

#elif defined(__WIN__)
# include <Windows.h>
# include <winioctl.h>
# include <ntddscsi.h>

#elif defined(__L4ENV__)

#else /* !__WIN__ nor __LINUX__ nor __L4ENV__ */
# error "Unsupported Platform."
#endif /* !__WIN__ nor __LINUX__ nor __L4ENV__ */

#include <VBox/pdm.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/critsect.h>
#include <VBox/scsi.h>

#include "Builtins.h"
#include "DrvHostBase.h"




/** @copydoc PDMIMOUNT::pfnUnmount */
static DECLCALLBACK(int) drvHostDvdUnmount(PPDMIMOUNT pInterface)
{
     PDRVHOSTBASE pThis = PDMIMOUNT_2_DRVHOSTBASE(pInterface);
     RTCritSectEnter(&pThis->CritSect);

     /*
      * Validate state.
      */
     int rc = VINF_SUCCESS;
     if (!pThis->fLocked)
     {
         /*
          * Eject the disc.
          */
#ifdef __LINUX__
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

#elif defined(__WIN__)
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
#ifdef __LINUX__
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

#elif defined(__WIN__)

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



#ifdef __LINUX__
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
#endif /* __LINUX__ */


#ifdef __LINUX__
/**
 * Do media change polling.
 */
DECLCALLBACK(int) drvHostDvdPoll(PDRVHOSTBASE pThis)
{
    /*
     * Poll for media change.
     */
#ifdef __LINUX__
    bool fMediaPresent = ioctl(pThis->FileDevice, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK;

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
        bool fMediaChanged;
#ifdef __LINUX__
        fMediaChanged = ioctl(pThis->FileDevice, CDROM_MEDIA_CHANGED, CDSL_CURRENT) == 1;

#else
# error "Unsupported platform."
#endif
        if (fMediaChanged)
        {
            int rc;
            LogFlow(("drvHostDVDMediaThread: Media changed!\n"));
            DRVHostBaseMediaNotPresent(pThis);
            rc = DRVHostBaseMediaPresent(pThis);
        }
    }

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}
#endif /* __LINUX__ */


/** @copydoc PDMIBLOCK::pfnSendCmd */
static int drvHostDvdSendCmd(PPDMIBLOCK pInterface, const uint8_t *pbCmd, PDMBLOCKTXDIR enmTxDir, void *pvBuf, size_t *pcbBuf, uint8_t *pbStat, uint32_t cTimeoutMillies)
{
    PDRVHOSTBASE pThis = PDMIBLOCK_2_DRVHOSTBASE(pInterface);
    int direction;
    int rc;
    LogFlow(("%s: cmd[0]=%#04x txdir=%d pcbBuf=%d timeout=%d\n", __FUNCTION__, pbCmd[0], enmTxDir, *pcbBuf, cTimeoutMillies));
#ifdef __LINUX__
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
#elif defined(__WIN__)
    struct _REQ {
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
#elif defined(__L4ENV__)
    /* L4 is silently unsupported. */
#else
# error "Unsupported platform."
#endif
    LogFlow(("%s: rc=%Vrc\n", __FUNCTION__, rc));
    return rc;
}


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
    if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0Interval\0Locked\0BIOSVisible\0Passthrough\0"))
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

#ifndef __L4ENV__ /* Passthrough is not supported on L4 yet */
        bool fPassthrough;
        rc = CFGMR3QueryBool(pCfgHandle, "Passthrough", &fPassthrough);
        if (VBOX_SUCCESS(rc) && fPassthrough)
        {
            pThis->IBlock.pfnSendCmd = drvHostDvdSendCmd;
            /* Passthrough requires opening the device in R/W mode. */
            pThis->fReadOnlyConfig = false;
        }
#endif /* !__L4ENV__ */

        pThis->IMount.pfnUnmount = drvHostDvdUnmount;
        pThis->pfnDoLock         = drvHostDvdDoLock;
#ifdef __LINUX__
        if (!fPassthrough)
            pThis->pfnPoll           = drvHostDvdPoll;
        else
            pThis->pfnPoll           = NULL;
        pThis->pfnGetMediaSize   = drvHostDvdGetMediaSize;
#endif

        /*
         * 2nd init part.
         */
        rc = DRVHostBaseInitFinish(pThis);
        if (VBOX_SUCCESS(rc))
        {
            LogFlow(("drvHostDvdConstruct: return %Vrc\n", rc));
            return rc;
        }
    }
    DRVHostBaseDestruct(pDrvIns);

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

