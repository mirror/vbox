/* $Id$ */
/** @file
 * VBox Qt GUI - Global definitions.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIDefs_h
#define FEQT_INCLUDED_SRC_globals_UIDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Define GUI log group: */
// WORKAROUND:
// This define should go *before* VBox/log.h include!
#ifndef LOG_GROUP
# define LOG_GROUP LOG_GROUP_GUI
#endif

/* Qt includes: */
#include <QEvent>
#include <QStringList>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Other VBox includes: */
#include <VBox/log.h>
#include <VBox/com/defs.h>

/* Defines: */
#ifdef RT_STRICT
# define AssertWrapperOk(w)         AssertMsg(w.isOk(), (#w " is not okay (RC=0x%08X)", w.lastRC()))
# define AssertWrapperOkMsg(w, m)   AssertMsg(w.isOk(), (#w ": " m " (RC=0x%08X)", w.lastRC()))
#else
# define AssertWrapperOk(w)         do {} while (0)
# define AssertWrapperOkMsg(w, m)   do {} while (0)
#endif


/** Global namespace. */
namespace UIDefs
{
    /** Additional Qt event types. */
    enum UIEventType
    {
        ActivateActionEventType = QEvent::User + 101,
#ifdef VBOX_WS_MAC
        ShowWindowEventType,
#endif
    };

    /** Default guest additions image name. */
    SHARED_LIBRARY_STUFF extern const char* GUI_GuestAdditionsName;
    /** Default extension pack name. */
    SHARED_LIBRARY_STUFF extern const char* GUI_ExtPackName;

    /** Allowed VBox file extensions. */
    SHARED_LIBRARY_STUFF extern QStringList VBoxFileExts;
    /** Allowed VBox Extension Pack file extensions. */
    SHARED_LIBRARY_STUFF extern QStringList VBoxExtPackFileExts;
    /** Allowed OVF file extensions. */
    SHARED_LIBRARY_STUFF extern QStringList OVFFileExts;
}
using namespace UIDefs /* if header included */;


#ifdef VBOX_WS_MAC
/** Known macOS releases. */
enum MacOSXRelease
{
    MacOSXRelease_Old,
    MacOSXRelease_FirstUnknown = 9,
    MacOSXRelease_SnowLeopard  = 10,
    MacOSXRelease_Lion         = 11,
    MacOSXRelease_MountainLion = 12,
    MacOSXRelease_Mavericks    = 13,
    MacOSXRelease_Yosemite     = 14,
    MacOSXRelease_ElCapitan    = 15,
    MacOSXRelease_Sierra       = 16,
    MacOSXRelease_HighSierra   = 17,
    MacOSXRelease_Mojave       = 18,
    MacOSXRelease_Catalina     = 19,
    MacOSXRelease_BigSur       = 20,
    MacOSXRelease_Monterey     = 21,
    MacOSXRelease_Ventura      = 22,
    MacOSXRelease_Sonoma       = 23,
    MacOSXRelease_LastUnknown  = 24,
    MacOSXRelease_New,
};
#endif /* VBOX_WS_MAC */


/** VM launch modes. */
enum UILaunchMode
{
    UILaunchMode_Invalid,
    UILaunchMode_Default,
    UILaunchMode_Headless,
    UILaunchMode_Separate
};


#endif /* !FEQT_INCLUDED_SRC_globals_UIDefs_h */
