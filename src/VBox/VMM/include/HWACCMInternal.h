/* $Id$ */
/** @file
 * HM - Internal header file.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___HWACCMInternal_h
#define ___HWACCMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/dis.h>
#include <VBox/vmm/hwaccm.h>
#include <VBox/vmm/hwacc_vmx.h>
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
#define HWACCM_VMX_EMULATE_REALMODE


#if 0
/* Seeing somewhat random behaviour on my Nehalem system with auto-save of guest MSRs;
 * for some strange reason the CPU doesn't save the MSRs during the VM-exit.
 * Clearly visible with a dual VCPU configured OpenSolaris 200906 live cd VM.
 *
 * Note: change the assembly files when enabling this! (remove the manual auto load/save)
 */
#define VBOX_WITH_AUTO_MSR_LOAD_RESTORE
#endif

RT_C_DECLS_BEGIN


/** @defgroup grp_hwaccm_int       Internal
 * @ingroup grp_hwaccm
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
#define HWACCM_CHANGED_GUEST_FPU                RT_BIT(0)
#define HWACCM_CHANGED_GUEST_CR0                RT_BIT(1)
#define HWACCM_CHANGED_GUEST_CR3                RT_BIT(2)
#define HWACCM_CHANGED_GUEST_CR4                RT_BIT(3)
#define HWACCM_CHANGED_GUEST_GDTR               RT_BIT(4)
#define HWACCM_CHANGED_GUEST_IDTR               RT_BIT(5)
#define HWACCM_CHANGED_GUEST_LDTR               RT_BIT(6)
#define HWACCM_CHANGED_GUEST_TR                 RT_BIT(7)
#define HWACCM_CHANGED_GUEST_MSR                RT_BIT(8)
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
                                            |   HWACCM_CHANGED_GUEST_MSR          \
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
                                            |   HWACCM_CHANGED_GUEST_MSR          \
                                            |   HWACCM_CHANGED_GUEST_DEBUG        \
                                            |   HWACCM_CHANGED_GUEST_FPU)

/** @} */

/** Maximum number of page flushes we are willing to remember before considering a full TLB flush. */
#define HWACCM_MAX_TLB_SHOOTDOWN_PAGES      8

/** Size for the EPT identity page table (1024 4 MB pages to cover the entire address space). */
#define HWACCM_EPT_IDENTITY_PG_TABLE_SIZE   PAGE_SIZE
/** Size of the TSS structure + 2 pages for the IO bitmap + end byte. */
#define HWACCM_VTX_TSS_SIZE                 (sizeof(VBOXTSS) + 2*PAGE_SIZE + 1)
/** Total guest mapped memory needed. */
#define HWACCM_VTX_TOTAL_DEVHEAP_MEM        (HWACCM_EPT_IDENTITY_PG_TABLE_SIZE + HWACCM_VTX_TSS_SIZE)

/** Enable for TPR guest patching. */
#define VBOX_HWACCM_WITH_GUEST_PATCHING

/** HWACCM SSM version
 */
#ifdef VBOX_HWACCM_WITH_GUEST_PATCHING
# define HWACCM_SSM_VERSION                 5
# define HWACCM_SSM_VERSION_NO_PATCHING     4
#else
# define HWACCM_SSM_VERSION                 4
# define HWACCM_SSM_VERSION_NO_PATCHING     4
#endif
#define HWACCM_SSM_VERSION_2_0_X            3

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
    uint32_t            uCurrentASID;
    /** TLB flush count. */
    uint32_t            cTLBFlushes;

    /** Whether to flush each new ASID/VPID before use. */
    bool                fFlushASIDBeforeUse;

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
    HWACCMPENDINGIO_INVALID = 0,
    HWACCMPENDINGIO_PORT_READ,
    HWACCMPENDINGIO_PORT_WRITE,
    HWACCMPENDINGIO_STRING_READ,
    HWACCMPENDINGIO_STRING_WRITE,
    /** The usual 32-bit paranoia. */
    HWACCMPENDINGIO_32BIT_HACK   = 0x7fffffff
} HWACCMPENDINGIO;


typedef enum
{
    HWACCMTPRINSTR_INVALID,
    HWACCMTPRINSTR_READ,
    HWACCMTPRINSTR_READ_SHR4,
    HWACCMTPRINSTR_WRITE_REG,
    HWACCMTPRINSTR_WRITE_IMM,
    HWACCMTPRINSTR_JUMP_REPLACEMENT,
    /** The usual 32-bit paranoia. */
    HWACCMTPRINSTR_32BIT_HACK   = 0x7fffffff
} HWACCMTPRINSTR;

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
    HWACCMTPRINSTR          enmType;
    /** Source operand. */
    uint32_t                uSrcOperand;
    /** Destination operand. */
    uint32_t                uDstOperand;
    /** Number of times the instruction caused a fault. */
    uint32_t                cFaults;
    /** Patch address of the jump replacement. */
    RTGCPTR32               pJumpTarget;
} HWACCMTPRPATCH;
/** Pointer to HWACCMTPRPATCH. */
typedef HWACCMTPRPATCH *PHWACCMTPRPATCH;

/**
 * Switcher function, HC to RC.
 *
 * @param   pVM             Pointer to the VM.
 * @param   uOffsetVMCPU    VMCPU offset from pVM
 * @returns Return code indicating the action to take.
 */
typedef DECLCALLBACK (int) FNHWACCMSWITCHERHC(PVM pVM, uint32_t uOffsetVMCPU);
/** Pointer to switcher function. */
typedef FNHWACCMSWITCHERHC *PFNHWACCMSWITCHERHC;

/**
 * HWACCM VM Instance data.
 * Changes to this must checked against the padding of the hwaccm union in VM!
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
    uint32_t                    uMaxASID;

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

    RTRCPTR                     uAlignment[2];
/*#elif defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    uint32_t                    u32Alignment[1]; */
#endif

    struct
    {
        /** Set by the ring-0 side of HWACCM to indicate VMX is supported by the
         *  CPU. */
        bool                        fSupported;

        /** Set when we've enabled VMX. */
        bool                        fEnabled;

        /** Set if VPID is supported. */
        bool                        fVPID;

        /** Set if VT-x VPID is allowed. */
        bool                        fAllowVPID;

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

        /** R0 memory object for the APIC physical page (serves for filtering accesses). */
        RTR0MEMOBJ                  pMemObjAPIC;
        /** Physical address of the APIC physical page (serves for filtering accesses). */
        RTHCPHYS                    pAPICPhys;
        /** Virtual address of the APIC physical page (serves for filtering accesses). */
        R0PTRTYPE(uint8_t *)        pAPIC;

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
            uint64_t                vmx_eptcaps;
        } msr;

        /** Flush types for invept & invvpid; they depend on capabilities. */
        VMX_FLUSH_EPT               enmFlushEPT;
        VMX_FLUSH_VPID              enmFlushVPID;
    } vmx;

    struct
    {
        /** Set by the ring-0 side of HWACCM to indicate SVM is supported by the
         *  CPU. */
        bool                        fSupported;
        /** Set when we've enabled SVM. */
        bool                        fEnabled;
        /** Set if erratum 170 affects the AMD cpu. */
        bool                        fAlwaysFlushTLB;
        /** Set when the hack to ignore VERR_SVM_IN_USE is active. */
        bool                        fIgnoreInUseError;

        /** R0 memory object for the IO bitmap (12kb). */
        RTR0MEMOBJ                  pMemObjIOBitmap;
        /** Physical address of the IO bitmap (12kb). */
        RTHCPHYS                    pIOBitmapPhys;
        /** Virtual address of the IO bitmap. */
        R0PTRTYPE(void *)           pIOBitmap;

        /* HWCR msr (for diagnostics) */
        uint64_t                    msrHWCR;

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
    HWACCMTPRPATCH                  aPatches[64];

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    /** Saved error from detection */
    int32_t                 lLastError;

    /** HWACCMR0Init was run */
    bool                    fHWACCMR0Init;
    bool                    u8Alignment1[7];

    STAMCOUNTER             StatTPRPatchSuccess;
    STAMCOUNTER             StatTPRPatchFailure;
    STAMCOUNTER             StatTPRReplaceSuccess;
    STAMCOUNTER             StatTPRReplaceFailure;
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
typedef DECLCALLBACK(int) FNHWACCMVMXSTARTVM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu);
/** Pointer to a VMX StartVM function. */
typedef R0PTRTYPE(FNHWACCMVMXSTARTVM *) PFNHWACCMVMXSTARTVM;

/** SVM VMRun function. */
typedef DECLCALLBACK(int) FNHWACCMSVMVMRUN(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);
/** Pointer to a SVM VMRun function. */
typedef R0PTRTYPE(FNHWACCMSVMVMRUN *) PFNHWACCMSVMVMRUN;

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

    /** Set when the TLB has been checked until we return from the world switch. */
    volatile bool               fCheckedTLBFlush;
    uint8_t                     bAlignment[3];

    /** World switch exit counter. */
    volatile uint32_t           cWorldSwitchExits;

    /** HWACCM_CHANGED_* flags. */
    uint32_t                    fContextUseFlags;

    /** Id of the last cpu we were executing code on (NIL_RTCPUID for the first time) */
    RTCPUID                     idLastCpu;

    /** TLB flush count */
    uint32_t                    cTLBFlushes;

    /** Current ASID in use by the VM */
    uint32_t                    uCurrentASID;

    uint32_t                    u32Alignment;

    struct
    {
        /** Physical address of the VM control structure (VMCS). */
        RTHCPHYS                    HCPhysVMCS;
        /** R0 memory object for the VM control structure (VMCS). */
        RTR0MEMOBJ                  hMemObjVMCS;
        /** Virtual address of the VM control structure (VMCS). */
        R0PTRTYPE(void *)           pvVMCS;

        /** Ring 0 handlers for VT-x. */
        PFNHWACCMVMXSTARTVM         pfnStartVM;

#if HC_ARCH_BITS == 32
        uint32_t                    u32Alignment;
#endif

        /** Current VMX_VMCS_CTRL_PROC_EXEC_CONTROLS. */
        uint64_t                    proc_ctls;

        /** Current VMX_VMCS_CTRL_PROC_EXEC2_CONTROLS. */
        uint64_t                    proc_ctls2;

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

        /** Current EPTP. */
        RTHCPHYS                    GCPhysEPTP;

        /** Physical address of the MSR bitmap (1 page). */
        RTHCPHYS                    pMSRBitmapPhys;
        /** R0 memory object for the MSR bitmap (1 page). */
        RTR0MEMOBJ                  pMemObjMSRBitmap;
        /** Virtual address of the MSR bitmap (1 page). */
        R0PTRTYPE(uint8_t *)        pMSRBitmap;

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /** Physical address of the guest MSR load area (1 page). */
        RTHCPHYS                    pGuestMSRPhys;
        /** R0 memory object for the guest MSR load area (1 page). */
        RTR0MEMOBJ                  pMemObjGuestMSR;
        /** Virtual address of the guest MSR load area (1 page). */
        R0PTRTYPE(uint8_t *)        pGuestMSR;

        /** Physical address of the MSR load area (1 page). */
        RTHCPHYS                    pHostMSRPhys;
        /** R0 memory object for the MSR load area (1 page). */
        RTR0MEMOBJ                  pMemObjHostMSR;
        /** Virtual address of the MSR load area (1 page). */
        R0PTRTYPE(uint8_t *)        pHostMSR;
#endif /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

        /* Number of automatically loaded/restored MSRs. */
        uint32_t                    cCachedMSRs;
        uint32_t                    uAlignement;

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
        /** Current guest paging mode (as seen by HWACCMR3PagingModeChanged). */
        PGMMODE                     enmCurrGuestMode;
        /** Previous guest paging mode (as seen by HWACCMR3PagingModeChanged). */
        PGMMODE                     enmPrevGuestMode;
    } vmx;

    struct
    {
        /** R0 memory object for the host VM control block (VMCB). */
        RTR0MEMOBJ                  pMemObjVMCBHost;
        /** Physical address of the host VM control block (VMCB). */
        RTHCPHYS                    pVMCBHostPhys;
        /** Virtual address of the host VM control block (VMCB). */
        R0PTRTYPE(void *)           pVMCBHost;

        /** R0 memory object for the VM control block (VMCB). */
        RTR0MEMOBJ                  pMemObjVMCB;
        /** Physical address of the VM control block (VMCB). */
        RTHCPHYS                    pVMCBPhys;
        /** Virtual address of the VM control block (VMCB). */
        R0PTRTYPE(void *)           pVMCB;

        /** Ring 0 handlers for VT-x. */
        PFNHWACCMSVMVMRUN           pfnVMRun;

        /** R0 memory object for the MSR bitmap (8kb). */
        RTR0MEMOBJ                  pMemObjMSRBitmap;
        /** Physical address of the MSR bitmap (8kb). */
        RTHCPHYS                    pMSRBitmapPhys;
        /** Virtual address of the MSR bitmap. */
        R0PTRTYPE(void *)           pMSRBitmap;
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
        HWACCMPENDINGIO         enmType;
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

    /** Currently shadow paging mode. */
    PGMMODE                 enmShadowMode;

    /** The CPU ID of the CPU currently owning the VMCS. Set in
     * HWACCMR0Enter and cleared in HWACCMR0Leave. */
    RTCPUID                 idEnteredCpu;

    /** To keep track of pending TLB shootdown pages. (SMP guest only) */
    struct
    {
        RTGCPTR             aPages[HWACCM_MAX_TLB_SHOOTDOWN_PAGES];
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
    STAMCOUNTER             StatExitInvpg;
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
    STAMCOUNTER             StatExitCLTS;
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
    STAMCOUNTER             StatExitMTF;
    STAMCOUNTER             StatIntReinject;
    STAMCOUNTER             StatPendingHostIrq;

    STAMCOUNTER             StatFlushPage;
    STAMCOUNTER             StatFlushPageManual;
    STAMCOUNTER             StatFlushPhysPageManual;
    STAMCOUNTER             StatFlushTLB;
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
    STAMCOUNTER             StatTSCInterceptOverFlow;

    STAMCOUNTER             StatExitReasonNPF;
    STAMCOUNTER             StatDRxArmed;
    STAMCOUNTER             StatDRxContextSwitch;
    STAMCOUNTER             StatDRxIOCheck;

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
} HWACCMCPU;
/** Pointer to HWACCM VM instance data. */
typedef HWACCMCPU *PHWACCMCPU;


#ifdef IN_RING0

VMMR0DECL(PHMGLOBLCPUINFO) HWACCMR0GetCurrentCpu(void);
VMMR0DECL(PHMGLOBLCPUINFO) HWACCMR0GetCurrentCpuEx(RTCPUID idCpu);


#ifdef VBOX_STRICT
VMMR0DECL(void) HWACCMDumpRegs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(void) HWACCMR0DumpDescriptor(PCX86DESCHC pDesc, RTSEL Sel, const char *pszMsg);
#else
# define HWACCMDumpRegs(a, b ,c)            do { } while (0)
# define HWACCMR0DumpDescriptor(a, b, c)    do { } while (0)
#endif

# ifdef VBOX_WITH_KERNEL_USING_XMM
DECLASM(int)   hwaccmR0VMXStartVMWrapXMM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu, PFNHWACCMVMXSTARTVM pfnStartVM);
DECLASM(int)   hwaccmR0SVMRunWrapXMM(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu, PFNHWACCMSVMVMRUN pfnVMRun);
# endif

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

RT_C_DECLS_END

#endif

