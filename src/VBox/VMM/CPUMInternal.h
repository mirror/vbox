/* $Id$ */
/** @file
 * CPUM - Internal header file.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___CPUMInternal_h
#define ___CPUMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>



/** @defgroup grp_cpum_int   Internals
 * @ingroup grp_cpum
 * @internal
 * @{
 */

/** Flags and types for CPUM fault handlers
 * @{ */
/** Type: Load DS */
#define CPUM_HANDLER_DS                 1
/** Type: Load ES */
#define CPUM_HANDLER_ES                 2
/** Type: Load FS */
#define CPUM_HANDLER_FS                 3
/** Type: Load GS */
#define CPUM_HANDLER_GS                 4
/** Type: IRET */
#define CPUM_HANDLER_IRET               5
/** Type mask. */
#define CPUM_HANDLER_TYPEMASK           0xff
/** If set EBP points to the CPUMCTXCORE that's being used. */
#define CPUM_HANDLER_CTXCORE_IN_EBP     RT_BIT(31)
/** @} */


/** Use flags (CPUM::fUseFlags).
 * (Don't forget to sync this with CPUMInternal.mac!)
 * @{ */
/** Used the FPU, SSE or such stuff. */
#define CPUM_USED_FPU                   RT_BIT(0)
/** Used the FPU, SSE or such stuff since last we were in REM.
 * REM syncing is clearing this, lazy FPU is setting it. */
#define CPUM_USED_FPU_SINCE_REM         RT_BIT(1)
/** Host OS is using SYSENTER and we must NULL the CS. */
#define CPUM_USE_SYSENTER               RT_BIT(2)
/** Host OS is using SYSENTER and we must NULL the CS. */
#define CPUM_USE_SYSCALL                RT_BIT(3)
/** Debug registers are used by host and must be disabled. */
#define CPUM_USE_DEBUG_REGS_HOST        RT_BIT(4)
/** Enabled use of debug registers in guest context. */
#define CPUM_USE_DEBUG_REGS             RT_BIT(5)
/** The XMM state was manually restored. (AMD only) */
#define CPUM_MANUAL_XMM_RESTORE         RT_BIT(6)
/** Sync the FPU state on entry (32->64 switcher only). */
#define CPUM_SYNC_FPU_STATE             RT_BIT(7)
/** Sync the debug state on entry (32->64 switcher only). */
#define CPUM_SYNC_DEBUG_STATE           RT_BIT(8)
/** @} */

/* Sanity check. */
#if defined(VBOX_WITH_HYBRID_32BIT_KERNEL) && (HC_ARCH_BITS != 32 || R0_ARCH_BITS != 32)
# error "VBOX_WITH_HYBRID_32BIT_KERNEL is only for 32 bit builds."
#endif


/**
 * The saved host CPU state.
 *
 * @remark  The special VBOX_WITH_HYBRID_32BIT_KERNEL checks here are for the 10.4.x series
 *          of Mac OS X where the OS is essentially 32-bit but the cpu mode can be 64-bit.
 */
typedef struct CPUMHOSTCTX
{
    /** FPU state. (16-byte alignment)
     * @remark On x86, the format isn't necessarily X86FXSTATE (not important). */
    X86FXSTATE      fpu;

    /** General purpose register, selectors, flags and more
     * @{ */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    /** General purpose register ++
     * { */
    //uint64_t        rax; - scratch
    uint64_t        rbx;
    //uint64_t        rcx; - scratch
    //uint64_t        rdx; - scratch
    uint64_t        rdi;
    uint64_t        rsi;
    uint64_t        rbp;
    uint64_t        rsp;
    //uint64_t        r8; - scratch
    //uint64_t        r9; - scratch
    uint64_t        r10;
    uint64_t        r11;
    uint64_t        r12;
    uint64_t        r13;
    uint64_t        r14;
    uint64_t        r15;
    //uint64_t        rip; - scratch
    uint64_t        rflags;
#endif

#if HC_ARCH_BITS == 32
    //uint32_t        eax; - scratch
    uint32_t        ebx;
    //uint32_t        ecx; - scratch
    //uint32_t        edx; - scratch
    uint32_t        edi;
    uint32_t        esi;
    uint32_t        ebp;
    X86EFLAGS       eflags;
    //uint32_t        eip; - scratch
    /* lss pair! */
    uint32_t        esp;
#endif
    /** @} */

    /** Selector registers
     * @{ */
    RTSEL           ss;
    RTSEL           ssPadding;
    RTSEL           gs;
    RTSEL           gsPadding;
    RTSEL           fs;
    RTSEL           fsPadding;
    RTSEL           es;
    RTSEL           esPadding;
    RTSEL           ds;
    RTSEL           dsPadding;
    RTSEL           cs;
    RTSEL           csPadding;
    /** @} */

#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    /** Control registers.
     * @{ */
    uint32_t        cr0;
    //uint32_t        cr2; - scratch
    uint32_t        cr3;
    uint32_t        cr4;
    /** @} */

    /** Debug registers.
     * @{ */
    uint32_t        dr0;
    uint32_t        dr1;
    uint32_t        dr2;
    uint32_t        dr3;
    uint32_t        dr6;
    uint32_t        dr7;
    /** @} */

    /** Global Descriptor Table register. */
    X86XDTR32       gdtr;
    uint16_t        gdtrPadding;
    /** Interrupt Descriptor Table register. */
    X86XDTR32       idtr;
    uint16_t        idtrPadding;
    /** The task register. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register. */
    RTSEL           tr;
    RTSEL           trPadding;
    uint32_t        SysEnterPadding;

    /** The sysenter msr registers.
     * This member is not used by the hypervisor context. */
    CPUMSYSENTER    SysEnter;

    /** MSRs
     * @{ */
    uint64_t        efer;
    /** @} */

    /* padding to get 64byte aligned size */
    uint8_t         auPadding[16+32];

#elif HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)

    /** Control registers.
     * @{ */
    uint64_t        cr0;
    //uint64_t        cr2; - scratch
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
    uint64_t        dr6;
    uint64_t        dr7;
    /** @} */

    /** Global Descriptor Table register. */
    X86XDTR64       gdtr;
    uint16_t        gdtrPadding;
    /** Interrupt Descriptor Table register. */
    X86XDTR64       idtr;
    uint16_t        idtrPadding;
    /** The task register. */
    RTSEL           ldtr;
    RTSEL           ldtrPadding;
    /** The task register. */
    RTSEL           tr;
    RTSEL           trPadding;

    /** MSRs
     * @{ */
    CPUMSYSENTER    SysEnter;
    uint64_t        FSbase;
    uint64_t        GSbase;
    uint64_t        efer;
    /** @} */

    /* padding to get 32byte aligned size */
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    uint8_t         auPadding[16];
# else
    uint8_t         auPadding[8+32];
# endif

#else
# error HC_ARCH_BITS not defined
#endif
} CPUMHOSTCTX;
/** Pointer to the saved host CPU state. */
typedef CPUMHOSTCTX *PCPUMHOSTCTX;


/**
 * CPUM Data (part of VM)
 */
typedef struct CPUM
{
    /* Offset from CPUM to CPUMCPU for the first CPU. */
    uint32_t                ulOffCPUMCPU;

    /** Use flags.
     * These flags indicates which CPU features the host uses.
     */
    uint32_t                fHostUseFlags;

    /** Host CPU Features - ECX */
    struct
    {
        /** edx part */
        X86CPUIDFEATEDX     edx;
        /** ecx part */
        X86CPUIDFEATECX     ecx;
    } CPUFeatures;
    /** Host extended CPU features. */
    struct
    {
        /** edx part */
        uint32_t            edx;
        /** ecx part */
        uint32_t            ecx;
    } CPUFeaturesExt;

    /* CPU manufacturer. */
    CPUMCPUVENDOR           enmCPUVendor;

    /** CR4 mask */
    struct
    {
        uint32_t            AndMask;
        uint32_t            OrMask;
    } CR4;

    /** Have we entered rawmode? */
    bool                    fRawEntered;
    uint8_t                 abPadding[3 + (HC_ARCH_BITS == 64) * 4];

    /** The standard set of CpuId leafs. */
    CPUMCPUID               aGuestCpuIdStd[6];
    /** The extended set of CpuId leafs. */
    CPUMCPUID               aGuestCpuIdExt[10];
    /** The centaur set of CpuId leafs. */
    CPUMCPUID               aGuestCpuIdCentaur[4];
    /** The default set of CpuId leafs. */
    CPUMCPUID               GuestCpuIdDef;

    /** Align the next member, and thereby the structure, on a 64-byte boundrary. */
    uint8_t                 abPadding2[HC_ARCH_BITS == 32 ? 8 : 4];

    /**
     * Guest context on raw mode entry.
     * This a debug feature, see CPUMR3SaveEntryCtx.
     */
    CPUMCTX                 GuestEntry;
} CPUM;
/** Pointer to the CPUM instance data residing in the shared VM structure. */
typedef CPUM *PCPUM;

/**
 * CPUM Data (part of VMCPU)
 */
typedef struct CPUMCPU
{
    /**
     * Hypervisor context.
     * Aligned on a 64-byte boundrary.
     */
    CPUMCTX                 Hyper;

    /**
     * Saved host context. Only valid while inside GC.
     * Aligned on a 64-byte boundrary.
     */
    CPUMHOSTCTX             Host;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    uint8_t                 aMagic[56];
    uint64_t                uMagic;
#endif

    /**
     * Guest context.
     * Aligned on a 64-byte boundrary.
     */
    CPUMCTX                 Guest;

    /**
     * Guest context - misc MSRs
     * Aligned on a 64-byte boundrary.
     */
    CPUMCTXMSR              GuestMsr;

    /** Pointer to the current hypervisor core context - R3Ptr. */
    R3PTRTYPE(PCPUMCTXCORE) pHyperCoreR3;
    /** Pointer to the current hypervisor core context - R0Ptr. */
    R0PTRTYPE(PCPUMCTXCORE) pHyperCoreR0;
    /** Pointer to the current hypervisor core context - RCPtr. */
    RCPTRTYPE(PCPUMCTXCORE) pHyperCoreRC;

    /** Use flags.
     * These flags indicates both what is to be used and what has been used.
     */
    uint32_t                fUseFlags;

    /** Changed flags.
     * These flags indicates to REM (and others) which important guest
     * registers which has been changed since last time the flags were cleared.
     * See the CPUM_CHANGED_* defines for what we keep track of.
     */
    uint32_t                fChanged;

    /* Offset to CPUM. (subtract from the pointer to get to CPUM) */
    uint32_t                ulOffCPUM;

    /* Temporary storage for the return code of the function called in the 32-64 switcher. */
    uint32_t                u32RetCode;

    /** Align the next member, and thereby the structure, on a 64-byte boundrary. */
    uint8_t                 abPadding2[HC_ARCH_BITS == 32 ? 36 : 28];
} CPUMCPU, *PCPUMCPU;
/** Pointer to the CPUMCPU instance data residing in the shared VMCPU structure. */
typedef CPUMCPU *PCPUMCPU;

__BEGIN_DECLS

DECLASM(int)      cpumHandleLazyFPUAsm(PCPUMCPU pCPUM);

#ifdef IN_RING0
DECLASM(int)      cpumR0SaveHostRestoreGuestFPUState(PCPUMCPU pCPUM);
DECLASM(int)      cpumR0SaveGuestRestoreHostFPUState(PCPUMCPU pCPUM);
DECLASM(int)      cpumR0SaveHostFPUState(PCPUMCPU pCPUM);
DECLASM(int)      cpumR0RestoreHostFPUState(PCPUMCPU pCPUM);
DECLASM(void)     cpumR0LoadFPU(PCPUMCTX pCtx);
DECLASM(void)     cpumR0SaveFPU(PCPUMCTX pCtx);
DECLASM(void)     cpumR0LoadXMM(PCPUMCTX pCtx);
DECLASM(void)     cpumR0SaveXMM(PCPUMCTX pCtx);
DECLASM(void)     cpumR0SetFCW(uint16_t u16FCW);
DECLASM(uint16_t) cpumR0GetFCW(void);
DECLASM(void)     cpumR0SetMXCSR(uint32_t u32MXCSR);
DECLASM(uint32_t) cpumR0GetMXCSR(void);
DECLASM(void)     cpumR0LoadDRx(uint64_t const *pa4Regs);
DECLASM(void)     cpumR0SaveDRx(uint64_t *pa4Regs);
#endif

__END_DECLS

/** @} */

#endif

