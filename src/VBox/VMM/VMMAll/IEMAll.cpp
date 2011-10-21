/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - All Contexts.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP   LOG_GROUP_IEM
#include <VBox/vmm/iem.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/dbgf.h>
#ifdef IEM_VERIFICATION_MODE
# include <VBox/vmm/rem.h>
# include <VBox/vmm/mm.h>
#endif
#include "IEMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/x86.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Generic pointer union.
 * @todo move me to iprt/types.h
 */
typedef union RTPTRUNION
{
    /** Pointer into the void... */
    void        *pv;
    /** Pointer to a 8-bit unsigned value. */
    uint8_t     *pu8;
    /** Pointer to a 16-bit unsigned value. */
    uint16_t    *pu16;
    /** Pointer to a 32-bit unsigned value. */
    uint32_t    *pu32;
    /** Pointer to a 64-bit unsigned value. */
    uint64_t    *pu64;
} RTPTRUNION;
/** Pointer to a pointer union. */
typedef RTPTRUNION *PRTPTRUNION;

/**
 * Generic const pointer union.
 * @todo move me to iprt/types.h
 */
typedef union RTCPTRUNION
{
    /** Pointer into the void... */
    void const       *pv;
    /** Pointer to a 8-bit unsigned value. */
    uint8_t const     *pu8;
    /** Pointer to a 16-bit unsigned value. */
    uint16_t const    *pu16;
    /** Pointer to a 32-bit unsigned value. */
    uint32_t const    *pu32;
    /** Pointer to a 64-bit unsigned value. */
    uint64_t const    *pu64;
} RTCPTRUNION;
/** Pointer to a const pointer union. */
typedef RTCPTRUNION *PRTCPTRUNION;

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
    static VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name (PIEMCPU pIemCpu)
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    static VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0)
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    static VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1)

#elif defined(_MSC_VER) && defined(RT_ARCH_X86)
typedef VBOXSTRICTRC (__fastcall * PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    static /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu) RT_NO_THROW
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    static /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0) RT_NO_THROW
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    static /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW

#elif defined(__GNUC__)
typedef VBOXSTRICTRC (* PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    static VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PIEMCPU pIemCpu)
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    static VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0)
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    static VBOXSTRICTRC __attribute__((__nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1)

#else
typedef VBOXSTRICTRC (* PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    static VBOXSTRICTRC a_Name(PIEMCPU pIemCpu) RT_NO_THROW
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    static VBOXSTRICTRC a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0) RT_NO_THROW
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    static VBOXSTRICTRC a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW

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


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name IEM status codes.
 *
 * Not quite sure how this will play out in the end, just aliasing safe status
 * codes for now.
 *
 * @{ */
#define VINF_IEM_RAISED_XCPT    VINF_EM_RESCHEDULE
/** @} */

/** Temporary hack to disable the double execution.  Will be removed in favor
 * of a dedicated execution mode in EM. */
//#define IEM_VERIFICATION_MODE_NO_REM

/** Used to shut up GCC warnings about variables that 'may be used uninitialized'
 * due to GCC lacking knowledge about the value range of a switch. */
#define IEM_NOT_REACHED_DEFAULT_CASE_RET() default: AssertFailedReturn(VERR_INTERNAL_ERROR_4)

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
 * Tests if an AMD CPUID feature (extended) is marked present - ECX.
 */
#define IEM_IS_AMD_CPUID_FEATURE_PRESENT_ECX(a_fEcx)    iemRegIsAmdCpuIdFeaturePresent(pIemCpu, 0, (a_fEcx))

/**
 * Checks if a intel CPUID feature is present.
 */
#define IEM_IS_INTEL_CPUID_FEATURE_PRESENT_EDX(a_fEdx)  \
    (   ((a_fEdx) & (X86_CPUID_FEATURE_EDX_TSC | 0)) \
     || iemRegIsIntelCpuIdFeaturePresent(pIemCpu, (a_fEdx), 0) )

/**
 * Check if the address is canonical.
 */
#define IEM_IS_CANONICAL(a_u64Addr)         ((uint64_t)(a_u64Addr) + UINT64_C(0x800000000000) < UINT64_C(0x1000000000000))


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
extern const PFNIEMOP g_apfnOneByteMap[256]; /* not static since we need to forward declare it. */


/** Function table for the ADD instruction. */
static const IEMOPBINSIZES g_iemAImpl_add =
{
    iemAImpl_add_u8,  iemAImpl_add_u8_locked,
    iemAImpl_add_u16, iemAImpl_add_u16_locked,
    iemAImpl_add_u32, iemAImpl_add_u32_locked,
    iemAImpl_add_u64, iemAImpl_add_u64_locked
};

/** Function table for the ADC instruction. */
static const IEMOPBINSIZES g_iemAImpl_adc =
{
    iemAImpl_adc_u8,  iemAImpl_adc_u8_locked,
    iemAImpl_adc_u16, iemAImpl_adc_u16_locked,
    iemAImpl_adc_u32, iemAImpl_adc_u32_locked,
    iemAImpl_adc_u64, iemAImpl_adc_u64_locked
};

/** Function table for the SUB instruction. */
static const IEMOPBINSIZES g_iemAImpl_sub =
{
    iemAImpl_sub_u8,  iemAImpl_sub_u8_locked,
    iemAImpl_sub_u16, iemAImpl_sub_u16_locked,
    iemAImpl_sub_u32, iemAImpl_sub_u32_locked,
    iemAImpl_sub_u64, iemAImpl_sub_u64_locked
};

/** Function table for the SBB instruction. */
static const IEMOPBINSIZES g_iemAImpl_sbb =
{
    iemAImpl_sbb_u8,  iemAImpl_sbb_u8_locked,
    iemAImpl_sbb_u16, iemAImpl_sbb_u16_locked,
    iemAImpl_sbb_u32, iemAImpl_sbb_u32_locked,
    iemAImpl_sbb_u64, iemAImpl_sbb_u64_locked
};

/** Function table for the OR instruction. */
static const IEMOPBINSIZES g_iemAImpl_or =
{
    iemAImpl_or_u8,  iemAImpl_or_u8_locked,
    iemAImpl_or_u16, iemAImpl_or_u16_locked,
    iemAImpl_or_u32, iemAImpl_or_u32_locked,
    iemAImpl_or_u64, iemAImpl_or_u64_locked
};

/** Function table for the XOR instruction. */
static const IEMOPBINSIZES g_iemAImpl_xor =
{
    iemAImpl_xor_u8,  iemAImpl_xor_u8_locked,
    iemAImpl_xor_u16, iemAImpl_xor_u16_locked,
    iemAImpl_xor_u32, iemAImpl_xor_u32_locked,
    iemAImpl_xor_u64, iemAImpl_xor_u64_locked
};

/** Function table for the AND instruction. */
static const IEMOPBINSIZES g_iemAImpl_and =
{
    iemAImpl_and_u8,  iemAImpl_and_u8_locked,
    iemAImpl_and_u16, iemAImpl_and_u16_locked,
    iemAImpl_and_u32, iemAImpl_and_u32_locked,
    iemAImpl_and_u64, iemAImpl_and_u64_locked
};

/** Function table for the CMP instruction.
 * @remarks Making operand order ASSUMPTIONS.
 */
static const IEMOPBINSIZES g_iemAImpl_cmp =
{
    iemAImpl_cmp_u8,  NULL,
    iemAImpl_cmp_u16, NULL,
    iemAImpl_cmp_u32, NULL,
    iemAImpl_cmp_u64, NULL
};

/** Function table for the TEST instruction.
 * @remarks Making operand order ASSUMPTIONS.
 */
static const IEMOPBINSIZES g_iemAImpl_test =
{
    iemAImpl_test_u8,  NULL,
    iemAImpl_test_u16, NULL,
    iemAImpl_test_u32, NULL,
    iemAImpl_test_u64, NULL
};

/** Function table for the BT instruction. */
static const IEMOPBINSIZES g_iemAImpl_bt =
{
    NULL,  NULL,
    iemAImpl_bt_u16, NULL,
    iemAImpl_bt_u32, NULL,
    iemAImpl_bt_u64, NULL
};

/** Function table for the BTC instruction. */
static const IEMOPBINSIZES g_iemAImpl_btc =
{
    NULL,  NULL,
    iemAImpl_btc_u16, iemAImpl_btc_u16_locked,
    iemAImpl_btc_u32, iemAImpl_btc_u32_locked,
    iemAImpl_btc_u64, iemAImpl_btc_u64_locked
};

/** Function table for the BTR instruction. */
static const IEMOPBINSIZES g_iemAImpl_btr =
{
    NULL,  NULL,
    iemAImpl_btr_u16, iemAImpl_btr_u16_locked,
    iemAImpl_btr_u32, iemAImpl_btr_u32_locked,
    iemAImpl_btr_u64, iemAImpl_btr_u64_locked
};

/** Function table for the BTS instruction. */
static const IEMOPBINSIZES g_iemAImpl_bts =
{
    NULL,  NULL,
    iemAImpl_bts_u16, iemAImpl_bts_u16_locked,
    iemAImpl_bts_u32, iemAImpl_bts_u32_locked,
    iemAImpl_bts_u64, iemAImpl_bts_u64_locked
};

/** Function table for the BSF instruction. */
static const IEMOPBINSIZES g_iemAImpl_bsf =
{
    NULL,  NULL,
    iemAImpl_bsf_u16, NULL,
    iemAImpl_bsf_u32, NULL,
    iemAImpl_bsf_u64, NULL
};

/** Function table for the BSR instruction. */
static const IEMOPBINSIZES g_iemAImpl_bsr =
{
    NULL,  NULL,
    iemAImpl_bsr_u16, NULL,
    iemAImpl_bsr_u32, NULL,
    iemAImpl_bsr_u64, NULL
};

/** Function table for the IMUL instruction. */
static const IEMOPBINSIZES g_iemAImpl_imul_two =
{
    NULL,  NULL,
    iemAImpl_imul_two_u16, NULL,
    iemAImpl_imul_two_u32, NULL,
    iemAImpl_imul_two_u64, NULL
};

/** Group 1 /r lookup table. */
static const PCIEMOPBINSIZES g_apIemImplGrp1[8] =
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
static const IEMOPUNARYSIZES g_iemAImpl_inc =
{
    iemAImpl_inc_u8,  iemAImpl_inc_u8_locked,
    iemAImpl_inc_u16, iemAImpl_inc_u16_locked,
    iemAImpl_inc_u32, iemAImpl_inc_u32_locked,
    iemAImpl_inc_u64, iemAImpl_inc_u64_locked
};

/** Function table for the DEC instruction. */
static const IEMOPUNARYSIZES g_iemAImpl_dec =
{
    iemAImpl_dec_u8,  iemAImpl_dec_u8_locked,
    iemAImpl_dec_u16, iemAImpl_dec_u16_locked,
    iemAImpl_dec_u32, iemAImpl_dec_u32_locked,
    iemAImpl_dec_u64, iemAImpl_dec_u64_locked
};

/** Function table for the NEG instruction. */
static const IEMOPUNARYSIZES g_iemAImpl_neg =
{
    iemAImpl_neg_u8,  iemAImpl_neg_u8_locked,
    iemAImpl_neg_u16, iemAImpl_neg_u16_locked,
    iemAImpl_neg_u32, iemAImpl_neg_u32_locked,
    iemAImpl_neg_u64, iemAImpl_neg_u64_locked
};

/** Function table for the NOT instruction. */
static const IEMOPUNARYSIZES g_iemAImpl_not =
{
    iemAImpl_not_u8,  iemAImpl_not_u8_locked,
    iemAImpl_not_u16, iemAImpl_not_u16_locked,
    iemAImpl_not_u32, iemAImpl_not_u32_locked,
    iemAImpl_not_u64, iemAImpl_not_u64_locked
};


/** Function table for the ROL instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_rol =
{
    iemAImpl_rol_u8,
    iemAImpl_rol_u16,
    iemAImpl_rol_u32,
    iemAImpl_rol_u64
};

/** Function table for the ROR instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_ror =
{
    iemAImpl_ror_u8,
    iemAImpl_ror_u16,
    iemAImpl_ror_u32,
    iemAImpl_ror_u64
};

/** Function table for the RCL instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_rcl =
{
    iemAImpl_rcl_u8,
    iemAImpl_rcl_u16,
    iemAImpl_rcl_u32,
    iemAImpl_rcl_u64
};

/** Function table for the RCR instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_rcr =
{
    iemAImpl_rcr_u8,
    iemAImpl_rcr_u16,
    iemAImpl_rcr_u32,
    iemAImpl_rcr_u64
};

/** Function table for the SHL instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_shl =
{
    iemAImpl_shl_u8,
    iemAImpl_shl_u16,
    iemAImpl_shl_u32,
    iemAImpl_shl_u64
};

/** Function table for the SHR instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_shr =
{
    iemAImpl_shr_u8,
    iemAImpl_shr_u16,
    iemAImpl_shr_u32,
    iemAImpl_shr_u64
};

/** Function table for the SAR instruction. */
static const IEMOPSHIFTSIZES g_iemAImpl_sar =
{
    iemAImpl_sar_u8,
    iemAImpl_sar_u16,
    iemAImpl_sar_u32,
    iemAImpl_sar_u64
};


/** Function table for the MUL instruction. */
static const IEMOPMULDIVSIZES g_iemAImpl_mul =
{
    iemAImpl_mul_u8,
    iemAImpl_mul_u16,
    iemAImpl_mul_u32,
    iemAImpl_mul_u64
};

/** Function table for the IMUL instruction working implicitly on rAX. */
static const IEMOPMULDIVSIZES g_iemAImpl_imul =
{
    iemAImpl_imul_u8,
    iemAImpl_imul_u16,
    iemAImpl_imul_u32,
    iemAImpl_imul_u64
};

/** Function table for the DIV instruction. */
static const IEMOPMULDIVSIZES g_iemAImpl_div =
{
    iemAImpl_div_u8,
    iemAImpl_div_u16,
    iemAImpl_div_u32,
    iemAImpl_div_u64
};

/** Function table for the MUL instruction. */
static const IEMOPMULDIVSIZES g_iemAImpl_idiv =
{
    iemAImpl_idiv_u8,
    iemAImpl_idiv_u16,
    iemAImpl_idiv_u32,
    iemAImpl_idiv_u64
};

/** Function table for the SHLD instruction */
static const IEMOPSHIFTDBLSIZES g_iemAImpl_shld =
{
    iemAImpl_shld_u16,
    iemAImpl_shld_u32,
    iemAImpl_shld_u64,
};

/** Function table for the SHRD instruction */
static const IEMOPSHIFTDBLSIZES g_iemAImpl_shrd =
{
    iemAImpl_shrd_u16,
    iemAImpl_shrd_u32,
    iemAImpl_shrd_u64,
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static VBOXSTRICTRC     iemRaiseTaskSwitchFaultCurrentTSS(PIEMCPU pIemCpu);
/*static VBOXSTRICTRC     iemRaiseSelectorNotPresent(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);*/
static VBOXSTRICTRC     iemRaiseSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel);
static VBOXSTRICTRC     iemRaiseSelectorNotPresentWithErr(PIEMCPU pIemCpu, uint16_t uErr);
static VBOXSTRICTRC     iemRaiseGeneralProtectionFault(PIEMCPU pIemCpu, uint16_t uErr);
static VBOXSTRICTRC     iemRaiseGeneralProtectionFault0(PIEMCPU pIemCpu);
static VBOXSTRICTRC     iemRaiseGeneralProtectionFaultBySelector(PIEMCPU pIemCpu, RTSEL uSel);
static VBOXSTRICTRC     iemRaiseSelectorBounds(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
static VBOXSTRICTRC     iemRaiseSelectorBoundsBySelector(PIEMCPU pIemCpu, RTSEL Sel);
static VBOXSTRICTRC     iemRaiseSelectorInvalidAccess(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
static VBOXSTRICTRC     iemRaisePageFault(PIEMCPU pIemCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc);
static VBOXSTRICTRC     iemMemMap(PIEMCPU pIemCpu, void **ppvMem, size_t cbMem, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t fAccess);
static VBOXSTRICTRC     iemMemCommitAndUnmap(PIEMCPU pIemCpu, void *pvMem, uint32_t fAccess);
static VBOXSTRICTRC     iemMemFetchDataU32(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
static VBOXSTRICTRC     iemMemFetchDataU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem);
static VBOXSTRICTRC     iemMemFetchSelDesc(PIEMCPU pIemCpu, PIEMSELDESC pDesc, uint16_t uSel);
static VBOXSTRICTRC     iemMemStackPushCommitSpecial(PIEMCPU pIemCpu, void *pvMem, uint64_t uNewRsp);
static VBOXSTRICTRC     iemMemStackPushBeginSpecial(PIEMCPU pIemCpu, size_t cbMem, void **ppvMem, uint64_t *puNewRsp);
static VBOXSTRICTRC     iemMemMarkSelDescAccessed(PIEMCPU pIemCpu, uint16_t uSel);
static uint16_t         iemSRegFetchU16(PIEMCPU pIemCpu, uint8_t iSegReg);

#ifdef IEM_VERIFICATION_MODE
static PIEMVERIFYEVTREC iemVerifyAllocRecord(PIEMCPU pIemCpu);
#endif
static VBOXSTRICTRC     iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue);
static VBOXSTRICTRC     iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue);


/**
 * Initializes the decoder state.
 *
 * @param   pIemCpu             The per CPU IEM state.
 */
DECLINLINE(void) iemInitDecode(PIEMCPU pIemCpu)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    pIemCpu->uCpl               = CPUMGetGuestCPL(IEMCPU_TO_VMCPU(pIemCpu), CPUMCTX2CORE(pCtx));
    IEMMODE enmMode = CPUMIsGuestIn64BitCodeEx(pCtx)
                    ? IEMMODE_64BIT
                    : pCtx->csHid.Attr.n.u1DefBig /** @todo check if this is correct... */
                    ? IEMMODE_32BIT
                    : IEMMODE_16BIT;
    pIemCpu->enmCpuMode         = enmMode;
    pIemCpu->enmDefAddrMode     = enmMode;  /** @todo check if this is correct... */
    pIemCpu->enmEffAddrMode     = enmMode;
    pIemCpu->enmDefOpSize       = enmMode;  /** @todo check if this is correct... */
    pIemCpu->enmEffOpSize       = enmMode;
    pIemCpu->fPrefixes          = 0;
    pIemCpu->uRexReg            = 0;
    pIemCpu->uRexB              = 0;
    pIemCpu->uRexIndex          = 0;
    pIemCpu->iEffSeg            = X86_SREG_DS;
    pIemCpu->offOpcode          = 0;
    pIemCpu->cbOpcode           = 0;
    pIemCpu->cActiveMappings    = 0;
    pIemCpu->iNextMapping       = 0;
}


/**
 * Prefetch opcodes the first time when starting executing.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 */
static VBOXSTRICTRC iemInitDecoderAndPrefetchOpcodes(PIEMCPU pIemCpu)
{
#ifdef IEM_VERIFICATION_MODE
    uint8_t const cbOldOpcodes = pIemCpu->cbOpcode;
#endif
    iemInitDecode(pIemCpu);

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
        Assert(!(GCPtrPC32 & ~(uint32_t)UINT16_MAX) || pIemCpu->enmCpuMode == IEMMODE_32BIT);
        if (GCPtrPC32 > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        cbToTryRead = pCtx->csHid.u32Limit - GCPtrPC32 + 1;
        GCPtrPC = pCtx->csHid.u64Base + GCPtrPC32;
    }

    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int rc = PGMGstGetPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrPC, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
        return iemRaisePageFault(pIemCpu, GCPtrPC, IEM_ACCESS_INSTRUCTION, rc);
    if ((fFlags & X86_PTE_US) && pIemCpu->uCpl == 2)
        return iemRaisePageFault(pIemCpu, GCPtrPC, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    if ((fFlags & X86_PTE_PAE_NX) && (pCtx->msrEFER & MSR_K6_EFER_NXE))
        return iemRaisePageFault(pIemCpu, GCPtrPC, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    GCPhys |= GCPtrPC & PAGE_OFFSET_MASK;
    /** @todo Check reserved bits and such stuff. PGM is better at doing
     *        that, so do it when implementing the guest virtual address
     *        TLB... */

#ifdef IEM_VERIFICATION_MODE
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
        memmove(&pIemCpu->abOpcode[0], &pIemCpu->abOpcode[offPrevOpcodes], cbNew);
        pIemCpu->cbOpcode = cbNew;
        return VINF_SUCCESS;
    }
#endif

    /*
     * Read the bytes at this address.
     */
    uint32_t cbLeftOnPage = PAGE_SIZE - (GCPtrPC & PAGE_OFFSET_MASK);
    if (cbToTryRead > cbLeftOnPage)
        cbToTryRead = cbLeftOnPage;
    if (cbToTryRead > sizeof(pIemCpu->abOpcode))
        cbToTryRead = sizeof(pIemCpu->abOpcode);
    /** @todo patch manager */
    if (!pIemCpu->fByPassHandlers)
        rc = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhys, pIemCpu->abOpcode, cbToTryRead);
    else
        rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), pIemCpu->abOpcode, GCPhys, cbToTryRead);
    if (rc != VINF_SUCCESS)
        return rc;
    pIemCpu->cbOpcode = cbToTryRead;

    return VINF_SUCCESS;
}


/**
 * Try fetch at least @a cbMin bytes more opcodes, raise the appropriate
 * exception if it fails.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   cbMin               Where to return the opcode byte.
 */
static VBOXSTRICTRC iemOpcodeFetchMoreBytes(PIEMCPU pIemCpu, size_t cbMin)
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
        cbToTryRead = PAGE_SIZE - (GCPtrNext & PAGE_OFFSET_MASK);
        Assert(cbToTryRead >= cbMin - cbLeft); /* ASSUMPTION based on iemInitDecoderAndPrefetchOpcodes. */
    }
    else
    {
        uint32_t GCPtrNext32 = pCtx->eip;
        Assert(!(GCPtrNext32 & ~(uint32_t)UINT16_MAX) || pIemCpu->enmCpuMode == IEMMODE_32BIT);
        GCPtrNext32 += pIemCpu->cbOpcode;
        if (GCPtrNext32 > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        cbToTryRead = pCtx->csHid.u32Limit - GCPtrNext32 + 1;
        if (cbToTryRead < cbMin - cbLeft)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        GCPtrNext = pCtx->csHid.u64Base + GCPtrNext32;
    }

    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int rc = PGMGstGetPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrNext, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
        return iemRaisePageFault(pIemCpu, GCPtrNext, IEM_ACCESS_INSTRUCTION, rc);
    if ((fFlags & X86_PTE_US) && pIemCpu->uCpl == 2)
        return iemRaisePageFault(pIemCpu, GCPtrNext, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    if ((fFlags & X86_PTE_PAE_NX) && (pCtx->msrEFER & MSR_K6_EFER_NXE))
        return iemRaisePageFault(pIemCpu, GCPtrNext, IEM_ACCESS_INSTRUCTION, VERR_ACCESS_DENIED);
    GCPhys |= GCPtrNext & PAGE_OFFSET_MASK;
    //Log(("GCPtrNext=%RGv GCPhys=%RGp cbOpcodes=%#x\n",  GCPtrNext,  GCPhys,  pIemCpu->cbOpcode));
    /** @todo Check reserved bits and such stuff. PGM is better at doing
     *        that, so do it when implementing the guest virtual address
     *        TLB... */

    /*
     * Read the bytes at this address.
     */
    uint32_t cbLeftOnPage = PAGE_SIZE - (GCPtrNext & PAGE_OFFSET_MASK);
    if (cbToTryRead > cbLeftOnPage)
        cbToTryRead = cbLeftOnPage;
    if (cbToTryRead > sizeof(pIemCpu->abOpcode) - pIemCpu->cbOpcode)
        cbToTryRead = sizeof(pIemCpu->abOpcode) - pIemCpu->cbOpcode;
    Assert(cbToTryRead >= cbMin - cbLeft);
    if (!pIemCpu->fByPassHandlers)
        rc = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhys, &pIemCpu->abOpcode[pIemCpu->cbOpcode], cbToTryRead);
    else
        rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), &pIemCpu->abOpcode[pIemCpu->cbOpcode], GCPhys, cbToTryRead);
    if (rc != VINF_SUCCESS)
        return rc;
    pIemCpu->cbOpcode += cbToTryRead;
    //Log(("%.*Rhxs\n", pIemCpu->cbOpcode, pIemCpu->abOpcode));

    return VINF_SUCCESS;
}


/**
 * Deals with the problematic cases that iemOpcodeGetNextByte doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pb                  Where to return the opcode byte.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemOpcodeGetNextByteSlow(PIEMCPU pIemCpu, uint8_t *pb)
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
 * Deals with the problematic cases that iemOpcodeGetNextS8SxU16 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the opcode dword.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemOpcodeGetNextS8SxU16Slow(PIEMCPU pIemCpu, uint16_t *pu16)
{
    uint8_t     u8;
    VBOXSTRICTRC rcStrict = iemOpcodeGetNextByteSlow(pIemCpu, &u8);
    if (rcStrict == VINF_SUCCESS)
        *pu16 = (int8_t)u8;
    return rcStrict;
}


/**
 * Deals with the problematic cases that iemOpcodeGetNextU16 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the opcode word.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemOpcodeGetNextU16Slow(PIEMCPU pIemCpu, uint16_t *pu16)
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
 * Deals with the problematic cases that iemOpcodeGetNextU32 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu32                Where to return the opcode dword.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemOpcodeGetNextU32Slow(PIEMCPU pIemCpu, uint32_t *pu32)
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
 * Deals with the problematic cases that iemOpcodeGetNextS32SxU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode qword.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemOpcodeGetNextS32SxU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
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
 * Deals with the problematic cases that iemOpcodeGetNextU64 doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu64                Where to return the opcode qword.
 */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemOpcodeGetNextU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
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
 * Fetches the next opcode byte.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pu8                 Where to return the opcode byte.
 */
DECLINLINE(VBOXSTRICTRC) iemOpcodeGetNextU8(PIEMCPU pIemCpu, uint8_t *pu8)
{
    uint8_t const offOpcode = pIemCpu->offOpcode;
    if (RT_UNLIKELY(offOpcode >= pIemCpu->cbOpcode))
        return iemOpcodeGetNextByteSlow(pIemCpu, pu8);

    *pu8 = pIemCpu->abOpcode[offOpcode];
    pIemCpu->offOpcode = offOpcode + 1;
    return VINF_SUCCESS;
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
 * @param   pi8                 Where to return the signed byte.
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
 * @param   pu16                Where to return the word.
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
 * @param   pi16                Where to return the signed word.
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
 * @param   a_u32               Where to return the opcode dword.
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
 * @param   pi32                Where to return the signed double word.
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
static VBOXSTRICTRC iemMiscValidateNewSS(PIEMCPU pIemCpu, PCCPUMCTX pCtx, RTSEL NewSS, uint8_t uCpl, PIEMSELDESC pDesc)
{
    /* Null selectors are not allowed (we're not called for dispatching
       interrupts with SS=0 in long mode). */
    if (!(NewSS & (X86_SEL_MASK | X86_SEL_LDT)))
    {
        Log(("iemMiscValidateNewSSandRsp: #x - null selector -> #GP(0)\n", NewSS));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /*
     * Read the descriptor.
     */
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, pDesc, NewSS);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /*
     * Perform the descriptor validation documented for LSS, POP SS and MOV SS.
     */
    if (!pDesc->Legacy.Gen.u1DescType)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - system selector -> #GP\n", NewSS, pDesc->Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, NewSS);
    }

    if (   (pDesc->Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
        || !(pDesc->Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - code or read only (%#x) -> #GP\n", NewSS, pDesc->Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, NewSS);
    }
    if (    (pDesc->Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
        || !(pDesc->Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - code or read only (%#x) -> #GP\n", NewSS, pDesc->Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, NewSS);
    }
    /** @todo testcase: check if the TSS.ssX RPL is checked. */
    if ((NewSS & X86_SEL_RPL) != uCpl)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - RPL and CPL (%d) differs -> #GP\n", NewSS, uCpl));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, NewSS);
    }
    if (pDesc->Legacy.Gen.u2Dpl != uCpl)
    {
        Log(("iemMiscValidateNewSSandRsp: %#x - DPL (%d) and CPL (%d) differs -> #GP\n", NewSS, pDesc->Legacy.Gen.u2Dpl, uCpl));
        return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, NewSS);
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
/** Software interrupt (int, into or bound). */
#define IEM_XCPT_FLAGS_T_SOFT_INT       RT_BIT_32(2)
/** Takes an error code. */
#define IEM_XCPT_FLAGS_ERR              RT_BIT_32(3)
/** Takes a CR2. */
#define IEM_XCPT_FLAGS_CR2              RT_BIT_32(4)
/** Generated by the breakpoint instruction. */
#define IEM_XCPT_FLAGS_BP_INSTR         RT_BIT_32(5)
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
static VBOXSTRICTRC iemRaiseLoadStackFromTss32Or16(PIEMCPU pIemCpu, PCCPUMCTX pCtx, uint8_t uCpl,
                                                   PRTSEL pSelSS, uint32_t *puEsp)
{
    VBOXSTRICTRC rcStrict;
    Assert(uCpl < 4);
    *puEsp  = 0; /* make gcc happy */
    *pSelSS = 0; /* make gcc happy */

    switch (pCtx->trHid.Attr.n.u4Type)
    {
        /*
         * 16-bit TSS (X86TSS16).
         */
        case X86_SEL_TYPE_SYS_286_TSS_AVAIL: AssertFailed();
        case X86_SEL_TYPE_SYS_286_TSS_BUSY:
        {
            uint32_t off = uCpl * 4 + 2;
            if (off + 4 > pCtx->trHid.u32Limit)
            {
                Log(("LoadStackFromTss32Or16: out of bounds! uCpl=%d, u32Limit=%#x TSS16\n", uCpl, pCtx->trHid.u32Limit));
                return iemRaiseTaskSwitchFaultCurrentTSS(pIemCpu);
            }

            uint32_t u32Tmp;
            rcStrict = iemMemFetchDataU32(pIemCpu, &u32Tmp, UINT8_MAX, pCtx->trHid.u64Base + off);
            if (rcStrict == VINF_SUCCESS)
            {
                *puEsp  = RT_LOWORD(u32Tmp);
                *pSelSS = RT_HIWORD(u32Tmp);
                return VINF_SUCCESS;
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
            if (off + 7 > pCtx->trHid.u32Limit)
            {
                Log(("LoadStackFromTss32Or16: out of bounds! uCpl=%d, u32Limit=%#x TSS16\n", uCpl, pCtx->trHid.u32Limit));
                return iemRaiseTaskSwitchFaultCurrentTSS(pIemCpu);
            }

            uint64_t u64Tmp;
            rcStrict = iemMemFetchDataU64(pIemCpu, &u64Tmp, UINT8_MAX, pCtx->trHid.u64Base + off);
            if (rcStrict == VINF_SUCCESS)
            {
                *puEsp  = u64Tmp & UINT32_MAX;
                *pSelSS = (RTSEL)(u64Tmp >> 32);
                return VINF_SUCCESS;
            }
            break;
        }

        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_2);
    }
    return rcStrict;
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
static VBOXSTRICTRC
iemRaiseXcptOrIntInRealMode(PIEMCPU     pIemCpu,
                             PCPUMCTX    pCtx,
                             uint8_t     cbInstr,
                             uint8_t     u8Vector,
                             uint32_t    fFlags,
                             uint16_t    uErr,
                             uint64_t    uCr2)
{
    AssertReturn(pIemCpu->enmCpuMode == IEMMODE_16BIT, VERR_INTERNAL_ERROR_3);

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

    pu16Frame[2] = (uint16_t)pCtx->eflags.u;
    pu16Frame[1] = (uint16_t)pCtx->cs;
    pu16Frame[0] = pCtx->ip + cbInstr;
    rcStrict = iemMemStackPushCommitSpecial(pIemCpu, pu16Frame, uNewRsp);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        return rcStrict;

    /*
     * Load the vector address into cs:ip and make exception specific state
     * adjustments.
     */
    pCtx->cs               = Idte.sel;
    pCtx->csHid.u64Base    = (uint32_t)Idte.sel << 4;
    /** @todo do we load attribs and limit as well? Should we check against limit like far jump? */
    pCtx->rip              = Idte.off;
    pCtx->eflags.Bits.u1IF = 0;

    /** @todo do we actually do this in real mode? */
    if (fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT)
        iemRaiseXcptAdjustState(pCtx, u8Vector);

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
static VBOXSTRICTRC
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
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pIemCpu, &Idte.u, UINT8_MAX,
                                               pCtx->idtr.pIdt + UINT32_C(8) * u8Vector);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        return rcStrict;

    /*
     * Check the descriptor type, DPL and such.
     * ASSUMES this is done in the same order as described for call-gate calls.
     */
    if (Idte.Gate.u1DescType)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - not system selector (%#x) -> #GP\n", u8Vector, Idte.Gate.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Vector << X86_TRAP_ERR_SEL_SHIFT));
    }
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
        case X86_SEL_TYPE_SYS_386_INT_GATE:
            fEflToClear |= X86_EFL_IF;
            break;

        case X86_SEL_TYPE_SYS_TASK_GATE:
            /** @todo task gates. */
            AssertFailedReturn(VERR_NOT_SUPPORTED);

        case X86_SEL_TYPE_SYS_286_TRAP_GATE:
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

    /* A null CS is bad. */
    RTSEL NewCS = Idte.Gate.u16Sel;
    if (!(NewCS & (X86_SEL_MASK | X86_SEL_LDT)))
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x -> #GP\n", u8Vector, NewCS));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Fetch the descriptor for the new CS. */
    IEMSELDESC DescCS;
    rcStrict = iemMemFetchSelDesc(pIemCpu, &DescCS, NewCS);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Must be a code segment. */
    if (!DescCS.Legacy.Gen.u1DescType)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - system selector (%#x) -> #GP\n", u8Vector, NewCS, DescCS.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & (X86_SEL_MASK | X86_SEL_LDT));
    }
    if (!(DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE))
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - data selector (%#x) -> #GP\n", u8Vector, NewCS, DescCS.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & (X86_SEL_MASK | X86_SEL_LDT));
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
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & (X86_SEL_MASK | X86_SEL_LDT));
    }
    /** @todo is the RPL of the interrupt/trap gate descriptor checked? */

    /* Check the new EIP against the new CS limit. */
    uint32_t const uNewEip =    Idte.Gate.u4Type == X86_SEL_TYPE_SYS_286_INT_GATE
                             || Idte.Gate.u4Type == X86_SEL_TYPE_SYS_286_TRAP_GATE
                           ? Idte.Gate.u16OffsetLow
                           : Idte.Gate.u16OffsetLow | ((uint32_t)Idte.Gate.u16OffsetHigh << 16);
    uint32_t cbLimitCS = X86DESC_LIMIT(DescCS.Legacy);
    if (DescCS.Legacy.Gen.u1Granularity)
        cbLimitCS = (cbLimitCS << PAGE_SHIFT) | PAGE_OFFSET_MASK;
    if (uNewEip > cbLimitCS)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - DPL (%d) > CPL (%d) -> #GP\n",
             u8Vector, NewCS, DescCS.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
        return iemRaiseGeneralProtectionFault(pIemCpu, NewCS & (X86_SEL_MASK | X86_SEL_LDT));
    }

    /* Make sure the selector is present. */
    if (!DescCS.Legacy.Gen.u1Present)
    {
        Log(("RaiseXcptOrIntInProtMode %#x - CS=%#x - segment not present -> #NP\n", u8Vector, NewCS));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, NewCS);
    }

    /*
     * If the privilege level changes, we need to get a new stack from the TSS.
     * This in turns means validating the new SS and ESP...
     */
    uint8_t const   uNewCpl = DescCS.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF
                            ? pIemCpu->uCpl : DescCS.Legacy.Gen.u2Dpl;
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
        uint32_t cbLimitSS = X86DESC_LIMIT(DescSS.Legacy);
        if (DescSS.Legacy.Gen.u1Granularity)
            cbLimitSS = (cbLimitSS << PAGE_SHIFT) | PAGE_OFFSET_MASK;
        AssertReturn(!(DescSS.Legacy.Gen.u4Type & X86_SEL_TYPE_DOWN), VERR_NOT_IMPLEMENTED);

        uint8_t const cbStackFrame = fFlags & IEM_XCPT_FLAGS_ERR ? 24 : 20;
        if (   uNewEsp - 1 > cbLimitSS
            || uNewEsp < cbStackFrame)
        {
            Log(("RaiseXcptOrIntInProtMode: %#x - SS=%#x ESP=%#x cbStackFrame=%#x is out of bounds -> #GP\n",
                 u8Vector, NewSS, uNewEsp, cbStackFrame));
            return iemRaiseSelectorBoundsBySelector(pIemCpu, NewSS);
        }

        /*
         * Start making changes.
         */

        /* Create the stack frame. */
        RTPTRUNION uStackFrame;
        rcStrict = iemMemMap(pIemCpu, &uStackFrame.pv, cbStackFrame, UINT8_MAX,
                             uNewEsp - cbStackFrame + X86DESC_BASE(DescSS.Legacy), IEM_ACCESS_STACK_W);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        void * const pvStackFrame = uStackFrame.pv;

        if (fFlags & IEM_XCPT_FLAGS_ERR)
            *uStackFrame.pu32++ = uErr;
        uStackFrame.pu32[0] = pCtx->eip;
        uStackFrame.pu32[1] = (pCtx->cs & ~X86_SEL_RPL) | pIemCpu->uCpl;
        uStackFrame.pu32[2] = pCtx->eflags.u;
        uStackFrame.pu32[3] = pCtx->esp;
        uStackFrame.pu32[4] = pCtx->ss;
        rcStrict = iemMemCommitAndUnmap(pIemCpu, pvStackFrame, IEM_ACCESS_STACK_W);
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
         * Start commint the register changes (joins with the DPL=CPL branch).
         */
        pCtx->ss                = NewSS;
        pCtx->ssHid.u32Limit    = cbLimitSS;
        pCtx->ssHid.u64Base     = X86DESC_BASE(DescSS.Legacy);
        pCtx->ssHid.Attr.u      = X86DESC_GET_HID_ATTR(DescSS.Legacy);
        pCtx->rsp               = uNewEsp - cbStackFrame; /** @todo Is the high word cleared for 16-bit stacks and/or interrupt handlers? */
        pIemCpu->uCpl           = uNewCpl;
    }
    /*
     * Same privilege, no stack change and smaller stack frame.
     */
    else
    {
        uint64_t        uNewRsp;
        RTPTRUNION      uStackFrame;
        uint8_t const   cbStackFrame = fFlags & IEM_XCPT_FLAGS_ERR ? 16 : 12;
        rcStrict = iemMemStackPushBeginSpecial(pIemCpu, cbStackFrame, &uStackFrame.pv, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        void * const pvStackFrame = uStackFrame.pv;

        if (fFlags & IEM_XCPT_FLAGS_ERR)
            *uStackFrame.pu32++ = uErr;
        uStackFrame.pu32[0] = pCtx->eip;
        uStackFrame.pu32[1] = (pCtx->cs & ~X86_SEL_RPL) | pIemCpu->uCpl;
        uStackFrame.pu32[2] = pCtx->eflags.u;
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
    pCtx->cs                = (NewCS & ~X86_SEL_RPL) | uNewCpl;
    pCtx->csHid.u32Limit    = cbLimitCS;
    pCtx->csHid.u64Base     = X86DESC_BASE(DescCS.Legacy);
    pCtx->csHid.Attr.u      = X86DESC_GET_HID_ATTR(DescCS.Legacy);

    pCtx->rip               = uNewEip;
    pCtx->rflags.u         &= ~fEflToClear;

    if (fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT)
        iemRaiseXcptAdjustState(pCtx, u8Vector);

    return fFlags & IEM_XCPT_FLAGS_T_CPU_XCPT ? VINF_IEM_RAISED_XCPT : VINF_SUCCESS;
}


/**
 * Implements exceptions and interrupts for V8086 mode.
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
static VBOXSTRICTRC
iemRaiseXcptOrIntInV8086Mode(PIEMCPU     pIemCpu,
                             PCPUMCTX    pCtx,
                             uint8_t     cbInstr,
                             uint8_t     u8Vector,
                             uint32_t    fFlags,
                             uint16_t    uErr,
                             uint64_t    uCr2)
{
    AssertMsgFailed(("V8086 exception / interrupt dispatching\n"));
    return VERR_NOT_IMPLEMENTED;
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
static VBOXSTRICTRC
iemRaiseXcptOrIntInLongMode(PIEMCPU     pIemCpu,
                            PCPUMCTX    pCtx,
                            uint8_t     cbInstr,
                            uint8_t     u8Vector,
                            uint32_t    fFlags,
                            uint16_t    uErr,
                            uint64_t    uCr2)
{
    AssertMsgFailed(("long mode exception / interrupt dispatching\n"));
    return VERR_NOT_IMPLEMENTED;
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
DECL_NO_INLINE(static, VBOXSTRICTRC)
iemRaiseXcptOrInt(PIEMCPU     pIemCpu,
                  uint8_t     cbInstr,
                  uint8_t     u8Vector,
                  uint32_t    fFlags,
                  uint16_t    uErr,
                  uint64_t    uCr2)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Do recursion accounting.
     */
    uint8_t const  uPrevXcpt = pIemCpu->uCurXcpt;
    uint32_t const fPrevXcpt = pIemCpu->fCurXcpt;
    if (pIemCpu->cXcptRecursions == 0)
        Log(("iemRaiseXcptOrInt: %#x at %04x:%RGv cbInstr=%#x fFlags=%#x uErr=%#x uCr2=%llx\n",
             u8Vector, pCtx->cs, pCtx->rip, cbInstr, fFlags, uErr, uCr2));
    else
    {
        Log(("iemRaiseXcptOrInt: %#x at %04x:%RGv cbInstr=%#x fFlags=%#x uErr=%#x uCr2=%llx; prev=%#x depth=%d flags=%#x\n",
             u8Vector, pCtx->cs, pCtx->rip, cbInstr, fFlags, uErr, uCr2, pIemCpu->uCurXcpt, pIemCpu->cXcptRecursions + 1, fPrevXcpt));

        /** @todo double and tripple faults. */
        AssertReturn(pIemCpu->cXcptRecursions < 3, VERR_NOT_IMPLEMENTED);

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
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
    {
        PVM     pVM   = IEMCPU_TO_VM(pIemCpu);
        PVMCPU  pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
        char    szRegs[4096];
        DBGFR3RegPrintf(pVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
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
        DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, 0, 0,
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
    else if (!pCtx->eflags.Bits.u1VM)
        rcStrict = iemRaiseXcptOrIntInProtMode( pIemCpu, pCtx, cbInstr, u8Vector, fFlags, uErr, uCr2);
    else
        rcStrict = iemRaiseXcptOrIntInV8086Mode(pIemCpu, pCtx, cbInstr, u8Vector, fFlags, uErr, uCr2);

    /*
     * Unwind.
     */
    pIemCpu->cXcptRecursions--;
    pIemCpu->uCurXcpt = uPrevXcpt;
    pIemCpu->fCurXcpt = fPrevXcpt;
    Log(("iemRaiseXcptOrInt: returns %Rrc (vec=%#x); cs:rip=%04x:%RGv ss:rsp=%04x:%RGv\n",
         VBOXSTRICTRC_VAL(rcStrict), u8Vector, pCtx->cs, pCtx->rip, pCtx->ss, pCtx->esp));
    return rcStrict;
}


/** \#DE - 00.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseDivideError(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_DE, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#DB - 01.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseDebugException(PIEMCPU pIemCpu)
{
    /** @todo set/clear RF. */
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_DB, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#UD - 06.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseUndefinedOpcode(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_UD, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


/** \#NM - 07.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseDeviceNotAvailable(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NM, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
}


#ifdef SOME_UNUSED_FUNCTION
/** \#TS(err) - 0a.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseTaskSwitchFaultWithErr(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_TS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}
#endif


/** \#TS(tr) - 0a.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseTaskSwitchFaultCurrentTSS(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_TS, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             pIemCpu->CTX_SUFF(pCtx)->tr, 0);
}


/** \#NP(err) - 0b.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseSelectorNotPresentWithErr(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}


/** \#NP(seg) - 0b.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseSelectorNotPresentBySegReg(PIEMCPU pIemCpu, uint32_t iSegReg)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             iemSRegFetchU16(pIemCpu, iSegReg) & ~X86_SEL_RPL, 0);
}


/** \#NP(sel) - 0b.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_NP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             uSel & ~X86_SEL_RPL, 0);
}


/** \#GP(n) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseGeneralProtectionFault(PIEMCPU pIemCpu, uint16_t uErr)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, uErr, 0);
}


/** \#GP(0) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseGeneralProtectionFault0(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseGeneralProtectionFaultBySelector(PIEMCPU pIemCpu, RTSEL Sel)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR,
                             Sel & ~X86_SEL_RPL, 0);
}


/** \#GP(0) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseNotCanonical(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseSelectorBounds(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess)
{
    NOREF(iSegReg); NOREF(fAccess);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseSelectorBoundsBySelector(PIEMCPU pIemCpu, RTSEL Sel)
{
    NOREF(Sel);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#GP(sel) - 0d.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseSelectorInvalidAccess(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess)
{
    NOREF(iSegReg); NOREF(fAccess);
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_GP, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR, 0, 0);
}


/** \#PF(n) - 0e.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaisePageFault(PIEMCPU pIemCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc)
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

    if (fAccess & IEM_ACCESS_TYPE_WRITE)
        uErr |= X86_TRAP_PF_RW;

    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_PF, IEM_XCPT_FLAGS_T_CPU_XCPT | IEM_XCPT_FLAGS_ERR | IEM_XCPT_FLAGS_CR2,
                             uErr, GCPtrWhere);
}


/** \#MF(n) - 10.  */
DECL_NO_INLINE(static, VBOXSTRICTRC) iemRaiseMathFault(PIEMCPU pIemCpu)
{
    return iemRaiseXcptOrInt(pIemCpu, 0, X86_XCPT_MF, IEM_XCPT_FLAGS_T_CPU_XCPT, 0, 0);
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
static void iemRecalEffOpSize(PIEMCPU pIemCpu)
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
static void iemRecalEffOpSize64Default(PIEMCPU pIemCpu)
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
#include <iprt/mem.h>

/**
 * Used to add extra details about a stub case.
 * @param   pIemCpu     The IEM per CPU state.
 */
static void iemOpStubMsg2(PIEMCPU pIemCpu)
{
    PVM     pVM   = IEMCPU_TO_VM(pIemCpu);
    PVMCPU  pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    char szRegs[4096];
    DBGFR3RegPrintf(pVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
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
    DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, 0, 0,
                       DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                       szInstr, sizeof(szInstr), NULL);

    RTAssertMsg2Weak("%s%s\n", szRegs, szInstr);
}


/** Stubs an opcode. */
#define FNIEMOP_STUB(a_Name) \
    FNIEMOP_DEF(a_Name) \
    { \
        RTAssertMsg1(NULL, __LINE__, __FILE__, __FUNCTION__); \
        iemOpStubMsg2(pIemCpu); \
        RTAssertPanic(); \
        return VERR_NOT_IMPLEMENTED; \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode. */
#define FNIEMOP_STUB_1(a_Name, a_Type0, a_Name0) \
    FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    { \
        RTAssertMsg1(NULL, __LINE__, __FILE__, __FUNCTION__); \
        iemOpStubMsg2(pIemCpu); \
        RTAssertPanic(); \
        return VERR_NOT_IMPLEMENTED; \
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
static PCPUMSELREGHID iemSRegGetHid(PIEMCPU pIemCpu, uint8_t iSegReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (iSegReg)
    {
        case X86_SREG_ES: return &pCtx->esHid;
        case X86_SREG_CS: return &pCtx->csHid;
        case X86_SREG_SS: return &pCtx->ssHid;
        case X86_SREG_DS: return &pCtx->dsHid;
        case X86_SREG_FS: return &pCtx->fsHid;
        case X86_SREG_GS: return &pCtx->gsHid;
    }
    AssertFailedReturn(NULL);
}


/**
 * Gets a reference (pointer) to the specified segment register (the selector
 * value).
 *
 * @returns Pointer to the selector variable.
 * @param   pIemCpu             The per CPU data.
 * @param   iSegReg             The segment register.
 */
static uint16_t *iemSRegRef(PIEMCPU pIemCpu, uint8_t iSegReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (iSegReg)
    {
        case X86_SREG_ES: return &pCtx->es;
        case X86_SREG_CS: return &pCtx->cs;
        case X86_SREG_SS: return &pCtx->ss;
        case X86_SREG_DS: return &pCtx->ds;
        case X86_SREG_FS: return &pCtx->fs;
        case X86_SREG_GS: return &pCtx->gs;
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
static uint16_t iemSRegFetchU16(PIEMCPU pIemCpu, uint8_t iSegReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (iSegReg)
    {
        case X86_SREG_ES: return pCtx->es;
        case X86_SREG_CS: return pCtx->cs;
        case X86_SREG_SS: return pCtx->ss;
        case X86_SREG_DS: return pCtx->ds;
        case X86_SREG_FS: return pCtx->fs;
        case X86_SREG_GS: return pCtx->gs;
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
static void *iemGRegRef(PIEMCPU pIemCpu, uint8_t iReg)
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
static uint8_t *iemGRegRefU8(PIEMCPU pIemCpu, uint8_t iReg)
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
static uint8_t iemGRegFetchU8(PIEMCPU pIemCpu, uint8_t iReg)
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
static uint16_t iemGRegFetchU16(PIEMCPU pIemCpu, uint8_t iReg)
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
static uint32_t iemGRegFetchU32(PIEMCPU pIemCpu, uint8_t iReg)
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
static uint64_t iemGRegFetchU64(PIEMCPU pIemCpu, uint8_t iReg)
{
    return *(uint64_t *)iemGRegRef(pIemCpu, iReg);
}


/**
 * Is the FPU state in FXSAVE format or not.
 *
 * @returns true if it is, false if it's in FNSAVE.
 * @param   pVCpu               The virtual CPU handle.
 */
DECLINLINE(bool) iemFRegIsFxSaveFormat(PIEMCPU pIemCpu)
{
#ifdef RT_ARCH_AMD64
    return true;
#else
/// @todo    return pVCpu->pVMR3->cpum.s.CPUFeatures.edx.u1FXSR;
    return true;
#endif
}


/**
 * Gets the FPU status word.
 *
 * @returns FPU status word
 * @param   pIemCpu             The per CPU data.
 */
static uint16_t iemFRegFetchFsw(PIEMCPU pIemCpu)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    uint16_t u16Fsw;
    if (iemFRegIsFxSaveFormat(pIemCpu))
        u16Fsw = pCtx->fpu.FSW;
    else
    {
        PX86FPUSTATE pFpu = (PX86FPUSTATE)&pCtx->fpu;
        u16Fsw = pFpu->FSW;
    }
    return u16Fsw;
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
static VBOXSTRICTRC iemRegRipRelativeJumpS8(PIEMCPU pIemCpu, int8_t offNextInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t uNewIp = pCtx->ip + offNextInstr + pIemCpu->offOpcode;
            if (   uNewIp > pCtx->csHid.u32Limit
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
            if (uNewEip > pCtx->csHid.u32Limit)
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
static VBOXSTRICTRC iemRegRipRelativeJumpS16(PIEMCPU pIemCpu, int16_t offNextInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    Assert(pIemCpu->enmEffOpSize == IEMMODE_16BIT);

    uint16_t uNewIp = pCtx->ip + offNextInstr + pIemCpu->offOpcode;
    if (   uNewIp > pCtx->csHid.u32Limit
        && pIemCpu->enmCpuMode != IEMMODE_64BIT) /* no need to check for non-canonical. */
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    /** @todo Test 16-bit jump in 64-bit mode.  */
    pCtx->rip = uNewIp;

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
static VBOXSTRICTRC iemRegRipRelativeJumpS32(PIEMCPU pIemCpu, int32_t offNextInstr)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    Assert(pIemCpu->enmEffOpSize != IEMMODE_16BIT);

    if (pIemCpu->enmEffOpSize == IEMMODE_32BIT)
    {
        Assert(pCtx->rip <= UINT32_MAX); Assert(pIemCpu->enmCpuMode != IEMMODE_64BIT);

        uint32_t uNewEip = pCtx->eip + offNextInstr + pIemCpu->offOpcode;
        if (uNewEip > pCtx->csHid.u32Limit)
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
static VBOXSTRICTRC iemRegRipJump(PIEMCPU pIemCpu, uint64_t uNewRip)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    switch (pIemCpu->enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            Assert(uNewRip <= UINT16_MAX);
            if (   uNewRip > pCtx->csHid.u32Limit
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

            if (uNewRip > pCtx->csHid.u32Limit)
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

    return VINF_SUCCESS;
}


/**
 * Get the address of the top of the stack.
 *
 * @param   pCtx                The CPU context which SP/ESP/RSP should be
 *                              read.
 */
DECLINLINE(RTGCPTR) iemRegGetEffRsp(PCCPUMCTX pCtx)
{
    if (pCtx->ssHid.Attr.n.u1Long)
        return pCtx->rsp;
    if (pCtx->ssHid.Attr.n.u1DefBig)
        return pCtx->esp;
    return pCtx->sp;
}


/**
 * Updates the RIP/EIP/IP to point to the next instruction.
 *
 * @param   pIemCpu             The per CPU data.
 * @param   cbInstr             The number of bytes to add.
 */
static void iemRegAddToRip(PIEMCPU pIemCpu, uint8_t cbInstr)
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


/**
 * Updates the RIP/EIP/IP to point to the next instruction.
 *
 * @param   pIemCpu             The per CPU data.
 */
static void iemRegUpdateRip(PIEMCPU pIemCpu)
{
    return iemRegAddToRip(pIemCpu, pIemCpu->offOpcode);
}


/**
 * Adds to the stack pointer.
 *
 * @param   pCtx                The CPU context which SP/ESP/RSP should be
 *                              updated.
 * @param   cbToAdd             The number of bytes to add.
 */
DECLINLINE(void) iemRegAddToRsp(PCPUMCTX pCtx, uint8_t cbToAdd)
{
    if (pCtx->ssHid.Attr.n.u1Long)
        pCtx->rsp += cbToAdd;
    else if (pCtx->ssHid.Attr.n.u1DefBig)
        pCtx->esp += cbToAdd;
    else
        pCtx->sp  += cbToAdd;
}


/**
 * Subtracts from the stack pointer.
 *
 * @param   pCtx                The CPU context which SP/ESP/RSP should be
 *                              updated.
 * @param   cbToSub             The number of bytes to subtract.
 */
DECLINLINE(void) iemRegSubFromRsp(PCPUMCTX pCtx, uint8_t cbToSub)
{
    if (pCtx->ssHid.Attr.n.u1Long)
        pCtx->rsp -= cbToSub;
    else if (pCtx->ssHid.Attr.n.u1DefBig)
        pCtx->esp -= cbToSub;
    else
        pCtx->sp  -= cbToSub;
}


/**
 * Adds to the temporary stack pointer.
 *
 * @param   pTmpRsp             The temporary SP/ESP/RSP to update.
 * @param   cbToAdd             The number of bytes to add.
 * @param   pCtx                Where to get the current stack mode.
 */
DECLINLINE(void) iemRegAddToRspEx(PRTUINT64U pTmpRsp, uint8_t cbToAdd, PCCPUMCTX pCtx)
{
    if (pCtx->ssHid.Attr.n.u1Long)
        pTmpRsp->u           += cbToAdd;
    else if (pCtx->ssHid.Attr.n.u1DefBig)
        pTmpRsp->DWords.dw0  += cbToAdd;
    else
        pTmpRsp->Words.w0    += cbToAdd;
}


/**
 * Subtracts from the temporary stack pointer.
 *
 * @param   pTmpRsp             The temporary SP/ESP/RSP to update.
 * @param   cbToSub             The number of bytes to subtract.
 * @param   pCtx                Where to get the current stack mode.
 */
DECLINLINE(void) iemRegSubFromRspEx(PRTUINT64U pTmpRsp, uint8_t cbToSub, PCCPUMCTX pCtx)
{
    if (pCtx->ssHid.Attr.n.u1Long)
        pTmpRsp->u          -= cbToSub;
    else if (pCtx->ssHid.Attr.n.u1DefBig)
        pTmpRsp->DWords.dw0 -= cbToSub;
    else
        pTmpRsp->Words.w0   -= cbToSub;
}


/**
 * Calculates the effective stack address for a push of the specified size as
 * well as the new RSP value (upper bits may be masked).
 *
 * @returns Effective stack addressf for the push.
 * @param   pCtx                Where to get the current stack mode.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPush(PCCPUMCTX pCtx, uint8_t cbItem, uint64_t *puNewRsp)
{
    RTUINT64U   uTmpRsp;
    RTGCPTR     GCPtrTop;
    uTmpRsp.u = pCtx->rsp;

    if (pCtx->ssHid.Attr.n.u1Long)
        GCPtrTop = uTmpRsp.u            -= cbItem;
    else if (pCtx->ssHid.Attr.n.u1DefBig)
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
 * @param   pCtx                Where to get the current stack mode.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPop(PCCPUMCTX pCtx, uint8_t cbItem, uint64_t *puNewRsp)
{
    RTUINT64U   uTmpRsp;
    RTGCPTR     GCPtrTop;
    uTmpRsp.u = pCtx->rsp;

    if (pCtx->ssHid.Attr.n.u1Long)
    {
        GCPtrTop = uTmpRsp.u;
        uTmpRsp.u += cbItem;
    }
    else if (pCtx->ssHid.Attr.n.u1DefBig)
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
 * @param   pTmpRsp             The temporary stack pointer.  This is updated.
 * @param   cbItem              The size of the stack item to pop.
 * @param   puNewRsp            Where to return the new RSP value.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPushEx(PRTUINT64U pTmpRsp, uint8_t cbItem, PCCPUMCTX pCtx)
{
    RTGCPTR GCPtrTop;

    if (pCtx->ssHid.Attr.n.u1Long)
        GCPtrTop = pTmpRsp->u          -= cbItem;
    else if (pCtx->ssHid.Attr.n.u1DefBig)
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
 * @param   pTmpRsp             The temporary stack pointer.  This is updated.
 * @param   pCtx                Where to get the current stack mode.
 * @param   cbItem              The size of the stack item to pop.
 */
DECLINLINE(RTGCPTR) iemRegGetRspForPopEx(PRTUINT64U pTmpRsp, uint8_t cbItem, PCCPUMCTX pCtx)
{
    RTGCPTR GCPtrTop;
    if (pCtx->ssHid.Attr.n.u1Long)
    {
        GCPtrTop = pTmpRsp->u;
        pTmpRsp->u          += cbItem;
    }
    else if (pCtx->ssHid.Attr.n.u1DefBig)
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


/**
 * Checks if an Intel CPUID feature bit is set.
 *
 * @returns true / false.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   fEdx                The EDX bit to test, or 0 if ECX.
 * @param   fEcx                The ECX bit to test, or 0 if EDX.
 * @remarks Used via IEM_IS_INTEL_CPUID_FEATURE_PRESENT_EDX,
 *          IEM_IS_INTEL_CPUID_FEATURE_PRESENT_ECX and others.
 */
static bool iemRegIsIntelCpuIdFeaturePresent(PIEMCPU pIemCpu, uint32_t fEdx, uint32_t fEcx)
{
    uint32_t uEax, uEbx, uEcx, uEdx;
    CPUMGetGuestCpuId(IEMCPU_TO_VMCPU(pIemCpu), 0x00000001, &uEax, &uEbx, &uEcx, &uEdx);
    return (fEcx && (uEcx & fEcx))
        || (fEdx && (uEdx & fEdx));
}


/**
 * Checks if an AMD CPUID feature bit is set.
 *
 * @returns true / false.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   fEdx                The EDX bit to test, or 0 if ECX.
 * @param   fEcx                The ECX bit to test, or 0 if EDX.
 * @remarks Used via IEM_IS_AMD_CPUID_FEATURE_PRESENT_EDX,
 *          IEM_IS_AMD_CPUID_FEATURE_PRESENT_ECX and others.
 */
static bool iemRegIsAmdCpuIdFeaturePresent(PIEMCPU pIemCpu, uint32_t fEdx, uint32_t fEcx)
{
    uint32_t uEax, uEbx, uEcx, uEdx;
    CPUMGetGuestCpuId(IEMCPU_TO_VMCPU(pIemCpu), 0x80000001, &uEax, &uEbx, &uEcx, &uEdx);
    return (fEcx && (uEcx & fEcx))
        || (fEdx && (uEdx & fEdx));
}

/** @}  */


/** @name   Memory access.
 *
 * @{
 */


/**
 * Checks if the given segment can be written to, raise the appropriate
 * exception if not.
 *
 * @returns VBox strict status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pHid                Pointer to the hidden register.
 * @param   iSegReg             The register number.
 */
static VBOXSTRICTRC iemMemSegCheckWriteAccessEx(PIEMCPU pIemCpu, PCCPUMSELREGHID pHid, uint8_t iSegReg)
{
    if (!pHid->Attr.n.u1Present)
        return iemRaiseSelectorNotPresentBySegReg(pIemCpu, iSegReg);

    if (   (   (pHid->Attr.n.u4Type & X86_SEL_TYPE_CODE)
            || !(pHid->Attr.n.u4Type & X86_SEL_TYPE_WRITE) )
        &&  pIemCpu->enmCpuMode != IEMMODE_64BIT )
        return iemRaiseSelectorInvalidAccess(pIemCpu, iSegReg, IEM_ACCESS_DATA_W);

    /** @todo DPL/RPL/CPL? */

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
 */
static VBOXSTRICTRC iemMemSegCheckReadAccessEx(PIEMCPU pIemCpu, PCCPUMSELREGHID pHid, uint8_t iSegReg)
{
    if (!pHid->Attr.n.u1Present)
        return iemRaiseSelectorNotPresentBySegReg(pIemCpu, iSegReg);

    if (   (pHid->Attr.n.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE
        &&  pIemCpu->enmCpuMode != IEMMODE_64BIT )
        return iemRaiseSelectorInvalidAccess(pIemCpu, iSegReg, IEM_ACCESS_DATA_R);

    /** @todo DPL/RPL/CPL? */

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
 * @param   pGCPtrMem           Pointer to the guest memory address to apply
 *                              segmentation to.  Input and output parameter.
 */
static VBOXSTRICTRC iemMemApplySegment(PIEMCPU pIemCpu, uint32_t fAccess, uint8_t iSegReg,
                                       size_t cbMem, PRTGCPTR pGCPtrMem)
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

            Assert(pSel->Attr.n.u1Present);
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

                    *pGCPtrMem = GCPtrFirst32 += (uint32_t)pSel->u64Base;
                }
                else
                {
                    /** @todo implement expand down segments. */
                    AssertFailed(/** @todo implement this */);
                    return VERR_NOT_IMPLEMENTED;
                }
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
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
            if (iSegReg == X86_SREG_GS || iSegReg == X86_SREG_FS)
                *pGCPtrMem += pSel->u64Base;
            return VINF_SUCCESS;

        default:
            AssertFailedReturn(VERR_INTERNAL_ERROR_5);
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
static VBOXSTRICTRC iemMemPageTranslateAndCheckAccess(PIEMCPU pIemCpu, RTGCPTR GCPtrMem, uint32_t fAccess,
                                                      PRTGCPHYS pGCPhysMem)
{
    /** @todo Need a different PGM interface here.  We're currently using
     *        generic / REM interfaces. this won't cut it for R0 & RC. */
    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int rc = PGMGstGetPage(IEMCPU_TO_VMCPU(pIemCpu), GCPtrMem, &fFlags, &GCPhys);
    if (RT_FAILURE(rc))
    {
        /** @todo Check unassigned memory in unpaged mode. */
        *pGCPhysMem = NIL_RTGCPHYS;
        return iemRaisePageFault(pIemCpu, GCPtrMem, fAccess, rc);
    }

    if (    (fFlags & (X86_PTE_RW | X86_PTE_US | X86_PTE_PAE_NX)) != (X86_PTE_RW | X86_PTE_US)
        &&  (   (   (fAccess & IEM_ACCESS_TYPE_WRITE) /* Write to read only memory? */
                 && !(fFlags & X86_PTE_RW)
                 && (   pIemCpu->uCpl != 0
                     || (pIemCpu->CTX_SUFF(pCtx)->cr0 & X86_CR0_WP)) )
             || (   !(fFlags & X86_PTE_US)            /* Kernel memory */
                 &&  pIemCpu->uCpl == 3)
             || (   (fAccess & IEM_ACCESS_TYPE_EXEC)  /* Executing non-executable memory? */
                 && (fFlags & X86_PTE_PAE_NX)
                 && (pIemCpu->CTX_SUFF(pCtx)->msrEFER & MSR_K6_EFER_NXE) )
            )
       )
    {
        *pGCPhysMem = NIL_RTGCPHYS;
        return iemRaisePageFault(pIemCpu, GCPtrMem, fAccess, VERR_ACCESS_DENIED);
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
 */
static int iemMemPageMap(PIEMCPU pIemCpu, RTGCPHYS GCPhysMem, uint32_t fAccess, void **ppvMem)
{
#ifdef IEM_VERIFICATION_MODE
    /* Force the alternative path so we can ignore writes. */
    if ((fAccess & IEM_ACCESS_TYPE_WRITE) && !pIemCpu->fNoRem)
        return VERR_PGM_PHYS_TLB_CATCH_ALL;
#endif

    /*
     * If we can map the page without trouble, do a block processing
     * until the end of the current page.
     */
    /** @todo need some better API. */
    return PGMR3PhysTlbGCPhys2Ptr(IEMCPU_TO_VM(pIemCpu),
                                  GCPhysMem,
                                  RT_BOOL(fAccess & IEM_ACCESS_TYPE_WRITE),
                                  ppvMem);
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
static unsigned iemMemMapFindFree(PIEMCPU pIemCpu)
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
static VBOXSTRICTRC iemMemBounceBufferCommitAndUnmap(PIEMCPU pIemCpu, unsigned iMemMap)
{
    Assert(pIemCpu->aMemMappings[iMemMap].fAccess & IEM_ACCESS_BOUNCE_BUFFERED);
    Assert(pIemCpu->aMemMappings[iMemMap].fAccess & IEM_ACCESS_TYPE_WRITE);

    /*
     * Do the writing.
     */
    int rc;
    if (   !pIemCpu->aMemBbMappings[iMemMap].fUnassigned
        && !IEM_VERIFICATION_ENABLED(pIemCpu))
    {
        uint16_t const  cbFirst  = pIemCpu->aMemBbMappings[iMemMap].cbFirst;
        uint16_t const  cbSecond = pIemCpu->aMemBbMappings[iMemMap].cbSecond;
        uint8_t const  *pbBuf    = &pIemCpu->aBounceBuffers[iMemMap].ab[0];
        if (!pIemCpu->fByPassHandlers)
        {
            rc = PGMPhysWrite(IEMCPU_TO_VM(pIemCpu),
                              pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst,
                              pbBuf,
                              cbFirst);
            if (cbSecond && rc == VINF_SUCCESS)
                rc = PGMPhysWrite(IEMCPU_TO_VM(pIemCpu),
                                  pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond,
                                  pbBuf + cbFirst,
                                  cbSecond);
        }
        else
        {
            rc = PGMPhysSimpleWriteGCPhys(IEMCPU_TO_VM(pIemCpu),
                                          pIemCpu->aMemBbMappings[iMemMap].GCPhysFirst,
                                          pbBuf,
                                          cbFirst);
            if (cbSecond && rc == VINF_SUCCESS)
                rc = PGMPhysSimpleWriteGCPhys(IEMCPU_TO_VM(pIemCpu),
                                              pIemCpu->aMemBbMappings[iMemMap].GCPhysSecond,
                                              pbBuf + cbFirst,
                                              cbSecond);
        }
    }
    else
        rc = VINF_SUCCESS;

#ifdef IEM_VERIFICATION_MODE
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

    /*
     * Free the mapping entry.
     */
    pIemCpu->aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    Assert(pIemCpu->cActiveMappings != 0);
    pIemCpu->cActiveMappings--;
    return rc;
}


/**
 * iemMemMap worker that deals with a request crossing pages.
 */
static VBOXSTRICTRC iemMemBounceBufferMapCrossPage(PIEMCPU pIemCpu, int iMemMap, void **ppvMem,
                                                   size_t cbMem, RTGCPTR GCPtrFirst, uint32_t fAccess)
{
    /*
     * Do the address translations.
     */
    RTGCPHYS GCPhysFirst;
    VBOXSTRICTRC rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, GCPtrFirst, fAccess, &GCPhysFirst);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    RTGCPHYS GCPhysSecond;
    rcStrict = iemMemPageTranslateAndCheckAccess(pIemCpu, GCPtrFirst + (cbMem - 1), fAccess, &GCPhysSecond);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;
    GCPhysSecond &= ~(RTGCPHYS)PAGE_OFFSET_MASK;

    /*
     * Read in the current memory content if it's a read of execute access.
     */
    uint8_t        *pbBuf        = &pIemCpu->aBounceBuffers[iMemMap].ab[0];
    uint32_t const  cbFirstPage  = PAGE_SIZE - (GCPhysFirst & PAGE_OFFSET_MASK);
    uint32_t const  cbSecondPage = (uint32_t)(cbMem - cbFirstPage);

    if (fAccess & (IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_EXEC))
    {
        int rc;
        if (!pIemCpu->fByPassHandlers)
        {
            rc = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhysFirst, pbBuf, cbFirstPage);
            if (rc != VINF_SUCCESS)
                return rc;
            rc = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhysSecond, pbBuf + cbFirstPage, cbSecondPage);
            if (rc != VINF_SUCCESS)
                return rc;
        }
        else
        {
            rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), pbBuf, GCPhysFirst, cbFirstPage);
            if (rc != VINF_SUCCESS)
                return rc;
            rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), pbBuf + cbFirstPage, GCPhysSecond, cbSecondPage);
            if (rc != VINF_SUCCESS)
                return rc;
        }

#ifdef IEM_VERIFICATION_MODE
        if (!pIemCpu->fNoRem)
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
#endif
#ifdef VBOX_STRICT
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
    pIemCpu->cActiveMappings++;

    *ppvMem = pbBuf;
    return VINF_SUCCESS;
}


/**
 * iemMemMap woker that deals with iemMemPageMap failures.
 */
static VBOXSTRICTRC iemMemBounceBufferMapPhys(PIEMCPU pIemCpu, unsigned iMemMap, void **ppvMem, size_t cbMem,
                                              RTGCPHYS GCPhysFirst, uint32_t fAccess, VBOXSTRICTRC rcMap)
{
    /*
     * Filter out conditions we can handle and the ones which shouldn't happen.
     */
    if (   rcMap != VINF_PGM_PHYS_TLB_CATCH_WRITE
        && rcMap != VERR_PGM_PHYS_TLB_CATCH_ALL
        && rcMap != VERR_PGM_PHYS_TLB_UNASSIGNED)
    {
        AssertReturn(RT_FAILURE_NP(rcMap), VERR_INTERNAL_ERROR_3);
        return rcMap;
    }
    pIemCpu->cPotentialExits++;

    /*
     * Read in the current memory content if it's a read of execute access.
     */
    uint8_t *pbBuf = &pIemCpu->aBounceBuffers[iMemMap].ab[0];
    if (fAccess & (IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_EXEC))
    {
        if (rcMap == VERR_PGM_PHYS_TLB_UNASSIGNED)
            memset(pbBuf, 0xff, cbMem);
        else
        {
            int rc;
            if (!pIemCpu->fByPassHandlers)
                rc = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhysFirst, pbBuf, cbMem);
            else
                rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), pbBuf, GCPhysFirst, cbMem);
            if (rc != VINF_SUCCESS)
                return rc;
        }

#ifdef IEM_VERIFICATION_MODE
        if (!pIemCpu->fNoRem)
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
    pIemCpu->cActiveMappings++;

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
 *                              2, 4, 6, 8, 12, 16 or 32.  When used by string
 *                              operations it can be up to a page.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 *                              Use UINT8_MAX to indicate that no segmentation
 *                              is required (for IDT, GDT and LDT accesses).
 * @param   GCPtrMem            The address of the guest memory.
 * @param   a_fAccess           How the memory is being accessed.  The
 *                              IEM_ACCESS_TYPE_XXX bit is used to figure out
 *                              how to map the memory, while the
 *                              IEM_ACCESS_WHAT_XXX bit is used when raising
 *                              exceptions.
 */
static VBOXSTRICTRC iemMemMap(PIEMCPU pIemCpu, void **ppvMem, size_t cbMem, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t fAccess)
{
    /*
     * Check the input and figure out which mapping entry to use.
     */
    Assert(cbMem <= 32);
    Assert(~(fAccess & ~(IEM_ACCESS_TYPE_MASK | IEM_ACCESS_WHAT_MASK)));

    unsigned iMemMap = pIemCpu->iNextMapping;
    if (iMemMap >= RT_ELEMENTS(pIemCpu->aMemMappings))
    {
        iMemMap = iemMemMapFindFree(pIemCpu);
        AssertReturn(iMemMap < RT_ELEMENTS(pIemCpu->aMemMappings), VERR_INTERNAL_ERROR_3);
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

    void *pvMem;
    rcStrict = iemMemPageMap(pIemCpu, GCPhysFirst, fAccess, &pvMem);
    if (rcStrict != VINF_SUCCESS)
        return iemMemBounceBufferMapPhys(pIemCpu, iMemMap, ppvMem, cbMem, GCPhysFirst, fAccess, rcStrict);

    /*
     * Fill in the mapping table entry.
     */
    pIemCpu->aMemMappings[iMemMap].pv      = pvMem;
    pIemCpu->aMemMappings[iMemMap].fAccess = fAccess;
    pIemCpu->iNextMapping = iMemMap + 1;
    pIemCpu->cActiveMappings++;

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
static VBOXSTRICTRC iemMemCommitAndUnmap(PIEMCPU pIemCpu, void *pvMem, uint32_t fAccess)
{
    int iMemMap = iemMapLookup(pIemCpu, pvMem, fAccess);
    AssertReturn(iMemMap >= 0, iMemMap);

    /*
     * If it's bounce buffered, we need to write back the buffer.
     */
    if (   (pIemCpu->aMemMappings[iMemMap].fAccess & (IEM_ACCESS_BOUNCE_BUFFERED | IEM_ACCESS_TYPE_WRITE))
        == (IEM_ACCESS_BOUNCE_BUFFERED | IEM_ACCESS_TYPE_WRITE))
        return iemMemBounceBufferCommitAndUnmap(pIemCpu, iMemMap);

    /* Free the entry. */
    pIemCpu->aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    Assert(pIemCpu->cActiveMappings != 0);
    pIemCpu->cActiveMappings--;
    return VINF_SUCCESS;
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
static VBOXSTRICTRC iemMemFetchDataU8(PIEMCPU pIemCpu, uint8_t *pu8Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
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
static VBOXSTRICTRC iemMemFetchDataU16(PIEMCPU pIemCpu, uint16_t *pu16Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
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
static VBOXSTRICTRC iemMemFetchDataU32(PIEMCPU pIemCpu, uint32_t *pu32Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
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
static VBOXSTRICTRC iemMemFetchDataS32SxU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
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
static VBOXSTRICTRC iemMemFetchDataU64(PIEMCPU pIemCpu, uint64_t *pu64Dst, uint8_t iSegReg, RTGCPTR GCPtrMem)
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
 * Fetches a descriptor register (lgdt, lidt).
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pcbLimit            Where to return the limit.
 * @param   pGCPTrBase          Where to return the base.
 * @param   iSegReg             The index of the segment register to use for
 *                              this access.  The base and limits are checked.
 * @param   GCPtrMem            The address of the guest memory.
 * @param   enmOpSize           The effective operand size.
 */
static VBOXSTRICTRC iemMemFetchDataXdtr(PIEMCPU pIemCpu, uint16_t *pcbLimit, PRTGCPTR pGCPtrBase,
                                        uint8_t iSegReg, RTGCPTR GCPtrMem, IEMMODE enmOpSize)
{
    uint8_t const *pu8Src;
    VBOXSTRICTRC rcStrict = iemMemMap(pIemCpu,
                                      (void **)&pu8Src,
                                      enmOpSize == IEMMODE_64BIT
                                      ? 2 + 8
                                      : enmOpSize == IEMMODE_32BIT
                                      ? 2 + 4
                                      : 2 + 3,
                                      iSegReg,
                                      GCPtrMem,
                                      IEM_ACCESS_DATA_R);
    if (rcStrict == VINF_SUCCESS)
    {
        *pcbLimit = RT_MAKE_U16(pu8Src[0], pu8Src[1]);
        switch (enmOpSize)
        {
            case IEMMODE_16BIT:
                *pGCPtrBase = RT_MAKE_U32_FROM_U8(pu8Src[2], pu8Src[3], pu8Src[4], 0);
                break;
            case IEMMODE_32BIT:
                *pGCPtrBase = RT_MAKE_U32_FROM_U8(pu8Src[2], pu8Src[3], pu8Src[4], pu8Src[5]);
                break;
            case IEMMODE_64BIT:
                *pGCPtrBase = RT_MAKE_U64_FROM_U8(pu8Src[2], pu8Src[3], pu8Src[4], pu8Src[5],
                                                  pu8Src[6], pu8Src[7], pu8Src[8], pu8Src[9]);
                break;

                IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
        rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pu8Src, IEM_ACCESS_DATA_R);
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
static VBOXSTRICTRC iemMemStoreDataU8(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint8_t u8Value)
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
static VBOXSTRICTRC iemMemStoreDataU16(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint16_t u16Value)
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
static VBOXSTRICTRC iemMemStoreDataU32(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint32_t u32Value)
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
static VBOXSTRICTRC iemMemStoreDataU64(PIEMCPU pIemCpu, uint8_t iSegReg, RTGCPTR GCPtrMem, uint64_t u64Value)
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
 * Pushes a word onto the stack.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u16Value            The value to push.
 */
static VBOXSTRICTRC iemMemStackPushU16(PIEMCPU pIemCpu, uint16_t u16Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pCtx, 2, &uNewRsp);

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
static VBOXSTRICTRC iemMemStackPushU32(PIEMCPU pIemCpu, uint32_t u32Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pCtx, 4, &uNewRsp);

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
static VBOXSTRICTRC iemMemStackPushU64(PIEMCPU pIemCpu, uint64_t u64Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pCtx, 8, &uNewRsp);

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
static VBOXSTRICTRC iemMemStackPopU16(PIEMCPU pIemCpu, uint16_t *pu16Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pCtx, 2, &uNewRsp);

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
static VBOXSTRICTRC iemMemStackPopU32(PIEMCPU pIemCpu, uint32_t *pu32Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pCtx, 4, &uNewRsp);

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
static VBOXSTRICTRC iemMemStackPopU64(PIEMCPU pIemCpu, uint64_t *pu64Value)
{
    /* Increment the stack pointer. */
    uint64_t    uNewRsp;
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pCtx, 8, &uNewRsp);

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
static VBOXSTRICTRC iemMemStackPushU16Ex(PIEMCPU pIemCpu, uint16_t u16Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(&NewRsp, 2, pCtx);

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
static VBOXSTRICTRC iemMemStackPushU32Ex(PIEMCPU pIemCpu, uint32_t u32Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(&NewRsp, 4, pCtx);

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


#ifdef SOME_UNUSED_FUNCTION
/**
 * Pushes a dword onto the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   u64Value            The value to push.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
static VBOXSTRICTRC iemMemStackPushU64Ex(PIEMCPU pIemCpu, uint64_t u64Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPushEx(&NewRsp, 8, pCtx);

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
#endif


/**
 * Pops a word from the stack, using a temporary stack pointer.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pu16Value           Where to store the popped value.
 * @param   pTmpRsp             Pointer to the temporary stack pointer.
 */
static VBOXSTRICTRC iemMemStackPopU16Ex(PIEMCPU pIemCpu, uint16_t *pu16Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(&NewRsp, 2, pCtx);

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
static VBOXSTRICTRC iemMemStackPopU32Ex(PIEMCPU pIemCpu, uint32_t *pu32Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(&NewRsp, 4, pCtx);

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
static VBOXSTRICTRC iemMemStackPopU64Ex(PIEMCPU pIemCpu, uint64_t *pu64Value, PRTUINT64U pTmpRsp)
{
    /* Increment the stack pointer. */
    PCPUMCTX    pCtx = pIemCpu->CTX_SUFF(pCtx);
    RTUINT64U   NewRsp = *pTmpRsp;
    RTGCPTR     GCPtrTop = iemRegGetRspForPopEx(&NewRsp, 8, pCtx);

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
 * This will raise #SS or #PF if appropriate.
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
static VBOXSTRICTRC iemMemStackPushBeginSpecial(PIEMCPU pIemCpu, size_t cbMem, void **ppvMem, uint64_t *puNewRsp)
{
    Assert(cbMem < UINT8_MAX);
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPush(pCtx, (uint8_t)cbMem, puNewRsp);
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
static VBOXSTRICTRC iemMemStackPushCommitSpecial(PIEMCPU pIemCpu, void *pvMem, uint64_t uNewRsp)
{
    VBOXSTRICTRC rcStrict = iemMemCommitAndUnmap(pIemCpu, pvMem, IEM_ACCESS_STACK_W);
    if (rcStrict == VINF_SUCCESS)
        pIemCpu->CTX_SUFF(pCtx)->rsp = uNewRsp;
    return rcStrict;
}


/**
 * Begin a special stack pop (used by iret, retf and such).
 *
 * This will raise #SS or #PF if appropriate.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   cbMem               The number of bytes to push onto the stack.
 * @param   ppvMem              Where to return the pointer to the stack memory.
 * @param   puNewRsp            Where to return the new RSP value.  This must be
 *                              passed unchanged to
 *                              iemMemStackPopCommitSpecial().
 */
static VBOXSTRICTRC iemMemStackPopBeginSpecial(PIEMCPU pIemCpu, size_t cbMem, void const **ppvMem, uint64_t *puNewRsp)
{
    Assert(cbMem < UINT8_MAX);
    PCPUMCTX    pCtx     = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR     GCPtrTop = iemRegGetRspForPop(pCtx, (uint8_t)cbMem, puNewRsp);
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
static VBOXSTRICTRC iemMemStackPopCommitSpecial(PIEMCPU pIemCpu, void const *pvMem, uint64_t uNewRsp)
{
    VBOXSTRICTRC rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pvMem, IEM_ACCESS_STACK_R);
    if (rcStrict == VINF_SUCCESS)
        pIemCpu->CTX_SUFF(pCtx)->rsp = uNewRsp;
    return rcStrict;
}


/**
 * Fetches a descriptor table entry.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU.
 * @param   pDesc               Where to return the descriptor table entry.
 * @param   uSel                The selector which table entry to fetch.
 */
static VBOXSTRICTRC iemMemFetchSelDesc(PIEMCPU pIemCpu, PIEMSELDESC pDesc, uint16_t uSel)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /** @todo did the 286 require all 8 bytes to be accessible? */
    /*
     * Get the selector table base and check bounds.
     */
    RTGCPTR GCPtrBase;
    if (uSel & X86_SEL_LDT)
    {
        if (   !pCtx->ldtrHid.Attr.n.u1Present
            || (uSel | 0x7U) > pCtx->ldtrHid.u32Limit )
        {
            Log(("iemMemFetchSelDesc: LDT selector %#x is out of bounds (%3x) or ldtr is NP (%#x)\n",
                 uSel, pCtx->ldtrHid.u32Limit, pCtx->ldtr));
            /** @todo is this the right exception? */
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }

        Assert(pCtx->ldtrHid.Attr.n.u1Present);
        GCPtrBase = pCtx->ldtrHid.u64Base;
    }
    else
    {
        if ((uSel | 0x7U) > pCtx->gdtr.cbGdt)
        {
            Log(("iemMemFetchSelDesc: GDT selector %#x is out of bounds (%3x)\n", uSel, pCtx->gdtr.cbGdt));
            /** @todo is this the right exception? */
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
        GCPtrBase = pCtx->gdtr.pGdt;
    }

    /*
     * Read the legacy descriptor and maybe the long mode extensions if
     * required.
     */
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pIemCpu, &pDesc->Legacy.u, UINT8_MAX, GCPtrBase + (uSel & X86_SEL_MASK));
    if (rcStrict == VINF_SUCCESS)
    {
        if (   !IEM_IS_LONG_MODE(pIemCpu)
            || pDesc->Legacy.Gen.u1DescType)
            pDesc->Long.au64[1] = 0;
        else if ((uint32_t)(uSel & X86_SEL_MASK) + 15 < (uSel & X86_SEL_LDT ? pCtx->ldtrHid.u32Limit : pCtx->gdtr.cbGdt))
            rcStrict = iemMemFetchDataU64(pIemCpu, &pDesc->Legacy.u, UINT8_MAX, GCPtrBase + (uSel & X86_SEL_MASK));
        else
        {
            Log(("iemMemFetchSelDesc: system selector %#x is out of bounds\n", uSel));
            /** @todo is this the right exception? */
            return iemRaiseGeneralProtectionFaultBySelector(pIemCpu, uSel);
        }
    }
    return rcStrict;
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
static VBOXSTRICTRC iemMemMarkSelDescAccessed(PIEMCPU pIemCpu, uint16_t uSel)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Get the selector table base and calculate the entry address.
     */
    RTGCPTR GCPtr = uSel & X86_SEL_LDT
                  ? pCtx->ldtrHid.u64Base
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
        rcStrict = iemMemMap(pIemCpu, (void **)&pu32, 4, UINT8_MAX, GCPtr, IEM_ACCESS_DATA_RW);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        ASMAtomicBitSet(pu32, 8); /* X86_SEL_TYPE_ACCESSED is 1, but it is preceeded by u8BaseHigh1. */
    }
    else
    {
        /* The misaligned GDT/LDT case, map the whole thing. */
        rcStrict = iemMemMap(pIemCpu, (void **)&pu32, 8, UINT8_MAX, GCPtr, IEM_ACCESS_DATA_RW);
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

    return iemMemCommitAndUnmap(pIemCpu, (void *)pu32, IEM_ACCESS_DATA_RW);
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

#define IEM_MC_ADVANCE_RIP()                            iemRegUpdateRip(pIemCpu)
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
        if (iemFRegFetchFsw(pIemCpu) & X86_FSW_ES) \
            return iemRaiseMathFault(pIemCpu); \
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
#define IEM_MC_ARG_CONST(a_Type, a_Name, a_Value, a_iArg)   a_Type const a_Name = (a_Value)
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
#define IEM_MC_FETCH_EFLAGS(a_EFlags)                   (a_EFlags) = (pIemCpu)->CTX_SUFF(pCtx)->eflags.u
#define IEM_MC_FETCH_EFLAGS_U8(a_EFlags)                (a_EFlags) = (uint8_t)(pIemCpu)->CTX_SUFF(pCtx)->eflags.u
#define IEM_MC_FETCH_FSW(a_u16Fsw)                      (a_u16Fsw) = iemFRegFetchFsw(pIemCpu)

#define IEM_MC_STORE_GREG_U8(a_iGReg, a_u8Value)        *iemGRegRefU8(pIemCpu, (a_iGReg)) = (a_u8Value)
#define IEM_MC_STORE_GREG_U16(a_iGReg, a_u16Value)      *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (a_u16Value)
#define IEM_MC_STORE_GREG_U32(a_iGReg, a_u32Value)      *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (uint32_t)(a_u32Value) /* clear high bits. */
#define IEM_MC_STORE_GREG_U64(a_iGReg, a_u64Value)      *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (a_u64Value)
#define IEM_MC_STORE_GREG_U8_CONST                      IEM_MC_STORE_GREG_U8
#define IEM_MC_STORE_GREG_U16_CONST                     IEM_MC_STORE_GREG_U16
#define IEM_MC_STORE_GREG_U32_CONST                     IEM_MC_STORE_GREG_U32
#define IEM_MC_STORE_GREG_U64_CONST                     IEM_MC_STORE_GREG_U64
#define IEM_MC_CLEAR_HIGH_GREG_U64(a_iGReg)             *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) &= UINT32_MAX

#define IEM_MC_REF_GREG_U8(a_pu8Dst, a_iGReg)           (a_pu8Dst) = iemGRegRefU8(pIemCpu, (a_iGReg))
#define IEM_MC_REF_GREG_U16(a_pu16Dst, a_iGReg)         (a_pu16Dst) = (uint16_t *)iemGRegRef(pIemCpu, (a_iGReg))
/** @todo User of IEM_MC_REF_GREG_U32 needs to clear the high bits on
 *        commit. */
#define IEM_MC_REF_GREG_U32(a_pu32Dst, a_iGReg)         (a_pu32Dst) = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg))
#define IEM_MC_REF_GREG_U64(a_pu64Dst, a_iGReg)         (a_pu64Dst) = (uint64_t *)iemGRegRef(pIemCpu, (a_iGReg))
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


#define IEM_MC_SET_EFL_BIT(a_fBit)                      do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u |= (a_fBit); } while (0)
#define IEM_MC_CLEAR_EFL_BIT(a_fBit)                    do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u &= ~(a_fBit); } while (0)
#define IEM_MC_FLIP_EFL_BIT(a_fBit)                     do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u ^= (a_fBit); } while (0)



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

#define IEM_MC_FETCH_MEM_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &(a_u32Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U32_DISP(a_u32Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &(a_u32Dst), (a_iSeg), (a_GCPtrMem) + (a_offDisp)))

#define IEM_MC_FETCH_MEM_S32_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataS32SxU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem)))

#define IEM_MC_FETCH_MEM_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U64_DISP(a_u64Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem) + (a_offDisp)))

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

#define IEM_MC_PUSH_U16(a_u16Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPushU16(pIemCpu, (a_u16Value)))
#define IEM_MC_PUSH_U32(a_u32Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStackPushU32(pIemCpu, (a_u32Value)))
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

/** Calculate efficient address from R/M. */
#define IEM_MC_CALC_RM_EFF_ADDR(a_GCPtrEff, bRm) \
    IEM_MC_RETURN_ON_FAILURE(iemOpHlpCalcRmEffAddr(pIemCpu, (bRm), &(a_GCPtrEff)))

#define IEM_MC_CALL_VOID_AIMPL_2(a_pfn, a0, a1)           (a_pfn)((a0), (a1))
#define IEM_MC_CALL_VOID_AIMPL_3(a_pfn, a0, a1, a2)       (a_pfn)((a0), (a1), (a2))
#define IEM_MC_CALL_VOID_AIMPL_4(a_pfn, a0, a1, a2, a3)   (a_pfn)((a0), (a1), (a2), (a3))
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
 * and returns, taking two arguments in addition to the standard ones.
 *
 * @param   a_pfnCImpl      The pointer to the C routine.
 * @param   a0              The first extra argument.
 * @param   a1              The second extra argument.
 * @param   a2              The third extra argument.
 */
#define IEM_MC_CALL_CIMPL_3(a_pfnCImpl, a0, a1, a2)     return (a_pfnCImpl)(pIemCpu, pIemCpu->offOpcode, a0, a1, a2)

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

#define IEM_MC_IF_EFL_BIT_SET(a_fBit)                   if (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) {
#define IEM_MC_IF_EFL_BIT_NOT_SET(a_fBit)               if (!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit))) {
#define IEM_MC_IF_EFL_ANY_BITS_SET(a_fBits)             if (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBits)) {
#define IEM_MC_IF_EFL_NO_BITS_SET(a_fBits)              if (!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBits))) {
#define IEM_MC_IF_EFL_BITS_NE(a_fBit1, a_fBit2)         \
    if (   !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
        != !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
#define IEM_MC_IF_EFL_BITS_EQ(a_fBit1, a_fBit2)         \
    if (   !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
        == !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
#define IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(a_fBit, a_fBit1, a_fBit2) \
    if (   (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) \
        ||    !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
           != !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
#define IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ(a_fBit, a_fBit1, a_fBit2) \
    if (   !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) \
        &&    !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
           == !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
#define IEM_MC_IF_CX_IS_NZ()                            if (pIemCpu->CTX_SUFF(pCtx)->cx != 0) {
#define IEM_MC_IF_ECX_IS_NZ()                           if (pIemCpu->CTX_SUFF(pCtx)->ecx != 0) {
#define IEM_MC_IF_RCX_IS_NZ()                           if (pIemCpu->CTX_SUFF(pCtx)->rcx != 0) {
#define IEM_MC_IF_CX_IS_NZ_AND_EFL_BIT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->cx != 0 \
            && (pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_ECX_IS_NZ_AND_EFL_BIT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->ecx != 0 \
            && (pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_RCX_IS_NZ_AND_EFL_BIT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->rcx != 0 \
            && (pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_CX_IS_NZ_AND_EFL_BIT_NOT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->cx != 0 \
            && !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_ECX_IS_NZ_AND_EFL_BIT_NOT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->ecx != 0 \
            && !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_RCX_IS_NZ_AND_EFL_BIT_NOT_SET(a_fBit) \
        if (   pIemCpu->CTX_SUFF(pCtx)->rcx != 0 \
            && !(pIemCpu->CTX_SUFF(pCtx)->eflags.u & a_fBit)) {
#define IEM_MC_IF_LOCAL_IS_Z(a_Local)                   if ((a_Local) == 0) {
#define IEM_MC_IF_GREG_BIT_SET(a_iGReg, a_iBitNo)       if (*(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) & RT_BIT_64(a_iBitNo)) {
#define IEM_MC_ELSE()                                   } else {
#define IEM_MC_ENDIF()                                  } do {} while (0)

/** @}  */


/** @name   Opcode Debug Helpers.
 * @{
 */
#ifdef DEBUG
# define IEMOP_MNEMONIC(a_szMnemonic) \
    Log2(("decode - %04x:%RGv %s%s [#%u]\n", pIemCpu->CTX_SUFF(pCtx)->cs, pIemCpu->CTX_SUFF(pCtx)->rip, \
          pIemCpu->fPrefixes & IEM_OP_PRF_LOCK ? "lock " : "", a_szMnemonic, pIemCpu->cInstructions))
# define IEMOP_MNEMONIC2(a_szMnemonic, a_szOps) \
    Log2(("decode - %04x:%RGv %s%s %s [#%u]\n", pIemCpu->CTX_SUFF(pCtx)->cs, pIemCpu->CTX_SUFF(pCtx)->rip, \
          pIemCpu->fPrefixes & IEM_OP_PRF_LOCK ? "lock " : "", a_szMnemonic, a_szOps, pIemCpu->cInstructions))
#else
# define IEMOP_MNEMONIC(a_szMnemonic) do { } while (0)
# define IEMOP_MNEMONIC2(a_szMnemonic, a_szOps) do { } while (0)
#endif

/** @} */


/** @name   Opcode Helpers.
 * @{
 */

/** The instruction allows no lock prefixing (in this encoding), throw #UD if
 * lock prefixed. */
#define IEMOP_HLP_NO_LOCK_PREFIX() \
    do \
    { \
        if (pIemCpu->fPrefixes & IEM_OP_PRF_LOCK) \
            return IEMOP_RAISE_INVALID_LOCK_PREFIX(); \
    } while (0)

/** The instruction is not available in 64-bit mode, throw #UD if we're in
 * 64-bit mode. */
#define IEMOP_HLP_NO_64BIT() \
    do \
    { \
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT) \
            return IEMOP_RAISE_INVALID_OPCODE(); \
    } while (0)

/** The instruction defaults to 64-bit operand size if 64-bit mode. */
#define IEMOP_HLP_DEFAULT_64BIT_OP_SIZE() \
    do \
    { \
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT) \
            iemRecalEffOpSize64Default(pIemCpu); \
    } while (0)



/**
 * Calculates the effective address of a ModR/M memory operand.
 *
 * Meant to be used via IEM_MC_CALC_RM_EFF_ADDR.
 *
 * @return  Strict VBox status code.
 * @param   pIemCpu             The IEM per CPU data.
 * @param   bRm                 The ModRM byte.
 * @param   pGCPtrEff           Where to return the effective address.
 */
static VBOXSTRICTRC iemOpHlpCalcRmEffAddr(PIEMCPU pIemCpu, uint8_t bRm, PRTGCPTR pGCPtrEff)
{
    LogFlow(("iemOpHlpCalcRmEffAddr: bRm=%#x\n", bRm));
    PCCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
#define SET_SS_DEF() \
    do \
    { \
        if (!(pIemCpu->fPrefixes & IEM_OP_PRF_SEG_MASK)) \
            pIemCpu->iEffSeg = X86_SREG_SS; \
    } while (0)

/** @todo Check the effective address size crap! */
    switch (pIemCpu->enmEffAddrMode)
    {
        case IEMMODE_16BIT:
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
                    default: AssertFailedReturn(VERR_INTERNAL_ERROR_2); /* (caller checked for these) */
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
            LogFlow(("iemOpHlpCalcRmEffAddr: EffAddr=%#06RGv\n", *pGCPtrEff));
            return VINF_SUCCESS;
        }

        case IEMMODE_32BIT:
        {
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
                        AssertFailedReturn(VERR_INTERNAL_ERROR_2); /* (caller checked for these) */
                }

            }
            if (pIemCpu->enmEffAddrMode == IEMMODE_32BIT)
                *pGCPtrEff = u32EffAddr;
            else
            {
                Assert(pIemCpu->enmEffAddrMode == IEMMODE_16BIT);
                *pGCPtrEff = u32EffAddr & UINT16_MAX;
            }
            LogFlow(("iemOpHlpCalcRmEffAddr: EffAddr=%#010RGv\n", *pGCPtrEff));
            return VINF_SUCCESS;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64EffAddr;

            /* Handle the rip+disp32 form with no registers first. */
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
            {
                IEM_OPCODE_GET_NEXT_S32_SX_U64(&u64EffAddr);
                u64EffAddr += pCtx->rip + pIemCpu->offOpcode;
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
                        switch (((bSib & X86_SIB_INDEX_SHIFT) >> X86_SIB_INDEX_SMASK) | pIemCpu->uRexIndex)
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
                *pGCPtrEff = u64EffAddr & UINT16_MAX;
            LogFlow(("iemOpHlpCalcRmEffAddr: EffAddr=%#010RGv\n", *pGCPtrEff));
            return VINF_SUCCESS;
        }
    }

    AssertFailedReturn(VERR_INTERNAL_ERROR_3);
}

/** @}  */



/*
 * Include the instructions
 */
#include "IEMAllInstructions.cpp.h"




#if defined(IEM_VERIFICATION_MODE) && defined(IN_RING3)

/**
 * Sets up execution verification mode.
 */
static void iemExecVerificationModeSetup(PIEMCPU pIemCpu)
{
    PVMCPU   pVCpu   = IEMCPU_TO_VMCPU(pIemCpu);
    PCPUMCTX pOrgCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Enable verification and/or logging.
     */
    pIemCpu->fNoRem  = !LogIs6Enabled(); /* logging triggers the no-rem/rem verification stuff */
    if (    pIemCpu->fNoRem
#if 0 /* auto enable on first paged protected mode interrupt */
        && pOrgCtx->eflags.Bits.u1IF
        && (pOrgCtx->cr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG)
        && TRPMHasTrap(pVCpu)
        && EMGetInhibitInterruptsPC(pVCpu) != pOrgCtx->rip)
#endif
#if 0
        &&  pOrgCtx->cs  == 0x10
        &&  (   pOrgCtx->rip == 0x90119e3e
             || pOrgCtx->rip == 0x901d9810
            )
#endif
#if 0 /* Auto enable; DSL. */
        &&  pOrgCtx->cs  == 0x10
        &&  (   pOrgCtx->rip == 0x00100fc7
             || pOrgCtx->rip == 0x00100ffc
             || pOrgCtx->rip == 0x00100ffe
            )
#endif
#if 0
       && 0
#endif
       )
    {
        RTLogGroupSettings(NULL, "iem.eo.l6.l2");
        RTLogFlags(NULL, "enabled");
        pIemCpu->fNoRem = false;
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
    if (   pOrgCtx->eflags.Bits.u1IF
        && TRPMHasTrap(pVCpu)
        && EMGetInhibitInterruptsPC(pVCpu) != pOrgCtx->rip)
    {
        uint8_t     u8TrapNo;
        TRPMEVENT   enmType;
        RTGCUINT    uErrCode;
        RTGCPTR     uCr2;
        int rc2 = TRPMQueryTrapAll(pVCpu, &u8TrapNo, &enmType, &uErrCode, &uCr2); AssertRC(rc2);
        IEMInjectTrap(pVCpu, u8TrapNo, enmType, (uint16_t)uErrCode, uCr2);
        if (!IEM_VERIFICATION_ENABLED(pIemCpu))
            TRPMResetTrap(pVCpu);
    }

    /*
     * Reset the counters.
     */
    pIemCpu->cIOReads    = 0;
    pIemCpu->cIOWrites   = 0;
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
 * @returns Poitner to a record.
 */
static PIEMVERIFYEVTREC iemVerifyAllocRecord(PIEMCPU pIemCpu)
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
    pEvtRec->u.IOPortRead.cbValue = (uint32_t)cbValue;
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
    pEvtRec->u.IOPortWrite.cbValue  = (uint32_t)cbValue;
    pEvtRec->u.IOPortWrite.u32Value = u32Value;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
}


VMM_INT_DECL(void)   IEMNotifyIOPortReadString(PVM pVM, RTIOPORT Port, RTGCPTR GCPtrDst, RTGCUINTREG cTransfers, size_t cbValue)
{
    AssertFailed();
}


VMM_INT_DECL(void)   IEMNotifyIOPortWriteString(PVM pVM, RTIOPORT Port, RTGCPTR GCPtrSrc, RTGCUINTREG cTransfers, size_t cbValue)
{
    AssertFailed();
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
static VBOXSTRICTRC iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue)
{
    PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (pEvtRec)
    {
        pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_READ;
        pEvtRec->u.IOPortRead.Port    = Port;
        pEvtRec->u.IOPortRead.cbValue = (uint32_t)cbValue;
        pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
        *pIemCpu->ppIemEvtRecNext = pEvtRec;
    }
    pIemCpu->cIOReads++;
    *pu32Value = 0xffffffff;
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
static VBOXSTRICTRC iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
    PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (pEvtRec)
    {
        pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_WRITE;
        pEvtRec->u.IOPortWrite.Port     = Port;
        pEvtRec->u.IOPortWrite.cbValue  = (uint32_t)cbValue;
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
static void iemVerifyAssertMsg2(PIEMCPU pIemCpu)
{
    PCPUMCTX pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVM      pVM   = IEMCPU_TO_VM(pIemCpu);
    PVMCPU   pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    char szRegs[4096];
    DBGFR3RegPrintf(pVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
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
    DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, pCtx->cs, pCtx->rip - pIemCpu->offOpcode,
                       DBGF_DISAS_FLAGS_DEFAULT_MODE,
                       szInstr1, sizeof(szInstr1), NULL);
    char szInstr2[256];
    DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, 0, 0,
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
static void iemVerifyAssertAddRecordDump(PIEMVERIFYEVTREC pEvtRec)
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
        case IEMVERIFYEVENT_RAM_READ:
            RTAssertMsg2Add("RAM READ  at %RGp, %#4zx bytes\n",
                            pEvtRec->u.RamRead.GCPhys,
                            pEvtRec->u.RamRead.cb);
            break;
        case IEMVERIFYEVENT_RAM_WRITE:
            RTAssertMsg2Add("RAM WRITE at %RGp, %#4zx bytes: %.*RHxs\n",
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
static void iemVerifyAssertRecords(PIEMCPU pIemCpu, PIEMVERIFYEVTREC pEvtRec1, PIEMVERIFYEVTREC pEvtRec2, const char *pszMsg)
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
static void iemVerifyAssertRecord(PIEMCPU pIemCpu, PIEMVERIFYEVTREC pEvtRec, const char *pszMsg)
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
 */
static void iemVerifyWriteRecord(PIEMCPU pIemCpu, PIEMVERIFYEVTREC pEvtRec)
{
    uint8_t abBuf[sizeof(pEvtRec->u.RamWrite.ab)]; RT_ZERO(abBuf);
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
            /* fend off ROMs */
            if (   pEvtRec->u.RamWrite.GCPhys - UINT32_C(0x000c0000) > UINT32_C(0x8000)
                && pEvtRec->u.RamWrite.GCPhys - UINT32_C(0x000e0000) > UINT32_C(0x20000)
                && pEvtRec->u.RamWrite.GCPhys - UINT32_C(0xfffc0000) > UINT32_C(0x40000) )
            {
                RTAssertMsg1(NULL, __LINE__, __FILE__, __PRETTY_FUNCTION__);
                RTAssertMsg2Weak("Memory at %RGv differs\n", pEvtRec->u.RamWrite.GCPhys);
                RTAssertMsg2Add("REM: %.*Rhxs\n"
                                "IEM: %.*Rhxs\n",
                                pEvtRec->u.RamWrite.cb, abBuf,
                                pEvtRec->u.RamWrite.cb, pEvtRec->u.RamWrite.ab);
                iemVerifyAssertAddRecordDump(pEvtRec);
                iemVerifyAssertMsg2(pIemCpu);
                RTAssertPanic();
            }
        }
    }

}

/**
 * Performs the post-execution verfication checks.
 */
static void iemExecVerificationModeCheck(PIEMCPU pIemCpu)
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
    PVM pVM = IEMCPU_TO_VM(pIemCpu);
    EMRemLock(pVM);
    int rc = REMR3EmulateInstruction(pVM, IEMCPU_TO_VMCPU(pIemCpu));
    AssertRC(rc);
    EMRemUnlock(pVM);

    /*
     * Compare the register states.
     */
    unsigned cDiffs = 0;
    if (memcmp(pOrgCtx, pDebugCtx, sizeof(*pDebugCtx)))
    {
        Log(("REM and IEM ends up with different registers!\n"));

#  define CHECK_FIELD(a_Field) \
        do \
        { \
            if (pOrgCtx->a_Field != pDebugCtx->a_Field) \
            { \
                switch (sizeof(pOrgCtx->a_Field)) \
                { \
                    case 1: RTAssertMsg2Weak("  %8s differs - iem=%02x - rem=%02x\n", #a_Field, pDebugCtx->a_Field, pOrgCtx->a_Field); break; \
                    case 2: RTAssertMsg2Weak("  %8s differs - iem=%04x - rem=%04x\n", #a_Field, pDebugCtx->a_Field, pOrgCtx->a_Field); break; \
                    case 4: RTAssertMsg2Weak("  %8s differs - iem=%08x - rem=%08x\n", #a_Field, pDebugCtx->a_Field, pOrgCtx->a_Field); break; \
                    case 8: RTAssertMsg2Weak("  %8s differs - iem=%016llx - rem=%016llx\n", #a_Field, pDebugCtx->a_Field, pOrgCtx->a_Field); break; \
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
                RTAssertMsg2Weak("  %8s differs - iem=%02x - rem=%02x\n", #a_Field, pDebugCtx->a_Field, pOrgCtx->a_Field); \
                cDiffs++; \
            } \
        } while (0)

#  define CHECK_SEL(a_Sel) \
        do \
        { \
            CHECK_FIELD(a_Sel); \
            if (   pOrgCtx->a_Sel##Hid.Attr.u != pDebugCtx->a_Sel##Hid.Attr.u \
                && (pOrgCtx->a_Sel##Hid.Attr.u | X86_SEL_TYPE_ACCESSED) != pDebugCtx->a_Sel##Hid.Attr.u) \
            { \
                RTAssertMsg2Weak("  %8sHid.Attr differs - iem=%02x - rem=%02x\n", #a_Sel, pDebugCtx->a_Sel##Hid.Attr.u, pOrgCtx->a_Sel##Hid.Attr.u); \
                cDiffs++; \
            } \
            CHECK_FIELD(a_Sel##Hid.u64Base); \
            CHECK_FIELD(a_Sel##Hid.u32Limit); \
        } while (0)

        if (memcmp(&pOrgCtx->fpu, &pDebugCtx->fpu, sizeof(pDebugCtx->fpu)))
        {
            RTAssertMsg2Weak("  the FPU state differs\n");
            cDiffs++;
            CHECK_FIELD(fpu.FCW);
            CHECK_FIELD(fpu.FSW);
            CHECK_FIELD(fpu.FTW);
            CHECK_FIELD(fpu.FOP);
            CHECK_FIELD(fpu.FPUIP);
            CHECK_FIELD(fpu.CS);
            CHECK_FIELD(fpu.Rsrvd1);
            CHECK_FIELD(fpu.FPUDP);
            CHECK_FIELD(fpu.DS);
            CHECK_FIELD(fpu.Rsrvd2);
            CHECK_FIELD(fpu.MXCSR);
            CHECK_FIELD(fpu.MXCSR_MASK);
            CHECK_FIELD(fpu.aRegs[0].au64[0]); CHECK_FIELD(fpu.aRegs[0].au64[1]);
            CHECK_FIELD(fpu.aRegs[1].au64[0]); CHECK_FIELD(fpu.aRegs[1].au64[1]);
            CHECK_FIELD(fpu.aRegs[2].au64[0]); CHECK_FIELD(fpu.aRegs[2].au64[1]);
            CHECK_FIELD(fpu.aRegs[3].au64[0]); CHECK_FIELD(fpu.aRegs[3].au64[1]);
            CHECK_FIELD(fpu.aRegs[4].au64[0]); CHECK_FIELD(fpu.aRegs[4].au64[1]);
            CHECK_FIELD(fpu.aRegs[5].au64[0]); CHECK_FIELD(fpu.aRegs[5].au64[1]);
            CHECK_FIELD(fpu.aRegs[6].au64[0]); CHECK_FIELD(fpu.aRegs[6].au64[1]);
            CHECK_FIELD(fpu.aRegs[7].au64[0]); CHECK_FIELD(fpu.aRegs[7].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 0].au64[0]);  CHECK_FIELD(fpu.aXMM[ 0].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 1].au64[0]);  CHECK_FIELD(fpu.aXMM[ 1].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 2].au64[0]);  CHECK_FIELD(fpu.aXMM[ 2].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 3].au64[0]);  CHECK_FIELD(fpu.aXMM[ 3].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 4].au64[0]);  CHECK_FIELD(fpu.aXMM[ 4].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 5].au64[0]);  CHECK_FIELD(fpu.aXMM[ 5].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 6].au64[0]);  CHECK_FIELD(fpu.aXMM[ 6].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 7].au64[0]);  CHECK_FIELD(fpu.aXMM[ 7].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 8].au64[0]);  CHECK_FIELD(fpu.aXMM[ 8].au64[1]);
            CHECK_FIELD(fpu.aXMM[ 9].au64[0]);  CHECK_FIELD(fpu.aXMM[ 9].au64[1]);
            CHECK_FIELD(fpu.aXMM[10].au64[0]);  CHECK_FIELD(fpu.aXMM[10].au64[1]);
            CHECK_FIELD(fpu.aXMM[11].au64[0]);  CHECK_FIELD(fpu.aXMM[11].au64[1]);
            CHECK_FIELD(fpu.aXMM[12].au64[0]);  CHECK_FIELD(fpu.aXMM[12].au64[1]);
            CHECK_FIELD(fpu.aXMM[13].au64[0]);  CHECK_FIELD(fpu.aXMM[13].au64[1]);
            CHECK_FIELD(fpu.aXMM[14].au64[0]);  CHECK_FIELD(fpu.aXMM[14].au64[1]);
            CHECK_FIELD(fpu.aXMM[15].au64[0]);  CHECK_FIELD(fpu.aXMM[15].au64[1]);
            for (unsigned i = 0; i < RT_ELEMENTS(pOrgCtx->fpu.au32RsrvdRest); i++)
                CHECK_FIELD(fpu.au32RsrvdRest[i]);
        }
        CHECK_FIELD(rip);
        uint32_t fFlagsMask = UINT32_MAX & ~pIemCpu->fUndefinedEFlags;
        if ((pOrgCtx->rflags.u & fFlagsMask) != (pDebugCtx->rflags.u & fFlagsMask))
        {
            RTAssertMsg2Weak("  rflags differs - iem=%08llx rem=%08llx\n", pDebugCtx->rflags.u, pOrgCtx->rflags.u);
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
        CHECK_FIELD(cr2);
        CHECK_FIELD(cr3);
        CHECK_FIELD(cr4);
        CHECK_FIELD(dr[0]);
        CHECK_FIELD(dr[1]);
        CHECK_FIELD(dr[2]);
        CHECK_FIELD(dr[3]);
        CHECK_FIELD(dr[6]);
        CHECK_FIELD(dr[7]);
        CHECK_FIELD(gdtr.cbGdt);
        CHECK_FIELD(gdtr.pGdt);
        CHECK_FIELD(idtr.cbIdt);
        CHECK_FIELD(idtr.pIdt);
        CHECK_FIELD(ldtr);
        CHECK_FIELD(ldtrHid.u64Base);
        CHECK_FIELD(ldtrHid.u32Limit);
        CHECK_FIELD(ldtrHid.Attr.u);
        CHECK_FIELD(tr);
        CHECK_FIELD(trHid.u64Base);
        CHECK_FIELD(trHid.u32Limit);
        CHECK_FIELD(trHid.Attr.u);
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
    if (cDiffs == 0)
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
                    iemVerifyWriteRecord(pIemCpu, pIemRec);
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
                    fEquals = pIemRec->u.IOPortRead.Port        == pOtherRec->u.IOPortRead.Port
                           && pIemRec->u.IOPortRead.cbValue     == pOtherRec->u.IOPortRead.cbValue;
                    break;
                case IEMVERIFYEVENT_IOPORT_WRITE:
                    fEquals = pIemRec->u.IOPortWrite.Port       == pOtherRec->u.IOPortWrite.Port
                           && pIemRec->u.IOPortWrite.cbValue    == pOtherRec->u.IOPortWrite.cbValue
                           && pIemRec->u.IOPortWrite.u32Value   == pOtherRec->u.IOPortWrite.u32Value;
                    break;
                case IEMVERIFYEVENT_RAM_READ:
                    fEquals = pIemRec->u.RamRead.GCPhys         == pOtherRec->u.RamRead.GCPhys
                           && pIemRec->u.RamRead.cb             == pOtherRec->u.RamRead.cb;
                    break;
                case IEMVERIFYEVENT_RAM_WRITE:
                    fEquals = pIemRec->u.RamWrite.GCPhys        == pOtherRec->u.RamWrite.GCPhys
                           && pIemRec->u.RamWrite.cb            == pOtherRec->u.RamWrite.cb
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
                iemVerifyWriteRecord(pIemCpu, pIemRec);
            pIemRec = pIemRec->pNext;
        }
        if (pIemRec != NULL)
            iemVerifyAssertRecord(pIemCpu, pIemRec, "Extra IEM record!");
        else if (pOtherRec != NULL)
            iemVerifyAssertRecord(pIemCpu, pIemRec, "Extra Other record!");
    }
    pIemCpu->CTX_SUFF(pCtx) = pOrgCtx;

#if 0
    /*
     * HACK ALERT! You don't normally want to verify a whole boot sequence.
     */
    if (pIemCpu->cInstructions == 1)
        RTLogFlags(NULL, "disabled");
#endif
}

#else  /* !IEM_VERIFICATION_MODE || !IN_RING3 */

/* stubs */
static VBOXSTRICTRC     iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue)
{
    return VERR_INTERNAL_ERROR;
}

static VBOXSTRICTRC     iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
    return VERR_INTERNAL_ERROR;
}

#endif /* !IEM_VERIFICATION_MODE || !IN_RING3 */


/**
 * Execute one instruction.
 *
 * @return  Strict VBox status code.
 * @param   pVCpu       The current virtual CPU.
 */
VMMDECL(VBOXSTRICTRC) IEMExecOne(PVMCPU pVCpu)
{
    PIEMCPU  pIemCpu = &pVCpu->iem.s;

#if defined(IEM_VERIFICATION_MODE) && defined(IN_RING3)
    iemExecVerificationModeSetup(pIemCpu);
#endif
#ifdef LOG_ENABLED
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    if (LogIs2Enabled())
    {
        char     szInstr[256];
        uint32_t cbInstr = 0;
        DBGFR3DisasInstrEx(pVCpu->pVMR3, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), &cbInstr);

        Log2(("**** "
              " eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
              " eip=%08x esp=%08x ebp=%08x iopl=%d\n"
              " cs=%04x ss=%04x ds=%04x es=%04x fs=%04x gs=%04x efl=%08x\n"
              " %s\n"
              ,
              pCtx->eax, pCtx->ebx, pCtx->ecx, pCtx->edx, pCtx->esi, pCtx->edi,
              pCtx->eip, pCtx->esp, pCtx->ebp, pCtx->eflags.Bits.u2IOPL,
              (RTSEL)pCtx->cs, (RTSEL)pCtx->ss, (RTSEL)pCtx->ds, (RTSEL)pCtx->es,
              (RTSEL)pCtx->fs, (RTSEL)pCtx->gs, pCtx->eflags.u,
              szInstr));
    }
#endif

    /*
     * Do the decoding and emulation.
     */
    VBOXSTRICTRC rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    uint8_t b; IEM_OPCODE_GET_NEXT_U8(&b);
    rcStrict = FNIEMOP_CALL(g_apfnOneByteMap[b]);
    if (rcStrict == VINF_SUCCESS)
        pIemCpu->cInstructions++;
//#ifdef DEBUG
//    AssertMsg(pIemCpu->offOpcode == cbInstr || rcStrict != VINF_SUCCESS, ("%u %u\n", pIemCpu->offOpcode, cbInstr));
//#endif

    /* Execute the next instruction as well if a cli, pop ss or
       mov ss, Gr has just completed successfully. */
    if (   rcStrict == VINF_SUCCESS
        && VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && EMGetInhibitInterruptsPC(pVCpu) == pIemCpu->CTX_SUFF(pCtx)->rip )
    {
        rcStrict = iemInitDecoderAndPrefetchOpcodes(pIemCpu);
        if (rcStrict == VINF_SUCCESS)
        {
            b; IEM_OPCODE_GET_NEXT_U8(&b);
            rcStrict = FNIEMOP_CALL(g_apfnOneByteMap[b]);
            if (rcStrict == VINF_SUCCESS)
                pIemCpu->cInstructions++;
        }
        EMSetInhibitInterruptsPC(pVCpu, UINT64_C(0x7777555533331111));
    }

    /*
     * Assert some sanity.
     */
#if defined(IEM_VERIFICATION_MODE) && defined(IN_RING3)
    iemExecVerificationModeCheck(pIemCpu);
#endif
    return rcStrict;
}


/**
 * Injects a trap, fault, abort, software interrupt or external interrupt.
 *
 * The parameter list matches TRPMQueryTrapAll pretty closely.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu               The current virtual CPU.
 * @param   u8TrapNo            The trap number.
 * @param   enmType             What type is it (trap/fault/abort), software
 *                              interrupt or hardware interrupt.
 * @param   uErrCode            The error code if applicable.
 * @param   uCr2                The CR2 value if applicable.
 */
VMM_INT_DECL(VBOXSTRICTRC) IEMInjectTrap(PVMCPU pVCpu, uint8_t u8TrapNo, TRPMEVENT enmType, uint16_t uErrCode, RTGCPTR uCr2)
{
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
            }
            break;

        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    return iemRaiseXcptOrInt(&pVCpu->iem.s, 0, u8TrapNo, fFlags, uErrCode, uCr2);
}

