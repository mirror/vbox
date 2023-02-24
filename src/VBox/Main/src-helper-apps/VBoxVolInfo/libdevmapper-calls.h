/** @file
 * Stubs for dynamically loading libdevmapper and the symbols which are needed by
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
 * SPDX-License-Identifier: GPL-3.0-only
 */


/** The file name of the libdevmapper library */
#define RT_RUNTIME_LOADER_LIB_NAME  "libdevmapper.so"

/** The name of the loader function */
#define RT_RUNTIME_LOADER_FUNCTION  RTDevmapperLoadLib

/** The following are the symbols which we need from the libdevmapper library. */
#define RT_RUNTIME_LOADER_INSERT_SYMBOLS \
 RT_PROXY_STUB(dm_task_create, struct dm_task *, (int type), \
                 (type)) \
 RT_PROXY_STUB(dm_task_set_name, int, (struct dm_task *dmt, const char *name), \
                 (dmt, name)) \
 RT_PROXY_STUB(dm_task_run, int, \
                 (struct dm_task *dmt), (dmt)) \
 RT_PROXY_STUB(dm_task_get_info, int, \
                 (struct dm_task *dmt, struct dm_info *dmi), (dmt, dmi)) \
 RT_PROXY_STUB(dm_task_get_deps, struct dm_deps *, (struct dm_task *dmt), \
                 (dmt)) \
 RT_PROXY_STUB(dm_task_destroy, void, (struct dm_task *dmt), \
                 (dmt))

#ifdef VBOX_LIBDEVMAPPER_GENERATE_HEADER
# define RT_RUNTIME_LOADER_GENERATE_HEADER
# define RT_RUNTIME_LOADER_GENERATE_DECLS
# include <iprt/runtime-loader.h>
# undef RT_RUNTIME_LOADER_GENERATE_HEADER
# undef RT_RUNTIME_LOADER_GENERATE_DECLS

#elif defined(VBOX_LIBDEVMAPPER_GENERATE_BODY)
# define RT_RUNTIME_LOADER_GENERATE_BODY_STUBS
# include <iprt/runtime-loader.h>
# undef RT_RUNTIME_LOADER_GENERATE_BODY_STUBS

#else
# error This file should only be included to generate stubs for loading the libdevmapper library at runtime
#endif

#undef RT_RUNTIME_LOADER_LIB_NAME
#undef RT_RUNTIME_LOADER_INSERT_SYMBOLS

