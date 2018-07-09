/* $Id$ */
/** @file
 * HM VMX (Intel VT-x) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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
#include <iprt/x86.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/thread.h>

#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/gim.h>
#include <VBox/vmm/apic.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include "HMVMXR0.h"
#include "dtrace/VBoxVMM.h"

#ifdef DEBUG_ramshankar
# define HMVMX_ALWAYS_SAVE_GUEST_RFLAGS
# define HMVMX_ALWAYS_SAVE_FULL_GUEST_STATE
# define HMVMX_ALWAYS_SYNC_FULL_GUEST_STATE
# define HMVMX_ALWAYS_CHECK_GUEST_STATE
# define HMVMX_ALWAYS_TRAP_ALL_XCPTS
# define HMVMX_ALWAYS_TRAP_PF
# define HMVMX_ALWAYS_FLUSH_TLB
# define HMVMX_ALWAYS_SWAP_EFER
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Use the function table. */
#define HMVMX_USE_FUNCTION_TABLE

/** Determine which tagged-TLB flush handler to use. */
#define HMVMX_FLUSH_TAGGED_TLB_EPT_VPID           0
#define HMVMX_FLUSH_TAGGED_TLB_EPT                1
#define HMVMX_FLUSH_TAGGED_TLB_VPID               2
#define HMVMX_FLUSH_TAGGED_TLB_NONE               3

/** @name HMVMX_READ_XXX
 * Flags to skip redundant reads of some common VMCS fields that are not part of
 * the guest-CPU or VCPU state but are needed while handling VM-exits.
 */
#define HMVMX_READ_IDT_VECTORING_INFO            RT_BIT_32(0)
#define HMVMX_READ_IDT_VECTORING_ERROR_CODE      RT_BIT_32(1)
#define HMVMX_READ_EXIT_QUALIFICATION            RT_BIT_32(2)
#define HMVMX_READ_EXIT_INSTR_LEN                RT_BIT_32(3)
#define HMVMX_READ_EXIT_INTERRUPTION_INFO        RT_BIT_32(4)
#define HMVMX_READ_EXIT_INTERRUPTION_ERROR_CODE  RT_BIT_32(5)
#define HMVMX_READ_EXIT_INSTR_INFO               RT_BIT_32(6)
/** @} */

/**
 * States of the VMCS.
 *
 * This does not reflect all possible VMCS states but currently only those
 * needed for maintaining the VMCS consistently even when thread-context hooks
 * are used. Maybe later this can be extended (i.e. Nested Virtualization).
 */
#define HMVMX_VMCS_STATE_CLEAR       RT_BIT(0)
#define HMVMX_VMCS_STATE_ACTIVE      RT_BIT(1)
#define HMVMX_VMCS_STATE_LAUNCHED    RT_BIT(2)

/**
 * Subset of the guest-CPU state that is kept by VMX R0 code while executing the
 * guest using hardware-assisted VMX.
 *
 * This excludes state like GPRs (other than RSP) which are always are
 * swapped and restored across the world-switch and also registers like EFER,
 * MSR which cannot be modified by the guest without causing a VM-exit.
 */
#define HMVMX_CPUMCTX_EXTRN_ALL      (  CPUMCTX_EXTRN_RIP             \
                                      | CPUMCTX_EXTRN_RFLAGS          \
                                      | CPUMCTX_EXTRN_RSP             \
                                      | CPUMCTX_EXTRN_SREG_MASK       \
                                      | CPUMCTX_EXTRN_TABLE_MASK      \
                                      | CPUMCTX_EXTRN_KERNEL_GS_BASE  \
                                      | CPUMCTX_EXTRN_SYSCALL_MSRS    \
                                      | CPUMCTX_EXTRN_SYSENTER_MSRS   \
                                      | CPUMCTX_EXTRN_TSC_AUX         \
                                      | CPUMCTX_EXTRN_OTHER_MSRS      \
                                      | CPUMCTX_EXTRN_CR0             \
                                      | CPUMCTX_EXTRN_CR3             \
                                      | CPUMCTX_EXTRN_CR4             \
                                      | CPUMCTX_EXTRN_DR7             \
                                      | CPUMCTX_EXTRN_HM_VMX_MASK)

/**
 * Exception bitmap mask for real-mode guests (real-on-v86).
 *
 * We need to intercept all exceptions manually except:
 * - \#AC and \#DB are always intercepted to prevent the CPU from deadlocking
 *   due to bugs in Intel CPUs.
 * - \#PF need not be intercepted even in real-mode if we have Nested Paging
 * support.
 */
#define HMVMX_REAL_MODE_XCPT_MASK    (  RT_BIT(X86_XCPT_DE)  /* always: | RT_BIT(X86_XCPT_DB) */ | RT_BIT(X86_XCPT_NMI)   \
                                      | RT_BIT(X86_XCPT_BP)             | RT_BIT(X86_XCPT_OF)    | RT_BIT(X86_XCPT_BR)    \
                                      | RT_BIT(X86_XCPT_UD)             | RT_BIT(X86_XCPT_NM)    | RT_BIT(X86_XCPT_DF)    \
                                      | RT_BIT(X86_XCPT_CO_SEG_OVERRUN) | RT_BIT(X86_XCPT_TS)    | RT_BIT(X86_XCPT_NP)    \
                                      | RT_BIT(X86_XCPT_SS)             | RT_BIT(X86_XCPT_GP)   /* RT_BIT(X86_XCPT_PF) */ \
                                      | RT_BIT(X86_XCPT_MF)  /* always: | RT_BIT(X86_XCPT_AC) */ | RT_BIT(X86_XCPT_MC)    \
                                      | RT_BIT(X86_XCPT_XF))

/** Maximum VM-instruction error number. */
#define HMVMX_INSTR_ERROR_MAX        28

/** Profiling macro. */
#ifdef HM_PROFILE_EXIT_DISPATCH
# define HMVMX_START_EXIT_DISPATCH_PROF() STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitDispatch, ed)
# define HMVMX_STOP_EXIT_DISPATCH_PROF()  STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitDispatch, ed)
#else
# define HMVMX_START_EXIT_DISPATCH_PROF() do { } while (0)
# define HMVMX_STOP_EXIT_DISPATCH_PROF()  do { } while (0)
#endif

/** Assert that preemption is disabled or covered by thread-context hooks. */
#define HMVMX_ASSERT_PREEMPT_SAFE(a_pVCpu)          Assert(   VMMR0ThreadCtxHookIsEnabled((a_pVCpu))   \
                                                           || !RTThreadPreemptIsEnabled(NIL_RTTHREAD))

/** Assert that we haven't migrated CPUs when thread-context hooks are not
 *  used. */
#define HMVMX_ASSERT_CPU_SAFE(a_pVCpu)              AssertMsg(   VMMR0ThreadCtxHookIsEnabled((a_pVCpu)) \
                                                              || (a_pVCpu)->hm.s.idEnteredCpu == RTMpCpuId(), \
                                                              ("Illegal migration! Entered on CPU %u Current %u\n", \
                                                              (a_pVCpu)->hm.s.idEnteredCpu, RTMpCpuId()))

/** Asserts that the given CPUMCTX_EXTRN_XXX bits are present in the guest-CPU
 *  context. */
#define HMVMX_CPUMCTX_ASSERT(a_pVCpu, a_fExtrnMbz)  AssertMsg(!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnMbz)), \
                                                              ("fExtrn=%#RX64 fExtrnMbz=%#RX64\n", \
                                                              (a_pVCpu)->cpum.GstCtx.fExtrn, (a_fExtrnMbz)))

/** Helper macro for VM-exit handlers called unexpectedly. */
#define HMVMX_UNEXPECTED_EXIT_RET(a_pVCpu, a_pVmxTransient) \
    do { \
        (a_pVCpu)->hm.s.u32HMError = (a_pVmxTransient)->uExitReason; \
        return VERR_VMX_UNEXPECTED_EXIT; \
    } while (0)

/** Macro for importing segment registers to the VMCS from the guest-CPU context. */
#ifdef VMX_USE_CACHED_VMCS_ACCESSES
# define HMVMX_IMPORT_SREG(Sel, a_pCtxSelReg) \
    hmR0VmxImportGuestSegmentReg(pVCpu, VMX_VMCS16_GUEST_##Sel##_SEL, VMX_VMCS32_GUEST_##Sel##_LIMIT, \
                                 VMX_VMCS_GUEST_##Sel##_BASE_CACHE_IDX, VMX_VMCS32_GUEST_##Sel##_ACCESS_RIGHTS, (a_pCtxSelReg))
#else
# define HMVMX_IMPORT_SREG(Sel, a_pCtxSelReg) \
    hmR0VmxImportGuestSegmentReg(pVCpu, VMX_VMCS16_GUEST_##Sel##_SEL, VMX_VMCS32_GUEST_##Sel##_LIMIT, \
                                 VMX_VMCS_GUEST_##Sel##_BASE, VMX_VMCS32_GUEST_##Sel##_ACCESS_RIGHTS, (a_pCtxSelReg))
#endif

/** Macro for exporting segment registers to the VMCS from the guest-CPU context. */
# define HMVMX_EXPORT_SREG(Sel, a_pCtxSelReg) \
    hmR0VmxExportGuestSegmentReg(pVCpu, VMX_VMCS16_GUEST_##Sel##_SEL, VMX_VMCS32_GUEST_##Sel##_LIMIT, \
                                 VMX_VMCS_GUEST_##Sel##_BASE, VMX_VMCS32_GUEST_##Sel##_ACCESS_RIGHTS, (a_pCtxSelReg))


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * VMX transient state.
 *
 * A state structure for holding miscellaneous information across
 * VMX non-root operation and restored after the transition.
 */
typedef struct VMXTRANSIENT
{
    /** The host's rflags/eflags. */
    RTCCUINTREG     fEFlags;
#if HC_ARCH_BITS == 32
    uint32_t        u32Alignment0;
#endif
    /** The guest's TPR value used for TPR shadowing. */
    uint8_t         u8GuestTpr;
    /** Alignment. */
    uint8_t         abAlignment0[7];

    /** The basic VM-exit reason. */
    uint16_t        uExitReason;
    /** Alignment. */
    uint16_t        u16Alignment0;
    /** The VM-exit interruption error code. */
    uint32_t        uExitIntErrorCode;
    /** The VM-exit exit code qualification. */
    uint64_t        uExitQualification;

    /** The VM-exit interruption-information field. */
    uint32_t        uExitIntInfo;
    /** The VM-exit instruction-length field. */
    uint32_t        cbInstr;
    /** The VM-exit instruction-information field. */
    union
    {
        /** Plain unsigned int representation. */
        uint32_t    u;
        /** INS and OUTS information. */
        struct
        {
            uint32_t    u7Reserved0 : 7;
            /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
            uint32_t    u3AddrSize  : 3;
            uint32_t    u5Reserved1 : 5;
            /** The segment register (X86_SREG_XXX). */
            uint32_t    iSegReg     : 3;
            uint32_t    uReserved2  : 14;
        } StrIo;
        /** INVEPT, INVVPID, INVPCID information.  */
        struct
        {
            /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
            uint32_t    u2Scaling     : 2;
            uint32_t    u5Reserved0   : 5;
            /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
            uint32_t    u3AddrSize    : 3;
            uint32_t    u1Reserved0   : 1;
            uint32_t    u4Reserved0   : 4;
            /** The segment register (X86_SREG_XXX). */
            uint32_t    iSegReg       : 3;
            /** The index register (X86_GREG_XXX). */
            uint32_t    iIdxReg       : 4;
            /** Set if index register is invalid. */
            uint32_t    fIdxRegValid  : 1;
            /** The base register (X86_GREG_XXX). */
            uint32_t    iBaseReg      : 4;
            /** Set if base register is invalid. */
            uint32_t    fBaseRegValid : 1;
            /** Register 2 (X86_GREG_XXX). */
            uint32_t    iReg2         : 4;
        } Inv;
    }               ExitInstrInfo;
    /** Whether the VM-entry failed or not. */
    bool            fVMEntryFailed;
    /** Alignment. */
    uint8_t         abAlignment1[3];

    /** The VM-entry interruption-information field. */
    uint32_t        uEntryIntInfo;
    /** The VM-entry exception error code field. */
    uint32_t        uEntryXcptErrorCode;
    /** The VM-entry instruction length field. */
    uint32_t        cbEntryInstr;

    /** IDT-vectoring information field. */
    uint32_t        uIdtVectoringInfo;
    /** IDT-vectoring error code. */
    uint32_t        uIdtVectoringErrorCode;

    /** Mask of currently read VMCS fields; HMVMX_READ_XXX. */
    uint32_t        fVmcsFieldsRead;

    /** Whether the guest debug state was active at the time of VM-exit. */
    bool            fWasGuestDebugStateActive;
    /** Whether the hyper debug state was active at the time of VM-exit. */
    bool            fWasHyperDebugStateActive;
    /** Whether TSC-offsetting should be setup before VM-entry. */
    bool            fUpdateTscOffsettingAndPreemptTimer;
    /** Whether the VM-exit was caused by a page-fault during delivery of a
     *  contributory exception or a page-fault. */
    bool            fVectoringDoublePF;
    /** Whether the VM-exit was caused by a page-fault during delivery of an
     *  external interrupt or NMI. */
    bool            fVectoringPF;
} VMXTRANSIENT;
AssertCompileMemberAlignment(VMXTRANSIENT, uExitReason,               sizeof(uint64_t));
AssertCompileMemberAlignment(VMXTRANSIENT, uExitIntInfo,              sizeof(uint64_t));
AssertCompileMemberAlignment(VMXTRANSIENT, uEntryIntInfo,             sizeof(uint64_t));
AssertCompileMemberAlignment(VMXTRANSIENT, fWasGuestDebugStateActive, sizeof(uint64_t));
AssertCompileMemberSize(VMXTRANSIENT, ExitInstrInfo, sizeof(uint32_t));
/** Pointer to VMX transient state. */
typedef VMXTRANSIENT *PVMXTRANSIENT;


/**
 * MSR-bitmap read permissions.
 */
typedef enum VMXMSREXITREAD
{
    /** Reading this MSR causes a VM-exit. */
    VMXMSREXIT_INTERCEPT_READ = 0xb,
    /** Reading this MSR does not cause a VM-exit. */
    VMXMSREXIT_PASSTHRU_READ
} VMXMSREXITREAD;
/** Pointer to MSR-bitmap read permissions. */
typedef VMXMSREXITREAD* PVMXMSREXITREAD;

/**
 * MSR-bitmap write permissions.
 */
typedef enum VMXMSREXITWRITE
{
    /** Writing to this MSR causes a VM-exit. */
    VMXMSREXIT_INTERCEPT_WRITE = 0xd,
    /** Writing to this MSR does not cause a VM-exit. */
    VMXMSREXIT_PASSTHRU_WRITE
} VMXMSREXITWRITE;
/** Pointer to MSR-bitmap write permissions. */
typedef VMXMSREXITWRITE* PVMXMSREXITWRITE;


/**
 * VMX VM-exit handler.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX-transient structure.
 */
#ifndef HMVMX_USE_FUNCTION_TABLE
typedef VBOXSTRICTRC                FNVMXEXITHANDLER(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
#else
typedef DECLCALLBACK(VBOXSTRICTRC)  FNVMXEXITHANDLER(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
/** Pointer to VM-exit handler. */
typedef FNVMXEXITHANDLER           *PFNVMXEXITHANDLER;
#endif

/**
 * VMX VM-exit handler, non-strict status code.
 *
 * This is generally the same as FNVMXEXITHANDLER, the NSRC bit is just FYI.
 *
 * @returns VBox status code, no informational status code returned.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX-transient structure.
 *
 * @remarks This is not used on anything returning VERR_EM_INTERPRETER as the
 *          use of that status code will be replaced with VINF_EM_SOMETHING
 *          later when switching over to IEM.
 */
#ifndef HMVMX_USE_FUNCTION_TABLE
typedef int                         FNVMXEXITHANDLERNSRC(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
#else
typedef FNVMXEXITHANDLER            FNVMXEXITHANDLERNSRC;
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void         hmR0VmxFlushEpt(PVMCPU pVCpu, VMXTLBFLUSHEPT enmTlbFlush);
static void         hmR0VmxFlushVpid(PVMCPU pVCpu, VMXTLBFLUSHVPID enmTlbFlush, RTGCPTR GCPtr);
static void         hmR0VmxClearIntNmiWindowsVmcs(PVMCPU pVCpu);
static int          hmR0VmxImportGuestState(PVMCPU pVCpu, uint64_t fWhat);
static VBOXSTRICTRC hmR0VmxInjectEventVmcs(PVMCPU pVCpu, uint64_t u64IntInfo, uint32_t cbInstr, uint32_t u32ErrCode,
                                           RTGCUINTREG GCPtrFaultAddress, bool fStepping, uint32_t *pfIntrState);
#if HC_ARCH_BITS == 32
static int          hmR0VmxInitVmcsReadCache(PVMCPU pVCpu);
#endif
#ifndef HMVMX_USE_FUNCTION_TABLE
DECLINLINE(VBOXSTRICTRC)      hmR0VmxHandleExit(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient, uint32_t rcReason);
# define HMVMX_EXIT_DECL      DECLINLINE(VBOXSTRICTRC)
# define HMVMX_EXIT_NSRC_DECL DECLINLINE(int)
#else
# define HMVMX_EXIT_DECL      static DECLCALLBACK(VBOXSTRICTRC)
# define HMVMX_EXIT_NSRC_DECL HMVMX_EXIT_DECL
#endif


/** @name VM-exit handlers.
 * @{
 */
static FNVMXEXITHANDLER     hmR0VmxExitXcptOrNmi;
static FNVMXEXITHANDLER     hmR0VmxExitExtInt;
static FNVMXEXITHANDLER     hmR0VmxExitTripleFault;
static FNVMXEXITHANDLERNSRC hmR0VmxExitInitSignal;
static FNVMXEXITHANDLERNSRC hmR0VmxExitSipi;
static FNVMXEXITHANDLERNSRC hmR0VmxExitIoSmi;
static FNVMXEXITHANDLERNSRC hmR0VmxExitSmi;
static FNVMXEXITHANDLERNSRC hmR0VmxExitIntWindow;
static FNVMXEXITHANDLERNSRC hmR0VmxExitNmiWindow;
static FNVMXEXITHANDLER     hmR0VmxExitTaskSwitch;
static FNVMXEXITHANDLER     hmR0VmxExitCpuid;
static FNVMXEXITHANDLER     hmR0VmxExitGetsec;
static FNVMXEXITHANDLER     hmR0VmxExitHlt;
static FNVMXEXITHANDLERNSRC hmR0VmxExitInvd;
static FNVMXEXITHANDLER     hmR0VmxExitInvlpg;
static FNVMXEXITHANDLER     hmR0VmxExitRdpmc;
static FNVMXEXITHANDLER     hmR0VmxExitVmcall;
static FNVMXEXITHANDLER     hmR0VmxExitRdtsc;
static FNVMXEXITHANDLERNSRC hmR0VmxExitRsm;
static FNVMXEXITHANDLERNSRC hmR0VmxExitSetPendingXcptUD;
static FNVMXEXITHANDLER     hmR0VmxExitMovCRx;
static FNVMXEXITHANDLER     hmR0VmxExitMovDRx;
static FNVMXEXITHANDLER     hmR0VmxExitIoInstr;
static FNVMXEXITHANDLER     hmR0VmxExitRdmsr;
static FNVMXEXITHANDLER     hmR0VmxExitWrmsr;
static FNVMXEXITHANDLERNSRC hmR0VmxExitErrInvalidGuestState;
static FNVMXEXITHANDLERNSRC hmR0VmxExitErrMsrLoad;
static FNVMXEXITHANDLERNSRC hmR0VmxExitErrUndefined;
static FNVMXEXITHANDLER     hmR0VmxExitMwait;
static FNVMXEXITHANDLER     hmR0VmxExitMtf;
static FNVMXEXITHANDLER     hmR0VmxExitMonitor;
static FNVMXEXITHANDLER     hmR0VmxExitPause;
static FNVMXEXITHANDLERNSRC hmR0VmxExitErrMachineCheck;
static FNVMXEXITHANDLERNSRC hmR0VmxExitTprBelowThreshold;
static FNVMXEXITHANDLER     hmR0VmxExitApicAccess;
static FNVMXEXITHANDLER     hmR0VmxExitXdtrAccess;
static FNVMXEXITHANDLER     hmR0VmxExitXdtrAccess;
static FNVMXEXITHANDLER     hmR0VmxExitEptViolation;
static FNVMXEXITHANDLER     hmR0VmxExitEptMisconfig;
static FNVMXEXITHANDLER     hmR0VmxExitRdtscp;
static FNVMXEXITHANDLER     hmR0VmxExitPreemptTimer;
static FNVMXEXITHANDLERNSRC hmR0VmxExitWbinvd;
static FNVMXEXITHANDLER     hmR0VmxExitXsetbv;
static FNVMXEXITHANDLER     hmR0VmxExitRdrand;
static FNVMXEXITHANDLER     hmR0VmxExitInvpcid;
/** @} */

static int          hmR0VmxExitXcptPF(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static int          hmR0VmxExitXcptMF(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static int          hmR0VmxExitXcptDB(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static int          hmR0VmxExitXcptBP(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static int          hmR0VmxExitXcptGP(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static int          hmR0VmxExitXcptAC(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static int          hmR0VmxExitXcptGeneric(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient);
static uint32_t     hmR0VmxCheckGuestState(PVMCPU pVCpu);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef HMVMX_USE_FUNCTION_TABLE

/**
 * VMX_EXIT dispatch table.
 */
static const PFNVMXEXITHANDLER g_apfnVMExitHandlers[VMX_EXIT_MAX + 1] =
{
 /* 00  VMX_EXIT_XCPT_OR_NMI             */  hmR0VmxExitXcptOrNmi,
 /* 01  VMX_EXIT_EXT_INT                 */  hmR0VmxExitExtInt,
 /* 02  VMX_EXIT_TRIPLE_FAULT            */  hmR0VmxExitTripleFault,
 /* 03  VMX_EXIT_INIT_SIGNAL             */  hmR0VmxExitInitSignal,
 /* 04  VMX_EXIT_SIPI                    */  hmR0VmxExitSipi,
 /* 05  VMX_EXIT_IO_SMI                  */  hmR0VmxExitIoSmi,
 /* 06  VMX_EXIT_SMI                     */  hmR0VmxExitSmi,
 /* 07  VMX_EXIT_INT_WINDOW              */  hmR0VmxExitIntWindow,
 /* 08  VMX_EXIT_NMI_WINDOW              */  hmR0VmxExitNmiWindow,
 /* 09  VMX_EXIT_TASK_SWITCH             */  hmR0VmxExitTaskSwitch,
 /* 10  VMX_EXIT_CPUID                   */  hmR0VmxExitCpuid,
 /* 11  VMX_EXIT_GETSEC                  */  hmR0VmxExitGetsec,
 /* 12  VMX_EXIT_HLT                     */  hmR0VmxExitHlt,
 /* 13  VMX_EXIT_INVD                    */  hmR0VmxExitInvd,
 /* 14  VMX_EXIT_INVLPG                  */  hmR0VmxExitInvlpg,
 /* 15  VMX_EXIT_RDPMC                   */  hmR0VmxExitRdpmc,
 /* 16  VMX_EXIT_RDTSC                   */  hmR0VmxExitRdtsc,
 /* 17  VMX_EXIT_RSM                     */  hmR0VmxExitRsm,
 /* 18  VMX_EXIT_VMCALL                  */  hmR0VmxExitVmcall,
 /* 19  VMX_EXIT_VMCLEAR                 */  hmR0VmxExitSetPendingXcptUD,
 /* 20  VMX_EXIT_VMLAUNCH                */  hmR0VmxExitSetPendingXcptUD,
 /* 21  VMX_EXIT_VMPTRLD                 */  hmR0VmxExitSetPendingXcptUD,
 /* 22  VMX_EXIT_VMPTRST                 */  hmR0VmxExitSetPendingXcptUD,
 /* 23  VMX_EXIT_VMREAD                  */  hmR0VmxExitSetPendingXcptUD,
 /* 24  VMX_EXIT_VMRESUME                */  hmR0VmxExitSetPendingXcptUD,
 /* 25  VMX_EXIT_VMWRITE                 */  hmR0VmxExitSetPendingXcptUD,
 /* 26  VMX_EXIT_VMXOFF                  */  hmR0VmxExitSetPendingXcptUD,
 /* 27  VMX_EXIT_VMXON                   */  hmR0VmxExitSetPendingXcptUD,
 /* 28  VMX_EXIT_MOV_CRX                 */  hmR0VmxExitMovCRx,
 /* 29  VMX_EXIT_MOV_DRX                 */  hmR0VmxExitMovDRx,
 /* 30  VMX_EXIT_IO_INSTR                */  hmR0VmxExitIoInstr,
 /* 31  VMX_EXIT_RDMSR                   */  hmR0VmxExitRdmsr,
 /* 32  VMX_EXIT_WRMSR                   */  hmR0VmxExitWrmsr,
 /* 33  VMX_EXIT_ERR_INVALID_GUEST_STATE */  hmR0VmxExitErrInvalidGuestState,
 /* 34  VMX_EXIT_ERR_MSR_LOAD            */  hmR0VmxExitErrMsrLoad,
 /* 35  UNDEFINED                        */  hmR0VmxExitErrUndefined,
 /* 36  VMX_EXIT_MWAIT                   */  hmR0VmxExitMwait,
 /* 37  VMX_EXIT_MTF                     */  hmR0VmxExitMtf,
 /* 38  UNDEFINED                        */  hmR0VmxExitErrUndefined,
 /* 39  VMX_EXIT_MONITOR                 */  hmR0VmxExitMonitor,
 /* 40  UNDEFINED                        */  hmR0VmxExitPause,
 /* 41  VMX_EXIT_PAUSE                   */  hmR0VmxExitErrMachineCheck,
 /* 42  VMX_EXIT_ERR_MACHINE_CHECK       */  hmR0VmxExitErrUndefined,
 /* 43  VMX_EXIT_TPR_BELOW_THRESHOLD     */  hmR0VmxExitTprBelowThreshold,
 /* 44  VMX_EXIT_APIC_ACCESS             */  hmR0VmxExitApicAccess,
 /* 45  UNDEFINED                        */  hmR0VmxExitErrUndefined,
 /* 46  VMX_EXIT_XDTR_ACCESS             */  hmR0VmxExitXdtrAccess,
 /* 47  VMX_EXIT_TR_ACCESS               */  hmR0VmxExitXdtrAccess,
 /* 48  VMX_EXIT_EPT_VIOLATION           */  hmR0VmxExitEptViolation,
 /* 49  VMX_EXIT_EPT_MISCONFIG           */  hmR0VmxExitEptMisconfig,
 /* 50  VMX_EXIT_INVEPT                  */  hmR0VmxExitSetPendingXcptUD,
 /* 51  VMX_EXIT_RDTSCP                  */  hmR0VmxExitRdtscp,
 /* 52  VMX_EXIT_PREEMPT_TIMER           */  hmR0VmxExitPreemptTimer,
 /* 53  VMX_EXIT_INVVPID                 */  hmR0VmxExitSetPendingXcptUD,
 /* 54  VMX_EXIT_WBINVD                  */  hmR0VmxExitWbinvd,
 /* 55  VMX_EXIT_XSETBV                  */  hmR0VmxExitXsetbv,
 /* 56  VMX_EXIT_APIC_WRITE              */  hmR0VmxExitErrUndefined,
 /* 57  VMX_EXIT_RDRAND                  */  hmR0VmxExitRdrand,
 /* 58  VMX_EXIT_INVPCID                 */  hmR0VmxExitInvpcid,
 /* 59  VMX_EXIT_VMFUNC                  */  hmR0VmxExitSetPendingXcptUD,
 /* 60  VMX_EXIT_ENCLS                   */  hmR0VmxExitErrUndefined,
 /* 61  VMX_EXIT_RDSEED                  */  hmR0VmxExitErrUndefined, /* only spurious exits, so undefined */
 /* 62  VMX_EXIT_PML_FULL                */  hmR0VmxExitErrUndefined,
 /* 63  VMX_EXIT_XSAVES                  */  hmR0VmxExitSetPendingXcptUD,
 /* 64  VMX_EXIT_XRSTORS                 */  hmR0VmxExitSetPendingXcptUD,
};
#endif /* HMVMX_USE_FUNCTION_TABLE */

#ifdef VBOX_STRICT
static const char * const g_apszVmxInstrErrors[HMVMX_INSTR_ERROR_MAX + 1] =
{
    /*  0 */ "(Not Used)",
    /*  1 */ "VMCALL executed in VMX root operation.",
    /*  2 */ "VMCLEAR with invalid physical address.",
    /*  3 */ "VMCLEAR with VMXON pointer.",
    /*  4 */ "VMLAUNCH with non-clear VMCS.",
    /*  5 */ "VMRESUME with non-launched VMCS.",
    /*  6 */ "VMRESUME after VMXOFF",
    /*  7 */ "VM-entry with invalid control fields.",
    /*  8 */ "VM-entry with invalid host state fields.",
    /*  9 */ "VMPTRLD with invalid physical address.",
    /* 10 */ "VMPTRLD with VMXON pointer.",
    /* 11 */ "VMPTRLD with incorrect revision identifier.",
    /* 12 */ "VMREAD/VMWRITE from/to unsupported VMCS component.",
    /* 13 */ "VMWRITE to read-only VMCS component.",
    /* 14 */ "(Not Used)",
    /* 15 */ "VMXON executed in VMX root operation.",
    /* 16 */ "VM-entry with invalid executive-VMCS pointer.",
    /* 17 */ "VM-entry with non-launched executing VMCS.",
    /* 18 */ "VM-entry with executive-VMCS pointer not VMXON pointer.",
    /* 19 */ "VMCALL with non-clear VMCS.",
    /* 20 */ "VMCALL with invalid VM-exit control fields.",
    /* 21 */ "(Not Used)",
    /* 22 */ "VMCALL with incorrect MSEG revision identifier.",
    /* 23 */ "VMXOFF under dual monitor treatment of SMIs and SMM.",
    /* 24 */ "VMCALL with invalid SMM-monitor features.",
    /* 25 */ "VM-entry with invalid VM-execution control fields in executive VMCS.",
    /* 26 */ "VM-entry with events blocked by MOV SS.",
    /* 27 */ "(Not Used)",
    /* 28 */ "Invalid operand to INVEPT/INVVPID."
};
#endif /* VBOX_STRICT */



/**
 * Updates the VM's last error record.
 *
 * If there was a VMX instruction error, reads the error data from the VMCS and
 * updates VCPU's last error record as well.
 *
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 *                  Can be NULL if @a rc is not VERR_VMX_UNABLE_TO_START_VM or
 *                  VERR_VMX_INVALID_VMCS_FIELD.
 * @param   rc      The error code.
 */
static void hmR0VmxUpdateErrorRecord(PVMCPU pVCpu, int rc)
{
    if (   rc == VERR_VMX_INVALID_VMCS_FIELD
        || rc == VERR_VMX_UNABLE_TO_START_VM)
    {
        AssertPtrReturnVoid(pVCpu);
        VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &pVCpu->hm.s.vmx.LastError.u32InstrError);
    }
    pVCpu->CTX_SUFF(pVM)->hm.s.rcInit = rc;
}


/**
 * Reads the VM-entry interruption-information field from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxReadEntryIntInfoVmcs(PVMXTRANSIENT pVmxTransient)
{
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, &pVmxTransient->uEntryIntInfo);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}

#ifdef VBOX_STRICT
/**
 * Reads the VM-entry exception error code field from the VMCS into
 * the VMX transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxReadEntryXcptErrorCodeVmcs(PVMXTRANSIENT pVmxTransient)
{
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE, &pVmxTransient->uEntryXcptErrorCode);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}


/**
 * Reads the VM-entry exception error code field from the VMCS into
 * the VMX transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxReadEntryInstrLenVmcs(PVMXTRANSIENT pVmxTransient)
{
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH, &pVmxTransient->cbEntryInstr);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}
#endif /* VBOX_STRICT */


/**
 * Reads the VM-exit interruption-information field from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitIntInfoVmcs(PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_EXIT_INTERRUPTION_INFO))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO, &pVmxTransient->uExitIntInfo);
        AssertRCReturn(rc,rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_EXIT_INTERRUPTION_INFO;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the VM-exit interruption error code from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitIntErrorCodeVmcs(PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_EXIT_INTERRUPTION_ERROR_CODE))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INTERRUPTION_ERROR_CODE, &pVmxTransient->uExitIntErrorCode);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_EXIT_INTERRUPTION_ERROR_CODE;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the VM-exit instruction length field from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitInstrLenVmcs(PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_EXIT_INSTR_LEN))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INSTR_LENGTH, &pVmxTransient->cbInstr);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_EXIT_INSTR_LEN;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the VM-exit instruction-information field from the VMCS into
 * the VMX transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitInstrInfoVmcs(PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_EXIT_INSTR_INFO))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INSTR_INFO, &pVmxTransient->ExitInstrInfo.u);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_EXIT_INSTR_INFO;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the exit code qualification from the VMCS into the VMX transient
 * structure.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT. (Required for the VMCS cache case.)
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitQualificationVmcs(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_EXIT_QUALIFICATION))
    {
        int rc = VMXReadVmcsGstN(VMX_VMCS_RO_EXIT_QUALIFICATION, &pVmxTransient->uExitQualification); NOREF(pVCpu);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_EXIT_QUALIFICATION;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the IDT-vectoring information field from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxReadIdtVectoringInfoVmcs(PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_IDT_VECTORING_INFO))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_IDT_VECTORING_INFO, &pVmxTransient->uIdtVectoringInfo);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_IDT_VECTORING_INFO;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the IDT-vectoring error code from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadIdtVectoringErrorCodeVmcs(PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_READ_IDT_VECTORING_ERROR_CODE))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_IDT_VECTORING_ERROR_CODE, &pVmxTransient->uIdtVectoringErrorCode);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_READ_IDT_VECTORING_ERROR_CODE;
    }
    return VINF_SUCCESS;
}


/**
 * Enters VMX root mode operation on the current CPU.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure. Can be
 *                          NULL, after a resume.
 * @param   HCPhysCpuPage   Physical address of the VMXON region.
 * @param   pvCpuPage       Pointer to the VMXON region.
 */
static int hmR0VmxEnterRootMode(PVM pVM, RTHCPHYS HCPhysCpuPage, void *pvCpuPage)
{
    Assert(HCPhysCpuPage && HCPhysCpuPage != NIL_RTHCPHYS);
    Assert(RT_ALIGN_T(HCPhysCpuPage, _4K, RTHCPHYS) == HCPhysCpuPage);
    Assert(pvCpuPage);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pVM)
    {
        /* Write the VMCS revision dword to the VMXON region. */
        *(uint32_t *)pvCpuPage = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hm.s.vmx.Msrs.u64BasicInfo);
    }

    /* Paranoid: Disable interrupts as, in theory, interrupt handlers might mess with CR4. */
    RTCCUINTREG fEFlags = ASMIntDisableFlags();

    /* Enable the VMX bit in CR4 if necessary. */
    RTCCUINTREG uOldCr4 = SUPR0ChangeCR4(X86_CR4_VMXE, RTCCUINTREG_MAX);

    /* Enter VMX root mode. */
    int rc = VMXEnable(HCPhysCpuPage);
    if (RT_FAILURE(rc))
    {
        if (!(uOldCr4 & X86_CR4_VMXE))
            SUPR0ChangeCR4(0, ~X86_CR4_VMXE);

        if (pVM)
            pVM->hm.s.vmx.HCPhysVmxEnableError = HCPhysCpuPage;
    }

    /* Restore interrupts. */
    ASMSetFlags(fEFlags);
    return rc;
}


/**
 * Exits VMX root mode operation on the current CPU.
 *
 * @returns VBox status code.
 */
static int hmR0VmxLeaveRootMode(void)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* Paranoid: Disable interrupts as, in theory, interrupts handlers might mess with CR4. */
    RTCCUINTREG fEFlags = ASMIntDisableFlags();

    /* If we're for some reason not in VMX root mode, then don't leave it. */
    RTCCUINTREG uHostCR4 = ASMGetCR4();

    int rc;
    if (uHostCR4 & X86_CR4_VMXE)
    {
        /* Exit VMX root mode and clear the VMX bit in CR4. */
        VMXDisable();
        SUPR0ChangeCR4(0, ~X86_CR4_VMXE);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VMX_NOT_IN_VMX_ROOT_MODE;

    /* Restore interrupts. */
    ASMSetFlags(fEFlags);
    return rc;
}


/**
 * Allocates and maps one physically contiguous page. The allocated page is
 * zero'd out. (Used by various VT-x structures).
 *
 * @returns IPRT status code.
 * @param   pMemObj         Pointer to the ring-0 memory object.
 * @param   ppVirt          Where to store the virtual address of the
 *                          allocation.
 * @param   pHCPhys         Where to store the physical address of the
 *                          allocation.
 */
static int hmR0VmxPageAllocZ(PRTR0MEMOBJ pMemObj, PRTR0PTR ppVirt, PRTHCPHYS pHCPhys)
{
    AssertPtrReturn(pMemObj, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppVirt,  VERR_INVALID_PARAMETER);
    AssertPtrReturn(pHCPhys, VERR_INVALID_PARAMETER);

    int rc = RTR0MemObjAllocCont(pMemObj, PAGE_SIZE, false /* fExecutable */);
    if (RT_FAILURE(rc))
        return rc;
    *ppVirt  = RTR0MemObjAddress(*pMemObj);
    *pHCPhys = RTR0MemObjGetPagePhysAddr(*pMemObj, 0 /* iPage */);
    ASMMemZero32(*ppVirt, PAGE_SIZE);
    return VINF_SUCCESS;
}


/**
 * Frees and unmaps an allocated physical page.
 *
 * @param   pMemObj         Pointer to the ring-0 memory object.
 * @param   ppVirt          Where to re-initialize the virtual address of
 *                          allocation as 0.
 * @param   pHCPhys         Where to re-initialize the physical address of the
 *                          allocation as 0.
 */
static void hmR0VmxPageFree(PRTR0MEMOBJ pMemObj, PRTR0PTR ppVirt, PRTHCPHYS pHCPhys)
{
    AssertPtr(pMemObj);
    AssertPtr(ppVirt);
    AssertPtr(pHCPhys);
    if (*pMemObj != NIL_RTR0MEMOBJ)
    {
        int rc = RTR0MemObjFree(*pMemObj, true /* fFreeMappings */);
        AssertRC(rc);
        *pMemObj = NIL_RTR0MEMOBJ;
        *ppVirt  = 0;
        *pHCPhys = 0;
    }
}


/**
 * Worker function to free VT-x related structures.
 *
 * @returns IPRT status code.
 * @param   pVM             The cross context VM structure.
 */
static void hmR0VmxStructsFree(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);

        hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjHostMsr, &pVCpu->hm.s.vmx.pvHostMsr, &pVCpu->hm.s.vmx.HCPhysHostMsr);
        hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjGuestMsr, &pVCpu->hm.s.vmx.pvGuestMsr, &pVCpu->hm.s.vmx.HCPhysGuestMsr);

        if (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
            hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjMsrBitmap, &pVCpu->hm.s.vmx.pvMsrBitmap, &pVCpu->hm.s.vmx.HCPhysMsrBitmap);

        hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjVmcs, &pVCpu->hm.s.vmx.pvVmcs, &pVCpu->hm.s.vmx.HCPhysVmcs);
    }

    hmR0VmxPageFree(&pVM->hm.s.vmx.hMemObjApicAccess, (PRTR0PTR)&pVM->hm.s.vmx.pbApicAccess, &pVM->hm.s.vmx.HCPhysApicAccess);
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    hmR0VmxPageFree(&pVM->hm.s.vmx.hMemObjScratch, &pVM->hm.s.vmx.pbScratch, &pVM->hm.s.vmx.HCPhysScratch);
#endif
}


/**
 * Worker function to allocate VT-x related VM structures.
 *
 * @returns IPRT status code.
 * @param   pVM             The cross context VM structure.
 */
static int hmR0VmxStructsAlloc(PVM pVM)
{
    /*
     * Initialize members up-front so we can cleanup properly on allocation failure.
     */
#define VMXLOCAL_INIT_VM_MEMOBJ(a_Name, a_VirtPrefix)       \
    pVM->hm.s.vmx.hMemObj##a_Name = NIL_RTR0MEMOBJ;         \
    pVM->hm.s.vmx.a_VirtPrefix##a_Name = 0;                 \
    pVM->hm.s.vmx.HCPhys##a_Name = 0;

#define VMXLOCAL_INIT_VMCPU_MEMOBJ(a_Name, a_VirtPrefix)    \
    pVCpu->hm.s.vmx.hMemObj##a_Name = NIL_RTR0MEMOBJ;       \
    pVCpu->hm.s.vmx.a_VirtPrefix##a_Name = 0;               \
    pVCpu->hm.s.vmx.HCPhys##a_Name = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    VMXLOCAL_INIT_VM_MEMOBJ(Scratch, pv);
#endif
    VMXLOCAL_INIT_VM_MEMOBJ(ApicAccess, pb);

    AssertCompile(sizeof(VMCPUID) == sizeof(pVM->cCpus));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        VMXLOCAL_INIT_VMCPU_MEMOBJ(Vmcs, pv);
        VMXLOCAL_INIT_VMCPU_MEMOBJ(MsrBitmap, pv);
        VMXLOCAL_INIT_VMCPU_MEMOBJ(GuestMsr, pv);
        VMXLOCAL_INIT_VMCPU_MEMOBJ(HostMsr, pv);
    }
#undef VMXLOCAL_INIT_VMCPU_MEMOBJ
#undef VMXLOCAL_INIT_VM_MEMOBJ

    /* The VMCS size cannot be more than 4096 bytes. See Intel spec. Appendix A.1 "Basic VMX Information". */
    AssertReturnStmt(MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(pVM->hm.s.vmx.Msrs.u64BasicInfo) <= PAGE_SIZE,
                     (&pVM->aCpus[0])->hm.s.u32HMError = VMX_UFC_INVALID_VMCS_SIZE,
                     VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO);

    /*
     * Allocate all the VT-x structures.
     */
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    rc = hmR0VmxPageAllocZ(&pVM->hm.s.vmx.hMemObjScratch, &pVM->hm.s.vmx.pbScratch, &pVM->hm.s.vmx.HCPhysScratch);
    if (RT_FAILURE(rc))
        goto cleanup;
    strcpy((char *)pVM->hm.s.vmx.pbScratch, "SCRATCH Magic");
    *(uint64_t *)(pVM->hm.s.vmx.pbScratch + 16) = UINT64_C(0xdeadbeefdeadbeef);
#endif

    /* Allocate the APIC-access page for trapping APIC accesses from the guest. */
    if (pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
    {
        rc = hmR0VmxPageAllocZ(&pVM->hm.s.vmx.hMemObjApicAccess, (PRTR0PTR)&pVM->hm.s.vmx.pbApicAccess,
                               &pVM->hm.s.vmx.HCPhysApicAccess);
        if (RT_FAILURE(rc))
            goto cleanup;
    }

    /*
     * Initialize per-VCPU VT-x structures.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);

        /* Allocate the VM control structure (VMCS). */
        rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjVmcs, &pVCpu->hm.s.vmx.pvVmcs, &pVCpu->hm.s.vmx.HCPhysVmcs);
        if (RT_FAILURE(rc))
            goto cleanup;

        /* Get the allocated virtual-APIC page from the APIC device for transparent TPR accesses. */
        if (   PDMHasApic(pVM)
            && (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW))
        {
            rc = APICGetApicPageForCpu(pVCpu, &pVCpu->hm.s.vmx.HCPhysVirtApic, (PRTR0PTR)&pVCpu->hm.s.vmx.pbVirtApic,
                                       NULL /* pR3Ptr */, NULL /* pRCPtr */);
            if (RT_FAILURE(rc))
                goto cleanup;
        }

        /*
         * Allocate the MSR-bitmap if supported by the CPU. The MSR-bitmap is for
         * transparent accesses of specific MSRs.
         *
         * If the condition for enabling MSR bitmaps changes here, don't forget to
         * update HMAreMsrBitmapsAvailable().
         */
        if (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
        {
            rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjMsrBitmap, &pVCpu->hm.s.vmx.pvMsrBitmap,
                                   &pVCpu->hm.s.vmx.HCPhysMsrBitmap);
            if (RT_FAILURE(rc))
                goto cleanup;
            ASMMemFill32(pVCpu->hm.s.vmx.pvMsrBitmap, PAGE_SIZE, UINT32_C(0xffffffff));
        }

        /* Allocate the VM-entry MSR-load and VM-exit MSR-store page for the guest MSRs. */
        rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjGuestMsr, &pVCpu->hm.s.vmx.pvGuestMsr, &pVCpu->hm.s.vmx.HCPhysGuestMsr);
        if (RT_FAILURE(rc))
            goto cleanup;

        /* Allocate the VM-exit MSR-load page for the host MSRs. */
        rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjHostMsr, &pVCpu->hm.s.vmx.pvHostMsr, &pVCpu->hm.s.vmx.HCPhysHostMsr);
        if (RT_FAILURE(rc))
            goto cleanup;
    }

    return VINF_SUCCESS;

cleanup:
    hmR0VmxStructsFree(pVM);
    return rc;
}


/**
 * Does global VT-x initialization (called during module initialization).
 *
 * @returns VBox status code.
 */
VMMR0DECL(int) VMXR0GlobalInit(void)
{
#ifdef HMVMX_USE_FUNCTION_TABLE
    AssertCompile(VMX_EXIT_MAX + 1 == RT_ELEMENTS(g_apfnVMExitHandlers));
# ifdef VBOX_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(g_apfnVMExitHandlers); i++)
        Assert(g_apfnVMExitHandlers[i]);
# endif
#endif
    return VINF_SUCCESS;
}


/**
 * Does global VT-x termination (called during module termination).
 */
VMMR0DECL(void) VMXR0GlobalTerm()
{
    /* Nothing to do currently. */
}


/**
 * Sets up and activates VT-x on the current CPU.
 *
 * @returns VBox status code.
 * @param   pHostCpu        Pointer to the global CPU info struct.
 * @param   pVM             The cross context VM structure.  Can be
 *                          NULL after a host resume operation.
 * @param   pvCpuPage       Pointer to the VMXON region (can be NULL if @a
 *                          fEnabledByHost is @c true).
 * @param   HCPhysCpuPage   Physical address of the VMXON region (can be 0 if
 *                          @a fEnabledByHost is @c true).
 * @param   fEnabledByHost  Set if SUPR0EnableVTx() or similar was used to
 *                          enable VT-x on the host.
 * @param   pvMsrs          Opaque pointer to VMXMSRS struct.
 */
VMMR0DECL(int) VMXR0EnableCpu(PHMGLOBALCPUINFO pHostCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage, bool fEnabledByHost,
                              void *pvMsrs)
{
    Assert(pHostCpu);
    Assert(pvMsrs);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* Enable VT-x if it's not already enabled by the host. */
    if (!fEnabledByHost)
    {
        int rc = hmR0VmxEnterRootMode(pVM, HCPhysCpuPage, pvCpuPage);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Flush all EPT tagged-TLB entries (in case VirtualBox or any other hypervisor have been
     * using EPTPs) so we don't retain any stale guest-physical mappings which won't get
     * invalidated when flushing by VPID.
     */
    PVMXMSRS pMsrs = (PVMXMSRS)pvMsrs;
    if (pMsrs->u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS)
    {
        hmR0VmxFlushEpt(NULL /* pVCpu */, VMXTLBFLUSHEPT_ALL_CONTEXTS);
        pHostCpu->fFlushAsidBeforeUse = false;
    }
    else
        pHostCpu->fFlushAsidBeforeUse = true;

    /* Ensure each VCPU scheduled on this CPU gets a new VPID on resume. See @bugref{6255}. */
    ++pHostCpu->cTlbFlushes;

    return VINF_SUCCESS;
}


/**
 * Deactivates VT-x on the current CPU.
 *
 * @returns VBox status code.
 * @param   pHostCpu        Pointer to the global CPU info struct.
 * @param   pvCpuPage       Pointer to the VMXON region.
 * @param   HCPhysCpuPage   Physical address of the VMXON region.
 *
 * @remarks This function should never be called when SUPR0EnableVTx() or
 *          similar was used to enable VT-x on the host.
 */
VMMR0DECL(int) VMXR0DisableCpu(PHMGLOBALCPUINFO pHostCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    RT_NOREF3(pHostCpu, pvCpuPage, HCPhysCpuPage);

    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    return hmR0VmxLeaveRootMode();
}


/**
 * Sets the permission bits for the specified MSR in the MSR bitmap.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uMsr        The MSR value.
 * @param   enmRead     Whether reading this MSR causes a VM-exit.
 * @param   enmWrite    Whether writing this MSR causes a VM-exit.
 */
static void hmR0VmxSetMsrPermission(PVMCPU pVCpu, uint32_t uMsr, VMXMSREXITREAD enmRead, VMXMSREXITWRITE enmWrite)
{
    int32_t iBit;
    uint8_t *pbMsrBitmap = (uint8_t *)pVCpu->hm.s.vmx.pvMsrBitmap;

    /*
     * Layout:
     * 0x000 - 0x3ff - Low MSR read bits
     * 0x400 - 0x7ff - High MSR read bits
     * 0x800 - 0xbff - Low MSR write bits
     * 0xc00 - 0xfff - High MSR write bits
     */
    if (uMsr <= 0x00001FFF)
        iBit = uMsr;
    else if (uMsr - UINT32_C(0xC0000000) <= UINT32_C(0x00001FFF))
    {
        iBit = uMsr - UINT32_C(0xC0000000);
        pbMsrBitmap += 0x400;
    }
    else
        AssertMsgFailedReturnVoid(("hmR0VmxSetMsrPermission: Invalid MSR %#RX32\n", uMsr));

    Assert(iBit <= 0x1fff);
    if (enmRead == VMXMSREXIT_INTERCEPT_READ)
        ASMBitSet(pbMsrBitmap, iBit);
    else
        ASMBitClear(pbMsrBitmap, iBit);

    if (enmWrite == VMXMSREXIT_INTERCEPT_WRITE)
        ASMBitSet(pbMsrBitmap + 0x800, iBit);
    else
        ASMBitClear(pbMsrBitmap + 0x800, iBit);
}


#ifdef VBOX_STRICT
/**
 * Gets the permission bits for the specified MSR in the MSR bitmap.
 *
 * @returns VBox status code.
 * @retval VINF_SUCCESS        if the specified MSR is found.
 * @retval VERR_NOT_FOUND      if the specified MSR is not found.
 * @retval VERR_NOT_SUPPORTED  if VT-x doesn't allow the MSR.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uMsr            The MSR.
 * @param   penmRead        Where to store the read permissions.
 * @param   penmWrite       Where to store the write permissions.
 */
static int hmR0VmxGetMsrPermission(PVMCPU pVCpu, uint32_t uMsr, PVMXMSREXITREAD penmRead, PVMXMSREXITWRITE penmWrite)
{
    AssertPtrReturn(penmRead,  VERR_INVALID_PARAMETER);
    AssertPtrReturn(penmWrite, VERR_INVALID_PARAMETER);
    int32_t iBit;
    uint8_t *pbMsrBitmap = (uint8_t *)pVCpu->hm.s.vmx.pvMsrBitmap;

    /* See hmR0VmxSetMsrPermission() for the layout. */
    if (uMsr <= 0x00001FFF)
        iBit = uMsr;
    else if (   uMsr >= 0xC0000000
             && uMsr <= 0xC0001FFF)
    {
        iBit = (uMsr - 0xC0000000);
        pbMsrBitmap += 0x400;
    }
    else
        AssertMsgFailedReturn(("hmR0VmxGetMsrPermission: Invalid MSR %#RX32\n", uMsr), VERR_NOT_SUPPORTED);

    Assert(iBit <= 0x1fff);
    if (ASMBitTest(pbMsrBitmap, iBit))
        *penmRead = VMXMSREXIT_INTERCEPT_READ;
    else
        *penmRead = VMXMSREXIT_PASSTHRU_READ;

    if (ASMBitTest(pbMsrBitmap + 0x800, iBit))
        *penmWrite = VMXMSREXIT_INTERCEPT_WRITE;
    else
        *penmWrite = VMXMSREXIT_PASSTHRU_WRITE;
    return VINF_SUCCESS;
}
#endif /* VBOX_STRICT */


/**
 * Updates the VMCS with the number of effective MSRs in the auto-load/store MSR
 * area.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   cMsrs       The number of MSRs.
 */
static int hmR0VmxSetAutoLoadStoreMsrCount(PVMCPU pVCpu, uint32_t cMsrs)
{
    /* Shouldn't ever happen but there -is- a number. We're well within the recommended 512. */
    uint32_t const cMaxSupportedMsrs = MSR_IA32_VMX_MISC_MAX_MSR(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.Msrs.u64Misc);
    if (RT_UNLIKELY(cMsrs > cMaxSupportedMsrs))
    {
        LogRel(("CPU auto-load/store MSR count in VMCS exceeded cMsrs=%u Supported=%u.\n", cMsrs, cMaxSupportedMsrs));
        pVCpu->hm.s.u32HMError = VMX_UFC_INSUFFICIENT_GUEST_MSR_STORAGE;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Update number of guest MSRs to load/store across the world-switch. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT, cMsrs);
    rc    |= VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT, cMsrs);

    /* Update number of host MSRs to load after the world-switch. Identical to guest-MSR count as it's always paired. */
    rc    |= VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT,  cMsrs);
    AssertRCReturn(rc, rc);

    /* Update the VCPU's copy of the MSR count. */
    pVCpu->hm.s.vmx.cMsrs = cMsrs;

    return VINF_SUCCESS;
}


/**
 * Adds a new (or updates the value of an existing) guest/host MSR
 * pair to be swapped during the world-switch as part of the
 * auto-load/store MSR area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uMsr                The MSR.
 * @param   uGuestMsrValue      Value of the guest MSR.
 * @param   fUpdateHostMsr      Whether to update the value of the host MSR if
 *                              necessary.
 * @param   pfAddedAndUpdated   Where to store whether the MSR was added -and-
 *                              its value was updated. Optional, can be NULL.
 */
static int hmR0VmxAddAutoLoadStoreMsr(PVMCPU pVCpu, uint32_t uMsr, uint64_t uGuestMsrValue, bool fUpdateHostMsr,
                                       bool *pfAddedAndUpdated)
{
    PVMXAUTOMSR pGuestMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
    uint32_t    cMsrs     = pVCpu->hm.s.vmx.cMsrs;
    uint32_t    i;
    for (i = 0; i < cMsrs; i++)
    {
        if (pGuestMsr->u32Msr == uMsr)
            break;
        pGuestMsr++;
    }

    bool fAdded = false;
    if (i == cMsrs)
    {
        ++cMsrs;
        int rc = hmR0VmxSetAutoLoadStoreMsrCount(pVCpu, cMsrs);
        AssertMsgRCReturn(rc, ("hmR0VmxAddAutoLoadStoreMsr: Insufficient space to add MSR %u\n", uMsr), rc);

        /* Now that we're swapping MSRs during the world-switch, allow the guest to read/write them without causing VM-exits. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
            hmR0VmxSetMsrPermission(pVCpu, uMsr, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);

        fAdded = true;
    }

    /* Update the MSR values in the auto-load/store MSR area. */
    pGuestMsr->u32Msr    = uMsr;
    pGuestMsr->u64Value  = uGuestMsrValue;

    /* Create/update the MSR slot in the host MSR area. */
    PVMXAUTOMSR pHostMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvHostMsr;
    pHostMsr += i;
    pHostMsr->u32Msr     = uMsr;

    /*
     * Update the host MSR only when requested by the caller AND when we're
     * adding it to the auto-load/store area. Otherwise, it would have been
     * updated by hmR0VmxExportHostMsrs(). We do this for performance reasons.
     */
    bool fUpdatedMsrValue = false;
    if (   fAdded
        && fUpdateHostMsr)
    {
        Assert(!VMMRZCallRing3IsEnabled(pVCpu));
        Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
        pHostMsr->u64Value = ASMRdMsr(pHostMsr->u32Msr);
        fUpdatedMsrValue = true;
    }

    if (pfAddedAndUpdated)
        *pfAddedAndUpdated = fUpdatedMsrValue;
    return VINF_SUCCESS;
}


/**
 * Removes a guest/host MSR pair to be swapped during the world-switch from the
 * auto-load/store MSR area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uMsr        The MSR.
 */
static int hmR0VmxRemoveAutoLoadStoreMsr(PVMCPU pVCpu, uint32_t uMsr)
{
    PVMXAUTOMSR pGuestMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
    uint32_t    cMsrs     = pVCpu->hm.s.vmx.cMsrs;
    for (uint32_t i = 0; i < cMsrs; i++)
    {
        /* Find the MSR. */
        if (pGuestMsr->u32Msr == uMsr)
        {
            /* If it's the last MSR, simply reduce the count. */
            if (i == cMsrs - 1)
            {
                --cMsrs;
                break;
            }

            /* Remove it by swapping the last MSR in place of it, and reducing the count. */
            PVMXAUTOMSR pLastGuestMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
            pLastGuestMsr            += cMsrs - 1;
            pGuestMsr->u32Msr         = pLastGuestMsr->u32Msr;
            pGuestMsr->u64Value       = pLastGuestMsr->u64Value;

            PVMXAUTOMSR pHostMsr     = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvHostMsr;
            PVMXAUTOMSR pLastHostMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvHostMsr;
            pLastHostMsr            += cMsrs - 1;
            pHostMsr->u32Msr         = pLastHostMsr->u32Msr;
            pHostMsr->u64Value       = pLastHostMsr->u64Value;
            --cMsrs;
            break;
        }
        pGuestMsr++;
    }

    /* Update the VMCS if the count changed (meaning the MSR was found). */
    if (cMsrs != pVCpu->hm.s.vmx.cMsrs)
    {
        int rc = hmR0VmxSetAutoLoadStoreMsrCount(pVCpu, cMsrs);
        AssertRCReturn(rc, rc);

        /* We're no longer swapping MSRs during the world-switch, intercept guest read/writes to them. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
            hmR0VmxSetMsrPermission(pVCpu, uMsr, VMXMSREXIT_INTERCEPT_READ, VMXMSREXIT_INTERCEPT_WRITE);

        Log4Func(("Removed MSR %#RX32 new cMsrs=%u\n", uMsr, pVCpu->hm.s.vmx.cMsrs));
        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}


/**
 * Checks if the specified guest MSR is part of the auto-load/store area in
 * the VMCS.
 *
 * @returns true if found, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uMsr        The MSR to find.
 */
static bool hmR0VmxIsAutoLoadStoreGuestMsr(PVMCPU pVCpu, uint32_t uMsr)
{
    PVMXAUTOMSR pGuestMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
    uint32_t    cMsrs     = pVCpu->hm.s.vmx.cMsrs;

    for (uint32_t i = 0; i < cMsrs; i++, pGuestMsr++)
    {
        if (pGuestMsr->u32Msr == uMsr)
            return true;
    }
    return false;
}


/**
 * Updates the value of all host MSRs in the auto-load/store area in the VMCS.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxUpdateAutoLoadStoreHostMsrs(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    PVMXAUTOMSR pHostMsr  = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvHostMsr;
    PVMXAUTOMSR pGuestMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
    uint32_t    cMsrs    = pVCpu->hm.s.vmx.cMsrs;

    for (uint32_t i = 0; i < cMsrs; i++, pHostMsr++, pGuestMsr++)
    {
        AssertReturnVoid(pHostMsr->u32Msr == pGuestMsr->u32Msr);

        /*
         * Performance hack for the host EFER MSR. We use the cached value rather than re-read it.
         * Strict builds will catch mismatches in hmR0VmxCheckAutoLoadStoreMsrs(). See @bugref{7368}.
         */
        if (pHostMsr->u32Msr == MSR_K6_EFER)
            pHostMsr->u64Value = pVCpu->CTX_SUFF(pVM)->hm.s.vmx.u64HostEfer;
        else
            pHostMsr->u64Value = ASMRdMsr(pHostMsr->u32Msr);
    }

    pVCpu->hm.s.vmx.fUpdatedHostMsrs = true;
}


/**
 * Saves a set of host MSRs to allow read/write passthru access to the guest and
 * perform lazy restoration of the host MSRs while leaving VT-x.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxLazySaveHostMsrs(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Note: If you're adding MSRs here, make sure to update the MSR-bitmap permissions in hmR0VmxSetupProcCtls().
     */
    if (!(pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_SAVED_HOST))
    {
        Assert(!(pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST));  /* Guest MSRs better not be loaded now. */
#if HC_ARCH_BITS == 64
        if (pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests)
        {
            pVCpu->hm.s.vmx.u64HostLStarMsr        = ASMRdMsr(MSR_K8_LSTAR);
            pVCpu->hm.s.vmx.u64HostStarMsr         = ASMRdMsr(MSR_K6_STAR);
            pVCpu->hm.s.vmx.u64HostSFMaskMsr       = ASMRdMsr(MSR_K8_SF_MASK);
            pVCpu->hm.s.vmx.u64HostKernelGSBaseMsr = ASMRdMsr(MSR_K8_KERNEL_GS_BASE);
        }
#endif
        pVCpu->hm.s.vmx.fLazyMsrs |= VMX_LAZY_MSRS_SAVED_HOST;
    }
}


/**
 * Checks whether the MSR belongs to the set of guest MSRs that we restore
 * lazily while leaving VT-x.
 *
 * @returns true if it does, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uMsr        The MSR to check.
 */
static bool hmR0VmxIsLazyGuestMsr(PVMCPU pVCpu, uint32_t uMsr)
{
    NOREF(pVCpu);
#if HC_ARCH_BITS == 64
    if (pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests)
    {
        switch (uMsr)
        {
            case MSR_K8_LSTAR:
            case MSR_K6_STAR:
            case MSR_K8_SF_MASK:
            case MSR_K8_KERNEL_GS_BASE:
                return true;
        }
    }
#else
    RT_NOREF(pVCpu, uMsr);
#endif
    return false;
}


/**
 * Loads a set of guests MSRs to allow read/passthru to the guest.
 *
 * The name of this function is slightly confusing. This function does NOT
 * postpone loading, but loads the MSR right now. "hmR0VmxLazy" is simply a
 * common prefix for functions dealing with "lazy restoration" of the shared
 * MSRs.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxLazyLoadGuestMsrs(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    Assert(pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_SAVED_HOST);
#if HC_ARCH_BITS == 64
    if (pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests)
    {
        /*
         * If the guest MSRs are not loaded -and- if all the guest MSRs are identical
         * to the MSRs on the CPU (which are the saved host MSRs, see assertion above) then
         * we can skip a few MSR writes.
         *
         * Otherwise, it implies either 1. they're not loaded, or 2. they're loaded but the
         * guest MSR values in the guest-CPU context might be different to what's currently
         * loaded in the CPU. In either case, we need to write the new guest MSR values to the
         * CPU, see @bugref{8728}.
         */
        PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        if (   !(pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST)
            && pCtx->msrKERNELGSBASE == pVCpu->hm.s.vmx.u64HostKernelGSBaseMsr
            && pCtx->msrLSTAR        == pVCpu->hm.s.vmx.u64HostLStarMsr
            && pCtx->msrSTAR         == pVCpu->hm.s.vmx.u64HostStarMsr
            && pCtx->msrSFMASK       == pVCpu->hm.s.vmx.u64HostSFMaskMsr)
        {
#ifdef VBOX_STRICT
            Assert(ASMRdMsr(MSR_K8_KERNEL_GS_BASE) == pCtx->msrKERNELGSBASE);
            Assert(ASMRdMsr(MSR_K8_LSTAR)          == pCtx->msrLSTAR);
            Assert(ASMRdMsr(MSR_K6_STAR)           == pCtx->msrSTAR);
            Assert(ASMRdMsr(MSR_K8_SF_MASK)        == pCtx->msrSFMASK);
#endif
        }
        else
        {
            ASMWrMsr(MSR_K8_KERNEL_GS_BASE, pCtx->msrKERNELGSBASE);
            ASMWrMsr(MSR_K8_LSTAR,          pCtx->msrLSTAR);
            ASMWrMsr(MSR_K6_STAR,           pCtx->msrSTAR);
            ASMWrMsr(MSR_K8_SF_MASK,        pCtx->msrSFMASK);
        }
    }
#endif
    pVCpu->hm.s.vmx.fLazyMsrs |= VMX_LAZY_MSRS_LOADED_GUEST;
}


/**
 * Performs lazy restoration of the set of host MSRs if they were previously
 * loaded with guest MSR values.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 * @remarks The guest MSRs should have been saved back into the guest-CPU
 *          context by hmR0VmxImportGuestState()!!!
 */
static void hmR0VmxLazyRestoreHostMsrs(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    if (pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST)
    {
        Assert(pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_SAVED_HOST);
#if HC_ARCH_BITS == 64
        if (pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests)
        {
            ASMWrMsr(MSR_K8_LSTAR,          pVCpu->hm.s.vmx.u64HostLStarMsr);
            ASMWrMsr(MSR_K6_STAR,           pVCpu->hm.s.vmx.u64HostStarMsr);
            ASMWrMsr(MSR_K8_SF_MASK,        pVCpu->hm.s.vmx.u64HostSFMaskMsr);
            ASMWrMsr(MSR_K8_KERNEL_GS_BASE, pVCpu->hm.s.vmx.u64HostKernelGSBaseMsr);
        }
#endif
    }
    pVCpu->hm.s.vmx.fLazyMsrs &= ~(VMX_LAZY_MSRS_LOADED_GUEST | VMX_LAZY_MSRS_SAVED_HOST);
}


/**
 * Verifies that our cached values of the VMCS fields are all consistent with
 * what's actually present in the VMCS.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if all our caches match their respective VMCS fields.
 * @retval  VERR_VMX_VMCS_FIELD_CACHE_INVALID if a cache field doesn't match the
 *                                            VMCS content. HMCPU error-field is
 *                                            updated, see VMX_VCI_XXX.
 * @param   pVCpu   The cross context virtual CPU structure.
 */
static int hmR0VmxCheckVmcsCtls(PVMCPU pVCpu)
{
    uint32_t u32Val;
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY, &u32Val);
    AssertRCReturn(rc, rc);
    AssertMsgReturnStmt(pVCpu->hm.s.vmx.u32EntryCtls == u32Val,
                        ("Cache=%#RX32 VMCS=%#RX32\n", pVCpu->hm.s.vmx.u32EntryCtls, u32Val),
                        pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_ENTRY,
                        VERR_VMX_VMCS_FIELD_CACHE_INVALID);

    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXIT, &u32Val);
    AssertRCReturn(rc, rc);
    AssertMsgReturnStmt(pVCpu->hm.s.vmx.u32ExitCtls == u32Val,
                        ("Cache=%#RX32 VMCS=%#RX32\n", pVCpu->hm.s.vmx.u32ExitCtls, u32Val),
                        pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_EXIT,
                        VERR_VMX_VMCS_FIELD_CACHE_INVALID);

    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PIN_EXEC, &u32Val);
    AssertRCReturn(rc, rc);
    AssertMsgReturnStmt(pVCpu->hm.s.vmx.u32PinCtls == u32Val,
                        ("Cache=%#RX32 VMCS=%#RX32\n", pVCpu->hm.s.vmx.u32PinCtls, u32Val),
                        pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_PIN_EXEC,
                        VERR_VMX_VMCS_FIELD_CACHE_INVALID);

    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, &u32Val);
    AssertRCReturn(rc, rc);
    AssertMsgReturnStmt(pVCpu->hm.s.vmx.u32ProcCtls == u32Val,
                        ("Cache=%#RX32 VMCS=%#RX32\n", pVCpu->hm.s.vmx.u32ProcCtls, u32Val),
                        pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_PROC_EXEC,
                        VERR_VMX_VMCS_FIELD_CACHE_INVALID);

    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
    {
        rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, &u32Val);
        AssertRCReturn(rc, rc);
        AssertMsgReturnStmt(pVCpu->hm.s.vmx.u32ProcCtls2 == u32Val,
                            ("Cache=%#RX32 VMCS=%#RX32\n", pVCpu->hm.s.vmx.u32ProcCtls2, u32Val),
                            pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_PROC_EXEC2,
                            VERR_VMX_VMCS_FIELD_CACHE_INVALID);
    }

    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, &u32Val);
    AssertRCReturn(rc, rc);
    AssertMsgReturnStmt(pVCpu->hm.s.vmx.u32XcptBitmap == u32Val,
                        ("Cache=%#RX32 VMCS=%#RX32\n", pVCpu->hm.s.vmx.u32XcptBitmap, u32Val),
                        pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_XCPT_BITMAP,
                        VERR_VMX_VMCS_FIELD_CACHE_INVALID);

    uint64_t u64Val;
    rc = VMXReadVmcs64(VMX_VMCS64_CTRL_TSC_OFFSET_FULL, &u64Val);
    AssertRCReturn(rc, rc);
    AssertMsgReturnStmt(pVCpu->hm.s.vmx.u64TscOffset == u64Val,
                        ("Cache=%#RX64 VMCS=%#RX64\n", pVCpu->hm.s.vmx.u64TscOffset, u64Val),
                        pVCpu->hm.s.u32HMError = VMX_VCI_CTRL_TSC_OFFSET,
                        VERR_VMX_VMCS_FIELD_CACHE_INVALID);

    return VINF_SUCCESS;
}


#ifdef VBOX_STRICT
/**
 * Verifies that our cached host EFER value has not changed
 * since we cached it.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static void hmR0VmxCheckHostEferMsr(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pVCpu->hm.s.vmx.u32ExitCtls & VMX_VMCS_CTRL_EXIT_LOAD_HOST_EFER_MSR)
    {
        uint64_t u64Val;
        int rc = VMXReadVmcs64(VMX_VMCS64_HOST_EFER_FULL, &u64Val);
        AssertRC(rc);

        uint64_t u64HostEferMsr = ASMRdMsr(MSR_K6_EFER);
        AssertMsgReturnVoid(u64HostEferMsr == u64Val, ("u64HostEferMsr=%#RX64 u64Val=%#RX64\n", u64HostEferMsr, u64Val));
    }
}


/**
 * Verifies whether the guest/host MSR pairs in the auto-load/store area in the
 * VMCS are correct.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static void hmR0VmxCheckAutoLoadStoreMsrs(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* Verify MSR counts in the VMCS are what we think it should be.  */
    uint32_t cMsrs;
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT, &cMsrs);  AssertRC(rc);
    Assert(cMsrs == pVCpu->hm.s.vmx.cMsrs);

    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT, &cMsrs);      AssertRC(rc);
    Assert(cMsrs == pVCpu->hm.s.vmx.cMsrs);

    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT, &cMsrs);       AssertRC(rc);
    Assert(cMsrs == pVCpu->hm.s.vmx.cMsrs);

    PCVMXAUTOMSR pHostMsr  = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvHostMsr;
    PCVMXAUTOMSR pGuestMsr = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
    for (uint32_t i = 0; i < cMsrs; i++, pHostMsr++, pGuestMsr++)
    {
        /* Verify that the MSRs are paired properly and that the host MSR has the correct value. */
        AssertMsgReturnVoid(pHostMsr->u32Msr == pGuestMsr->u32Msr, ("HostMsr=%#RX32 GuestMsr=%#RX32 cMsrs=%u\n", pHostMsr->u32Msr,
                                                                    pGuestMsr->u32Msr, cMsrs));

        uint64_t u64Msr = ASMRdMsr(pHostMsr->u32Msr);
        AssertMsgReturnVoid(pHostMsr->u64Value == u64Msr, ("u32Msr=%#RX32 VMCS Value=%#RX64 ASMRdMsr=%#RX64 cMsrs=%u\n",
                                                           pHostMsr->u32Msr, pHostMsr->u64Value, u64Msr, cMsrs));

        /* Verify that the permissions are as expected in the MSR bitmap. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
        {
            VMXMSREXITREAD  enmRead;
            VMXMSREXITWRITE enmWrite;
            rc = hmR0VmxGetMsrPermission(pVCpu, pGuestMsr->u32Msr, &enmRead, &enmWrite);
            AssertMsgReturnVoid(rc == VINF_SUCCESS, ("hmR0VmxGetMsrPermission! failed. rc=%Rrc\n", rc));
            if (pGuestMsr->u32Msr == MSR_K6_EFER)
            {
                AssertMsgReturnVoid(enmRead  == VMXMSREXIT_INTERCEPT_READ,  ("Passthru read for EFER!?\n"));
                AssertMsgReturnVoid(enmWrite == VMXMSREXIT_INTERCEPT_WRITE, ("Passthru write for EFER!?\n"));
            }
            else
            {
                AssertMsgReturnVoid(enmRead  == VMXMSREXIT_PASSTHRU_READ,  ("u32Msr=%#RX32 cMsrs=%u No passthru read!\n",
                                                                            pGuestMsr->u32Msr, cMsrs));
                AssertMsgReturnVoid(enmWrite == VMXMSREXIT_PASSTHRU_WRITE, ("u32Msr=%#RX32 cMsrs=%u No passthru write!\n",
                                                                            pGuestMsr->u32Msr, cMsrs));
            }
        }
    }
}
#endif /* VBOX_STRICT */


/**
 * Flushes the TLB using EPT.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure of the calling
 *                          EMT.  Can be NULL depending on @a enmTlbFlush.
 * @param   enmTlbFlush     Type of flush.
 *
 * @remarks Caller is responsible for making sure this function is called only
 *          when NestedPaging is supported and providing @a enmTlbFlush that is
 *          supported by the CPU.
 * @remarks Can be called with interrupts disabled.
 */
static void hmR0VmxFlushEpt(PVMCPU pVCpu, VMXTLBFLUSHEPT enmTlbFlush)
{
    uint64_t au64Descriptor[2];
    if (enmTlbFlush == VMXTLBFLUSHEPT_ALL_CONTEXTS)
        au64Descriptor[0] = 0;
    else
    {
        Assert(pVCpu);
        au64Descriptor[0] = pVCpu->hm.s.vmx.HCPhysEPTP;
    }
    au64Descriptor[1] = 0;                       /* MBZ. Intel spec. 33.3 "VMX Instructions" */

    int rc = VMXR0InvEPT(enmTlbFlush, &au64Descriptor[0]);
    AssertMsg(rc == VINF_SUCCESS,
              ("VMXR0InvEPT %#x %RGv failed with %Rrc\n", enmTlbFlush, pVCpu ? pVCpu->hm.s.vmx.HCPhysEPTP : 0, rc));

    if (   RT_SUCCESS(rc)
        && pVCpu)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushNestedPaging);
}


/**
 * Flushes the TLB using VPID.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure of the calling
 *                          EMT.  Can be NULL depending on @a enmTlbFlush.
 * @param   enmTlbFlush     Type of flush.
 * @param   GCPtr           Virtual address of the page to flush (can be 0 depending
 *                          on @a enmTlbFlush).
 *
 * @remarks Can be called with interrupts disabled.
 */
static void hmR0VmxFlushVpid(PVMCPU pVCpu, VMXTLBFLUSHVPID enmTlbFlush, RTGCPTR GCPtr)
{
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fVpid);

    uint64_t au64Descriptor[2];
    if (enmTlbFlush == VMXTLBFLUSHVPID_ALL_CONTEXTS)
    {
        au64Descriptor[0] = 0;
        au64Descriptor[1] = 0;
    }
    else
    {
        AssertPtr(pVCpu);
        AssertMsg(pVCpu->hm.s.uCurrentAsid != 0, ("VMXR0InvVPID: invalid ASID %lu\n", pVCpu->hm.s.uCurrentAsid));
        AssertMsg(pVCpu->hm.s.uCurrentAsid <= UINT16_MAX, ("VMXR0InvVPID: invalid ASID %lu\n", pVCpu->hm.s.uCurrentAsid));
        au64Descriptor[0] = pVCpu->hm.s.uCurrentAsid;
        au64Descriptor[1] = GCPtr;
    }

    int rc = VMXR0InvVPID(enmTlbFlush, &au64Descriptor[0]);
    AssertMsg(rc == VINF_SUCCESS,
              ("VMXR0InvVPID %#x %u %RGv failed with %Rrc\n", enmTlbFlush, pVCpu ? pVCpu->hm.s.uCurrentAsid : 0, GCPtr, rc));

    if (   RT_SUCCESS(rc)
        && pVCpu)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushAsid);
    NOREF(rc);
}


/**
 * Invalidates a guest page by guest virtual address. Only relevant for
 * EPT/VPID, otherwise there is nothing really to invalidate.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   GCVirt      Guest virtual address of the page to invalidate.
 */
VMMR0DECL(int) VMXR0InvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt)
{
    AssertPtr(pVCpu);
    LogFlowFunc(("pVCpu=%p GCVirt=%RGv\n", pVCpu, GCVirt));

    bool fFlushPending = VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_TLB_FLUSH);
    if (!fFlushPending)
    {
        /*
         * We must invalidate the guest TLB entry in either case, we cannot ignore it even for
         * the EPT case. See @bugref{6043} and @bugref{6177}.
         *
         * Set the VMCPU_FF_TLB_FLUSH force flag and flush before VM-entry in hmR0VmxFlushTLB*()
         * as this function maybe called in a loop with individual addresses.
         */
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        if (pVM->hm.s.vmx.fVpid)
        {
            bool fVpidFlush = RT_BOOL(pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR);

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
            /*
             * Workaround Erratum BV75, AAJ159 and others that affect several Intel CPUs
             * where executing INVVPID outside 64-bit mode does not flush translations of
             * 64-bit linear addresses, see @bugref{6208#c72}.
             */
            if (RT_HI_U32(GCVirt))
                fVpidFlush = false;
#endif

            if (fVpidFlush)
            {
                hmR0VmxFlushVpid(pVCpu, VMXTLBFLUSHVPID_INDIV_ADDR, GCVirt);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbInvlpgVirt);
            }
            else
                VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
        }
        else if (pVM->hm.s.fNestedPaging)
            VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    }

    return VINF_SUCCESS;
}


/**
 * Dummy placeholder for tagged-TLB flush handling before VM-entry. Used in the
 * case where neither EPT nor VPID is supported by the CPU.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCpu            Pointer to the global HM struct.
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbNone(PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu)
{
    AssertPtr(pVCpu);
    AssertPtr(pCpu);

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH);

    Assert(pCpu->idCpu != NIL_RTCPUID);
    pVCpu->hm.s.idLastCpu           = pCpu->idCpu;
    pVCpu->hm.s.cTlbFlushes         = pCpu->cTlbFlushes;
    pVCpu->hm.s.fForceTLBFlush      = false;
    return;
}


/**
 * Flushes the tagged-TLB entries for EPT+VPID CPUs as necessary.
 *
 * @param    pVCpu          The cross context virtual CPU structure.
 * @param    pCpu           Pointer to the global HM CPU struct.
 *
 * @remarks  All references to "ASID" in this function pertains to "VPID" in Intel's
 *           nomenclature. The reason is, to avoid confusion in compare statements
 *           since the host-CPU copies are named "ASID".
 *
 * @remarks  Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbBoth(PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu)
{
#ifdef VBOX_WITH_STATISTICS
    bool fTlbFlushed = false;
# define HMVMX_SET_TAGGED_TLB_FLUSHED()       do { fTlbFlushed = true; } while (0)
# define HMVMX_UPDATE_FLUSH_SKIPPED_STAT()    do { \
                                                if (!fTlbFlushed) \
                                                    STAM_COUNTER_INC(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch); \
                                              } while (0)
#else
# define HMVMX_SET_TAGGED_TLB_FLUSHED()       do { } while (0)
# define HMVMX_UPDATE_FLUSH_SKIPPED_STAT()    do { } while (0)
#endif

    AssertPtr(pCpu);
    AssertPtr(pVCpu);
    Assert(pCpu->idCpu != NIL_RTCPUID);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    AssertMsg(pVM->hm.s.fNestedPaging && pVM->hm.s.vmx.fVpid,
              ("hmR0VmxFlushTaggedTlbBoth cannot be invoked unless NestedPaging & VPID are enabled."
               "fNestedPaging=%RTbool fVpid=%RTbool", pVM->hm.s.fNestedPaging, pVM->hm.s.vmx.fVpid));

    /*
     * Force a TLB flush for the first world-switch if the current CPU differs from the one we
     * ran on last. If the TLB flush count changed, another VM (VCPU rather) has hit the ASID
     * limit while flushing the TLB or the host CPU is online after a suspend/resume, so we
     * cannot reuse the current ASID anymore.
     */
    if (   pVCpu->hm.s.idLastCpu   != pCpu->idCpu
        || pVCpu->hm.s.cTlbFlushes != pCpu->cTlbFlushes)
    {
        ++pCpu->uCurrentAsid;
        if (pCpu->uCurrentAsid >= pVM->hm.s.uMaxAsid)
        {
            pCpu->uCurrentAsid = 1;              /* Wraparound to 1; host uses 0. */
            pCpu->cTlbFlushes++;                 /* All VCPUs that run on this host CPU must use a new VPID. */
            pCpu->fFlushAsidBeforeUse = true;    /* All VCPUs that run on this host CPU must flush their new VPID before use. */
        }

        pVCpu->hm.s.uCurrentAsid = pCpu->uCurrentAsid;
        pVCpu->hm.s.idLastCpu    = pCpu->idCpu;
        pVCpu->hm.s.cTlbFlushes  = pCpu->cTlbFlushes;

        /*
         * Flush by EPT when we get rescheduled to a new host CPU to ensure EPT-only tagged mappings are also
         * invalidated. We don't need to flush-by-VPID here as flushing by EPT covers it. See @bugref{6568}.
         */
        hmR0VmxFlushEpt(pVCpu, pVM->hm.s.vmx.enmTlbFlushEpt);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
        HMVMX_SET_TAGGED_TLB_FLUSHED();
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH);
    }
    else if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))    /* Check for explicit TLB flushes. */
    {
        /*
         * Changes to the EPT paging structure by VMM requires flushing-by-EPT as the CPU
         * creates guest-physical (ie. only EPT-tagged) mappings while traversing the EPT
         * tables when EPT is in use. Flushing-by-VPID will only flush linear (only
         * VPID-tagged) and combined (EPT+VPID tagged) mappings but not guest-physical
         * mappings, see @bugref{6568}.
         *
         * See Intel spec. 28.3.2 "Creating and Using Cached Translation Information".
         */
        hmR0VmxFlushEpt(pVCpu, pVM->hm.s.vmx.enmTlbFlushEpt);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
        HMVMX_SET_TAGGED_TLB_FLUSHED();
    }

    pVCpu->hm.s.fForceTLBFlush = false;
    HMVMX_UPDATE_FLUSH_SKIPPED_STAT();

    Assert(pVCpu->hm.s.idLastCpu == pCpu->idCpu);
    Assert(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes);
    AssertMsg(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes,
              ("Flush count mismatch for cpu %d (%u vs %u)\n", pCpu->idCpu, pVCpu->hm.s.cTlbFlushes, pCpu->cTlbFlushes));
    AssertMsg(pCpu->uCurrentAsid >= 1 && pCpu->uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("Cpu[%u] uCurrentAsid=%u cTlbFlushes=%u pVCpu->idLastCpu=%u pVCpu->cTlbFlushes=%u\n", pCpu->idCpu,
               pCpu->uCurrentAsid, pCpu->cTlbFlushes, pVCpu->hm.s.idLastCpu, pVCpu->hm.s.cTlbFlushes));
    AssertMsg(pVCpu->hm.s.uCurrentAsid >= 1 && pVCpu->hm.s.uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("Cpu[%u] pVCpu->uCurrentAsid=%u\n", pCpu->idCpu, pVCpu->hm.s.uCurrentAsid));

    /* Update VMCS with the VPID. */
    int rc  = VMXWriteVmcs32(VMX_VMCS16_VPID, pVCpu->hm.s.uCurrentAsid);
    AssertRC(rc);

#undef HMVMX_SET_TAGGED_TLB_FLUSHED
}


/**
 * Flushes the tagged-TLB entries for EPT CPUs as necessary.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCpu        Pointer to the global HM CPU struct.
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbEpt(PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu)
{
    AssertPtr(pVCpu);
    AssertPtr(pCpu);
    Assert(pCpu->idCpu != NIL_RTCPUID);
    AssertMsg(pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging, ("hmR0VmxFlushTaggedTlbEpt cannot be invoked without NestedPaging."));
    AssertMsg(!pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fVpid, ("hmR0VmxFlushTaggedTlbEpt cannot be invoked with VPID."));

    /*
     * Force a TLB flush for the first world-switch if the current CPU differs from the one we ran on last.
     * A change in the TLB flush count implies the host CPU is online after a suspend/resume.
     */
    if (   pVCpu->hm.s.idLastCpu   != pCpu->idCpu
        || pVCpu->hm.s.cTlbFlushes != pCpu->cTlbFlushes)
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
    }

    /* Check for explicit TLB flushes. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
    }

    pVCpu->hm.s.idLastCpu   = pCpu->idCpu;
    pVCpu->hm.s.cTlbFlushes = pCpu->cTlbFlushes;

    if (pVCpu->hm.s.fForceTLBFlush)
    {
        hmR0VmxFlushEpt(pVCpu, pVCpu->CTX_SUFF(pVM)->hm.s.vmx.enmTlbFlushEpt);
        pVCpu->hm.s.fForceTLBFlush = false;
    }
}


/**
 * Flushes the tagged-TLB entries for VPID CPUs as necessary.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCpu        Pointer to the global HM CPU struct.
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbVpid(PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu)
{
    AssertPtr(pVCpu);
    AssertPtr(pCpu);
    Assert(pCpu->idCpu != NIL_RTCPUID);
    AssertMsg(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fVpid, ("hmR0VmxFlushTlbVpid cannot be invoked without VPID."));
    AssertMsg(!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging, ("hmR0VmxFlushTlbVpid cannot be invoked with NestedPaging"));

    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one we
     * ran on last. If the TLB flush count changed, another VM (VCPU rather) has hit the ASID
     * limit while flushing the TLB or the host CPU is online after a suspend/resume, so we
     * cannot reuse the current ASID anymore.
     */
    if (   pVCpu->hm.s.idLastCpu   != pCpu->idCpu
        || pVCpu->hm.s.cTlbFlushes != pCpu->cTlbFlushes)
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
    }

    /* Check for explicit TLB flushes. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        /*
         * If we ever support VPID flush combinations other than ALL or SINGLE-context (see
         * hmR0VmxSetupTaggedTlb()) we would need to explicitly flush in this case (add an
         * fExplicitFlush = true here and change the pCpu->fFlushAsidBeforeUse check below to
         * include fExplicitFlush's too) - an obscure corner case.
         */
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
    }

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    pVCpu->hm.s.idLastCpu = pCpu->idCpu;
    if (pVCpu->hm.s.fForceTLBFlush)
    {
        ++pCpu->uCurrentAsid;
        if (pCpu->uCurrentAsid >= pVM->hm.s.uMaxAsid)
        {
            pCpu->uCurrentAsid        = 1;       /* Wraparound to 1; host uses 0 */
            pCpu->cTlbFlushes++;                 /* All VCPUs that run on this host CPU must use a new VPID. */
            pCpu->fFlushAsidBeforeUse = true;    /* All VCPUs that run on this host CPU must flush their new VPID before use. */
        }

        pVCpu->hm.s.fForceTLBFlush = false;
        pVCpu->hm.s.cTlbFlushes    = pCpu->cTlbFlushes;
        pVCpu->hm.s.uCurrentAsid   = pCpu->uCurrentAsid;
        if (pCpu->fFlushAsidBeforeUse)
        {
            if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_SINGLE_CONTEXT)
                hmR0VmxFlushVpid(pVCpu, VMXTLBFLUSHVPID_SINGLE_CONTEXT, 0 /* GCPtr */);
            else if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_ALL_CONTEXTS)
            {
                hmR0VmxFlushVpid(pVCpu, VMXTLBFLUSHVPID_ALL_CONTEXTS, 0 /* GCPtr */);
                pCpu->fFlushAsidBeforeUse = false;
            }
            else
            {
                /* hmR0VmxSetupTaggedTlb() ensures we never get here. Paranoia. */
                AssertMsgFailed(("Unsupported VPID-flush context type.\n"));
            }
        }
    }

    AssertMsg(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes,
              ("Flush count mismatch for cpu %d (%u vs %u)\n", pCpu->idCpu, pVCpu->hm.s.cTlbFlushes, pCpu->cTlbFlushes));
    AssertMsg(pCpu->uCurrentAsid >= 1 && pCpu->uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("Cpu[%u] uCurrentAsid=%u cTlbFlushes=%u pVCpu->idLastCpu=%u pVCpu->cTlbFlushes=%u\n", pCpu->idCpu,
               pCpu->uCurrentAsid, pCpu->cTlbFlushes, pVCpu->hm.s.idLastCpu, pVCpu->hm.s.cTlbFlushes));
    AssertMsg(pVCpu->hm.s.uCurrentAsid >= 1 && pVCpu->hm.s.uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("Cpu[%u] pVCpu->uCurrentAsid=%u\n", pCpu->idCpu, pVCpu->hm.s.uCurrentAsid));

    int rc  = VMXWriteVmcs32(VMX_VMCS16_VPID, pVCpu->hm.s.uCurrentAsid);
    AssertRC(rc);
}


/**
 * Flushes the guest TLB entry based on CPU capabilities.
 *
 * @param   pVCpu     The cross context virtual CPU structure.
 * @param   pCpu      Pointer to the global HM CPU struct.
 */
DECLINLINE(void) hmR0VmxFlushTaggedTlb(PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu)
{
#ifdef HMVMX_ALWAYS_FLUSH_TLB
    VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
#endif
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    switch (pVM->hm.s.vmx.enmTlbFlushType)
    {
        case VMXTLBFLUSHTYPE_EPT_VPID: hmR0VmxFlushTaggedTlbBoth(pVCpu, pCpu); break;
        case VMXTLBFLUSHTYPE_EPT:      hmR0VmxFlushTaggedTlbEpt(pVCpu, pCpu);  break;
        case VMXTLBFLUSHTYPE_VPID:     hmR0VmxFlushTaggedTlbVpid(pVCpu, pCpu); break;
        case VMXTLBFLUSHTYPE_NONE:     hmR0VmxFlushTaggedTlbNone(pVCpu, pCpu); break;
        default:
            AssertMsgFailed(("Invalid flush-tag function identifier\n"));
            break;
    }
    /* Don't assert that VMCPU_FF_TLB_FLUSH should no longer be pending. It can be set by other EMTs. */
}


/**
 * Sets up the appropriate tagged TLB-flush level and handler for flushing guest
 * TLB entries from the host TLB before VM-entry.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
static int hmR0VmxSetupTaggedTlb(PVM pVM)
{
    /*
     * Determine optimal flush type for Nested Paging.
     * We cannot ignore EPT if no suitable flush-types is supported by the CPU as we've already setup unrestricted
     * guest execution (see hmR3InitFinalizeR0()).
     */
    if (pVM->hm.s.fNestedPaging)
    {
        if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT)
        {
            if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT)
                pVM->hm.s.vmx.enmTlbFlushEpt = VMXTLBFLUSHEPT_SINGLE_CONTEXT;
            else if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS)
                pVM->hm.s.vmx.enmTlbFlushEpt = VMXTLBFLUSHEPT_ALL_CONTEXTS;
            else
            {
                /* Shouldn't happen. EPT is supported but no suitable flush-types supported. */
                pVM->hm.s.vmx.enmTlbFlushEpt = VMXTLBFLUSHEPT_NOT_SUPPORTED;
                pVM->aCpus[0].hm.s.u32HMError = VMX_UFC_EPT_FLUSH_TYPE_UNSUPPORTED;
                return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
            }

            /* Make sure the write-back cacheable memory type for EPT is supported. */
            if (RT_UNLIKELY(!(pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB)))
            {
                pVM->hm.s.vmx.enmTlbFlushEpt = VMXTLBFLUSHEPT_NOT_SUPPORTED;
                pVM->aCpus[0].hm.s.u32HMError = VMX_UFC_EPT_MEM_TYPE_NOT_WB;
                return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
            }

            /* EPT requires a page-walk length of 4. */
            if (RT_UNLIKELY(!(pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_PAGE_WALK_LENGTH_4)))
            {
                pVM->hm.s.vmx.enmTlbFlushEpt = VMXTLBFLUSHEPT_NOT_SUPPORTED;
                pVM->aCpus[0].hm.s.u32HMError = VMX_UFC_EPT_PAGE_WALK_LENGTH_UNSUPPORTED;
                return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
            }
        }
        else
        {
            /* Shouldn't happen. EPT is supported but INVEPT instruction is not supported. */
            pVM->hm.s.vmx.enmTlbFlushEpt = VMXTLBFLUSHEPT_NOT_SUPPORTED;
            pVM->aCpus[0].hm.s.u32HMError = VMX_UFC_EPT_INVEPT_UNAVAILABLE;
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }
    }

    /*
     * Determine optimal flush type for VPID.
     */
    if (pVM->hm.s.vmx.fVpid)
    {
        if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID)
        {
            if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT)
                pVM->hm.s.vmx.enmTlbFlushVpid = VMXTLBFLUSHVPID_SINGLE_CONTEXT;
            else if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS)
                pVM->hm.s.vmx.enmTlbFlushVpid = VMXTLBFLUSHVPID_ALL_CONTEXTS;
            else
            {
                /* Neither SINGLE nor ALL-context flush types for VPID is supported by the CPU. Ignore VPID capability. */
                if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR)
                    LogRelFunc(("Only INDIV_ADDR supported. Ignoring VPID.\n"));
                if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS)
                    LogRelFunc(("Only SINGLE_CONTEXT_RETAIN_GLOBALS supported. Ignoring VPID.\n"));
                pVM->hm.s.vmx.enmTlbFlushVpid = VMXTLBFLUSHVPID_NOT_SUPPORTED;
                pVM->hm.s.vmx.fVpid = false;
            }
        }
        else
        {
            /*  Shouldn't happen. VPID is supported but INVVPID is not supported by the CPU. Ignore VPID capability. */
            Log4Func(("VPID supported without INVEPT support. Ignoring VPID.\n"));
            pVM->hm.s.vmx.enmTlbFlushVpid = VMXTLBFLUSHVPID_NOT_SUPPORTED;
            pVM->hm.s.vmx.fVpid = false;
        }
    }

    /*
     * Setup the handler for flushing tagged-TLBs.
     */
    if (pVM->hm.s.fNestedPaging && pVM->hm.s.vmx.fVpid)
        pVM->hm.s.vmx.enmTlbFlushType = VMXTLBFLUSHTYPE_EPT_VPID;
    else if (pVM->hm.s.fNestedPaging)
        pVM->hm.s.vmx.enmTlbFlushType = VMXTLBFLUSHTYPE_EPT;
    else if (pVM->hm.s.vmx.fVpid)
        pVM->hm.s.vmx.enmTlbFlushType = VMXTLBFLUSHTYPE_VPID;
    else
        pVM->hm.s.vmx.enmTlbFlushType = VMXTLBFLUSHTYPE_NONE;
    return VINF_SUCCESS;
}


/**
 * Sets up pin-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks We don't really care about optimizing vmwrites here as it's done only
 *          once per VM and hence we don't care about VMCS-field cache comparisons.
 */
static int hmR0VmxSetupPinCtls(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    uint32_t       fVal = pVM->hm.s.vmx.Msrs.VmxPinCtls.n.disallowed0;   /* Bits set here must always be set. */
    uint32_t const fZap = pVM->hm.s.vmx.Msrs.VmxPinCtls.n.allowed1;      /* Bits cleared here must always be cleared. */

    fVal |= VMX_VMCS_CTRL_PIN_EXEC_EXT_INT_EXIT              /* External interrupts cause a VM-exit. */
          | VMX_VMCS_CTRL_PIN_EXEC_NMI_EXIT;                 /* Non-maskable interrupts (NMIs) cause a VM-exit. */

    if (pVM->hm.s.vmx.Msrs.VmxPinCtls.n.allowed1 & VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI)
        fVal |= VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI;         /* Use virtual NMIs and virtual-NMI blocking features. */

    /* Enable the VMX preemption timer. */
    if (pVM->hm.s.vmx.fUsePreemptTimer)
    {
        Assert(pVM->hm.s.vmx.Msrs.VmxPinCtls.n.allowed1 & VMX_VMCS_CTRL_PIN_EXEC_PREEMPT_TIMER);
        fVal |= VMX_VMCS_CTRL_PIN_EXEC_PREEMPT_TIMER;
    }

#if 0
    /* Enable posted-interrupt processing. */
    if (pVM->hm.s.fPostedIntrs)
    {
        Assert(pVM->hm.s.vmx.Msrs.VmxPinCtls.n.allowed1 & VMX_VMCS_CTRL_PIN_EXEC_POSTED_INTR);
        Assert(pVM->hm.s.vmx.Msrs.VmxExit.n.allowed1 & VMX_VMCS_CTRL_EXIT_ACK_EXT_INT);
        fVal |= VMX_VMCS_CTRL_PIN_EXEC_POSTED_INTR;
    }
#endif

    if ((fVal & fZap) != fVal)
    {
        LogRelFunc(("Invalid pin-based VM-execution controls combo! Cpu=%#RX64 fVal=%#RX64 fZap=%#RX64\n",
                    pVM->hm.s.vmx.Msrs.VmxPinCtls.n.disallowed0, fVal, fZap));
        pVCpu->hm.s.u32HMError = VMX_UFC_CTRL_PIN_EXEC;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Commit it to the VMCS and update our cache. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PIN_EXEC, fVal);
    AssertRCReturn(rc, rc);
    pVCpu->hm.s.vmx.u32PinCtls = fVal;

    return VINF_SUCCESS;
}


/**
 * Sets up secondary processor-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks We don't really care about optimizing vmwrites here as it's done only
 *          once per VM and hence we don't care about VMCS-field cache comparisons.
 */
static int hmR0VmxSetupProcCtls2(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    uint32_t       fVal = pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.disallowed0; /* Bits set here must be set in the VMCS. */
    uint32_t const fZap = pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1;    /* Bits cleared here must be cleared in the VMCS. */

    /* WBINVD causes a VM-exit. */
    if (pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT;

    /* Enable EPT (aka nested-paging). */
    if (pVM->hm.s.fNestedPaging)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_EPT;

    /*
     * Enable the INVPCID instruction if supported by the hardware and we expose
     * it to the guest. Without this, guest executing INVPCID would cause a #UD.
     */
    if (   (pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_INVPCID)
        && pVM->cpum.ro.GuestFeatures.fInvpcid)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_INVPCID;

    /* Enable VPID. */
    if (pVM->hm.s.vmx.fVpid)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_VPID;

    /* Enable Unrestricted guest execution. */
    if (pVM->hm.s.vmx.fUnrestrictedGuest)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_UNRESTRICTED_GUEST;

#if 0
    if (pVM->hm.s.fVirtApicRegs)
    {
        /* Enable APIC-register virtualization. */
        Assert(pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_APIC_REG_VIRT);
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_APIC_REG_VIRT;

        /* Enable virtual-interrupt delivery. */
        Assert(pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_INTR_DELIVERY);
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_VIRT_INTR_DELIVERY;
    }
#endif

    /* Enable Virtual-APIC page accesses if supported by the CPU. This is where the TPR shadow resides. */
    /** @todo VIRT_X2APIC support, it's mutually exclusive with this. So must be
     *        done dynamically. */
    if (pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
    {
        Assert(pVM->hm.s.vmx.HCPhysApicAccess);
        Assert(!(pVM->hm.s.vmx.HCPhysApicAccess & 0xfff));    /* Bits 11:0 MBZ. */
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC;           /* Virtualize APIC accesses. */
        int rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL, pVM->hm.s.vmx.HCPhysApicAccess);
        AssertRCReturn(rc, rc);
    }

    /* Enable RDTSCP. */
    if (pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP;

    /* Enable Pause-Loop exiting. */
    if (   pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT
        && pVM->hm.s.vmx.cPleGapTicks
        && pVM->hm.s.vmx.cPleWindowTicks)
    {
        fVal |= VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT;

        int rc  = VMXWriteVmcs32(VMX_VMCS32_CTRL_PLE_GAP, pVM->hm.s.vmx.cPleGapTicks);
        rc     |= VMXWriteVmcs32(VMX_VMCS32_CTRL_PLE_WINDOW, pVM->hm.s.vmx.cPleWindowTicks);
        AssertRCReturn(rc, rc);
    }

    if ((fVal & fZap) != fVal)
    {
        LogRelFunc(("Invalid secondary processor-based VM-execution controls combo! cpu=%#RX64 fVal=%#RX64 fZap=%#RX64\n",
                    pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.disallowed0, fVal, fZap));
        pVCpu->hm.s.u32HMError = VMX_UFC_CTRL_PROC_EXEC2;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Commit it to the VMCS and update our cache. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, fVal);
    AssertRCReturn(rc, rc);
    pVCpu->hm.s.vmx.u32ProcCtls2 = fVal;

    return VINF_SUCCESS;
}


/**
 * Sets up processor-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks We don't really care about optimizing vmwrites here as it's done only
 *          once per VM and hence we don't care about VMCS-field cache comparisons.
 */
static int hmR0VmxSetupProcCtls(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    uint32_t       fVal = pVM->hm.s.vmx.Msrs.VmxProcCtls.n.disallowed0;  /* Bits set here must be set in the VMCS. */
    uint32_t const fZap = pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1;     /* Bits cleared here must be cleared in the VMCS. */

    fVal |= VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT                      /* HLT causes a VM-exit. */
          | VMX_VMCS_CTRL_PROC_EXEC_USE_TSC_OFFSETTING            /* Use TSC-offsetting. */
          | VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT                   /* MOV DRx causes a VM-exit. */
          | VMX_VMCS_CTRL_PROC_EXEC_UNCOND_IO_EXIT                /* All IO instructions cause a VM-exit. */
          | VMX_VMCS_CTRL_PROC_EXEC_RDPMC_EXIT                    /* RDPMC causes a VM-exit. */
          | VMX_VMCS_CTRL_PROC_EXEC_MONITOR_EXIT                  /* MONITOR causes a VM-exit. */
          | VMX_VMCS_CTRL_PROC_EXEC_MWAIT_EXIT;                   /* MWAIT causes a VM-exit. */

    /* We toggle VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT later, check if it's not -always- needed to be set or clear. */
    if (   !(pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT)
        ||  (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.disallowed0 & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT))
    {
        LogRelFunc(("Unsupported VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT combo!"));
        pVCpu->hm.s.u32HMError = VMX_UFC_CTRL_PROC_MOV_DRX_EXIT;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Without Nested Paging, INVLPG (also affects INVPCID) and MOV CR3 instructions should cause VM-exits. */
    if (!pVM->hm.s.fNestedPaging)
    {
        Assert(!pVM->hm.s.vmx.fUnrestrictedGuest);                /* Paranoia. */
        fVal |= VMX_VMCS_CTRL_PROC_EXEC_INVLPG_EXIT
              | VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT
              | VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT;
    }

    /* Use TPR shadowing if supported by the CPU. */
    if (   PDMHasApic(pVM)
        && pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
    {
        Assert(pVCpu->hm.s.vmx.HCPhysVirtApic);
        Assert(!(pVCpu->hm.s.vmx.HCPhysVirtApic & 0xfff));        /* Bits 11:0 MBZ. */
        int rc  = VMXWriteVmcs32(VMX_VMCS32_CTRL_TPR_THRESHOLD, 0);
        rc     |= VMXWriteVmcs64(VMX_VMCS64_CTRL_VAPIC_PAGEADDR_FULL, pVCpu->hm.s.vmx.HCPhysVirtApic);
        AssertRCReturn(rc, rc);

        fVal |= VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW;           /* CR8 reads from the Virtual-APIC page. */
                                                                  /* CR8 writes cause a VM-exit based on TPR threshold. */
        Assert(!(fVal & VMX_VMCS_CTRL_PROC_EXEC_CR8_STORE_EXIT));
        Assert(!(fVal & VMX_VMCS_CTRL_PROC_EXEC_CR8_LOAD_EXIT));
    }
    else
    {
        /*
         * Some 32-bit CPUs do not support CR8 load/store exiting as MOV CR8 is invalid on 32-bit Intel CPUs.
         * Set this control only for 64-bit guests.
         */
        if (pVM->hm.s.fAllow64BitGuests)
        {
            fVal |= VMX_VMCS_CTRL_PROC_EXEC_CR8_STORE_EXIT        /* CR8 reads cause a VM-exit. */
                  | VMX_VMCS_CTRL_PROC_EXEC_CR8_LOAD_EXIT;        /* CR8 writes cause a VM-exit. */
        }
    }

    /* Use MSR-bitmaps if supported by the CPU. */
    if (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
    {
        fVal |= VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS;

        Assert(pVCpu->hm.s.vmx.HCPhysMsrBitmap);
        Assert(!(pVCpu->hm.s.vmx.HCPhysMsrBitmap & 0xfff));       /* Bits 11:0 MBZ. */
        int rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_MSR_BITMAP_FULL, pVCpu->hm.s.vmx.HCPhysMsrBitmap);
        AssertRCReturn(rc, rc);

        /*
         * The guest can access the following MSRs (read, write) without causing VM-exits; they are loaded/stored
         * automatically using dedicated fields in the VMCS.
         */
        hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_CS,  VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_ESP, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_EIP, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_GS_BASE,        VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_FS_BASE,        VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
#if HC_ARCH_BITS == 64
        /*
         * Set passthru permissions for the following MSRs (mandatory for VT-x) required for 64-bit guests.
         */
        if (pVM->hm.s.fAllow64BitGuests)
        {
            hmR0VmxSetMsrPermission(pVCpu, MSR_K8_LSTAR,          VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
            hmR0VmxSetMsrPermission(pVCpu, MSR_K6_STAR,           VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
            hmR0VmxSetMsrPermission(pVCpu, MSR_K8_SF_MASK,        VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
            hmR0VmxSetMsrPermission(pVCpu, MSR_K8_KERNEL_GS_BASE, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        }
#endif
        /*
         * The IA32_PRED_CMD MSR is write-only and has no state associated with it. We never need to intercept
         * access (writes need to be executed without exiting, reds will #GP-fault anyway).
         */
        if (pVM->cpum.ro.GuestFeatures.fIbpb)
            hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_PRED_CMD,     VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);

        /* Though MSR_IA32_PERF_GLOBAL_CTRL is saved/restored lazily, we want intercept reads/write to it for now. */
    }

    /* Use the secondary processor-based VM-execution controls if supported by the CPU. */
    if (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
        fVal |= VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL;

    if ((fVal & fZap) != fVal)
    {
        LogRelFunc(("Invalid processor-based VM-execution controls combo! cpu=%#RX64 fVal=%#RX64 fZap=%#RX64\n",
                    pVM->hm.s.vmx.Msrs.VmxProcCtls.n.disallowed0, fVal, fZap));
        pVCpu->hm.s.u32HMError = VMX_UFC_CTRL_PROC_EXEC;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Commit it to the VMCS and update our cache. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, fVal);
    AssertRCReturn(rc, rc);
    pVCpu->hm.s.vmx.u32ProcCtls = fVal;

    /* Set up secondary processor-based VM-execution controls if the CPU supports it. */
    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
        return hmR0VmxSetupProcCtls2(pVCpu);

    /* Sanity check, should not really happen. */
    if (RT_UNLIKELY(pVM->hm.s.vmx.fUnrestrictedGuest))
    {
        LogRelFunc(("Unrestricted Guest enabled when secondary processor-based VM-execution controls not available\n"));
        pVCpu->hm.s.u32HMError = VMX_UFC_INVALID_UX_COMBO;
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Old CPUs without secondary processor-based VM-execution controls would end up here. */
    return VINF_SUCCESS;
}


/**
 * Sets up miscellaneous (everything other than Pin & Processor-based
 * VM-execution) control fields in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int hmR0VmxSetupMiscCtls(PVMCPU pVCpu)
{
    AssertPtr(pVCpu);

    int rc = VERR_GENERAL_FAILURE;

    /* All fields are zero-initialized during allocation; but don't remove the commented block below. */
#if 0
    /* All CR3 accesses cause VM-exits. Later we optimize CR3 accesses (see hmR0VmxExportGuestCR3AndCR4())*/
    rc  = VMXWriteVmcs32(VMX_VMCS32_CTRL_CR3_TARGET_COUNT, 0);
    rc |= VMXWriteVmcs64(VMX_VMCS64_CTRL_TSC_OFFSET_FULL, 0);

    /*
     * Set MASK & MATCH to 0. VMX checks if GuestPFErrCode & MASK == MATCH. If equal (in our case it always is)
     * and if the X86_XCPT_PF bit in the exception bitmap is set it causes a VM-exit, if clear doesn't cause an exit.
     * We thus use the exception bitmap to control it rather than use both.
     */
    rc  = VMXWriteVmcs32(VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK, 0);
    rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH, 0);

    /* All IO & IOIO instructions cause VM-exits. */
    rc |= VMXWriteVmcs64(VMX_VMCS64_CTRL_IO_BITMAP_A_FULL, 0);
    rc |= VMXWriteVmcs64(VMX_VMCS64_CTRL_IO_BITMAP_B_FULL, 0);

    /* Initialize the MSR-bitmap area. */
    rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT, 0);
    rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT, 0);
    rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT,  0);
    AssertRCReturn(rc, rc);
#endif

    /* Setup MSR auto-load/store area. */
    Assert(pVCpu->hm.s.vmx.HCPhysGuestMsr);
    Assert(!(pVCpu->hm.s.vmx.HCPhysGuestMsr & 0xf));    /* Lower 4 bits MBZ. */
    rc  = VMXWriteVmcs64(VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL, pVCpu->hm.s.vmx.HCPhysGuestMsr);
    rc |= VMXWriteVmcs64(VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL, pVCpu->hm.s.vmx.HCPhysGuestMsr);
    AssertRCReturn(rc, rc);

    Assert(pVCpu->hm.s.vmx.HCPhysHostMsr);
    Assert(!(pVCpu->hm.s.vmx.HCPhysHostMsr & 0xf));     /* Lower 4 bits MBZ. */
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL,  pVCpu->hm.s.vmx.HCPhysHostMsr);
    AssertRCReturn(rc, rc);

    /* Set VMCS link pointer. Reserved for future use, must be -1. Intel spec. 24.4 "Guest-State Area". */
    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL, UINT64_C(0xffffffffffffffff));
    AssertRCReturn(rc, rc);

    /* All fields are zero-initialized during allocation; but don't remove the commented block below. */
#if 0
    /* Setup debug controls */
    rc  = VMXWriteVmcs64(VMX_VMCS64_GUEST_DEBUGCTL_FULL, 0);
    rc |= VMXWriteVmcs32(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
    AssertRCReturn(rc, rc);
#endif

    return rc;
}


/**
 * Sets up the initial exception bitmap in the VMCS based on static conditions.
 *
 * We shall setup those exception intercepts that don't change during the
 * lifetime of the VM here. The rest are done dynamically while loading the
 * guest state.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int hmR0VmxInitXcptBitmap(PVMCPU pVCpu)
{
    AssertPtr(pVCpu);

    uint32_t uXcptBitmap;

    /* Must always intercept #AC to prevent the guest from hanging the CPU. */
    uXcptBitmap = RT_BIT_32(X86_XCPT_AC);

    /* Because we need to maintain the DR6 state even when intercepting DRx reads
       and writes, and because recursive #DBs can cause the CPU hang, we must always
       intercept #DB. */
    uXcptBitmap |= RT_BIT_32(X86_XCPT_DB);

    /* Without Nested Paging, #PF must cause a VM-exit so we can sync our shadow page tables. */
    if (!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging)
        uXcptBitmap |= RT_BIT(X86_XCPT_PF);

    /* Commit it to the VMCS. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, uXcptBitmap);
    AssertRCReturn(rc, rc);

    /* Update our cache of the exception bitmap. */
    pVCpu->hm.s.vmx.u32XcptBitmap = uXcptBitmap;
    return VINF_SUCCESS;
}


/**
 * Does per-VM VT-x initialization.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 */
VMMR0DECL(int) VMXR0InitVM(PVM pVM)
{
    LogFlowFunc(("pVM=%p\n", pVM));

    int rc = hmR0VmxStructsAlloc(pVM);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("hmR0VmxStructsAlloc failed! rc=%Rrc\n", rc));
        return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Does per-VM VT-x termination.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR0DECL(int) VMXR0TermVM(PVM pVM)
{
    LogFlowFunc(("pVM=%p\n", pVM));

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    if (pVM->hm.s.vmx.hMemObjScratch != NIL_RTR0MEMOBJ)
        ASMMemZero32(pVM->hm.s.vmx.pvScratch, PAGE_SIZE);
#endif
    hmR0VmxStructsFree(pVM);
    return VINF_SUCCESS;
}


/**
 * Sets up the VM for execution under VT-x.
 * This function is only called once per-VM during initialization.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR0DECL(int) VMXR0SetupVM(PVM pVM)
{
    AssertPtrReturn(pVM, VERR_INVALID_PARAMETER);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    LogFlowFunc(("pVM=%p\n", pVM));

    /*
     * Without UnrestrictedGuest, pRealModeTSS and pNonPagingModeEPTPageTable *must* always be
     * allocated. We no longer support the highly unlikely case of UnrestrictedGuest without
     * pRealModeTSS, see hmR3InitFinalizeR0Intel().
     */
    if (   !pVM->hm.s.vmx.fUnrestrictedGuest
        &&  (   !pVM->hm.s.vmx.pNonPagingModeEPTPageTable
             || !pVM->hm.s.vmx.pRealModeTSS))
    {
        LogRelFunc(("Invalid real-on-v86 state.\n"));
        return VERR_INTERNAL_ERROR;
    }

    /* Initialize these always, see hmR3InitFinalizeR0().*/
    pVM->hm.s.vmx.enmTlbFlushEpt  = VMXTLBFLUSHEPT_NONE;
    pVM->hm.s.vmx.enmTlbFlushVpid = VMXTLBFLUSHVPID_NONE;

    /* Setup the tagged-TLB flush handlers. */
    int rc = hmR0VmxSetupTaggedTlb(pVM);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("hmR0VmxSetupTaggedTlb failed! rc=%Rrc\n", rc));
        return rc;
    }

    /* Check if we can use the VMCS controls for swapping the EFER MSR. */
    Assert(!pVM->hm.s.vmx.fSupportsVmcsEfer);
#if HC_ARCH_BITS == 64
    if (   (pVM->hm.s.vmx.Msrs.VmxEntry.n.allowed1 & VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_EFER_MSR)
        && (pVM->hm.s.vmx.Msrs.VmxExit.n.allowed1  & VMX_VMCS_CTRL_EXIT_LOAD_HOST_EFER_MSR)
        && (pVM->hm.s.vmx.Msrs.VmxExit.n.allowed1  & VMX_VMCS_CTRL_EXIT_SAVE_GUEST_EFER_MSR))
    {
        pVM->hm.s.vmx.fSupportsVmcsEfer = true;
    }
#endif

    /* At least verify VMX is enabled, since we can't check if we're in VMX root mode without #GP'ing. */
    RTCCUINTREG uHostCR4 = ASMGetCR4();
    if (RT_UNLIKELY(!(uHostCR4 & X86_CR4_VMXE)))
        return VERR_VMX_NOT_IN_VMX_ROOT_MODE;

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);
        AssertPtr(pVCpu->hm.s.vmx.pvVmcs);

        /* Log the VCPU pointers, useful for debugging SMP VMs. */
        Log4Func(("pVCpu=%p idCpu=%RU32\n", pVCpu, pVCpu->idCpu));

        /* Set revision dword at the beginning of the VMCS structure. */
        *(uint32_t *)pVCpu->hm.s.vmx.pvVmcs = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hm.s.vmx.Msrs.u64BasicInfo);

        /* Initialize our VMCS region in memory, set the VMCS launch state to "clear". */
        rc  = VMXClearVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: VMXClearVmcs failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

        /* Load this VMCS as the current VMCS. */
        rc = VMXActivateVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: VMXActivateVmcs failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

        rc = hmR0VmxSetupPinCtls(pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxSetupPinCtls failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

        rc = hmR0VmxSetupProcCtls(pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxSetupProcCtls failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

        rc = hmR0VmxSetupMiscCtls(pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxSetupMiscCtls failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

        rc = hmR0VmxInitXcptBitmap(pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxInitXcptBitmap failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

#if HC_ARCH_BITS == 32
        rc = hmR0VmxInitVmcsReadCache(pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxInitVmcsReadCache failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);
#endif

        /* Re-sync the CPU's internal data into our VMCS memory region & reset the launch state to "clear". */
        rc = VMXClearVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: VMXClearVmcs(2) failed! rc=%Rrc\n", rc),
                                    hmR0VmxUpdateErrorRecord(pVCpu, rc), rc);

        pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_CLEAR;

        hmR0VmxUpdateErrorRecord(pVCpu, rc);
    }

    return VINF_SUCCESS;
}


/**
 * Saves the host control registers (CR0, CR3, CR4) into the host-state area in
 * the VMCS.
 *
 * @returns VBox status code.
 */
static int hmR0VmxExportHostControlRegs(void)
{
    RTCCUINTREG uReg = ASMGetCR0();
    int rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_CR0, uReg);
    AssertRCReturn(rc, rc);

    uReg = ASMGetCR3();
    rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_CR3, uReg);
    AssertRCReturn(rc, rc);

    uReg = ASMGetCR4();
    rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_CR4, uReg);
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Saves the host segment registers and GDTR, IDTR, (TR, GS and FS bases) into
 * the host-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int hmR0VmxExportHostSegmentRegs(PVMCPU pVCpu)
{
#if HC_ARCH_BITS == 64
/**
 * Macro for adjusting host segment selectors to satisfy VT-x's VM-entry
 * requirements. See hmR0VmxExportHostSegmentRegs().
 */
# define VMXLOCAL_ADJUST_HOST_SEG(seg, selValue)  \
    if ((selValue) & (X86_SEL_RPL | X86_SEL_LDT)) \
    { \
        bool fValidSelector = true; \
        if ((selValue) & X86_SEL_LDT) \
        { \
            uint32_t uAttr = ASMGetSegAttr((selValue)); \
            fValidSelector = RT_BOOL(uAttr != UINT32_MAX && (uAttr & X86_DESC_P)); \
        } \
        if (fValidSelector) \
        { \
            pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_SEL_##seg; \
            pVCpu->hm.s.vmx.RestoreHost.uHostSel##seg = (selValue); \
        } \
        (selValue) = 0; \
    }

    /*
     * If we've executed guest code using VT-x, the host-state bits will be messed up. We
     * should -not- save the messed up state without restoring the original host-state,
     * see @bugref{7240}.
     *
     * This apparently can happen (most likely the FPU changes), deal with it rather than
     * asserting. Was observed booting Solaris 10u10 32-bit guest.
     */
    if (   (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_REQUIRED)
        && (pVCpu->hm.s.vmx.fRestoreHostFlags & ~VMX_RESTORE_HOST_REQUIRED))
    {
        Log4Func(("Restoring Host State: fRestoreHostFlags=%#RX32 HostCpuId=%u\n", pVCpu->hm.s.vmx.fRestoreHostFlags,
                  pVCpu->idCpu));
        VMXRestoreHostState(pVCpu->hm.s.vmx.fRestoreHostFlags, &pVCpu->hm.s.vmx.RestoreHost);
    }
    pVCpu->hm.s.vmx.fRestoreHostFlags = 0;
#else
    RT_NOREF(pVCpu);
#endif

    /*
     * Host DS, ES, FS and GS segment registers.
     */
#if HC_ARCH_BITS == 64
    RTSEL uSelDS = ASMGetDS();
    RTSEL uSelES = ASMGetES();
    RTSEL uSelFS = ASMGetFS();
    RTSEL uSelGS = ASMGetGS();
#else
    RTSEL uSelDS = 0;
    RTSEL uSelES = 0;
    RTSEL uSelFS = 0;
    RTSEL uSelGS = 0;
#endif

    /*
     * Host CS and SS segment registers.
     */
    RTSEL uSelCS = ASMGetCS();
    RTSEL uSelSS = ASMGetSS();

    /*
     * Host TR segment register.
     */
    RTSEL uSelTR = ASMGetTR();

#if HC_ARCH_BITS == 64
    /*
     * Determine if the host segment registers are suitable for VT-x. Otherwise use zero to
     * gain VM-entry and restore them before we get preempted.
     *
     * See Intel spec. 26.2.3 "Checks on Host Segment and Descriptor-Table Registers".
     */
    VMXLOCAL_ADJUST_HOST_SEG(DS, uSelDS);
    VMXLOCAL_ADJUST_HOST_SEG(ES, uSelES);
    VMXLOCAL_ADJUST_HOST_SEG(FS, uSelFS);
    VMXLOCAL_ADJUST_HOST_SEG(GS, uSelGS);
# undef VMXLOCAL_ADJUST_HOST_SEG
#endif

    /* Verification based on Intel spec. 26.2.3 "Checks on Host Segment and Descriptor-Table Registers"  */
    Assert(!(uSelCS & X86_SEL_RPL)); Assert(!(uSelCS & X86_SEL_LDT));
    Assert(!(uSelSS & X86_SEL_RPL)); Assert(!(uSelSS & X86_SEL_LDT));
    Assert(!(uSelDS & X86_SEL_RPL)); Assert(!(uSelDS & X86_SEL_LDT));
    Assert(!(uSelES & X86_SEL_RPL)); Assert(!(uSelES & X86_SEL_LDT));
    Assert(!(uSelFS & X86_SEL_RPL)); Assert(!(uSelFS & X86_SEL_LDT));
    Assert(!(uSelGS & X86_SEL_RPL)); Assert(!(uSelGS & X86_SEL_LDT));
    Assert(!(uSelTR & X86_SEL_RPL)); Assert(!(uSelTR & X86_SEL_LDT));
    Assert(uSelCS);
    Assert(uSelTR);

    /* Assertion is right but we would not have updated u32ExitCtls yet. */
#if 0
    if (!(pVCpu->hm.s.vmx.u32ExitCtls & VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE))
        Assert(uSelSS != 0);
#endif

    /* Write these host selector fields into the host-state area in the VMCS. */
    int rc  = VMXWriteVmcs32(VMX_VMCS16_HOST_CS_SEL, uSelCS);
    rc     |= VMXWriteVmcs32(VMX_VMCS16_HOST_SS_SEL, uSelSS);
#if HC_ARCH_BITS == 64
    rc     |= VMXWriteVmcs32(VMX_VMCS16_HOST_DS_SEL, uSelDS);
    rc     |= VMXWriteVmcs32(VMX_VMCS16_HOST_ES_SEL, uSelES);
    rc     |= VMXWriteVmcs32(VMX_VMCS16_HOST_FS_SEL, uSelFS);
    rc     |= VMXWriteVmcs32(VMX_VMCS16_HOST_GS_SEL, uSelGS);
#else
    NOREF(uSelDS);
    NOREF(uSelES);
    NOREF(uSelFS);
    NOREF(uSelGS);
#endif
    rc     |= VMXWriteVmcs32(VMX_VMCS16_HOST_TR_SEL, uSelTR);
    AssertRCReturn(rc, rc);

    /*
     * Host GDTR and IDTR.
     */
    RTGDTR Gdtr;
    RTIDTR Idtr;
    RT_ZERO(Gdtr);
    RT_ZERO(Idtr);
    ASMGetGDTR(&Gdtr);
    ASMGetIDTR(&Idtr);
    rc  = VMXWriteVmcsHstN(VMX_VMCS_HOST_GDTR_BASE, Gdtr.pGdt);
    rc |= VMXWriteVmcsHstN(VMX_VMCS_HOST_IDTR_BASE, Idtr.pIdt);
    AssertRCReturn(rc, rc);

#if HC_ARCH_BITS == 64
    /*
     * Determine if we need to manually need to restore the GDTR and IDTR limits as VT-x zaps
     * them to the maximum limit (0xffff) on every VM-exit.
     */
    if (Gdtr.cbGdt != 0xffff)
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_GDTR;

    /*
     * IDT limit is effectively capped at 0xfff. (See Intel spec. 6.14.1 "64-Bit Mode IDT" and
     * Intel spec. 6.2 "Exception and Interrupt Vectors".)  Therefore if the host has the limit
     * as 0xfff, VT-x bloating the limit to 0xffff shouldn't cause any different CPU behavior.
     * However, several hosts either insists on 0xfff being the limit (Windows Patch Guard) or
     * uses the limit for other purposes (darwin puts the CPU ID in there but botches sidt
     * alignment in at least one consumer).  So, we're only allowing the IDTR.LIMIT to be left
     * at 0xffff on hosts where we are sure it won't cause trouble.
     */
# if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
    if (Idtr.cbIdt <  0x0fff)
# else
    if (Idtr.cbIdt != 0xffff)
# endif
    {
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_IDTR;
        AssertCompile(sizeof(Idtr) == sizeof(X86XDTR64));
        memcpy(&pVCpu->hm.s.vmx.RestoreHost.HostIdtr, &Idtr, sizeof(X86XDTR64));
    }
#endif

    /*
     * Host TR base. Verify that TR selector doesn't point past the GDT. Masking off the TI
     * and RPL bits is effectively what the CPU does for "scaling by 8". TI is always 0 and
     * RPL should be too in most cases.
     */
    AssertMsgReturn((uSelTR | X86_SEL_RPL_LDT) <= Gdtr.cbGdt,
                    ("TR selector exceeds limit. TR=%RTsel cbGdt=%#x\n", uSelTR, Gdtr.cbGdt), VERR_VMX_INVALID_HOST_STATE);

    PCX86DESCHC pDesc = (PCX86DESCHC)(Gdtr.pGdt + (uSelTR & X86_SEL_MASK));
#if HC_ARCH_BITS == 64
    uintptr_t uTRBase = X86DESC64_BASE(pDesc);

    /*
     * VT-x unconditionally restores the TR limit to 0x67 and type to 11 (32-bit busy TSS) on
     * all VM-exits. The type is the same for 64-bit busy TSS[1]. The limit needs manual
     * restoration if the host has something else. Task switching is not supported in 64-bit
     * mode[2], but the limit still matters as IOPM is supported in 64-bit mode. Restoring the
     * limit lazily while returning to ring-3 is safe because IOPM is not applicable in ring-0.
     *
     * [1] See Intel spec. 3.5 "System Descriptor Types".
     * [2] See Intel spec. 7.2.3 "TSS Descriptor in 64-bit mode".
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(pDesc->System.u4Type == 11);
    if (   pDesc->System.u16LimitLow != 0x67
        || pDesc->System.u4LimitHigh)
    {
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_SEL_TR;
        /* If the host has made GDT read-only, we would need to temporarily toggle CR0.WP before writing the GDT. */
        if (pVM->hm.s.fHostKernelFeatures & SUPKERNELFEATURES_GDT_READ_ONLY)
            pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_GDT_READ_ONLY;
        pVCpu->hm.s.vmx.RestoreHost.uHostSelTR = uSelTR;
    }

    /*
     * Store the GDTR as we need it when restoring the GDT and while restoring the TR.
     */
    if (pVCpu->hm.s.vmx.fRestoreHostFlags & (VMX_RESTORE_HOST_GDTR | VMX_RESTORE_HOST_SEL_TR))
    {
        AssertCompile(sizeof(Gdtr) == sizeof(X86XDTR64));
        memcpy(&pVCpu->hm.s.vmx.RestoreHost.HostGdtr, &Gdtr, sizeof(X86XDTR64));
        if (pVM->hm.s.fHostKernelFeatures & SUPKERNELFEATURES_GDT_NEED_WRITABLE)
        {
            /* The GDT is read-only but the writable GDT is available. */
            pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_GDT_NEED_WRITABLE;
            pVCpu->hm.s.vmx.RestoreHost.HostGdtrRw.cb = Gdtr.cbGdt;
            rc = SUPR0GetCurrentGdtRw(&pVCpu->hm.s.vmx.RestoreHost.HostGdtrRw.uAddr);
            AssertRCReturn(rc, rc);
        }
    }
#else
    uintptr_t uTRBase = X86DESC_BASE(pDesc);
#endif
    rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_TR_BASE, uTRBase);
    AssertRCReturn(rc, rc);

    /*
     * Host FS base and GS base.
     */
#if HC_ARCH_BITS == 64
    uint64_t u64FSBase = ASMRdMsr(MSR_K8_FS_BASE);
    uint64_t u64GSBase = ASMRdMsr(MSR_K8_GS_BASE);
    rc  = VMXWriteVmcs64(VMX_VMCS_HOST_FS_BASE, u64FSBase);
    rc |= VMXWriteVmcs64(VMX_VMCS_HOST_GS_BASE, u64GSBase);
    AssertRCReturn(rc, rc);

    /* Store the base if we have to restore FS or GS manually as we need to restore the base as well. */
    if (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_SEL_FS)
        pVCpu->hm.s.vmx.RestoreHost.uHostFSBase = u64FSBase;
    if (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_SEL_GS)
        pVCpu->hm.s.vmx.RestoreHost.uHostGSBase = u64GSBase;
#endif
    return VINF_SUCCESS;
}


/**
 * Exports certain host MSRs in the VM-exit MSR-load area and some in the
 * host-state area of the VMCS.
 *
 * Theses MSRs will be automatically restored on the host after every successful
 * VM-exit.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportHostMsrs(PVMCPU pVCpu)
{
    AssertPtr(pVCpu);
    AssertPtr(pVCpu->hm.s.vmx.pvHostMsr);

    /*
     * Save MSRs that we restore lazily (due to preemption or transition to ring-3)
     * rather than swapping them on every VM-entry.
     */
    hmR0VmxLazySaveHostMsrs(pVCpu);

    /*
     * Host Sysenter MSRs.
     */
    int rc = VMXWriteVmcs32(VMX_VMCS32_HOST_SYSENTER_CS, ASMRdMsr_Low(MSR_IA32_SYSENTER_CS));
#if HC_ARCH_BITS == 32
    rc |= VMXWriteVmcs32(VMX_VMCS_HOST_SYSENTER_ESP, ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
    rc |= VMXWriteVmcs32(VMX_VMCS_HOST_SYSENTER_EIP, ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
#else
    rc |= VMXWriteVmcs64(VMX_VMCS_HOST_SYSENTER_ESP, ASMRdMsr(MSR_IA32_SYSENTER_ESP));
    rc |= VMXWriteVmcs64(VMX_VMCS_HOST_SYSENTER_EIP, ASMRdMsr(MSR_IA32_SYSENTER_EIP));
#endif
    AssertRCReturn(rc, rc);

    /*
     * Host EFER MSR.
     *
     * If the CPU supports the newer VMCS controls for managing EFER, use it. Otherwise it's
     * done as part of auto-load/store MSR area in the VMCS, see hmR0VmxExportGuestMsrs().
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->hm.s.vmx.fSupportsVmcsEfer)
    {
        rc = VMXWriteVmcs64(VMX_VMCS64_HOST_EFER_FULL, pVM->hm.s.vmx.u64HostEfer);
        AssertRCReturn(rc, rc);
    }

    /** @todo IA32_PERF_GLOBALCTRL, IA32_PAT also see hmR0VmxExportGuestExitCtls(). */

    return VINF_SUCCESS;
}


/**
 * Figures out if we need to swap the EFER MSR which is particularly expensive.
 *
 * We check all relevant bits. For now, that's everything besides LMA/LME, as
 * these two bits are handled by VM-entry, see hmR0VmxExportGuestExitCtls() and
 * hmR0VMxExportGuestEntryCtls().
 *
 * @returns true if we need to load guest EFER, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks Requires EFER, CR4.
 * @remarks No-long-jump zone!!!
 */
static bool hmR0VmxShouldSwapEferMsr(PVMCPU pVCpu)
{
#ifdef HMVMX_ALWAYS_SWAP_EFER
    return true;
#endif

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    /* For 32-bit hosts running 64-bit guests, we always swap EFER in the world-switcher. Nothing to do here. */
    if (CPUMIsGuestInLongModeEx(pCtx))
        return false;
#endif

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    uint64_t const u64HostEfer  = pVM->hm.s.vmx.u64HostEfer;
    uint64_t const u64GuestEfer = pCtx->msrEFER;

    /*
     * For 64-bit guests, if EFER.SCE bit differs, we need to swap EFER to ensure that the
     * guest's SYSCALL behaviour isn't broken, see @bugref{7386}.
     */
    if (   CPUMIsGuestInLongModeEx(pCtx)
        && (u64GuestEfer & MSR_K6_EFER_SCE) != (u64HostEfer & MSR_K6_EFER_SCE))
    {
        return true;
    }

    /*
     * If the guest uses PAE and EFER.NXE bit differs, we need to swap EFER as it
     * affects guest paging. 64-bit paging implies CR4.PAE as well.
     * See Intel spec. 4.5 "IA-32e Paging" and Intel spec. 4.1.1 "Three Paging Modes".
     */
    if (   (pCtx->cr4 & X86_CR4_PAE)
        && (pCtx->cr0 & X86_CR0_PG)
        && (u64GuestEfer & MSR_K6_EFER_NXE) != (u64HostEfer & MSR_K6_EFER_NXE))
    {
        /* Assert that host is PAE capable. */
        Assert(pVM->hm.s.cpuid.u32AMDFeatureEDX & X86_CPUID_EXT_FEATURE_EDX_NX);
        return true;
    }

    return false;
}


/**
 * Exports the guest state with appropriate VM-entry controls in the VMCS.
 *
 * These controls can affect things done on VM-exit; e.g. "load debug controls",
 * see Intel spec. 24.8.1 "VM-entry controls".
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks Requires EFER.
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestEntryCtls(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_VMX_ENTRY_CTLS)
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        uint32_t       fVal = pVM->hm.s.vmx.Msrs.VmxEntry.n.disallowed0; /* Bits set here must be set in the VMCS. */
        uint32_t const fZap = pVM->hm.s.vmx.Msrs.VmxEntry.n.allowed1;    /* Bits cleared here must be cleared in the VMCS. */

        /* Load debug controls (DR7 & IA32_DEBUGCTL_MSR). The first VT-x capable CPUs only supports the 1-setting of this bit. */
        fVal |= VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG;

        /* Set if the guest is in long mode. This will set/clear the EFER.LMA bit on VM-entry. */
        if (CPUMIsGuestInLongModeEx(&pVCpu->cpum.GstCtx))
        {
            fVal |= VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST;
            Log4Func(("VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST\n"));
        }
        else
            Assert(!(fVal & VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST));

        /* If the CPU supports the newer VMCS controls for managing guest/host EFER, use it. */
        if (   pVM->hm.s.vmx.fSupportsVmcsEfer
            && hmR0VmxShouldSwapEferMsr(pVCpu))
        {
            fVal |= VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_EFER_MSR;
            Log4Func(("VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_EFER_MSR\n"));
        }

        /*
         * The following should -not- be set (since we're not in SMM mode):
         * - VMX_VMCS_CTRL_ENTRY_ENTRY_SMM
         * - VMX_VMCS_CTRL_ENTRY_DEACTIVATE_DUALMON
         */

        /** @todo VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_PERF_MSR,
         *        VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_PAT_MSR. */

        if ((fVal & fZap) != fVal)
        {
            Log4Func(("Invalid VM-entry controls combo! Cpu=%RX64 fVal=%RX64 fZap=%RX64\n",
                      pVM->hm.s.vmx.Msrs.VmxEntry.n.disallowed0, fVal, fZap));
            pVCpu->hm.s.u32HMError = VMX_UFC_CTRL_ENTRY;
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }

        /* Commit it to the VMCS and update our cache. */
        if (pVCpu->hm.s.vmx.u32EntryCtls != fVal)
        {
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY, fVal);
            AssertRCReturn(rc, rc);
            pVCpu->hm.s.vmx.u32EntryCtls = fVal;
        }

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_VMX_ENTRY_CTLS);
    }
    return VINF_SUCCESS;
}


/**
 * Exports the guest state with appropriate VM-exit controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks Requires EFER.
 */
static int hmR0VmxExportGuestExitCtls(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_VMX_EXIT_CTLS)
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        uint32_t       fVal = pVM->hm.s.vmx.Msrs.VmxExit.n.disallowed0;  /* Bits set here must be set in the VMCS. */
        uint32_t const fZap = pVM->hm.s.vmx.Msrs.VmxExit.n.allowed1;     /* Bits cleared here must be cleared in the VMCS. */

        /* Save debug controls (DR7 & IA32_DEBUGCTL_MSR). The first VT-x CPUs only supported the 1-setting of this bit. */
        fVal |= VMX_VMCS_CTRL_EXIT_SAVE_DEBUG;

        /*
         * Set the host long mode active (EFER.LMA) bit (which Intel calls "Host address-space size") if necessary.
         * On VM-exit, VT-x sets both the host EFER.LMA and EFER.LME bit to this value. See assertion in
         * hmR0VmxExportHostMsrs().
         */
#if HC_ARCH_BITS == 64
        fVal |= VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE;
        Log4Func(("VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE\n"));
#else
        Assert(   pVCpu->hm.s.vmx.pfnStartVM == VMXR0SwitcherStartVM64
               || pVCpu->hm.s.vmx.pfnStartVM == VMXR0StartVM32);
        /* Set the host address-space size based on the switcher, not guest state. See @bugref{8432}. */
        if (pVCpu->hm.s.vmx.pfnStartVM == VMXR0SwitcherStartVM64)
        {
            /* The switcher returns to long mode, EFER is managed by the switcher. */
            fVal |= VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE;
            Log4Func(("VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE\n"));
        }
        else
            Assert(!(fVal & VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE));
#endif

        /* If the newer VMCS fields for managing EFER exists, use it. */
        if (   pVM->hm.s.vmx.fSupportsVmcsEfer
            && hmR0VmxShouldSwapEferMsr(pVCpu))
        {
            fVal |= VMX_VMCS_CTRL_EXIT_SAVE_GUEST_EFER_MSR
                  | VMX_VMCS_CTRL_EXIT_LOAD_HOST_EFER_MSR;
            Log4Func(("VMX_VMCS_CTRL_EXIT_SAVE_GUEST_EFER_MSR and VMX_VMCS_CTRL_EXIT_LOAD_HOST_EFER_MSR\n"));
        }

        /* Don't acknowledge external interrupts on VM-exit. We want to let the host do that. */
        Assert(!(fVal & VMX_VMCS_CTRL_EXIT_ACK_EXT_INT));

        /** @todo VMX_VMCS_CTRL_EXIT_LOAD_PERF_MSR,
         *        VMX_VMCS_CTRL_EXIT_SAVE_GUEST_PAT_MSR,
         *        VMX_VMCS_CTRL_EXIT_LOAD_HOST_PAT_MSR. */

        /* Enable saving of the VMX preemption timer value on VM-exit. */
        if (    pVM->hm.s.vmx.fUsePreemptTimer
            && (pVM->hm.s.vmx.Msrs.VmxExit.n.allowed1 & VMX_VMCS_CTRL_EXIT_SAVE_VMX_PREEMPT_TIMER))
            fVal |= VMX_VMCS_CTRL_EXIT_SAVE_VMX_PREEMPT_TIMER;

        if ((fVal & fZap) != fVal)
        {
            LogRelFunc(("Invalid VM-exit controls combo! cpu=%RX64 fVal=%RX64 fZap=%RX64\n",
                        pVM->hm.s.vmx.Msrs.VmxExit.n.disallowed0, fVal, fZap));
            pVCpu->hm.s.u32HMError = VMX_UFC_CTRL_EXIT;
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }

        /* Commit it to the VMCS and update our cache. */
        if (pVCpu->hm.s.vmx.u32ExitCtls != fVal)
        {
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT, fVal);
            AssertRCReturn(rc, rc);
            pVCpu->hm.s.vmx.u32ExitCtls = fVal;
        }

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_VMX_EXIT_CTLS);
    }
    return VINF_SUCCESS;
}


/**
 * Sets the TPR threshold in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   u32TprThreshold     The TPR threshold (task-priority class only).
 */
DECLINLINE(int) hmR0VmxApicSetTprThreshold(PVMCPU pVCpu, uint32_t u32TprThreshold)
{
    Assert(!(u32TprThreshold & 0xfffffff0));         /* Bits 31:4 MBZ. */
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW); RT_NOREF_PV(pVCpu);
    return VMXWriteVmcs32(VMX_VMCS32_CTRL_TPR_THRESHOLD, u32TprThreshold);
}


/**
 * Exports the guest APIC TPR state into the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestApicTpr(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_APIC_TPR)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_APIC_TPR);

        if (   PDMHasApic(pVCpu->CTX_SUFF(pVM))
            && APICIsEnabled(pVCpu))
        {
            /*
             * Setup TPR shadowing.
             */
            if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
            {
                Assert(pVCpu->hm.s.vmx.HCPhysVirtApic);

                bool    fPendingIntr  = false;
                uint8_t u8Tpr         = 0;
                uint8_t u8PendingIntr = 0;
                int rc = APICGetTpr(pVCpu, &u8Tpr, &fPendingIntr, &u8PendingIntr);
                AssertRCReturn(rc, rc);

                /*
                 * If there are interrupts pending but masked by the TPR, instruct VT-x to
                 * cause a TPR-below-threshold VM-exit when the guest lowers its TPR below the
                 * priority of the pending interrupt so we can deliver the interrupt. If there
                 * are no interrupts pending, set threshold to 0 to not cause any
                 * TPR-below-threshold VM-exits.
                 */
                pVCpu->hm.s.vmx.pbVirtApic[XAPIC_OFF_TPR] = u8Tpr;
                uint32_t u32TprThreshold = 0;
                if (fPendingIntr)
                {
                    /* Bits 3:0 of the TPR threshold field correspond to bits 7:4 of the TPR (which is the Task-Priority Class). */
                    const uint8_t u8PendingPriority = u8PendingIntr >> 4;
                    const uint8_t u8TprPriority     = u8Tpr >> 4;
                    if (u8PendingPriority <= u8TprPriority)
                        u32TprThreshold = u8PendingPriority;
                }

                rc = hmR0VmxApicSetTprThreshold(pVCpu, u32TprThreshold);
                AssertRCReturn(rc, rc);
            }
        }
        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_APIC_TPR);
    }
    return VINF_SUCCESS;
}


/**
 * Gets the guest's interruptibility-state ("interrupt shadow" as AMD calls it).
 *
 * @returns Guest's interruptibility-state.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static uint32_t hmR0VmxGetGuestIntrState(PVMCPU pVCpu)
{
    /*
     * Check if we should inhibit interrupt delivery due to instructions like STI and MOV SS.
     */
    uint32_t fIntrState = 0;
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    {
        /* If inhibition is active, RIP & RFLAGS should've been accessed
           (i.e. read previously from the VMCS or from ring-3). */
        PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
#ifdef VBOX_STRICT
        uint64_t const fExtrn = ASMAtomicUoReadU64(&pCtx->fExtrn);
        AssertMsg(!(fExtrn & (CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS)), ("%#x\n", fExtrn));
#endif
        if (pCtx->rip == EMGetInhibitInterruptsPC(pVCpu))
        {
            if (pCtx->eflags.Bits.u1IF)
                fIntrState = VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI;
            else
                fIntrState = VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS;
        }
        else if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
        {
            /*
             * We can clear the inhibit force flag as even if we go back to the recompiler
             * without executing guest code in VT-x, the flag's condition to be cleared is
             * met and thus the cleared state is correct.
             */
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
        }
    }

    /*
     * NMIs to the guest are blocked after an NMI is injected until the guest executes an IRET. We only
     * bother with virtual-NMI blocking when we have support for virtual NMIs in the CPU, otherwise
     * setting this would block host-NMIs and IRET will not clear the blocking.
     *
     * See Intel spec. 26.6.1 "Interruptibility state". See @bugref{7445}.
     */
    if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS)
        && (pVCpu->hm.s.vmx.u32PinCtls & VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI))
    {
        fIntrState |= VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI;
    }

    return fIntrState;
}


/**
 * Exports the guest's interruptibility-state into the guest-state area in the
 * VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   fIntrState  The interruptibility-state to set.
 */
static int hmR0VmxExportGuestIntrState(PVMCPU pVCpu, uint32_t fIntrState)
{
    NOREF(pVCpu);
    AssertMsg(!(fIntrState & 0xfffffff0), ("%#x\n", fIntrState));   /* Bits 31:4 MBZ. */
    Assert((fIntrState & 0x3) != 0x3);                              /* Block-by-STI and MOV SS cannot be simultaneously set. */
    return VMXWriteVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, fIntrState);
}


/**
 * Exports the exception intercepts required for guest execution in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestXcptIntercepts(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_VMX_GUEST_XCPT_INTERCEPTS)
    {
        uint32_t uXcptBitmap = pVCpu->hm.s.vmx.u32XcptBitmap;

        /* The remaining exception intercepts are handled elsewhere, e.g. in hmR0VmxExportSharedCR0(). */
        if (pVCpu->hm.s.fGIMTrapXcptUD)
            uXcptBitmap |= RT_BIT(X86_XCPT_UD);
#ifndef HMVMX_ALWAYS_TRAP_ALL_XCPTS
        else
            uXcptBitmap &= ~RT_BIT(X86_XCPT_UD);
#endif

        Assert(uXcptBitmap & RT_BIT_32(X86_XCPT_AC));
        Assert(uXcptBitmap & RT_BIT_32(X86_XCPT_DB));

        if (uXcptBitmap != pVCpu->hm.s.vmx.u32XcptBitmap)
        {
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, uXcptBitmap);
            AssertRCReturn(rc, rc);
            pVCpu->hm.s.vmx.u32XcptBitmap = uXcptBitmap;
        }

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_VMX_GUEST_XCPT_INTERCEPTS);
        Log4Func(("VMX_VMCS32_CTRL_EXCEPTION_BITMAP=%#RX64\n", uXcptBitmap));
    }
    return VINF_SUCCESS;
}


/**
 * Exports the guest's RIP into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestRip(PVMCPU pVCpu)
{
    int rc = VINF_SUCCESS;
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_RIP)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_RIP);

        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_RIP, pVCpu->cpum.GstCtx.rip);
        AssertRCReturn(rc, rc);

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_RIP);
        Log4Func(("RIP=%#RX64\n", pVCpu->cpum.GstCtx.rip));
    }
    return rc;
}


/**
 * Exports the guest's RSP into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestRsp(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_RSP)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_RSP);

        int rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_RSP, pVCpu->cpum.GstCtx.rsp);
        AssertRCReturn(rc, rc);

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_RSP);
    }
    return VINF_SUCCESS;
}


/**
 * Exports the guest's RFLAGS into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestRflags(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_RFLAGS)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_RFLAGS);

        /* Intel spec. 2.3.1 "System Flags and Fields in IA-32e Mode" claims the upper 32-bits of RFLAGS are reserved (MBZ).
           Let us assert it as such and use 32-bit VMWRITE. */
        Assert(!RT_HI_U32(pVCpu->cpum.GstCtx.rflags.u64));
        X86EFLAGS fEFlags = pVCpu->cpum.GstCtx.eflags;
        Assert(fEFlags.u32 & X86_EFL_RA1_MASK);
        Assert(!(fEFlags.u32 & ~(X86_EFL_1 | X86_EFL_LIVE_MASK)));

        /*
         * If we're emulating real-mode using Virtual 8086 mode, save the real-mode eflags so
         * we can restore them on VM-exit. Modify the real-mode guest's eflags so that VT-x
         * can run the real-mode guest code under Virtual 8086 mode.
         */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.pRealModeTSS);
            Assert(PDMVmmDevHeapIsEnabled(pVCpu->CTX_SUFF(pVM)));
            pVCpu->hm.s.vmx.RealMode.Eflags.u32 = fEFlags.u32;  /* Save the original eflags of the real-mode guest. */
            fEFlags.Bits.u1VM   = 1;                            /* Set the Virtual 8086 mode bit. */
            fEFlags.Bits.u2IOPL = 0;                            /* Change IOPL to 0, otherwise certain instructions won't fault. */
        }

        int rc = VMXWriteVmcs32(VMX_VMCS_GUEST_RFLAGS, fEFlags.u32);
        AssertRCReturn(rc, rc);

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_RFLAGS);
        Log4Func(("EFlags=%#RX32\n", fEFlags.u32));
    }
    return VINF_SUCCESS;
}


/**
 * Exports the guest CR0 control register into the guest-state area in the VMCS.
 *
 * The guest FPU state is always pre-loaded hence we don't need to bother about
 * sharing FPU related CR0 bits between the guest and host.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestCR0(PVMCPU pVCpu)
{
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_CR0)
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CR0);
        Assert(!RT_HI_U32(pVCpu->cpum.GstCtx.cr0));

        uint32_t const u32ShadowCr0 = pVCpu->cpum.GstCtx.cr0;
        uint32_t       u32GuestCr0  = pVCpu->cpum.GstCtx.cr0;

        /*
         * Setup VT-x's view of the guest CR0.
         * Minimize VM-exits due to CR3 changes when we have NestedPaging.
         */
        uint32_t uProcCtls = pVCpu->hm.s.vmx.u32ProcCtls;
        if (pVM->hm.s.fNestedPaging)
        {
            if (CPUMIsGuestPagingEnabled(pVCpu))
            {
                /* The guest has paging enabled, let it access CR3 without causing a VM-exit if supported. */
                uProcCtls &= ~(  VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT
                               | VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT);
            }
            else
            {
                /* The guest doesn't have paging enabled, make CR3 access cause a VM-exit to update our shadow. */
                uProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT
                           | VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT;
            }

            /* If we have unrestricted guest execution, we never have to intercept CR3 reads. */
            if (pVM->hm.s.vmx.fUnrestrictedGuest)
                uProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT;
        }
        else
        {
            /* Guest CPL 0 writes to its read-only pages should cause a #PF VM-exit. */
            u32GuestCr0 |= X86_CR0_WP;
        }

        /*
         * Guest FPU bits.
         *
         * Since we pre-load the guest FPU always before VM-entry there is no need to track lazy state
         * using CR0.TS.
         *
         * Intel spec. 23.8 "Restrictions on VMX operation" mentions that CR0.NE bit must always be
         * set on the first CPUs to support VT-x and no mention of with regards to UX in VM-entry checks.
         */
        u32GuestCr0 |= X86_CR0_NE;

        /* If CR0.NE isn't set, we need to intercept #MF exceptions and report them to the guest differently. */
        bool const fInterceptMF = !(u32ShadowCr0 & X86_CR0_NE);

        /*
         * Update exception intercepts.
         */
        uint32_t uXcptBitmap = pVCpu->hm.s.vmx.u32XcptBitmap;
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            Assert(PDMVmmDevHeapIsEnabled(pVM));
            Assert(pVM->hm.s.vmx.pRealModeTSS);
            uXcptBitmap |= HMVMX_REAL_MODE_XCPT_MASK;
        }
        else
        {
            /* For now, cleared here as mode-switches can happen outside HM/VT-x. See @bugref{7626#c11}. */
            uXcptBitmap &= ~HMVMX_REAL_MODE_XCPT_MASK;
            if (fInterceptMF)
                uXcptBitmap |= RT_BIT(X86_XCPT_MF);
        }

        /* Additional intercepts for debugging, define these yourself explicitly. */
#ifdef HMVMX_ALWAYS_TRAP_ALL_XCPTS
        uXcptBitmap |= 0
                     | RT_BIT(X86_XCPT_BP)
                     | RT_BIT(X86_XCPT_DE)
                     | RT_BIT(X86_XCPT_NM)
                     | RT_BIT(X86_XCPT_TS)
                     | RT_BIT(X86_XCPT_UD)
                     | RT_BIT(X86_XCPT_NP)
                     | RT_BIT(X86_XCPT_SS)
                     | RT_BIT(X86_XCPT_GP)
                     | RT_BIT(X86_XCPT_PF)
                     | RT_BIT(X86_XCPT_MF)
                     ;
#elif defined(HMVMX_ALWAYS_TRAP_PF)
        uXcptBitmap |= RT_BIT(X86_XCPT_PF);
#endif
        Assert(pVM->hm.s.fNestedPaging || (uXcptBitmap & RT_BIT(X86_XCPT_PF)));

        /*
         * Set/clear the CR0 specific bits along with their exceptions (PE, PG, CD, NW).
         */
        uint32_t fSetCr0 = (uint32_t)(pVM->hm.s.vmx.Msrs.u64Cr0Fixed0 & pVM->hm.s.vmx.Msrs.u64Cr0Fixed1);
        uint32_t fZapCr0 = (uint32_t)(pVM->hm.s.vmx.Msrs.u64Cr0Fixed0 | pVM->hm.s.vmx.Msrs.u64Cr0Fixed1);
        if (pVM->hm.s.vmx.fUnrestrictedGuest)             /* Exceptions for unrestricted-guests for fixed CR0 bits (PE, PG). */
            fSetCr0 &= ~(X86_CR0_PE | X86_CR0_PG);
        else
            Assert((fSetCr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG));

        u32GuestCr0 |= fSetCr0;
        u32GuestCr0 &= fZapCr0;
        u32GuestCr0 &= ~(X86_CR0_CD | X86_CR0_NW);        /* Always enable caching. */

        /*
         * CR0 is shared between host and guest along with a CR0 read shadow. Therefore, certain bits must not be changed
         * by the guest because VT-x ignores saving/restoring them (namely CD, ET, NW) and for certain other bits
         * we want to be notified immediately of guest CR0 changes (e.g. PG to update our shadow page tables).
         */
        uint32_t u32Cr0Mask = X86_CR0_PE
                            | X86_CR0_NE
                            | (pVM->hm.s.fNestedPaging ? 0 : X86_CR0_WP)
                            | X86_CR0_PG
                            | X86_CR0_ET   /* Bit ignored on VM-entry and VM-exit. Don't let the guest modify the host CR0.ET */
                            | X86_CR0_CD   /* Bit ignored on VM-entry and VM-exit. Don't let the guest modify the host CR0.CD */
                            | X86_CR0_NW;  /* Bit ignored on VM-entry and VM-exit. Don't let the guest modify the host CR0.NW */

        /** @todo Avoid intercepting CR0.PE with unrestricted guests. Fix PGM
         *        enmGuestMode to be in-sync with the current mode. See @bugref{6398}
         *        and @bugref{6944}. */
#if 0
        if (pVM->hm.s.vmx.fUnrestrictedGuest)
            u32Cr0Mask &= ~X86_CR0_PE;
#endif
        /*
         * Finally, update VMCS fields with the CR0 values and the exception bitmap.
         */
        int rc = VMXWriteVmcs32(VMX_VMCS_GUEST_CR0, u32GuestCr0);
        rc    |= VMXWriteVmcs32(VMX_VMCS_CTRL_CR0_READ_SHADOW, u32ShadowCr0);
        if (u32Cr0Mask != pVCpu->hm.s.vmx.u32Cr0Mask)
            rc |= VMXWriteVmcs32(VMX_VMCS_CTRL_CR0_MASK, u32Cr0Mask);
        if (uProcCtls != pVCpu->hm.s.vmx.u32ProcCtls)
            rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, uProcCtls);
        if (uXcptBitmap != pVCpu->hm.s.vmx.u32XcptBitmap)
            rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, uXcptBitmap);
        AssertRCReturn(rc, rc);

        /* Update our caches. */
        pVCpu->hm.s.vmx.u32Cr0Mask    = u32Cr0Mask;
        pVCpu->hm.s.vmx.u32ProcCtls   = uProcCtls;
        pVCpu->hm.s.vmx.u32XcptBitmap = uXcptBitmap;

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_CR0);

        Log4Func(("u32Cr0Mask=%#RX32 u32ShadowCr0=%#RX32 u32GuestCr0=%#RX32 (fSetCr0=%#RX32 fZapCr0=%#RX32\n", u32Cr0Mask,
                  u32ShadowCr0, u32GuestCr0, fSetCr0, fZapCr0));
    }

    return VINF_SUCCESS;
}


/**
 * Exports the guest control registers (CR3, CR4) into the guest-state area
 * in the VMCS.
 *
 * @returns VBox strict status code.
 * @retval  VINF_EM_RESCHEDULE_REM if we try to emulate non-paged guest code
 *          without unrestricted guest access and the VMMDev is not presently
 *          mapped (e.g. EFI32).
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static VBOXSTRICTRC hmR0VmxExportGuestCR3AndCR4(PVMCPU pVCpu)
{
    int rc  = VINF_SUCCESS;
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Guest CR2.
     * It's always loaded in the assembler code. Nothing to do here.
     */

    /*
     * Guest CR3.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_CR3)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CR3);

        RTGCPHYS GCPhysGuestCR3 = NIL_RTGCPHYS;
        if (pVM->hm.s.fNestedPaging)
        {
            pVCpu->hm.s.vmx.HCPhysEPTP = PGMGetHyperCR3(pVCpu);

            /* Validate. See Intel spec. 28.2.2 "EPT Translation Mechanism" and 24.6.11 "Extended-Page-Table Pointer (EPTP)" */
            Assert(pVCpu->hm.s.vmx.HCPhysEPTP);
            Assert(!(pVCpu->hm.s.vmx.HCPhysEPTP & UINT64_C(0xfff0000000000000)));
            Assert(!(pVCpu->hm.s.vmx.HCPhysEPTP & 0xfff));

            /* VMX_EPT_MEMTYPE_WB support is already checked in hmR0VmxSetupTaggedTlb(). */
            pVCpu->hm.s.vmx.HCPhysEPTP |= VMX_EPT_MEMTYPE_WB
                                        | (VMX_EPT_PAGE_WALK_LENGTH_DEFAULT << VMX_EPT_PAGE_WALK_LENGTH_SHIFT);

            /* Validate. See Intel spec. 26.2.1 "Checks on VMX Controls" */
            AssertMsg(   ((pVCpu->hm.s.vmx.HCPhysEPTP >> 3) & 0x07) == 3      /* Bits 3:5 (EPT page walk length - 1) must be 3. */
                      && ((pVCpu->hm.s.vmx.HCPhysEPTP >> 7) & 0x1f) == 0,     /* Bits 7:11 MBZ. */
                         ("EPTP %#RX64\n", pVCpu->hm.s.vmx.HCPhysEPTP));
            AssertMsg(  !((pVCpu->hm.s.vmx.HCPhysEPTP >> 6) & 0x01)           /* Bit 6 (EPT accessed & dirty bit). */
                      || (pVM->hm.s.vmx.Msrs.u64EptVpidCaps & MSR_IA32_VMX_EPT_VPID_CAP_EPT_ACCESS_DIRTY),
                         ("EPTP accessed/dirty bit not supported by CPU but set %#RX64\n", pVCpu->hm.s.vmx.HCPhysEPTP));

            rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_EPTP_FULL, pVCpu->hm.s.vmx.HCPhysEPTP);
            AssertRCReturn(rc, rc);

            PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
            if (   pVM->hm.s.vmx.fUnrestrictedGuest
                || CPUMIsGuestPagingEnabledEx(pCtx))
            {
                /* If the guest is in PAE mode, pass the PDPEs to VT-x using the VMCS fields. */
                if (CPUMIsGuestInPAEModeEx(pCtx))
                {
                    rc  = PGMGstGetPaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);
                    AssertRCReturn(rc, rc);
                    rc  = VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE0_FULL, pVCpu->hm.s.aPdpes[0].u);
                    rc |= VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE1_FULL, pVCpu->hm.s.aPdpes[1].u);
                    rc |= VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE2_FULL, pVCpu->hm.s.aPdpes[2].u);
                    rc |= VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE3_FULL, pVCpu->hm.s.aPdpes[3].u);
                    AssertRCReturn(rc, rc);
                }

                /*
                 * The guest's view of its CR3 is unblemished with Nested Paging when the
                 * guest is using paging or we have unrestricted guest execution to handle
                 * the guest when it's not using paging.
                 */
                GCPhysGuestCR3 = pCtx->cr3;
            }
            else
            {
                /*
                 * The guest is not using paging, but the CPU (VT-x) has to. While the guest
                 * thinks it accesses physical memory directly, we use our identity-mapped
                 * page  table to map guest-linear to guest-physical addresses. EPT takes care
                 * of translating it to host-physical addresses.
                 */
                RTGCPHYS GCPhys;
                Assert(pVM->hm.s.vmx.pNonPagingModeEPTPageTable);

                /* We obtain it here every time as the guest could have relocated this PCI region. */
                rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                if (RT_SUCCESS(rc))
                { /* likely */ }
                else if (rc == VERR_PDM_DEV_HEAP_R3_TO_GCPHYS)
                {
                    Log4Func(("VERR_PDM_DEV_HEAP_R3_TO_GCPHYS -> VINF_EM_RESCHEDULE_REM\n"));
                    return VINF_EM_RESCHEDULE_REM;  /* We cannot execute now, switch to REM/IEM till the guest maps in VMMDev. */
                }
                else
                    AssertMsgFailedReturn(("%Rrc\n",  rc), rc);

                GCPhysGuestCR3 = GCPhys;
            }

            Log4Func(("u32GuestCr3=%#RGp (GstN)\n", GCPhysGuestCR3));
            rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_CR3, GCPhysGuestCR3);
            AssertRCReturn(rc, rc);
        }
        else
        {
            /* Non-nested paging case, just use the hypervisor's CR3. */
            RTHCPHYS HCPhysGuestCR3 = PGMGetHyperCR3(pVCpu);

            Log4Func(("u32GuestCr3=%#RHv (HstN)\n", HCPhysGuestCR3));
            rc = VMXWriteVmcsHstN(VMX_VMCS_GUEST_CR3, HCPhysGuestCR3);
            AssertRCReturn(rc, rc);
        }

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_CR3);
    }

    /*
     * Guest CR4.
     * ASSUMES this is done everytime we get in from ring-3! (XCR0)
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_CR4)
    {
        PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CR4);
        Assert(!RT_HI_U32(pCtx->cr4));

        uint32_t       u32GuestCr4  = pCtx->cr4;
        uint32_t const u32ShadowCr4 = pCtx->cr4;

        /*
         * Setup VT-x's view of the guest CR4.
         *
         * If we're emulating real-mode using virtual-8086 mode, we want to redirect software
         * interrupts to the 8086 program interrupt handler. Clear the VME bit (the interrupt
         * redirection bitmap is already all 0, see hmR3InitFinalizeR0())
         *
         * See Intel spec. 20.2 "Software Interrupt Handling Methods While in Virtual-8086 Mode".
         */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            Assert(pVM->hm.s.vmx.pRealModeTSS);
            Assert(PDMVmmDevHeapIsEnabled(pVM));
            u32GuestCr4 &= ~X86_CR4_VME;
        }

        if (pVM->hm.s.fNestedPaging)
        {
            if (   !CPUMIsGuestPagingEnabledEx(pCtx)
                && !pVM->hm.s.vmx.fUnrestrictedGuest)
            {
                /* We use 4 MB pages in our identity mapping page table when the guest doesn't have paging. */
                u32GuestCr4 |= X86_CR4_PSE;
                /* Our identity mapping is a 32-bit page directory. */
                u32GuestCr4 &= ~X86_CR4_PAE;
            }
            /* else use guest CR4.*/
        }
        else
        {
            /*
             * The shadow paging modes and guest paging modes are different, the shadow is in accordance with the host
             * paging mode and thus we need to adjust VT-x's view of CR4 depending on our shadow page tables.
             */
            switch (pVCpu->hm.s.enmShadowMode)
            {
                case PGMMODE_REAL:              /* Real-mode. */
                case PGMMODE_PROTECTED:         /* Protected mode without paging. */
                case PGMMODE_32_BIT:            /* 32-bit paging. */
                {
                    u32GuestCr4 &= ~X86_CR4_PAE;
                    break;
                }

                case PGMMODE_PAE:               /* PAE paging. */
                case PGMMODE_PAE_NX:            /* PAE paging with NX. */
                {
                    u32GuestCr4 |= X86_CR4_PAE;
                    break;
                }

                case PGMMODE_AMD64:             /* 64-bit AMD paging (long mode). */
                case PGMMODE_AMD64_NX:          /* 64-bit AMD paging (long mode) with NX enabled. */
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                    break;
#endif
                default:
                    AssertFailed();
                    return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
            }
        }

        /* We need to set and clear the CR4 specific bits here (mainly the X86_CR4_VMXE bit). */
        uint64_t const fSetCr4 = (pVM->hm.s.vmx.Msrs.u64Cr4Fixed0 & pVM->hm.s.vmx.Msrs.u64Cr4Fixed1);
        uint64_t const fZapCr4 = (pVM->hm.s.vmx.Msrs.u64Cr4Fixed0 | pVM->hm.s.vmx.Msrs.u64Cr4Fixed1);
        u32GuestCr4 |= fSetCr4;
        u32GuestCr4 &= fZapCr4;

        /* Setup CR4 mask. CR4 flags owned by the host, if the guest attempts to change them,
           that would cause a VM-exit. */
        uint32_t u32Cr4Mask = X86_CR4_VME
                            | X86_CR4_PAE
                            | X86_CR4_PGE
                            | X86_CR4_PSE
                            | X86_CR4_VMXE;
        if (pVM->cpum.ro.HostFeatures.fXSaveRstor)
            u32Cr4Mask |= X86_CR4_OSXSAVE;
        if (pVM->cpum.ro.GuestFeatures.fPcid)
            u32Cr4Mask |= X86_CR4_PCIDE;

        /* Write VT-x's view of the guest CR4, the CR4 modify mask and the read-only CR4 shadow
           into the VMCS and update our cache. */
        rc  = VMXWriteVmcs32(VMX_VMCS_GUEST_CR4, u32GuestCr4);
        rc |= VMXWriteVmcs32(VMX_VMCS_CTRL_CR4_READ_SHADOW, u32ShadowCr4);
        if (pVCpu->hm.s.vmx.u32Cr4Mask != u32Cr4Mask)
            rc |= VMXWriteVmcs32(VMX_VMCS_CTRL_CR4_MASK, u32Cr4Mask);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.vmx.u32Cr4Mask = u32Cr4Mask;

        /* Whether to save/load/restore XCR0 during world switch depends on CR4.OSXSAVE and host+guest XCR0. */
        pVCpu->hm.s.fLoadSaveGuestXcr0 = (pCtx->cr4 & X86_CR4_OSXSAVE) && pCtx->aXcr[0] != ASMGetXcr0();

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_CR4);

        Log4Func(("u32GuestCr4=%#RX32 u32ShadowCr4=%#RX32 (fSetCr4=%#RX32 fZapCr4=%#RX32)\n", u32GuestCr4, u32ShadowCr4, fSetCr4,
                  fZapCr4));
    }
    return rc;
}


/**
 * Exports the guest debug registers into the guest-state area in the VMCS.
 * The guest debug bits are partially shared with the host (e.g. DR6, DR0-3).
 *
 * This also sets up whether \#DB and MOV DRx accesses cause VM-exits.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportSharedDebugState(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

#ifdef VBOX_STRICT
    /* Validate. Intel spec. 26.3.1.1 "Checks on Guest Controls Registers, Debug Registers, MSRs" */
    if (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG)
    {
        /* Validate. Intel spec. 17.2 "Debug Registers", recompiler paranoia checks. */
        Assert((pVCpu->cpum.GstCtx.dr[7] & (X86_DR7_MBZ_MASK | X86_DR7_RAZ_MASK)) == 0);
        Assert((pVCpu->cpum.GstCtx.dr[7] & X86_DR7_RA1_MASK) == X86_DR7_RA1_MASK);
    }
#endif

    bool     fSteppingDB      = false;
    bool     fInterceptMovDRx = false;
    uint32_t uProcCtls        = pVCpu->hm.s.vmx.u32ProcCtls;
    if (pVCpu->hm.s.fSingleInstruction)
    {
        /* If the CPU supports the monitor trap flag, use it for single stepping in DBGF and avoid intercepting #DB. */
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        if (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG)
        {
            uProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG;
            Assert(fSteppingDB == false);
        }
        else
        {
            pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_TF;
            pVCpu->hm.s.fCtxChanged |= HM_CHANGED_GUEST_RFLAGS;
            pVCpu->hm.s.fClearTrapFlag = true;
            fSteppingDB = true;
        }
    }

    uint32_t u32GuestDr7;
    if (   fSteppingDB
        || (CPUMGetHyperDR7(pVCpu) & X86_DR7_ENABLED_MASK))
    {
        /*
         * Use the combined guest and host DRx values found in the hypervisor register set
         * because the debugger has breakpoints active or someone is single stepping on the
         * host side without a monitor trap flag.
         *
         * Note! DBGF expects a clean DR6 state before executing guest code.
         */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
        if (    CPUMIsGuestInLongModeEx(&pVCpu->cpum.GstCtx)
            && !CPUMIsHyperDebugStateActivePending(pVCpu))
        {
            CPUMR0LoadHyperDebugState(pVCpu, true /* include DR6 */);
            Assert(CPUMIsHyperDebugStateActivePending(pVCpu));
            Assert(!CPUMIsGuestDebugStateActivePending(pVCpu));
        }
        else
#endif
        if (!CPUMIsHyperDebugStateActive(pVCpu))
        {
            CPUMR0LoadHyperDebugState(pVCpu, true /* include DR6 */);
            Assert(CPUMIsHyperDebugStateActive(pVCpu));
            Assert(!CPUMIsGuestDebugStateActive(pVCpu));
        }

        /* Update DR7 with the hypervisor value (other DRx registers are handled by CPUM one way or another). */
        u32GuestDr7 = (uint32_t)CPUMGetHyperDR7(pVCpu);
        pVCpu->hm.s.fUsingHyperDR7 = true;
        fInterceptMovDRx = true;
    }
    else
    {
        /*
         * If the guest has enabled debug registers, we need to load them prior to
         * executing guest code so they'll trigger at the right time.
         */
        if (pVCpu->cpum.GstCtx.dr[7] & (X86_DR7_ENABLED_MASK | X86_DR7_GD))
        {
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
            if (    CPUMIsGuestInLongModeEx(&pVCpu->cpum.GstCtx)
                && !CPUMIsGuestDebugStateActivePending(pVCpu))
            {
                CPUMR0LoadGuestDebugState(pVCpu, true /* include DR6 */);
                Assert(CPUMIsGuestDebugStateActivePending(pVCpu));
                Assert(!CPUMIsHyperDebugStateActivePending(pVCpu));
                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxArmed);
            }
            else
#endif
            if (!CPUMIsGuestDebugStateActive(pVCpu))
            {
                CPUMR0LoadGuestDebugState(pVCpu, true /* include DR6 */);
                Assert(CPUMIsGuestDebugStateActive(pVCpu));
                Assert(!CPUMIsHyperDebugStateActive(pVCpu));
                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxArmed);
            }
            Assert(!fInterceptMovDRx);
        }
        /*
         * If no debugging enabled, we'll lazy load DR0-3.  Unlike on AMD-V, we
         * must intercept #DB in order to maintain a correct DR6 guest value, and
         * because we need to intercept it to prevent nested #DBs from hanging the
         * CPU, we end up always having to intercept it.  See hmR0VmxInitXcptBitmap.
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

        /* Update DR7 with the actual guest value. */
        u32GuestDr7 = pVCpu->cpum.GstCtx.dr[7];
        pVCpu->hm.s.fUsingHyperDR7 = false;
    }

    if (fInterceptMovDRx)
        uProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT;
    else
        uProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT;

    /*
     * Update the processor-based VM-execution controls with the MOV-DRx intercepts and the
     * monitor-trap flag and update our cache.
     */
    if (uProcCtls != pVCpu->hm.s.vmx.u32ProcCtls)
    {
        int rc2 = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, uProcCtls);
        AssertRCReturn(rc2, rc2);
        pVCpu->hm.s.vmx.u32ProcCtls = uProcCtls;
    }

    /*
     * Update guest DR7.
     */
    int rc = VMXWriteVmcs32(VMX_VMCS_GUEST_DR7, u32GuestDr7);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


#ifdef VBOX_STRICT
/**
 * Strict function to validate segment registers.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks Will import guest CR0 on strict builds during validation of
 *          segments.
 */
static void hmR0VmxValidateSegmentRegs(PVMCPU pVCpu)
{
    /*
     * Validate segment registers. See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers".
     *
     * The reason we check for attribute value 0 in this function and not just the unusable bit is
     * because hmR0VmxExportGuestSegmentReg() only updates the VMCS' copy of the value with the unusable bit
     * and doesn't change the guest-context value.
     */
    PVM       pVM  = pVCpu->CTX_SUFF(pVM);
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR0);
    if (   !pVM->hm.s.vmx.fUnrestrictedGuest
        && (   !CPUMIsGuestInRealModeEx(pCtx)
            && !CPUMIsGuestInV86ModeEx(pCtx)))
    {
        /* Protected mode checks */
        /* CS */
        Assert(pCtx->cs.Attr.n.u1Present);
        Assert(!(pCtx->cs.Attr.u & 0xf00));
        Assert(!(pCtx->cs.Attr.u & 0xfffe0000));
        Assert(   (pCtx->cs.u32Limit & 0xfff) == 0xfff
               || !(pCtx->cs.Attr.n.u1Granularity));
        Assert(   !(pCtx->cs.u32Limit & 0xfff00000)
               || (pCtx->cs.Attr.n.u1Granularity));
        /* CS cannot be loaded with NULL in protected mode. */
        Assert(pCtx->cs.Attr.u && !(pCtx->cs.Attr.u & X86DESCATTR_UNUSABLE)); /** @todo is this really true even for 64-bit CS? */
        if (pCtx->cs.Attr.n.u4Type == 9 || pCtx->cs.Attr.n.u4Type == 11)
            Assert(pCtx->cs.Attr.n.u2Dpl == pCtx->ss.Attr.n.u2Dpl);
        else if (pCtx->cs.Attr.n.u4Type == 13 || pCtx->cs.Attr.n.u4Type == 15)
            Assert(pCtx->cs.Attr.n.u2Dpl <= pCtx->ss.Attr.n.u2Dpl);
        else
            AssertMsgFailed(("Invalid CS Type %#x\n", pCtx->cs.Attr.n.u2Dpl));
        /* SS */
        Assert((pCtx->ss.Sel & X86_SEL_RPL) == (pCtx->cs.Sel & X86_SEL_RPL));
        Assert(pCtx->ss.Attr.n.u2Dpl == (pCtx->ss.Sel & X86_SEL_RPL));
        if (   !(pCtx->cr0 & X86_CR0_PE)
            || pCtx->cs.Attr.n.u4Type == 3)
        {
            Assert(!pCtx->ss.Attr.n.u2Dpl);
        }
        if (pCtx->ss.Attr.u && !(pCtx->ss.Attr.u & X86DESCATTR_UNUSABLE))
        {
            Assert((pCtx->ss.Sel & X86_SEL_RPL) == (pCtx->cs.Sel & X86_SEL_RPL));
            Assert(pCtx->ss.Attr.n.u4Type == 3 || pCtx->ss.Attr.n.u4Type == 7);
            Assert(pCtx->ss.Attr.n.u1Present);
            Assert(!(pCtx->ss.Attr.u & 0xf00));
            Assert(!(pCtx->ss.Attr.u & 0xfffe0000));
            Assert(   (pCtx->ss.u32Limit & 0xfff) == 0xfff
                   || !(pCtx->ss.Attr.n.u1Granularity));
            Assert(   !(pCtx->ss.u32Limit & 0xfff00000)
                   || (pCtx->ss.Attr.n.u1Granularity));
        }
        /* DS, ES, FS, GS - only check for usable selectors, see hmR0VmxExportGuestSegmentReg(). */
        if (pCtx->ds.Attr.u && !(pCtx->ds.Attr.u & X86DESCATTR_UNUSABLE))
        {
            Assert(pCtx->ds.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED);
            Assert(pCtx->ds.Attr.n.u1Present);
            Assert(pCtx->ds.Attr.n.u4Type > 11 || pCtx->ds.Attr.n.u2Dpl >= (pCtx->ds.Sel & X86_SEL_RPL));
            Assert(!(pCtx->ds.Attr.u & 0xf00));
            Assert(!(pCtx->ds.Attr.u & 0xfffe0000));
            Assert(   (pCtx->ds.u32Limit & 0xfff) == 0xfff
                   || !(pCtx->ds.Attr.n.u1Granularity));
            Assert(   !(pCtx->ds.u32Limit & 0xfff00000)
                   || (pCtx->ds.Attr.n.u1Granularity));
            Assert(   !(pCtx->ds.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                   || (pCtx->ds.Attr.n.u4Type & X86_SEL_TYPE_READ));
        }
        if (pCtx->es.Attr.u && !(pCtx->es.Attr.u & X86DESCATTR_UNUSABLE))
        {
            Assert(pCtx->es.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED);
            Assert(pCtx->es.Attr.n.u1Present);
            Assert(pCtx->es.Attr.n.u4Type > 11 || pCtx->es.Attr.n.u2Dpl >= (pCtx->es.Sel & X86_SEL_RPL));
            Assert(!(pCtx->es.Attr.u & 0xf00));
            Assert(!(pCtx->es.Attr.u & 0xfffe0000));
            Assert(   (pCtx->es.u32Limit & 0xfff) == 0xfff
                   || !(pCtx->es.Attr.n.u1Granularity));
            Assert(   !(pCtx->es.u32Limit & 0xfff00000)
                   || (pCtx->es.Attr.n.u1Granularity));
            Assert(   !(pCtx->es.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                   || (pCtx->es.Attr.n.u4Type & X86_SEL_TYPE_READ));
        }
        if (pCtx->fs.Attr.u && !(pCtx->fs.Attr.u & X86DESCATTR_UNUSABLE))
        {
            Assert(pCtx->fs.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED);
            Assert(pCtx->fs.Attr.n.u1Present);
            Assert(pCtx->fs.Attr.n.u4Type > 11 || pCtx->fs.Attr.n.u2Dpl >= (pCtx->fs.Sel & X86_SEL_RPL));
            Assert(!(pCtx->fs.Attr.u & 0xf00));
            Assert(!(pCtx->fs.Attr.u & 0xfffe0000));
            Assert(   (pCtx->fs.u32Limit & 0xfff) == 0xfff
                   || !(pCtx->fs.Attr.n.u1Granularity));
            Assert(   !(pCtx->fs.u32Limit & 0xfff00000)
                   || (pCtx->fs.Attr.n.u1Granularity));
            Assert(   !(pCtx->fs.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                   || (pCtx->fs.Attr.n.u4Type & X86_SEL_TYPE_READ));
        }
        if (pCtx->gs.Attr.u && !(pCtx->gs.Attr.u & X86DESCATTR_UNUSABLE))
        {
            Assert(pCtx->gs.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED);
            Assert(pCtx->gs.Attr.n.u1Present);
            Assert(pCtx->gs.Attr.n.u4Type > 11 || pCtx->gs.Attr.n.u2Dpl >= (pCtx->gs.Sel & X86_SEL_RPL));
            Assert(!(pCtx->gs.Attr.u & 0xf00));
            Assert(!(pCtx->gs.Attr.u & 0xfffe0000));
            Assert(   (pCtx->gs.u32Limit & 0xfff) == 0xfff
                   || !(pCtx->gs.Attr.n.u1Granularity));
            Assert(   !(pCtx->gs.u32Limit & 0xfff00000)
                   || (pCtx->gs.Attr.n.u1Granularity));
            Assert(   !(pCtx->gs.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                   || (pCtx->gs.Attr.n.u4Type & X86_SEL_TYPE_READ));
        }
        /* 64-bit capable CPUs. */
# if HC_ARCH_BITS == 64
        Assert(!RT_HI_U32(pCtx->cs.u64Base));
        Assert(!pCtx->ss.Attr.u || !RT_HI_U32(pCtx->ss.u64Base));
        Assert(!pCtx->ds.Attr.u || !RT_HI_U32(pCtx->ds.u64Base));
        Assert(!pCtx->es.Attr.u || !RT_HI_U32(pCtx->es.u64Base));
# endif
    }
    else if (   CPUMIsGuestInV86ModeEx(pCtx)
             || (   CPUMIsGuestInRealModeEx(pCtx)
                 && !pVM->hm.s.vmx.fUnrestrictedGuest))
    {
        /* Real and v86 mode checks. */
        /* hmR0VmxExportGuestSegmentReg() writes the modified in VMCS. We want what we're feeding to VT-x. */
        uint32_t u32CSAttr, u32SSAttr, u32DSAttr, u32ESAttr, u32FSAttr, u32GSAttr;
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            u32CSAttr = 0xf3; u32SSAttr = 0xf3; u32DSAttr = 0xf3; u32ESAttr = 0xf3; u32FSAttr = 0xf3; u32GSAttr = 0xf3;
        }
        else
        {
            u32CSAttr = pCtx->cs.Attr.u; u32SSAttr = pCtx->ss.Attr.u; u32DSAttr = pCtx->ds.Attr.u;
            u32ESAttr = pCtx->es.Attr.u; u32FSAttr = pCtx->fs.Attr.u; u32GSAttr = pCtx->gs.Attr.u;
        }

        /* CS */
        AssertMsg((pCtx->cs.u64Base == (uint64_t)pCtx->cs.Sel << 4), ("CS base %#x %#x\n", pCtx->cs.u64Base, pCtx->cs.Sel));
        Assert(pCtx->cs.u32Limit == 0xffff);
        Assert(u32CSAttr == 0xf3);
        /* SS */
        Assert(pCtx->ss.u64Base == (uint64_t)pCtx->ss.Sel << 4);
        Assert(pCtx->ss.u32Limit == 0xffff);
        Assert(u32SSAttr == 0xf3);
        /* DS */
        Assert(pCtx->ds.u64Base == (uint64_t)pCtx->ds.Sel << 4);
        Assert(pCtx->ds.u32Limit == 0xffff);
        Assert(u32DSAttr == 0xf3);
        /* ES */
        Assert(pCtx->es.u64Base == (uint64_t)pCtx->es.Sel << 4);
        Assert(pCtx->es.u32Limit == 0xffff);
        Assert(u32ESAttr == 0xf3);
        /* FS */
        Assert(pCtx->fs.u64Base == (uint64_t)pCtx->fs.Sel << 4);
        Assert(pCtx->fs.u32Limit == 0xffff);
        Assert(u32FSAttr == 0xf3);
        /* GS */
        Assert(pCtx->gs.u64Base == (uint64_t)pCtx->gs.Sel << 4);
        Assert(pCtx->gs.u32Limit == 0xffff);
        Assert(u32GSAttr == 0xf3);
        /* 64-bit capable CPUs. */
# if HC_ARCH_BITS == 64
        Assert(!RT_HI_U32(pCtx->cs.u64Base));
        Assert(!u32SSAttr || !RT_HI_U32(pCtx->ss.u64Base));
        Assert(!u32DSAttr || !RT_HI_U32(pCtx->ds.u64Base));
        Assert(!u32ESAttr || !RT_HI_U32(pCtx->es.u64Base));
# endif
    }
}
#endif /* VBOX_STRICT */


/**
 * Exports a guest segment register into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxSel      Index of the selector in the VMCS.
 * @param   idxLimit    Index of the segment limit in the VMCS.
 * @param   idxBase     Index of the segment base in the VMCS.
 * @param   idxAccess   Index of the access rights of the segment in the VMCS.
 * @param   pSelReg     Pointer to the segment selector.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestSegmentReg(PVMCPU pVCpu, uint32_t idxSel, uint32_t idxLimit, uint32_t idxBase, uint32_t idxAccess,
                                        PCCPUMSELREG pSelReg)
{
    int rc = VMXWriteVmcs32(idxSel,    pSelReg->Sel);       /* 16-bit guest selector field. */
    rc    |= VMXWriteVmcs32(idxLimit,  pSelReg->u32Limit);  /* 32-bit guest segment limit field. */
    rc    |= VMXWriteVmcsGstN(idxBase, pSelReg->u64Base);   /* Natural width guest segment base field.*/
    AssertRCReturn(rc, rc);

    uint32_t u32Access = pSelReg->Attr.u;
    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
    {
        /* VT-x requires our real-using-v86 mode hack to override the segment access-right bits. */
        u32Access = 0xf3;
        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.pRealModeTSS);
        Assert(PDMVmmDevHeapIsEnabled(pVCpu->CTX_SUFF(pVM)));
    }
    else
    {
        /*
         * The way to differentiate between whether this is really a null selector or was just
         * a selector loaded with 0 in real-mode is using the segment attributes. A selector
         * loaded in real-mode with the value 0 is valid and usable in protected-mode and we
         * should -not- mark it as an unusable segment. Both the recompiler & VT-x ensures
         * NULL selectors loaded in protected-mode have their attribute as 0.
         */
        if (!u32Access)
            u32Access = X86DESCATTR_UNUSABLE;
    }

    /* Validate segment access rights. Refer to Intel spec. "26.3.1.2 Checks on Guest Segment Registers". */
    AssertMsg((u32Access & X86DESCATTR_UNUSABLE) || (u32Access & X86_SEL_TYPE_ACCESSED),
              ("Access bit not set for usable segment. idx=%#x sel=%#x attr %#x\n", idxBase, pSelReg, pSelReg->Attr.u));

    rc = VMXWriteVmcs32(idxAccess, u32Access);              /* 32-bit guest segment access-rights field. */
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Exports the guest segment registers, GDTR, IDTR, LDTR, (TR, FS and GS bases)
 * into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks Will import guest CR0 on strict builds during validation of
 *          segments.
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestSegmentRegs(PVMCPU pVCpu)
{
    int       rc   = VERR_INTERNAL_ERROR_5;
    PVM       pVM  = pVCpu->CTX_SUFF(pVM);
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;

    /*
     * Guest Segment registers: CS, SS, DS, ES, FS, GS.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_SREG_MASK)
    {
#ifdef VBOX_WITH_REM
        if (!pVM->hm.s.vmx.fUnrestrictedGuest)
        {
            Assert(pVM->hm.s.vmx.pRealModeTSS);
            AssertCompile(PGMMODE_REAL < PGMMODE_PROTECTED);
            if (   pVCpu->hm.s.vmx.fWasInRealMode
                && PGMGetGuestMode(pVCpu) >= PGMMODE_PROTECTED)
            {
                /* Signal that the recompiler must flush its code-cache as the guest -may- rewrite code it will later execute
                   in real-mode (e.g. OpenBSD 4.0) */
                REMFlushTBs(pVM);
                Log4Func(("Switch to protected mode detected!\n"));
                pVCpu->hm.s.vmx.fWasInRealMode = false;
            }
        }
#endif
        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_CS)
        {
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_CS);
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                pVCpu->hm.s.vmx.RealMode.AttrCS.u = pCtx->cs.Attr.u;
            rc = HMVMX_EXPORT_SREG(CS, &pCtx->cs);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_CS);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_SS)
        {
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SS);
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                pVCpu->hm.s.vmx.RealMode.AttrSS.u = pCtx->ss.Attr.u;
            rc = HMVMX_EXPORT_SREG(SS, &pCtx->ss);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_SS);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_DS)
        {
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_DS);
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                pVCpu->hm.s.vmx.RealMode.AttrDS.u = pCtx->ds.Attr.u;
            rc = HMVMX_EXPORT_SREG(DS, &pCtx->ds);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_DS);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_ES)
        {
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_ES);
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                pVCpu->hm.s.vmx.RealMode.AttrES.u = pCtx->es.Attr.u;
            rc = HMVMX_EXPORT_SREG(ES, &pCtx->es);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_ES);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_FS)
        {
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_FS);
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                pVCpu->hm.s.vmx.RealMode.AttrFS.u = pCtx->fs.Attr.u;
            rc = HMVMX_EXPORT_SREG(FS, &pCtx->fs);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_FS);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_GS)
        {
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_GS);
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                pVCpu->hm.s.vmx.RealMode.AttrGS.u = pCtx->gs.Attr.u;
            rc = HMVMX_EXPORT_SREG(GS, &pCtx->gs);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_GS);
        }

#ifdef VBOX_STRICT
        hmR0VmxValidateSegmentRegs(pVCpu);
#endif

        Log4Func(("CS=%#RX16 Base=%#RX64 Limit=%#RX32 Attr=%#RX32\n", pCtx->cs.Sel, pCtx->cs.u64Base,
                  pCtx->cs.u32Limit, pCtx->cs.Attr.u));
    }

    /*
     * Guest TR.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_TR)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_TR);

        /*
         * Real-mode emulation using virtual-8086 mode with CR4.VME. Interrupt redirection is
         * achieved using the interrupt redirection bitmap (all bits cleared to let the guest
         * handle INT-n's) in the TSS. See hmR3InitFinalizeR0() to see how pRealModeTSS is setup.
         */
        uint16_t u16Sel          = 0;
        uint32_t u32Limit        = 0;
        uint64_t u64Base         = 0;
        uint32_t u32AccessRights = 0;

        if (!pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            u16Sel          = pCtx->tr.Sel;
            u32Limit        = pCtx->tr.u32Limit;
            u64Base         = pCtx->tr.u64Base;
            u32AccessRights = pCtx->tr.Attr.u;
        }
        else
        {
            Assert(pVM->hm.s.vmx.pRealModeTSS);
            Assert(PDMVmmDevHeapIsEnabled(pVM));    /* Guaranteed by HMR3CanExecuteGuest() -XXX- what about inner loop changes? */

            /* We obtain it here every time as PCI regions could be reconfigured in the guest, changing the VMMDev base. */
            RTGCPHYS GCPhys;
            rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pRealModeTSS, &GCPhys);
            AssertRCReturn(rc, rc);

            X86DESCATTR DescAttr;
            DescAttr.u           = 0;
            DescAttr.n.u1Present = 1;
            DescAttr.n.u4Type    = X86_SEL_TYPE_SYS_386_TSS_BUSY;

            u16Sel          = 0;
            u32Limit        = HM_VTX_TSS_SIZE;
            u64Base         = GCPhys;   /* in real-mode phys = virt. */
            u32AccessRights = DescAttr.u;
        }

        /* Validate. */
        Assert(!(u16Sel & RT_BIT(2)));
        AssertMsg(   (u32AccessRights & 0xf) == X86_SEL_TYPE_SYS_386_TSS_BUSY
                  || (u32AccessRights & 0xf) == X86_SEL_TYPE_SYS_286_TSS_BUSY, ("TSS is not busy!? %#x\n", u32AccessRights));
        AssertMsg(!(u32AccessRights & X86DESCATTR_UNUSABLE), ("TR unusable bit is not clear!? %#x\n", u32AccessRights));
        Assert(!(u32AccessRights & RT_BIT(4)));                 /* System MBZ.*/
        Assert(u32AccessRights & RT_BIT(7));                    /* Present MB1.*/
        Assert(!(u32AccessRights & 0xf00));                     /* 11:8 MBZ. */
        Assert(!(u32AccessRights & 0xfffe0000));                /* 31:17 MBZ. */
        Assert(   (u32Limit & 0xfff) == 0xfff
               || !(u32AccessRights & RT_BIT(15)));             /* Granularity MBZ. */
        Assert(   !(pCtx->tr.u32Limit & 0xfff00000)
               || (u32AccessRights & RT_BIT(15)));              /* Granularity MB1. */

        rc  = VMXWriteVmcs32(VMX_VMCS16_GUEST_TR_SEL,           u16Sel);
        rc |= VMXWriteVmcs32(VMX_VMCS32_GUEST_TR_LIMIT,         u32Limit);
        rc |= VMXWriteVmcs32(VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS, u32AccessRights);
        rc |= VMXWriteVmcsGstN(VMX_VMCS_GUEST_TR_BASE,          u64Base);
        AssertRCReturn(rc, rc);

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_TR);
        Log4Func(("TR base=%#RX64\n", pCtx->tr.u64Base));
    }

    /*
     * Guest GDTR.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_GDTR)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_GDTR);

        rc  = VMXWriteVmcs32(VMX_VMCS32_GUEST_GDTR_LIMIT, pCtx->gdtr.cbGdt);
        rc |= VMXWriteVmcsGstN(VMX_VMCS_GUEST_GDTR_BASE,  pCtx->gdtr.pGdt);
        AssertRCReturn(rc, rc);

        /* Validate. */
        Assert(!(pCtx->gdtr.cbGdt & 0xffff0000));          /* Bits 31:16 MBZ. */

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_GDTR);
        Log4Func(("GDTR base=%#RX64\n", pCtx->gdtr.pGdt));
    }

    /*
     * Guest LDTR.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_LDTR)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_LDTR);

        /* The unusable bit is specific to VT-x, if it's a null selector mark it as an unusable segment. */
        uint32_t u32Access = 0;
        if (!pCtx->ldtr.Attr.u)
            u32Access = X86DESCATTR_UNUSABLE;
        else
            u32Access = pCtx->ldtr.Attr.u;

        rc  = VMXWriteVmcs32(VMX_VMCS16_GUEST_LDTR_SEL,           pCtx->ldtr.Sel);
        rc |= VMXWriteVmcs32(VMX_VMCS32_GUEST_LDTR_LIMIT,         pCtx->ldtr.u32Limit);
        rc |= VMXWriteVmcs32(VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS, u32Access);
        rc |= VMXWriteVmcsGstN(VMX_VMCS_GUEST_LDTR_BASE,          pCtx->ldtr.u64Base);
        AssertRCReturn(rc, rc);

        /* Validate. */
        if (!(u32Access & X86DESCATTR_UNUSABLE))
        {
            Assert(!(pCtx->ldtr.Sel & RT_BIT(2)));              /* TI MBZ. */
            Assert(pCtx->ldtr.Attr.n.u4Type == 2);              /* Type MB2 (LDT). */
            Assert(!pCtx->ldtr.Attr.n.u1DescType);              /* System MBZ. */
            Assert(pCtx->ldtr.Attr.n.u1Present == 1);           /* Present MB1. */
            Assert(!pCtx->ldtr.Attr.n.u4LimitHigh);             /* 11:8 MBZ. */
            Assert(!(pCtx->ldtr.Attr.u & 0xfffe0000));          /* 31:17 MBZ. */
            Assert(   (pCtx->ldtr.u32Limit & 0xfff) == 0xfff
                   || !pCtx->ldtr.Attr.n.u1Granularity);        /* Granularity MBZ. */
            Assert(   !(pCtx->ldtr.u32Limit & 0xfff00000)
                   || pCtx->ldtr.Attr.n.u1Granularity);         /* Granularity MB1. */
        }

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_LDTR);
        Log4Func(("LDTR base=%#RX64\n", pCtx->ldtr.u64Base));
    }

    /*
     * Guest IDTR.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_IDTR)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_IDTR);

        rc  = VMXWriteVmcs32(VMX_VMCS32_GUEST_IDTR_LIMIT, pCtx->idtr.cbIdt);
        rc |= VMXWriteVmcsGstN(VMX_VMCS_GUEST_IDTR_BASE,  pCtx->idtr.pIdt);
        AssertRCReturn(rc, rc);

        /* Validate. */
        Assert(!(pCtx->idtr.cbIdt & 0xffff0000));          /* Bits 31:16 MBZ. */

        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_IDTR);
        Log4Func(("IDTR base=%#RX64\n", pCtx->idtr.pIdt));
    }

    return VINF_SUCCESS;
}


/**
 * Exports certain guest MSRs into the VM-entry MSR-load and VM-exit MSR-store
 * areas.
 *
 * These MSRs will automatically be loaded to the host CPU on every successful
 * VM-entry and stored from the host CPU on every successful VM-exit. This also
 * creates/updates MSR slots for the host MSRs. The actual host MSR values are
 * -not- updated here for performance reasons. See hmR0VmxExportHostMsrs().
 *
 * Also exports the guest sysenter MSRs into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportGuestMsrs(PVMCPU pVCpu)
{
    AssertPtr(pVCpu);
    AssertPtr(pVCpu->hm.s.vmx.pvGuestMsr);

    /*
     * MSRs that we use the auto-load/store MSR area in the VMCS.
     * For 64-bit hosts, we load/restore them lazily, see hmR0VmxLazyLoadGuestMsrs().
     */
    PVM       pVM  = pVCpu->CTX_SUFF(pVM);
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_VMX_GUEST_AUTO_MSRS)
    {
        if (pVM->hm.s.fAllow64BitGuests)
        {
#if HC_ARCH_BITS == 32
            HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SYSCALL_MSRS | CPUMCTX_EXTRN_KERNEL_GS_BASE);

            int rc = hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_K8_LSTAR,          pCtx->msrLSTAR,        false, NULL);
            rc    |= hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_K6_STAR,           pCtx->msrSTAR,         false, NULL);
            rc    |= hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_K8_SF_MASK,        pCtx->msrSFMASK,       false, NULL);
            rc    |= hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_K8_KERNEL_GS_BASE, pCtx->msrKERNELGSBASE, false, NULL);
            AssertRCReturn(rc, rc);
# ifdef LOG_ENABLED
            PCVMXAUTOMSR pMsr = (PCVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
            for (uint32_t i = 0; i < pVCpu->hm.s.vmx.cMsrs; i++, pMsr++)
                Log4Func(("MSR[%RU32]: u32Msr=%#RX32 u64Value=%#RX64\n", i, pMsr->u32Msr, pMsr->u64Value));
# endif
#endif
        }
        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_VMX_GUEST_AUTO_MSRS);
    }

    /*
     * Guest Sysenter MSRs.
     * These flags are only set when MSR-bitmaps are not supported by the CPU and we cause
     * VM-exits on WRMSRs for these MSRs.
     */
    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_SYSENTER_MSR_MASK)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_SYSENTER_MSRS);

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_SYSENTER_CS_MSR)
        {
            int rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_SYSENTER_CS, pCtx->SysEnter.cs);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_SYSENTER_CS_MSR);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_SYSENTER_EIP_MSR)
        {
            int rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_SYSENTER_EIP, pCtx->SysEnter.eip);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_SYSENTER_EIP_MSR);
        }

        if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_SYSENTER_ESP_MSR)
        {
            int rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_SYSENTER_ESP, pCtx->SysEnter.esp);
            AssertRCReturn(rc, rc);
            ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_SYSENTER_ESP_MSR);
        }
    }

    if (ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged) & HM_CHANGED_GUEST_EFER_MSR)
    {
        HMVMX_CPUMCTX_ASSERT(pVCpu, CPUMCTX_EXTRN_EFER);

        if (hmR0VmxShouldSwapEferMsr(pVCpu))
        {
            /*
             * If the CPU supports VMCS controls for swapping EFER, use it. Otherwise, we have no option
             * but to use the auto-load store MSR area in the VMCS for swapping EFER. See @bugref{7368}.
             */
            if (pVM->hm.s.vmx.fSupportsVmcsEfer)
            {
                int rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_EFER_FULL, pCtx->msrEFER);
                AssertRCReturn(rc,rc);
                Log4Func(("EFER=%#RX64\n", pCtx->msrEFER));
            }
            else
            {
                int rc = hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_K6_EFER, pCtx->msrEFER, false /* fUpdateHostMsr */,
                                                    NULL /* pfAddedAndUpdated */);
                AssertRCReturn(rc, rc);

                /* We need to intercept reads too, see @bugref{7386#c16}. */
                if (pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
                    hmR0VmxSetMsrPermission(pVCpu, MSR_K6_EFER, VMXMSREXIT_INTERCEPT_READ, VMXMSREXIT_INTERCEPT_WRITE);
                Log4Func(("MSR[--]: u32Msr=%#RX32 u64Value=%#RX64 cMsrs=%u\n", MSR_K6_EFER, pCtx->msrEFER,
                          pVCpu->hm.s.vmx.cMsrs));
            }
        }
        else if (!pVM->hm.s.vmx.fSupportsVmcsEfer)
            hmR0VmxRemoveAutoLoadStoreMsr(pVCpu, MSR_K6_EFER);
        ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~HM_CHANGED_GUEST_EFER_MSR);
    }

    return VINF_SUCCESS;
}


#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
/**
 * Check if guest state allows safe use of 32-bit switcher again.
 *
 * Segment bases and protected mode structures must be 32-bit addressable
 * because the  32-bit switcher will ignore high dword when writing these VMCS
 * fields.  See @bugref{8432} for details.
 *
 * @returns true if safe, false if must continue to use the 64-bit switcher.
 * @param   pCtx   Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static bool hmR0VmxIs32BitSwitcherSafe(PCCPUMCTX pCtx)
{
    if (pCtx->gdtr.pGdt    & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->idtr.pIdt    & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->ldtr.u64Base & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->tr.u64Base   & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->es.u64Base   & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->cs.u64Base   & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->ss.u64Base   & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->ds.u64Base   & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->fs.u64Base   & UINT64_C(0xffffffff00000000))     return false;
    if (pCtx->gs.u64Base   & UINT64_C(0xffffffff00000000))     return false;

    /* All good, bases are 32-bit. */
    return true;
}
#endif


/**
 * Selects up the appropriate function to run guest code.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSelectVMRunHandler(PVMCPU pVCpu)
{
    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
#ifndef VBOX_ENABLE_64_BITS_GUESTS
        return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif
        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests);    /* Guaranteed by hmR3InitFinalizeR0(). */
#if HC_ARCH_BITS == 32
        /* 32-bit host. We need to switch to 64-bit before running the 64-bit guest. */
        if (pVCpu->hm.s.vmx.pfnStartVM != VMXR0SwitcherStartVM64)
        {
#ifdef VBOX_STRICT
            if (pVCpu->hm.s.vmx.pfnStartVM != NULL) /* Very first entry would have saved host-state already, ignore it. */
            {
                /* Currently, all mode changes sends us back to ring-3, so these should be set. See @bugref{6944}. */
                uint64_t const fCtxChanged = ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged);
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
                AssertMsg(fCtxChanged & (  HM_CHANGED_VMX_EXIT_CTLS
                                         | HM_CHANGED_VMX_ENTRY_CTLS
                                         | HM_CHANGED_GUEST_EFER_MSR), ("fCtxChanged=%#RX64\n", fCtxChanged));
            }
#endif
            pVCpu->hm.s.vmx.pfnStartVM = VMXR0SwitcherStartVM64;

            /* Mark that we've switched to 64-bit handler, we can't safely switch back to 32-bit for
               the rest of the VM run (until VM reset). See @bugref{8432#c7}. */
            pVCpu->hm.s.vmx.fSwitchedTo64on32 = true;
            Log4Func(("Selected 64-bit switcher\n"));
        }
#else
        /* 64-bit host. */
        pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM64;
#endif
    }
    else
    {
        /* Guest is not in long mode, use the 32-bit handler. */
#if HC_ARCH_BITS == 32
        if (    pVCpu->hm.s.vmx.pfnStartVM != VMXR0StartVM32
            && !pVCpu->hm.s.vmx.fSwitchedTo64on32   /* If set, guest mode change does not imply switcher change. */
            &&  pVCpu->hm.s.vmx.pfnStartVM != NULL) /* Very first entry would have saved host-state already, ignore it. */
        {
# ifdef VBOX_STRICT
            /* Currently, all mode changes sends us back to ring-3, so these should be set. See @bugref{6944}. */
            uint64_t const fCtxChanged = ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged);
            RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
            AssertMsg(fCtxChanged & (  HM_CHANGED_VMX_EXIT_CTLS
                                     | HM_CHANGED_VMX_ENTRY_CTLS
                                     | HM_CHANGED_GUEST_EFER_MSR), ("fCtxChanged=%#RX64\n", fCtxChanged));
# endif
        }
# ifdef VBOX_ENABLE_64_BITS_GUESTS
        /*
         * Keep using the 64-bit switcher even though we're in 32-bit because of bad Intel
         * design, see @bugref{8432#c7}. If real-on-v86 mode is active, clear the 64-bit
         * switcher flag because now we know the guest is in a sane state where it's safe
         * to use the 32-bit switcher. Otherwise check the guest state if it's safe to use
         * the much faster 32-bit switcher again.
         */
        if (!pVCpu->hm.s.vmx.fSwitchedTo64on32)
        {
            if (pVCpu->hm.s.vmx.pfnStartVM != VMXR0StartVM32)
                Log4Func(("Selected 32-bit switcher\n"));
            pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM32;
        }
        else
        {
            Assert(pVCpu->hm.s.vmx.pfnStartVM == VMXR0SwitcherStartVM64);
            if (   pVCpu->hm.s.vmx.RealMode.fRealOnV86Active
                || hmR0VmxIs32BitSwitcherSafe(pCtx))
            {
                pVCpu->hm.s.vmx.fSwitchedTo64on32 = false;
                pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM32;
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_EFER_MSR
                                                         | HM_CHANGED_VMX_ENTRY_CTLS
                                                         | HM_CHANGED_VMX_EXIT_CTLS
                                                         | HM_CHANGED_HOST_CONTEXT);
                Log4Func(("Selected 32-bit switcher (safe)\n"));
            }
        }
# else
        pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM32;
# endif
#else
        pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM32;
#endif
    }
    Assert(pVCpu->hm.s.vmx.pfnStartVM);
    return VINF_SUCCESS;
}


/**
 * Wrapper for running the guest code in VT-x.
 *
 * @returns VBox status code, no informational status codes.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxRunGuest(PVMCPU pVCpu)
{
    /* Mark that HM is the keeper of all guest-CPU registers now that we're going to execute guest code. */
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    pCtx->fExtrn |= HMVMX_CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_KEEPER_HM;

    /*
     * 64-bit Windows uses XMM registers in the kernel as the Microsoft compiler expresses
     * floating-point operations using SSE instructions. Some XMM registers (XMM6-XMM15) are
     * callee-saved and thus the need for this XMM wrapper.
     *
     * See MSDN "Configuring Programs for 64-bit/x64 Software Conventions / Register Usage".
     */
    bool const fResumeVM = RT_BOOL(pVCpu->hm.s.vmx.uVmcsState & HMVMX_VMCS_STATE_LAUNCHED);
    /** @todo Add stats for resume vs launch. */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
#ifdef VBOX_WITH_KERNEL_USING_XMM
    int rc = hmR0VMXStartVMWrapXMM(fResumeVM, pCtx, &pVCpu->hm.s.vmx.VMCSCache, pVM, pVCpu, pVCpu->hm.s.vmx.pfnStartVM);
#else
    int rc = pVCpu->hm.s.vmx.pfnStartVM(fResumeVM, pCtx, &pVCpu->hm.s.vmx.VMCSCache, pVM, pVCpu);
#endif
    AssertMsg(rc <= VINF_SUCCESS, ("%Rrc\n", rc));
    return rc;
}


/**
 * Reports world-switch error and dumps some useful debug info.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   rcVMRun         The return code from VMLAUNCH/VMRESUME.
 * @param   pVmxTransient   Pointer to the VMX transient structure (only
 *                          exitReason updated).
 */
static void hmR0VmxReportWorldSwitchError(PVMCPU pVCpu, int rcVMRun, PVMXTRANSIENT pVmxTransient)
{
    Assert(pVCpu);
    Assert(pVmxTransient);
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);

    Log4Func(("VM-entry failure: %Rrc\n", rcVMRun));
    switch (rcVMRun)
    {
        case VERR_VMX_INVALID_VMXON_PTR:
            AssertFailed();
            break;
        case VINF_SUCCESS:                  /* VMLAUNCH/VMRESUME succeeded but VM-entry failed... yeah, true story. */
        case VERR_VMX_UNABLE_TO_START_VM:   /* VMLAUNCH/VMRESUME itself failed. */
        {
            int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_REASON, &pVCpu->hm.s.vmx.LastError.u32ExitReason);
            rc    |= VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &pVCpu->hm.s.vmx.LastError.u32InstrError);
            rc    |= hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
            AssertRC(rc);

            pVCpu->hm.s.vmx.LastError.idEnteredCpu = pVCpu->hm.s.idEnteredCpu;
            /* LastError.idCurrentCpu was already updated in hmR0VmxPreRunGuestCommitted().
               Cannot do it here as we may have been long preempted. */

#ifdef VBOX_STRICT
                Log4(("uExitReason        %#RX32 (VmxTransient %#RX16)\n", pVCpu->hm.s.vmx.LastError.u32ExitReason,
                     pVmxTransient->uExitReason));
                Log4(("Exit Qualification %#RX64\n", pVmxTransient->uExitQualification));
                Log4(("InstrError         %#RX32\n", pVCpu->hm.s.vmx.LastError.u32InstrError));
                if (pVCpu->hm.s.vmx.LastError.u32InstrError <= HMVMX_INSTR_ERROR_MAX)
                    Log4(("InstrError Desc.  \"%s\"\n", g_apszVmxInstrErrors[pVCpu->hm.s.vmx.LastError.u32InstrError]));
                else
                    Log4(("InstrError Desc.    Range exceeded %u\n", HMVMX_INSTR_ERROR_MAX));
                Log4(("Entered host CPU   %u\n", pVCpu->hm.s.vmx.LastError.idEnteredCpu));
                Log4(("Current host CPU   %u\n", pVCpu->hm.s.vmx.LastError.idCurrentCpu));

                /* VMX control bits. */
                uint32_t        u32Val;
                uint64_t        u64Val;
                RTHCUINTREG     uHCReg;
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PIN_EXEC, &u32Val);                  AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PIN_EXEC                %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, &u32Val);                 AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PROC_EXEC               %#RX32\n", u32Val));
                if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                {
                    rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, &u32Val);            AssertRC(rc);
                    Log4(("VMX_VMCS32_CTRL_PROC_EXEC2              %#RX32\n", u32Val));
                }
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY, &u32Val);                     AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_ENTRY                   %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXIT, &u32Val);                      AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_EXIT                    %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_CR3_TARGET_COUNT, &u32Val);          AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_CR3_TARGET_COUNT        %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, &u32Val);   AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE, &u32Val);   AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH, &u32Val);        AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH      %u\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_TPR_THRESHOLD, &u32Val);             AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_TPR_THRESHOLD           %u\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT, &u32Val);      AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT    %u (guest MSRs)\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT, &u32Val);       AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT     %u (host MSRs)\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT, &u32Val);      AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT    %u (guest MSRs)\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, &u32Val);          AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_EXCEPTION_BITMAP        %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK, &u32Val);      AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK    %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH, &u32Val);     AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH   %#RX32\n", u32Val));
                rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR0_MASK, &uHCReg);                  AssertRC(rc);
                Log4(("VMX_VMCS_CTRL_CR0_MASK                  %#RHr\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR0_READ_SHADOW, &uHCReg);           AssertRC(rc);
                Log4(("VMX_VMCS_CTRL_CR4_READ_SHADOW           %#RHr\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR4_MASK, &uHCReg);                  AssertRC(rc);
                Log4(("VMX_VMCS_CTRL_CR4_MASK                  %#RHr\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR4_READ_SHADOW, &uHCReg);           AssertRC(rc);
                Log4(("VMX_VMCS_CTRL_CR4_READ_SHADOW           %#RHr\n", uHCReg));
                if (pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging)
                {
                    rc = VMXReadVmcs64(VMX_VMCS64_CTRL_EPTP_FULL, &u64Val);             AssertRC(rc);
                    Log4(("VMX_VMCS64_CTRL_EPTP_FULL               %#RX64\n", u64Val));
                }

                /* Guest bits. */
                rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RIP, &u64Val);          AssertRC(rc);
                Log4(("Old Guest Rip %#RX64 New %#RX64\n", pVCpu->cpum.GstCtx.rip, u64Val));
                rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RSP, &u64Val);          AssertRC(rc);
                Log4(("Old Guest Rsp %#RX64 New %#RX64\n", pVCpu->cpum.GstCtx.rsp, u64Val));
                rc = VMXReadVmcs32(VMX_VMCS_GUEST_RFLAGS, &u32Val);         AssertRC(rc);
                Log4(("Old Guest Rflags %#RX32 New %#RX32\n", pVCpu->cpum.GstCtx.eflags.u32, u32Val));
                if (pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fVpid)
                {
                    rc = VMXReadVmcs32(VMX_VMCS16_VPID, &u32Val);           AssertRC(rc);
                    Log4(("VMX_VMCS16_VPID  %u\n", u32Val));
                }

                /* Host bits. */
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_CR0, &uHCReg);           AssertRC(rc);
                Log4(("Host CR0 %#RHr\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_CR3, &uHCReg);           AssertRC(rc);
                Log4(("Host CR3 %#RHr\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_CR4, &uHCReg);           AssertRC(rc);
                Log4(("Host CR4 %#RHr\n", uHCReg));

                RTGDTR      HostGdtr;
                PCX86DESCHC pDesc;
                ASMGetGDTR(&HostGdtr);
                rc = VMXReadVmcs32(VMX_VMCS16_HOST_CS_SEL, &u32Val);      AssertRC(rc);
                Log4(("Host CS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "CS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_DS_SEL, &u32Val);      AssertRC(rc);
                Log4(("Host DS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "DS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_ES_SEL, &u32Val);      AssertRC(rc);
                Log4(("Host ES %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "ES: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FS_SEL, &u32Val);      AssertRC(rc);
                Log4(("Host FS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "FS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_GS_SEL, &u32Val);      AssertRC(rc);
                Log4(("Host GS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "GS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_SS_SEL, &u32Val);      AssertRC(rc);
                Log4(("Host SS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "SS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_TR_SEL,  &u32Val);     AssertRC(rc);
                Log4(("Host TR %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    hmR0DumpDescriptor(pDesc, u32Val, "TR: ");
                }

                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_TR_BASE, &uHCReg);       AssertRC(rc);
                Log4(("Host TR Base %#RHv\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_GDTR_BASE, &uHCReg);     AssertRC(rc);
                Log4(("Host GDTR Base %#RHv\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_IDTR_BASE, &uHCReg);     AssertRC(rc);
                Log4(("Host IDTR Base %#RHv\n", uHCReg));
                rc = VMXReadVmcs32(VMX_VMCS32_HOST_SYSENTER_CS, &u32Val);   AssertRC(rc);
                Log4(("Host SYSENTER CS  %#08x\n", u32Val));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_SYSENTER_EIP, &uHCReg);  AssertRC(rc);
                Log4(("Host SYSENTER EIP %#RHv\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_SYSENTER_ESP, &uHCReg);  AssertRC(rc);
                Log4(("Host SYSENTER ESP %#RHv\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_RSP, &uHCReg);           AssertRC(rc);
                Log4(("Host RSP %#RHv\n", uHCReg));
                rc = VMXReadVmcsHstN(VMX_VMCS_HOST_RIP, &uHCReg);           AssertRC(rc);
                Log4(("Host RIP %#RHv\n", uHCReg));
# if HC_ARCH_BITS == 64
                Log4(("MSR_K6_EFER            = %#RX64\n", ASMRdMsr(MSR_K6_EFER)));
                Log4(("MSR_K8_CSTAR           = %#RX64\n", ASMRdMsr(MSR_K8_CSTAR)));
                Log4(("MSR_K8_LSTAR           = %#RX64\n", ASMRdMsr(MSR_K8_LSTAR)));
                Log4(("MSR_K6_STAR            = %#RX64\n", ASMRdMsr(MSR_K6_STAR)));
                Log4(("MSR_K8_SF_MASK         = %#RX64\n", ASMRdMsr(MSR_K8_SF_MASK)));
                Log4(("MSR_K8_KERNEL_GS_BASE  = %#RX64\n", ASMRdMsr(MSR_K8_KERNEL_GS_BASE)));
# endif
#endif /* VBOX_STRICT */
            break;
        }

        default:
            /* Impossible */
            AssertMsgFailed(("hmR0VmxReportWorldSwitchError %Rrc (%#x)\n", rcVMRun, rcVMRun));
            break;
    }
}


#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
#ifndef VMX_USE_CACHED_VMCS_ACCESSES
# error "VMX_USE_CACHED_VMCS_ACCESSES not defined when it should be!"
#endif
#ifdef VBOX_STRICT
static bool hmR0VmxIsValidWriteField(uint32_t idxField)
{
    switch (idxField)
    {
        case VMX_VMCS_GUEST_RIP:
        case VMX_VMCS_GUEST_RSP:
        case VMX_VMCS_GUEST_SYSENTER_EIP:
        case VMX_VMCS_GUEST_SYSENTER_ESP:
        case VMX_VMCS_GUEST_GDTR_BASE:
        case VMX_VMCS_GUEST_IDTR_BASE:
        case VMX_VMCS_GUEST_CS_BASE:
        case VMX_VMCS_GUEST_DS_BASE:
        case VMX_VMCS_GUEST_ES_BASE:
        case VMX_VMCS_GUEST_FS_BASE:
        case VMX_VMCS_GUEST_GS_BASE:
        case VMX_VMCS_GUEST_SS_BASE:
        case VMX_VMCS_GUEST_LDTR_BASE:
        case VMX_VMCS_GUEST_TR_BASE:
        case VMX_VMCS_GUEST_CR3:
            return true;
    }
    return false;
}

static bool hmR0VmxIsValidReadField(uint32_t idxField)
{
    switch (idxField)
    {
        /* Read-only fields. */
        case VMX_VMCS_RO_EXIT_QUALIFICATION:
            return true;
    }
    /* Remaining readable fields should also be writable. */
    return hmR0VmxIsValidWriteField(idxField);
}
#endif /* VBOX_STRICT */


/**
 * Executes the specified handler in 64-bit mode.
 *
 * @returns VBox status code (no informational status codes).
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmOp       The operation to perform.
 * @param   cParams     Number of parameters.
 * @param   paParam     Array of 32-bit parameters.
 */
VMMR0DECL(int) VMXR0Execute64BitsHandler(PVMCPU pVCpu, HM64ON32OP enmOp, uint32_t cParams, uint32_t *paParam)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    AssertReturn(pVM->hm.s.pfnHost32ToGuest64R0, VERR_HM_NO_32_TO_64_SWITCHER);
    Assert(enmOp > HM64ON32OP_INVALID && enmOp < HM64ON32OP_END);
    Assert(pVCpu->hm.s.vmx.VMCSCache.Write.cValidEntries <= RT_ELEMENTS(pVCpu->hm.s.vmx.VMCSCache.Write.aField));
    Assert(pVCpu->hm.s.vmx.VMCSCache.Read.cValidEntries <= RT_ELEMENTS(pVCpu->hm.s.vmx.VMCSCache.Read.aField));

#ifdef VBOX_STRICT
    for (uint32_t i = 0; i < pVCpu->hm.s.vmx.VMCSCache.Write.cValidEntries; i++)
        Assert(hmR0VmxIsValidWriteField(pVCpu->hm.s.vmx.VMCSCache.Write.aField[i]));

    for (uint32_t i = 0; i <pVCpu->hm.s.vmx.VMCSCache.Read.cValidEntries; i++)
        Assert(hmR0VmxIsValidReadField(pVCpu->hm.s.vmx.VMCSCache.Read.aField[i]));
#endif

    /* Disable interrupts. */
    RTCCUINTREG fOldEFlags = ASMIntDisableFlags();

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    RTCPUID idHostCpu = RTMpCpuId();
    CPUMR0SetLApic(pVCpu, idHostCpu);
#endif

    PHMGLOBALCPUINFO pCpu = hmR0GetCurrentCpu();
    RTHCPHYS HCPhysCpuPage = pCpu->HCPhysMemObj;

    /* Clear VMCS. Marking it inactive, clearing implementation-specific data and writing VMCS data back to memory. */
    VMXClearVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
    pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_CLEAR;

    /* Leave VMX Root Mode. */
    VMXDisable();

    SUPR0ChangeCR4(0, ~X86_CR4_VMXE);

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVCpu));
    CPUMSetHyperEIP(pVCpu, enmOp);
    for (int i = (int)cParams - 1; i >= 0; i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatWorldSwitch3264, z);

    /* Call the switcher. */
    int rc = pVM->hm.s.pfnHost32ToGuest64R0(pVM, RT_OFFSETOF(VM, aCpus[pVCpu->idCpu].cpum) - RT_OFFSETOF(VM, cpum));
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatWorldSwitch3264, z);

    /** @todo replace with hmR0VmxEnterRootMode() and hmR0VmxLeaveRootMode(). */
    /* Make sure the VMX instructions don't cause #UD faults. */
    SUPR0ChangeCR4(X86_CR4_VMXE, RTCCUINTREG_MAX);

    /* Re-enter VMX Root Mode */
    int rc2 = VMXEnable(HCPhysCpuPage);
    if (RT_FAILURE(rc2))
    {
        SUPR0ChangeCR4(0, ~X86_CR4_VMXE);
        ASMSetFlags(fOldEFlags);
        pVM->hm.s.vmx.HCPhysVmxEnableError = HCPhysCpuPage;
        return rc2;
    }

    rc2 = VMXActivateVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
    AssertRC(rc2);
    pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_ACTIVE;
    Assert(!(ASMGetFlags() & X86_EFL_IF));
    ASMSetFlags(fOldEFlags);
    return rc;
}


/**
 * Prepares for and executes VMLAUNCH (64-bit guests) for 32-bit hosts
 * supporting 64-bit guests.
 *
 * @returns VBox status code.
 * @param   fResume     Whether to VMLAUNCH or VMRESUME.
 * @param   pCtx        Pointer to the guest-CPU context.
 * @param   pCache      Pointer to the VMCS cache.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLASM(int) VMXR0SwitcherStartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu)
{
    NOREF(fResume);

    PHMGLOBALCPUINFO pCpu = hmR0GetCurrentCpu();
    RTHCPHYS HCPhysCpuPage = pCpu->HCPhysMemObj;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCache->uPos = 1;
    pCache->interPD = PGMGetInterPaeCR3(pVM);
    pCache->pSwitcher = (uint64_t)pVM->hm.s.pfnHost32ToGuest64R0;
#endif

#if defined(DEBUG) && defined(VMX_USE_CACHED_VMCS_ACCESSES)
    pCache->TestIn.HCPhysCpuPage = 0;
    pCache->TestIn.HCPhysVmcs    = 0;
    pCache->TestIn.pCache        = 0;
    pCache->TestOut.HCPhysVmcs   = 0;
    pCache->TestOut.pCache       = 0;
    pCache->TestOut.pCtx         = 0;
    pCache->TestOut.eflags       = 0;
#else
    NOREF(pCache);
#endif

    uint32_t aParam[10];
    aParam[0] = RT_LO_U32(HCPhysCpuPage);                               /* Param 1: VMXON physical address - Lo. */
    aParam[1] = RT_HI_U32(HCPhysCpuPage);                               /* Param 1: VMXON physical address - Hi. */
    aParam[2] = RT_LO_U32(pVCpu->hm.s.vmx.HCPhysVmcs);                  /* Param 2: VMCS physical address - Lo. */
    aParam[3] = RT_HI_U32(pVCpu->hm.s.vmx.HCPhysVmcs);                  /* Param 2: VMCS physical address - Hi. */
    aParam[4] = VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hm.s.vmx.VMCSCache);
    aParam[5] = 0;
    aParam[6] = VM_RC_ADDR(pVM, pVM);
    aParam[7] = 0;
    aParam[8] = VM_RC_ADDR(pVM, pVCpu);
    aParam[9] = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCtx->dr[4] = pVM->hm.s.vmx.pScratchPhys + 16 + 8;
    *(uint32_t *)(pVM->hm.s.vmx.pScratch + 16 + 8) = 1;
#endif
    int rc = VMXR0Execute64BitsHandler(pVCpu, HM64ON32OP_VMXRCStartVM64, RT_ELEMENTS(aParam), &aParam[0]);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    Assert(*(uint32_t *)(pVM->hm.s.vmx.pScratch + 16 + 8) == 5);
    Assert(pCtx->dr[4] == 10);
    *(uint32_t *)(pVM->hm.s.vmx.pScratch + 16 + 8) = 0xff;
#endif

#if defined(DEBUG) && defined(VMX_USE_CACHED_VMCS_ACCESSES)
    AssertMsg(pCache->TestIn.HCPhysCpuPage == HCPhysCpuPage, ("%RHp vs %RHp\n", pCache->TestIn.HCPhysCpuPage, HCPhysCpuPage));
    AssertMsg(pCache->TestIn.HCPhysVmcs    == pVCpu->hm.s.vmx.HCPhysVmcs, ("%RHp vs %RHp\n", pCache->TestIn.HCPhysVmcs,
                                                                           pVCpu->hm.s.vmx.HCPhysVmcs));
    AssertMsg(pCache->TestIn.HCPhysVmcs    == pCache->TestOut.HCPhysVmcs, ("%RHp vs %RHp\n", pCache->TestIn.HCPhysVmcs,
                                                                           pCache->TestOut.HCPhysVmcs));
    AssertMsg(pCache->TestIn.pCache        == pCache->TestOut.pCache, ("%RGv vs %RGv\n", pCache->TestIn.pCache,
                                                                       pCache->TestOut.pCache));
    AssertMsg(pCache->TestIn.pCache        == VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hm.s.vmx.VMCSCache),
              ("%RGv vs %RGv\n", pCache->TestIn.pCache, VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hm.s.vmx.VMCSCache)));
    AssertMsg(pCache->TestIn.pCtx          == pCache->TestOut.pCtx, ("%RGv vs %RGv\n", pCache->TestIn.pCtx,
                                                                     pCache->TestOut.pCtx));
    Assert(!(pCache->TestOut.eflags & X86_EFL_IF));
#endif
    NOREF(pCtx);
    return rc;
}


/**
 * Initialize the VMCS-Read cache.
 *
 * The VMCS cache is used for 32-bit hosts running 64-bit guests (except 32-bit
 * Darwin which runs with 64-bit paging in 32-bit mode) for 64-bit fields that
 * cannot be accessed in 32-bit mode. Some 64-bit fields -can- be accessed
 * (those that have a 32-bit FULL & HIGH part).
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
static int hmR0VmxInitVmcsReadCache(PVMCPU pVCpu)
{
#define VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, idxField)           \
    do {                                                           \
        Assert(pCache->Read.aField[idxField##_CACHE_IDX] == 0);    \
        pCache->Read.aField[idxField##_CACHE_IDX]    = idxField;   \
        pCache->Read.aFieldVal[idxField##_CACHE_IDX] = 0;          \
        ++cReadFields;                                             \
    } while (0)

    PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;
    uint32_t cReadFields = 0;

    /*
     * Don't remove the #if 0'd fields in this code. They're listed here for consistency
     * and serve to indicate exceptions to the rules.
     */

    /* Guest-natural selector base fields. */
#if 0
    /* These are 32-bit in practice. See Intel spec. 2.5 "Control Registers". */
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_CR0);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_CR4);
#endif
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_ES_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_CS_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_SS_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_DS_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_FS_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_GS_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_LDTR_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_TR_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_GDTR_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_IDTR_BASE);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_RSP);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_RIP);
#if 0
    /* Unused natural width guest-state fields. */
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_CR3); /* Handled in Nested Paging case */
#endif
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_SYSENTER_ESP);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_SYSENTER_EIP);

    /* 64-bit guest-state fields; unused as we use two 32-bit VMREADs for
       these 64-bit fields (using "FULL" and "HIGH" fields). */
#if 0
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_DEBUGCTL_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_PAT_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_EFER_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_PDPTE0_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_PDPTE1_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_PDPTE2_FULL);
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS64_GUEST_PDPTE3_FULL);
#endif

    /* Natural width guest-state fields. */
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_RO_EXIT_QUALIFICATION);
#if 0
    /* Currently unused field. */
    VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_RO_EXIT_GUEST_LINEAR_ADDR);
#endif

    if (pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging)
    {
        VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, VMX_VMCS_GUEST_CR3);
        AssertMsg(cReadFields == VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX, ("cReadFields=%u expected %u\n", cReadFields,
                                                                        VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX));
        pCache->Read.cValidEntries = VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX;
    }
    else
    {
        AssertMsg(cReadFields == VMX_VMCS_MAX_CACHE_IDX, ("cReadFields=%u expected %u\n", cReadFields, VMX_VMCS_MAX_CACHE_IDX));
        pCache->Read.cValidEntries = VMX_VMCS_MAX_CACHE_IDX;
    }

#undef VMXLOCAL_INIT_READ_CACHE_FIELD
    return VINF_SUCCESS;
}


/**
 * Writes a field into the VMCS. This can either directly invoke a VMWRITE or
 * queue up the VMWRITE by using the VMCS write cache (on 32-bit hosts, except
 * darwin, running 64-bit guests).
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   idxField        The VMCS field encoding.
 * @param   u64Val          16, 32 or 64-bit value.
 */
VMMR0DECL(int) VMXWriteVmcs64Ex(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val)
{
    int rc;
    switch (idxField)
    {
        /*
         * These fields consists of a "FULL" and a "HIGH" part which can be written to individually.
         */
        /* 64-bit Control fields. */
        case VMX_VMCS64_CTRL_IO_BITMAP_A_FULL:
        case VMX_VMCS64_CTRL_IO_BITMAP_B_FULL:
        case VMX_VMCS64_CTRL_MSR_BITMAP_FULL:
        case VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL:
        case VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL:
        case VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL:
        case VMX_VMCS64_CTRL_EXEC_VMCS_PTR_FULL:
        case VMX_VMCS64_CTRL_TSC_OFFSET_FULL:
        case VMX_VMCS64_CTRL_VAPIC_PAGEADDR_FULL:
        case VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL:
        case VMX_VMCS64_CTRL_VMFUNC_CTRLS_FULL:
        case VMX_VMCS64_CTRL_EPTP_FULL:
        case VMX_VMCS64_CTRL_EPTP_LIST_FULL:
        /* 64-bit Guest-state fields. */
        case VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL:
        case VMX_VMCS64_GUEST_DEBUGCTL_FULL:
        case VMX_VMCS64_GUEST_PAT_FULL:
        case VMX_VMCS64_GUEST_EFER_FULL:
        case VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL:
        case VMX_VMCS64_GUEST_PDPTE0_FULL:
        case VMX_VMCS64_GUEST_PDPTE1_FULL:
        case VMX_VMCS64_GUEST_PDPTE2_FULL:
        case VMX_VMCS64_GUEST_PDPTE3_FULL:
        /* 64-bit Host-state fields. */
        case VMX_VMCS64_HOST_PAT_FULL:
        case VMX_VMCS64_HOST_EFER_FULL:
        case VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_FULL:
        {
            rc  = VMXWriteVmcs32(idxField,     RT_LO_U32(u64Val));
            rc |= VMXWriteVmcs32(idxField + 1, RT_HI_U32(u64Val));
            break;
        }

        /*
         * These fields do not have high and low parts. Queue up the VMWRITE by using the VMCS write-cache (for 64-bit
         * values). When we switch the host to 64-bit mode for running 64-bit guests, these VMWRITEs get executed then.
         */
        /* Natural-width Guest-state fields.  */
        case VMX_VMCS_GUEST_CR3:
        case VMX_VMCS_GUEST_ES_BASE:
        case VMX_VMCS_GUEST_CS_BASE:
        case VMX_VMCS_GUEST_SS_BASE:
        case VMX_VMCS_GUEST_DS_BASE:
        case VMX_VMCS_GUEST_FS_BASE:
        case VMX_VMCS_GUEST_GS_BASE:
        case VMX_VMCS_GUEST_LDTR_BASE:
        case VMX_VMCS_GUEST_TR_BASE:
        case VMX_VMCS_GUEST_GDTR_BASE:
        case VMX_VMCS_GUEST_IDTR_BASE:
        case VMX_VMCS_GUEST_RSP:
        case VMX_VMCS_GUEST_RIP:
        case VMX_VMCS_GUEST_SYSENTER_ESP:
        case VMX_VMCS_GUEST_SYSENTER_EIP:
        {
            if (!(RT_HI_U32(u64Val)))
            {
                /* If this field is 64-bit, VT-x will zero out the top bits. */
                rc = VMXWriteVmcs32(idxField, RT_LO_U32(u64Val));
            }
            else
            {
                /* Assert that only the 32->64 switcher case should ever come here. */
                Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests);
                rc = VMXWriteCachedVmcsEx(pVCpu, idxField, u64Val);
            }
            break;
        }

        default:
        {
            AssertMsgFailed(("VMXWriteVmcs64Ex: Invalid field %#RX32 (pVCpu=%p u64Val=%#RX64)\n", idxField, pVCpu, u64Val));
            rc = VERR_INVALID_PARAMETER;
            break;
        }
    }
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Queue up a VMWRITE by using the VMCS write cache.
 * This is only used on 32-bit hosts (except darwin) for 64-bit guests.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxField    The VMCS field encoding.
 * @param   u64Val      16, 32 or 64-bit value.
 */
VMMR0DECL(int) VMXWriteCachedVmcsEx(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val)
{
    AssertPtr(pVCpu);
    PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;

    AssertMsgReturn(pCache->Write.cValidEntries < VMCSCACHE_MAX_ENTRY - 1,
                    ("entries=%u\n", pCache->Write.cValidEntries), VERR_ACCESS_DENIED);

    /* Make sure there are no duplicates. */
    for (uint32_t i = 0; i < pCache->Write.cValidEntries; i++)
    {
        if (pCache->Write.aField[i] == idxField)
        {
            pCache->Write.aFieldVal[i] = u64Val;
            return VINF_SUCCESS;
        }
    }

    pCache->Write.aField[pCache->Write.cValidEntries]    = idxField;
    pCache->Write.aFieldVal[pCache->Write.cValidEntries] = u64Val;
    pCache->Write.cValidEntries++;
    return VINF_SUCCESS;
}
#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) */


/**
 * Sets up the usage of TSC-offsetting and updates the VMCS.
 *
 * If offsetting is not possible, cause VM-exits on RDTSC(P)s. Also sets up the
 * VMX preemption timer.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxUpdateTscOffsettingAndPreemptTimer(PVMCPU pVCpu)
{
    bool     fOffsettedTsc;
    bool     fParavirtTsc;
    PVM      pVM = pVCpu->CTX_SUFF(pVM);
    uint64_t uTscOffset;
    if (pVM->hm.s.vmx.fUsePreemptTimer)
    {
        uint64_t cTicksToDeadline = TMCpuTickGetDeadlineAndTscOffset(pVM, pVCpu, &uTscOffset, &fOffsettedTsc, &fParavirtTsc);

        /* Make sure the returned values have sane upper and lower boundaries. */
        uint64_t u64CpuHz  = SUPGetCpuHzFromGipBySetIndex(g_pSUPGlobalInfoPage, pVCpu->iHostCpuSet);
        cTicksToDeadline   = RT_MIN(cTicksToDeadline, u64CpuHz / 64);      /* 1/64th of a second */
        cTicksToDeadline   = RT_MAX(cTicksToDeadline, u64CpuHz / 2048);    /* 1/2048th of a second */
        cTicksToDeadline >>= pVM->hm.s.vmx.cPreemptTimerShift;

        uint32_t cPreemptionTickCount = (uint32_t)RT_MIN(cTicksToDeadline, UINT32_MAX - 16);
        int rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_PREEMPT_TIMER_VALUE, cPreemptionTickCount);
        AssertRC(rc);
    }
    else
        fOffsettedTsc = TMCpuTickCanUseRealTSC(pVM, pVCpu, &uTscOffset, &fParavirtTsc);

    if (fParavirtTsc)
    {
        /* Currently neither Hyper-V nor KVM need to update their paravirt. TSC
           information before every VM-entry, hence disable it for performance sake. */
#if 0
        int rc = GIMR0UpdateParavirtTsc(pVM, 0 /* u64Offset */);
        AssertRC(rc);
#endif
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscParavirt);
    }

    uint32_t uProcCtls = pVCpu->hm.s.vmx.u32ProcCtls;
    if (   fOffsettedTsc
        && RT_LIKELY(!pVCpu->hm.s.fDebugWantRdTscExit))
    {
        if (pVCpu->hm.s.vmx.u64TscOffset != uTscOffset)
        {
            int rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_TSC_OFFSET_FULL, uTscOffset);
            AssertRC(rc);
            pVCpu->hm.s.vmx.u64TscOffset = uTscOffset;
        }

        if (uProcCtls & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT)
        {
            uProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT;
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, uProcCtls);
            AssertRC(rc);
            pVCpu->hm.s.vmx.u32ProcCtls = uProcCtls;
        }
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscOffset);
    }
    else
    {
        /* We can't use TSC-offsetting (non-fixed TSC, warp drive active etc.), VM-exit on RDTSC(P). */
        if (!(uProcCtls & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT))
        {
            uProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT;
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, uProcCtls);
            AssertRC(rc);
            pVCpu->hm.s.vmx.u32ProcCtls = uProcCtls;
        }
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscIntercept);
    }
}


/**
 * Gets the IEM exception flags for the specified vector and IDT vectoring /
 * VM-exit interruption info type.
 *
 * @returns The IEM exception flags.
 * @param   uVector         The event vector.
 * @param   uVmxVectorType  The VMX event type.
 *
 * @remarks This function currently only constructs flags required for
 *          IEMEvaluateRecursiveXcpt and not the complete flags (e.g, error-code
 *          and CR2 aspects of an exception are not included).
 */
static uint32_t hmR0VmxGetIemXcptFlags(uint8_t uVector, uint32_t uVmxVectorType)
{
    uint32_t fIemXcptFlags;
    switch (uVmxVectorType)
    {
        case VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT:
        case VMX_IDT_VECTORING_INFO_TYPE_NMI:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_CPU_XCPT;
            break;

        case VMX_IDT_VECTORING_INFO_TYPE_EXT_INT:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_EXT_INT;
            break;

        case VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_SOFT_INT | IEM_XCPT_FLAGS_ICEBP_INSTR;
            break;

        case VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT:
        {
            fIemXcptFlags = IEM_XCPT_FLAGS_T_SOFT_INT;
            if (uVector == X86_XCPT_BP)
                fIemXcptFlags |= IEM_XCPT_FLAGS_BP_INSTR;
            else if (uVector == X86_XCPT_OF)
                fIemXcptFlags |= IEM_XCPT_FLAGS_OF_INSTR;
            else
            {
                fIemXcptFlags = 0;
                AssertMsgFailed(("Unexpected vector for software int. uVector=%#x", uVector));
            }
            break;
        }

        case VMX_IDT_VECTORING_INFO_TYPE_SW_INT:
            fIemXcptFlags = IEM_XCPT_FLAGS_T_SOFT_INT;
            break;

        default:
            fIemXcptFlags = 0;
            AssertMsgFailed(("Unexpected vector type! uVmxVectorType=%#x uVector=%#x", uVmxVectorType, uVector));
            break;
    }
    return fIemXcptFlags;
}


/**
 * Sets an event as a pending event to be injected into the guest.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   u32IntInfo          The VM-entry interruption-information field.
 * @param   cbInstr             The VM-entry instruction length in bytes (for software
 *                              interrupts, exceptions and privileged software
 *                              exceptions).
 * @param   u32ErrCode          The VM-entry exception error code.
 * @param   GCPtrFaultAddress   The fault-address (CR2) in case it's a
 *                              page-fault.
 *
 * @remarks Statistics counter assumes this is a guest event being injected or
 *          re-injected into the guest, i.e. 'StatInjectPendingReflect' is
 *          always incremented.
 */
DECLINLINE(void) hmR0VmxSetPendingEvent(PVMCPU pVCpu, uint32_t u32IntInfo, uint32_t cbInstr, uint32_t u32ErrCode,
                                        RTGCUINTPTR GCPtrFaultAddress)
{
    Assert(!pVCpu->hm.s.Event.fPending);
    pVCpu->hm.s.Event.fPending          = true;
    pVCpu->hm.s.Event.u64IntInfo        = u32IntInfo;
    pVCpu->hm.s.Event.u32ErrCode        = u32ErrCode;
    pVCpu->hm.s.Event.cbInstr           = cbInstr;
    pVCpu->hm.s.Event.GCPtrFaultAddress = GCPtrFaultAddress;
}


/**
 * Sets a double-fault (\#DF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptDF(PVMCPU pVCpu)
{
    uint32_t u32IntInfo  = X86_XCPT_DF | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntInfo          |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    u32IntInfo          |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
    hmR0VmxSetPendingEvent(pVCpu, u32IntInfo,  0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Handle a condition that occurred while delivering an event through the guest
 * IDT.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_SUCCESS if we should continue handling the VM-exit.
 * @retval  VINF_HM_DOUBLE_FAULT if a \#DF condition was detected and we ought
 *          to continue execution of the guest which will delivery the \#DF.
 * @retval  VINF_EM_RESET if we detected a triple-fault condition.
 * @retval  VERR_EM_GUEST_CPU_HANG if we detected a guest CPU hang.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
static VBOXSTRICTRC hmR0VmxCheckExitDueToEventDelivery(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    uint32_t const uExitVector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVmxTransient->uExitIntInfo);

    int rc2 = hmR0VmxReadIdtVectoringInfoVmcs(pVmxTransient);
    rc2    |= hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
    AssertRCReturn(rc2, rc2);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    if (VMX_IDT_VECTORING_INFO_VALID(pVmxTransient->uIdtVectoringInfo))
    {
        uint32_t const uIdtVectorType = VMX_IDT_VECTORING_INFO_TYPE(pVmxTransient->uIdtVectoringInfo);
        uint32_t const uIdtVector     = VMX_IDT_VECTORING_INFO_VECTOR(pVmxTransient->uIdtVectoringInfo);

        /*
         * If the event was a software interrupt (generated with INT n) or a software exception
         * (generated by INT3/INTO) or a privileged software exception (generated by INT1), we
         * can handle the VM-exit and continue guest execution which will re-execute the
         * instruction rather than re-injecting the exception, as that can cause premature
         * trips to ring-3 before injection and involve TRPM which currently has no way of
         * storing that these exceptions were caused by these instructions (ICEBP's #DB poses
         * the problem).
         */
        IEMXCPTRAISE     enmRaise;
        IEMXCPTRAISEINFO fRaiseInfo;
        if (   uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_SW_INT
            || uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT
            || uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT)
        {
            enmRaise   = IEMXCPTRAISE_REEXEC_INSTR;
            fRaiseInfo = IEMXCPTRAISEINFO_NONE;
        }
        else if (VMX_EXIT_INTERRUPTION_INFO_IS_VALID(pVmxTransient->uExitIntInfo))
        {
            uint32_t const uExitVectorType  = VMX_IDT_VECTORING_INFO_TYPE(pVmxTransient->uExitIntInfo);
            uint32_t const fIdtVectorFlags  = hmR0VmxGetIemXcptFlags(uIdtVector, uIdtVectorType);
            uint32_t const fExitVectorFlags = hmR0VmxGetIemXcptFlags(uExitVector, uExitVectorType);
            /** @todo Make AssertMsgReturn as just AssertMsg later. */
            AssertMsgReturn(uExitVectorType == VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT,
                            ("hmR0VmxCheckExitDueToEventDelivery: Unexpected VM-exit interruption info. %#x!\n",
                             uExitVectorType), VERR_VMX_IPE_5);

            enmRaise = IEMEvaluateRecursiveXcpt(pVCpu, fIdtVectorFlags, uIdtVector, fExitVectorFlags, uExitVector, &fRaiseInfo);

            /* Determine a vectoring #PF condition, see comment in hmR0VmxExitXcptPF(). */
            if (fRaiseInfo & (IEMXCPTRAISEINFO_EXT_INT_PF | IEMXCPTRAISEINFO_NMI_PF))
            {
                pVmxTransient->fVectoringPF = true;
                enmRaise = IEMXCPTRAISE_PREV_EVENT;
            }
        }
        else
        {
            /*
             * If an exception or hardware interrupt delivery caused an EPT violation/misconfig or APIC access
             * VM-exit, then the VM-exit interruption-information will not be valid and we end up here.
             * It is sufficient to reflect the original event to the guest after handling the VM-exit.
             */
            Assert(   uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT
                   || uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_NMI
                   || uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_EXT_INT);
            enmRaise   = IEMXCPTRAISE_PREV_EVENT;
            fRaiseInfo = IEMXCPTRAISEINFO_NONE;
        }

        /*
         * On CPUs that support Virtual NMIs, if this VM-exit (be it an exception or EPT violation/misconfig
         * etc.) occurred while delivering the NMI, we need to clear the block-by-NMI field in the guest
         * interruptibility-state before re-delivering the NMI after handling the VM-exit. Otherwise the
         * subsequent VM-entry would fail.
         *
         * See Intel spec. 30.7.1.2 "Resuming Guest Software after Handling an Exception". See @bugref{7445}.
         */
        if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS)
            && uIdtVectorType == VMX_IDT_VECTORING_INFO_TYPE_NMI
            && (   enmRaise   == IEMXCPTRAISE_PREV_EVENT
                || (fRaiseInfo & IEMXCPTRAISEINFO_NMI_PF))
            && (pVCpu->hm.s.vmx.u32PinCtls & VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI))
        {
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }

        switch (enmRaise)
        {
            case IEMXCPTRAISE_CURRENT_XCPT:
            {
                Log4Func(("IDT: Pending secondary Xcpt: uIdtVectoringInfo=%#RX64 uExitIntInfo=%#RX64\n",
                          pVmxTransient->uIdtVectoringInfo, pVmxTransient->uExitIntInfo));
                Assert(rcStrict == VINF_SUCCESS);
                break;
            }

            case IEMXCPTRAISE_PREV_EVENT:
            {
                uint32_t u32ErrCode;
                if (VMX_IDT_VECTORING_INFO_ERROR_CODE_IS_VALID(pVmxTransient->uIdtVectoringInfo))
                {
                    rc2 = hmR0VmxReadIdtVectoringErrorCodeVmcs(pVmxTransient);
                    AssertRCReturn(rc2, rc2);
                    u32ErrCode = pVmxTransient->uIdtVectoringErrorCode;
                }
                else
                    u32ErrCode = 0;

                /* If uExitVector is #PF, CR2 value will be updated from the VMCS if it's a guest #PF, see hmR0VmxExitXcptPF(). */
                STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingReflect);
                hmR0VmxSetPendingEvent(pVCpu, VMX_ENTRY_INT_INFO_FROM_EXIT_IDT_INFO(pVmxTransient->uIdtVectoringInfo),
                                       0 /* cbInstr */, u32ErrCode, pVCpu->cpum.GstCtx.cr2);

                Log4Func(("IDT: Pending vectoring event %#RX64 Err=%#RX32\n", pVCpu->hm.s.Event.u64IntInfo,
                          pVCpu->hm.s.Event.u32ErrCode));
                Assert(rcStrict == VINF_SUCCESS);
                break;
            }

            case IEMXCPTRAISE_REEXEC_INSTR:
                Assert(rcStrict == VINF_SUCCESS);
                break;

            case IEMXCPTRAISE_DOUBLE_FAULT:
            {
                /*
                 * Determing a vectoring double #PF condition. Used later, when PGM evaluates the
                 * second #PF as a guest #PF (and not a shadow #PF) and needs to be converted into a #DF.
                 */
                if (fRaiseInfo & IEMXCPTRAISEINFO_PF_PF)
                {
                    pVmxTransient->fVectoringDoublePF = true;
                    Log4Func(("IDT: Vectoring double #PF %#RX64 cr2=%#RX64\n", pVCpu->hm.s.Event.u64IntInfo,
                          pVCpu->cpum.GstCtx.cr2));
                    rcStrict = VINF_SUCCESS;
                }
                else
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingReflect);
                    hmR0VmxSetPendingXcptDF(pVCpu);
                    Log4Func(("IDT: Pending vectoring #DF %#RX64 uIdtVector=%#x uExitVector=%#x\n", pVCpu->hm.s.Event.u64IntInfo,
                              uIdtVector, uExitVector));
                    rcStrict = VINF_HM_DOUBLE_FAULT;
                }
                break;
            }

            case IEMXCPTRAISE_TRIPLE_FAULT:
            {
                Log4Func(("IDT: Pending vectoring triple-fault uIdt=%#x uExit=%#x\n", uIdtVector, uExitVector));
                rcStrict = VINF_EM_RESET;
                break;
            }

            case IEMXCPTRAISE_CPU_HANG:
            {
                Log4Func(("IDT: Bad guest! Entering CPU hang. fRaiseInfo=%#x\n", fRaiseInfo));
                rcStrict = VERR_EM_GUEST_CPU_HANG;
                break;
            }

            default:
            {
                AssertMsgFailed(("IDT: vcpu[%RU32] Unexpected/invalid value! enmRaise=%#x\n", pVCpu->idCpu, enmRaise));
                rcStrict = VERR_VMX_IPE_2;
                break;
            }
        }
    }
    else if (   VMX_EXIT_INTERRUPTION_INFO_IS_VALID(pVmxTransient->uExitIntInfo)
             && VMX_EXIT_INTERRUPTION_INFO_NMI_UNBLOCK_IRET(pVmxTransient->uExitIntInfo)
             && uExitVector != X86_XCPT_DF
             && (pVCpu->hm.s.vmx.u32PinCtls & VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI))
    {
        /*
         * Execution of IRET caused this fault when NMI blocking was in effect (i.e we're in the guest NMI handler).
         * We need to set the block-by-NMI field so that NMIs remain blocked until the IRET execution is restarted.
         * See Intel spec. 30.7.1.2 "Resuming guest software after handling an exception".
         */
        if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
        {
            Log4Func(("Setting VMCPU_FF_BLOCK_NMIS. fValid=%RTbool uExitReason=%u\n",
                      VMX_EXIT_INTERRUPTION_INFO_IS_VALID(pVmxTransient->uExitIntInfo), pVmxTransient->uExitReason));
            VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }
    }

    Assert(   rcStrict == VINF_SUCCESS  || rcStrict == VINF_HM_DOUBLE_FAULT
           || rcStrict == VINF_EM_RESET || rcStrict == VERR_EM_GUEST_CPU_HANG);
    return rcStrict;
}


/**
 * Imports a guest segment register from the current VMCS into
 * the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idxSel      Index of the selector in the VMCS.
 * @param   idxLimit    Index of the segment limit in the VMCS.
 * @param   idxBase     Index of the segment base in the VMCS.
 * @param   idxAccess   Index of the access rights of the segment in the VMCS.
 * @param   pSelReg     Pointer to the segment selector.
 *
 * @remarks Called with interrupts and/or preemption disabled, try not to assert and
 *          do not log!
 *
 * @remarks Never call this function directly!!! Use the
 *          HMVMX_IMPORT_SREG() macro as that takes care
 *          of whether to read from the VMCS cache or not.
 */
static int hmR0VmxImportGuestSegmentReg(PVMCPU pVCpu, uint32_t idxSel, uint32_t idxLimit, uint32_t idxBase, uint32_t idxAccess,
                                        PCPUMSELREG pSelReg)
{
    NOREF(pVCpu);

    uint32_t u32Sel;
    uint32_t u32Limit;
    uint32_t u32Attr;
    uint64_t u64Base;
    int rc = VMXReadVmcs32(idxSel, &u32Sel);
    rc    |= VMXReadVmcs32(idxLimit, &u32Limit);
    rc    |= VMXReadVmcs32(idxAccess, &u32Attr);
    rc    |= VMXReadVmcsGstNByIdxVal(idxBase, &u64Base);
    AssertRCReturn(rc, rc);

    pSelReg->Sel      = (uint16_t)u32Sel;
    pSelReg->ValidSel = (uint16_t)u32Sel;
    pSelReg->fFlags   = CPUMSELREG_FLAGS_VALID;
    pSelReg->u32Limit = u32Limit;
    pSelReg->u64Base  = u64Base;
    pSelReg->Attr.u   = u32Attr;

    /*
     * If VT-x marks the segment as unusable, most other bits remain undefined:
     *   - For CS the L, D and G bits have meaning.
     *   - For SS the DPL has meaning (it -is- the CPL for Intel and VBox).
     *   - For the remaining data segments no bits are defined.
     *
     * The present bit and the unusable bit has been observed to be set at the
     * same time (the selector was supposed to be invalid as we started executing
     * a V8086 interrupt in ring-0).
     *
     * What should be important for the rest of the VBox code, is that the P bit is
     * cleared.  Some of the other VBox code recognizes the unusable bit, but
     * AMD-V certainly don't, and REM doesn't really either.  So, to be on the
     * safe side here, we'll strip off P and other bits we don't care about.  If
     * any code breaks because Attr.u != 0 when Sel < 4, it should be fixed.
     *
     * See Intel spec. 27.3.2 "Saving Segment Registers and Descriptor-Table Registers".
     */
    if (pSelReg->Attr.u & X86DESCATTR_UNUSABLE)
    {
        Assert(idxSel != VMX_VMCS16_GUEST_TR_SEL);          /* TR is the only selector that can never be unusable. */

        /* Masking off: X86DESCATTR_P, X86DESCATTR_LIMIT_HIGH, and X86DESCATTR_AVL. The latter two are really irrelevant. */
        pSelReg->Attr.u &= X86DESCATTR_UNUSABLE | X86DESCATTR_L    | X86DESCATTR_D  | X86DESCATTR_G
                         | X86DESCATTR_DPL      | X86DESCATTR_TYPE | X86DESCATTR_DT;
#ifdef VBOX_STRICT
        VMMRZCallRing3Disable(pVCpu);
        Log4Func(("Unusable idxSel=%#x attr=%#x -> %#x\n", idxSel, u32Sel, pSelReg->Attr.u));
# ifdef DEBUG_bird
        AssertMsg((u32Attr & ~X86DESCATTR_P) == pSelReg->Attr.u,
                  ("%#x: %#x != %#x (sel=%#x base=%#llx limit=%#x)\n",
                   idxSel, u32Sel, pSelReg->Attr.u, pSelReg->Sel, pSelReg->u64Base, pSelReg->u32Limit));
# endif
        VMMRZCallRing3Enable(pVCpu);
#endif
    }
    return VINF_SUCCESS;
}


/**
 * Imports the guest RIP from the VMCS back into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @remarks Called with interrupts and/or preemption disabled, should not assert!
 * @remarks Do -not- call this function directly, use hmR0VmxImportGuestState()
 *          instead!!!
 */
DECLINLINE(int) hmR0VmxImportGuestRip(PVMCPU pVCpu)
{
    uint64_t u64Val;
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (pCtx->fExtrn & CPUMCTX_EXTRN_RIP)
    {
        int rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RIP, &u64Val);
        if (RT_SUCCESS(rc))
        {
            pCtx->rip = u64Val;
            EMR0HistoryUpdatePC(pVCpu, pCtx->rip, false);
            pCtx->fExtrn &= ~CPUMCTX_EXTRN_RIP;
        }
        return rc;
    }
    return VINF_SUCCESS;
}


/**
 * Imports the guest RFLAGS from the VMCS back into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @remarks Called with interrupts and/or preemption disabled, should not assert!
 * @remarks Do -not- call this function directly, use hmR0VmxImportGuestState()
 *          instead!!!
 */
DECLINLINE(int) hmR0VmxImportGuestRFlags(PVMCPU pVCpu)
{
    uint32_t u32Val;
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (pCtx->fExtrn & CPUMCTX_EXTRN_RFLAGS)
    {
        int rc = VMXReadVmcs32(VMX_VMCS_GUEST_RFLAGS, &u32Val);
        if (RT_SUCCESS(rc))
        {
            pCtx->eflags.u32 = u32Val;

            /* Restore eflags for real-on-v86-mode hack. */
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
            {
                pCtx->eflags.Bits.u1VM   = 0;
                pCtx->eflags.Bits.u2IOPL = pVCpu->hm.s.vmx.RealMode.Eflags.Bits.u2IOPL;
            }
        }
        pCtx->fExtrn &= ~CPUMCTX_EXTRN_RFLAGS;
        return rc;
    }
    return VINF_SUCCESS;
}


/**
 * Imports the guest interruptibility-state from the VMCS back into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @remarks Called with interrupts and/or preemption disabled, try not to assert and
 *          do not log!
 * @remarks Do -not- call this function directly, use hmR0VmxImportGuestState()
 *          instead!!!
 */
DECLINLINE(int) hmR0VmxImportGuestIntrState(PVMCPU pVCpu)
{
    uint32_t u32Val;
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    int rc = VMXReadVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &u32Val);
    if (RT_SUCCESS(rc))
    {
        /*
         * We additionally have a requirement to import RIP, RFLAGS depending on whether we
         * might need them in hmR0VmxEvaluatePendingEvent().
         */
        if (!u32Val)
        {
            if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
            {
                rc =  hmR0VmxImportGuestRip(pVCpu);
                rc |= hmR0VmxImportGuestRFlags(pVCpu);
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
            }

            if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }
        else
        {
            rc =  hmR0VmxImportGuestRip(pVCpu);
            rc |= hmR0VmxImportGuestRFlags(pVCpu);

            if (u32Val & (  VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS
                          | VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI))
            {
                EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
            }
            else if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

            if (u32Val & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI)
            {
                if (!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_BLOCK_NMIS);
            }
            else if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS))
                VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_BLOCK_NMIS);
        }
    }
    return rc;
}


/**
 * Worker for VMXR0ImportStateOnDemand.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   fWhat   What to import, CPUMCTX_EXTRN_XXX.
 */
static int hmR0VmxImportGuestState(PVMCPU pVCpu, uint64_t fWhat)
{
#define VMXLOCAL_BREAK_RC(a_rc) \
    if (RT_FAILURE(a_rc))       \
        break

    int      rc   = VINF_SUCCESS;
    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    uint64_t u64Val;
    uint32_t u32Val;

    Log4Func(("fExtrn=%#RX64 fWhat=%#RX64\n", pCtx->fExtrn, fWhat));
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatImportGuestState, x);

    /*
     * We disable interrupts to make the updating of the state and in particular
     * the fExtrn modification atomic wrt to preemption hooks.
     */
    RTCCUINTREG const fEFlags = ASMIntDisableFlags();

    fWhat &= pCtx->fExtrn;
    if (fWhat)
    {
        do
        {
            if (fWhat & CPUMCTX_EXTRN_RIP)
            {
                rc = hmR0VmxImportGuestRip(pVCpu);
                VMXLOCAL_BREAK_RC(rc);
            }

            if (fWhat & CPUMCTX_EXTRN_RFLAGS)
            {
                rc = hmR0VmxImportGuestRFlags(pVCpu);
                VMXLOCAL_BREAK_RC(rc);
            }

            if (fWhat & CPUMCTX_EXTRN_HM_VMX_INT_STATE)
            {
                rc = hmR0VmxImportGuestIntrState(pVCpu);
                VMXLOCAL_BREAK_RC(rc);
            }

            if (fWhat & CPUMCTX_EXTRN_RSP)
            {
                rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RSP, &u64Val);
                VMXLOCAL_BREAK_RC(rc);
                pCtx->rsp = u64Val;
            }

            if (fWhat & CPUMCTX_EXTRN_SREG_MASK)
            {
                if (fWhat & CPUMCTX_EXTRN_CS)
                {
                    rc  = HMVMX_IMPORT_SREG(CS, &pCtx->cs);
                    rc |= hmR0VmxImportGuestRip(pVCpu);
                    VMXLOCAL_BREAK_RC(rc);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                        pCtx->cs.Attr.u = pVCpu->hm.s.vmx.RealMode.AttrCS.u;
                    EMR0HistoryUpdatePC(pVCpu, pCtx->cs.u64Base + pCtx->rip, true);
                }
                if (fWhat & CPUMCTX_EXTRN_SS)
                {
                    rc = HMVMX_IMPORT_SREG(SS, &pCtx->ss);
                    VMXLOCAL_BREAK_RC(rc);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                        pCtx->ss.Attr.u = pVCpu->hm.s.vmx.RealMode.AttrSS.u;
                }
                if (fWhat & CPUMCTX_EXTRN_DS)
                {
                    rc = HMVMX_IMPORT_SREG(DS, &pCtx->ds);
                    VMXLOCAL_BREAK_RC(rc);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                        pCtx->ds.Attr.u = pVCpu->hm.s.vmx.RealMode.AttrDS.u;
                }
                if (fWhat & CPUMCTX_EXTRN_ES)
                {
                    rc = HMVMX_IMPORT_SREG(ES, &pCtx->es);
                    VMXLOCAL_BREAK_RC(rc);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                        pCtx->es.Attr.u = pVCpu->hm.s.vmx.RealMode.AttrES.u;
                }
               if (fWhat & CPUMCTX_EXTRN_FS)
               {
                    rc = HMVMX_IMPORT_SREG(FS, &pCtx->fs);
                    VMXLOCAL_BREAK_RC(rc);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                        pCtx->fs.Attr.u = pVCpu->hm.s.vmx.RealMode.AttrFS.u;
               }
                if (fWhat & CPUMCTX_EXTRN_GS)
                {
                    rc = HMVMX_IMPORT_SREG(GS, &pCtx->gs);
                    VMXLOCAL_BREAK_RC(rc);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                        pCtx->gs.Attr.u = pVCpu->hm.s.vmx.RealMode.AttrGS.u;
                }
            }

            if (fWhat & CPUMCTX_EXTRN_TABLE_MASK)
            {
                if (fWhat & CPUMCTX_EXTRN_LDTR)
                {
                    rc = HMVMX_IMPORT_SREG(LDTR, &pCtx->ldtr);
                    VMXLOCAL_BREAK_RC(rc);
                }

                if (fWhat & CPUMCTX_EXTRN_GDTR)
                {
                    rc  = VMXReadVmcsGstN(VMX_VMCS_GUEST_GDTR_BASE,  &u64Val);
                    rc |= VMXReadVmcs32(VMX_VMCS32_GUEST_GDTR_LIMIT, &u32Val);
                    VMXLOCAL_BREAK_RC(rc);
                    pCtx->gdtr.pGdt  = u64Val;
                    pCtx->gdtr.cbGdt = u32Val;
                }

                /* Guest IDTR. */
                if (fWhat & CPUMCTX_EXTRN_IDTR)
                {
                    rc  = VMXReadVmcsGstN(VMX_VMCS_GUEST_IDTR_BASE,  &u64Val);
                    rc |= VMXReadVmcs32(VMX_VMCS32_GUEST_IDTR_LIMIT, &u32Val);
                    VMXLOCAL_BREAK_RC(rc);
                    pCtx->idtr.pIdt  = u64Val;
                    pCtx->idtr.cbIdt = u32Val;
                }

                /* Guest TR. */
                if (fWhat & CPUMCTX_EXTRN_TR)
                {
                    /* Real-mode emulation using virtual-8086 mode has the fake TSS (pRealModeTSS) in TR, don't save that one. */
                    if (!pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                    {
                        rc = HMVMX_IMPORT_SREG(TR, &pCtx->tr);
                        VMXLOCAL_BREAK_RC(rc);
                    }
                }
            }

            if (fWhat & CPUMCTX_EXTRN_SYSENTER_MSRS)
            {
                rc  = VMXReadVmcsGstN(VMX_VMCS_GUEST_SYSENTER_EIP, &pCtx->SysEnter.eip);
                rc |= VMXReadVmcsGstN(VMX_VMCS_GUEST_SYSENTER_ESP, &pCtx->SysEnter.esp);
                rc |= VMXReadVmcs32(VMX_VMCS32_GUEST_SYSENTER_CS,  &u32Val);
                pCtx->SysEnter.cs = u32Val;
                VMXLOCAL_BREAK_RC(rc);
            }

#if HC_ARCH_BITS == 64
            if (fWhat & CPUMCTX_EXTRN_KERNEL_GS_BASE)
            {
                if (   pVM->hm.s.fAllow64BitGuests
                    && (pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST))
                    pCtx->msrKERNELGSBASE = ASMRdMsr(MSR_K8_KERNEL_GS_BASE);
            }

            if (fWhat & CPUMCTX_EXTRN_SYSCALL_MSRS)
            {
                if (   pVM->hm.s.fAllow64BitGuests
                    && (pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST))
                {
                    pCtx->msrLSTAR  = ASMRdMsr(MSR_K8_LSTAR);
                    pCtx->msrSTAR   = ASMRdMsr(MSR_K6_STAR);
                    pCtx->msrSFMASK = ASMRdMsr(MSR_K8_SF_MASK);
                }
            }
#endif

            if (   (fWhat & (CPUMCTX_EXTRN_TSC_AUX | CPUMCTX_EXTRN_OTHER_MSRS))
#if HC_ARCH_BITS == 32
                || (fWhat & (CPUMCTX_EXTRN_KERNEL_GS_BASE | CPUMCTX_EXTRN_SYSCALL_MSRS))
#endif
                )
            {
                PCVMXAUTOMSR   pMsr  = (PVMXAUTOMSR)pVCpu->hm.s.vmx.pvGuestMsr;
                uint32_t const cMsrs = pVCpu->hm.s.vmx.cMsrs;
                for (uint32_t i = 0; i < cMsrs; i++, pMsr++)
                {
                    switch (pMsr->u32Msr)
                    {
#if HC_ARCH_BITS == 32
                        case MSR_K8_LSTAR:          pCtx->msrLSTAR        = pMsr->u64Value;         break;
                        case MSR_K6_STAR:           pCtx->msrSTAR         = pMsr->u64Value;         break;
                        case MSR_K8_SF_MASK:        pCtx->msrSFMASK       = pMsr->u64Value;         break;
                        case MSR_K8_KERNEL_GS_BASE: pCtx->msrKERNELGSBASE = pMsr->u64Value;         break;
#endif
                        case MSR_IA32_SPEC_CTRL:    CPUMSetGuestSpecCtrl(pVCpu, pMsr->u64Value);    break;
                        case MSR_K8_TSC_AUX:        CPUMSetGuestTscAux(pVCpu, pMsr->u64Value);      break;
                        case MSR_K6_EFER: /* EFER can't be changed without causing a VM-exit */     break;
                        default:
                        {
                            pVCpu->hm.s.u32HMError = pMsr->u32Msr;
                            ASMSetFlags(fEFlags);
                            AssertMsgFailed(("Unexpected MSR in auto-load/store area. uMsr=%#RX32 cMsrs=%u\n", pMsr->u32Msr,
                                             cMsrs));
                            return VERR_HM_UNEXPECTED_LD_ST_MSR;
                        }
                    }
                }
            }

            if (fWhat & CPUMCTX_EXTRN_DR7)
            {
                if (!pVCpu->hm.s.fUsingHyperDR7)
                {
                    /* Upper 32-bits are always zero. See Intel spec. 2.7.3 "Loading and Storing Debug Registers". */
                    rc = VMXReadVmcs32(VMX_VMCS_GUEST_DR7, &u32Val);
                    VMXLOCAL_BREAK_RC(rc);
                    pCtx->dr[7] = u32Val;
                }
            }

            if (fWhat & CPUMCTX_EXTRN_CR_MASK)
            {
                uint32_t u32Shadow;
                if (fWhat & CPUMCTX_EXTRN_CR0)
                {
                    rc  = VMXReadVmcs32(VMX_VMCS_GUEST_CR0,            &u32Val);
                    rc |= VMXReadVmcs32(VMX_VMCS_CTRL_CR0_READ_SHADOW, &u32Shadow);
                    VMXLOCAL_BREAK_RC(rc);
                    u32Val = (u32Val & ~pVCpu->hm.s.vmx.u32Cr0Mask)
                           | (u32Shadow & pVCpu->hm.s.vmx.u32Cr0Mask);
                    VMMRZCallRing3Disable(pVCpu);   /* Calls into PGM which has Log statements. */
                    CPUMSetGuestCR0(pVCpu, u32Val);
                    VMMRZCallRing3Enable(pVCpu);
                }

                if (fWhat & CPUMCTX_EXTRN_CR4)
                {
                    rc  = VMXReadVmcs32(VMX_VMCS_GUEST_CR4,            &u32Val);
                    rc |= VMXReadVmcs32(VMX_VMCS_CTRL_CR4_READ_SHADOW, &u32Shadow);
                    VMXLOCAL_BREAK_RC(rc);
                    u32Val = (u32Val & ~pVCpu->hm.s.vmx.u32Cr4Mask)
                           | (u32Shadow & pVCpu->hm.s.vmx.u32Cr4Mask);
                    CPUMSetGuestCR4(pVCpu, u32Val);
                }

                if (fWhat & CPUMCTX_EXTRN_CR3)
                {
                    /* CR0.PG bit changes are always intercepted, so it's up to date. */
                    if (   pVM->hm.s.vmx.fUnrestrictedGuest
                        || (   pVM->hm.s.fNestedPaging
                            && CPUMIsGuestPagingEnabledEx(pCtx)))
                    {
                        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_CR3, &u64Val);
                        if (pCtx->cr3 != u64Val)
                        {
                            CPUMSetGuestCR3(pVCpu, u64Val);
                            VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3);
                        }

                        /* If the guest is in PAE mode, sync back the PDPE's into the guest state.
                           Note: CR4.PAE, CR0.PG, EFER bit changes are always intercepted, so they're up to date. */
                        if (CPUMIsGuestInPAEModeEx(pCtx))
                        {
                            rc  = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE0_FULL, &pVCpu->hm.s.aPdpes[0].u);
                            rc |= VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE1_FULL, &pVCpu->hm.s.aPdpes[1].u);
                            rc |= VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE2_FULL, &pVCpu->hm.s.aPdpes[2].u);
                            rc |= VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE3_FULL, &pVCpu->hm.s.aPdpes[3].u);
                            VMXLOCAL_BREAK_RC(rc);
                            VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES);
                        }
                    }
                }
            }
        } while (0);

        if (RT_SUCCESS(rc))
        {
            /* Update fExtrn. */
            pCtx->fExtrn &= ~fWhat;

            /* If everything has been imported, clear the HM keeper bit. */
            if (!(pCtx->fExtrn & HMVMX_CPUMCTX_EXTRN_ALL))
            {
                pCtx->fExtrn &= ~CPUMCTX_EXTRN_KEEPER_HM;
                Assert(!pCtx->fExtrn);
            }
        }
    }
    else
        AssertMsg(!pCtx->fExtrn || (pCtx->fExtrn & HMVMX_CPUMCTX_EXTRN_ALL), ("%#RX64\n", pCtx->fExtrn));

    ASMSetFlags(fEFlags);

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatImportGuestState, x);

    /*
     * Honor any pending CR3 updates.
     *
     * Consider this scenario: VM-exit -> VMMRZCallRing3Enable() -> do stuff that causes a longjmp -> hmR0VmxCallRing3Callback()
     * -> VMMRZCallRing3Disable() -> hmR0VmxImportGuestState() -> Sets VMCPU_FF_HM_UPDATE_CR3 pending -> return from the longjmp
     * -> continue with VM-exit handling -> hmR0VmxImportGuestState() and here we are.
     *
     * The reason for such complicated handling is because VM-exits that call into PGM expect CR3 to be up-to-date and thus
     * if any CR3-saves -before- the VM-exit (longjmp) postponed the CR3 update via the force-flag, any VM-exit handler that
     * calls into PGM when it re-saves CR3 will end up here and we call PGMUpdateCR3(). This is why the code below should
     * -NOT- check if CPUMCTX_EXTRN_CR3 is set!
     *
     * The longjmp exit path can't check these CR3 force-flags and call code that takes a lock again. We cover for it here.
     */
    if (VMMRZCallRing3IsEnabled(pVCpu))
    {
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
        {
            Assert(!(ASMAtomicUoReadU64(&pCtx->fExtrn) & CPUMCTX_EXTRN_CR3));
            PGMUpdateCR3(pVCpu, CPUMGetGuestCR3(pVCpu));
        }

        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES))
            PGMGstUpdatePaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);

        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));
    }

    return VINF_SUCCESS;
#undef VMXLOCAL_BREAK_RC
}


/**
 * Saves the guest state from the VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   fWhat   What to import, CPUMCTX_EXTRN_XXX.
 */
VMMR0DECL(int) VMXR0ImportStateOnDemand(PVMCPU pVCpu, uint64_t fWhat)
{
    return hmR0VmxImportGuestState(pVCpu, fWhat);
}


/**
 * Check per-VM and per-VCPU force flag actions that require us to go back to
 * ring-3 for one reason or another.
 *
 * @returns Strict VBox status code (i.e. informational status codes too)
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
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fStepping       Running in hmR0VmxRunGuestCodeStep().
 */
static VBOXSTRICTRC hmR0VmxCheckForceFlags(PVMCPU pVCpu, bool fStepping)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));

    /*
     * Anything pending?  Should be more likely than not if we're doing a good job.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (  !fStepping
        ?    !VM_FF_IS_PENDING(pVM, VM_FF_HP_R0_PRE_HM_MASK)
          && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HP_R0_PRE_HM_MASK)
        :    !VM_FF_IS_PENDING(pVM, VM_FF_HP_R0_PRE_HM_STEP_MASK)
          && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
        return VINF_SUCCESS;

    /* Pending PGM C3 sync. */
    if (VMCPU_FF_IS_PENDING(pVCpu,VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
    {
        PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        Assert(!(ASMAtomicUoReadU64(&pCtx->fExtrn) & (CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4)));
        VBOXSTRICTRC rcStrict2 = PGMSyncCR3(pVCpu, pCtx->cr0, pCtx->cr3, pCtx->cr4,
                                            VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
        if (rcStrict2 != VINF_SUCCESS)
        {
            AssertRC(VBOXSTRICTRC_VAL(rcStrict2));
            Log4Func(("PGMSyncCR3 forcing us back to ring-3. rc2=%d\n", VBOXSTRICTRC_VAL(rcStrict2)));
            return rcStrict2;
        }
    }

    /* Pending HM-to-R3 operations (critsects, timers, EMT rendezvous etc.) */
    if (   VM_FF_IS_PENDING(pVM, VM_FF_HM_TO_R3_MASK)
        || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHmToR3FF);
        int rc2 = RT_UNLIKELY(VM_FF_IS_PENDING(pVM, VM_FF_PGM_NO_MEMORY)) ? VINF_EM_NO_MEMORY : VINF_EM_RAW_TO_R3;
        Log4Func(("HM_TO_R3 forcing us back to ring-3. rc=%d\n", rc2));
        return rc2;
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

    return VINF_SUCCESS;
}


/**
 * Converts any TRPM trap into a pending HM event. This is typically used when
 * entering from ring-3 (not longjmp returns).
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void hmR0VmxTrpmTrapToPendingEvent(PVMCPU pVCpu)
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

    /* Refer Intel spec. 24.8.3 "VM-entry Controls for Event Injection" for the format of u32IntInfo. */
    uint32_t u32IntInfo = uVector | VMX_EXIT_INTERRUPTION_INFO_VALID;
    if (enmTrpmEvent == TRPM_TRAP)
    {
        switch (uVector)
        {
            case X86_XCPT_NMI:
                u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
                break;

            case X86_XCPT_BP:
            case X86_XCPT_OF:
                u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
                break;

            case X86_XCPT_PF:
            case X86_XCPT_DF:
            case X86_XCPT_TS:
            case X86_XCPT_NP:
            case X86_XCPT_SS:
            case X86_XCPT_GP:
            case X86_XCPT_AC:
                u32IntInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
                RT_FALL_THRU();
            default:
                u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
                break;
        }
    }
    else if (enmTrpmEvent == TRPM_HARDWARE_INT)
        u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    else if (enmTrpmEvent == TRPM_SOFTWARE_INT)
        u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    else
        AssertMsgFailed(("Invalid TRPM event type %d\n", enmTrpmEvent));

    rc = TRPMResetTrap(pVCpu);
    AssertRC(rc);
    Log4(("TRPM->HM event: u32IntInfo=%#RX32 enmTrpmEvent=%d cbInstr=%u uErrCode=%#RX32 GCPtrFaultAddress=%#RGv\n",
          u32IntInfo, enmTrpmEvent, cbInstr, uErrCode, GCPtrFaultAddress));

    hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, cbInstr, uErrCode, GCPtrFaultAddress);
}


/**
 * Converts the pending HM event into a TRPM trap.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void hmR0VmxPendingEventToTrpmTrap(PVMCPU pVCpu)
{
    Assert(pVCpu->hm.s.Event.fPending);

    uint32_t uVectorType     = VMX_IDT_VECTORING_INFO_TYPE(pVCpu->hm.s.Event.u64IntInfo);
    uint32_t uVector         = VMX_IDT_VECTORING_INFO_VECTOR(pVCpu->hm.s.Event.u64IntInfo);
    bool     fErrorCodeValid = VMX_IDT_VECTORING_INFO_ERROR_CODE_IS_VALID(pVCpu->hm.s.Event.u64IntInfo);
    uint32_t uErrorCode      = pVCpu->hm.s.Event.u32ErrCode;

    /* If a trap was already pending, we did something wrong! */
    Assert(TRPMQueryTrap(pVCpu, NULL /* pu8TrapNo */, NULL /* pEnmType */) == VERR_TRPM_NO_ACTIVE_TRAP);

    TRPMEVENT enmTrapType;
    switch (uVectorType)
    {
        case VMX_IDT_VECTORING_INFO_TYPE_EXT_INT:
           enmTrapType = TRPM_HARDWARE_INT;
           break;

        case VMX_IDT_VECTORING_INFO_TYPE_SW_INT:
            enmTrapType = TRPM_SOFTWARE_INT;
            break;

        case VMX_IDT_VECTORING_INFO_TYPE_NMI:
        case VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT:
        case VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT:      /* #BP and #OF */
        case VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT:
            enmTrapType = TRPM_TRAP;
            break;

        default:
            AssertMsgFailed(("Invalid trap type %#x\n", uVectorType));
            enmTrapType = TRPM_32BIT_HACK;
            break;
    }

    Log4(("HM event->TRPM: uVector=%#x enmTrapType=%d\n", uVector, enmTrapType));

    int rc = TRPMAssertTrap(pVCpu, uVector, enmTrapType);
    AssertRC(rc);

    if (fErrorCodeValid)
        TRPMSetErrorCode(pVCpu, uErrorCode);

    if (   uVectorType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT
        && uVector == X86_XCPT_PF)
    {
        TRPMSetFaultAddress(pVCpu, pVCpu->hm.s.Event.GCPtrFaultAddress);
    }
    else if (   uVectorType == VMX_IDT_VECTORING_INFO_TYPE_SW_INT
             || uVectorType == VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT
             || uVectorType == VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT)
    {
        AssertMsg(   uVectorType == VMX_IDT_VECTORING_INFO_TYPE_SW_INT
                  || (uVector == X86_XCPT_BP || uVector == X86_XCPT_OF),
                  ("Invalid vector: uVector=%#x uVectorType=%#x\n", uVector, uVectorType));
        TRPMSetInstrLength(pVCpu, pVCpu->hm.s.Event.cbInstr);
    }

    /* Clear any pending events from the VMCS. */
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, 0);        AssertRC(rc);
    rc = VMXWriteVmcs32(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);        AssertRC(rc);

    /* We're now done converting the pending event. */
    pVCpu->hm.s.Event.fPending = false;
}


/**
 * Does the necessary state syncing before returning to ring-3 for any reason
 * (longjmp, preemption, voluntary exits to ring-3) from VT-x.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fImportState    Whether to import the guest state from the VMCS back
 *                          to the guest-CPU context.
 *
 * @remarks No-long-jmp zone!!!
 */
static int hmR0VmxLeave(PVMCPU pVCpu, bool fImportState)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    RTCPUID idCpu = RTMpCpuId();
    Log4Func(("HostCpuId=%u\n", idCpu));

    /*
     * !!! IMPORTANT !!!
     * If you modify code here, check whether hmR0VmxCallRing3Callback() needs to be updated too.
     */

    /* Save the guest state if necessary. */
    if (fImportState)
    {
        int rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        AssertRCReturn(rc, rc);
    }

    /* Restore host FPU state if necessary. We will resync on next R0 reentry. */
    CPUMR0FpuStateMaybeSaveGuestAndRestoreHost(pVCpu);
    Assert(!CPUMIsGuestFPUStateActive(pVCpu));

    /* Restore host debug registers if necessary. We will resync on next R0 reentry. */
#ifdef VBOX_STRICT
    if (CPUMIsHyperDebugStateActive(pVCpu))
        Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT);
#endif
    CPUMR0DebugStateMaybeSaveGuestAndRestoreHost(pVCpu, true /* save DR6 */);
    Assert(!CPUMIsGuestDebugStateActive(pVCpu) && !CPUMIsGuestDebugStateActivePending(pVCpu));
    Assert(!CPUMIsHyperDebugStateActive(pVCpu) && !CPUMIsHyperDebugStateActivePending(pVCpu));

#if HC_ARCH_BITS == 64
    /* Restore host-state bits that VT-x only restores partially. */
    if (   (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_REQUIRED)
        && (pVCpu->hm.s.vmx.fRestoreHostFlags & ~VMX_RESTORE_HOST_REQUIRED))
    {
        Log4Func(("Restoring Host State: fRestoreHostFlags=%#RX32 HostCpuId=%u\n", pVCpu->hm.s.vmx.fRestoreHostFlags, idCpu));
        VMXRestoreHostState(pVCpu->hm.s.vmx.fRestoreHostFlags, &pVCpu->hm.s.vmx.RestoreHost);
    }
    pVCpu->hm.s.vmx.fRestoreHostFlags = 0;
#endif

    /* Restore the lazy host MSRs as we're leaving VT-x context. */
    if (pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST)
    {
        /* We shouldn't restore the host MSRs without saving the guest MSRs first. */
        if (!fImportState)
        {
            int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_KERNEL_GS_BASE | CPUMCTX_EXTRN_SYSCALL_MSRS);
            AssertRCReturn(rc, rc);
        }
        hmR0VmxLazyRestoreHostMsrs(pVCpu);
        Assert(!pVCpu->hm.s.vmx.fLazyMsrs);
    }
    else
        pVCpu->hm.s.vmx.fLazyMsrs = 0;

    /* Update auto-load/store host MSRs values when we re-enter VT-x (as we could be on a different CPU). */
    pVCpu->hm.s.vmx.fUpdatedHostMsrs = false;

    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatEntry);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatImportGuestState);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExportGuestState);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatPreExit);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExitHandling);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExitIO);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExitMovCRx);
    STAM_PROFILE_ADV_SET_STOPPED(&pVCpu->hm.s.StatExitXcptNmi);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchLongJmpToR3);

    VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_HM, VMCPUSTATE_STARTED_EXEC);

    /** @todo This partially defeats the purpose of having preemption hooks.
     *  The problem is, deregistering the hooks should be moved to a place that
     *  lasts until the EMT is about to be destroyed not everytime while leaving HM
     *  context.
     */
    if (pVCpu->hm.s.vmx.uVmcsState & HMVMX_VMCS_STATE_ACTIVE)
    {
        int rc = VMXClearVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertRCReturn(rc, rc);

        pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_CLEAR;
        Log4Func(("Cleared Vmcs. HostCpuId=%u\n", idCpu));
    }
    Assert(!(pVCpu->hm.s.vmx.uVmcsState & HMVMX_VMCS_STATE_LAUNCHED));
    NOREF(idCpu);

    return VINF_SUCCESS;
}


/**
 * Leaves the VT-x session.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jmp zone!!!
 */
static int hmR0VmxLeaveSession(PVMCPU pVCpu)
{
    HM_DISABLE_PREEMPT(pVCpu);
    HMVMX_ASSERT_CPU_SAFE(pVCpu);
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* When thread-context hooks are used, we can avoid doing the leave again if we had been preempted before
       and done this from the VMXR0ThreadCtxCallback(). */
    if (!pVCpu->hm.s.fLeaveDone)
    {
        int rc2 = hmR0VmxLeave(pVCpu, true /* fImportState */);
        AssertRCReturnStmt(rc2, HM_RESTORE_PREEMPT(), rc2);
        pVCpu->hm.s.fLeaveDone = true;
    }
    Assert(!pVCpu->cpum.GstCtx.fExtrn);

    /*
     * !!! IMPORTANT !!!
     * If you modify code here, make sure to check whether hmR0VmxCallRing3Callback() needs to be updated too.
     */

    /* Deregister hook now that we've left HM context before re-enabling preemption. */
    /** @todo Deregistering here means we need to VMCLEAR always
     *        (longjmp/exit-to-r3) in VT-x which is not efficient, eliminate need
     *        for calling VMMR0ThreadCtxHookDisable here! */
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
DECLINLINE(int) hmR0VmxLongJmpToRing3(PVMCPU pVCpu)
{
    return hmR0VmxLeaveSession(pVCpu);
}


/**
 * Take necessary actions before going back to ring-3.
 *
 * An action requires us to go back to ring-3. This function does the necessary
 * steps before we can safely return to ring-3. This is not the same as longjmps
 * to ring-3, this is voluntary and prepares the guest so it may continue
 * executing outside HM (recompiler/IEM).
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   rcExit      The reason for exiting to ring-3. Can be
 *                      VINF_VMM_UNKNOWN_RING3_CALL.
 */
static int hmR0VmxExitToRing3(PVMCPU pVCpu, VBOXSTRICTRC rcExit)
{
    Assert(pVCpu);
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);

    if (RT_UNLIKELY(rcExit == VERR_VMX_INVALID_VMCS_PTR))
    {
        VMXGetActivatedVmcs(&pVCpu->hm.s.vmx.LastError.u64VMCSPhys);
        pVCpu->hm.s.vmx.LastError.u32VMCSRevision = *(uint32_t *)pVCpu->hm.s.vmx.pvVmcs;
        pVCpu->hm.s.vmx.LastError.idEnteredCpu    = pVCpu->hm.s.idEnteredCpu;
        /* LastError.idCurrentCpu was updated in hmR0VmxPreRunGuestCommitted(). */
    }

    /* Please, no longjumps here (any logging shouldn't flush jump back to ring-3). NO LOGGING BEFORE THIS POINT! */
    VMMRZCallRing3Disable(pVCpu);
    Log4Func(("rcExit=%d\n", VBOXSTRICTRC_VAL(rcExit)));

    /* We need to do this only while truly exiting the "inner loop" back to ring-3 and -not- for any longjmp to ring3. */
    if (pVCpu->hm.s.Event.fPending)
    {
        hmR0VmxPendingEventToTrpmTrap(pVCpu);
        Assert(!pVCpu->hm.s.Event.fPending);
    }

    /* Clear interrupt-window and NMI-window controls as we re-evaluate it when we return from ring-3. */
    hmR0VmxClearIntNmiWindowsVmcs(pVCpu);

    /* If we're emulating an instruction, we shouldn't have any TRPM traps pending
       and if we're injecting an event we should have a TRPM trap pending. */
    AssertMsg(rcExit != VINF_EM_RAW_INJECT_TRPM_EVENT || TRPMHasTrap(pVCpu), ("%Rrc\n", VBOXSTRICTRC_VAL(rcExit)));
#ifndef DEBUG_bird /* Triggered after firing an NMI against NT4SP1, possibly a triple fault in progress. */
    AssertMsg(rcExit != VINF_EM_RAW_EMULATE_INSTR || !TRPMHasTrap(pVCpu), ("%Rrc\n", VBOXSTRICTRC_VAL(rcExit)));
#endif

    /* Save guest state and restore host state bits. */
    int rc = hmR0VmxLeaveSession(pVCpu);
    AssertRCReturn(rc, rc);
    STAM_COUNTER_DEC(&pVCpu->hm.s.StatSwitchLongJmpToR3);
    /* Thread-context hooks are unregistered at this point!!! */

    /* Sync recompiler state. */
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

    Assert(!pVCpu->hm.s.fClearTrapFlag);

    /* Update the exit-to-ring 3 reason. */
    pVCpu->hm.s.rcLastExitToR3 = VBOXSTRICTRC_VAL(rcExit);

    /* On our way back from ring-3 reload the guest state if there is a possibility of it being changed. */
    if (rcExit != VINF_EM_RAW_INTERRUPT)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);

    STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchExitToR3);

    /* We do -not- want any longjmp notifications after this! We must return to ring-3 ASAP. */
    VMMRZCallRing3RemoveNotification(pVCpu);
    VMMRZCallRing3Enable(pVCpu);

    return rc;
}


/**
 * VMMRZCallRing3() callback wrapper which saves the guest state before we
 * longjump to ring-3 and possibly get preempted.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   enmOperation    The operation causing the ring-3 longjump.
 * @param   pvUser          User argument, currently unused, NULL.
 */
static DECLCALLBACK(int) hmR0VmxCallRing3Callback(PVMCPU pVCpu, VMMCALLRING3 enmOperation, void *pvUser)
{
    RT_NOREF(pvUser);
    if (enmOperation == VMMCALLRING3_VM_R0_ASSERTION)
    {
        /*
         * !!! IMPORTANT !!!
         * If you modify code here, check whether hmR0VmxLeave() and hmR0VmxLeaveSession() needs to be updated too.
         * This is a stripped down version which gets out ASAP, trying to not trigger any further assertions.
         */
        VMMRZCallRing3RemoveNotification(pVCpu);
        VMMRZCallRing3Disable(pVCpu);
        RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
        RTThreadPreemptDisable(&PreemptState);

        hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        CPUMR0FpuStateMaybeSaveGuestAndRestoreHost(pVCpu);
        CPUMR0DebugStateMaybeSaveGuestAndRestoreHost(pVCpu, true /* save DR6 */);

#if HC_ARCH_BITS == 64
        /* Restore host-state bits that VT-x only restores partially. */
        if (   (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_REQUIRED)
            && (pVCpu->hm.s.vmx.fRestoreHostFlags & ~VMX_RESTORE_HOST_REQUIRED))
            VMXRestoreHostState(pVCpu->hm.s.vmx.fRestoreHostFlags, &pVCpu->hm.s.vmx.RestoreHost);
        pVCpu->hm.s.vmx.fRestoreHostFlags = 0;
#endif

        /* Restore the lazy host MSRs as we're leaving VT-x context. */
        if (pVCpu->hm.s.vmx.fLazyMsrs & VMX_LAZY_MSRS_LOADED_GUEST)
            hmR0VmxLazyRestoreHostMsrs(pVCpu);

        /* Update auto-load/store host MSRs values when we re-enter VT-x (as we could be on a different CPU). */
        pVCpu->hm.s.vmx.fUpdatedHostMsrs = false;
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_HM, VMCPUSTATE_STARTED_EXEC);
        if (pVCpu->hm.s.vmx.uVmcsState & HMVMX_VMCS_STATE_ACTIVE)
        {
            VMXClearVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
            pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_CLEAR;
        }

        /** @todo eliminate the need for calling VMMR0ThreadCtxHookDisable here!  */
        VMMR0ThreadCtxHookDisable(pVCpu);
        HMR0LeaveCpu(pVCpu);
        RTThreadPreemptRestore(&PreemptState);
        return VINF_SUCCESS;
    }

    Assert(pVCpu);
    Assert(pvUser);
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);

    VMMRZCallRing3Disable(pVCpu);
    Assert(VMMR0IsLogFlushDisabled(pVCpu));

    Log4Func((" -> hmR0VmxLongJmpToRing3 enmOperation=%d\n", enmOperation));

    int rc = hmR0VmxLongJmpToRing3(pVCpu);
    AssertRCReturn(rc, rc);

    VMMRZCallRing3Enable(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Sets the interrupt-window exiting control in the VMCS which instructs VT-x to
 * cause a VM-exit as soon as the guest is in a state to receive interrupts.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxSetIntWindowExitVmcs(PVMCPU pVCpu)
{
    if (RT_LIKELY(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT))
    {
        if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT))
        {
            pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT;
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
            AssertRC(rc);
            Log4Func(("Setup interrupt-window exiting\n"));
        }
    } /* else we will deliver interrupts whenever the guest exits next and is in a state to receive events. */
}


/**
 * Clears the interrupt-window exiting control in the VMCS.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxClearIntWindowExitVmcs(PVMCPU pVCpu)
{
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT);
    pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT;
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
    AssertRC(rc);
    Log4Func(("Cleared interrupt-window exiting\n"));
}


/**
 * Sets the NMI-window exiting control in the VMCS which instructs VT-x to
 * cause a VM-exit as soon as the guest is in a state to receive NMIs.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxSetNmiWindowExitVmcs(PVMCPU pVCpu)
{
    if (RT_LIKELY(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT))
    {
        if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT))
        {
            pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT;
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
            AssertRC(rc);
            Log4Func(("Setup NMI-window exiting\n"));
        }
    } /* else we will deliver NMIs whenever we VM-exit next, even possibly nesting NMIs. Can't be helped on ancient CPUs. */
}


/**
 * Clears the NMI-window exiting control in the VMCS.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxClearNmiWindowExitVmcs(PVMCPU pVCpu)
{
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT);
    pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT;
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
    AssertRC(rc);
    Log4Func(("Cleared NMI-window exiting\n"));
}


/**
 * Evaluates the event to be delivered to the guest and sets it as the pending
 * event.
 *
 * @returns The VT-x guest-interruptibility state.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static uint32_t hmR0VmxEvaluatePendingEvent(PVMCPU pVCpu)
{
    /* Get the current interruptibility-state of the guest and then figure out what can be injected. */
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    uint32_t const fIntrState = hmR0VmxGetGuestIntrState(pVCpu);
    bool const fBlockMovSS    = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS);
    bool const fBlockSti      = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);
    bool const fBlockNmi      = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI);

    Assert(!fBlockSti || !(ASMAtomicUoReadU64(&pCtx->fExtrn) & CPUMCTX_EXTRN_RFLAGS));
    Assert(!(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI));    /* We don't support block-by-SMI yet.*/
    Assert(!fBlockSti || pCtx->eflags.Bits.u1IF);     /* Cannot set block-by-STI when interrupts are disabled. */
    Assert(!TRPMHasTrap(pVCpu));

    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_UPDATE_APIC))
        APICUpdatePendingInterrupts(pVCpu);

    /*
     * Toggling of interrupt force-flags here is safe since we update TRPM on premature exits
     * to ring-3 before executing guest code, see hmR0VmxExitToRing3(). We must NOT restore these force-flags.
     */
                                                               /** @todo SMI. SMIs take priority over NMIs. */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INTERRUPT_NMI))    /* NMI. NMIs take priority over regular interrupts. */
    {
        /* On some CPUs block-by-STI also blocks NMIs. See Intel spec. 26.3.1.5 "Checks On Guest Non-Register State". */
        if (   !pVCpu->hm.s.Event.fPending
            && !fBlockNmi
            && !fBlockSti
            && !fBlockMovSS)
        {
            Log4Func(("Pending NMI\n"));
            uint32_t u32IntInfo = X86_XCPT_NMI | VMX_EXIT_INTERRUPTION_INFO_VALID;
            u32IntInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

            hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
        }
        else
            hmR0VmxSetNmiWindowExitVmcs(pVCpu);
    }
    /*
     * Check if the guest can receive external interrupts (PIC/APIC). Once PDMGetInterrupt() returns
     * a valid interrupt we must- deliver the interrupt. We can no longer re-request it from the APIC.
     */
    else if (   VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
             && !pVCpu->hm.s.fSingleInstruction)
    {
        Assert(!DBGFIsStepping(pVCpu));
        int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_RFLAGS);
        AssertRCReturn(rc, 0);
        bool const fBlockInt = !(pCtx->eflags.u32 & X86_EFL_IF);
        if (   !pVCpu->hm.s.Event.fPending
            && !fBlockInt
            && !fBlockSti
            && !fBlockMovSS)
        {
            uint8_t u8Interrupt;
            rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
            if (RT_SUCCESS(rc))
            {
                Log4Func(("Pending external interrupt u8Interrupt=%#x\n", u8Interrupt));
                uint32_t u32IntInfo = u8Interrupt
                                    | VMX_EXIT_INTERRUPTION_INFO_VALID
                                    | (VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);

                hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrfaultAddress */);
            }
            else if (rc == VERR_APIC_INTR_MASKED_BY_TPR)
            {
                if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
                    hmR0VmxApicSetTprThreshold(pVCpu, u8Interrupt >> 4);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchTprMaskedIrq);

                /*
                 * If the CPU doesn't have TPR shadowing, we will always get a VM-exit on TPR changes and
                 * APICSetTpr() will end up setting the VMCPU_FF_INTERRUPT_APIC if required, so there is no
                 * need to re-set this force-flag here.
                 */
            }
            else
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchGuestIrq);
        }
        else
            hmR0VmxSetIntWindowExitVmcs(pVCpu);
    }

    return fIntrState;
}


/**
 * Sets a pending-debug exception to be delivered to the guest if the guest is
 * single-stepping in the VMCS.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(int) hmR0VmxSetPendingDebugXcptVmcs(PVMCPU pVCpu)
{
    Assert(!(ASMAtomicUoReadU64(&pVCpu->cpum.GstCtx.fExtrn) & CPUMCTX_EXTRN_RFLAGS));
    RT_NOREF(pVCpu);
    return VMXWriteVmcs32(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_BS);
}


/**
 * Injects any pending events into the guest if the guest is in a state to
 * receive them.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fIntrState      The VT-x guest-interruptibility state.
 * @param   fStepping       Running in hmR0VmxRunGuestCodeStep() and we should
 *                          return VINF_EM_DBG_STEPPED if the event was
 *                          dispatched directly.
 */
static VBOXSTRICTRC hmR0VmxInjectPendingEvent(PVMCPU pVCpu, uint32_t fIntrState, bool fStepping)
{
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);
    Assert(VMMRZCallRing3IsEnabled(pVCpu));

    bool fBlockMovSS = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS);
    bool fBlockSti   = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);

    Assert(!fBlockSti || !(ASMAtomicUoReadU64(&pVCpu->cpum.GstCtx.fExtrn) & CPUMCTX_EXTRN_RFLAGS));
    Assert(!(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI));         /* We don't support block-by-SMI yet.*/
    Assert(!fBlockSti || pVCpu->cpum.GstCtx.eflags.Bits.u1IF);   /* Cannot set block-by-STI when interrupts are disabled. */
    Assert(!TRPMHasTrap(pVCpu));

    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    if (pVCpu->hm.s.Event.fPending)
    {
        /*
         * Do -not- clear any interrupt-window exiting control here. We might have an interrupt
         * pending even while injecting an event and in this case, we want a VM-exit as soon as
         * the guest is ready for the next interrupt, see @bugref{6208#c45}.
         *
         * See Intel spec. 26.6.5 "Interrupt-Window Exiting and Virtual-Interrupt Delivery".
         */
        uint32_t const uIntType = VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hm.s.Event.u64IntInfo);
#ifdef VBOX_STRICT
        if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT)
        {
            bool const fBlockInt = !(pCtx->eflags.u32 & X86_EFL_IF);
            Assert(!fBlockInt);
            Assert(!fBlockSti);
            Assert(!fBlockMovSS);
        }
        else if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI)
        {
            bool const fBlockNmi = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI);
            Assert(!fBlockSti);
            Assert(!fBlockMovSS);
            Assert(!fBlockNmi);
        }
#endif
        Log4(("Injecting pending event vcpu[%RU32] u64IntInfo=%#RX64 Type=%#RX32\n", pVCpu->idCpu, pVCpu->hm.s.Event.u64IntInfo,
              uIntType));
        rcStrict = hmR0VmxInjectEventVmcs(pVCpu, pVCpu->hm.s.Event.u64IntInfo, pVCpu->hm.s.Event.cbInstr,
                                          pVCpu->hm.s.Event.u32ErrCode, pVCpu->hm.s.Event.GCPtrFaultAddress, fStepping,
                                          &fIntrState);
        AssertRCReturn(VBOXSTRICTRC_VAL(rcStrict), rcStrict);

        /* Update the interruptibility-state as it could have been changed by
           hmR0VmxInjectEventVmcs() (e.g. real-on-v86 guest injecting software interrupts) */
        fBlockMovSS = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS);
        fBlockSti   = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);

        if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT)
            STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectInterrupt);
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectXcpt);
    }

    /* Deliver pending debug exception if the guest is single-stepping. Evaluate and set the BS bit. */
    if (   fBlockSti
        || fBlockMovSS)
    {
        if (!pVCpu->hm.s.fSingleInstruction)
        {
            /*
             * The pending-debug exceptions field is cleared on all VM-exits except VMX_EXIT_TPR_BELOW_THRESHOLD,
             * VMX_EXIT_MTF, VMX_EXIT_APIC_WRITE and VMX_EXIT_VIRTUALIZED_EOI.
             * See Intel spec. 27.3.4 "Saving Non-Register State".
             */
            Assert(!DBGFIsStepping(pVCpu));
            int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_RFLAGS);
            AssertRCReturn(rc, rc);
            if (pCtx->eflags.Bits.u1TF)
            {
                int rc2 = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                AssertRCReturn(rc2, rc2);
            }
        }
        else if (pCtx->eflags.Bits.u1TF)
        {
            /*
             * We are single-stepping in the hypervisor debugger using EFLAGS.TF. Clear interrupt inhibition as setting the
             * BS bit would mean delivering a #DB to the guest upon VM-entry when it shouldn't be.
             */
            Assert(!(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG));
            fIntrState = 0;
        }
    }

    /*
     * There's no need to clear the VM-entry interruption-information field here if we're not injecting anything.
     * VT-x clears the valid bit on every VM-exit. See Intel spec. 24.8.3 "VM-Entry Controls for Event Injection".
     */
    int rc3 = hmR0VmxExportGuestIntrState(pVCpu, fIntrState);
    AssertRCReturn(rc3, rc3);

    Assert(rcStrict == VINF_SUCCESS || rcStrict == VINF_EM_RESET || (rcStrict == VINF_EM_DBG_STEPPED && fStepping));
    NOREF(fBlockMovSS); NOREF(fBlockSti);
    return rcStrict;
}


/**
 * Sets an invalid-opcode (\#UD) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptUD(PVMCPU pVCpu)
{
    uint32_t u32IntInfo = X86_XCPT_UD | VMX_EXIT_INTERRUPTION_INFO_VALID;
    hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Injects a double-fault (\#DF) exception into the VM.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fStepping       Whether we're running in hmR0VmxRunGuestCodeStep()
 *                          and should return VINF_EM_DBG_STEPPED if the event
 *                          is injected directly (register modified by us, not
 *                          by hardware on VM-entry).
 * @param   pfIntrState     Pointer to the current guest interruptibility-state.
 *                          This interruptibility-state will be updated if
 *                          necessary. This cannot not be NULL.
 */
DECLINLINE(VBOXSTRICTRC) hmR0VmxInjectXcptDF(PVMCPU pVCpu, bool fStepping, uint32_t *pfIntrState)
{
    uint32_t u32IntInfo  = X86_XCPT_DF | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntInfo          |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    u32IntInfo          |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
    return hmR0VmxInjectEventVmcs(pVCpu, u32IntInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */, fStepping,
                                  pfIntrState);
}


/**
 * Sets a debug (\#DB) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptDB(PVMCPU pVCpu)
{
    uint32_t u32IntInfo  = X86_XCPT_DB | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntInfo          |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Sets an overflow (\#OF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The value of RIP that is to be pushed on the guest
 *                          stack.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptOF(PVMCPU pVCpu, uint32_t cbInstr)
{
    uint32_t u32IntInfo  = X86_XCPT_OF | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntInfo          |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, cbInstr, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Injects a general-protection (\#GP) fault into the VM.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   fErrorCodeValid     Whether the error code is valid (depends on the CPU
 *                              mode, i.e. in real-mode it's not valid).
 * @param   u32ErrorCode        The error code associated with the \#GP.
 * @param   fStepping           Whether we're running in
 *                              hmR0VmxRunGuestCodeStep() and should return
 *                              VINF_EM_DBG_STEPPED if the event is injected
 *                              directly (register modified by us, not by
 *                              hardware on VM-entry).
 * @param   pfIntrState         Pointer to the current guest interruptibility-state.
 *                              This interruptibility-state will be updated if
 *                              necessary. This cannot not be NULL.
 */
DECLINLINE(VBOXSTRICTRC) hmR0VmxInjectXcptGP(PVMCPU pVCpu, bool fErrorCodeValid, uint32_t u32ErrorCode, bool fStepping,
                                             uint32_t *pfIntrState)
{
    uint32_t u32IntInfo  = X86_XCPT_GP | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntInfo          |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    if (fErrorCodeValid)
        u32IntInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
    return hmR0VmxInjectEventVmcs(pVCpu, u32IntInfo, 0 /* cbInstr */, u32ErrorCode, 0 /* GCPtrFaultAddress */, fStepping,
                                  pfIntrState);
}


/**
 * Sets a software interrupt (INTn) as pending-for-injection into the VM.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uVector         The software interrupt vector number.
 * @param   cbInstr         The value of RIP that is to be pushed on the guest
 *                          stack.
 */
DECLINLINE(void) hmR0VmxSetPendingIntN(PVMCPU pVCpu, uint16_t uVector, uint32_t cbInstr)
{
    uint32_t u32IntInfo = uVector | VMX_EXIT_INTERRUPTION_INFO_VALID;
    if (   uVector == X86_XCPT_BP
        || uVector == X86_XCPT_OF)
        u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    else
        u32IntInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    hmR0VmxSetPendingEvent(pVCpu, u32IntInfo, cbInstr, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Pushes a 2-byte value onto the real-mode (in virtual-8086 mode) guest's
 * stack.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_EM_RESET if pushing a value to the stack caused a triple-fault.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uValue      The value to push to the guest stack.
 */
static VBOXSTRICTRC hmR0VmxRealModeGuestStackPush(PVMCPU pVCpu, uint16_t uValue)
{
    /*
     * The stack limit is 0xffff in real-on-virtual 8086 mode. Real-mode with weird stack limits cannot be run in
     * virtual 8086 mode in VT-x. See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers".
     * See Intel Instruction reference for PUSH and Intel spec. 22.33.1 "Segment Wraparound".
     */
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (pCtx->sp == 1)
        return VINF_EM_RESET;
    pCtx->sp -= sizeof(uint16_t);       /* May wrap around which is expected behaviour. */
    int rc = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), pCtx->ss.u64Base + pCtx->sp, &uValue, sizeof(uint16_t));
    AssertRC(rc);
    return rc;
}


/**
 * Injects an event into the guest upon VM-entry by updating the relevant fields
 * in the VM-entry area in the VMCS.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_SUCCESS if the event is successfully injected into the VMCS.
 * @retval  VINF_EM_RESET if event injection resulted in a triple-fault.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   u64IntInfo          The VM-entry interruption-information field.
 * @param   cbInstr             The VM-entry instruction length in bytes (for
 *                              software interrupts, exceptions and privileged
 *                              software exceptions).
 * @param   u32ErrCode          The VM-entry exception error code.
 * @param   GCPtrFaultAddress   The page-fault address for \#PF exceptions.
 * @param   pfIntrState         Pointer to the current guest interruptibility-state.
 *                              This interruptibility-state will be updated if
 *                              necessary. This cannot not be NULL.
 * @param   fStepping           Whether we're running in
 *                              hmR0VmxRunGuestCodeStep() and should return
 *                              VINF_EM_DBG_STEPPED if the event is injected
 *                              directly (register modified by us, not by
 *                              hardware on VM-entry).
 */
static VBOXSTRICTRC hmR0VmxInjectEventVmcs(PVMCPU pVCpu, uint64_t u64IntInfo, uint32_t cbInstr, uint32_t u32ErrCode,
                                           RTGCUINTREG GCPtrFaultAddress, bool fStepping, uint32_t *pfIntrState)
{
    /* Intel spec. 24.8.3 "VM-Entry Controls for Event Injection" specifies the interruption-information field to be 32-bits. */
    AssertMsg(!RT_HI_U32(u64IntInfo), ("%#RX64\n", u64IntInfo));
    Assert(pfIntrState);

    PCPUMCTX       pCtx       = &pVCpu->cpum.GstCtx;
    uint32_t       u32IntInfo = (uint32_t)u64IntInfo;
    uint32_t const uVector    = VMX_EXIT_INTERRUPTION_INFO_VECTOR(u32IntInfo);
    uint32_t const uIntType   = VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntInfo);

#ifdef VBOX_STRICT
    /*
     * Validate the error-code-valid bit for hardware exceptions.
     * No error codes for exceptions in real-mode.
     *
     * See Intel spec. 20.1.4 "Interrupt and Exception Handling"
     */
    if (   uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT
        && !CPUMIsGuestInRealModeEx(pCtx))
    {
        switch (uVector)
        {
            case X86_XCPT_PF:
            case X86_XCPT_DF:
            case X86_XCPT_TS:
            case X86_XCPT_NP:
            case X86_XCPT_SS:
            case X86_XCPT_GP:
            case X86_XCPT_AC:
                AssertMsg(VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(u32IntInfo),
                          ("Error-code-valid bit not set for exception that has an error code uVector=%#x\n", uVector));
                RT_FALL_THRU();
            default:
                break;
        }
    }
#endif

    /* Cannot inject an NMI when block-by-MOV SS is in effect. */
    Assert(   uIntType != VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI
           || !(*pfIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS));

    STAM_COUNTER_INC(&pVCpu->hm.s.paStatInjectedIrqsR0[uVector & MASK_INJECT_IRQ_STAT]);

    /*
     * Hardware interrupts & exceptions cannot be delivered through the software interrupt
     * redirection bitmap to the real mode task in virtual-8086 mode. We must jump to the
     * interrupt handler in the (real-mode) guest.
     *
     * See Intel spec. 20.3 "Interrupt and Exception handling in Virtual-8086 Mode".
     * See Intel spec. 20.1.4 "Interrupt and Exception Handling" for real-mode interrupt handling.
     */
    if (CPUMIsGuestInRealModeEx(pCtx))     /* CR0.PE bit changes are always intercepted, so it's up to date. */
    {
        if (pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fUnrestrictedGuest)
        {
            /*
             * For unrestricted execution enabled CPUs running real-mode guests, we must not
             * set the deliver-error-code bit.
             *
             * See Intel spec. 26.2.1.3 "VM-Entry Control Fields".
             */
            u32IntInfo &= ~VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
        }
        else
        {
            PVM pVM = pVCpu->CTX_SUFF(pVM);
            Assert(PDMVmmDevHeapIsEnabled(pVM));
            Assert(pVM->hm.s.vmx.pRealModeTSS);

            /* We require RIP, RSP, RFLAGS, CS, IDTR, import them. */
            int rc2 = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_SREG_MASK | CPUMCTX_EXTRN_TABLE_MASK | CPUMCTX_EXTRN_RIP
                                                   | CPUMCTX_EXTRN_RSP       | CPUMCTX_EXTRN_RFLAGS);
            AssertRCReturn(rc2, rc2);

            /* Check if the interrupt handler is present in the IVT (real-mode IDT). IDT limit is (4N - 1). */
            size_t const cbIdtEntry = sizeof(X86IDTR16);
            if (uVector * cbIdtEntry + (cbIdtEntry - 1) > pCtx->idtr.cbIdt)
            {
                /* If we are trying to inject a #DF with no valid IDT entry, return a triple-fault. */
                if (uVector == X86_XCPT_DF)
                    return VINF_EM_RESET;

                /* If we're injecting a #GP with no valid IDT entry, inject a double-fault. */
                if (uVector == X86_XCPT_GP)
                    return hmR0VmxInjectXcptDF(pVCpu, fStepping, pfIntrState);

                /*
                 * If we're injecting an event with no valid IDT entry, inject a #GP.
                 * No error codes for exceptions in real-mode.
                 *
                 * See Intel spec. 20.1.4 "Interrupt and Exception Handling"
                 */
                return hmR0VmxInjectXcptGP(pVCpu, false /* fErrCodeValid */, 0 /* u32ErrCode */, fStepping, pfIntrState);
            }

            /* Software exceptions (#BP and #OF exceptions thrown as a result of INT3 or INTO) */
            uint16_t uGuestIp = pCtx->ip;
            if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT)
            {
                Assert(uVector == X86_XCPT_BP || uVector == X86_XCPT_OF);
                /* #BP and #OF are both benign traps, we need to resume the next instruction. */
                uGuestIp = pCtx->ip + (uint16_t)cbInstr;
            }
            else if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT)
                uGuestIp = pCtx->ip + (uint16_t)cbInstr;

            /* Get the code segment selector and offset from the IDT entry for the interrupt handler. */
            X86IDTR16 IdtEntry;
            RTGCPHYS GCPhysIdtEntry = (RTGCPHYS)pCtx->idtr.pIdt + uVector * cbIdtEntry;
            rc2 = PGMPhysSimpleReadGCPhys(pVM, &IdtEntry, GCPhysIdtEntry, cbIdtEntry);
            AssertRCReturn(rc2, rc2);

            /* Construct the stack frame for the interrupt/exception handler. */
            VBOXSTRICTRC rcStrict;
            rcStrict = hmR0VmxRealModeGuestStackPush(pVCpu, pCtx->eflags.u32);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = hmR0VmxRealModeGuestStackPush(pVCpu, pCtx->cs.Sel);
            if (rcStrict == VINF_SUCCESS)
                rcStrict = hmR0VmxRealModeGuestStackPush(pVCpu, uGuestIp);

            /* Clear the required eflag bits and jump to the interrupt/exception handler. */
            if (rcStrict == VINF_SUCCESS)
            {
                pCtx->eflags.u32 &= ~(X86_EFL_IF | X86_EFL_TF | X86_EFL_RF | X86_EFL_AC);
                pCtx->rip         = IdtEntry.offSel;
                pCtx->cs.Sel      = IdtEntry.uSel;
                pCtx->cs.ValidSel = IdtEntry.uSel;
                pCtx->cs.u64Base  = IdtEntry.uSel << cbIdtEntry;
                if (   uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT
                    && uVector  == X86_XCPT_PF)
                    pCtx->cr2 = GCPtrFaultAddress;

                /* If any other guest-state bits are changed here, make sure to update
                   hmR0VmxPreRunGuestCommitted() when thread-context hooks are used. */
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CS  | HM_CHANGED_GUEST_CR2
                                                         | HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS
                                                         | HM_CHANGED_GUEST_RSP);

                /* We're clearing interrupts, which means no block-by-STI interrupt-inhibition. */
                if (*pfIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI)
                {
                    Assert(   uIntType != VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI
                           && uIntType != VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT);
                    Log4Func(("Clearing inhibition due to STI\n"));
                    *pfIntrState &= ~VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI;
                }
                Log4(("Injecting real-mode: u32IntInfo=%#x u32ErrCode=%#x cbInstr=%#x Eflags=%#x CS:EIP=%04x:%04x\n",
                      u32IntInfo, u32ErrCode, cbInstr, pCtx->eflags.u, pCtx->cs.Sel, pCtx->eip));

                /* The event has been truly dispatched. Mark it as no longer pending so we don't attempt to 'undo'
                   it, if we are returning to ring-3 before executing guest code. */
                pVCpu->hm.s.Event.fPending = false;

                /* Make hmR0VmxPreRunGuest() return if we're stepping since we've changed cs:rip. */
                if (fStepping)
                    rcStrict = VINF_EM_DBG_STEPPED;
            }
            AssertMsg(rcStrict == VINF_SUCCESS || rcStrict == VINF_EM_RESET || (rcStrict == VINF_EM_DBG_STEPPED && fStepping),
                      ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
            return rcStrict;
        }
    }

    /* Validate. */
    Assert(VMX_EXIT_INTERRUPTION_INFO_IS_VALID(u32IntInfo));             /* Bit 31 (Valid bit) must be set by caller. */
    Assert(!VMX_EXIT_INTERRUPTION_INFO_NMI_UNBLOCK_IRET(u32IntInfo));    /* Bit 12 MBZ. */
    Assert(!(u32IntInfo & 0x7ffff000));                                  /* Bits 30:12 MBZ. */

    /* Inject. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, u32IntInfo);
    if (VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(u32IntInfo))
        rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE, u32ErrCode);
    rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH, cbInstr);
    AssertRCReturn(rc, rc);

    /* Update CR2. */
    if (   VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT
        && uVector == X86_XCPT_PF)
        pCtx->cr2 = GCPtrFaultAddress;

    Log4(("Injecting u32IntInfo=%#x u32ErrCode=%#x cbInstr=%#x CR2=%#RX64\n", u32IntInfo, u32ErrCode, cbInstr, pCtx->cr2));

    return VINF_SUCCESS;
}


/**
 * Clears the interrupt-window exiting control in the VMCS and if necessary
 * clears the current event in the VMCS as well.
 *
 * @returns VBox status code.
 * @param   pVCpu         The cross context virtual CPU structure.
 *
 * @remarks Use this function only to clear events that have not yet been
 *          delivered to the guest but are injected in the VMCS!
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxClearIntNmiWindowsVmcs(PVMCPU pVCpu)
{
    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT)
    {
        hmR0VmxClearIntWindowExitVmcs(pVCpu);
        Log4Func(("Cleared interrupt widow\n"));
    }

    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT)
    {
        hmR0VmxClearNmiWindowExitVmcs(pVCpu);
        Log4Func(("Cleared interrupt widow\n"));
    }
}


/**
 * Enters the VT-x session.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pHostCpu    Pointer to the global CPU info struct.
 */
VMMR0DECL(int) VMXR0Enter(PVMCPU pVCpu, PHMGLOBALCPUINFO pHostCpu)
{
    AssertPtr(pVCpu);
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fSupported);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    RT_NOREF(pHostCpu);

    LogFlowFunc(("pVCpu=%p\n", pVCpu));
    Assert((pVCpu->hm.s.fCtxChanged &  (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE))
                                    == (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE));

#ifdef VBOX_STRICT
    /* At least verify VMX is enabled, since we can't check if we're in VMX root mode without #GP'ing. */
    RTCCUINTREG uHostCR4 = ASMGetCR4();
    if (!(uHostCR4 & X86_CR4_VMXE))
    {
        LogRelFunc(("X86_CR4_VMXE bit in CR4 is not set!\n"));
        return VERR_VMX_X86_CR4_VMXE_CLEARED;
    }
#endif

    /*
     * Load the VCPU's VMCS as the current (and active) one.
     */
    Assert(pVCpu->hm.s.vmx.uVmcsState & HMVMX_VMCS_STATE_CLEAR);
    int rc = VMXActivateVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
    if (RT_FAILURE(rc))
        return rc;

    pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_ACTIVE;
    pVCpu->hm.s.fLeaveDone = false;
    Log4Func(("Activated Vmcs. HostCpuId=%u\n", RTMpCpuId()));

    return VINF_SUCCESS;
}


/**
 * The thread-context callback (only on platforms which support it).
 *
 * @param   enmEvent        The thread-context event.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fGlobalInit     Whether global VT-x/AMD-V init. was used.
 * @thread  EMT(pVCpu)
 */
VMMR0DECL(void) VMXR0ThreadCtxCallback(RTTHREADCTXEVENT enmEvent, PVMCPU pVCpu, bool fGlobalInit)
{
    NOREF(fGlobalInit);

    switch (enmEvent)
    {
        case RTTHREADCTXEVENT_OUT:
        {
            Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
            Assert(VMMR0ThreadCtxHookIsEnabled(pVCpu));
            VMCPU_ASSERT_EMT(pVCpu);

            /* No longjmps (logger flushes, locks) in this fragile context. */
            VMMRZCallRing3Disable(pVCpu);
            Log4Func(("Preempting: HostCpuId=%u\n", RTMpCpuId()));

            /*
             * Restore host-state (FPU, debug etc.)
             */
            if (!pVCpu->hm.s.fLeaveDone)
            {
                /*
                 * Do -not- import the guest-state here as we might already be in the middle of importing
                 * it, esp. bad if we're holding the PGM lock, see comment in hmR0VmxImportGuestState().
                 */
                hmR0VmxLeave(pVCpu, false /* fImportState */);
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

            /* No longjmps here, as we don't want to trigger preemption (& its hook) while resuming. */
            VMMRZCallRing3Disable(pVCpu);
            Log4Func(("Resumed: HostCpuId=%u\n", RTMpCpuId()));

            /* Initialize the bare minimum state required for HM. This takes care of
               initializing VT-x if necessary (onlined CPUs, local init etc.) */
            int rc = hmR0EnterCpu(pVCpu);
            AssertRC(rc);
            Assert((pVCpu->hm.s.fCtxChanged &  (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE))
                                            == (HM_CHANGED_HOST_CONTEXT | HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE));

            /* Load the active VMCS as the current one. */
            if (pVCpu->hm.s.vmx.uVmcsState & HMVMX_VMCS_STATE_CLEAR)
            {
                rc = VMXActivateVmcs(pVCpu->hm.s.vmx.HCPhysVmcs);
                AssertRC(rc); NOREF(rc);
                pVCpu->hm.s.vmx.uVmcsState = HMVMX_VMCS_STATE_ACTIVE;
                Log4Func(("Resumed: Activated Vmcs. HostCpuId=%u\n", RTMpCpuId()));
            }
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
 * Exports the host state into the VMCS host-state area.
 * Sets up the VM-exit MSR-load area.
 *
 * The CPU state will be loaded from these fields on every successful VM-exit.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxExportHostState(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_HOST_CONTEXT)
    {
        rc = hmR0VmxExportHostControlRegs();
        AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

        rc = hmR0VmxExportHostSegmentRegs(pVCpu);
        AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

        rc = hmR0VmxExportHostMsrs(pVCpu);
        AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

        pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_HOST_CONTEXT;
    }
    return rc;
}


/**
 * Saves the host state in the VMCS host-state.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(int) VMXR0ExportHostState(PVMCPU pVCpu)
{
    AssertPtr(pVCpu);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Export the host state here while entering HM context.
     * When thread-context hooks are used, we might get preempted and have to re-save the host
     * state but most of the time we won't be, so do it here before we disable interrupts.
     */
    return hmR0VmxExportHostState(pVCpu);
}


/**
 * Exports the guest state into the VMCS guest-state area.
 *
 * The will typically be done before VM-entry when the guest-CPU state and the
 * VMCS state may potentially be out of sync.
 *
 * Sets up the VM-entry MSR-load and VM-exit MSR-store areas. Sets up the
 * VM-entry controls.
 * Sets up the appropriate VMX non-root function to execute guest code based on
 * the guest CPU mode.
 *
 * @returns VBox strict status code.
 * @retval  VINF_EM_RESCHEDULE_REM if we try to emulate non-paged guest code
 *          without unrestricted guest access and the VMMDev is not presently
 *          mapped (e.g. EFI32).
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static VBOXSTRICTRC hmR0VmxExportGuestState(PVMCPU pVCpu)
{
    AssertPtr(pVCpu);
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);

    LogFlowFunc(("pVCpu=%p\n", pVCpu));

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExportGuestState, x);

    /* Determine real-on-v86 mode. */
    pVCpu->hm.s.vmx.RealMode.fRealOnV86Active = false;
    if (   !pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fUnrestrictedGuest
        &&  CPUMIsGuestInRealModeEx(&pVCpu->cpum.GstCtx))
    {
        pVCpu->hm.s.vmx.RealMode.fRealOnV86Active = true;
    }

    /*
     * Any ordering dependency among the sub-functions below must be explicitly stated using comments.
     * Ideally, assert that the cross-dependent bits are up-to-date at the point of using it.
     */
    int rc = hmR0VmxSelectVMRunHandler(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    /* This needs to be done after hmR0VmxSelectVMRunHandler() as changing pfnStartVM may require VM-entry control updates. */
    rc = hmR0VmxExportGuestEntryCtls(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    /* This needs to be done after hmR0VmxSelectVMRunHandler() as changing pfnStartVM may require VM-exit control updates. */
    rc = hmR0VmxExportGuestExitCtls(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    rc = hmR0VmxExportGuestCR0(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    VBOXSTRICTRC rcStrict = hmR0VmxExportGuestCR3AndCR4(pVCpu);
    if (rcStrict == VINF_SUCCESS)
    { /* likely */ }
    else
    {
        Assert(rcStrict == VINF_EM_RESCHEDULE_REM || RT_FAILURE_NP(rcStrict));
        return rcStrict;
    }

    rc = hmR0VmxExportGuestSegmentRegs(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    /* This needs to be done after hmR0VmxExportGuestEntryCtls() and hmR0VmxExportGuestExitCtls() as it
       may alter controls if we determine we don't have to swap EFER after all. */
    rc = hmR0VmxExportGuestMsrs(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    rc = hmR0VmxExportGuestApicTpr(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    rc = hmR0VmxExportGuestXcptIntercepts(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    /* Exporting RFLAGS here is fine, even though RFLAGS.TF might depend on guest debug state which is
       not exported here. It is re-evaluated and updated if necessary in hmR0VmxExportSharedState(). */
    rc  = hmR0VmxExportGuestRip(pVCpu);
    rc |= hmR0VmxExportGuestRsp(pVCpu);
    rc |= hmR0VmxExportGuestRflags(pVCpu);
    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc\n", rc), rc);

    /* Clear any bits that may be set but exported unconditionally or unused/reserved bits. */
    ASMAtomicUoAndU64(&pVCpu->hm.s.fCtxChanged, ~(  (HM_CHANGED_GUEST_GPRS_MASK & ~HM_CHANGED_GUEST_RSP)
                                                  |  HM_CHANGED_GUEST_CR2
                                                  | (HM_CHANGED_GUEST_DR_MASK & ~HM_CHANGED_GUEST_DR7)
                                                  |  HM_CHANGED_GUEST_X87
                                                  |  HM_CHANGED_GUEST_SSE_AVX
                                                  |  HM_CHANGED_GUEST_OTHER_XSAVE
                                                  |  HM_CHANGED_GUEST_XCRx
                                                  |  HM_CHANGED_GUEST_KERNEL_GS_BASE /* Part of lazy or auto load-store MSRs. */
                                                  |  HM_CHANGED_GUEST_SYSCALL_MSRS   /* Part of lazy or auto load-store MSRs. */
                                                  |  HM_CHANGED_GUEST_TSC_AUX
                                                  |  HM_CHANGED_GUEST_OTHER_MSRS
                                                  |  HM_CHANGED_GUEST_HWVIRT
                                                  | (HM_CHANGED_KEEPER_STATE_MASK & ~HM_CHANGED_VMX_MASK)));

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExportGuestState, x);
    return rc;
}


/**
 * Exports the state shared between the host and guest into the VMCS.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxExportSharedState(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_DR_MASK)
    {
        int rc = hmR0VmxExportSharedDebugState(pVCpu);
        AssertRC(rc);
        pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_GUEST_DR_MASK;

        /* Loading shared debug bits might have changed eflags.TF bit for debugging purposes. */
        if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_GUEST_RFLAGS)
        {
            rc = hmR0VmxExportGuestRflags(pVCpu);
            AssertRC(rc);
        }
    }

    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_VMX_GUEST_LAZY_MSRS)
    {
        hmR0VmxLazyLoadGuestMsrs(pVCpu);
        pVCpu->hm.s.fCtxChanged &= ~HM_CHANGED_VMX_GUEST_LAZY_MSRS;
    }

    AssertMsg(!(pVCpu->hm.s.fCtxChanged & HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE),
              ("fCtxChanged=%#RX64\n", pVCpu->hm.s.fCtxChanged));
}


/**
 * Worker for loading the guest-state bits in the inner VT-x execution loop.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_EM_RESCHEDULE_REM if we try to emulate non-paged guest code
 *          without unrestricted guest access and the VMMDev is not presently
 *          mapped (e.g. EFI32).
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 *
 * @remarks No-long-jump zone!!!
 */
static VBOXSTRICTRC hmR0VmxExportGuestStateOptimal(PVMCPU pVCpu)
{
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(VMMR0IsLogFlushDisabled(pVCpu));

#ifdef HMVMX_ALWAYS_SYNC_FULL_GUEST_STATE
    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
#endif

    /*
     * For many exits it's only RIP that changes and hence try to export it first
     * without going through a lot of change flag checks.
     */
    VBOXSTRICTRC rcStrict;
    uint64_t     fCtxChanged = ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    if ((fCtxChanged & (HM_CHANGED_ALL_GUEST & ~HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE)) == HM_CHANGED_GUEST_RIP)
    {
        rcStrict = hmR0VmxExportGuestRip(pVCpu);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /* likely */}
        else
            AssertMsgFailedReturn(("hmR0VmxExportGuestRip failed! rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)), rcStrict);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExportMinimal);
    }
    else if (fCtxChanged & (HM_CHANGED_ALL_GUEST & ~HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE))
    {
        rcStrict = hmR0VmxExportGuestState(pVCpu);
        if (RT_LIKELY(rcStrict == VINF_SUCCESS))
        { /* likely */}
        else
        {
            AssertMsg(rcStrict == VINF_EM_RESCHEDULE_REM, ("hmR0VmxExportGuestState failed! rc=%Rrc\n",
                                                           VBOXSTRICTRC_VAL(rcStrict)));
            Assert(!VMMRZCallRing3IsEnabled(pVCpu));
            return rcStrict;
        }
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExportFull);
    }
    else
        rcStrict = VINF_SUCCESS;

#ifdef VBOX_STRICT
    /* All the guest state bits should be loaded except maybe the host context and/or the shared host/guest bits. */
    fCtxChanged = ASMAtomicUoReadU64(&pVCpu->hm.s.fCtxChanged);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    AssertMsg(!(fCtxChanged & (HM_CHANGED_ALL_GUEST & ~HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE)),
              ("fCtxChanged=%#RX64\n", fCtxChanged));
#endif
    return rcStrict;
}


/**
 * Does the preparations before executing guest code in VT-x.
 *
 * This may cause longjmps to ring-3 and may even result in rescheduling to the
 * recompiler/IEM. We must be cautious what we do here regarding committing
 * guest-state information into the VMCS assuming we assuredly execute the
 * guest in VT-x mode.
 *
 * If we fall back to the recompiler/IEM after updating the VMCS and clearing
 * the common-state (TRPM/forceflags), we must undo those changes so that the
 * recompiler/IEM can (and should) use them when it resumes guest execution.
 * Otherwise such operations must be done when we can no longer exit to ring-3.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @retval  VINF_SUCCESS if we can proceed with running the guest, interrupts
 *          have been disabled.
 * @retval  VINF_EM_RESET if a triple-fault occurs while injecting a
 *          double-fault into the guest.
 * @retval  VINF_EM_DBG_STEPPED if @a fStepping is true and an event was
 *          dispatched directly.
 * @retval  VINF_* scheduling changes, we have to go back to ring-3.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 * @param   fStepping       Set if called from hmR0VmxRunGuestCodeStep().  Makes
 *                          us ignore some of the reasons for returning to
 *                          ring-3, and return VINF_EM_DBG_STEPPED if event
 *                          dispatching took place.
 */
static VBOXSTRICTRC hmR0VmxPreRunGuest(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient, bool fStepping)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PGMRZDynMapFlushAutoSet(pVCpu);
#endif

    /* Check force flag actions that might require us to go back to ring-3. */
    VBOXSTRICTRC rcStrict = hmR0VmxCheckForceFlags(pVCpu, fStepping);
    if (rcStrict == VINF_SUCCESS)
    { /* FFs doesn't get set all the time. */ }
    else
        return rcStrict;

    /*
     * Setup the virtualized-APIC accesses.
     *
     * Note! This can cause a longjumps to R3 due to the acquisition of the PGM lock
     * in both PGMHandlerPhysicalReset() and IOMMMIOMapMMIOHCPage(), see @bugref{8721}.
     *
     * This is the reason we do it here and not in hmR0VmxExportGuestState().
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (   !pVCpu->hm.s.vmx.u64MsrApicBase
        && (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
        && PDMHasApic(pVM))
    {
        uint64_t const u64MsrApicBase = APICGetBaseMsrNoCheck(pVCpu);
        Assert(u64MsrApicBase);
        Assert(pVM->hm.s.vmx.HCPhysApicAccess);

        RTGCPHYS const GCPhysApicBase = u64MsrApicBase & PAGE_BASE_GC_MASK;

        /* Unalias any existing mapping. */
        int rc = PGMHandlerPhysicalReset(pVM, GCPhysApicBase);
        AssertRCReturn(rc, rc);

        /* Map the HC APIC-access page in place of the MMIO page, also updates the shadow page tables if necessary. */
        Log4Func(("Mapped HC APIC-access page at %#RGp\n", GCPhysApicBase));
        rc = IOMMMIOMapMMIOHCPage(pVM, pVCpu, GCPhysApicBase, pVM->hm.s.vmx.HCPhysApicAccess, X86_PTE_RW | X86_PTE_P);
        AssertRCReturn(rc, rc);

        /* Update the per-VCPU cache of the APIC base MSR. */
        pVCpu->hm.s.vmx.u64MsrApicBase = u64MsrApicBase;
    }

    if (TRPMHasTrap(pVCpu))
        hmR0VmxTrpmTrapToPendingEvent(pVCpu);
    uint32_t fIntrState = hmR0VmxEvaluatePendingEvent(pVCpu);

    /*
     * Event injection may take locks (currently the PGM lock for real-on-v86 case) and thus
     * needs to be done with longjmps or interrupts + preemption enabled. Event injection might
     * also result in triple-faulting the VM.
     */
    rcStrict = hmR0VmxInjectPendingEvent(pVCpu, fIntrState, fStepping);
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    { /* likely */ }
    else
    {
        AssertMsg(rcStrict == VINF_EM_RESET || (rcStrict == VINF_EM_DBG_STEPPED && fStepping),
                  ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
        return rcStrict;
    }

    /*
     * A longjump might result in importing CR3 even for VM-exits that don't necessarily
     * import CR3 themselves. We will need to update them here, as even as late as the above
     * hmR0VmxInjectPendingEvent() call may lazily import guest-CPU state on demand causing
     * the below force flags to be set.
     */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
    {
        Assert(!(ASMAtomicUoReadU64(&pVCpu->cpum.GstCtx.fExtrn) & CPUMCTX_EXTRN_CR3));
        int rc2 = PGMUpdateCR3(pVCpu, CPUMGetGuestCR3(pVCpu));
        AssertMsgReturn(rc2 == VINF_SUCCESS || rc2 == VINF_PGM_SYNC_CR3,
                        ("%Rrc\n", rc2), RT_FAILURE_NP(rc2) ? rc2 : VERR_IPE_UNEXPECTED_INFO_STATUS);
        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
    }
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES))
    {
        PGMGstUpdatePaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);
        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));
    }

    /*
     * No longjmps to ring-3 from this point on!!!
     * Asserts() will still longjmp to ring-3 (but won't return), which is intentional, better than a kernel panic.
     * This also disables flushing of the R0-logger instance (if any).
     */
    VMMRZCallRing3Disable(pVCpu);

    /*
     * Export the guest state bits.
     *
     * We cannot perform longjmps while loading the guest state because we do not preserve the
     * host/guest state (although the VMCS will be preserved) across longjmps which can cause
     * CPU migration.
     *
     * If we are injecting events to a real-on-v86 mode guest, we will have to update
     * RIP and some segment registers, i.e. hmR0VmxInjectPendingEvent()->hmR0VmxInjectEventVmcs().
     * Hence, loading of the guest state needs to be done -after- injection of events.
     */
    rcStrict = hmR0VmxExportGuestStateOptimal(pVCpu);
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    { /* likely */ }
    else
    {
        VMMRZCallRing3Enable(pVCpu);
        return rcStrict;
    }

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
    pVmxTransient->fEFlags = ASMIntDisableFlags();

    if (   (   !VM_FF_IS_PENDING(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        || (   fStepping /* Optimized for the non-stepping case, so a bit of unnecessary work when stepping. */
            && !VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK & ~(VMCPU_FF_TIMER | VMCPU_FF_PDM_CRITSECT))) )
    {
        if (!RTThreadPreemptIsPending(NIL_RTTHREAD))
        {
            pVCpu->hm.s.Event.fPending = false;

            /*
             * We've injected any pending events. This is really the point of no return (to ring-3).
             *
             * Note! The caller expects to continue with interrupts & longjmps disabled on successful
             * returns from this function, so don't enable them here.
             */
            return VINF_SUCCESS;
        }

        STAM_COUNTER_INC(&pVCpu->hm.s.StatPendingHostIrq);
        rcStrict = VINF_EM_RAW_INTERRUPT;
    }
    else
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHmToR3FF);
        rcStrict = VINF_EM_RAW_TO_R3;
    }

    ASMSetFlags(pVmxTransient->fEFlags);
    VMMRZCallRing3Enable(pVCpu);

    return rcStrict;
}


/**
 * Prepares to run guest code in VT-x and we've committed to doing so. This
 * means there is no backing out to ring-3 or anywhere else at this
 * point.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks Called with preemption disabled.
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxPreRunGuestCommitted(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(VMMR0IsLogFlushDisabled(pVCpu));
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Indicate start of guest execution and where poking EMT out of guest-context is recognized.
     */
    VMCPU_ASSERT_STATE(pVCpu, VMCPUSTATE_STARTED_HM);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (!CPUMIsGuestFPUStateActive(pVCpu))
    {
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatLoadGuestFpuState, x);
        if (CPUMR0LoadGuestFPU(pVM, pVCpu) == VINF_CPUM_HOST_CR0_MODIFIED)
            pVCpu->hm.s.fCtxChanged |= HM_CHANGED_HOST_CONTEXT;
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatLoadGuestFpuState, x);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatLoadGuestFpu);
    }

    /*
     * Lazy-update of the host MSRs values in the auto-load/store MSR area.
     */
    if (   !pVCpu->hm.s.vmx.fUpdatedHostMsrs
        && pVCpu->hm.s.vmx.cMsrs > 0)
        hmR0VmxUpdateAutoLoadStoreHostMsrs(pVCpu);

    /*
     * Re-save the host state bits as we may've been preempted (only happens when
     * thread-context hooks are used or when hmR0VmxSetupVMRunHandler() changes pfnStartVM).
     * Note that the 64-on-32 switcher saves the (64-bit) host state into the VMCS and
     * if we change the switcher back to 32-bit, we *must* save the 32-bit host state here.
     * See @bugref{8432}.
     */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_HOST_CONTEXT)
    {
        int rc = hmR0VmxExportHostState(pVCpu);
        AssertRC(rc);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchPreemptExportHostState);
    }
    Assert(!(pVCpu->hm.s.fCtxChanged & HM_CHANGED_HOST_CONTEXT));

    /*
     * Export the state shared between host and guest (FPU, debug, lazy MSRs).
     */
    if (pVCpu->hm.s.fCtxChanged & HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE)
        hmR0VmxExportSharedState(pVCpu);
    AssertMsg(!pVCpu->hm.s.fCtxChanged, ("fCtxChanged=%#RX64\n", pVCpu->hm.s.fCtxChanged));

    /* Store status of the shared guest-host state at the time of VM-entry. */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
    if (CPUMIsGuestInLongModeEx(&pVCpu->cpum.GstCtx))
    {
        pVmxTransient->fWasGuestDebugStateActive = CPUMIsGuestDebugStateActivePending(pVCpu);
        pVmxTransient->fWasHyperDebugStateActive = CPUMIsHyperDebugStateActivePending(pVCpu);
    }
    else
#endif
    {
        pVmxTransient->fWasGuestDebugStateActive = CPUMIsGuestDebugStateActive(pVCpu);
        pVmxTransient->fWasHyperDebugStateActive = CPUMIsHyperDebugStateActive(pVCpu);
    }

    /*
     * Cache the TPR-shadow for checking on every VM-exit if it might have changed.
     */
    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
        pVmxTransient->u8GuestTpr = pVCpu->hm.s.vmx.pbVirtApic[XAPIC_OFF_TPR];

    PHMGLOBALCPUINFO pCpu = hmR0GetCurrentCpu();
    RTCPUID  idCurrentCpu = pCpu->idCpu;
    if (   pVmxTransient->fUpdateTscOffsettingAndPreemptTimer
        || idCurrentCpu != pVCpu->hm.s.idLastCpu)
    {
        hmR0VmxUpdateTscOffsettingAndPreemptTimer(pVCpu);
        pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = false;
    }

    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, true);    /* Used for TLB flushing, set this across the world switch. */
    hmR0VmxFlushTaggedTlb(pVCpu, pCpu);                         /* Invalidate the appropriate guest entries from the TLB. */
    Assert(idCurrentCpu == pVCpu->hm.s.idLastCpu);
    pVCpu->hm.s.vmx.LastError.idCurrentCpu = idCurrentCpu;      /* Update the error reporting info. with the current host CPU. */

    STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatEntry, &pVCpu->hm.s.StatInGC, x);

    TMNotifyStartOfExecution(pVCpu);                            /* Finally, notify TM to resume its clocks as we're about
                                                                   to start executing. */

    /*
     * Load the TSC_AUX MSR when we are not intercepting RDTSCP.
     */
    if (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
    {
        if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT))
        {
            bool fMsrUpdated;
            hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_TSC_AUX);
            int rc2 = hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_K8_TSC_AUX, CPUMGetGuestTscAux(pVCpu), true /* fUpdateHostMsr */,
                                             &fMsrUpdated);
            AssertRC(rc2);
            Assert(fMsrUpdated || pVCpu->hm.s.vmx.fUpdatedHostMsrs);
            /* Finally, mark that all host MSR values are updated so we don't redo it without leaving VT-x. See @bugref{6956}. */
            pVCpu->hm.s.vmx.fUpdatedHostMsrs = true;
        }
        else
        {
            hmR0VmxRemoveAutoLoadStoreMsr(pVCpu, MSR_K8_TSC_AUX);
            Assert(!pVCpu->hm.s.vmx.cMsrs || pVCpu->hm.s.vmx.fUpdatedHostMsrs);
        }
    }

    if (pVM->cpum.ro.GuestFeatures.fIbrs)
    {
        bool fMsrUpdated;
        hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_OTHER_MSRS);
        int rc2 = hmR0VmxAddAutoLoadStoreMsr(pVCpu, MSR_IA32_SPEC_CTRL, CPUMGetGuestSpecCtrl(pVCpu), true /* fUpdateHostMsr */,
                                             &fMsrUpdated);
        AssertRC(rc2);
        Assert(fMsrUpdated || pVCpu->hm.s.vmx.fUpdatedHostMsrs);
        /* Finally, mark that all host MSR values are updated so we don't redo it without leaving VT-x. See @bugref{6956}. */
        pVCpu->hm.s.vmx.fUpdatedHostMsrs = true;
    }

#ifdef VBOX_STRICT
    hmR0VmxCheckAutoLoadStoreMsrs(pVCpu);
    hmR0VmxCheckHostEferMsr(pVCpu);
    AssertRC(hmR0VmxCheckVmcsCtls(pVCpu));
#endif
#ifdef HMVMX_ALWAYS_CHECK_GUEST_STATE
    if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS))
    {
        uint32_t uInvalidReason = hmR0VmxCheckGuestState(pVCpu);
        if (uInvalidReason != VMX_IGS_REASON_NOT_FOUND)
            Log4(("hmR0VmxCheckGuestState returned %#x\n", uInvalidReason));
    }
#endif
}


/**
 * Performs some essential restoration of state after running guest code in
 * VT-x.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 * @param   rcVMRun         Return code of VMLAUNCH/VMRESUME.
 *
 * @remarks Called with interrupts disabled, and returns with interrupts enabled!
 *
 * @remarks No-long-jump zone!!! This function will however re-enable longjmps
 *          unconditionally when it is safe to do so.
 */
static void hmR0VmxPostRunGuest(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient, int rcVMRun)
{
    uint64_t const uHostTsc = ASMReadTSC();
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));

    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, false);   /* See HMInvalidatePageOnAllVCpus(): used for TLB flushing. */
    ASMAtomicIncU32(&pVCpu->hm.s.cWorldSwitchExits);            /* Initialized in vmR3CreateUVM(): used for EMT poking. */
    pVCpu->hm.s.fCtxChanged            = 0;                     /* Exits/longjmps to ring-3 requires saving the guest state. */
    pVmxTransient->fVmcsFieldsRead     = 0;                     /* Transient fields need to be read from the VMCS. */
    pVmxTransient->fVectoringPF        = false;                 /* Vectoring page-fault needs to be determined later. */
    pVmxTransient->fVectoringDoublePF  = false;                 /* Vectoring double page-fault needs to be determined later. */

    if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT))
        TMCpuTickSetLastSeen(pVCpu, uHostTsc + pVCpu->hm.s.vmx.u64TscOffset);

    STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatInGC, &pVCpu->hm.s.StatPreExit, x);
    TMNotifyEndOfExecution(pVCpu);                                    /* Notify TM that the guest is no longer running. */
    Assert(!ASMIntAreEnabled());
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_HM);

#if HC_ARCH_BITS == 64
    pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_REQUIRED;   /* Host state messed up by VT-x, we must restore. */
#endif
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    /* The 64-on-32 switcher maintains uVmcsState on its own and we need to leave it alone here. */
    if (pVCpu->hm.s.vmx.pfnStartVM != VMXR0SwitcherStartVM64)
        pVCpu->hm.s.vmx.uVmcsState |= HMVMX_VMCS_STATE_LAUNCHED;      /* Use VMRESUME instead of VMLAUNCH in the next run. */
#else
    pVCpu->hm.s.vmx.uVmcsState |= HMVMX_VMCS_STATE_LAUNCHED;          /* Use VMRESUME instead of VMLAUNCH in the next run. */
#endif
#ifdef VBOX_STRICT
    hmR0VmxCheckHostEferMsr(pVCpu);                                   /* Verify that VMRUN/VMLAUNCH didn't modify host EFER. */
#endif
    ASMSetFlags(pVmxTransient->fEFlags);                              /* Enable interrupts. */

    /* Save the basic VM-exit reason. Refer Intel spec. 24.9.1 "Basic VM-exit Information". */
    uint32_t uExitReason;
    int rc  = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_REASON, &uExitReason);
    rc     |= hmR0VmxReadEntryIntInfoVmcs(pVmxTransient);
    AssertRC(rc);
    pVmxTransient->uExitReason    = (uint16_t)VMX_EXIT_REASON_BASIC(uExitReason);
    pVmxTransient->fVMEntryFailed = VMX_ENTRY_INTERRUPTION_INFO_IS_VALID(pVmxTransient->uEntryIntInfo);

    if (rcVMRun == VINF_SUCCESS)
    {
        /*
         * Update the VM-exit history array here even if the VM-entry failed due to:
         *   - Invalid guest state.
         *   - MSR loading.
         *   - Machine-check event.
         *
         * In any of the above cases we will still have a "valid" VM-exit reason
         * despite @a fVMEntryFailed being false.
         *
         * See Intel spec. 26.7 "VM-Entry failures during or after loading guest state".
         *
         * Note! We don't have CS or RIP at this point.  Will probably address that later
         *       by amending the history entry added here.
         */
        EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_VMX, pVmxTransient->uExitReason & EMEXIT_F_TYPE_MASK),
                         UINT64_MAX, uHostTsc);

        if (!pVmxTransient->fVMEntryFailed)
        {
            VMMRZCallRing3Enable(pVCpu);

            Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
            Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));

#if defined(HMVMX_ALWAYS_SYNC_FULL_GUEST_STATE) || defined(HMVMX_ALWAYS_SAVE_FULL_GUEST_STATE)
            rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
            AssertRC(rc);
#elif defined(HMVMX_ALWAYS_SAVE_GUEST_RFLAGS)
            rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_RFLAGS);
            AssertRC(rc);
#else
            /*
             * Import the guest-interruptibility state always as we need it while evaluating
             * injecting events on re-entry.
             *
             * We don't import CR0 (when Unrestricted guest execution is unavailable) despite
             * checking for real-mode while exporting the state because all bits that cause
             * mode changes wrt CR0 are intercepted.
             */
            rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_HM_VMX_INT_STATE);
            AssertRC(rc);
#endif

            /*
             * Sync the TPR shadow with our APIC state.
             */
            if (   (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
                && pVmxTransient->u8GuestTpr != pVCpu->hm.s.vmx.pbVirtApic[XAPIC_OFF_TPR])
            {
                rc = APICSetTpr(pVCpu, pVCpu->hm.s.vmx.pbVirtApic[XAPIC_OFF_TPR]);
                AssertRC(rc);
                ASMAtomicOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
            }

            return;
        }
    }
    else
        Log4Func(("VM-entry failure: rcVMRun=%Rrc fVMEntryFailed=%RTbool\n", rcVMRun, pVmxTransient->fVMEntryFailed));

    VMMRZCallRing3Enable(pVCpu);
}


/**
 * Runs the guest code using VT-x the normal way.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @note    Mostly the same as hmR0VmxRunGuestCodeStep().
 */
static VBOXSTRICTRC hmR0VmxRunGuestCodeNormal(PVMCPU pVCpu)
{
    VMXTRANSIENT VmxTransient;
    VmxTransient.fUpdateTscOffsettingAndPreemptTimer = true;
    VBOXSTRICTRC rcStrict = VERR_INTERNAL_ERROR_5;
    uint32_t     cLoops = 0;

    for (;; cLoops++)
    {
        Assert(!HMR0SuspendPending());
        HMVMX_ASSERT_CPU_SAFE(pVCpu);

        /* Preparatory work for running guest code, this may force us to return
           to ring-3.  This bugger disables interrupts on VINF_SUCCESS! */
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatEntry, x);
        rcStrict = hmR0VmxPreRunGuest(pVCpu, &VmxTransient, false /* fStepping */);
        if (rcStrict != VINF_SUCCESS)
            break;

        hmR0VmxPreRunGuestCommitted(pVCpu, &VmxTransient);
        int rcRun = hmR0VmxRunGuest(pVCpu);

        /* Restore any residual host-state and save any bits shared between host
           and guest into the guest-CPU state.  Re-enables interrupts! */
        hmR0VmxPostRunGuest(pVCpu, &VmxTransient, rcRun);

        /* Check for errors with running the VM (VMLAUNCH/VMRESUME). */
        if (RT_SUCCESS(rcRun))
        { /* very likely */ }
        else
        {
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatPreExit, x);
            hmR0VmxReportWorldSwitchError(pVCpu, rcRun, &VmxTransient);
            return rcRun;
        }

        /* Profile the VM-exit. */
        AssertMsg(VmxTransient.uExitReason <= VMX_EXIT_MAX, ("%#x\n", VmxTransient.uExitReason));
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitAll);
        STAM_COUNTER_INC(&pVCpu->hm.s.paStatExitReasonR0[VmxTransient.uExitReason & MASK_EXITREASON_STAT]);
        STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatPreExit, &pVCpu->hm.s.StatExitHandling, x);
        HMVMX_START_EXIT_DISPATCH_PROF();

        VBOXVMM_R0_HMVMX_VMEXIT_NOCTX(pVCpu, &pVCpu->cpum.GstCtx, VmxTransient.uExitReason);

        /* Handle the VM-exit. */
#ifdef HMVMX_USE_FUNCTION_TABLE
        rcStrict = g_apfnVMExitHandlers[VmxTransient.uExitReason](pVCpu, &VmxTransient);
#else
        rcStrict = hmR0VmxHandleExit(pVCpu, &VmxTransient, VmxTransient.uExitReason);
#endif
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitHandling, x);
        if (rcStrict == VINF_SUCCESS)
        {
            if (cLoops <= pVCpu->CTX_SUFF(pVM)->hm.s.cMaxResumeLoops)
                continue; /* likely */
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchMaxResumeLoops);
            rcStrict = VINF_EM_RAW_INTERRUPT;
        }
        break;
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    return rcStrict;
}



/** @name Execution loop for single stepping, DBGF events and expensive Dtrace
 *  probes.
 *
 * The following few functions and associated structure contains the bloat
 * necessary for providing detailed debug events and dtrace probes as well as
 * reliable host side single stepping.  This works on the principle of
 * "subclassing" the normal execution loop and workers.  We replace the loop
 * method completely and override selected helpers to add necessary adjustments
 * to their core operation.
 *
 * The goal is to keep the "parent" code lean and mean, so as not to sacrifice
 * any performance for debug and analysis features.
 *
 * @{
 */

/**
 * Transient per-VCPU debug state of VMCS and related info. we save/restore in
 * the debug run loop.
 */
typedef struct VMXRUNDBGSTATE
{
    /** The RIP we started executing at.  This is for detecting that we stepped.  */
    uint64_t    uRipStart;
    /** The CS we started executing with.  */
    uint16_t    uCsStart;

    /** Whether we've actually modified the 1st execution control field. */
    bool        fModifiedProcCtls : 1;
    /** Whether we've actually modified the 2nd execution control field. */
    bool        fModifiedProcCtls2 : 1;
    /** Whether we've actually modified the exception bitmap. */
    bool        fModifiedXcptBitmap : 1;

    /** We desire the modified the CR0 mask to be cleared. */
    bool        fClearCr0Mask : 1;
    /** We desire the modified the CR4 mask to be cleared. */
    bool        fClearCr4Mask : 1;
    /** Stuff we need in VMX_VMCS32_CTRL_PROC_EXEC. */
    uint32_t    fCpe1Extra;
    /** Stuff we do not want in VMX_VMCS32_CTRL_PROC_EXEC. */
    uint32_t    fCpe1Unwanted;
    /** Stuff we need in VMX_VMCS32_CTRL_PROC_EXEC2. */
    uint32_t    fCpe2Extra;
    /** Extra stuff we need in VMX_VMCS32_CTRL_EXCEPTION_BITMAP. */
    uint32_t    bmXcptExtra;
    /** The sequence number of the Dtrace provider settings the state was
     *  configured against. */
    uint32_t    uDtraceSettingsSeqNo;
    /** VM-exits to check (one bit per VM-exit). */
    uint32_t    bmExitsToCheck[3];

    /** The initial VMX_VMCS32_CTRL_PROC_EXEC value (helps with restore). */
    uint32_t    fProcCtlsInitial;
    /** The initial VMX_VMCS32_CTRL_PROC_EXEC2 value (helps with restore). */
    uint32_t    fProcCtls2Initial;
    /** The initial VMX_VMCS32_CTRL_EXCEPTION_BITMAP value (helps with restore). */
    uint32_t    bmXcptInitial;
} VMXRUNDBGSTATE;
AssertCompileMemberSize(VMXRUNDBGSTATE, bmExitsToCheck, (VMX_EXIT_MAX + 1 + 31) / 32 * 4);
typedef VMXRUNDBGSTATE *PVMXRUNDBGSTATE;


/**
 * Initializes the VMXRUNDBGSTATE structure.
 *
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   pDbgState       The structure to initialize.
 */
static void hmR0VmxRunDebugStateInit(PVMCPU pVCpu, PVMXRUNDBGSTATE pDbgState)
{
    pDbgState->uRipStart            = pVCpu->cpum.GstCtx.rip;
    pDbgState->uCsStart             = pVCpu->cpum.GstCtx.cs.Sel;

    pDbgState->fModifiedProcCtls    = false;
    pDbgState->fModifiedProcCtls2   = false;
    pDbgState->fModifiedXcptBitmap  = false;
    pDbgState->fClearCr0Mask        = false;
    pDbgState->fClearCr4Mask        = false;
    pDbgState->fCpe1Extra           = 0;
    pDbgState->fCpe1Unwanted        = 0;
    pDbgState->fCpe2Extra           = 0;
    pDbgState->bmXcptExtra          = 0;
    pDbgState->fProcCtlsInitial     = pVCpu->hm.s.vmx.u32ProcCtls;
    pDbgState->fProcCtls2Initial    = pVCpu->hm.s.vmx.u32ProcCtls2;
    pDbgState->bmXcptInitial        = pVCpu->hm.s.vmx.u32XcptBitmap;
}


/**
 * Updates the VMSC fields with changes requested by @a pDbgState.
 *
 * This is performed after hmR0VmxPreRunGuestDebugStateUpdate as well
 * immediately before executing guest code, i.e. when interrupts are disabled.
 * We don't check status codes here as we cannot easily assert or return in the
 * latter case.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pDbgState   The debug state.
 */
static void hmR0VmxPreRunGuestDebugStateApply(PVMCPU pVCpu, PVMXRUNDBGSTATE pDbgState)
{
    /*
     * Ensure desired flags in VMCS control fields are set.
     * (Ignoring write failure here, as we're committed and it's just debug extras.)
     *
     * Note! We load the shadow CR0 & CR4 bits when we flag the clearing, so
     *       there should be no stale data in pCtx at this point.
     */
    if (   (pVCpu->hm.s.vmx.u32ProcCtls & pDbgState->fCpe1Extra) != pDbgState->fCpe1Extra
        || (pVCpu->hm.s.vmx.u32ProcCtls & pDbgState->fCpe1Unwanted))
    {
        pVCpu->hm.s.vmx.u32ProcCtls   |= pDbgState->fCpe1Extra;
        pVCpu->hm.s.vmx.u32ProcCtls   &= ~pDbgState->fCpe1Unwanted;
        VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
        Log6Func(("VMX_VMCS32_CTRL_PROC_EXEC: %#RX32\n", pVCpu->hm.s.vmx.u32ProcCtls));
        pDbgState->fModifiedProcCtls   = true;
    }

    if ((pVCpu->hm.s.vmx.u32ProcCtls2 & pDbgState->fCpe2Extra) != pDbgState->fCpe2Extra)
    {
        pVCpu->hm.s.vmx.u32ProcCtls2  |= pDbgState->fCpe2Extra;
        VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, pVCpu->hm.s.vmx.u32ProcCtls2);
        Log6Func(("VMX_VMCS32_CTRL_PROC_EXEC2: %#RX32\n", pVCpu->hm.s.vmx.u32ProcCtls2));
        pDbgState->fModifiedProcCtls2  = true;
    }

    if ((pVCpu->hm.s.vmx.u32XcptBitmap & pDbgState->bmXcptExtra) != pDbgState->bmXcptExtra)
    {
        pVCpu->hm.s.vmx.u32XcptBitmap |= pDbgState->bmXcptExtra;
        VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, pVCpu->hm.s.vmx.u32XcptBitmap);
        Log6Func(("VMX_VMCS32_CTRL_EXCEPTION_BITMAP: %#RX32\n", pVCpu->hm.s.vmx.u32XcptBitmap));
        pDbgState->fModifiedXcptBitmap = true;
    }

    if (pDbgState->fClearCr0Mask && pVCpu->hm.s.vmx.u32Cr0Mask != 0)
    {
        pVCpu->hm.s.vmx.u32Cr0Mask = 0;
        VMXWriteVmcs32(VMX_VMCS_CTRL_CR0_MASK, 0);
        Log6Func(("VMX_VMCS_CTRL_CR0_MASK: 0\n"));
    }

    if (pDbgState->fClearCr4Mask && pVCpu->hm.s.vmx.u32Cr4Mask != 0)
    {
        pVCpu->hm.s.vmx.u32Cr4Mask = 0;
        VMXWriteVmcs32(VMX_VMCS_CTRL_CR4_MASK, 0);
        Log6Func(("VMX_VMCS_CTRL_CR4_MASK: 0\n"));
    }
}


/**
 * Restores VMCS fields that were changed by hmR0VmxPreRunGuestDebugStateApply for
 * re-entry next time around.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pDbgState   The debug state.
 * @param   rcStrict    The return code from executing the guest using single
 *                      stepping.
 */
static VBOXSTRICTRC hmR0VmxRunDebugStateRevert(PVMCPU pVCpu, PVMXRUNDBGSTATE pDbgState, VBOXSTRICTRC rcStrict)
{
    /*
     * Restore VM-exit control settings as we may not reenter this function the
     * next time around.
     */
    /* We reload the initial value, trigger what we can of recalculations the
       next time around.  From the looks of things, that's all that's required atm. */
    if (pDbgState->fModifiedProcCtls)
    {
        if (!(pDbgState->fProcCtlsInitial & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT) && CPUMIsHyperDebugStateActive(pVCpu))
            pDbgState->fProcCtlsInitial |= VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT; /* Avoid assertion in hmR0VmxLeave */
        int rc2 = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pDbgState->fProcCtlsInitial);
        AssertRCReturn(rc2, rc2);
        pVCpu->hm.s.vmx.u32ProcCtls = pDbgState->fProcCtlsInitial;
    }

    /* We're currently the only ones messing with this one, so just restore the
       cached value and reload the field. */
    if (   pDbgState->fModifiedProcCtls2
        && pVCpu->hm.s.vmx.u32ProcCtls2 != pDbgState->fProcCtls2Initial)
    {
        int rc2 = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, pDbgState->fProcCtls2Initial);
        AssertRCReturn(rc2, rc2);
        pVCpu->hm.s.vmx.u32ProcCtls2 = pDbgState->fProcCtls2Initial;
    }

    /* If we've modified the exception bitmap, we restore it and trigger
       reloading and partial recalculation the next time around. */
    if (pDbgState->fModifiedXcptBitmap)
        pVCpu->hm.s.vmx.u32XcptBitmap = pDbgState->bmXcptInitial;

    return rcStrict;
}


/**
 * Configures VM-exit controls for current DBGF and DTrace settings.
 *
 * This updates @a pDbgState and the VMCS execution control fields to reflect
 * the necessary VM-exits demanded by DBGF and DTrace.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pDbgState       The debug state.
 * @param   pVmxTransient   Pointer to the VMX transient structure.  May update
 *                          fUpdateTscOffsettingAndPreemptTimer.
 */
static void hmR0VmxPreRunGuestDebugStateUpdate(PVMCPU pVCpu, PVMXRUNDBGSTATE pDbgState, PVMXTRANSIENT pVmxTransient)
{
    /*
     * Take down the dtrace serial number so we can spot changes.
     */
    pDbgState->uDtraceSettingsSeqNo = VBOXVMM_GET_SETTINGS_SEQ_NO();
    ASMCompilerBarrier();

    /*
     * We'll rebuild most of the middle block of data members (holding the
     * current settings) as we go along here, so start by clearing it all.
     */
    pDbgState->bmXcptExtra      = 0;
    pDbgState->fCpe1Extra       = 0;
    pDbgState->fCpe1Unwanted    = 0;
    pDbgState->fCpe2Extra       = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(pDbgState->bmExitsToCheck); i++)
        pDbgState->bmExitsToCheck[i] = 0;

    /*
     * Software interrupts (INT XXh) - no idea how to trigger these...
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (   DBGF_IS_EVENT_ENABLED(pVM, DBGFEVENT_INTERRUPT_SOFTWARE)
        || VBOXVMM_INT_SOFTWARE_ENABLED())
    {
        ASMBitSet(pDbgState->bmExitsToCheck, VMX_EXIT_XCPT_OR_NMI);
    }

    /*
     * INT3 breakpoints - triggered by #BP exceptions.
     */
    if (pVM->dbgf.ro.cEnabledInt3Breakpoints > 0)
        pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_BP);

    /*
     * Exception bitmap and XCPT events+probes.
     */
    for (int iXcpt = 0; iXcpt < (DBGFEVENT_XCPT_LAST - DBGFEVENT_XCPT_FIRST + 1); iXcpt++)
        if (DBGF_IS_EVENT_ENABLED(pVM, (DBGFEVENTTYPE)(DBGFEVENT_XCPT_FIRST + iXcpt)))
            pDbgState->bmXcptExtra |= RT_BIT_32(iXcpt);

    if (VBOXVMM_XCPT_DE_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_DE);
    if (VBOXVMM_XCPT_DB_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_DB);
    if (VBOXVMM_XCPT_BP_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_BP);
    if (VBOXVMM_XCPT_OF_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_OF);
    if (VBOXVMM_XCPT_BR_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_BR);
    if (VBOXVMM_XCPT_UD_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_UD);
    if (VBOXVMM_XCPT_NM_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_NM);
    if (VBOXVMM_XCPT_DF_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_DF);
    if (VBOXVMM_XCPT_TS_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_TS);
    if (VBOXVMM_XCPT_NP_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_NP);
    if (VBOXVMM_XCPT_SS_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_SS);
    if (VBOXVMM_XCPT_GP_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_GP);
    if (VBOXVMM_XCPT_PF_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_PF);
    if (VBOXVMM_XCPT_MF_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_MF);
    if (VBOXVMM_XCPT_AC_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_AC);
    if (VBOXVMM_XCPT_XF_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_XF);
    if (VBOXVMM_XCPT_VE_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_VE);
    if (VBOXVMM_XCPT_SX_ENABLED())  pDbgState->bmXcptExtra |= RT_BIT_32(X86_XCPT_SX);

    if (pDbgState->bmXcptExtra)
        ASMBitSet(pDbgState->bmExitsToCheck, VMX_EXIT_XCPT_OR_NMI);

    /*
     * Process events and probes for VM-exits, making sure we get the wanted VM-exits.
     *
     * Note! This is the reverse of what hmR0VmxHandleExitDtraceEvents does.
     *       So, when adding/changing/removing please don't forget to update it.
     *
     * Some of the macros are picking up local variables to save horizontal space,
     * (being able to see it in a table is the lesser evil here).
     */
#define IS_EITHER_ENABLED(a_pVM, a_EventSubName) \
        (    DBGF_IS_EVENT_ENABLED(a_pVM, RT_CONCAT(DBGFEVENT_, a_EventSubName)) \
         ||  RT_CONCAT3(VBOXVMM_, a_EventSubName, _ENABLED)() )
#define SET_ONLY_XBM_IF_EITHER_EN(a_EventSubName, a_uExit) \
        if (IS_EITHER_ENABLED(pVM, a_EventSubName)) \
        {   AssertCompile((unsigned)(a_uExit) < sizeof(pDbgState->bmExitsToCheck) * 8); \
            ASMBitSet((pDbgState)->bmExitsToCheck, a_uExit); \
        } else do { } while (0)
#define SET_CPE1_XBM_IF_EITHER_EN(a_EventSubName, a_uExit, a_fCtrlProcExec) \
        if (IS_EITHER_ENABLED(pVM, a_EventSubName)) \
        { \
            (pDbgState)->fCpe1Extra |= (a_fCtrlProcExec); \
            AssertCompile((unsigned)(a_uExit) < sizeof(pDbgState->bmExitsToCheck) * 8); \
            ASMBitSet((pDbgState)->bmExitsToCheck, a_uExit); \
        } else do { } while (0)
#define SET_CPEU_XBM_IF_EITHER_EN(a_EventSubName, a_uExit, a_fUnwantedCtrlProcExec) \
        if (IS_EITHER_ENABLED(pVM, a_EventSubName)) \
        { \
            (pDbgState)->fCpe1Unwanted |= (a_fUnwantedCtrlProcExec); \
            AssertCompile((unsigned)(a_uExit) < sizeof(pDbgState->bmExitsToCheck) * 8); \
            ASMBitSet((pDbgState)->bmExitsToCheck, a_uExit); \
        } else do { } while (0)
#define SET_CPE2_XBM_IF_EITHER_EN(a_EventSubName, a_uExit, a_fCtrlProcExec2) \
        if (IS_EITHER_ENABLED(pVM, a_EventSubName)) \
        { \
            (pDbgState)->fCpe2Extra |= (a_fCtrlProcExec2); \
            AssertCompile((unsigned)(a_uExit) < sizeof(pDbgState->bmExitsToCheck) * 8); \
            ASMBitSet((pDbgState)->bmExitsToCheck, a_uExit); \
        } else do { } while (0)

    SET_ONLY_XBM_IF_EITHER_EN(EXIT_TASK_SWITCH,         VMX_EXIT_TASK_SWITCH);      /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN(EXIT_VMX_EPT_VIOLATION,   VMX_EXIT_EPT_VIOLATION);    /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN(EXIT_VMX_EPT_MISCONFIG,   VMX_EXIT_EPT_MISCONFIG);    /* unconditional (unless #VE) */
    SET_ONLY_XBM_IF_EITHER_EN(EXIT_VMX_VAPIC_ACCESS,    VMX_EXIT_APIC_ACCESS);      /* feature dependent, nothing to enable here */
    SET_ONLY_XBM_IF_EITHER_EN(EXIT_VMX_VAPIC_WRITE,     VMX_EXIT_APIC_WRITE);       /* feature dependent, nothing to enable here */

    SET_ONLY_XBM_IF_EITHER_EN(INSTR_CPUID,              VMX_EXIT_CPUID);            /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_CPUID,              VMX_EXIT_CPUID);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_GETSEC,             VMX_EXIT_GETSEC);           /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_GETSEC,             VMX_EXIT_GETSEC);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_HALT,               VMX_EXIT_HLT,      VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT); /* paranoia */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_HALT,               VMX_EXIT_HLT);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_INVD,               VMX_EXIT_INVD);             /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_INVD,               VMX_EXIT_INVD);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_INVLPG,             VMX_EXIT_INVLPG,   VMX_VMCS_CTRL_PROC_EXEC_INVLPG_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_INVLPG,             VMX_EXIT_INVLPG);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_RDPMC,              VMX_EXIT_RDPMC,    VMX_VMCS_CTRL_PROC_EXEC_RDPMC_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RDPMC,              VMX_EXIT_RDPMC);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_RDTSC,              VMX_EXIT_RDTSC,    VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RDTSC,              VMX_EXIT_RDTSC);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_RSM,                VMX_EXIT_RSM);              /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RSM,                VMX_EXIT_RSM);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMM_CALL,           VMX_EXIT_VMCALL);           /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMM_CALL,           VMX_EXIT_VMCALL);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMCLEAR,        VMX_EXIT_VMCLEAR);          /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMCLEAR,        VMX_EXIT_VMCLEAR);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMLAUNCH,       VMX_EXIT_VMLAUNCH);         /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMLAUNCH,       VMX_EXIT_VMLAUNCH);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMPTRLD,        VMX_EXIT_VMPTRLD);          /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMPTRLD,        VMX_EXIT_VMPTRLD);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMPTRST,        VMX_EXIT_VMPTRST);          /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMPTRST,        VMX_EXIT_VMPTRST);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMREAD,         VMX_EXIT_VMREAD);           /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMREAD,         VMX_EXIT_VMREAD);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMRESUME,       VMX_EXIT_VMRESUME);         /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMRESUME,       VMX_EXIT_VMRESUME);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMWRITE,        VMX_EXIT_VMWRITE);          /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMWRITE,        VMX_EXIT_VMWRITE);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMXOFF,         VMX_EXIT_VMXOFF);           /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMXOFF,         VMX_EXIT_VMXOFF);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMXON,          VMX_EXIT_VMXON);            /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMXON,          VMX_EXIT_VMXON);

    if (   IS_EITHER_ENABLED(pVM, INSTR_CRX_READ)
        || IS_EITHER_ENABLED(pVM, INSTR_CRX_WRITE))
    {
        int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR4 | CPUMCTX_EXTRN_APIC_TPR);
        AssertRC(rc);

#if 0 /** @todo fix me */
        pDbgState->fClearCr0Mask = true;
        pDbgState->fClearCr4Mask = true;
#endif
        if (IS_EITHER_ENABLED(pVM, INSTR_CRX_READ))
            pDbgState->fCpe1Extra |= VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT | VMX_VMCS_CTRL_PROC_EXEC_CR8_STORE_EXIT;
        if (IS_EITHER_ENABLED(pVM, INSTR_CRX_WRITE))
            pDbgState->fCpe1Extra |= VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT | VMX_VMCS_CTRL_PROC_EXEC_CR8_LOAD_EXIT;
        pDbgState->fCpe1Unwanted |= VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW; /* risky? */
        /* Note! We currently don't use VMX_VMCS32_CTRL_CR3_TARGET_COUNT.  It would
                 require clearing here and in the loop if we start using it. */
        ASMBitSet(pDbgState->bmExitsToCheck, VMX_EXIT_MOV_CRX);
    }
    else
    {
        if (pDbgState->fClearCr0Mask)
        {
            pDbgState->fClearCr0Mask = false;
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR0);
        }
        if (pDbgState->fClearCr4Mask)
        {
            pDbgState->fClearCr4Mask = false;
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_CR4);
        }
    }
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_CRX_READ,           VMX_EXIT_MOV_CRX);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_CRX_WRITE,          VMX_EXIT_MOV_CRX);

    if (   IS_EITHER_ENABLED(pVM, INSTR_DRX_READ)
        || IS_EITHER_ENABLED(pVM, INSTR_DRX_WRITE))
    {
        /** @todo later, need to fix handler as it assumes this won't usually happen. */
        ASMBitSet(pDbgState->bmExitsToCheck, VMX_EXIT_MOV_DRX);
    }
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_DRX_READ,           VMX_EXIT_MOV_DRX);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_DRX_WRITE,          VMX_EXIT_MOV_DRX);

    SET_CPEU_XBM_IF_EITHER_EN(INSTR_RDMSR,               VMX_EXIT_RDMSR,     VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS); /* risky clearing this? */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RDMSR,               VMX_EXIT_RDMSR);
    SET_CPEU_XBM_IF_EITHER_EN(INSTR_WRMSR,               VMX_EXIT_WRMSR,     VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_WRMSR,               VMX_EXIT_WRMSR);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_MWAIT,               VMX_EXIT_MWAIT,     VMX_VMCS_CTRL_PROC_EXEC_MWAIT_EXIT);   /* paranoia */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_MWAIT,               VMX_EXIT_MWAIT);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_MONITOR,             VMX_EXIT_MONITOR,   VMX_VMCS_CTRL_PROC_EXEC_MONITOR_EXIT); /* paranoia */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_MONITOR,             VMX_EXIT_MONITOR);
#if 0 /** @todo too slow, fix handler. */
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_PAUSE,               VMX_EXIT_PAUSE,     VMX_VMCS_CTRL_PROC_EXEC_PAUSE_EXIT);
#endif
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_PAUSE,               VMX_EXIT_PAUSE);

    if (   IS_EITHER_ENABLED(pVM, INSTR_SGDT)
        || IS_EITHER_ENABLED(pVM, INSTR_SIDT)
        || IS_EITHER_ENABLED(pVM, INSTR_LGDT)
        || IS_EITHER_ENABLED(pVM, INSTR_LIDT))
    {
        pDbgState->fCpe2Extra |= VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT;
        ASMBitSet(pDbgState->bmExitsToCheck, VMX_EXIT_XDTR_ACCESS);
    }
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_SGDT,               VMX_EXIT_XDTR_ACCESS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_SIDT,               VMX_EXIT_XDTR_ACCESS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_LGDT,               VMX_EXIT_XDTR_ACCESS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_LIDT,               VMX_EXIT_XDTR_ACCESS);

    if (   IS_EITHER_ENABLED(pVM, INSTR_SLDT)
        || IS_EITHER_ENABLED(pVM, INSTR_STR)
        || IS_EITHER_ENABLED(pVM, INSTR_LLDT)
        || IS_EITHER_ENABLED(pVM, INSTR_LTR))
    {
        pDbgState->fCpe2Extra |= VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT;
        ASMBitSet(pDbgState->bmExitsToCheck, VMX_EXIT_TR_ACCESS);
    }
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_SLDT,               VMX_EXIT_TR_ACCESS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_STR,                VMX_EXIT_TR_ACCESS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_LLDT,               VMX_EXIT_TR_ACCESS);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_LTR,                VMX_EXIT_TR_ACCESS);

    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_INVEPT,         VMX_EXIT_INVEPT);           /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_INVEPT,         VMX_EXIT_INVEPT);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_RDTSCP,             VMX_EXIT_RDTSCP,    VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RDTSCP,             VMX_EXIT_RDTSCP);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_INVVPID,        VMX_EXIT_INVVPID);          /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_INVVPID,        VMX_EXIT_INVVPID);
    SET_CPE2_XBM_IF_EITHER_EN(INSTR_WBINVD,             VMX_EXIT_WBINVD,    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_WBINVD,             VMX_EXIT_WBINVD);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_XSETBV,             VMX_EXIT_XSETBV);           /* unconditional */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_XSETBV,             VMX_EXIT_XSETBV);
    SET_CPE2_XBM_IF_EITHER_EN(INSTR_RDRAND,             VMX_EXIT_RDRAND,    VMX_VMCS_CTRL_PROC_EXEC2_RDRAND_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RDRAND,             VMX_EXIT_RDRAND);
    SET_CPE1_XBM_IF_EITHER_EN(INSTR_VMX_INVPCID,        VMX_EXIT_INVPCID,   VMX_VMCS_CTRL_PROC_EXEC_INVLPG_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_INVPCID,        VMX_EXIT_INVPCID);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_VMX_VMFUNC,         VMX_EXIT_VMFUNC);           /* unconditional for the current setup */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_VMX_VMFUNC,         VMX_EXIT_VMFUNC);
    SET_CPE2_XBM_IF_EITHER_EN(INSTR_RDSEED,             VMX_EXIT_RDSEED,    VMX_VMCS_CTRL_PROC_EXEC2_RDSEED_EXIT);
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_RDSEED,             VMX_EXIT_RDSEED);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_XSAVES,             VMX_EXIT_XSAVES);           /* unconditional (enabled by host, guest cfg) */
    SET_ONLY_XBM_IF_EITHER_EN(EXIT_XSAVES,              VMX_EXIT_XSAVES);
    SET_ONLY_XBM_IF_EITHER_EN(INSTR_XRSTORS,            VMX_EXIT_XRSTORS);          /* unconditional (enabled by host, guest cfg) */
    SET_ONLY_XBM_IF_EITHER_EN( EXIT_XRSTORS,            VMX_EXIT_XRSTORS);

#undef IS_EITHER_ENABLED
#undef SET_ONLY_XBM_IF_EITHER_EN
#undef SET_CPE1_XBM_IF_EITHER_EN
#undef SET_CPEU_XBM_IF_EITHER_EN
#undef SET_CPE2_XBM_IF_EITHER_EN

    /*
     * Sanitize the control stuff.
     */
    pDbgState->fCpe2Extra       &= pVM->hm.s.vmx.Msrs.VmxProcCtls2.n.allowed1;
    if (pDbgState->fCpe2Extra)
        pDbgState->fCpe1Extra   |= VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL;
    pDbgState->fCpe1Extra       &= pVM->hm.s.vmx.Msrs.VmxProcCtls.n.allowed1;
    pDbgState->fCpe1Unwanted    &= ~pVM->hm.s.vmx.Msrs.VmxProcCtls.n.disallowed0;
    if (pVCpu->hm.s.fDebugWantRdTscExit != RT_BOOL(pDbgState->fCpe1Extra & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT))
    {
        pVCpu->hm.s.fDebugWantRdTscExit ^= true;
        pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;
    }

    Log6(("HM: debug state: cpe1=%#RX32 cpeu=%#RX32 cpe2=%#RX32%s%s\n",
          pDbgState->fCpe1Extra, pDbgState->fCpe1Unwanted, pDbgState->fCpe2Extra,
          pDbgState->fClearCr0Mask ? " clr-cr0" : "",
          pDbgState->fClearCr4Mask ? " clr-cr4" : ""));
}


/**
 * Fires off DBGF events and dtrace probes for a VM-exit, when it's
 * appropriate.
 *
 * The caller has checked the VM-exit against the
 * VMXRUNDBGSTATE::bmExitsToCheck bitmap. The caller has checked for NMIs
 * already, so we don't have to do that either.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX-transient structure.
 * @param   uExitReason     The VM-exit reason.
 *
 * @remarks The name of this function is displayed by dtrace, so keep it short
 *          and to the point. No longer than 33 chars long, please.
 */
static VBOXSTRICTRC hmR0VmxHandleExitDtraceEvents(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient, uint32_t uExitReason)
{
    /*
     * Translate the event into a DBGF event (enmEvent + uEventArg) and at the
     * same time check whether any corresponding Dtrace event is enabled (fDtrace).
     *
     * Note! This is the reverse operation of what hmR0VmxPreRunGuestDebugStateUpdate
     *       does.  Must add/change/remove both places.  Same ordering, please.
     *
     *       Added/removed events must also be reflected in the next section
     *       where we dispatch dtrace events.
     */
    bool            fDtrace1   = false;
    bool            fDtrace2   = false;
    DBGFEVENTTYPE   enmEvent1  = DBGFEVENT_END;
    DBGFEVENTTYPE   enmEvent2  = DBGFEVENT_END;
    uint32_t        uEventArg  = 0;
#define SET_EXIT(a_EventSubName) \
        do { \
            enmEvent2 = RT_CONCAT(DBGFEVENT_EXIT_,  a_EventSubName); \
            fDtrace2  = RT_CONCAT3(VBOXVMM_EXIT_,   a_EventSubName, _ENABLED)(); \
        } while (0)
#define SET_BOTH(a_EventSubName) \
        do { \
            enmEvent1 = RT_CONCAT(DBGFEVENT_INSTR_, a_EventSubName); \
            enmEvent2 = RT_CONCAT(DBGFEVENT_EXIT_,  a_EventSubName); \
            fDtrace1  = RT_CONCAT3(VBOXVMM_INSTR_,  a_EventSubName, _ENABLED)(); \
            fDtrace2  = RT_CONCAT3(VBOXVMM_EXIT_,   a_EventSubName, _ENABLED)(); \
        } while (0)
    switch (uExitReason)
    {
        case VMX_EXIT_MTF:
            return hmR0VmxExitMtf(pVCpu, pVmxTransient);

        case VMX_EXIT_XCPT_OR_NMI:
        {
            uint8_t const idxVector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVmxTransient->uExitIntInfo);
            switch (VMX_EXIT_INTERRUPTION_INFO_TYPE(pVmxTransient->uExitIntInfo))
            {
                case VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT:
                case VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT:
                case VMX_EXIT_INTERRUPTION_INFO_TYPE_PRIV_SW_XCPT:
                    if (idxVector <= (unsigned)(DBGFEVENT_XCPT_LAST - DBGFEVENT_XCPT_FIRST))
                    {
                        if (VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(pVmxTransient->uExitIntInfo))
                        {
                            hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
                            uEventArg = pVmxTransient->uExitIntErrorCode;
                        }
                        enmEvent1 = (DBGFEVENTTYPE)(DBGFEVENT_XCPT_FIRST + idxVector);
                        switch (enmEvent1)
                        {
                            case DBGFEVENT_XCPT_DE: fDtrace1 = VBOXVMM_XCPT_DE_ENABLED(); break;
                            case DBGFEVENT_XCPT_DB: fDtrace1 = VBOXVMM_XCPT_DB_ENABLED(); break;
                            case DBGFEVENT_XCPT_BP: fDtrace1 = VBOXVMM_XCPT_BP_ENABLED(); break;
                            case DBGFEVENT_XCPT_OF: fDtrace1 = VBOXVMM_XCPT_OF_ENABLED(); break;
                            case DBGFEVENT_XCPT_BR: fDtrace1 = VBOXVMM_XCPT_BR_ENABLED(); break;
                            case DBGFEVENT_XCPT_UD: fDtrace1 = VBOXVMM_XCPT_UD_ENABLED(); break;
                            case DBGFEVENT_XCPT_NM: fDtrace1 = VBOXVMM_XCPT_NM_ENABLED(); break;
                            case DBGFEVENT_XCPT_DF: fDtrace1 = VBOXVMM_XCPT_DF_ENABLED(); break;
                            case DBGFEVENT_XCPT_TS: fDtrace1 = VBOXVMM_XCPT_TS_ENABLED(); break;
                            case DBGFEVENT_XCPT_NP: fDtrace1 = VBOXVMM_XCPT_NP_ENABLED(); break;
                            case DBGFEVENT_XCPT_SS: fDtrace1 = VBOXVMM_XCPT_SS_ENABLED(); break;
                            case DBGFEVENT_XCPT_GP: fDtrace1 = VBOXVMM_XCPT_GP_ENABLED(); break;
                            case DBGFEVENT_XCPT_PF: fDtrace1 = VBOXVMM_XCPT_PF_ENABLED(); break;
                            case DBGFEVENT_XCPT_MF: fDtrace1 = VBOXVMM_XCPT_MF_ENABLED(); break;
                            case DBGFEVENT_XCPT_AC: fDtrace1 = VBOXVMM_XCPT_AC_ENABLED(); break;
                            case DBGFEVENT_XCPT_XF: fDtrace1 = VBOXVMM_XCPT_XF_ENABLED(); break;
                            case DBGFEVENT_XCPT_VE: fDtrace1 = VBOXVMM_XCPT_VE_ENABLED(); break;
                            case DBGFEVENT_XCPT_SX: fDtrace1 = VBOXVMM_XCPT_SX_ENABLED(); break;
                            default:                                                      break;
                        }
                    }
                    else
                        AssertFailed();
                    break;

                case VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT:
                    uEventArg = idxVector;
                    enmEvent1 = DBGFEVENT_INTERRUPT_SOFTWARE;
                    fDtrace1  = VBOXVMM_INT_SOFTWARE_ENABLED();
                    break;
            }
            break;
        }

        case VMX_EXIT_TRIPLE_FAULT:
            enmEvent1 = DBGFEVENT_TRIPLE_FAULT;
            //fDtrace1  = VBOXVMM_EXIT_TRIPLE_FAULT_ENABLED();
            break;
        case VMX_EXIT_TASK_SWITCH:      SET_EXIT(TASK_SWITCH); break;
        case VMX_EXIT_EPT_VIOLATION:    SET_EXIT(VMX_EPT_VIOLATION); break;
        case VMX_EXIT_EPT_MISCONFIG:    SET_EXIT(VMX_EPT_MISCONFIG); break;
        case VMX_EXIT_APIC_ACCESS:      SET_EXIT(VMX_VAPIC_ACCESS); break;
        case VMX_EXIT_APIC_WRITE:       SET_EXIT(VMX_VAPIC_WRITE); break;

        /* Instruction specific VM-exits: */
        case VMX_EXIT_CPUID:            SET_BOTH(CPUID); break;
        case VMX_EXIT_GETSEC:           SET_BOTH(GETSEC); break;
        case VMX_EXIT_HLT:              SET_BOTH(HALT); break;
        case VMX_EXIT_INVD:             SET_BOTH(INVD); break;
        case VMX_EXIT_INVLPG:           SET_BOTH(INVLPG); break;
        case VMX_EXIT_RDPMC:            SET_BOTH(RDPMC); break;
        case VMX_EXIT_RDTSC:            SET_BOTH(RDTSC); break;
        case VMX_EXIT_RSM:              SET_BOTH(RSM); break;
        case VMX_EXIT_VMCALL:           SET_BOTH(VMM_CALL); break;
        case VMX_EXIT_VMCLEAR:          SET_BOTH(VMX_VMCLEAR); break;
        case VMX_EXIT_VMLAUNCH:         SET_BOTH(VMX_VMLAUNCH); break;
        case VMX_EXIT_VMPTRLD:          SET_BOTH(VMX_VMPTRLD); break;
        case VMX_EXIT_VMPTRST:          SET_BOTH(VMX_VMPTRST); break;
        case VMX_EXIT_VMREAD:           SET_BOTH(VMX_VMREAD); break;
        case VMX_EXIT_VMRESUME:         SET_BOTH(VMX_VMRESUME); break;
        case VMX_EXIT_VMWRITE:          SET_BOTH(VMX_VMWRITE); break;
        case VMX_EXIT_VMXOFF:           SET_BOTH(VMX_VMXOFF); break;
        case VMX_EXIT_VMXON:            SET_BOTH(VMX_VMXON); break;
        case VMX_EXIT_MOV_CRX:
            hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
            if (VMX_EXIT_QUAL_CRX_ACCESS(pVmxTransient->uExitQualification) == VMX_EXIT_QUAL_CRX_ACCESS_READ)
                SET_BOTH(CRX_READ);
            else
                SET_BOTH(CRX_WRITE);
            uEventArg = VMX_EXIT_QUAL_CRX_REGISTER(pVmxTransient->uExitQualification);
            break;
        case VMX_EXIT_MOV_DRX:
            hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
            if (   VMX_EXIT_QUAL_DRX_DIRECTION(pVmxTransient->uExitQualification)
                == VMX_EXIT_QUAL_DRX_DIRECTION_READ)
                SET_BOTH(DRX_READ);
            else
                SET_BOTH(DRX_WRITE);
            uEventArg = VMX_EXIT_QUAL_DRX_REGISTER(pVmxTransient->uExitQualification);
            break;
        case VMX_EXIT_RDMSR:            SET_BOTH(RDMSR); break;
        case VMX_EXIT_WRMSR:            SET_BOTH(WRMSR); break;
        case VMX_EXIT_MWAIT:            SET_BOTH(MWAIT); break;
        case VMX_EXIT_MONITOR:          SET_BOTH(MONITOR); break;
        case VMX_EXIT_PAUSE:            SET_BOTH(PAUSE); break;
        case VMX_EXIT_XDTR_ACCESS:
            hmR0VmxReadExitInstrInfoVmcs(pVmxTransient);
            switch (RT_BF_GET(pVmxTransient->ExitInstrInfo.u, VMX_XDTR_INSINFO_INSTR_ID))
            {
                case VMX_XDTR_INSINFO_II_SGDT: SET_BOTH(SGDT); break;
                case VMX_XDTR_INSINFO_II_SIDT: SET_BOTH(SIDT); break;
                case VMX_XDTR_INSINFO_II_LGDT: SET_BOTH(LGDT); break;
                case VMX_XDTR_INSINFO_II_LIDT: SET_BOTH(LIDT); break;
            }
            break;

        case VMX_EXIT_TR_ACCESS:
            hmR0VmxReadExitInstrInfoVmcs(pVmxTransient);
            switch (RT_BF_GET(pVmxTransient->ExitInstrInfo.u, VMX_YYTR_INSINFO_INSTR_ID))
            {
                case VMX_YYTR_INSINFO_II_SLDT: SET_BOTH(SLDT); break;
                case VMX_YYTR_INSINFO_II_STR:  SET_BOTH(STR); break;
                case VMX_YYTR_INSINFO_II_LLDT: SET_BOTH(LLDT); break;
                case VMX_YYTR_INSINFO_II_LTR:  SET_BOTH(LTR); break;
            }
            break;

        case VMX_EXIT_INVEPT:           SET_BOTH(VMX_INVEPT); break;
        case VMX_EXIT_RDTSCP:           SET_BOTH(RDTSCP); break;
        case VMX_EXIT_INVVPID:          SET_BOTH(VMX_INVVPID); break;
        case VMX_EXIT_WBINVD:           SET_BOTH(WBINVD); break;
        case VMX_EXIT_XSETBV:           SET_BOTH(XSETBV); break;
        case VMX_EXIT_RDRAND:           SET_BOTH(RDRAND); break;
        case VMX_EXIT_INVPCID:          SET_BOTH(VMX_INVPCID); break;
        case VMX_EXIT_VMFUNC:           SET_BOTH(VMX_VMFUNC); break;
        case VMX_EXIT_RDSEED:           SET_BOTH(RDSEED); break;
        case VMX_EXIT_XSAVES:           SET_BOTH(XSAVES); break;
        case VMX_EXIT_XRSTORS:          SET_BOTH(XRSTORS); break;

        /* Events that aren't relevant at this point. */
        case VMX_EXIT_EXT_INT:
        case VMX_EXIT_INT_WINDOW:
        case VMX_EXIT_NMI_WINDOW:
        case VMX_EXIT_TPR_BELOW_THRESHOLD:
        case VMX_EXIT_PREEMPT_TIMER:
        case VMX_EXIT_IO_INSTR:
            break;

        /* Errors and unexpected events. */
        case VMX_EXIT_INIT_SIGNAL:
        case VMX_EXIT_SIPI:
        case VMX_EXIT_IO_SMI:
        case VMX_EXIT_SMI:
        case VMX_EXIT_ERR_INVALID_GUEST_STATE:
        case VMX_EXIT_ERR_MSR_LOAD:
        case VMX_EXIT_ERR_MACHINE_CHECK:
            break;

        default:
            AssertMsgFailed(("Unexpected VM-exit=%#x\n", uExitReason));
            break;
    }
#undef SET_BOTH
#undef SET_EXIT

    /*
     * Dtrace tracepoints go first.   We do them here at once so we don't
     * have to copy the guest state saving and stuff a few dozen times.
     * Down side is that we've got to repeat the switch, though this time
     * we use enmEvent since the probes are a subset of what DBGF does.
     */
    if (fDtrace1 || fDtrace2)
    {
        hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
        hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        switch (enmEvent1)
        {
            /** @todo consider which extra parameters would be helpful for each probe.   */
            case DBGFEVENT_END: break;
            case DBGFEVENT_XCPT_DE:                 VBOXVMM_XCPT_DE(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_DB:                 VBOXVMM_XCPT_DB(pVCpu, pCtx, pCtx->dr[6]); break;
            case DBGFEVENT_XCPT_BP:                 VBOXVMM_XCPT_BP(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_OF:                 VBOXVMM_XCPT_OF(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_BR:                 VBOXVMM_XCPT_BR(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_UD:                 VBOXVMM_XCPT_UD(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_NM:                 VBOXVMM_XCPT_NM(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_DF:                 VBOXVMM_XCPT_DF(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_TS:                 VBOXVMM_XCPT_TS(pVCpu, pCtx, uEventArg); break;
            case DBGFEVENT_XCPT_NP:                 VBOXVMM_XCPT_NP(pVCpu, pCtx, uEventArg); break;
            case DBGFEVENT_XCPT_SS:                 VBOXVMM_XCPT_SS(pVCpu, pCtx, uEventArg); break;
            case DBGFEVENT_XCPT_GP:                 VBOXVMM_XCPT_GP(pVCpu, pCtx, uEventArg); break;
            case DBGFEVENT_XCPT_PF:                 VBOXVMM_XCPT_PF(pVCpu, pCtx, uEventArg, pCtx->cr2); break;
            case DBGFEVENT_XCPT_MF:                 VBOXVMM_XCPT_MF(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_AC:                 VBOXVMM_XCPT_AC(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_XF:                 VBOXVMM_XCPT_XF(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_VE:                 VBOXVMM_XCPT_VE(pVCpu, pCtx); break;
            case DBGFEVENT_XCPT_SX:                 VBOXVMM_XCPT_SX(pVCpu, pCtx, uEventArg); break;
            case DBGFEVENT_INTERRUPT_SOFTWARE:      VBOXVMM_INT_SOFTWARE(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_INSTR_CPUID:             VBOXVMM_INSTR_CPUID(pVCpu, pCtx, pCtx->eax, pCtx->ecx); break;
            case DBGFEVENT_INSTR_GETSEC:            VBOXVMM_INSTR_GETSEC(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_HALT:              VBOXVMM_INSTR_HALT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_INVD:              VBOXVMM_INSTR_INVD(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_INVLPG:            VBOXVMM_INSTR_INVLPG(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_RDPMC:             VBOXVMM_INSTR_RDPMC(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_RDTSC:             VBOXVMM_INSTR_RDTSC(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_RSM:               VBOXVMM_INSTR_RSM(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_CRX_READ:          VBOXVMM_INSTR_CRX_READ(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_INSTR_CRX_WRITE:         VBOXVMM_INSTR_CRX_WRITE(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_INSTR_DRX_READ:          VBOXVMM_INSTR_DRX_READ(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_INSTR_DRX_WRITE:         VBOXVMM_INSTR_DRX_WRITE(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_INSTR_RDMSR:             VBOXVMM_INSTR_RDMSR(pVCpu, pCtx, pCtx->ecx); break;
            case DBGFEVENT_INSTR_WRMSR:             VBOXVMM_INSTR_WRMSR(pVCpu, pCtx, pCtx->ecx,
                                                                        RT_MAKE_U64(pCtx->eax, pCtx->edx)); break;
            case DBGFEVENT_INSTR_MWAIT:             VBOXVMM_INSTR_MWAIT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_MONITOR:           VBOXVMM_INSTR_MONITOR(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_PAUSE:             VBOXVMM_INSTR_PAUSE(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_SGDT:              VBOXVMM_INSTR_SGDT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_SIDT:              VBOXVMM_INSTR_SIDT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_LGDT:              VBOXVMM_INSTR_LGDT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_LIDT:              VBOXVMM_INSTR_LIDT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_SLDT:              VBOXVMM_INSTR_SLDT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_STR:               VBOXVMM_INSTR_STR(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_LLDT:              VBOXVMM_INSTR_LLDT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_LTR:               VBOXVMM_INSTR_LTR(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_RDTSCP:            VBOXVMM_INSTR_RDTSCP(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_WBINVD:            VBOXVMM_INSTR_WBINVD(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_XSETBV:            VBOXVMM_INSTR_XSETBV(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_RDRAND:            VBOXVMM_INSTR_RDRAND(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_RDSEED:            VBOXVMM_INSTR_RDSEED(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_XSAVES:            VBOXVMM_INSTR_XSAVES(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_XRSTORS:           VBOXVMM_INSTR_XRSTORS(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMM_CALL:          VBOXVMM_INSTR_VMM_CALL(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMCLEAR:       VBOXVMM_INSTR_VMX_VMCLEAR(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMLAUNCH:      VBOXVMM_INSTR_VMX_VMLAUNCH(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMPTRLD:       VBOXVMM_INSTR_VMX_VMPTRLD(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMPTRST:       VBOXVMM_INSTR_VMX_VMPTRST(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMREAD:        VBOXVMM_INSTR_VMX_VMREAD(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMRESUME:      VBOXVMM_INSTR_VMX_VMRESUME(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMWRITE:       VBOXVMM_INSTR_VMX_VMWRITE(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMXOFF:        VBOXVMM_INSTR_VMX_VMXOFF(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMXON:         VBOXVMM_INSTR_VMX_VMXON(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_INVEPT:        VBOXVMM_INSTR_VMX_INVEPT(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_INVVPID:       VBOXVMM_INSTR_VMX_INVVPID(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_INVPCID:       VBOXVMM_INSTR_VMX_INVPCID(pVCpu, pCtx); break;
            case DBGFEVENT_INSTR_VMX_VMFUNC:        VBOXVMM_INSTR_VMX_VMFUNC(pVCpu, pCtx); break;
            default: AssertMsgFailed(("enmEvent1=%d uExitReason=%d\n", enmEvent1, uExitReason)); break;
        }
        switch (enmEvent2)
        {
            /** @todo consider which extra parameters would be helpful for each probe. */
            case DBGFEVENT_END: break;
            case DBGFEVENT_EXIT_TASK_SWITCH:        VBOXVMM_EXIT_TASK_SWITCH(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_CPUID:              VBOXVMM_EXIT_CPUID(pVCpu, pCtx, pCtx->eax, pCtx->ecx); break;
            case DBGFEVENT_EXIT_GETSEC:             VBOXVMM_EXIT_GETSEC(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_HALT:               VBOXVMM_EXIT_HALT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_INVD:               VBOXVMM_EXIT_INVD(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_INVLPG:             VBOXVMM_EXIT_INVLPG(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_RDPMC:              VBOXVMM_EXIT_RDPMC(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_RDTSC:              VBOXVMM_EXIT_RDTSC(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_RSM:                VBOXVMM_EXIT_RSM(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_CRX_READ:           VBOXVMM_EXIT_CRX_READ(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_EXIT_CRX_WRITE:          VBOXVMM_EXIT_CRX_WRITE(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_EXIT_DRX_READ:           VBOXVMM_EXIT_DRX_READ(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_EXIT_DRX_WRITE:          VBOXVMM_EXIT_DRX_WRITE(pVCpu, pCtx, (uint8_t)uEventArg); break;
            case DBGFEVENT_EXIT_RDMSR:              VBOXVMM_EXIT_RDMSR(pVCpu, pCtx, pCtx->ecx); break;
            case DBGFEVENT_EXIT_WRMSR:              VBOXVMM_EXIT_WRMSR(pVCpu, pCtx, pCtx->ecx,
                                                                       RT_MAKE_U64(pCtx->eax, pCtx->edx)); break;
            case DBGFEVENT_EXIT_MWAIT:              VBOXVMM_EXIT_MWAIT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_MONITOR:            VBOXVMM_EXIT_MONITOR(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_PAUSE:              VBOXVMM_EXIT_PAUSE(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_SGDT:               VBOXVMM_EXIT_SGDT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_SIDT:               VBOXVMM_EXIT_SIDT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_LGDT:               VBOXVMM_EXIT_LGDT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_LIDT:               VBOXVMM_EXIT_LIDT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_SLDT:               VBOXVMM_EXIT_SLDT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_STR:                VBOXVMM_EXIT_STR(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_LLDT:               VBOXVMM_EXIT_LLDT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_LTR:                VBOXVMM_EXIT_LTR(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_RDTSCP:             VBOXVMM_EXIT_RDTSCP(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_WBINVD:             VBOXVMM_EXIT_WBINVD(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_XSETBV:             VBOXVMM_EXIT_XSETBV(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_RDRAND:             VBOXVMM_EXIT_RDRAND(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_RDSEED:             VBOXVMM_EXIT_RDSEED(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_XSAVES:             VBOXVMM_EXIT_XSAVES(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_XRSTORS:            VBOXVMM_EXIT_XRSTORS(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMM_CALL:           VBOXVMM_EXIT_VMM_CALL(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMCLEAR:        VBOXVMM_EXIT_VMX_VMCLEAR(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMLAUNCH:       VBOXVMM_EXIT_VMX_VMLAUNCH(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMPTRLD:        VBOXVMM_EXIT_VMX_VMPTRLD(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMPTRST:        VBOXVMM_EXIT_VMX_VMPTRST(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMREAD:         VBOXVMM_EXIT_VMX_VMREAD(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMRESUME:       VBOXVMM_EXIT_VMX_VMRESUME(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMWRITE:        VBOXVMM_EXIT_VMX_VMWRITE(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMXOFF:         VBOXVMM_EXIT_VMX_VMXOFF(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMXON:          VBOXVMM_EXIT_VMX_VMXON(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_INVEPT:         VBOXVMM_EXIT_VMX_INVEPT(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_INVVPID:        VBOXVMM_EXIT_VMX_INVVPID(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_INVPCID:        VBOXVMM_EXIT_VMX_INVPCID(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VMFUNC:         VBOXVMM_EXIT_VMX_VMFUNC(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_EPT_MISCONFIG:  VBOXVMM_EXIT_VMX_EPT_MISCONFIG(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_EPT_VIOLATION:  VBOXVMM_EXIT_VMX_EPT_VIOLATION(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VAPIC_ACCESS:   VBOXVMM_EXIT_VMX_VAPIC_ACCESS(pVCpu, pCtx); break;
            case DBGFEVENT_EXIT_VMX_VAPIC_WRITE:    VBOXVMM_EXIT_VMX_VAPIC_WRITE(pVCpu, pCtx); break;
            default: AssertMsgFailed(("enmEvent2=%d uExitReason=%d\n", enmEvent2, uExitReason)); break;
        }
    }

    /*
     * Fire of the DBGF event, if enabled (our check here is just a quick one,
     * the DBGF call will do a full check).
     *
     * Note! DBGF sets DBGFEVENT_INTERRUPT_SOFTWARE in the bitmap.
     * Note! If we have to events, we prioritize the first, i.e. the instruction
     *       one, in order to avoid event nesting.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (   enmEvent1 != DBGFEVENT_END
        && DBGF_IS_EVENT_ENABLED(pVM, enmEvent1))
    {
        VBOXSTRICTRC rcStrict = DBGFEventGenericWithArg(pVM, pVCpu, enmEvent1, uEventArg, DBGFEVENTCTX_HM);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }
    else if (   enmEvent2 != DBGFEVENT_END
             && DBGF_IS_EVENT_ENABLED(pVM, enmEvent2))
    {
        VBOXSTRICTRC rcStrict = DBGFEventGenericWithArg(pVM, pVCpu, enmEvent2, uEventArg, DBGFEVENTCTX_HM);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }

    return VINF_SUCCESS;
}


/**
 * Single-stepping VM-exit filtering.
 *
 * This is preprocessing the VM-exits and deciding whether we've gotten far
 * enough to return VINF_EM_DBG_STEPPED already.  If not, normal VM-exit
 * handling is performed.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu           The cross context virtual CPU structure of the calling EMT.
 * @param   pVmxTransient   Pointer to the VMX-transient structure.
 * @param   pDbgState       The debug state.
 */
DECLINLINE(VBOXSTRICTRC) hmR0VmxRunDebugHandleExit(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient, PVMXRUNDBGSTATE pDbgState)
{
    /*
     * Expensive (saves context) generic dtrace VM-exit probe.
     */
    uint32_t const uExitReason = pVmxTransient->uExitReason;
    if (!VBOXVMM_R0_HMVMX_VMEXIT_ENABLED())
    { /* more likely */ }
    else
    {
        hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
        int rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        AssertRC(rc);
        VBOXVMM_R0_HMVMX_VMEXIT(pVCpu, &pVCpu->cpum.GstCtx, pVmxTransient->uExitReason, pVmxTransient->uExitQualification);
    }

    /*
     * Check for host NMI, just to get that out of the way.
     */
    if (uExitReason != VMX_EXIT_XCPT_OR_NMI)
    { /* normally likely */ }
    else
    {
        int rc2 = hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
        AssertRCReturn(rc2, rc2);
        uint32_t uIntType = VMX_EXIT_INTERRUPTION_INFO_TYPE(pVmxTransient->uExitIntInfo);
        if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI)
            return hmR0VmxExitXcptOrNmi(pVCpu, pVmxTransient);
    }

    /*
     * Check for single stepping event if we're stepping.
     */
    if (pVCpu->hm.s.fSingleInstruction)
    {
        switch (uExitReason)
        {
            case VMX_EXIT_MTF:
                return hmR0VmxExitMtf(pVCpu, pVmxTransient);

            /* Various events: */
            case VMX_EXIT_XCPT_OR_NMI:
            case VMX_EXIT_EXT_INT:
            case VMX_EXIT_TRIPLE_FAULT:
            case VMX_EXIT_INT_WINDOW:
            case VMX_EXIT_NMI_WINDOW:
            case VMX_EXIT_TASK_SWITCH:
            case VMX_EXIT_TPR_BELOW_THRESHOLD:
            case VMX_EXIT_APIC_ACCESS:
            case VMX_EXIT_EPT_VIOLATION:
            case VMX_EXIT_EPT_MISCONFIG:
            case VMX_EXIT_PREEMPT_TIMER:

            /* Instruction specific VM-exits: */
            case VMX_EXIT_CPUID:
            case VMX_EXIT_GETSEC:
            case VMX_EXIT_HLT:
            case VMX_EXIT_INVD:
            case VMX_EXIT_INVLPG:
            case VMX_EXIT_RDPMC:
            case VMX_EXIT_RDTSC:
            case VMX_EXIT_RSM:
            case VMX_EXIT_VMCALL:
            case VMX_EXIT_VMCLEAR:
            case VMX_EXIT_VMLAUNCH:
            case VMX_EXIT_VMPTRLD:
            case VMX_EXIT_VMPTRST:
            case VMX_EXIT_VMREAD:
            case VMX_EXIT_VMRESUME:
            case VMX_EXIT_VMWRITE:
            case VMX_EXIT_VMXOFF:
            case VMX_EXIT_VMXON:
            case VMX_EXIT_MOV_CRX:
            case VMX_EXIT_MOV_DRX:
            case VMX_EXIT_IO_INSTR:
            case VMX_EXIT_RDMSR:
            case VMX_EXIT_WRMSR:
            case VMX_EXIT_MWAIT:
            case VMX_EXIT_MONITOR:
            case VMX_EXIT_PAUSE:
            case VMX_EXIT_XDTR_ACCESS:
            case VMX_EXIT_TR_ACCESS:
            case VMX_EXIT_INVEPT:
            case VMX_EXIT_RDTSCP:
            case VMX_EXIT_INVVPID:
            case VMX_EXIT_WBINVD:
            case VMX_EXIT_XSETBV:
            case VMX_EXIT_RDRAND:
            case VMX_EXIT_INVPCID:
            case VMX_EXIT_VMFUNC:
            case VMX_EXIT_RDSEED:
            case VMX_EXIT_XSAVES:
            case VMX_EXIT_XRSTORS:
            {
                int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP);
                AssertRCReturn(rc, rc);
                if (   pVCpu->cpum.GstCtx.rip    != pDbgState->uRipStart
                    || pVCpu->cpum.GstCtx.cs.Sel != pDbgState->uCsStart)
                    return VINF_EM_DBG_STEPPED;
                break;
            }

            /* Errors and unexpected events: */
            case VMX_EXIT_INIT_SIGNAL:
            case VMX_EXIT_SIPI:
            case VMX_EXIT_IO_SMI:
            case VMX_EXIT_SMI:
            case VMX_EXIT_ERR_INVALID_GUEST_STATE:
            case VMX_EXIT_ERR_MSR_LOAD:
            case VMX_EXIT_ERR_MACHINE_CHECK:
            case VMX_EXIT_APIC_WRITE:  /* Some talk about this being fault like, so I guess we must process it? */
                break;

            default:
                AssertMsgFailed(("Unexpected VM-exit=%#x\n", uExitReason));
                break;
        }
    }

    /*
     * Check for debugger event breakpoints and dtrace probes.
     */
    if (   uExitReason < RT_ELEMENTS(pDbgState->bmExitsToCheck) * 32U
        && ASMBitTest(pDbgState->bmExitsToCheck, uExitReason) )
    {
        VBOXSTRICTRC rcStrict = hmR0VmxHandleExitDtraceEvents(pVCpu, pVmxTransient, uExitReason);
        if (rcStrict != VINF_SUCCESS)
            return rcStrict;
    }

    /*
     * Normal processing.
     */
#ifdef HMVMX_USE_FUNCTION_TABLE
    return g_apfnVMExitHandlers[uExitReason](pVCpu, pVmxTransient);
#else
    return hmR0VmxHandleExit(pVCpu, pVmxTransient, uExitReason);
#endif
}


/**
 * Single steps guest code using VT-x.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 * @note    Mostly the same as hmR0VmxRunGuestCodeNormal().
 */
static VBOXSTRICTRC hmR0VmxRunGuestCodeDebug(PVMCPU pVCpu)
{
    VMXTRANSIENT VmxTransient;
    VmxTransient.fUpdateTscOffsettingAndPreemptTimer = true;

    /* Set HMCPU indicators.  */
    bool const fSavedSingleInstruction = pVCpu->hm.s.fSingleInstruction;
    pVCpu->hm.s.fSingleInstruction     = pVCpu->hm.s.fSingleInstruction || DBGFIsStepping(pVCpu);
    pVCpu->hm.s.fDebugWantRdTscExit    = false;
    pVCpu->hm.s.fUsingDebugLoop        = true;

    /* State we keep to help modify and later restore the VMCS fields we alter, and for detecting steps.  */
    VMXRUNDBGSTATE DbgState;
    hmR0VmxRunDebugStateInit(pVCpu, &DbgState);
    hmR0VmxPreRunGuestDebugStateUpdate(pVCpu, &DbgState, &VmxTransient);

    /*
     * The loop.
     */
    VBOXSTRICTRC rcStrict  = VERR_INTERNAL_ERROR_5;
    for (uint32_t cLoops = 0; ; cLoops++)
    {
        Assert(!HMR0SuspendPending());
        HMVMX_ASSERT_CPU_SAFE(pVCpu);
        bool fStepping = pVCpu->hm.s.fSingleInstruction;

        /*
         * Preparatory work for running guest code, this may force us to return
         * to ring-3.  This bugger disables interrupts on VINF_SUCCESS!
         */
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatEntry, x);
        hmR0VmxPreRunGuestDebugStateApply(pVCpu, &DbgState); /* Set up execute controls the next to can respond to. */
        rcStrict = hmR0VmxPreRunGuest(pVCpu, &VmxTransient, fStepping);
        if (rcStrict != VINF_SUCCESS)
            break;

        hmR0VmxPreRunGuestCommitted(pVCpu, &VmxTransient);
        hmR0VmxPreRunGuestDebugStateApply(pVCpu, &DbgState); /* Override any obnoxious code in the above two calls. */

        /*
         * Now we can run the guest code.
         */
        int rcRun = hmR0VmxRunGuest(pVCpu);

        /*
         * Restore any residual host-state and save any bits shared between host
         * and guest into the guest-CPU state.  Re-enables interrupts!
         */
        hmR0VmxPostRunGuest(pVCpu, &VmxTransient, rcRun);

        /* Check for errors with running the VM (VMLAUNCH/VMRESUME). */
        if (RT_SUCCESS(rcRun))
        { /* very likely */ }
        else
        {
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatPreExit, x);
            hmR0VmxReportWorldSwitchError(pVCpu, rcRun, &VmxTransient);
            return rcRun;
        }

        /* Profile the VM-exit. */
        AssertMsg(VmxTransient.uExitReason <= VMX_EXIT_MAX, ("%#x\n", VmxTransient.uExitReason));
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitAll);
        STAM_COUNTER_INC(&pVCpu->hm.s.paStatExitReasonR0[VmxTransient.uExitReason & MASK_EXITREASON_STAT]);
        STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatPreExit, &pVCpu->hm.s.StatExitHandling, x);
        HMVMX_START_EXIT_DISPATCH_PROF();

        VBOXVMM_R0_HMVMX_VMEXIT_NOCTX(pVCpu, &pVCpu->cpum.GstCtx, VmxTransient.uExitReason);

        /*
         * Handle the VM-exit - we quit earlier on certain VM-exits, see hmR0VmxHandleExitDebug().
         */
        rcStrict = hmR0VmxRunDebugHandleExit(pVCpu, &VmxTransient, &DbgState);
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitHandling, x);
        if (rcStrict != VINF_SUCCESS)
            break;
        if (cLoops > pVCpu->CTX_SUFF(pVM)->hm.s.cMaxResumeLoops)
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchMaxResumeLoops);
            rcStrict = VINF_EM_RAW_INTERRUPT;
            break;
        }

        /*
         * Stepping: Did the RIP change, if so, consider it a single step.
         * Otherwise, make sure one of the TFs gets set.
         */
        if (fStepping)
        {
            int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP);
            AssertRC(rc);
            if (   pVCpu->cpum.GstCtx.rip    != DbgState.uRipStart
                || pVCpu->cpum.GstCtx.cs.Sel != DbgState.uCsStart)
            {
                rcStrict = VINF_EM_DBG_STEPPED;
                break;
            }
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_DR7);
        }

        /*
         * Update when dtrace settings changes (DBGF kicks us, so no need to check).
         */
        if (VBOXVMM_GET_SETTINGS_SEQ_NO() != DbgState.uDtraceSettingsSeqNo)
            hmR0VmxPreRunGuestDebugStateUpdate(pVCpu, &DbgState, &VmxTransient);
    }

    /*
     * Clear the X86_EFL_TF if necessary.
     */
    if (pVCpu->hm.s.fClearTrapFlag)
    {
        int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_RFLAGS);
        AssertRC(rc);
        pVCpu->hm.s.fClearTrapFlag = false;
        pVCpu->cpum.GstCtx.eflags.Bits.u1TF = 0;
    }
    /** @todo there seems to be issues with the resume flag when the monitor trap
     *        flag is pending without being used. Seen early in bios init when
     *        accessing APIC page in protected mode. */

    /*
     * Restore VM-exit control settings as we may not reenter this function the
     * next time around.
     */
    rcStrict = hmR0VmxRunDebugStateRevert(pVCpu, &DbgState, rcStrict);

    /* Restore HMCPU indicators. */
    pVCpu->hm.s.fUsingDebugLoop     = false;
    pVCpu->hm.s.fDebugWantRdTscExit = false;
    pVCpu->hm.s.fSingleInstruction  = fSavedSingleInstruction;

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    return rcStrict;
}


/** @} */


/**
 * Checks if any expensive dtrace probes are enabled and we should go to the
 * debug loop.
 *
 * @returns true if we should use debug loop, false if not.
 */
static bool hmR0VmxAnyExpensiveProbesEnabled(void)
{
    /* It's probably faster to OR the raw 32-bit counter variables together.
       Since the variables are in an array and the probes are next to one
       another (more or less), we have good locality.  So, better read
       eight-nine cache lines ever time and only have one conditional, than
       128+ conditionals, right? */
    return (  VBOXVMM_R0_HMVMX_VMEXIT_ENABLED_RAW() /* expensive too due to context */
            | VBOXVMM_XCPT_DE_ENABLED_RAW()
            | VBOXVMM_XCPT_DB_ENABLED_RAW()
            | VBOXVMM_XCPT_BP_ENABLED_RAW()
            | VBOXVMM_XCPT_OF_ENABLED_RAW()
            | VBOXVMM_XCPT_BR_ENABLED_RAW()
            | VBOXVMM_XCPT_UD_ENABLED_RAW()
            | VBOXVMM_XCPT_NM_ENABLED_RAW()
            | VBOXVMM_XCPT_DF_ENABLED_RAW()
            | VBOXVMM_XCPT_TS_ENABLED_RAW()
            | VBOXVMM_XCPT_NP_ENABLED_RAW()
            | VBOXVMM_XCPT_SS_ENABLED_RAW()
            | VBOXVMM_XCPT_GP_ENABLED_RAW()
            | VBOXVMM_XCPT_PF_ENABLED_RAW()
            | VBOXVMM_XCPT_MF_ENABLED_RAW()
            | VBOXVMM_XCPT_AC_ENABLED_RAW()
            | VBOXVMM_XCPT_XF_ENABLED_RAW()
            | VBOXVMM_XCPT_VE_ENABLED_RAW()
            | VBOXVMM_XCPT_SX_ENABLED_RAW()
            | VBOXVMM_INT_SOFTWARE_ENABLED_RAW()
            | VBOXVMM_INT_HARDWARE_ENABLED_RAW()
           ) != 0
        || (  VBOXVMM_INSTR_HALT_ENABLED_RAW()
            | VBOXVMM_INSTR_MWAIT_ENABLED_RAW()
            | VBOXVMM_INSTR_MONITOR_ENABLED_RAW()
            | VBOXVMM_INSTR_CPUID_ENABLED_RAW()
            | VBOXVMM_INSTR_INVD_ENABLED_RAW()
            | VBOXVMM_INSTR_WBINVD_ENABLED_RAW()
            | VBOXVMM_INSTR_INVLPG_ENABLED_RAW()
            | VBOXVMM_INSTR_RDTSC_ENABLED_RAW()
            | VBOXVMM_INSTR_RDTSCP_ENABLED_RAW()
            | VBOXVMM_INSTR_RDPMC_ENABLED_RAW()
            | VBOXVMM_INSTR_RDMSR_ENABLED_RAW()
            | VBOXVMM_INSTR_WRMSR_ENABLED_RAW()
            | VBOXVMM_INSTR_CRX_READ_ENABLED_RAW()
            | VBOXVMM_INSTR_CRX_WRITE_ENABLED_RAW()
            | VBOXVMM_INSTR_DRX_READ_ENABLED_RAW()
            | VBOXVMM_INSTR_DRX_WRITE_ENABLED_RAW()
            | VBOXVMM_INSTR_PAUSE_ENABLED_RAW()
            | VBOXVMM_INSTR_XSETBV_ENABLED_RAW()
            | VBOXVMM_INSTR_SIDT_ENABLED_RAW()
            | VBOXVMM_INSTR_LIDT_ENABLED_RAW()
            | VBOXVMM_INSTR_SGDT_ENABLED_RAW()
            | VBOXVMM_INSTR_LGDT_ENABLED_RAW()
            | VBOXVMM_INSTR_SLDT_ENABLED_RAW()
            | VBOXVMM_INSTR_LLDT_ENABLED_RAW()
            | VBOXVMM_INSTR_STR_ENABLED_RAW()
            | VBOXVMM_INSTR_LTR_ENABLED_RAW()
            | VBOXVMM_INSTR_GETSEC_ENABLED_RAW()
            | VBOXVMM_INSTR_RSM_ENABLED_RAW()
            | VBOXVMM_INSTR_RDRAND_ENABLED_RAW()
            | VBOXVMM_INSTR_RDSEED_ENABLED_RAW()
            | VBOXVMM_INSTR_XSAVES_ENABLED_RAW()
            | VBOXVMM_INSTR_XRSTORS_ENABLED_RAW()
            | VBOXVMM_INSTR_VMM_CALL_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMCLEAR_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMLAUNCH_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMPTRLD_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMPTRST_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMREAD_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMRESUME_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMWRITE_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMXOFF_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMXON_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_VMFUNC_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_INVEPT_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_INVVPID_ENABLED_RAW()
            | VBOXVMM_INSTR_VMX_INVPCID_ENABLED_RAW()
           ) != 0
        || (  VBOXVMM_EXIT_TASK_SWITCH_ENABLED_RAW()
            | VBOXVMM_EXIT_HALT_ENABLED_RAW()
            | VBOXVMM_EXIT_MWAIT_ENABLED_RAW()
            | VBOXVMM_EXIT_MONITOR_ENABLED_RAW()
            | VBOXVMM_EXIT_CPUID_ENABLED_RAW()
            | VBOXVMM_EXIT_INVD_ENABLED_RAW()
            | VBOXVMM_EXIT_WBINVD_ENABLED_RAW()
            | VBOXVMM_EXIT_INVLPG_ENABLED_RAW()
            | VBOXVMM_EXIT_RDTSC_ENABLED_RAW()
            | VBOXVMM_EXIT_RDTSCP_ENABLED_RAW()
            | VBOXVMM_EXIT_RDPMC_ENABLED_RAW()
            | VBOXVMM_EXIT_RDMSR_ENABLED_RAW()
            | VBOXVMM_EXIT_WRMSR_ENABLED_RAW()
            | VBOXVMM_EXIT_CRX_READ_ENABLED_RAW()
            | VBOXVMM_EXIT_CRX_WRITE_ENABLED_RAW()
            | VBOXVMM_EXIT_DRX_READ_ENABLED_RAW()
            | VBOXVMM_EXIT_DRX_WRITE_ENABLED_RAW()
            | VBOXVMM_EXIT_PAUSE_ENABLED_RAW()
            | VBOXVMM_EXIT_XSETBV_ENABLED_RAW()
            | VBOXVMM_EXIT_SIDT_ENABLED_RAW()
            | VBOXVMM_EXIT_LIDT_ENABLED_RAW()
            | VBOXVMM_EXIT_SGDT_ENABLED_RAW()
            | VBOXVMM_EXIT_LGDT_ENABLED_RAW()
            | VBOXVMM_EXIT_SLDT_ENABLED_RAW()
            | VBOXVMM_EXIT_LLDT_ENABLED_RAW()
            | VBOXVMM_EXIT_STR_ENABLED_RAW()
            | VBOXVMM_EXIT_LTR_ENABLED_RAW()
            | VBOXVMM_EXIT_GETSEC_ENABLED_RAW()
            | VBOXVMM_EXIT_RSM_ENABLED_RAW()
            | VBOXVMM_EXIT_RDRAND_ENABLED_RAW()
            | VBOXVMM_EXIT_RDSEED_ENABLED_RAW()
            | VBOXVMM_EXIT_XSAVES_ENABLED_RAW()
            | VBOXVMM_EXIT_XRSTORS_ENABLED_RAW()
            | VBOXVMM_EXIT_VMM_CALL_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMCLEAR_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMLAUNCH_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMPTRLD_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMPTRST_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMREAD_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMRESUME_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMWRITE_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMXOFF_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMXON_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VMFUNC_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_INVEPT_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_INVVPID_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_INVPCID_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_EPT_VIOLATION_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_EPT_MISCONFIG_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VAPIC_ACCESS_ENABLED_RAW()
            | VBOXVMM_EXIT_VMX_VAPIC_WRITE_ENABLED_RAW()
           ) != 0;
}


/**
 * Runs the guest code using VT-x.
 *
 * @returns Strict VBox status code (i.e. informational status codes too).
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMR0DECL(VBOXSTRICTRC) VMXR0RunGuestCode(PVMCPU pVCpu)
{
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    Assert(!ASMAtomicUoReadU64(&pCtx->fExtrn));
    HMVMX_ASSERT_PREEMPT_SAFE(pVCpu);

    VMMRZCallRing3SetNotification(pVCpu, hmR0VmxCallRing3Callback, pCtx);

    VBOXSTRICTRC rcStrict;
    if (   !pVCpu->hm.s.fUseDebugLoop
        && (!VBOXVMM_ANY_PROBES_ENABLED() || !hmR0VmxAnyExpensiveProbesEnabled())
        && !DBGFIsStepping(pVCpu)
        && !pVCpu->CTX_SUFF(pVM)->dbgf.ro.cEnabledInt3Breakpoints)
        rcStrict = hmR0VmxRunGuestCodeNormal(pVCpu);
    else
        rcStrict = hmR0VmxRunGuestCodeDebug(pVCpu);

    if (rcStrict == VERR_EM_INTERPRETER)
        rcStrict = VINF_EM_RAW_EMULATE_INSTR;
    else if (rcStrict == VINF_EM_RESET)
        rcStrict = VINF_EM_TRIPLE_FAULT;

    int rc2 = hmR0VmxExitToRing3(pVCpu, rcStrict);
    if (RT_FAILURE(rc2))
    {
        pVCpu->hm.s.u32HMError = (uint32_t)VBOXSTRICTRC_VAL(rcStrict);
        rcStrict = rc2;
    }
    Assert(!ASMAtomicUoReadU64(&pCtx->fExtrn));
    Assert(!VMMRZCallRing3IsNotificationSet(pVCpu));
    return rcStrict;
}


#ifndef HMVMX_USE_FUNCTION_TABLE
DECLINLINE(VBOXSTRICTRC) hmR0VmxHandleExit(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient, uint32_t rcReason)
{
#ifdef DEBUG_ramshankar
#define VMEXIT_CALL_RET(a_fSave, a_CallExpr) \
       do { \
            if (a_fSave != 0) \
                hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL); \
            VBOXSTRICTRC rcStrict = a_CallExpr; \
            if (a_fSave != 0) \
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST); \
            return rcStrict; \
        } while (0)
#else
# define VMEXIT_CALL_RET(a_fSave, a_CallExpr) return a_CallExpr
#endif
    switch (rcReason)
    {
        case VMX_EXIT_EPT_MISCONFIG:           VMEXIT_CALL_RET(0, hmR0VmxExitEptMisconfig(pVCpu, pVmxTransient));
        case VMX_EXIT_EPT_VIOLATION:           VMEXIT_CALL_RET(0, hmR0VmxExitEptViolation(pVCpu, pVmxTransient));
        case VMX_EXIT_IO_INSTR:                VMEXIT_CALL_RET(0, hmR0VmxExitIoInstr(pVCpu, pVmxTransient));
        case VMX_EXIT_CPUID:                   VMEXIT_CALL_RET(0, hmR0VmxExitCpuid(pVCpu, pVmxTransient));
        case VMX_EXIT_RDTSC:                   VMEXIT_CALL_RET(0, hmR0VmxExitRdtsc(pVCpu, pVmxTransient));
        case VMX_EXIT_RDTSCP:                  VMEXIT_CALL_RET(0, hmR0VmxExitRdtscp(pVCpu, pVmxTransient));
        case VMX_EXIT_APIC_ACCESS:             VMEXIT_CALL_RET(0, hmR0VmxExitApicAccess(pVCpu, pVmxTransient));
        case VMX_EXIT_XCPT_OR_NMI:             VMEXIT_CALL_RET(0, hmR0VmxExitXcptOrNmi(pVCpu, pVmxTransient));
        case VMX_EXIT_MOV_CRX:                 VMEXIT_CALL_RET(0, hmR0VmxExitMovCRx(pVCpu, pVmxTransient));
        case VMX_EXIT_EXT_INT:                 VMEXIT_CALL_RET(0, hmR0VmxExitExtInt(pVCpu, pVmxTransient));
        case VMX_EXIT_INT_WINDOW:              VMEXIT_CALL_RET(0, hmR0VmxExitIntWindow(pVCpu, pVmxTransient));
        case VMX_EXIT_TPR_BELOW_THRESHOLD:     VMEXIT_CALL_RET(0, hmR0VmxExitTprBelowThreshold(pVCpu, pVmxTransient));
        case VMX_EXIT_MWAIT:                   VMEXIT_CALL_RET(0, hmR0VmxExitMwait(pVCpu, pVmxTransient));
        case VMX_EXIT_MONITOR:                 VMEXIT_CALL_RET(0, hmR0VmxExitMonitor(pVCpu, pVmxTransient));
        case VMX_EXIT_TASK_SWITCH:             VMEXIT_CALL_RET(0, hmR0VmxExitTaskSwitch(pVCpu, pVmxTransient));
        case VMX_EXIT_PREEMPT_TIMER:           VMEXIT_CALL_RET(0, hmR0VmxExitPreemptTimer(pVCpu, pVmxTransient));
        case VMX_EXIT_RDMSR:                   VMEXIT_CALL_RET(0, hmR0VmxExitRdmsr(pVCpu, pVmxTransient));
        case VMX_EXIT_WRMSR:                   VMEXIT_CALL_RET(0, hmR0VmxExitWrmsr(pVCpu, pVmxTransient));
        case VMX_EXIT_VMCALL:                  VMEXIT_CALL_RET(0, hmR0VmxExitVmcall(pVCpu, pVmxTransient));
        case VMX_EXIT_MOV_DRX:                 VMEXIT_CALL_RET(0, hmR0VmxExitMovDRx(pVCpu, pVmxTransient));
        case VMX_EXIT_HLT:                     VMEXIT_CALL_RET(0, hmR0VmxExitHlt(pVCpu, pVmxTransient));
        case VMX_EXIT_INVD:                    VMEXIT_CALL_RET(0, hmR0VmxExitInvd(pVCpu, pVmxTransient));
        case VMX_EXIT_INVLPG:                  VMEXIT_CALL_RET(0, hmR0VmxExitInvlpg(pVCpu, pVmxTransient));
        case VMX_EXIT_RSM:                     VMEXIT_CALL_RET(0, hmR0VmxExitRsm(pVCpu, pVmxTransient));
        case VMX_EXIT_MTF:                     VMEXIT_CALL_RET(0, hmR0VmxExitMtf(pVCpu, pVmxTransient));
        case VMX_EXIT_PAUSE:                   VMEXIT_CALL_RET(0, hmR0VmxExitPause(pVCpu, pVmxTransient));
        case VMX_EXIT_XDTR_ACCESS:             VMEXIT_CALL_RET(0, hmR0VmxExitXdtrAccess(pVCpu, pVmxTransient));
        case VMX_EXIT_TR_ACCESS:               VMEXIT_CALL_RET(0, hmR0VmxExitXdtrAccess(pVCpu, pVmxTransient));
        case VMX_EXIT_WBINVD:                  VMEXIT_CALL_RET(0, hmR0VmxExitWbinvd(pVCpu, pVmxTransient));
        case VMX_EXIT_XSETBV:                  VMEXIT_CALL_RET(0, hmR0VmxExitXsetbv(pVCpu, pVmxTransient));
        case VMX_EXIT_RDRAND:                  VMEXIT_CALL_RET(0, hmR0VmxExitRdrand(pVCpu, pVmxTransient));
        case VMX_EXIT_INVPCID:                 VMEXIT_CALL_RET(0, hmR0VmxExitInvpcid(pVCpu, pVmxTransient));
        case VMX_EXIT_GETSEC:                  VMEXIT_CALL_RET(0, hmR0VmxExitGetsec(pVCpu, pVmxTransient));
        case VMX_EXIT_RDPMC:                   VMEXIT_CALL_RET(0, hmR0VmxExitRdpmc(pVCpu, pVmxTransient));

        case VMX_EXIT_TRIPLE_FAULT:            return hmR0VmxExitTripleFault(pVCpu, pVmxTransient);
        case VMX_EXIT_NMI_WINDOW:              return hmR0VmxExitNmiWindow(pVCpu, pVmxTransient);
        case VMX_EXIT_INIT_SIGNAL:             return hmR0VmxExitInitSignal(pVCpu, pVmxTransient);
        case VMX_EXIT_SIPI:                    return hmR0VmxExitSipi(pVCpu, pVmxTransient);
        case VMX_EXIT_IO_SMI:                  return hmR0VmxExitIoSmi(pVCpu, pVmxTransient);
        case VMX_EXIT_SMI:                     return hmR0VmxExitSmi(pVCpu, pVmxTransient);
        case VMX_EXIT_ERR_MSR_LOAD:            return hmR0VmxExitErrMsrLoad(pVCpu, pVmxTransient);
        case VMX_EXIT_ERR_INVALID_GUEST_STATE: return hmR0VmxExitErrInvalidGuestState(pVCpu, pVmxTransient);
        case VMX_EXIT_ERR_MACHINE_CHECK:       return hmR0VmxExitErrMachineCheck(pVCpu, pVmxTransient);

        case VMX_EXIT_VMCLEAR:
        case VMX_EXIT_VMLAUNCH:
        case VMX_EXIT_VMPTRLD:
        case VMX_EXIT_VMPTRST:
        case VMX_EXIT_VMREAD:
        case VMX_EXIT_VMRESUME:
        case VMX_EXIT_VMWRITE:
        case VMX_EXIT_VMXOFF:
        case VMX_EXIT_VMXON:
        case VMX_EXIT_INVEPT:
        case VMX_EXIT_INVVPID:
        case VMX_EXIT_VMFUNC:
        case VMX_EXIT_XSAVES:
        case VMX_EXIT_XRSTORS:
            return hmR0VmxExitSetPendingXcptUD(pVCpu, pVmxTransient);

        case VMX_EXIT_ENCLS:
        case VMX_EXIT_RDSEED: /* only spurious VM-exits, so undefined */
        case VMX_EXIT_PML_FULL:
        default:
            return hmR0VmxExitErrUndefined(pVCpu, pVmxTransient);
    }
#undef VMEXIT_CALL_RET
}
#endif /* !HMVMX_USE_FUNCTION_TABLE */


#ifdef VBOX_STRICT
/* Is there some generic IPRT define for this that are not in Runtime/internal/\* ?? */
# define HMVMX_ASSERT_PREEMPT_CPUID_VAR() \
    RTCPUID const idAssertCpu = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId()

# define HMVMX_ASSERT_PREEMPT_CPUID() \
    do { \
         RTCPUID const idAssertCpuNow = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId(); \
         AssertMsg(idAssertCpu == idAssertCpuNow,  ("VMX %#x, %#x\n", idAssertCpu, idAssertCpuNow)); \
    } while (0)

# define HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(a_pVCpu, a_pVmxTransient) \
    do { \
        AssertPtr((a_pVCpu)); \
        AssertPtr((a_pVmxTransient)); \
        Assert((a_pVmxTransient)->fVMEntryFailed == false); \
        Assert(ASMIntAreEnabled()); \
        HMVMX_ASSERT_PREEMPT_SAFE(a_pVCpu); \
        HMVMX_ASSERT_PREEMPT_CPUID_VAR(); \
        Log4Func(("vcpu[%RU32] -v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v\n", (a_pVCpu)->idCpu)); \
        HMVMX_ASSERT_PREEMPT_SAFE(a_pVCpu); \
        if (VMMR0IsLogFlushDisabled((a_pVCpu))) \
            HMVMX_ASSERT_PREEMPT_CPUID(); \
        HMVMX_STOP_EXIT_DISPATCH_PROF(); \
    } while (0)

# define HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(a_pVCpu, a_pVmxTransient) \
    do { \
        Log4Func(("\n")); \
    } while (0)
#else
# define HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(a_pVCpu, a_pVmxTransient) \
    do { \
        HMVMX_STOP_EXIT_DISPATCH_PROF(); \
        NOREF((a_pVCpu)); NOREF((a_pVmxTransient)); \
    } while (0)
# define HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(a_pVCpu, a_pVmxTransient) do { } while (0)
#endif


/**
 * Advances the guest RIP by the specified number of bytes.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         Number of bytes to advance the RIP by.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(void) hmR0VmxAdvanceGuestRipBy(PVMCPU pVCpu, uint32_t cbInstr)
{
    /* Advance the RIP. */
    pVCpu->cpum.GstCtx.rip += cbInstr;
    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP);

    /* Update interrupt inhibition. */
    if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && pVCpu->cpum.GstCtx.rip != EMGetInhibitInterruptsPC(pVCpu))
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
}


/**
 * Advances the guest RIP after reading it from the VMCS.
 *
 * @returns VBox status code, no informational status codes.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxAdvanceGuestRip(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    int rc = hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS);
    AssertRCReturn(rc, rc);

    hmR0VmxAdvanceGuestRipBy(pVCpu, pVmxTransient->cbInstr);

    /*
     * Deliver a debug exception to the guest if it is single-stepping. Don't directly inject a #DB but use the
     * pending debug exception field as it takes care of priority of events.
     *
     * See Intel spec. 32.2.1 "Debug Exceptions".
     */
    if (  !pVCpu->hm.s.fSingleInstruction
        && pVCpu->cpum.GstCtx.eflags.Bits.u1TF)
    {
        rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}


/**
 * Tries to determine what part of the guest-state VT-x has deemed as invalid
 * and update error record fields accordingly.
 *
 * @return VMX_IGS_* return codes.
 * @retval VMX_IGS_REASON_NOT_FOUND if this function could not find anything
 *         wrong with the guest state.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @remarks This function assumes our cache of the VMCS controls
 *          are valid, i.e. hmR0VmxCheckVmcsCtls() succeeded.
 */
static uint32_t hmR0VmxCheckGuestState(PVMCPU pVCpu)
{
#define HMVMX_ERROR_BREAK(err)              { uError = (err); break; }
#define HMVMX_CHECK_BREAK(expr, err)        if (!(expr)) { \
                                                uError = (err); \
                                                break; \
                                            } else do { } while (0)

    int        rc;
    PVM        pVM = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX   pCtx = &pVCpu->cpum.GstCtx;
    uint32_t   uError = VMX_IGS_ERROR;
    uint32_t   u32Val;
    bool const fUnrestrictedGuest = pVM->hm.s.vmx.fUnrestrictedGuest;

    do
    {
        /*
         * CR0.
         */
        uint32_t       fSetCr0 = (uint32_t)(pVM->hm.s.vmx.Msrs.u64Cr0Fixed0 & pVM->hm.s.vmx.Msrs.u64Cr0Fixed1);
        uint32_t const fZapCr0 = (uint32_t)(pVM->hm.s.vmx.Msrs.u64Cr0Fixed0 | pVM->hm.s.vmx.Msrs.u64Cr0Fixed1);
        /* Exceptions for unrestricted-guests for fixed CR0 bits (PE, PG).
           See Intel spec. 26.3.1 "Checks on Guest Control Registers, Debug Registers and MSRs." */
        if (fUnrestrictedGuest)
            fSetCr0 &= ~(X86_CR0_PE | X86_CR0_PG);

        uint32_t u32GuestCr0;
        rc = VMXReadVmcs32(VMX_VMCS_GUEST_CR0, &u32GuestCr0);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK((u32GuestCr0 & fSetCr0) == fSetCr0, VMX_IGS_CR0_FIXED1);
        HMVMX_CHECK_BREAK(!(u32GuestCr0 & ~fZapCr0), VMX_IGS_CR0_FIXED0);
        if (   !fUnrestrictedGuest
            &&  (u32GuestCr0 & X86_CR0_PG)
            && !(u32GuestCr0 & X86_CR0_PE))
        {
            HMVMX_ERROR_BREAK(VMX_IGS_CR0_PG_PE_COMBO);
        }

        /*
         * CR4.
         */
        uint64_t const fSetCr4 = (pVM->hm.s.vmx.Msrs.u64Cr4Fixed0 & pVM->hm.s.vmx.Msrs.u64Cr4Fixed1);
        uint64_t const fZapCr4 = (pVM->hm.s.vmx.Msrs.u64Cr4Fixed0 | pVM->hm.s.vmx.Msrs.u64Cr4Fixed1);

        uint32_t u32GuestCr4;
        rc = VMXReadVmcs32(VMX_VMCS_GUEST_CR4, &u32GuestCr4);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK((u32GuestCr4 & fSetCr4) == fSetCr4, VMX_IGS_CR4_FIXED1);
        HMVMX_CHECK_BREAK(!(u32GuestCr4 & ~fZapCr4), VMX_IGS_CR4_FIXED0);

        /*
         * IA32_DEBUGCTL MSR.
         */
        uint64_t u64Val;
        rc = VMXReadVmcs64(VMX_VMCS64_GUEST_DEBUGCTL_FULL, &u64Val);
        AssertRCBreak(rc);
        if (   (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG)
            && (u64Val & 0xfffffe3c))                           /* Bits 31:9, bits 5:2 MBZ. */
        {
            HMVMX_ERROR_BREAK(VMX_IGS_DEBUGCTL_MSR_RESERVED);
        }
        uint64_t u64DebugCtlMsr = u64Val;

#ifdef VBOX_STRICT
        rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY, &u32Val);
        AssertRCBreak(rc);
        Assert(u32Val == pVCpu->hm.s.vmx.u32EntryCtls);
#endif
        bool const fLongModeGuest = RT_BOOL(pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST);

        /*
         * RIP and RFLAGS.
         */
        uint32_t u32Eflags;
#if HC_ARCH_BITS == 64
        rc = VMXReadVmcs64(VMX_VMCS_GUEST_RIP, &u64Val);
        AssertRCBreak(rc);
        /* pCtx->rip can be different than the one in the VMCS (e.g. run guest code and VM-exits that don't update it). */
        if (   !fLongModeGuest
            || !pCtx->cs.Attr.n.u1Long)
        {
            HMVMX_CHECK_BREAK(!(u64Val & UINT64_C(0xffffffff00000000)), VMX_IGS_LONGMODE_RIP_INVALID);
        }
        /** @todo If the processor supports N < 64 linear-address bits, bits 63:N
         *        must be identical if the "IA-32e mode guest" VM-entry
         *        control is 1 and CS.L is 1. No check applies if the
         *        CPU supports 64 linear-address bits. */

        /* Flags in pCtx can be different (real-on-v86 for instance). We are only concerned about the VMCS contents here. */
        rc = VMXReadVmcs64(VMX_VMCS_GUEST_RFLAGS, &u64Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(!(u64Val & UINT64_C(0xffffffffffc08028)),                     /* Bit 63:22, Bit 15, 5, 3 MBZ. */
                          VMX_IGS_RFLAGS_RESERVED);
        HMVMX_CHECK_BREAK((u64Val & X86_EFL_RA1_MASK), VMX_IGS_RFLAGS_RESERVED1);       /* Bit 1 MB1. */
        u32Eflags = u64Val;
#else
        rc = VMXReadVmcs32(VMX_VMCS_GUEST_RFLAGS, &u32Eflags);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(!(u32Eflags & 0xffc08028), VMX_IGS_RFLAGS_RESERVED);          /* Bit 31:22, Bit 15, 5, 3 MBZ. */
        HMVMX_CHECK_BREAK((u32Eflags & X86_EFL_RA1_MASK), VMX_IGS_RFLAGS_RESERVED1);    /* Bit 1 MB1. */
#endif

        if (   fLongModeGuest
            || (   fUnrestrictedGuest
                && !(u32GuestCr0 & X86_CR0_PE)))
        {
            HMVMX_CHECK_BREAK(!(u32Eflags & X86_EFL_VM), VMX_IGS_RFLAGS_VM_INVALID);
        }

        uint32_t u32EntryInfo;
        rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, &u32EntryInfo);
        AssertRCBreak(rc);
        if (   VMX_ENTRY_INTERRUPTION_INFO_IS_VALID(u32EntryInfo)
            && VMX_ENTRY_INTERRUPTION_INFO_TYPE(u32EntryInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT)
        {
            HMVMX_CHECK_BREAK(u32Eflags & X86_EFL_IF, VMX_IGS_RFLAGS_IF_INVALID);
        }

        /*
         * 64-bit checks.
         */
#if HC_ARCH_BITS == 64
        if (fLongModeGuest)
        {
            HMVMX_CHECK_BREAK(u32GuestCr0 & X86_CR0_PG, VMX_IGS_CR0_PG_LONGMODE);
            HMVMX_CHECK_BREAK(u32GuestCr4 & X86_CR4_PAE, VMX_IGS_CR4_PAE_LONGMODE);
        }

        if (   !fLongModeGuest
            && (u32GuestCr4 & X86_CR4_PCIDE))
        {
            HMVMX_ERROR_BREAK(VMX_IGS_CR4_PCIDE);
        }

        /** @todo CR3 field must be such that bits 63:52 and bits in the range
         *        51:32 beyond the processor's physical-address width are 0. */

        if (   (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG)
            && (pCtx->dr[7] & X86_DR7_MBZ_MASK))
        {
            HMVMX_ERROR_BREAK(VMX_IGS_DR7_RESERVED);
        }

        rc = VMXReadVmcs64(VMX_VMCS_HOST_SYSENTER_ESP, &u64Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(X86_IS_CANONICAL(u64Val), VMX_IGS_SYSENTER_ESP_NOT_CANONICAL);

        rc = VMXReadVmcs64(VMX_VMCS_HOST_SYSENTER_EIP, &u64Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(X86_IS_CANONICAL(u64Val), VMX_IGS_SYSENTER_EIP_NOT_CANONICAL);
#endif

        /*
         * PERF_GLOBAL MSR.
         */
        if (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_PERF_MSR)
        {
            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & UINT64_C(0xfffffff8fffffffc)),
                              VMX_IGS_PERF_GLOBAL_MSR_RESERVED);        /* Bits 63:35, bits 31:2 MBZ. */
        }

        /*
         * PAT MSR.
         */
        if (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_PAT_MSR)
        {
            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PAT_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & UINT64_C(0x707070707070707)), VMX_IGS_PAT_MSR_RESERVED);
            for (unsigned i = 0; i < 8; i++)
            {
                uint8_t u8Val = (u64Val & 0xff);
                if (   u8Val != 0 /* UC */
                    && u8Val != 1 /* WC */
                    && u8Val != 4 /* WT */
                    && u8Val != 5 /* WP */
                    && u8Val != 6 /* WB */
                    && u8Val != 7 /* UC- */)
                {
                    HMVMX_ERROR_BREAK(VMX_IGS_PAT_MSR_INVALID);
                }
                u64Val >>= 8;
            }
        }

        /*
         * EFER MSR.
         */
        if (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_EFER_MSR)
        {
            Assert(pVM->hm.s.vmx.fSupportsVmcsEfer);
            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_EFER_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & UINT64_C(0xfffffffffffff2fe)),
                              VMX_IGS_EFER_MSR_RESERVED);               /* Bits 63:12, bit 9, bits 7:1 MBZ. */
            HMVMX_CHECK_BREAK(RT_BOOL(u64Val & MSR_K6_EFER_LMA) == RT_BOOL(  pVCpu->hm.s.vmx.u32EntryCtls
                                                                           & VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST),
                              VMX_IGS_EFER_LMA_GUEST_MODE_MISMATCH);
            HMVMX_CHECK_BREAK(   fUnrestrictedGuest
                              || !(u32GuestCr0 & X86_CR0_PG)
                              || RT_BOOL(u64Val & MSR_K6_EFER_LMA) == RT_BOOL(u64Val & MSR_K6_EFER_LME),
                              VMX_IGS_EFER_LMA_LME_MISMATCH);
        }

        /*
         * Segment registers.
         */
        HMVMX_CHECK_BREAK(   (pCtx->ldtr.Attr.u & X86DESCATTR_UNUSABLE)
                          || !(pCtx->ldtr.Sel & X86_SEL_LDT), VMX_IGS_LDTR_TI_INVALID);
        if (!(u32Eflags & X86_EFL_VM))
        {
            /* CS */
            HMVMX_CHECK_BREAK(pCtx->cs.Attr.n.u1Present, VMX_IGS_CS_ATTR_P_INVALID);
            HMVMX_CHECK_BREAK(!(pCtx->cs.Attr.u & 0xf00), VMX_IGS_CS_ATTR_RESERVED);
            HMVMX_CHECK_BREAK(!(pCtx->cs.Attr.u & 0xfffe0000), VMX_IGS_CS_ATTR_RESERVED);
            HMVMX_CHECK_BREAK(   (pCtx->cs.u32Limit & 0xfff) == 0xfff
                              || !(pCtx->cs.Attr.n.u1Granularity), VMX_IGS_CS_ATTR_G_INVALID);
            HMVMX_CHECK_BREAK(   !(pCtx->cs.u32Limit & 0xfff00000)
                              || (pCtx->cs.Attr.n.u1Granularity), VMX_IGS_CS_ATTR_G_INVALID);
            /* CS cannot be loaded with NULL in protected mode. */
            HMVMX_CHECK_BREAK(pCtx->cs.Attr.u && !(pCtx->cs.Attr.u & X86DESCATTR_UNUSABLE), VMX_IGS_CS_ATTR_UNUSABLE);
            HMVMX_CHECK_BREAK(pCtx->cs.Attr.n.u1DescType, VMX_IGS_CS_ATTR_S_INVALID);
            if (pCtx->cs.Attr.n.u4Type == 9 || pCtx->cs.Attr.n.u4Type == 11)
                HMVMX_CHECK_BREAK(pCtx->cs.Attr.n.u2Dpl == pCtx->ss.Attr.n.u2Dpl, VMX_IGS_CS_SS_ATTR_DPL_UNEQUAL);
            else if (pCtx->cs.Attr.n.u4Type == 13 || pCtx->cs.Attr.n.u4Type == 15)
                HMVMX_CHECK_BREAK(pCtx->cs.Attr.n.u2Dpl <= pCtx->ss.Attr.n.u2Dpl, VMX_IGS_CS_SS_ATTR_DPL_MISMATCH);
            else if (pVM->hm.s.vmx.fUnrestrictedGuest && pCtx->cs.Attr.n.u4Type == 3)
                HMVMX_CHECK_BREAK(pCtx->cs.Attr.n.u2Dpl == 0, VMX_IGS_CS_ATTR_DPL_INVALID);
            else
                HMVMX_ERROR_BREAK(VMX_IGS_CS_ATTR_TYPE_INVALID);

            /* SS */
            HMVMX_CHECK_BREAK(   pVM->hm.s.vmx.fUnrestrictedGuest
                              || (pCtx->ss.Sel & X86_SEL_RPL) == (pCtx->cs.Sel & X86_SEL_RPL), VMX_IGS_SS_CS_RPL_UNEQUAL);
            HMVMX_CHECK_BREAK(pCtx->ss.Attr.n.u2Dpl == (pCtx->ss.Sel & X86_SEL_RPL), VMX_IGS_SS_ATTR_DPL_RPL_UNEQUAL);
            if (   !(pCtx->cr0 & X86_CR0_PE)
                || pCtx->cs.Attr.n.u4Type == 3)
            {
                HMVMX_CHECK_BREAK(!pCtx->ss.Attr.n.u2Dpl, VMX_IGS_SS_ATTR_DPL_INVALID);
            }
            if (!(pCtx->ss.Attr.u & X86DESCATTR_UNUSABLE))
            {
                HMVMX_CHECK_BREAK(pCtx->ss.Attr.n.u4Type == 3 || pCtx->ss.Attr.n.u4Type == 7, VMX_IGS_SS_ATTR_TYPE_INVALID);
                HMVMX_CHECK_BREAK(pCtx->ss.Attr.n.u1Present, VMX_IGS_SS_ATTR_P_INVALID);
                HMVMX_CHECK_BREAK(!(pCtx->ss.Attr.u & 0xf00), VMX_IGS_SS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(!(pCtx->ss.Attr.u & 0xfffe0000), VMX_IGS_SS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(   (pCtx->ss.u32Limit & 0xfff) == 0xfff
                                  || !(pCtx->ss.Attr.n.u1Granularity), VMX_IGS_SS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->ss.u32Limit & 0xfff00000)
                                  || (pCtx->ss.Attr.n.u1Granularity), VMX_IGS_SS_ATTR_G_INVALID);
            }

            /* DS, ES, FS, GS - only check for usable selectors, see hmR0VmxExportGuestSegmenReg(). */
            if (!(pCtx->ds.Attr.u & X86DESCATTR_UNUSABLE))
            {
                HMVMX_CHECK_BREAK(pCtx->ds.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED, VMX_IGS_DS_ATTR_A_INVALID);
                HMVMX_CHECK_BREAK(pCtx->ds.Attr.n.u1Present, VMX_IGS_DS_ATTR_P_INVALID);
                HMVMX_CHECK_BREAK(   pVM->hm.s.vmx.fUnrestrictedGuest
                                  || pCtx->ds.Attr.n.u4Type > 11
                                  || pCtx->ds.Attr.n.u2Dpl >= (pCtx->ds.Sel & X86_SEL_RPL), VMX_IGS_DS_ATTR_DPL_RPL_UNEQUAL);
                HMVMX_CHECK_BREAK(!(pCtx->ds.Attr.u & 0xf00), VMX_IGS_DS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(!(pCtx->ds.Attr.u & 0xfffe0000), VMX_IGS_DS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(   (pCtx->ds.u32Limit & 0xfff) == 0xfff
                                  || !(pCtx->ds.Attr.n.u1Granularity), VMX_IGS_DS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->ds.u32Limit & 0xfff00000)
                                  || (pCtx->ds.Attr.n.u1Granularity), VMX_IGS_DS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->ds.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                                  || (pCtx->ds.Attr.n.u4Type & X86_SEL_TYPE_READ), VMX_IGS_DS_ATTR_TYPE_INVALID);
            }
            if (!(pCtx->es.Attr.u & X86DESCATTR_UNUSABLE))
            {
                HMVMX_CHECK_BREAK(pCtx->es.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED, VMX_IGS_ES_ATTR_A_INVALID);
                HMVMX_CHECK_BREAK(pCtx->es.Attr.n.u1Present, VMX_IGS_ES_ATTR_P_INVALID);
                HMVMX_CHECK_BREAK(   pVM->hm.s.vmx.fUnrestrictedGuest
                                  || pCtx->es.Attr.n.u4Type > 11
                                  || pCtx->es.Attr.n.u2Dpl >= (pCtx->es.Sel & X86_SEL_RPL), VMX_IGS_DS_ATTR_DPL_RPL_UNEQUAL);
                HMVMX_CHECK_BREAK(!(pCtx->es.Attr.u & 0xf00), VMX_IGS_ES_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(!(pCtx->es.Attr.u & 0xfffe0000), VMX_IGS_ES_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(   (pCtx->es.u32Limit & 0xfff) == 0xfff
                                  || !(pCtx->es.Attr.n.u1Granularity), VMX_IGS_ES_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->es.u32Limit & 0xfff00000)
                                  || (pCtx->es.Attr.n.u1Granularity), VMX_IGS_ES_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->es.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                                  || (pCtx->es.Attr.n.u4Type & X86_SEL_TYPE_READ), VMX_IGS_ES_ATTR_TYPE_INVALID);
            }
            if (!(pCtx->fs.Attr.u & X86DESCATTR_UNUSABLE))
            {
                HMVMX_CHECK_BREAK(pCtx->fs.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED, VMX_IGS_FS_ATTR_A_INVALID);
                HMVMX_CHECK_BREAK(pCtx->fs.Attr.n.u1Present, VMX_IGS_FS_ATTR_P_INVALID);
                HMVMX_CHECK_BREAK(   pVM->hm.s.vmx.fUnrestrictedGuest
                                  || pCtx->fs.Attr.n.u4Type > 11
                                  || pCtx->fs.Attr.n.u2Dpl >= (pCtx->fs.Sel & X86_SEL_RPL), VMX_IGS_FS_ATTR_DPL_RPL_UNEQUAL);
                HMVMX_CHECK_BREAK(!(pCtx->fs.Attr.u & 0xf00), VMX_IGS_FS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(!(pCtx->fs.Attr.u & 0xfffe0000), VMX_IGS_FS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(   (pCtx->fs.u32Limit & 0xfff) == 0xfff
                                  || !(pCtx->fs.Attr.n.u1Granularity), VMX_IGS_FS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->fs.u32Limit & 0xfff00000)
                                  || (pCtx->fs.Attr.n.u1Granularity), VMX_IGS_FS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->fs.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                                  || (pCtx->fs.Attr.n.u4Type & X86_SEL_TYPE_READ), VMX_IGS_FS_ATTR_TYPE_INVALID);
            }
            if (!(pCtx->gs.Attr.u & X86DESCATTR_UNUSABLE))
            {
                HMVMX_CHECK_BREAK(pCtx->gs.Attr.n.u4Type & X86_SEL_TYPE_ACCESSED, VMX_IGS_GS_ATTR_A_INVALID);
                HMVMX_CHECK_BREAK(pCtx->gs.Attr.n.u1Present, VMX_IGS_GS_ATTR_P_INVALID);
                HMVMX_CHECK_BREAK(   pVM->hm.s.vmx.fUnrestrictedGuest
                                  || pCtx->gs.Attr.n.u4Type > 11
                                  || pCtx->gs.Attr.n.u2Dpl >= (pCtx->gs.Sel & X86_SEL_RPL), VMX_IGS_GS_ATTR_DPL_RPL_UNEQUAL);
                HMVMX_CHECK_BREAK(!(pCtx->gs.Attr.u & 0xf00), VMX_IGS_GS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(!(pCtx->gs.Attr.u & 0xfffe0000), VMX_IGS_GS_ATTR_RESERVED);
                HMVMX_CHECK_BREAK(   (pCtx->gs.u32Limit & 0xfff) == 0xfff
                                  || !(pCtx->gs.Attr.n.u1Granularity), VMX_IGS_GS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->gs.u32Limit & 0xfff00000)
                                  || (pCtx->gs.Attr.n.u1Granularity), VMX_IGS_GS_ATTR_G_INVALID);
                HMVMX_CHECK_BREAK(   !(pCtx->gs.Attr.n.u4Type & X86_SEL_TYPE_CODE)
                                  || (pCtx->gs.Attr.n.u4Type & X86_SEL_TYPE_READ), VMX_IGS_GS_ATTR_TYPE_INVALID);
            }
            /* 64-bit capable CPUs. */
#if HC_ARCH_BITS == 64
            HMVMX_CHECK_BREAK(X86_IS_CANONICAL(pCtx->fs.u64Base), VMX_IGS_FS_BASE_NOT_CANONICAL);
            HMVMX_CHECK_BREAK(X86_IS_CANONICAL(pCtx->gs.u64Base), VMX_IGS_GS_BASE_NOT_CANONICAL);
            HMVMX_CHECK_BREAK(   (pCtx->ldtr.Attr.u & X86DESCATTR_UNUSABLE)
                              || X86_IS_CANONICAL(pCtx->ldtr.u64Base), VMX_IGS_LDTR_BASE_NOT_CANONICAL);
            HMVMX_CHECK_BREAK(!RT_HI_U32(pCtx->cs.u64Base), VMX_IGS_LONGMODE_CS_BASE_INVALID);
            HMVMX_CHECK_BREAK((pCtx->ss.Attr.u & X86DESCATTR_UNUSABLE) || !RT_HI_U32(pCtx->ss.u64Base),
                              VMX_IGS_LONGMODE_SS_BASE_INVALID);
            HMVMX_CHECK_BREAK((pCtx->ds.Attr.u & X86DESCATTR_UNUSABLE) || !RT_HI_U32(pCtx->ds.u64Base),
                              VMX_IGS_LONGMODE_DS_BASE_INVALID);
            HMVMX_CHECK_BREAK((pCtx->es.Attr.u & X86DESCATTR_UNUSABLE) || !RT_HI_U32(pCtx->es.u64Base),
                              VMX_IGS_LONGMODE_ES_BASE_INVALID);
#endif
        }
        else
        {
            /* V86 mode checks. */
            uint32_t u32CSAttr, u32SSAttr, u32DSAttr, u32ESAttr, u32FSAttr, u32GSAttr;
            if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
            {
                u32CSAttr = 0xf3;   u32SSAttr = 0xf3;
                u32DSAttr = 0xf3;   u32ESAttr = 0xf3;
                u32FSAttr = 0xf3;   u32GSAttr = 0xf3;
            }
            else
            {
                u32CSAttr = pCtx->cs.Attr.u;   u32SSAttr = pCtx->ss.Attr.u;
                u32DSAttr = pCtx->ds.Attr.u;   u32ESAttr = pCtx->es.Attr.u;
                u32FSAttr = pCtx->fs.Attr.u;   u32GSAttr = pCtx->gs.Attr.u;
            }

            /* CS */
            HMVMX_CHECK_BREAK((pCtx->cs.u64Base == (uint64_t)pCtx->cs.Sel << 4), VMX_IGS_V86_CS_BASE_INVALID);
            HMVMX_CHECK_BREAK(pCtx->cs.u32Limit == 0xffff, VMX_IGS_V86_CS_LIMIT_INVALID);
            HMVMX_CHECK_BREAK(u32CSAttr == 0xf3, VMX_IGS_V86_CS_ATTR_INVALID);
            /* SS */
            HMVMX_CHECK_BREAK((pCtx->ss.u64Base == (uint64_t)pCtx->ss.Sel << 4), VMX_IGS_V86_SS_BASE_INVALID);
            HMVMX_CHECK_BREAK(pCtx->ss.u32Limit == 0xffff, VMX_IGS_V86_SS_LIMIT_INVALID);
            HMVMX_CHECK_BREAK(u32SSAttr == 0xf3, VMX_IGS_V86_SS_ATTR_INVALID);
            /* DS */
            HMVMX_CHECK_BREAK((pCtx->ds.u64Base == (uint64_t)pCtx->ds.Sel << 4), VMX_IGS_V86_DS_BASE_INVALID);
            HMVMX_CHECK_BREAK(pCtx->ds.u32Limit == 0xffff, VMX_IGS_V86_DS_LIMIT_INVALID);
            HMVMX_CHECK_BREAK(u32DSAttr == 0xf3, VMX_IGS_V86_DS_ATTR_INVALID);
            /* ES */
            HMVMX_CHECK_BREAK((pCtx->es.u64Base == (uint64_t)pCtx->es.Sel << 4), VMX_IGS_V86_ES_BASE_INVALID);
            HMVMX_CHECK_BREAK(pCtx->es.u32Limit == 0xffff, VMX_IGS_V86_ES_LIMIT_INVALID);
            HMVMX_CHECK_BREAK(u32ESAttr == 0xf3, VMX_IGS_V86_ES_ATTR_INVALID);
            /* FS */
            HMVMX_CHECK_BREAK((pCtx->fs.u64Base == (uint64_t)pCtx->fs.Sel << 4), VMX_IGS_V86_FS_BASE_INVALID);
            HMVMX_CHECK_BREAK(pCtx->fs.u32Limit == 0xffff, VMX_IGS_V86_FS_LIMIT_INVALID);
            HMVMX_CHECK_BREAK(u32FSAttr == 0xf3, VMX_IGS_V86_FS_ATTR_INVALID);
            /* GS */
            HMVMX_CHECK_BREAK((pCtx->gs.u64Base == (uint64_t)pCtx->gs.Sel << 4), VMX_IGS_V86_GS_BASE_INVALID);
            HMVMX_CHECK_BREAK(pCtx->gs.u32Limit == 0xffff, VMX_IGS_V86_GS_LIMIT_INVALID);
            HMVMX_CHECK_BREAK(u32GSAttr == 0xf3, VMX_IGS_V86_GS_ATTR_INVALID);
            /* 64-bit capable CPUs. */
#if HC_ARCH_BITS == 64
            HMVMX_CHECK_BREAK(X86_IS_CANONICAL(pCtx->fs.u64Base), VMX_IGS_FS_BASE_NOT_CANONICAL);
            HMVMX_CHECK_BREAK(X86_IS_CANONICAL(pCtx->gs.u64Base), VMX_IGS_GS_BASE_NOT_CANONICAL);
            HMVMX_CHECK_BREAK(   (pCtx->ldtr.Attr.u & X86DESCATTR_UNUSABLE)
                              || X86_IS_CANONICAL(pCtx->ldtr.u64Base), VMX_IGS_LDTR_BASE_NOT_CANONICAL);
            HMVMX_CHECK_BREAK(!RT_HI_U32(pCtx->cs.u64Base), VMX_IGS_LONGMODE_CS_BASE_INVALID);
            HMVMX_CHECK_BREAK((pCtx->ss.Attr.u & X86DESCATTR_UNUSABLE) || !RT_HI_U32(pCtx->ss.u64Base),
                              VMX_IGS_LONGMODE_SS_BASE_INVALID);
            HMVMX_CHECK_BREAK((pCtx->ds.Attr.u & X86DESCATTR_UNUSABLE) || !RT_HI_U32(pCtx->ds.u64Base),
                              VMX_IGS_LONGMODE_DS_BASE_INVALID);
            HMVMX_CHECK_BREAK((pCtx->es.Attr.u & X86DESCATTR_UNUSABLE) || !RT_HI_U32(pCtx->es.u64Base),
                              VMX_IGS_LONGMODE_ES_BASE_INVALID);
#endif
        }

        /*
         * TR.
         */
        HMVMX_CHECK_BREAK(!(pCtx->tr.Sel & X86_SEL_LDT), VMX_IGS_TR_TI_INVALID);
        /* 64-bit capable CPUs. */
#if HC_ARCH_BITS == 64
        HMVMX_CHECK_BREAK(X86_IS_CANONICAL(pCtx->tr.u64Base), VMX_IGS_TR_BASE_NOT_CANONICAL);
#endif
        if (fLongModeGuest)
        {
            HMVMX_CHECK_BREAK(pCtx->tr.Attr.n.u4Type == 11,           /* 64-bit busy TSS. */
                              VMX_IGS_LONGMODE_TR_ATTR_TYPE_INVALID);
        }
        else
        {
            HMVMX_CHECK_BREAK(   pCtx->tr.Attr.n.u4Type == 3          /* 16-bit busy TSS. */
                              || pCtx->tr.Attr.n.u4Type == 11,        /* 32-bit busy TSS.*/
                              VMX_IGS_TR_ATTR_TYPE_INVALID);
        }
        HMVMX_CHECK_BREAK(!pCtx->tr.Attr.n.u1DescType, VMX_IGS_TR_ATTR_S_INVALID);
        HMVMX_CHECK_BREAK(pCtx->tr.Attr.n.u1Present, VMX_IGS_TR_ATTR_P_INVALID);
        HMVMX_CHECK_BREAK(!(pCtx->tr.Attr.u & 0xf00), VMX_IGS_TR_ATTR_RESERVED);   /* Bits 11:8 MBZ. */
        HMVMX_CHECK_BREAK(   (pCtx->tr.u32Limit & 0xfff) == 0xfff
                          || !(pCtx->tr.Attr.n.u1Granularity), VMX_IGS_TR_ATTR_G_INVALID);
        HMVMX_CHECK_BREAK(   !(pCtx->tr.u32Limit & 0xfff00000)
                          || (pCtx->tr.Attr.n.u1Granularity), VMX_IGS_TR_ATTR_G_INVALID);
        HMVMX_CHECK_BREAK(!(pCtx->tr.Attr.u & X86DESCATTR_UNUSABLE), VMX_IGS_TR_ATTR_UNUSABLE);

        /*
         * GDTR and IDTR.
         */
#if HC_ARCH_BITS == 64
        rc = VMXReadVmcs64(VMX_VMCS_GUEST_GDTR_BASE, &u64Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(X86_IS_CANONICAL(u64Val), VMX_IGS_GDTR_BASE_NOT_CANONICAL);

        rc = VMXReadVmcs64(VMX_VMCS_GUEST_IDTR_BASE, &u64Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(X86_IS_CANONICAL(u64Val), VMX_IGS_IDTR_BASE_NOT_CANONICAL);
#endif

        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_GDTR_LIMIT, &u32Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(!(u32Val & 0xffff0000), VMX_IGS_GDTR_LIMIT_INVALID);      /* Bits 31:16 MBZ. */

        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_IDTR_LIMIT, &u32Val);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(!(u32Val & 0xffff0000), VMX_IGS_IDTR_LIMIT_INVALID);      /* Bits 31:16 MBZ. */

        /*
         * Guest Non-Register State.
         */
        /* Activity State. */
        uint32_t u32ActivityState;
        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_ACTIVITY_STATE, &u32ActivityState);
        AssertRCBreak(rc);
        HMVMX_CHECK_BREAK(   !u32ActivityState
                          || (u32ActivityState & MSR_IA32_VMX_MISC_ACTIVITY_STATES(pVM->hm.s.vmx.Msrs.u64Misc)),
                             VMX_IGS_ACTIVITY_STATE_INVALID);
        HMVMX_CHECK_BREAK(   !(pCtx->ss.Attr.n.u2Dpl)
                          || u32ActivityState != VMX_VMCS_GUEST_ACTIVITY_HLT, VMX_IGS_ACTIVITY_STATE_HLT_INVALID);
        uint32_t u32IntrState;
        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &u32IntrState);
        AssertRCBreak(rc);
        if (   u32IntrState == VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS
            || u32IntrState == VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI)
        {
            HMVMX_CHECK_BREAK(u32ActivityState == VMX_VMCS_GUEST_ACTIVITY_ACTIVE, VMX_IGS_ACTIVITY_STATE_ACTIVE_INVALID);
        }

        /** @todo Activity state and injecting interrupts. Left as a todo since we
         *        currently don't use activity states but ACTIVE. */

        HMVMX_CHECK_BREAK(   !(pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_ENTRY_SMM)
                          || u32ActivityState != VMX_VMCS_GUEST_ACTIVITY_SIPI_WAIT, VMX_IGS_ACTIVITY_STATE_SIPI_WAIT_INVALID);

        /* Guest interruptibility-state. */
        HMVMX_CHECK_BREAK(!(u32IntrState & 0xfffffff0), VMX_IGS_INTERRUPTIBILITY_STATE_RESERVED);
        HMVMX_CHECK_BREAK((u32IntrState & (  VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI
                                           | VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS))
                            != (  VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI
                                | VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS),
                          VMX_IGS_INTERRUPTIBILITY_STATE_STI_MOVSS_INVALID);
        HMVMX_CHECK_BREAK(   (u32Eflags & X86_EFL_IF)
                          || !(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI),
                          VMX_IGS_INTERRUPTIBILITY_STATE_STI_EFL_INVALID);
        if (VMX_ENTRY_INTERRUPTION_INFO_IS_VALID(u32EntryInfo))
        {
            if (VMX_ENTRY_INTERRUPTION_INFO_TYPE(u32EntryInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT)
            {
                HMVMX_CHECK_BREAK(   !(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI)
                                  && !(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS),
                                  VMX_IGS_INTERRUPTIBILITY_STATE_EXT_INT_INVALID);
            }
            else if (VMX_ENTRY_INTERRUPTION_INFO_TYPE(u32EntryInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI)
            {
                HMVMX_CHECK_BREAK(!(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS),
                                  VMX_IGS_INTERRUPTIBILITY_STATE_MOVSS_INVALID);
                HMVMX_CHECK_BREAK(!(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI),
                                  VMX_IGS_INTERRUPTIBILITY_STATE_STI_INVALID);
            }
        }
        /** @todo Assumes the processor is not in SMM. */
        HMVMX_CHECK_BREAK(!(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI),
                          VMX_IGS_INTERRUPTIBILITY_STATE_SMI_INVALID);
        HMVMX_CHECK_BREAK(   !(pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_ENTRY_SMM)
                          || (u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI),
                             VMX_IGS_INTERRUPTIBILITY_STATE_SMI_SMM_INVALID);
        if (   (pVCpu->hm.s.vmx.u32PinCtls & VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI)
            && VMX_ENTRY_INTERRUPTION_INFO_IS_VALID(u32EntryInfo)
            && VMX_ENTRY_INTERRUPTION_INFO_TYPE(u32EntryInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI)
        {
            HMVMX_CHECK_BREAK(!(u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI),
                              VMX_IGS_INTERRUPTIBILITY_STATE_NMI_INVALID);
        }

        /* Pending debug exceptions. */
#if HC_ARCH_BITS == 64
        rc = VMXReadVmcs64(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, &u64Val);
        AssertRCBreak(rc);
        /* Bits 63:15, Bit 13, Bits 11:4 MBZ. */
        HMVMX_CHECK_BREAK(!(u64Val & UINT64_C(0xffffffffffffaff0)), VMX_IGS_LONGMODE_PENDING_DEBUG_RESERVED);
        u32Val = u64Val;    /* For pending debug exceptions checks below. */
#else
        rc = VMXReadVmcs32(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, &u32Val);
        AssertRCBreak(rc);
        /* Bits 31:15, Bit 13, Bits 11:4 MBZ. */
        HMVMX_CHECK_BREAK(!(u32Val & 0xffffaff0), VMX_IGS_PENDING_DEBUG_RESERVED);
#endif

        if (   (u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI)
            || (u32IntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS)
            || u32ActivityState == VMX_VMCS_GUEST_ACTIVITY_HLT)
        {
            if (   (u32Eflags & X86_EFL_TF)
                && !(u64DebugCtlMsr & RT_BIT_64(1)))    /* Bit 1 is IA32_DEBUGCTL.BTF. */
            {
                /* Bit 14 is PendingDebug.BS. */
                HMVMX_CHECK_BREAK(u32Val & RT_BIT(14), VMX_IGS_PENDING_DEBUG_XCPT_BS_NOT_SET);
            }
            if (   !(u32Eflags & X86_EFL_TF)
                || (u64DebugCtlMsr & RT_BIT_64(1)))     /* Bit 1 is IA32_DEBUGCTL.BTF. */
            {
                /* Bit 14 is PendingDebug.BS. */
                HMVMX_CHECK_BREAK(!(u32Val & RT_BIT(14)), VMX_IGS_PENDING_DEBUG_XCPT_BS_NOT_CLEAR);
            }
        }

        /* VMCS link pointer. */
        rc = VMXReadVmcs64(VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL, &u64Val);
        AssertRCBreak(rc);
        if (u64Val != UINT64_C(0xffffffffffffffff))
        {
            HMVMX_CHECK_BREAK(!(u64Val & 0xfff), VMX_IGS_VMCS_LINK_PTR_RESERVED);
            /** @todo Bits beyond the processor's physical-address width MBZ. */
            /** @todo 32-bit located in memory referenced by value of this field (as a
             *        physical address) must contain the processor's VMCS revision ID. */
            /** @todo SMM checks. */
        }

        /** @todo Checks on Guest Page-Directory-Pointer-Table Entries when guest is
         *        not using Nested Paging? */
        if (   pVM->hm.s.fNestedPaging
            && !fLongModeGuest
            && CPUMIsGuestInPAEModeEx(pCtx))
        {
            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE0_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & X86_PDPE_PAE_MBZ_MASK), VMX_IGS_PAE_PDPTE_RESERVED);

            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE1_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & X86_PDPE_PAE_MBZ_MASK), VMX_IGS_PAE_PDPTE_RESERVED);

            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE2_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & X86_PDPE_PAE_MBZ_MASK), VMX_IGS_PAE_PDPTE_RESERVED);

            rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE3_FULL, &u64Val);
            AssertRCBreak(rc);
            HMVMX_CHECK_BREAK(!(u64Val & X86_PDPE_PAE_MBZ_MASK), VMX_IGS_PAE_PDPTE_RESERVED);
        }

        /* Shouldn't happen but distinguish it from AssertRCBreak() errors. */
        if (uError == VMX_IGS_ERROR)
            uError = VMX_IGS_REASON_NOT_FOUND;
    } while (0);

    pVCpu->hm.s.u32HMError = uError;
    return uError;

#undef HMVMX_ERROR_BREAK
#undef HMVMX_CHECK_BREAK
}

/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- VM-exit handlers -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

/** @name VM-exit handlers.
 * @{
 */

/**
 * VM-exit handler for external interrupts (VMX_EXIT_EXT_INT).
 */
HMVMX_EXIT_DECL hmR0VmxExitExtInt(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitExtInt);
    /* Windows hosts (32-bit and 64-bit) have DPC latency issues. See @bugref{6853}. */
    if (VMMR0ThreadCtxHookIsEnabled(pVCpu))
        return VINF_SUCCESS;
    return VINF_EM_RAW_INTERRUPT;
}


/**
 * VM-exit handler for exceptions or NMIs (VMX_EXIT_XCPT_OR_NMI).
 */
HMVMX_EXIT_DECL hmR0VmxExitXcptOrNmi(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitXcptNmi, y3);

    int rc = hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
    AssertRCReturn(rc, rc);

    uint32_t uIntType = VMX_EXIT_INTERRUPTION_INFO_TYPE(pVmxTransient->uExitIntInfo);
    Assert(   !(pVCpu->hm.s.vmx.u32ExitCtls & VMX_VMCS_CTRL_EXIT_ACK_EXT_INT)
           && uIntType != VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT);
    Assert(VMX_EXIT_INTERRUPTION_INFO_IS_VALID(pVmxTransient->uExitIntInfo));

    if (uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI)
    {
        /*
         * This cannot be a guest NMI as the only way for the guest to receive an NMI is if we
         * injected it ourselves and anything we inject is not going to cause a VM-exit directly
         * for the event being injected[1]. Go ahead and dispatch the NMI to the host[2].
         *
         * [1] -- See Intel spec. 27.2.3 "Information for VM Exits During Event Delivery".
         * [2] -- See Intel spec. 27.5.5 "Updating Non-Register State".
         */
        VMXDispatchHostNmi();
        STAM_REL_COUNTER_INC(&pVCpu->hm.s.StatExitHostNmiInGC);
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
        return VINF_SUCCESS;
    }

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    VBOXSTRICTRC rcStrictRc1 = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pVmxTransient);
    if (RT_UNLIKELY(rcStrictRc1 == VINF_SUCCESS))
    { /* likely */ }
    else
    {
        if (rcStrictRc1 == VINF_HM_DOUBLE_FAULT)
            rcStrictRc1 = VINF_SUCCESS;
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
        return rcStrictRc1;
    }

    uint32_t uExitIntInfo = pVmxTransient->uExitIntInfo;
    uint32_t uVector      = VMX_EXIT_INTERRUPTION_INFO_VECTOR(uExitIntInfo);
    switch (uIntType)
    {
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_PRIV_SW_XCPT:  /* Privileged software exception. (#DB from ICEBP) */
            Assert(uVector == X86_XCPT_DB);
            RT_FALL_THRU();
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT:       /* Software exception. (#BP or #OF) */
            Assert(uVector == X86_XCPT_BP || uVector == X86_XCPT_OF || uIntType == VMX_EXIT_INTERRUPTION_INFO_TYPE_PRIV_SW_XCPT);
            RT_FALL_THRU();
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT:
        {
            /*
             * If there's any exception caused as a result of event injection, the resulting
             * secondary/final execption will be pending, we shall continue guest execution
             * after injecting the event. The page-fault case is complicated and we manually
             * handle any currently pending event in hmR0VmxExitXcptPF.
             */
            if (!pVCpu->hm.s.Event.fPending)
            { /* likely */ }
            else if (uVector != X86_XCPT_PF)
            {
                rc = VINF_SUCCESS;
                break;
            }

            switch (uVector)
            {
                case X86_XCPT_PF: rc = hmR0VmxExitXcptPF(pVCpu, pVmxTransient);      break;
                case X86_XCPT_GP: rc = hmR0VmxExitXcptGP(pVCpu, pVmxTransient);      break;
                case X86_XCPT_MF: rc = hmR0VmxExitXcptMF(pVCpu, pVmxTransient);      break;
                case X86_XCPT_DB: rc = hmR0VmxExitXcptDB(pVCpu, pVmxTransient);      break;
                case X86_XCPT_BP: rc = hmR0VmxExitXcptBP(pVCpu, pVmxTransient);      break;
                case X86_XCPT_AC: rc = hmR0VmxExitXcptAC(pVCpu, pVmxTransient);      break;

                case X86_XCPT_NM: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestNM);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                case X86_XCPT_XF: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestXF);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                case X86_XCPT_DE: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestDE);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                case X86_XCPT_UD: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestUD);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                case X86_XCPT_SS: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestSS);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                case X86_XCPT_NP: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestNP);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                case X86_XCPT_TS: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestTS);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pVmxTransient); break;
                default:
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestXcpUnk);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                    {
                        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.pRealModeTSS);
                        Assert(PDMVmmDevHeapIsEnabled(pVCpu->CTX_SUFF(pVM)));
                        Assert(CPUMIsGuestInRealModeEx(&pVCpu->cpum.GstCtx));

                        rc  = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR0);
                        rc |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
                        rc |= hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
                        AssertRCReturn(rc, rc);
                        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(uExitIntInfo),
                                               pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode,
                                               0 /* GCPtrFaultAddress */);
                    }
                    else
                    {
                        AssertMsgFailed(("Unexpected VM-exit caused by exception %#x\n", uVector));
                        pVCpu->hm.s.u32HMError = uVector;
                        rc = VERR_VMX_UNEXPECTED_EXCEPTION;
                    }
                    break;
                }
            }
            break;
        }

        default:
        {
            pVCpu->hm.s.u32HMError = uExitIntInfo;
            rc = VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_TYPE;
            AssertMsgFailed(("Unexpected interruption info %#x\n", VMX_EXIT_INTERRUPTION_INFO_TYPE(uExitIntInfo)));
            break;
        }
    }
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
    return rc;
}


/**
 * VM-exit handler for interrupt-window exiting (VMX_EXIT_INT_WINDOW).
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitIntWindow(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /* Indicate that we no longer need to VM-exit when the guest is ready to receive interrupts, it is now ready. */
    hmR0VmxClearIntWindowExitVmcs(pVCpu);

    /* Deliver the pending interrupts via hmR0VmxEvaluatePendingEvent() and resume guest execution. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIntWindow);
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for NMI-window exiting (VMX_EXIT_NMI_WINDOW).
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitNmiWindow(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    if (RT_UNLIKELY(!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_NMI_WINDOW_EXIT)))
    {
        AssertMsgFailed(("Unexpected NMI-window exit.\n"));
        HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
    }

    Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_BLOCK_NMIS));

    /*
     * If block-by-STI is set when we get this VM-exit, it means the CPU doesn't block NMIs following STI.
     * It is therefore safe to unblock STI and deliver the NMI ourselves. See @bugref{7445}.
     */
    uint32_t fIntrState = 0;
    int rc = VMXReadVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &fIntrState);
    AssertRCReturn(rc, rc);

    bool const fBlockSti = RT_BOOL(fIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);
    if (   fBlockSti
        && VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
    }

    /* Indicate that we no longer need to VM-exit when the guest is ready to receive NMIs, it is now ready */
    hmR0VmxClearNmiWindowExitVmcs(pVCpu);

    /* Deliver the pending NMI via hmR0VmxEvaluatePendingEvent() and resume guest execution. */
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for WBINVD (VMX_EXIT_WBINVD). Conditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitWbinvd(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    return hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for INVD (VMX_EXIT_INVD). Unconditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitInvd(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    return hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for CPUID (VMX_EXIT_CPUID). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitCpuid(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /*
     * Get the state we need and update the exit history entry.
     */
    int rc = hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict;
    PCEMEXITREC pExitRec = EMHistoryUpdateFlagsAndTypeAndPC(pVCpu,
                                                            EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_CPUID),
                                                            pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base);
    if (!pExitRec)
    {
        /*
         * Regular CPUID instruction execution.
         */
        rcStrict = IEMExecDecodedCpuid(pVCpu, pVmxTransient->cbInstr);
        if (rcStrict == VINF_SUCCESS)
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_RAX
                                                     | HM_CHANGED_GUEST_RCX | HM_CHANGED_GUEST_RDX    | HM_CHANGED_GUEST_RBX);
        else if (rcStrict == VINF_IEM_RAISED_XCPT)
        {
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
            rcStrict = VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * Frequent exit or something needing probing.  Get state and call EMHistoryExec.
         */
        int rc2 = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        AssertRCReturn(rc2, rc2);

        Log4(("CpuIdExit/%u: %04x:%08RX64: %#x/%#x -> EMHistoryExec\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, pVCpu->cpum.GstCtx.eax, pVCpu->cpum.GstCtx.ecx));

        rcStrict = EMHistoryExec(pVCpu, pExitRec, 0);
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);

        Log4(("CpuIdExit/%u: %04x:%08RX64: EMHistoryExec -> %Rrc + %04x:%08RX64\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    }
    return rcStrict;
}


/**
 * VM-exit handler for GETSEC (VMX_EXIT_GETSEC). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitGetsec(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    int rc  = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR4);
    AssertRCReturn(rc, rc);

    if (pVCpu->cpum.GstCtx.cr4 & X86_CR4_SMXE)
        return VINF_EM_RAW_EMULATE_INSTR;

    AssertMsgFailed(("hmR0VmxExitGetsec: unexpected VM-exit when CR4.SMXE is 0.\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for RDTSC (VMX_EXIT_RDTSC). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdtsc(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    int rc = hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict = IEMExecDecodedRdtsc(pVCpu, pVmxTransient->cbInstr);
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    {
        /* If we get a spurious VM-exit when offsetting is enabled,
           we must reset offsetting on VM-reentry. See @bugref{6634}. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TSC_OFFSETTING)
            pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS
                                                 | HM_CHANGED_GUEST_RAX | HM_CHANGED_GUEST_RDX);
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
        rcStrict = VINF_SUCCESS;
    }
    return rcStrict;
}


/**
 * VM-exit handler for RDTSCP (VMX_EXIT_RDTSCP). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdtscp(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    int rc = hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_TSC_AUX);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict = IEMExecDecodedRdtscp(pVCpu, pVmxTransient->cbInstr);
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    {
        /* If we get a spurious VM-exit when offsetting is enabled,
           we must reset offsetting on VM-reentry. See @bugref{6634}. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TSC_OFFSETTING)
            pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS
                                                 | HM_CHANGED_GUEST_RAX | HM_CHANGED_GUEST_RDX | HM_CHANGED_GUEST_RCX);
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
        rcStrict = VINF_SUCCESS;
    }
    return rcStrict;
}


/**
 * VM-exit handler for RDPMC (VMX_EXIT_RDPMC). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdpmc(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR4 | CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_SS);
    AssertRCReturn(rc, rc);

    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    rc = EMInterpretRdpmc(pVM, pVCpu, CPUMCTX2CORE(pCtx));
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
        Assert(pVmxTransient->cbInstr == 2);
    }
    else
    {
        AssertMsgFailed(("hmR0VmxExitRdpmc: EMInterpretRdpmc failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    return rc;
}


/**
 * VM-exit handler for VMCALL (VMX_EXIT_VMCALL). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitVmcall(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    VBOXSTRICTRC rcStrict = VERR_VMX_IPE_3;
    if (EMAreHypercallInstructionsEnabled(pVCpu))
    {
        int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_RIP | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_SS
                                              | CPUMCTX_EXTRN_CS  | CPUMCTX_EXTRN_EFER);
        AssertRCReturn(rc, rc);

        /* Perform the hypercall. */
        rcStrict = GIMHypercall(pVCpu, &pVCpu->cpum.GstCtx);
        if (rcStrict == VINF_SUCCESS)
        {
            rc = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
            AssertRCReturn(rc, rc);
        }
        else
            Assert(   rcStrict == VINF_GIM_R3_HYPERCALL
                   || rcStrict == VINF_GIM_HYPERCALL_CONTINUING
                   || RT_FAILURE(rcStrict));

        /* If the hypercall changes anything other than guest's general-purpose registers,
           we would need to reload the guest changed bits here before VM-entry. */
    }
    else
        Log4Func(("Hypercalls not enabled\n"));

    /* If hypercalls are disabled or the hypercall failed for some reason, raise #UD and continue. */
    if (RT_FAILURE(rcStrict))
    {
        hmR0VmxSetPendingXcptUD(pVCpu);
        rcStrict = VINF_SUCCESS;
    }

    return rcStrict;
}


/**
 * VM-exit handler for INVLPG (VMX_EXIT_INVLPG). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitInvlpg(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    Assert(!pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging || pVCpu->hm.s.fUsingDebugLoop);

    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_MEM_MASK | CPUMCTX_EXTRN_DS);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict = IEMExecDecodedInvlpg(pVCpu, pVmxTransient->cbInstr, pVmxTransient->uExitQualification);

    if (rcStrict == VINF_SUCCESS || rcStrict == VINF_PGM_SYNC_CR3)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS);
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
        rcStrict = VINF_SUCCESS;
    }
    else
        AssertMsgFailed(("Unexpected IEMExecDecodedInvlpg(%#RX64) sttus: %Rrc\n",
                         pVmxTransient->uExitQualification, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VM-exit handler for MONITOR (VMX_EXIT_MONITOR). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMonitor(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_SS);
    AssertRCReturn(rc, rc);

    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    rc = EMInterpretMonitor(pVM, pVCpu, CPUMCTX2CORE(pCtx));
    if (RT_LIKELY(rc == VINF_SUCCESS))
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
    else
    {
        AssertMsg(rc == VERR_EM_INTERPRETER, ("hmR0VmxExitMonitor: EMInterpretMonitor failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMonitor);
    return rc;
}


/**
 * VM-exit handler for MWAIT (VMX_EXIT_MWAIT). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMwait(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_RFLAGS | CPUMCTX_EXTRN_SS);
    AssertRCReturn(rc, rc);

    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    VBOXSTRICTRC rc2 = EMInterpretMWait(pVM, pVCpu, CPUMCTX2CORE(pCtx));
    rc = VBOXSTRICTRC_VAL(rc2);
    if (RT_LIKELY(   rc == VINF_SUCCESS
                  || rc == VINF_EM_HALT))
    {
        int rc3 = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
        AssertRCReturn(rc3, rc3);

        if (   rc == VINF_EM_HALT
            && EMMonitorWaitShouldContinue(pVCpu, pCtx))
            rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsg(rc == VERR_EM_INTERPRETER, ("hmR0VmxExitMwait: EMInterpretMWait failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    AssertMsg(rc == VINF_SUCCESS || rc == VINF_EM_HALT || rc == VERR_EM_INTERPRETER,
              ("hmR0VmxExitMwait: failed, invalid error code %Rrc\n", rc));
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMwait);
    return rc;
}


/**
 * VM-exit handler for RSM (VMX_EXIT_RSM). Unconditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitRsm(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    /*
     * Execution of RSM outside of SMM mode causes #UD regardless of VMX root or VMX non-root
     * mode. In theory, we should never get this VM-exit. This can happen only if dual-monitor
     * treatment of SMI and VMX is enabled, which can (only?) be done by executing VMCALL in
     * VMX root operation. If we get here, something funny is going on.
     *
     * See Intel spec. 33.15.5 "Enabling the Dual-Monitor Treatment".
     */
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    AssertMsgFailed(("Unexpected RSM VM-exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for SMI (VMX_EXIT_SMI). Unconditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitSmi(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    /*
     * This can only happen if we support dual-monitor treatment of SMI, which can be activated
     * by executing VMCALL in VMX root operation. Only an STM (SMM transfer monitor) would get
     * this VM-exit when we (the executive monitor) execute a VMCALL in VMX root mode or receive
     * an SMI. If we get here, something funny is going on.
     *
     * See Intel spec. 33.15.6 "Activating the Dual-Monitor Treatment"
     * See Intel spec. 25.3 "Other Causes of VM-Exits"
     */
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    AssertMsgFailed(("Unexpected SMI VM-exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for IO SMI (VMX_EXIT_IO_SMI). Unconditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitIoSmi(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    /* Same treatment as VMX_EXIT_SMI. See comment in hmR0VmxExitSmi(). */
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    AssertMsgFailed(("Unexpected IO SMI VM-exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for SIPI (VMX_EXIT_SIPI). Conditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitSipi(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    /*
     * SIPI exits can only occur in VMX non-root operation when the "wait-for-SIPI" guest activity state is used.
     * We don't make use of it as our guests don't have direct access to the host LAPIC.
     * See Intel spec. 25.3 "Other Causes of VM-exits".
     */
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    AssertMsgFailed(("Unexpected SIPI VM-exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for INIT signal (VMX_EXIT_INIT_SIGNAL). Unconditional
 * VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitInitSignal(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    /*
     * INIT signals are blocked in VMX root operation by VMXON and by SMI in SMM.
     * See Intel spec. 33.14.1 Default Treatment of SMI Delivery" and Intel spec. 29.3 "VMX Instructions" for "VMXON".
     *
     * It is -NOT- blocked in VMX non-root operation so we can, in theory, still get these VM-exits.
     * See Intel spec. "23.8 Restrictions on VMX operation".
     */
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for triple faults (VMX_EXIT_TRIPLE_FAULT). Unconditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitTripleFault(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    return VINF_EM_RESET;
}


/**
 * VM-exit handler for HLT (VMX_EXIT_HLT). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitHlt(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT);

    int rc = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_RFLAGS);
    AssertRCReturn(rc, rc);

    if (EMShouldContinueAfterHalt(pVCpu, &pVCpu->cpum.GstCtx))    /* Requires eflags. */
        rc = VINF_SUCCESS;
    else
        rc = VINF_EM_HALT;

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitHlt);
    if (rc != VINF_SUCCESS)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHltToR3);
    return rc;
}


/**
 * VM-exit handler for instructions that result in a \#UD exception delivered to
 * the guest.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitSetPendingXcptUD(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    hmR0VmxSetPendingXcptUD(pVCpu);
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for expiry of the VMX preemption timer.
 */
HMVMX_EXIT_DECL hmR0VmxExitPreemptTimer(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /* If the preemption-timer has expired, reinitialize the preemption timer on next VM-entry. */
    pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;

    /* If there are any timer events pending, fall back to ring-3, otherwise resume guest execution. */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    bool fTimersPending = TMTimerPollBool(pVM, pVCpu);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitPreemptTimer);
    return fTimersPending ? VINF_EM_RAW_TIMER_PENDING : VINF_SUCCESS;
}


/**
 * VM-exit handler for XSETBV (VMX_EXIT_XSETBV). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitXsetbv(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    int rc = hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_CR4);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict = IEMExecDecodedXsetbv(pVCpu, pVmxTransient->cbInstr);
    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, rcStrict != VINF_IEM_RAISED_XCPT ? HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS
                                                                                : HM_CHANGED_XCPT_RAISED_MASK);

    PCCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    pVCpu->hm.s.fLoadSaveGuestXcr0 = (pCtx->cr4 & X86_CR4_OSXSAVE) && pCtx->aXcr[0] != ASMGetXcr0();

    return rcStrict;
}


/**
 * VM-exit handler for INVPCID (VMX_EXIT_INVPCID). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitInvpcid(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    /** @todo Use VM-exit instruction information. */
    return VERR_EM_INTERPRETER;
}


/**
 * VM-exit handler for invalid-guest-state (VMX_EXIT_ERR_INVALID_GUEST_STATE).
 * Error VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitErrInvalidGuestState(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    int rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
    AssertRCReturn(rc, rc);
    rc = hmR0VmxCheckVmcsCtls(pVCpu);
    if (RT_FAILURE(rc))
        return rc;

    uint32_t uInvalidReason = hmR0VmxCheckGuestState(pVCpu);
    NOREF(uInvalidReason);

#ifdef VBOX_STRICT
    uint32_t       fIntrState;
    RTHCUINTREG    uHCReg;
    uint64_t       u64Val;
    uint32_t       u32Val;

    rc  = hmR0VmxReadEntryIntInfoVmcs(pVmxTransient);
    rc |= hmR0VmxReadEntryXcptErrorCodeVmcs(pVmxTransient);
    rc |= hmR0VmxReadEntryInstrLenVmcs(pVmxTransient);
    rc |= VMXReadVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &fIntrState);
    AssertRCReturn(rc, rc);

    Log4(("uInvalidReason                             %u\n", uInvalidReason));
    Log4(("VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO    %#RX32\n", pVmxTransient->uEntryIntInfo));
    Log4(("VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE    %#RX32\n", pVmxTransient->uEntryXcptErrorCode));
    Log4(("VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH         %#RX32\n", pVmxTransient->cbEntryInstr));
    Log4(("VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE    %#RX32\n", fIntrState));

    rc = VMXReadVmcs32(VMX_VMCS_GUEST_CR0, &u32Val);                        AssertRC(rc);
    Log4(("VMX_VMCS_GUEST_CR0                         %#RX32\n", u32Val));
    rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR0_MASK, &uHCReg);                  AssertRC(rc);
    Log4(("VMX_VMCS_CTRL_CR0_MASK                     %#RHr\n", uHCReg));
    rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR0_READ_SHADOW, &uHCReg);           AssertRC(rc);
    Log4(("VMX_VMCS_CTRL_CR4_READ_SHADOW              %#RHr\n", uHCReg));
    rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR4_MASK, &uHCReg);                  AssertRC(rc);
    Log4(("VMX_VMCS_CTRL_CR4_MASK                     %#RHr\n", uHCReg));
    rc = VMXReadVmcsHstN(VMX_VMCS_CTRL_CR4_READ_SHADOW, &uHCReg);           AssertRC(rc);
    Log4(("VMX_VMCS_CTRL_CR4_READ_SHADOW              %#RHr\n", uHCReg));
    rc = VMXReadVmcs64(VMX_VMCS64_CTRL_EPTP_FULL, &u64Val);                 AssertRC(rc);
    Log4(("VMX_VMCS64_CTRL_EPTP_FULL                  %#RX64\n", u64Val));

    hmR0DumpRegs(pVCpu);
#else
    NOREF(pVmxTransient);
#endif

    return VERR_VMX_INVALID_GUEST_STATE;
}


/**
 * VM-exit handler for VM-entry failure due to an MSR-load
 * (VMX_EXIT_ERR_MSR_LOAD). Error VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitErrMsrLoad(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    AssertMsgFailed(("Unexpected MSR-load exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for VM-entry failure due to a machine-check event
 * (VMX_EXIT_ERR_MACHINE_CHECK). Error VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitErrMachineCheck(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    AssertMsgFailed(("Unexpected machine-check event exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for all undefined reasons. Should never ever happen.. in
 * theory.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitErrUndefined(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    RT_NOREF2(pVCpu, pVmxTransient);
    AssertMsgFailed(("Huh!? Undefined VM-exit reason %d\n", pVmxTransient->uExitReason));
    return VERR_VMX_UNDEFINED_EXIT_CODE;
}


/**
 * VM-exit handler for XDTR (LGDT, SGDT, LIDT, SIDT) accesses
 * (VMX_EXIT_XDTR_ACCESS) and LDT and TR access (LLDT, LTR, SLDT, STR).
 * Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitXdtrAccess(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /* By default, we don't enable VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitXdtrAccess);
    if (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT)
        return VERR_EM_INTERPRETER;
    AssertMsgFailed(("Unexpected XDTR access\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for RDRAND (VMX_EXIT_RDRAND). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdrand(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /* By default, we don't enable VMX_VMCS_CTRL_PROC_EXEC2_RDRAND_EXIT. */
    if (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_RDRAND_EXIT)
        return VERR_EM_INTERPRETER;
    AssertMsgFailed(("Unexpected RDRAND exit\n"));
    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
}


/**
 * VM-exit handler for RDMSR (VMX_EXIT_RDMSR).
 */
HMVMX_EXIT_DECL hmR0VmxExitRdmsr(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /** @todo Optimize this: We currently drag in in the whole MSR state
     * (CPUMCTX_EXTRN_ALL_MSRS) here.  We should optimize this to only get
     * MSRs required.  That would require changes to IEM and possibly CPUM too.
     * (Should probably do it lazy fashion from CPUMAllMsrs.cpp). */
    uint32_t const idMsr = pVCpu->cpum.GstCtx.ecx;  NOREF(idMsr); /* Save it. */
    int rc = hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_ALL_MSRS);
    AssertRCReturn(rc, rc);

    Log4Func(("ecx=%#RX32\n", idMsr));

#ifdef VBOX_STRICT
    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
    {
        if (   hmR0VmxIsAutoLoadStoreGuestMsr(pVCpu, idMsr)
            && idMsr != MSR_K6_EFER)
        {
            AssertMsgFailed(("Unexpected RDMSR for an MSR in the auto-load/store area in the VMCS. ecx=%#RX32\n", idMsr));
            HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
        }
        if (hmR0VmxIsLazyGuestMsr(pVCpu, idMsr))
        {
            VMXMSREXITREAD  enmRead;
            VMXMSREXITWRITE enmWrite;
            int rc2 = hmR0VmxGetMsrPermission(pVCpu, idMsr, &enmRead, &enmWrite);
            AssertRCReturn(rc2, rc2);
            if (enmRead == VMXMSREXIT_PASSTHRU_READ)
            {
                AssertMsgFailed(("Unexpected RDMSR for a passthru lazy-restore MSR. ecx=%#RX32\n", idMsr));
                HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
            }
        }
    }
#endif

    VBOXSTRICTRC rcStrict = IEMExecDecodedRdmsr(pVCpu, pVmxTransient->cbInstr);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdmsr);
    if (rcStrict == VINF_SUCCESS)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS
                                                 | HM_CHANGED_GUEST_RAX | HM_CHANGED_GUEST_RDX);
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
        rcStrict = VINF_SUCCESS;
    }
    else
        AssertMsg(rcStrict == VINF_CPUM_R3_MSR_READ, ("Unexpected IEMExecDecodedRdmsr status: %Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

    return rcStrict;
}


/**
 * VM-exit handler for WRMSR (VMX_EXIT_WRMSR).
 */
HMVMX_EXIT_DECL hmR0VmxExitWrmsr(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /** @todo Optimize this: We currently drag in in the whole MSR state
     * (CPUMCTX_EXTRN_ALL_MSRS) here.  We should optimize this to only get
     * MSRs required.  That would require changes to IEM and possibly CPUM too.
     * (Should probably do it lazy fashion from CPUMAllMsrs.cpp). */
    uint32_t const idMsr = pVCpu->cpum.GstCtx.ecx; /* Save it. */
    int rc = hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK | CPUMCTX_EXTRN_ALL_MSRS);
    AssertRCReturn(rc, rc);

    Log4Func(("ecx=%#RX32 edx:eax=%#RX32:%#RX32\n", idMsr, pVCpu->cpum.GstCtx.edx, pVCpu->cpum.GstCtx.eax));

    VBOXSTRICTRC rcStrict = IEMExecDecodedWrmsr(pVCpu, pVmxTransient->cbInstr);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitWrmsr);

    if (rcStrict == VINF_SUCCESS)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS);

        /* If this is an X2APIC WRMSR access, update the APIC state as well. */
        if (    idMsr == MSR_IA32_APICBASE
            || (   idMsr >= MSR_IA32_X2APIC_START
                && idMsr <= MSR_IA32_X2APIC_END))
        {
            /*
             * We've already saved the APIC related guest-state (TPR) in hmR0VmxPostRunGuest(). When full APIC register
             * virtualization is implemented we'll have to make sure APIC state is saved from the VMCS before IEM changes it.
             */
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_APIC_TPR);
        }
        else if (idMsr == MSR_IA32_TSC)        /* Windows 7 does this during bootup. See @bugref{6398}. */
            pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;
        else if (idMsr == MSR_K6_EFER)
        {
            /*
             * If the guest touches EFER we need to update the VM-Entry and VM-Exit controls as well,
             * even if it is -not- touching bits that cause paging mode changes (LMA/LME). We care about
             * the other bits as well, SCE and NXE. See @bugref{7368}.
             */
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_EFER_MSR | HM_CHANGED_VMX_ENTRY_CTLS
                                                     | HM_CHANGED_VMX_EXIT_CTLS);
        }

        /* Update MSRs that are part of the VMCS and auto-load/store area when MSR-bitmaps are not supported. */
        if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS))
        {
            switch (idMsr)
            {
                case MSR_IA32_SYSENTER_CS:  ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_SYSENTER_CS_MSR);  break;
                case MSR_IA32_SYSENTER_EIP: ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_SYSENTER_EIP_MSR); break;
                case MSR_IA32_SYSENTER_ESP: ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_SYSENTER_ESP_MSR); break;
                case MSR_K8_FS_BASE:        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_FS);               break;
                case MSR_K8_GS_BASE:        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_GS);               break;
                case MSR_K6_EFER:           /* Nothing to do, already handled above. */                                    break;
                default:
                {
                    if (hmR0VmxIsAutoLoadStoreGuestMsr(pVCpu, idMsr))
                        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_VMX_GUEST_AUTO_MSRS);
                    else if (hmR0VmxIsLazyGuestMsr(pVCpu, idMsr))
                        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_VMX_GUEST_LAZY_MSRS);
                    break;
                }
            }
        }
#ifdef VBOX_STRICT
        else
        {
            /* Paranoia. Validate that MSRs in the MSR-bitmaps with write-passthru are not intercepted. */
            switch (idMsr)
            {
                case MSR_IA32_SYSENTER_CS:
                case MSR_IA32_SYSENTER_EIP:
                case MSR_IA32_SYSENTER_ESP:
                case MSR_K8_FS_BASE:
                case MSR_K8_GS_BASE:
                {
                    AssertMsgFailed(("Unexpected WRMSR for an MSR in the VMCS. ecx=%#RX32\n", idMsr));
                    HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
                }

                /* Writes to MSRs in auto-load/store area/swapped MSRs, shouldn't cause VM-exits with MSR-bitmaps. */
                default:
                {
                    if (hmR0VmxIsAutoLoadStoreGuestMsr(pVCpu, idMsr))
                    {
                        /* EFER writes are always intercepted, see hmR0VmxExportGuestMsrs(). */
                        if (idMsr != MSR_K6_EFER)
                        {
                            AssertMsgFailed(("Unexpected WRMSR for an MSR in the auto-load/store area in the VMCS. ecx=%#RX32\n",
                                             idMsr));
                            HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
                        }
                    }

                    if (hmR0VmxIsLazyGuestMsr(pVCpu, idMsr))
                    {
                        VMXMSREXITREAD  enmRead;
                        VMXMSREXITWRITE enmWrite;
                        int rc2 = hmR0VmxGetMsrPermission(pVCpu, idMsr, &enmRead, &enmWrite);
                        AssertRCReturn(rc2, rc2);
                        if (enmWrite == VMXMSREXIT_PASSTHRU_WRITE)
                        {
                            AssertMsgFailed(("Unexpected WRMSR for passthru, lazy-restore MSR. ecx=%#RX32\n", idMsr));
                            HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
                        }
                    }
                    break;
                }
            }
        }
#endif  /* VBOX_STRICT */
    }
    else if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
        rcStrict = VINF_SUCCESS;
    }
    else
        AssertMsg(rcStrict == VINF_CPUM_R3_MSR_WRITE, ("Unexpected IEMExecDecodedWrmsr status: %Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

    return rcStrict;
}


/**
 * VM-exit handler for PAUSE (VMX_EXIT_PAUSE). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitPause(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    /** @todo The guest has likely hit a contended spinlock. We might want to
     *        poke a schedule different guest VCPU. */
    return VINF_EM_RAW_INTERRUPT;
}


/**
 * VM-exit handler for when the TPR value is lowered below the specified
 * threshold (VMX_EXIT_TPR_BELOW_THRESHOLD). Conditional VM-exit.
 */
HMVMX_EXIT_NSRC_DECL hmR0VmxExitTprBelowThreshold(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW);

    /*
     * The TPR shadow would've been synced with the APIC TPR in hmR0VmxPostRunGuest(). We'll re-evaluate
     * pending interrupts and inject them before the next VM-entry so we can just continue execution here.
     */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitTprBelowThreshold);
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for control-register accesses (VMX_EXIT_MOV_CRX). Conditional
 * VM-exit.
 *
 * @retval VINF_SUCCESS when guest execution can continue.
 * @retval VINF_PGM_CHANGE_MODE when shadow paging mode changed, back to ring-3.
 * @retval VINF_PGM_SYNC_CR3 CR3 sync is required, back to ring-3.
 * @retval VERR_EM_INTERPRETER when something unexpected happened, fallback to
 *         interpreter.
 */
HMVMX_EXIT_DECL hmR0VmxExitMovCRx(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitMovCRx, y2);

    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict;
    PVM pVM  = pVCpu->CTX_SUFF(pVM);
    RTGCUINTPTR const uExitQualification = pVmxTransient->uExitQualification;
    uint32_t const uAccessType           = VMX_EXIT_QUAL_CRX_ACCESS(uExitQualification);
    switch (uAccessType)
    {
        case VMX_EXIT_QUAL_CRX_ACCESS_WRITE:       /* MOV to CRx */
        {
            rcStrict = IEMExecDecodedMovCRxWrite(pVCpu, pVmxTransient->cbInstr,
                                                 VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification),
                                                 VMX_EXIT_QUAL_CRX_GENREG(uExitQualification));
            AssertMsg(   rcStrict == VINF_SUCCESS
                      || rcStrict == VINF_IEM_RAISED_XCPT
                      || rcStrict == VINF_PGM_CHANGE_MODE
                      || rcStrict == VINF_PGM_SYNC_CR3, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

            switch (VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification))
            {
                case 0:
                {
                    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged,
                                     HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_CR0);
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR0Write);
                    Log4(("CRX CR0 write rcStrict=%Rrc CR0=%#RX64\n", VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cr0));
                    break;
                }

                case 2:
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR2Write);
                    /* Nothing to do here, CR2 it's not part of the VMCS. */
                    break;
                }

                case 3:
                {
                    Assert(   !pVM->hm.s.fNestedPaging
                           || !CPUMIsGuestPagingEnabledEx(&pVCpu->cpum.GstCtx)
                           || pVCpu->hm.s.fUsingDebugLoop);
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR3Write);
                    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged,
                                     HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_CR3);
                    Log4(("CRX CR3 write rcStrict=%Rrc CR3=%#RX64\n", VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cr3));
                    break;
                }

                case 4:
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR4Write);
                    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged,
                                     HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_CR4);
                    Log4(("CRX CR4 write rc=%Rrc CR4=%#RX64 fLoadSaveGuestXcr0=%u\n", VBOXSTRICTRC_VAL(rcStrict),
                          pVCpu->cpum.GstCtx.cr4, pVCpu->hm.s.fLoadSaveGuestXcr0));
                    break;
                }

                case 8:
                {
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR8Write);
                    Assert(!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW));
                    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged,
                                     HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_APIC_TPR);
                    break;
                }
                default:
                    AssertMsgFailed(("Invalid CRx register %#x\n", VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification)));
                    break;
            }
            break;
        }

        case VMX_EXIT_QUAL_CRX_ACCESS_READ:        /* MOV from CRx */
        {
            Assert(   !pVM->hm.s.fNestedPaging
                   || !CPUMIsGuestPagingEnabledEx(&pVCpu->cpum.GstCtx)
                   || pVCpu->hm.s.fUsingDebugLoop
                   || VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification) != 3);
            /* CR8 reads only cause a VM-exit when the TPR shadow feature isn't enabled. */
            Assert(   VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification) != 8
                   || !(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW));

            rcStrict = IEMExecDecodedMovCRxRead(pVCpu, pVmxTransient->cbInstr,
                                                VMX_EXIT_QUAL_CRX_GENREG(uExitQualification),
                                                VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification));
            AssertMsg(   rcStrict == VINF_SUCCESS
                      || rcStrict == VINF_IEM_RAISED_XCPT, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
#ifdef VBOX_WITH_STATISTICS
            switch (VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification))
            {
                case 0: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR0Read); break;
                case 2: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR2Read); break;
                case 3: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR3Read); break;
                case 4: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR4Read); break;
                case 8: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCR8Read); break;
            }
#endif
            Log4(("CRX CR%d Read access rcStrict=%Rrc\n", VMX_EXIT_QUAL_CRX_REGISTER(uExitQualification),
                  VBOXSTRICTRC_VAL(rcStrict)));
            if (VMX_EXIT_QUAL_CRX_GENREG(uExitQualification) == X86_GREG_xSP)
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_RSP);
            else
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS);
            break;
        }

        case VMX_EXIT_QUAL_CRX_ACCESS_CLTS:        /* CLTS (Clear Task-Switch Flag in CR0) */
        {
            rcStrict = IEMExecDecodedClts(pVCpu, pVmxTransient->cbInstr);
            AssertMsg(   rcStrict == VINF_SUCCESS
                      || rcStrict == VINF_IEM_RAISED_XCPT, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_CR0);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitClts);
            Log4(("CRX CLTS rcStrict=%d\n", VBOXSTRICTRC_VAL(rcStrict)));
            break;
        }

        case VMX_EXIT_QUAL_CRX_ACCESS_LMSW:        /* LMSW (Load Machine-Status Word into CR0) */
        {
            rcStrict = IEMExecDecodedLmsw(pVCpu, pVmxTransient->cbInstr,
                                          VMX_EXIT_QUAL_CRX_LMSW_DATA(uExitQualification));
            AssertMsg(   rcStrict == VINF_SUCCESS
                      || rcStrict == VINF_IEM_RAISED_XCPT
                      || rcStrict == VINF_PGM_CHANGE_MODE,
                      ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));

            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS | HM_CHANGED_GUEST_CR0);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitLmsw);
            Log4(("CRX LMSW rcStrict=%d\n", VBOXSTRICTRC_VAL(rcStrict)));
            break;
        }

        default:
            AssertMsgFailedReturn(("Invalid access-type in Mov CRx VM-exit qualification %#x\n", uAccessType),
                                  VERR_VMX_UNEXPECTED_EXCEPTION);
    }

    Assert(   (pVCpu->hm.s.fCtxChanged & (HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS))
           == (HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS));
    if (rcStrict == VINF_IEM_RAISED_XCPT)
    {
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_XCPT_RAISED_MASK);
        rcStrict = VINF_SUCCESS;
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitMovCRx, y2);
    NOREF(pVM);
    return rcStrict;
}


/**
 * VM-exit handler for I/O instructions (VMX_EXIT_IO_INSTR). Conditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitIoInstr(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitIO, y1);

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK | CPUMCTX_EXTRN_SREG_MASK | CPUMCTX_EXTRN_EFER);
    /* EFER also required for longmode checks in EMInterpretDisasCurrent(), but it's always up-to-date. */
    AssertRCReturn(rc, rc);

    /* Refer Intel spec. 27-5. "Exit Qualifications for I/O Instructions" for the format. */
    uint32_t uIOPort      = VMX_EXIT_QUAL_IO_PORT(pVmxTransient->uExitQualification);
    uint8_t  uIOWidth     = VMX_EXIT_QUAL_IO_WIDTH(pVmxTransient->uExitQualification);
    bool     fIOWrite     = (   VMX_EXIT_QUAL_IO_DIRECTION(pVmxTransient->uExitQualification)
                             == VMX_EXIT_QUAL_IO_DIRECTION_OUT);
    bool     fIOString    = VMX_EXIT_QUAL_IO_IS_STRING(pVmxTransient->uExitQualification);
    bool     fGstStepping = RT_BOOL(pCtx->eflags.Bits.u1TF);
    bool     fDbgStepping = pVCpu->hm.s.fSingleInstruction;
    AssertReturn(uIOWidth <= 3 && uIOWidth != 2, VERR_VMX_IPE_1);

    /*
     * Update exit history to see if this exit can be optimized.
     */
    VBOXSTRICTRC rcStrict;
    PCEMEXITREC  pExitRec = NULL;
    if (   !fGstStepping
        && !fDbgStepping)
        pExitRec = EMHistoryUpdateFlagsAndTypeAndPC(pVCpu,
                                                    !fIOString
                                                    ? !fIOWrite
                                                    ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_READ)
                                                    : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_WRITE)
                                                    : !fIOWrite
                                                    ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_STR_READ)
                                                    : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_IO_PORT_STR_WRITE),
                                                    pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base);
    if (!pExitRec)
    {
        /* I/O operation lookup arrays. */
        static uint32_t const s_aIOSizes[4] = { 1, 2, 0, 4 };                    /* Size of the I/O accesses. */
        static uint32_t const s_aIOOpAnd[4] = { 0xff, 0xffff, 0, 0xffffffff };   /* AND masks for saving result in AL/AX/EAX. */
        uint32_t const cbValue  = s_aIOSizes[uIOWidth];
        uint32_t const cbInstr  = pVmxTransient->cbInstr;
        bool fUpdateRipAlready  = false; /* ugly hack, should be temporary. */
        PVM pVM                 = pVCpu->CTX_SUFF(pVM);
        if (fIOString)
        {
            /*
             * INS/OUTS - I/O String instruction.
             *
             * Use instruction-information if available, otherwise fall back on
             * interpreting the instruction.
             */
            Log4(("CS:RIP=%04x:%08RX64 %#06x/%u %c str\n", pCtx->cs.Sel, pCtx->rip, uIOPort, cbValue,
                  fIOWrite ? 'w' : 'r'));
            AssertReturn(pCtx->dx == uIOPort, VERR_VMX_IPE_2);
            if (MSR_IA32_VMX_BASIC_INFO_VMCS_INS_OUTS(pVM->hm.s.vmx.Msrs.u64BasicInfo))
            {
                int rc2  = hmR0VmxReadExitInstrInfoVmcs(pVmxTransient);
                AssertRCReturn(rc2, rc2);
                AssertReturn(pVmxTransient->ExitInstrInfo.StrIo.u3AddrSize <= 2, VERR_VMX_IPE_3);
                AssertCompile(IEMMODE_16BIT == 0 && IEMMODE_32BIT == 1 && IEMMODE_64BIT == 2);
                IEMMODE const enmAddrMode = (IEMMODE)pVmxTransient->ExitInstrInfo.StrIo.u3AddrSize;
                bool const fRep           = VMX_EXIT_QUAL_IO_IS_REP(pVmxTransient->uExitQualification);
                if (fIOWrite)
                    rcStrict = IEMExecStringIoWrite(pVCpu, cbValue, enmAddrMode, fRep, cbInstr,
                                                    pVmxTransient->ExitInstrInfo.StrIo.iSegReg, true /*fIoChecked*/);
                else
                {
                    /*
                     * The segment prefix for INS cannot be overridden and is always ES. We can safely assume X86_SREG_ES.
                     * Hence "iSegReg" field is undefined in the instruction-information field in VT-x for INS.
                     * See Intel Instruction spec. for "INS".
                     * See Intel spec. Table 27-8 "Format of the VM-Exit Instruction-Information Field as Used for INS and OUTS".
                     */
                    rcStrict = IEMExecStringIoRead(pVCpu, cbValue, enmAddrMode, fRep, cbInstr, true /*fIoChecked*/);
                }
            }
            else
                rcStrict = IEMExecOne(pVCpu);

            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP);
            fUpdateRipAlready = true;
        }
        else
        {
            /*
             * IN/OUT - I/O instruction.
             */
            Log4(("CS:RIP=%04x:%08RX64 %#06x/%u %c\n", pCtx->cs.Sel, pCtx->rip, uIOPort, cbValue,
                  fIOWrite ? 'w' : 'r'));
            uint32_t const uAndVal = s_aIOOpAnd[uIOWidth];
            Assert(!VMX_EXIT_QUAL_IO_IS_REP(pVmxTransient->uExitQualification));
            if (fIOWrite)
            {
                rcStrict = IOMIOPortWrite(pVM, pVCpu, uIOPort, pCtx->eax & uAndVal, cbValue);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOWrite);
            }
            else
            {
                uint32_t u32Result = 0;
                rcStrict = IOMIOPortRead(pVM, pVCpu, uIOPort, &u32Result, cbValue);
                if (IOM_SUCCESS(rcStrict))
                {
                    /* Save result of I/O IN instr. in AL/AX/EAX. */
                    pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Result & uAndVal);
                }
                else if (rcStrict == VINF_IOM_R3_IOPORT_READ)
                    HMR0SavePendingIOPortRead(pVCpu, pCtx->rip, pCtx->rip + cbInstr, uIOPort, uAndVal, cbValue);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIORead);
            }
        }

        if (IOM_SUCCESS(rcStrict))
        {
            if (!fUpdateRipAlready)
            {
                hmR0VmxAdvanceGuestRipBy(pVCpu, cbInstr);
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP);
            }

            /*
             * INS/OUTS with REP prefix updates RFLAGS, can be observed with triple-fault guru
             * while booting Fedora 17 64-bit guest.
             *
             * See Intel Instruction reference for REP/REPE/REPZ/REPNE/REPNZ.
             */
            if (fIOString)
            {
                /** @todo Single-step for INS/OUTS with REP prefix? */
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RFLAGS);
            }
            else if (  !fDbgStepping
                     && fGstStepping)
            {
                rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                AssertRCReturn(rc, rc);
            }

            /*
             * If any I/O breakpoints are armed, we need to check if one triggered
             * and take appropriate action.
             * Note that the I/O breakpoint type is undefined if CR4.DE is 0.
             */
            rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_DR7);
            AssertRCReturn(rc, rc);

            /** @todo Optimize away the DBGFBpIsHwIoArmed call by having DBGF tell the
             *  execution engines about whether hyper BPs and such are pending. */
            uint32_t const uDr7 = pCtx->dr[7];
            if (RT_UNLIKELY(   (   (uDr7 & X86_DR7_ENABLED_MASK)
                                && X86_DR7_ANY_RW_IO(uDr7)
                                && (pCtx->cr4 & X86_CR4_DE))
                            || DBGFBpIsHwIoArmed(pVM)))
            {
                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxIoCheck);

                /* We're playing with the host CPU state here, make sure we don't preempt or longjmp. */
                VMMRZCallRing3Disable(pVCpu);
                HM_DISABLE_PREEMPT(pVCpu);

                bool fIsGuestDbgActive = CPUMR0DebugStateMaybeSaveGuest(pVCpu, true /* fDr6 */);

                VBOXSTRICTRC rcStrict2 = DBGFBpCheckIo(pVM, pVCpu, pCtx, uIOPort, cbValue);
                if (rcStrict2 == VINF_EM_RAW_GUEST_TRAP)
                {
                    /* Raise #DB. */
                    if (fIsGuestDbgActive)
                        ASMSetDR6(pCtx->dr[6]);
                    if (pCtx->dr[7] != uDr7)
                        pVCpu->hm.s.fCtxChanged |= HM_CHANGED_GUEST_DR7;

                    hmR0VmxSetPendingXcptDB(pVCpu);
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
        }

#ifdef VBOX_STRICT
        if (rcStrict == VINF_IOM_R3_IOPORT_READ)
            Assert(!fIOWrite);
        else if (rcStrict == VINF_IOM_R3_IOPORT_WRITE || rcStrict == VINF_IOM_R3_IOPORT_COMMIT_WRITE)
            Assert(fIOWrite);
        else
        {
# if 0 /** @todo r=bird: This is missing a bunch of VINF_EM_FIRST..VINF_EM_LAST
           *        statuses, that the VMM device and some others may return. See
           *        IOM_SUCCESS() for guidance. */
            AssertMsg(   RT_FAILURE(rcStrict)
                      || rcStrict == VINF_SUCCESS
                      || rcStrict == VINF_EM_RAW_EMULATE_INSTR
                      || rcStrict == VINF_EM_DBG_BREAKPOINT
                      || rcStrict == VINF_EM_RAW_GUEST_TRAP
                      || rcStrict == VINF_EM_RAW_TO_R3
                      || rcStrict == VINF_TRPM_XCPT_DISPATCHED, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
# endif
        }
#endif
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitIO, y1);
    }
    else
    {
        /*
         * Frequent exit or something needing probing.  Get state and call EMHistoryExec.
         */
        int rc2 = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        AssertRCReturn(rc2, rc2);
        STAM_COUNTER_INC(!fIOString ? fIOWrite ? &pVCpu->hm.s.StatExitIOWrite : &pVCpu->hm.s.StatExitIORead
                         : fIOWrite ? &pVCpu->hm.s.StatExitIOStringWrite : &pVCpu->hm.s.StatExitIOStringRead);
        Log4(("IOExit/%u: %04x:%08RX64: %s%s%s %#x LB %u -> EMHistoryExec\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              VMX_EXIT_QUAL_IO_IS_REP(pVmxTransient->uExitQualification) ? "REP " : "",
              fIOWrite ? "OUT" : "IN", fIOString ? "S" : "", uIOPort, uIOWidth));

        rcStrict = EMHistoryExec(pVCpu, pExitRec, 0);
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);

        Log4(("IOExit/%u: %04x:%08RX64: EMHistoryExec -> %Rrc + %04x:%08RX64\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    }
    return rcStrict;
}


/**
 * VM-exit handler for task switches (VMX_EXIT_TASK_SWITCH). Unconditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitTaskSwitch(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /* Check if this task-switch occurred while delivery an event through the guest IDT. */
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);
    if (VMX_EXIT_QUAL_TASK_SWITCH_TYPE(pVmxTransient->uExitQualification) == VMX_EXIT_QUAL_TASK_SWITCH_TYPE_IDT)
    {
        rc = hmR0VmxReadIdtVectoringInfoVmcs(pVmxTransient);
        AssertRCReturn(rc, rc);
        if (VMX_IDT_VECTORING_INFO_VALID(pVmxTransient->uIdtVectoringInfo))
        {
            uint32_t       uErrCode;
            RTGCUINTPTR    GCPtrFaultAddress;
            uint32_t const uIntType        = VMX_IDT_VECTORING_INFO_TYPE(pVmxTransient->uIdtVectoringInfo);
            uint32_t const uVector         = VMX_IDT_VECTORING_INFO_VECTOR(pVmxTransient->uIdtVectoringInfo);
            bool const     fErrorCodeValid = VMX_IDT_VECTORING_INFO_ERROR_CODE_IS_VALID(pVmxTransient->uIdtVectoringInfo);
            if (fErrorCodeValid)
            {
                rc = hmR0VmxReadIdtVectoringErrorCodeVmcs(pVmxTransient);
                AssertRCReturn(rc, rc);
                uErrCode = pVmxTransient->uIdtVectoringErrorCode;
            }
            else
                uErrCode = 0;

            if (   uIntType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT
                && uVector == X86_XCPT_PF)
                GCPtrFaultAddress = pVCpu->cpum.GstCtx.cr2;
            else
                GCPtrFaultAddress = 0;

            hmR0VmxSetPendingEvent(pVCpu, VMX_ENTRY_INT_INFO_FROM_EXIT_IDT_INFO(pVmxTransient->uIdtVectoringInfo),
                                   0 /* cbInstr */, uErrCode, GCPtrFaultAddress);

            Log4(("Pending event on TaskSwitch uIntType=%#x uVector=%#x\n", uIntType, uVector));
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitTaskSwitch);
            return VINF_EM_RAW_INJECT_TRPM_EVENT;
        }
    }

    /* Fall back to the interpreter to emulate the task-switch. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitTaskSwitch);
    return VERR_EM_INTERPRETER;
}


/**
 * VM-exit handler for monitor-trap-flag (VMX_EXIT_MTF). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMtf(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG);
    pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG;
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
    AssertRCReturn(rc, rc);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMtf);
    return VINF_EM_DBG_STEPPED;
}


/**
 * VM-exit handler for APIC access (VMX_EXIT_APIC_ACCESS). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitApicAccess(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitApicAccess);

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    VBOXSTRICTRC rcStrict1 = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pVmxTransient);
    if (RT_LIKELY(rcStrict1 == VINF_SUCCESS))
    {
        /* For some crazy guest, if an event delivery causes an APIC-access VM-exit, go to instruction emulation. */
        if (RT_UNLIKELY(pVCpu->hm.s.Event.fPending))
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingInterpret);
            return VINF_EM_RAW_INJECT_TRPM_EVENT;
        }
    }
    else
    {
        if (rcStrict1 == VINF_HM_DOUBLE_FAULT)
            rcStrict1 = VINF_SUCCESS;
        return rcStrict1;
    }

    /* IOMMIOPhysHandler() below may call into IEM, save the necessary state. */
    int rc  = hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    rc     |= hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);

    /* See Intel spec. 27-6 "Exit Qualifications for APIC-access VM-exits from Linear Accesses & Guest-Phyiscal Addresses" */
    uint32_t uAccessType = VMX_EXIT_QUAL_APIC_ACCESS_TYPE(pVmxTransient->uExitQualification);
    VBOXSTRICTRC rcStrict2;
    switch (uAccessType)
    {
        case VMX_APIC_ACCESS_TYPE_LINEAR_WRITE:
        case VMX_APIC_ACCESS_TYPE_LINEAR_READ:
        {
            AssertMsg(   !(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
                      || VMX_EXIT_QUAL_APIC_ACCESS_OFFSET(pVmxTransient->uExitQualification) != XAPIC_OFF_TPR,
                      ("hmR0VmxExitApicAccess: can't access TPR offset while using TPR shadowing.\n"));

            RTGCPHYS GCPhys = pVCpu->hm.s.vmx.u64MsrApicBase;   /* Always up-to-date, u64MsrApicBase is not part of the VMCS. */
            GCPhys &= PAGE_BASE_GC_MASK;
            GCPhys += VMX_EXIT_QUAL_APIC_ACCESS_OFFSET(pVmxTransient->uExitQualification);
            PVM pVM = pVCpu->CTX_SUFF(pVM);
            Log4Func(("Linear access uAccessType=%#x GCPhys=%#RGp Off=%#x\n", uAccessType, GCPhys,
                 VMX_EXIT_QUAL_APIC_ACCESS_OFFSET(pVmxTransient->uExitQualification)));

            PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
            rcStrict2 = IOMMMIOPhysHandler(pVM, pVCpu,
                                           uAccessType == VMX_APIC_ACCESS_TYPE_LINEAR_READ ? 0 : X86_TRAP_PF_RW,
                                           CPUMCTX2CORE(pCtx), GCPhys);
            Log4Func(("IOMMMIOPhysHandler returned %Rrc\n", VBOXSTRICTRC_VAL(rcStrict2)));
            if (   rcStrict2 == VINF_SUCCESS
                || rcStrict2 == VERR_PAGE_TABLE_NOT_PRESENT
                || rcStrict2 == VERR_PAGE_NOT_PRESENT)
            {
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS
                                                         | HM_CHANGED_GUEST_APIC_TPR);
                rcStrict2 = VINF_SUCCESS;
            }
            break;
        }

        default:
            Log4Func(("uAccessType=%#x\n", uAccessType));
            rcStrict2 = VINF_EM_RAW_EMULATE_INSTR;
            break;
    }

    if (rcStrict2 != VINF_SUCCESS)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchApicAccessToR3);
    return rcStrict2;
}


/**
 * VM-exit handler for debug-register accesses (VMX_EXIT_MOV_DRX). Conditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMovDRx(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /* We should -not- get this VM-exit if the guest's debug registers were active. */
    if (pVmxTransient->fWasGuestDebugStateActive)
    {
        AssertMsgFailed(("Unexpected MOV DRx exit\n"));
        HMVMX_UNEXPECTED_EXIT_RET(pVCpu, pVmxTransient);
    }

    if (   !pVCpu->hm.s.fSingleInstruction
        && !pVmxTransient->fWasHyperDebugStateActive)
    {
        Assert(!DBGFIsStepping(pVCpu));
        Assert(pVCpu->hm.s.vmx.u32XcptBitmap & RT_BIT_32(X86_XCPT_DB));

        /* Don't intercept MOV DRx any more. */
        pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT;
        int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
        AssertRCReturn(rc, rc);

        /* We're playing with the host CPU state here, make sure we can't preempt or longjmp. */
        VMMRZCallRing3Disable(pVCpu);
        HM_DISABLE_PREEMPT(pVCpu);

        /* Save the host & load the guest debug state, restart execution of the MOV DRx instruction. */
        CPUMR0LoadGuestDebugState(pVCpu, true /* include DR6 */);
        Assert(CPUMIsGuestDebugStateActive(pVCpu) || HC_ARCH_BITS == 32);

        HM_RESTORE_PREEMPT();
        VMMRZCallRing3Enable(pVCpu);

#ifdef VBOX_WITH_STATISTICS
        rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
        AssertRCReturn(rc, rc);
        if (VMX_EXIT_QUAL_DRX_DIRECTION(pVmxTransient->uExitQualification) == VMX_EXIT_QUAL_DRX_DIRECTION_WRITE)
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxWrite);
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxRead);
#endif
        STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxContextSwitch);
        return VINF_SUCCESS;
    }

    /*
     * EMInterpretDRx[Write|Read]() calls CPUMIsGuestIn64BitCode() which requires EFER, CS. EFER is always up-to-date.
     * Update the segment registers and DR7 from the CPU.
     */
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_SREG_MASK | CPUMCTX_EXTRN_DR7);
    AssertRCReturn(rc, rc);
    Log4Func(("CS:RIP=%04x:%08RX64\n", pCtx->cs.Sel, pCtx->rip));

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (VMX_EXIT_QUAL_DRX_DIRECTION(pVmxTransient->uExitQualification) == VMX_EXIT_QUAL_DRX_DIRECTION_WRITE)
    {
        rc = EMInterpretDRxWrite(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                 VMX_EXIT_QUAL_DRX_REGISTER(pVmxTransient->uExitQualification),
                                 VMX_EXIT_QUAL_DRX_GENREG(pVmxTransient->uExitQualification));
        if (RT_SUCCESS(rc))
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_DR7);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxWrite);
    }
    else
    {
        rc = EMInterpretDRxRead(pVM, pVCpu, CPUMCTX2CORE(pCtx),
                                VMX_EXIT_QUAL_DRX_GENREG(pVmxTransient->uExitQualification),
                                VMX_EXIT_QUAL_DRX_REGISTER(pVmxTransient->uExitQualification));
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxRead);
    }

    Assert(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER);
    if (RT_SUCCESS(rc))
    {
        int rc2 = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
        AssertRCReturn(rc2, rc2);
        return VINF_SUCCESS;
    }
    return rc;
}


/**
 * VM-exit handler for EPT misconfiguration (VMX_EXIT_EPT_MISCONFIG).
 * Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitEptMisconfig(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    VBOXSTRICTRC rcStrict1 = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pVmxTransient);
    if (RT_LIKELY(rcStrict1 == VINF_SUCCESS))
    {
        /* If event delivery causes an EPT misconfig (MMIO), go back to instruction emulation as otherwise
           injecting the original pending event would most likely cause the same EPT misconfig VM-exit. */
        if (RT_UNLIKELY(pVCpu->hm.s.Event.fPending))
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatInjectPendingInterpret);
            return VINF_EM_RAW_INJECT_TRPM_EVENT;
        }
    }
    else
    {
        if (rcStrict1 == VINF_HM_DOUBLE_FAULT)
            rcStrict1 = VINF_SUCCESS;
        return rcStrict1;
    }

    /*
     * Get sufficent state and update the exit history entry.
     */
    RTGCPHYS GCPhys;
    int rc = VMXReadVmcs64(VMX_VMCS64_EXIT_GUEST_PHYS_ADDR_FULL, &GCPhys);
    rc    |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rcStrict;
    PCEMEXITREC pExitRec = EMHistoryUpdateFlagsAndTypeAndPC(pVCpu,
                                                            EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM | EMEXIT_F_HM, EMEXITTYPE_MMIO),
                                                            pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base);
    if (!pExitRec)
    {
        /*
         * If we succeed, resume guest execution.
         * If we fail in interpreting the instruction because we couldn't get the guest physical address
         * of the page containing the instruction via the guest's page tables (we would invalidate the guest page
         * in the host TLB), resume execution which would cause a guest page fault to let the guest handle this
         * weird case. See @bugref{6043}.
         */
        PVM      pVM  = pVCpu->CTX_SUFF(pVM);
        PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
        rcStrict = PGMR0Trap0eHandlerNPMisconfig(pVM, pVCpu, PGMMODE_EPT, CPUMCTX2CORE(pCtx), GCPhys, UINT32_MAX);
        Log4(("EPT misconfig at %#RGp RIP=%#RX64 rc=%Rrc\n", GCPhys, pCtx->rip, VBOXSTRICTRC_VAL(rcStrict)));
        if (   rcStrict == VINF_SUCCESS
            || rcStrict == VERR_PAGE_TABLE_NOT_PRESENT
            || rcStrict == VERR_PAGE_NOT_PRESENT)
        {
            /* Successfully handled MMIO operation. */
            ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS
                                                     | HM_CHANGED_GUEST_APIC_TPR);
            rcStrict = VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * Frequent exit or something needing probing.  Get state and call EMHistoryExec.
         */
        int rc2 = hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
        AssertRCReturn(rc2, rc2);

        Log4(("EptMisscfgExit/%u: %04x:%08RX64: %RGp -> EMHistoryExec\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, GCPhys));

        rcStrict = EMHistoryExec(pVCpu, pExitRec, 0);
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);

        Log4(("EptMisscfgExit/%u: %04x:%08RX64: EMHistoryExec -> %Rrc + %04x:%08RX64\n",
              pVCpu->idCpu, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
              VBOXSTRICTRC_VAL(rcStrict), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    }
    return VBOXSTRICTRC_TODO(rcStrict);
}


/**
 * VM-exit handler for EPT violation (VMX_EXIT_EPT_VIOLATION). Conditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitEptViolation(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    VBOXSTRICTRC rcStrict1 = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pVmxTransient);
    if (RT_LIKELY(rcStrict1 == VINF_SUCCESS))
    {
        /* In the unlikely case that the EPT violation happened as a result of delivering an event, log it. */
        if (RT_UNLIKELY(pVCpu->hm.s.Event.fPending))
            Log4Func(("EPT violation with an event pending u64IntInfo=%#RX64\n", pVCpu->hm.s.Event.u64IntInfo));
    }
    else
    {
        if (rcStrict1 == VINF_HM_DOUBLE_FAULT)
            rcStrict1 = VINF_SUCCESS;
        return rcStrict1;
    }

    RTGCPHYS GCPhys;
    int rc  = VMXReadVmcs64(VMX_VMCS64_EXIT_GUEST_PHYS_ADDR_FULL, &GCPhys);
    rc     |= hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc     |= hmR0VmxImportGuestState(pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    AssertRCReturn(rc, rc);

    /* Intel spec. Table 27-7 "Exit Qualifications for EPT violations". */
    AssertMsg(((pVmxTransient->uExitQualification >> 7) & 3) != 2, ("%#RX64", pVmxTransient->uExitQualification));

    RTGCUINT uErrorCode = 0;
    if (pVmxTransient->uExitQualification & VMX_EXIT_QUAL_EPT_INSTR_FETCH)
        uErrorCode |= X86_TRAP_PF_ID;
    if (pVmxTransient->uExitQualification & VMX_EXIT_QUAL_EPT_DATA_WRITE)
        uErrorCode |= X86_TRAP_PF_RW;
    if (pVmxTransient->uExitQualification & VMX_EXIT_QUAL_EPT_ENTRY_PRESENT)
        uErrorCode |= X86_TRAP_PF_P;

    TRPMAssertXcptPF(pVCpu, GCPhys, uErrorCode);


    /* Handle the pagefault trap for the nested shadow table. */
    PVM      pVM  = pVCpu->CTX_SUFF(pVM);
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;

    Log4Func(("EPT violation %#x at %#RX64 ErrorCode %#x CS:RIP=%04x:%08RX64\n", pVmxTransient->uExitQualification, GCPhys,
              uErrorCode, pCtx->cs.Sel, pCtx->rip));

    VBOXSTRICTRC rcStrict2 = PGMR0Trap0eHandlerNestedPaging(pVM, pVCpu, PGMMODE_EPT, uErrorCode, CPUMCTX2CORE(pCtx), GCPhys);
    TRPMResetTrap(pVCpu);

    /* Same case as PGMR0Trap0eHandlerNPMisconfig(). See comment above, @bugref{6043}. */
    if (   rcStrict2 == VINF_SUCCESS
        || rcStrict2 == VERR_PAGE_TABLE_NOT_PRESENT
        || rcStrict2 == VERR_PAGE_NOT_PRESENT)
    {
        /* Successfully synced our nested page tables. */
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitReasonNpf);
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS);
        return VINF_SUCCESS;
    }

    Log4Func(("EPT return to ring-3 rcStrict2=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict2)));
    return rcStrict2;
}

/** @} */

/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= VM-exit exception handlers =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */

/** @name VM-exit exception handlers.
 * @{
 */

/**
 * VM-exit exception handler for \#MF (Math Fault: floating point exception).
 */
static int hmR0VmxExitXcptMF(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestMF);

    int rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CR0);
    AssertRCReturn(rc, rc);

    if (!(pVCpu->cpum.GstCtx.cr0 & X86_CR0_NE))
    {
        /* Convert a #MF into a FERR -> IRQ 13. See @bugref{6117}. */
        rc = PDMIsaSetIrq(pVCpu->CTX_SUFF(pVM), 13, 1, 0 /* uTagSrc */);

        /** @todo r=ramshankar: The Intel spec. does -not- specify that this VM-exit
         *        provides VM-exit instruction length. If this causes problem later,
         *        disassemble the instruction like it's done on AMD-V. */
        int rc2 = hmR0VmxAdvanceGuestRip(pVCpu, pVmxTransient);
        AssertRCReturn(rc2, rc2);
        return rc;
    }

    hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                           pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode, 0 /* GCPtrFaultAddress */);
    return rc;
}


/**
 * VM-exit exception handler for \#BP (Breakpoint exception).
 */
static int hmR0VmxExitXcptBP(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestBP);

    int rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
    AssertRCReturn(rc, rc);

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    rc = DBGFRZTrap03Handler(pVCpu->CTX_SUFF(pVM), pVCpu, CPUMCTX2CORE(pCtx));
    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        rc  = hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
        rc |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
        rc |= hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
        AssertRCReturn(rc, rc);

        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                               pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode, 0 /* GCPtrFaultAddress */);
    }

    Assert(rc == VINF_SUCCESS || rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_EM_DBG_BREAKPOINT);
    return rc;
}


/**
 * VM-exit exception handler for \#AC (alignment check exception).
 */
static int hmR0VmxExitXcptAC(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);

    /*
     * Re-inject it. We'll detect any nesting before getting here.
     */
    int rc = hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    AssertRCReturn(rc, rc);
    Assert(ASMAtomicUoReadU32(&pVmxTransient->fVmcsFieldsRead) & HMVMX_READ_EXIT_INTERRUPTION_INFO);

    hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                           pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode, 0 /* GCPtrFaultAddress */);
    return VINF_SUCCESS;
}


/**
 * VM-exit exception handler for \#DB (Debug exception).
 */
static int hmR0VmxExitXcptDB(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestDB);

    /*
     * Get the DR6-like values from the VM-exit qualification and pass it to DBGF
     * for processing.
     */
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);

    /* Refer Intel spec. Table 27-1. "Exit Qualifications for debug exceptions" for the format. */
    uint64_t uDR6 = X86_DR6_INIT_VAL;
    uDR6         |= (  pVmxTransient->uExitQualification
                     & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3 | X86_DR6_BD | X86_DR6_BS));

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    rc = DBGFRZTrap01Handler(pVCpu->CTX_SUFF(pVM), pVCpu, CPUMCTX2CORE(pCtx), uDR6, pVCpu->hm.s.fSingleInstruction);
    Log6Func(("rc=%Rrc\n", rc));
    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        /*
         * The exception was for the guest.  Update DR6, DR7.GD and
         * IA32_DEBUGCTL.LBR before forwarding it.
         * (See Intel spec. 27.1 "Architectural State before a VM-Exit".)
         */
        VMMRZCallRing3Disable(pVCpu);
        HM_DISABLE_PREEMPT(pVCpu);

        pCtx->dr[6] &= ~X86_DR6_B_MASK;
        pCtx->dr[6] |= uDR6;
        if (CPUMIsGuestDebugStateActive(pVCpu))
            ASMSetDR6(pCtx->dr[6]);

        HM_RESTORE_PREEMPT();
        VMMRZCallRing3Enable(pVCpu);

        rc = hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_DR7);
        AssertRCReturn(rc, rc);

        /* X86_DR7_GD will be cleared if DRx accesses should be trapped inside the guest. */
        pCtx->dr[7] &= ~X86_DR7_GD;

        /* Paranoia. */
        pCtx->dr[7] &= ~X86_DR7_RAZ_MASK;
        pCtx->dr[7] |= X86_DR7_RA1_MASK;

        rc = VMXWriteVmcs32(VMX_VMCS_GUEST_DR7, (uint32_t)pCtx->dr[7]);
        AssertRCReturn(rc, rc);

        /*
         * Raise #DB in the guest.
         *
         * It is important to reflect exactly what the VM-exit gave us (preserving the
         * interruption-type) rather than use hmR0VmxSetPendingXcptDB() as the #DB could've
         * been raised while executing ICEBP (INT1) and not the regular #DB. Thus it may
         * trigger different handling in the CPU (like skipping DPL checks), see @bugref{6398}.
         *
         * Intel re-documented ICEBP/INT1 on May 2018 previously documented as part of
         * Intel 386, see Intel spec. 24.8.3 "VM-Entry Controls for Event Injection".
         */
        rc  = hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
        rc |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
        rc |= hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
        AssertRCReturn(rc, rc);
        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                               pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode, 0 /* GCPtrFaultAddress */);
        return VINF_SUCCESS;
    }

    /*
     * Not a guest trap, must be a hypervisor related debug event then.
     * Update DR6 in case someone is interested in it.
     */
    AssertMsg(rc == VINF_EM_DBG_STEPPED || rc == VINF_EM_DBG_BREAKPOINT, ("%Rrc\n", rc));
    AssertReturn(pVmxTransient->fWasHyperDebugStateActive, VERR_HM_IPE_5);
    CPUMSetHyperDR6(pVCpu, uDR6);

    return rc;
}

/**
 * VM-exit exception handler for \#GP (General-protection exception).
 *
 * @remarks Requires pVmxTransient->uExitIntInfo to be up-to-date.
 */
static int hmR0VmxExitXcptGP(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestGP);

    int rc;
    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
    { /* likely */ }
    else
    {
#ifndef HMVMX_ALWAYS_TRAP_ALL_XCPTS
        Assert(pVCpu->hm.s.fUsingDebugLoop);
#endif
        /* If the guest is not in real-mode or we have unrestricted execution support, reflect #GP to the guest. */
        rc  = hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
        rc |= hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
        rc |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
        rc |= hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
        AssertRCReturn(rc, rc);
        Log4Func(("Gst: CS:RIP %04x:%08RX64 ErrorCode=%#x CR0=%#RX64 CPL=%u TR=%#04x\n", pCtx->cs.Sel, pCtx->rip,
                  pVmxTransient->uExitIntErrorCode, pCtx->cr0, CPUMGetGuestCPL(pVCpu), pCtx->tr.Sel));
        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                               pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode, 0 /* GCPtrFaultAddress */);
        return rc;
    }

    Assert(CPUMIsGuestInRealModeEx(pCtx));
    Assert(!pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fUnrestrictedGuest);

    /* EMInterpretDisasCurrent() requires a lot of the state, save the entire state. */
    rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
    AssertRCReturn(rc, rc);

    PDISCPUSTATE pDis = &pVCpu->hm.s.DisState;
    uint32_t cbOp     = 0;
    PVM pVM           = pVCpu->CTX_SUFF(pVM);
    bool fDbgStepping = pVCpu->hm.s.fSingleInstruction;
    rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
    if (RT_SUCCESS(rc))
    {
        rc = VINF_SUCCESS;
        Assert(cbOp == pDis->cbInstr);
        Log4Func(("Disas OpCode=%u CS:EIP %04x:%04RX64\n", pDis->pCurInstr->uOpcode, pCtx->cs.Sel, pCtx->rip));
        switch (pDis->pCurInstr->uOpcode)
        {
            case OP_CLI:
            {
                pCtx->eflags.Bits.u1IF = 0;
                pCtx->eflags.Bits.u1RF = 0;
                pCtx->rip += pDis->cbInstr;
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS);
                if (   !fDbgStepping
                    && pCtx->eflags.Bits.u1TF)
                {
                    rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                    AssertRCReturn(rc, rc);
                }
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCli);
                break;
            }

            case OP_STI:
            {
                bool fOldIF = pCtx->eflags.Bits.u1IF;
                pCtx->eflags.Bits.u1IF = 1;
                pCtx->eflags.Bits.u1RF = 0;
                pCtx->rip += pDis->cbInstr;
                if (!fOldIF)
                {
                    EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
                    Assert(VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
                }
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS);
                if (   !fDbgStepping
                    && pCtx->eflags.Bits.u1TF)
                {
                    rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                    AssertRCReturn(rc, rc);
                }
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitSti);
                break;
            }

            case OP_HLT:
            {
                rc = VINF_EM_HALT;
                pCtx->rip += pDis->cbInstr;
                pCtx->eflags.Bits.u1RF = 0;
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitHlt);
                break;
            }

            case OP_POPF:
            {
                Log4Func(("POPF CS:EIP %04x:%04RX64\n", pCtx->cs.Sel, pCtx->rip));
                uint32_t cbParm;
                uint32_t uMask;
                bool     fGstStepping = RT_BOOL(pCtx->eflags.Bits.u1TF);
                if (pDis->fPrefix & DISPREFIX_OPSIZE)
                {
                    cbParm = 4;
                    uMask  = 0xffffffff;
                }
                else
                {
                    cbParm = 2;
                    uMask  = 0xffff;
                }

                /* Get the stack pointer & pop the contents of the stack onto Eflags. */
                RTGCPTR   GCPtrStack = 0;
                X86EFLAGS Eflags;
                Eflags.u32 = 0;
                rc = SELMToFlatEx(pVCpu, DISSELREG_SS, CPUMCTX2CORE(pCtx), pCtx->esp & uMask, SELMTOFLAT_FLAGS_CPL0,
                                  &GCPtrStack);
                if (RT_SUCCESS(rc))
                {
                    Assert(sizeof(Eflags.u32) >= cbParm);
                    rc = VBOXSTRICTRC_TODO(PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &Eflags.u32, cbParm, PGMACCESSORIGIN_HM));
                    AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc)); /** @todo allow strict return codes here */
                }
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                Log4Func(("POPF %#x -> %#RX64 mask=%#x RIP=%#RX64\n", Eflags.u, pCtx->rsp, uMask, pCtx->rip));
                pCtx->eflags.u32 = (pCtx->eflags.u32 & ~((X86_EFL_POPF_BITS & uMask) | X86_EFL_RF))
                                 | (Eflags.u32 & X86_EFL_POPF_BITS & uMask);
                pCtx->esp += cbParm;
                pCtx->esp &= uMask;
                pCtx->rip += pDis->cbInstr;
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS);
                /* Generate a pending-debug exception when the guest stepping over POPF regardless of how
                   POPF restores EFLAGS.TF. */
                if (  !fDbgStepping
                    && fGstStepping)
                {
                    rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                    AssertRCReturn(rc, rc);
                }
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitPopf);
                break;
            }

            case OP_PUSHF:
            {
                uint32_t cbParm;
                uint32_t uMask;
                if (pDis->fPrefix & DISPREFIX_OPSIZE)
                {
                    cbParm = 4;
                    uMask  = 0xffffffff;
                }
                else
                {
                    cbParm = 2;
                    uMask  = 0xffff;
                }

                /* Get the stack pointer & push the contents of eflags onto the stack. */
                RTGCPTR GCPtrStack = 0;
                rc = SELMToFlatEx(pVCpu, DISSELREG_SS, CPUMCTX2CORE(pCtx), (pCtx->esp - cbParm) & uMask,
                                  SELMTOFLAT_FLAGS_CPL0, &GCPtrStack);
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                X86EFLAGS Eflags = pCtx->eflags;
                /* The RF & VM bits are cleared on image stored on stack; see Intel Instruction reference for PUSHF. */
                Eflags.Bits.u1RF = 0;
                Eflags.Bits.u1VM = 0;

                rc = VBOXSTRICTRC_TODO(PGMPhysWrite(pVM, (RTGCPHYS)GCPtrStack, &Eflags.u, cbParm, PGMACCESSORIGIN_HM));
                if (RT_UNLIKELY(rc != VINF_SUCCESS))
                {
                    AssertMsgFailed(("%Rrc\n", rc)); /** @todo allow strict return codes here */
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                Log4Func(("PUSHF %#x -> %#RGv\n", Eflags.u, GCPtrStack));
                pCtx->esp -= cbParm;
                pCtx->esp &= uMask;
                pCtx->rip += pDis->cbInstr;
                pCtx->eflags.Bits.u1RF = 0;
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS);
                if (  !fDbgStepping
                    && pCtx->eflags.Bits.u1TF)
                {
                    rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                    AssertRCReturn(rc, rc);
                }
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitPushf);
                break;
            }

            case OP_IRET:
            {
                /** @todo Handle 32-bit operand sizes and check stack limits. See Intel
                 *        instruction reference. */
                RTGCPTR  GCPtrStack    = 0;
                uint32_t uMask         = 0xffff;
                bool     fGstStepping  = RT_BOOL(pCtx->eflags.Bits.u1TF);
                uint16_t aIretFrame[3];
                if (pDis->fPrefix & (DISPREFIX_OPSIZE | DISPREFIX_ADDRSIZE))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                rc = SELMToFlatEx(pVCpu, DISSELREG_SS, CPUMCTX2CORE(pCtx), pCtx->esp & uMask, SELMTOFLAT_FLAGS_CPL0,
                                  &GCPtrStack);
                if (RT_SUCCESS(rc))
                {
                    rc = VBOXSTRICTRC_TODO(PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &aIretFrame[0], sizeof(aIretFrame),
                                                       PGMACCESSORIGIN_HM));
                    AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc)); /** @todo allow strict return codes here */
                }
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                pCtx->eip                = 0;
                pCtx->ip                 = aIretFrame[0];
                pCtx->cs.Sel             = aIretFrame[1];
                pCtx->cs.ValidSel        = aIretFrame[1];
                pCtx->cs.u64Base         = (uint64_t)pCtx->cs.Sel << 4;
                pCtx->eflags.u32         = (pCtx->eflags.u32 & ((UINT32_C(0xffff0000) | X86_EFL_1) & ~X86_EFL_RF))
                                         | (aIretFrame[2] & X86_EFL_POPF_BITS & uMask);
                pCtx->sp                += sizeof(aIretFrame);
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS
                                                         | HM_CHANGED_GUEST_CS);
                /* Generate a pending-debug exception when stepping over IRET regardless of how IRET modifies EFLAGS.TF. */
                if (   !fDbgStepping
                    && fGstStepping)
                {
                    rc = hmR0VmxSetPendingDebugXcptVmcs(pVCpu);
                    AssertRCReturn(rc, rc);
                }
                Log4Func(("IRET %#RX32 to %04x:%04x\n", GCPtrStack, pCtx->cs.Sel, pCtx->ip));
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIret);
                break;
            }

            case OP_INT:
            {
                uint16_t uVector = pDis->Param1.uValue & 0xff;
                hmR0VmxSetPendingIntN(pVCpu, uVector, pDis->cbInstr);
                /* INT clears EFLAGS.TF, we must not set any pending debug exceptions here. */
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitInt);
                break;
            }

            case OP_INTO:
            {
                if (pCtx->eflags.Bits.u1OF)
                {
                    hmR0VmxSetPendingXcptOF(pVCpu, pDis->cbInstr);
                    /* INTO clears EFLAGS.TF, we must not set any pending debug exceptions here. */
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitInt);
                }
                else
                {
                    pCtx->eflags.Bits.u1RF = 0;
                    ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_GUEST_RFLAGS);
                }
                break;
            }

            default:
            {
                pCtx->eflags.Bits.u1RF = 0; /* This is correct most of the time... */
                VBOXSTRICTRC rc2 = EMInterpretInstructionDisasState(pVCpu, pDis, CPUMCTX2CORE(pCtx), 0 /* pvFault */,
                                                                    EMCODETYPE_SUPERVISOR);
                rc = VBOXSTRICTRC_VAL(rc2);
                ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
                /** @todo We have to set pending-debug exceptions here when the guest is
                 *        single-stepping depending on the instruction that was interpreted. */
                Log4Func(("#GP rc=%Rrc\n", rc));
                break;
            }
        }
    }
    else
        rc = VERR_EM_INTERPRETER;

    AssertMsg(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_EM_HALT,
              ("#GP Unexpected rc=%Rrc\n", rc));
    return rc;
}


/**
 * VM-exit exception handler wrapper for generic exceptions. Simply re-injects
 * the exception reported in the VMX transient structure back into the VM.
 *
 * @remarks Requires uExitIntInfo in the VMX transient structure to be
 *          up-to-date.
 */
static int hmR0VmxExitXcptGeneric(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);
#ifndef HMVMX_ALWAYS_TRAP_ALL_XCPTS
    AssertMsg(pVCpu->hm.s.fUsingDebugLoop || pVCpu->hm.s.vmx.RealMode.fRealOnV86Active,
              ("uVector=%#x u32XcptBitmap=%#X32\n",
               VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVmxTransient->uExitIntInfo), pVCpu->hm.s.vmx.u32XcptBitmap));
#endif

    /* Re-inject the exception into the guest. This cannot be a double-fault condition which would have been handled in
       hmR0VmxCheckExitDueToEventDelivery(). */
    int rc = hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVmxTransient);
    AssertRCReturn(rc, rc);
    Assert(ASMAtomicUoReadU32(&pVmxTransient->fVmcsFieldsRead) & HMVMX_READ_EXIT_INTERRUPTION_INFO);

#ifdef DEBUG_ramshankar
    rc |= hmR0VmxImportGuestState(pVCpu, CPUMCTX_EXTRN_CS | CPUMCTX_EXTRN_RIP);
    uint8_t uVector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVmxTransient->uExitIntInfo);
    Log(("hmR0VmxExitXcptGeneric: Reinjecting Xcpt. uVector=%#x cs:rip=%#04x:%#RX64\n", uVector, pCtx->cs.Sel, pCtx->rip));
#endif

    hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                           pVmxTransient->cbInstr, pVmxTransient->uExitIntErrorCode, 0 /* GCPtrFaultAddress */);
    return VINF_SUCCESS;
}


/**
 * VM-exit exception handler for \#PF (Page-fault exception).
 */
static int hmR0VmxExitXcptPF(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS(pVCpu, pVmxTransient);
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitIntInfoVmcs(pVmxTransient);
    rc    |= hmR0VmxReadExitIntErrorCodeVmcs(pVmxTransient);
    AssertRCReturn(rc, rc);

    if (!pVM->hm.s.fNestedPaging)
    { /* likely */ }
    else
    {
#if !defined(HMVMX_ALWAYS_TRAP_ALL_XCPTS) && !defined(HMVMX_ALWAYS_TRAP_PF)
        Assert(pVCpu->hm.s.fUsingDebugLoop);
#endif
        pVCpu->hm.s.Event.fPending = false;                  /* In case it's a contributory or vectoring #PF. */
        if (RT_LIKELY(!pVmxTransient->fVectoringDoublePF))
        {
            hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                                   0 /* cbInstr */, pVmxTransient->uExitIntErrorCode, pVmxTransient->uExitQualification);
        }
        else
        {
            /* A guest page-fault occurred during delivery of a page-fault. Inject #DF. */
            hmR0VmxSetPendingXcptDF(pVCpu);
            Log4Func(("Pending #DF due to vectoring #PF w/ NestedPaging\n"));
        }
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestPF);
        return rc;
    }

    /* If it's a vectoring #PF, emulate injecting the original event injection as PGMTrap0eHandler() is incapable
       of differentiating between instruction emulation and event injection that caused a #PF. See @bugref{6607}. */
    if (pVmxTransient->fVectoringPF)
    {
        Assert(pVCpu->hm.s.Event.fPending);
        return VINF_EM_RAW_INJECT_TRPM_EVENT;
    }

    PCPUMCTX pCtx = &pVCpu->cpum.GstCtx;
    rc = hmR0VmxImportGuestState(pVCpu, HMVMX_CPUMCTX_EXTRN_ALL);
    AssertRCReturn(rc, rc);

    Log4Func(("#PF: cr2=%#RX64 cs:rip=%#04x:%#RX64 uErrCode %#RX32 cr3=%#RX64\n", pVmxTransient->uExitQualification,
              pCtx->cs.Sel, pCtx->rip, pVmxTransient->uExitIntErrorCode, pCtx->cr3));

    TRPMAssertXcptPF(pVCpu, pVmxTransient->uExitQualification, (RTGCUINT)pVmxTransient->uExitIntErrorCode);
    rc = PGMTrap0eHandler(pVCpu, pVmxTransient->uExitIntErrorCode, CPUMCTX2CORE(pCtx),
                          (RTGCPTR)pVmxTransient->uExitQualification);

    Log4Func(("#PF: rc=%Rrc\n", rc));
    if (rc == VINF_SUCCESS)
    {
        /*
         * This is typically a shadow page table sync or a MMIO instruction. But we may have
         * emulated something like LTR or a far jump. Any part of the CPU context may have changed.
         */
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_ALL_GUEST);
        TRPMResetTrap(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPF);
        return rc;
    }

    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        if (!pVmxTransient->fVectoringDoublePF)
        {
            /* It's a guest page fault and needs to be reflected to the guest. */
            uint32_t uGstErrorCode = TRPMGetErrorCode(pVCpu);
            TRPMResetTrap(pVCpu);
            pVCpu->hm.s.Event.fPending = false;                 /* In case it's a contributory #PF. */
            hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntInfo),
                                   0 /* cbInstr */, uGstErrorCode, pVmxTransient->uExitQualification);
        }
        else
        {
            /* A guest page-fault occurred during delivery of a page-fault. Inject #DF. */
            TRPMResetTrap(pVCpu);
            pVCpu->hm.s.Event.fPending = false;     /* Clear pending #PF to replace it with #DF. */
            hmR0VmxSetPendingXcptDF(pVCpu);
            Log4Func(("#PF: Pending #DF due to vectoring #PF\n"));
        }

        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestPF);
        return VINF_SUCCESS;
    }

    TRPMResetTrap(pVCpu);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPFEM);
    return rc;
}

/** @} */

