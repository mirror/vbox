/** @file
 *
 * VBox input devices:
 * Mouse queue driver
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
#define LOG_GROUP LOG_GROUP_DRV_MOUSE_QUEUE
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>

#include "Builtins.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Mouse queue driver instance data.
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
    PDMIMOUSECONNECTOR          Connector;
    /** Our mouse port interface. */
    PDMIMOUSEPORT               Port;
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
    int32_t             i32DeltaX;
    int32_t             i32DeltaY;
    int32_t             i32DeltaZ;
    uint32_t            fButtonStates;
} DRVMOUSEQUEUEITEM, *PDRVMOUSEQUEUEITEM;



/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *)  drvMouseQueueQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMOUSEQUEUE pDrv = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_MOUSE_PORT:
            return &pDrv->Port;
        case PDMINTERFACE_MOUSE_CONNECTOR:
            return &pDrv->Connector;
        default:
            return NULL;
    }
}


/* -=-=-=-=- IMousePort -=-=-=-=- */

/** Converts a pointer to DRVMOUSEQUEUE::Port to a DRVMOUSEQUEUE pointer. */
#define IMOUSEPORT_2_DRVMOUSEQUEUE(pInterface) ( (PDRVMOUSEQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVMOUSEQUEUE, Port)) )


/**
 * Queues a mouse event.
 * Because of the event queueing the EMT context requirement is lifted.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to interface structure.
 * @param   i32DeltaX       The X delta.
 * @param   i32DeltaY       The Y delta.
 * @param   i32DeltaZ       The Z delta.
 * @param   fButtonStates   The button states.
 * @thread  Any thread.
 */
static DECLCALLBACK(int) drvMouseQueuePutEvent(PPDMIMOUSEPORT pInterface, int32_t i32DeltaX, int32_t i32DeltaY, int32_t i32DeltaZ, uint32_t fButtonStates)
{
    PDRVMOUSEQUEUE pDrv = IMOUSEPORT_2_DRVMOUSEQUEUE(pInterface);
    if (pDrv->fInactive)
        return VINF_SUCCESS;

    PDRVMOUSEQUEUEITEM pItem = (PDRVMOUSEQUEUEITEM)PDMQueueAlloc(pDrv->pQueue);
    if (pItem)
    {
        pItem->i32DeltaX = i32DeltaX;
        pItem->i32DeltaY = i32DeltaY;
        pItem->i32DeltaZ = i32DeltaZ;
        pItem->fButtonStates = fButtonStates;
        PDMQueueInsert(pDrv->pQueue, &pItem->Core);
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("drvMouseQueuePutEvent: Queue is full!!!!\n"));
    return VERR_PDM_NO_QUEUE_ITEMS;
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
    PDRVMOUSEQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    PDRVMOUSEQUEUEITEM    pItem = (PDRVMOUSEQUEUEITEM)pItemCore;
    int rc = pData->pUpPort->pfnPutEvent(pData->pUpPort, pItem->i32DeltaX, pItem->i32DeltaY, pItem->i32DeltaZ, pItem->fButtonStates);
    return VBOX_SUCCESS(rc);
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
    PDRVMOUSEQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    pData->fInactive = false;
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvMouseQueueReset(PPDMDRVINS pDrvIns)
{
    //PDRVKBDQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
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
    PDRVMOUSEQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    pData->fInactive = true;
}


/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvMouseQueueResume(PPDMDRVINS pDrvIns)
{
    PDRVMOUSEQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    pData->fInactive = false;
}


/**
 * Power Off notification.
 *
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void) drvMouseQueuePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVMOUSEQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    pData->fInactive = true;
}


/**
 * Construct a mouse driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvMouseQueueConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMOUSEQUEUE pDrv = PDMINS2DATA(pDrvIns, PDRVMOUSEQUEUE);
    LogFlow(("drvMouseQueueConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "QueueSize\0Interval\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init basic data members and interfaces.
     */
    pDrv->fInactive                         = true;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface        = drvMouseQueueQueryInterface;
    /* IMousePort. */
    pDrv->Port.pfnPutEvent                  = drvMouseQueuePutEvent;

    /*
     * Get the IMousePort interface of the above driver/device.
     */
    pDrv->pUpPort = (PPDMIMOUSEPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_MOUSE_PORT);
    if (!pDrv->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No mouse port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Attach driver below and query it's connector interface.
     */
    PPDMIBASE pDownBase;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pDownBase);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to attach driver below us! rc=%Vra\n", rc));
        return rc;
    }
    pDrv->pDownConnector = (PPDMIMOUSECONNECTOR)pDownBase->pfnQueryInterface(pDownBase, PDMINTERFACE_MOUSE_CONNECTOR);
    if (!pDrv->pDownConnector)
    {
        AssertMsgFailed(("Configuration error: No mouse connector interface below!\n"));
        return VERR_PDM_MISSING_INTERFACE_BELOW;
    }

    /*
     * Create the queue.
     */
    uint32_t cMilliesInterval = 0;
    rc = CFGMR3QueryU32(pCfgHandle, "Interval", &cMilliesInterval);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cMilliesInterval = 0;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: 32-bit \"Interval\" -> rc=%Vrc\n", rc));
        return rc;
    }

    uint32_t cItems = 0;
    rc = CFGMR3QueryU32(pCfgHandle, "QueueSize", &cItems);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cItems = 128;
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: 32-bit \"QueueSize\" -> rc=%Vrc\n", rc));
        return rc;
    }

    rc = pDrvIns->pDrvHlp->pfnPDMQueueCreate(pDrvIns, sizeof(DRVMOUSEQUEUEITEM), cItems, cMilliesInterval, drvMouseQueueConsumer, &pDrv->pQueue);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create driver: cItems=%d cMilliesInterval=%d rc=%Vrc\n", cItems, cMilliesInterval, rc));
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
    /* szDriverName */
    "MouseQueue",
    /* pszDescription */
    "Mouse queue driver to plug in between the key source and the device to do queueing and inter-thread transport.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MOUSE,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVMOUSEQUEUE),
    /* pfnConstruct */
    drvMouseQueueConstruct,
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
    /* pfnDetach */
    NULL,
    /** pfnPowerOff */
    drvMouseQueuePowerOff
};


