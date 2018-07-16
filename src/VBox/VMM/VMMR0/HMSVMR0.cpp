/* $Id$ */
/** @file
 * HM SVM (AMD-V) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#define VMCPU_INCL_CPUM_GST_CTX
#include <iprt/asm-amd64-x86.h>
#include <iprt/thread.h>

#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/gim.h>
#include <VBox/vmm/apic.h>
#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include "HMSVMR0.h"
#include "dtrace/VBoxVMM.h"

#ifdef DEBUG_ramshankar
# define HMSVM_SYNC_FULL_GUEST_STATE
# define HMSVM_ALWAYS_TRAP_ALL_XCPTS
# define HMSVM_ALWAYS_TRAP_PF
# define HMSVM_ALWAYS_TRAP_TASK_SWITCH
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_STATISTICS
# define HMSVM_EXITCODE_STAM_COUNTER_INC(u64ExitCode) do { \
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitAll); \
        if ((u64ExitCode) == SVM_EXIT_NPF) \
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitReasonNpf); \
        else \
            STAM_COUNTER_INC(&pVCpu->hm.s.paStatExitReasonR0[(u64ExitCode) & MASK_EXITREASON_STAT]); \
        } while (0)

# ifdef VBOX_WITH_NESTED_HWVIRT_SVM
#  define HMSVM_NESTED_EXITCODE_STAM_COUNTER_INC(u64ExitCode) do { \
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitAll); \
        if ((u64ExitCode) == SVM_EXIT_NPF) \
            STAM_COUNTER_INC(&pVCpu->hm.s.StatNestedExitReasonNpf); \
        else \
            STAM_COUNTER_INC(&pVCpu->hm.s.paStatNestedExitReasonR0[(u64ExitCode) & MASK_EXITREASON_STAT]); \
        } while (0)
# endif
#else
# define HMSVM_EXITCODE_STAM_COUNTER_INC(u64ExitCode)           do { } while (0)
# ifdef VBOX_WITH_NESTED_HWVIRT_SVM
#  define HMSVM_NESTED_EXITCODE_STAM_COUNTER_INC(u64ExitCode)   do { } while (0)
# endif
#endif /* !VBOX_WITH_STATISTICS */

/** If we decide to use a function table approach this can be useful to
 *  switch to a "static DECLCALLBACK(int)". */
#define HMSVM_EXIT_DECL                 static int

/**
 * Subset of the guest-CPU state that is kept by SVM R0 code while executing the
 * guest using hardware-assisted SVM.
 *
 * This excludes state like TSC AUX, GPRs (other than RSP, RAX) which are always
 * are swapped and restored across the world-switch and also registers like
 * EFER, PAT MSR etc. which cannot be modified by the guest without causing a
 * \#VMEXIT.
 */
#define HMSVM_CPUMCTX_EXTRN_ALL         (  CPUMCTX_EXTRN_RIP            \
                                         | CPUMCTX_EXTRN_RFLAGS         \
                                         | CPUMCTX_EXTRN_RAX            \
                                         | CPUMCTX_EXTRN_RSP            \
                                         | CPUMCTX_EXTRN_SREG_MASK      \
                                         | CPUMCTX_EXTRN_CR0            \
                                         | CPUMCTX_EXTRN_CR2            \
                                         | CPUMCTX_EXTRN_CR3            \
                                         | CPUMCTX_EXTRN_TABLE_MASK     \
                                         | CPUMCTX_EXTRN_DR6            \
                                         | CPUMCTX_EXTRN_DR7            \
                                         | CPUMCTX_EXTRN_KERNEL_GS_BASE \
                                         | CPUMCTX_EXTRN_SYSCALL_MSRS   \
                                         | CPUMCTX_EXTRN_SYSENTER_MSRS  \
                                         | CPUMCTX_EXTRN_HWVIRT         \
                                         | CPUMCTX_EXTRN_HM_SVM_MASK)

/**
 * Subset of the guest-CPU state that is shared between the guest and host.
 */
#define HMSVM_CPUMCTX_SHARED_STATE      CPUMCTX_EXTRN_DR_MASK

/** Macro for importing guest state from the VMCB back into CPUMCTX.  */
#define HMSVM_CPUMCTX_IMPORT_STATE(a_pVCpu, a_fWhat) \
    do { \
        if ((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fWhat)) \
            hmR0SvmImportGuestState((a_pVCpu), (a_fWhat)); \
    } while (0)

/** Assert that the required state bits are fetched. */
#define HMSVM_CPUMCTX_ASSERT(a_pVCpu, a_fExtrnMbz)      AssertMsg(!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnMbz)), \
                                                                  ("fExtrn=%#RX64 fExtrnMbz=%#RX64\n", \
                                                                  (a_pVCpu)->cpum.GstCtx.fExtrn, (a_fExtrnMbz)))

/** Assert that preemption is disabled or covered by thread-context hooks. */
#define HMSVM_ASSERT_PREEMPT_SAFE(a_pVCpu)              Assert(   VMMR0ThreadCtxHookIsEnabled((a_pVCpu)) \
                                                               || !RTThreadPreemptIsEnabled(NIL_RTTHREAD));

/** Assert that we haven't migrated CPUs when thread-context hooks are not
 *  used. */
#define HMSVM_ASSERT_CPU_SAFE(a_pVCpu)                  AssertMsg(   VMMR0ThreadCtxHookIsEnabled((a_pVCpu)) \
                                                                  || (a_pVCpu)->hm.s.idEnteredCpu == RTMpCpuId(), \
                                                                  ("Illegal migration! Entered on CPU %u Current %u\n", \
                                                                   (a_pVCpu)->hm.s.idEnteredCpu, RTMpCpuId()));

/** Assert that we're not executing a nested-guest. */
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
# define HMSVM_ASSERT_NOT_IN_NESTED_GUEST(a_pCtx)       Assert(!CPUMIsGuestInSvmNestedHwVirtMode((a_pCtx)))
#else
# define HMSVM_ASSERT_NOT_IN_NESTED_GUEST(a_pCtx)       do { NOREF((a_pCtx)); } while (0)
#endif

/** Assert that we're executing a nested-guest. */
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
# define HMSVM_ASSERT_IN_NESTED_GUEST(a_pCtx)           Assert(CPUMIsGuestInSvmNestedHwVirtMode((a_pCtx)))
#else
# define HMSVM_ASSERT_IN_NESTED_GUEST(a_pCtx)           do { NOREF((a_pCtx)); } while (0)
#endif

/** Macro for checking and returning from the using function for
 * \#VMEXIT intercepts that maybe caused during delivering of another
 * event in the guest. */
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
# define HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(a_pVCpu, a_pSvmTransient) \
    do \
    { \
        int rc = hmR0SvmCheckExitDueToEventDelivery((a_pVCpu), (a_pSvmTransient)); \
        if (RT_LIKELY(rc == VINF_SUCCESS))        { /* continue #VMEXIT handling */ } \
        else if (     rc == VINF_HM_DOUBLE_FAULT) { return VINF_SUCCESS;            } \
        else if (     rc == VINF_EM_RESET \
                 &&   CPUMIsGuestSvmCtrlInterceptSet((a_pVCpu), &(a_pVCpu)->cpum.GstCtx, SVM_CTRL_INTERCEPT_SHUTDOWN)) \
        { \
            HMSVM_CPUMCTX_IMPORT_STATE((a_pVCpu), HMSVM_CPUMCTX_EXTRN_ALL); \
            return VBOXSTRICTRC_TODO(IEMExecSvmVmexit((a_pVCpu), SVM_EXIT_SHUTDOWN, 0, 0)); \
        } \
        else \
            return rc; \
    } while (0)
#else
# define HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(a_pVCpu, a_pSvmTransient) \
    do \
    { \
        int rc = hmR0SvmCheckExitDueToEventDelivery((a_pVCpu), (a_pSvmTransient)); \
        if (RT_LIKELY(rc == VINF_SUCCESS))        { /* continue #VMEXIT handling */ } \
        else if (     rc == VINF_HM_DOUBLE_FAULT) { return VINF_SUCCESS;            } \
        else \
            return rc; \
    } while (0)
#endif

/** Macro for upgrading a @a a_rc to VINF_EM_DBG_STEPPED after emulating an
 * instruction that exited. */
#define HMSVM_CHECK_SINGLE_STEP(a_pVCpu, a_rc) \
    do { \
        if ((a_pVCpu)->hm.s.fSingleInstruction && (a_rc) == VINF_SUCCESS) \
            (a_rc) = VINF_EM_DBG_STEPPED; \
    } while (0)

/** Validate segment descriptor granularity bit. */
#ifdef VBOX_STRICT
# define HMSVM_ASSERT_SEG_GRANULARITY(a_pCtx, reg) \
    AssertMsg(   !(a_pCtx)->reg.Attr.n.u1Present \
              || (   (a_pCtx)->reg.Attr.n.u1Granularity \
                  ? ((a_pCtx)->reg.u32Limit & 0xfff) == 0xfff \
                  :  (a_pCtx)->reg.u32Limit <= UINT32_C(0xfffff)), \
              ("Invalid Segment Attributes Limit=%#RX32 Attr=%#RX32 Base=%#RX64\n", (a_pCtx)->reg.u32Limit, \
              (a_pCtx)->reg.Attr.u, (a_pCtx)->reg.u64Base))
#else
# define HMSVM_ASSERT_SEG_GRANULARITY(a_pCtx, reg)      do { } while (0)
#endif

/**
 * Exception bitmap mask for all contributory exceptions.
 *
 * Page fault is deliberately excluded here as it's conditional as to whether
 * it's contributory or benign. Page faults are handled separately.
 */
#define HMSVM_CONTRIBUTORY_XCPT_MASK  (  RT_BIT(X86_XCPT_GP) | RT_BIT(X86_XCPT_NP) | RT_BIT(X86_XCPT_SS) | RT_BIT(X86_XCPT_TS) \
                                       | RT_BIT(X86_XCPT_DE))

/**
 * Mandatory/unconditional guest control intercepts.
 *
 * SMIs can and do happen in normal operation. We need not intercept them
 * while executing the guest (or nested-guest).
 */
#define HMSVM_MANDATORY_GUEST_CTRL_INTERCEPTS           (  SVM_CTRL_INTERCEPT_INTR          \
                                                         | SVM_CTRL_INTERCEPT_NMI           \
                                                         | SVM_CTRL_INTERCEPT_INIT          \
                                                         | SVM_CTRL_INTERCEPT_RDPMC         \
                                                         | SVM_CTRL_INTERCEPT_CPUID         \
                                                         | SVM_CTRL_INTERCEPT_RSM           \
                                                         | SVM_CTRL_INTERCEPT_HLT           \
                                                         | SVM_CTRL_INTERCEPT_IOIO_PROT     \
                                                         | SVM_CTRL_INTERCEPT_MSR_PROT      \
                                                         | SVM_CTRL_INTERCEPT_INVLPGA       \
                                                         | SVM_CTRL_INTERCEPT_SHUTDOWN      \
                                                         | SVM_CTRL_INTERCEPT_FERR_FREEZE   \
                                                         | SVM_CTRL_INTERCEPT_VMRUN         \
                                                         | SVM_CTRL_INTERCEPT_SKINIT        \
                                                         | SVM_CTRL_INTERCEPT_WBINVD        \
                                                         | SVM_CTRL_INTERCEPT_MONITOR       \
                                                         | SVM_CTRL_INTERCEPT_MWAIT         \
                                                         | SVM_CTRL_INTERCEPT_CR0_SEL_WRITE \
                                                         | SVM_CTRL_INTERCEPT_XSETBV)

/** @name VMCB Clean Bits.
 *
 * These flags are used for VMCB-state caching. A set VMCB Clean bit indicates
 * AMD-V doesn't need to reload the corresponding value(s) from the VMCB in
 * memory.
 *
 * @{ */
/** All intercepts vectors, TSC offset, PAUSE filter counter. */
#define HMSVM_VMCB_CLEAN_INTERCEPTS             RT_BIT(0)
/** I/O permission bitmap, MSR permission bitmap. */
#define HMSVM_VMCB_CLEAN_IOPM_MSRPM             RT_BIT(1)
/** ASID.  */
#define HMSVM_VMCB_CLEAN_ASID                   RT_BIT(2)
/** TRP: V_TPR, V_IRQ, V_INTR_PRIO, V_IGN_TPR, V_INTR_MASKING,
V_INTR_VECTOR. */
#define HMSVM_VMCB_CLEAN_INT_CTRL               RT_BIT(3)
/** Nested Paging: Nested CR3 (nCR3), PAT. */
#define HMSVM_VMCB_CLEAN_NP                     RT_BIT(4)
/** Control registers (CR0, CR3, CR4, EFER). */
#define HMSVM_VMCB_CLEAN_CRX_EFER               RT_BIT(5)
/** Debug registers (DR6, DR7). */
#define HMSVM_VMCB_CLEAN_DRX                    RT_BIT(6)
/** GDT, IDT limit and base. */
#define HMSVM_VMCB_CLEAN_DT                     RT_BIT(7)
/** Segment register: CS, SS, DS, ES limit and base. */
#define HMSVM_VMCB_CLEAN_SEG                    RT_BIT(8)
/** CR2.*/
#define HMSVM_VMCB_CLEAN_CR2                    RT_BIT(9)
/** Last-branch record (DbgCtlMsr, br_from, br_to, lastint_from, lastint_to) */
#define HMSVM_VMCB_CLEAN_LBR                    RT_BIT(10)
/** AVIC (AVIC APIC_BAR; AVIC APIC_BACKING_PAGE, AVIC
PHYSICAL_TABLE and AVIC LOGICAL_TABLE Pointers). */
#define HMSVM_VMCB_CLEAN_AVIC                   RT_BIT(11)
/** Mask of all valid VMCB Clean bits. */
#define HMSVM_VMCB_CLEAN_ALL                    (  HMSVM_VMCB_CLEAN_INTERCEPTS  \
                                                 | HMSVM_VMCB_CLEAN_IOPM_MSRPM  \
                                                 | HMSVM_VMCB_CLEAN_ASID        \
                                                 | HMSVM_VMCB_CLEAN_INT_CTRL    \
                                                 | HMSVM_VMCB_CLEAN_NP          \
                                                 | HMSVM_VMCB_CLEAN_CRX_EFER    \
                                                 | HMSVM_VMCB_CLEAN_DRX         \
                                                 | HMSVM_VMCB_CLEAN_DT          \
                                                 | HMSVM_VMCB_CLEAN_SEG         \
                                                 | HMSVM_VMCB_CLEAN_CR2         \
                                                 | HMSVM_VMCB_CLEAN_LBR         \
                                                 | HMSVM_VMCB_CLEAN_AVIC)
/** @} */

/** @name SVM transient.
 *
 * A state structure for holding miscellaneous information across AMD-V
 * VMRUN/\#VMEXIT operation, restored after the transition.
 *
 * @{ */
typedef struct SVMTRANSIENT
{
    /** The host's rflags/eflags. */
    RTCCUINTREG     fEFlags;
#if HC_ARCH_BITS == 32
    uint32_t        u32Alignment0;
#endif

    /** The \#VMEXIT exit code (the EXITCODE field in the VMCB). */
    uint64_t        u64ExitCode;
    /** The guest's TPR value used for TPR shadowing. */
    uint8_t         u8GuestTpr;
    /** Alignment. */
    uint8_t         abAlignment0[7];

    /** Pointer to the currently executing VMCB. */
    PSVMVMCB        pVmcb;
    /** Whether we are currently executing a nested-guest. */
    bool            fIsNestedGuest;

    /** Whether the guest debug state was active at the time of \#VMEXIT. */
    bool            fWasGuestDebugStateActive;
    /** Whether the hyper debug state was active at the time of \#VMEXIT. */
    bool            fWasHyperDebugStateActive;
    /** Whether the TSC offset mode needs to be updated. */
    bool            fUpdateTscOffsetting;
    /** Whether the TSC_AUX MSR needs restoring on \#VMEXIT. */
    bool            fRestoreTscAuxMsr;
    /** Whether the \#VMEXIT was caused by a page-fault during delivery of a
     *  contributary exception or a page-fault. */
    bool            fVectoringDoublePF;
    /** Whether the \#VMEXIT was caused by a page-fault during delivery of an
     *  external interrupt or NMI. */
    bool            fVectoringPF;
} SVMTRANSIENT, *PSVMTRANSIENT;
AssertCompileMemberAlignment(SVMTRANSIENT, u64ExitCode, sizeof(uint64_t));
AssertCompileMemberAlignment(SVMTRANSIENT, pVmcb,       sizeof(uint64_t));
/** @}  */

/**
 * MSRPM (MSR permission bitmap) read permissions (for guest RDMSR).
 */
typedef enum SVMMSREXITREAD
{
    /** Reading this MSR causes a \#VMEXIT. */
    SVMMSREXIT_INTERCEPT_READ = 0xb,
    /** Reading this MSR does not cause a \#VMEXIT. */
    SVMMSREXIT_PASSTHRU_READ
} SVMMSREXITREAD;

/**
 * MSRPM (MSR permission bitmap) write permissions (for guest WRMSR).
 */
typedef enum SVMMSREXITWRITE
{
    /** Writing to this MSR causes a \#VMEXIT. */
    SVMMSREXIT_INTERCEPT_WRITE = 0xd,
    /** Writing to this MSR does not cause a \#VMEXIT. */
    SVMMSREXIT_PASSTHRU_WRITE
} SVMMSREXITWRITE;

/**
 * SVM \#VMEXIT handler.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM-transient structure.
 */
typedef int FNSVMEXITHANDLER(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient);


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void hmR0SvmPendingEventToTrpmTrap(PVMCPU pVCpu);
static void hmR0SvmLeave(PVMCPU pVCpu, bool fImportState);


/** @name \#VMEXIT handlers.
 * @{
 */
static FNSVMEXITHANDLER hmR0SvmExitIntr;
static FNSVMEXITHANDLER hmR0SvmExitWbinvd;
static FNSVMEXITHANDLER hmR0SvmExitInvd;
static FNSVMEXITHANDLER hmR0SvmExitCpuid;
static FNSVMEXITHANDLER hmR0SvmExitRdtsc;
static FNSVMEXITHANDLER hmR0SvmExitRdtscp;
static FNSVMEXITHANDLER hmR0SvmExitRdpmc;
static FNSVMEXITHANDLER hmR0SvmExitInvlpg;
static FNSVMEXITHANDLER hmR0SvmExitHlt;
static FNSVMEXITHANDLER hmR0SvmExitMonitor;
static FNSVMEXITHANDLER hmR0SvmExitMwait;
static FNSVMEXITHANDLER hmR0SvmExitShutdown;
static FNSVMEXITHANDLER hmR0SvmExitUnexpected;
static FNSVMEXITHANDLER hmR0SvmExitReadCRx;
static FNSVMEXITHANDLER hmR0SvmExitWriteCRx;
static FNSVMEXITHANDLER hmR0SvmExitMsr;
static FNSVMEXITHANDLER hmR0SvmExitReadDRx;
static FNSVMEXITHANDLER hmR0SvmExitWriteDRx;
static FNSVMEXITHANDLER hmR0SvmExitXsetbv;
static FNSVMEXITHANDLER hmR0SvmExitIOInstr;
static FNSVMEXITHANDLER hmR0SvmExitNestedPF;
static FNSVMEXITHANDLER hmR0SvmExitVIntr;
static FNSVMEXITHANDLER hmR0SvmExitTaskSwitch;
static FNSVMEXITHANDLER hmR0SvmExitVmmCall;
static FNSVMEXITHANDLER hmR0SvmExitPause;
static FNSVMEXITHANDLER hmR0SvmExitFerrFreeze;
static FNSVMEXITHANDLER hmR0SvmExitIret;
static FNSVMEXITHANDLER hmR0SvmExitXcptPF;
static FNSVMEXITHANDLER hmR0SvmExitXcptUD;
static FNSVMEXITHANDLER hmR0SvmExitXcptMF;
static FNSVMEXITHANDLER hmR0SvmExitXcptDB;
static FNSVMEXITHANDLER hmR0SvmExitXcptAC;
static FNSVMEXITHANDLER hmR0SvmExitXcptBP;
#if defined(HMSVM_ALWAYS_TRAP_ALL_XCPTS) || defined(VBOX_WITH_NESTED_HWVIRT_SVM)
static FNSVMEXITHANDLER hmR0SvmExitXcptGeneric;
#endif
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
static FNSVMEXITHANDLER hmR0SvmExitClgi;
static FNSVMEXITHANDLER hmR0SvmExitStgi;
static FNSVMEXITHANDLER hmR0SvmExitVmload;
static FNSVMEXITHANDLER hmR0SvmExitVmsave;
static FNSVMEXITHANDLER hmR0SvmExitInvlpga;
static FNSVMEXITHANDLER hmR0SvmExitVmrun;
static FNSVMEXITHANDLER hmR0SvmNestedExitXcptDB;
static FNSVMEXITHANDLER hmR0SvmNestedExitXcptBP;
#endif
/** @} */

static int hmR0SvmHandleExit(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
static int hmR0SvmHandleExitNested(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient);
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Ring-0 memory object for the IO bitmap. */
static RTR0MEMOBJ           g_hMemObjIOBitmap = NIL_RTR0MEMOBJ;
/** Physical address of the IO bitmap. */
static RTHCPHYS             g_HCPhysIOBitmap;
/** Pointer to the IO bitmap. */
static R0PTRTYPE(void *)    g_pvIOBitmap;

#ifdef VBOX_STRICT
# define HMSVM_LOG_RBP_RSP      RT_BIT_32(0)
# define HMSVM_LOG_CR_REGS      RT_BIT_32(1)
# define HMSVM_LOG_CS           RT_BIT_32(2)
# define HMSVM_LOG_SS           RT_BIT_32(3)
# define HMSVM_LOG_FS           RT_BIT_32(4)
# define HMSVM_LOG_GS           RT_BIT_32(5)
# define HMSVM_LOG_LBR          RT_BIT_32(6)
# define HMSVM_LOG_ALL          (  HMSVM_LOG_RBP_RSP \
                                 | HMSVM_LOG_CR_REGS \
                                 | HMSVM_LOG_CS \
                                 | HMSVM_LOG_SS \
                                 | HMSVM_LOG_FS \
                                 | HMSVM_LOG_GS \
                                 | HMSVM_LOG_LBR)

/**
 * Dumps virtual CPU state and additional info. to the logger for diagnostics.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 * @param   pszPrefix   Log prefix.
 * @param   fFlags      Log flags, see HMSVM_LOG_XXX.
 * @param   uVerbose    The verbosity level, currently unused.
 */
static void hmR0SvmLogState(PVMCPU pVCpu, PCSVMVMCB pVmcb, const char *pszPrefix, uint32_t fFlags, uint8_t uVerbose)
{
    RT_NOREF2(pVCpu, uVerbose);
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;

    HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS);
    Log4(("%s: cs:rip=%04x:%RX64 efl=%#RX64\n", pszPrefix, pCtx->cs.Sel, pCtx->rip, pCtx->rflags.u));

    if (fFlags & HMSVM_LOG_RBP_RSP)
    {
        HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_RSP | CPUMCTX_EXTRN_RBP);
        Log4(("%s: rsp=%#RX64 rbp=%#RX64\n", pszPrefix, pCtx->rsp, pCtx->rbp));
    }

    if (fFlags & HMSVM_LOG_CR_REGS)
    {
        HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4);
        Log4(("%s: cr0=%#RX64 cr3=%#RX64 cr4=%#RX64\n", pszPrefix, pCtx->cr0, pCtx->cr3, pCtx->cr4));
    }

    if (fFlags & HMSVM_LOG_CS)
    {
        HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CS);
        Log4(("%s: cs={%04x base=%016RX64 limit=%08x flags=%08x}\n", pszPrefix, pCtx->cs.Sel, pCtx->cs.u64Base,
              pCtx->cs.u32Limit, pCtx->cs.Attr.u));
    }
    if (fFlags & HMSVM_LOG_SS)
    {
        HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SS);
        Log4(("%s: ss={%04x base=%016RX64 limit=%08x flags=%08x}\n", pszPrefix, pCtx->ss.Sel, pCtx->ss.u64Base,
              pCtx->ss.u32Limit, pCtx->ss.Attr.u));
    }
    if (fFlags & HMSVM_LOG_FS)
    {
        HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_FS);
        Log4(("%s: fs={%04x base=%016RX64 limit=%08x flags=%08x}\n", pszPrefix, pCtx->fs.Sel, pCtx->fs.u64Base,
              pCtx->fs.u32Limit, pCtx->fs.Attr.u));
    }
    if (fFlags & HMSVM_LOG_GS)
    {
        HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_GS);
        Log4(("%s: gs={%04x base=%016RX64 limit=%08x flags=%08x}\n", pszPrefix, pCtx->gs.Sel, pCtx->gs.u64Base,
              pCtx->gs.u32Limit, pCtx->gs.Attr.u));
    }

    PCSVMVMCBSTATESAVE pVmcbGuest = &pVmcb->guest;
    if (fFlags & HMSVM_LOG_LBR)
    {
        Log4(("%s: br_from=%#RX64 br_to=%#RX64 lastxcpt_from=%#RX64 lastxcpt_to=%#RX64\n", pszPrefix, pVmcbGuest->u64BR_FROM,
              pVmcbGuest->u64BR_TO, pVmcbGuest->u64LASTEXCPFROM, pVmcbGuest->u64LASTEXCPTO));
    }
    NOREF(pVmcbGuest); NOREF(pCtx);
}
#endif  /* VBOX_STRICT */


/**
 * Sets up and activates AMD-V on the current CPU.
 *
 * @returns VBox status code.
 * @param   pHostCpu        Pointer to the CPU info struct.
 * @param   pVM             The cross context VM structure. Can be
 *                          NULL after a resume!
 * @param   pvCpuPage       Pointer to the global CPU page.
 * @param   HCPhysCpuPage   Physical address of the global CPU page.
 * @param   fEnabledByHost  Whether the host OS has already initialized AMD-V.
 * @param   pvArg           Unused on AMD-V.
 */
VMMR0DECL(int) SVMR0EnableCpu(PHMGLOBALCPUINFO pHostCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage, bool fEnabledByHost,
                              void *pvArg)
{
    Assert(!fEnabledByHost);
    Assert(HCPhysCpuPage && HCPhysCpuPage != NIL_RTHCPHYS);
    Assert(RT_ALIGN_T(HCPhysCpuPage, _4K, RTHCPHYS) == HCPhysCpuPage);
    Assert(pvCpuPage); NOREF(pvCpuPage);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    NOREF(pvArg);
    NOREF(fEnabledByHost);

    /* Paranoid: Disable interrupt as, in theory, interrupt handlers might mess with EFER. */
    RTCCUINTREG const fEFlags = ASMIntDisableFlags();

    /*
     * We must turn on AMD-V and setup the host state physical address, as those MSRs are per CPU.
     */
    uint64_t u64HostEfer = ASMRdMsr(MSR_K6_EFER);
    if (u64HostEfer & MSR_K6_EFER_SVME)
    {
        /* If the VBOX_HWVIRTEX_IGNORE_SVM_IN_USE is active, then we blindly use AMD-V. */
        if (   pVM
            && pVM->hm.s.svm.fIgnoreInUseError)
            pHostCpu->fIgnoreAMDVInUseError = true;

        if (!pHostCpu->fIgnoreAMDVInUseError)
        {
            ASMSetFlags(fEFlags);
            return VERR_SVM_IN_USE;
        }
    }

    /* Turn on AMD-V in the EFER MSR. */
    ASMWrMsr(MSR_K6_EFER, u64HostEfer | MSR_K6_EFER_SVME);

    /* Write the physical page address where the CPU will store the host state while executing the VM. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, HCPhysCpuPage);

    /* Restore interrupts. */
    ASMSetFlags(fEFlags);

    /*
     * Theoretically, other hypervisors may have used ASIDs, ideally we should flush all
     * non-zero ASIDs when enabling SVM. AMD doesn't have an SVM instruction to flush all
     * ASIDs (flushing is done upon VMRUN). Therefore, flag that we need to flush the TLB
     * entirely with before executing any guest code.
     */
    pHostCpu->fFlushAsidBeforeUse = true;

    /*
     * Ensure each VCPU scheduled on this CPU gets a new ASID on resume. See @bugref{6255}.
     */
    ++pHostCpu->cTlbFlushes;

    return VINF_SUCCESS;
}


/**
 * Deactivates AMD-V on the current CPU.
 *
 * @returns VBox status code.
 * @param   pHostCpu        Pointer to the CPU info struct.
 * @param   pvCpuPage       Pointer to the global CPU page.
 * @param   HCPhysCpuPage   Physical address of the global CPU page.
 */
VMMR0DECL(int) SVMR0DisableCpu(PHMGLOBALCPUINFO pHostCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    AssertReturn(   HCPhysCpuPage
                 && HCPhysCpuPage != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(pvCpuPage, VERR_INVALID_PARAMETER);
    RT_NOREF(pHostCpu);

    /* Paranoid: Disable interrupts as, in theory, interrupt handlers might mess with EFER. */
    RTCCUINTREG const fEFlags = ASMIntDisableFlags();

    /* Turn off AMD-V in the EFER MSR. */
    uint64_t u64HostEfer = ASMRdMsr(MSR_K6_EFER);
    ASMWrMsr(MSR_K6_EFER, u64HostEfer & ~MSR_K6_EFER_SVME);

    /* Invalidate host state physical address. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, 0);

    /* Restore interrupts. */
    ASMSetFlags(fEFlags);

    return VINF_SUCCESS;
}


/**
 * Does global AMD-V initialization (called during module initialization).
 *
 * @returns VBox status code.
 */
VMMR0DECL(int) SVMR0GlobalInit(void)
{
    /*
     * Allocate 12 KB (3 pages) for the IO bitmap. Since this is non-optional and we always
     * intercept all IO accesses, it's done once globally here instead of per-VM.
     */
    Assert(g_hMemObjIOBitmap == NIL_RTR0MEMOBJ);
    int rc = RTR0MemObjAllocCont(&g_hMemObjIOBitmap, SVM_IOPM_PAGES << X86_PAGE_4K_SHIFT, false /* fExecutable */);
    if (RT_FAILURE(rc))
        return rc;

    g_pvIOBitmap     = RTR0MemObjAddress(g_hMemObjIOBitmap);
    g_HCPhysIOBitmap = RTR0MemObjGetPagePhysAddr(g_hMemObjIOBitmap, 0 /* iPage */);

    /* Set all bits to intercept all IO accesses. */
    ASMMemFill32(g_pvIOBitmap, SVM_IOPM_PAGES << X86_PAGE_4K_SHIFT, UINT32_C(0xffffffff));

    return VINF_SUCCESS;
}


/**
 * Does global AMD-V termination (called during module termination).
 */
VMMR0DECL(void) SVMR0GlobalTerm(void)
{
    if (g_hMemObjIOBitmap != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(g_hMemObjIOBitmap, true /* fFreeMappings */);
        g_pvIOBitmap      = NULL;
        g_HCPhysIOBitmap  = 0;
        g_hMemObjIOBitmap = NIL_RTR0MEMOBJ;
    }
}


/**
 * Frees any allocated per-VCPU structures for a VM.
 *
 * @param   pVM     The cross context VM structure.
 */
DECLINLINE(void) hmR0SvmFreeStructs(PVM pVM)
{
    for (uint32_t i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);

        if (pVCpu->hm.s.svm.hMemObjVmcbHost != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hm.s.svm.hMemObjVmcbHost, false);
            pVCpu->hm.s.svm.HCPhysVmcbHost   = 0;
            pVCpu->hm.s.svm.hMemObjVmcbHost  = NIL_RTR0MEMOBJ;
        }

        if (pVCpu->hm.s.svm.hMemObjVmcb != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hm.s.svm.hMemObjVmcb, false);
            pVCpu->hm.s.svm.pVmcb            = NULL;
            pVCpu->hm.s.svm.HCPhysVmcb       = 0;
            pVCpu->hm.s.svm.hMemObjVmcb      = NIL_RTR0MEMOBJ;
        }

        if (pVCpu->hm.s.svm.hMemObjMsrBitmap != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hm.s.svm.hMemObjMsrBitmap, false);
            pVCpu->hm.s.svm.pvMsrBitmap      = NULL;
            pVCpu->hm.s.svm.HCPhysMsrBitmap  = 0;
            pVCpu->hm.s.svm.hMemObjMsrBitmap = NIL_RTR0MEMOBJ;
        }
    }
}


/**
 * Does per-VM AMD-V initialization.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR0DECL(int) SVMR0InitVM(PVM pVM)
{
    int rc = VERR_INTERNAL_ERROR_5;

    /*
     * Check for an AMD CPU erratum which requires us to flush the TLB before every world-switch.
     */
    uint32_t u32Family;
    uint32_t u32Model;
    uint32_t u32Stepping;
    if (HMAmdIsSubjectToErratum170(&u32Family, &u32Model, &u32Stepping))
    {
        Log4Func(("AMD cpu with erratum 170 family %#x model %#x stepping %#x\n", u32Family, u32Model, u32Stepping));
        pVM->hm.s.svm.fAlwaysFlushTLB = true;
    }

    /*
     * Initialize the R0 memory objects up-front so we can properly cleanup on allocation failures.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->hm.s.svm.hMemObjVmcbHost  = NIL_RTR0MEMOBJ;
        pVCpu->hm.s.svm.hMemObjVmcb      = NIL_RTR0MEMOBJ;
        pVCpu->hm.s.svm.hMemObjMsrBitmap = NIL_RTR0MEMOBJ;
    }

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        /*
         * Allocate one page for the host-context VM control block (VMCB). This is used for additional host-state (such as
         * FS, GS, Kernel GS Base, etc.) apart from the host-state save area specified in MSR_K8_VM_HSAVE_PA.
         */
        rc = RTR0MemObjAllocCont(&pVCpu->hm.s.svm.hMemObjVmcbHost, SVM_VMCB_PAGES << PAGE_SHIFT, false /* fExecutable */);
        if (RT_FAILURE(rc))
            goto failure_cleanup;

        void *pvVmcbHost               = RTR0MemObjAddress(pVCpu->hm.s.svm.hMemObjVmcbHost);
        pVCpu->hm.s.svm.HCPhysVmcbHost = RTR0MemObjGetPagePhysAddr(pVCpu->hm.s.svm.hMemObjVmcbHost, 0 /* iPage */);
        Assert(pVCpu->hm.s.svm.HCPhysVmcbHost < _4G);
        ASMMemZeroPage(pvVmcbHost);

        /*
         * Allocate one page for the guest-state VMCB.
         */
        rc = RTR0MemObjAllocCont(&pVCpu->hm.s.svm.hMemObjVmcb, SVM_VMCB_PAGES << PAGE_SHIFT, false /* fExecutable */);
        if (RT_FAILURE(rc))
            goto failure_cleanup;

        pVCpu->hm.s.svm.pVmcb           = (PSVMVMCB)RTR0MemObjAddress(pVCpu->hm.s.svm.hMemObjVmcb);
        pVCpu->hm.s.svm.HCPhysVmcb      = RTR0MemObjGetPagePhysAddr(pVCpu->hm.s.svm.hMemObjVmcb, 0 /* iPage */);
        Assert(pVCpu->hm.s.svm.HCPhysVmcb < _4G);
        ASMMemZeroPage(pVCpu->hm.s.svm.pVmcb);

        /*
         * Allocate two pages (8 KB) for the MSR permission bitmap. There doesn't seem to be a way to convince
         * SVM to not require one.
         */
        rc = RTR0MemObjAllocCont(&pVCpu->hm.s.svm.hMemObjMsrBitmap, SVM_MSRPM_PAGES << X86_PAGE_4K_SHIFT,
                                 false /* fExecutable */);
        if (RT_FAILURE(rc))
            goto failure_cleanup;

        pVCpu->hm.s.svm.pvMsrBitmap     = RTR0MemObjAddress(pVCpu->hm.s.svm.hMemObjMsrBitmap);
        pVCpu->hm.s.svm.HCPhysMsrBitmap = RTR0MemObjGetPagePhysAddr(pVCpu->hm.s.svm.hMemObjMsrBitmap, 0 /* iPage */);
        /* Set all bits to intercept all MSR accesses (changed later on). */
        ASMMemFill32(pVCpu->hm.s.svm.pvMsrBitmap, SVM_MSRPM_PAGES << X86_PAGE_4K_SHIFT, UINT32_C(0xffffffff));
   }

    return VINF_SUCCESS;

failure_cleanup:
    hmR0SvmFreeStructs(pVM);
    return rc;
}


/**
 * Does per-VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR0DECL(int) SVMR0TermVM(PVM pVM)
{
    hmR0SvmFreeStructs(pVM);
    return VINF_SUCCESS;
}


/**
 * Returns whether the VMCB Clean Bits feature is supported.
 *
 * @return @c true if supported, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(bool) hmR0SvmSupportsVmcbCleanBits(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
    {
        return (pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_VMCB_CLEAN)
            &&  pVM->cpum.ro.GuestFeatures.fSvmVmcbClean;
    }
#endif
    return RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_VMCB_CLEAN);
}


/**
 * Returns whether the decode assists feature is supported.
 *
 * @return @c true if supported, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(bool) hmR0SvmSupportsDecodeAssists(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
    {
        return (pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_DECODE_ASSISTS)
            &&  pVM->cpum.ro.GuestFeatures.fSvmDecodeAssists;
    }
#endif
    return RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_DECODE_ASSISTS);
}


/**
 * Returns whether the NRIP_SAVE feature is supported.
 *
 * @return @c true if supported, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(bool) hmR0SvmSupportsNextRipSave(PVMCPU pVCpu)
{
#if 0
    PVM pVM = pVCpu->CTX_SUFF(pVM);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
    {
        return (pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_NRIP_SAVE)
            &&  pVM->cpum.ro.GuestFeatures.fSvmNextRipSave;
    }
#endif
    return RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_NRIP_SAVE);
#endif

    /** @todo Temporarily disabled NRIP_SAVE for testing. re-enable once its working. */
    NOREF(pVCpu);
    return false;
}


/**
 * Sets the permission bits for the specified MSR in the MSRPM bitmap.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pbMsrBitmap     Pointer to the MSR bitmap.
 * @param   idMsr           The MSR for which the permissions are being set.
 * @param   enmRead         MSR read permissions.
 * @param   enmWrite        MSR write permissions.
 *
 * @remarks This function does -not- clear the VMCB clean bits for MSRPM. The
 *          caller needs to take care of this.
 */
static void hmR0SvmSetMsrPermission(PVMCPU pVCpu, uint8_t *pbMsrBitmap, uint32_t idMsr, SVMMSREXITREAD enmRead,
                                    SVMMSREXITWRITE enmWrite)
{
    bool const  fInNestedGuestMode = CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx);
    uint16_t    offMsrpm;
    uint8_t     uMsrpmBit;
    int rc = HMSvmGetMsrpmOffsetAndBit(idMsr, &offMsrpm, &uMsrpmBit);
    AssertRC(rc);

    Assert(uMsrpmBit == 0 || uMsrpmBit == 2 || uMsrpmBit == 4 || uMsrpmBit == 6);
    Assert(offMsrpm < SVM_MSRPM_PAGES << X86_PAGE_4K_SHIFT);

    pbMsrBitmap += offMsrpm;
    if (enmRead == SVMMSREXIT_INTERCEPT_READ)
        *pbMsrBitmap |= RT_BIT(uMsrpmBit);
    else
    {
        if (!fInNestedGuestMode)
            *pbMsrBitmap &= ~RT_BIT(uMsrpmBit);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        else
        {
            /* Only clear the bit if the nested-guest is also not intercepting the MSR read.*/
            uint8_t const *pbNstGstMsrBitmap = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pvMsrBitmap);
            pbNstGstMsrBitmap += offMsrpm;
            if (!(*pbNstGstMsrBitmap & RT_BIT(uMsrpmBit)))
                *pbMsrBitmap &= ~RT_BIT(uMsrpmBit);
            else
                Assert(*pbMsrBitmap & RT_BIT(uMsrpmBit));
        }
#endif
    }

    if (enmWrite == SVMMSREXIT_INTERCEPT_WRITE)
        *pbMsrBitmap |= RT_BIT(uMsrpmBit + 1);
    else
    {
        if (!fInNestedGuestMode)
            *pbMsrBitmap &= ~RT_BIT(uMsrpmBit + 1);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        else
        {
            /* Only clear the bit if the nested-guest is also not intercepting the MSR write.*/
            uint8_t const *pbNstGstMsrBitmap = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pvMsrBitmap);
            pbNstGstMsrBitmap += offMsrpm;
            if (!(*pbNstGstMsrBitmap & RT_BIT(uMsrpmBit + 1)))
                *pbMsrBitmap &= ~RT_BIT(uMsrpmBit + 1);
            else
                Assert(*pbMsrBitmap & RT_BIT(uMsrpmBit + 1));
        }
#endif
    }
}


/**
 * Sets up AMD-V for the specified VM.
 * This function is only called once per-VM during initalization.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR0DECL(int) SVMR0SetupVM(PVM pVM)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    Assert(pVM->hm.s.svm.fSupported);

    bool const fPauseFilter          = RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_PAUSE_FILTER);
    bool const fPauseFilterThreshold = RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_PAUSE_FILTER_THRESHOLD);
    bool const fUsePauseFilter       = fPauseFilter && pVM->hm.s.svm.cPauseFilter;

    bool const fLbrVirt              = RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_LBR_VIRT);
    bool const fUseLbrVirt           = fLbrVirt; /** @todo CFGM, IEM implementation etc. */

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    bool const fVirtVmsaveVmload     = RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_VIRT_VMSAVE_VMLOAD);
    bool const fUseVirtVmsaveVmload  = fVirtVmsaveVmload && pVM->hm.s.svm.fVirtVmsaveVmload && pVM->hm.s.fNestedPaging;

    bool const fVGif                 = RT_BOOL(pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_VGIF);
    bool const fUseVGif              = fVGif && pVM->hm.s.svm.fVGif;
#endif

    PVMCPU       pVCpu = &pVM->aCpus[0];
    PSVMVMCB     pVmcb = pVCpu->hm.s.svm.pVmcb;
    AssertMsgReturn(pVmcb, ("Invalid pVmcb for vcpu[0]\n"), VERR_SVM_INVALID_PVMCB);
    PSVMVMCBCTRL pVmcbCtrl = &pVmcb->ctrl;

    /* Always trap #AC for reasons of security. */
    pVmcbCtrl->u32InterceptXcpt |= RT_BIT_32(X86_XCPT_AC);

    /* Always trap #DB for reasons of security. */
    pVmcbCtrl->u32InterceptXcpt |= RT_BIT_32(X86_XCPT_DB);

    /* Trap exceptions unconditionally (debug purposes). */
#ifdef HMSVM_ALWAYS_TRAP_PF
    pVmcbCtrl->u32InterceptXcpt |=   RT_BIT(X86_XCPT_PF);
#endif
#ifdef HMSVM_ALWAYS_TRAP_ALL_XCPTS
    /* If you add any exceptions here, make sure to update hmR0SvmHandleExit(). */
    pVmcbCtrl->u32InterceptXcpt |= 0
                                 | RT_BIT(X86_XCPT_BP)
                                 | RT_BIT(X86_XCPT_DE)
                                 | RT_BIT(X86_XCPT_NM)
                                 | RT_BIT(X86_XCPT_UD)
                                 | RT_BIT(X86_XCPT_NP)
                                 | RT_BIT(X86_XCPT_SS)
                                 | RT_BIT(X86_XCPT_GP)
                                 | RT_BIT(X86_XCPT_PF)
                                 | RT_BIT(X86_XCPT_MF)
                                 ;
#endif

    /* Apply the exceptions intercepts needed by the GIM provider. */
    if (pVCpu->hm.s.fGIMTrapXcptUD)
        pVmcbCtrl->u32InterceptXcpt |= RT_BIT(X86_XCPT_UD);

    /* Set up unconditional intercepts and conditions. */
    pVmcbCtrl->u64InterceptCtrl = HMSVM_MANDATORY_GUEST_CTRL_INTERCEPTS
                                | SVM_CTRL_INTERCEPT_VMMCALL;

#ifdef HMSVM_ALWAYS_TRAP_TASK_SWITCH
    pVmcbCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_TASK_SWITCH;
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    /* Virtualized VMSAVE/VMLOAD. */
    pVmcbCtrl->LbrVirt.n.u1VirtVmsaveVmload = fUseVirtVmsaveVmload;
    if (!fUseVirtVmsaveVmload)
    {
        pVmcbCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_VMSAVE
                                     | SVM_CTRL_INTERCEPT_VMLOAD;
    }

    /* Virtual GIF. */
    pVmcbCtrl->IntCtrl.n.u1VGifEnable = fUseVGif;
    if (!fUseVGif)
    {
        pVmcbCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_CLGI
                                     | SVM_CTRL_INTERCEPT_STGI;
    }
#endif

    /* CR4 writes must always be intercepted for tracking PGM mode changes. */
    pVmcbCtrl->u16InterceptWrCRx = RT_BIT(4);

    /* Intercept all DRx reads and writes by default. Changed later on. */
    pVmcbCtrl->u16InterceptRdDRx = 0xffff;
    pVmcbCtrl->u16InterceptWrDRx = 0xffff;

    /* Virtualize masking of INTR interrupts. (reads/writes from/to CR8 go to the V_TPR register) */
    pVmcbCtrl->IntCtrl.n.u1VIntrMasking = 1;

    /* Ignore the priority in the virtual TPR. This is necessary for delivering PIC style (ExtInt) interrupts
       and we currently deliver both PIC and APIC interrupts alike, see hmR0SvmEvaluatePendingEvent() */
    pVmcbCtrl->IntCtrl.n.u1IgnoreTPR = 1;

    /* Set the IO permission bitmap physical addresses. */
    pVmcbCtrl->u64IOPMPhysAddr = g_HCPhysIOBitmap;

    /* LBR virtualization. */
    pVmcbCtrl->LbrVirt.n.u1LbrVirt = fUseLbrVirt;

    /* The host ASID MBZ, for the guest start with 1. */
    pVmcbCtrl->TLBCtrl.n.u32ASID = 1;

    /* Setup Nested Paging. This doesn't change throughout the execution time of the VM. */
    pVmcbCtrl->NestedPagingCtrl.n.u1NestedPaging = pVM->hm.s.fNestedPaging;

    /* Without Nested Paging, we need additionally intercepts. */
    if (!pVM->hm.s.fNestedPaging)
    {
        /* CR3 reads/writes must be intercepted; our shadow values differ from the guest values. */
        pVmcbCtrl->u16InterceptRdCRx |= RT_BIT(3);
        pVmcbCtrl->u16InterceptWrCRx |= RT_BIT(3);

        /* Intercept INVLPG and task switches (may change CR3, EFLAGS, LDT). */
        pVmcbCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_INVLPG
                                     | SVM_CTRL_INTERCEPT_TASK_SWITCH;

        /* Page faults must be intercepted to implement shadow paging. */
        pVmcbCtrl->u32InterceptXcpt |= RT_BIT(X86_XCPT_PF);
    }

    /* Setup Pause Filter for guest pause-loop (spinlock) exiting. */
    if (fUsePauseFilter)
    {
        Assert(pVM->hm.s.svm.cPauseFilter > 0);
        pVmcbCtrl->u16PauseFilterCount = pVM->hm.s.svm.cPauseFilter;
        if (fPauseFilterThreshold)
            pVmcbCtrl->u16PauseFilterThreshold = pVM->hm.s.svm.cPauseFilterThresholdTicks;
        pVmcbCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_PAUSE;
    }

    /*
     * Setup the MSR permission bitmap.
     * The following MSRs are saved/restored automatically during the world-switch.
     * Don't intercept guest read/write accesses to these MSRs.
     */
    uint8_t *pbMsrBitmap = (uint8_t *)pVCpu->hm.s.svm.pvMsrBitmap;
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_LSTAR,          SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_CSTAR,          SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K6_STAR,           SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_SF_MASK,        SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_FS_BASE,        SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_GS_BASE,        SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_KERNEL_GS_BASE, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_IA32_SYSENTER_CS,  SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_IA32_SYSENTER_ESP, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_IA32_SYSENTER_EIP, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    pVmcbCtrl->u64MSRPMPhysAddr = pVCpu->hm.s.svm.HCPhysMsrBitmap;

    /* Initially all VMCB clean bits MBZ indicating that everything should be loaded from the VMCB in memory. */
    Assert(pVmcbCtrl->u32VmcbCleanBits == 0);

    for (VMCPUID i = 1; i < pVM->cCpus; i++)
    {
        PVMCPU       pVCpuCur = &pVM->aCpus[i];
        PSVMVMCB     pVmcbCur = pVM->aCpus[i].hm.s.svm.pVmcb;
        AssertMsgReturn(pVmcbCur, ("Invalid pVmcb for vcpu[%u]\n", i), VERR_SVM_INVALID_PVMCB);
        PSVMVMCBCTRL pVmcbCtrlCur = &pVmcbCur->ctrl;

        /* Copy the VMCB control area. */
        memcpy(pVmcbCtrlCur, pVmcbCtrl, sizeof(*pVmcbCtrlCur));

        /* Copy the MSR bitmap and setup the VCPU-specific host physical address. */
        uint8_t *pbMsrBitmapCur = (uint8_t *)pVCpuCur->hm.s.svm.pvMsrBitmap;
        memcpy(pbMsrBitmapCur, pbMsrBitmap, SVM_MSRPM_PAGES << X86_PAGE_4K_SHIFT);
        pVmcbCtrlCur->u64MSRPMPhysAddr = pVCpuCur->hm.s.svm.HCPhysMsrBitmap;

        /* Initially all VMCB clean bits MBZ indicating that everything should be loaded from the VMCB in memory. */
        Assert(pVmcbCtrlCur->u32VmcbCleanBits == 0);

        /* Verify our assumption that GIM providers trap #UD uniformly across VCPUs initially. */
        Assert(pVCpuCur->hm.s.fGIMTrapXcptUD == pVCpu->hm.s.fGIMTrapXcptUD);
    }

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    LogRel(("HM: fUsePauseFilter=%RTbool fUseLbrVirt=%RTbool fUseVGif=%RTbool fUseVirtVmsaveVmload=%RTbool\n", fUsePauseFilter,
            fUseLbrVirt, fUseVGif, fUseVirtVmsaveVmload));
#else
    LogRel(("HM: fUsePauseFilter=%RTbool fUseLbrVirt=%RTbool\n", fUsePauseFilter, fUseLbrVirt));
#endif
    return VINF_SUCCESS;
}


/**
 * Gets a pointer to the currently active guest (or nested-guest) VMCB.
 *
 * @returns Pointer to the current context VMCB.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(PSVMVMCB) hmR0SvmGetCurrentVmcb(PVMCPU pVCpu)
{
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
        return pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pVmcb);
#endif
    return pVCpu->hm.s.svm.pVmcb;
}


/**
 * Gets a pointer to the nested-guest VMCB cache.
 *
 * @returns Pointer to the nested-guest VMCB cache.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(PSVMNESTEDVMCBCACHE) hmR0SvmGetNestedVmcbCache(PVMCPU pVCpu)
{
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    Assert(pVCpu->hm.s.svm.NstGstVmcbCache.fCacheValid);
    return &pVCpu->hm.s.svm.NstGstVmcbCache;
#else
    RT_NOREF(pVCpu);
    return NULL;
#endif
}


/**
 * Invalidates a guest page by guest virtual address.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   GCVirt      Guest virtual address of the page to invalidate.
 */
VMMR0DECL(int) SVMR0InvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt)
{
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.svm.fSupported);

    bool const fFlushPending = pVCpu->CTX_SUFF(pVM)->hm.s.svm.fAlwaysFlushTLB || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_TLB_FLUSH);

    /* Skip it if a TLB flush is already pending. */
    if (!fFlushPending)
    {
        Log4Func(("%#RGv\n", GCVirt));

        PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
        AssertMsgReturn(pVmcb, ("Invalid pVmcb!\n"), VERR_SVM_INVALID_PVMCB);

#if HC_ARCH_BITS == 32
        /* If we get a flush in 64-bit guest mode, then force a full TLB flush. INVLPGA takes only 32-bit addresses. */
        if (CPUMIsGuestInLongMode(pVCpu))
            VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
        else
#endif
        {
            SVMR0InvlpgA(GCVirt, pVmcb->ctrl.TLBCtrl.n.u32ASID);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbInvlpgVirt);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Flushes the appropriate tagged-TLB entries.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 * @param   pHostCpu    Pointer to the HM host-CPU info.
 */
static void hmR0SvmFlushTaggedTlb(PVMCPU pVCpu, PSVMVMCB pVmcb, PHMGLOBALCPUINFO pHostCpu)
{
    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one
     * we ran on last. This can happen both for start & resume due to long jumps back to
     * ring-3.
     *
     * We also force a TLB flush every time when executing a nested-guest VCPU as there is no
     * correlation between it and the physical CPU.
     *
     * If the TLB flush count changed, another VM (VCPU rather) has hit the ASID limit while
     * flushing the TLB, so we cannot reuse the ASIDs without flushing.
     */
    bool fNewAsid = false;
    Assert(pHostCpu->idCpu != NIL_RTCPUID);
    if (   pVCpu->hm.s.idLastCpu   != pHostCpu->idCpu
        || pVCpu->hm.s.cTlbFlushes != pHostCpu->cTlbFlushes
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        || CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx)
#endif
        )
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
        pVCpu->hm.s.fForceTLBFlush = true;
        fNewAsid = true;
    }

    /* Set TLB flush state as checked until we return from the world switch. */
    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, true);

    /* Check for explicit TLB flushes. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
    }

    /*
     * If the AMD CPU erratum 170, We need to flush the entire TLB for each world switch. Sad.
     * This Host CPU requirement takes precedence.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->hm.s.svm.fAlwaysFlushTLB)
    {
        pHostCpu->uCurrentAsid           = 1;
        pVCpu->hm.s.uCurrentAsid         = 1;
        pVCpu->hm.s.cTlbFlushes          = pHostCpu->cTlbFlushes;
        pVCpu->hm.s.idLastCpu            = pHostCpu->idCpu;
        pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;

        /* Clear the VMCB Clean Bit for NP while flushing the TLB. See @bugref{7152}. */
        pVmcb->ctrl.u32VmcbCleanBits    &= ~HMSVM_VMCB_CLEAN_NP;
    }
    else
    {
        pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_NOTHING;
        if (pVCpu->hm.s.fForceTLBFlush)
        {
            /* Clear the VMCB Clean Bit for NP while flushing the TLB. See @bugref{7152}. */
            pVmcb->ctrl.u32VmcbCleanBits    &= ~HMSVM_VMCB_CLEAN_NP;

            if (fNewAsid)
            {
                ++pHostCpu->uCurrentAsid;

                bool fHitASIDLimit = false;
                if (pHostCpu->uCurrentAsid >= pVM->hm.s.uMaxAsid)
                {
                    pHostCpu->uCurrentAsid = 1;      /* Wraparound at 1; host uses 0 */
                    pHostCpu->cTlbFlushes++;         /* All VCPUs that run on this host CPU must use a new ASID. */
                    fHitASIDLimit      = true;
                }

                if (   fHitASIDLimit
                    || pHostCpu->fFlushAsidBeforeUse)
                {
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;
                    pHostCpu->fFlushAsidBeforeUse = false;
                }

                pVCpu->hm.s.uCurrentAsid = pHostCpu->uCurrentAsid;
                pVCpu->hm.s.idLastCpu    = pHostCpu->idCpu;
                pVCpu->hm.s.cTlbFlushes  = pHostCpu->cTlbFlushes;
            }
            else
            {
                if (pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_FLUSH_BY_ASID)
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_SINGLE_CONTEXT;
                else
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;
            }

            pVCpu->hm.s.fForceTLBFlush = false;
        }
    }

    /* Update VMCB with the ASID. */
    if (pVmcb->ctrl.TLBCtrl.n.u32ASID != pVCpu->hm.s.uCurrentAsid)
    {
        pVmcb->ctrl.TLBCtrl.n.u32ASID = pVCpu->hm.s.uCurrentAsid;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_ASID;
    }

    AssertMsg(pVCpu->hm.s.idLastCpu == pHostCpu->idCpu,
              ("vcpu idLastCpu=%u hostcpu idCpu=%u\n", pVCpu->hm.s.idLastCpu, pHostCpu->idCpu));
    AssertMsg(pVCpu->hm.s.cTlbFlushes == pHostCpu->cTlbFlushes,
              ("Flush count mismatch for cpu %u (%u vs %u)\n", pHostCpu->idCpu, pVCpu->hm.s.cTlbFlushes, pHostCpu->cTlbFlushes));
    AssertMsg(pHostCpu->uCurrentAsid >= 1 && pHostCpu->uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d uCurrentAsid = %x\n", pHostCpu->idCpu, pHostCpu->uCurrentAsid));
    AssertMsg(pVCpu->hm.s.uCurrentAsid >= 1 && pVCpu->hm.s.uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d VM uCurrentAsid = %x\n", pHostCpu->idCpu, pVCpu->hm.s.uCurrentAsid));

#ifdef VBOX_WITH_STATISTICS
    if (pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_NOTHING)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch);
    else if (   pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_SINGLE_CONTEXT
             || pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_SINGLE_CONTEXT_RETAIN_GLOBALS)
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushAsid);
    }
    else
    {
        Assert(pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_ENTIRE);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushEntire);
    }
#endif
}


/** @name 64-bit guest on 32-bit host OS helper functions.
 *
 * The host CPU is still 64-bit capable but the host OS is running in 32-bit
 * mode (code segment, paging). These wrappers/helpers perform the necessary
 * bits for the 32->64 switcher.
 *
 * @{ */
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
/**
 * Prepares for and executes VMRUN (64-bit guests on a 32-bit host).
 *
 * @returns VBox status code.
 * @param   HCPhysVmcbHost  Physical address of host VMCB.
 * @param   HCPhysVmcb      Physical address of the VMCB.
 * @param   pCtx            Pointer to the guest-CPU context.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLASM(int) SVMR0VMSwitcherRun64(RTHCPHYS HCPhysVmcbHost, RTHCPHYS HCPhysVmcb, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF2(pVM, pCtx);
    uint32_t aParam[8];
    aParam[0] = RT_LO_U32(HCPhysVmcbHost);              /* Param 1: HCPhysVmcbHost - Lo. */
    aParam[1] = RT_HI_U32(HCPhysVmcbHost);              /* Param 1: HCPhysVmcbHost - Hi. */
    aParam[2] = RT_LO_U32(HCPhysVmcb);                  /* Param 2: HCPhysVmcb - Lo. */
    aParam[3] = RT_HI_U32(HCPhysVmcb);                  /* Param 2: HCPhysVmcb - Hi. */
    aParam[4] = VM_RC_ADDR(pVM, pVM);
    aParam[5] = 0;
    aParam[6] = VM_RC_ADDR(pVM, pVCpu);
    aParam[7] = 0;

    return SVMR0Execute64BitsHandler(pVCpu, HM64ON32OP_SVMRCVMRun64, RT_ELEMENTS(aParam), &aParam[0]);
}


/**
 * Executes the specified VMRUN handler in 64-bit mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmOp       The operation to perform.
 * @param   cParams     Number of parameters.
 * @param   paParam     Array of 32-bit parameters.
 */
VMMR0DECL(int) SVMR0Execute64BitsHandler(PVMCPU pVCpu, HM64ON32OP enmOp, uint32_t cParams, uint32_t *paParam)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    AssertReturn(pVM->hm.s.pfnHost32ToGuest64R0, VERR_HM_NO_32_TO_64_SWITCHER);
    Assert(enmOp > HM64ON32OP_INVALID && enmOp < HM64ON32OP_END);

    /* Disable interrupts. */
    RTHCUINTREG const fEFlags = ASMIntDisableFlags();

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    RTCPUID idHostCpu = RTMpCpuId();
    CPUMR0SetLApic(pVCpu, idHostCpu);
#endif

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVCpu));
    CPUMSetHyperEIP(pVCpu, enmOp);
    for (int i = (int)cParams - 1; i >= 0; i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    /* Call the switcher. */
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatWorldSwitch3264, z);
    int rc = pVM->hm.s.pfnHost32ToGuest64R0(pVM, RT_UOFFSETOF_DYN(VM, aCpus[pVCpu->idCpu].cpum) - RT_UOFFSETOF(VM, cpum));
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatWorldSwitch3264, z);

    /* Restore interrupts. */
    ASMSetFlags(fEFlags);
    return rc;
}

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) */
/** @} */


/**
 * Sets an exception intercept in the specified VMCB.
 *
 * @param   pVmcb       Pointer to the VM control block.
 * @param   uXcpt       The exception (X86_XCPT_*).
 */
DECLINLINE(void) hmR0SvmSetXcptIntercept(PSVMVMCB pVmcb, uint8_t uXcpt)
{
    if (!(pVmcb->ctrl.u32InterceptXcpt & RT_BIT(uXcpt)))
    {
        pVmcb->ctrl.u32InterceptXcpt |= RT_BIT(uXcpt);
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
    }
}


/**
 * Clears an exception intercept in the specified VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 * @param   uXcpt       The exception (X86_XCPT_*).
 *
 * @remarks This takes into account if we're executing a nested-guest and only
 *          removes the exception intercept if both the guest -and- nested-guest
 *          are not intercepting it.
 */
DECLINLINE(void) hmR0SvmClearXcptIntercept(PVMCPU pVCpu, PSVMVMCB pVmcb, uint8_t uXcpt)
{
    Assert(uXcpt != X86_XCPT_DB);
    Assert(uXcpt != X86_XCPT_AC);
#ifndef HMSVM_ALWAYS_TRAP_ALL_XCPTS
    if (pVmcb->ctrl.u32InterceptXcpt & RT_BIT(uXcpt))
    {
        bool fRemove = true;
# ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        /* Only remove the intercept if the nested-guest is also not intercepting it! */
        PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        if (CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
        {
            PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = hmR0SvmGetNestedVmcbCache(pVCpu);
            fRemove = !(pVmcbNstGstCache->u32InterceptXcpt & RT_BIT(uXcpt));
        }
# else
        RT_NOREF(pVCpu);
# endif
        if (fRemove)
        {
            pVmcb->ctrl.u32InterceptXcpt &= ~RT_BIT(uXcpt);
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
    }
#else
    RT_NOREF3(pVCpu, pVmcb, uXcpt);
#endif
}


/**
 * Sets a control intercept in the specified VMCB.
 *
 * @param   pVmcb           Pointer to the VM control block.
 * @param   fCtrlIntercept  The control intercept (SVM_CTRL_INTERCEPT_*).
 */
DECLINLINE(void) hmR0SvmSetCtrlIntercept(PSVMVMCB pVmcb, uint64_t fCtrlIntercept)
{
    if (!(pVmcb->ctrl.u64InterceptCtrl & fCtrlIntercept))
    {
        pVmcb->ctrl.u64InterceptCtrl |= fCtrlIntercept;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
    }
}


/**
 * Clears a control intercept in the specified VMCB.
 *
 * @returns @c true if the intercept is still set, @c false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmcb           Pointer to the VM control block.
 * @param   fCtrlIntercept  The control intercept (SVM_CTRL_INTERCEPT_*).
 *
 * @remarks This takes into account if we're executing a nested-guest and only
 *          removes the control intercept if both the guest -and- nested-guest
 *          are not intercepting it.
 */
static bool hmR0SvmClearCtrlIntercept(PVMCPU pVCpu, PSVMVMCB pVmcb, uint64_t fCtrlIntercept)
{
    if (pVmcb->ctrl.u64InterceptCtrl & fCtrlIntercept)
    {
        bool fRemove = true;
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        /* Only remove the control intercept if the nested-guest is also not intercepting it! */
        if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
        {
            PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = hmR0SvmGetNestedVmcbCache(pVCpu);
            fRemove = !(pVmcbNstGstCache->u64InterceptCtrl & fCtrlIntercept);
        }
#else
        RT_NOREF(pVCpu);
#endif
        if (fRemove)
        {
            pVmcb->ctrl.u64InterceptCtrl &= ~fCtrlIntercept;
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
    }

    return RT_BOOL(pVmcb->ctrl.u64InterceptCtrl & fCtrlIntercept);
}


/**
 * Exports the guest (or nested-guest) CR0 into the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks This assumes we always pre-load the guest FPU.
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportGuestCR0(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    PCPUMCTX       pCtx = &pVCpu->cpum.GstCtx;
    uint64_t const uGuestCr0  = pCtx->cr0;
    uint64_t       uShadowCr0 = uGuestCr0;

    /* Always enable caching. */
    uShadowCr0 &= ~(X86_CR0_CD | X86_CR0_NW);

    /* When Nested Paging is not available use shadow page tables and intercept #PFs (latter done in SVMR0SetupVM()). */
    if (!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging)
    {
        uShadowCr0 |= X86_CR0_PG      /* Use shadow page tables. */
                    |  X86_CR0_WP;    /* Guest CPL 0 writes to its read-only pages should cause a #PF #VMEXIT. */
    }

    /*
     * Use the #MF style of legacy-FPU error reporting for now. Although AMD-V has MSRs that
     * lets us isolate the host from it, IEM/REM still needs work to emulate it properly,
     * see @bugref{7243#c103}.
     */
    if (!(uGuestCr0 & X86_CR0_NE))
    {
        uShadowCr0 |= X86_CR0_NE;
        hmR0SvmSetXcptIntercept(pVmcb, X86_XCPT_MF);
    }
    else
        hmR0SvmClearXcptIntercept(pVCpu, pVmcb, X86_XCPT_MF);

    /*
     * If the shadow and guest CR0 are identical we can avoid intercepting CR0 reads.
     *
     * CR0 writes still needs interception as PGM requires tracking paging mode changes,
     * see @bugref{6944}.
     *
     * We also don't ever want to honor weird things like cache disable from the guest.
     * However, we can avoid intercepting changes to the TS & MP bits by clearing the CR0
     * write intercept below and keeping SVM_CTRL_INTERCEPT_CR0_SEL_WRITE instead.
     */
    if (uShadowCr0 == uGuestCr0)
    {
        if (!CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
        {
            pVmcb->ctrl.u16InterceptRdCRx &= ~RT_BIT(0);
            pVmcb->ctrl.u16InterceptWrCRx &= ~RT_BIT(0);
            Assert(pVmcb->ctrl.u64InterceptCtrl & SVM_CTRL_INTERCEPT_CR0_SEL_WRITE);
        }
        else
        {
            /* If the nested-hypervisor intercepts CR0 reads/writes, we need to continue intercepting them. */
            PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = hmR0SvmGetNestedVmcbCache(pVCpu);
            pVmcb->ctrl.u16InterceptRdCRx = (pVmcb->ctrl.u16InterceptRdCRx       & ~RT_BIT(0))
                                          | (pVmcbNstGstCache->u16InterceptRdCRx &  RT_BIT(0));
            pVmcb->ctrl.u16InterceptWrCRx = (pVmcb->ctrl.u16InterceptWrCRx       & ~RT_BIT(0))
                                          | (pVmcbNstGstCache->u16InterceptWrCRx &  RT_BIT(0));
        }
    }
    else
    {
        pVmcb->ctrl.u16InterceptRdCRx |= RT_BIT(0);
        pVmcb->ctrl.u16InterceptWrCRx |= RT_BIT(0);
    }
    pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;

    Assert(!RT_HI_U32(uShadowCr0));
    if (pVmcb->guest.u64CR0 != uShadowCr0)
    {
        pVmcb->guest.u64CR0 = uShadowCr0;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CRX_EFER;
    }
}


/**
 * Exports the guest (or nested-guest) CR3 into the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportGuestCR3(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (pVM->hm.s.fNestedPaging)
    {
        PGMMODE enmShwPagingMode;
#if HC_ARCH_BITS == 32
        if (CPUMIsGuestInLongModeEx(pCtx))
            enmShwPagingMode = PGMMODE_AMD64_NX;
        else
#endif
            enmShwPagingMode = PGMGetHostMode(pVM);

        pVmcb->ctrl.u64NestedPagingCR3 = PGMGetNestedCR3(pVCpu, enmShwPagingMode);
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_NP;
        pVmcb->guest.u64CR3 = pCtx->cr3;
        Assert(pVmcb->ctrl.u64NestedPagingCR3);
    }
    else
        pVmcb->guest.u64CR3 = PGMGetHyperCR3(pVCpu);

    pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CRX_EFER;
}


/**
 * Exports the guest (or nested-guest) CR4 into the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmExportGuestCR4(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    uint64_t uShadowCr4 = pCtx->cr4;
    if (!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging)
    {
        switch (pVCpu->hm.s.enmShadowMode)
        {
            case PGMMODE_REAL:
            case PGMMODE_PROTECTED:     /* Protected mode, no paging. */
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;

            case PGMMODE_32_BIT:        /* 32-bit paging. */
                uShadowCr4 &= ~X86_CR4_PAE;
                break;

            case PGMMODE_PAE:           /* PAE paging. */
            case PGMMODE_PAE_NX:        /* PAE paging with NX enabled. */
                /** Must use PAE paging as we could use physical memory > 4 GB */
                uShadowCr4 |= X86_CR4_PAE;
                break;

            case PGMMODE_AMD64:         /* 64-bit AMD paging (long mode). */
            case PGMMODE_AMD64_NX:      /* 64-bit AMD paging (long mode) with NX enabled. */
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                break;
#else
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif

            default:                    /* shut up gcc */
                return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
        }
    }

    /* Whether to save/load/restore XCR0 during world switch depends on CR4.OSXSAVE and host+guest XCR0. */
    pVCpu->hm.s.fLoadSaveGuestXcr0 = (pCtx->cr4 & X86_CR4_OSXSAVE) && pCtx->aXcr[0] != ASMGetXcr0();

    /* Avoid intercepting CR4 reads if the guest and shadow CR4 values are identical. */
    if (uShadowCr4 == pCtx->cr4)
    {
        if (!CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
            pVmcb->ctrl.u16InterceptRdCRx &= ~RT_BIT(4);
        else
        {
            /* If the nested-hypervisor intercepts CR4 reads, we need to continue intercepting them. */
            PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = hmR0SvmGetNestedVmcbCache(pVCpu);
            pVmcb->ctrl.u16InterceptRdCRx = (pVmcb->ctrl.u16InterceptRdCRx       & ~RT_BIT(4))
                                          | (pVmcbNstGstCache->u16InterceptRdCRx &  RT_BIT(4));
        }
    }
    else
        pVmcb->ctrl.u16InterceptRdCRx |= RT_BIT(4);

    /* CR4 writes are always intercepted (both guest, nested-guest) for tracking PGM mode changes. */
    Assert(pVmcb->ctrl.u16InterceptWrCRx & RT_BIT(4));

    /* Update VMCB with the shadow CR4 the appropriate VMCB clean bits. */
    Assert(!RT_HI_U32(uShadowCr4));
    pVmcb->guest.u64CR4 = uShadowCr4;
    pVmcb->ctrl.u32VmcbCleanBits &= ~(HMSVM_VMCB_CLEAN_CRX_EFER | HMSVM_VMCB_CLEAN_INTERCEPTS);

    return VINF_SUCCESS;
}


/**
 * Exports the guest (or nested-guest) control registers into the VMCB.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmExportGuestControlRegs(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_CR_MASK)
    {
        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_CR0)
            hmR0SvmExportGuestCR0(pVCpu, pVmcb);

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_CR2)
        {
            pVmcb->guest.u64CR2 = pVCpu->cpum.GstCtx.cr2;
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CR2;
        }

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_CR3)
            hmR0SvmExportGuestCR3(pVCpu, pVmcb);

        /* CR4 re-loading is ASSUMED to be done everytime we get in from ring-3! (XCR0) */
        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_CR4)
        {
            int rc = hmR0SvmExportGuestCR4(pVCpu, pVmcb);
            if (RT_FAILURE(rc))
                return rc;
        }

        pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_GUEST_CR_MASK;
    }
    return VINF_SUCCESS;
}


/**
 * Exports the guest (or nested-guest) segment registers into the VMCB.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportGuestSegmentRegs(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;

    /* Guest segment registers. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SREG_MASK)
    {
        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_CS)
            HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, CS, cs);

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SS)
        {
            HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, SS, ss);
            pVmcb->guest.u8CPL = pCtx->ss.Attr.n.u2Dpl;
        }

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_DS)
            HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, DS, ds);

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_ES)
            HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, ES, es);

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_FS)
            HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, FS, fs);

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_GS)
            HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, GS, gs);

        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_SEG;
    }

    /* Guest TR. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_TR)
        HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, TR, tr);

    /* Guest LDTR. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_LDTR)
        HMSVM_SEG_REG_COPY_TO_VMCB(pCtx, &pVmcb->guest, LDTR, ldtr);

    /* Guest GDTR. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_GDTR)
    {
        pVmcb->guest.GDTR.u32Limit = pCtx->gdtr.cbGdt;
        pVmcb->guest.GDTR.u64Base  = pCtx->gdtr.pGdt;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DT;
    }

    /* Guest IDTR. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_IDTR)
    {
        pVmcb->guest.IDTR.u32Limit = pCtx->idtr.cbIdt;
        pVmcb->guest.IDTR.u64Base  = pCtx->idtr.pIdt;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DT;
    }

    pVCpu->hm.s.fCtxChanged &= ~(  HM_CHANGED_GUEST_SREG_MASK
                                 | HM_CHANGED_GUEST_TABLE_MASK);
}


/**
 * Exports the guest (or nested-guest) MSRs into the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportGuestMsrs(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;

    /* Guest Sysenter MSRs. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SYSENTER_MSR_MASK)
    {
        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SYSENTER_CS_MSR)
            pVmcb->guest.u64SysEnterCS  = pCtx->SysEnter.cs;

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SYSENTER_EIP_MSR)
            pVmcb->guest.u64SysEnterEIP = pCtx->SysEnter.eip;

        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SYSENTER_ESP_MSR)
            pVmcb->guest.u64SysEnterESP = pCtx->SysEnter.esp;
    }

    /*
     * Guest EFER MSR.
     * AMD-V requires guest EFER.SVME to be set. Weird.
     * See AMD spec. 15.5.1 "Basic Operation" | "Canonicalization and Consistency Checks".
     */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_EFER_MSR)
    {
        pVmcb->guest.u64EFER = pCtx->msrEFER | MSR_K6_EFER_SVME;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CRX_EFER;
    }

    /* If the guest isn't in 64-bit mode, clear MSR_K6_LME bit, otherwise SVM expects amd64 shadow paging. */
    if (   !CPUMIsGuestInLongModeEx(pCtx)
        && (pCtx->msrEFER & MSR_K6_EFER_LME))
    {
        pVmcb->guest.u64EFER &= ~MSR_K6_EFER_LME;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CRX_EFER;
    }

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_SYSCALL_MSRS)
    {
        pVmcb->guest.u64STAR   = pCtx->msrSTAR;
        pVmcb->guest.u64LSTAR  = pCtx->msrLSTAR;
        pVmcb->guest.u64CSTAR  = pCtx->msrCSTAR;
        pVmcb->guest.u64SFMASK = pCtx->msrSFMASK;
    }

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_KERNEL_GS_BASE)
        pVmcb->guest.u64KernelGSBase = pCtx->msrKERNELGSBASE;

    pVCpu->hm.s.fCtxChanged &= ~(  HM_CHANGED_GUEST_SYSENTER_MSR_MASK
                                 | HM_CHANGED_GUEST_EFER_MSR
                                 | HM_CHANGED_GUEST_SYSCALL_MSRS
                                 | HM_CHANGED_GUEST_KERNEL_GS_BASE);

    /*
     * Setup the PAT MSR (applicable for Nested Paging only).
     *
     * While guests can modify and see the modified values through the shadow values,
     * we shall not honor any guest modifications of this MSR to ensure caching is always
     * enabled similar to how we clear CR0.CD and NW bits.
     *
     * For nested-guests this needs to always be set as well, see @bugref{7243#c109}.
     */
    pVmcb->guest.u64PAT = MSR_IA32_CR_PAT_INIT_VAL;

    /* Enable the last branch record bit if LBR virtualization is enabled. */
    if (pVmcb->ctrl.LbrVirt.n.u1LbrVirt)
        pVmcb->guest.u64DBGCTL = MSR_IA32_DEBUGCTL_LBR;
}


/**
 * Exports the guest (or nested-guest) debug state into the VMCB and programs
 * the necessary intercepts accordingly.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 * @remarks Requires EFLAGS to be up-to-date in the VMCB!
 */
static void hmR0SvmExportSharedDebugState(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;

    /*
     * Anyone single stepping on the host side? If so, we'll have to use the
     * trap flag in the guest EFLAGS since AMD-V doesn't have a trap flag on
     * the VMM level like the VT-x implementations does.
     */
    bool       fInterceptMovDRx = false;
    bool const fStepping = pVCpu->hm.s.fSingleInstruction || DBGFIsStepping(pVCpu);
    if (fStepping)
    {
        pVCpu->hm.s.fClearTrapFlag = true;
        pVmcb->guest.u64RFlags |= X86_EFL_TF;
        fInterceptMovDRx = true; /* Need clean DR6, no guest mess. */
    }

    if (   fStepping
        || (CPUMGetHyperDR7(pVCpu) & X86_DR7_ENABLED_MASK))
    {
        /*
         * Use the combined guest and host DRx values found in the hypervisor
         * register set because the debugger has breakpoints active or someone
         * is single stepping on the host side.
         *
         * Note! DBGF expects a clean DR6 state before executing guest code.
         */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
        if (   CPUMIsGuestInLongModeEx(pCtx)
            && !CPUMIsHyperDebugStateActivePending(pVCpu))
        {
            CPUMR0LoadHyperDebugState(pVCpu, false /* include DR6 */);
            Assert(!CPUMIsGuestDebugStateActivePending(pVCpu));
            Assert(CPUMIsHyperDebugStateActivePending(pVCpu));
        }
        else
#endif
        if (!CPUMIsHyperDebugStateActive(pVCpu))
        {
            CPUMR0LoadHyperDebugState(pVCpu, false /* include DR6 */);
            Assert(!CPUMIsGuestDebugStateActive(pVCpu));
            Assert(CPUMIsHyperDebugStateActive(pVCpu));
        }

        /* Update DR6 & DR7. (The other DRx values are handled by CPUM one way or the other.) */
        if (   pVmcb->guest.u64DR6 != X86_DR6_INIT_VAL
            || pVmcb->guest.u64DR7 != CPUMGetHyperDR7(pVCpu))
        {
            pVmcb->guest.u64DR7 = CPUMGetHyperDR7(pVCpu);
            pVmcb->guest.u64DR6 = X86_DR6_INIT_VAL;
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DRX;
        }

        /** @todo If we cared, we could optimize to allow the guest to read registers
         *        with the same values. */
        fInterceptMovDRx = true;
        pVCpu->hm.s.fUsingHyperDR7 = true;
        Log5(("hmR0SvmExportSharedDebugState: Loaded hyper DRx\n"));
    }
    else
    {
        /*
         * Update DR6, DR7 with the guest values if necessary.
         */
        if (   pVmcb->guest.u64DR7 != pCtx->dr[7]
            || pVmcb->guest.u64DR6 != pCtx->dr[6])
        {
            pVmcb->guest.u64DR7 = pCtx->dr[7];
            pVmcb->guest.u64DR6 = pCtx->dr[6];
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DRX;
        }
        pVCpu->hm.s.fUsingHyperDR7 = false;

        /*
         * If the guest has enabled debug registers, we need to load them prior to
         * executing guest code so they'll trigger at the right time.
         */
        if (pCtx->dr[7] & (X86_DR7_ENABLED_MASK | X86_DR7_GD)) /** @todo Why GD? */
        {
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
            if (   CPUMIsGuestInLongModeEx(pCtx)
                && !CPUMIsGuestDebugStateActivePending(pVCpu))
            {
                CPUMR0LoadGuestDebugState(pVCpu, false /* include DR6 */);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxArmed);
                Assert(!CPUMIsHyperDebugStateActivePending(pVCpu));
                Assert(CPUMIsGuestDebugStateActivePending(pVCpu));
            }
            else
#endif
            if (!CPUMIsGuestDebugStateActive(pVCpu))
            {
                CPUMR0LoadGuestDebugState(pVCpu, false /* include DR6 */);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxArmed);
                Assert(!CPUMIsHyperDebugStateActive(pVCpu));
                Assert(CPUMIsGuestDebugStateActive(pVCpu));
            }
            Log5(("hmR0SvmExportSharedDebugState: Loaded guest DRx\n"));
        }
        /*
         * If no debugging enabled, we'll lazy load DR0-3. We don't need to
         * intercept #DB as DR6 is updated in the VMCB.
         *
         * Note! If we cared and dared, we could skip intercepting \#DB here.
         *       However, \#DB shouldn't be performance critical, so we'll play safe
         *       and keep the code similar to the VT-x code and always intercept it.
         */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
        else if (   !CPUMIsGuestDebugStateActivePending(pVCpu)
                 && !CPUMIsGuestDebugStateActive(pVCpu))
#else
        else if (!CPUMIsGuestDebugStateActive(pVCpu))
#endif
        {
            fInterceptMovDRx = true;
        }
    }

    Assert(pVmcb->ctrl.u32InterceptXcpt & RT_BIT_32(X86_XCPT_DB));
    if (fInterceptMovDRx)
    {
        if (   pVmcb->ctrl.u16InterceptRdDRx != 0xffff
            || pVmcb->ctrl.u16InterceptWrDRx != 0xffff)
        {
            pVmcb->ctrl.u16InterceptRdDRx = 0xffff;
            pVmcb->ctrl.u16InterceptWrDRx = 0xffff;
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
    }
    else
    {
        if (   pVmcb->ctrl.u16InterceptRdDRx
            || pVmcb->ctrl.u16InterceptWrDRx)
        {
            pVmcb->ctrl.u16InterceptRdDRx = 0;
            pVmcb->ctrl.u16InterceptWrDRx = 0;
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
    }
    Log4Func(("DR6=%#RX64 DR7=%#RX64\n", pCtx->dr[6], pCtx->dr[7]));
}

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Exports the nested-guest hardware virtualization state into the nested-guest
 * VMCB.
 *
 * @param   pVCpu         The cross context virtual CPU structure.
 * @param   pVmcbNstGst   Pointer to the nested-guest VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportGuestHwvirtStateNested(PVMCPU pVCpu, PSVMVMCB pVmcbNstGst)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_HWVIRT)
    {
        /*
         * Ensure the nested-guest pause-filter counters don't exceed the outer guest values esp.
         * since SVM doesn't have a preemption timer.
         *
         * We do this here rather than in hmR0SvmSetupVmcbNested() as we may have been executing the
         * nested-guest in IEM incl. PAUSE instructions which would update the pause-filter counters
         * and may continue execution in SVM R0 without a nested-guest #VMEXIT in between.
         */
        PVM            pVM = pVCpu->CTX_SUFF(pVM);
        PSVMVMCBCTRL   pVmcbNstGstCtrl = &pVmcbNstGst->ctrl;
        uint16_t const uGuestPauseFilterCount     = pVM->hm.s.svm.cPauseFilter;
        uint16_t const uGuestPauseFilterThreshold = pVM->hm.s.svm.cPauseFilterThresholdTicks;
        if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_PAUSE))
        {
            PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
            pVmcbNstGstCtrl->u16PauseFilterCount     = RT_MIN(pCtx->hwvirt.svm.cPauseFilter, uGuestPauseFilterCount);
            pVmcbNstGstCtrl->u16PauseFilterThreshold = RT_MIN(pCtx->hwvirt.svm.cPauseFilterThreshold, uGuestPauseFilterThreshold);
            pVmcbNstGstCtrl->u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
        else
        {
            pVmcbNstGstCtrl->u16PauseFilterCount     = uGuestPauseFilterCount;
            pVmcbNstGstCtrl->u16PauseFilterThreshold = uGuestPauseFilterThreshold;
        }

        pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_GUEST_HWVIRT;
    }
}
#endif

/**
 * Exports the guest APIC TPR state into the VMCB.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 */
static int hmR0SvmExportGuestApicTpr(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_APIC_TPR)
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        if (   PDMHasApic(pVM)
            && APICIsEnabled(pVCpu))
        {
            bool    fPendingIntr;
            uint8_t u8Tpr;
            int rc = APICGetTpr(pVCpu, &u8Tpr, &fPendingIntr, NULL /* pu8PendingIrq */);
            AssertRCReturn(rc, rc);

            /* Assume that we need to trap all TPR accesses and thus need not check on
               every #VMEXIT if we should update the TPR. */
            Assert(pVmcb->ctrl.IntCtrl.n.u1VIntrMasking);
            pVCpu->hm.s.svm.fSyncVTpr = false;

            if (!pVM->hm.s.fTPRPatchingActive)
            {
                /* Bits 3-0 of the VTPR field correspond to bits 7-4 of the TPR (which is the Task-Priority Class). */
                pVmcb->ctrl.IntCtrl.n.u8VTPR = (u8Tpr >> 4);

                /* If there are interrupts pending, intercept CR8 writes to evaluate ASAP if we
                   can deliver the interrupt to the guest. */
                if (fPendingIntr)
                    pVmcb->ctrl.u16InterceptWrCRx |= RT_BIT(8);
                else
                {
                    pVmcb->ctrl.u16InterceptWrCRx &= ~RT_BIT(8);
                    pVCpu->hm.s.svm.fSyncVTpr = true;
                }

                pVmcb->ctrl.u32VmcbCleanBits &= ~(HMSVM_VMCB_CLEAN_INTERCEPTS | HMSVM_VMCB_CLEAN_INT_CTRL);
            }
            else
            {
                /* 32-bit guests uses LSTAR MSR for patching guest code which touches the TPR. */
                pVmcb->guest.u64LSTAR = u8Tpr;
                uint8_t *pbMsrBitmap = (uint8_t *)pVCpu->hm.s.svm.pvMsrBitmap;

                /* If there are interrupts pending, intercept LSTAR writes, otherwise don't intercept reads or writes. */
                if (fPendingIntr)
                    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_LSTAR, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_INTERCEPT_WRITE);
                else
                {
                    hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_LSTAR, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
                    pVCpu->hm.s.svm.fSyncVTpr = true;
                }
                pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_IOPM_MSRPM;
            }
        }
        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_APIC_TPR);
    }
    return VINF_SUCCESS;
}


/**
 * Sets up the exception interrupts required for guest (or nested-guest)
 * execution in the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportGuestXcptIntercepts(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* If we modify intercepts from here, please check & adjust hmR0SvmMergeVmcbCtrlsNested() if required. */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS)
    {
        /* Trap #UD for GIM provider (e.g. for hypercalls). */
        if (pVCpu->hm.s.fGIMTrapXcptUD)
            hmR0SvmSetXcptIntercept(pVmcb, X86_XCPT_UD);
        else
            hmR0SvmClearXcptIntercept(pVCpu, pVmcb, X86_XCPT_UD);

        /* Trap #BP for INT3 debug breakpoints set by the VM debugger. */
        if (pVCpu->CTX_SUFF(pVM)->dbgf.ro.cEnabledInt3Breakpoints)
            hmR0SvmSetXcptIntercept(pVmcb, X86_XCPT_BP);
        else
            hmR0SvmClearXcptIntercept(pVCpu, pVmcb, X86_XCPT_BP);

        /* The remaining intercepts are handled elsewhere, e.g. in hmR0SvmExportGuestCR0(). */
        pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS;
    }
}


#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Merges guest and nested-guest intercepts for executing the nested-guest using
 * hardware-assisted SVM.
 *
 * This merges the guest and nested-guest intercepts in a way that if the outer
 * guest intercept is set we need to intercept it in the nested-guest as
 * well.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmcbNstGst     Pointer to the nested-guest VM control block.
 */
static void hmR0SvmMergeVmcbCtrlsNested(PVMCPU pVCpu)
{
    PVM          pVM             = pVCpu->CTX_SUFF(pVM);
    PCSVMVMCB    pVmcb           = pVCpu->hm.s.svm.pVmcb;
    PSVMVMCB     pVmcbNstGst     = pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pVmcb);
    PSVMVMCBCTRL pVmcbNstGstCtrl = &pVmcbNstGst->ctrl;

    /* Merge the guest's CR intercepts into the nested-guest VMCB. */
    pVmcbNstGstCtrl->u16InterceptRdCRx |= pVmcb->ctrl.u16InterceptRdCRx;
    pVmcbNstGstCtrl->u16InterceptWrCRx |= pVmcb->ctrl.u16InterceptWrCRx;

    /* Always intercept CR4 writes for tracking PGM mode changes. */
    pVmcbNstGstCtrl->u16InterceptWrCRx |= RT_BIT(4);

    /* Without nested paging, intercept CR3 reads and writes as we load shadow page tables. */
    if (!pVM->hm.s.fNestedPaging)
    {
        pVmcbNstGstCtrl->u16InterceptRdCRx |= RT_BIT(3);
        pVmcbNstGstCtrl->u16InterceptWrCRx |= RT_BIT(3);
    }

    /** @todo Figure out debugging with nested-guests, till then just intercept
     *        all DR[0-15] accesses. */
    pVmcbNstGstCtrl->u16InterceptRdDRx |= 0xffff;
    pVmcbNstGstCtrl->u16InterceptWrDRx |= 0xffff;

    /*
     * Merge the guest's exception intercepts into the nested-guest VMCB.
     *
     * - \#UD: Exclude these as the outer guest's GIM hypercalls are not applicable
     * while executing the nested-guest.
     *
     * - \#BP: Exclude breakpoints set by the VM debugger for the outer guest. This can
     * be tweaked later depending on how we wish to implement breakpoints.
     *
     * Warning!! This ASSUMES we only intercept \#UD for hypercall purposes and \#BP
     * for VM debugger breakpoints, see hmR0SvmExportGuestXcptIntercepts().
     */
#ifndef HMSVM_ALWAYS_TRAP_ALL_XCPTS
    pVmcbNstGstCtrl->u32InterceptXcpt  |= (pVmcb->ctrl.u32InterceptXcpt & ~(  RT_BIT(X86_XCPT_UD)
                                                                            | RT_BIT(X86_XCPT_BP)));
#else
    pVmcbNstGstCtrl->u32InterceptXcpt  |= pVmcb->ctrl.u32InterceptXcpt;
#endif

    /*
     * Adjust intercepts while executing the nested-guest that differ from the
     * outer guest intercepts.
     *
     * - VINTR: Exclude the outer guest intercept as we don't need to cause VINTR #VMEXITs
     *   that belong to the nested-guest to the outer guest.
     *
     * - VMMCALL: Exclude the outer guest intercept as when it's also not intercepted by
     *   the nested-guest, the physical CPU raises a \#UD exception as expected.
     */
    pVmcbNstGstCtrl->u64InterceptCtrl  |= (pVmcb->ctrl.u64InterceptCtrl & ~(  SVM_CTRL_INTERCEPT_VINTR
                                                                            | SVM_CTRL_INTERCEPT_VMMCALL))
                                        | HMSVM_MANDATORY_GUEST_CTRL_INTERCEPTS;

    Assert(   (pVmcbNstGstCtrl->u64InterceptCtrl & HMSVM_MANDATORY_GUEST_CTRL_INTERCEPTS)
           == HMSVM_MANDATORY_GUEST_CTRL_INTERCEPTS);

    /* Finally, update the VMCB clean bits. */
    pVmcbNstGstCtrl->u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
}
#endif


/**
 * Selects the appropriate function to run guest code.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmSelectVMRunHandler(PVMCPU pVCpu)
{
    if (CPUMIsGuestInLongMode(pVCpu))
    {
#ifndef VBOX_ENABLE_64_BITS_GUESTS
        return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif
        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests);    /* Guaranteed by hmR3InitFinalizeR0(). */
#if HC_ARCH_BITS == 32
        /* 32-bit host. We need to switch to 64-bit before running the 64-bit guest. */
        pVCpu->hm.s.svm.pfnVMRun = SVMR0VMSwitcherRun64;
#else
        /* 64-bit host or hybrid host. */
        pVCpu->hm.s.svm.pfnVMRun = SVMR0VMRun64;
#endif
    }
    else
    {
        /* Guest is not in long mode, use the 32-bit handler. */
        pVCpu->hm.s.svm.pfnVMRun = SVMR0VMRun;
    }
    return VINF_SUCCESS;
}


/**
 * Enters the AMD-V session.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pHostCpu    Pointer to the CPU info struct.
 */
VMMR0DECL(int) SVMR0Enter(PVMCPU pVCpu, PHMGLOBALCPUINFO pHostCpu)
{
    AssertPtr(pVCpu);
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.svm.fSupported);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    RT_NOREF(pHostCpu);

    LogFlowFunc(("pVCpu=%p\n", pVCpu));
    Assert((pVCpu->hm.s.fCtxChanged & (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE))
                                   == (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE));

    pVCpu->hm.s.fLeaveDone = false;
    return VINF_SUCCESS;
}


/**
 * Thread-context callback for AMD-V.
 *
 * @param   enmEvent        The thread-context event.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fGlobalInit     Whether global VT-x/AMD-V init. is used.
 * @thread  EMT(pVCpu)
 */
VMMR0DECL(void) SVMR0ThreadCtxCallback(RTTHREADCTXEVENT enmEvent, PVMCPU pVCpu, bool fGlobalInit)
{
    NOREF(fGlobalInit);

    switch (enmEvent)
    {
        case RTTHREADCTXEVENT_OUT:
        {
            Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
            Assert(VMMR0ThreadCtxHookIsEnabled(pVCpu));
            VMCPU_ASSERT_EMT(pVCpu);

            /* No longjmps (log-flush, locks) in this fragile context. */
            VMMRZCallRing3Disable(pVCpu);

            if (!pVCpu->hm.s.fLeaveDone)
            {
                hmR0SvmLeave(pVCpu, false /* fImportState */);
                pVCpu->hm.s.fLeaveDone = true;
            }

            /* Leave HM context, takes care of local init (term). */
            int rc = HMR0LeaveCpu(pVCpu);
            AssertRC(rc); NOREF(rc);

            /* Restore longjmp state. */
            VMMRZCallRing3Enable(pVCpu);
            STAM_REL_COUNTER_INC(&pVCpu->hm.s.StatSwitchPreempt);
            break;
        }

        case RTTHREADCTXEVENT_IN:
        {
            Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
            Assert(VMMR0ThreadCtxHookIsEnabled(pVCpu));
            VMCPU_ASSERT_EMT(pVCpu);

            /* No longjmps (log-flush, locks) in this fragile context. */
            VMMRZCallRing3Disable(pVCpu);

            /*
             * Initialize the bare minimum state required for HM. This takes care of
             * initializing AMD-V if necessary (onlined CPUs, local init etc.)
             */
            int rc = hmR0EnterCpu(pVCpu);
            AssertRC(rc); NOREF(rc);
            Assert((pVCpu->hm.s.fCtxChanged & (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE))
                                           == (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE));

            pVCpu->hm.s.fLeaveDone = false;

            /* Restore longjmp state. */
            VMMRZCallRing3Enable(pVCpu);
            break;
        }

        default:
            break;
    }
}


/**
 * Saves the host state.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(int) SVMR0ExportHostState(PVMCPU pVCpu)
{
    NOREF(pVCpu);

    /* Nothing to do here. AMD-V does this for us automatically during the world-switch. */
    ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_HOST_CONTEXT);
    return VINF_SUCCESS;
}


/**
 * Exports the guest state from the guest-CPU context into the VMCB.
 *
 * The CPU state will be loaded from these fields on every successful VM-entry.
 * Also sets up the appropriate VMRUN function to execute guest code based on
 * the guest CPU mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmExportGuestState(PVMCPU pVCpu)
{
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExportGuestState, x);

    PSVMVMCB  pVmcb = pVCpu->hm.s.svm.pVmcb;
    PCCPUMCTX pCtx  = &pVCpu->cpum.GstCtx;

    Assert(pVmcb);
    HMSVM_ASSERT_NOT_IN_NESTED_GUEST(pCtx);

    pVmcb->guest.u64RIP    = pCtx->rip;
    pVmcb->guest.u64RSP    = pCtx->rsp;
    pVmcb->guest.u64RFlags = pCtx->eflags.u32;
    pVmcb->guest.u64RAX    = pCtx->rax;
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    if (pVmcb->ctrl.IntCtrl.n.u1VGifEnable)
    {
        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_VGIF);
        pVmcb->ctrl.IntCtrl.n.u1VGif = pCtx->hwvirt.fGif;
    }
#endif

    RTCCUINTREG const fEFlags = ASMIntDisableFlags();

    int rc = hmR0SvmExportGuestControlRegs(pVCpu, pVmcb);
    AssertRCReturnStmt(rc, ASMSetFlags(fEFlags), rc);

    hmR0SvmExportGuestSegmentRegs(pVCpu, pVmcb);
    hmR0SvmExportGuestMsrs(pVCpu, pVmcb);
    hmR0SvmExportGuestXcptIntercepts(pVCpu, pVmcb);

    ASMSetFlags(fEFlags);

    /* hmR0SvmExportGuestApicTpr() must be called -after- hmR0SvmExportGuestMsrs() as we
       otherwise we would overwrite the LSTAR MSR that we use for TPR patching. */
    hmR0SvmExportGuestApicTpr(pVCpu, pVmcb);

    rc = hmR0SvmSelectVMRunHandler(pVCpu);
    AssertRCReturn(rc, rc);

    /* Clear any bits that may be set but exported unconditionally or unused/reserved bits. */
    ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~(   HM_CHANGED_GUEST_RIP
                                                  |  HM_CHANGED_GUEST_RFLAGS
                                                  |  HM_CHANGED_GUEST_GPRS_MASK
                                                  |  HM_CHANGED_GUEST_X87
                                                  |  HM_CHANGED_GUEST_SSE_AVX
                                                  |  HM_CHANGED_GUEST_OTHER_XSAVE
                                                  |  HM_CHANGED_GUEST_XCRx
                                                  |  HM_CHANGED_GUEST_TSC_AUX
                                                  |  HM_CHANGED_GUEST_OTHER_MSRS
                                                  |  HM_CHANGED_GUEST_HWVIRT
                                                  | (HM_CHANGED_KEEPER_STATE_MASK & ~HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS)));

#ifdef VBOX_STRICT
    /*
     * All of the guest-CPU state and SVM keeper bits should be exported here by now,
     * except for the host-context and/or shared host-guest context bits.
     */
    uint64_t const fCtxChanged = ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    AssertMsg(!(fCtxChanged & (HM_CHANGED_ALL_GUEST & ~HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE)),
              ("fCtxChanged=%#RX64\n", fCtxChanged));

    /*
     * If we need to log state that isn't always imported, we'll need to import them here.
     * See hmR0SvmPostRunGuest() for which part of the state is imported uncondtionally.
     */
    hmR0SvmLogState(pVCpu, pVmcb, "hmR0SvmExportGuestState", 0 /* fFlags */, 0 /* uVerbose */);
#endif

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExportGuestState, x);
    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Merges the guest and nested-guest MSR permission bitmap.
 *
 * If the guest is intercepting an MSR we need to intercept it regardless of
 * whether the nested-guest is intercepting it or not.
 *
 * @param   pHostCpu        Pointer to the physical CPU HM info. struct.
 * @param   pVCpu           The cross context virtual CPU structure.
 *
 * @remarks No-long-jmp zone!!!
 */
DECLINLINE(void) hmR0SvmMergeMsrpmNested(PHMGLOBALCPUINFO pHostCpu, PVMCPU pVCpu)
{
    uint64_t const *pu64GstMsrpm    = (uint64_t const *)pVCpu->hm.s.svm.pvMsrBitmap;
    uint64_t const *pu64NstGstMsrpm = (uint64_t const *)pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pvMsrBitmap);
    uint64_t       *pu64DstMsrpm    = (uint64_t *)pHostCpu->n.svm.pvNstGstMsrpm;

    /* MSRPM bytes from offset 0x1800 are reserved, so we stop merging there. */
    uint32_t const offRsvdQwords = 0x1800 >> 3;
    for (uint32_t i = 0; i < offRsvdQwords; i++)
        pu64DstMsrpm[i] = pu64NstGstMsrpm[i] | pu64GstMsrpm[i];
}


/**
 * Caches the nested-guest VMCB fields before we modify them for execution using
 * hardware-assisted SVM.
 *
 * @returns true if the VMCB was previously already cached, false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 *
 * @sa      HMSvmNstGstVmExitNotify.
 */
static bool hmR0SvmCacheVmcbNested(PVMCPU pVCpu)
{
    /*
     * Cache the nested-guest programmed VMCB fields if we have not cached it yet.
     * Otherwise we risk re-caching the values we may have modified, see @bugref{7243#c44}.
     *
     * Nested-paging CR3 is not saved back into the VMCB on #VMEXIT, hence no need to
     * cache and restore it, see AMD spec. 15.25.4 "Nested Paging and VMRUN/#VMEXIT".
     */
    PSVMNESTEDVMCBCACHE pVmcbNstGstCache = &pVCpu->hm.s.svm.NstGstVmcbCache;
    bool const fWasCached = pVmcbNstGstCache->fCacheValid;
    if (!fWasCached)
    {
        PCSVMVMCB      pVmcbNstGst    = pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pVmcb);
        PCSVMVMCBCTRL pVmcbNstGstCtrl = &pVmcbNstGst->ctrl;
        pVmcbNstGstCache->u16InterceptRdCRx       = pVmcbNstGstCtrl->u16InterceptRdCRx;
        pVmcbNstGstCache->u16InterceptWrCRx       = pVmcbNstGstCtrl->u16InterceptWrCRx;
        pVmcbNstGstCache->u16InterceptRdDRx       = pVmcbNstGstCtrl->u16InterceptRdDRx;
        pVmcbNstGstCache->u16InterceptWrDRx       = pVmcbNstGstCtrl->u16InterceptWrDRx;
        pVmcbNstGstCache->u16PauseFilterThreshold = pVmcbNstGstCtrl->u16PauseFilterThreshold;
        pVmcbNstGstCache->u16PauseFilterCount     = pVmcbNstGstCtrl->u16PauseFilterCount;
        pVmcbNstGstCache->u32InterceptXcpt        = pVmcbNstGstCtrl->u32InterceptXcpt;
        pVmcbNstGstCache->u64InterceptCtrl        = pVmcbNstGstCtrl->u64InterceptCtrl;
        pVmcbNstGstCache->u64TSCOffset            = pVmcbNstGstCtrl->u64TSCOffset;
        pVmcbNstGstCache->fVIntrMasking           = pVmcbNstGstCtrl->IntCtrl.n.u1VIntrMasking;
        pVmcbNstGstCache->fNestedPaging           = pVmcbNstGstCtrl->NestedPagingCtrl.n.u1NestedPaging;
        pVmcbNstGstCache->fLbrVirt                = pVmcbNstGstCtrl->LbrVirt.n.u1LbrVirt;
        pVmcbNstGstCache->fCacheValid             = true;
        Log4Func(("Cached VMCB fields\n"));
    }

    return fWasCached;
}


/**
 * Sets up the nested-guest VMCB for execution using hardware-assisted SVM.
 *
 * This is done the first time we enter nested-guest execution using SVM R0
 * until the nested-guest \#VMEXIT (not to be confused with physical CPU
 * \#VMEXITs which may or may not cause a corresponding nested-guest \#VMEXIT).
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void hmR0SvmSetupVmcbNested(PVMCPU pVCpu)
{
    PSVMVMCB     pVmcbNstGst     = pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pVmcb);
    PSVMVMCBCTRL pVmcbNstGstCtrl = &pVmcbNstGst->ctrl;

    /*
     * First cache the nested-guest VMCB fields we may potentially modify.
     */
    bool const fVmcbCached = hmR0SvmCacheVmcbNested(pVCpu);
    if (!fVmcbCached)
    {
        /*
         * The IOPM of the nested-guest can be ignored because the the guest always
         * intercepts all IO port accesses. Thus, we'll swap to the guest IOPM rather
         * than the nested-guest IOPM and swap the field back on the #VMEXIT.
         */
        pVmcbNstGstCtrl->u64IOPMPhysAddr = g_HCPhysIOBitmap;

        /*
         * Use the same nested-paging as the outer guest. We can't dynamically switch off
         * nested-paging suddenly while executing a VM (see assertion at the end of
         * Trap0eHandler() in PGMAllBth.h).
         */
        pVmcbNstGstCtrl->NestedPagingCtrl.n.u1NestedPaging = pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging;

        /* Always enable V_INTR_MASKING as we do not want to allow access to the physical APIC TPR. */
        pVmcbNstGstCtrl->IntCtrl.n.u1VIntrMasking = 1;

        /*
         * Turn off TPR syncing on #VMEXIT for nested-guests as CR8 intercepts are subject
         * to the nested-guest intercepts and we always run with V_INTR_MASKING.
         */
        pVCpu->hm.s.svm.fSyncVTpr = false;

#ifdef DEBUG_ramshankar
        /* For debugging purposes - copy the LBR info. from outer guest VMCB. */
        pVmcbNstGstCtrl->LbrVirt.n.u1LbrVirt = pVmcb->ctrl.LbrVirt.n.u1LbrVirt;
#endif

        /*
         * If we don't expose Virtualized-VMSAVE/VMLOAD feature to the outer guest, we
         * need to intercept VMSAVE/VMLOAD instructions executed by the nested-guest.
         */
        if (!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fSvmVirtVmsaveVmload)
            pVmcbNstGstCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_VMSAVE
                                               |  SVM_CTRL_INTERCEPT_VMLOAD;

        /*
         * If we don't expose Virtual GIF feature to the outer guest, we need to intercept
         * CLGI/STGI instructions executed by the nested-guest.
         */
        if (!pVCpu->CTX_SUFF(pVM)->cpum.ro.GuestFeatures.fSvmVGif)
            pVmcbNstGstCtrl->u64InterceptCtrl |= SVM_CTRL_INTERCEPT_CLGI
                                               |  SVM_CTRL_INTERCEPT_STGI;

        /* Merge the guest and nested-guest intercepts. */
        hmR0SvmMergeVmcbCtrlsNested(pVCpu);

        /* Update the VMCB clean bits. */
        pVmcbNstGstCtrl->u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
    }
    else
    {
        Assert(!pVCpu->hm.s.svm.fSyncVTpr);
        Assert(pVmcbNstGstCtrl->u64IOPMPhysAddr == g_HCPhysIOBitmap);
        Assert(RT_BOOL(pVmcbNstGstCtrl->NestedPagingCtrl.n.u1NestedPaging) == pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);
    }
}


/**
 * Exports the nested-guest state into the VMCB.
 *
 * We need to export the entire state as we could be continuing nested-guest
 * execution at any point (not just immediately after VMRUN) and thus the VMCB
 * can be out-of-sync with the nested-guest state if it was executed in IEM.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmExportGuestStateNested(PVMCPU pVCpu)
{
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExportGuestState, x);

    PCCPUMCTX   pCtx        = &pVCpu->cpum.GstCtx;
    PSVMVMCB    pVmcbNstGst = pCtx->hwvirt.svm.CTX_SUFF(pVmcb);
    Assert(pVmcbNstGst);

    hmR0SvmSetupVmcbNested(pVCpu);

    pVmcbNstGst->guest.u64RIP    = pCtx->rip;
    pVmcbNstGst->guest.u64RSP    = pCtx->rsp;
    pVmcbNstGst->guest.u64RFlags = pCtx->eflags.u32;
    pVmcbNstGst->guest.u64RAX    = pCtx->rax;

    RTCCUINTREG const fEFlags = ASMIntDisableFlags();

    int rc = hmR0SvmExportGuestControlRegs(pVCpu, pVmcbNstGst);
    AssertRCReturnStmt(rc, ASMSetFlags(fEFlags), rc);

    hmR0SvmExportGuestSegmentRegs(pVCpu, pVmcbNstGst);
    hmR0SvmExportGuestMsrs(pVCpu, pVmcbNstGst);
    hmR0SvmExportGuestHwvirtStateNested(pVCpu, pVmcbNstGst);

    ASMSetFlags(fEFlags);

    /* Nested VGIF not supported yet. */
    Assert(!pVmcbNstGst->ctrl.IntCtrl.n.u1VGifEnable);

    rc = hmR0SvmSelectVMRunHandler(pVCpu);
    AssertRCReturn(rc, rc);

    /* Clear any bits that may be set but exported unconditionally or unused/reserved bits. */
    ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~(   HM_CHANGED_GUEST_RIP
                                                  |  HM_CHANGED_GUEST_RFLAGS
                                                  |  HM_CHANGED_GUEST_GPRS_MASK
                                                  |  HM_CHANGED_GUEST_APIC_TPR
                                                  |  HM_CHANGED_GUEST_X87
                                                  |  HM_CHANGED_GUEST_SSE_AVX
                                                  |  HM_CHANGED_GUEST_OTHER_XSAVE
                                                  |  HM_CHANGED_GUEST_XCRx
                                                  |  HM_CHANGED_GUEST_TSC_AUX
                                                  |  HM_CHANGED_GUEST_OTHER_MSRS
                                                  |  HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS
                                                  | (HM_CHANGED_KEEPER_STATE_MASK & ~HM_CHANGED_SVM_MASK)));

#ifdef VBOX_STRICT
    /*
     * All of the guest-CPU state and SVM keeper bits should be exported here by now, except
     * for the host-context and/or shared host-guest context bits.
     */
    uint64_t const fCtxChanged = ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    AssertMsg(!(fCtxChanged & (HM_CHANGED_ALL_GUEST & ~HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE)),
              ("fCtxChanged=%#RX64\n", fCtxChanged));

    /*
     * If we need to log state that isn't always imported, we'll need to import them here.
     * See hmR0SvmPostRunGuest() for which part of the state is imported uncondtionally.
     */
    hmR0SvmLogState(pVCpu, pVmcbNstGst, "hmR0SvmExportGuestStateNested", 0 /* fFlags */, 0 /* uVerbose */);
#endif

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExportGuestState, x);
    return rc;
}
#endif /* VBOX_WITH_NESTED_HWVIRT_SVM */


/**
 * Exports the state shared between the host and guest (or nested-guest) into
 * the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmExportSharedState(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_DR_MASK)
    {
        /** @todo Figure out stepping with nested-guest. */
        PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        if (!CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
            hmR0SvmExportSharedDebugState(pVCpu, pVmcb);
        else
        {
            pVmcb->guest.u64DR6 = pCtx->dr[6];
            pVmcb->guest.u64DR7 = pCtx->dr[7];
        }
    }

    pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_GUEST_DR_MASK;
    AssertMsg(!(pVCpu->hm.s.fCtxChanged & HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE),
              ("fCtxChanged=%#RX64\n", pVCpu->hm.s.fCtxChanged));
}


/**
 * Worker for SVMR0ImportStateOnDemand.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   fWhat   What to import, CPUMCTX_EXTRN_XXX.
 */
static void hmR0SvmImportGuestState(PVMCPU pVCpu, uint64_t fWhat)
{
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatImportGuestState, x);

    PCPUMCTX           pCtx       = &pVCpu->cpum.GstCtx;
    PCSVMVMCB          pVmcb      = hmR0SvmGetCurrentVmcb(pVCpu);
    PCSVMVMCBSTATESAVE pVmcbGuest = &pVmcb->guest;
    PCSVMVMCBCTRL      pVmcbCtrl  = &pVmcb->ctrl;

    Log4Func(("fExtrn=%#RX64 fWhat=%#RX64\n", pCtx->fExtrn, fWhat));

    /*
     * We disable interrupts to make the updating of the state and in particular
     * the fExtrn modification atomic wrt to preemption hooks.
     */
    RTCCUINTREG const fEFlags = ASMIntDisableFlags();

    fWhat &= pCtx->fExtrn;
    if (fWhat)
    {
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        if (fWhat & CPUMCTX_EXTRN_HWVIRT)
        {
            if (   !CPUMIsGuestInSvmNestedHwVirtMode(pCtx)
                && pVmcbCtrl->IntCtrl.n.u1VGifEnable)
            {
                /* We don't yet support passing VGIF feature to the guest. */
                Assert(pVCpu->CTX_SUFF(pVM)->hm.s.svm.fVGif);
                pCtx->hwvirt.fGif = pVmcbCtrl->IntCtrl.n.u1VGif;
            }
        }

        if (fWhat & CPUMCTX_EXTRN_HM_SVM_HWVIRT_VIRQ)
        {
            if (  !pVmcbCtrl->IntCtrl.n.u1VIrqPending
                && VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST))
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST);
        }
#endif

        if (fWhat & CPUMCTX_EXTRN_HM_SVM_INT_SHADOW)
        {
            if (pVmcbCtrl->IntShadow.n.u1IntShadow)
                EMSetInhibitInterruptsPC(pVCpu, pVmcbGuest->u64RIP);
            else if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
        }

        if (fWhat & CPUMCTX_EXTRN_RIP)
            pCtx->rip = pVmcbGuest->u64RIP;

        if (fWhat & CPUMCTX_EXTRN_RFLAGS)
            pCtx->eflags.u32 = pVmcbGuest->u64RFlags;

        if (fWhat & CPUMCTX_EXTRN_RSP)
            pCtx->rsp = pVmcbGuest->u64RSP;

        if (fWhat & CPUMCTX_EXTRN_RAX)
            pCtx->rax = pVmcbGuest->u64RAX;

        if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
        {
            if (fWhat & CPUMCTX_EXTRN_CS)
            {
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, CS, cs);
                /* Correct the CS granularity bit. Haven't seen it being wrong in any other register (yet). */
                /** @todo SELM might need to be fixed as it too should not care about the
                 *        granularity bit. See @bugref{6785}. */
                if (   !pCtx->cs.Attr.n.u1Granularity
                    &&  pCtx->cs.Attr.n.u1Present
                    &&  pCtx->cs.u32Limit > UINT32_C(0xfffff))
                {
                    Assert((pCtx->cs.u32Limit & 0xfff) == 0xfff);
                    pCtx->cs.Attr.n.u1Granularity = 1;
                }
                HMSVM_ASSERT_SEG_GRANULARITY(pCtx, cs);
            }
            if (fWhat & CPUMCTX_EXTRN_SS)
            {
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, SS, ss);
                HMSVM_ASSERT_SEG_GRANULARITY(pCtx, ss);
                /*
                 * Sync the hidden SS DPL field. AMD CPUs have a separate CPL field in the
                 * VMCB and uses that and thus it's possible that when the CPL changes during
                 * guest execution that the SS DPL isn't updated by AMD-V. Observed on some
                 * AMD Fusion CPUs with 64-bit guests.
                 *
                 * See AMD spec. 15.5.1 "Basic operation".
                 */
                Assert(!(pVmcbGuest->u8CPL & ~0x3));
                uint8_t const uCpl = pVmcbGuest->u8CPL;
                if (pCtx->ss.Attr.n.u2Dpl != uCpl)
                    pCtx->ss.Attr.n.u2Dpl = uCpl & 0x3;
            }
            if (fWhat & CPUMCTX_EXTRN_DS)
            {
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, DS, ds);
                HMSVM_ASSERT_SEG_GRANULARITY(pCtx, ds);
            }
            if (fWhat & CPUMCTX_EXTRN_ES)
            {
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, ES, es);
                HMSVM_ASSERT_SEG_GRANULARITY(pCtx, es);
            }
            if (fWhat & CPUMCTX_EXTRN_FS)
            {
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, FS, fs);
                HMSVM_ASSERT_SEG_GRANULARITY(pCtx, fs);
            }
            if (fWhat & CPUMCTX_EXTRN_GS)
            {
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, GS, gs);
                HMSVM_ASSERT_SEG_GRANULARITY(pCtx, gs);
            }
        }

        if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
        {
            if (fWhat & CPUMCTX_EXTRN_TR)
            {
                /*
                 * Fixup TR attributes so it's compatible with Intel. Important when saved-states
                 * are used between Intel and AMD, see @bugref{6208#c39}.
                 * ASSUME that it's normally correct and that we're in 32-bit or 64-bit mode.
                 */
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, TR, tr);
                if (pCtx->tr.Attr.n.u4Type != X86_SEL_TYPE_SYS_386_TSS_BUSY)
                {
                    if (   pCtx->tr.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_AVAIL
                        || CPUMIsGuestInLongModeEx(pCtx))
                        pCtx->tr.Attr.n.u4Type = X86_SEL_TYPE_SYS_386_TSS_BUSY;
                    else if (pCtx->tr.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_AVAIL)
                        pCtx->tr.Attr.n.u4Type = X86_SEL_TYPE_SYS_286_TSS_BUSY;
                }
            }

            if (fWhat & CPUMCTX_EXTRN_LDTR)
                HMSVM_SEG_REG_COPY_FROM_VMCB(pCtx, pVmcbGuest, LDTR, ldtr);

            if (fWhat & CPUMCTX_EXTRN_GDTR)
            {
                pCtx->gdtr.cbGdt = pVmcbGuest->GDTR.u32Limit;
                pCtx->gdtr.pGdt  = pVmcbGuest->GDTR.u64Base;
            }

            if (fWhat & CPUMCTX_EXTRN_IDTR)
            {
                pCtx->idtr.cbIdt = pVmcbGuest->IDTR.u32Limit;
                pCtx->idtr.pIdt  = pVmcbGuest->IDTR.u64Base;
            }
        }

        if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
        {
            pCtx->msrSTAR   = pVmcbGuest->u64STAR;
            pCtx->msrLSTAR  = pVmcbGuest->u64LSTAR;
            pCtx->msrCSTAR  = pVmcbGuest->u64CSTAR;
            pCtx->msrSFMASK = pVmcbGuest->u64SFMASK;
        }

        if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
        {
            pCtx->SysEnter.cs  = pVmcbGuest->u64SysEnterCS;
            pCtx->SysEnter.eip = pVmcbGuest->u64SysEnterEIP;
            pCtx->SysEnter.esp = pVmcbGuest->u64SysEnterESP;
        }

        if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
            pCtx->msrKERNELGSBASE = pVmcbGuest->u64KernelGSBase;

        if (fWhat & CPUMCTX_EXTRN_DR_MASK)
        {
            if (fWhat & CPUMCTX_EXTRN_DR6)
            {
                if (!pVCpu->hm.s.fUsingHyperDR7)
                    pCtx->dr[6] = pVmcbGuest->u64DR6;
                else
                    CPUMSetHyperDR6(pVCpu, pVmcbGuest->u64DR6);
            }

            if (fWhat & CPUMCTX_EXTRN_DR7)
            {
                if (!pVCpu->hm.s.fUsingHyperDR7)
                    pCtx->dr[7] = pVmcbGuest->u64DR7;
                else
                    Assert(pVmcbGuest->u64DR7 == CPUMGetHyperDR7(pVCpu));
            }
        }

        if (fWhat & CPUMCTX_EXTRN_CR_MASK)
        {
            if (fWhat & CPUMCTX_EXTRN_CR0)
            {
                /* We intercept changes to all CR0 bits except maybe TS & MP bits. */
                uint64_t const uCr0 = (pCtx->cr0          & ~(X86_CR0_TS | X86_CR0_MP))
                                    | (pVmcbGuest->u64CR0 &  (X86_CR0_TS | X86_CR0_MP));
                VMMRZCallRing3Disable(pVCpu); /* Calls into PGM which has Log statements. */
                CPUMSetGuestCR0(pVCpu, uCr0);
                VMMRZCallRing3Enable(pVCpu);
            }

            if (fWhat & CPUMCTX_EXTRN_CR2)
                pCtx->cr2 = pVmcbGuest->u64CR2;

            if (fWhat & CPUMCTX_EXTRN_CR3)
            {
                if (   pVmcbCtrl->NestedPagingCtrl.n.u1NestedPaging
                    && pCtx->cr3 != pVmcbGuest->u64CR3)
                {
                    CPUMSetGuestCR3(pVCpu, pVmcbGuest->u64CR3);
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3);
                }
            }

            /* Changes to CR4 are always intercepted. */
        }

        /* Update fExtrn. */
        pCtx->fExtrn &= ~fWhat;

        /* If everything has been imported, clear the HM keeper bit. */
        if (!(pCtx->fExtrn & HMSVM_CPUMCTX_EXTRN_ALL))
        {
            pCtx->fExtrn &= ~CPUMCTX_EXTRN_KEEPER_HM;
            Assert(!pCtx->fExtrn);
        }
    }
    else
        Assert(!pCtx->fExtrn || (pCtx->fExtrn & HMSVM_CPUMCTX_EXTRN_ALL));

    ASMSetFlags(fEFlags);

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatImportGuestState, x);

    /*
     * Honor any pending CR3 updates.
     *
     * Consider this scenario: #VMEXIT -> VMMRZCallRing3Enable() -> do stuff that causes a longjmp
     * -> hmR0SvmCallRing3Callback() -> VMMRZCallRing3Disable() -> hmR0SvmImportGuestState()
     * -> Sets VMCPU_FF_HM_UPDATE_CR3 pending -> return from the longjmp -> continue with #VMEXIT
     * handling -> hmR0SvmImportGuestState() and here we are.
     *
     * The reason for such complicated handling is because VM-exits that call into PGM expect
     * CR3 to be up-to-date and thus any CR3-saves -before- the VM-exit (longjmp) would've
     * postponed the CR3 update via the force-flag and cleared CR3 from fExtrn. Any SVM R0
     * VM-exit handler that requests CR3 to be saved will end up here and we call PGMUpdateCR3().
     *
     * The longjmp exit path can't check these CR3 force-flags and call code that takes a lock again,
     * and does not process force-flag like regular exits to ring-3 either, we cover for it here.
     */
    if (   VMMRZCallRing3IsEnabled(pVCpu)
        && VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
    {
        Assert(pCtx->cr3 == pVmcbGuest->u64CR3);
        PGMUpdateCR3(pVCpu, pCtx->cr3);
    }
}


/**
 * Saves the guest (or nested-guest) state from the VMCB into the guest-CPU
 * context.
 *
 * Currently there is no residual state left in the CPU that is not updated in the
 * VMCB.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   fWhat   What to import, CPUMCTX_EXTRN_XXX.
 */
VMMR0DECL(int) SVMR0ImportStateOnDemand(PVMCPU pVCpu, uint64_t fWhat)
{
    hmR0SvmImportGuestState(pVCpu, fWhat);
    return VINF_SUCCESS;
}


/**
 * Does the necessary state syncing before returning to ring-3 for any reason
 * (longjmp, preemption, voluntary exits to ring-3) from AMD-V.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fImportState    Whether to import the guest state from the VMCB back
 *                          to the guest-CPU context.
 *
 * @remarks No-long-jmp zone!!!
 */
static void hmR0SvmLeave(PVMCPU pVCpu, bool fImportState)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(VMMR0IsLogFlushDisabled(pVCpu));

    /*
     * !!! IMPORTANT !!!
     * If you modify code here, make sure to check whether hmR0SvmCallRing3Callback() needs to be updated too.
     */

    /* Save the guest state if necessary. */
    if (fImportState)
        hmR0SvmImportGuestState(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

    /* Restore host FPU state if necessary and resync on next R0 reentry. */
    CPUMR0FpuStateMaybeSaveGuestAndRestoreHost(pVCpu);
    Assert(!CPUMIsGuestFPUStateActive(pVCpu));

    /*
     * Restore host debug registers if necessary and resync on next R0 reentry.
     */
#ifdef VBOX_STRICT
    if (CPUMIsHyperDebugStateActive(pVCpu))
    {
        PSVMVMCB pVmcb = pVCpu->hm.s.svm.pVmcb; /** @todo nested-guest. */
        Assert(pVmcb->ctrl.u16InterceptRdDRx == 0xffff);
        Assert(pVmcb->ctrl.u16InterceptWrDRx == 0xffff);
    }
#endif
    CPUMR0DebugStateMaybeSaveGuestAndRestoreHost(pVCpu, false /* save DR6 */);
    Assert(!CPUMIsHyperDebugStateActive(pVCpu));
    Assert(!CPUMIsGuestDebugStateActive(pVCpu));

    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatEntry);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatImportGuestState);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExportGuestState);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatPreExit);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExitHandling);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchLongJmpToR3);

    VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_HM, VMCPUSTATE_STARTED_EXEC);
}


/**
 * Leaves the AMD-V session.
 *
 * Only used while returning to ring-3 either due to longjump or exits to
 * ring-3.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int hmR0SvmLeaveSession(PVMCPU pVCpu)
{
    HM_DISABLE_PREEMPT(pVCpu);
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* When thread-context hooks are used, we can avoid doing the leave again if we had been preempted before
       and done this from the SVMR0ThreadCtxCallback(). */
    if (!pVCpu->hm.s.fLeaveDone)
    {
        hmR0SvmLeave(pVCpu, true /* fImportState */);
        pVCpu->hm.s.fLeaveDone = true;
    }

    /*
     * !!! IMPORTANT !!!
     * If you modify code here, make sure to check whether hmR0SvmCallRing3Callback() needs to be updated too.
     */

    /** @todo eliminate the need for calling VMMR0ThreadCtxHookDisable here!  */
    /* Deregister hook now that we've left HM context before re-enabling preemption. */
    VMMR0ThreadCtxHookDisable(pVCpu);

    /* Leave HM context. This takes care of local init (term). */
    int rc = HMR0LeaveCpu(pVCpu);

    HM_RESTORE_PREEMPT();
    return rc;
}


/**
 * Does the necessary state syncing before doing a longjmp to ring-3.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jmp zone!!!
 */
static int hmR0SvmLongJmpToRing3(PVMCPU pVCpu)
{
    return hmR0SvmLeaveSession(pVCpu);
}


/**
 * VMMRZCallRing3() callback wrapper which saves the guest state (or restores
 * any remaining host state) before we longjump to ring-3 and possibly get
 * preempted.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   enmOperation    The operation causing the ring-3 longjump.
 * @param   pvUser          The user argument, NULL (currently unused).
 */
static DECLCALLBACK(int) hmR0SvmCallRing3Callback(PVMCPU pVCpu, VMMCALLRING3 enmOperation, void *pvUser)
{
    RT_NOREF_PV(pvUser);

    if (enmOperation == VMMCALLRING3_VM_R0_ASSERTION)
    {
        /*
         * !!! IMPORTANT !!!
         * If you modify code here, make sure to check whether hmR0SvmLeave() and hmR0SvmLeaveSession() needs
         * to be updated too. This is a stripped down version which gets out ASAP trying to not trigger any assertion.
         */
        VMMRZCallRing3RemoveNotification(pVCpu);
        VMMRZCallRing3Disable(pVCpu);
        HM_DISABLE_PREEMPT(pVCpu);

        /* Import the entire guest state. */
        hmR0SvmImportGuestState(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

        /* Restore host FPU state if necessary and resync on next R0 reentry. */
        CPUMR0FpuStateMaybeSaveGuestAndRestoreHost(pVCpu);

        /* Restore host debug registers if necessary and resync on next R0 reentry. */
        CPUMR0DebugStateMaybeSaveGuestAndRestoreHost(pVCpu, false /* save DR6 */);

        /* Deregister the hook now that we've left HM context before re-enabling preemption. */
        /** @todo eliminate the need for calling VMMR0ThreadCtxHookDisable here!  */
        VMMR0ThreadCtxHookDisable(pVCpu);

        /* Leave HM context. This takes care of local init (term). */
        HMR0LeaveCpu(pVCpu);

        HM_RESTORE_PREEMPT();
        return VINF_SUCCESS;
    }

    Assert(pVCpu);
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    HMSVM_ASSERT_PREEMPT_SAFE(pVCpu);

    VMMRZCallRing3Disable(pVCpu);
    Assert(VMMR0IsLogFlushDisabled(pVCpu));

    Log4Func(("Calling hmR0SvmLongJmpToRing3\n"));
    int rc = hmR0SvmLongJmpToRing3(pVCpu);
    AssertRCReturn(rc, rc);

    VMMRZCallRing3Enable(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Take necessary actions before going back to ring-3.
 *
 * An action requires us to go back to ring-3. This function does the necessary
 * steps before we can safely return to ring-3. This is not the same as longjmps
 * to ring-3, this is voluntary.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   rcExit      The reason for exiting to ring-3. Can be
 *                      VINF_VMM_UNKNOWN_RING3_CALL.
 */
static int hmR0SvmExitToRing3(PVMCPU pVCpu, int rcExit)
{
    Assert(pVCpu);
    HMSVM_ASSERT_PREEMPT_SAFE(pVCpu);

    /* Please, no longjumps here (any logging shouldn't flush jump back to ring-3). NO LOGGING BEFORE THIS POINT! */
    VMMRZCallRing3Disable(pVCpu);
    Log4Func(("rcExit=%d LocalFF=%#RX32 GlobalFF=%#RX32\n", rcExit, pVCpu->fLocalForcedActions,
              pVCpu->CTX_SUFF(pVM)->fGlobalForcedActions));

    /* We need to do this only while truly exiting the "inner loop" back to ring-3 and -not- for any longjmp to ring3. */
    if (pVCpu->hm.s.Event.fPending)
    {
        hmR0SvmPendingEventToTrpmTrap(pVCpu);
        Assert(!pVCpu->hm.s.Event.fPending);
    }

    /* Sync. the necessary state for going back to ring-3. */
    hmR0SvmLeaveSession(pVCpu);
    STAM_COUNTER_DEC(&pVCpu->hm.s.StatSwitchLongJmpToR3);

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);
    CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR
                             | CPUM_CHANGED_LDTR
                             | CPUM_CHANGED_GDTR
                             | CPUM_CHANGED_IDTR
                             | CPUM_CHANGED_TR
                             | CPUM_CHANGED_HIDDEN_SEL_REGS);
    if (   pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging
        && CPUMIsGuestPagingEnabledEx(&pVCpu->cpum.GstCtx))
    {
        CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_GLOBAL_TLB_FLUSH);
    }

    /* Update the exit-to-ring 3 reason. */
    pVCpu->hm.s.rcLastExitToR3 = rcExit;

    /* On our way back from ring-3, reload the guest-CPU state if it may change while in ring-3. */
    if (   rcExit != VINF_EM_RAW_INTERRUPT
        || CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
    {
        Assert(!(pVCpu->cpum.GstCtx.fExtrn & HMSVM_CPUMCTX_EXTRN_ALL));
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
    }

    STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchExitToR3);

    /* We do -not- want any longjmp notifications after this! We must return to ring-3 ASAP. */
    VMMRZCallRing3RemoveNotification(pVCpu);
    VMMRZCallRing3Enable(pVCpu);

    /*
     * If we're emulating an instruction, we shouldn't have any TRPM traps pending
     * and if we're injecting an event we should have a TRPM trap pending.
     */
    AssertReturnStmt(rcExit != VINF_EM_RAW_INJECT_TRPM_EVENT || TRPMHasTrap(pVCpu),
                     pVCpu->hm.s.u32HMError = rcExit,
                     VERR_SVM_IPE_5);
    AssertReturnStmt(rcExit != VINF_EM_RAW_EMULATE_INSTR || !TRPMHasTrap(pVCpu),
                     pVCpu->hm.s.u32HMError = rcExit,
                     VERR_SVM_IPE_4);

    return rcExit;
}


/**
 * Updates the use of TSC offsetting mode for the CPU and adjusts the necessary
 * intercepts.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmUpdateTscOffsetting(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    /*
     * Avoid intercepting RDTSC/RDTSCP if we determined the host TSC (++) is stable
     * and in case of a nested-guest, if the nested-VMCB specifies it is not intercepting
     * RDTSC/RDTSCP as well.
     */
    bool       fParavirtTsc;
    uint64_t   uTscOffset;
    bool const fCanUseRealTsc = TMCpuTickCanUseRealTSC(pVCpu->CTX_SUFF(pVM), pVCpu, &uTscOffset, &fParavirtTsc);

    bool fIntercept;
    if (fCanUseRealTsc)
         fIntercept = hmR0SvmClearCtrlIntercept(pVCpu, pVmcb, SVM_CTRL_INTERCEPT_RDTSC | SVM_CTRL_INTERCEPT_RDTSCP);
    else
    {
        hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_RDTSC | SVM_CTRL_INTERCEPT_RDTSCP);
        fIntercept = true;
    }

    if (!fIntercept)
    {
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        /* Apply the nested-guest VMCB's TSC offset over the guest TSC offset. */
        if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
            uTscOffset = HMSvmNstGstApplyTscOffset(pVCpu, uTscOffset);
#endif

        /* Update the TSC offset in the VMCB and the relevant clean bits. */
        pVmcb->ctrl.u64TSCOffset = uTscOffset;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;

        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscOffset);
    }
    else
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscIntercept);

    /* Currently neither Hyper-V nor KVM need to update their paravirt. TSC
       information before every VM-entry, hence we have nothing to do here at the moment. */
    if (fParavirtTsc)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscParavirt);
}


/**
 * Sets an event as a pending event to be injected into the guest.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   pEvent              Pointer to the SVM event.
 * @param   GCPtrFaultAddress   The fault-address (CR2) in case it's a
 *                              page-fault.
 *
 * @remarks Statistics counter assumes this is a guest event being reflected to
 *          the guest i.e. 'StatInjectPendingReflect' is incremented always.
 */
DECLINLINE(void) hmR0SvmSetPendingEvent(PVMCPU pVCpu, PSVMEVENT pEvent, RTGCUINTPTR GCPtrFaultAddress)
{
    Assert(!pVCpu->hm.s.Event.fPending);
    Assert(pEvent->n.u1Valid);

    pVCpu->hm.s.Event.u64IntInfo        = pEvent->u;
    pVCpu->hm.s.Event.fPending          = true;
    pVCpu->hm.s.Event.GCPtrFaultAddress = GCPtrFaultAddress;

    Log4Func(("u=%#RX64 u8Vector=%#x Type=%#x ErrorCodeValid=%RTbool ErrorCode=%#RX32\n", pEvent->u, pEvent->n.u8Vector,
              (uint8_t)pEvent->n.u3Type, !!pEvent->n.u1ErrorCodeValid, pEvent->n.u32ErrorCode));
}


/**
 * Sets an invalid-opcode (\#UD) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0SvmSetPendingXcptUD(PVMCPU pVCpu)
{
    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u3Type   = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector = X86_XCPT_UD;
    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
}


/**
 * Sets a debug (\#DB) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0SvmSetPendingXcptDB(PVMCPU pVCpu)
{
    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u3Type   = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector = X86_XCPT_DB;
    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
}


/**
 * Sets a page fault (\#PF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32ErrCode      The error-code for the page-fault.
 * @param   uFaultAddress   The page fault address (CR2).
 *
 * @remarks This updates the guest CR2 with @a uFaultAddress!
 */
DECLINLINE(void) hmR0SvmSetPendingXcptPF(PVMCPU pVCpu, uint32_t u32ErrCode, RTGCUINTPTR uFaultAddress)
{
    SVMEVENT Event;
    Event.u                  = 0;
    Event.n.u1Valid          = 1;
    Event.n.u3Type           = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector         = X86_XCPT_PF;
    Event.n.u1ErrorCodeValid = 1;
    Event.n.u32ErrorCode     = u32ErrCode;

    /* Update CR2 of the guest. */
    HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CR2);
    if (pVCpu->cpum.GstCtx.cr2 != uFaultAddress)
    {
        pVCpu->cpum.GstCtx.cr2 = uFaultAddress;
        /* The VMCB clean bit for CR2 will be updated while re-loading the guest state. */
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR2);
    }

    hmR0SvmSetPendingEvent(pVCpu, &Event, uFaultAddress);
}


/**
 * Sets a math-fault (\#MF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0SvmSetPendingXcptMF(PVMCPU pVCpu)
{
    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u3Type   = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector = X86_XCPT_MF;
    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
}


/**
 * Sets a double fault (\#DF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0SvmSetPendingXcptDF(PVMCPU pVCpu)
{
    SVMEVENT Event;
    Event.u                  = 0;
    Event.n.u1Valid          = 1;
    Event.n.u3Type           = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector         = X86_XCPT_DF;
    Event.n.u1ErrorCodeValid = 1;
    Event.n.u32ErrorCode     = 0;
    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
}


/**
 * Injects an event into the guest upon VMRUN by updating the relevant field
 * in the VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the guest VM control block.
 * @param   pEvent      Pointer to the event.
 *
 * @remarks No-long-jump zone!!!
 * @remarks Requires CR0!
 */
DECLINLINE(void) hmR0SvmInjectEventVmcb(PVMCPU pVCpu, PSVMVMCB pVmcb, PSVMEVENT pEvent)
{
    Assert(!pVmcb->ctrl.EventInject.n.u1Valid);
    pVmcb->ctrl.EventInject.u = pEvent->u;
    STAM_COUNTER_INC(&pVCpu->hm.s.paStatInjectedIrqsR0[pEvent->n.u8Vector & MASK_INJECT_IRQ_STAT]);
    RT_NOREF(pVCpu);

    Log4Func(("u=%#RX64 u8Vector=%#x Type=%#x ErrorCodeValid=%RTbool ErrorCode=%#RX32\n", pEvent->u, pEvent->n.u8Vector,
              (uint8_t)pEvent->n.u3Type, !!pEvent->n.u1ErrorCodeValid, pEvent->n.u32ErrorCode));
}



/**
 * Converts any TRPM trap into a pending HM event. This is typically used when
 * entering from ring-3 (not longjmp returns).
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void hmR0SvmTrpmTrapToPendingEvent(PVMCPU pVCpu)
{
    Assert(TRPMHasTrap(pVCpu));
    Assert(!pVCpu->hm.s.Event.fPending);

    uint8_t     uVector;
    TRPMEVENT   enmTrpmEvent;
    RTGCUINT    uErrCode;
    RTGCUINTPTR GCPtrFaultAddress;
    uint8_t     cbInstr;

    int rc = TRPMQueryTrapAll(pVCpu, &uVector, &enmTrpmEvent, &uErrCode, &GCPtrFaultAddress, &cbInstr);
    AssertRC(rc);

    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u8Vector = uVector;

    /* Refer AMD spec. 15.20 "Event Injection" for the format. */
    if (enmTrpmEvent == TRPM_TRAP)
    {
        Event.n.u3Type = SVM_EVENT_EXCEPTION;
        switch (uVector)
        {
            case X86_XCPT_NMI:
            {
                Event.n.u3Type = SVM_EVENT_NMI;
                break;
            }

            case X86_XCPT_PF:
            case X86_XCPT_DF:
            case X86_XCPT_TS:
            case X86_XCPT_NP:
            case X86_XCPT_SS:
            case X86_XCPT_GP:
            case X86_XCPT_AC:
            {
                Event.n.u1ErrorCodeValid = 1;
                Event.n.u32ErrorCode     = uErrCode;
                break;
            }
        }
    }
    else if (enmTrpmEvent == TRPM_HARDWARE_INT)
        Event.n.u3Type = SVM_EVENT_EXTERNAL_IRQ;
    else if (enmTrpmEvent == TRPM_SOFTWARE_INT)
        Event.n.u3Type = SVM_EVENT_SOFTWARE_INT;
    else
        AssertMsgFailed(("Invalid TRPM event type %d\n", enmTrpmEvent));

    rc = TRPMResetTrap(pVCpu);
    AssertRC(rc);

    Log4(("TRPM->HM event: u=%#RX64 u8Vector=%#x uErrorCodeValid=%RTbool uErrorCode=%#RX32\n", Event.u, Event.n.u8Vector,
          !!Event.n.u1ErrorCodeValid, Event.n.u32ErrorCode));

    hmR0SvmSetPendingEvent(pVCpu, &Event, GCPtrFaultAddress);
}


/**
 * Converts any pending SVM event into a TRPM trap. Typically used when leaving
 * AMD-V to execute any instruction.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void hmR0SvmPendingEventToTrpmTrap(PVMCPU pVCpu)
{
    Assert(pVCpu->hm.s.Event.fPending);
    Assert(TRPMQueryTrap(pVCpu, NULL /* pu8TrapNo */, NULL /* pEnmType */) == VERR_TRPM_NO_ACTIVE_TRAP);

    SVMEVENT Event;
    Event.u = pVCpu->hm.s.Event.u64IntInfo;

    uint8_t   uVector     = Event.n.u8Vector;
    uint8_t   uVectorType = Event.n.u3Type;
    TRPMEVENT enmTrapType = HMSvmEventToTrpmEventType(&Event);

    Log4(("HM event->TRPM: uVector=%#x enmTrapType=%d\n", uVector, uVectorType));

    int rc = TRPMAssertTrap(pVCpu, uVector, enmTrapType);
    AssertRC(rc);

    if (Event.n.u1ErrorCodeValid)
        TRPMSetErrorCode(pVCpu, Event.n.u32ErrorCode);

    if (   uVectorType == SVM_EVENT_EXCEPTION
        && uVector     == X86_XCPT_PF)
    {
        TRPMSetFaultAddress(pVCpu, pVCpu->hm.s.Event.GCPtrFaultAddress);
        Assert(pVCpu->hm.s.Event.GCPtrFaultAddress == CPUMGetGuestCR2(pVCpu));
    }
    else if (uVectorType == SVM_EVENT_SOFTWARE_INT)
    {
        AssertMsg(   uVectorType == SVM_EVENT_SOFTWARE_INT
                  || (uVector == X86_XCPT_BP || uVector == X86_XCPT_OF),
                  ("Invalid vector: uVector=%#x uVectorType=%#x\n", uVector, uVectorType));
        TRPMSetInstrLength(pVCpu, pVCpu->hm.s.Event.cbInstr);
    }
    pVCpu->hm.s.Event.fPending = false;
}


/**
 * Checks if the guest (or nested-guest) has an interrupt shadow active right
 * now.
 *
 * @returns @c true if the interrupt shadow is active, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 * @remarks Has side-effects with VMCPU_FF_INHIBIT_INTERRUPTS force-flag.
 */
static bool hmR0SvmIsIntrShadowActive(PVMCPU pVCpu)
{
    /*
     * Instructions like STI and MOV SS inhibit interrupts till the next instruction
     * completes. Check if we should inhibit interrupts or clear any existing
     * interrupt inhibition.
     */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    {
        if (pVCpu->cpum.GstCtx.rip != EMGetInhibitInterruptsPC(pVCpu))
        {
            /*
             * We can clear the inhibit force flag as even if we go back to the recompiler
             * without executing guest code in AMD-V, the flag's condition to be cleared is
             * met and thus the cleared state is correct.
             */
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
            return false;
        }
        return true;
    }
    return false;
}


/**
 * Sets the virtual interrupt intercept control in the VMCB.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   pVmcb   Pointer to the VM control block.
 */
static void hmR0SvmSetIntWindowExiting(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    /*
     * When AVIC isn't supported, set up an interrupt window to cause a #VMEXIT when the guest
     * is ready to accept interrupts. At #VMEXIT, we then get the interrupt from the APIC
     * (updating ISR at the right time) and inject the interrupt.
     *
     * With AVIC is supported, we could make use of the asynchronously delivery without
     * #VMEXIT and we would be passing the AVIC page to SVM.
     *
     * In AMD-V, an interrupt window is achieved using a combination of V_IRQ (an interrupt
     * is pending), V_IGN_TPR (ignore TPR priorities) and the VINTR intercept all being set.
     */
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    /*
     * Currently we don't overlay interupt windows and if there's any V_IRQ pending in the
     * nested-guest VMCB, we avoid setting up any interrupt window on behalf of the outer
     * guest.
     */
    /** @todo Does this mean we end up prioritizing virtual interrupt
     *        delivery/window over a physical interrupt (from the outer guest)
     *        might be pending? */
    bool const fEnableIntWindow = !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NESTED_GUEST);
    if (!fEnableIntWindow)
    {
        Assert(CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx));
        Log4(("Nested-guest V_IRQ already pending\n"));
    }
#else
    bool const fEnableIntWindow = true;
    RT_NOREF(pVCpu);
#endif
    if (fEnableIntWindow)
    {
        Assert(pVmcb->ctrl.IntCtrl.n.u1IgnoreTPR);
        pVmcb->ctrl.IntCtrl.n.u1VIrqPending = 1;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INT_CTRL;
        hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_VINTR);
        Log4(("Set VINTR intercept\n"));
    }
}


/**
 * Clears the virtual interrupt intercept control in the VMCB as
 * we are figured the guest is unable process any interrupts
 * at this point of time.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   pVmcb   Pointer to the VM control block.
 */
static void hmR0SvmClearIntWindowExiting(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    PSVMVMCBCTRL pVmcbCtrl = &pVmcb->ctrl;
    if (    pVmcbCtrl->IntCtrl.n.u1VIrqPending
        || (pVmcbCtrl->u64InterceptCtrl & SVM_CTRL_INTERCEPT_VINTR))
    {
        pVmcbCtrl->IntCtrl.n.u1VIrqPending = 0;
        pVmcbCtrl->u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INT_CTRL;
        hmR0SvmClearCtrlIntercept(pVCpu, pVmcb, SVM_CTRL_INTERCEPT_VINTR);
        Log4(("Cleared VINTR intercept\n"));
    }
}

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Evaluates the event to be delivered to the nested-guest and sets it as the
 * pending event.
 *
 * @returns VBox strict status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static VBOXSTRICTRC hmR0SvmEvaluatePendingEventNested(PVMCPU pVCpu)
{
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    HMSVM_ASSERT_IN_NESTED_GUEST(pCtx);
    HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_HWVIRT
                              | CPUMCTX_EXTRN_RFLAGS
                              | CPUMCTX_EXTRN_HM_SVM_INT_SHADOW
                              | CPUMCTX_EXTRN_HM_SVM_HWVIRT_VIRQ);

    Assert(!pVCpu->hm.s.Event.fPending);
    Assert(pCtx->hwvirt.fGif);
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    Assert(pVmcb);

    bool const fVirtualGif = CPUMGetSvmNstGstVGif(pCtx);
    bool const fIntShadow  = hmR0SvmIsIntrShadowActive(pVCpu);
    bool const fBlockNmi   = VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS);

    Log4Func(("fVirtualGif=%RTbool fBlockNmi=%RTbool fIntShadow=%RTbool fIntPending=%RTbool fNmiPending=%RTbool\n",
              fVirtualGif, fBlockNmi, fIntShadow, VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC),
              VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NMI)));

    /** @todo SMI. SMIs take priority over NMIs. */

    /*
     * Check if the guest can receive NMIs.
     * Nested NMIs are not allowed, see AMD spec. 8.1.4 "Masking External Interrupts".
     * NMIs take priority over maskable interrupts, see AMD spec. 8.5 "Priorities".
     */
    if (    VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NMI)
        && !fBlockNmi)
    {
        if (    fVirtualGif
            && !fIntShadow)
        {
            if (CPUMIsGuestSvmCtrlInterceptSet(pVCpu, pCtx, SVM_CTRL_INTERCEPT_NMI))
            {
                Log4(("Intercepting NMI -> #VMEXIT\n"));
                HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
                return IEMExecSvmVmexit(pVCpu, SVM_EXIT_NMI, 0, 0);
            }

            Log4(("Setting NMI pending for injection\n"));
            SVMEVENT Event;
            Event.u = 0;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = X86_XCPT_NMI;
            Event.n.u3Type   = SVM_EVENT_NMI;
            hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
        }
        else if (!fVirtualGif)
            hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_STGI);
        else
            hmR0SvmSetIntWindowExiting(pVCpu, pVmcb);
    }
    /*
     * Check if the nested-guest can receive external interrupts (generated by the guest's
     * PIC/APIC).
     *
     * External intercepts, NMI, SMI etc. from the physical CPU are -always- intercepted
     * when executing using hardware-assisted SVM, see HMSVM_MANDATORY_GUEST_CTRL_INTERCEPTS.
     *
     * External interrupts that are generated for the outer guest may be intercepted
     * depending on how the nested-guest VMCB was programmed by guest software.
     *
     * Physical interrupts always take priority over virtual interrupts,
     * see AMD spec. 15.21.4 "Injecting Virtual (INTR) Interrupts".
     */
    else if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC)
             && !pVCpu->hm.s.fSingleInstruction)
    {
        if (    fVirtualGif
            && !fIntShadow
            &&  CPUMCanSvmNstGstTakePhysIntr(pVCpu, pCtx))
        {
            if (CPUMIsGuestSvmCtrlInterceptSet(pVCpu, pCtx, SVM_CTRL_INTERCEPT_INTR))
            {
                Log4(("Intercepting INTR -> #VMEXIT\n"));
                HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
                return IEMExecSvmVmexit(pVCpu, SVM_EXIT_INTR, 0, 0);
            }

            uint8_t u8Interrupt;
            int rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
            if (RT_SUCCESS(rc))
            {
                Log4(("Setting external interrupt %#x pending for injection\n", u8Interrupt));
                SVMEVENT Event;
                Event.u = 0;
                Event.n.u1Valid  = 1;
                Event.n.u8Vector = u8Interrupt;
                Event.n.u3Type   = SVM_EVENT_EXTERNAL_IRQ;
                hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
            }
            else if (rc == VERR_APIC_INTR_MASKED_BY_TPR)
            {
                /*
                 * AMD-V has no TPR thresholding feature. TPR and the force-flag will be
                 * updated eventually when the TPR is written by the guest.
                 */
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchTprMaskedIrq);
            }
            else
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchGuestIrq);
        }
        else if (!fVirtualGif)
            hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_STGI);
        else
            hmR0SvmSetIntWindowExiting(pVCpu, pVmcb);
    }

    return VINF_SUCCESS;
}
#endif

/**
 * Evaluates the event to be delivered to the guest and sets it as the pending
 * event.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static void hmR0SvmEvaluatePendingEvent(PVMCPU pVCpu)
{
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    HMSVM_ASSERT_NOT_IN_NESTED_GUEST(pCtx);
    HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_HWVIRT
                              | CPUMCTX_EXTRN_RFLAGS
                              | CPUMCTX_EXTRN_HM_SVM_INT_SHADOW);

    Assert(!pVCpu->hm.s.Event.fPending);
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    Assert(pVmcb);

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    bool const fGif       = pCtx->hwvirt.fGif;
#else
    bool const fGif       = true;
#endif
    bool const fIntShadow = hmR0SvmIsIntrShadowActive(pVCpu);
    bool const fBlockInt  = !(pCtx->eflags.u32 & X86_EFL_IF);
    bool const fBlockNmi  = VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS);

    Log4Func(("fGif=%RTbool fBlockNmi=%RTbool fBlockInt=%RTbool fIntShadow=%RTbool fIntPending=%RTbool NMI pending=%RTbool\n",
              fGif, fBlockNmi, fBlockInt, fIntShadow,
              VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC),
              VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NMI)));

    /** @todo SMI. SMIs take priority over NMIs. */

    /*
     * Check if the guest can receive NMIs.
     * Nested NMIs are not allowed, see AMD spec. 8.1.4 "Masking External Interrupts".
     * NMIs take priority over maskable interrupts, see AMD spec. 8.5 "Priorities".
     */
    if (    VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NMI)
        && !fBlockNmi)
    {
        if (    fGif
            && !fIntShadow)
        {
            Log4(("Setting NMI pending for injection\n"));
            SVMEVENT Event;
            Event.u = 0;
            Event.n.u1Valid  = 1;
            Event.n.u8Vector = X86_XCPT_NMI;
            Event.n.u3Type   = SVM_EVENT_NMI;
            hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
        }
        else if (!fGif)
            hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_STGI);
        else
            hmR0SvmSetIntWindowExiting(pVCpu, pVmcb);
    }
    /*
     * Check if the guest can receive external interrupts (PIC/APIC). Once PDMGetInterrupt()
     * returns a valid interrupt we -must- deliver the interrupt. We can no longer re-request
     * it from the APIC device.
     */
    else if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC)
             && !pVCpu->hm.s.fSingleInstruction)
    {
        if (    fGif
            && !fBlockInt
            && !fIntShadow)
        {
            uint8_t u8Interrupt;
            int rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
            if (RT_SUCCESS(rc))
            {
                Log4(("Setting external interrupt %#x pending for injection\n", u8Interrupt));
                SVMEVENT Event;
                Event.u = 0;
                Event.n.u1Valid  = 1;
                Event.n.u8Vector = u8Interrupt;
                Event.n.u3Type   = SVM_EVENT_EXTERNAL_IRQ;
                hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
            }
            else if (rc == VERR_APIC_INTR_MASKED_BY_TPR)
            {
                /*
                 * AMD-V has no TPR thresholding feature. TPR and the force-flag will be
                 * updated eventually when the TPR is written by the guest.
                 */
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchTprMaskedIrq);
            }
            else
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchGuestIrq);
        }
        else if (!fGif)
            hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_STGI);
        else
            hmR0SvmSetIntWindowExiting(pVCpu, pVmcb);
    }
}


/**
 * Injects any pending events into the guest (or nested-guest).
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 *
 * @remarks Must only be called when we are guaranteed to enter
 *          hardware-assisted SVM execution and not return to ring-3
 *          prematurely.
 */
static void hmR0SvmInjectPendingEvent(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    Assert(!TRPMHasTrap(pVCpu));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    bool const fIntShadow = hmR0SvmIsIntrShadowActive(pVCpu);
#ifdef VBOX_STRICT
    PCCPUMCTX  pCtx       = &pVCpu->cpum.GstCtx;
    bool const fGif       = pCtx->hwvirt.fGif;
    bool       fAllowInt  = fGif;
    if (fGif)
    {
        /*
         * For nested-guests we have no way to determine if we're injecting a physical or
         * virtual interrupt at this point. Hence the partial verification below.
         */
        if (CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
            fAllowInt = CPUMCanSvmNstGstTakePhysIntr(pVCpu, pCtx) || CPUMCanSvmNstGstTakeVirtIntr(pVCpu, pCtx);
        else
            fAllowInt = RT_BOOL(pCtx->eflags.u32 & X86_EFL_IF);
    }
#endif

    if (pVCpu->hm.s.Event.fPending)
    {
        SVMEVENT Event;
        Event.u = pVCpu->hm.s.Event.u64IntInfo;
        Assert(Event.n.u1Valid);

        /*
         * Validate event injection pre-conditions.
         */
        if (Event.n.u3Type == SVM_EVENT_EXTERNAL_IRQ)
        {
            Assert(fAllowInt);
            Assert(!fIntShadow);
        }
        else if (Event.n.u3Type == SVM_EVENT_NMI)
        {
            Assert(fGif);
            Assert(!fIntShadow);
        }

        /*
         * Before injecting an NMI we must set VMCPU_FF_BLOCK_NMIS to prevent nested NMIs. We
         * do this only when we are surely going to inject the NMI as otherwise if we return
         * to ring-3 prematurely we could leave NMIs blocked indefinitely upon re-entry into
         * SVM R0.
         *
         * With VT-x, this is handled by the Guest interruptibility information VMCS field
         * which will set the VMCS field after actually delivering the NMI which we read on
         * VM-exit to determine the state.
         */
        if (    Event.n.u3Type   == SVM_EVENT_NMI
            &&  Event.n.u8Vector == X86_XCPT_NMI
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
        {
            VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }

        /*
         * Inject it (update VMCB for injection by the hardware).
         */
        Log4(("Injecting pending HM event\n"));
        hmR0SvmInjectEventVmcb(pVCpu, pVmcb, &Event);
        pVCpu->hm.s.Event.fPending = false;

        if (Event.n.u3Type == SVM_EVENT_EXTERNAL_IRQ)
            STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectInterrupt);
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectXcpt);
    }
    else
        Assert(pVmcb->ctrl.EventInject.n.u1Valid == 0);

    /*
     * We could have injected an NMI through IEM and continue guest execution using
     * hardware-assisted SVM. In which case, we would not have any events pending (above)
     * but we still need to intercept IRET in order to eventually clear NMI inhibition.
     */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
        hmR0SvmSetCtrlIntercept(pVmcb, SVM_CTRL_INTERCEPT_IRET);

    /*
     * Update the guest interrupt shadow in the guest (or nested-guest) VMCB.
     *
     * For nested-guests: We need to update it too for the scenario where IEM executes
     * the nested-guest but execution later continues here with an interrupt shadow active.
     */
    pVmcb->ctrl.IntShadow.n.u1IntShadow = fIntShadow;
}


/**
 * Reports world-switch error and dumps some useful debug info.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   rcVMRun         The return code from VMRUN (or
 *                          VERR_SVM_INVALID_GUEST_STATE for invalid
 *                          guest-state).
 */
static void hmR0SvmReportWorldSwitchError(PVMCPU pVCpu, int rcVMRun)
{
    HMSVM_ASSERT_PREEMPT_SAFE(pVCpu);
    HMSVM_ASSERT_NOT_IN_NESTED_GUEST(&pVCpu->cpum.GstCtx);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

    if (rcVMRun == VERR_SVM_INVALID_GUEST_STATE)
    {
#ifdef VBOX_STRICT
        hmR0DumpRegs(pVCpu);
        PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
        Log4(("ctrl.u32VmcbCleanBits                 %#RX32\n",   pVmcb->ctrl.u32VmcbCleanBits));
        Log4(("ctrl.u16InterceptRdCRx                %#x\n",      pVmcb->ctrl.u16InterceptRdCRx));
        Log4(("ctrl.u16InterceptWrCRx                %#x\n",      pVmcb->ctrl.u16InterceptWrCRx));
        Log4(("ctrl.u16InterceptRdDRx                %#x\n",      pVmcb->ctrl.u16InterceptRdDRx));
        Log4(("ctrl.u16InterceptWrDRx                %#x\n",      pVmcb->ctrl.u16InterceptWrDRx));
        Log4(("ctrl.u32InterceptXcpt                 %#x\n",      pVmcb->ctrl.u32InterceptXcpt));
        Log4(("ctrl.u64InterceptCtrl                 %#RX64\n",   pVmcb->ctrl.u64InterceptCtrl));
        Log4(("ctrl.u64IOPMPhysAddr                  %#RX64\n",   pVmcb->ctrl.u64IOPMPhysAddr));
        Log4(("ctrl.u64MSRPMPhysAddr                 %#RX64\n",   pVmcb->ctrl.u64MSRPMPhysAddr));
        Log4(("ctrl.u64TSCOffset                     %#RX64\n",   pVmcb->ctrl.u64TSCOffset));

        Log4(("ctrl.TLBCtrl.u32ASID                  %#x\n",      pVmcb->ctrl.TLBCtrl.n.u32ASID));
        Log4(("ctrl.TLBCtrl.u8TLBFlush               %#x\n",      pVmcb->ctrl.TLBCtrl.n.u8TLBFlush));
        Log4(("ctrl.TLBCtrl.u24Reserved              %#x\n",      pVmcb->ctrl.TLBCtrl.n.u24Reserved));

        Log4(("ctrl.IntCtrl.u8VTPR                   %#x\n",      pVmcb->ctrl.IntCtrl.n.u8VTPR));
        Log4(("ctrl.IntCtrl.u1VIrqPending            %#x\n",      pVmcb->ctrl.IntCtrl.n.u1VIrqPending));
        Log4(("ctrl.IntCtrl.u1VGif                   %#x\n",      pVmcb->ctrl.IntCtrl.n.u1VGif));
        Log4(("ctrl.IntCtrl.u6Reserved0              %#x\n",      pVmcb->ctrl.IntCtrl.n.u6Reserved));
        Log4(("ctrl.IntCtrl.u4VIntrPrio              %#x\n",      pVmcb->ctrl.IntCtrl.n.u4VIntrPrio));
        Log4(("ctrl.IntCtrl.u1IgnoreTPR              %#x\n",      pVmcb->ctrl.IntCtrl.n.u1IgnoreTPR));
        Log4(("ctrl.IntCtrl.u3Reserved               %#x\n",      pVmcb->ctrl.IntCtrl.n.u3Reserved));
        Log4(("ctrl.IntCtrl.u1VIntrMasking           %#x\n",      pVmcb->ctrl.IntCtrl.n.u1VIntrMasking));
        Log4(("ctrl.IntCtrl.u1VGifEnable             %#x\n",      pVmcb->ctrl.IntCtrl.n.u1VGifEnable));
        Log4(("ctrl.IntCtrl.u5Reserved1              %#x\n",      pVmcb->ctrl.IntCtrl.n.u5Reserved));
        Log4(("ctrl.IntCtrl.u8VIntrVector            %#x\n",      pVmcb->ctrl.IntCtrl.n.u8VIntrVector));
        Log4(("ctrl.IntCtrl.u24Reserved              %#x\n",      pVmcb->ctrl.IntCtrl.n.u24Reserved));

        Log4(("ctrl.IntShadow.u1IntShadow            %#x\n",      pVmcb->ctrl.IntShadow.n.u1IntShadow));
        Log4(("ctrl.IntShadow.u1GuestIntMask         %#x\n",      pVmcb->ctrl.IntShadow.n.u1GuestIntMask));
        Log4(("ctrl.u64ExitCode                      %#RX64\n",   pVmcb->ctrl.u64ExitCode));
        Log4(("ctrl.u64ExitInfo1                     %#RX64\n",   pVmcb->ctrl.u64ExitInfo1));
        Log4(("ctrl.u64ExitInfo2                     %#RX64\n",   pVmcb->ctrl.u64ExitInfo2));
        Log4(("ctrl.ExitIntInfo.u8Vector             %#x\n",      pVmcb->ctrl.ExitIntInfo.n.u8Vector));
        Log4(("ctrl.ExitIntInfo.u3Type               %#x\n",      pVmcb->ctrl.ExitIntInfo.n.u3Type));
        Log4(("ctrl.ExitIntInfo.u1ErrorCodeValid     %#x\n",      pVmcb->ctrl.ExitIntInfo.n.u1ErrorCodeValid));
        Log4(("ctrl.ExitIntInfo.u19Reserved          %#x\n",      pVmcb->ctrl.ExitIntInfo.n.u19Reserved));
        Log4(("ctrl.ExitIntInfo.u1Valid              %#x\n",      pVmcb->ctrl.ExitIntInfo.n.u1Valid));
        Log4(("ctrl.ExitIntInfo.u32ErrorCode         %#x\n",      pVmcb->ctrl.ExitIntInfo.n.u32ErrorCode));
        Log4(("ctrl.NestedPagingCtrl.u1NestedPaging  %#x\n",      pVmcb->ctrl.NestedPagingCtrl.n.u1NestedPaging));
        Log4(("ctrl.NestedPagingCtrl.u1Sev           %#x\n",      pVmcb->ctrl.NestedPagingCtrl.n.u1Sev));
        Log4(("ctrl.NestedPagingCtrl.u1SevEs         %#x\n",      pVmcb->ctrl.NestedPagingCtrl.n.u1SevEs));
        Log4(("ctrl.EventInject.u8Vector             %#x\n",      pVmcb->ctrl.EventInject.n.u8Vector));
        Log4(("ctrl.EventInject.u3Type               %#x\n",      pVmcb->ctrl.EventInject.n.u3Type));
        Log4(("ctrl.EventInject.u1ErrorCodeValid     %#x\n",      pVmcb->ctrl.EventInject.n.u1ErrorCodeValid));
        Log4(("ctrl.EventInject.u19Reserved          %#x\n",      pVmcb->ctrl.EventInject.n.u19Reserved));
        Log4(("ctrl.EventInject.u1Valid              %#x\n",      pVmcb->ctrl.EventInject.n.u1Valid));
        Log4(("ctrl.EventInject.u32ErrorCode         %#x\n",      pVmcb->ctrl.EventInject.n.u32ErrorCode));

        Log4(("ctrl.u64NestedPagingCR3               %#RX64\n",   pVmcb->ctrl.u64NestedPagingCR3));

        Log4(("ctrl.LbrVirt.u1LbrVirt                %#x\n",      pVmcb->ctrl.LbrVirt.n.u1LbrVirt));
        Log4(("ctrl.LbrVirt.u1VirtVmsaveVmload       %#x\n",      pVmcb->ctrl.LbrVirt.n.u1VirtVmsaveVmload));

        Log4(("guest.CS.u16Sel                       %RTsel\n",   pVmcb->guest.CS.u16Sel));
        Log4(("guest.CS.u16Attr                      %#x\n",      pVmcb->guest.CS.u16Attr));
        Log4(("guest.CS.u32Limit                     %#RX32\n",   pVmcb->guest.CS.u32Limit));
        Log4(("guest.CS.u64Base                      %#RX64\n",   pVmcb->guest.CS.u64Base));
        Log4(("guest.DS.u16Sel                       %#RTsel\n",  pVmcb->guest.DS.u16Sel));
        Log4(("guest.DS.u16Attr                      %#x\n",      pVmcb->guest.DS.u16Attr));
        Log4(("guest.DS.u32Limit                     %#RX32\n",   pVmcb->guest.DS.u32Limit));
        Log4(("guest.DS.u64Base                      %#RX64\n",   pVmcb->guest.DS.u64Base));
        Log4(("guest.ES.u16Sel                       %RTsel\n",   pVmcb->guest.ES.u16Sel));
        Log4(("guest.ES.u16Attr                      %#x\n",      pVmcb->guest.ES.u16Attr));
        Log4(("guest.ES.u32Limit                     %#RX32\n",   pVmcb->guest.ES.u32Limit));
        Log4(("guest.ES.u64Base                      %#RX64\n",   pVmcb->guest.ES.u64Base));
        Log4(("guest.FS.u16Sel                       %RTsel\n",   pVmcb->guest.FS.u16Sel));
        Log4(("guest.FS.u16Attr                      %#x\n",      pVmcb->guest.FS.u16Attr));
        Log4(("guest.FS.u32Limit                     %#RX32\n",   pVmcb->guest.FS.u32Limit));
        Log4(("guest.FS.u64Base                      %#RX64\n",   pVmcb->guest.FS.u64Base));
        Log4(("guest.GS.u16Sel                       %RTsel\n",   pVmcb->guest.GS.u16Sel));
        Log4(("guest.GS.u16Attr                      %#x\n",      pVmcb->guest.GS.u16Attr));
        Log4(("guest.GS.u32Limit                     %#RX32\n",   pVmcb->guest.GS.u32Limit));
        Log4(("guest.GS.u64Base                      %#RX64\n",   pVmcb->guest.GS.u64Base));

        Log4(("guest.GDTR.u32Limit                   %#RX32\n",   pVmcb->guest.GDTR.u32Limit));
        Log4(("guest.GDTR.u64Base                    %#RX64\n",   pVmcb->guest.GDTR.u64Base));

        Log4(("guest.LDTR.u16Sel                     %RTsel\n",   pVmcb->guest.LDTR.u16Sel));
        Log4(("guest.LDTR.u16Attr                    %#x\n",      pVmcb->guest.LDTR.u16Attr));
        Log4(("guest.LDTR.u32Limit                   %#RX32\n",   pVmcb->guest.LDTR.u32Limit));
        Log4(("guest.LDTR.u64Base                    %#RX64\n",   pVmcb->guest.LDTR.u64Base));

        Log4(("guest.IDTR.u32Limit                   %#RX32\n",   pVmcb->guest.IDTR.u32Limit));
        Log4(("guest.IDTR.u64Base                    %#RX64\n",   pVmcb->guest.IDTR.u64Base));

        Log4(("guest.TR.u16Sel                       %RTsel\n",   pVmcb->guest.TR.u16Sel));
        Log4(("guest.TR.u16Attr                      %#x\n",      pVmcb->guest.TR.u16Attr));
        Log4(("guest.TR.u32Limit                     %#RX32\n",   pVmcb->guest.TR.u32Limit));
        Log4(("guest.TR.u64Base                      %#RX64\n",   pVmcb->guest.TR.u64Base));

        Log4(("guest.u8CPL                           %#x\n",      pVmcb->guest.u8CPL));
        Log4(("guest.u64CR0                          %#RX64\n",   pVmcb->guest.u64CR0));
        Log4(("guest.u64CR2                          %#RX64\n",   pVmcb->guest.u64CR2));
        Log4(("guest.u64CR3                          %#RX64\n",   pVmcb->guest.u64CR3));
        Log4(("guest.u64CR4                          %#RX64\n",   pVmcb->guest.u64CR4));
        Log4(("guest.u64DR6                          %#RX64\n",   pVmcb->guest.u64DR6));
        Log4(("guest.u64DR7                          %#RX64\n",   pVmcb->guest.u64DR7));

        Log4(("guest.u64RIP                          %#RX64\n",   pVmcb->guest.u64RIP));
        Log4(("guest.u64RSP                          %#RX64\n",   pVmcb->guest.u64RSP));
        Log4(("guest.u64RAX                          %#RX64\n",   pVmcb->guest.u64RAX));
        Log4(("guest.u64RFlags                       %#RX64\n",   pVmcb->guest.u64RFlags));

        Log4(("guest.u64SysEnterCS                   %#RX64\n",   pVmcb->guest.u64SysEnterCS));
        Log4(("guest.u64SysEnterEIP                  %#RX64\n",   pVmcb->guest.u64SysEnterEIP));
        Log4(("guest.u64SysEnterESP                  %#RX64\n",   pVmcb->guest.u64SysEnterESP));

        Log4(("guest.u64EFER                         %#RX64\n",   pVmcb->guest.u64EFER));
        Log4(("guest.u64STAR                         %#RX64\n",   pVmcb->guest.u64STAR));
        Log4(("guest.u64LSTAR                        %#RX64\n",   pVmcb->guest.u64LSTAR));
        Log4(("guest.u64CSTAR                        %#RX64\n",   pVmcb->guest.u64CSTAR));
        Log4(("guest.u64SFMASK                       %#RX64\n",   pVmcb->guest.u64SFMASK));
        Log4(("guest.u64KernelGSBase                 %#RX64\n",   pVmcb->guest.u64KernelGSBase));
        Log4(("guest.u64PAT                          %#RX64\n",   pVmcb->guest.u64PAT));
        Log4(("guest.u64DBGCTL                       %#RX64\n",   pVmcb->guest.u64DBGCTL));
        Log4(("guest.u64BR_FROM                      %#RX64\n",   pVmcb->guest.u64BR_FROM));
        Log4(("guest.u64BR_TO                        %#RX64\n",   pVmcb->guest.u64BR_TO));
        Log4(("guest.u64LASTEXCPFROM                 %#RX64\n",   pVmcb->guest.u64LASTEXCPFROM));
        Log4(("guest.u64LASTEXCPTO                   %#RX64\n",   pVmcb->guest.u64LASTEXCPTO));

        NOREF(pVmcb);
#endif  /* VBOX_STRICT */
    }
    else
        Log4Func(("rcVMRun=%d\n", rcVMRun));
}


/**
 * Check per-VM and per-VCPU force flag actions that require us to go back to
 * ring-3 for one reason or another.
 *
 * @returns VBox status code (information status code included).
 * @retval VINF_SUCCESS if we don't have any actions that require going back to
 *         ring-3.
 * @retval VINF_PGM_SYNC_CR3 if we have pending PGM CR3 sync.
 * @retval VINF_EM_PENDING_REQUEST if we have pending requests (like hardware
 *         interrupts)
 * @retval VINF_PGM_POOL_FLUSH_PENDING if PGM is doing a pool flush and requires
 *         all EMTs to be in ring-3.
 * @retval VINF_EM_RAW_TO_R3 if there is pending DMA requests.
 * @retval VINF_EM_NO_MEMORY PGM is out of memory, we need to return
 *         to the EM loop.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int hmR0SvmCheckForceFlags(PVMCPU pVCpu)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));

    /* Could happen as a result of longjump. */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
        PGMUpdateCR3(pVCpu, CPUMGetGuestCR3(pVCpu));

    /* Update pending interrupts into the APIC's IRR. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_UPDATE_APIC))
        APICUpdatePendingInterrupts(pVCpu);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (   VM_FF_IS_PENDING(pVM, !pVCpu->hm.s.fSingleInstruction
                            ? VM_FF_HP_R0_PRE_HM_MASK : VM_FF_HP_R0_PRE_HM_STEP_MASK)
        || VMCPU_FF_IS_PENDING(pVCpu, !pVCpu->hm.s.fSingleInstruction
                               ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
    {
        /* Pending PGM C3 sync. */
        if (VMCPU_FF_IS_PENDING(pVCpu,VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
        {
            int rc = PGMSyncCR3(pVCpu, pVCpu->cpum.GstCtx.cr0, pVCpu->cpum.GstCtx.cr3, pVCpu->cpum.GstCtx.cr4,
                                VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
            if (rc != VINF_SUCCESS)
            {
                Log4Func(("PGMSyncCR3 forcing us back to ring-3. rc=%d\n", rc));
                return rc;
            }
        }

        /* Pending HM-to-R3 operations (critsects, timers, EMT rendezvous etc.) */
        /* -XXX- what was that about single stepping?  */
        if (   VM_FF_IS_PENDING(pVM, VM_FF_HM_TO_R3_MASK)
            || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHmToR3FF);
            int rc = RT_UNLIKELY(VM_FF_IS_PENDING(pVM, VM_FF_PGM_NO_MEMORY)) ? VINF_EM_NO_MEMORY : VINF_EM_RAW_TO_R3;
            Log4Func(("HM_TO_R3 forcing us back to ring-3. rc=%d\n", rc));
            return rc;
        }

        /* Pending VM request packets, such as hardware interrupts. */
        if (   VM_FF_IS_PENDING(pVM, VM_FF_REQUEST)
            || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_REQUEST))
        {
            Log4Func(("Pending VM request forcing us back to ring-3\n"));
            return VINF_EM_PENDING_REQUEST;
        }

        /* Pending PGM pool flushes. */
        if (VM_FF_IS_PENDING(pVM, VM_FF_PGM_POOL_FLUSH_PENDING))
        {
            Log4Func(("PGM pool flush pending forcing us back to ring-3\n"));
            return VINF_PGM_POOL_FLUSH_PENDING;
        }

        /* Pending DMA requests. */
        if (VM_FF_IS_PENDING(pVM, VM_FF_PDM_DMA))
        {
            Log4Func(("Pending DMA request forcing us back to ring-3\n"));
            return VINF_EM_RAW_TO_R3;
        }
    }

    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Does the preparations before executing nested-guest code in AMD-V.
 *
 * @returns VBox status code (informational status codes included).
 * @retval VINF_SUCCESS if we can proceed with running the guest.
 * @retval VINF_* scheduling changes, we have to go back to ring-3.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 *
 * @remarks Same caveats regarding longjumps as hmR0SvmPreRunGuest applies.
 * @sa      hmR0SvmPreRunGuest.
 */
static int hmR0SvmPreRunGuestNested(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    HMSVM_ASSERT_PREEMPT_SAFE(pVCpu);
    HMSVM_ASSERT_IN_NESTED_GUEST(pCtx);

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM_ONLY_IN_IEM
    Log2(("hmR0SvmPreRunGuest: Rescheduling to IEM due to nested-hwvirt or forced IEM exec -> VINF_EM_RESCHEDULE_REM\n"));
    return VINF_EM_RESCHEDULE_REM;
#endif

    /* Check force flag actions that might require us to go back to ring-3. */
    int rc = hmR0SvmCheckForceFlags(pVCpu);
    if (rc != VINF_SUCCESS)
        return rc;

    if (TRPMHasTrap(pVCpu))
        hmR0SvmTrpmTrapToPendingEvent(pVCpu);
    else if (!pVCpu->hm.s.Event.fPending)
    {
        VBOXSTRICTRC rcStrict = hmR0SvmEvaluatePendingEventNested(pVCpu);
        if (    rcStrict != VINF_SUCCESS
            || !CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
            return VBOXSTRICTRC_VAL(rcStrict);
    }

    HMSVM_ASSERT_IN_NESTED_GUEST(pCtx);

    /*
     * On the oldest AMD-V systems, we may not get enough information to reinject an NMI.
     * Just do it in software, see @bugref{8411}.
     * NB: If we could continue a task switch exit we wouldn't need to do this.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (RT_UNLIKELY(   !pVM->hm.s.svm.u32Features
                    &&  pVCpu->hm.s.Event.fPending
                    &&  SVM_EVENT_GET_TYPE(pVCpu->hm.s.Event.u64IntInfo) == SVM_EVENT_NMI))
    {
        return VINF_EM_RAW_INJECT_TRPM_EVENT;
    }

#ifdef HMSVM_SYNC_FULL_GUEST_STATE
    Assert(!(pCtx->fExtrn & HMSVM_CPUMCTX_EXTRN_ALL));
    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
#endif

    /*
     * Export the nested-guest state bits that are not shared with the host in any way as we
     * can longjmp or get preempted in the midst of exporting some of the state.
     */
    rc = hmR0SvmExportGuestStateNested(pVCpu);
    AssertRCReturn(rc, rc);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExportFull);

    /* Ensure we've cached (and hopefully modified) the VMCB for execution using hardware-assisted SVM. */
    Assert(pVCpu->hm.s.svm.NstGstVmcbCache.fCacheValid);

    /*
     * No longjmps to ring-3 from this point on!!!
     *
     * Asserts() will still longjmp to ring-3 (but won't return), which is intentional,
     * better than a kernel panic. This also disables flushing of the R0-logger instance.
     */
    VMMRZCallRing3Disable(pVCpu);

    /*
     * We disable interrupts so that we don't miss any interrupts that would flag preemption
     * (IPI/timers etc.) when thread-context hooks aren't used and we've been running with
     * preemption disabled for a while.  Since this is purly to aid the
     * RTThreadPreemptIsPending() code, it doesn't matter that it may temporarily reenable and
     * disable interrupt on NT.
     *
     * We need to check for force-flags that could've possible been altered since we last
     * checked them (e.g. by PDMGetInterrupt() leaving the PDM critical section,
     * see @bugref{6398}).
     *
     * We also check a couple of other force-flags as a last opportunity to get the EMT back
     * to ring-3 before executing guest code.
     */
    pSvmTransient->fEFlags = ASMIntDisableFlags();
    if (   VM_FF_IS_PENDING(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
        || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
    {
        ASMSetFlags(pSvmTransient->fEFlags);
        VMMRZCallRing3Enable(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHmToR3FF);
        return VINF_EM_RAW_TO_R3;
    }
    if (RTThreadPreemptIsPending(NIL_RTTHREAD))
    {
        ASMSetFlags(pSvmTransient->fEFlags);
        VMMRZCallRing3Enable(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatPendingHostIrq);
        return VINF_EM_RAW_INTERRUPT;
    }
    return VINF_SUCCESS;
}
#endif


/**
 * Does the preparations before executing guest code in AMD-V.
 *
 * This may cause longjmps to ring-3 and may even result in rescheduling to the
 * recompiler. We must be cautious what we do here regarding committing
 * guest-state information into the VMCB assuming we assuredly execute the guest
 * in AMD-V. If we fall back to the recompiler after updating the VMCB and
 * clearing the common-state (TRPM/forceflags), we must undo those changes so
 * that the recompiler can (and should) use them when it resumes guest
 * execution. Otherwise such operations must be done when we can no longer
 * exit to ring-3.
 *
 * @returns VBox status code (informational status codes included).
 * @retval VINF_SUCCESS if we can proceed with running the guest.
 * @retval VINF_* scheduling changes, we have to go back to ring-3.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 */
static int hmR0SvmPreRunGuest(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_ASSERT_PREEMPT_SAFE(pVCpu);
    HMSVM_ASSERT_NOT_IN_NESTED_GUEST(&pVCpu->cpum.GstCtx);

    /* Check force flag actions that might require us to go back to ring-3. */
    int rc = hmR0SvmCheckForceFlags(pVCpu);
    if (rc != VINF_SUCCESS)
        return rc;

    if (TRPMHasTrap(pVCpu))
        hmR0SvmTrpmTrapToPendingEvent(pVCpu);
    else if (!pVCpu->hm.s.Event.fPending)
        hmR0SvmEvaluatePendingEvent(pVCpu);

    /*
     * On the oldest AMD-V systems, we may not get enough information to reinject an NMI.
     * Just do it in software, see @bugref{8411}.
     * NB: If we could continue a task switch exit we wouldn't need to do this.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (RT_UNLIKELY(pVCpu->hm.s.Event.fPending && (((pVCpu->hm.s.Event.u64IntInfo >> 8) & 7) == SVM_EVENT_NMI)))
        if (RT_UNLIKELY(!pVM->hm.s.svm.u32Features))
            return VINF_EM_RAW_INJECT_TRPM_EVENT;

#ifdef HMSVM_SYNC_FULL_GUEST_STATE
    Assert(!(pVCpu->cpum.GstCtx->fExtrn & HMSVM_CPUMCTX_EXTRN_ALL));
    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
#endif

    /*
     * Export the guest state bits that are not shared with the host in any way as we can
     * longjmp or get preempted in the midst of exporting some of the state.
     */
    rc = hmR0SvmExportGuestState(pVCpu);
    AssertRCReturn(rc, rc);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExportFull);

    /*
     * If we're not intercepting TPR changes in the guest, save the guest TPR before the
     * world-switch so we can update it on the way back if the guest changed the TPR.
     */
    if (pVCpu->hm.s.svm.fSyncVTpr)
    {
        PCSVMVMCB pVmcb = pVCpu->hm.s.svm.pVmcb;
        if (pVM->hm.s.fTPRPatchingActive)
            pSvmTransient->u8GuestTpr = pVmcb->guest.u64LSTAR;
        else
            pSvmTransient->u8GuestTpr = pVmcb->ctrl.IntCtrl.n.u8VTPR;
    }

    /*
     * No longjmps to ring-3 from this point on!!!
     *
     * Asserts() will still longjmp to ring-3 (but won't return), which is intentional,
     * better than a kernel panic. This also disables flushing of the R0-logger instance.
     */
    VMMRZCallRing3Disable(pVCpu);

    /*
     * We disable interrupts so that we don't miss any interrupts that would flag preemption
     * (IPI/timers etc.) when thread-context hooks aren't used and we've been running with
     * preemption disabled for a while.  Since this is purly to aid the
     * RTThreadPreemptIsPending() code, it doesn't matter that it may temporarily reenable and
     * disable interrupt on NT.
     *
     * We need to check for force-flags that could've possible been altered since we last
     * checked them (e.g. by PDMGetInterrupt() leaving the PDM critical section,
     * see @bugref{6398}).
     *
     * We also check a couple of other force-flags as a last opportunity to get the EMT back
     * to ring-3 before executing guest code.
     */
    pSvmTransient->fEFlags = ASMIntDisableFlags();
    if (   VM_FF_IS_PENDING(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
        || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
    {
        ASMSetFlags(pSvmTransient->fEFlags);
        VMMRZCallRing3Enable(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHmToR3FF);
        return VINF_EM_RAW_TO_R3;
    }
    if (RTThreadPreemptIsPending(NIL_RTTHREAD))
    {
        ASMSetFlags(pSvmTransient->fEFlags);
        VMMRZCallRing3Enable(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatPendingHostIrq);
        return VINF_EM_RAW_INTERRUPT;
    }

    return VINF_SUCCESS;
}


/**
 * Prepares to run guest (or nested-guest) code in AMD-V and we've committed to
 * doing so.
 *
 * This means there is no backing out to ring-3 or anywhere else at this point.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 *
 * @remarks Called with preemption disabled.
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmPreRunGuestCommitted(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(VMMR0IsLogFlushDisabled(pVCpu));
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    VMCPU_ASSERT_STATE(pVCpu, VMCPUSTATE_STARTED_HM);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);            /* Indicate the start of guest execution. */

    PVM      pVM   = pVCpu->CTX_SUFF(pVM);
    PSVMVMCB pVmcb = pSvmTransient->pVmcb;

    hmR0SvmInjectPendingEvent(pVCpu, pVmcb);

    if (!CPUMIsGuestFPUStateActive(pVCpu))
    {
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatLoadGuestFpuState, x);
        CPUMR0LoadGuestFPU(pVM, pVCpu);     /* (Ignore rc, no need to set HM_CHANGED_HOST_CONTEXT for SVM.) */
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatLoadGuestFpuState, x);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatLoadGuestFpu);
    }

    /* Load the state shared between host and guest (FPU, debug). */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE)
        hmR0SvmExportSharedState(pVCpu, pVmcb);

    pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_HOST_CONTEXT;        /* Preemption might set this, nothing to do on AMD-V. */
    AssertMsg(!pVCpu->hm.s.fCtxChanged, ("fCtxChanged=%#RX64\n", pVCpu->hm.s.fCtxChanged));

    PHMGLOBALCPUINFO pHostCpu         = hmR0GetCurrentCpu();
    RTCPUID const    idHostCpu        = pHostCpu->idCpu;
    bool const       fMigratedHostCpu = idHostCpu != pVCpu->hm.s.idLastCpu;

    /* Setup TSC offsetting. */
    if (   pSvmTransient->fUpdateTscOffsetting
        || fMigratedHostCpu)
    {
        hmR0SvmUpdateTscOffsetting(pVCpu, pVmcb);
        pSvmTransient->fUpdateTscOffsetting = false;
    }

    /* If we've migrating CPUs, mark the VMCB Clean bits as dirty. */
    if (fMigratedHostCpu)
        pVmcb->ctrl.u32VmcbCleanBits = 0;

    /* Store status of the shared guest-host state at the time of VMRUN. */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
    if (CPUMIsGuestInLongModeEx(&pVCpu->cpum.GstCtx))
    {
        pSvmTransient->fWasGuestDebugStateActive = CPUMIsGuestDebugStateActivePending(pVCpu);
        pSvmTransient->fWasHyperDebugStateActive = CPUMIsHyperDebugStateActivePending(pVCpu);
    }
    else
#endif
    {
        pSvmTransient->fWasGuestDebugStateActive = CPUMIsGuestDebugStateActive(pVCpu);
        pSvmTransient->fWasHyperDebugStateActive = CPUMIsHyperDebugStateActive(pVCpu);
    }

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    uint8_t *pbMsrBitmap;
    if (!pSvmTransient->fIsNestedGuest)
        pbMsrBitmap = (uint8_t *)pVCpu->hm.s.svm.pvMsrBitmap;
    else
    {
        hmR0SvmMergeMsrpmNested(pHostCpu, pVCpu);

        /* Update the nested-guest VMCB with the newly merged MSRPM (clean bits updated below). */
        pVmcb->ctrl.u64MSRPMPhysAddr = pHostCpu->n.svm.HCPhysNstGstMsrpm;
        pbMsrBitmap = (uint8_t *)pHostCpu->n.svm.pvNstGstMsrpm;
    }
#else
    uint8_t *pbMsrBitmap = (uint8_t *)pVCpu->hm.s.svm.pvMsrBitmap;
#endif

    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, true);    /* Used for TLB flushing, set this across the world switch. */
    /* Flush the appropriate tagged-TLB entries. */
    hmR0SvmFlushTaggedTlb(pVCpu, pVmcb, pHostCpu);
    Assert(pVCpu->hm.s.idLastCpu == idHostCpu);

    STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatEntry, &pVCpu->hm.s.StatInGC, x);

    TMNotifyStartOfExecution(pVCpu);                            /* Finally, notify TM to resume its clocks as we're about
                                                                   to start executing. */

    /*
     * Save the current Host TSC_AUX and write the guest TSC_AUX to the host, so that RDTSCPs
     * (that don't cause exits) reads the guest MSR, see @bugref{3324}.
     *
     * This should be done -after- any RDTSCPs for obtaining the host timestamp (TM, STAM etc).
     */
    if (    (pVM->hm.s.cpuid.u32AMDFeatureEDX & X86_CPUID_EXT_FEATURE_EDX_RDTSCP)
        && !(pVmcb->ctrl.u64InterceptCtrl & SVM_CTRL_INTERCEPT_RDTSCP))
    {
        uint64_t const uGuestTscAux = CPUMGetGuestTscAux(pVCpu);
        pVCpu->hm.s.u64HostTscAux   = ASMRdMsr(MSR_K8_TSC_AUX);
        if (uGuestTscAux != pVCpu->hm.s.u64HostTscAux)
            ASMWrMsr(MSR_K8_TSC_AUX, uGuestTscAux);
        hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_TSC_AUX, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        pSvmTransient->fRestoreTscAuxMsr = true;
    }
    else
    {
        hmR0SvmSetMsrPermission(pVCpu, pbMsrBitmap, MSR_K8_TSC_AUX, SVMMSREXIT_INTERCEPT_READ, SVMMSREXIT_INTERCEPT_WRITE);
        pSvmTransient->fRestoreTscAuxMsr = false;
    }
    pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_IOPM_MSRPM;

    /*
     * If VMCB Clean bits isn't supported by the CPU or exposed to the guest in the nested
     * virtualization case, mark all state-bits as dirty indicating to the CPU to re-load
     * from the VMCB.
     */
    bool const fSupportsVmcbCleanBits = hmR0SvmSupportsVmcbCleanBits(pVCpu);
    if (!fSupportsVmcbCleanBits)
        pVmcb->ctrl.u32VmcbCleanBits = 0;
}


/**
 * Wrapper for running the guest (or nested-guest) code in AMD-V.
 *
 * @returns VBox strict status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   HCPhysVmcb  The host physical address of the VMCB.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0SvmRunGuest(PVMCPU pVCpu, RTHCPHYS HCPhysVmcb)
{
    /* Mark that HM is the keeper of all guest-CPU registers now that we're going to execute guest code. */
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    pCtx->fExtrn |= HMSVM_CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_KEEPER_HM;

    /*
     * 64-bit Windows uses XMM registers in the kernel as the Microsoft compiler expresses
     * floating-point operations using SSE instructions. Some XMM registers (XMM6-XMM15) are
     * callee-saved and thus the need for this XMM wrapper.
     *
     * Refer MSDN "Configuring Programs for 64-bit/x64 Software Conventions / Register Usage".
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
#ifdef VBOX_WITH_KERNEL_USING_XMM
    return hmR0SVMRunWrapXMM(pVCpu->hm.s.svm.HCPhysVmcbHost, HCPhysVmcb, pCtx, pVM, pVCpu, pVCpu->hm.s.svm.pfnVMRun);
#else
    return pVCpu->hm.s.svm.pfnVMRun(pVCpu->hm.s.svm.HCPhysVmcbHost, HCPhysVmcb, pCtx, pVM, pVCpu);
#endif
}


/**
 * Undoes the TSC offset applied for an SVM nested-guest and returns the TSC
 * value for the guest.
 *
 * @returns The TSC offset after undoing any nested-guest TSC offset.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   uTicks      The nested-guest TSC.
 *
 * @note    If you make any changes to this function, please check if
 *          hmR0SvmNstGstUndoTscOffset() needs adjusting.
 *
 * @sa      HMSvmNstGstApplyTscOffset().
 */
DECLINLINE(uint64_t) hmR0SvmNstGstUndoTscOffset(PVMCPU pVCpu, uint64_t uTicks)
{
    PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = &pVCpu->hm.s.svm.NstGstVmcbCache;
    Assert(pVmcbNstGstCache->fCacheValid);
    return uTicks - pVmcbNstGstCache->u64TSCOffset;
}


/**
 * Performs some essential restoration of state after running guest (or
 * nested-guest) code in AMD-V.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 * @param   rcVMRun         Return code of VMRUN.
 *
 * @remarks Called with interrupts disabled.
 * @remarks No-long-jump zone!!! This function will however re-enable longjmps
 *          unconditionally when it is safe to do so.
 */
static void hmR0SvmPostRunGuest(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient, int rcVMRun)
{
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    uint64_t const uHostTsc = ASMReadTSC();                     /* Read the TSC as soon as possible. */
    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, false);   /* See HMInvalidatePageOnAllVCpus(): used for TLB flushing. */
    ASMAtomicIncU32(&pVCpu->hm.s.cWorldSwitchExits);            /* Initialized in vmR3CreateUVM(): used for EMT poking. */

    PSVMVMCB     pVmcb     = pSvmTransient->pVmcb;
    PSVMVMCBCTRL pVmcbCtrl = &pVmcb->ctrl;

    /* TSC read must be done early for maximum accuracy. */
    if (!(pVmcbCtrl->u64InterceptCtrl & SVM_CTRL_INTERCEPT_RDTSC))
    {
        if (!pSvmTransient->fIsNestedGuest)
            TMCpuTickSetLastSeen(pVCpu, uHostTsc + pVmcbCtrl->u64TSCOffset);
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        else
        {
            /* The nested-guest VMCB TSC offset shall eventually be restored on #VMEXIT via HMSvmNstGstVmExitNotify(). */
            uint64_t const uGstTsc = hmR0SvmNstGstUndoTscOffset(pVCpu, uHostTsc + pVmcbCtrl->u64TSCOffset);
            TMCpuTickSetLastSeen(pVCpu, uGstTsc);
        }
#endif
    }

    if (pSvmTransient->fRestoreTscAuxMsr)
    {
        uint64_t u64GuestTscAuxMsr = ASMRdMsr(MSR_K8_TSC_AUX);
        CPUMSetGuestTscAux(pVCpu, u64GuestTscAuxMsr);
        if (u64GuestTscAuxMsr != pVCpu->hm.s.u64HostTscAux)
            ASMWrMsr(MSR_K8_TSC_AUX, pVCpu->hm.s.u64HostTscAux);
    }

    STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatInGC, &pVCpu->hm.s.StatPreExit, x);
    TMNotifyEndOfExecution(pVCpu);                              /* Notify TM that the guest is no longer running. */
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_HM);

    Assert(!(ASMGetFlags() & X86_EFL_IF));
    ASMSetFlags(pSvmTransient->fEFlags);                        /* Enable interrupts. */
    VMMRZCallRing3Enable(pVCpu);                                /* It is now safe to do longjmps to ring-3!!! */

    /* If VMRUN failed, we can bail out early. This does -not- cover SVM_EXIT_INVALID. */
    if (RT_UNLIKELY(rcVMRun != VINF_SUCCESS))
    {
        Log4Func(("VMRUN failure: rcVMRun=%Rrc\n", rcVMRun));
        return;
    }

    pSvmTransient->u64ExitCode        = pVmcbCtrl->u64ExitCode; /* Save the #VMEXIT reason. */
    pVmcbCtrl->u32VmcbCleanBits       = HMSVM_VMCB_CLEAN_ALL;   /* Mark the VMCB-state cache as unmodified by VMM. */
    pSvmTransient->fVectoringDoublePF = false;                  /* Vectoring double page-fault needs to be determined later. */
    pSvmTransient->fVectoringPF       = false;                  /* Vectoring page-fault needs to be determined later. */

#ifdef HMSVM_SYNC_FULL_GUEST_STATE
    hmR0SvmImportGuestState(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    Assert(!(pVCpu->cpum.GstCtx.fExtrn & HMSVM_CPUMCTX_EXTRN_ALL));
#else
    /*
     * Always import the following:
     *
     *   - RIP for exit optimizations and evaluating event injection on re-entry.
     *   - RFLAGS for evaluating event injection on VM re-entry and for exporting shared debug
     *     state on preemption.
     *   - Interrupt shadow, GIF for evaluating event injection on VM re-entry.
     *   - CS for exit optimizations.
     *   - RAX, RSP for simplifying assumptions on GPRs. All other GPRs are swapped by the
     *     assembly switcher code.
     *   - Shared state (only DR7 currently) for exporting shared debug state on preemption.
     */
    hmR0SvmImportGuestState(pVCpu, CPUMCTX_EXTRN_RIP
                                 | CPUMCTX_EXTRN_RFLAGS
                                 | CPUMCTX_EXTRN_RAX
                                 | CPUMCTX_EXTRN_RSP
                                 | CPUMCTX_EXTRN_CS
                                 | CPUMCTX_EXTRN_HWVIRT
                                 | CPUMCTX_EXTRN_HM_SVM_INT_SHADOW
                                 | CPUMCTX_EXTRN_HM_SVM_HWVIRT_VIRQ
                                 | HMSVM_CPUMCTX_SHARED_STATE);
#endif

    if (   pSvmTransient->u64ExitCode != SVM_EXIT_INVALID
        && pVCpu->hm.s.svm.fSyncVTpr)
    {
        Assert(!pSvmTransient->fIsNestedGuest);
        /* TPR patching (for 32-bit guests) uses LSTAR MSR for holding the TPR value, otherwise uses the VTPR. */
        if (   pVCpu->CTX_SUFF(pVM)->hm.s.fTPRPatchingActive
            && (pVmcb->guest.u64LSTAR & 0xff) != pSvmTransient->u8GuestTpr)
        {
            int rc = APICSetTpr(pVCpu, pVmcb->guest.u64LSTAR & 0xff);
            AssertRC(rc);
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
        }
        /* Sync TPR when we aren't intercepting CR8 writes. */
        else if (pSvmTransient->u8GuestTpr != pVmcbCtrl->IntCtrl.n.u8VTPR)
        {
            int rc = APICSetTpr(pVCpu, pVmcbCtrl->IntCtrl.n.u8VTPR << 4);
            AssertRC(rc);
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
        }
    }

#ifdef DEBUG_ramshankar
    if (CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
    {
        hmR0SvmImportGuestState(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
        hmR0SvmLogState(pVCpu, pVmcb, pVCpu->cpum.GstCtx, "hmR0SvmPostRunGuestNested", HMSVM_LOG_ALL & ~HMSVM_LOG_LBR,
                        0 /* uVerbose */);
    }
#endif

    HMSVM_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP);
    EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_SVM, pSvmTransient->u64ExitCode & EMEXIT_F_TYPE_MASK),
                     pVCpu->cpum.GstCtx.cs.u64Base + pVCpu->cpum.GstCtx.rip, uHostTsc);
}


/**
 * Runs the guest code using AMD-V.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pcLoops     Pointer to the number of executed loops.
 */
static int hmR0SvmRunGuestCodeNormal(PVMCPU pVCpu, uint32_t *pcLoops)
{
    uint32_t const cMaxResumeLoops = pVCpu->CTX_SUFF(pVM)->hm.s.cMaxResumeLoops;
    Assert(pcLoops);
    Assert(*pcLoops <= cMaxResumeLoops);

    SVMTRANSIENT SvmTransient;
    RT_ZERO(SvmTransient);
    SvmTransient.fUpdateTscOffsetting = true;
    SvmTransient.pVmcb = pVCpu->hm.s.svm.pVmcb;

    int rc = VERR_INTERNAL_ERROR_5;
    for (;;)
    {
        Assert(!HMR0SuspendPending());
        HMSVM_ASSERT_CPU_SAFE(pVCpu);

        /* Preparatory work for running nested-guest code, this may force us to return to
           ring-3.  This bugger disables interrupts on VINF_SUCCESS! */
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatEntry, x);
        rc = hmR0SvmPreRunGuest(pVCpu, &SvmTransient);
        if (rc != VINF_SUCCESS)
            break;

        /*
         * No longjmps to ring-3 from this point on!!!
         *
         * Asserts() will still longjmp to ring-3 (but won't return), which is intentional,
         * better than a kernel panic. This also disables flushing of the R0-logger instance.
         */
        hmR0SvmPreRunGuestCommitted(pVCpu, &SvmTransient);
        rc = hmR0SvmRunGuest(pVCpu, pVCpu->hm.s.svm.HCPhysVmcb);

        /* Restore any residual host-state and save any bits shared between host and guest
           into the guest-CPU state.  Re-enables interrupts! */
        hmR0SvmPostRunGuest(pVCpu, &SvmTransient, rc);

        if (RT_UNLIKELY(   rc != VINF_SUCCESS                               /* Check for VMRUN errors. */
                        || SvmTransient.u64ExitCode == SVM_EXIT_INVALID))   /* Check for invalid guest-state errors. */
        {
            if (rc == VINF_SUCCESS)
                rc = VERR_SVM_INVALID_GUEST_STATE;
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatPreExit, x);
            hmR0SvmReportWorldSwitchError(pVCpu, rc);
            break;
        }

        /* Handle the #VMEXIT. */
        HMSVM_EXITCODE_STAM_COUNTER_INC(SvmTransient.u64ExitCode);
        STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatPreExit, &pVCpu->hm.s.StatExitHandling, x);
        VBOXVMM_R0_HMSVM_VMEXIT(pVCpu, &pVCpu->cpum.GstCtx, SvmTransient.u64ExitCode, pVCpu->hm.s.svm.pVmcb);
        rc = hmR0SvmHandleExit(pVCpu, &SvmTransient);
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitHandling, x);
        if (rc != VINF_SUCCESS)
            break;
        if (++(*pcLoops) >= cMaxResumeLoops)
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchMaxResumeLoops);
            rc = VINF_EM_RAW_INTERRUPT;
            break;
        }
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    return rc;
}


/**
 * Runs the guest code using AMD-V in single step mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pcLoops     Pointer to the number of executed loops.
 */
static int hmR0SvmRunGuestCodeStep(PVMCPU pVCpu, uint32_t *pcLoops)
{
    uint32_t const cMaxResumeLoops = pVCpu->CTX_SUFF(pVM)->hm.s.cMaxResumeLoops;
    Assert(pcLoops);
    Assert(*pcLoops <= cMaxResumeLoops);

    SVMTRANSIENT SvmTransient;
    RT_ZERO(SvmTransient);
    SvmTransient.fUpdateTscOffsetting = true;
    SvmTransient.pVmcb = pVCpu->hm.s.svm.pVmcb;

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    uint16_t uCsStart  = pCtx->cs.Sel;
    uint64_t uRipStart = pCtx->rip;

    int rc = VERR_INTERNAL_ERROR_5;
    for (;;)
    {
        Assert(!HMR0SuspendPending());
        AssertMsg(pVCpu->hm.s.idEnteredCpu == RTMpCpuId(),
                  ("Illegal migration! Entered on CPU %u Current %u cLoops=%u\n", (unsigned)pVCpu->hm.s.idEnteredCpu,
                  (unsigned)RTMpCpuId(), *pcLoops));

        /* Preparatory work for running nested-guest code, this may force us to return to
           ring-3.  This bugger disables interrupts on VINF_SUCCESS! */
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatEntry, x);
        rc = hmR0SvmPreRunGuest(pVCpu, &SvmTransient);
        if (rc != VINF_SUCCESS)
            break;

        /*
         * No longjmps to ring-3 from this point on!!!
         *
         * Asserts() will still longjmp to ring-3 (but won't return), which is intentional,
         * better than a kernel panic. This also disables flushing of the R0-logger instance.
         */
        VMMRZCallRing3Disable(pVCpu);
        VMMRZCallRing3RemoveNotification(pVCpu);
        hmR0SvmPreRunGuestCommitted(pVCpu, &SvmTransient);

        rc = hmR0SvmRunGuest(pVCpu, pVCpu->hm.s.svm.HCPhysVmcb);

        /* Restore any residual host-state and save any bits shared between host and guest
           into the guest-CPU state.  Re-enables interrupts! */
        hmR0SvmPostRunGuest(pVCpu, &SvmTransient, rc);

        if (RT_UNLIKELY(   rc != VINF_SUCCESS                               /* Check for VMRUN errors. */
                        || SvmTransient.u64ExitCode == SVM_EXIT_INVALID))   /* Check for invalid guest-state errors. */
        {
            if (rc == VINF_SUCCESS)
                rc = VERR_SVM_INVALID_GUEST_STATE;
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatPreExit, x);
            hmR0SvmReportWorldSwitchError(pVCpu, rc);
            return rc;
        }

        /* Handle the #VMEXIT. */
        HMSVM_EXITCODE_STAM_COUNTER_INC(SvmTransient.u64ExitCode);
        STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatPreExit, &pVCpu->hm.s.StatExitHandling, x);
        VBOXVMM_R0_HMSVM_VMEXIT(pVCpu, pCtx, SvmTransient.u64ExitCode, pVCpu->hm.s.svm.pVmcb);
        rc = hmR0SvmHandleExit(pVCpu, &SvmTransient);
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitHandling, x);
        if (rc != VINF_SUCCESS)
            break;
        if (++(*pcLoops) >= cMaxResumeLoops)
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchMaxResumeLoops);
            rc = VINF_EM_RAW_INTERRUPT;
            break;
        }

        /*
         * Did the RIP change, if so, consider it a single step.
         * Otherwise, make sure one of the TFs gets set.
         */
        if (   pCtx->rip    != uRipStart
            || pCtx->cs.Sel != uCsStart)
        {
            rc = VINF_EM_DBG_STEPPED;
            break;
        }
        pVCpu->hm.s.fCtxChanged |= HM_CHANGED_GUEST_DR_MASK;
    }

    /*
     * Clear the X86_EFL_TF if necessary.
     */
    if (pVCpu->hm.s.fClearTrapFlag)
    {
        pVCpu->hm.s.fClearTrapFlag = false;
        pCtx->eflags.Bits.u1TF = 0;
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    return rc;
}

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Runs the nested-guest code using AMD-V.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pcLoops     Pointer to the number of executed loops. If we're switching
 *                      from the guest-code execution loop to this nested-guest
 *                      execution loop pass the remainder value, else pass 0.
 */
static int hmR0SvmRunGuestCodeNested(PVMCPU pVCpu, uint32_t *pcLoops)
{
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    HMSVM_ASSERT_IN_NESTED_GUEST(pCtx);
    Assert(pcLoops);
    Assert(*pcLoops <= pVCpu->CTX_SUFF(pVM)->hm.s.cMaxResumeLoops);

    SVMTRANSIENT SvmTransient;
    RT_ZERO(SvmTransient);
    SvmTransient.fUpdateTscOffsetting = true;
    SvmTransient.pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb);
    SvmTransient.fIsNestedGuest = true;

    int rc = VERR_INTERNAL_ERROR_4;
    for (;;)
    {
        Assert(!HMR0SuspendPending());
        HMSVM_ASSERT_CPU_SAFE(pVCpu);

        /* Preparatory work for running nested-guest code, this may force us to return to
           ring-3.  This bugger disables interrupts on VINF_SUCCESS! */
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatEntry, x);
        rc = hmR0SvmPreRunGuestNested(pVCpu, &SvmTransient);
        if (    rc != VINF_SUCCESS
            || !CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
        {
            break;
        }

        /*
         * No longjmps to ring-3 from this point on!!!
         *
         * Asserts() will still longjmp to ring-3 (but won't return), which is intentional,
         * better than a kernel panic. This also disables flushing of the R0-logger instance.
         */
        hmR0SvmPreRunGuestCommitted(pVCpu, &SvmTransient);

        rc = hmR0SvmRunGuest(pVCpu, pCtx->hwvirt.svm.HCPhysVmcb);

        /* Restore any residual host-state and save any bits shared between host and guest
           into the guest-CPU state.  Re-enables interrupts! */
        hmR0SvmPostRunGuest(pVCpu, &SvmTransient, rc);

        if (RT_LIKELY(   rc == VINF_SUCCESS
                      && SvmTransient.u64ExitCode != SVM_EXIT_INVALID))
        { /* extremely likely */ }
        else
        {
            /* VMRUN failed, shouldn't really happen, Guru. */
            if (rc != VINF_SUCCESS)
                break;

            /* Invalid nested-guest state. Cause a #VMEXIT but assert on strict builds. */
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
            AssertMsgFailed(("Invalid nested-guest state. rc=%Rrc u64ExitCode=%#RX64\n", rc, SvmTransient.u64ExitCode));
            rc = VBOXSTRICTRC_TODO(IEMExecSvmVmexit(pVCpu, SVM_EXIT_INVALID, 0, 0));
            break;
        }

        /* Handle the #VMEXIT. */
        HMSVM_NESTED_EXITCODE_STAM_COUNTER_INC(SvmTransient.u64ExitCode);
        STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatPreExit, &pVCpu->hm.s.StatExitHandling, x);
        VBOXVMM_R0_HMSVM_VMEXIT(pVCpu, pCtx, SvmTransient.u64ExitCode, pCtx->hwvirt.svm.CTX_SUFF(pVmcb));
        rc = hmR0SvmHandleExitNested(pVCpu, &SvmTransient);
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitHandling, x);
        if (    rc != VINF_SUCCESS
            || !CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
            break;
        if (++(*pcLoops) >= pVCpu->CTX_SUFF(pVM)->hm.s.cMaxResumeLoops)
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchMaxResumeLoops);
            rc = VINF_EM_RAW_INTERRUPT;
            break;
        }

        /** @todo handle single-stepping   */
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    return rc;
}
#endif


/**
 * Runs the guest code using AMD-V.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMR0DECL(VBOXSTRICTRC) SVMR0RunGuestCode(PVMCPU pVCpu)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    HMSVM_ASSERT_PREEMPT_SAFE(pVCpu);
    VMMRZCallRing3SetNotification(pVCpu, hmR0SvmCallRing3Callback, NULL /* pvUser */);

    uint32_t cLoops = 0;
    int      rc;
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    if (!CPUMIsGuestInSvmNestedHwVirtMode(&pVCpu->cpum.GstCtx))
#endif
    {
        if (!pVCpu->hm.s.fSingleInstruction)
            rc = hmR0SvmRunGuestCodeNormal(pVCpu, &cLoops);
        else
            rc = hmR0SvmRunGuestCodeStep(pVCpu, &cLoops);
    }
#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
    else
    {
        rc = VINF_SVM_VMRUN;
    }

    /* Re-check the nested-guest condition here as we may be transitioning from the normal
       execution loop into the nested-guest, hence this is not placed in the 'else' part above. */
    if (rc == VINF_SVM_VMRUN)
    {
        rc = hmR0SvmRunGuestCodeNested(pVCpu, &cLoops);
        if (rc == VINF_SVM_VMEXIT)
            rc = VINF_SUCCESS;
    }
#endif

    /* Fixup error codes. */
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;
    else if (rc == VINF_EM_RESET)
        rc = VINF_EM_TRIPLE_FAULT;

    /* Prepare to return to ring-3. This will remove longjmp notifications. */
    rc = hmR0SvmExitToRing3(pVCpu, rc);
    Assert(!VMMRZCallRing3IsNotificationSet(pVCpu));
    return rc;
}


#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * Determines whether an IOIO intercept is active for the nested-guest or not.
 *
 * @param   pvIoBitmap      Pointer to the nested-guest IO bitmap.
 * @param   pIoExitInfo     Pointer to the SVMIOIOEXITINFO.
 */
static bool hmR0SvmIsIoInterceptActive(void *pvIoBitmap, PSVMIOIOEXITINFO pIoExitInfo)
{
    const uint16_t    u16Port       = pIoExitInfo->n.u16Port;
    const SVMIOIOTYPE enmIoType     = (SVMIOIOTYPE)pIoExitInfo->n.u1Type;
    const uint8_t     cbReg         = (pIoExitInfo->u  >> SVM_IOIO_OP_SIZE_SHIFT)   & 7;
    const uint8_t     cAddrSizeBits = ((pIoExitInfo->u >> SVM_IOIO_ADDR_SIZE_SHIFT) & 7) << 4;
    const uint8_t     iEffSeg       = pIoExitInfo->n.u3Seg;
    const bool        fRep          = pIoExitInfo->n.u1Rep;
    const bool        fStrIo        = pIoExitInfo->n.u1Str;

    return HMSvmIsIOInterceptActive(pvIoBitmap, u16Port, enmIoType, cbReg, cAddrSizeBits, iEffSeg, fRep, fStrIo,
                                    NULL /* pIoExitInfo */);
}


/**
 * Handles a nested-guest \#VMEXIT (for all EXITCODE values except
 * SVM_EXIT_INVALID).
 *
 * @returns VBox status code (informational status codes included).
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 */
static int hmR0SvmHandleExitNested(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_ASSERT_IN_NESTED_GUEST(&pVCpu->cpum.GstCtx);
    Assert(pSvmTransient->u64ExitCode != SVM_EXIT_INVALID);
    Assert(pSvmTransient->u64ExitCode <= SVM_EXIT_MAX);

    /*
     * We import the complete state here because we use separate VMCBs for the guest and the
     * nested-guest, and the guest's VMCB is used after the #VMEXIT. We can only save/restore
     * the #VMEXIT specific state if we used the same VMCB for both guest and nested-guest.
     */
#define NST_GST_VMEXIT_CALL_RET(a_pVCpu, a_uExitCode, a_uExitInfo1, a_uExitInfo2) \
    do { \
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL); \
        return VBOXSTRICTRC_TODO(IEMExecSvmVmexit((a_pVCpu), (a_uExitCode), (a_uExitInfo1), (a_uExitInfo2))); \
    } while (0)

    /*
     * For all the #VMEXITs here we primarily figure out if the #VMEXIT is expected by the
     * nested-guest. If it isn't, it should be handled by the (outer) guest.
     */
    PSVMVMCB       pVmcbNstGst     = pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pVmcb);
    PSVMVMCBCTRL   pVmcbNstGstCtrl = &pVmcbNstGst->ctrl;
    uint64_t const uExitCode       = pVmcbNstGstCtrl->u64ExitCode;
    uint64_t const uExitInfo1      = pVmcbNstGstCtrl->u64ExitInfo1;
    uint64_t const uExitInfo2      = pVmcbNstGstCtrl->u64ExitInfo2;

    Assert(uExitCode == pVmcbNstGstCtrl->u64ExitCode);
    switch (uExitCode)
    {
        case SVM_EXIT_CPUID:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_CPUID))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitCpuid(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_RDTSC:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_RDTSC))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitRdtsc(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_RDTSCP:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_RDTSCP))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitRdtscp(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_MONITOR:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_MONITOR))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitMonitor(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_MWAIT:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_MWAIT))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitMwait(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_HLT:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_HLT))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitHlt(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_MSR:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_MSR_PROT))
            {
                uint32_t const idMsr = pVCpu->cpum.GstCtx.ecx;
                uint16_t offMsrpm;
                uint8_t  uMsrpmBit;
                int rc = HMSvmGetMsrpmOffsetAndBit(idMsr, &offMsrpm, &uMsrpmBit);
                if (RT_SUCCESS(rc))
                {
                    Assert(uMsrpmBit == 0 || uMsrpmBit == 2 || uMsrpmBit == 4 || uMsrpmBit == 6);
                    Assert(offMsrpm < SVM_MSRPM_PAGES << X86_PAGE_4K_SHIFT);

                    uint8_t const *pbMsrBitmap = (uint8_t const *)pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pvMsrBitmap);
                    pbMsrBitmap               += offMsrpm;
                    bool const fInterceptRead  = RT_BOOL(*pbMsrBitmap & RT_BIT(uMsrpmBit));
                    bool const fInterceptWrite = RT_BOOL(*pbMsrBitmap & RT_BIT(uMsrpmBit + 1));

                    if (   (fInterceptWrite && pVmcbNstGstCtrl->u64ExitInfo1 == SVM_EXIT1_MSR_WRITE)
                        || (fInterceptRead  && pVmcbNstGstCtrl->u64ExitInfo1 == SVM_EXIT1_MSR_READ))
                    {
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    }
                }
                else
                {
                    /*
                     * MSRs not covered by the MSRPM automatically cause an #VMEXIT.
                     * See AMD-V spec. "15.11 MSR Intercepts".
                     */
                    Assert(rc == VERR_OUT_OF_RANGE);
                    NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                }
            }
            return hmR0SvmExitMsr(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_IOIO:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_IOIO_PROT))
            {
                void *pvIoBitmap = pVCpu->cpum.GstCtx.hwvirt.svm.CTX_SUFF(pvIoBitmap);
                SVMIOIOEXITINFO IoExitInfo;
                IoExitInfo.u = pVmcbNstGst->ctrl.u64ExitInfo1;
                bool const fIntercept = hmR0SvmIsIoInterceptActive(pvIoBitmap, &IoExitInfo);
                if (fIntercept)
                    NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            }
            return hmR0SvmExitIOInstr(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_XCPT_PF:
        {
            PVM pVM = pVCpu->CTX_SUFF(pVM);
            if (pVM->hm.s.fNestedPaging)
            {
                uint32_t const u32ErrCode    = pVmcbNstGstCtrl->u64ExitInfo1;
                uint64_t const uFaultAddress = pVmcbNstGstCtrl->u64ExitInfo2;

                /* If the nested-guest is intercepting #PFs, cause a #PF #VMEXIT. */
                if (HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_PF))
                    NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, u32ErrCode, uFaultAddress);

                /* If the nested-guest is not intercepting #PFs, forward the #PF to the guest. */
                HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_CR2);
                hmR0SvmSetPendingXcptPF(pVCpu, u32ErrCode, uFaultAddress);
                return VINF_SUCCESS;
            }
            return hmR0SvmExitXcptPF(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_XCPT_UD:
        {
            if (HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_UD))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            hmR0SvmSetPendingXcptUD(pVCpu);
            return VINF_SUCCESS;
        }

        case SVM_EXIT_XCPT_MF:
        {
            if (HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_MF))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitXcptMF(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_XCPT_DB:
        {
            if (HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_DB))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmNestedExitXcptDB(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_XCPT_AC:
        {
            if (HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_AC))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitXcptAC(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_XCPT_BP:
        {
            if (HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_BP))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmNestedExitXcptBP(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_READ_CR0:
        case SVM_EXIT_READ_CR3:
        case SVM_EXIT_READ_CR4:
        {
            uint8_t const uCr = uExitCode - SVM_EXIT_READ_CR0;
            if (HMIsGuestSvmReadCRxInterceptSet(pVCpu, uCr))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitReadCRx(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_CR0_SEL_WRITE:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_CR0_SEL_WRITE))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitWriteCRx(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_WRITE_CR0:
        case SVM_EXIT_WRITE_CR3:
        case SVM_EXIT_WRITE_CR4:
        case SVM_EXIT_WRITE_CR8:    /* CR8 writes would go to the V_TPR rather than here, since we run with V_INTR_MASKING. */
        {
            uint8_t const uCr = uExitCode - SVM_EXIT_WRITE_CR0;
            Log4Func(("Write CR%u: uExitInfo1=%#RX64 uExitInfo2=%#RX64\n", uCr, uExitInfo1, uExitInfo2));

            if (HMIsGuestSvmWriteCRxInterceptSet(pVCpu, uCr))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitWriteCRx(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_PAUSE:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_PAUSE))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitPause(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_VINTR:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_VINTR))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitUnexpected(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_INTR:
        case SVM_EXIT_NMI:
        case SVM_EXIT_SMI:
        case SVM_EXIT_XCPT_NMI:     /* Should not occur, SVM_EXIT_NMI is used instead. */
        {
            /*
             * We shouldn't direct physical interrupts, NMIs, SMIs to the nested-guest.
             *
             * Although we don't intercept SMIs, the nested-guest might. Therefore, we might
             * get an SMI #VMEXIT here so simply ignore rather than causing a corresponding
             * nested-guest #VMEXIT.
             *
             * We shall import the complete state here as we may cause #VMEXITs from ring-3
             * while trying to inject interrupts, see comment at the top of this function.
             */
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_ALL);
            return hmR0SvmExitIntr(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_FERR_FREEZE:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_FERR_FREEZE))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitFerrFreeze(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_INVLPG:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_INVLPG))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitInvlpg(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_WBINVD:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_WBINVD))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitWbinvd(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_INVD:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_INVD))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitInvd(pVCpu, pSvmTransient);
        }

        case SVM_EXIT_RDPMC:
        {
            if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_RDPMC))
                NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
            return hmR0SvmExitRdpmc(pVCpu, pSvmTransient);
        }

        default:
        {
            switch (uExitCode)
            {
                case SVM_EXIT_READ_DR0:     case SVM_EXIT_READ_DR1:     case SVM_EXIT_READ_DR2:     case SVM_EXIT_READ_DR3:
                case SVM_EXIT_READ_DR6:     case SVM_EXIT_READ_DR7:     case SVM_EXIT_READ_DR8:     case SVM_EXIT_READ_DR9:
                case SVM_EXIT_READ_DR10:    case SVM_EXIT_READ_DR11:    case SVM_EXIT_READ_DR12:    case SVM_EXIT_READ_DR13:
                case SVM_EXIT_READ_DR14:    case SVM_EXIT_READ_DR15:
                {
                    uint8_t const uDr = uExitCode - SVM_EXIT_READ_DR0;
                    if (HMIsGuestSvmReadDRxInterceptSet(pVCpu, uDr))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitReadDRx(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_WRITE_DR0:    case SVM_EXIT_WRITE_DR1:    case SVM_EXIT_WRITE_DR2:    case SVM_EXIT_WRITE_DR3:
                case SVM_EXIT_WRITE_DR6:    case SVM_EXIT_WRITE_DR7:    case SVM_EXIT_WRITE_DR8:    case SVM_EXIT_WRITE_DR9:
                case SVM_EXIT_WRITE_DR10:   case SVM_EXIT_WRITE_DR11:   case SVM_EXIT_WRITE_DR12:   case SVM_EXIT_WRITE_DR13:
                case SVM_EXIT_WRITE_DR14:   case SVM_EXIT_WRITE_DR15:
                {
                    uint8_t const uDr = uExitCode - SVM_EXIT_WRITE_DR0;
                    if (HMIsGuestSvmWriteDRxInterceptSet(pVCpu, uDr))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitWriteDRx(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_XCPT_DE:
                /*   SVM_EXIT_XCPT_DB: */       /* Handled above. */
                /*   SVM_EXIT_XCPT_NMI: */      /* Handled above. */
                /*   SVM_EXIT_XCPT_BP: */       /* Handled above. */
                case SVM_EXIT_XCPT_OF:
                case SVM_EXIT_XCPT_BR:
                /*   SVM_EXIT_XCPT_UD: */       /* Handled above. */
                case SVM_EXIT_XCPT_NM:
                case SVM_EXIT_XCPT_DF:
                case SVM_EXIT_XCPT_CO_SEG_OVERRUN:
                case SVM_EXIT_XCPT_TS:
                case SVM_EXIT_XCPT_NP:
                case SVM_EXIT_XCPT_SS:
                case SVM_EXIT_XCPT_GP:
                /*   SVM_EXIT_XCPT_PF: */       /* Handled above. */
                case SVM_EXIT_XCPT_15:          /* Reserved.      */
                /*   SVM_EXIT_XCPT_MF: */       /* Handled above. */
                /*   SVM_EXIT_XCPT_AC: */       /* Handled above. */
                case SVM_EXIT_XCPT_MC:
                case SVM_EXIT_XCPT_XF:
                case SVM_EXIT_XCPT_20: case SVM_EXIT_XCPT_21: case SVM_EXIT_XCPT_22: case SVM_EXIT_XCPT_23:
                case SVM_EXIT_XCPT_24: case SVM_EXIT_XCPT_25: case SVM_EXIT_XCPT_26: case SVM_EXIT_XCPT_27:
                case SVM_EXIT_XCPT_28: case SVM_EXIT_XCPT_29: case SVM_EXIT_XCPT_30: case SVM_EXIT_XCPT_31:
                {
                    uint8_t const uVector = uExitCode - SVM_EXIT_XCPT_0;
                    if (HMIsGuestSvmXcptInterceptSet(pVCpu, uVector))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitXcptGeneric(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_XSETBV:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_XSETBV))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitXsetbv(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_TASK_SWITCH:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_TASK_SWITCH))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitTaskSwitch(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_IRET:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_IRET))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitIret(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_SHUTDOWN:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_SHUTDOWN))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitShutdown(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_VMMCALL:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_VMMCALL))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitVmmCall(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_CLGI:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_CLGI))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                     return hmR0SvmExitClgi(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_STGI:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_STGI))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                     return hmR0SvmExitStgi(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_VMLOAD:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_VMLOAD))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitVmload(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_VMSAVE:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_VMSAVE))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitVmsave(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_INVLPGA:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_INVLPGA))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitInvlpga(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_VMRUN:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_VMRUN))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    return hmR0SvmExitVmrun(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_RSM:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_RSM))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    hmR0SvmSetPendingXcptUD(pVCpu);
                    return VINF_SUCCESS;
                }

                case SVM_EXIT_SKINIT:
                {
                    if (HMIsGuestSvmCtrlInterceptSet(pVCpu, SVM_CTRL_INTERCEPT_SKINIT))
                        NST_GST_VMEXIT_CALL_RET(pVCpu, uExitCode, uExitInfo1, uExitInfo2);
                    hmR0SvmSetPendingXcptUD(pVCpu);
                    return VINF_SUCCESS;
                }

                case SVM_EXIT_NPF:
                {
                    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);
                    return hmR0SvmExitNestedPF(pVCpu, pSvmTransient);
                }

                case SVM_EXIT_INIT:  /* We shouldn't get INIT signals while executing a nested-guest. */
                    return hmR0SvmExitUnexpected(pVCpu, pSvmTransient);

                default:
                {
                    AssertMsgFailed(("hmR0SvmHandleExitNested: Unknown exit code %#x\n", pSvmTransient->u64ExitCode));
                    pVCpu->hm.s.u32HMError = pSvmTransient->u64ExitCode;
                    return VERR_SVM_UNKNOWN_EXIT;
                }
            }
        }
    }
    /* not reached */

#undef NST_GST_VMEXIT_CALL_RET
}
#endif


/**
 * Handles a guest \#VMEXIT (for all EXITCODE values except SVM_EXIT_INVALID).
 *
 * @returns VBox status code (informational status codes included).
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 */
static int hmR0SvmHandleExit(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    Assert(pSvmTransient->u64ExitCode != SVM_EXIT_INVALID);
    Assert(pSvmTransient->u64ExitCode <= SVM_EXIT_MAX);

#ifdef DEBUG_ramshankar
# define VMEXIT_CALL_RET(a_fDbg, a_CallExpr) \
        do { \
            if ((a_fDbg) == 1) \
                HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL); \
            int rc = a_CallExpr; \
            if ((a_fDbg) == 1) \
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST); \
            return rc; \
        } while (0)
#else
# define VMEXIT_CALL_RET(a_fDbg, a_CallExpr) return a_CallExpr
#endif

    /*
     * The ordering of the case labels is based on most-frequently-occurring #VMEXITs
     * for most guests under normal workloads (for some definition of "normal").
     */
    uint64_t const uExitCode = pSvmTransient->u64ExitCode;
    switch (uExitCode)
    {
        case SVM_EXIT_NPF:          VMEXIT_CALL_RET(0, hmR0SvmExitNestedPF(pVCpu, pSvmTransient));
        case SVM_EXIT_IOIO:         VMEXIT_CALL_RET(0, hmR0SvmExitIOInstr(pVCpu, pSvmTransient));
        case SVM_EXIT_RDTSC:        VMEXIT_CALL_RET(0, hmR0SvmExitRdtsc(pVCpu, pSvmTransient));
        case SVM_EXIT_RDTSCP:       VMEXIT_CALL_RET(0, hmR0SvmExitRdtscp(pVCpu, pSvmTransient));
        case SVM_EXIT_CPUID:        VMEXIT_CALL_RET(0, hmR0SvmExitCpuid(pVCpu, pSvmTransient));
        case SVM_EXIT_XCPT_PF:      VMEXIT_CALL_RET(0, hmR0SvmExitXcptPF(pVCpu, pSvmTransient));
        case SVM_EXIT_MSR:          VMEXIT_CALL_RET(0, hmR0SvmExitMsr(pVCpu, pSvmTransient));
        case SVM_EXIT_MONITOR:      VMEXIT_CALL_RET(0, hmR0SvmExitMonitor(pVCpu, pSvmTransient));
        case SVM_EXIT_MWAIT:        VMEXIT_CALL_RET(0, hmR0SvmExitMwait(pVCpu, pSvmTransient));
        case SVM_EXIT_HLT:          VMEXIT_CALL_RET(0, hmR0SvmExitHlt(pVCpu, pSvmTransient));

        case SVM_EXIT_XCPT_NMI:     /* Should not occur, SVM_EXIT_NMI is used instead. */
        case SVM_EXIT_INTR:
        case SVM_EXIT_NMI:          VMEXIT_CALL_RET(0, hmR0SvmExitIntr(pVCpu, pSvmTransient));

        case SVM_EXIT_READ_CR0:
        case SVM_EXIT_READ_CR3:
        case SVM_EXIT_READ_CR4:     VMEXIT_CALL_RET(0, hmR0SvmExitReadCRx(pVCpu, pSvmTransient));

        case SVM_EXIT_CR0_SEL_WRITE:
        case SVM_EXIT_WRITE_CR0:
        case SVM_EXIT_WRITE_CR3:
        case SVM_EXIT_WRITE_CR4:
        case SVM_EXIT_WRITE_CR8:    VMEXIT_CALL_RET(0, hmR0SvmExitWriteCRx(pVCpu, pSvmTransient));

        case SVM_EXIT_VINTR:        VMEXIT_CALL_RET(0, hmR0SvmExitVIntr(pVCpu, pSvmTransient));
        case SVM_EXIT_PAUSE:        VMEXIT_CALL_RET(0, hmR0SvmExitPause(pVCpu, pSvmTransient));
        case SVM_EXIT_VMMCALL:      VMEXIT_CALL_RET(0, hmR0SvmExitVmmCall(pVCpu, pSvmTransient));
        case SVM_EXIT_INVLPG:       VMEXIT_CALL_RET(0, hmR0SvmExitInvlpg(pVCpu, pSvmTransient));
        case SVM_EXIT_WBINVD:       VMEXIT_CALL_RET(0, hmR0SvmExitWbinvd(pVCpu, pSvmTransient));
        case SVM_EXIT_INVD:         VMEXIT_CALL_RET(0, hmR0SvmExitInvd(pVCpu, pSvmTransient));
        case SVM_EXIT_RDPMC:        VMEXIT_CALL_RET(0, hmR0SvmExitRdpmc(pVCpu, pSvmTransient));
        case SVM_EXIT_IRET:         VMEXIT_CALL_RET(0, hmR0SvmExitIret(pVCpu, pSvmTransient));
        case SVM_EXIT_XCPT_UD:      VMEXIT_CALL_RET(0, hmR0SvmExitXcptUD(pVCpu, pSvmTransient));
        case SVM_EXIT_XCPT_MF:      VMEXIT_CALL_RET(0, hmR0SvmExitXcptMF(pVCpu, pSvmTransient));
        case SVM_EXIT_XCPT_DB:      VMEXIT_CALL_RET(0, hmR0SvmExitXcptDB(pVCpu, pSvmTransient));
        case SVM_EXIT_XCPT_AC:      VMEXIT_CALL_RET(0, hmR0SvmExitXcptAC(pVCpu, pSvmTransient));
        case SVM_EXIT_XCPT_BP:      VMEXIT_CALL_RET(0, hmR0SvmExitXcptBP(pVCpu, pSvmTransient));
        case SVM_EXIT_XSETBV:       VMEXIT_CALL_RET(0, hmR0SvmExitXsetbv(pVCpu, pSvmTransient));
        case SVM_EXIT_FERR_FREEZE:  VMEXIT_CALL_RET(0, hmR0SvmExitFerrFreeze(pVCpu, pSvmTransient));

        default:
        {
            switch (pSvmTransient->u64ExitCode)
            {
                case SVM_EXIT_READ_DR0:     case SVM_EXIT_READ_DR1:     case SVM_EXIT_READ_DR2:     case SVM_EXIT_READ_DR3:
                case SVM_EXIT_READ_DR6:     case SVM_EXIT_READ_DR7:     case SVM_EXIT_READ_DR8:     case SVM_EXIT_READ_DR9:
                case SVM_EXIT_READ_DR10:    case SVM_EXIT_READ_DR11:    case SVM_EXIT_READ_DR12:    case SVM_EXIT_READ_DR13:
                case SVM_EXIT_READ_DR14:    case SVM_EXIT_READ_DR15:
                    VMEXIT_CALL_RET(0, hmR0SvmExitReadDRx(pVCpu, pSvmTransient));

                case SVM_EXIT_WRITE_DR0:    case SVM_EXIT_WRITE_DR1:    case SVM_EXIT_WRITE_DR2:    case SVM_EXIT_WRITE_DR3:
                case SVM_EXIT_WRITE_DR6:    case SVM_EXIT_WRITE_DR7:    case SVM_EXIT_WRITE_DR8:    case SVM_EXIT_WRITE_DR9:
                case SVM_EXIT_WRITE_DR10:   case SVM_EXIT_WRITE_DR11:   case SVM_EXIT_WRITE_DR12:   case SVM_EXIT_WRITE_DR13:
                case SVM_EXIT_WRITE_DR14:   case SVM_EXIT_WRITE_DR15:
                    VMEXIT_CALL_RET(0, hmR0SvmExitWriteDRx(pVCpu, pSvmTransient));

                case SVM_EXIT_TASK_SWITCH:  VMEXIT_CALL_RET(0, hmR0SvmExitTaskSwitch(pVCpu, pSvmTransient));
                case SVM_EXIT_SHUTDOWN:     VMEXIT_CALL_RET(0, hmR0SvmExitShutdown(pVCpu, pSvmTransient));

                case SVM_EXIT_SMI:
                case SVM_EXIT_INIT:
                {
                    /*
                     * We don't intercept SMIs. As for INIT signals, it really shouldn't ever happen here.
                     * If it ever does, we want to know about it so log the exit code and bail.
                     */
                    VMEXIT_CALL_RET(0, hmR0SvmExitUnexpected(pVCpu, pSvmTransient));
                }

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
                case SVM_EXIT_CLGI:     VMEXIT_CALL_RET(0, hmR0SvmExitClgi(pVCpu, pSvmTransient));
                case SVM_EXIT_STGI:     VMEXIT_CALL_RET(0, hmR0SvmExitStgi(pVCpu, pSvmTransient));
                case SVM_EXIT_VMLOAD:   VMEXIT_CALL_RET(0, hmR0SvmExitVmload(pVCpu, pSvmTransient));
                case SVM_EXIT_VMSAVE:   VMEXIT_CALL_RET(0, hmR0SvmExitVmsave(pVCpu, pSvmTransient));
                case SVM_EXIT_INVLPGA:  VMEXIT_CALL_RET(0, hmR0SvmExitInvlpga(pVCpu, pSvmTransient));
                case SVM_EXIT_VMRUN:    VMEXIT_CALL_RET(0, hmR0SvmExitVmrun(pVCpu, pSvmTransient));
#else
                case SVM_EXIT_CLGI:
                case SVM_EXIT_STGI:
                case SVM_EXIT_VMLOAD:
                case SVM_EXIT_VMSAVE:
                case SVM_EXIT_INVLPGA:
                case SVM_EXIT_VMRUN:
#endif
                case SVM_EXIT_RSM:
                case SVM_EXIT_SKINIT:
                {
                    hmR0SvmSetPendingXcptUD(pVCpu);
                    return VINF_SUCCESS;
                }

#ifdef HMSVM_ALWAYS_TRAP_ALL_XCPTS
                case SVM_EXIT_XCPT_DE:
                /*   SVM_EXIT_XCPT_DB: */       /* Handled above. */
                /*   SVM_EXIT_XCPT_NMI: */      /* Handled above. */
                /*   SVM_EXIT_XCPT_BP: */       /* Handled above. */
                case SVM_EXIT_XCPT_OF:
                case SVM_EXIT_XCPT_BR:
                /*   SVM_EXIT_XCPT_UD: */       /* Handled above. */
                case SVM_EXIT_XCPT_NM:
                case SVM_EXIT_XCPT_DF:
                case SVM_EXIT_XCPT_CO_SEG_OVERRUN:
                case SVM_EXIT_XCPT_TS:
                case SVM_EXIT_XCPT_NP:
                case SVM_EXIT_XCPT_SS:
                case SVM_EXIT_XCPT_GP:
                /*   SVM_EXIT_XCPT_PF: */
                case SVM_EXIT_XCPT_15:          /* Reserved. */
                /*   SVM_EXIT_XCPT_MF: */       /* Handled above. */
                /*   SVM_EXIT_XCPT_AC: */       /* Handled above. */
                case SVM_EXIT_XCPT_MC:
                case SVM_EXIT_XCPT_XF:
                case SVM_EXIT_XCPT_20: case SVM_EXIT_XCPT_21: case SVM_EXIT_XCPT_22: case SVM_EXIT_XCPT_23:
                case SVM_EXIT_XCPT_24: case SVM_EXIT_XCPT_25: case SVM_EXIT_XCPT_26: case SVM_EXIT_XCPT_27:
                case SVM_EXIT_XCPT_28: case SVM_EXIT_XCPT_29: case SVM_EXIT_XCPT_30: case SVM_EXIT_XCPT_31:
                    VMEXIT_CALL_RET(0, hmR0SvmExitXcptGeneric(pVCpu, pSvmTransient));
#endif  /* HMSVM_ALWAYS_TRAP_ALL_XCPTS */

                default:
                {
                    AssertMsgFailed(("hmR0SvmHandleExit: Unknown exit code %#RX64\n", uExitCode));
                    pVCpu->hm.s.u32HMError = uExitCode;
                    return VERR_SVM_UNKNOWN_EXIT;
                }
            }
        }
    }
    /* not reached */
#undef VMEXIT_CALL_RET
}


#ifdef VBOX_STRICT
/* Is there some generic IPRT define for this that are not in Runtime/internal/\* ?? */
# define HMSVM_ASSERT_PREEMPT_CPUID_VAR() \
    RTCPUID const idAssertCpu = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId()

# define HMSVM_ASSERT_PREEMPT_CPUID() \
    do \
    { \
         RTCPUID const idAssertCpuNow = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId(); \
         AssertMsg(idAssertCpu == idAssertCpuNow, ("SVM %#x, %#x\n", idAssertCpu, idAssertCpuNow)); \
    } while (0)

# define HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(a_pVCpu, a_pSvmTransient) \
    do { \
        AssertPtr((a_pVCpu)); \
        AssertPtr((a_pSvmTransient)); \
        Assert(ASMIntAreEnabled()); \
        HMSVM_ASSERT_PREEMPT_SAFE((a_pVCpu)); \
        HMSVM_ASSERT_PREEMPT_CPUID_VAR(); \
        Log4Func(("vcpu[%u] -v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-\n", (a_pVCpu)->idCpu)); \
        HMSVM_ASSERT_PREEMPT_SAFE((a_pVCpu)); \
        if (VMMR0IsLogFlushDisabled((a_pVCpu))) \
            HMSVM_ASSERT_PREEMPT_CPUID(); \
    } while (0)
#else
# define HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(a_pVCpu, a_pSvmTransient) \
    do { \
        RT_NOREF2(a_pVCpu, a_pSvmTransient); \
    } while (0)
#endif


/**
 * Gets the IEM exception flags for the specified SVM event.
 *
 * @returns The IEM exception flags.
 * @param   pEvent      Pointer to the SVM event.
 *
 * @remarks This function currently only constructs flags required for
 *          IEMEvaluateRecursiveXcpt and not the complete flags (e.g. error-code
 *          and CR2 aspects of an exception are not included).
 */
static uint32_t hmR0SvmGetIemXcptFlags(PCSVMEVENT pEvent)
{
    uint8_t const uEventType = pEvent->n.u3Type;
    uint32_t      fIemXcptFlags;
    switch (uEventType)
    {
        case SVM_EVENT_EXCEPTION:
            /*
             * Only INT3 and INTO instructions can raise #BP and #OF exceptions.
             * See AMD spec. Table 8-1. "Interrupt Vector Source and Cause".
             */
            if (pEvent->n.u8Vector == X86_XCPT_BP)
            {
                fIemXcptFlags = IEM_XCPT_FLAGS_T_SOFT_INT | IEM_XCPT_FLAGS_BP_INSTR;
                break;
            }
            if (pEvent->n.u8Vector == X86_XCPT_OF)
            {
                fIemXcptFlags = IEM_XCPT_FLAGS_T_SOFT_INT | IEM_XCPT_FLAGS_OF_INSTR;
                break;
            }
            /** @todo How do we distinguish ICEBP \#DB from the regular one? */
            RT_FALL_THRU();
        case SVM_EVENT_NMI:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_CPU_XCPT;
            break;

        case SVM_EVENT_EXTERNAL_IRQ:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_EXT_INT;
            break;

        case SVM_EVENT_SOFTWARE_INT:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_SOFT_INT;
            break;

        default:
            fIemXcptFlags = 0;
            AssertMsgFailed(("Unexpected event type! uEventType=%#x uVector=%#x", uEventType, pEvent->n.u8Vector));
            break;
    }
    return fIemXcptFlags;
}


/**
 * Handle a condition that occurred while delivering an event through the guest
 * IDT.
 *
 * @returns VBox status code (informational error codes included).
 * @retval VINF_SUCCESS if we should continue handling the \#VMEXIT.
 * @retval VINF_HM_DOUBLE_FAULT if a \#DF condition was detected and we ought to
 *         continue execution of the guest which will delivery the \#DF.
 * @retval VINF_EM_RESET if we detected a triple-fault condition.
 * @retval VERR_EM_GUEST_CPU_HANG if we detected a guest CPU hang.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pSvmTransient   Pointer to the SVM transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmCheckExitDueToEventDelivery(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    int rc = VINF_SUCCESS;
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_CR2);

    Log4(("EXITINTINFO: Pending vectoring event %#RX64 Valid=%RTbool ErrValid=%RTbool Err=%#RX32 Type=%u Vector=%u\n",
          pVmcb->ctrl.ExitIntInfo.u, !!pVmcb->ctrl.ExitIntInfo.n.u1Valid, !!pVmcb->ctrl.ExitIntInfo.n.u1ErrorCodeValid,
          pVmcb->ctrl.ExitIntInfo.n.u32ErrorCode, pVmcb->ctrl.ExitIntInfo.n.u3Type, pVmcb->ctrl.ExitIntInfo.n.u8Vector));

    /*
     * The EXITINTINFO (if valid) contains the prior exception (IDT vector) that was trying to
     * be delivered to the guest which caused a #VMEXIT which was intercepted (Exit vector).
     *
     * See AMD spec. 15.7.3 "EXITINFO Pseudo-Code".
     */
    if (pVmcb->ctrl.ExitIntInfo.n.u1Valid)
    {
        IEMXCPTRAISE     enmRaise;
        IEMXCPTRAISEINFO fRaiseInfo;
        bool const       fExitIsHwXcpt  = pSvmTransient->u64ExitCode - SVM_EXIT_XCPT_0 <= SVM_EXIT_XCPT_31;
        uint8_t const    uIdtVector     = pVmcb->ctrl.ExitIntInfo.n.u8Vector;
        if (fExitIsHwXcpt)
        {
            uint8_t  const uExitVector      = pSvmTransient->u64ExitCode - SVM_EXIT_XCPT_0;
            uint32_t const fIdtVectorFlags  = hmR0SvmGetIemXcptFlags(&pVmcb->ctrl.ExitIntInfo);
            uint32_t const fExitVectorFlags = IEM_XCPT_FLAGS_T_CPU_XCPT;
            enmRaise = IEMEvaluateRecursiveXcpt(pVCpu, fIdtVectorFlags, uIdtVector, fExitVectorFlags, uExitVector, &fRaiseInfo);
        }
        else
        {
            /*
             * If delivery of an event caused a #VMEXIT that is not an exception (e.g. #NPF)
             * then we end up here.
             *
             * If the event was:
             *   - a software interrupt, we can re-execute the instruction which will
             *     regenerate the event.
             *   - an NMI, we need to clear NMI blocking and re-inject the NMI.
             *   - a hardware exception or external interrupt, we re-inject it.
             */
            fRaiseInfo = IEMXCPTRAISEINFO_NONE;
            if (pVmcb->ctrl.ExitIntInfo.n.u3Type == SVM_EVENT_SOFTWARE_INT)
                enmRaise = IEMXCPTRAISE_REEXEC_INSTR;
            else
                enmRaise = IEMXCPTRAISE_PREV_EVENT;
        }

        switch (enmRaise)
        {
            case IEMXCPTRAISE_CURRENT_XCPT:
            case IEMXCPTRAISE_PREV_EVENT:
            {
                /* For software interrupts, we shall re-execute the instruction. */
                if (!(fRaiseInfo & IEMXCPTRAISEINFO_SOFT_INT_XCPT))
                {
                    RTGCUINTPTR GCPtrFaultAddress = 0;

                    /* If we are re-injecting an NMI, clear NMI blocking. */
                    if (pVmcb->ctrl.ExitIntInfo.n.u3Type == SVM_EVENT_NMI)
                        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);

                    /* Determine a vectoring #PF condition, see comment in hmR0SvmExitXcptPF(). */
                    if (fRaiseInfo & (IEMXCPTRAISEINFO_EXT_INT_PF | IEMXCPTRAISEINFO_NMI_PF))
                    {
                        pSvmTransient->fVectoringPF = true;
                        Log4Func(("IDT: Pending vectoring #PF due to delivery of Ext-Int/NMI. uCR2=%#RX64\n",
                                  pVCpu->cpum.GstCtx.cr2));
                    }
                    else if (   pVmcb->ctrl.ExitIntInfo.n.u3Type == SVM_EVENT_EXCEPTION
                             && uIdtVector == X86_XCPT_PF)
                    {
                        /*
                         * If the previous exception was a #PF, we need to recover the CR2 value.
                         * This can't happen with shadow paging.
                         */
                        GCPtrFaultAddress = pVCpu->cpum.GstCtx.cr2;
                    }

                    /*
                     * Without nested paging, when uExitVector is #PF, CR2 value will be updated from the VMCB's
                     * exit info. fields, if it's a guest #PF, see hmR0SvmExitXcptPF().
                     */
                    Assert(pVmcb->ctrl.ExitIntInfo.n.u3Type != SVM_EVENT_SOFTWARE_INT);
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingReflect);
                    hmR0SvmSetPendingEvent(pVCpu, &pVmcb->ctrl.ExitIntInfo, GCPtrFaultAddress);

                    Log4Func(("IDT: Pending vectoring event %#RX64 ErrValid=%RTbool Err=%#RX32 GCPtrFaultAddress=%#RX64\n",
                              pVmcb->ctrl.ExitIntInfo.u, RT_BOOL(pVmcb->ctrl.ExitIntInfo.n.u1ErrorCodeValid),
                              pVmcb->ctrl.ExitIntInfo.n.u32ErrorCode, GCPtrFaultAddress));
                }
                break;
            }

            case IEMXCPTRAISE_REEXEC_INSTR:
            {
                Assert(rc == VINF_SUCCESS);
                break;
            }

            case IEMXCPTRAISE_DOUBLE_FAULT:
            {
                /*
                 * Determing a vectoring double #PF condition. Used later, when PGM evaluates
                 * the second #PF as a guest #PF (and not a shadow #PF) and needs to be
                 * converted into a #DF.
                 */
                if (fRaiseInfo & IEMXCPTRAISEINFO_PF_PF)
                {
                    Log4Func(("IDT: Pending vectoring double #PF uCR2=%#RX64\n", pVCpu->cpum.GstCtx.cr2));
                    pSvmTransient->fVectoringDoublePF = true;
                    Assert(rc == VINF_SUCCESS);
                }
                else
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingReflect);
                    hmR0SvmSetPendingXcptDF(pVCpu);
                    rc = VINF_HM_DOUBLE_FAULT;
                }
                break;
            }

            case IEMXCPTRAISE_TRIPLE_FAULT:
            {
                rc = VINF_EM_RESET;
                break;
            }

            case IEMXCPTRAISE_CPU_HANG:
            {
                rc = VERR_EM_GUEST_CPU_HANG;
                break;
            }

            default:
                AssertMsgFailedBreakStmt(("Bogus enmRaise value: %d (%#x)\n", enmRaise, enmRaise), rc = VERR_SVM_IPE_2);
        }
    }
    Assert(rc == VINF_SUCCESS || rc == VINF_HM_DOUBLE_FAULT || rc == VINF_EM_RESET || rc == VERR_EM_GUEST_CPU_HANG);
    return rc;
}


/**
 * Advances the guest RIP by the number of bytes specified in @a cb.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cb          RIP increment value in bytes.
 */
DECLINLINE(void) hmR0SvmAdvanceRip(PVMCPU pVCpu, uint32_t cb)
{
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    pCtx->rip += cb;

    /* Update interrupt shadow. */
    if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && pCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
}


/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- #VMEXIT handlers -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

/** @name \#VMEXIT handlers.
 * @{
 */

/**
 * \#VMEXIT handler for external interrupts, NMIs, FPU assertion freeze and INIT
 * signals (SVM_EXIT_INTR, SVM_EXIT_NMI, SVM_EXIT_FERR_FREEZE, SVM_EXIT_INIT).
 */
HMSVM_EXIT_DECL hmR0SvmExitIntr(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    if (pSvmTransient->u64ExitCode == SVM_EXIT_NMI)
        STAM_REL_COUNTER_INC(&pVCpu->hm.s.StatExitHostNmiInGC);
    else if (pSvmTransient->u64ExitCode == SVM_EXIT_INTR)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitExtInt);

    /*
     * AMD-V has no preemption timer and the generic periodic preemption timer has no way to
     * signal -before- the timer fires if the current interrupt is our own timer or a some
     * other host interrupt. We also cannot examine what interrupt it is until the host
     * actually take the interrupt.
     *
     * Going back to executing guest code here unconditionally causes random scheduling
     * problems (observed on an AMD Phenom 9850 Quad-Core on Windows 64-bit host).
     */
    return VINF_EM_RAW_INTERRUPT;
}


/**
 * \#VMEXIT handler for WBINVD (SVM_EXIT_WBINVD). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitWbinvd(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedWbinvd(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for INVD (SVM_EXIT_INVD). Unconditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitInvd(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedInvd(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for INVD (SVM_EXIT_CPUID). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitCpuid(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX);
    VBOXSTRICTRC rcStrict;
    PCEMEXITREC pExitRec = EMHistoryUpdateFlagsAndTypeAndPC(pVCpu,
                                                            EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_CPUID),
                                                            pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base);
    if (!pExitRec)
    {
        bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
        if (fSupportsNextRipSave)
        {
            PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
            uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
            rcStrict = IEMExecDecodedCpuid(pVCpu, cbInstr);
        }
        else
        {
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
            rcStrict = IEMExecOne(pVCpu);
        }

        if (rcStrict == VINF_IEM_RAISED_XCPT)
        {
            rcStrict = VINF_SUCCESS;
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
        }
        HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    }
    else
    {
        /*
         * Frequent exit or something needing probing.  Get state and call EMHistoryExec.
         */
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);

        Log4(("CpuIdExit/%u: %04x:%08RX64: %#x/%#x -> EMHistoryExec\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, pVCpu->cpum.GstCtx.eax, pVCpu->cpum.GstCtx.ecx));

        rcStrict = EMHistoryExec(pVCpu, pExitRec, 0);

        Log4(("CpuIdExit/%u: %04x:%08RX64: EMHistoryExec -> %Rrc + %04x:%08RX64\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    }
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for RDTSC (SVM_EXIT_RDTSC). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitRdtsc(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_CR4);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedRdtsc(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_SUCCESS)
        pSvmTransient->fUpdateTscOffsetting = true;
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for RDTSCP (SVM_EXIT_RDTSCP). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitRdtscp(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_CR4 | CPUMCTX_EXTRN_TSC_AUX);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedRdtscp(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_SUCCESS)
        pSvmTransient->fUpdateTscOffsetting = true;
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for RDPMC (SVM_EXIT_RDPMC). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitRdpmc(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_CR4);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedRdpmc(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for INVLPG (SVM_EXIT_INVLPG). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitInvlpg(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    Assert(!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsDecodeAssists = hmR0SvmSupportsDecodeAssists(pVCpu);
    bool const fSupportsNextRipSave   = hmR0SvmSupportsNextRipSave(pVCpu);
    if (   fSupportsDecodeAssists
        && fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_MEM_MASK);
        PCSVMVMCB     pVmcb     = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr   = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        RTGCPTR const GCPtrPage = pVmcb->ctrl.u64ExitInfo1;
        rcStrict = IEMExecDecodedInvlpg(pVCpu, cbInstr, GCPtrPage);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_VAL(rcStrict);
}


/**
 * \#VMEXIT handler for HLT (SVM_EXIT_HLT). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitHlt(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedHlt(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (   rcStrict == VINF_EM_HALT
        || rcStrict == VINF_SUCCESS)
        rcStrict = EMShouldContinueAfterHalt(pVCpu, &pVCpu->cpum.GstCtx) ? VINF_SUCCESS : VINF_EM_HALT;
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitHlt);
    if (rcStrict != VINF_SUCCESS)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHltToR3);
    return VBOXSTRICTRC_VAL(rcStrict);;
}


/**
 * \#VMEXIT handler for MONITOR (SVM_EXIT_MONITOR). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitMonitor(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    /*
     * If the instruction length is supplied by the CPU is 3 bytes, we can be certain that no
     * segment override prefix is present (and thus use the default segment DS). Otherwise, a
     * segment override prefix or other prefixes might be used, in which case we fallback to
     * IEMExecOne() to figure out.
     */
    VBOXSTRICTRC  rcStrict;
    PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
    uint8_t const cbInstr = hmR0SvmSupportsNextRipSave(pVCpu) ? pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip : 0;
    if (cbInstr)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_MEM_MASK | CPUMCTX_EXTRN_DS);
        rcStrict = IEMExecDecodedMonitor(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMonitor);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for MWAIT (SVM_EXIT_MWAIT). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitMwait(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK);
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedMwait(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (   rcStrict == VINF_EM_HALT
        && EMMonitorWaitShouldContinue(pVCpu, &pVCpu->cpum.GstCtx))
        rcStrict = VINF_SUCCESS;
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMwait);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for shutdown (triple-fault) (SVM_EXIT_SHUTDOWN). Conditional
 * \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitShutdown(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    return VINF_EM_RESET;
}


/**
 * \#VMEXIT handler for unexpected exits. Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitUnexpected(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    AssertMsgFailed(("hmR0SvmExitUnexpected: ExitCode=%#RX64 uExitInfo1=%#RX64 uExitInfo2=%#RX64\n", pSvmTransient->u64ExitCode,
                     pVmcb->ctrl.u64ExitInfo1, pVmcb->ctrl.u64ExitInfo2));
    RT_NOREF(pVmcb);
    pVCpu->hm.s.u32HMError = (uint32_t)pSvmTransient->u64ExitCode;
    return VERR_SVM_UNEXPECTED_EXIT;
}


/**
 * \#VMEXIT handler for CRx reads (SVM_EXIT_READ_CR*). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitReadCRx(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    Log4Func(("CS:RIP=%04x:%#RX64\n", pCtx->cs.Sel, pCtx->rip));
#ifdef VBOX_WITH_STATISTICS
    switch (pSvmTransient->u64ExitCode)
    {
        case SVM_EXIT_READ_CR0: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR0Read); break;
        case SVM_EXIT_READ_CR2: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR2Read); break;
        case SVM_EXIT_READ_CR3: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR3Read); break;
        case SVM_EXIT_READ_CR4: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR4Read); break;
        case SVM_EXIT_READ_CR8: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR8Read); break;
    }
#endif

    bool const fSupportsDecodeAssists = hmR0SvmSupportsDecodeAssists(pVCpu);
    bool const fSupportsNextRipSave   = hmR0SvmSupportsNextRipSave(pVCpu);
    if (   fSupportsDecodeAssists
        && fSupportsNextRipSave)
    {
        PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
        bool const fMovCRx = RT_BOOL(pVmcb->ctrl.u64ExitInfo1 & SVM_EXIT1_MOV_CRX_MASK);
        if (fMovCRx)
        {
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_CR_MASK
                                            | CPUMCTX_EXTRN_APIC_TPR);
            uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pCtx->rip;
            uint8_t const iCrReg  = pSvmTransient->u64ExitCode - SVM_EXIT_READ_CR0;
            uint8_t const iGReg   = pVmcb->ctrl.u64ExitInfo1 & SVM_EXIT1_MOV_CRX_GPR_NUMBER;
            VBOXSTRICTRC rcStrict = IEMExecDecodedMovCRxRead(pVCpu, cbInstr, iGReg, iCrReg);
            HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
            return VBOXSTRICTRC_VAL(rcStrict);
        }
        /* else: SMSW instruction, fall back below to IEM for this. */
    }

    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    VBOXSTRICTRC rcStrict = IEMExecOne(pVCpu);
    AssertMsg(   rcStrict == VINF_SUCCESS
              || rcStrict == VINF_PGM_CHANGE_MODE
              || rcStrict == VINF_PGM_SYNC_CR3
              || rcStrict == VINF_IEM_RAISED_XCPT,
              ("hmR0SvmExitReadCRx: IEMExecOne failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    Assert((pSvmTransient->u64ExitCode - SVM_EXIT_READ_CR0) <= 15);
    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for CRx writes (SVM_EXIT_WRITE_CR*). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitWriteCRx(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    uint64_t const uExitCode = pSvmTransient->u64ExitCode;
    uint8_t  const iCrReg    = uExitCode == SVM_EXIT_CR0_SEL_WRITE ? 0 : (pSvmTransient->u64ExitCode - SVM_EXIT_WRITE_CR0);
    Assert(iCrReg <= 15);

    VBOXSTRICTRC rcStrict = VERR_SVM_IPE_5;
    PCPUMCTX     pCtx = &pVCpu->cpum.GstCtx;
    bool         fDecodedInstr = false;
    bool const   fSupportsDecodeAssists = hmR0SvmSupportsDecodeAssists(pVCpu);
    bool const   fSupportsNextRipSave   = hmR0SvmSupportsNextRipSave(pVCpu);
    if (   fSupportsDecodeAssists
        && fSupportsNextRipSave)
    {
        PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
        bool const fMovCRx = RT_BOOL(pVmcb->ctrl.u64ExitInfo1 & SVM_EXIT1_MOV_CRX_MASK);
        if (fMovCRx)
        {
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_MEM_MASK | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4
                                            | CPUMCTX_EXTRN_APIC_TPR);
            uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pCtx->rip;
            uint8_t const iGReg   = pVmcb->ctrl.u64ExitInfo1 & SVM_EXIT1_MOV_CRX_GPR_NUMBER;
            Log4Func(("Mov CR%u w/ iGReg=%#x\n", iCrReg, iGReg));
            rcStrict = IEMExecDecodedMovCRxWrite(pVCpu, cbInstr, iCrReg, iGReg);
            fDecodedInstr = true;
        }
        /* else: LMSW or CLTS instruction, fall back below to IEM for this. */
    }

    if (!fDecodedInstr)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        Log4Func(("iCrReg=%#x\n", iCrReg));
        rcStrict = IEMExecOne(pVCpu);
        if (RT_UNLIKELY(   rcStrict == VERR_IEM_ASPECT_NOT_IMPLEMENTED
                        || rcStrict == VERR_IEM_INSTR_NOT_IMPLEMENTED))
            rcStrict = VERR_EM_INTERPRETER;
    }

    if (rcStrict == VINF_SUCCESS)
    {
        switch (iCrReg)
        {
            case 0:
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR0);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR0Write);
                break;

            case 2:
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR2);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR2Write);
                break;

            case 3:
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR3);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR3Write);
                break;

            case 4:
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR4);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR4Write);
                break;

            case 8:
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR8Write);
                break;

            default:
            {
                AssertMsgFailed(("hmR0SvmExitWriteCRx: Invalid/Unexpected Write-CRx exit. u64ExitCode=%#RX64 %#x\n",
                                 pSvmTransient->u64ExitCode, iCrReg));
                break;
            }
        }
        HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
        HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    }
    else
        Assert(rcStrict == VERR_EM_INTERPRETER || rcStrict == VINF_PGM_CHANGE_MODE || rcStrict == VINF_PGM_SYNC_CR3);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT helper for read MSRs, see hmR0SvmExitMsr.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 */
static VBOXSTRICTRC hmR0SvmExitReadMsr(PVMCPU pVCpu, PSVMVMCB pVmcb)
{
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdmsr);
    Log4Func(("idMsr=%#RX32\n", pVCpu->cpum.GstCtx.ecx));

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        /** @todo Optimize this: Only retrieve the MSR bits we need here. CPUMAllMsrs.cpp
         *  can ask for what it needs instead of using CPUMCTX_EXTRN_ALL_MSRS. */
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_ALL_MSRS);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedRdmsr(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_ALL_MSRS);
        rcStrict = IEMExecOne(pVCpu);
    }

    AssertMsg(   rcStrict == VINF_SUCCESS
              || rcStrict == VINF_IEM_RAISED_XCPT
              || rcStrict == VINF_CPUM_R3_MSR_READ,
              ("hmR0SvmExitReadMsr: Unexpected status %Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return rcStrict;
}


/**
 * \#VMEXIT helper for write MSRs, see hmR0SvmExitMsr.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmcb       Pointer to the VM control block.
 * @param   pSvmTransient   Pointer to the SVM-transient structure.
 */
static VBOXSTRICTRC hmR0SvmExitWriteMsr(PVMCPU pVCpu, PSVMVMCB pVmcb, PSVMTRANSIENT pSvmTransient)
{
    PCPUMCTX pCtx  = &pVCpu->cpum.GstCtx;
    uint32_t const idMsr = pCtx->ecx;
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitWrmsr);
    Log4Func(("idMsr=%#RX32\n", idMsr));

    /*
     * Handle TPR patching MSR writes.
     * We utilitize the LSTAR MSR for patching.
     */
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (   pVCpu->CTX_SUFF(pVM)->hm.s.fTPRPatchingActive
        && idMsr == MSR_K8_LSTAR)
    {
        unsigned cbInstr;
        if (fSupportsNextRipSave)
            cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        else
        {
            PDISCPUSTATE pDis = &pVCpu->hm.s.DisState;
            int rc = EMInterpretDisasCurrent(pVCpu->CTX_SUFF(pVM), pVCpu, pDis, &cbInstr);
            if (   rc == VINF_SUCCESS
                && pDis->pCurInstr->uOpcode == OP_WRMSR)
                Assert(cbInstr > 0);
            else
                cbInstr = 0;
        }

        /* Our patch code uses LSTAR for TPR caching for 32-bit guests. */
        if ((pCtx->eax & 0xff) != pSvmTransient->u8GuestTpr)
        {
            int rc = APICSetTpr(pVCpu, pCtx->eax & 0xff);
            AssertRCReturn(rc, rc);
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
        }

        int rc = VINF_SUCCESS;
        hmR0SvmAdvanceRip(pVCpu, cbInstr);
        HMSVM_CHECK_SINGLE_STEP(pVCpu, rc);
        return rc;
    }

    /*
     * Handle regular MSR writes.
     */
    VBOXSTRICTRC rcStrict;
    if (fSupportsNextRipSave)
    {
        /** @todo Optimize this: We don't need to get much of the MSR state here
         * since we're only updating.  CPUMAllMsrs.cpp can ask for what it needs and
         * clear the applicable extern flags. */
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_ALL_MSRS);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedWrmsr(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_ALL_MSRS);
        rcStrict = IEMExecOne(pVCpu);
    }

    AssertMsg(   rcStrict == VINF_SUCCESS
              || rcStrict == VINF_IEM_RAISED_XCPT
              || rcStrict == VINF_CPUM_R3_MSR_WRITE,
              ("hmR0SvmExitWriteMsr: Unexpected status %Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

    if (rcStrict == VINF_SUCCESS)
    {
        /* If this is an X2APIC WRMSR access, update the APIC TPR state. */
        if (   idMsr >= MSR_IA32_X2APIC_START
            && idMsr <= MSR_IA32_X2APIC_END)
        {
            /*
             * We've already saved the APIC related guest-state (TPR) in hmR0SvmPostRunGuest().
             * When full APIC register virtualization is implemented we'll have to make sure
             * APIC state is saved from the VMCB before IEM changes it.
             */
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
        }
        else
        {
            switch (idMsr)
            {
                case MSR_IA32_TSC:          pSvmTransient->fUpdateTscOffsetting = true;                                     break;
                case MSR_K6_EFER:           ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_EFER_MSR);          break;
                case MSR_K8_FS_BASE:        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_FS);                break;
                case MSR_K8_GS_BASE:        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_GS);                break;
                case MSR_IA32_SYSENTER_CS:  ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_SYSENTER_CS_MSR);   break;
                case MSR_IA32_SYSENTER_EIP: ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_SYSENTER_EIP_MSR);  break;
                case MSR_IA32_SYSENTER_ESP: ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_SYSENTER_ESP_MSR);  break;
            }
        }
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return rcStrict;
}


/**
 * \#VMEXIT handler for MSR read and writes (SVM_EXIT_MSR). Conditional
 * \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitMsr(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    if (pVmcb->ctrl.u64ExitInfo1 == SVM_EXIT1_MSR_READ)
        return VBOXSTRICTRC_TODO(hmR0SvmExitReadMsr(pVCpu, pVmcb));

    Assert(pVmcb->ctrl.u64ExitInfo1 == SVM_EXIT1_MSR_WRITE);
    return VBOXSTRICTRC_TODO(hmR0SvmExitWriteMsr(pVCpu, pVmcb, pSvmTransient));
}


/**
 * \#VMEXIT handler for DRx read (SVM_EXIT_READ_DRx). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitReadDRx(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxRead);

    /** @todo Stepping with nested-guest. */
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (!CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
    {
        /* We should -not- get this #VMEXIT if the guest's debug registers were active. */
        if (pSvmTransient->fWasGuestDebugStateActive)
        {
            AssertMsgFailed(("hmR0SvmExitReadDRx: Unexpected exit %#RX32\n", (uint32_t)pSvmTransient->u64ExitCode));
            pVCpu->hm.s.u32HMError = (uint32_t)pSvmTransient->u64ExitCode;
            return VERR_SVM_UNEXPECTED_EXIT;
        }

        /*
         * Lazy DR0-3 loading.
         */
        if (!pSvmTransient->fWasHyperDebugStateActive)
        {
            Assert(!DBGFIsStepping(pVCpu)); Assert(!pVCpu->hm.s.fSingleInstruction);
            Log5(("hmR0SvmExitReadDRx: Lazy loading guest debug registers\n"));

            /* Don't intercept DRx read and writes. */
            PSVMVMCB pVmcb = pVCpu->hm.s.svm.pVmcb;
            pVmcb->ctrl.u16InterceptRdDRx = 0;
            pVmcb->ctrl.u16InterceptWrDRx = 0;
            pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;

            /* We're playing with the host CPU state here, make sure we don't preempt or longjmp. */
            VMMRZCallRing3Disable(pVCpu);
            HM_DISABLE_PREEMPT(pVCpu);

            /* Save the host & load the guest debug state, restart execution of the MOV DRx instruction. */
            CPUMR0LoadGuestDebugState(pVCpu, false /* include DR6 */);
            Assert(CPUMIsGuestDebugStateActive(pVCpu) || HC_ARCH_BITS == 32);

            HM_RESTORE_PREEMPT();
            VMMRZCallRing3Enable(pVCpu);

            STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxContextSwitch);
            return VINF_SUCCESS;
        }
    }

    /*
     * Interpret the read/writing of DRx.
     */
    /** @todo Decode assist.  */
    VBOXSTRICTRC rc = EMInterpretInstruction(pVCpu, CPUMCTX2CORE(pCtx), 0 /* pvFault */);
    Log5(("hmR0SvmExitReadDRx: Emulated DRx access: rc=%Rrc\n", VBOXSTRICTRC_VAL(rc)));
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        /* Not necessary for read accesses but whatever doesn't hurt for now, will be fixed with decode assist. */
        /** @todo CPUM should set this flag! */
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_DR_MASK);
        HMSVM_CHECK_SINGLE_STEP(pVCpu, rc);
    }
    else
        Assert(rc == VERR_EM_INTERPRETER);
    return VBOXSTRICTRC_TODO(rc);
}


/**
 * \#VMEXIT handler for DRx write (SVM_EXIT_WRITE_DRx). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitWriteDRx(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    /* For now it's the same since we interpret the instruction anyway. Will change when using of Decode Assist is implemented. */
    int rc = hmR0SvmExitReadDRx(pVCpu, pSvmTransient);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxWrite);
    STAM_COUNTER_DEC(&pVCpu->hm.s.StatExitDRxRead);
    return rc;
}


/**
 * \#VMEXIT handler for XCRx write (SVM_EXIT_XSETBV). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXsetbv(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);

    /** @todo decode assists... */
    VBOXSTRICTRC rcStrict = IEMExecOne(pVCpu);
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    {
        PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        pVCpu->hm.s.fLoadSaveGuestXcr0 = (pCtx->cr4 & X86_CR4_OSXSAVE) && pCtx->aXcr[0] != ASMGetXcr0();
        Log4Func(("New XCR0=%#RX64 fLoadSaveGuestXcr0=%RTbool (cr4=%#RX64)\n", pCtx->aXcr[0], pVCpu->hm.s.fLoadSaveGuestXcr0,
                  pCtx->cr4));
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for I/O instructions (SVM_EXIT_IOIO). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitIOInstr(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_SREG_MASK);

    /* I/O operation lookup arrays. */
    static uint32_t const s_aIOSize[8]  = { 0, 1, 2, 0, 4, 0, 0, 0 };                   /* Size of the I/O accesses in bytes. */
    static uint32_t const s_aIOOpAnd[8] = { 0, 0xff, 0xffff, 0, 0xffffffff, 0, 0, 0 };  /* AND masks for saving
                                                                                           the result (in AL/AX/EAX). */
    PVM      pVM   = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx  = &pVCpu->cpum.GstCtx;
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);

    Log4Func(("CS:RIP=%04x:%#RX64\n", pCtx->cs.Sel, pCtx->rip));

    /* Refer AMD spec. 15.10.2 "IN and OUT Behaviour" and Figure 15-2. "EXITINFO1 for IOIO Intercept" for the format. */
    SVMIOIOEXITINFO IoExitInfo;
    IoExitInfo.u       = (uint32_t)pVmcb->ctrl.u64ExitInfo1;
    uint32_t uIOWidth  = (IoExitInfo.u >> 4) & 0x7;
    uint32_t cbValue   = s_aIOSize[uIOWidth];
    uint32_t uAndVal   = s_aIOOpAnd[uIOWidth];

    if (RT_UNLIKELY(!cbValue))
    {
        AssertMsgFailed(("hmR0SvmExitIOInstr: Invalid IO operation. uIOWidth=%u\n", uIOWidth));
        return VERR_EM_INTERPRETER;
    }

    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS);
    VBOXSTRICTRC rcStrict;
    PCEMEXITREC pExitRec = NULL;
    if (   !pVCpu->hm.s.fSingleInstruction
        && !pVCpu->cpum.GstCtx.eflags.Bits.u1TF)
        pExitRec = EMHistoryUpdateFlagsAndTypeAndPC(pVCpu,
                                                    !IoExitInfo.n.u1Str
                                                    ? IoExitInfo.n.u1Type == SVM_IOIO_READ
                                                    ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_READ)
                                                    : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_WRITE)
                                                    : IoExitInfo.n.u1Type == SVM_IOIO_READ
                                                    ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_STR_READ)
                                                    : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_STR_WRITE),
                                                    pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base);
    if (!pExitRec)
    {
        bool fUpdateRipAlready = false;
        if (IoExitInfo.n.u1Str)
        {
            /* INS/OUTS - I/O String instruction. */
            /** @todo Huh? why can't we use the segment prefix information given by AMD-V
             *        in EXITINFO1? Investigate once this thing is up and running. */
            Log4Func(("CS:RIP=%04x:%08RX64 %#06x/%u %c str\n", pCtx->cs.Sel, pCtx->rip, IoExitInfo.n.u16Port, cbValue,
                      IoExitInfo.n.u1Type == SVM_IOIO_WRITE ? 'w' : 'r'));
            AssertReturn(pCtx->dx == IoExitInfo.n.u16Port, VERR_SVM_IPE_2);
            static IEMMODE const s_aenmAddrMode[8] =
            {
                (IEMMODE)-1, IEMMODE_16BIT, IEMMODE_32BIT, (IEMMODE)-1, IEMMODE_64BIT, (IEMMODE)-1, (IEMMODE)-1, (IEMMODE)-1
            };
            IEMMODE enmAddrMode = s_aenmAddrMode[(IoExitInfo.u >> 7) & 0x7];
            if (enmAddrMode != (IEMMODE)-1)
            {
                uint64_t cbInstr = pVmcb->ctrl.u64ExitInfo2 - pCtx->rip;
                if (cbInstr <= 15 && cbInstr >= 1)
                {
                    Assert(cbInstr >= 1U + IoExitInfo.n.u1Rep);
                    if (IoExitInfo.n.u1Type == SVM_IOIO_WRITE)
                    {
                        /* Don't know exactly how to detect whether u3Seg is valid, currently
                           only enabling it for Bulldozer and later with NRIP.  OS/2 broke on
                           2384 Opterons when only checking NRIP. */
                        bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
                        if (   fSupportsNextRipSave
                            && pVM->cpum.ro.GuestFeatures.enmMicroarch >= kCpumMicroarch_AMD_15h_First)
                        {
                            AssertMsg(IoExitInfo.n.u3Seg == X86_SREG_DS || cbInstr > 1U + IoExitInfo.n.u1Rep,
                                      ("u32Seg=%d cbInstr=%d u1REP=%d", IoExitInfo.n.u3Seg, cbInstr, IoExitInfo.n.u1Rep));
                            rcStrict = IEMExecStringIoWrite(pVCpu, cbValue, enmAddrMode, IoExitInfo.n.u1Rep, (uint8_t)cbInstr,
                                                            IoExitInfo.n.u3Seg, true /*fIoChecked*/);
                        }
                        else if (cbInstr == 1U + IoExitInfo.n.u1Rep)
                            rcStrict = IEMExecStringIoWrite(pVCpu, cbValue, enmAddrMode, IoExitInfo.n.u1Rep, (uint8_t)cbInstr,
                                                            X86_SREG_DS, true /*fIoChecked*/);
                        else
                            rcStrict = IEMExecOne(pVCpu);
                        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOStringWrite);
                    }
                    else
                    {
                        AssertMsg(IoExitInfo.n.u3Seg == X86_SREG_ES /*=0*/, ("%#x\n", IoExitInfo.n.u3Seg));
                        rcStrict = IEMExecStringIoRead(pVCpu, cbValue, enmAddrMode, IoExitInfo.n.u1Rep, (uint8_t)cbInstr,
                                                       true /*fIoChecked*/);
                        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOStringRead);
                    }
                }
                else
                {
                    AssertMsgFailed(("rip=%RX64 nrip=%#RX64 cbInstr=%#RX64\n", pCtx->rip, pVmcb->ctrl.u64ExitInfo2, cbInstr));
                    rcStrict = IEMExecOne(pVCpu);
                }
            }
            else
            {
                AssertMsgFailed(("IoExitInfo=%RX64\n", IoExitInfo.u));
                rcStrict = IEMExecOne(pVCpu);
            }
            fUpdateRipAlready = true;
        }
        else
        {
            /* IN/OUT - I/O instruction. */
            Assert(!IoExitInfo.n.u1Rep);

            if (IoExitInfo.n.u1Type == SVM_IOIO_WRITE)
            {
                rcStrict = IOMIOPortWrite(pVM, pVCpu, IoExitInfo.n.u16Port, pCtx->eax & uAndVal, cbValue);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOWrite);
            }
            else
            {
                uint32_t u32Val = 0;
                rcStrict = IOMIOPortRead(pVM, pVCpu, IoExitInfo.n.u16Port, &u32Val, cbValue);
                if (IOM_SUCCESS(rcStrict))
                {
                    /* Save result of I/O IN instr. in AL/AX/EAX. */
                    /** @todo r=bird: 32-bit op size should clear high bits of rax! */
                    pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Val & uAndVal);
                }
                else if (rcStrict == VINF_IOM_R3_IOPORT_READ)
                {
                    HMR0SavePendingIOPortRead(pVCpu, pVCpu->cpum.GstCtx.rip, pVmcb->ctrl.u64ExitInfo2, IoExitInfo.n.u16Port,
                                              uAndVal, cbValue);
                }

                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIORead);
            }
        }

        if (IOM_SUCCESS(rcStrict))
        {
            /* AMD-V saves the RIP of the instruction following the IO instruction in EXITINFO2. */
            if (!fUpdateRipAlready)
                pCtx->rip = pVmcb->ctrl.u64ExitInfo2;

            /*
             * If any I/O breakpoints are armed, we need to check if one triggered
             * and take appropriate action.
             * Note that the I/O breakpoint type is undefined if CR4.DE is 0.
             */
            /** @todo Optimize away the DBGFBpIsHwIoArmed call by having DBGF tell the
             *  execution engines about whether hyper BPs and such are pending. */
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_DR7);
            uint32_t const uDr7 = pCtx->dr[7];
            if (RT_UNLIKELY(   (   (uDr7 & X86_DR7_ENABLED_MASK)
                                && X86_DR7_ANY_RW_IO(uDr7)
                                && (pCtx->cr4 & X86_CR4_DE))
                            || DBGFBpIsHwIoArmed(pVM)))
            {
                /* We're playing with the host CPU state here, make sure we don't preempt or longjmp. */
                VMMRZCallRing3Disable(pVCpu);
                HM_DISABLE_PREEMPT(pVCpu);

                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxIoCheck);
                CPUMR0DebugStateMaybeSaveGuest(pVCpu, false /*fDr6*/);

                VBOXSTRICTRC rcStrict2 = DBGFBpCheckIo(pVM, pVCpu, &pVCpu->cpum.GstCtx, IoExitInfo.n.u16Port, cbValue);
                if (rcStrict2 == VINF_EM_RAW_GUEST_TRAP)
                {
                    /* Raise #DB. */
                    pVmcb->guest.u64DR6 = pCtx->dr[6];
                    pVmcb->guest.u64DR7 = pCtx->dr[7];
                    pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DRX;
                    hmR0SvmSetPendingXcptDB(pVCpu);
                }
                /* rcStrict is VINF_SUCCESS, VINF_IOM_R3_IOPORT_COMMIT_WRITE, or in [VINF_EM_FIRST..VINF_EM_LAST],
                   however we can ditch VINF_IOM_R3_IOPORT_COMMIT_WRITE as it has VMCPU_FF_IOM as backup. */
                else if (   rcStrict2 != VINF_SUCCESS
                         && (rcStrict == VINF_SUCCESS || rcStrict2 < rcStrict))
                    rcStrict = rcStrict2;
                AssertCompile(VINF_EM_LAST < VINF_IOM_R3_IOPORT_COMMIT_WRITE);

                HM_RESTORE_PREEMPT();
                VMMRZCallRing3Enable(pVCpu);
            }

            HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
        }

#ifdef VBOX_STRICT
        if (rcStrict == VINF_IOM_R3_IOPORT_READ)
            Assert(IoExitInfo.n.u1Type == SVM_IOIO_READ);
        else if (rcStrict == VINF_IOM_R3_IOPORT_WRITE || rcStrict == VINF_IOM_R3_IOPORT_COMMIT_WRITE)
            Assert(IoExitInfo.n.u1Type == SVM_IOIO_WRITE);
        else
        {
            /** @todo r=bird: This is missing a bunch of VINF_EM_FIRST..VINF_EM_LAST
             *        statuses, that the VMM device and some others may return. See
             *        IOM_SUCCESS() for guidance. */
            AssertMsg(   RT_FAILURE(rcStrict)
                      || rcStrict == VINF_SUCCESS
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR
                      || rcStrict == VINF_EM_DBG_BREAKPOINT
                      || rcStrict == VINF_EM_RAW_GUEST_TRAP
                      || rcStrict == VINF_EM_RAW_TO_R3
                      || rcStrict == VINF_TRPM_XCPT_DISPATCHED
                      || rcStrict == VINF_EM_TRIPLE_FAULT, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
        }
#endif
    }
    else
    {
        /*
         * Frequent exit or something needing probing.  Get state and call EMHistoryExec.
         */
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
        STAM_COUNTER_INC(!IoExitInfo.n.u1Str
                         ? IoExitInfo.n.u1Type == SVM_IOIO_WRITE ? &pVCpu->hm.s.StatExitIOWrite : &pVCpu->hm.s.StatExitIORead
                         : IoExitInfo.n.u1Type == SVM_IOIO_WRITE ? &pVCpu->hm.s.StatExitIOStringWrite : &pVCpu->hm.s.StatExitIOStringRead);
        Log4(("IOExit/%u: %04x:%08RX64: %s%s%s %#x LB %u -> EMHistoryExec\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, IoExitInfo.n.u1Rep ? "REP " : "",
              IoExitInfo.n.u1Type == SVM_IOIO_WRITE ? "OUT" : "IN", IoExitInfo.n.u1Str ? "S" : "", IoExitInfo.n.u16Port, uIOWidth));

        rcStrict = EMHistoryExec(pVCpu, pExitRec, 0);
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);

        Log4(("IOExit/%u: %04x:%08RX64: EMHistoryExec -> %Rrc + %04x:%08RX64\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    }
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for Nested Page-faults (SVM_EXIT_NPF). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitNestedPF(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    Assert(pVM->hm.s.fNestedPaging);

    /* See AMD spec. 15.25.6 "Nested versus Guest Page Faults, Fault Ordering" for VMCB details for #NPF. */
    PSVMVMCB pVmcb           = hmR0SvmGetCurrentVmcb(pVCpu);
    RTGCPHYS GCPhysFaultAddr = pVmcb->ctrl.u64ExitInfo2;
    uint32_t u32ErrCode      = pVmcb->ctrl.u64ExitInfo1;    /* Note! High bits in EXITINFO1 may contain additional info and are
                                                               thus intentionally not copied into u32ErrCode. */

    Log4Func(("#NPF at CS:RIP=%04x:%#RX64 GCPhysFaultAddr=%RGp ErrCode=%#x \n", pCtx->cs.Sel, pCtx->rip, GCPhysFaultAddr,
              u32ErrCode));

    /*
     * TPR patching for 32-bit guests, using the reserved bit in the page tables for MMIO regions.
     */
    if (   pVM->hm.s.fTprPatchingAllowed
        && (GCPhysFaultAddr & PAGE_OFFSET_MASK) == XAPIC_OFF_TPR
        && (   !(u32ErrCode & X86_TRAP_PF_P)                                                             /* Not present */
            || (u32ErrCode & (X86_TRAP_PF_P | X86_TRAP_PF_RSVD)) == (X86_TRAP_PF_P | X86_TRAP_PF_RSVD))  /* MMIO page. */
        && !CPUMIsGuestInSvmNestedHwVirtMode(pCtx)
        && !CPUMIsGuestInLongModeEx(pCtx)
        && !CPUMGetGuestCPL(pVCpu)
        && pVM->hm.s.cPatches < RT_ELEMENTS(pVM->hm.s.aPatches))
    {
        RTGCPHYS GCPhysApicBase = APICGetBaseMsrNoCheck(pVCpu);
        GCPhysApicBase &= PAGE_BASE_GC_MASK;

        if (GCPhysFaultAddr == GCPhysApicBase + XAPIC_OFF_TPR)
        {
            /* Only attempt to patch the instruction once. */
            PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
            if (!pPatch)
                return VINF_EM_HM_PATCH_TPR_INSTR;
        }
    }

    /*
     * Determine the nested paging mode.
     */
    PGMMODE enmNestedPagingMode;
#if HC_ARCH_BITS == 32
    if (CPUMIsGuestInLongModeEx(pCtx))
        enmNestedPagingMode = PGMMODE_AMD64_NX;
    else
#endif
        enmNestedPagingMode = PGMGetHostMode(pVM);

    /*
     * MMIO optimization using the reserved (RSVD) bit in the guest page tables for MMIO pages.
     */
    Assert((u32ErrCode & (X86_TRAP_PF_RSVD | X86_TRAP_PF_P)) != X86_TRAP_PF_RSVD);
    if ((u32ErrCode & (X86_TRAP_PF_RSVD | X86_TRAP_PF_P)) == (X86_TRAP_PF_RSVD | X86_TRAP_PF_P))
    {
        /*
         * If event delivery causes an MMIO #NPF, go back to instruction emulation as otherwise
         * injecting the original pending event would most likely cause the same MMIO #NPF.
         */
        if (pVCpu->hm.s.Event.fPending)
            return VINF_EM_RAW_INJECT_TRPM_EVENT;

        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP);
        VBOXSTRICTRC rcStrict;
        PCEMEXITREC pExitRec = EMHistoryUpdateFlagsAndTypeAndPC(pVCpu,
                                                                EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_MMIO),
                                                                pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base);
        if (!pExitRec)
        {

            rcStrict = PGMR0Trap0eHandlerNPMisconfig(pVM, pVCpu, enmNestedPagingMode, CPUMCTX2CORE(pCtx), GCPhysFaultAddr,
                                                     u32ErrCode);

            /*
             * If we succeed, resume guest execution.
             *
             * If we fail in interpreting the instruction because we couldn't get the guest
             * physical address of the page containing the instruction via the guest's page
             * tables (we would invalidate the guest page in the host TLB), resume execution
             * which would cause a guest page fault to let the guest handle this weird case.
             *
             * See @bugref{6043}.
             */
            if (   rcStrict == VINF_SUCCESS
                || rcStrict == VERR_PAGE_TABLE_NOT_PRESENT
                || rcStrict == VERR_PAGE_NOT_PRESENT)
            {
                /* Successfully handled MMIO operation. */
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
                rcStrict = VINF_SUCCESS;
            }
        }
        else
        {
            /*
             * Frequent exit or something needing probing.  Get state and call EMHistoryExec.
             */
            Assert(pCtx == &pVCpu->cpum.GstCtx);
            HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
            Log4(("EptMisscfgExit/%u: %04x:%08RX64: %RGp -> EMHistoryExec\n",
                  pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, GCPhysFaultAddr));

            rcStrict = EMHistoryExec(pVCpu, pExitRec, 0);
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);

            Log4(("EptMisscfgExit/%u: %04x:%08RX64: EMHistoryExec -> %Rrc + %04x:%08RX64\n",
                  pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
                  VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
        }
        return VBOXSTRICTRC_TODO(rcStrict);
    }

    TRPMAssertXcptPF(pVCpu, GCPhysFaultAddr, u32ErrCode);
    int rc = PGMR0Trap0eHandlerNestedPaging(pVM, pVCpu, enmNestedPagingMode, u32ErrCode, CPUMCTX2CORE(pCtx), GCPhysFaultAddr);
    TRPMResetTrap(pVCpu);

    Log4Func(("#NPF: PGMR0Trap0eHandlerNestedPaging returns %Rrc CS:RIP=%04x:%#RX64\n", rc, pCtx->cs.Sel, pCtx->rip));

    /*
     * Same case as PGMR0Trap0eHandlerNPMisconfig(). See comment above, @bugref{6043}.
     */
    if (   rc == VINF_SUCCESS
        || rc == VERR_PAGE_TABLE_NOT_PRESENT
        || rc == VERR_PAGE_NOT_PRESENT)
    {
        /* We've successfully synced our shadow page tables. */
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPF);
        rc = VINF_SUCCESS;
    }

    return rc;
}


/**
 * \#VMEXIT handler for virtual interrupt (SVM_EXIT_VINTR). Conditional
 * \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitVIntr(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_ASSERT_NOT_IN_NESTED_GUEST(&pVCpu->cpum.GstCtx);

    /* Indicate that we no longer need to #VMEXIT when the guest is ready to receive NMIs, it is now ready. */
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    hmR0SvmClearIntWindowExiting(pVCpu, pVmcb);

    /* Deliver the pending interrupt via hmR0SvmEvaluatePendingEvent() and resume guest execution. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIntWindow);
    return VINF_SUCCESS;
}


/**
 * \#VMEXIT handler for task switches (SVM_EXIT_TASK_SWITCH). Conditional
 * \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitTaskSwitch(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

#ifndef HMSVM_ALWAYS_TRAP_TASK_SWITCH
    Assert(!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);
#endif

    /* Check if this task-switch occurred while delivering an event through the guest IDT. */
    if (pVCpu->hm.s.Event.fPending)  /* Can happen with exceptions/NMI. See @bugref{8411}. */
    {
        /*
         * AMD-V provides us with the exception which caused the TS; we collect
         * the information in the call to hmR0SvmCheckExitDueToEventDelivery().
         */
        Log4Func(("TS occurred during event delivery\n"));
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitTaskSwitch);
        return VINF_EM_RAW_INJECT_TRPM_EVENT;
    }

    /** @todo Emulate task switch someday, currently just going back to ring-3 for
     *        emulation. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitTaskSwitch);
    return VERR_EM_INTERPRETER;
}


/**
 * \#VMEXIT handler for VMMCALL (SVM_EXIT_VMMCALL). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitVmmCall(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

    if (pVCpu->CTX_SUFF(pVM)->hm.s.fTprPatchingAllowed)
    {
        int rc = hmSvmEmulateMovTpr(pVCpu);
        if (rc != VERR_NOT_FOUND)
        {
            Log4Func(("hmSvmEmulateMovTpr returns %Rrc\n", rc));
            return rc;
        }
    }

    if (EMAreHypercallInstructionsEnabled(pVCpu))
    {
        unsigned cbInstr;
        if (hmR0SvmSupportsNextRipSave(pVCpu))
        {
            PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
            cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        }
        else
        {
            PDISCPUSTATE pDis = &pVCpu->hm.s.DisState;
            int rc = EMInterpretDisasCurrent(pVCpu->CTX_SUFF(pVM), pVCpu, pDis, &cbInstr);
            if (   rc == VINF_SUCCESS
                && pDis->pCurInstr->uOpcode == OP_VMMCALL)
                Assert(cbInstr > 0);
            else
                cbInstr = 0;
        }

        VBOXSTRICTRC rcStrict = GIMHypercall(pVCpu, &pVCpu->cpum.GstCtx);
        if (RT_SUCCESS(rcStrict))
        {
            /* Only update the RIP if we're continuing guest execution and not in the case
               of say VINF_GIM_R3_HYPERCALL. */
            if (rcStrict == VINF_SUCCESS)
                hmR0SvmAdvanceRip(pVCpu, cbInstr);

            return VBOXSTRICTRC_VAL(rcStrict);
        }
        else
            Log4Func(("GIMHypercall returns %Rrc -> #UD\n", VBOXSTRICTRC_VAL(rcStrict)));
    }

    hmR0SvmSetPendingXcptUD(pVCpu);
    return VINF_SUCCESS;
}


/**
 * \#VMEXIT handler for VMMCALL (SVM_EXIT_VMMCALL). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitPause(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    unsigned cbInstr;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
        cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
    }
    else
    {
        PDISCPUSTATE pDis = &pVCpu->hm.s.DisState;
        int rc = EMInterpretDisasCurrent(pVCpu->CTX_SUFF(pVM), pVCpu, pDis, &cbInstr);
        if (   rc == VINF_SUCCESS
            && pDis->pCurInstr->uOpcode == OP_PAUSE)
            Assert(cbInstr > 0);
        else
            cbInstr = 0;
    }

    /** @todo The guest has likely hit a contended spinlock. We might want to
     *        poke a schedule different guest VCPU. */
    hmR0SvmAdvanceRip(pVCpu, cbInstr);
    return VINF_EM_RAW_INTERRUPT;
}


/**
 * \#VMEXIT handler for FERR intercept (SVM_EXIT_FERR_FREEZE). Conditional
 * \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitFerrFreeze(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, CPUMCTX_EXTRN_CR0);
    Assert(!(pVCpu->cpum.GstCtx.cr0 & X86_CR0_NE));

    Log4Func(("Raising IRQ 13 in response to #FERR\n"));
    return PDMIsaSetIrq(pVCpu->CTX_SUFF(pVM), 13 /* u8Irq */, 1 /* u8Level */, 0 /* uTagSrc */);
}


/**
 * \#VMEXIT handler for IRET (SVM_EXIT_IRET). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitIret(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    /* Clear NMI blocking. */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);

    /* Indicate that we no longer need to #VMEXIT when the guest is ready to receive NMIs, it is now ready. */
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    hmR0SvmClearCtrlIntercept(pVCpu, pVmcb, SVM_CTRL_INTERCEPT_IRET);

    /* Deliver the pending NMI via hmR0SvmEvaluatePendingEvent() and resume guest execution. */
    return VINF_SUCCESS;
}


/**
 * \#VMEXIT handler for page-fault exceptions (SVM_EXIT_XCPT_14).
 * Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptPF(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    /* See AMD spec. 15.12.15 "#PF (Page Fault)". */
    PVM             pVM           = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX        pCtx          = &pVCpu->cpum.GstCtx;
    PSVMVMCB        pVmcb         = hmR0SvmGetCurrentVmcb(pVCpu);
    uint32_t        uErrCode      = pVmcb->ctrl.u64ExitInfo1;
    uint64_t const  uFaultAddress = pVmcb->ctrl.u64ExitInfo2;

#if defined(HMSVM_ALWAYS_TRAP_ALL_XCPTS) || defined(HMSVM_ALWAYS_TRAP_PF)
    if (pVM->hm.s.fNestedPaging)
    {
        pVCpu->hm.s.Event.fPending = false;     /* In case it's a contributory or vectoring #PF. */
        if (   !pSvmTransient->fVectoringDoublePF
            || CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
        {
            /* A genuine guest #PF, reflect it to the guest. */
            hmR0SvmSetPendingXcptPF(pVCpu, uErrCode, uFaultAddress);
            Log4Func(("#PF: Guest page fault at %04X:%RGv FaultAddr=%RX64 ErrCode=%#x\n", pCtx->cs.Sel, (RTGCPTR)pCtx->rip,
                      uFaultAddress, uErrCode));
        }
        else
        {
            /* A guest page-fault occurred during delivery of a page-fault. Inject #DF. */
            hmR0SvmSetPendingXcptDF(pVCpu);
            Log4Func(("Pending #DF due to vectoring #PF. NP\n"));
        }
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestPF);
        return VINF_SUCCESS;
    }
#endif

    Assert(!pVM->hm.s.fNestedPaging);

    /*
     * TPR patching shortcut for APIC TPR reads and writes; only applicable to 32-bit guests.
     */
    if (   pVM->hm.s.fTprPatchingAllowed
        && (uFaultAddress & 0xfff) == XAPIC_OFF_TPR
        && !(uErrCode & X86_TRAP_PF_P)                /* Not present. */
        && !CPUMIsGuestInSvmNestedHwVirtMode(pCtx)
        && !CPUMIsGuestInLongModeEx(pCtx)
        && !CPUMGetGuestCPL(pVCpu)
        && pVM->hm.s.cPatches < RT_ELEMENTS(pVM->hm.s.aPatches))
    {
        RTGCPHYS GCPhysApicBase;
        GCPhysApicBase  = APICGetBaseMsrNoCheck(pVCpu);
        GCPhysApicBase &= PAGE_BASE_GC_MASK;

        /* Check if the page at the fault-address is the APIC base. */
        RTGCPHYS GCPhysPage;
        int rc2 = PGMGstGetPage(pVCpu, (RTGCPTR)uFaultAddress, NULL /* pfFlags */, &GCPhysPage);
        if (   rc2 == VINF_SUCCESS
            && GCPhysPage == GCPhysApicBase)
        {
            /* Only attempt to patch the instruction once. */
            PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
            if (!pPatch)
                return VINF_EM_HM_PATCH_TPR_INSTR;
        }
    }

    Log4Func(("#PF: uFaultAddress=%#RX64 CS:RIP=%#04x:%#RX64 uErrCode %#RX32 cr3=%#RX64\n", uFaultAddress, pCtx->cs.Sel,
              pCtx->rip, uErrCode, pCtx->cr3));

    /*
     * If it's a vectoring #PF, emulate injecting the original event injection as
     * PGMTrap0eHandler() is incapable of differentiating between instruction emulation and
     * event injection that caused a #PF. See @bugref{6607}.
     */
    if (pSvmTransient->fVectoringPF)
    {
        Assert(pVCpu->hm.s.Event.fPending);
        return VINF_EM_RAW_INJECT_TRPM_EVENT;
    }

    TRPMAssertXcptPF(pVCpu, uFaultAddress, uErrCode);
    int rc = PGMTrap0eHandler(pVCpu, uErrCode, CPUMCTX2CORE(pCtx), (RTGCPTR)uFaultAddress);

    Log4Func(("#PF: rc=%Rrc\n", rc));

    if (rc == VINF_SUCCESS)
    {
        /* Successfully synced shadow pages tables or emulated an MMIO instruction. */
        TRPMResetTrap(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPF);
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
        return rc;
    }

    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        pVCpu->hm.s.Event.fPending = false;     /* In case it's a contributory or vectoring #PF. */

        /*
         * If a nested-guest delivers a #PF and that causes a #PF which is -not- a shadow #PF,
         * we should simply forward the #PF to the guest and is up to the nested-hypervisor to
         * determine whether it is a nested-shadow #PF or a #DF, see @bugref{7243#c121}.
         */
        if (  !pSvmTransient->fVectoringDoublePF
            || CPUMIsGuestInSvmNestedHwVirtMode(pCtx))
        {
            /* It's a guest (or nested-guest) page fault and needs to be reflected. */
            uErrCode = TRPMGetErrorCode(pVCpu);        /* The error code might have been changed. */
            TRPMResetTrap(pVCpu);

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
            /* If the nested-guest is intercepting #PFs, cause a #PF #VMEXIT. */
            if (   CPUMIsGuestInSvmNestedHwVirtMode(pCtx)
                && HMIsGuestSvmXcptInterceptSet(pVCpu, X86_XCPT_PF))
                return VBOXSTRICTRC_TODO(IEMExecSvmVmexit(pVCpu, SVM_EXIT_XCPT_PF, uErrCode, uFaultAddress));
#endif

            hmR0SvmSetPendingXcptPF(pVCpu, uErrCode, uFaultAddress);
        }
        else
        {
            /* A guest page-fault occurred during delivery of a page-fault. Inject #DF. */
            TRPMResetTrap(pVCpu);
            hmR0SvmSetPendingXcptDF(pVCpu);
            Log4Func(("#PF: Pending #DF due to vectoring #PF\n"));
        }

        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestPF);
        return VINF_SUCCESS;
    }

    TRPMResetTrap(pVCpu);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPFEM);
    return rc;
}


/**
 * \#VMEXIT handler for undefined opcode (SVM_EXIT_XCPT_6).
 * Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptUD(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_ASSERT_NOT_IN_NESTED_GUEST(&pVCpu->cpum.GstCtx);

    /* Paranoia; Ensure we cannot be called as a result of event delivery. */
    PSVMVMCB pVmcb = pVCpu->hm.s.svm.pVmcb;
    Assert(!pVmcb->ctrl.ExitIntInfo.n.u1Valid);  NOREF(pVmcb);

    int rc = VERR_SVM_UNEXPECTED_XCPT_EXIT;
    if (pVCpu->hm.s.fGIMTrapXcptUD)
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
        uint8_t cbInstr = 0;
        VBOXSTRICTRC rcStrict = GIMXcptUD(pVCpu, &pVCpu->cpum.GstCtx, NULL /* pDis */, &cbInstr);
        if (rcStrict == VINF_SUCCESS)
        {
            /* #UD #VMEXIT does not have valid NRIP information, manually advance RIP. See @bugref{7270#c170}. */
            hmR0SvmAdvanceRip(pVCpu, cbInstr);
            rc = VINF_SUCCESS;
            HMSVM_CHECK_SINGLE_STEP(pVCpu, rc);
        }
        else if (rcStrict == VINF_GIM_HYPERCALL_CONTINUING)
            rc = VINF_SUCCESS;
        else if (rcStrict == VINF_GIM_R3_HYPERCALL)
            rc = VINF_GIM_R3_HYPERCALL;
        else
            Assert(RT_FAILURE(VBOXSTRICTRC_VAL(rcStrict)));
    }

    /* If the GIM #UD exception handler didn't succeed for some reason or wasn't needed, raise #UD. */
    if (RT_FAILURE(rc))
    {
        hmR0SvmSetPendingXcptUD(pVCpu);
        rc = VINF_SUCCESS;
    }

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestUD);
    return rc;
}


/**
 * \#VMEXIT handler for math-fault exceptions (SVM_EXIT_XCPT_16).
 * Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptMF(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

    PCPUMCTX pCtx  = &pVCpu->cpum.GstCtx;
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);

    /* Paranoia; Ensure we cannot be called as a result of event delivery. */
    Assert(!pVmcb->ctrl.ExitIntInfo.n.u1Valid); NOREF(pVmcb);

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestMF);

    if (!(pCtx->cr0 & X86_CR0_NE))
    {
        PVM       pVM  = pVCpu->CTX_SUFF(pVM);
        PDISSTATE pDis = &pVCpu->hm.s.DisState;
        unsigned  cbInstr;
        int rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbInstr);
        if (RT_SUCCESS(rc))
        {
            /* Convert a #MF into a FERR -> IRQ 13. See @bugref{6117}. */
            rc = PDMIsaSetIrq(pVCpu->CTX_SUFF(pVM), 13 /* u8Irq */, 1 /* u8Level */, 0 /* uTagSrc */);
            if (RT_SUCCESS(rc))
                hmR0SvmAdvanceRip(pVCpu, cbInstr);
        }
        else
            Log4Func(("EMInterpretDisasCurrent returned %Rrc uOpCode=%#x\n", rc, pDis->pCurInstr->uOpcode));
        return rc;
    }

    hmR0SvmSetPendingXcptMF(pVCpu);
    return VINF_SUCCESS;
}


/**
 * \#VMEXIT handler for debug exceptions (SVM_EXIT_XCPT_1). Conditional
 * \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptDB(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    if (RT_UNLIKELY(pVCpu->hm.s.Event.fPending))
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingInterpret);
        return VINF_EM_RAW_INJECT_TRPM_EVENT;
    }

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestDB);

    /*
     * This can be a fault-type #DB (instruction breakpoint) or a trap-type #DB (data
     * breakpoint). However, for both cases DR6 and DR7 are updated to what the exception
     * handler expects. See AMD spec. 15.12.2 "#DB (Debug)".
     */
    PVM      pVM   = pVCpu->CTX_SUFF(pVM);
    PSVMVMCB pVmcb = pVCpu->hm.s.svm.pVmcb;
    PCPUMCTX pCtx  = &pVCpu->cpum.GstCtx;
    int rc = DBGFRZTrap01Handler(pVM, pVCpu, CPUMCTX2CORE(pCtx), pVmcb->guest.u64DR6, pVCpu->hm.s.fSingleInstruction);
    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        Log5(("hmR0SvmExitXcptDB: DR6=%#RX64 -> guest trap\n", pVmcb->guest.u64DR6));
        if (CPUMIsHyperDebugStateActive(pVCpu))
            CPUMSetGuestDR6(pVCpu, CPUMGetGuestDR6(pVCpu) | pVmcb->guest.u64DR6);

        /* Reflect the exception back to the guest. */
        hmR0SvmSetPendingXcptDB(pVCpu);
        rc = VINF_SUCCESS;
    }

    /*
     * Update DR6.
     */
    if (CPUMIsHyperDebugStateActive(pVCpu))
    {
        Log5(("hmR0SvmExitXcptDB: DR6=%#RX64 -> %Rrc\n", pVmcb->guest.u64DR6, rc));
        pVmcb->guest.u64DR6 = X86_DR6_INIT_VAL;
        pVmcb->ctrl.u32VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DRX;
    }
    else
    {
        AssertMsg(rc == VINF_SUCCESS, ("rc=%Rrc\n", rc));
        Assert(!pVCpu->hm.s.fSingleInstruction && !DBGFIsStepping(pVCpu));
    }

    return rc;
}


/**
 * \#VMEXIT handler for alignment check exceptions (SVM_EXIT_XCPT_17).
 * Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptAC(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u3Type   = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector = X86_XCPT_AC;
    Event.n.u1ErrorCodeValid = 1;
    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
    return VINF_SUCCESS;
}


/**
 * \#VMEXIT handler for breakpoint exceptions (SVM_EXIT_XCPT_3).
 * Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptBP(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    int rc = DBGFRZTrap03Handler(pVCpu->CTX_SUFF(pVM), pVCpu, CPUMCTX2CORE(pCtx));
    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        SVMEVENT Event;
        Event.u          = 0;
        Event.n.u1Valid  = 1;
        Event.n.u3Type   = SVM_EVENT_EXCEPTION;
        Event.n.u8Vector = X86_XCPT_BP;
        hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
    }

    Assert(rc == VINF_SUCCESS || rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_EM_DBG_BREAKPOINT);
    return rc;
}


#if defined(HMSVM_ALWAYS_TRAP_ALL_XCPTS) || defined(VBOX_WITH_NESTED_HWVIRT_SVM)
/**
 * \#VMEXIT handler for generic exceptions. Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitXcptGeneric(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    uint8_t const  uVector  = pVmcb->ctrl.u64ExitCode - SVM_EXIT_XCPT_0;
    uint32_t const uErrCode = pVmcb->ctrl.u64ExitInfo1;
    Assert(pSvmTransient->u64ExitCode == pVmcb->ctrl.u64ExitCode);
    Assert(uVector <= X86_XCPT_LAST);
    Log4Func(("uVector=%#x uErrCode=%u\n", uVector, uErrCode));

    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u3Type   = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector = uVector;
    switch (uVector)
    {
        /* Shouldn't be here for reflecting #PFs (among other things, the fault address isn't passed along). */
        case X86_XCPT_PF:   AssertMsgFailed(("hmR0SvmExitXcptGeneric: Unexpected exception")); return VERR_SVM_IPE_5;
        case X86_XCPT_DF:
        case X86_XCPT_TS:
        case X86_XCPT_NP:
        case X86_XCPT_SS:
        case X86_XCPT_GP:
        case X86_XCPT_AC:
        {
            Event.n.u1ErrorCodeValid = 1;
            Event.n.u32ErrorCode     = uErrCode;
            break;
        }
    }

    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
    return VINF_SUCCESS;
}
#endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/**
 * \#VMEXIT handler for CLGI (SVM_EXIT_CLGI). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitClgi(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    Assert(pVmcb);
    Assert(!pVmcb->ctrl.IntCtrl.n.u1VGifEnable);

    VBOXSTRICTRC   rcStrict;
    bool const     fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    uint64_t const fImport = CPUMCTX_EXTRN_HWVIRT;
    if (fSupportsNextRipSave)
    {
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedClgi(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | fImport);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_SUCCESS)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_HWVIRT);
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for STGI (SVM_EXIT_STGI). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitStgi(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    /*
     * When VGIF is not used we always intercept STGI instructions. When VGIF is used,
     * we only intercept STGI when events are pending for GIF to become 1.
     */
    PSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    if (pVmcb->ctrl.IntCtrl.n.u1VGifEnable)
        hmR0SvmClearCtrlIntercept(pVCpu, pVmcb, SVM_CTRL_INTERCEPT_STGI);

    VBOXSTRICTRC   rcStrict;
    bool const     fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    uint64_t const fImport = CPUMCTX_EXTRN_HWVIRT;
    if (fSupportsNextRipSave)
    {
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedStgi(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | fImport);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_SUCCESS)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_HWVIRT);
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for VMLOAD (SVM_EXIT_VMLOAD). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitVmload(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    Assert(pVmcb);
    Assert(!pVmcb->ctrl.LbrVirt.n.u1VirtVmsaveVmload);

    VBOXSTRICTRC   rcStrict;
    bool const     fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    uint64_t const fImport = CPUMCTX_EXTRN_FS   | CPUMCTX_EXTRN_GS   | CPUMCTX_EXTRN_KERNEL_GS_BASE
                           | CPUMCTX_EXTRN_TR   | CPUMCTX_EXTRN_LDTR | CPUMCTX_EXTRN_SYSCALL_MSRS
                           | CPUMCTX_EXTRN_SYSENTER_MSRS;
    if (fSupportsNextRipSave)
    {
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedVmload(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | fImport);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_SUCCESS)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_FS              | HM_CHANGED_GUEST_GS
                                                 | HM_CHANGED_GUEST_TR              | HM_CHANGED_GUEST_LDTR
                                                 | HM_CHANGED_GUEST_KERNEL_GS_BASE  | HM_CHANGED_GUEST_SYSCALL_MSRS
                                                 | HM_CHANGED_GUEST_SYSENTER_MSR_MASK);
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for VMSAVE (SVM_EXIT_VMSAVE). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitVmsave(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    PCSVMVMCB pVmcb = hmR0SvmGetCurrentVmcb(pVCpu);
    Assert(!pVmcb->ctrl.LbrVirt.n.u1VirtVmsaveVmload);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedVmsave(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for INVLPGA (SVM_EXIT_INVLPGA). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitInvlpga(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedInvlpga(pVCpu, cbInstr);
    }
    else
    {
        HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        rcStrict = IEMExecOne(pVCpu);
    }

    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * \#VMEXIT handler for STGI (SVM_EXIT_VMRUN). Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmExitVmrun(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    /* We shall import the entire state here, just in case we enter and continue execution of
       the nested-guest with hardware-assisted SVM in ring-0, we would be switching VMCBs and
       could lose lose part of CPU state. */
    HMSVM_CPUMCTX_IMPORT_STATE(pVCpu, HMSVM_CPUMCTX_EXTRN_ALL);

    VBOXSTRICTRC rcStrict;
    bool const fSupportsNextRipSave = hmR0SvmSupportsNextRipSave(pVCpu);
    if (fSupportsNextRipSave)
    {
        PCSVMVMCB     pVmcb   = hmR0SvmGetCurrentVmcb(pVCpu);
        uint8_t const cbInstr = pVmcb->ctrl.u64NextRIP - pVCpu->cpum.GstCtx.rip;
        rcStrict = IEMExecDecodedVmrun(pVCpu, cbInstr);
    }
    else
    {
        /* We use IEMExecOneBypassEx() here as it supresses attempt to continue emulating any
           instruction(s) when interrupt inhibition is set as part of emulating the VMRUN
           instruction itself, see @bugref{7243#c126} */
        rcStrict = IEMExecOneBypassEx(pVCpu, CPUMCTX2CORE(&pVCpu->cpum.GstCtx), NULL /* pcbWritten */);
    }

    if (rcStrict == VINF_SUCCESS)
    {
        rcStrict = VINF_SVM_VMRUN;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_SVM_VMRUN_MASK);
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        rcStrict = VINF_SUCCESS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_RAISED_XCPT_MASK);
    }
    HMSVM_CHECK_SINGLE_STEP(pVCpu, rcStrict);
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * Nested-guest \#VMEXIT handler for debug exceptions (SVM_EXIT_XCPT_1).
 * Unconditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmNestedExitXcptDB(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    if (pVCpu->hm.s.Event.fPending)
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingInterpret);
        return VINF_EM_RAW_INJECT_TRPM_EVENT;
    }

    hmR0SvmSetPendingXcptDB(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Nested-guest \#VMEXIT handler for breakpoint exceptions (SVM_EXIT_XCPT_3).
 * Conditional \#VMEXIT.
 */
HMSVM_EXIT_DECL hmR0SvmNestedExitXcptBP(PVMCPU pVCpu, PSVMTRANSIENT pSvmTransient)
{
    HMSVM_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pSvmTransient);
    HMSVM_CHECK_EXIT_DUE_TO_EVENT_DELIVERY(pVCpu, pSvmTransient);

    SVMEVENT Event;
    Event.u          = 0;
    Event.n.u1Valid  = 1;
    Event.n.u3Type   = SVM_EVENT_EXCEPTION;
    Event.n.u8Vector = X86_XCPT_BP;
    hmR0SvmSetPendingEvent(pVCpu, &Event, 0 /* GCPtrFaultAddress */);
    return VINF_SUCCESS;
}
#endif /* VBOX_WITH_NESTED_HWVIRT_SVM */

/** @} */

