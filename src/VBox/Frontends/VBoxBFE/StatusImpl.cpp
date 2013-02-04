/* $Id$ */
/** @file
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of VMStatus class
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
#else
# include <VBox/com/defs.h>
#endif
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/uuid.h>
#include "StatusImpl.h"

// defines
////////////////////////////////////////////////////////////////////////////////

// globals
////////////////////////////////////////////////////////////////////////////////

/**
 * The Main status driver instance data.
 */
typedef struct DRVMAINSTATUS
{
    /** The LED connectors. */
    PDMILEDCONNECTORS   ILedConnectors;
    /** Pointer to the LED ports interface above us. */
    PPDMILEDPORTS       pLedPorts;
    /** Pointer to the array of LED pointers. */
    PPDMLED            *papLeds;
    /** The unit number corresponding to the first entry in the LED array. */
    RTUINT              iFirstLUN;
    /** The unit number corresponding to the last entry in the LED array.
     * (The size of the LED array is iLastLUN - iFirstLUN + 1.) */
    RTUINT              iLastLUN;
} DRVMAINSTATUS, *PDRVMAINSTATUS;


/**
 * Notification about a unit which have been changed.
 *
 * The driver must discard any pointers to data owned by
 * the unit and requery it.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit number.
 */
DECLCALLBACK(void) VMStatus::drvUnitChanged(PPDMILEDCONNECTORS pInterface, unsigned iLUN)
{
    PDRVMAINSTATUS pData = (PDRVMAINSTATUS)(void *)pInterface;
    if (iLUN >= pData->iFirstLUN && iLUN <= pData->iLastLUN)
    {
        PPDMLED pLed;
        int rc = pData->pLedPorts->pfnQueryStatusLed(pData->pLedPorts, iLUN, &pLed);
        if (RT_FAILURE(rc))
            pLed = NULL;
        ASMAtomicXchgPtr((void * volatile *)&pData->papLeds[iLUN - pData->iFirstLUN], pLed);
        Log(("drvUnitChanged: iLUN=%d pLed=%p\n", iLUN, pLed));
    }
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *)  VMStatus::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINSTATUS pDrv = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    if (RTUuidCompare2Strs(pszIID, PDMIBASE_IID) == 0)
        return &pDrvIns->IBase;
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDCONNECTORS, &pDrv->ILedConnectors);
    return NULL;
}


/**
 * Destruct a status driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) VMStatus::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINSTATUS pData = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    LogFlow(("VMStatus::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
    if (pData->papLeds)
    {
        unsigned iLed = pData->iLastLUN - pData->iFirstLUN + 1;
        while (iLed-- > 0)
            ASMAtomicXchgPtr((void * volatile *)&pData->papLeds[iLed], NULL);
    }
}


/**
 * Construct a status driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) VMStatus::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVMAINSTATUS pData = PDMINS_2_DATA(pDrvIns, PDRVMAINSTATUS);
    LogFlow(("VMStatus::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "papLeds\0First\0Last\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Data.
     */
    pDrvIns->IBase.pfnQueryInterface        = VMStatus::drvQueryInterface;
    pData->ILedConnectors.pfnUnitChanged    = VMStatus::drvUnitChanged;

    /*
     * Read config.
     */
    int rc = CFGMR3QueryPtr(pCfg, "papLeds", (void **)&pData->papLeds);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"papLeds\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfg, "First", &pData->iFirstLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iFirstLUN = 0;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"First\" value! rc=%Rrc\n", rc));
        return rc;
    }

    rc = CFGMR3QueryU32(pCfg, "Last", &pData->iLastLUN);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->iLastLUN = 0;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Failed to query the \"Last\" value! rc=%Rrc\n", rc));
        return rc;
    }
    if (pData->iFirstLUN > pData->iLastLUN)
    {
        AssertMsgFailed(("Configuration error: Invalid unit range %u-%u\n", pData->iFirstLUN, pData->iLastLUN));
        return VERR_GENERAL_FAILURE;
    }

    /*
     * Get the ILedPorts interface of the above driver/device and
     * query the LEDs we want.
     */
    pData->pLedPorts = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMILEDPORTS);
    AssertMsgReturn(pData->pLedPorts, ("Configuration error: No led ports interface above!\n"),
                    VERR_PDM_MISSING_INTERFACE_ABOVE);

    for (unsigned i = pData->iFirstLUN; i <= pData->iLastLUN; i++)
        VMStatus::drvUnitChanged(&pData->ILedConnectors, i);

    return VINF_SUCCESS;
}


/**
 * VMStatus driver registration record.
 */
const PDMDRVREG VMStatus::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainStatus",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main status driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STATUS,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMAINSTATUS),
    /* pfnConstruct */
    VMStatus::drvConstruct,
    /* pfnDestruct */
    VMStatus::drvDestruct,
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

