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
DECL_INLINE_THROW(uint32_t) iemNativeEmitBrk(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t uInfo)
{
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pbCodeBuf[off++] = 0xcc;
    RT_NOREF(uInfo);

#elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrBrk(uInfo & UINT32_C(0xffff));

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
DECLINLINE(uint32_t) iemNativeEmitLoadGprImmEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t iGpr, uint64_t uImm64)
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
#endif


/**
 * Emits a 64-bit GPR load of a VCpu value.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromVCpuU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov reg64, mem64 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGpr < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off,iGpr, offVCpu);
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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, gprsrc */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_W | X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_W | X86_OP_REX_R;
    else
        pbCodeBuf[off++] = X86_OP_REX_W;
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* mov dst, src;   alias for: orr dst, xzr, src */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrOrr(iGprDst, ARMV8_A64_REG_XZR, iGprSrc);

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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, gprsrc */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* mov dst32, src32;   alias for: orr dst32, wzr, src32 */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrOrr(iGprDst, ARMV8_A64_REG_WZR, iGprSrc, false /*f64bit*/);

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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprFromGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* movzx Gv,Eb */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_R;
    else if (iGprSrc >= 4)
        pbCodeBuf[off++] = X86_OP_REX;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb6;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    /* and gprdst, gprsrc, #0xff */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
# if 1
    Assert(Armv8A64ConvertImmRImmS2Mask32(0x07, 0) == UINT8_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprSrc, 0x07, 0, false /*f64Bit*/);
# else
    Assert(Armv8A64ConvertImmRImmS2Mask64(0x47, 0) == UINT8_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(iGprDst, iGprSrc, 0x47, 0);
# endif

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
        uint8_t const idxTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (uint64_t)offDisp);

        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGprReg, iGprBase, idxTmpReg);

        iemNativeRegFreeTmpImm(pReNative, idxTmpReg);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#endif


/**
 * Emits a 64-bit GPR load via a GPR base address with a displacement.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprBase, int32_t offDisp)
{
#ifdef RT_ARCH_AMD64
    /* mov reg64, mem64 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprBase < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByGprDisp(pbCodeBuf, off, iGprDst, iGprBase, offDisp);
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
 * Emits adding a 64-bit GPR to another, storing the result in the first.
 * @note The AMD64 version sets flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddTwoGprs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend)
{
#if defined(RT_ARCH_AMD64)
    /* add Gv,Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = (iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R)
                     | (iGprAddend < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x04;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprAddend & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, iGprDst, iGprDst, iGprAddend);

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
iemNativeEmitAddGprImm8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    /* add or inc */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    pbCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (iImm8 != 1)
    {
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pbCodeBuf[off++] = (uint8_t)iImm8;
    }
    else
    {
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (iImm8 >= 0)
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint8_t)iImm8);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint8_t)-iImm8);

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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGpr32Imm8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    /* add or inc */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (iImm8 != 1)
    {
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
        pbCodeBuf[off++] = (uint8_t)iImm8;
    }
    else
    {
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (iImm8 >= 0)
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint8_t)iImm8, false /*f64Bit*/);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint8_t)-iImm8, false /*f64Bit*/);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
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
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGpr32Imm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iAddend)
{
#if defined(RT_ARCH_AMD64)
    if (iAddend <= INT8_MAX && iAddend >= INT8_MIN)
        return iemNativeEmitAddGpr32Imm8(pReNative, off, iGprDst, (int8_t)iAddend);

    /* add grp, imm32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    pbCodeBuf[off++] = 0x81;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprDst & 7);
    pbCodeBuf[off++] = RT_BYTE1((uint32_t)iAddend);
    pbCodeBuf[off++] = RT_BYTE2((uint32_t)iAddend);
    pbCodeBuf[off++] = RT_BYTE3((uint32_t)iAddend);
    pbCodeBuf[off++] = RT_BYTE4((uint32_t)iAddend);

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


/*********************************************************************************************************************************
*   Unary Operations                                                                                                             *
*********************************************************************************************************************************/

/**
 * Emits code for clearing bits 16 thru 63 in the GPR.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitNegGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    /* neg Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    pbCodeBuf[off++] = 0xf7;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 3, iGprDst & 7);

#elif defined(RT_ARCH_ARM64)
    /* sub dst, xzr, dst */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrNeg(iGprDst);

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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAndGpr32ByGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x23;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);
    RT_NOREF(fSetFlags);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (!fSetFlags)
        pu32CodeBuf[off++] = Armv8A64MkInstrAnd(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrAnds(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);

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
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAndGpr32ByImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint32_t uImm, bool fSetFlags = false)
{
#if defined(RT_ARCH_AMD64)
    /* and Ev, imm */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if ((int32_t)uImm == (int8_t)uImm)
    {
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = (uint8_t)uImm;
    }
    else
    {
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = RT_BYTE1(uImm);
        pbCodeBuf[off++] = RT_BYTE2(uImm);
        pbCodeBuf[off++] = RT_BYTE3(uImm);
        pbCodeBuf[off++] = RT_BYTE4(uImm);
    }
    RT_NOREF(fSetFlags);

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


/*********************************************************************************************************************************
*   Shifting                                                                                                                     *
*********************************************************************************************************************************/

/**
 * Emits code for shifting a GPR a fixed number of bits to the left.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGprLeft(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* shl dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    pbCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (cShift != 1)
    {
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = cShift;
    }
    else
    {
        pbCodeBuf[off++] = 0xd1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrLslImm(iGprDst, iGprDst, cShift);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for shifting a 32-bit GPR a fixed number of bits to the left.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGpr32Left(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 32);

#if defined(RT_ARCH_AMD64)
    /* shl dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (cShift != 1)
    {
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = cShift;
    }
    else
    {
        pbCodeBuf[off++] = 0xd1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrLslImm(iGprDst, iGprDst, cShift, false /*64Bit*/);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for (unsigned) shifting a GPR a fixed number of bits to the right.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGprRight(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* shr dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    pbCodeBuf[off++] = iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_B;
    if (cShift != 1)
    {
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pbCodeBuf[off++] = cShift;
    }
    else
    {
        pbCodeBuf[off++] = 0xd1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrLsrImm(iGprDst, iGprDst, cShift);

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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitShiftGpr32Right(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 32);

#if defined(RT_ARCH_AMD64)
    /* shr dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (cShift != 1)
    {
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
        pbCodeBuf[off++] = cShift;
    }
    else
    {
        pbCodeBuf[off++] = 0xd1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, iGprDst & 7);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrLsrImm(iGprDst, iGprDst, cShift, false /*64Bit*/);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpGprWithGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    /* cmp Gv, Ev */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprLeft >= 8 ? X86_OP_REX_R : 0) | (iGprRight >= 8 ? X86_OP_REX_B : 0);
    pbCodeBuf[off++] = 0x3b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprLeft & 7, iGprRight & 7);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitCmpArm64(pReNative, off, iGprLeft, iGprRight, false /*f64Bit*/);

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
    /* cmp Gv, Ev */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (iGprLeft >= 8 || iGprRight >= 8)
        pbCodeBuf[off++] = (iGprLeft >= 8 ? X86_OP_REX_R : 0) | (iGprRight >= 8 ? X86_OP_REX_B : 0);
    pbCodeBuf[off++] = 0x3b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprLeft & 7, iGprRight & 7);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitCmpArm64(pReNative, off, iGprLeft, iGprRight, false /*f64Bit*/);

#else
# error "Port me!"
#endif
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
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_XZR, iGprLeft, (uint32_t)uImm,
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
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCmpGpr32WithImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint32_t uImm)
{
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprLeft >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (uImm <= UINT32_C(0xff))
    {
        /* cmp Ev, Ib */
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, iGprLeft & 7);
        pbCodeBuf[off++] = (uint8_t)uImm;
    }
    else
    {
        /* cmp Ev, imm */
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, iGprLeft & 7);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        pbCodeBuf[off++] = RT_BYTE1(uImm);
        pbCodeBuf[off++] = RT_BYTE2(uImm);
        pbCodeBuf[off++] = RT_BYTE3(uImm);
        pbCodeBuf[off++] = RT_BYTE4(uImm);
    }

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
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    if (pReNative->paLabels[idxLabel].off != UINT32_MAX)
    {
        uint32_t offRel = pReNative->paLabels[idxLabel].off - (off + 2);
        if ((int32_t)offRel < 128 && (int32_t)offRel >= -128)
        {
            pbCodeBuf[off++] = 0xeb;                /* jmp rel8 */
            pbCodeBuf[off++] = (uint8_t)offRel;
        }
        else
        {
            offRel -= 3;
            pbCodeBuf[off++] = 0xe9;                /* jmp rel32 */
            pbCodeBuf[off++] = RT_BYTE1(offRel);
            pbCodeBuf[off++] = RT_BYTE2(offRel);
            pbCodeBuf[off++] = RT_BYTE3(offRel);
            pbCodeBuf[off++] = RT_BYTE4(offRel);
        }
    }
    else
    {
        pbCodeBuf[off++] = 0xe9;                    /* jmp rel32 */
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4);
        pbCodeBuf[off++] = 0xfe;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;
    }
    pbCodeBuf[off++] = 0xcc;                        /* int3 poison */

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (pReNative->paLabels[idxLabel].off != UINT32_MAX)
        pu32CodeBuf[off++] = Armv8A64MkInstrB(pReNative->paLabels[idxLabel].off - off);
    else
    {
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm26At0);
        pu32CodeBuf[off++] = Armv8A64MkInstrB(-1);
    }

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
#else
# error "Port me!"
#endif


/**
 * Emits a Jcc rel32 / B.cc imm19 to the given label (ASSUMED requiring fixup).
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJccToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel, IEMNATIVEINSTRCOND enmCond)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    /* jcc rel32 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = (uint8_t)enmCond | 0x80;
    iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4);
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5);
    pu32CodeBuf[off++] = Armv8A64MkInstrBCond(enmCond, -1);

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
 * The @a offTarget is applied x86-style, so zero means the next instruction.
 * The unit is IEMNATIVEINSTR.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitJccToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget, IEMNATIVEINSTRCOND enmCond)
{
#ifdef RT_ARCH_AMD64
    /* jcc rel32 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    if (offTarget < 128 && offTarget >= -128)
    {
        pbCodeBuf[off++] = (uint8_t)enmCond | 0x70;
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)offTarget);
    }
    else
    {
        pbCodeBuf[off++] = 0x0f;
        pbCodeBuf[off++] = (uint8_t)enmCond | 0x80;
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)offTarget);
        pbCodeBuf[off++] = RT_BYTE2((uint32_t)offTarget);
        pbCodeBuf[off++] = RT_BYTE3((uint32_t)offTarget);
        pbCodeBuf[off++] = RT_BYTE4((uint32_t)offTarget);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrBCond(enmCond, offTarget + 1);

#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a JZ/JE rel32 / B.EQ imm19 with a fixed displacement.
 *
 * The @a offTarget is applied x86-style, so zero means the next instruction.
 * The unit is IEMNATIVEINSTR.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJzToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
 * The @a offTarget is applied x86-style, so zero means the next instruction.
 * The unit is IEMNATIVEINSTR.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJnzToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
 * The @a offTarget is applied x86-style, so zero means the next instruction.
 * The unit is IEMNATIVEINSTR.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJbeToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
 * The @a offTarget is applied x86-style, so zero means the next instruction.
 * The unit is IEMNATIVEINSTR.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitJaToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
 * Fixes up a conditional jump to a fixed label.
 * @see  iemNativeEmitJnzToFixed, iemNativeEmitJzToFixed, ...
 */
DECLINLINE(void) iemNativeFixupFixedJump(PIEMRECOMPILERSTATE pReNative, uint32_t offFixup, uint32_t offTarget)
{
# if defined(RT_ARCH_AMD64)
    uint8_t * const pbCodeBuf = pReNative->pInstrBuf;
    if (pbCodeBuf[offFixup] != 0x0f)
    {
        Assert((uint8_t)(pbCodeBuf[offFixup] - 0x70) <= 0x10);
        pbCodeBuf[offFixup + 1] = (uint8_t)(offTarget - (offFixup + 2));
        Assert(pbCodeBuf[offFixup + 1] == offTarget - (offFixup + 2));
    }
    else
    {
        Assert((uint8_t)(pbCodeBuf[offFixup + 1] - 0x80) <= 0x10);
        uint32_t const offRel32 = offTarget - (offFixup + 6);
        pbCodeBuf[offFixup + 2] = RT_BYTE1(offRel32);
        pbCodeBuf[offFixup + 3] = RT_BYTE2(offRel32);
        pbCodeBuf[offFixup + 4] = RT_BYTE3(offRel32);
        pbCodeBuf[offFixup + 5] = RT_BYTE4(offRel32);
    }

# elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = pReNative->pInstrBuf;

    int32_t const offDisp = offTarget - offFixup;
    Assert(offDisp >= -262144 && offDisp < 262144);
    Assert((pu32CodeBuf[offFixup] & UINT32_C(0xff000000)) == UINT32_C(0x54000000)); /* B.COND + BC.COND */

    pu32CodeBuf[offFixup] = (pu32CodeBuf[offFixup] & UINT32_C(0xff00001f))
                          | (((uint32_t)offDisp    & UINT32_C(0x0007ffff)) << 5);

# endif
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
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitTestAnyBitsInGpr8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc, uint8_t fBits)
{
    Assert(fBits != 0);

#ifdef RT_ARCH_AMD64
    /* test Eb, imm8 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (iGprSrc >= 4)
        pbCodeBuf[off++] = iGprSrc >= 8 ? X86_OP_REX_B : X86_OP_REX;
    pbCodeBuf[off++] = 0xf6;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
    pbCodeBuf[off++] = fBits;

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
 * Emits code that jumps to @a idxLabel if @a iGprSrc is zero.
 *
 * The operand size is given by @a f64Bit.
 */
DECL_INLINE_THROW(uint32_t) iemNativeEmitTestIfGprIsZeroAndJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                      uint8_t iGprSrc, bool f64Bit, uint32_t idxLabel)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    /* test reg32,reg32  / test reg64,reg64 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (f64Bit)
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprSrc < 8 ? 0 : X86_OP_REX_R | X86_OP_REX_B);
    else if (iGprSrc >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pbCodeBuf[off++] = 0x85;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprSrc & 7, iGprSrc & 7);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    /* jz idxLabel  */
    off = iemNativeEmitJzToLabel(pReNative, off, idxLabel);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5);
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 0, iGprSrc, f64Bit);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me!"
#endif
    return off;
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
                                     int32_t offAddend = 0, bool fVarAllowInVolatileReg = false)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    AssertStmt(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    uint8_t const idxRegVar = pReNative->Core.aVars[idxVar].idxReg;
    if (idxRegVar < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        Assert(!(RT_BIT_32(idxRegVar) & IEMNATIVE_CALL_VOLATILE_GREG_MASK) || fVarAllowInVolatileReg);
        RT_NOREF(fVarAllowInVolatileReg);
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
                                          int32_t offAddend = 0, bool fVarAllowInVolatileReg = false)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Immediate)
        off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegArg, pReNative->Core.aVars[idxVar].u.uValue + offAddend);
    else
        off = iemNativeEmitLoadArgGregFromStackVar(pReNative, off, idxRegArg, idxVar, offAddend, fVarAllowInVolatileReg);
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

