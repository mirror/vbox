/* $Id$ */
/** @file
 * VBox input devices: Mouse queue driver
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_MOUSE_QUEUE
#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Mouse queue driver instance data.
 *
 * @implements  PDMIMOUSECONNECTOR
 * @implements  PDMIMOUSEPORT
 */
typedef struct DRVMOUSEQUEUE
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the mouse port interface of the driver/device above us. */
    PPDMIMOUSEPORT              pUpPort;
    /** Pointer to the mouse port interface of the driver/device below us. */
    PPDMIMOUSECONNECTOR         pDownConnector;
    /** Our mouse connector interface. */
    PDMIMOUSECONNECTOR          IConnector;
    /** Our mouse port interface. */
    PDMIMOUSEPORT               IPort;
    /** The queue handle. */
    PPDMQUEUE                   pQueue;
    /** Discard input when this flag is set.
     * We only accept input when the VM is running. */
    bool                        fInactive;
} DRVMOUSEQUEUE, *PDRVMOUSEQUEUE;


/**
 * Mouse queue item.
 */
typedef struct DRVMOUSEQUEUEITEM
{
    /** The core part owned by the queue manager. */
    PDMQUEUEITEMCORE    Core;
    uint32_t            fAbs;
    int32_t             iDeltaX;
    int32_t             iDeltaY;
    int32_t             iDeltaZ;
    int32_t             iDeltaW;
    uint32_t            fButtonStates;
    uint32_t            uX;
    uint32_t            uY;
} DRVMOUSEQUEUEITEM, *PDRVMOUSEQUEUEITEM;



/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvMouseQueueQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMOUSEQUEUE  pThis   = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUSEPORT, &pThis->IPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUSECONNECTOR, &pThis->IConnector);
    return NULL;
}


/* -=-=-=-=- IMousePort -=-=-=-=- */

/** Converts a pointer to DRVMOUSEQUEUE::Port to a DRVMOUSEQUEUE pointer. */
#define IMOUSEPORT_2_DRVMOUSEQUEUE(pInterface) ( (PDRVMOUSEQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVMOUSEQUEUE, IPort)) )


/**
 * @interface_method_impl{PDMIMOUSEPORT,pfnPutEvent}
 */
static DECLCALLBACK(int) drvMouseQueuePutEvent(PPDMIMOUSEPORT pInterface, int32_t iDeltaX, int32_t iDeltaY, int32_t iDeltaZ, int32_t iDeltaW, uint32_t fButtonStates)
{
    PDRVMOUSEQUEUE pDrv = IMOUSEPORT_2_DRVMOUSEQUEUE(pInterface);
    if (pDrv->fInactive)
        return VINF_SUCCESS;

    PDRVMOUSEQUEUEITEM pItem = (PDRVMOUSEQUEUEITEM)PDMQueueAlloc(pDrv->pQueue);
    if (pItem)
    {
        pItem->fAbs = 0;
        pItem->iDeltaX = iDeltaX;
        pItem->iDeltaY = iDeltaY;
        pItem->iDeltaZ = iDeltaZ;
        pItem->iDeltaW = iDeltaW;
        pItem->fButtonStates = fButtonStates;
        pItem->uX = 0;
        pItem->uY = 0;
        PDMQueueInsert(pDrv->pQueue, &pItem->Core);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_QUEUE_ITEMS;
}

/**
 * @interface_method_impl{PDMIMOUSEPORT,pfnPutEventAbs}
 */
static DECLCALLBACK(int) drvMouseQueuePutEventAbs(PPDMIMOUSEPORT pInterface, uint32_t uX, uint32_t uY, int32_t iDeltaZ, int32_t iDeltaW, uint32_t fButtonStates)
{
    PDRVMOUSEQUEUE pDrv = IMOUSEPORT_2_DRVMOUSEQUEUE(pInterface);
    if (pDrv->fInactive)
        return VINF_SUCCESS;

    PDRVMOUSEQUEUEITEM pItem = (PDRVMOUSEQUEUEITEM)PDMQueueAlloc(pDrv->pQueue);
    if (pItem)
    {
        pItem->fAbs = 1;
        pItem->iDeltaX = 0;
        pItem->iDeltaY = 0;
        pItem->iDeltaZ = iDeltaZ;
        pItem->iDeltaW = iDeltaW;
        pItem->fButtonStates = fButtonStates;
        pItem->uX = uX;
        pItem->uY = uY;
        PDMQueueInsert(pDrv->pQueue, &pItem->Core);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_QUEUE_ITEMS;
}


/* -=-=-=-=- IConnector -=-=-=-=- */

#define PPDMIMOUSECONNECTOR_2_DRVMOUSEQUEUE(pInterface) ( (PDRVMOUSEQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVMOUSEQUEUE, IConnector)) )


/**
 * Pass absolute mode status changes from the guest through to the frontend
 * driver.
 *
 * @param   pInterface  Pointer to the mouse connector interface structure.
 * @param   fAbs        The new absolute mode state.
 */
static DECLCALLBACK(void) drvMousePassThruReportModes(PPDMIMOUSECONNECTOR pInterface, bool fRel, bool fAbs)
{
    PDRVMOUSEQUEUE pDrv = PPDMIMOUSECONNECTOR_2_DRVMOUSEQUEUE(pInterface);
    pDrv->pDownConnector->pfnReportModes(pDrv->pDownConnector, fRel, fAbs);
}



/* -=-=-=-=- queue -=-=-=-=- */

/**
 * Queue callback for processing a queued item.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDrvIns         The driver instance.
 * @param   pItemCore       Pointer to the queue item to process.
 */
static DECLCALLBACK(bool) drvMouseQueueConsumer(PPDMDRVINS pDrvIns, PPDMQUEUEITEMCORE pItemCore)
{
    PDRVMOUSEQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    PDRVMOUSEQUEUEITEM    pItem = (PDRVMOUSEQUEUEITEM)pItemCore;
    int rc;
    if (!pItem->fAbs)
        rc = pThis->pUpPort->pfnPutEvent(pThis->pUpPort, pItem->iDeltaX, pItem->iDeltaY, pItem->iDeltaZ, pItem->iDeltaW, pItem->fButtonStates);
    else
        rc = pThis->pUpPort->pfnPutEventAbs(pThis->pUpPort, pItem->uX,   pItem->uY,      pItem->iDeltaZ, pItem->iDeltaW, pItem->fButtonStates);
    return RT_SUCCESS(rc);
}


/* -=-=-=-=- driver interface -=-=-=-=- */

/**
 * Power On notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void) drvMouseQueuePowerOn(PPDMDRVINS pDrvIns)
{
    PDRVMOUSEQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    pThis->fInactive = false;
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvMouseQueueReset(PPDMDRVINS pDrvIns)
{
    //PDRVKBDQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    /** @todo purge the queue on reset. */
}


/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvMouseQueueSuspend(PPDMDRVINS pDrvIns)
{
    PDRVMOUSEQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    pThis->fInactive = true;
}


/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvMouseQueueResume(PPDMDRVINS pDrvIns)
{
    PDRVMOUSEQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    pThis->fInactive = false;
}


/**
 * Power Off notification.
 *
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void) drvMouseQueuePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVMOUSEQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    pThis->fInactive = true;
}


/**
 * Construct a mouse driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvMouseQueueConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVMOUSEQUEUE pDrv = PDMINS_2_DATA(pDrvIns, PDRVMOUSEQUEUE);
    LogFlow(("drvMouseQueueConstruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "QueueSize\0Interval\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init basic data members and interfaces.
     */
    pDrv->fInactive                         = true;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvMouseQueueQueryInterface;
    /* IMouseConnector. */
    pDrv->IConnector.pfnReportModes         = drvMousePassThruReportModes;
    /* IMousePort. */
    pDrv->IPort.pfnPutEvent                 = drvMouseQueuePutEvent;
    pDrv->IPort.pfnPutEventAbs              = drvMouseQueuePutEventAbs;

    /*
     * Get the IMousePort interface of the above driver/device.
     */
    pDrv->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIMOUSEPORT);
    if (!pDrv->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No mouse port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Attach driver below and query it's connector interface.
     */
    PPDMIBASE pDownBase;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pDownBase);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to attach driver below us! rc=%Rra\n", rc));
        return rc;
    }
    pDrv->pDownConnector = PDMIBASE_QUERY_INTERFACE(pDownBase, PDMIMOUSECONNECTOR);
    if (!pDrv->pDownConnector)
    {
        AssertMsgFailed(("Configuration error: No mouse connector interface below!\n"));
        return VERR_PDM_MISSING_INTERFACE_BELOW;
    }

    /*
     * Create the queue.
     */
    uint32_t cMilliesInterval = 0;
    rc = CFGMR3QueryU32(pCfg, "Interval", &cMilliesInterval);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cMilliesInterval = 0;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: 32-bit \"Interval\" -> rc=%Rrc\n", rc));
        return rc;
    }

    uint32_t cItems = 0;
    rc = CFGMR3QueryU32(pCfg, "QueueSize", &cItems);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cItems = 128;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: 32-bit \"QueueSize\" -> rc=%Rrc\n", rc));
        return rc;
    }

    rc = PDMDrvHlpQueueCreate(pDrvIns, sizeof(DRVMOUSEQUEUEITEM), cItems, cMilliesInterval, drvMouseQueueConsumer, "Mouse", &pDrv->pQueue);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create driver: cItems=%d cMilliesInterval=%d rc=%Rrc\n", cItems, cMilliesInterval, rc));
        return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Mouse queue driver registration record.
 */
const PDMDRVREG g_DrvMouseQueue =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MouseQueue",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Mouse queue driver to plug in between the key source and the device to do queueing and inter-thread transport.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MOUSE,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMOUSEQUEUE),
    /* pfnConstruct */
    drvMouseQueueConstruct,
    /* pfnRelocate */
    NULL,
    /* pfnDestruct */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    drvMouseQueuePowerOn,
    /* pfnReset */
    drvMouseQueueReset,
    /* pfnSuspend */
    drvMouseQueueSuspend,
    /* pfnResume */
    drvMouseQueueResume,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvMouseQueuePowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

