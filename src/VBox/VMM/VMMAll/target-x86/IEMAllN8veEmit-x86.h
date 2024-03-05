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

#endif /* RT_ARCH_AMD64 */

/**
 * This is an implementation of IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGICAL.
 *
 * It takes liveness stuff into account.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitEFlagsForLogical(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarEfl,
                              uint8_t cOpBits, uint8_t idxRegResult)
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
        RT_NOREF(cOpBits, idxRegResult);

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

        /* Calculate zero: mov tmp, zf; cmp result,zero; csel.eq tmp,tmp,wxr */
        if (cOpBits > 32)
            off = iemNativeEmitCmpGprWithGprEx(pCodeBuf, off, idxRegResult, ARMV8_A64_REG_XZR);
        else
            off = iemNativeEmitCmpGpr32WithGprEx(pCodeBuf, off, idxRegResult, ARMV8_A64_REG_XZR);
        pCodeBuf[off++] = Armv8A64MkInstrCSet(idxTmpReg, kArmv8InstrCond_Eq, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegEfl, idxRegEfl, idxTmpReg, false /*f64Bit*/, X86_EFL_ZF_BIT);

        /* Calculate signed: We could use the native SF flag, but it's just as simple to calculate it by shifting. */
        pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxRegResult, cOpBits - 1, cOpBits > 32 /*f64Bit*/);
# if 0 /* BFI and ORR hsould have the same performance characteristics, so use BFI like we'll have to do for SUB/ADD/++. */
        pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegEfl, idxRegEfl, idxTmpReg, false /*f64Bit*/, X86_EFL_SF_BIT);
# else
        pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl, idxTmpReg, X86_EFL_SF_BIT, 1, false /*f64Bit*/);
# endif

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
                                 , bool fInvertCarry
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

            /* The overflow flag is more work. See IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC. */
            pCodeBuf[off++] = Armv8A64MkInstrEon(idxTmpReg,  idxRegDstIn, idxRegSrc,    false); /* ~((a_uDst) ^ (a_uSrcOf)) */
            pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg2, idxRegDstIn, idxRegResult, false); /*  (a_uDst) ^ (a_uResult) */
            pCodeBuf[off++] = Armv8A64MkInstrAnd(idxTmpReg,  idxTmpReg,   idxTmpReg2,   false /*f64Bit*/);
            pCodeBuf[off++] = Armv8A64MkInstrLsrImm(idxTmpReg, idxTmpReg, cOpBits - 1,  false /*f64Bit*/);
            pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegEfl,  idxTmpReg, X86_EFL_OF_BIT, 1);
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

        /* Calculate auxilary carry/borrow.  This is related to 8-bit BCD.*/
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxRegDstIn, idxRegSrc, false /*f64Bit*/);
        pCodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxTmpReg, idxRegResult, false /*f64Bit*/);
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

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit AND for the 8-bit and 16-bit bit ones. */
    /** @todo we should use ANDS on ARM64 and get the ZF for free for all
     *        variants, and SF for 32-bit and 64-bit.  */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrAnd(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst);
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
    /** @todo we should use ANDS on ARM64 and get the ZF for free for all
     *        variants, and SF for 32-bit and 64-bit.  */
    uint8_t const         idxRegResult = iemNativeRegAllocTmp(pReNative, &off);
    PIEMNATIVEINSTR const pCodeBuf     = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrAnd(idxRegResult, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif
    if (idxVarSrc != idxVarDst)
        iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#ifdef RT_ARCH_AMD64
    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, UINT8_MAX);
#else
    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegResult);
    iemNativeRegFreeTmp(pReNative, idxRegResult);
#endif
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

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit OR for the 8-bit and 16-bit bit ones. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrOrr(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
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

#elif defined(RT_ARCH_ARM64)
    /* On ARM64 we use 32-bit OR for the 8-bit and 16-bit bit ones. */
    PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pCodeBuf[off++] = Armv8A64MkInstrEor(idxRegDst, idxRegDst, idxRegSrc, cOpBits > 32 /*f64Bit*/);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);

    off = iemNativeEmitEFlagsForLogical(pReNative, off, idxVarEfl, cOpBits, idxRegDst);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);
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

    /** @todo Explain why the carry flag shouldn't be inverted for ADDS. */
    off = iemNativeEmitEFlagsForArithmetic(pReNative, off, idxVarEfl, UINT8_MAX, cOpBits, idxRegDst,
                                           idxRegDstIn, idxRegSrc, false /*fInvertCarry*/);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

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
                                           idxRegDstIn, idxRegSrc, false /*fInvertCarry*/);

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
                                           idxRegDstIn, idxRegSrc, true /*fInvertCarry*/);

    iemNativeRegFreeTmp(pReNative, idxRegDstIn);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

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
                                           idxRegDst, idxRegSrc, true /*fInvertCarry*/);

    iemNativeRegFreeTmp(pReNative, idxRegResult);
    iemNativeVarRegisterRelease(pReNative, idxVarSrc);
    iemNativeVarRegisterRelease(pReNative, idxVarDst);

#else
# error "port me"
#endif
    return off;
}


DECL_INLINE_THROW(uint32_t)
iemNativeEmit_sbb_r_r_efl(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                          uint8_t idxVarDst, uint8_t idxVarSrc, uint8_t idxVarEfl, uint8_t cOpBits)
{
    RT_NOREF(idxVarDst, idxVarSrc, idxVarEfl, cOpBits);
    AssertFailed();
    return iemNativeEmitBrk(pReNative, off, 0x666);
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
