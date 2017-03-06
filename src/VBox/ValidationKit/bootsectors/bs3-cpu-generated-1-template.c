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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define P_CS        X86_OP_PRF_CS
#define P_SS        X86_OP_PRF_SS
#define P_DS        X86_OP_PRF_DS
#define P_ES        X86_OP_PRF_ES
#define P_FS        X86_OP_PRF_FS
#define P_GS        X86_OP_PRF_GS
#define P_OZ        X86_OP_PRF_SIZE_OP
#define P_AZ        X86_OP_PRF_SIZE_ADDR
#define P_LK        X86_OP_PRF_LOCK
#define P_RN        X86_OP_PRF_REPNZ
#define P_RZ        X86_OP_PRF_REPZ

#define REX_WRBX    (X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_B | X86_OP_REX_X)
#define REX_W___    (X86_OP_REX_W)
#define REX_WR__    (X86_OP_REX_W | X86_OP_REX_R)
#define REX_W_B_    (X86_OP_REX_W | X86_OP_REX_B)
#define REX_W__X    (X86_OP_REX_W | X86_OP_REX_X)
#define REX_WRB_    (X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_B)
#define REX_WR_X    (X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_X)
#define REX_W_BX    (X86_OP_REX_W | X86_OP_REX_B | X86_OP_REX_X)
#define REX__R__    (X86_OP_REX_R)
#define REX__RB_    (X86_OP_REX_R | X86_OP_REX_B)
#define REX__R_X    (X86_OP_REX_R | X86_OP_REX_X)
#define REX__RBX    (X86_OP_REX_R | X86_OP_REX_B | X86_OP_REX_X)
#define REX___B_    (X86_OP_REX_B)
#define REX___BX    (X86_OP_REX_B | X86_OP_REX_X)
#define REX____X    (X86_OP_REX_X)
#define REX_____    (0x40)


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
    uint8_t                 cbOpcodes;
    /** Number of operands. */
    uint8_t                 cOperands;
    /** @} */

    /** Operand size (16, 32, 64, or 0). */
    uint8_t                 cBitsOp;
    /** Target ring (0..3). */
    uint8_t                 uCpl;

    /** Target mode (g_bBs3CurrentMode).  */
    uint8_t                 bMode;

    /** @name Current encoded instruction.
     * @{ */
    /** The size of the current instruction that we're testing. */
    uint8_t                 cbCurInstr;
    /** The size the prefixes. */
    uint8_t                 cbCurPrefix;
    /** The offset into abCurInstr of the immediate. */
    uint8_t                 offCurImm;
    /** Buffer for assembling the current instruction. */
    uint8_t                 abCurInstr[31];

    /** Operands details. */
    struct
    {
        uint8_t             cbOp;
        bool                fMem;
        bool                afUnused[2];
        BS3PTRUNION         uOpPtr;
    } aOperands[4];
    /** @} */

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
/** Pointer to the generated test state. */
typedef BS3CG1STATE *PBS3CG1STATE;


#define BS3CG1_PF_OZ  UINT16_C(0x0001)
#define BS3CG1_PF_AZ  UINT16_C(0x0002)
#define BS3CG1_PF_CS  UINT16_C(0x0004)
#define BS3CG1_PF_DS  UINT16_C(0x0008)
#define BS3CG1_PF_ES  UINT16_C(0x0010)
#define BS3CG1_PF_FS  UINT16_C(0x0020)
#define BS3CG1_PF_GS  UINT16_C(0x0040)
#define BS3CG1_PF_SS  UINT16_C(0x0080)
#define BS3CG1_PF_SEGS (BS3CG1_PF_CS | BS3CG1_PF_DS | BS3CG1_PF_ES | BS3CG1_PF_FS | BS3CG1_PF_GS | BS3CG1_PF_SS)
#define BS3CG1_PF_MEM  (BS3CG1_PF_SEGS | BS3CG1_PF_AZ)
#define BS3CG1_PF_LK  UINT16_C(0x0100)
#define BS3CG1_PF_RN  UINT16_C(0x0200)
#define BS3CG1_PF_RZ  UINT16_C(0x0400)
#define BS3CG1_PF_W   UINT16_C(0x0800) /**< REX.W */
#define BS3CG1_PF_R   UINT16_C(0x1000) /**< REX.R */
#define BS3CG1_PF_B   UINT16_C(0x2000) /**< REX.B */
#define BS3CG1_PF_X   UINT16_C(0x4000) /**< REX.X */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#if 0
static const struct
{
    uint8_t     cbPrefixes;
    uint8_t     abPrefixes[14];
    uint16_t    fEffective;
} g_aPrefixVariations[] =
{
    { 0, { 0x00 }, BS3CG1_PF_NONE },

    { 1, { P_OZ }, BS3CG1_PF_OZ },
    { 1, { P_CS }, BS3CG1_PF_CS },
    { 1, { P_DS }, BS3CG1_PF_DS },
    { 1, { P_ES }, BS3CG1_PF_ES },
    { 1, { P_FS }, BS3CG1_PF_FS },
    { 1, { P_GS }, BS3CG1_PF_GS },
    { 1, { P_SS }, BS3CG1_PF_SS },
    { 1, { P_LK }, BS3CG1_PF_LK },

    { 2, { P_CS, P_OZ, }, BS3CG1_PF_CS | BS3CFG1_PF_OZ },
    { 2, { P_DS, P_OZ, }, BS3CG1_PF_DS | BS3CFG1_PF_OZ },
    { 2, { P_ES, P_OZ, }, BS3CG1_PF_ES | BS3CFG1_PF_OZ },
    { 2, { P_FS, P_OZ, }, BS3CG1_PF_FS | BS3CFG1_PF_OZ },
    { 2, { P_GS, P_OZ, }, BS3CG1_PF_GS | BS3CFG1_PF_OZ },
    { 2, { P_GS, P_OZ, }, BS3CG1_PF_SS | BS3CFG1_PF_OZ },
    { 2, { P_SS, P_OZ, }, BS3CG1_PF_SS | BS3CFG1_PF_OZ },

    { 2, { P_OZ, P_CS, }, BS3CG1_PF_CS | BS3CFG1_PF_OZ },
    { 2, { P_OZ, P_DS, }, BS3CG1_PF_DS | BS3CFG1_PF_OZ },
    { 2, { P_OZ, P_ES, }, BS3CG1_PF_ES | BS3CFG1_PF_OZ },
    { 2, { P_OZ, P_FS, }, BS3CG1_PF_FS | BS3CFG1_PF_OZ },
    { 2, { P_OZ, P_GS, }, BS3CG1_PF_GS | BS3CFG1_PF_OZ },
    { 2, { P_OZ, P_GS, }, BS3CG1_PF_SS | BS3CFG1_PF_OZ },
    { 2, { P_OZ, P_SS, }, BS3CG1_PF_SS | BS3CFG1_PF_OZ },
};

static const uint16_t g_afPfxKindToIgnoredFlags[BS3CGPFXKIND_END] =
{
    /* [BS3CGPFXKIND_INVALID] = */              UINT16_MAX,
    /* [BS3CGPFXKIND_MODRM] = */                0,
    /* [BS3CGPFXKIND_MODRM_NO_OP_SIZES] = */    BS3CG1_PF_OZ | BS3CG1_PF_W,
};

#endif


DECLINLINE(void) Bs3Cg1InsertOpcodes(PBS3CG1STATE pThis, unsigned offDst)
{
    switch (pThis->cbOpcodes)
    {
        case 4: pThis->abCurInstr[offDst + 3] = pThis->abOpcodes[3];
        case 3: pThis->abCurInstr[offDst + 2] = pThis->abOpcodes[2];
        case 2: pThis->abCurInstr[offDst + 1] = pThis->abOpcodes[1];
        case 1: pThis->abCurInstr[offDst]     = pThis->abOpcodes[0];
            return;

        default:
            BS3_ASSERT(0);
    }
}


static bool Bs3Cg1EncodeNext(PBS3CG1STATE pThis, unsigned iEncoding)
{
    bool fDone = false;
    switch (pThis->enmEncoding)
    {
        case BS3CG1ENC_MODRM_Eb_Gb:
            //Bs3CgiInsertOpcodes(pThis, 0);
            //This.aenmOperands[0]

            break;

        case BS3CG1ENC_MODRM_Ev_Gv:
        case BS3CG1ENC_FIXED_AL_Ib:
        case BS3CG1ENC_FIXED_rAX_Iz:
            fDone = true;
            break;

        case BS3CG1ENC_END: case BS3CG1ENC_INVALID: /* Impossible; to shut up gcc. */ fDone = true; break;
    }


    return false;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(Bs3Cg1Worker)(uint8_t bMode)
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

    This.bMode              = bMode;
    This.pchMnemonic        = g_achBs3Cg1Mnemonics;
    This.pabOperands        = g_abBs3Cg1Operands;
    This.pabOpcodes         = g_abBs3Cg1Opcodes;
    This.fAdvanceMnemonic   = 1;

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
         This.pabOpcodes  += This.cbOpcodes)
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
        if (This.fAdvanceMnemonic)
            Bs3TestSubF("%.*s", This.cchMnemonic, This.pchMnemonic);
        This.fAdvanceMnemonic = pInstr->fAdvanceMnemonic;
        This.cOperands       = pInstr->cOperands;
        This.cbOpcodes       = pInstr->cbOpcodes;
        switch (This.cOperands)
        {
            case 3: This.aenmOperands[3] = (BS3CG1OP)This.pabOperands[3];
            case 2: This.aenmOperands[2] = (BS3CG1OP)This.pabOperands[2];
            case 1: This.aenmOperands[1] = (BS3CG1OP)This.pabOperands[1];
            case 0: This.aenmOperands[0] = (BS3CG1OP)This.pabOperands[0];
        }

        switch (This.cbOpcodes)
        {
            case 3: This.abOpcodes[3] = This.pabOpcodes[3];
            case 2: This.abOpcodes[2] = This.pabOpcodes[2];
            case 1: This.abOpcodes[1] = This.pabOpcodes[1];
            case 0: This.abOpcodes[0] = This.pabOpcodes[0];
        }

        /*
         * Prep the operands and encoding handling.
         */
        switch (This.enmEncoding)
        {
            case BS3CG1ENC_MODRM_Eb_Gb:
                break;
            case BS3CG1ENC_MODRM_Ev_Gv:
                break;
            case BS3CG1ENC_FIXED_AL_Ib:
                break;
            case BS3CG1ENC_FIXED_rAX_Iz:
                break;

            default:
                Bs3TestFailedF("Invalid enmEncoding for instruction #%u (%.*s): %d",
                               iInstr, This.cchMnemonic, This.pchMnemonic, This.enmEncoding);
                continue;
        }

        /*
         * Encode the instruction in various ways and check out the test values.
         */
        for (iEncoding = 0; ; iEncoding++)
        {
            /*
             * Encode the next instruction variation.
             */
            if (Bs3Cg1EncodeNext(&This, iEncoding)) { /* likely*/ }
            else break;

            /*
             * Run the tests.
             */


        }

    }

    return 0;
}

