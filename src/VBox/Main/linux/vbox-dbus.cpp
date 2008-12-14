/** @file
 *
 * Module to dynamically load libdbus and load all symbols
 * which are needed by VirtualBox.
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

#include "vbox-dbus.h"

#include <iprt/err.h>
#include <iprt/ldr.h>

/**
 * Whether we have tried to load libdbus-1 yet.  This flag should only be set
 * to "true" after we have either loaded the library and all symbols which we
 * need, or failed to load something and unloaded and set back to zero
 * pLibDBus.
 */
static bool gCheckedForLibDBus = false;
/**
 * Pointer to the libdbus-1 shared object.  This should only be set once all
 * needed libraries and symbols have been successfully loaded.
 */
static RTLDRMOD ghLibDBus = NULL;

/** The following are the symbols which we need from libdbus. */
void (*dbus_error_init)(DBusError *);
DBusConnection *(*dbus_bus_get)(DBusBusType, DBusError *);
DBusConnection *(*dbus_bus_get_private)(DBusBusType, DBusError *);
void (*dbus_error_free)(DBusError *);
void (*dbus_connection_unref)(DBusConnection *);
void (*dbus_connection_close)(DBusConnection *);
void (*dbus_connection_set_exit_on_disconnect)(DBusConnection *, dbus_bool_t);
dbus_bool_t (*dbus_bus_name_has_owner)(DBusConnection *, const char *,
                                       DBusError *);
void (*dbus_bus_add_match)(DBusConnection *, const char *, DBusError *);
void (*dbus_bus_remove_match)(DBusConnection *, const char *, DBusError *);
void (*dbus_message_unref)(DBusMessage *);
DBusMessage* (*dbus_message_new_method_call)(const char *, const char *,
                                             const char *, const char *);
void (*dbus_message_iter_init_append)(DBusMessage *, DBusMessageIter *);
dbus_bool_t (*dbus_message_iter_append_basic)(DBusMessageIter *, int,
                                              const void *);
DBusMessage * (*dbus_connection_send_with_reply_and_block)(DBusConnection *,
                                                           DBusMessage *, int,
                                                           DBusError *error);
dbus_bool_t (*dbus_message_iter_init) (DBusMessage *, DBusMessageIter *);
int (*dbus_message_iter_get_arg_type) (DBusMessageIter *);
int (*dbus_message_iter_get_element_type) (DBusMessageIter *);
void (*dbus_message_iter_recurse) (DBusMessageIter *, DBusMessageIter *);
void (*dbus_message_iter_get_basic) (DBusMessageIter *, void *);
dbus_bool_t (*dbus_message_iter_next) (DBusMessageIter *);
dbus_bool_t (*dbus_connection_add_filter) (DBusConnection *, DBusHandleMessageFunction,
                                           void *, DBusFreeFunction);
void (*dbus_connection_remove_filter) (DBusConnection *, DBusHandleMessageFunction,
                                       void *);
dbus_bool_t (*dbus_connection_read_write_dispatch) (DBusConnection *, int);
dbus_bool_t (*dbus_message_is_signal) (DBusMessage *, const char *, const char *);
DBusMessage *(*dbus_connection_pop_message)(DBusConnection *);

bool VBoxDBusCheckPresence(void)
{
    RTLDRMOD hLibDBus;

    if (ghLibDBus != 0 && gCheckedForLibDBus == true)
        return true;
    if (gCheckedForLibDBus == true)
        return false;
    if (   !RT_SUCCESS(RTLdrLoad(LIB_DBUS_1_3, &hLibDBus))
        && !RT_SUCCESS(RTLdrLoad(LIB_DBUS_1_2, &hLibDBus)))
    {
        return false;
    }
    if (   RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_error_init",
                                     (void **) &dbus_error_init))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_bus_get",
                                     (void **) &dbus_bus_get))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_bus_get_private",
                                     (void **) &dbus_bus_get_private))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_error_free",
                                     (void **) &dbus_error_free))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_unref",
                                     (void **) &dbus_connection_unref))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_close",
                                     (void **) &dbus_connection_close))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_set_exit_on_disconnect",
                                     (void **) &dbus_connection_set_exit_on_disconnect))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_bus_name_has_owner",
                                     (void **) &dbus_bus_name_has_owner))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_bus_add_match",
                                     (void **) &dbus_bus_add_match))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_bus_remove_match",
                                     (void **) &dbus_bus_remove_match))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_unref",
                                     (void **) &dbus_message_unref))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_new_method_call",
                                     (void **) &dbus_message_new_method_call))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_init_append",
                                     (void **) &dbus_message_iter_init_append))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_append_basic",
                                     (void **) &dbus_message_iter_append_basic))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_send_with_reply_and_block",
                                     (void **) &dbus_connection_send_with_reply_and_block))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_init",
                                     (void **) &dbus_message_iter_init))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_get_arg_type",
                                     (void **) &dbus_message_iter_get_arg_type))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_get_element_type",
                                     (void **) &dbus_message_iter_get_element_type))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_recurse",
                                     (void **) &dbus_message_iter_recurse))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_get_basic",
                                     (void **) &dbus_message_iter_get_basic))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_iter_next",
                                     (void **) &dbus_message_iter_next))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_add_filter",
                                     (void **) &dbus_connection_add_filter))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_remove_filter",
                                     (void **) &dbus_connection_remove_filter))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_read_write_dispatch",
                                     (void **) &dbus_connection_read_write_dispatch))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_message_is_signal",
                                     (void **) &dbus_message_is_signal))
        && RT_SUCCESS(RTLdrGetSymbol(hLibDBus, "dbus_connection_pop_message",
                                     (void **) &dbus_connection_pop_message))
       )
    {
        ghLibDBus = hLibDBus;
        gCheckedForLibDBus = true;
        return true;
    }
    else
    {
        RTLdrClose(hLibDBus);
        gCheckedForLibDBus = true;
        return false;
    }
}

void VBoxDBusConnectionUnref(DBusConnection *pConnection)
{
    dbus_connection_unref(pConnection);
}

void VBoxDBusMessageUnref(DBusMessage *pMessage)
{
    dbus_message_unref(pMessage);
}

