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
 * GPR definition
 */
typedef struct
{
    /** Flag whether this is a 32-bit or 64-bit register. */
                     bool    f32Bit : 1;
    /** The register index. */
    RT_GCC_EXTENSION uint8_t idGpr  : 7;
} DISOPPARAMARMV8REG;
AssertCompileSize(DISOPPARAMARMV8REG, sizeof(uint8_t));
/** Pointer to a disassembler GPR. */
typedef DISOPPARAMARMV8REG *PDISOPPARAMARMV8REG;
/** Pointer to a const disasssembler GPR. */
typedef const DISOPPARAMARMV8REG *PCDISOPPARAMARMV8REG;


/**
 * Opcode parameter (operand) details.
 */
typedef struct
{
    /** Parameter type (Actually DISARMV8OPPARM). */
    uint8_t                         enmType;
    /** Any extension applied (DISARMV8OPPARMEXTEND). */
    uint8_t                         enmExtend;
    /** The register operand. */
    union
    {
        /** General register index (DISGREG_XXX), applicable if DISUSE_REG_GEN32
         * or DISUSE_REG_GEN64 is set in fUse. */
        DISOPPARAMARMV8REG          Gpr;
        /** IPRT System register encoding. */
        uint16_t                    idSysReg;
        /**
         *  Conditional parameter (not a register I know but this saves us struct size and
         *  and these never occur at the same time, might get renamed if everything is done).
         *
         *  DISARMV8INSTRCOND
         */
        uint8_t                     enmCond;
    } Reg;
    /** Register holding the offset. Applicable if DISUSE_INDEX is set in fUse. */
    DISOPPARAMARMV8REG              GprIndex;
    /** Parameter size. */
    uint8_t                         cb;
    union
    {
        /** Offset from the base register. */
        int16_t                     offBase;
        /** Amount of bits to extend. */
        uint8_t                     cExtend;
    } u;
} DIS_OP_PARAM_ARMV8_T;
AssertCompile(sizeof(DIS_OP_PARAM_ARMV8_T) <= 16);
/** Pointer to opcode parameter. */
typedef DIS_OP_PARAM_ARMV8_T *PDIS_OP_PARAM_ARMV8_T;
/** Pointer to opcode parameter. */
typedef const DIS_OP_PARAM_ARMV8_T *PCDIS_OP_PARAM_ARMV8_T;


/**
 * The armv8 specific disassembler state and result.
 */
typedef struct
{
    /** Condition flag for the instruction - kArmv8InstrCond_Al if not conditional instruction. */
    DISARMV8INSTRCOND   enmCond;
    /** Operand size (for loads/stores primarily). */
    uint8_t             cbOperand;
} DIS_STATE_ARMV8_T;
AssertCompile(sizeof(DIS_STATE_ARMV8_T) <= 32);


/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_dis_armv8_h */

