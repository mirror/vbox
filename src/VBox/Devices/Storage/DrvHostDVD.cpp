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
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#ifdef RT_OS_DARWIN
# include <mach/mach.h>
# include <Carbon/Carbon.h>
# include <IOKit/IOKitLib.h>
# include <IOKit/IOCFPlugIn.h>
# include <IOKit/scsi/SCSITaskLib.h>
# include <IOKit/scsi/SCSICommandOperationCodes.h>
# include <IOKit/storage/IOStorageDeviceCharacteristics.h>
# include <mach/mach_error.h>
# define USE_MEDIA_POLLING

#elif defined RT_OS_LINUX
# include <sys/ioctl.h>
# include <linux/version.h>
/* All the following crap is apparently not necessary anymore since Linux
 * version 2.6.29. */
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
/* This is a hack to work around conflicts between these linux kernel headers
 * and the GLIBC tcpip headers. They have different declarations of the 4
 * standard byte order functions. */
#  define _LINUX_BYTEORDER_GENERIC_H
/* This is another hack for not bothering with C++ unfriendly byteswap macros. */
/* Those macros that are needed are defined in the header below. */
#  include "swab.h"
# endif
# include <linux/cdrom.h>
# include <sys/fcntl.h>
# include <errno.h>
# include <limits.h>
# include <iprt/mem.h>
# define USE_MEDIA_POLLING

#elif defined(RT_OS_SOLARIS)
# include <stropts.h>
# include <fcntl.h>
# include <errno.h>
# include <pwd.h>
# include <unistd.h>
# include <syslog.h>
# ifdef VBOX_WITH_SUID_WRAPPER
#  include <auth_attr.h>
# endif
# include <sys/dkio.h>
# include <sys/sockio.h>
# include <sys/scsi/scsi.h>
# define USE_MEDIA_POLLING

#elif defined(RT_OS_WINDOWS)
# pragma warning(disable : 4163)
# define _interlockedbittestandset      they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandset
# define _interlockedbittestandreset    they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandreset
# define _interlockedbittestandset64    they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandset64
# define _interlockedbittestandreset64  they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandreset64
# include <iprt/win/windows.h>
# include <winioctl.h>
# include <ntddscsi.h>
# pragma warning(default : 4163)
# undef _interlockedbittestandset
# undef _interlockedbittestandreset
# undef _interlockedbittestandset64
# undef _interlockedbittestandreset64
# undef USE_MEDIA_POLLING

#elif defined(RT_OS_FREEBSD)
# include <sys/cdefs.h>
# include <sys/param.h>
# include <stdio.h>
# include <cam/cam.h>
# include <cam/cam_ccb.h>
# define USE_MEDIA_POLLING

#else
# error "Unsupported Platform."
#endif

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


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) drvHostDvdDoLock(PDRVHOSTBASE pThis, bool fLock);

#ifdef VBOX_WITH_SUID_WRAPPER
/**
 * Checks if the current user is authorized using Solaris' role-based access control.
 * Made as a separate function with so that it need not be invoked each time we need
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
#endif

/** @interface_method_impl{PDMIMOUNT,pfnUnmount} */
static DECLCALLBACK(int) drvHostDvdUnmount(PPDMIMOUNT pInterface, bool fForce, bool fEject)
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
            drvHostBaseDoLockOs(pThis, false);

        if (fEject)
        {
            /*
             * Eject the disc.
             */
            rc = drvHostBaseEjectOs(pThis);
        }

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
    LogFlow(("drvHostDvdUnmount: returns %Rrc\n", rc));
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
    int rc = drvHostBaseDoLockOs(pThis, fLock);

    LogFlow(("drvHostDvdDoLock(, fLock=%RTbool): returns %Rrc\n", fLock, rc));
    return rc;
}


#ifdef USE_MEDIA_POLLING
/**
 * Do media change polling.
 */
static DECLCALLBACK(int) drvHostDvdPoll(PDRVHOSTBASE pThis)
{
    /*
     * Poll for media change.
     */
    bool fMediaPresent = false;
    bool fMediaChanged = false;
    drvHostBaseQueryMediaStatusOs(pThis, &fMediaChanged, &fMediaPresent);

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


/** @interface_method_impl{PDMIMEDIA,pfnSendCmd} */
static DECLCALLBACK(int) drvHostDvdSendCmd(PPDMIMEDIA pInterface, const uint8_t *pbCmd,
                                           PDMMEDIATXDIR enmTxDir, void *pvBuf, uint32_t *pcbBuf,
                                           uint8_t *pabSense, size_t cbSense, uint32_t cTimeoutMillies)
{
    RT_NOREF(cbSense);
    PDRVHOSTBASE pThis = PDMIMEDIA_2_DRVHOSTBASE(pInterface);
    int rc;
    LogFlow(("%s: cmd[0]=%#04x txdir=%d pcbBuf=%d timeout=%d\n", __FUNCTION__, pbCmd[0], enmTxDir, *pcbBuf, cTimeoutMillies));

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

    LogFlow(("%s: rc=%Rrc\n", __FUNCTION__, rc));
    return rc;
}


/* -=-=-=-=- driver interface -=-=-=-=- */


/** @copydoc FNPDMDRVDESTRUCT */
static DECLCALLBACK(void) drvHostDvdDestruct(PPDMDRVINS pDrvIns)
{
    return DRVHostBaseDestruct(pDrvIns);
}


/**
 * Construct a host dvd drive driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostDvdConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDRVHOSTBASE pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTBASE);
    LogFlow(("drvHostDvdConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Init instance data.
     */
    int rc = DRVHostBaseInitData(pDrvIns, pCfg, "Path\0Interval\0Locked\0BIOSVisible\0AttachFailError\0Passthrough\0",
                                 PDMMEDIATYPE_DVD);
    if (RT_SUCCESS(rc))
    {
        /*
         * Override stuff.
         */
#ifdef RT_OS_LINUX
        pThis->pbDoubleBuffer = (uint8_t *)RTMemAlloc(SCSI_MAX_BUFFER_SIZE);
        if (!pThis->pbDoubleBuffer)
            return VERR_NO_MEMORY;
#endif

        bool fPassthrough;
        rc = CFGMR3QueryBool(pCfg, "Passthrough", &fPassthrough);
        if (RT_SUCCESS(rc) && fPassthrough)
        {
            pThis->IMedia.pfnSendCmd = drvHostDvdSendCmd;
            /* Passthrough requires opening the device in R/W mode. */
            pThis->fReadOnlyConfig = false;
#ifdef VBOX_WITH_SUID_WRAPPER  /* Solaris setuid for Passthrough mode. */
            rc = solarisCheckUserAuth();
            if (RT_FAILURE(rc))
            {
                Log(("DVD: solarisCheckUserAuth failed. Permission denied!\n"));
                return rc;
            }
#endif /* VBOX_WITH_SUID_WRAPPER */
        }

        pThis->IMount.pfnUnmount = drvHostDvdUnmount;
        pThis->pfnDoLock         = drvHostDvdDoLock;
#ifdef USE_MEDIA_POLLING
        if (!fPassthrough)
            pThis->pfnPoll       = drvHostDvdPoll;
        else
            pThis->pfnPoll       = NULL;
#endif

        /*
         * 2nd init part.
         */
        rc = DRVHostBaseInitFinish(pThis);
    }
    if (RT_FAILURE(rc))
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
    sizeof(DRVHOSTBASE),
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

