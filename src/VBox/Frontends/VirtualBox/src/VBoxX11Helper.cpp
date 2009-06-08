/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 helpers.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "VBoxX11Helper.h"

#include <iprt/cdefs.h>
#include <QX11Info>

/* rhel3 build hack */
RT_BEGIN_DECLS
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
RT_END_DECLS

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
