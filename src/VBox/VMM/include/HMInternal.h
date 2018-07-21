/* $Id$ */
/** @file
 * HM - Internal header file.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#include <VBox/vmm/hm_svm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/trpm.h>
#include <iprt/memobj.h>
#include <iprt/cpuset.h>
#include <iprt/mp.h>
#include <iprt/avl.h>
#include <iprt/string.h>

#if HC_ARCH_BITS == 32 && defined(RT_OS_DARWIN)
# error "32-bit darwin is no longer supported. Go back to 4.3 or earlier!"
#endif

#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_64_BITS_GUESTS)
/* Enable 64 bits guest support. */
# define VBOX_ENABLE_64_BITS_GUESTS
#endif

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
# define VMX_USE_CACHED_VMCS_ACCESSES
#endif

/** @def HM_PROFILE_EXIT_DISPATCH
 * Enables profiling of the VM exit handler dispatching. */
#if 0 || defined(DOXYGEN_RUNNING)
# define HM_PROFILE_EXIT_DISPATCH
#endif

RT_C_DECLS_BEGIN


/** @defgroup grp_hm_int       Internal
 * @ingroup grp_hm
 * @internal
 * @{
 */

/** @name HM_CHANGED_XXX
 * HM CPU-context changed flags.
 *
 * These flags are used to keep track of which registers and state has been
 * modified since they were imported back into the guest-CPU context.
 *
 * @{
 */
#define HM_CHANGED_HOST_CONTEXT                  UINT64_C(0x0000000000000001)
#define HM_CHANGED_GUEST_RIP                     UINT64_C(0x0000000000000004)
#define HM_CHANGED_GUEST_RFLAGS                  UINT64_C(0x0000000000000008)

#define HM_CHANGED_GUEST_RAX                     UINT64_C(0x0000000000000010)
#define HM_CHANGED_GUEST_RCX                     UINT64_C(0x0000000000000020)
#define HM_CHANGED_GUEST_RDX                     UINT64_C(0x0000000000000040)
#define HM_CHANGED_GUEST_RBX                     UINT64_C(0x0000000000000080)
#define HM_CHANGED_GUEST_RSP                     UINT64_C(0x0000000000000100)
#define HM_CHANGED_GUEST_RBP                     UINT64_C(0x0000000000000200)
#define HM_CHANGED_GUEST_RSI                     UINT64_C(0x0000000000000400)
#define HM_CHANGED_GUEST_RDI                     UINT64_C(0x0000000000000800)
#define HM_CHANGED_GUEST_R8_R15                  UINT64_C(0x0000000000001000)
#define HM_CHANGED_GUEST_GPRS_MASK               UINT64_C(0x0000000000001ff0)

#define HM_CHANGED_GUEST_ES                      UINT64_C(0x0000000000002000)
#define HM_CHANGED_GUEST_CS                      UINT64_C(0x0000000000004000)
#define HM_CHANGED_GUEST_SS                      UINT64_C(0x0000000000008000)
#define HM_CHANGED_GUEST_DS                      UINT64_C(0x0000000000010000)
#define HM_CHANGED_GUEST_FS                      UINT64_C(0x0000000000020000)
#define HM_CHANGED_GUEST_GS                      UINT64_C(0x0000000000040000)
#define HM_CHANGED_GUEST_SREG_MASK               UINT64_C(0x000000000007e000)

#define HM_CHANGED_GUEST_GDTR                    UINT64_C(0x0000000000080000)
#define HM_CHANGED_GUEST_IDTR                    UINT64_C(0x0000000000100000)
#define HM_CHANGED_GUEST_LDTR                    UINT64_C(0x0000000000200000)
#define HM_CHANGED_GUEST_TR                      UINT64_C(0x0000000000400000)
#define HM_CHANGED_GUEST_TABLE_MASK              UINT64_C(0x0000000000780000)

#define HM_CHANGED_GUEST_CR0                     UINT64_C(0x0000000000800000)
#define HM_CHANGED_GUEST_CR2                     UINT64_C(0x0000000001000000)
#define HM_CHANGED_GUEST_CR3                     UINT64_C(0x0000000002000000)
#define HM_CHANGED_GUEST_CR4                     UINT64_C(0x0000000004000000)
#define HM_CHANGED_GUEST_CR_MASK                 UINT64_C(0x0000000007800000)

#define HM_CHANGED_GUEST_APIC_TPR                UINT64_C(0x0000000008000000)
#define HM_CHANGED_GUEST_EFER_MSR                UINT64_C(0x0000000010000000)

#define HM_CHANGED_GUEST_DR0_DR3                 UINT64_C(0x0000000020000000)
#define HM_CHANGED_GUEST_DR6                     UINT64_C(0x0000000040000000)
#define HM_CHANGED_GUEST_DR7                     UINT64_C(0x0000000080000000)
#define HM_CHANGED_GUEST_DR_MASK                 UINT64_C(0x00000000e0000000)

#define HM_CHANGED_GUEST_X87                     UINT64_C(0x0000000100000000)
#define HM_CHANGED_GUEST_SSE_AVX                 UINT64_C(0x0000000200000000)
#define HM_CHANGED_GUEST_OTHER_XSAVE             UINT64_C(0x0000000400000000)
#define HM_CHANGED_GUEST_XCRx                    UINT64_C(0x0000000800000000)

#define HM_CHANGED_GUEST_KERNEL_GS_BASE          UINT64_C(0x0000001000000000)
#define HM_CHANGED_GUEST_SYSCALL_MSRS            UINT64_C(0x0000002000000000)
#define HM_CHANGED_GUEST_SYSENTER_CS_MSR         UINT64_C(0x0000004000000000)
#define HM_CHANGED_GUEST_SYSENTER_EIP_MSR        UINT64_C(0x0000008000000000)
#define HM_CHANGED_GUEST_SYSENTER_ESP_MSR        UINT64_C(0x0000010000000000)
#define HM_CHANGED_GUEST_SYSENTER_MSR_MASK       UINT64_C(0x000001c000000000)
#define HM_CHANGED_GUEST_TSC_AUX                 UINT64_C(0x0000020000000000)
#define HM_CHANGED_GUEST_OTHER_MSRS              UINT64_C(0x0000040000000000)
#define HM_CHANGED_GUEST_ALL_MSRS                (  HM_CHANGED_GUEST_EFER              \
                                                  | HM_CHANGED_GUEST_KERNEL_GS_BASE    \
                                                  | HM_CHANGED_GUEST_SYSCALL_MSRS      \
                                                  | HM_CHANGED_GUEST_SYSENTER_MSR_MASK \
                                                  | HM_CHANGED_GUEST_TSC_AUX           \
                                                  | HM_CHANGED_GUEST_OTHER_MSRS)

#define HM_CHANGED_GUEST_HWVIRT                  UINT64_C(0x0000080000000000)
#define HM_CHANGED_GUEST_MASK                    UINT64_C(0x00000ffffffffffc)

#define HM_CHANGED_KEEPER_STATE_MASK             UINT64_C(0xffff000000000000)

#define HM_CHANGED_VMX_GUEST_XCPT_INTERCEPTS     UINT64_C(0x0001000000000000)
#define HM_CHANGED_VMX_GUEST_AUTO_MSRS           UINT64_C(0x0002000000000000)
#define HM_CHANGED_VMX_GUEST_LAZY_MSRS           UINT64_C(0x0004000000000000)
#define HM_CHANGED_VMX_ENTRY_CTLS                UINT64_C(0x0008000000000000)
#define HM_CHANGED_VMX_EXIT_CTLS                 UINT64_C(0x0010000000000000)
#define HM_CHANGED_VMX_MASK                      UINT64_C(0x001f000000000000)
#define HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE   (  HM_CHANGED_GUEST_DR_MASK \
                                                  | HM_CHANGED_VMX_GUEST_LAZY_MSRS)

#define HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS     UINT64_C(0x0001000000000000)
#define HM_CHANGED_SVM_MASK                      UINT64_C(0x0001000000000000)
#define HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE   HM_CHANGED_GUEST_DR_MASK

#define HM_CHANGED_ALL_GUEST                     (  HM_CHANGED_GUEST_MASK \
                                                  | HM_CHANGED_KEEPER_STATE_MASK)

/** Mask of what state might have changed when IEM raised an exception.
 *  This is a based on IEM_CPUMCTX_EXTRN_XCPT_MASK. */
#define HM_CHANGED_RAISED_XCPT_MASK              (  HM_CHANGED_GUEST_GPRS_MASK  \
                                                  | HM_CHANGED_GUEST_RIP        \
                                                  | HM_CHANGED_GUEST_RFLAGS     \
                                                  | HM_CHANGED_GUEST_SS         \
                                                  | HM_CHANGED_GUEST_CS         \
                                                  | HM_CHANGED_GUEST_CR0        \
                                                  | HM_CHANGED_GUEST_CR3        \
                                                  | HM_CHANGED_GUEST_CR4        \
                                                  | HM_CHANGED_GUEST_APIC_TPR   \
                                                  | HM_CHANGED_GUEST_EFER_MSR   \
                                                  | HM_CHANGED_GUEST_DR7        \
                                                  | HM_CHANGED_GUEST_CR2        \
                                                  | HM_CHANGED_GUEST_SREG_MASK  \
                                                  | HM_CHANGED_GUEST_TABLE_MASK)

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/** Mask of what state might have changed when \#VMEXIT is emulated. */
# define HM_CHANGED_SVM_VMEXIT_MASK              (  HM_CHANGED_GUEST_RSP         \
                                                  | HM_CHANGED_GUEST_RAX         \
                                                  | HM_CHANGED_GUEST_RIP         \
                                                  | HM_CHANGED_GUEST_RFLAGS      \
                                                  | HM_CHANGED_GUEST_CS          \
                                                  | HM_CHANGED_GUEST_SS          \
                                                  | HM_CHANGED_GUEST_DS          \
                                                  | HM_CHANGED_GUEST_ES          \
                                                  | HM_CHANGED_GUEST_GDTR        \
                                                  | HM_CHANGED_GUEST_IDTR        \
                                                  | HM_CHANGED_GUEST_CR_MASK     \
                                                  | HM_CHANGED_GUEST_EFER_MSR    \
                                                  | HM_CHANGED_GUEST_DR6         \
                                                  | HM_CHANGED_GUEST_DR7         \
                                                  | HM_CHANGED_GUEST_OTHER_MSRS  \
                                                  | HM_CHANGED_GUEST_HWVIRT      \
                                                  | HM_CHANGED_SVM_MASK          \
                                                  | HM_CHANGED_GUEST_APIC_TPR)

/** Mask of what state might have changed when \#VMEXIT is emulated. */
# define HM_CHANGED_SVM_VMRUN_MASK               HM_CHANGED_SVM_VMEXIT_MASK
#endif
/** @} */

/** Maximum number of exit reason statistics counters. */
#define MAX_EXITREASON_STAT                        0x100
#define MASK_EXITREASON_STAT                       0xff
#define MASK_INJECT_IRQ_STAT                       0xff

/** Size for the EPT identity page table (1024 4 MB pages to cover the entire address space). */
#define HM_EPT_IDENTITY_PG_TABLE_SIZE               PAGE_SIZE
/** Size of the TSS structure + 2 pages for the IO bitmap + end byte. */
#define HM_VTX_TSS_SIZE                             (sizeof(VBOXTSS) + 2 * PAGE_SIZE + 1)
/** Total guest mapped memory needed. */
#define HM_VTX_TOTAL_DEVHEAP_MEM                    (HM_EPT_IDENTITY_PG_TABLE_SIZE + HM_VTX_TSS_SIZE)


/** @name Macros for enabling and disabling preemption.
 * These are really just for hiding the RTTHREADPREEMPTSTATE and asserting that
 * preemption has already been disabled when there is no context hook.
 * @{ */
#ifdef VBOX_STRICT
# define HM_DISABLE_PREEMPT(a_pVCpu) \
    RTTHREADPREEMPTSTATE PreemptStateInternal = RTTHREADPREEMPTSTATE_INITIALIZER; \
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD) || VMMR0ThreadCtxHookIsEnabled((a_pVCpu))); \
    RTThreadPreemptDisable(&PreemptStateInternal)
#else
# define HM_DISABLE_PREEMPT(a_pVCpu) \
    RTTHREADPREEMPTSTATE PreemptStateInternal = RTTHREADPREEMPTSTATE_INITIALIZER; \
    RTThreadPreemptDisable(&PreemptStateInternal)
#endif /* VBOX_STRICT */
#define HM_RESTORE_PREEMPT()    do { RTThreadPreemptRestore(&PreemptStateInternal); } while(0)
/** @} */


/** @name HM saved state versions.
 * @{
 */
#define HM_SAVED_STATE_VERSION                         HM_SAVED_STATE_VERSION_SVM_NESTED_HWVIRT
#define HM_SAVED_STATE_VERSION_SVM_NESTED_HWVIRT       6
#define HM_SAVED_STATE_VERSION_TPR_PATCHING            5
#define HM_SAVED_STATE_VERSION_NO_TPR_PATCHING         4
#define HM_SAVED_STATE_VERSION_2_0_X                   3
/** @} */


/**
 * Global per-cpu information. (host)
 */
typedef struct HMGLOBALCPUINFO
{
    /** The CPU ID. */
    RTCPUID             idCpu;
    /** The VM_HSAVE_AREA (AMD-V) / VMXON region (Intel) memory backing. */
    RTR0MEMOBJ          hMemObj;
    /** The physical address of the first page in hMemObj (it's a
     *  physcially contigous allocation if it spans multiple pages). */
    RTHCPHYS            HCPhysMemObj;
    /** The address of the memory (for pfnEnable). */
    void               *pvMemObj;
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
    bool volatile       fInUse;
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    /** Nested-guest union (put data common to SVM/VMX outside the union). */
    union
    {
        /** Nested-guest SVM data. */
        struct
        {
            /** The active nested-guest MSR permission bitmap memory backing. */
            RTR0MEMOBJ          hNstGstMsrpm;
            /** The physical address of the first page in hNstGstMsrpm (physcially
             *  contiguous allocation). */
            RTHCPHYS            HCPhysNstGstMsrpm;
            /** The address of the active nested-guest MSRPM. */
            void               *pvNstGstMsrpm;
        } svm;
        /** @todo Nested-VMX. */
    } n;
#endif
} HMGLOBALCPUINFO;
/** Pointer to the per-cpu global information. */
typedef HMGLOBALCPUINFO *PHMGLOBALCPUINFO;

typedef enum
{
    HMPENDINGIO_INVALID = 0,
    HMPENDINGIO_PORT_READ,
    /* not implemented: HMPENDINGIO_STRING_READ, */
    /* not implemented: HMPENDINGIO_STRING_WRITE, */
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
    HMTPRINSTR              enmType;
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
/** Pointer to a const HMTPRPATCH. */
typedef const HMTPRPATCH *PCHMTPRPATCH;


/**
 * Makes a HMEXITSTAT::uKey value from a program counter and an exit code.
 *
 * @returns 64-bit key
 * @param   a_uPC           The RIP + CS.BASE value of the exit.
 * @param   a_uExit         The exit code.
 * @todo    Add CPL?
 */
#define HMEXITSTAT_MAKE_KEY(a_uPC, a_uExit) (((a_uPC) & UINT64_C(0x0000ffffffffffff)) | (uint64_t)(a_uExit) << 48)

typedef struct HMEXITINFO
{
    /** See HMEXITSTAT_MAKE_KEY(). */
    uint64_t                uKey;
    /** Number of recent hits (depreciates with time). */
    uint32_t volatile       cHits;
    /** The age + lock. */
    uint16_t volatile       uAge;
    /** Action or action table index. */
    uint16_t                iAction;
} HMEXITINFO;
AssertCompileSize(HMEXITINFO, 16); /* Lots of these guys, so don't add any unnecessary stuff! */

typedef struct HMEXITHISTORY
{
    /** The exit timestamp. */
    uint64_t                uTscExit;
    /** The index of the corresponding HMEXITINFO entry.
     * UINT32_MAX if none (too many collisions, race, whatever). */
    uint32_t                iExitInfo;
    /** Figure out later, needed for padding now. */
    uint32_t                uSomeClueOrSomething;
} HMEXITHISTORY;

/**
 * Switcher function, HC to the special 64-bit RC.
 *
 * @param   pVM             The cross context VM structure.
 * @param   offCpumVCpu     Offset from pVM->cpum to pVM->aCpus[idCpu].cpum.
 * @returns Return code indicating the action to take.
 */
typedef DECLCALLBACK(int) FNHMSWITCHERHC(PVM pVM, uint32_t offCpumVCpu);
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
    /** Set if nested paging is enabled. */
    bool                        fNestedPaging;
    /** Set if nested paging is allowed. */
    bool                        fAllowNestedPaging;
    /** Set if large pages are enabled (requires nested paging). */
    bool                        fLargePages;
    /** Set if we can support 64-bit guests or not. */
    bool                        fAllow64BitGuests;
    /** Set when TPR patching is allowed. */
    bool                        fTprPatchingAllowed;
    /** Set when we initialize VT-x or AMD-V once for all CPUs. */
    bool                        fGlobalInit;
    /** Set when TPR patching is active. */
    bool                        fTPRPatchingActive;
    /** Set when the debug facility has breakpoints/events enabled that requires
     *  us to use the debug execution loop in ring-0. */
    bool                        fUseDebugLoop;
    /** Set if hardware APIC virtualization is enabled. */
    bool                        fVirtApicRegs;
    /** Set if posted interrupt processing is enabled. */
    bool                        fPostedIntrs;
    /** Set if indirect branch prediction barrier on VM exit. */
    bool                        fIbpbOnVmExit;
    /** Set if indirect branch prediction barrier on VM entry. */
    bool                        fIbpbOnVmEntry;
    /** Set if host manages speculation control settings. */
    bool                        fSpecCtrlByHost;
    /** Explicit padding. */
    bool                        afPadding[2];

    /** Maximum ASID allowed. */
    uint32_t                    uMaxAsid;
    /** The maximum number of resumes loops allowed in ring-0 (safety precaution).
     * This number is set much higher when RTThreadPreemptIsPending is reliable. */
    uint32_t                    cMaxResumeLoops;

    /** Host kernel flags that HM might need to know (SUPKERNELFEATURES_XXX). */
    uint32_t                    fHostKernelFeatures;

    /** Size of the guest patch memory block. */
    uint32_t                    cbGuestPatchMem;
    /** Guest allocated memory for patching purposes. */
    RTGCPTR                     pGuestPatchMem;
    /** Current free pointer inside the patch block. */
    RTGCPTR                     pFreeGuestPatchMem;

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    /** 32 to 64 bits switcher entrypoint. */
    R0PTRTYPE(PFNHMSWITCHERHC)  pfnHost32ToGuest64R0;
    RTR0PTR                     pvR0Alignment0;
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
        /** Set if unrestricted guest execution is in use (real and protected mode without paging). */
        bool                        fUnrestrictedGuest;
        /** Set if unrestricted guest execution is allowed to be used. */
        bool                        fAllowUnrestricted;
        /** Set if the preemption timer is in use or not. */
        bool                        fUsePreemptTimer;
        /** The shift mask employed by the VMX-Preemption timer. */
        uint8_t                     cPreemptTimerShift;

        /** Virtual address of the TSS page used for real mode emulation. */
        R3PTRTYPE(PVBOXTSS)         pRealModeTSS;
        /** Virtual address of the identity page table used for real mode and protected mode without paging emulation in EPT mode. */
        R3PTRTYPE(PX86PD)           pNonPagingModeEPTPageTable;

        /** Physical address of the APIC-access page. */
        RTHCPHYS                    HCPhysApicAccess;
        /** R0 memory object for the APIC-access page. */
        RTR0MEMOBJ                  hMemObjApicAccess;
        /** Virtual address of the APIC-access page. */
        R0PTRTYPE(uint8_t *)        pbApicAccess;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        RTHCPHYS                    HCPhysScratch;
        RTR0MEMOBJ                  hMemObjScratch;
        R0PTRTYPE(uint8_t *)        pbScratch;
#endif

        /** Tagged-TLB flush type. */
        VMXTLBFLUSHTYPE             enmTlbFlushType;
        /** Flush type to use for INVEPT. */
        VMXTLBFLUSHEPT              enmTlbFlushEpt;
        /** Flush type to use for INVVPID. */
        VMXTLBFLUSHVPID             enmTlbFlushVpid;

        /** Pause-loop exiting (PLE) gap in ticks. */
        uint32_t                    cPleGapTicks;
        /** Pause-loop exiting (PLE) window in ticks. */
        uint32_t                    cPleWindowTicks;
        uint32_t                    u32Alignment0;

        /** Host CR4 value (set by ring-0 VMX init) */
        uint64_t                    u64HostCr4;
        /** Host SMM monitor control (set by ring-0 VMX init) */
        uint64_t                    u64HostSmmMonitorCtl;
        /** Host EFER value (set by ring-0 VMX init) */
        uint64_t                    u64HostEfer;
        /** Whether the CPU supports VMCS fields for swapping EFER. */
        bool                        fSupportsVmcsEfer;
        uint8_t                     u8Alignment2[7];

        /** VMX MSR values. */
        VMXMSRS                     Msrs;

        /** Host-physical address for a failing VMXON instruction. */
        RTHCPHYS                    HCPhysVmxEnableError;
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
        /** Whether to use virtualized VMSAVE/VMLOAD feature. */
        bool                        fVirtVmsaveVmload;
        /** Whether to use virtual GIF feature. */
        bool                        fVGif;
        uint8_t                     u8Alignment0[2];

        /** Physical address of the IO bitmap (12kb). */
        RTHCPHYS                    HCPhysIOBitmap;
        /** R0 memory object for the IO bitmap (12kb). */
        RTR0MEMOBJ                  hMemObjIOBitmap;
        /** Virtual address of the IO bitmap. */
        R0PTRTYPE(void *)           pvIOBitmap;

        /* HWCR MSR (for diagnostics) */
        uint64_t                    u64MsrHwcr;

        /** SVM revision. */
        uint32_t                    u32Rev;
        /** SVM feature bits from cpuid 0x8000000a */
        uint32_t                    u32Features;

        /** Pause filter counter. */
        uint16_t                    cPauseFilter;
        /** Pause filter treshold in ticks. */
        uint16_t                    cPauseFilterThresholdTicks;
        uint32_t                    u32Alignment0;
    } svm;

    /**
     * AVL tree with all patches (active or disabled) sorted by guest instruction
     * address.
     */
    AVLOU32TREE                     PatchTree;
    uint32_t                        cPatches;
    HMTPRPATCH                      aPatches[64];

    /** Last recorded error code during HM ring-0 init. */
    int32_t                 rcInit;

    /** HMR0Init was run */
    bool                    fHMR0Init;
    bool                    u8Alignment1[3];

    STAMCOUNTER             StatTprPatchSuccess;
    STAMCOUNTER             StatTprPatchFailure;
    STAMCOUNTER             StatTprReplaceSuccessCr8;
    STAMCOUNTER             StatTprReplaceSuccessVmc;
    STAMCOUNTER             StatTprReplaceFailure;
} HM;
/** Pointer to HM VM instance data. */
typedef HM *PHM;

AssertCompileMemberAlignment(HM, StatTprPatchSuccess, 8);

/* Maximum number of cached entries. */
#define VMCSCACHE_MAX_ENTRY                             128

/**
 * Structure for storing read and write VMCS actions.
 */
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
#ifdef VBOX_STRICT
    struct
    {
        RTHCPHYS    HCPhysCpuPage;
        RTHCPHYS    HCPhysVmcs;
        RTGCPTR     pCache;
        RTGCPTR     pCtx;
    } TestIn;
    struct
    {
        RTHCPHYS    HCPhysVmcs;
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
AssertCompileSizeAlignment(VMCSCACHE, 8);

/**
 * VMX StartVM function.
 *
 * @returns VBox status code (no informational stuff).
 * @param   fResume     Whether to use VMRESUME (true) or VMLAUNCH (false).
 * @param   pCtx        The CPU register context.
 * @param   pCache      The VMCS cache.
 * @param   pVM         Pointer to the cross context VM structure.
 * @param   pVCpu       Pointer to the cross context per-CPU structure.
 */
typedef DECLCALLBACK(int) FNHMVMXSTARTVM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu);
/** Pointer to a VMX StartVM function. */
typedef R0PTRTYPE(FNHMVMXSTARTVM *) PFNHMVMXSTARTVM;

/** SVM VMRun function. */
typedef DECLCALLBACK(int) FNHMSVMVMRUN(RTHCPHYS pVmcbHostPhys, RTHCPHYS pVmcbPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);
/** Pointer to a SVM VMRun function. */
typedef R0PTRTYPE(FNHMSVMVMRUN *) PFNHMSVMVMRUN;

/**
 * HM VMCPU Instance data.
 *
 * Note! If you change members of this struct, make sure to check if the
 * assembly counterpart in HMInternal.mac needs to be updated as well.
 */
typedef struct HMCPU
{
    /** Set when the TLB has been checked until we return from the world switch. */
    bool volatile               fCheckedTLBFlush;
    /** Set if we need to flush the TLB during the world switch. */
    bool                        fForceTLBFlush;
    /** Set when we're using VT-x or AMD-V at that moment. */
    bool                        fActive;
    /** Whether we've completed the inner HM leave function. */
    bool                        fLeaveDone;
    /** Whether we're using the hyper DR7 or guest DR7. */
    bool                        fUsingHyperDR7;
    /** Set if XCR0 needs to be saved/restored when entering/exiting guest code
     *  execution. */
    bool                        fLoadSaveGuestXcr0;

    /** Whether we should use the debug loop because of single stepping or special
     *  debug breakpoints / events are armed. */
    bool                        fUseDebugLoop;
    /** Whether we are currently executing in the debug loop.
     *  Mainly for assertions. */
    bool                        fUsingDebugLoop;
    /** Set if we using the debug loop and wish to intercept RDTSC. */
    bool                        fDebugWantRdTscExit;
    /** Whether we're executing a single instruction. */
    bool                        fSingleInstruction;
    /** Set if we need to clear the trap flag because of single stepping. */
    bool                        fClearTrapFlag;

    /** Whether \#UD needs to be intercepted (required by certain GIM providers). */
    bool                        fGIMTrapXcptUD;
    uint8_t                     u8Alignment0[4];

    /** World switch exit counter. */
    uint32_t volatile           cWorldSwitchExits;
    /** The last CPU we were executing code on (NIL_RTCPUID for the first time). */
    RTCPUID                     idLastCpu;
    /** TLB flush count. */
    uint32_t                    cTlbFlushes;
    /** Current ASID in use by the VM. */
    uint32_t                    uCurrentAsid;
    /** An additional error code used for some gurus. */
    uint32_t                    u32HMError;
    /** The last exit-to-ring-3 reason. */
    int32_t                     rcLastExitToR3;
    /** CPU-context changed flags (see HM_CHANGED_xxx). */
    uint64_t                    fCtxChanged;
    /** Host's TSC_AUX MSR (used when RDTSCP doesn't cause VM-exits). */
    uint64_t                    u64HostTscAux;

    struct
    {
        /** Ring 0 handlers for VT-x. */
        PFNHMVMXSTARTVM             pfnStartVM;
#if HC_ARCH_BITS == 32
        uint32_t                    u32Alignment0;
#endif
        /** Current VMX_VMCS32_CTRL_PIN_EXEC. */
        uint32_t                    u32PinCtls;
        /** Current VMX_VMCS32_CTRL_PROC_EXEC. */
        uint32_t                    u32ProcCtls;
        /** Current VMX_VMCS32_CTRL_PROC_EXEC2. */
        uint32_t                    u32ProcCtls2;
        /** Current VMX_VMCS32_CTRL_EXIT. */
        uint32_t                    u32ExitCtls;
        /** Current VMX_VMCS32_CTRL_ENTRY. */
        uint32_t                    u32EntryCtls;

        /** Current CR0 mask. */
        uint32_t                    u32Cr0Mask;
        /** Current CR4 mask. */
        uint32_t                    u32Cr4Mask;
        /** Current exception bitmap. */
        uint32_t                    u32XcptBitmap;
        /** The updated-guest-state mask. */
        uint32_t                    au32Alignment0[2];

        /** Physical address of the VM control structure (VMCS). */
        RTHCPHYS                    HCPhysVmcs;
        /** R0 memory object for the VM control structure (VMCS). */
        RTR0MEMOBJ                  hMemObjVmcs;
        /** Virtual address of the VM control structure (VMCS). */
        R0PTRTYPE(void *)           pvVmcs;

        /** Physical address of the virtual APIC page for TPR caching. */
        RTHCPHYS                    HCPhysVirtApic;
        /** Padding. */
        R0PTRTYPE(void *)           pvAlignment0;
        /** Virtual address of the virtual APIC page for TPR caching. */
        R0PTRTYPE(uint8_t *)        pbVirtApic;

        /** Physical address of the MSR bitmap. */
        RTHCPHYS                    HCPhysMsrBitmap;
        /** R0 memory object for the MSR bitmap. */
        RTR0MEMOBJ                  hMemObjMsrBitmap;
        /** Virtual address of the MSR bitmap. */
        R0PTRTYPE(void *)           pvMsrBitmap;

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

        /** Current EPTP. */
        RTHCPHYS                    HCPhysEPTP;

        /** Number of guest/host MSR pairs in the auto-load/store area. */
        uint32_t                    cMsrs;
        /** Whether the host MSR values are up-to-date in the auto-load/store area. */
        bool                        fUpdatedHostMsrs;
        uint8_t                     u8Alignment0[3];

        /** Host LSTAR MSR value to restore lazily while leaving VT-x. */
        uint64_t                    u64HostLStarMsr;
        /** Host STAR MSR value to restore lazily while leaving VT-x. */
        uint64_t                    u64HostStarMsr;
        /** Host SF_MASK MSR value to restore lazily while leaving VT-x. */
        uint64_t                    u64HostSFMaskMsr;
        /** Host KernelGS-Base MSR value to restore lazily while leaving VT-x. */
        uint64_t                    u64HostKernelGSBaseMsr;
        /** A mask of which MSRs have been swapped and need restoration. */
        uint32_t                    fLazyMsrs;
        uint32_t                    u32Alignment2;

        /** The cached APIC-base MSR used for identifying when to map the HC physical APIC-access page. */
        uint64_t                    u64MsrApicBase;
        /** Last use TSC offset value. (cached) */
        uint64_t                    u64TscOffset;

        /** VMCS cache. */
        VMCSCACHE                   VMCSCache;

        /** Real-mode emulation state. */
        struct
        {
            X86DESCATTR             AttrCS;
            X86DESCATTR             AttrDS;
            X86DESCATTR             AttrES;
            X86DESCATTR             AttrFS;
            X86DESCATTR             AttrGS;
            X86DESCATTR             AttrSS;
            X86EFLAGS               Eflags;
            uint32_t                fRealOnV86Active;
        } RealMode;

        /** VT-x error-reporting (mainly for ring-3 propagation). */
        struct
        {
            uint64_t                u64VMCSPhys;
            uint32_t                u32VMCSRevision;
            uint32_t                u32InstrError;
            uint32_t                u32ExitReason;
            RTCPUID                 idEnteredCpu;
            RTCPUID                 idCurrentCpu;
            uint32_t                u32Alignment0;
        } LastError;

        /** Current state of the VMCS. */
        uint32_t                    uVmcsState;
        /** Which host-state bits to restore before being preempted. */
        uint32_t                    fRestoreHostFlags;
        /** The host-state restoration structure. */
        VMXRESTOREHOST              RestoreHost;

        /** Set if guest was executing in real mode (extra checks). */
        bool                        fWasInRealMode;
        /** Set if guest switched to 64-bit mode on a 32-bit host. */
        bool                        fSwitchedTo64on32;

        uint8_t                     u8Alignment1[6];
    } vmx;

    struct
    {
        /** Ring 0 handlers for VT-x. */
        PFNHMSVMVMRUN               pfnVMRun;
#if HC_ARCH_BITS == 32
        uint32_t                    u32Alignment0;
#endif

        /** Physical address of the host VMCB which holds additional host-state. */
        RTHCPHYS                    HCPhysVmcbHost;
        /** R0 memory object for the host VMCB which holds additional host-state. */
        RTR0MEMOBJ                  hMemObjVmcbHost;
        /** Padding. */
        R0PTRTYPE(void *)           pvPadding;

        /** Physical address of the guest VMCB. */
        RTHCPHYS                    HCPhysVmcb;
        /** R0 memory object for the guest VMCB. */
        RTR0MEMOBJ                  hMemObjVmcb;
        /** Pointer to the guest VMCB. */
        R0PTRTYPE(PSVMVMCB)         pVmcb;

        /** Physical address of the MSR bitmap (8 KB). */
        RTHCPHYS                    HCPhysMsrBitmap;
        /** R0 memory object for the MSR bitmap (8 KB). */
        RTR0MEMOBJ                  hMemObjMsrBitmap;
        /** Pointer to the MSR bitmap. */
        R0PTRTYPE(void *)           pvMsrBitmap;

        /** Whether VTPR with V_INTR_MASKING set is in effect, indicating
         *  we should check if the VTPR changed on every VM-exit. */
        bool                        fSyncVTpr;
        uint8_t                     u8Alignment0[7];

        /** Cache of the nested-guest's VMCB fields that we modify in order to run the
         *  nested-guest using AMD-V. This will be restored on \#VMEXIT. */
        SVMNESTEDVMCBCACHE          NstGstVmcbCache;
    } svm;

    /** Event injection state. */
    struct
    {
        uint32_t                    fPending;
        uint32_t                    u32ErrCode;
        uint32_t                    cbInstr;
        uint32_t                    u32Padding; /**< Explicit alignment padding. */
        uint64_t                    u64IntInfo;
        RTGCUINTPTR                 GCPtrFaultAddress;
    } Event;

    /** The PAE PDPEs used with Nested Paging (only valid when
     *  VMCPU_FF_HM_UPDATE_PAE_PDPES is set). */
    X86PDPE                 aPdpes[4];

    /** Current shadow paging mode for updating CR4. */
    PGMMODE                 enmShadowMode;

    /** The CPU ID of the CPU currently owning the VMCS. Set in
     * HMR0Enter and cleared in HMR0Leave. */
    RTCPUID                 idEnteredCpu;

    /** For saving stack space, the disassembler state is allocated here instead of
     * on the stack. */
    DISCPUSTATE             DisState;

    STAMPROFILEADV          StatEntry;
    STAMPROFILEADV          StatPreExit;
    STAMPROFILEADV          StatExitHandling;
    STAMPROFILEADV          StatExitIO;
    STAMPROFILEADV          StatExitMovCRx;
    STAMPROFILEADV          StatExitXcptNmi;
    STAMPROFILEADV          StatImportGuestState;
    STAMPROFILEADV          StatExportGuestState;
    STAMPROFILEADV          StatLoadGuestFpuState;
    STAMPROFILEADV          StatInGC;
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    STAMPROFILEADV          StatWorldSwitch3264;
#endif
    STAMPROFILEADV          StatPoke;
    STAMPROFILEADV          StatSpinPoke;
    STAMPROFILEADV          StatSpinPokeFailed;

    STAMCOUNTER             StatInjectInterrupt;
    STAMCOUNTER             StatInjectXcpt;
    STAMCOUNTER             StatInjectPendingReflect;
    STAMCOUNTER             StatInjectPendingInterpret;

    STAMCOUNTER             StatExitAll;
    STAMCOUNTER             StatExitShadowNM;
    STAMCOUNTER             StatExitGuestNM;
    STAMCOUNTER             StatExitShadowPF;       /**< Misleading, currently used for MMIO \#PFs as well. */
    STAMCOUNTER             StatExitShadowPFEM;
    STAMCOUNTER             StatExitGuestPF;
    STAMCOUNTER             StatExitGuestUD;
    STAMCOUNTER             StatExitGuestSS;
    STAMCOUNTER             StatExitGuestNP;
    STAMCOUNTER             StatExitGuestTS;
    STAMCOUNTER             StatExitGuestGP;
    STAMCOUNTER             StatExitGuestDE;
    STAMCOUNTER             StatExitGuestDB;
    STAMCOUNTER             StatExitGuestMF;
    STAMCOUNTER             StatExitGuestBP;
    STAMCOUNTER             StatExitGuestXF;
    STAMCOUNTER             StatExitGuestXcpUnk;
    STAMCOUNTER             StatExitCli;
    STAMCOUNTER             StatExitSti;
    STAMCOUNTER             StatExitPushf;
    STAMCOUNTER             StatExitPopf;
    STAMCOUNTER             StatExitIret;
    STAMCOUNTER             StatExitInt;
    STAMCOUNTER             StatExitHlt;
    STAMCOUNTER             StatExitDRxWrite;
    STAMCOUNTER             StatExitDRxRead;
    STAMCOUNTER             StatExitCR0Read;
    STAMCOUNTER             StatExitCR2Read;
    STAMCOUNTER             StatExitCR3Read;
    STAMCOUNTER             StatExitCR4Read;
    STAMCOUNTER             StatExitCR8Read;
    STAMCOUNTER             StatExitCR0Write;
    STAMCOUNTER             StatExitCR2Write;
    STAMCOUNTER             StatExitCR3Write;
    STAMCOUNTER             StatExitCR4Write;
    STAMCOUNTER             StatExitCR8Write;
    STAMCOUNTER             StatExitRdmsr;
    STAMCOUNTER             StatExitWrmsr;
    STAMCOUNTER             StatExitClts;
    STAMCOUNTER             StatExitXdtrAccess;
    STAMCOUNTER             StatExitMwait;
    STAMCOUNTER             StatExitMonitor;
    STAMCOUNTER             StatExitLmsw;
    STAMCOUNTER             StatExitIOWrite;
    STAMCOUNTER             StatExitIORead;
    STAMCOUNTER             StatExitIOStringWrite;
    STAMCOUNTER             StatExitIOStringRead;
    STAMCOUNTER             StatExitIntWindow;
    STAMCOUNTER             StatExitExtInt;
    STAMCOUNTER             StatExitHostNmiInGC;
    STAMCOUNTER             StatExitPreemptTimer;
    STAMCOUNTER             StatExitTprBelowThreshold;
    STAMCOUNTER             StatExitTaskSwitch;
    STAMCOUNTER             StatExitMtf;
    STAMCOUNTER             StatExitApicAccess;
    STAMCOUNTER             StatExitReasonNpf;

    STAMCOUNTER             StatNestedExitReasonNpf;

    STAMCOUNTER             StatFlushPage;
    STAMCOUNTER             StatFlushPageManual;
    STAMCOUNTER             StatFlushPhysPageManual;
    STAMCOUNTER             StatFlushTlb;
    STAMCOUNTER             StatFlushTlbManual;
    STAMCOUNTER             StatFlushTlbWorldSwitch;
    STAMCOUNTER             StatNoFlushTlbWorldSwitch;
    STAMCOUNTER             StatFlushEntire;
    STAMCOUNTER             StatFlushAsid;
    STAMCOUNTER             StatFlushNestedPaging;
    STAMCOUNTER             StatFlushTlbInvlpgVirt;
    STAMCOUNTER             StatFlushTlbInvlpgPhys;
    STAMCOUNTER             StatTlbShootdown;
    STAMCOUNTER             StatTlbShootdownFlush;

    STAMCOUNTER             StatSwitchPendingHostIrq;
    STAMCOUNTER             StatSwitchTprMaskedIrq;
    STAMCOUNTER             StatSwitchGuestIrq;
    STAMCOUNTER             StatSwitchHmToR3FF;
    STAMCOUNTER             StatSwitchExitToR3;
    STAMCOUNTER             StatSwitchLongJmpToR3;
    STAMCOUNTER             StatSwitchMaxResumeLoops;
    STAMCOUNTER             StatSwitchHltToR3;
    STAMCOUNTER             StatSwitchApicAccessToR3;
    STAMCOUNTER             StatSwitchPreempt;
    STAMCOUNTER             StatSwitchPreemptExportHostState;

    STAMCOUNTER             StatTscParavirt;
    STAMCOUNTER             StatTscOffset;
    STAMCOUNTER             StatTscIntercept;

    STAMCOUNTER             StatDRxArmed;
    STAMCOUNTER             StatDRxContextSwitch;
    STAMCOUNTER             StatDRxIoCheck;

    STAMCOUNTER             StatExportMinimal;
    STAMCOUNTER             StatExportFull;
    STAMCOUNTER             StatLoadGuestFpu;

    STAMCOUNTER             StatVmxCheckBadRmSelBase;
    STAMCOUNTER             StatVmxCheckBadRmSelLimit;
    STAMCOUNTER             StatVmxCheckRmOk;
    STAMCOUNTER             StatVmxCheckBadSel;
    STAMCOUNTER             StatVmxCheckBadRpl;
    STAMCOUNTER             StatVmxCheckBadLdt;
    STAMCOUNTER             StatVmxCheckBadTr;
    STAMCOUNTER             StatVmxCheckPmOk;

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    STAMCOUNTER             StatFpu64SwitchBack;
    STAMCOUNTER             StatDebug64SwitchBack;
#endif
#ifdef VBOX_WITH_STATISTICS
    R3PTRTYPE(PSTAMCOUNTER) paStatExitReason;
    R0PTRTYPE(PSTAMCOUNTER) paStatExitReasonR0;
    R3PTRTYPE(PSTAMCOUNTER) paStatInjectedIrqs;
    R0PTRTYPE(PSTAMCOUNTER) paStatInjectedIrqsR0;
    R3PTRTYPE(PSTAMCOUNTER) paStatNestedExitReason;
    R0PTRTYPE(PSTAMCOUNTER) paStatNestedExitReasonR0;
#endif
#ifdef HM_PROFILE_EXIT_DISPATCH
    STAMPROFILEADV          StatExitDispatch;
#endif
} HMCPU;
/** Pointer to HM VMCPU instance data. */
typedef HMCPU *PHMCPU;
AssertCompileMemberAlignment(HMCPU, cWorldSwitchExits, 4);
AssertCompileMemberAlignment(HMCPU, fCtxChanged, 8);
AssertCompileMemberAlignment(HMCPU, vmx, 8);
AssertCompileMemberAlignment(HMCPU, svm, 8);
AssertCompileMemberAlignment(HMCPU, Event, 8);

#ifdef IN_RING0
VMMR0_INT_DECL(PHMGLOBALCPUINFO) hmR0GetCurrentCpu(void);
VMMR0_INT_DECL(int) hmR0EnterCpu(PVMCPU pVCpu);

# ifdef VBOX_STRICT
VMMR0_INT_DECL(void) hmR0DumpRegs(PVMCPU pVCpu);
VMMR0_INT_DECL(void) hmR0DumpDescriptor(PCX86DESCHC pDesc, RTSEL Sel, const char *pszMsg);
# endif

# ifdef VBOX_WITH_KERNEL_USING_XMM
DECLASM(int) hmR0VMXStartVMWrapXMM(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu, PFNHMVMXSTARTVM pfnStartVM);
DECLASM(int) hmR0SVMRunWrapXMM(RTHCPHYS pVmcbHostPhys, RTHCPHYS pVmcbPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu, PFNHMSVMVMRUN pfnVMRun);
# endif
#endif /* IN_RING0 */

int hmSvmEmulateMovTpr(PVMCPU pVCpu);

/** @} */

RT_C_DECLS_END

#endif

