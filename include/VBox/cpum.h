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

/** @defgroup grp_cpum      The CPU Monitor / Manager API
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

/* Guest MSR state. */
typedef union CPUMCTXMSR
{
    struct
    {
        uint64_t        tscAux;         /* MSR_K8_TSC_AUX */
    } msr;
    uint64_t    au64[64];
} CPUMCTXMSR;
/** Pointer to the guest MSR state. */
typedef CPUMCTXMSR *PCPUMCTXMSR;
/** Pointer to the const guest MSR state. */
typedef const CPUMCTXMSR *PCCPUMCTXMSR;


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
    /** The x2APIC  feature bit. (Std) */
    CPUMCPUIDFEATURE_X2APIC,
    /** The RDTSCP feature bit. (Ext) */
    CPUMCPUIDFEATURE_RDTSCP,
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
VMMDECL(void)       CPUMGetGuestGDTR(PVMCPU pVCpu, PVBOXGDTR pGDTR);
VMMDECL(RTGCPTR)    CPUMGetGuestIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)      CPUMGetGuestTR(PVMCPU pVCpu, PCPUMSELREGHID pHidden);
VMMDECL(RTSEL)      CPUMGetGuestLDTR(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR4(PVMCPU pVCpu);
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
VMMDECL(void)       CPUMGetGuestCpuId(PVM pVM, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdStdMax(PVM pVM);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdExtMax(PVM pVM);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdCentaurMax(PVM pVM);
VMMDECL(uint64_t)   CPUMGetGuestEFER(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestMsr(PVMCPU pVCpu, unsigned idMsr);
VMMDECL(void)       CPUMSetGuestMsr(PVMCPU pVCpu, unsigned idMsr, uint64_t valMsr);
/** @} */

/** @name Guest Register Setters.
 * @{ */
VMMDECL(int)        CPUMSetGuestGDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(int)        CPUMSetGuestIDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
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
VMMDECL(CPUMCPUVENDOR) CPUMGetCPUVendor(PVM pVM);

/**
 * Tests if the guest is running in real mode or not.
 *
 * @returns true if in real mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInRealMode(PVMCPU pVCpu)
{
    return !(CPUMGetGuestCR0(pVCpu) & X86_CR0_PE);
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
DECLINLINE(bool) CPUMIsGuestInProtectedMode(PVMCPU pVCpu)
{
    return !!(CPUMGetGuestCR0(pVCpu) & X86_CR0_PE);
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInPagedProtectedMode(PVMCPU pVCpu)
{
    return (CPUMGetGuestCR0(pVCpu) & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG);
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInPagedProtectedModeEx(PCPUMCTX pCtx)
{
    return (pCtx->cr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG);
}

/**
 * Tests if the guest is running in long mode or not.
 *
 * @returns true if in long mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInLongMode(PVMCPU pVCpu)
{
    return (CPUMGetGuestEFER(pVCpu) & MSR_K6_EFER_LMA) == MSR_K6_EFER_LMA;
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
DECLINLINE(bool) CPUMIsGuestIn64BitCode(PVMCPU pVCpu, PCCPUMCTXCORE pCtx)
{
    if (!CPUMIsGuestInLongMode(pVCpu))
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

/**
 * Tests if the guest is running in PAE mode or not.
 *
 * @returns true if in PAE mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool) CPUMIsGuestInPAEMode(PVMCPU pVCpu)
{
    return (    CPUMIsGuestInPagedProtectedMode(pVCpu)
            &&  (CPUMGetGuestCR4(pVCpu) & X86_CR4_PAE)
            &&  !CPUMIsGuestInLongMode(pVCpu));
}

/**
 * Tests if the guest is running in PAE mode or not.
 *
 * @returns true if in PAE mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestInPAEModeEx(PCPUMCTX pCtx)
{
    return (    CPUMIsGuestInPagedProtectedModeEx(pCtx)
            &&  (pCtx->cr4 & X86_CR4_PAE)
            &&  !CPUMIsGuestInLongModeEx(pCtx));
}

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
VMMDECL(void)           CPUMHyperSetCtxCore(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
VMMDECL(int)            CPUMQueryHyperCtxPtr(PVMCPU pVCpu, PCPUMCTX *ppCtx);
VMMDECL(PCCPUMCTXCORE)  CPUMGetHyperCtxCore(PVMCPU pVCpu);
VMMDECL(PCPUMCTX)       CPUMQueryGuestCtxPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetGuestCtxCore(PVMCPU pVCpu);
VMMDECL(void)           CPUMSetGuestCtxCore(PVMCPU pVCpu, PCCPUMCTXCORE pCtxCore);
VMMDECL(int)            CPUMRawEnter(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
VMMDECL(int)            CPUMRawLeave(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, int rc);
VMMDECL(uint32_t)       CPUMRawGetEFlags(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
VMMDECL(void)           CPUMRawSetEFlags(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, uint32_t eflags);
VMMDECL(int)            CPUMHandleLazyFPU(PVMCPU pVCpu);

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

VMMDECL(unsigned)       CPUMGetAndClearChangedFlagsREM(PVMCPU pVCpu);
VMMDECL(void)           CPUMSetChangedFlags(PVMCPU pVCpu, uint32_t fChangedFlags);
VMMDECL(bool)           CPUMSupportsFXSR(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysEnter(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysCall(PVM pVM);
VMMDECL(bool)           CPUMIsGuestFPUStateActive(PVMCPU pVCPU);
VMMDECL(void)           CPUMDeactivateGuestFPUState(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestDebugStateActive(PVMCPU pVCpu);
VMMDECL(void)           CPUMDeactivateGuestDebugState(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestCPL(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
VMMDECL(bool)           CPUMAreHiddenSelRegsValid(PVM pVM);

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

VMMDECL(CPUMMODE)  CPUMGetGuestMode(PVMCPU pVCpu);


#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

VMMR3DECL(int)          CPUMR3Init(PVM pVM);
VMMR3DECL(int)          CPUMR3InitCPU(PVM pVM);
VMMR3DECL(void)         CPUMR3Relocate(PVM pVM);
VMMR3DECL(int)          CPUMR3Term(PVM pVM);
VMMR3DECL(int)          CPUMR3TermCPU(PVM pVM);
VMMR3DECL(void)         CPUMR3Reset(PVM pVM);
# ifdef DEBUG
VMMR3DECL(void)         CPUMR3SaveEntryCtx(PVM pVM);
# endif
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
DECLASM(void)           CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTRCPTR pHandler, uint32_t eflags, uint32_t selSS, RTRCPTR pEsp);
VMMRCDECL(void)         CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);

/** @} */
#endif /* IN_RC */

#ifdef IN_RING0
/** @defgroup grp_cpum_r0    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */
VMMR0DECL(int)          CPUMR0Init(PVM pVM);
VMMR0DECL(int)          CPUMR0LoadGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          CPUMR0SaveGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          CPUMR0SaveGuestDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6);
VMMR0DECL(int)          CPUMR0LoadGuestDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6);

/** @} */
#endif /* IN_RING0 */

/** @} */
__END_DECLS


#endif

