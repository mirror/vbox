/* $Id$ */
/** @file
 * VBoxSeamless - Display notifications
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_VBoxTray_VBoxDisplay_h
#define GA_INCLUDED_SRC_WINNT_VBoxTray_VBoxDisplay_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @todo r=bird: This file has the same name as ../include/VBoxDisplay.h
 *        which some of the VBoxTray source files also wish to include.
 *
 *        The result is a certifiable mess. To quote a Tina Fey's character in
 *        Ponyo: "BAKA BAKA BAKA BAKA BAKA BAKA BAKA BAKA BAKA BAKA BAKA!!!"
 *
 *        There are too many 'ing header files here.  Most of them can be merged
 *        into VBoxTray.h (or a VBoxTrayInternal.h file).
 *
 */


DWORD VBoxDisplayGetCount();
DWORD VBoxDisplayGetConfig(const DWORD NumDevices, DWORD *pDevPrimaryNum, DWORD *pNumDevices, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes);

DWORD EnableAndResizeDispDev(DEVMODE *paDeviceModes, DISPLAY_DEVICE *paDisplayDevices, DWORD totalDispNum, UINT Id, DWORD aWidth, DWORD aHeight,
                             DWORD aBitsPerPixel, LONG aPosX, LONG aPosY, BOOL fEnabled, BOOL fExtDispSup);

#ifndef VBOX_WITH_WDDM
static bool isVBoxDisplayDriverActive(void);
#else
/* @misha: getVBoxDisplayDriverType is used instead.
 * it seems bad to put static function declaration to header,
 * so it is moved to VBoxDisplay.cpp */
#endif

#endif /* !GA_INCLUDED_SRC_WINNT_VBoxTray_VBoxDisplay_h */
