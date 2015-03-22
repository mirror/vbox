/* $Id$ */
/** @file
 * CPUM - Internal header file.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___CPUMInternal_h
#define ___CPUMInternal_h

#ifndef VBOX_FOR_DTRACE_LIB
# include <VBox/cdefs.h>
# include <VBox/types.h>
# include <VBox/vmm/stam.h>
# include <iprt/x86.h>
#else
# pragma D depends_on library x86.d
# pragma D depends_on library cpumctx.d

/* Some fudging. */
typedef uint32_t CPUMMICROARCH;
typedef uint32_t CPUMUNKNOWNCPUID;
typedef struct CPUMCPUIDLEAF *PCPUMCPUIDLEAF;
typedef struct CPUMMSRRANGE  *PCPUMMSRRANGE;
typedef uint64_t STAMCOUNTER;
#endif




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
 * (Don't forget to sync this with CPUMInternal.mac !)
 * @{ */
/** Used the FPU, SSE or such stuff. */
#define CPUM_USED_FPU                   RT_BIT(0)
/** Used the FPU, SSE or such stuff since last we were in REM.
 * REM syncing is clearing this, lazy FPU is setting it. */
#define CPUM_USED_FPU_SINCE_REM         RT_BIT(1)
/** The XMM state was manually restored. (AMD only) */
#define CPUM_USED_MANUAL_XMM_RESTORE    RT_BIT(2)

/** Host OS is using SYSENTER and we must NULL the CS. */
#define CPUM_USE_SYSENTER               RT_BIT(3)
/** Host OS is using SYSENTER and we must NULL the CS. */
#define CPUM_USE_SYSCALL                RT_BIT(4)

/** Debug registers are used by host and that DR7 and DR6 must be saved and
 *  disabled when switching to raw-mode. */
#define CPUM_USE_DEBUG_REGS_HOST        RT_BIT(5)
/** Records that we've saved the host DRx registers.
 * In ring-0 this means all (DR0-7), while in raw-mode context this means DR0-3
 * since DR6 and DR7 are covered by CPUM_USE_DEBUG_REGS_HOST. */
#define CPUM_USED_DEBUG_REGS_HOST       RT_BIT(6)
/** Set to indicate that we should save host DR0-7 and load the hypervisor debug
 * registers in the raw-mode world switchers. (See CPUMRecalcHyperDRx.) */
#define CPUM_USE_DEBUG_REGS_HYPER       RT_BIT(7)
/** Used in ring-0 to indicate that we have loaded the hypervisor debug
 * registers. */
#define CPUM_USED_DEBUG_REGS_HYPER      RT_BIT(8)
/** Used in ring-0 to indicate that we have loaded the guest debug
 * registers (DR0-3 and maybe DR6) for direct use by the guest.
 * DR7 (and AMD-V DR6) are handled via the VMCB. */
#define CPUM_USED_DEBUG_REGS_GUEST      RT_BIT(9)


/** Sync the FPU state on next entry (32->64 switcher only). */
#define CPUM_SYNC_FPU_STATE             RT_BIT(16)
/** Sync the debug state on next entry (32->64 switcher only). */
#define CPUM_SYNC_DEBUG_REGS_GUEST      RT_BIT(17)
/** Sync the debug state on next entry (32->64 switcher only).
 * Almost the same as CPUM_USE_DEBUG_REGS_HYPER in the raw-mode switchers. */
#define CPUM_SYNC_DEBUG_REGS_HYPER      RT_BIT(18)
/** Host CPU requires fxsave/fxrstor leaky bit handling. */
#define CPUM_USE_FFXSR_LEAKY            RT_BIT(19)
/** Set if the VM supports long-mode. */
#define CPUM_USE_SUPPORTS_LONGMODE      RT_BIT(20)
/** @} */

/* Sanity check. */
#ifndef VBOX_FOR_DTRACE_LIB
#if defined(VBOX_WITH_HYBRID_32BIT_KERNEL) && (HC_ARCH_BITS != 32 || R0_ARCH_BITS != 32)
# error "VBOX_WITH_HYBRID_32BIT_KERNEL is only for 32 bit builds."
#endif
#endif


/** @name CPUM Saved State Version.
 * @{ */
/** The current saved state version. */
#define CPUM_SAVED_STATE_VERSION                16
/** CPUID changes with explode forgetting to update the leaf count on
 * restore, resulting in garbage being saved restoring+saving old states). */
#define CPUM_SAVED_STATE_VERSION_BAD_CPUID_COUNT 15
/** The saved state version before the CPUIDs changes. */
#define CPUM_SAVED_STATE_VERSION_PUT_STRUCT     14
/** The saved state version before using SSMR3PutStruct. */
#define CPUM_SAVED_STATE_VERSION_MEM            13
/** The saved state version before introducing the MSR size field. */
#define CPUM_SAVED_STATE_VERSION_NO_MSR_SIZE    12
/** The saved state version of 3.2, 3.1 and 3.3 trunk before the hidden
 * selector register change (CPUM_CHANGED_HIDDEN_SEL_REGS_INVALID). */
#define CPUM_SAVED_STATE_VERSION_VER3_2         11
/** The saved state version of 3.0 and 3.1 trunk before the teleportation
 * changes. */
#define CPUM_SAVED_STATE_VERSION_VER3_0         10
/** The saved state version for the 2.1 trunk before the MSR changes. */
#define CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR   9
/** The saved state version of 2.0, used for backwards compatibility. */
#define CPUM_SAVED_STATE_VERSION_VER2_0         8
/** The saved state version of 1.6, used for backwards compatibility. */
#define CPUM_SAVED_STATE_VERSION_VER1_6         6
/** @} */



/**
 * CPU features and quirks.
 * This is mostly exploded CPUID info.
 */
typedef struct CPUMFEATURES
{
    /** The CPU vendor (CPUMCPUVENDOR). */
    uint8_t         enmCpuVendor;
    /** The CPU family. */
    uint8_t         uFamily;
    /** The CPU model. */
    uint8_t         uModel;
    /** The CPU stepping. */
    uint8_t         uStepping;
    /** The microarchitecture. */
#ifndef VBOX_FOR_DTRACE_LIB
    CPUMMICROARCH   enmMicroarch;
#else
    uint32_t        enmMicroarch;
#endif
    /** The maximum physical address with of the CPU. */
    uint8_t         cMaxPhysAddrWidth;
    /** Alignment padding. */
    uint8_t         abPadding[3];

    /** Supports MSRs. */
    uint32_t        fMsr : 1;
    /** Supports the page size extension (4/2 MB pages). */
    uint32_t        fPse : 1;
    /** Supports 36-bit page size extension (4 MB pages can map memory above
     *  4GB). */
    uint32_t        fPse36 : 1;
    /** Supports physical address extension (PAE). */
    uint32_t        fPae : 1;
    /** Page attribute table (PAT) support (page level cache control). */
    uint32_t        fPat : 1;
    /** Supports the FXSAVE and FXRSTOR instructions. */
    uint32_t        fFxSaveRstor : 1;
    /** Intel SYSENTER/SYSEXIT support */
    uint32_t        fSysEnter : 1;
    /** First generation APIC. */
    uint32_t        fApic : 1;
    /** Second generation APIC. */
    uint32_t        fX2Apic : 1;
    /** Hypervisor present. */
    uint32_t        fHypervisorPresent : 1;
    /** MWAIT & MONITOR instructions supported. */
    uint32_t        fMonitorMWait : 1;
    /** MWAIT Extensions present. */
    uint32_t        fMWaitExtensions : 1;

    /** AMD64: Supports long mode. */
    uint32_t        fLongMode : 1;
    /** AMD64: SYSCALL/SYSRET support. */
    uint32_t        fSysCall : 1;
    /** AMD64: No-execute page table bit. */
    uint32_t        fNoExecute : 1;
    /** AMD64: Supports LAHF & SAHF instructions in 64-bit mode. */
    uint32_t        fLahfSahf : 1;
    /** AMD64: Supports RDTSCP. */
    uint32_t        fRdTscP : 1;

    /** Indicates that FPU instruction and data pointers may leak.
     * This generally applies to recent AMD CPUs, where the FPU IP and DP pointer
     * is only saved and restored if an exception is pending. */
    uint32_t        fLeakyFxSR : 1;

    /** Alignment padding. */
    uint32_t        fPadding : 8;

    uint64_t        auPadding[2];
} CPUMFEATURES;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMFEATURES, 32);
#endif
/** Pointer to a CPU feature structure. */
typedef CPUMFEATURES *PCPUMFEATURES;
/** Pointer to a const CPU feature structure. */
typedef CPUMFEATURES const *PCCPUMFEATURES;


/**
 * CPU info
 */
typedef struct CPUMINFO
{
    /** The number of MSR ranges (CPUMMSRRANGE) in the array pointed to below. */
    uint32_t                    cMsrRanges;
    /** Mask applied to ECX before looking up the MSR for a RDMSR/WRMSR
     * instruction.  Older hardware has been observed to ignore higher bits. */
    uint32_t                    fMsrMask;

    /** The number of CPUID leaves (CPUMCPUIDLEAF) in the array pointed to below. */
    uint32_t                    cCpuIdLeaves;
    /** The index of the first extended CPUID leaf in the array.
     *  Set to cCpuIdLeaves if none present. */
    uint32_t                    iFirstExtCpuIdLeaf;
    /** Alignment padding. */
    uint32_t                    uPadding;
    /** How to handle unknown CPUID leaves. */
    CPUMUNKNOWNCPUID            enmUnknownCpuIdMethod;
    /** For use with CPUMUNKNOWNCPUID_DEFAULTS (DB & VM),
     * CPUMUNKNOWNCPUID_LAST_STD_LEAF (VM) and CPUMUNKNOWNCPUID_LAST_STD_LEAF_WITH_ECX (VM). */
    CPUMCPUID                   DefCpuId;

    /** Scalable bus frequency used for reporting other frequencies. */
    uint64_t                    uScalableBusFreq;

    /** Pointer to the MSR ranges (ring-0 pointer). */
    R0PTRTYPE(PCPUMMSRRANGE)    paMsrRangesR0;
    /** Pointer to the CPUID leaves (ring-0 pointer). */
    R0PTRTYPE(PCPUMCPUIDLEAF)   paCpuIdLeavesR0;

    /** Pointer to the MSR ranges (ring-3 pointer). */
    R3PTRTYPE(PCPUMMSRRANGE)    paMsrRangesR3;
    /** Pointer to the CPUID leaves (ring-3 pointer). */
    R3PTRTYPE(PCPUMCPUIDLEAF)   paCpuIdLeavesR3;

    /** Pointer to the MSR ranges (raw-mode context pointer). */
    RCPTRTYPE(PCPUMMSRRANGE)    paMsrRangesRC;
    /** Pointer to the CPUID leaves (raw-mode context pointer). */
    RCPTRTYPE(PCPUMCPUIDLEAF)   paCpuIdLeavesRC;
} CPUMINFO;
/** Pointer to a CPU info structure. */
typedef CPUMINFO *PCPUMINFO;
/** Pointer to a const CPU info structure. */
typedef CPUMINFO const *CPCPUMINFO;


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
    /*uint64_t        rax; - scratch*/
    uint64_t        rbx;
    /*uint64_t        rcx; - scratch*/
    /*uint64_t        rdx; - scratch*/
    uint64_t        rdi;
    uint64_t        rsi;
    uint64_t        rbp;
    uint64_t        rsp;
    /*uint64_t        r8; - scratch*/
    /*uint64_t        r9; - scratch*/
    uint64_t        r10;
    uint64_t        r11;
    uint64_t        r12;
    uint64_t        r13;
    uint64_t        r14;
    uint64_t        r15;
    /*uint64_t        rip; - scratch*/
    uint64_t        rflags;
#endif

#if HC_ARCH_BITS == 32
    /*uint32_t        eax; - scratch*/
    uint32_t        ebx;
    /*uint32_t        ecx; - scratch*/
    /*uint32_t        edx; - scratch*/
    uint32_t        edi;
    uint32_t        esi;
    uint32_t        ebp;
    X86EFLAGS       eflags;
    /*uint32_t        eip; - scratch*/
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
    /*uint32_t        cr2; - scratch*/
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
    /*uint64_t        cr2; - scratch*/
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
    /** Offset from CPUM to CPUMCPU for the first CPU. */
    uint32_t                offCPUMCPU0;

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

    /** CR4 mask */
    struct
    {
        uint32_t            AndMask; /**< @todo Move these to the per-CPU structure and fix the switchers. Saves a register! */
        uint32_t            OrMask;
    } CR4;

    /** The (more) portable CPUID level. */
    uint8_t                 u8PortableCpuIdLevel;
    /** Indicates that a state restore is pending.
     * This is used to verify load order dependencies (PGM). */
    bool                    fPendingRestore;
    uint8_t                 abPadding[HC_ARCH_BITS == 64 ? 6 : 2];

    /** The standard set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdPatmStd[6];
    /** The extended set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdPatmExt[10];
    /** The centaur set of CpuId leaves. */
    CPUMCPUID               aGuestCpuIdPatmCentaur[4];

#if HC_ARCH_BITS == 32
    uint8_t                 abPadding2[4];
#endif

    /** Guest CPU info. */
    CPUMINFO                GuestInfo;
    /** Guest CPU feature information. */
    CPUMFEATURES            GuestFeatures;
    /** Host CPU feature information. */
    CPUMFEATURES            HostFeatures;

    /** @name MSR statistics.
     * @{ */
    STAMCOUNTER             cMsrWrites;
    STAMCOUNTER             cMsrWritesToIgnoredBits;
    STAMCOUNTER             cMsrWritesRaiseGp;
    STAMCOUNTER             cMsrWritesUnknown;
    STAMCOUNTER             cMsrReads;
    STAMCOUNTER             cMsrReadsRaiseGp;
    STAMCOUNTER             cMsrReadsUnknown;
    /** @} */
} CPUM;
/** Pointer to the CPUM instance data residing in the shared VM structure. */
typedef CPUM *PCPUM;

/**
 * CPUM Data (part of VMCPU)
 */
typedef struct CPUMCPU
{
    /**
     * Guest context.
     * Aligned on a 64-byte boundary.
     */
    CPUMCTX                 Guest;

    /**
     * Guest context - misc MSRs
     * Aligned on a 64-byte boundary.
     */
    CPUMCTXMSRS             GuestMsrs;

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

    /** Offset from CPUM to CPUMCPU. */
    uint32_t                offCPUM;

    /** Temporary storage for the return code of the function called in the
     * 32-64 switcher. */
    uint32_t                u32RetCode;

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    /** The address of the APIC mapping, NULL if no APIC.
     * Call CPUMR0SetLApic to update this before doing a world switch. */
    RTHCPTR                 pvApicBase;
    /** Used by the world switcher code to store which vectors needs restoring on
     * the way back. */
    uint32_t                fApicDisVectors;
    /** Set if the CPU has the X2APIC mode enabled.
     * Call CPUMR0SetLApic to update this before doing a world switch. */
    bool                    fX2Apic;
#else
    uint8_t                 abPadding3[(HC_ARCH_BITS == 64 ? 8 : 4) + 4 + 1];
#endif

    /** Have we entered raw-mode? */
    bool                    fRawEntered;
    /** Have we entered the recompiler? */
    bool                    fRemEntered;

    /** Align the next member on a 64-bit boundrary. */
    uint8_t                 abPadding2[64 - 16 - (HC_ARCH_BITS == 64 ? 8 : 4) - 4 - 1 - 2];

    /** Saved host context.  Only valid while inside RC or HM contexts.
     * Must be aligned on a 64-byte boundary. */
    CPUMHOSTCTX             Host;
    /** Hypervisor context. Must be aligned on a 64-byte boundary. */
    CPUMCTX                 Hyper;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    uint8_t                 aMagic[56];
    uint64_t                uMagic;
#endif
} CPUMCPU;
/** Pointer to the CPUMCPU instance data residing in the shared VMCPU structure. */
typedef CPUMCPU *PCPUMCPU;

#ifndef VBOX_FOR_DTRACE_LIB
RT_C_DECLS_BEGIN

PCPUMCPUIDLEAF      cpumCpuIdGetLeaf(PVM pVM, uint32_t uLeaf);
PCPUMCPUIDLEAF      cpumCpuIdGetLeafEx(PVM pVM, uint32_t uLeaf, uint32_t uSubLeaf, bool *pfExactSubLeafHit);

#ifdef IN_RING3
int                 cpumR3DbgInit(PVM pVM);
int                 cpumR3CpuIdExplodeFeatures(PCCPUMCPUIDLEAF paLeaves, uint32_t cLeaves, PCPUMFEATURES pFeatures);
int                 cpumR3InitCpuIdAndMsrs(PVM pVM);
void                cpumR3SaveCpuId(PVM pVM, PSSMHANDLE pSSM);
int                 cpumR3LoadCpuId(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion);
DECLCALLBACK(void)  cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);

int                 cpumR3DbGetCpuInfo(const char *pszName, PCPUMINFO pInfo);
int                 cpumR3MsrRangesInsert(PVM pVM, PCPUMMSRRANGE *ppaMsrRanges, uint32_t *pcMsrRanges, PCCPUMMSRRANGE pNewRange);
int                 cpumR3MsrApplyFudge(PVM pVM);
int                 cpumR3MsrRegStats(PVM pVM);
int                 cpumR3MsrStrictInitChecks(void);
PCPUMMSRRANGE       cpumLookupMsrRange(PVM pVM, uint32_t idMsr);
#endif

#ifdef IN_RC
DECLASM(int)        cpumHandleLazyFPUAsm(PCPUMCPU pCPUM);
#endif

#ifdef IN_RING0
DECLASM(int)        cpumR0SaveHostRestoreGuestFPUState(PCPUMCPU pCPUM);
DECLASM(int)        cpumR0SaveGuestRestoreHostFPUState(PCPUMCPU pCPUM);
DECLASM(int)        cpumR0SaveHostFPUState(PCPUMCPU pCPUM);
DECLASM(int)        cpumR0RestoreHostFPUState(PCPUMCPU pCPUM);
DECLASM(void)       cpumR0LoadFPU(PCPUMCTX pCtx);
DECLASM(void)       cpumR0SaveFPU(PCPUMCTX pCtx);
DECLASM(void)       cpumR0LoadXMM(PCPUMCTX pCtx);
DECLASM(void)       cpumR0SaveXMM(PCPUMCTX pCtx);
DECLASM(void)       cpumR0SetFCW(uint16_t u16FCW);
DECLASM(uint16_t)   cpumR0GetFCW(void);
DECLASM(void)       cpumR0SetMXCSR(uint32_t u32MXCSR);
DECLASM(uint32_t)   cpumR0GetMXCSR(void);
DECLASM(void)       cpumR0LoadDRx(uint64_t const *pa4Regs);
DECLASM(void)       cpumR0SaveDRx(uint64_t *pa4Regs);
#endif

RT_C_DECLS_END
#endif /* !VBOX_FOR_DTRACE_LIB */

/** @} */

#endif

