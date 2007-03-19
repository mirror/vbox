/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Implementation of Linux-specific keyboard functions
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include <X11/Xlib.h>
#include <XKeyboard.h>
#include <cstring>

#define LOG_GROUP LOG_GROUP_GUI
#include <VBox/log.h>

/*****************************************************************************
*   Global Variables                                                         *
*****************************************************************************/

/** Lookup table to translate X keysyms of extended scancodes.  CTRL-PrtScn and keys not found on
    a 105-key keyboard are not yet handled.  Japanese and Korean keyboards should work but have
    not been tested yet.  See
    http://www.win.tue.nl/~aeb/linux/kbd/scancodes.html
    and the Xorg keyboard configuration files in /etc/X11/xkb for interesting information. */
static int ExtKeySymToScanCode[38] =
{
    0x147 /* 97 non-KP-home */, 0x148 /* 98 non-KP-up */, 0x149 /* 99 non-KP-PgUp */,
    0x14b /* 100 non-KP-left */, 0 /* 101 empty */, 0x14d /* 102 non-KP-right */,
    0x14f /* 103 non-KP-end */, 0x150 /* 104 non-KP-down */, 0x151 /* 105 non-KP-PgDn */,
    0x152 /* 106 non-KP-insert */, 0x153 /* 107 non-KP-del */, 0x11c /* 108 KP ENTER */,
    0x11d /* 109 RCTRL */, 0 /* 110 pause */, 0 /* 111 PrtScn */, 0x135 /* 112 / (KP) */,
    0x138 /* 113 R-ALT */, 0 /* 114 break */, 0x15b /* 115 left-Windows */,
    0x15c /* 116 right-Windows */, 0x15d /* 117 Windows-Menu */, 0 /* 118 */, 0 /* 119 */,
    0 /* 120 */, 0xf1 /* 121 Korean Hangul to Latin */, 0xf2 /* 122 Korean Hangul to Hanja */,
    0 /* 123 */, 0 /* 124 */, 0 /* 125 */, 0 /* 126 */, 0 /* 127 */, 0 /* 128 */,
    0x79 /* 129 Japanese Henkan */, 0 /* 130 */, 0x7b /* 131 Japanese Muhenkan */, 0 /* 132 */,
    0x7d /* 133 Japanese Yen */, 0x7e /* 134 Brazilian keypad . */
};

static int keysyms_per_keycode;  /** Number of keyboard language layouts the host has */

/**
 * Find out how many different language mappings there are for this keyboard.  This is needed
 * so that Dmitry can press <hostkey>+Q while using his Cyrillic keyboard layout :)
 */
bool initXKeyboard(Display *dpy)
{
    int min, max;
    KeySym *syms;
    /* Find out how approximately how many keys there are on the keyboard, needed for the next
       call */
    XDisplayKeycodes(dpy, &min, &max);
    /* Find out how many different language mappings there are for this keyboard.  Does anyone
       know a better way to get this value? */
    syms = XGetKeyboardMapping(dpy, min, max - min + 1, &keysyms_per_keycode);
    /* And free all the symbols which we just looked up, even though we aren't interested */
    XFree(syms);
    return true;
}

/**
 * Convert X11 keycodes back to scancodes.
 */
void handleXKeyEvent(Display * /* dpy */, XEvent *event, WINEKEYBOARDINFO *wineKbdInfo)
{
    unsigned int uKeyCode = event->xkey.keycode;
    memset(reinterpret_cast<void *>(wineKbdInfo), 0, sizeof(WINEKEYBOARDINFO));
    /* Basic scancodes are translated to keycodes by adding 8, so we just subtract again. */
    if ((uKeyCode >= 9) && (uKeyCode <= 96))
    {
        wineKbdInfo->wScan = uKeyCode - 8;
        return;
    }
    /* Extended scancodes need to be looked up in a table. */
    if ((uKeyCode >= 97) && (uKeyCode <= 134))
    {
        wineKbdInfo->wScan = ExtKeySymToScanCode[uKeyCode - 97] & 255;
        wineKbdInfo->dwFlags = ExtKeySymToScanCode[uKeyCode - 97] >> 8;
        return;
    }
    if (uKeyCode == 208) /* Japanese Hiragana to Katakana */
        wineKbdInfo->wScan = 0x70;
    else if (uKeyCode == 211) /* Japanese backslash/underscore and Brazilian
                                 backslash/question mark */
        wineKbdInfo->wScan = 0x73;
    /* If we couldn't find a translation, we just return 0. */
    return;
}

/**
 * Return the number of keyboard language mappings.
 */
int getKeysymsPerKeycode()
{
    return keysyms_per_keycode;
}
