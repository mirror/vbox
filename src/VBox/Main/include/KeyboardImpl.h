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

#ifndef ____H_KEYBOARDIMPL
#define ____H_KEYBOARDIMPL

#include "VirtualBoxBase.h"
#include "ConsoleEvents.h"

#include <VBox/pdmdrv.h>

/** Simple keyboard event class. */
class KeyboardEvent
{
public:
    KeyboardEvent() : scan(-1) {}
    KeyboardEvent(int _scan) : scan(_scan) {}
    bool isValid()
    {
        return (scan & ~0x80) && !(scan & ~0xFF);
    }
    int scan;
};
// template instantiation
typedef ConsoleEventBuffer<KeyboardEvent> KeyboardEventBuffer;

class Console;

class ATL_NO_VTABLE Keyboard :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <Keyboard, IKeyboard>,
    public VirtualBoxSupportTranslation <Keyboard>,
    VBOX_SCRIPTABLE_IMPL(IKeyboard)
{

public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (Keyboard)

    DECLARE_NOT_AGGREGATABLE(Keyboard)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Keyboard)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IKeyboard)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (Keyboard)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Console *aParent);
    void uninit();

    STDMETHOD(PutScancode)(LONG scancode);
    STDMETHOD(PutScancodes)(ComSafeArrayIn (LONG, scancodes),
                            ULONG *codesStored);
    STDMETHOD(PutCAD)();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Keyboard"; }

    static const PDMDRVREG  DrvReg;

    Console *getParent() const
    {
        return mParent;
    }

private:

    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

    const ComObjPtr <Console, ComWeakRef> mParent;
    /** Pointer to the associated keyboard driver. */
    struct DRVMAINKEYBOARD *mpDrv;
    /** Pointer to the device instance for the VMM Device. */
    PPDMDEVINS              mpVMMDev;
    /** Set after the first attempt to find the VMM Device. */
    bool                    mfVMMDevInited;
};

#endif // ____H_KEYBOARDIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
