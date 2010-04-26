/** @file
 *
 * Module to dynamically load libvdeplug and load all symbols
 * which are needed by VirtualBox - header file.
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

#ifndef ___VBox_VDEPlug_h
#define ___VBox_VDEPlug_h

#define LIBVDEPLUG_INTERFACE_VERSION 1

#define vde_open(vde_switch, descr, open_args) \
    vde_open_real((vde_switch), (descr), LIBVDEPLUG_INTERFACE_VERSION, (open_args))

/* Declarations of the functions that we need from the library */
#define VDEPLUG_GENERATE_HEADER

#include "VDEPlugSymDefs.h"

#undef VDEPLUG_GENERATE_HEADER

#endif /* ___VBox_VDEPlug_h not defined */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
