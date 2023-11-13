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

/** @def IEMNATIVE_WITH_TB_DEBUG_INFO
 * Enables generating internal debug info for better TB disassembly dumping. */
#if defined(DEBUG) || defined(DOXYGEN_RUNNING)
# define IEMNATIVE_WITH_TB_DEBUG_INFO
#endif


/** @name Stack Frame Layout
 *
 * @{  */
/** The size of the area for stack variables and spills and stuff.
 * @note This limit is duplicated in the python script(s).  We add 0x40 for
 *       alignment padding. */
#define IEMNATIVE_FRAME_VAR_SIZE            (0xc0 + 0x40)
/** Number of 64-bit variable slots (0x100 / 8 = 32. */
#define IEMNATIVE_FRAME_VAR_SLOTS           (IEMNATIVE_FRAME_VAR_SIZE / 8)
AssertCompile(IEMNATIVE_FRAME_VAR_SLOTS == 32);

#ifdef RT_ARCH_AMD64
/** An stack alignment adjustment (between non-volatile register pushes and
 *  the stack variable area, so the latter better aligned). */
# define IEMNATIVE_FRAME_ALIGN_SIZE         8

/** Number of stack arguments slots for calls made from the frame. */
# ifdef RT_OS_WINDOWS
#  define IEMNATIVE_FRAME_STACK_ARG_COUNT   4
# else
#  define IEMNATIVE_FRAME_STACK_ARG_COUNT   2
# endif
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
# ifdef RT_OS_WINDOWS
/** Frame pointer (RBP) relative offset of the third stack argument for calls. */
#  define IEMNATIVE_FP_OFF_STACK_ARG2       (IEMNATIVE_FP_OFF_STACK_ARG0 + 16)
/** Frame pointer (RBP) relative offset of the fourth stack argument for calls. */
#  define IEMNATIVE_FP_OFF_STACK_ARG3       (IEMNATIVE_FP_OFF_STACK_ARG0 + 24)
# endif

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
/** No alignment padding needed for arm64. */
# define IEMNATIVE_FRAME_ALIGN_SIZE         0
/** No stack argument slots, got 8 registers for arguments will suffice. */
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
 * The number of the register holding the pVCpu pointer.  */
/** @def IEMNATIVE_REG_FIXED_PCPUMCTX
 * The number of the register holding the &pVCpu->cpum.GstCtx pointer.
 * @note This not available on AMD64, only ARM64. */
/** @def IEMNATIVE_REG_FIXED_TMP0
 * Dedicated temporary register.
 * @todo replace this by a register allocator and content tracker.  */
/** @def IEMNATIVE_REG_FIXED_MASK
 * Mask GPRs with fixes assignments, either by us or dictated by the CPU/OS
 * architecture. */
#if defined(RT_ARCH_AMD64) && !defined(DOXYGEN_RUNNING)
# define IEMNATIVE_REG_FIXED_PVMCPU         X86_GREG_xBX
# define IEMNATIVE_REG_FIXED_TMP0           X86_GREG_x11
# define IEMNATIVE_REG_FIXED_MASK          (  RT_BIT_32(IEMNATIVE_REG_FIXED_PVMCPU) \
                                            | RT_BIT_32(IEMNATIVE_REG_FIXED_TMP0) \
                                            | RT_BIT_32(X86_GREG_xSP) \
                                            | RT_BIT_32(X86_GREG_xBP) )

#elif defined(RT_ARCH_ARM64) || defined(DOXYGEN_RUNNING)
# define IEMNATIVE_REG_FIXED_PVMCPU         ARMV8_A64_REG_X28
# define IEMNATIVE_REG_FIXED_PCPUMCTX       ARMV8_A64_REG_X27
# define IEMNATIVE_REG_FIXED_TMP0           ARMV8_A64_REG_X15
# define IEMNATIVE_REG_FIXED_MASK           (  RT_BIT_32(ARMV8_A64_REG_SP) \
                                             | RT_BIT_32(ARMV8_A64_REG_LR) \
                                             | RT_BIT_32(ARMV8_A64_REG_BP) \
                                             | RT_BIT_32(IEMNATIVE_REG_FIXED_PVMCPU) \
                                             | RT_BIT_32(IEMNATIVE_REG_FIXED_PCPUMCTX) \
                                             | RT_BIT_32(ARMV8_A64_REG_X18) \
                                             | RT_BIT_32(IEMNATIVE_REG_FIXED_TMP0) )

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
/** @def IEMNATIVE_CALL_VOLATILE_GREG_MASK
 * Mask of registers the callee will not save and may trash. */
#ifdef RT_ARCH_AMD64
# define IEMNATIVE_CALL_RET_GREG             X86_GREG_xAX

# ifdef RT_OS_WINDOWS
#  define IEMNATIVE_CALL_ARG_GREG_COUNT     4
#  define IEMNATIVE_CALL_ARG0_GREG          X86_GREG_xCX
#  define IEMNATIVE_CALL_ARG1_GREG          X86_GREG_xDX
#  define IEMNATIVE_CALL_ARG2_GREG          X86_GREG_x8
#  define IEMNATIVE_CALL_ARG3_GREG          X86_GREG_x9
#  define IEMNATIVE_CALL_VOLATILE_GREG_MASK (  RT_BIT_32(X86_GREG_xAX) \
                                             | RT_BIT_32(X86_GREG_xCX) \
                                             | RT_BIT_32(X86_GREG_xDX) \
                                             | RT_BIT_32(X86_GREG_x8) \
                                             | RT_BIT_32(X86_GREG_x9) \
                                             | RT_BIT_32(X86_GREG_x10) \
                                             | RT_BIT_32(X86_GREG_x11) )
# else
#  define IEMNATIVE_CALL_ARG_GREG_COUNT     6
#  define IEMNATIVE_CALL_ARG0_GREG          X86_GREG_xDI
#  define IEMNATIVE_CALL_ARG1_GREG          X86_GREG_xSI
#  define IEMNATIVE_CALL_ARG2_GREG          X86_GREG_xDX
#  define IEMNATIVE_CALL_ARG3_GREG          X86_GREG_xCX
#  define IEMNATIVE_CALL_ARG4_GREG          X86_GREG_x8
#  define IEMNATIVE_CALL_ARG5_GREG          X86_GREG_x9
#  define IEMNATIVE_CALL_VOLATILE_GREG_MASK (  RT_BIT_32(X86_GREG_xAX) \
                                             | RT_BIT_32(X86_GREG_xCX) \
                                             | RT_BIT_32(X86_GREG_xDX) \
                                             | RT_BIT_32(X86_GREG_xDI) \
                                             | RT_BIT_32(X86_GREG_xSI) \
                                             | RT_BIT_32(X86_GREG_x8) \
                                             | RT_BIT_32(X86_GREG_x9) \
                                             | RT_BIT_32(X86_GREG_x10) \
                                             | RT_BIT_32(X86_GREG_x11) )
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
# define IEMNATIVE_CALL_VOLATILE_GREG_MASK  (  RT_BIT_32(ARMV8_A64_REG_X0) \
                                             | RT_BIT_32(ARMV8_A64_REG_X1) \
                                             | RT_BIT_32(ARMV8_A64_REG_X2) \
                                             | RT_BIT_32(ARMV8_A64_REG_X3) \
                                             | RT_BIT_32(ARMV8_A64_REG_X4) \
                                             | RT_BIT_32(ARMV8_A64_REG_X5) \
                                             | RT_BIT_32(ARMV8_A64_REG_X6) \
                                             | RT_BIT_32(ARMV8_A64_REG_X7) \
                                             | RT_BIT_32(ARMV8_A64_REG_X8) \
                                             | RT_BIT_32(ARMV8_A64_REG_X9) \
                                             | RT_BIT_32(ARMV8_A64_REG_X10) \
                                             | RT_BIT_32(ARMV8_A64_REG_X11) \
                                             | RT_BIT_32(ARMV8_A64_REG_X12) \
                                             | RT_BIT_32(ARMV8_A64_REG_X13) \
                                             | RT_BIT_32(ARMV8_A64_REG_X14) \
                                             | RT_BIT_32(ARMV8_A64_REG_X15) \
                                             | RT_BIT_32(ARMV8_A64_REG_X16) \
                                             | RT_BIT_32(ARMV8_A64_REG_X17) )

#endif

/** This is the maximum argument count we'll ever be needing. */
#define IEMNATIVE_CALL_MAX_ARG_COUNT        7
/** @} */


/** @def IEMNATIVE_HST_GREG_COUNT
 * Number of host general purpose registers we tracker. */
/** @def IEMNATIVE_HST_GREG_MASK
 * Mask corresponding to IEMNATIVE_HST_GREG_COUNT that can be applied to
 * inverted register masks and such to get down to a correct set of regs. */
#ifdef RT_ARCH_AMD64
# define IEMNATIVE_HST_GREG_COUNT           16
# define IEMNATIVE_HST_GREG_MASK            UINT32_C(0xffff)

#elif defined(RT_ARCH_ARM64)
# define IEMNATIVE_HST_GREG_COUNT           32
# define IEMNATIVE_HST_GREG_MASK            UINT32_MAX
#else
# error "Port me!"
#endif


/** Native code generator label types. */
typedef enum
{
    kIemNativeLabelType_Invalid = 0,
    /* Labels w/o data, only once instance per TB: */
    kIemNativeLabelType_Return,
    kIemNativeLabelType_ReturnBreak,
    kIemNativeLabelType_ReturnWithFlags,
    kIemNativeLabelType_NonZeroRetOrPassUp,
    kIemNativeLabelType_RaiseGp0,
    /* Labels with data, potentially multiple instances per TB: */
    kIemNativeLabelType_If,
    kIemNativeLabelType_Else,
    kIemNativeLabelType_Endif,
    kIemNativeLabelType_CheckIrq,
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
    /** ARM64 fixup: PC relative offset at bits 25:0 (B, BL).  */
    kIemNativeFixupType_RelImm26At0,
    /** ARM64 fixup: PC relative offset at bits 23:5 (CBZ, CBNZ, B.CC).  */
    kIemNativeFixupType_RelImm19At5,
    /** ARM64 fixup: PC relative offset at bits 18:5 (TBZ, TBNZ).  */
    kIemNativeFixupType_RelImm14At5,
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
 * Guest registers that can be shadowed in GPRs.
 */
typedef enum IEMNATIVEGSTREG : uint8_t
{
    kIemNativeGstReg_GprFirst      = 0,
    kIemNativeGstReg_GprLast       = kIemNativeGstReg_GprFirst + 15,
    kIemNativeGstReg_Pc,
    kIemNativeGstReg_EFlags,            /**< 32-bit, includes internal flags.  */
    kIemNativeGstReg_SegSelFirst,
    kIemNativeGstReg_SegSelLast    = kIemNativeGstReg_SegSelFirst + 5,
    kIemNativeGstReg_SegBaseFirst,
    kIemNativeGstReg_SegBaseLast   = kIemNativeGstReg_SegBaseFirst + 5,
    kIemNativeGstReg_SegLimitFirst,
    kIemNativeGstReg_SegLimitLast  = kIemNativeGstReg_SegLimitFirst + 5,
    kIemNativeGstReg_End
} IEMNATIVEGSTREG;

/** @name Helpers for converting register numbers to IEMNATIVEGSTREG values.
 * @{  */
#define IEMNATIVEGSTREG_GPR(a_iGpr)             ((IEMNATIVEGSTREG)(kIemNativeGstReg_GprFirst      + (a_iGpr)    ))
#define IEMNATIVEGSTREG_SEG_SEL(a_iSegReg)      ((IEMNATIVEGSTREG)(kIemNativeGstReg_SegSelFirst   + (a_iSegReg) ))
#define IEMNATIVEGSTREG_SEG_BASE(a_iSegReg)     ((IEMNATIVEGSTREG)(kIemNativeGstReg_SegBaseFirst  + (a_iSegReg) ))
#define IEMNATIVEGSTREG_SEG_LIMIT(a_iSegReg)    ((IEMNATIVEGSTREG)(kIemNativeGstReg_SegLimitFirst + (a_iSegReg) ))
/** @} */

/**
 * Intended use statement for iemNativeRegAllocTmpForGuestReg().
 */
typedef enum IEMNATIVEGSTREGUSE
{
    /** The usage is read-only, the register holding the guest register
     * shadow copy will not be modified by the caller. */
    kIemNativeGstRegUse_ReadOnly = 0,
    /** The caller will update the guest register (think: PC += cbInstr).
     * The guest shadow copy will follow the returned register. */
    kIemNativeGstRegUse_ForUpdate,
    /** The call will put an entirely new value in the guest register, so
     * if new register is allocate it will be returned uninitialized. */
    kIemNativeGstRegUse_ForFullWrite,
    /** The caller will use the guest register value as input in a calculation
     * and the host register will be modified.
     * This means that the returned host register will not be marked as a shadow
     * copy of the guest register. */
    kIemNativeGstRegUse_Calculation
} IEMNATIVEGSTREGUSE;

/**
 * Guest registers (classes) that can be referenced.
 */
typedef enum IEMNATIVEGSTREGREF : uint8_t
{
    kIemNativeGstRegRef_Invalid = 0,
    kIemNativeGstRegRef_Gpr,
    kIemNativeGstRegRef_GprHighByte,    /**< AH, CH, DH, BH*/
    kIemNativeGstRegRef_EFlags,
    kIemNativeGstRegRef_MxCsr,
    kIemNativeGstRegRef_FpuReg,
    kIemNativeGstRegRef_MReg,
    kIemNativeGstRegRef_XReg,
    //kIemNativeGstRegRef_YReg, - doesn't work.
    kIemNativeGstRegRef_End
} IEMNATIVEGSTREGREF;


/** Variable kinds. */
typedef enum IEMNATIVEVARKIND : uint8_t
{
    /** Customary invalid zero value. */
    kIemNativeVarKind_Invalid = 0,
    /** This is either in a register or on the stack. */
    kIemNativeVarKind_Stack,
    /** Immediate value - loaded into register when needed, or can live on the
     *  stack if referenced (in theory). */
    kIemNativeVarKind_Immediate,
    /** Variable reference - loaded into register when needed, never stack. */
    kIemNativeVarKind_VarRef,
    /** Guest register reference - loaded into register when needed, never stack. */
    kIemNativeVarKind_GstRegRef,
    /** End of valid values. */
    kIemNativeVarKind_End
} IEMNATIVEVARKIND;


/** Variable or argument. */
typedef struct IEMNATIVEVAR
{
    /** The kind of variable. */
    IEMNATIVEVARKIND    enmKind;
    /** The variable size in bytes. */
    uint8_t             cbVar;
    /** The first stack slot (uint64_t), except for immediate and references
     *  where it usually is UINT8_MAX. */
    uint8_t             idxStackSlot;
    /** The host register allocated for the variable, UINT8_MAX if not. */
    uint8_t             idxReg;
    /** The argument number if argument, UINT8_MAX if regular variable. */
    uint8_t             uArgNo;
    /** If referenced, the index of the variable referencing this one, otherwise
     *  UINT8_MAX.  A referenced variable must only be placed on the stack and
     *  must be either kIemNativeVarKind_Stack or kIemNativeVarKind_Immediate. */
    uint8_t             idxReferrerVar;
    /** Guest register being shadowed here, kIemNativeGstReg_End(/UINT8_MAX) if not.
     * @todo not sure what this really is for...   */
    IEMNATIVEGSTREG     enmGstReg;
    uint8_t             bAlign;

    union
    {
        /** kIemNativeVarKind_Immediate: The immediate value. */
        uint64_t            uValue;
        /** kIemNativeVarKind_VarRef: The index of the variable being referenced. */
        uint8_t             idxRefVar;
        /** kIemNativeVarKind_GstRegRef: The guest register being referrenced. */
        struct
        {
            /** The class of register. */
            IEMNATIVEGSTREGREF  enmClass;
            /** Index within the class. */
            uint8_t             idx;
        } GstRegRef;
    } u;
} IEMNATIVEVAR;

/** What is being kept in a host register. */
typedef enum IEMNATIVEWHAT : uint8_t
{
    /** The traditional invalid zero value. */
    kIemNativeWhat_Invalid = 0,
    /** Mapping a variable (IEMNATIVEHSTREG::idxVar). */
    kIemNativeWhat_Var,
    /** Temporary register, this is typically freed when a MC completes. */
    kIemNativeWhat_Tmp,
    /** Call argument w/o a variable mapping.  This is free (via
     * IEMNATIVE_CALL_VOLATILE_GREG_MASK) after the call is emitted. */
    kIemNativeWhat_Arg,
    /** Return status code.
     * @todo not sure if we need this... */
    kIemNativeWhat_rc,
    /** The fixed pVCpu (PVMCPUCC) register.
     * @todo consider offsetting this on amd64 to use negative offsets to access
     *       more members using 8-byte disp. */
    kIemNativeWhat_pVCpuFixed,
    /** The fixed pCtx (PCPUMCTX) register.
     * @todo consider offsetting this on amd64 to use negative offsets to access
     *       more members using 8-byte disp. */
    kIemNativeWhat_pCtxFixed,
    /** Fixed temporary register. */
    kIemNativeWhat_FixedTmp,
    /** Register reserved by the CPU or OS architecture. */
    kIemNativeWhat_FixedReserved,
    /** End of valid values. */
    kIemNativeWhat_End
} IEMNATIVEWHAT;

/**
 * Host general register entry.
 *
 * The actual allocation status is kept in IEMRECOMPILERSTATE::bmHstRegs.
 *
 * @todo Track immediate values in host registers similarlly to how we track the
 *       guest register shadow copies. For it to be real helpful, though,
 *       we probably need to know which will be reused and put them into
 *       non-volatile registers, otherwise it's going to be more or less
 *       restricted to an instruction or two.
 */
typedef struct IEMNATIVEHSTREG
{
    /** Set of guest registers this one shadows.
     *
     * Using a bitmap here so we can designate the same host register as a copy
     * for more than one guest register.  This is expected to be useful in
     * situations where one value is copied to several registers in a sequence.
     * If the mapping is 1:1, then we'd have to pick which side of a 'MOV SRC,DST'
     * sequence we'd want to let this register follow to be a copy of and there
     * will always be places where we'd be picking the wrong one.
     */
    uint64_t        fGstRegShadows;
    /** What is being kept in this register. */
    IEMNATIVEWHAT   enmWhat;
    /** Variable index if holding a variable, otherwise UINT8_MAX. */
    uint8_t         idxVar;
    /** Alignment padding. */
    uint8_t         abAlign[6];
} IEMNATIVEHSTREG;


/**
 * Core state for the native recompiler, that is, things that needs careful
 * handling when dealing with branches.
 */
typedef struct IEMNATIVECORESTATE
{
    /** Allocation bitmap for aHstRegs. */
    uint32_t                    bmHstRegs;

    /** Bitmap marking which host register contains guest register shadow copies.
     * This is used during register allocation to try preserve copies.  */
    uint32_t                    bmHstRegsWithGstShadow;
    /** Bitmap marking valid entries in aidxGstRegShadows. */
    uint64_t                    bmGstRegShadows;

    union
    {
        /** Index of variable arguments, UINT8_MAX if not valid. */
        uint8_t                 aidxArgVars[8];
        /** For more efficient resetting. */
        uint64_t                u64ArgVars;
    };

    /** Allocation bitmap for the stack. */
    uint32_t                    bmStack;
    /** Allocation bitmap for aVars. */
    uint32_t                    bmVars;

    /** Maps a guest register to a host GPR (index by IEMNATIVEGSTREG).
     * Entries are only valid if the corresponding bit in bmGstRegShadows is set.
     * (A shadow copy of a guest register can only be held in a one host register,
     * there are no duplicate copies or ambiguities like that). */
    uint8_t                     aidxGstRegShadows[kIemNativeGstReg_End];

    /** Host register allocation tracking. */
    IEMNATIVEHSTREG             aHstRegs[IEMNATIVE_HST_GREG_COUNT];

    /** Variables and arguments. */
    IEMNATIVEVAR                aVars[9];
} IEMNATIVECORESTATE;
/** Pointer to core state. */
typedef IEMNATIVECORESTATE *PIEMNATIVECORESTATE;
/** Pointer to const core state. */
typedef IEMNATIVECORESTATE const *PCIEMNATIVECORESTATE;


/**
 * Conditional stack entry.
 */
typedef struct IEMNATIVECOND
{
    /** Set if we're in the "else" part, clear if we're in the "if" before it. */
    bool                        fInElse;
    /** The label for the IEM_MC_ELSE. */
    uint32_t                    idxLabelElse;
    /** The label for the IEM_MC_ENDIF. */
    uint32_t                    idxLabelEndIf;
    /** The initial state snapshot as the if-block starts executing. */
    IEMNATIVECORESTATE          InitialState;
    /** The state snapshot at the end of the if-block. */
    IEMNATIVECORESTATE          IfFinalState;
} IEMNATIVECOND;
/** Pointer to a condition stack entry. */
typedef IEMNATIVECOND *PIEMNATIVECOND;


/**
 * Native recompiler state.
 */
typedef struct IEMRECOMPILERSTATE
{
    /** Size of the buffer that pbNativeRecompileBufR3 points to in
     * IEMNATIVEINSTR units. */
    uint32_t                    cInstrBufAlloc;
#ifdef VBOX_STRICT
    /** Strict: How far the last iemNativeInstrBufEnsure() checked. */
    uint32_t                    offInstrBufChecked;
#else
    uint32_t                    uPadding1; /* We don't keep track of the size here... */
#endif
    /** Fixed temporary code buffer for native recompilation. */
    PIEMNATIVEINSTR             pInstrBuf;

    /** Bitmaps with the label types used. */
    uint64_t                    bmLabelTypes;
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

#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    /** Number of debug info entries allocated for pDbgInfo. */
    uint32_t                    cDbgInfoAlloc;
    uint32_t                    uPadding;
    /** Debug info. */
    PIEMTBDBG                   pDbgInfo;
#endif

    /** The translation block being recompiled. */
    PCIEMTB                     pTbOrg;

    /** The current condition stack depth (aCondStack). */
    uint8_t                     cCondDepth;
    uint8_t                     bPadding2;
    /** Condition sequence number (for generating unique labels). */
    uint16_t                    uCondSeqNo;
    /** Check IRQ seqeunce number (for generating unique lables). */
    uint16_t                    uCheckIrqSeqNo;
    uint8_t                     bPadding3;

    /** The argument count + hidden regs from the IEM_MC_BEGIN statement. */
    uint8_t                     cArgs;
    /** The IEM_CIMPL_F_XXX flags from the IEM_MC_BEGIN statement. */
    uint32_t                    fCImpl;
    /** The IEM_MC_F_XXX flags from the IEM_MC_BEGIN statement. */
    uint32_t                    fMc;

    /** Core state requiring care with branches. */
    IEMNATIVECORESTATE          Core;

    /** The condition nesting stack. */
    IEMNATIVECOND               aCondStack[2];

#ifndef IEM_WITH_THROW_CATCH
    /** Pointer to the setjmp/longjmp buffer if we're not using C++ exceptions
     *  for recompilation error handling. */
    jmp_buf                     JmpBuf;
#endif
} IEMRECOMPILERSTATE;
/** Pointer to a native recompiler state. */
typedef IEMRECOMPILERSTATE *PIEMRECOMPILERSTATE;


/** @def IEMNATIVE_TRY_SETJMP
 * Wrapper around setjmp / try, hiding all the ugly differences.
 *
 * @note Use with extreme care as this is a fragile macro.
 * @param   a_pReNative The native recompile state.
 * @param   a_rcTarget  The variable that should receive the status code in case
 *                      of a longjmp/throw.
 */
/** @def IEMNATIVE_CATCH_LONGJMP_BEGIN
 * Start wrapper for catch / setjmp-else.
 *
 * This will set up a scope.
 *
 * @note Use with extreme care as this is a fragile macro.
 * @param   a_pReNative The native recompile state.
 * @param   a_rcTarget  The variable that should receive the status code in case
 *                      of a longjmp/throw.
 */
/** @def IEMNATIVE_CATCH_LONGJMP_END
 * End wrapper for catch / setjmp-else.
 *
 * This will close the scope set up by IEMNATIVE_CATCH_LONGJMP_BEGIN and clean
 * up the state.
 *
 * @note Use with extreme care as this is a fragile macro.
 * @param   a_pReNative The native recompile state.
 */
/** @def IEMNATIVE_DO_LONGJMP
 *
 * Wrapper around longjmp / throw.
 *
 * @param   a_pReNative The native recompile state.
 * @param   a_rc        The status code jump back with / throw.
 */
#ifdef IEM_WITH_THROW_CATCH
# define IEMNATIVE_TRY_SETJMP(a_pReNative, a_rcTarget) \
       a_rcTarget = VINF_SUCCESS; \
       try
# define IEMNATIVE_CATCH_LONGJMP_BEGIN(a_pReNative, a_rcTarget) \
       catch (int rcThrown) \
       { \
           a_rcTarget = rcThrown
# define IEMNATIVE_CATCH_LONGJMP_END(a_pReNative) \
       } \
       ((void)0)
# define IEMNATIVE_DO_LONGJMP(a_pReNative, a_rc)  throw int(a_rc)
#else  /* !IEM_WITH_THROW_CATCH */
# define IEMNATIVE_TRY_SETJMP(a_pReNative, a_rcTarget) \
       if ((a_rcTarget = setjmp((a_pReNative)->JmpBuf)) == 0)
# define IEMNATIVE_CATCH_LONGJMP_BEGIN(a_pReNative, a_rcTarget) \
       else \
       { \
           ((void)0)
# define IEMNATIVE_CATCH_LONGJMP_END(a_pReNative) \
       }
# define IEMNATIVE_DO_LONGJMP(a_pReNative, a_rc)  longjmp((a_pReNative)->JmpBuf, (a_rc))
#endif /* !IEM_WITH_THROW_CATCH */


/**
 * Native recompiler worker for a threaded function.
 *
 * @returns New code buffer offset; throws VBox status code in case of a failure.
 * @param   pReNative   The native recompiler state.
 * @param   off         The current code buffer offset.
 * @param   pCallEntry  The threaded call entry.
 *
 * @note    This may throw/longjmp VBox status codes (int) to abort compilation, so no RT_NOEXCEPT!
 */
typedef uint32_t (VBOXCALL FNIEMNATIVERECOMPFUNC)(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry);
/** Pointer to a native recompiler worker for a threaded function. */
typedef FNIEMNATIVERECOMPFUNC *PFNIEMNATIVERECOMPFUNC;

/** Defines a native recompiler worker for a threaded function.
 * @see FNIEMNATIVERECOMPFUNC  */
#define IEM_DECL_IEMNATIVERECOMPFUNC_DEF(a_Name) \
    uint32_t VBOXCALL a_Name(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)

/** Prototypes a native recompiler function for a threaded function.
 * @see FNIEMNATIVERECOMPFUNC  */
#define IEM_DECL_IEMNATIVERECOMPFUNC_PROTO(a_Name) FNIEMNATIVERECOMPFUNC a_Name

DECL_HIDDEN_THROW(uint32_t) iemNativeLabelCreate(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                                                 uint32_t offWhere = UINT32_MAX, uint16_t uData = 0);
DECL_HIDDEN_THROW(void)     iemNativeLabelDefine(PIEMRECOMPILERSTATE pReNative, uint32_t idxLabel, uint32_t offWhere);
DECL_HIDDEN_THROW(void)     iemNativeAddFixup(PIEMRECOMPILERSTATE pReNative, uint32_t offWhere, uint32_t idxLabel,
                                              IEMNATIVEFIXUPTYPE enmType, int8_t offAddend = 0);
DECL_HIDDEN_THROW(PIEMNATIVEINSTR) iemNativeInstrBufEnsureSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t cInstrReq);

DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAllocTmp(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, bool fPreferVolatile = true);
DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAllocTmpImm(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint64_t uImm,
                                                    bool fPreferVolatile = true);
DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAllocTmpForGuestReg(PIEMRECOMPILERSTATE pReNative, uint32_t *poff,
                                                            IEMNATIVEGSTREG enmGstReg, IEMNATIVEGSTREGUSE enmIntendedUse);
DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAllocTmpForGuestRegIfAlreadyPresent(PIEMRECOMPILERSTATE pReNative, uint32_t *poff,
                                                                            IEMNATIVEGSTREG enmGstReg);

DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAllocVar(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint8_t idxVar);
DECL_HIDDEN_THROW(uint32_t) iemNativeRegAllocArgs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs);
DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAssignRc(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg);
DECLHIDDEN(void)            iemNativeRegFree(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFreeTmp(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFreeTmpImm(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFreeAndFlushMask(PIEMRECOMPILERSTATE pReNative, uint32_t fHstRegMask) RT_NOEXCEPT;
DECL_HIDDEN_THROW(uint32_t) iemNativeRegFlushPendingWrites(PIEMRECOMPILERSTATE pReNative, uint32_t off);

DECL_HIDDEN_THROW(uint32_t) iemNativeEmitLoadGprWithGstShadowReg(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                 uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg);
DECL_HIDDEN_THROW(uint32_t) iemNativeEmitCheckCallRetAndPassUp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr);

extern DECL_HIDDEN_DATA(const char * const) g_apszIemNativeHstRegNames[];


/**
 * Ensures that there is sufficient space in the instruction output buffer.
 *
 * This will reallocate the buffer if needed and allowed.
 *
 * @note    Always use IEMNATIVE_ASSERT_INSTR_BUF_ENSURE when done to check the
 *          allocation size.
 *
 * @returns Pointer to the instruction output buffer on success; throws VBox
 *          status code on failure, so no need to check it.
 * @param   pReNative   The native recompile state.
 * @param   off         Current instruction offset.  Works safely for UINT32_MAX
 *                      as well.
 * @param   cInstrReq   Number of instruction about to be added.  It's okay to
 *                      overestimate this a bit.
 */
DECL_FORCE_INLINE_THROW(PIEMNATIVEINSTR)
iemNativeInstrBufEnsure(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t cInstrReq)
{
    uint64_t const offChecked = off + (uint64_t)cInstrReq; /** @todo may reconsider the need for UINT32_MAX safety... */
    if (RT_LIKELY(offChecked <= pReNative->cInstrBufAlloc))
    {
#ifdef VBOX_STRICT
        pReNative->offInstrBufChecked = offChecked;
#endif
        return pReNative->pInstrBuf;
    }
    return iemNativeInstrBufEnsureSlow(pReNative, off, cInstrReq);
}

/**
 * Checks that we didn't exceed the space requested in the last
 * iemNativeInstrBufEnsure() call.
 */
#define IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(a_pReNative, a_off) \
    AssertMsg((a_off) <= (a_pReNative)->offInstrBufChecked, \
              ("off=%#x offInstrBufChecked=%#x\n", (a_off), (a_pReNative)->offInstrBufChecked))

/**
 * Checks that a variable index is valid.
 */
#define IEMNATIVE_ASSERT_VAR_IDX(a_pReNative, a_idxVar) \
    AssertMsg(   (unsigned)(a_idxVar) < RT_ELEMENTS((a_pReNative)->Core.aVars) \
              && ((a_pReNative)->Core.bmVars & RT_BIT_32(a_idxVar)), ("%s=%d\n", #a_idxVar, a_idxVar))

/**
 * Checks that a variable index is valid and that the variable is assigned the
 * correct argument number.
 * This also adds a RT_NOREF of a_idxVar.
 */
#define IEMNATIVE_ASSERT_ARG_VAR_IDX(a_pReNative, a_idxVar, a_uArgNo) do { \
        RT_NOREF(a_idxVar); \
        AssertMsg(   (unsigned)(a_idxVar) < RT_ELEMENTS((a_pReNative)->Core.aVars) \
                  && ((a_pReNative)->Core.bmVars & RT_BIT_32(a_idxVar))\
                  && (a_pReNative)->Core.aVars[a_idxVar].uArgNo == (a_uArgNo) \
                  , ("%s=%d; uArgNo=%d, expected %u\n", #a_idxVar, a_idxVar, \
                     (a_pReNative)->Core.aVars[RT_MAX(a_idxVar, RT_ELEMENTS((a_pReNative)->Core.aVars)) - 1].uArgNo, a_uArgNo)); \
    } while (0)

/**
 * Calculates the stack address of a variable as a [r]BP displacement value.
 */
DECL_FORCE_INLINE(int32_t)
iemNativeStackCalcBpDisp(uint8_t idxStackSlot)
{
    Assert(idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS);
    return idxStackSlot * sizeof(uint64_t) + IEMNATIVE_FP_OFF_STACK_VARS;
}

/**
 * Calculates the stack address of a variable as a [r]BP displacement value.
 */
DECL_FORCE_INLINE(int32_t)
iemNativeVarCalcBpDisp(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    return iemNativeStackCalcBpDisp(pReNative->Core.aVars[idxVar].idxStackSlot);
}

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_IEMN8veRecompiler_h */

