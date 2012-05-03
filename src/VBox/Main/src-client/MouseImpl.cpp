/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/cpp/utils.h>

#include "MouseImpl.h"
#include "DisplayImpl.h"
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/vmm/pdmdrv.h>
#include <VBox/VMMDev.h>

#include <iprt/asm.h>

/** @name Mouse device capabilities bitfield
 * @{ */
enum
{
    /** The mouse device can do relative reporting */
    MOUSE_DEVCAP_RELATIVE = 1,
    /** The mouse device can do absolute reporting */
    MOUSE_DEVCAP_ABSOLUTE = 2
};
/** @} */


/**
 * Mouse driver instance data.
 */
struct DRVMAINMOUSE
{
    /** Pointer to the mouse object. */
    Mouse                      *pMouse;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the mouse port interface of the driver/device above us. */
    PPDMIMOUSEPORT              pUpPort;
    /** Our mouse connector interface. */
    PDMIMOUSECONNECTOR          IConnector;
    /** The capabilities of this device. */
    uint32_t                    u32DevCaps;
};


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

Mouse::Mouse()
    : mParent(NULL)
{
}

Mouse::~Mouse()
{
}


HRESULT Mouse::FinalConstruct()
{
    RT_ZERO(mpDrv);
    mcLastAbsX = 0x8000;
    mcLastAbsY = 0x8000;
    mfLastButtons = 0;
    mfVMMDevGuestCaps = 0;
    return BaseFinalConstruct();
}

void Mouse::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the mouse object.
 *
 * @returns COM result indicator
 * @param parent handle of our parent object
 */
HRESULT Mouse::init (Console *parent)
{
    LogFlowThisFunc(("\n"));

    ComAssertRet(parent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = parent;

#ifndef VBOXBFE_WITHOUT_COM
    unconst(mEventSource).createObject();
    HRESULT rc = mEventSource->init(static_cast<IMouse*>(this));
    AssertComRCReturnRC(rc);
    mMouseEvent.init(mEventSource, VBoxEventType_OnGuestMouse,
                     0, 0, 0, 0, 0);
#endif

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Mouse::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    for (unsigned i = 0; i < MOUSE_MAX_DEVICES; ++i)
    {
        if (mpDrv[i])
            mpDrv[i]->pMouse = NULL;
        mpDrv[i] = NULL;
    }

#ifdef VBOXBFE_WITHOUT_COM
    mParent = NULL;
#else
    mMouseEvent.uninit();
    unconst(mEventSource).setNull();
    unconst(mParent) = NULL;
#endif
}


// IMouse properties
/////////////////////////////////////////////////////////////////////////////

/** Report the front-end's mouse handling capabilities to the VMM device and
 * thus to the guest.
 * @note all calls out of this object are made with no locks held! */
HRESULT Mouse::updateVMMDevMouseCaps(uint32_t fCapsAdded,
                                     uint32_t fCapsRemoved)
{
    VMMDev *pVMMDev = mParent->getVMMDev();
    if (!pVMMDev)
        return E_FAIL;  /* No assertion, as the front-ends can send events
                         * at all sorts of inconvenient times. */
    PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
    if (!pVMMDevPort)
        return E_FAIL;  /* same here */

    int rc = pVMMDevPort->pfnUpdateMouseCapabilities(pVMMDevPort, fCapsAdded,
                                                     fCapsRemoved);
    return RT_SUCCESS(rc) ? S_OK : E_FAIL;
}

/**
 * Returns whether the current setup can accept absolute mouse events, either
 * because an emulated absolute pointing device is active or because the Guest
 * Additions are.
 *
 * @returns COM status code
 * @param absoluteSupported address of result variable
 */
STDMETHODIMP Mouse::COMGETTER(AbsoluteSupported) (BOOL *absoluteSupported)
{
    if (!absoluteSupported)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *absoluteSupported = supportsAbs();
    return S_OK;
}

/**
 * Returns whether the current setup can accept relative mouse events, that is,
 * whether an emulated relative pointing device is active.
 *
 * @returns COM status code
 * @param relativeSupported address of result variable
 */
STDMETHODIMP Mouse::COMGETTER(RelativeSupported) (BOOL *relativeSupported)
{
    if (!relativeSupported)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *relativeSupported = supportsRel();
    return S_OK;
}

/**
 * Returns whether the guest can currently switch to drawing the mouse cursor
 * itself if it is asked to by the front-end.
 *
 * @returns COM status code
 * @param pfNeedsHostCursor address of result variable
 */
STDMETHODIMP Mouse::COMGETTER(NeedsHostCursor) (BOOL *pfNeedsHostCursor)
{
    if (!pfNeedsHostCursor)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *pfNeedsHostCursor = guestNeedsHostCursor();
    return S_OK;
}

// IMouse methods
/////////////////////////////////////////////////////////////////////////////

/** Converts a bitfield containing information about mouse buttons currently
 * held down from the format used by the front-end to the format used by PDM
 * and the emulated pointing devices. */
static uint32_t mouseButtonsToPDM(LONG buttonState)
{
    uint32_t fButtons = 0;
    if (buttonState & MouseButtonState_LeftButton)
        fButtons |= PDMIMOUSEPORT_BUTTON_LEFT;
    if (buttonState & MouseButtonState_RightButton)
        fButtons |= PDMIMOUSEPORT_BUTTON_RIGHT;
    if (buttonState & MouseButtonState_MiddleButton)
        fButtons |= PDMIMOUSEPORT_BUTTON_MIDDLE;
    if (buttonState & MouseButtonState_XButton1)
        fButtons |= PDMIMOUSEPORT_BUTTON_X1;
    if (buttonState & MouseButtonState_XButton2)
        fButtons |= PDMIMOUSEPORT_BUTTON_X2;
    return fButtons;
}

#ifndef VBOXBFE_WITHOUT_COM
STDMETHODIMP Mouse::COMGETTER(EventSource)(IEventSource ** aEventSource)
{
    CheckComArgOutPointerValid(aEventSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // no need to lock - lifetime constant
    mEventSource.queryInterfaceTo(aEventSource);

    return S_OK;
}
#endif

/**
 * Send a relative pointer event to the relative device we deem most
 * appropriate.
 *
 * @returns   COM status code
 */
HRESULT Mouse::reportRelEventToMouseDev(int32_t dx, int32_t dy, int32_t dz,
                                        int32_t dw, uint32_t fButtons)
{
    if (dx || dy || dz || dw || fButtons != mfLastButtons)
    {
        PPDMIMOUSEPORT pUpPort = NULL;
        {
            AutoReadLock aLock(this COMMA_LOCKVAL_SRC_POS);

            for (unsigned i = 0; !pUpPort && i < MOUSE_MAX_DEVICES; ++i)
            {
                if (mpDrv[i] && (mpDrv[i]->u32DevCaps & MOUSE_DEVCAP_RELATIVE))
                    pUpPort = mpDrv[i]->pUpPort;
            }
        }
        if (!pUpPort)
            return S_OK;

        int vrc = pUpPort->pfnPutEvent(pUpPort, dx, dy, dz, dw, fButtons);

        if (RT_FAILURE(vrc))
            return setError(VBOX_E_IPRT_ERROR,
                            tr("Could not send the mouse event to the virtual mouse (%Rrc)"),
                            vrc);
        mfLastButtons = fButtons;
    }
    return S_OK;
}


/**
 * Send an absolute pointer event to the emulated absolute device we deem most
 * appropriate.
 *
 * @returns   COM status code
 */
HRESULT Mouse::reportAbsEventToMouseDev(int32_t mouseXAbs, int32_t mouseYAbs,
                                        int32_t dz, int32_t dw, uint32_t fButtons)
{
    if (   mouseXAbs < VMMDEV_MOUSE_RANGE_MIN
        || mouseXAbs > VMMDEV_MOUSE_RANGE_MAX)
        return S_OK;
    if (   mouseYAbs < VMMDEV_MOUSE_RANGE_MIN
        || mouseYAbs > VMMDEV_MOUSE_RANGE_MAX)
        return S_OK;
    if (   mouseXAbs != mcLastAbsX || mouseYAbs != mcLastAbsY
        || dz || dw || fButtons != mfLastButtons)
    {
        PPDMIMOUSEPORT pUpPort = NULL;
        {
            AutoReadLock aLock(this COMMA_LOCKVAL_SRC_POS);

            for (unsigned i = 0; !pUpPort && i < MOUSE_MAX_DEVICES; ++i)
            {
                if (mpDrv[i] && (mpDrv[i]->u32DevCaps & MOUSE_DEVCAP_ABSOLUTE))
                    pUpPort = mpDrv[i]->pUpPort;
            }
        }
        if (!pUpPort)
            return S_OK;

        int vrc = pUpPort->pfnPutEventAbs(pUpPort, mouseXAbs, mouseYAbs, dz,
                                          dw, fButtons);
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_IPRT_ERROR,
                            tr("Could not send the mouse event to the virtual mouse (%Rrc)"),
                            vrc);
        mfLastButtons = fButtons;

    }
    return S_OK;
}


/**
 * Send an absolute position event to the VMM device.
 * @note all calls out of this object are made with no locks held!
 *
 * @returns   COM status code
 */
HRESULT Mouse::reportAbsEventToVMMDev(int32_t mouseXAbs, int32_t mouseYAbs)
{
    VMMDev *pVMMDev = mParent->getVMMDev();
    ComAssertRet(pVMMDev, E_FAIL);
    PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
    ComAssertRet(pVMMDevPort, E_FAIL);

    if (mouseXAbs != mcLastAbsX || mouseYAbs != mcLastAbsY)
    {
        int vrc = pVMMDevPort->pfnSetAbsoluteMouse(pVMMDevPort,
                                                   mouseXAbs, mouseYAbs);
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_IPRT_ERROR,
                            tr("Could not send the mouse event to the virtual mouse (%Rrc)"),
                            vrc);
    }
    return S_OK;
}


/**
 * Send an absolute pointer event to a pointing device (the VMM device if
 * possible or whatever emulated absolute device seems best to us if not).
 *
 * @returns   COM status code
 */
HRESULT Mouse::reportAbsEvent(int32_t mouseXAbs, int32_t mouseYAbs,
                              int32_t dz, int32_t dw, uint32_t fButtons,
                              bool fUsesVMMDevEvent)
{
    HRESULT rc;
    /** If we are using the VMMDev to report absolute position but without
     * VMMDev IRQ support then we need to send a small "jiggle" to the emulated
     * relative mouse device to alert the guest to changes. */
    LONG cJiggle = 0;

    if (vmmdevCanAbs())
    {
        /*
         * Send the absolute mouse position to the VMM device.
         */
        if (mouseXAbs != mcLastAbsX || mouseYAbs != mcLastAbsY)
        {
            rc = reportAbsEventToVMMDev(mouseXAbs, mouseYAbs);
            cJiggle = !fUsesVMMDevEvent;
        }
        rc = reportRelEventToMouseDev(cJiggle, 0, dz, dw, fButtons);
    }
    else
        rc = reportAbsEventToMouseDev(mouseXAbs, mouseYAbs, dz, dw, fButtons);

    mcLastAbsX = mouseXAbs;
    mcLastAbsY = mouseYAbs;
    return rc;
}

#ifndef VBOXBFE_WITHOUT_COM
void Mouse::fireMouseEvent(bool fAbsolute, LONG x, LONG y, LONG dz, LONG dw, LONG Buttons)
{
    /* If mouse button is pressed, we generate new event, to avoid reusable events coalescing and thus
       dropping key press events */
    if (Buttons != 0)
    {
        VBoxEventDesc evDesc;
        evDesc.init(mEventSource, VBoxEventType_OnGuestMouse, fAbsolute, x, y, dz, dw, Buttons);
        evDesc.fire(0);
    }
    else
    {
        mMouseEvent.reinit(VBoxEventType_OnGuestMouse, fAbsolute, x, y, dz, dw, Buttons);
        mMouseEvent.fire(0);
    }
}
#endif


/**
 * Send a relative mouse event to the guest.
 * @note the VMMDev capability change is so that the guest knows we are sending
 *       real events over the PS/2 device and not dummy events to signal the
 *       arrival of new absolute pointer data
 *
 * @returns COM status code
 * @param dx          X movement
 * @param dy          Y movement
 * @param dz          Z movement
 * @param buttonState The mouse button state
 */
STDMETHODIMP Mouse::PutMouseEvent(LONG dx, LONG dy, LONG dz, LONG dw, LONG buttonState)
{
    HRESULT rc;
    uint32_t fButtons;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    LogRel3(("%s: dx=%d, dy=%d, dz=%d, dw=%d\n", __PRETTY_FUNCTION__,
                 dx, dy, dz, dw));

    fButtons = mouseButtonsToPDM(buttonState);
    /* Make sure that the guest knows that we are sending real movement
     * events to the PS/2 device and not just dummy wake-up ones. */
    updateVMMDevMouseCaps(0, VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE);
    rc = reportRelEventToMouseDev(dx, dy, dz, dw, fButtons);

    fireMouseEvent(false, dx, dy, dz, dw, buttonState);

    return rc;
}

/**
 * Convert an (X, Y) value pair in screen co-ordinates (starting from 1) to a
 * value from VMMDEV_MOUSE_RANGE_MIN to VMMDEV_MOUSE_RANGE_MAX.  Sets the
 * optional validity value to false if the pair is not on an active screen and
 * to true otherwise.
 * @note      since guests with recent versions of X.Org use a different method
 *            to everyone else to map the valuator value to a screen pixel (they
 *            multiply by the screen dimension, do a floating point divide by
 *            the valuator maximum and round the result, while everyone else
 *            does truncating integer operations) we adjust the value we send
 *            so that it maps to the right pixel both when the result is rounded
 *            and when it is truncated.
 *
 * @returns   COM status value
 */
HRESULT Mouse::convertDisplayRes(LONG x, LONG y, int32_t *pcX, int32_t *pcY,
                                 bool *pfValid)
{
    AssertPtrReturn(pcX, E_POINTER);
    AssertPtrReturn(pcY, E_POINTER);
    AssertPtrNullReturn(pfValid, E_POINTER);
    Display *pDisplay = mParent->getDisplay();
    ComAssertRet(pDisplay, E_FAIL);
    /** The amount to add to the result (multiplied by the screen width/height)
     * to compensate for differences in guest methods for mapping back to
     * pixels */
    enum { ADJUST_RANGE = - 3 * VMMDEV_MOUSE_RANGE / 4 };

    if (pfValid)
        *pfValid = true;
    if (!(mfVMMDevGuestCaps & VMMDEV_MOUSE_NEW_PROTOCOL))
    {
        ULONG displayWidth, displayHeight;
        /* Takes the display lock */
        HRESULT rc = pDisplay->GetScreenResolution(0, &displayWidth,
                                                   &displayHeight, NULL);
        if (FAILED(rc))
            return rc;

        *pcX = displayWidth ?    (x * VMMDEV_MOUSE_RANGE + ADJUST_RANGE)
                               / (LONG) displayWidth: 0;
        *pcY = displayHeight ?   (y * VMMDEV_MOUSE_RANGE + ADJUST_RANGE)
                               / (LONG) displayHeight: 0;
    }
    else
    {
        int32_t x1, y1, x2, y2;
        /* Takes the display lock */
        pDisplay->getFramebufferDimensions(&x1, &y1, &x2, &y2);
        *pcX = x1 < x2 ?   ((x - x1) * VMMDEV_MOUSE_RANGE + ADJUST_RANGE)
                         / (x2 - x1) : 0;
        *pcY = y1 < y2 ?   ((y - y1) * VMMDEV_MOUSE_RANGE + ADJUST_RANGE)
                         / (y2 - y1) : 0;
        if (   *pcX < VMMDEV_MOUSE_RANGE_MIN || *pcX > VMMDEV_MOUSE_RANGE_MAX
            || *pcY < VMMDEV_MOUSE_RANGE_MIN || *pcY > VMMDEV_MOUSE_RANGE_MAX)
            if (pfValid)
                *pfValid = false;
    }
    return S_OK;
}


/**
 * Send an absolute mouse event to the VM. This requires either VirtualBox-
 * specific drivers installed in the guest or absolute pointing device
 * emulation.
 * @note the VMMDev capability change is so that the guest knows we are sending
 *       dummy events over the PS/2 device to signal the arrival of new
 *       absolute pointer data, and not pointer real movement data
 * @note all calls out of this object are made with no locks held!
 *
 * @returns COM status code
 * @param x          X position (pixel), starting from 1
 * @param y          Y position (pixel), starting from 1
 * @param dz         Z movement
 * @param buttonState The mouse button state
 */
STDMETHODIMP Mouse::PutMouseEventAbsolute(LONG x, LONG y, LONG dz, LONG dw,
                                          LONG buttonState)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    LogRel3(("%s: x=%d, y=%d, dz=%d, dw=%d, buttonState=0x%x\n",
             __PRETTY_FUNCTION__, x, y, dz, dw, buttonState));

    int32_t mouseXAbs, mouseYAbs;
    uint32_t fButtons;
    bool fValid;

    /** @todo the front end should do this conversion to avoid races */
    /** @note Or maybe not... races are pretty inherent in everything done in
     *        this object and not really bad as far as I can see. */
    HRESULT rc = convertDisplayRes(x, y, &mouseXAbs, &mouseYAbs, &fValid);
    if (FAILED(rc)) return rc;

    fButtons = mouseButtonsToPDM(buttonState);
    /* If we are doing old-style (IRQ-less) absolute reporting to the VMM
     * device then make sure the guest is aware of it, so that it knows to
     * ignore relative movement on the PS/2 device. */
    updateVMMDevMouseCaps(VMMDEV_MOUSE_HOST_WANTS_ABSOLUTE, 0);
    if (fValid)
    {
        rc = reportAbsEvent(mouseXAbs, mouseYAbs, dz, dw, fButtons,
                            RT_BOOL(  mfVMMDevGuestCaps
                                    & VMMDEV_MOUSE_NEW_PROTOCOL));

        fireMouseEvent(true, x, y, dz, dw, buttonState);
    }

    return rc;
}

// private methods
/////////////////////////////////////////////////////////////////////////////


/** Does the guest currently rely on the host to draw the mouse cursor or
 * can it switch to doing it itself in software? */
bool Mouse::guestNeedsHostCursor(void)
{
    return RT_BOOL(mfVMMDevGuestCaps & VMMDEV_MOUSE_GUEST_NEEDS_HOST_CURSOR);
}


/** Check what sort of reporting can be done using the devices currently
 * enabled.  Does not consider the VMM device. */
void Mouse::getDeviceCaps(bool *pfAbs, bool *pfRel)
{
    bool fAbsDev = false;
    bool fRelDev = false;

    AutoReadLock aLock(this COMMA_LOCKVAL_SRC_POS);

    for (unsigned i = 0; i < MOUSE_MAX_DEVICES; ++i)
        if (mpDrv[i])
        {
           if (mpDrv[i]->u32DevCaps & MOUSE_DEVCAP_ABSOLUTE)
               fAbsDev = true;
           if (mpDrv[i]->u32DevCaps & MOUSE_DEVCAP_RELATIVE)
               fRelDev = true;
        }
    if (pfAbs)
        *pfAbs = fAbsDev;
    if (pfRel)
        *pfRel = fRelDev;
}


/** Does the VMM device currently support absolute reporting? */
bool Mouse::vmmdevCanAbs(void)
{
    bool fRelDev;

    getDeviceCaps(NULL, &fRelDev);
    return    (mfVMMDevGuestCaps & VMMDEV_MOUSE_GUEST_CAN_ABSOLUTE)
           && fRelDev;
}


/** Does the VMM device currently support absolute reporting? */
bool Mouse::deviceCanAbs(void)
{
    bool fAbsDev;

    getDeviceCaps(&fAbsDev, NULL);
    return fAbsDev;
}


/** Can we currently send relative events to the guest? */
bool Mouse::supportsRel(void)
{
    bool fRelDev;

    getDeviceCaps(NULL, &fRelDev);
    return fRelDev;
}


/** Can we currently send absolute events to the guest? */
bool Mouse::supportsAbs(void)
{
    bool fAbsDev;

    getDeviceCaps(&fAbsDev, NULL);
    return fAbsDev || vmmdevCanAbs();
}


/** Check what sort of reporting can be done using the devices currently
 * enabled (including the VMM device) and notify the guest and the front-end.
 */
void Mouse::sendMouseCapsNotifications(void)
{
    bool fAbsDev, fRelDev, fCanAbs, fNeedsHostCursor;

    {
        AutoReadLock aLock(this COMMA_LOCKVAL_SRC_POS);

        getDeviceCaps(&fAbsDev, &fRelDev);
        fCanAbs = supportsAbs();
        fNeedsHostCursor = guestNeedsHostCursor();
    }
    if (fAbsDev)
        updateVMMDevMouseCaps(VMMDEV_MOUSE_HOST_HAS_ABS_DEV, 0);
    else
        updateVMMDevMouseCaps(0, VMMDEV_MOUSE_HOST_HAS_ABS_DEV);
    /** @todo this call takes the Console lock in order to update the cached
     * callback data atomically.  However I can't see any sign that the cached
     * data is ever used again. */
    mParent->onMouseCapabilityChange(fCanAbs, fRelDev, fNeedsHostCursor);
}


/**
 * @interface_method_impl{PDMIMOUSECONNECTOR,pfnReportModes}
 * A virtual device is notifying us about its current state and capabilities
 */
DECLCALLBACK(void) Mouse::mouseReportModes(PPDMIMOUSECONNECTOR pInterface, bool fRel, bool fAbs)
{
    PDRVMAINMOUSE pDrv = RT_FROM_MEMBER(pInterface, DRVMAINMOUSE, IConnector);
    if (fRel)
        pDrv->u32DevCaps |= MOUSE_DEVCAP_RELATIVE;
    else
        pDrv->u32DevCaps &= ~MOUSE_DEVCAP_RELATIVE;
    if (fAbs)
        pDrv->u32DevCaps |= MOUSE_DEVCAP_ABSOLUTE;
    else
        pDrv->u32DevCaps &= ~MOUSE_DEVCAP_ABSOLUTE;

    pDrv->pMouse->sendMouseCapsNotifications();
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *)  Mouse::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINMOUSE   pDrv    = PDMINS_2_DATA(pDrvIns, PDRVMAINMOUSE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUSECONNECTOR, &pDrv->IConnector);
    return NULL;
}


/**
 * Destruct a mouse driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) Mouse::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVMAINMOUSE pData = PDMINS_2_DATA(pDrvIns, PDRVMAINMOUSE);
    LogFlow(("Mouse::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pData->pMouse)
    {
        AutoWriteLock mouseLock(pData->pMouse COMMA_LOCKVAL_SRC_POS);
        for (unsigned cDev = 0; cDev < MOUSE_MAX_DEVICES; ++cDev)
            if (pData->pMouse->mpDrv[cDev] == pData)
            {
                pData->pMouse->mpDrv[cDev] = NULL;
                break;
            }
    }
}


/**
 * Construct a mouse driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Mouse::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVMAINMOUSE pData = PDMINS_2_DATA(pDrvIns, PDRVMAINMOUSE);
    LogFlow(("drvMainMouse_Construct: iInstance=%d\n", pDrvIns->iInstance));
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
    pDrvIns->IBase.pfnQueryInterface        = Mouse::drvQueryInterface;

    pData->IConnector.pfnReportModes        = Mouse::mouseReportModes;

    /*
     * Get the IMousePort interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIMOUSEPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMIMOUSEPORT_IID);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No mouse port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Mouse object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }
    pData->pMouse = (Mouse *)pv;        /** @todo Check this cast! */
    unsigned cDev;
    {
        AutoReadLock mouseLock(pData->pMouse COMMA_LOCKVAL_SRC_POS);

        for (cDev = 0; cDev < MOUSE_MAX_DEVICES; ++cDev)
            if (!pData->pMouse->mpDrv[cDev])
            {
                pData->pMouse->mpDrv[cDev] = pData;
                break;
            }
    }
    if (cDev == MOUSE_MAX_DEVICES)
        return VERR_NO_MORE_HANDLES;

    return VINF_SUCCESS;
}


/**
 * Main mouse driver registration record.
 */
const PDMDRVREG Mouse::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainMouse",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main mouse driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MOUSE,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMAINMOUSE),
    /* pfnConstruct */
    Mouse::drvConstruct,
    /* pfnDestruct */
    Mouse::drvDestruct,
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
