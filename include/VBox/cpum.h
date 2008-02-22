/** @file
 * CPUM - CPU Monitor(/ Manager).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_cpum_h
#define ___VBox_cpum_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>


__BEGIN_DECLS

/** @defgroup grp_cpum      The CPU Monitor(/Manager) API
 * @{
 */

/**
 * Selector hidden registers.
 */
typedef struct CPUMSELREGHIDDEN
{
    /** Base register. */
    uint32_t    u32Base;
    /** Limit (expanded). */
    uint32_t    u32Limit;
    /** Flags.
     * This is the high 32-bit word of the descriptor entry.
     * Only the flags, dpl and type are used. */
    X86DESCATTR Attr;
} CPUMSELREGHID;
/** Pointer to selector hidden registers. */
typedef CPUMSELREGHID *PCPUMSELREGHID;
/** Pointer to const selector hidden registers. */
typedef const CPUMSELREGHID *PCCPUMSELREGHID;


/**
 * The sysenter register set.
 */
typedef struct CPUMSYSENTER
{
    /** Ring 0 cs.
     * This value +  8 is the Ring 0 ss.
     * This value + 16 is the Ring 3 cs.
     * This value + 24 is the Ring 3 ss.
     */
    uint64_t    cs;
    /** Ring 0 eip. */
    uint64_t    eip;
    /** Ring 0 esp. */
    uint64_t    esp;
} CPUMSYSENTER;


/**
 * CPU context core.
 */
#pragma pack(1)
typedef struct CPUMCTXCORE
{
    union
    {
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint32_t        ecx;
        uint64_t        rcx;
    };
    /* Note: we rely on the exact layout, because we use lss esp, [] in the switcher */
    uint32_t        esp;
    RTSEL           ss;
    RTSEL           ssPadding;
    /* Note: no overlap with esp here. */
    uint64_t        rsp;

    RTSEL           gs;
    RTSEL           gsPadding;
    RTSEL           fs;
    RTSEL           fsPadding;
    RTSEL           es;
    RTSEL           esPadding;
    RTSEL           ds;
    RTSEL           dsPadding;
    RTSEL           cs;
    RTSEL           csPadding[3];  /* 3 words to force 8 byte alignment for the remainder */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint32_t        eip;
        uint64_t        rip;
    };

    uint64_t            r8;
    uint64_t            r9;
    uint64_t            r10;
    uint64_t            r11;
    uint64_t            r12;
    uint64_t            r13;
    uint64_t            r14;
    uint64_t            r15;

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   esHid;
    CPUMSELREGHID   csHid;
    CPUMSELREGHID   ssHid;
    CPUMSELREGHID   dsHid;
    CPUMSELREGHID   fsHid;
    CPUMSELREGHID   gsHid;
    /** @} */

} CPUMCTXCORE;
/** Pointer to CPU context core. */
typedef CPUMCTXCORE *PCPUMCTXCORE;
/** Pointer to const CPU context core. */
typedef const CPUMCTXCORE *PCCPUMCTXCORE;
#pragma pack()

/**
 * CPU context.
 */
#pragma pack(1)
typedef struct CPUMCTX
{
    /** FPU state. (16-byte alignment)
     * @todo This doesn't have to be in X86FXSTATE on CPUs without fxsr - we need a type for the
     *       actual format or convert it (waste of time).  */
    X86FXSTATE      fpu;

    /** CPUMCTXCORE Part.
     * @{ */
    union
    {
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint32_t        ecx;
        uint64_t        rcx;
    };
    /* Note: we rely on the exact layout, because we use lss esp, [] in the switcher */
    uint32_t        esp;
    RTSEL           ss;
    RTSEL           ssPadding;
    /* Note: no overlap with esp here. */
    uint64_t        rsp;

    RTSEL           gs;
    RTSEL           gsPadding;
    RTSEL           fs;
    RTSEL           fsPadding;
    RTSEL           es;
    RTSEL           esPadding;
    RTSEL           ds;
    RTSEL           dsPadding;
    RTSEL           cs;
    RTSEL           csPadding[3];  /* 3 words to force 8 byte alignment for the remainder */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint32_t        eip;
        uint64_t        rip;
    };

    uint64_t            r8;
    uint64_t            r9;
    uint64_t            r10;
    uint64_t            r11;
    uint64_t            r12;
    uint64_t            r13;
    uint64_t            r14;
    uint64_t            r15;

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   esHid;
    CPUMSELREGHID   csHid;
    CPUMSELREGHID   ssHid;
    CPUMSELREGHID   dsHid;
    CPUMSELREGHID   fsHid;
    CPUMSELREGHID   gsHid;
    /** @} */

    /** @} */

    /** Control registers.
     * @{ */
    uint64_t        cr0;
    uint64_t        cr2;
    uint64_t        cr3;
    uint64_t        cr4;
    uint64_t        cr8;
    /** @} */

    /** Debug registers.
     * @{ */
    uint64_t        dr0;
    uint64_t        dr1;
    uint64_t        dr2;
    uint64_t        dr3;
    uint64_t        dr4; /**< @todo remove dr4 and dr5. */
    uint64_t        dr5;
    uint64_t        dr6;
    uint64_t        dr7;
    /* DR8-15 are currently not supported */
    /** @} */

    /** Global Descriptor Table register. */
    VBOXGDTR        gdtr;
    uint16_t        gdtrPadding;
    uint32_t        gdtrPadding64;/** @todo fix this hack */
    /** Interrupt Descriptor Table register. */
    VBOXIDTR        idtr;
    uint16_t        idtrPadding;
    uint32_t        idtrPadding64;/** @todo fix this hack */
    /** The task register.
     * Only the guest context uses all the members. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register.
     * Only the guest context uses all the members. */
    RTSEL           tr;
    RTSEL           trPadding;

    /** The sysenter msr registers.
     * This member is not used by the hypervisor context. */
    CPUMSYSENTER    SysEnter;

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   ldtrHid;
    CPUMSELREGHID   trHid;
    /** @} */

    /* padding to get 32byte aligned size */
    uint32_t        padding[4];
} CPUMCTX;
#pragma pack()
/** Pointer to CPUMCTX. */
typedef CPUMCTX *PCPUMCTX;

/**
 * Gets the CPUMCTXCORE part of a CPUMCTX.
 */
#define CPUMCTX2CORE(pCtx) ((PCPUMCTXCORE)(void *)&(pCtx)->edi)

/**
 * The register set returned by a CPUID operation.
 */
typedef struct CPUMCPUID
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} CPUMCPUID;
/** Pointer to a CPUID leaf. */
typedef CPUMCPUID *PCPUMCPUID;
/** Pointer to a const CPUID leaf. */
typedef const CPUMCPUID *PCCPUMCPUID;

/**
 * CPUID feature to set or clear.
 */
typedef enum CPUMCPUIDFEATURE
{
    CPUMCPUIDFEATURE_INVALID = 0,
    /** The APIC feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_APIC,
    /** The sysenter/sysexit feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_SEP
} CPUMCPUIDFEATURE;


/** @name Guest Register Getters.
 * @{ */
CPUMDECL(void)      CPUMGetGuestGDTR(PVM pVM, PVBOXGDTR pGDTR);
CPUMDECL(uint32_t)  CPUMGetGuestIDTR(PVM pVM, uint16_t *pcbLimit);
CPUMDECL(RTSEL)     CPUMGetGuestTR(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestLDTR(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCR0(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCR2(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCR3(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCR4(PVM pVM);
CPUMDECL(int)       CPUMGetGuestCRx(PVM pVM, uint32_t iReg, uint32_t *pValue);
CPUMDECL(uint32_t)  CPUMGetGuestEFlags(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEIP(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEAX(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEBX(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestECX(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEDX(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestESI(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEDI(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestESP(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEBP(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestCS(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestDS(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestES(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestFS(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestGS(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestSS(PVM pVM);
CPUMDECL(RTUINTREG) CPUMGetGuestDR0(PVM pVM);
CPUMDECL(RTUINTREG) CPUMGetGuestDR1(PVM pVM);
CPUMDECL(RTUINTREG) CPUMGetGuestDR2(PVM pVM);
CPUMDECL(RTUINTREG) CPUMGetGuestDR3(PVM pVM);
CPUMDECL(RTUINTREG) CPUMGetGuestDR6(PVM pVM);
CPUMDECL(RTUINTREG) CPUMGetGuestDR7(PVM pVM);
CPUMDECL(int)       CPUMGetGuestDRx(PVM pVM, uint32_t iReg, uint32_t *pValue);
CPUMDECL(void)      CPUMGetGuestCpuId(PVM pVM, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
CPUMDECL(GCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdStdGCPtr(PVM pVM);
CPUMDECL(GCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdExtGCPtr(PVM pVM);
CPUMDECL(GCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdCentaurGCPtr(PVM pVM);
CPUMDECL(GCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdDefGCPtr(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCpuIdStdMax(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCpuIdExtMax(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCpuIdCentaurMax(PVM pVM);
CPUMDECL(CPUMSELREGHID *) CPUMGetGuestTRHid(PVM pVM);
/** @} */

/** @name Guest Register Setters.
 * @{ */
CPUMDECL(int)       CPUMSetGuestGDTR(PVM pVM, uint32_t addr, uint16_t limit);
CPUMDECL(int)       CPUMSetGuestIDTR(PVM pVM, uint32_t addr, uint16_t limit);
CPUMDECL(int)       CPUMSetGuestTR(PVM pVM, uint16_t tr);
CPUMDECL(int)       CPUMSetGuestLDTR(PVM pVM, uint16_t ldtr);
CPUMDECL(int)       CPUMSetGuestCR0(PVM pVM, uint32_t cr0);
CPUMDECL(int)       CPUMSetGuestCR2(PVM pVM, uint32_t cr2);
CPUMDECL(int)       CPUMSetGuestCR3(PVM pVM, uint32_t cr3);
CPUMDECL(int)       CPUMSetGuestCR4(PVM pVM, uint32_t cr4);
CPUMDECL(int)       CPUMSetGuestCRx(PVM pVM, uint32_t iReg, uint32_t Value);
CPUMDECL(int)       CPUMSetGuestDR0(PVM pVM, RTGCUINTREG uDr0);
CPUMDECL(int)       CPUMSetGuestDR1(PVM pVM, RTGCUINTREG uDr1);
CPUMDECL(int)       CPUMSetGuestDR2(PVM pVM, RTGCUINTREG uDr2);
CPUMDECL(int)       CPUMSetGuestDR3(PVM pVM, RTGCUINTREG uDr3);
CPUMDECL(int)       CPUMSetGuestDR6(PVM pVM, RTGCUINTREG uDr6);
CPUMDECL(int)       CPUMSetGuestDR7(PVM pVM, RTGCUINTREG uDr7);
CPUMDECL(int)       CPUMSetGuestDRx(PVM pVM, uint32_t iReg, uint32_t Value);
CPUMDECL(int)       CPUMSetGuestEFlags(PVM pVM, uint32_t eflags);
CPUMDECL(int)       CPUMSetGuestEIP(PVM pVM, uint32_t eip);
CPUMDECL(int)       CPUMSetGuestEAX(PVM pVM, uint32_t eax);
CPUMDECL(int)       CPUMSetGuestEBX(PVM pVM, uint32_t ebx);
CPUMDECL(int)       CPUMSetGuestECX(PVM pVM, uint32_t ecx);
CPUMDECL(int)       CPUMSetGuestEDX(PVM pVM, uint32_t edx);
CPUMDECL(int)       CPUMSetGuestESI(PVM pVM, uint32_t esi);
CPUMDECL(int)       CPUMSetGuestEDI(PVM pVM, uint32_t edi);
CPUMDECL(int)       CPUMSetGuestESP(PVM pVM, uint32_t esp);
CPUMDECL(int)       CPUMSetGuestEBP(PVM pVM, uint32_t ebp);
CPUMDECL(int)       CPUMSetGuestCS(PVM pVM, uint16_t cs);
CPUMDECL(int)       CPUMSetGuestDS(PVM pVM, uint16_t ds);
CPUMDECL(int)       CPUMSetGuestES(PVM pVM, uint16_t es);
CPUMDECL(int)       CPUMSetGuestFS(PVM pVM, uint16_t fs);
CPUMDECL(int)       CPUMSetGuestGS(PVM pVM, uint16_t gs);
CPUMDECL(int)       CPUMSetGuestSS(PVM pVM, uint16_t ss);
CPUMDECL(void)      CPUMSetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
CPUMDECL(void)      CPUMClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
CPUMDECL(void)      CPUMSetGuestCtx(PVM pVM, const PCPUMCTX pCtx);
/** @} */

/** @name Misc Guest Predicate Functions.
 * @{  */

/**
 * Tests if the guest is running in real mode or not.
 *
 * @returns true if in real mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInRealMode(PVM pVM)
{
    return !(CPUMGetGuestCR0(pVM) & X86_CR0_PE);
}

/**
 * Tests if the guest is running in protected or not.
 *
 * @returns true if in protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInProtectedMode(PVM pVM)
{
    return !!(CPUMGetGuestCR0(pVM) & X86_CR0_PE);
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInPagedProtectedMode(PVM pVM)
{
    return (CPUMGetGuestCR0(pVM) & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG);
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsGuestIn16BitCode(PVM pVM);

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsGuestIn32BitCode(PVM pVM);

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsGuestIn64BitCode(PVM pVM);

/** @} */



/** @name Hypervisor Register Getters.
 * @{ */
CPUMDECL(RTSEL)         CPUMGetHyperCS(PVM pVM);
CPUMDECL(RTSEL)         CPUMGetHyperDS(PVM pVM);
CPUMDECL(RTSEL)         CPUMGetHyperES(PVM pVM);
CPUMDECL(RTSEL)         CPUMGetHyperFS(PVM pVM);
CPUMDECL(RTSEL)         CPUMGetHyperGS(PVM pVM);
CPUMDECL(RTSEL)         CPUMGetHyperSS(PVM pVM);
#if 0 /* these are not correct. */
CPUMDECL(uint32_t)      CPUMGetHyperCR0(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperCR2(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperCR3(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperCR4(PVM pVM);
#endif
/** This register is only saved on fatal traps. */
CPUMDECL(uint32_t)      CPUMGetHyperEAX(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperEBX(PVM pVM);
/** This register is only saved on fatal traps. */
CPUMDECL(uint32_t)      CPUMGetHyperECX(PVM pVM);
/** This register is only saved on fatal traps. */
CPUMDECL(uint32_t)      CPUMGetHyperEDX(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperESI(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperEDI(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperEBP(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperESP(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperEFlags(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperEIP(PVM pVM);
CPUMDECL(uint32_t)      CPUMGetHyperIDTR(PVM pVM, uint16_t *pcbLimit);
CPUMDECL(uint32_t)      CPUMGetHyperGDTR(PVM pVM, uint16_t *pcbLimit);
CPUMDECL(RTSEL)         CPUMGetHyperLDTR(PVM pVM);
CPUMDECL(RTGCUINTREG)   CPUMGetHyperDR0(PVM pVM);
CPUMDECL(RTGCUINTREG)   CPUMGetHyperDR1(PVM pVM);
CPUMDECL(RTGCUINTREG)   CPUMGetHyperDR2(PVM pVM);
CPUMDECL(RTGCUINTREG)   CPUMGetHyperDR3(PVM pVM);
CPUMDECL(RTGCUINTREG)   CPUMGetHyperDR6(PVM pVM);
CPUMDECL(RTGCUINTREG)   CPUMGetHyperDR7(PVM pVM);
CPUMDECL(void)          CPUMGetHyperCtx(PVM pVM, PCPUMCTX pCtx);
/** @} */

/** @name Hypervisor Register Setters.
 * @{ */
CPUMDECL(void)          CPUMSetHyperGDTR(PVM pVM, uint32_t addr, uint16_t limit);
CPUMDECL(void)          CPUMSetHyperLDTR(PVM pVM, RTSEL SelLDTR);
CPUMDECL(void)          CPUMSetHyperIDTR(PVM pVM, uint32_t addr, uint16_t limit);
CPUMDECL(void)          CPUMSetHyperCR3(PVM pVM, uint32_t cr3);
CPUMDECL(void)          CPUMSetHyperTR(PVM pVM, RTSEL SelTR);
CPUMDECL(void)          CPUMSetHyperCS(PVM pVM, RTSEL SelCS);
CPUMDECL(void)          CPUMSetHyperDS(PVM pVM, RTSEL SelDS);
CPUMDECL(void)          CPUMSetHyperES(PVM pVM, RTSEL SelDS);
CPUMDECL(void)          CPUMSetHyperFS(PVM pVM, RTSEL SelDS);
CPUMDECL(void)          CPUMSetHyperGS(PVM pVM, RTSEL SelDS);
CPUMDECL(void)          CPUMSetHyperSS(PVM pVM, RTSEL SelSS);
CPUMDECL(void)          CPUMSetHyperESP(PVM pVM, uint32_t u32ESP);
CPUMDECL(int)           CPUMSetHyperEFlags(PVM pVM, uint32_t Efl);
CPUMDECL(void)          CPUMSetHyperEIP(PVM pVM, uint32_t u32EIP);
CPUMDECL(void)          CPUMSetHyperDR0(PVM pVM, RTGCUINTREG uDr0);
CPUMDECL(void)          CPUMSetHyperDR1(PVM pVM, RTGCUINTREG uDr1);
CPUMDECL(void)          CPUMSetHyperDR2(PVM pVM, RTGCUINTREG uDr2);
CPUMDECL(void)          CPUMSetHyperDR3(PVM pVM, RTGCUINTREG uDr3);
CPUMDECL(void)          CPUMSetHyperDR6(PVM pVM, RTGCUINTREG uDr6);
CPUMDECL(void)          CPUMSetHyperDR7(PVM pVM, RTGCUINTREG uDr7);
CPUMDECL(void)          CPUMSetHyperCtx(PVM pVM, const PCPUMCTX pCtx);
CPUMDECL(int)           CPUMRecalcHyperDRx(PVM pVM);
/** @} */

CPUMDECL(void) CPUMPushHyper(PVM pVM, uint32_t u32);

/**
 * Sets or resets an alternative hypervisor context core.
 *
 * This is called when we get a hypervisor trap set switch the context
 * core with the trap frame on the stack. It is called again to reset
 * back to the default context core when resuming hypervisor execution.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    Pointer to the alternative context core or NULL
 *                      to go back to the default context core.
 */
CPUMDECL(void) CPUMHyperSetCtxCore(PVM pVM, PCPUMCTXCORE pCtxCore);


/**
 * Queries the pointer to the internal CPUMCTX structure
 *
 * @returns VBox status code.
 * @param   pVM         Handle to the virtual machine.
 * @param   ppCtx       Receives the CPUMCTX pointer when successful.
 */
CPUMDECL(int) CPUMQueryGuestCtxPtr(PVM pVM, PCPUMCTX *ppCtx);

/**
 * Queries the pointer to the internal CPUMCTX structure for the hypervisor.
 *
 * @returns VBox status code.
 * @param   pVM         Handle to the virtual machine.
 * @param   ppCtx       Receives the hyper CPUMCTX pointer when successful.
 */
CPUMDECL(int) CPUMQueryHyperCtxPtr(PVM pVM, PCPUMCTX *ppCtx);


/**
 * Gets the pointer to the internal CPUMCTXCORE structure.
 * This is only for reading in order to save a few calls.
 *
 * @param   pVM         Handle to the virtual machine.
 */
CPUMDECL(PCCPUMCTXCORE) CPUMGetGuestCtxCore(PVM pVM);

/**
 * Gets the pointer to the internal CPUMCTXCORE structure for the hypervisor.
 * This is only for reading in order to save a few calls.
 *
 * @param   pVM         Handle to the virtual machine.
 */
CPUMDECL(PCCPUMCTXCORE) CPUMGetHyperCtxCore(PVM pVM);

/**
 * Sets the guest context core registers.
 *
 * @param   pVM         Handle to the virtual machine.
 * @param   pCtxCore    The new context core values.
 */
CPUMDECL(void) CPUMSetGuestCtxCore(PVM pVM, PCCPUMCTXCORE pCtxCore);


/**
 * Transforms the guest CPU state to raw-ring mode.
 *
 * This function will change the any of the cs and ss register with DPL=0 to DPL=1.
 *
 * @returns VBox status. (recompiler failure)
 * @param   pVM         VM handle.
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
CPUMDECL(int) CPUMRawEnter(PVM pVM, PCPUMCTXCORE pCtxCore);

/**
 * Transforms the guest CPU state from raw-ring mode to correct values.
 *
 * This function will change any selector registers with DPL=1 to DPL=0.
 *
 * @returns Adjusted rc.
 * @param   pVM         VM handle.
 * @param   rc          Raw mode return code
 * @param   pCtxCore    The context core (for trap usage).
 * @see     @ref pg_raw
 */
CPUMDECL(int) CPUMRawLeave(PVM pVM, PCPUMCTXCORE pCtxCore, int rc);

/**
 * Gets the EFLAGS while we're in raw-mode.
 *
 * @returns The eflags.
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 */
CPUMDECL(uint32_t) CPUMRawGetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore);

/**
 * Updates the EFLAGS while we're in raw-mode.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   eflags      The new EFLAGS value.
 */
CPUMDECL(void) CPUMRawSetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore, uint32_t eflags);

/**
 * Lazily sync in the FPU/XMM state
 *
 * This function will change any selector registers with DPL=1 to DPL=0.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
CPUMDECL(int) CPUMHandleLazyFPU(PVM pVM);


/**
 * Restore host FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
CPUMDECL(int) CPUMRestoreHostFPUState(PVM pVM);

/** @name Changed flags
 * These flags are used to keep track of which important register that
 * have been changed since last they were reset. The only one allowed
 * to clear them is REM!
 * @{
 */
#define CPUM_CHANGED_FPU_REM            RT_BIT(0)
#define CPUM_CHANGED_CR0                RT_BIT(1)
#define CPUM_CHANGED_CR4                RT_BIT(2)
#define CPUM_CHANGED_GLOBAL_TLB_FLUSH   RT_BIT(3)
#define CPUM_CHANGED_CR3                RT_BIT(4)
#define CPUM_CHANGED_GDTR               RT_BIT(5)
#define CPUM_CHANGED_IDTR               RT_BIT(6)
#define CPUM_CHANGED_LDTR               RT_BIT(7)
#define CPUM_CHANGED_TR                 RT_BIT(8)
#define CPUM_CHANGED_SYSENTER_MSR       RT_BIT(9)
#define CPUM_CHANGED_HIDDEN_SEL_REGS    RT_BIT(10)
/** @} */

/**
 * Gets and resets the changed flags (CPUM_CHANGED_*).
 *
 * @returns The changed flags.
 * @param   pVM     VM handle.
 */
CPUMDECL(unsigned) CPUMGetAndClearChangedFlagsREM(PVM pVM);

/**
 * Sets the specified changed flags (CPUM_CHANGED_*).
 *
 * @param   pVM     The VM handle.
 */
CPUMDECL(void) CPUMSetChangedFlags(PVM pVM, uint32_t fChangedFlags);

/**
 * Checks if the CPU supports the FXSAVE and FXRSTOR instruction.
 * @returns true if supported.
 * @returns false if not supported.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMSupportsFXSR(PVM pVM);

/**
 * Checks if the host OS uses the SYSENTER / SYSEXIT instructions.
 * @returns true if used.
 * @returns false if not used.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsHostUsingSysEnter(PVM pVM);

/**
 * Checks if the host OS uses the SYSCALL / SYSRET instructions.
 * @returns true if used.
 * @returns false if not used.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsHostUsingSysCall(PVM pVM);

/**
 * Checks if we activated the FPU/XMM state of the guest OS
 * @returns true if we did.
 * @returns false if not.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMIsGuestFPUStateActive(PVM pVM);

/**
 * Deactivate the FPU/XMM state of the guest OS
 * @param   pVM     The VM handle.
 */
CPUMDECL(void) CPUMDeactivateGuestFPUState(PVM pVM);


/**
 * Checks if the hidden selector registers are valid
 * @returns true if they are.
 * @returns false if not.
 * @param   pVM     The VM handle.
 */
CPUMDECL(bool) CPUMAreHiddenSelRegsValid(PVM pVM);

/**
 * Checks if the hidden selector registers are valid
 * @param   pVM     The VM handle.
 * @param   fValid  Valid or not
 */
CPUMDECL(void) CPUMSetHiddenSelRegsValid(PVM pVM, bool fValid);

/**
 * Get the current privilege level of the guest.
 *
 * @returns cpl
 * @param   pVM         VM Handle.
 * @param   pRegFrame   Trap register frame.
 */
CPUMDECL(uint32_t) CPUMGetGuestCPL(PVM pVM, PCPUMCTXCORE pCtxCore);

/**
 * CPU modes.
 */
typedef enum CPUMMODE
{
    /** The usual invalid zero entry. */
    CPUMMODE_INVALID = 0,
    /** Real mode. */
    CPUMMODE_REAL,
    /** Protected mode (32-bit). */
    CPUMMODE_PROTECTED,
    /** Long mode (64-bit). */
    CPUMMODE_LONG
} CPUMMODE;

/**
 * Gets the current guest CPU mode.
 *
 * If paging mode is what you need, check out PGMGetGuestMode().
 *
 * @returns The CPU mode.
 * @param   pVM         The VM handle.
 */
CPUMDECL(CPUMMODE) CPUMGetGuestMode(PVM pVM);


#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

/**
 * Initializes the CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR3DECL(int) CPUMR3Init(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The CPUM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
CPUMR3DECL(void) CPUMR3Relocate(PVM pVM);

/**
 * Terminates the CPUM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR3DECL(int) CPUMR3Term(PVM pVM);

/**
 * Resets the CPU.
 *
 * @param   pVM         The VM handle.
 */
CPUMR3DECL(void) CPUMR3Reset(PVM pVM);

/**
 * Queries the pointer to the internal CPUMCTX structure
 *
 * @returns VBox status code.
 * @param   pVM         Handle to the virtual machine.
 * @param   ppCtx       Receives the CPUMCTX GC pointer when successful.
 */
CPUMR3DECL(int) CPUMR3QueryGuestCtxGCPtr(PVM pVM, GCPTRTYPE(PCPUMCTX) *ppCtx);


#ifdef DEBUG
/**
 * Debug helper - Saves guest context on raw mode entry (for fatal dump)
 *
 * @internal
 */
CPUMR3DECL(void) CPUMR3SaveEntryCtx(PVM pVM);
#endif

/**
 * API for controlling a few of the CPU features found in CR4.
 *
 * Currently only X86_CR4_TSD is accepted as input.
 *
 * @returns VBox status code.
 *
 * @param   pVM     The VM handle.
 * @param   fOr     The CR4 OR mask.
 * @param   fAnd    The CR4 AND mask.
 */
CPUMR3DECL(int) CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd);

/** @} */
#endif

#ifdef IN_GC
/** @defgroup grp_cpum_gc    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

/**
 * Calls a guest trap/interrupt handler directly
 * Assumes a trap stack frame has already been setup on the guest's stack!
 *
 * @param   pRegFrame   Original trap/interrupt context
 * @param   selCS       Code selector of handler
 * @param   pHandler    GC virtual address of handler
 * @param   eflags      Callee's EFLAGS
 * @param   selSS       Stack selector for handler
 * @param   pEsp        Stack address for handler
 *
 * This function does not return!
 *
 */
CPUMGCDECL(void) CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTGCPTR pHandler, uint32_t eflags, uint32_t selSS, RTGCPTR pEsp);

/**
 * Performs an iret to V86 code
 * Assumes a trap stack frame has already been setup on the guest's stack!
 *
 * @param   pRegFrame   Original trap/interrupt context
 *
 * This function does not return!
 */
CPUMGCDECL(void) CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);

/** @} */
#endif

#ifdef IN_RING0
/** @defgroup grp_cpum_r0    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

/**
 * Does Ring-0 CPUM initialization.
 *
 * This is mainly to check that the Host CPU mode is compatible
 * with VBox.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR0DECL(int) CPUMR0Init(PVM pVM);

/** @} */
#endif

/** @} */
__END_DECLS


#endif





