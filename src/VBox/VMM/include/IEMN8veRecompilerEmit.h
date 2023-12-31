/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Native Recompiler Inlined Emitters.
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

#ifndef VMM_INCLUDED_SRC_include_IEMN8veRecompilerEmit_h
#define VMM_INCLUDED_SRC_include_IEMN8veRecompilerEmit_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "IEMN8veRecompiler.h"


/** @defgroup grp_iem_n8ve_re_inline    Native Recompiler Inlined Emitters
 * @ingroup grp_iem_n8ve_re
 * @{
 */

/**
 * Emit a simple marker instruction to more easily tell where something starts
 * in the disassembly.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitMarker(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t uInfo)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (uInfo == 0)
    {
        /* nop */
        pbCodeBuf[off++] = 0x90;
    }
    else
    {
        /* nop [disp32] */
        pbCodeBuf[off++] = 0x0f;
        pbCodeBuf[off++] = 0x1f;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM0, 0, 5);
        pbCodeBuf[off++] = RT_BYTE1(uInfo);
        pbCodeBuf[off++] = RT_BYTE2(uInfo);
        pbCodeBuf[off++] = RT_BYTE3(uInfo);
        pbCodeBuf[off++] = RT_BYTE4(uInfo);
    }
#elif defined(RT_ARCH_ARM64)
    /* nop */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = 0xd503201f;

    RT_NOREF(uInfo);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emit a breakpoint instruction.
 */
DECL_FORCE_INLINE(uint32_t) iemNativeEmitBrkEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint32_t uInfo)
{
#ifdef RT_ARCH_AMD64
    pCodeBuf[off++] = 0xcc;
    RT_NOREF(uInfo);   /** @todo use multibyte nop for info? */

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrBrk(uInfo & UINT32_C(0xffff));

#else
# error "error"
#endif
    return off;
}


/**
 * Emit a breakpoint instruction.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitBrk(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t uInfo)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitBrkEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, uInfo);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitBrkEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, uInfo);
#else
# error "error"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/*********************************************************************************************************************************
*   Loads, Stores and Related Stuff.                                                                                             *
*********************************************************************************************************************************/

#ifdef RT_ARCH_AMD64
/**
 * Common bit of iemNativeEmitLoadGprByGpr and friends.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitGprByGprDisp(uint8_t *pbCodeBuf, uint32_t off, uint8_t iGprReg, uint8_t iGprBase, int32_t offDisp)
{
    if (offDisp == 0 && (iGprBase & 7) != X86_GREG_xBP) /* Can use encoding w/o displacement field. */
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM0, iGprReg & 7, iGprBase & 7);
        if ((iGprBase & 7) == X86_GREG_xSP) /* for RSP/R12 relative addressing we have to use a SIB byte. */
            pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_xSP, X86_GREG_xSP, 0); /* -> [RSP/R12] */
    }
    else if (offDisp == (int8_t)offDisp)
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGprReg & 7, iGprBase & 7);
        if ((iGprBase & 7) == X86_GREG_xSP) /* for RSP/R12 relative addressing we have to use a SIB byte. */
            pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_xSP, X86_GREG_xSP, 0); /* -> [RSP/R12] */
        pbCodeBuf[off++] = (uint8_t)offDisp;
    }
    else
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, iGprReg & 7, iGprBase & 7);
        if ((iGprBase & 7) == X86_GREG_xSP) /* for RSP/R12 relative addressing we have to use a SIB byte. */
            pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_xSP, X86_GREG_xSP, 0); /* -> [RSP/R12] */
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
    }
    return off;
}
#endif /* RT_ARCH_AMD64 */

/**
 * Emits setting a GPR to zero.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitGprZero(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr)
{
#ifdef RT_ARCH_AMD64
    /* xor gpr32, gpr32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGpr & 7, iGpr & 7);

#elif defined(RT_ARCH_ARM64)
    /* mov gpr, #0x0 */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = UINT32_C(0xd2800000) | iGpr;

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Variant of iemNativeEmitLoadGprImm64 where the caller ensures sufficent
 * buffer space.
 *
 * Max buffer consumption:
 *      - AMD64: 10 instruction bytes.
 *      - ARM64: 4 instruction words (16 bytes).
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitLoadGprImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGpr, uint64_t uImm64)
{
#ifdef RT_ARCH_AMD64
    if (uImm64 == 0)
    {
        /* xor gpr, gpr */
        if (iGpr >= 8)
            pCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
        pCodeBuf[off++] = 0x33;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGpr & 7, iGpr & 7);
    }
    else if (uImm64 <= UINT32_MAX)
    {
        /* mov gpr, imm32 */
        if (iGpr >= 8)
            pCodeBuf[off++] = X86_OP_REX_B;
        pCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm64);
        pCodeBuf[off++] = RT_BYTE2(uImm64);
        pCodeBuf[off++] = RT_BYTE3(uImm64);
        pCodeBuf[off++] = RT_BYTE4(uImm64);
    }
    else if (uImm64 == (uint64_t)(int32_t)uImm64)
    {
        /* mov gpr, sx(imm32) */
        if (iGpr < 8)
            pCodeBuf[off++] = X86_OP_REX_W;
        else
            pCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_B;
        pCodeBuf[off++] = 0xc7;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGpr & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm64);
        pCodeBuf[off++] = RT_BYTE2(uImm64);
        pCodeBuf[off++] = RT_BYTE3(uImm64);
        pCodeBuf[off++] = RT_BYTE4(uImm64);
    }
    else
    {
        /* mov gpr, imm64 */
        if (iGpr < 8)
            pCodeBuf[off++] = X86_OP_REX_W;
        else
            pCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_B;
        pCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm64);
        pCodeBuf[off++] = RT_BYTE2(uImm64);
        pCodeBuf[off++] = RT_BYTE3(uImm64);
        pCodeBuf[off++] = RT_BYTE4(uImm64);
        pCodeBuf[off++] = RT_BYTE5(uImm64);
        pCodeBuf[off++] = RT_BYTE6(uImm64);
        pCodeBuf[off++] = RT_BYTE7(uImm64);
        pCodeBuf[off++] = RT_BYTE8(uImm64);
    }

#elif defined(RT_ARCH_ARM64)
    /*
     * We need to start this sequence with a 'mov grp, imm16, lsl #x' and
     * supply remaining bits using 'movk grp, imm16, lsl #x'.
     *
     * The mov instruction is encoded 0xd2800000 + shift + imm16 + grp,
     * while the movk is 0xf2800000 + shift + imm16 + grp, meaning the diff
     * is 0x20000000 (bit 29). So, we keep this bit in a variable and set it
     * after the first non-zero immediate component so we switch to movk for
     * the remainder.
     */
    unsigned cZeroHalfWords = !( uImm64        & UINT16_MAX)
                            + !((uImm64 >> 16) & UINT16_MAX)
                            + !((uImm64 >> 32) & UINT16_MAX)
                            + !((uImm64 >> 48) & UINT16_MAX);
    unsigned cFfffHalfWords = cZeroHalfWords >= 2 ? 0 /* skip */
                            : ( (uImm64        & UINT16_MAX) == UINT16_MAX)
                            + (((uImm64 >> 16) & UINT16_MAX) == UINT16_MAX)
                            + (((uImm64 >> 32) & UINT16_MAX) == UINT16_MAX)
                            + (((uImm64 >> 48) & UINT16_MAX) == UINT16_MAX);
    if (cFfffHalfWords <= cZeroHalfWords)
    {
        uint32_t fMovBase = UINT32_C(0xd2800000) | iGpr;

        /* movz gpr, imm16 */
        uint32_t uImmPart = (uint32_t)((uImm64 >>  0) & UINT32_C(0xffff));
        if (uImmPart || cZeroHalfWords == 4)
        {
            pCodeBuf[off++] = fMovBase | (UINT32_C(0) << 21) | (uImmPart << 5);
            fMovBase |= RT_BIT_32(29);
        }
        /* mov[z/k] gpr, imm16, lsl #16 */
        uImmPart = (uint32_t)((uImm64 >> 16) & UINT32_C(0xffff));
        if (uImmPart)
        {
            pCodeBuf[off++] = fMovBase | (UINT32_C(1) << 21) | (uImmPart << 5);
            fMovBase |= RT_BIT_32(29);
        }
        /* mov[z/k] gpr, imm16, lsl #32 */
        uImmPart = (uint32_t)((uImm64 >> 32) & UINT32_C(0xffff));
        if (uImmPart)
        {
            pCodeBuf[off++] = fMovBase | (UINT32_C(2) << 21) | (uImmPart << 5);
            fMovBase |= RT_BIT_32(29);
        }
        /* mov[z/k] gpr, imm16, lsl #48 */
        uImmPart = (uint32_t)((uImm64 >> 48) & UINT32_C(0xffff));
        if (uImmPart)
            pCodeBuf[off++] = fMovBase | (UINT32_C(3) << 21) | (uImmPart << 5);
    }
    else
    {
        uint32_t fMovBase = UINT32_C(0x92800000) | iGpr;

        /* find the first half-word that isn't UINT16_MAX. */
        uint32_t const iHwNotFfff =  (uImm64        & UINT16_MAX) != UINT16_MAX ? 0
                                  : ((uImm64 >> 16) & UINT16_MAX) != UINT16_MAX ? 1
                                  : ((uImm64 >> 32) & UINT16_MAX) != UINT16_MAX ? 2 : 3;

        /* movn gpr, imm16, lsl #iHwNotFfff*16 */
        uint32_t uImmPart = (uint32_t)(~(uImm64 >> (iHwNotFfff * 16)) & UINT32_C(0xffff)) << 5;
        pCodeBuf[off++] = fMovBase | (iHwNotFfff << 21) | uImmPart;
        fMovBase |= RT_BIT_32(30) | RT_BIT_32(29); /* -> movk */
        /* movk gpr, imm16 */
        if (iHwNotFfff != 0)
        {
            uImmPart = (uint32_t)((uImm64 >>  0) & UINT32_C(0xffff));
            if (uImmPart != UINT32_C(0xffff))
                pCodeBuf[off++] = fMovBase | (UINT32_C(0) << 21) | (uImmPart << 5);
        }
        /* movk gpr, imm16, lsl #16 */
        if (iHwNotFfff != 1)
        {
            uImmPart = (uint32_t)((uImm64 >> 16) & UINT32_C(0xffff));
            if (uImmPart != UINT32_C(0xffff))
                pCodeBuf[off++] = fMovBase | (UINT32_C(1) << 21) | (uImmPart << 5);
        }
        /* movk gpr, imm16, lsl #32 */
        if (iHwNotFfff != 2)
        {
            uImmPart = (uint32_t)((uImm64 >> 32) & UINT32_C(0xffff));
            if (uImmPart != UINT32_C(0xffff))
                pCodeBuf[off++] = fMovBase | (UINT32_C(2) << 21) | (uImmPart << 5);
        }
        /* movk gpr, imm16, lsl #48 */
        if (iHwNotFfff != 3)
        {
            uImmPart = (uint32_t)((uImm64 >> 48) & UINT32_C(0xffff));
            if (uImmPart != UINT32_C(0xffff))
                pCodeBuf[off++] = fMovBase | (UINT32_C(3) << 21) | (uImmPart << 5);
        }
    }

    /** @todo load into 'w' register instead of 'x' when imm64 <= UINT32_MAX?
     *        clang 12.x does that, only to use the 'x' version for the
     *        addressing in the following ldr). */

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits loading a constant into a 64-bit GPR
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprImm64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint64_t uImm64)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprImmEx(iemNativeInstrBufEnsure(pReNative, off, 10), off, iGpr, uImm64);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprImmEx(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGpr, uImm64);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Variant of iemNativeEmitLoadGpr32Imm where the caller ensures sufficent
 * buffer space.
 *
 * Max buffer consumption:
 *      - AMD64: 6 instruction bytes.
 *      - ARM64: 2 instruction words (8 bytes).
 *
 * @note The top 32 bits will be cleared.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGpr32ImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGpr, uint32_t uImm32)
{
#ifdef RT_ARCH_AMD64
    if (uImm32 == 0)
    {
        /* xor gpr, gpr */
        if (iGpr >= 8)
            pCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
        pCodeBuf[off++] = 0x33;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGpr & 7, iGpr & 7);
    }
    else
    {
        /* mov gpr, imm32 */
        if (iGpr >= 8)
            pCodeBuf[off++] = X86_OP_REX_B;
        pCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm32);
        pCodeBuf[off++] = RT_BYTE2(uImm32);
        pCodeBuf[off++] = RT_BYTE3(uImm32);
        pCodeBuf[off++] = RT_BYTE4(uImm32);
    }

#elif defined(RT_ARCH_ARM64)
    if ((uImm32 >> 16) == 0)
        /* movz gpr, imm16 */
        pCodeBuf[off++] = Armv8A64MkInstrMovZ(iGpr, uImm32,                    0, false /*f64Bit*/);
    else if ((uImm32 & UINT32_C(0xffff)) == 0)
        /* movz gpr, imm16, lsl #16 */
        pCodeBuf[off++] = Armv8A64MkInstrMovZ(iGpr, uImm32 >> 16,              1, false /*f64Bit*/);
    else if ((uImm32 & UINT32_C(0xffff)) == UINT32_C(0xffff))
        /* movn gpr, imm16, lsl #16 */
        pCodeBuf[off++] = Armv8A64MkInstrMovN(iGpr, ~uImm32 >> 16,             1, false /*f64Bit*/);
    else if ((uImm32 >> 16) == UINT32_C(0xffff))
        /* movn gpr, imm16 */
        pCodeBuf[off++] = Armv8A64MkInstrMovN(iGpr, ~uImm32,                   0, false /*f64Bit*/);
    else
    {
        pCodeBuf[off++] = Armv8A64MkInstrMovZ(iGpr, uImm32 & UINT32_C(0xffff), 0, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrMovK(iGpr, uImm32 >> 16,              1, false /*f64Bit*/);
    }

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits loading a constant into a 32-bit GPR.
 * @note The top 32 bits will be cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprImm32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t uImm32)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGpr32ImmEx(iemNativeInstrBufEnsure(pReNative, off, 6), off, iGpr, uImm32);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGpr32ImmEx(iemNativeInstrBufEnsure(pReNative, off, 2), off, iGpr, uImm32);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits loading a constant into a 8-bit GPR
 * @note The AMD64 version does *NOT* clear any bits in the 8..63 range,
 *       only the ARM64 version does that.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGpr8Imm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint8_t uImm8)
{
#ifdef RT_ARCH_AMD64
    /* mov gpr, imm8 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    else if (iGpr >= 4)
        pbCodeBuf[off++] = X86_OP_REX;
    pbCodeBuf[off++] = 0xb0 + (iGpr & 7);
    pbCodeBuf[off++] = RT_BYTE1(uImm8);

#elif defined(RT_ARCH_ARM64)
    /* movz gpr, imm16, lsl #0 */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = UINT32_C(0xd2800000) | (UINT32_C(0) << 21) | ((uint32_t)uImm8 << 5) | iGpr;

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


#ifdef RT_ARCH_AMD64
/**
 * Common bit of iemNativeEmitLoadGprFromVCpuU64 and friends.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitGprByVCpuDisp(uint8_t *pbCodeBuf, uint32_t off, uint8_t iGprReg, uint32_t offVCpu)
{
    if (offVCpu < 128)
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGprReg & 7, IEMNATIVE_REG_FIXED_PVMCPU);
        pbCodeBuf[off++] = (uint8_t)(int8_t)offVCpu;
    }
    else
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, iGprReg & 7, IEMNATIVE_REG_FIXED_PVMCPU);
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)offVCpu);
        pbCodeBuf[off++] = RT_BYTE2((uint32_t)offVCpu);
        pbCodeBuf[off++] = RT_BYTE3((uint32_t)offVCpu);
        pbCodeBuf[off++] = RT_BYTE4((uint32_t)offVCpu);
    }
    return off;
}

#elif defined(RT_ARCH_ARM64)

/**
 * Common bit of iemNativeEmitLoadGprFromVCpuU64Ex and friends.
 *
 * @note Loads can use @a iGprReg for large offsets, stores requires a temporary
 *       registers (@a iGprTmp).
 * @note DON'T try this with prefetch.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGprByVCpuLdStEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprReg, uint32_t offVCpu,
                             ARMV8A64INSTRLDSTTYPE enmOperation, unsigned cbData, uint8_t iGprTmp = UINT8_MAX)
{
    /*
     * There are a couple of ldr variants that takes an immediate offset, so
     * try use those if we can, otherwise we have to use the temporary register
     * help with the addressing.
     */
    if (offVCpu < _4K * cbData && !(offVCpu & (cbData - 1)))
        /* Use the unsigned variant of ldr Wt, [<Xn|SP>, #off]. */
        pCodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, IEMNATIVE_REG_FIXED_PVMCPU, offVCpu / cbData);
    else if (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx) < (unsigned)(_4K * cbData) && !(offVCpu & (cbData - 1)))
        pCodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, IEMNATIVE_REG_FIXED_PCPUMCTX,
                                                   (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx)) / cbData);
    else if (!ARMV8A64INSTRLDSTTYPE_IS_STORE(enmOperation) || iGprTmp != UINT8_MAX)
    {
        /* The offset is too large, so we must load it into a register and use
           ldr Wt, [<Xn|SP>, (<Wm>|<Xm>)]. */
        /** @todo reduce by offVCpu by >> 3 or >> 2? if it saves instructions? */
        if (iGprTmp == UINT8_MAX)
            iGprTmp = iGprReg;
        off = iemNativeEmitLoadGprImmEx(pCodeBuf, off, iGprTmp, offVCpu);
        pCodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGprReg, IEMNATIVE_REG_FIXED_PVMCPU, iGprTmp);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

    return off;
}

/**
 * Common bit of iemNativeEmitLoadGprFromVCpuU64 and friends.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGprByVCpuLdSt(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprReg,
                           uint32_t offVCpu, ARMV8A64INSTRLDSTTYPE enmOperation, unsigned cbData)
{
    /*
     * There are a couple of ldr variants that takes an immediate offset, so
     * try use those if we can, otherwise we have to use the temporary register
     * help with the addressing.
     */
    if (offVCpu < _4K * cbData && !(offVCpu & (cbData - 1)))
    {
        /* Use the unsigned variant of ldr Wt, [<Xn|SP>, #off]. */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, IEMNATIVE_REG_FIXED_PVMCPU, offVCpu / cbData);
    }
    else if (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx) < (unsigned)(_4K * cbData) && !(offVCpu & (cbData - 1)))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, IEMNATIVE_REG_FIXED_PCPUMCTX,
                                                      (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx)) / cbData);
    }
    else
    {
        /* The offset is too large, so we must load it into a register and use
           ldr Wt, [<Xn|SP>, (<Wm>|<Xm>)]. */
        /** @todo reduce by offVCpu by >> 3 or >> 2? if it saves instructions? */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, offVCpu);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGprReg, IEMNATIVE_REG_FIXED_PVMCPU,
                                                       IEMNATIVE_REG_FIXED_TMP0);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}

#endif /* RT_ARCH_ARM64 */


/**
 * Emits a 64-bit GPR load of a VCpu value.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromVCpuU64Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov reg64, mem64 */
    if (iGpr < 8)
        pCodeBuf[off++] = X86_OP_REX_W;
    else
        pCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByVCpuDisp(pCodeBuf, off,iGpr, offVCpu);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdStEx(pCodeBuf, off, iGpr, offVCpu, kArmv8A64InstrLdStType_Ld_Dword, sizeof(uint64_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 64-bit GPR load of a VCpu value.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromVCpuU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprFromVCpuU64Ex(iemNativeInstrBufEnsure(pReNative, off, 7), off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_Ld_Dword, sizeof(uint64_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 32-bit GPR load of a VCpu value.
 * @note Bits 32 thru 63 in the GPR will be zero after the operation.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromVCpuU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov reg32, mem32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_Ld_Word, sizeof(uint32_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 16-bit GPR load of a VCpu value.
 * @note Bits 16 thru 63 in the GPR will be zero after the operation.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromVCpuU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* movzx reg32, mem16 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_Ld_Half, sizeof(uint16_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 8-bit GPR load of a VCpu value.
 * @note Bits 8 thru 63 in the GPR will be zero after the operation.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromVCpuU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* movzx reg32, mem8 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb6;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_Ld_Byte, sizeof(uint8_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a store of a GPR value to a 64-bit VCpu field.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGprToVCpuU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem64, reg64 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGpr < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x89;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf,off,iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_St_Dword, sizeof(uint64_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a store of a GPR value to a 32-bit VCpu field.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGprToVCpuU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem32, reg32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x89;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_St_Word, sizeof(uint32_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a store of a GPR value to a 16-bit VCpu field.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGprToVCpuU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem16, reg16 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x89;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_St_Half, sizeof(uint16_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a store of a GPR value to a 8-bit VCpu field.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGprToVCpuU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem8, reg8 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x88;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGpr, offVCpu);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_St_Byte, sizeof(uint8_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a store of an immediate value to a 8-bit VCpu field.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreImmToVCpuU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t bImm, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem8, imm8 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    pbCodeBuf[off++] = 0xc6;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, 0, offVCpu);
    pbCodeBuf[off++] = bImm;
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    /* Cannot use IEMNATIVE_REG_FIXED_TMP0 for the immediate as that's used by iemNativeEmitGprByVCpuLdSt. */
    uint8_t const idxRegImm = iemNativeRegAllocTmpImm(pReNative, &off, bImm);
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, idxRegImm, offVCpu, kArmv8A64InstrLdStType_St_Byte, sizeof(uint8_t));
    iemNativeRegFreeTmpImm(pReNative, idxRegImm);

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a load effective address to a GRP of a VCpu field.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLeaGprByVCpu(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* lea gprdst, [rbx + offDisp] */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8d;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, iGprDst, offVCpu);

#elif defined(RT_ARCH_ARM64)
    if (offVCpu < (unsigned)_4K)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, IEMNATIVE_REG_FIXED_PVMCPU, offVCpu);
    }
    else if (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx) < (unsigned)_4K)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, IEMNATIVE_REG_FIXED_PCPUMCTX,
                                                         offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx));
    }
    else
    {
        Assert(iGprDst != IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitLoadGprImm64(pReNative, off, iGprDst, offVCpu);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, IEMNATIVE_REG_FIXED_PCPUMCTX, iGprDst);
    }

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc load.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitLoadGprFromGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, gprsrc */
    if ((iGprDst | iGprSrc) >= 8)
        pCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_W | X86_OP_REX_B
                        : iGprSrc >= 8 ? X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_B
                        :                X86_OP_REX_W | X86_OP_REX_R;
    else
        pCodeBuf[off++] = X86_OP_REX_W;
    pCodeBuf[off++] = 0x8b;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* mov dst, src;   alias for: orr dst, xzr, src */
    pCodeBuf[off++] = Armv8A64MkInstrOrr(iGprDst, ARMV8_A64_REG_XZR, iGprSrc);

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a gprdst = gprsrc load.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprFromGprEx(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst, iGprSrc);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprFromGprEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprSrc);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc[31:0] load.
 * @note Bits 63 thru 32 are cleared.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitLoadGprFromGpr32Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, gprsrc */
    if ((iGprDst | iGprSrc) >= 8)
        pCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_B
                        : iGprSrc >= 8 ? X86_OP_REX_R | X86_OP_REX_B
                        :                X86_OP_REX_R;
    pCodeBuf[off++] = 0x8b;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* mov dst32, src32;   alias for: orr dst32, wzr, src32 */
    pCodeBuf[off++] = Armv8A64MkInstrOrr(iGprDst, ARMV8_A64_REG_WZR, iGprSrc, false /*f64bit*/);

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a gprdst = gprsrc[31:0] load.
 * @note Bits 63 thru 32 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprFromGpr32Ex(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst, iGprSrc);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprFromGpr32Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprSrc);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc[15:0] load.
 * @note Bits 63 thru 15 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movzx Gv,Ew */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* and gprdst, gprsrc, #0xffff */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
# if 1
    Assert(Armv8A64ConvertImmRImmS2Mask32(0x0f, 0) == UINT16_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprSrc, 0x0f, 0, false /*f64Bit*/);
# else
    Assert(Armv8A64ConvertImmRImmS2Mask64(0x4f, 0) == UINT16_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprSrc, 0x4f, 0);
# endif

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc[7:0] load.
 * @note Bits 63 thru 8 are cleared.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitLoadGprFromGpr8Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movzx Gv,Eb */
    if (iGprDst >= 8 || iGprSrc >= 8)
        pCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_B
                        : iGprSrc >= 8 ? X86_OP_REX_R | X86_OP_REX_B
                        :                X86_OP_REX_R;
    else if (iGprSrc >= 4)
        pCodeBuf[off++] = X86_OP_REX;
    pCodeBuf[off++] = 0x0f;
    pCodeBuf[off++] = 0xb6;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* and gprdst, gprsrc, #0xff */
    Assert(Armv8A64ConvertImmRImmS2Mask32(0x07, 0) == UINT8_MAX);
    pCodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprSrc, 0x07, 0, false /*f64Bit*/);

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a gprdst = gprsrc[7:0] load.
 * @note Bits 63 thru 8 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprFromGpr8Ex(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, iGprSrc);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprFromGpr8Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprSrc);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc[15:8] load (ah, ch, dh, bh).
 * @note Bits 63 thru 8 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr8Hi(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);

    /* movzx Gv,Ew */
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

    /* shr Ev,8 */
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    pbCodeBuf[off++] = 0xc1;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
    pbCodeBuf[off++] = 8;

#elif defined(RT_ARCH_ARM64)
    /* ubfx gprdst, gprsrc, #8, #8 - gprdst = gprsrc[15:8] */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrUbfx(iGprDst, iGprSrc, 8, 8, false /*f64Bit*/);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Sign-extends 32-bit value in @a iGprSrc into a 64-bit value in @a iGprDst.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprSignExtendedFromGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movsxd r64, r/m32 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x63;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* sxtw dst, src */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSxtw(iGprDst, iGprSrc);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Sign-extends 16-bit value in @a iGprSrc into a 64-bit value in @a iGprDst.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprSignExtendedFromGpr16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movsx r64, r/m16 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xbf;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* sxth dst, src */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSxth(iGprDst, iGprSrc);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Sign-extends 16-bit value in @a iGprSrc into a 32-bit value in @a iGprDst.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGpr32SignExtendedFromGpr16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movsx r64, r/m16 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xbf;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* sxth dst32, src */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSxth(iGprDst, iGprSrc, false /*f64Bit*/);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Sign-extends 8-bit value in @a iGprSrc into a 64-bit value in @a iGprDst.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprSignExtendedFromGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movsx r64, r/m8 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xbe;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* sxtb dst, src */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSxtb(iGprDst, iGprSrc);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Sign-extends 8-bit value in @a iGprSrc into a 32-bit value in @a iGprDst.
 * @note Bits 63 thru 32 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGpr32SignExtendedFromGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movsx r32, r/m8 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xbe;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* sxtb dst32, src32 */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSxtb(iGprDst, iGprSrc, false /*f64Bit*/);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Sign-extends 8-bit value in @a iGprSrc into a 16-bit value in @a iGprDst.
 * @note Bits 63 thru 16 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGpr16SignExtendedFromGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movsx r16, r/m8 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 9);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xbe;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

    /* movzx r32, r/m16 */
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprDst & 7);

#elif defined(RT_ARCH_ARM64)
    /* sxtb dst32, src32;  and dst32, dst32, #0xffff */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    pu32CodeBuf[off++] = Armv8A64MkInstrSxtb(iGprDst, iGprSrc, false /*f64Bit*/);
    Assert(Armv8A64ConvertImmRImmS2Mask32(15, 0) == 0xffff);
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprDst, 15, 0, false /*f64Bit*/);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc + addend load.
 * @note The added is 32-bit for AMD64 and 64-bit for ARM64.
 */
#ifdef RT_ARCH_AMD64
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGprWithAddend(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                      uint8_t iGprDst, uint8_t iGprSrc, int32_t iAddend)
{
    Assert(iAddend != 0);

    /* lea gprdst, [gprsrc + iAddend] */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst >= 8 ? X86_OP_REX_R : 0) | (iGprSrc >= 8 ? X86_OP_REX_B : 0);
    pbCodeBuf[off++] = 0x8d;
    off = iemNativeEmitGprByGprDisp(pbCodeBuf, off, iGprDst, iGprSrc, iAddend);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}

#elif defined(RT_ARCH_ARM64)
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGprWithAddend(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                      uint8_t iGprDst, uint8_t iGprSrc, int64_t iAddend)
{
    if ((uint32_t)iAddend < 4096)
    {
        /* add dst, src, uimm12 */
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprSrc, (uint32_t)iAddend);
    }
    else if ((uint32_t)-iAddend < 4096)
    {
        /* sub dst, src, uimm12 */
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprSrc, (uint32_t)-iAddend);
    }
    else
    {
        Assert(iGprSrc != iGprDst);
        off = iemNativeEmitLoadGprImm64(pReNative, off, iGprDst, iAddend);
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprSrc, iGprDst);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#else
# error "port me"
#endif

/**
 * Emits a gprdst = gprsrc + addend load, accepting iAddend == 0.
 * @note The added is 32-bit for AMD64 and 64-bit for ARM64.
 */
#ifdef RT_ARCH_AMD64
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGprWithAddendMaybeZero(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                               uint8_t iGprDst, uint8_t iGprSrc, int32_t iAddend)
#else
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGprWithAddendMaybeZero(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                               uint8_t iGprDst, uint8_t iGprSrc, int64_t iAddend)
#endif
{
    if (iAddend != 0)
        return iemNativeEmitLoadGprFromGprWithAddend(pReNative, off, iGprDst, iGprSrc, iAddend);
    return iemNativeEmitLoadGprFromGpr(pReNative, off, iGprDst, iGprSrc);
}


/**
 * Emits a gprdst = gprsrc32 + addend load.
 * @note Bits 63 thru 32 are cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr32WithAddend(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                        uint8_t iGprDst, uint8_t iGprSrc, int32_t iAddend)
{
    Assert(iAddend != 0);

#ifdef RT_ARCH_AMD64
    /* a32 o32 lea gprdst, [gprsrc + iAddend] */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 9);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_ADDR;
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = (iGprDst >= 8 ? X86_OP_REX_R : 0) | (iGprSrc >= 8 ? X86_OP_REX_B : 0);
    pbCodeBuf[off++] = 0x8d;
    off = iemNativeEmitGprByGprDisp(pbCodeBuf, off, iGprDst, iGprSrc, iAddend);

#elif defined(RT_ARCH_ARM64)
    if ((uint32_t)iAddend < 4096)
    {
        /* add dst, src, uimm12 */
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprSrc, (uint32_t)iAddend, false /*f64Bit*/);
    }
    else if ((uint32_t)-iAddend < 4096)
    {
        /* sub dst, src, uimm12 */
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprSrc, (uint32_t)-iAddend, false /*f64Bit*/);
    }
    else
    {
        Assert(iGprSrc != iGprDst);
        off = iemNativeEmitLoadGprImm64(pReNative, off, iGprDst, (int64_t)iAddend);
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprSrc, iGprDst, false /*f64Bit*/);
    }

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a gprdst = gprsrc32 + addend load, accepting iAddend == 0.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr32WithAddendMaybeZero(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                 uint8_t iGprDst, uint8_t iGprSrc, int32_t iAddend)
{
    if (iAddend != 0)
        return iemNativeEmitLoadGprFromGpr32WithAddend(pReNative, off, iGprDst, iGprSrc, iAddend);
    return iemNativeEmitLoadGprFromGpr32(pReNative, off, iGprDst, iGprSrc);
}



#ifdef RT_ARCH_AMD64
/**
 * Common bit of iemNativeEmitLoadGprByBp and friends.
 */
DECL_FORCE_INLINE(uint32_t) iemNativeEmitGprByBpDisp(uint8_t *pbCodeBuf, uint32_t off, uint8_t iGprReg, int32_t offDisp,
                                                     PIEMRECOMPILERSTATE pReNativeAssert)
{
    if (offDisp < 128 && offDisp >= -128)
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGprReg & 7, X86_GREG_xBP);
        pbCodeBuf[off++] = (uint8_t)(int8_t)offDisp;
    }
    else
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, iGprReg & 7, X86_GREG_xBP);
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNativeAssert, off); RT_NOREF(pReNativeAssert);
    return off;
}
#elif defined(RT_ARCH_ARM64)
/**
 * Common bit of iemNativeEmitLoadGprByBp and friends.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGprByBpLdSt(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprReg,
                         int32_t offDisp, ARMV8A64INSTRLDSTTYPE enmOperation, unsigned cbData)
{
    if ((uint32_t)offDisp < 4096U * cbData && !((uint32_t)offDisp & (cbData - 1)))
    {
        /* str w/ unsigned imm12 (scaled) */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, ARMV8_A64_REG_BP, (uint32_t)offDisp / cbData);
    }
    else if (offDisp >= -256 && offDisp <= 256)
    {
        /* stur w/ signed imm9 (unscaled) */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrSturLdur(enmOperation, iGprReg, ARMV8_A64_REG_BP, offDisp);
    }
    else
    {
        /* Use temporary indexing register. */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, (uint32_t)offDisp);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGprReg, ARMV8_A64_REG_BP,
                                                       IEMNATIVE_REG_FIXED_TMP0, kArmv8A64InstrLdStExtend_Sxtw);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#endif


/**
 * Emits a 64-bit GRP load instruction with an BP relative source address.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, qword [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);

#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitGprByBpLdSt(pReNative, off, iGprDst, offDisp, kArmv8A64InstrLdStType_Ld_Dword, sizeof(uint64_t));

#else
# error "port me"
#endif
}


/**
 * Emits a 32-bit GRP load instruction with an BP relative source address.
 * @note Bits 63 thru 32 of the GPR will be cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByBpU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, dword [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);

#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitGprByBpLdSt(pReNative, off, iGprDst, offDisp, kArmv8A64InstrLdStType_Ld_Word, sizeof(uint32_t));

#else
# error "port me"
#endif
}


/**
 * Emits a 16-bit GRP load instruction with an BP relative source address.
 * @note Bits 63 thru 16 of the GPR will be cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByBpU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* movzx gprdst, word [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);

#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitGprByBpLdSt(pReNative, off, iGprDst, offDisp, kArmv8A64InstrLdStType_Ld_Half, sizeof(uint32_t));

#else
# error "port me"
#endif
}


/**
 * Emits a 8-bit GRP load instruction with an BP relative source address.
 * @note Bits 63 thru 8 of the GPR will be cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByBpU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* movzx gprdst, byte [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb6;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);

#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitGprByBpLdSt(pReNative, off, iGprDst, offDisp, kArmv8A64InstrLdStType_Ld_Byte, sizeof(uint32_t));

#else
# error "port me"
#endif
}


/**
 * Emits a load effective address to a GRP with an BP relative source address.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLeaGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* lea gprdst, [rbp + offDisp] */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8d;
    off = iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);

#elif defined(RT_ARCH_ARM64)
    if ((uint32_t)offDisp < (unsigned)_4K)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, ARMV8_A64_REG_BP, (uint32_t)offDisp);
    }
    else if ((uint32_t)-offDisp < (unsigned)_4K)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, ARMV8_A64_REG_BP, (uint32_t)-offDisp);
    }
    else
    {
        Assert(iGprDst != IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitLoadGprImm64(pReNative, off, iGprDst, offDisp >= 0 ? (uint32_t)offDisp : (uint32_t)-offDisp);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (offDisp >= 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, ARMV8_A64_REG_BP, iGprDst);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(true /*fSub*/, iGprDst, ARMV8_A64_REG_BP, iGprDst);
    }

#else
# error "port me"
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a 64-bit GPR store with an BP relative destination address.
 *
 * @note May trash IEMNATIVE_REG_FIXED_TMP0.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offDisp, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov qword [rbp + offDisp], gprdst */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprSrc < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x89;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprSrc, offDisp, pReNative);

#elif defined(RT_ARCH_ARM64)
    if (offDisp >= 0 && offDisp < 4096 * 8 && !((uint32_t)offDisp & 7))
    {
        /* str w/ unsigned imm12 (scaled) */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_St_Dword, iGprSrc,
                                                      ARMV8_A64_REG_BP, (uint32_t)offDisp / 8);
    }
    else if (offDisp >= -256 && offDisp <= 256)
    {
        /* stur w/ signed imm9 (unscaled) */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrSturLdur(kArmv8A64InstrLdStType_St_Dword, iGprSrc, ARMV8_A64_REG_BP, offDisp);
    }
    else if ((uint32_t)-offDisp < (unsigned)_4K)
    {
        /* Use temporary indexing register w/ sub uimm12. */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, IEMNATIVE_REG_FIXED_TMP0,
                                                         ARMV8_A64_REG_BP, (uint32_t)-offDisp);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_St_Dword, iGprSrc, IEMNATIVE_REG_FIXED_TMP0, 0);
    }
    else
    {
        /* Use temporary indexing register. */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, (uint32_t)offDisp);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(kArmv8A64InstrLdStType_St_Dword, iGprSrc, ARMV8_A64_REG_BP,
                                                       IEMNATIVE_REG_FIXED_TMP0, kArmv8A64InstrLdStExtend_Sxtw);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;

#else
# error "Port me!"
#endif
}


/**
 * Emits a 64-bit immediate store with an BP relative destination address.
 *
 * @note May trash IEMNATIVE_REG_FIXED_TMP0.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreImm64ByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offDisp, uint64_t uImm64)
{
#ifdef RT_ARCH_AMD64
    if ((int64_t)uImm64 == (int32_t)uImm64)
    {
        /* mov qword [rbp + offDisp], imm32 - sign extended */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 11);
        pbCodeBuf[off++] = X86_OP_REX_W;
        pbCodeBuf[off++] = 0xc7;
        if (offDisp < 128 && offDisp >= -128)
        {
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, 0, X86_GREG_xBP);
            pbCodeBuf[off++] = (uint8_t)offDisp;
        }
        else
        {
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, 0, X86_GREG_xBP);
            pbCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
            pbCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
            pbCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
            pbCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
        }
        pbCodeBuf[off++] = RT_BYTE1(uImm64);
        pbCodeBuf[off++] = RT_BYTE2(uImm64);
        pbCodeBuf[off++] = RT_BYTE3(uImm64);
        pbCodeBuf[off++] = RT_BYTE4(uImm64);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        return off;
    }
#endif

    /* Load tmp0, imm64; Store tmp to bp+disp. */
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, uImm64);
    return iemNativeEmitStoreGprByBp(pReNative, off, offDisp, IEMNATIVE_REG_FIXED_TMP0);
}

#if defined(RT_ARCH_ARM64)

/**
 * Common bit of iemNativeEmitLoadGprFromVCpuU64 and friends.
 *
 * @note Odd and large @a offDisp values requires a temporary, unless it's a
 *       load and @a iGprReg differs from @a iGprBase.  Will assert / throw if
 *       caller does not heed this.
 *
 * @note DON'T try this with prefetch.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGprByGprLdStEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprReg, uint8_t iGprBase, int32_t offDisp,
                            ARMV8A64INSTRLDSTTYPE enmOperation, unsigned cbData, uint8_t iGprTmp = UINT8_MAX)
{
    if ((uint32_t)offDisp < _4K * cbData && !((uint32_t)offDisp & (cbData - 1)))
    {
        /* Use the unsigned variant of ldr Wt, [<Xn|SP>, #off]. */
        pCodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, iGprBase, (uint32_t)offDisp / cbData);
    }
    else if (   (   !ARMV8A64INSTRLDSTTYPE_IS_STORE(enmOperation)
                 && iGprReg != iGprBase)
             || iGprTmp != UINT8_MAX)
    {
        /* The offset is too large, so we must load it into a register and use
           ldr Wt, [<Xn|SP>, (<Wm>|<Xm>)]. */
        /** @todo reduce by offVCpu by >> 3 or >> 2? if it saves instructions? */
        if (iGprTmp == UINT8_MAX)
            iGprTmp = iGprReg;
        off = iemNativeEmitLoadGprImmEx(pCodeBuf, off, iGprTmp, (int64_t)offDisp);
        pCodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGprReg, iGprBase, iGprTmp);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif
    return off;
}

/**
 * Common bit of iemNativeEmitLoadGprFromVCpuU64 and friends.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGprByGprLdSt(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprReg,
                          uint8_t iGprBase, int32_t offDisp, ARMV8A64INSTRLDSTTYPE enmOperation, unsigned cbData)
{
    /*
     * There are a couple of ldr variants that takes an immediate offset, so
     * try use those if we can, otherwise we have to use the temporary register
     * help with the addressing.
     */
    if ((uint32_t)offDisp < _4K * cbData && !((uint32_t)offDisp & (cbData - 1)))
    {
        /* Use the unsigned variant of ldr Wt, [<Xn|SP>, #off]. */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGprReg, iGprBase, (uint32_t)offDisp / cbData);
    }
    else
    {
        /* The offset is too large, so we must load it into a register and use
           ldr Wt, [<Xn|SP>, (<Wm>|<Xm>)]. */
        /** @todo reduce by offVCpu by >> 3 or >> 2? if it saves instructions? */
        uint8_t const idxTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (int64_t)offDisp);

        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGprReg, iGprBase, idxTmpReg);

        iemNativeRegFreeTmpImm(pReNative, idxTmpReg);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}

#endif /* RT_ARCH_ARM64 */

/**
 * Emits a 64-bit GPR load via a GPR base address with a displacement.
 *
 * @note ARM64: Misaligned @a offDisp values and values not in the
 *       -0x7ff8...0x7ff8 range will require a temporary register (@a iGprTmp) if
 *       @a iGprReg and @a iGprBase are the same. Will assert / throw if caller
 *       does not heed this.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprBase,
                            int32_t offDisp, uint8_t iGprTmp = UINT8_MAX)
{
#ifdef RT_ARCH_AMD64
    /* mov reg64, mem64 */
    pCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprBase < 8 ? 0 : X86_OP_REX_B);
    pCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByGprDisp(pCodeBuf, off, iGprDst, iGprBase, offDisp);
    RT_NOREF(iGprTmp);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByGprLdStEx(pCodeBuf, off, iGprDst, iGprBase, offDisp,
                                      kArmv8A64InstrLdStType_Ld_Dword, sizeof(uint64_t), iGprTmp);

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 64-bit GPR load via a GPR base address with a displacement.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprBase, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprByGprEx(iemNativeInstrBufEnsure(pReNative, off, 8), off, iGprDst, iGprBase, offDisp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByGprLdSt(pReNative, off, iGprDst, iGprBase, offDisp, kArmv8A64InstrLdStType_Ld_Dword, sizeof(uint64_t));

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 32-bit GPR load via a GPR base address with a displacement.
 * @note Bits 63 thru 32 in @a iGprDst will be cleared.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGpr32ByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprBase, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* mov reg32, mem32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    if (iGprDst >= 8 || iGprBase >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprBase < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByGprDisp(pbCodeBuf, off, iGprDst, iGprBase, offDisp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitGprByGprLdSt(pReNative, off, iGprDst, iGprBase, offDisp, kArmv8A64InstrLdStType_Ld_Word, sizeof(uint32_t));

#else
# error "port me"
#endif
    return off;
}


/*********************************************************************************************************************************
*   Subtraction and Additions                                                                                                    *
*********************************************************************************************************************************/

/**
 * Emits subtracting a 64-bit GPR from another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitSubTwoGprs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSubtrahend)
{
#if defined(RT_ARCH_AMD64)
    /* sub Gv,Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = (iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R)
                     | (iGprSubtrahend < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x2b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSubtrahend & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSubReg(iGprDst, iGprDst, iGprSubtrahend);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits subtracting a 32-bit GPR from another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitSubTwoGprs32Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSubtrahend)
{
#if defined(RT_ARCH_AMD64)
    /* sub Gv,Ev */
    if (iGprDst >= 8 || iGprSubtrahend >= 8)
        pCodeBuf[off++] = (iGprDst        < 8 ? 0 : X86_OP_REX_R)
                        | (iGprSubtrahend < 8 ? 0 : X86_OP_REX_B);
    pCodeBuf[off++] = 0x2b;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSubtrahend & 7);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrSubReg(iGprDst, iGprDst, iGprSubtrahend, false /*f64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits subtracting a 32-bit GPR from another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitSubTwoGprs32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSubtrahend)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitSubTwoGprs32Ex(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst, iGprSubtrahend);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitSubTwoGprs32Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprSubtrahend);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GPR subtract with a signed immediate subtrahend.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitSubGprImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iSubtrahend)
{
    /* sub gprdst, imm8/imm32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_B;
    if (iSubtrahend < 128 && iSubtrahend >= -128)
    {
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pbCodeBuf[off++] = (uint8_t)iSubtrahend;
    }
    else
    {
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pbCodeBuf[off++] = RT_BYTE1(iSubtrahend);
        pbCodeBuf[off++] = RT_BYTE2(iSubtrahend);
        pbCodeBuf[off++] = RT_BYTE3(iSubtrahend);
        pbCodeBuf[off++] = RT_BYTE4(iSubtrahend);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#endif


/**
 * Emits a 32-bit GPR subtract with a signed immediate subtrahend.
 *
 * This will optimize using DEC/INC/whatever, so try avoid flag dependencies.
 *
 * @note ARM64: Larger constants will require a temporary register.  Failing to
 *       specify one when needed will trigger fatal assertion / throw.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitSubGpr32ImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, int32_t iSubtrahend,
                           uint8_t iGprTmp = UINT8_MAX)
{
#ifdef RT_ARCH_AMD64
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if (iSubtrahend == 1)
    {
        /* dec r/m32 */
        pCodeBuf[off++] = 0xff;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 1, iGprDst & 7);
    }
    else if (iSubtrahend == -1)
    {
        /* inc r/m32 */
        pCodeBuf[off++] = 0xff;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    }
    else if (iSubtrahend < 128 && iSubtrahend >= -128)
    {
        /* sub r/m32, imm8 */
        pCodeBuf[off++] = 0x83;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pCodeBuf[off++] = (uint8_t)iSubtrahend;
    }
    else
    {
        /* sub r/m32, imm32 */
        pCodeBuf[off++] = 0x81;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pCodeBuf[off++] = RT_BYTE1(iSubtrahend);
        pCodeBuf[off++] = RT_BYTE2(iSubtrahend);
        pCodeBuf[off++] = RT_BYTE3(iSubtrahend);
        pCodeBuf[off++] = RT_BYTE4(iSubtrahend);
    }
    RT_NOREF(iGprTmp);

#elif defined(RT_ARCH_ARM64)
    uint32_t uAbsSubtrahend = RT_ABS(iSubtrahend);
    if (uAbsSubtrahend < 4096)
    {
        if (iSubtrahend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, uAbsSubtrahend, false /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, uAbsSubtrahend, false /*f64Bit*/);
    }
    else if (uAbsSubtrahend <= 0xfff000 && !(uAbsSubtrahend & 0xfff))
    {
        if (iSubtrahend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, uAbsSubtrahend >> 12,
                                                       false /*f64Bit*/, false /*fSetFlags*/, true /*fShift*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, uAbsSubtrahend >> 12,
                                                       false /*f64Bit*/, false /*fSetFlags*/, true /*fShift*/);
    }
    else if (iGprTmp != UINT8_MAX)
    {
        off = iemNativeEmitLoadGpr32ImmEx(pCodeBuf, off, iGprTmp, (uint32_t)iSubtrahend);
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(iGprDst, iGprDst, iGprTmp);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits adding a 64-bit GPR to another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAddTwoGprsEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend)
{
#if defined(RT_ARCH_AMD64)
    /* add Gv,Ev */
    pCodeBuf[off++] = (iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R)
                    | (iGprAddend < 8 ? 0 : X86_OP_REX_B);
    pCodeBuf[off++] = 0x03;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprAddend & 7);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprDst, iGprAddend);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits adding a 64-bit GPR to another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddTwoGprs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAddTwoGprsEx(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst, iGprAddend);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitAddTwoGprsEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprAddend);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits adding a 64-bit GPR to another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAddTwoGprs32Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend)
{
#if defined(RT_ARCH_AMD64)
    /* add Gv,Ev */
    if (iGprDst >= 8 || iGprAddend >= 8)
        pCodeBuf[off++] = (iGprDst    >= 8 ? X86_OP_REX_R : 0)
                        | (iGprAddend >= 8 ? X86_OP_REX_B : 0);
    pCodeBuf[off++] = 0x03;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprAddend & 7);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprDst, iGprAddend, false /*f64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits adding a 64-bit GPR to another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddTwoGprs32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAddTwoGprs32Ex(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst, iGprAddend);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitAddTwoGprs32Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprAddend);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a 64-bit GPR additions with a 8-bit signed immediate.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGprImm8Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    /* add or inc */
    pCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (iImm8 != 1)
    {
        pCodeBuf[off++] = 0x83;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pCodeBuf[off++] = (uint8_t)iImm8;
    }
    else
    {
        pCodeBuf[off++] = 0xff;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    if (iImm8 >= 0)
        pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, (uint8_t)iImm8);
    else
        pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, (uint8_t)-iImm8);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits a 64-bit GPR additions with a 8-bit signed immediate.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGprImm8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAddGprImm8Ex(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, iImm8);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitAddGprImm8Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iImm8);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a 32-bit GPR additions with a 8-bit signed immediate.
 * @note Bits 32 thru 63 in the GPR will be zero after the operation.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAddGpr32Imm8Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    /* add or inc */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if (iImm8 != 1)
    {
        pCodeBuf[off++] = 0x83;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pCodeBuf[off++] = (uint8_t)iImm8;
    }
    else
    {
        pCodeBuf[off++] = 0xff;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    if (iImm8 >= 0)
        pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint8_t)iImm8, false /*f64Bit*/);
    else
        pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint8_t)-iImm8, false /*f64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits a 32-bit GPR additions with a 8-bit signed immediate.
 * @note Bits 32 thru 63 in the GPR will be zero after the operation.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGpr32Imm8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAddGpr32Imm8Ex(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, iImm8);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitAddGpr32Imm8Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iImm8);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a 64-bit GPR additions with a 64-bit signed addend.
 *
 * @note Will assert / throw if @a iGprTmp is not specified when needed.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitAddGprImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, int64_t iAddend, uint8_t iGprTmp = UINT8_MAX)
{
#if defined(RT_ARCH_AMD64)
    if ((int8_t)iAddend == iAddend)
        return iemNativeEmitAddGprImm8Ex(pCodeBuf, off, iGprDst, (int8_t)iAddend);

    if ((int32_t)iAddend == iAddend)
    {
        /* add grp, imm32 */
        pCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
        pCodeBuf[off++] = 0x81;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pCodeBuf[off++] = RT_BYTE1((uint32_t)iAddend);
        pCodeBuf[off++] = RT_BYTE2((uint32_t)iAddend);
        pCodeBuf[off++] = RT_BYTE3((uint32_t)iAddend);
        pCodeBuf[off++] = RT_BYTE4((uint32_t)iAddend);
    }
    else if (iGprTmp != UINT8_MAX)
    {
        off = iemNativeEmitLoadGprImmEx(pCodeBuf, off, iGprTmp, iAddend);

        /* add dst, tmpreg  */
        pCodeBuf[off++] = (iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R)
                        | (iGprTmp < 8 ? 0 : X86_OP_REX_B);
        pCodeBuf[off++] = 0x03;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprTmp & 7);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#elif defined(RT_ARCH_ARM64)
    uint64_t const uAbsAddend = (uint64_t)RT_ABS(iAddend);
    if (uAbsAddend < 4096)
    {
        if (iAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, (uint32_t)uAbsAddend);
        else
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, (uint32_t)uAbsAddend);
    }
    else if (uAbsAddend <= 0xfff000 && !(uAbsAddend & 0xfff))
    {
        if (iAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, (uint32_t)uAbsAddend >> 12,
                                                       true /*f64Bit*/, true /*fShift12*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, (uint32_t)uAbsAddend >> 12,
                                                       true /*f64Bit*/, true /*fShift12*/);
    }
    else if (iGprTmp != UINT8_MAX)
    {
        off = iemNativeEmitLoadGprImmEx(pCodeBuf, off, iGprTmp, iAddend);
        pCodeBuf[off++] = Armv8A64MkInstrAddReg(iGprDst, iGprDst, iGprTmp);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits a 64-bit GPR additions with a 64-bit signed addend.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGprImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int64_t iAddend)
{
#if defined(RT_ARCH_AMD64)
    if (iAddend <= INT8_MAX && iAddend >= INT8_MIN)
        return iemNativeEmitAddGprImm8(pReNative, off, iGprDst, (int8_t)iAddend);

    if (iAddend <= INT32_MAX && iAddend >= INT32_MIN)
    {
        /* add grp, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        pbCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)iAddend);
        pbCodeBuf[off++] = RT_BYTE2((uint32_t)iAddend);
        pbCodeBuf[off++] = RT_BYTE3((uint32_t)iAddend);
        pbCodeBuf[off++] = RT_BYTE4((uint32_t)iAddend);
    }
    else
    {
        /* Best to use a temporary register to deal with this in the simplest way: */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (uint64_t)iAddend);

        /* add dst, tmpreg  */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
        pbCodeBuf[off++] = (iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R)
                         | (iTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x03;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iTmpReg & 7);

        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#elif defined(RT_ARCH_ARM64)
    if ((uint64_t)RT_ABS(iAddend) < RT_BIT_32(12))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (iAddend >= 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint32_t)iAddend);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint32_t)-iAddend);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (uint64_t)iAddend);

        /* add gprdst, gprdst, tmpreg */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprDst, iTmpReg);

        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a 32-bit GPR additions with a 32-bit signed immediate.
 * @note Bits 32 thru 63 in the GPR will be zero after the operation.
 * @note For ARM64 the iAddend value must be in the range 0x000..0xfff,
 *       or that range shifted 12 bits to the left (e.g. 0x1000..0xfff000 with
 *       the lower 12 bits always zero).  The negative ranges are also allowed,
 *       making it behave like a subtraction.  If the constant does not conform,
 *       bad stuff will happen.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitAddGpr32ImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, int32_t iAddend)
{
#if defined(RT_ARCH_AMD64)
    if (iAddend <= INT8_MAX && iAddend >= INT8_MIN)
        return iemNativeEmitAddGpr32Imm8Ex(pCodeBuf, off, iGprDst, (int8_t)iAddend);

    /* add grp, imm32 */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    pCodeBuf[off++] = 0x81;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    pCodeBuf[off++] = RT_BYTE1((uint32_t)iAddend);
    pCodeBuf[off++] = RT_BYTE2((uint32_t)iAddend);
    pCodeBuf[off++] = RT_BYTE3((uint32_t)iAddend);
    pCodeBuf[off++] = RT_BYTE4((uint32_t)iAddend);

#elif defined(RT_ARCH_ARM64)
    uint32_t const uAbsAddend = (uint32_t)RT_ABS(iAddend);
    if (uAbsAddend <= 0xfff)
    {
        if (iAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, uAbsAddend, false /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/,  iGprDst, iGprDst, uAbsAddend, false /*f64Bit*/);
    }
    else if (uAbsAddend <= 0xfff000 && !(uAbsAddend & 0xfff))
    {
        if (iAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, uAbsAddend >> 12,
                                                          false /*f64Bit*/, true /*fShift12*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/,  iGprDst, iGprDst, uAbsAddend >> 12,
                                                          false /*f64Bit*/, true /*fShift12*/);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits a 32-bit GPR additions with a 32-bit signed immediate.
 * @note Bits 32 thru 63 in the GPR will be zero after the operation.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGpr32Imm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iAddend)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAddGpr32ImmEx(iemNativeInstrBufEnsure(pReNative, off, 7), off, iGprDst, iAddend);

#elif defined(RT_ARCH_ARM64)
    if ((uint64_t)RT_ABS(iAddend) < RT_BIT_32(12))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (iAddend >= 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint32_t)iAddend, false /*f64Bit*/);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint32_t)-iAddend, false /*f64Bit*/);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (uint32_t)iAddend);

        /* add gprdst, gprdst, tmpreg */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprDst, iTmpReg, false /*f64Bit*/);

        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Adds two 64-bit GPRs together, storing the result in a third register.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitGprEqGprPlusGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend1, uint8_t iGprAddend2)
{
#ifdef RT_ARCH_AMD64
    if (iGprDst != iGprAddend1 && iGprDst != iGprAddend2)
    {
        /** @todo consider LEA */
        off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, iGprDst, iGprAddend1);
        off = iemNativeEmitAddTwoGprsEx(pCodeBuf, off, iGprDst, iGprAddend2);
    }
    else
        off = iemNativeEmitAddTwoGprsEx(pCodeBuf, off, iGprDst, iGprDst != iGprAddend1 ? iGprAddend1 : iGprAddend2);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrAddReg(iGprDst, iGprAddend1, iGprAddend2);

#else
# error "Port me!"
#endif
    return off;
}



/**
 * Adds two 32-bit GPRs together, storing the result in a third register.
 * @note Bits 32 thru 63 in @a iGprDst will be zero after the operation.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitGpr32EqGprPlusGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend1, uint8_t iGprAddend2)
{
#ifdef RT_ARCH_AMD64
    if (iGprDst != iGprAddend1 && iGprDst != iGprAddend2)
    {
        /** @todo consider LEA */
        off = iemNativeEmitLoadGprFromGpr32Ex(pCodeBuf, off, iGprDst, iGprAddend1);
        off = iemNativeEmitAddTwoGprs32Ex(pCodeBuf, off, iGprDst, iGprAddend2);
    }
    else
        off = iemNativeEmitAddTwoGprs32Ex(pCodeBuf, off, iGprDst, iGprDst != iGprAddend1 ? iGprAddend1 : iGprAddend2);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrAddReg(iGprDst, iGprAddend1, iGprAddend2, false /*f64Bit*/);

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Adds a 64-bit GPR and a 64-bit unsigned constant, storing the result in a
 * third register.
 *
 * @note The ARM64 version does not work for non-trivial constants if the
 *       two registers are the same.  Will assert / throw exception.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGprEqGprPlusImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend, int64_t iImmAddend)
{
#ifdef RT_ARCH_AMD64
    /** @todo consider LEA */
    if ((int8_t)iImmAddend == iImmAddend)
    {
        off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, iGprDst, iGprAddend);
        off = iemNativeEmitAddGprImm8Ex(pCodeBuf, off, iGprDst, (int8_t)iImmAddend);
    }
    else
    {
        off = iemNativeEmitLoadGprImmEx(pCodeBuf, off, iGprDst, iImmAddend);
        off = iemNativeEmitAddTwoGprsEx(pCodeBuf, off, iGprDst, iGprAddend);
    }

#elif defined(RT_ARCH_ARM64)
    uint64_t const uAbsImmAddend = RT_ABS(iImmAddend);
    if (uAbsImmAddend < 4096)
    {
        if (iImmAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprAddend, uAbsImmAddend);
        else
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprAddend, uAbsImmAddend);
    }
    else if (uAbsImmAddend <= 0xfff000 && !(uAbsImmAddend & 0xfff))
    {
        if (iImmAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, uAbsImmAddend >> 12, true /*f64Bit*/, true /*fShift12*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, uAbsImmAddend >> 12, true /*f64Bit*/, true /*fShift12*/);
    }
    else if (iGprDst != iGprAddend)
    {
        off = iemNativeEmitLoadGprImmEx(pCodeBuf, off, iGprDst, (uint64_t)iImmAddend);
        off = iemNativeEmitAddTwoGprsEx(pCodeBuf, off, iGprDst, iGprAddend);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Adds a 32-bit GPR and a 32-bit unsigned constant, storing the result in a
 * third register.
 *
 * @note Bits 32 thru 63 in @a iGprDst will be zero after the operation.
 *
 * @note The ARM64 version does not work for non-trivial constants if the
 *       two registers are the same.  Will assert / throw exception.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGpr32EqGprPlusImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend, int32_t iImmAddend)
{
#ifdef RT_ARCH_AMD64
    /** @todo consider LEA */
    if ((int8_t)iImmAddend == iImmAddend)
    {
        off = iemNativeEmitLoadGprFromGpr32Ex(pCodeBuf, off, iGprDst, iGprAddend);
        off = iemNativeEmitAddGpr32Imm8Ex(pCodeBuf, off, iGprDst, (int8_t)iImmAddend);
    }
    else
    {
        off = iemNativeEmitLoadGpr32ImmEx(pCodeBuf, off, iGprDst, iImmAddend);
        off = iemNativeEmitAddTwoGprsEx(pCodeBuf, off, iGprDst, iGprAddend);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t const uAbsImmAddend = RT_ABS(iImmAddend);
    if (uAbsImmAddend < 4096)
    {
        if (iImmAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprAddend, uAbsImmAddend, false /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprAddend, uAbsImmAddend, false /*f64Bit*/);
    }
    else if (uAbsImmAddend <= 0xfff000 && !(uAbsImmAddend & 0xfff))
    {
        if (iImmAddend >= 0)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(iGprDst, iGprDst, uAbsImmAddend >> 12, false /*f64Bit*/, true /*fShift12*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(iGprDst, iGprDst, uAbsImmAddend >> 12, false /*f64Bit*/, true /*fShift12*/);
    }
    else if (iGprDst != iGprAddend)
    {
        off = iemNativeEmitLoadGpr32ImmEx(pCodeBuf, off, iGprDst, (uint32_t)iImmAddend);
        off = iemNativeEmitAddTwoGprs32Ex(pCodeBuf, off, iGprDst, iGprAddend);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me!"
#endif
    return off;
}


/*********************************************************************************************************************************
*   Unary Operations                                                                                                             *
*********************************************************************************************************************************/

/**
 * Emits code for two complement negation of a 64-bit GPR.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitNegGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    /* neg Ev */
    pCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    pCodeBuf[off++] = 0xf7;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 3, iGprDst & 7);

#elif defined(RT_ARCH_ARM64)
    /* sub dst, xzr, dst */
    pCodeBuf[off++] = Armv8A64MkInstrNeg(iGprDst);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for two complement negation of a 64-bit GPR.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitNegGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitNegGprEx(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitNegGprEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for two complement negation of a 32-bit GPR.
 * @note bit 32 thru 63 are set to zero.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitNegGpr32Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    /* neg Ev */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    pCodeBuf[off++] = 0xf7;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 3, iGprDst & 7);

#elif defined(RT_ARCH_ARM64)
    /* sub dst, xzr, dst */
    pCodeBuf[off++] = Armv8A64MkInstrNeg(iGprDst, false /*f64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for two complement negation of a 32-bit GPR.
 * @note bit 32 thru 63 are set to zero.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitNegGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitNegGpr32Ex(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitNegGpr32Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}



/*********************************************************************************************************************************
*   Bit Operations                                                                                                               *
*********************************************************************************************************************************/

/**
 * Emits code for clearing bits 16 thru 63 in the GPR.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitClear16UpGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    /* movzx Gv,Ew */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprDst & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
# if 1
    pu32CodeBuf[off++] = Armv8A64MkInstrUxth(iGprDst, iGprDst);
# else
    ///* This produces 0xffff; 0x4f: N=1 imms=001111 (immr=0) => size=64 length=15 */
    //pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprDst, 0x4f);
# endif
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for AND'ing two 64-bit GPRs.
 *
 * @note When fSetFlags=true, JZ/JNZ jumps can be used afterwards on both AMD64
 *       and ARM64 hosts.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAndGprByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x23;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);
    RT_NOREF(fSetFlags);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (!fSetFlags)
        pu32CodeBuf[off++] = Armv8A64MkInstrAnd(iGprDst, iGprDst, iGprSrc);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrAnds(iGprDst, iGprDst, iGprSrc);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for AND'ing two 32-bit GPRs.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAndGpr32ByGpr32Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    if (iGprDst >= 8 || iGprSrc >= 8)
        pCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pCodeBuf[off++] = 0x23;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);
    RT_NOREF(fSetFlags);

#elif defined(RT_ARCH_ARM64)
    if (!fSetFlags)
        pCodeBuf[off++] = Armv8A64MkInstrAnd(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);
    else
        pCodeBuf[off++] = Armv8A64MkInstrAnds(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for AND'ing two 32-bit GPRs.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAndGpr32ByGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAndGpr32ByGpr32Ex(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprDst, iGprSrc, fSetFlags);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitAndGpr32ByGpr32Ex(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, iGprSrc, fSetFlags);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for AND'ing a 64-bit GPRs with a constant.
 *
 * @note When fSetFlags=true, JZ/JNZ jumps can be used afterwards on both AMD64
 *       and ARM64 hosts.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAndGprByImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint64_t uImm, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    if ((int64_t)uImm == (int8_t)uImm)
    {
        /* and Ev, imm8 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = (uint8_t)uImm;
    }
    else if ((int64_t)uImm == (int32_t)uImm)
    {
        /* and Ev, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = RT_BYTE1(uImm);
        pbCodeBuf[off++] = RT_BYTE2(uImm);
        pbCodeBuf[off++] = RT_BYTE3(uImm);
        pbCodeBuf[off++] = RT_BYTE4(uImm);
    }
    else
    {
        /* Use temporary register for the 64-bit immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, uImm);
        off = iemNativeEmitAndGprByGpr(pReNative, off, iGprDst, iTmpReg);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }
    RT_NOREF(fSetFlags);

#elif defined(RT_ARCH_ARM64)
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask64ToImmRImmS(uImm, &uImmNandS, &uImmR))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (!fSetFlags)
            pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprDst, uImmNandS, uImmR);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAndsImm(iGprDst, iGprDst, uImmNandS, uImmR);
    }
    else
    {
        /* Use temporary register for the 64-bit immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, uImm);
        off = iemNativeEmitAndGprByGpr(pReNative, off, iGprDst, iTmpReg, fSetFlags);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for AND'ing an 32-bit GPRs with a constant.
 * @note Bits 32 thru 63 in the destination will be zero after the operation.
 * @note For ARM64 this only supports @a uImm values that can be expressed using
 *       the two 6-bit immediates of the AND/ANDS instructions.  The caller must
 *       make sure this is possible!
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitAndGpr32ByImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint32_t uImm, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    /* and Ev, imm */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if ((int32_t)uImm == (int8_t)uImm)
    {
        pCodeBuf[off++] = 0x83;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pCodeBuf[off++] = (uint8_t)uImm;
    }
    else
    {
        pCodeBuf[off++] = 0x81;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm);
        pCodeBuf[off++] = RT_BYTE2(uImm);
        pCodeBuf[off++] = RT_BYTE3(uImm);
        pCodeBuf[off++] = RT_BYTE4(uImm);
    }
    RT_NOREF(fSetFlags);

#elif defined(RT_ARCH_ARM64)
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask32ToImmRImmS(uImm, &uImmNandS, &uImmR))
    {
        if (!fSetFlags)
            pCodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprDst, uImmNandS, uImmR, false /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAndsImm(iGprDst, iGprDst, uImmNandS, uImmR, false /*f64Bit*/);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for AND'ing an 32-bit GPRs with a constant.
 *
 * @note Bits 32 thru 63 in the destination will be zero after the operation.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAndGpr32ByImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint32_t uImm, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitAndGpr32ByImmEx(iemNativeInstrBufEnsure(pReNative, off, 7), off, iGprDst, uImm, fSetFlags);

#elif defined(RT_ARCH_ARM64)
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask32ToImmRImmS(uImm, &uImmNandS, &uImmR))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (!fSetFlags)
            pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprDst, uImmNandS, uImmR, false /*f64Bit*/);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAndsImm(iGprDst, iGprDst, uImmNandS, uImmR, false /*f64Bit*/);
    }
    else
    {
        /* Use temporary register for the 64-bit immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, uImm);
        off = iemNativeEmitAndGpr32ByGpr32(pReNative, off, iGprDst, iTmpReg, fSetFlags);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for AND'ing an 32-bit GPRs with a constant.
 *
 * @note For ARM64 any complicated immediates w/o a AND/ANDS compatible
 *       encoding will assert / throw exception if @a iGprDst and @a iGprSrc are
 *       the same.
 *
 * @note Bits 32 thru 63 in the destination will be zero after the operation.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitGpr32EqGprAndImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc, uint32_t uImm,
                                bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitLoadGpr32ImmEx(pCodeBuf, off, iGprDst, uImm);
    off = iemNativeEmitAndGpr32ByGpr32Ex(pCodeBuf, off, iGprDst, iGprSrc);
    RT_NOREF(fSetFlags);

#elif defined(RT_ARCH_ARM64)
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask32ToImmRImmS(uImm, &uImmNandS, &uImmR))
    {
        if (!fSetFlags)
            pCodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprSrc, uImmNandS, uImmR, false /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAndsImm(iGprDst, iGprSrc, uImmNandS, uImmR, false /*f64Bit*/);
    }
    else if (iGprDst != iGprSrc)
    {
        off = iemNativeEmitLoadGpr32ImmEx(pCodeBuf, off, iGprDst, uImm);
        off = iemNativeEmitAndGpr32ByGpr32Ex(pCodeBuf, off, iGprDst, iGprSrc, fSetFlags);
    }
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for OR'ing two 64-bit GPRs.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitOrGprByGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* or Gv, Ev */
    pCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pCodeBuf[off++] = 0x0b;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrOrr(iGprDst, iGprDst, iGprSrc);

#else
# error "Port me"
#endif
    return off;
}



/**
 * Emits code for XOR'ing two 64-bit GPRs.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitXorGprByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrEor(iGprDst, iGprDst, iGprSrc);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for XOR'ing two 32-bit GPRs.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitXorGpr32ByGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrEor(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for XOR'ing an 32-bit GPRs with a constant.
 * @note Bits 32 thru 63 in the destination will be zero after the operation.
 * @note For ARM64 this only supports @a uImm values that can be expressed using
 *       the two 6-bit immediates of the EOR instructions.  The caller must make
 *       sure this is possible!
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitXorGpr32ByImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint32_t uImm)
{
#if defined(RT_ARCH_AMD64)
    /* and Ev, imm */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if ((int32_t)uImm == (int8_t)uImm)
    {
        pCodeBuf[off++] = 0x83;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 6, iGprDst & 7);
        pCodeBuf[off++] = (uint8_t)uImm;
    }
    else
    {
        pCodeBuf[off++] = 0x81;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 6, iGprDst & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm);
        pCodeBuf[off++] = RT_BYTE2(uImm);
        pCodeBuf[off++] = RT_BYTE3(uImm);
        pCodeBuf[off++] = RT_BYTE4(uImm);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask32ToImmRImmS(uImm, &uImmNandS, &uImmR))
        pCodeBuf[off++] = Armv8A64MkInstrEorImm(iGprDst, iGprDst, uImmNandS, uImmR, false /*f64Bit*/);
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me"
#endif
    return off;
}


/*********************************************************************************************************************************
*   Shifting                                                                                                                     *
*********************************************************************************************************************************/

/**
 * Emits code for shifting a GPR a fixed number of bits to the left.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitShiftGprLeftEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* shl dst, cShift */
    pCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (cShift != 1)
    {
        pCodeBuf[off++] = 0xc1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pCodeBuf[off++] = cShift;
    }
    else
    {
        pCodeBuf[off++] = 0xd1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrLslImm(iGprDst, iGprDst, cShift);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for shifting a GPR a fixed number of bits to the left.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGprLeft(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitShiftGprLeftEx(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, cShift);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitShiftGprLeftEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, cShift);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for shifting a 32-bit GPR a fixed number of bits to the left.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitShiftGpr32LeftEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 32);

#if defined(RT_ARCH_AMD64)
    /* shl dst, cShift */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if (cShift != 1)
    {
        pCodeBuf[off++] = 0xc1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pCodeBuf[off++] = cShift;
    }
    else
    {
        pCodeBuf[off++] = 0xd1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrLslImm(iGprDst, iGprDst, cShift, false /*64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for shifting a 32-bit GPR a fixed number of bits to the left.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGpr32Left(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitShiftGpr32LeftEx(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, cShift);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitShiftGpr32LeftEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, cShift);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for (unsigned) shifting a GPR a fixed number of bits to the right.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitShiftGprRightEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* shr dst, cShift */
    pCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (cShift != 1)
    {
        pCodeBuf[off++] = 0xc1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pCodeBuf[off++] = cShift;
    }
    else
    {
        pCodeBuf[off++] = 0xd1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrLsrImm(iGprDst, iGprDst, cShift);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for (unsigned) shifting a GPR a fixed number of bits to the right.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGprRight(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitShiftGprRightEx(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, cShift);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitShiftGprRightEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, cShift);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for (unsigned) shifting a 32-bit GPR a fixed number of bits to the
 * right.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitShiftGpr32RightEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 32);

#if defined(RT_ARCH_AMD64)
    /* shr dst, cShift */
    if (iGprDst >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if (cShift != 1)
    {
        pCodeBuf[off++] = 0xc1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pCodeBuf[off++] = cShift;
    }
    else
    {
        pCodeBuf[off++] = 0xd1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrLsrImm(iGprDst, iGprDst, cShift, false /*64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for (unsigned) shifting a 32-bit GPR a fixed number of bits to the
 * right.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGpr32Right(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitShiftGpr32RightEx(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprDst, cShift);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitShiftGpr32RightEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprDst, cShift);
#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for (unsigned) shifting a 32-bit GPR a fixed number of bits to the
 * right and assigning it to a different GPR.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitGpr32EqGprShiftRightImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc, uint8_t cShift)
{
    Assert(cShift > 0); Assert(cShift < 32);
#if defined(RT_ARCH_AMD64)
    off = iemNativeEmitLoadGprFromGpr32Ex(pCodeBuf, off, iGprDst, iGprSrc);
    off = iemNativeEmitShiftGpr32RightEx(pCodeBuf, off, iGprDst, cShift);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrLsrImm(iGprDst, iGprSrc, cShift, false /*64Bit*/);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code for rotating a GPR a fixed number of bits to the left.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitRotateGprLeftEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* rol dst, cShift */
    pCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (cShift != 1)
    {
        pCodeBuf[off++] = 0xc1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pCodeBuf[off++] = cShift;
    }
    else
    {
        pCodeBuf[off++] = 0xd1;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrRorImm(iGprDst, iGprDst, cShift);

#else
# error "Port me"
#endif
    return off;
}



/*********************************************************************************************************************************
*   Compare and Testing                                                                                                          *
*********************************************************************************************************************************/


#ifdef RT_ARCH_ARM64
/**
 * Emits an ARM64 compare instruction.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpArm64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight,
                      bool f64Bit = true, uint32_t cShift = 0, ARMV8A64INSTRSHIFT enmShift = kArmv8A64InstrShift_Lsr)
{
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(true /*fSub*/, ARMV8_A64_REG_XZR /*iRegResult*/, iGprLeft, iGprRight,
                                                  f64Bit, true /*fSetFlags*/, cShift, enmShift);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#endif


/**
 * Emits a compare of two 64-bit GPRs, settings status flags/whatever for use
 * with conditional instruction.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitCmpGprWithGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    /* cmp Gv, Ev */
    pCodeBuf[off++] = X86_OP_REX_W | (iGprLeft >= 8 ? X86_OP_REX_R : 0) | (iGprRight >= 8 ? X86_OP_REX_B : 0);
    pCodeBuf[off++] = 0x3b;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprLeft & 7, iGprRight & 7);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrCmpReg(iGprLeft, iGprRight);

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a compare of two 64-bit GPRs, settings status flags/whatever for use
 * with conditional instruction.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpGprWithGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitCmpGprWithGprEx(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprLeft, iGprRight);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitCmpGprWithGprEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprLeft, iGprRight);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a compare of two 32-bit GPRs, settings status flags/whatever for use
 * with conditional instruction.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitCmpGpr32WithGprEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    /* cmp Gv, Ev */
    if (iGprLeft >= 8 || iGprRight >= 8)
        pCodeBuf[off++] = (iGprLeft >= 8 ? X86_OP_REX_R : 0) | (iGprRight >= 8 ? X86_OP_REX_B : 0);
    pCodeBuf[off++] = 0x3b;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprLeft & 7, iGprRight & 7);

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrCmpReg(iGprLeft, iGprRight, false /*f64Bit*/);

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a compare of two 32-bit GPRs, settings status flags/whatever for use
 * with conditional instruction.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpGpr32WithGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitCmpGpr32WithGprEx(iemNativeInstrBufEnsure(pReNative, off, 3), off, iGprLeft, iGprRight);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitCmpGpr32WithGprEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, iGprLeft, iGprRight);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a compare of a 64-bit GPR with a constant value, settings status
 * flags/whatever for use with conditional instruction.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpGprWithImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint64_t uImm)
{
#ifdef RT_ARCH_AMD64
    if (uImm <= UINT32_C(0xff))
    {
        /* cmp Ev, Ib */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprLeft >= 8 ? X86_OP_REX_B : 0);
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, iGprLeft & 7);
        pbCodeBuf[off++] = (uint8_t)uImm;
    }
    else if ((int64_t)uImm == (int32_t)uImm)
    {
        /* cmp Ev, imm */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprLeft >= 8 ? X86_OP_REX_B : 0);
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, iGprLeft & 7);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        pbCodeBuf[off++] = RT_BYTE1(uImm);
        pbCodeBuf[off++] = RT_BYTE2(uImm);
        pbCodeBuf[off++] = RT_BYTE3(uImm);
        pbCodeBuf[off++] = RT_BYTE4(uImm);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t const iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, uImm);
        off = iemNativeEmitCmpGprWithGpr(pReNative, off, iGprLeft, iTmpReg);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#elif defined(RT_ARCH_ARM64)
    /** @todo guess there are clevere things we can do here...   */
    if (uImm < _4K)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm,
                                                         true /*64Bit*/, true /*fSetFlags*/);
    }
    else if (uImm < RT_BIT_32(12+12) && (uImm & (_4K - 1)) == 0)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm >> 12,
                                                         true /*64Bit*/, true /*fSetFlags*/, true /*fShift12*/);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, uImm);
        off = iemNativeEmitCmpGprWithGpr(pReNative, off, iGprLeft, iTmpReg);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me!"
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a compare of a 32-bit GPR with a constant value, settings status
 * flags/whatever for use with conditional instruction.
 *
 * @note On ARM64 the @a uImm value must be in the range 0x000..0xfff or that
 *       shifted 12 bits to the left (e.g. 0x1000..0xfff0000 with the lower 12
 *       bits all zero).  Will release assert or throw exception if the caller
 *       violates this restriction.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitCmpGpr32WithImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprLeft, uint32_t uImm)
{
#ifdef RT_ARCH_AMD64
    if (iGprLeft >= 8)
        pCodeBuf[off++] = X86_OP_REX_B;
    if (uImm <= UINT32_C(0x7f))
    {
        /* cmp Ev, Ib */
        pCodeBuf[off++] = 0x83;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, iGprLeft & 7);
        pCodeBuf[off++] = (uint8_t)uImm;
    }
    else
    {
        /* cmp Ev, imm */
        pCodeBuf[off++] = 0x81;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, iGprLeft & 7);
        pCodeBuf[off++] = RT_BYTE1(uImm);
        pCodeBuf[off++] = RT_BYTE2(uImm);
        pCodeBuf[off++] = RT_BYTE3(uImm);
        pCodeBuf[off++] = RT_BYTE4(uImm);
    }

#elif defined(RT_ARCH_ARM64)
    /** @todo guess there are clevere things we can do here...   */
    if (uImm < _4K)
        pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm,
                                                      false /*64Bit*/, true /*fSetFlags*/);
    else if (uImm < RT_BIT_32(12+12) && (uImm & (_4K - 1)) == 0)
        pCodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm,
                                                      false /*64Bit*/, true /*fSetFlags*/, true /*fShift12*/);
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a compare of a 32-bit GPR with a constant value, settings status
 * flags/whatever for use with conditional instruction.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpGpr32WithImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint32_t uImm)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitCmpGpr32WithImmEx(iemNativeInstrBufEnsure(pReNative, off, 7), off, iGprLeft, uImm);

#elif defined(RT_ARCH_ARM64)
    /** @todo guess there are clevere things we can do here...   */
    if (uImm < _4K)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm,
                                                         false /*64Bit*/, true /*fSetFlags*/);
    }
    else if (uImm < RT_BIT_32(12+12) && (uImm & (_4K - 1)) == 0)
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm,
                                                         false /*64Bit*/, true /*fSetFlags*/, true /*fShift12*/);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, uImm);
        off = iemNativeEmitCmpGpr32WithGpr(pReNative, off, iGprLeft, iTmpReg);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me!"
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}



/*********************************************************************************************************************************
*   Branching                                                                                                                    *
*********************************************************************************************************************************/

/**
 * Emits a JMP rel32 / B imm19 to the given label.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitJmpToLabelEx(PIEMRECOMPILERSTATE pReNative, PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint32_t idxLabel)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    if (pReNative->paLabels[idxLabel].off != UINT32_MAX)
    {
        uint32_t offRel = pReNative->paLabels[idxLabel].off - (off + 2);
        if ((int32_t)offRel < 128 && (int32_t)offRel >= -128)
        {
            pCodeBuf[off++] = 0xeb;                 /* jmp rel8 */
            pCodeBuf[off++] = (uint8_t)offRel;
        }
        else
        {
            offRel -= 3;
            pCodeBuf[off++] = 0xe9;                 /* jmp rel32 */
            pCodeBuf[off++] = RT_BYTE1(offRel);
            pCodeBuf[off++] = RT_BYTE2(offRel);
            pCodeBuf[off++] = RT_BYTE3(offRel);
            pCodeBuf[off++] = RT_BYTE4(offRel);
        }
    }
    else
    {
        pCodeBuf[off++] = 0xe9;                     /* jmp rel32 */
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4);
        pCodeBuf[off++] = 0xfe;
        pCodeBuf[off++] = 0xff;
        pCodeBuf[off++] = 0xff;
        pCodeBuf[off++] = 0xff;
    }
    pCodeBuf[off++] = 0xcc;                         /* int3 poison */

#elif defined(RT_ARCH_ARM64)
    if (pReNative->paLabels[idxLabel].off != UINT32_MAX)
        pCodeBuf[off++] = Armv8A64MkInstrB(pReNative->paLabels[idxLabel].off - off);
    else
    {
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm26At0);
        pCodeBuf[off++] = Armv8A64MkInstrB(-1);
    }

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a JMP rel32 / B imm19 to the given label.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitJmpToLabelEx(pReNative, iemNativeInstrBufEnsure(pReNative, off, 6), off, idxLabel);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitJmpToLabelEx(pReNative, iemNativeInstrBufEnsure(pReNative, off, 1), off, idxLabel);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a JMP rel32 / B imm19 to a new undefined label.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitJmpToLabel(pReNative, off, idxLabel);
}

/** Condition type. */
#ifdef RT_ARCH_AMD64
typedef enum IEMNATIVEINSTRCOND : uint8_t
{
    kIemNativeInstrCond_o = 0,
    kIemNativeInstrCond_no,
    kIemNativeInstrCond_c,
    kIemNativeInstrCond_nc,
    kIemNativeInstrCond_e,
    kIemNativeInstrCond_ne,
    kIemNativeInstrCond_be,
    kIemNativeInstrCond_nbe,
    kIemNativeInstrCond_s,
    kIemNativeInstrCond_ns,
    kIemNativeInstrCond_p,
    kIemNativeInstrCond_np,
    kIemNativeInstrCond_l,
    kIemNativeInstrCond_nl,
    kIemNativeInstrCond_le,
    kIemNativeInstrCond_nle
} IEMNATIVEINSTRCOND;
#elif defined(RT_ARCH_ARM64)
typedef ARMV8INSTRCOND  IEMNATIVEINSTRCOND;
# define kIemNativeInstrCond_o      todo_conditional_codes
# define kIemNativeInstrCond_no     todo_conditional_codes
# define kIemNativeInstrCond_c      todo_conditional_codes
# define kIemNativeInstrCond_nc     todo_conditional_codes
# define kIemNativeInstrCond_e      kArmv8InstrCond_Eq
# define kIemNativeInstrCond_ne     kArmv8InstrCond_Ne
# define kIemNativeInstrCond_be     kArmv8InstrCond_Ls
# define kIemNativeInstrCond_nbe    kArmv8InstrCond_Hi
# define kIemNativeInstrCond_s      todo_conditional_codes
# define kIemNativeInstrCond_ns     todo_conditional_codes
# define kIemNativeInstrCond_p      todo_conditional_codes
# define kIemNativeInstrCond_np     todo_conditional_codes
# define kIemNativeInstrCond_l      kArmv8InstrCond_Lt
# define kIemNativeInstrCond_nl     kArmv8InstrCond_Ge
# define kIemNativeInstrCond_le     kArmv8InstrCond_Le
# define kIemNativeInstrCond_nle    kArmv8InstrCond_Gt
#else
# error "Port me!"
#endif


/**
 * Emits a Jcc rel32 / B.cc imm19 to the given label (ASSUMED requiring fixup).
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitJccToLabelEx(PIEMRECOMPILERSTATE pReNative, PIEMNATIVEINSTR pCodeBuf, uint32_t off,
                          uint32_t idxLabel, IEMNATIVEINSTRCOND enmCond)
{
    Assert(idxLabel < pReNative->cLabels);

    uint32_t const offLabel = pReNative->paLabels[idxLabel].off;
#ifdef RT_ARCH_AMD64
    if (offLabel >= off)
    {
        /* jcc rel32 */
        pCodeBuf[off++] = 0x0f;
        pCodeBuf[off++] = (uint8_t)enmCond | 0x80;
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4);
        pCodeBuf[off++] = 0x00;
        pCodeBuf[off++] = 0x00;
        pCodeBuf[off++] = 0x00;
        pCodeBuf[off++] = 0x00;
    }
    else
    {
        int32_t offDisp = offLabel - (off + 2);
        if ((int8_t)offDisp == offDisp)
        {
            /* jcc rel8 */
            pCodeBuf[off++] = (uint8_t)enmCond | 0x70;
            pCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
        }
        else
        {
            /* jcc rel32 */
            offDisp -= 4;
            pCodeBuf[off++] = 0x0f;
            pCodeBuf[off++] = (uint8_t)enmCond | 0x80;
            pCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
            pCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
            pCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
            pCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
        }
    }

#elif defined(RT_ARCH_ARM64)
    if (offLabel >= off)
    {
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5);
        pCodeBuf[off++] = Armv8A64MkInstrBCond(enmCond, -1);
    }
    else
    {
        Assert(off - offLabel <= 0x3ffffU);
        pCodeBuf[off++] = Armv8A64MkInstrBCond(enmCond, offLabel - off);
    }

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a Jcc rel32 / B.cc imm19 to the given label (ASSUMED requiring fixup).
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJccToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel, IEMNATIVEINSTRCOND enmCond)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitJccToLabelEx(pReNative, iemNativeInstrBufEnsure(pReNative, off, 6), off, idxLabel, enmCond);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitJccToLabelEx(pReNative, iemNativeInstrBufEnsure(pReNative, off, 1), off, idxLabel, enmCond);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a Jcc rel32 / B.cc imm19 to a new label.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJccToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                           IEMNATIVELABELTYPE enmLabelType, uint16_t uData, IEMNATIVEINSTRCOND enmCond)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, enmCond);
}


/**
 * Emits a JZ/JE rel32 / B.EQ imm19 to the given label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJzToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kIemNativeInstrCond_e);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kArmv8InstrCond_Eq);
#else
# error "Port me!"
#endif
}

/**
 * Emits a JZ/JE rel32 / B.EQ imm19 to a new label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJzToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                      IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kIemNativeInstrCond_e);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kArmv8InstrCond_Eq);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JNZ/JNE rel32 / B.NE imm19 to the given label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJnzToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kIemNativeInstrCond_ne);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kArmv8InstrCond_Ne);
#else
# error "Port me!"
#endif
}

/**
 * Emits a JNZ/JNE rel32 / B.NE imm19 to a new label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJnzToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                       IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kIemNativeInstrCond_ne);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kArmv8InstrCond_Ne);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JBE/JNA rel32 / B.LS imm19 to the given label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJbeToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kIemNativeInstrCond_be);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kArmv8InstrCond_Ls);
#else
# error "Port me!"
#endif
}

/**
 * Emits a JBE/JNA rel32 / B.LS imm19 to a new label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJbeToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                       IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kIemNativeInstrCond_be);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kArmv8InstrCond_Ls);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JA/JNBE rel32 / B.HI imm19 to the given label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJaToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kIemNativeInstrCond_nbe);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kArmv8InstrCond_Hi);
#else
# error "Port me!"
#endif
}

/**
 * Emits a JA/JNBE rel32 / B.HI imm19 to a new label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJaToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                      IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kIemNativeInstrCond_nbe);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kArmv8InstrCond_Hi);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JL/JNGE rel32 / B.LT imm19 to the given label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJlToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kIemNativeInstrCond_l);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, kArmv8InstrCond_Lt);
#else
# error "Port me!"
#endif
}

/**
 * Emits a JA/JNGE rel32 / B.HI imm19 to a new label.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJlToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                      IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kIemNativeInstrCond_l);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToNewLabel(pReNative, off, enmLabelType, uData, kArmv8InstrCond_Lt);
#else
# error "Port me!"
#endif
}


/**
 * Emits a Jcc rel32 / B.cc imm19 with a fixed displacement.
 *
 * @note The @a offTarget is the absolute jump target (unit is IEMNATIVEINSTR).
 *
 *       Only use hardcoded jumps forward when emitting for exactly one
 *       platform, otherwise apply iemNativeFixupFixedJump() to ensure hitting
 *       the right target address on all platforms!
 *
 *       Please also note that on x86 it is necessary pass off + 256 or higher
 *       for @a offTarget one believe the intervening code is more than 127
 *       bytes long.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitJccToFixedEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint32_t offTarget, IEMNATIVEINSTRCOND enmCond)
{
#ifdef RT_ARCH_AMD64
    /* jcc rel8 / rel32 */
    int32_t offDisp = (int32_t)(offTarget - (off + 2));
    if (offDisp < 128 && offDisp >= -128)
    {
        pCodeBuf[off++] = (uint8_t)enmCond | 0x70;
        pCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
    }
    else
    {
        offDisp -= 4;
        pCodeBuf[off++] = 0x0f;
        pCodeBuf[off++] = (uint8_t)enmCond | 0x80;
        pCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
        pCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
        pCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
        pCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrBCond(enmCond, (int32_t)(offTarget - off));

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a Jcc rel32 / B.cc imm19 with a fixed displacement.
 *
 * @note The @a offTarget is the absolute jump target (unit is IEMNATIVEINSTR).
 *
 *       Only use hardcoded jumps forward when emitting for exactly one
 *       platform, otherwise apply iemNativeFixupFixedJump() to ensure hitting
 *       the right target address on all platforms!
 *
 *       Please also note that on x86 it is necessary pass off + 256 or higher
 *       for @a offTarget one believe the intervening code is more than 127
 *       bytes long.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJccToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t offTarget, IEMNATIVEINSTRCOND enmCond)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitJccToFixedEx(iemNativeInstrBufEnsure(pReNative, off, 6), off, offTarget, enmCond);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitJccToFixedEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, offTarget, enmCond);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a JZ/JE rel32 / B.EQ imm19 with a fixed displacement.
 *
 * See notes on @a offTarget in the iemNativeEmitJccToFixed() documentation.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJzToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kIemNativeInstrCond_e);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kArmv8InstrCond_Eq);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JNZ/JNE rel32 / B.NE imm19 with a fixed displacement.
 *
 * See notes on @a offTarget in the iemNativeEmitJccToFixed() documentation.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJnzToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kIemNativeInstrCond_ne);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kArmv8InstrCond_Ne);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JBE/JNA rel32 / B.LS imm19 with a fixed displacement.
 *
 * See notes on @a offTarget in the iemNativeEmitJccToFixed() documentation.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJbeToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kIemNativeInstrCond_be);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kArmv8InstrCond_Ls);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JA/JNBE rel32 / B.HI imm19 with a fixed displacement.
 *
 * See notes on @a offTarget in the iemNativeEmitJccToFixed() documentation.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJaToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kIemNativeInstrCond_nbe);
#elif defined(RT_ARCH_ARM64)
    return iemNativeEmitJccToFixed(pReNative, off, offTarget, kArmv8InstrCond_Hi);
#else
# error "Port me!"
#endif
}


/**
 * Emits a JMP rel32/rel8 / B imm26 with a fixed displacement.
 *
 * See notes on @a offTarget in the iemNativeEmitJccToFixed() documentation.
 */
DECL_FORCE_INLINE(uint32_t) iemNativeEmitJmpToFixedEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    /* jmp rel8 or rel32 */
    int32_t offDisp = offTarget - (off + 2);
    if (offDisp < 128 && offDisp >= -128)
    {
        pCodeBuf[off++] = 0xeb;
        pCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
    }
    else
    {
        offDisp -= 3;
        pCodeBuf[off++] = 0xe9;
        pCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
        pCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
        pCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
        pCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
    }

#elif defined(RT_ARCH_ARM64)
    pCodeBuf[off++] = Armv8A64MkInstrB((int32_t)(offTarget - off));

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a JMP rel32/rel8 / B imm26 with a fixed displacement.
 *
 * See notes on @a offTarget in the iemNativeEmitJccToFixed() documentation.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJmpToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitJmpToFixedEx(iemNativeInstrBufEnsure(pReNative, off, 5), off, offTarget);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitJmpToFixedEx(iemNativeInstrBufEnsure(pReNative, off, 1), off, offTarget);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Fixes up a conditional jump to a fixed label.
 * @see  iemNativeEmitJmpToFixed, iemNativeEmitJnzToFixed,
 *       iemNativeEmitJzToFixed, ...
 */
DECL_INLINE_THROW(void) iemNativeFixupFixedJump(PIEMRECOMPILERSTATE pReNative, uint32_t offFixup, uint32_t offTarget)
{
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = pReNative->pInstrBuf;
    uint8_t const   bOpcode   = pbCodeBuf[offFixup];
    if ((uint8_t)(bOpcode - 0x70) < (uint8_t)0x10 || bOpcode == 0xeb)
    {
        pbCodeBuf[offFixup + 1] = (uint8_t)(offTarget - (offFixup + 2));
        AssertStmt(pbCodeBuf[offFixup + 1] == offTarget - (offFixup + 2),
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_EMIT_FIXED_JUMP_OUT_OF_RANGE));
    }
    else
    {
        if (bOpcode != 0x0f)
            Assert(bOpcode == 0xe9);
        else
        {
            offFixup += 1;
            Assert((uint8_t)(pbCodeBuf[offFixup] - 0x80) <= 0x10);
        }
        uint32_t const offRel32 = offTarget - (offFixup + 5);
        pbCodeBuf[offFixup + 1] = RT_BYTE1(offRel32);
        pbCodeBuf[offFixup + 2] = RT_BYTE2(offRel32);
        pbCodeBuf[offFixup + 3] = RT_BYTE3(offRel32);
        pbCodeBuf[offFixup + 4] = RT_BYTE4(offRel32);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = pReNative->pInstrBuf;
    if ((pu32CodeBuf[offFixup] & UINT32_C(0xff000000)) == UINT32_C(0x54000000))
    {
        /* B.COND + BC.COND */
        int32_t const offDisp = offTarget - offFixup;
        Assert(offDisp >= -262144 && offDisp < 262144);
        pu32CodeBuf[offFixup] = (pu32CodeBuf[offFixup] & UINT32_C(0xff00001f))
                              | (((uint32_t)offDisp    & UINT32_C(0x0007ffff)) << 5);
    }
    else
    {
        /* B imm26 */
        Assert((pu32CodeBuf[offFixup] & UINT32_C(0xfc000000)) == UINT32_C(0x14000000));
        int32_t const offDisp = offTarget - offFixup;
        Assert(offDisp >= -33554432 && offDisp < 33554432);
        pu32CodeBuf[offFixup] = (pu32CodeBuf[offFixup] & UINT32_C(0xfc000000))
                              | ((uint32_t)offDisp     & UINT32_C(0x03ffffff));
    }

#else
# error "Port me!"
#endif
}


/**
 * Internal helper, don't call directly.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestBitInGprAndJmpToLabelIfCc(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc,
                                           uint8_t iBitNo, uint32_t idxLabel, bool fJmpIfSet)
{
    Assert(iBitNo < 64);
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5);
    if (iBitNo < 8)
    {
        /* test Eb, imm8 */
        if (iGprSrc >= 4)
            pbCodeBuf[off++] = iGprSrc >= 8 ? X86_OP_REX_B : X86_OP_REX;
        pbCodeBuf[off++] = 0xf6;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
        pbCodeBuf[off++] = (uint8_t)1 << iBitNo;
        off = iemNativeEmitJccToLabel(pReNative, off, idxLabel, fJmpIfSet ? kIemNativeInstrCond_ne : kIemNativeInstrCond_e);
    }
    else
    {
        /* bt Ev, imm8 */
        if (iBitNo >= 32)
            pbCodeBuf[off++] = X86_OP_REX_W | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
        else if (iGprSrc >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
        pbCodeBuf[off++] = 0x0f;
        pbCodeBuf[off++] = 0xba;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprSrc & 7);
        pbCodeBuf[off++] = iBitNo;
        off = iemNativeEmitJccToLabel(pReNative, off, idxLabel, fJmpIfSet ? kIemNativeInstrCond_c : kIemNativeInstrCond_nc);
    }

#elif defined(RT_ARCH_ARM64)
    /* Use the TBNZ instruction here. */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm14At5);
    pu32CodeBuf[off++] = Armv8A64MkInstrTbzTbnz(fJmpIfSet, 0, iGprSrc, iBitNo);

#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a jump to @a idxLabel on the condition that bit @a iBitNo _is_ _set_ in
 * @a iGprSrc.
 *
 * @note On ARM64 the range is only +/-8191 instructions.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitTestBitInGprAndJmpToLabelIfSet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                        uint8_t iGprSrc, uint8_t iBitNo, uint32_t idxLabel)
{
    return iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, iGprSrc, iBitNo, idxLabel, true /*fJmpIfSet*/);
}


/**
 * Emits a jump to @a idxLabel on the condition that bit @a iBitNo _is_ _not_
 * _set_ in @a iGprSrc.
 *
 * @note On ARM64 the range is only +/-8191 instructions.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitTestBitInGprAndJmpToLabelIfNotSet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                           uint8_t iGprSrc, uint8_t iBitNo, uint32_t idxLabel)
{
    return iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, iGprSrc, iBitNo, idxLabel, false /*fJmpIfSet*/);
}


/**
 * Emits a test for any of the bits from @a fBits in @a iGprSrc, setting CPU
 * flags accordingly.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestAnyBitsInGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc, uint64_t fBits)
{
    Assert(fBits != 0);
#ifdef RT_ARCH_AMD64

    if (fBits >= UINT32_MAX)
    {
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBits);

        /* test Ev,Gv */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprSrc < 8 ? 0 : X86_OP_REX_R) | (iTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x85;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprSrc & 8, iTmpReg & 7);

        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }
    else if (fBits <= UINT32_MAX)
    {
        /* test Eb, imm8 or test Ev, imm32 */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        if (fBits <= UINT8_MAX)
        {
            if (iGprSrc >= 4)
                pbCodeBuf[off++] = iGprSrc >= 8 ? X86_OP_REX_B : X86_OP_REX;
            pbCodeBuf[off++] = 0xf6;
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
            pbCodeBuf[off++] = (uint8_t)fBits;
        }
        else
        {
            if (iGprSrc >= 8)
                pbCodeBuf[off++] = X86_OP_REX_B;
            pbCodeBuf[off++] = 0xf7;
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
            pbCodeBuf[off++] = RT_BYTE1(fBits);
            pbCodeBuf[off++] = RT_BYTE2(fBits);
            pbCodeBuf[off++] = RT_BYTE3(fBits);
            pbCodeBuf[off++] = RT_BYTE4(fBits);
        }
    }
    /** @todo implement me. */
    else
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_EMIT_CASE_NOT_IMPLEMENTED_1));

#elif defined(RT_ARCH_ARM64)

    if (false)
    {
        /** @todo figure out how to work the immr / N:imms constants. */
    }
    else
    {
        /* ands Zr, iGprSrc, iTmpReg */
        uint8_t const iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBits);
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAnds(ARMV8_A64_REG_XZR, iGprSrc, iTmpReg);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a test for any of the bits from @a fBits in the lower 8 bits of
 * @a iGprSrc, setting CPU flags accordingly.
 *
 * @note For ARM64 this only supports @a fBits values that can be expressed
 *       using the two 6-bit immediates of the ANDS instruction.  The caller
 *       must make sure this is possible!
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitTestAnyBitsInGpr8Ex(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGprSrc, uint8_t fBits)
{
    Assert(fBits != 0);

    /* test Eb, imm8 */
#ifdef RT_ARCH_AMD64
    if (iGprSrc >= 4)
        pCodeBuf[off++] = iGprSrc >= 8 ? X86_OP_REX_B : X86_OP_REX;
    pCodeBuf[off++] = 0xf6;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
    pCodeBuf[off++] = fBits;

#elif defined(RT_ARCH_ARM64)
    /* ands xzr, src, [tmp|#imm] */
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask32ToImmRImmS(fBits, &uImmNandS, &uImmR))
        pCodeBuf[off++] = Armv8A64MkInstrAndsImm(ARMV8_A64_REG_XZR, iGprSrc, uImmNandS, uImmR, false /*f64Bit*/);
    else
# ifdef IEM_WITH_THROW_CATCH
        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(NULL, VERR_IEM_IPE_9));
# else
        AssertReleaseFailedStmt(off = UINT32_MAX);
# endif

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits a test for any of the bits from @a fBits in the lower 8 bits of
 * @a iGprSrc, setting CPU flags accordingly.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestAnyBitsInGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc, uint8_t fBits)
{
    Assert(fBits != 0);

#ifdef RT_ARCH_AMD64
    off = iemNativeEmitTestAnyBitsInGpr8Ex(iemNativeInstrBufEnsure(pReNative, off, 4), off, iGprSrc, fBits);

#elif defined(RT_ARCH_ARM64)
    /* ands xzr, src, [tmp|#imm] */
    uint32_t uImmR     = 0;
    uint32_t uImmNandS = 0;
    if (Armv8A64ConvertMask32ToImmRImmS(fBits, &uImmNandS, &uImmR))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAndsImm(ARMV8_A64_REG_XZR, iGprSrc, uImmNandS, uImmR, false /*f64Bit*/);
    }
    else
    {
        /* Use temporary register for the 64-bit immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBits);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrAnds(ARMV8_A64_REG_XZR, iGprSrc, iTmpReg, false /*f64Bit*/);
        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }

#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a jump to @a idxLabel on the condition _any_ of the bits in @a fBits
 * are set in @a iGprSrc.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfAnySet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                   uint8_t iGprSrc, uint64_t fBits, uint32_t idxLabel)
{
    Assert(fBits); Assert(!RT_IS_POWER_OF_TWO(fBits));

    off = iemNativeEmitTestAnyBitsInGpr(pReNative, off, iGprSrc, fBits);
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabel);

    return off;
}


/**
 * Emits a jump to @a idxLabel on the condition _none_ of the bits in @a fBits
 * are set in @a iGprSrc.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfNoneSet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                    uint8_t iGprSrc, uint64_t fBits, uint32_t idxLabel)
{
    Assert(fBits); Assert(!RT_IS_POWER_OF_TWO(fBits));

    off = iemNativeEmitTestAnyBitsInGpr(pReNative, off, iGprSrc, fBits);
    off = iemNativeEmitJzToLabel(pReNative, off, idxLabel);

    return off;
}


/**
 * Emits code that jumps to @a idxLabel if @a iGprSrc is not zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabelEx(PIEMRECOMPILERSTATE pReNative, PIEMNATIVEINSTR pCodeBuf, uint32_t off,
                                                     uint8_t iGprSrc, bool f64Bit, bool fJmpIfNotZero, uint32_t idxLabel)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    /* test reg32,reg32  / test reg64,reg64 */
    if (f64Bit)
        pCodeBuf[off++] = X86_OP_REX_W | (iGprSrc < 8 ? 0 : X86_OP_REX_R | X86_OP_REX_B);
    else if (iGprSrc >= 8)
        pCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pCodeBuf[off++] = 0x85;
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprSrc & 7, iGprSrc & 7);

    /* jnz idxLabel  */
    off = iemNativeEmitJccToLabelEx(pReNative, pCodeBuf, off, idxLabel,
                                    fJmpIfNotZero ? kIemNativeInstrCond_ne : kIemNativeInstrCond_e);

#elif defined(RT_ARCH_ARM64)
    if (pReNative->paLabels[idxLabel].off != UINT32_MAX)
        pCodeBuf[off++] = Armv8A64MkInstrCbzCbnz(fJmpIfNotZero, (int32_t)(pReNative->paLabels[idxLabel].off - off),
                                                 iGprSrc, f64Bit);
    else
    {
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5);
        pCodeBuf[off++] = Armv8A64MkInstrCbzCbnz(fJmpIfNotZero, 0, iGprSrc, f64Bit);
    }

#else
# error "Port me!"
#endif
    return off;
}


/**
 * Emits code that jumps to @a idxLabel if @a iGprSrc is not zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc,
                                                   bool f64Bit, bool fJmpIfNotZero, uint32_t idxLabel)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabelEx(pReNative, iemNativeInstrBufEnsure(pReNative, off, 3 + 6),
                                                               off, iGprSrc, f64Bit, fJmpIfNotZero, idxLabel);
#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabelEx(pReNative, iemNativeInstrBufEnsure(pReNative, off, 1),
                                                               off, iGprSrc, f64Bit, fJmpIfNotZero, idxLabel);
#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code that jumps to @a idxLabel if @a iGprSrc is zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprIsZeroAndJmpToLabelEx(PIEMRECOMPILERSTATE pReNative, PIEMNATIVEINSTR pCodeBuf, uint32_t off,
                                            uint8_t iGprSrc, bool f64Bit, uint32_t idxLabel)
{
    return iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabelEx(pReNative, pCodeBuf, off, iGprSrc,
                                                                f64Bit, false /*fJmpIfNotZero*/, idxLabel);
}


/**
 * Emits code that jumps to @a idxLabel if @a iGprSrc is zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitTestIfGprIsZeroAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                      uint8_t iGprSrc, bool f64Bit, uint32_t idxLabel)
{
    return iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabel(pReNative, off, iGprSrc, f64Bit, false /*fJmpIfNotZero*/, idxLabel);
}


/**
 * Emits code that jumps to a new label if @a iGprSrc is zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprIsZeroAndJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc, bool f64Bit,
                                             IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitTestIfGprIsZeroAndJmpToLabel(pReNative, off, iGprSrc, f64Bit, idxLabel);
}


/**
 * Emits code that jumps to @a idxLabel if @a iGprSrc is not zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprIsNotZeroAndJmpToLabelEx(PIEMRECOMPILERSTATE pReNative, PIEMNATIVEINSTR pCodeBuf, uint32_t off,
                                               uint8_t iGprSrc, bool f64Bit, uint32_t idxLabel)
{
    return iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabelEx(pReNative, pCodeBuf, off, iGprSrc,
                                                                f64Bit, true /*fJmpIfNotZero*/, idxLabel);
}


/**
 * Emits code that jumps to @a idxLabel if @a iGprSrc is not zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitTestIfGprIsNotZeroAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                         uint8_t iGprSrc, bool f64Bit, uint32_t idxLabel)
{
    return iemNativeEmitTestIfGprIsZeroOrNotZeroAndJmpToLabel(pReNative, off, iGprSrc, f64Bit, true /*fJmpIfNotZero*/, idxLabel);
}


/**
 * Emits code that jumps to a new label if @a iGprSrc is not zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprIsNotZeroAndJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc, bool f64Bit,
                                               IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitTestIfGprIsNotZeroAndJmpToLabel(pReNative, off, iGprSrc, f64Bit, idxLabel);
}


/**
 * Emits code that jumps to the given label if @a iGprLeft and @a iGprRight
 * differs.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprNotEqualGprAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                               uint8_t iGprLeft, uint8_t iGprRight, uint32_t idxLabel)
{
    off = iemNativeEmitCmpGprWithGpr(pReNative, off, iGprLeft, iGprRight);
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabel);
    return off;
}


/**
 * Emits code that jumps to a new label if @a iGprLeft and @a iGprRight differs.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprNotEqualGprAndJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                  uint8_t iGprLeft, uint8_t iGprRight,
                                                  IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitTestIfGprNotEqualGprAndJmpToLabel(pReNative, off, iGprLeft, iGprRight, idxLabel);
}


/**
 * Emits code that jumps to the given label if @a iGprSrc differs from @a uImm.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprNotEqualImmAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                               uint8_t iGprSrc, uint64_t uImm, uint32_t idxLabel)
{
    off = iemNativeEmitCmpGprWithImm(pReNative, off, iGprSrc, uImm);
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabel);
    return off;
}


/**
 * Emits code that jumps to a new label if @a iGprSrc differs from @a uImm.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGprNotEqualImmAndJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                  uint8_t iGprSrc, uint64_t uImm,
                                                  IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitTestIfGprNotEqualImmAndJmpToLabel(pReNative, off, iGprSrc, uImm, idxLabel);
}


/**
 * Emits code that jumps to the given label if 32-bit @a iGprSrc differs from
 * @a uImm.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitTestIfGpr32NotEqualImmAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                             uint8_t iGprSrc, uint32_t uImm, uint32_t idxLabel)
{
    off = iemNativeEmitCmpGpr32WithImm(pReNative, off, iGprSrc, uImm);
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabel);
    return off;
}


/**
 * Emits code that jumps to a new label if 32-bit @a iGprSrc differs from
 * @a uImm.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestIfGpr32NotEqualImmAndJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                    uint8_t iGprSrc, uint32_t uImm,
                                                    IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    return iemNativeEmitTestIfGpr32NotEqualImmAndJmpToLabel(pReNative, off, iGprSrc, uImm, idxLabel);
}


/*********************************************************************************************************************************
*   Calls.                                                                                                                       *
*********************************************************************************************************************************/

/**
 * Emits a call to a 64-bit address.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitCallImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uintptr_t uPfn)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xAX, uPfn);

    /* call rax */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    pbCodeBuf[off++] = 0xff;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, uPfn);

    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrBlr(IEMNATIVE_REG_FIXED_TMP0);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code to load a stack variable into an argument GPR.
 * @throws VERR_IEM_VAR_NOT_INITIALIZED, VERR_IEM_VAR_UNEXPECTED_KIND
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitLoadArgGregFromStackVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxRegArg, uint8_t idxVar,
                                     int32_t offAddend = 0, uint32_t fHstVolatileRegsAllowed = UINT32_MAX,
                                     bool fSpilledVarsInVolatileRegs = false)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    AssertStmt(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    uint8_t const idxRegVar = pReNative->Core.aVars[idxVar].idxReg;
    if (   idxRegVar < RT_ELEMENTS(pReNative->Core.aHstRegs)
        && (   (RT_BIT_32(idxRegVar) & (~IEMNATIVE_CALL_VOLATILE_GREG_MASK | fHstVolatileRegsAllowed))
            || !fSpilledVarsInVolatileRegs ))
    {
        AssertStmt(   !(RT_BIT_32(idxRegVar) & IEMNATIVE_CALL_VOLATILE_GREG_MASK)
                   || (RT_BIT_32(idxRegVar) & fHstVolatileRegsAllowed),
                   IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_REG_IPE_13));
        if (!offAddend)
        {
            if (idxRegArg != idxRegVar)
                off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegArg, idxRegVar);
        }
        else
            off = iemNativeEmitLoadGprFromGprWithAddend(pReNative, off, idxRegArg, idxRegVar, offAddend);
    }
    else
    {
        uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
        AssertStmt(idxStackSlot != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
        off = iemNativeEmitLoadGprByBp(pReNative, off, idxRegArg, iemNativeStackCalcBpDisp(idxStackSlot));
        if (offAddend)
            off = iemNativeEmitAddGprImm(pReNative, off, idxRegArg, offAddend);
    }
    return off;
}


/**
 * Emits code to load a stack or immediate variable value into an argument GPR,
 * optional with a addend.
 * @throws VERR_IEM_VAR_NOT_INITIALIZED, VERR_IEM_VAR_UNEXPECTED_KIND
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitLoadArgGregFromImmOrStackVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxRegArg, uint8_t idxVar,
                                          int32_t offAddend = 0, uint32_t fHstVolatileRegsAllowed = 0,
                                          bool fSpilledVarsInVolatileRegs = false)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Immediate)
        off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegArg, pReNative->Core.aVars[idxVar].u.uValue + offAddend);
    else
        off = iemNativeEmitLoadArgGregFromStackVar(pReNative, off, idxRegArg, idxVar, offAddend,
                                                   fHstVolatileRegsAllowed, fSpilledVarsInVolatileRegs);
    return off;
}


/**
 * Emits code to load the variable address into an argument GRP.
 *
 * This only works for uninitialized and stack variables.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitLoadArgGregWithVarAddr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxRegArg, uint8_t idxVar,
                                    bool fFlushShadows)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    AssertStmt(   pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Invalid
               || pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    uint8_t const idxStackSlot   = iemNativeVarGetStackSlot(pReNative, idxVar);
    int32_t const offBpDisp      = iemNativeStackCalcBpDisp(idxStackSlot);

    uint8_t const idxRegVar      = pReNative->Core.aVars[idxVar].idxReg;
    if (idxRegVar < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, idxRegVar);
        iemNativeRegFreeVar(pReNative, idxRegVar, fFlushShadows);
        Assert(pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX);
    }
    Assert(   pReNative->Core.aVars[idxVar].idxStackSlot != UINT8_MAX
           && pReNative->Core.aVars[idxVar].idxReg       == UINT8_MAX);

    return iemNativeEmitLeaGprByBp(pReNative, off, idxRegArg, offBpDisp);
}


/** @} */

#endif /* !VMM_INCLUDED_SRC_include_IEMN8veRecompilerEmit_h */

