/* $Id$ */
/** @file
 * HWACCM - Internal header file.
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

#ifndef ___HWACCMInternal_h
#define ___HWACCMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/dis.h>
#include <VBox/hwaccm.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <iprt/memobj.h>
#include <iprt/cpuset.h>
#include <iprt/mp.h>

#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL) || defined (VBOX_WITH_64_BITS_GUESTS)
/* Enable 64 bits guest support. */
# define VBOX_ENABLE_64_BITS_GUESTS
#endif

#define VMX_USE_CACHED_VMCS_ACCESSES
#define HWACCM_VMX_EMULATE_REALMODE
#define HWACCM_VTX_WITH_EPT
#define HWACCM_VTX_WITH_VPID

__BEGIN_DECLS


/** @defgroup grp_hwaccm_int       Internal
 * @ingroup grp_hwaccm
 * @internal
 * @{
 */


/** Maximum number of exit reason statistics counters. */
#define MAX_EXITREASON_STAT        0x100
#define MASK_EXITREASON_STAT       0xff

/** @name Changed flags
 * These flags are used to keep track of which important registers that
 * have been changed since last they were reset.
 * @{
 */
#define HWACCM_CHANGED_GUEST_FPU                RT_BIT(0)
#define HWACCM_CHANGED_GUEST_CR0                RT_BIT(1)
#define HWACCM_CHANGED_GUEST_CR3                RT_BIT(2)
#define HWACCM_CHANGED_GUEST_CR4                RT_BIT(3)
#define HWACCM_CHANGED_GUEST_GDTR               RT_BIT(4)
#define HWACCM_CHANGED_GUEST_IDTR               RT_BIT(5)
#define HWACCM_CHANGED_GUEST_LDTR               RT_BIT(6)
#define HWACCM_CHANGED_GUEST_TR                 RT_BIT(7)
#define HWACCM_CHANGED_GUEST_SYSENTER_MSR       RT_BIT(8)
#define HWACCM_CHANGED_GUEST_SEGMENT_REGS       RT_BIT(9)
#define HWACCM_CHANGED_GUEST_DEBUG              RT_BIT(10)
#define HWACCM_CHANGED_HOST_CONTEXT             RT_BIT(11)

#define HWACCM_CHANGED_ALL                  (   HWACCM_CHANGED_GUEST_SEGMENT_REGS \
                                            |   HWACCM_CHANGED_GUEST_CR0          \
                                            |   HWACCM_CHANGED_GUEST_CR3          \
                                            |   HWACCM_CHANGED_GUEST_CR4          \
                                            |   HWACCM_CHANGED_GUEST_GDTR         \
                                            |   HWACCM_CHANGED_GUEST_IDTR         \
                                            |   HWACCM_CHANGED_GUEST_LDTR         \
                                            |   HWACCM_CHANGED_GUEST_TR           \
                                            |   HWACCM_CHANGED_GUEST_SYSENTER_MSR \
                                            |   HWACCM_CHANGED_GUEST_FPU          \
                                            |   HWACCM_CHANGED_GUEST_DEBUG        \
                                            |   HWACCM_CHANGED_HOST_CONTEXT)

#define HWACCM_CHANGED_ALL_GUEST            (   HWACCM_CHANGED_GUEST_SEGMENT_REGS \
                                            |   HWACCM_CHANGED_GUEST_CR0          \
                                            |   HWACCM_CHANGED_GUEST_CR3          \
                                            |   HWACCM_CHANGED_GUEST_CR4          \
                                            |   HWACCM_CHANGED_GUEST_GDTR         \
                                            |   HWACCM_CHANGED_GUEST_IDTR         \
                                            |   HWACCM_CHANGED_GUEST_LDTR         \
                                            |   HWACCM_CHANGED_GUEST_TR           \
                                            |   HWACCM_CHANGED_GUEST_SYSENTER_MSR \
                                            |   HWACCM_CHANGED_GUEST_DEBUG        \
                                            |   HWACCM_CHANGED_GUEST_FPU)

/** @} */

/** @name Intercepted traps
 *  Traps that need to be intercepted so we can correctly dispatch them to the guest if required.
 *  Currently #NM and #PF only
 */
#ifdef VBOX_STRICT
#define HWACCM_VMX_TRAP_MASK                RT_BIT(X86_XCPT_DE) | RT_BIT(X86_XCPT_NM) | RT_BIT(X86_XCPT_PF) | RT_BIT(X86_XCPT_UD) | RT_BIT(X86_XCPT_NP) | RT_BIT(X86_XCPT_SS) | RT_BIT(X86_XCPT_GP) | RT_BIT(X86_XCPT_MF)
#define HWACCM_SVM_TRAP_MASK                HWACCM_VMX_TRAP_MASK
#else
#define HWACCM_VMX_TRAP_MASK                RT_BIT(X86_XCPT_NM) | RT_BIT(X86_XCPT_PF)
#define HWACCM_SVM_TRAP_MASK                RT_BIT(X86_XCPT_NM) | RT_BIT(X86_XCPT_PF)
#endif
/* All exceptions have to be intercept in emulated real-mode (minues NM & PF as they are always intercepted. */
#define HWACCM_VMX_TRAP_MASK_REALMODE       RT_BIT(X86_XCPT_DE) | RT_BIT(X86_XCPT_DB) | RT_BIT(X86_XCPT_NMI) | RT_BIT(X86_XCPT_BP) | RT_BIT(X86_XCPT_OF) | RT_BIT(X86_XCPT_BR) | RT_BIT(X86_XCPT_UD) | RT_BIT(X86_XCPT_DF) | RT_BIT(X86_XCPT_CO_SEG_OVERRUN) | RT_BIT(X86_XCPT_TS) | RT_BIT(X86_XCPT_NP) | RT_BIT(X86_XCPT_SS) | RT_BIT(X86_XCPT_GP) | RT_BIT(X86_XCPT_MF) | RT_BIT(X86_XCPT_AC) | RT_BIT(X86_XCPT_MC) | RT_BIT(X86_XCPT_XF)
/** @} */


/** Maxium resume loops allowed in ring 0 (safety precaution) */
#define HWACCM_MAX_RESUME_LOOPS             1024

/** Maximum number of page flushes we are willing to remember before considering a full TLB flush. */
#define HWACCM_MAX_TLB_SHOOTDOWN_PAGES      16

/** Size for the EPT identity page table (1024 4 MB pages to cover the entire address space). */
#define HWACCM_EPT_IDENTITY_PG_TABLE_SIZE   PAGE_SIZE
/** Size of the TSS structure + 2 pages for the IO bitmap + end byte. */
#define HWACCM_VTX_TSS_SIZE                 (sizeof(VBOXTSS) + 2*PAGE_SIZE + 1)
/** Total guest mapped memory needed. */
#define HWACCM_VTX_TOTAL_DEVHEAP_MEM        (HWACCM_EPT_IDENTITY_PG_TABLE_SIZE + HWACCM_VTX_TSS_SIZE)

/** HWACCM SSM version
 */
#define HWACCM_SSM_VERSION                  4
#define HWACCM_SSM_VERSION_2_0_X            3

/* Per-cpu information. (host) */
typedef struct
{
    RTCPUID             idCpu;

    RTR0MEMOBJ          pMemObj;
    /* Current ASID (AMD-V)/VPID (Intel) */
    uint32_t            uCurrentASID;
    /* TLB flush count */
    uint32_t            cTLBFlushes;

    /* Set the first time a cpu is used to make sure we start with a clean TLB. */
    bool                fFlushTLB;

    /** Configured for VT-x or AMD-V. */
    bool                fConfigured;

    /** In use by our code. (for power suspend) */
    volatile bool       fInUse;
} HWACCM_CPUINFO;
typedef HWACCM_CPUINFO *PHWACCM_CPUINFO;

/* VT-x capability qword. */
typedef union
{
    struct
    {
        uint32_t        disallowed0;
        uint32_t        allowed1;
    } n;
    uint64_t            u;
} VMX_CAPABILITY;

/**
 * Switcher function, HC to RC.
 *
 * @param   pVM         The VM handle.
 * @returns Return code indicating the action to take.
 */
typedef DECLASMTYPE(int) FNHWACCMSWITCHERHC(PVM pVM);
/** Pointer to switcher function. */
typedef FNHWACCMSWITCHERHC *PFNHWACCMSWITCHERHC;

/**
 * HWACCM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct HWACCM
{
    /** Set when we've initialized VMX or SVM. */
    bool                        fInitialized;

    /** Set when hardware acceleration is allowed. */
    bool                        fAllowed;

    /** Set if nested paging is enabled. */
    bool                        fNestedPaging;

    /** Set if nested paging is allowed. */
    bool                        fAllowNestedPaging;

    /** Set if we're supposed to inject an NMI. */
    bool                        fInjectNMI;

    /** Set if we can support 64-bit guests or not. */
    bool                        fAllow64BitGuests;

    /** Explicit alignment padding to make 32-bit gcc align u64RegisterMask
     *  naturally. */
    bool                        padding[2];

    /** And mask for copying register contents. */
    uint64_t                    u64RegisterMask;

    /** Maximum ASID allowed. */
    RTUINT                      uMaxASID;

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    /** 32 to 64 bits switcher entrypoint. */
    R0PTRTYPE(PFNHWACCMSWITCHERHC) pfnHost32ToGuest64R0;

    /* AMD-V 64 bits vmrun handler */
    RTRCPTR                     pfnSVMGCVMRun64;

    /* VT-x 64 bits vmlaunch handler */
    RTRCPTR                     pfnVMXGCStartVM64;

    /* RC handler to setup the 64 bits FPU state. */
    RTRCPTR                     pfnSaveGuestFPU64;

    /* RC handler to setup the 64 bits debug state. */
    RTRCPTR                     pfnSaveGuestDebug64;

    /* Test handler */
    RTRCPTR                     pfnTest64;

    RTRCPTR                     uAlignment[1];
#elif defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    uint32_t                    u32Alignment[1];
#endif

    struct
    {
        /** Set by the ring-0 driver to indicate VMX is supported by the CPU. */
        bool                        fSupported;

        /** Set when we've enabled VMX. */
        bool                        fEnabled;

        /** Set if VPID is supported. */
        bool                        fVPID;

        /** Set if VT-x VPID is allowed. */
        bool                        fAllowVPID;

        /** Virtual address of the TSS page used for real mode emulation. */
        R3PTRTYPE(PVBOXTSS)         pRealModeTSS;

        /** Virtual address of the identity page table used for real mode and protected mode without paging emulation in EPT mode. */
        R3PTRTYPE(PX86PD)           pNonPagingModeEPTPageTable;

        /** R0 memory object for the virtual APIC mmio cache. */
        RTR0MEMOBJ                  pMemObjAPIC;
        /** Physical address of the virtual APIC mmio cache. */
        RTHCPHYS                    pAPICPhys;
        /** Virtual address of the virtual APIC mmio cache. */
        R0PTRTYPE(uint8_t *)        pAPIC;

        /** R0 memory object for the MSR bitmap (1 page). */
        RTR0MEMOBJ                  pMemObjMSRBitmap;
        /** Physical address of the MSR bitmap (1 page). */
        RTHCPHYS                    pMSRBitmapPhys;
        /** Virtual address of the MSR bitmap (1 page). */
        R0PTRTYPE(uint8_t *)        pMSRBitmap;

        /** R0 memory object for the MSR entry load page (guest MSRs). */
        RTR0MEMOBJ                  pMemObjMSREntryLoad;
        /** Physical address of the MSR entry load page (guest MSRs). */
        RTHCPHYS                    pMSREntryLoadPhys;
        /** Virtual address of the MSR entry load page (guest MSRs). */
        R0PTRTYPE(uint8_t *)        pMSREntryLoad;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        RTR0MEMOBJ                  pMemObjScratch;
        RTHCPHYS                    pScratchPhys;
        R0PTRTYPE(uint8_t *)        pScratch;
#endif
        /** R0 memory object for the MSR exit store page (guest MSRs). */
        RTR0MEMOBJ                  pMemObjMSRExitStore;
        /** Physical address of the MSR exit store page (guest MSRs). */
        RTHCPHYS                    pMSRExitStorePhys;
        /** Virtual address of the MSR exit store page (guest MSRs). */
        R0PTRTYPE(uint8_t *)        pMSRExitStore;

        /** R0 memory object for the MSR exit load page (host MSRs). */
        RTR0MEMOBJ                  pMemObjMSRExitLoad;
        /** Physical address of the MSR exit load page (host MSRs). */
        RTHCPHYS                    pMSRExitLoadPhys;
        /** Virtual address of the MSR exit load page (host MSRs). */
        R0PTRTYPE(uint8_t *)        pMSRExitLoad;

        /** Ring 0 handlers for VT-x. */
        DECLR0CALLBACKMEMBER(void, pfnSetupTaggedTLB, (PVM pVM, PVMCPU pVCpu));

        /** Host CR4 value (set by ring-0 VMX init) */
        uint64_t                    hostCR4;

        /** VMX MSR values */
        struct
        {
            uint64_t                feature_ctrl;
            uint64_t                vmx_basic_info;
            VMX_CAPABILITY          vmx_pin_ctls;
            VMX_CAPABILITY          vmx_proc_ctls;
            VMX_CAPABILITY          vmx_proc_ctls2;
            VMX_CAPABILITY          vmx_exit;
            VMX_CAPABILITY          vmx_entry;
            uint64_t                vmx_misc;
            uint64_t                vmx_cr0_fixed0;
            uint64_t                vmx_cr0_fixed1;
            uint64_t                vmx_cr4_fixed0;
            uint64_t                vmx_cr4_fixed1;
            uint64_t                vmx_vmcs_enum;
            uint64_t                vmx_eptcaps;
        } msr;

        /** Flush types for invept & invvpid; they depend on capabilities. */
        VMX_FLUSH                   enmFlushPage;
        VMX_FLUSH                   enmFlushContext;
    } vmx;

    struct
    {
        /** Set by the ring-0 driver to indicate SVM is supported by the CPU. */
        bool                        fSupported;
        /** Set when we've enabled SVM. */
        bool                        fEnabled;
        /** Set if erratum 170 affects the AMD cpu. */
        bool                        fAlwaysFlushTLB;
        /** Explicit alignment padding to make 32-bit gcc align u64RegisterMask
         *  naturally. */
        bool                        padding[1];

        /** R0 memory object for the host VM control block (VMCB). */
        RTR0MEMOBJ                  pMemObjVMCBHost;
        /** Physical address of the host VM control block (VMCB). */
        RTHCPHYS                    pVMCBHostPhys;
        /** Virtual address of the host VM control block (VMCB). */
        R0PTRTYPE(void *)           pVMCBHost;

        /** R0 memory object for the IO bitmap (12kb). */
        RTR0MEMOBJ                  pMemObjIOBitmap;
        /** Physical address of the IO bitmap (12kb). */
        RTHCPHYS                    pIOBitmapPhys;
        /** Virtual address of the IO bitmap. */
        R0PTRTYPE(void *)           pIOBitmap;

        /** R0 memory object for the MSR bitmap (8kb). */
        RTR0MEMOBJ                  pMemObjMSRBitmap;
        /** Physical address of the MSR bitmap (8kb). */
        RTHCPHYS                    pMSRBitmapPhys;
        /** Virtual address of the MSR bitmap. */
        R0PTRTYPE(void *)           pMSRBitmap;

        /** SVM revision. */
        uint32_t                    u32Rev;

        /** SVM feature bits from cpuid 0x8000000a */
        uint32_t                    u32Features;
    } svm;

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    /** Saved error from detection */
    int32_t                 lLastError;

    /** HWACCMR0Init was run */
    bool                    fHWACCMR0Init;
} HWACCM;
/** Pointer to HWACCM VM instance data. */
typedef HWACCM *PHWACCM;

/* Maximum number of cached entries. */
#define VMCSCACHE_MAX_ENTRY                             128

/* Structure for storing read and write VMCS actions. */
typedef struct VMCSCACHE
{
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /* Magic marker for searching in crash dumps. */
    uint8_t         aMagic[16];
    uint64_t        uMagic;
    uint64_t        u64TimeEntry;
    uint64_t        u64TimeSwitch;
    uint64_t        cResume;
    uint64_t        interPD;
    uint64_t        pSwitcher;
    uint32_t        uPos;
    uint32_t        idCpu;
#endif
    /* CR2 is saved here for EPT syncing. */
    uint64_t        cr2;
    struct
    {
        uint32_t    cValidEntries;
        uint32_t    uAlignment;
        uint32_t    aField[VMCSCACHE_MAX_ENTRY];
        uint64_t    aFieldVal[VMCSCACHE_MAX_ENTRY];
    } Write;
    struct
    {
        uint32_t    cValidEntries;
        uint32_t    uAlignment;
        uint32_t    aField[VMCSCACHE_MAX_ENTRY];
        uint64_t    aFieldVal[VMCSCACHE_MAX_ENTRY];
    } Read;
#ifdef DEBUG
    struct
    {
        RTHCPHYS    pPageCpuPhys;
        RTHCPHYS    pVMCSPhys;
        RTGCPTR     pCache;
        RTGCPTR     pCtx;
    } TestIn;
    struct
    {
        RTHCPHYS    pVMCSPhys;
        RTGCPTR     pCache;
        RTGCPTR     pCtx;
        uint64_t    eflags;
        uint64_t    cr8;
    } TestOut;
    struct
    {
        uint64_t    param1;
        uint64_t    param2;
        uint64_t    param3;
        uint64_t    param4;
    } ScratchPad;
#endif
} VMCSCACHE;
/** Pointer to VMCSCACHE. */
typedef VMCSCACHE *PVMCSCACHE;

/**
 * HWACCM VMCPU Instance data.
 */
typedef struct HWACCMCPU
{
    /** Old style FPU reporting trap mask override performed (optimization) */
    bool                        fFPUOldStyleOverride;

    /** Set if we don't have to flush the TLB on VM entry. */
    bool                        fResumeVM;

    /** Set if we need to flush the TLB during the world switch. */
    bool                        fForceTLBFlush;

    /** Set when we're using VT-x or AMD-V at that moment. */
    bool                        fActive;

    /** HWACCM_CHANGED_* flags. */
    RTUINT                      fContextUseFlags;

    /* Id of the last cpu we were executing code on (NIL_RTCPUID for the first time) */
    RTCPUID                     idLastCpu;

    /* TLB flush count */
    RTUINT                      cTLBFlushes;

    /* Current ASID in use by the VM */
    RTUINT                      uCurrentASID;

    struct
    {
        /** R0 memory object for the VM control structure (VMCS). */
        RTR0MEMOBJ                  pMemObjVMCS;
        /** Physical address of the VM control structure (VMCS). */
        RTHCPHYS                    pVMCSPhys;
        /** Virtual address of the VM control structure (VMCS). */
        R0PTRTYPE(void *)           pVMCS;

        /** Ring 0 handlers for VT-x. */
        DECLR0CALLBACKMEMBER(int,  pfnStartVM,(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu));

        /** Current VMX_VMCS_CTRL_PROC_EXEC_CONTROLS. */
        uint64_t                    proc_ctls;

        /** Current CR0 mask. */
        uint64_t                    cr0_mask;
        /** Current CR4 mask. */
        uint64_t                    cr4_mask;

        /** Current EPTP. */
        RTHCPHYS                    GCPhysEPTP;

        /** VMCS cache. */
        VMCSCACHE                   VMCSCache;

        /** Real-mode emulation state. */
        struct
        {
            X86EFLAGS                   eflags;
            uint32_t                    fValid;
        } RealMode;

        struct
        {
            uint64_t                u64VMCSPhys;
            uint32_t                ulVMCSRevision;
            uint32_t                ulInstrError;
            uint32_t                ulExitReason;
            RTCPUID                 idEnteredCpu;
            RTCPUID                 idCurrentCpu;
            uint32_t                padding;
        } lasterror;

        /** The last seen guest paging mode (by VT-x). */
        PGMMODE                     enmLastSeenGuestMode;
        /** Current guest paging mode (as seen by HWACCMR3PagingModeChanged). */
        PGMMODE                     enmCurrGuestMode;
        /** Previous guest paging mode (as seen by HWACCMR3PagingModeChanged). */
        PGMMODE                     enmPrevGuestMode;
    } vmx;

    struct
    {
        /** R0 memory object for the VM control block (VMCB). */
        RTR0MEMOBJ                  pMemObjVMCB;
        /** Physical address of the VM control block (VMCB). */
        RTHCPHYS                    pVMCBPhys;
        /** Virtual address of the VM control block (VMCB). */
        R0PTRTYPE(void *)           pVMCB;

        /** Ring 0 handlers for VT-x. */
        DECLR0CALLBACKMEMBER(int, pfnVMRun,(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu));

    } svm;

    /** Event injection state. */
    struct
    {
        uint32_t                    fPending;
        uint32_t                    errCode;
        uint64_t                    intInfo;
    } Event;

    /** IO Block emulation state. */
    struct
    {
        bool                    fEnabled;
        uint8_t                 u8Align[7];

        /** RIP at the start of the io code we wish to emulate in the recompiler. */
        RTGCPTR                 GCPtrFunctionEip;

        uint64_t                cr0;
    } EmulateIoBlock;

    /** Currenty shadow paging mode. */
    PGMMODE                 enmShadowMode;

    /** The CPU ID of the CPU currently owning the VMCS. Set in
     * HWACCMR0Enter and cleared in HWACCMR0Leave. */
    RTCPUID                 idEnteredCpu;

    /** To keep track of pending TLB shootdown pages. (SMP guest only) */
    RTGCPTR                 aTlbShootdownPages[HWACCM_MAX_TLB_SHOOTDOWN_PAGES];
    RTUINT                  cTlbShootdownPages;

    RTUINT                  padding2[1];

    STAMPROFILEADV          StatEntry;
    STAMPROFILEADV          StatExit1;
    STAMPROFILEADV          StatExit2;
#if 1 /* temporary for tracking down darwin issues. */
    STAMPROFILEADV          StatExit2Sub1;
    STAMPROFILEADV          StatExit2Sub2;
    STAMPROFILEADV          StatExit2Sub3;
#endif
    STAMPROFILEADV          StatInGC;

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    STAMPROFILEADV          StatWorldSwitch3264;
#endif

    STAMCOUNTER             StatIntInject;

    STAMCOUNTER             StatExitShadowNM;
    STAMCOUNTER             StatExitGuestNM;
    STAMCOUNTER             StatExitShadowPF;
    STAMCOUNTER             StatExitGuestPF;
    STAMCOUNTER             StatExitGuestUD;
    STAMCOUNTER             StatExitGuestSS;
    STAMCOUNTER             StatExitGuestNP;
    STAMCOUNTER             StatExitGuestGP;
    STAMCOUNTER             StatExitGuestDE;
    STAMCOUNTER             StatExitGuestDB;
    STAMCOUNTER             StatExitGuestMF;
    STAMCOUNTER             StatExitInvpg;
    STAMCOUNTER             StatExitInvd;
    STAMCOUNTER             StatExitCpuid;
    STAMCOUNTER             StatExitRdtsc;
    STAMCOUNTER             StatExitRdpmc;
    STAMCOUNTER             StatExitCli;
    STAMCOUNTER             StatExitSti;
    STAMCOUNTER             StatExitPushf;
    STAMCOUNTER             StatExitPopf;
    STAMCOUNTER             StatExitIret;
    STAMCOUNTER             StatExitInt;
    STAMCOUNTER             StatExitCRxWrite[16];
    STAMCOUNTER             StatExitCRxRead[16];
    STAMCOUNTER             StatExitDRxWrite;
    STAMCOUNTER             StatExitDRxRead;
    STAMCOUNTER             StatExitRdmsr;
    STAMCOUNTER             StatExitWrmsr;
    STAMCOUNTER             StatExitCLTS;
    STAMCOUNTER             StatExitHlt;
    STAMCOUNTER             StatExitMwait;
    STAMCOUNTER             StatExitLMSW;
    STAMCOUNTER             StatExitIOWrite;
    STAMCOUNTER             StatExitIORead;
    STAMCOUNTER             StatExitIOStringWrite;
    STAMCOUNTER             StatExitIOStringRead;
    STAMCOUNTER             StatExitIrqWindow;
    STAMCOUNTER             StatExitMaxResume;
    STAMCOUNTER             StatIntReinject;
    STAMCOUNTER             StatPendingHostIrq;

    STAMCOUNTER             StatFlushPageManual;
    STAMCOUNTER             StatFlushPhysPageManual;
    STAMCOUNTER             StatFlushTLBManual;
    STAMCOUNTER             StatFlushPageInvlpg;
    STAMCOUNTER             StatFlushTLBWorldSwitch;
    STAMCOUNTER             StatNoFlushTLBWorldSwitch;
    STAMCOUNTER             StatFlushTLBCRxChange;
    STAMCOUNTER             StatFlushASID;
    STAMCOUNTER             StatFlushTLBInvlpga;
    STAMCOUNTER             StatTlbShootdown;
    STAMCOUNTER             StatTlbShootdownFlush;

    STAMCOUNTER             StatSwitchGuestIrq;
    STAMCOUNTER             StatSwitchToR3;

    STAMCOUNTER             StatTSCOffset;
    STAMCOUNTER             StatTSCIntercept;

    STAMCOUNTER             StatExitReasonNPF;
    STAMCOUNTER             StatDRxArmed;
    STAMCOUNTER             StatDRxContextSwitch;
    STAMCOUNTER             StatDRxIOCheck;


    R3PTRTYPE(PSTAMCOUNTER) paStatExitReason;
    R0PTRTYPE(PSTAMCOUNTER) paStatExitReasonR0;
} HWACCMCPU;
/** Pointer to HWACCM VM instance data. */
typedef HWACCMCPU *PHWACCMCPU;


#ifdef IN_RING0

VMMR0DECL(PHWACCM_CPUINFO) HWACCMR0GetCurrentCpu();
VMMR0DECL(PHWACCM_CPUINFO) HWACCMR0GetCurrentCpuEx(RTCPUID idCpu);


#ifdef VBOX_STRICT
VMMR0DECL(void) HWACCMDumpRegs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(void) HWACCMR0DumpDescriptor(PX86DESCHC  Desc, RTSEL Sel, const char *pszMsg);
#else
#define HWACCMDumpRegs(a, b ,c)             do { } while (0)
#define HWACCMR0DumpDescriptor(a, b, c)     do { } while (0)
#endif

/* Dummy callback handlers. */
VMMR0DECL(int) HWACCMR0DummyEnter(PVM pVM, PVMCPU pVCpu, PHWACCM_CPUINFO pCpu);
VMMR0DECL(int) HWACCMR0DummyLeave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int) HWACCMR0DummyEnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys);
VMMR0DECL(int) HWACCMR0DummyDisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);
VMMR0DECL(int) HWACCMR0DummyInitVM(PVM pVM);
VMMR0DECL(int) HWACCMR0DummyTermVM(PVM pVM);
VMMR0DECL(int) HWACCMR0DummySetupVM(PVM pVM);
VMMR0DECL(int) HWACCMR0DummyRunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int) HWACCMR0DummySaveHostState(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int) HWACCMR0DummyLoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
/**
 * Gets 64-bit GDTR and IDTR on darwin.
 * @param  pGdtr        Where to store the 64-bit GDTR.
 * @param  pIdtr        Where to store the 64-bit IDTR.
 */
DECLASM(void) hwaccmR0Get64bitGDTRandIDTR(PX86XDTR64 pGdtr, PX86XDTR64 pIdtr);

/**
 * Gets 64-bit CR3 on darwin.
 * @returns CR3
 */
DECLASM(uint64_t) hwaccmR0Get64bitCR3(void);
# endif

#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

