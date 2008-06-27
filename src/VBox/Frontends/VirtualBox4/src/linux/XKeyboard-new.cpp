/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Implementation of Linux-specific keyboard functions
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

#define LOG_GROUP LOG_GROUP_GUI

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <XKeyboard.h>
#include <VBox/log.h>
#include "keyboard.h"

static unsigned gfByLayoutOK = 1;
static unsigned gfByTypeOK = 1;

/**
 * DEBUG function
 * Print a key to the release log in the format needed for the Wine
 * layout tables.
 */
static void printKey(Display *display, int keyc)
{
    bool was_escape = false;

    for (int i = 0; i < 2; ++i)
    {
        int keysym = XKeycodeToKeysym (display, keyc, i);

        int val = keysym & 0xff;
        if ('\\' == val)
        {
            LogRel(("\\\\"));
        }
        else if ('"' == val)
        {
            LogRel(("\\\""));
        }
        else if ((val > 32) && (val < 127))
        {
            if (
                   was_escape
                && (
                       ((val >= '0') && (val <= '9'))
                    || ((val >= 'A') && (val <= 'F'))
                    || ((val >= 'a') && (val <= 'f'))
                   )
               )
            {
                LogRel(("\"\""));
            }
            LogRel(("%c", (char) val));
        }
        else
        {
            LogRel(("\\x%x", val));
            was_escape = true;
        }
    }
}

/**
 * DEBUG function
 * Dump the keyboard layout to the release log.
 */
static void dumpLayout(Display *display)
{
    LogRel(("Your keyboard layout does not appear to fully supported by\n"
            "VirtualBox. If you would like to help us improve the product,\n"
            "please submit a bug report and attach this logfile.\n\n"
            "The correct table for your layout is:\n"));
    /* First, build up a table of scan-to-key code mappings */
    unsigned scanToKeycode[512] = { 0 };
    int minKey, maxKey;
    XDisplayKeycodes(display, &minKey, &maxKey);
    for (int i = minKey; i < maxKey; ++i)
        scanToKeycode[X11DRV_KeyEvent(i)] = i;
    LogRel(("\""));
    printKey(display, scanToKeycode[0x29]); /* `~ */
    for (int i = 2; i <= 0xd; ++i) /* 1! - =+ */
    {
        LogRel(("\",\""));
        printKey(display, scanToKeycode[i]);
    }
    LogRel(("\",\n"));
    LogRel(("\""));
    printKey(display, scanToKeycode[0x10]); /* qQ */
    for (int i = 0x11; i <= 0x1b; ++i) /* wW - ]} */
    {
        LogRel(("\",\""));
        printKey(display, scanToKeycode[i]);
    }
    LogRel(("\",\n"));
    LogRel(("\""));
    printKey(display, scanToKeycode[0x1e]); /* aA */
    for (int i = 0x1f; i <= 0x28; ++i) /* sS - '" */
    {
        LogRel(("\",\""));
        printKey(display, scanToKeycode[i]);
    }
    LogRel(("\",\""));
    printKey(display, scanToKeycode[0x2b]); /* \| */
    LogRel(("\",\n"));
    LogRel(("\""));
    printKey(display, scanToKeycode[0x2c]); /* zZ */
    for (int i = 0x2d; i <= 0x35; ++i) /* xX - /? */
    {
        LogRel(("\",\""));
        printKey(display, scanToKeycode[i]);
    }
    LogRel(("\",\""));
    printKey(display, scanToKeycode[0x56]); /* The 102nd key */
    LogRel(("\",\""));
    printKey(display, scanToKeycode[0x73]); /* The Brazilian key */
    LogRel(("\",\""));
    printKey(display, scanToKeycode[0x7d]); /* The Yen key */
    LogRel(("\"\n\n"));
}

/**
 * DEBUG function
 * Dump the keyboard type tables to the release log.
 */
static void dumpType(Display *display)
{
    LogRel(("Your keyboard type does not appear to be known to VirtualBox. If\n"
            "you would like to help us improve the product, please submit a bug\n"
            "report, attach this logfile and provide information about what type\n"
            "of keyboard you have and whether you are using a remote X server or\n"
            "something similar.\n\n"
            "The tables for your keyboard are:\n"));
    for (unsigned i = 0; i < 256; ++i)
    {
        LogRel(("0x%x", X11DRV_KeyEvent(i)));
        if (i < 255)
            LogRel((", "));
        if (15 == (i % 16))
            LogRel(("\n"));
    }
    LogRel(("and\n"));
    LogRel(("NULL, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x,\n0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
            XKeysymToKeycode(display, XK_Control_L),
            XKeysymToKeycode(display, XK_Shift_L),
            XKeysymToKeycode(display, XK_Caps_Lock),
            XKeysymToKeycode(display, XK_Tab),
            XKeysymToKeycode(display, XK_Escape),
            XKeysymToKeycode(display, XK_Return),
            XKeysymToKeycode(display, XK_Up),
            XKeysymToKeycode(display, XK_Down),
            XKeysymToKeycode(display, XK_Left),
            XKeysymToKeycode(display, XK_Right),
            XKeysymToKeycode(display, XK_F1),
            XKeysymToKeycode(display, XK_F2),
            XKeysymToKeycode(display, XK_F3),
            XKeysymToKeycode(display, XK_F4),
            XKeysymToKeycode(display, XK_F5),
            XKeysymToKeycode(display, XK_F6),
            XKeysymToKeycode(display, XK_F7),
            XKeysymToKeycode(display, XK_F8)));
}

/*
 * This function builds a table mapping the X server's scan codes to PC
 * keyboard scan codes.  The logic of the function is that while the
 * X server may be using a different set of scan codes (if for example
 * it is running on a non-PC machine), the keyboard layout should be similar
 * to a PC layout.  So we look at the symbols attached to each key on the
 * X server, find the PC layout which is closest to it and remember the
 * mappings.
 */
bool initXKeyboard(Display *dpy)
{
    X11DRV_InitKeyboard(dpy, &gfByLayoutOK, &gfByTypeOK);
    /* It will almost always work to some extent */
    return true;
}

/**
 * Do deferred logging after initialisation
 */
void doXKeyboardLogging(Display *dpy)
{
    if ((gfByLayoutOK != 1) && (1 == gfByTypeOK))
        dumpLayout(dpy);
    if ((1 == gfByLayoutOK) && (gfByTypeOK != 1))
        dumpType(dpy);
    if ((gfByLayoutOK != 1) && (gfByTypeOK != 1))
        LogRel(("Failed to recognize the keyboard mapping or to guess it based on\n"
                "the keyboard layout.  It is very likely that some keys will not\n"
                "work correctly in the guest.  If you would like to help us improve\n"
                "the product, please submit a bug report, giving us information\n"
                "about your keyboard type, its layout and other relevant\n"
                "information such as whether you are using a remote X server or\n"
                "something similar.\n"));
}

/*
 * Translate an X server scancode to a PC keyboard scancode.
 */
unsigned handleXKeyEvent(XEvent *event)
{
    // call the WINE event handler
    return X11DRV_KeyEvent(event->xkey.keycode);
}

/**
 * Return the maximum number of symbols which can be associated with a key
 * in the current layout.  This is needed so that Dmitry can use keyboard
 * shortcuts without switching to Latin layout, by looking at all symbols
 * which a given key can produce and seeing if any of them match the shortcut.
 */
int getKeysymsPerKeycode()
{
    /* This can never be higher than 8, and returning too high a value is
       completely harmless. */
    return 8;
}
