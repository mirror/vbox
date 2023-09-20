/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Native Recompiler Internals.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_include_IEMN8veRecompiler_h
#define VMM_INCLUDED_SRC_include_IEMN8veRecompiler_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/** @defgroup grp_iem_n8ve_re   Native Recompiler Internals.
 * @ingroup grp_iem_int
 * @{
 */

/** Native code generator label types. */
typedef enum
{
    kIemNativeLabelType_Invalid = 0,
    kIemNativeLabelType_Return,
    kIemNativeLabelType_NonZeroRetOrPassUp,
    kIemNativeLabelType_End
} IEMNATIVELABELTYPE;

/** Native code generator label definition. */
typedef struct IEMNATIVELABEL
{
    /** Code offset if defined, UINT32_MAX if it needs to be generated after/in
     * the epilog. */
    uint32_t    off;
    /** The type of label (IEMNATIVELABELTYPE). */
    uint16_t    enmType;
    /** Additional label data, type specific. */
    uint16_t    uData;
} IEMNATIVELABEL;
/** Pointer to a label. */
typedef IEMNATIVELABEL *PIEMNATIVELABEL;


/** Native code generator fixup types.  */
typedef enum
{
    kIemNativeFixupType_Invalid = 0,
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    /** AMD64 fixup: PC relative 32-bit with addend in bData. */
    kIemNativeFixupType_Rel32,
#elif defined(RT_ARCH_ARM64)
#endif
    kIemNativeFixupType_End
} IEMNATIVEFIXUPTYPE;

/** Native code generator fixup. */
typedef struct IEMNATIVEFIXUP
{
    /** Code offset of the fixup location. */
    uint32_t    off;
    /** The IEMNATIVELABEL this is a fixup for. */
    uint16_t    idxLabel;
    /** The fixup type (IEMNATIVEFIXUPTYPE). */
    uint8_t     enmType;
    /** Addend or other data. */
    int8_t      offAddend;
} IEMNATIVEFIXUP;
/** Pointer to a native code generator fixup. */
typedef IEMNATIVEFIXUP *PIEMNATIVEFIXUP;

/**
 * Native recompiler state.
 */
typedef struct IEMRECOMPILERSTATE
{
    /** Size of the buffer that pbNativeRecompileBufR3 points to in
     * IEMNATIVEINSTR units. */
    uint32_t                    cInstrBufAlloc;
    uint32_t                    uPadding; /* We don't keep track of this here... */
    /** Fixed temporary code buffer for native recompilation. */
    PIEMNATIVEINSTR             pInstrBuf;

    /** Actual number of labels in paLabels. */
    uint32_t                    cLabels;
    /** Max number of entries allowed in paLabels before reallocating it. */
    uint32_t                    cLabelsAlloc;
    /** Labels defined while recompiling (referenced by fixups). */
    PIEMNATIVELABEL             paLabels;

    /** Actual number of fixups paFixups. */
    uint32_t                    cFixups;
    /** Max number of entries allowed in paFixups before reallocating it. */
    uint32_t                    cFixupsAlloc;
    /** Buffer used by the recompiler for recording fixups when generating code. */
    PIEMNATIVEFIXUP             paFixups;
} IEMRECOMPILERSTATE;
/** Pointer to a native recompiler state. */
typedef IEMRECOMPILERSTATE *PIEMRECOMPILERSTATE;



DECLHIDDEN(uint32_t)        iemNativeMakeLabel(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                                               uint32_t offWhere = UINT32_MAX, uint16_t uData = 0) RT_NOEXCEPT;
DECLHIDDEN(bool)            iemNativeAddFixup(PIEMRECOMPILERSTATE pReNative, uint32_t offWhere, uint32_t idxLabel,
                                              IEMNATIVEFIXUPTYPE enmType, int8_t offAddend = 0) RT_NOEXCEPT;
DECLHIDDEN(PIEMNATIVEINSTR) iemNativeInstrBufEnsureSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                        uint32_t cInstrReq) RT_NOEXCEPT;

DECLHIDDEN(uint32_t)        iemNativeEmitCheckCallRetAndPassUp(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                               uint8_t idxInstr) RT_NOEXCEPT;


/**
 * Ensures that there is sufficient space in the instruction output buffer.
 *
 * This will reallocate the buffer if needed and allowed.
 *
 * @returns Pointer to the instruction output buffer on success, NULL on
 *          failure.
 * @param   pReNative   The native recompile state.
 * @param   off         Current instruction offset.
 * @param   cInstrReq   Number of instruction about to be added.  It's okay to
 *                      overestimate this a bit.
 */
DECL_FORCE_INLINE(PIEMNATIVEINSTR) iemNativeInstrBufEnsure(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t cInstrReq)
{
    if (RT_LIKELY(off + cInstrReq <= pReNative->cInstrBufAlloc))
        return pReNative->pInstrBuf;
    return iemNativeInstrBufEnsureSlow(pReNative, off, cInstrReq);
}


/**
 * Emit a simple marker instruction to more easily tell where something starts
 * in the disassembly.
 */
DECLINLINE(uint32_t) iemNativeEmitMarker(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0x90;                    /* nop */

#elif RT_ARCH_ARM64
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = 0xe503201f;            /* nop? */

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits setting a GPR to zero.
 */
DECLINLINE(uint32_t) iemNativeEmitGprZero(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGpr >= 8)                          /* xor gpr32, gpr32 */
        pbCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGpr & 7, iGpr & 7);

#elif RT_ARCH_ARM64
    RT_NOREF(pReNative, iGpr, uImm64);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    RT_NOREF(pReNative);
    return off;
}


/**
 * Emits loading a constant into a 64-bit GPR
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGprImm64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint64_t uImm64)
{
    if (!uImm64)
        return iemNativeEmitGprZero(pReNative, off, iGpr);

#ifdef RT_ARCH_AMD64
    if (uImm64 <= UINT32_MAX)
    {
        /* mov gpr, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        if (iGpr >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
        pbCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pbCodeBuf[off++] = RT_BYTE1(uImm64);
        pbCodeBuf[off++] = RT_BYTE2(uImm64);
        pbCodeBuf[off++] = RT_BYTE3(uImm64);
        pbCodeBuf[off++] = RT_BYTE4(uImm64);
    }
    else
    {
        /* mov gpr, imm64 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        if (iGpr < 8)
            pbCodeBuf[off++] = X86_OP_REX_W;
        else
            pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_B;
        pbCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pbCodeBuf[off++] = RT_BYTE1(uImm64);
        pbCodeBuf[off++] = RT_BYTE2(uImm64);
        pbCodeBuf[off++] = RT_BYTE3(uImm64);
        pbCodeBuf[off++] = RT_BYTE4(uImm64);
        pbCodeBuf[off++] = RT_BYTE5(uImm64);
        pbCodeBuf[off++] = RT_BYTE6(uImm64);
        pbCodeBuf[off++] = RT_BYTE7(uImm64);
        pbCodeBuf[off++] = RT_BYTE8(uImm64);
    }

#elif RT_ARCH_ARM64
    RT_NOREF(pReNative, iGpr, uImm64);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a 32-bit GPR load of a VCpu value.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGprFromVCpuU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /* mov reg32, mem32 */
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    if (offVCpu < 128)
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGpr & 7, X86_GREG_xBX);
        pbCodeBuf[off++] = (uint8_t)offVCpu;
    }
    else
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, iGpr & 7, X86_GREG_xBX);
        pbCodeBuf[off++] = RT_BYTE1(offVCpu);
        pbCodeBuf[off++] = RT_BYTE2(offVCpu);
        pbCodeBuf[off++] = RT_BYTE3(offVCpu);
        pbCodeBuf[off++] = RT_BYTE4(offVCpu);
    }

#elif RT_ARCH_ARM64
    RT_NOREF(pReNative, idxInstr);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a gprdst = gprsrc load.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGprFromGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, gprsrc */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_W | X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_W | X86_OP_REX_R;
    else
        pbCodeBuf[off++] = X86_OP_REX_W;
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif RT_ARCH_ARM64
    RT_NOREF(pReNative, iGprDst, iGprSrc);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}

#ifdef RT_ARCH_AMD64
/**
 * Common bit of iemNativeEmitLoadGprByBp and friends.
 */
DECL_FORCE_INLINE(uint32_t) iemNativeEmitGprByBpDisp(uint8_t *pbCodeBuf, uint32_t off, uint8_t iGprReg, int32_t offDisp)
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
    return off;
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GRP load instruction with an BP relative source address.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
    /* mov gprdst, qword [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp);
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a 32-bit GRP load instruction with an BP relative source address.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGprByBpU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
    /* mov gprdst, dword [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp);
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a load effective address to a GRP with an BP relative source address.
 */
DECLINLINE(uint32_t) iemNativeEmitLeaGrpByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
    /* lea gprdst, [rbp + offDisp] */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8d;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp);
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GPR store with an BP relative destination address.
 */
DECLINLINE(uint32_t) iemNativeEmitStoreGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offDisp, uint8_t iGprSrc)
{
    /* mov qword [rbp + offDisp], gprdst */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprSrc < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x89;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprSrc, offDisp);
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GPR subtract with a signed immediate subtrahend.
 */
DECLINLINE(uint32_t) iemNativeEmitSubGprImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iSubtrahend)
{
    /* sub gprdst, imm8/imm32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 7)
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
    return off;
}
#endif

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_IEMN8veRecompiler_h */

