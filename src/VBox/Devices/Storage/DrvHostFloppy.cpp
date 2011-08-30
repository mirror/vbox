/** @file
 *
 * VBox storage devices:
 * Host floppy block driver
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_FLOPPY
#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
# include <linux/fd.h>
# include <sys/fcntl.h>
# include <errno.h>

# elif defined(RT_OS_WINDOWS)
# include <windows.h>
# include <dbt.h>

#elif defined(RT_OS_L4)

#else /* !RT_OS_WINDOWS nor RT_OS_LINUX nor RT_OS_L4 */
# error "Unsupported Platform."
#endif /* !RT_OS_WINDOWS nor RT_OS_LINUX nor RT_OS_L4 */

#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/asm.h>
#include <iprt/critsect.h>

#include "VBoxDD.h"
#include "DrvHostBase.h"


/**
 * Floppy driver instance data.
 */
typedef struct DRVHOSTFLOPPY
{
    DRVHOSTBASE     Base;
    /** Previous poll status. */
    bool            fPrevDiskIn;

} DRVHOSTFLOPPY, *PDRVHOSTFLOPPY;



#ifdef RT_OS_WINDOWS
/**
 * Get media size - needs a special IOCTL.
 *
 * @param   pThis   The instance data.
 */
static DECLCALLBACK(int) drvHostFloppyGetMediaSize(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    DISK_GEOMETRY   geom;
    DWORD           cbBytesReturned;
    int             rc;
    int             cbSectors;

    memset(&geom, 0, sizeof(geom));
    rc = DeviceIoControl((HANDLE)RTFileToNative(pThis->hFileDevice), IOCTL_DISK_GET_DRIVE_GEOMETRY,
                         NULL, 0, &geom, sizeof(geom), &cbBytesReturned,  NULL);
    if (rc) {
        cbSectors = geom.Cylinders.QuadPart * geom.TracksPerCylinder * geom.SectorsPerTrack;
        *pcb = cbSectors * geom.BytesPerSector;
        rc = VINF_SUCCESS;
    }
    else
    {
        DWORD   dwLastError;

        dwLastError = GetLastError();
        rc = RTErrConvertFromWin32(dwLastError);
        Log(("DrvHostFloppy: IOCTL_DISK_GET_DRIVE_GEOMETRY(%s) failed, LastError=%d rc=%Rrc\n", 
             pThis->pszDevice, dwLastError, rc));
        return rc;
    }

    return rc;
}
#endif /* RT_OS_WINDOWS */

#ifdef RT_OS_LINUX
/**
 * Get media size and do change processing.
 *
 * @param   pThis   The instance data.
 */
static DECLCALLBACK(int) drvHostFloppyGetMediaSize(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    int rc = ioctl(RTFileToNative(pThis->hFileDevice), FDFLUSH);
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

    return RTFileSeek(pThis->hFileDevice, 0, RTFILE_SEEK_END, pcb);
}
#endif /* RT_OS_LINUX */


#ifdef RT_OS_LINUX
/**
 * This thread will periodically poll the Floppy for media presence.
 *
 * @returns Ignored.
 * @param   ThreadSelf  Handle of this thread. Ignored.
 * @param   pvUser      Pointer to the driver instance structure.
 */
static DECLCALLBACK(int) drvHostFloppyPoll(PDRVHOSTBASE pThis)
{
    PDRVHOSTFLOPPY          pThisFloppy = (PDRVHOSTFLOPPY)pThis;
    floppy_drive_struct     DrvStat;
    int rc = ioctl(RTFileToNative(pThis->hFileDevice), FDPOLLDRVSTAT, &DrvStat);
    if (rc)
        return RTErrConvertFromErrno(errno);

    RTCritSectEnter(&pThis->CritSect);
    bool fDiskIn = !(DrvStat.flags & (FD_VERIFY | FD_DISK_NEWCHANGE));
    if (    fDiskIn
        &&  !pThisFloppy->fPrevDiskIn)
    {
        if (pThis->fMediaPresent)
            DRVHostBaseMediaNotPresent(pThis);
        rc = DRVHostBaseMediaPresent(pThis);
        if (RT_FAILURE(rc))
        {
            pThisFloppy->fPrevDiskIn = fDiskIn;
            RTCritSectLeave(&pThis->CritSect);
            return rc;
        }
    }

    if (    !fDiskIn
        &&  pThisFloppy->fPrevDiskIn
        &&  pThis->fMediaPresent)
        DRVHostBaseMediaNotPresent(pThis);
    pThisFloppy->fPrevDiskIn = fDiskIn;

    RTCritSectLeave(&pThis->CritSect);
    return VINF_SUCCESS;
}
#endif /* RT_OS_LINUX */


/**
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostFloppyConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVHOSTFLOPPY pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTFLOPPY);
    LogFlow(("drvHostFloppyConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "Path\0ReadOnly\0Interval\0Locked\0BIOSVisible\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init instance data.
     */
    int rc = DRVHostBaseInitData(pDrvIns, pCfg, PDMBLOCKTYPE_FLOPPY_1_44);
    if (RT_SUCCESS(rc))
    {
        /*
         * Override stuff.
         */
#ifdef RT_OS_WINDOWS
        pThis->Base.pfnGetMediaSize = drvHostFloppyGetMediaSize;
#endif
#ifdef RT_OS_LINUX
        pThis->Base.pfnPoll         = drvHostFloppyPoll;
        pThis->Base.pfnGetMediaSize = drvHostFloppyGetMediaSize;
#endif

        /*
         * 2nd init part.
         */
        rc = DRVHostBaseInitFinish(&pThis->Base);
    }
    if (RT_FAILURE(rc))
    {
        if (!pThis->Base.fAttachFailError)
        {
            /* Suppressing the attach failure error must not affect the normal
             * DRVHostBaseDestruct, so reset this flag below before leaving. */
            pThis->Base.fKeepInstance = true;
            rc = VINF_SUCCESS;
        }
        DRVHostBaseDestruct(pDrvIns);
        pThis->Base.fKeepInstance = false;
    }

    LogFlow(("drvHostFloppyConstruct: returns %Rrc\n", rc));
    return rc;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvHostFloppy =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "HostFloppy",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Host Floppy Block Driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_BLOCK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVHOSTFLOPPY),
    /* pfnConstruct */
    drvHostFloppyConstruct,
    /* pfnDestruct */
    DRVHostBaseDestruct,
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

