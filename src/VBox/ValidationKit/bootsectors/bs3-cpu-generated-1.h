/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-generated-1, common header file.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___bs3_cpu_generated_1_h___
#define ___bs3_cpu_generated_1_h___

#include <bs3kit.h>
#include <iprt/assert.h>


/**
 * Operand details.
 *
 * Currently simply using the encoding from the reference manuals.
 */
typedef enum BS3CG1OP
{
    BS3CG1OP_INVALID = 0,

    BS3CG1OP_Eb,
    BS3CG1OP_Ev,
    BS3CG1OP_Wss,
    BS3CG1OP_Wsd,
    BS3CG1OP_Wps,
    BS3CG1OP_Wpd,
    BS3CG1OP_Wdq,
    BS3CG1OP_WqZxReg,

    BS3CG1OP_Gb,
    BS3CG1OP_Gv,
    BS3CG1OP_Uq,
    BS3CG1OP_UqHi,
    BS3CG1OP_Vss,
    BS3CG1OP_Vsd,
    BS3CG1OP_Vps,
    BS3CG1OP_Vpd,
    BS3CG1OP_Vq,
    BS3CG1OP_Vdq,

    BS3CG1OP_Ib,
    BS3CG1OP_Iz,

    BS3CG1OP_AL,
    BS3CG1OP_rAX,

    BS3CG1OP_Ma,
    BS3CG1OP_MbRO,
    BS3CG1OP_Mq,

    BS3CG1OP_END
} BS3CG1OP;
/** Pointer to a const operand enum. */
typedef const BS3CG1OP BS3_FAR *PCBS3CG1OP;


/**
 * Instruction encoding format.
 *
 * This duplicates some of the info in the operand array, however it makes it
 * easier to figure out encoding variations.
 */
typedef enum BS3CG1ENC
{
    BS3CG1ENC_INVALID = 0,

    BS3CG1ENC_MODRM_Eb_Gb,
    BS3CG1ENC_MODRM_Ev_Gv,
    BS3CG1ENC_MODRM_Wss_Vss,
    BS3CG1ENC_MODRM_Wsd_Vsd,
    BS3CG1ENC_MODRM_Wps_Vps,
    BS3CG1ENC_MODRM_Wpd_Vpd,
    BS3CG1ENC_MODRM_WqZxReg_Vq,

    BS3CG1ENC_MODRM_Gb_Eb,
    BS3CG1ENC_MODRM_Gv_Ev,
    BS3CG1ENC_MODRM_Gv_Ma, /**< bound instruction */
    BS3CG1ENC_MODRM_Vq_UqHi,
    BS3CG1ENC_MODRM_Vq_Mq,
    BS3CG1ENC_MODRM_Vdq_Wdq,
    BS3CG1ENC_MODRM_MbRO,

    BS3CG1ENC_FIXED,
    BS3CG1ENC_FIXED_AL_Ib,
    BS3CG1ENC_FIXED_rAX_Iz,

    BS3CG1ENC_MODRM_MOD_EQ_3, /**< Unused or invalid instruction. */
    BS3CG1ENC_MODRM_MOD_NE_3, /**< Unused or invalid instruction. */

    BS3CG1ENC_END
} BS3CG1ENC;


/**
 * Prefix sensitivitiy kind.
 */
typedef enum BS3CG1PFXKIND
{
    BS3CG1PFXKIND_INVALID = 0,

    BS3CG1PFXKIND_NO_F2_F3_66,           /**< No 66, F2 or F3 prefixes allowed as that would alter the meaning. */
    BS3CG1PFXKIND_REQ_F2,                /**< Requires F2 (REPNE) prefix as part of the instr encoding. */
    BS3CG1PFXKIND_REQ_F3,                /**< Requires F3 (REPE) prefix as part of the instr encoding. */
    BS3CG1PFXKIND_REQ_66,                /**< Requires 66 (OP SIZE) prefix as part of the instr encoding.  */

    /** @todo more work to be done here...   */
    BS3CG1PFXKIND_MODRM,
    BS3CG1PFXKIND_MODRM_NO_OP_SIZES,

    BS3CG1PFXKIND_END
} BS3CG1PFXKIND;

/**
 * CPU selection or CPU ID.
 */
typedef enum BS3CG1CPU
{
    /** Works with an CPU. */
    BS3CG1CPU_ANY = 0,
    BS3CG1CPU_GE_80186,
    BS3CG1CPU_GE_80286,
    BS3CG1CPU_GE_80386,
    BS3CG1CPU_GE_80486,
    BS3CG1CPU_GE_Pentium,

    BS3CG1CPU_SSE,
    BS3CG1CPU_SSE2,
    BS3CG1CPU_SSE3,
    BS3CG1CPU_AVX,
    BS3CG1CPU_AVX2,
    BS3CG1CPU_CLFSH,

    BS3CG1CPU_END
} BS3CG1CPU;


/**
 * SSE & AVX exception types.
 */
typedef enum BS3CG1XCPTTYPE
{
    BS3CG1XCPTTYPE_NONE = 0,
    /* SSE: */
    BS3CG1XCPTTYPE_1,
    BS3CG1XCPTTYPE_2,
    BS3CG1XCPTTYPE_3,
    BS3CG1XCPTTYPE_4,
    BS3CG1XCPTTYPE_4UA,
    BS3CG1XCPTTYPE_5,
    BS3CG1XCPTTYPE_6,
    BS3CG1XCPTTYPE_7,
    BS3CG1XCPTTYPE_8,
    BS3CG1XCPTTYPE_11,
    BS3CG1XCPTTYPE_12,
    /* EVEX: */
    BS3CG1XCPTTYPE_E1,
    BS3CG1XCPTTYPE_E1NF,
    BS3CG1XCPTTYPE_E2,
    BS3CG1XCPTTYPE_E3,
    BS3CG1XCPTTYPE_E3NF,
    BS3CG1XCPTTYPE_E4,
    BS3CG1XCPTTYPE_E4NF,
    BS3CG1XCPTTYPE_E5,
    BS3CG1XCPTTYPE_E5NF,
    BS3CG1XCPTTYPE_E6,
    BS3CG1XCPTTYPE_E6NF,
    BS3CG1XCPTTYPE_E7NF,
    BS3CG1XCPTTYPE_E9,
    BS3CG1XCPTTYPE_E9NF,
    BS3CG1XCPTTYPE_E10,
    BS3CG1XCPTTYPE_E11,
    BS3CG1XCPTTYPE_E12,
    BS3CG1XCPTTYPE_E12NF,
    BS3CG1XCPTTYPE_END
} BS3CG1XCPTTYPE;
AssertCompile(BS3CG1XCPTTYPE_END <= 32);


/**
 * Generated instruction info.
 */
typedef struct BS3CG1INSTR
{
    /** The opcode size.   */
    uint32_t    cbOpcodes : 2;
    /** The number of operands.   */
    uint32_t    cOperands : 2;
    /** The length of the mnemonic. */
    uint32_t    cchMnemonic : 4;
    /** Whether to advance the mnemonic array pointer. */
    uint32_t    fAdvanceMnemonic : 1;
    /** Offset into g_abBs3Cg1Tests of the first test. */
    uint32_t    offTests : 23;
    /** BS3CG1ENC values. */
    uint32_t    enmEncoding : 10;
    /** BS3CG1PFXKIND values. */
    uint32_t    enmPrefixKind : 4;
    /** CPU test / CPU ID bit test (BS3CG1CPU). */
    uint32_t    enmCpuTest : 6;
    /** Exception type (BS3CG1XCPTTYPE)   */
    uint32_t    enmXcptType : 5;
    /** Currently unused bits. */
    uint32_t    uUnused : 6;
    /** BS3CG1INSTR_F_XXX. */
    uint32_t    fFlags;
} BS3CG1INSTR;
AssertCompileSize(BS3CG1INSTR, 12);
/** Pointer to a const instruction. */
typedef BS3CG1INSTR const BS3_FAR *PCBS3CG1INSTR;


/** @name BS3CG1INSTR_F_XXX
 * @{ */
/** Defaults to SS rather than DS. */
#define BS3CG1INSTR_F_DEF_SS            UINT32_C(0x00000001)
/** Invalid instruction in 64-bit mode. */
#define BS3CG1INSTR_F_INVALID_64BIT     UINT32_C(0x00000002)
/** Unused instruction. */
#define BS3CG1INSTR_F_UNUSED            UINT32_C(0x00000004)
/** @} */


/**
 * Test header.
 */
typedef struct BS3CG1TESTHDR
{
    /** The size of the selector program in bytes.
     * This is also the offset of the input context modification program.  */
    uint32_t    cbSelector : 8;
    /** The size of the input context modification program in bytes.
     * This immediately follows the selector program.  */
    uint32_t    cbInput    : 12;
    /** The size of the output context modification program in bytes.
     * This immediately follows the input context modification program.  The
     * program takes the result of the input program as starting point. */
    uint32_t    cbOutput   : 11;
    /** Indicates whether this is the last test or not. */
    uint32_t    fLast      : 1;
} BS3CG1TESTHDR;
AssertCompileSize(BS3CG1TESTHDR, 4);
/** Pointer to a const test header. */
typedef BS3CG1TESTHDR const BS3_FAR *PCBS3CG1TESTHDR;

/** @name Opcode format for the BS3CG1 context modifier.
 *
 * Used by both the input and output context programs.
 *
 * The most common operations are encoded as a single byte opcode followed by
 * one or more immediate bytes with data.
 *
 * @{ */
#define BS3CG1_CTXOP_SIZE_MASK      UINT8_C(0x07)
#define BS3CG1_CTXOP_1_BYTE         UINT8_C(0x00)
#define BS3CG1_CTXOP_2_BYTES        UINT8_C(0x01)
#define BS3CG1_CTXOP_4_BYTES        UINT8_C(0x02)
#define BS3CG1_CTXOP_8_BYTES        UINT8_C(0x03)
#define BS3CG1_CTXOP_16_BYTES       UINT8_C(0x04)
#define BS3CG1_CTXOP_32_BYTES       UINT8_C(0x05)
#define BS3CG1_CTXOP_12_BYTES       UINT8_C(0x06)
#define BS3CG1_CTXOP_SIZE_ESC       UINT8_C(0x07)   /**< Separate byte encoding the value size following any destination escape byte. */

#define BS3CG1_CTXOP_DST_MASK       UINT8_C(0x18)
#define BS3CG1_CTXOP_OP1            UINT8_C(0x00)
#define BS3CG1_CTXOP_OP2            UINT8_C(0x08)
#define BS3CG1_CTXOP_EFL            UINT8_C(0x10)
#define BS3CG1_CTXOP_DST_ESC        UINT8_C(0x18)   /**< Separate byte giving the destination follows immediately. */

#define BS3CG1_CTXOP_SIGN_EXT       UINT8_C(0x20)   /**< Whether to sign-extend (set) the immediate value. */

#define BS3CG1_CTXOP_OPERATOR_MASK  UINT8_C(0xc0)
#define BS3CG1_CTXOP_ASSIGN         UINT8_C(0x00)   /**< Simple assignment operator (=) */
#define BS3CG1_CTXOP_OR             UINT8_C(0x40)   /**< OR assignment operator (|=). */
#define BS3CG1_CTXOP_AND            UINT8_C(0x80)   /**< AND assignment operator (&=). */
#define BS3CG1_CTXOP_AND_INV        UINT8_C(0xc0)   /**< AND assignment operator of the inverted value (&~=). */
/** @} */

/**
 * Escaped destination values
 *
 * These are just uppercased versions of TestInOut.kdFields, where dots are
 * replaced by underscores.
 */
typedef enum BS3CG1DST
{
    BS3CG1DST_INVALID = 0,
    /* Operands. */
    BS3CG1DST_OP1,
    BS3CG1DST_OP2,
    BS3CG1DST_OP3,
    BS3CG1DST_OP4,
    /* Flags. */
    BS3CG1DST_EFL,
    BS3CG1DST_EFL_UNDEF, /**< Special field only valid in output context modifiers: EFLAGS |= Value & Ouput.EFLAGS; */
    /* 8-bit GPRs. */
    BS3CG1DST_AL,
    BS3CG1DST_CL,
    BS3CG1DST_DL,
    BS3CG1DST_BL,
    BS3CG1DST_AH,
    BS3CG1DST_CH,
    BS3CG1DST_DH,
    BS3CG1DST_BH,
    BS3CG1DST_SPL,
    BS3CG1DST_BPL,
    BS3CG1DST_SIL,
    BS3CG1DST_DIL,
    BS3CG1DST_R8L,
    BS3CG1DST_R9L,
    BS3CG1DST_R10L,
    BS3CG1DST_R11L,
    BS3CG1DST_R12L,
    BS3CG1DST_R13L,
    BS3CG1DST_R14L,
    BS3CG1DST_R15L,
    /* 16-bit GPRs. */
    BS3CG1DST_AX,
    BS3CG1DST_CX,
    BS3CG1DST_DX,
    BS3CG1DST_BX,
    BS3CG1DST_SP,
    BS3CG1DST_BP,
    BS3CG1DST_SI,
    BS3CG1DST_DI,
    BS3CG1DST_R8W,
    BS3CG1DST_R9W,
    BS3CG1DST_R10W,
    BS3CG1DST_R11W,
    BS3CG1DST_R12W,
    BS3CG1DST_R13W,
    BS3CG1DST_R14W,
    BS3CG1DST_R15W,
    /* 32-bit GPRs. */
    BS3CG1DST_EAX,
    BS3CG1DST_ECX,
    BS3CG1DST_EDX,
    BS3CG1DST_EBX,
    BS3CG1DST_ESP,
    BS3CG1DST_EBP,
    BS3CG1DST_ESI,
    BS3CG1DST_EDI,
    BS3CG1DST_R8D,
    BS3CG1DST_R9D,
    BS3CG1DST_R10D,
    BS3CG1DST_R11D,
    BS3CG1DST_R12D,
    BS3CG1DST_R13D,
    BS3CG1DST_R14D,
    BS3CG1DST_R15D,
    /* 64-bit GPRs. */
    BS3CG1DST_RAX,
    BS3CG1DST_RCX,
    BS3CG1DST_RDX,
    BS3CG1DST_RBX,
    BS3CG1DST_RSP,
    BS3CG1DST_RBP,
    BS3CG1DST_RSI,
    BS3CG1DST_RDI,
    BS3CG1DST_R8,
    BS3CG1DST_R9,
    BS3CG1DST_R10,
    BS3CG1DST_R11,
    BS3CG1DST_R12,
    BS3CG1DST_R13,
    BS3CG1DST_R14,
    BS3CG1DST_R15,
    /* 16-bit, 32-bit or 64-bit registers according to operand size. */
    BS3CG1DST_OZ_RAX,
    BS3CG1DST_OZ_RCX,
    BS3CG1DST_OZ_RDX,
    BS3CG1DST_OZ_RBX,
    BS3CG1DST_OZ_RSP,
    BS3CG1DST_OZ_RBP,
    BS3CG1DST_OZ_RSI,
    BS3CG1DST_OZ_RDI,
    BS3CG1DST_OZ_R8,
    BS3CG1DST_OZ_R9,
    BS3CG1DST_OZ_R10,
    BS3CG1DST_OZ_R11,
    BS3CG1DST_OZ_R12,
    BS3CG1DST_OZ_R13,
    BS3CG1DST_OZ_R14,
    BS3CG1DST_OZ_R15,

    /* FPU registers. */
    BS3CG1DST_FPU_FIRST,
    BS3CG1DST_FCW = BS3CG1DST_FPU_FIRST,
    BS3CG1DST_FSW,
    BS3CG1DST_FTW,
    BS3CG1DST_FOP,
    BS3CG1DST_FPUIP,
    BS3CG1DST_FPUCS,
    BS3CG1DST_FPUDP,
    BS3CG1DST_FPUDS,
    BS3CG1DST_MXCSR,
    BS3CG1DST_MXCSR_MASK,
    BS3CG1DST_ST0,
    BS3CG1DST_ST1,
    BS3CG1DST_ST2,
    BS3CG1DST_ST3,
    BS3CG1DST_ST4,
    BS3CG1DST_ST5,
    BS3CG1DST_ST6,
    BS3CG1DST_ST7,
    /* MMX registers. */
    BS3CG1DST_MM0,
    BS3CG1DST_MM1,
    BS3CG1DST_MM2,
    BS3CG1DST_MM3,
    BS3CG1DST_MM4,
    BS3CG1DST_MM5,
    BS3CG1DST_MM6,
    BS3CG1DST_MM7,
    /* SSE registers. */
    BS3CG1DST_XMM0,
    BS3CG1DST_XMM1,
    BS3CG1DST_XMM2,
    BS3CG1DST_XMM3,
    BS3CG1DST_XMM4,
    BS3CG1DST_XMM5,
    BS3CG1DST_XMM6,
    BS3CG1DST_XMM7,
    BS3CG1DST_XMM8,
    BS3CG1DST_XMM9,
    BS3CG1DST_XMM10,
    BS3CG1DST_XMM11,
    BS3CG1DST_XMM12,
    BS3CG1DST_XMM13,
    BS3CG1DST_XMM14,
    BS3CG1DST_XMM15,
    BS3CG1DST_XMM0_LO,
    BS3CG1DST_XMM1_LO,
    BS3CG1DST_XMM2_LO,
    BS3CG1DST_XMM3_LO,
    BS3CG1DST_XMM4_LO,
    BS3CG1DST_XMM5_LO,
    BS3CG1DST_XMM6_LO,
    BS3CG1DST_XMM7_LO,
    BS3CG1DST_XMM8_LO,
    BS3CG1DST_XMM9_LO,
    BS3CG1DST_XMM10_LO,
    BS3CG1DST_XMM11_LO,
    BS3CG1DST_XMM12_LO,
    BS3CG1DST_XMM13_LO,
    BS3CG1DST_XMM14_LO,
    BS3CG1DST_XMM15_LO,
    BS3CG1DST_XMM0_HI,
    BS3CG1DST_XMM1_HI,
    BS3CG1DST_XMM2_HI,
    BS3CG1DST_XMM3_HI,
    BS3CG1DST_XMM4_HI,
    BS3CG1DST_XMM5_HI,
    BS3CG1DST_XMM6_HI,
    BS3CG1DST_XMM7_HI,
    BS3CG1DST_XMM8_HI,
    BS3CG1DST_XMM9_HI,
    BS3CG1DST_XMM10_HI,
    BS3CG1DST_XMM11_HI,
    BS3CG1DST_XMM12_HI,
    BS3CG1DST_XMM13_HI,
    BS3CG1DST_XMM14_HI,
    BS3CG1DST_XMM15_HI,
    BS3CG1DST_XMM0_LO_ZX,
    BS3CG1DST_XMM1_LO_ZX,
    BS3CG1DST_XMM2_LO_ZX,
    BS3CG1DST_XMM3_LO_ZX,
    BS3CG1DST_XMM4_LO_ZX,
    BS3CG1DST_XMM5_LO_ZX,
    BS3CG1DST_XMM6_LO_ZX,
    BS3CG1DST_XMM7_LO_ZX,
    BS3CG1DST_XMM8_LO_ZX,
    BS3CG1DST_XMM9_LO_ZX,
    BS3CG1DST_XMM10_LO_ZX,
    BS3CG1DST_XMM11_LO_ZX,
    BS3CG1DST_XMM12_LO_ZX,
    BS3CG1DST_XMM13_LO_ZX,
    BS3CG1DST_XMM14_LO_ZX,
    BS3CG1DST_XMM15_LO_ZX,
    BS3CG1DST_XMM0_DW0,
    BS3CG1DST_XMM1_DW0,
    BS3CG1DST_XMM2_DW0,
    BS3CG1DST_XMM3_DW0,
    BS3CG1DST_XMM4_DW0,
    BS3CG1DST_XMM5_DW0,
    BS3CG1DST_XMM6_DW0,
    BS3CG1DST_XMM7_DW0,
    BS3CG1DST_XMM8_DW0,
    BS3CG1DST_XMM9_DW0,
    BS3CG1DST_XMM10_DW0,
    BS3CG1DST_XMM11_DW0,
    BS3CG1DST_XMM12_DW0,
    BS3CG1DST_XMM13_DW0,
    BS3CG1DST_XMM14_DW0,
    BS3CG1DST_XMM15_DW0,
    /* AVX registers. */
    BS3CG1DST_YMM0,
    BS3CG1DST_YMM1,
    BS3CG1DST_YMM2,
    BS3CG1DST_YMM3,
    BS3CG1DST_YMM4,
    BS3CG1DST_YMM5,
    BS3CG1DST_YMM6,
    BS3CG1DST_YMM7,
    BS3CG1DST_YMM8,
    BS3CG1DST_YMM9,
    BS3CG1DST_YMM10,
    BS3CG1DST_YMM11,
    BS3CG1DST_YMM12,
    BS3CG1DST_YMM13,
    BS3CG1DST_YMM14,
    BS3CG1DST_YMM15,

    /* Special fields: */
    BS3CG1DST_SPECIAL_START,
    BS3CG1DST_VALUE_XCPT = BS3CG1DST_SPECIAL_START, /**< Expected exception based on input or result. */

    BS3CG1DST_END
} BS3CG1DST;
AssertCompile(BS3CG1DST_END <= 256);

/** @name Selector opcode definitions.
 *
 * Selector programs are very simple, they are zero or more predicate tests
 * that are ANDed together.  If a predicate test fails, the test is skipped.
 *
 * One instruction is encoded as byte, where the first bit indicates what kind
 * of test and the 7 remaining bits indicates which predicate to check.
 *
 * @{ */
#define BS3CG1SEL_OP_KIND_MASK  UINT8_C(0x01)   /**< The operator part (put in lower bit to reduce switch value range). */
#define BS3CG1SEL_OP_IS_TRUE    UINT8_C(0x00)   /**< Check that the predicate is true. */
#define BS3CG1SEL_OP_IS_FALSE   UINT8_C(0x01)   /**< Check that the predicate is false. */
#define BS3CG1SEL_OP_PRED_SHIFT 1               /**< Shift factor for getting/putting a BS3CG1PRED value into/from a byte. */
/** @} */

/**
 * Test selector predicates (values are shifted by BS3CG1SEL_OP_PRED_SHIFT).
 */
typedef enum BS3CG1PRED
{
     BS3CG1PRED_INVALID = 0,

     /* Operand size. */
     BS3CG1PRED_SIZE_O16,
     BS3CG1PRED_SIZE_O32,
     BS3CG1PRED_SIZE_O64,
     /* Execution ring. */
     BS3CG1PRED_RING_0,
     BS3CG1PRED_RING_1,
     BS3CG1PRED_RING_2,
     BS3CG1PRED_RING_3,
     BS3CG1PRED_RING_0_THRU_2,
     BS3CG1PRED_RING_1_THRU_3,
     /* Basic code mode. */
     BS3CG1PRED_CODE_64BIT,
     BS3CG1PRED_CODE_32BIT,
     BS3CG1PRED_CODE_16BIT,
     /* CPU modes. */
     BS3CG1PRED_MODE_REAL,
     BS3CG1PRED_MODE_PROT,
     BS3CG1PRED_MODE_LONG,
     BS3CG1PRED_MODE_V86,
     BS3CG1PRED_MODE_SMM,
     BS3CG1PRED_MODE_VMX,
     BS3CG1PRED_MODE_SVM,
     /* Paging on/off */
     BS3CG1PRED_PAGING_ON,
     BS3CG1PRED_PAGING_OFF,

     BS3CG1PRED_END
} BS3CG1PRED;


/** The test instructions (generated). */
extern const BS3CG1INSTR BS3_FAR_DATA   g_aBs3Cg1Instructions[];
/** The number of test instructions (generated). */
extern const uint16_t BS3_FAR_DATA      g_cBs3Cg1Instructions;
/** The mnemonics (generated).
 * Variable length sequence of mnemonics that runs in parallel to
 * g_aBs3Cg1Instructions. */
extern const char BS3_FAR_DATA          g_achBs3Cg1Mnemonics[];
/** The opcodes (generated).
 * Variable length sequence of opcode bytes that runs in parallel to
 * g_aBs3Cg1Instructions, advancing by BS3CG1INSTR::cbOpcodes each time. */
extern const uint8_t BS3_FAR_DATA       g_abBs3Cg1Opcodes[];
/** The operands (generated).
 * Variable length sequence of opcode values (BS3CG1OP) that runs in
 * parallel to g_aBs3Cg1Instructions, advancing by BS3CG1INSTR::cOperands. */
extern const uint8_t BS3_FAR_DATA       g_abBs3Cg1Operands[];
/** The test data that BS3CG1INSTR.
 * In order to simplify generating these, we use a byte array. */
extern const uint8_t BS3_FAR_DATA       g_abBs3Cg1Tests[];


#endif

