/* $Id$ */
/** @file
 * VBox Qt GUI - Windows keyboard handling..
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_GUI
 
#include "WinKeyboard.h"
#include <iprt/assert.h>
#include <VBox/log.h>

#include <stdio.h>


/* Beautification of log output */
#define VBOX_BOOL_TO_STR_STATE(x)   (x) ? "ON" : "OFF"
#define VBOX_CONTROL_TO_STR_NAME(x) ((x == VK_CAPITAL) ? "CAPS" : (x == VK_SCROLL ? "SCROLL" : ((x == VK_NUMLOCK) ? "NUM" : "UNKNOWN")))

/* A structure that contains internal control state representation */
typedef struct VBoxModifiersState_t {
    bool fNumLockOn;                        /** A state of NUM LOCK */
    bool fCapsLockOn;                       /** A state of CAPS LOCK */
    bool fScrollLockOn;                     /** A state of SCROLL LOCK */
} VBoxModifiersState_t;

/**
 * Get current state of a keyboard modifier.
 *
 * @param   idModifier        Modifier to examine (VK_CAPITAL, VK_SCROLL or VK_NUMLOCK)
 *
 * @returns modifier state or asserts if wrong modifier is specified.
 */
static bool winGetModifierState(int idModifier)
{
    AssertReturn((idModifier == VK_CAPITAL) || (idModifier == VK_SCROLL) || (idModifier == VK_NUMLOCK), false);
    return (GetKeyState(idModifier) & 0x0001) != 0;
}

/**
 * Set current state of a keyboard modifier.
 *
 * @param   idModifier        Modifier to set (VK_CAPITAL, VK_SCROLL or VK_NUMLOCK)
 * @param   fState            State to set
 */
static void winSetModifierState(int idModifier, bool fState)
{
    AssertReturnVoid((idModifier == VK_CAPITAL) || (idModifier == VK_SCROLL) || (idModifier == VK_NUMLOCK));

    /* If the modifier is already in desired state, just do nothing. Otherwise, toggle it. */
    if (winGetModifierState(idModifier) != fState)
    {
        /* Simulate KeyUp+KeyDown keystroke */
        keybd_event(idModifier, 0, KEYEVENTF_EXTENDEDKEY, 0);
        keybd_event(idModifier, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

        LogRel2(("HID LEDs sync: setting %s state to %s (0x%X).\n",
                 VBOX_CONTROL_TO_STR_NAME(idModifier), VBOX_BOOL_TO_STR_STATE(fState), MapVirtualKey(idModifier, MAPVK_VK_TO_VSC)));
    }
    else
    {
        LogRel2(("HID LEDs sync: setting %s state: skipped: state is already %s (0x%X).\n",
                 VBOX_CONTROL_TO_STR_NAME(idModifier), VBOX_BOOL_TO_STR_STATE(fState), MapVirtualKey(idModifier, MAPVK_VK_TO_VSC)));
    }
}

/** Set all HID LEDs at once. */
static bool winSetHidLeds(bool fNumLockOn, bool fCapsLockOn, bool fScrollLockOn)
{
    winSetModifierState(VK_NUMLOCK, fNumLockOn);
    winSetModifierState(VK_CAPITAL, fCapsLockOn);
    winSetModifierState(VK_SCROLL,  fScrollLockOn);
    return true;
}

/** Check if specified LED states correspond to the system modifier states. */
bool winHidLedsInSync(bool fNumLockOn, bool fCapsLockOn, bool fScrollLockOn)
{
    if (winGetModifierState(VK_NUMLOCK) != fNumLockOn)
        return false;

    if (winGetModifierState(VK_CAPITAL) != fCapsLockOn)
        return false;

    if (winGetModifierState(VK_SCROLL) != fScrollLockOn)
        return false;

    return true;
}

/**
 * Allocate memory and store modifier states there.
 *
 * @returns allocated memory witch contains modifier states or NULL.
 */
void * WinHidDevicesKeepLedsState(void)
{
    VBoxModifiersState_t *pState;

    pState = (VBoxModifiersState_t *)malloc(sizeof(VBoxModifiersState_t));
    if (pState)
    {
        pState->fNumLockOn    = winGetModifierState(VK_NUMLOCK);
        pState->fCapsLockOn   = winGetModifierState(VK_CAPITAL);
        pState->fScrollLockOn = winGetModifierState(VK_SCROLL);

        LogRel2(("HID LEDs sync: host state captured: NUM(%s) CAPS(%s) SCROLL(%s)\n",
                 VBOX_BOOL_TO_STR_STATE(pState->fNumLockOn),
                 VBOX_BOOL_TO_STR_STATE(pState->fCapsLockOn),
                 VBOX_BOOL_TO_STR_STATE(pState->fScrollLockOn)));

        return (void *)pState;
    }

    LogRel2(("HID LEDs sync: unable to allocate memory for HID LEDs synchronization data. LEDs sync will be disabled.\n"));

    return NULL;
}

/**
 * Restore host modifier states and free memory.
 */
void WinHidDevicesApplyAndReleaseLedsState(void *pData)
{
    VBoxModifiersState_t *pState = (VBoxModifiersState_t *)pData;

    if (pState)
    {
        LogRel2(("HID LEDs sync: attempt to restore host state: NUM(%s) CAPS(%s) SCROLL(%s)\n",
                 VBOX_BOOL_TO_STR_STATE(pState->fNumLockOn),
                 VBOX_BOOL_TO_STR_STATE(pState->fCapsLockOn),
                 VBOX_BOOL_TO_STR_STATE(pState->fScrollLockOn)));

        if (winSetHidLeds(pState->fNumLockOn, pState->fCapsLockOn, pState->fScrollLockOn))
            LogRel2(("HID LEDs sync: host state restored\n"));
        else
            LogRel2(("HID LEDs sync: host state restore failed\n"));

        free(pState);
    }
}

/**
 * Broadcast HID modifier states.
 *
 * @param   fNumLockOn        NUM LOCK state
 * @param   fCapsLockOn       CAPS LOCK state
 * @param   fScrollLockOn     SCROLL LOCK state
 */
void WinHidDevicesBroadcastLeds(bool fNumLockOn, bool fCapsLockOn, bool fScrollLockOn)
{
    LogRel2(("HID LEDs sync: start broadcast guest modifier states: NUM(%s) CAPS(%s) SCROLL(%s)\n",
             VBOX_BOOL_TO_STR_STATE(fNumLockOn),
             VBOX_BOOL_TO_STR_STATE(fCapsLockOn),
             VBOX_BOOL_TO_STR_STATE(fScrollLockOn)));

    if (winSetHidLeds(fNumLockOn, fCapsLockOn, fScrollLockOn))
        LogRel2(("HID LEDs sync: broadcast completed\n"));
    else
        LogRel2(("HID LEDs sync: broadcast failed\n"));
}
