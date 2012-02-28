/* $Id$ */
/** @file
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of Keyboard class and related things
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
# include <VBox/com/array.h>
#endif
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/uuid.h>
#include "KeyboardImpl.h"

// defines
////////////////////////////////////////////////////////////////////////////////

// globals
////////////////////////////////////////////////////////////////////////////////

/**
 * Keyboard driver instance data.
 */
typedef struct DRVMAINKEYBOARD
{
    /** Pointer to the keyboard object. */
    Keyboard                   *pKeyboard;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the keyboard port interface of the driver/device above us. */
    PPDMIKEYBOARDPORT           pUpPort;
    /** Our mouse connector interface. */
    PDMIKEYBOARDCONNECTOR       Connector;
} DRVMAINKEYBOARD, *PDRVMAINKEYBOARD;

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

Keyboard::Keyboard()
{
    mpDrv = NULL;
    mpVMMDev = NULL;
    mfVMMDevInited = false;
}

Keyboard::~Keyboard()
{
    if (mpDrv)
        mpDrv->pKeyboard = NULL;
    mpDrv = NULL;
    mpVMMDev = NULL;
    mfVMMDevInited = true;
}

// public methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Sends a scancode to the keyboard.
 *
 * @returns COM status code
 * @param scancode The scancode to send
 */
STDMETHODIMP Keyboard::PutScancode(LONG scancode)
{
    if (!mpDrv)
        return S_OK;

    int rcVBox = mpDrv->pUpPort->pfnPutEvent(mpDrv->pUpPort, (uint8_t)scancode);
    if (RT_FAILURE (rcVBox))
        return E_FAIL;

    return S_OK;
}

/**
 * Sends a list of scancodes to the keyboard.
 *
 * @returns COM status code
 * @param scancodes   Safe array of scancodes
 * @param codesStored Address of variable to store the number
 *                    of scancodes that were sent to the keyboard.
                      This value can be NULL.
 */
STDMETHODIMP Keyboard::PutScancodes(ComSafeArrayIn (LONG, scancodes),
                                    ULONG *codesStored)
{
    if (ComSafeArrayInIsNull(scancodes))
        return E_INVALIDARG;
    if (!mpDrv)
        return S_OK;

    com::SafeArray <LONG> keys(ComSafeArrayInArg(scancodes));
    int rcVBox = VINF_SUCCESS;

    for (uint32_t i = 0; (i < keys.size()) && RT_SUCCESS(rcVBox); i++)
    {
        rcVBox = mpDrv->pUpPort->pfnPutEvent(mpDrv->pUpPort, (uint8_t)keys[i]);
    }

    if (RT_FAILURE (rcVBox))
        return E_FAIL;

    /// @todo is it actually possible that not all scancodes can be transmitted?
    if (codesStored)
        *codesStored = keys.size();

    return S_OK;
}

/**
 * Sends Control-Alt-Delete to the keyboard. This could be done otherwise
 * but it's so common that we'll be nice and supply a convenience API.
 *
 * @returns COM status code
 *
 */
STDMETHODIMP Keyboard::PutCAD()
{
    static com::SafeArray<LONG> cadSequence(6);

    cadSequence[0] = 0x1d; // Ctrl down
    cadSequence[1] = 0x38; // Alt down
    cadSequence[2] = 0x53; // Del down
    cadSequence[3] = 0xd3; // Del up
    cadSequence[4] = 0xb8; // Alt up
    cadSequence[5] = 0x9d; // Ctrl up

    return PutScancodes (ComSafeArrayAsInParam(cadSequence), NULL);
}

//
// private methods
//

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *)  Keyboard::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINKEYBOARD pDrv = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);
    if (RTUuidCompare2Strs(pszIID, PDMIBASE_IID) == 0)
        return &pDrvIns->IBase;
    if (RTUuidCompare2Strs(pszIID, PDMIKEYBOARDCONNECTOR_IID) == 0)
        return &pDrv->Connector;
    return NULL;
}


/**
 * Destruct a keyboard driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) Keyboard::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINKEYBOARD pData = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);
    LogFlow(("Keyboard::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
    if (pData->pKeyboard)
    {
        pData->pKeyboard->mpDrv = NULL;
        pData->pKeyboard->mpVMMDev = NULL;
    }
}


/** @copydoc PDMIKEYBOARDCONNECTOR::pfnLedStatusChange */
DECLCALLBACK(void) keyboardLedStatusChange(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds)
{
    /** @todo Implement me. */
}

/** @copydoc PDMIKEYBOARDCONNECTOR::pfnLedStatusChange */
DECLCALLBACK(void) keyboardSetActive(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive)
{
    /** @todo Implement me. */
}

/**
 * Construct a keyboard driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Keyboard::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVMAINKEYBOARD pData = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);
    LogFlow(("Keyboard::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface        = Keyboard::drvQueryInterface;

    pData->Connector.pfnLedStatusChange     = keyboardLedStatusChange;
    pData->Connector.pfnSetActive           = keyboardSetActive;

    /*
     * Get the IKeyboardPort interface of the above driver/device.
     */
    pData->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIKEYBOARDPORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No keyboard port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Keyboard object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }
    pData->pKeyboard = (Keyboard *)pv;        /** @todo Check this cast! */
    pData->pKeyboard->mpDrv = pData;

    return VINF_SUCCESS;
}


/**
 * Keyboard driver registration record.
 */
const PDMDRVREG Keyboard::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainKeyboard",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main keyboard driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_KEYBOARD,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMAINKEYBOARD),
    /* pfnConstruct */
    Keyboard::drvConstruct,
    /* pfnDestruct */
    Keyboard::drvDestruct,
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
