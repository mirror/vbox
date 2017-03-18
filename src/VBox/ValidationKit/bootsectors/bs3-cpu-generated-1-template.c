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

    /** Operand size in bytes (0 if not applicable). */
    uint8_t                 cbOperand;
    /** Target ring (0..3). */
    uint8_t                 uCpl;

    /** The current test number. */
    uint8_t                 iTest;

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
    uint8_t                 abCurInstr[27];

    /** Set if the encoding can't be tested in the same ring as this test code.
     *  This is used to deal with encodings modifying SP/ESP/RSP. */
    bool                    fSameRingNotOkay;
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
    /** Page for placing data operands in.  When paging is enabled, the page before
     * and after are marked not-present.  */
    uint8_t BS3_FAR        *pbDataPg;

    /** The name corresponding to bMode. */
    const char BS3_FAR     *pszMode;

    /** @name Expected result (modifiable by output program).
     * @{ */
    /** The expected exception based on operand values or result.
     * UINT8_MAX if no special exception expected. */
    uint8_t                 bValueXcpt;
    /** @} */

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

    /* [BS3CG1DST_VALUE_XCPT] = */ 1,

};

/** Destination field offset indexed by bBS3CG1DST.
 * Zero means operand size sized.  */
static const unsigned g_aoffBs3Cg1DstFields[] =
{
    /* [BS3CG1DST_INVALID] = */ ~0U,
    /* [BS3CG1DST_OP1] = */     ~0U,
    /* [BS3CG1DST_OP2] = */     ~0U,
    /* [BS3CG1DST_OP3] = */     ~0U,
    /* [BS3CG1DST_OP4] = */     ~0U,
    /* [BS3CG1DST_EFL] = */     RT_OFFSETOF(BS3REGCTX, rflags),
    /* [BS3CG1DST_EFL_UNDEF]=*/ ~0, /* special field */

    /* [BS3CG1DST_AL] = */      RT_OFFSETOF(BS3REGCTX, rax.u8),
    /* [BS3CG1DST_CL] = */      RT_OFFSETOF(BS3REGCTX, rcx.u8),
    /* [BS3CG1DST_DL] = */      RT_OFFSETOF(BS3REGCTX, rdx.u8),
    /* [BS3CG1DST_BL] = */      RT_OFFSETOF(BS3REGCTX, rbx.u8),
    /* [BS3CG1DST_AH] = */      RT_OFFSETOF(BS3REGCTX, rax.b.bHi),
    /* [BS3CG1DST_CH] = */      RT_OFFSETOF(BS3REGCTX, rcx.b.bHi),
    /* [BS3CG1DST_DH] = */      RT_OFFSETOF(BS3REGCTX, rdx.b.bHi),
    /* [BS3CG1DST_BH] = */      RT_OFFSETOF(BS3REGCTX, rbx.b.bHi),
    /* [BS3CG1DST_SPL] = */     RT_OFFSETOF(BS3REGCTX, rsp.u8),
    /* [BS3CG1DST_BPL] = */     RT_OFFSETOF(BS3REGCTX, rbp.u8),
    /* [BS3CG1DST_SIL] = */     RT_OFFSETOF(BS3REGCTX, rsi.u8),
    /* [BS3CG1DST_DIL] = */     RT_OFFSETOF(BS3REGCTX, rdi.u8),
    /* [BS3CG1DST_R8L] = */     RT_OFFSETOF(BS3REGCTX, r8.u8),
    /* [BS3CG1DST_R9L] = */     RT_OFFSETOF(BS3REGCTX, r9.u8),
    /* [BS3CG1DST_R10L] = */    RT_OFFSETOF(BS3REGCTX, r10.u8),
    /* [BS3CG1DST_R11L] = */    RT_OFFSETOF(BS3REGCTX, r11.u8),
    /* [BS3CG1DST_R12L] = */    RT_OFFSETOF(BS3REGCTX, r12.u8),
    /* [BS3CG1DST_R13L] = */    RT_OFFSETOF(BS3REGCTX, r13.u8),
    /* [BS3CG1DST_R14L] = */    RT_OFFSETOF(BS3REGCTX, r14.u8),
    /* [BS3CG1DST_R15L] = */    RT_OFFSETOF(BS3REGCTX, r15.u8),

    /* [BS3CG1DST_AX] = */      RT_OFFSETOF(BS3REGCTX, rax.u16),
    /* [BS3CG1DST_CX] = */      RT_OFFSETOF(BS3REGCTX, rcx.u16),
    /* [BS3CG1DST_DX] = */      RT_OFFSETOF(BS3REGCTX, rdx.u16),
    /* [BS3CG1DST_BX] = */      RT_OFFSETOF(BS3REGCTX, rbx.u16),
    /* [BS3CG1DST_SP] = */      RT_OFFSETOF(BS3REGCTX, rsp.u16),
    /* [BS3CG1DST_BP] = */      RT_OFFSETOF(BS3REGCTX, rbp.u16),
    /* [BS3CG1DST_SI] = */      RT_OFFSETOF(BS3REGCTX, rsi.u16),
    /* [BS3CG1DST_DI] = */      RT_OFFSETOF(BS3REGCTX, rdi.u16),
    /* [BS3CG1DST_R8W] = */     RT_OFFSETOF(BS3REGCTX, r8.u16),
    /* [BS3CG1DST_R9W] = */     RT_OFFSETOF(BS3REGCTX, r9.u16),
    /* [BS3CG1DST_R10W] = */    RT_OFFSETOF(BS3REGCTX, r10.u16),
    /* [BS3CG1DST_R11W] = */    RT_OFFSETOF(BS3REGCTX, r11.u16),
    /* [BS3CG1DST_R12W] = */    RT_OFFSETOF(BS3REGCTX, r12.u16),
    /* [BS3CG1DST_R13W] = */    RT_OFFSETOF(BS3REGCTX, r13.u16),
    /* [BS3CG1DST_R14W] = */    RT_OFFSETOF(BS3REGCTX, r14.u16),
    /* [BS3CG1DST_R15W] = */    RT_OFFSETOF(BS3REGCTX, r15.u16),

    /* [BS3CG1DST_EAX] = */     RT_OFFSETOF(BS3REGCTX, rax.u32),
    /* [BS3CG1DST_ECX] = */     RT_OFFSETOF(BS3REGCTX, rcx.u32),
    /* [BS3CG1DST_EDX] = */     RT_OFFSETOF(BS3REGCTX, rdx.u32),
    /* [BS3CG1DST_EBX] = */     RT_OFFSETOF(BS3REGCTX, rbx.u32),
    /* [BS3CG1DST_ESP] = */     RT_OFFSETOF(BS3REGCTX, rsp.u32),
    /* [BS3CG1DST_EBP] = */     RT_OFFSETOF(BS3REGCTX, rbp.u32),
    /* [BS3CG1DST_ESI] = */     RT_OFFSETOF(BS3REGCTX, rsi.u32),
    /* [BS3CG1DST_EDI] = */     RT_OFFSETOF(BS3REGCTX, rdi.u32),
    /* [BS3CG1DST_R8D] = */     RT_OFFSETOF(BS3REGCTX, r8.u32),
    /* [BS3CG1DST_R9D] = */     RT_OFFSETOF(BS3REGCTX, r9.u32),
    /* [BS3CG1DST_R10D] = */    RT_OFFSETOF(BS3REGCTX, r10.u32),
    /* [BS3CG1DST_R11D] = */    RT_OFFSETOF(BS3REGCTX, r11.u32),
    /* [BS3CG1DST_R12D] = */    RT_OFFSETOF(BS3REGCTX, r12.u32),
    /* [BS3CG1DST_R13D] = */    RT_OFFSETOF(BS3REGCTX, r13.u32),
    /* [BS3CG1DST_R14D] = */    RT_OFFSETOF(BS3REGCTX, r14.u32),
    /* [BS3CG1DST_R15D] = */    RT_OFFSETOF(BS3REGCTX, r15.u32),

    /* [BS3CG1DST_RAX] = */     RT_OFFSETOF(BS3REGCTX, rax.u64),
    /* [BS3CG1DST_RCX] = */     RT_OFFSETOF(BS3REGCTX, rcx.u64),
    /* [BS3CG1DST_RDX] = */     RT_OFFSETOF(BS3REGCTX, rdx.u64),
    /* [BS3CG1DST_RBX] = */     RT_OFFSETOF(BS3REGCTX, rbx.u64),
    /* [BS3CG1DST_RSP] = */     RT_OFFSETOF(BS3REGCTX, rsp.u64),
    /* [BS3CG1DST_RBP] = */     RT_OFFSETOF(BS3REGCTX, rbp.u64),
    /* [BS3CG1DST_RSI] = */     RT_OFFSETOF(BS3REGCTX, rsi.u64),
    /* [BS3CG1DST_RDI] = */     RT_OFFSETOF(BS3REGCTX, rdi.u64),
    /* [BS3CG1DST_R8] = */      RT_OFFSETOF(BS3REGCTX, r8.u64),
    /* [BS3CG1DST_R9] = */      RT_OFFSETOF(BS3REGCTX, r9.u64),
    /* [BS3CG1DST_R10] = */     RT_OFFSETOF(BS3REGCTX, r10.u64),
    /* [BS3CG1DST_R11] = */     RT_OFFSETOF(BS3REGCTX, r11.u64),
    /* [BS3CG1DST_R12] = */     RT_OFFSETOF(BS3REGCTX, r12.u64),
    /* [BS3CG1DST_R13] = */     RT_OFFSETOF(BS3REGCTX, r13.u64),
    /* [BS3CG1DST_R14] = */     RT_OFFSETOF(BS3REGCTX, r14.u64),
    /* [BS3CG1DST_R15] = */     RT_OFFSETOF(BS3REGCTX, r15.u64),

    /* [BS3CG1DST_OZ_RAX] = */  RT_OFFSETOF(BS3REGCTX, rax),
    /* [BS3CG1DST_OZ_RCX] = */  RT_OFFSETOF(BS3REGCTX, rcx),
    /* [BS3CG1DST_OZ_RDX] = */  RT_OFFSETOF(BS3REGCTX, rdx),
    /* [BS3CG1DST_OZ_RBX] = */  RT_OFFSETOF(BS3REGCTX, rbx),
    /* [BS3CG1DST_OZ_RSP] = */  RT_OFFSETOF(BS3REGCTX, rsp),
    /* [BS3CG1DST_OZ_RBP] = */  RT_OFFSETOF(BS3REGCTX, rbp),
    /* [BS3CG1DST_OZ_RSI] = */  RT_OFFSETOF(BS3REGCTX, rsi),
    /* [BS3CG1DST_OZ_RDI] = */  RT_OFFSETOF(BS3REGCTX, rdi),
    /* [BS3CG1DST_OZ_R8] = */   RT_OFFSETOF(BS3REGCTX, r8),
    /* [BS3CG1DST_OZ_R9] = */   RT_OFFSETOF(BS3REGCTX, r9),
    /* [BS3CG1DST_OZ_R10] = */  RT_OFFSETOF(BS3REGCTX, r10),
    /* [BS3CG1DST_OZ_R11] = */  RT_OFFSETOF(BS3REGCTX, r11),
    /* [BS3CG1DST_OZ_R12] = */  RT_OFFSETOF(BS3REGCTX, r12),
    /* [BS3CG1DST_OZ_R13] = */  RT_OFFSETOF(BS3REGCTX, r13),
    /* [BS3CG1DST_OZ_R14] = */  RT_OFFSETOF(BS3REGCTX, r14),
    /* [BS3CG1DST_OZ_R15] = */  RT_OFFSETOF(BS3REGCTX, r15),

    /* [BS3CG1DST_VALUE_XCPT] = */ ~0U,
};

#ifdef BS3CG1_DEBUG_CTX_MOD
/** Destination field names. */
static const struct { char sz[8]; } g_aszBs3Cg1DstFields[] =
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

    { "VALXCPT" },
};

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

static const uint16_t g_afPfxKindToIgnoredFlags[BS3CGPFXKIND_END] =
{
    /* [BS3CGPFXKIND_INVALID] = */              UINT16_MAX,
    /* [BS3CGPFXKIND_MODRM] = */                0,
    /* [BS3CGPFXKIND_MODRM_NO_OP_SIZES] = */    BS3CG1_PF_OZ | BS3CG1_PF_W,
};

#endif


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
    switch (pThis->enmEncoding)
    {
        case BS3CG1ENC_MODRM_Eb_Gb:
            /* Start by reg,reg encoding. */
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, 0);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xAX, X86_GREG_xCX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_AL;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_CL;
            }
            else if (iEncoding == 1 || iEncoding == 2)
            {
                /** @todo need to figure out a more flexible way to do all this crap. */
                off = 0;
                if (iEncoding == 2)
                    pThis->abCurInstr[off++] = 0x67;
                off = Bs3Cg1InsertOpcodes(pThis, off);
                if (BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                {
#if ARCH_BITS == 16 /** @todo fixme */
                    unsigned iRing = 4;
                    if (BS3_MODE_IS_RM_OR_V86(pThis->bMode))
                        while (iRing-- > 0)
                            pThis->aInitialCtxs[iRing].ds = BS3_FP_SEG(pThis->pbDataPg);
                    else
                        while (iRing-- > 0)
                            pThis->aInitialCtxs[iRing].ds = BS3_FP_SEG(pThis->pbDataPg) | iRing;
#endif
                    if (iEncoding == 1)
                    {
                        pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 6 /*disp16*/);
                        *(uint16_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - 1;
                        off += 2;
                    }
                    else
                    {
                        pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 5 /*disp32*/);
                        *(uint32_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - 1;
                        off += 4;
                    }
                }
                else
                {
                    pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 5 /*disp32*/);
                    *(uint32_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - 1;
                    if (BS3_MODE_IS_64BIT_CODE(pThis->bMode))
                        *(uint32_t *)&pThis->abCurInstr[off] -= BS3_FP_OFF(&pThis->pbCodePg[X86_PAGE_SIZE]);
                    off += 4;
                }
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_CH;
                pThis->aOperands[pThis->iRmOp ].idxField    = BS3CG1DST_INVALID;
                pThis->aOperands[pThis->iRmOp ].enmLocation = BS3CG1OPLOC_MEM_RW;
                pThis->aOperands[pThis->iRmOp ].off         = 1;

                if (BS3_MODE_IS_32BIT_CODE(pThis->bMode)) /* skip address size override */
                    iEncoding++;
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
                off = Bs3Cg1InsertOpcodes(pThis, 0);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xAX, X86_GREG_xCX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_AL;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_CL;
            }
            else if (iEncoding == 1 || iEncoding == 2)
            {
                /** @todo need to figure out a more flexible way to do all this crap. */
                off = 0;
                if (iEncoding == 2)
                    pThis->abCurInstr[off++] = 0x67;
                off = Bs3Cg1InsertOpcodes(pThis, off);
                if (BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                {
#if ARCH_BITS == 16 /** @todo fixme */
                    unsigned iRing = 4;
                    if (BS3_MODE_IS_RM_OR_V86(pThis->bMode))
                        while (iRing-- > 0)
                            pThis->aInitialCtxs[iRing].ds = BS3_FP_SEG(pThis->pbDataPg);
                    else
                        while (iRing-- > 0)
                            pThis->aInitialCtxs[iRing].ds = BS3_FP_SEG(pThis->pbDataPg) | iRing;
#endif
                    if (iEncoding == 1)
                    {
                        pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 6 /*disp16*/);
                        *(uint16_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - 1;
                        off += 2;
                    }
                    else
                    {
                        pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 5 /*disp32*/);
                        *(uint32_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - 1;
                        off += 4;
                    }
                }
                else
                {
                    pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 5 /*disp32*/);
                    *(uint32_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - 1;
                    if (BS3_MODE_IS_64BIT_CODE(pThis->bMode))
                        *(uint32_t *)&pThis->abCurInstr[off] -= BS3_FP_OFF(&pThis->pbCodePg[X86_PAGE_SIZE]);
                    off += 4;
                }
                pThis->aOperands[pThis->iRegOp].idxField    = BS3CG1DST_CH;
                pThis->aOperands[pThis->iRmOp ].idxField    = BS3CG1DST_INVALID;
                pThis->aOperands[pThis->iRmOp ].enmLocation = BS3CG1OPLOC_MEM;
                pThis->aOperands[pThis->iRmOp ].off         = 1;

                if (BS3_MODE_IS_32BIT_CODE(pThis->bMode)) /* skip address size override */
                    iEncoding++;
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_FIXED:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, 0);
                pThis->cbCurInstr = off;
                iEncoding++;
            }
            break;

        case BS3CG1ENC_FIXED_AL_Ib:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, 0);
                pThis->aOperands[1].off = (uint8_t)off;
                pThis->abCurInstr[off++] = 0xff;
                pThis->cbCurInstr = off;
                iEncoding++;
            }
            break;

        case BS3CG1ENC_FIXED_rAX_Iz:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, 0);
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
                off = Bs3Cg1InsertOpcodes(pThis, 1);
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
                pThis->abCurInstr[0] = REX_W___;
                off = Bs3Cg1InsertOpcodes(pThis, 1);
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

        case BS3CG1ENC_MODRM_Gv_Ev:
        case BS3CG1ENC_MODRM_Ev_Gv:
            if (iEncoding == 0)
            {
                off = Bs3Cg1InsertOpcodes(pThis, 0);
                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xBX, X86_GREG_xDX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBX;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_OZ_RDX;
                if (BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                    pThis->aOperands[0].cbOp = pThis->aOperands[1].cbOp = pThis->cbOperand = 2;
                else
                    pThis->aOperands[0].cbOp = pThis->aOperands[1].cbOp = pThis->cbOperand = 4;
            }
            else if (iEncoding == 1 && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
            {
                pThis->abCurInstr[0] = P_OZ;
                off = Bs3Cg1InsertOpcodes(pThis, 1);

                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xBX, X86_GREG_xDX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBX;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_OZ_RDX;
                if (!BS3_MODE_IS_16BIT_CODE(pThis->bMode))
                    pThis->aOperands[0].cbOp = pThis->aOperands[1].cbOp = pThis->cbOperand = 2;
                else
                    pThis->aOperands[0].cbOp = pThis->aOperands[1].cbOp = pThis->cbOperand = 4;
            }
            else if (iEncoding == 2 && BS3_MODE_IS_64BIT_CODE(pThis->bMode))
            {
                pThis->abCurInstr[0] = REX_W___;
                off = Bs3Cg1InsertOpcodes(pThis, 1);

                pThis->abCurInstr[off++] = X86_MODRM_MAKE(3, X86_GREG_xBX, X86_GREG_xDX);
                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_RBX;
                pThis->aOperands[pThis->iRmOp ].idxField = BS3CG1DST_RDX;
                pThis->aOperands[0].cbOp = pThis->aOperands[1].cbOp = pThis->cbOperand = 8;
            }
            else
                break;
            pThis->cbCurInstr = off;
            iEncoding++;
            break;

        case BS3CG1ENC_MODRM_Gv_Ma:
            if (iEncoding < 2)
            {
                /** @todo need to figure out a more flexible way to do all this crap. */
                uint8_t cbAddr = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 2 : 4;
                uint8_t cbOp   = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 2 : 4;

                off = 0;
                if (iEncoding == 1)
                {
                    pThis->abCurInstr[off++] = P_OZ;
                    cbOp = BS3_MODE_IS_16BIT_CODE(pThis->bMode) ? 4 : 2;
                }

                off = Bs3Cg1InsertOpcodes(pThis, off);

                if (cbAddr == 2)
                {
                    pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 6 /*disp16*/);
                    *(uint16_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - cbOp * 2;
                }
                else
                {
                    pThis->abCurInstr[off++] = X86_MODRM_MAKE(0, X86_GREG_xBP, 5 /*disp32*/);
                    *(uint32_t *)&pThis->abCurInstr[off] = BS3_FP_OFF(pThis->pbDataPg) + X86_PAGE_SIZE - cbOp * 2;
                }
                off += cbAddr;

                pThis->aOperands[pThis->iRegOp].idxField = BS3CG1DST_OZ_RBP;
                pThis->aOperands[pThis->iRegOp].cbOp     = cbOp;
                pThis->aOperands[pThis->iRmOp ].cbOp     = cbOp * 2;
                pThis->aOperands[pThis->iRmOp ].off      = cbOp * 2;
                pThis->cbOperand                         = cbOp;

#if ARCH_BITS == 16 /** @todo fixme */
                if (cbAddr == 2)
                {
                    unsigned iRing = 4;
                    if (BS3_MODE_IS_RM_OR_V86(pThis->bMode))
                        while (iRing-- > 0)
                            pThis->aInitialCtxs[iRing].ds = BS3_FP_SEG(pThis->pbDataPg);
                    else
                        while (iRing-- > 0)
                            pThis->aInitialCtxs[iRing].ds = BS3_FP_SEG(pThis->pbDataPg) | iRing;
                }
#endif
                pThis->cbCurInstr = off;
                iEncoding++;
            }
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

    pThis->iRmOp         = RT_ELEMENTS(pThis->aOperands) - 1;
    pThis->iRegOp        = RT_ELEMENTS(pThis->aOperands) - 1;
    pThis->fSameRingNotOkay = false;

    switch (pThis->enmEncoding)
    {
        case BS3CG1ENC_MODRM_Eb_Gb:
            pThis->iRmOp  = 0;
            pThis->iRegOp = 1;
            pThis->aOperands[0].cbOp = 1;
            pThis->aOperands[1].cbOp = 1;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Ev_Gv:
            pThis->iRmOp  = 0;
            pThis->iRegOp = 1;
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 2;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Gb_Eb:
            pThis->iRmOp  = 1;
            pThis->iRegOp = 0;
            pThis->aOperands[0].cbOp = 1;
            pThis->aOperands[1].cbOp = 1;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Gv_Ev:
            pThis->iRmOp  = 1;
            pThis->iRegOp = 0;
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 2;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_CTX;
            break;

        case BS3CG1ENC_MODRM_Gv_Ma:
            pThis->iRmOp  = 1;
            pThis->iRegOp = 0;
            pThis->aOperands[0].cbOp = 2;
            pThis->aOperands[1].cbOp = 4;
            pThis->aOperands[0].enmLocation = BS3CG1OPLOC_CTX;
            pThis->aOperands[1].enmLocation = BS3CG1OPLOC_MEM;
            pThis->aOperands[1].idxField    = BS3CG1DST_INVALID;
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

        default:
            return Bs3TestFailedF("Invalid/unimplemented enmEncoding for instruction #%RU32 (%.*s): %d",
                                  pThis->iInstr, pThis->cchMnemonic, pThis->pchMnemonic, pThis->enmEncoding);
    }
    return true;
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
            BS3PTRUNION    PtrField;

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
                            PtrField.pu8 = pThis->MemOp.ab[1];
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
            /// @todo else if (idxField <= BS3CG1DST_OP4)
            /// @todo {
            /// @todo
            /// @todo }
            else
                return Bs3TestFailedF("Todo implement me: cbDst=%u idxField=%d offField=%#x", cbDst, idxField, offField);

#ifdef BS3CG1_DEBUG_CTX_MOD
            {
                const char BS3_FAR *pszOp;
                switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                {
                    case BS3CG1_CTXOP_ASSIGN:   pszOp = "="; break;
                    case BS3CG1_CTXOP_OR:       pszOp = "|="; break;
                    case BS3CG1_CTXOP_AND:      pszOp = "&="; break;
                    case BS3CG1_CTXOP_AND_INV:  pszOp = "&~="; break;
                }
                switch (cbDst)
                {
                    case 1:
                        BS3CG1_DPRINTF(("dbg: modify %s: %#04RX8 (LB %u) %s %#RX64 (LB %u)\n",
                                        g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu8, cbDst, pszOp, uValue, cbValue));
                        break;
                    case 2:
                        BS3CG1_DPRINTF(("dbg: modify %s: %#06RX16 (LB %u) %s %#RX64 (LB %u)\n",
                                        g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu16, cbDst, pszOp, uValue, cbValue));
                        break;
                    case 4:
                        BS3CG1_DPRINTF(("dbg: modify %s: %#010RX32 (LB %u) %s %#RX64 (LB %u)\n",
                                        g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu32, cbDst, pszOp, uValue, cbValue));
                        break;
                    default:
                        BS3CG1_DPRINTF(("dbg: modify %s: %#018RX64 (LB %u) %s %#RX64 (LB %u)\n",
                                        g_aszBs3Cg1DstFields[idxField].sz, *PtrField.pu64, cbDst, pszOp, uValue, cbValue));
                        break;
                }
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
                    switch (bOpcode & BS3CG1_CTXOP_OPERATOR_MASK)
                    {
                        case BS3CG1_CTXOP_ASSIGN:   *PtrField.pu32  =  (uint32_t)uValue; break;
                        case BS3CG1_CTXOP_OR:       *PtrField.pu32 |=  (uint32_t)uValue; break;
                        case BS3CG1_CTXOP_AND:      *PtrField.pu32 &=  (uint32_t)uValue; break;
                        case BS3CG1_CTXOP_AND_INV:  *PtrField.pu32 &= ~(uint32_t)uValue; break;
                    }
                    break;

                case 8:
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
            return Bs3TestFailedF("TODO: Implement me: cbDst=%u idxField=%d", cbDst, idxField);

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
        if (RT_LIKELY(Bs3TestCheckRegCtxEx(&pThis->TrapFrame.Ctx, &pThis->Ctx,
                                           cbAdjustPc, 0 /*cbSpAdjust*/, 0 /*fExtraEfl*/,
                                           pThis->pszMode, iEncoding)))
        {
            bool fOkay = true;
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
            if (fOkay)
                return true;
        }

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

    /* Display memory operands. */
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
                    Bs3TestPrintf("op%u: imm%u: %.*Rhxs\n", iOperand, pThis->aOperands[iOperand].cbOp * 8,
                                  pThis->aOperands[iOperand].cbOp, *PtrUnion.pu64);
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
                                      pThis->aOperands[iOperand].cbOp, *PtrUnion.pu64);
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
                                      pThis->aOperands[iOperand].cbOp, *PtrUnion.pu64);
                        break;
                }
                break;

            case BS3CG1OPLOC_MEM:
            case BS3CG1OPLOC_MEM_RW:
                PtrUnion.pb = &pThis->pbDataPg[X86_PAGE_SIZE - pThis->aOperands[iOperand].off];
                switch (pThis->aOperands[iOperand].cbOp)
                {
                    case 1: Bs3TestPrintf("op%u: mem08: %#04RX8\n", iOperand, *PtrUnion.pu8); break;
                    case 2: Bs3TestPrintf("op%u: mem16: %#06RX16\n", iOperand, *PtrUnion.pu16); break;
                    case 4: Bs3TestPrintf("op%u: mem32: %#010RX32\n", iOperand, *PtrUnion.pu32); break;
                    case 8: Bs3TestPrintf("op%u: mem64: %#018RX64\n", iOperand, *PtrUnion.pu64); break;
                    default:
                        Bs3TestPrintf("op%u: mem%u: %.*Rhxs\n", iOperand, pThis->aOperands[iOperand].cbOp * 8,
                                      pThis->aOperands[iOperand].cbOp, *PtrUnion.pu64);
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
                                          pThis->aOperands[iOperand].cbOp, *PtrUnion.pu64);
                            break;
                    }
                }
                break;
        }
    }

    /* Display contexts: */
    Bs3TestPrintf("-- Expected context:\n");
    Bs3RegCtxPrint(&pThis->Ctx);
    Bs3TestPrintf("-- Actual context:\n");
    Bs3TrapPrintFrame(&pThis->TrapFrame);
    Bs3TestPrintf("\n");
    return false;
}


BS3_DECL_FAR(uint8_t) BS3_CMN_NM(Bs3Cg1Worker)(uint8_t bMode)
{
    BS3CG1STATE                 This;
    unsigned const              iFirstRing = BS3_MODE_IS_V86(bMode)       ? 3 : 0;
    uint8_t const               cRings     = BS3_MODE_IS_RM_OR_V86(bMode) ? 1 : 4;
    uint8_t                     iRing;
    unsigned                    iInstr;
    BS3MEMKIND const            enmMemKind = BS3_MODE_IS_RM_OR_V86(bMode) ? BS3MEMKIND_REAL
                                           : BS3_MODE_IS_16BIT_CODE(bMode) ? BS3MEMKIND_TILED : BS3MEMKIND_FLAT32;

#if 0
    if (bMode != BS3_MODE_PP16_V86)
        return BS3TESTDOMODE_SKIPPED;
#endif

    /*
     * Initalize the state.
     */
    Bs3MemSet(&This, 0, sizeof(This));

    This.bMode              = bMode;
    This.pszMode            = Bs3GetModeName(bMode);
    This.pchMnemonic        = g_achBs3Cg1Mnemonics;
    This.pabOperands        = g_abBs3Cg1Operands;
    This.pabOpcodes         = g_abBs3Cg1Opcodes;
    This.fAdvanceMnemonic   = 1;

    /* Allocate guarded exectuable and data memory. */
    if (BS3_MODE_IS_PAGED(bMode))
    {
        This.pbCodePg = Bs3MemGuardedTestPageAlloc(enmMemKind);
        if (!This.pbCodePg)
            return Bs3TestFailedF("First Bs3MemGuardedTestPageAlloc(%d) failed", enmMemKind);
        This.pbDataPg = Bs3MemGuardedTestPageAlloc(enmMemKind);
        if (!This.pbDataPg)
        {
            Bs3MemGuardedTestPageFree(This.pbCodePg);
            return Bs3TestFailedF("Second Bs3MemGuardedTestPageAlloc(%d) failed", enmMemKind);
        }
        if (   BS3_MODE_IS_64BIT_CODE(bMode)
            && (uintptr_t)This.pbDataPg >= _2G)
        {
            Bs3TestFailedF("pbDataPg=%p is above 2GB and not simple to address from 64-bit code", This.pbDataPg);
            Bs3MemGuardedTestPageFree(This.pbDataPg);
            Bs3MemGuardedTestPageFree(This.pbCodePg);
            return 0;
        }
    }
    else
    {
        This.pbCodePg = Bs3MemAlloc(enmMemKind, X86_PAGE_SIZE);
        if (!This.pbCodePg)
            return Bs3TestFailedF("First Bs3MemAlloc(%d,Pg) failed", enmMemKind);
        This.pbDataPg = Bs3MemAlloc(enmMemKind, X86_PAGE_SIZE);
        if (!This.pbDataPg)
        {
            Bs3MemFree(This.pbCodePg, X86_PAGE_SIZE);
            return Bs3TestFailedF("Second Bs3MemAlloc(%d,Pg) failed", enmMemKind);
        }
    }
    This.uCodePgFlat = Bs3SelPtrToFlat(This.pbCodePg);

    /* Create basic context for each target ring.  In protected 16-bit code we need
       set up code selectors that can access pbCodePg.  ASSUMES 16-bit driver code! */
    Bs3RegCtxSaveEx(&This.aInitialCtxs[iFirstRing], bMode, 512);
    if (BS3_MODE_IS_16BIT_CODE(bMode) && !BS3_MODE_IS_RM_OR_V86(bMode))
    {
#if ARCH_BITS == 16
        uintptr_t const uFlatCodePg = Bs3SelPtrToFlat(BS3_FP_MAKE(BS3_FP_SEG(This.pbCodePg), 0));
#else
        uintptr_t const uFlatCodePg = (uintptr_t)This.pbCodePg;
#endif
        BS3_ASSERT(ARCH_BITS == 16);
        for (iRing = iFirstRing + 1; iRing < cRings; iRing++)
        {
            Bs3MemCpy(&This.aInitialCtxs[iRing], &This.aInitialCtxs[iFirstRing], sizeof(This.aInitialCtxs[iRing]));
            Bs3RegCtxConvertToRingX(&This.aInitialCtxs[iRing], iRing);
        }
        for (iRing = iFirstRing; iRing < cRings; iRing++)
        {
            This.aInitialCtxs[iRing].cs = BS3_SEL_SPARE_00 + iRing * 8 + iRing;
            Bs3SelSetup16BitCode(&Bs3GdteSpare00 + iRing, uFlatCodePg, iRing);
        }
    }
    else
    {
        Bs3RegCtxSetRipCsFromCurPtr(&This.aInitialCtxs[iFirstRing], (FPFNBS3FAR)This.pbCodePg);
        for (iRing = iFirstRing + 1; iRing < cRings; iRing++)
        {
            Bs3MemCpy(&This.aInitialCtxs[iRing], &This.aInitialCtxs[iFirstRing], sizeof(This.aInitialCtxs[iRing]));
            Bs3RegCtxConvertToRingX(&This.aInitialCtxs[iRing], iRing);
        }
    }

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
        unsigned iEncodingNext;
        bool     fInvalidInstr = false;
        uint8_t  bTestXcptExpected = BS3_MODE_IS_PAGED(bMode) ? X86_XCPT_PF : X86_XCPT_UD;

        /*
         * Expand the instruction information into the state.
         * Note! 16-bit will switch to a two level test header lookup once we exceed 64KB.
         */
        PCBS3CG1INSTR pInstr = &g_aBs3Cg1Instructions[iInstr];
        This.iInstr          = iInstr;
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

        if ((This.fFlags & BS3CG1INSTR_F_INVALID_64BIT) && BS3_MODE_IS_64BIT_CODE(bMode))
        {
            fInvalidInstr = true;
            bTestXcptExpected = X86_XCPT_UD;
        }

        /*
         * Prep the operands and encoding handling.
         */
        if (!Bs3Cg1EncodePrep(&This))
            continue;

        /*
         * Encode the instruction in various ways and check out the test values.
         */
        for (iEncoding = 0;; iEncoding = iEncodingNext)
        {
            /*
             * Encode the next instruction variation.
             */
            iEncodingNext = Bs3Cg1EncodeNext(&This, iEncoding);
            if (iEncodingNext <= iEncoding)
                break;
            BS3CG1_DPRINTF(("\ndbg: Encoding #%u: cbCurInst=%u: %.*Rhxs\n", iEncoding, This.cbCurInstr, This.cbCurInstr, This.abCurInstr));

            /*
             * Do the rings.
             */
            for (iRing = iFirstRing + This.fSameRingNotOkay; iRing < cRings; iRing++)
            {
                PCBS3CG1TESTHDR pHdr;

                This.uCpl = iRing;
                BS3CG1_DPRINTF(("dbg:  Ring %u\n", iRing));

                /*
                 * Do the tests one by one.
                 */
                pHdr = This.pTestHdr;
                for (This.iTest = 0;; This.iTest++)
                {
                    if (Bs3Cg1RunSelector(&This, pHdr))
                    {
                        /* Okay, set up the execution context. */
                        uint8_t BS3_FAR *pbCode;

                        Bs3MemCpy(&This.Ctx, &This.aInitialCtxs[iRing], sizeof(This.Ctx));
                        if (BS3_MODE_IS_PAGED(bMode))
                            pbCode = &This.pbCodePg[X86_PAGE_SIZE - This.cbCurInstr];
                        else
                        {
                            pbCode = This.pbCodePg;
                            pbCode[This.cbCurInstr]     = 0x0f; /* UD2 */
                            pbCode[This.cbCurInstr + 1] = 0x0b;
                        }
                        Bs3MemCpy(pbCode, This.abCurInstr, This.cbCurInstr);
                        This.Ctx.rip.u = BS3_FP_OFF(pbCode);

                        if (Bs3Cg1RunContextModifier(&This, &This.Ctx, pHdr, pHdr->cbSelector, pHdr->cbInput, NULL, pbCode))
                        {
                            /* Run the instruction. */
                            BS3CG1_DPRINTF(("dbg:  Running test #%u\n", This.iTest));
                            //Bs3RegCtxPrint(&This.Ctx);
                            Bs3TrapSetJmpAndRestore(&This.Ctx, &This.TrapFrame);
                            BS3CG1_DPRINTF(("dbg:  bXcpt=%#x rip=%RX64 -> %RX64\n", This.TrapFrame.bXcpt, This.Ctx.rip.u, This.TrapFrame.Ctx.rip.u));

                            /*
                             * Apply the output modification program to the context.
                             */
                            This.Ctx.rflags.u32 &= ~X86_EFL_RF;
                            This.Ctx.rflags.u32 |= This.TrapFrame.Ctx.rflags.u32 & X86_EFL_RF;
                            This.bValueXcpt      = UINT8_MAX;
                            if (   fInvalidInstr
                                || Bs3Cg1RunContextModifier(&This, &This.Ctx, pHdr,
                                                            pHdr->cbSelector + pHdr->cbInput, pHdr->cbOutput,
                                                            &This.TrapFrame.Ctx, NULL /*pbCode*/))
                            {
                                Bs3Cg1CheckResult(&This, fInvalidInstr, bTestXcptExpected, iEncoding);
                            }
                        }
                    }
                    else
                        BS3CG1_DPRINTF(("dbg:  Skipping #%u\n", This.iTest));

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
         * Clean up (segment registers, etc).
         */
        Bs3Cg1EncodeCleanup(&This);
    }

    /*
     * Clean up.
     */
    if (BS3_MODE_IS_PAGED(bMode))
    {
        Bs3MemGuardedTestPageFree(This.pbCodePg);
        Bs3MemGuardedTestPageFree(This.pbDataPg);
    }
    else
    {
        Bs3MemFree(This.pbCodePg, X86_PAGE_SIZE);
        Bs3MemFree(This.pbDataPg, X86_PAGE_SIZE);
    }

    Bs3TestSubDone();
#if 0
    if (bMode >= BS3_MODE_PP16)
    {
        Bs3TestTerm();
        Bs3Shutdown();
    }
#endif

    return 0;
}

