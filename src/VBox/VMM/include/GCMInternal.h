/** @file
 * GCM - Internal header file.
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_include_GCMInternal_h
#define VMM_INCLUDED_SRC_include_GCMInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/gcm.h>
#include <VBox/vmm/pgm.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_gcm_int       Internal
 * @ingroup grp_gcm
 * @internal
 * @{
 */

/** The saved state version. */
#define GCM_SAVED_STATE_VERSION     1

/**
 * GCM Fixer Identifiers.
 * @remarks Part of saved state!
 * @{
 */
/** DOS division by zero, the worst. Includes Windows 3.x. */
#define GCMFIXER_DBZ_DOS                RT_BIT_32(GCMFIXER_DBZ_DOS_BIT)
#define GCMFIXER_DBZ_DOS_BIT            0
/** OS/2 (any version) division by zero. */
#define GCMFIXER_DBZ_OS2                RT_BIT_32(GCMFIXER_DBZ_OS2_BIT)
#define GCMFIXER_DBZ_OS2_BIT            1
/** Windows 9x division by zero. */
#define GCMFIXER_DBZ_WIN9X              RT_BIT_32(GCMFIXER_DBZ_WIN9X_BIT)
#define GCMFIXER_DBZ_WIN9X_BIT          2
/** Workaround for the Mesa vmsvga driver using a IN/OUT backdoor.
 * @since 7.1  */
#define GCMFIXER_MESA_VMSVGA_DRV        RT_BIT_32(GCMFIXER_MESA_VMSVGA_DRV_BIT)
#define GCMFIXER_MESA_VMSVGA_DRV_BIT    3
/** @} */

/**
 * GCM VM Instance data.
 */
typedef struct GCM
{
    /** The provider that is active for this VM. */
    uint32_t                        fFixerSet;
} GCM;
/** Pointer to GCM VM instance data. */
typedef GCM *PGCM;

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_GCMInternal_h */

