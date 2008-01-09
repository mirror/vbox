/** @file
 *
 * VBox storage devices:
 * Generic block driver
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_DRV_BLOCK
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>

#include <string.h>

#include "Builtins.h"


/** @def VBOX_PERIODIC_FLUSH
 * Enable support for periodically flushing the VDI to disk. This may prove
 * useful for those nasty problems with the ultra-slow host filesystems.
 * If this is enabled, it can be configured via the CFGM key
 * "VBoxInternal/Devices/piix3ide/0/LUN#<x>/Config/FlushInterval". <x>
 * must be replaced with the correct LUN number of the disk that should
 * do the periodic flushes. The value of the key is the number of bytes
 * written between flushes. A value of 0 (the default) denotes no flushes. */
#define VBOX_PERIODIC_FLUSH

/** @def VBOX_IGNORE_FLUSH
 * Enable support for ignoring VDI flush requests. This can be useful for
 * filesystems that show bad guest IDE write performance (especially with
 * Windows guests). NOTE that this does not disable the flushes caused by
 * the periodic flush cache feature above.
 * If this feature is enabled, it can be configured via the CFGM key
 * "VBoxInternal/Devices/piix3ide/0/LUN#<x>/Config/IgnoreFlush". <x>
 * must be replaced with the correct LUN number of the disk that should
 * ignore flush requests. The value of the key is a boolean. The default
 * is to ignore flushes, i.e. true. */
#define VBOX_IGNORE_FLUSH

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 */
typedef struct DRVBLOCK
{
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Drive type. */
    PDMBLOCKTYPE            enmType;
    /** Locked indicator. */
    bool                    fLocked;
    /** Mountable indicator. */
    bool                    fMountable;
    /** Visible to the BIOS. */
    bool                    fBiosVisible;
#ifdef VBOX_PERIODIC_FLUSH
    /** HACK: Configuration value for number of bytes written after which to flush. */
    uint32_t                cbFlushInterval;
    /** HACK: Current count for the number of bytes written since the last flush. */
    uint32_t                cbDataWritten;
#endif /* VBOX_PERIODIC_FLUSH */
#ifdef VBOX_IGNORE_FLUSH
    /** HACK: Disable flushes for this drive. */
    bool                    fIgnoreFlush;
#endif /* VBOX_IGNORE_FLUSH */
    /** Pointer to the media driver below us.
     * This is NULL if the media is not mounted. */
    PPDMIMEDIA              pDrvMedia;
    /** Pointer to the block port interface above us. */
    PPDMIBLOCKPORT          pDrvBlockPort;
    /** Pointer to the mount notify interface above us. */
    PPDMIMOUNTNOTIFY        pDrvMountNotify;
    /** Our block interface. */
    PDMIBLOCK               IBlock;
    /** Our block interface. */
    PDMIBLOCKBIOS           IBlockBios;
    /** Our mountable interface. */
    PDMIMOUNT               IMount;

    /** Pointer to the async media driver below us.
     * This is NULL if the media is not mounted. */
    PPDMIMEDIAASYNC         pDrvMediaAsync;
    /** Our media async port. */
    PDMIMEDIAASYNCPORT      IMediaAsyncPort;
    /** Pointer to the async block port interface above us. */
    PPDMIBLOCKASYNCPORT     pDrvBlockAsyncPort;
    /** Our async block interface. */
    PDMIBLOCKASYNC          IBlockAsync;

    /** Uuid of the drive. */
    RTUUID                  Uuid;

    /** BIOS PCHS Geometry. */
    PDMMEDIAGEOMETRY        PCHSGeometry;
    /** BIOS LCHS Geometry. */
    PDMMEDIAGEOMETRY        LCHSGeometry;
} DRVBLOCK, *PDRVBLOCK;


/* -=-=-=-=- IBlock -=-=-=-=- */

/** Makes a PDRVBLOCK out of a PPDMIBLOCK. */
#define PDMIBLOCK_2_DRVBLOCK(pInterface)        ( (PDRVBLOCK)((uintptr_t)pInterface - RT_OFFSETOF(DRVBLOCK, IBlock)) )

/** @copydoc PDMIBLOCK::pfnRead */
static DECLCALLBACK(int) drvblockRead(PPDMIBLOCK pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

    int rc = pData->pDrvMedia->pfnRead(pData->pDrvMedia, off, pvBuf, cbRead);
    return rc;
}


/** @copydoc PDMIBLOCK::pfnWrite */
static DECLCALLBACK(int) drvblockWrite(PPDMIBLOCK pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

    int rc = pData->pDrvMedia->pfnWrite(pData->pDrvMedia, off, pvBuf, cbWrite);
#ifdef VBOX_PERIODIC_FLUSH
    if (pData->cbFlushInterval)
    {
        pData->cbDataWritten += cbWrite;
        if (pData->cbDataWritten > pData->cbFlushInterval)
        {
            pData->cbDataWritten = 0;
            pData->pDrvMedia->pfnFlush(pData->pDrvMedia);
        }
    }
#endif /* VBOX_PERIODIC_FLUSH */

    return rc;
}


/** @copydoc PDMIBLOCK::pfnFlush */
static DECLCALLBACK(int) drvblockFlush(PPDMIBLOCK pInterface)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

#ifdef VBOX_IGNORE_FLUSH
    if (pData->fIgnoreFlush)
        return VINF_SUCCESS;
#endif /* VBOX_IGNORE_FLUSH */

    int rc = pData->pDrvMedia->pfnFlush(pData->pDrvMedia);
    if (rc == VERR_NOT_IMPLEMENTED)
        rc = VINF_SUCCESS;
    return rc;
}


/** @copydoc PDMIBLOCK::pfnIsReadOnly */
static DECLCALLBACK(bool) drvblockIsReadOnly(PPDMIBLOCK pInterface)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
        return false;

    bool fRc = pData->pDrvMedia->pfnIsReadOnly(pData->pDrvMedia);
    return fRc;
}


/** @copydoc PDMIBLOCK::pfnGetSize */
static DECLCALLBACK(uint64_t) drvblockGetSize(PPDMIBLOCK pInterface)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
        return 0;

    uint64_t cb = pData->pDrvMedia->pfnGetSize(pData->pDrvMedia);
    LogFlow(("drvblockGetSize: returns %llu\n", cb));
    return cb;
}


/** @copydoc PDMIBLOCK::pfnGetType */
static DECLCALLBACK(PDMBLOCKTYPE) drvblockGetType(PPDMIBLOCK pInterface)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);
    LogFlow(("drvblockGetType: returns %d\n", pData->enmType));
    return pData->enmType;
}


/** @copydoc PDMIBLOCK::pfnGetUuid */
static DECLCALLBACK(int) drvblockGetUuid(PPDMIBLOCK pInterface, PRTUUID pUuid)
{
    PDRVBLOCK pData = PDMIBLOCK_2_DRVBLOCK(pInterface);

    /*
     * Copy the uuid.
     */
    *pUuid = pData->Uuid;
    return VINF_SUCCESS;
}

/* -=-=-=-=- IBlockAsync -=-=-=-=- */

/** Makes a PDRVBLOCK out of a PPDMIBLOCKASYNC. */
#define PDMIBLOCKASYNC_2_DRVBLOCK(pInterface)        ( (PDRVBLOCK)((uintptr_t)pInterface - RT_OFFSETOF(DRVBLOCK, IBlockAsync)) )

/** @copydoc PDMIBLOCKASYNC::pfnRead */
static DECLCALLBACK(int) drvblockAsyncReadStart(PPDMIBLOCKASYNC pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser)
{
    PDRVBLOCK pData = PDMIBLOCKASYNC_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMediaAsync)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

    int rc = pData->pDrvMediaAsync->pfnStartRead(pData->pDrvMediaAsync, off, pvBuf, cbRead, pvUser);
    return rc;
}


/** @copydoc PDMIBLOCKASYNC::pfnWrite */
static DECLCALLBACK(int) drvblockAsyncWriteStart(PPDMIBLOCKASYNC pInterface, uint64_t off, const void *pvBuf, size_t cbWrite, void *pvUser)
{
    PDRVBLOCK pData = PDMIBLOCKASYNC_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMediaAsync)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

    int rc = pData->pDrvMediaAsync->pfnStartWrite(pData->pDrvMediaAsync, off, pvBuf, cbWrite, pvUser);

    return rc;
}

/* -=-=-=-=- IMediaAsyncPort -=-=-=-=- */

/** Makes a PDRVBLOCKASYNC out of a PPDMIMEDIAASYNCPORT. */
#define PDMIMEDIAASYNCPORT_2_DRVBLOCK(pInterface)    ( (PDRVBLOCK((uintptr_t)pInterface - RT_OFFSETOF(DRVBLOCK, IMediaAsyncPort))) )


static DECLCALLBACK(int) drvblockAsyncReadCompleteNotify(PPDMIMEDIAASYNCPORT pInterface, uint64_t uOffset, void *pvBuf, size_t cbRead, void *pvUser)
{
    PDRVBLOCK pData = PDMIMEDIAASYNCPORT_2_DRVBLOCK(pInterface);

    return pData->pDrvBlockAsyncPort->pfnReadCompleteNotify(pData->pDrvBlockAsyncPort, uOffset, pvBuf, cbRead, pvUser);
}

static DECLCALLBACK(int) drvblockAsyncWriteCompleteNotify(PPDMIMEDIAASYNCPORT pInterface, uint64_t uOffset, void *pvBuf, size_t cbWritten, void *pvUser)
{
    PDRVBLOCK pData = PDMIMEDIAASYNCPORT_2_DRVBLOCK(pInterface);

#ifdef VBOX_PERIODIC_FLUSH
    if (pData->cbFlushInterval)
    {
        pData->cbDataWritten += cbWritten;
        if (pData->cbDataWritten > pData->cbFlushInterval)
        {
            pData->cbDataWritten = 0;
            pData->pDrvMedia->pfnFlush(pData->pDrvMedia);
        }
    }
#endif /* VBOX_PERIODIC_FLUSH */

    return pData->pDrvBlockAsyncPort->pfnWriteCompleteNotify(pData->pDrvBlockAsyncPort, uOffset, pvBuf, cbWritten, pvUser);
}

/* -=-=-=-=- IBlockBios -=-=-=-=- */

/** Makes a PDRVBLOCK out of a PPDMIBLOCKBIOS. */
#define PDMIBLOCKBIOS_2_DRVBLOCK(pInterface)    ( (PDRVBLOCK((uintptr_t)pInterface - RT_OFFSETOF(DRVBLOCK, IBlockBios))) )


/** @copydoc PDMIBLOCKBIOS::pfnGetPCHSGeometry */
static DECLCALLBACK(int) drvblockGetPCHSGeometry(PPDMIBLOCKBIOS pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVBLOCK pData = PDMIBLOCKBIOS_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
        return VERR_PDM_MEDIA_NOT_MOUNTED;

    /*
     * Use configured/cached values if present.
     */
    if (    pData->PCHSGeometry.cCylinders > 0
        &&  pData->PCHSGeometry.cHeads > 0
        &&  pData->PCHSGeometry.cSectors > 0)
    {
        *pPCHSGeometry = pData->PCHSGeometry;
        LogFlow(("%s: returns VINF_SUCCESS {%d,%d,%d}\n", __FUNCTION__, pData->PCHSGeometry.cCylinders, pData->PCHSGeometry.cHeads, pData->PCHSGeometry.cSectors));
        return VINF_SUCCESS;
    }

    /*
     * Call media.
     */
    int rc = pData->pDrvMedia->pfnBiosGetPCHSGeometry(pData->pDrvMedia, &pData->PCHSGeometry);

    if (VBOX_SUCCESS(rc))
    {
        *pPCHSGeometry = pData->PCHSGeometry;
        LogFlow(("%s: returns %Vrc {%d,%d,%d}\n", __FUNCTION__, rc, pData->PCHSGeometry.cCylinders, pData->PCHSGeometry.cHeads, pData->PCHSGeometry.cSectors));
    }
    else if (rc == VERR_NOT_IMPLEMENTED)
    {
        rc = VERR_PDM_GEOMETRY_NOT_SET;
        LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    }
    return rc;
}


/** @copydoc PDMIBLOCKBIOS::pfnSetPCHSGeometry */
static DECLCALLBACK(int) drvblockSetPCHSGeometry(PPDMIBLOCKBIOS pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlow(("%s: cCylinders=%d cHeads=%d cSectors=%d\n", __FUNCTION__, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PDRVBLOCK pData = PDMIBLOCKBIOS_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

    /*
     * Call media. Ignore the not implemented return code.
     */
    int rc = pData->pDrvMedia->pfnBiosSetPCHSGeometry(pData->pDrvMedia, pPCHSGeometry);

    if (    VBOX_SUCCESS(rc)
        ||  rc == VERR_NOT_IMPLEMENTED)
    {
        pData->PCHSGeometry = *pPCHSGeometry;
        rc = VINF_SUCCESS;
    }
    return rc;
}


/** @copydoc PDMIBLOCKBIOS::pfnGetLCHSGeometry */
static DECLCALLBACK(int) drvblockGetLCHSGeometry(PPDMIBLOCKBIOS pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVBLOCK pData = PDMIBLOCKBIOS_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
        return VERR_PDM_MEDIA_NOT_MOUNTED;

    /*
     * Use configured/cached values if present.
     */
    if (    pData->LCHSGeometry.cCylinders > 0
        &&  pData->LCHSGeometry.cHeads > 0
        &&  pData->LCHSGeometry.cSectors > 0)
    {
        *pLCHSGeometry = pData->LCHSGeometry;
        LogFlow(("%s: returns VINF_SUCCESS {%d,%d,%d}\n", __FUNCTION__, pData->LCHSGeometry.cCylinders, pData->LCHSGeometry.cHeads, pData->LCHSGeometry.cSectors));
        return VINF_SUCCESS;
    }

    /*
     * Call media.
     */
    int rc = pData->pDrvMedia->pfnBiosGetLCHSGeometry(pData->pDrvMedia, &pData->LCHSGeometry);

    if (VBOX_SUCCESS(rc))
    {
        *pLCHSGeometry = pData->LCHSGeometry;
        LogFlow(("%s: returns %Vrc {%d,%d,%d}\n", __FUNCTION__, rc, pData->LCHSGeometry.cCylinders, pData->LCHSGeometry.cHeads, pData->LCHSGeometry.cSectors));
    }
    else if (rc == VERR_NOT_IMPLEMENTED)
    {
        rc = VERR_PDM_GEOMETRY_NOT_SET;
        LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    }
    return rc;
}


/** @copydoc PDMIBLOCKBIOS::pfnSetLCHSGeometry */
static DECLCALLBACK(int) drvblockSetLCHSGeometry(PPDMIBLOCKBIOS pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlow(("%s: cCylinders=%d cHeads=%d cSectors=%d\n", __FUNCTION__, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PDRVBLOCK pData = PDMIBLOCKBIOS_2_DRVBLOCK(pInterface);

    /*
     * Check the state.
     */
    if (!pData->pDrvMedia)
    {
        AssertMsgFailed(("Invalid state! Not mounted!\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }

    /*
     * Call media. Ignore the not implemented return code.
     */
    int rc = pData->pDrvMedia->pfnBiosSetLCHSGeometry(pData->pDrvMedia, pLCHSGeometry);

    if (    VBOX_SUCCESS(rc)
        ||  rc == VERR_NOT_IMPLEMENTED)
    {
        pData->LCHSGeometry = *pLCHSGeometry;
        rc = VINF_SUCCESS;
    }
    return rc;
}


/** @copydoc PDMIBLOCKBIOS::pfnIsVisible */
static DECLCALLBACK(bool) drvblockIsVisible(PPDMIBLOCKBIOS pInterface)
{
    PDRVBLOCK pData = PDMIBLOCKBIOS_2_DRVBLOCK(pInterface);
    LogFlow(("drvblockIsVisible: returns %d\n", pData->fBiosVisible));
    return pData->fBiosVisible;
}


/** @copydoc PDMIBLOCKBIOS::pfnGetType */
static DECLCALLBACK(PDMBLOCKTYPE) drvblockBiosGetType(PPDMIBLOCKBIOS pInterface)
{
    PDRVBLOCK pData = PDMIBLOCKBIOS_2_DRVBLOCK(pInterface);
    LogFlow(("drvblockBiosGetType: returns %d\n", pData->enmType));
    return pData->enmType;
}



/* -=-=-=-=- IMount -=-=-=-=- */

/** Makes a PDRVBLOCK out of a PPDMIMOUNT. */
#define PDMIMOUNT_2_DRVBLOCK(pInterface)        ( (PDRVBLOCK)((uintptr_t)pInterface - RT_OFFSETOF(DRVBLOCK, IMount)) )


/** @copydoc PDMIMOUNT::pfnMount */
static DECLCALLBACK(int) drvblockMount(PPDMIMOUNT pInterface, const char *pszFilename, const char *pszCoreDriver)
{
    LogFlow(("drvblockMount: pszFilename=%p:{%s} pszCoreDriver=%p:{%s}\n", pszFilename, pszFilename, pszCoreDriver, pszCoreDriver));
    PDRVBLOCK pData = PDMIMOUNT_2_DRVBLOCK(pInterface);

    /*
     * Validate state.
     */
    if (pData->pDrvMedia)
    {
        AssertMsgFailed(("Already mounted\n"));
        return VERR_PDM_MEDIA_MOUNTED;
    }

    /*
     * Prepare configuration.
     */
    if (pszFilename)
    {
        int rc = pData->pDrvIns->pDrvHlp->pfnMountPrepare(pData->pDrvIns, pszFilename, pszCoreDriver);
        if (VBOX_FAILURE(rc))
        {
            Log(("drvblockMount: Prepare failed for \"%s\" rc=%Vrc\n", pszFilename, rc));
            return rc;
        }
    }

    /*
     * Attach the media driver and query it's interface.
     */
    PPDMIBASE pBase;
    int rc = pData->pDrvIns->pDrvHlp->pfnAttach(pData->pDrvIns, &pBase);
    if (VBOX_FAILURE(rc))
    {
        Log(("drvblockMount: Attach failed rc=%Vrc\n", rc));
        return rc;
    }

    pData->pDrvMedia = (PPDMIMEDIA)pBase->pfnQueryInterface(pBase, PDMINTERFACE_MEDIA);
    if (pData->pDrvMedia)
    {
        /*
         * Initialize state.
         */
        pData->fLocked = false;
        pData->PCHSGeometry.cCylinders  = 0;
        pData->PCHSGeometry.cHeads      = 0;
        pData->PCHSGeometry.cSectors    = 0;
        pData->LCHSGeometry.cCylinders  = 0;
        pData->LCHSGeometry.cHeads      = 0;
        pData->LCHSGeometry.cSectors    = 0;
#ifdef VBOX_PERIODIC_FLUSH
        pData->cbDataWritten = 0;
#endif /* VBOX_PERIODIC_FLUSH */

        /*
         * Notify driver/device above us.
         */
        if (pData->pDrvMountNotify)
            pData->pDrvMountNotify->pfnMountNotify(pData->pDrvMountNotify);
        Log(("drvblockMount: Success\n"));
        return VINF_SUCCESS;
    }
    else
        rc = VERR_PDM_MISSING_INTERFACE_BELOW;

    /*
     * Failed, detatch the media driver.
     */
    AssertMsgFailed(("No media interface!\n"));
    int rc2 = pData->pDrvIns->pDrvHlp->pfnDetach(pData->pDrvIns);
    AssertRC(rc2);
    pData->pDrvMedia = NULL;
    return rc;
}


/** @copydoc PDMIMOUNT::pfnUnmount */
static DECLCALLBACK(int) drvblockUnmount(PPDMIMOUNT pInterface, bool fForce)
{
    PDRVBLOCK pData = PDMIMOUNT_2_DRVBLOCK(pInterface);

    /*
     * Validate state.
     */
    if (!pData->pDrvMedia)
    {
        Log(("drvblockUmount: Not mounted\n"));
        return VERR_PDM_MEDIA_NOT_MOUNTED;
    }
    if (pData->fLocked && !fForce)
    {
        Log(("drvblockUmount: Locked\n"));
        return VERR_PDM_MEDIA_LOCKED;
    }

    /* Media is no longer locked even if it was previously. */
    pData->fLocked = false;

    /*
     * Detach the media driver and query it's interface.
     */
    int rc = pData->pDrvIns->pDrvHlp->pfnDetach(pData->pDrvIns);
    if (VBOX_FAILURE(rc))
    {
        Log(("drvblockUnmount: Detach failed rc=%Vrc\n", rc));
        return rc;
    }
    Assert(!pData->pDrvMedia);

    /*
     * Notify driver/device above us.
     */
    if (pData->pDrvMountNotify)
        pData->pDrvMountNotify->pfnUnmountNotify(pData->pDrvMountNotify);
    Log(("drvblockUnmount: success\n"));
    return VINF_SUCCESS;
}


/** @copydoc PDMIMOUNT::pfnIsMounted */
static DECLCALLBACK(bool) drvblockIsMounted(PPDMIMOUNT pInterface)
{
    PDRVBLOCK pData = PDMIMOUNT_2_DRVBLOCK(pInterface);
    return pData->pDrvMedia != NULL;
}

/** @copydoc PDMIMOUNT::pfnLock */
static DECLCALLBACK(int) drvblockLock(PPDMIMOUNT pInterface)
{
    PDRVBLOCK pData = PDMIMOUNT_2_DRVBLOCK(pInterface);
    Log(("drvblockLock: %d -> %d\n", pData->fLocked, true));
    pData->fLocked = true;
    return VINF_SUCCESS;
}

/** @copydoc PDMIMOUNT::pfnUnlock */
static DECLCALLBACK(int) drvblockUnlock(PPDMIMOUNT pInterface)
{
    PDRVBLOCK pData = PDMIMOUNT_2_DRVBLOCK(pInterface);
    Log(("drvblockUnlock: %d -> %d\n", pData->fLocked, false));
    pData->fLocked = false;
    return VINF_SUCCESS;
}

/** @copydoc PDMIMOUNT::pfnIsLocked */
static DECLCALLBACK(bool) drvblockIsLocked(PPDMIMOUNT pInterface)
{
    PDRVBLOCK pData = PDMIMOUNT_2_DRVBLOCK(pInterface);
    return pData->fLocked;
}


/* -=-=-=-=- IBase -=-=-=-=- */

/** @copydoc PDMIBASE::pfnQueryInterface. */
static DECLCALLBACK(void *)  drvblockQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVBLOCK   pData = PDMINS2DATA(pDrvIns, PDRVBLOCK);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_BLOCK:
            return &pData->IBlock;
        case PDMINTERFACE_BLOCK_BIOS:
            return pData->fBiosVisible ? &pData->IBlockBios : NULL;
        case PDMINTERFACE_MOUNT:
            return pData->fMountable ? &pData->IMount : NULL;
        case PDMINTERFACE_BLOCK_ASYNC:
            return pData->pDrvMediaAsync ? &pData->IBlockAsync : NULL;
        case PDMINTERFACE_MEDIA_ASYNC_PORT:
            return &pData->IMediaAsyncPort;
        default:
            return NULL;
    }
}


/* -=-=-=-=- driver interface -=-=-=-=- */

/** @copydoc FNPDMDRVDETACH. */
static DECLCALLBACK(void)  drvblockDetach(PPDMDRVINS pDrvIns)
{
    PDRVBLOCK pData = PDMINS2DATA(pDrvIns, PDRVBLOCK);
    pData->pDrvMedia = NULL;
    pData->pDrvMediaAsync = NULL;
}


/**
 * Construct a block driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvblockConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVBLOCK pData = PDMINS2DATA(pDrvIns, PDRVBLOCK);
    LogFlow(("drvblockConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
#if defined(VBOX_PERIODIC_FLUSH) || defined(VBOX_IGNORE_FLUSH)
    if (!CFGMR3AreValuesValid(pCfgHandle, "Type\0Locked\0BIOSVisible\0AttachFailError\0Cylinders\0Heads\0Sectors\0Mountable\0FlushInterval\0IgnoreFlush\0"))
#else /* !(VBOX_PERIODIC_FLUSH || VBOX_IGNORE_FLUSH) */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Type\0Locked\0BIOSVisible\0AttachFailError\0Cylinders\0Heads\0Sectors\0Mountable\0"))
#endif /* !(VBOX_PERIODIC_FLUSH || VBOX_IGNORE_FLUSH) */
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Initialize most of the data members.
     */
    pData->pDrvIns                          = pDrvIns;

    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvblockQueryInterface;

    /* IBlock. */
    pData->IBlock.pfnRead                   = drvblockRead;
    pData->IBlock.pfnWrite                  = drvblockWrite;
    pData->IBlock.pfnFlush                  = drvblockFlush;
    pData->IBlock.pfnIsReadOnly             = drvblockIsReadOnly;
    pData->IBlock.pfnGetSize                = drvblockGetSize;
    pData->IBlock.pfnGetType                = drvblockGetType;
    pData->IBlock.pfnGetUuid                = drvblockGetUuid;

    /* IBlockBios. */
    pData->IBlockBios.pfnGetPCHSGeometry    = drvblockGetPCHSGeometry;
    pData->IBlockBios.pfnSetPCHSGeometry    = drvblockSetPCHSGeometry;
    pData->IBlockBios.pfnGetLCHSGeometry    = drvblockGetLCHSGeometry;
    pData->IBlockBios.pfnSetLCHSGeometry    = drvblockSetLCHSGeometry;
    pData->IBlockBios.pfnIsVisible          = drvblockIsVisible;
    pData->IBlockBios.pfnGetType            = drvblockBiosGetType;

    /* IMount. */
    pData->IMount.pfnMount                  = drvblockMount;
    pData->IMount.pfnUnmount                = drvblockUnmount;
    pData->IMount.pfnIsMounted              = drvblockIsMounted;
    pData->IMount.pfnLock                   = drvblockLock;
    pData->IMount.pfnUnlock                 = drvblockUnlock;
    pData->IMount.pfnIsLocked               = drvblockIsLocked;

    /* IBlockAsync. */
    pData->IBlockAsync.pfnStartRead         = drvblockAsyncReadStart;
    pData->IBlockAsync.pfnStartWrite        = drvblockAsyncWriteStart;

    /* IMediaAsyncPort. */
    pData->IMediaAsyncPort.pfnReadCompleteNotify  = drvblockAsyncReadCompleteNotify;
    pData->IMediaAsyncPort.pfnWriteCompleteNotify = drvblockAsyncWriteCompleteNotify;

    /*
     * Get the IBlockPort & IMountNotify interfaces of the above driver/device.
     */
    pData->pDrvBlockPort = (PPDMIBLOCKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_BLOCK_PORT);
    if (!pData->pDrvBlockPort)
    {
        AssertMsgFailed(("Configuration error: No block port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /* Try to get the optional async block port interface above. */
    pData->pDrvBlockAsyncPort = (PPDMIBLOCKASYNCPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_BLOCK_ASYNC_PORT);

    pData->pDrvMountNotify = (PPDMIMOUNTNOTIFY)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_MOUNT_NOTIFY);

    /*
     * Query configuration.
     */
    /* type */
    char *psz;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "Type", &psz);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to obtain the type, rc=%Vrc.\n", rc));
        return VERR_PDM_BLOCK_NO_TYPE;
    }
    if (!strcmp(psz, "HardDisk"))
        pData->enmType = PDMBLOCKTYPE_HARD_DISK;
    else if (!strcmp(psz, "DVD"))
        pData->enmType = PDMBLOCKTYPE_DVD;
    else if (!strcmp(psz, "CDROM"))
        pData->enmType = PDMBLOCKTYPE_CDROM;
    else if (!strcmp(psz, "Floppy 2.88"))
        pData->enmType = PDMBLOCKTYPE_FLOPPY_2_88;
    else if (!strcmp(psz, "Floppy 1.44"))
        pData->enmType = PDMBLOCKTYPE_FLOPPY_1_44;
    else if (!strcmp(psz, "Floppy 1.20"))
        pData->enmType = PDMBLOCKTYPE_FLOPPY_1_20;
    else if (!strcmp(psz, "Floppy 720"))
        pData->enmType = PDMBLOCKTYPE_FLOPPY_720;
    else if (!strcmp(psz, "Floppy 360"))
        pData->enmType = PDMBLOCKTYPE_FLOPPY_360;
    else
    {
        AssertMsgFailed(("Configuration error: Unknown type \"%s\".\n", psz));
        MMR3HeapFree(psz);
        return VERR_PDM_BLOCK_UNKNOWN_TYPE;
    }
    Log2(("drvblockConstruct: enmType=%d\n", pData->enmType));
    MMR3HeapFree(psz); psz = NULL;

    /* Mountable */
    rc = CFGMR3QueryBool(pCfgHandle, "Mountable", &pData->fMountable);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fMountable = false;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"Mountable\" resulted in %Vrc.\n", rc));
        return rc;
    }

    /* Locked */
    rc = CFGMR3QueryBool(pCfgHandle, "Locked", &pData->fLocked);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fLocked = false;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"Locked\" resulted in %Vrc.\n", rc));
        return rc;
    }

    /* BIOS visible */
    rc = CFGMR3QueryBool(pCfgHandle, "BIOSVisible", &pData->fBiosVisible);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fBiosVisible = true;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"BIOSVisible\" resulted in %Vrc.\n", rc));
        return rc;
    }

    /** @todo AttachFailError is currently completely ignored. */

    /* Cylinders */
    rc = CFGMR3QueryU32(pCfgHandle, "Cylinders", &pData->LCHSGeometry.cCylinders);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->LCHSGeometry.cCylinders = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"Cylinders\" resulted in %Vrc.\n", rc));
        return rc;
    }

    /* Heads */
    rc = CFGMR3QueryU32(pCfgHandle, "Heads", &pData->LCHSGeometry.cHeads);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->LCHSGeometry.cHeads = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"Heads\" resulted in %Vrc.\n", rc));
        return rc;
    }

    /* Sectors */
    rc = CFGMR3QueryU32(pCfgHandle, "Sectors", &pData->LCHSGeometry.cSectors);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->LCHSGeometry.cSectors = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"Sectors\" resulted in %Vrc.\n", rc));
        return rc;
    }

    /* Uuid */
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Uuid", &psz);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        RTUuidClear(&pData->Uuid);
    else if (VBOX_SUCCESS(rc))
    {
        rc = RTUuidFromStr(&pData->Uuid, psz);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("Configuration error: Uuid from string failed on \"%s\", rc=%Vrc.\n", psz, rc));
            MMR3HeapFree(psz);
            return rc;
        }
        MMR3HeapFree(psz); psz = NULL;
    }
    else
    {
        AssertMsgFailed(("Configuration error: Failed to obtain the type, rc=%Vrc.\n", rc));
        return VERR_PDM_BLOCK_NO_TYPE;
    }

#ifdef VBOX_PERIODIC_FLUSH
    rc = CFGMR3QueryU32(pCfgHandle, "FlushInterval", &pData->cbFlushInterval);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->cbFlushInterval = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"FlushInterval\" resulted in %Vrc.\n", rc));
        return rc;
    }
#endif /* VBOX_PERIODIC_FLUSH */

#ifdef VBOX_IGNORE_FLUSH
    rc = CFGMR3QueryBool(pCfgHandle, "IgnoreFlush", &pData->fIgnoreFlush);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fIgnoreFlush = true;     /* The default is to ignore flushes. */
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Query \"IgnoreFlush\" resulted in %Vrc.\n", rc));
        return rc;
    }
#endif /* VBOX_IGNORE_FLUSH */

    /*
     * Try attach driver below and query it's media interface.
     */
    PPDMIBASE pBase;
    rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBase);
    if (    rc == VERR_PDM_NO_ATTACHED_DRIVER
        &&  pData->enmType != PDMBLOCKTYPE_HARD_DISK)
        return VINF_SUCCESS;
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to attach driver below us! rc=%Vra\n", rc));
        return rc;
    }
    pData->pDrvMedia = (PPDMIMEDIA)pBase->pfnQueryInterface(pBase, PDMINTERFACE_MEDIA);
    if (!pData->pDrvMedia)
    {
            AssertMsgFailed(("Configuration error: No media or async media interface below!\n"));
            return VERR_PDM_MISSING_INTERFACE_BELOW;
    }

    /* Try to get the optional async interface. */
    pData->pDrvMediaAsync = (PPDMIMEDIAASYNC)pBase->pfnQueryInterface(pBase, PDMINTERFACE_MEDIA_ASYNC);

    if (RTUuidIsNull(&pData->Uuid))
    {
        if (pData->enmType == PDMBLOCKTYPE_HARD_DISK)
            pData->pDrvMedia->pfnGetUuid(pData->pDrvMedia, &pData->Uuid);
    }

    return VINF_SUCCESS;
}


/**
 * Block driver registration record.
 */
const PDMDRVREG g_DrvBlock =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "Block",
    /* pszDescription */
    "Generic block driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_BLOCK,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVBLOCK),
    /* pfnConstruct */
    drvblockConstruct,
    /* pfnDestruct */
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
    /* pfnDetach */
    drvblockDetach
};



