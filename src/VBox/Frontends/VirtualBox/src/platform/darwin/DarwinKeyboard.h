/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility functions for handling Darwin Keyboard specific tasks.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___DarwinKeyboard_h___
#define ___DarwinKeyboard_h___

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* External includes: */
#include <CoreFoundation/CFBase.h>


RT_C_DECLS_BEGIN

/** Private hack for missing rightCmdKey enum. */
#define kEventKeyModifierRightCmdKeyMask (1<<27)

/** The scancode mask. */
#define VBOXKEY_SCANCODE_MASK       0x007f
/** Extended key. */
#define VBOXKEY_EXTENDED            0x0080
/** Modifier key. */
#define VBOXKEY_MODIFIER            0x0400
/** Lock key (like num lock and caps lock). */
#define VBOXKEY_LOCK                0x0800

/** Converts a darwin (virtual) key code to a set 1 scan code. */
unsigned DarwinKeycodeToSet1Scancode(unsigned uKeyCode);
/** Adjusts the modifier mask left / right using the current keyboard state. */
UInt32   DarwinAdjustModifierMask(UInt32 fModifiers, const void *pvCocoaEvent);
/** Converts a single modifier to a set 1 scan code. */
unsigned DarwinModifierMaskToSet1Scancode(UInt32 fModifiers);
/** Converts a single modifier to a darwin keycode. */
unsigned DarwinModifierMaskToDarwinKeycode(UInt32 fModifiers);
/** Converts a darwin keycode to a modifier mask. */
UInt32   DarwinKeyCodeToDarwinModifierMask(unsigned uKeyCode);

/** Disables or enabled global hot keys. */
void     DarwinDisableGlobalHotKeys(bool fDisable);

/** Start grabbing keyboard events.
  * @param   fGlobalHotkeys  Brings whether to disable global hotkeys or not. */
void     DarwinGrabKeyboard(bool fGlobalHotkeys);
/** Reverses the actions taken by DarwinGrabKeyboard. */
void     DarwinReleaseKeyboard();

/** Saves the states of leds for all HID devices attached to the system and return it. */
void    *DarwinHidDevicesKeepLedsState();

/** Applies LEDs @a pState release its resources afterwards. */
int      DarwinHidDevicesApplyAndReleaseLedsState(void *pState);
/** Set states for host keyboard LEDs.
  * @note This function will set led values for all
  *       keyboard devices attached to the system.
  * @param pState         Brings the pointer to saved LEDs state.
  * @param fNumLockOn     Turns on NumLock led if TRUE, off otherwise
  * @param fCapsLockOn    Turns on CapsLock led if TRUE, off otherwise
  * @param fScrollLockOn  Turns on ScrollLock led if TRUE, off otherwise */
void     DarwinHidDevicesBroadcastLeds(void *pState, bool fNumLockOn, bool fCapsLockOn, bool fScrollLockOn);

RT_C_DECLS_END


#endif /* !___DarwinKeyboard_h___ */

