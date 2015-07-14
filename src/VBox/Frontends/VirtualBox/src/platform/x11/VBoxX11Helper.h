/* $Id$ */
/** @file
 * VBox Qt GUI - VBox X11 helper functions.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxX11Helpers_h___
#define ___VBoxX11Helpers_h___

/** X11: Inits the screen saver save/restore mechanism. */
void X11ScreenSaverSettingsInit();
/** X11: Saves screen saver settings. */
void X11ScreenSaverSettingsSave();
/** X11: Restores previously saved screen saver settings. */
void X11ScreenSaverSettingsRestore();

/** X11: Determines whether current Window Manager is KWin. */
bool X11IsWindowManagerKWin();

#endif /* !___VBoxX11Helpers_h___ */

