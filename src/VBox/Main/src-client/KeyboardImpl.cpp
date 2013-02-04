/* $Id$ */
/** @file
 * VirtualBox COM class implementation
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

#include "KeyboardImpl.h"
#include "ConsoleImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/com/array.h>
#include <VBox/vmm/pdmdrv.h>

#include <iprt/asm.h>
#include <iprt/cpp/utils.h>

// defines
////////////////////////////////////////////////////////////////////////////////

// globals
////////////////////////////////////////////////////////////////////////////////

/** @name Keyboard device capabilities bitfield
 * @{ */
enum
{
    /** The keyboard device does not wish to receive keystrokes. */
    KEYBOARD_DEVCAP_DISABLED = 0,
    /** The keyboard device does wishes to receive keystrokes. */
    KEYBOARD_DEVCAP_ENABLED  = 1
};

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
    /** Our keyboard connector interface. */
    PDMIKEYBOARDCONNECTOR       IConnector;
    /** The capabilities of this device. */
    uint32_t                    u32DevCaps;
} DRVMAINKEYBOARD, *PDRVMAINKEYBOARD;

/** Converts PDMIVMMDEVCONNECTOR pointer to a DRVMAINVMMDEV pointer. */
#define PPDMIKEYBOARDCONNECTOR_2_MAINKEYBOARD(pInterface) ( (PDRVMAINKEYBOARD) ((uintptr_t)pInterface - RT_OFFSETOF(DRVMAINKEYBOARD, IConnector)) )


// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

Keyboard::Keyboard()
    : mParent(NULL)
{
}

Keyboard::~Keyboard()
{
}

HRESULT Keyboard::FinalConstruct()
{
    RT_ZERO(mpDrv);
    mpVMMDev = NULL;
    mfVMMDevInited = false;
    return BaseFinalConstruct();
}

void Keyboard::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the keyboard object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
HRESULT Keyboard::init(Console *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    unconst(mEventSource).createObject();
    HRESULT rc = mEventSource->init(static_cast<IKeyboard*>(this));
    AssertComRCReturnRC(rc);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Keyboard::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    for (unsigned i = 0; i < KEYBOARD_MAX_DEVICES; ++i)
    {
        if (mpDrv[i])
            mpDrv[i]->pKeyboard = NULL;
        mpDrv[i] = NULL;
    }

    mpVMMDev = NULL;
    mfVMMDevInited = true;

    unconst(mParent) = NULL;
    unconst(mEventSource).setNull();
}

/**
 * Sends a scancode to the keyboard.
 *
 * @returns COM status code
 * @param scancode The scancode to send
 */
STDMETHODIMP Keyboard::PutScancode(LONG scancode)
{
    com::SafeArray<LONG> scancodes(1);
    scancodes[0] = scancode;
    return PutScancodes(ComSafeArrayAsInParam(scancodes), NULL);
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
STDMETHODIMP Keyboard::PutScancodes(ComSafeArrayIn(LONG, scancodes),
                                    ULONG *codesStored)
{
    if (ComSafeArrayInIsNull(scancodes))
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    com::SafeArray<LONG> keys(ComSafeArrayInArg(scancodes));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    CHECK_CONSOLE_DRV(mpDrv[0]);

    /* Send input to the last enabled device. Relies on the fact that
     * the USB keyboard is always initialized after the PS/2 keyboard.
     */
    PPDMIKEYBOARDPORT pUpPort = NULL;
    for (int i = KEYBOARD_MAX_DEVICES - 1; i >= 0 ; --i)
    {
        if (mpDrv[i] && (mpDrv[i]->u32DevCaps & KEYBOARD_DEVCAP_ENABLED))
        {
            pUpPort = mpDrv[i]->pUpPort;
            break;
        }
    }
    /* No enabled keyboard - throw the input away. */
    if (!pUpPort)
    {
        if (codesStored)
            *codesStored = (uint32_t)keys.size();
        return S_OK;
    }

    int vrc = VINF_SUCCESS;

    uint32_t sent;
    for (sent = 0; (sent < keys.size()) && RT_SUCCESS(vrc); sent++)
        vrc = pUpPort->pfnPutEvent(pUpPort, (uint8_t)keys[sent]);

    if (codesStored)
        *codesStored = sent;

    /* Only signal the keys in the event which have been actually sent. */
    com::SafeArray<LONG> keysSent(sent);
    memcpy(keysSent.raw(), keys.raw(), sent*sizeof(LONG));

    VBoxEventDesc evDesc;
    evDesc.init(mEventSource, VBoxEventType_OnGuestKeyboard, ComSafeArrayAsInParam(keys));
    evDesc.fire(0);

    if (RT_FAILURE(vrc))
        return setError(VBOX_E_IPRT_ERROR,
                        tr("Could not send all scan codes to the virtual keyboard (%Rrc)"),
                        vrc);

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
    static com::SafeArray<LONG> cadSequence(8);

    cadSequence[0] = 0x1d; // Ctrl down
    cadSequence[1] = 0x38; // Alt down
    cadSequence[2] = 0xe0; // Del down 1
    cadSequence[3] = 0x53; // Del down 2
    cadSequence[4] = 0xe0; // Del up 1
    cadSequence[5] = 0xd3; // Del up 2
    cadSequence[6] = 0xb8; // Alt up
    cadSequence[7] = 0x9d; // Ctrl up

    return PutScancodes(ComSafeArrayAsInParam(cadSequence), NULL);
}

STDMETHODIMP Keyboard::COMGETTER(EventSource)(IEventSource ** aEventSource)
{
    CheckComArgOutPointerValid(aEventSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // no need to lock - lifetime constant
    mEventSource.queryInterfaceTo(aEventSource);

    return S_OK;
}

//
// private methods
//

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *) Keyboard::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINKEYBOARD    pDrv    = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIKEYBOARDCONNECTOR, &pDrv->IConnector);
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
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pData->pKeyboard)
    {
        AutoWriteLock kbdLock(pData->pKeyboard COMMA_LOCKVAL_SRC_POS);
        for (unsigned cDev = 0; cDev < KEYBOARD_MAX_DEVICES; ++cDev)
            if (pData->pKeyboard->mpDrv[cDev] == pData)
            {
                pData->pKeyboard->mpDrv[cDev] = NULL;
                break;
            }
        pData->pKeyboard->mpVMMDev = NULL;
    }
}

DECLCALLBACK(void) keyboardLedStatusChange(PPDMIKEYBOARDCONNECTOR pInterface,
                                           PDMKEYBLEDS enmLeds)
{
    PDRVMAINKEYBOARD pDrv = PPDMIKEYBOARDCONNECTOR_2_MAINKEYBOARD(pInterface);
    pDrv->pKeyboard->getParent()->onKeyboardLedsChange(!!(enmLeds & PDMKEYBLEDS_NUMLOCK),
                                                       !!(enmLeds & PDMKEYBLEDS_CAPSLOCK),
                                                       !!(enmLeds & PDMKEYBLEDS_SCROLLLOCK));
}

/**
 * @interface_method_impl{PDMIKEYBOARDCONNECTOR,pfnSetActive}
 */
DECLCALLBACK(void) Keyboard::keyboardSetActive(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive)
{
    PDRVMAINKEYBOARD pDrv = PPDMIKEYBOARDCONNECTOR_2_MAINKEYBOARD(pInterface);
    if (fActive)
        pDrv->u32DevCaps |= KEYBOARD_DEVCAP_ENABLED;
    else
        pDrv->u32DevCaps &= ~KEYBOARD_DEVCAP_ENABLED;
}

/**
 * Construct a keyboard driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Keyboard::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg,
                                         uint32_t fFlags)
{
    PDRVMAINKEYBOARD pData = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);
    LogFlow(("Keyboard::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

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

    pData->IConnector.pfnLedStatusChange    = keyboardLedStatusChange;
    pData->IConnector.pfnSetActive          = keyboardSetActive;

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
    unsigned cDev;
    for (cDev = 0; cDev < KEYBOARD_MAX_DEVICES; ++cDev)
        if (!pData->pKeyboard->mpDrv[cDev])
        {
            pData->pKeyboard->mpDrv[cDev] = pData;
            break;
        }
    if (cDev == KEYBOARD_MAX_DEVICES)
        return VERR_NO_MORE_HANDLES;

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
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
