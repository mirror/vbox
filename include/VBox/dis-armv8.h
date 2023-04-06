/** @file
 * DIS - The VirtualBox Disassembler.
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

#ifndef VBOX_INCLUDED_dis_armv8_h
#define VBOX_INCLUDED_dis_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/disopcode-armv8.h>
#include <iprt/assert.h>


RT_C_DECLS_BEGIN

/** @addtogroup grp_dis   VBox Disassembler
 * @{ */

/**
 * Opcode parameter (operand) details.
 */
typedef struct DISOPPARAMARMV8
{
    /** The register operand. */
    union
    {
        /** General register index (DISGREG_XXX), applicable if DISUSE_REG_GEN32
         * or DISUSE_REG_GEN64 is set in fUse. */
        uint8_t     idxGenReg;
    } Reg;
    /** Scale factor. */
    uint8_t           uScale;
    /** Parameter size. */
    uint8_t           cb;
    /** Copy of the corresponding DISOPCODE::fParam1 / DISOPCODE::fParam2 /
     * DISOPCODE::fParam3. */
    uint32_t          fParam;
} DISOPPARAMARMV8;
//AssertCompileSize(DISOPPARAMARMV8, 16);
/** Pointer to opcode parameter. */
typedef DISOPPARAMARMV8 *PDISOPPARAMARMV8;
/** Pointer to opcode parameter. */
typedef const DISOPPARAMARMV8 *PCDISOPPARAMARMV8;


/**
 * The armv8 specific disassembler state and result.
 */
typedef struct DISSTATEARMV8
{
    uint8_t bDummy;
} DISSTATEARMV8;
//AssertCompileSize(DISSTATEARMV8, 32);


/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_dis_armv8_h */

