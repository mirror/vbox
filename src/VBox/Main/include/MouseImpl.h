/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef ____H_MOUSEIMPL
#define ____H_MOUSEIMPL

#include "VirtualBoxBase.h"
#include "ConsoleEvents.h"
#include "ConsoleImpl.h"
#include <VBox/pdmdrv.h>

/** Simple mouse event class. */
class MouseEvent
{
public:
    MouseEvent() : dx(0), dy(0), dz(0), dw(0), state(-1) {}
    MouseEvent(int32_t _dx, int32_t _dy, int32_t _dz, int32_t _dw, int32_t _state) :
        dx(_dx), dy(_dy), dz(_dz), dw(_dw), state(_state) {}
    bool isValid()
    {
        return state != -1;
    }
    /* Note: dw is the horizontal scroll wheel */
    int32_t dx, dy, dz, dw;
    int32_t state;
};
// template instantiation
typedef ConsoleEventBuffer<MouseEvent> MouseEventBuffer;

class ATL_NO_VTABLE Mouse :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<Mouse, IMouse>,
    public VirtualBoxSupportTranslation<Mouse>,
    VBOX_SCRIPTABLE_IMPL(IMouse)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (Mouse)

    DECLARE_NOT_AGGREGATABLE(Mouse)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Mouse)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (IMouse)
        COM_INTERFACE_ENTRY2 (IDispatch, IMouse)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Mouse)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Console *parent);
    void uninit();

    // IMouse properties
    STDMETHOD(COMGETTER(AbsoluteSupported)) (BOOL *absoluteSupported);
    STDMETHOD(COMGETTER(NeedsHostCursor)) (BOOL *needsHostCursor);

    // IMouse methods
    STDMETHOD(PutMouseEvent)(LONG dx, LONG dy, LONG dz, LONG dw,
                             LONG buttonState);
    STDMETHOD(PutMouseEventAbsolute)(LONG x, LONG y, LONG dz, LONG dw,
                                     LONG buttonState);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Mouse"; }

    static const PDMDRVREG  DrvReg;

private:

    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

    const ComObjPtr<Console, ComWeakRef> mParent;
    /** Pointer to the associated mouse driver. */
    struct DRVMAINMOUSE    *mpDrv;

    LONG uHostCaps;
    uint32_t mLastAbsX;
    uint32_t mLastAbsY;
};

#endif // ____H_MOUSEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
