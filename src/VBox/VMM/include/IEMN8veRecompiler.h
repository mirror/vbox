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
 * @note This limit is duplicated in the python script(s). */
#define IEMNATIVE_FRAME_VAR_SIZE            0xc0
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
    /** ARM64 fixup: PC relative offset at bits 23:5 (CBZ, CBNZ, B, B.CC).  */
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
    kIemNativeGstReg_GprLast       = 15,
    kIemNativeGstReg_Pc,
    kIemNativeGstReg_EFlags,            /**< This one is problematic since the higher bits are used internally. */
    /* gap: 18..23 */
    kIemNativeGstReg_SegSelFirst   = 24,
    kIemNativeGstReg_SegSelLast    = 29,
    kIemNativeGstReg_SegBaseFirst  = 30,
    kIemNativeGstReg_SegBaseLast   = 35,
    kIemNativeGstReg_SegLimitFirst = 36,
    kIemNativeGstReg_SegLimitLast  = 41,
    kIemNativeGstReg_End
} IEMNATIVEGSTREG;

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
    kIemNativeGstRegRef_YReg,
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
    /** Guest register being shadowed here, kIemNativeGstReg_End(/UINT8_MAX) if not. */
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
    uint16_t                    uPadding3;

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
 * iemNativeInstrBufEnsure() call. */
#define IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(a_pReNative, a_off) \
    AssertMsg((a_off) <= (a_pReNative)->offInstrBufChecked, \
              ("off=%#x offInstrBufChecked=%#x\n", (a_off), (a_pReNative)->offInstrBufChecked))


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
#elif RT_ARCH_ARM64
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


/*********************************************************************************************************************************
*   Loads, Stores and Related Stuff.                                                                                             *
*********************************************************************************************************************************/

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

#elif RT_ARCH_ARM64
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
 * Emits loading a constant into a 64-bit GPR
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprImm64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint64_t uImm64)
{
    if (!uImm64)
        return iemNativeEmitGprZero(pReNative, off, iGpr);

#ifdef RT_ARCH_AMD64
    if (uImm64 <= UINT32_MAX)
    {
        /* mov gpr, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
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

#elif RT_ARCH_ARM64
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
#elif RT_ARCH_ARM64
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
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGpr, IEMNATIVE_REG_FIXED_PVMCPU, offVCpu / cbData);
    }
    else if (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx) < (unsigned)(_4K * cbData) && !(offVCpu & (cbData - 1)))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGpr, IEMNATIVE_REG_FIXED_PCPUMCTX,
                                                      (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx)) / cbData);
    }
    else
    {
        /* The offset is too large, so we must load it into a register and use
           ldr Wt, [<Xn|SP>, (<Wm>|<Xm>)]. */
        /** @todo reduce by offVCpu by >> 3 or >> 2? if it saves instructions? */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, offVCpu);

        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGpr, IEMNATIVE_REG_FIXED_PVMCPU, IEMNATIVE_REG_FIXED_TMP);
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
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

#elif RT_ARCH_ARM64
    off = iemNativeEmitGprByVCpuLdSt(pReNative, off, iGpr, offVCpu, kArmv8A64InstrLdStType_St_Byte, sizeof(uint8_t));

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

#elif RT_ARCH_ARM64
    /* mov dst, src;   alias for: orr dst, xzr, src */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = UINT32_C(0xaa000000) | ((uint32_t)iGprSrc << 16) | ((uint32_t)ARMV8_A64_REG_XZR << 5) | iGprDst;

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
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
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GRP load instruction with an BP relative source address.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
    /* mov gprdst, qword [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a 32-bit GRP load instruction with an BP relative source address.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLoadGprByBpU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
    /* mov gprdst, dword [rbp + offDisp]  */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);
}
#endif


#ifdef RT_ARCH_AMD64
/**
 * Emits a load effective address to a GRP with an BP relative source address.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLeaGprByBp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t offDisp)
{
    /* lea gprdst, [rbp + offDisp] */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (iGprDst < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8d;
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);
}
#endif


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
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGprReg & 7, iGprBase & 7);
        if ((iGprBase & 7) == X86_GREG_xSP) /* for RSP/R12 relative addressing we have to use a SIB byte. */
            pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_xSP, X86_GREG_xSP, 0); /* -> [RSP/R12] */
        pbCodeBuf[off++] = RT_BYTE1((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE2((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE3((uint32_t)offDisp);
        pbCodeBuf[off++] = RT_BYTE4((uint32_t)offDisp);
    }
    return off;
}
#elif RT_ARCH_ARM64
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
        uint8_t const idxTmpReg = iemNativeRegAllocTmpImm(pReNative, off, (uint64)offDisp);

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

#elif RT_ARCH_ARM64
    off = iemNativeEmitGprByGprLdSt(pReNative, off, iGprDst, offDisp, kArmv8A64InstrLdStType_Ld_Dword, sizeof(uint64_t));

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

#elif RT_ARCH_ARM64
    off = iemNativeEmitGprByGprLdSt(pReNative, off, iGprDst, offDisp, kArmv8A64InstrLdStType_Ld_Word, sizeof(uint32_t));

#else
# error "port me"
#endif
    return off;
}


/*********************************************************************************************************************************
*   Subtraction and Additions                                                                                                    *
*********************************************************************************************************************************/


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
 * Emits adding a 64-bit GPR to another, storing the result in the frist.
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
*   Bit Operations                                                                                                               *
*********************************************************************************************************************************/

/**
 * Emits code for clearing bits 16 thru 63 in the GPR.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitClear16UpGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    /* movzx reg32, reg16 */
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
iemNativeEmitAndGpr32ByGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x23;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAnd(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);

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
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R);
        pbCodeBuf[off++] = 0x83;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 4, iGprDst & 7);
        pbCodeBuf[off++] = (uint8_t)uImm;
    }
    else if ((int64_t)uImm == (int32_t)uImm)
    {
        /* and Ev, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R);
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
    if (Armv8A64ConvertMaskToImmRImmS(uImm, &uImmNandS, &uImmR))
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
        pbCodeBuf[off++] = X86_OP_REX_R;
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
    if (Armv8A64ConvertMaskToImmRImmS(uImm, &uImmNandS, &uImmR))
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
        if (!fSetFlags)
            off = iemNativeEmitAndGpr32ByGpr32(pReNative, off, iGprDst, iTmpReg);
        else
            off = iemNativeEmitAndsGpr32ByGpr32(pReNative, off, iGprDst, iTmpReg);
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
        pu32CodeBuf[off++] = Armv8A64MkInstrB(pReNative->paLabels[idxReturnLabel].off - off);
    else
    {
        iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5);
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
 * Emits a Jcc rel32 / B.cc imm19 with a fixed displacement.
 * How @a offJmp is applied is are target specific.
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
    pu32CodeBuf[off++] = Armv8A64MkInstrBCond(enmCond, offTarget);

#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits a JZ/JE rel32 / B.EQ imm19 with a fixed displacement.
 * How @a offJmp is applied is are target specific.
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
 * How @a offJmp is applied is are target specific.
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
 * How @a offJmp is applied is are target specific.
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
 * Emits a JA/JNBE rel32 / B.EQ imm19 with a fixed displacement.
 * How @a offJmp is applied is are target specific.
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
    Assert(RT_ABS((int32_t)(offTarget - offFixup)) < RT_BIT_32(18)); /* off by one for negative jumps, but not relevant here */
    pu32CodeBuf[offFixup] = (pu32CodeBuf[offFixup] & ~((RT_BIT_32(19) - 1U) << 5))
                          | (((offTarget - offFixup) & (RT_BIT_32(19) - 1U)) << 5);

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
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 0, f64Bit);
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


/** @} */

#endif /* !VMM_INCLUDED_SRC_include_IEMN8veRecompiler_h */

