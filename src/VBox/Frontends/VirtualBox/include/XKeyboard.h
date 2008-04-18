/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of Linux-specific keyboard functions
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

#ifndef __XKeyboard_h__
#define __XKeyboard_h__

// our structure used to return keyboard event information
typedef struct _WINEKEYBOARDINFO
{
    unsigned short wVk;
    unsigned short wScan;
    unsigned long dwFlags;
    unsigned long time;
} WINEKEYBOARDINFO;

// initialize the X keyboard subsystem
bool initXKeyboard(Display *dpy);
// our custom keyboard handler
void handleXKeyEvent(Display *dpy, XEvent *event, WINEKEYBOARDINFO *wineKbdInfo);
// returns the number of keysyms per keycode (only valid after initXKeyboard())
int getKeysymsPerKeycode();
// Called after release logging is started, in case initXKeyboard wishes to log
// anything
void doXKeyboardLogging(Display *dpy);

#endif // __XKeyboard_h__
