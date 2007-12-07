/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of Mouse class
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

#ifndef ____H_MOUSEIMPL
#define ____H_MOUSEIMPL

#include <VBox/pdm.h>

/** Simple mouse event class. */
class MouseEvent
{
public:
    MouseEvent() : dx(0), dy(0), dz(0), state(-1) {}
    MouseEvent(int _dx, int _dy, int _dz, int _state) :
        dx(_dx), dy(_dy), dz(_dz), state(_state) {}
    bool isValid()
    {
        return state != -1;
    }
    // not logical to be int but that's how it's defined in QEMU
    /** @todo r=bird: and what is the logical declaration then? We'll be using int32_t btw. */
    int dx, dy, dz;
    int state;
};

class Mouse
{
public:

    Mouse() : fAbsolute(false), fNeedsHostCursor(false), mpDrv(NULL), uHostCaps(0) {}

    // IMouse methods
    int  PutMouseEvent(LONG dx, LONG dy, LONG dz, LONG buttonState);
    int  PutMouseEventAbsolute(LONG x, LONG y, LONG dz, LONG buttonState);

    int  setAbsoluteCoordinates(bool fAbsolute);
    int  setNeedsHostCursor(bool fNeedsHostCursor);
#ifdef RT_OS_L4
    // So far L4Con does not support an own mouse pointer.
    bool getAbsoluteCoordinates() { return false; }
#else
    bool getAbsoluteCoordinates() { return fAbsolute; }
#endif
    bool getNeedsHostCursor() { return fNeedsHostCursor; }
    int  setHostCursor(bool enable);

    static const PDMDRVREG  DrvReg;

private:

    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

    /** Guest supports absolute coordinates */
    bool fAbsolute;
    /** Guest is not able to draw a cursor itself */
    bool fNeedsHostCursor;
    /** Pointer to the associated mouse driver. */
    struct DRVMAINMOUSE    *mpDrv;
    /** Host capabilities */
    LONG uHostCaps;
};

extern Mouse *gMouse;

#endif // ____H_MOUSEIMPL
