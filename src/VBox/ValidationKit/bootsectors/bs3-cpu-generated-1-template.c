/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-generated-1, C code template.
 */

/*
 * Copyright (C) 2007-2017 Oracle Corporation
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

#ifndef BS3_INSTANTIATING_CMN
# error "BS3_INSTANTIATING_CMN not defined"
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>

#include "bs3-cpu-generated-1.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The state.
 */
typedef struct BS3CG1STATE
{
    /** @name Instruction details (expanded from BS3CG1INSTR).
     * @{ */
    /** Pointer to the mnemonic string (not terminated) (g_achBs3Cg1Mnemonics). */
    const char BS3_FAR     *pchMnemonic;
    /** Pointer to the test header. */
    PCBS3CG1TESTHDR         pTestHdr;
    /** Pointer to the per operand flags (g_abBs3Cg1Operands). */
    const uint8_t BS3_FAR  *pabOperands;
    /** Opcode bytes (g_abBs3Cg1Opcodes). */
    const uint8_t BS3_FAR  *pabOpcodes;

    /** The instruction flags. */
    uint32_t                fFlags;
    /** The encoding. */
    BS3CG1ENC               enmEncoding;
#if ARCH_BITS == 16
    uint16_t                u16Padding0;
#endif
    /** Per operand flags. */
    BS3CG1OP                aenmOperands[4];
    /** Opcode bytes. */
    uint8_t                 abOpcodes[4];

    /** The length of the mnemonic. */
    uint8_t                 cchMnemonic;
    /** Whether to advance the mnemonic pointer or not. */
    uint8_t                 fAdvanceMnemonic;
    /** The number of opcode bytes.   */
    uint8_t                 cbOpcode;
    /** Number of operands. */
    uint8_t                 cOperands;
    /** @} */

    /** Operand size (16, 32, 64, or 0). */
    uint8_t                 cBitsOp;
    /** Target ring (0..3). */
    uint8_t                 uCpl;

    /** Target mode (g_bBs3CurrentMode).  */
    uint8_t                 bMode;

    uint8_t                 abPadding1[2];

    /** Page to put code in.  When paging is enabled, the page before and after
     * are marked not-present. */
    uint8_t BS3_FAR        *pbCodePg;
    /** Page for placing data operands in.  When paging is enabled, the page before
     * and after are marked not-present.  */
    uint8_t BS3_FAR        *pbDataPg;

    /** The context we're working on. */
    BS3REGCTX               Ctx;
    /** The trap context and frame. */
    BS3TRAPFRAME            TrapFrame;
    /** Initial contexts, one for each ring. */
    BS3REGCTX               aInitialCtxs[4];

    /** Memory operand scratch space. */
    union
    {
        uint8_t             ab[128];
        uint16_t            au16[128 / sizeof(uint16_t)];
        uint32_t            au32[128 / sizeof(uint32_t)];
        uint64_t            au64[128 / sizeof(uint64_t)];
    } MemOp;
} BS3CG1STATE;



BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr2_mul)(uint8_t bMode)
{
    BS3CG1STATE         This;
    unsigned const      iFirstRing = BS3_MODE_IS_V86(bMode)       ? 3 : 0;
    unsigned const      cRings     = BS3_MODE_IS_RM_OR_V86(bMode) ? 1 : 4;
    unsigned            iRing;
    unsigned            iInstr;
    BS3MEMKIND const    enmMemKind = BS3_MODE_IS_RM_OR_V86(bMode) ? BS3MEMKIND_REAL
                                   : BS3_MODE_IS_16BIT_CODE(bMode) ? BS3MEMKIND_TILED : BS3MEMKIND_FLAT32;

    /*
     * Initalize the state.
     */
    Bs3MemSet(&This, 0, sizeof(This));

    This.bMode          = bMode;
    This.pchMnemonic    = g_achBs3Cg1Mnemonics;
    This.pabOperands    = g_abBs3Cg1Operands;
    This.pabOpcodes     = g_abBs3Cg1Opcodes;

    /* Allocate guarded exectuable and data memory. */
    if (BS3_MODE_IS_PAGED(bMode))
    {
        This.pbCodePg = Bs3MemGuardedTestPageAlloc(enmMemKind);
        if (This.pbCodePg)
        {
            Bs3TestFailedF("First Bs3MemGuardedTestPageAlloc(%d) failed", enmMemKind);
            return 0;
        }
        This.pbDataPg = Bs3MemGuardedTestPageAlloc(enmMemKind);
        if (!This.pbDataPg)
        {
            Bs3MemGuardedTestPageFree(This.pbCodePg);
            Bs3TestFailedF("Second Bs3MemGuardedTestPageAlloc(%d) failed", enmMemKind);
            return 0;
        }
    }
    else
    {
        This.pbCodePg = Bs3MemAlloc(enmMemKind, X86_PAGE_SIZE);
        if (!This.pbCodePg)
        {
            Bs3TestFailedF("First Bs3MemAlloc(%d,Pg) failed", enmMemKind);
            return 0;
        }
        This.pbDataPg = Bs3MemAlloc(enmMemKind, X86_PAGE_SIZE);
        if (!This.pbDataPg)
        {
            Bs3MemFree(This.pbCodePg, X86_PAGE_SIZE);
            Bs3TestFailedF("Second Bs3MemAlloc(%d,Pg) failed", enmMemKind);
            return 0;
        }
    }

    Bs3RegCtxSaveEx(&This.aInitialCtxs[iFirstRing], bMode, 512);
    for (iRing = iFirstRing + 1; iRing < cRings; iRing++)
        Bs3RegCtxConvertToRingX(&This.aInitialCtxs[iFirstRing], iRing);

    /*
     * Test the instructions.
     */
    for (iInstr = 0; iInstr < g_cBs3Cg1Instructions;
         iInstr++,
         This.pchMnemonic += This.fAdvanceMnemonic * This.cchMnemonic,
         This.pabOperands += This.cOperands,
         This.pabOpcodes  += This.cbOpcode )
    {
        unsigned iEncoding;

        /*
         * Expand the instruction information into the state.
         * Note! 16-bit will switch to a two level test header lookup once we exceed 64KB.
         */
        PCBS3CG1INSTR pInstr = &g_aBs3Cg1Instructions[iInstr];
        This.pTestHdr        = (PCBS3CG1TESTHDR)&g_abBs3Cg1Tests[pInstr->offTests];
        This.fFlags          = pInstr->fFlags;
        This.enmEncoding     = (BS3CG1ENC)pInstr->enmEncoding;
        This.cchMnemonic     = pInstr->cchMnemonic;
        This.fAdvanceMnemonic= pInstr->fAdvanceMnemonic;
        This.cOperands       = pInstr->cOperands;
        This.cbOpcode        = pInstr->cbOpcode;
        switch (This.cOperands)
        {
            case 3: This.aenmOperands[3] = (BS3CG1OP)This.pabOperands[3];
            case 2: This.aenmOperands[2] = (BS3CG1OP)This.pabOperands[2];
            case 1: This.aenmOperands[1] = (BS3CG1OP)This.pabOperands[1];
            case 0: This.aenmOperands[0] = (BS3CG1OP)This.pabOperands[0];
        }

        switch (This.cbOpcode)
        {
            case 3: This.abOpcodes[3] = This.pabOpcodes[3];
            case 2: This.abOpcodes[2] = This.pabOpcodes[2];
            case 1: This.abOpcodes[1] = This.pabOpcodes[1];
            case 0: This.abOpcodes[0] = This.pabOpcodes[0];
        }

        if (This.enmEncoding <= BS3CG1ENC_INVALID || This.enmEncoding >= BS3CG1ENC_END)
        {
            Bs3TestFailedF("Invalid enmEncoding for instruction #%u (%.*s): %d",
                           iInstr, This.cchMnemonic, This.pchMnemonic, This.enmEncoding);
            continue;
        }

        /*
         * Encode the instruction in various ways and check out the test values.
         */
        for (iEncoding = 0; ; iEncoding++)
        {
            //switch (This.enmEncoding)
            //{
            //
            //}


            /*
             * Run the tests.
             */

        }

    }

    return 0;
}

