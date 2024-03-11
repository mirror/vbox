/* $Id$ */
/** @file
 * IEM - Native Recompiler, x86 Target - Code Emitters.
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_VMMAll_target_x86_IEMAllN8veEmit_x86_h
#define VMM_INCLUDED_SRC_VMMAll_target_x86_IEMAllN8veEmit_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#ifdef RT_ARCH_AMD64

/**
 * Emits an ModR/M instruction with one opcode byte and only register operands.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAmd64OneByteModRmInstrRREx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t bOpcode8, uint8_t bOpcodeOther,
                                        uint8_t cOpBits, uint8_t idxRegReg, uint8_t idxRegRm)
{
    Assert(idxRegReg < 16); Assert(idxRegRm < 16);
    switch (cOpBits)
    {
        case 16:
            pCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
            RT_FALL_THRU();
        case 32:
            if (idxRegReg >= 8 || idxRegRm >= 8)
                pCodeBuf[off++] = (idxRegReg >= 8 ? X86_OP_REX_R : 0) | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
            pCodeBuf[off++] = bOpcodeOther;
            break;

        default: AssertFailed(); RT_FALL_THRU();
        case 64:
            pCodeBuf[off++] = X86_OP_REX_W | (idxRegReg >= 8 ? X86_OP_REX_R : 0) | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
            pCodeBuf[off++] = bOpcodeOther;
            break;

        case 8:
            if (idxRegReg >= 8 || idxRegRm >= 8)
                pCodeBuf[off++] = (idxRegReg >= 8 ? X86_OP_REX_R : 0) | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
            else if (idxRegReg >= 4 || idxRegRm >= 4)
                pCodeBuf[off++] = X86_OP_REX;
            pCodeBuf[off++] = bOpcode8;
            break;
    }
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegReg & 7, idxRegRm & 7);
    return off;
}


/**
 * Emits an ModR/M instruction with two opcode bytes and only register operands.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAmd64TwoByteModRmInstrRREx(PIEMNATIVEINSTR pCodeBuf, uint32_t off,
                                        uint8_t bOpcode0, uint8_t bOpcode8, uint8_t bOpcodeOther,
                                        uint8_t cOpBits, uint8_t idxRegReg, uint8_t idxRegRm)
{
    Assert(idxRegReg < 16); Assert(idxRegRm < 16);
    switch (cOpBits)
    {
        case 16:
            pCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
            RT_FALL_THRU();
        case 32:
            if (idxRegReg >= 8 || idxRegRm >= 8)
                pCodeBuf[off++] = (idxRegReg >= 8 ? X86_OP_REX_R : 0) | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
            pCodeBuf[off++] = bOpcode0;
            pCodeBuf[off++] = bOpcodeOther;
            break;

        default: AssertFailed(); RT_FALL_THRU();
        case 64:
            pCodeBuf[off++] = X86_OP_REX_W | (idxRegReg >= 8 ? X86_OP_REX_R : 0) | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
            pCodeBuf[off++] = bOpcode0;
            pCodeBuf[off++] = bOpcodeOther;
            break;

        case 8:
            if (idxRegReg >= 8 || idxRegRm >= 8)
                pCodeBuf[off++] = (idxRegReg >= 8 ? X86_OP_REX_R : 0) | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
            else if (idxRegReg >= 4 || idxRegRm >= 4)
                pCodeBuf[off++] = X86_OP_REX;
            pCodeBuf[off++] = bOpcode0;
            pCodeBuf[off++] = bOpcode8;
            break;
    }
    pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegReg & 7, idxRegRm & 7);
    return off;
}


/**
 * Emits one of three opcodes with an immediate.
 *
 * These are expected to be a /idxRegReg form.
 */
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitAmd64OneByteModRmInstrRIEx(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t bOpcode8, uint8_t bOpcodeOtherImm8,
                                        uint8_t bOpcodeOther, uint8_t cOpBits, uint8_t cImmBits, uint8_t idxRegReg,
                                        uint8_t idxRegRm, uint64_t uImmOp)
{
    Assert(idxRegReg < 8); Assert(idxRegRm < 16);
    if (cImmBits == 8 || uImmOp <= (uint64_t)0x7f)
    {
        switch (cOpBits)
        {
            case 16:
                pCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
                RT_FALL_THRU();
            case 32:
                if (idxRegRm >= 8)
                    pCodeBuf[off++] = X86_OP_REX_B;
                pCodeBuf[off++] = bOpcodeOtherImm8;
                break;

            default: AssertFailed(); RT_FALL_THRU();
            case 64:
                pCodeBuf[off++] = X86_OP_REX_W | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
                pCodeBuf[off++] = bOpcodeOtherImm8;
                break;

            case 8:
                if (idxRegRm >= 8)
                    pCodeBuf[off++] = X86_OP_REX_B;
                else if (idxRegRm >= 4)
                    pCodeBuf[off++] = X86_OP_REX;
                pCodeBuf[off++] = bOpcode8;
                break;
        }
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegReg, idxRegRm & 7);
        pCodeBuf[off++] = (uint8_t)uImmOp;
    }
    else
    {
        switch (cOpBits)
        {
            case 32:
                if (idxRegRm >= 8)
                    pCodeBuf[off++] = X86_OP_REX_B;
                break;

            default: AssertFailed(); RT_FALL_THRU();
            case 64:
                pCodeBuf[off++] = X86_OP_REX_W | (idxRegRm >= 8 ? X86_OP_REX_B : 0);
                break;

            case 16:
                pCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
                if (idxRegRm >= 8)
                    pCodeBuf[off++] = X86_OP_REX_B;
                pCodeBuf[off++] = bOpcodeOther;
                pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegReg, idxRegRm & 7);
                pCodeBuf[off++] = RT_BYTE1(uImmOp);
                pCodeBuf[off++] = RT_BYTE2(uImmOp);
                Assert(cImmBits == 16);
                return off;
        }
        pCodeBuf[off++] = bOpcodeOther;
        pCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegReg, idxRegRm & 7);
        pCodeBuf[off++] = RT_BYTE1(uImmOp);
        pCodeBuf[off++] = RT_BYTE2(uImmOp);
        pCodeBuf[off++] = RT_BYTE3(uImmOp);
        pCodeBuf[off++] = RT_BYTE4(uImmOp);
        Assert(cImmBits == 32);
    }
    return off;
}

#endif /* RT_ARCH_AMD64 */

/**
 * This is an implementation of IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGICAL.
 *
 * It takes liveness stuff into account.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitEFlagsForLogical(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarEfl
#ifndef RT_ARCH_AMD64
                              , uint8_t cOpBits, uint8_t idxRegResult, bool fNativeFlags = false
#endif
                              )
{
#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
    if (1) /** @todo check if all bits are clobbered. */
#endif
    {
#ifdef RT_ARCH_AMD64
        /*
         * Collect flags and merge them with eflags.
         */
        /** @todo we could alternatively use SAHF here when host rax is free since,
         *        OF is cleared. */
        PIEMNATIVEINSTR pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        /* pushf - do this before any reg allocations as they may emit instructions too. */
        pCodeBuf[off++] = 0x9c;

        uint8_t const idxRegEfl = iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);
        uint8_t const idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);
        pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2 + 7 + 7 + 3);
        /* pop   tmp */
        if (idxTmpReg >= 8)
            pCodeBuf[off++] = X86_OP_REX_B;
        pCodeBuf[off++] = 0x58 + (idxTmpReg & 7);
        /* and  tmp, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF */
        off = iemNativeEmitAndGpr32ByImmEx(pCodeBuf, off, idxTmpReg, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF);
        /* Clear the status bits in EFLs. */
        off = iemNativeEmitAndGpr32ByImmEx(pCodeBuf, off, idxRegEfl, ~X86_EFL_STATUS_BITS);
        /* OR in the flags we collected. */
        off = iemNativeEmitOrGpr32ByGprEx(pCodeBuf, off, idxRegEfl, idxTmpReg);
        iemNativeVarRegisterRelease(pReNative, idxVarEfl);
        iemNativeRegFreeTmp(pReNative, idxTmpReg);

#elif defined(RT_ARCH_ARM64)
        /*
         * Calculate flags.
         */
        uint8_t const         idxRegEfl = iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);
        uint8_t const         idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);
        PIEMNATIVEINSTR const pCodeBuf  = iemNativeInstrBufEnsure(pReNative, off, 15);

        /* Clear the status bits. ~0x8D5 (or ~0x8FD) can't be AND immediate, so use idxTmpReg for constant. */
        off = iemNativeEmitLoadGpr32ImmEx(pCodeBuf, off, idxTmpReg, ~X86_EFL_STATUS_BITS);
        off = iemNativeEmitAndGpr32ByGpr32Ex(pCodeBuf, off, idxRegEfl, idxTmpReg);

        /* N,Z -> SF,ZF */
        if (cOpBits < 32)
            pCodeBuf[off++] = Armv8A64MkInstrSetF8SetF16(idxRegResult, cOpBits > 8); /* sets NZ */
        else if (!fNativeFlags)
            pCodeBuf[off++] = Armv8A64MkInstrAnds(ARMV8_A64_REG_XZR, idxRegResult, idxRegResult, cOpBits > 32 /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrMrs(idxTmpReg, ARMV8_AARCH64_SYSREG_NZCV); /* Bits: 31=N; 30=Z; 29=C; 28=V; */
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg, 30);
        pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_ZF_BIT, 2, false /*f64Bit*/);
        AssertCompile(X86_EFL_ZF_BIT + 1 == X86_EFL_SF_BIT);

        /* Calculate 8-bit parity of the result. */
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxRegResult, idxRegResult, false /*f64Bit*/,
                                             4 /*offShift6*/, kArmv8A64InstrShift_Lsr);
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxTmpReg,    idxTmpReg,    false /*f64Bit*/,
                                             2 /*offShift6*/, kArmv8A64InstrShift_Lsr);
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxTmpReg,    idxTmpReg,    false /*f64Bit*/,
                                             1 /*offShift6*/, kArmv8A64InstrShift_Lsr);
        Assert(Armv8A64ConvertImmRImmS2Mask32(0, 0) == 1);
        pCodeBuf[off++] = Armv8A64MkInstrEorImm(idxTmpReg, idxTmpReg, 0, 0, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_PF_BIT, 1,  false /*f64Bit*/);

        iemNativeVarRegisterRelease(pReNative, idxVarEfl);
        iemNativeRegFreeTmp(pReNative, idxTmpReg);
#else
# error "port me"
#endif
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
    return off;
}


/**
 * This is an implementation of IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC.
 *
 * It takes liveness stuff into account.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitEFlagsForArithmetic(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarEfl, uint8_t idxRegEflIn
#ifndef RT_ARCH_AMD64
                                 , uint8_t cOpBits, uint8_t idxRegResult, uint8_t idxRegDstIn, uint8_t idxRegSrc
                                 , bool fInvertCarry, uint64_t uImmSrc
#endif
                                 )
{
#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
    if (1) /** @todo check if all bits are clobbered. */
#endif
    {
#ifdef RT_ARCH_AMD64
        /*
         * Collect flags and merge them with eflags.
         */
        PIEMNATIVEINSTR pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        /* pushf - do this before any reg allocations as they may emit instructions too. */
        pCodeBuf[off++] = 0x9c;

        uint8_t const idxRegEfl = idxRegEflIn != UINT8_MAX ? idxRegEflIn
                                : iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);
        uint8_t const idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);
        pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2 + 7 + 7 + 3);
        /* pop   tmp */
        if (idxTmpReg >= 8)
            pCodeBuf[off++] = X86_OP_REX_B;
        pCodeBuf[off++] = 0x58 + (idxTmpReg & 7);
        /* Isolate the flags we want. */
        off = iemNativeEmitAndGpr32ByImmEx(pCodeBuf, off, idxTmpReg, X86_EFL_STATUS_BITS);
        /* Clear the status bits in EFLs. */
        off = iemNativeEmitAndGpr32ByImmEx(pCodeBuf, off, idxRegEfl, ~X86_EFL_STATUS_BITS);
        /* OR in the flags we collected. */
        off = iemNativeEmitOrGpr32ByGprEx(pCodeBuf, off, idxRegEfl, idxTmpReg);
        if (idxRegEflIn != idxRegEfl)
            iemNativeVarRegisterRelease(pReNative, idxVarEfl);
        iemNativeRegFreeTmp(pReNative, idxTmpReg);

#elif defined(RT_ARCH_ARM64)
        /*
         * Calculate flags.
         */
        uint8_t const         idxRegEfl  = idxRegEflIn != UINT8_MAX ? idxRegEflIn
                                         : iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);
        uint8_t const         idxTmpReg  = iemNativeRegAllocTmp(pReNative, &off);
        uint8_t const         idxTmpReg2 = cOpBits >= 32 ? UINT8_MAX : iemNativeRegAllocTmp(pReNative, &off);
        PIEMNATIVEINSTR const pCodeBuf   = iemNativeInstrBufEnsure(pReNative, off, 20);

        /* Invert CF (stored inved on ARM) and load the flags into the temporary register. */
        if (fInvertCarry)
            pCodeBuf[off++] = ARMV8_A64_INSTR_CFINV;
        pCodeBuf[off++] = Armv8A64MkInstrMrs(idxTmpReg, ARMV8_AARCH64_SYSREG_NZCV); /* Bits: 31=N; 30=Z; 29=C; 28=V; */

        if (cOpBits >= 32)
        {
            /* V -> OF */
            pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg, 28);
            pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_OF_BIT, 1, false /*f64Bit*/);

            /* C -> CF */
            pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg, 1);
            pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_CF_BIT, 1, false /*f64Bit*/);
        }

        /* N,Z -> SF,ZF */
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg, cOpBits >= 32 ? 1 : 30);
        pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_ZF_BIT, 2, false /*f64Bit*/);

        /* For ADC and SBB we have to calculate overflow and carry our selves. */
        if (cOpBits < 32)
        {
            /* Since the carry flag is the zero'th flag, we just use BFXIL got copy it over. */
            AssertCompile(X86_EFL_CF_BIT == 0);
            pCodeBuf[off++] = Armv8A64MkInstrBfxil(idxRegEfl, idxRegResult, cOpBits, 1, false /*f64Bit*/);

            /* The overflow flag is more work as we have to compare the signed bits for
               both inputs and the result. See IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC.

               Formula: ~(a_uDst ^ a_uSrcOf) & (a_uResult ^ a_uDst)
               With a_uSrcOf as a_uSrc for additions and ~a_uSrc for subtractions.

               It is a bit simpler when the right (source) side is constant:
               adc: S D R -> OF                     sbb: S D R -> OF
                    0 0 0 -> 0  \                        0 0 0 -> 0  \
                    0 0 1 -> 1   \                       0 0 1 -> 0   \
                    0 1 0 -> 0   / and not(D), R         0 1 0 -> 1   / and D, not(R)
                    0 1 1 -> 0  /                        0 1 1 -> 0  /
                    1 0 0 -> 0  \                        1 0 0 -> 0  \
                    1 0 1 -> 0   \ and D, not(R)         1 0 1 -> 1   \ and not(D), R
                    1 1 0 -> 1   /                       1 1 0 -> 0   /
                    1 1 1 -> 0  /                        1 1 1 -> 0  / */
            if (idxRegSrc != UINT8_MAX)
            {
                if (fInvertCarry) /* sbb:  ~((a_uDst) ^ ~(a_uSrcOf)) ->  (a_uDst) ^  (a_uSrcOf); HACK ALERT: fInvertCarry == sbb */
                    pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg,  idxRegDstIn, idxRegSrc,  false);
                else              /* adc:  ~((a_uDst) ^ (a_uSrcOf))  ->  (a_uDst) ^ ~(a_uSrcOf) */
                    pCodeBuf[off++] = Armv8A64MkInstrEon(idxTmpReg,  idxRegDstIn, idxRegSrc,  false);
                pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg2,   idxRegDstIn, idxRegResult, false); /* (a_uDst) ^ (a_uResult) */
                pCodeBuf[off++] = Armv8A64MkInstrAnd(idxTmpReg,    idxTmpReg,   idxTmpReg2,   false /*f64Bit*/);
            }
            else if (uImmSrc & RT_BIT_32(cOpBits - 1))
            {
                if (fInvertCarry) /* HACK ALERT: fInvertCarry == sbb */
                    pCodeBuf[off++] = Armv8A64MkInstrBic(idxTmpReg, idxRegResult, idxRegDstIn, false);
                else
                    pCodeBuf[off++] = Armv8A64MkInstrBic(idxTmpReg, idxRegDstIn, idxRegResult, false);
            }
            else
            {
                if (fInvertCarry) /* HACK ALERT: fInvertCarry == sbb */
                    pCodeBuf[off++] = Armv8A64MkInstrBic(idxTmpReg, idxRegDstIn, idxRegResult, false);
                else
                    pCodeBuf[off++] = Armv8A64MkInstrBic(idxTmpReg, idxRegResult, idxRegDstIn, false);
            }
            pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg,   cOpBits - 1,  false /*f64Bit*/);
            pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl,    idxTmpReg,   X86_EFL_OF_BIT, 1);
            iemNativeRegFreeTmp(pReNative, idxTmpReg2);
        }

        /* Calculate 8-bit parity of the result. */
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxRegResult, idxRegResult, false /*f64Bit*/,
                                             4 /*offShift6*/, kArmv8A64InstrShift_Lsr);
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxTmpReg,    idxTmpReg,    false /*f64Bit*/,
                                             2 /*offShift6*/, kArmv8A64InstrShift_Lsr);
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxTmpReg,    idxTmpReg,    false /*f64Bit*/,
                                             1 /*offShift6*/, kArmv8A64InstrShift_Lsr);
        Assert(Armv8A64ConvertImmRImmS2Mask32(0, 0) == 1);
        pCodeBuf[off++] = Armv8A64MkInstrEorImm(idxTmpReg, idxTmpReg, 0, 0, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_PF_BIT, 1,  false /*f64Bit*/);

        /* Calculate auxilary carry/borrow.  This is related to 8-bit BCD.
           General formula: ((uint32_t)(a_uResult)  ^ (uint32_t)(a_uSrc) ^ (uint32_t)(a_uDst)) & X86_EFL_AF;
               S D R
               0 0 0 -> 0;  \
               0 0 1 -> 1;   \  regular
               0 1 0 -> 1;   /    xor R, D
               0 1 1 -> 0;  /
               1 0 0 -> 1;  \
               1 0 1 -> 0;   \  invert one of the two
               1 1 0 -> 0;   /    xor not(R), D
               1 1 1 -> 1;  /
           a_uSrc[bit 4]=0: ((uint32_t)(a_uResult)  ^ (uint32_t)(a_uDst)) & X86_EFL_AF;
           a_uSrc[bit 4]=1: ((uint32_t)~(a_uResult) ^ (uint32_t)(a_uDst)) & X86_EFL_AF;
           */

        if (idxRegSrc != UINT8_MAX)
        {
            pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxRegDstIn, idxRegSrc, false /*f64Bit*/);
            pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxTmpReg, idxRegResult, false /*f64Bit*/);
        }
        else if (uImmSrc & X86_EFL_AF)
            pCodeBuf[off++] = Armv8A64MkInstrEon(idxTmpReg, idxRegDstIn, idxRegResult, false /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxRegDstIn, idxRegResult, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg, X86_EFL_AF_BIT, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_AF_BIT, 1,  false /*f64Bit*/);

        if (idxRegEflIn != idxRegEfl)
            iemNativeVarRegisterRelease(pReNative, idxVarEfl);
        iemNativeRegFreeTmp(pReNative, idxTmpReg);
#else
# error "port me"
#endif
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
    return off;

}


/**
 * The AND instruction will clear OF, CF and AF (latter is undefined) and
 * set the other flags according to the result.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_and_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);
#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly size AND instruction harvest the EFLAGS. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x22, 0x23, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit AND for the 8-bit and 16-bit bit ones. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrAnds(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst, true /*fNativeFlags*/);
#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    return off;
}


/**
 * The AND instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_and_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly size AND instruction harvest the EFLAGS. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    off = iemNativeEmitAmd64OneByteModRmInstrRIEx(pCodeBuf, off, 0x80, 0x83, 0x81, cOpBits, cImmBits, 4, idxRegDst, uImmOp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit AND for the 8-bit and 16-bit bit ones. */
    uint32_t uImmSizeLen, uImmRotations;
    if (  cOpBits > 32
        ? Armv8A64ConvertMask64ToImmRImmS(uImmOp, &uImmSizeLen, &uImmRotations)
        : Armv8A64ConvertMask32ToImmRImmS(uImmOp, &uImmSizeLen, &uImmRotations))
    {
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (cOpBits >= 32)
            pCodeBuf[off++] = Armv8A64MkInstrAndsImm(idxRegDst, idxRegDst, uImmSizeLen, uImmRotations, cOpBits > 32 /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAndImm(idxRegDst, idxRegDst, uImmSizeLen, uImmRotations, cOpBits > 32 /*f64Bit*/);
    }
    else
    {
        uint8_t const idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (cOpBits >= 32)
            pCodeBuf[off++] = Armv8A64MkInstrAnds(idxRegDst, idxRegDst, idxRegTmpImm, cOpBits > 32 /*f64Bit*/);
        else
            pCodeBuf[off++] = Armv8A64MkInstrAnd(idxRegDst, idxRegDst, idxRegTmpImm, cOpBits > 32 /*f64Bit*/);
        iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst, cOpBits >= 32 /*fNativeFlags*/);
    RT_NOREF_PV(cImmBits)

#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    return off;
}


/**
 * The TEST instruction will clear OF, CF and AF (latter is undefined) and
 * set the other flags according to the result.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_test_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                           uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const         idxRegDst    = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const         idxRegSrc    = idxVarSrc == idxVarDst ? idxRegDst /* special case of 'test samereg,samereg' */
                                       : iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);
#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly size TEST instruction harvest the EFLAGS. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x84, 0x85, cOpBits, idxRegSrc, idxRegDst);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit AND for the 8-bit and 16-bit bit ones.  We also
       need to keep the result in order to calculate the flags. */
    uint8_t const         idxRegResult = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf     = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (cOpBits >= 32)
        pCodeBuf[off++] = Armv8A64MkInstrAnds(idxRegResult, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    else
        pCodeBuf[off++] = Armv8A64MkInstrAnd(idxRegResult, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif
    if (idxVarSrc != idxVarDst)
        iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#ifdef RT_ARCH_AMD64
    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl);
#else
    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegResult, cOpBits >= 32 /*fNativeFlags*/);
    iemNativeRegFreeTmp(pReNative, idxRegResult);
#endif
    return off;
}


/**
 * The TEST instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_test_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    RT_NOREF(pReNative, off, idxVarDst, uImmOp, idxVarEfl, cOpBits, cImmBits);
    return off;
}


/**
 * The OR instruction will clear OF, CF and AF (latter is undefined) and
 * set the other flags according to the result.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_or_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                         uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);
#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly size OR instruction harvest the EFLAGS. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x0a, 0x0b, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit OR for the 8-bit and 16-bit bit ones. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst);

#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    return off;
}


/**
 * The OR instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_or_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    RT_NOREF(pReNative, off, idxVarDst, uImmOp, idxVarEfl, cOpBits, cImmBits);
    return off;
}


/**
 * The XOR instruction will clear OF, CF and AF (latter is undefined) and
 * set the other flags according to the result.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_xor_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);
#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly size OR instruction harvest the EFLAGS. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x32, 0x33, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit OR for the 8-bit and 16-bit bit ones. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrEor(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst);

#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    return off;
}


/**
 * The XOR instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_xor_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    RT_NOREF(pReNative, off, idxVarDst, uImmOp, idxVarEfl, cOpBits, cImmBits);
    return off;
}


/**
 * The ADD instruction will set all status flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_add_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly sized ADD instruction to get the right EFLAGS.SF value. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x02, 0x03, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we'll need the two input operands as well as the result in order
       to calculate the right flags, even if we use ADDS and translates NZCV into
       OF, CF, ZF and SF. */
    uint8_t const         idxRegDstIn = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (cOpBits >= 32)
    {
        off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
        pCodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
    }
    else
    {
        /* Shift the operands up so we can perform a 32-bit operation and get all four flags. */
        uint32_t const cShift = 32 - cOpBits;
        pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegDstIn, ARMV8_A64_REG_XZR, idxRegDst, false /*f64Bit*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegDst, idxRegDstIn, idxRegSrc, false /*f64Bit*/,
                                                true /*fSetFlags*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDstIn, idxRegDstIn, cShift, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDst, idxRegDst, cShift, false /*f64Bit*/);
        cOpBits = 32;
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegDst,
                                           idxRegDstIn, idxRegSrc, false /*fInvertCarry*/, 0);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#else
# error "port me"
#endif
    return off;
}


/**
 * The ADD instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_add_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly sized ADD instruction to get the right EFLAGS.SF value. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    off = iemNativeEmitAmd64OneByteModRmInstrRIEx(pCodeBuf, off, 0x80, 0x83, 0x81, cOpBits, cImmBits, 0, idxRegDst, uImmOp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we'll need the two input operands as well as the result in order
       to calculate the right flags, even if we use ADDS and translates NZCV into
       OF, CF, ZF and SF. */
    uint8_t const   idxRegDstIn  = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR pCodeBuf     = iemNativeInstrBufEnsure(pReNative, off, 8);
    off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
    if (cOpBits >= 32)
    {
        if (uImmOp <= 0xfffU)
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegDst, idxRegDst, uImmOp, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
        else if (uImmOp <= 0xfff000U && !(uImmOp & 0xfff))
            pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegDst, idxRegDst, uImmOp >> 12, cOpBits > 32 /*f64Bit*/,
                                                       true /*fSetFlags*/, true /*fShift12*/);
        else
        {
            uint8_t const idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
            pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pCodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegDst, idxRegDst, idxRegTmpImm, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
            iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
        }
    }
    else
    {
        /* Shift the operands up so we can perform a 32-bit operation and get all four flags. */
        uint32_t const cShift = 32 - cOpBits;
        uint8_t const idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp << cShift);
        pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        pCodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegDst, idxRegTmpImm, idxRegDstIn, false /*f64Bit*/, true /*fSetFlags*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDst, idxRegDst, cShift, false /*f64Bit*/);
        cOpBits = 32;
        iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegDst,
                                           idxRegDstIn, UINT8_MAX, false /*fInvertCarry*/, uImmOp);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    RT_NOREF(cImmBits);

#else
# error "port me"
#endif
    return off;
}


/**
 * The ADC instruction takes CF as input and will set all status flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_adc_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);
    uint8_t const idxRegEfl = iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we use BT to set EFLAGS.CF and then issue an ADC instruction
       with matching size to get the correct flags. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 9);

    /* Use the BT instruction to set CF according to idxRegEfl. */
    off = iemNativeEmitAmd64TwoByteModRmInstrRREx(pCodeBuf, off, 0x0f, 0x0b, 0xba, 32 /*cOpBits*/, 4, idxRegEfl);
    pCodeBuf[off++] = X86_EFL_CF_BIT;

    off = iemNativeEmitAmd64OneByteModRmInstrRREx(pCodeBuf, off, 0x12, 0x13, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use the RMIF instruction to load PSTATE.CF from idxRegEfl and
       then ADCS for the calculation.  We need all inputs and result for the two
       flags (AF,PF) that can't be directly derived from PSTATE.NZCV. */
    uint8_t const         idxRegDstIn = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 7);

    pCodeBuf[off++] = Armv8A64MkInstrRmif(idxRegEfl, (X86_EFL_CF_BIT - 1) & 63, RT_BIT_32(1) /*fMask=C*/);
    off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
    if (cOpBits >= 32)
        pCodeBuf[off++] = Armv8A64MkInstrAdcs(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    else
    {
        /* Since we're also adding in the carry flag here, shifting operands up
           doesn't work. So, we have to calculate carry & overflow manually. */
        pCodeBuf[off++] = Armv8A64MkInstrAdc(idxRegDst, idxRegDst, idxRegSrc, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrSetF8SetF16(idxRegDst, cOpBits > 8); /* NZ are okay, CV aren't.*/
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl, cOpBits, idxRegDst,
                                           idxRegDstIn, idxRegSrc, false /*fInvertCarry*/, 0);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    if (cOpBits < 32)
        off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegDst, RT_BIT_32(cOpBits) - 1U);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#else
# error "port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarEfl);
    return off;
}


/**
 * The ADC instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_adc_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegEfl = iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we use BT to set EFLAGS.CF and then issue an ADC instruction
       with matching size to get the correct flags. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 12);

    off = iemNativeEmitAmd64TwoByteModRmInstrRREx(pCodeBuf, off, 0x0f, 0x0b, 0xba, 32 /*cOpBits*/, 4, idxRegEfl);
    pCodeBuf[off++] = X86_EFL_CF_BIT;

    off = iemNativeEmitAmd64OneByteModRmInstrRIEx(pCodeBuf, off, 0x80, 0x83, 0x81, cOpBits, cImmBits, 2, idxRegDst, uImmOp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use the RMIF instructions to load PSTATE.CF from idxRegEfl
       and then ADCS for the calculation.  We need all inputs and result for
       the two flags (AF,PF) that can't be directly derived from PSTATE.NZCV. */
    uint8_t const         idxRegDstIn = iemNativeRegAllocTmp(pReNative, &off);
    uint8_t const         idxRegImm   = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
    PIEMNATIVEINSTR const pCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 4);

    pCodeBuf[off++] = Armv8A64MkInstrRmif(idxRegEfl, (X86_EFL_CF_BIT - 1) & 63, RT_BIT_32(1) /*fMask=C*/);
    off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
    if (cOpBits >= 32)
        pCodeBuf[off++] = Armv8A64MkInstrAdcs(idxRegDst, idxRegDst, idxRegImm, cOpBits > 32 /*f64Bit*/);
    else
    {
        /* Since we're also adding in the carry flag here, shifting operands up
           doesn't work. So, we have to calculate carry & overflow manually. */
        pCodeBuf[off++] = Armv8A64MkInstrAdc(idxRegDst, idxRegDst, idxRegImm, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrSetF8SetF16(idxRegDst, cOpBits > 8); /* NZ are okay, CV aren't.*/
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeRegFreeTmp(pReNative, idxRegImm);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl, cOpBits, idxRegDst,
                                           idxRegDstIn, UINT8_MAX, false /*fInvertCarry*/, uImmOp);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    if (cOpBits < 32)
        off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegDst, RT_BIT_32(cOpBits) - 1U);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    RT_NOREF(cImmBits);

#else
# error "port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarEfl);
    return off;
}


/**
 * The SUB instruction will set all status flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_sub_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly sized SUB instruction to get the right EFLAGS.SF value. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x2a, 0x2b, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we'll need the two input operands as well as the result in order
       to calculate the right flags, even if we use SUBS and translates NZCV into
       OF, CF, ZF and SF. */
    uint8_t const         idxRegDstIn = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 4);
    if (cOpBits >= 32)
    {
        off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
    }
    else
    {
        /* Shift the operands up so we can perform a 32-bit operation and get all four flags. */
        uint32_t const cShift = 32 - cOpBits;
        pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegDstIn, ARMV8_A64_REG_XZR, idxRegDst, false /*f64Bit*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegDst, idxRegDstIn, idxRegSrc, false /*f64Bit*/,
                                                true /*fSetFlags*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDstIn, idxRegDstIn, cShift, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDst, idxRegDst, cShift, false /*f64Bit*/);
        cOpBits = 32;
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegDst,
                                           idxRegDstIn, idxRegSrc, true /*fInvertCarry*/, 0);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#else
# error "port me"
#endif
    return off;
}


/**
 * The SUB instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_sub_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly sized SUB instruction to get the right EFLAGS.SF value. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    off = iemNativeEmitAmd64OneByteModRmInstrRIEx(pCodeBuf, off, 0x80, 0x83, 0x81, cOpBits, cImmBits, 5, idxRegDst, uImmOp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we'll need the two input operands as well as the result in order
       to calculate the right flags, even if we use SUBS and translates NZCV into
       OF, CF, ZF and SF. */
    uint8_t const   idxRegDstIn  = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR pCodeBuf     = iemNativeInstrBufEnsure(pReNative, off, 8);
    off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
    if (cOpBits >= 32)
    {
        if (uImmOp <= 0xfffU)
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(idxRegDst, idxRegDst, uImmOp, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
        else if (uImmOp <= 0xfff000U && !(uImmOp & 0xfff))
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(idxRegDst, idxRegDst, uImmOp >> 12, cOpBits > 32 /*f64Bit*/,
                                                       true /*fSetFlags*/, true /*fShift12*/);
        else
        {
            uint8_t const idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
            pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegDst, idxRegDst, idxRegTmpImm, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
            iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
        }
    }
    else
    {
        /* Shift the operands up so we can perform a 32-bit operation and get all four flags. */
        uint32_t const cShift       = 32 - cOpBits;
        uint8_t const  idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
        pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
        pCodeBuf[off++] = Armv8A64MkInstrLslImm(idxRegDstIn, idxRegDstIn, cShift, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegDst,   idxRegDstIn, idxRegTmpImm, false /*f64Bit*/, true /*fSetFlags*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDstIn, idxRegDstIn, cShift, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegDst,   idxRegDst,   cShift, false /*f64Bit*/);
        cOpBits = 32;
        iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegDst,
                                           idxRegDstIn, UINT8_MAX, true /*fInvertCarry*/, uImmOp);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    RT_NOREF(cImmBits);

#else
# error "port me"
#endif
    return off;
}


/**
 * The CMP instruction will set all status flags, but modifies no registers.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_cmp_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly sized CMP instruction to get the right EFLAGS.SF value. */
    off = iemNativeEmitAmd64OneByteModRmInstrRREx(iemNativeInstrBufEnsure(pReNative, off, 4), off,
                                                  0x3a, 0x3b, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we'll need the actual result as well as both input operands in order
       to calculate the right flags, even if we use SUBS and translates NZCV into
       OF, CF, ZF and SF. */
    uint8_t const         idxRegResult = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf     = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (cOpBits >= 32)
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegResult, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
    else
    {
        /* Shift the operands up so we can perform a 32-bit operation and get all four flags. */
        uint32_t const cShift = 32 - cOpBits;
        pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegResult, ARMV8_A64_REG_XZR, idxRegDst, false /*f64Bit*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegResult, idxRegResult, idxRegSrc, false /*f64Bit*/,
                                                true /*fSetFlags*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegResult, idxRegResult, cShift, false /*f64Bit*/);
        cOpBits = 32;
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegResult,
                                           idxRegDst, idxRegSrc, true /*fInvertCarry*/, 0);

    iemNativeRegFreeTmp(pReNative, idxRegResult);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#else
# error "port me"
#endif
    return off;
}


/**
 * The CMP instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_cmp_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we just use the correctly sized CMP instruction to get the right EFLAGS.SF value. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    off = iemNativeEmitAmd64OneByteModRmInstrRIEx(pCodeBuf, off, 0x80, 0x83, 0x81, cOpBits, cImmBits, 7, idxRegDst, uImmOp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we'll need the actual result as well as both input operands in order
       to calculate the right flags, even if we use SUBS and translates NZCV into
       OF, CF, ZF and SF. */
    uint8_t const   idxRegResult = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR pCodeBuf     = iemNativeInstrBufEnsure(pReNative, off, 8);
    if (cOpBits >= 32)
    {
        if (uImmOp <= 0xfffU)
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(idxRegResult, idxRegDst, uImmOp, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
        else if (uImmOp <= 0xfff000U && !(uImmOp & 0xfff))
            pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(idxRegResult, idxRegDst, uImmOp >> 12, cOpBits > 32 /*f64Bit*/,
                                                       true /*fSetFlags*/, true /*fShift12*/);
        else
        {
            uint8_t const idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
            pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegResult, idxRegDst, idxRegTmpImm, cOpBits > 32 /*f64Bit*/, true /*fSetFlags*/);
            iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
        }
    }
    else
    {
        /* Shift the operands up so we can perform a 32-bit operation and get all four flags. */
        uint32_t const cShift       = 32 - cOpBits;
        uint8_t const  idxRegTmpImm = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
        pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
        pCodeBuf[off++] = Armv8A64MkInstrLslImm(idxRegResult, idxRegDst,    cShift, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegResult, idxRegResult, idxRegTmpImm, false /*f64Bit*/, true /*fSetFlags*/, cShift);
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxRegResult, idxRegResult, cShift, false /*f64Bit*/);
        cOpBits = 32;
        iemNativeRegFreeTmpImm(pReNative, idxRegTmpImm);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegResult,
                                           idxRegDst, UINT8_MAX, true /*fInvertCarry*/, uImmOp);

    iemNativeRegFreeTmp(pReNative, idxRegResult);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    RT_NOREF(cImmBits);

#else
# error "port me"
#endif
    return off;
}


/**
 * The SBB instruction takes CF as input and will set all status flags.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_sbb_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off, true /*fInitialized*/);
    uint8_t const idxRegEfl = iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we use BT to set EFLAGS.CF and then issue an SBB instruction
       with matching size to get the correct flags. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 9);

    off = iemNativeEmitAmd64TwoByteModRmInstrRREx(pCodeBuf, off, 0x0f, 0x0b, 0xba, 32 /*cOpBits*/, 4, idxRegEfl);
    pCodeBuf[off++] = X86_EFL_CF_BIT;

    off = iemNativeEmitAmd64OneByteModRmInstrRREx(pCodeBuf, off, 0x1a, 0x1b, cOpBits, idxRegDst, idxRegSrc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use the RMIF+CFINV instructions to load PSTATE.CF from
       idxRegEfl and then SBCS for the calculation.  We need all inputs and
       result for the two flags (AF,PF) that can't be directly derived from
       PSTATE.NZCV. */
    uint8_t const         idxRegDstIn = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 5);

    pCodeBuf[off++] = Armv8A64MkInstrRmif(idxRegEfl, (X86_EFL_CF_BIT - 1) & 63, RT_BIT_32(1) /*fMask=C*/);
    pCodeBuf[off++] = ARMV8_A64_INSTR_CFINV;
    off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
    if (cOpBits >= 32)
        pCodeBuf[off++] = Armv8A64MkInstrSbcs(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    else
    {
        /* Since we're also adding in the carry flag here, shifting operands up
           doesn't work. So, we have to calculate carry & overflow manually. */
        pCodeBuf[off++] = Armv8A64MkInstrSbc(idxRegDst, idxRegDst, idxRegSrc, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrSetF8SetF16(idxRegDst, cOpBits > 8); /* NZ are okay, CV aren't.*/
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl, cOpBits, idxRegDst,
                                           idxRegDstIn, idxRegSrc, true /*fInvertCarry*/, 0);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    if (cOpBits < 32)
        off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegDst, RT_BIT_32(cOpBits) - 1U);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#else
# error "port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarEfl);
    return off;
}


/**
 * The SBB instruction with immediate value as right operand.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmit_sbb_r_i_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint64_t uImmOp, uint8_t idxVarEfl, uint8_t cOpBits, uint8_t cImmBits)
{
    uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off, true /*fInitialized*/);
    uint8_t const idxRegEfl = iemNativeVarRegisterAcquire(pReNative, idxVarEfl, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* On AMD64 we use BT to set EFLAGS.CF and then issue an SBB instruction
       with matching size to get the correct flags. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 12);

    off = iemNativeEmitAmd64TwoByteModRmInstrRREx(pCodeBuf, off, 0x0f, 0x0b, 0xba, 32 /*cOpBits*/, 4, idxRegEfl);
    pCodeBuf[off++] = X86_EFL_CF_BIT;

    off = iemNativeEmitAmd64OneByteModRmInstrRIEx(pCodeBuf, off, 0x80, 0x83, 0x81, cOpBits, cImmBits, 3, idxRegDst, uImmOp);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxVarDst);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl);

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use the RMIF+CFINV instructions to load PSTATE.CF from
       idxRegEfl and then SBCS for the calculation.  We need all inputs and
       result for the two flags (AF,PF) that can't be directly derived from
       PSTATE.NZCV. */
    uint8_t const         idxRegDstIn = iemNativeRegAllocTmp(pReNative, &off);
    uint8_t const         idxRegImm   = iemNativeRegAllocTmpImm(pReNative, &off, uImmOp);
    PIEMNATIVEINSTR const pCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 5);

    pCodeBuf[off++] = Armv8A64MkInstrRmif(idxRegEfl, (X86_EFL_CF_BIT - 1) & 63, RT_BIT_32(1) /*fMask=C*/);
    pCodeBuf[off++] = ARMV8_A64_INSTR_CFINV;
    off = iemNativeEmitLoadGprFromGprEx(pCodeBuf, off, idxRegDstIn, idxRegDst);
    if (cOpBits >= 32)
        pCodeBuf[off++] = Armv8A64MkInstrSbcs(idxRegDst, idxRegDst, idxRegImm, cOpBits > 32 /*f64Bit*/);
    else
    {
        /* Since we're also adding in the carry flag here, shifting operands up
           doesn't work. So, we have to calculate carry & overflow manually. */
        pCodeBuf[off++] = Armv8A64MkInstrSbc(idxRegDst, idxRegDst, idxRegImm, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrSetF8SetF16(idxRegDst, cOpBits > 8); /* NZ are okay, CV aren't.*/
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeRegFreeTmp(pReNative, idxRegImm);

    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, UINT8_MAX, idxRegEfl, cOpBits, idxRegDst,
                                           idxRegDstIn, UINT8_MAX, true /*fInvertCarry*/, uImmOp);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    if (cOpBits < 32)
        off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegDst, RT_BIT_32(cOpBits) - 1U);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
    RT_NOREF(cImmBits);

#else
# error "port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarEfl);
    return off;
}


DECL_INLINE_THROW(uint32_t)
iemNativeEmit_imul_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                           uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    RT_NOREF(idxVarDst, idxVarSrc, idxVarEfl, cOpBits);
    AssertFailed();
    return iemNativeEmitBrk(pReNative, off, 0x666);
}


DECL_INLINE_THROW(uint32_t)
iemNativeEmit_popcnt_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                             uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    RT_NOREF(idxVarDst, idxVarSrc, idxVarEfl, cOpBits);
    AssertFailed();
    return iemNativeEmitBrk(pReNative, off, 0x666);
}


DECL_INLINE_THROW(uint32_t)
iemNativeEmit_tzcnt_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                            uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    RT_NOREF(idxVarDst, idxVarSrc, idxVarEfl, cOpBits);
    AssertFailed();
    return iemNativeEmitBrk(pReNative, off, 0x666);
}


DECL_INLINE_THROW(uint32_t)
iemNativeEmit_lzcnt_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                            uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    RT_NOREF(idxVarDst, idxVarSrc, idxVarEfl, cOpBits);
    AssertFailed();
    return iemNativeEmitBrk(pReNative, off, 0x666);
}


#endif /* !VMM_INCLUDED_SRC_VMMAll_target_x86_IEMAllN8veEmit_x86_h */
