/** @file
 *
 * Module to dynamically load libhal and libdbus and load all symbols
 * which are needed by VirtualBox.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "vbox-libhal.h"

#include <iprt/err.h>
#include <iprt/ldr.h>

/**
 * Whether we have tried to load libdbus and libhal yet.  This flag should only be set
 * to "true" after we have either loaded both libraries and all symbols which we need,
 * or failed to load something and unloaded and set back to zero pLibDBus and pLibHal.
 */
static bool gCheckedForLibHal = false;
/**
 * Pointer to the libdbus shared object.  This should only be set once all needed libraries
 * and symbols have been successfully loaded.
 */
static RTLDRMOD ghLibDBus = 0;
/**
 * Pointer to the libhal shared object.  This should only be set once all needed libraries
 * and symbols have been successfully loaded.
 */
static RTLDRMOD ghLibHal = 0;

/** The following are the symbols which we need from libdbus and libhal. */
void (*DBusErrorInit)(DBusError *);
DBusConnection *(*DBusBusGet)(DBusBusType, DBusError *);
void (*DBusErrorFree)(DBusError *);
void (*DBusConnectionUnref)(DBusConnection *);
LibHalContext *(*LibHalCtxNew)(void);
dbus_bool_t (*LibHalCtxSetDBusConnection)(LibHalContext *, DBusConnection *);
dbus_bool_t (*LibHalCtxInit)(LibHalContext *, DBusError *);
char **(*LibHalFindDeviceByCapability)(LibHalContext *, const char *, int *, DBusError *);
char *(*LibHalDeviceGetPropertyString)(LibHalContext *, const char *, const char *, DBusError *);
void (*LibHalFreeString)(char *);
void (*LibHalFreeStringArray)(char **);
dbus_bool_t (*LibHalCtxShutdown)(LibHalContext *, DBusError *);
dbus_bool_t (*LibHalCtxFree)(LibHalContext *);

bool LibHalCheckPresence(void)
{
    RTLDRMOD hLibDBus;
    RTLDRMOD hLibHal;

    if (ghLibDBus != 0 && ghLibHal != 0 && gCheckedForLibHal == true)
        return true;
    if (gCheckedForLibHal == true)
        return false;
    if (!RT_SUCCESS(RTLdrLoad(LIB_DBUS, &hLibDBus)))
        return false;
    if (!RT_SUCCESS(RTLdrLoad(LIB_HAL, &hLibHal)))
    {
        RTLdrClose(hLibDBus);
        return false;
    }
    if (   RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_error_init", (void **) &DBusErrorInit))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_bus_get", (void **) &DBusBusGet))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_error_free", (void **) &DBusErrorFree))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_unref",
                                     (void **) &DBusConnectionUnref))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_new", (void **) &LibHalCtxNew))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_set_dbus_connection",
                                     (void **) &LibHalCtxSetDBusConnection))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_init", (void **) &LibHalCtxInit))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_find_device_by_capability",
                                     (void **) &LibHalFindDeviceByCapability))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_device_get_property_string",
                                     (void **) &LibHalDeviceGetPropertyString))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_free_string", (void **) &LibHalFreeString))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_free_string_array",
                                     (void **) &LibHalFreeStringArray))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_shutdown", (void **) &LibHalCtxShutdown))
        && RT_SUCCESS(RTLdrGetSymbol(hLibHal, "libhal_ctx_free", (void **) &LibHalCtxFree))
       )
    {
        ghLibDBus = hLibDBus;
        ghLibHal = hLibHal;
        gCheckedForLibHal = true;
        return true;
    }
    else
    {
        RTLdrClose(hLibDBus);
        RTLdrClose(hLibHal);
        gCheckedForLibHal = true;
        return false;
    }
}
