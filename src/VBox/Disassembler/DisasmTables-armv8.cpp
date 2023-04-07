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

#define DIS_ARMV8_OP(a_fMask, a_fValue, a_szOpcode, a_uOpcode, a_fOpType) \
    { a_fMask, a_fValue, OP(a_szOpcode, 0, 0, 0, a_uOpcode, OP_ARMV8_PARM_NONE, OP_ARMV8_PARM_NONE, OP_ARMV8_PARM_NONE, a_fOpType) }

#ifndef DIS_CORE_ONLY
static char g_szInvalidOpcode[] = "Invalid Opcode";
#endif

#define INVALID_OPCODE  \
    DIS_ARMV8_OP(0xffffffff, 0, g_szInvalidOpcode,    OP_ARMV8_INVALID, DISOPTYPE_INVALID)


/* Invalid opcode */
DECL_HIDDEN_CONST(DISOPCODE) g_ArmV8A64InvalidOpcode[1] =
{
    OP(g_szInvalidOpcode, 0, 0, 0, OP_ARMV8_INVALID, OP_ARMV8_PARM_NONE, OP_ARMV8_PARM_NONE, OP_ARMV8_PARM_NONE, DISOPTYPE_INVALID)
};


/* UDF */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_aArmV8A64InsnRsvd)
    DIS_ARMV8_OP(0xffff0000, 0x00000000, "udf %I" ,                 OP_ARMV8_A64_UDF,       DISOPTYPE_INVALID)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_aArmV8A64InsnRsvd, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeNop, 0xffff0000, 16)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,    0, 16),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* ADR/ADRP */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Adr)
    DIS_ARMV8_OP(0x9f000000, 0x10000000, "adr %X,%I" ,              OP_ARMV8_A64_ADR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x9f000000, 0x90000000, "adrp %X,%I" ,             OP_ARMV8_A64_ADRP,      DISOPTYPE_HARMLESS)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64Adr, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(31), 31)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,    0, 5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmAdr, 0, 0),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* ADD/ADDS/SUB/SUBS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64AddSubImm)
    DIS_ARMV8_OP(0x7f800000, 0x11000000, "add %X,%X,%I" ,           OP_ARMV8_A64_ADD,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x31000000, "adds %X,%X,%I" ,          OP_ARMV8_A64_ADDS,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x51000000, "sub %X,%X,%I" ,           OP_ARMV8_A64_SUB,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x71000000, "subs %X,%X,%I" ,          OP_ARMV8_A64_SUBS,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64AddSubImm, DISARMV8INSNCLASS_F_SF,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,    0, 5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,    5, 5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,  10, 12),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* AND/ORR/EOR/ANDS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64LogicalImm)
    DIS_ARMV8_OP(0x7f800000, 0x12000000, "and %X,%X,%I" ,           OP_ARMV8_A64_AND,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x32000000, "orr %X,%X,%I" ,           OP_ARMV8_A64_ORR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x52000000, "eor %X,%X,%I" ,           OP_ARMV8_A64_EOR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x72000000, "ands %X,%X,%I" ,          OP_ARMV8_A64_ANDS,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64LogicalImm, DISARMV8INSNCLASS_F_SF,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            5,  6),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmsImmrN,     10, 13),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* MOVN/MOVZ/MOVK */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64MoveWide)
    DIS_ARMV8_OP(0x7f800000, 0x12800000, "movn %X,%I LSL %I",       OP_ARMV8_A64_MOVN,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x7f800000, 0x52800000, "movz %X,%I LSL %I" ,      OP_ARMV8_A64_MOVZ,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x72800000, "movk %X,%I LSL %I" ,      OP_ARMV8_A64_MOVK,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64MoveWide, DISARMV8INSNCLASS_F_SF,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,            5, 16),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseHw,            21,  2),
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* SBFM/BFM/UBFM */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Bitfield)
    DIS_ARMV8_OP(0x7f800000, 0x13000000, "sbfm %X,%X,%I",           OP_ARMV8_A64_SBFM,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x33000000, "bfm  %X,%X,%I",           OP_ARMV8_A64_BFM,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x7f800000, 0x23000000, "ubfm %X,%X,%I",           OP_ARMV8_A64_UBFM,      DISOPTYPE_HARMLESS),
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
    DIS_ARMV8_OP(0xff000010, 0x54000000, "b.%C   %J",               OP_ARMV8_A64_B,         DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW | DISOPTYPE_RELATIVE_CONTROLFLOW | DISOPTYPE_COND_CONTROLFLOW),
    DIS_ARMV8_OP(0xff000010, 0x54000010, "bc.%C  %J" ,              OP_ARMV8_A64_BC,        DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW | DISOPTYPE_RELATIVE_CONTROLFLOW | DISOPTYPE_COND_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64CondBr, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(4), 4)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseCond,           0,  4),
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImmRel,         5, 19),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* SVC/HVC/SMC/BRK/HLT/TCANCEL/DCPS1/DCPS2/DCPS3 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Excp)
    DIS_ARMV8_OP(0xffe0001f, 0xd4000001, "svc       %I",            OP_ARMV8_A64_SVC,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xffe0001f, 0xd4000002, "hvc       %I",            OP_ARMV8_A64_HVC,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT | DISOPTYPE_PRIVILEGED),
    DIS_ARMV8_OP(0xffe0001f, 0xd4000003, "smc       %I",            OP_ARMV8_A64_SMC,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT | DISOPTYPE_PRIVILEGED),
    DIS_ARMV8_OP(0xffe0001f, 0xd4200000, "brk       %I",            OP_ARMV8_A64_BRK,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xffe0001f, 0xd4400000, "hlt       %I",            OP_ARMV8_A64_HLT,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xffe0001f, 0xd4600000, "tcancel   %I",            OP_ARMV8_A64_TCANCEL,   DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT), /* FEAT_TME */
    DIS_ARMV8_OP(0xffe0001f, 0xd4a00001, "dcps1     %I",            OP_ARMV8_A64_DCPS1,     DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xffe0001f, 0xd4a00002, "dcps2     %I",            OP_ARMV8_A64_DCPS2,     DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xffe0001f, 0xd4a00003, "dcps3     %I",            OP_ARMV8_A64_DCPS3,     DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64Excp, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeLookup, 0xffe0001f, 0)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,            5, 16),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* WFET/WFIT */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64SysReg)
    DIS_ARMV8_OP(0xffffffe0, 0xd5031000, "wfet   %X",  OP_ARMV8_A64_WFET,      DISOPTYPE_HARMLESS), /* FEAT_WFxT */
    DIS_ARMV8_OP(0xffffffe0, 0x54000010, "wfit   %X" , OP_ARMV8_A64_WFIT,      DISOPTYPE_HARMLESS), /* FEAT_WFxT */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64SysReg, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                          kDisArmV8OpcDecodeNop, 0xfe0, 5)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* Various hint instructions */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Hints)
    DIS_ARMV8_OP(0xffffffff, 0xd503201f, "nop",        OP_ARMV8_A64_NOP,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xffffffff, 0xd503203f, "yield",      OP_ARMV8_A64_YIELD,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xffffffff, 0xd503205f, "wfe",        OP_ARMV8_A64_WFE,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xffffffff, 0xd503207f, "wfi",        OP_ARMV8_A64_WFI,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xffffffff, 0xd503209f, "sev",        OP_ARMV8_A64_SEV,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xffffffff, 0xd50320bf, "sevl",       OP_ARMV8_A64_SEVL,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xffffffff, 0xd50320df, "dgh",        OP_ARMV8_A64_DGH,       DISOPTYPE_HARMLESS), /* FEAT_DGH */
    DIS_ARMV8_OP(0xffffffff, 0xd50320ff, "xpaclri",    OP_ARMV8_A64_XPACLRI,   DISOPTYPE_HARMLESS), /* FEAT_PAuth */
    /** @todo */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64Hints, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeNop, 0xfe0, 5)
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* CLREX */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64Clrex)
    DIS_ARMV8_OP(0xfffff0ff, 0xd503305f, "clrex %I",   OP_ARMV8_A64_CLREX,     DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64Clrex, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeNop, 0, 0)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,            8,  4),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* Barrier instructions, we divide these instructions further based on the op2 field. */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(g_ArmV8A64DecodeBarriers)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo DSB - Encoding */
    DIS_ARMV8_DECODE_MAP_ENTRY(g_ArmV8A64Clrex),    /* CLREX */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo TCOMMIT */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo DSB - Encoding */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo DMB */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo ISB */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY              /** @todo SB */
DIS_ARMV8_DECODE_MAP_DEFINE_END(g_ArmV8A64DecodeBarriers, RT_BIT_32(5) | RT_BIT_32(6) | RT_BIT_32(7), 5);


/* MSR (and potentially CFINV,XAFLAG,AXFLAG) */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64PState)
    DIS_ARMV8_OP(0xfffff0ff, 0xd503305f, "msr %P, %I", OP_ARMV8_A64_MSR,       DISOPTYPE_PRIVILEGED),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64PState, 0 /*fClass*/,
                                          kDisArmV8OpcDecodeNop, 0, 0)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParsePState,         0,  0), /* This is special for the MSR instruction. */
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseImm,            8,  4), /* CRm field encodes the immediate value */
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


/* TSTART/TTEST */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(g_ArmV8A64SysResult)
    DIS_ARMV8_OP(0xfffffffe, 0xd5233060, "tstart %X",  OP_ARMV8_A64_TSTART,    DISOPTYPE_HARMLESS | DISOPTYPE_PRIVILEGED),  /* FEAT_TME */
    DIS_ARMV8_OP(0xfffffffe, 0xd5233160, "ttest  %X",  OP_ARMV8_A64_TTEST,     DISOPTYPE_HARMLESS),                         /* FEAT_TME */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_PARAMS(g_ArmV8A64SysResult, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                          kDisArmV8OpcDecodeNop, RT_BIT_32(8) | RT_BIT_32(9) | RT_BIT_32(10) | RT_BIT_32(11), 8)
    DIS_ARMV8_INSN_PARAM_CREATE(kDisParmParseReg,            0,  5),
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE,
    DIS_ARMV8_INSN_PARAM_NONE
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END;


DIS_ARMV8_DECODE_TBL_DEFINE_BEGIN(g_ArmV8A64BrExcpSys)
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfe000000, RT_BIT_32(26) | RT_BIT_32(28) | RT_BIT_32(30),                  g_ArmV8A64CondBr),          /* op0: 010, op1: 0xxxxxxxxxxxxx, op2: - (including o1 from the conditional branch (immediate) class to save us one layer). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xff000000, RT_BIT_32(26) | RT_BIT_32(28) | RT_BIT_32(30) | RT_BIT_32(31),  g_ArmV8A64Excp),            /* op0: 110, op1: 00xxxxxxxxxxxx, op2: -. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff000, 0xd5031000,                                                     g_ArmV8A64SysReg),          /* op0: 110, op1: 01000000110001, op2: -. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff01f, 0xd503201f,                                                     g_ArmV8A64Hints),           /* op0: 110, op1: 01000000110010, op2: 11111. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff01f, 0xd503301f,                                                     g_ArmV8A64DecodeBarriers),  /* op0: 110, op1: 01000000110011, op2: - (we include Rt:  11111 from the next stage here). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfff8f01f, 0xd500401f,                                                     g_ArmV8A64PState),          /* op0: 110, op1: 0100000xxx0100, op2: - (we include Rt:  11111 from the next stage here). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff0e0, 0xd5233060,                                                     g_ArmV8A64SysResult)        /* op0: 110, op1: 0100100xxxxxxx, op2: - (we include op1, CRn and op2 from the next stage here). */
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
