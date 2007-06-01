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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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

//
// global variables
//

Display *dpy_global;
BYTE InputKeyStateTable[256];

// WINE keyboard prototypes
extern "C"
{
    void X11DRV_InitKeyboard( BYTE *key_state_table );
    void X11DRV_KeyEvent(XKeyEvent *event, WINEKEYBOARDINFO *wKbInfo);
    int X11DRV_GetKeysymsPerKeycode();
}

/*
 * This needs to be called to initialize the WINE keyboard subsystem
 * within our environment.
 */
bool initXKeyboardSafe(Display *dpy)
{
    // update the global display pointer
    dpy_global = dpy;
    X11DRV_InitKeyboard(InputKeyStateTable);
    return true;
}

/*
 * our custom X keyboard event handler with the goal to try everything
 * possible to undo the dreaded scancode to X11 conversion. Sigh!
 */
void handleXKeyEventSafe(Display *dpy, XEvent *event, WINEKEYBOARDINFO *wineKbdInfo)
{
    // update the global display pointer
    dpy_global = dpy;
    // call the WINE event handler
    X11DRV_KeyEvent((XKeyEvent*)event, wineKbdInfo);
}

int getKeysymsPerKeycodeSafe()
{
    return X11DRV_GetKeysymsPerKeycode();
}
