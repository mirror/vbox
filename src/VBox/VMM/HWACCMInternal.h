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

#if HC_ARCH_BITS == 64
/* Enable 64 bits guest support. */
# define VBOX_ENABLE_64_BITS_GUESTS
#endif

#define HWACCM_VMX_EMULATE_REALMODE

__BEGIN_DECLS


/** @defgroup grp_hwaccm_int       Internal
 * @ingroup grp_hwaccm
 * @internal
 * @{
 */


/**
 * Converts a HWACCM pointer into a VM pointer.
 * @returns Pointer to the VM structure the EM is part of.
 * @param   pHWACCM   Pointer to HWACCM instance data.
 */
#define HWACCM2VM(pHWACCM)  ( (PVM)((char*)pHWACCM - pHWACCM->offVM) )

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
/** @} */


/** Maxium resume loops allowed in ring 0 (safety precaution) */
#define HWACCM_MAX_RESUME_LOOPS             1024

/** Size for the EPT identity page table (1024 4 MB pages to cover the entire address space). */
#define HWACCM_EPT_IDENTITY_PG_TABLE_SIZE   PAGE_SIZE
/** Size of the TSS structure + 2 pages for the IO bitmap + end byte. */
#define HWACCM_VTX_TSS_SIZE                 (sizeof(VBOXTSS) + 2*PAGE_SIZE + 1)
/** Total guest mapped memory needed. */
#define HWACCM_VTX_TOTAL_DEVHEAP_MEM        (HWACCM_EPT_IDENTITY_PG_TABLE_SIZE + HWACCM_VTX_TSS_SIZE)

/** HWACCM SSM version
 */
#define HWACCM_SSM_VERSION                  3

/* Per-cpu information. */
typedef struct
{
    RTCPUID     idCpu;

    RTR0MEMOBJ  pMemObj;
    /* Current ASID (AMD-V)/VPID (Intel) */
    uint32_t    uCurrentASID;
    /* TLB flush count */
    uint32_t    cTLBFlushes;

    /* Set the first time a cpu is used to make sure we start with a clean TLB. */
    bool        fFlushTLB;

    bool        fConfigured;
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
 * HWACCM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct HWACCM
{
    /** Offset to the VM structure.
     * See HWACCM2VM(). */
    RTUINT                      offVM;

    /** Set when we've initialized VMX or SVM. */
    bool                        fInitialized;
    /** Set when we're using VMX/SVN at that moment. */
    bool                        fActive;

    /** Set when hardware acceleration is allowed. */
    bool                        fAllowed;

    /** Set if nested paging is enabled. */
    bool                        fNestedPaging;

    /** Set if nested paging is allowed. */
    bool                        fAllowNestedPaging;

    /** Set if we need to flush the TLB during the world switch. */
    bool                        fForceTLBFlush;

    /** Old style FPU reporting trap mask override performed (optimization) */
    bool                        fFPUOldStyleOverride;

    /** Explicit alignment padding to make 32-bit gcc align u64RegisterMask
     *  naturally. */
    bool                        padding[1];

    /** HWACCM_CHANGED_* flags. */
    RTUINT                      fContextUseFlags;

    /* Id of the last cpu we were executing code on (NIL_RTCPUID for the first time) */
    RTCPUID                     idLastCpu;

    /* TLB flush count */
    RTUINT                      cTLBFlushes;

    /* Current ASID in use by the VM */
    RTUINT                      uCurrentASID;

    /** Maximum ASID allowed. */
    RTUINT                      uMaxASID;

    /** And mask for copying register contents. */
    uint64_t                    u64RegisterMask;
    struct
    {
        /** Set by the ring-0 driver to indicate VMX is supported by the CPU. */
        bool                        fSupported;

        /** Set when we've enabled VMX. */
        bool                        fEnabled;

        /** Set if we can use VMXResume to execute guest code. */
        bool                        fResumeVM;

        /** Set if VPID is supported. */
        bool                        fVPID;

        /** R0 memory object for the VM control structure (VMCS). */
        RTR0MEMOBJ                  pMemObjVMCS;
        /** Physical address of the VM control structure (VMCS). */
        RTHCPHYS                    pVMCSPhys;
        /** Virtual address of the VM control structure (VMCS). */
        R0PTRTYPE(void *)           pVMCS;

        /** Virtual address of the TSS page used for real mode emulation. */
        R3PTRTYPE(PVBOXTSS)         pRealModeTSS;

        /** Virtual address of the identity page table used for real mode emulation in EPT mode. */
        R3PTRTYPE(PX86PD)           pRealModeEPTPageTable;

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
        DECLR0CALLBACKMEMBER(int,  pfnStartVM,(RTHCUINT fResume, PCPUMCTX pCtx));
        DECLR0CALLBACKMEMBER(void, pfnSetupTaggedTLB, (PVM pVM));

#if HC_ARCH_BITS == 32
        uint32_t                    Alignment1;
#endif

        /** Host CR4 value (set by ring-0 VMX init) */
        uint64_t                    hostCR4;

        /** Current VMX_VMCS_CTRL_PROC_EXEC_CONTROLS. */
        uint64_t                    proc_ctls;

        /** Current CR0 mask. */
        uint64_t                    cr0_mask;
        /** Current CR4 mask. */
        uint64_t                    cr4_mask;

        /** Current EPTP. */
        RTHCPHYS                    GCPhysEPTP;

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

        /* Last instruction error */
        uint32_t                    ulLastInstrError;

        /** Current trap mask. */
        uint32_t                    u32TrapMask;

        /** The last known guest paging mode. */
        PGMMODE                     enmCurrGuestMode;

        /** Flush types for invept & invvpid; they depend on capabilities. */
        VMX_FLUSH                   enmFlushPage;
        VMX_FLUSH                   enmFlushContext;

        /** Real-mode emulation state. */
        struct
        {
            struct
            {
                uint32_t                fPending;
                uint32_t                padding4;
                uint64_t                intInfo;
            } Event;

            CPUMSELREGHID               dsHid;
            CPUMSELREGHID               esHid;
            CPUMSELREGHID               fsHid;
            CPUMSELREGHID               gsHid;
            CPUMSELREGHID               ssHid;
            RTSEL                       ds;
            RTSEL                       es;
            RTSEL                       fs;
            RTSEL                       gs;
            RTSEL                       ss;
            RTSEL                       padding5[3];
            uint32_t                    eip;
            uint32_t                    fValid;
        } RealMode;

        struct
        {
            uint64_t                u64VMCSPhys;
            uint32_t                ulVMCSRevision;
        } lasterror;
    } vmx;

    struct
    {
        /** Set by the ring-0 driver to indicate SVM is supported by the CPU. */
        bool                        fSupported;
        /** Set when we've enabled SVM. */
        bool                        fEnabled;
        /** Set if we don't have to flush the TLB on VM entry. */
        bool                        fResumeVM;
        /** Set if erratum 170 affects the AMD cpu. */
        bool                        fAlwaysFlushTLB;

        /** R0 memory object for the VM control block (VMCB). */
        RTR0MEMOBJ                  pMemObjVMCB;
        /** Physical address of the VM control block (VMCB). */
        RTHCPHYS                    pVMCBPhys;
        /** Virtual address of the VM control block (VMCB). */
        R0PTRTYPE(void *)           pVMCB;

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

        /** Ring 0 handlers for VT-x. */
        DECLR0CALLBACKMEMBER(int, pfnVMRun,(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx));

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

    /** Event injection state. */
    struct
    {
        uint32_t                    fPending;
        uint32_t                    errCode;
        uint64_t                    intInfo;
    } Event;

    /** Saved error from detection */
    int32_t                 lLastError;

    /** HWACCMR0Init was run */
    bool                    fHWACCMR0Init;

    /** Currenty shadow paging mode. */
    PGMMODE                 enmShadowMode;

    /** Explicit alignment padding of StatEntry (32-bit g++ again). */
    int32_t                 padding2;

#ifdef VBOX_STRICT
    /** The CPU ID of the CPU currently owning the VMCS. Set in
     * HWACCMR0Enter and cleared in HWACCMR0Leave. */
    RTCPUID                 idEnteredCpu;
# if HC_ARCH_BITS == 32
    RTCPUID                 Alignment0;
# endif
#endif

    STAMPROFILEADV          StatEntry;
    STAMPROFILEADV          StatExit;
    STAMPROFILEADV          StatInGC;

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
    STAMCOUNTER             StatExitCRxWrite;
    STAMCOUNTER             StatExitCRxRead;
    STAMCOUNTER             StatExitDRxWrite;
    STAMCOUNTER             StatExitDRxRead;
    STAMCOUNTER             StatExitCLTS;
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
} HWACCM;
/** Pointer to HWACCM VM instance data. */
typedef HWACCM *PHWACCM;

#ifdef IN_RING0

VMMR0DECL(PHWACCM_CPUINFO) HWACCMR0GetCurrentCpu();

#ifdef VBOX_STRICT
VMMR0DECL(void) HWACCMDumpRegs(PVM pVM, PCPUMCTX pCtx);
VMMR0DECL(void) HWACCMR0DumpDescriptor(PX86DESCHC  Desc, RTSEL Sel, const char *pszMsg);
#else
#define HWACCMDumpRegs(a, b)                do { } while (0)
#define HWACCMR0DumpDescriptor(a, b, c)     do { } while (0)
#endif

/* Dummy callback handlers. */
VMMR0DECL(int) HWACCMR0DummyEnter(PVM pVM, PHWACCM_CPUINFO pCpu);
VMMR0DECL(int) HWACCMR0DummyLeave(PVM pVM, CPUMCTX *pCtx);
VMMR0DECL(int) HWACCMR0DummyEnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys);
VMMR0DECL(int) HWACCMR0DummyDisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);
VMMR0DECL(int) HWACCMR0DummyInitVM(PVM pVM);
VMMR0DECL(int) HWACCMR0DummyTermVM(PVM pVM);
VMMR0DECL(int) HWACCMR0DummySetupVM(PVM pVM);
VMMR0DECL(int) HWACCMR0DummyRunGuestCode(PVM pVM, CPUMCTX *pCtx);
VMMR0DECL(int) HWACCMR0DummySaveHostState(PVM pVM);
VMMR0DECL(int) HWACCMR0DummyLoadGuestState(PVM pVM, CPUMCTX *pCtx);

#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

