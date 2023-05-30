/** @file
 * IPRT - Flattened Devicetree parser and generator API.
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

#ifndef IPRT_INCLUDED_fdt_h
#define IPRT_INCLUDED_fdt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/vfs.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_fdt    RTFdt - Flattened Devicetree parser and generator API.
 * @ingroup grp_rt
 * @{
 */

/**
 * Flattened device tree type.
 */
typedef enum RTFDTTYPE
{
    /** The invalid output type. */
    RTFDTTYPE_INVALID = 0,
    /** Output is an UTF-8 DTS. */
    RTFDTTYPE_DTS,
    /** Output is the flattened devicetree binary format. */
    RTFDTTYPE_DTB,
    /** Usual 32-bit hack. */
    RTFDTTYPE_32BIT_HACK = 0x7fffffff
} RTFDTTYPE;


#ifdef IN_RING3

/**
 * Creates a new empty flattened devicetree returning the handle.
 *
 * @returns IPRT status code.
 * @param   phFdt           Where to store the handle to the flattened devicetree on success.
 */
RTDECL(int) RTFdtCreateEmpty(PRTFDT phFdt);


/**
 * Creates a flattened device tree from the given VFS file.
 *
 * @returns IPRT status code.
 * @param   phFdt           Where to store the handle to the flattened devicetree on success.
 * @param   hVfsIos         The VFS I/O stream handle to read the devicetree from.
 * @param   enmInType       The input type of the flattened devicetree.
 * @param   pErrInfo        Where to return additional error information.
 */
RTDECL(int) RTFdtCreateFromVfsIoStrm(PRTFDT phFdt, RTVFSIOSTREAM hVfsIos, RTFDTTYPE enmInType, PRTERRINFO pErrInfo);


/**
 * Creates a flattened device tree from the given filename.
 *
 * @returns IPRT status code.
 * @param   phFdt           Where to store the handle to the flattened devicetree on success.
 * @param   pszFilename     The filename to read the devicetree from.
 * @param   enmInType       The input type of the flattened devicetree.
 * @param   pErrInfo        Where to return additional error information.
 */
RTDECL(int) RTFdtCreateFromFile(PRTFDT phFdt, const char *pszFilename, RTFDTTYPE enmInType, PRTERRINFO pErrInfo);


/**
 * Destroys the given flattened devicetree.
 *
 * @param   hFdt            The flattened devicetree handle to destroy.
 */
RTDECL(void) RTFdtDestroy(RTFDT hFdt);


/**
 * Dumps the given flattened devicetree to the given VFS file.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   enmOutType      The output type.
 * @param   fFlags          Flags controlling the output, MBZ.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 */
RTDECL(int) RTFdtDumpToVfsIoStrm(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, RTVFSIOSTREAM hVfsIos);


/**
 * Dumps the given flattened devicetree to the given file.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   enmOutType      The output type.
 * @param   fFlags          Flags controlling the output, MBZ.
 * @param   pszFilename     The filename to dump to.
 */
RTDECL(int) RTFdtDumpToFile(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, const char *pszFilename);

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_fdt_h */

