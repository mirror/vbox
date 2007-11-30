/** @file
 *
 * VBox storage devices:
 * Host Hard Disk media driver
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_HDD
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>

#ifdef RT_OS_WINDOWS
#include <windows.h>
#include <winioctl.h>
#elif RT_OS_LINUX
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#elif RT_OS_SOLARIS
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dkio.h>
#include <stropts.h>
#include <errno.h>
#endif /* !RT_OS_WINDOWS && !RT_OS_LINUX && !RT_OS_SOLARIS */

#include "Builtins.h"


typedef struct DRVHOSTHDD *PDRVHOSTHDD;

typedef struct DRVHOSTHDD
{
    /** File name for the host hard disk. */
    char            *pszPath;
    /** Size of the disk. */
    uint64_t        cbSize;
    /** Disk geometry information, cylinders. */
    uint32_t        cCylinders;
    /** Disk geometry information, heads. */
    uint32_t        cHeads;
    /** Disk geometry information, sectors. */
    uint32_t        cSectors;
    /** Translation mode. */
    PDMBIOSTRANSLATION enmTranslation;
    /** Flag if this drive should refuse write operations. */
    bool            fReadOnly;
    /** File handle for the host hard disk. */
    RTFILE          HostDiskFile;

    /** The media interface. */
    PDMIMEDIA       IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS      pDrvIns;
} DRVHOSTHDD;


/** Converts a pointer to DRVHOSTHDD::IMedia to a PDRVHOSTHDD. */
#define PDMIMEDIA_2_DRVHOSTHDD(pInterface) ( (PDRVHOSTHDD)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTHDD, IMedia)) )


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) drvHostHDDQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTHDD pThis = PDMINS2DATA(pDrvIns, PDRVHOSTHDD);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_MEDIA:
            return &pThis->IMedia;
        default:
            return NULL;
    }
}


/** @copydoc PDMIMEDIA::pfnRead */
static DECLCALLBACK(int) drvHostHDDRead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    int rc = VINF_SUCCESS;
    uint64_t uLBA;
    uint32_t cSectors;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s: off=%lld pvBuf=%p cbRead=%lld\n", __FUNCTION__, off, pvBuf, cbRead));
    Assert(off % 512 == 0);
    Assert(cbRead % 512 == 0);
    uLBA = off / 512;
    cSectors = cbRead / 512;
    /** @todo add partition filtering */
    rc = RTFileReadAt(pThis->HostDiskFile, off, pvBuf, cbRead, NULL);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvHostHDDWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    int rc = VINF_SUCCESS;
    uint64_t uLBA;
    uint32_t cSectors;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s: off=%lld pvBuf=%p cbWrite=%lld\n", __FUNCTION__, off, pvBuf, cbWrite));
    Assert(off % 512 == 0);
    Assert(cbWrite % 512 == 0);
    uLBA = off / 512;
    cSectors = cbWrite / 512;
    /** @todo add partition filtering */
    rc = RTFileWriteAt(pThis->HostDiskFile, off, pvBuf, cbWrite, NULL);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvHostHDDFlush(PPDMIMEDIA pInterface)
{
    int rc = VINF_SUCCESS;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);
    LogFlow(("%s:\n", __FUNCTION__));
    rc = RTFileFlush(pThis->HostDiskFile);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvHostHDDGetSize(PPDMIMEDIA pInterface)
{
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);
    uint64_t cbSize;
    LogFlow(("%s:\n", __FUNCTION__));
    cbSize = pThis->cbSize;
    LogFlow(("%s: returns %lld\n", __FUNCTION__, cbSize));
    return cbSize;
}


/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvHostHDDIsReadOnly(PPDMIMEDIA pInterface)
{
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);
    bool fReadOnly;
    LogFlow(("%s:\n", __FUNCTION__));
    fReadOnly = pThis->fReadOnly;
    LogFlow(("%s: returns %d\n", __FUNCTION__, fReadOnly));
    return fReadOnly;
}


/** @copydoc PDMIMEDIA::pfnBiosGetGeometry */
static DECLCALLBACK(int) drvHostHDDBiosGetGeometry(PPDMIMEDIA pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors)
{
    int rc = VINF_SUCCESS;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s:\n", __FUNCTION__));
    if (pThis->cCylinders != 0)
    {
        *pcCylinders = pThis->cCylinders;
        *pcHeads = pThis->cHeads;
        *pcSectors = pThis->cSectors;
    }
    else
        rc = VERR_PDM_GEOMETRY_NOT_SET;
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnBiosSetGeometry */
static DECLCALLBACK(int) drvHostHDDBiosSetGeometry(PPDMIMEDIA pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors)
{
    int rc = VINF_SUCCESS;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s:\n", __FUNCTION__));
    pThis->cCylinders = cCylinders;
    pThis->cHeads = cHeads;
    pThis->cSectors = cSectors;
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnBiosGetTranslation */
static DECLCALLBACK(int) drvHostHDDBiosGetTranslation(PPDMIMEDIA pInterface, PPDMBIOSTRANSLATION penmTranslation)
{
    int rc = VINF_SUCCESS;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s:\n", __FUNCTION__));
    *penmTranslation = pThis->enmTranslation;
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnBiosSetTranslation */
static DECLCALLBACK(int) drvHostHDDBiosSetTranslation(PPDMIMEDIA pInterface, PDMBIOSTRANSLATION enmTranslation)
{
    int rc = VINF_SUCCESS;
    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s:\n", __FUNCTION__));
    pThis->enmTranslation = enmTranslation;
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnGetUUID */
static DECLCALLBACK(int) drvHostHDDGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    int rc = VINF_SUCCESS;
//    PDRVHOSTHDD pThis = PDMIMEDIA_2_DRVHOSTHDD(pInterface);

    LogFlow(("%s:\n", __FUNCTION__));
    memset(pUuid, '\0', sizeof(*pUuid));
    rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returns %Vrc ({%Vuuid})\n", rc, pUuid));
    return rc;
}



/* -=-=-=-=- driver interface -=-=-=-=- */


/**
 * Construct a host hard disk (raw partition) media driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvHostHDDConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    int rc = VINF_SUCCESS;
    PDRVHOSTHDD pThis = PDMINS2DATA(pDrvIns, PDRVHOSTHDD);
    LogFlow(("%s: iInstance=%d\n", __FUNCTION__, pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0ReadOnly\0"))
        return PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, RT_SRC_POS, N_("RawHDD#%d: configuration keys other than \"Path\" and \"ReadOnly\" present"));

    /*
     * Init instance data.
     */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Path", &pThis->pszPath);
    if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("RawHDD#%d: configuration query for \"Path\" string returned %Vra"), rc);

    rc = CFGMR3QueryBool(pCfgHandle, "ReadOnly", &pThis->fReadOnly);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->fReadOnly = false;
        rc = VINF_SUCCESS;
    }
    else if (VBOX_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("RawHDD#%d: configuration query for \"ReadOnly\" boolean returned %Vra"), rc);

    pDrvIns->IBase.pfnQueryInterface = drvHostHDDQueryInterface;
    pThis->IMedia.pfnRead = drvHostHDDRead;
    pThis->IMedia.pfnWrite = drvHostHDDWrite;
    pThis->IMedia.pfnFlush = drvHostHDDFlush;
    pThis->IMedia.pfnGetSize = drvHostHDDGetSize;
    pThis->IMedia.pfnIsReadOnly = drvHostHDDIsReadOnly;
    pThis->IMedia.pfnBiosGetGeometry = drvHostHDDBiosGetGeometry;
    pThis->IMedia.pfnBiosSetGeometry = drvHostHDDBiosSetGeometry;
    pThis->IMedia.pfnBiosGetTranslation = drvHostHDDBiosGetTranslation;
    pThis->IMedia.pfnBiosSetTranslation = drvHostHDDBiosSetTranslation;
    pThis->IMedia.pfnGetUuid = drvHostHDDGetUuid;

    pThis->cbSize = 0;

    if (pThis->fReadOnly)
        rc = RTFileOpen(&pThis->HostDiskFile, pThis->pszPath, RTFILE_O_READ);
    else
        rc = RTFileOpen(&pThis->HostDiskFile, pThis->pszPath, RTFILE_O_READWRITE);
    if (VBOX_SUCCESS(rc))
    {
        /* Get disk size (in case it's an image file). */
        rc = RTFileGetSize(pThis->HostDiskFile, &pThis->cbSize);
        if (VBOX_FAILURE(rc))
            pThis->cbSize = 0;
    }
    else
    {
        /* Failed to open the raw disk. Specify a proper error message. */
        rc = PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("RawHDD#%d: cannot open file \"%s\", error code %Vrc"), pDrvIns->iInstance, pThis->pszPath, rc);
    }

    if (VBOX_SUCCESS(rc))
    {
        pThis->cCylinders = 0;
        pThis->cHeads = 0;
        pThis->cSectors = 0;
        pThis->enmTranslation = PDMBIOSTRANSLATION_AUTO;
#ifdef RT_OS_WINDOWS
        DISK_GEOMETRY DriveGeo;
        DWORD cbDriveGeo;
        if (DeviceIoControl((HANDLE)pThis->HostDiskFile,
                            IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                            &DriveGeo, sizeof(DriveGeo), &cbDriveGeo, NULL))
        {
            if (DriveGeo.MediaType == FixedMedia)
            {
                pThis->cCylinders = DriveGeo.Cylinders.QuadPart;
                pThis->cHeads = DriveGeo.TracksPerCylinder;
                pThis->cSectors = DriveGeo.SectorsPerTrack;
                if (!pThis->cbSize)
                {
                    /* Windows NT has no IOCTL_DISK_GET_LENGTH_INFORMATION
                     * ioctl. This was added to Windows XP, so use the
                     * available info from DriveGeo. */
                    pThis->cbSize =     DriveGeo.Cylinders.QuadPart
                                    *   DriveGeo.TracksPerCylinder
                                    *   DriveGeo.SectorsPerTrack
                                    *   DriveGeo.BytesPerSector;
                }
            }
            else
                rc = VERR_MEDIA_NOT_RECOGNIZED;
        }
        else
            rc = RTErrConvertFromWin32(GetLastError());
#elif defined(RT_OS_LINUX)
        struct stat DevStat;
        if (!fstat(pThis->HostDiskFile, &DevStat) && S_ISBLK(DevStat.st_mode))
        {
            struct hd_geometry DriveGeo;
            if (!ioctl(pThis->HostDiskFile, HDIO_GETGEO, &DriveGeo))
            {
                pThis->cCylinders = DriveGeo.cylinders;
                pThis->cHeads = DriveGeo.heads;
                pThis->cSectors = DriveGeo.sectors;
                if (!pThis->cbSize)
                {
                    long cBlocks;
                    if (!ioctl(pThis->HostDiskFile, BLKGETSIZE, &cBlocks))
                        pThis->cbSize = (uint64_t)cBlocks * 512;
                    else
                        rc = RTErrConvertFromErrno(errno);
                }
            }
            else
                rc = RTErrConvertFromErrno(errno);
        }
#elif defined(RT_OS_SOLARIS)
        struct stat DevStat;
        if (!fstat(pThis->HostDiskFile, &DevStat) && S_ISBLK(DevStat.st_mode))
        {
            struct dk_geom DriveGeo;
            if (!ioctl(pThis->HostDiskFile, DKIOCGGEOM, &DriveGeo))
            {
                pThis->cCylinders = DriveGeo.dkg_ncyl;
                pThis->cHeads = DriveGeo.dkg_nhead;
                pThis->cSectors = DriveGeo.dkg_nsect;
                if (!pThis->cbSize)
                {
                    struct dk_minfo DriveInfo;
                    if (!ioctl(pThis->HostDiskFile, DKIOCGMEDIAINFO, &DriveInfo))
                        pThis->cbSize = (uint64_t)DriveInfo.dki_lbsize * DriveInfo.dki_capacity;
                    else
                        rc = RTErrConvertFromErrno(errno);
                }
            }
            else
                rc = RTErrConvertFromErrno(errno);
        }
#else
        /** @todo add further host OS geometry detection mechanisms. */
        AssertMsgFailed("Host disk support for this host is unimplemented.\n");
        rc = VERR_NOT_IMPLEMENTED;
#endif
        /* Do geometry cleanup common to all host operating systems.
         * Very important, as Windows guests are very sensitive to odd
         * PCHS settings, and for big disks they consider anything
         * except the standard mapping as odd. */
        if (pThis->cCylinders != 0)
        {
            if (pThis->cSectors == 63 && (pThis->cHeads != 16 || pThis->cCylinders >= 1024))
            {
                /* For big disks, use dummy PCHS values and let the BIOS
                 * select an appropriate LCHS mapping. */
                pThis->cCylinders = pThis->cbSize / 512 / 63 / 16;
                pThis->cHeads = 16;
                pThis->cSectors = 63;
                pThis->enmTranslation = PDMBIOSTRANSLATION_LBA;
            }
            else
                pThis->enmTranslation = PDMBIOSTRANSLATION_NONE;
        }
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}


/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvHostHDDDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTHDD pThis = PDMINS2DATA(pDrvIns, PDRVHOSTHDD);
    LogFlow(("%s: '%s'\n", __FUNCTION__, pThis->pszPath));

    if (pThis->HostDiskFile != NIL_RTFILE)
    {
        RTFileClose(pThis->HostDiskFile);
        pThis->HostDiskFile = NIL_RTFILE;
    }
    if (pThis->pszPath)
        MMR3HeapFree(pThis->pszPath);
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvHostHDD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "HostHDD",
    /* pszDescription */
    "Host Hard Disk Media Driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVHOSTHDD),
    /* pfnConstruct */
    drvHostHDDConstruct,
    /* pfnDestruct */
    drvHostHDDDestruct,
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

