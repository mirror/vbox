/** @file
 * CPUM - CPU Monitor(/ Manager).
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_cpum_h
#define ___VBox_vmm_cpum_h

#include <iprt/x86.h>
#include <VBox/types.h>
#include <VBox/vmm/cpumctx.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_cpum      The CPU Monitor / Manager API
 * @{
 */

/**
 * CPUID feature to set or clear.
 */
typedef enum CPUMCPUIDFEATURE
{
    CPUMCPUIDFEATURE_INVALID = 0,
    /** The APIC feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_APIC,
    /** The sysenter/sysexit feature bit. (Std) */
    CPUMCPUIDFEATURE_SEP,
    /** The SYSCALL/SYSEXIT feature bit (64 bits mode only for Intel CPUs). (Ext) */
    CPUMCPUIDFEATURE_SYSCALL,
    /** The PAE feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_PAE,
    /** The NXE feature bit. (Ext) */
    CPUMCPUIDFEATURE_NXE,
    /** The LAHF/SAHF feature bit (64 bits mode only). (Ext) */
    CPUMCPUIDFEATURE_LAHF,
    /** The LONG MODE feature bit. (Ext) */
    CPUMCPUIDFEATURE_LONG_MODE,
    /** The PAT feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_PAT,
    /** The x2APIC  feature bit. (Std) */
    CPUMCPUIDFEATURE_X2APIC,
    /** The RDTSCP feature bit. (Ext) */
    CPUMCPUIDFEATURE_RDTSCP,
    /** The Hypervisor Present bit. (Std) */
    CPUMCPUIDFEATURE_HVP,
    /** 32bit hackishness. */
    CPUMCPUIDFEATURE_32BIT_HACK = 0x7fffffff
} CPUMCPUIDFEATURE;

/**
 * CPU Vendor.
 */
typedef enum CPUMCPUVENDOR
{
    CPUMCPUVENDOR_INVALID = 0,
    CPUMCPUVENDOR_INTEL,
    CPUMCPUVENDOR_AMD,
    CPUMCPUVENDOR_VIA,
    CPUMCPUVENDOR_UNKNOWN,
    CPUMCPUVENDOR_SYNTHETIC,
    /** 32bit hackishness. */
    CPUMCPUVENDOR_32BIT_HACK = 0x7fffffff
} CPUMCPUVENDOR;


/** @name Guest Register Getters.
 * @{ */
VMMDECL(void)       CPUMGetGuestGDTR(PVMCPU pVCpu, PVBOXGDTR pGDTR);
VMMDECL(RTGCPTR)    CPUMGetGuestIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)      CPUMGetGuestTR(PVMCPU pVCpu, PCPUMSELREGHID pHidden);
VMMDECL(RTSEL)      CPUMGetGuestLDTR(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR4(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR8(PVMCPU pVCpu);
VMMDECL(int)        CPUMGetGuestCRx(PVMCPU pVCpu, unsigned iReg, uint64_t *pValue);
VMMDECL(uint32_t)   CPUMGetGuestEFlags(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEIP(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestRIP(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEAX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEBX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestECX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEDX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestESI(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEDI(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestESP(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEBP(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestCS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestDS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestES(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestFS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestGS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestSS(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR1(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR6(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR7(PVMCPU pVCpu);
VMMDECL(int)        CPUMGetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t *pValue);
VMMDECL(void)       CPUMGetGuestCpuId(PVMCPU pVCpu, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdStdMax(PVM pVM);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdExtMax(PVM pVM);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdCentaurMax(PVM pVM);
VMMDECL(uint64_t)   CPUMGetGuestEFER(PVMCPU pVCpu);
VMMDECL(int)        CPUMQueryGuestMsr(PVMCPU pVCpu, uint32_t idMsr, uint64_t *puValue);
VMMDECL(int)        CPUMSetGuestMsr(PVMCPU pVCpu, uint32_t idMsr, uint64_t uValue);
VMMDECL(CPUMCPUVENDOR)  CPUMGetGuestCpuVendor(PVM pVM);
VMMDECL(CPUMCPUVENDOR)  CPUMGetHostCpuVendor(PVM pVM);
/** @} */

/** @name Guest Register Setters.
 * @{ */
VMMDECL(int)        CPUMSetGuestGDTR(PVMCPU pVCpu, uint64_t GCPtrBase, uint16_t cbLimit);
VMMDECL(int)        CPUMSetGuestIDTR(PVMCPU pVCpu, uint64_t GCPtrBase, uint16_t cbLimit);
VMMDECL(int)        CPUMSetGuestTR(PVMCPU pVCpu, uint16_t tr);
VMMDECL(int)        CPUMSetGuestLDTR(PVMCPU pVCpu, uint16_t ldtr);
VMMDECL(int)        CPUMSetGuestCR0(PVMCPU pVCpu, uint64_t cr0);
VMMDECL(int)        CPUMSetGuestCR2(PVMCPU pVCpu, uint64_t cr2);
VMMDECL(int)        CPUMSetGuestCR3(PVMCPU pVCpu, uint64_t cr3);
VMMDECL(int)        CPUMSetGuestCR4(PVMCPU pVCpu, uint64_t cr4);
VMMDECL(int)        CPUMSetGuestDR0(PVMCPU pVCpu, uint64_t uDr0);
VMMDECL(int)        CPUMSetGuestDR1(PVMCPU pVCpu, uint64_t uDr1);
VMMDECL(int)        CPUMSetGuestDR2(PVMCPU pVCpu, uint64_t uDr2);
VMMDECL(int)        CPUMSetGuestDR3(PVMCPU pVCpu, uint64_t uDr3);
VMMDECL(int)        CPUMSetGuestDR6(PVMCPU pVCpu, uint64_t uDr6);
VMMDECL(int)        CPUMSetGuestDR7(PVMCPU pVCpu, uint64_t uDr7);
VMMDECL(int)        CPUMSetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t Value);
VMMDECL(int)        CPUMSetGuestEFlags(PVMCPU pVCpu, uint32_t eflags);
VMMDECL(int)        CPUMSetGuestEIP(PVMCPU pVCpu, uint32_t eip);
VMMDECL(int)        CPUMSetGuestEAX(PVMCPU pVCpu, uint32_t eax);
VMMDECL(int)        CPUMSetGuestEBX(PVMCPU pVCpu, uint32_t ebx);
VMMDECL(int)        CPUMSetGuestECX(PVMCPU pVCpu, uint32_t ecx);
VMMDECL(int)        CPUMSetGuestEDX(PVMCPU pVCpu, uint32_t edx);
VMMDECL(int)        CPUMSetGuestESI(PVMCPU pVCpu, uint32_t esi);
VMMDECL(int)        CPUMSetGuestEDI(PVMCPU pVCpu, uint32_t edi);
VMMDECL(int)        CPUMSetGuestESP(PVMCPU pVCpu, uint32_t esp);
VMMDECL(int)        CPUMSetGuestEBP(PVMCPU pVCpu, uint32_t ebp);
VMMDECL(int)        CPUMSetGuestCS(PVMCPU pVCpu, uint16_t cs);
VMMDECL(int)        CPUMSetGuestDS(PVMCPU pVCpu, uint16_t ds);
VMMDECL(int)        CPUMSetGuestES(PVMCPU pVCpu, uint16_t es);
VMMDECL(int)        CPUMSetGuestFS(PVMCPU pVCpu, uint16_t fs);
VMMDECL(int)        CPUMSetGuestGS(PVMCPU pVCpu, uint16_t gs);
VMMDECL(int)        CPUMSetGuestSS(PVMCPU pVCpu, uint16_t ss);
VMMDECL(void)       CPUMSetGuestEFER(PVMCPU pVCpu, uint64_t val);
VMMDECL(void)       CPUMSetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(void)       CPUMClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(bool)       CPUMGetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(void)       CPUMSetGuestCtx(PVMCPU pVCpu, const PCPUMCTX pCtx);
/** @} */


/** @name Misc Guest Predicate Functions.
 * @{  */

VMMDECL(bool)       CPUMIsGuestIn16BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestIn32BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestNXEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestPageSizeExtEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestPagingEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestR0WriteProtEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInRealMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInRealOrV86Mode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInProtectedMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInPagedProtectedMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInLongMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInPAEMode(PVMCPU pVCpu);

#ifndef VBOX_WITHOUT_UNNAMED_UNIONS

/**
 * Tests if the guest is running in real mode or not.
 *
 * @returns true if in real mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestInRealModeEx(PCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE);
}

/**
 * Tests if the guest is running in real or virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestInRealOrV86ModeEx(PCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE)
        || pCtx->eflags.Bits.u1VM; /** @todo verify that this cannot be set in long mode. */
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool)    CPUMIsGuestInPagedProtectedModeEx(PCPUMCTX pCtx)
{
    return (pCtx->cr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG);
}

/**
 * Tests if the guest is running in long mode or not.
 *
 * @returns true if in long mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestInLongModeEx(PCPUMCTX pCtx)
{
    return (pCtx->msrEFER & MSR_K6_EFER_LMA) == MSR_K6_EFER_LMA;
}

/**
 * Tests if the guest is running in 64 bits mode or not.
 *
 * @returns true if in 64 bits protected mode, otherwise false.
 * @param   pVM     The VM handle.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestIn64BitCode(PVMCPU pVCpu, PCCPUMCTXCORE pCtx)
{
    if (!CPUMIsGuestInLongMode(pVCpu))
        return false;

    return pCtx->cs.Attr.n.u1Long;
}

/**
 * Tests if the guest is running in 64 bits mode or not.
 *
 * @returns true if in 64 bits protected mode, otherwise false.
 * @param   pVM     The VM handle.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestIn64BitCodeEx(PCCPUMCTX pCtx)
{
    if (!(pCtx->msrEFER & MSR_K6_EFER_LMA))
        return false;

    return pCtx->cs.Attr.n.u1Long;
}

/**
 * Tests if the guest is running in PAE mode or not.
 *
 * @returns true if in PAE mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestInPAEModeEx(PCPUMCTX pCtx)
{
    return (    (pCtx->cr4 & X86_CR4_PAE)
            &&  CPUMIsGuestInPagedProtectedModeEx(pCtx)
            &&  !CPUMIsGuestInLongModeEx(pCtx));
}

#endif /* VBOX_WITHOUT_UNNAMED_UNIONS */

/** @} */


/** @name Hypervisor Register Getters.
 * @{ */
VMMDECL(RTSEL)          CPUMGetHyperCS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperDS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperES(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperFS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperGS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperSS(PVMCPU pVCpu);
#if 0 /* these are not correct. */
VMMDECL(uint32_t)       CPUMGetHyperCR0(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR2(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR3(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR4(PVMCPU pVCpu);
#endif
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperEAX(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEBX(PVMCPU pVCpu);
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperECX(PVMCPU pVCpu);
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperEDX(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperESI(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEDI(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEBP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperESP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEFlags(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEIP(PVMCPU pVCpu);
VMMDECL(uint64_t)       CPUMGetHyperRIP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(uint32_t)       CPUMGetHyperGDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)          CPUMGetHyperLDTR(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR0(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR1(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR2(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR3(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR6(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR7(PVMCPU pVCpu);
VMMDECL(void)           CPUMGetHyperCtx(PVMCPU pVCpu, PCPUMCTX pCtx);
VMMDECL(uint32_t)       CPUMGetHyperCR3(PVMCPU pVCpu);
/** @} */

/** @name Hypervisor Register Setters.
 * @{ */
VMMDECL(void)           CPUMSetHyperGDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(void)           CPUMSetHyperLDTR(PVMCPU pVCpu, RTSEL SelLDTR);
VMMDECL(void)           CPUMSetHyperIDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(void)           CPUMSetHyperCR3(PVMCPU pVCpu, uint32_t cr3);
VMMDECL(void)           CPUMSetHyperTR(PVMCPU pVCpu, RTSEL SelTR);
VMMDECL(void)           CPUMSetHyperCS(PVMCPU pVCpu, RTSEL SelCS);
VMMDECL(void)           CPUMSetHyperDS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperES(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperFS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperGS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperSS(PVMCPU pVCpu, RTSEL SelSS);
VMMDECL(void)           CPUMSetHyperESP(PVMCPU pVCpu, uint32_t u32ESP);
VMMDECL(int)            CPUMSetHyperEFlags(PVMCPU pVCpu, uint32_t Efl);
VMMDECL(void)           CPUMSetHyperEIP(PVMCPU pVCpu, uint32_t u32EIP);
VMMDECL(void)           CPUMSetHyperDR0(PVMCPU pVCpu, RTGCUINTREG uDr0);
VMMDECL(void)           CPUMSetHyperDR1(PVMCPU pVCpu, RTGCUINTREG uDr1);
VMMDECL(void)           CPUMSetHyperDR2(PVMCPU pVCpu, RTGCUINTREG uDr2);
VMMDECL(void)           CPUMSetHyperDR3(PVMCPU pVCpu, RTGCUINTREG uDr3);
VMMDECL(void)           CPUMSetHyperDR6(PVMCPU pVCpu, RTGCUINTREG uDr6);
VMMDECL(void)           CPUMSetHyperDR7(PVMCPU pVCpu, RTGCUINTREG uDr7);
VMMDECL(void)           CPUMSetHyperCtx(PVMCPU pVCpu, const PCPUMCTX pCtx);
VMMDECL(int)            CPUMRecalcHyperDRx(PVMCPU pVCpu);
/** @} */

VMMDECL(void)           CPUMPushHyper(PVMCPU pVCpu, uint32_t u32);
VMMDECL(int)            CPUMQueryHyperCtxPtr(PVMCPU pVCpu, PCPUMCTX *ppCtx);
VMMDECL(PCPUMCTX)       CPUMGetHyperCtxPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetHyperCtxCore(PVMCPU pVCpu);
VMMDECL(PCPUMCTX)       CPUMQueryGuestCtxPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetGuestCtxCore(PVMCPU pVCpu);
VMMDECL(void)           CPUMSetGuestCtxCore(PVMCPU pVCpu, PCCPUMCTXCORE pCtxCore);
VMMR3DECL(int)          CPUMR3RawEnter(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
VMMR3DECL(int)          CPUMR3RawLeave(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, int rc);
VMMDECL(uint32_t)       CPUMRawGetEFlags(PVMCPU pVCpu);
VMMDECL(void)           CPUMRawSetEFlags(PVMCPU pVCpu, uint32_t fEfl);
VMMDECL(int)            CPUMHandleLazyFPU(PVMCPU pVCpu);

/** @name Changed flags
 * These flags are used to keep track of which important register that
 * have been changed since last they were reset. The only one allowed
 * to clear them is REM!
 * @{
 */
#define CPUM_CHANGED_FPU_REM                    RT_BIT(0)
#define CPUM_CHANGED_CR0                        RT_BIT(1)
#define CPUM_CHANGED_CR4                        RT_BIT(2)
#define CPUM_CHANGED_GLOBAL_TLB_FLUSH           RT_BIT(3)
#define CPUM_CHANGED_CR3                        RT_BIT(4)
#define CPUM_CHANGED_GDTR                       RT_BIT(5)
#define CPUM_CHANGED_IDTR                       RT_BIT(6)
#define CPUM_CHANGED_LDTR                       RT_BIT(7)
#define CPUM_CHANGED_TR                         RT_BIT(8)
#define CPUM_CHANGED_SYSENTER_MSR               RT_BIT(9)
#define CPUM_CHANGED_HIDDEN_SEL_REGS            RT_BIT(10)
#define CPUM_CHANGED_CPUID                      RT_BIT(11)
/** All except CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID.  */
#define CPUM_CHANGED_ALL                        (  CPUM_CHANGED_FPU_REM \
                                                 | CPUM_CHANGED_CR0 \
                                                 | CPUM_CHANGED_GLOBAL_TLB_FLUSH \
                                                 | CPUM_CHANGED_CR3 \
                                                 | CPUM_CHANGED_CR4 \
                                                 | CPUM_CHANGED_GDTR \
                                                 | CPUM_CHANGED_IDTR \
                                                 | CPUM_CHANGED_LDTR \
                                                 | CPUM_CHANGED_TR \
                                                 | CPUM_CHANGED_SYSENTER_MSR \
                                                 | CPUM_CHANGED_HIDDEN_SEL_REGS \
                                                 | CPUM_CHANGED_CPUID )
/** This one is used by raw-mode to indicate that the hidden register
 * information is not longer reliable and have to be re-determined.
 *
 * @remarks This must not be part of CPUM_CHANGED_ALL! */
#define CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID    RT_BIT(12)
/** @} */

VMMDECL(void)           CPUMSetChangedFlags(PVMCPU pVCpu, uint32_t fChangedFlags);
VMMR3DECL(uint32_t)     CPUMR3RemEnter(PVMCPU pVCpu, uint32_t *puCpl);
VMMR3DECL(void)         CPUMR3RemLeave(PVMCPU pVCpu, bool fNoOutOfSyncSels);
VMMDECL(bool)           CPUMSupportsFXSR(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysEnter(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysCall(PVM pVM);
VMMDECL(bool)           CPUMIsGuestFPUStateActive(PVMCPU pVCPU);
VMMDECL(void)           CPUMDeactivateGuestFPUState(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestDebugStateActive(PVMCPU pVCpu);
VMMDECL(void)           CPUMDeactivateGuestDebugState(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsHyperDebugStateActive(PVMCPU pVCpu);
VMMDECL(void)           CPUMDeactivateHyperDebugState(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestCPL(PVMCPU pVCpu);
VMMDECL(bool)           CPUMAreHiddenSelRegsValid(PVMCPU pVCpu);
VMMDECL(CPUMMODE)       CPUMGetGuestMode(PVMCPU pVCpu);


#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

VMMR3DECL(int)          CPUMR3Init(PVM pVM);
VMMR3DECL(void)         CPUMR3Relocate(PVM pVM);
VMMR3DECL(int)          CPUMR3Term(PVM pVM);
VMMR3DECL(void)         CPUMR3Reset(PVM pVM);
VMMR3DECL(void)         CPUMR3ResetCpu(PVMCPU pVCpu);
VMMDECL(bool)           CPUMR3IsStateRestorePending(PVM pVM);
VMMR3DECL(void)         CPUMR3SetHWVirtEx(PVM pVM, bool fHWVirtExEnabled);
VMMR3DECL(int)          CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdStdRCPtr(PVM pVM);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdExtRCPtr(PVM pVM);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdCentaurRCPtr(PVM pVM);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdDefRCPtr(PVM pVM);

/** @} */
#endif /* IN_RING3 */

#ifdef IN_RC
/** @defgroup grp_cpum_gc    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

/**
 * Calls a guest trap/interrupt handler directly
 *
 * Assumes a trap stack frame has already been setup on the guest's stack!
 * This function does not return!
 *
 * @param   pRegFrame   Original trap/interrupt context
 * @param   selCS       Code selector of handler
 * @param   pHandler    GC virtual address of handler
 * @param   eflags      Callee's EFLAGS
 * @param   selSS       Stack selector for handler
 * @param   pEsp        Stack address for handler
 */
DECLASM(void)           CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTRCPTR pHandler,
                                                   uint32_t eflags, uint32_t selSS, RTRCPTR pEsp);

/**
 * Call guest V86 code directly.
 *
 * This function does not return!
 *
 * @param   pRegFrame   Original trap/interrupt context
 */
DECLASM(void)           CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);

/** @} */
#endif /* IN_RC */

#ifdef IN_RING0
/** @defgroup grp_cpum_r0    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */
VMMR0DECL(int)          CPUMR0ModuleInit(void);
VMMR0DECL(int)          CPUMR0ModuleTerm(void);
VMMR0DECL(int)          CPUMR0Init(PVM pVM);
VMMR0DECL(int)          CPUMR0LoadGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          CPUMR0SaveGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          CPUMR0SaveGuestDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6);
VMMR0DECL(int)          CPUMR0LoadGuestDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6);
VMMR0DECL(int)          CPUMR0LoadHostDebugState(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          CPUMR0SaveHostDebugState(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          CPUMR0LoadHyperDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6);
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
VMMR0DECL(void)         CPUMR0SetLApic(PVM pVM, RTCPUID idHostCpu);
#endif

/** @} */
#endif /* IN_RING0 */

/** @} */
RT_C_DECLS_END


#endif

