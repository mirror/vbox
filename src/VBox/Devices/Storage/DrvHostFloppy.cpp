/** @file
 *
 * VBox storage devices:
 * Host floppy block driver
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/asm.h>
#include <iprt/critsect.h>

#include "Builtins.h"
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



#ifdef RT_OS_LINUX
/**
 * Get media size and do change processing.
 *
 * @param   pThis   The instance data.
 */
static DECLCALLBACK(int) drvHostFloppyGetMediaSize(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    int rc = ioctl(pThis->FileDevice, FDFLUSH);
    if (rc)
    {
        rc = RTErrConvertFromErrno (errno);
        Log(("DrvHostFloppy: FDFLUSH ioctl(%s) failed, errno=%d rc=%Vrc\n", pThis->pszDevice, errno, rc));
        return rc;
    }

    floppy_drive_struct DrvStat;
    rc = ioctl(pThis->FileDevice, FDGETDRVSTAT, &DrvStat);
    if (rc)
    {
        rc = RTErrConvertFromErrno(errno);
        Log(("DrvHostFloppy: FDGETDRVSTAT ioctl(%s) failed, errno=%d rc=%Vrc\n", pThis->pszDevice, errno, rc));
        return rc;
    }
    pThis->fReadOnly = !(DrvStat.flags & FD_DISK_WRITABLE);

    return RTFileSeek(pThis->FileDevice, 0, RTFILE_SEEK_END, pcb);
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
    int rc = ioctl(pThis->FileDevice, FDPOLLDRVSTAT, &DrvStat);
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
        if (VBOX_FAILURE(rc))
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
static DECLCALLBACK(int) drvHostFloppyConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVHOSTFLOPPY pThis = PDMINS2DATA(pDrvIns, PDRVHOSTFLOPPY);
    LogFlow(("drvHostFloppyConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0ReadOnly\0Interval\0Locked\0BIOSVisible\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init instance data.
     */
    int rc = DRVHostBaseInitData(pDrvIns, pCfgHandle, PDMBLOCKTYPE_FLOPPY_1_44);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Override stuff.
         */
#ifdef RT_OS_LINUX
        pThis->Base.pfnPoll         = drvHostFloppyPoll;
        pThis->Base.pfnGetMediaSize = drvHostFloppyGetMediaSize;
#endif

        /*
         * 2nd init part.
         */
        rc = DRVHostBaseInitFinish(&pThis->Base);
    }
    if (VBOX_FAILURE(rc))
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

    LogFlow(("drvHostFloppyConstruct: returns %Vrc\n", rc));
    return rc;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvHostFloppy =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostFloppy",
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

