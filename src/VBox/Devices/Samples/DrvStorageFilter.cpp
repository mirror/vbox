/* $Id$ */
/** @file
 * VBox storage filter driver sample.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_MISC
#include <VBox/vmm/pdmifs.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/log.h>

#include <iprt/uuid.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Storage Filter Driver Instance Data.
 */
typedef struct DRVSTORAGEFILTER
{
    /** @name Interfaces exposed by this driver.
     * @{ */
    PDMIMEDIA           IMedia;
    PDMIMEDIAPORT       IMediaPort;
    PDMIMEDIAASYNC      IMediaAsync;
    PDMIMEDIAASYNCPORT  IMediaAsyncPort;
    /** @}  */

    /** @name Interfaces exposed by the driver below us.
     * @{ */
    PPDMIMEDIA          pIMediaBelow;
    PPDMIMEDIAASYNC     pIMediaAsyncBelow;
    /** @} */

    /** @name Interfaces exposed by the driver/device above us.
     * @{ */
    PPDMIMEDIAPORT      pIMediaPortAbove;
    PPDMIMEDIAASYNCPORT pIMediaAsyncPortAbove;
    /** @} */

    /** If clear, then suppress Async support. */
    bool                fAsyncIOSupported;

    /** @todo implement memfrob. */
} DRVSTORAGEFILTER;
/** Pointer to a storage filter driver instance. */
typedef DRVSTORAGEFILTER *PDRVSTORAGEFILTER;



/*
 *
 * IMediaAsyncPort Implementation.
 *
 */

/** @interface_method_impl{PDMIMEDIAASYNCPORT,pfnTransferCompleteNotify} */
static DECLCALLBACK(int)
drvStorageFltIMediaAsyncPort_TransferCompleteNotify(PPDMIMEDIAASYNCPORT pInterface, void *pvUser, int rcReq)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMediaAsyncPort);
    int rc = pThis->pIMediaAsyncPortAbove->pfnTransferCompleteNotify(pThis->pIMediaAsyncPortAbove, pvUser, rcReq);
    return rc;
}


/*
 *
 * IMediaAsync Implementation.
 *
 */

/** @interface_method_impl{PDMIMEDIAASYNC,pfnStartRead} */
static DECLCALLBACK(int) drvStorageFltIMediaAsync_StartRead(PPDMIMEDIAASYNC pInterface, uint64_t off,
                                                            PCRTSGSEG paSegs, unsigned cSegs, size_t cbRead, void *pvUser)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMediaAsync);
    int rc = pThis->pIMediaAsyncBelow->pfnStartRead(pThis->pIMediaAsyncBelow, off, paSegs, cSegs, cbRead, pvUser);
    return rc;
}

/** @interface_method_impl{PDMIMEDIAASYNC,pfnStartWrite} */
static DECLCALLBACK(int) drvStorageFltIMediaAsync_StartWrite(PPDMIMEDIAASYNC pInterface, uint64_t off,
                                                             PCRTSGSEG paSegs, unsigned cSegs, size_t cbWrite, void *pvUser)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMediaAsync);
    int rc = pThis->pIMediaAsyncBelow->pfnStartWrite(pThis->pIMediaAsyncBelow, off, paSegs, cSegs, cbWrite, pvUser);
    return rc;
}

/** @interface_method_impl{PDMIMEDIAASYNC,pfnStartFlush} */
static DECLCALLBACK(int) drvStorageFltIMediaAsync_StartFlush(PPDMIMEDIAASYNC pInterface, void *pvUser)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMediaAsync);
    int rc = pThis->pIMediaAsyncBelow->pfnStartFlush(pThis->pIMediaAsyncBelow, pvUser);
    return rc;
}


/** @interface_method_impl{PDMIMEDIAASYNC,pfnStartDiscard} */
static DECLCALLBACK(int) drvStorageFltIMediaAsync_StartDiscard(PPDMIMEDIAASYNC pInterface, PCRTRANGE paRanges,
                                                               unsigned cRanges, void *pvUser)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMediaAsync);
    int rc = pThis->pIMediaAsyncBelow->pfnStartDiscard(pThis->pIMediaAsyncBelow, paRanges, cRanges, pvUser);
    return rc;
}


/*
 *
 * IMedia Implementation.
 *
 */

/** @interface_method_impl{PDMIMEDIAPORT,pfnQueryDeviceLocation} */
static DECLCALLBACK(int) drvStorageFltIMediaPort_QueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                                     uint32_t *piInstance, uint32_t *piLUN)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMediaPort);
    int rc = pThis->pIMediaPortAbove->pfnQueryDeviceLocation(pThis->pIMediaPortAbove, ppcszController, piInstance, piLUN);
    return rc;
}


/*
 *
 * IMedia Implementation.
 *
 */

/** @interface_method_impl{PDMIMEDIA,pfnRead} */
static DECLCALLBACK(int) drvStorageFltIMedia_Read(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnRead(pThis->pIMediaBelow, off, pvBuf, cbRead);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnWrite} */
static DECLCALLBACK(int) drvStorageFltIMedia_Write(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnWrite(pThis->pIMediaBelow, off, pvBuf, cbWrite);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnFlush} */
static DECLCALLBACK(int) drvStorageFltIMedia_Flush(PPDMIMEDIA pInterface)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnFlush(pThis->pIMediaBelow);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnMerge} */
static DECLCALLBACK(int) drvStorageFltIMedia_Merge(PPDMIMEDIA pInterface, PFNSIMPLEPROGRESS pfnProgress, void *pvUser)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnMerge(pThis->pIMediaBelow, pfnProgress, pvUser);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnGetSize} */
static DECLCALLBACK(uint64_t) drvStorageFltIMedia_GetSize(PPDMIMEDIA pInterface)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    uint64_t cb = pThis->pIMediaBelow->pfnGetSize(pThis->pIMediaBelow);
    return cb;
}

/** @interface_method_impl{PDMIMEDIA,pfnIsReadOnly} */
static DECLCALLBACK(bool) drvStorageFltIMedia_IsReadOnly(PPDMIMEDIA pInterface)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    bool fRc = pThis->pIMediaBelow->pfnIsReadOnly(pThis->pIMediaBelow);
    return fRc;
}

/** @interface_method_impl{PDMIMEDIA,pfnBiosGetPCHSGeometry} */
static DECLCALLBACK(int) drvStorageFltIMedia_BiosGetPCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnBiosGetPCHSGeometry(pThis->pIMediaBelow, pPCHSGeometry);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnBiosSetPCHSGeometry} */
static DECLCALLBACK(int) drvStorageFltIMedia_BiosSetPCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnBiosSetPCHSGeometry(pThis->pIMediaBelow, pPCHSGeometry);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnBiosGetLCHSGeometry} */
static DECLCALLBACK(int) drvStorageFltIMedia_BiosGetLCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnBiosGetLCHSGeometry(pThis->pIMediaBelow, pLCHSGeometry);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnBiosSetLCHSGeometry} */
static DECLCALLBACK(int) drvStorageFltIMedia_BiosSetLCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnBiosSetLCHSGeometry(pThis->pIMediaBelow, pLCHSGeometry);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnGetUuid} */
static DECLCALLBACK(int) drvStorageFltIMedia_GetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnGetUuid(pThis->pIMediaBelow, pUuid);
    return rc;
}

/** @interface_method_impl{PDMIMEDIA,pfnDiscard} */
static DECLCALLBACK(int) drvStorageFltIMedia_Discard(PPDMIMEDIA pInterface, PCRTRANGE paRanges, unsigned cRanges)
{
    PDRVSTORAGEFILTER pThis = RT_FROM_MEMBER(pInterface, DRVSTORAGEFILTER, IMedia);
    int rc = pThis->pIMediaBelow->pfnDiscard(pThis->pIMediaBelow, paRanges, cRanges);
    return rc;
}


/*
 *
 * IBase Implementation.
 *
 */


static DECLCALLBACK(void *) drvStorageFltIBase_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVSTORAGEFILTER   pThis   = PDMINS_2_DATA(pDrvIns, PDRVSTORAGEFILTER);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    if (pThis->pIMediaBelow)
        PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIA, &pThis->IMedia);
    if (pThis->pIMediaPortAbove)
        PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT, &pThis->IMediaPort);

    if (pThis->fAsyncIOSupported && pThis->pIMediaAsyncBelow)
        PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAASYNC, &pThis->IMediaAsync);
    if (pThis->fAsyncIOSupported && pThis->pIMediaAsyncPortAbove)
        PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAASYNCPORT, &pThis->IMediaAsyncPort);
    return NULL;
}


/*
 *
 * PDMDRVREG Methods
 *
 */


/**
 * Construct a storage filter driver.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvStorageFlt_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVSTORAGEFILTER   pThis   = PDMINS_2_DATA(pDrvIns, PDRVSTORAGEFILTER);

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Initialize the instance data.
     */
    pDrvIns->IBase.pfnQueryInterface     = drvStorageFltIBase_QueryInterface;

    pThis->IMedia.pfnRead                = drvStorageFltIMedia_Read;
    pThis->IMedia.pfnWrite               = drvStorageFltIMedia_Write;
    pThis->IMedia.pfnFlush               = drvStorageFltIMedia_Flush;
    pThis->IMedia.pfnMerge               = drvStorageFltIMedia_Merge;
    pThis->IMedia.pfnGetSize             = drvStorageFltIMedia_GetSize;
    pThis->IMedia.pfnIsReadOnly          = drvStorageFltIMedia_IsReadOnly;
    pThis->IMedia.pfnBiosGetPCHSGeometry = drvStorageFltIMedia_BiosGetPCHSGeometry;
    pThis->IMedia.pfnBiosSetPCHSGeometry = drvStorageFltIMedia_BiosSetPCHSGeometry;
    pThis->IMedia.pfnBiosGetLCHSGeometry = drvStorageFltIMedia_BiosGetLCHSGeometry;
    pThis->IMedia.pfnBiosSetLCHSGeometry = drvStorageFltIMedia_BiosSetLCHSGeometry;
    pThis->IMedia.pfnGetUuid             = drvStorageFltIMedia_GetUuid;
    pThis->IMedia.pfnDiscard             = drvStorageFltIMedia_Discard;

    pThis->IMediaPort.pfnQueryDeviceLocation = drvStorageFltIMediaPort_QueryDeviceLocation;

    pThis->IMediaAsync.pfnStartRead      = drvStorageFltIMediaAsync_StartRead;
    pThis->IMediaAsync.pfnStartWrite     = drvStorageFltIMediaAsync_StartWrite;
    pThis->IMediaAsync.pfnStartFlush     = drvStorageFltIMediaAsync_StartFlush;
    pThis->IMediaAsync.pfnStartDiscard   = drvStorageFltIMediaAsync_StartDiscard;

    pThis->IMediaAsyncPort.pfnTransferCompleteNotify = drvStorageFltIMediaAsyncPort_TransferCompleteNotify;

    /*
     * Validate and read config.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "AsyncIOSupported|", "");

    int rc = CFGMR3QueryBoolDef(pCfg, "AsyncIOSupported", &pThis->fAsyncIOSupported, true);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Query interfaces from the driver/device above us.
     */
    pThis->pIMediaPortAbove      = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIMEDIAPORT);
    pThis->pIMediaAsyncPortAbove = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIMEDIAASYNCPORT);

    /*
     * Attach driver below us.
     */
    PPDMIBASE pIBaseBelow;
    rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pIBaseBelow);
    AssertLogRelRCReturn(rc, rc);

    pThis->pIMediaBelow      = PDMIBASE_QUERY_INTERFACE(pIBaseBelow, PDMIMEDIA);
    pThis->pIMediaAsyncBelow = PDMIBASE_QUERY_INTERFACE(pIBaseBelow, PDMIMEDIAASYNC);

    AssertLogRelReturn(pThis->pIMediaBelow, VERR_PDM_MISSING_INTERFACE_BELOW);

    if (!pThis->pIMediaBelow->pfnDiscard)
        pThis->IMedia.pfnDiscard = NULL;
    if (!pThis->pIMediaAsyncBelow || !pThis->pIMediaAsyncBelow->pfnStartDiscard)
        pThis->IMediaAsync.pfnStartDiscard = NULL;

    return VINF_SUCCESS;
}


/**
 * Storage filter driver registration record.
 */
static const PDMDRVREG g_DrvStorageFilter =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "StorageFilter",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Storage Filter Driver Sample",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVSTORAGEFILTER),
    /* pfnConstruct */
    drvStorageFlt_Construct,
    /* pfnDestruct */
    NULL,
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


/**
 * Register builtin devices.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxDriversRegister(PPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxSampleDriver::VBoxDriversRegister: u32Version=%#x pCallbacks->u32Version=%#x\n",
             u32Version, pCallbacks->u32Version));

    AssertLogRelMsgReturn(pCallbacks->u32Version == PDM_DRVREG_CB_VERSION,
                          ("%#x, expected %#x\n", pCallbacks->u32Version, PDM_DRVREG_CB_VERSION),
                          VERR_VERSION_MISMATCH);

    return pCallbacks->pfnRegister(pCallbacks, &g_DrvStorageFilter);
}

