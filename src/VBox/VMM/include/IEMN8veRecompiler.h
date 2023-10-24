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
    kIemNativeLabelType_Return,
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    kIemNativeLabelType_If,
#endif
    kIemNativeLabelType_Else,
    kIemNativeLabelType_Endif,
    kIemNativeLabelType_NonZeroRetOrPassUp,
    kIemNativeLabelType_RaiseGp0,
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
    uint32_t                    uPadding3;

    /** Core state requiring care with branches. */
    IEMNATIVECORESTATE          Core;

    /** The condition nesting stack. */
    IEMNATIVECOND               aCondStack[2];
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


DECLHIDDEN(uint32_t)        iemNativeLabelCreate(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                                                 uint32_t offWhere = UINT32_MAX, uint16_t uData = 0) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeLabelDefine(PIEMRECOMPILERSTATE pReNative, uint32_t idxLabel, uint32_t offWhere) RT_NOEXCEPT;
DECLHIDDEN(bool)            iemNativeAddFixup(PIEMRECOMPILERSTATE pReNative, uint32_t offWhere, uint32_t idxLabel,
                                              IEMNATIVEFIXUPTYPE enmType, int8_t offAddend = 0) RT_NOEXCEPT;
DECLHIDDEN(PIEMNATIVEINSTR) iemNativeInstrBufEnsureSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                        uint32_t cInstrReq) RT_NOEXCEPT;

DECLHIDDEN(uint8_t)         iemNativeRegAllocTmp(PIEMRECOMPILERSTATE pReNative, uint32_t *poff,
                                                 bool fPreferVolatile = true) RT_NOEXCEPT;
DECLHIDDEN(uint8_t)         iemNativeRegAllocTmpImm(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint64_t uImm,
                                                    bool fPreferVolatile = true) RT_NOEXCEPT;
DECLHIDDEN(uint8_t)         iemNativeRegAllocTmpForGuest(PIEMRECOMPILERSTATE pReNative, uint32_t *poff,
                                                         IEMNATIVEGSTREG enmGstReg) RT_NOEXCEPT;
DECLHIDDEN(uint8_t)         iemNativeRegAllocVar(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint8_t idxVar) RT_NOEXCEPT;
DECLHIDDEN(uint32_t)        iemNativeRegAllocArgs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs) RT_NOEXCEPT;
DECLHIDDEN(uint8_t)         iemNativeRegAssignRc(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFree(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFreeTmp(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFreeTmpImm(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;
DECLHIDDEN(void)            iemNativeRegFreeAndFlushMask(PIEMRECOMPILERSTATE pReNative, uint32_t fHstRegMask) RT_NOEXCEPT;
DECLHIDDEN(uint32_t)        iemNativeRegFlushPendingWrites(PIEMRECOMPILERSTATE pReNative, uint32_t off) RT_NOEXCEPT;

DECLHIDDEN(uint32_t)        iemNativeEmitLoadGprWithGstShadowReg(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                 uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg) RT_NOEXCEPT;
DECLHIDDEN(uint32_t)        iemNativeEmitCheckCallRetAndPassUp(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                               uint8_t idxInstr) RT_NOEXCEPT;


/**
 * Ensures that there is sufficient space in the instruction output buffer.
 *
 * This will reallocate the buffer if needed and allowed.
 *
 * @note    Always use IEMNATIVE_ASSERT_INSTR_BUF_ENSURE when done to check the
 *          allocation size.
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
    uint64_t const offChecked = off + (uint64_t)cInstrReq;
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
DECLINLINE(uint32_t) iemNativeEmitMarker(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t uInfo)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    /* nop */
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
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
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
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits loading a constant into a 8-bit GPR
 * @note The AMD64 version does *NOT* clear any bits in the 8..63 range,
 *       only the ARM64 version does that.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGpr8Imm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint8_t uImm8)
{
#ifdef RT_ARCH_AMD64
    /* mov gpr, imm8 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    else if (iGpr >= 4)
        pbCodeBuf[off++] = X86_OP_REX;
    pbCodeBuf[off++] = 0xb0 + (iGpr & 7);
    pbCodeBuf[off++] = RT_BYTE1(uImm8);

#elif RT_ARCH_ARM64
    /* movz gpr, imm16, lsl #0 */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECL_FORCE_INLINE(uint32_t) iemNativeEmitGprByVCpuDisp(uint8_t *pbCodeBuf, uint32_t off, uint8_t iGprReg, uint32_t offVCpu)
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
DECL_FORCE_INLINE(uint32_t) iemNativeEmitGprByVCpuLdSt(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprReg,
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
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(enmOperation, iGpr, IEMNATIVE_REG_FIXED_PVMCPU, offVCpu / cbData);
    }
    else if (offVCpu - RT_UOFFSETOF(VMCPU, cpum.GstCtx) < (unsigned)(_4K * cbData) && !(offVCpu & (cbData - 1)))
    {
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
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
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRegIdx(enmOperation, iGpr, IEMNATIVE_REG_FIXED_PVMCPU, IEMNATIVE_REG_FIXED_TMP);
    }
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#endif


/**
 * Emits a 64-bit GPR load of a VCpu value.
 */
DECLINLINE(uint32_t) iemNativeEmitLoadGprFromVCpuU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov reg64, mem64 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGpr < 8)
        pbCodeBuf[off++] = X86_OP_REX_W;
    else
        pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf,off,iGpr, offVCpu);
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
DECLINLINE(uint32_t) iemNativeEmitLoadGprFromVCpuU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov reg32, mem32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitLoadGprFromVCpuU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* movzx reg32, mem16 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitLoadGprFromVCpuU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* movzx reg32, mem8 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitStoreGprToVCpuU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem64, reg64 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitStoreGprFromVCpuU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem32, reg32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitStoreGprFromVCpuU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem16, reg16 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitStoreGprFromVCpuU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    /* mov mem8, reg8 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);
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
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);
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
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprDst, offDisp, pReNative);
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
    return iemNativeEmitGprByBpDisp(pbCodeBuf, off, iGprSrc, offDisp, pReNative);

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
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        return off;
    }
#endif

    /* Load tmp0, imm64; Store tmp to bp+disp. */
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, uImm64);
    return iemNativeEmitStoreGprByBp(pReNative, off, offDisp, IEMNATIVE_REG_FIXED_TMP0);
}


/*********************************************************************************************************************************
*   Subtraction and Additions                                                                                                    *
*********************************************************************************************************************************/


#ifdef RT_ARCH_AMD64
/**
 * Emits a 64-bit GPR subtract with a signed immediate subtrahend.
 */
DECLINLINE(uint32_t) iemNativeEmitSubGprImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iSubtrahend)
{
    /* sub gprdst, imm8/imm32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitAddTwoGprs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprAddend)
{
#if defined(RT_ARCH_AMD64)
    /* add Gv,Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = (iGprDst < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R)
                     | (iGprAddend < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x04;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprAddend & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitAddGprImm8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    /* add or inc */
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitAddGpr32Imm8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int8_t iImm8)
{
#if defined(RT_ARCH_AMD64)
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    /* add or inc */
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitAddGprImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int64_t iAddend)
{
#if defined(RT_ARCH_AMD64)
    if (iAddend <= INT8_MAX && iAddend >= INT8_MIN)
        return iemNativeEmitAddGprImm8(pReNative, off, iGprDst, (int8_t)iAddend);

    if (iAddend <= INT32_MAX && iAddend >= INT32_MIN)
    {
        /* add grp, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        AssertReturn(pbCodeBuf, UINT32_MAX);
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
        AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->Core.aHstRegs), UINT32_MAX);

        /* add dst, tmpreg  */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
        AssertReturn(pbCodeBuf, UINT32_MAX);
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
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        if (iAddend >= 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint32_t)iAddend);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint32_t)-iAddend);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (uint64_t)iAddend);
        AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->Core.aHstRegs), UINT32_MAX);

        /* add gprdst, gprdst, tmpreg */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitAddGpr32Imm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, int32_t iAddend)
{
#if defined(RT_ARCH_AMD64)
    if (iAddend <= INT8_MAX && iAddend >= INT8_MIN)
        return iemNativeEmitAddGpr32Imm8(pReNative, off, iGprDst, (int8_t)iAddend);

    /* add grp, imm32 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        if (iAddend >= 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, iGprDst, iGprDst, (uint32_t)iAddend, false /*f64Bit*/);
        else
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, iGprDst, iGprDst, (uint32_t)-iAddend, false /*f64Bit*/);
    }
    else
    {
        /* Use temporary register for the immediate. */
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, (uint32_t)iAddend);
        AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->Core.aHstRegs), UINT32_MAX);

        /* add gprdst, gprdst, tmpreg */
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitClear16UpGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst)
{
#if defined(RT_ARCH_AMD64)
    /* movzx reg32, reg16 */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGprDst >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0xb7;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprDst & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
 */
DECLINLINE(uint32_t ) iemNativeEmitAndGprByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x23;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrAnd(iGprDst, iGprDst, iGprSrc);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for AND'ing two 32-bit GPRs.
 */
DECLINLINE(uint32_t ) iemNativeEmitAndGpr32ByGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x23;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrAnd(iGprDst, iGprDst, iGprSrc, false /*f64Bit*/);

#else
# error "Port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code for XOR'ing two 64-bit GPRs.
 */
DECLINLINE(uint32_t ) iemNativeEmitXorGprByGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = X86_OP_REX_W | (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitXorGpr32ByGpr32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#if defined(RT_ARCH_AMD64)
    /* and Gv, Ev */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGprDst >= 8 || iGprSrc >= 8)
        pbCodeBuf[off++] = (iGprDst < 8 ? 0 : X86_OP_REX_R) | (iGprSrc < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitShiftGprLeft(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* shl dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitShiftGpr32Left(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 32);

#if defined(RT_ARCH_AMD64)
    /* shl dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitShiftGprRight(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 64);

#if defined(RT_ARCH_AMD64)
    /* shr dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t ) iemNativeEmitShiftGpr32Right(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprDst, uint8_t cShift)
{
    Assert(cShift > 0 && cShift < 32);

#if defined(RT_ARCH_AMD64)
    /* shr dst, cShift */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitCmpArm64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight,
                                           bool f64Bit = true, uint32_t cShift = 0,
                                           ARMV8A64INSTRSHIFT enmShift = kArmv8A64InstrShift_Lsr)
{
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitCmpGprWithGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    /* cmp Gv, Ev */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitCmpGpr32WithGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                  uint8_t iGprLeft, uint8_t iGprRight)
{
#ifdef RT_ARCH_AMD64
    /* cmp Gv, Ev */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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



/*********************************************************************************************************************************
*   Branching                                                                                                                    *
*********************************************************************************************************************************/

/**
 * Emits a JMP rel32 / B imm19 to the given label.
 */
DECLINLINE(uint32_t) iemNativeEmitJmpToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
        AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4), UINT32_MAX);
        pbCodeBuf[off++] = 0xfe;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;
    }
    pbCodeBuf[off++] = 0xcc;                        /* int3 poison */

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    if (pReNative->paLabels[idxLabel].off != UINT32_MAX)
        pu32CodeBuf[off++] = Armv8A64MkInstrB(pReNative->paLabels[idxReturnLabel].off - off);
    else
    {
        AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5), UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitJmpToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                IEMNATIVELABELTYPE enmLabelType, uint16_t uData = 0)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    AssertReturn(idxLabel != UINT32_MAX, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitJccToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                             uint32_t idxLabel, IEMNATIVEINSTRCOND enmCond)
{
    Assert(idxLabel < pReNative->cLabels);

#ifdef RT_ARCH_AMD64
    /* jcc rel32 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = (uint8_t)enmCond | 0x80;
    AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4), UINT32_MAX);
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;

#elif defined(RT_ARCH_ARM64)
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5), UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitJccToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                IEMNATIVELABELTYPE enmLabelType, uint16_t uData, IEMNATIVEINSTRCOND enmCond)
{
    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, enmLabelType, UINT32_MAX /*offWhere*/, uData);
    AssertReturn(idxLabel != UINT32_MAX, UINT32_MAX);
    return iemNativeEmitJccToLabel(pReNative, off, idxLabel, enmCond);
}


/**
 * Emits a JZ/JE rel32 / B.EQ imm19 to the given label.
 */
DECLINLINE(uint32_t) iemNativeEmitJzToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
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
DECLINLINE(uint32_t) iemNativeEmitJzToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
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
DECLINLINE(uint32_t) iemNativeEmitJnzToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
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
DECLINLINE(uint32_t) iemNativeEmitJnzToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
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
DECLINLINE(uint32_t) iemNativeEmitJbeToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
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
DECLINLINE(uint32_t) iemNativeEmitJbeToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
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
DECLINLINE(uint32_t) iemNativeEmitJaToLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxLabel)
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
DECLINLINE(uint32_t) iemNativeEmitJaToNewLabel(PIEMRECOMPILERSTATE pReNative, uint32_t off,
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
DECLINLINE(uint32_t) iemNativeEmitJccToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                             int32_t offTarget, IEMNATIVEINSTRCOND enmCond)
{
#ifdef RT_ARCH_AMD64
    /* jcc rel32 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    AssertReturn(pbCodeBuf, UINT32_MAX);
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitJzToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
DECLINLINE(uint32_t) iemNativeEmitJnzToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
DECLINLINE(uint32_t) iemNativeEmitJbeToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
DECLINLINE(uint32_t) iemNativeEmitJaToFixed(PIEMRECOMPILERSTATE pReNative, uint32_t off, int32_t offTarget)
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
DECLINLINE(uint32_t) iemNativeEmitTestBitInGprAndJmpToLabelIfCc(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                uint8_t iGprSrc, uint8_t iBitNo, uint32_t idxLabel,
                                                                bool fJmpIfSet)
{
    Assert(iBitNo < 64);
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iBitNo < 8)
    {
        /* test Eb, imm8 */
        if (iGprSrc >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
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
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm14At5), UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitTestBitInGprAndJmpToLabelIfSet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                 uint8_t iGprSrc, uint8_t iBitNo, uint32_t idxLabel)
{
    return iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, iGprSrc, iBitNo, idxLabel, false /*fJmpIfSet*/);
}


/**
 * Emits a jump to @a idxLabel on the condition that bit @a iBitNo _is_ _not_
 * _set_ in @a iGprSrc.
 *
 * @note On ARM64 the range is only +/-8191 instructions.
 */
DECLINLINE(uint32_t) iemNativeEmitTestBitInGprAndJmpToLabelIfNotSet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                    uint8_t iGprSrc, uint8_t iBitNo, uint32_t idxLabel)
{
    return iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, iGprSrc, iBitNo, idxLabel, true /*fJmpIfSet*/);
}


/**
 * Emits a test for any of the bits from @a fBits in @a iGprSrc, setting CPU
 * flags accordingly.
 */
DECLINLINE(uint32_t) iemNativeEmitTestAnyBitsInGpr(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGprSrc, uint64_t fBits)
{
    Assert(fBits != 0);
#ifdef RT_ARCH_AMD64

    if (fBits >= UINT32_MAX)
    {
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBits);
        AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->Core.aHstRegs), UINT32_MAX);

        /* test Ev,Gv */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        pbCodeBuf[off++] = X86_OP_REX_W | (iGprSrc < 8 ? 0 : X86_OP_REX_R) | (iTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x85;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprSrc & 8, iTmpReg & 7);

        iemNativeRegFreeTmpImm(pReNative, iTmpReg);
    }
    else
    {
        /* test Eb, imm8 or test Ev, imm32 */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        if (iGprSrc >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
        if (fBits <= UINT8_MAX)
        {
            pbCodeBuf[off++] = 0xf6;
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
            pbCodeBuf[off++] = (uint8_t)fBits;
        }
        else
        {
            pbCodeBuf[off++] = 0xf7;
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, iGprSrc & 7);
            pbCodeBuf[off++] = RT_BYTE1(fBits);
            pbCodeBuf[off++] = RT_BYTE2(fBits);
            pbCodeBuf[off++] = RT_BYTE3(fBits);
            pbCodeBuf[off++] = RT_BYTE4(fBits);
        }
    }

#elif defined(RT_ARCH_ARM64)

    if (false)
    {
        /** @todo figure out how to work the immr / N:imms constants. */
    }
    else
    {
        uint8_t iTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBits);
        AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->Core.aHstRegs), UINT32_MAX);

        /* ands Zr, iGprSrc, iTmpReg */
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
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
DECLINLINE(uint32_t) iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfAnySet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
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
DECLINLINE(uint32_t) iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfNoneSet(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                         uint8_t iGprSrc, uint64_t fBits, uint32_t idxLabel)
{
    Assert(fBits); Assert(!RT_IS_POWER_OF_TWO(fBits));

    off = iemNativeEmitTestAnyBitsInGpr(pReNative, off, iGprSrc, fBits);
    off = iemNativeEmitJzToLabel(pReNative, off, idxLabel);

    return off;
}


/**
 * Emits a call to a 64-bit address.
 */
DECLINLINE(uint32_t) iemNativeEmitCallImm(PIEMRECOMPILERSTATE pReNative, uint32_t off, uintptr_t uPfn)
{
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xAX, uPfn);

    /* call rax */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0xff;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, uPfn);

    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrBlr(IEMNATIVE_REG_FIXED_TMP0);
#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}



/** @} */

#endif /* !VMM_INCLUDED_SRC_include_IEMN8veRecompiler_h */

