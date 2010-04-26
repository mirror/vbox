/** @file
 * Symbols from libvdeplug.so.2 to be loaded at runtime for DrvVDE.cpp
 */

/*
 * Copyright (C) 2008-2010 Sun Microsystems, Inc.
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

#include <stddef.h>
#include <sys/types.h>
/** Opaque connection type */
struct vdeconn;
typedef struct vdeconn VDECONN;

/** Structure to be passed to the open function describing the connection.
 * All members can be left zero to use the default values. */
struct vde_open_args
{
    /** The port of the switch to connect to */
    int port;
    /** The group to set ownership of the port socket to */
    char *group;
    /** The file mode to set the port socket to */
    mode_t mode;
};

/** The file name of the DBus library */
#define RT_RUNTIME_LOADER_LIB_NAME "libvdeplug.so.2"

/** The name of the loader function */
#define RT_RUNTIME_LOADER_FUNCTION DrvVDELoadVDEPlug

/** The following are the symbols which we need from the library. */
#define RT_RUNTIME_LOADER_INSERT_SYMBOLS \
 RT_PROXY_STUB(vde_open_real, VDECONN *, \
               (const char *vde_switch, const char *descr, int interface_version, struct vde_open_args *open_args), \
               (vde_switch, descr, interface_version, open_args)) \
 RT_PROXY_STUB(vde_recv, size_t, \
               (VDECONN *conn,void *buf,size_t len,int flags), \
               (conn, buf, len, flags)) \
 RT_PROXY_STUB(vde_send, size_t, \
               (VDECONN *conn,const void *buf,size_t len,int flags), \
               (conn, buf, len, flags)) \
 RT_PROXY_STUB(vde_datafd, int, (VDECONN *conn), (conn))

#ifdef VDEPLUG_GENERATE_HEADER
# define RT_RUNTIME_LOADER_GENERATE_HEADER
# define RT_RUNTIME_LOADER_GENERATE_DECLS
# include <iprt/runtime-loader.h>
# undef RT_RUNTIME_LOADER_GENERATE_HEADER
# undef RT_RUNTIME_LOADER_GENERATE_DECLS
#elif defined (VDEPLUG_GENERATE_BODY)
# define RT_RUNTIME_LOADER_GENERATE_BODY_STUBS
# include <iprt/runtime-loader.h>
# undef RT_RUNTIME_LOADER_GENERATE_BODY_STUBS
#else
# error This file should only be included to generate stubs for loading the \
libvdeplug library at runtime
#endif

#undef RT_RUNTIME_LOADER_LIB_NAME
#undef RT_RUNTIME_LOADER_INSERT_SYMBOLS
