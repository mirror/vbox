/** $Id$ */
/** @file
 *
 * VBox storage devices:
 * Media implementation for VBox disk container
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
*   Header files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VD
#include <VBox/VBoxHDD-new.h>
#include <VBox/pdmdrv.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/cache.h>

#include "Builtins.h"


/*******************************************************************************
*   Defined types, constants and macros                                        *
*******************************************************************************/

/** Converts a pointer to VDIDISK::IMedia to a PVBOXDISK. */
#define PDMIMEDIA_2_VBOXDISK(pInterface) \
    ( (PVBOXDISK)((uintptr_t)pInterface - RT_OFFSETOF(VBOXDISK, IMedia)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface) \
    ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/** Converts a pointer to PDMDRVINS::IBase to a PVBOXDISK. */
#define PDMIBASE_2_VBOXDISK(pInterface) \
    ( PDMINS2DATA(PDMIBASE_2_DRVINS(pInterface), PVBOXDISK) )

/** Converts a pointer to VBOXDISK::IMediaAsync to a PVBOXDISK. */
#define PDMIMEDIAASYNC_2_VBOXDISK(pInterface) \
    ( (PVBOXDISK)((uintptr_t)pInterface - RT_OFFSETOF(VBOXDISK, IMediaAsync)) )

/** Converts a pointer to VBOXDISK::ITransportAsyncPort to a PVBOXDISK. */
#define PDMITRANSPORTASYNCPORT_2_VBOXDISK(pInterface) \
    ( (PVBOXDISK)((uintptr_t)pInterface - RT_OFFSETOF(VBOXDISK, ITransportAsyncPort)) )

/**
 * Structure for an async I/O task.
 */
typedef struct DRVVDASYNCTASK
{
    /** Callback which is called on completion. */
    PFNVDCOMPLETED pfnCompleted;
    /** Opqaue user data which is passed on completion. */
    void           *pvUser;
    /** Opaque user data the caller passed on transfer initiation. */
    void           *pvUserCaller;
} DRVVDASYNCTASK, *PDRVVDASYNCTASK;

/**
 * VBox disk container media main structure, private part.
 */
typedef struct VBOXDISK
{
    /** The VBox disk container. */
    PVBOXHDD           pDisk;
    /** The media interface. */
    PDMIMEDIA          IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS         pDrvIns;
    /** Flag whether suspend has changed image open mode to read only. */
    bool               fTempReadOnly;
    /** Common structure for the supported error interface. */
    VDINTERFACE        VDIError;
    /** Callback table for error interface. */
    VDINTERFACEERROR   VDIErrorCallbacks;
    /** Common structure for the supported async I/O interface. */
    VDINTERFACE        VDIAsyncIO;
    /** Callback table for async I/O interface. */
    VDINTERFACEASYNCIO VDIAsyncIOCallbacks;
    /** Common structure for the configuration information interface. */
    VDINTERFACE        VDIConfig;
    /** Callback table for the configuration information interface. */
    VDINTERFACECONFIG  VDIConfigCallbacks;
    /** Flag whether opened disk suppports async I/O operations. */
    bool               fAsyncIOSupported;
    /** The async media interface. */
    PDMIMEDIAASYNC           IMediaAsync;
    /** The async media port interface above. */
    PPDMIMEDIAASYNCPORT      pDrvMediaAsyncPort;
    /** Pointer to the asynchronous media driver below. */
    PPDMITRANSPORTASYNC      pDrvTransportAsync;
    /** Async transport port interface. */
    PDMITRANSPORTASYNCPORT   ITransportAsyncPort;
    /** Our cache to reduce allocation overhead. */
    PRTOBJCACHE              pCache;
} VBOXDISK, *PVBOXDISK;

/*******************************************************************************
*   Error reporting callback                                                   *
*******************************************************************************/

static void drvvdErrorCallback(void *pvUser, int rc, RT_SRC_POS_DECL,
                               const char *pszFormat, va_list va)
{
    PPDMDRVINS pDrvIns = (PPDMDRVINS)pvUser;
    pDrvIns->pDrvHlp->pfnVMSetErrorV(pDrvIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
}

/*******************************************************************************
*   VD Async I/O interface implementation                                      *
*******************************************************************************/

static DECLCALLBACK(int) drvvdAsyncIOOpen(void *pvUser, const char *pszLocation, bool fReadonly, void **ppStorage)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    return pDrvVD->pDrvTransportAsync->pfnOpen(pDrvVD->pDrvTransportAsync, pszLocation, fReadonly, ppStorage);
}

static DECLCALLBACK(int) drvvdAsyncIOClose(void *pvUser, void *pStorage)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

   return pDrvVD->pDrvTransportAsync->pfnClose(pDrvVD->pDrvTransportAsync, pStorage);
}

static DECLCALLBACK(int) drvvdAsyncIORead(void *pvUser, void *pStorage, uint64_t uOffset,
                                          size_t cbRead, void *pvBuf, size_t *pcbRead)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

    return pDrvVD->pDrvTransportAsync->pfnReadSynchronous(pDrvVD->pDrvTransportAsync,
                                                          pStorage,
                                                          uOffset, pvBuf, cbRead, pcbRead);
}

static DECLCALLBACK(int) drvvdAsyncIOWrite(void *pvUser, void *pStorage, uint64_t uOffset,
                                           size_t cbWrite, const void *pvBuf, size_t *pcbWritten)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

    return pDrvVD->pDrvTransportAsync->pfnWriteSynchronous(pDrvVD->pDrvTransportAsync,
                                                           pStorage,
                                                           uOffset, pvBuf, cbWrite, pcbWritten);
}

static DECLCALLBACK(int) drvvdAsyncIOFlush(void *pvUser, void *pStorage)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

    return pDrvVD->pDrvTransportAsync->pfnFlushSynchronous(pDrvVD->pDrvTransportAsync, pStorage);
}

static DECLCALLBACK(int) drvvdAsyncIOPrepareRead(void *pvUser, void *pStorage, uint64_t uOffset, void *pvBuf, size_t cbRead, void **ppTask)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

    return pDrvVD->pDrvTransportAsync->pfnPrepareRead(pDrvVD->pDrvTransportAsync, pStorage, uOffset, pvBuf, cbRead, ppTask);
}

static DECLCALLBACK(int) drvvdAsyncIOPrepareWrite(void *pvUser, void *pStorage, uint64_t uOffset, void *pvBuf, size_t cbWrite, void **ppTask)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

    return pDrvVD->pDrvTransportAsync->pfnPrepareWrite(pDrvVD->pDrvTransportAsync, pStorage, uOffset, pvBuf, cbWrite, ppTask);
}

static DECLCALLBACK(int) drvvdAsyncIOTasksSubmit(void *pvUser, void *apTasks[], unsigned cTasks, void *pvUser2,
                                                 void *pvUserCaller, PFNVDCOMPLETED pfnTasksCompleted)
{
    PVBOXDISK pDrvVD = (PVBOXDISK)pvUser;
    PDRVVDASYNCTASK pDrvVDAsyncTask;
    int rc;

    AssertMsg(pDrvVD->pDrvTransportAsync, ("Asynchronous function called but no async transport interface below\n"));

    rc = RTCacheRequest(pDrvVD->pCache, (void **)&pDrvVDAsyncTask);

    if (RT_FAILURE(rc))
        return rc;

    pDrvVDAsyncTask->pfnCompleted = pfnTasksCompleted;
    pDrvVDAsyncTask->pvUser       = pvUser2;
    pDrvVDAsyncTask->pvUserCaller = pvUserCaller;

    return pDrvVD->pDrvTransportAsync->pfnTasksSubmit(pDrvVD->pDrvTransportAsync, apTasks, cTasks, pDrvVDAsyncTask);
}

/*******************************************************************************
*   VD Configuration interface implementation                                  *
*******************************************************************************/

static bool drvvdCfgAreValuesValid(PVDCFGNODE pNode, const char *pszzValid)
{
    return CFGMR3AreValuesValid((PCFGMNODE)pNode, pszzValid);
}

static int drvvdCfgQueryType(PVDCFGNODE pNode, const char *pszName, PVDCFGVALUETYPE penmType)
{
    Assert(VDCFGVALUETYPE_INTEGER == CFGMVALUETYPE_INTEGER);
    Assert(VDCFGVALUETYPE_STRING == CFGMVALUETYPE_STRING);
    Assert(VDCFGVALUETYPE_BYTES == CFGMVALUETYPE_BYTES);
    return CFGMR3QueryType((PCFGMNODE)pNode, pszName, (PCFGMVALUETYPE)penmType);
}

static int drvvdCfgQuerySize(PVDCFGNODE pNode, const char *pszName, size_t *pcb)
{
    return CFGMR3QuerySize((PCFGMNODE)pNode, pszName, pcb);
}

static int drvvdCfgQueryInteger(PVDCFGNODE pNode, const char *pszName, uint64_t *pu64)
{
    return CFGMR3QueryInteger((PCFGMNODE)pNode, pszName, pu64);
}

static int drvvdCfgQueryIntegerDef(PVDCFGNODE pNode, const char *pszName, uint64_t *pu64, uint64_t u64Def)
{
    return CFGMR3QueryInteger((PCFGMNODE)pNode, pszName, pu64);
}

static int drvvdCfgQueryString(PVDCFGNODE pNode, const char *pszName, char *pszString, size_t cchString)
{
    return CFGMR3QueryString((PCFGMNODE)pNode, pszName, pszString, cchString);
}

static int drvvdCfgQueryStringDef(PVDCFGNODE pNode, const char *pszName, char *pszString, size_t cchString, const char *pszDef)
{
    return CFGMR3QueryStringDef((PCFGMNODE)pNode, pszName, pszString, cchString, pszDef);
}

static int drvvdCfgQueryBytes(PVDCFGNODE pNode, const char *pszName, void *pvData, size_t cbData)
{
    return CFGMR3QueryBytes((PCFGMNODE)pNode, pszName, pvData, cbData);
}


/*******************************************************************************
*   Media interface methods                                                    *
*******************************************************************************/

/** @copydoc PDMIMEDIA::pfnRead */
static DECLCALLBACK(int) drvvdRead(PPDMIMEDIA pInterface,
                                   uint64_t off, void *pvBuf, size_t cbRead)
{
    LogFlow(("%s: off=%#llx pvBuf=%p cbRead=%d\n", __FUNCTION__,
             off, pvBuf, cbRead));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDRead(pData->pDisk, off, pvBuf, cbRead);
    if (RT_SUCCESS(rc))
        Log2(("%s: off=%#llx pvBuf=%p cbRead=%d %.*Vhxd\n", __FUNCTION__,
              off, pvBuf, cbRead, cbRead, pvBuf));
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvvdWrite(PPDMIMEDIA pInterface,
                                    uint64_t off, const void *pvBuf,
                                    size_t cbWrite)
{
    LogFlow(("%s: off=%#llx pvBuf=%p cbWrite=%d\n", __FUNCTION__,
             off, pvBuf, cbWrite));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    Log2(("%s: off=%#llx pvBuf=%p cbWrite=%d %.*Vhxd\n", __FUNCTION__,
          off, pvBuf, cbWrite, cbWrite, pvBuf));
    int rc = VDWrite(pData->pDisk, off, pvBuf, cbWrite);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvvdFlush(PPDMIMEDIA pInterface)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDFlush(pData->pDisk);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvvdGetSize(PPDMIMEDIA pInterface)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    uint64_t cb = VDGetSize(pData->pDisk, VD_LAST_IMAGE);
    LogFlow(("%s: returns %#llx (%llu)\n", __FUNCTION__, cb, cb));
    return cb;
}

/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvvdIsReadOnly(PPDMIMEDIA pInterface)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    bool f = VDIsReadOnly(pData->pDisk);
    LogFlow(("%s: returns %d\n", __FUNCTION__, f));
    return f;
}

/** @copydoc PDMIMEDIA::pfnBiosGetPCHSGeometry */
static DECLCALLBACK(int) drvvdBiosGetPCHSGeometry(PPDMIMEDIA pInterface,
                                                  PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDGetPCHSGeometry(pData->pDisk, VD_LAST_IMAGE, pPCHSGeometry);
    if (RT_FAILURE(rc))
    {
        Log(("%s: geometry not available.\n", __FUNCTION__));
        rc = VERR_PDM_GEOMETRY_NOT_SET;
    }
    LogFlow(("%s: returns %Vrc (CHS=%d/%d/%d)\n", __FUNCTION__,
             rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosSetPCHSGeometry */
static DECLCALLBACK(int) drvvdBiosSetPCHSGeometry(PPDMIMEDIA pInterface,
                                                  PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlow(("%s: CHS=%d/%d/%d\n", __FUNCTION__,
             pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDSetPCHSGeometry(pData->pDisk, VD_LAST_IMAGE, pPCHSGeometry);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosGetLCHSGeometry */
static DECLCALLBACK(int) drvvdBiosGetLCHSGeometry(PPDMIMEDIA pInterface,
                                                  PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDGetLCHSGeometry(pData->pDisk, VD_LAST_IMAGE, pLCHSGeometry);
    if (RT_FAILURE(rc))
    {
        Log(("%s: geometry not available.\n", __FUNCTION__));
        rc = VERR_PDM_GEOMETRY_NOT_SET;
    }
    LogFlow(("%s: returns %Vrc (CHS=%d/%d/%d)\n", __FUNCTION__,
             rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosSetLCHSGeometry */
static DECLCALLBACK(int) drvvdBiosSetLCHSGeometry(PPDMIMEDIA pInterface,
                                                  PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlow(("%s: CHS=%d/%d/%d\n", __FUNCTION__,
             pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDSetLCHSGeometry(pData->pDisk, VD_LAST_IMAGE, pLCHSGeometry);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) drvvdGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDGetUuid(pData->pDisk, 0, pUuid);
    LogFlow(("%s: returns %Vrc ({%Vuuid})\n", __FUNCTION__, rc, pUuid));
    return rc;
}

/*******************************************************************************
*   Async Media interface methods                                              *
*******************************************************************************/

static DECLCALLBACK(int) drvvdStartRead(PPDMIMEDIAASYNC pInterface, uint64_t uOffset,
                                        PPDMDATASEG paSeg, unsigned cSeg,
                                        size_t cbRead, void *pvUser)
{
     LogFlow(("%s: uOffset=%#llx paSeg=%#p cSeg=%u cbRead=%d\n pvUser=%#p", __FUNCTION__,
             uOffset, paSeg, cSeg, cbRead, pvUser));
    PVBOXDISK pData = PDMIMEDIAASYNC_2_VBOXDISK(pInterface);
    int rc = VDAsyncRead(pData->pDisk, uOffset, cbRead, paSeg, cSeg, pvUser);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

static DECLCALLBACK(int) drvvdStartWrite(PPDMIMEDIAASYNC pInterface, uint64_t uOffset,
                                         PPDMDATASEG paSeg, unsigned cSeg,
                                         size_t cbWrite, void *pvUser)
{
     LogFlow(("%s: uOffset=%#llx paSeg=%#p cSeg=%u cbWrite=%d\n pvUser=%#p", __FUNCTION__,
             uOffset, paSeg, cSeg, cbWrite, pvUser));
    PVBOXDISK pData = PDMIMEDIAASYNC_2_VBOXDISK(pInterface);
    int rc = VDAsyncWrite(pData->pDisk, uOffset, cbWrite, paSeg, cSeg, pvUser);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/*******************************************************************************
*   Async transport port interface methods                                     *
*******************************************************************************/

static DECLCALLBACK(int) drvvdTasksCompleteNotify(PPDMITRANSPORTASYNCPORT pInterface, void *pvUser)
{
    PVBOXDISK pData = PDMITRANSPORTASYNCPORT_2_VBOXDISK(pInterface);
    PDRVVDASYNCTASK pDrvVDAsyncTask = (PDRVVDASYNCTASK)pvUser;
    int rc = VINF_VDI_ASYNC_IO_FINISHED;

    /* Having a completion callback for a task is not mandatory. */
    if (pDrvVDAsyncTask->pfnCompleted)
        rc = pDrvVDAsyncTask->pfnCompleted(pDrvVDAsyncTask->pvUser);

    /* Check if the request is finished. */
    if (rc == VINF_VDI_ASYNC_IO_FINISHED)
    {
        rc = pData->pDrvMediaAsyncPort->pfnTransferCompleteNotify(pData->pDrvMediaAsyncPort, pDrvVDAsyncTask->pvUserCaller);
    }
    else if (rc == VERR_VDI_ASYNC_IO_IN_PROGRESS)
        rc = VINF_SUCCESS;

    rc = RTCacheInsert(pData->pCache, pDrvVDAsyncTask);
    AssertRC(rc);

    return rc;
}


/*******************************************************************************
*   Base interface methods                                                     *
*******************************************************************************/

/** @copydoc PDMIBASE::pfnQueryInterface */
static DECLCALLBACK(void *) drvvdQueryInterface(PPDMIBASE pInterface,
                                                PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_MEDIA:
            return &pData->IMedia;
        case PDMINTERFACE_MEDIA_ASYNC:
            return pData->fAsyncIOSupported ? &pData->IMediaAsync : NULL;
        case PDMINTERFACE_TRANSPORT_ASYNC_PORT:
            return &pData->ITransportAsyncPort;
        default:
            return NULL;
    }
}


/*******************************************************************************
*   Driver methods                                                             *
*******************************************************************************/


/**
 * Construct a VBox disk media driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle as it's expected
 *                      to be used frequently in this function.
 */
static DECLCALLBACK(int) drvvdConstruct(PPDMDRVINS pDrvIns,
                                        PCFGMNODE pCfgHandle)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    int rc = VINF_SUCCESS;
    char *pszName = NULL;   /**< The path of the disk image file. */
    char *pszFormat = NULL; /**< The format backed to use for this image. */
    bool fReadOnly;         /**< True if the media is readonly. */
    bool fHonorZeroWrites;  /**< True if zero blocks should be written. */

    /*
     * Init the static parts.
     */
    pDrvIns->IBase.pfnQueryInterface    = drvvdQueryInterface;
    pData->pDrvIns                      = pDrvIns;
    pData->fTempReadOnly                = false;
    pData->pDisk                        = NULL;

    /* IMedia */
    pData->IMedia.pfnRead               = drvvdRead;
    pData->IMedia.pfnWrite              = drvvdWrite;
    pData->IMedia.pfnFlush              = drvvdFlush;
    pData->IMedia.pfnGetSize            = drvvdGetSize;
    pData->IMedia.pfnIsReadOnly         = drvvdIsReadOnly;
    pData->IMedia.pfnBiosGetPCHSGeometry = drvvdBiosGetPCHSGeometry;
    pData->IMedia.pfnBiosSetPCHSGeometry = drvvdBiosSetPCHSGeometry;
    pData->IMedia.pfnBiosGetLCHSGeometry = drvvdBiosGetLCHSGeometry;
    pData->IMedia.pfnBiosSetLCHSGeometry = drvvdBiosSetLCHSGeometry;
    pData->IMedia.pfnGetUuid            = drvvdGetUuid;

    /* IMediaAsync */
    pData->IMediaAsync.pfnStartRead       = drvvdStartRead;
    pData->IMediaAsync.pfnStartWrite      = drvvdStartWrite;

    /* ITransportAsyncPort */
    pData->ITransportAsyncPort.pfnTaskCompleteNotify  = drvvdTasksCompleteNotify;

    /* Initialize supported VD interfaces. */
    pData->VDIErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    pData->VDIErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    pData->VDIErrorCallbacks.pfnError     = drvvdErrorCallback;

    rc = VDInterfaceCreate(&pData->VDIError, "DrvVD_VDIError", VDINTERFACETYPE_ERROR,
                           &pData->VDIErrorCallbacks, pDrvIns, NULL);
    AssertRC(rc);

    pData->VDIAsyncIOCallbacks.cbSize                  = sizeof(VDINTERFACEASYNCIO);
    pData->VDIAsyncIOCallbacks.enmInterface            = VDINTERFACETYPE_ASYNCIO;
    pData->VDIAsyncIOCallbacks.pfnOpen                 = drvvdAsyncIOOpen;
    pData->VDIAsyncIOCallbacks.pfnClose                = drvvdAsyncIOClose;
    pData->VDIAsyncIOCallbacks.pfnRead                 = drvvdAsyncIORead;
    pData->VDIAsyncIOCallbacks.pfnWrite                = drvvdAsyncIOWrite;
    pData->VDIAsyncIOCallbacks.pfnFlush                = drvvdAsyncIOFlush;
    pData->VDIAsyncIOCallbacks.pfnPrepareRead          = drvvdAsyncIOPrepareRead;
    pData->VDIAsyncIOCallbacks.pfnPrepareWrite         = drvvdAsyncIOPrepareWrite;
    pData->VDIAsyncIOCallbacks.pfnTasksSubmit          = drvvdAsyncIOTasksSubmit;

    rc = VDInterfaceCreate(&pData->VDIAsyncIO, "DrvVD_AsyncIO", VDINTERFACETYPE_ASYNCIO,
                           &pData->VDIAsyncIOCallbacks, pData, &pData->VDIError);
    AssertRC(rc);

    pData->VDIConfigCallbacks.cbSize                = sizeof(VDINTERFACECONFIG);
    pData->VDIConfigCallbacks.enmInterface          = VDINTERFACETYPE_CONFIG;
    pData->VDIConfigCallbacks.pfnAreValuesValid     = drvvdCfgAreValuesValid;
    pData->VDIConfigCallbacks.pfnQueryType          = drvvdCfgQueryType;
    pData->VDIConfigCallbacks.pfnQuerySize          = drvvdCfgQuerySize;
    pData->VDIConfigCallbacks.pfnQueryInteger       = drvvdCfgQueryInteger;
    pData->VDIConfigCallbacks.pfnQueryIntegerDef    = drvvdCfgQueryIntegerDef;
    pData->VDIConfigCallbacks.pfnQueryString        = drvvdCfgQueryString;
    pData->VDIConfigCallbacks.pfnQueryStringDef     = drvvdCfgQueryStringDef;
    pData->VDIConfigCallbacks.pfnQueryBytes         = drvvdCfgQueryBytes;

    /** @todo TEMP! this isn't really correct - this needs to be made per image,
     * as CFGM needs access to the right configuration node for each image.
     * At the moment this is harmless, as iSCSI can only be used as a base
     * image, and no other backend uses the private data for these callbacks. */
    rc = VDInterfaceCreate(&pData->VDIConfig, "DrvVD_Config", VDINTERFACETYPE_CONFIG,
                           &pData->VDIConfigCallbacks, NULL /**< @todo TEMP */, &pData->VDIAsyncIO);
    AssertRC(rc);

    /* Try to attach async media port interface above.*/
    pData->pDrvMediaAsyncPort = (PPDMIMEDIAASYNCPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_MEDIA_ASYNC_PORT);

    /*
     * Attach the async transport driver below of the device above us implements the
     * async interface.
     */
    if (pData->pDrvMediaAsyncPort)
    {
        /* Try to attach the driver. */
        PPDMIBASE pBase;

        rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBase);
        if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            /*
             * Though the device supports async I/O the backend seems to not support it.
             * Revert to non async I/O.
             */
            pData->pDrvMediaAsyncPort = NULL;
        }
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Failed to attach async transport driver below rc=%Vrc\n", rc));
        }
        else
        {
            /* Success query the async transport interface. */
            pData->pDrvTransportAsync = (PPDMITRANSPORTASYNC)pBase->pfnQueryInterface(pBase, PDMINTERFACE_TRANSPORT_ASYNC);
            if (!pData->pDrvTransportAsync)
            {
                /* Whoops. */
                AssertMsgFailed(("Configuration error: No async transport interface below!\n"));
                return VERR_PDM_MISSING_INTERFACE_ABOVE;
            }
        }
    }

    /*
     * Validate configuration and find all parent images.
     * It's sort of up side down from the image dependency tree.
     */
    unsigned    iLevel = 0;
    PCFGMNODE   pCurNode = pCfgHandle;
    for (;;)
    {
        bool fValid;

        if (pCurNode == pCfgHandle)
        {
            /* Toplevel configuration additionally contains the global image
             * open flags. Some might be converted to per-image flags later. */
            fValid = CFGMR3AreValuesValid(pCurNode,
                                          "Format\0Path\0"
                                          "ReadOnly\0HonorZeroWrites\0");
        }
        else
        {
            /* All other image configurations only contain image name and
             * the format information. */
            fValid = CFGMR3AreValuesValid(pCurNode, "Format\0Path\0");
        }
        if (!fValid)
        {
            rc = PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                     RT_SRC_POS, N_("DrvVD: Configuration error: keys incorrect at level %d"), iLevel);
            break;
        }

        PCFGMNODE pParent = CFGMR3GetChild(pCurNode, "Parent");
        if (!pParent)
            break;
        pCurNode = pParent;
        iLevel++;
    }

    /*
     * Open the images.
     */
    if (RT_SUCCESS(rc))
    {
        /** @todo TEMP! later the iSCSI config callbacks won't be included here */
        rc = VDCreate(&pData->VDIConfig, &pData->pDisk);
        /* Error message is already set correctly. */
    }

    unsigned cImages = iLevel;
    while (pCurNode && RT_SUCCESS(rc))
    {
        /*
         * Read the image configuration.
         */
        rc = CFGMR3QueryStringAlloc(pCurNode, "Path", &pszName);
        if (RT_FAILURE(rc))
        {
            rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                  N_("DrvVD: Configuration error: Querying \"Path\" as string failed"));
            break;
        }

        rc = CFGMR3QueryStringAlloc(pCfgHandle, "Format", &pszFormat);
        if (RT_FAILURE(rc))
        {
            rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                  N_("DrvVD: Configuration error: Querying \"Format\" as string failed"));
            break;
        }

        if (iLevel == 0)
        {
            rc = CFGMR3QueryBool(pCurNode, "ReadOnly", &fReadOnly);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fReadOnly = false;
            else if (RT_FAILURE(rc))
            {
                rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                      N_("DrvVD: Configuration error: Querying \"ReadOnly\" as boolean failed"));
                break;
            }

            rc = CFGMR3QueryBool(pCfgHandle, "HonorZeroWrites", &fHonorZeroWrites);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fHonorZeroWrites = false;
            else if (RT_FAILURE(rc))
            {
                rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                      N_("DrvVD: Configuration error: Querying \"HonorZeroWrites\" as boolean failed"));
                break;
            }
        }
        else
        {
            fReadOnly = true;
            fHonorZeroWrites = false;
        }

        /** @todo TEMP! Later this needs to be done for each image. */
        if (iLevel == cImages)
        {
            PCFGMNODE pCfg = CFGMR3GetChild(pCurNode, "VDConfig");
            pData->VDIConfig.pvUser = pCfg; /**< @todo TEMP! */
        }

        /*
         * Open the image.
         */
        unsigned uOpenFlags;
        if (fReadOnly)
            uOpenFlags = VD_OPEN_FLAGS_READONLY;
        else
            uOpenFlags = VD_OPEN_FLAGS_NORMAL;
        if (fHonorZeroWrites)
            uOpenFlags |= VD_OPEN_FLAGS_HONOR_ZEROES;
        if (pData->pDrvMediaAsyncPort)
            uOpenFlags |= VD_OPEN_FLAGS_ASYNC_IO;

        /** Try to open backend in asyc I/O mode first. */
        rc = VDOpen(pData->pDisk, pszFormat, pszName, uOpenFlags);
        if (rc == VERR_NOT_SUPPORTED)
        {
            /* Seems async I/O is not supported by the backend, open in normal mode. */
            uOpenFlags &= ~VD_OPEN_FLAGS_ASYNC_IO;
            rc = VDOpen(pData->pDisk, pszFormat, pszName, uOpenFlags);
        }

        if (RT_SUCCESS(rc))
            Log(("%s: %d - Opened '%s' in %s mode\n", __FUNCTION__,
                 iLevel, pszName,
                 VDIsReadOnly(pData->pDisk) ? "read-only" : "read-write"));
        else
        {
           AssertMsgFailed(("Failed to open image '%s' rc=%Vrc\n", pszName, rc));
           break;
        }


        MMR3HeapFree(pszName);
        pszName = NULL;
        MMR3HeapFree(pszFormat);
        pszFormat = NULL;

        /* next */
        iLevel--;
        pCurNode = CFGMR3GetParent(pCurNode);
    }

    if (RT_FAILURE(rc))
    {
        if (VALID_PTR(pData->pDisk))
        {
            VDDestroy(pData->pDisk);
            pData->pDisk = NULL;
        }
        if (VALID_PTR(pszName))
            MMR3HeapFree(pszName);
        if (VALID_PTR(pszFormat))
            MMR3HeapFree(pszFormat);
    }

    /*
     * Check for async I/O support. Every opened image has to support
     * it.
     */
    pData->fAsyncIOSupported = true;
    for (unsigned i = 0; i < VDGetCount(pData->pDisk); i++)
    {
        VDBACKENDINFO vdBackendInfo;

        rc = VDBackendInfoSingle(pData->pDisk, i, &vdBackendInfo);
        AssertRC(rc);

        if (vdBackendInfo.uBackendCaps & VD_CAP_ASYNC)
        {
            /*
             * Backend indicates support for at least some files.
             * Check if current file is supported with async I/O)
             */
            rc = VDImageIsAsyncIOSupported(pData->pDisk, i, &pData->fAsyncIOSupported);
            AssertRC(rc);

            /*
             * Check if current image is supported.
             * If not we can stop checking because
             * at least one does not support it.
             */
            if (!pData->fAsyncIOSupported)
                break;
        }
        else
        {
            pData->fAsyncIOSupported = false;
            break;
        }
    }

    /* Create cache if async I/O is supported. */
    if (pData->fAsyncIOSupported)
    {
        rc = RTCacheCreate(&pData->pCache, 0, sizeof(DRVVDASYNCTASK), RTOBJCACHE_PROTECT_INSERT);
        AssertMsg(RT_SUCCESS(rc), ("Failed to create cache rc=%Vrc\n", rc));
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvvdDestruct(PPDMDRVINS pDrvIns)
{
    int rc;
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    LogFlow(("%s:\n", __FUNCTION__));

    if (pData->pCache)
    {
        rc = RTCacheDestroy(pData->pCache);
        AssertRC(rc);
    }
}


/**
 * When the VM has been suspended we'll change the image mode to read-only
 * so that main and others can read the VDIs. This is important when
 * saving state and so forth.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvvdSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    if (!VDIsReadOnly(pData->pDisk))
    {
        unsigned uOpenFlags;
        int rc = VDGetOpenFlags(pData->pDisk, VD_LAST_IMAGE, &uOpenFlags);
        AssertRC(rc);
        uOpenFlags |= VD_OPEN_FLAGS_READONLY;
        rc = VDSetOpenFlags(pData->pDisk, VD_LAST_IMAGE, uOpenFlags);
        AssertRC(rc);
        pData->fTempReadOnly = true;
    }
}

/**
 * Before the VM resumes we'll have to undo the read-only mode change
 * done in drvvdSuspend.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvvdResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    if (pData->fTempReadOnly)
    {
        unsigned uOpenFlags;
        int rc = VDGetOpenFlags(pData->pDisk, VD_LAST_IMAGE, &uOpenFlags);
        AssertRC(rc);
        uOpenFlags &= ~VD_OPEN_FLAGS_READONLY;
        rc = VDSetOpenFlags(pData->pDisk, VD_LAST_IMAGE, uOpenFlags);
        AssertRC(rc);
        pData->fTempReadOnly = false;
    }
}

static DECLCALLBACK(void) drvvdPowerOff(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);

    /*
     * We must close the disk here to ensure that
     * the backend closes all files before the
     * async transport driver is destructed.
     */
    int rc = VDCloseAll(pData->pDisk);
    AssertRC(rc);
}

/**
 * VBox disk container media driver registration record.
 */
const PDMDRVREG g_DrvVD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "VD",
    /* pszDescription */
    "Generic VBox disk media driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(VBOXDISK),
    /* pfnConstruct */
    drvvdConstruct,
    /* pfnDestruct */
    drvvdDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvvdSuspend,
    /* pfnResume */
    drvvdResume,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvvdPowerOff
};
