/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Implementation of Keyboard class and related things
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

#ifdef VBOXBFE_WITHOUT_COM
# include "COMDefs.h"
#else
# include <VBox/com/defs.h>
#endif
#include <VBox/pdm.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
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
    if (VBOX_FAILURE (rcVBox))
        return E_FAIL;

    return S_OK;
}

/**
 * Sends a list of scancodes to the keyboard.
 *
 * @returns COM status code
 * @param scancodes   Pointer to the first scancode
 * @param count       Number of scancodes
 * @param codesStored Address of variable to store the number
 *                    of scancodes that were sent to the keyboard.
                      This value can be NULL.
 */
STDMETHODIMP Keyboard::PutScancodes(LONG *scancodes,
                                    ULONG count,
                                    ULONG *codesStored)
{
    if (!scancodes)
        return E_INVALIDARG;
    if (!mpDrv)
        return S_OK;

    LONG *currentScancode = scancodes;
    int rcVBox = VINF_SUCCESS;

    for (uint32_t i = 0; (i < count) && VBOX_SUCCESS(rcVBox); i++, currentScancode++)
    {
        rcVBox = mpDrv->pUpPort->pfnPutEvent(mpDrv->pUpPort, *(uint8_t*)currentScancode);
    }

    if (VBOX_FAILURE (rcVBox))
        return E_FAIL;

    /// @todo is it actually possible that not all scancodes can be transmitted?
    if (codesStored)
        *codesStored = count;

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
    static LONG cadSequence[] = {
        0x1d, // Ctrl down
        0x38, // Alt down
        0x53, // Del down
        0xd3, // Del up
        0xb8, // Alt up
        0x9d  // Ctrl up
    };

    return PutScancodes (cadSequence, ELEMENTS (cadSequence), NULL);
}

//
// private methods
//

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
DECLCALLBACK(void *)  Keyboard::drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINKEYBOARD pDrv = PDMINS2DATA(pDrvIns, PDRVMAINKEYBOARD);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_KEYBOARD_CONNECTOR:
            return &pDrv->Connector;
        default:
            return NULL;
    }
}


/**
 * Destruct a keyboard driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) Keyboard::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINKEYBOARD pData = PDMINS2DATA(pDrvIns, PDRVMAINKEYBOARD);
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
DECLCALLBACK(int) Keyboard::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMAINKEYBOARD pData = PDMINS2DATA(pDrvIns, PDRVMAINKEYBOARD);
    LogFlow(("Keyboard::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    PPDMIBASE pBaseIgnore;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBaseIgnore);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Not possible to attach anything to this driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface        = Keyboard::drvQueryInterface;

    pData->Connector.pfnLedStatusChange     = keyboardLedStatusChange;

    /*
     * Get the IKeyboardPort interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIKEYBOARDPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_KEYBOARD_PORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No keyboard port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Keyboard object pointer and update the mpDrv member.
     */
    void *pv;
    rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Vrc\n", rc));
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
    /* szDriverName */
    "MainKeyboard",
    /* pszDescription */
    "Main keyboard driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_KEYBOARD,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVMAINKEYBOARD),
    /* pfnConstruct */
    Keyboard::drvConstruct,
    /* pfnDestruct */
    Keyboard::drvDestruct,
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
