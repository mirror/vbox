/* $Id$ */
/** @file
 * HM - Internal header file.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___HMInternal_h
#define ___HMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/dis.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <iprt/memobj.h>
#include <iprt/cpuset.h>
#include <iprt/mp.h>
#include <iprt/avl.h>

#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL) || defined (VBOX_WITH_64_BITS_GUESTS)
/* Enable 64 bits guest support. */
# define VBOX_ENABLE_64_BITS_GUESTS
#endif

#define VMX_USE_CACHED_VMCS_ACCESSES
#define HM_VMX_EMULATE_REALMODE

/* The MSR auto load/store does not work for KERNEL_GS_BASE MSR, thus we
 * handle this MSR manually. See @bugref{6208}. This is clearly visible while
 * booting Solaris 11 (11.1 b19) VMs with 2 Cpus.
 *
 * Note: don't forget to update the assembly files while modifying this!
 */
# define VBOX_WITH_AUTO_MSR_LOAD_RESTORE

RT_C_DECLS_BEGIN


/** @defgroup grp_hm_int       Internal
 * @ingroup grp_hm
 * @internal
 * @{
 */


/** Maximum number of exit reason statistics counters. */
#define MAX_EXITREASON_STAT        0x100
#define MASK_EXITREASON_STAT       0xff
#define MASK_INJECT_IRQ_STAT       0xff

/** @name Changed flags
 * These flags are used to keep track of which important registers that
 * have been changed since last they were reset.
 * @{
 */
#define HM_CHANGED_GUEST_FPU                RT_BIT(0)
#define HM_CHANGED_GUEST_CR0                RT_BIT(1)
#define HM_CHANGED_GUEST_CR3                RT_BIT(2)
#define HM_CHANGED_GUEST_CR4                RT_BIT(3)
#define HM_CHANGED_GUEST_GDTR               RT_BIT(4)
#define HM_CHANGED_GUEST_IDTR               RT_BIT(5)
#define HM_CHANGED_GUEST_LDTR               RT_BIT(6)
#define HM_CHANGED_GUEST_TR                 RT_BIT(7)
#define HM_CHANGED_GUEST_MSR                RT_BIT(8)
#define HM_CHANGED_GUEST_SEGMENT_REGS       RT_BIT(9)
#define HM_CHANGED_GUEST_DEBUG              RT_BIT(10)
#define HM_CHANGED_HOST_CONTEXT             RT_BIT(11)

#define HM_CHANGED_ALL_GUEST               (  HM_CHANGED_GUEST_SEGMENT_REGS \
                                            | HM_CHANGED_GUEST_CR0          \
                                            | HM_CHANGED_GUEST_CR3          \
                                            | HM_CHANGED_GUEST_CR4          \
                                            | HM_CHANGED_GUEST_GDTR         \
                                            | HM_CHANGED_GUEST_IDTR         \
                                            | HM_CHANGED_GUEST_LDTR         \
                                            | HM_CHANGED_GUEST_TR           \
                                            | HM_CHANGED_GUEST_MSR          \
                                            | HM_CHANGED_GUEST_DEBUG        \
                                            | HM_CHANGED_GUEST_FPU)

#define HM_CHANGED_ALL                      (HM_CHANGED_ALL_GUEST | HM_CHANGED_HOST_CONTEXT)
/** @} */

/** Maximum number of page flushes we are willing to remember before considering a full TLB flush. */
#define HM_MAX_TLB_SHOOTDOWN_PAGES      8

/** Size for the EPT identity page table (1024 4 MB pages to cover the entire address space). */
#define HM_EPT_IDENTITY_PG_TABLE_SIZE   PAGE_SIZE
/** Size of the TSS structure + 2 pages for the IO bitmap + end byte. */
#define HM_VTX_TSS_SIZE                 (sizeof(VBOXTSS) + 2*PAGE_SIZE + 1)
/** Total guest mapped memory needed. */
#define HM_VTX_TOTAL_DEVHEAP_MEM        (HM_EPT_IDENTITY_PG_TABLE_SIZE + HM_VTX_TSS_SIZE)

/** Enable for TPR guest patching. */
#define VBOX_HM_WITH_GUEST_PATCHING

/** HM SSM version
 */
#ifdef VBOX_HM_WITH_GUEST_PATCHING
# define HM_SSM_VERSION                 5
# define HM_SSM_VERSION_NO_PATCHING     4
#else
# define HM_SSM_VERSION                 4
# define HM_SSM_VERSION_NO_PATCHING     4
#endif
#define HM_SSM_VERSION_2_0_X            3

/**
 * Global per-cpu information. (host)
 */
typedef struct HMGLOBLCPUINFO
{
    /** The CPU ID. */
    RTCPUID             idCpu;
    /** The memory object   */
    RTR0MEMOBJ          hMemObj;
    /** Current ASID (AMD-V) / VPID (Intel). */
    uint32_t            uCurrentAsid;
    /** TLB flush count. */
    uint32_t            cTlbFlushes;
    /** Whether to flush each new ASID/VPID before use. */
    bool                fFlushAsidBeforeUse;
    /** Configured for VT-x or AMD-V. */
    bool                fConfigured;
    /** Set if the VBOX_HWVIRTEX_IGNORE_SVM_IN_USE hack is active. */
    bool                fIgnoreAMDVInUseError;
    /** In use by our code. (for power suspend) */
    volatile bool       fInUse;
} HMGLOBLCPUINFO;
/** Pointer to the per-cpu global information. */
typedef HMGLOBLCPUINFO *PHMGLOBLCPUINFO;

typedef enum
{
    HMPENDINGIO_INVALID = 0,
    HMPENDINGIO_PORT_READ,
    HMPENDINGIO_PORT_WRITE,
    HMPENDINGIO_STRING_READ,
    HMPENDINGIO_STRING_WRITE,
    /** The usual 32-bit paranoia. */
    HMPENDINGIO_32BIT_HACK   = 0x7fffffff
} HMPENDINGIO;


typedef enum
{
    HMTPRINSTR_INVALID,
    HMTPRINSTR_READ,
    HMTPRINSTR_READ_SHR4,
    HMTPRINSTR_WRITE_REG,
    HMTPRINSTR_WRITE_IMM,
    HMTPRINSTR_JUMP_REPLACEMENT,
    /** The usual 32-bit paranoia. */
    HMTPRINSTR_32BIT_HACK   = 0x7fffffff
} HMTPRINSTR;

typedef struct
{
    /** The key is the address of patched instruction. (32 bits GC ptr) */
    AVLOU32NODECORE         Core;
    /** Original opcode. */
    uint8_t                 aOpcode[16];
    /** Instruction size. */
    uint32_t                cbOp;
    /** Replacement opcode. */
    uint8_t                 aNewOpcode[16];
    /** Replacement instruction size. */
    uint32_t                cbNewOp;
    /** Instruction type. */
    HMTPRINSTR          enmType;
    /** Source operand. */
    uint32_t                uSrcOperand;
    /** Destination operand. */
    uint32_t                uDstOperand;
    /** Number of times the instruction caused a fault. */
    uint32_t                cFaults;
    /** Patch address of the jump replacement. */
    RTGCPTR32               pJumpTarget;
} HMTPRPATCH;
/** Pointer to HMTPRPATCH. */
typedef HMTPRPATCH *PHMTPRPATCH;

/**
 * Switcher function, HC to RC.
 *
 * @param   pVM             Pointer to the VM.
 * @param   uOffsetVMCPU    VMCPU offset from pVM
 * @returns Return code indicating the action to take.
 */
typedef DECLCALLBACK (int) FNHMSWITCHERHC(PVM pVM, uint32_t uOffsetVMCPU);
/** Pointer to switcher function. */
typedef FNHMSWITCHERHC *PFNHMSWITCHERHC;

/**
 * HM VM Instance data.
 * Changes to this must checked against the padding of the hm union in VM!
 */
typedef struct HM
{
    /** Set when we've initialized VMX or SVM. */
    bool                        fInitialized;

    /** Set when hardware acceleration is allowed. */
    bool                        fAllowed;

    /** Set if nested paging is enabled. */
    bool                        fNestedPaging;

    /** Set if nested paging is allowed. */
    bool                        fAllowNestedPaging;

    /** Set if large pages are enabled (requires nested paging). */
    bool                        fLargePages;

    /** Set if we can support 64-bit guests or not. */
    bool                        fAllow64BitGuests;

    /** Set if an IO-APIC is configured for this VM. */
    bool                        fHasIoApic;

    /** Set when TPR patching is allowed. */
    bool                        fTRPPatchingAllowed;

    /** Set when we initialize VT-x or AMD-V once for all CPUs. */
    bool                        fGlobalInit;

    /** Set when TPR patching is active. */
    bool                        fTPRPatchingActive;
    bool                        u8Alignment[6];

    /** And mask for copying register contents. */
    uint64_t                    u64RegisterMask;

    /** Maximum ASID allowed. */
    uint32_t                    uMaxAsid;

    /** The maximum number of resumes loops allowed in ring-0 (safety precaution).
     * This number is set much higher when RTThreadPreemptIsPending is reliable. */
    uint32_t                    cMaxResumeLoops;

    /** Guest allocated memory for patching purposes. */
    RTGCPTR                     pGuestPatchMem;
    /** Current free pointer inside the patch block. */
    RTGCPTR                     pFreeGuestPatchMem;
    /** Size of the guest patch memory block. */
    uint32_t                    cbGuestPatchMem;
    uint32_t                    uPadding1;

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    /** 32 to 64 bits switcher entrypoint. */
    R0PTRTYPE(PFNHMSWITCHERHC) pfnHost32ToGuest64R0;

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

    RTRCPTR                     uAlignment[2];
/*#elif defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    uint32_t                    u32Alignment[1]; */
#endif

    struct
    {
        /** Set by the ring-0 side of HM to indicate VMX is supported by the
         *  CPU. */
        bool                        fSupported;

        /** Set when we've enabled VMX. */
        bool                        fEnabled;

        /** Set if VPID is supported. */
        bool                        fVpid;

        /** Set if VT-x VPID is allowed. */
        bool                        fAllowVpid;

        /** Set if unrestricted guest execution is allowed (real and protected mode without paging). */
        bool                        fUnrestrictedGuest;

        /** Whether we're using the preemption timer or not. */
        bool                        fUsePreemptTimer;
        /** The shift mask employed by the VMX-Preemption timer. */
        uint8_t                     cPreemptTimerShift;

        bool                        uAlignment[1];

        /** Virtual address of the TSS page used for real mode emulation. */
        R3PTRTYPE(PVBOXTSS)         pRealModeTSS;

        /** Virtual address of the identity page table used for real mode and protected mode without paging emulation in EPT mode. */
        R3PTRTYPE(PX86PD)           pNonPagingModeEPTPageTable;

        /** R0 memory object for the APIC-access page. */
        RTR0MEMOBJ                  hMemObjApicAccess;
        /** Physical address of the APIC-access page. */
        RTHCPHYS                    HCPhysApicAccess;
        /** Virtual address of the APIC-access page. */
        R0PTRTYPE(uint8_t *)        pbApicAccess;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        RTR0MEMOBJ                  hMemObjScratch;
        RTHCPHYS                    HCPhysScratch;
        R0PTRTYPE(uint8_t *)        pbScratch;
#endif
        /** Ring 0 handlers for VT-x. */
        DECLR0CALLBACKMEMBER(void, pfnSetupTaggedTlb, (PVM pVM, PVMCPU pVCpu));

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
        uint32_t                    u32Alignment;
#endif
        /** Host CR4 value (set by ring-0 VMX init) */
        uint64_t                    hostCR4;

        /** Host EFER value (set by ring-0 VMX init) */
        uint64_t                    hostEFER;

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
            uint64_t                vmx_ept_vpid_caps;
        } msr;

        /** Flush types for invept & invvpid; they depend on capabilities. */
        VMX_FLUSH_EPT               enmFlushEpt;
        VMX_FLUSH_VPID              enmFlushVpid;
    } vmx;

    struct
    {
        /** Set by the ring-0 side of HM to indicate SVM is supported by the
         *  CPU. */
        bool                        fSupported;
        /** Set when we've enabled SVM. */
        bool                        fEnabled;
        /** Set if erratum 170 affects the AMD cpu. */
        bool                        fAlwaysFlushTLB;
        /** Set when the hack to ignore VERR_SVM_IN_USE is active. */
        bool                        fIgnoreInUseError;

        /** R0 memory object for the IO bitmap (12kb). */
        RTR0MEMOBJ                  hMemObjIOBitmap;
        /** Physical address of the IO bitmap (12kb). */
        RTHCPHYS                    HCPhysIOBitmap;
        /** Virtual address of the IO bitmap. */
        R0PTRTYPE(void *)           pvIOBitmap;

        /* HWCR MSR (for diagnostics) */
        uint64_t                    msrHwcr;

        /** SVM revision. */
        uint32_t                    u32Rev;

        /** SVM feature bits from cpuid 0x8000000a */
        uint32_t                    u32Features;
    } svm;

    /**
     * AVL tree with all patches (active or disabled) sorted by guest instruction address
     */
    AVLOU32TREE                     PatchTree;
    uint32_t                        cPatches;
    HMTPRPATCH                  aPatches[64];

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    /** Saved error from detection */
    int32_t                 lLastError;

    /** HMR0Init was run */
    bool                    fHMR0Init;
    bool                    u8Alignment1[7];

    STAMCOUNTER             StatTprPatchSuccess;
    STAMCOUNTER             StatTprPatchFailure;
    STAMCOUNTER             StatTprReplaceSuccess;
    STAMCOUNTER             StatTprReplaceFailure;
} HM;
/** Pointer to HM VM instance data. */
typedef HM *PHM;

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
        RTHCPHYS    HCPhysCpuPage;
        RTHCPHYS    HCPhysVMCS;
        RTGCPTR     pCache;
        RTGCPTR     pCtx;
    } TestIn;
    struct
    {
        RTHCPHYS    HCPhysVMCS;
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

/** VMX StartVM function. */
typedef DECLCALLBACK(int) FNHMVMXSTARTVM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu);
/** Pointer to a VMX StartVM function. */
typedef R0PTRTYPE(FNHMVMXSTARTVM *) PFNHMVMXSTARTVM;

/** SVM VMRun function. */
typedef DECLCALLBACK(int) FNHMSVMVMRUN(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);
/** Pointer to a SVM VMRun function. */
typedef R0PTRTYPE(FNHMSVMVMRUN *) PFNHMSVMVMRUN;

/**
 * HM VMCPU Instance data.
 */
typedef struct HMCPU
{
    /** Old style FPU reporting trap mask override performed (optimization) */
    bool                        fFPUOldStyleOverride;
    /** Set if we don't have to flush the TLB on VM entry. */
    bool                        fResumeVM;
    /** Set if we need to flush the TLB during the world switch. */
    bool                        fForceTLBFlush;
    /** Set when we're using VT-x or AMD-V at that moment. */
    bool                        fActive;
    /** Set when the TLB has been checked until we return from the world switch. */
    volatile bool               fCheckedTLBFlush;
    uint8_t                     u8Alignment[3];

    /** World switch exit counter. */
    volatile uint32_t           cWorldSwitchExits;
    /** HM_CHANGED_* flags. */
    uint32_t                    fContextUseFlags;
    /** Id of the last cpu we were executing code on (NIL_RTCPUID for the first time) */
    RTCPUID                     idLastCpu;
    /** TLB flush count */
    uint32_t                    cTlbFlushes;
    /** Current ASID in use by the VM */
    uint32_t                    uCurrentAsid;
    uint32_t                    u32Alignment;

    /* Host's TSC_AUX MSR (used when RDTSCP doesn't cause VM-exits). */
    uint64_t                    u64HostTscAux;

    struct
    {
        /** Physical address of the VM control structure (VMCS). */
        RTHCPHYS                    HCPhysVMCS;
        /** R0 memory object for the VM control structure (VMCS). */
        RTR0MEMOBJ                  hMemObjVMCS;
        /** Virtual address of the VM control structure (VMCS). */
        R0PTRTYPE(void *)           pvVMCS;
        /** Ring 0 handlers for VT-x. */
        PFNHMVMXSTARTVM             pfnStartVM;

#if HC_ARCH_BITS == 32
        uint32_t                    u32Alignment;
#endif

        /** Current VMX_VMCS_CTRL_PROC_EXEC_CONTROLS. */
        uint64_t                    u64ProcCtls;
        /** Current VMX_VMCS_CTRL_PROC_EXEC2_CONTROLS. */
        uint64_t                    u64ProcCtls2;
        /** Current VMX_VMCS_CTRL_EXIT_CONTROLS. */
        uint64_t                    u64ExitCtls;
        /** Current VMX_VMCS_CTRL_ENTRY_CONTROLS. */
        uint64_t                    u64EntryCtls;
        /** Physical address of the virtual APIC page for TPR caching. */
        RTHCPHYS                    HCPhysVAPIC;
        /** R0 memory object for the virtual APIC page for TPR caching. */
        RTR0MEMOBJ                  hMemObjVAPIC;
        /** Virtual address of the virtual APIC page for TPR caching. */
        R0PTRTYPE(uint8_t *)        pbVAPIC;

        /** Current CR0 mask. */
        uint64_t                    cr0_mask;
        /** Current CR4 mask. */
        uint64_t                    cr4_mask;
        /** Current exception bitmap. */
        uint32_t                    u32XcptBitmap;
        uint32_t                    uAlignment0;
        /** Current EPTP. */
        RTHCPHYS                    GCPhysEPTP;

        /** Physical address of the MSR bitmap. */
        RTHCPHYS                    HCPhysMsrBitmap;
        /** R0 memory object for the MSR bitmap. */
        RTR0MEMOBJ                  hMemObjMsrBitmap;
        /** Virtual address of the MSR bitmap. */
        R0PTRTYPE(void *)           pvMsrBitmap;

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /** Physical address of the VM-entry MSR-load and VM-exit MSR-store area (used
         *  for guest MSRs). */
        RTHCPHYS                    HCPhysGuestMsr;
        /** R0 memory object of the VM-entry MSR-load and VM-exit MSR-store area
         *  (used for guest MSRs). */
        RTR0MEMOBJ                  hMemObjGuestMsr;
        /** Virtual address of the VM-entry MSR-load and VM-exit MSR-store area (used
         *  for guest MSRs). */
        R0PTRTYPE(void *)           pvGuestMsr;

        /** Physical address of the VM-exit MSR-load area (used for host MSRs). */
        RTHCPHYS                    HCPhysHostMsr;
        /** R0 memory object for the VM-exit MSR-load area (used for host MSRs). */
        RTR0MEMOBJ                  hMemObjHostMsr;
        /** Virtual address of the VM-exit MSR-load area (used for host MSRs). */
        R0PTRTYPE(void *)           pvHostMsr;

        /* Number of automatically loaded/restored guest MSRs during the world switch. */
        uint32_t                    cGuestMsrs;
        uint32_t                    uAlignment;
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

        /* Last use TSC offset value. (cached) */
        uint64_t                    u64TSCOffset;
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
        /** Current guest paging mode (as seen by HMR3PagingModeChanged). */
        PGMMODE                     enmCurrGuestMode;
        /** Previous guest paging mode (as seen by HMR3PagingModeChanged). */
        PGMMODE                     enmPrevGuestMode;
    } vmx;

    struct
    {
        /** R0 memory object for the host VM control block (VMCB). */
        RTR0MEMOBJ                  hMemObjVMCBHost;
        /** Physical address of the host VM control block (VMCB). */
        RTHCPHYS                    HCPhysVMCBHost;
        /** Virtual address of the host VM control block (VMCB). */
        R0PTRTYPE(void *)           pvVMCBHost;

        /** R0 memory object for the VM control block (VMCB). */
        RTR0MEMOBJ                  hMemObjVMCB;
        /** Physical address of the VM control block (VMCB). */
        RTHCPHYS                    HCPhysVMCB;
        /** Virtual address of the VM control block (VMCB). */
        R0PTRTYPE(void *)           pvVMCB;

        /** Ring 0 handlers for VT-x. */
        PFNHMSVMVMRUN               pfnVMRun;

        /** R0 memory object for the MSR bitmap (8kb). */
        RTR0MEMOBJ                  hMemObjMsrBitmap;
        /** Physical address of the MSR bitmap (8kb). */
        RTHCPHYS                    HCPhysMsrBitmap;
        /** Virtual address of the MSR bitmap. */
        R0PTRTYPE(void *)           pvMsrBitmap;
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

    struct
    {
        /* Pending IO operation type. */
        HMPENDINGIO         enmType;
        uint32_t                uPadding;
        RTGCPTR                 GCPtrRip;
        RTGCPTR                 GCPtrRipNext;
        union
        {
            struct
            {
                unsigned        uPort;
                unsigned        uAndVal;
                unsigned        cbSize;
            } Port;
            uint64_t            aRaw[2];
        } s;
    } PendingIO;

    /** Current shadow paging mode. */
    PGMMODE                 enmShadowMode;

    /** The CPU ID of the CPU currently owning the VMCS. Set in
     * HMR0Enter and cleared in HMR0Leave. */
    RTCPUID                 idEnteredCpu;

    /** To keep track of pending TLB shootdown pages. (SMP guest only) */
    struct
    {
        RTGCPTR             aPages[HM_MAX_TLB_SHOOTDOWN_PAGES];
        unsigned            cPages;
    } TlbShootdown;

    /** For saving stack space, the disassembler state is allocated here instead of
     * on the stack. */
    DISCPUSTATE             DisState;

    uint32_t                padding2[1];

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
    STAMPROFILEADV          StatPoke;
    STAMPROFILEADV          StatSpinPoke;
    STAMPROFILEADV          StatSpinPokeFailed;

    STAMCOUNTER             StatIntInject;

    STAMCOUNTER             StatExitShadowNM;
    STAMCOUNTER             StatExitGuestNM;
    STAMCOUNTER             StatExitShadowPF;
    STAMCOUNTER             StatExitShadowPFEM;
    STAMCOUNTER             StatExitGuestPF;
    STAMCOUNTER             StatExitGuestUD;
    STAMCOUNTER             StatExitGuestSS;
    STAMCOUNTER             StatExitGuestNP;
    STAMCOUNTER             StatExitGuestGP;
    STAMCOUNTER             StatExitGuestDE;
    STAMCOUNTER             StatExitGuestDB;
    STAMCOUNTER             StatExitGuestMF;
    STAMCOUNTER             StatExitGuestBP;
    STAMCOUNTER             StatExitGuestXF;
    STAMCOUNTER             StatExitGuestXcpUnk;
    STAMCOUNTER             StatExitInvlpg;
    STAMCOUNTER             StatExitInvd;
    STAMCOUNTER             StatExitCpuid;
    STAMCOUNTER             StatExitRdtsc;
    STAMCOUNTER             StatExitRdtscp;
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
    STAMCOUNTER             StatExitClts;
    STAMCOUNTER             StatExitHlt;
    STAMCOUNTER             StatExitMwait;
    STAMCOUNTER             StatExitMonitor;
    STAMCOUNTER             StatExitLMSW;
    STAMCOUNTER             StatExitIOWrite;
    STAMCOUNTER             StatExitIORead;
    STAMCOUNTER             StatExitIOStringWrite;
    STAMCOUNTER             StatExitIOStringRead;
    STAMCOUNTER             StatExitIrqWindow;
    STAMCOUNTER             StatExitMaxResume;
    STAMCOUNTER             StatExitPreemptPending;
    STAMCOUNTER             StatExitMtf;
    STAMCOUNTER             StatIntReinject;
    STAMCOUNTER             StatPendingHostIrq;

    STAMCOUNTER             StatFlushPage;
    STAMCOUNTER             StatFlushPageManual;
    STAMCOUNTER             StatFlushPhysPageManual;
    STAMCOUNTER             StatFlushTlb;
    STAMCOUNTER             StatFlushTlbManual;
    STAMCOUNTER             StatFlushPageInvlpg;
    STAMCOUNTER             StatFlushTlbWorldSwitch;
    STAMCOUNTER             StatNoFlushTlbWorldSwitch;
    STAMCOUNTER             StatFlushTlbCRxChange;
    STAMCOUNTER             StatFlushAsid;
    STAMCOUNTER             StatFlushNestedPaging;
    STAMCOUNTER             StatFlushTlbInvlpga;
    STAMCOUNTER             StatTlbShootdown;
    STAMCOUNTER             StatTlbShootdownFlush;

    STAMCOUNTER             StatSwitchGuestIrq;
    STAMCOUNTER             StatSwitchToR3;

    STAMCOUNTER             StatTscOffset;
    STAMCOUNTER             StatTscIntercept;
    STAMCOUNTER             StatTscInterceptOverFlow;

    STAMCOUNTER             StatExitReasonNpf;
    STAMCOUNTER             StatDRxArmed;
    STAMCOUNTER             StatDRxContextSwitch;
    STAMCOUNTER             StatDRxIoCheck;

    STAMCOUNTER             StatLoadMinimal;
    STAMCOUNTER             StatLoadFull;

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    STAMCOUNTER             StatFpu64SwitchBack;
    STAMCOUNTER             StatDebug64SwitchBack;
#endif

#ifdef VBOX_WITH_STATISTICS
    R3PTRTYPE(PSTAMCOUNTER) paStatExitReason;
    R0PTRTYPE(PSTAMCOUNTER) paStatExitReasonR0;
    R3PTRTYPE(PSTAMCOUNTER) paStatInjectedIrqs;
    R0PTRTYPE(PSTAMCOUNTER) paStatInjectedIrqsR0;
#endif
} HMCPU;
/** Pointer to HM VM instance data. */
typedef HMCPU *PHMCPU;


#ifdef IN_RING0

VMMR0DECL(PHMGLOBLCPUINFO) HMR0GetCurrentCpu(void);
VMMR0DECL(PHMGLOBLCPUINFO) HMR0GetCurrentCpuEx(RTCPUID idCpu);


#ifdef VBOX_STRICT
VMMR0DECL(void) HMDumpRegs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(void) HMR0DumpDescriptor(PCX86DESCHC pDesc, RTSEL Sel, const char *pszMsg);
#else
# define HMDumpRegs(a, b ,c)            do { } while (0)
# define HMR0DumpDescriptor(a, b, c)    do { } while (0)
#endif

# ifdef VBOX_WITH_KERNEL_USING_XMM
DECLASM(int)   hmR0VMXStartVMWrapXMM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu, PFNHMVMXSTARTVM pfnStartVM);
DECLASM(int)   hmR0SVMRunWrapXMM(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu, PFNHMSVMVMRUN pfnVMRun);
# endif

# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
/**
 * Gets 64-bit GDTR and IDTR on darwin.
 * @param  pGdtr        Where to store the 64-bit GDTR.
 * @param  pIdtr        Where to store the 64-bit IDTR.
 */
DECLASM(void) hmR0Get64bitGdtrAndIdtr(PX86XDTR64 pGdtr, PX86XDTR64 pIdtr);

/**
 * Gets 64-bit CR3 on darwin.
 * @returns CR3
 */
DECLASM(uint64_t) hmR0Get64bitCR3(void);
# endif

#endif /* IN_RING0 */

/** @} */

RT_C_DECLS_END

#endif

