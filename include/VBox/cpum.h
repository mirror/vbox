/** @file
 * CPUM - CPU Monitor(/ Manager).
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
typedef struct CPUMSELREGHID
{
    /** Base register.
     *
     *  Long mode remarks:
     *  - Unused in long mode for CS, DS, ES, SS
     *  - 32 bits for FS & GS; FS(GS)_BASE msr used for the base address
     *  - 64 bits for TR & LDTR
     */
    uint64_t    u64Base;
    /** Limit (expanded). */
    uint32_t    u32Limit;
    /** Flags.
     * This is the high 32-bit word of the descriptor entry.
     * Only the flags, dpl and type are used. */
    X86DESCATTR Attr;
} CPUMSELREGHID;


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
        uint16_t        di;
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint16_t        si;
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint16_t        bp;
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint16_t        ax;
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint16_t        bx;
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint16_t        dx;
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint16_t        cx;
        uint32_t        ecx;
        uint64_t        rcx;
    };
    union
    {
        uint16_t        sp;
        uint32_t        esp;
        uint64_t        rsp;
    };
    /* Note: lss esp, [] in the switcher needs some space, so we reserve it here instead of relying on the exact esp & ss layout as before. */
    uint32_t            lss_esp;
    RTSEL               ss;
    RTSEL               ssPadding;

    RTSEL               gs;
    RTSEL               gsPadding;
    RTSEL               fs;
    RTSEL               fsPadding;
    RTSEL               es;
    RTSEL               esPadding;
    RTSEL               ds;
    RTSEL               dsPadding;
    RTSEL               cs;
    RTSEL               csPadding[3];  /* 3 words to force 8 byte alignment for the remainder */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint16_t        ip;
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
        uint16_t        di;
        uint32_t        edi;
        uint64_t        rdi;
    };
    union
    {
        uint16_t        si;
        uint32_t        esi;
        uint64_t        rsi;
    };
    union
    {
        uint16_t        bp;
        uint32_t        ebp;
        uint64_t        rbp;
    };
    union
    {
        uint16_t        ax;
        uint32_t        eax;
        uint64_t        rax;
    };
    union
    {
        uint16_t        bx;
        uint32_t        ebx;
        uint64_t        rbx;
    };
    union
    {
        uint16_t        dx;
        uint32_t        edx;
        uint64_t        rdx;
    };
    union
    {
        uint16_t        cx;
        uint32_t        ecx;
        uint64_t        rcx;
    };
    union
    {
        uint16_t        sp;
        uint32_t        esp;
        uint64_t        rsp;
    };
    /* Note: lss esp, [] in the switcher needs some space, so we reserve it here instead of relying on the exact esp & ss layout as before (prevented us from using a union with rsp). */
    uint32_t            lss_esp;
    RTSEL               ss;
    RTSEL               ssPadding;

    RTSEL               gs;
    RTSEL               gsPadding;
    RTSEL               fs;
    RTSEL               fsPadding;
    RTSEL               es;
    RTSEL               esPadding;
    RTSEL               ds;
    RTSEL               dsPadding;
    RTSEL               cs;
    RTSEL               csPadding[3];  /* 3 words to force 8 byte alignment for the remainder */

    union
    {
        X86EFLAGS       eflags;
        X86RFLAGS       rflags;
    };
    union
    {
        uint16_t        ip;
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
    /** @} */

    /** Debug registers.
     * @remarks DR4 and DR5 should not be used since they are aliases for
     *          DR6 and DR7 respectively on both AMD and Intel CPUs.
     * @remarks DR8-15 are currently not supported by AMD or Intel, so
     *          neither do we.
     * @{ */
    uint64_t        dr[8];
    /** @} */

    /** Global Descriptor Table register. */
    VBOXGDTR        gdtr;
    uint16_t        gdtrPadding;
    /** Interrupt Descriptor Table register. */
    VBOXIDTR        idtr;
    uint16_t        idtrPadding;
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

    /** System MSRs.
     * @{ */
    uint64_t        msrEFER;
    uint64_t        msrSTAR;        /* legacy syscall eip, cs & ss */
    uint64_t        msrPAT;
    uint64_t        msrLSTAR;       /* 64 bits mode syscall rip */
    uint64_t        msrCSTAR;       /* compatibility mode syscall rip */
    uint64_t        msrSFMASK;      /* syscall flag mask */
    uint64_t        msrKERNELGSBASE;/* swapgs exchange value */
    /** @} */

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID   ldtrHid;
    CPUMSELREGHID   trHid;
    /** @} */

#if 0
    /*& Padding to align the size on a 64 byte boundrary. */
    uint32_t        padding[6];
#endif
} CPUMCTX;
#pragma pack()

/**
 * Gets the CPUMCTXCORE part of a CPUMCTX.
 */
#define CPUMCTX2CORE(pCtx) ((PCPUMCTXCORE)(void *)&(pCtx)->edi)


/**
 * Selector hidden registers, for version 1.6 saved state.
 */
typedef struct CPUMSELREGHID_VER1_6
{
    /** Base register. */
    uint32_t    u32Base;
    /** Limit (expanded). */
    uint32_t    u32Limit;
    /** Flags.
     * This is the high 32-bit word of the descriptor entry.
     * Only the flags, dpl and type are used. */
    X86DESCATTR Attr;
} CPUMSELREGHID_VER1_6;

/**
 * CPU context, for version 1.6 saved state.
 * @remarks PATM uses this, which is why it has to be here.
 */
#pragma pack(1)
typedef struct CPUMCTX_VER1_6
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
    uint64_t        rsp_notused;

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
    CPUMSELREGHID_VER1_6   esHid;
    CPUMSELREGHID_VER1_6   csHid;
    CPUMSELREGHID_VER1_6   ssHid;
    CPUMSELREGHID_VER1_6   dsHid;
    CPUMSELREGHID_VER1_6   fsHid;
    CPUMSELREGHID_VER1_6   gsHid;
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
    VBOXGDTR_VER1_6 gdtr;
    uint16_t        gdtrPadding;
    uint32_t        gdtrPadding64;/** @todo fix this hack */
    /** Interrupt Descriptor Table register. */
    VBOXIDTR_VER1_6 idtr;
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

    /** System MSRs.
     * @{ */
    uint64_t        msrEFER;
    uint64_t        msrSTAR;
    uint64_t        msrPAT;
    uint64_t        msrLSTAR;
    uint64_t        msrCSTAR;
    uint64_t        msrSFMASK;
    uint64_t        msrFSBASE;
    uint64_t        msrGSBASE;
    uint64_t        msrKERNELGSBASE;
    /** @} */

    /** Hidden selector registers.
     * @{ */
    CPUMSELREGHID_VER1_6   ldtrHid;
    CPUMSELREGHID_VER1_6   trHid;
    /** @} */

    /* padding to get 32byte aligned size */
    uint32_t        padding[2];
} CPUMCTX_VER1_6;
#pragma pack()


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
    /** 32bit hackishness. */
    CPUMCPUVENDOR_32BIT_HACK = 0x7fffffff
} CPUMCPUVENDOR;


/** @name Guest Register Getters.
 * @{ */
CPUMDECL(void)      CPUMGetGuestGDTR(PVM pVM, PVBOXGDTR pGDTR);
CPUMDECL(RTGCPTR)   CPUMGetGuestIDTR(PVM pVM, uint16_t *pcbLimit);
CPUMDECL(RTSEL)     CPUMGetGuestTR(PVM pVM);
CPUMDECL(RTSEL)     CPUMGetGuestLDTR(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestCR0(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestCR2(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestCR3(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestCR4(PVM pVM);
CPUMDECL(int)       CPUMGetGuestCRx(PVM pVM, unsigned iReg, uint64_t *pValue);
CPUMDECL(uint32_t)  CPUMGetGuestEFlags(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestEIP(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestRIP(PVM pVM);
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
CPUMDECL(uint64_t)  CPUMGetGuestDR0(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestDR1(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestDR2(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestDR3(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestDR6(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestDR7(PVM pVM);
CPUMDECL(int)       CPUMGetGuestDRx(PVM pVM, uint32_t iReg, uint64_t *pValue);
CPUMDECL(void)      CPUMGetGuestCpuId(PVM pVM, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdStdGCPtr(PVM pVM);
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdExtGCPtr(PVM pVM);
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdCentaurGCPtr(PVM pVM);
CPUMDECL(RCPTRTYPE(PCCPUMCPUID)) CPUMGetGuestCpuIdDefGCPtr(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCpuIdStdMax(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCpuIdExtMax(PVM pVM);
CPUMDECL(uint32_t)  CPUMGetGuestCpuIdCentaurMax(PVM pVM);
CPUMDECL(CPUMSELREGHID *) CPUMGetGuestTRHid(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestEFER(PVM pVM);
CPUMDECL(uint64_t)  CPUMGetGuestMsr(PVM pVM, unsigned idMsr);
/** @} */

/** @name Guest Register Setters.
 * @{ */
CPUMDECL(int)       CPUMSetGuestGDTR(PVM pVM, uint32_t addr, uint16_t limit);
CPUMDECL(int)       CPUMSetGuestIDTR(PVM pVM, uint32_t addr, uint16_t limit);
CPUMDECL(int)       CPUMSetGuestTR(PVM pVM, uint16_t tr);
CPUMDECL(int)       CPUMSetGuestLDTR(PVM pVM, uint16_t ldtr);
CPUMDECL(int)       CPUMSetGuestCR0(PVM pVM, uint64_t cr0);
CPUMDECL(int)       CPUMSetGuestCR2(PVM pVM, uint64_t cr2);
CPUMDECL(int)       CPUMSetGuestCR3(PVM pVM, uint64_t cr3);
CPUMDECL(int)       CPUMSetGuestCR4(PVM pVM, uint64_t cr4);
CPUMDECL(int)       CPUMSetGuestDR0(PVM pVM, uint64_t uDr0);
CPUMDECL(int)       CPUMSetGuestDR1(PVM pVM, uint64_t uDr1);
CPUMDECL(int)       CPUMSetGuestDR2(PVM pVM, uint64_t uDr2);
CPUMDECL(int)       CPUMSetGuestDR3(PVM pVM, uint64_t uDr3);
CPUMDECL(int)       CPUMSetGuestDR6(PVM pVM, uint64_t uDr6);
CPUMDECL(int)       CPUMSetGuestDR7(PVM pVM, uint64_t uDr7);
CPUMDECL(int)       CPUMSetGuestDRx(PVM pVM, uint32_t iReg, uint64_t Value);
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
CPUMDECL(void)      CPUMSetGuestEFER(PVM pVM, uint64_t val);
CPUMDECL(void)      CPUMSetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
CPUMDECL(void)      CPUMClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
CPUMDECL(bool)      CPUMGetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
CPUMDECL(void)      CPUMSetGuestCtx(PVM pVM, const PCPUMCTX pCtx);
/** @} */

/** @name Misc Guest Predicate Functions.
 * @{  */


CPUMDECL(bool)      CPUMIsGuestIn16BitCode(PVM pVM);
CPUMDECL(bool)      CPUMIsGuestIn32BitCode(PVM pVM);
CPUMDECL(CPUMCPUVENDOR) CPUMGetCPUVendor(PVM pVM);

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
 * Tests if the guest is running in real mode or not.
 *
 * @returns true if in real mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestInRealModeEx(PCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE);
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
 * Tests if the guest is running in long mode or not.
 *
 * @returns true if in long mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInLongMode(PVM pVM)
{
    return (CPUMGetGuestEFER(pVM) & MSR_K6_EFER_LMA) == MSR_K6_EFER_LMA;
}

/**
 * Tests if the guest is running in long mode or not.
 *
 * @returns true if in long mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestInLongModeEx(PCPUMCTX pCtx)
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
DECLINLINE(bool) CPUMIsGuestIn64BitCode(PVM pVM, PCCPUMCTXCORE pCtx)
{
    if (!CPUMIsGuestInLongMode(pVM))
        return false;

    return pCtx->csHid.Attr.n.u1Long;
}

/**
 * Tests if the guest is running in 64 bits mode or not.
 *
 * @returns true if in 64 bits protected mode, otherwise false.
 * @param   pVM     The VM handle.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestIn64BitCodeEx(PCCPUMCTX pCtx)
{
    if (!(pCtx->msrEFER & MSR_K6_EFER_LMA))
        return false;

    return pCtx->csHid.Attr.n.u1Long;
}

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
CPUMDECL(uint64_t)      CPUMGetHyperRIP(PVM pVM);
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

CPUMDECL(void)      CPUMPushHyper(PVM pVM, uint32_t u32);
CPUMDECL(void)      CPUMHyperSetCtxCore(PVM pVM, PCPUMCTXCORE pCtxCore);
CPUMDECL(int)       CPUMQueryGuestCtxPtr(PVM pVM, PCPUMCTX *ppCtx);
CPUMDECL(int)       CPUMQueryHyperCtxPtr(PVM pVM, PCPUMCTX *ppCtx);
CPUMDECL(PCCPUMCTXCORE) CPUMGetGuestCtxCore(PVM pVM);
CPUMDECL(PCCPUMCTXCORE) CPUMGetHyperCtxCore(PVM pVM);
CPUMDECL(void)      CPUMSetGuestCtxCore(PVM pVM, PCCPUMCTXCORE pCtxCore);
CPUMDECL(int)       CPUMRawEnter(PVM pVM, PCPUMCTXCORE pCtxCore);
CPUMDECL(int)       CPUMRawLeave(PVM pVM, PCPUMCTXCORE pCtxCore, int rc);
CPUMDECL(uint32_t)  CPUMRawGetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore);
CPUMDECL(void)      CPUMRawSetEFlags(PVM pVM, PCPUMCTXCORE pCtxCore, uint32_t eflags);
CPUMDECL(int)       CPUMHandleLazyFPU(PVM pVM);
CPUMDECL(int)       CPUMRestoreHostFPUState(PVM pVM);

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
#define CPUM_CHANGED_CPUID              RT_BIT(11)
#define CPUM_CHANGED_ALL                (CPUM_CHANGED_FPU_REM|CPUM_CHANGED_CR0|CPUM_CHANGED_CR3|CPUM_CHANGED_CR4|CPUM_CHANGED_GDTR|CPUM_CHANGED_IDTR|CPUM_CHANGED_LDTR|CPUM_CHANGED_TR|CPUM_CHANGED_SYSENTER_MSR|CPUM_CHANGED_HIDDEN_SEL_REGS|CPUM_CHANGED_CPUID)
/** @} */

CPUMDECL(unsigned)  CPUMGetAndClearChangedFlagsREM(PVM pVM);
CPUMDECL(void)      CPUMSetChangedFlags(PVM pVM, uint32_t fChangedFlags);
CPUMDECL(bool)      CPUMSupportsFXSR(PVM pVM);
CPUMDECL(bool)      CPUMIsHostUsingSysEnter(PVM pVM);
CPUMDECL(bool)      CPUMIsHostUsingSysCall(PVM pVM);
CPUMDECL(bool)      CPUMIsGuestFPUStateActive(PVM pVM);
CPUMDECL(void)      CPUMDeactivateGuestFPUState(PVM pVM);
CPUMDECL(bool)      CPUMIsGuestDebugStateActive(PVM pVM);
CPUMDECL(void)      CPUMDeactivateGuestDebugState(PVM pVM);
CPUMDECL(bool)      CPUMAreHiddenSelRegsValid(PVM pVM);
CPUMDECL(void)      CPUMSetHiddenSelRegsValid(PVM pVM, bool fValid);
CPUMDECL(uint32_t)  CPUMGetGuestCPL(PVM pVM, PCPUMCTXCORE pCtxCore);

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

CPUMDECL(CPUMMODE)  CPUMGetGuestMode(PVM pVM);


#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

CPUMR3DECL(int)     CPUMR3Init(PVM pVM);
CPUMR3DECL(void)    CPUMR3Relocate(PVM pVM);
CPUMR3DECL(int)     CPUMR3Term(PVM pVM);
CPUMR3DECL(void)    CPUMR3Reset(PVM pVM);
CPUMR3DECL(int)     CPUMR3QueryGuestCtxGCPtr(PVM pVM, RCPTRTYPE(PCPUMCTX) *ppCtx);
# ifdef DEBUG
CPUMR3DECL(void)    CPUMR3SaveEntryCtx(PVM pVM);
# endif
CPUMR3DECL(int)     CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd);

/** @} */
#endif /* IN_RING3 */

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
 */
DECLASM(void)       CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTRCPTR pHandler, uint32_t eflags, uint32_t selSS, RTRCPTR pEsp);
CPUMGCDECL(void)    CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);

/** @} */
#endif /* IN_GC */

#ifdef IN_RING0
/** @defgroup grp_cpum_r0    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */
CPUMR0DECL(int) CPUMR0Init(PVM pVM);
CPUMR0DECL(int) CPUMR0LoadGuestFPU(PVM pVM, PCPUMCTX pCtx);
CPUMR0DECL(int) CPUMR0SaveGuestFPU(PVM pVM, PCPUMCTX pCtx);
CPUMR0DECL(int) CPUMR0SaveGuestDebugState(PVM pVM, PCPUMCTX pCtx, bool fDR6);
CPUMR0DECL(int) CPUMR0LoadGuestDebugState(PVM pVM, PCPUMCTX pCtx, bool fDR6);

/** @} */
#endif /* IN_RING0 */

/** @} */
__END_DECLS


#endif





