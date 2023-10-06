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

/** @name Stack Frame Layout
 *
 * @{  */
/** The size of the area for stack variables and spills and stuff. */
#define IEMNATIVE_FRAME_VAR_SIZE            0x40
#ifdef RT_ARCH_AMD64
/** Number of stack arguments slots for calls made from the frame. */
# define IEMNATIVE_FRAME_STACK_ARG_COUNT    4
/** An stack alignment adjustment (between non-volatile register pushes and
 *  the stack variable area, so the latter better aligned). */
# define IEMNATIVE_FRAME_ALIGN_SIZE         8
/** Number of any shadow arguments (spill area) for calls we make. */
# ifdef RT_OS_WINDOWS
#  define IEMNATIVE_FRAME_SHADOW_ARG_COUNT  4
# else
#  define IEMNATIVE_FRAME_SHADOW_ARG_COUNT  0
# endif

/** Frame pointer (RBP) relative offset of the last push. */
# ifdef RT_OS_WINDOWS
#  define IEMNATIVE_FP_OFF_LAST_PUSH        (7 * -8)
# else
#  define IEMNATIVE_FP_OFF_LAST_PUSH        (5 * -8)
# endif
/** Frame pointer (RBP) relative offset of the stack variable area (the lowest
 * address for it). */
# define IEMNATIVE_FP_OFF_STACK_VARS        (IEMNATIVE_FP_OFF_LAST_PUSH - IEMNATIVE_FRAME_ALIGN_SIZE - IEMNATIVE_FRAME_VAR_SIZE)
/** Frame pointer (RBP) relative offset of the first stack argument for calls. */
# define IEMNATIVE_FP_OFF_STACK_ARG0        (IEMNATIVE_FP_OFF_STACK_VARS - IEMNATIVE_FRAME_STACK_ARG_COUNT * 8)
/** Frame pointer (RBP) relative offset of the second stack argument for calls. */
# define IEMNATIVE_FP_OFF_STACK_ARG1        (IEMNATIVE_FP_OFF_STACK_ARG0 + 8)
/** Frame pointer (RBP) relative offset of the third stack argument for calls. */
# define IEMNATIVE_FP_OFF_STACK_ARG2        (IEMNATIVE_FP_OFF_STACK_ARG0 + 16)
/** Frame pointer (RBP) relative offset of the fourth stack argument for calls. */
# define IEMNATIVE_FP_OFF_STACK_ARG3        (IEMNATIVE_FP_OFF_STACK_ARG0 + 24)

# ifdef RT_OS_WINDOWS
/** Frame pointer (RBP) relative offset of the first incoming shadow argument. */
#  define IEMNATIVE_FP_OFF_IN_SHADOW_ARG0   (16)
/** Frame pointer (RBP) relative offset of the second incoming shadow argument. */
#  define IEMNATIVE_FP_OFF_IN_SHADOW_ARG1   (24)
/** Frame pointer (RBP) relative offset of the third incoming shadow argument. */
#  define IEMNATIVE_FP_OFF_IN_SHADOW_ARG2   (32)
/** Frame pointer (RBP) relative offset of the fourth incoming shadow argument. */
#  define IEMNATIVE_FP_OFF_IN_SHADOW_ARG3   (40)
# endif

#elif RT_ARCH_ARM64
/** No stack argument slots, enough got 8 registers for arguments.  */
# define IEMNATIVE_FRAME_STACK_ARG_COUNT    0
/** There are no argument spill area. */
# define IEMNATIVE_FRAME_SHADOW_ARG_COUNT   0

/** Number of saved registers at the top of our stack frame.
 * This includes the return address and old frame pointer, so x19 thru x30. */
# define IEMNATIVE_FRAME_SAVE_REG_COUNT     (12)
/** The size of the save registered (IEMNATIVE_FRAME_SAVE_REG_COUNT). */
# define IEMNATIVE_FRAME_SAVE_REG_SIZE      (IEMNATIVE_FRAME_SAVE_REG_COUNT * 8)

/** Frame pointer (BP) relative offset of the last push. */
# define IEMNATIVE_FP_OFF_LAST_PUSH         (7 * -8)

/** Frame pointer (BP) relative offset of the stack variable area (the lowest
 * address for it). */
# define IEMNATIVE_FP_OFF_STACK_VARS        (IEMNATIVE_FP_OFF_LAST_PUSH - IEMNATIVE_FRAME_ALIGN_SIZE - IEMNATIVE_FRAME_VAR_SIZE)

#else
# error "port me"
#endif
/** @} */


/** @name Fixed Register Allocation(s)
 * @{ */
/** @def IEMNATIVE_REG_FIXED_PVMCPU
 * The register number hold in pVCpu pointer.  */
/** @def IEMNATIVE_REG_FIXED_TMP0
 * Dedicated temporary register.
 * @todo replace this by a register allocator and content tracker.  */
#ifdef RT_ARCH_AMD64
# define IEMNATIVE_REG_FIXED_PVMCPU         X86_GREG_xBX
# define IEMNATIVE_REG_FIXED_TMP0           X86_GREG_x11

#elif defined(RT_ARCH_ARM64)
# define IEMNATIVE_REG_FIXED_PVMCPU         ARMV8_A64_REG_X28
# define IEMNATIVE_REG_FIXED_TMP0           ARMV8_A64_REG_X15

#else
# error "port me"
#endif
/** @} */

/** @name Call related registers.
 * @{ */
/** @def IEMNATIVE_CALL_RET_GREG
 * The return value register. */
/** @def IEMNATIVE_CALL_ARG_GREG_COUNT
 * Number of arguments in registers. */
/** @def IEMNATIVE_CALL_ARG0_GREG
 * The general purpose register carrying argument \#0. */
/** @def IEMNATIVE_CALL_ARG1_GREG
 * The general purpose register carrying argument \#1. */
/** @def IEMNATIVE_CALL_ARG2_GREG
 * The general purpose register carrying argument \#2. */
/** @def IEMNATIVE_CALL_ARG3_GREG
 * The general purpose register carrying argument \#3. */
#ifdef RT_ARCH_AMD64
# define IEMNATIVE_CALL_RET_GREG             X86_GREG_xAX

# ifdef RT_OS_WINDOWS
#  define IEMNATIVE_CALL_ARG_GREG_COUNT     4
#  define IEMNATIVE_CALL_ARG0_GREG          X86_GREG_xCX
#  define IEMNATIVE_CALL_ARG1_GREG          X86_GREG_xDX
#  define IEMNATIVE_CALL_ARG2_GREG          X86_GREG_x8
#  define IEMNATIVE_CALL_ARG3_GREG          X86_GREG_x9
# else
#  define IEMNATIVE_CALL_ARG_GREG_COUNT     6
#  define IEMNATIVE_CALL_ARG0_GREG          X86_GREG_xDI
#  define IEMNATIVE_CALL_ARG1_GREG          X86_GREG_xSI
#  define IEMNATIVE_CALL_ARG2_GREG          X86_GREG_xDX
#  define IEMNATIVE_CALL_ARG3_GREG          X86_GREG_xCX
#  define IEMNATIVE_CALL_ARG4_GREG          X86_GREG_x8
#  define IEMNATIVE_CALL_ARG5_GREG          X86_GREG_x9
# endif

#elif defined(RT_ARCH_ARM64)
# define IEMNATIVE_CALL_RET_GREG            ARMV8_A64_REG_X0
# define IEMNATIVE_CALL_ARG_GREG_COUNT      8
# define IEMNATIVE_CALL_ARG0_GREG           ARMV8_A64_REG_X0
# define IEMNATIVE_CALL_ARG1_GREG           ARMV8_A64_REG_X1
# define IEMNATIVE_CALL_ARG2_GREG           ARMV8_A64_REG_X2
# define IEMNATIVE_CALL_ARG3_GREG           ARMV8_A64_REG_X3
# define IEMNATIVE_CALL_ARG4_GREG           ARMV8_A64_REG_X4
# define IEMNATIVE_CALL_ARG5_GREG           ARMV8_A64_REG_X5
# define IEMNATIVE_CALL_ARG6_GREG           ARMV8_A64_REG_X6
# define IEMNATIVE_CALL_ARG7_GREG           ARMV8_A64_REG_X7

#endif

/** @} */

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
    /** ARM64 fixup: PC relative offset at bits 23:5 (CBZ, CBNZ).  */
    kIemNativeFixupType_RelImm19At5,
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


/**
 * Native recompiler worker for a threaded function.
 *
 * @returns New code buffer offset, UINT32_MAX in case of failure.
 * @param   pReNative   The native recompiler state.
 * @param   off         The current code buffer offset.
 * @param   pCallEntry  The threaded call entry.
 *
 * @note    This is not allowed to throw anything atm.
 */
typedef DECLCALLBACKTYPE(uint32_t, FNIEMNATIVERECOMPFUNC,(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                          PCIEMTHRDEDCALLENTRY pCallEntry));
/** Pointer to a native recompiler worker for a threaded function. */
typedef FNIEMNATIVERECOMPFUNC *PFNIEMNATIVERECOMPFUNC;

/** Defines a native recompiler worker for a threaded function. */
#define IEM_DECL_IEMNATIVERECOMPFUNC_DEF(a_Name) \
    DECLCALLBACK(uint32_t) a_Name(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)
/** Prototypes a native recompiler function for a threaded function. */
#define IEM_DECL_IEMNATIVERECOMPFUNC_PROTO(a_Name) FNIEMNATIVERECOMPFUNC a_Name


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
 * @param   off         Current instruction offset.  Works safely for UINT32_MAX
 *                      as well.
 * @param   cInstrReq   Number of instruction about to be added.  It's okay to
 *                      overestimate this a bit.
 */
DECL_FORCE_INLINE(PIEMNATIVEINSTR) iemNativeInstrBufEnsure(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t cInstrReq)
{
    if (RT_LIKELY(off + (uint64_t)cInstrReq <= pReNative->cInstrBufAlloc))
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
    /* nop */
    pbCodeBuf[off++] = 0x90;

#elif RT_ARCH_ARM64
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    /* nop */
    pu32CodeBuf[off++] = 0xd503201f;

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
    /* xor gpr32, gpr32 */
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGpr & 7, iGpr & 7);

#elif RT_ARCH_ARM64
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    /* mov gpr, #0x0 */
    pu32CodeBuf[off++] = UINT32_C(0xd2800000) | iGpr;

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
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pu32CodeBuf, UINT32_MAX);

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
    uint32_t fMovK = 0;
    /* mov  gpr, imm16 */
    uint32_t uImmPart = ((uint32_t)((uImm64 >>  0) & UINT32_C(0xffff)) << 5);
    if (uImmPart)
    {
        pu32CodeBuf[off++] = UINT32_C(0xd2800000) |         (UINT32_C(0) << 21) | uImmPart | iGpr;
        fMovK |= RT_BIT_32(29);
    }
    /* mov[k] gpr, imm16, lsl #16 */
    uImmPart = ((uint32_t)((uImm64 >> 16) & UINT32_C(0xffff)) << 5);
    if (uImmPart)
    {
        pu32CodeBuf[off++] = UINT32_C(0xd2800000) | fMovK | (UINT32_C(1) << 21) | uImmPart | iGpr;
        fMovK |= RT_BIT_32(29);
    }
    /* mov[k] gpr, imm16, lsl #32 */
    uImmPart = ((uint32_t)((uImm64 >> 32) & UINT32_C(0xffff)) << 5);
    if (uImmPart)
    {
        pu32CodeBuf[off++] = UINT32_C(0xd2800000) | fMovK | (UINT32_C(2) << 21) | uImmPart | iGpr;
        fMovK |= RT_BIT_32(29);
    }
    /* mov[k] gpr, imm16, lsl #48 */
    uImmPart = ((uint32_t)((uImm64 >> 48) & UINT32_C(0xffff)) << 5);
    if (uImmPart)
        pu32CodeBuf[off++] = UINT32_C(0xd2800000) | fMovK | (UINT32_C(3) << 21) | uImmPart | iGpr;

    /** @todo there is an inverted mask variant we might want to explore if it
     *        reduces the number of instructions... */
    /** @todo load into 'w' register instead of 'x' when imm64 <= UINT32_MAX?
     *        clang 12.x does that, only to use the 'x' version for the
     *        addressing in the following ldr). */

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
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGpr & 7, IEMNATIVE_REG_FIXED_PVMCPU);
        pbCodeBuf[off++] = (uint8_t)offVCpu;
    }
    else
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, iGpr & 7, IEMNATIVE_REG_FIXED_PVMCPU);
        pbCodeBuf[off++] = RT_BYTE1(offVCpu);
        pbCodeBuf[off++] = RT_BYTE2(offVCpu);
        pbCodeBuf[off++] = RT_BYTE3(offVCpu);
        pbCodeBuf[off++] = RT_BYTE4(offVCpu);
    }

#elif RT_ARCH_ARM64
    /*
     * There are a couple of ldr variants that takes an immediate offset, so
     * try use those if we can, otherwise we have to use the temporary register
     * help with the addressing.
     */
    if (offVCpu < _16K)
    {
        /* Use the unsigned variant of ldr Wt, [<Xn|SP>, #off]. */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = UINT32_C(0xb9400000) | (offVCpu << 10) | (IEMNATIVE_REG_FIXED_PVMCPU << 5) | iGpr;
    }
    else
    {
        /* The offset is too large, so we must load it into a register and use
           ldr Wt, [<Xn|SP>, (<Wm>|<Xm>). */
        /** @todo reduce by offVCpu by >> 3 or >> 2? if it saves instructions? */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, offVCpu);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = UINT32_C(0xb8600800) | ((uint32_t)IEMNATIVE_REG_FIXED_TMP0 << 16)
                           | ((uint32_t)IEMNATIVE_REG_FIXED_PVMCPU << 5) | iGpr;
    }

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
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    /* mov dst, src;   alias for: orr dst, xzr, src */
    pu32CodeBuf[off++] = UINT32_C(0xaa000000) | ((uint32_t)iGprSrc << 16) | ((uint32_t)ARMV8_A64_REG_XZR << 5) | iGprDst;

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
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8d;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp);
}
#endif


/**
 * Emits a 64-bit GPR store with an BP relative destination address.
 *
 * @note May trash IEMNATIVE_REG_FIXED_TMP0.
 */
DECLINLINE(uint32_t) iemNativeEmitStoreGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offDisp, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov qword [rbp + offDisp], gprdst */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGprSrc < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x89;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprSrc, offDisp);

#elif defined(RT_ARCH_ARM64)
    if (offDisp >= 0 && offDisp < 4096 * 8 && !((uint32_t)offDisp & 7))
    {
        /* str w/ unsigned imm12 (scaled) */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_St_Dword, iGprSrc,
                                                      ARMV8_A64_REG_BP, (uint32_t)offDisp / 8);
    }
    else if (offDisp >= -256 && offDisp <= 256)
    {
        /* stur w/ signed imm9 (unscaled) */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = Armv8A64MkInstrSturLdur(kArmv8A64InstrLdStType_St_Dword, iGprSrc, ARMV8_A64_REG_BP, offDisp);
    }
    else
    {
        /* Use temporary indexing register. */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, (uint32_t)offDisp);
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(kArmv8A64InstrLdStType_St_Dword, iGprSrc, ARMV8_A64_REG_BP,
                                                       IEMNATIVE_REG_FIXED_TMP0, kArmv8A64InstrLdStExtend_Sxtw);
    }
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
DECLINLINE(uint32_t) iemNativeEmitStoreImm64ByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offDisp, uint64_t uImm64)
{
#ifdef RT_ARCH_AMD64
    if ((int64_t)uImm64 == (int32_t)uImm64)
    {
        /* mov qword [rbp + offDisp], imm32 - sign extended */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 11);
        AssertReturn(pbCodeBuf, UINT32_MAX);

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
        return off;
    }
#endif

    /* Load tmp0, imm64; Store tmp to bp+disp. */
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, uImm64);
    return iemNativeEmitStoreGprByBp(pReNative, off, offDisp, IEMNATIVE_REG_FIXED_TMP0);
}


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GPR subtract with a signed immediate subtrahend.
 */
DECLINLINE(uint32_t) iemNativeEmitSubGprImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iSubtrahend)
{
    /* sub gprdst, imm8/imm32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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

