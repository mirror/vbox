/* $Id$ */
/** @file
 * Guest Additions - Definitions for Wayland Desktop Environments helpers.
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

#ifndef GA_INCLUDED_SRC_x11_VBoxClient_wayland_helper_h
#define GA_INCLUDED_SRC_x11_VBoxClient_wayland_helper_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/err.h>


/** Helper capabilities list. */
typedef enum
{
    /** Indicates that helper does not support any functionality (initializer). */
    VBOX_WAYLAND_HELPER_CAP_NONE = 0,
    /** Indicates that helper supported shared clipboard functionality. */
    VBOX_WAYLAND_HELPER_CAP_CLIPBOARD,
    /** Indicates that helper supported drag-and-drop functionality. */
    VBOX_WAYLAND_HELPER_CAP_DND
} vbox_wayland_helper_cap_t;

/**
 * Wayland Desktop Environment helper definition structure.
 */
typedef struct
{
    /** A short helper name. 16 chars maximum (RTTHREAD_NAME_LEN). */
    const char *pszName;

    /**
     * Probing callback.
     *
     * Called in attempt to detect if user is currently running Desktop Environment
     * which is compatible with the helper.
     *
     * @returns Helpercapabilities bitmask as described by vbox_wayland_helper_cap_t.
     */
    DECLCALLBACKMEMBER(int, pfnProbe, (void));

    /**
     * Initialization callback.
     *
     * @returns IPRT status code.
     */
    DECLCALLBACKMEMBER(int, pfnInit, (void));

    /**
     * Termination callback.
     *
     * @returns IPRT status code.
     */
    DECLCALLBACKMEMBER(int, pfnTerm, (void));

} VBCLWAYLANDHELPER;

/** Wayland helper which uses GTK library. */
extern const VBCLWAYLANDHELPER g_WaylandHelperGtk;
/** Wayland helper which uses Data Control Protocol. */
extern const VBCLWAYLANDHELPER g_WaylandHelperDcp;

#endif /* !GA_INCLUDED_SRC_x11_VBoxClient_wayland_helper_h */
