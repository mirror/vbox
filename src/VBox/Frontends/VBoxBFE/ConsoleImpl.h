/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of Console class
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ____H_CONSOLEIMPL
#define ____H_CONSOLEIMPL

/*
 * Host key handling.
 *
 * The golden rule is that host-key combinations should not be seen
 * by the guest. For instance a CAD should not have any extra RCtrl down
 * and RCtrl up around itself. Nor should a resume be followed by a Ctrl-P
 * that could encourage applications to start printing.
 *
 * We must not confuse the hostkey processing into any release sequences
 * either, the host key is supposed to be explicitly pressing one key.
 *
 * Quick state diagram:
 *
 *            host key down alone
 *  (Normal) ---------------
 *    ^ ^                  |
 *    | |                  v          host combination key down
 *    | |            (Host key down) ----------------
 *    | | host key up v    |                        |
 *    | |--------------    | other key down         v           host combination key down
 *    |                    |                  (host key used) -------------
 *    |                    |                        |      ^              |
 *    |              (not host key)--               |      |---------------
 *    |                    |     |  |               |
 *    |                    |     ---- other         |
 *    |  modifiers = 0     v                        v
 *    -----------------------------------------------
 */
typedef enum _HKEYSTATE
{
    /** The initial and most common state, pass keystrokes to the guest.
     * Next state: HKEYSTATE_DOWN
     * Prev state: Any */
    HKEYSTATE_NORMAL = 1,
    /** The host key has been pressed down.
     * Prev state: HKEYSTATE_NORMAL
     * Next state: HKEYSTATE_NORMAL - host key up, capture toggle.
     * Next state: HKEYSTATE_USED   - host key combination down.
     * Next state: HKEYSTATE_NOT_IT - non-host key combination down.
     */
    HKEYSTATE_DOWN,
    /** A host key combination was pressed.
     * Prev state: HKEYSTATE_DOWN
     * Next state: HKEYSTATE_NORMAL - when modifiers are all 0
     */
    HKEYSTATE_USED,
    /** A non-host key combination was attempted. Send hostkey down to the
     * guest and continue until all modifiers have been released.
     * Prev state: HKEYSTATE_DOWN
     * Next state: HKEYSTATE_NORMAL - when modifiers are all 0
     */
    HKEYSTATE_NOT_IT
} HKEYSTATE;

typedef enum _CONEVENT
{
    CONEVENT_SCREENUPDATE = 1,
    CONEVENT_KEYUP,
    CONEVENT_KEYDOWN,
    CONEVENT_MOUSEMOVE,
    CONEVENT_MOUSEBUTTONUP,
    CONEVENT_MOUSEBUTTONDOWN,
    CONEVENT_FOCUSCHANGE,

    CONEVENT_USR_QUIT,
    CONEVENT_USR_SCREENUPDATERECT,
    CONEVENT_USR_SCREENRESIZE,
    CONEVENT_USR_TITLEBARUPDATE,
    CONEVENT_USR_SECURELABELUPDATE,
    CONEVENT_USR_MOUSEPOINTERCHANGE,

    CONEVENT_QUIT,

    CONEVENT_NONE
} CONEVENT;

class Console
{
public:
    Console()
    {
        enmHKeyState  = HKEYSTATE_NORMAL;
        mfInitialized = false;
        mfInputGrab   = false;
    }
    virtual ~Console() {}

    virtual void     updateTitlebar() = 0;
    virtual void     updateTitlebarProgress(const char *pszStr, int iPercent) = 0;

    virtual void     inputGrabStart() = 0;
    virtual void     inputGrabEnd() = 0;
    virtual bool     inputGrabbed() { return mfInputGrab; }
    virtual void     resetCursor() {}

    virtual void     mouseSendEvent(int dz) = 0;
    virtual void     onMousePointerShapeChange(bool fVisible,
                                               bool fAlpha, uint32_t xHot,
                                               uint32_t yHot, uint32_t width,
                                               uint32_t height, void *pShape) = 0;

    virtual CONEVENT eventWait() = 0;
    virtual void     eventQuit() = 0;
            bool     initialized() { return mfInitialized; }
    virtual void     progressInfo(PVM pVM, unsigned uPercent, void *pvUser) = 0;
    virtual void     resetKeys(void) = 0;

protected:
    HKEYSTATE enmHKeyState;
    bool      mfInitialized;
    bool      mfInputGrab;
};

extern Console *gConsole;

#endif // ____H_CONSOLEIMPL
