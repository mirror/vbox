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
    unsigned int dummy1 : 1; /**< placeholder */
    unsigned int dummy2 : 1; /**< placeholder */
    unsigned int dummy3 : 1; /**< placeholder */
    unsigned int dummy4 : 1; /**< placeholder */
    unsigned int dummy5 : 1; /**< placeholder */
    void *padding1; /**< placeholder */
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
    void *dummy1;         /**< Don't use this */
    void *dummy2;         /**< Don't use this */
    dbus_uint32_t dummy3; /**< Don't use this */
    int dummy4;           /**< Don't use this */
    int dummy5;           /**< Don't use this */
    int dummy6;           /**< Don't use this */
    int dummy7;           /**< Don't use this */
    int dummy8;           /**< Don't use this */
    int dummy9;           /**< Don't use this */
    int dummy10;          /**< Don't use this */
    int dummy11;          /**< Don't use this */
    int pad1;             /**< Don't use this */
    int pad2;             /**< Don't use this */
    void *pad3;           /**< Don't use this */
};
typedef struct DBusMessageIter DBusMessageIter;

#define DBUS_ERROR_NO_MEMORY                  "org.freedesktop.DBus.Error.NoMemory"
#define DBUS_TYPE_STRING        ((int) 's')
#define DBUS_TYPE_ARRAY         ((int) 'a')
#define DBUS_TYPE_DICT_ENTRY    ((int) 'e')

typedef enum
{
  DBUS_HANDLER_RESULT_HANDLED,         /**< Message has had its effect - no need
 to run more handlers. */ 
  DBUS_HANDLER_RESULT_NOT_YET_HANDLED, /**< Message has not had any effect - see
 if other handlers want it. */
  DBUS_HANDLER_RESULT_NEED_MEMORY      /**< Need more memory in order to return 
#DBUS_HANDLER_RESULT_HANDLED or #DBUS_HANDLER_RESULT_NOT_YET_HANDLED. Please try
 again later with more memory. */
} DBusHandlerResult;

typedef DBusHandlerResult (*DBusHandleMessageFunction) (DBusConnection *,
                                                        DBusMessage *, void *);
typedef void (*DBusFreeFunction) (void *);

/** The following are the symbols which we need from libdbus-1. */
extern void (*dbus_error_init)(DBusError *);
extern DBusConnection *(*dbus_bus_get)(DBusBusType, DBusError *);
extern DBusConnection *(*dbus_bus_get_private)(DBusBusType, DBusError *);
extern void (*dbus_error_free)(DBusError *);
extern void (*dbus_connection_unref)(DBusConnection *);
extern void (*dbus_connection_close)(DBusConnection *);
extern void (*dbus_connection_set_exit_on_disconnect)(DBusConnection *, dbus_bool_t);
extern dbus_bool_t (*dbus_bus_name_has_owner)(DBusConnection *, const char *,
                                       DBusError *);
extern void (*dbus_bus_add_match)(DBusConnection *, const char *, DBusError *);
extern void (*dbus_bus_remove_match)(DBusConnection *, const char *, DBusError *);
extern void (*dbus_message_unref)(DBusMessage *);
extern DBusMessage* (*dbus_message_new_method_call)(const char *, const char *,
                                             const char *, const char *);
extern void (*dbus_message_iter_init_append)(DBusMessage *, DBusMessageIter *);
extern dbus_bool_t (*dbus_message_iter_append_basic)(DBusMessageIter *, int,
                                              const void *);
extern DBusMessage * (*dbus_connection_send_with_reply_and_block)(DBusConnection *,
                                                           DBusMessage *, int,
                                                           DBusError *error);
extern dbus_bool_t (*dbus_message_iter_init) (DBusMessage *, DBusMessageIter *);
extern int (*dbus_message_iter_get_arg_type) (DBusMessageIter *);
extern int (*dbus_message_iter_get_element_type) (DBusMessageIter *);
extern void (*dbus_message_iter_recurse) (DBusMessageIter *, DBusMessageIter *);
extern void (*dbus_message_iter_get_basic) (DBusMessageIter *, void *);
extern dbus_bool_t (*dbus_message_iter_next) (DBusMessageIter *);
extern dbus_bool_t (*dbus_connection_add_filter) (DBusConnection *, DBusHandleMessageFunction,
                                                  void *, DBusFreeFunction);
extern void (*dbus_connection_remove_filter) (DBusConnection *, DBusHandleMessageFunction,
                                              void *);
extern dbus_bool_t (*dbus_connection_read_write_dispatch) (DBusConnection *, int);
extern dbus_bool_t (*dbus_message_is_signal) (DBusMessage *, const char *, const char *);
extern DBusMessage *(*dbus_connection_pop_message)(DBusConnection *);

extern bool VBoxDBusCheckPresence(void);
extern void VBoxDBusConnectionUnref(DBusConnection *pConnection);
extern void VBoxDBusMessageUnref(DBusMessage *pMessage);

#endif /* ____H_VBOX_DBUS not defined */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
