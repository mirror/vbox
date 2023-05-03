/* $Id$ */
/** @file
 * Guest / Host common code - Session type detection + handling.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_GuestHost_SessionType_h
#define VBOX_INCLUDED_GuestHost_SessionType_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** Environment variable which is exported when in Wayland Desktop Environment. */
#define VBGH_ENV_WAYLAND_DISPLAY        "WAYLAND_DISPLAY"
/** Environment variable which contains information about currently running Desktop Environment. */
#define VBGH_ENV_XDG_CURRENT_DESKTOP    "XDG_CURRENT_DESKTOP"
/** Environment variable which contains information about currently running session (X11, Wayland, etc). */
#define VBGH_ENV_XDG_SESSION_TYPE       "XDG_SESSION_TYPE"

/**
 * Enumeration holding a guest / host desktop session type.
 */
typedef enum
{
    /** No session detected or (explicitly) not set. */
    VBGHSESSIONTYPE_NONE = 0,
    /** Automatic detection -- might not work reliably on all systems. */
    VBGHSESSIONTYPE_AUTO,
    /** X11 (X.org). */
    VBGHSESSIONTYPE_X11,
    /** Wayland. */
    VBGHSESSIONTYPE_WAYLAND,
} VBGHSESSIONTYPE;

const char *VBGHSessionTypeToStr(VBGHSESSIONTYPE enmType);
VBGHSESSIONTYPE VBGHSessionTypeDetect(void);

#endif /* !VBOX_INCLUDED_GuestHost_SessionType_h */

