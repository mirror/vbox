/* $Id$ */
/** @file
 * Guest Additions - Wayland Desktop Environment assistant.
 */

/*
 * Copyright (C) 2017-2023 Oracle and/or its affiliates.
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

#include "VBoxClient.h"
#include "wayland-helper.h"

/** List of available Wayland Desktop Environment helpers. Sorted in order of preference. */
static const VBCLWAYLANDHELPER *g_apWaylandHelpers[] =
{
    &g_WaylandHelperGtk,    /* GTK helper. */
    &g_WaylandHelperDcp,    /* Device Control Protocol helper. */
    NULL,                   /* Terminate list. */
};

/** Selected helpers for Clipboard and Drag-and-Drop. */
static const VBCLWAYLANDHELPER *g_pWaylandHelperHelperClipboard = NULL;
static const VBCLWAYLANDHELPER *g_pWaylandHelperHelperDnd       = NULL;

/**
 * @interface_method_impl{VBCLSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vbclWaylandInit(void)
{
    int rc;
    int idxHelper = 0;

    /** Custom log prefix to be used for logger instance of this process. */
    static const char *pszLogPrefix = "VBoxClient Wayland:";

    VBClLogSetLogPrefix(pszLogPrefix);

    /* Go through list of available helpers and try to pick up one. */
    while (g_apWaylandHelpers[idxHelper])
    {
        if (RT_VALID_PTR(g_apWaylandHelpers[idxHelper]->pfnProbe))
        {
            int fCaps = VBOX_WAYLAND_HELPER_CAP_NONE;

            VBClLogInfo("probing Wayland helper '%s'\n",
                         g_apWaylandHelpers[idxHelper]->pszName);

            fCaps = g_apWaylandHelpers[idxHelper]->pfnProbe();

            /* Try Clipboard helper. */
            if (   fCaps & VBOX_WAYLAND_HELPER_CAP_CLIPBOARD
                && !RT_VALID_PTR(g_pWaylandHelperHelperClipboard))
            {
                if (RT_VALID_PTR(g_apWaylandHelpers[idxHelper]->pfnInit))
                {
                    rc = g_apWaylandHelpers[idxHelper]->pfnInit();
                    if (RT_SUCCESS(rc))
                        g_pWaylandHelperHelperClipboard = g_apWaylandHelpers[idxHelper];
                    else
                        VBClLogError("Wayland helper '%s' cannot be initialized, skipping");
                }
            }

            /* Try DnD helper. */
            if (   fCaps & VBOX_WAYLAND_HELPER_CAP_DND
                && !RT_VALID_PTR(g_pWaylandHelperHelperDnd))
            {
                if (RT_VALID_PTR(g_apWaylandHelpers[idxHelper]->pfnInit))
                {
                    rc = g_apWaylandHelpers[idxHelper]->pfnInit();
                    if (RT_SUCCESS(rc))
                        g_pWaylandHelperHelperDnd = g_apWaylandHelpers[idxHelper];
                    else
                        VBClLogError("Wayland helper '%s' cannot be initialized, skipping");
                }
            }
        }

        /* See if we found all the needed helpers. */
        if (   RT_VALID_PTR(g_pWaylandHelperHelperClipboard)
            && RT_VALID_PTR(g_pWaylandHelperHelperDnd))
            break;

        idxHelper++;
    }

    /* Check result. */
    if (RT_VALID_PTR(g_pWaylandHelperHelperClipboard))
        VBClLogInfo("found Wayland Shared Clipboard helper '%s'\n", g_pWaylandHelperHelperClipboard->pszName);
    else
        VBClLogError("Wayland Shared Clipboard helper not found, clipboard sharing not possible\n");

    /* Check result. */
    if (RT_VALID_PTR(g_pWaylandHelperHelperDnd))
        VBClLogInfo("found Wayland Drag-and-Drop helper '%s'\n", g_pWaylandHelperHelperDnd->pszName);
    else
        VBClLogError("Wayland Drag-and-Drop helper not found, drag-and-drop not possible\n");

    return rc;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnWorker}
 */
static DECLCALLBACK(int) vbclWaylandWorker(bool volatile *pfShutdown)
{
    RT_NOREF(pfShutdown);
    return VERR_NOT_SUPPORTED;
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vbclWaylandStop(void)
{
}

/**
 * @interface_method_impl{VBCLSERVICE,pfnTerm}
 */
static DECLCALLBACK(int) vbclWaylandTerm(void)
{
    return VERR_NOT_SUPPORTED;
}

VBCLSERVICE g_SvcWayland =
{
    "wayland",              /* szName */
    "Wayland assistant",    /* pszDescription */
    ".vboxclient-wayland",  /* pszPidFilePathTemplate */
    NULL,                   /* pszUsage */
    NULL,                   /* pszOptions */
    NULL,                   /* pfnOption */
    vbclWaylandInit,        /* pfnInit */
    vbclWaylandWorker,      /* pfnWorker */
    vbclWaylandStop,        /* pfnStop */
    vbclWaylandTerm,        /* pfnTerm */
};
