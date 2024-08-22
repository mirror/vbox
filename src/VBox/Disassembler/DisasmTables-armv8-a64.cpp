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

#define DIS_ARMV8_OP(a_fValue, a_szOpcode, a_uOpcode, a_fOpType) \
    { a_fValue, OP(a_szOpcode, 0, 0, 0, a_uOpcode, 0, 0, 0, a_fOpType) }

#ifndef DIS_CORE_ONLY
static char g_szInvalidOpcode[] = "Invalid Opcode";
#endif

#define INVALID_OPCODE  \
    DIS_ARMV8_OP(0, g_szInvalidOpcode,    OP_ARMV8_INVALID, DISOPTYPE_INVALID)


/* Invalid opcode */
DECL_HIDDEN_CONST(DISOPCODE) g_ArmV8A64InvalidOpcode[1] =
{
    OP(g_szInvalidOpcode, 0, 0, 0, 0, 0, 0, 0, DISOPTYPE_INVALID)
};


/* UDF */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Rsvd)
    DIS_ARMV8_OP(0x00000000, "udf" ,            OP_ARMV8_A64_UDF,       DISOPTYPE_INVALID)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Rsvd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,    0, 16, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(Rsvd, 0xffff0000 /*fFixedInsn*/, 0 /*fClass*/,
                                            kDisArmV8OpcDecodeNop, 0xffff0000, 16,
                                            kDisArmv8OpParmImm);

/* ADR/ADRP */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Adr)
    DIS_ARMV8_OP(0x10000000, "adr" ,            OP_ARMV8_A64_ADR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x90000000, "adrp" ,           OP_ARMV8_A64_ADRP,      DISOPTYPE_HARMLESS)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Adr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,    0, 5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmAdr, 0, 0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(Adr, 0x9f000000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(31), 31,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmImmRel);


/* ADD/ADDS/SUB/SUBS - shifted immediate variant */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(AddSubImm)
    DIS_ARMV8_OP(0x11000000, "add" ,            OP_ARMV8_A64_ADD,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x31000000, "adds" ,           OP_ARMV8_A64_ADDS,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x51000000, "sub" ,            OP_ARMV8_A64_SUB,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x71000000, "subs" ,           OP_ARMV8_A64_SUBS,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(AddSubImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,    0, 5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,    5, 5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,  10, 12, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSh12, 22,  1, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_3(AddSubImm, 0x7f800000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmGpr, kDisArmv8OpParmImm);


/* ADD/ADDS/SUB/SUBS - shifted register variant */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(AddSubShiftReg)
    DIS_ARMV8_OP(0x0b000000, "add" ,            OP_ARMV8_A64_ADD,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x2b000000, "adds" ,           OP_ARMV8_A64_ADDS,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x4b000000, "sub" ,            OP_ARMV8_A64_SUB,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x6b000000, "subs" ,           OP_ARMV8_A64_SUBS,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(AddSubShiftReg)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,           16,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseShift,         22,  2, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseShiftAmount,   10,  6, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_3(AddSubShiftReg, 0x7f200000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmGpr, kDisArmv8OpParmGpr);


/* AND/ORR/EOR/ANDS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LogicalImm)
    DIS_ARMV8_OP(0x12000000, "and" ,            OP_ARMV8_A64_AND,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x32000000, "orr" ,            OP_ARMV8_A64_ORR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x52000000, "eor" ,            OP_ARMV8_A64_EOR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x72000000, "ands" ,           OP_ARMV8_A64_ANDS,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LogicalImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmsImmrN,     10, 13, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_3(LogicalImm, 0x7f800000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmGpr, kDisArmv8OpParmImm);


/* MOVN/MOVZ/MOVK */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(MoveWide)
    DIS_ARMV8_OP(0x12800000, "movn",            OP_ARMV8_A64_MOVN,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0x52800000, "movz" ,           OP_ARMV8_A64_MOVZ,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x72800000, "movk" ,           OP_ARMV8_A64_MOVK,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(MoveWide)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            5, 16, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseHw,            21,  2, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(MoveWide, 0x7f800000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmImm);


/* SBFM/BFM/UBFM */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Bitfield)
    DIS_ARMV8_OP(0x13000000, "sbfm",            OP_ARMV8_A64_SBFM,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x33000000, "bfm",             OP_ARMV8_A64_BFM,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x53000000, "ubfm",            OP_ARMV8_A64_UBFM,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Bitfield)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,           16,  6, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,           10,  6, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_4(Bitfield, 0x7f800000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF | DISARMV8INSNCLASS_F_N_FORCED_1_ON_64BIT,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmGpr, kDisArmv8OpParmImm, kDisArmv8OpParmImm);


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
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcessingImm)
    DIS_ARMV8_DECODE_MAP_ENTRY(Adr),
    DIS_ARMV8_DECODE_MAP_ENTRY(Adr),
    DIS_ARMV8_DECODE_MAP_ENTRY(AddSubImm),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                 /** @todo Add/subtract immediate with tags. */
    DIS_ARMV8_DECODE_MAP_ENTRY(LogicalImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(MoveWide),
    DIS_ARMV8_DECODE_MAP_ENTRY(Bitfield),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY                  /** @todo Extract */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcessingImm, RT_BIT_32(23) | RT_BIT_32(24) | RT_BIT_32(25),  23);


/* B.cond/BC.cond */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(CondBr)
    DIS_ARMV8_OP(0x54000000, "b",               OP_ARMV8_A64_B,         DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW | DISOPTYPE_RELATIVE_CONTROLFLOW | DISOPTYPE_COND_CONTROLFLOW),
    DIS_ARMV8_OP(0x54000010, "bc" ,             OP_ARMV8_A64_BC,        DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW | DISOPTYPE_RELATIVE_CONTROLFLOW | DISOPTYPE_COND_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(CondBr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseCond,           0,  4, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,         5, 19, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(CondBr, 0xff000010 /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(4), 4,
                                                kDisArmv8OpParmImmRel);


/* SVC/HVC/SMC/BRK/HLT/TCANCEL/DCPS1/DCPS2/DCPS3 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Excp)
    DIS_ARMV8_OP(0xd4000001, "svc",             OP_ARMV8_A64_SVC,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xd4000002, "hvc",             OP_ARMV8_A64_HVC,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT | DISOPTYPE_PRIVILEGED),
    DIS_ARMV8_OP(0xd4000003, "smc",             OP_ARMV8_A64_SMC,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT | DISOPTYPE_PRIVILEGED),
    DIS_ARMV8_OP(0xd4200000, "brk",             OP_ARMV8_A64_BRK,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xd4400000, "hlt",             OP_ARMV8_A64_HLT,       DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xd4600000, "tcancel",         OP_ARMV8_A64_TCANCEL,   DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT), /* FEAT_TME */
    DIS_ARMV8_OP(0xd4a00001, "dcps1",           OP_ARMV8_A64_DCPS1,     DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xd4a00002, "dcps2",           OP_ARMV8_A64_DCPS2,     DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
    DIS_ARMV8_OP(0xd4a00003, "dcps3",           OP_ARMV8_A64_DCPS3,     DISOPTYPE_CONTROLFLOW | DISOPTYPE_INTERRUPT),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Excp)
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            5, 16, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(Excp, 0xffe0001f /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeLookup, 0xffe0001f, 0,
                                                kDisArmv8OpParmImm);


/* WFET/WFIT */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(SysReg)
    DIS_ARMV8_OP(0xd5031000, "wfet",            OP_ARMV8_A64_WFET,      DISOPTYPE_HARMLESS), /* FEAT_WFxT */
    DIS_ARMV8_OP(0x54000010, "wfit" ,           OP_ARMV8_A64_WFIT,      DISOPTYPE_HARMLESS), /* FEAT_WFxT */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(SysReg)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(SysReg, 0xffffffe0 /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, 0xfe0, 5,
                                                kDisArmv8OpParmGpr);


/* Various hint instructions */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Hints)
    DIS_ARMV8_OP(0xd503201f, "nop",             OP_ARMV8_A64_NOP,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd503203f, "yield",           OP_ARMV8_A64_YIELD,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd503205f, "wfe",             OP_ARMV8_A64_WFE,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd503207f, "wfi",             OP_ARMV8_A64_WFI,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd503209f, "sev",             OP_ARMV8_A64_SEV,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd50320bf, "sevl",            OP_ARMV8_A64_SEVL,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd50320df, "dgh",             OP_ARMV8_A64_DGH,       DISOPTYPE_HARMLESS), /* FEAT_DGH */
    DIS_ARMV8_OP(0xd50320ff, "xpaclri",         OP_ARMV8_A64_XPACLRI,   DISOPTYPE_HARMLESS), /* FEAT_PAuth */
    /** @todo */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Hints)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_0(Hints, 0xffffffff /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeNop, 0xfe0, 5);


/* CLREX */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(DecBarriers)
    DIS_ARMV8_OP(0xd503304f, "clrex",           OP_ARMV8_A64_CLREX,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xd50330bf, "dmb",             OP_ARMV8_A64_DMB,       DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(DecBarriers)
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            8,  4, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(DecBarriers, 0xfffff0ff /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(5), 5,
                                                kDisArmv8OpParmImm);


/* Barrier instructions, we divide these instructions further based on the op2 field. */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DecodeBarriers)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                     /** @todo DSB - Encoding */
    DIS_ARMV8_DECODE_MAP_ENTRY(DecBarriers),                /* CLREX */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                     /** @todo TCOMMIT */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                     /** @todo DSB - Encoding */
    DIS_ARMV8_DECODE_MAP_ENTRY(DecBarriers),                /* DMB */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                     /** @todo ISB */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY                      /** @todo SB */
DIS_ARMV8_DECODE_MAP_DEFINE_END(DecodeBarriers, RT_BIT_32(5) | RT_BIT_32(6) | RT_BIT_32(7), 5);


/* MSR (and potentially CFINV,XAFLAG,AXFLAG) */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(PState)
    DIS_ARMV8_OP(0xd503305f, "msr",             OP_ARMV8_A64_MSR,       DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(PState)
    DIS_ARMV8_INSN_DECODE(kDisParmParsePState,         0,  0, 0 /*idxParam*/), /* This is special for the MSR instruction. */
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            8,  4, 1 /*idxParam*/), /* CRm field encodes the immediate value */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(PState, 0xfffff0ff /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeNop, 0, 0,
                                                kDisArmv8OpParmImm, kDisArmv8OpParmNone); /** @todo */


/* TSTART/TTEST */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(SysResult)
    DIS_ARMV8_OP(0xd5233060, "tstart",          OP_ARMV8_A64_TSTART,    DISOPTYPE_HARMLESS | DISOPTYPE_PRIVILEGED),  /* FEAT_TME */
    DIS_ARMV8_OP(0xd5233160, "ttest",           OP_ARMV8_A64_TTEST,     DISOPTYPE_HARMLESS),                         /* FEAT_TME */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(SysResult)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(SysResult, 0xfffffffe /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(8) | RT_BIT_32(9) | RT_BIT_32(10) | RT_BIT_32(11), 8,
                                                kDisArmv8OpParmGpr);


/* SYS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Sys)
    DIS_ARMV8_OP(0xd5080000, "sys",             OP_ARMV8_A64_SYS,       DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Sys)
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,           16,  3, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseCRnCRm,         8,  8, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            5,  3, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_0(Sys, 0xfff80000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, 0, 0); /** @todo */


/* SYSL */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(SysL)
    DIS_ARMV8_OP(0xd5280000, "sysl",            OP_ARMV8_A64_SYSL,      DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(SysL)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,           16,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseCRnCRm,         8,  8, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImm,            5,  3, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_0(SysL, 0xfff80000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, 0, 0); /** @todo */


/* MSR */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Msr)
    DIS_ARMV8_OP(0xd5100000, "msr",             OP_ARMV8_A64_MSR,       DISOPTYPE_HARMLESS | DISOPTYPE_PRIVILEGED),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Msr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSysReg,         5, 15, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(Msr, 0xfff00000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, 0, 0,
                                                kDisArmv8OpParmSysReg, kDisArmv8OpParmGpr);


/* MRS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(Mrs)
    DIS_ARMV8_OP(0xd5300000, "mrs",             OP_ARMV8_A64_MRS,       DISOPTYPE_HARMLESS | DISOPTYPE_PRIVILEGED),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(Mrs)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSysReg,         5, 15, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(Mrs, 0xfff00000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeNop, 0, 0,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmSysReg);


/* BR/BRAA/BRAAZ/BRAB/BRABZ/BLR/BLRAA/BLRAAZ/BLRAB/BLRABZ/RET/RETAA/RETAB */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(BrBlrRet)
    DIS_ARMV8_OP(0xd61f0000, "br",             OP_ARMV8_A64_BR,         DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    DIS_ARMV8_OP(0xd63f0000, "blr",            OP_ARMV8_A64_BLR,        DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    /** @todo All the FEAT_PAuth related branch instructions. */
    DIS_ARMV8_OP(0xd65f0000, "ret",            OP_ARMV8_A64_RET,        DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    DIS_ARMV8_OP(0xd65f0800, "retaa",          OP_ARMV8_A64_RETAA,      DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    DIS_ARMV8_OP(0xd65f0c00, "retab",          OP_ARMV8_A64_RETAB,      DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(BrBlrRet)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(BrBlrRet, 0xfffffc1f /*fFixedInsn*/, DISARMV8INSNCLASS_F_FORCED_64BIT,
                                                kDisArmV8OpcDecodeLookup, 0xfffffc1f, 0,
                                                kDisArmv8OpParmGpr);


/* Unconditional branch (register) instructions, we divide these instructions further based on the opc field. */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(UncondBrReg)
    DIS_ARMV8_DECODE_MAP_ENTRY(BrBlrRet),    /* BR/BRAA/BRAAZ/BRAB/BRABZ */
    DIS_ARMV8_DECODE_MAP_ENTRY(BrBlrRet),    /* BLR/BLRAA/BLRAAZ/BLRAB/BLRABZ */
    DIS_ARMV8_DECODE_MAP_ENTRY(BrBlrRet),    /* RET/RETAA/RETAB */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY
DIS_ARMV8_DECODE_MAP_DEFINE_END(UncondBrReg, RT_BIT_32(21) | RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(24), 21);


/* B/BL */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(UncondBrImm)
    DIS_ARMV8_OP(0x14000000, "b",              OP_ARMV8_A64_B,         DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    DIS_ARMV8_OP(0x94000000, "bl",             OP_ARMV8_A64_BL,        DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(UncondBrImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,         0,  26, 0 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_1(UncondBrImm, 0xfc000000 /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(31), 31,
                                                kDisArmv8OpParmImmRel);


/* CBZ/CBNZ */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(CmpBrImm)
    DIS_ARMV8_OP(0x34000000, "cbz",             OP_ARMV8_A64_CBZ,       DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    DIS_ARMV8_OP(0x35000000, "cbnz",            OP_ARMV8_A64_CBNZ,      DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(CmpBrImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,         5, 19, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(CmpBrImm, 0x7f000000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(24), 24,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmImmRel);


/* TBZ/TBNZ */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(TestBrImm)
    DIS_ARMV8_OP(0x36000000, "tbz",             OP_ARMV8_A64_TBZ,       DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
    DIS_ARMV8_OP(0x37000000, "tbnz",            OP_ARMV8_A64_TBNZ,      DISOPTYPE_HARMLESS | DISOPTYPE_CONTROLFLOW),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(TestBrImm)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmTbz,         0,  0, 1 /*idxParam*/), /* Hardcoded bit offsets in parser. */
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,         5, 14, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_3(TestBrImm, 0x7f000000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF, /* Not an SF bit but has the same meaning. */
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(24), 24,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmImm, kDisArmv8OpParmImmRel);


DIS_ARMV8_DECODE_TBL_DEFINE_BEGIN(BrExcpSys)
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfe000000, RT_BIT_32(26) | RT_BIT_32(28) | RT_BIT_32(30),                  CondBr),          /* op0: 010, op1: 0xxxxxxxxxxxxx, op2: - (including o1 from the conditional branch (immediate) class to save us one layer). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xff000000, RT_BIT_32(26) | RT_BIT_32(28) | RT_BIT_32(30) | RT_BIT_32(31),  Excp),            /* op0: 110, op1: 00xxxxxxxxxxxx, op2: -. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff000, 0xd5031000,                                                     SysReg),          /* op0: 110, op1: 01000000110001, op2: -. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff01f, 0xd503201f,                                                     Hints),           /* op0: 110, op1: 01000000110010, op2: 11111. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff01f, 0xd503301f,                                                     DecodeBarriers),  /* op0: 110, op1: 01000000110011, op2: - (we include Rt:  11111 from the next stage here). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfff8f01f, 0xd500401f,                                                     PState),          /* op0: 110, op1: 0100000xxx0100, op2: - (we include Rt:  11111 from the next stage here). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfffff0e0, 0xd5233060,                                                     SysResult),       /* op0: 110, op1: 0100100xxxxxxx, op2: - (we include op1, CRn and op2 from the next stage here). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfff80000, 0xd5080000,                                                     Sys),             /* op0: 110, op1: 0100x01xxxxxxx, op2: - (we include the L field of the next stage here to differentiate between SYS/SYSL as they have a different string representation). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfff80000, 0xd5280000,                                                     SysL),            /* op0: 110, op1: 0100x01xxxxxxx, op2: - (we include the L field of the next stage here to differentiate between SYS/SYSL as they have a different string representation). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfff00000, 0xd5100000,                                                     Msr),             /* op0: 110, op1: 0100x1xxxxxxxx, op2: - (we include the L field of the next stage here to differentiate between MSR/MRS as they have a different string representation). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfff00000, 0xd5300000,                                                     Mrs),             /* op0: 110, op1: 0100x1xxxxxxxx, op2: - (we include the L field of the next stage here to differentiate between MSR/MRS as they have a different string representation). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0xfe1f0000, 0xd61f0000,                                                     UncondBrReg),     /* op0: 110, op1: 1xxxxxxxxxxxxx, op2: - (we include the op2 field from the next stage here as it should be always 11111). */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0x7c000000, 0x14000000,                                                     UncondBrImm),     /* op0: x00, op1: xxxxxxxxxxxxxx, op2: -. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0x7e000000, 0x34000000,                                                     CmpBrImm),        /* op0: x01, op1: 0xxxxxxxxxxxxx, op2: -. */
    DIS_ARMV8_DECODE_TBL_ENTRY_INIT(0x7e000000, 0x36000000,                                                     TestBrImm),       /* op0: x01, op1: 1xxxxxxxxxxxxx, op2: -. */
DIS_ARMV8_DECODE_TBL_DEFINE_END(BrExcpSys);


/* AND/ORR/EOR/ANDS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LogShiftRegN0)
    DIS_ARMV8_OP(0x0a000000, "and",             OP_ARMV8_A64_AND,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x2a000000, "orr",             OP_ARMV8_A64_ORR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x4a000000, "eor",             OP_ARMV8_A64_EOR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x6a000000, "ands",            OP_ARMV8_A64_ANDS,      DISOPTYPE_HARMLESS)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LogShiftRegN0)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,           16,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseShift,         22,  2, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseShiftAmount,   10,  6, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_3(LogShiftRegN0, 0x7f200000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmGpr, kDisArmv8OpParmGpr);


/* AND/ORR/EOR/ANDS */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LogShiftRegN1)
    DIS_ARMV8_OP(0x0a200000, "bic",             OP_ARMV8_A64_BIC,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x2a200000, "orn",             OP_ARMV8_A64_ORN,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x4a200000, "eon",             OP_ARMV8_A64_EON,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x6a200000, "bics",            OP_ARMV8_A64_BICS,      DISOPTYPE_HARMLESS)
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LogShiftRegN1)
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,           16,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseShift,         22,  2, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseShiftAmount,   10,  6, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_3(LogShiftRegN1, 0x7f200000 /*fFixedInsn*/, DISARMV8INSNCLASS_F_SF,
                                                kDisArmV8OpcDecodeNop, RT_BIT_32(29) | RT_BIT_32(30), 29,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmGpr, kDisArmv8OpParmGpr);


DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LogShiftRegN)
    DIS_ARMV8_DECODE_MAP_ENTRY(LogShiftRegN0),       /* Logical (shifted register) - N = 0 */
    DIS_ARMV8_DECODE_MAP_ENTRY(LogShiftRegN1),       /* Logical (shifted register) - N = 1 */
DIS_ARMV8_DECODE_MAP_DEFINE_END(LogShiftRegN, RT_BIT_32(21), 21);



DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(AddSubExtReg)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY, /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(AddSubExtReg, RT_BIT_32(24), 24);


DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(AddSubShiftExtReg)
    DIS_ARMV8_DECODE_MAP_ENTRY(AddSubShiftReg),      /* Add/Subtract (shifted register) */
    DIS_ARMV8_DECODE_MAP_ENTRY(AddSubExtReg),        /* Add/Subtract (extended register) */
DIS_ARMV8_DECODE_MAP_DEFINE_END(AddSubShiftExtReg, RT_BIT_32(21), 21);


DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LogicalAddSubReg)
    DIS_ARMV8_DECODE_MAP_ENTRY(LogShiftRegN),        /* Logical (shifted register) */
    DIS_ARMV8_DECODE_MAP_ENTRY(AddSubShiftExtReg),   /* Add/subtract (shifted/extended register) */
DIS_ARMV8_DECODE_MAP_DEFINE_END(LogicalAddSubReg, RT_BIT_32(24), 24);


DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DataProcReg)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,
DIS_ARMV8_DECODE_MAP_DEFINE_END(DataProcReg, RT_BIT_32(24), 24);


DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdSt)
    DIS_ARMV8_OP(0xb9400000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb9000000, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdSt)
    DIS_ARMV8_INSN_DECODE(kDisParmParseIs32Bit,       30,  1, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseReg,            5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmMemOff,     10, 12, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END_PARAMS_2(LdSt, 0xbfc00000 /*fFixedInsn*/, 0 /*fClass*/,
                                                kDisArmV8OpcDecodeLookup, 0xbfc00000, 0,
                                                kDisArmv8OpParmGpr, kDisArmv8OpParmAddrInGpr);

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
 *      5    0  1  0  1 Data processing - register (using op1 (bit 28) from the next stage to differentiate further already)
 *      6    0  1  1  0 Loads and stores
 *      7    0  1  1  1 Data processing - SIMD and floating point
 *      8    1  0  0  0 Data processing immediate
 *      9    1  0  0  1 Data processing immediate
 *     10    1  0  1  0 Branch, exception generation and system instructions
 *     11    1  0  1  1 Branch, exception generation and system instructions
 *     12    1  1  0  0 Loads and stores
 *     13    1  1  0  1 Data processing - register (using op1 (bit 28) from the next stage to differentiate further already)
 *     14    1  1  1  0 Loads and stores
 *     15    1  1  1  1 Data processing - SIMD and floating point
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(DecodeL0)
    DIS_ARMV8_DECODE_MAP_ENTRY(Rsvd),                               /* Reserved class or SME encoding (@todo). */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Unallocated */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /** @todo SVE */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Unallocated */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Load/Stores */
    DIS_ARMV8_DECODE_MAP_ENTRY(LogicalAddSubReg),                   /* Data processing (register) (see op1 in C4.1.68). */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Load/Stores */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Data processing (SIMD & FP) */
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcessingImm),                  /* Data processing (immediate). */
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcessingImm),                  /* Data processing (immediate). */
    DIS_ARMV8_DECODE_MAP_ENTRY(BrExcpSys),                          /* Branches / Exception generation and system instructions. */
    DIS_ARMV8_DECODE_MAP_ENTRY(BrExcpSys),                          /* Branches / Exception generation and system instructions. */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdSt),                               /* Load/Stores. */
    DIS_ARMV8_DECODE_MAP_ENTRY(DataProcReg),                        /* Data processing (register) (see op1 in C4.1.68). */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,                             /* Load/Stores. */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY                              /* Data processing (SIMD & FP). */
DIS_ARMV8_DECODE_MAP_DEFINE_END_NON_STATIC(DecodeL0, RT_BIT_32(25) | RT_BIT_32(26) | RT_BIT_32(27) | RT_BIT_32(28), 25);
