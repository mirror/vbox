/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 helpers.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxX11Helper.h"

#include <iprt/cdefs.h>
#include <iprt/string.h>
#include <QX11Info>

/* rhel3 build hack */
RT_C_DECLS_BEGIN
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
RT_C_DECLS_END

static int  gX11ScreenSaverTimeout;
static BOOL gX11ScreenSaverDpmsAvailable;
static BOOL gX11DpmsState;

/**
 * Init the screen saver save/restore mechanism.
 */
void X11ScreenSaverSettingsInit()
{
    int     dummy;
    Display *display = QX11Info::display();
    gX11ScreenSaverDpmsAvailable =
        DPMSQueryExtension(display, &dummy, &dummy);
}

/**
 * Actually this is a big mess. By default the libSDL disables the screen
 * saver during the SDL_InitSubSystem() call and restores the saved settings
 * during the SDL_QuitSubSystem() call. This mechanism can be disabled by
 * setting the environment variable SDL_VIDEO_ALLOW_SCREENSAVER to 1. However,
 * there is a known bug in the Debian libSDL: If this environment variable is
 * set, the screen saver is still disabled but the old state is not restored
 * during SDL_QuitSubSystem()! So the only solution to overcome this problem
 * is to save and restore the state prior and after each of these function
 * calls.
 */
void X11ScreenSaverSettingsSave()
{
    int     dummy;
    CARD16  dummy2;
    Display *display = QX11Info::display();

    XGetScreenSaver(display, &gX11ScreenSaverTimeout, &dummy, &dummy, &dummy);
    if (gX11ScreenSaverDpmsAvailable)
        DPMSInfo(display, &dummy2, &gX11DpmsState);
}

/**
 * Restore previously saved screen saver settings.
 */
void X11ScreenSaverSettingsRestore()
{
    int     timeout, interval, preferBlank, allowExp;
    Display *display = QX11Info::display();

    XGetScreenSaver(display, &timeout, &interval, &preferBlank, &allowExp);
    timeout = gX11ScreenSaverTimeout;
    XSetScreenSaver(display, timeout, interval, preferBlank, allowExp);

    if (gX11DpmsState && gX11ScreenSaverDpmsAvailable)
        DPMSEnable(display);
}

/**
 * Determine if the current Window manager is KWin (KDE)
 */
bool X11IsWindowManagerKWin()
{
    Atom typeReturned;
    Atom utf8Atom;
    int formatReturned;
    unsigned long ulNitemsReturned;
    unsigned long ulDummy;
    unsigned char *pcData = NULL;
    bool fIsKWinManaged = false;
    Display *display = QX11Info::display();
    Atom propNameAtom;
    Window WMWindow = None;

    propNameAtom = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", True);
    if (XGetWindowProperty(display, QX11Info::appRootWindow(), propNameAtom,
                           0, 512, False, XA_WINDOW, &typeReturned,
                           &formatReturned, &ulNitemsReturned, &ulDummy, &pcData)
                            == Success)
    {

        if (typeReturned == XA_WINDOW && formatReturned == 32)
            WMWindow = *((Window*) pcData);
        if (pcData)
            XFree(pcData);
        if (WMWindow != None)
        {
            propNameAtom = XInternAtom(display, "_NET_WM_NAME", True);
            utf8Atom = XInternAtom(display, "UTF8_STRING", True);
            if (XGetWindowProperty(QX11Info::display(), WMWindow, propNameAtom,
                                   0, 512, False, utf8Atom, &typeReturned,
                                   &formatReturned, &ulNitemsReturned, &ulDummy, &pcData)
                    == Success)
            {
                fIsKWinManaged = RTStrCmp((const char*)pcData, "KWin") == 0;
                if (pcData)
                    XFree(pcData);
            }
        }
    }
    return fIsKWinManaged;
}

