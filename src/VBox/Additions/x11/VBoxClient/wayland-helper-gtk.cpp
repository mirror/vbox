/* $Id$ */
/** @file
 * Guest Additions - Wayland Desktop Environment helper which uses GTK library.
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

#include "wayland-helper.h"

/**
 * @interface_method_impl{VBCLWAYLANDHELPER,pfnProbe}
 */
static DECLCALLBACK(int) vbcl_wayland_hlp_gtk_probe(void)
{
    return VBOX_WAYLAND_HELPER_CAP_NONE;
}

/**
 * @interface_method_impl{VBCLWAYLANDHELPER,pfnInit}
 */
RTDECL(int) vbcl_wayland_hlp_gtk_init(void)
{
    return VERR_NOT_SUPPORTED;
}

/**
 * @interface_method_impl{VBCLWAYLANDHELPER,pfnTerm}
 */
RTDECL(int) vbcl_wayland_hlp_gtk_term(void)
{
    return VERR_NOT_SUPPORTED;
}

/* Helper callbacks. */
const VBCLWAYLANDHELPER g_WaylandHelperGtk =
{
    "wayland-gtk",                  /* .pszName */
    vbcl_wayland_hlp_gtk_probe,     /* .pfnProbe */
    vbcl_wayland_hlp_gtk_init,      /* .pfnInit */
    vbcl_wayland_hlp_gtk_term,      /* .pfnTerm */
};
