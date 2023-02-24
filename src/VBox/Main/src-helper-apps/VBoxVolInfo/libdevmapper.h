/** @file
 * Module to dynamically load libdevmapper and load all symbols which are needed by
 * VirtualBox.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_libdevmapper_h
#define VBOX_INCLUDED_libdevmapper_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/stdarg.h>

#ifndef __cplusplus
# error "This header requires C++ to avoid name clashes."
#endif

/*
 * Types and defines from the libdevmapper header files which we need.  These are
 * taken more or less verbatim from libdevmapper.h.
 */
enum
{
    DM_DEVICE_CREATE,
    DM_DEVICE_RELOAD,
    DM_DEVICE_REMOVE,
    DM_DEVICE_REMOVE_ALL,
    DM_DEVICE_SUSPEND,
    DM_DEVICE_RESUME,
    DM_DEVICE_INFO,
    DM_DEVICE_DEPS,
    DM_DEVICE_RENAME,
    DM_DEVICE_VERSION,
    DM_DEVICE_STATUS,
    DM_DEVICE_TABLE,
    DM_DEVICE_WAITEVENT,
    DM_DEVICE_LIST,
    DM_DEVICE_CLEAR,
    DM_DEVICE_MKNODES,
    DM_DEVICE_LIST_VERSIONS,
    DM_DEVICE_TARGET_MSG,
    DM_DEVICE_SET_GEOMETRY
};

struct dm_task;

struct dm_info
{
    int exists;
    int suspended;
    int live_table;
    int inactive_table;
    int32_t open_count;
    uint32_t event_nr;
    uint32_t major;
    uint32_t minor;
    int read_only;
    int32_t target_count;
    int deferred_remove;
    int internal_suspend;
};

struct dm_deps
{
    uint32_t count;
    uint32_t filler;
    RT_FLEXIBLE_ARRAY_EXTENSION
    uint64_t device[RT_FLEXIBLE_ARRAY];
};

/* Declarations of the functions that we need from libdevmapper */
#define VBOX_LIBDEVMAPPER_GENERATE_HEADER

#include "libdevmapper-calls.h"

#undef VBOX_LIBDEVMAPPER_GENERATE_HEADER

#endif /* !VBOX_INCLUDED_libdevmapper_h */

