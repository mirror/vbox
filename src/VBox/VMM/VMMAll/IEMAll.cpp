/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - All Contexts.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_iem    IEM - Interpreted Execution Manager
 *
 * The interpreted exeuction manager (IEM) is for executing short guest code
 * sequences that are causing too many exits / virtualization traps.  It will
 * also be used to interpret single instructions, thus replacing the selective
 * interpreters in EM and IOM.
 *
 * Design goals:
 *      - Relatively small footprint, although we favour speed and correctness
 *        over size.
 *      - Reasonably fast.
 *      - Correctly handle lock prefixed instructions.
 *      - Complete instruction set - eventually.
 *      - Refactorable into a recompiler, maybe.
 *      - Replace EMInterpret*.
 *
 * Using the existing disassembler has been considered, however this is thought
 * to conflict with speed as the disassembler chews things a bit too much while
 * leaving us with a somewhat complicated state to interpret afterwards.
 *
 *
 * The current code is very much work in progress. You've been warned!
 *
 *
 * @section sec_iem_fpu_instr   FPU Instructions
 *
 * On x86 and AMD64 hosts, the FPU instructions are implemented by executing the
 * same or equivalent instructions on the host FPU.  To make life easy, we also
 * let the FPU prioritize the unmasked exceptions for us.  This however, only
 * works reliably when CR0.NE is set, i.e. when using \#MF instead the IRQ 13
 * for FPU exception delivery, because with CR0.NE=0 there is a window where we
 * can trigger spurious FPU exceptions.
 *
 * The guest FPU state is not loaded into the host CPU and kept there till we
 * leave IEM because the calling conventions have declared an all year open
 * season on much of the FPU state.  For instance an innocent looking call to
 * memcpy might end up using a whole bunch of XMM or MM registers if the
 * particular implementation finds it worthwhile.
 *
 *
 * @section sec_iem_logging     Logging
 *
 * The IEM code uses the \"IEM\" log group for the main logging. The different
 * logging levels/flags are generally used for the following purposes:
 *      - Level 1 (Log) : Errors, exceptions, interrupts and such major events.
 *      - Flow (LogFlow): Basic enter/exit IEM state info.
 *      - Level 2 (Log2): ?
 *      - Level 3 (Log3): More detailed enter/exit IEM state info.
 *      - Level 4 (Log4): Decoding mnemonics w/ EIP.
 *      - Level 5 (Log5): Decoding details.
 *      - Level 6 (Log6): Enables/disables the lockstep comparison with REM.
 *      - Level 7 (Log7): iret++ execution logging.
 *      - Level 8 (Log8): Memory writes.
 *      - Level 9 (Log9): Memory reads.
 *
 */

/** @def IEM_VERIFICATION_MODE_MINIMAL
 * Use for pitting IEM against EM or something else in ring-0 or raw-mode
 * context. */
#if defined(DOXYGEN_RUNNING)
# define IEM_VERIFICATION_MODE_MINIMAL
#endif
//#define IEM_LOG_MEMORY_WRITES
#define IEM_IMPLEMENTS_TASKSWITCH


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP   LOG_GROUP_IEM
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pgm.h>
#include <internal/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/dbgftrace.h>
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
# include <VBox/vmm/patm.h>
# if defined(VBOX_WITH_CALL_RECORD) || defined(REM_MONITOR_CODE_PAGES)
#  include <VBox/vmm/csam.h>
# endif
#endif
#include "IEMInternal.h"
#ifdef IEM_VERIFICATION_MODE_FULL
# include <VBox/vmm/rem.h>
# include <VBox/vmm/mm.h>
#endif
#include <VBox/vmm/vm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/x86.h>



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** @typedef PFNIEMOP
 * Pointer to an opcode decoder function.
 */

/** @def FNIEMOP_DEF
 * Define an opcode decoder function.
 *
 * We're using macors for this so that adding and removing parameters as well as
 * tweaking compiler specific attributes becomes easier.  See FNIEMOP_CALL
 *
 * @param   a_Name      The function name.
 */


#if defined(__GNUC__) && defined(RT_ARCH_X86)
typedef VBOXSTRICTRC (__attribute__((__fastcall__)) * PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu)
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0)
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1)

#elif defined(_MSC_VER) && defined(RT_ARCH_X86)
typedef VBOXSTRICTRC (__fastcall * PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu) RT_NO_THROW_DEF
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0) RT_NO_THROW_DEF
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW_DEF

#elif defined(__GNUC__)
typedef VBOXSTRICTRC (* PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PIEMCPU pIemCpu)
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0)
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1)

#else
typedef VBOXSTRICTRC (* PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    IEM_STATIC VBOXSTRICTRC a_Name(PIEMCPU pIemCpu) RT_NO_THROW_DEF
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    IEM_STATIC VBOXSTRICTRC a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0) RT_NO_THROW_DEF
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    IEM_STATIC VBOXSTRICTRC a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW_DEF

#endif


/**
 * Selector descriptor table entry as fetched by iemMemFetchSelDesc.
 */
typedef union IEMSELDESC
{
    /** The legacy view. */
    X86DESC     Legacy;
    /** The long mode view. */
    X86DESC64   Long;
} IEMSELDESC;
/** Pointer to a selector descriptor table entry. */
typedef IEMSELDESC *PIEMSELDESC;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Temporary hack to disable the double execution.  Will be removed in favor
 * of a dedicated execution mode in EM. */
//#define IEM_VERIFICATION_MODE_NO_REM

/** Used to shut up GCC warnings about variables that 'may be used uninitialized'
 * due to GCC lacking knowledge about the value range of a switch. */
#define IEM_NOT_REACHED_DEFAULT_CASE_RET() default: AssertFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE)

/**
 * Returns IEM_RETURN_ASPECT_NOT_IMPLEMENTED, and in debug builds logs the
 * occation.
 */
#ifdef LOG_ENABLED
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED() \
    do { \
        /*Log*/ LogAlways(("%s: returning IEM_RETURN_ASPECT_NOT_IMPLEMENTED (line %d)\n", __FUNCTION__, __LINE__)); \
        return VERR_IEM_ASPECT_NOT_IMPLEMENTED; \
    } while (0)
#else
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED() \
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED
#endif

/**
 * Returns IEM_RETURN_ASPECT_NOT_IMPLEMENTED, and in debug builds logs the
 * occation using the supplied logger statement.
 *
 * @param   a_LoggerArgs    What to log on failure.
 */
#ifdef LOG_ENABLED
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(a_LoggerArgs) \
    do { \
        LogAlways((LOG_FN_FMT ": ", __PRETTY_FUNCTION__)); LogAlways(a_LoggerArgs); \
        /*LogFunc(a_LoggerArgs);*/ \
        return VERR_IEM_ASPECT_NOT_IMPLEMENTED; \
    } while (0)
#else
# define IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(a_LoggerArgs) \
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED
#endif

/**
 * Call an opcode decoder function.
 *
 * We're using macors for this so that adding and removing parameters can be
 * done as we please.  See FNIEMOP_DEF.
 */
#define FNIEMOP_CALL(a_pfn) (a_pfn)(pIemCpu)

/**
 * Call a common opcode decoder function taking one extra argument.
 *
 * We're using macors for this so that adding and removing parameters can be
 * done as we please.  See FNIEMOP_DEF_1.
 */
#define FNIEMOP_CALL_1(a_pfn, a0)           (a_pfn)(pIemCpu, a0)

/**
 * Call a common opcode decoder function taking one extra argument.
 *
 * We're using macors for this so that adding and removing parameters can be
 * done as we please.  See FNIEMOP_DEF_1.
 */
#define FNIEMOP_CALL_2(a_pfn, a0, a1)       (a_pfn)(pIemCpu, a0, a1)

/**
 * Check if we're currently executing in real or virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pIemCpu       The IEM state of the current CPU.
 */
#define IEM_IS_REAL_OR_V86_MODE(a_pIemCpu)  (CPUMIsGuestInRealOrV86ModeEx((a_pIemCpu)->CTX_SUFF(pCtx)))

/**
 * Check if we're currently executing in virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pIemCpu       The IEM state of the current CPU.
 */
#define IEM_IS_V86_MODE(a_pIemCpu)          (CPUMIsGuestInV86ModeEx((a_pIemCpu)->CTX_SUFF(pCtx)))

/**
 * Check if we're currently executing in long mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pIemCpu       The IEM state of the current CPU.
 */
#define IEM_IS_LONG_MODE(a_pIemCpu)         (CPUMIsGuestInLongModeEx((a_pIemCpu)->CTX_SUFF(pCtx)))

/**
 * Check if we're currently executing in real mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   a_pIemCpu       The IEM state of the current CPU.
 */
#define IEM_IS_REAL_MODE(a_pIemCpu)         (CPUMIsGuestInRealModeEx((a_pIemCpu)->CTX_SUFF(pCtx)))

/**
 * Returns a (const) pointer to the CPUMFEATURES for the guest CPU.
 * @returns PCCPUMFEATURES
 * @param   a_pIemCpu       The IEM state of the current CPU.
 */
#define IEM_GET_GUEST_CPU_FEATURES(a_pIemCpu) (&(IEMCPU_TO_VM(a_pIemCpu)->cpum.ro.GuestFeatures))

/**
 * Returns a (const) pointer to the CPUMFEATURES for the host CPU.
 * @returns PCCPUMFEATURES
 * @param   a_pIemCpu       The IEM state of the current CPU.
 */
#define IEM_GET_HOST_CPU_FEATURES(a_pIemCpu)  (&(IEMCPU_TO_VM(a_pIemCpu)->cpum.ro.HostFeatures))

/**
 * Evaluates to true if we're presenting an Intel CPU to the guest.
 */
#define IEM_IS_GUEST_CPU_INTEL(a_pIemCpu)   ( (a_pIemCpu)->enmCpuVendor == CPUMCPUVENDOR_INTEL )

/**
 * Evaluates to true if we're presenting an AMD CPU to the guest.
 */
#define IEM_IS_GUEST_CPU_AMD(a_pIemCpu)     ( (a_pIemCpu)->enmCpuVendor == CPUMCPUVENDOR_AMD )

/**
 * Check if the address is canonical.
 */
#define IEM_IS_CANONICAL(a_u64Addr)         X86_IS_CANONICAL(a_u64Addr)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern const PFNIEMOP g_apfnOneByteMap[256]; /* not static since we need to forward declare it. */


/** Function table for the ADD instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_add =
{
    iemAImpl_add_u8,  iemAImpl_add_u8_locked,
    iemAImpl_add_u16, iemAImpl_add_u16_locked,
    iemAImpl_add_u32, iemAImpl_add_u32_locked,
    iemAImpl_add_u64, iemAImpl_add_u64_locked
};

/** Function table for the ADC instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_adc =
{
    iemAImpl_adc_u8,  iemAImpl_adc_u8_locked,
    iemAImpl_adc_u16, iemAImpl_adc_u16_locked,
    iemAImpl_adc_u32, iemAImpl_adc_u32_locked,
    iemAImpl_adc_u64, iemAImpl_adc_u64_locked
};

/** Function table for the SUB instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_sub =
{
    iemAImpl_sub_u8,  iemAImpl_sub_u8_locked,
    iemAImpl_sub_u16, iemAImpl_sub_u16_locked,
    iemAImpl_sub_u32, iemAImpl_sub_u32_locked,
    iemAImpl_sub_u64, iemAImpl_sub_u64_locked
};

/** Function table for the SBB instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_sbb =
{
    iemAImpl_sbb_u8,  iemAImpl_sbb_u8_locked,
    iemAImpl_sbb_u16, iemAImpl_sbb_u16_locked,
    iemAImpl_sbb_u32, iemAImpl_sbb_u32_locked,
    iemAImpl_sbb_u64, iemAImpl_sbb_u64_locked
};

/** Function table for the OR instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_or =
{
    iemAImpl_or_u8,  iemAImpl_or_u8_locked,
    iemAImpl_or_u16, iemAImpl_or_u16_locked,
    iemAImpl_or_u32, iemAImpl_or_u32_locked,
    iemAImpl_or_u64, iemAImpl_or_u64_locked
};

/** Function table for the XOR instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_xor =
{
    iemAImpl_xor_u8,  iemAImpl_xor_u8_locked,
    iemAImpl_xor_u16, iemAImpl_xor_u16_locked,
    iemAImpl_xor_u32, iemAImpl_xor_u32_locked,
    iemAImpl_xor_u64, iemAImpl_xor_u64_locked
};

/** Function table for the AND instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_and =
{
    iemAImpl_and_u8,  iemAImpl_and_u8_locked,
    iemAImpl_and_u16, iemAImpl_and_u16_locked,
    iemAImpl_and_u32, iemAImpl_and_u32_locked,
    iemAImpl_and_u64, iemAImpl_and_u64_locked
};

/** Function table for the CMP instruction.
 * @remarks Making operand order ASSUMPTIONS.
 */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_cmp =
{
    iemAImpl_cmp_u8,  NULL,
    iemAImpl_cmp_u16, NULL,
    iemAImpl_cmp_u32, NULL,
    iemAImpl_cmp_u64, NULL
};

/** Function table for the TEST instruction.
 * @remarks Making operand order ASSUMPTIONS.
 */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_test =
{
    iemAImpl_test_u8,  NULL,
    iemAImpl_test_u16, NULL,
    iemAImpl_test_u32, NULL,
    iemAImpl_test_u64, NULL
};

/** Function table for the BT instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bt =
{
    NULL,  NULL,
    iemAImpl_bt_u16, NULL,
    iemAImpl_bt_u32, NULL,
    iemAImpl_bt_u64, NULL
};

/** Function table for the BTC instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_btc =
{
    NULL,  NULL,
    iemAImpl_btc_u16, iemAImpl_btc_u16_locked,
    iemAImpl_btc_u32, iemAImpl_btc_u32_locked,
    iemAImpl_btc_u64, iemAImpl_btc_u64_locked
};

/** Function table for the BTR instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_btr =
{
    NULL,  NULL,
    iemAImpl_btr_u16, iemAImpl_btr_u16_locked,
    iemAImpl_btr_u32, iemAImpl_btr_u32_locked,
    iemAImpl_btr_u64, iemAImpl_btr_u64_locked
};

/** Function table for the BTS instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bts =
{
    NULL,  NULL,
    iemAImpl_bts_u16, iemAImpl_bts_u16_locked,
    iemAImpl_bts_u32, iemAImpl_bts_u32_locked,
    iemAImpl_bts_u64, iemAImpl_bts_u64_locked
};

/** Function table for the BSF instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsf =
{
    NULL,  NULL,
    iemAImpl_bsf_u16, NULL,
    iemAImpl_bsf_u32, NULL,
    iemAImpl_bsf_u64, NULL
};

/** Function table for the BSR instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_bsr =
{
    NULL,  NULL,
    iemAImpl_bsr_u16, NULL,
    iemAImpl_bsr_u32, NULL,
    iemAImpl_bsr_u64, NULL
};

/** Function table for the IMUL instruction. */
IEM_STATIC const IEMOPBINSIZES g_iemAImpl_imul_two =
{
    NULL,  NULL,
    iemAImpl_imul_two_u16, NULL,
    iemAImpl_imul_two_u32, NULL,
    iemAImpl_imul_two_u64, NULL
};

/** Group 1 /r lookup table. */
IEM_STATIC const PCIEMOPBINSIZES g_apIemImplGrp1[8] =
{
    &g_iemAImpl_add,
    &g_iemAImpl_or,
    &g_iemAImpl_adc,
    &g_iemAImpl_sbb,
    &g_iemAImpl_and,
    &g_iemAImpl_sub,
    &g_iemAImpl_xor,
    &g_iemAImpl_cmp
};

/** Function table for the INC instruction. */
IEM_STATIC const IEMOPUNARYSIZES g_iemAImpl_inc =
{
    iemAImpl_inc_u8,  iemAImpl_inc_u8_locked,
    iemAImpl_inc_u16, iemAImpl_inc_u16_locked,
    iemAImpl_inc_u32, iemAImpl_inc_u32_locked,
    iemAImpl_inc_u64, iemAImpl_inc_u64_locked
};

/** Function table for the DEC instruction. */
IEM_STATIC const IEMOPUNARYSIZES g_iemAImpl_dec =
{
    iemAImpl_dec_u8,  iemAImpl_dec_u8_locked,
    iemAImpl_dec_u16, iemAImpl_dec_u16_locked,
    iemAImpl_dec_u32, iemAImpl_dec_u32_locked,
    iemAImpl_dec_u64, iemAImpl_dec_u64_locked
};

/** Function table for the NEG instruction. */
IEM_STATIC const IEMOPUNARYSIZES g_iemAImpl_neg =
{
    iemAImpl_neg_u8,  iemAImpl_neg_u8_locked,
    iemAImpl_neg_u16, iemAImpl_neg_u16_locked,
    iemAImpl_neg_u32, iemAImpl_neg_u32_locked,
    iemAImpl_neg_u64, iemAImpl_neg_u64_locked
};

/** Function table for the NOT instruction. */
IEM_STATIC const IEMOPUNARYSIZES g_iemAImpl_not =
{
    iemAImpl_not_u8,  iemAImpl_not_u8_locked,
    iemAImpl_not_u16, iemAImpl_not_u16_locked,
    iemAImpl_not_u32, iemAImpl_not_u32_locked,
    iemAImpl_not_u64, iemAImpl_not_u64_locked
};


/** Function table for the ROL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rol =
{
    iemAImpl_rol_u8,
    iemAImpl_rol_u16,
    iemAImpl_rol_u32,
    iemAImpl_rol_u64
};

/** Function table for the ROR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_ror =
{
    iemAImpl_ror_u8,
    iemAImpl_ror_u16,
    iemAImpl_ror_u32,
    iemAImpl_ror_u64
};

/** Function table for the RCL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcl =
{
    iemAImpl_rcl_u8,
    iemAImpl_rcl_u16,
    iemAImpl_rcl_u32,
    iemAImpl_rcl_u64
};

/** Function table for the RCR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_rcr =
{
    iemAImpl_rcr_u8,
    iemAImpl_rcr_u16,
    iemAImpl_rcr_u32,
    iemAImpl_rcr_u64
};

/** Function table for the SHL instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shl =
{
    iemAImpl_shl_u8,
    iemAImpl_shl_u16,
    iemAImpl_shl_u32,
    iemAImpl_shl_u64
};

/** Function table for the SHR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_shr =
{
    iemAImpl_shr_u8,
    iemAImpl_shr_u16,
    iemAImpl_shr_u32,
    iemAImpl_shr_u64
};

/** Function table for the SAR instruction. */
IEM_STATIC const IEMOPSHIFTSIZES g_iemAImpl_sar =
{
    iemAImpl_sar_u8,
    iemAImpl_sar_u16,
    iemAImpl_sar_u32,
    iemAImpl_sar_u64
};


/** Function table for the MUL instruction. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_mul =
{
    iemAImpl_mul_u8,
    iemAImpl_mul_u16,
    iemAImpl_mul_u32,
    iemAImpl_mul_u64
};

/** Function table for the IMUL instruction working implicitly on rAX. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_imul =
{
    iemAImpl_imul_u8,
    iemAImpl_imul_u16,
    iemAImpl_imul_u32,
    iemAImpl_imul_u64
};

/** Function table for the DIV instruction. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_div =
{
    iemAImpl_div_u8,
    iemAImpl_div_u16,
    iemAImpl_div_u32,
    iemAImpl_div_u64
};

/** Function table for the MUL instruction. */
IEM_STATIC const IEMOPMULDIVSIZES g_iemAImpl_idiv =
{
    iemAImpl_idiv_u8,
    iemAImpl_idiv_u16,
    iemAImpl_idiv_u32,
    iemAImpl_idiv_u64
};

/** Function table for the SHLD instruction */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shld =
{
    iemAImpl_shld_u16,
    iemAImpl_shld_u32,
    iemAImpl_shld_u64,
};

/** Function table for the SHRD instruction */
IEM_STATIC const IEMOPSHIFTDBLSIZES g_iemAImpl_shrd =
{
    iemAImpl_shrd_u16,
    iemAImpl_shrd_u32,
    iemAImpl_shrd_u64,
};


/** Function table for the PUNPCKLBW instruction */
IEM_STATIC const IEMOPMEDIAF1L1 g_iemAImpl_punpcklbw  = { iemAImpl_punpcklbw_u64,  iemAImpl_punpcklbw_u128 };
/** Function table for the PUNPCKLBD instruction */
IEM_STATIC const IEMOPMEDIAF1L1 g_iemAImpl_punpcklwd  = { iemAImpl_punpcklwd_u64,  iemAImpl_punpcklwd_u128 };
/** Function table for the PUNPCKLDQ instruction */
IEM_STATIC const IEMOPMEDIAF1L1 g_iemAImpl_punpckldq  = { iemAImpl_punpckldq_u64,  iemAImpl_punpckldq_u128 };
/** Function table for the PUNPCKLQDQ instruction */
IEM_STATIC const IEMOPMEDIAF1L1 g_iemAImpl_punpcklqdq = { NULL, iemAImpl_punpcklqdq_u128 };

/** Function table for the PUNPCKHBW instruction */
IEM_STATIC const IEMOPMEDIAF1H1 g_iemAImpl_punpckhbw  = { iemAImpl_punpckhbw_u64,  iemAImpl_punpckhbw_u128 };
/** Function table for the PUNPCKHBD instruction */
IEM_STATIC const IEMOPMEDIAF1H1 g_iemAImpl_punpckhwd  = { iemAImpl_punpckhwd_u64,  iemAImpl_punpckhwd_u128 };
/** Function table for the PUNPCKHDQ instruction */
IEM_STATIC const IEMOPMEDIAF1H1 g_iemAImpl_punpckhdq  = { iemAImpl_punpckhdq_u64,  iemAImpl_punpckhdq_u128 };
/** Function table for the PUNPCKHQDQ instruction */
IEM_STATIC const IEMOPMEDIAF1H1 g_iemAImpl_punpckhqdq = { NULL, iemAImpl_punpckhqdq_u128 };

/** Function table for the PXOR instruction */
IEM_STATIC const IEMOPMEDIAF2 g_iemAImpl_pxor         = { iemAImpl_pxor_u64,       iemAImpl_pxor_u128 };
/** Function table for the PCMPEQB instruction */
IEM_STATIC const IEMOPMEDIAF2 g_iemAImpl_pcmpeqb      = { iemAImpl_pcmpeqb_u64,    iemAImpl_pcmpeqb_u128 };
/** Function table for the PCMPEQW instruction */
IEM_STATIC const IEMOPMEDIAF2 g_iemAImpl_pcmpeqw      = { iemAImpl_pcmpeqw_u64,    iemAImpl_pcmpeqw_u128 };
/** Function table for the PCMPEQD instruction */
IEM_STATIC const IEMOPMEDIAF2 g_iemAImpl_pcmpeqd      = { iemAImpl_pcmpeqd_u64,    iemAImpl_pcmpeqd_u128 };


#if defined(IEM_VERIFICATION_MODE_MINIMAL) || defined(IEM_LOG_MEMORY_WRITES)
/** What IEM just wrote. */
uint8_t g_abIemWrote[256];
/** How much IEM just wrote. */
size_t g_cbIemWrote;
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
IEM_STATIC VBOXSTRICTRC     iemRaiseTaskSwitchFaultWithErr(PIEMCPU pIemCpu, uint16_t uErr);
IEM_STATIC VBOXSTRICTRC     iemRaiseTaskSwitchFaultCurrentTSS(PIEMCPU pIemCpu);
IEM_STATIC VBOXSTRICTRC     iemRaiseTaskSwitchFault0(PIEMCPU pIemCpu);
IEM_STATIC VBOXSTRICTRC     iemRaiseTaskSwitchFaultBySelector(PIEMCPU pIemCpu, uint16_t uSel);
/*IEM_STATIC VBOXSTRICTRC     iemRaiseSelectorNotPresent(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);*/
IEM_STATIC VBOXSTRICTRC     iemRaiseSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel);
IEM_STATIC VBOXSTRICTRC     iemRaiseSelectorNotPresentWithErr(PIEMCPU pIemCpu, uint16_t uErr);
IEM_STATIC VBOXSTRICTRC     iemRaiseStackSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel);
IEM_STATIC VBOXSTRICTRC     iemRaiseStackSelectorNotPresentWithErr(PIEMCPU pIemCpu, uint16_t uErr);
IEM_STATIC VBOXSTRICTRC     iemRaiseGeneralProtectionFault(PIEMCPU pIemCpu, uint16_t uErr);
IEM_STATIC VBOXSTRICTRC     iemRaiseGeneralProtectionFault0(PIEMCPU pIemCpu);
IEM_STATIC VBOXSTRICTRC     iemRaiseGeneralProtectionFaultBySelector(PIEMCPU pIemCpu, RTSEL uSel);
IEM_STATIC VBOXSTRICTRC     iemRaiseSelectorBounds(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
IEM_STATIC VBOXSTRICTRC     iemRaiseSelectorBoundsBySelector(PIEMCPU pIemCpu, RTSEL Sel);
IEM_STATIC VBOXSTRICTRC     iemRaiseSelectorInvalidAccess(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
IEM_STATIC VBOXSTRICTRC     iemRaisePageFault(PIEMCPU pIemCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc);
IEM_STATIC VBOXSTRICTRC     iemRaiseAlignmentCheckException(PIEMCPU pIemCpu);
IEM_STATIC VBOXSTRICTRC     iemMemMap(PIEMCPU pIemCpu, void **ppvMem, size_t cbMem, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t fAccess);
IEM_STATIC VBOXSTRICTRC     iemMemCommitAndUnmap(PIEMCPU pIemCpu, void *pvMem, uint32_t fAccess);
IEM_STATIC VBOXSTRICTRC     iemMemFetchDataU32(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
IEM_STATIC VBOXSTRICTRC     iemMemFetchDataU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
IEM_STATIC VBOXSTRICTRC     iemMemFetchSysU8(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
IEM_STATIC VBOXSTRICTRC     iemMemFetchSysU16(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
IEM_STATIC VBOXSTRICTRC     iemMemFetchSysU32(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
IEM_STATIC VBOXSTRICTRC     iemMemFetchSysU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
IEM_STATIC VBOXSTRICTRC     iemMemFetchSelDescWithErr(PIEMCPU pIemCpu, PIEMSELDESC pDesc, uint16_t uSel, uint8_t uXcpt, uint16_t uErrorCode);
IEM_STATIC VBOXSTRICTRC     iemMemFetchSelDesc(PIEMCPU pIemCpu, PIEMSELDESC pDesc, uint16_t uSel, uint8_t uXcpt);
IEM_STATIC VBOXSTRICTRC     iemMemStackPushCommitSpecial(PIEMCPU pIemCpu, void *pvMem, uint64_t uNewRsp);
IEM_STATIC VBOXSTRICTRC     iemMemStackPushBeginSpecial(PIEMCPU pIemCpu, size_t cbMem, void **ppvMem, uint64_t *puNewRsp);
IEM_STATIC VBOXSTRICTRC     iemMemStackPushU32(PIEMCPU pIemCpu, uint32_t u32Value);
IEM_STATIC VBOXSTRICTRC     iemMemStackPushU16(PIEMCPU pIemCpu, uint16_t u16Value);
IEM_STATIC VBOXSTRICTRC     iemMemMarkSelDescAccessed(PIEMCPU pIemCpu, uint16_t uSel);
IEM_STATIC uint16_t         iemSRegFetchU16(PIEMCPU pIemCpu, uint8_t iSegReg);

#if defined(IEM_VERIFICATION_MODE_FULL) && !defined(IEM_VERIFICATION_MODE_MINIMAL)
IEM_STATIC PIEMVERIFYEVTREC iemVerifyAllocRecord(PIEMCPU pIemCpu);
#endif
IEM_STATIC VBOXSTRICTRC     iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue);
IEM_STATIC VBOXSTRICTRC     iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue);



/**
 * Sets the pass up status.
 *
 * @returns VINF_SUCCESS.
 * @param   pIemCpu             The per CPU IEM state of the calling thread.
 * @param   rcPassUp            The pass up status.  Must be informational.
 *                              VINF_SUCCESS is not allowed.
 */
IEM_STATIC int iemSetPassUpStatus(PIEMCPU pIemCpu, VBOXSTRICTRC rcPassUp)
{
    AssertRC(VBOXSTRICTRC_VAL(rcPassUp)); Assert(rcPassUp != VINF_SUCCESS);

    int32_t const rcOldPassUp = pIemCpu->rcPassUp;
    if (rcOldPassUp == VINF_SUCCESS)
        pIemCpu->rcPassUp = VBOXSTRICTRC_VAL(rcPassUp);
    /* If both are EM scheduling codes, use EM priority rules. */
    else if (   rcOldPassUp >= VINF_EM_FIRST && rcOldPassUp <= VINF_EM_LAST
             && rcPassUp    >= VINF_EM_FIRST && rcPassUp    <= VINF_EM_LAST)
    {
        if (rcPassUp < rcOldPassUp)
        {
            Log(("IEM: rcPassUp=%Rrc! rcOldPassUp=%Rrc\n", VBOXSTRICTRC_VAL(rcPassUp), rcOldPassUp));
            pIemCpu->rcPassUp = VBOXSTRICTRC_VAL(rcPassUp);
        }
        else
            Log(("IEM: rcPassUp=%Rrc  rcOldPassUp=%Rrc!\n", VBOXSTRICTRC_VAL(rcPassUp), rcOldPassUp));
    }
    /* Override EM scheduling with specific status code. */
    else if (rcOldPassUp >= VINF_EM_FIRST && rcOldPassUp <= VINF_EM_LAST)
    {
        Log(("IEM: rcPassUp=%Rrc! rcOldPassUp=%Rrc\n", VBOXSTRICTRC_VAL(rcPassUp), rcOldPassUp));
        pIemCpu->rcPassUp = VBOXSTRICTRC_VAL(rcPassUp);
    }
    /* Don't override specific status code, first come first served. */
    else
        Log(("IEM: rcPassUp=%Rrc  rcOldPassUp=%Rrc!\n", VBOXSTRICTRC_VAL(rcPassUp), rcOldPassUp));
    return VINF_SUCCESS;
}


/**
 * Calculates the CPU mode.
 *
 * This is mainly for updating IEMCPU::enmCpuMode.
 *
 * @returns CPU mode.
 * @param   pCtx        The register context for the CPU.
 */
DECLINLINE(IEMMODE) iemCalcCpuMode(PCPUMCTX pCtx)
{
    if (CPUMIsGuestIn64BitCodeEx(pCtx))
        return IEMMODE_64BIT;
    if (pCtx->cs.Attr.n.u1DefBig) /** @todo check if this is correct... */
        return IEMMODE_32BIT;
    return IEMMODE_16BIT;
}


/**
 * Initializes the execution state.
 *
 * @param   pIemCpu             The per CPU IEM state.
 * @param   fBypassHandlers     Whether to bypass access handlers.
 *
 * @remarks Callers of this must call iemUninitExec() to undo potentially fatal
 *          side-effects in strict builds.
 */
DECLINLINE(void) iemInitExec(PIEMCPU pIemCpu, bool fBypassHandlers)
{
    PCPUMCTX pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVMCPU   pVCpu = IEMCPU_TO_VMCPU(pIemCpu);

    Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_IEM));
    Assert(pIemCpu->PendingCommit.enmFn == IEMCOMMIT_INVALID);

#if defined(VBOX_STRICT) && (defined(IEM_VERIFICATION_MODE_FULL) || !defined(VBOX_WITH_RAW_MODE_NOT_R0))
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->cs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->ss));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->es));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->ds));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->fs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->gs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->ldtr));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->tr));
#endif

#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    CPUMGuestLazyLoadHiddenCsAndSs(pVCpu);
#endif
    pIemCpu->uCpl               = CPUMGetGuestCPL(pVCpu);
    pIemCpu->enmCpuMode         = iemCalcCpuMode(pCtx);
#ifdef VBOX_STRICT
    pIemCpu->enmDefAddrMode     = (IEMMODE)0xc0fe;
    pIemCpu->enmEffAddrMode     = (IEMMODE)0xc0fe;
    pIemCpu->enmDefOpSize       = (IEMMODE)0xc0fe;
    pIemCpu->enmEffOpSize       = (IEMMODE)0xc0fe;
    pIemCpu->fPrefixes          = (IEMMODE)0xfeedbeef;
    pIemCpu->uRexReg            = 127;
    pIemCpu->uRexB              = 127;
    pIemCpu->uRexIndex          = 127;
    pIemCpu->iEffSeg            = 127;
    pIemCpu->offOpcode          = 127;
    pIemCpu->cbOpcode           = 127;
#endif

    pIemCpu->cActiveMappings    = 0;
    pIemCpu->iNextMapping       = 0;
    pIemCpu->rcPassUp           = VINF_SUCCESS;
    pIemCpu->fBypassHandlers    = fBypassHandlers;
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    pIemCpu->fInPatchCode       = pIemCpu->uCpl == 0
                               && pCtx->cs.u64Base == 0
                               && pCtx->cs.u32Limit == UINT32_MAX
                               && PATMIsPatchGCAddr(IEMCPU_TO_VM(pIemCpu), pCtx->eip);
    if (!pIemCpu->fInPatchCode)
        CPUMRawLeave(pVCpu, VINF_SUCCESS);
#endif

#ifdef IEM_VERIFICATION_MODE_FULL
    pIemCpu->fNoRemSavedByExec = pIemCpu->fNoRem;
    pIemCpu->fNoRem = true;
#endif
}


/**
 * Counterpart to #iemInitExec that undoes evil strict-build stuff.
 *
 * @param   pIemCpu             The per CPU IEM state.
 */
DECLINLINE(void) iemUninitExec(PIEMCPU pIemCpu)
{
#ifdef IEM_VERIFICATION_MODE_FULL
    pIemCpu->fNoRem = pIemCpu->fNoRemSavedByExec;
#endif
#ifdef VBOX_STRICT
    pIemCpu->cbOpcode = 0;
#else
    NOREF(pIemCpu);
#endif
}


/**
 * Initializes the decoder state.
 *
 * @param   pIemCpu             The per CPU IEM state.
 * @param   fBypassHandlers     Whether to bypass access handlers.
 */
DECLINLINE(void) iemInitDecoder(PIEMCPU pIemCpu, bool fBypassHandlers)
{
    PCPUMCTX pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVMCPU   pVCpu = IEMCPU_TO_VMCPU(pIemCpu);

    Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_IEM));
    Assert(pIemCpu->PendingCommit.enmFn == IEMCOMMIT_INVALID);

#if defined(VBOX_STRICT) && (defined(IEM_VERIFICATION_MODE_FULL) || !defined(VBOX_WITH_RAW_MODE_NOT_R0))
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->cs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->ss));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->es));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->ds));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->fs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->gs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->ldtr));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pCtx->tr));
#endif

#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    CPUMGuestLazyLoadHiddenCsAndSs(pVCpu);
#endif
    pIemCpu->uCpl               = CPUMGetGuestCPL(pVCpu);
#ifdef IEM_VERIFICATION_MODE_FULL
    if (pIemCpu->uInjectCpl != UINT8_MAX)
        pIemCpu->uCpl           = pIemCpu->uInjectCpl;
#endif
    IEMMODE enmMode = iemCalcCpuMode(pCtx);
    pIemCpu->enmCpuMode         = enmMode;
    pIemCpu->enmDefAddrMode     = enmMode;  /** @todo check if this is correct... */
    pIemCpu->enmEffAddrMode     = enmMode;
    if (enmMode != IEMMODE_64BIT)
    {
        pIemCpu->enmDefOpSize   = enmMode;  /** @todo check if this is correct... */
        pIemCpu->enmEffOpSize   = enmMode;
    }
    else
    {
        pIemCpu->enmDefOpSize   = IEMMODE_32BIT;
        pIemCpu->enmEffOpSize   = IEMMODE_32BIT;
    }
    pIemCpu->fPrefixes          = 0;
    pIemCpu->uRexReg            = 0;
    pIemCpu->uRexB              = 0;
    pIemCpu->uRexIndex          = 0;
    pIemCpu->iEffSeg            = X86_SREG_DS;
    pIemCpu->offOpcode          = 0;
    pIemCpu->cbOpcode           = 0;
    pIemCpu->cActiveMappings    = 0;
    pIemCpu->iNextMapping       = 0;
    pIemCpu->rcPassUp           = VINF_SUCCESS;
    pIemCpu->fBypassHandlers    = fBypassHandlers;
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    pIemCpu->fInPatchCode       = pIemCpu->uCpl == 0
                               && pCtx->cs.u64Base == 0
                               && pCtx->cs.u32Limit == UINT32_MAX
                               && PATMIsPatchGCAddr(IEMCPU_TO_VM(pIemCpu), pCtx->eip);
    if (!pIemCpu->fInPatchCode)
        CPUMRawLeave(pVCpu, VINF_SUCCESS);
#endif

#ifdef DBGFTRACE_ENABLED
    switch (enmMode)
    {
        case IEMMODE_64BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I64/%u %08llx", pIemCpu->uCpl, pCtx->rip);
            break;
        case IEMMODE_32BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I32/%u %04x:%08x", pIemCpu->uCpl, pCtx->cs.Sel, pCtx->eip);
            break;
        case IEMMODE_16BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I16/%u %04x:%04x", pIemCpu->uCpl, pCtx->cs.Sel, pCtx->eip);
            break;
    }
#endif
}


/**
 * Prefetch opcodes the first time when starting executing.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   fBypassHandlers     Whether to bypass access handlers.
 */
IEM_STATIC VBOXSTRICTRC iemInitDecoderAndPrefetchOpcodes(PIEMCPU pIemCpu, bool fBypassHandlers)
{
#ifdef IEM_VERIFICATION_MODE_FULL
    uint8_t const cbOldOpcodes = pIemCpu->cbOpcode;
#endif
    iemInitDecoder(pIemCpu, fBypassHandlers);

    /*
     * What we're doing here is very similar to iemMemMap/iemMemBounceBufferMap.
     *
     * First translate CS:rIP to a physical address.
     */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    uint32_t    cbToTryRead;
    RTGCPTR     GCPtrPC;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        cbToTryRead = PAGE_SIZE;
        GCPtrPC     = pCtx->rip;
        if (!IEM_IS_CANONICAL(GCPtrPC))
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        cbToTryRead = PAGE_SIZE - (GCPtrPC & PAGE_OFFSET_MASK);
    }
    else
    {
        uint32_t GCPtrPC32 = pCtx->eip;
        AssertMsg(!(GCPtrPC32 & ~(uint32_t)UINT16_MAX) || pIemCpu->enmCpuMode == IEMMODE_32BIT, ("%04x:%RX64\n", pCtx->cs.Sel, pCtx->rip));
        if (GCPtrPC32 > pCtx->cs.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        cbToTryRead = pCtx->cs.u32Limit - GCPtrPC32 + 1;
        if (!cbToTryRead) /* overflowed */
        {
            Assert(GCPtrPC32 == 0); Assert(pCtx->cs.u32Limit == UINT32_MAX);
            cbToTryRead = UINT32_MAX;
        }
        GCPtrPC = (uint32_t)pCtx->cs.u64Base + GCPtrPC32;
        Assert(GCPtrPC <= UINT32_MAX);
    }

#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    /* Allow interpretation of patch manager code blocks since they can for
       instance throw #PFs for perfectly good reasons. */
    if (pIemCpu->fInPatchCode)
    {
        size_t cbRead = 0;
        int rc = PATMReadPatchCode(IEMCPU_TO_VM(pIemCpu), GCPtrPC, pIemCpu->abOpcode, sizeof(pIemCpu->abOpcode), &cbRead);
        AssertRCReturn(rc, rc);
        pIemCpu->cbOpcode = (uint8_t)cbRead; Assert(pIemCpu->cbOpcode == cbRead); Assert(cbRead > 0);
        return VINF_SUCCESS;
    }
#endif /* VBOX_WITH_RAW_MODE_NOT_R0 */

    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int rc = PGMGstGetPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrPC, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
    {
        Log(("iemInitDecoderAndPrefetchOpcodes: %RGv - rc=%Rrc\n", GCPtrPC, rc));
        return iemRaisePageFault(pIemCpu, GCPtrPC, IEM_ACCESS_INSTRUCTION, rc);
    }
    if (!(fFlags & X86_PTE_US) && pIemCpu->uCpl == 3)
    {
        Log(("iemInitDecoderAndPrefetchOpcodes: %RGv - supervisor page\n", GCPtrPC));
        return iemRaisePageFault(pIemCpu, GCPtrPC, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    }
    if ((fFlags & X86_PTE_PAE_NX) && (pCtx->msrEFER & MSR_K6_EFER_NXE))
    {
        Log(("iemInitDecoderAndPrefetchOpcodes: %RGv - NX\n", GCPtrPC));
        return iemRaisePageFault(pIemCpu, GCPtrPC, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    }
    GCPhys |= GCPtrPC & PAGE_OFFSET_MASK;
    /** @todo Check reserved bits and such stuff. PGM is better at doing
     *        that, so do it when implementing the guest virtual address
     *        TLB... */

#ifdef IEM_VERIFICATION_MODE_FULL
    /*
     * Optimistic optimization: Use unconsumed opcode bytes from the previous
     *                          instruction.
     */
    /** @todo optimize this differently by not using PGMPhysRead. */
    RTGCPHYS const offPrevOpcodes = GCPhys - pIemCpu->GCPhysOpcodes;
    pIemCpu->GCPhysOpcodes = GCPhys;
    if (   offPrevOpcodes < cbOldOpcodes
        && PAGE_SIZE - (GCPhys & PAGE_OFFSET_MASK) > sizeof(pIemCpu->abOpcode))
    {
        uint8_t cbNew = cbOldOpcodes - (uint8_t)offPrevOpcodes;
        Assert(cbNew <= RT_ELEMENTS(pIemCpu->abOpcode));
        memmove(&pIemCpu->abOpcode[0], &pIemCpu->abOpcode[offPrevOpcodes], cbNew);
        pIemCpu->cbOpcode = cbNew;
        return VINF_SUCCESS;
    }
#endif

    /*
     * Read the bytes at this address.
     */
    PVM pVM = IEMCPU_TO_VM(pIemCpu);
#if defined(IN_RING3) && defined(VBOX_WITH_RAW_MODE_NOT_R0)
    size_t cbActual;
    if (   PATMIsEnabled(pVM)
        && RT_SUCCESS(PATMR3ReadOrgInstr(pVM, GCPtrPC, pIemCpu->abOpcode, sizeof(pIemCpu->abOpcode), &cbActual)))
    {
        Log4(("decode - Read %u unpatched bytes at %RGv\n", cbActual, GCPtrPC));
        Assert(cbActual > 0);
        pIemCpu->cbOpcode = (uint8_t)cbActual;
    }
    else
#endif
    {
        uint32_t cbLeftOnPage = PAGE_SIZE - (GCPtrPC & PAGE_OFFSET_MASK);
        if (cbToTryRead > cbLeftOnPage)
            cbToTryRead = cbLeftOnPage;
        if (cbToTryRead > sizeof(pIemCpu->abOpcode))
            cbToTryRead = sizeof(pIemCpu->abOpcode);

        if (!pIemCpu->fBypassHandlers)
        {
            VBOXSTRICTRC rcStrict = PGMPhysRead(pVM, GCPhys, pIemCpu->abOpcode, cbToTryRead, PGMACCESSORIGIN_IEM);
            if (RT_LIKELY(rcStrict == VINF_SUCCESS))
            { /* likely */ }
            else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
            {
                Log(("iemInitDecoderAndPrefetchOpcodes: %RGv/%RGp LB %#x - read status -  rcStrict=%Rrc\n",
                     GCPtrPC, GCPhys, VBOXSTRICTRC_VAL(rcStrict), cbToTryRead));
                rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
            }
            else
            {
                Log((RT_SUCCESS(rcStrict)
                     ? "iemInitDecoderAndPrefetchOpcodes: %RGv/%RGp LB %#x - read status - rcStrict=%Rrc\n"
                     : "iemInitDecoderAndPrefetchOpcodes: %RGv/%RGp LB %#x - read error - rcStrict=%Rrc (!!)\n",
                     GCPtrPC, GCPhys, VBOXSTRICTRC_VAL(rcStrict), cbToTryRead));
                return rcStrict;
            }
        }
        else
        {
            rc = PGMPhysSimpleReadGCPhys(pVM, pIemCpu->abOpcode, GCPhys, cbToTryRead);
            if (RT_SUCCESS(rc))
            { /* likely */ }
            else
            {
                Log(("iemInitDecoderAndPrefetchOpcodes: %RGv/%RGp LB %#x - read error - rc=%Rrc (!!)\n",
                     GCPtrPC, GCPhys, rc, cbToTryRead));
                return rc;
            }
        }
        pIemCpu->cbOpcode = cbToTryRead;
    }

    return VINF_SUCCESS;
}


/**
 * Try fetch at least @a cbMin bytes more opcodes, raise the appropriate
 * exception if it fails.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   cbMin               The minimum number of bytes relative offOpcode
 *                              that must be read.
 */
IEM_STATIC VBOXSTRICTRC iemOpcodeFetchMoreBytes(PIEMCPU pIemCpu, size_t cbMin)
{
    /*
     * What we're doing here is very similar to iemMemMap/iemMemBounceBufferMap.
     *
     * First translate CS:rIP to a physical address.
     */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    uint8_t     cbLeft = pIemCpu->cbOpcode - pIemCpu->offOpcode; Assert(cbLeft < cbMin);
    uint32_t    cbToTryRead;
    RTGCPTR     GCPtrNext;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        cbToTryRead = PAGE_SIZE;
        GCPtrNext   = pCtx->rip + pIemCpu->cbOpcode;
        if (!IEM_IS_CANONICAL(GCPtrNext))
            return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    else
    {
        uint32_t GCPtrNext32 = pCtx->eip;
        Assert(!(GCPtrNext32 & ~(uint32_t)UINT16_MAX) || pIemCpu->enmCpuMode == IEMMODE_32BIT);
        GCPtrNext32 += pIemCpu->cbOpcode;
        if (GCPtrNext32 > pCtx->cs.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        cbToTryRead = pCtx->cs.u32Limit - GCPtrNext32 + 1;
        if (!cbToTryRead) /* overflowed */
        {
            Assert(GCPtrNext32 == 0); Assert(pCtx->cs.u32Limit == UINT32_MAX);
            cbToTryRead = UINT32_MAX;
            /** @todo check out wrapping around the code segment.  */
        }
        if (cbToTryRead < cbMin - cbLeft)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        GCPtrNext = (uint32_t)pCtx->cs.u64Base + GCPtrNext32;
    }

    /* Only read up to the end of the page, and make sure we don't read more
       than the opcode buffer can hold. */
    uint32_t cbLeftOnPage = PAGE_SIZE - (GCPtrNext & PAGE_OFFSET_MASK);
    if (cbToTryRead > cbLeftOnPage)
        cbToTryRead = cbLeftOnPage;
    if (cbToTryRead > sizeof(pIemCpu->abOpcode) - pIemCpu->cbOpcode)
        cbToTryRead = sizeof(pIemCpu->abOpcode) - pIemCpu->cbOpcode;
/** @todo r=bird: Convert assertion into undefined opcode exception? */
    Assert(cbToTryRead >= cbMin - cbLeft); /* ASSUMPTION based on iemInitDecoderAndPrefetchOpcodes. */

#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    /* Allow interpretation of patch manager code blocks since they can for
       instance throw #PFs for perfectly good reasons. */
    if (pIemCpu->fInPatchCode)
    {
        size_t cbRead = 0;
        int rc = PATMReadPatchCode(IEMCPU_TO_VM(pIemCpu), GCPtrNext, pIemCpu->abOpcode, cbToTryRead, &cbRead);
        AssertRCReturn(rc, rc);
        pIemCpu->cbOpcode = (uint8_t)cbRead; Assert(pIemCpu->cbOpcode == cbRead); Assert(cbRead > 0);
        return VINF_SUCCESS;
    }
#endif /* VBOX_WITH_RAW_MODE_NOT_R0 */

    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int rc = PGMGstGetPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrNext, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
    {
        Log(("iemOpcodeFetchMoreBytes: %RGv - rc=%Rrc\n", GCPtrNext, rc));
        return iemRaisePageFault(pIemCpu, GCPtrNext, IEM_ACCESS_INSTRUCTION, rc);
    }
    if (!(fFlags & X86_PTE_US) && pIemCpu->uCpl == 3)
    {
        Log(("iemOpcodeFetchMoreBytes: %RGv - supervisor page\n", GCPtrNext));
        return iemRaisePageFault(pIemCpu, GCPtrNext, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    }
    if ((fFlags & X86_PTE_PAE_NX) && (pCtx->msrEFER & MSR_K6_EFER_NXE))
    {
        Log(("iemOpcodeFetchMoreBytes: %RGv - NX\n", GCPtrNext));
        return iemRaisePageFault(pIemCpu, GCPtrNext, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    }
    GCPhys |= GCPtrNext & PAGE_OFFSET_MASK;
    Log5(("GCPtrNext=%RGv GCPhys=%RGp cbOpcodes=%#x\n",  GCPtrNext,  GCPhys,  pIemCpu->cbOpcode));
    /** @todo Check reserved bits and such stuff. PGM is better at doing
     *        that, so do it when implementing the guest virtual address
     *        TLB... */

    /*
     * Read the bytes at this address.
     *
     * We read all unpatched bytes in iemInitDecoderAndPrefetchOpcodes already,
     * and since PATM should only patch the start of an instruction there
     * should be no need to check again here.
     */
    if (!pIemCpu->fBypassHandlers)
    {
        VBOXSTRICTRC rcStrict = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhys, &pIemCpu->abOpcode[pIemCpu->cbOpcode],
                                            cbToTryRead, PGMACCESSORIGIN_IEM);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /* likely */ }
        else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
        {
            Log(("iemOpcodeFetchMoreBytes: %RGv/%RGp LB %#x - read status -  rcStrict=%Rrc\n",
                 GCPtrNext, GCPhys, VBOXSTRICTRC_VAL(rcStrict), cbToTryRead));
            rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
        }
        else
        {
            Log((RT_SUCCESS(rcStrict)
                 ? "iemOpcodeFetchMoreBytes: %RGv/%RGp LB %#x - read status - rcStrict=%Rrc\n"
                 : "iemOpcodeFetchMoreBytes: %RGv/%RGp LB %#x - read error - rcStrict=%Rrc (!!)\n",
                 GCPtrNext, GCPhys, VBOXSTRICTRC_VAL(rcStrict), cbToTryRead));
            return rcStrict;
        }
    }
    else
    {
        rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), &pIemCpu->abOpcode[pIemCpu->cbOpcode], GCPhys, cbToTryRead);
        if (RT_SUCCESS(rc))
        { /* likely */ }
        else
        {
            Log(("iemOpcodeFetchMoreBytes: %RGv - read error - rc=%Rrc (!!)\n", GCPtrNext, rc));
            return rc;
        }
    }
    pIemCpu->cbOpcode += cbToTryRead;
    Log5(("%.*Rhxs\n", pIemCpu->cbOpcode, pIemCpu->abOpcode));

    return VINF_SUCCESS;
}


/**
 * Deals with the problematic cases that iemOpcodeGetNextU8 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pb                  Where to return the opcode byte.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU8Slow(PIEMCPU pIemCpu, uint8_t *pb)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 1);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pb = pIemCpu->abOpcode[offOpcode];
        pIemCpu->offOpcode = offOpcode + 1;
    }
    else
        *pb = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode byte.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu8                 Where to return the opcode byte.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU8(PIEMCPU pIemCpu, uint8_t *pu8)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_LIKELY(offOpcode < pIemCpu->cbOpcode))
    {
        *pu8 = pIemCpu->abOpcode[offOpcode];
        pIemCpu->offOpcode = offOpcode + 1;
        return VINF_SUCCESS;
    }
    return iemOpcodeGetNextU8Slow(pIemCpu, pu8);
}


/**
 * Fetches the next opcode byte, returns automatically on failure.
 *
 * @param   a_pu8               Where to return the opcode byte.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U8(a_pu8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU8(pIemCpu, (a_pu8)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Fetches the next signed byte from the opcode stream.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pi8                 Where to return the signed byte.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8(PIEMCPU pIemCpu, int8_t *pi8)
{
    return iemOpcodeGetNextU8(pIemCpu, (uint8_t *)pi8);
}


/**
 * Fetches the next signed byte from the opcode stream, returning automatically
 * on failure.
 *
 * @param   a_pi8               Where to return the signed byte.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S8(a_pi8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8(pIemCpu, (a_pi8)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextS8SxU16 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the opcode dword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextS8SxU16Slow(PIEMCPU pIemCpu, uint16_t *pu16)
{
    uint8_t      u8;
    VBOXSTRICTRC rcStrict = iemOpcodeGetNextU8Slow(pIemCpu, &u8);
    if (rcStrict == VINF_SUCCESS)
        *pu16 = (int8_t)u8;
    return rcStrict;
}


/**
 * Fetches the next signed byte from the opcode stream, extending it to
 * unsigned 16-bit.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the unsigned word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8SxU16(PIEMCPU pIemCpu, uint16_t *pu16)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode >= pIemCpu->cbOpcode))
        return iemOpcodeGetNextS8SxU16Slow(pIemCpu, pu16);

    *pu16 = (int8_t)pIemCpu->abOpcode[offOpcode];
    pIemCpu->offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
}


/**
 * Fetches the next signed byte from the opcode stream and sign-extending it to
 * a word, returning automatically on failure.
 *
 * @param   a_pu16              Where to return the word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S8_SX_U16(a_pu16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU16(pIemCpu, (a_pu16)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextS8SxU32 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the opcode dword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextS8SxU32Slow(PIEMCPU pIemCpu, uint32_t *pu32)
{
    uint8_t      u8;
    VBOXSTRICTRC rcStrict = iemOpcodeGetNextU8Slow(pIemCpu, &u8);
    if (rcStrict == VINF_SUCCESS)
        *pu32 = (int8_t)u8;
    return rcStrict;
}


/**
 * Fetches the next signed byte from the opcode stream, extending it to
 * unsigned 32-bit.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the unsigned dword.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8SxU32(PIEMCPU pIemCpu, uint32_t *pu32)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode >= pIemCpu->cbOpcode))
        return iemOpcodeGetNextS8SxU32Slow(pIemCpu, pu32);

    *pu32 = (int8_t)pIemCpu->abOpcode[offOpcode];
    pIemCpu->offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
}


/**
 * Fetches the next signed byte from the opcode stream and sign-extending it to
 * a word, returning automatically on failure.
 *
 * @param   a_pu32              Where to return the word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S8_SX_U32(a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU32(pIemCpu, (a_pu32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextS8SxU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode qword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextS8SxU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
{
    uint8_t      u8;
    VBOXSTRICTRC rcStrict = iemOpcodeGetNextU8Slow(pIemCpu, &u8);
    if (rcStrict == VINF_SUCCESS)
        *pu64 = (int8_t)u8;
    return rcStrict;
}


/**
 * Fetches the next signed byte from the opcode stream, extending it to
 * unsigned 64-bit.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the unsigned qword.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS8SxU64(PIEMCPU pIemCpu, uint64_t *pu64)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode >= pIemCpu->cbOpcode))
        return iemOpcodeGetNextS8SxU64Slow(pIemCpu, pu64);

    *pu64 = (int8_t)pIemCpu->abOpcode[offOpcode];
    pIemCpu->offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
}


/**
 * Fetches the next signed byte from the opcode stream and sign-extending it to
 * a word, returning automatically on failure.
 *
 * @param   a_pu64              Where to return the word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S8_SX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU64(pIemCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextU16 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the opcode word.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU16Slow(PIEMCPU pIemCpu, uint16_t *pu16)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 2);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu16 = RT_MAKE_U16(pIemCpu->abOpcode[offOpcode], pIemCpu->abOpcode[offOpcode + 1]);
        pIemCpu->offOpcode = offOpcode + 2;
    }
    else
        *pu16 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the opcode word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU16(PIEMCPU pIemCpu, uint16_t *pu16)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 2 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextU16Slow(pIemCpu, pu16);

    *pu16 = RT_MAKE_U16(pIemCpu->abOpcode[offOpcode], pIemCpu->abOpcode[offOpcode + 1]);
    pIemCpu->offOpcode = offOpcode + 2;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode word, returns automatically on failure.
 *
 * @param   a_pu16              Where to return the opcode word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U16(a_pu16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16(pIemCpu, (a_pu16)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextU16ZxU32 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the opcode double word.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU16ZxU32Slow(PIEMCPU pIemCpu, uint32_t *pu32)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 2);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu32 = RT_MAKE_U16(pIemCpu->abOpcode[offOpcode], pIemCpu->abOpcode[offOpcode + 1]);
        pIemCpu->offOpcode = offOpcode + 2;
    }
    else
        *pu32 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode word, zero extending it to a double word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the opcode double word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU16ZxU32(PIEMCPU pIemCpu, uint32_t *pu32)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 2 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextU16ZxU32Slow(pIemCpu, pu32);

    *pu32 = RT_MAKE_U16(pIemCpu->abOpcode[offOpcode], pIemCpu->abOpcode[offOpcode + 1]);
    pIemCpu->offOpcode = offOpcode + 2;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode word and zero extends it to a double word, returns
 * automatically on failure.
 *
 * @param   a_pu32              Where to return the opcode double word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U16_ZX_U32(a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16ZxU32(pIemCpu, (a_pu32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextU16ZxU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode quad word.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU16ZxU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 2);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu64 = RT_MAKE_U16(pIemCpu->abOpcode[offOpcode], pIemCpu->abOpcode[offOpcode + 1]);
        pIemCpu->offOpcode = offOpcode + 2;
    }
    else
        *pu64 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode word, zero extending it to a quad word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode quad word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU16ZxU64(PIEMCPU pIemCpu, uint64_t *pu64)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 2 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextU16ZxU64Slow(pIemCpu, pu64);

    *pu64 = RT_MAKE_U16(pIemCpu->abOpcode[offOpcode], pIemCpu->abOpcode[offOpcode + 1]);
    pIemCpu->offOpcode = offOpcode + 2;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode word and zero extends it to a quad word, returns
 * automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U16_ZX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16ZxU64(pIemCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Fetches the next signed word from the opcode stream.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pi16                Where to return the signed word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS16(PIEMCPU pIemCpu, int16_t *pi16)
{
    return iemOpcodeGetNextU16(pIemCpu, (uint16_t *)pi16);
}


/**
 * Fetches the next signed word from the opcode stream, returning automatically
 * on failure.
 *
 * @param   a_pi16              Where to return the signed word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S16(a_pi16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS16(pIemCpu, (a_pi16)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextU32 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the opcode dword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU32Slow(PIEMCPU pIemCpu, uint32_t *pu32)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 4);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu32 = RT_MAKE_U32_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                    pIemCpu->abOpcode[offOpcode + 1],
                                    pIemCpu->abOpcode[offOpcode + 2],
                                    pIemCpu->abOpcode[offOpcode + 3]);
        pIemCpu->offOpcode = offOpcode + 4;
    }
    else
        *pu32 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode dword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the opcode double word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU32(PIEMCPU pIemCpu, uint32_t *pu32)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 4 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextU32Slow(pIemCpu, pu32);

    *pu32 = RT_MAKE_U32_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                pIemCpu->abOpcode[offOpcode + 1],
                                pIemCpu->abOpcode[offOpcode + 2],
                                pIemCpu->abOpcode[offOpcode + 3]);
    pIemCpu->offOpcode = offOpcode + 4;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode dword, returns automatically on failure.
 *
 * @param   a_pu32              Where to return the opcode dword.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U32(a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU32(pIemCpu, (a_pu32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextU32ZxU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode dword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU32ZxU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 4);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu64 = RT_MAKE_U32_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                    pIemCpu->abOpcode[offOpcode + 1],
                                    pIemCpu->abOpcode[offOpcode + 2],
                                    pIemCpu->abOpcode[offOpcode + 3]);
        pIemCpu->offOpcode = offOpcode + 4;
    }
    else
        *pu64 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode dword, zero extending it to a quad word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode quad word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU32ZxU64(PIEMCPU pIemCpu, uint64_t *pu64)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 4 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextU32ZxU64Slow(pIemCpu, pu64);

    *pu64 = RT_MAKE_U32_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                pIemCpu->abOpcode[offOpcode + 1],
                                pIemCpu->abOpcode[offOpcode + 2],
                                pIemCpu->abOpcode[offOpcode + 3]);
    pIemCpu->offOpcode = offOpcode + 4;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode dword and zero extends it to a quad word, returns
 * automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U32_ZX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU32ZxU64(pIemCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Fetches the next signed double word from the opcode stream.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pi32                Where to return the signed double word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS32(PIEMCPU pIemCpu, int32_t *pi32)
{
    return iemOpcodeGetNextU32(pIemCpu, (uint32_t *)pi32);
}

/**
 * Fetches the next signed double word from the opcode stream, returning
 * automatically on failure.
 *
 * @param   a_pi32              Where to return the signed double word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S32(a_pi32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS32(pIemCpu, (a_pi32)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextS32SxU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode qword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextS32SxU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 4);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu64 = (int32_t)RT_MAKE_U32_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                             pIemCpu->abOpcode[offOpcode + 1],
                                             pIemCpu->abOpcode[offOpcode + 2],
                                             pIemCpu->abOpcode[offOpcode + 3]);
        pIemCpu->offOpcode = offOpcode + 4;
    }
    else
        *pu64 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode dword, sign extending it into a quad word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode quad word.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextS32SxU64(PIEMCPU pIemCpu, uint64_t *pu64)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 4 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextS32SxU64Slow(pIemCpu, pu64);

    int32_t i32 = RT_MAKE_U32_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                      pIemCpu->abOpcode[offOpcode + 1],
                                      pIemCpu->abOpcode[offOpcode + 2],
                                      pIemCpu->abOpcode[offOpcode + 3]);
    *pu64 = i32;
    pIemCpu->offOpcode = offOpcode + 4;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode double word and sign extends it to a quad word,
 * returns automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_S32_SX_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS32SxU64(pIemCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/**
 * Deals with the problematic cases that iemOpcodeGetNextU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode qword.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemOpcodeGetNextU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
{
    VBOXSTRICTRC rcStrict = iemOpcodeFetchMoreBytes(pIemCpu, 8);
    if (rcStrict == VINF_SUCCESS)
    {
        uint8_t offOpcode = pIemCpu->offOpcode;
        *pu64 = RT_MAKE_U64_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                    pIemCpu->abOpcode[offOpcode + 1],
                                    pIemCpu->abOpcode[offOpcode + 2],
                                    pIemCpu->abOpcode[offOpcode + 3],
                                    pIemCpu->abOpcode[offOpcode + 4],
                                    pIemCpu->abOpcode[offOpcode + 5],
                                    pIemCpu->abOpcode[offOpcode + 6],
                                    pIemCpu->abOpcode[offOpcode + 7]);
        pIemCpu->offOpcode = offOpcode + 8;
    }
    else
        *pu64 = 0;
    return rcStrict;
}


/**
 * Fetches the next opcode qword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode qword.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU64(PIEMCPU pIemCpu, uint64_t *pu64)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode + 8 > pIemCpu->cbOpcode))
        return iemOpcodeGetNextU64Slow(pIemCpu, pu64);

    *pu64 = RT_MAKE_U64_FROM_U8(pIemCpu->abOpcode[offOpcode],
                                pIemCpu->abOpcode[offOpcode + 1],
                                pIemCpu->abOpcode[offOpcode + 2],
                                pIemCpu->abOpcode[offOpcode + 3],
                                pIemCpu->abOpcode[offOpcode + 4],
                                pIemCpu->abOpcode[offOpcode + 5],
                                pIemCpu->abOpcode[offOpcode + 6],
                                pIemCpu->abOpcode[offOpcode + 7]);
    pIemCpu->offOpcode = offOpcode + 8;
    return VINF_SUCCESS;
}


/**
 * Fetches the next opcode quad word, returns automatically on failure.
 *
 * @param   a_pu64              Where to return the opcode quad word.
 * @remark Implicitly references pIemCpu.
 */
#define IEM_OPCODE_GET_NEXT_U64(a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU64(pIemCpu, (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/** @name  Misc Worker Functions.
 * @{
 */


/**
 * Validates a new SS segment.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   NewSS           The new SS selctor.
 * @param   uCpl            The CPL to load the stack for.
 * @param   pDesc           Where to return the descriptor.
 */
IEM_STATIC VBOXSTRICTRC iemMiscValidateNewSS(PIEMCPU pIemCpu, PCCPUMCTX pCtx, RTSEL NewSS, uint8_t uCpl, PIEMSELDESC pDesc)
{
    NOREF(pCtx);

    /* Null selectors are not allowed (we're not called for dispatching
       interrupts with SS=0 in long mode). */
    if (!(NewSS & X86_SEL_MASK_OFF_RPL))
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - null selector -> #TS(0)\n", NewSS));
        return iemRaiseTaskSwitchFault0(pIemCpu);
    }

    /** @todo testcase: check that the TSS.ssX RPL is checked.  Also check when. */
    if ((NewSS & X86_SEL_RPL) != uCpl)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - RPL and CPL (%d) differs -> #TS\n", NewSS, uCpl));
        return iemRaiseTaskSwitchFaultBySelector(pIemCpu, NewSS);
    }

    /*
     * Read the descriptor.
     */
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, pDesc, NewSS, X86_XCPT_TS);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /*
     * Perform the descriptor validation documented for LSS, POP SS and MOV SS.
     */
    if (!pDesc->Legacy.Gen.u1DescType)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - system selector (%#x) -> #TS\n", NewSS, pDesc->Legacy.Gen.u4Type));
        return iemRaiseTaskSwitchFaultBySelector(pIemCpu, NewSS);
    }

    if (    (pDesc->Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
        || !(pDesc->Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - code or read only (%#x) -> #TS\n", NewSS, pDesc->Legacy.Gen.u4Type));
        return iemRaiseTaskSwitchFaultBySelector(pIemCpu, NewSS);
    }
    if (pDesc->Legacy.Gen.u2Dpl != uCpl)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - DPL (%d) and CPL (%d) differs -> #TS\n", NewSS, pDesc->Legacy.Gen.u2Dpl, uCpl));
        return iemRaiseTaskSwitchFaultBySelector(pIemCpu, NewSS);
    }

    /* Is it there? */
    /** @todo testcase: Is this checked before the canonical / limit check below? */
    if (!pDesc->Legacy.Gen.u1Present)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - segment not present -> #NP\n", NewSS));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, NewSS);
    }

    return VINF_SUCCESS;
}


/**
 * Gets the correct EFLAGS regardless of whether PATM stores parts of them or
 * not.
 *
 * @param   a_pIemCpu           The IEM per CPU data.
 * @param   a_pCtx              The CPU context.
 */
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
# define IEMMISC_GET_EFL(a_pIemCpu, a_pCtx) \
    ( IEM_VERIFICATION_ENABLED(a_pIemCpu) \
      ? (a_pCtx)->eflags.u \
      : CPUMRawGetEFlags(IEMCPU_TO_VMCPU(a_pIemCpu)) )
#else
# define IEMMISC_GET_EFL(a_pIemCpu, a_pCtx) \
    ( (a_pCtx)->eflags.u  )
#endif

/**
 * Updates the EFLAGS in the correct manner wrt. PATM.
 *
 * @param   a_pIemCpu           The IEM per CPU data.
 * @param   a_pCtx              The CPU context.
 * @param   a_fEfl              The new EFLAGS.
 */
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
# define IEMMISC_SET_EFL(a_pIemCpu, a_pCtx, a_fEfl) \
    do { \
        if (IEM_VERIFICATION_ENABLED(a_pIemCpu)) \
            (a_pCtx)->eflags.u = (a_fEfl); \
        else \
            CPUMRawSetEFlags(IEMCPU_TO_VMCPU(a_pIemCpu), a_fEfl); \
    } while (0)
#else
# define IEMMISC_SET_EFL(a_pIemCpu, a_pCtx, a_fEfl) \
    do { \
        (a_pCtx)->eflags.u = (a_fEfl); \
    } while (0)
#endif


/** @}  */

/** @name  Raising Exceptions.
 *
 * @{
 */

/** @name IEM_XCPT_FLAGS_XXX - flags for iemRaiseXcptOrInt.
 * @{ */
/** CPU exception. */
#define IEM_XCPT_FLAGS_T_CPU_XCPT       RT_BIT_32(0)
/** External interrupt (from PIC, APIC, whatever). */
#define IEM_XCPT_FLAGS_T_EXT_INT        RT_BIT_32(1)
/** Software interrupt (int or into, not bound).
 * Returns to the following instruction */
#define IEM_XCPT_FLAGS_T_SOFT_INT       RT_BIT_32(2)
/** Takes an error code. */
#define IEM_XCPT_FLAGS_ERR              RT_BIT_32(3)
/** Takes a CR2. */
#define IEM_XCPT_FLAGS_CR2              RT_BIT_32(4)
/** Generated by the breakpoint instruction. */
#define IEM_XCPT_FLAGS_BP_INSTR         RT_BIT_32(5)
/** Generated by a DRx instruction breakpoint and RF should be cleared. */
#define IEM_XCPT_FLAGS_DRx_INSTR_BP     RT_BIT_32(6)
/** @}  */


/**
 * Loads the specified stack far pointer from the TSS.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   uCpl            The CPL to load the stack for.
 * @param   pSelSS          Where to return the new stack segment.
 * @param   puEsp           Where to return the new stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemRaiseLoadStackFromTss32Or16(PIEMCPU pIemCpu, PCCPUMCTX pCtx, uint8_t uCpl,
                                                       PRTSEL pSelSS, uint32_t *puEsp)
{
    VBOXSTRICTRC rcStrict;
    Assert(uCpl < 4);

    switch (pCtx->tr.Attr.n.u4Type)
    {
        /*
         * 16-bit TSS (X86TSS16).
         */
        case X86_SEL_TYPE_SYS_286_TSS_AVAIL: AssertFailed();
        case X86_SEL_TYPE_SYS_286_TSS_BUSY:
        {
            uint32_t off = uCpl * 4 + 2;
            if (off + 4 <= pCtx->tr.u32Limit)
            {
                /** @todo check actual access pattern here. */
                uint32_t u32Tmp = 0; /* gcc maybe... */
                rcStrict = iemMemFetchSysU32(pIemCpu, &u32Tmp, UINT8_MAX, pCtx->tr.u64Base + off);
                if (rcStrict == VINF_SUCCESS)
                {
                    *puEsp  = RT_LOWORD(u32Tmp);
                    *pSelSS = RT_HIWORD(u32Tmp);
                    return VINF_SUCCESS;
                }
            }
            else
            {
                Log(("LoadStackFromTss32Or16: out of bounds! uCpl=%d, u32Limit=%#x TSS16\n", uCpl, pCtx->tr.u32Limit));
                rcStrict = iemRaiseTaskSwitchFaultCurrentTSS(pIemCpu);
            }
            break;
        }

        /*
         * 32-bit TSS (X86TSS32).
         */
        case X86_SEL_TYPE_SYS_386_TSS_AVAIL: AssertFailed();
        case X86_SEL_TYPE_SYS_386_TSS_BUSY:
        {
            uint32_t off = uCpl * 8 + 4;
            if (off + 7 <= pCtx->tr.u32Limit)
            {
/** @todo check actual access pattern here. */
                uint64_t u64Tmp;
                rcStrict = iemMemFetchSysU64(pIemCpu, &u64Tmp, UINT8_MAX, pCtx->tr.u64Base + off);
                if (rcStrict == VINF_SUCCESS)
                {
                    *puEsp  = u64Tmp & UINT32_MAX;
                    *pSelSS = (RTSEL)(u64Tmp >> 32);
                    return VINF_SUCCESS;
                }
            }
            else
            {
                Log(("LoadStackFromTss32Or16: out of bounds! uCpl=%d, u32Limit=%#x TSS16\n", uCpl, pCtx->tr.u32Limit));
                rcStrict = iemRaiseTaskSwitchFaultCurrentTSS(pIemCpu);
            }
            break;
        }

        default:
            AssertFailed();
            rcStrict = VERR_IEM_IPE_4;
            break;
    }

    *puEsp  = 0; /* make gcc happy */
    *pSelSS = 0; /* make gcc happy */
    return rcStrict;
}


/**
 * Loads the specified stack pointer from the 64-bit TSS.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   uCpl            The CPL to load the stack for.
 * @param   uIst            The interrupt stack table index, 0 if to use uCpl.
 * @param   puRsp           Where to return the new stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemRaiseLoadStackFromTss64(PIEMCPU pIemCpu, PCCPUMCTX pCtx, uint8_t uCpl, uint8_t uIst, uint64_t *puRsp)
{
    Assert(uCpl < 4);
    Assert(uIst < 8);
    *puRsp  = 0; /* make gcc happy */

    AssertReturn(pCtx->tr.Attr.n.u4Type == AMD64_SEL_TYPE_SYS_TSS_BUSY, VERR_IEM_IPE_5);

    uint32_t off;
    if (uIst)
        off = (uIst - 1) * sizeof(uint64_t) + RT_OFFSETOF(X86TSS64, ist1);
    else
        off = uCpl * sizeof(uint64_t) + RT_OFFSETOF(X86TSS64, rsp0);
    if (off + sizeof(uint64_t) > pCtx->tr.u32Limit)
    {
        Log(("iemRaiseLoadStackFromTss64: out of bounds! uCpl=%d uIst=%d, u32Limit=%#x\n", uCpl, uIst, pCtx->tr.u32Limit));
        return iemRaiseTaskSwitchFaultCurrentTSS(pIemCpu);
    }

    return iemMemFetchSysU64(pIemCpu, puRsp, UINT8_MAX, pCtx->tr.u64Base + off);
}


/**
 * Adjust the CPU state according to the exception being raised.
 *
 * @param   pCtx                The CPU context.
 * @param   u8Vector            The exception that has been raised.
 */
DECLINLINE(void) iemRaiseXcptAdjustState(PCPUMCTX pCtx, uint8_t u8Vector)
{
    switch (u8Vector)
    {
        case X86_XCPT_DB:
            pCtx->dr[7] &= ~X86_DR7_GD;
            break;
        /** @todo Read the AMD and Intel exception reference... */
    }
}


/**
 * Implements exceptions and interrupts for real mode.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   cbInstr         The number of bytes to offset rIP by in the return
 *                          address.
 * @param   u8Vector        The interrupt / exception vector number.
 * @param   fFlags          The flags.
 * @param   uErr            The error value if IEM_XCPT_FLAGS_ERR is set.
 * @param   uCr2            The CR2 value if IEM_XCPT_FLAGS_CR2 is set.
 */
IEM_STATIC VBOXSTRICTRC
iemRaiseXcptOrIntInRealMode(PIEMCPU     pIemCpu,
                            PCPUMCTX    pCtx,
                            uint8_t     cbInstr,
                            uint8_t     u8Vector,
                            uint32_t    fFlags,
                            uint16_t    uErr,
                            uint64_t    uCr2)
{
    AssertReturn(pIemCpu->enmCpuMode == IEMMODE_16BIT, VERR_IEM_IPE_6);
    NOREF(uErr); NOREF(uCr2);

    /*
     * Read the IDT entry.
     */
    if (pCtx->idtr.cbIdt < UINT32_C(4) * u8Vector + 3)
    {
        Log(("RaiseXcptOrIntInRealMode: %#x is out of bounds (%#x)\n", u8Vector, pCtx->idtr.cbIdt));
        return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }
    RTFAR16 Idte;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU32(pIemCpu, (uint32_t *)&Idte, UINT8_MAX,
                                               pCtx->idtr.pIdt + UINT32_C(4) * u8Vector);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        return rcStrict;

    /*
     * Push the stack frame.
     */
    uint16_t *pu16Frame;
    uint64_t  uNewRsp;
    rcStrict = iemMemStackPushBeginSpecial(pIemCpu, 6, (void **)&pu16Frame, &uNewRsp);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    uint32_t fEfl = IEMMISC_GET_EFL(pIemCpu, pCtx);
#if IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC
    AssertCompile(IEMTARGETCPU_8086 <= IEMTARGETCPU_186 && IEMTARGETCPU_V20 <= IEMTARGETCPU_186 && IEMTARGETCPU_286 > IEMTARGETCPU_186);
    if (pIemCpu->uTargetCpu <= IEMTARGETCPU_186)
        fEfl |= UINT16_C(0xf000);
#endif
    pu16Frame[2] = (uint16_t)fEfl;
    pu16Frame[1] = (uint16_t)pCtx->cs.Sel;
    pu16Frame[0] = (fFlags & IEM_XCPT_FLAGS_T_SOFT_INT) ? pCtx->ip + cbInstr : pCtx->ip;
    rcStrict = iemMemStackPushCommitSpecial(pIemCpu, pu16Frame, uNewRsp);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        return rcStrict;

    /*
     * Load the vector address into cs:ip and make exception specific state
     * adjustments.
     */
    pCtx->cs.Sel           = Idte.sel;
    pCtx->cs.ValidSel      = Idte.sel;
    pCtx->cs.fFlags        = CPUMSELREG_FLAGS_VALID;
    pCtx->cs.u64Base       = (uint32_t)Idte.sel << 4;
    /** @todo do we load attribs and limit as well? Should we check against limit like far jump? */
    pCtx->rip              = Idte.off;
    fEfl &= ~X86_EFL_IF;
    IEMMISC_SET_EFL(pIemCpu, pCtx, fEfl);

    /** @todo do we actually do this in real mode? */
    if (fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT)
        iemRaiseXcptAdjustState(pCtx, u8Vector);

    return fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT ? VINF_IEM_RAISED_XCPT : VINF_SUCCESS;
}


/**
 * Loads a NULL data selector into when coming from V8086 mode.
 *
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pSReg           Pointer to the segment register.
 */
IEM_STATIC void iemHlpLoadNullDataSelectorOnV86Xcpt(PIEMCPU pIemCpu, PCPUMSELREG pSReg)
{
    pSReg->Sel      = 0;
    pSReg->ValidSel = 0;
    if (IEM_IS_GUEST_CPU_INTEL(pIemCpu) && !IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
    {
        /* VT-x (Intel 3960x) doesn't change the base and limit, clears and sets the following attributes */
        pSReg->Attr.u &= X86DESCATTR_DT | X86DESCATTR_TYPE | X86DESCATTR_DPL | X86DESCATTR_G | X86DESCATTR_D;
        pSReg->Attr.u |= X86DESCATTR_UNUSABLE;
    }
    else
    {
        pSReg->fFlags   = CPUMSELREG_FLAGS_VALID;
        /** @todo check this on AMD-V */
        pSReg->u64Base  = 0;
        pSReg->u32Limit = 0;
    }
}


/**
 * Loads a segment selector during a task switch in V8086 mode.
 *
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pSReg           Pointer to the segment register.
 * @param   uSel            The selector value to load.
 */
IEM_STATIC void iemHlpLoadSelectorInV86Mode(PIEMCPU pIemCpu, PCPUMSELREG pSReg, uint16_t uSel)
{
    /* See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers". */
    pSReg->Sel      = uSel;
    pSReg->ValidSel = uSel;
    pSReg->fFlags   = CPUMSELREG_FLAGS_VALID;
    pSReg->u64Base  = uSel << 4;
    pSReg->u32Limit = 0xffff;
    pSReg->Attr.u   = 0xf3;
}


/**
 * Loads a NULL data selector into a selector register, both the hidden and
 * visible parts, in protected mode.
 *
 * @param   pIemCpu             The IEM state of the calling EMT.
 * @param   pSReg               Pointer to the segment register.
 * @param   uRpl                The RPL.
 */
IEM_STATIC void iemHlpLoadNullDataSelectorProt(PIEMCPU pIemCpu, PCPUMSELREG pSReg, RTSEL uRpl)
{
    /** @todo Testcase: write a testcase checking what happends when loading a NULL
     *        data selector in protected mode. */
    pSReg->Sel      = uRpl;
    pSReg->ValidSel = uRpl;
    pSReg->fFlags   = CPUMSELREG_FLAGS_VALID;
    if (IEM_IS_GUEST_CPU_INTEL(pIemCpu) && !IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
    {
        /* VT-x (Intel 3960x) observed doing something like this. */
        pSReg->Attr.u   = X86DESCATTR_UNUSABLE | X86DESCATTR_G | X86DESCATTR_D | (pIemCpu->uCpl << X86DESCATTR_DPL_SHIFT);
        pSReg->u32Limit = UINT32_MAX;
        pSReg->u64Base  = 0;
    }
    else
    {
        pSReg->Attr.u   = X86DESCATTR_UNUSABLE;
        pSReg->u32Limit = 0;
        pSReg->u64Base  = 0;
    }
}


/**
 * Loads a segment selector during a task switch in protected mode.
 *
 * In this task switch scenario, we would throw \#TS exceptions rather than
 * \#GPs.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pSReg           Pointer to the segment register.
 * @param   uSel            The new selector value.
 *
 * @remarks This does _not_ handle CS or SS.
 * @remarks This expects pIemCpu->uCpl to be up to date.
 */
IEM_STATIC VBOXSTRICTRC iemHlpTaskSwitchLoadDataSelectorInProtMode(PIEMCPU pIemCpu, PCPUMSELREG pSReg, uint16_t uSel)
{
    Assert(pIemCpu->enmCpuMode != IEMMODE_64BIT);

    /* Null data selector. */
    if (!(uSel & X86_SEL_MASK_OFF_RPL))
    {
        iemHlpLoadNullDataSelectorProt(pIemCpu, pSReg, uSel);
        Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), pSReg));
        CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_HIDDEN_SEL_REGS);
        return VINF_SUCCESS;
    }

    /* Fetch the descriptor. */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uSel, X86_XCPT_TS);
    if (rcStrict != VINF_SUCCESS)
    {
        Log(("iemHlpTaskSwitchLoadDataSelectorInProtMode: failed to fetch selector. uSel=%u rc=%Rrc\n", uSel,
             VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    /* Must be a data segment or readable code segment. */
    if (   !Desc.Legacy.Gen.u1DescType
        || (Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
    {
        Log(("iemHlpTaskSwitchLoadDataSelectorInProtMode: invalid segment type. uSel=%u Desc.u4Type=%#x\n", uSel,
             Desc.Legacy.Gen.u4Type));
        return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uSel & X86_SEL_MASK_OFF_RPL);
    }

    /* Check privileges for data segments and non-conforming code segments. */
    if (   (Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
        != (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
    {
        /* The RPL and the new CPL must be less than or equal to the DPL. */
        if (   (unsigned)(uSel & X86_SEL_RPL) > Desc.Legacy.Gen.u2Dpl
            || (pIemCpu->uCpl > Desc.Legacy.Gen.u2Dpl))
        {
            Log(("iemHlpTaskSwitchLoadDataSelectorInProtMode: Invalid priv. uSel=%u uSel.RPL=%u DPL=%u CPL=%u\n",
                 uSel, (uSel & X86_SEL_RPL), Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uSel & X86_SEL_MASK_OFF_RPL);
        }
    }

    /* Is it there? */
    if (!Desc.Legacy.Gen.u1Present)
    {
        Log(("iemHlpTaskSwitchLoadDataSelectorInProtMode: Segment not present. uSel=%u\n", uSel));
        return iemRaiseSelectorNotPresentWithErr(pIemCpu, uSel & X86_SEL_MASK_OFF_RPL);
    }

    /* The base and limit. */
    uint32_t cbLimit = X86DESC_LIMIT_G(&Desc.Legacy);
    uint64_t u64Base = X86DESC_BASE(&Desc.Legacy);

    /*
     * Ok, everything checked out fine. Now set the accessed bit before
     * committing the result into the registers.
     */
    if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
    {
        rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uSel);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
    }

    /* Commit */
    pSReg->Sel      = uSel;
    pSReg->Attr.u   = X86DESC_GET_HID_ATTR(&Desc.Legacy);
    pSReg->u32Limit = cbLimit;
    pSReg->u64Base  = u64Base;  /** @todo testcase/investigate: seen claims that the upper half of the base remains unchanged... */
    pSReg->ValidSel = uSel;
    pSReg->fFlags   = CPUMSELREG_FLAGS_VALID;
    if (IEM_IS_GUEST_CPU_INTEL(pIemCpu) && !IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
        pSReg->Attr.u &= ~X86DESCATTR_UNUSABLE;

    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), pSReg));
    CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_HIDDEN_SEL_REGS);
    return VINF_SUCCESS;
}


/**
 * Performs a task switch.
 *
 * If the task switch is the result of a JMP, CALL or IRET instruction, the
 * caller is responsible for performing the necessary checks (like DPL, TSS
 * present etc.) which are specific to JMP/CALL/IRET. See Intel Instruction
 * reference for JMP, CALL, IRET.
 *
 * If the task switch is the due to a software interrupt or hardware exception,
 * the caller is responsible for validating the TSS selector and descriptor. See
 * Intel Instruction reference for INT n.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   enmTaskSwitch   What caused this task switch.
 * @param   uNextEip        The EIP effective after the task switch.
 * @param   fFlags          The flags.
 * @param   uErr            The error value if IEM_XCPT_FLAGS_ERR is set.
 * @param   uCr2            The CR2 value if IEM_XCPT_FLAGS_CR2 is set.
 * @param   SelTSS          The TSS selector of the new task.
 * @param   pNewDescTSS     Pointer to the new TSS descriptor.
 */
IEM_STATIC VBOXSTRICTRC
iemTaskSwitch(PIEMCPU         pIemCpu,
              PCPUMCTX        pCtx,
              IEMTASKSWITCH   enmTaskSwitch,
              uint32_t        uNextEip,
              uint32_t        fFlags,
              uint16_t        uErr,
              uint64_t        uCr2,
              RTSEL           SelTSS,
              PIEMSELDESC     pNewDescTSS)
{
    Assert(!IEM_IS_REAL_MODE(pIemCpu));
    Assert(pIemCpu->enmCpuMode != IEMMODE_64BIT);

    uint32_t const uNewTSSType = pNewDescTSS->Legacy.Gate.u4Type;
    Assert(   uNewTSSType == X86_SEL_TYPE_SYS_286_TSS_AVAIL
           || uNewTSSType == X86_SEL_TYPE_SYS_286_TSS_BUSY
           || uNewTSSType == X86_SEL_TYPE_SYS_386_TSS_AVAIL
           || uNewTSSType == X86_SEL_TYPE_SYS_386_TSS_BUSY);

    bool const fIsNewTSS386 = (   uNewTSSType == X86_SEL_TYPE_SYS_386_TSS_AVAIL
                               || uNewTSSType == X86_SEL_TYPE_SYS_386_TSS_BUSY);

    Log(("iemTaskSwitch: enmTaskSwitch=%u NewTSS=%#x fIsNewTSS386=%RTbool EIP=%#RGv uNextEip=%#RGv\n", enmTaskSwitch, SelTSS,
         fIsNewTSS386, pCtx->eip, uNextEip));

    /* Update CR2 in case it's a page-fault. */
    /** @todo This should probably be done much earlier in IEM/PGM. See
     *        @bugref{5653#c49}. */
    if (fFlags & IEM_XCPT_FLAGS_CR2)
        pCtx->cr2 = uCr2;

    /*
     * Check the new TSS limit. See Intel spec. 6.15 "Exception and Interrupt Reference"
     * subsection "Interrupt 10 - Invalid TSS Exception (#TS)".
     */
    uint32_t const uNewTSSLimit    = pNewDescTSS->Legacy.Gen.u16LimitLow | (pNewDescTSS->Legacy.Gen.u4LimitHigh << 16);
    uint32_t const uNewTSSLimitMin = fIsNewTSS386 ? X86_SEL_TYPE_SYS_386_TSS_LIMIT_MIN : X86_SEL_TYPE_SYS_286_TSS_LIMIT_MIN;
    if (uNewTSSLimit < uNewTSSLimitMin)
    {
        Log(("iemTaskSwitch: Invalid new TSS limit. enmTaskSwitch=%u uNewTSSLimit=%#x uNewTSSLimitMin=%#x -> #TS\n",
             enmTaskSwitch, uNewTSSLimit, uNewTSSLimitMin));
        return iemRaiseTaskSwitchFaultWithErr(pIemCpu, SelTSS & X86_SEL_MASK_OFF_RPL);
    }

    /*
     * Check the current TSS limit. The last written byte to the current TSS during the
     * task switch will be 2 bytes at offset 0x5C (32-bit) and 1 byte at offset 0x28 (16-bit).
     * See Intel spec. 7.2.1 "Task-State Segment (TSS)" for static and dynamic fields.
     *
     * The AMD docs doesn't mention anything about limit checks with LTR which suggests you can
     * end up with smaller than "legal" TSS limits.
     */
    uint32_t const uCurTSSLimit    = pCtx->tr.u32Limit;
    uint32_t const uCurTSSLimitMin = fIsNewTSS386 ? 0x5F : 0x29;
    if (uCurTSSLimit < uCurTSSLimitMin)
    {
        Log(("iemTaskSwitch: Invalid current TSS limit. enmTaskSwitch=%u uCurTSSLimit=%#x uCurTSSLimitMin=%#x -> #TS\n",
             enmTaskSwitch, uCurTSSLimit, uCurTSSLimitMin));
        return iemRaiseTaskSwitchFaultWithErr(pIemCpu, SelTSS & X86_SEL_MASK_OFF_RPL);
    }

    /*
     * Verify that the new TSS can be accessed and map it. Map only the required contents
     * and not the entire TSS.
     */
    void     *pvNewTSS;
    uint32_t  cbNewTSS    = uNewTSSLimitMin + 1;
    RTGCPTR   GCPtrNewTSS = X86DESC_BASE(&pNewDescTSS->Legacy);
    AssertCompile(sizeof(X86TSS32) == X86_SEL_TYPE_SYS_386_TSS_LIMIT_MIN + 1);
    /** @todo Handle if the TSS crosses a page boundary. Intel specifies that it may
     *        not perform correct translation if this happens. See Intel spec. 7.2.1
     *        "Task-State Segment" */
    VBOXSTRICTRC rcStrict = iemMemMap(pIemCpu, &pvNewTSS, cbNewTSS, UINT8_MAX, GCPtrNewTSS, IEM_ACCESS_SYS_RW);
    if (rcStrict != VINF_SUCCESS)
    {
        Log(("iemTaskSwitch: Failed to read new TSS. enmTaskSwitch=%u cbNewTSS=%u uNewTSSLimit=%u rc=%Rrc\n", enmTaskSwitch,
             cbNewTSS, uNewTSSLimit, VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    /*
     * Clear the busy bit in current task's TSS descriptor if it's a task switch due to JMP/IRET.
     */
    uint32_t u32EFlags = pCtx->eflags.u32;
    if (   enmTaskSwitch == IEMTASKSWITCH_JUMP
        || enmTaskSwitch == IEMTASKSWITCH_IRET)
    {
        PX86DESC pDescCurTSS;
        rcStrict = iemMemMap(pIemCpu, (void **)&pDescCurTSS, sizeof(*pDescCurTSS), UINT8_MAX,
                             pCtx->gdtr.pGdt + (pCtx->tr.Sel & X86_SEL_MASK), IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to read new TSS descriptor in GDT. enmTaskSwitch=%u pGdt=%#RX64 rc=%Rrc\n",
                 enmTaskSwitch, pCtx->gdtr.pGdt, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        pDescCurTSS->Gate.u4Type &= ~X86_SEL_TYPE_SYS_TSS_BUSY_MASK;
        rcStrict = iemMemCommitAndUnmap(pIemCpu, pDescCurTSS, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to commit new TSS descriptor in GDT. enmTaskSwitch=%u pGdt=%#RX64 rc=%Rrc\n",
                 enmTaskSwitch, pCtx->gdtr.pGdt, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* Clear EFLAGS.NT (Nested Task) in the eflags memory image, if it's a task switch due to an IRET. */
        if (enmTaskSwitch == IEMTASKSWITCH_IRET)
        {
            Assert(   uNewTSSType == X86_SEL_TYPE_SYS_286_TSS_BUSY
                   || uNewTSSType == X86_SEL_TYPE_SYS_386_TSS_BUSY);
            u32EFlags &= ~X86_EFL_NT;
        }
    }

    /*
     * Save the CPU state into the current TSS.
     */
    RTGCPTR GCPtrCurTSS = pCtx->tr.u64Base;
    if (GCPtrNewTSS == GCPtrCurTSS)
    {
        Log(("iemTaskSwitch: Switching to the same TSS! enmTaskSwitch=%u GCPtr[Cur|New]TSS=%#RGv\n", enmTaskSwitch, GCPtrCurTSS));
        Log(("uCurCr3=%#x uCurEip=%#x uCurEflags=%#x uCurEax=%#x uCurEsp=%#x uCurEbp=%#x uCurCS=%#04x uCurSS=%#04x uCurLdt=%#x\n",
             pCtx->cr3, pCtx->eip, pCtx->eflags.u32, pCtx->eax, pCtx->esp, pCtx->ebp, pCtx->cs.Sel, pCtx->ss.Sel, pCtx->ldtr.Sel));
    }
    if (fIsNewTSS386)
    {
        /*
         * Verify that the current TSS (32-bit) can be accessed, only the minimum required size.
         * See Intel spec. 7.2.1 "Task-State Segment (TSS)" for static and dynamic fields.
         */
        void    *pvCurTSS32;
        uint32_t offCurTSS = RT_OFFSETOF(X86TSS32, eip);
        uint32_t cbCurTSS  = RT_OFFSETOF(X86TSS32, selLdt) - RT_OFFSETOF(X86TSS32, eip);
        AssertCompile(RTASSERT_OFFSET_OF(X86TSS32, selLdt) - RTASSERT_OFFSET_OF(X86TSS32, eip) == 64);
        rcStrict = iemMemMap(pIemCpu, &pvCurTSS32, cbCurTSS, UINT8_MAX, GCPtrCurTSS + offCurTSS, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to read current 32-bit TSS. enmTaskSwitch=%u GCPtrCurTSS=%#RGv cb=%u rc=%Rrc\n",
                 enmTaskSwitch, GCPtrCurTSS, cbCurTSS, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* !! WARNING !! Access -only- the members (dynamic fields) that are mapped, i.e interval [offCurTSS..cbCurTSS). */
        PX86TSS32 pCurTSS32 = (PX86TSS32)((uintptr_t)pvCurTSS32 - offCurTSS);
        pCurTSS32->eip    = uNextEip;
        pCurTSS32->eflags = u32EFlags;
        pCurTSS32->eax    = pCtx->eax;
        pCurTSS32->ecx    = pCtx->ecx;
        pCurTSS32->edx    = pCtx->edx;
        pCurTSS32->ebx    = pCtx->ebx;
        pCurTSS32->esp    = pCtx->esp;
        pCurTSS32->ebp    = pCtx->ebp;
        pCurTSS32->esi    = pCtx->esi;
        pCurTSS32->edi    = pCtx->edi;
        pCurTSS32->es     = pCtx->es.Sel;
        pCurTSS32->cs     = pCtx->cs.Sel;
        pCurTSS32->ss     = pCtx->ss.Sel;
        pCurTSS32->ds     = pCtx->ds.Sel;
        pCurTSS32->fs     = pCtx->fs.Sel;
        pCurTSS32->gs     = pCtx->gs.Sel;

        rcStrict = iemMemCommitAndUnmap(pIemCpu, pvCurTSS32, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to commit current 32-bit TSS. enmTaskSwitch=%u rc=%Rrc\n", enmTaskSwitch,
                 VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }
    }
    else
    {
        /*
         * Verify that the current TSS (16-bit) can be accessed. Again, only the minimum required size.
         */
        void    *pvCurTSS16;
        uint32_t offCurTSS = RT_OFFSETOF(X86TSS16, ip);
        uint32_t cbCurTSS  = RT_OFFSETOF(X86TSS16, selLdt) - RT_OFFSETOF(X86TSS16, ip);
        AssertCompile(RTASSERT_OFFSET_OF(X86TSS16, selLdt) - RTASSERT_OFFSET_OF(X86TSS16, ip) == 28);
        rcStrict = iemMemMap(pIemCpu, &pvCurTSS16, cbCurTSS, UINT8_MAX, GCPtrCurTSS + offCurTSS, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to read current 16-bit TSS. enmTaskSwitch=%u GCPtrCurTSS=%#RGv cb=%u rc=%Rrc\n",
                 enmTaskSwitch, GCPtrCurTSS, cbCurTSS, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* !! WARNING !! Access -only- the members (dynamic fields) that are mapped, i.e interval [offCurTSS..cbCurTSS). */
        PX86TSS16 pCurTSS16 = (PX86TSS16)((uintptr_t)pvCurTSS16 - offCurTSS);
        pCurTSS16->ip    = uNextEip;
        pCurTSS16->flags = u32EFlags;
        pCurTSS16->ax    = pCtx->ax;
        pCurTSS16->cx    = pCtx->cx;
        pCurTSS16->dx    = pCtx->dx;
        pCurTSS16->bx    = pCtx->bx;
        pCurTSS16->sp    = pCtx->sp;
        pCurTSS16->bp    = pCtx->bp;
        pCurTSS16->si    = pCtx->si;
        pCurTSS16->di    = pCtx->di;
        pCurTSS16->es    = pCtx->es.Sel;
        pCurTSS16->cs    = pCtx->cs.Sel;
        pCurTSS16->ss    = pCtx->ss.Sel;
        pCurTSS16->ds    = pCtx->ds.Sel;

        rcStrict = iemMemCommitAndUnmap(pIemCpu, pvCurTSS16, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to commit current 16-bit TSS. enmTaskSwitch=%u rc=%Rrc\n", enmTaskSwitch,
                 VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }
    }

    /*
     * Update the previous task link field for the new TSS, if the task switch is due to a CALL/INT_XCPT.
     */
    if (   enmTaskSwitch == IEMTASKSWITCH_CALL
        || enmTaskSwitch == IEMTASKSWITCH_INT_XCPT)
    {
        /* 16 or 32-bit TSS doesn't matter, we only access the first, common 16-bit field (selPrev) here. */
        PX86TSS32 pNewTSS = (PX86TSS32)pvNewTSS;
        pNewTSS->selPrev  = pCtx->tr.Sel;
    }

    /*
     * Read the state from the new TSS into temporaries. Setting it immediately as the new CPU state is tricky,
     * it's done further below with error handling (e.g. CR3 changes will go through PGM).
     */
    uint32_t uNewCr3, uNewEip, uNewEflags, uNewEax, uNewEcx, uNewEdx, uNewEbx, uNewEsp, uNewEbp, uNewEsi, uNewEdi;
    uint16_t uNewES,  uNewCS, uNewSS, uNewDS, uNewFS, uNewGS, uNewLdt;
    bool     fNewDebugTrap;
    if (fIsNewTSS386)
    {
        PX86TSS32 pNewTSS32 = (PX86TSS32)pvNewTSS;
        uNewCr3       = (pCtx->cr0 & X86_CR0_PG) ? pNewTSS32->cr3 : 0;
        uNewEip       = pNewTSS32->eip;
        uNewEflags    = pNewTSS32->eflags;
        uNewEax       = pNewTSS32->eax;
        uNewEcx       = pNewTSS32->ecx;
        uNewEdx       = pNewTSS32->edx;
        uNewEbx       = pNewTSS32->ebx;
        uNewEsp       = pNewTSS32->esp;
        uNewEbp       = pNewTSS32->ebp;
        uNewEsi       = pNewTSS32->esi;
        uNewEdi       = pNewTSS32->edi;
        uNewES        = pNewTSS32->es;
        uNewCS        = pNewTSS32->cs;
        uNewSS        = pNewTSS32->ss;
        uNewDS        = pNewTSS32->ds;
        uNewFS        = pNewTSS32->fs;
        uNewGS        = pNewTSS32->gs;
        uNewLdt       = pNewTSS32->selLdt;
        fNewDebugTrap = RT_BOOL(pNewTSS32->fDebugTrap);
    }
    else
    {
        PX86TSS16 pNewTSS16 = (PX86TSS16)pvNewTSS;
        uNewCr3       = 0;
        uNewEip       = pNewTSS16->ip;
        uNewEflags    = pNewTSS16->flags;
        uNewEax       = UINT32_C(0xffff0000) | pNewTSS16->ax;
        uNewEcx       = UINT32_C(0xffff0000) | pNewTSS16->cx;
        uNewEdx       = UINT32_C(0xffff0000) | pNewTSS16->dx;
        uNewEbx       = UINT32_C(0xffff0000) | pNewTSS16->bx;
        uNewEsp       = UINT32_C(0xffff0000) | pNewTSS16->sp;
        uNewEbp       = UINT32_C(0xffff0000) | pNewTSS16->bp;
        uNewEsi       = UINT32_C(0xffff0000) | pNewTSS16->si;
        uNewEdi       = UINT32_C(0xffff0000) | pNewTSS16->di;
        uNewES        = pNewTSS16->es;
        uNewCS        = pNewTSS16->cs;
        uNewSS        = pNewTSS16->ss;
        uNewDS        = pNewTSS16->ds;
        uNewFS        = 0;
        uNewGS        = 0;
        uNewLdt       = pNewTSS16->selLdt;
        fNewDebugTrap = false;
    }

    if (GCPtrNewTSS == GCPtrCurTSS)
        Log(("uNewCr3=%#x uNewEip=%#x uNewEflags=%#x uNewEax=%#x uNewEsp=%#x uNewEbp=%#x uNewCS=%#04x uNewSS=%#04x uNewLdt=%#x\n",
             uNewCr3, uNewEip, uNewEflags, uNewEax, uNewEsp, uNewEbp, uNewCS, uNewSS, uNewLdt));

    /*
     * We're done accessing the new TSS.
     */
    rcStrict = iemMemCommitAndUnmap(pIemCpu, pvNewTSS, IEM_ACCESS_SYS_RW);
    if (rcStrict != VINF_SUCCESS)
    {
        Log(("iemTaskSwitch: Failed to commit new TSS. enmTaskSwitch=%u rc=%Rrc\n", enmTaskSwitch, VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    /*
     * Set the busy bit in the new TSS descriptor, if the task switch is a JMP/CALL/INT_XCPT.
     */
    if (enmTaskSwitch != IEMTASKSWITCH_IRET)
    {
        rcStrict = iemMemMap(pIemCpu, (void **)&pNewDescTSS, sizeof(*pNewDescTSS), UINT8_MAX,
                             pCtx->gdtr.pGdt + (SelTSS & X86_SEL_MASK), IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to read new TSS descriptor in GDT (2). enmTaskSwitch=%u pGdt=%#RX64 rc=%Rrc\n",
                 enmTaskSwitch, pCtx->gdtr.pGdt, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* Check that the descriptor indicates the new TSS is available (not busy). */
        AssertMsg(   pNewDescTSS->Legacy.Gate.u4Type == X86_SEL_TYPE_SYS_286_TSS_AVAIL
                  || pNewDescTSS->Legacy.Gate.u4Type == X86_SEL_TYPE_SYS_386_TSS_AVAIL,
                     ("Invalid TSS descriptor type=%#x", pNewDescTSS->Legacy.Gate.u4Type));

        pNewDescTSS->Legacy.Gate.u4Type |= X86_SEL_TYPE_SYS_TSS_BUSY_MASK;
        rcStrict = iemMemCommitAndUnmap(pIemCpu, pNewDescTSS, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Failed to commit new TSS descriptor in GDT (2). enmTaskSwitch=%u pGdt=%#RX64 rc=%Rrc\n",
                 enmTaskSwitch, pCtx->gdtr.pGdt, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }
    }

    /*
     * From this point on, we're technically in the new task. We will defer exceptions
     * until the completion of the task switch but before executing any instructions in the new task.
     */
    pCtx->tr.Sel      = SelTSS;
    pCtx->tr.ValidSel = SelTSS;
    pCtx->tr.fFlags   = CPUMSELREG_FLAGS_VALID;
    pCtx->tr.Attr.u   = X86DESC_GET_HID_ATTR(&pNewDescTSS->Legacy);
    pCtx->tr.u32Limit = X86DESC_LIMIT_G(&pNewDescTSS->Legacy);
    pCtx->tr.u64Base  = X86DESC_BASE(&pNewDescTSS->Legacy);
    CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_TR);

    /* Set the busy bit in TR. */
    pCtx->tr.Attr.n.u4Type |= X86_SEL_TYPE_SYS_TSS_BUSY_MASK;
    /* Set EFLAGS.NT (Nested Task) in the eflags loaded from the new TSS, if it's a task switch due to a CALL/INT_XCPT. */
    if (   enmTaskSwitch == IEMTASKSWITCH_CALL
        || enmTaskSwitch == IEMTASKSWITCH_INT_XCPT)
    {
        uNewEflags |= X86_EFL_NT;
    }

    pCtx->dr[7] &= ~X86_DR7_LE_ALL;     /** @todo Should we clear DR7.LE bit too? */
    pCtx->cr0   |= X86_CR0_TS;
    CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_CR0);

    pCtx->eip    = uNewEip;
    pCtx->eax    = uNewEax;
    pCtx->ecx    = uNewEcx;
    pCtx->edx    = uNewEdx;
    pCtx->ebx    = uNewEbx;
    pCtx->esp    = uNewEsp;
    pCtx->ebp    = uNewEbp;
    pCtx->esi    = uNewEsi;
    pCtx->edi    = uNewEdi;

    uNewEflags &= X86_EFL_LIVE_MASK;
    uNewEflags |= X86_EFL_RA1_MASK;
    IEMMISC_SET_EFL(pIemCpu, pCtx, uNewEflags);

    /*
     * Switch the selectors here and do the segment checks later. If we throw exceptions, the selectors
     * will be valid in the exception handler. We cannot update the hidden parts until we've switched CR3
     * due to the hidden part data originating from the guest LDT/GDT which is accessed through paging.
     */
    pCtx->es.Sel       = uNewES;
    pCtx->es.fFlags    = CPUMSELREG_FLAGS_STALE;
    pCtx->es.Attr.u   &= ~X86DESCATTR_P;

    pCtx->cs.Sel       = uNewCS;
    pCtx->cs.fFlags    = CPUMSELREG_FLAGS_STALE;
    pCtx->cs.Attr.u   &= ~X86DESCATTR_P;

    pCtx->ss.Sel       = uNewSS;
    pCtx->ss.fFlags    = CPUMSELREG_FLAGS_STALE;
    pCtx->ss.Attr.u   &= ~X86DESCATTR_P;

    pCtx->ds.Sel       = uNewDS;
    pCtx->ds.fFlags    = CPUMSELREG_FLAGS_STALE;
    pCtx->ds.Attr.u   &= ~X86DESCATTR_P;

    pCtx->fs.Sel       = uNewFS;
    pCtx->fs.fFlags    = CPUMSELREG_FLAGS_STALE;
    pCtx->fs.Attr.u   &= ~X86DESCATTR_P;

    pCtx->gs.Sel       = uNewGS;
    pCtx->gs.fFlags    = CPUMSELREG_FLAGS_STALE;
    pCtx->gs.Attr.u   &= ~X86DESCATTR_P;
    CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_HIDDEN_SEL_REGS);

    pCtx->ldtr.Sel     = uNewLdt;
    pCtx->ldtr.fFlags  = CPUMSELREG_FLAGS_STALE;
    pCtx->ldtr.Attr.u &= ~X86DESCATTR_P;
    CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_LDTR);

    if (IEM_IS_GUEST_CPU_INTEL(pIemCpu) && !IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
    {
        pCtx->es.Attr.u   |= X86DESCATTR_UNUSABLE;
        pCtx->cs.Attr.u   |= X86DESCATTR_UNUSABLE;
        pCtx->ss.Attr.u   |= X86DESCATTR_UNUSABLE;
        pCtx->ds.Attr.u   |= X86DESCATTR_UNUSABLE;
        pCtx->fs.Attr.u   |= X86DESCATTR_UNUSABLE;
        pCtx->gs.Attr.u   |= X86DESCATTR_UNUSABLE;
        pCtx->ldtr.Attr.u |= X86DESCATTR_UNUSABLE;
    }

    /*
     * Switch CR3 for the new task.
     */
    if (   fIsNewTSS386
        && (pCtx->cr0 & X86_CR0_PG))
    {
        /** @todo Should we update and flush TLBs only if CR3 value actually changes? */
        if (!IEM_FULL_VERIFICATION_ENABLED(pIemCpu))
        {
            int rc = CPUMSetGuestCR3(IEMCPU_TO_VMCPU(pIemCpu), uNewCr3);
            AssertRCSuccessReturn(rc, rc);
        }
        else
            pCtx->cr3 = uNewCr3;

        /* Inform PGM. */
        if (!IEM_FULL_VERIFICATION_ENABLED(pIemCpu))
        {
            int rc = PGMFlushTLB(IEMCPU_TO_VMCPU(pIemCpu), pCtx->cr3, !(pCtx->cr4 & X86_CR4_PGE));
            AssertRCReturn(rc, rc);
            /* ignore informational status codes */
        }
        CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_CR3);
    }

    /*
     * Switch LDTR for the new task.
     */
    if (!(uNewLdt & X86_SEL_MASK_OFF_RPL))
        iemHlpLoadNullDataSelectorProt(pIemCpu, &pCtx->ldtr, uNewLdt);
    else
    {
        Assert(!pCtx->ldtr.Attr.n.u1Present);       /* Ensures that LDT.TI check passes in iemMemFetchSelDesc() below. */

        IEMSELDESC DescNewLdt;
        rcStrict = iemMemFetchSelDesc(pIemCpu, &DescNewLdt, uNewLdt, X86_XCPT_TS);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: fetching LDT failed. enmTaskSwitch=%u uNewLdt=%u cbGdt=%u rc=%Rrc\n", enmTaskSwitch,
                 uNewLdt, pCtx->gdtr.cbGdt, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }
        if (   !DescNewLdt.Legacy.Gen.u1Present
            ||  DescNewLdt.Legacy.Gen.u1DescType
            ||  DescNewLdt.Legacy.Gen.u4Type != X86_SEL_TYPE_SYS_LDT)
        {
            Log(("iemTaskSwitch: Invalid LDT. enmTaskSwitch=%u uNewLdt=%u DescNewLdt.Legacy.u=%#RX64 -> #TS\n", enmTaskSwitch,
                 uNewLdt, DescNewLdt.Legacy.u));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewLdt & X86_SEL_MASK_OFF_RPL);
        }

        pCtx->ldtr.ValidSel = uNewLdt;
        pCtx->ldtr.fFlags   = CPUMSELREG_FLAGS_VALID;
        pCtx->ldtr.u64Base  = X86DESC_BASE(&DescNewLdt.Legacy);
        pCtx->ldtr.u32Limit = X86DESC_LIMIT_G(&DescNewLdt.Legacy);
        pCtx->ldtr.Attr.u   = X86DESC_GET_HID_ATTR(&DescNewLdt.Legacy);
        if (IEM_IS_GUEST_CPU_INTEL(pIemCpu) && !IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
            pCtx->ldtr.Attr.u &= ~X86DESCATTR_UNUSABLE;
        Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), &pCtx->ldtr));
    }

    IEMSELDESC DescSS;
    if (IEM_IS_V86_MODE(pIemCpu))
    {
        pIemCpu->uCpl = 3;
        iemHlpLoadSelectorInV86Mode(pIemCpu, &pCtx->es, uNewES);
        iemHlpLoadSelectorInV86Mode(pIemCpu, &pCtx->cs, uNewCS);
        iemHlpLoadSelectorInV86Mode(pIemCpu, &pCtx->ss, uNewSS);
        iemHlpLoadSelectorInV86Mode(pIemCpu, &pCtx->ds, uNewDS);
        iemHlpLoadSelectorInV86Mode(pIemCpu, &pCtx->fs, uNewFS);
        iemHlpLoadSelectorInV86Mode(pIemCpu, &pCtx->gs, uNewGS);
    }
    else
    {
        uint8_t uNewCpl = (uNewCS & X86_SEL_RPL);

        /*
         * Load the stack segment for the new task.
         */
        if (!(uNewSS & X86_SEL_MASK_OFF_RPL))
        {
            Log(("iemTaskSwitch: Null stack segment. enmTaskSwitch=%u uNewSS=%#x -> #TS\n", enmTaskSwitch, uNewSS));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewSS & X86_SEL_MASK_OFF_RPL);
        }

        /* Fetch the descriptor. */
        rcStrict = iemMemFetchSelDesc(pIemCpu, &DescSS, uNewSS, X86_XCPT_TS);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: failed to fetch SS. uNewSS=%#x rc=%Rrc\n", uNewSS,
                 VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* SS must be a data segment and writable. */
        if (    !DescSS.Legacy.Gen.u1DescType
            ||  (DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE))
        {
            Log(("iemTaskSwitch: SS invalid descriptor type. uNewSS=%#x u1DescType=%u u4Type=%#x\n",
                 uNewSS, DescSS.Legacy.Gen.u1DescType, DescSS.Legacy.Gen.u4Type));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewSS & X86_SEL_MASK_OFF_RPL);
        }

        /* The SS.RPL, SS.DPL, CS.RPL (CPL) must be equal. */
        if (   (uNewSS & X86_SEL_RPL) != uNewCpl
            || DescSS.Legacy.Gen.u2Dpl != uNewCpl)
        {
            Log(("iemTaskSwitch: Invalid priv. for SS. uNewSS=%#x SS.DPL=%u uNewCpl=%u -> #TS\n", uNewSS, DescSS.Legacy.Gen.u2Dpl,
                 uNewCpl));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewSS & X86_SEL_MASK_OFF_RPL);
        }

        /* Is it there? */
        if (!DescSS.Legacy.Gen.u1Present)
        {
            Log(("iemTaskSwitch: SS not present. uNewSS=%#x -> #NP\n", uNewSS));
            return iemRaiseSelectorNotPresentWithErr(pIemCpu, uNewSS & X86_SEL_MASK_OFF_RPL);
        }

        uint32_t cbLimit = X86DESC_LIMIT_G(&DescSS.Legacy);
        uint64_t u64Base = X86DESC_BASE(&DescSS.Legacy);

        /* Set the accessed bit before committing the result into SS. */
        if (!(DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uNewSS);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            DescSS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        /* Commit SS. */
        pCtx->ss.Sel      = uNewSS;
        pCtx->ss.ValidSel = uNewSS;
        pCtx->ss.Attr.u   = X86DESC_GET_HID_ATTR(&DescSS.Legacy);
        pCtx->ss.u32Limit = cbLimit;
        pCtx->ss.u64Base  = u64Base;
        pCtx->ss.fFlags   = CPUMSELREG_FLAGS_VALID;
        Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), &pCtx->ss));

        /* CPL has changed, update IEM before loading rest of segments. */
        pIemCpu->uCpl = uNewCpl;

        /*
         * Load the data segments for the new task.
         */
        rcStrict = iemHlpTaskSwitchLoadDataSelectorInProtMode(pIemCpu, &pCtx->es, uNewES);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        rcStrict = iemHlpTaskSwitchLoadDataSelectorInProtMode(pIemCpu, &pCtx->ds, uNewDS);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        rcStrict = iemHlpTaskSwitchLoadDataSelectorInProtMode(pIemCpu, &pCtx->fs, uNewFS);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        rcStrict = iemHlpTaskSwitchLoadDataSelectorInProtMode(pIemCpu, &pCtx->gs, uNewGS);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /*
         * Load the code segment for the new task.
         */
        if (!(uNewCS & X86_SEL_MASK_OFF_RPL))
        {
            Log(("iemTaskSwitch #TS: Null code segment. enmTaskSwitch=%u uNewCS=%#x\n", enmTaskSwitch, uNewCS));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewCS & X86_SEL_MASK_OFF_RPL);
        }

        /* Fetch the descriptor. */
        IEMSELDESC DescCS;
        rcStrict = iemMemFetchSelDesc(pIemCpu, &DescCS, uNewCS, X86_XCPT_TS);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: failed to fetch CS. uNewCS=%u rc=%Rrc\n", uNewCS, VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* CS must be a code segment. */
        if (   !DescCS.Legacy.Gen.u1DescType
            || !(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE))
        {
            Log(("iemTaskSwitch: CS invalid descriptor type. uNewCS=%#x u1DescType=%u u4Type=%#x -> #TS\n", uNewCS,
                 DescCS.Legacy.Gen.u1DescType, DescCS.Legacy.Gen.u4Type));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewCS & X86_SEL_MASK_OFF_RPL);
        }

        /* For conforming CS, DPL must be less than or equal to the RPL. */
        if (   (DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF)
            && DescCS.Legacy.Gen.u2Dpl > (uNewCS & X86_SEL_RPL))
        {
            Log(("iemTaskSwitch: confirming CS DPL > RPL. uNewCS=%#x u4Type=%#x DPL=%u -> #TS\n", uNewCS, DescCS.Legacy.Gen.u4Type,
                 DescCS.Legacy.Gen.u2Dpl));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewCS & X86_SEL_MASK_OFF_RPL);
        }

        /* For non-conforming CS, DPL must match RPL. */
        if (   !(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF)
            && DescCS.Legacy.Gen.u2Dpl != (uNewCS & X86_SEL_RPL))
        {
            Log(("iemTaskSwitch: non-confirming CS DPL RPL mismatch. uNewCS=%#x u4Type=%#x DPL=%u -> #TS\n", uNewCS,
                 DescCS.Legacy.Gen.u4Type, DescCS.Legacy.Gen.u2Dpl));
            return iemRaiseTaskSwitchFaultWithErr(pIemCpu, uNewCS & X86_SEL_MASK_OFF_RPL);
        }

        /* Is it there? */
        if (!DescCS.Legacy.Gen.u1Present)
        {
            Log(("iemTaskSwitch: CS not present. uNewCS=%#x -> #NP\n", uNewCS));
            return iemRaiseSelectorNotPresentWithErr(pIemCpu, uNewCS & X86_SEL_MASK_OFF_RPL);
        }

        cbLimit = X86DESC_LIMIT_G(&DescCS.Legacy);
        u64Base = X86DESC_BASE(&DescCS.Legacy);

        /* Set the accessed bit before committing the result into CS. */
        if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uNewCS);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            DescCS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        /* Commit CS. */
        pCtx->cs.Sel      = uNewCS;
        pCtx->cs.ValidSel = uNewCS;
        pCtx->cs.Attr.u   = X86DESC_GET_HID_ATTR(&DescCS.Legacy);
        pCtx->cs.u32Limit = cbLimit;
        pCtx->cs.u64Base  = u64Base;
        pCtx->cs.fFlags   = CPUMSELREG_FLAGS_VALID;
        Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), &pCtx->cs));
    }

    /** @todo Debug trap. */
    if (fIsNewTSS386 && fNewDebugTrap)
        Log(("iemTaskSwitch: Debug Trap set in new TSS. Not implemented!\n"));

    /*
     * Construct the error code masks based on what caused this task switch.
     * See Intel Instruction reference for INT.
     */
    uint16_t uExt;
    if (   enmTaskSwitch == IEMTASKSWITCH_INT_XCPT
        && !(fFlags & IEM_XCPT_FLAGS_T_SOFT_INT))
    {
        uExt = 1;
    }
    else
        uExt = 0;

    /*
     * Push any error code on to the new stack.
     */
    if (fFlags & IEM_XCPT_FLAGS_ERR)
    {
        Assert(enmTaskSwitch == IEMTASKSWITCH_INT_XCPT);
        uint32_t cbLimitSS = X86DESC_LIMIT_G(&DescSS.Legacy);
        uint8_t const cbStackFrame = fIsNewTSS386 ? 4 : 2;

        /* Check that there is sufficient space on the stack. */
        /** @todo Factor out segment limit checking for normal/expand down segments
         *        into a separate function. */
        if (!(DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_DOWN))
        {
            if (   pCtx->esp - 1 > cbLimitSS
                || pCtx->esp < cbStackFrame)
            {
                /** @todo Intel says \#SS(EXT) for INT/XCPT, I couldn't figure out AMD yet. */
                Log(("iemTaskSwitch: SS=%#x ESP=%#x cbStackFrame=%#x is out of bounds -> #SS\n", pCtx->ss.Sel, pCtx->esp,
                     cbStackFrame));
                return iemRaiseStackSelectorNotPresentWithErr(pIemCpu, uExt);
            }
        }
        else
        {
            if (   pCtx->esp - 1 > (DescSS.Legacy.Gen.u4Type & X86_DESC_DB ? UINT32_MAX : UINT32_C(0xffff))
                || pCtx->esp - cbStackFrame < cbLimitSS + UINT32_C(1))
            {
                Log(("iemTaskSwitch: SS=%#x ESP=%#x cbStackFrame=%#x (expand down) is out of bounds -> #SS\n", pCtx->ss.Sel, pCtx->esp,
                     cbStackFrame));
                return iemRaiseStackSelectorNotPresentWithErr(pIemCpu, uExt);
            }
        }


        if (fIsNewTSS386)
            rcStrict = iemMemStackPushU32(pIemCpu, uErr);
        else
            rcStrict = iemMemStackPushU16(pIemCpu, uErr);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("iemTaskSwitch: Can't push error code to new task's stack. %s-bit TSS. rc=%Rrc\n", fIsNewTSS386 ? "32" : "16",
                 VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }
    }

    /* Check the new EIP against the new CS limit. */
    if (pCtx->eip > pCtx->cs.u32Limit)
    {
        Log(("iemHlpTaskSwitchLoadDataSelectorInProtMode: New EIP exceeds CS limit. uNewEIP=%#RGv CS limit=%u -> #GP(0)\n",
             pCtx->eip, pCtx->cs.u32Limit));
        /** @todo Intel says \#GP(EXT) for INT/XCPT, I couldn't figure out AMD yet. */
        return iemRaiseGeneralProtectionFault(pIemCpu, uExt);
    }

    Log(("iemTaskSwitch: Success! New CS:EIP=%#04x:%#x SS=%#04x\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel));
    return fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT ? VINF_IEM_RAISED_XCPT : VINF_SUCCESS;
}


/**
 * Implements exceptions and interrupts for protected mode.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   cbInstr         The number of bytes to offset rIP by in the return
 *                          address.
 * @param   u8Vector        The interrupt / exception vector number.
 * @param   fFlags          The flags.
 * @param   uErr            The error value if IEM_XCPT_FLAGS_ERR is set.
 * @param   uCr2            The CR2 value if IEM_XCPT_FLAGS_CR2 is set.
 */
IEM_STATIC VBOXSTRICTRC
iemRaiseXcptOrIntInProtMode(PIEMCPU     pIemCpu,
                            PCPUMCTX    pCtx,
                            uint8_t     cbInstr,
                            uint8_t     u8Vector,
                            uint32_t    fFlags,
                            uint16_t    uErr,
                            uint64_t    uCr2)
{
    /*
     * Read the IDT entry.
     */
    if (pCtx->idtr.cbIdt < UINT32_C(8) * u8Vector + 7)
    {
        Log(("RaiseXcptOrIntInProtMode: %#x is out of bounds (%#x)\n", u8Vector, pCtx->idtr.cbIdt));
        return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }
    X86DESC Idte;
    VBOXSTRICTRC rcStrict = iemMemFetchSysU64(pIemCpu, &Idte.u, UINT8_MAX,
                                              pCtx->idtr.pIdt + UINT32_C(8) * u8Vector);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        return rcStrict;
    Log(("iemRaiseXcptOrIntInProtMode: vec=%#x P=%u DPL=%u DT=%u:%u A=%u %04x:%04x%04x\n",
         u8Vector, Idte.Gate.u1Present, Idte.Gate.u2Dpl, Idte.Gate.u1DescType, Idte.Gate.u4Type,
         Idte.Gate.u4ParmCount, Idte.Gate.u16Sel, Idte.Gate.u16OffsetHigh, Idte.Gate.u16OffsetLow));

    /*
     * Check the descriptor type, DPL and such.
     * ASSUMES this is done in the same order as described for call-gate calls.
     */
    if (Idte.Gate.u1DescType)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - not system selector (%#x) -> #GP\n", u8Vector, Idte.Gate.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }
    bool     fTaskGate   = false;
    uint8_t  f32BitGate  = true;
    uint32_t fEflToClear = X86_EFL_TF | X86_EFL_NT | X86_EFL_RF | X86_EFL_VM;
    switch (Idte.Gate.u4Type)
    {
        case X86_SEL_TYPE_SYS_UNDEFINED:
        case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
        case X86_SEL_TYPE_SYS_LDT:
        case X86_SEL_TYPE_SYS_286_TSS_BUSY:
        case X86_SEL_TYPE_SYS_286_CALL_GATE:
        case X86_SEL_TYPE_SYS_UNDEFINED2:
        case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
        case X86_SEL_TYPE_SYS_UNDEFINED3:
        case X86_SEL_TYPE_SYS_386_TSS_BUSY:
        case X86_SEL_TYPE_SYS_386_CALL_GATE:
        case X86_SEL_TYPE_SYS_UNDEFINED4:
        {
            /** @todo check what actually happens when the type is wrong...
             *        esp. call gates. */
            Log(("RaiseXcptOrIntInProtMode %#x - invalid type (%#x) -> #GP\n", u8Vector, Idte.Gate.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
        }

        case X86_SEL_TYPE_SYS_286_INT_GATE:
            f32BitGate = false;
        case X86_SEL_TYPE_SYS_386_INT_GATE:
            fEflToClear |= X86_EFL_IF;
            break;

        case X86_SEL_TYPE_SYS_TASK_GATE:
            fTaskGate = true;
#ifndef IEM_IMPLEMENTS_TASKSWITCH
            IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(("Task gates\n"));
#endif
            break;

        case X86_SEL_TYPE_SYS_286_TRAP_GATE:
            f32BitGate = false;
        case X86_SEL_TYPE_SYS_386_TRAP_GATE:
            break;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    /* Check DPL against CPL if applicable. */
    if (fFlags & IEM_XCPT_FLAGS_T_SOFT_INT)
    {
        if (pIemCpu->uCpl > Idte.Gate.u2Dpl)
        {
            Log(("RaiseXcptOrIntInProtMode %#x - CPL (%d) > DPL (%d) -> #GP\n", u8Vector, pIemCpu->uCpl, Idte.Gate.u2Dpl));
            return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
        }
    }

    /* Is it there? */
    if (!Idte.Gate.u1Present)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - not present -> #NP\n", u8Vector));
        return iemRaiseSelectorNotPresentWithErr(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }

    /* Is it a task-gate? */
    if (fTaskGate)
    {
        /*
         * Construct the error code masks based on what caused this task switch.
         * See Intel Instruction reference for INT.
         */
        uint16_t const uExt     = (fFlags & IEM_XCPT_FLAGS_T_SOFT_INT) ? 0 : 1;
        uint16_t const uSelMask = X86_SEL_MASK_OFF_RPL;
        RTSEL          SelTSS   = Idte.Gate.u16Sel;

        /*
         * Fetch the TSS descriptor in the GDT.
         */
        IEMSELDESC DescTSS;
        rcStrict = iemMemFetchSelDescWithErr(pIemCpu, &DescTSS, SelTSS, X86_XCPT_GP, (SelTSS & uSelMask) | uExt);
        if (rcStrict != VINF_SUCCESS)
        {
            Log(("RaiseXcptOrIntInProtMode %#x - failed to fetch TSS selector %#x, rc=%Rrc\n", u8Vector, SelTSS,
                 VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }

        /* The TSS descriptor must be a system segment and be available (not busy). */
        if (   DescTSS.Legacy.Gen.u1DescType
            || (   DescTSS.Legacy.Gen.u4Type != X86_SEL_TYPE_SYS_286_TSS_AVAIL
                && DescTSS.Legacy.Gen.u4Type != X86_SEL_TYPE_SYS_386_TSS_AVAIL))
        {
            Log(("RaiseXcptOrIntInProtMode %#x - TSS selector %#x of task gate not a system descriptor or not available %#RX64\n",
                 u8Vector, SelTSS, DescTSS.Legacy.au64));
            return iemRaiseGeneralProtectionFault(pIemCpu, (SelTSS & uSelMask) | uExt);
        }

        /* The TSS must be present. */
        if (!DescTSS.Legacy.Gen.u1Present)
        {
            Log(("RaiseXcptOrIntInProtMode %#x - TSS selector %#x not present %#RX64\n", u8Vector, SelTSS, DescTSS.Legacy.au64));
            return iemRaiseSelectorNotPresentWithErr(pIemCpu, (SelTSS & uSelMask) | uExt);
        }

        /* Do the actual task switch. */
        return iemTaskSwitch(pIemCpu, pCtx, IEMTASKSWITCH_INT_XCPT, pCtx->eip, fFlags, uErr, uCr2, SelTSS, &DescTSS);
    }

    /* A null CS is bad. */
    RTSEL NewCS = Idte.Gate.u16Sel;
    if (!(NewCS & X86_SEL_MASK_OFF_RPL))
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x -> #GP\n", u8Vector, NewCS));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Fetch the descriptor for the new CS. */
    IEMSELDESC DescCS;
    rcStrict = iemMemFetchSelDesc(pIemCpu, &DescCS, NewCS, X86_XCPT_GP); /** @todo correct exception? */
    if (rcStrict != VINF_SUCCESS)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - rc=%Rrc\n", u8Vector, NewCS, VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    /* Must be a code segment. */
    if (!DescCS.Legacy.Gen.u1DescType)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - system selector (%#x) -> #GP\n", u8Vector, NewCS, DescCS.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & X86_SEL_MASK_OFF_RPL);
    }
    if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE))
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - data selector (%#x) -> #GP\n", u8Vector, NewCS, DescCS.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & X86_SEL_MASK_OFF_RPL);
    }

    /* Don't allow lowering the privilege level. */
    /** @todo Does the lowering of privileges apply to software interrupts
     *        only?  This has bearings on the more-privileged or
     *        same-privilege stack behavior further down.  A testcase would
     *        be nice. */
    if (DescCS.Legacy.Gen.u2Dpl > pIemCpu->uCpl)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - DPL (%d) > CPL (%d) -> #GP\n",
             u8Vector, NewCS, DescCS.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & X86_SEL_MASK_OFF_RPL);
    }

    /* Make sure the selector is present. */
    if (!DescCS.Legacy.Gen.u1Present)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - segment not present -> #NP\n", u8Vector, NewCS));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, NewCS);
    }

    /* Check the new EIP against the new CS limit. */
    uint32_t const uNewEip =    Idte.Gate.u4Type == X86_SEL_TYPE_SYS_286_INT_GATE
                             || Idte.Gate.u4Type == X86_SEL_TYPE_SYS_286_TRAP_GATE
                           ? Idte.Gate.u16OffsetLow
                           : Idte.Gate.u16OffsetLow | ((uint32_t)Idte.Gate.u16OffsetHigh << 16);
    uint32_t cbLimitCS = X86DESC_LIMIT_G(&DescCS.Legacy);
    if (uNewEip > cbLimitCS)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - EIP=%#x > cbLimitCS=%#x (CS=%#x) -> #GP(0)\n",
             u8Vector, uNewEip, cbLimitCS, NewCS));
        return iemRaiseGeneralProtectionFault(pIemCpu, 0);
    }

    /* Calc the flag image to push. */
    uint32_t        fEfl    = IEMMISC_GET_EFL(pIemCpu, pCtx);
    if (fFlags & (IEM_XCPT_FLAGS_DRx_INSTR_BP | IEM_XCPT_FLAGS_T_SOFT_INT))
        fEfl &= ~X86_EFL_RF;
    else if (!IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
        fEfl |= X86_EFL_RF; /* Vagueness is all I've found on this so far... */ /** @todo Automatically pushing EFLAGS.RF. */

    /* From V8086 mode only go to CPL 0. */
    uint8_t const   uNewCpl = DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF
                            ? pIemCpu->uCpl : DescCS.Legacy.Gen.u2Dpl;
    if ((fEfl & X86_EFL_VM) && uNewCpl != 0) /** @todo When exactly is this raised? */
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - New CPL (%d) != 0 w/ VM=1 -> #GP\n", u8Vector, NewCS, uNewCpl));
        return iemRaiseGeneralProtectionFault(pIemCpu, 0);
    }

    /*
     * If the privilege level changes, we need to get a new stack from the TSS.
     * This in turns means validating the new SS and ESP...
     */
    if (uNewCpl != pIemCpu->uCpl)
    {
        RTSEL    NewSS;
        uint32_t uNewEsp;
        rcStrict = iemRaiseLoadStackFromTss32Or16(pIemCpu, pCtx, uNewCpl, &NewSS, &uNewEsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        IEMSELDESC DescSS;
        rcStrict = iemMiscValidateNewSS(pIemCpu, pCtx, NewSS, uNewCpl, &DescSS);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Check that there is sufficient space for the stack frame. */
        uint32_t cbLimitSS = X86DESC_LIMIT_G(&DescSS.Legacy);
        uint8_t const cbStackFrame = !(fEfl & X86_EFL_VM)
                                   ? (fFlags & IEM_XCPT_FLAGS_ERR ? 12 : 10) << f32BitGate
                                   : (fFlags & IEM_XCPT_FLAGS_ERR ? 20 : 18) << f32BitGate;

        if (!(DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_DOWN))
        {
            if (   uNewEsp - 1 > cbLimitSS
                || uNewEsp < cbStackFrame)
            {
                Log(("RaiseXcptOrIntInProtMode: %#x - SS=%#x ESP=%#x cbStackFrame=%#x is out of bounds -> #GP\n",
                     u8Vector, NewSS, uNewEsp, cbStackFrame));
                return iemRaiseSelectorBoundsBySelector(pIemCpu, NewSS);
            }
        }
        else
        {
            if (   uNewEsp - 1 > (DescSS.Legacy.Gen.u4Type & X86_DESC_DB ? UINT32_MAX : UINT32_C(0xffff))
                || uNewEsp - cbStackFrame < cbLimitSS + UINT32_C(1))
            {
                Log(("RaiseXcptOrIntInProtMode: %#x - SS=%#x ESP=%#x cbStackFrame=%#x (expand down) is out of bounds -> #GP\n",
                     u8Vector, NewSS, uNewEsp, cbStackFrame));
                return iemRaiseSelectorBoundsBySelector(pIemCpu, NewSS);
            }
        }

        /*
         * Start making changes.
         */

        /* Create the stack frame. */
        RTPTRUNION uStackFrame;
        rcStrict = iemMemMap(pIemCpu, &uStackFrame.pv, cbStackFrame, UINT8_MAX,
                             uNewEsp - cbStackFrame + X86DESC_BASE(&DescSS.Legacy), IEM_ACCESS_STACK_W | IEM_ACCESS_WHAT_SYS); /* _SYS is a hack ... */
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        void * const pvStackFrame = uStackFrame.pv;
        if (f32BitGate)
        {
            if (fFlags & IEM_XCPT_FLAGS_ERR)
                *uStackFrame.pu32++ = uErr;
            uStackFrame.pu32[0] = (fFlags & IEM_XCPT_FLAGS_T_SOFT_INT) ? pCtx->eip + cbInstr : pCtx->eip;
            uStackFrame.pu32[1] = (pCtx->cs.Sel & ~X86_SEL_RPL) | pIemCpu->uCpl;
            uStackFrame.pu32[2] = fEfl;
            uStackFrame.pu32[3] = pCtx->esp;
            uStackFrame.pu32[4] = pCtx->ss.Sel;
            if (fEfl & X86_EFL_VM)
            {
                uStackFrame.pu32[1] = pCtx->cs.Sel;
                uStackFrame.pu32[5] = pCtx->es.Sel;
                uStackFrame.pu32[6] = pCtx->ds.Sel;
                uStackFrame.pu32[7] = pCtx->fs.Sel;
                uStackFrame.pu32[8] = pCtx->gs.Sel;
            }
        }
        else
        {
            if (fFlags & IEM_XCPT_FLAGS_ERR)
                *uStackFrame.pu16++ = uErr;
            uStackFrame.pu16[0] = (fFlags & IEM_XCPT_FLAGS_T_SOFT_INT) ? pCtx->ip + cbInstr : pCtx->ip;
            uStackFrame.pu16[1] = (pCtx->cs.Sel & ~X86_SEL_RPL) | pIemCpu->uCpl;
            uStackFrame.pu16[2] = fEfl;
            uStackFrame.pu16[3] = pCtx->sp;
            uStackFrame.pu16[4] = pCtx->ss.Sel;
            if (fEfl & X86_EFL_VM)
            {
                uStackFrame.pu16[1] = pCtx->cs.Sel;
                uStackFrame.pu16[5] = pCtx->es.Sel;
                uStackFrame.pu16[6] = pCtx->ds.Sel;
                uStackFrame.pu16[7] = pCtx->fs.Sel;
                uStackFrame.pu16[8] = pCtx->gs.Sel;
            }
        }
        rcStrict = iemMemCommitAndUnmap(pIemCpu, pvStackFrame, IEM_ACCESS_STACK_W | IEM_ACCESS_WHAT_SYS);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Mark the selectors 'accessed' (hope this is the correct time). */
        /** @todo testcase: excatly _when_ are the accessed bits set - before or
         *        after pushing the stack frame? (Write protect the gdt + stack to
         *        find out.) */
        if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, NewCS);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            DescCS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        if (!(DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, NewSS);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            DescSS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        /*
         * Start comitting the register changes (joins with the DPL=CPL branch).
         */
        pCtx->ss.Sel            = NewSS;
        pCtx->ss.ValidSel       = NewSS;
        pCtx->ss.fFlags         = CPUMSELREG_FLAGS_VALID;
        pCtx->ss.u32Limit       = cbLimitSS;
        pCtx->ss.u64Base        = X86DESC_BASE(&DescSS.Legacy);
        pCtx->ss.Attr.u         = X86DESC_GET_HID_ATTR(&DescSS.Legacy);
        /** @todo When coming from 32-bit code and operating with a 16-bit TSS and
         *        16-bit handler, the high word of ESP remains unchanged (i.e. only
         *        SP is loaded).
         *  Need to check the other combinations too:
         *      - 16-bit TSS, 32-bit handler
         *      - 32-bit TSS, 16-bit handler */
        if (!pCtx->ss.Attr.n.u1DefBig)
            pCtx->sp            = (uint16_t)(uNewEsp - cbStackFrame);
        else
            pCtx->rsp           = uNewEsp - cbStackFrame;
        pIemCpu->uCpl           = uNewCpl;

        if (fEfl & X86_EFL_VM)
        {
            iemHlpLoadNullDataSelectorOnV86Xcpt(pIemCpu, &pCtx->gs);
            iemHlpLoadNullDataSelectorOnV86Xcpt(pIemCpu, &pCtx->fs);
            iemHlpLoadNullDataSelectorOnV86Xcpt(pIemCpu, &pCtx->es);
            iemHlpLoadNullDataSelectorOnV86Xcpt(pIemCpu, &pCtx->ds);
        }
    }
    /*
     * Same privilege, no stack change and smaller stack frame.
     */
    else
    {
        uint64_t        uNewRsp;
        RTPTRUNION      uStackFrame;
        uint8_t const   cbStackFrame = (fFlags & IEM_XCPT_FLAGS_ERR ? 8 : 6) << f32BitGate;
        rcStrict = iemMemStackPushBeginSpecial(pIemCpu, cbStackFrame, &uStackFrame.pv, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        void * const pvStackFrame = uStackFrame.pv;

        if (f32BitGate)
        {
            if (fFlags & IEM_XCPT_FLAGS_ERR)
                *uStackFrame.pu32++ = uErr;
            uStackFrame.pu32[0] = fFlags & IEM_XCPT_FLAGS_T_SOFT_INT ? pCtx->eip + cbInstr : pCtx->eip;
            uStackFrame.pu32[1] = (pCtx->cs.Sel & ~X86_SEL_RPL) | pIemCpu->uCpl;
            uStackFrame.pu32[2] = fEfl;
        }
        else
        {
            if (fFlags & IEM_XCPT_FLAGS_ERR)
                *uStackFrame.pu16++ = uErr;
            uStackFrame.pu16[0] = fFlags & IEM_XCPT_FLAGS_T_SOFT_INT ? pCtx->eip + cbInstr : pCtx->eip;
            uStackFrame.pu16[1] = (pCtx->cs.Sel & ~X86_SEL_RPL) | pIemCpu->uCpl;
            uStackFrame.pu16[2] = fEfl;
        }
        rcStrict = iemMemCommitAndUnmap(pIemCpu, pvStackFrame, IEM_ACCESS_STACK_W); /* don't use the commit here */
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Mark the CS selector as 'accessed'. */
        if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, NewCS);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            DescCS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        /*
         * Start committing the register changes (joins with the other branch).
         */
        pCtx->rsp = uNewRsp;
    }

    /* ... register committing continues. */
    pCtx->cs.Sel            = (NewCS & ~X86_SEL_RPL) | uNewCpl;
    pCtx->cs.ValidSel       = (NewCS & ~X86_SEL_RPL) | uNewCpl;
    pCtx->cs.fFlags         = CPUMSELREG_FLAGS_VALID;
    pCtx->cs.u32Limit       = cbLimitCS;
    pCtx->cs.u64Base        = X86DESC_BASE(&DescCS.Legacy);
    pCtx->cs.Attr.u         = X86DESC_GET_HID_ATTR(&DescCS.Legacy);

    pCtx->rip               = uNewEip;  /* (The entire register is modified, see pe16_32 bs3kit tests.) */
    fEfl &= ~fEflToClear;
    IEMMISC_SET_EFL(pIemCpu, pCtx, fEfl);

    if (fFlags & IEM_XCPT_FLAGS_CR2)
        pCtx->cr2 = uCr2;

    if (fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT)
        iemRaiseXcptAdjustState(pCtx, u8Vector);

    return fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT ? VINF_IEM_RAISED_XCPT : VINF_SUCCESS;
}


/**
 * Implements exceptions and interrupts for long mode.
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   pCtx            The CPU context.
 * @param   cbInstr         The number of bytes to offset rIP by in the return
 *                          address.
 * @param   u8Vector        The interrupt / exception vector number.
 * @param   fFlags          The flags.
 * @param   uErr            The error value if IEM_XCPT_FLAGS_ERR is set.
 * @param   uCr2            The CR2 value if IEM_XCPT_FLAGS_CR2 is set.
 */
IEM_STATIC VBOXSTRICTRC
iemRaiseXcptOrIntInLongMode(PIEMCPU     pIemCpu,
                            PCPUMCTX    pCtx,
                            uint8_t     cbInstr,
                            uint8_t     u8Vector,
                            uint32_t    fFlags,
                            uint16_t    uErr,
                            uint64_t    uCr2)
{
    /*
     * Read the IDT entry.
     */
    uint16_t offIdt = (uint16_t)u8Vector << 4;
    if (pCtx->idtr.cbIdt < offIdt + 7)
    {
        Log(("iemRaiseXcptOrIntInLongMode: %#x is out of bounds (%#x)\n", u8Vector, pCtx->idtr.cbIdt));
        return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }
    X86DESC64 Idte;
    VBOXSTRICTRC rcStrict = iemMemFetchSysU64(pIemCpu, &Idte.au64[0], UINT8_MAX, pCtx->idtr.pIdt + offIdt);
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        rcStrict = iemMemFetchSysU64(pIemCpu, &Idte.au64[1], UINT8_MAX, pCtx->idtr.pIdt + offIdt + 8);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        return rcStrict;
    Log(("iemRaiseXcptOrIntInLongMode: vec=%#x P=%u DPL=%u DT=%u:%u IST=%u %04x:%08x%04x%04x\n",
         u8Vector, Idte.Gate.u1Present, Idte.Gate.u2Dpl, Idte.Gate.u1DescType, Idte.Gate.u4Type,
         Idte.Gate.u3IST, Idte.Gate.u16Sel, Idte.Gate.u32OffsetTop, Idte.Gate.u16OffsetHigh, Idte.Gate.u16OffsetLow));

    /*
     * Check the descriptor type, DPL and such.
     * ASSUMES this is done in the same order as described for call-gate calls.
     */
    if (Idte.Gate.u1DescType)
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - not system selector (%#x) -> #GP\n", u8Vector, Idte.Gate.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }
    uint32_t fEflToClear = X86_EFL_TF | X86_EFL_NT | X86_EFL_RF | X86_EFL_VM;
    switch (Idte.Gate.u4Type)
    {
        case AMD64_SEL_TYPE_SYS_INT_GATE:
            fEflToClear |= X86_EFL_IF;
            break;
        case AMD64_SEL_TYPE_SYS_TRAP_GATE:
            break;

        default:
            Log(("iemRaiseXcptOrIntInLongMode %#x - invalid type (%#x) -> #GP\n", u8Vector, Idte.Gate.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }

    /* Check DPL against CPL if applicable. */
    if (fFlags & IEM_XCPT_FLAGS_T_SOFT_INT)
    {
        if (pIemCpu->uCpl > Idte.Gate.u2Dpl)
        {
            Log(("iemRaiseXcptOrIntInLongMode %#x - CPL (%d) > DPL (%d) -> #GP\n", u8Vector, pIemCpu->uCpl, Idte.Gate.u2Dpl));
            return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
        }
    }

    /* Is it there? */
    if (!Idte.Gate.u1Present)
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - not present -> #NP\n", u8Vector));
        return iemRaiseSelectorNotPresentWithErr(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }

    /* A null CS is bad. */
    RTSEL NewCS = Idte.Gate.u16Sel;
    if (!(NewCS & X86_SEL_MASK_OFF_RPL))
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - CS=%#x -> #GP\n", u8Vector, NewCS));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Fetch the descriptor for the new CS. */
    IEMSELDESC DescCS;
    rcStrict = iemMemFetchSelDesc(pIemCpu, &DescCS, NewCS, X86_XCPT_GP);
    if (rcStrict != VINF_SUCCESS)
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - CS=%#x - rc=%Rrc\n", u8Vector, NewCS, VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    /* Must be a 64-bit code segment. */
    if (!DescCS.Long.Gen.u1DescType)
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - CS=%#x - system selector (%#x) -> #GP\n", u8Vector, NewCS, DescCS.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & X86_SEL_MASK_OFF_RPL);
    }
    if (   !DescCS.Long.Gen.u1Long
        || DescCS.Long.Gen.u1DefBig
        || !(DescCS.Long.Gen.u4Type & X86_SEL_TYPE_CODE) )
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - CS=%#x - not 64-bit code selector (%#x, L=%u, D=%u) -> #GP\n",
             u8Vector, NewCS, DescCS.Legacy.Gen.u4Type, DescCS.Long.Gen.u1Long, DescCS.Long.Gen.u1DefBig));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & X86_SEL_MASK_OFF_RPL);
    }

    /* Don't allow lowering the privilege level.  For non-conforming CS
       selectors, the CS.DPL sets the privilege level the trap/interrupt
       handler runs at.  For conforming CS selectors, the CPL remains
       unchanged, but the CS.DPL must be <= CPL. */
    /** @todo Testcase: Interrupt handler with CS.DPL=1, interrupt dispatched
     *        when CPU in Ring-0. Result \#GP?  */
    if (DescCS.Legacy.Gen.u2Dpl > pIemCpu->uCpl)
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - CS=%#x - DPL (%d) > CPL (%d) -> #GP\n",
             u8Vector, NewCS, DescCS.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & X86_SEL_MASK_OFF_RPL);
    }


    /* Make sure the selector is present. */
    if (!DescCS.Legacy.Gen.u1Present)
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - CS=%#x - segment not present -> #NP\n", u8Vector, NewCS));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, NewCS);
    }

    /* Check that the new RIP is canonical. */
    uint64_t const uNewRip = Idte.Gate.u16OffsetLow
                           | ((uint32_t)Idte.Gate.u16OffsetHigh << 16)
                           | ((uint64_t)Idte.Gate.u32OffsetTop  << 32);
    if (!IEM_IS_CANONICAL(uNewRip))
    {
        Log(("iemRaiseXcptOrIntInLongMode %#x - RIP=%#RX64 - Not canonical -> #GP(0)\n", u8Vector, uNewRip));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /*
     * If the privilege level changes or if the IST isn't zero, we need to get
     * a new stack from the TSS.
     */
    uint64_t        uNewRsp;
    uint8_t const   uNewCpl = DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF
                            ? pIemCpu->uCpl : DescCS.Legacy.Gen.u2Dpl;
    if (   uNewCpl != pIemCpu->uCpl
        || Idte.Gate.u3IST != 0)
    {
        rcStrict = iemRaiseLoadStackFromTss64(pIemCpu, pCtx, uNewCpl, Idte.Gate.u3IST, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }
    else
        uNewRsp = pCtx->rsp;
    uNewRsp &= ~(uint64_t)0xf;

    /*
     * Calc the flag image to push.
     */
    uint32_t        fEfl    = IEMMISC_GET_EFL(pIemCpu, pCtx);
    if (fFlags & (IEM_XCPT_FLAGS_DRx_INSTR_BP | IEM_XCPT_FLAGS_T_SOFT_INT))
        fEfl &= ~X86_EFL_RF;
    else if (!IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
        fEfl |= X86_EFL_RF; /* Vagueness is all I've found on this so far... */ /** @todo Automatically pushing EFLAGS.RF. */

    /*
     * Start making changes.
     */

    /* Create the stack frame. */
    uint32_t   cbStackFrame = sizeof(uint64_t) * (5 + !!(fFlags & IEM_XCPT_FLAGS_ERR));
    RTPTRUNION uStackFrame;
    rcStrict = iemMemMap(pIemCpu, &uStackFrame.pv, cbStackFrame, UINT8_MAX,
                         uNewRsp - cbStackFrame, IEM_ACCESS_STACK_W | IEM_ACCESS_WHAT_SYS); /* _SYS is a hack ... */
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    void * const pvStackFrame = uStackFrame.pv;

    if (fFlags & IEM_XCPT_FLAGS_ERR)
        *uStackFrame.pu64++ = uErr;
    uStackFrame.pu64[0] = fFlags & IEM_XCPT_FLAGS_T_SOFT_INT ? pCtx->rip + cbInstr : pCtx->rip;
    uStackFrame.pu64[1] = (pCtx->cs.Sel & ~X86_SEL_RPL) | pIemCpu->uCpl; /* CPL paranoia */
    uStackFrame.pu64[2] = fEfl;
    uStackFrame.pu64[3] = pCtx->rsp;
    uStackFrame.pu64[4] = pCtx->ss.Sel;
    rcStrict = iemMemCommitAndUnmap(pIemCpu, pvStackFrame, IEM_ACCESS_STACK_W | IEM_ACCESS_WHAT_SYS);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Mark the CS selectors 'accessed' (hope this is the correct time). */
    /** @todo testcase: excatly _when_ are the accessed bits set - before or
     *        after pushing the stack frame? (Write protect the gdt + stack to
     *        find out.) */
    if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
    {
        rcStrict = iemMemMarkSelDescAccessed(pIemCpu, NewCS);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        DescCS.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
    }

    /*
     * Start comitting the register changes.
     */
    /** @todo research/testcase: Figure out what VT-x and AMD-V loads into the
     *        hidden registers when interrupting 32-bit or 16-bit code! */
    if (uNewCpl != pIemCpu->uCpl)
    {
        pCtx->ss.Sel        = 0 | uNewCpl;
        pCtx->ss.ValidSel   = 0 | uNewCpl;
        pCtx->ss.fFlags     = CPUMSELREG_FLAGS_VALID;
        pCtx->ss.u32Limit   = UINT32_MAX;
        pCtx->ss.u64Base    = 0;
        pCtx->ss.Attr.u     = (uNewCpl << X86DESCATTR_DPL_SHIFT) | X86DESCATTR_UNUSABLE;
    }
    pCtx->rsp           = uNewRsp - cbStackFrame;
    pCtx->cs.Sel        = (NewCS & ~X86_SEL_RPL) | uNewCpl;
    pCtx->cs.ValidSel   = (NewCS & ~X86_SEL_RPL) | uNewCpl;
    pCtx->cs.fFlags     = CPUMSELREG_FLAGS_VALID;
    pCtx->cs.u32Limit   = X86DESC_LIMIT_G(&DescCS.Legacy);
    pCtx->cs.u64Base    = X86DESC_BASE(&DescCS.Legacy);
    pCtx->cs.Attr.u     = X86DESC_GET_HID_ATTR(&DescCS.Legacy);
    pCtx->rip           = uNewRip;
    pIemCpu->uCpl       = uNewCpl;

    fEfl &= ~fEflToClear;
    IEMMISC_SET_EFL(pIemCpu, pCtx, fEfl);

    if (fFlags & IEM_XCPT_FLAGS_CR2)
        pCtx->cr2 = uCr2;

    if (fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT)
        iemRaiseXcptAdjustState(pCtx, u8Vector);

    return fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT ? VINF_IEM_RAISED_XCPT : VINF_SUCCESS;
}


/**
 * Implements exceptions and interrupts.
 *
 * All exceptions and interrupts goes thru this function!
 *
 * @returns VBox strict status code.
 * @param   pIemCpu         The IEM per CPU instance data.
 * @param   cbInstr         The number of bytes to offset rIP by in the return
 *                          address.
 * @param   u8Vector        The interrupt / exception vector number.
 * @param   fFlags          The flags.
 * @param   uErr            The error value if IEM_XCPT_FLAGS_ERR is set.
 * @param   uCr2            The CR2 value if IEM_XCPT_FLAGS_CR2 is set.
 */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC)
iemRaiseXcptOrInt(PIEMCPU     pIemCpu,
                  uint8_t     cbInstr,
                  uint8_t     u8Vector,
                  uint32_t    fFlags,
                  uint16_t    uErr,
                  uint64_t    uCr2)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
#ifdef IN_RING0
    int rc = HMR0EnsureCompleteBasicContext(IEMCPU_TO_VMCPU(pIemCpu), pCtx);
    AssertRCReturn(rc, rc);
#endif

    /*
     * Perform the V8086 IOPL check and upgrade the fault without nesting.
     */
    if (   pCtx->eflags.Bits.u1VM
        && pCtx->eflags.Bits.u2IOPL != 3
        && (fFlags & (IEM_XCPT_FLAGS_T_SOFT_INT | IEM_XCPT_FLAGS_BP_INSTR)) == IEM_XCPT_FLAGS_T_SOFT_INT
        && (pCtx->cr0 & X86_CR0_PE) )
    {
        Log(("iemRaiseXcptOrInt: V8086 IOPL check failed for int %#x -> #GP(0)\n", u8Vector));
        fFlags   = IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR;
        u8Vector = X86_XCPT_GP;
        uErr     = 0;
    }
#ifdef DBGFTRACE_ENABLED
    RTTraceBufAddMsgF(IEMCPU_TO_VM(pIemCpu)->CTX_SUFF(hTraceBuf), "Xcpt/%u: %02x %u %x %x %llx %04x:%04llx %04x:%04llx",
                      pIemCpu->cXcptRecursions, u8Vector, cbInstr, fFlags, uErr, uCr2,
                      pCtx->cs.Sel, pCtx->rip, pCtx->ss.Sel, pCtx->rsp);
#endif

    /*
     * Do recursion accounting.
     */
    uint8_t const  uPrevXcpt = pIemCpu->uCurXcpt;
    uint32_t const fPrevXcpt = pIemCpu->fCurXcpt;
    if (pIemCpu->cXcptRecursions == 0)
        Log(("iemRaiseXcptOrInt: %#x at %04x:%RGv cbInstr=%#x fFlags=%#x uErr=%#x uCr2=%llx\n",
             u8Vector, pCtx->cs.Sel, pCtx->rip, cbInstr, fFlags, uErr, uCr2));
    else
    {
        Log(("iemRaiseXcptOrInt: %#x at %04x:%RGv cbInstr=%#x fFlags=%#x uErr=%#x uCr2=%llx; prev=%#x depth=%d flags=%#x\n",
             u8Vector, pCtx->cs.Sel, pCtx->rip, cbInstr, fFlags, uErr, uCr2, pIemCpu->uCurXcpt, pIemCpu->cXcptRecursions + 1, fPrevXcpt));

        /** @todo double and tripple faults. */
        if (pIemCpu->cXcptRecursions >= 3)
        {
#ifdef DEBUG_bird
            AssertFailed();
#endif
            IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(("Too many fault nestings.\n"));
        }

        /** @todo set X86_TRAP_ERR_EXTERNAL when appropriate.
        if (fPrevXcpt & IEM_XCPT_FLAGS_T_EXT_INT)
        {
        ....
        } */
    }
    pIemCpu->cXcptRecursions++;
    pIemCpu->uCurXcpt = u8Vector;
    pIemCpu->fCurXcpt = fFlags;

    /*
     * Extensive logging.
     */
#if defined(LOG_ENABLED) && defined(IN_RING3)
    if (LogIs3Enabled())
    {
        PVM     pVM   = IEMCPU_TO_VM(pIemCpu);
        PVMCPU  pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
        char    szRegs[4096];
        DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                        "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                        "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                        "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                        "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                        "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                        "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                        "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                        "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                        "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                        "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                        "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                        "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                        "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                        "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                        "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                        "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                        "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                        "        efer=%016VR{efer}\n"
                        "         pat=%016VR{pat}\n"
                        "     sf_mask=%016VR{sf_mask}\n"
                        "krnl_gs_base=%016VR{krnl_gs_base}\n"
                        "       lstar=%016VR{lstar}\n"
                        "        star=%016VR{star} cstar=%016VR{cstar}\n"
                        "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                        );

        char szInstr[256];
        DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), NULL);
        Log3(("%s%s\n", szRegs, szInstr));
    }
#endif /* LOG_ENABLED */

    /*
     * Call the mode specific worker function.
     */
    VBOXSTRICTRC    rcStrict;
    if (!(pCtx->cr0 & X86_CR0_PE))
        rcStrict = iemRaiseXcptOrIntInRealMode( pIemCpu, pCtx, cbInstr, u8Vector, fFlags, uErr, uCr2);
    else if (pCtx->msrEFER & MSR_K6_EFER_LMA)
        rcStrict = iemRaiseXcptOrIntInLongMode( pIemCpu, pCtx, cbInstr, u8Vector, fFlags, uErr, uCr2);
    else
        rcStrict = iemRaiseXcptOrIntInProtMode( pIemCpu, pCtx, cbInstr, u8Vector, fFlags, uErr, uCr2);

    /*
     * Unwind.
     */
    pIemCpu->cXcptRecursions--;
    pIemCpu->uCurXcpt = uPrevXcpt;
    pIemCpu->fCurXcpt = fPrevXcpt;
    Log(("iemRaiseXcptOrInt: returns %Rrc (vec=%#x); cs:rip=%04x:%RGv ss:rsp=%04x:%RGv cpl=%u\n",
         VBOXSTRICTRC_VAL(rcStrict), u8Vector, pCtx->cs.Sel, pCtx->rip, pCtx->ss.Sel, pCtx->esp, pIemCpu->uCpl));
    return rcStrict;
}


/** \#DE - 00.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseDivideError(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_DE, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#DB - 01.
 * @note This automatically clear DR7.GD.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseDebugException(PIEMCPU pIemCpu)
{
    /** @todo set/clear RF. */
    pIemCpu->CTX_SUFF(pCtx)->dr[7] &= ~X86_DR7_GD;
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_DB, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#UD - 06.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseUndefinedOpcode(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_UD, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#NM - 07.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseDeviceNotAvailable(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NM, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#TS(err) - 0a.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseTaskSwitchFaultWithErr(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_TS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}


/** \#TS(tr) - 0a.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseTaskSwitchFaultCurrentTSS(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_TS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             pIemCpu->CTX_SUFF(pCtx)->tr.Sel, 0);
}


/** \#TS(0) - 0a.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseTaskSwitchFault0(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_TS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             0, 0);
}


/** \#TS(err) - 0a.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseTaskSwitchFaultBySelector(PIEMCPU pIemCpu, uint16_t uSel)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_TS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             uSel & X86_SEL_MASK_OFF_RPL, 0);
}


/** \#NP(err) - 0b.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseSelectorNotPresentWithErr(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}


/** \#NP(seg) - 0b.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseSelectorNotPresentBySegReg(PIEMCPU pIemCpu, uint32_t iSegReg)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             iemSRegFetchU16(pIemCpu, iSegReg) & ~X86_SEL_RPL, 0);
}


/** \#NP(sel) - 0b.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             uSel & ~X86_SEL_RPL, 0);
}


/** \#SS(seg) - 0c.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseStackSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_SS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             uSel & ~X86_SEL_RPL, 0);
}


/** \#SS(err) - 0c.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseStackSelectorNotPresentWithErr(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_SS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}


/** \#GP(n) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseGeneralProtectionFault(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}


/** \#GP(0) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseGeneralProtectionFault0(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseGeneralProtectionFaultBySelector(PIEMCPU pIemCpu, RTSEL Sel)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             Sel & ~X86_SEL_RPL, 0);
}


/** \#GP(0) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseNotCanonical(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseSelectorBounds(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess)
{
    NOREF(iSegReg); NOREF(fAccess);
    return iemRaiseXcptOrInt(pIemCpu, 0, iSegReg == X86_SREG_SS ? X86_XCPT_SS : X86_XCPT_GP,
                             IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseSelectorBoundsBySelector(PIEMCPU pIemCpu, RTSEL Sel)
{
    NOREF(Sel);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseSelectorInvalidAccess(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess)
{
    NOREF(iSegReg); NOREF(fAccess);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#PF(n) - 0e.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaisePageFault(PIEMCPU pIemCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc)
{
    uint16_t uErr;
    switch (rc)
    {
        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
        case VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT:
        case VERR_PAGE_MAP_LEVEL4_NOT_PRESENT:
            uErr = 0;
            break;

        default:
            AssertMsgFailed(("%Rrc\n", rc));
        case VERR_ACCESS_DENIED:
            uErr = X86_TRAP_PF_P;
            break;

        /** @todo reserved  */
    }

    if (pIemCpu->uCpl == 3)
        uErr |= X86_TRAP_PF_US;

    if (   (fAccess & IEM_ACCESS_WHAT_MASK) == IEM_ACCESS_WHAT_CODE
        && (   (pIemCpu->CTX_SUFF(pCtx)->cr4 & X86_CR4_PAE)
            && (pIemCpu->CTX_SUFF(pCtx)->msrEFER & MSR_K6_EFER_NXE) ) )
        uErr |= X86_TRAP_PF_ID;

#if 0 /* This is so much non-sense, really.  Why was it done like that? */
    /* Note! RW access callers reporting a WRITE protection fault, will clear
             the READ flag before calling.  So, read-modify-write accesses (RW)
             can safely be reported as READ faults. */
    if ((fAccess & (IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_TYPE_READ)) == IEM_ACCESS_TYPE_WRITE)
        uErr |= X86_TRAP_PF_RW;
#else
    if (fAccess & IEM_ACCESS_TYPE_WRITE)
    {
        if (!IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu) || !(fAccess & IEM_ACCESS_TYPE_READ))
            uErr |= X86_TRAP_PF_RW;
    }
#endif

    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_PF, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR | IEM_XCPT_FLAGS_CR2,
                             uErr, GCPtrWhere);
}


/** \#MF(0) - 10.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseMathFault(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_MF, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#AC(0) - 11.  */
DECL_NO_INLINE(IEM_STATIC, VBOXSTRICTRC) iemRaiseAlignmentCheckException(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_AC, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/**
 * Macro for calling iemCImplRaiseDivideError().
 *
 * This enables us to add/remove arguments and force different levels of
 * inlining as we wish.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_DIVIDE_ERROR()          IEM_MC_DEFER_TO_CIMPL_0(iemCImplRaiseDivideError)
IEM_CIMPL_DEF_0(iemCImplRaiseDivideError)
{
    NOREF(cbInstr);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_DE, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/**
 * Macro for calling iemCImplRaiseInvalidLockPrefix().
 *
 * This enables us to add/remove arguments and force different levels of
 * inlining as we wish.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_LOCK_PREFIX()   IEM_MC_DEFER_TO_CIMPL_0(iemCImplRaiseInvalidLockPrefix)
IEM_CIMPL_DEF_0(iemCImplRaiseInvalidLockPrefix)
{
    NOREF(cbInstr);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_UD, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/**
 * Macro for calling iemCImplRaiseInvalidOpcode().
 *
 * This enables us to add/remove arguments and force different levels of
 * inlining as we wish.
 *
 * @return  Strict VBox status code.
 */
#define IEMOP_RAISE_INVALID_OPCODE()        IEM_MC_DEFER_TO_CIMPL_0(iemCImplRaiseInvalidOpcode)
IEM_CIMPL_DEF_0(iemCImplRaiseInvalidOpcode)
{
    NOREF(cbInstr);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_UD, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** @}  */


/*
 *
 * Helpers routines.
 * Helpers routines.
 * Helpers routines.
 *
 */

/**
 * Recalculates the effective operand size.
 *
 * @param   pIemCpu             The IEM state.
 */
IEM_STATIC void iemRecalEffOpSize(PIEMCPU pIemCpu)
{
    switch (pIemCpu->enmCpuMode)
    {
        case IEMMODE_16BIT:
            pIemCpu->enmEffOpSize = pIemCpu->fPrefixes & IEM_OP_PRF_SIZE_OP ? IEMMODE_32BIT : IEMMODE_16BIT;
            break;
        case IEMMODE_32BIT:
            pIemCpu->enmEffOpSize = pIemCpu->fPrefixes & IEM_OP_PRF_SIZE_OP ? IEMMODE_16BIT : IEMMODE_32BIT;
            break;
        case IEMMODE_64BIT:
            switch (pIemCpu->fPrefixes & (IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP))
            {
                case 0:
                    pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize;
                    break;
                case IEM_OP_PRF_SIZE_OP:
                    pIemCpu->enmEffOpSize = IEMMODE_16BIT;
                    break;
                case IEM_OP_PRF_SIZE_REX_W:
                case IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP:
                    pIemCpu->enmEffOpSize = IEMMODE_64BIT;
                    break;
            }
            break;
        default:
            AssertFailed();
    }
}


/**
 * Sets the default operand size to 64-bit and recalculates the effective
 * operand size.
 *
 * @param   pIemCpu             The IEM state.
 */
IEM_STATIC void iemRecalEffOpSize64Default(PIEMCPU pIemCpu)
{
    Assert(pIemCpu->enmCpuMode == IEMMODE_64BIT);
    pIemCpu->enmDefOpSize = IEMMODE_64BIT;
    if ((pIemCpu->fPrefixes & (IEM_OP_PRF_SIZE_REX_W | IEM_OP_PRF_SIZE_OP)) != IEM_OP_PRF_SIZE_OP)
        pIemCpu->enmEffOpSize = IEMMODE_64BIT;
    else
        pIemCpu->enmEffOpSize = IEMMODE_16BIT;
}


/*
 *
 * Common opcode decoders.
 * Common opcode decoders.
 * Common opcode decoders.
 *
 */
//#include <iprt/mem.h>

/**
 * Used to add extra details about a stub case.
 * @param   pIemCpu     The IEM per CPU state.
 */
IEM_STATIC void iemOpStubMsg2(PIEMCPU pIemCpu)
{
#if defined(LOG_ENABLED) && defined(IN_RING3)
    PVM     pVM   = IEMCPU_TO_VM(pIemCpu);
    PVMCPU  pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    char szRegs[4096];
    DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                    "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                    "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                    "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                    "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                    "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                    "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                    "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                    "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                    "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                    "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                    "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                    "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                    "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                    "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                    "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                    "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                    "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                    "        efer=%016VR{efer}\n"
                    "         pat=%016VR{pat}\n"
                    "     sf_mask=%016VR{sf_mask}\n"
                    "krnl_gs_base=%016VR{krnl_gs_base}\n"
                    "       lstar=%016VR{lstar}\n"
                    "        star=%016VR{star} cstar=%016VR{cstar}\n"
                    "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                    );

    char szInstr[256];
    DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                       DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                       szInstr, sizeof(szInstr), NULL);

    RTAssertMsg2Weak("%s%s\n", szRegs, szInstr);
#else
    RTAssertMsg2Weak("cs:rip=%04x:%RX64\n", pIemCpu->CTX_SUFF(pCtx)->cs, pIemCpu->CTX_SUFF(pCtx)->rip);
#endif
}

/**
 * Complains about a stub.
 *
 * Providing two versions of this macro, one for daily use and one for use when
 * working on IEM.
 */
#if 0
# define IEMOP_BITCH_ABOUT_STUB() \
    do { \
        RTAssertMsg1(NULL, __LINE__, __FILE__, __FUNCTION__); \
        iemOpStubMsg2(pIemCpu); \
        RTAssertPanic(); \
    } while (0)
#else
# define IEMOP_BITCH_ABOUT_STUB() Log(("Stub: %s (line %d)\n", __FUNCTION__, __LINE__));
#endif

/** Stubs an opcode. */
#define FNIEMOP_STUB(a_Name) \
    FNIEMOP_DEF(a_Name) \
    { \
        IEMOP_BITCH_ABOUT_STUB(); \
        return VERR_IEM_INSTR_NOT_IMPLEMENTED; \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode. */
#define FNIEMOP_STUB_1(a_Name, a_Type0, a_Name0) \
    FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    { \
        IEMOP_BITCH_ABOUT_STUB(); \
        NOREF(a_Name0); \
        return VERR_IEM_INSTR_NOT_IMPLEMENTED; \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode which currently should raise \#UD. */
#define FNIEMOP_UD_STUB(a_Name) \
    FNIEMOP_DEF(a_Name) \
    { \
        Log(("Unsupported instruction %Rfn\n", __FUNCTION__)); \
        return IEMOP_RAISE_INVALID_OPCODE(); \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode which currently should raise \#UD. */
#define FNIEMOP_UD_STUB_1(a_Name, a_Type0, a_Name0) \
    FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    { \
        NOREF(a_Name0); \
        Log(("Unsupported instruction %Rfn\n", __FUNCTION__)); \
        return IEMOP_RAISE_INVALID_OPCODE(); \
    } \
    typedef int ignore_semicolon



/** @name   Register Access.
 * @{
 */

/**
 * Gets a reference (pointer) to the specified hidden segment register.
 *
 * @returns Hidden register reference.
 * @param   pIemCpu             The per CPU data.
 * @param   iSegReg             The segment register.
 */
IEM_STATIC PCPUMSELREG iemSRegGetHid(PIEMCPU pIemCpu, uint8_t iSegReg)
{
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    PCPUMSELREG pSReg;
    switch (iSegReg)
    {
        case X86_SREG_ES: pSReg = &pCtx->es; break;
        case X86_SREG_CS: pSReg = &pCtx->cs; break;
        case X86_SREG_SS: pSReg = &pCtx->ss; break;
        case X86_SREG_DS: pSReg = &pCtx->ds; break;
        case X86_SREG_FS: pSReg = &pCtx->fs; break;
        case X86_SREG_GS: pSReg = &pCtx->gs; break;
        default:
            AssertFailedReturn(NULL);
    }
#ifdef VBOX_WITH_RAW_MODE_NOT_R0
    if (!CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), pSReg))
        CPUMGuestLazyLoadHiddenSelectorReg(IEMCPU_TO_VMCPU(pIemCpu), pSReg);
#else
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(IEMCPU_TO_VMCPU(pIemCpu), pSReg));
#endif
    return pSReg;
}


/**
 * Gets a reference (pointer) to the specified segment register (the selector
 * value).
 *
 * @returns Pointer to the selector variable.
 * @param   pIemCpu             The per CPU data.
 * @param   iSegReg             The segment register.
 */
IEM_STATIC uint16_t *iemSRegRef(PIEMCPU pIemCpu, uint8_t iSegReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (iSegReg)
    {
        case X86_SREG_ES: return &pCtx->es.Sel;
        case X86_SREG_CS: return &pCtx->cs.Sel;
        case X86_SREG_SS: return &pCtx->ss.Sel;
        case X86_SREG_DS: return &pCtx->ds.Sel;
        case X86_SREG_FS: return &pCtx->fs.Sel;
        case X86_SREG_GS: return &pCtx->gs.Sel;
    }
    AssertFailedReturn(NULL);
}


/**
 * Fetches the selector value of a segment register.
 *
 * @returns The selector value.
 * @param   pIemCpu             The per CPU data.
 * @param   iSegReg             The segment register.
 */
IEM_STATIC uint16_t iemSRegFetchU16(PIEMCPU pIemCpu, uint8_t iSegReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (iSegReg)
    {
        case X86_SREG_ES: return pCtx->es.Sel;
        case X86_SREG_CS: return pCtx->cs.Sel;
        case X86_SREG_SS: return pCtx->ss.Sel;
        case X86_SREG_DS: return pCtx->ds.Sel;
        case X86_SREG_FS: return pCtx->fs.Sel;
        case X86_SREG_GS: return pCtx->gs.Sel;
    }
    AssertFailedReturn(0xffff);
}


/**
 * Gets a reference (pointer) to the specified general register.
 *
 * @returns Register reference.
 * @param   pIemCpu             The per CPU data.
 * @param   iReg                The general register.
 */
IEM_STATIC void *iemGRegRef(PIEMCPU pIemCpu, uint8_t iReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (iReg)
    {
        case X86_GREG_xAX: return &pCtx->rax;
        case X86_GREG_xCX: return &pCtx->rcx;
        case X86_GREG_xDX: return &pCtx->rdx;
        case X86_GREG_xBX: return &pCtx->rbx;
        case X86_GREG_xSP: return &pCtx->rsp;
        case X86_GREG_xBP: return &pCtx->rbp;
        case X86_GREG_xSI: return &pCtx->rsi;
        case X86_GREG_xDI: return &pCtx->rdi;
        case X86_GREG_x8:  return &pCtx->r8;
        case X86_GREG_x9:  return &pCtx->r9;
        case X86_GREG_x10: return &pCtx->r10;
        case X86_GREG_x11: return &pCtx->r11;
        case X86_GREG_x12: return &pCtx->r12;
        case X86_GREG_x13: return &pCtx->r13;
        case X86_GREG_x14: return &pCtx->r14;
        case X86_GREG_x15: return &pCtx->r15;
    }
    AssertFailedReturn(NULL);
}


/**
 * Gets a reference (pointer) to the specified 8-bit general register.
 *
 * Because of AH, CH, DH and BH we cannot use iemGRegRef directly here.
 *
 * @returns Register reference.
 * @param   pIemCpu             The per CPU data.
 * @param   iReg                The register.
 */
IEM_STATIC uint8_t *iemGRegRefU8(PIEMCPU pIemCpu, uint8_t iReg)
{
    if (pIemCpu->fPrefixes & IEM_OP_PRF_REX)
        return (uint8_t *)iemGRegRef(pIemCpu, iReg);

    uint8_t *pu8Reg = (uint8_t *)iemGRegRef(pIemCpu, iReg & 3);
    if (iReg >= 4)
        pu8Reg++;
    return pu8Reg;
}


/**
 * Fetches the value of a 8-bit general register.
 *
 * @returns The register value.
 * @param   pIemCpu             The per CPU data.
 * @param   iReg                The register.
 */
IEM_STATIC uint8_t iemGRegFetchU8(PIEMCPU pIemCpu, uint8_t iReg)
{
    uint8_t const *pbSrc = iemGRegRefU8(pIemCpu, iReg);
    return *pbSrc;
}


/**
 * Fetches the value of a 16-bit general register.
 *
 * @returns The register value.
 * @param   pIemCpu             The per CPU data.
 * @param   iReg                The register.
 */
IEM_STATIC uint16_t iemGRegFetchU16(PIEMCPU pIemCpu, uint8_t iReg)
{
    return *(uint16_t *)iemGRegRef(pIemCpu, iReg);
}


/**
 * Fetches the value of a 32-bit general register.
 *
 * @returns The register value.
 * @param   pIemCpu             The per CPU data.
 * @param   iReg                The register.
 */
IEM_STATIC uint32_t iemGRegFetchU32(PIEMCPU pIemCpu, uint8_t iReg)
{
    return *(uint32_t *)iemGRegRef(pIemCpu, iReg);
}


/**
 * Fetches the value of a 64-bit general register.
 *
 * @returns The register value.
 * @param   pIemCpu             The per CPU data.
 * @param   iReg                The register.
 */
IEM_STATIC uint64_t iemGRegFetchU64(PIEMCPU pIemCpu, uint8_t iReg)
{
    return *(uint64_t *)iemGRegRef(pIemCpu, iReg);
}


/**
 * Adds a 8-bit signed jump offset to RIP/EIP/IP.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   offNextInstr        The offset of the next instruction.
 */
IEM_STATIC VBOXSTRICTRC iemRegRipRelativeJumpS8(PIEMCPU pIemCpu, int8_t offNextInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t uNewIp = pCtx->ip + offNextInstr + pIemCpu->offOpcode;
            if (   uNewIp > pCtx->cs.u32Limit
                && pIemCpu->enmCpuMode != IEMMODE_64BIT) /* no need to check for non-canonical. */
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            pCtx->rip = uNewIp;
            break;
        }

        case IEMMODE_32BIT:
        {
            Assert(pCtx->rip <= UINT32_MAX);
            Assert(pIemCpu->enmCpuMode != IEMMODE_64BIT);

            uint32_t uNewEip = pCtx->eip + offNextInstr + pIemCpu->offOpcode;
            if (uNewEip > pCtx->cs.u32Limit)
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            pCtx->rip = uNewEip;
            break;
        }

        case IEMMODE_64BIT:
        {
            Assert(pIemCpu->enmCpuMode == IEMMODE_64BIT);

            uint64_t uNewRip = pCtx->rip + offNextInstr + pIemCpu->offOpcode;
            if (!IEM_IS_CANONICAL(uNewRip))
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            pCtx->rip = uNewRip;
            break;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    pCtx->eflags.Bits.u1RF = 0;
    return VINF_SUCCESS;
}


/**
 * Adds a 16-bit signed jump offset to RIP/EIP/IP.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The per CPU data.
 * @param   offNextInstr        The offset of the next instruction.
 */
IEM_STATIC VBOXSTRICTRC iemRegRipRelativeJumpS16(PIEMCPU pIemCpu, int16_t offNextInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    Assert(pIemCpu->enmEffOpSize == IEMMODE_16BIT);

    uint16_t uNewIp = pCtx->ip + offNextInstr + pIemCpu->offOpcode;
    if (   uNewIp > pCtx->cs.u32Limit
        && pIemCpu->enmCpuMode != IEMMODE_64BIT) /* no need to check for non-canonical. */
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    /** @todo Test 16-bit jump in 64-bit mode. possible?  */
    pCtx->rip = uNewIp;
    pCtx->eflags.Bits.u1RF = 0;

    return VINF_SUCCESS;
}


/**
 * Adds a 32-bit signed jump offset to RIP/EIP/IP.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The per CPU data.
 * @param   offNextInstr        The offset of the next instruction.
 */
IEM_STATIC VBOXSTRICTRC iemRegRipRelativeJumpS32(PIEMCPU pIemCpu, int32_t offNextInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    Assert(pIemCpu->enmEffOpSize != IEMMODE_16BIT);

    if (pIemCpu->enmEffOpSize == IEMMODE_32BIT)
    {
        Assert(pCtx->rip <= UINT32_MAX); Assert(pIemCpu->enmCpuMode != IEMMODE_64BIT);

        uint32_t uNewEip = pCtx->eip + offNextInstr + pIemCpu->offOpcode;
        if (uNewEip > pCtx->cs.u32Limit)
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        pCtx->rip = uNewEip;
    }
    else
    {
        Assert(pIemCpu->enmCpuMode == IEMMODE_64BIT);

        uint64_t uNewRip = pCtx->rip + offNextInstr + pIemCpu->offOpcode;
        if (!IEM_IS_CANONICAL(uNewRip))
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        pCtx->rip = uNewRip;
    }
    pCtx->eflags.Bits.u1RF = 0;
    return VINF_SUCCESS;
}


/**
 * Performs a near jump to the specified address.
 *
 * May raise a \#GP(0) if the new RIP is non-canonical or outside the code
 * segment limit.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   uNewRip             The new RIP value.
 */
IEM_STATIC VBOXSTRICTRC iemRegRipJump(PIEMCPU pIemCpu, uint64_t uNewRip)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            Assert(uNewRip <= UINT16_MAX);
            if (   uNewRip > pCtx->cs.u32Limit
                && pIemCpu->enmCpuMode != IEMMODE_64BIT) /* no need to check for non-canonical. */
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            /** @todo Test 16-bit jump in 64-bit mode.  */
            pCtx->rip = uNewRip;
            break;
        }

        case IEMMODE_32BIT:
        {
            Assert(uNewRip <= UINT32_MAX);
            Assert(pCtx->rip <= UINT32_MAX);
            Assert(pIemCpu->enmCpuMode != IEMMODE_64BIT);

            if (uNewRip > pCtx->cs.u32Limit)
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            pCtx->rip = uNewRip;
            break;
        }

        case IEMMODE_64BIT:
        {
            Assert(pIemCpu->enmCpuMode == IEMMODE_64BIT);

            if (!IEM_IS_CANONICAL(uNewRip))
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            pCtx->rip = uNewRip;
            break;
        }

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    pCtx->eflags.Bits.u1RF = 0;
    return VINF_SUCCESS;
}


/**
 * Get the address of the top of the stack.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   pCtx                The CPU context which SP/ESP/RSP should be
 *                              read.
 */
DECLINLINE(RTGCPTR) iemRegGetEffRsp(PCIEMCPU pIemCpu, PCCPUMCTX pCtx)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        return pCtx->rsp;
    if (pCtx->ss.Attr.n.u1DefBig)
        return pCtx->esp;
    return pCtx->sp;
}


/**
 * Updates the RIP/EIP/IP to point to the next instruction.
 *
 * This function leaves the EFLAGS.RF flag alone.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   cbInstr             The number of bytes to add.
 */
IEM_STATIC void iemRegAddToRipKeepRF(PIEMCPU pIemCpu, uint8_t cbInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (pIemCpu->enmCpuMode)
    {
        case IEMMODE_16BIT:
            Assert(pCtx->rip <= UINT16_MAX);
            pCtx->eip += cbInstr;
            pCtx->eip &= UINT32_C(0xffff);
            break;

        case IEMMODE_32BIT:
            pCtx->eip += cbInstr;
            Assert(pCtx->rip <= UINT32_MAX);
            break;

        case IEMMODE_64BIT:
            pCtx->rip += cbInstr;
            break;
        default: AssertFailed();
    }
}


#if 0
/**
 * Updates the RIP/EIP/IP to point to the next instruction.
 *
 * @param   pIemCpu             The per CPU data.
 */
IEM_STATIC void iemRegUpdateRipKeepRF(PIEMCPU pIemCpu)
{
    return iemRegAddToRipKeepRF(pIemCpu, pIemCpu->offOpcode);
}
#endif



/**
 * Updates the RIP/EIP/IP to point to the next instruction and clears EFLAGS.RF.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   cbInstr             The number of bytes to add.
 */
IEM_STATIC void iemRegAddToRipAndClearRF(PIEMCPU pIemCpu, uint8_t cbInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    pCtx->eflags.Bits.u1RF = 0;

    /* NB: Must be kept in sync with HM (xxxAdvanceGuestRip). */
    switch (pIemCpu->enmCpuMode)
    {
        /** @todo investigate if EIP or RIP is really incremented. */
        case IEMMODE_16BIT:
        case IEMMODE_32BIT:
            pCtx->eip += cbInstr;
            Assert(pCtx->rip <= UINT32_MAX);
            break;

        case IEMMODE_64BIT:
            pCtx->rip += cbInstr;
            break;
        default: AssertFailed();
    }
}


/**
 * Updates the RIP/EIP/IP to point to the next instruction and clears EFLAGS.RF.
 *
 * @param   pIemCpu             The per CPU data.
 */
IEM_STATIC void iemRegUpdateRipAndClearRF(PIEMCPU pIemCpu)
{
    return iemRegAddToRipAndClearRF(pIemCpu, pIemCpu->offOpcode);
}


/**
 * Adds to the stack pointer.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   pCtx                The CPU context which SP/ESP/RSP should be
 *                              updated.
 * @param   cbToAdd             The number of bytes to add.
 */
DECLINLINE(void) iemRegAddToRsp(PCIEMCPU pIemCpu, PCPUMCTX pCtx, uint8_t cbToAdd)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        pCtx->rsp += cbToAdd;
    else if (pCtx->ss.Attr.n.u1DefBig)
        pCtx->esp += cbToAdd;
    else
        pCtx->sp  += cbToAdd;
}


/**
 * Subtracts from the stack pointer.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   pCtx                The CPU context which SP/ESP/RSP should be
 *                              updated.
 * @param   cbToSub             The number of bytes to subtract.
 */
DECLINLINE(void) iemRegSubFromRsp(PCIEMCPU pIemCpu, PCPUMCTX pCtx, uint8_t cbToSub)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        pCtx->rsp -= cbToSub;
    else if (pCtx->ss.Attr.n.u1DefBig)
        pCtx->esp -= cbToSub;
    else
        pCtx->sp  -= cbToSub;
}


/**
 * Adds to the temporary stack pointer.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   pTmpRsp             The temporary SP/ESP/RSP to update.
 * @param   cbToAdd             The number of bytes to add.
 * @param   pCtx                Where to get the current stack mode.
 */
DECLINLINE(void) iemRegAddToRspEx(PCIEMCPU pIemCpu, PCCPUMCTX pCtx, PRTUINT64U pTmpRsp, uint16_t cbToAdd)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        pTmpRsp->u           += cbToAdd;
    else if (pCtx->ss.Attr.n.u1DefBig)
        pTmpRsp->DWords.dw0  += cbToAdd;
    else
        pTmpRsp->Words.w0    += cbToAdd;
}


/**
 * Subtracts from the temporary stack pointer.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   pTmpRsp             The temporary SP/ESP/RSP to update.
 * @param   cbToSub             The number of bytes to subtract.
 * @param   pCtx                Where to get the current stack mode.
 * @remarks The @a cbToSub argument *MUST* be 16-bit, iemCImpl_enter is
 *          expecting that.
 */
DECLINLINE(void) iemRegSubFromRspEx(PCIEMCPU pIemCpu, PCCPUMCTX pCtx, PRTUINT64U pTmpRsp, uint16_t cbToSub)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        pTmpRsp->u          -= cbToSub;
    else if (pCtx->ss.Attr.n.u1DefBig)
        pTmpRsp->DWords.dw0 -= cbToSub;
    else
        pTmpRsp->Words.w0   -= cbToSub;
}


/**
 * Calculates the effective stack address for a push of the specified size as
 * well as the new RSP value (upper bits may be masked).
 *
 * @returns Effective stack addressf for the push.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pCtx                Where to get the current stack mode.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPush(PCIEMCPU pIemCpu, PCCPUMCTX pCtx, uint8_t cbItem, uint64_t *puNewRsp)
{
    RTUINT64U   uTmpRsp;
    RTGCPTR     GCPtrTop;
    uTmpRsp.u = pCtx->rsp;

    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        GCPtrTop = uTmpRsp.u            -= cbItem;
    else if (pCtx->ss.Attr.n.u1DefBig)
        GCPtrTop = uTmpRsp.DWords.dw0   -= cbItem;
    else
        GCPtrTop = uTmpRsp.Words.w0     -= cbItem;
    *puNewRsp = uTmpRsp.u;
    return GCPtrTop;
}


/**
 * Gets the current stack pointer and calculates the value after a pop of the
 * specified size.
 *
 * @returns Current stack pointer.
 * @param   pIemCpu             The per CPU data.
 * @param   pCtx                Where to get the current stack mode.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPop(PCIEMCPU pIemCpu, PCCPUMCTX pCtx, uint8_t cbItem, uint64_t *puNewRsp)
{
    RTUINT64U   uTmpRsp;
    RTGCPTR     GCPtrTop;
    uTmpRsp.u = pCtx->rsp;

    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        GCPtrTop = uTmpRsp.u;
        uTmpRsp.u += cbItem;
    }
    else if (pCtx->ss.Attr.n.u1DefBig)
    {
        GCPtrTop = uTmpRsp.DWords.dw0;
        uTmpRsp.DWords.dw0 += cbItem;
    }
    else
    {
        GCPtrTop = uTmpRsp.Words.w0;
        uTmpRsp.Words.w0 += cbItem;
    }
    *puNewRsp = uTmpRsp.u;
    return GCPtrTop;
}


/**
 * Calculates the effective stack address for a push of the specified size as
 * well as the new temporary RSP value (upper bits may be masked).
 *
 * @returns Effective stack addressf for the push.
 * @param   pIemCpu             The per CPU data.
 * @param   pCtx                Where to get the current stack mode.
 * @param   pTmpRsp             The temporary stack pointer.  This is updated.
 * @param   cbItem              The size of the stack item to pop.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPushEx(PCIEMCPU pIemCpu, PCCPUMCTX pCtx, PRTUINT64U pTmpRsp, uint8_t cbItem)
{
    RTGCPTR GCPtrTop;

    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        GCPtrTop = pTmpRsp->u          -= cbItem;
    else if (pCtx->ss.Attr.n.u1DefBig)
        GCPtrTop = pTmpRsp->DWords.dw0 -= cbItem;
    else
        GCPtrTop = pTmpRsp->Words.w0   -= cbItem;
    return GCPtrTop;
}


/**
 * Gets the effective stack address for a pop of the specified size and
 * calculates and updates the temporary RSP.
 *
 * @returns Current stack pointer.
 * @param   pIemCpu             The per CPU data.
 * @param   pCtx                Where to get the current stack mode.
 * @param   pTmpRsp             The temporary stack pointer.  This is updated.
 * @param   cbItem              The size of the stack item to pop.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPopEx(PCIEMCPU pIemCpu, PCCPUMCTX pCtx, PRTUINT64U pTmpRsp, uint8_t cbItem)
{
    RTGCPTR GCPtrTop;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        GCPtrTop = pTmpRsp->u;
        pTmpRsp->u          += cbItem;
    }
    else if (pCtx->ss.Attr.n.u1DefBig)
    {
        GCPtrTop = pTmpRsp->DWords.dw0;
        pTmpRsp->DWords.dw0 += cbItem;
    }
    else
    {
        GCPtrTop = pTmpRsp->Words.w0;
        pTmpRsp->Words.w0   += cbItem;
    }
    return GCPtrTop;
}

/** @}  */


/** @name   FPU access and helpers.
 *
 * @{
 */


/**
 * Hook for preparing to use the host FPU.
 *
 * This is necessary in ring-0 and raw-mode context.
 *
 * @param   pIemCpu             The IEM per CPU data.
 */
DECLINLINE(void) iemFpuPrepareUsage(PIEMCPU pIemCpu)
{
#ifdef IN_RING3
    NOREF(pIemCpu);
#else
/** @todo RZ: FIXME */
//# error "Implement me"
#endif
}


/**
 * Hook for preparing to use the host FPU for SSE
 *
 * This is necessary in ring-0 and raw-mode context.
 *
 * @param   pIemCpu             The IEM per CPU data.
 */
DECLINLINE(void) iemFpuPrepareUsageSse(PIEMCPU pIemCpu)
{
    iemFpuPrepareUsage(pIemCpu);
}


/**
 * Stores a QNaN value into a FPU register.
 *
 * @param   pReg                Pointer to the register.
 */
DECLINLINE(void) iemFpuStoreQNan(PRTFLOAT80U pReg)
{
    pReg->au32[0] = UINT32_C(0x00000000);
    pReg->au32[1] = UINT32_C(0xc0000000);
    pReg->au16[4] = UINT16_C(0xffff);
}


/**
 * Updates the FOP, FPU.CS and FPUIP registers.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pCtx                The CPU context.
 * @param   pFpuCtx             The FPU context.
 */
DECLINLINE(void) iemFpuUpdateOpcodeAndIpWorker(PIEMCPU pIemCpu, PCPUMCTX pCtx, PX86FXSTATE pFpuCtx)
{
    pFpuCtx->FOP       = pIemCpu->abOpcode[pIemCpu->offFpuOpcode]
                       | ((uint16_t)(pIemCpu->abOpcode[pIemCpu->offFpuOpcode - 1] & 0x7) << 8);
    /** @todo x87.CS and FPUIP needs to be kept seperately. */
    if (IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        /** @todo Testcase: making assumptions about how FPUIP and FPUDP are handled
         *        happens in real mode here based on the fnsave and fnstenv images. */
        pFpuCtx->CS    = 0;
        pFpuCtx->FPUIP = pCtx->eip | ((uint32_t)pCtx->cs.Sel << 4);
    }
    else
    {
        pFpuCtx->CS    = pCtx->cs.Sel;
        pFpuCtx->FPUIP = pCtx->rip;
    }
}


/**
 * Updates the x87.DS and FPUDP registers.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pCtx                The CPU context.
 * @param   pFpuCtx             The FPU context.
 * @param   iEffSeg             The effective segment register.
 * @param   GCPtrEff            The effective address relative to @a iEffSeg.
 */
DECLINLINE(void) iemFpuUpdateDP(PIEMCPU pIemCpu, PCPUMCTX pCtx, PX86FXSTATE pFpuCtx, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    RTSEL sel;
    switch (iEffSeg)
    {
        case X86_SREG_DS: sel = pCtx->ds.Sel; break;
        case X86_SREG_SS: sel = pCtx->ss.Sel; break;
        case X86_SREG_CS: sel = pCtx->cs.Sel; break;
        case X86_SREG_ES: sel = pCtx->es.Sel; break;
        case X86_SREG_FS: sel = pCtx->fs.Sel; break;
        case X86_SREG_GS: sel = pCtx->gs.Sel; break;
        default:
            AssertMsgFailed(("%d\n", iEffSeg));
            sel = pCtx->ds.Sel;
    }
    /** @todo pFpuCtx->DS and FPUDP needs to be kept seperately. */
    if (IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        pFpuCtx->DS    = 0;
        pFpuCtx->FPUDP = (uint32_t)GCPtrEff | ((uint32_t)sel << 4);
    }
    else
    {
        pFpuCtx->DS    = sel;
        pFpuCtx->FPUDP = GCPtrEff;
    }
}


/**
 * Rotates the stack registers in the push direction.
 *
 * @param   pFpuCtx             The FPU context.
 * @remarks This is a complete waste of time, but fxsave stores the registers in
 *          stack order.
 */
DECLINLINE(void) iemFpuRotateStackPush(PX86FXSTATE pFpuCtx)
{
    RTFLOAT80U r80Tmp = pFpuCtx->aRegs[7].r80;
    pFpuCtx->aRegs[7].r80 = pFpuCtx->aRegs[6].r80;
    pFpuCtx->aRegs[6].r80 = pFpuCtx->aRegs[5].r80;
    pFpuCtx->aRegs[5].r80 = pFpuCtx->aRegs[4].r80;
    pFpuCtx->aRegs[4].r80 = pFpuCtx->aRegs[3].r80;
    pFpuCtx->aRegs[3].r80 = pFpuCtx->aRegs[2].r80;
    pFpuCtx->aRegs[2].r80 = pFpuCtx->aRegs[1].r80;
    pFpuCtx->aRegs[1].r80 = pFpuCtx->aRegs[0].r80;
    pFpuCtx->aRegs[0].r80 = r80Tmp;
}


/**
 * Rotates the stack registers in the pop direction.
 *
 * @param   pFpuCtx             The FPU context.
 * @remarks This is a complete waste of time, but fxsave stores the registers in
 *          stack order.
 */
DECLINLINE(void) iemFpuRotateStackPop(PX86FXSTATE pFpuCtx)
{
    RTFLOAT80U r80Tmp = pFpuCtx->aRegs[0].r80;
    pFpuCtx->aRegs[0].r80 = pFpuCtx->aRegs[1].r80;
    pFpuCtx->aRegs[1].r80 = pFpuCtx->aRegs[2].r80;
    pFpuCtx->aRegs[2].r80 = pFpuCtx->aRegs[3].r80;
    pFpuCtx->aRegs[3].r80 = pFpuCtx->aRegs[4].r80;
    pFpuCtx->aRegs[4].r80 = pFpuCtx->aRegs[5].r80;
    pFpuCtx->aRegs[5].r80 = pFpuCtx->aRegs[6].r80;
    pFpuCtx->aRegs[6].r80 = pFpuCtx->aRegs[7].r80;
    pFpuCtx->aRegs[7].r80 = r80Tmp;
}


/**
 * Updates FSW and pushes a FPU result onto the FPU stack if no pending
 * exception prevents it.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The FPU operation result to push.
 * @param   pFpuCtx             The FPU context.
 */
IEM_STATIC void iemFpuMaybePushResult(PIEMCPU pIemCpu, PIEMFPURESULT pResult, PX86FXSTATE pFpuCtx)
{
    /* Update FSW and bail if there are pending exceptions afterwards. */
    uint16_t fFsw = pFpuCtx->FSW & ~X86_FSW_C_MASK;
    fFsw |= pResult->FSW & ~X86_FSW_TOP_MASK;
    if (   (fFsw             & (X86_FSW_IE | X86_FSW_ZE | X86_FSW_DE))
        & ~(pFpuCtx->FCW & (X86_FCW_IM | X86_FCW_ZM | X86_FCW_DM)))
    {
        pFpuCtx->FSW = fFsw;
        return;
    }

    uint16_t iNewTop = (X86_FSW_TOP_GET(fFsw) + 7) & X86_FSW_TOP_SMASK;
    if (!(pFpuCtx->FTW & RT_BIT(iNewTop)))
    {
        /* All is fine, push the actual value. */
        pFpuCtx->FTW |= RT_BIT(iNewTop);
        pFpuCtx->aRegs[7].r80 = pResult->r80Result;
    }
    else if (pFpuCtx->FCW & X86_FCW_IM)
    {
        /* Masked stack overflow, push QNaN. */
        fFsw |= X86_FSW_IE | X86_FSW_SF | X86_FSW_C1;
        iemFpuStoreQNan(&pFpuCtx->aRegs[7].r80);
    }
    else
    {
        /* Raise stack overflow, don't push anything. */
        pFpuCtx->FSW |= pResult->FSW & ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF | X86_FSW_C1 | X86_FSW_B | X86_FSW_ES;
        return;
    }

    fFsw &= ~X86_FSW_TOP_MASK;
    fFsw |= iNewTop << X86_FSW_TOP_SHIFT;
    pFpuCtx->FSW = fFsw;

    iemFpuRotateStackPush(pFpuCtx);
}


/**
 * Stores a result in a FPU register and updates the FSW and FTW.
 *
 * @param   pFpuCtx             The FPU context.
 * @param   pResult             The result to store.
 * @param   iStReg              Which FPU register to store it in.
 */
IEM_STATIC void iemFpuStoreResultOnly(PX86FXSTATE pFpuCtx, PIEMFPURESULT pResult, uint8_t iStReg)
{
    Assert(iStReg < 8);
    uint16_t iReg = (X86_FSW_TOP_GET(pFpuCtx->FSW) + iStReg) & X86_FSW_TOP_SMASK;
    pFpuCtx->FSW &= ~X86_FSW_C_MASK;
    pFpuCtx->FSW |= pResult->FSW & ~X86_FSW_TOP_MASK;
    pFpuCtx->FTW |= RT_BIT(iReg);
    pFpuCtx->aRegs[iStReg].r80 = pResult->r80Result;
}


/**
 * Only updates the FPU status word (FSW) with the result of the current
 * instruction.
 *
 * @param   pFpuCtx             The FPU context.
 * @param   u16FSW              The FSW output of the current instruction.
 */
IEM_STATIC void iemFpuUpdateFSWOnly(PX86FXSTATE pFpuCtx, uint16_t u16FSW)
{
    pFpuCtx->FSW &= ~X86_FSW_C_MASK;
    pFpuCtx->FSW |= u16FSW & ~X86_FSW_TOP_MASK;
}


/**
 * Pops one item off the FPU stack if no pending exception prevents it.
 *
 * @param   pFpuCtx             The FPU context.
 */
IEM_STATIC void iemFpuMaybePopOne(PX86FXSTATE pFpuCtx)
{
    /* Check pending exceptions. */
    uint16_t uFSW = pFpuCtx->FSW;
    if (   (pFpuCtx->FSW & (X86_FSW_IE | X86_FSW_ZE | X86_FSW_DE))
        & ~(pFpuCtx->FCW & (X86_FCW_IM | X86_FCW_ZM | X86_FCW_DM)))
        return;

    /* TOP--. */
    uint16_t iOldTop = uFSW & X86_FSW_TOP_MASK;
    uFSW &= ~X86_FSW_TOP_MASK;
    uFSW |= (iOldTop + (UINT16_C(9) << X86_FSW_TOP_SHIFT)) & X86_FSW_TOP_MASK;
    pFpuCtx->FSW = uFSW;

    /* Mark the previous ST0 as empty. */
    iOldTop >>= X86_FSW_TOP_SHIFT;
    pFpuCtx->FTW &= ~RT_BIT(iOldTop);

    /* Rotate the registers. */
    iemFpuRotateStackPop(pFpuCtx);
}


/**
 * Pushes a FPU result onto the FPU stack if no pending exception prevents it.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The FPU operation result to push.
 */
IEM_STATIC void iemFpuPushResult(PIEMCPU pIemCpu, PIEMFPURESULT pResult)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuMaybePushResult(pIemCpu, pResult, pFpuCtx);
}


/**
 * Pushes a FPU result onto the FPU stack if no pending exception prevents it,
 * and sets FPUDP and FPUDS.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The FPU operation result to push.
 * @param   iEffSeg             The effective segment register.
 * @param   GCPtrEff            The effective address relative to @a iEffSeg.
 */
IEM_STATIC void iemFpuPushResultWithMemOp(PIEMCPU pIemCpu, PIEMFPURESULT pResult, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuMaybePushResult(pIemCpu, pResult, pFpuCtx);
}


/**
 * Replace ST0 with the first value and push the second onto the FPU stack,
 * unless a pending exception prevents it.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The FPU operation result to store and push.
 */
IEM_STATIC void iemFpuPushResultTwo(PIEMCPU pIemCpu, PIEMFPURESULTTWO pResult)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);

    /* Update FSW and bail if there are pending exceptions afterwards. */
    uint16_t fFsw = pFpuCtx->FSW & ~X86_FSW_C_MASK;
    fFsw |= pResult->FSW & ~X86_FSW_TOP_MASK;
    if (   (fFsw             & (X86_FSW_IE | X86_FSW_ZE | X86_FSW_DE))
        & ~(pFpuCtx->FCW & (X86_FCW_IM | X86_FCW_ZM | X86_FCW_DM)))
    {
        pFpuCtx->FSW = fFsw;
        return;
    }

    uint16_t iNewTop = (X86_FSW_TOP_GET(fFsw) + 7) & X86_FSW_TOP_SMASK;
    if (!(pFpuCtx->FTW & RT_BIT(iNewTop)))
    {
        /* All is fine, push the actual value. */
        pFpuCtx->FTW |= RT_BIT(iNewTop);
        pFpuCtx->aRegs[0].r80 = pResult->r80Result1;
        pFpuCtx->aRegs[7].r80 = pResult->r80Result2;
    }
    else if (pFpuCtx->FCW & X86_FCW_IM)
    {
        /* Masked stack overflow, push QNaN. */
        fFsw |= X86_FSW_IE | X86_FSW_SF | X86_FSW_C1;
        iemFpuStoreQNan(&pFpuCtx->aRegs[0].r80);
        iemFpuStoreQNan(&pFpuCtx->aRegs[7].r80);
    }
    else
    {
        /* Raise stack overflow, don't push anything. */
        pFpuCtx->FSW |= pResult->FSW & ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF | X86_FSW_C1 | X86_FSW_B | X86_FSW_ES;
        return;
    }

    fFsw &= ~X86_FSW_TOP_MASK;
    fFsw |= iNewTop << X86_FSW_TOP_SHIFT;
    pFpuCtx->FSW = fFsw;

    iemFpuRotateStackPush(pFpuCtx);
}


/**
 * Stores a result in a FPU register, updates the FSW, FTW, FPUIP, FPUCS, and
 * FOP.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The result to store.
 * @param   iStReg              Which FPU register to store it in.
 */
IEM_STATIC void iemFpuStoreResult(PIEMCPU pIemCpu, PIEMFPURESULT pResult, uint8_t iStReg)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStoreResultOnly(pFpuCtx, pResult, iStReg);
}


/**
 * Stores a result in a FPU register, updates the FSW, FTW, FPUIP, FPUCS, and
 * FOP, and then pops the stack.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The result to store.
 * @param   iStReg              Which FPU register to store it in.
 */
IEM_STATIC void iemFpuStoreResultThenPop(PIEMCPU pIemCpu, PIEMFPURESULT pResult, uint8_t iStReg)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStoreResultOnly(pFpuCtx, pResult, iStReg);
    iemFpuMaybePopOne(pFpuCtx);
}


/**
 * Stores a result in a FPU register, updates the FSW, FTW, FPUIP, FPUCS, FOP,
 * FPUDP, and FPUDS.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The result to store.
 * @param   iStReg              Which FPU register to store it in.
 * @param   iEffSeg             The effective memory operand selector register.
 * @param   GCPtrEff            The effective memory operand offset.
 */
IEM_STATIC void iemFpuStoreResultWithMemOp(PIEMCPU pIemCpu, PIEMFPURESULT pResult, uint8_t iStReg,
                                           uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStoreResultOnly(pFpuCtx, pResult, iStReg);
}


/**
 * Stores a result in a FPU register, updates the FSW, FTW, FPUIP, FPUCS, FOP,
 * FPUDP, and FPUDS, and then pops the stack.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pResult             The result to store.
 * @param   iStReg              Which FPU register to store it in.
 * @param   iEffSeg             The effective memory operand selector register.
 * @param   GCPtrEff            The effective memory operand offset.
 */
IEM_STATIC void iemFpuStoreResultWithMemOpThenPop(PIEMCPU pIemCpu, PIEMFPURESULT pResult,
                                                  uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStoreResultOnly(pFpuCtx, pResult, iStReg);
    iemFpuMaybePopOne(pFpuCtx);
}


/**
 * Updates the FOP, FPUIP, and FPUCS.  For FNOP.
 *
 * @param   pIemCpu             The IEM per CPU data.
 */
IEM_STATIC void iemFpuUpdateOpcodeAndIp(PIEMCPU pIemCpu)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
}


/**
 * Marks the specified stack register as free (for FFREE).
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iStReg              The register to free.
 */
IEM_STATIC void iemFpuStackFree(PIEMCPU pIemCpu, uint8_t iStReg)
{
    Assert(iStReg < 8);
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint8_t     iReg    = (X86_FSW_TOP_GET(pFpuCtx->FSW) + iStReg) & X86_FSW_TOP_SMASK;
    pFpuCtx->FTW &= ~RT_BIT(iReg);
}


/**
 * Increments FSW.TOP, i.e. pops an item off the stack without freeing it.
 *
 * @param   pIemCpu             The IEM per CPU data.
 */
IEM_STATIC void iemFpuStackIncTop(PIEMCPU pIemCpu)
{
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint16_t    uFsw    = pFpuCtx->FSW;
    uint16_t    uTop    = uFsw & X86_FSW_TOP_MASK;
    uTop  = (uTop + (1 << X86_FSW_TOP_SHIFT)) & X86_FSW_TOP_MASK;
    uFsw &= ~X86_FSW_TOP_MASK;
    uFsw |= uTop;
    pFpuCtx->FSW = uFsw;
}


/**
 * Decrements FSW.TOP, i.e. push an item off the stack without storing anything.
 *
 * @param   pIemCpu             The IEM per CPU data.
 */
IEM_STATIC void iemFpuStackDecTop(PIEMCPU pIemCpu)
{
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint16_t    uFsw    = pFpuCtx->FSW;
    uint16_t    uTop    = uFsw & X86_FSW_TOP_MASK;
    uTop  = (uTop + (7 << X86_FSW_TOP_SHIFT)) & X86_FSW_TOP_MASK;
    uFsw &= ~X86_FSW_TOP_MASK;
    uFsw |= uTop;
    pFpuCtx->FSW = uFsw;
}


/**
 * Updates the FSW, FOP, FPUIP, and FPUCS.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16FSW              The FSW from the current instruction.
 */
IEM_STATIC void iemFpuUpdateFSW(PIEMCPU pIemCpu, uint16_t u16FSW)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuUpdateFSWOnly(pFpuCtx, u16FSW);
}


/**
 * Updates the FSW, FOP, FPUIP, and FPUCS, then pops the stack.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16FSW              The FSW from the current instruction.
 */
IEM_STATIC void iemFpuUpdateFSWThenPop(PIEMCPU pIemCpu, uint16_t u16FSW)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuUpdateFSWOnly(pFpuCtx, u16FSW);
    iemFpuMaybePopOne(pFpuCtx);
}


/**
 * Updates the FSW, FOP, FPUIP, FPUCS, FPUDP, and FPUDS.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16FSW              The FSW from the current instruction.
 * @param   iEffSeg             The effective memory operand selector register.
 * @param   GCPtrEff            The effective memory operand offset.
 */
IEM_STATIC void iemFpuUpdateFSWWithMemOp(PIEMCPU pIemCpu, uint16_t u16FSW, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuUpdateFSWOnly(pFpuCtx, u16FSW);
}


/**
 * Updates the FSW, FOP, FPUIP, and FPUCS, then pops the stack twice.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16FSW              The FSW from the current instruction.
 */
IEM_STATIC void iemFpuUpdateFSWThenPopPop(PIEMCPU pIemCpu, uint16_t u16FSW)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuUpdateFSWOnly(pFpuCtx, u16FSW);
    iemFpuMaybePopOne(pFpuCtx);
    iemFpuMaybePopOne(pFpuCtx);
}


/**
 * Updates the FSW, FOP, FPUIP, FPUCS, FPUDP, and FPUDS, then pops the stack.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16FSW              The FSW from the current instruction.
 * @param   iEffSeg             The effective memory operand selector register.
 * @param   GCPtrEff            The effective memory operand offset.
 */
IEM_STATIC void iemFpuUpdateFSWWithMemOpThenPop(PIEMCPU pIemCpu, uint16_t u16FSW, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuUpdateFSWOnly(pFpuCtx, u16FSW);
    iemFpuMaybePopOne(pFpuCtx);
}


/**
 * Worker routine for raising an FPU stack underflow exception.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pFpuCtx             The FPU context.
 * @param   iStReg              The stack register being accessed.
 */
IEM_STATIC void iemFpuStackUnderflowOnly(PIEMCPU pIemCpu, PX86FXSTATE pFpuCtx, uint8_t iStReg)
{
    Assert(iStReg < 8 || iStReg == UINT8_MAX);
    if (pFpuCtx->FCW & X86_FCW_IM)
    {
        /* Masked underflow. */
        pFpuCtx->FSW &= ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF;
        uint16_t iReg = (X86_FSW_TOP_GET(pFpuCtx->FSW) + iStReg) & X86_FSW_TOP_SMASK;
        if (iStReg != UINT8_MAX)
        {
            pFpuCtx->FTW |= RT_BIT(iReg);
            iemFpuStoreQNan(&pFpuCtx->aRegs[iStReg].r80);
        }
    }
    else
    {
        pFpuCtx->FSW &= ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF | X86_FSW_ES | X86_FSW_B;
    }
}


/**
 * Raises a FPU stack underflow exception.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iStReg              The destination register that should be loaded
 *                              with QNaN if \#IS is not masked. Specify
 *                              UINT8_MAX if none (like for fcom).
 */
DECL_NO_INLINE(IEM_STATIC, void) iemFpuStackUnderflow(PIEMCPU pIemCpu, uint8_t iStReg)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackUnderflowOnly(pIemCpu, pFpuCtx, iStReg);
}


DECL_NO_INLINE(IEM_STATIC, void)
iemFpuStackUnderflowWithMemOp(PIEMCPU pIemCpu, uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackUnderflowOnly(pIemCpu, pFpuCtx, iStReg);
}


DECL_NO_INLINE(IEM_STATIC, void) iemFpuStackUnderflowThenPop(PIEMCPU pIemCpu, uint8_t iStReg)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackUnderflowOnly(pIemCpu, pFpuCtx, iStReg);
    iemFpuMaybePopOne(pFpuCtx);
}


DECL_NO_INLINE(IEM_STATIC, void)
iemFpuStackUnderflowWithMemOpThenPop(PIEMCPU pIemCpu, uint8_t iStReg, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx      = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackUnderflowOnly(pIemCpu, pFpuCtx, iStReg);
    iemFpuMaybePopOne(pFpuCtx);
}


DECL_NO_INLINE(IEM_STATIC, void) iemFpuStackUnderflowThenPopPop(PIEMCPU pIemCpu)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackUnderflowOnly(pIemCpu, pFpuCtx, UINT8_MAX);
    iemFpuMaybePopOne(pFpuCtx);
    iemFpuMaybePopOne(pFpuCtx);
}


DECL_NO_INLINE(IEM_STATIC, void)
iemFpuStackPushUnderflow(PIEMCPU pIemCpu)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);

    if (pFpuCtx->FCW & X86_FCW_IM)
    {
        /* Masked overflow - Push QNaN. */
        uint16_t iNewTop = (X86_FSW_TOP_GET(pFpuCtx->FSW) + 7) & X86_FSW_TOP_SMASK;
        pFpuCtx->FSW &= ~(X86_FSW_TOP_MASK | X86_FSW_C_MASK);
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF;
        pFpuCtx->FSW |= iNewTop << X86_FSW_TOP_SHIFT;
        pFpuCtx->FTW |= RT_BIT(iNewTop);
        iemFpuStoreQNan(&pFpuCtx->aRegs[7].r80);
        iemFpuRotateStackPush(pFpuCtx);
    }
    else
    {
        /* Exception pending - don't change TOP or the register stack. */
        pFpuCtx->FSW &= ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF | X86_FSW_ES | X86_FSW_B;
    }
}


DECL_NO_INLINE(IEM_STATIC, void)
iemFpuStackPushUnderflowTwo(PIEMCPU pIemCpu)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);

    if (pFpuCtx->FCW & X86_FCW_IM)
    {
        /* Masked overflow - Push QNaN. */
        uint16_t iNewTop = (X86_FSW_TOP_GET(pFpuCtx->FSW) + 7) & X86_FSW_TOP_SMASK;
        pFpuCtx->FSW &= ~(X86_FSW_TOP_MASK | X86_FSW_C_MASK);
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF;
        pFpuCtx->FSW |= iNewTop << X86_FSW_TOP_SHIFT;
        pFpuCtx->FTW |= RT_BIT(iNewTop);
        iemFpuStoreQNan(&pFpuCtx->aRegs[0].r80);
        iemFpuStoreQNan(&pFpuCtx->aRegs[7].r80);
        iemFpuRotateStackPush(pFpuCtx);
    }
    else
    {
        /* Exception pending - don't change TOP or the register stack. */
        pFpuCtx->FSW &= ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_IE | X86_FSW_SF | X86_FSW_ES | X86_FSW_B;
    }
}


/**
 * Worker routine for raising an FPU stack overflow exception on a push.
 *
 * @param   pFpuCtx             The FPU context.
 */
IEM_STATIC void iemFpuStackPushOverflowOnly(PX86FXSTATE pFpuCtx)
{
    if (pFpuCtx->FCW & X86_FCW_IM)
    {
        /* Masked overflow. */
        uint16_t iNewTop = (X86_FSW_TOP_GET(pFpuCtx->FSW) + 7) & X86_FSW_TOP_SMASK;
        pFpuCtx->FSW &= ~(X86_FSW_TOP_MASK | X86_FSW_C_MASK);
        pFpuCtx->FSW |= X86_FSW_C1 | X86_FSW_IE | X86_FSW_SF;
        pFpuCtx->FSW |= iNewTop << X86_FSW_TOP_SHIFT;
        pFpuCtx->FTW |= RT_BIT(iNewTop);
        iemFpuStoreQNan(&pFpuCtx->aRegs[7].r80);
        iemFpuRotateStackPush(pFpuCtx);
    }
    else
    {
        /* Exception pending - don't change TOP or the register stack. */
        pFpuCtx->FSW &= ~X86_FSW_C_MASK;
        pFpuCtx->FSW |= X86_FSW_C1 | X86_FSW_IE | X86_FSW_SF | X86_FSW_ES | X86_FSW_B;
    }
}


/**
 * Raises a FPU stack overflow exception on a push.
 *
 * @param   pIemCpu             The IEM per CPU data.
 */
DECL_NO_INLINE(IEM_STATIC, void) iemFpuStackPushOverflow(PIEMCPU pIemCpu)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackPushOverflowOnly(pFpuCtx);
}


/**
 * Raises a FPU stack overflow exception on a push with a memory operand.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iEffSeg             The effective memory operand selector register.
 * @param   GCPtrEff            The effective memory operand offset.
 */
DECL_NO_INLINE(IEM_STATIC, void)
iemFpuStackPushOverflowWithMemOp(PIEMCPU pIemCpu, uint8_t iEffSeg, RTGCPTR GCPtrEff)
{
    PCPUMCTX    pCtx    = pIemCpu->CTX_SUFF(pCtx);
    PX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
    iemFpuUpdateDP(pIemCpu, pCtx, pFpuCtx, iEffSeg, GCPtrEff);
    iemFpuUpdateOpcodeAndIpWorker(pIemCpu, pCtx, pFpuCtx);
    iemFpuStackPushOverflowOnly(pFpuCtx);
}


IEM_STATIC int iemFpuStRegNotEmpty(PIEMCPU pIemCpu, uint8_t iStReg)
{
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint16_t    iReg    = (X86_FSW_TOP_GET(pFpuCtx->FSW) + iStReg) & X86_FSW_TOP_SMASK;
    if (pFpuCtx->FTW & RT_BIT(iReg))
        return VINF_SUCCESS;
    return VERR_NOT_FOUND;
}


IEM_STATIC int iemFpuStRegNotEmptyRef(PIEMCPU pIemCpu, uint8_t iStReg, PCRTFLOAT80U *ppRef)
{
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint16_t    iReg    = (X86_FSW_TOP_GET(pFpuCtx->FSW) + iStReg) & X86_FSW_TOP_SMASK;
    if (pFpuCtx->FTW & RT_BIT(iReg))
    {
        *ppRef = &pFpuCtx->aRegs[iStReg].r80;
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}


IEM_STATIC int iemFpu2StRegsNotEmptyRef(PIEMCPU pIemCpu, uint8_t iStReg0, PCRTFLOAT80U *ppRef0,
                                        uint8_t iStReg1, PCRTFLOAT80U *ppRef1)
{
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint16_t    iTop    = X86_FSW_TOP_GET(pFpuCtx->FSW);
    uint16_t    iReg0   = (iTop + iStReg0) & X86_FSW_TOP_SMASK;
    uint16_t    iReg1   = (iTop + iStReg1) & X86_FSW_TOP_SMASK;
    if ((pFpuCtx->FTW & (RT_BIT(iReg0) | RT_BIT(iReg1))) == (RT_BIT(iReg0) | RT_BIT(iReg1)))
    {
        *ppRef0 = &pFpuCtx->aRegs[iStReg0].r80;
        *ppRef1 = &pFpuCtx->aRegs[iStReg1].r80;
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}


IEM_STATIC int iemFpu2StRegsNotEmptyRefFirst(PIEMCPU pIemCpu, uint8_t iStReg0, PCRTFLOAT80U *ppRef0, uint8_t iStReg1)
{
    PX86FXSTATE pFpuCtx = &pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87;
    uint16_t    iTop    = X86_FSW_TOP_GET(pFpuCtx->FSW);
    uint16_t    iReg0   = (iTop + iStReg0) & X86_FSW_TOP_SMASK;
    uint16_t    iReg1   = (iTop + iStReg1) & X86_FSW_TOP_SMASK;
    if ((pFpuCtx->FTW & (RT_BIT(iReg0) | RT_BIT(iReg1))) == (RT_BIT(iReg0) | RT_BIT(iReg1)))
    {
        *ppRef0 = &pFpuCtx->aRegs[iStReg0].r80;
        return VINF_SUCCESS;
    }
    return VERR_NOT_FOUND;
}


/**
 * Updates the FPU exception status after FCW is changed.
 *
 * @param   pFpuCtx             The FPU context.
 */
IEM_STATIC void iemFpuRecalcExceptionStatus(PX86FXSTATE pFpuCtx)
{
    uint16_t u16Fsw = pFpuCtx->FSW;
    if ((u16Fsw & X86_FSW_XCPT_MASK) & ~(pFpuCtx->FCW & X86_FCW_XCPT_MASK))
        u16Fsw |= X86_FSW_ES | X86_FSW_B;
    else
        u16Fsw &= ~(X86_FSW_ES | X86_FSW_B);
    pFpuCtx->FSW = u16Fsw;
}


/**
 * Calculates the full FTW (FPU tag word) for use in FNSTENV and FNSAVE.
 *
 * @returns The full FTW.
 * @param   pFpuCtx             The FPU context.
 */
IEM_STATIC uint16_t iemFpuCalcFullFtw(PCX86FXSTATE pFpuCtx)
{
    uint8_t const   u8Ftw  = (uint8_t)pFpuCtx->FTW;
    uint16_t        u16Ftw = 0;
    unsigned const  iTop   = X86_FSW_TOP_GET(pFpuCtx->FSW);
    for (unsigned iSt = 0; iSt < 8; iSt++)
    {
        unsigned const iReg = (iSt + iTop) & 7;
        if (!(u8Ftw & RT_BIT(iReg)))
            u16Ftw |= 3 << (iReg * 2); /* empty */
        else
        {
            uint16_t uTag;
            PCRTFLOAT80U const pr80Reg = &pFpuCtx->aRegs[iSt].r80;
            if (pr80Reg->s.uExponent == 0x7fff)
                uTag = 2; /* Exponent is all 1's => Special. */
            else if (pr80Reg->s.uExponent == 0x0000)
            {
                if (pr80Reg->s.u64Mantissa == 0x0000)
                    uTag = 1; /* All bits are zero => Zero. */
                else
                    uTag = 2; /* Must be special. */
            }
            else if (pr80Reg->s.u64Mantissa & RT_BIT_64(63)) /* The J bit. */
                uTag = 0; /* Valid. */
            else
                uTag = 2; /* Must be special. */

            u16Ftw |= uTag << (iReg * 2); /* empty */
        }
    }

    return u16Ftw;
}


/**
 * Converts a full FTW to a compressed one (for use in FLDENV and FRSTOR).
 *
 * @returns The compressed FTW.
 * @param   u16FullFtw      The full FTW to convert.
 */
IEM_STATIC uint16_t iemFpuCompressFtw(uint16_t u16FullFtw)
{
    uint8_t u8Ftw = 0;
    for (unsigned i = 0; i < 8; i++)
    {
        if ((u16FullFtw & 3) != 3 /*empty*/)
            u8Ftw |= RT_BIT(i);
        u16FullFtw >>= 2;
    }

    return u8Ftw;
}

/** @}  */


/** @name   Memory access.
 *
 * @{
 */


/**
 * Updates the IEMCPU::cbWritten counter if applicable.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   fAccess             The access being accounted for.
 * @param   cbMem               The access size.
 */
DECL_FORCE_INLINE(void) iemMemUpdateWrittenCounter(PIEMCPU pIemCpu, uint32_t fAccess, size_t cbMem)
{
    if (   (fAccess & (IEM_ACCESS_WHAT_MASK | IEM_ACCESS_TYPE_WRITE)) == (IEM_ACCESS_WHAT_STACK | IEM_ACCESS_TYPE_WRITE)
        || (fAccess & (IEM_ACCESS_WHAT_MASK | IEM_ACCESS_TYPE_WRITE)) == (IEM_ACCESS_WHAT_DATA | IEM_ACCESS_TYPE_WRITE) )
        pIemCpu->cbWritten += (uint32_t)cbMem;
}


/**
 * Checks if the given segment can be written to, raise the appropriate
 * exception if not.
 *
 * @returns VBox strict status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pHid                Pointer to the hidden register.
 * @param   iSegReg             The register number.
 * @param   pu64BaseAddr        Where to return the base address to use for the
 *                              segment. (In 64-bit code it may differ from the
 *                              base in the hidden segment.)
 */
IEM_STATIC VBOXSTRICTRC
iemMemSegCheckWriteAccessEx(PIEMCPU pIemCpu, PCCPUMSELREGHID pHid, uint8_t iSegReg, uint64_t *pu64BaseAddr)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        *pu64BaseAddr = iSegReg < X86_SREG_FS ? 0 : pHid->u64Base;
    else
    {
        if (!pHid->Attr.n.u1Present)
            return iemRaiseSelectorNotPresentBySegReg(pIemCpu, iSegReg);

        if (   (   (pHid->Attr.n.u4Type & X86_SEL_TYPE_CODE)
                || !(pHid->Attr.n.u4Type & X86_SEL_TYPE_WRITE) )
            &&  pIemCpu->enmCpuMode != IEMMODE_64BIT )
            return iemRaiseSelectorInvalidAccess(pIemCpu, iSegReg, IEM_ACCESS_DATA_W);
        *pu64BaseAddr = pHid->u64Base;
    }
    return VINF_SUCCESS;
}


/**
 * Checks if the given segment can be read from, raise the appropriate
 * exception if not.
 *
 * @returns VBox strict status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pHid                Pointer to the hidden register.
 * @param   iSegReg             The register number.
 * @param   pu64BaseAddr        Where to return the base address to use for the
 *                              segment. (In 64-bit code it may differ from the
 *                              base in the hidden segment.)
 */
IEM_STATIC VBOXSTRICTRC
iemMemSegCheckReadAccessEx(PIEMCPU pIemCpu, PCCPUMSELREGHID pHid, uint8_t iSegReg, uint64_t *pu64BaseAddr)
{
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        *pu64BaseAddr = iSegReg < X86_SREG_FS ? 0 : pHid->u64Base;
    else
    {
        if (!pHid->Attr.n.u1Present)
            return iemRaiseSelectorNotPresentBySegReg(pIemCpu, iSegReg);

        if ((pHid->Attr.n.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
            return iemRaiseSelectorInvalidAccess(pIemCpu, iSegReg, IEM_ACCESS_DATA_R);
        *pu64BaseAddr = pHid->u64Base;
    }
    return VINF_SUCCESS;
}


/**
 * Applies the segment limit, base and attributes.
 *
 * This may raise a \#GP or \#SS.
 *
 * @returns VBox strict status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   fAccess             The kind of access which is being performed.
 * @param   iSegReg             The index of the segment register to apply.
 *                              This is UINT8_MAX if none (for IDT, GDT, LDT,
 *                              TSS, ++).
 * @param   cbMem               The access size.
 * @param   pGCPtrMem           Pointer to the guest memory address to apply
 *                              segmentation to.  Input and output parameter.
 */
IEM_STATIC VBOXSTRICTRC
iemMemApplySegment(PIEMCPU pIemCpu, uint32_t fAccess, uint8_t iSegReg, size_t cbMem, PRTGCPTR pGCPtrMem)
{
    if (iSegReg == UINT8_MAX)
        return VINF_SUCCESS;

    PCPUMSELREGHID pSel = iemSRegGetHid(pIemCpu, iSegReg);
    switch (pIemCpu->enmCpuMode)
    {
        case IEMMODE_16BIT:
        case IEMMODE_32BIT:
        {
            RTGCPTR32 GCPtrFirst32 = (RTGCPTR32)*pGCPtrMem;
            RTGCPTR32 GCPtrLast32  = GCPtrFirst32 + (uint32_t)cbMem - 1;

            if (   pSel->Attr.n.u1Present
                && !pSel->Attr.n.u1Unusable)
            {
                Assert(pSel->Attr.n.u1DescType);
                if (!(pSel->Attr.n.u4Type & X86_SEL_TYPE_CODE))
                {
                    if (   (fAccess & IEM_ACCESS_TYPE_WRITE)
                        && !(pSel->Attr.n.u4Type & X86_SEL_TYPE_WRITE) )
                        return iemRaiseSelectorInvalidAccess(pIemCpu, iSegReg, fAccess);

                    if (!IEM_IS_REAL_OR_V86_MODE(pIemCpu))
                    {
                        /** @todo CPL check. */
                    }

                    /*
                     * There are two kinds of data selectors, normal and expand down.
                     */
                    if (!(pSel->Attr.n.u4Type & X86_SEL_TYPE_DOWN))
                    {
                        if (   GCPtrFirst32 > pSel->u32Limit
                            || GCPtrLast32  > pSel->u32Limit) /* yes, in real mode too (since 80286). */
                            return iemRaiseSelectorBounds(pIemCpu, iSegReg, fAccess);
                    }
                    else
                    {
                       /*
                        * The upper boundary is defined by the B bit, not the G bit!
                        */
                       if (   GCPtrFirst32 < pSel->u32Limit + UINT32_C(1)
                           || GCPtrLast32  > (pSel->Attr.n.u1DefBig ? UINT32_MAX : UINT32_C(0xffff)))
                          return iemRaiseSelectorBounds(pIemCpu, iSegReg, fAccess);
                    }
                    *pGCPtrMem = GCPtrFirst32 += (uint32_t)pSel->u64Base;
                }
                else
                {

                    /*
                     * Code selector and usually be used to read thru, writing is
                     * only permitted in real and V8086 mode.
                     */
                    if (   (   (fAccess & IEM_ACCESS_TYPE_WRITE)
                            || (   (fAccess & IEM_ACCESS_TYPE_READ)
                               && !(pSel->Attr.n.u4Type & X86_SEL_TYPE_READ)) )
                        && !IEM_IS_REAL_OR_V86_MODE(pIemCpu) )
                        return iemRaiseSelectorInvalidAccess(pIemCpu, iSegReg, fAccess);

                    if (   GCPtrFirst32 > pSel->u32Limit
                        || GCPtrLast32  > pSel->u32Limit) /* yes, in real mode too (since 80286). */
                        return iemRaiseSelectorBounds(pIemCpu, iSegReg, fAccess);

                    if (!IEM_IS_REAL_OR_V86_MODE(pIemCpu))
                    {
                        /** @todo CPL check. */
                    }

                    *pGCPtrMem  = GCPtrFirst32 += (uint32_t)pSel->u64Base;
                }
            }
            else
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            RTGCPTR GCPtrMem = *pGCPtrMem;
            if (iSegReg == X86_SREG_GS || iSegReg == X86_SREG_FS)
                *pGCPtrMem = GCPtrMem + pSel->u64Base;

            Assert(cbMem >= 1);
            if (RT_LIKELY(X86_IS_CANONICAL(GCPtrMem) && X86_IS_CANONICAL(GCPtrMem + cbMem - 1)))
                return VINF_SUCCESS;
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        }

        default:
            AssertFailedReturn(VERR_IEM_IPE_7);
    }
}


/**
 * Translates a virtual address to a physical physical address and checks if we
 * can access the page as specified.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   GCPtrMem            The virtual address.
 * @param   fAccess             The intended access.
 * @param   pGCPhysMem          Where to return the physical address.
 */
IEM_STATIC VBOXSTRICTRC
iemMemPageTranslateAndCheckAccess(PIEMCPU pIemCpu, RTGCPTR GCPtrMem, uint32_t fAccess, PRTGCPHYS pGCPhysMem)
{
    /** @todo Need a different PGM interface here.  We're currently using
     *        generic / REM interfaces. this won't cut it for R0 & RC. */
    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int rc = PGMGstGetPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrMem, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
    {
        /** @todo Check unassigned memory in unpaged mode. */
        /** @todo Reserved bits in page tables. Requires new PGM interface. */
        *pGCPhysMem = NIL_RTGCPHYS;
        return iemRaisePageFault(pIemCpu, GCPtrMem, fAccess, rc);
    }

    /* If the page is writable and does not have the no-exec bit set, all
       access is allowed.  Otherwise we'll have to check more carefully... */
    if ((fFlags & (X86_PTE_RW | X86_PTE_US | X86_PTE_PAE_NX)) != (X86_PTE_RW | X86_PTE_US))
    {
        /* Write to read only memory? */
        if (   (fAccess & IEM_ACCESS_TYPE_WRITE)
            && !(fFlags & X86_PTE_RW)
            && (   pIemCpu->uCpl != 0
                || (pIemCpu->CTX_SUFF(pCtx)->cr0 & X86_CR0_WP)))
        {
            Log(("iemMemPageTranslateAndCheckAccess: GCPtrMem=%RGv - read-only page -> #PF\n", GCPtrMem));
            *pGCPhysMem = NIL_RTGCPHYS;
            return iemRaisePageFault(pIemCpu, GCPtrMem, fAccess & ~IEM_ACCESS_TYPE_READ, VERR_ACCESS_DENIED);
        }

        /* Kernel memory accessed by userland? */
        if (   !(fFlags & X86_PTE_US)
            && pIemCpu->uCpl == 3
            && !(fAccess & IEM_ACCESS_WHAT_SYS))
        {
            Log(("iemMemPageTranslateAndCheckAccess: GCPtrMem=%RGv - user access to kernel page -> #PF\n", GCPtrMem));
            *pGCPhysMem = NIL_RTGCPHYS;
            return iemRaisePageFault(pIemCpu, GCPtrMem, fAccess, VERR_ACCESS_DENIED);
        }

        /* Executing non-executable memory? */
        if (   (fAccess & IEM_ACCESS_TYPE_EXEC)
            && (fFlags & X86_PTE_PAE_NX)
            && (pIemCpu->CTX_SUFF(pCtx)->msrEFER & MSR_K6_EFER_NXE) )
        {
            Log(("iemMemPageTranslateAndCheckAccess: GCPtrMem=%RGv - NX -> #PF\n", GCPtrMem));
            *pGCPhysMem = NIL_RTGCPHYS;
            return iemRaisePageFault(pIemCpu, GCPtrMem, fAccess & ~(IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_WRITE),
                                     VERR_ACCESS_DENIED);
        }
    }

    /*
     * Set the dirty / access flags.
     * ASSUMES this is set when the address is translated rather than on committ...
     */
    /** @todo testcase: check when A and D bits are actually set by the CPU.  */
    uint32_t fAccessedDirty = fAccess & IEM_ACCESS_TYPE_WRITE ? X86_PTE_D | X86_PTE_A : X86_PTE_A;
    if ((fFlags & fAccessedDirty) != fAccessedDirty)
    {
        int rc2 = PGMGstModifyPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrMem, 1, fAccessedDirty, ~(uint64_t)fAccessedDirty);
        AssertRC(rc2);
    }

    GCPhys |= GCPtrMem & PAGE_OFFSET_MASK;
    *pGCPhysMem = GCPhys;
    return VINF_SUCCESS;
}



/**
 * Maps a physical page.
 *
 * @returns VBox status code (see PGMR3PhysTlbGCPhys2Ptr).
 * @param   pIemCpu             The IEM per CPU data.
 * @param   GCPhysMem           The physical address.
 * @param   fAccess             The intended access.
 * @param   ppvMem              Where to return the mapping address.
 * @param   pLock               The PGM lock.
 */
IEM_STATIC int iemMemPageMap(PIEMCPU pIemCpu, RTGCPHYS GCPhysMem, uint32_t fAccess, void **ppvMem, PPGMPAGEMAPLOCK pLock)
{
#ifdef IEM_VERIFICATION_MODE_FULL
    /* Force the alternative path so we can ignore writes. */
    if ((fAccess & IEM_ACCESS_TYPE_WRITE) && !pIemCpu->fNoRem)
    {
        if (IEM_FULL_VERIFICATION_ENABLED(pIemCpu))
        {
            int rc2 = PGMPhysIemQueryAccess(IEMCPU_TO_VM(pIemCpu), GCPhysMem,
                                            RT_BOOL(fAccess & IEM_ACCESS_TYPE_WRITE), pIemCpu->fBypassHandlers);
            if (RT_FAILURE(rc2))
                pIemCpu->fProblematicMemory = true;
        }
        return VERR_PGM_PHYS_TLB_CATCH_ALL;
    }
#endif
#ifdef IEM_LOG_MEMORY_WRITES
    if (fAccess & IEM_ACCESS_TYPE_WRITE)
        return VERR_PGM_PHYS_TLB_CATCH_ALL;
#endif
#ifdef IEM_VERIFICATION_MODE_MINIMAL
    return VERR_PGM_PHYS_TLB_CATCH_ALL;
#endif

    /** @todo This API may require some improving later.  A private deal with PGM
     *        regarding locking and unlocking needs to be struct.  A couple of TLBs
     *        living in PGM, but with publicly accessible inlined access methods
     *        could perhaps be an even better solution. */
    int rc = PGMPhysIemGCPhys2Ptr(IEMCPU_TO_VM(pIemCpu), IEMCPU_TO_VMCPU(pIemCpu),
                                  GCPhysMem,
                                  RT_BOOL(fAccess & IEM_ACCESS_TYPE_WRITE),
                                  pIemCpu->fBypassHandlers,
                                  ppvMem,
                                  pLock);
    /*Log(("PGMPhysIemGCPhys2Ptr %Rrc pLock=%.*Rhxs\n", rc, sizeof(*pLock), pLock));*/
    AssertMsg(rc == VINF_SUCCESS || RT_FAILURE_NP(rc), ("%Rrc\n", rc));

#ifdef IEM_VERIFICATION_MODE_FULL
    if (RT_FAILURE(rc) && IEM_FULL_VERIFICATION_ENABLED(pIemCpu))
        pIemCpu->fProblematicMemory = true;
#endif
    return rc;
}


/**
 * Unmap a page previously mapped by iemMemPageMap.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   GCPhysMem           The physical address.
 * @param   fAccess             The intended access.
 * @param   pvMem               What iemMemPageMap returned.
 * @param   pLock               The PGM lock.
 */
DECLINLINE(void) iemMemPageUnmap(PIEMCPU pIemCpu, RTGCPHYS GCPhysMem, uint32_t fAccess, const void *pvMem, PPGMPAGEMAPLOCK pLock)
{
    NOREF(pIemCpu);
    NOREF(GCPhysMem);
    NOREF(fAccess);
    NOREF(pvMem);
    PGMPhysReleasePageMappingLock(IEMCPU_TO_VM(pIemCpu), pLock);
}


/**
 * Looks up a memory mapping entry.
 *
 * @returns The mapping index (positive) or VERR_NOT_FOUND (negative).
 * @param   pIemCpu         The IEM per CPU data.
 * @param   pvMem           The memory address.
 * @param   fAccess         The access to.
 */
DECLINLINE(int) iemMapLookup(PIEMCPU pIemCpu, void *pvMem, uint32_t fAccess)
{
    Assert(pIemCpu->cActiveMappings < RT_ELEMENTS(pIemCpu->aMemMappings));
    fAccess &= IEM_ACCESS_WHAT_MASK | IEM_ACCESS_TYPE_MASK;
    if (   pIemCpu->aMemMappings[0].pv == pvMem
        && (pIemCpu->aMemMappings[0].fAccess & (IEM_ACCESS_WHAT_MASK | IEM_ACCESS_TYPE_MASK)) == fAccess)
        return 0;
    if (   pIemCpu->aMemMappings[1].pv == pvMem
        && (pIemCpu->aMemMappings[1].fAccess & (IEM_ACCESS_WHAT_MASK | IEM_ACCESS_TYPE_MASK)) == fAccess)
        return 1;
    if (   pIemCpu->aMemMappings[2].pv == pvMem
        && (pIemCpu->aMemMappings[2].fAccess & (IEM_ACCESS_WHAT_MASK | IEM_ACCESS_TYPE_MASK)) == fAccess)
        return 2;
    return VERR_NOT_FOUND;
}


/**
 * Finds a free memmap entry when using iNextMapping doesn't work.
 *
 * @returns Memory mapping index, 1024 on failure.
 * @param   pIemCpu             The IEM per CPU data.
 */
IEM_STATIC unsigned iemMemMapFindFree(PIEMCPU pIemCpu)
{
    /*
     * The easy case.
     */
    if (pIemCpu->cActiveMappings == 0)
    {
        pIemCpu->iNextMapping = 1;
        return 0;
    }

    /* There should be enough mappings for all instructions. */
    AssertReturn(pIemCpu->cActiveMappings < RT_ELEMENTS(pIemCpu->aMemMappings), 1024);

    for (unsigned i = 0; i < RT_ELEMENTS(pIemCpu->aMemMappings); i++)
        if (pIemCpu->aMemMappings[i].fAccess == IEM_ACCESS_INVALID)
            return i;

    AssertFailedReturn(1024);
}


/**
 * Commits a bounce buffer that needs writing back and unmaps it.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu         The IEM per CPU data.
 * @param   iMemMap         The index of the buffer to commit.
 */
IEM_STATIC VBOXSTRICTRC iemMemBounceBufferCommitAndUnmap(PIEMCPU pIemCpu, unsigned iMemMap)
{
    Assert(pIemCpu->aMemMappings[iMemMap].fAccess & IEM_ACCESS_BOUNCE_BUFFERED);
    Assert(pIemCpu->aMemMappings[iMemMap].fAccess & IEM_ACCESS_TYPE_WRITE);

    /*
     * Do the writing.
     */
#ifndef IEM_VERIFICATION_MODE_MINIMAL
    PVM          pVM = IEMCPU_TO_VM(pIemCpu);
    if (   !pIemCpu->aMemBbMappings[iMemMap].fUnassigned
        && !IEM_VERIFICATION_ENABLED(pIemCpu))
    {
        uint16_t const  cbFirst  = pIemCpu->aMemBbMappings[iMemMap].cbFirst;
        uint16_t const  cbSecond = pIemCpu->aMemBbMappings[iMemMap].cbSecond;
        uint8_t const  *pbBuf    = &pIemCpu->aBounceBuffers[iMemMap].ab[0];
        if (!pIemCpu->fBypassHandlers)
        {
            /*
             * Carefully and efficiently dealing with access handler return
             * codes make this a little bloated.
             */
            VBOXSTRICTRC rcStrict = PGMPhysWrite(pVM,
                                                 pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst,
                                                 pbBuf,
                                                 cbFirst,
                                                 PGMACCESSORIGIN_IEM);
            if (rcStrict == VINF_SUCCESS)
            {
                if (cbSecond)
                {
                    rcStrict = PGMPhysWrite(pVM,
                                            pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond,
                                            pbBuf + cbFirst,
                                            cbSecond,
                                            PGMACCESSORIGIN_IEM);
                    if (rcStrict == VINF_SUCCESS)
                    { /* nothing */ }
                    else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
                    {
                        Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x GCPhysSecond=%RGp/%#x %Rrc\n",
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst,
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond, VBOXSTRICTRC_VAL(rcStrict) ));
                        rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                    }
                    else
                    {
                        Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x GCPhysSecond=%RGp/%#x %Rrc (!!)\n",
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst,
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond, VBOXSTRICTRC_VAL(rcStrict) ));
                        return rcStrict;
                    }
                }
            }
            else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
            {
                if (!cbSecond)
                {
                    Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x %Rrc\n",
                         pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst, VBOXSTRICTRC_VAL(rcStrict) ));
                    rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                }
                else
                {
                    VBOXSTRICTRC rcStrict2 = PGMPhysWrite(pVM,
                                                          pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond,
                                                          pbBuf + cbFirst,
                                                          cbSecond,
                                                          PGMACCESSORIGIN_IEM);
                    if (rcStrict2 == VINF_SUCCESS)
                    {
                        Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x %Rrc GCPhysSecond=%RGp/%#x\n",
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst, VBOXSTRICTRC_VAL(rcStrict),
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond));
                        rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                    }
                    else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict2))
                    {
                        Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x %Rrc GCPhysSecond=%RGp/%#x %Rrc\n",
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst, VBOXSTRICTRC_VAL(rcStrict),
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond, VBOXSTRICTRC_VAL(rcStrict2) ));
                        PGM_PHYS_RW_DO_UPDATE_STRICT_RC(rcStrict, rcStrict2);
                        rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                    }
                    else
                    {
                        Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x %Rrc GCPhysSecond=%RGp/%#x %Rrc (!!)\n",
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst, VBOXSTRICTRC_VAL(rcStrict),
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond, VBOXSTRICTRC_VAL(rcStrict2) ));
                        return rcStrict2;
                    }
                }
            }
            else
            {
                Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysWrite GCPhysFirst=%RGp/%#x %Rrc [GCPhysSecond=%RGp/%#x] (!!)\n",
                     pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst, VBOXSTRICTRC_VAL(rcStrict),
                     pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond));
                return rcStrict;
            }
        }
        else
        {
            /*
             * No access handlers, much simpler.
             */
            int rc = PGMPhysSimpleWriteGCPhys(pVM, pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, pbBuf, cbFirst);
            if (RT_SUCCESS(rc))
            {
                if (cbSecond)
                {
                    rc = PGMPhysSimpleWriteGCPhys(pVM, pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, pbBuf + cbFirst, cbSecond);
                    if (RT_SUCCESS(rc))
                    { /* likely */ }
                    else
                    {
                        Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysSimpleWriteGCPhys GCPhysFirst=%RGp/%#x GCPhysSecond=%RGp/%#x %Rrc (!!)\n",
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst,
                             pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond, rc));
                        return rc;
                    }
                }
            }
            else
            {
                Log(("iemMemBounceBufferCommitAndUnmap: PGMPhysSimpleWriteGCPhys GCPhysFirst=%RGp/%#x %Rrc [GCPhysSecond=%RGp/%#x] (!!)\n",
                     pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst, cbFirst, rc,
                     pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond, cbSecond));
                return rc;
            }
        }
    }
#endif

#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)
    /*
     * Record the write(s).
     */
    if (!pIemCpu->fNoRem)
    {
        PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
        if (pEvtRec)
        {
            pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_WRITE;
            pEvtRec->u.RamWrite.GCPhys  = pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst;
            pEvtRec->u.RamWrite.cb      = pIemCpu->aMemBbMappings[iMemMap].cbFirst;
            memcpy(pEvtRec->u.RamWrite.ab, &pIemCpu->aBounceBuffers[iMemMap].ab[0], pIemCpu->aMemBbMappings[iMemMap].cbFirst);
            AssertCompile(sizeof(pEvtRec->u.RamWrite.ab) == sizeof(pIemCpu->aBounceBuffers[0].ab));
            pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
            *pIemCpu->ppIemEvtRecNext = pEvtRec;
        }
        if (pIemCpu->aMemBbMappings[iMemMap].cbSecond)
        {
            pEvtRec = iemVerifyAllocRecord(pIemCpu);
            if (pEvtRec)
            {
                pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_WRITE;
                pEvtRec->u.RamWrite.GCPhys  = pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond;
                pEvtRec->u.RamWrite.cb      = pIemCpu->aMemBbMappings[iMemMap].cbSecond;
                memcpy(pEvtRec->u.RamWrite.ab,
                       &pIemCpu->aBounceBuffers[iMemMap].ab[pIemCpu->aMemBbMappings[iMemMap].cbFirst],
                       pIemCpu->aMemBbMappings[iMemMap].cbSecond);
                pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
                *pIemCpu->ppIemEvtRecNext = pEvtRec;
            }
        }
    }
#endif
#if defined(IEM_VERIFICATION_MODE_MINIMAL) || defined(IEM_LOG_MEMORY_WRITES)
    Log(("IEM Wrote %RGp: %.*Rhxs\n", pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst,
         RT_MAX(RT_MIN(pIemCpu->aMemBbMappings[iMemMap].cbFirst, 64), 1), &pIemCpu->aBounceBuffers[iMemMap].ab[0]));
    if (pIemCpu->aMemBbMappings[iMemMap].cbSecond)
        Log(("IEM Wrote %RGp: %.*Rhxs [2nd page]\n", pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond,
             RT_MIN(pIemCpu->aMemBbMappings[iMemMap].cbSecond, 64),
             &pIemCpu->aBounceBuffers[iMemMap].ab[pIemCpu->aMemBbMappings[iMemMap].cbFirst]));

    size_t cbWrote = pIemCpu->aMemBbMappings[iMemMap].cbFirst + pIemCpu->aMemBbMappings[iMemMap].cbSecond;
    g_cbIemWrote = cbWrote;
    memcpy(g_abIemWrote, &pIemCpu->aBounceBuffers[iMemMap].ab[0], RT_MIN(cbWrote, sizeof(g_abIemWrote)));
#endif

    /*
     * Free the mapping entry.
     */
    pIemCpu->aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    Assert(pIemCpu->cActiveMappings != 0);
    pIemCpu->cActiveMappings--;
    return VINF_SUCCESS;
}


/**
 * iemMemMap worker that deals with a request crossing pages.
 */
IEM_STATIC VBOXSTRICTRC
iemMemBounceBufferMapCrossPage(PIEMCPU pIemCpu, int iMemMap, void **ppvMem, size_t cbMem, RTGCPTR GCPtrFirst, uint32_t fAccess)
{
    /*
     * Do the address translations.
     */
    RTGCPHYS GCPhysFirst;
    VBOXSTRICTRC rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, GCPtrFirst, fAccess, &GCPhysFirst);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    RTGCPHYS GCPhysSecond;
    rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, (GCPtrFirst + (cbMem - 1)) & ~(RTGCPTR)PAGE_OFFSET_MASK,
                                                 fAccess, &GCPhysSecond);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    GCPhysSecond &= ~(RTGCPHYS)PAGE_OFFSET_MASK;

    PVM pVM = IEMCPU_TO_VM(pIemCpu);
#ifdef IEM_VERIFICATION_MODE_FULL
    /*
     * Detect problematic memory when verifying so we can select
     * the right execution engine. (TLB: Redo this.)
     */
    if (IEM_FULL_VERIFICATION_ENABLED(pIemCpu))
    {
        int rc2 = PGMPhysIemQueryAccess(pVM, GCPhysFirst,  RT_BOOL(fAccess & IEM_ACCESS_TYPE_WRITE), pIemCpu->fBypassHandlers);
        if (RT_SUCCESS(rc2))
            rc2 = PGMPhysIemQueryAccess(pVM, GCPhysSecond, RT_BOOL(fAccess & IEM_ACCESS_TYPE_WRITE), pIemCpu->fBypassHandlers);
        if (RT_FAILURE(rc2))
            pIemCpu->fProblematicMemory = true;
    }
#endif


    /*
     * Read in the current memory content if it's a read, execute or partial
     * write access.
     */
    uint8_t        *pbBuf        = &pIemCpu->aBounceBuffers[iMemMap].ab[0];
    uint32_t const  cbFirstPage  = PAGE_SIZE - (GCPhysFirst & PAGE_OFFSET_MASK);
    uint32_t const  cbSecondPage = (uint32_t)(cbMem - cbFirstPage);

    if (fAccess & (IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_EXEC | IEM_ACCESS_PARTIAL_WRITE))
    {
        if (!pIemCpu->fBypassHandlers)
        {
            /*
             * Must carefully deal with access handler status codes here,
             * makes the code a bit bloated.
             */
            rcStrict = PGMPhysRead(pVM, GCPhysFirst, pbBuf, cbFirstPage, PGMACCESSORIGIN_IEM);
            if (rcStrict == VINF_SUCCESS)
            {
                rcStrict = PGMPhysRead(pVM, GCPhysSecond, pbBuf + cbFirstPage, cbSecondPage, PGMACCESSORIGIN_IEM);
                if (rcStrict == VINF_SUCCESS)
                { /*likely */ }
                else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
                    rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                else
                {
                    Log(("iemMemBounceBufferMapPhys: PGMPhysRead GCPhysSecond=%RGp rcStrict2=%Rrc (!!)\n",
                         GCPhysSecond, VBOXSTRICTRC_VAL(rcStrict) ));
                    return rcStrict;
                }
            }
            else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
            {
                VBOXSTRICTRC rcStrict2 = PGMPhysRead(pVM, GCPhysSecond, pbBuf + cbFirstPage, cbSecondPage, PGMACCESSORIGIN_IEM);
                if (PGM_PHYS_RW_IS_SUCCESS(rcStrict2))
                {
                    PGM_PHYS_RW_DO_UPDATE_STRICT_RC(rcStrict, rcStrict2);
                    rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                }
                else
                {
                    Log(("iemMemBounceBufferMapPhys: PGMPhysRead GCPhysSecond=%RGp rcStrict2=%Rrc (rcStrict=%Rrc) (!!)\n",
                         GCPhysSecond, VBOXSTRICTRC_VAL(rcStrict2), VBOXSTRICTRC_VAL(rcStrict2) ));
                    return rcStrict2;
                }
            }
            else
            {
                Log(("iemMemBounceBufferMapPhys: PGMPhysRead GCPhysFirst=%RGp rcStrict=%Rrc (!!)\n",
                     GCPhysFirst, VBOXSTRICTRC_VAL(rcStrict) ));
                return rcStrict;
            }
        }
        else
        {
            /*
             * No informational status codes here, much more straight forward.
             */
            int rc = PGMPhysSimpleReadGCPhys(pVM, pbBuf, GCPhysFirst, cbFirstPage);
            if (RT_SUCCESS(rc))
            {
                Assert(rc == VINF_SUCCESS);
                rc = PGMPhysSimpleReadGCPhys(pVM, pbBuf + cbFirstPage, GCPhysSecond, cbSecondPage);
                if (RT_SUCCESS(rc))
                    Assert(rc == VINF_SUCCESS);
                else
                {
                    Log(("iemMemBounceBufferMapPhys: PGMPhysSimpleReadGCPhys GCPhysSecond=%RGp rc=%Rrc (!!)\n", GCPhysSecond, rc));
                    return rc;
                }
            }
            else
            {
                Log(("iemMemBounceBufferMapPhys: PGMPhysSimpleReadGCPhys GCPhysFirst=%RGp rc=%Rrc (!!)\n", GCPhysFirst, rc));
                return rc;
            }
        }

#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)
        if (   !pIemCpu->fNoRem
            && (fAccess & (IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_EXEC)) )
        {
            /*
             * Record the reads.
             */
            PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
            if (pEvtRec)
            {
                pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_READ;
                pEvtRec->u.RamRead.GCPhys  = GCPhysFirst;
                pEvtRec->u.RamRead.cb      = cbFirstPage;
                pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
                *pIemCpu->ppIemEvtRecNext = pEvtRec;
            }
            pEvtRec = iemVerifyAllocRecord(pIemCpu);
            if (pEvtRec)
            {
                pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_READ;
                pEvtRec->u.RamRead.GCPhys  = GCPhysSecond;
                pEvtRec->u.RamRead.cb      = cbSecondPage;
                pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
                *pIemCpu->ppIemEvtRecNext = pEvtRec;
            }
        }
#endif
    }
#ifdef VBOX_STRICT
    else
        memset(pbBuf, 0xcc, cbMem);
    if (cbMem < sizeof(pIemCpu->aBounceBuffers[iMemMap].ab))
        memset(pbBuf + cbMem, 0xaa, sizeof(pIemCpu->aBounceBuffers[iMemMap].ab) - cbMem);
#endif

    /*
     * Commit the bounce buffer entry.
     */
    pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst    = GCPhysFirst;
    pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond   = GCPhysSecond;
    pIemCpu->aMemBbMappings[iMemMap].cbFirst        = (uint16_t)cbFirstPage;
    pIemCpu->aMemBbMappings[iMemMap].cbSecond       = (uint16_t)cbSecondPage;
    pIemCpu->aMemBbMappings[iMemMap].fUnassigned    = false;
    pIemCpu->aMemMappings[iMemMap].pv               = pbBuf;
    pIemCpu->aMemMappings[iMemMap].fAccess          = fAccess | IEM_ACCESS_BOUNCE_BUFFERED;
    pIemCpu->iNextMapping = iMemMap + 1;
    pIemCpu->cActiveMappings++;

    iemMemUpdateWrittenCounter(pIemCpu, fAccess, cbMem);
    *ppvMem = pbBuf;
    return VINF_SUCCESS;
}


/**
 * iemMemMap woker that deals with iemMemPageMap failures.
 */
IEM_STATIC VBOXSTRICTRC iemMemBounceBufferMapPhys(PIEMCPU pIemCpu, unsigned iMemMap, void **ppvMem, size_t cbMem,
                                                  RTGCPHYS GCPhysFirst, uint32_t fAccess, VBOXSTRICTRC rcMap)
{
    /*
     * Filter out conditions we can handle and the ones which shouldn't happen.
     */
    if (   rcMap != VERR_PGM_PHYS_TLB_CATCH_WRITE
        && rcMap != VERR_PGM_PHYS_TLB_CATCH_ALL
        && rcMap != VERR_PGM_PHYS_TLB_UNASSIGNED)
    {
        AssertReturn(RT_FAILURE_NP(rcMap), VERR_IEM_IPE_8);
        return rcMap;
    }
    pIemCpu->cPotentialExits++;

    /*
     * Read in the current memory content if it's a read, execute or partial
     * write access.
     */
    uint8_t *pbBuf = &pIemCpu->aBounceBuffers[iMemMap].ab[0];
    if (fAccess & (IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_EXEC | IEM_ACCESS_PARTIAL_WRITE))
    {
        if (rcMap == VERR_PGM_PHYS_TLB_UNASSIGNED)
            memset(pbBuf, 0xff, cbMem);
        else
        {
            int rc;
            if (!pIemCpu->fBypassHandlers)
            {
                VBOXSTRICTRC rcStrict = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhysFirst, pbBuf, cbMem, PGMACCESSORIGIN_IEM);
                if (rcStrict == VINF_SUCCESS)
                { /* nothing */ }
                else if (PGM_PHYS_RW_IS_SUCCESS(rcStrict))
                    rcStrict = iemSetPassUpStatus(pIemCpu, rcStrict);
                else
                {
                    Log(("iemMemBounceBufferMapPhys: PGMPhysRead GCPhysFirst=%RGp rcStrict=%Rrc (!!)\n",
                         GCPhysFirst, VBOXSTRICTRC_VAL(rcStrict) ));
                    return rcStrict;
                }
            }
            else
            {
                rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), pbBuf, GCPhysFirst, cbMem);
                if (RT_SUCCESS(rc))
                { /* likely */ }
                else
                {
                    Log(("iemMemBounceBufferMapPhys: PGMPhysSimpleReadGCPhys GCPhysFirst=%RGp rcStrict=%Rrc (!!)\n",
                         GCPhysFirst, rc));
                    return rc;
                }
            }
        }

#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)
        if (   !pIemCpu->fNoRem
            && (fAccess & (IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_EXEC)) )
        {
            /*
             * Record the read.
             */
            PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
            if (pEvtRec)
            {
                pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_READ;
                pEvtRec->u.RamRead.GCPhys  = GCPhysFirst;
                pEvtRec->u.RamRead.cb      = (uint32_t)cbMem;
                pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
                *pIemCpu->ppIemEvtRecNext = pEvtRec;
            }
        }
#endif
    }
#ifdef VBOX_STRICT
    else
        memset(pbBuf, 0xcc, cbMem);
#endif
#ifdef VBOX_STRICT
    if (cbMem < sizeof(pIemCpu->aBounceBuffers[iMemMap].ab))
        memset(pbBuf + cbMem, 0xaa, sizeof(pIemCpu->aBounceBuffers[iMemMap].ab) - cbMem);
#endif

    /*
     * Commit the bounce buffer entry.
     */
    pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst    = GCPhysFirst;
    pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond   = NIL_RTGCPHYS;
    pIemCpu->aMemBbMappings[iMemMap].cbFirst        = (uint16_t)cbMem;
    pIemCpu->aMemBbMappings[iMemMap].cbSecond       = 0;
    pIemCpu->aMemBbMappings[iMemMap].fUnassigned    = rcMap == VERR_PGM_PHYS_TLB_UNASSIGNED;
    pIemCpu->aMemMappings[iMemMap].pv               = pbBuf;
    pIemCpu->aMemMappings[iMemMap].fAccess          = fAccess | IEM_ACCESS_BOUNCE_BUFFERED;
    pIemCpu->iNextMapping = iMemMap + 1;
    pIemCpu->cActiveMappings++;

    iemMemUpdateWrittenCounter(pIemCpu, fAccess, cbMem);
    *ppvMem = pbBuf;
    return VINF_SUCCESS;
}



/**
 * Maps the specified guest memory for the given kind of access.
 *
 * This may be using bounce buffering of the memory if it's crossing a page
 * boundary or if there is an access handler installed for any of it.  Because
 * of lock prefix guarantees, we're in for some extra clutter when this
 * happens.
 *
 * This may raise a \#GP, \#SS, \#PF or \#AC.
 *
 * @returns VBox strict status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   ppvMem              Where to return the pointer to the mapped
 *                              memory.
 * @param   cbMem               The number of bytes to map.  This is usually 1,
 *                              2, 4, 6, 8, 12, 16, 32 or 512.  When used by
 *                              string operations it can be up to a page.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 *                              Use UINT8_MAX to indicate that no segmentation
 *                              is required (for IDT, GDT and LDT accesses).
 * @param   GCPtrMem            The address of the guest memory.
 * @param   fAccess             How the memory is being accessed.  The
 *                              IEM_ACCESS_TYPE_XXX bit is used to figure out
 *                              how to map the memory, while the
 *                              IEM_ACCESS_WHAT_XXX bit is used when raising
 *                              exceptions.
 */
IEM_STATIC VBOXSTRICTRC
iemMemMap(PIEMCPU pIemCpu, void **ppvMem, size_t cbMem, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t fAccess)
{
    /*
     * Check the input and figure out which mapping entry to use.
     */
    Assert(cbMem <= 64 || cbMem == 512 || cbMem == 108 || cbMem == 104 || cbMem == 94); /* 512 is the max! */
    Assert(~(fAccess & ~(IEM_ACCESS_TYPE_MASK | IEM_ACCESS_WHAT_MASK)));
    Assert(pIemCpu->cActiveMappings < RT_ELEMENTS(pIemCpu->aMemMappings));

    unsigned iMemMap = pIemCpu->iNextMapping;
    if (   iMemMap >= RT_ELEMENTS(pIemCpu->aMemMappings)
        || pIemCpu->aMemMappings[iMemMap].fAccess != IEM_ACCESS_INVALID)
    {
        iMemMap = iemMemMapFindFree(pIemCpu);
        AssertLogRelMsgReturn(iMemMap < RT_ELEMENTS(pIemCpu->aMemMappings),
                              ("active=%d fAccess[0] = {%#x, %#x, %#x}\n", pIemCpu->cActiveMappings,
                               pIemCpu->aMemMappings[0].fAccess, pIemCpu->aMemMappings[1].fAccess,
                               pIemCpu->aMemMappings[2].fAccess),
                              VERR_IEM_IPE_9);
    }

    /*
     * Map the memory, checking that we can actually access it.  If something
     * slightly complicated happens, fall back on bounce buffering.
     */
    VBOXSTRICTRC rcStrict = iemMemApplySegment(pIemCpu, fAccess, iSegReg, cbMem, &GCPtrMem);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    if ((GCPtrMem & PAGE_OFFSET_MASK) + cbMem > PAGE_SIZE) /* Crossing a page boundary? */
        return iemMemBounceBufferMapCrossPage(pIemCpu, iMemMap, ppvMem, cbMem, GCPtrMem, fAccess);

    RTGCPHYS GCPhysFirst;
    rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, GCPtrMem, fAccess, &GCPhysFirst);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    if (fAccess & IEM_ACCESS_TYPE_WRITE)
        Log8(("IEM WR %RGv (%RGp) LB %#zx\n", GCPtrMem, GCPhysFirst, cbMem));
    if (fAccess & IEM_ACCESS_TYPE_READ)
        Log9(("IEM RD %RGv (%RGp) LB %#zx\n", GCPtrMem, GCPhysFirst, cbMem));

    void *pvMem;
    rcStrict = iemMemPageMap(pIemCpu, GCPhysFirst, fAccess, &pvMem, &pIemCpu->aMemMappingLocks[iMemMap].Lock);
    if (rcStrict != VINF_SUCCESS)
        return iemMemBounceBufferMapPhys(pIemCpu, iMemMap, ppvMem, cbMem, GCPhysFirst, fAccess, rcStrict);

    /*
     * Fill in the mapping table entry.
     */
    pIemCpu->aMemMappings[iMemMap].pv      = pvMem;
    pIemCpu->aMemMappings[iMemMap].fAccess = fAccess;
    pIemCpu->iNextMapping = iMemMap + 1;
    pIemCpu->cActiveMappings++;

    iemMemUpdateWrittenCounter(pIemCpu, fAccess, cbMem);
    *ppvMem = pvMem;
    return VINF_SUCCESS;
}


/**
 * Commits the guest memory if bounce buffered and unmaps it.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pvMem               The mapping.
 * @param   fAccess             The kind of access.
 */
IEM_STATIC VBOXSTRICTRC iemMemCommitAndUnmap(PIEMCPU pIemCpu, void *pvMem, uint32_t fAccess)
{
    int iMemMap = iemMapLookup(pIemCpu, pvMem, fAccess);
    AssertReturn(iMemMap >= 0, iMemMap);

    /* If it's bounce buffered, we may need to write back the buffer. */
    if (pIemCpu->aMemMappings[iMemMap].fAccess & IEM_ACCESS_BOUNCE_BUFFERED)
    {
        if (pIemCpu->aMemMappings[iMemMap].fAccess & IEM_ACCESS_TYPE_WRITE)
            return iemMemBounceBufferCommitAndUnmap(pIemCpu, iMemMap);
    }
    /* Otherwise unlock it. */
    else
        PGMPhysReleasePageMappingLock(IEMCPU_TO_VM(pIemCpu), &pIemCpu->aMemMappingLocks[iMemMap].Lock);

    /* Free the entry. */
    pIemCpu->aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    Assert(pIemCpu->cActiveMappings != 0);
    pIemCpu->cActiveMappings--;
    return VINF_SUCCESS;
}


/**
 * Rollbacks mappings, releasing page locks and such.
 *
 * The caller shall only call this after checking cActiveMappings.
 *
 * @returns Strict VBox status code to pass up.
 * @param   pIemCpu     The IEM per CPU data.
 */
IEM_STATIC void iemMemRollback(PIEMCPU pIemCpu)
{
    Assert(pIemCpu->cActiveMappings > 0);

    uint32_t iMemMap = RT_ELEMENTS(pIemCpu->aMemMappings);
    while (iMemMap-- > 0)
    {
        uint32_t fAccess = pIemCpu->aMemMappings[iMemMap].fAccess;
        if (fAccess != IEM_ACCESS_INVALID)
        {
            AssertMsg(!(fAccess & ~IEM_ACCESS_VALID_MASK) && fAccess != 0, ("%#x\n", fAccess));
            pIemCpu->aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
            if (!(fAccess & IEM_ACCESS_BOUNCE_BUFFERED))
                PGMPhysReleasePageMappingLock(IEMCPU_TO_VM(pIemCpu), &pIemCpu->aMemMappingLocks[iMemMap].Lock);
            Assert(pIemCpu->cActiveMappings > 0);
            pIemCpu->cActiveMappings--;
        }
    }
}


/**
 * Fetches a data byte.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu8Dst              Where to return the byte.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU8(PIEMCPU pIemCpu, uint8_t *pu8Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint8_t const *pu8Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu8Src, sizeof(*pu8Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu8Dst = *pu8Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu8Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


/**
 * Fetches a data word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu16Dst             Where to return the word.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU16(PIEMCPU pIemCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint16_t const *pu16Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Src, sizeof(*pu16Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu16Dst = *pu16Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu16Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


/**
 * Fetches a data dword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu32Dst             Where to return the dword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU32(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint32_t const *pu32Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Src, sizeof(*pu32Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu32Dst = *pu32Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu32Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


#ifdef SOME_UNUSED_FUNCTION
/**
 * Fetches a data dword and sign extends it to a qword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu64Dst             Where to return the sign extended value.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataS32SxU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    int32_t const *pi32Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pi32Src, sizeof(*pi32Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = *pi32Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pi32Src, IEM_ACCESS_DATA_R);
    }
#ifdef __GNUC__ /* warning: GCC may be a royal pain */
    else
        *pu64Dst = 0;
#endif
    return rc;
}
#endif


/**
 * Fetches a data qword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu64Dst             Where to return the qword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint64_t const *pu64Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Src, sizeof(*pu64Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = *pu64Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu64Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


/**
 * Fetches a data qword, aligned at a 16 byte boundrary (for SSE).
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu64Dst             Where to return the qword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU64AlignedU128(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    /** @todo testcase: Ordering of \#SS(0) vs \#GP() vs \#PF on SSE stuff. */
    if (RT_UNLIKELY(GCPtrMem & 15))
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    uint64_t const *pu64Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Src, sizeof(*pu64Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = *pu64Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu64Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


/**
 * Fetches a data tword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pr80Dst             Where to return the tword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataR80(PIEMCPU pIemCpu, PRTFLOAT80U pr80Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    PCRTFLOAT80U pr80Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pr80Src, sizeof(*pr80Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pr80Dst = *pr80Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pr80Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


/**
 * Fetches a data dqword (double qword), generally SSE related.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu128Dst            Where to return the qword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU128(PIEMCPU pIemCpu, uint128_t *pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint128_t const *pu128Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu128Src, sizeof(*pu128Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu128Dst = *pu128Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu128Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}


/**
 * Fetches a data dqword (double qword) at an aligned address, generally SSE
 * related.
 *
 * Raises \#GP(0) if not aligned.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu128Dst            Where to return the qword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataU128AlignedSse(PIEMCPU pIemCpu, uint128_t *pu128Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    /** @todo testcase: Ordering of \#SS(0) vs \#GP() vs \#PF on SSE stuff. */
    if (   (GCPtrMem & 15)
        && !(pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.MXCSR & X86_MXSCR_MM)) /** @todo should probably check this *after* applying seg.u64Base... Check real HW. */
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    uint128_t const *pu128Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu128Src, sizeof(*pu128Src), iSegReg, GCPtrMem, IEM_ACCESS_DATA_R);
    if (rc == VINF_SUCCESS)
    {
        *pu128Dst = *pu128Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu128Src, IEM_ACCESS_DATA_R);
    }
    return rc;
}




/**
 * Fetches a descriptor register (lgdt, lidt).
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pcbLimit            Where to return the limit.
 * @param   pGCPtrBase          Where to return the base.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   enmOpSize           The effective operand size.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchDataXdtr(PIEMCPU pIemCpu, uint16_t *pcbLimit, PRTGCPTR pGCPtrBase, uint8_t iSegReg,
                                            RTGCPTR GCPtrMem, IEMMODE enmOpSize)
{
    /*
     * Just like SIDT and SGDT, the LIDT and LGDT instructions are a
     * little special:
     *      - The two reads are done separately.
     *      - Operand size override works in 16-bit and 32-bit code, but 64-bit.
     *      - We suspect the 386 to actually commit the limit before the base in
     *        some cases (search for 386 in  bs3CpuBasic2_lidt_lgdt_One).  We
     *        don't try emulate this eccentric behavior, because it's not well
     *        enough understood and rather hard to trigger.
     *      - The 486 seems to do a dword limit read when the operand size is 32-bit.
     */
    VBOXSTRICTRC rcStrict;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        rcStrict = iemMemFetchDataU16(pIemCpu, pcbLimit, iSegReg, GCPtrMem);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemFetchDataU64(pIemCpu, pGCPtrBase, iSegReg, GCPtrMem + 2);
    }
    else
    {
        uint32_t uTmp;
        if (enmOpSize == IEMMODE_32BIT)
        {
            if (IEM_GET_TARGET_CPU(pIemCpu) != IEMTARGETCPU_486)
            {
                rcStrict = iemMemFetchDataU16(pIemCpu, pcbLimit, iSegReg, GCPtrMem);
                if (rcStrict == VINF_SUCCESS)
                    rcStrict = iemMemFetchDataU32(pIemCpu, &uTmp, iSegReg, GCPtrMem + 2);
            }
            else
            {
                rcStrict = iemMemFetchDataU32(pIemCpu, &uTmp, iSegReg, GCPtrMem);
                if (rcStrict == VINF_SUCCESS)
                {
                    *pcbLimit = (uint16_t)uTmp;
                    rcStrict = iemMemFetchDataU32(pIemCpu, &uTmp, iSegReg, GCPtrMem + 2);
                }
            }
            if (rcStrict == VINF_SUCCESS)
                *pGCPtrBase = uTmp;
        }
        else
        {
            rcStrict = iemMemFetchDataU16(pIemCpu, pcbLimit, iSegReg, GCPtrMem);
            if (rcStrict == VINF_SUCCESS)
            {
                rcStrict = iemMemFetchDataU32(pIemCpu, &uTmp, iSegReg, GCPtrMem + 2);
                if (rcStrict == VINF_SUCCESS)
                    *pGCPtrBase = uTmp & UINT32_C(0x00ffffff);
            }
        }
    }
    return rcStrict;
}



/**
 * Stores a data byte.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u8Value             The value to store.
 */
IEM_STATIC VBOXSTRICTRC iemMemStoreDataU8(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value)
{
    /* The lazy approach for now... */
    uint8_t *pu8Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu8Dst, sizeof(*pu8Dst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W);
    if (rc == VINF_SUCCESS)
    {
        *pu8Dst = u8Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu8Dst, IEM_ACCESS_DATA_W);
    }
    return rc;
}


/**
 * Stores a data word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u16Value            The value to store.
 */
IEM_STATIC VBOXSTRICTRC iemMemStoreDataU16(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value)
{
    /* The lazy approach for now... */
    uint16_t *pu16Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Dst, sizeof(*pu16Dst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W);
    if (rc == VINF_SUCCESS)
    {
        *pu16Dst = u16Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu16Dst, IEM_ACCESS_DATA_W);
    }
    return rc;
}


/**
 * Stores a data dword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u32Value            The value to store.
 */
IEM_STATIC VBOXSTRICTRC iemMemStoreDataU32(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value)
{
    /* The lazy approach for now... */
    uint32_t *pu32Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Dst, sizeof(*pu32Dst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W);
    if (rc == VINF_SUCCESS)
    {
        *pu32Dst = u32Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu32Dst, IEM_ACCESS_DATA_W);
    }
    return rc;
}


/**
 * Stores a data qword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u64Value            The value to store.
 */
IEM_STATIC VBOXSTRICTRC iemMemStoreDataU64(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value)
{
    /* The lazy approach for now... */
    uint64_t *pu64Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Dst, sizeof(*pu64Dst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = u64Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu64Dst, IEM_ACCESS_DATA_W);
    }
    return rc;
}


/**
 * Stores a data dqword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u128Value            The value to store.
 */
IEM_STATIC VBOXSTRICTRC iemMemStoreDataU128(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint128_t u128Value)
{
    /* The lazy approach for now... */
    uint128_t *pu128Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu128Dst, sizeof(*pu128Dst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W);
    if (rc == VINF_SUCCESS)
    {
        *pu128Dst = u128Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu128Dst, IEM_ACCESS_DATA_W);
    }
    return rc;
}


/**
 * Stores a data dqword, SSE aligned.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   u128Value           The value to store.
 */
IEM_STATIC VBOXSTRICTRC iemMemStoreDataU128AlignedSse(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint128_t u128Value)
{
    /* The lazy approach for now... */
    if (   (GCPtrMem & 15)
        && !(pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.MXCSR & X86_MXSCR_MM)) /** @todo should probably check this *after* applying seg.u64Base... Check real HW. */
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    uint128_t *pu128Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu128Dst, sizeof(*pu128Dst), iSegReg, GCPtrMem, IEM_ACCESS_DATA_W);
    if (rc == VINF_SUCCESS)
    {
        *pu128Dst = u128Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu128Dst, IEM_ACCESS_DATA_W);
    }
    return rc;
}


/**
 * Stores a descriptor register (sgdt, sidt).
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   cbLimit             The limit.
 * @param   GCPtrBase           The base address.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC
iemMemStoreDataXdtr(PIEMCPU pIemCpu, uint16_t cbLimit, RTGCPTR GCPtrBase, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /*
     * The SIDT and SGDT instructions actually stores the data using two
     * independent writes.  The instructions does not respond to opsize prefixes.
     */
    VBOXSTRICTRC rcStrict = iemMemStoreDataU16(pIemCpu, iSegReg, GCPtrMem, cbLimit);
    if (rcStrict == VINF_SUCCESS)
    {
        if (pIemCpu->enmCpuMode == IEMMODE_16BIT)
            rcStrict = iemMemStoreDataU32(pIemCpu, iSegReg, GCPtrMem + 2,
                                          IEM_GET_TARGET_CPU(pIemCpu) <= IEMTARGETCPU_286
                                          ? (uint32_t)GCPtrBase | UINT32_C(0xff000000) : (uint32_t)GCPtrBase);
        else if (pIemCpu->enmCpuMode == IEMMODE_32BIT)
            rcStrict = iemMemStoreDataU32(pIemCpu, iSegReg, GCPtrMem + 2, (uint32_t)GCPtrBase);
        else
            rcStrict = iemMemStoreDataU64(pIemCpu, iSegReg, GCPtrMem + 2, GCPtrBase);
    }
    return rcStrict;
}


/**
 * Pushes a word onto the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16Value            The value to push.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU16(PIEMCPU pIemCpu, uint16_t u16Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pIemCpu, pCtx, 2, &uNewRsp);

    /* Write the word the lazy way. */
    uint16_t *pu16Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Dst, sizeof(*pu16Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
    if (rc == VINF_SUCCESS)
    {
        *pu16Dst = u16Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu16Dst, IEM_ACCESS_STACK_W);
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        pCtx->rsp = uNewRsp;

    return rc;
}


/**
 * Pushes a dword onto the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u32Value            The value to push.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU32(PIEMCPU pIemCpu, uint32_t u32Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pIemCpu, pCtx, 4, &uNewRsp);

    /* Write the dword the lazy way. */
    uint32_t *pu32Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Dst, sizeof(*pu32Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
    if (rc == VINF_SUCCESS)
    {
        *pu32Dst = u32Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu32Dst, IEM_ACCESS_STACK_W);
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        pCtx->rsp = uNewRsp;

    return rc;
}


/**
 * Pushes a dword segment register value onto the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u32Value            The value to push.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU32SReg(PIEMCPU pIemCpu, uint32_t u32Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pIemCpu, pCtx, 4, &uNewRsp);

    VBOXSTRICTRC rc;
    if (IEM_FULL_VERIFICATION_REM_ENABLED(pIemCpu))
    {
        /* The recompiler writes a full dword. */
        uint32_t *pu32Dst;
        rc = iemMemMap(pIemCpu, (void **)&pu32Dst, sizeof(*pu32Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
        if (rc == VINF_SUCCESS)
        {
            *pu32Dst = u32Value;
            rc = iemMemCommitAndUnmap(pIemCpu, pu32Dst, IEM_ACCESS_STACK_W);
        }
    }
    else
    {
        /* The intel docs talks about zero extending the selector register
           value.  My actual intel CPU here might be zero extending the value
           but it still only writes the lower word... */
        /** @todo Test this on new HW and on AMD and in 64-bit mode.  Also test what
         * happens when crossing an electric page boundrary, is the high word checked
         * for write accessibility or not? Probably it is.  What about segment limits?
         * It appears this behavior is also shared with trap error codes.
         *
         * Docs indicate the behavior changed maybe in Pentium or Pentium Pro. Check
         * ancient hardware when it actually did change. */
        uint16_t *pu16Dst;
        rc = iemMemMap(pIemCpu, (void **)&pu16Dst, sizeof(uint32_t), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_RW);
        if (rc == VINF_SUCCESS)
        {
            *pu16Dst = (uint16_t)u32Value;
            rc = iemMemCommitAndUnmap(pIemCpu, pu16Dst, IEM_ACCESS_STACK_RW);
        }
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        pCtx->rsp = uNewRsp;

    return rc;
}


/**
 * Pushes a qword onto the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u64Value            The value to push.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU64(PIEMCPU pIemCpu, uint64_t u64Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pIemCpu, pCtx, 8, &uNewRsp);

    /* Write the word the lazy way. */
    uint64_t *pu64Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Dst, sizeof(*pu64Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = u64Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu64Dst, IEM_ACCESS_STACK_W);
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        pCtx->rsp = uNewRsp;

    return rc;
}


/**
 * Pops a word from the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu16Value           Where to store the popped value.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopU16(PIEMCPU pIemCpu, uint16_t *pu16Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pIemCpu, pCtx, 2, &uNewRsp);

    /* Write the word the lazy way. */
    uint16_t const *pu16Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Src, sizeof(*pu16Src), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
    if (rc == VINF_SUCCESS)
    {
        *pu16Value = *pu16Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu16Src, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
            pCtx->rsp = uNewRsp;
    }

    return rc;
}


/**
 * Pops a dword from the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu32Value           Where to store the popped value.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopU32(PIEMCPU pIemCpu, uint32_t *pu32Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pIemCpu, pCtx, 4, &uNewRsp);

    /* Write the word the lazy way. */
    uint32_t const *pu32Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Src, sizeof(*pu32Src), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
    if (rc == VINF_SUCCESS)
    {
        *pu32Value = *pu32Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu32Src, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
            pCtx->rsp = uNewRsp;
    }

    return rc;
}


/**
 * Pops a qword from the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu64Value           Where to store the popped value.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopU64(PIEMCPU pIemCpu, uint64_t *pu64Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pIemCpu, pCtx, 8, &uNewRsp);

    /* Write the word the lazy way. */
    uint64_t const *pu64Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Src, sizeof(*pu64Src), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
    if (rc == VINF_SUCCESS)
    {
        *pu64Value = *pu64Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu64Src, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
            pCtx->rsp = uNewRsp;
    }

    return rc;
}


/**
 * Pushes a word onto the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16Value            The value to push.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU16Ex(PIEMCPU pIemCpu, uint16_t u16Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(pIemCpu, pCtx, &NewRsp, 2);

    /* Write the word the lazy way. */
    uint16_t *pu16Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Dst, sizeof(*pu16Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
    if (rc == VINF_SUCCESS)
    {
        *pu16Dst = u16Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu16Dst, IEM_ACCESS_STACK_W);
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        *pTmpRsp = NewRsp;

    return rc;
}


/**
 * Pushes a dword onto the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u32Value            The value to push.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU32Ex(PIEMCPU pIemCpu, uint32_t u32Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(pIemCpu, pCtx, &NewRsp, 4);

    /* Write the word the lazy way. */
    uint32_t *pu32Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Dst, sizeof(*pu32Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
    if (rc == VINF_SUCCESS)
    {
        *pu32Dst = u32Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu32Dst, IEM_ACCESS_STACK_W);
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        *pTmpRsp = NewRsp;

    return rc;
}


/**
 * Pushes a dword onto the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u64Value            The value to push.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushU64Ex(PIEMCPU pIemCpu, uint64_t u64Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(pIemCpu, pCtx, &NewRsp, 8);

    /* Write the word the lazy way. */
    uint64_t *pu64Dst;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Dst, sizeof(*pu64Dst), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = u64Value;
        rc = iemMemCommitAndUnmap(pIemCpu, pu64Dst, IEM_ACCESS_STACK_W);
    }

    /* Commit the new RSP value unless we an access handler made trouble. */
    if (rc == VINF_SUCCESS)
        *pTmpRsp = NewRsp;

    return rc;
}


/**
 * Pops a word from the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu16Value           Where to store the popped value.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopU16Ex(PIEMCPU pIemCpu, uint16_t *pu16Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(pIemCpu, pCtx, &NewRsp, 2);

    /* Write the word the lazy way. */
    uint16_t const *pu16Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Src, sizeof(*pu16Src), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
    if (rc == VINF_SUCCESS)
    {
        *pu16Value = *pu16Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu16Src, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
            *pTmpRsp = NewRsp;
    }

    return rc;
}


/**
 * Pops a dword from the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu32Value           Where to store the popped value.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopU32Ex(PIEMCPU pIemCpu, uint32_t *pu32Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(pIemCpu, pCtx, &NewRsp, 4);

    /* Write the word the lazy way. */
    uint32_t const *pu32Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Src, sizeof(*pu32Src), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
    if (rc == VINF_SUCCESS)
    {
        *pu32Value = *pu32Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu32Src, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rc == VINF_SUCCESS)
            *pTmpRsp = NewRsp;
    }

    return rc;
}


/**
 * Pops a qword from the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu64Value           Where to store the popped value.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopU64Ex(PIEMCPU pIemCpu, uint64_t *pu64Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(pIemCpu, pCtx, &NewRsp, 8);

    /* Write the word the lazy way. */
    uint64_t const *pu64Src;
    VBOXSTRICTRC rcStrict = iemMemMap(pIemCpu, (void **)&pu64Src, sizeof(*pu64Src), X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
    if (rcStrict == VINF_SUCCESS)
    {
        *pu64Value = *pu64Src;
        rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pu64Src, IEM_ACCESS_STACK_R);

        /* Commit the new RSP value. */
        if (rcStrict == VINF_SUCCESS)
            *pTmpRsp = NewRsp;
    }

    return rcStrict;
}


/**
 * Begin a special stack push (used by interrupt, exceptions and such).
 *
 * This will raise \#SS or \#PF if appropriate.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   cbMem               The number of bytes to push onto the stack.
 * @param   ppvMem              Where to return the pointer to the stack memory.
 *                              As with the other memory functions this could be
 *                              direct access or bounce buffered access, so
 *                              don't commit register until the commit call
 *                              succeeds.
 * @param   puNewRsp            Where to return the new RSP value.  This must be
 *                              passed unchanged to
 *                              iemMemStackPushCommitSpecial().
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushBeginSpecial(PIEMCPU pIemCpu, size_t cbMem, void **ppvMem, uint64_t *puNewRsp)
{
    Assert(cbMem < UINT8_MAX);
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pIemCpu, pCtx, (uint8_t)cbMem, puNewRsp);
    return iemMemMap(pIemCpu, ppvMem, cbMem, X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_W);
}


/**
 * Commits a special stack push (started by iemMemStackPushBeginSpecial).
 *
 * This will update the rSP.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pvMem               The pointer returned by
 *                              iemMemStackPushBeginSpecial().
 * @param   uNewRsp             The new RSP value returned by
 *                              iemMemStackPushBeginSpecial().
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPushCommitSpecial(PIEMCPU pIemCpu, void *pvMem, uint64_t uNewRsp)
{
    VBOXSTRICTRC rcStrict = iemMemCommitAndUnmap(pIemCpu, pvMem, IEM_ACCESS_STACK_W);
    if (rcStrict == VINF_SUCCESS)
        pIemCpu->CTX_SUFF(pCtx)->rsp = uNewRsp;
    return rcStrict;
}


/**
 * Begin a special stack pop (used by iret, retf and such).
 *
 * This will raise \#SS or \#PF if appropriate.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   cbMem               The number of bytes to push onto the stack.
 * @param   ppvMem              Where to return the pointer to the stack memory.
 * @param   puNewRsp            Where to return the new RSP value.  This must be
 *                              passed unchanged to
 *                              iemMemStackPopCommitSpecial() or applied
 *                              manually if iemMemStackPopDoneSpecial() is used.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopBeginSpecial(PIEMCPU pIemCpu, size_t cbMem, void const **ppvMem, uint64_t *puNewRsp)
{
    Assert(cbMem < UINT8_MAX);
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pIemCpu, pCtx, (uint8_t)cbMem, puNewRsp);
    return iemMemMap(pIemCpu, (void **)ppvMem, cbMem, X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
}


/**
 * Continue a special stack pop (used by iret and retf).
 *
 * This will raise \#SS or \#PF if appropriate.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   cbMem               The number of bytes to push onto the stack.
 * @param   ppvMem              Where to return the pointer to the stack memory.
 * @param   puNewRsp            Where to return the new RSP value.  This must be
 *                              passed unchanged to
 *                              iemMemStackPopCommitSpecial() or applied
 *                              manually if iemMemStackPopDoneSpecial() is used.
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopContinueSpecial(PIEMCPU pIemCpu, size_t cbMem, void const **ppvMem, uint64_t *puNewRsp)
{
    Assert(cbMem < UINT8_MAX);
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp;
    NewRsp.u = *puNewRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(pIemCpu, pCtx, &NewRsp, 8);
    *puNewRsp = NewRsp.u;
    return iemMemMap(pIemCpu, (void **)ppvMem, cbMem, X86_SREG_SS, GCPtrTop, IEM_ACCESS_STACK_R);
}


/**
 * Commits a special stack pop (started by iemMemStackPopBeginSpecial).
 *
 * This will update the rSP.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pvMem               The pointer returned by
 *                              iemMemStackPopBeginSpecial().
 * @param   uNewRsp             The new RSP value returned by
 *                              iemMemStackPopBeginSpecial().
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopCommitSpecial(PIEMCPU pIemCpu, void const *pvMem, uint64_t uNewRsp)
{
    VBOXSTRICTRC rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pvMem, IEM_ACCESS_STACK_R);
    if (rcStrict == VINF_SUCCESS)
        pIemCpu->CTX_SUFF(pCtx)->rsp = uNewRsp;
    return rcStrict;
}


/**
 * Done with a special stack pop (started by iemMemStackPopBeginSpecial or
 * iemMemStackPopContinueSpecial).
 *
 * The caller will manually commit the rSP.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pvMem               The pointer returned by
 *                              iemMemStackPopBeginSpecial() or
 *                              iemMemStackPopContinueSpecial().
 */
IEM_STATIC VBOXSTRICTRC iemMemStackPopDoneSpecial(PIEMCPU pIemCpu, void const *pvMem)
{
    return iemMemCommitAndUnmap(pIemCpu, (void *)pvMem, IEM_ACCESS_STACK_R);
}


/**
 * Fetches a system table byte.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pbDst               Where to return the byte.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchSysU8(PIEMCPU pIemCpu, uint8_t *pbDst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint8_t const *pbSrc;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pbSrc, sizeof(*pbSrc), iSegReg, GCPtrMem, IEM_ACCESS_SYS_R);
    if (rc == VINF_SUCCESS)
    {
        *pbDst = *pbSrc;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pbSrc, IEM_ACCESS_SYS_R);
    }
    return rc;
}


/**
 * Fetches a system table word.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu16Dst             Where to return the word.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchSysU16(PIEMCPU pIemCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint16_t const *pu16Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu16Src, sizeof(*pu16Src), iSegReg, GCPtrMem, IEM_ACCESS_SYS_R);
    if (rc == VINF_SUCCESS)
    {
        *pu16Dst = *pu16Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu16Src, IEM_ACCESS_SYS_R);
    }
    return rc;
}


/**
 * Fetches a system table dword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu32Dst             Where to return the dword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchSysU32(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint32_t const *pu32Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu32Src, sizeof(*pu32Src), iSegReg, GCPtrMem, IEM_ACCESS_SYS_R);
    if (rc == VINF_SUCCESS)
    {
        *pu32Dst = *pu32Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu32Src, IEM_ACCESS_SYS_R);
    }
    return rc;
}


/**
 * Fetches a system table qword.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu64Dst             Where to return the qword.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchSysU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
{
    /* The lazy approach for now... */
    uint64_t const *pu64Src;
    VBOXSTRICTRC rc = iemMemMap(pIemCpu, (void **)&pu64Src, sizeof(*pu64Src), iSegReg, GCPtrMem, IEM_ACCESS_SYS_R);
    if (rc == VINF_SUCCESS)
    {
        *pu64Dst = *pu64Src;
        rc = iemMemCommitAndUnmap(pIemCpu, (void *)pu64Src, IEM_ACCESS_SYS_R);
    }
    return rc;
}


/**
 * Fetches a descriptor table entry with caller specified error code.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU.
 * @param   pDesc               Where to return the descriptor table entry.
 * @param   uSel                The selector which table entry to fetch.
 * @param   uXcpt               The exception to raise on table lookup error.
 * @param   uErrorCode          The error code associated with the exception.
 */
IEM_STATIC VBOXSTRICTRC
iemMemFetchSelDescWithErr(PIEMCPU pIemCpu, PIEMSELDESC pDesc, uint16_t uSel, uint8_t uXcpt, uint16_t uErrorCode)
{
    AssertPtr(pDesc);
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /** @todo did the 286 require all 8 bytes to be accessible? */
    /*
     * Get the selector table base and check bounds.
     */
    RTGCPTR GCPtrBase;
    if (uSel & X86_SEL_LDT)
    {
        if (   !pCtx->ldtr.Attr.n.u1Present
            || (uSel | X86_SEL_RPL_LDT) > pCtx->ldtr.u32Limit )
        {
            Log(("iemMemFetchSelDesc: LDT selector %#x is out of bounds (%3x) or ldtr is NP (%#x)\n",
                 uSel, pCtx->ldtr.u32Limit, pCtx->ldtr.Sel));
            return iemRaiseXcptOrInt(pIemCpu, 0, uXcpt, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                                     uErrorCode, 0);
        }

        Assert(pCtx->ldtr.Attr.n.u1Present);
        GCPtrBase = pCtx->ldtr.u64Base;
    }
    else
    {
        if ((uSel | X86_SEL_RPL_LDT) > pCtx->gdtr.cbGdt)
        {
            Log(("iemMemFetchSelDesc: GDT selector %#x is out of bounds (%3x)\n", uSel, pCtx->gdtr.cbGdt));
            return iemRaiseXcptOrInt(pIemCpu, 0, uXcpt, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                                     uErrorCode, 0);
        }
        GCPtrBase = pCtx->gdtr.pGdt;
    }

    /*
     * Read the legacy descriptor and maybe the long mode extensions if
     * required.
     */
    VBOXSTRICTRC rcStrict = iemMemFetchSysU64(pIemCpu, &pDesc->Legacy.u, UINT8_MAX, GCPtrBase + (uSel & X86_SEL_MASK));
    if (rcStrict == VINF_SUCCESS)
    {
        if (   !IEM_IS_LONG_MODE(pIemCpu)
            || pDesc->Legacy.Gen.u1DescType)
            pDesc->Long.au64[1] = 0;
        else if ((uint32_t)(uSel | X86_SEL_RPL_LDT) + 8 <= (uSel & X86_SEL_LDT ? pCtx->ldtr.u32Limit : pCtx->gdtr.cbGdt))
            rcStrict = iemMemFetchSysU64(pIemCpu, &pDesc->Long.au64[1], UINT8_MAX, GCPtrBase + (uSel | X86_SEL_RPL_LDT) + 1);
        else
        {
            Log(("iemMemFetchSelDesc: system selector %#x is out of bounds\n", uSel));
            /** @todo is this the right exception? */
            return iemRaiseXcptOrInt(pIemCpu, 0, uXcpt, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErrorCode, 0);
        }
    }
    return rcStrict;
}


/**
 * Fetches a descriptor table entry.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU.
 * @param   pDesc               Where to return the descriptor table entry.
 * @param   uSel                The selector which table entry to fetch.
 * @param   uXcpt               The exception to raise on table lookup error.
 */
IEM_STATIC VBOXSTRICTRC iemMemFetchSelDesc(PIEMCPU pIemCpu, PIEMSELDESC pDesc, uint16_t uSel, uint8_t uXcpt)
{
    return iemMemFetchSelDescWithErr(pIemCpu, pDesc, uSel, uXcpt, uSel & X86_SEL_MASK_OFF_RPL);
}


/**
 * Fakes a long mode stack selector for SS = 0.
 *
 * @param   pDescSs             Where to return the fake stack descriptor.
 * @param   uDpl                The DPL we want.
 */
IEM_STATIC void iemMemFakeStackSelDesc(PIEMSELDESC pDescSs, uint32_t uDpl)
{
    pDescSs->Long.au64[0] = 0;
    pDescSs->Long.au64[1] = 0;
    pDescSs->Long.Gen.u4Type     = X86_SEL_TYPE_RW_ACC;
    pDescSs->Long.Gen.u1DescType = 1; /* 1 = code / data, 0 = system. */
    pDescSs->Long.Gen.u2Dpl      = uDpl;
    pDescSs->Long.Gen.u1Present  = 1;
    pDescSs->Long.Gen.u1Long     = 1;
}


/**
 * Marks the selector descriptor as accessed (only non-system descriptors).
 *
 * This function ASSUMES that iemMemFetchSelDesc has be called previously and
 * will therefore skip the limit checks.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU.
 * @param   uSel                The selector.
 */
IEM_STATIC VBOXSTRICTRC iemMemMarkSelDescAccessed(PIEMCPU pIemCpu, uint16_t uSel)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Get the selector table base and calculate the entry address.
     */
    RTGCPTR GCPtr = uSel & X86_SEL_LDT
                  ? pCtx->ldtr.u64Base
                  : pCtx->gdtr.pGdt;
    GCPtr += uSel & X86_SEL_MASK;

    /*
     * ASMAtomicBitSet will assert if the address is misaligned, so do some
     * ugly stuff to avoid this.  This will make sure it's an atomic access
     * as well more or less remove any question about 8-bit or 32-bit accesss.
     */
    VBOXSTRICTRC        rcStrict;
    uint32_t volatile  *pu32;
    if ((GCPtr & 3) == 0)
    {
        /* The normal case, map the 32-bit bits around the accessed bit (40). */
        GCPtr += 2 + 2;
        rcStrict = iemMemMap(pIemCpu, (void **)&pu32, 4, UINT8_MAX, GCPtr, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        ASMAtomicBitSet(pu32, 8); /* X86_SEL_TYPE_ACCESSED is 1, but it is preceeded by u8BaseHigh1. */
    }
    else
    {
        /* The misaligned GDT/LDT case, map the whole thing. */
        rcStrict = iemMemMap(pIemCpu, (void **)&pu32, 8, UINT8_MAX, GCPtr, IEM_ACCESS_SYS_RW);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        switch ((uintptr_t)pu32 & 3)
        {
            case 0: ASMAtomicBitSet(pu32,                         40 + 0 -  0); break;
            case 1: ASMAtomicBitSet((uint8_t volatile *)pu32 + 3, 40 + 0 - 24); break;
            case 2: ASMAtomicBitSet((uint8_t volatile *)pu32 + 2, 40 + 0 - 16); break;
            case 3: ASMAtomicBitSet((uint8_t volatile *)pu32 + 1, 40 + 0 -  8); break;
        }
    }

    return iemMemCommitAndUnmap(pIemCpu, (void *)pu32, IEM_ACCESS_SYS_RW);
}

/** @} */


/*
 * Include the C/C++ implementation of instruction.
 */
#include "IEMAllCImpl.cpp.h"



/** @name   "Microcode" macros.
 *
 * The idea is that we should be able to use the same code to interpret
 * instructions as well as recompiler instructions.  Thus this obfuscation.
 *
 * @{
 */
#define IEM_MC_BEGIN(a_cArgs, a_cLocals)                {
#define IEM_MC_END()                                    }
#define IEM_MC_PAUSE()                                  do {} while (0)
#define IEM_MC_CONTINUE()                               do {} while (0)

/** Internal macro. */
#define IEM_MC_RETURN_ON_FAILURE(a_Expr) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = a_Expr; \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)

#define IEM_MC_ADVANCE_RIP()                            iemRegUpdateRipAndClearRF(pIemCpu)
#define IEM_MC_REL_JMP_S8(a_i8)                         IEM_MC_RETURN_ON_FAILURE(iemRegRipRelativeJumpS8(pIemCpu, a_i8))
#define IEM_MC_REL_JMP_S16(a_i16)                       IEM_MC_RETURN_ON_FAILURE(iemRegRipRelativeJumpS16(pIemCpu, a_i16))
#define IEM_MC_REL_JMP_S32(a_i32)                       IEM_MC_RETURN_ON_FAILURE(iemRegRipRelativeJumpS32(pIemCpu, a_i32))
#define IEM_MC_SET_RIP_U16(a_u16NewIP)                  IEM_MC_RETURN_ON_FAILURE(iemRegRipJump((pIemCpu), (a_u16NewIP)))
#define IEM_MC_SET_RIP_U32(a_u32NewIP)                  IEM_MC_RETURN_ON_FAILURE(iemRegRipJump((pIemCpu), (a_u32NewIP)))
#define IEM_MC_SET_RIP_U64(a_u64NewIP)                  IEM_MC_RETURN_ON_FAILURE(iemRegRipJump((pIemCpu), (a_u64NewIP)))

#define IEM_MC_RAISE_DIVIDE_ERROR()                     return iemRaiseDivideError(pIemCpu)
#define IEM_MC_MAYBE_RAISE_DEVICE_NOT_AVAILABLE()       \
    do { \
        if ((pIemCpu)->CTX_SUFF(pCtx)->cr0 & (X86_CR0_EM | X86_CR0_TS)) \
            return iemRaiseDeviceNotAvailable(pIemCpu); \
    } while (0)
#define IEM_MC_MAYBE_RAISE_FPU_XCPT() \
    do { \
        if ((pIemCpu)->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.FSW & X86_FSW_ES) \
            return iemRaiseMathFault(pIemCpu); \
    } while (0)
#define IEM_MC_MAYBE_RAISE_SSE2_RELATED_XCPT() \
    do { \
        if (   (pIemCpu->CTX_SUFF(pCtx)->cr0 & X86_CR0_EM) \
            || !(pIemCpu->CTX_SUFF(pCtx)->cr4 & X86_CR4_OSFXSR) \
            || !IEM_GET_GUEST_CPU_FEATURES(pIemCpu)->fSse2) \
            return iemRaiseUndefinedOpcode(pIemCpu); \
        if (pIemCpu->CTX_SUFF(pCtx)->cr0 & X86_CR0_TS) \
            return iemRaiseDeviceNotAvailable(pIemCpu); \
    } while (0)
#define IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT() \
    do { \
        if (   ((pIemCpu)->CTX_SUFF(pCtx)->cr0 & X86_CR0_EM) \
            || !IEM_GET_GUEST_CPU_FEATURES(pIemCpu)->fMmx) \
            return iemRaiseUndefinedOpcode(pIemCpu); \
        if (pIemCpu->CTX_SUFF(pCtx)->cr0 & X86_CR0_TS) \
            return iemRaiseDeviceNotAvailable(pIemCpu); \
    } while (0)
#define IEM_MC_MAYBE_RAISE_MMX_RELATED_XCPT_CHECK_SSE_OR_MMXEXT() \
    do { \
        if (   ((pIemCpu)->CTX_SUFF(pCtx)->cr0 & X86_CR0_EM) \
            || (   !IEM_GET_GUEST_CPU_FEATURES(pIemCpu)->fSse \
                && !IEM_GET_GUEST_CPU_FEATURES(pIemCpu)->fAmdMmxExts) ) \
            return iemRaiseUndefinedOpcode(pIemCpu); \
        if (pIemCpu->CTX_SUFF(pCtx)->cr0 & X86_CR0_TS) \
            return iemRaiseDeviceNotAvailable(pIemCpu); \
    } while (0)
#define IEM_MC_RAISE_GP0_IF_CPL_NOT_ZERO() \
    do { \
        if (pIemCpu->uCpl != 0) \
            return iemRaiseGeneralProtectionFault0(pIemCpu); \
    } while (0)


#define IEM_MC_LOCAL(a_Type, a_Name)                    a_Type a_Name
#define IEM_MC_LOCAL_CONST(a_Type, a_Name, a_Value)     a_Type const a_Name = (a_Value)
#define IEM_MC_REF_LOCAL(a_pRefArg, a_Local)            (a_pRefArg) = &(a_Local)
#define IEM_MC_ARG(a_Type, a_Name, a_iArg)              a_Type a_Name
#define IEM_MC_ARG_CONST(a_Type, a_Name, a_Value, a_iArg)       a_Type const a_Name = (a_Value)
#define IEM_MC_ARG_LOCAL_REF(a_Type, a_Name, a_Local, a_iArg)   a_Type const a_Name = &(a_Local)
#define IEM_MC_ARG_LOCAL_EFLAGS(a_pName, a_Name, a_iArg) \
    uint32_t a_Name; \
    uint32_t *a_pName = &a_Name
#define IEM_MC_COMMIT_EFLAGS(a_EFlags) \
   do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u = (a_EFlags); Assert((pIemCpu)->CTX_SUFF(pCtx)->eflags.u & X86_EFL_1); } while (0)

#define IEM_MC_ASSIGN(a_VarOrArg, a_CVariableOrConst)   (a_VarOrArg) = (a_CVariableOrConst)
#define IEM_MC_ASSIGN_TO_SMALLER                        IEM_MC_ASSIGN

#define IEM_MC_FETCH_GREG_U8(a_u8Dst, a_iGReg)          (a_u8Dst)  = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_ZX_U16(a_u16Dst, a_iGReg)  (a_u16Dst) = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_ZX_U32(a_u32Dst, a_iGReg)  (a_u32Dst) = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_ZX_U64(a_u64Dst, a_iGReg)  (a_u64Dst) = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_SX_U16(a_u16Dst, a_iGReg)  (a_u16Dst) = (int8_t)iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_SX_U32(a_u32Dst, a_iGReg)  (a_u32Dst) = (int8_t)iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_SX_U64(a_u64Dst, a_iGReg)  (a_u64Dst) = (int8_t)iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16(a_u16Dst, a_iGReg)        (a_u16Dst) = iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16_ZX_U32(a_u32Dst, a_iGReg) (a_u32Dst) = iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16_ZX_U64(a_u64Dst, a_iGReg) (a_u64Dst) = iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16_SX_U32(a_u32Dst, a_iGReg) (a_u32Dst) = (int16_t)iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16_SX_U64(a_u64Dst, a_iGReg) (a_u64Dst) = (int16_t)iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U32(a_u32Dst, a_iGReg)        (a_u32Dst) = iemGRegFetchU32(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U32_ZX_U64(a_u64Dst, a_iGReg) (a_u64Dst) = iemGRegFetchU32(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U32_SX_U64(a_u64Dst, a_iGReg) (a_u64Dst) = (int32_t)iemGRegFetchU32(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U64(a_u64Dst, a_iGReg)        (a_u64Dst) = iemGRegFetchU64(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U64_ZX_U64                    IEM_MC_FETCH_GREG_U64
#define IEM_MC_FETCH_SREG_U16(a_u16Dst, a_iSReg)        (a_u16Dst) = iemSRegFetchU16(pIemCpu, (a_iSReg))
#define IEM_MC_FETCH_SREG_ZX_U32(a_u32Dst, a_iSReg)     (a_u32Dst) = iemSRegFetchU16(pIemCpu, (a_iSReg))
#define IEM_MC_FETCH_SREG_ZX_U64(a_u64Dst, a_iSReg)     (a_u64Dst) = iemSRegFetchU16(pIemCpu, (a_iSReg))
#define IEM_MC_FETCH_CR0_U16(a_u16Dst)                  (a_u16Dst) = (uint16_t)(pIemCpu)->CTX_SUFF(pCtx)->cr0
#define IEM_MC_FETCH_CR0_U32(a_u32Dst)                  (a_u32Dst) = (uint32_t)(pIemCpu)->CTX_SUFF(pCtx)->cr0
#define IEM_MC_FETCH_CR0_U64(a_u64Dst)                  (a_u64Dst) = (pIemCpu)->CTX_SUFF(pCtx)->cr0
#define IEM_MC_FETCH_LDTR_U16(a_u16Dst)                 (a_u16Dst) = (pIemCpu)->CTX_SUFF(pCtx)->ldtr.Sel
#define IEM_MC_FETCH_LDTR_U32(a_u32Dst)                 (a_u32Dst) = (pIemCpu)->CTX_SUFF(pCtx)->ldtr.Sel
#define IEM_MC_FETCH_LDTR_U64(a_u64Dst)                 (a_u64Dst) = (pIemCpu)->CTX_SUFF(pCtx)->ldtr.Sel
#define IEM_MC_FETCH_TR_U16(a_u16Dst)                   (a_u16Dst) = (pIemCpu)->CTX_SUFF(pCtx)->tr.Sel
#define IEM_MC_FETCH_TR_U32(a_u32Dst)                   (a_u32Dst) = (pIemCpu)->CTX_SUFF(pCtx)->tr.Sel
#define IEM_MC_FETCH_TR_U64(a_u64Dst)                   (a_u64Dst) = (pIemCpu)->CTX_SUFF(pCtx)->tr.Sel
/** @note Not for IOPL or IF testing or modification. */
#define IEM_MC_FETCH_EFLAGS(a_EFlags)                   (a_EFlags) = (pIemCpu)->CTX_SUFF(pCtx)->eflags.u
#define IEM_MC_FETCH_EFLAGS_U8(a_EFlags)                (a_EFlags) = (uint8_t)(pIemCpu)->CTX_SUFF(pCtx)->eflags.u
#define IEM_MC_FETCH_FSW(a_u16Fsw)                      (a_u16Fsw) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.FSW
#define IEM_MC_FETCH_FCW(a_u16Fcw)                      (a_u16Fcw) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.FCW

#define IEM_MC_STORE_GREG_U8(a_iGReg, a_u8Value)        *iemGRegRefU8(pIemCpu, (a_iGReg)) = (a_u8Value)
#define IEM_MC_STORE_GREG_U16(a_iGReg, a_u16Value)      *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (a_u16Value)
#define IEM_MC_STORE_GREG_U32(a_iGReg, a_u32Value)      *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (uint32_t)(a_u32Value) /* clear high bits. */
#define IEM_MC_STORE_GREG_U64(a_iGReg, a_u64Value)      *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (a_u64Value)
#define IEM_MC_STORE_GREG_U8_CONST                      IEM_MC_STORE_GREG_U8
#define IEM_MC_STORE_GREG_U16_CONST                     IEM_MC_STORE_GREG_U16
#define IEM_MC_STORE_GREG_U32_CONST                     IEM_MC_STORE_GREG_U32
#define IEM_MC_STORE_GREG_U64_CONST                     IEM_MC_STORE_GREG_U64
#define IEM_MC_CLEAR_HIGH_GREG_U64(a_iGReg)             *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) &= UINT32_MAX
#define IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF(a_pu32Dst)    do { (a_pu32Dst)[1] = 0; } while (0)
#define IEM_MC_STORE_FPUREG_R80_SRC_REF(a_iSt, a_pr80Src) \
    do { pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[a_iSt].r80 = *(a_pr80Src); } while (0)

#define IEM_MC_REF_GREG_U8(a_pu8Dst, a_iGReg)           (a_pu8Dst) = iemGRegRefU8(pIemCpu, (a_iGReg))
#define IEM_MC_REF_GREG_U16(a_pu16Dst, a_iGReg)         (a_pu16Dst) = (uint16_t *)iemGRegRef(pIemCpu, (a_iGReg))
/** @todo User of IEM_MC_REF_GREG_U32 needs to clear the high bits on commit.
 *        Use IEM_MC_CLEAR_HIGH_GREG_U64_BY_REF! */
#define IEM_MC_REF_GREG_U32(a_pu32Dst, a_iGReg)         (a_pu32Dst) = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg))
#define IEM_MC_REF_GREG_U64(a_pu64Dst, a_iGReg)         (a_pu64Dst) = (uint64_t *)iemGRegRef(pIemCpu, (a_iGReg))
/** @note Not for IOPL or IF testing or modification. */
#define IEM_MC_REF_EFLAGS(a_pEFlags)                    (a_pEFlags) = &(pIemCpu)->CTX_SUFF(pCtx)->eflags.u

#define IEM_MC_ADD_GREG_U8(a_iGReg, a_u8Value)          *(uint8_t  *)iemGRegRef(pIemCpu, (a_iGReg)) += (a_u8Value)
#define IEM_MC_ADD_GREG_U16(a_iGReg, a_u16Value)        *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) += (a_u16Value)
#define IEM_MC_ADD_GREG_U32(a_iGReg, a_u32Value) \
    do { \
        uint32_t *pu32Reg = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg)); \
        *pu32Reg += (a_u32Value); \
        pu32Reg[1] = 0; /* implicitly clear the high bit. */ \
    } while (0)
#define IEM_MC_ADD_GREG_U64(a_iGReg, a_u64Value)        *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) += (a_u64Value)

#define IEM_MC_SUB_GREG_U8(a_iGReg,  a_u8Value)         *(uint8_t *)iemGRegRef(pIemCpu, (a_iGReg)) -= (a_u8Value)
#define IEM_MC_SUB_GREG_U16(a_iGReg, a_u16Value)        *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) -= (a_u16Value)
#define IEM_MC_SUB_GREG_U32(a_iGReg, a_u32Value) \
    do { \
        uint32_t *pu32Reg = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg)); \
        *pu32Reg -= (a_u32Value); \
        pu32Reg[1] = 0; /* implicitly clear the high bit. */ \
    } while (0)
#define IEM_MC_SUB_GREG_U64(a_iGReg, a_u64Value)        *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) -= (a_u64Value)
#define IEM_MC_SUB_LOCAL_U16(a_u16Value, a_u16Const)   do { (a_u16Value) -= a_u16Const; } while (0)

#define IEM_MC_ADD_GREG_U8_TO_LOCAL(a_u8Value, a_iGReg)    do { (a_u8Value)  += iemGRegFetchU8( pIemCpu, (a_iGReg)); } while (0)
#define IEM_MC_ADD_GREG_U16_TO_LOCAL(a_u16Value, a_iGReg)  do { (a_u16Value) += iemGRegFetchU16(pIemCpu, (a_iGReg)); } while (0)
#define IEM_MC_ADD_GREG_U32_TO_LOCAL(a_u32Value, a_iGReg)  do { (a_u32Value) += iemGRegFetchU32(pIemCpu, (a_iGReg)); } while (0)
#define IEM_MC_ADD_GREG_U64_TO_LOCAL(a_u64Value, a_iGReg)  do { (a_u64Value) += iemGRegFetchU64(pIemCpu, (a_iGReg)); } while (0)
#define IEM_MC_ADD_LOCAL_S16_TO_EFF_ADDR(a_EffAddr, a_i16) do { (a_EffAddr) += (a_i16); } while (0)
#define IEM_MC_ADD_LOCAL_S32_TO_EFF_ADDR(a_EffAddr, a_i32) do { (a_EffAddr) += (a_i32); } while (0)
#define IEM_MC_ADD_LOCAL_S64_TO_EFF_ADDR(a_EffAddr, a_i64) do { (a_EffAddr) += (a_i64); } while (0)

#define IEM_MC_AND_LOCAL_U8(a_u8Local, a_u8Mask)        do { (a_u8Local)  &= (a_u8Mask);  } while (0)
#define IEM_MC_AND_LOCAL_U16(a_u16Local, a_u16Mask)     do { (a_u16Local) &= (a_u16Mask); } while (0)
#define IEM_MC_AND_LOCAL_U32(a_u32Local, a_u32Mask)     do { (a_u32Local) &= (a_u32Mask); } while (0)
#define IEM_MC_AND_LOCAL_U64(a_u64Local, a_u64Mask)     do { (a_u64Local) &= (a_u64Mask); } while (0)

#define IEM_MC_AND_ARG_U16(a_u16Arg, a_u16Mask)         do { (a_u16Arg) &= (a_u16Mask); } while (0)
#define IEM_MC_AND_ARG_U32(a_u32Arg, a_u32Mask)         do { (a_u32Arg) &= (a_u32Mask); } while (0)
#define IEM_MC_AND_ARG_U64(a_u64Arg, a_u64Mask)         do { (a_u64Arg) &= (a_u64Mask); } while (0)

#define IEM_MC_OR_LOCAL_U8(a_u8Local, a_u8Mask)         do { (a_u8Local)  |= (a_u8Mask);  } while (0)
#define IEM_MC_OR_LOCAL_U16(a_u16Local, a_u16Mask)      do { (a_u16Local) |= (a_u16Mask); } while (0)
#define IEM_MC_OR_LOCAL_U32(a_u32Local, a_u32Mask)      do { (a_u32Local) |= (a_u32Mask); } while (0)

#define IEM_MC_SAR_LOCAL_S16(a_i16Local, a_cShift)      do { (a_i16Local) >>= (a_cShift);  } while (0)
#define IEM_MC_SAR_LOCAL_S32(a_i32Local, a_cShift)      do { (a_i32Local) >>= (a_cShift);  } while (0)
#define IEM_MC_SAR_LOCAL_S64(a_i64Local, a_cShift)      do { (a_i64Local) >>= (a_cShift);  } while (0)

#define IEM_MC_SHL_LOCAL_S16(a_i16Local, a_cShift)      do { (a_i16Local) <<= (a_cShift);  } while (0)
#define IEM_MC_SHL_LOCAL_S32(a_i32Local, a_cShift)      do { (a_i32Local) <<= (a_cShift);  } while (0)
#define IEM_MC_SHL_LOCAL_S64(a_i64Local, a_cShift)      do { (a_i64Local) <<= (a_cShift);  } while (0)

#define IEM_MC_AND_2LOCS_U32(a_u32Local, a_u32Mask)     do { (a_u32Local) &= (a_u32Mask); } while (0)

#define IEM_MC_OR_2LOCS_U32(a_u32Local, a_u32Mask)      do { (a_u32Local) |= (a_u32Mask); } while (0)

#define IEM_MC_AND_GREG_U8(a_iGReg, a_u8Value)          *(uint8_t  *)iemGRegRef(pIemCpu, (a_iGReg)) &= (a_u8Value)
#define IEM_MC_AND_GREG_U16(a_iGReg, a_u16Value)        *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) &= (a_u16Value)
#define IEM_MC_AND_GREG_U32(a_iGReg, a_u32Value) \
    do { \
        uint32_t *pu32Reg = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg)); \
        *pu32Reg &= (a_u32Value); \
        pu32Reg[1] = 0; /* implicitly clear the high bit. */ \
    } while (0)
#define IEM_MC_AND_GREG_U64(a_iGReg, a_u64Value)        *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) &= (a_u64Value)

#define IEM_MC_OR_GREG_U8(a_iGReg, a_u8Value)           *(uint8_t  *)iemGRegRef(pIemCpu, (a_iGReg)) |= (a_u8Value)
#define IEM_MC_OR_GREG_U16(a_iGReg, a_u16Value)         *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) |= (a_u16Value)
#define IEM_MC_OR_GREG_U32(a_iGReg, a_u32Value) \
    do { \
        uint32_t *pu32Reg = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg)); \
        *pu32Reg |= (a_u32Value); \
        pu32Reg[1] = 0; /* implicitly clear the high bit. */ \
    } while (0)
#define IEM_MC_OR_GREG_U64(a_iGReg, a_u64Value)         *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) |= (a_u64Value)


/** @note Not for IOPL or IF modification. */
#define IEM_MC_SET_EFL_BIT(a_fBit)                      do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u |= (a_fBit); } while (0)
/** @note Not for IOPL or IF modification. */
#define IEM_MC_CLEAR_EFL_BIT(a_fBit)                    do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u &= ~(a_fBit); } while (0)
/** @note Not for IOPL or IF modification. */
#define IEM_MC_FLIP_EFL_BIT(a_fBit)                     do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u ^= (a_fBit); } while (0)

#define IEM_MC_CLEAR_FSW_EX()   do { (pIemCpu)->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.FSW &= X86_FSW_C_MASK | X86_FSW_TOP_MASK; } while (0)


#define IEM_MC_FETCH_MREG_U64(a_u64Value, a_iMReg) \
    do { (a_u64Value) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].mmx; } while (0)
#define IEM_MC_FETCH_MREG_U32(a_u32Value, a_iMReg) \
    do { (a_u32Value) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].au32[0]; } while (0)
#define IEM_MC_STORE_MREG_U64(a_iMReg, a_u64Value) \
    do { pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].mmx = (a_u64Value); } while (0)
#define IEM_MC_STORE_MREG_U32_ZX_U64(a_iMReg, a_u32Value) \
    do { pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].mmx = (uint32_t)(a_u32Value); } while (0)
#define IEM_MC_REF_MREG_U64(a_pu64Dst, a_iMReg)         \
        (a_pu64Dst) = (&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].mmx)
#define IEM_MC_REF_MREG_U64_CONST(a_pu64Dst, a_iMReg) \
        (a_pu64Dst) = ((uint64_t const *)&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].mmx)
#define IEM_MC_REF_MREG_U32_CONST(a_pu32Dst, a_iMReg) \
        (a_pu32Dst) = ((uint32_t const *)&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aRegs[(a_iMReg)].mmx)

#define IEM_MC_FETCH_XREG_U128(a_u128Value, a_iXReg) \
    do { (a_u128Value) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].xmm; } while (0)
#define IEM_MC_FETCH_XREG_U64(a_u64Value, a_iXReg) \
    do { (a_u64Value) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au64[0]; } while (0)
#define IEM_MC_FETCH_XREG_U32(a_u32Value, a_iXReg) \
    do { (a_u32Value) = pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au32[0]; } while (0)
#define IEM_MC_STORE_XREG_U128(a_iXReg, a_u128Value) \
    do { pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].xmm = (a_u128Value); } while (0)
#define IEM_MC_STORE_XREG_U64_ZX_U128(a_iXReg, a_u64Value) \
    do { pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au64[0] = (a_u64Value); \
         pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au64[1] = 0; \
    } while (0)
#define IEM_MC_STORE_XREG_U32_ZX_U128(a_iXReg, a_u32Value) \
    do { pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au64[0] = (uint32_t)(a_u32Value); \
         pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au64[1] = 0; \
    } while (0)
#define IEM_MC_REF_XREG_U128(a_pu128Dst, a_iXReg)       \
    (a_pu128Dst) = (&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].xmm)
#define IEM_MC_REF_XREG_U128_CONST(a_pu128Dst, a_iXReg) \
    (a_pu128Dst) = ((uint128_t const *)&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].xmm)
#define IEM_MC_REF_XREG_U64_CONST(a_pu64Dst, a_iXReg) \
    (a_pu64Dst) = ((uint64_t const *)&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.aXMM[(a_iXReg)].au64[0])

#define IEM_MC_FETCH_MEM_U8(a_u8Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &(a_u8Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM16_U8(a_u8Dst, a_iSeg, a_GCPtrMem16) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &(a_u8Dst), (a_iSeg), (a_GCPtrMem16)))
#define IEM_MC_FETCH_MEM32_U8(a_u8Dst, a_iSeg, a_GCPtrMem32) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &(a_u8Dst), (a_iSeg), (a_GCPtrMem32)))

#define IEM_MC_FETCH_MEM_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &(a_u16Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U16_DISP(a_u16Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &(a_u16Dst), (a_iSeg), (a_GCPtrMem) + (a_offDisp)))
#define IEM_MC_FETCH_MEM_I16(a_i16Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, (uint16_t *)&(a_i16Dst), (a_iSeg), (a_GCPtrMem)))

#define IEM_MC_FETCH_MEM_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &(a_u32Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U32_DISP(a_u32Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &(a_u32Dst), (a_iSeg), (a_GCPtrMem) + (a_offDisp)))
#define IEM_MC_FETCH_MEM_I32(a_i32Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, (uint32_t *)&(a_i32Dst), (a_iSeg), (a_GCPtrMem)))

#define IEM_MC_FETCH_MEM_S32_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataS32SxU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem)))

#define IEM_MC_FETCH_MEM_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U64_DISP(a_u64Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem) + (a_offDisp)))
#define IEM_MC_FETCH_MEM_U64_ALIGN_U128(a_u128Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64AlignedU128(pIemCpu, &(a_u128Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_I64(a_i64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, (uint64_t *)&(a_i64Dst), (a_iSeg), (a_GCPtrMem)))

#define IEM_MC_FETCH_MEM_R32(a_r32Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &(a_r32Dst).u32, (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_R64(a_r64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, &(a_r64Dst).au64[0], (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_R80(a_r80Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataR80(pIemCpu, &(a_r80Dst), (a_iSeg), (a_GCPtrMem)))

#define IEM_MC_FETCH_MEM_U128(a_u128Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU128(pIemCpu, &(a_u128Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U128_ALIGN_SSE(a_u128Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU128AlignedSse(pIemCpu, &(a_u128Dst), (a_iSeg), (a_GCPtrMem)))



#define IEM_MC_FETCH_MEM_U8_ZX_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint8_t u8Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &u8Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u16Dst) = u8Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U8_ZX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint8_t u8Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &u8Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u32Dst) = u8Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U8_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint8_t u8Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &u8Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u64Dst) = u8Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U16_ZX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint16_t u16Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &u16Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u32Dst) = u16Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U16_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint16_t u16Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &u16Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u64Dst) = u16Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U32_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint32_t u32Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &u32Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u64Dst) = u32Tmp; \
    } while (0)

#define IEM_MC_FETCH_MEM_U8_SX_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint8_t u8Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &u8Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u16Dst) = (int8_t)u8Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U8_SX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint8_t u8Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &u8Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u32Dst) = (int8_t)u8Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U8_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint8_t u8Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &u8Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u64Dst) = (int8_t)u8Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U16_SX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint16_t u16Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &u16Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u32Dst) = (int16_t)u16Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U16_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint16_t u16Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &u16Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u64Dst) = (int16_t)u16Tmp; \
    } while (0)
#define IEM_MC_FETCH_MEM_U32_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    do { \
        uint32_t u32Tmp; \
        IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &u32Tmp, (a_iSeg), (a_GCPtrMem))); \
        (a_u64Dst) = (int32_t)u32Tmp; \
    } while (0)

#define IEM_MC_STORE_MEM_U8(a_iSeg, a_GCPtrMem, a_u8Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU8(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u8Value)))
#define IEM_MC_STORE_MEM_U16(a_iSeg, a_GCPtrMem, a_u16Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU16(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u16Value)))
#define IEM_MC_STORE_MEM_U32(a_iSeg, a_GCPtrMem, a_u32Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU32(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u32Value)))
#define IEM_MC_STORE_MEM_U64(a_iSeg, a_GCPtrMem, a_u64Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU64(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u64Value)))

#define IEM_MC_STORE_MEM_U8_CONST(a_iSeg, a_GCPtrMem, a_u8C) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU8(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u8C)))
#define IEM_MC_STORE_MEM_U16_CONST(a_iSeg, a_GCPtrMem, a_u16C) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU16(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u16C)))
#define IEM_MC_STORE_MEM_U32_CONST(a_iSeg, a_GCPtrMem, a_u32C) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU32(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u32C)))
#define IEM_MC_STORE_MEM_U64_CONST(a_iSeg, a_GCPtrMem, a_u64C) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU64(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u64C)))

#define IEM_MC_STORE_MEM_I8_CONST_BY_REF( a_pi8Dst,  a_i8C)     *(a_pi8Dst)  = (a_i8C)
#define IEM_MC_STORE_MEM_I16_CONST_BY_REF(a_pi16Dst, a_i16C)    *(a_pi16Dst) = (a_i16C)
#define IEM_MC_STORE_MEM_I32_CONST_BY_REF(a_pi32Dst, a_i32C)    *(a_pi32Dst) = (a_i32C)
#define IEM_MC_STORE_MEM_I64_CONST_BY_REF(a_pi64Dst, a_i64C)    *(a_pi64Dst) = (a_i64C)
#define IEM_MC_STORE_MEM_NEG_QNAN_R32_BY_REF(a_pr32Dst)         (a_pr32Dst)->u32     = UINT32_C(0xffc00000)
#define IEM_MC_STORE_MEM_NEG_QNAN_R64_BY_REF(a_pr64Dst)         (a_pr64Dst)->au64[0] = UINT64_C(0xfff8000000000000)
#define IEM_MC_STORE_MEM_NEG_QNAN_R80_BY_REF(a_pr80Dst) \
    do { \
        (a_pr80Dst)->au64[0] = UINT64_C(0xc000000000000000); \
        (a_pr80Dst)->au16[4] = UINT16_C(0xffff); \
    } while (0)

#define IEM_MC_STORE_MEM_U128(a_iSeg, a_GCPtrMem, a_u128Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU128(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u128Value)))
#define IEM_MC_STORE_MEM_U128_ALIGN_SSE(a_iSeg, a_GCPtrMem, a_u128Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU128AlignedSse(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u128Value)))


#define IEM_MC_PUSH_U16(a_u16Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPushU16(pIemCpu, (a_u16Value)))
#define IEM_MC_PUSH_U32(a_u32Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPushU32(pIemCpu, (a_u32Value)))
#define IEM_MC_PUSH_U32_SREG(a_u32Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPushU32SReg(pIemCpu, (a_u32Value)))
#define IEM_MC_PUSH_U64(a_u64Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPushU64(pIemCpu, (a_u64Value)))

#define IEM_MC_POP_U16(a_pu16Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPopU16(pIemCpu, (a_pu16Value)))
#define IEM_MC_POP_U32(a_pu32Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPopU32(pIemCpu, (a_pu32Value)))
#define IEM_MC_POP_U64(a_pu64Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPopU64(pIemCpu, (a_pu64Value)))

/** Maps guest memory for direct or bounce buffered access.
 * The purpose is to pass it to an operand implementation, thus the a_iArg.
 * @remarks     May return.
 */
#define IEM_MC_MEM_MAP(a_pMem, a_fAccess, a_iSeg, a_GCPtrMem, a_iArg) \
    IEM_MC_RETURN_ON_FAILURE(iemMemMap(pIemCpu, (void **)&(a_pMem), sizeof(*(a_pMem)), (a_iSeg), (a_GCPtrMem), (a_fAccess)))

/** Maps guest memory for direct or bounce buffered access.
 * The purpose is to pass it to an operand implementation, thus the a_iArg.
 * @remarks     May return.
 */
#define IEM_MC_MEM_MAP_EX(a_pvMem, a_fAccess, a_cbMem, a_iSeg, a_GCPtrMem, a_iArg) \
    IEM_MC_RETURN_ON_FAILURE(iemMemMap(pIemCpu, (void **)&(a_pvMem), (a_cbMem), (a_iSeg), (a_GCPtrMem), (a_fAccess)))

/** Commits the memory and unmaps the guest memory.
 * @remarks     May return.
 */
#define IEM_MC_MEM_COMMIT_AND_UNMAP(a_pvMem, a_fAccess) \
    IEM_MC_RETURN_ON_FAILURE(iemMemCommitAndUnmap(pIemCpu, (a_pvMem), (a_fAccess)))

/** Commits the memory and unmaps the guest memory unless the FPU status word
 * indicates (@a a_u16FSW) and FPU control word indicates a pending exception
 * that would cause FLD not to store.
 *
 * The current understanding is that \#O, \#U, \#IA and \#IS will prevent a
 * store, while \#P will not.
 *
 * @remarks     May in theory return - for now.
 */
#define IEM_MC_MEM_COMMIT_AND_UNMAP_FOR_FPU_STORE(a_pvMem, a_fAccess, a_u16FSW) \
    do { \
        if (   !(a_u16FSW & X86_FSW_ES) \
            || !(  (a_u16FSW & (X86_FSW_UE | X86_FSW_OE | X86_FSW_IE)) \
                 & ~(pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.FCW & X86_FCW_MASK_ALL) ) ) \
            IEM_MC_RETURN_ON_FAILURE(iemMemCommitAndUnmap(pIemCpu, (a_pvMem), (a_fAccess))); \
    } while (0)

/** Calculate efficient address from R/M. */
#define IEM_MC_CALC_RM_EFF_ADDR(a_GCPtrEff, bRm, cbImm) \
    IEM_MC_RETURN_ON_FAILURE(iemOpHlpCalcRmEffAddr(pIemCpu, (bRm), (cbImm), &(a_GCPtrEff)))

#define IEM_MC_CALL_VOID_AIMPL_0(a_pfn)                   (a_pfn)()
#define IEM_MC_CALL_VOID_AIMPL_1(a_pfn, a0)               (a_pfn)((a0))
#define IEM_MC_CALL_VOID_AIMPL_2(a_pfn, a0, a1)           (a_pfn)((a0), (a1))
#define IEM_MC_CALL_VOID_AIMPL_3(a_pfn, a0, a1, a2)       (a_pfn)((a0), (a1), (a2))
#define IEM_MC_CALL_VOID_AIMPL_4(a_pfn, a0, a1, a2, a3)   (a_pfn)((a0), (a1), (a2), (a3))
#define IEM_MC_CALL_AIMPL_3(a_rc, a_pfn, a0, a1, a2)      (a_rc) = (a_pfn)((a0), (a1), (a2))
#define IEM_MC_CALL_AIMPL_4(a_rc, a_pfn, a0, a1, a2, a3)  (a_rc) = (a_pfn)((a0), (a1), (a2), (a3))

/**
 * Defers the rest of the instruction emulation to a C implementation routine
 * and returns, only taking the standard parameters.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @sa      IEM_DECL_IMPL_C_TYPE_0 and IEM_CIMPL_DEF_0.
 */
#define IEM_MC_CALL_CIMPL_0(a_pfnCImpl)                 return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode)

/**
 * Defers the rest of instruction emulation to a C implementation routine and
 * returns, taking one argument in addition to the standard ones.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The argument.
 */
#define IEM_MC_CALL_CIMPL_1(a_pfnCImpl, a0)             return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0)

/**
 * Defers the rest of the instruction emulation to a C implementation routine
 * and returns, taking two arguments in addition to the standard ones.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 */
#define IEM_MC_CALL_CIMPL_2(a_pfnCImpl, a0, a1)         return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1)

/**
 * Defers the rest of the instruction emulation to a C implementation routine
 * and returns, taking three arguments in addition to the standard ones.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 */
#define IEM_MC_CALL_CIMPL_3(a_pfnCImpl, a0, a1, a2)     return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1, a2)

/**
 * Defers the rest of the instruction emulation to a C implementation routine
 * and returns, taking four arguments in addition to the standard ones.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 * @param   a3              The fourth extra argument.
 */
#define IEM_MC_CALL_CIMPL_4(a_pfnCImpl, a0, a1, a2, a3)     return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1, a2, a3)

/**
 * Defers the rest of the instruction emulation to a C implementation routine
 * and returns, taking two arguments in addition to the standard ones.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 * @param   a3              The fourth extra argument.
 * @param   a4              The fifth extra argument.
 */
#define IEM_MC_CALL_CIMPL_5(a_pfnCImpl, a0, a1, a2, a3, a4) return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1, a2, a3, a4)

/**
 * Defers the entire instruction emulation to a C implementation routine and
 * returns, only taking the standard parameters.
 *
 * This shall be used without any IEM_MC_BEGIN or IEM_END macro surrounding it.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @sa      IEM_DECL_IMPL_C_TYPE_0 and IEM_CIMPL_DEF_0.
 */
#define IEM_MC_DEFER_TO_CIMPL_0(a_pfnCImpl)             (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode)

/**
 * Defers the entire instruction emulation to a C implementation routine and
 * returns, taking one argument in addition to the standard ones.
 *
 * This shall be used without any IEM_MC_BEGIN or IEM_END macro surrounding it.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The argument.
 */
#define IEM_MC_DEFER_TO_CIMPL_1(a_pfnCImpl, a0)         (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0)

/**
 * Defers the entire instruction emulation to a C implementation routine and
 * returns, taking two arguments in addition to the standard ones.
 *
 * This shall be used without any IEM_MC_BEGIN or IEM_END macro surrounding it.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 */
#define IEM_MC_DEFER_TO_CIMPL_2(a_pfnCImpl, a0, a1)     (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1)

/**
 * Defers the entire instruction emulation to a C implementation routine and
 * returns, taking three arguments in addition to the standard ones.
 *
 * This shall be used without any IEM_MC_BEGIN or IEM_END macro surrounding it.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 */
#define IEM_MC_DEFER_TO_CIMPL_3(a_pfnCImpl, a0, a1, a2) (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1, a2)

/**
 * Calls a FPU assembly implementation taking one visible argument.
 *
 * @param   a_pfnAImpl      Pointer to the assembly FPU routine.
 * @param   a0              The first extra argument.
 */
#define IEM_MC_CALL_FPU_AIMPL_1(a_pfnAImpl, a0) \
    do { \
        iemFpuPrepareUsage(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0)); \
    } while (0)

/**
 * Calls a FPU assembly implementation taking two visible arguments.
 *
 * @param   a_pfnAImpl      Pointer to the assembly FPU routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 */
#define IEM_MC_CALL_FPU_AIMPL_2(a_pfnAImpl, a0, a1) \
    do { \
        iemFpuPrepareUsage(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0), (a1)); \
    } while (0)

/**
 * Calls a FPU assembly implementation taking three visible arguments.
 *
 * @param   a_pfnAImpl      Pointer to the assembly FPU routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 */
#define IEM_MC_CALL_FPU_AIMPL_3(a_pfnAImpl, a0, a1, a2) \
    do { \
        iemFpuPrepareUsage(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0), (a1), (a2)); \
    } while (0)

#define IEM_MC_SET_FPU_RESULT(a_FpuData, a_FSW, a_pr80Value) \
    do { \
        (a_FpuData).FSW       = (a_FSW); \
        (a_FpuData).r80Result = *(a_pr80Value); \
    } while (0)

/** Pushes FPU result onto the stack. */
#define IEM_MC_PUSH_FPU_RESULT(a_FpuData) \
    iemFpuPushResult(pIemCpu, &a_FpuData)
/** Pushes FPU result onto the stack and sets the FPUDP. */
#define IEM_MC_PUSH_FPU_RESULT_MEM_OP(a_FpuData, a_iEffSeg, a_GCPtrEff) \
    iemFpuPushResultWithMemOp(pIemCpu, &a_FpuData, a_iEffSeg, a_GCPtrEff)

/** Replaces ST0 with value one and pushes value 2 onto the FPU stack. */
#define IEM_MC_PUSH_FPU_RESULT_TWO(a_FpuDataTwo) \
    iemFpuPushResultTwo(pIemCpu, &a_FpuDataTwo)

/** Stores FPU result in a stack register. */
#define IEM_MC_STORE_FPU_RESULT(a_FpuData, a_iStReg) \
    iemFpuStoreResult(pIemCpu, &a_FpuData, a_iStReg)
/** Stores FPU result in a stack register and pops the stack. */
#define IEM_MC_STORE_FPU_RESULT_THEN_POP(a_FpuData, a_iStReg) \
    iemFpuStoreResultThenPop(pIemCpu, &a_FpuData, a_iStReg)
/** Stores FPU result in a stack register and sets the FPUDP. */
#define IEM_MC_STORE_FPU_RESULT_MEM_OP(a_FpuData, a_iStReg, a_iEffSeg, a_GCPtrEff) \
    iemFpuStoreResultWithMemOp(pIemCpu, &a_FpuData, a_iStReg, a_iEffSeg, a_GCPtrEff)
/** Stores FPU result in a stack register, sets the FPUDP, and pops the
 *  stack. */
#define IEM_MC_STORE_FPU_RESULT_WITH_MEM_OP_THEN_POP(a_FpuData, a_iStReg, a_iEffSeg, a_GCPtrEff) \
    iemFpuStoreResultWithMemOpThenPop(pIemCpu, &a_FpuData, a_iStReg, a_iEffSeg, a_GCPtrEff)

/** Only update the FOP, FPUIP, and FPUCS. (For FNOP.) */
#define IEM_MC_UPDATE_FPU_OPCODE_IP() \
    iemFpuUpdateOpcodeAndIp(pIemCpu)
/** Free a stack register (for FFREE and FFREEP). */
#define IEM_MC_FPU_STACK_FREE(a_iStReg) \
    iemFpuStackFree(pIemCpu, a_iStReg)
/** Increment the FPU stack pointer. */
#define IEM_MC_FPU_STACK_INC_TOP() \
    iemFpuStackIncTop(pIemCpu)
/** Decrement the FPU stack pointer. */
#define IEM_MC_FPU_STACK_DEC_TOP() \
    iemFpuStackDecTop(pIemCpu)

/** Updates the FSW, FOP, FPUIP, and FPUCS. */
#define IEM_MC_UPDATE_FSW(a_u16FSW) \
    iemFpuUpdateFSW(pIemCpu, a_u16FSW)
/** Updates the FSW with a constant value as well as FOP, FPUIP, and FPUCS. */
#define IEM_MC_UPDATE_FSW_CONST(a_u16FSW) \
    iemFpuUpdateFSW(pIemCpu, a_u16FSW)
/** Updates the FSW, FOP, FPUIP, FPUCS, FPUDP, and FPUDS. */
#define IEM_MC_UPDATE_FSW_WITH_MEM_OP(a_u16FSW, a_iEffSeg, a_GCPtrEff) \
    iemFpuUpdateFSWWithMemOp(pIemCpu, a_u16FSW, a_iEffSeg, a_GCPtrEff)
/** Updates the FSW, FOP, FPUIP, and FPUCS, and then pops the stack. */
#define IEM_MC_UPDATE_FSW_THEN_POP(a_u16FSW) \
    iemFpuUpdateFSWThenPop(pIemCpu, a_u16FSW)
/** Updates the FSW, FOP, FPUIP, FPUCS, FPUDP and FPUDS, and then pops the
 *  stack. */
#define IEM_MC_UPDATE_FSW_WITH_MEM_OP_THEN_POP(a_u16FSW, a_iEffSeg, a_GCPtrEff) \
    iemFpuUpdateFSWWithMemOpThenPop(pIemCpu, a_u16FSW, a_iEffSeg, a_GCPtrEff)
/** Updates the FSW, FOP, FPUIP, and FPUCS, and then pops the stack twice. */
#define IEM_MC_UPDATE_FSW_THEN_POP_POP(a_u16FSW) \
    iemFpuUpdateFSWThenPop(pIemCpu, a_u16FSW)

/** Raises a FPU stack underflow exception.  Sets FPUIP, FPUCS and FOP. */
#define IEM_MC_FPU_STACK_UNDERFLOW(a_iStDst) \
    iemFpuStackUnderflow(pIemCpu, a_iStDst)
/** Raises a FPU stack underflow exception.  Sets FPUIP, FPUCS and FOP. Pops
 *  stack. */
#define IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP(a_iStDst) \
    iemFpuStackUnderflowThenPop(pIemCpu, a_iStDst)
/** Raises a FPU stack underflow exception.  Sets FPUIP, FPUCS, FOP, FPUDP and
 *  FPUDS. */
#define IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP(a_iStDst, a_iEffSeg, a_GCPtrEff) \
    iemFpuStackUnderflowWithMemOp(pIemCpu, a_iStDst, a_iEffSeg, a_GCPtrEff)
/** Raises a FPU stack underflow exception.  Sets FPUIP, FPUCS, FOP, FPUDP and
 *  FPUDS. Pops stack. */
#define IEM_MC_FPU_STACK_UNDERFLOW_MEM_OP_THEN_POP(a_iStDst, a_iEffSeg, a_GCPtrEff) \
    iemFpuStackUnderflowWithMemOpThenPop(pIemCpu, a_iStDst, a_iEffSeg, a_GCPtrEff)
/** Raises a FPU stack underflow exception.  Sets FPUIP, FPUCS and FOP. Pops
 *  stack twice. */
#define IEM_MC_FPU_STACK_UNDERFLOW_THEN_POP_POP() \
    iemFpuStackUnderflowThenPopPop(pIemCpu)
/** Raises a FPU stack underflow exception for an instruction pushing a result
 *  value onto the stack. Sets FPUIP, FPUCS and FOP. */
#define IEM_MC_FPU_STACK_PUSH_UNDERFLOW() \
    iemFpuStackPushUnderflow(pIemCpu)
/** Raises a FPU stack underflow exception for an instruction pushing a result
 *  value onto the stack and replacing ST0. Sets FPUIP, FPUCS and FOP. */
#define IEM_MC_FPU_STACK_PUSH_UNDERFLOW_TWO() \
    iemFpuStackPushUnderflowTwo(pIemCpu)

/** Raises a FPU stack overflow exception as part of a push attempt.  Sets
 *  FPUIP, FPUCS and FOP. */
#define IEM_MC_FPU_STACK_PUSH_OVERFLOW() \
    iemFpuStackPushOverflow(pIemCpu)
/** Raises a FPU stack overflow exception as part of a push attempt.  Sets
 *  FPUIP, FPUCS, FOP, FPUDP and FPUDS. */
#define IEM_MC_FPU_STACK_PUSH_OVERFLOW_MEM_OP(a_iEffSeg, a_GCPtrEff) \
    iemFpuStackPushOverflowWithMemOp(pIemCpu, a_iEffSeg, a_GCPtrEff)
/** Indicates that we (might) have modified the FPU state. */
#define IEM_MC_USED_FPU() \
    CPUMSetChangedFlags(IEMCPU_TO_VMCPU(pIemCpu), CPUM_CHANGED_FPU_REM)

/**
 * Calls a MMX assembly implementation taking two visible arguments.
 *
 * @param   a_pfnAImpl      Pointer to the assembly MMX routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 */
#define IEM_MC_CALL_MMX_AIMPL_2(a_pfnAImpl, a0, a1) \
    do { \
        iemFpuPrepareUsage(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0), (a1)); \
    } while (0)

/**
 * Calls a MMX assembly implementation taking three visible arguments.
 *
 * @param   a_pfnAImpl      Pointer to the assembly MMX routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 */
#define IEM_MC_CALL_MMX_AIMPL_3(a_pfnAImpl, a0, a1, a2) \
    do { \
        iemFpuPrepareUsage(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0), (a1), (a2)); \
    } while (0)


/**
 * Calls a SSE assembly implementation taking two visible arguments.
 *
 * @param   a_pfnAImpl      Pointer to the assembly MMX routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 */
#define IEM_MC_CALL_SSE_AIMPL_2(a_pfnAImpl, a0, a1) \
    do { \
        iemFpuPrepareUsageSse(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0), (a1)); \
    } while (0)

/**
 * Calls a SSE assembly implementation taking three visible arguments.
 *
 * @param   a_pfnAImpl      Pointer to the assembly MMX routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 */
#define IEM_MC_CALL_SSE_AIMPL_3(a_pfnAImpl, a0, a1, a2) \
    do { \
        iemFpuPrepareUsageSse(pIemCpu); \
        a_pfnAImpl(&pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87, (a0), (a1), (a2)); \
    } while (0)


/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_BIT_SET(a_fBit)                   if (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_BIT_NOT_SET(a_fBit)               if (!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit))) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_ANY_BITS_SET(a_fBits)             if (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBits)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_NO_BITS_SET(a_fBits)              if (!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBits))) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_BITS_NE(a_fBit1, a_fBit2)         \
    if (   !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
        != !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_BITS_EQ(a_fBit1, a_fBit2)         \
    if (   !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
        == !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(a_fBit, a_fBit1, a_fBit2) \
    if (   (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) \
        ||    !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
           != !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ(a_fBit, a_fBit1, a_fBit2) \
    if (   !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) \
        &&    !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
           == !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
#define IEM_MC_IF_CX_IS_NZ()                            if (pIemCpu->CTX_SUFF(pCtx)->cx != 0) {
#define IEM_MC_IF_ECX_IS_NZ()                           if (pIemCpu->CTX_SUFF(pCtx)->ecx != 0) {
#define IEM_MC_IF_RCX_IS_NZ()                           if (pIemCpu->CTX_SUFF(pCtx)->rcx != 0) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_CX_IS_NZ_AND_EFL_BIT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->cx != 0 \
            && (pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_ECX_IS_NZ_AND_EFL_BIT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->ecx != 0 \
            && (pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_RCX_IS_NZ_AND_EFL_BIT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->rcx != 0 \
            && (pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_CX_IS_NZ_AND_EFL_BIT_NOT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->cx != 0 \
            && !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_ECX_IS_NZ_AND_EFL_BIT_NOT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->ecx != 0 \
            && !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
/** @note Not for IOPL or IF testing. */
#define IEM_MC_IF_RCX_IS_NZ_AND_EFL_BIT_NOT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->rcx != 0 \
            && !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_LOCAL_IS_Z(a_Local)                   if ((a_Local) == 0) {
#define IEM_MC_IF_GREG_BIT_SET(a_iGReg, a_iBitNo)       if (*(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) & RT_BIT_64(a_iBitNo)) {
#define IEM_MC_IF_FPUREG_NOT_EMPTY(a_iSt) \
    if (iemFpuStRegNotEmpty(pIemCpu, (a_iSt)) == VINF_SUCCESS) {
#define IEM_MC_IF_FPUREG_IS_EMPTY(a_iSt) \
    if (iemFpuStRegNotEmpty(pIemCpu, (a_iSt)) != VINF_SUCCESS) {
#define IEM_MC_IF_FPUREG_NOT_EMPTY_REF_R80(a_pr80Dst, a_iSt) \
    if (iemFpuStRegNotEmptyRef(pIemCpu, (a_iSt), &(a_pr80Dst)) == VINF_SUCCESS) {
#define IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80(a_pr80Dst0, a_iSt0, a_pr80Dst1, a_iSt1) \
    if (iemFpu2StRegsNotEmptyRef(pIemCpu, (a_iSt0), &(a_pr80Dst0), (a_iSt1), &(a_pr80Dst1)) == VINF_SUCCESS) {
#define IEM_MC_IF_TWO_FPUREGS_NOT_EMPTY_REF_R80_FIRST(a_pr80Dst0, a_iSt0, a_iSt1) \
    if (iemFpu2StRegsNotEmptyRefFirst(pIemCpu, (a_iSt0), &(a_pr80Dst0), (a_iSt1)) == VINF_SUCCESS) {
#define IEM_MC_IF_FCW_IM() \
    if (pIemCpu->CTX_SUFF(pCtx)->CTX_SUFF(pXState)->x87.FCW & X86_FCW_IM) {

#define IEM_MC_ELSE()                                   } else {
#define IEM_MC_ENDIF()                                  } do {} while (0)

/** @}  */


/** @name   Opcode Debug Helpers.
 * @{
 */
#ifdef DEBUG
# define IEMOP_MNEMONIC(a_szMnemonic) \
    Log4(("decode - %04x:%RGv %s%s [#%u]\n", pIemCpu->CTX_SUFF(pCtx)->cs.Sel, pIemCpu->CTX_SUFF(pCtx)->rip, \
          pIemCpu->fPrefixes & IEM_OP_PRF_LOCK ? "lock " : "", a_szMnemonic, pIemCpu->cInstructions))
# define IEMOP_MNEMONIC2(a_szMnemonic, a_szOps) \
    Log4(("decode - %04x:%RGv %s%s %s [#%u]\n", pIemCpu->CTX_SUFF(pCtx)->cs.Sel, pIemCpu->CTX_SUFF(pCtx)->rip, \
          pIemCpu->fPrefixes & IEM_OP_PRF_LOCK ? "lock " : "", a_szMnemonic, a_szOps, pIemCpu->cInstructions))
#else
# define IEMOP_MNEMONIC(a_szMnemonic) do { } while (0)
# define IEMOP_MNEMONIC2(a_szMnemonic, a_szOps) do { } while (0)
#endif

/** @} */


/** @name   Opcode Helpers.
 * @{
 */

#ifdef IN_RING3
# define IEMOP_HLP_MIN_CPU(a_uMinCpu, a_fOnlyIf) \
    do { \
        if (IEM_GET_TARGET_CPU(pIemCpu) >= (a_uMinCpu) || !(a_fOnlyIf)) { } \
        else \
        { \
            DBGFSTOP(IEMCPU_TO_VM(pIemCpu)); \
            return IEMOP_RAISE_INVALID_OPCODE(); \
        } \
    } while (0)
#else
# define IEMOP_HLP_MIN_CPU(a_uMinCpu, a_fOnlyIf) \
    do { \
        if (IEM_GET_TARGET_CPU(pIemCpu) >= (a_uMinCpu) || !(a_fOnlyIf)) { } \
        else return IEMOP_RAISE_INVALID_OPCODE(); \
    } while (0)
#endif

/** The instruction requires a 186 or later. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_186
# define IEMOP_HLP_MIN_186() do { } while (0)
#else
# define IEMOP_HLP_MIN_186() IEMOP_HLP_MIN_CPU(IEMTARGETCPU_186, true)
#endif

/** The instruction requires a 286 or later. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_286
# define IEMOP_HLP_MIN_286() do { } while (0)
#else
# define IEMOP_HLP_MIN_286() IEMOP_HLP_MIN_CPU(IEMTARGETCPU_286, true)
#endif

/** The instruction requires a 386 or later. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_386
# define IEMOP_HLP_MIN_386() do { } while (0)
#else
# define IEMOP_HLP_MIN_386() IEMOP_HLP_MIN_CPU(IEMTARGETCPU_386, true)
#endif

/** The instruction requires a 386 or later if the given expression is true. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_386
# define IEMOP_HLP_MIN_386_EX(a_fOnlyIf) do { } while (0)
#else
# define IEMOP_HLP_MIN_386_EX(a_fOnlyIf) IEMOP_HLP_MIN_CPU(IEMTARGETCPU_386, a_fOnlyIf)
#endif

/** The instruction requires a 486 or later. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_486
# define IEMOP_HLP_MIN_486() do { } while (0)
#else
# define IEMOP_HLP_MIN_486() IEMOP_HLP_MIN_CPU(IEMTARGETCPU_486, true)
#endif

/** The instruction requires a Pentium (586) or later. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_586
# define IEMOP_HLP_MIN_586() do { } while (0)
#else
# define IEMOP_HLP_MIN_586() IEMOP_HLP_MIN_CPU(IEMTARGETCPU_586, true)
#endif

/** The instruction requires a PentiumPro (686) or later. */
#if IEM_CFG_TARGET_CPU >= IEMTARGETCPU_686
# define IEMOP_HLP_MIN_686() do { } while (0)
#else
# define IEMOP_HLP_MIN_686() IEMOP_HLP_MIN_CPU(IEMTARGETCPU_686, true)
#endif


/** The instruction raises an \#UD in real and V8086 mode. */
#define IEMOP_HLP_NO_REAL_OR_V86_MODE() \
    do \
    { \
        if (IEM_IS_REAL_OR_V86_MODE(pIemCpu)) \
            return IEMOP_RAISE_INVALID_LOCK_PREFIX(); \
    } while (0)

/** The instruction allows no lock prefixing (in this encoding), throw \#UD if
 * lock prefixed.
 * @deprecated  IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX */
#define IEMOP_HLP_NO_LOCK_PREFIX() \
    do \
    { \
        if (pIemCpu->fPrefixes & IEM_OP_PRF_LOCK) \
            return IEMOP_RAISE_INVALID_LOCK_PREFIX(); \
    } while (0)

/** The instruction is not available in 64-bit mode, throw \#UD if we're in
 * 64-bit mode. */
#define IEMOP_HLP_NO_64BIT() \
    do \
    { \
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT) \
            return IEMOP_RAISE_INVALID_OPCODE(); \
    } while (0)

/** The instruction is only available in 64-bit mode, throw \#UD if we're not in
 * 64-bit mode. */
#define IEMOP_HLP_ONLY_64BIT() \
    do \
    { \
        if (pIemCpu->enmCpuMode != IEMMODE_64BIT) \
            return IEMOP_RAISE_INVALID_OPCODE(); \
    } while (0)

/** The instruction defaults to 64-bit operand size if 64-bit mode. */
#define IEMOP_HLP_DEFAULT_64BIT_OP_SIZE() \
    do \
    { \
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT) \
            iemRecalEffOpSize64Default(pIemCpu); \
    } while (0)

/** The instruction has 64-bit operand size if 64-bit mode. */
#define IEMOP_HLP_64BIT_OP_SIZE() \
    do \
    { \
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT) \
            pIemCpu->enmEffOpSize = pIemCpu->enmDefOpSize = IEMMODE_64BIT; \
    } while (0)

/** Only a REX prefix immediately preceeding the first opcode byte takes
 * effect. This macro helps ensuring this as well as logging bad guest code.  */
#define IEMOP_HLP_CLEAR_REX_NOT_BEFORE_OPCODE(a_szPrf) \
    do \
    { \
        if (RT_UNLIKELY(pIemCpu->fPrefixes & IEM_OP_PRF_REX)) \
        { \
            Log5((a_szPrf ": Overriding REX prefix at %RX16! fPrefixes=%#x\n", \
                  pIemCpu->CTX_SUFF(pCtx)->rip, pIemCpu->fPrefixes)); \
            pIemCpu->fPrefixes &= ~IEM_OP_PRF_REX_MASK; \
            pIemCpu->uRexB     = 0; \
            pIemCpu->uRexIndex = 0; \
            pIemCpu->uRexReg   = 0; \
            iemRecalEffOpSize(pIemCpu); \
        } \
    } while (0)

/**
 * Done decoding.
 */
#define IEMOP_HLP_DONE_DECODING() \
    do \
    { \
        /*nothing for now, maybe later... */ \
    } while (0)

/**
 * Done decoding, raise \#UD exception if lock prefix present.
 */
#define IEMOP_HLP_DONE_DECODING_NO_LOCK_PREFIX() \
    do \
    { \
        if (RT_LIKELY(!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))) \
        { /* likely */ } \
        else \
            return IEMOP_RAISE_INVALID_LOCK_PREFIX(); \
    } while (0)
#define IEMOP_HLP_DECODED_NL_1(a_uDisOpNo, a_fIemOpFlags, a_uDisParam0, a_fDisOpType) \
    do \
    { \
        if (RT_LIKELY(!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))) \
        { /* likely */ } \
        else \
        { \
            NOREF(a_uDisOpNo); NOREF(a_fIemOpFlags); NOREF(a_uDisParam0); NOREF(a_fDisOpType); \
            return IEMOP_RAISE_INVALID_LOCK_PREFIX(); \
        } \
    } while (0)
#define IEMOP_HLP_DECODED_NL_2(a_uDisOpNo, a_fIemOpFlags, a_uDisParam0, a_uDisParam1, a_fDisOpType) \
    do \
    { \
        if (RT_LIKELY(!(pIemCpu->fPrefixes & IEM_OP_PRF_LOCK))) \
        { /* likely */ } \
        else \
        { \
            NOREF(a_uDisOpNo); NOREF(a_fIemOpFlags); NOREF(a_uDisParam0); NOREF(a_uDisParam1); NOREF(a_fDisOpType); \
            return IEMOP_RAISE_INVALID_LOCK_PREFIX(); \
        } \
    } while (0)
/**
 * Done decoding, raise \#UD exception if any lock, repz or repnz prefixes
 * are present.
 */
#define IEMOP_HLP_DONE_DECODING_NO_LOCK_REPZ_OR_REPNZ_PREFIXES() \
    do \
    { \
        if (RT_LIKELY(!(pIemCpu->fPrefixes & (IEM_OP_PRF_LOCK | IEM_OP_PRF_REPNZ | IEM_OP_PRF_REPZ)))) \
        { /* likely */ } \
        else \
            return IEMOP_RAISE_INVALID_OPCODE(); \
    } while (0)


/**
 * Calculates the effective address of a ModR/M memory operand.
 *
 * Meant to be used via IEM_MC_CALC_RM_EFF_ADDR.
 *
 * @return  Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   bRm                 The ModRM byte.
 * @param   cbImm               The size of any immediate following the
 *                              effective address opcode bytes. Important for
 *                              RIP relative addressing.
 * @param   pGCPtrEff           Where to return the effective address.
 */
IEM_STATIC VBOXSTRICTRC iemOpHlpCalcRmEffAddr(PIEMCPU pIemCpu, uint8_t bRm, uint8_t cbImm, PRTGCPTR pGCPtrEff)
{
    Log5(("iemOpHlpCalcRmEffAddr: bRm=%#x\n", bRm));
    PCCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
#define SET_SS_DEF() \
    do \
    { \
        if (!(pIemCpu->fPrefixes & IEM_OP_PRF_SEG_MASK)) \
            pIemCpu->iEffSeg = X86_SREG_SS; \
    } while (0)

    if (pIemCpu->enmCpuMode != IEMMODE_64BIT)
    {
/** @todo Check the effective address size crap! */
        if (pIemCpu->enmEffAddrMode == IEMMODE_16BIT)
        {
            uint16_t u16EffAddr;

            /* Handle the disp16 form with no registers first. */
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 6)
                IEM_OPCODE_GET_NEXT_U16(&u16EffAddr);
            else
            {
                /* Get the displacment. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:  u16EffAddr = 0;                             break;
                    case 1:  IEM_OPCODE_GET_NEXT_S8_SX_U16(&u16EffAddr); break;
                    case 2:  IEM_OPCODE_GET_NEXT_U16(&u16EffAddr);       break;
                    default: AssertFailedReturn(VERR_IEM_IPE_1); /* (caller checked for these) */
                }

                /* Add the base and index registers to the disp. */
                switch (bRm & X86_MODRM_RM_MASK)
                {
                    case 0: u16EffAddr += pCtx->bx + pCtx->si; break;
                    case 1: u16EffAddr += pCtx->bx + pCtx->di; break;
                    case 2: u16EffAddr += pCtx->bp + pCtx->si; SET_SS_DEF(); break;
                    case 3: u16EffAddr += pCtx->bp + pCtx->di; SET_SS_DEF(); break;
                    case 4: u16EffAddr += pCtx->si;            break;
                    case 5: u16EffAddr += pCtx->di;            break;
                    case 6: u16EffAddr += pCtx->bp;            SET_SS_DEF(); break;
                    case 7: u16EffAddr += pCtx->bx;            break;
                }
            }

            *pGCPtrEff = u16EffAddr;
        }
        else
        {
            Assert(pIemCpu->enmEffAddrMode == IEMMODE_32BIT);
            uint32_t u32EffAddr;

            /* Handle the disp32 form with no registers first. */
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
                IEM_OPCODE_GET_NEXT_U32(&u32EffAddr);
            else
            {
                /* Get the register (or SIB) value. */
                switch ((bRm & X86_MODRM_RM_MASK))
                {
                    case 0: u32EffAddr = pCtx->eax; break;
                    case 1: u32EffAddr = pCtx->ecx; break;
                    case 2: u32EffAddr = pCtx->edx; break;
                    case 3: u32EffAddr = pCtx->ebx; break;
                    case 4: /* SIB */
                    {
                        uint8_t bSib; IEM_OPCODE_GET_NEXT_U8(&bSib);

                        /* Get the index and scale it. */
                        switch ((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK)
                        {
                            case 0: u32EffAddr = pCtx->eax; break;
                            case 1: u32EffAddr = pCtx->ecx; break;
                            case 2: u32EffAddr = pCtx->edx; break;
                            case 3: u32EffAddr = pCtx->ebx; break;
                            case 4: u32EffAddr = 0; /*none */ break;
                            case 5: u32EffAddr = pCtx->ebp; break;
                            case 6: u32EffAddr = pCtx->esi; break;
                            case 7: u32EffAddr = pCtx->edi; break;
                            IEM_NOT_REACHED_DEFAULT_CASE_RET();
                        }
                        u32EffAddr <<= (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                        /* add base */
                        switch (bSib & X86_SIB_BASE_MASK)
                        {
                            case 0: u32EffAddr += pCtx->eax; break;
                            case 1: u32EffAddr += pCtx->ecx; break;
                            case 2: u32EffAddr += pCtx->edx; break;
                            case 3: u32EffAddr += pCtx->ebx; break;
                            case 4: u32EffAddr += pCtx->esp; SET_SS_DEF(); break;
                            case 5:
                                if ((bRm & X86_MODRM_MOD_MASK) != 0)
                                {
                                    u32EffAddr += pCtx->ebp;
                                    SET_SS_DEF();
                                }
                                else
                                {
                                    uint32_t u32Disp;
                                    IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                                    u32EffAddr += u32Disp;
                                }
                                break;
                            case 6: u32EffAddr += pCtx->esi; break;
                            case 7: u32EffAddr += pCtx->edi; break;
                            IEM_NOT_REACHED_DEFAULT_CASE_RET();
                        }
                        break;
                    }
                    case 5: u32EffAddr = pCtx->ebp; SET_SS_DEF(); break;
                    case 6: u32EffAddr = pCtx->esi; break;
                    case 7: u32EffAddr = pCtx->edi; break;
                    IEM_NOT_REACHED_DEFAULT_CASE_RET();
                }

                /* Get and add the displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:
                        break;
                    case 1:
                    {
                        int8_t i8Disp; IEM_OPCODE_GET_NEXT_S8(&i8Disp);
                        u32EffAddr += i8Disp;
                        break;
                    }
                    case 2:
                    {
                        uint32_t u32Disp; IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                        u32EffAddr += u32Disp;
                        break;
                    }
                    default:
                        AssertFailedReturn(VERR_IEM_IPE_2); /* (caller checked for these) */
                }

            }
            if (pIemCpu->enmEffAddrMode == IEMMODE_32BIT)
                *pGCPtrEff = u32EffAddr;
            else
            {
                Assert(pIemCpu->enmEffAddrMode == IEMMODE_16BIT);
                *pGCPtrEff = u32EffAddr & UINT16_MAX;
            }
        }
    }
    else
    {
        uint64_t u64EffAddr;

        /* Handle the rip+disp32 form with no registers first. */
        if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
        {
            IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64EffAddr);
            u64EffAddr += pCtx->rip + pIemCpu->offOpcode + cbImm;
        }
        else
        {
            /* Get the register (or SIB) value. */
            switch ((bRm & X86_MODRM_RM_MASK) | pIemCpu->uRexB)
            {
                case  0: u64EffAddr = pCtx->rax; break;
                case  1: u64EffAddr = pCtx->rcx; break;
                case  2: u64EffAddr = pCtx->rdx; break;
                case  3: u64EffAddr = pCtx->rbx; break;
                case  5: u64EffAddr = pCtx->rbp; SET_SS_DEF(); break;
                case  6: u64EffAddr = pCtx->rsi; break;
                case  7: u64EffAddr = pCtx->rdi; break;
                case  8: u64EffAddr = pCtx->r8;  break;
                case  9: u64EffAddr = pCtx->r9;  break;
                case 10: u64EffAddr = pCtx->r10; break;
                case 11: u64EffAddr = pCtx->r11; break;
                case 13: u64EffAddr = pCtx->r13; break;
                case 14: u64EffAddr = pCtx->r14; break;
                case 15: u64EffAddr = pCtx->r15; break;
                /* SIB */
                case 4:
                case 12:
                {
                    uint8_t bSib; IEM_OPCODE_GET_NEXT_U8(&bSib);

                    /* Get the index and scale it. */
                    switch (((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK) | pIemCpu->uRexIndex)
                    {
                        case  0: u64EffAddr = pCtx->rax; break;
                        case  1: u64EffAddr = pCtx->rcx; break;
                        case  2: u64EffAddr = pCtx->rdx; break;
                        case  3: u64EffAddr = pCtx->rbx; break;
                        case  4: u64EffAddr = 0; /*none */ break;
                        case  5: u64EffAddr = pCtx->rbp; break;
                        case  6: u64EffAddr = pCtx->rsi; break;
                        case  7: u64EffAddr = pCtx->rdi; break;
                        case  8: u64EffAddr = pCtx->r8;  break;
                        case  9: u64EffAddr = pCtx->r9;  break;
                        case 10: u64EffAddr = pCtx->r10; break;
                        case 11: u64EffAddr = pCtx->r11; break;
                        case 12: u64EffAddr = pCtx->r12; break;
                        case 13: u64EffAddr = pCtx->r13; break;
                        case 14: u64EffAddr = pCtx->r14; break;
                        case 15: u64EffAddr = pCtx->r15; break;
                        IEM_NOT_REACHED_DEFAULT_CASE_RET();
                    }
                    u64EffAddr <<= (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* add base */
                    switch ((bSib & X86_SIB_BASE_MASK) | pIemCpu->uRexB)
                    {
                        case  0: u64EffAddr += pCtx->rax; break;
                        case  1: u64EffAddr += pCtx->rcx; break;
                        case  2: u64EffAddr += pCtx->rdx; break;
                        case  3: u64EffAddr += pCtx->rbx; break;
                        case  4: u64EffAddr += pCtx->rsp; SET_SS_DEF(); break;
                        case  6: u64EffAddr += pCtx->rsi; break;
                        case  7: u64EffAddr += pCtx->rdi; break;
                        case  8: u64EffAddr += pCtx->r8;  break;
                        case  9: u64EffAddr += pCtx->r9;  break;
                        case 10: u64EffAddr += pCtx->r10; break;
                        case 11: u64EffAddr += pCtx->r11; break;
                        case 12: u64EffAddr += pCtx->r12; break;
                        case 14: u64EffAddr += pCtx->r14; break;
                        case 15: u64EffAddr += pCtx->r15; break;
                        /* complicated encodings */
                        case 5:
                        case 13:
                            if ((bRm & X86_MODRM_MOD_MASK) != 0)
                            {
                                if (!pIemCpu->uRexB)
                                {
                                    u64EffAddr += pCtx->rbp;
                                    SET_SS_DEF();
                                }
                                else
                                    u64EffAddr += pCtx->r13;
                            }
                            else
                            {
                                uint32_t u32Disp;
                                IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                                u64EffAddr += (int32_t)u32Disp;
                            }
                            break;
                        IEM_NOT_REACHED_DEFAULT_CASE_RET();
                    }
                    break;
                }
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }

            /* Get and add the displacement. */
            switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
            {
                case 0:
                    break;
                case 1:
                {
                    int8_t i8Disp;
                    IEM_OPCODE_GET_NEXT_S8(&i8Disp);
                    u64EffAddr += i8Disp;
                    break;
                }
                case 2:
                {
                    uint32_t u32Disp;
                    IEM_OPCODE_GET_NEXT_U32(&u32Disp);
                    u64EffAddr += (int32_t)u32Disp;
                    break;
                }
                IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* (caller checked for these) */
            }

        }

        if (pIemCpu->enmEffAddrMode == IEMMODE_64BIT)
            *pGCPtrEff = u64EffAddr;
        else
        {
            Assert(pIemCpu->enmEffAddrMode == IEMMODE_32BIT);
            *pGCPtrEff = u64EffAddr & UINT32_MAX;
        }
    }

    Log5(("iemOpHlpCalcRmEffAddr: EffAddr=%#010RGv\n", *pGCPtrEff));
    return VINF_SUCCESS;
}

/** @}  */



/*
 * Include the instructions
 */
#include "IEMAllInstructions.cpp.h"




#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)

/**
 * Sets up execution verification mode.
 */
IEM_STATIC void iemExecVerificationModeSetup(PIEMCPU pIemCpu)
{
    PVMCPU   pVCpu   = IEMCPU_TO_VMCPU(pIemCpu);
    PCPUMCTX pOrgCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Always note down the address of the current instruction.
     */
    pIemCpu->uOldCs  = pOrgCtx->cs.Sel;
    pIemCpu->uOldRip = pOrgCtx->rip;

    /*
     * Enable verification and/or logging.
     */
    bool fNewNoRem = !LogIs6Enabled(); /* logging triggers the no-rem/rem verification stuff */;
    if (    fNewNoRem
        && (   0
#if 0 /* auto enable on first paged protected mode interrupt */
            || (   pOrgCtx->eflags.Bits.u1IF
                && (pOrgCtx->cr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG)
                && TRPMHasTrap(pVCpu)
                && EMGetInhibitInterruptsPC(pVCpu) != pOrgCtx->rip) )
#endif
#if 0
            || (   pOrgCtx->cs  == 0x10
                && (   pOrgCtx->rip == 0x90119e3e
                    || pOrgCtx->rip == 0x901d9810)
#endif
#if 0 /* Auto enable DSL - FPU stuff. */
            || (   pOrgCtx->cs  == 0x10
                && (//   pOrgCtx->rip == 0xc02ec07f
                    //|| pOrgCtx->rip == 0xc02ec082
                    //|| pOrgCtx->rip == 0xc02ec0c9
                       0
                    || pOrgCtx->rip == 0x0c010e7c4   /* fxsave */ ) )
#endif
#if 0 /* Auto enable DSL - fstp st0 stuff. */
            || (pOrgCtx->cs.Sel  == 0x23  pOrgCtx->rip == 0x804aff7)
#endif
#if 0
            || pOrgCtx->rip == 0x9022bb3a
#endif
#if 0
            || (pOrgCtx->cs.Sel == 0x58 && pOrgCtx->rip == 0x3be) /* NT4SP1 sidt/sgdt in early loader code */
#endif
#if 0
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8013ec28) /* NT4SP1 first str (early boot) */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x80119e3f) /* NT4SP1 second str (early boot) */
#endif
#if 0 /* NT4SP1 - later on the blue screen, things goes wrong... */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8010a5df)
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8013a7c4)
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8013a7d2)
#endif
#if 0 /* NT4SP1 - xadd early boot. */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8019cf0f)
#endif
#if 0 /* NT4SP1 - wrmsr (intel MSR). */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8011a6d4)
#endif
#if 0 /* NT4SP1 - cmpxchg (AMD). */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x801684c1)
#endif
#if 0 /* NT4SP1 - fnstsw + 2 (AMD). */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x801c6b88+2)
#endif
#if 0 /* NT4SP1 - iret to v8086 -- too generic a place? (N/A with GAs installed) */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8013bd5d)

#endif
#if 0 /* NT4SP1 - iret to v8086 (executing edlin) */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8013b609)

#endif
#if 0 /* NT4SP1 - frstor [ecx] */
            || (pOrgCtx->cs.Sel == 8 && pOrgCtx->rip == 0x8013d11f)
#endif
#if 0 /* xxxxxx - All long mode code. */
            || (pOrgCtx->msrEFER & MSR_K6_EFER_LMA)
#endif
#if 0 /* rep movsq linux 3.7 64-bit boot. */
            || (pOrgCtx->rip == 0x0000000000100241)
#endif
#if 0 /* linux 3.7 64-bit boot - '000000000215e240'. */
            || (pOrgCtx->rip == 0x000000000215e240)
#endif
#if 0 /* DOS's size-overridden iret to v8086. */
            || (pOrgCtx->rip == 0x427 && pOrgCtx->cs.Sel == 0xb8)
#endif
           )
       )
    {
        RTLogGroupSettings(NULL, "iem.eo.l6.l2");
        RTLogFlags(NULL, "enabled");
        fNewNoRem = false;
    }
    if (fNewNoRem != pIemCpu->fNoRem)
    {
        pIemCpu->fNoRem = fNewNoRem;
        if (!fNewNoRem)
        {
            LogAlways(("Enabling verification mode!\n"));
            CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_ALL);
        }
        else
            LogAlways(("Disabling verification mode!\n"));
    }

    /*
     * Switch state.
     */
    if (IEM_VERIFICATION_ENABLED(pIemCpu))
    {
        static CPUMCTX  s_DebugCtx; /* Ugly! */

        s_DebugCtx = *pOrgCtx;
        pIemCpu->CTX_SUFF(pCtx) = &s_DebugCtx;
    }

    /*
     * See if there is an interrupt pending in TRPM and inject it if we can.
     */
    pIemCpu->uInjectCpl = UINT8_MAX;
    if (   pOrgCtx->eflags.Bits.u1IF
        && TRPMHasTrap(pVCpu)
        && EMGetInhibitInterruptsPC(pVCpu) != pOrgCtx->rip)
    {
        uint8_t     u8TrapNo;
        TRPMEVENT   enmType;
        RTGCUINT    uErrCode;
        RTGCPTR     uCr2;
        int rc2 = TRPMQueryTrapAll(pVCpu, &u8TrapNo, &enmType, &uErrCode, &uCr2, NULL /* pu8InstLen */); AssertRC(rc2);
        IEMInjectTrap(pVCpu, u8TrapNo, enmType, (uint16_t)uErrCode, uCr2, 0 /* cbInstr */);
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            TRPMResetTrap(pVCpu);
        pIemCpu->uInjectCpl = pIemCpu->uCpl;
    }

    /*
     * Reset the counters.
     */
    pIemCpu->cIOReads    = 0;
    pIemCpu->cIOWrites   = 0;
    pIemCpu->fIgnoreRaxRdx = false;
    pIemCpu->fOverlappingMovs = false;
    pIemCpu->fProblematicMemory = false;
    pIemCpu->fUndefinedEFlags = 0;

    if (IEM_VERIFICATION_ENABLED(pIemCpu))
    {
        /*
         * Free all verification records.
         */
        PIEMVERIFYEVTREC pEvtRec = pIemCpu->pIemEvtRecHead;
        pIemCpu->pIemEvtRecHead = NULL;
        pIemCpu->ppIemEvtRecNext = &pIemCpu->pIemEvtRecHead;
        do
        {
            while (pEvtRec)
            {
                PIEMVERIFYEVTREC pNext = pEvtRec->pNext;
                pEvtRec->pNext = pIemCpu->pFreeEvtRec;
                pIemCpu->pFreeEvtRec = pEvtRec;
                pEvtRec = pNext;
            }
            pEvtRec = pIemCpu->pOtherEvtRecHead;
            pIemCpu->pOtherEvtRecHead = NULL;
            pIemCpu->ppOtherEvtRecNext = &pIemCpu->pOtherEvtRecHead;
        } while (pEvtRec);
    }
}


/**
 * Allocate an event record.
 * @returns Pointer to a record.
 */
IEM_STATIC PIEMVERIFYEVTREC iemVerifyAllocRecord(PIEMCPU pIemCpu)
{
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        return NULL;

    PIEMVERIFYEVTREC pEvtRec = pIemCpu->pFreeEvtRec;
    if (pEvtRec)
        pIemCpu->pFreeEvtRec = pEvtRec->pNext;
    else
    {
        if (!pIemCpu->ppIemEvtRecNext)
            return NULL; /* Too early (fake PCIBIOS), ignore notification. */

        pEvtRec = (PIEMVERIFYEVTREC)MMR3HeapAlloc(IEMCPU_TO_VM(pIemCpu), MM_TAG_EM /* lazy bird*/, sizeof(*pEvtRec));
        if (!pEvtRec)
            return NULL;
    }
    pEvtRec->enmEvent = IEMVERIFYEVENT_INVALID;
    pEvtRec->pNext    = NULL;
    return pEvtRec;
}


/**
 * IOMMMIORead notification.
 */
VMM_INT_DECL(void)   IEMNotifyMMIORead(PVM pVM, RTGCPHYS GCPhys, size_t cbValue)
{
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_READ;
    pEvtRec->u.RamRead.GCPhys  = GCPhys;
    pEvtRec->u.RamRead.cb      = (uint32_t)cbValue;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}


/**
 * IOMMMIOWrite notification.
 */
VMM_INT_DECL(void)   IEMNotifyMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue)
{
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_WRITE;
    pEvtRec->u.RamWrite.GCPhys   = GCPhys;
    pEvtRec->u.RamWrite.cb       = (uint32_t)cbValue;
    pEvtRec->u.RamWrite.ab[0]    = RT_BYTE1(u32Value);
    pEvtRec->u.RamWrite.ab[1]    = RT_BYTE2(u32Value);
    pEvtRec->u.RamWrite.ab[2]    = RT_BYTE3(u32Value);
    pEvtRec->u.RamWrite.ab[3]    = RT_BYTE4(u32Value);
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}


/**
 * IOMIOPortRead notification.
 */
VMM_INT_DECL(void)   IEMNotifyIOPortRead(PVM pVM, RTIOPORT Port, size_t cbValue)
{
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_READ;
    pEvtRec->u.IOPortRead.Port    = Port;
    pEvtRec->u.IOPortRead.cbValue = (uint8_t)cbValue;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}

/**
 * IOMIOPortWrite notification.
 */
VMM_INT_DECL(void)   IEMNotifyIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_WRITE;
    pEvtRec->u.IOPortWrite.Port     = Port;
    pEvtRec->u.IOPortWrite.cbValue  = (uint8_t)cbValue;
    pEvtRec->u.IOPortWrite.u32Value = u32Value;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}


VMM_INT_DECL(void)   IEMNotifyIOPortReadString(PVM pVM, RTIOPORT Port, void *pvDst, RTGCUINTREG cTransfers, size_t cbValue)
{
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_STR_READ;
    pEvtRec->u.IOPortStrRead.Port       = Port;
    pEvtRec->u.IOPortStrRead.cbValue    = (uint8_t)cbValue;
    pEvtRec->u.IOPortStrRead.cTransfers = cTransfers;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}


VMM_INT_DECL(void)   IEMNotifyIOPortWriteString(PVM pVM, RTIOPORT Port, void const *pvSrc, RTGCUINTREG cTransfers, size_t cbValue)
{
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_STR_WRITE;
    pEvtRec->u.IOPortStrWrite.Port       = Port;
    pEvtRec->u.IOPortStrWrite.cbValue    = (uint8_t)cbValue;
    pEvtRec->u.IOPortStrWrite.cTransfers = cTransfers;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}


/**
 * Fakes and records an I/O port read.
 *
 * @returns VINF_SUCCESS.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   Port                The I/O port.
 * @param   pu32Value           Where to store the fake value.
 * @param   cbValue             The size of the access.
 */
IEM_STATIC VBOXSTRICTRC iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue)
{
    PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (pEvtRec)
    {
        pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_READ;
        pEvtRec->u.IOPortRead.Port    = Port;
        pEvtRec->u.IOPortRead.cbValue = (uint8_t)cbValue;
        pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
        *pIemCpu->ppIemEvtRecNext = pEvtRec;
    }
    pIemCpu->cIOReads++;
    *pu32Value = 0xcccccccc;
    return VINF_SUCCESS;
}


/**
 * Fakes and records an I/O port write.
 *
 * @returns VINF_SUCCESS.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   Port                The I/O port.
 * @param   u32Value            The value being written.
 * @param   cbValue             The size of the access.
 */
IEM_STATIC VBOXSTRICTRC iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
    PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (pEvtRec)
    {
        pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_WRITE;
        pEvtRec->u.IOPortWrite.Port     = Port;
        pEvtRec->u.IOPortWrite.cbValue  = (uint8_t)cbValue;
        pEvtRec->u.IOPortWrite.u32Value = u32Value;
        pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
        *pIemCpu->ppIemEvtRecNext = pEvtRec;
    }
    pIemCpu->cIOWrites++;
    return VINF_SUCCESS;
}


/**
 * Used to add extra details about a stub case.
 * @param   pIemCpu     The IEM per CPU state.
 */
IEM_STATIC void iemVerifyAssertMsg2(PIEMCPU pIemCpu)
{
    PCPUMCTX pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVM      pVM   = IEMCPU_TO_VM(pIemCpu);
    PVMCPU   pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    char szRegs[4096];
    DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                    "rax=%016VR{rax} rbx=%016VR{rbx} rcx=%016VR{rcx} rdx=%016VR{rdx}\n"
                    "rsi=%016VR{rsi} rdi=%016VR{rdi} r8 =%016VR{r8} r9 =%016VR{r9}\n"
                    "r10=%016VR{r10} r11=%016VR{r11} r12=%016VR{r12} r13=%016VR{r13}\n"
                    "r14=%016VR{r14} r15=%016VR{r15} %VRF{rflags}\n"
                    "rip=%016VR{rip} rsp=%016VR{rsp} rbp=%016VR{rbp}\n"
                    "cs={%04VR{cs} base=%016VR{cs_base} limit=%08VR{cs_lim} flags=%04VR{cs_attr}} cr0=%016VR{cr0}\n"
                    "ds={%04VR{ds} base=%016VR{ds_base} limit=%08VR{ds_lim} flags=%04VR{ds_attr}} cr2=%016VR{cr2}\n"
                    "es={%04VR{es} base=%016VR{es_base} limit=%08VR{es_lim} flags=%04VR{es_attr}} cr3=%016VR{cr3}\n"
                    "fs={%04VR{fs} base=%016VR{fs_base} limit=%08VR{fs_lim} flags=%04VR{fs_attr}} cr4=%016VR{cr4}\n"
                    "gs={%04VR{gs} base=%016VR{gs_base} limit=%08VR{gs_lim} flags=%04VR{gs_attr}} cr8=%016VR{cr8}\n"
                    "ss={%04VR{ss} base=%016VR{ss_base} limit=%08VR{ss_lim} flags=%04VR{ss_attr}}\n"
                    "dr0=%016VR{dr0} dr1=%016VR{dr1} dr2=%016VR{dr2} dr3=%016VR{dr3}\n"
                    "dr6=%016VR{dr6} dr7=%016VR{dr7}\n"
                    "gdtr=%016VR{gdtr_base}:%04VR{gdtr_lim}  idtr=%016VR{idtr_base}:%04VR{idtr_lim}  rflags=%08VR{rflags}\n"
                    "ldtr={%04VR{ldtr} base=%016VR{ldtr_base} limit=%08VR{ldtr_lim} flags=%08VR{ldtr_attr}}\n"
                    "tr  ={%04VR{tr} base=%016VR{tr_base} limit=%08VR{tr_lim} flags=%08VR{tr_attr}}\n"
                    "    sysenter={cs=%04VR{sysenter_cs} eip=%08VR{sysenter_eip} esp=%08VR{sysenter_esp}}\n"
                    "        efer=%016VR{efer}\n"
                    "         pat=%016VR{pat}\n"
                    "     sf_mask=%016VR{sf_mask}\n"
                    "krnl_gs_base=%016VR{krnl_gs_base}\n"
                    "       lstar=%016VR{lstar}\n"
                    "        star=%016VR{star} cstar=%016VR{cstar}\n"
                    "fcw=%04VR{fcw} fsw=%04VR{fsw} ftw=%04VR{ftw} mxcsr=%04VR{mxcsr} mxcsr_mask=%04VR{mxcsr_mask}\n"
                    );

    char szInstr1[256];
    DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, pIemCpu->uOldCs, pIemCpu->uOldRip,
                       DBGF_DISAS_FLAGS_DEFAULT_MODE,
                       szInstr1, sizeof(szInstr1), NULL);
    char szInstr2[256];
    DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                       DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                       szInstr2, sizeof(szInstr2), NULL);

    RTAssertMsg2Weak("%s%s\n%s\n", szRegs, szInstr1, szInstr2);
}


/**
 * Used by iemVerifyAssertRecord and iemVerifyAssertRecords to add a record
 * dump to the assertion info.
 *
 * @param   pEvtRec         The record to dump.
 */
IEM_STATIC void iemVerifyAssertAddRecordDump(PIEMVERIFYEVTREC pEvtRec)
{
    switch (pEvtRec->enmEvent)
    {
        case IEMVERIFYEVENT_IOPORT_READ:
            RTAssertMsg2Add("I/O PORT READ from %#6x, %d bytes\n",
                            pEvtRec->u.IOPortWrite.Port,
                            pEvtRec->u.IOPortWrite.cbValue);
            break;
        case IEMVERIFYEVENT_IOPORT_WRITE:
            RTAssertMsg2Add("I/O PORT WRITE  to %#6x, %d bytes, value %#x\n",
                            pEvtRec->u.IOPortWrite.Port,
                            pEvtRec->u.IOPortWrite.cbValue,
                            pEvtRec->u.IOPortWrite.u32Value);
            break;
        case IEMVERIFYEVENT_IOPORT_STR_READ:
            RTAssertMsg2Add("I/O PORT STRING READ from %#6x, %d bytes, %#x times\n",
                            pEvtRec->u.IOPortStrWrite.Port,
                            pEvtRec->u.IOPortStrWrite.cbValue,
                            pEvtRec->u.IOPortStrWrite.cTransfers);
            break;
        case IEMVERIFYEVENT_IOPORT_STR_WRITE:
            RTAssertMsg2Add("I/O PORT STRING WRITE  to %#6x, %d bytes, %#x times\n",
                            pEvtRec->u.IOPortStrWrite.Port,
                            pEvtRec->u.IOPortStrWrite.cbValue,
                            pEvtRec->u.IOPortStrWrite.cTransfers);
            break;
        case IEMVERIFYEVENT_RAM_READ:
            RTAssertMsg2Add("RAM READ  at %RGp, %#4zx bytes\n",
                            pEvtRec->u.RamRead.GCPhys,
                            pEvtRec->u.RamRead.cb);
            break;
        case IEMVERIFYEVENT_RAM_WRITE:
            RTAssertMsg2Add("RAM WRITE at %RGp, %#4zx bytes: %.*Rhxs\n",
                            pEvtRec->u.RamWrite.GCPhys,
                            pEvtRec->u.RamWrite.cb,
                            (int)pEvtRec->u.RamWrite.cb,
                            pEvtRec->u.RamWrite.ab);
            break;
        default:
            AssertMsgFailed(("Invalid event type %d\n", pEvtRec->enmEvent));
            break;
    }
}


/**
 * Raises an assertion on the specified record, showing the given message with
 * a record dump attached.
 *
 * @param   pIemCpu         The IEM per CPU data.
 * @param   pEvtRec1        The first record.
 * @param   pEvtRec2        The second record.
 * @param   pszMsg          The message explaining why we're asserting.
 */
IEM_STATIC void iemVerifyAssertRecords(PIEMCPU pIemCpu, PIEMVERIFYEVTREC pEvtRec1, PIEMVERIFYEVTREC pEvtRec2, const char *pszMsg)
{
    RTAssertMsg1(pszMsg, __LINE__, __FILE__, __PRETTY_FUNCTION__);
    iemVerifyAssertAddRecordDump(pEvtRec1);
    iemVerifyAssertAddRecordDump(pEvtRec2);
    iemVerifyAssertMsg2(pIemCpu);
    RTAssertPanic();
}


/**
 * Raises an assertion on the specified record, showing the given message with
 * a record dump attached.
 *
 * @param   pIemCpu         The IEM per CPU data.
 * @param   pEvtRec1        The first record.
 * @param   pszMsg          The message explaining why we're asserting.
 */
IEM_STATIC void iemVerifyAssertRecord(PIEMCPU pIemCpu, PIEMVERIFYEVTREC pEvtRec, const char *pszMsg)
{
    RTAssertMsg1(pszMsg, __LINE__, __FILE__, __PRETTY_FUNCTION__);
    iemVerifyAssertAddRecordDump(pEvtRec);
    iemVerifyAssertMsg2(pIemCpu);
    RTAssertPanic();
}


/**
 * Verifies a write record.
 *
 * @param   pIemCpu         The IEM per CPU data.
 * @param   pEvtRec         The write record.
 * @param   fRem            Set if REM was doing the other executing. If clear
 *                          it was HM.
 */
IEM_STATIC void iemVerifyWriteRecord(PIEMCPU pIemCpu, PIEMVERIFYEVTREC pEvtRec, bool fRem)
{
    uint8_t abBuf[sizeof(pEvtRec->u.RamWrite.ab)]; RT_ZERO(abBuf);
    Assert(sizeof(abBuf) >= pEvtRec->u.RamWrite.cb);
    int rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), abBuf, pEvtRec->u.RamWrite.GCPhys, pEvtRec->u.RamWrite.cb);
    if (   RT_FAILURE(rc)
        || memcmp(abBuf, pEvtRec->u.RamWrite.ab, pEvtRec->u.RamWrite.cb) )
    {
        /* fend off ins */
        if (   !pIemCpu->cIOReads
            || pEvtRec->u.RamWrite.ab[0] != 0xcc
            || (   pEvtRec->u.RamWrite.cb != 1
                && pEvtRec->u.RamWrite.cb != 2
                && pEvtRec->u.RamWrite.cb != 4) )
        {
            /* fend off ROMs and MMIO */
            if (   pEvtRec->u.RamWrite.GCPhys - UINT32_C(0x000a0000) > UINT32_C(0x60000)
                && pEvtRec->u.RamWrite.GCPhys - UINT32_C(0xfffc0000) > UINT32_C(0x40000) )
            {
                /* fend off fxsave */
                if (pEvtRec->u.RamWrite.cb != 512)
                {
                    const char *pszWho = fRem ? "rem" : HMR3IsVmxEnabled(IEMCPU_TO_VM(pIemCpu)->pUVM) ? "vmx" : "svm";
                    RTAssertMsg1(NULL, __LINE__, __FILE__, __PRETTY_FUNCTION__);
                    RTAssertMsg2Weak("Memory at %RGv differs\n", pEvtRec->u.RamWrite.GCPhys);
                    RTAssertMsg2Add("%s: %.*Rhxs\n"
                                    "iem: %.*Rhxs\n",
                                    pszWho, pEvtRec->u.RamWrite.cb, abBuf,
                                    pEvtRec->u.RamWrite.cb, pEvtRec->u.RamWrite.ab);
                    iemVerifyAssertAddRecordDump(pEvtRec);
                    iemVerifyAssertMsg2(pIemCpu);
                    RTAssertPanic();
                }
            }
        }
    }

}

/**
 * Performs the post-execution verfication checks.
 */
IEM_STATIC void iemExecVerificationModeCheck(PIEMCPU pIemCpu)
{
    if (!IEM_VERIFICATION_ENABLED(pIemCpu))
        return;

    /*
     * Switch back the state.
     */
    PCPUMCTX    pOrgCtx   = CPUMQueryGuestCtxPtr(IEMCPU_TO_VMCPU(pIemCpu));
    PCPUMCTX    pDebugCtx = pIemCpu->CTX_SUFF(pCtx);
    Assert(pOrgCtx != pDebugCtx);
    pIemCpu->CTX_SUFF(pCtx) = pOrgCtx;

    /*
     * Execute the instruction in REM.
     */
    bool   fRem  = false;
    PVM    pVM   = IEMCPU_TO_VM(pIemCpu);
    PVMCPU pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    VBOXSTRICTRC rc = VERR_EM_CANNOT_EXEC_GUEST;
#ifdef IEM_VERIFICATION_MODE_FULL_HM
    if (   HMIsEnabled(pVM)
        && pIemCpu->cIOReads == 0
        && pIemCpu->cIOWrites == 0
        && !pIemCpu->fProblematicMemory)
    {
        uint64_t uStartRip = pOrgCtx->rip;
        unsigned iLoops = 0;
        do
        {
            rc = EMR3HmSingleInstruction(pVM, pVCpu, EM_ONE_INS_FLAGS_RIP_CHANGE);
            iLoops++;
        } while (   rc == VINF_SUCCESS
                 || (   rc == VINF_EM_DBG_STEPPED
                     && VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
                     && EMGetInhibitInterruptsPC(pVCpu) == pOrgCtx->rip)
                 || (   pOrgCtx->rip != pDebugCtx->rip
                     && pIemCpu->uInjectCpl != UINT8_MAX
                     && iLoops < 8) );
        if (rc == VINF_EM_RESCHEDULE && pOrgCtx->rip != uStartRip)
            rc = VINF_SUCCESS;
    }
#endif
    if (   rc == VERR_EM_CANNOT_EXEC_GUEST
        || rc == VINF_IOM_R3_IOPORT_READ
        || rc == VINF_IOM_R3_IOPORT_WRITE
        || rc == VINF_IOM_R3_MMIO_READ
        || rc == VINF_IOM_R3_MMIO_READ_WRITE
        || rc == VINF_IOM_R3_MMIO_WRITE
        || rc == VINF_CPUM_R3_MSR_READ
        || rc == VINF_CPUM_R3_MSR_WRITE
        || rc == VINF_EM_RESCHEDULE
        )
    {
        EMRemLock(pVM);
        rc = REMR3EmulateInstruction(pVM, pVCpu);
        AssertRC(rc);
        EMRemUnlock(pVM);
        fRem = true;
    }

    /*
     * Compare the register states.
     */
    unsigned cDiffs = 0;
    if (memcmp(pOrgCtx, pDebugCtx, sizeof(*pDebugCtx)))
    {
        //Log(("REM and IEM ends up with different registers!\n"));
        const char *pszWho = fRem ? "rem" : HMR3IsVmxEnabled(pVM->pUVM) ? "vmx" : "svm";

#  define CHECK_FIELD(a_Field) \
    do \
    { \
        if (pOrgCtx->a_Field != pDebugCtx->a_Field) \
        { \
            switch (sizeof(pOrgCtx->a_Field)) \
            { \
                case 1: RTAssertMsg2Weak("  %8s differs - iem=%02x - %s=%02x\n", #a_Field, pDebugCtx->a_Field, pszWho, pOrgCtx->a_Field); break; \
                case 2: RTAssertMsg2Weak("  %8s differs - iem=%04x - %s=%04x\n", #a_Field, pDebugCtx->a_Field, pszWho, pOrgCtx->a_Field); break; \
                case 4: RTAssertMsg2Weak("  %8s differs - iem=%08x - %s=%08x\n", #a_Field, pDebugCtx->a_Field, pszWho, pOrgCtx->a_Field); break; \
                case 8: RTAssertMsg2Weak("  %8s differs - iem=%016llx - %s=%016llx\n", #a_Field, pDebugCtx->a_Field, pszWho, pOrgCtx->a_Field); break; \
                default: RTAssertMsg2Weak("  %8s differs\n", #a_Field); break; \
            } \
            cDiffs++; \
        } \
    } while (0)
#  define CHECK_XSTATE_FIELD(a_Field) \
    do \
    { \
        if (pOrgXState->a_Field != pDebugXState->a_Field) \
        { \
            switch (sizeof(pOrgXState->a_Field)) \
            { \
                case 1: RTAssertMsg2Weak("  %8s differs - iem=%02x - %s=%02x\n", #a_Field, pDebugXState->a_Field, pszWho, pOrgXState->a_Field); break; \
                case 2: RTAssertMsg2Weak("  %8s differs - iem=%04x - %s=%04x\n", #a_Field, pDebugXState->a_Field, pszWho, pOrgXState->a_Field); break; \
                case 4: RTAssertMsg2Weak("  %8s differs - iem=%08x - %s=%08x\n", #a_Field, pDebugXState->a_Field, pszWho, pOrgXState->a_Field); break; \
                case 8: RTAssertMsg2Weak("  %8s differs - iem=%016llx - %s=%016llx\n", #a_Field, pDebugXState->a_Field, pszWho, pOrgXState->a_Field); break; \
                default: RTAssertMsg2Weak("  %8s differs\n", #a_Field); break; \
            } \
            cDiffs++; \
        } \
    } while (0)

#  define CHECK_BIT_FIELD(a_Field) \
    do \
    { \
        if (pOrgCtx->a_Field != pDebugCtx->a_Field) \
        { \
            RTAssertMsg2Weak("  %8s differs - iem=%02x - %s=%02x\n", #a_Field, pDebugCtx->a_Field, pszWho, pOrgCtx->a_Field); \
            cDiffs++; \
        } \
    } while (0)

#  define CHECK_SEL(a_Sel) \
    do \
    { \
        CHECK_FIELD(a_Sel.Sel); \
        CHECK_FIELD(a_Sel.Attr.u); \
        CHECK_FIELD(a_Sel.u64Base); \
        CHECK_FIELD(a_Sel.u32Limit); \
        CHECK_FIELD(a_Sel.fFlags); \
    } while (0)

        PX86XSAVEAREA pOrgXState   = pOrgCtx->CTX_SUFF(pXState);
        PX86XSAVEAREA pDebugXState = pDebugCtx->CTX_SUFF(pXState);

#if 1 /* The recompiler doesn't update these the intel way. */
        if (fRem)
        {
            pOrgXState->x87.FOP        = pDebugXState->x87.FOP;
            pOrgXState->x87.FPUIP      = pDebugXState->x87.FPUIP;
            pOrgXState->x87.CS         = pDebugXState->x87.CS;
            pOrgXState->x87.Rsrvd1     = pDebugXState->x87.Rsrvd1;
            pOrgXState->x87.FPUDP      = pDebugXState->x87.FPUDP;
            pOrgXState->x87.DS         = pDebugXState->x87.DS;
            pOrgXState->x87.Rsrvd2     = pDebugXState->x87.Rsrvd2;
            //pOrgXState->x87.MXCSR_MASK = pDebugXState->x87.MXCSR_MASK;
            if ((pOrgXState->x87.FSW & X86_FSW_TOP_MASK) == (pDebugXState->x87.FSW & X86_FSW_TOP_MASK))
                pOrgXState->x87.FSW = pDebugXState->x87.FSW;
        }
#endif
        if (memcmp(&pOrgXState->x87, &pDebugXState->x87, sizeof(pDebugXState->x87)))
        {
            RTAssertMsg2Weak("  the FPU state differs\n");
            cDiffs++;
            CHECK_XSTATE_FIELD(x87.FCW);
            CHECK_XSTATE_FIELD(x87.FSW);
            CHECK_XSTATE_FIELD(x87.FTW);
            CHECK_XSTATE_FIELD(x87.FOP);
            CHECK_XSTATE_FIELD(x87.FPUIP);
            CHECK_XSTATE_FIELD(x87.CS);
            CHECK_XSTATE_FIELD(x87.Rsrvd1);
            CHECK_XSTATE_FIELD(x87.FPUDP);
            CHECK_XSTATE_FIELD(x87.DS);
            CHECK_XSTATE_FIELD(x87.Rsrvd2);
            CHECK_XSTATE_FIELD(x87.MXCSR);
            CHECK_XSTATE_FIELD(x87.MXCSR_MASK);
            CHECK_XSTATE_FIELD(x87.aRegs[0].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[0].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[1].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[1].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[2].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[2].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[3].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[3].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[4].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[4].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[5].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[5].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[6].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[6].au64[1]);
            CHECK_XSTATE_FIELD(x87.aRegs[7].au64[0]); CHECK_XSTATE_FIELD(x87.aRegs[7].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 0].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 0].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 1].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 1].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 2].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 2].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 3].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 3].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 4].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 4].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 5].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 5].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 6].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 6].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 7].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 7].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 8].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 8].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[ 9].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[ 9].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[10].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[10].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[11].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[11].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[12].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[12].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[13].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[13].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[14].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[14].au64[1]);
            CHECK_XSTATE_FIELD(x87.aXMM[15].au64[0]);  CHECK_XSTATE_FIELD(x87.aXMM[15].au64[1]);
            for (unsigned i = 0; i < RT_ELEMENTS(pOrgXState->x87.au32RsrvdRest); i++)
                CHECK_XSTATE_FIELD(x87.au32RsrvdRest[i]);
        }
        CHECK_FIELD(rip);
        uint32_t fFlagsMask = UINT32_MAX & ~pIemCpu->fUndefinedEFlags;
        if ((pOrgCtx->rflags.u & fFlagsMask) != (pDebugCtx->rflags.u & fFlagsMask))
        {
            RTAssertMsg2Weak("  rflags differs - iem=%08llx %s=%08llx\n", pDebugCtx->rflags.u, pszWho, pOrgCtx->rflags.u);
            CHECK_BIT_FIELD(rflags.Bits.u1CF);
            CHECK_BIT_FIELD(rflags.Bits.u1Reserved0);
            CHECK_BIT_FIELD(rflags.Bits.u1PF);
            CHECK_BIT_FIELD(rflags.Bits.u1Reserved1);
            CHECK_BIT_FIELD(rflags.Bits.u1AF);
            CHECK_BIT_FIELD(rflags.Bits.u1Reserved2);
            CHECK_BIT_FIELD(rflags.Bits.u1ZF);
            CHECK_BIT_FIELD(rflags.Bits.u1SF);
            CHECK_BIT_FIELD(rflags.Bits.u1TF);
            CHECK_BIT_FIELD(rflags.Bits.u1IF);
            CHECK_BIT_FIELD(rflags.Bits.u1DF);
            CHECK_BIT_FIELD(rflags.Bits.u1OF);
            CHECK_BIT_FIELD(rflags.Bits.u2IOPL);
            CHECK_BIT_FIELD(rflags.Bits.u1NT);
            CHECK_BIT_FIELD(rflags.Bits.u1Reserved3);
            if (0 && !fRem) /** @todo debug the occational clear RF flags when running against VT-x. */
                CHECK_BIT_FIELD(rflags.Bits.u1RF);
            CHECK_BIT_FIELD(rflags.Bits.u1VM);
            CHECK_BIT_FIELD(rflags.Bits.u1AC);
            CHECK_BIT_FIELD(rflags.Bits.u1VIF);
            CHECK_BIT_FIELD(rflags.Bits.u1VIP);
            CHECK_BIT_FIELD(rflags.Bits.u1ID);
        }

        if (pIemCpu->cIOReads != 1 && !pIemCpu->fIgnoreRaxRdx)
            CHECK_FIELD(rax);
        CHECK_FIELD(rcx);
        if (!pIemCpu->fIgnoreRaxRdx)
            CHECK_FIELD(rdx);
        CHECK_FIELD(rbx);
        CHECK_FIELD(rsp);
        CHECK_FIELD(rbp);
        CHECK_FIELD(rsi);
        CHECK_FIELD(rdi);
        CHECK_FIELD(r8);
        CHECK_FIELD(r9);
        CHECK_FIELD(r10);
        CHECK_FIELD(r11);
        CHECK_FIELD(r12);
        CHECK_FIELD(r13);
        CHECK_SEL(cs);
        CHECK_SEL(ss);
        CHECK_SEL(ds);
        CHECK_SEL(es);
        CHECK_SEL(fs);
        CHECK_SEL(gs);
        CHECK_FIELD(cr0);

        /* Klugde #1: REM fetches code and across the page boundrary and faults on the next page, while we execute
           the faulting instruction first: 001b:77f61ff3 66 8b 42 02   mov ax, word [edx+002h] (NT4SP1) */
        /* Kludge #2: CR2 differs slightly on cross page boundrary faults, we report the last address of the access
           while REM reports the address of the first byte on the page.  Pending investigation as to which is correct. */
        if (pOrgCtx->cr2 != pDebugCtx->cr2)
        {
            if (pIemCpu->uOldCs == 0x1b && pIemCpu->uOldRip == 0x77f61ff3 && fRem)
            { /* ignore */ }
            else if (   (pOrgCtx->cr2 & ~(uint64_t)3) == (pDebugCtx->cr2 & ~(uint64_t)3)
                     && (pOrgCtx->cr2 & PAGE_OFFSET_MASK) == 0
                     && fRem)
            { /* ignore */ }
            else
                CHECK_FIELD(cr2);
        }
        CHECK_FIELD(cr3);
        CHECK_FIELD(cr4);
        CHECK_FIELD(dr[0]);
        CHECK_FIELD(dr[1]);
        CHECK_FIELD(dr[2]);
        CHECK_FIELD(dr[3]);
        CHECK_FIELD(dr[6]);
        if (!fRem || (pOrgCtx->dr[7] & ~X86_DR7_RA1_MASK) != (pDebugCtx->dr[7] & ~X86_DR7_RA1_MASK)) /* REM 'mov drX,greg' bug.*/
            CHECK_FIELD(dr[7]);
        CHECK_FIELD(gdtr.cbGdt);
        CHECK_FIELD(gdtr.pGdt);
        CHECK_FIELD(idtr.cbIdt);
        CHECK_FIELD(idtr.pIdt);
        CHECK_SEL(ldtr);
        CHECK_SEL(tr);
        CHECK_FIELD(SysEnter.cs);
        CHECK_FIELD(SysEnter.eip);
        CHECK_FIELD(SysEnter.esp);
        CHECK_FIELD(msrEFER);
        CHECK_FIELD(msrSTAR);
        CHECK_FIELD(msrPAT);
        CHECK_FIELD(msrLSTAR);
        CHECK_FIELD(msrCSTAR);
        CHECK_FIELD(msrSFMASK);
        CHECK_FIELD(msrKERNELGSBASE);

        if (cDiffs != 0)
        {
            DBGFR3Info(pVM->pUVM, "cpumguest", "verbose", NULL);
            RTAssertMsg1(NULL, __LINE__, __FILE__, __FUNCTION__);
            iemVerifyAssertMsg2(pIemCpu);
            RTAssertPanic();
        }
#  undef CHECK_FIELD
#  undef CHECK_BIT_FIELD
    }

    /*
     * If the register state compared fine, check the verification event
     * records.
     */
    if (cDiffs == 0 && !pIemCpu->fOverlappingMovs)
    {
        /*
         * Compare verficiation event records.
         *  - I/O port accesses should be a 1:1 match.
         */
        PIEMVERIFYEVTREC pIemRec   = pIemCpu->pIemEvtRecHead;
        PIEMVERIFYEVTREC pOtherRec = pIemCpu->pOtherEvtRecHead;
        while (pIemRec && pOtherRec)
        {
            /* Since we might miss RAM writes and reads, ignore reads and check
               that any written memory is the same extra ones.  */
            while (   IEMVERIFYEVENT_IS_RAM(pIemRec->enmEvent)
                   && !IEMVERIFYEVENT_IS_RAM(pOtherRec->enmEvent)
                   && pIemRec->pNext)
            {
                if (pIemRec->enmEvent == IEMVERIFYEVENT_RAM_WRITE)
                    iemVerifyWriteRecord(pIemCpu, pIemRec, fRem);
                pIemRec = pIemRec->pNext;
            }

            /* Do the compare. */
            if (pIemRec->enmEvent != pOtherRec->enmEvent)
            {
                iemVerifyAssertRecords(pIemCpu, pIemRec, pOtherRec, "Type mismatches");
                break;
            }
            bool fEquals;
            switch (pIemRec->enmEvent)
            {
                case IEMVERIFYEVENT_IOPORT_READ:
                    fEquals = pIemRec->u.IOPortRead.Port            == pOtherRec->u.IOPortRead.Port
                           && pIemRec->u.IOPortRead.cbValue         == pOtherRec->u.IOPortRead.cbValue;
                    break;
                case IEMVERIFYEVENT_IOPORT_WRITE:
                    fEquals = pIemRec->u.IOPortWrite.Port           == pOtherRec->u.IOPortWrite.Port
                           && pIemRec->u.IOPortWrite.cbValue        == pOtherRec->u.IOPortWrite.cbValue
                           && pIemRec->u.IOPortWrite.u32Value       == pOtherRec->u.IOPortWrite.u32Value;
                    break;
                case IEMVERIFYEVENT_IOPORT_STR_READ:
                    fEquals = pIemRec->u.IOPortStrRead.Port         == pOtherRec->u.IOPortStrRead.Port
                           && pIemRec->u.IOPortStrRead.cbValue      == pOtherRec->u.IOPortStrRead.cbValue
                           && pIemRec->u.IOPortStrRead.cTransfers   == pOtherRec->u.IOPortStrRead.cTransfers;
                    break;
                case IEMVERIFYEVENT_IOPORT_STR_WRITE:
                    fEquals = pIemRec->u.IOPortStrWrite.Port        == pOtherRec->u.IOPortStrWrite.Port
                           && pIemRec->u.IOPortStrWrite.cbValue     == pOtherRec->u.IOPortStrWrite.cbValue
                           && pIemRec->u.IOPortStrWrite.cTransfers  == pOtherRec->u.IOPortStrWrite.cTransfers;
                    break;
                case IEMVERIFYEVENT_RAM_READ:
                    fEquals = pIemRec->u.RamRead.GCPhys             == pOtherRec->u.RamRead.GCPhys
                           && pIemRec->u.RamRead.cb                 == pOtherRec->u.RamRead.cb;
                    break;
                case IEMVERIFYEVENT_RAM_WRITE:
                    fEquals = pIemRec->u.RamWrite.GCPhys            == pOtherRec->u.RamWrite.GCPhys
                           && pIemRec->u.RamWrite.cb                == pOtherRec->u.RamWrite.cb
                           && !memcmp(pIemRec->u.RamWrite.ab, pOtherRec->u.RamWrite.ab, pIemRec->u.RamWrite.cb);
                    break;
                default:
                    fEquals = false;
                    break;
            }
            if (!fEquals)
            {
                iemVerifyAssertRecords(pIemCpu, pIemRec, pOtherRec, "Mismatch");
                break;
            }

            /* advance */
            pIemRec   = pIemRec->pNext;
            pOtherRec = pOtherRec->pNext;
        }

        /* Ignore extra writes and reads. */
        while (pIemRec && IEMVERIFYEVENT_IS_RAM(pIemRec->enmEvent))
        {
            if (pIemRec->enmEvent == IEMVERIFYEVENT_RAM_WRITE)
                iemVerifyWriteRecord(pIemCpu, pIemRec, fRem);
            pIemRec = pIemRec->pNext;
        }
        if (pIemRec != NULL)
            iemVerifyAssertRecord(pIemCpu, pIemRec, "Extra IEM record!");
        else if (pOtherRec != NULL)
            iemVerifyAssertRecord(pIemCpu, pOtherRec, "Extra Other record!");
    }
    pIemCpu->CTX_SUFF(pCtx) = pOrgCtx;
}

#else  /* !IEM_VERIFICATION_MODE_FULL || !IN_RING3 */

/* stubs */
IEM_STATIC VBOXSTRICTRC     iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue)
{
    NOREF(pIemCpu); NOREF(Port); NOREF(pu32Value); NOREF(cbValue);
    return VERR_INTERNAL_ERROR;
}

IEM_STATIC VBOXSTRICTRC     iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
    NOREF(pIemCpu); NOREF(Port); NOREF(u32Value); NOREF(cbValue);
    return VERR_INTERNAL_ERROR;
}

#endif /* !IEM_VERIFICATION_MODE_FULL || !IN_RING3 */


#ifdef LOG_ENABLED
/**
 * Logs the current instruction.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        The current CPU context.
 * @param   fSameCtx    Set if we have the same context information as the VMM,
 *                      clear if we may have already executed an instruction in
 *                      our debug context. When clear, we assume IEMCPU holds
 *                      valid CPU mode info.
 */
IEM_STATIC void iemLogCurInstr(PVMCPU pVCpu, PCPUMCTX pCtx, bool fSameCtx)
{
# ifdef IN_RING3
    if (LogIs2Enabled())
    {
        char     szInstr[256];
        uint32_t cbInstr = 0;
        if (fSameCtx)
            DBGFR3DisasInstrEx(pVCpu->pVMR3->pUVM, pVCpu->idCpu, 0, 0,
                               DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                               szInstr, sizeof(szInstr), &cbInstr);
        else
        {
            uint32_t fFlags = 0;
            switch (pVCpu->iem.s.enmCpuMode)
            {
                case IEMMODE_64BIT: fFlags |= DBGF_DISAS_FLAGS_64BIT_MODE; break;
                case IEMMODE_32BIT: fFlags |= DBGF_DISAS_FLAGS_32BIT_MODE; break;
                case IEMMODE_16BIT:
                    if (!(pCtx->cr0 & X86_CR0_PE) || pCtx->eflags.Bits.u1VM)
                        fFlags |= DBGF_DISAS_FLAGS_16BIT_REAL_MODE;
                    else
                        fFlags |= DBGF_DISAS_FLAGS_16BIT_MODE;
                    break;
            }
            DBGFR3DisasInstrEx(pVCpu->pVMR3->pUVM, pVCpu->idCpu, pCtx->cs.Sel, pCtx->rip, fFlags,
                               szInstr, sizeof(szInstr), &cbInstr);
        }

        PCX86FXSTATE pFpuCtx = &pCtx->CTX_SUFF(pXState)->x87;
        Log2(("****\n"
              " eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
              " eip=%08x esp=%08x ebp=%08x iopl=%d tr=%04x\n"
              " cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x efl=%08x\n"
              " fsw=%04x fcw=%04x ftw=%02x mxcsr=%04x/%04x\n"
              " %s\n"
              ,
              pCtx->eax, pCtx->ebx, pCtx->ecx, pCtx->edx, pCtx->esi, pCtx->edi,
              pCtx->eip, pCtx->esp, pCtx->ebp, pCtx->eflags.Bits.u2IOPL, pCtx->tr.Sel,
              pCtx->cs.Sel, pCtx->ss.Sel, pCtx->ds.Sel, pCtx->es.Sel,
              pCtx->fs.Sel, pCtx->gs.Sel, pCtx->eflags.u,
              pFpuCtx->FSW, pFpuCtx->FCW, pFpuCtx->FTW, pFpuCtx->MXCSR, pFpuCtx->MXCSR_MASK,
              szInstr));

        if (LogIs3Enabled())
            DBGFR3Info(pVCpu->pVMR3->pUVM, "cpumguest", "verbose", NULL);
    }
    else
# endif
        LogFlow(("IEMExecOne: cs:rip=%04x:%08RX64 ss:rsp=%04x:%08RX64 EFL=%06x\n",
                 pCtx->cs.Sel, pCtx->rip, pCtx->ss.Sel, pCtx->rsp, pCtx->eflags.u));

    uint8_t abTmp[16]; RT_ZERO(abTmp);
    VBOXSTRICTRC rc2 = PGMPhysRead(pVCpu->CTX_SUFF(pVM), 0x2c370, abTmp, sizeof(abTmp), PGMACCESSORIGIN_IEM);
    Log(("0x2c370: %.16Rhxs %Rrc\n", &abTmp[0], VBOXSTRICTRC_VAL(rc2)));
}
#endif


/**
 * Makes status code addjustments (pass up from I/O and access handler)
 * as well as maintaining statistics.
 *
 * @returns Strict VBox status code to pass up.
 * @param   pIemCpu     The IEM per CPU data.
 * @param   rcStrict    The status from executing an instruction.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemExecStatusCodeFiddling(PIEMCPU pIemCpu, VBOXSTRICTRC rcStrict)
{
    if (rcStrict != VINF_SUCCESS)
    {
        if (RT_SUCCESS(rcStrict))
        {
            AssertMsg(   (rcStrict >= VINF_EM_FIRST && rcStrict <= VINF_EM_LAST)
                      || rcStrict == VINF_IOM_R3_IOPORT_READ
                      || rcStrict == VINF_IOM_R3_IOPORT_WRITE
                      || rcStrict == VINF_IOM_R3_MMIO_READ
                      || rcStrict == VINF_IOM_R3_MMIO_READ_WRITE
                      || rcStrict == VINF_IOM_R3_MMIO_WRITE
                      || rcStrict == VINF_CPUM_R3_MSR_READ
                      || rcStrict == VINF_CPUM_R3_MSR_WRITE
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR
                      || rcStrict == VINF_EM_RAW_TO_R3
                      || rcStrict == VINF_EM_RAW_EMULATE_IO_BLOCK
                      /* raw-mode / virt handlers only: */
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT
                      || rcStrict == VINF_SELM_SYNC_GDT
                      || rcStrict == VINF_CSAM_PENDING_ACTION
                      || rcStrict == VINF_PATM_CHECK_PATCH_PAGE
                      , ("rcStrict=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
/** @todo adjust for VINF_EM_RAW_EMULATE_INSTR   */
            int32_t const rcPassUp = pIemCpu->rcPassUp;
            if (rcPassUp == VINF_SUCCESS)
                pIemCpu->cRetInfStatuses++;
            else if (   rcPassUp < VINF_EM_FIRST
                     || rcPassUp > VINF_EM_LAST
                     || rcPassUp < VBOXSTRICTRC_VAL(rcStrict))
            {
                Log(("IEM: rcPassUp=%Rrc! rcStrict=%Rrc\n", rcPassUp, VBOXSTRICTRC_VAL(rcStrict)));
                pIemCpu->cRetPassUpStatus++;
                rcStrict = rcPassUp;
            }
            else
            {
                Log(("IEM: rcPassUp=%Rrc  rcStrict=%Rrc!\n", rcPassUp, VBOXSTRICTRC_VAL(rcStrict)));
                pIemCpu->cRetInfStatuses++;
            }
        }
        else if (rcStrict == VERR_IEM_ASPECT_NOT_IMPLEMENTED)
            pIemCpu->cRetAspectNotImplemented++;
        else if (rcStrict == VERR_IEM_INSTR_NOT_IMPLEMENTED)
            pIemCpu->cRetInstrNotImplemented++;
#ifdef IEM_VERIFICATION_MODE_FULL
        else if (rcStrict == VERR_IEM_RESTART_INSTRUCTION)
            rcStrict = VINF_SUCCESS;
#endif
        else
            pIemCpu->cRetErrStatuses++;
    }
    else if (pIemCpu->rcPassUp != VINF_SUCCESS)
    {
        pIemCpu->cRetPassUpStatus++;
        rcStrict = pIemCpu->rcPassUp;
    }

    return rcStrict;
}


/**
 * The actual code execution bits of IEMExecOne, IEMExecOneEx, and
 * IEMExecOneWithPrefetchedByPC.
 *
 * @return  Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pIemCpu     The IEM per CPU data.
 * @param   fExecuteInhibit     If set, execute the instruction following CLI,
 *                      POP SS and MOV SS,GR.
 */
DECL_FORCE_INLINE(VBOXSTRICTRC) iemExecOneInner(PVMCPU pVCpu, PIEMCPU pIemCpu, bool fExecuteInhibit)
{
    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    VBOXSTRICTRC rcStrict = FNIEMOP_CALL(g_apfnOneByteMap[b]);
    if (rcStrict == VINF_SUCCESS)
        pIemCpu->cInstructions++;
    if (pIemCpu->cActiveMappings > 0)
        iemMemRollback(pIemCpu);
//#ifdef DEBUG
//    AssertMsg(pIemCpu->offOpcode == cbInstr || rcStrict != VINF_SUCCESS, ("%u %u\n", pIemCpu->offOpcode, cbInstr));
//#endif

    /* Execute the next instruction as well if a cli, pop ss or
       mov ss, Gr has just completed successfully. */
    if (   fExecuteInhibit
        && rcStrict == VINF_SUCCESS
        && VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && EMGetInhibitInterruptsPC(pVCpu) == pIemCpu->CTX_SUFF(pCtx)->rip )
    {
        rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, pIemCpu->fBypassHandlers);
        if (rcStrict == VINF_SUCCESS)
        {
# ifdef LOG_ENABLED
            iemLogCurInstr(IEMCPU_TO_VMCPU(pIemCpu), pIemCpu->CTX_SUFF(pCtx), false);
# endif
            IEM_OPCODE_GET_NEXT_U8(&b);
            rcStrict = FNIEMOP_CALL(g_apfnOneByteMap[b]);
            if (rcStrict == VINF_SUCCESS)
                pIemCpu->cInstructions++;
            if (pIemCpu->cActiveMappings > 0)
                iemMemRollback(pIemCpu);
        }
        EMSetInhibitInterruptsPC(pVCpu, UINT64_C(0x7777555533331111));
    }

    /*
     * Return value fiddling, statistics and sanity assertions.
     */
    rcStrict = iemExecStatusCodeFiddling(pIemCpu, rcStrict);

    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pIemCpu->CTX_SUFF(pCtx)->cs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pIemCpu->CTX_SUFF(pCtx)->ss));
#if defined(IEM_VERIFICATION_MODE_FULL)
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pIemCpu->CTX_SUFF(pCtx)->es));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pIemCpu->CTX_SUFF(pCtx)->ds));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pIemCpu->CTX_SUFF(pCtx)->fs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pIemCpu->CTX_SUFF(pCtx)->gs));
#endif
    return rcStrict;
}


#ifdef IN_RC
/**
 * Re-enters raw-mode or ensure we return to ring-3.
 *
 * @returns rcStrict, maybe modified.
 * @param   pIemCpu     The IEM CPU structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        The current CPU context.
 * @param   rcStrict    The status code returne by the interpreter.
 */
DECLINLINE(VBOXSTRICTRC) iemRCRawMaybeReenter(PIEMCPU pIemCpu, PVMCPU pVCpu, PCPUMCTX pCtx, VBOXSTRICTRC rcStrict)
{
    if (!pIemCpu->fInPatchCode)
        CPUMRawEnter(pVCpu);
    return rcStrict;
}
#endif


/**
 * Execute one instruction.
 *
 * @return  Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 */
VMMDECL(VBOXSTRICTRC) IEMExecOne(PVMCPU pVCpu)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;

#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)
    iemExecVerificationModeSetup(pIemCpu);
#endif
#ifdef LOG_ENABLED
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    iemLogCurInstr(pVCpu, pCtx, true);
#endif

    /*
     * Do the decoding and emulation.
     */
    VBOXSTRICTRC rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, false);
    if (rcStrict == VINF_SUCCESS)
        rcStrict = iemExecOneInner(pVCpu, pIemCpu, true);

#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)
    /*
     * Assert some sanity.
     */
    iemExecVerificationModeCheck(pIemCpu);
#endif
#ifdef IN_RC
    rcStrict = iemRCRawMaybeReenter(pIemCpu, pVCpu, pIemCpu->CTX_SUFF(pCtx), rcStrict);
#endif
    if (rcStrict != VINF_SUCCESS)
        LogFlow(("IEMExecOne: cs:rip=%04x:%08RX64 ss:rsp=%04x:%08RX64 EFL=%06x - rcStrict=%Rrc\n",
                 pCtx->cs.Sel, pCtx->rip, pCtx->ss.Sel, pCtx->rsp, pCtx->eflags.u, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


VMMDECL(VBOXSTRICTRC)       IEMExecOneEx(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint32_t *pcbWritten)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;
    PCPUMCTX pCtx    = pVCpu->iem.s.CTX_SUFF(pCtx);
    AssertReturn(CPUMCTX2CORE(pCtx) == pCtxCore, VERR_IEM_IPE_3);

    uint32_t const cbOldWritten = pIemCpu->cbWritten;
    VBOXSTRICTRC rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, false);
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = iemExecOneInner(pVCpu, pIemCpu, true);
        if (pcbWritten)
            *pcbWritten = pIemCpu->cbWritten - cbOldWritten;
    }

#ifdef IN_RC
    rcStrict = iemRCRawMaybeReenter(pIemCpu, pVCpu, pCtx, rcStrict);
#endif
    return rcStrict;
}


VMMDECL(VBOXSTRICTRC)       IEMExecOneWithPrefetchedByPC(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint64_t OpcodeBytesPC,
                                                         const void *pvOpcodeBytes, size_t cbOpcodeBytes)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;
    PCPUMCTX pCtx    = pVCpu->iem.s.CTX_SUFF(pCtx);
    AssertReturn(CPUMCTX2CORE(pCtx) == pCtxCore, VERR_IEM_IPE_3);

    VBOXSTRICTRC rcStrict;
    if (   cbOpcodeBytes
        && pCtx->rip == OpcodeBytesPC)
    {
        iemInitDecoder(pIemCpu, false);
        pIemCpu->cbOpcode = (uint8_t)RT_MIN(cbOpcodeBytes, sizeof(pIemCpu->abOpcode));
        memcpy(pIemCpu->abOpcode, pvOpcodeBytes, pIemCpu->cbOpcode);
        rcStrict = VINF_SUCCESS;
    }
    else
        rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, false);
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = iemExecOneInner(pVCpu, pIemCpu, true);
    }

#ifdef IN_RC
    rcStrict = iemRCRawMaybeReenter(pIemCpu, pVCpu, pCtx, rcStrict);
#endif
    return rcStrict;
}


VMMDECL(VBOXSTRICTRC)       IEMExecOneBypassEx(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint32_t *pcbWritten)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;
    PCPUMCTX pCtx    = pVCpu->iem.s.CTX_SUFF(pCtx);
    AssertReturn(CPUMCTX2CORE(pCtx) == pCtxCore, VERR_IEM_IPE_3);

    uint32_t const cbOldWritten = pIemCpu->cbWritten;
    VBOXSTRICTRC rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, true);
    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = iemExecOneInner(pVCpu, pIemCpu, false);
        if (pcbWritten)
            *pcbWritten = pIemCpu->cbWritten - cbOldWritten;
    }

#ifdef IN_RC
    rcStrict = iemRCRawMaybeReenter(pIemCpu, pVCpu, pCtx, rcStrict);
#endif
    return rcStrict;
}


VMMDECL(VBOXSTRICTRC)       IEMExecOneBypassWithPrefetchedByPC(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint64_t OpcodeBytesPC,
                                                               const void *pvOpcodeBytes, size_t cbOpcodeBytes)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;
    PCPUMCTX pCtx    = pVCpu->iem.s.CTX_SUFF(pCtx);
    AssertReturn(CPUMCTX2CORE(pCtx) == pCtxCore, VERR_IEM_IPE_3);

    VBOXSTRICTRC rcStrict;
    if (   cbOpcodeBytes
        && pCtx->rip == OpcodeBytesPC)
    {
        iemInitDecoder(pIemCpu, true);
        pIemCpu->cbOpcode = (uint8_t)RT_MIN(cbOpcodeBytes, sizeof(pIemCpu->abOpcode));
        memcpy(pIemCpu->abOpcode, pvOpcodeBytes, pIemCpu->cbOpcode);
        rcStrict = VINF_SUCCESS;
    }
    else
        rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, true);
    if (rcStrict == VINF_SUCCESS)
        rcStrict = iemExecOneInner(pVCpu, pIemCpu, false);

#ifdef IN_RC
    rcStrict = iemRCRawMaybeReenter(pIemCpu, pVCpu, pCtx, rcStrict);
#endif
    return rcStrict;
}


VMMDECL(VBOXSTRICTRC) IEMExecLots(PVMCPU pVCpu)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;

    /*
     * See if there is an interrupt pending in TRPM and inject it if we can.
     */
#if !defined(IEM_VERIFICATION_MODE_FULL) || !defined(IN_RING3)
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
# ifdef IEM_VERIFICATION_MODE_FULL
    pIemCpu->uInjectCpl = UINT8_MAX;
# endif
    if (   pCtx->eflags.Bits.u1IF
        && TRPMHasTrap(pVCpu)
        && EMGetInhibitInterruptsPC(pVCpu) != pCtx->rip)
    {
        uint8_t     u8TrapNo;
        TRPMEVENT   enmType;
        RTGCUINT    uErrCode;
        RTGCPTR     uCr2;
        int rc2 = TRPMQueryTrapAll(pVCpu, &u8TrapNo, &enmType, &uErrCode, &uCr2, NULL /* pu8InstLen */); AssertRC(rc2);
        IEMInjectTrap(pVCpu, u8TrapNo, enmType, (uint16_t)uErrCode, uCr2, 0 /* cbInstr */);
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            TRPMResetTrap(pVCpu);
    }
#else
    iemExecVerificationModeSetup(pIemCpu);
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
#endif

    /*
     * Log the state.
     */
#ifdef LOG_ENABLED
    iemLogCurInstr(pVCpu, pCtx, true);
#endif

    /*
     * Do the decoding and emulation.
     */
    VBOXSTRICTRC rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu, false);
    if (rcStrict == VINF_SUCCESS)
        rcStrict = iemExecOneInner(pVCpu, pIemCpu, true);

#if defined(IEM_VERIFICATION_MODE_FULL) && defined(IN_RING3)
    /*
     * Assert some sanity.
     */
    iemExecVerificationModeCheck(pIemCpu);
#endif

    /*
     * Maybe re-enter raw-mode and log.
     */
#ifdef IN_RC
    rcStrict = iemRCRawMaybeReenter(pIemCpu, pVCpu, pIemCpu->CTX_SUFF(pCtx), rcStrict);
#endif
    if (rcStrict != VINF_SUCCESS)
        LogFlow(("IEMExecLots: cs:rip=%04x:%08RX64 ss:rsp=%04x:%08RX64 EFL=%06x - rcStrict=%Rrc\n",
                 pCtx->cs.Sel, pCtx->rip, pCtx->ss.Sel, pCtx->rsp, pCtx->eflags.u, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}



/**
 * Injects a trap, fault, abort, software interrupt or external interrupt.
 *
 * The parameter list matches TRPMQueryTrapAll pretty closely.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling EMT.
 * @param   u8TrapNo            The trap number.
 * @param   enmType             What type is it (trap/fault/abort), software
 *                              interrupt or hardware interrupt.
 * @param   uErrCode            The error code if applicable.
 * @param   uCr2                The CR2 value if applicable.
 * @param   cbInstr             The instruction length (only relevant for
 *                              software interrupts).
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMInjectTrap(PVMCPU pVCpu, uint8_t u8TrapNo, TRPMEVENT enmType, uint16_t uErrCode, RTGCPTR uCr2,
                                         uint8_t cbInstr)
{
    iemInitDecoder(&pVCpu->iem.s, false);
#ifdef DBGFTRACE_ENABLED
    RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "IEMInjectTrap: %x %d %x %llx",
                      u8TrapNo, enmType, uErrCode, uCr2);
#endif

    uint32_t fFlags;
    switch (enmType)
    {
        case TRPM_HARDWARE_INT:
            Log(("IEMInjectTrap: %#4x ext\n", u8TrapNo));
            fFlags = IEM_XCPT_FLAGS_T_EXT_INT;
            uErrCode = uCr2 = 0;
            break;

        case TRPM_SOFTWARE_INT:
            Log(("IEMInjectTrap: %#4x soft\n", u8TrapNo));
            fFlags = IEM_XCPT_FLAGS_T_SOFT_INT;
            uErrCode = uCr2 = 0;
            break;

        case TRPM_TRAP:
            Log(("IEMInjectTrap: %#4x trap err=%#x cr2=%#RGv\n", u8TrapNo, uErrCode, uCr2));
            fFlags = IEM_XCPT_FLAGS_T_CPU_XCPT;
            if (u8TrapNo == X86_XCPT_PF)
                fFlags |= IEM_XCPT_FLAGS_CR2;
            switch (u8TrapNo)
            {
                case X86_XCPT_DF:
                case X86_XCPT_TS:
                case X86_XCPT_NP:
                case X86_XCPT_SS:
                case X86_XCPT_PF:
                case X86_XCPT_AC:
                    fFlags |= IEM_XCPT_FLAGS_ERR;
                    break;

                case X86_XCPT_NMI:
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
                    break;
            }
            break;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    return iemRaiseXcptOrInt(&pVCpu->iem.s, cbInstr, u8TrapNo, fFlags, uErrCode, uCr2);
}


/**
 * Injects the active TRPM event.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 */
VMMDECL(VBOXSTRICTRC) IEMInjectTrpmEvent(PVMCPU pVCpu)
{
#ifndef IEM_IMPLEMENTS_TASKSWITCH
    IEM_RETURN_ASPECT_NOT_IMPLEMENTED_LOG(("Event injection\n"));
#else
    uint8_t     u8TrapNo;
    TRPMEVENT   enmType;
    RTGCUINT    uErrCode;
    RTGCUINTPTR uCr2;
    uint8_t     cbInstr;
    int rc = TRPMQueryTrapAll(pVCpu, &u8TrapNo, &enmType, &uErrCode, &uCr2, &cbInstr);
    if (RT_FAILURE(rc))
        return rc;

    VBOXSTRICTRC rcStrict = IEMInjectTrap(pVCpu, u8TrapNo, enmType, uErrCode, uCr2, cbInstr);

    /** @todo Are there any other codes that imply the event was successfully
     *        delivered to the guest? See @bugref{6607}.  */
    if (   rcStrict == VINF_SUCCESS
        || rcStrict == VINF_IEM_RAISED_XCPT)
    {
        TRPMResetTrap(pVCpu);
    }
    return rcStrict;
#endif
}


VMM_INT_DECL(int) IEMBreakpointSet(PVM pVM, RTGCPTR GCPtrBp)
{
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(int) IEMBreakpointClear(PVM pVM, RTGCPTR GCPtrBp)
{
    return VERR_NOT_IMPLEMENTED;
}


#if 0 /* The IRET-to-v8086 mode in PATM is very optimistic, so I don't dare do this yet. */
/**
 * Executes a IRET instruction with default operand size.
 *
 * This is for PATM.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure of the calling EMT.
 * @param   pCtxCore            The register frame.
 */
VMM_INT_DECL(int) IEMExecInstr_iret(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;
    PCPUMCTX pCtx    = pVCpu->iem.s.CTX_SUFF(pCtx);

    iemCtxCoreToCtx(pCtx, pCtxCore);
    iemInitDecoder(pIemCpu);
    VBOXSTRICTRC rcStrict = iemCImpl_iret(pIemCpu, 1, pIemCpu->enmDefOpSize);
    if (rcStrict == VINF_SUCCESS)
        iemCtxToCtxCore(pCtxCore, pCtx);
    else
        LogFlow(("IEMExecInstr_iret: cs:rip=%04x:%08RX64 ss:rsp=%04x:%08RX64 EFL=%06x - rcStrict=%Rrc\n",
                 pCtx->cs, pCtx->rip, pCtx->ss, pCtx->rsp, pCtx->eflags.u, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}
#endif


/**
 * Macro used by the IEMExec* method to check the given instruction length.
 *
 * Will return on failure!
 *
 * @param   a_cbInstr   The given instruction length.
 * @param   a_cbMin     The minimum length.
 */
#define IEMEXEC_ASSERT_INSTR_LEN_RETURN(a_cbInstr, a_cbMin) \
    AssertMsgReturn((unsigned)(a_cbInstr) - (unsigned)(a_cbMin) <= (unsigned)15 - (unsigned)(a_cbMin), \
                    ("cbInstr=%u cbMin=%u\n", (a_cbInstr), (a_cbMin)), VERR_IEM_INVALID_INSTR_LENGTH)


/**
 * Interface for HM and EM for executing string I/O OUT (write) instructions.
 *
 * This API ASSUMES that the caller has already verified that the guest code is
 * allowed to access the I/O port.  (The I/O port is in the DX register in the
 * guest state.)
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   cbValue             The size of the I/O port access (1, 2, or 4).
 * @param   enmAddrMode         The addressing mode.
 * @param   fRepPrefix          Indicates whether a repeat prefix is used
 *                              (doesn't matter which for this instruction).
 * @param   cbInstr             The instruction length in bytes.
 * @param   iEffSeg             The effective segment address.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecStringIoWrite(PVMCPU pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                                bool fRepPrefix, uint8_t cbInstr, uint8_t iEffSeg)
{
    AssertMsgReturn(iEffSeg < X86_SREG_COUNT, ("%#x\n", iEffSeg), VERR_IEM_INVALID_EFF_SEG);
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 1);

    /*
     * State init.
     */
    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);

    /*
     * Switch orgy for getting to the right handler.
     */
    VBOXSTRICTRC rcStrict;
    if (fRepPrefix)
    {
        switch (enmAddrMode)
        {
            case IEMMODE_16BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_rep_outs_op8_addr16(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_rep_outs_op16_addr16(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_rep_outs_op32_addr16(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_32BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_rep_outs_op8_addr32(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_rep_outs_op16_addr32(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_rep_outs_op32_addr32(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_64BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_rep_outs_op8_addr64(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_rep_outs_op16_addr64(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_rep_outs_op32_addr64(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            default:
                AssertMsgFailedReturn(("enmAddrMode=%d\n", enmAddrMode), VERR_IEM_INVALID_ADDRESS_MODE);
        }
    }
    else
    {
        switch (enmAddrMode)
        {
            case IEMMODE_16BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_outs_op8_addr16(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_outs_op16_addr16(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_outs_op32_addr16(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_32BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_outs_op8_addr32(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_outs_op16_addr32(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_outs_op32_addr32(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_64BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_outs_op8_addr64(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_outs_op16_addr64(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_outs_op32_addr64(pIemCpu, cbInstr, iEffSeg, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            default:
                AssertMsgFailedReturn(("enmAddrMode=%d\n", enmAddrMode), VERR_IEM_INVALID_ADDRESS_MODE);
        }
    }

    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}


/**
 * Interface for HM and EM for executing string I/O IN (read) instructions.
 *
 * This API ASSUMES that the caller has already verified that the guest code is
 * allowed to access the I/O port.  (The I/O port is in the DX register in the
 * guest state.)
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   cbValue             The size of the I/O port access (1, 2, or 4).
 * @param   enmAddrMode         The addressing mode.
 * @param   fRepPrefix          Indicates whether a repeat prefix is used
 *                              (doesn't matter which for this instruction).
 * @param   cbInstr             The instruction length in bytes.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecStringIoRead(PVMCPU pVCpu, uint8_t cbValue, IEMMODE enmAddrMode,
                                               bool fRepPrefix, uint8_t cbInstr)
{
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 1);

    /*
     * State init.
     */
    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);

    /*
     * Switch orgy for getting to the right handler.
     */
    VBOXSTRICTRC rcStrict;
    if (fRepPrefix)
    {
        switch (enmAddrMode)
        {
            case IEMMODE_16BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_rep_ins_op8_addr16(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_rep_ins_op16_addr16(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_rep_ins_op32_addr16(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_32BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_rep_ins_op8_addr32(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_rep_ins_op16_addr32(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_rep_ins_op32_addr32(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_64BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_rep_ins_op8_addr64(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_rep_ins_op16_addr64(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_rep_ins_op32_addr64(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            default:
                AssertMsgFailedReturn(("enmAddrMode=%d\n", enmAddrMode), VERR_IEM_INVALID_ADDRESS_MODE);
        }
    }
    else
    {
        switch (enmAddrMode)
        {
            case IEMMODE_16BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_ins_op8_addr16(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_ins_op16_addr16(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_ins_op32_addr16(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_32BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_ins_op8_addr32(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_ins_op16_addr32(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_ins_op32_addr32(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            case IEMMODE_64BIT:
                switch (cbValue)
                {
                    case 1: rcStrict = iemCImpl_ins_op8_addr64(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 2: rcStrict = iemCImpl_ins_op16_addr64(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    case 4: rcStrict = iemCImpl_ins_op32_addr64(pIemCpu, cbInstr, true /*fIoChecked*/); break;
                    default:
                        AssertMsgFailedReturn(("cbValue=%#x\n", cbValue), VERR_IEM_INVALID_OPERAND_SIZE);
                }
                break;

            default:
                AssertMsgFailedReturn(("enmAddrMode=%d\n", enmAddrMode), VERR_IEM_INVALID_ADDRESS_MODE);
        }
    }

    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}



/**
 * Interface for HM and EM to write to a CRx register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cbInstr     The instruction length in bytes.
 * @param   iCrReg      The control register number (destination).
 * @param   iGReg       The general purpose register number (source).
 *
 * @remarks In ring-0 not all of the state needs to be synced in.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecDecodedMovCRxWrite(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iCrReg, uint8_t iGReg)
{
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 2);
    Assert(iCrReg < 16);
    Assert(iGReg < 16);

    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);
    VBOXSTRICTRC rcStrict = IEM_CIMPL_CALL_2(iemCImpl_mov_Cd_Rd, iCrReg, iGReg);
    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}


/**
 * Interface for HM and EM to read from a CRx register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cbInstr     The instruction length in bytes.
 * @param   iGReg       The general purpose register number (destination).
 * @param   iCrReg      The control register number (source).
 *
 * @remarks In ring-0 not all of the state needs to be synced in.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecDecodedMovCRxRead(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iGReg, uint8_t iCrReg)
{
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 2);
    Assert(iCrReg < 16);
    Assert(iGReg < 16);

    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);
    VBOXSTRICTRC rcStrict = IEM_CIMPL_CALL_2(iemCImpl_mov_Rd_Cd, iGReg, iCrReg);
    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}


/**
 * Interface for HM and EM to clear the CR0[TS] bit.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cbInstr     The instruction length in bytes.
 *
 * @remarks In ring-0 not all of the state needs to be synced in.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecDecodedClts(PVMCPU pVCpu, uint8_t cbInstr)
{
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 2);

    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);
    VBOXSTRICTRC rcStrict = IEM_CIMPL_CALL_0(iemCImpl_clts);
    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}


/**
 * Interface for HM and EM to emulate the LMSW instruction (loads CR0).
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cbInstr     The instruction length in bytes.
 * @param   uValue      The value to load into CR0.
 *
 * @remarks In ring-0 not all of the state needs to be synced in.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecDecodedLmsw(PVMCPU pVCpu, uint8_t cbInstr, uint16_t uValue)
{
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 3);

    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);
    VBOXSTRICTRC rcStrict = IEM_CIMPL_CALL_1(iemCImpl_lmsw, uValue);
    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}


/**
 * Interface for HM and EM to emulate the XSETBV instruction (loads XCRx).
 *
 * Takes input values in ecx and edx:eax of the CPU context of the calling EMT.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   cbInstr     The instruction length in bytes.
 * @remarks In ring-0 not all of the state needs to be synced in.
 * @thread  EMT(pVCpu)
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMExecDecodedXsetbv(PVMCPU pVCpu, uint8_t cbInstr)
{
    IEMEXEC_ASSERT_INSTR_LEN_RETURN(cbInstr, 3);

    PIEMCPU pIemCpu = &pVCpu->iem.s;
    iemInitExec(pIemCpu, false /*fBypassHandlers*/);
    VBOXSTRICTRC rcStrict = IEM_CIMPL_CALL_0(iemCImpl_xsetbv);
    iemUninitExec(pIemCpu);
    return iemExecStatusCodeFiddling(pIemCpu, rcStrict);
}

#ifdef IN_RING3

/**
 * Called by force-flag handling code when VMCPU_FF_IEM is set.
 *
 * @returns Merge between @a rcStrict and what the commit operation returned.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   rcStrict    The status code returned by ring-0 or raw-mode.
 */
VMMR3_INT_DECL(VBOXSTRICTRC) IEMR3DoPendingAction(PVMCPU pVCpu, VBOXSTRICTRC rcStrict)
{
    PIEMCPU      pIemCpu = &pVCpu->iem.s;

    /*
     * Retrieve and reset the pending commit.
     */
    IEMCOMMIT const enmFn = pIemCpu->PendingCommit.enmFn;
    pIemCpu->PendingCommit.enmFn = IEMCOMMIT_INVALID;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_IEM);

    /*
     * Must reset pass-up status code.
     */
    pIemCpu->rcPassUp = VINF_SUCCESS;

    /*
     * Call the function.  Currently using switch here instead of function
     * pointer table as a switch won't get skewed.
     */
    VBOXSTRICTRC rcStrictCommit;
    switch (enmFn)
    {
        case IEMCOMMIT_INS_OP8_ADDR16:          rcStrictCommit = iemR3CImpl_commit_ins_op8_addr16(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP8_ADDR32:          rcStrictCommit = iemR3CImpl_commit_ins_op8_addr32(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP8_ADDR64:          rcStrictCommit = iemR3CImpl_commit_ins_op8_addr64(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP16_ADDR16:         rcStrictCommit = iemR3CImpl_commit_ins_op16_addr16(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP16_ADDR32:         rcStrictCommit = iemR3CImpl_commit_ins_op16_addr32(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP16_ADDR64:         rcStrictCommit = iemR3CImpl_commit_ins_op16_addr64(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP32_ADDR16:         rcStrictCommit = iemR3CImpl_commit_ins_op32_addr16(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP32_ADDR32:         rcStrictCommit = iemR3CImpl_commit_ins_op32_addr32(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_INS_OP32_ADDR64:         rcStrictCommit = iemR3CImpl_commit_ins_op32_addr64(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP8_ADDR16:      rcStrictCommit = iemR3CImpl_commit_rep_ins_op8_addr16(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP8_ADDR32:      rcStrictCommit = iemR3CImpl_commit_rep_ins_op8_addr32(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP8_ADDR64:      rcStrictCommit = iemR3CImpl_commit_rep_ins_op8_addr64(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP16_ADDR16:     rcStrictCommit = iemR3CImpl_commit_rep_ins_op16_addr16(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP16_ADDR32:     rcStrictCommit = iemR3CImpl_commit_rep_ins_op16_addr32(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP16_ADDR64:     rcStrictCommit = iemR3CImpl_commit_rep_ins_op16_addr64(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP32_ADDR16:     rcStrictCommit = iemR3CImpl_commit_rep_ins_op32_addr16(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP32_ADDR32:     rcStrictCommit = iemR3CImpl_commit_rep_ins_op32_addr32(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        case IEMCOMMIT_REP_INS_OP32_ADDR64:     rcStrictCommit = iemR3CImpl_commit_rep_ins_op32_addr64(pIemCpu, pIemCpu->PendingCommit.cbInstr); break;
        default:
            AssertLogRelMsgFailedReturn(("enmFn=%#x (%d)\n", pIemCpu->PendingCommit.enmFn, pIemCpu->PendingCommit.enmFn), VERR_IEM_IPE_2);
    }

    /*
     * Merge status code (if any) with the incomming one.
     */
    rcStrictCommit = iemExecStatusCodeFiddling(pIemCpu, rcStrictCommit);
    if (RT_LIKELY(rcStrictCommit == VINF_SUCCESS))
        return rcStrict;
    if (RT_LIKELY(rcStrict == VINF_SUCCESS || rcStrict == VINF_EM_RAW_TO_R3))
        return rcStrictCommit;

    /* Complicated. */
    if (RT_FAILURE(rcStrict))
        return rcStrict;
    if (RT_FAILURE(rcStrictCommit))
        return rcStrictCommit;
    if (   rcStrict >= VINF_EM_FIRST
        && rcStrict <= VINF_EM_LAST)
    {
        if (   rcStrictCommit >= VINF_EM_FIRST
            && rcStrictCommit <= VINF_EM_LAST)
            return rcStrict < rcStrictCommit ? rcStrict : rcStrictCommit;

        /* This really shouldn't happen. Check PGM + handler code! */
        AssertLogRelMsgFailedReturn(("rcStrictCommit=%Rrc rcStrict=%Rrc enmFn=%d\n", VBOXSTRICTRC_VAL(rcStrictCommit), VBOXSTRICTRC_VAL(rcStrict), enmFn), VERR_IEM_IPE_1);
    }
    /* This shouldn't really happen either, see IOM_SUCCESS. */
    AssertLogRelMsgFailedReturn(("rcStrictCommit=%Rrc rcStrict=%Rrc enmFn=%d\n", VBOXSTRICTRC_VAL(rcStrictCommit), VBOXSTRICTRC_VAL(rcStrict), enmFn), VERR_IEM_IPE_2);
}

#endif /* IN_RING */

