/** @file
 *
 * VBox storage devices:
 * Host floppy block driver
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_FLOPPY
#ifdef __LINUX__
# include <sys/ioctl.h>
# include <linux/fd.h>
# include <sys/fcntl.h>
# include <errno.h>

# elif defined(__WIN__)
# include <windows.h>
# include <dbt.h>

#elif defined(__L4ENV__)

#else /* !__WIN__ nor __LINUX__ nor __L4ENV__ */
# error "Unsupported Platform."
#endif /* !__WIN__ nor __LINUX__ nor __L4ENV__ */

#include <VBox/pdm.h>
#include <VBox/cfgm.h>
#include <VBox/mm.h>
#include <VBox/err.h>

#include <VBox/log.h>
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



#ifdef __LINUX__
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
    pThis->fReadOnly = !!(DrvStat.flags & FD_DISK_WRITABLE);

    return RTFileSeek(pThis->FileDevice, 0, RTFILE_SEEK_END, pcb);
}
#endif /* __LINUX__ */


#ifdef __LINUX__
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
#endif /* __LINUX__ */


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
#ifdef __LINUX__
        pThis->Base.pfnPoll         = drvHostFloppyPoll;
        pThis->Base.pfnGetMediaSize = drvHostFloppyGetMediaSize;
#endif

        /*
         * 2nd init part.
         */
        rc = DRVHostBaseInitFinish(&pThis->Base);
        if (VBOX_SUCCESS(rc))
        {
            LogFlow(("drvHostFloppyConstruct: return %Vrc\n", rc));
            return rc;
        }
    }
    DRVHostBaseDestruct(pDrvIns);

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

