/* $Id$ */
/** @file
 * VBox input devices: Keyboard queue driver
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
#define LOG_GROUP LOG_GROUP_DRV_KBD_QUEUE
#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Keyboard queue driver instance data.
 *
 * @implements  PDMIKEYBOARDCONNECTOR
 * @implements  PDMIKEYBOARDPORT
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
    PDMIKEYBOARDCONNECTOR       IConnector;
    /** Our keyboard port interface. */
    PDMIKEYBOARDPORT            IPort;
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
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *)  drvKbdQueueQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVKBDQUEUE    pThis   = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIKEYBOARDCONNECTOR, &pThis->IConnector);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIKEYBOARDPORT, &pThis->IPort);
    return NULL;
}


/* -=-=-=-=- IKeyboardPort -=-=-=-=- */

/** Converts a pointer to DRVKBDQUEUE::IPort to a DRVKBDQUEUE pointer. */
#define IKEYBOARDPORT_2_DRVKBDQUEUE(pInterface) ( (PDRVKBDQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVKBDQUEUE, IPort)) )


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


/* -=-=-=-=- IConnector -=-=-=-=- */

#define PPDMIKEYBOARDCONNECTOR_2_DRVKBDQUEUE(pInterface) ( (PDRVKBDQUEUE)((char *)(pInterface) - RT_OFFSETOF(DRVKBDQUEUE, IConnector)) )


/**
 * Pass LED status changes from the guest thru to the frontend driver.
 *
 * @param   pInterface  Pointer to the keyboard connector interface structure.
 * @param   enmLeds     The new LED mask.
 */
static DECLCALLBACK(void) drvKbdPassThruLedsChange(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds)
{
    PDRVKBDQUEUE pDrv = PPDMIKEYBOARDCONNECTOR_2_DRVKBDQUEUE(pInterface);
    pDrv->pDownConnector->pfnLedStatusChange(pDrv->pDownConnector, enmLeds);
}

/**
 * Pass keyboard state changes from the guest thru to the frontend driver.
 *
 * @param   pInterface  Pointer to the keyboard connector interface structure.
 * @param   fActive     The new active/inactive state.
 */
static DECLCALLBACK(void) drvKbdPassThruSetActive(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive)
{
    PDRVKBDQUEUE pDrv = PPDMIKEYBOARDCONNECTOR_2_DRVKBDQUEUE(pInterface);

    AssertPtr(pDrv->pDownConnector->pfnSetActive);
    pDrv->pDownConnector->pfnSetActive(pDrv->pDownConnector, fActive);
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
    PDRVKBDQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    PDRVKBDQUEUEITEM    pItem = (PDRVKBDQUEUEITEM)pItemCore;
    int rc = pThis->pUpPort->pfnPutEvent(pThis->pUpPort, pItem->u8KeyCode);
    return RT_SUCCESS(rc);
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
    PDRVKBDQUEUE    pThis = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    pThis->fInactive = false;
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvKbdQueueReset(PPDMDRVINS pDrvIns)
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
static DECLCALLBACK(void)  drvKbdQueueSuspend(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    pThis->fInactive = true;
}


/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void)  drvKbdQueueResume(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    pThis->fInactive = false;
}


/**
 * Power Off notification.
 *
 * @param   pDrvIns     The drive instance data.
 */
static DECLCALLBACK(void) drvKbdQueuePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVKBDQUEUE        pThis = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    pThis->fInactive = true;
}


/**
 * Construct a keyboard driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvKbdQueueConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVKBDQUEUE pDrv = PDMINS_2_DATA(pDrvIns, PDRVKBDQUEUE);
    LogFlow(("drvKbdQueueConstruct: iInstance=%d\n", pDrvIns->iInstance));
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
    pDrvIns->IBase.pfnQueryInterface        = drvKbdQueueQueryInterface;
    /* IKeyboardConnector. */
    pDrv->IConnector.pfnLedStatusChange     = drvKbdPassThruLedsChange;
    pDrv->IConnector.pfnSetActive           = drvKbdPassThruSetActive;
    /* IKeyboardPort. */
    pDrv->IPort.pfnPutEvent                 = drvKbdQueuePutEvent;

    /*
     * Get the IKeyboardPort interface of the above driver/device.
     */
    pDrv->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIKEYBOARDPORT);
    if (!pDrv->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No keyboard port interface above!\n"));
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
    pDrv->pDownConnector = PDMIBASE_QUERY_INTERFACE(pDownBase, PDMIKEYBOARDCONNECTOR);
    if (!pDrv->pDownConnector)
    {
        AssertMsgFailed(("Configuration error: No keyboard connector interface below!\n"));
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

    rc = PDMDrvHlpQueueCreate(pDrvIns, sizeof(DRVKBDQUEUEITEM), cItems, cMilliesInterval, drvKbdQueueConsumer, "Keyboard", &pDrv->pQueue);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to create driver: cItems=%d cMilliesInterval=%d rc=%Rrc\n", cItems, cMilliesInterval, rc));
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
    /* szName */
    "KeyboardQueue",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Keyboard queue driver to plug in between the key source and the device to do queueing and inter-thread transport.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_KEYBOARD,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVKBDQUEUE),
    /* pfnConstruct */
    drvKbdQueueConstruct,
    /* pfnRelocate */
    NULL,
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
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvKbdQueuePowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

