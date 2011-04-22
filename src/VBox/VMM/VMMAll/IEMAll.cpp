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
//#define RT_STRICT
//#define LOG_ENABLED
#define LOG_GROUP   LOG_GROUP_EM /** @todo add log group */
#include <VBox/vmm/iem.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
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
#include <VBox/x86.h>
#include <iprt/assert.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
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
    static VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0) RT_NO_THROW
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    static VBOXSTRICTRC __attribute__((__fastcall__, __nothrow__)) a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW

#elif defined(_MSC_VER) && defined(RT_ARCH_X86)
typedef VBOXSTRICTRC (__fastcall * PFNIEMOP)(PIEMCPU pIemCpu);
# define FNIEMOP_DEF(a_Name) \
    static /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu) RT_NO_THROW
# define FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    static /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0) RT_NO_THROW
# define FNIEMOP_DEF_2(a_Name, a_Type0, a_Name0, a_Type1, a_Name1) \
    static /*__declspec(naked)*/ VBOXSTRICTRC __fastcall a_Name(PIEMCPU pIemCpu, a_Type0 a_Name0, a_Type1 a_Name1) RT_NO_THROW

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
 * Function table for a binary operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPBINSIZES
{
    PFNIEMAIMPLBINU8  pfnNormalU8,    pfnLockedU8;
    PFNIEMAIMPLBINU16 pfnNormalU16,   pfnLockedU16;
    PFNIEMAIMPLBINU32 pfnNormalU32,   pfnLockedU32;
    PFNIEMAIMPLBINU64 pfnNormalU64,   pfnLockedU64;
} IEMOPBINSIZES;
/** Pointer to a binary operator function table. */
typedef IEMOPBINSIZES const *PCIEMOPBINSIZES;


/**
 * Function table for a unary operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPUNARYSIZES
{
    PFNIEMAIMPLUNARYU8  pfnNormalU8,    pfnLockedU8;
    PFNIEMAIMPLUNARYU16 pfnNormalU16,   pfnLockedU16;
    PFNIEMAIMPLUNARYU32 pfnNormalU32,   pfnLockedU32;
    PFNIEMAIMPLUNARYU64 pfnNormalU64,   pfnLockedU64;
} IEMOPUNARYSIZES;
/** Pointer to a unary operator function table. */
typedef IEMOPUNARYSIZES const *PCIEMOPUNARYSIZES;


/**
 * Function table for a shift operator providing implementation based on
 * operand size.
 */
typedef struct IEMOPSHIFTSIZES
{
    PFNIEMAIMPLSHIFTU8  pfnNormalU8;
    PFNIEMAIMPLSHIFTU16 pfnNormalU16;
    PFNIEMAIMPLSHIFTU32 pfnNormalU32;
    PFNIEMAIMPLSHIFTU64 pfnNormalU64;
} IEMOPSHIFTSIZES;
/** Pointer to a shift operator function table. */
typedef IEMOPSHIFTSIZES const *PCIEMOPSHIFTSIZES;


/**
 * Function table for a multiplication or division operation.
 */
typedef struct IEMOPMULDIVSIZES
{
    PFNIEMAIMPLMULDIVU8  pfnU8;
    PFNIEMAIMPLMULDIVU16 pfnU16;
    PFNIEMAIMPLMULDIVU32 pfnU32;
    PFNIEMAIMPLMULDIVU64 pfnU64;
} IEMOPMULDIVSIZES;
/** Pointer to a multiplication or division operation function table. */
typedef IEMOPMULDIVSIZES const *PCIEMOPMULDIVSIZES;


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
/** Temporary hack to disable the double execution.  Will be removed in favor
 * of a dedicated execution mode in EM. */
#define IEM_VERIFICATION_MODE_NO_REM

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


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static VBOXSTRICTRC     iemRaiseGeneralProtectionFault0(PIEMCPU pIemCpu);
static VBOXSTRICTRC     iemRaiseSelectorBounds(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
static VBOXSTRICTRC     iemRaiseSelectorInvalidAccess(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
static VBOXSTRICTRC     iemRaiseSelectorNotPresent(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess);
static VBOXSTRICTRC     iemRaisePageFault(PIEMCPU pIemCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc);
#if defined(IEM_VERIFICATION_MODE) && !defined(IEM_VERIFICATION_MODE_NO_REM)
static PIEMVERIFYEVTREC iemVerifyAllocRecord(PIEMCPU pIemCpu);
static VBOXSTRICTRC     iemVerifyFakeIOPortRead(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t *pu32Value, size_t cbValue);
static VBOXSTRICTRC     iemVerifyFakeIOPortWrite(PIEMCPU pIemCpu, RTIOPORT Port, uint32_t u32Value, size_t cbValue);
#endif


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
    if (offPrevOpcodes < cbOldOpcodes)
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
    uint32_t    cbToTryRead;
    RTGCPTR     GCPtrNext;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
    {
        cbToTryRead = PAGE_SIZE;
        GCPtrNext   = pCtx->rip + pIemCpu->cbOpcode;
        if (!IEM_IS_CANONICAL(GCPtrNext))
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        cbToTryRead = PAGE_SIZE - (GCPtrNext & PAGE_OFFSET_MASK);
        Assert(cbToTryRead >= cbMin); /* ASSUMPTION based on iemInitDecoderAndPrefetchOpcodes. */
    }
    else
    {
        uint32_t GCPtrNext32 = pCtx->eip;
        Assert(!(GCPtrNext32 & ~(uint32_t)UINT16_MAX) || pIemCpu->enmCpuMode == IEMMODE_32BIT);
        GCPtrNext32 += pIemCpu->cbOpcode;
        if (GCPtrNext32 > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        cbToTryRead = pCtx->csHid.u32Limit - GCPtrNext32 + 1;
        if (cbToTryRead < cbMin)
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
    if (!pIemCpu->fByPassHandlers)
        rc = PGMPhysRead(IEMCPU_TO_VM(pIemCpu), GCPhys, &pIemCpu->abOpcode[pIemCpu->cbOpcode], cbToTryRead);
    else
        rc = PGMPhysSimpleReadGCPhys(IEMCPU_TO_VM(pIemCpu), &pIemCpu->abOpcode[pIemCpu->cbOpcode], GCPhys, cbToTryRead);
    if (rc != VINF_SUCCESS)
        return rc;
    pIemCpu->cbOpcode += cbToTryRead;

    return VINF_SUCCESS;
}


/**
 * Deals with the problematic cases that iemOpcodeGetNextByte doesn't like.
 *
 * @returns Strict VBox status code.
 * @param   pIemCpu             The IEM state.
 * @param   pb                  Where to return the opcode byte.
 */
static VBOXSTRICTRC iemOpcodeGetNextByteSlow(PIEMCPU pIemCpu, uint8_t *pb)
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
static VBOXSTRICTRC iemOpcodeGetNextS8SxU16Slow(PIEMCPU pIemCpu, uint16_t *pu16)
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
static VBOXSTRICTRC iemOpcodeGetNextU16Slow(PIEMCPU pIemCpu, uint16_t *pu16)
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
static VBOXSTRICTRC iemOpcodeGetNextU32Slow(PIEMCPU pIemCpu, uint32_t *pu32)
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
static VBOXSTRICTRC iemOpcodeGetNextS32SxU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
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
static VBOXSTRICTRC iemOpcodeGetNextU64Slow(PIEMCPU pIemCpu, uint64_t *pu64)
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
 * @param   pIemCpu             The IEM state.
 * @param   a_pu8               Where to return the opcode byte.
 */
#define IEM_OPCODE_GET_NEXT_BYTE(a_pIemCpu, a_pu8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU8((a_pIemCpu), (a_pu8)); \
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
 * @param   pIemCpu             The IEM state.
 * @param   pi8                 Where to return the signed byte.
 */
#define IEM_OPCODE_GET_NEXT_S8(a_pIemCpu, a_pi8) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8((a_pIemCpu), (a_pi8)); \
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
 * @param   pIemCpu             The IEM state.
 * @param   pu16                Where to return the word.
 */
#define IEM_OPCODE_GET_NEXT_S8_SX_U16(a_pIemCpu, a_pu16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS8SxU16((a_pIemCpu), (a_pu16)); \
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
 * @param   pIemCpu             The IEM state.
 * @param   a_pu16              Where to return the opcode word.
 */
#define IEM_OPCODE_GET_NEXT_U16(a_pIemCpu, a_pu16) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU16((a_pIemCpu), (a_pu16)); \
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
 * @param   pIemCpu             The IEM state.
 * @param   a_u32               Where to return the opcode dword.
 */
#define IEM_OPCODE_GET_NEXT_U32(a_pIemCpu, a_pu32) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU32((a_pIemCpu), (a_pu32)); \
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
 * @param   pIemCpu             The IEM state.
 * @param   a_pu64              Where to return the opcode quad word.
 */
#define IEM_OPCODE_GET_NEXT_S32_SX_U64(a_pIemCpu, a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextS32SxU64((a_pIemCpu), (a_pu64)); \
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
 * Fetches the next opcode word, returns automatically on failure.
 *
 * @param   pIemCpu             The IEM state.
 * @param   a_pu64              Where to return the opcode qword.
 */
#define IEM_OPCODE_GET_NEXT_U64(a_pIemCpu, a_pu64) \
    do \
    { \
        VBOXSTRICTRC rcStrict2 = iemOpcodeGetNextU64((a_pIemCpu), (a_pu64)); \
        if (rcStrict2 != VINF_SUCCESS) \
            return rcStrict2; \
    } while (0)


/** @name  Raising Exceptions.
 *
 * @{
 */

static VBOXSTRICTRC iemRaiseDivideError(PIEMCPU pIemCpu)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseGeneralProtectionFault(PIEMCPU pIemCpu, uint16_t uErr)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseGeneralProtectionFault0(PIEMCPU pIemCpu)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseNotCanonical(PIEMCPU pIemCpu)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseSelectorBounds(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseSelectorInvalidAccess(PIEMCPU pIemCpu, uint32_t iSegReg, uint32_t fAccess)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseSelectorNotPresentBySegReg(PIEMCPU pIemCpu, uint32_t iSegReg)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaiseSelectorNotPresentBySelector(PIEMCPU pIemCpu, uint16_t uSel)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
}


static VBOXSTRICTRC iemRaisePageFault(PIEMCPU pIemCpu, RTGCPTR GCPtrWhere, uint32_t fAccess, int rc)
{
    AssertFailed(/** @todo implement this */);
    return VERR_NOT_IMPLEMENTED;
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
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
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
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
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

/** Stubs an opcode. */
#define FNIEMOP_STUB(a_Name) \
    FNIEMOP_DEF(a_Name) \
    { \
        IEMOP_MNEMONIC(#a_Name); \
        AssertMsgFailed(("After %d instructions\n", pIemCpu->cInstructions)); \
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
 * Checks if an AMD CPUID feature bit is set.
 *
 * @returns true / false.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   fEdx                The EDX bit to test, or 0 if ECX.
 * @param   fEcx                The ECX bit to test, or 0 if EDX.
 * @remarks Used via IEM_IS_AMD_CPUID_FEATURE_PRESENT_ECX.
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
#if defined(IEM_VERIFICATION_MODE) && !defined(IEM_VERIFICATION_MODE_NO_REM)
    /* Force the alternative path so we can ignore writes. */
    if (fAccess & IEM_ACCESS_TYPE_WRITE)
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

    AssertFailed(); /** @todo implement me. */
    return 1024;

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
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM) /* No memory changes in verification mode. */
    if (!pIemCpu->aMemBbMappings[iMemMap].fUnassigned)
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
#endif
        rc = VINF_SUCCESS;

#if defined(IEM_VERIFICATION_MODE) && !defined(IEM_VERIFICATION_MODE_NO_REM)
    /*
     * Record the write(s).
     */
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

#if defined(IEM_VERIFICATION_MODE) && !defined(IEM_VERIFICATION_MODE_NO_REM)
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

#if defined(IEM_VERIFICATION_MODE) && !defined(IEM_VERIFICATION_MODE_NO_REM)
        /*
         * Record the read.
         */
        PIEMVERIFYEVTREC pEvtRec = iemVerifyAllocRecord(pIemCpu);
        if (pEvtRec)
        {
            pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_READ;
            pEvtRec->u.RamRead.GCPhys  = GCPhysFirst;
            pEvtRec->u.RamRead.cb      = cbMem;
            pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
            *pIemCpu->ppIemEvtRecNext = pEvtRec;
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
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
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
     * Get the selector table base and check bounds.
     */
    RTGCPTR GCPtr = uSel & X86_SEL_LDT
                  ? pCtx->ldtrHid.u64Base
                  : pCtx->gdtr.pGdt;
    GCPtr += uSel & X86_SEL_MASK;
    GCPtr += 2 + 2;
    uint32_t volatile *pu32; /** @todo Does the CPU do a 32-bit or 8-bit access here? */
    VBOXSTRICTRC rcStrict = iemMemMap(pIemCpu, (void **)&pu32, 4, UINT8_MAX, GCPtr, IEM_ACCESS_DATA_RW);
    if (rcStrict == VINF_SUCCESS)
    {
        ASMAtomicBitSet(pu32, 0); /* X86_SEL_TYPE_ACCESSED is 1 */

        rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pu32, IEM_ACCESS_DATA_RW);
    }

    return rcStrict;
}

/** @} */


/** @name Misc Helpers
 * @{
 */

/**
 * Checks if we are allowed to access the given I/O port, raising the
 * appropriate exceptions if we aren't (or if the I/O bitmap is not
 * accessible).
 *
 * @returns Strict VBox status code.
 *
 * @param   pIemCpu             The IEM per CPU data.
 * @param   pCtx                The register context.
 * @param   u16Port             The port number.
 * @param   cbOperand           The operand size.
 */
DECLINLINE(VBOXSTRICTRC) iemHlpCheckPortIOPermission(PIEMCPU pIemCpu, PCCPUMCTX pCtx, uint16_t u16Port, uint8_t cbOperand)
{
    if (   (pCtx->cr0 & X86_CR0_PE)
        && (    pIemCpu->uCpl > pCtx->eflags.Bits.u2IOPL
            ||  pCtx->eflags.Bits.u1VM) )
    {
        /** @todo I/O port permission bitmap check */
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }
    return VINF_SUCCESS;
}

/** @} */


/** @name C Implementations
 * @{
 */

/**
 * Implements a 16-bit popa.
 */
IEM_CIMPL_DEF_0(iemCImpl_popa_16)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrStart  = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrLast   = GCPtrStart + 15;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "popa" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do popa boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   IEM_IS_REAL_OR_V86_MODE(pIemCpu)
                    && (pCtx->csHid.u32Limit < GCPtrLast)) ) /* ASSUMES 64-bit RTGCPTR */
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->di, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->si, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->bp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            iemRegAddToRspEx(&TmpRsp, 2, pCtx); /* sp */
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->bx, &TmpRsp);
        }
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->dx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->cx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &pCtx->ax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        uint16_t const *pa16Mem = NULL;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa16Mem, 16, X86_SREG_SS, GCPtrStart, IEM_ACCESS_STACK_R);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->di = pa16Mem[7 - X86_GREG_xDI];
            pCtx->si = pa16Mem[7 - X86_GREG_xSI];
            pCtx->bp = pa16Mem[7 - X86_GREG_xBP];
            /* skip sp */
            pCtx->bx = pa16Mem[7 - X86_GREG_xBX];
            pCtx->dx = pa16Mem[7 - X86_GREG_xDX];
            pCtx->cx = pa16Mem[7 - X86_GREG_xCX];
            pCtx->ax = pa16Mem[7 - X86_GREG_xAX];
            rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pa16Mem, IEM_ACCESS_STACK_R);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegAddToRsp(pCtx, 16);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements a 32-bit popa.
 */
IEM_CIMPL_DEF_0(iemCImpl_popa_32)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrStart  = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrLast   = GCPtrStart + 31;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "popa" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do popa boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   IEM_IS_REAL_OR_V86_MODE(pIemCpu)
                    && (pCtx->csHid.u32Limit < GCPtrLast)) ) /* ASSUMES 64-bit RTGCPTR */
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->edi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->esi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->ebp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            iemRegAddToRspEx(&TmpRsp, 2, pCtx); /* sp */
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->ebx, &TmpRsp);
        }
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->edx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->ecx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &pCtx->eax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
#if 1  /** @todo what actually happens with the high bits when we're in 16-bit mode? */
            pCtx->rdi &= UINT32_MAX;
            pCtx->rsi &= UINT32_MAX;
            pCtx->rbp &= UINT32_MAX;
            pCtx->rbx &= UINT32_MAX;
            pCtx->rdx &= UINT32_MAX;
            pCtx->rcx &= UINT32_MAX;
            pCtx->rax &= UINT32_MAX;
#endif
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        uint32_t const *pa32Mem;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa32Mem, 32, X86_SREG_SS, GCPtrStart, IEM_ACCESS_STACK_R);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rdi = pa32Mem[7 - X86_GREG_xDI];
            pCtx->rsi = pa32Mem[7 - X86_GREG_xSI];
            pCtx->rbp = pa32Mem[7 - X86_GREG_xBP];
            /* skip esp */
            pCtx->rbx = pa32Mem[7 - X86_GREG_xBX];
            pCtx->rdx = pa32Mem[7 - X86_GREG_xDX];
            pCtx->rcx = pa32Mem[7 - X86_GREG_xCX];
            pCtx->rax = pa32Mem[7 - X86_GREG_xAX];
            rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pa32Mem, IEM_ACCESS_STACK_R);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegAddToRsp(pCtx, 32);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements a 16-bit pusha.
 */
IEM_CIMPL_DEF_0(iemCImpl_pusha_16)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrTop    = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrBottom = GCPtrTop - 15;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "pushd" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do pusha boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   GCPtrBottom > GCPtrTop
                    && IEM_IS_REAL_OR_V86_MODE(pIemCpu) ) )
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->ax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->cx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->dx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->bx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->sp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->bp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->si, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU16Ex(pIemCpu, pCtx->di, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        GCPtrBottom--;
        uint16_t *pa16Mem = NULL;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa16Mem, 16, X86_SREG_SS, GCPtrBottom, IEM_ACCESS_STACK_W);
        if (rcStrict == VINF_SUCCESS)
        {
            pa16Mem[7 - X86_GREG_xDI] = pCtx->di;
            pa16Mem[7 - X86_GREG_xSI] = pCtx->si;
            pa16Mem[7 - X86_GREG_xBP] = pCtx->bp;
            pa16Mem[7 - X86_GREG_xSP] = pCtx->sp;
            pa16Mem[7 - X86_GREG_xBX] = pCtx->bx;
            pa16Mem[7 - X86_GREG_xDX] = pCtx->dx;
            pa16Mem[7 - X86_GREG_xCX] = pCtx->cx;
            pa16Mem[7 - X86_GREG_xAX] = pCtx->ax;
            rcStrict = iemMemCommitAndUnmap(pIemCpu, (void *)pa16Mem, IEM_ACCESS_STACK_W);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegSubFromRsp(pCtx, 16);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements a 32-bit pusha.
 */
IEM_CIMPL_DEF_0(iemCImpl_pusha_32)
{
    PCPUMCTX        pCtx        = pIemCpu->CTX_SUFF(pCtx);
    RTGCPTR         GCPtrTop    = iemRegGetEffRsp(pCtx);
    RTGCPTR         GCPtrBottom = GCPtrTop - 31;
    VBOXSTRICTRC    rcStrict;

    /*
     * The docs are a bit hard to comprehend here, but it looks like we wrap
     * around in real mode as long as none of the individual "pusha" crosses the
     * end of the stack segment.  In protected mode we check the whole access
     * in one go.  For efficiency, only do the word-by-word thing if we're in
     * danger of wrapping around.
     */
    /** @todo do pusha boundary / wrap-around checks.  */
    if (RT_UNLIKELY(   GCPtrBottom > GCPtrTop
                    && IEM_IS_REAL_OR_V86_MODE(pIemCpu) ) )
    {
        /* word-by-word */
        RTUINT64U TmpRsp;
        TmpRsp.u = pCtx->rsp;
        rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->eax, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->ecx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->edx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->ebx, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->esp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->ebp, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->esi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
            rcStrict = iemMemStackPushU32Ex(pIemCpu, pCtx->edi, &TmpRsp);
        if (rcStrict == VINF_SUCCESS)
        {
            pCtx->rsp = TmpRsp.u;
            iemRegAddToRip(pIemCpu, cbInstr);
        }
    }
    else
    {
        GCPtrBottom--;
        uint32_t *pa32Mem;
        rcStrict = iemMemMap(pIemCpu, (void **)&pa32Mem, 32, X86_SREG_SS, GCPtrBottom, IEM_ACCESS_STACK_W);
        if (rcStrict == VINF_SUCCESS)
        {
            pa32Mem[7 - X86_GREG_xDI] = pCtx->edi;
            pa32Mem[7 - X86_GREG_xSI] = pCtx->esi;
            pa32Mem[7 - X86_GREG_xBP] = pCtx->ebp;
            pa32Mem[7 - X86_GREG_xSP] = pCtx->esp;
            pa32Mem[7 - X86_GREG_xBX] = pCtx->ebx;
            pa32Mem[7 - X86_GREG_xDX] = pCtx->edx;
            pa32Mem[7 - X86_GREG_xCX] = pCtx->ecx;
            pa32Mem[7 - X86_GREG_xAX] = pCtx->eax;
            rcStrict = iemMemCommitAndUnmap(pIemCpu, pa32Mem, IEM_ACCESS_STACK_W);
            if (rcStrict == VINF_SUCCESS)
            {
                iemRegSubFromRsp(pCtx, 32);
                iemRegAddToRip(pIemCpu, cbInstr);
            }
        }
    }
    return rcStrict;
}


/**
 * Implements pushf.
 *
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_pushf, IEMMODE, enmEffOpSize)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * If we're in V8086 mode some care is required (which is why we're in
     * doing this in a C implementation).
     */
    uint32_t fEfl = pCtx->eflags.u;
    if (   (fEfl & X86_EFL_VM)
        && X86_EFL_GET_IOPL(fEfl) != 3 )
    {
        Assert(pCtx->cr0 & X86_CR0_PE);
        if (   enmEffOpSize != IEMMODE_16BIT
            || !(pCtx->cr4 & X86_CR4_VME))
            return iemRaiseGeneralProtectionFault0(pIemCpu);
        fEfl &= ~X86_EFL_IF;          /* (RF and VM are out of range) */
        fEfl |= (fEfl & X86_EFL_VIF) >> (19 - 9);
        return iemMemStackPushU16(pIemCpu, (uint16_t)fEfl);
    }

    /*
     * Ok, clear RF and VM and push the flags.
     */
    fEfl &= ~(X86_EFL_RF | X86_EFL_VM);

    VBOXSTRICTRC rcStrict;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
            rcStrict = iemMemStackPushU16(pIemCpu, (uint16_t)fEfl);
            break;
        case IEMMODE_32BIT:
            rcStrict = iemMemStackPushU32(pIemCpu, fEfl);
            break;
        case IEMMODE_64BIT:
            rcStrict = iemMemStackPushU64(pIemCpu, fEfl);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements popf.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_popf, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx    = pIemCpu->CTX_SUFF(pCtx);
    uint32_t const  fEflOld = pCtx->eflags.u;
    VBOXSTRICTRC    rcStrict;
    uint32_t        fEflNew;

    /*
     * V8086 is special as usual.
     */
    if (fEflOld & X86_EFL_VM)
    {
        /*
         * Almost anything goes if IOPL is 3.
         */
        if (X86_EFL_GET_IOPL(fEflOld) == 3)
        {
            switch (enmEffOpSize)
            {
                case IEMMODE_16BIT:
                {
                    uint16_t u16Value;
                    rcStrict = iemMemStackPopU16(pIemCpu, &u16Value);
                    if (rcStrict != VINF_SUCCESS)
                        return rcStrict;
                    fEflNew = u16Value | (fEflOld & UINT32_C(0xffff0000));
                    break;
                }
                case IEMMODE_32BIT:
                    rcStrict = iemMemStackPopU32(pIemCpu, &fEflNew);
                    if (rcStrict != VINF_SUCCESS)
                        return rcStrict;
                    break;
                IEM_NOT_REACHED_DEFAULT_CASE_RET();
            }

            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL)) & fEflOld;
        }
        /*
         * Interrupt flag virtualization with CR4.VME=1.
         */
        else if (   enmEffOpSize == IEMMODE_16BIT
                 && (pCtx->cr4 & X86_CR4_VME) )
        {
            uint16_t    u16Value;
            RTUINT64U   TmpRsp;
            TmpRsp.u = pCtx->rsp;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &u16Value, &TmpRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;

            /** @todo Is the popf VME #GP(0) delivered after updating RSP+RIP
             *        or before? */
            if (    (   (u16Value & X86_EFL_IF)
                     && (fEflOld  & X86_EFL_VIP))
                ||  (u16Value & X86_EFL_TF) )
                return iemRaiseGeneralProtectionFault0(pIemCpu);

            fEflNew = u16Value | (fEflOld & UINT32_C(0xffff0000) & ~X86_EFL_VIF);
            fEflNew |= (fEflNew & X86_EFL_IF) << (19 - 9);
            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF)) & fEflOld;

            pCtx->rsp = TmpRsp.u;
        }
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);

    }
    /*
     * Not in V8086 mode.
     */
    else
    {
        /* Pop the flags. */
        switch (enmEffOpSize)
        {
            case IEMMODE_16BIT:
            {
                uint16_t u16Value;
                rcStrict = iemMemStackPopU16(pIemCpu, &u16Value);
                if (rcStrict != VINF_SUCCESS)
                    return rcStrict;
                fEflNew = u16Value | (fEflOld & UINT32_C(0xffff0000));
                break;
            }
            case IEMMODE_32BIT:
            case IEMMODE_64BIT:
                rcStrict = iemMemStackPopU32(pIemCpu, &fEflNew);
                if (rcStrict != VINF_SUCCESS)
                    return rcStrict;
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }

        /* Merge them with the current flags. */
        if (   (fEflNew & (X86_EFL_IOPL | X86_EFL_IF)) == (fEflOld & (X86_EFL_IOPL | X86_EFL_IF))
            || pIemCpu->uCpl == 0)
        {
            fEflNew &=  X86_EFL_POPF_BITS;
            fEflNew |= ~X86_EFL_POPF_BITS & fEflOld;
        }
        else if (pIemCpu->uCpl <= X86_EFL_GET_IOPL(fEflOld))
        {
            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL)) & fEflOld;
        }
        else
        {
            fEflNew &=   X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF);
            fEflNew |= ~(X86_EFL_POPF_BITS & ~(X86_EFL_IOPL | X86_EFL_IF)) & fEflOld;
        }
    }

    /*
     * Commit the flags.
     */
    Assert(fEflNew & RT_BIT_32(1));
    pCtx->eflags.u = fEflNew;
    iemRegAddToRip(pIemCpu, cbInstr);

    return VINF_SUCCESS;
}


/**
 * Implements an indirect call.
 *
 * @param   uNewPC          The new program counter (RIP) value (loaded from the
 *                          operand).
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_16, uint16_t, uNewPC)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint16_t uOldPC = pCtx->ip + cbInstr;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;

}


/**
 * Implements a 16-bit relative call.
 *
 * @param   offDisp      The displacment offset.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_rel_16, int16_t, offDisp)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint16_t uOldPC = pCtx->ip + cbInstr;
    uint16_t uNewPC = uOldPC + offDisp;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU16(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;
}


/**
 * Implements a 32-bit indirect call.
 *
 * @param   uNewPC          The new program counter (RIP) value (loaded from the
 *                          operand).
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_32, uint32_t, uNewPC)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint32_t uOldPC = pCtx->eip + cbInstr;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;

}


/**
 * Implements a 32-bit relative call.
 *
 * @param   offDisp      The displacment offset.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_rel_32, int32_t, offDisp)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint32_t uOldPC = pCtx->eip + cbInstr;
    uint32_t uNewPC = uOldPC + offDisp;
    if (uNewPC > pCtx->csHid.u32Limit)
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU32(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;
}


/**
 * Implements a 64-bit indirect call.
 *
 * @param   uNewPC          The new program counter (RIP) value (loaded from the
 *                          operand).
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_64, uint64_t, uNewPC)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint64_t uOldPC = pCtx->rip + cbInstr;
    if (!IEM_IS_CANONICAL(uNewPC))
        return iemRaiseGeneralProtectionFault0(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;

}


/**
 * Implements a 64-bit relative call.
 *
 * @param   offDisp      The displacment offset.
 */
IEM_CIMPL_DEF_1(iemCImpl_call_rel_64, int64_t, offDisp)
{
    PCPUMCTX pCtx   = pIemCpu->CTX_SUFF(pCtx);
    uint64_t uOldPC = pCtx->rip + cbInstr;
    uint64_t uNewPC = uOldPC + offDisp;
    if (!IEM_IS_CANONICAL(uNewPC))
        return iemRaiseNotCanonical(pIemCpu);

    VBOXSTRICTRC rcStrict = iemMemStackPushU64(pIemCpu, uOldPC);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    pCtx->rip = uNewPC;
    return VINF_SUCCESS;
}


/**
 * Implements far jumps.
 *
 * @param   uSel        The selector.
 * @param   offSeg      The segment offset.
 */
IEM_CIMPL_DEF_2(iemCImpl_FarJmp, uint16_t, uSel, uint32_t, offSeg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Real mode and V8086 mode are easy.  The only snag seems to be that
     * CS.limit doesn't change and the limit check is done against the current
     * limit.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        if (offSeg > pCtx->csHid.u32Limit)
            return iemRaiseGeneralProtectionFault0(pIemCpu);

        if (pIemCpu->enmEffOpSize == IEMMODE_16BIT) /** @todo WRONG, must pass this. */
            pCtx->rip       = offSeg;
        else
            pCtx->rip       = offSeg & UINT16_MAX;
        pCtx->cs            = uSel;
        pCtx->csHid.u64Base = (uint32_t)uSel << 4;
        /** @todo REM reset the accessed bit (see on jmp far16 after disabling
         *        PE.  Check with VT-x and AMD-V. */
#ifdef IEM_VERIFICATION_MODE
        pCtx->csHid.Attr.u  &= ~X86_SEL_TYPE_ACCESSED;
#endif
        return VINF_SUCCESS;
    }

    /*
     * Protected mode. Need to parse the specified descriptor...
     */
    if (!(uSel & (X86_SEL_MASK | X86_SEL_LDT)))
    {
        Log(("jmpf %04x:%08x -> invalid selector, #GP(0)\n", uSel, offSeg));
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    }

    /* Fetch the descriptor. */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uSel);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Is it there? */
    if (!Desc.Legacy.Gen.u1Present)
    {
        Log(("jmpf %04x:%08x -> segment not present\n", uSel, offSeg));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, uSel);
    }

    /*
     * Deal with it according to its type.
     */
    if (Desc.Legacy.Gen.u1DescType)
    {
        /* Only code segments. */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE))
        {
            Log(("jmpf %04x:%08x -> not a code selector (u4Type=%#x).\n", uSel, offSeg, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }

        /* L vs D. */
        if (   Desc.Legacy.Gen.u1Long
            && Desc.Legacy.Gen.u1DefBig
            && IEM_IS_LONG_MODE(pIemCpu))
        {
            Log(("jmpf %04x:%08x -> both L and D are set.\n", uSel, offSeg));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }

        /* DPL/RPL/CPL check, where conforming segments makes a difference. */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CONF))
        {
            if (Desc.Legacy.Gen.u2Dpl > pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> DPL violation (conforming); DPL=%d CPL=%u\n",
                     uSel, offSeg, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
        }
        else
        {
            if (Desc.Legacy.Gen.u2Dpl != pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> CPL != DPL; DPL=%d CPL=%u\n", uSel, offSeg, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
            if ((uSel & X86_SEL_RPL) > pIemCpu->uCpl)
            {
                Log(("jmpf %04x:%08x -> RPL > DPL; RPL=%d CPL=%u\n", uSel, offSeg, (uSel & X86_SEL_RPL), pIemCpu->uCpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
        }

        /* Limit check. (Should alternatively check for non-canonical addresses
           here, but that is ruled out by offSeg being 32-bit, right?) */
        uint64_t u64Base;
        uint32_t cbLimit = X86DESC_LIMIT(Desc.Legacy);
        if (Desc.Legacy.Gen.u1Granularity)
            cbLimit = (cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;
        if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
            u64Base = 0;
        else
        {
            if (offSeg > cbLimit)
            {
                Log(("jmpf %04x:%08x -> out of bounds (%#x)\n", uSel, offSeg, cbLimit));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
            u64Base = X86DESC_BASE(Desc.Legacy);
        }

        /*
         * Ok, everything checked out fine.  Now set the accessed bit before
         * committing the result into CS, CSHID and RIP.
         */
        if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
        {
            rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uSel);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
        }

        /* commit */
        pCtx->rip = offSeg;
        pCtx->cs  = uSel & (X86_SEL_MASK | X86_SEL_LDT);
        pCtx->cs |= pIemCpu->uCpl; /** @todo is this right for conforming segs? or in general? */
        pCtx->csHid.Attr.u   = (Desc.Legacy.u >> (16+16+8)) & UINT32_C(0xf0ff);
        pCtx->csHid.u32Limit = cbLimit;
        pCtx->csHid.u64Base  = u64Base;
        /** @todo check if the hidden bits are loaded correctly for 64-bit
         *        mode.  */
        return VINF_SUCCESS;
    }

    /*
     * System selector.
     */
    if (IEM_IS_LONG_MODE(pIemCpu))
        switch (Desc.Legacy.Gen.u4Type)
        {
            case AMD64_SEL_TYPE_SYS_LDT:
            case AMD64_SEL_TYPE_SYS_TSS_AVAIL:
            case AMD64_SEL_TYPE_SYS_TSS_BUSY:
            case AMD64_SEL_TYPE_SYS_CALL_GATE:
            case AMD64_SEL_TYPE_SYS_INT_GATE:
            case AMD64_SEL_TYPE_SYS_TRAP_GATE:
                /* Call various functions to do the work. */
                AssertFailedReturn(VERR_NOT_IMPLEMENTED);
            default:
                Log(("jmpf %04x:%08x -> wrong sys selector (64-bit): %d\n", uSel, offSeg, Desc.Legacy.Gen.u4Type));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));

        }
    switch (Desc.Legacy.Gen.u4Type)
    {
        case X86_SEL_TYPE_SYS_286_TSS_AVAIL:
        case X86_SEL_TYPE_SYS_LDT:
        case X86_SEL_TYPE_SYS_286_CALL_GATE:
        case X86_SEL_TYPE_SYS_TASK_GATE:
        case X86_SEL_TYPE_SYS_286_INT_GATE:
        case X86_SEL_TYPE_SYS_286_TRAP_GATE:
        case X86_SEL_TYPE_SYS_386_TSS_AVAIL:
        case X86_SEL_TYPE_SYS_386_CALL_GATE:
        case X86_SEL_TYPE_SYS_386_INT_GATE:
        case X86_SEL_TYPE_SYS_386_TRAP_GATE:
            /* Call various functions to do the work. */
            AssertFailedReturn(VERR_NOT_IMPLEMENTED);

        case X86_SEL_TYPE_SYS_286_TSS_BUSY:
        case X86_SEL_TYPE_SYS_386_TSS_BUSY:
            /* Call various functions to do the work. */
            AssertFailedReturn(VERR_NOT_IMPLEMENTED);

        default:
            Log(("jmpf %04x:%08x -> wrong sys selector (32-bit): %d\n", uSel, offSeg, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
    }
}


/**
 * Implements far calls.
 *
 * @param   uSel        The selector.
 * @param   offSeg      The segment offset.
 * @param   enmOpSize   The operand size (in case we need it).
 */
IEM_CIMPL_DEF_3(iemCImpl_callf, uint16_t, uSel, uint64_t, offSeg, IEMMODE, enmOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;
    uint64_t        uNewRsp;
    void           *pvRet;

    /*
     * Real mode and V8086 mode are easy.  The only snag seems to be that
     * CS.limit doesn't change and the limit check is done against the current
     * limit.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        Assert(enmOpSize == IEMMODE_16BIT || enmOpSize == IEMMODE_32BIT);

        /* Check stack first - may #SS(0). */
        rcStrict = iemMemStackPushBeginSpecial(pIemCpu, enmOpSize == IEMMODE_32BIT ? 6 : 4,
                                               &pvRet, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Check the target address range. */
        if (offSeg > UINT32_MAX)
            return iemRaiseGeneralProtectionFault0(pIemCpu);

        /* Everything is fine, push the return address. */
        if (enmOpSize == IEMMODE_16BIT)
        {
            ((uint16_t *)pvRet)[0] = pCtx->ip + cbInstr;
            ((uint16_t *)pvRet)[1] = pCtx->cs;
        }
        else
        {
            ((uint32_t *)pvRet)[0] = pCtx->eip + cbInstr;
            ((uint16_t *)pvRet)[3] = pCtx->cs;
        }
        rcStrict = iemMemStackPushCommitSpecial(pIemCpu, pvRet, uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;

        /* Branch. */
        pCtx->rip           = offSeg;
        pCtx->cs            = uSel;
        pCtx->csHid.u64Base = (uint32_t)uSel << 4;
        /** @todo Does REM reset the accessed bit here to? (See on jmp far16
         *        after disabling PE.) Check with VT-x and AMD-V. */
#ifdef IEM_VERIFICATION_MODE
        pCtx->csHid.Attr.u  &= ~X86_SEL_TYPE_ACCESSED;
#endif
        return VINF_SUCCESS;
    }

    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/**
 * Implements retf.
 *
 * @param   enmEffOpSize    The effective operand size.
 * @param   cbPop           The amount of arguments to pop from the stack
 *                          (bytes).
 */
IEM_CIMPL_DEF_2(iemCImpl_retf, IEMMODE, enmEffOpSize, uint16_t, cbPop)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;
    uint64_t        uNewRsp;

    /*
     * Real mode and V8086 mode are easy.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);
        uint16_t const *pu16Frame;
        rcStrict = iemMemStackPopBeginSpecial(pIemCpu, enmEffOpSize == IEMMODE_32BIT ? 8 : 4,
                                              (void const **)&pu16Frame, &uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        uint32_t uNewEip;
        uint16_t uNewCs;
        if (enmEffOpSize == IEMMODE_32BIT)
        {
            uNewCs  = pu16Frame[2];
            uNewEip = RT_MAKE_U32(pu16Frame[0], pu16Frame[1]);
        }
        else
        {
            uNewCs  = pu16Frame[1];
            uNewEip = pu16Frame[0];
        }
        /** @todo check how this is supposed to work if sp=0xfffe. */

        /* Check the limit of the new EIP. */
        /** @todo Intel pseudo code only does the limit check for 16-bit
         *        operands, AMD does not make any distinction. What is right? */
        if (uNewEip > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);

        /* commit the operation. */
        rcStrict = iemMemStackPopCommitSpecial(pIemCpu, pu16Frame, uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        pCtx->rip           = uNewEip;
        pCtx->cs            = uNewCs;
        pCtx->csHid.u64Base = (uint32_t)uNewCs << 4;
        /** @todo do we load attribs and limit as well? */
        if (cbPop)
            iemRegAddToRsp(pCtx, cbPop);
        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Implements retn.
 *
 * We're doing this in C because of the \#GP that might be raised if the popped
 * program counter is out of bounds.
 *
 * @param   enmEffOpSize    The effective operand size.
 * @param   cbPop           The amount of arguments to pop from the stack
 *                          (bytes).
 */
IEM_CIMPL_DEF_2(iemCImpl_retn, IEMMODE, enmEffOpSize, uint16_t, cbPop)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);

    /* Fetch the RSP from the stack. */
    VBOXSTRICTRC    rcStrict;
    RTUINT64U       NewRip;
    RTUINT64U       NewRsp;
    NewRsp.u = pCtx->rsp;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
            NewRip.u = 0;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &NewRip.Words.w0, &NewRsp);
            break;
        case IEMMODE_32BIT:
            NewRip.u = 0;
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &NewRip.DWords.dw0, &NewRsp);
            break;
        case IEMMODE_64BIT:
            rcStrict = iemMemStackPopU64Ex(pIemCpu, &NewRip.u, &NewRsp);
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check the new RSP before loading it. */
    /** @todo Should test this as the intel+amd pseudo code doesn't mention half
     *        of it.  The canonical test is performed here and for call. */
    if (enmEffOpSize != IEMMODE_64BIT)
    {
        if (NewRip.DWords.dw0 > pCtx->csHid.u32Limit)
        {
            Log(("retn newrip=%llx - out of bounds (%x) -> #GP\n", NewRip.u, pCtx->csHid.u32Limit));
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);
        }
    }
    else
    {
        if (!IEM_IS_CANONICAL(NewRip.u))
        {
            Log(("retn newrip=%llx - not canonical -> #GP\n", NewRip.u));
            return iemRaiseNotCanonical(pIemCpu);
        }
    }

    /* Commit it. */
    pCtx->rip = NewRip.u;
    pCtx->rsp = NewRsp.u;
    if (cbPop)
        iemRegAddToRsp(pCtx, cbPop);

    return VINF_SUCCESS;
}


/**
 * Implements int3 and int XX.
 *
 * @param   u8Int       The interrupt vector number.
 * @param   fIsBpInstr  Is it the breakpoint instruction.
 */
IEM_CIMPL_DEF_2(iemCImpl_int, uint8_t, u8Int, bool, fIsBpInstr)
{
    /** @todo we should call TRPM to do this job.  */
    VBOXSTRICTRC    rcStrict;
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * Real mode is easy.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_MODE(pIemCpu))
    {
        /* read the IDT entry. */
        if (pCtx->idtr.cbIdt < UINT32_C(4) * u8Int + 3)
            return iemRaiseGeneralProtectionFault(pIemCpu, X86_TRAP_ERR_IDT | ((uint16_t)u8Int << X86_TRAP_ERR_SEL_SHIFT));
        RTFAR16 Idte;
        rcStrict = iemMemFetchDataU32(pIemCpu, (uint32_t *)&Idte, UINT8_MAX, pCtx->idtr.pIdt + UINT32_C(4) * u8Int);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
            return rcStrict;

        /* push the stack frame. */
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

        /* load the vector address into cs:ip. */
        pCtx->cs               = Idte.sel;
        pCtx->csHid.u64Base    = (uint32_t)Idte.sel << 4;
        /** @todo do we load attribs and limit as well? Should we check against limit like far jump? */
        pCtx->rip              = Idte.off;
        pCtx->eflags.Bits.u1IF = 0;
        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Implements iret.
 *
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_1(iemCImpl_iret, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;
    uint64_t        uNewRsp;

    /*
     * Real mode is easy, V8086 mode is relative similar.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        /* iret throws an exception if VME isn't enabled.  */
        if (   pCtx->eflags.Bits.u1VM
            && !(pCtx->cr4 & X86_CR4_VME))
            return iemRaiseGeneralProtectionFault0(pIemCpu);

        /* Do the stack bits, but don't commit RSP before everything checks
           out right. */
        union
        {
            uint32_t const *pu32;
            uint16_t const *pu16;
            void const     *pv;
        } uFrame;
        Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);
        uint16_t uNewCs;
        uint32_t uNewEip;
        uint32_t uNewFlags;
        if (enmEffOpSize == IEMMODE_32BIT)
        {
            rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 12, &uFrame.pv, &uNewRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            uNewEip    = uFrame.pu32[0];
            uNewCs     = (uint16_t)uFrame.pu32[1];
            uNewFlags  = uFrame.pu32[2];
            uNewFlags &= X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF
                       | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT
                       | X86_EFL_RF /*| X86_EFL_VM*/ | X86_EFL_AC /*|X86_EFL_VIF*/ /*|X86_EFL_VIP*/
                       | X86_EFL_ID;
            uNewFlags |= pCtx->eflags.u & (X86_EFL_VM | X86_EFL_VIF | X86_EFL_VIP | X86_EFL_1);
        }
        else
        {
            rcStrict = iemMemStackPopBeginSpecial(pIemCpu, 6, &uFrame.pv, &uNewRsp);
            if (rcStrict != VINF_SUCCESS)
                return rcStrict;
            uNewEip    = uFrame.pu16[0];
            uNewCs     = uFrame.pu16[1];
            uNewFlags  = uFrame.pu16[2];
            uNewFlags &= X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF
                       | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT;
            uNewFlags |= pCtx->eflags.u & (UINT16_C(0xffff0000) | X86_EFL_1);
            /** @todo The intel pseudo code does not indicate what happens to
             *        reserved flags. We just ignore them. */
        }
        /** @todo Check how this is supposed to work if sp=0xfffe. */

        /* Check the limit of the new EIP. */
        /** @todo Only the AMD pseudo code check the limit here, what's
         *        right? */
        if (uNewEip > pCtx->csHid.u32Limit)
            return iemRaiseSelectorBounds(pIemCpu, X86_SREG_CS, IEM_ACCESS_INSTRUCTION);

        /* V8086 checks and flag adjustments */
        if (pCtx->eflags.Bits.u1VM)
        {
            if (pCtx->eflags.Bits.u2IOPL == 3)
            {
                /* Preserve IOPL and clear RF. */
                uNewFlags &=                 ~(X86_EFL_IOPL | X86_EFL_RF);
                uNewFlags |= pCtx->eflags.u & (X86_EFL_IOPL);
            }
            else if (   enmEffOpSize == IEMMODE_16BIT
                     && (   !(uNewFlags & X86_EFL_IF)
                         || !pCtx->eflags.Bits.u1VIP )
                     && !(uNewFlags & X86_EFL_TF)   )
            {
                /* Move IF to VIF, clear RF and preserve IF and IOPL.*/
                uNewFlags &= ~X86_EFL_VIF;
                uNewFlags |= (uNewFlags & X86_EFL_IF) << (19 - 9);
                uNewFlags &=                 ~(X86_EFL_IF | X86_EFL_IOPL | X86_EFL_RF);
                uNewFlags |= pCtx->eflags.u & (X86_EFL_IF | X86_EFL_IOPL);
            }
            else
                return iemRaiseGeneralProtectionFault0(pIemCpu);
        }

        /* commit the operation. */
        rcStrict = iemMemStackPopCommitSpecial(pIemCpu, uFrame.pv, uNewRsp);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        pCtx->rip           = uNewEip;
        pCtx->cs            = uNewCs;
        pCtx->csHid.u64Base = (uint32_t)uNewCs << 4;
        /** @todo do we load attribs and limit as well? */
        Assert(uNewFlags & X86_EFL_1);
        pCtx->eflags.u      = uNewFlags;

        return VINF_SUCCESS;
    }


    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Implements 'mov SReg, r/m'.
 *
 * @param   iSegReg     The segment register number (valid).
 * @param   uSel        The new selector value.
 */
IEM_CIMPL_DEF_2(iemCImpl_LoadSReg, uint8_t, iSegReg, uint16_t, uSel)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    uint16_t       *pSel = iemSRegRef(pIemCpu, iSegReg);
    PCPUMSELREGHID  pHid = iemSRegGetHid(pIemCpu, iSegReg);

    Assert(iSegReg <= X86_SREG_GS && iSegReg != X86_SREG_CS);

    /*
     * Real mode and V8086 mode are easy.
     */
    if (   pIemCpu->enmCpuMode == IEMMODE_16BIT
        && IEM_IS_REAL_OR_V86_MODE(pIemCpu))
    {
        *pSel           = uSel;
        pHid->u64Base   = (uint32_t)uSel << 4;
        /** @todo Does the CPU actually load limits and attributes in the
         *        real/V8086 mode segment load case?  It doesn't for CS in far
         *        jumps...  Affects unreal mode.  */
        pHid->u32Limit          = 0xffff;
        pHid->Attr.u = 0;
        pHid->Attr.n.u1Present  = 1;
        pHid->Attr.n.u1DescType = 1;
        pHid->Attr.n.u4Type     = iSegReg != X86_SREG_CS
                                ? X86_SEL_TYPE_RW
                                : X86_SEL_TYPE_READ | X86_SEL_TYPE_CODE;

        iemRegAddToRip(pIemCpu, cbInstr);
        if (iSegReg == X86_SREG_SS)
            EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
        return VINF_SUCCESS;
    }

    /*
     * Protected mode.
     *
     * Check if it's a null segment selector value first, that's OK for DS, ES,
     * FS and GS.  If not null, then we have to load and parse the descriptor.
     */
    if (!(uSel & (X86_SEL_MASK | X86_SEL_LDT)))
    {
        if (iSegReg == X86_SREG_SS)
        {
            if (   pIemCpu->enmCpuMode != IEMMODE_64BIT
                || pIemCpu->uCpl != 0
                || uSel != 0) /** @todo We cannot 'mov ss, 3' in 64-bit kernel mode, can we?  */
            {
                Log(("load sreg -> invalid stack selector, #GP(0)\n", uSel));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* In 64-bit kernel mode, the stack can be 0 because of the way
               interrupts are dispatched when in kernel ctx. Just load the
               selector value into the register and leave the hidden bits
               as is. */
            *pSel = uSel;
            iemRegAddToRip(pIemCpu, cbInstr);
            EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
            return VINF_SUCCESS;
        }

        *pSel = uSel;   /* Not RPL, remember :-) */
        if (   pIemCpu->enmCpuMode == IEMMODE_64BIT
            && iSegReg != X86_SREG_FS
            && iSegReg != X86_SREG_GS)
        {
            /** @todo figure out what this actually does, it works. Needs
             *        testcase! */
            pHid->Attr.u           = 0;
            pHid->Attr.n.u1Present = 1;
            pHid->Attr.n.u1Long    = 1;
            pHid->Attr.n.u4Type    = X86_SEL_TYPE_RW;
            pHid->Attr.n.u2Dpl     = 3;
            pHid->u32Limit         = 0;
            pHid->u64Base          = 0;
        }
        else
        {
            pHid->Attr.u   = 0;
            pHid->u32Limit = 0;
            pHid->u64Base  = 0;
        }
        iemRegAddToRip(pIemCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Fetch the descriptor. */
    IEMSELDESC Desc;
    VBOXSTRICTRC rcStrict = iemMemFetchSelDesc(pIemCpu, &Desc, uSel);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /* Check GPs first. */
    if (!Desc.Legacy.Gen.u1DescType)
    {
        Log(("load sreg %d - system selector (%#x) -> #GP\n", iSegReg, uSel, Desc.Legacy.Gen.u4Type));
        return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
    }
    if (iSegReg == X86_SREG_SS) /* SS gets different treatment */
    {
        if (   (Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
        {
            Log(("load sreg SS, %#x - code or read only (%#x) -> #GP\n", uSel, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if (    (Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_CODE)
            || !(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_WRITE) )
        {
            Log(("load sreg SS, %#x - code or read only (%#x) -> #GP\n", uSel, Desc.Legacy.Gen.u4Type));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if ((uSel & X86_SEL_RPL) != pIemCpu->uCpl)
        {
            Log(("load sreg SS, %#x - RPL and CPL (%d) differs -> #GP\n", uSel, pIemCpu->uCpl));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if (Desc.Legacy.Gen.u2Dpl != pIemCpu->uCpl)
        {
            Log(("load sreg SS, %#x - DPL (%d) and CPL (%d) differs -> #GP\n", uSel, Desc.Legacy.Gen.u2Dpl, pIemCpu->uCpl));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
    }
    else
    {
        if ((Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ)) == X86_SEL_TYPE_CODE)
        {
            Log(("load sreg%u, %#x - execute only segment -> #GP\n", iSegReg, uSel));
            return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
        }
        if (   (Desc.Legacy.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
            != (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
        {
#if 0 /* this is what intel says. */
            if (   (uSel & X86_SEL_RPL) > Desc.Legacy.Gen.u2Dpl
                && pIemCpu->uCpl        > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - both RPL (%d) and CPL (%d) are greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, (uSel & X86_SEL_RPL), pIemCpu->uCpl, Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
#else /* this is what makes more sense. */
            if ((unsigned)(uSel & X86_SEL_RPL) > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - RPL (%d) is greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, (uSel & X86_SEL_RPL), Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
            if (pIemCpu->uCpl > Desc.Legacy.Gen.u2Dpl)
            {
                Log(("load sreg%u, %#x - CPL (%d) is greater than DPL (%d) -> #GP\n",
                     iSegReg, uSel, pIemCpu->uCpl, Desc.Legacy.Gen.u2Dpl));
                return iemRaiseGeneralProtectionFault(pIemCpu, uSel & (X86_SEL_MASK | X86_SEL_LDT));
            }
#endif
        }
    }

    /* Is it there? */
    if (!Desc.Legacy.Gen.u1Present)
    {
        Log(("load sreg%d,%#x - segment not present -> #NP\n", iSegReg, uSel));
        return iemRaiseSelectorNotPresentBySelector(pIemCpu, uSel);
    }

    /* The the base and limit. */
    uint64_t u64Base;
    uint32_t cbLimit = X86DESC_LIMIT(Desc.Legacy);
    if (Desc.Legacy.Gen.u1Granularity)
        cbLimit = (cbLimit << PAGE_SHIFT) | PAGE_OFFSET_MASK;

    if (   pIemCpu->enmCpuMode == IEMMODE_64BIT
        && iSegReg < X86_SREG_FS)
        u64Base = 0;
    else
        u64Base = X86DESC_BASE(Desc.Legacy);

    /*
     * Ok, everything checked out fine.  Now set the accessed bit before
     * committing the result into the registers.
     */
    if (!(Desc.Legacy.Gen.u4Type & X86_SEL_TYPE_ACCESSED))
    {
        rcStrict = iemMemMarkSelDescAccessed(pIemCpu, uSel);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
        Desc.Legacy.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;
    }

    /* commit */
    *pSel = uSel;
    pHid->Attr.u   = (Desc.Legacy.u >> (16+16+8)) & UINT32_C(0xf0ff); /** @todo do we have a define for 0xf0ff? */
    pHid->u32Limit = cbLimit;
    pHid->u64Base  = u64Base;

    /** @todo check if the hidden bits are loaded correctly for 64-bit
     *        mode.  */

    iemRegAddToRip(pIemCpu, cbInstr);
    if (iSegReg == X86_SREG_SS)
        EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
    return VINF_SUCCESS;
}


/**
 * Implements lgs, lfs, les, lds & lss.
 */
IEM_CIMPL_DEF_5(iemCImpl_load_SReg_Greg,
                uint16_t, uSel,
                uint64_t, offSeg,
                uint8_t,  iSegReg,
                uint8_t,  iGReg,
                IEMMODE,  enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;

    /*
     * Use iemCImpl_LoadSReg to do the tricky segment register loading.
     */
    /** @todo verify and test that mov, pop and lXs works the segment
     *        register loading in the exact same way. */
    rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, uSel);
    if (rcStrict == VINF_SUCCESS)
    {
        switch (enmEffOpSize)
        {
            case IEMMODE_16BIT:
                *(uint16_t *)iemGRegRef(pIemCpu, iGReg) = offSeg;
                break;
            case IEMMODE_32BIT:
                *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = offSeg;
                break;
            case IEMMODE_64BIT:
                *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = offSeg;
                break;
            IEM_NOT_REACHED_DEFAULT_CASE_RET();
        }
    }

    return rcStrict;
}


/**
 * Implements 'pop SReg'.
 *
 * @param   iSegReg         The segment register number (valid).
 * @param   enmEffOpSize    The efficient operand size (valid).
 */
IEM_CIMPL_DEF_2(iemOpCImpl_pop_Sreg, uint8_t, iSegReg, IEMMODE, enmEffOpSize)
{
    PCPUMCTX        pCtx = pIemCpu->CTX_SUFF(pCtx);
    VBOXSTRICTRC    rcStrict;

    /*
     * Read the selector off the stack and join paths with mov ss, reg.
     */
    RTUINT64U TmpRsp;
    TmpRsp.u = pCtx->rsp;
    switch (enmEffOpSize)
    {
        case IEMMODE_16BIT:
        {
            uint16_t uSel;
            rcStrict = iemMemStackPopU16Ex(pIemCpu, &uSel, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, uSel);
            break;
        }

        case IEMMODE_32BIT:
        {
            uint32_t u32Value;
            rcStrict = iemMemStackPopU32Ex(pIemCpu, &u32Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, (uint16_t)u32Value);
            break;
        }

        case IEMMODE_64BIT:
        {
            uint64_t u64Value;
            rcStrict = iemMemStackPopU64Ex(pIemCpu, &u64Value, &TmpRsp);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = IEM_CIMPL_CALL_2(iemCImpl_LoadSReg, iSegReg, (uint16_t)u64Value);
            break;
        }
        IEM_NOT_REACHED_DEFAULT_CASE_RET();
    }

    /*
     * Commit the stack on success.
     */
    if (rcStrict == VINF_SUCCESS)
        pCtx->rsp = TmpRsp.u;
    return rcStrict;
}


/**
 * Implements lgdt.
 *
 * @param   iEffSeg         The segment of the new ldtr contents
 * @param   GCPtrEffSrc     The address of the new ldtr contents.
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_3(iemCImpl_lgdt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pIemCpu->CTX_SUFF(pCtx)->eflags.Bits.u1VM);

    /*
     * Fetch the limit and base address.
     */
    uint16_t cbLimit;
    RTGCPTR  GCPtrBase;
    VBOXSTRICTRC rcStrict = iemMemFetchDataXdtr(pIemCpu, &cbLimit, &GCPtrBase, iEffSeg, GCPtrEffSrc, enmEffOpSize);
    if (rcStrict == VINF_SUCCESS)
    {
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
        rcStrict = CPUMSetGuestGDTR(IEMCPU_TO_VMCPU(pIemCpu), GCPtrBase, cbLimit);
#else
        PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
        pCtx->gdtr.cbGdt = cbLimit;
        pCtx->gdtr.pGdt  = GCPtrBase;
#endif
        if (rcStrict == VINF_SUCCESS)
            iemRegAddToRip(pIemCpu, cbInstr);
    }
    return rcStrict;
}


/**
 * Implements lidt.
 *
 * @param   iEffSeg         The segment of the new ldtr contents
 * @param   GCPtrEffSrc     The address of the new ldtr contents.
 * @param   enmEffOpSize    The effective operand size.
 */
IEM_CIMPL_DEF_3(iemCImpl_lidt, uint8_t, iEffSeg, RTGCPTR, GCPtrEffSrc, IEMMODE, enmEffOpSize)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pIemCpu->CTX_SUFF(pCtx)->eflags.Bits.u1VM);

    /*
     * Fetch the limit and base address.
     */
    uint16_t cbLimit;
    RTGCPTR  GCPtrBase;
    VBOXSTRICTRC rcStrict = iemMemFetchDataXdtr(pIemCpu, &cbLimit, &GCPtrBase, iEffSeg, GCPtrEffSrc, enmEffOpSize);
    if (rcStrict == VINF_SUCCESS)
    {
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
        rcStrict = CPUMSetGuestIDTR(IEMCPU_TO_VMCPU(pIemCpu), GCPtrBase, cbLimit);
#else
        PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
        pCtx->idtr.cbIdt = cbLimit;
        pCtx->idtr.pIdt  = GCPtrBase;
#endif
        if (rcStrict == VINF_SUCCESS)
            iemRegAddToRip(pIemCpu, cbInstr);
    }
    return rcStrict;
}


/**
 * Implements mov GReg,CRx.
 *
 * @param   iGReg           The general register to store the CRx value in.
 * @param   iCrReg          The CRx register to read (valid).
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Rd_Cd, uint8_t, iGReg, uint8_t, iCrReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    /* read it */
    uint64_t crX;
    switch (iCrReg)
    {
        case 0: crX = pCtx->cr0; break;
        case 2: crX = pCtx->cr2; break;
        case 3: crX = pCtx->cr3; break;
        case 4: crX = pCtx->cr4; break;
        case 8:
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            AssertFailedReturn(VERR_NOT_IMPLEMENTED); /** @todo implement CR8 reading and writing. */
#else
            crX = 0xff;
#endif
            break;
        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* call checks */
    }

    /* store it */
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = crX;
    else
        *(uint64_t *)iemGRegRef(pIemCpu, iGReg) = (uint32_t)crX;

    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements mov CRx,GReg.
 *
 * @param   iCrReg          The CRx register to read (valid).
 * @param   iGReg           The general register to store the CRx value in.
 */
IEM_CIMPL_DEF_2(iemCImpl_mov_Cd_Rd, uint8_t, iCrReg, uint8_t, iGReg)
{
    PCPUMCTX        pCtx  = pIemCpu->CTX_SUFF(pCtx);
    PVMCPU          pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    VBOXSTRICTRC    rcStrict;
    int             rc;

    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    Assert(!pCtx->eflags.Bits.u1VM);

    /*
     * Read the new value from the source register.
     */
    uint64_t NewCrX;
    if (pIemCpu->enmCpuMode == IEMMODE_64BIT)
        NewCrX = iemGRegFetchU64(pIemCpu, iGReg);
    else
        NewCrX = iemGRegFetchU32(pIemCpu, iGReg);

    /*
     * Try store it.
     * Unfortunately, CPUM only does a tiny bit of the work.
     */
    switch (iCrReg)
    {
        case 0:
        {
            /*
             * Perform checks.
             */
            uint64_t const OldCrX = pCtx->cr0;
            NewCrX |= X86_CR0_ET; /* hardcoded */

            /* Check for reserved bits. */
            uint32_t const fValid = X86_CR0_PE | X86_CR0_MP | X86_CR0_EM | X86_CR0_TS
                                  | X86_CR0_ET | X86_CR0_NE | X86_CR0_WP | X86_CR0_AM
                                  | X86_CR0_NW | X86_CR0_CD | X86_CR0_PG;
            if (NewCrX & ~(uint64_t)fValid)
            {
                Log(("Trying to set reserved CR0 bits: NewCR0=%#llx InvalidBits=%#llx\n", NewCrX, NewCrX & ~(uint64_t)fValid));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* Check for invalid combinations. */
            if (    (NewCrX & X86_CR0_PG)
                && !(NewCrX & X86_CR0_PE) )
            {
                Log(("Trying to set CR0.PG without CR0.PE\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            if (   !(NewCrX & X86_CR0_CD)
                && (NewCrX & X86_CR0_NW) )
            {
                Log(("Trying to clear CR0.CD while leaving CR0.NW set\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* Long mode consistency checks. */
            if (    (NewCrX & X86_CR0_PG)
                && !(OldCrX & X86_CR0_PG)
                &&  (pCtx->msrEFER & MSR_K6_EFER_LME) )
            {
                if (!(pCtx->cr4 & X86_CR4_PAE))
                {
                    Log(("Trying to enabled long mode paging without CR4.PAE set\n"));
                    return iemRaiseGeneralProtectionFault0(pIemCpu);
                }
                if (pCtx->csHid.Attr.n.u1Long)
                {
                    Log(("Trying to enabled long mode paging with a long CS descriptor loaded.\n"));
                    return iemRaiseGeneralProtectionFault0(pIemCpu);
                }
            }

            /** @todo check reserved PDPTR bits as AMD states. */

            /*
             * Change CR0.
             */
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            rc = CPUMSetGuestCR0(pVCpu, NewCrX);
            AssertRCSuccessReturn(rc, RT_FAILURE_NP(rc) ? rc : VERR_INTERNAL_ERROR_3);
#else
            pCtx->cr0 = NewCrX;
#endif
            Assert(pCtx->cr0 == NewCrX);

            /*
             * Change EFER.LMA if entering or leaving long mode.
             */
            if (   (NewCrX & X86_CR0_PG) != (OldCrX & X86_CR0_PG)
                && (pCtx->msrEFER & MSR_K6_EFER_LME) )
            {
                uint64_t NewEFER = pCtx->msrEFER;
                if (NewCrX & X86_CR0_PG)
                    NewEFER |= MSR_K6_EFER_LME;
                else
                    NewEFER &= ~MSR_K6_EFER_LME;

#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
                CPUMSetGuestEFER(pVCpu, NewEFER);
#else
                pCtx->msrEFER = NewEFER;
#endif
                Assert(pCtx->msrEFER == NewEFER);
            }

#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            /*
             * Inform PGM.
             */
            if (    (NewCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                !=  (OldCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) )
            {
                rc = PGMFlushTLB(pVCpu, pCtx->cr3, true /* global */);
                AssertRCReturn(rc, rc);
                /* ignore informational status codes */
            }
            rcStrict = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
            /** @todo Status code management.  */
#else
            rcStrict = VINF_SUCCESS;
#endif
            break;
        }

        /*
         * CR2 can be changed without any restrictions.
         */
        case 2:
            pCtx->cr2 = NewCrX;
            rcStrict  = VINF_SUCCESS;
            break;

        /*
         * CR3 is relatively simple, although AMD and Intel have different
         * accounts of how setting reserved bits are handled.  We take intel's
         * word for the lower bits and AMD's for the high bits (63:52).
         */
        /** @todo Testcase: Setting reserved bits in CR3, especially before
         *        enabling paging. */
        case 3:
        {
            /* check / mask the value. */
            if (NewCrX & UINT64_C(0xfff0000000000000))
            {
                Log(("Trying to load CR3 with invalid high bits set: %#llx\n", NewCrX));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            uint64_t fValid;
            if (   (pCtx->cr4 & X86_CR4_PAE)
                && (pCtx->msrEFER & MSR_K6_EFER_LME))
                fValid = UINT64_C(0x000ffffffffff014);
            else if (pCtx->cr4 & X86_CR4_PAE)
                fValid = UINT64_C(0xfffffff4);
            else
                fValid = UINT64_C(0xfffff014);
            if (NewCrX & ~fValid)
            {
                Log(("Automatically clearing reserved bits in CR3 load: NewCR3=%#llx ClearedBits=%#llx\n",
                     NewCrX, NewCrX & ~fValid));
                NewCrX &= fValid;
            }

            /** @todo If we're in PAE mode we should check the PDPTRs for
             *        invalid bits. */

            /* Make the change. */
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            rc = CPUMSetGuestCR3(pVCpu, NewCrX);
            AssertRCSuccessReturn(rc, rc);
#else
            pCtx->cr3 = NewCrX;
#endif

#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            /* Inform PGM. */
            if (pCtx->cr0 & X86_CR0_PG)
            {
                rc = PGMFlushTLB(pVCpu, pCtx->cr3, !(pCtx->cr3 & X86_CR4_PGE));
                AssertRCReturn(rc, rc);
                /* ignore informational status codes */
                /** @todo status code management */
            }
#endif
            rcStrict = VINF_SUCCESS;
            break;
        }

        /*
         * CR4 is a bit more tedious as there are bits which cannot be cleared
         * under some circumstances and such.
         */
        case 4:
        {
            uint64_t const OldCrX = pCtx->cr0;

            /* reserved bits */
            uint32_t fValid = X86_CR4_VME | X86_CR4_PVI
                            | X86_CR4_TSD | X86_CR4_DE
                            | X86_CR4_PSE | X86_CR4_PAE
                            | X86_CR4_MCE | X86_CR4_PGE
                            | X86_CR4_PCE | X86_CR4_OSFSXR
                            | X86_CR4_OSXMMEEXCPT;
            //if (xxx)
            //    fValid |= X86_CR4_VMXE;
            //if (xxx)
            //    fValid |= X86_CR4_OSXSAVE;
            if (NewCrX & ~(uint64_t)fValid)
            {
                Log(("Trying to set reserved CR4 bits: NewCR4=%#llx InvalidBits=%#llx\n", NewCrX, NewCrX & ~(uint64_t)fValid));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }

            /* long mode checks. */
            if (   (OldCrX & X86_CR4_PAE)
                && !(NewCrX & X86_CR4_PAE)
                && (pCtx->msrEFER & MSR_K6_EFER_LMA) )
            {
                Log(("Trying to set clear CR4.PAE while long mode is active\n"));
                return iemRaiseGeneralProtectionFault0(pIemCpu);
            }


            /*
             * Change it.
             */
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            rc = CPUMSetGuestCR4(pVCpu, NewCrX);
            AssertRCSuccessReturn(rc, rc);
#else
            pCtx->cr4 = NewCrX;
#endif
            Assert(pCtx->cr4 == NewCrX);

            /*
             * Notify SELM and PGM.
             */
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            /* SELM - VME may change things wrt to the TSS shadowing. */
            if ((NewCrX ^ OldCrX) & X86_CR4_VME)
                VMCPU_FF_SET(pVCpu, VMCPU_FF_SELM_SYNC_TSS);

            /* PGM - flushing and mode. */
            if (    (NewCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE))
                !=  (OldCrX & (X86_CR0_PG | X86_CR0_WP | X86_CR0_PE)) )
            {
                rc = PGMFlushTLB(pVCpu, pCtx->cr3, true /* global */);
                AssertRCReturn(rc, rc);
                /* ignore informational status codes */
            }
            rcStrict = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
            /** @todo Status code management.  */
#else
            rcStrict = VINF_SUCCESS;
#endif
            break;
        }

        /*
         * CR8 maps to the APIC TPR.
         */
        case 8:
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
            AssertFailedReturn(VERR_NOT_IMPLEMENTED); /** @todo implement CR8 reading and writing. */
#else
            rcStrict = VINF_SUCCESS;
#endif
            break;

        IEM_NOT_REACHED_DEFAULT_CASE_RET(); /* call checks */
    }

    /*
     * Advance the RIP on success.
     */
    /** @todo Status code management.  */
    if (rcStrict == VINF_SUCCESS)
        iemRegAddToRip(pIemCpu, cbInstr);
    return rcStrict;
}


/**
 * Implements 'IN eAX, port'.
 *
 * @param   u16Port     The source port.
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_2(iemCImpl_in, uint16_t, u16Port, uint8_t, cbReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * CPL check
     */
    VBOXSTRICTRC rcStrict = iemHlpCheckPortIOPermission(pIemCpu, pCtx, u16Port, cbReg);
    if (rcStrict != VINF_SUCCESS)
        return rcStrict;

    /*
     * Perform the I/O.
     */
    uint32_t u32Value;
#if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
    rcStrict = IOMIOPortRead(IEMCPU_TO_VM(pIemCpu), u16Port, &u32Value, cbReg);
#else
    rcStrict = iemVerifyFakeIOPortRead(pIemCpu, u16Port, &u32Value, cbReg);
#endif
    if (IOM_SUCCESS(rcStrict))
    {
        switch (cbReg)
        {
            case 1: pCtx->al  = (uint8_t)u32Value;  break;
            case 2: pCtx->ax  = (uint16_t)u32Value; break;
            case 4: pCtx->rax = u32Value;           break;
            default: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
        }
        iemRegAddToRip(pIemCpu, cbInstr);
        pIemCpu->cPotentialExits++;
    }
    /** @todo massage rcStrict. */
    return rcStrict;
}


/**
 * Implements 'IN eAX, DX'.
 *
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_1(iemCImpl_in_eAX_DX, uint8_t, cbReg)
{
    return IEM_CIMPL_CALL_2(iemCImpl_in, pIemCpu->CTX_SUFF(pCtx)->dx, cbReg);
}


/**
 * Implements 'OUT port, eAX'.
 *
 * @param   u16Port     The destination port.
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_2(iemCImpl_out, uint16_t, u16Port, uint8_t, cbReg)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    /*
     * CPL check
     */
    if (   (pCtx->cr0 & X86_CR0_PE)
        && (    pIemCpu->uCpl > pCtx->eflags.Bits.u2IOPL
            ||  pCtx->eflags.Bits.u1VM) )
    {
        /** @todo I/O port permission bitmap check */
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);
    }

    /*
     * Perform the I/O.
     */
    uint32_t u32Value;
    switch (cbReg)
    {
        case 1: u32Value = pCtx->al;  break;
        case 2: u32Value = pCtx->ax;  break;
        case 4: u32Value = pCtx->eax; break;
        default: AssertFailedReturn(VERR_INTERNAL_ERROR_3);
    }
# if !defined(IEM_VERIFICATION_MODE) || defined(IEM_VERIFICATION_MODE_NO_REM)
    VBOXSTRICTRC rc = IOMIOPortWrite(IEMCPU_TO_VM(pIemCpu), u16Port, u32Value, cbReg);
# else
    VBOXSTRICTRC rc = iemVerifyFakeIOPortWrite(pIemCpu, u16Port, u32Value, cbReg);
# endif
    if (IOM_SUCCESS(rc))
    {
        iemRegAddToRip(pIemCpu, cbInstr);
        pIemCpu->cPotentialExits++;
        /** @todo massage rc. */
    }
    return rc;
}


/**
 * Implements 'OUT DX, eAX'.
 *
 * @param   cbReg       The register size.
 */
IEM_CIMPL_DEF_1(iemCImpl_out_DX_eAX, uint8_t, cbReg)
{
    return IEM_CIMPL_CALL_2(iemCImpl_out, pIemCpu->CTX_SUFF(pCtx)->dx, cbReg);
}


/**
 * Implements 'CLI'.
 */
IEM_CIMPL_DEF_0(iemCImpl_cli)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    if (pCtx->cr0 & X86_CR0_PE)
    {
        uint8_t const uIopl = pCtx->eflags.Bits.u2IOPL;
        if (!pCtx->eflags.Bits.u1VM)
        {
            if (pIemCpu->uCpl <= uIopl)
                pCtx->eflags.Bits.u1IF = 0;
            else if (   pIemCpu->uCpl == 3
                     && (pCtx->cr4 & X86_CR4_PVI) )
                pCtx->eflags.Bits.u1VIF = 0;
            else
                return iemRaiseGeneralProtectionFault0(pIemCpu);
        }
        /* V8086 */
        else if (uIopl == 3)
            pCtx->eflags.Bits.u1IF = 0;
        else if (   uIopl < 3
                 && (pCtx->cr4 & X86_CR4_VME) )
            pCtx->eflags.Bits.u1VIF = 0;
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    /* real mode */
    else
        pCtx->eflags.Bits.u1IF = 0;
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * Implements 'STI'.
 */
IEM_CIMPL_DEF_0(iemCImpl_sti)
{
    PCPUMCTX pCtx = pIemCpu->CTX_SUFF(pCtx);

    if (pCtx->cr0 & X86_CR0_PE)
    {
        uint8_t const uIopl = pCtx->eflags.Bits.u2IOPL;
        if (!pCtx->eflags.Bits.u1VM)
        {
            if (pIemCpu->uCpl <= uIopl)
                pCtx->eflags.Bits.u1IF = 1;
            else if (   pIemCpu->uCpl == 3
                     && (pCtx->cr4 & X86_CR4_PVI)
                     && !pCtx->eflags.Bits.u1VIP )
                pCtx->eflags.Bits.u1VIF = 1;
            else
                return iemRaiseGeneralProtectionFault0(pIemCpu);
        }
        /* V8086 */
        else if (uIopl == 3)
            pCtx->eflags.Bits.u1IF = 1;
        else if (   uIopl < 3
                 && (pCtx->cr4 & X86_CR4_VME)
                 && !pCtx->eflags.Bits.u1VIP )
            pCtx->eflags.Bits.u1VIF = 1;
        else
            return iemRaiseGeneralProtectionFault0(pIemCpu);
    }
    /* real mode */
    else
        pCtx->eflags.Bits.u1IF = 1;

    iemRegAddToRip(pIemCpu, cbInstr);
    EMSetInhibitInterruptsPC(IEMCPU_TO_VMCPU(pIemCpu), pCtx->rip);
    return VINF_SUCCESS;
}


/**
 * Implements 'HLT'.
 */
IEM_CIMPL_DEF_0(iemCImpl_hlt)
{
    if (pIemCpu->uCpl != 0)
        return iemRaiseGeneralProtectionFault0(pIemCpu);
    iemRegAddToRip(pIemCpu, cbInstr);
    return VINF_EM_HALT;
}


/*
 * Instantiate the various string operation combinations.
 */
#define OP_SIZE     8
#define ADDR_SIZE   16
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     8
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     8
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"

#define OP_SIZE     16
#define ADDR_SIZE   16
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     16
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     16
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"

#define OP_SIZE     32
#define ADDR_SIZE   16
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     32
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     32
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"

#define OP_SIZE     64
#define ADDR_SIZE   32
#include "IEMAllCImplStrInstr.cpp.h"
#define OP_SIZE     64
#define ADDR_SIZE   64
#include "IEMAllCImplStrInstr.cpp.h"


/** @} */


/** @name   "Microcode" macros.
 *
 * The idea is that we should be able to use the same code to interpret
 * instructions as well as recompiler instructions.  Thus this obfuscation.
 *
 * @{
 */
#define IEM_MC_BEGIN(cArgs, cLocals)                    {
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

#define IEM_MC_FETCH_GREG_U8(a_u8Dst, a_iGReg)          (a_u8Dst)  = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_ZX_U16(a_u16Dst, a_iGReg)  (a_u16Dst) = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_ZX_U32(a_u32Dst, a_iGReg)  (a_u32Dst) = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U8_ZX_U64(a_u64Dst, a_iGReg)  (a_u64Dst) = iemGRegFetchU8(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16(a_u16Dst, a_iGReg)        (a_u16Dst) = iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16_ZX_U32(a_u32Dst, a_iGReg) (a_u32Dst) = iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U16_ZX_U64(a_u64Dst, a_iGReg) (a_u64Dst) = iemGRegFetchU16(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U32(a_u32Dst, a_iGReg)        (a_u32Dst) = iemGRegFetchU32(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U32_ZX_U64(a_u64Dst, a_iGReg) (a_u64Dst) = iemGRegFetchU32(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_GREG_U64(a_u64Dst, a_iGReg)        (a_u64Dst) = iemGRegFetchU64(pIemCpu, (a_iGReg))
#define IEM_MC_FETCH_SREG_U16(a_u16Dst, a_iSReg)        (a_u16Dst) = iemSRegFetchU16(pIemCpu, (a_iSReg))
#define IEM_MC_FETCH_SREG_ZX_U32(a_u32Dst, a_iSReg)     (a_u32Dst) = iemSRegFetchU16(pIemCpu, (a_iSReg))
#define IEM_MC_FETCH_SREG_ZX_U64(a_u64Dst, a_iSReg)     (a_u64Dst) = iemSRegFetchU16(pIemCpu, (a_iSReg))
#define IEM_MC_FETCH_EFLAGS(a_EFlags)                   (a_EFlags) = (pIemCpu)->CTX_SUFF(pCtx)->eflags.u

#define IEM_MC_STORE_GREG_U8(a_iGReg, a_u8Value)        *iemGRegRefU8(pIemCpu, (a_iGReg)) = (a_u8Value)
#define IEM_MC_STORE_GREG_U16(a_iGReg, a_u16Value)      *(uint16_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (a_u16Value)
#define IEM_MC_STORE_GREG_U32(a_iGReg, a_u32Value)      *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (uint32_t)(a_u32Value) /* clear high bits. */
#define IEM_MC_STORE_GREG_U64(a_iGReg, a_u64Value)      *(uint64_t *)iemGRegRef(pIemCpu, (a_iGReg)) = (a_u64Value)

#define IEM_MC_REF_GREG_U8(a_pu8Dst, a_iGReg)           (a_pu8Dst) = iemGRegRefU8(pIemCpu, (a_iGReg))
#define IEM_MC_REF_GREG_U16(a_pu16Dst, a_iGReg)         (a_pu16Dst) = (uint16_t *)iemGRegRef(pIemCpu, (a_iGReg))
/** @todo User of IEM_MC_REF_GREG_U32 needs to clear the high bits on
 *        commit. */
#define IEM_MC_REF_GREG_U32(a_pu32Dst, a_iGReg)         (a_pu32Dst) = (uint32_t *)iemGRegRef(pIemCpu, (a_iGReg))
#define IEM_MC_REF_GREG_U64(a_pu64Dst, a_iGReg)         (a_pu64Dst) = (uint64_t *)iemGRegRef(pIemCpu, (a_iGReg))
#define IEM_MC_REF_EFLAGS(a_pEFlags)                    (a_pEFlags) = &(pIemCpu)->CTX_SUFF(pCtx)->eflags.u

#define IEM_MC_ADD_GREG_U8(a_iGReg, a_u16Value)         *(uint8_t *)iemGRegRef(pIemCpu, (a_iGReg)) += (a_u8Value)
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

#define IEM_MC_SET_EFL_BIT(a_fBit)                      do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u |= (a_fBit); } while (0)
#define IEM_MC_CLEAR_EFL_BIT(a_fBit)                    do { (pIemCpu)->CTX_SUFF(pCtx)->eflags.u &= ~(a_fBit); } while (0)



#define IEM_MC_FETCH_MEM_U8(a_u8Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU8(pIemCpu, &(a_u8Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU16(pIemCpu, &(a_u16Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU32(pIemCpu, &(a_u32Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_S32_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataS32SxU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem)))
#define IEM_MC_FETCH_MEM_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    IEM_MC_RETURN_ON_FAILURE(iemMemFetchDataU64(pIemCpu, &(a_u64Dst), (a_iSeg), (a_GCPtrMem)))

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

#define IEM_MC_STORE_MEM_U8(a_iSeg, a_GCPtrMem, a_u8Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU8(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u8Value)))
#define IEM_MC_STORE_MEM_U16(a_iSeg, a_GCPtrMem, a_u16Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU16(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u16Value)))
#define IEM_MC_STORE_MEM_U32(a_iSeg, a_GCPtrMem, a_u32Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU32(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u32Value)))
#define IEM_MC_STORE_MEM_U64(a_iSeg, a_GCPtrMem, a_u64Value) \
    IEM_MC_RETURN_ON_FAILURE(iemMemStoreDataU64(pIemCpu, (a_iSeg), (a_GCPtrMem), (a_u64Value)))

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

#define IEM_MC_IF_EFL_BIT_SET(a_fBit)                   if (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) {
#define IEM_MC_IF_EFL_ANY_BITS_SET(a_fBits)             if (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBits)) {
#define IEM_MC_IF_EFL_BITS_NE(a_fBit1, a_fBit2)         \
    if (   !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
        != !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
#define IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(a_fBit, a_fBit1, a_fBit2) \
    if (   (pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit)) \
        || !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit1)) \
        != !!(pIemCpu->CTX_SUFF(pCtx)->eflags.u & (a_fBit2)) ) {
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
    Log2(("decode - %04x:%08RGv %s\n", pIemCpu->CTX_SUFF(pCtx)->cs, pIemCpu->CTX_SUFF(pCtx)->rip, a_szMnemonic))
# define IEMOP_MNEMONIC2(a_szMnemonic, a_szOps) \
    Log2(("decode - %04x:%08RGv %s %s\n", pIemCpu->CTX_SUFF(pCtx)->cs, pIemCpu->CTX_SUFF(pCtx)->rip, a_szMnemonic, a_szOps))
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
        if (pIemCpu->fPrefixes & IEM_OP_PRF_LOCK) \
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
                IEM_OPCODE_GET_NEXT_U16(pIemCpu, &u16EffAddr);
            else
            {
                /* Get the displacment. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:  u16EffAddr = 0;                                       break;
                    case 1:  IEM_OPCODE_GET_NEXT_S8_SX_U16(pIemCpu, &u16EffAddr);  break;
                    case 2:  IEM_OPCODE_GET_NEXT_U16(pIemCpu, &u16EffAddr);        break;
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
                IEM_OPCODE_GET_NEXT_U32(pIemCpu, &u32EffAddr);
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
                        uint8_t bSib; IEM_OPCODE_GET_NEXT_BYTE(pIemCpu, &bSib);

                        /* Get the index and scale it. */
                        switch ((bSib & X86_SIB_INDEX_SHIFT) >> X86_SIB_INDEX_SMASK)
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
                                    IEM_OPCODE_GET_NEXT_U32(pIemCpu, &u32Disp);
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
                        int8_t i8Disp;
                        IEM_OPCODE_GET_NEXT_S8(pIemCpu, &i8Disp);
                        u32EffAddr += i8Disp;
                        break;
                    }
                    case 2:
                    {
                        uint32_t u32Disp;
                        IEM_OPCODE_GET_NEXT_U32(pIemCpu, &u32Disp);
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
                IEM_OPCODE_GET_NEXT_S32_SX_U64(pIemCpu, &u64EffAddr);
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
                        uint8_t bSib; IEM_OPCODE_GET_NEXT_BYTE(pIemCpu, &bSib);

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
                                    IEM_OPCODE_GET_NEXT_U32(pIemCpu, &u32Disp);
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
                        IEM_OPCODE_GET_NEXT_S8(pIemCpu, &i8Disp);
                        u64EffAddr += i8Disp;
                        break;
                    }
                    case 2:
                    {
                        uint32_t u32Disp;
                        IEM_OPCODE_GET_NEXT_U32(pIemCpu, &u32Disp);
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
    PCPUMCTX pOrgCtx = pIemCpu->CTX_SUFF(pCtx);

# ifndef IEM_VERIFICATION_MODE_NO_REM
    /*
     * Switch state.
     */
    static CPUMCTX  s_DebugCtx; /* Ugly! */

    s_DebugCtx = *pOrgCtx;
    pIemCpu->CTX_SUFF(pCtx) = &s_DebugCtx;
# endif

    /*
     * See if there is an interrupt pending in TRPM and inject it if we can.
     */
    PVMCPU pVCpu = IEMCPU_TO_VMCPU(pIemCpu);
    if (   pOrgCtx->eflags.Bits.u1IF
        && TRPMHasTrap(pVCpu)
        //&& TRPMIsSoftwareInterrupt(pVCpu)
        && EMGetInhibitInterruptsPC(pVCpu) != pOrgCtx->rip)
    {
        Log(("Injecting trap %#x\n", TRPMGetTrapNo(pVCpu)));
        iemCImpl_int(pIemCpu, 0, TRPMGetTrapNo(pVCpu), false);
    }

    /*
     * Reset the counters.
     */
    pIemCpu->cIOReads    = 0;
    pIemCpu->cIOWrites   = 0;
    pIemCpu->fMulDivHack = false;
    pIemCpu->fShiftOfHack= false;

# ifndef IEM_VERIFICATION_MODE_NO_REM
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
# endif
}


# ifndef IEM_VERIFICATION_MODE_NO_REM
/**
 * Allocate an event record.
 * @returns Poitner to a record.
 */
static PIEMVERIFYEVTREC iemVerifyAllocRecord(PIEMCPU pIemCpu)
{
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
# endif


/**
 * IOMMMIORead notification.
 */
VMM_INT_DECL(void)   IEMNotifyMMIORead(PVM pVM, RTGCPHYS GCPhys, size_t cbValue)
{
# ifndef IEM_VERIFICATION_MODE_NO_REM
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_READ;
    pEvtRec->u.RamRead.GCPhys  = GCPhys;
    pEvtRec->u.RamRead.cb      = cbValue;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
# endif
}


/**
 * IOMMMIOWrite notification.
 */
VMM_INT_DECL(void)   IEMNotifyMMIOWrite(PVM pVM, RTGCPHYS GCPhys, uint32_t u32Value, size_t cbValue)
{
# ifndef IEM_VERIFICATION_MODE_NO_REM
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_RAM_WRITE;
    pEvtRec->u.RamWrite.GCPhys   = GCPhys;
    pEvtRec->u.RamWrite.cb       = cbValue;
    pEvtRec->u.RamWrite.ab[0]    = RT_BYTE1(u32Value);
    pEvtRec->u.RamWrite.ab[1]    = RT_BYTE2(u32Value);
    pEvtRec->u.RamWrite.ab[2]    = RT_BYTE3(u32Value);
    pEvtRec->u.RamWrite.ab[3]    = RT_BYTE4(u32Value);
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
# endif
}


/**
 * IOMIOPortRead notification.
 */
VMM_INT_DECL(void)   IEMNotifyIOPortRead(PVM pVM, RTIOPORT Port, size_t cbValue)
{
# ifndef IEM_VERIFICATION_MODE_NO_REM
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_READ;
    pEvtRec->u.IOPortRead.Port    = Port;
    pEvtRec->u.IOPortRead.cbValue = cbValue;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
# endif
}

/**
 * IOMIOPortWrite notification.
 */
VMM_INT_DECL(void)   IEMNotifyIOPortWrite(PVM pVM, RTIOPORT Port, uint32_t u32Value, size_t cbValue)
{
# ifndef IEM_VERIFICATION_MODE_NO_REM
    PVMCPU              pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        return;
    PIEMCPU             pIemCpu = &pVCpu->iem.s;
    PIEMVERIFYEVTREC    pEvtRec = iemVerifyAllocRecord(pIemCpu);
    if (!pEvtRec)
        return;
    pEvtRec->enmEvent = IEMVERIFYEVENT_IOPORT_WRITE;
    pEvtRec->u.IOPortWrite.Port     = Port;
    pEvtRec->u.IOPortWrite.cbValue  = cbValue;
    pEvtRec->u.IOPortWrite.u32Value = u32Value;
    pEvtRec->pNext = *pIemCpu->ppOtherEvtRecNext;
    *pIemCpu->ppOtherEvtRecNext = pEvtRec;
# endif
}


VMM_INT_DECL(void)   IEMNotifyIOPortReadString(PVM pVM, RTIOPORT Port, RTGCPTR GCPtrDst, RTGCUINTREG cTransfers, size_t cbValue)
{
    AssertFailed();
}


VMM_INT_DECL(void)   IEMNotifyIOPortWriteString(PVM pVM, RTIOPORT Port, RTGCPTR GCPtrSrc, RTGCUINTREG cTransfers, size_t cbValue)
{
    AssertFailed();
}

# ifndef IEM_VERIFICATION_MODE_NO_REM

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
        pEvtRec->u.IOPortRead.cbValue = cbValue;
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
        pEvtRec->u.IOPortWrite.cbValue  = cbValue;
        pEvtRec->u.IOPortWrite.u32Value = u32Value;
        pEvtRec->pNext = *pIemCpu->ppIemEvtRecNext;
        *pIemCpu->ppIemEvtRecNext = pEvtRec;
    }
    pIemCpu->cIOWrites++;
    return VINF_SUCCESS;
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
 * @param   pEvtRec1        The first record.
 * @param   pEvtRec2        The second record.
 * @param   pszMsg          The message explaining why we're asserting.
 */
static void iemVerifyAssertRecords(PIEMVERIFYEVTREC pEvtRec1, PIEMVERIFYEVTREC pEvtRec2, const char *pszMsg)
{
    RTAssertMsg1(pszMsg, __LINE__, __FILE__, __PRETTY_FUNCTION__);
    iemVerifyAssertAddRecordDump(pEvtRec1);
    iemVerifyAssertAddRecordDump(pEvtRec2);
    RTAssertPanic();
}


/**
 * Raises an assertion on the specified record, showing the given message with
 * a record dump attached.
 *
 * @param   pEvtRec1        The first record.
 * @param   pszMsg          The message explaining why we're asserting.
 */
static void iemVerifyAssertRecord(PIEMVERIFYEVTREC pEvtRec, const char *pszMsg)
{
    RTAssertMsg1(pszMsg, __LINE__, __FILE__, __PRETTY_FUNCTION__);
    iemVerifyAssertAddRecordDump(pEvtRec);
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
            RTAssertMsg1(NULL, __LINE__, __FILE__, __PRETTY_FUNCTION__);
            RTAssertMsg2Weak("Memory at %RGv differs\n", pEvtRec->u.RamWrite.GCPhys);
            RTAssertMsg2Add("REM: %.*Rhxs\n"
                            "IEM: %.*Rhxs\n",
                            pEvtRec->u.RamWrite.cb, abBuf,
                            pEvtRec->u.RamWrite.cb, pEvtRec->u.RamWrite.ab);
            iemVerifyAssertAddRecordDump(pEvtRec);
            RTAssertPanic();
        }
    }

}

# endif /* !IEM_VERIFICATION_MODE_NO_REM */

/**
 * Performs the post-execution verfication checks.
 */
static void iemExecVerificationModeCheck(PIEMCPU pIemCpu)
{
# if defined(IEM_VERIFICATION_MODE) && !defined(IEM_VERIFICATION_MODE_NO_REM)
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
    int rc = REMR3EmulateInstruction(IEMCPU_TO_VM(pIemCpu), IEMCPU_TO_VMCPU(pIemCpu));
    AssertRC(rc);

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

        if (memcmp(&pOrgCtx->fpu, &pDebugCtx->fpu, sizeof(pDebugCtx->fpu)))
        {
            if (pIemCpu->cInstructions != 1)
            {
                RTAssertMsg2Weak("  the FPU state differs\n");
                cDiffs++;
            }
            else
                RTAssertMsg2Weak("  the FPU state differs - happens the first time...\n");
        }
        CHECK_FIELD(rip);
        uint32_t fFlagsMask = UINT32_MAX;
        if (pIemCpu->fMulDivHack)
            fFlagsMask &= ~(X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF);
        if (pIemCpu->fShiftOfHack)
            fFlagsMask &= ~(X86_EFL_OF);
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

        if (pIemCpu->cIOReads != 1)
            CHECK_FIELD(rax);
        CHECK_FIELD(rcx);
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
        CHECK_FIELD(cs);
        CHECK_FIELD(csHid.u64Base);
        CHECK_FIELD(csHid.u32Limit);
        CHECK_FIELD(csHid.Attr.u);
        CHECK_FIELD(ss);
        CHECK_FIELD(ssHid.u64Base);
        CHECK_FIELD(ssHid.u32Limit);
        CHECK_FIELD(ssHid.Attr.u);
        CHECK_FIELD(ds);
        CHECK_FIELD(dsHid.u64Base);
        CHECK_FIELD(dsHid.u32Limit);
        CHECK_FIELD(dsHid.Attr.u);
        CHECK_FIELD(es);
        CHECK_FIELD(esHid.u64Base);
        CHECK_FIELD(esHid.u32Limit);
        CHECK_FIELD(esHid.Attr.u);
        CHECK_FIELD(fs);
        CHECK_FIELD(fsHid.u64Base);
        CHECK_FIELD(fsHid.u32Limit);
        CHECK_FIELD(fsHid.Attr.u);
        CHECK_FIELD(gs);
        CHECK_FIELD(gsHid.u64Base);
        CHECK_FIELD(gsHid.u32Limit);
        CHECK_FIELD(gsHid.Attr.u);
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
            AssertFailed();
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
                iemVerifyAssertRecords(pIemRec, pOtherRec, "Type mismatches");
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
                iemVerifyAssertRecords(pIemRec, pOtherRec, "Mismatch");
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
            iemVerifyAssertRecord(pIemRec, "Extra IEM record!");
        else if (pOtherRec != NULL)
            iemVerifyAssertRecord(pIemRec, "Extra Other record!");
    }
    pIemCpu->CTX_SUFF(pCtx) = pOrgCtx;
# endif
}

#endif /* IEM_VERIFICATION_MODE && IN_RING3 */


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
    if (0)//LogIs2Enabled())
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

    uint8_t b; IEM_OPCODE_GET_NEXT_BYTE(pIemCpu, &b);
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
            b; IEM_OPCODE_GET_NEXT_BYTE(pIemCpu, &b);
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

