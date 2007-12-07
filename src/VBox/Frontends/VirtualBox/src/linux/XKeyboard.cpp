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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <X11/Xlib.h>
#include <XKeyboard.h>

// some WINE types we need
#ifndef HWND
typedef void *HWND;
#endif
#ifndef BYTE
typedef unsigned char BYTE;
#endif

// WINE keyboard prototypes
extern "C"
{
    void X11DRV_InitKeyboard(Display *dpy);
    void X11DRV_KeyEvent(Display *dpy, XEvent *event, WINEKEYBOARDINFO *wKbInfo);
    int X11DRV_GetKeysymsPerKeycode();
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
    X11DRV_InitKeyboard(dpy);
    return true;
}

/*
 * Translate an X server scancode to a PC keyboard scancode.
 */
void handleXKeyEvent(Display *dpy, XEvent *event, WINEKEYBOARDINFO *wineKbdInfo)
{
    // call the WINE event handler
    X11DRV_KeyEvent(dpy, event, wineKbdInfo);
}

int getKeysymsPerKeycode()
{
    return X11DRV_GetKeysymsPerKeycode();
}
