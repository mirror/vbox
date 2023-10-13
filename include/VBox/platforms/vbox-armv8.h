/** @file
 * VirtualBox - ARMv8 virtual platform definitions shared by the device and firmware.
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

#ifndef VBOX_INCLUDED_platforms_vbox_armv8_h
#define VBOX_INCLUDED_platforms_vbox_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>
#include <iprt/types.h>
#include <iprt/cdefs.h>


/**
 * The VBox region descriptor.
 */
typedef struct VBOXPLATFORMARMV8
{
    /** A magic value identifying the descriptor. */
    uint32_t                    u32Magic;
    /** Decriptor version. */
    uint32_t                    u32Version;
    /** Size of this descriptor. */
    uint32_t                    cbDesc;
    /** Some flags (MBZ for now). */
    uint32_t                    fFlags;
    /** Base RAM region start address. */
    uint64_t                    u64PhysAddrRamBase;
    /** Size of the base RAM region in bytes. */
    uint64_t                    cbRamBase;
    /** Offset to the beginning of the FDT backwards from the start of this descriptor, 0 if not available. */
    uint64_t                    u64OffBackFdt;
    /** Size of the FDT in bytes, 0 if not available. */
    uint64_t                    cbFdt;
    /** Offset to the RDSP/XSDP table for ACPI backwards from the start of this descriptor, 0 if not available. */
    uint64_t                    u64OffBackAcpiXsdp;
    /** Size of the RDSP/XSDP table, 0 if not available. */
    uint64_t                    cbAcpiXsdp;
    /** Offset backwards to the start of the UEFI ROM region from the start of this descriptor, 0 if not available (doesn't make much sense though). */
    uint64_t                    u64OffBackUefiRom;
    /** Size if the UEFI ROM region in bytes, 0 if not available. */
    uint64_t                    cbUefiRom;
    /** Offset backwards to the start of the MMIO region from the start of this descriptor, 0 if not available (doesn't make much sense though). */
    uint64_t                    u64OffBackMmio;
    /** Size of the MMIO region in bytes, 0 if not available. */
    uint64_t                    cbMmio;
    /** Padding to 64KiB. */
    uint8_t                     abPadding[_64K - 4 * sizeof(uint32_t) - 10 * sizeof(uint64_t)];
} VBOXPLATFORMARMV8;
AssertCompileSize(VBOXPLATFORMARMV8, _64K);
typedef VBOXPLATFORMARMV8 *PVBOXPLATFORMARMV8;
typedef const VBOXPLATFORMARMV8 *PCVBOXPLATFORMARMV8;

/** Magic identifying the VirtualBox descriptor. */
#define VBOXPLATFORMARMV8_MAGIC   RT_MAKE_U32_FROM_U8('V', '8', 'O', 'X')
/** Current version of the descriptor. */
#define VBOXPLATFORMARMV8_VERSION 0x1

#endif /* !VBOX_INCLUDED_platforms_vbox_armv8_h */
