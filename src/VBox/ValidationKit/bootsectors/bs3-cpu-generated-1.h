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



/** Instruction encoding format. */
typedef enum BS3CG1ENC
{
    BS3CG1ENC_INVALID = 0,
    BS3CG1ENC_FIXED,
    BS3CG1ENC_MODRM,
    BS3CG1ENC_END
} BS3CG1ENC;


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

    BS3CG1OP_Gb,
    BS3CG1OP_Gv,

    BS3CG1OP_END
} BS3CG1OP;


/**
 * Generated instruction info.
 */
typedef struct BS3CG1INSTR
{
    /** Opcode bytes. */
    uint8_t     abOpcode[4];
    /** BS3CG1OP values for each operand. */
    uint8_t     aenmOperands[4];
    /** The opcode size.   */
    uint32_t    cbOpcode : 2;
    /** The number of operands.   */
    uint32_t    cOperands : 2;
    /** BS3CG1ENC values. */
    uint32_t    enmEncoding : 5;
    /** The length of the mnemonic. */
    uint32_t    cchMnemonic : 4;
    /** Index of the test header into g_aBs3Cg1Tests */
    uint32_t    idxTestHdr : 19;
    /** BS3CG1INSTR_F_XXX. */
    uint32_t    fFlags;
} BS3CG1INSTR;
AssertCompileSize(BS3CG1INSTR, 16);
/** Pointer to a const test. */
typedef BS3CG1INSTR const BS3_FAR *PCBS3CG1INSTR;


/** @name BS3CG1INSTR_F_XXX
 * @{ */
#define BS3CG1INSTR_F_          UINT32_C(0x00000000)
/** @} */


typedef struct BS3CG1TESTHDR
{
    /** Number of tests left for this instruction. */
    uint16_t    cLeft;
    /** The size of this test. */
    uint16_t    cbTest;
} BS3CG1TESTHDR;



/** The number of test instructions (generated). */
extern uint16_t BS3_FAR_DATA        g_cBs3Cg1Instructions;
/** The test instructions (generated). */
extern const char BS3_FAR_DATA      g_aBs3Cg1Instructions[];
/** The test data that BS3CG1INSTR. */
extern BS3CG1TESTHDR BS3_FAR_DATA   g_aBs3Cg1Tests[];
/** The mnemonics (generated).
 * Variable length sequence of mnemonics that runs in parallel to
 * g_aBs3Cg1Instructions. */
extern const char BS3_FAR_DATA      g_achBs3Cg1Mnemonics[];


#endif

