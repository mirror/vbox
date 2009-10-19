/** @file
 *
 * Stubs for dynamically loading libdbus-1 and the symbols
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

#ifndef VBOX_PROXY_STUB
#error This file is a private header, intended to be included in specific places only
#endif

/** The following are the symbols which we need from libdbus-1. */
VBOX_PROXY_STUB(dbus_error_init, void, (DBusError *error),
                (error))
VBOX_PROXY_STUB(dbus_bus_get, DBusConnection *,
                (DBusBusType type, DBusError *error), (type, error))
VBOX_PROXY_STUB(dbus_bus_get_private, DBusConnection *,
                (DBusBusType type, DBusError *error), (type, error))
VBOX_PROXY_STUB(dbus_error_free, void, (DBusError *error),
                (error))
VBOX_PROXY_STUB(dbus_connection_unref, void, (DBusConnection *connection),
                (connection))
VBOX_PROXY_STUB(dbus_connection_close, void, (DBusConnection *connection),
                (connection))
VBOX_PROXY_STUB(dbus_connection_send, dbus_bool_t,
                (DBusConnection *connection, DBusMessage *message, dbus_uint32_t *serial),
                (connection, message, serial))
VBOX_PROXY_STUB(dbus_connection_flush, void, (DBusConnection *connection),
                (connection))
VBOX_PROXY_STUB(dbus_connection_set_exit_on_disconnect, void,
                (DBusConnection *connection, dbus_bool_t boolean),
                (connection, boolean))
VBOX_PROXY_STUB(dbus_bus_name_has_owner, dbus_bool_t,
                (DBusConnection *connection, const char *string, DBusError *error),
                (connection, string, error))
VBOX_PROXY_STUB(dbus_bus_add_match, void,
                (DBusConnection *connection, const char *string,
                 DBusError *error),
                (connection, string, error))
VBOX_PROXY_STUB(dbus_bus_remove_match, void,
                (DBusConnection *connection, const char *string,
                 DBusError *error),
                (connection, string, error))
VBOX_PROXY_STUB(dbus_message_append_args_valist, dbus_bool_t,
                (DBusMessage *message, int first_arg_type, va_list var_args),
                (message, first_arg_type, var_args))
VBOX_PROXY_STUB(dbus_message_iter_open_container, dbus_bool_t,
                (DBusMessageIter *iter, int type, const char *contained_signature, DBusMessageIter *sub),
                (iter, type, contained_signature, sub))
VBOX_PROXY_STUB(dbus_message_iter_close_container, dbus_bool_t,
                (DBusMessageIter *iter, DBusMessageIter *sub),
                (iter, sub))
VBOX_PROXY_STUB(dbus_message_iter_append_fixed_array, dbus_bool_t,
                (DBusMessageIter *iter, int element_type, const void *value, int n_elements),
                (iter, element_type, value, n_elements))
VBOX_PROXY_STUB(dbus_message_unref, void, (DBusMessage *message),
                (message))
VBOX_PROXY_STUB(dbus_message_new_method_call, DBusMessage*,
                (const char *string1, const char *string2, const char *string3,
                 const char *string4),
                (string1, string2, string3, string4))
VBOX_PROXY_STUB(dbus_message_iter_init_append, void,
                (DBusMessage *message, DBusMessageIter *iter),
                (message, iter))
VBOX_PROXY_STUB(dbus_message_iter_append_basic, dbus_bool_t,
                (DBusMessageIter *iter, int val, const void *string),
                (iter, val, string))
VBOX_PROXY_STUB(dbus_connection_send_with_reply_and_block, DBusMessage *,
                (DBusConnection *connection, DBusMessage *message, int val,
                 DBusError *error),
                (connection, message, val, error))
VBOX_PROXY_STUB(dbus_message_iter_init, dbus_bool_t,
                (DBusMessage *message, DBusMessageIter *iter),
                (message, iter))
VBOX_PROXY_STUB(dbus_message_iter_get_arg_type, int, (DBusMessageIter *iter),
                (iter))
VBOX_PROXY_STUB(dbus_message_iter_get_element_type, int,
                (DBusMessageIter *iter), (iter))
VBOX_PROXY_STUB(dbus_message_iter_recurse, void,
                (DBusMessageIter *iter1, DBusMessageIter *iter2),
                (iter1, iter2))
VBOX_PROXY_STUB(dbus_message_iter_get_basic, void,
                (DBusMessageIter *iter, void *pvoid), (iter, pvoid))
VBOX_PROXY_STUB(dbus_message_iter_next, dbus_bool_t, (DBusMessageIter *iter),
                (iter))
VBOX_PROXY_STUB(dbus_connection_add_filter, dbus_bool_t,
                (DBusConnection *connection,
                 DBusHandleMessageFunction function1, void *pvoid,
                 DBusFreeFunction function2),
                (connection, function1, pvoid, function2))
VBOX_PROXY_STUB(dbus_connection_remove_filter, void,
                (DBusConnection *connection,
                 DBusHandleMessageFunction function, void *pvoid),
                (connection, function, pvoid))
VBOX_PROXY_STUB(dbus_connection_read_write_dispatch, dbus_bool_t,
                (DBusConnection *connection, int val), (connection, val))
VBOX_PROXY_STUB(dbus_message_is_signal, dbus_bool_t,
                (DBusMessage *message, const char *string1,
                 const char *string2),
                (message, string1, string2))
VBOX_PROXY_STUB(dbus_connection_pop_message, DBusMessage *,
                (DBusConnection *connection), (connection))

