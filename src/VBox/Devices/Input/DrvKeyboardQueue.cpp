/** @file
 *
 * VBox input devices:
 * Keyboard queue driver
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
#define LOG_GROUP LOG_GROUP_DRV_KBD_QUEUE
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>

#include "Builtins.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Keyboard queue driver instance data.
 */
typedef struct DRVKBDQUEUE
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the keyboard port interface of the driver/device above us. */
    PPDMIKEYBOARDPORT           pUpPort;
    /** Pointer to the keyboard port interface of the driver/device below us. */
    PPDMIKEYBOARDCONNECTOR      pDownConnector;
    /** Our keyboard connector interface. */
    PDMIKEYBOARDCONNECTOR       Connector;
    /** Our keyboard port interface. */
    PDMIKEYBOARDPORT            Port;
    /** The queue handle. */
    PPDMQUEUE                   pQueue;
    /** Discard input when this flag is set.
     * We only accept input when the VM is running. */
    bool                        fInactive;
} DRVKBDQUEUE, *PDRVKBDQUEUE;


/**
 * Keyboard queue item.
 */
typedef struct DRVKBDQUEUEITEM
{
    /** The core part owned by the queue manager. */
    PDMQUEUEITEMCORE    Core;
    /** The keycode. */
    uint8_t             u8KeyCode;
} DRVKBDQUEUEITEM, *PDRVKBDQUEUEITEM;



/* -=-=-=-=- IBase -=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *)  drvKbdQueueQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVKBDQUEUE pDrv = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_KEYBOARD_PORT:
            return &pDrv->Port;
        case PDMINTERFACE_KEYBOARD_CONNECTOR:
            return &pDrv->Connector;
        default:
            return NULL;
    }
}


/* -=-=-=-=- IKeyboardPort -=-=-=-=- */

/** Converts a pointer to DRVKBDQUEUE::Port to a DRVKBDQUEUE pointer. */
#define IKEYBOARDPORT_2_DRVKBDQUEUE(pInterface) ( (PDRVKBDQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVKBDQUEUE, Port)) )


/**
 * Queues a keyboard event.
 * Because of the event queueing the EMT context requirement is lifted.
 *
 * @returns VBox status code.
 * @param   pInterface          Pointer to this interface structure.
 * @param   u8KeyCode           The keycode to queue.
 * @thread  Any thread.
 */
static DECLCALLBACK(int) drvKbdQueuePutEvent(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode)
{
    PDRVKBDQUEUE pDrv = IKEYBOARDPORT_2_DRVKBDQUEUE(pInterface);
    if (pDrv->fInactive)
        return VINF_SUCCESS;

    PDRVKBDQUEUEITEM pItem = (PDRVKBDQUEUEITEM)PDMQueueAlloc(pDrv->pQueue);
    if (pItem)
    {
        pItem->u8KeyCode = u8KeyCode;
        PDMQueueInsert(pDrv->pQueue, &pItem->Core);
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("drvKbdQueuePutEvent: Queue is full!!!!\n"));
    return VERR_PDM_NO_QUEUE_ITEMS;
}


/* -=-=-=-=- Connector -=-=-=-=- */

#define PPDMIKEYBOARDCONNECTOR_2_DRVKBDQUEUE(pInterface) ( (PDRVKBDQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVKBDQUEUE, Connector)) )


/**
 * Pass LED status changes from the guest thru to the frontent driver.
 *
 * @param   pInterface  Pointer to the keyboard connector interface structure.
 * @param   enmLeds     The new LED mask.
 */
static DECLCALLBACK(void) drvKbdPassThruLedsChange(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds)
{
    PDRVKBDQUEUE pDrv = PPDMIKEYBOARDCONNECTOR_2_DRVKBDQUEUE(pInterface);
    pDrv->pDownConnector->pfnLedStatusChange(pDrv->pDownConnector, enmLeds);
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
static DECLCALLBACK(bool) drvKbdQueueConsumer(PPDMDRVINS pDrvIns, PPDMQUEUEITEMCORE pItemCore)
{
    PDRVKBDQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    PDRVKBDQUEUEITEM    pItem = (PDRVKBDQUEUEITEM)pItemCore;
    int rc = pData->pUpPort->pfnPutEvent(pData->pUpPort, pItem->u8KeyCode);
    return VBOX_SUCCESS(rc);
}


/* -=-=-=-=- driver interface -=-=-=-=- */

/**
 * Power On notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void) drvKbdQueuePowerOn(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE    pData = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    pData->fInactive = false;
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvKbdQueueReset(PPDMDRVINS pDrvIns)
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
static DECLCALLBACK(void)  drvKbdQueueSuspend(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    pData->fInactive = true;
}


/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvKbdQueueResume(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    pData->fInactive = false;
}


/**
 * Power Off notification.
 *
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void) drvKbdQueuePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE        pData = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    pData->fInactive = true;
}


/**
 * Construct a keyboard driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvKbdQueueConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVKBDQUEUE pDrv = PDMINS2DATA(pDrvIns, PDRVKBDQUEUE);
    LogFlow(("drvKbdQueueConstruct: iInstance=%d\n", pDrvIns->iInstance));

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
    pDrvIns->IBase.pfnQueryInterface        = drvKbdQueueQueryInterface;
    /* IKeyboardConnector. */
    pDrv->Connector.pfnLedStatusChange      = drvKbdPassThruLedsChange;
    /* IKeyboardPort. */
    pDrv->Port.pfnPutEvent                  = drvKbdQueuePutEvent;

    /*
     * Get the IKeyboardPort interface of the above driver/device.
     */
    pDrv->pUpPort = (PPDMIKEYBOARDPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_KEYBOARD_PORT);
    if (!pDrv->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No keyboard port interface above!\n"));
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
    pDrv->pDownConnector = (PPDMIKEYBOARDCONNECTOR)pDownBase->pfnQueryInterface(pDownBase, PDMINTERFACE_KEYBOARD_CONNECTOR);
    if (!pDrv->pDownConnector)
    {
        AssertMsgFailed(("Configuration error: No keyboard connector interface below!\n"));
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

    rc = pDrvIns->pDrvHlp->pfnPDMQueueCreate(pDrvIns, sizeof(DRVKBDQUEUEITEM), cItems, cMilliesInterval, drvKbdQueueConsumer, &pDrv->pQueue);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create driver: cItems=%d cMilliesInterval=%d rc=%Vrc\n", cItems, cMilliesInterval, rc));
        return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Keyboard queue driver registration record.
 */
const PDMDRVREG g_DrvKeyboardQueue =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "KeyboardQueue",
    /* pszDescription */
    "Keyboard queue driver to plug in between the key source and the device to do queueing and inter-thread transport.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_KEYBOARD,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVKBDQUEUE),
    /* pfnConstruct */
    drvKbdQueueConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    drvKbdQueuePowerOn,
    /* pfnReset */
    drvKbdQueueReset,
    /* pfnSuspend */
    drvKbdQueueSuspend,
    /* pfnResume */
    drvKbdQueueResume,
    /* pfnDetach */
    NULL,
    /** pfnPowerOff */
    drvKbdQueuePowerOff
};

