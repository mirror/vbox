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
 * Finalizes the given FDT for writing out.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 */
RTDECL(int) RTFdtFinalize(RTFDT hFdt);


/**
 * Allocates a new phandle serving as a unique identifer within the given FDT.
 *
 * @returns The phandle value
 * @retval  UINT32_MAX in case the given FDT handle is invalid or the FDT is out of free phandle values.
 * @param   hFdt            The flattened devicetree handle to destroy.
 */
RTDECL(uint32_t) RTFdtPHandleAllocate(RTFDT hFdt);


/**
 * Dumps the given flattened devicetree to the given VFS file.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   enmOutType      The output type.
 * @param   fFlags          Flags controlling the output, MBZ.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pErrInfo        Where to return additional error information.
 */
RTDECL(int) RTFdtDumpToVfsIoStrm(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo);


/**
 * Dumps the given flattened devicetree to the given file.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   enmOutType      The output type.
 * @param   fFlags          Flags controlling the output, MBZ.
 * @param   pszFilename     The filename to dump to.
 * @param   pErrInfo        Where to return additional error information.
 */
RTDECL(int) RTFdtDumpToFile(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, const char *pszFilename, PRTERRINFO pErrInfo);


/**
 * Adds a new memory reservation to the given FDT instance.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   PhysAddrStart   The physical start address of the reservation.
 * @param   cbArea          Size of the area in bytes.
 */
RTDECL(int) RTFdtAddMemoryReservation(RTFDT hFdt, uint64_t PhysAddrStart, uint64_t cbArea);


/**
 * Sets the physical boot CPU id for the given FDT instance.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   idPhysBootCpu   The physical boot CPU ID.
 */
RTDECL(int) RTFdtSetPhysBootCpuId(RTFDT hFdt, uint32_t idPhysBootCpu);


/**
 * Adds a new ode with the given name under the currently active one.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszName         The name of the node.
 */
RTDECL(int) RTFdtNodeAdd(RTFDT hFdt, const char *pszName);


/**
 * Adds a new ode with the given name under the currently active one.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszNameFmt      The name of the node as a format string.
 * @param   ...             The format arguments.
 */
RTDECL(int) RTFdtNodeAddF(RTFDT hFdt, const char *pszNameFmt, ...) RT_IPRT_FORMAT_ATTR(2, 3);


/**
 * Adds a new ode with the given name under the currently active one.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszNameFmt      The name of the node as a format string.
 * @param   va              The format arguments.
 */
RTDECL(int) RTFdtNodeAddV(RTFDT hFdt, const char *pszNameFmt, va_list va) RT_IPRT_FORMAT_ATTR(2, 0);


/**
 * Finalizes the current active node and returns the state to the parent node.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 */
RTDECL(int) RTFdtNodeFinalize(RTFDT hFdt);


/**
 * Adds a single empty property with the given name to the current node acting as a boolean.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszProperty     The property name.
 */
RTDECL(int) RTFdtNodePropertyAddEmpty(RTFDT hFdt, const char *pszProperty);


/**
 * Adds a single u32 property with the given name to the current node.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszProperty     The property name.
 * @param   u32             The u32 value to set the property to.
 */
RTDECL(int) RTFdtNodePropertyAddU32(RTFDT hFdt, const char *pszProperty, uint32_t u32);


/**
 * Adds a string property with the given name to the current node.
 *
 * @returns IPRT status code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszProperty     The property name.
 * @param   pszVal          The string value to set the property to.
 */
RTDECL(int) RTFdtNodePropertyAddString(RTFDT hFdt, const char *pszProperty, const char *pszVal);


/**
 * Adds a property with a variable number of u32 items.
 *
 * @returns IPRT staus code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszProperty     The property name.
 * @param   cCells          The number of cells.
 * @param   ...             The cell u32 data items.
 */
RTDECL(int) RTFdtNodePropertyAddCellsU32(RTFDT hFdt, const char *pszProperty, uint32_t cCells, ...);


/**
 * Adds a property with a variable number of u32 items.
 *
 * @returns IPRT staus code.
 * @param   hFdt            The flattened devicetree handle.
 * @param   pszProperty     The property name.
 * @param   cCells          The number of cells.
 * @param   va              The cell u32 data items.
 */
RTDECL(int) RTFdtNodePropertyAddCellsU32V(RTFDT hFdt, const char *pszProperty, uint32_t cCells, va_list va);

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_fdt_h */

