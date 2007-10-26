/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Implementation of Linux-specific keyboard functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_GUI

#include <X11/Xlib.h>
#include <XKeyboard.h>
#include <VBox/log.h>
#include "keyboard.h"

static bool gInitStatus = false;

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
 * Dump the keyboard layout to the release log.  The result will only make
 * sense if the user is using an X.org or XFree86 server locally.
 */
static void dumpLayout(Display *display)
{
    LogRel(("Your keyboard layout does not appear to fully supported by VirtualBox.  "
            "If you would like to help us improve the product, please submit a "
            "bug report and attach this logfile.  Please note that the following "
            "table will only be valid if you are using an X.org or XFree86 server "
            "locally.\n\nThe correct table for your layout is:\n"));
    LogRel(("\""));
    printKey(display, 49);
    for (int i = 10; i <= 21; ++i)
    {
        LogRel(("\",\""));
        printKey(display, i);
    }
    LogRel(("\",\n"));
    LogRel(("\""));
    printKey(display, 24);
    for (int i = 25; i <= 35; ++i)
    {
        LogRel(("\",\""));
        printKey(display, i);
    }
    LogRel(("\",\n"));
    LogRel(("\""));
    printKey(display, 38);
    for (int i = 39; i <= 48; ++i)
    {
        LogRel(("\",\""));
        printKey(display, i);
    }
    LogRel(("\",\""));
    printKey(display, 51);
    LogRel(("\",\n"));
    LogRel(("\""));
    printKey(display, 52);
    for (int i = 53; i <= 61; ++i)
    {
        LogRel(("\",\""));
        printKey(display, i);
    }
    LogRel(("\",\""));
    printKey(display, 94); /* The 102nd key */
    LogRel(("\",\""));
    printKey(display, 211); /* The Brazilian key */
    LogRel(("\",\""));
    printKey(display, 133); /* The Yen key */
    LogRel(("\"\n\n"));
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
    if (0 != X11DRV_InitKeyboard(dpy))
    {
        /* The layout found was optimal. */
        gInitStatus = true;
    }
    return true;
}

/**
 * Do deferred logging after initialisation
 */
void doXKeyboardLogging(Display *dpy)
{
    if (!gInitStatus)
    {
        dumpLayout(dpy);
    }
}

/*
 * Translate an X server scancode to a PC keyboard scancode.
 */
void handleXKeyEvent(Display *, XEvent *event, WINEKEYBOARDINFO *wineKbdInfo)
{
    // call the WINE event handler
    XKeyEvent *keyEvent = &event->xkey;
    unsigned scan = X11DRV_KeyEvent(keyEvent->keycode);
    wineKbdInfo->wScan = scan & 0xFF;
    wineKbdInfo->dwFlags = scan >> 8;
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
