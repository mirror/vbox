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

#ifndef ____H_VBOX_DBUS
#define ____H_VBOX_DBUS

#include <stdint.h>

#define LIB_DBUS_1_2 "libdbus-1.so.2"  /* This should be compatible */
#define LIB_DBUS_1_3 "libdbus-1.so.3"

/** Types and defines from the dbus header files which we need.  These are
 * taken more or less verbatim from the DBus public interface header files. */
struct DBusError
{
    const char *name;
    const char *message;
    unsigned int dummy1 : 1;
    unsigned int dummy2 : 1;
    unsigned int dummy3 : 1;
    unsigned int dummy4 : 1;
    unsigned int dummy5 : 1;
    void *padding1;
};
struct DBusConnection;
typedef struct DBusConnection DBusConnection;
typedef uint32_t dbus_bool_t;
typedef uint32_t dbus_uint32_t;
typedef enum { DBUS_BUS_SESSON, DBUS_BUS_SYSTEM, DBUS_BUS_STARTER } DBusBusType;
struct DBusMessage;
typedef struct DBusMessage DBusMessage;
struct DBusMessageIter
{ 
    void *dummy1;
    void *dummy2;
    dbus_uint32_t dummy3;
    int dummy4;
    int dummy5;
    int dummy6;
    int dummy7;
    int dummy8;
    int dummy9;
    int dummy10;
    int dummy11;
    int pad1;
    int pad2;
    void *pad3;
};
typedef struct DBusMessageIter DBusMessageIter;

#define DBUS_ERROR_NO_MEMORY                  "org.freedesktop.DBus.Error.NoMemory"
#define DBUS_TYPE_STRING        ((int) 's')
#define DBUS_TYPE_ARRAY         ((int) 'a')
#define DBUS_TYPE_DICT_ENTRY    ((int) 'e')

typedef enum
{
  DBUS_HANDLER_RESULT_HANDLED,
  DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
  DBUS_HANDLER_RESULT_NEED_MEMORY
} DBusHandlerResult;

typedef DBusHandlerResult (*DBusHandleMessageFunction) (DBusConnection *,
                                                        DBusMessage *, void *);
typedef void (*DBusFreeFunction) (void *);

/** The following are the symbols which we need from libdbus-1. */
extern void (*vbox_dbus_error_init)(DBusError *);
inline void dbus_error_init(DBusError *error)
{ vbox_dbus_error_init(error); }
extern DBusConnection *(*vbox_dbus_bus_get)(DBusBusType, DBusError *);
inline DBusConnection *dbus_bus_get(DBusBusType type, DBusError *error)
{ return vbox_dbus_bus_get(type, error); }
extern DBusConnection *(*vbox_dbus_bus_get_private)(DBusBusType, DBusError *);
inline DBusConnection *dbus_bus_get_private(DBusBusType type, DBusError *error)
{ return vbox_dbus_bus_get_private(type, error); }
extern void (*vbox_dbus_error_free)(DBusError *);
inline void dbus_error_free(DBusError *error)
{ vbox_dbus_error_free(error); }
extern void (*vbox_dbus_connection_unref)(DBusConnection *);
inline void dbus_connection_unref(DBusConnection *connection)
{ vbox_dbus_connection_unref(connection); }
extern void (*vbox_dbus_connection_close)(DBusConnection *);
inline void dbus_connection_close(DBusConnection *connection)
{ vbox_dbus_connection_close(connection); }
extern void (*vbox_dbus_connection_set_exit_on_disconnect)(DBusConnection *, dbus_bool_t);
inline void dbus_connection_set_exit_on_disconnect(DBusConnection *connection, dbus_bool_t boolean)
{ vbox_dbus_connection_set_exit_on_disconnect(connection, boolean); }
extern dbus_bool_t (*vbox_dbus_bus_name_has_owner)(DBusConnection *, const char *,
                                                   DBusError *);
inline dbus_bool_t dbus_bus_name_has_owner(DBusConnection *connection, const char *string,
                                                   DBusError *error)
{ return vbox_dbus_bus_name_has_owner(connection, string, error); }
extern void (*vbox_dbus_bus_add_match)(DBusConnection *, const char *, DBusError *);
inline void dbus_bus_add_match(DBusConnection *connection, const char *string, DBusError *error)
{ vbox_dbus_bus_add_match(connection, string, error); }
extern void (*vbox_dbus_bus_remove_match)(DBusConnection *, const char *, DBusError *);
inline void dbus_bus_remove_match(DBusConnection *connection, const char *string, DBusError *error)
{ vbox_dbus_bus_remove_match(connection, string, error); }
extern void (*vbox_dbus_message_unref)(DBusMessage *);
inline void dbus_message_unref(DBusMessage *message)
{ vbox_dbus_message_unref(message); }
extern DBusMessage* (*vbox_dbus_message_new_method_call)(const char *, const char *,
                                                         const char *, const char *);
inline DBusMessage* dbus_message_new_method_call(const char *string1, const char *string2,
                                                         const char *string3, const char *string4)
{ return vbox_dbus_message_new_method_call(string1, string2, string3, string4); }
extern void (*vbox_dbus_message_iter_init_append)(DBusMessage *, DBusMessageIter *);
inline void dbus_message_iter_init_append(DBusMessage *message, DBusMessageIter *iter)
{ vbox_dbus_message_iter_init_append(message, iter); }
extern dbus_bool_t (*vbox_dbus_message_iter_append_basic)(DBusMessageIter *, int,
                                                          const void *);
inline dbus_bool_t dbus_message_iter_append_basic(DBusMessageIter *iter, int val,
                                                          const void *string)
{ return vbox_dbus_message_iter_append_basic(iter, val, string); }
extern DBusMessage * (*vbox_dbus_connection_send_with_reply_and_block)(DBusConnection *,
                                                                       DBusMessage *, int,
                                                                       DBusError *error);
inline DBusMessage * dbus_connection_send_with_reply_and_block(DBusConnection *connection,
                                                               DBusMessage *message, int val,
                                                               DBusError *error)
{ return vbox_dbus_connection_send_with_reply_and_block(connection, message, val, error); }
extern dbus_bool_t (*vbox_dbus_message_iter_init) (DBusMessage *, DBusMessageIter *);
inline dbus_bool_t dbus_message_iter_init(DBusMessage *message, DBusMessageIter *iter)
{ return vbox_dbus_message_iter_init(message, iter); }
extern int (*vbox_dbus_message_iter_get_arg_type) (DBusMessageIter *);
inline int dbus_message_iter_get_arg_type(DBusMessageIter *iter)
{ return vbox_dbus_message_iter_get_arg_type(iter); }
extern int (*vbox_dbus_message_iter_get_element_type) (DBusMessageIter *);
inline int dbus_message_iter_get_element_type(DBusMessageIter *iter)
{ return vbox_dbus_message_iter_get_element_type(iter); }
extern void (*vbox_dbus_message_iter_recurse) (DBusMessageIter *, DBusMessageIter *);
inline void dbus_message_iter_recurse(DBusMessageIter *iter1, DBusMessageIter *iter2)
{ vbox_dbus_message_iter_recurse(iter1, iter2); }
extern void (*vbox_dbus_message_iter_get_basic) (DBusMessageIter *, void *);
inline void dbus_message_iter_get_basic(DBusMessageIter *iter, void *pvoid)
{ vbox_dbus_message_iter_get_basic(iter, pvoid); }
extern dbus_bool_t (*vbox_dbus_message_iter_next) (DBusMessageIter *);
inline dbus_bool_t dbus_message_iter_next(DBusMessageIter *iter)
{ return vbox_dbus_message_iter_next(iter); }
extern dbus_bool_t (*vbox_dbus_connection_add_filter) (DBusConnection *, DBusHandleMessageFunction,
                                                       void *, DBusFreeFunction);
inline dbus_bool_t dbus_connection_add_filter(DBusConnection *connection,
                                              DBusHandleMessageFunction function1,
                                              void *pvoid, DBusFreeFunction function2)
{ return vbox_dbus_connection_add_filter(connection, function1, pvoid, function2); }
extern void (*vbox_dbus_connection_remove_filter) (DBusConnection *, DBusHandleMessageFunction,
                                                   void *);
inline void dbus_connection_remove_filter(DBusConnection *connection,
                                          DBusHandleMessageFunction function, void *pvoid)
{ vbox_dbus_connection_remove_filter(connection, function, pvoid); }
extern dbus_bool_t (*vbox_dbus_connection_read_write_dispatch) (DBusConnection *, int);
inline dbus_bool_t dbus_connection_read_write_dispatch(DBusConnection *connection, int val)
{ return vbox_dbus_connection_read_write_dispatch(connection, val); }
extern dbus_bool_t (*vbox_dbus_message_is_signal) (DBusMessage *, const char *, const char *);
inline dbus_bool_t dbus_message_is_signal(DBusMessage *message, const char *string1, const char *string2)
{ return vbox_dbus_message_is_signal(message, string1, string2); }
extern DBusMessage *(*vbox_dbus_connection_pop_message)(DBusConnection *);
inline DBusMessage *dbus_connection_pop_message(DBusConnection *connection)
{ return vbox_dbus_connection_pop_message(connection); }

/**
 * Checks whether libdbus is present and if so loads all symbols we need.  This
 * should be called before making any calls to libdbus if that library is being
 * loaded at runtime.
 * @returns true if libdbus was found, false otherwise.
 */
extern bool VBoxDBusCheckPresence(void);
/** Wrapper around dbus_connection_unref.  We need this to use it as a free
 * function in auto pointers, as a function pointer won't wash here. */
extern void VBoxDBusConnectionUnref(DBusConnection *pConnection);
/** Wrapper around dbus_message_unref.  We need this to use it as a free
 * function in auto pointers, as a function pointer won't wash here. */
extern void VBoxDBusMessageUnref(DBusMessage *pMessage);

#endif /* ____H_VBOX_DBUS not defined */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

