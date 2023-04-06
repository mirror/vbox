/* $Id$ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/dis.h>
#include <VBox/disopcode-armv8.h>
#include "DisasmInternal-armv8.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

#define DIS_ARMV8_OP(a_szOpcode, a_uOpcode, a_fOpType) \
    OP(a_szOpcode, 0, 0, 0, a_uOpcode, OP_ARMV8_PARM_NONE, OP_ARMV8_PARM_NONE, OP_ARMV8_PARM_NONE, a_fOpType)

#ifndef DIS_CORE_ONLY
static char g_szInvalidOpcode[] = "Invalid Opcode";
#endif

#define INVALID_OPCODE  \
    DIS_ARMV8_OP(g_szInvalidOpcode,    OP_ARMV8_INVALID, DISOPTYPE_INVALID)


/* Invalid opcode */
DECL_HIDDEN_CONST(DISOPCODE) g_ArmV8A64InvalidOpcode[1] =
{
    INVALID_OPCODE
};


/* UDF */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_aArmV8A64InsnRsvd)
    DIS_ARMV8_OP("udf %I" ,                 OP_ARMV8_A64_UDF,   DISOPTYPE_INVALID)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_aArmV8A64InsnRsvd, 0 /*fClass*/, kDisArmV8OpcDecodeNop, 0xffff0000, 16)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,    0, 16),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* ADR/ADRP */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Adr)
    DIS_ARMV8_OP("adr %X,%I" ,              OP_ARMV8_A64_ADR,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("adrp %X,%I" ,             OP_ARMV8_A64_ADRP,  DISOPTYPE_HARMLESS)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64Adr, DISARMV8INSNCLASS_F_FORCED_64BIT, kDisArmV8OpcDecodeNop, RT_BIT_32(31), 31)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,    0, 5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmAdr, 0, 0),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* ADD/ADDS/SUB/SUBS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64AddSubImm)
    DIS_ARMV8_OP("add %X,%X,%I" ,           OP_ARMV8_A64_ADD,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("adds %X,%X,%I" ,          OP_ARMV8_A64_ADDS,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("sub %X,%X,%I" ,           OP_ARMV8_A64_SUB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("subs %X,%X,%I" ,          OP_ARMV8_A64_SUBS,  DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64AddSubImm, DISARMV8INSNCLASS_F_SF, kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,    0, 5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,    5, 5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,  10, 12),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* AND/ORR/EOR/ANDS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64LogicalImm)
    DIS_ARMV8_OP("and %X,%X,%I" ,           OP_ARMV8_A64_AND,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("orr %X,%X,%I" ,           OP_ARMV8_A64_ORR,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("eor %X,%X,%I" ,           OP_ARMV8_A64_EOR,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("ands %X,%X,%I" ,          OP_ARMV8_A64_ANDS,  DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64LogicalImm, DISARMV8INSNCLASS_F_SF, kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            5,  6),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmsImmrN,     10, 13),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* MOVN/MOVZ/MOVK */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64MoveWide)
    DIS_ARMV8_OP("movn %X,%I LSL %I",       OP_ARMV8_A64_MOVN,  DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    DIS_ARMV8_OP("movz %X,%I LSL %I" ,      OP_ARMV8_A64_MOVZ,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("movk %X,%I LSL %I" ,      OP_ARMV8_A64_MOVK,  DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64MoveWide, DISARMV8INSNCLASS_F_SF, kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,            5, 16),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseHw,            21,  2),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* SBFM/BFM/UBFM */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Bitfield)
    DIS_ARMV8_OP("sbfm %X,%X,%I",  OP_ARMV8_A64_SBFM,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("bfm  %X,%X,%I" , OP_ARMV8_A64_BFM,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP("ubfm %X,%X,%I" , OP_ARMV8_A64_UBFM,  DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64Bitfield, DISARMV8INSNCLASS_F_SF | DISARMV8INSNCLASS_F_N_FORCED_1_ON_64BIT,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            5,  5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmsImmrN,     10, 13),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/*
 * C4.1.65 of the ARMv8 architecture reference manual has the following table for the
 * data processing (immediate) instruction classes:
 *
 *     Bit  25 24 23
 *     +-------------------------------------------
 *           0  0  x PC-rel. addressing.
 *           0  1  0 Add/subtract (immediate)
 *           0  1  1 Add/subtract (immediate, with tags)
 *           1  0  0 Logical (immediate)
 *           1  0  1 Move wide (immediate)
 *           1  1  0 Bitfield
 *           1  1  1 Extract
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(g_aArmV8A64InsnDataProcessingImm)
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64Adr),
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64Adr),
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64AddSubImm),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo Add/subtract immediate with tags. */
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64LogicalImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64MoveWide),
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64Bitfield),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY                  /** @todo Extract */
DIS_ARMV8_DECODE_MAP_DEFINE_END(g_aArmV8A64InsnDataProcessingImm, RT_BIT_32(23) | RT_BIT_32(24) | RT_BIT_32(25),  23);


/* B.cond/BC.cond */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64CondBr)
    DIS_ARMV8_OP("b.%C   %J",  OP_ARMV8_A64_B,         DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW | DISOPTYPE_RELATIVE_CONTROLFLOW | DISOPTYPE_COND_CONTROLFLOW),
    DIS_ARMV8_OP("bc.%C  %J" , OP_ARMV8_A64_BC,        DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW | DISOPTYPE_RELATIVE_CONTROLFLOW | DISOPTYPE_COND_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64CondBr, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(4), 4)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseCond,           0,  4),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmRel,         5, 19),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


DIS_ARMV8_DECODE_TBL_DEFINE_BEGIN(g_ArmV8A64BrExcpSys)
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xff000000, RT_BIT_32(26) | RT_BIT_32(28) | RT_BIT_32(30), g_ArmV8A64CondBr) /* op0: 010, op1: 0xxxxxxxxxxxxx, op2: - (including o1 from the conditional branch (immediate) class to save us one layer). */
DIS_ARMV8_DECODE_TBL_DEFINE_END(g_ArmV8A64BrExcpSys);


/*
 * C4.1 of the ARMv8 architecture reference manual has the following table for the
 * topmost decoding level (Level 0 in our terms), x means don't care:
 *
 *     Bit  28 27 26 25
 *     +-------------------------------------------
 *           0  0  0  0 Reserved or SME encoding (depends on bit 31).
 *           0  0  0  1 UNALLOC
 *           0  0  1  0 SVE encodings
 *           0  0  1  1 UNALLOC
 *           1  0  0  x Data processing immediate
 *           1  0  1  x Branch, exception generation and system instructions
 *           x  1  x  0 Loads and stores
 *           x  1  0  1 Data processing - register
 *           x  1  1  1 Data processing - SIMD and floating point
 *
 * In order to save us some fiddling with the don't care bits we blow up the lookup table
 * which gives us 16 possible values (4 bits) we can use as an index into the decoder
 * lookup table for the next level:
 *     Bit  28 27 26 25
 *     +-------------------------------------------
 *      0    0  0  0  0 Reserved or SME encoding (depends on bit 31).
 *      1    0  0  0  1 UNALLOC
 *      2    0  0  1  0 SVE encodings
 *      3    0  0  1  1 UNALLOC
 *      4    0  1  0  0 Loads and stores
 *      5    0  1  0  1 Data processing - register
 *      6    0  1  1  0 Loads and stores
 *      7    0  1  1  1 Data processing - SIMD and floating point
 *      8    1  0  0  0 Data processing immediate
 *      9    1  0  0  1 Data processing immediate
 *     10    1  0  1  0 Branch, exception generation and system instructions
 *     11    1  0  1  1 Branch, exception generation and system instructions
 *     12    1  1  0  0 Loads and stores
 *     13    1  1  0  1 Data processing - register
 *     14    1  1  1  0 Loads and stores
 *     15    1  1  1  1 Data processing - SIMD and floating point
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(g_ArmV8A64DecodeL0)
    DIS_ARMV8_DECODE_MAP_ENTRY(g_aArmV8A64InsnRsvd),                /* Reserved class or SME encoding (@todo). */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Unallocated */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /** @todo SVE */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Unallocated */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Load/Stores */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Data processing (register). */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Lod/Stores */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Data processing (SIMD & FP) */
    DIS_ARMV8_DECODE_MAP_ENTRY(g_aArmV8A64InsnDataProcessingImm),   /* Data processing (immediate). */
    DIS_ARMV8_DECODE_MAP_ENTRY(g_aArmV8A64InsnDataProcessingImm),   /* Data processing (immediate). */
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64BrExcpSys),                /* Branches / Exception generation and system instructions. */
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64BrExcpSys),                /* Branches / Exception generation and system instructions. */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Load/Stores. */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Data processing (register). */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Load/Stores. */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY                              /* Data processing (SIMD & FP). */
DIS_ARMV8_DECODE_MAP_DEFINE_END_NON_STATIC(g_ArmV8A64DecodeL0, RT_BIT_32(25) | RT_BIT_32(26) | RT_BIT_32(27) | RT_BIT_32(28), 25);
