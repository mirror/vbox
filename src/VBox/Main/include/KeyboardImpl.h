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

#ifndef ____H_KEYBOARDIMPL
#define ____H_KEYBOARDIMPL

#include "VirtualBoxBase.h"
#include "ConsoleEvents.h"
#include "EventImpl.h"

#include <VBox/vmm/pdmdrv.h>

/** Limit of simultaneously attached devices (just USB and/or PS/2). */
enum { KEYBOARD_MAX_DEVICES = 2 };

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
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IKeyboard)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Keyboard, IKeyboard)

    DECLARE_NOT_AGGREGATABLE(Keyboard)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Keyboard)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IKeyboard)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(Keyboard)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Console *aParent);
    void uninit();

    STDMETHOD(PutScancode)(LONG scancode);
    STDMETHOD(PutScancodes)(ComSafeArrayIn(LONG, scancodes),
                            ULONG *codesStored);
    STDMETHOD(PutCAD)();

    STDMETHOD(COMGETTER(EventSource))(IEventSource ** aEventSource);

    static const PDMDRVREG  DrvReg;

    Console *getParent() const
    {
        return mParent;
    }

private:

    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(void)   keyboardSetActive(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

    Console * const         mParent;
    /** Pointer to the associated keyboard driver(s). */
    struct DRVMAINKEYBOARD *mpDrv[KEYBOARD_MAX_DEVICES];
    /** Pointer to the device instance for the VMM Device. */
    PPDMDEVINS              mpVMMDev;
    /** Set after the first attempt to find the VMM Device. */
    bool                    mfVMMDevInited;

    const ComObjPtr<EventSource> mEventSource;
};

#endif // !____H_KEYBOARDIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
