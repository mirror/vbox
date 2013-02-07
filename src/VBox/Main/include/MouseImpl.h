/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_MOUSEIMPL
#define ____H_MOUSEIMPL

#include "VirtualBoxBase.h"
#include "ConsoleEvents.h"
#include "ConsoleImpl.h"
#include "EventImpl.h"
#include <VBox/vmm/pdmdrv.h>

/** Maximum number of devices supported */
enum { MOUSE_MAX_DEVICES = 3 };
/** Mouse driver instance data. */
typedef struct DRVMAINMOUSE DRVMAINMOUSE, *PDRVMAINMOUSE;

class ATL_NO_VTABLE Mouse :
    public VirtualBoxBase
    , VBOX_SCRIPTABLE_IMPL(IMouse)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Mouse, IMouse)

    DECLARE_NOT_AGGREGATABLE(Mouse)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Mouse)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IMouse)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Mouse)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Console *parent);
    void uninit();

    // IMouse properties
    STDMETHOD(COMGETTER(AbsoluteSupported)) (BOOL *absoluteSupported);
    STDMETHOD(COMGETTER(RelativeSupported)) (BOOL *relativeSupported);
    STDMETHOD(COMGETTER(NeedsHostCursor)) (BOOL *needsHostCursor);

    // IMouse methods
    STDMETHOD(PutMouseEvent)(LONG dx, LONG dy, LONG dz, LONG dw,
                             LONG buttonState);
    STDMETHOD(PutMouseEventAbsolute)(LONG x, LONG y, LONG dz, LONG dw,
                                     LONG buttonState);
    STDMETHOD(COMGETTER(EventSource)) (IEventSource ** aEventSource);

    static const PDMDRVREG  DrvReg;

    Console *getParent() const
    {
        return mParent;
    }

    /** notify the front-end of guest capability changes */
    void onVMMDevGuestCapsChange(uint32_t fCaps)
    {
        mfVMMDevGuestCaps = fCaps;
        sendMouseCapsNotifications();
    }

private:

    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(void)   mouseReportModes (PPDMIMOUSECONNECTOR pInterface, bool fRel, bool fAbs);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

    HRESULT updateVMMDevMouseCaps(uint32_t fCapsAdded, uint32_t fCapsRemoved);
    HRESULT reportRelEventToMouseDev(int32_t dx, int32_t dy, int32_t dz,
                                 int32_t dw, uint32_t fButtons);
    HRESULT reportAbsEventToMouseDev(int32_t mouseXAbs, int32_t mouseYAbs,
                                 int32_t dz, int32_t dw, uint32_t fButtons);
    HRESULT reportAbsEventToVMMDev(int32_t mouseXAbs, int32_t mouseYAbs);
    HRESULT reportAbsEvent(int32_t mouseXAbs, int32_t mouseYAbs,
                           int32_t dz, int32_t dw, uint32_t fButtons,
                           bool fUsesVMMDevEvent);
    HRESULT convertDisplayRes(LONG x, LONG y, int32_t *pcX, int32_t *pcY,
                              bool *pfValid);

    void getDeviceCaps(bool *pfAbs, bool *pfRel);
    void sendMouseCapsNotifications(void);
    bool guestNeedsHostCursor(void);
    bool vmmdevCanAbs(void);
    bool deviceCanAbs(void);
    bool supportsAbs(void);
    bool supportsRel(void);

    Console * const         mParent;
    /** Pointer to the associated mouse driver. */
    struct DRVMAINMOUSE    *mpDrv[MOUSE_MAX_DEVICES];

    uint32_t mfVMMDevGuestCaps;  /** We cache this to avoid access races */
    int32_t mcLastAbsX;
    int32_t mcLastAbsY;
    uint32_t mfLastButtons;

    const ComObjPtr<EventSource> mEventSource;
    VBoxEventDesc                mMouseEvent;

    void fireMouseEvent(bool fAbsolute, LONG x, LONG y, LONG dz, LONG dw, LONG Buttons);
};

#endif // !____H_MOUSEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
