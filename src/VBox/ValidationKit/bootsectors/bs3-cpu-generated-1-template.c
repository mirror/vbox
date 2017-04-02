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


/** @def  BS3CG1_DPRINTF
 * Debug print macro.
 */
#if 0
# define BS3CG1_DPRINTF(a_ArgList) Bs3TestPrintf a_ArgList
# define BS3CG1_DEBUG_CTX_MOD
#else
# define BS3CG1_DPRINTF(a_ArgList) do { } while (0)
#endif



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Operand value location. */
typedef enum BS3CG1OPLOC
{
    BS3CG1OPLOC_INVALID = 0,
    BS3CG1OPLOC_CTX,
    BS3CG1OPLOC_IMM,
    BS3CG1OPLOC_MEM,
    BS3CG1OPLOC_MEM_RW,
    BS3CG1OPLOC_END
} BS3CG1OPLOC;

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
    /** The current instruction number in the input array (for error reporting). */
    uint32_t                iInstr;

    /** The instruction flags. */
    uint32_t                fFlags;
    /** The encoding. */
    BS3CG1ENC               enmEncoding;
    /** The CPU test / CPU ID. */
    BS3CG1CPU               enmCpuTest;
    /** Prefix sensitivity and requirements. */
    BS3CG1PFXKIND           enmPrefixKind;
    /** Exception type (SSE, AVX). */
    BS3CG1XCPTTYPE          enmXcptType;
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

    /** Operand size in bytes (0 if not applicable). */
    uint8_t                 cbOperand;
    /** Current target ring (0..3). */
    uint8_t                 uCpl;

    /** The current test number. */
    uint8_t                 iTest;

    /** Target mode (g_bBs3CurrentMode).  */
    uint8_t                 bMode;
    /** First ring being tested. */
    uint8_t                 iFirstRing;
    /** End of rings being tested. */
    uint8_t                 iEndRing;


    /** @name Current encoded instruction.
     * @{ */
    /** The size of the current instruction that we're testing. */
    uint8_t                 cbCurInstr;
    /** The size the prefixes. */
    uint8_t                 cbCurPrefix;
    /** The offset into abCurInstr of the immediate. */
    uint8_t                 offCurImm;
    /** Buffer for assembling the current instruction. */
    uint8_t                 abCurInstr[24];

    /** Set if the encoding can't be tested in the same ring as this test code.
     *  This is used to deal with encodings modifying SP/ESP/RSP. */
    bool                    fSameRingNotOkay;
    /** Whether to work the extended context too. */
    bool                    fWorkExtCtx;
    /** The aOperands index of the modrm.reg operand (if applicable). */
    uint8_t                 iRegOp;
    /** The aOperands index of the modrm.rm operand (if applicable). */
    uint8_t                 iRmOp;

    /** Operands details. */
    struct
    {
        uint8_t             cbOp;
        /** BS3CG1OPLOC_XXX. */
        uint8_t             enmLocation;
        /** The BS3CG1DST value for this field.
         * Set to BS3CG1DST_INVALID if memory or immediate.  */
        uint8_t             idxField;
        /** Depends on enmLocation.
         * - BS3CG1OPLOC_IMM:       offset relative to start of the instruction.
         * - BS3CG1OPLOC_MEM:       offset should be subtracted from &pbDataPg[_4K].
         * - BS3CG1OPLOC_MEM_RW:    offset should be subtracted from &pbDataPg[_4K].
         * - BS3CG1OPLOC_CTX:       not used (use idxField instead).
         */
        uint8_t             off;
    } aOperands[4];
    /** @} */

    /** Page to put code in.  When paging is enabled, the page before and after
     * are marked not-present. */
    uint8_t BS3_FAR        *pbCodePg;
    /** The flat address corresponding to pbCodePg.  */
    uintptr_t               uCodePgFlat;
    /** The 16-bit address corresponding to pbCodePg if relevant for bMode.  */
    RTFAR16                 CodePgFar;
    /** The IP/EIP/RIP value for pbCodePg[0] relative to CS (bMode). */
    uintptr_t               CodePgRip;

    /** Page for placing data operands in.  When paging is enabled, the page before
     * and after are marked not-present.  */
    uint8_t BS3_FAR        *pbDataPg;
    /** The flat address corresponding to pbDataPg.  */
    uintptr_t               uDataPgFlat;
    /** The 16-bit address corresponding to pbDataPg.  */
    RTFAR16                 DataPgFar;

    /** The name corresponding to bMode. */
    const char BS3_FAR     *pszMode;
    /** The short name corresponding to bMode. */
    const char BS3_FAR     *pszModeShort;

    /** @name Expected result (modifiable by output program).
     * @{ */
    /** The expected exception based on operand values or result.
     * UINT8_MAX if no special exception expected. */
    uint8_t                 bValueXcpt;
    /** @} */
    /** Alignment exception expected by the encoder.
     * UINT8_MAX if no special exception expected. */
    uint8_t                 bAlignmentXcpt;

    /** The context we're working on. */
    BS3REGCTX               Ctx;
    /** The trap context and frame. */
    BS3TRAPFRAME            TrapFrame;
    /** Initial contexts, one for each ring. */
    BS3REGCTX               aInitialCtxs[4];

    /** The extended context we're working on (input, expected output). */
    PBS3EXTCTX              pExtCtx;
    /** The extended result context (analoguous to TrapFrame). */
    PBS3EXTCTX              pResultExtCtx;
    /** The initial extended context. */
    PBS3EXTCTX              pInitialExtCtx;

    /** Memory operand scratch space. */
    union
    {
        uint8_t             ab[128];
        uint16_t            au16[128 / sizeof(uint16_t)];
        uint32_t            au32[128 / sizeof(uint32_t)];
        uint64_t            au64[128 / sizeof(uint64_t)];
    } MemOp;

    /** Array parallel to aInitialCtxs for saving segment registers. */
    struct
    {
        RTSEL               ds;
    } aSavedSegRegs[4];

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


/** Used in g_cbBs3Cg1DstFields to indicate that it's one of the 4 operands. */
#define BS3CG1DSTSIZE_OPERAND               UINT8_C(255)
/** Used in g_cbBs3Cg1DstFields to indicate that the operand size determins
 * the field size (2, 4, or 8). */
#define BS3CG1DSTSIZE_OPERAND_SIZE_GRP      UINT8_C(254)



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Destination field sizes indexed by bBS3CG1DST.
 * Zero means operand size sized.  */
static const uint8_t g_acbBs3Cg1DstFields[] =
{
    /* [BS3CG1DST_INVALID] = */ BS3CG1DSTSIZE_OPERAND,

    /* [BS3CG1DST_OP1] = */     BS3CG1DSTSIZE_OPERAND,
    /* [BS3CG1DST_OP2] = */     BS3CG1DSTSIZE_OPERAND,
    /* [BS3CG1DST_OP3] = */     BS3CG1DSTSIZE_OPERAND,
    /* [BS3CG1DST_OP4] = */     BS3CG1DSTSIZE_OPERAND,
    /* [BS3CG1DST_EFL] = */     4,
    /* [BS3CG1DST_EFL_UNDEF]=*/ 4,

    /* [BS3CG1DST_AL] = */      1,
    /* [BS3CG1DST_CL] = */      1,
    /* [BS3CG1DST_DL] = */      1,
    /* [BS3CG1DST_BL] = */      1,
    /* [BS3CG1DST_AH] = */      1,
    /* [BS3CG1DST_CH] = */      1,
    /* [BS3CG1DST_DH] = */      1,
    /* [BS3CG1DST_BH] = */      1,
    /* [BS3CG1DST_SPL] = */     1,
    /* [BS3CG1DST_BPL] = */     1,
    /* [BS3CG1DST_SIL] = */     1,
    /* [BS3CG1DST_DIL] = */     1,
    /* [BS3CG1DST_R8L] = */     1,
    /* [BS3CG1DST_R9L] = */     1,
    /* [BS3CG1DST_R10L] = */    1,
    /* [BS3CG1DST_R11L] = */    1,
    /* [BS3CG1DST_R12L] = */    1,
    /* [BS3CG1DST_R13L] = */    1,
    /* [BS3CG1DST_R14L] = */    1,
    /* [BS3CG1DST_R15L] = */    1,

    /* [BS3CG1DST_AX] = */      2,
    /* [BS3CG1DST_CX] = */      2,
    /* [BS3CG1DST_DX] = */      2,
    /* [BS3CG1DST_BX] = */      2,
    /* [BS3CG1DST_SP] = */      2,
    /* [BS3CG1DST_BP] = */      2,
    /* [BS3CG1DST_SI] = */      2,
    /* [BS3CG1DST_DI] = */      2,
    /* [BS3CG1DST_R8W] = */     2,
    /* [BS3CG1DST_R9W] = */     2,
    /* [BS3CG1DST_R10W] = */    2,
    /* [BS3CG1DST_R11W] = */    2,
    /* [BS3CG1DST_R12W] = */    2,
    /* [BS3CG1DST_R13W] = */    2,
    /* [BS3CG1DST_R14W] = */    2,
    /* [BS3CG1DST_R15W] = */    2,

    /* [BS3CG1DST_EAX] = */     4,
    /* [BS3CG1DST_ECX] = */     4,
    /* [BS3CG1DST_EDX] = */     4,
    /* [BS3CG1DST_EBX] = */     4,
    /* [BS3CG1DST_ESP] = */     4,
    /* [BS3CG1DST_EBP] = */     4,
    /* [BS3CG1DST_ESI] = */     4,
    /* [BS3CG1DST_EDI] = */     4,
    /* [BS3CG1DST_R8D] = */     4,
    /* [BS3CG1DST_R9D] = */     4,
    /* [BS3CG1DST_R10D] = */    4,
    /* [BS3CG1DST_R11D] = */    4,
    /* [BS3CG1DST_R12D] = */    4,
    /* [BS3CG1DST_R13D] = */    4,
    /* [BS3CG1DST_R14D] = */    4,
    /* [BS3CG1DST_R15D] = */    4,

    /* [BS3CG1DST_RAX] = */     8,
    /* [BS3CG1DST_RCX] = */     8,
    /* [BS3CG1DST_RDX] = */     8,
    /* [BS3CG1DST_RBX] = */     8,
    /* [BS3CG1DST_RSP] = */     8,
    /* [BS3CG1DST_RBP] = */     8,
    /* [BS3CG1DST_RSI] = */     8,
    /* [BS3CG1DST_RDI] = */     8,
    /* [BS3CG1DST_R8] = */      8,
    /* [BS3CG1DST_R9] = */      8,
    /* [BS3CG1DST_R10] = */     8,
    /* [BS3CG1DST_R11] = */     8,
    /* [BS3CG1DST_R12] = */     8,
    /* [BS3CG1DST_R13] = */     8,
    /* [BS3CG1DST_R14] = */     8,
    /* [BS3CG1DST_R15] = */     8,

    /* [BS3CG1DST_OZ_RAX] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RCX] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RDX] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RBX] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RSP] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RBP] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RSI] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_RDI] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R8] = */   BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R9] = */   BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R10] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R11] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R12] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R13] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R14] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,
    /* [BS3CG1DST_OZ_R15] = */  BS3CG1DSTSIZE_OPERAND_SIZE_GRP,

    /* [BS3CG1DST_FCW] = */         2,
    /* [BS3CG1DST_FSW] = */         2,
    /* [BS3CG1DST_FTW] = */         2,
    /* [BS3CG1DST_FOP] = */         2,
    /* [BS3CG1DST_FPUIP] = */       2,
    /* [BS3CG1DST_FPUCS] = */       2,
    /* [BS3CG1DST_FPUDP] = */       2,
    /* [BS3CG1DST_FPUDS] = */       2,
    /* [BS3CG1DST_MXCSR] = */       4,
    /* [BS3CG1DST_ST0] = */         12,
    /* [BS3CG1DST_ST1] = */         12,
    /* [BS3CG1DST_ST2] = */         12,
    /* [BS3CG1DST_ST3] = */         12,
    /* [BS3CG1DST_ST4] = */         12,
    /* [BS3CG1DST_ST5] = */         12,
    /* [BS3CG1DST_ST6] = */         12,
    /* [BS3CG1DST_ST7] = */         12,
    /* [BS3CG1DST_MM0] = */         8,
    /* [BS3CG1DST_MM1] = */         8,
    /* [BS3CG1DST_MM2] = */         8,
    /* [BS3CG1DST_MM3] = */         8,
    /* [BS3CG1DST_MM4] = */         8,
    /* [BS3CG1DST_MM5] = */         8,
    /* [BS3CG1DST_MM6] = */         8,
    /* [BS3CG1DST_MM7] = */         8,
    /* [BS3CG1DST_XMM0] = */        16,
    /* [BS3CG1DST_XMM1] = */        16,
    /* [BS3CG1DST_XMM2] = */        16,
    /* [BS3CG1DST_XMM3] = */        16,
    /* [BS3CG1DST_XMM4] = */        16,
    /* [BS3CG1DST_XMM5] = */        16,
    /* [BS3CG1DST_XMM6] = */        16,
    /* [BS3CG1DST_XMM7] = */        16,
    /* [BS3CG1DST_XMM8] = */        16,
    /* [BS3CG1DST_XMM9] = */        16,
    /* [BS3CG1DST_XMM10] = */       16,
    /* [BS3CG1DST_XMM11] = */       16,
    /* [BS3CG1DST_XMM12] = */       16,
    /* [BS3CG1DST_XMM13] = */       16,
    /* [BS3CG1DST_XMM14] = */       16,
    /* [BS3CG1DST_XMM15] = */       16,
    /* [BS3CG1DST_XMM0_LO] = */     8,
    /* [BS3CG1DST_XMM1_LO] = */     8,
    /* [BS3CG1DST_XMM2_LO] = */     8,
    /* [BS3CG1DST_XMM3_LO] = */     8,
    /* [BS3CG1DST_XMM4_LO] = */     8,
    /* [BS3CG1DST_XMM5_LO] = */     8,
    /* [BS3CG1DST_XMM6_LO] = */     8,
    /* [BS3CG1DST_XMM7_LO] = */     8,
    /* [BS3CG1DST_XMM8_LO] = */     8,
    /* [BS3CG1DST_XMM9_LO] = */     8,
    /* [BS3CG1DST_XMM10_LO] = */    8,
    /* [BS3CG1DST_XMM11_LO] = */    8,
    /* [BS3CG1DST_XMM12_LO] = */    8,
    /* [BS3CG1DST_XMM13_LO] = */    8,
    /* [BS3CG1DST_XMM14_LO] = */    8,
    /* [BS3CG1DST_XMM15_LO] = */    8,
    /* [BS3CG1DST_XMM0_HI] = */     8,
    /* [BS3CG1DST_XMM1_HI] = */     8,
    /* [BS3CG1DST_XMM2_HI] = */     8,
    /* [BS3CG1DST_XMM3_HI] = */     8,
    /* [BS3CG1DST_XMM4_HI] = */     8,
    /* [BS3CG1DST_XMM5_HI] = */     8,
    /* [BS3CG1DST_XMM6_HI] = */     8,
    /* [BS3CG1DST_XMM7_HI] = */     8,
    /* [BS3CG1DST_XMM8_HI] = */     8,
    /* [BS3CG1DST_XMM9_HI] = */     8,
    /* [BS3CG1DST_XMM10_HI] = */    8,
    /* [BS3CG1DST_XMM11_HI] = */    8,
    /* [BS3CG1DST_XMM12_HI] = */    8,
    /* [BS3CG1DST_XMM13_HI] = */    8,
    /* [BS3CG1DST_XMM14_HI] = */    8,
    /* [BS3CG1DST_XMM15_HI] = */    8,
    /* [BS3CG1DST_XMM0_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM1_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM2_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM3_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM4_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM5_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM6_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM7_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM8_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM9_LO_ZX] = */  8,
    /* [BS3CG1DST_XMM10_LO_ZX] = */ 8,
    /* [BS3CG1DST_XMM11_LO_ZX] = */ 8,
    /* [BS3CG1DST_XMM12_LO_ZX] = */ 8,
    /* [BS3CG1DST_XMM13_LO_ZX] = */ 8,
    /* [BS3CG1DST_XMM14_LO_ZX] = */ 8,
    /* [BS3CG1DST_XMM15_LO_ZX] = */ 8,
    /* [BS3CG1DST_XMM0_DW0] = */    4,
    /* [BS3CG1DST_XMM1_DW0] = */    4,
    /* [BS3CG1DST_XMM2_DW0] = */    4,
    /* [BS3CG1DST_XMM3_DW0] = */    4,
    /* [BS3CG1DST_XMM4_DW0] = */    4,
    /* [BS3CG1DST_XMM5_DW0] = */    4,
    /* [BS3CG1DST_XMM6_DW0] = */    4,
    /* [BS3CG1DST_XMM7_DW0] = */    4,
    /* [BS3CG1DST_XMM8_DW0] = */    4,
    /* [BS3CG1DST_XMM9_DW0] = */    4,
    /* [BS3CG1DST_XMM10_DW0] = */   4,
    /* [BS3CG1DST_XMM11_DW0] = */   4,
    /* [BS3CG1DST_XMM12_DW0] = */   4,
    /* [BS3CG1DST_XMM13_DW0] = */   4,
    /* [BS3CG1DST_XMM14_DW0] = */   4,
    /* [BS3CG1DST_XMM15_DW0] = */   4,
    /* [BS3CG1DST_YMM0] = */        32,
    /* [BS3CG1DST_YMM1] = */        32,
    /* [BS3CG1DST_YMM2] = */        32,
    /* [BS3CG1DST_YMM3] = */        32,
    /* [BS3CG1DST_YMM4] = */        32,
    /* [BS3CG1DST_YMM5] = */        32,
    /* [BS3CG1DST_YMM6] = */        32,
    /* [BS3CG1DST_YMM7] = */        32,
    /* [BS3CG1DST_YMM8] = */        32,
    /* [BS3CG1DST_YMM9] = */        32,
    /* [BS3CG1DST_YMM10] = */       32,
    /* [BS3CG1DST_YMM11] = */       32,
    /* [BS3CG1DST_YMM12] = */       32,
    /* [BS3CG1DST_YMM13] = */       32,
    /* [BS3CG1DST_YMM14] = */       32,
    /* [BS3CG1DST_YMM15] = */       32,

    /* [BS3CG1DST_VALUE_XCPT] = */ 1,
};
AssertCompile(RT_ELEMENTS(g_acbBs3Cg1DstFields) == BS3CG1DST_END);

/** Destination field offset indexed by bBS3CG1DST.
 * Zero means operand size sized.  */
static const unsigned g_aoffBs3Cg1DstFields[] =
{
    /* [BS3CG1DST_INVALID] = */     ~0U,
    /* [BS3CG1DST_OP1] = */         ~0U,
    /* [BS3CG1DST_OP2] = */         ~0U,
    /* [BS3CG1DST_OP3] = */         ~0U,
    /* [BS3CG1DST_OP4] = */         ~0U,
    /* [BS3CG1DST_EFL] = */         RT_OFFSETOF(BS3REGCTX, rflags),
    /* [BS3CG1DST_EFL_UNDEF]=*/     ~0, /* special field */

    /* [BS3CG1DST_AL] = */          RT_OFFSETOF(BS3REGCTX, rax.u8),
    /* [BS3CG1DST_CL] = */          RT_OFFSETOF(BS3REGCTX, rcx.u8),
    /* [BS3CG1DST_DL] = */          RT_OFFSETOF(BS3REGCTX, rdx.u8),
    /* [BS3CG1DST_BL] = */          RT_OFFSETOF(BS3REGCTX, rbx.u8),
    /* [BS3CG1DST_AH] = */          RT_OFFSETOF(BS3REGCTX, rax.b.bHi),
    /* [BS3CG1DST_CH] = */          RT_OFFSETOF(BS3REGCTX, rcx.b.bHi),
    /* [BS3CG1DST_DH] = */          RT_OFFSETOF(BS3REGCTX, rdx.b.bHi),
    /* [BS3CG1DST_BH] = */          RT_OFFSETOF(BS3REGCTX, rbx.b.bHi),
    /* [BS3CG1DST_SPL] = */         RT_OFFSETOF(BS3REGCTX, rsp.u8),
    /* [BS3CG1DST_BPL] = */         RT_OFFSETOF(BS3REGCTX, rbp.u8),
    /* [BS3CG1DST_SIL] = */         RT_OFFSETOF(BS3REGCTX, rsi.u8),
    /* [BS3CG1DST_DIL] = */         RT_OFFSETOF(BS3REGCTX, rdi.u8),
    /* [BS3CG1DST_R8L] = */         RT_OFFSETOF(BS3REGCTX, r8.u8),
    /* [BS3CG1DST_R9L] = */         RT_OFFSETOF(BS3REGCTX, r9.u8),
    /* [BS3CG1DST_R10L] = */        RT_OFFSETOF(BS3REGCTX, r10.u8),
    /* [BS3CG1DST_R11L] = */        RT_OFFSETOF(BS3REGCTX, r11.u8),
    /* [BS3CG1DST_R12L] = */        RT_OFFSETOF(BS3REGCTX, r12.u8),
    /* [BS3CG1DST_R13L] = */        RT_OFFSETOF(BS3REGCTX, r13.u8),
    /* [BS3CG1DST_R14L] = */        RT_OFFSETOF(BS3REGCTX, r14.u8),
    /* [BS3CG1DST_R15L] = */        RT_OFFSETOF(BS3REGCTX, r15.u8),

    /* [BS3CG1DST_AX] = */          RT_OFFSETOF(BS3REGCTX, rax.u16),
    /* [BS3CG1DST_CX] = */          RT_OFFSETOF(BS3REGCTX, rcx.u16),
    /* [BS3CG1DST_DX] = */          RT_OFFSETOF(BS3REGCTX, rdx.u16),
    /* [BS3CG1DST_BX] = */          RT_OFFSETOF(BS3REGCTX, rbx.u16),
    /* [BS3CG1DST_SP] = */          RT_OFFSETOF(BS3REGCTX, rsp.u16),
    /* [BS3CG1DST_BP] = */          RT_OFFSETOF(BS3REGCTX, rbp.u16),
    /* [BS3CG1DST_SI] = */          RT_OFFSETOF(BS3REGCTX, rsi.u16),
    /* [BS3CG1DST_DI] = */          RT_OFFSETOF(BS3REGCTX, rdi.u16),
    /* [BS3CG1DST_R8W] = */         RT_OFFSETOF(BS3REGCTX, r8.u16),
    /* [BS3CG1DST_R9W] = */         RT_OFFSETOF(BS3REGCTX, r9.u16),
    /* [BS3CG1DST_R10W] = */        RT_OFFSETOF(BS3REGCTX, r10.u16),
    /* [BS3CG1DST_R11W] = */        RT_OFFSETOF(BS3REGCTX, r11.u16),
    /* [BS3CG1DST_R12W] = */        RT_OFFSETOF(BS3REGCTX, r12.u16),
    /* [BS3CG1DST_R13W] = */        RT_OFFSETOF(BS3REGCTX, r13.u16),
    /* [BS3CG1DST_R14W] = */        RT_OFFSETOF(BS3REGCTX, r14.u16),
    /* [BS3CG1DST_R15W] = */        RT_OFFSETOF(BS3REGCTX, r15.u16),

    /* [BS3CG1DST_EAX] = */         RT_OFFSETOF(BS3REGCTX, rax.u32),
    /* [BS3CG1DST_ECX] = */         RT_OFFSETOF(BS3REGCTX, rcx.u32),
    /* [BS3CG1DST_EDX] = */         RT_OFFSETOF(BS3REGCTX, rdx.u32),
    /* [BS3CG1DST_EBX] = */         RT_OFFSETOF(BS3REGCTX, rbx.u32),
    /* [BS3CG1DST_ESP] = */         RT_OFFSETOF(BS3REGCTX, rsp.u32),
    /* [BS3CG1DST_EBP] = */         RT_OFFSETOF(BS3REGCTX, rbp.u32),
    /* [BS3CG1DST_ESI] = */         RT_OFFSETOF(BS3REGCTX, rsi.u32),
    /* [BS3CG1DST_EDI] = */         RT_OFFSETOF(BS3REGCTX, rdi.u32),
    /* [BS3CG1DST_R8D] = */         RT_OFFSETOF(BS3REGCTX, r8.u32),
    /* [BS3CG1DST_R9D] = */         RT_OFFSETOF(BS3REGCTX, r9.u32),
    /* [BS3CG1DST_R10D] = */        RT_OFFSETOF(BS3REGCTX, r10.u32),
    /* [BS3CG1DST_R11D] = */        RT_OFFSETOF(BS3REGCTX, r11.u32),
    /* [BS3CG1DST_R12D] = */        RT_OFFSETOF(BS3REGCTX, r12.u32),
    /* [BS3CG1DST_R13D] = */        RT_OFFSETOF(BS3REGCTX, r13.u32),
    /* [BS3CG1DST_R14D] = */        RT_OFFSETOF(BS3REGCTX, r14.u32),
    /* [BS3CG1DST_R15D] = */        RT_OFFSETOF(BS3REGCTX, r15.u32),

    /* [BS3CG1DST_RAX] = */         RT_OFFSETOF(BS3REGCTX, rax.u64),
    /* [BS3CG1DST_RCX] = */         RT_OFFSETOF(BS3REGCTX, rcx.u64),
    /* [BS3CG1DST_RDX] = */         RT_OFFSETOF(BS3REGCTX, rdx.u64),
    /* [BS3CG1DST_RBX] = */         RT_OFFSETOF(BS3REGCTX, rbx.u64),
    /* [BS3CG1DST_RSP] = */         RT_OFFSETOF(BS3REGCTX, rsp.u64),
    /* [BS3CG1DST_RBP] = */         RT_OFFSETOF(BS3REGCTX, rbp.u64),
    /* [BS3CG1DST_RSI] = */         RT_OFFSETOF(BS3REGCTX, rsi.u64),
    /* [BS3CG1DST_RDI] = */         RT_OFFSETOF(BS3REGCTX, rdi.u64),
    /* [BS3CG1DST_R8] = */          RT_OFFSETOF(BS3REGCTX, r8.u64),
    /* [BS3CG1DST_R9] = */          RT_OFFSETOF(BS3REGCTX, r9.u64),
    /* [BS3CG1DST_R10] = */         RT_OFFSETOF(BS3REGCTX, r10.u64),
    /* [BS3CG1DST_R11] = */         RT_OFFSETOF(BS3REGCTX, r11.u64),
    /* [BS3CG1DST_R12] = */         RT_OFFSETOF(BS3REGCTX, r12.u64),
    /* [BS3CG1DST_R13] = */         RT_OFFSETOF(BS3REGCTX, r13.u64),
    /* [BS3CG1DST_R14] = */         RT_OFFSETOF(BS3REGCTX, r14.u64),
    /* [BS3CG1DST_R15] = */         RT_OFFSETOF(BS3REGCTX, r15.u64),

    /* [BS3CG1DST_OZ_RAX] = */      RT_OFFSETOF(BS3REGCTX, rax),
    /* [BS3CG1DST_OZ_RCX] = */      RT_OFFSETOF(BS3REGCTX, rcx),
    /* [BS3CG1DST_OZ_RDX] = */      RT_OFFSETOF(BS3REGCTX, rdx),
    /* [BS3CG1DST_OZ_RBX] = */      RT_OFFSETOF(BS3REGCTX, rbx),
    /* [BS3CG1DST_OZ_RSP] = */      RT_OFFSETOF(BS3REGCTX, rsp),
    /* [BS3CG1DST_OZ_RBP] = */      RT_OFFSETOF(BS3REGCTX, rbp),
    /* [BS3CG1DST_OZ_RSI] = */      RT_OFFSETOF(BS3REGCTX, rsi),
    /* [BS3CG1DST_OZ_RDI] = */      RT_OFFSETOF(BS3REGCTX, rdi),
    /* [BS3CG1DST_OZ_R8] = */       RT_OFFSETOF(BS3REGCTX, r8),
    /* [BS3CG1DST_OZ_R9] = */       RT_OFFSETOF(BS3REGCTX, r9),
    /* [BS3CG1DST_OZ_R10] = */      RT_OFFSETOF(BS3REGCTX, r10),
    /* [BS3CG1DST_OZ_R11] = */      RT_OFFSETOF(BS3REGCTX, r11),
    /* [BS3CG1DST_OZ_R12] = */      RT_OFFSETOF(BS3REGCTX, r12),
    /* [BS3CG1DST_OZ_R13] = */      RT_OFFSETOF(BS3REGCTX, r13),
    /* [BS3CG1DST_OZ_R14] = */      RT_OFFSETOF(BS3REGCTX, r14),
    /* [BS3CG1DST_OZ_R15] = */      RT_OFFSETOF(BS3REGCTX, r15),

    /* [BS3CG1DST_FCW] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.FCW),
    /* [BS3CG1DST_FSW] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.FSW),
    /* [BS3CG1DST_FTW] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.FTW),
    /* [BS3CG1DST_FOP] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.FOP),
    /* [BS3CG1DST_FPUIP] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.FPUIP),
    /* [BS3CG1DST_FPUCS] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.CS),
    /* [BS3CG1DST_FPUDP] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.FPUDP),
    /* [BS3CG1DST_FPUDS] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.DS),
    /* [BS3CG1DST_MXCSR] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.MXCSR),
    /* [BS3CG1DST_ST0] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[0]),
    /* [BS3CG1DST_ST1] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[1]),
    /* [BS3CG1DST_ST2] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[2]),
    /* [BS3CG1DST_ST3] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[3]),
    /* [BS3CG1DST_ST4] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[4]),
    /* [BS3CG1DST_ST5] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[5]),
    /* [BS3CG1DST_ST6] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[6]),
    /* [BS3CG1DST_ST7] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[7]),
    /* [BS3CG1DST_MM0] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[0]),
    /* [BS3CG1DST_MM1] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[1]),
    /* [BS3CG1DST_MM2] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[2]),
    /* [BS3CG1DST_MM3] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[3]),
    /* [BS3CG1DST_MM4] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[4]),
    /* [BS3CG1DST_MM5] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[5]),
    /* [BS3CG1DST_MM6] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[6]),
    /* [BS3CG1DST_MM7] = */         sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aRegs[7]),

    /* [BS3CG1DST_XMM0] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[0]),
    /* [BS3CG1DST_XMM1] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[1]),
    /* [BS3CG1DST_XMM2] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[2]),
    /* [BS3CG1DST_XMM3] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[3]),
    /* [BS3CG1DST_XMM4] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[4]),
    /* [BS3CG1DST_XMM5] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[5]),
    /* [BS3CG1DST_XMM6] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[6]),
    /* [BS3CG1DST_XMM7] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[7]),
    /* [BS3CG1DST_XMM8] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[8]),
    /* [BS3CG1DST_XMM9] = */        sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[9]),
    /* [BS3CG1DST_XMM10] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[10]),
    /* [BS3CG1DST_XMM11] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[11]),
    /* [BS3CG1DST_XMM12] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[12]),
    /* [BS3CG1DST_XMM13] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[13]),
    /* [BS3CG1DST_XMM14] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[14]),
    /* [BS3CG1DST_XMM15] = */       sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]),
    /* [BS3CG1DST_XMM0_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[0]),
    /* [BS3CG1DST_XMM1_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[1]),
    /* [BS3CG1DST_XMM2_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[2]),
    /* [BS3CG1DST_XMM3_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[3]),
    /* [BS3CG1DST_XMM4_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[4]),
    /* [BS3CG1DST_XMM5_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[5]),
    /* [BS3CG1DST_XMM6_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[6]),
    /* [BS3CG1DST_XMM7_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[7]),
    /* [BS3CG1DST_XMM8_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[8]),
    /* [BS3CG1DST_XMM9_LO] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[9]),
    /* [BS3CG1DST_XMM10_LO] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[10]),
    /* [BS3CG1DST_XMM11_LO] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[11]),
    /* [BS3CG1DST_XMM12_LO] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[12]),
    /* [BS3CG1DST_XMM13_LO] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[13]),
    /* [BS3CG1DST_XMM14_LO] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[14]),
    /* [BS3CG1DST_XMM15_LO] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]),
    /* [BS3CG1DST_XMM0_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[0])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM1_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[1])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM2_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[2])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM3_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[3])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM4_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[4])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM5_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[5])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM6_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[6])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM7_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[7])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM8_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[8])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM9_HI] = */     sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[9])  + sizeof(uint64_t),
    /* [BS3CG1DST_XMM10_HI] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[10]) + sizeof(uint64_t),
    /* [BS3CG1DST_XMM11_HI] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[11]) + sizeof(uint64_t),
    /* [BS3CG1DST_XMM12_HI] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[12]) + sizeof(uint64_t),
    /* [BS3CG1DST_XMM13_HI] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[13]) + sizeof(uint64_t),
    /* [BS3CG1DST_XMM14_HI] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[14]) + sizeof(uint64_t),
    /* [BS3CG1DST_XMM15_HI] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]) + sizeof(uint64_t),
    /* [BS3CG1DST_XMM0_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[0]),
    /* [BS3CG1DST_XMM1_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[1]),
    /* [BS3CG1DST_XMM2_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[2]),
    /* [BS3CG1DST_XMM3_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[3]),
    /* [BS3CG1DST_XMM4_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[4]),
    /* [BS3CG1DST_XMM5_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[5]),
    /* [BS3CG1DST_XMM6_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[6]),
    /* [BS3CG1DST_XMM7_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[7]),
    /* [BS3CG1DST_XMM8_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[8]),
    /* [BS3CG1DST_XMM9_LO_ZX] = */  sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[9]),
    /* [BS3CG1DST_XMM10_LO_ZX] = */ sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[10]),
    /* [BS3CG1DST_XMM11_LO_ZX] = */ sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[11]),
    /* [BS3CG1DST_XMM12_LO_ZX] = */ sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[12]),
    /* [BS3CG1DST_XMM13_LO_ZX] = */ sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[13]),
    /* [BS3CG1DST_XMM14_LO_ZX] = */ sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[14]),
    /* [BS3CG1DST_XMM15_LO_ZX] = */ sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]),
    /* [BS3CG1DST_XMM0_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[0]),
    /* [BS3CG1DST_XMM1_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[1]),
    /* [BS3CG1DST_XMM2_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[2]),
    /* [BS3CG1DST_XMM3_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[3]),
    /* [BS3CG1DST_XMM4_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[4]),
    /* [BS3CG1DST_XMM5_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[5]),
    /* [BS3CG1DST_XMM6_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[6]),
    /* [BS3CG1DST_XMM7_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[7]),
    /* [BS3CG1DST_XMM8_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[8]),
    /* [BS3CG1DST_XMM9_DW0] = */    sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[9]),
    /* [BS3CG1DST_XMM10_DW0] = */   sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[10]),
    /* [BS3CG1DST_XMM11_DW0] = */   sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[11]),
    /* [BS3CG1DST_XMM12_DW0] = */   sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[12]),
    /* [BS3CG1DST_XMM13_DW0] = */   sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[13]),
    /* [BS3CG1DST_XMM14_DW0] = */   sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[14]),
    /* [BS3CG1DST_XMM15_DW0] = */   sizeof(BS3REGCTX) + RT_OFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]),

    /* [BS3CG1DST_YMM0] = */        ~0U,
    /* [BS3CG1DST_YMM1] = */        ~0U,
    /* [BS3CG1DST_YMM2] = */        ~0U,
    /* [BS3CG1DST_YMM3] = */        ~0U,
    /* [BS3CG1DST_YMM4] = */        ~0U,
    /* [BS3CG1DST_YMM5] = */        ~0U,
    /* [BS3CG1DST_YMM6] = */        ~0U,
    /* [BS3CG1DST_YMM7] = */        ~0U,
    /* [BS3CG1DST_YMM8] = */        ~0U,
    /* [BS3CG1DST_YMM9] = */        ~0U,
    /* [BS3CG1DST_YMM10] = */       ~0U,
    /* [BS3CG1DST_YMM11] = */       ~0U,
    /* [BS3CG1DST_YMM12] = */       ~0U,
    /* [BS3CG1DST_YMM13] = */       ~0U,
    /* [BS3CG1DST_YMM14] = */       ~0U,
    /* [BS3CG1DST_YMM15] = */       ~0U,

    /* [BS3CG1DST_VALUE_XCPT] = */  ~0U,
};
AssertCompile(RT_ELEMENTS(g_aoffBs3Cg1DstFields) == BS3CG1DST_END);

#ifdef BS3CG1_DEBUG_CTX_MOD
/** Destination field names. */
static const struct { char sz[12]; } g_aszBs3Cg1DstFields[] =
{
    { "INVALID" },
    { "OP1" },
    { "OP2" },
    { "OP3" },
    { "OP4" },
    { "EFL" },
    { "EFL_UND" },

    { "AL" },
    { "CL" },
    { "DL" },
    { "BL" },
    { "AH" },
    { "CH" },
    { "DH" },
    { "BH" },
    { "SPL" },
    { "BPL" },
    { "SIL" },
    { "DIL" },
    { "R8L" },
    { "R9L" },
    { "R10L" },
    { "R11L" },
    { "R12L" },
    { "R13L" },
    { "R14L" },
    { "R15L" },

    { "AX" },
    { "CX" },
    { "DX" },
    { "BX" },
    { "SP" },
    { "BP" },
    { "SI" },
    { "DI" },
    { "R8W" },
    { "R9W" },
    { "R10W" },
    { "R11W" },
    { "R12W" },
    { "R13W" },
    { "R14W" },
    { "R15W" },

    { "EAX" },
    { "ECX" },
    { "EDX" },
    { "EBX" },
    { "ESP" },
    { "EBP" },
    { "ESI" },
    { "EDI" },
    { "R8D" },
    { "R9D" },
    { "R10D" },
    { "R11D" },
    { "R12D" },
    { "R13D" },
    { "R14D" },
    { "R15D" },

    { "RAX" },
    { "RCX" },
    { "RDX" },
    { "RBX" },
    { "RSP" },
    { "RBP" },
    { "RSI" },
    { "RDI" },
    { "R8"  },
    { "R9"  },
    { "R10" },
    { "R11" },
    { "R12" },
    { "R13" },
    { "R14" },
    { "R15" },

    { "OZ_RAX" },
    { "OZ_RCX" },
    { "OZ_RDX" },
    { "OZ_RBX" },
    { "OZ_RSP" },
    { "OZ_RBP" },
    { "OZ_RSI" },
    { "OZ_RDI" },
    { "OZ_R8"  },
    { "OZ_R9"  },
    { "OZ_R10" },
    { "OZ_R11" },
    { "OZ_R12" },
    { "OZ_R13" },
    { "OZ_R14" },
    { "OZ_R15" },

    { "FCW" },
    { "FSW" },
    { "FTW" },
    { "FOP" },
    { "FPUIP" },
    { "FPUCS" },
    { "FPUDP" },
    { "FPUDS" },
    { "MXCSR" },
    { "MXCSR_M" },
    { "ST0" },
    { "ST1" },
    { "ST2" },
    { "ST3" },
    { "ST4" },
    { "ST5" },
    { "ST6" },
    { "ST7" },
    { "MM0" },
    { "MM1" },
    { "MM2" },
    { "MM3" },
    { "MM4" },
    { "MM5" },
    { "MM6" },
    { "MM7" },
    { "XMM0" },
    { "XMM1" },
    { "XMM2" },
    { "XMM3" },
    { "XMM4" },
    { "XMM5" },
    { "XMM6" },
    { "XMM7" },
    { "XMM8" },
    { "XMM9" },
    { "XMM10" },
    { "XMM11" },
    { "XMM12" },
    { "XMM13" },
    { "XMM14" },
    { "XMM15" },
    { "XMM0_LO" },
    { "XMM1_LO" },
    { "XMM2_LO" },
    { "XMM3_LO" },
    { "XMM4_LO" },
    { "XMM5_LO" },
    { "XMM6_LO" },
    { "XMM7_LO" },
    { "XMM8_LO" },
    { "XMM9_LO" },
    { "XMM10_LO" },
    { "XMM11_LO" },
    { "XMM12_LO" },
    { "XMM13_LO" },
    { "XMM14_LO" },
    { "XMM15_LO" },
    { "XMM0_HI" },
    { "XMM1_HI" },
    { "XMM2_HI" },
    { "XMM3_HI" },
    { "XMM4_HI" },
    { "XMM5_HI" },
    { "XMM6_HI" },
    { "XMM7_HI" },
    { "XMM8_HI" },
    { "XMM9_HI" },
    { "XMM10_HI" },
    { "XMM11_HI" },
    { "XMM12_HI" },
    { "XMM13_HI" },
    { "XMM14_HI" },
    { "XMM15_HI" },
    { "XMM0_LO_ZX" },
    { "XMM1_LO_ZX" },
    { "XMM2_LO_ZX" },
    { "XMM3_LO_ZX" },
    { "XMM4_LO_ZX" },
    { "XMM5_LO_ZX" },
    { "XMM6_LO_ZX" },
    { "XMM7_LO_ZX" },
    { "XMM8_LO_ZX" },
    { "XMM9_LO_ZX" },
    { "XMM10_LO_ZX" },
    { "XMM11_LO_ZX" },
    { "XMM12_LO_ZX" },
    { "XMM13_LO_ZX" },
    { "XMM14_LO_ZX" },
    { "XMM15_LO_ZX" },
    { "XMM0_DW0" },
    { "XMM1_DW0" },
    { "XMM2_DW0" },
    { "XMM3_DW0" },
    { "XMM4_DW0" },
    { "XMM5_DW0" },
    { "XMM6_DW0" },
    { "XMM7_DW0" },
    { "XMM8_DW0" },
    { "XMM9_DW0" },
    { "XMM10_DW0" },
    { "XMM11_DW0" },
    { "XMM12_DW0" },
    { "XMM13_DW0" },
    { "XMM14_DW0" },
    { "XMM15_DW0" },
    { "YMM0" },
    { "YMM1" },
    { "YMM2" },
    { "YMM3" },
    { "YMM4" },
    { "YMM5" },
    { "YMM6" },
    { "YMM7" },
    { "YMM8" },
    { "YMM9" },
    { "YMM10" },
    { "YMM11" },
    { "YMM12" },
    { "YMM13" },
    { "YMM14" },
    { "YMM15" },

    { "VALXCPT" },
};
AssertCompile(RT_ELEMENTS(g_aszBs3Cg1DstFields) == BS3CG1DST_END);

#endif

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

static const uint16_t g_afPfxKindToIgnoredFlags[BS3CG1PFXKIND_END] =
{
    /* [BS3CG1PFXKIND_INVALID] = */              UINT16_MAX,
    /* [BS3CG1PFXKIND_MODRM] = */                0,
    /* [BS3CG1PFXKIND_MODRM_NO_OP_SIZES] = */    BS3CG1_PF_OZ | BS3CG1_PF_W,
};

#endif


/**
 * Checks if >= 16 byte SSE/AVX alignment are exempted for the exception type.
 *
 * @returns true / false.
 * @param   enmXcptType         The type to check.
 */
static bool Bs3Cg1XcptTypeIsUnaligned(BS3CG1XCPTTYPE enmXcptType)
{
    switch (enmXcptType)
    {
        case BS3CG1XCPTTYPE_4UA:
        case BS3CG1XCPTTYPE_5:
            return true;
        default:
            return false;
    }
}


DECLINLINE(unsigned) Bs3Cg1InsertReqPrefix(PBS3CG1STATE pThis, unsigned offDst)
{
    switch (pThis->enmPrefixKind)
    {
        case BS3CG1PFXKIND_REQ_66:
            pThis->abCurInstr[offDst] = 0x66;
            break;
        case BS3CG1PFXKIND_REQ_F2:
            pThis->abCurInstr[offDst] = 0xf2;
            break;
        case BS3CG1PFXKIND_REQ_F3:
            pThis->abCurInstr[offDst] = 0xf3;
            break;
        default:
            return offDst;
    }
    return offDst + 1;
}


DECLINLINE(unsigned) Bs3Cg1InsertOpcodes(PBS3CG1STATE pThis, unsigned offDst)
{
    switch (pThis->cbOpcodes)
    {
        case 4: pThis->abCurInstr[offDst + 3] = pThis->abOpcodes[3];
        case 3: pThis->abCurInstr[offDst + 2] = pThis->abOpcodes[2];
        case 2: pThis->abCurInstr[offDst + 1] = pThis->abOpcodes[1];
        case 1: pThis->abCurInstr[offDst]     = pThis->abOpcodes[0];
            return offDst + pThis->cbOpcodes;

        default:
            BS3_ASSERT(0);
            return 0;
    }
}


/**
 * Cleans up state and context changes made by the encoder.
 *
 * @param   pThis       The state.
 */
static void Bs3Cg1EncodeCleanup(PBS3CG1STATE pThis)
{
    /* Restore the DS registers in the contexts. */
    unsigned iRing = 4;
    while (iRing-- > 0)
        pThis->aInitialCtxs[iRing].ds = pThis->aSavedSegRegs[iRing].ds;

    switch (pThis->enmEncoding)
    {
        /* Most encodings currently doesn't need any special cleaning up. */
        default:
            return;
    }
}


static unsigned Bs3Cfg1EncodeMemMod0Disp(PBS3CG1STATE pThis, bool fAddrOverride, unsigned off, uint8_t iReg,
                                         uint8_t cbOp, uint8_t cbMissalign, BS3CG1OPLOC enmLocation)
{
    pThis->aOperands[pThis->iRmOp].idxField     = BS3CG1DST_INVALID;
    pThis->aOperands[pThis->iRmOp].enmLocation  = enmLocation;
    pThis->aOperands[pThis->iRmOp].cbOp         = cbOp;
    pThis->aOperands[pThis->iRmOp].off          = cbOp + cbMissalign;

    if (   BS3_MODE_IS_16BIT_CODE(pThis->bMode)
        || (fAddrOverride && BS3_MODE_IS_32BIT_CODE(pThis->bMode)) )
    {
        /*
         * 16-bit code doing 16-bit or 32-bit addressing,
         * or 32-bit code doing 16-bit addressing.
         */
        unsigned iRing = 4;
        if (BS3_MODE_IS_RM_OR_V86(pThis->bMode))
            while (iRing-- > 0)
                pThis->aInitialCtxs[iRing].ds = pThis->DataPgFar.sel;
        else
            while (iRing-- > 0)
                pThis->aInitialCtxs[iRing].ds = pThis->DataPgFar.sel | iRing;
        if (!fAddrOverride || BS3_MODE_IS_32BIT_CODE(pThis->bMode))
        {
            pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, iReg, 6 /*disp16*/);
            *(uint16_t *)&pThis->abCurInstr[off] = pThis->DataPgFar.off + X86_PAGE_SIZE - cbOp - cbMissalign;
            off += 2;
        }
        else
        {
            pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, iReg, 5 /*disp32*/);
            *(uint32_t *)&pThis->abCurInstr[off] = pThis->DataPgFar.off + X86_PAGE_SIZE - cbOp - cbMissalign;
            off += 4;
        }
    }
    else
    {
        /*
         * 32-bit code doing 32-bit addressing,
         * or 64-bit code doing either 64-bit or 32-bit addressing.
         */
        pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, iReg, 5 /*disp32*/);
        *(uint32_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - cbOp - cbMissalign;

        /* In 64-bit mode we always have a rip relative encoding regardless of fAddrOverride. */
        if (BS3_MODE_IS_64BIT_CODE(pThis->bMode))
            *(uint32_t *)&pThis->abCurInstr[off] -= BS3_FP_OFF(&pThis->pbCodePg[X86_PAGE_SIZE]);
        off += 4;
    }

    /*
     * Fill the memory with 0xcc.
     */
    switch (cbOp + cbMissalign)
    {
        case 8: pThis->pbDataPg[X86_PAGE_SIZE - 8] = 0xcc;  /* fall thru */
        case 7: pThis->pbDataPg[X86_PAGE_SIZE - 7] = 0xcc;  /* fall thru */
        case 6: pThis->pbDataPg[X86_PAGE_SIZE - 6] = 0xcc;  /* fall thru */
        case 5: pThis->pbDataPg[X86_PAGE_SIZE - 5] = 0xcc;  /* fall thru */
        case 4: pThis->pbDataPg[X86_PAGE_SIZE - 4] = 0xcc;  /* fall thru */
        case 3: pThis->pbDataPg[X86_PAGE_SIZE - 3] = 0xcc;  /* fall thru */
        case 2: pThis->pbDataPg[X86_PAGE_SIZE - 2] = 0xcc;  /* fall thru */
        case 1: pThis->pbDataPg[X86_PAGE_SIZE - 1] = 0xcc;  /* fall thru */
        case 0: break;
        default:
            Bs3MemSet(&pThis->pbDataPg[X86_PAGE_SIZE - cbOp - cbMissalign], 0xcc, cbOp - cbMissalign);
            break;
    }

    return off;
}


/**
 * Encodes the next instruction.
 *
 * @returns Next iEncoding value.  Returns @a iEncoding unchanged to indicate
 *          that there are no more encodings to test.
 * @param   pThis       The state.
 * @param   iEncoding   The encoding to produce.  Meaning is specific to each
 *                      BS3CG1ENC_XXX value and should be considered internal.
 */
static unsigned Bs3Cg1EncodeNext(PBS3CG1STATE pThis, unsigned iEncoding)
{
    unsigned off;
    unsigned cbOp;

    pThis->bAlignmentXcpt = UINT8_MAX;

    switch (pThis->enmEncoding)
    {
        case BS3CG1ENC_MODRM_Eb_Gb:
            /* Start by reg,reg encoding. */
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xAX, X86_GREG_xCX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_AL;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_CL;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_CH;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, X86_GREG_xBP, 1, 0, BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 2 && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_BH;
                pThis->abCurInstr[0] = P_AZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, true, off, X86_GREG_xDI, 1, 0, BS3CG1OPLOC_MEM_RW);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Gb_Eb:
            /* Start by reg,reg encoding. */
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xAX, X86_GREG_xCX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_AL;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_CL;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_CH;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, X86_GREG_xBP, 1, 0, BS3CG1OPLOC_MEM);
            }
            else if (iEncoding == 2 && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_BH;
                pThis->abCurInstr[0] = P_AZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, true, off, X86_GREG_xDI, 1, 0, BS3CG1OPLOC_MEM);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Gv_Ev:
        case BS3CG1ENC_MODRM_Ev_Gv:
            if (iEncoding == 0)
            {
                cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 2 : 4;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xBX, X86_GREG_xDX);
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_OZ_RBX;
                pThis->aOperands[pThis->iRmOp ].idxField    = BS3CG1DST_OZ_RDX;
            }
            else if (iEncoding == 1)
            {
                cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 2 : 4;
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_OZ_RBP;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, X86_GREG_xBP, cbOp, 0,
                                               pThis->enmEncoding == BS3CG1ENC_MODRM_Gv_Ev ? BS3CG1OPLOC_MEM : BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 2 && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
            {
                cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 4 : 2;
                pThis->abCurInstr[0] = P_OZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xBX, X86_GREG_xDX);
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_OZ_RBX;
                pThis->aOperands[pThis->iRmOp ].idxField    = BS3CG1DST_OZ_RDX;
                pThis->aOperands[pThis->iRmOp ].enmLocation = BS3CG1OPLOC_CTX;
            }
            else if (iEncoding == 3)
            {
                cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 4 : 2;
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_OZ_RSI;
                pThis->abCurInstr[0] = P_OZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, X86_GREG_xSI, cbOp, 0,
                                               pThis->enmEncoding == BS3CG1ENC_MODRM_Gv_Ev ? BS3CG1OPLOC_MEM : BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 4)
            {
                cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 2 : 4;
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_OZ_RDI;
                pThis->abCurInstr[0] = P_AZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, true, off, X86_GREG_xDI, cbOp, 0,
                                               pThis->enmEncoding == BS3CG1ENC_MODRM_Gv_Ev ? BS3CG1OPLOC_MEM : BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 5)
            {
                cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 4 : 2;
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_OZ_RSI;
                pThis->abCurInstr[0] = P_OZ;
                pThis->abCurInstr[1] = P_AZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 2));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, true, off, X86_GREG_xSI, cbOp, 0,
                                               pThis->enmEncoding == BS3CG1ENC_MODRM_Gv_Ev ? BS3CG1OPLOC_MEM : BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 6 && BS3_MODE_IS_64BIT_CODE(pThis->bMode))
            {
                cbOp = 8;
                off = Bs3Cg1InsertReqPrefix(pThis, 0);
                pThis->abCurInstr[off++] = REX_W___;
                off = Bs3Cg1InsertOpcodes(pThis, off);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xBX, X86_GREG_xDX);
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_RBX;
                pThis->aOperands[pThis->iRmOp ].idxField    = BS3CG1DST_RDX;
                pThis->aOperands[pThis->iRmOp ].enmLocation = BS3CG1OPLOC_CTX;
            }
            else
                break;
            pThis->aOperands[0].cbOp = cbOp;
            pThis->aOperands[1].cbOp = cbOp;
            pThis->cbOperand  = cbOp;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Wss_Vss:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 1, 0);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM0_DW0;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM1_DW0;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2_DW0;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 2 /*iReg*/, 4, 0, BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 2)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM3_DW0;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 3 /*iReg*/, 4, 1 /*cbMissalign*/, BS3CG1OPLOC_MEM_RW);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Wsd_Vsd:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 1, 0);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM0_LO;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM1_LO;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2_LO;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 2 /*iReg*/, 8, 0, BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 2)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM3_LO;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 3 /*iReg*/, 8, 1 /*cbMissalign*/, BS3CG1OPLOC_MEM_RW);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Wps_Vps:
        case BS3CG1ENC_MODRM_Wpd_Vpd:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 1, 0);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM0;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM1;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 2 /*iReg*/, 16, 0, BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 2)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM3;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 3 /*iReg*/, 16, 1 /*cbMissalign*/, BS3CG1OPLOC_MEM_RW);
                if (!Bs3Cg1XcptTypeIsUnaligned(pThis->enmXcptType))
                    pThis->bAlignmentXcpt = X86_XCPT_GP;
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_WqZxReg_Vq:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 1, 0);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM0_LO_ZX;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM1_LO;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2_LO;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 2 /*iReg*/, 8, 0, BS3CG1OPLOC_MEM_RW);
            }
            else if (iEncoding == 2)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM3_LO;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 3 /*iReg*/, 8, 1 /*cbMissalign*/, BS3CG1OPLOC_MEM_RW);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Vq_UqHi:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 1, 0);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM0_HI;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM1_LO;
            }
            else if (iEncoding == 1)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 2, 2);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM2_HI;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2_LO;
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Vq_Mq:
            if (iEncoding == 0)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2_LO;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 2 /*iReg*/, 8, 0, BS3CG1OPLOC_MEM);
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM3_LO;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 3 /*iReg*/, 8, 1 /*cbMissalign*/, BS3CG1OPLOC_MEM);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Vdq_Wdq:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 1, 0);
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_XMM0;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM1;
            }
            else if (iEncoding == 1)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM2;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 2 /*iReg*/, 16, 0, BS3CG1OPLOC_MEM);
            }
            else if (iEncoding == 2)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_XMM3;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, 3 /*iReg*/, 16, 1 /*cbMissalign*/, BS3CG1OPLOC_MEM);
                if (!Bs3Cg1XcptTypeIsUnaligned(pThis->enmXcptType))
                    pThis->bAlignmentXcpt = X86_XCPT_GP;
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Gv_Ma:
            cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 2 : 4;
            if (iEncoding == 0)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBP;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, X86_GREG_xBP, cbOp * 2, 0, BS3CG1OPLOC_MEM);
            }
            else if (iEncoding == 1 && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
            {
                cbOp = cbOp == 2 ? 4 : 2;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBP;
                pThis->abCurInstr[0] = P_OZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off, X86_GREG_xBP, cbOp * 2, 0, BS3CG1OPLOC_MEM);
            }
            else if (iEncoding == 2)
            {
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBP;
                pThis->abCurInstr[0] = P_AZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, true, off, X86_GREG_xBP, cbOp * 2, 0, BS3CG1OPLOC_MEM);
            }
            else if (iEncoding == 3)
            {
                cbOp = cbOp == 2 ? 4 : 2;
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBP;
                pThis->abCurInstr[0] = P_AZ;
                pThis->abCurInstr[1] = P_OZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 2));
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, true, off, X86_GREG_xBP, cbOp * 2, 0, BS3CG1OPLOC_MEM);
            }
            else
                break;
            pThis->aOperands[pThis->iRegOp].cbOp = cbOp;
            pThis->cbOperand  = cbOp;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_MbRO:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0)) - 1;
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off,
                                               (pThis->abCurInstr[off] & X86_MODRM_REG_MASK) >> X86_MODRM_REG_SHIFT,
                                               1, 0, BS3CG1OPLOC_MEM);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_MdWO:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0)) - 1;
                off = Bs3Cfg1EncodeMemMod0Disp(pThis, false, off,
                                               (pThis->abCurInstr[off] & X86_MODRM_REG_MASK) >> X86_MODRM_REG_SHIFT,
                                               4, 0, BS3CG1OPLOC_MEM_RW);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_FIXED:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->cbCurInstr = off;
                iEncoding++;
            }
            break;

        case BS3CG1ENC_FIXED_AL_Ib:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->aOperands[1].off = (uint8_t)off;
                pThis->abCurInstr[off++] = 0xff;
                pThis->cbCurInstr = off;
                iEncoding++;
            }
            break;

        case BS3CG1ENC_FIXED_rAX_Iz:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 0));
                pThis->aOperands[1].off = (uint8_t)off;
                if (BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                {
                    *(uint16_t *)&pThis->abCurInstr[off] = UINT16_MAX;
                    off += 2;
                    pThis->aOperands[0].cbOp = 2;
                    pThis->aOperands[1].cbOp = 2;
                    pThis->cbOperand         = 2;
                }
                else
                {
                    *(uint32_t *)&pThis->abCurInstr[off] = UINT32_MAX;
                    off += 4;
                    pThis->aOperands[0].cbOp = 4;
                    pThis->aOperands[1].cbOp = 4;
                    pThis->cbOperand         = 4;
                }
            }
            else if (iEncoding == 1 && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
            {
                pThis->abCurInstr[0] = P_OZ;
                off = Bs3Cg1InsertOpcodes(pThis, Bs3Cg1InsertReqPrefix(pThis, 1));
                pThis->aOperands[1].off = (uint8_t)off;
                if (!BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                {
                    *(uint16_t *)&pThis->abCurInstr[off] = UINT16_MAX;
                    off += 2;
                    pThis->aOperands[0].cbOp = 2;
                    pThis->aOperands[1].cbOp = 2;
                    pThis->cbOperand         = 2;
                }
                else
                {
                    *(uint32_t *)&pThis->abCurInstr[off] = UINT32_MAX;
                    off += 4;
                    pThis->aOperands[0].cbOp = 4;
                    pThis->aOperands[1].cbOp = 4;
                    pThis->cbOperand         = 4;
                }
            }
            else if (iEncoding == 2 && BS3_MODE_IS_64BIT_CODE(pThis->bMode))
            {
                off = Bs3Cg1InsertReqPrefix(pThis, 0);
                pThis->abCurInstr[off++] = REX_W___;
                off = Bs3Cg1InsertOpcodes(pThis, off);
                pThis->aOperands[1].off = (uint8_t)off;
                *(uint32_t *)&pThis->abCurInstr[off] = UINT32_MAX;
                off += 4;
                pThis->aOperands[0].cbOp = 8;
                pThis->aOperands[1].cbOp = 4;
                pThis->cbOperand         = 8;
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_MOD_EQ_3:
            if (iEncoding < 8)
            {
                off = Bs3Cg1InsertReqPrefix(pThis, 0);
                off = Bs3Cg1InsertOpcodes(pThis, off);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, iEncoding, 1);
            }
            else if (iEncoding < 16)
            {
                off = Bs3Cg1InsertReqPrefix(pThis, 0);
                off = Bs3Cg1InsertOpcodes(pThis, off);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, 0, iEncoding);
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_MOD_NE_3:
            if (iEncoding < 3)
            {
                off = Bs3Cg1InsertReqPrefix(pThis, 0);
                off = Bs3Cg1InsertOpcodes(pThis, off);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(iEncoding, 0, 1);
                if (iEncoding >= 1)
                    pThis->abCurInstr[off++] = 0x7f;
                if (iEncoding == 2)
                {
                    pThis->abCurInstr[off++] = 0x5f;
                    if (!BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                    {
                        pThis->abCurInstr[off++] = 0x3f;
                        pThis->abCurInstr[off++] = 0x1f;
                    }
                }
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        default:
            Bs3TestFailedF("Internal error! BS3CG1ENC_XXX = %u not implemented", pThis->enmEncoding);
            break;
    }


    return iEncoding;
}


/**
 * Prepares doing instruction encodings.
 *
 * This is in part specific to how the instruction is encoded, but generally it
 * sets up basic operand values that doesn't change (much) when Bs3Cg1EncodeNext
 * is called from within the loop.
 *
 * @returns Success indicator (true/false).
 * @param   pThis       The state.
 */
static bool Bs3Cg1EncodePrep(PBS3CG1STATE pThis)
{
    unsigned iRing = 4;
    while (iRing-- > 0)
        pThis->aSavedSegRegs[iRing].ds = pThis->aInitialCtxs[iRing].ds;

    pThis->iRmOp            = RT_ELEMENTS(pThis->aOperands) - 1;
    pThis->iRegOp           = RT_ELEMENTS(pThis->aOperands) - 1;
    pThis->fSameRingNotOkay = false;
    pThis->cbOperand        = 0;

    switch (pThis->enmEncoding)
    {
        case BS3CG1ENC_MODRM_Eb_Gb:
            pThis->iRmOp             = 0;
            pThis->iRegOp            = 1;
            pThis->aOperands[0].cbOp = 1;
            pThis->aOperands[1].cbOp = 1;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Ev_Gv:
            pThis->iRmOp             = 0;
            pThis->iRegOp            = 1;
            pThis->cbOperand         = 2;
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 2;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Gb_Eb:
            pThis->iRmOp             = 1;
            pThis->iRegOp            = 0;
            pThis->aOperands[0].cbOp = 1;
            pThis->aOperands[1].cbOp = 1;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Gv_Ev:
            pThis->iRmOp             = 1;
            pThis->iRegOp            = 0;
            pThis->cbOperand         = 2;
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 2;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Gv_Ma:
            pThis->iRmOp             = 1;
            pThis->iRegOp            = 0;
            pThis->cbOperand         = 2;
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 4;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_MEM;
            pThis->aOperands[1].idxField    = BS3CG1DST_INVALID;
            break;

        case BS3CG1ENC_MODRM_Wss_Vss:
            pThis->iRmOp             = 0;
            pThis->iRegOp            = 1;
            pThis->aOperands[0].cbOp = 4;
            pThis->aOperands[1].cbOp = 4;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Wsd_Vsd:
        case BS3CG1ENC_MODRM_WqZxReg_Vq:
            pThis->iRmOp             = 0;
            pThis->iRegOp            = 1;
            pThis->aOperands[0].cbOp = 8;
            pThis->aOperands[1].cbOp = 8;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Wps_Vps:
        case BS3CG1ENC_MODRM_Wpd_Vpd:
            pThis->iRmOp             = 0;
            pThis->iRegOp            = 1;
            pThis->aOperands[0].cbOp = 16;
            pThis->aOperands[1].cbOp = 16;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Vdq_Wdq:
            pThis->iRmOp             = 1;
            pThis->iRegOp            = 0;
            pThis->aOperands[0].cbOp = 16;
            pThis->aOperands[1].cbOp = 16;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Vq_UqHi:
            pThis->iRmOp             = 1;
            pThis->iRegOp            = 0;
            pThis->aOperands[0].cbOp = 8;
            pThis->aOperands[1].cbOp = 8;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Vq_Mq:
            pThis->iRmOp             = 1;
            pThis->iRegOp            = 0;
            pThis->aOperands[0].cbOp = 8;
            pThis->aOperands[1].cbOp = 8;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_MEM;
            break;

        case BS3CG1ENC_MODRM_MbRO:
            pThis->iRmOp             = 0;
            pThis->aOperands[0].cbOp = 1;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_MEM;
            break;

        case BS3CG1ENC_MODRM_MdWO:
            pThis->iRmOp             = 0;
            pThis->aOperands[0].cbOp = 4;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_MEM_RW;
            break;

        case BS3CG1ENC_FIXED:
            /* nothing to do here */
            break;

        case BS3CG1ENC_FIXED_AL_Ib:
            pThis->aOperands[0].cbOp = 1;
            pThis->aOperands[1].cbOp = 1;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_IMM;
            pThis->aOperands[0].idxField    = BS3CG1DST_AL;
            pThis->aOperands[1].idxField    = BS3CG1DST_INVALID;
            break;

        case BS3CG1ENC_FIXED_rAX_Iz:
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 2;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_IMM;
            pThis->aOperands[0].idxField    = BS3CG1DST_OZ_RAX;
            pThis->aOperands[1].idxField    = BS3CG1DST_INVALID;
            break;

        case BS3CG1ENC_MODRM_MOD_EQ_3:
        case BS3CG1ENC_MODRM_MOD_NE_3:
            /* Unused or invalid instructions mostly. */
            break;

        default:
            return Bs3TestFailedF("Invalid/unimplemented enmEncoding for instruction #%RU32 (%.*s): %d",
                                  pThis->iInstr, pThis->cchMnemonic, pThis->pchMnemonic, pThis->enmEncoding);
    }
    return true;
}


/**
 * Sets up SSE and maybe AVX.
 *
 * @returns true (if successful, false if not and the SSE instructions ends up
 *          being invalid).
 * @param   pThis               The state.
 */
static bool Bs3Cg3SetupSseAndAvx(PBS3CG1STATE pThis)
{
    if (!pThis->fWorkExtCtx)
    {
        unsigned i;
        uint32_t cr0 = ASMGetCR0();
        uint32_t cr4 = ASMGetCR4();

        cr0 &= ~(X86_CR0_TS | X86_CR0_MP | X86_CR0_EM);
        cr0 |= X86_CR0_NE;
        ASMSetCR0(cr0);
        if (pThis->pExtCtx->enmMethod == BS3EXTCTXMETHOD_XSAVE)
        {
            cr4 |= X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT | X86_CR4_OSXSAVE;
            ASMSetCR4(cr4);
            ASMSetXcr0(pThis->pExtCtx->fXcr0);
        }
        else
        {
            cr4 |= X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT;
            ASMSetCR4(cr4);
        }

        for (i = 0; i < RT_ELEMENTS(pThis->aInitialCtxs); i++)
        {
            pThis->aInitialCtxs[i].cr0.u32 = cr0;
            pThis->aInitialCtxs[i].cr4.u32 = cr4;
        }
        pThis->fWorkExtCtx = true;
    }

    return true;
}


/**
 * Next CPU configuration to test the current instruction in.
 *
 * This is for testing FPU, SSE and AVX instructions with the various lazy state
 * load and enable bits in different configurations to ensure we're getting the
 * right response.
 *
 * This also cleans up the CPU and test driver state.
 *
 * @returns true if we're to do another round, false if we're done.
 * @param   pThis           The state.
 * @param   iCpuSetup       The current CPU setup number.
 * @param   pfInvalidInstr  Where to indicate whether the setup causes an
 *                          invalid instruction or not.  This is also used as
 *                          input to avoid unnecessary CPUID work.
 */
static bool Bs3Cg1CpuSetupNext(PBS3CG1STATE pThis, unsigned iCpuSetup, bool *pfInvalidInstr)
{
    if (   (pThis->fFlags & BS3CG1INSTR_F_INVALID_64BIT)
        && BS3_MODE_IS_64BIT_CODE(pThis->bMode))
        return false;

    switch (pThis->enmCpuTest)
    {
        case BS3CG1CPU_ANY:
        case BS3CG1CPU_GE_80186:
        case BS3CG1CPU_GE_80286:
        case BS3CG1CPU_GE_80386:
        case BS3CG1CPU_GE_80486:
        case BS3CG1CPU_GE_Pentium:
        case BS3CG1CPU_CLFSH:
        case BS3CG1CPU_CLFLUSHOPT:
            return false;

        case BS3CG1CPU_SSE:
        case BS3CG1CPU_SSE2:
        case BS3CG1CPU_SSE3:
        case BS3CG1CPU_AVX:
        case BS3CG1CPU_AVX2:
            if (iCpuSetup > 0 || *pfInvalidInstr)
            {
                /** @todo do more configs here. */
                pThis->fWorkExtCtx = false;
                ASMSetCR0(ASMGetCR0() | X86_CR0_EM | X86_CR0_MP);
                ASMSetCR4(ASMGetCR4() & ~(X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT | X86_CR4_OSXSAVE));
                return false;
            }
            return false;

        default:
            Bs3TestFailedF("Invalid enmCpuTest value: %d", pThis->enmCpuTest);
            return false;
    }
}


/**
 * Check if the instruction is supported by the CPU, possibly making state
 * adjustments to enable support for it.
 *
 * @returns true if supported, false if not.
 * @param   pThis               The state.
 */
static bool Bs3Cg1CpuSetupFirst(PBS3CG1STATE pThis)
{
    uint32_t fEax;
    uint32_t fEbx;
    uint32_t fEcx;
    uint32_t fEdx;

    if (   (pThis->fFlags & BS3CG1INSTR_F_INVALID_64BIT)
        && BS3_MODE_IS_64BIT_CODE(pThis->bMode))
        return false;

    switch (pThis->enmCpuTest)
    {
        case BS3CG1CPU_ANY:
            return true;

        case BS3CG1CPU_GE_80186:
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80186)
                return true;
            return false;

        case BS3CG1CPU_GE_80286:
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80286)
                return true;
            return false;

        case BS3CG1CPU_GE_80386:
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
                return true;
            return false;

        case BS3CG1CPU_GE_80486:
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80486)
                return true;
            return false;

        case BS3CG1CPU_GE_Pentium:
            if ((g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_Pentium)
                return true;
            return false;

        case BS3CG1CPU_SSE:
        case BS3CG1CPU_SSE2:
        case BS3CG1CPU_SSE3:
        case BS3CG1CPU_AVX:
            if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
            {
                ASMCpuIdExSlow(1, 0, 0, 0, NULL, NULL, &fEcx, &fEdx);
                switch (pThis->enmCpuTest)
                {
                    case BS3CG1CPU_SSE:
                        if (fEdx & X86_CPUID_FEATURE_EDX_SSE)
                            return Bs3Cg3SetupSseAndAvx(pThis);
                        return false;
                    case BS3CG1CPU_SSE2:
                        if (fEdx & X86_CPUID_FEATURE_EDX_SSE2)
                            return Bs3Cg3SetupSseAndAvx(pThis);
                        return false;
                    case BS3CG1CPU_SSE3:
                        if (fEcx & X86_CPUID_FEATURE_ECX_SSE3)
                            return Bs3Cg3SetupSseAndAvx(pThis);
                        return false;
                    case BS3CG1CPU_AVX:
                        if (fEcx & X86_CPUID_FEATURE_ECX_AVX)
                            return Bs3Cg3SetupSseAndAvx(pThis);
                        return false;
                    default: BS3_ASSERT(0); /* impossible */
                }
            }
            return false;

        case BS3CG1CPU_AVX2:
            if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
            {
                ASMCpuIdExSlow(7, 0, 0/*leaf*/, 0, &fEax, &fEbx, &fEcx, &fEdx);
                switch (pThis->enmCpuTest)
                {
                    case BS3CG1CPU_AVX2:
                        if (fEbx & X86_CPUID_STEXT_FEATURE_EBX_AVX2)
                            return Bs3Cg3SetupSseAndAvx(pThis);
                        return false;
                    default: BS3_ASSERT(0); return false; /* impossible */
                }
            }
            return false;

        case BS3CG1CPU_CLFSH:
            if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
            {
                ASMCpuIdExSlow(1, 0, 0, 0, NULL, NULL, NULL, &fEdx);
                if (fEdx & X86_CPUID_FEATURE_EDX_CLFSH)
                    return true;
            }
            return false;

        case BS3CG1CPU_CLFLUSHOPT:
            if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
            {
                ASMCpuIdExSlow(7, 0, 0/*leaf*/, 0, NULL, &fEbx, NULL, NULL);
                if (fEbx & X86_CPUID_STEXT_FEATURE_EBX_CLFLUSHOPT)
                    return true;
            }
            return false;

        default:
            Bs3TestFailedF("Invalid enmCpuTest value: %d", pThis->enmCpuTest);
            return false;
    }
}



/**
 * Checks the preconditions for a test.
 *
 * @returns true if the test be executed, false if not.
 * @param   pThis       The state.
 * @param   pHdr        The test header.
 */
static bool Bs3Cg1RunSelector(PBS3CG1STATE pThis, PCBS3CG1TESTHDR pHdr)
{

    uint8_t const BS3_FAR *pbCode = (uint8_t const BS3_FAR *)(pHdr + 1);
    unsigned cbLeft = pHdr->cbSelector;
    while (cbLeft-- > 0)
    {
        switch (*pbCode++)
        {
#define CASE_PRED(a_Pred, a_Expr) \
            case ((a_Pred) << BS3CG1SEL_OP_KIND_MASK) | BS3CG1SEL_OP_IS_TRUE: \
                if (!(a_Expr)) return false; \
                break; \
            case ((a_Pred) << BS3CG1SEL_OP_KIND_MASK) | BS3CG1SEL_OP_IS_FALSE: \
                if (a_Expr) return false; \
                break
            CASE_PRED(BS3CG1PRED_SIZE_O16, pThis->cbOperand == 2);
            CASE_PRED(BS3CG1PRED_SIZE_O32, pThis->cbOperand == 4);
            CASE_PRED(BS3CG1PRED_SIZE_O64, pThis->cbOperand == 8);
            CASE_PRED(BS3CG1PRED_RING_0, pThis->uCpl == 0);
            CASE_PRED(BS3CG1PRED_RING_1, pThis->uCpl == 1);
            CASE_PRED(BS3CG1PRED_RING_2, pThis->uCpl == 2);
            CASE_PRED(BS3CG1PRED_RING_3, pThis->uCpl == 3);
            CASE_PRED(BS3CG1PRED_RING_0_THRU_2, pThis->uCpl <= 2);
            CASE_PRED(BS3CG1PRED_RING_1_THRU_3, pThis->uCpl >= 1);
            CASE_PRED(BS3CG1PRED_CODE_64BIT, BS3_MODE_IS_64BIT_CODE(pThis->bMode));
            CASE_PRED(BS3CG1PRED_CODE_32BIT, BS3_MODE_IS_32BIT_CODE(pThis->bMode));
            CASE_PRED(BS3CG1PRED_CODE_16BIT, BS3_MODE_IS_16BIT_CODE(pThis->bMode));
            CASE_PRED(BS3CG1PRED_MODE_REAL,  BS3_MODE_IS_RM_SYS(pThis->bMode));
            CASE_PRED(BS3CG1PRED_MODE_PROT,  BS3_MODE_IS_PM_SYS(pThis->bMode));
            CASE_PRED(BS3CG1PRED_MODE_LONG,  BS3_MODE_IS_64BIT_SYS(pThis->bMode));
            CASE_PRED(BS3CG1PRED_MODE_SMM,   false);
            CASE_PRED(BS3CG1PRED_MODE_VMX,   false);
            CASE_PRED(BS3CG1PRED_MODE_SVM,   false);
            CASE_PRED(BS3CG1PRED_PAGING_ON,  BS3_MODE_IS_PAGED(pThis->bMode));
            CASE_PRED(BS3CG1PRED_PAGING_OFF, !BS3_MODE_IS_PAGED(pThis->bMode));

#undef CASE_PRED
            default:
                return Bs3TestFailedF("Invalid selector opcode %#x!", pbCode[-1]);
        }
    }

    return true;
}


#ifdef BS3CG1_DEBUG_CTX_MOD
/**
 * Translates the operator into a string.
 *
 * @returns Read-only string pointer.
 * @param   bOpcode             The context modifier program opcode.
 */
static const char BS3_FAR *Bs3Cg1CtxOpToString(uint8_t bOpcode)
{
    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
    {
        case BS3CG1_CTXOP_ASSIGN:   return "=";
        case BS3CG1_CTXOP_OR:       return "|=";
        case BS3CG1_CTXOP_AND:      return "&=";
        case BS3CG1_CTXOP_AND_INV:  return "&~=";
    }
}
#endif


/**
 * Runs a context modifier program.
 *
 * @returns Success indicator (true/false).
 * @param   pThis       The state.
 * @param   pCtx        The context.
 * @param   pHdr        The program header.
 * @param   off         The program offset relative to the end of the header.
 * @param   cb          The program size.
 * @param   pEflCtx     The context to take undefined EFLAGS from.  (This is NULL
 *                      if we're processing a input context modifier program.)
 * @param   pbInstr     Points to the first instruction byte.  For storing
 *                      immediate operands during input context modification.
 *                      NULL for output contexts.
 */
static bool Bs3Cg1RunContextModifier(PBS3CG1STATE pThis, PBS3REGCTX pCtx, PCBS3CG1TESTHDR pHdr, unsigned off, unsigned cb,
                                     PCBS3REGCTX pEflCtx, uint8_t BS3_FAR *pbInstr)
{
    uint8_t const BS3_FAR *pbCode = (uint8_t const BS3_FAR *)(pHdr + 1) + off;
    int                    cbLeft = cb;
    while (cbLeft-- > 0)
    {
        /*
         * Decode the instruction.
         */
        uint8_t const   bOpcode = *pbCode++;
        unsigned        cbValue;
        unsigned        cbDst;
        BS3CG1DST       idxField;
        BS3PTRUNION     PtrField;

        /* Expand the destiation field (can be escaped). */
        switch (bOpcode & BS3CG1_CTXOP_DST_MASK)
        {
            case BS3CG1_CTXOP_OP1:
                idxField = pThis->aOperands[0].idxField;
                if (idxField == BS3CG1DST_INVALID)
                    idxField = BS3CG1DST_OP1;
                break;

            case BS3CG1_CTXOP_OP2:
                idxField = pThis->aOperands[1].idxField;
                if (idxField == BS3CG1DST_INVALID)
                    idxField = BS3CG1DST_OP2;
                break;

            case BS3CG1_CTXOP_EFL:
                idxField = BS3CG1DST_EFL;
                break;

            case BS3CG1_CTXOP_DST_ESC:
                if (cbLeft-- > 0)
                {
                    idxField = (BS3CG1DST)*pbCode++;
                    if (idxField <= BS3CG1DST_OP4)
                    {
                        if (idxField > BS3CG1DST_INVALID)
                        {
                            uint8_t idxField2 = pThis->aOperands[idxField - BS3CG1DST_OP1].idxField;
                            if (idxField2 != BS3CG1DST_INVALID)
                                idxField = idxField2;
                            break;
                        }
                    }
                    else if (idxField < BS3CG1DST_END)
                        break;
                    return Bs3TestFailedF("Malformed context instruction: idxField=%d", idxField);
                }
                /* fall thru */
            default:
                return Bs3TestFailed("Malformed context instruction: Destination");
        }


        /* Expand value size (can be escaped). */
        switch (bOpcode & BS3CG1_CTXOP_SIZE_MASK)
        {
            case BS3CG1_CTXOP_1_BYTE:   cbValue =  1; break;
            case BS3CG1_CTXOP_2_BYTES:  cbValue =  2; break;
            case BS3CG1_CTXOP_4_BYTES:  cbValue =  4; break;
            case BS3CG1_CTXOP_8_BYTES:  cbValue =  8; break;
            case BS3CG1_CTXOP_16_BYTES: cbValue = 16; break;
            case BS3CG1_CTXOP_32_BYTES: cbValue = 32; break;
            case BS3CG1_CTXOP_12_BYTES: cbValue = 12; break;
            case BS3CG1_CTXOP_SIZE_ESC:
                if (cbLeft-- > 0)
                {
                    cbValue = *pbCode++;
                    if (cbValue)
                        break;
                }
                /* fall thru */
            default:
                return Bs3TestFailed("Malformed context instruction: size");
        }

        /* Make sure there is enough instruction bytes for the value. */
        if (cbValue <= cbLeft)
        { /* likely */ }
        else
            return Bs3TestFailedF("Malformed context instruction: %u bytes value, %u bytes left", cbValue, cbLeft);

        /*
         * Do value processing specific to the target field size.
         */
        cbDst = g_acbBs3Cg1DstFields[idxField];
        if (cbDst == BS3CG1DSTSIZE_OPERAND)
            cbDst = pThis->aOperands[idxField - BS3CG1DST_OP1].cbOp;
        else if (cbDst == BS3CG1DSTSIZE_OPERAND_SIZE_GRP)
            cbDst = pThis->cbOperand;
        if (cbDst <= 8)
        {
            unsigned const offField = g_aoffBs3Cg1DstFields[idxField];

            /*
             * Deal with fields up to 8-byte wide.
             */
            /* Get the value. */
            uint64_t uValue;
            if ((bOpcode & BS3CG1_CTXOP_SIGN_EXT))
                switch (cbValue)
                {
                    case 1: uValue = *(int8_t   const BS3_FAR *)pbCode; break;
                    case 2: uValue = *(int16_t  const BS3_FAR *)pbCode; break;
                    case 4: uValue = *(int32_t  const BS3_FAR *)pbCode; break;
                    default:
                        if (cbValue >= 8)
                        {
                            uValue = *(uint64_t const BS3_FAR *)pbCode;
                            break;
                        }
                        return Bs3TestFailedF("Malformed context instruction: %u bytes value (%u dst)", cbValue, cbDst);
                }
            else
                switch (cbValue)
                {
                    case 1: uValue = *(uint8_t  const BS3_FAR *)pbCode; break;
                    case 2: uValue = *(uint16_t const BS3_FAR *)pbCode; break;
                    case 4: uValue = *(uint32_t const BS3_FAR *)pbCode; break;
                    default:
                        if (cbValue >= 8)
                        {
                            uValue = *(uint64_t const BS3_FAR *)pbCode;
                            break;
                        }
                        return Bs3TestFailedF("Malformed context instruction: %u bytes value (%u dst)", cbValue, cbDst);
                }

            /* Find the field. */
            if (offField < sizeof(BS3REGCTX))
                PtrField.pu8 = (uint8_t BS3_FAR *)pCtx + offField;
            /* Non-register operands: */
            else if ((unsigned)(idxField - BS3CG1DST_OP1) < 4U)
            {
                unsigned const idxOp = idxField - BS3CG1DST_OP1;

                switch (pThis->aOperands[idxOp].enmLocation)
                {
                    case BS3CG1OPLOC_IMM:
                        if (pbInstr)
                            PtrField.pu8 = &pbInstr[pThis->aOperands[idxOp].off];
                        else
                            return Bs3TestFailedF("Immediate operand referenced in output context!");
                        break;

                    case BS3CG1OPLOC_MEM:
                        if (!pbInstr)
                            return Bs3TestFailedF("Read only operand specified in output!");
                        PtrField.pu8 = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[idxOp].off];
                        break;

                    case BS3CG1OPLOC_MEM_RW:
                        if (pbInstr)
                            PtrField.pu8 = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[idxOp].off];
                        else
                            PtrField.pu8 = pThis->MemOp.ab;
                        break;

                    default:
                        return Bs3TestFailedF("Internal error: cbDst=%u idxField=%d (%d) offField=%#x: enmLocation=%u off=%#x idxField=%u",
                                              cbDst, idxField, idxOp, offField, pThis->aOperands[idxOp].enmLocation,
                                              pThis->aOperands[idxOp].off, pThis->aOperands[idxOp].idxField);
                }
            }
            /* Special field: Copying in undefined EFLAGS from the result context. */
            else if (idxField == BS3CG1DST_EFL_UNDEF)
            {
                if (!pEflCtx || (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK) != BS3CG1_CTXOP_ASSIGN)
                    return Bs3TestFailed("Invalid BS3CG1DST_EFL_UNDEF usage");
                PtrField.pu32 = &pCtx->rflags.u32;
                uValue = (*PtrField.pu32 & ~(uint32_t)uValue) | (pEflCtx->rflags.u32 & (uint32_t)uValue);
            }
            /* Special field: Expected value (in/result) exception. */
            else if (idxField == BS3CG1DST_VALUE_XCPT)
            {
                if (!pEflCtx || (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK) != BS3CG1_CTXOP_ASSIGN || cbDst != 1)
                    return Bs3TestFailed("Invalid BS3CG1DST_VALUE_XCPT usage");
                PtrField.pu8 = &pThis->bValueXcpt;
            }
            /* FPU and FXSAVE format. */
            else if (   pThis->pExtCtx->enmMethod != BS3EXTCTXMETHOD_ANCIENT
                     && offField - sizeof(BS3REGCTX) <= RT_UOFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]) )
            {
                if (!pThis->fWorkExtCtx)
                    return Bs3TestFailedF("Extended context disabled: Field %d @ %#x LB %u\n", idxField, offField, cbDst);
                PtrField.pb = (uint8_t *)pThis->pExtCtx + offField - sizeof(BS3REGCTX);
            }
            /** @todo other FPU fields and FPU state formats. */
            else
                return Bs3TestFailedF("Todo implement me: cbDst=%u idxField=%d offField=%#x", cbDst, idxField, offField);

#ifdef BS3CG1_DEBUG_CTX_MOD
            switch (cbDst)
            {
                case 1:
                    BS3CG1_DPRINTF(("dbg: modify %s: %#04RX8 (LB %u) %s %#RX64 (LB %u)\n", g_aszBs3Cg1DstFields[idxField].sz,
                                    *PtrField.pu8, cbDst, Bs3Cg1CtxOpToString(bOpcode), uValue, cbValue));
                    break;
                case 2:
                    BS3CG1_DPRINTF(("dbg: modify %s: %#06RX16 (LB %u) %s %#RX64 (LB %u)\n", g_aszBs3Cg1DstFields[idxField].sz,
                                    *PtrField.pu16, cbDst, Bs3Cg1CtxOpToString(bOpcode), uValue, cbValue));
                    break;
                case 4:
                    BS3CG1_DPRINTF(("dbg: modify %s: %#010RX32 (LB %u) %s %#RX64 (LB %u)\n", g_aszBs3Cg1DstFields[idxField].sz,
                                    *PtrField.pu32, cbDst, Bs3Cg1CtxOpToString(bOpcode), uValue, cbValue));
                    break;
                default:
                    BS3CG1_DPRINTF(("dbg: modify %s: %#018RX64 (LB %u) %s %#RX64 (LB %u)\n", g_aszBs3Cg1DstFields[idxField].sz,
                                    *PtrField.pu64, cbDst, Bs3Cg1CtxOpToString(bOpcode), uValue, cbValue));
                    break;
            }
#endif

            /* Modify the field. */
            switch (cbDst)
            {
                case 1:
                    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                    {
                        case BS3CG1_CTXOP_ASSIGN:   *PtrField.pu8  =  (uint8_t)uValue; break;
                        case BS3CG1_CTXOP_OR:       *PtrField.pu8 |=  (uint8_t)uValue; break;
                        case BS3CG1_CTXOP_AND:      *PtrField.pu8 &=  (uint8_t)uValue; break;
                        case BS3CG1_CTXOP_AND_INV:  *PtrField.pu8 &= ~(uint8_t)uValue; break;
                    }
                    break;

                case 2:
                    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                    {
                        case BS3CG1_CTXOP_ASSIGN:   *PtrField.pu16  =  (uint16_t)uValue; break;
                        case BS3CG1_CTXOP_OR:       *PtrField.pu16 |=  (uint16_t)uValue; break;
                        case BS3CG1_CTXOP_AND:      *PtrField.pu16 &=  (uint16_t)uValue; break;
                        case BS3CG1_CTXOP_AND_INV:  *PtrField.pu16 &= ~(uint16_t)uValue; break;
                    }
                    break;

                case 4:
                    if (offField <= RT_OFFSETOF(BS3REGCTX, r15)) /* Clear the top dword. */
                        PtrField.pu32[1] = 0;
                    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                    {
                        case BS3CG1_CTXOP_ASSIGN:   *PtrField.pu32  =  (uint32_t)uValue; break;
                        case BS3CG1_CTXOP_OR:       *PtrField.pu32 |=  (uint32_t)uValue; break;
                        case BS3CG1_CTXOP_AND:      *PtrField.pu32 &=  (uint32_t)uValue; break;
                        case BS3CG1_CTXOP_AND_INV:  *PtrField.pu32 &= ~(uint32_t)uValue; break;
                    }
                    break;

                case 8:
                    if ((unsigned)(idxField - BS3CG1DST_XMM0_LO_ZX) <= (unsigned)(BS3CG1DST_XMM15_LO_ZX - BS3CG1DST_XMM0_LO_ZX))
                        PtrField.pu64[1] = 0;
                    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                    {
                        case BS3CG1_CTXOP_ASSIGN:   *PtrField.pu64  =  (uint64_t)uValue; break;
                        case BS3CG1_CTXOP_OR:       *PtrField.pu64 |=  (uint64_t)uValue; break;
                        case BS3CG1_CTXOP_AND:      *PtrField.pu64 &=  (uint64_t)uValue; break;
                        case BS3CG1_CTXOP_AND_INV:  *PtrField.pu64 &= ~(uint64_t)uValue; break;
                    }
                    break;

                default:
                    return Bs3TestFailedF("Malformed context instruction: cbDst=%u, expected 1, 2, 4, or 8", cbDst);
            }

#ifdef BS3CG1_DEBUG_CTX_MOD
            switch (cbDst)
            {
                case 1:  BS3CG1_DPRINTF(("dbg:    --> %s: %#04RX8\n",   g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu8));  break;
                case 2:  BS3CG1_DPRINTF(("dbg:    --> %s: %#06RX16\n",  g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu16)); break;
                case 4:  BS3CG1_DPRINTF(("dbg:    --> %s: %#010RX32\n", g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu32)); break;
                default: BS3CG1_DPRINTF(("dbg:    --> %s: %#018RX64\n", g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu64)); break;
            }
#endif

        }
        /*
         * Deal with larger field (FPU, SSE, AVX, ...).
         */
        else
        {
            union
            {
                X86FPUREG   FpuReg;
                X86XMMREG   XmmReg;
                X86YMMREG   YmmReg;
                X86ZMMREG   ZmmReg;
                uint8_t     ab[sizeof(X86ZMMREG)];
                uint32_t    au32[sizeof(X86ZMMREG) / sizeof(uint32_t)];
            } Value;
            unsigned const offField = g_aoffBs3Cg1DstFields[idxField];

            if (!pThis->fWorkExtCtx)
                return Bs3TestFailedF("Extended context disabled: Field %d @ %#x LB %u\n", idxField, offField, cbDst);

            /* Copy the value into the union, doing the zero padding / extending. */
            Bs3MemCpy(&Value, pbCode, cbValue);
            if (cbValue < sizeof(Value))
            {
                if ((bOpcode & BS3CG1_CTXOP_SIGN_EXT) && (Value.ab[cbValue - 1] & 0x80))
                    Bs3MemSet(&Value.ab[cbValue], 0xff, sizeof(Value) - cbValue);
                else
                    Bs3MemSet(&Value.ab[cbValue], 0x00, sizeof(Value) - cbValue);
            }

            /* Optimized access to XMM and STx registers. */
            if (   pThis->pExtCtx->enmMethod != BS3EXTCTXMETHOD_ANCIENT
                && offField - sizeof(BS3REGCTX) <= RT_UOFFSETOF(BS3EXTCTX, Ctx.x87.aXMM[15]) )
                PtrField.pb = (uint8_t *)pThis->pExtCtx + offField - sizeof(BS3REGCTX);
            /* Non-register operands: */
            else if ((unsigned)(idxField - BS3CG1DST_OP1) < 4U)
            {
                unsigned const idxOp = idxField - BS3CG1DST_OP1;
                switch (pThis->aOperands[idxOp].enmLocation)
                {
                    case BS3CG1OPLOC_MEM:
                        if (!pbInstr)
                            return Bs3TestFailedF("Read only operand specified in output!");
                        PtrField.pu8 = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[idxOp].off];
                        break;

                    case BS3CG1OPLOC_MEM_RW:
                        if (pbInstr)
                            PtrField.pu8 = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[idxOp].off];
                        else
                            PtrField.pu8 = pThis->MemOp.ab;
                        break;

                    default:
                        return Bs3TestFailedF("Internal error: Field %d (%d) @ %#x LB %u: enmLocation=%u off=%#x idxField=%u",
                                              idxField, idxOp, offField, cbDst, pThis->aOperands[idxOp].enmLocation,
                                              pThis->aOperands[idxOp].off, pThis->aOperands[idxOp].idxField);
                }
            }
            /* The YMM (AVX) and the first 16 ZMM (AVX512) registers have split storage in
               the state, so they need special handling.  */
            else
            {
                return Bs3TestFailedF("TODO: implement me: cbDst=%d idxField=%d (AVX and other weird state)", cbDst, idxField);
            }

            if (PtrField.pb)
            {
                /* Modify the field / memory. */
                unsigned i;
                if (cbDst & 3)
                    return Bs3TestFailedF("Malformed context instruction: cbDst=%u, multiple of 4", cbDst);

#ifdef BS3CG1_DEBUG_CTX_MOD
                BS3CG1_DPRINTF(("dbg: modify %s: %.*Rhxs (LB %u) %s %.*Rhxs (LB %u)\n", g_aszBs3Cg1DstFields[idxField].sz,
                                cbDst, PtrField.pb, cbDst, Bs3Cg1CtxOpToString(bOpcode), cbValue, Value.ab, cbValue));
#endif

                i = cbDst / 4;
                while (i-- > 0)
                {
                    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                    {
                        case BS3CG1_CTXOP_ASSIGN:   PtrField.pu32[i]  =  Value.au32[i]; break;
                        case BS3CG1_CTXOP_OR:       PtrField.pu32[i] |=  Value.au32[i]; break;
                        case BS3CG1_CTXOP_AND:      PtrField.pu32[i] &=  Value.au32[i]; break;
                        case BS3CG1_CTXOP_AND_INV:  PtrField.pu32[i] &= ~Value.au32[i]; break;
                    }
                }

#ifdef BS3CG1_DEBUG_CTX_MOD
                BS3CG1_DPRINTF(("dbg:    --> %s: %.*Rhxs\n", g_aszBs3Cg1DstFields[idxField].sz, cbDst, PtrField.pb));
#endif
            }
        }

        /*
         * Advance to the next instruction.
         */
        pbCode += cbValue;
        cbLeft -= cbValue;
    }

    return true;
}


/**
 * Checks the result of a run.
 *
 * @returns true if successful, false if not.
 * @param   pThis               The state.
 * @param   fInvalidInstr       Whether this is an invalid instruction.
 * @param   bTestXcptExpected   The exception causing the test code to stop
 *                              executing.
 * @param   iEncoding           For error reporting.
 */
static bool Bs3Cg1CheckResult(PBS3CG1STATE pThis, bool fInvalidInstr, uint8_t bTestXcptExpected, unsigned iEncoding)
{
    unsigned iOperand;

    /*
     * Check the exception state first.
     */
    uint8_t bExpectedXcpt;
    uint8_t cbAdjustPc;
    if (!fInvalidInstr)
    {
        bExpectedXcpt = pThis->bAlignmentXcpt;
        if (bExpectedXcpt == UINT8_MAX)
            bExpectedXcpt = pThis->bValueXcpt;
        if (bExpectedXcpt == UINT8_MAX)
        {
            cbAdjustPc    = pThis->cbCurInstr;
            bExpectedXcpt = bTestXcptExpected;
            if (bTestXcptExpected == X86_XCPT_PF)
                pThis->Ctx.cr2.u = pThis->uCodePgFlat + X86_PAGE_SIZE;
        }
        else
            cbAdjustPc = 0;
    }
    else
    {
        cbAdjustPc = 0;
        bExpectedXcpt = bTestXcptExpected;
    }
    if (RT_LIKELY(   pThis->TrapFrame.bXcpt     == bExpectedXcpt
                  && pThis->TrapFrame.Ctx.rip.u == pThis->Ctx.rip.u + cbAdjustPc))
    {
        /*
         * Check the register content.
         */
        bool fOkay = Bs3TestCheckRegCtxEx(&pThis->TrapFrame.Ctx, &pThis->Ctx,
                                           cbAdjustPc, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/,
                                           pThis->pszMode, iEncoding);

        /*
         * Check memory output operands.
         */
        iOperand = pThis->cOperands;
        while (iOperand-- > 0)
            if (pThis->aOperands[iOperand].enmLocation == BS3CG1OPLOC_MEM_RW)
            {
                BS3PTRUNION PtrUnion;
                PtrUnion.pb = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[iOperand].off];
                switch (pThis->aOperands[iOperand].cbOp)
                {
                    case 1:
                        if (*PtrUnion.pu8 == pThis->MemOp.ab[0])
                            continue;
                        Bs3TestFailedF("op%u: Wrote %#04RX8, expected %#04RX8", iOperand, *PtrUnion.pu8, pThis->MemOp.ab[0]);
                        break;
                    case 2:
                        if (*PtrUnion.pu16 == pThis->MemOp.au16[0])
                            continue;
                        Bs3TestFailedF("op%u: Wrote %#06RX16, expected %#06RX16",
                                       iOperand, *PtrUnion.pu16, pThis->MemOp.au16[0]);
                        break;
                    case 4:
                        if (*PtrUnion.pu32 == pThis->MemOp.au32[0])
                            continue;
                        Bs3TestFailedF("op%u: Wrote %#010RX32, expected %#010RX32",
                                       iOperand, *PtrUnion.pu32, pThis->MemOp.au32[0]);
                        break;
                    case 8:
                        if (*PtrUnion.pu64 == pThis->MemOp.au64[0])
                            continue;
                        Bs3TestFailedF("op%u: Wrote %#018RX64, expected %#018RX64",
                                       iOperand, *PtrUnion.pu64, pThis->MemOp.au64[0]);
                        break;
                    default:
                        if (Bs3MemCmp(PtrUnion.pb, pThis->MemOp.ab, pThis->aOperands[iOperand].cbOp) == 0)
                            continue;
                        Bs3TestFailedF("op%u: Wrote %.*Rhxs, expected %.*Rhxs",
                                       iOperand,
                                       pThis->aOperands[iOperand].cbOp, PtrUnion.pb,
                                       pThis->aOperands[iOperand].cbOp, pThis->MemOp.ab);
                        break;
                }
                fOkay = false;
            }

        /*
         * Check extended context if enabled.
         */
        if (pThis->fWorkExtCtx)
        {
            PBS3EXTCTX pExpect = pThis->pExtCtx;
            PBS3EXTCTX pResult = pThis->pResultExtCtx;
            unsigned   i;
            if (   pExpect->enmMethod == BS3EXTCTXMETHOD_XSAVE
                || pExpect->enmMethod == BS3EXTCTXMETHOD_FXSAVE)
            {
                /* Compare the x87 state, ASSUMING XCR0 bit 1 is set. */
#define CHECK_FIELD(a_Field, a_szFmt) \
    if (pResult->Ctx.a_Field != pExpect->Ctx.a_Field) fOkay = Bs3TestFailedF(a_szFmt, pResult->Ctx.a_Field, pExpect->Ctx.a_Field)
                CHECK_FIELD(x87.FCW, "FCW: %#06x, expected %#06x");
                CHECK_FIELD(x87.FSW, "FSW: %#06x, expected %#06x");
                CHECK_FIELD(x87.FTW, "FTW: %#06x, expected %#06x");
                //CHECK_FIELD(x87.FOP,      "FOP: %#06x, expected %#06x");
                //CHECK_FIELD(x87.FPUIP,    "FPUIP:  %#010x, expected %#010x");
                //CHECK_FIELD(x87.CS,       "FPUCS:  %#06x, expected %#06x");
                //CHECK_FIELD(x87.Rsrvd1,   "Rsrvd1: %#06x, expected %#06x");
                //CHECK_FIELD(x87.DP,       "FPUDP:  %#010x, expected %#010x");
                //CHECK_FIELD(x87.DS,       "FPUDS:  %#06x, expected %#06x");
                //CHECK_FIELD(x87.Rsrvd2,   "Rsrvd2: %#06x, expected %#06x");
                CHECK_FIELD(x87.MXCSR,      "MXCSR:  %#010x, expected %#010x");
#undef CHECK_FIELD
                for (i = 0; i < RT_ELEMENTS(pExpect->Ctx.x87.aRegs); i++)
                    if (   pResult->Ctx.x87.aRegs[i].au64[0] != pExpect->Ctx.x87.aRegs[i].au64[0]
                        || pResult->Ctx.x87.aRegs[i].au16[4] != pExpect->Ctx.x87.aRegs[i].au16[4])
                        fOkay = Bs3TestFailedF("ST[%u]: %c m=%#RX64 e=%d, expected %c m=%#RX64 e=%d", i,
                                               pResult->Ctx.x87.aRegs[i].r80Ex.s.fSign ? '-' : '+',
                                               pResult->Ctx.x87.aRegs[i].r80Ex.s.u64Mantissa,
                                               pResult->Ctx.x87.aRegs[i].r80Ex.s.uExponent,
                                               pExpect->Ctx.x87.aRegs[i].r80Ex.s.fSign ? '-' : '+',
                                               pExpect->Ctx.x87.aRegs[i].r80Ex.s.u64Mantissa,
                                               pExpect->Ctx.x87.aRegs[i].r80Ex.s.uExponent);
                for (i = 0; i < (ARCH_BITS == 64 ? 16 : 8); i++)
                    if (   pResult->Ctx.x87.aXMM[i].au64[0] != pExpect->Ctx.x87.aXMM[i].au64[0]
                        || pResult->Ctx.x87.aXMM[i].au64[1] != pExpect->Ctx.x87.aXMM[i].au64[1])
                        fOkay = Bs3TestFailedF("XMM%u: %#010RX64'%08RX64, expected %#010RX64'%08RX64", i,
                                               pResult->Ctx.x87.aXMM[i].au64[0],
                                               pResult->Ctx.x87.aXMM[i].au64[1],
                                               pExpect->Ctx.x87.aXMM[i].au64[0],
                                               pExpect->Ctx.x87.aXMM[i].au64[1]);
            }
            else
                fOkay = Bs3TestFailedF("Unsupported extended CPU context method: %d", pExpect->enmMethod);
        }

        /*
         * Done.
         */
        if (fOkay)
            return true;

        /*
         * Report failure.
         */
        Bs3TestFailedF("%RU32[%u]: encoding#%u: %.*Rhxs",
                       pThis->iInstr, pThis->iTest, iEncoding, pThis->cbCurInstr, pThis->abCurInstr);
    }
    else
        Bs3TestFailedF("%RU32[%u]: bXcpt=%#x expected %#x; rip=%RX64 expected %RX64; encoding#%u: %.*Rhxs",
                       pThis->iInstr, pThis->iTest,
                       pThis->TrapFrame.bXcpt, bExpectedXcpt,
                       pThis->TrapFrame.Ctx.rip.u, pThis->Ctx.rip.u + cbAdjustPc,
                       iEncoding, pThis->cbCurInstr, pThis->abCurInstr);
    Bs3TestPrintf("cpl=%u cbOperands=%u\n", pThis->uCpl, pThis->cbOperand);

    /*
     * Display memory operands.
     */
    for (iOperand = 0; iOperand < pThis->cOperands; iOperand++)
    {
        BS3PTRUNION PtrUnion;
        switch (pThis->aOperands[iOperand].enmLocation)
        {
            case BS3CG1OPLOC_CTX:
            {
                uint8_t  idxField = pThis->aOperands[iOperand].idxField;
                unsigned offField = g_aoffBs3Cg1DstFields[idxField];
                if (offField <= sizeof(BS3REGCTX))
                    PtrUnion.pb = (uint8_t BS3_FAR *)&pThis->Ctx + offField;
                else
                {
                    Bs3TestPrintf("op%u: ctx%u: xxxx\n", iOperand, pThis->aOperands[iOperand].cbOp * 8);
                    break;
                }
                switch (pThis->aOperands[iOperand].cbOp)
                {
                    case 1: Bs3TestPrintf("op%u: ctx08: %#04RX8\n", iOperand, *PtrUnion.pu8); break;
                    case 2: Bs3TestPrintf("op%u: ctx16: %#06RX16\n", iOperand, *PtrUnion.pu16); break;
                    case 4: Bs3TestPrintf("op%u: ctx32: %#010RX32\n", iOperand, *PtrUnion.pu32); break;
                    case 8: Bs3TestPrintf("op%u: ctx64: %#018RX64\n", iOperand, *PtrUnion.pu64); break;
                    default:
                        Bs3TestPrintf("op%u: ctx%u: %.*Rhxs\n", iOperand, pThis->aOperands[iOperand].cbOp * 8,
                                      pThis->aOperands[iOperand].cbOp, PtrUnion.pb);
                        break;
                }
                break;
            }

            case BS3CG1OPLOC_IMM:
                PtrUnion.pb = &pThis->pbCodePg[pThis->aOperands[iOperand].off];
                switch (pThis->aOperands[iOperand].cbOp)
                {
                    case 1: Bs3TestPrintf("op%u: imm08: %#04RX8\n", iOperand, *PtrUnion.pu8); break;
                    case 2: Bs3TestPrintf("op%u: imm16: %#06RX16\n", iOperand, *PtrUnion.pu16); break;
                    case 4: Bs3TestPrintf("op%u: imm32: %#010RX32\n", iOperand, *PtrUnion.pu32); break;
                    case 8: Bs3TestPrintf("op%u: imm64: %#018RX64\n", iOperand, *PtrUnion.pu64); break;
                    default:
                        Bs3TestPrintf("op%u: imm%u: %.*Rhxs\n", iOperand, pThis->aOperands[iOperand].cbOp * 8,
                                      pThis->aOperands[iOperand].cbOp, PtrUnion.pb);
                        break;
                }
                break;

            case BS3CG1OPLOC_MEM:
            case BS3CG1OPLOC_MEM_RW:
                PtrUnion.pb = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[iOperand].off];
                switch (pThis->aOperands[iOperand].cbOp)
                {
                    case 1: Bs3TestPrintf("op%u: result mem08: %#04RX8\n", iOperand, *PtrUnion.pu8); break;
                    case 2: Bs3TestPrintf("op%u: result mem16: %#06RX16\n", iOperand, *PtrUnion.pu16); break;
                    case 4: Bs3TestPrintf("op%u: result mem32: %#010RX32\n", iOperand, *PtrUnion.pu32); break;
                    case 8: Bs3TestPrintf("op%u: result mem64: %#018RX64\n", iOperand, *PtrUnion.pu64); break;
                    default:
                        Bs3TestPrintf("op%u: result mem%u: %.*Rhxs\n", iOperand, pThis->aOperands[iOperand].cbOp * 8,
                                      pThis->aOperands[iOperand].cbOp, PtrUnion.pb);
                        break;
                }
                if (pThis->aOperands[iOperand].enmLocation == BS3CG1OPLOC_MEM_RW)
                {
                    PtrUnion.pb = pThis->MemOp.ab;
                    switch (pThis->aOperands[iOperand].cbOp)
                    {
                        case 1: Bs3TestPrintf("op%u: expect mem08: %#04RX8\n", iOperand, *PtrUnion.pu8); break;
                        case 2: Bs3TestPrintf("op%u: expect mem16: %#06RX16\n", iOperand, *PtrUnion.pu16); break;
                        case 4: Bs3TestPrintf("op%u: expect mem32: %#010RX32\n", iOperand, *PtrUnion.pu32); break;
                        case 8: Bs3TestPrintf("op%u: expect mem64: %#018RX64\n", iOperand, *PtrUnion.pu64); break;
                        default:
                            Bs3TestPrintf("op%u: expect mem%u: %.*Rhxs\n", iOperand, pThis->aOperands[iOperand].cbOp * 8,
                                          pThis->aOperands[iOperand].cbOp, PtrUnion.pb);
                            break;
                    }
                }
                break;
        }
    }

    /*
     * Display contexts.
     */
    Bs3TestPrintf("-- Expected context:\n");
    Bs3RegCtxPrint(&pThis->Ctx);
    Bs3TestPrintf("-- Actual context:\n");
    Bs3TrapPrintFrame(&pThis->TrapFrame);
    Bs3TestPrintf("\n");
    return false;
}


/**
 * Destroys the state, freeing all allocations and such.
 *
 * @param   pThis               The state.
 */
static void Bs3Cg1Destroy(PBS3CG1STATE pThis)
{
    if (BS3_MODE_IS_PAGED(pThis->bMode))
    {
#if ARCH_BITS != 16
        Bs3MemGuardedTestPageFree(pThis->pbCodePg);
        Bs3MemGuardedTestPageFree(pThis->pbDataPg);
#endif
    }
    else
    {
        Bs3MemFree(pThis->pbCodePg, X86_PAGE_SIZE);
        Bs3MemFree(pThis->pbDataPg, X86_PAGE_SIZE);
    }

    if (pThis->pExtCtx)
        Bs3MemFree(pThis->pExtCtx, pThis->pExtCtx->cb * 3);

    pThis->pbCodePg       = NULL;
    pThis->pbDataPg       = NULL;
    pThis->pExtCtx        = NULL;
    pThis->pResultExtCtx  = NULL;
    pThis->pInitialExtCtx = NULL;
}


/**
 * Initializes the state.
 *
 * @returns Success indicator (true/false)
 * @param   pThis               The state.
 * @param   bMode               The mode being tested.
 */
bool BS3_CMN_NM(Bs3Cg1Init)(PBS3CG1STATE pThis, uint8_t bMode)
{
    BS3MEMKIND const    enmMemKind = BS3_MODE_IS_RM_OR_V86(bMode) ? BS3MEMKIND_REAL
                                   : !BS3_MODE_IS_64BIT_CODE(bMode) ? BS3MEMKIND_TILED : BS3MEMKIND_FLAT32;
    unsigned            iRing;
    unsigned            cb;
    unsigned            i;
    uint64_t            fFlags;
    PBS3EXTCTX          pExtCtx;

    Bs3MemSet(pThis, 0, sizeof(*pThis));

    pThis->iFirstRing         = BS3_MODE_IS_V86(bMode)    ? 3 : 0;
    pThis->iEndRing           = BS3_MODE_IS_RM_SYS(bMode) ? 1 : 4;
    pThis->bMode              = bMode;
    pThis->pszMode            = Bs3GetModeName(bMode);
    pThis->pszModeShort       = Bs3GetModeNameShortLower(bMode);
    pThis->pchMnemonic        = g_achBs3Cg1Mnemonics;
    pThis->pabOperands        = g_abBs3Cg1Operands;
    pThis->pabOpcodes         = g_abBs3Cg1Opcodes;
    pThis->fAdvanceMnemonic   = 1;

    /* Allocate extended context structures. */
    cb = Bs3ExtCtxGetSize(&fFlags);
    pExtCtx = Bs3MemAlloc(BS3MEMKIND_TILED, cb * 3);
    if (!pExtCtx)
        return Bs3TestFailedF("Bs3MemAlloc(tiled,%#x)", cb * 3);
    pThis->pExtCtx        = pExtCtx;
    pThis->pResultExtCtx  = (PBS3EXTCTX)((uint8_t BS3_FAR *)pExtCtx + cb);
    pThis->pInitialExtCtx = (PBS3EXTCTX)((uint8_t BS3_FAR *)pExtCtx + cb + cb);

    Bs3ExtCtxInit(pThis->pExtCtx, cb, fFlags);
    Bs3ExtCtxInit(pThis->pResultExtCtx, cb, fFlags);
    Bs3ExtCtxInit(pThis->pInitialExtCtx, cb, fFlags);
    //Bs3TestPrintf("fCR0=%RX64 cbExtCtx=%#x method=%d\n", fFlags, cb, pExtCtx->enmMethod);

    /* Allocate guarded exectuable and data memory. */
    if (BS3_MODE_IS_PAGED(bMode))
    {
#if ARCH_BITS != 16
        pThis->pbCodePg = Bs3MemGuardedTestPageAlloc(enmMemKind);
        pThis->pbDataPg = Bs3MemGuardedTestPageAlloc(enmMemKind);
        if (!pThis->pbCodePg || !pThis->pbDataPg)
        {
            Bs3TestFailedF("Bs3MemGuardedTestPageAlloc(%d) failed", enmMemKind);
            Bs3MemPrintInfo();
            Bs3Shutdown();
            return Bs3TestFailedF("Bs3MemGuardedTestPageAlloc(%d) failed", enmMemKind);
        }
        if (   BS3_MODE_IS_64BIT_CODE(bMode)
            && (uintptr_t)pThis->pbDataPg >= _2G)
            return Bs3TestFailedF("pbDataPg=%p is above 2GB and not simple to address from 64-bit code", pThis->pbDataPg);
#else
        return Bs3TestFailed("WTF?! #1");
#endif
    }
    else
    {
        pThis->pbCodePg = Bs3MemAlloc(enmMemKind, X86_PAGE_SIZE);
        pThis->pbDataPg = Bs3MemAlloc(enmMemKind, X86_PAGE_SIZE);
        if (!pThis->pbCodePg || !pThis->pbDataPg)
        {
            Bs3MemPrintInfo();
            return Bs3TestFailedF("Bs3MemAlloc(%d,Pg) failed", enmMemKind);
        }
    }
    pThis->uCodePgFlat = Bs3SelPtrToFlat(pThis->pbCodePg);
    pThis->uDataPgFlat = Bs3SelPtrToFlat(pThis->pbDataPg);
#if ARCH_BITS == 16
    pThis->CodePgFar.sel = BS3_FP_SEG(pThis->pbCodePg);
    pThis->CodePgFar.off = BS3_FP_OFF(pThis->pbCodePg);
    pThis->CodePgRip     = BS3_FP_OFF(pThis->pbCodePg);
    pThis->DataPgFar.sel = BS3_FP_SEG(pThis->pbDataPg);
    pThis->DataPgFar.off = BS3_FP_OFF(pThis->pbDataPg);
#else
    if (BS3_MODE_IS_RM_OR_V86(bMode))
    {
        *(uint32_t *)&pThis->DataPgFar = Bs3SelFlatDataToRealMode(pThis->uDataPgFlat);
        ASMCompilerBarrier();
        pThis->CodePgFar.off = 0;
        pThis->CodePgFar.sel = pThis->uCodePgFlat >> 4;
        pThis->CodePgRip     = pThis->CodePgFar.off;
    }
    else if (BS3_MODE_IS_16BIT_CODE(bMode))
    {
        *(uint32_t *)&pThis->DataPgFar = Bs3SelFlatDataToProtFar16(pThis->uDataPgFlat);
        ASMCompilerBarrier();
        pThis->CodePgFar.sel = BS3_SEL_SPARE_00;
        pThis->CodePgFar.off = 0;
        pThis->CodePgRip     = 0;
    }
    else if (BS3_MODE_IS_32BIT_CODE(bMode))
    {
        *(uint32_t *)&pThis->DataPgFar = Bs3SelFlatDataToProtFar16(pThis->uDataPgFlat);
        ASMCompilerBarrier();
        pThis->CodePgFar.sel = 0;
        pThis->CodePgFar.off = 0;
        pThis->CodePgRip     = (uintptr_t)pThis->pbCodePg;
    }
    else
    {
        pThis->DataPgFar.off = 0;
        pThis->DataPgFar.sel = 0;
        pThis->CodePgFar.off = 0;
        pThis->CodePgFar.sel = 0;
        pThis->CodePgRip     = (uintptr_t)pThis->pbCodePg;
    }
#endif

    /*
     * Create basic context for each target ring.
     *
     * In protected 16-bit code we need set up code selectors that can access
     * pbCodePg.
     *
     * In long mode we make sure the high 32-bits of GPRs (sans RSP) have some
     * bits set so we can check that the implicit clearing is tested.
     */
    Bs3RegCtxSaveEx(&pThis->aInitialCtxs[pThis->iFirstRing], bMode, 1024 * 3);
#if ARCH_BITS == 64
    pThis->aInitialCtxs[pThis->iFirstRing].rax.u |= UINT64_C(0x0101010100000000);
    pThis->aInitialCtxs[pThis->iFirstRing].rbx.u |= UINT64_C(0x0202020200000000);
    pThis->aInitialCtxs[pThis->iFirstRing].rcx.u |= UINT64_C(0x0303030300000000);
    pThis->aInitialCtxs[pThis->iFirstRing].rdx.u |= UINT64_C(0x0404040400000000);
    pThis->aInitialCtxs[pThis->iFirstRing].rbp.u |= UINT64_C(0x0505050500000000);
    pThis->aInitialCtxs[pThis->iFirstRing].rdi.u |= UINT64_C(0x0606060600000000);
    pThis->aInitialCtxs[pThis->iFirstRing].rsi.u |= UINT64_C(0x0707070700000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r8.u  |= UINT64_C(0x0808080800000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r9.u  |= UINT64_C(0x0909090900000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r10.u |= UINT64_C(0x1010101000000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r11.u |= UINT64_C(0x1111111100000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r12.u |= UINT64_C(0x1212121200000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r13.u |= UINT64_C(0x1313131300000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r14.u |= UINT64_C(0x1414141400000000);
    pThis->aInitialCtxs[pThis->iFirstRing].r15.u |= UINT64_C(0x1515151500000000);
#endif

    if (BS3_MODE_IS_RM_OR_V86(bMode))
    {
        pThis->aInitialCtxs[pThis->iFirstRing].cs = pThis->CodePgFar.sel;
        BS3_ASSERT(pThis->iFirstRing + 1 == pThis->iEndRing);
    }
    else if (BS3_MODE_IS_16BIT_CODE(bMode))
    {
#if ARCH_BITS == 16
        uintptr_t const uFlatCodePgSeg = Bs3SelPtrToFlat(BS3_FP_MAKE(BS3_FP_SEG(pThis->pbCodePg), 0));
#else
        uintptr_t const uFlatCodePgSeg = (uintptr_t)pThis->pbCodePg;
#endif
        for (iRing = pThis->iFirstRing + 1; iRing < pThis->iEndRing; iRing++)
        {
            Bs3MemCpy(&pThis->aInitialCtxs[iRing], &pThis->aInitialCtxs[pThis->iFirstRing], sizeof(pThis->aInitialCtxs[iRing]));
            Bs3RegCtxConvertToRingX(&pThis->aInitialCtxs[iRing], iRing);
        }
        for (iRing = pThis->iFirstRing; iRing < pThis->iEndRing; iRing++)
        {
            pThis->aInitialCtxs[iRing].cs = BS3_SEL_SPARE_00 + iRing * 8 + iRing;
            Bs3SelSetup16BitCode(&Bs3GdteSpare00 + iRing, uFlatCodePgSeg, iRing);
        }
    }
    else
    {
        Bs3RegCtxSetRipCsFromCurPtr(&pThis->aInitialCtxs[pThis->iFirstRing], (FPFNBS3FAR)pThis->pbCodePg);
        for (iRing = pThis->iFirstRing + 1; iRing < pThis->iEndRing; iRing++)
        {
            Bs3MemCpy(&pThis->aInitialCtxs[iRing], &pThis->aInitialCtxs[pThis->iFirstRing], sizeof(pThis->aInitialCtxs[iRing]));
            Bs3RegCtxConvertToRingX(&pThis->aInitialCtxs[iRing], iRing);
        }
    }

    /*
     * Create an initial extended CPU context.
     */
    pExtCtx = pThis->pInitialExtCtx;
    if (   pExtCtx->enmMethod == BS3EXTCTXMETHOD_FXSAVE
        || pExtCtx->enmMethod == BS3EXTCTXMETHOD_XSAVE)
    {
        pExtCtx->Ctx.x87.FCW   = X86_FCW_MASK_ALL | X86_FCW_PC_64 | X86_FCW_RC_NEAREST;
        pExtCtx->Ctx.x87.FSW   = 0;
        pExtCtx->Ctx.x87.MXCSR      = X86_MXCSR_IM | X86_MXCSR_DM | X86_MXCSR_RC_NEAREST;
        pExtCtx->Ctx.x87.MXCSR_MASK = 0;
        for (i = 0; i < RT_ELEMENTS(pExtCtx->Ctx.x87.aRegs); i++)
        {
            pExtCtx->Ctx.x87.aRegs[i].au16[0] = i << 4;
            pExtCtx->Ctx.x87.aRegs[i].au16[1] = i << 4;
            pExtCtx->Ctx.x87.aRegs[i].au16[2] = i << 4;
            pExtCtx->Ctx.x87.aRegs[i].au16[3] = i << 4;
        }
        for (i = 0; i < RT_ELEMENTS(pExtCtx->Ctx.x87.aXMM); i++)
        {
            pExtCtx->Ctx.x87.aXMM[i].au16[0] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[1] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[2] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[3] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[4] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[5] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[6] = i;
            pExtCtx->Ctx.x87.aXMM[i].au16[7] = i;
        }
        if (pExtCtx->fXcr0 & XSAVE_C_YMM)
            for (i = 0; i < RT_ELEMENTS(pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi); i++)
            {
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[0] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[1] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[2] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[3] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[4] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[5] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[6] = i << 8;
                pExtCtx->Ctx.x.u.Intel.YmmHi.aYmmHi[i].au16[7] = i << 8;
            }

    }
    //else if (pExtCtx->enmMethod == BS3EXTCTXMETHOD_ANCIENT)
    else
        return Bs3TestFailedF("Unsupported extended CPU context method: %d", pExtCtx->enmMethod);

    return true;
}


static uint8_t BS3_CMN_NM(Bs3Cg1WorkerInner)(PBS3CG1STATE pThis)
{
    uint8_t  iRing;
    unsigned iInstr;

    /*
     * Test the instructions.
     */
    for (iInstr = 0; iInstr < g_cBs3Cg1Instructions;
         iInstr++,
         pThis->pchMnemonic += pThis->fAdvanceMnemonic * pThis->cchMnemonic,
         pThis->pabOperands += pThis->cOperands,
         pThis->pabOpcodes  += pThis->cbOpcodes)
    {
        bool     fInvalidInstr = false;
        unsigned iCpuSetup;
        uint8_t  bTestXcptExpected = BS3_MODE_IS_PAGED(pThis->bMode) ? X86_XCPT_PF : X86_XCPT_UD;

        /*
         * Expand the instruction information into the state.
         * Note! 16-bit will switch to a two level test header lookup once we exceed 64KB.
         */
        PCBS3CG1INSTR pInstr = &g_aBs3Cg1Instructions[iInstr];
        pThis->iInstr          = iInstr;
        pThis->pTestHdr        = (PCBS3CG1TESTHDR)&g_abBs3Cg1Tests[pInstr->offTests];
        pThis->fFlags          = pInstr->fFlags;
        pThis->enmEncoding     = (BS3CG1ENC)pInstr->enmEncoding;
        pThis->enmCpuTest      = (BS3CG1CPU)pInstr->enmCpuTest;
        pThis->enmPrefixKind   = (BS3CG1PFXKIND)pInstr->enmPrefixKind;
        pThis->enmXcptType     = (BS3CG1XCPTTYPE)pInstr->enmXcptType;
        pThis->cchMnemonic     = pInstr->cchMnemonic;
        if (pThis->fAdvanceMnemonic)
            Bs3TestSubF("%s / %.*s", pThis->pszModeShort, pThis->cchMnemonic, pThis->pchMnemonic);
        pThis->fAdvanceMnemonic = pInstr->fAdvanceMnemonic;
        pThis->cOperands       = pInstr->cOperands;
        pThis->cbOpcodes       = pInstr->cbOpcodes;
        switch (pThis->cOperands)
        {
            case 3: pThis->aenmOperands[3] = (BS3CG1OP)pThis->pabOperands[3];
            case 2: pThis->aenmOperands[2] = (BS3CG1OP)pThis->pabOperands[2];
            case 1: pThis->aenmOperands[1] = (BS3CG1OP)pThis->pabOperands[1];
            case 0: pThis->aenmOperands[0] = (BS3CG1OP)pThis->pabOperands[0];
        }

        switch (pThis->cbOpcodes)
        {
            case 3: pThis->abOpcodes[3] = pThis->pabOpcodes[3];
            case 2: pThis->abOpcodes[2] = pThis->pabOpcodes[2];
            case 1: pThis->abOpcodes[1] = pThis->pabOpcodes[1];
            case 0: pThis->abOpcodes[0] = pThis->pabOpcodes[0];
        }

        /*
         * Check if the CPU supports the instruction.
         */
        if (   !Bs3Cg1CpuSetupFirst(pThis)
            || (pThis->fFlags & (BS3CG1INSTR_F_UNUSED | BS3CG1INSTR_F_INVALID)))
        {
            fInvalidInstr = true;
            bTestXcptExpected = X86_XCPT_UD;
        }

        for (iCpuSetup = 0;; iCpuSetup++)
        {
            unsigned iEncoding;
            unsigned iEncodingNext;

            /*
             * Prep the operands and encoding handling.
             */
            if (!Bs3Cg1EncodePrep(pThis))
                break;

            /*
             * Encode the instruction in various ways and check out the test values.
             */
            for (iEncoding = 0;; iEncoding = iEncodingNext)
            {
                /*
                 * Encode the next instruction variation.
                 */
                iEncodingNext = Bs3Cg1EncodeNext(pThis, iEncoding);
                if (iEncodingNext <= iEncoding)
                    break;
                BS3CG1_DPRINTF(("\ndbg: Encoding #%u: cbCurInst=%u: %.*Rhxs\n",
                                iEncoding, pThis->cbCurInstr, pThis->cbCurInstr, pThis->abCurInstr));

                /*
                 * Do the rings.
                 */
                for (iRing = pThis->iFirstRing + pThis->fSameRingNotOkay; iRing < pThis->iEndRing; iRing++)
                {
                    PCBS3CG1TESTHDR pHdr;

                    pThis->uCpl = iRing;
                    BS3CG1_DPRINTF(("dbg:  Ring %u\n", iRing));

                    /*
                     * Do the tests one by one.
                     */
                    pHdr = pThis->pTestHdr;
                    for (pThis->iTest = 0;; pThis->iTest++)
                    {
                        if (Bs3Cg1RunSelector(pThis, pHdr))
                        {
                            /* Okay, set up the execution context. */
                            unsigned         offCode;
                            uint8_t BS3_FAR *pbCode;

                            Bs3MemCpy(&pThis->Ctx, &pThis->aInitialCtxs[iRing], sizeof(pThis->Ctx));
                            if (pThis->fWorkExtCtx)
                                Bs3ExtCtxCopy(pThis->pExtCtx, pThis->pInitialExtCtx);
                            if (BS3_MODE_IS_PAGED(pThis->bMode))
                            {
                                offCode = X86_PAGE_SIZE - pThis->cbCurInstr;
                                pbCode = &pThis->pbCodePg[offCode];
                                //if (iEncoding > 0) { pbCode[-1] = 0xf4; offCode--; }
                            }
                            else
                            {
                                pbCode = pThis->pbCodePg;
                                pbCode[pThis->cbCurInstr]     = 0x0f; /* UD2 */
                                pbCode[pThis->cbCurInstr + 1] = 0x0b;
                                offCode = 0;
                            }
                            pThis->Ctx.rip.u = pThis->CodePgRip + offCode;
                            Bs3MemCpy(pbCode, pThis->abCurInstr, pThis->cbCurInstr);

                            if (Bs3Cg1RunContextModifier(pThis, &pThis->Ctx, pHdr, pHdr->cbSelector, pHdr->cbInput, NULL, pbCode))
                            {
                                /* Run the instruction. */
                                BS3CG1_DPRINTF(("dbg:  Running test #%u\n", pThis->iTest));
                                //Bs3RegCtxPrint(&pThis->Ctx);
                                if (pThis->fWorkExtCtx)
                                    Bs3ExtCtxRestore(pThis->pExtCtx);
                                Bs3TrapSetJmpAndRestore(&pThis->Ctx, &pThis->TrapFrame);
                                if (pThis->fWorkExtCtx)
                                    Bs3ExtCtxSave(pThis->pResultExtCtx);
                                BS3CG1_DPRINTF(("dbg:  bXcpt=%#x rip=%RX64 -> %RX64\n",
                                                pThis->TrapFrame.bXcpt, pThis->Ctx.rip.u, pThis->TrapFrame.Ctx.rip.u));

                                /*
                                 * Apply the output modification program to the context.
                                 */
                                pThis->Ctx.rflags.u32 &= ~X86_EFL_RF;
                                pThis->Ctx.rflags.u32 |= pThis->TrapFrame.Ctx.rflags.u32 & X86_EFL_RF;
                                pThis->bValueXcpt      = UINT8_MAX;
                                if (   fInvalidInstr
                                    || pThis->bAlignmentXcpt != UINT8_MAX
                                    || pThis->bValueXcpt     != UINT8_MAX
                                    || Bs3Cg1RunContextModifier(pThis, &pThis->Ctx, pHdr,
                                                                pHdr->cbSelector + pHdr->cbInput, pHdr->cbOutput,
                                                                &pThis->TrapFrame.Ctx, NULL /*pbCode*/))
                                {
                                    Bs3Cg1CheckResult(pThis, fInvalidInstr, bTestXcptExpected, iEncoding);
                                }
                            }
                        }
                        else
                            BS3CG1_DPRINTF(("dbg:  Skipping #%u\n", pThis->iTest));

                        /* advance */
                        if (pHdr->fLast)
                        {
                            BS3CG1_DPRINTF(("dbg:  Last\n\n"));
                            break;
                        }
                        pHdr = (PCBS3CG1TESTHDR)((uint8_t BS3_FAR *)(pHdr + 1) + pHdr->cbInput + pHdr->cbOutput + pHdr->cbSelector);
                    }
                }
            }

            /*
             * Clean up (segment registers, etc) and get the next CPU config.
             */
            Bs3Cg1EncodeCleanup(pThis);
            if (!Bs3Cg1CpuSetupNext(pThis, iCpuSetup, &fInvalidInstr))
                break;
            if (pThis->fFlags & (BS3CG1INSTR_F_UNUSED | BS3CG1INSTR_F_INVALID))
                fInvalidInstr = true;
            if (fInvalidInstr)
                bTestXcptExpected = X86_XCPT_UD;
        }
    }

    return 0;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(Bs3Cg1Worker)(uint8_t bMode)
{
    uint8_t     bRet = 1;
    BS3CG1STATE This;

#if 0
    /* (for debugging) */
    if (!BS3_MODE_IS_RM_OR_V86(bMode))
        return BS3TESTDOMODE_SKIPPED;
#endif

    if (BS3_CMN_NM(Bs3Cg1Init)(&This, bMode))
    {
        bRet = BS3_CMN_NM(Bs3Cg1WorkerInner)(&This);
        Bs3TestSubDone();
    }
    Bs3Cg1Destroy(&This);

#if 0
    /* (for debugging) */
    if (bMode >= BS3_MODE_RM)
    {
        Bs3TestTerm();
        Bs3Shutdown();
    }
#endif
    return bRet;
}

