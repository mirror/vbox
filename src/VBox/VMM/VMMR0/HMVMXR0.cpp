/* $Id$ */
/** @file
 * HM VMX (Intel VT-x) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#include <iprt/asm-amd64-x86.h>
#include <iprt/thread.h>
#include <iprt/string.h>

#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include "HWVMXR0.h"
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/tm.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#ifdef DEBUG_ramshankar
#define HMVMX_SAVE_FULL_GUEST_STATE
#define HMVMX_SYNC_FULL_GUEST_STATE
#define HMVMX_ALWAYS_TRAP_ALL_XCPTS
#define HMVMX_ALWAYS_TRAP_PF
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define HMVMXHCUINTREG                RTHCUINTREG
#if defined(RT_ARCH_AMD64)
# define HMVMX_IS_64BIT_HOST_MODE()   (true)
#elif defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
extern "C" uint32_t g_fVMXIs64bitHost;
# define HMVMX_IS_64BIT_HOST_MODE()   (g_fVMXIs64bitHost != 0)
# undef HMVMXHCUINTREG
# define HMVMXHCUINTREG               uint64_t
#else
# define HMVMX_IS_64BIT_HOST_MODE()   (false)
#endif

/** Use the function table. */
#define HMVMX_USE_FUNCTION_TABLE

/** This bit indicates the segment selector is unusable in VT-x. */
#define HMVMX_SEL_UNUSABLE                       RT_BIT(16)

/** Determine which tagged-TLB flush handler to use. */
#define HMVMX_FLUSH_TAGGED_TLB_EPT_VPID          0
#define HMVMX_FLUSH_TAGGED_TLB_EPT               1
#define HMVMX_FLUSH_TAGGED_TLB_VPID              2
#define HMVMX_FLUSH_TAGGED_TLB_NONE              3

/** @name Updated-guest-state flags.
 * @{ */
#define HMVMX_UPDATED_GUEST_RIP                   RT_BIT(0)
#define HMVMX_UPDATED_GUEST_RSP                   RT_BIT(1)
#define HMVMX_UPDATED_GUEST_RFLAGS                RT_BIT(2)
#define HMVMX_UPDATED_GUEST_CR0                   RT_BIT(3)
#define HMVMX_UPDATED_GUEST_CR3                   RT_BIT(4)
#define HMVMX_UPDATED_GUEST_CR4                   RT_BIT(5)
#define HMVMX_UPDATED_GUEST_GDTR                  RT_BIT(6)
#define HMVMX_UPDATED_GUEST_IDTR                  RT_BIT(7)
#define HMVMX_UPDATED_GUEST_LDTR                  RT_BIT(8)
#define HMVMX_UPDATED_GUEST_TR                    RT_BIT(9)
#define HMVMX_UPDATED_GUEST_SEGMENT_REGS          RT_BIT(10)
#define HMVMX_UPDATED_GUEST_DEBUG                 RT_BIT(11)
#define HMVMX_UPDATED_GUEST_FS_BASE_MSR           RT_BIT(12)
#define HMVMX_UPDATED_GUEST_GS_BASE_MSR           RT_BIT(13)
#define HMVMX_UPDATED_GUEST_SYSENTER_CS_MSR       RT_BIT(14)
#define HMVMX_UPDATED_GUEST_SYSENTER_EIP_MSR      RT_BIT(15)
#define HMVMX_UPDATED_GUEST_SYSENTER_ESP_MSR      RT_BIT(16)
#define HMVMX_UPDATED_GUEST_AUTO_LOAD_STORE_MSRS  RT_BIT(17)
#define HMVMX_UPDATED_GUEST_ACTIVITY_STATE        RT_BIT(18)
#define HMVMX_UPDATED_GUEST_APIC_STATE            RT_BIT(19)
#define HMVMX_UPDATED_GUEST_ALL                   (  HMVMX_UPDATED_GUEST_RIP                   \
                                                   | HMVMX_UPDATED_GUEST_RSP                   \
                                                   | HMVMX_UPDATED_GUEST_RFLAGS                \
                                                   | HMVMX_UPDATED_GUEST_CR0                   \
                                                   | HMVMX_UPDATED_GUEST_CR3                   \
                                                   | HMVMX_UPDATED_GUEST_CR4                   \
                                                   | HMVMX_UPDATED_GUEST_GDTR                  \
                                                   | HMVMX_UPDATED_GUEST_IDTR                  \
                                                   | HMVMX_UPDATED_GUEST_LDTR                  \
                                                   | HMVMX_UPDATED_GUEST_TR                    \
                                                   | HMVMX_UPDATED_GUEST_SEGMENT_REGS          \
                                                   | HMVMX_UPDATED_GUEST_DEBUG                 \
                                                   | HMVMX_UPDATED_GUEST_FS_BASE_MSR           \
                                                   | HMVMX_UPDATED_GUEST_GS_BASE_MSR           \
                                                   | HMVMX_UPDATED_GUEST_SYSENTER_CS_MSR       \
                                                   | HMVMX_UPDATED_GUEST_SYSENTER_EIP_MSR      \
                                                   | HMVMX_UPDATED_GUEST_SYSENTER_ESP_MSR      \
                                                   | HMVMX_UPDATED_GUEST_AUTO_LOAD_STORE_MSRS  \
                                                   | HMVMX_UPDATED_GUEST_ACTIVITY_STATE        \
                                                   | HMVMX_UPDATED_GUEST_APIC_STATE)
/** @} */

/**
 * Flags to skip redundant reads of some common VMCS fields that are not part of
 * the guest-CPU state but are in the transient structure.
 */
#define HMVMX_UPDATED_TRANSIENT_IDT_VECTORING_INFO            RT_BIT(0)
#define HMVMX_UPDATED_TRANSIENT_IDT_VECTORING_ERROR_CODE      RT_BIT(1)
#define HMVMX_UPDATED_TRANSIENT_EXIT_QUALIFICATION            RT_BIT(2)
#define HMVMX_UPDATED_TRANSIENT_EXIT_INSTR_LEN                RT_BIT(3)
#define HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_INFO        RT_BIT(4)
#define HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_ERROR_CODE  RT_BIT(5)

/**
 * Exception bitmap mask for real-mode guests (real-on-v86). We need to intercept all exceptions manually (except #PF).
 * #NM is also handled spearetely, see hmR0VmxLoadGuestControlRegs(). #PF need not be intercepted even in real-mode if
 * we have Nested Paging support.
 */
#define HMVMX_REAL_MODE_XCPT_MASK    (  RT_BIT(X86_XCPT_DE)             | RT_BIT(X86_XCPT_DB)    | RT_BIT(X86_XCPT_NMI)   \
                                      | RT_BIT(X86_XCPT_BP)             | RT_BIT(X86_XCPT_OF)    | RT_BIT(X86_XCPT_BR)    \
                                      | RT_BIT(X86_XCPT_UD)            /* RT_BIT(X86_XCPT_NM) */ | RT_BIT(X86_XCPT_DF)    \
                                      | RT_BIT(X86_XCPT_CO_SEG_OVERRUN) | RT_BIT(X86_XCPT_TS)    | RT_BIT(X86_XCPT_NP)    \
                                      | RT_BIT(X86_XCPT_SS)             | RT_BIT(X86_XCPT_GP)   /* RT_BIT(X86_XCPT_PF) */ \
                                      | RT_BIT(X86_XCPT_MF)             | RT_BIT(X86_XCPT_AC)    | RT_BIT(X86_XCPT_MC)    \
                                      | RT_BIT(X86_XCPT_XF))

/**
 * Exception bitmap mask for all contributory exceptions.
 */
#define HMVMX_CONTRIBUTORY_XCPT_MASK  ( RT_BIT(X86_XCPT_GP) | RT_BIT(X86_XCPT_NP) | RT_BIT(X86_XCPT_SS) | RT_BIT(X86_XCPT_TS) \
                                       | RT_BIT(X86_XCPT_DE))

/** Maximum VM-instruction error number. */
#define HMVMX_INSTR_ERROR_MAX     28

/** Profiling macro. */
#ifdef HM_PROFILE_EXIT_DISPATCH
# define HMVMX_START_EXIT_DISPATCH_PROF() STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitDispatch, ed)
# define HMVMX_STOP_EXIT_DISPATCH_PROF()  STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitDispatch, ed)
#else
# define HMVMX_START_EXIT_DISPATCH_PROF() do { } while (0)
# define HMVMX_STOP_EXIT_DISPATCH_PROF()  do { } while (0)
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** @name VMX transient.
 *
 * A state structure for holding miscellaneous information across
 * VMX non-root operation and restored after the transition.
 *
 * @{ */
typedef struct VMXTRANSIENT
{
    /** The host's rflags/eflags. */
    RTCCUINTREG     uEFlags;
#if HC_ARCH_BITS == 32
    uint32_t        u32Alignment0;
#endif
    /** The guest's LSTAR MSR value used for TPR patching for 32-bit guests. */
    uint64_t        u64LStarMsr;
    /** The guest's TPR value used for TPR shadowing. */
    uint8_t         u8GuestTpr;
    /** Alignment. */
    uint8_t         abAlignment0[6];

    /** The basic VM-exit reason. */
    uint16_t        uExitReason;
    /** Alignment. */
    uint16_t        u16Alignment0;
    /** The VM-exit interruption error code. */
    uint32_t        uExitIntrErrorCode;
    /** The VM-exit exit qualification. */
    uint64_t        uExitQualification;

    /** The VM-exit interruption-information field. */
    uint32_t        uExitIntrInfo;
    /** The VM-exit instruction-length field. */
    uint32_t        cbInstr;
    /** Whether the VM-entry failed or not. */
    bool            fVMEntryFailed;
    /** Alignment. */
    uint8_t         abAlignment1[5];

    /** The VM-entry interruption-information field. */
    uint32_t        uEntryIntrInfo;
    /** The VM-entry exception error code field. */
    uint32_t        uEntryXcptErrorCode;
    /** The VM-entry instruction length field. */
    uint32_t        cbEntryInstr;

    /** IDT-vectoring information field. */
    uint32_t        uIdtVectoringInfo;
    /** IDT-vectoring error code. */
    uint32_t        uIdtVectoringErrorCode;

    /** Mask of currently read VMCS fields; HMVMX_UPDATED_TRANSIENT_*. */
    uint32_t        fVmcsFieldsRead;
    /** Whether TSC-offsetting should be setup before VM-entry. */
    bool            fUpdateTscOffsettingAndPreemptTimer;
    /** Whether the VM-exit was caused by a page-fault during delivery of a
     *  contributary exception or a page-fault. */
    bool            fVectoringPF;
} VMXTRANSIENT, *PVMXTRANSIENT;
AssertCompileMemberAlignment(VMXTRANSIENT, uExitReason,    sizeof(uint64_t));
AssertCompileMemberAlignment(VMXTRANSIENT, uExitIntrInfo,  sizeof(uint64_t));
AssertCompileMemberAlignment(VMXTRANSIENT, uEntryIntrInfo, sizeof(uint64_t));
/** @} */


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


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void               hmR0VmxFlushEpt(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_EPT enmFlush);
static void               hmR0VmxFlushVpid(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_VPID enmFlush, RTGCPTR GCPtr);
static int                hmR0VmxInjectEventVmcs(PVMCPU pVCpu, PCPUMCTX pMixedCtx, uint64_t u64IntrInfo, uint32_t cbInstr,
                                                 uint32_t u32ErrCode, RTGCUINTREG GCPtrFaultAddress, uint32_t *puIntrState);
#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
static int                hmR0VmxInitVmcsReadCache(PVM pVM, PVMCPU pVCpu);
#endif
#ifndef HMVMX_USE_FUNCTION_TABLE
DECLINLINE(int)           hmR0VmxHandleExit(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient, uint32_t rcReason);
#define HMVMX_EXIT_DECL   static int
#else
#define HMVMX_EXIT_DECL   static DECLCALLBACK(int)
#endif

HMVMX_EXIT_DECL hmR0VmxExitXcptNmi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitExtInt(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitTripleFault(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitInitSignal(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitSipi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitIoSmi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitSmi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitIntWindow(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitNmiWindow(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitTaskSwitch(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitCpuid(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitGetsec(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitHlt(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitInvd(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitInvlpg(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitRdpmc(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitRdtsc(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitRsm(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitSetPendingXcptUD(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitMovCRx(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitMovDRx(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitIoInstr(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitRdmsr(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitWrmsr(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitErrInvalidGuestState(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitErrMsrLoad(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitErrUndefined(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitMwait(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitMtf(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitMonitor(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitPause(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitErrMachineCheck(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitTprBelowThreshold(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitApicAccess(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitXdtrAccess(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitXdtrAccess(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitEptViolation(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitEptMisconfig(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitRdtscp(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitPreemptTimer(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitWbinvd(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitXsetbv(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitRdrand(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
HMVMX_EXIT_DECL hmR0VmxExitInvpcid(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);

static int      hmR0VmxExitXcptNM(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
static int      hmR0VmxExitXcptPF(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
static int      hmR0VmxExitXcptMF(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
static int      hmR0VmxExitXcptDB(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
static int      hmR0VmxExitXcptBP(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
static int      hmR0VmxExitXcptGP(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
static int      hmR0VmxExitXcptGeneric(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef HMVMX_USE_FUNCTION_TABLE
/**
 * VM-exit handler.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required
 *                          fields before using them.
 * @param   pVmxTransient   Pointer to the VMX-transient structure.
 */
typedef DECLCALLBACK(int) FNVMEXITHANDLER(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient);
/** Pointer to VM-exit handler. */
typedef FNVMEXITHANDLER *const PFNVMEXITHANDLER;

/**
 * VMX_EXIT dispatch table.
 */
static const PFNVMEXITHANDLER g_apfnVMExitHandlers[VMX_EXIT_MAX + 1] =
{
 /* 00  VMX_EXIT_XCPT_NMI                */  hmR0VmxExitXcptNmi,
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
 /* 18  VMX_EXIT_VMCALL                  */  hmR0VmxExitSetPendingXcptUD,
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
 /* 56  UNDEFINED                        */  hmR0VmxExitErrUndefined,
 /* 57  VMX_EXIT_RDRAND                  */  hmR0VmxExitRdrand,
 /* 58  VMX_EXIT_INVPCID                 */  hmR0VmxExitInvpcid,
 /* 59  VMX_EXIT_VMFUNC                  */  hmR0VmxExitSetPendingXcptUD
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
    /*  7 */ "VM entry with invalid control fields.",
    /*  8 */ "VM entry with invalid host state fields.",
    /*  9 */ "VMPTRLD with invalid physical address.",
    /* 10 */ "VMPTRLD with VMXON pointer.",
    /* 11 */ "VMPTRLD with incorrect revision identifier.",
    /* 12 */ "VMREAD/VMWRITE from/to unsupported VMCS component.",
    /* 13 */ "VMWRITE to read-only VMCS component.",
    /* 14 */ "(Not Used)",
    /* 15 */ "VMXON executed in VMX root operation.",
    /* 16 */ "VM entry with invalid executive-VMCS pointer.",
    /* 17 */ "VM entry with non-launched executing VMCS.",
    /* 18 */ "VM entry with executive-VMCS pointer not VMXON pointer.",
    /* 19 */ "VMCALL with non-clear VMCS.",
    /* 20 */ "VMCALL with invalid VM-exit control fields.",
    /* 21 */ "(Not Used)",
    /* 22 */ "VMCALL with incorrect MSEG revision identifier.",
    /* 23 */ "VMXOFF under dual monitor treatment of SMIs and SMM.",
    /* 24 */ "VMCALL with invalid SMM-monitor features.",
    /* 25 */ "VM entry with invalid VM-execution control fields in executive VMCS.",
    /* 26 */ "VM entry with events blocked by MOV SS.",
    /* 27 */ "(Not Used)",
    /* 28 */ "Invalid operand to INVEPT/INVVPID."
};
#endif /* VBOX_STRICT */



/**
 * Updates the VM's last error record. If there was a VMX instruction error,
 * reads the error data from the VMCS and updates VCPU's last error record as
 * well.
 *
 * @param    pVM        Pointer to the VM.
 * @param    pVCpu      Pointer to the VMCPU (can be NULL if @a rc is not
 *                      VERR_VMX_UNABLE_TO_START_VM or
 *                      VERR_VMX_INVALID_VMCS_FIELD).
 * @param    rc         The error code.
 */
static void hmR0VmxUpdateErrorRecord(PVM pVM, PVMCPU pVCpu, int rc)
{
    AssertPtr(pVM);
    if (   rc == VERR_VMX_INVALID_VMCS_FIELD
        || rc == VERR_VMX_UNABLE_TO_START_VM)
    {
        AssertPtrReturnVoid(pVCpu);
        VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &pVCpu->hm.s.vmx.lasterror.u32InstrError);
    }
    pVM->hm.s.lLastError = rc;
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
DECLINLINE(int) hmR0VmxReadEntryIntrInfoVmcs(PVMXTRANSIENT pVmxTransient)
{
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, &pVmxTransient->uEntryIntrInfo);
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
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxReadEntryInstrLenVmcs(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    int rc = VMXReadVmcs32(VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH, &pVmxTransient->cbEntryInstr);
    AssertRCReturn(rc, rc);
    return VINF_SUCCESS;
}


/**
 * Reads the VM-exit interruption-information field from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitIntrInfoVmcs(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_INFO))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO, &pVmxTransient->uExitIntrInfo);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_INFO;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the VM-exit interruption error code from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitIntrErrorCodeVmcs(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_ERROR_CODE))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INTERRUPTION_ERROR_CODE, &pVmxTransient->uExitIntrErrorCode);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_ERROR_CODE;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the VM-exit instruction length field from the VMCS into the VMX
 * transient structure.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitInstrLenVmcs(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_EXIT_INSTR_LEN))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_INSTR_LENGTH, &pVmxTransient->cbInstr);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_UPDATED_TRANSIENT_EXIT_INSTR_LEN;
    }
    return VINF_SUCCESS;
}


/**
 * Reads the exit qualification from the VMCS into the VMX transient structure.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 */
DECLINLINE(int) hmR0VmxReadExitQualificationVmcs(PVMCPU pVCpu, PVMXTRANSIENT pVmxTransient)
{
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_EXIT_QUALIFICATION))
    {
        int rc = VMXReadVmcsGstN(VMX_VMCS_RO_EXIT_QUALIFICATION, &pVmxTransient->uExitQualification);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_UPDATED_TRANSIENT_EXIT_QUALIFICATION;
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
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_IDT_VECTORING_INFO))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_IDT_INFO, &pVmxTransient->uIdtVectoringInfo);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_UPDATED_TRANSIENT_IDT_VECTORING_INFO;
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
    if (!(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_IDT_VECTORING_ERROR_CODE))
    {
        int rc = VMXReadVmcs32(VMX_VMCS32_RO_IDT_ERROR_CODE, &pVmxTransient->uIdtVectoringErrorCode);
        AssertRCReturn(rc, rc);
        pVmxTransient->fVmcsFieldsRead |= HMVMX_UPDATED_TRANSIENT_IDT_VECTORING_ERROR_CODE;
    }
    return VINF_SUCCESS;
}


/**
 * Enters VMX root mode operation on the current CPU.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to the VM (optional, can be NULL, after
 *                              a resume).
 * @param   HCPhysCpuPage       Physical address of the VMXON region.
 * @param   pvCpuPage           Pointer to the VMXON region.
 */
static int hmR0VmxEnterRootMode(PVM pVM, RTHCPHYS HCPhysCpuPage, void *pvCpuPage)
{
    AssertReturn(HCPhysCpuPage != 0 && HCPhysCpuPage != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(pvCpuPage, VERR_INVALID_PARAMETER);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pVM)
    {
        /* Write the VMCS revision dword to the VMXON region. */
        *(uint32_t *)pvCpuPage = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hm.s.vmx.msr.vmx_basic_info);
    }

    /* Enable the VMX bit in CR4 if necessary. */
    RTCCUINTREG uCr4 = ASMGetCR4();
    if (!(uCr4 & X86_CR4_VMXE))
        ASMSetCR4(uCr4 | X86_CR4_VMXE);

    /* Enter VMX root mode. */
    int rc = VMXEnable(HCPhysCpuPage);
    if (RT_FAILURE(rc))
        ASMSetCR4(uCr4);

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

    /* If we're for some reason not in VMX root mode, then don't leave it. */
    RTCCUINTREG uHostCR4 = ASMGetCR4();
    if (uHostCR4 & X86_CR4_VMXE)
    {
        /* Exit VMX root mode and clear the VMX bit in CR4. */
        VMXDisable();
        ASMSetCR4(uHostCR4 & ~X86_CR4_VMXE);
        return VINF_SUCCESS;
    }

    return VERR_VMX_NOT_IN_VMX_ROOT_MODE;
}


/**
 * Allocates and maps one physically contiguous page. The allocated page is
 * zero'd out. (Used by various VT-x structures).
 *
 * @returns IPRT status code.
 * @param   pMemObj         Pointer to the ring-0 memory object.
 * @param   ppVirt          Where to store the virtual address of the
 *                          allocation.
 * @param   pPhys           Where to store the physical address of the
 *                          allocation.
 */
DECLINLINE(int) hmR0VmxPageAllocZ(PRTR0MEMOBJ pMemObj, PRTR0PTR ppVirt, PRTHCPHYS pHCPhys)
{
    AssertPtrReturn(pMemObj, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppVirt, VERR_INVALID_PARAMETER);
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
DECLINLINE(void) hmR0VmxPageFree(PRTR0MEMOBJ pMemObj, PRTR0PTR ppVirt, PRTHCPHYS pHCPhys)
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
 * @param   pVM             Pointer to the VM.
 */
static void hmR0VmxStructsFree(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjHostMsr, &pVCpu->hm.s.vmx.pvHostMsr, &pVCpu->hm.s.vmx.HCPhysHostMsr);
        hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjGuestMsr, &pVCpu->hm.s.vmx.pvGuestMsr, &pVCpu->hm.s.vmx.HCPhysGuestMsr);
#endif

        if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
            hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjMsrBitmap, &pVCpu->hm.s.vmx.pvMsrBitmap, &pVCpu->hm.s.vmx.HCPhysMsrBitmap);

        hmR0VmxPageFree(&pVCpu->hm.s.vmx.hMemObjVirtApic, (PRTR0PTR)&pVCpu->hm.s.vmx.pbVirtApic, &pVCpu->hm.s.vmx.HCPhysVirtApic);
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
 * @param   pVM             Pointer to the VM.
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
        VMXLOCAL_INIT_VMCPU_MEMOBJ(VirtApic, pb);
        VMXLOCAL_INIT_VMCPU_MEMOBJ(MsrBitmap, pv);
#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        VMXLOCAL_INIT_VMCPU_MEMOBJ(GuestMsr, pv);
        VMXLOCAL_INIT_VMCPU_MEMOBJ(HostMsr, pv);
#endif
    }
#undef VMXLOCAL_INIT_VMCPU_MEMOBJ
#undef VMXLOCAL_INIT_VM_MEMOBJ

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
    if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
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
        AssertReturn(MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(pVM->hm.s.vmx.msr.vmx_basic_info) <= PAGE_SIZE, VERR_INTERNAL_ERROR);
        rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjVmcs, &pVCpu->hm.s.vmx.pvVmcs, &pVCpu->hm.s.vmx.HCPhysVmcs);
        if (RT_FAILURE(rc))
            goto cleanup;

        /* Allocate the Virtual-APIC page for transparent TPR accesses. */
        if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
        {
            rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjVirtApic, (PRTR0PTR)&pVCpu->hm.s.vmx.pbVirtApic,
                                   &pVCpu->hm.s.vmx.HCPhysVirtApic);
            if (RT_FAILURE(rc))
                goto cleanup;
        }

        /* Allocate the MSR-bitmap if supported by the CPU. The MSR-bitmap is for transparent accesses of specific MSRs. */
        if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
        {
            rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjMsrBitmap, &pVCpu->hm.s.vmx.pvMsrBitmap,
                                   &pVCpu->hm.s.vmx.HCPhysMsrBitmap);
            if (RT_FAILURE(rc))
                goto cleanup;
            memset(pVCpu->hm.s.vmx.pvMsrBitmap, 0xff, PAGE_SIZE);
        }

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /* Allocate the VM-entry MSR-load and VM-exit MSR-store page for the guest MSRs. */
        rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjGuestMsr, &pVCpu->hm.s.vmx.pvGuestMsr, &pVCpu->hm.s.vmx.HCPhysGuestMsr);
        if (RT_FAILURE(rc))
            goto cleanup;

        /* Allocate the VM-exit MSR-load page for the host MSRs. */
        rc = hmR0VmxPageAllocZ(&pVCpu->hm.s.vmx.hMemObjHostMsr, &pVCpu->hm.s.vmx.pvHostMsr, &pVCpu->hm.s.vmx.HCPhysHostMsr);
        if (RT_FAILURE(rc))
            goto cleanup;
#endif
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
 * @param   pCpu            Pointer to the global CPU info struct.
 * @param   pVM             Pointer to the VM (can be NULL after a host resume
 *                          operation).
 * @param   pvCpuPage       Pointer to the VMXON region (can be NULL if @a
 *                          fEnabledByHost is true).
 * @param   HCPhysCpuPage   Physical address of the VMXON region (can be 0 if
 *                          @a fEnabledByHost is true).
 * @param   fEnabledByHost  Set if SUPR0EnableVTx() or similar was used to
 *                          enable VT-x on the host.
 */
VMMR0DECL(int) VMXR0EnableCpu(PHMGLOBLCPUINFO pCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage, bool fEnabledByHost)
{
    AssertReturn(pCpu, VERR_INVALID_PARAMETER);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (!fEnabledByHost)
    {
        int rc = hmR0VmxEnterRootMode(pVM, HCPhysCpuPage, pvCpuPage);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Flush all EPTP tagged-TLB entries (in case any other hypervisor have been using EPTPs) so that
     * we can avoid an explicit flush while using new VPIDs. We would still need to flush
     * each time while reusing a VPID after hitting the MaxASID limit once.
     */
    if (   pVM
        && pVM->hm.s.fNestedPaging)
    {
        /* We require ALL_CONTEXT flush-type to be available on the CPU. See hmR0VmxSetupTaggedTlb(). */
        Assert(pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS);
        hmR0VmxFlushEpt(pVM, NULL /* pVCpu */, VMX_FLUSH_EPT_ALL_CONTEXTS);
        pCpu->fFlushAsidBeforeUse = false;
    }
    else
    {
        /** @todo This is still not perfect. If on host resume (pVM is NULL or a VM
         *        without Nested Paging triggered this function) we still have the risk
         *        of potentially running with stale TLB-entries from other hypervisors
         *        when later we use a VM with NestedPaging. To fix this properly we will
         *        have to pass '&g_HvmR0' (see HMR0.cpp) to this function and read
         *        'vmx_ept_vpid_caps' from it. Sigh. */
        pCpu->fFlushAsidBeforeUse = true;
    }

    /* Ensure each VCPU scheduled on this CPU gets a new VPID on resume. See @bugref{6255}. */
    ++pCpu->cTlbFlushes;

    return VINF_SUCCESS;
}


/**
 * Deactivates VT-x on the current CPU.
 *
 * @returns VBox status code.
 * @param   pCpu            Pointer to the global CPU info struct.
 * @param   pvCpuPage       Pointer to the VMXON region.
 * @param   HCPhysCpuPage   Physical address of the VMXON region.
 *
 * @remarks This function should never be called when SUPR0EnableVTx() or
 *          similar was used to enable VT-x on the host.
 */
VMMR0DECL(int) VMXR0DisableCpu(PHMGLOBLCPUINFO pCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    NOREF(pCpu);
    NOREF(pvCpuPage);
    NOREF(HCPhysCpuPage);

    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    return hmR0VmxLeaveRootMode();
}


/**
 * Sets the permission bits for the specified MSR in the MSR bitmap.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   uMSR        The MSR value.
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
    else if (   uMsr >= 0xC0000000
             && uMsr <= 0xC0001FFF)
    {
        iBit = (uMsr - 0xC0000000);
        pbMsrBitmap += 0x400;
    }
    else
    {
        AssertMsgFailed(("hmR0VmxSetMsrPermission: Invalid MSR %#RX32\n", uMsr));
        return;
    }

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


/**
 * Flushes the TLB using EPT.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU (can be NULL depending on @a
 *                      enmFlush).
 * @param   enmFlush    Type of flush.
 */
static void hmR0VmxFlushEpt(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_EPT enmFlush)
{
    AssertPtr(pVM);
    Assert(pVM->hm.s.fNestedPaging);

    uint64_t descriptor[2];
    if (enmFlush == VMX_FLUSH_EPT_ALL_CONTEXTS)
        descriptor[0] = 0;
    else
    {
        Assert(pVCpu);
        descriptor[0] = pVCpu->hm.s.vmx.HCPhysEPTP;
    }
    descriptor[1] = 0;                           /* MBZ. Intel spec. 33.3 "VMX Instructions" */

    int rc = VMXR0InvEPT(enmFlush, &descriptor[0]);
    AssertMsg(rc == VINF_SUCCESS, ("VMXR0InvEPT %#x %RGv failed with %Rrc\n", enmFlush, pVCpu ? pVCpu->hm.s.vmx.HCPhysEPTP : 0,
                                   rc));
    if (   RT_SUCCESS(rc)
        && pVCpu)
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushNestedPaging);
    }
}


/**
 * Flushes the TLB using VPID.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU (can be NULL depending on @a
 *                      enmFlush).
 * @param   enmFlush    Type of flush.
 * @param   GCPtr       Virtual address of the page to flush (can be 0 depending
 *                      on @a enmFlush).
 */
static void hmR0VmxFlushVpid(PVM pVM, PVMCPU pVCpu, VMX_FLUSH_VPID enmFlush, RTGCPTR GCPtr)
{
    AssertPtr(pVM);
    Assert(pVM->hm.s.vmx.fVpid);

    uint64_t descriptor[2];
    if (enmFlush == VMX_FLUSH_VPID_ALL_CONTEXTS)
    {
        descriptor[0] = 0;
        descriptor[1] = 0;
    }
    else
    {
        AssertPtr(pVCpu);
        AssertMsg(pVCpu->hm.s.uCurrentAsid != 0, ("VMXR0InvVPID: invalid ASID %lu\n", pVCpu->hm.s.uCurrentAsid));
        AssertMsg(pVCpu->hm.s.uCurrentAsid <= UINT16_MAX, ("VMXR0InvVPID: invalid ASID %lu\n", pVCpu->hm.s.uCurrentAsid));
        descriptor[0] = pVCpu->hm.s.uCurrentAsid;
        descriptor[1] = GCPtr;
    }

    int rc = VMXR0InvVPID(enmFlush, &descriptor[0]); NOREF(rc);
    AssertMsg(rc == VINF_SUCCESS,
              ("VMXR0InvVPID %#x %u %RGv failed with %d\n", enmFlush, pVCpu ? pVCpu->hm.s.uCurrentAsid : 0, GCPtr, rc));
    if (   RT_SUCCESS(rc)
        && pVCpu)
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushAsid);
    }
}


/**
 * Invalidates a guest page by guest virtual address. Only relevant for
 * EPT/VPID, otherwise there is nothing really to invalidate.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCVirt      Guest virtual address of the page to invalidate.
 */
VMMR0DECL(int) VMXR0InvalidatePage(PVM pVM, PVMCPU pVCpu, RTGCPTR GCVirt)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    LogFlowFunc(("pVM=%p pVCpu=%p GCVirt=%RGv\n", pVM, pVCpu, GCVirt));

    bool fFlushPending = VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    if (!fFlushPending)
    {
        /*
         * We must invalidate the guest TLB entry in either case, we cannot ignore it even for the EPT case
         * See @bugref{6043} and @bugref{6177}.
         *
         * Set the VMCPU_FF_TLB_FLUSH force flag and flush before VM-entry in hmR0VmxFlushTLB*() as this
         * function maybe called in a loop with individual addresses.
         */
        if (pVM->hm.s.vmx.fVpid)
        {
            if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR)
            {
                hmR0VmxFlushVpid(pVM, pVCpu, VMX_FLUSH_VPID_INDIV_ADDR, GCVirt);
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
 * Invalidates a guest page by physical address. Only relevant for EPT/VPID,
 * otherwise there is nothing really to invalidate.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCPhys      Guest physical address of the page to invalidate.
 */
VMMR0DECL(int) VMXR0InvalidatePhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys)
{
    LogFlowFunc(("%RGp\n", GCPhys));

    /*
     * We cannot flush a page by guest-physical address. invvpid takes only a linear address while invept only flushes
     * by EPT not individual addresses. We update the force flag here and flush before the next VM-entry in hmR0VmxFlushTLB*().
     * This function might be called in a loop. This should cause a flush-by-EPT if EPT is in use. See @bugref{6568}.
     */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbInvlpgPhys);
    return VINF_SUCCESS;
}


/**
 * Dummy placeholder for tagged-TLB flush handling before VM-entry. Used in the
 * case where neither EPT nor VPID is supported by the CPU.
 *
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbNone(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);
    AssertPtr(pVCpu);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    PHMGLOBLCPUINFO pCpu = HMR0GetCurrentCpu();
    AssertPtr(pCpu);

    pVCpu->hm.s.TlbShootdown.cPages = 0;
    pVCpu->hm.s.idLastCpu           = pCpu->idCpu;
    pVCpu->hm.s.cTlbFlushes         = pCpu->cTlbFlushes;
    pVCpu->hm.s.fForceTLBFlush      = false;
    return;
}


/**
 * Flushes the tagged-TLB entries for EPT+VPID CPUs as necessary.
 *
 * @param    pVM            Pointer to the VM.
 * @param    pVCpu          Pointer to the VMCPU.
 * @remarks All references to "ASID" in this function pertains to "VPID" in
 *          Intel's nomenclature. The reason is, to avoid confusion in compare
 *          statements since the host-CPU copies are named "ASID".
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbBoth(PVM pVM, PVMCPU pVCpu)
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

    AssertPtr(pVM);
    AssertPtr(pVCpu);
    AssertMsg(pVM->hm.s.fNestedPaging && pVM->hm.s.vmx.fVpid,
              ("hmR0VmxFlushTaggedTlbBoth cannot be invoked unless NestedPaging & VPID are enabled."
               "fNestedPaging=%RTbool fVpid=%RTbool", pVM->hm.s.fNestedPaging, pVM->hm.s.vmx.fVpid));

    PHMGLOBLCPUINFO pCpu = HMR0GetCurrentCpu();
    AssertPtr(pCpu);

    /*
     * Force a TLB flush for the first world-switch if the current CPU differs from the one we ran on last.
     * If the TLB flush count changed, another VM (VCPU rather) has hit the ASID limit while flushing the TLB
     * or the host CPU is online after a suspend/resume, so we cannot reuse the current ASID anymore.
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
        hmR0VmxFlushEpt(pVM, pVCpu, pVM->hm.s.vmx.enmFlushEpt);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
        HMVMX_SET_TAGGED_TLB_FLUSHED();
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH);  /* Already flushed-by-EPT, skip doing it again below. */
    }

    /* Check for explicit TLB shootdowns. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        /*
         * Changes to the EPT paging structure by VMM requires flushing by EPT as the CPU creates
         * guest-physical (only EPT-tagged) mappings while traversing the EPT tables when EPT is in use.
         * Flushing by VPID will only flush linear (only VPID-tagged) and combined (EPT+VPID tagged) mappings
         * but not guest-physical mappings.
         * See Intel spec. 28.3.2 "Creating and Using Cached Translation Information". See @bugref{6568}.
         */
        hmR0VmxFlushEpt(pVM, pVCpu, pVM->hm.s.vmx.enmFlushEpt);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
        HMVMX_SET_TAGGED_TLB_FLUSHED();
    }

    /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
     *        not be executed. See hmQueueInvlPage() where it is commented
     *        out. Support individual entry flushing someday. */
    if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTlbShootdown);

        /*
         * Flush individual guest entries using VPID from the TLB or as little as possible with EPT
         * as supported by the CPU.
         */
        if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR)
        {
            for (uint32_t i = 0; i < pVCpu->hm.s.TlbShootdown.cPages; i++)
                hmR0VmxFlushVpid(pVM, pVCpu, VMX_FLUSH_VPID_INDIV_ADDR, pVCpu->hm.s.TlbShootdown.aPages[i]);
        }
        else
            hmR0VmxFlushEpt(pVM, pVCpu, pVM->hm.s.vmx.enmFlushEpt);

        HMVMX_SET_TAGGED_TLB_FLUSHED();
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);
    }

    pVCpu->hm.s.TlbShootdown.cPages = 0;
    pVCpu->hm.s.fForceTLBFlush = false;

    HMVMX_UPDATE_FLUSH_SKIPPED_STAT();

    Assert(pVCpu->hm.s.idLastCpu == pCpu->idCpu);
    Assert(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes);
    AssertMsg(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes,
              ("Flush count mismatch for cpu %d (%u vs %u)\n", pCpu->idCpu, pVCpu->hm.s.cTlbFlushes, pCpu->cTlbFlushes));
    AssertMsg(pCpu->uCurrentAsid >= 1 && pCpu->uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d uCurrentAsid = %u\n", pCpu->idCpu, pCpu->uCurrentAsid));
    AssertMsg(pVCpu->hm.s.uCurrentAsid >= 1 && pVCpu->hm.s.uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d VM uCurrentAsid = %u\n", pCpu->idCpu, pVCpu->hm.s.uCurrentAsid));

    /* Update VMCS with the VPID. */
    int rc  = VMXWriteVmcs32(VMX_VMCS16_GUEST_FIELD_VPID, pVCpu->hm.s.uCurrentAsid);
    AssertRC(rc);

#undef HMVMX_SET_TAGGED_TLB_FLUSHED
}


/**
 * Flushes the tagged-TLB entries for EPT CPUs as necessary.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbEpt(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    AssertMsg(pVM->hm.s.fNestedPaging, ("hmR0VmxFlushTaggedTlbEpt cannot be invoked with NestedPaging disabled."));
    AssertMsg(!pVM->hm.s.vmx.fVpid, ("hmR0VmxFlushTaggedTlbEpt cannot be invoked with VPID enabled."));

    PHMGLOBLCPUINFO pCpu = HMR0GetCurrentCpu();
    AssertPtr(pCpu);

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

    /* Check for explicit TLB shootdown flushes. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
    }

    pVCpu->hm.s.idLastCpu   = pCpu->idCpu;
    pVCpu->hm.s.cTlbFlushes = pCpu->cTlbFlushes;

    if (pVCpu->hm.s.fForceTLBFlush)
    {
        hmR0VmxFlushEpt(pVM, pVCpu, pVM->hm.s.vmx.enmFlushEpt);
        pVCpu->hm.s.fForceTLBFlush = false;
    }
    else
    {
        /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
         *        not be executed. See hmQueueInvlPage() where it is commented
         *        out. Support individual entry flushing someday. */
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /* We cannot flush individual entries without VPID support. Flush using EPT. */
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTlbShootdown);
            hmR0VmxFlushEpt(pVM, pVCpu, pVM->hm.s.vmx.enmFlushEpt);
        }
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch);
    }

    pVCpu->hm.s.TlbShootdown.cPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);
}


/**
 * Flushes the tagged-TLB entries for VPID CPUs as necessary.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks Called with interrupts disabled.
 */
static void hmR0VmxFlushTaggedTlbVpid(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    AssertMsg(pVM->hm.s.vmx.fVpid, ("hmR0VmxFlushTlbVpid cannot be invoked with VPID disabled."));
    AssertMsg(!pVM->hm.s.fNestedPaging, ("hmR0VmxFlushTlbVpid cannot be invoked with NestedPaging enabled"));

    PHMGLOBLCPUINFO pCpu = HMR0GetCurrentCpu();

    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one we ran on last.
     * If the TLB flush count changed, another VM (VCPU rather) has hit the ASID limit while flushing the TLB
     * or the host CPU is online after a suspend/resume, so we cannot reuse the current ASID anymore.
     */
    if (   pVCpu->hm.s.idLastCpu   != pCpu->idCpu
        || pVCpu->hm.s.cTlbFlushes != pCpu->cTlbFlushes)
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
    }

    /* Check for explicit TLB shootdown flushes. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        /*
         * If we ever support VPID flush combinations other than ALL or SINGLE-context (see hmR0VmxSetupTaggedTlb())
         * we would need to explicitly flush in this case (add an fExplicitFlush = true here and change the
         * pCpu->fFlushAsidBeforeUse check below to include fExplicitFlush's too) - an obscure corner case.
         */
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
    }

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
            hmR0VmxFlushVpid(pVM, pVCpu, pVM->hm.s.vmx.enmFlushVpid, 0 /* GCPtr */);
    }
    else
    {
        AssertMsg(pVCpu->hm.s.uCurrentAsid && pCpu->uCurrentAsid,
                  ("hm->uCurrentAsid=%lu hm->cTlbFlushes=%lu cpu->uCurrentAsid=%lu cpu->cTlbFlushes=%lu\n",
                   pVCpu->hm.s.uCurrentAsid, pVCpu->hm.s.cTlbFlushes,
                   pCpu->uCurrentAsid, pCpu->cTlbFlushes));

        /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
         *        not be executed. See hmQueueInvlPage() where it is commented
         *        out. Support individual entry flushing someday. */
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /* Flush individual guest entries using VPID or as little as possible with EPT as supported by the CPU. */
            if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR)
            {
                for (uint32_t i = 0; i < pVCpu->hm.s.TlbShootdown.cPages; i++)
                    hmR0VmxFlushVpid(pVM, pVCpu, VMX_FLUSH_VPID_INDIV_ADDR, pVCpu->hm.s.TlbShootdown.aPages[i]);
            }
            else
                hmR0VmxFlushVpid(pVM, pVCpu, pVM->hm.s.vmx.enmFlushVpid, 0 /* GCPtr */);
        }
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch);
    }

    pVCpu->hm.s.TlbShootdown.cPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    AssertMsg(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes,
              ("Flush count mismatch for cpu %d (%u vs %u)\n", pCpu->idCpu, pVCpu->hm.s.cTlbFlushes, pCpu->cTlbFlushes));
    AssertMsg(pCpu->uCurrentAsid >= 1 && pCpu->uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d uCurrentAsid = %u\n", pCpu->idCpu, pCpu->uCurrentAsid));
    AssertMsg(pVCpu->hm.s.uCurrentAsid >= 1 && pVCpu->hm.s.uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d VM uCurrentAsid = %u\n", pCpu->idCpu, pVCpu->hm.s.uCurrentAsid));

    int rc  = VMXWriteVmcs32(VMX_VMCS16_GUEST_FIELD_VPID, pVCpu->hm.s.uCurrentAsid);
    AssertRC(rc);
}


/**
 * Flushes the guest TLB entry based on CPU capabilities.
 *
 * @param pVCpu     Pointer to the VMCPU.
 */
DECLINLINE(void) hmR0VmxFlushTaggedTlb(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    switch (pVM->hm.s.vmx.uFlushTaggedTlb)
    {
        case HMVMX_FLUSH_TAGGED_TLB_EPT_VPID: hmR0VmxFlushTaggedTlbBoth(pVM, pVCpu); break;
        case HMVMX_FLUSH_TAGGED_TLB_EPT:      hmR0VmxFlushTaggedTlbEpt(pVM, pVCpu);  break;
        case HMVMX_FLUSH_TAGGED_TLB_VPID:     hmR0VmxFlushTaggedTlbVpid(pVM, pVCpu); break;
        case HMVMX_FLUSH_TAGGED_TLB_NONE:     hmR0VmxFlushTaggedTlbNone(pVM, pVCpu); break;
        default:
            AssertMsgFailed(("Invalid flush-tag function identifier\n"));
            break;
    }
}


/**
 * Sets up the appropriate tagged TLB-flush level and handler for flushing guest
 * TLB entries from the host TLB before VM-entry.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
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
        if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT)
        {
            if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT)
                pVM->hm.s.vmx.enmFlushEpt = VMX_FLUSH_EPT_SINGLE_CONTEXT;
            else if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS)
                pVM->hm.s.vmx.enmFlushEpt = VMX_FLUSH_EPT_ALL_CONTEXTS;
            else
            {
                /* Shouldn't happen. EPT is supported but no suitable flush-types supported. */
                pVM->hm.s.vmx.enmFlushEpt = VMX_FLUSH_EPT_NOT_SUPPORTED;
                return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
            }

            /* Make sure the write-back cacheable memory type for EPT is supported. */
            if (!(pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB))
            {
                LogRel(("hmR0VmxSetupTaggedTlb: Unsupported EPTP memory type %#x.\n", pVM->hm.s.vmx.msr.vmx_ept_vpid_caps));
                pVM->hm.s.vmx.enmFlushEpt = VMX_FLUSH_EPT_NOT_SUPPORTED;
                return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
            }
        }
        else
        {
            /* Shouldn't happen. EPT is supported but INVEPT instruction is not supported. */
            pVM->hm.s.vmx.enmFlushEpt = VMX_FLUSH_EPT_NOT_SUPPORTED;
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }
    }

    /*
     * Determine optimal flush type for VPID.
     */
    if (pVM->hm.s.vmx.fVpid)
    {
        if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID)
        {
            if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT)
                pVM->hm.s.vmx.enmFlushVpid = VMX_FLUSH_VPID_SINGLE_CONTEXT;
            else if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS)
                pVM->hm.s.vmx.enmFlushVpid = VMX_FLUSH_VPID_ALL_CONTEXTS;
            else
            {
                /* Neither SINGLE nor ALL-context flush types for VPID is supported by the CPU. Ignore VPID capability. */
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR)
                    LogRel(("hmR0VmxSetupTaggedTlb: Only INDIV_ADDR supported. Ignoring VPID.\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS)
                    LogRel(("hmR0VmxSetupTaggedTlb: Only SINGLE_CONTEXT_RETAIN_GLOBALS supported. Ignoring VPID.\n"));
                pVM->hm.s.vmx.enmFlushVpid = VMX_FLUSH_VPID_NOT_SUPPORTED;
                pVM->hm.s.vmx.fVpid = false;
            }
        }
        else
        {
            /*  Shouldn't happen. VPID is supported but INVVPID is not supported by the CPU. Ignore VPID capability. */
            Log4(("hmR0VmxSetupTaggedTlb: VPID supported without INVEPT support. Ignoring VPID.\n"));
            pVM->hm.s.vmx.enmFlushVpid = VMX_FLUSH_VPID_NOT_SUPPORTED;
            pVM->hm.s.vmx.fVpid = false;
        }
    }

    /*
     * Setup the handler for flushing tagged-TLBs.
     */
    if (pVM->hm.s.fNestedPaging && pVM->hm.s.vmx.fVpid)
        pVM->hm.s.vmx.uFlushTaggedTlb = HMVMX_FLUSH_TAGGED_TLB_EPT_VPID;
    else if (pVM->hm.s.fNestedPaging)
        pVM->hm.s.vmx.uFlushTaggedTlb = HMVMX_FLUSH_TAGGED_TLB_EPT;
    else if (pVM->hm.s.vmx.fVpid)
        pVM->hm.s.vmx.uFlushTaggedTlb = HMVMX_FLUSH_TAGGED_TLB_VPID;
    else
        pVM->hm.s.vmx.uFlushTaggedTlb = HMVMX_FLUSH_TAGGED_TLB_NONE;
    return VINF_SUCCESS;
}


/**
 * Sets up pin-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static int hmR0VmxSetupPinCtls(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);

    uint32_t val = pVM->hm.s.vmx.msr.vmx_pin_ctls.n.disallowed0;    /* Bits set here must always be set. */
    uint32_t zap = pVM->hm.s.vmx.msr.vmx_pin_ctls.n.allowed1;       /* Bits cleared here must always be cleared. */

    val |=   VMX_VMCS_CTRL_PIN_EXEC_EXT_INT_EXIT           /* External interrupts causes a VM-exits. */
           | VMX_VMCS_CTRL_PIN_EXEC_NMI_EXIT;              /* Non-maskable interrupts causes a VM-exit. */
    Assert(!(val & VMX_VMCS_CTRL_PIN_EXEC_VIRTUAL_NMI));

    /* Enable the VMX preemption timer. */
    if (pVM->hm.s.vmx.fUsePreemptTimer)
    {
        Assert(pVM->hm.s.vmx.msr.vmx_pin_ctls.n.allowed1 & VMX_VMCS_CTRL_PIN_EXEC_PREEMPT_TIMER);
        val |= VMX_VMCS_CTRL_PIN_EXEC_PREEMPT_TIMER;
    }

    if ((val & zap) != val)
    {
        LogRel(("hmR0VmxSetupPinCtls: invalid pin-based VM-execution controls combo! cpu=%#RX64 val=%#RX64 zap=%#RX64\n",
                pVM->hm.s.vmx.msr.vmx_pin_ctls.n.disallowed0, val, zap));
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PIN_EXEC, val);
    AssertRCReturn(rc, rc);

    /* Update VCPU with the currently set pin-based VM-execution controls. */
    pVCpu->hm.s.vmx.u32PinCtls = val;
    return rc;
}


/**
 * Sets up processor-based VM-execution controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVMCPU      Pointer to the VMCPU.
 */
static int hmR0VmxSetupProcCtls(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);

    int rc = VERR_INTERNAL_ERROR_5;
    uint32_t val = pVM->hm.s.vmx.msr.vmx_proc_ctls.n.disallowed0;       /* Bits set here must be set in the VMCS. */
    uint32_t zap = pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1;          /* Bits cleared here must be cleared in the VMCS. */

    val |=   VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT                  /* HLT causes a VM-exit. */
           | VMX_VMCS_CTRL_PROC_EXEC_USE_TSC_OFFSETTING        /* Use TSC-offsetting. */
           | VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT               /* MOV DRx causes a VM-exit. */
           | VMX_VMCS_CTRL_PROC_EXEC_UNCOND_IO_EXIT            /* All IO instructions cause a VM-exit. */
           | VMX_VMCS_CTRL_PROC_EXEC_RDPMC_EXIT                /* RDPMC causes a VM-exit. */
           | VMX_VMCS_CTRL_PROC_EXEC_MONITOR_EXIT              /* MONITOR causes a VM-exit. */
           | VMX_VMCS_CTRL_PROC_EXEC_MWAIT_EXIT;               /* MWAIT causes a VM-exit. */

    /* We toggle VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT later, check if it's not -always- needed to be set or clear. */
    if (   !(pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT)
        ||  (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.disallowed0 & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT))
    {
        LogRel(("hmR0VmxSetupProcCtls: unsupported VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT combo!"));
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    /* Without Nested Paging, INVLPG (also affects INVPCID) and MOV CR3 instructions should cause VM-exits. */
    if (!pVM->hm.s.fNestedPaging)
    {
        Assert(!pVM->hm.s.vmx.fUnrestrictedGuest);                      /* Paranoia. */
        val |=   VMX_VMCS_CTRL_PROC_EXEC_INVLPG_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT
               | VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT;
    }

    /* Use TPR shadowing if supported by the CPU. */
    if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
    {
        Assert(pVCpu->hm.s.vmx.HCPhysVirtApic);
        Assert(!(pVCpu->hm.s.vmx.HCPhysVirtApic & 0xfff));              /* Bits 11:0 MBZ. */
        rc  = VMXWriteVmcs32(VMX_VMCS32_CTRL_TPR_THRESHOLD, 0);
        rc |= VMXWriteVmcs64(VMX_VMCS64_CTRL_VAPIC_PAGEADDR_FULL, pVCpu->hm.s.vmx.HCPhysVirtApic);
        AssertRCReturn(rc, rc);

        val |= VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW;         /* CR8 reads from the Virtual-APIC page. */
                                                               /* CR8 writes causes a VM-exit based on TPR threshold. */
        Assert(!(val & VMX_VMCS_CTRL_PROC_EXEC_CR8_STORE_EXIT));
        Assert(!(val & VMX_VMCS_CTRL_PROC_EXEC_CR8_LOAD_EXIT));
    }
    else
    {
        val |=   VMX_VMCS_CTRL_PROC_EXEC_CR8_STORE_EXIT        /* CR8 reads causes a VM-exit. */
               | VMX_VMCS_CTRL_PROC_EXEC_CR8_LOAD_EXIT;        /* CR8 writes causes a VM-exit. */
    }

    /* Use MSR-bitmaps if supported by the CPU. */
    if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
    {
        val |= VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS;

        Assert(pVCpu->hm.s.vmx.HCPhysMsrBitmap);
        Assert(!(pVCpu->hm.s.vmx.HCPhysMsrBitmap & 0xfff));             /* Bits 11:0 MBZ. */
        rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_MSR_BITMAP_FULL, pVCpu->hm.s.vmx.HCPhysMsrBitmap);
        AssertRCReturn(rc, rc);

        /*
         * The guest can access the following MSRs (read, write) without causing VM-exits; they are loaded/stored
         * automatically (either as part of the MSR-load/store areas or dedicated fields in the VMCS).
         */
        hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_CS,  VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_ESP, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_EIP, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_LSTAR,          VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K6_STAR,           VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_SF_MASK,        VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_KERNEL_GS_BASE, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_GS_BASE,        VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        hmR0VmxSetMsrPermission(pVCpu, MSR_K8_FS_BASE,        VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
    }

    /* Use the secondary processor-based VM-execution controls if supported by the CPU. */
    if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
        val |= VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL;

    if ((val & zap) != val)
    {
        LogRel(("hmR0VmxSetupProcCtls: invalid processor-based VM-execution controls combo! cpu=%#RX64 val=%#RX64 zap=%#RX64\n",
                pVM->hm.s.vmx.msr.vmx_proc_ctls.n.disallowed0, val, zap));
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, val);
    AssertRCReturn(rc, rc);

    /* Update VCPU with the currently set processor-based VM-execution controls. */
    pVCpu->hm.s.vmx.u32ProcCtls = val;

    /*
     * Secondary processor-based VM-execution controls.
     */
    if (RT_LIKELY(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL))
    {
        val = pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;           /* Bits set here must be set in the VMCS. */
        zap = pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1;              /* Bits cleared here must be cleared in the VMCS. */

        if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
            val |= VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT;                /* WBINVD causes a VM-exit. */

        if (pVM->hm.s.fNestedPaging)
            val |= VMX_VMCS_CTRL_PROC_EXEC2_EPT;                        /* Enable EPT. */
        else
        {
            /*
             * Without Nested Paging, INVPCID should cause a VM-exit. Enabling this bit causes the CPU to refer to
             * VMX_VMCS_CTRL_PROC_EXEC_INVLPG_EXIT when INVPCID is executed by the guest.
             * See Intel spec. 25.4 "Changes to instruction behaviour in VMX non-root operation".
             */
            if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_INVPCID)
                val |= VMX_VMCS_CTRL_PROC_EXEC2_INVPCID;
        }

        if (pVM->hm.s.vmx.fVpid)
            val |= VMX_VMCS_CTRL_PROC_EXEC2_VPID;                       /* Enable VPID. */

        if (pVM->hm.s.vmx.fUnrestrictedGuest)
            val |= VMX_VMCS_CTRL_PROC_EXEC2_UNRESTRICTED_GUEST;         /* Enable Unrestricted Execution. */

        /* Enable Virtual-APIC page accesses if supported by the CPU. This is essentially where the TPR shadow resides. */
        /** @todo VIRT_X2APIC support, it's mutually exclusive with this. So must be
         *        done dynamically. */
        if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
        {
            Assert(pVM->hm.s.vmx.HCPhysApicAccess);
            Assert(!(pVM->hm.s.vmx.HCPhysApicAccess & 0xfff));          /* Bits 11:0 MBZ. */
            val |= VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC;                  /* Virtualize APIC accesses. */
            rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL, pVM->hm.s.vmx.HCPhysApicAccess);
            AssertRCReturn(rc, rc);
        }

        if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
        {
            val |= VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP;                     /* Enable RDTSCP support. */
            if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS)
                hmR0VmxSetMsrPermission(pVCpu, MSR_K8_TSC_AUX, VMXMSREXIT_PASSTHRU_READ, VMXMSREXIT_PASSTHRU_WRITE);
        }

        if ((val & zap) != val)
        {
            LogRel(("hmR0VmxSetupProcCtls: invalid secondary processor-based VM-execution controls combo! "
                    "cpu=%#RX64 val=%#RX64 zap=%#RX64\n", pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0, val, zap));
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }

        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, val);
        AssertRCReturn(rc, rc);

        /* Update VCPU with the currently set secondary processor-based VM-execution controls. */
        pVCpu->hm.s.vmx.u32ProcCtls2 = val;
    }

    return VINF_SUCCESS;
}


/**
 * Sets up miscellaneous (everything other than Pin & Processor-based
 * VM-execution) control fields in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static int hmR0VmxSetupMiscCtls(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);

    int rc = VERR_GENERAL_FAILURE;

    /* All fields are zero-initialized during allocation; but don't remove the commented block below. */
#if 0
    /* All CR3 accesses cause VM-exits. Later we optimize CR3 accesses (see hmR0VmxLoadGuestControlRegs())*/
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_CR3_TARGET_COUNT, 0);           AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_TSC_OFFSET_FULL, 0);            AssertRCReturn(rc, rc);

    /*
     * Set MASK & MATCH to 0. VMX checks if GuestPFErrCode & MASK == MATCH. If equal (in our case it always is)
     * and if the X86_XCPT_PF bit in the exception bitmap is set it causes a VM-exit, if clear doesn't cause an exit.
     * We thus use the exception bitmap to control it rather than use both.
     */
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK, 0);       AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH, 0);      AssertRCReturn(rc, rc);

    /** @todo Explore possibility of using IO-bitmaps. */
    /* All IO & IOIO instructions cause VM-exits. */
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_IO_BITMAP_A_FULL, 0);           AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_IO_BITMAP_B_FULL, 0);           AssertRCReturn(rc, rc);

    /* Initialize the MSR-bitmap area. */
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT, 0);       AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT, 0);       AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT,  0);       AssertRCReturn(rc, rc);
#endif

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
    /* Setup MSR autoloading/storing. */
    Assert(pVCpu->hm.s.vmx.HCPhysGuestMsr);
    Assert(!(pVCpu->hm.s.vmx.HCPhysGuestMsr & 0xf));    /* Lower 4 bits MBZ. */
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL, pVCpu->hm.s.vmx.HCPhysGuestMsr);
    AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL, pVCpu->hm.s.vmx.HCPhysGuestMsr);
    AssertRCReturn(rc, rc);

    Assert(pVCpu->hm.s.vmx.HCPhysHostMsr);
    Assert(!(pVCpu->hm.s.vmx.HCPhysHostMsr & 0xf));     /* Lower 4 bits MBZ. */
    rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL,  pVCpu->hm.s.vmx.HCPhysHostMsr);
    AssertRCReturn(rc, rc);
#endif

    /* Set VMCS link pointer. Reserved for future use, must be -1. Intel spec. 24.4 "Guest-State Area". */
    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL, UINT64_C(0xffffffffffffffff));
    AssertRCReturn(rc, rc);

    /* All fields are zero-initialized during allocation; but don't remove the commented block below. */
#if 0
    /* Setup debug controls */
    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_DEBUGCTL_FULL, 0);        /** @todo We don't support IA32_DEBUGCTL MSR. Should we? */
    AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
    AssertRCReturn(rc, rc);
#endif

    return rc;
}


/**
 * Sets up the initial exception bitmap in the VMCS based on static conditions
 * (i.e. conditions that cannot ever change at runtime).
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static int hmR0VmxInitXcptBitmap(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);

    LogFlowFunc(("pVM=%p pVCpu=%p\n", pVM, pVCpu));

    uint32_t u32XcptBitmap = 0;

    /* Without Nested Paging, #PF must cause a VM-exit so we can sync our shadow page tables. */
    if (!pVM->hm.s.fNestedPaging)
        u32XcptBitmap |= RT_BIT(X86_XCPT_PF);

    pVCpu->hm.s.vmx.u32XcptBitmap = u32XcptBitmap;
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, u32XcptBitmap);
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Sets up the initial guest-state mask. The guest-state mask is consulted
 * before reading guest-state fields from the VMCS as VMREADs can be expensive
 * for the nested virtualization case (as it would cause a VM-exit).
 *
 * @param   pVCpu       Pointer to the VMCPU.
 */
static int hmR0VmxInitUpdatedGuestStateMask(PVMCPU pVCpu)
{
    /* Initially the guest-state is up-to-date as there is nothing in the VMCS. */
    pVCpu->hm.s.vmx.fUpdatedGuestState = HMVMX_UPDATED_GUEST_ALL;
    return VINF_SUCCESS;
}


/**
 * Does per-VM VT-x initialization.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 */
VMMR0DECL(int) VMXR0InitVM(PVM pVM)
{
    LogFlowFunc(("pVM=%p\n", pVM));

    int rc = hmR0VmxStructsAlloc(pVM);
    if (RT_FAILURE(rc))
    {
        LogRel(("VMXR0InitVM: hmR0VmxStructsAlloc failed! rc=%Rrc\n", rc));
        return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Does per-VM VT-x termination.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
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
 * This function is only called once per-VM during initalization.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) VMXR0SetupVM(PVM pVM)
{
    AssertPtrReturn(pVM, VERR_INVALID_PARAMETER);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    LogFlowFunc(("pVM=%p\n", pVM));

    /*
     * Without UnrestrictedGuest, pRealModeTSS and pNonPagingModeEPTPageTable *must* always be allocated.
     * We no longer support the highly unlikely case of UnrestrictedGuest without pRealModeTSS. See hmR3InitFinalizeR0().
     */
    /* -XXX- change hmR3InitFinalizeR0Intel() to fail if pRealModeTSS alloc fails. */
    if (   !pVM->hm.s.vmx.fUnrestrictedGuest
        &&  (   !pVM->hm.s.vmx.pNonPagingModeEPTPageTable
             || !pVM->hm.s.vmx.pRealModeTSS))
    {
        LogRel(("VMXR0SetupVM: invalid real-on-v86 state.\n"));
        return VERR_INTERNAL_ERROR;
    }

    /* Initialize these always, see hmR3InitFinalizeR0().*/
    pVM->hm.s.vmx.enmFlushEpt  = VMX_FLUSH_EPT_NONE;
    pVM->hm.s.vmx.enmFlushVpid = VMX_FLUSH_VPID_NONE;

    /* Setup the tagged-TLB flush handlers. */
    int rc = hmR0VmxSetupTaggedTlb(pVM);
    if (RT_FAILURE(rc))
    {
        LogRel(("VMXR0SetupVM: hmR0VmxSetupTaggedTlb failed! rc=%Rrc\n", rc));
        return rc;
    }

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);
        AssertPtr(pVCpu->hm.s.vmx.pvVmcs);

        /* Log the VCPU pointers, useful for debugging SMP VMs. */
        Log4(("VMXR0SetupVM: pVCpu=%p idCpu=%RU32\n", pVCpu, pVCpu->idCpu));

        /* Set revision dword at the beginning of the VMCS structure. */
        *(uint32_t *)pVCpu->hm.s.vmx.pvVmcs = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hm.s.vmx.msr.vmx_basic_info);

        /* Initialize our VMCS region in memory, set the VMCS launch state to "clear". */
        rc  = VMXClearVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: VMXClearVMCS failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        /* Load this VMCS as the current VMCS. */
        rc = VMXActivateVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: VMXActivateVMCS failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        rc = hmR0VmxSetupPinCtls(pVM, pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxSetupPinCtls failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        rc = hmR0VmxSetupProcCtls(pVM, pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxSetupProcCtls failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        rc = hmR0VmxSetupMiscCtls(pVM, pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxSetupMiscCtls failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        rc = hmR0VmxInitXcptBitmap(pVM, pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxInitXcptBitmap failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        rc = hmR0VmxInitUpdatedGuestStateMask(pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxInitUpdatedGuestStateMask failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        rc = hmR0VmxInitVmcsReadCache(pVM, pVCpu);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: hmR0VmxInitVmcsReadCache failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);
#endif

        /* Re-sync the CPU's internal data into our VMCS memory region & reset the launch state to "clear". */
        rc = VMXClearVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);
        AssertLogRelMsgRCReturnStmt(rc, ("VMXR0SetupVM: VMXClearVMCS(2) failed! rc=%Rrc (pVM=%p)\n", rc, pVM),
                                    hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc), rc);

        hmR0VmxUpdateErrorRecord(pVM, pVCpu, rc);
    }

    return VINF_SUCCESS;
}


/**
 * Saves the host control registers (CR0, CR3, CR4) into the host-state area in
 * the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
DECLINLINE(int) hmR0VmxSaveHostControlRegs(PVM pVM, PVMCPU pVCpu)
{
    RTCCUINTREG uReg = ASMGetCR0();
    int rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_CR0, uReg);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    /* For the darwin 32-bit hybrid kernel, we need the 64-bit CR3 as it uses 64-bit paging. */
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        uint64_t uRegCR3 = HMR0Get64bitCR3();
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_CR3, uRegCR3);
    }
    else
#endif
    {
        uReg = ASMGetCR3();
        rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_CR3, uReg);
    }
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
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
DECLINLINE(int) hmR0VmxSaveHostSegmentRegs(PVM pVM, PVMCPU pVCpu)
{
    int rc = VERR_INTERNAL_ERROR_5;
    RTSEL uSelDS = 0;
    RTSEL uSelES = 0;
    RTSEL uSelFS = 0;
    RTSEL uSelGS = 0;
    RTSEL uSelTR = 0;

    /*
     * Host DS, ES, FS and GS segment registers.
     */
#if HC_ARCH_BITS == 64
    pVCpu->hm.s.vmx.fRestoreHostFlags = 0;
    uSelDS = ASMGetDS();
    uSelES = ASMGetES();
    uSelFS = ASMGetFS();
    uSelGS = ASMGetGS();
#endif

    /*
     * Host CS and SS segment registers.
     */
    RTSEL uSelCS;
    RTSEL uSelSS;
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        uSelCS = (RTSEL)(uintptr_t)&SUPR0Abs64bitKernelCS;
        uSelSS = (RTSEL)(uintptr_t)&SUPR0Abs64bitKernelSS;
    }
    else
    {
        /* Seems darwin uses the LDT (TI flag is set) in the CS & SS selectors which VT-x doesn't like. */
        uSelCS = (RTSEL)(uintptr_t)&SUPR0AbsKernelCS;
        uSelSS = (RTSEL)(uintptr_t)&SUPR0AbsKernelSS;
    }
#else
    uSelCS = ASMGetCS();
    uSelSS = ASMGetSS();
#endif

    /*
     * Host TR segment register.
     */
    uSelTR = ASMGetTR();

#if HC_ARCH_BITS == 64
    /*
     * Determine if the host segment registers are suitable for VT-x. Otherwise use zero to gain VM-entry and restore them
     * before we get preempted. See Intel spec. 26.2.3 "Checks on Host Segment and Descriptor-Table Registers".
     */
    if (uSelDS & (X86_SEL_RPL | X86_SEL_LDT))
    {
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_SEL_DS;
        pVCpu->hm.s.vmx.RestoreHost.uHostSelDS = uSelDS;
        uSelDS = 0;
    }
    if (uSelES & (X86_SEL_RPL | X86_SEL_LDT))
    {
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_SEL_ES;
        pVCpu->hm.s.vmx.RestoreHost.uHostSelES = uSelES;
        uSelES = 0;
    }
    if (uSelFS & (X86_SEL_RPL | X86_SEL_LDT))
    {
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_SEL_FS;
        pVCpu->hm.s.vmx.RestoreHost.uHostSelFS = uSelFS;
        uSelFS = 0;
    }
    if (uSelGS & (X86_SEL_RPL | X86_SEL_LDT))
    {
        pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_SEL_GS;
        pVCpu->hm.s.vmx.RestoreHost.uHostSelGS = uSelGS;
        uSelGS = 0;
    }
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
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_CS, uSelCS);      AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_SS, uSelSS);      AssertRCReturn(rc, rc);
#if HC_ARCH_BITS == 64
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_DS, uSelDS);      AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_ES, uSelES);      AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_FS, uSelFS);      AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_GS, uSelGS);      AssertRCReturn(rc, rc);
#endif
    rc = VMXWriteVmcs32(VMX_VMCS16_HOST_FIELD_TR, uSelTR);      AssertRCReturn(rc, rc);

    /*
     * Host GDTR and IDTR.
     */
    RTGDTR Gdtr;
    RT_ZERO(Gdtr);
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        X86XDTR64 Gdtr64;
        X86XDTR64 Idtr64;
        HMR0Get64bitGdtrAndIdtr(&Gdtr64, &Idtr64);
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_GDTR_BASE, Gdtr64.uAddr);     AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_IDTR_BASE, Idtr64.uAddr);     AssertRCReturn(rc, rc);

        Gdtr.cbGdt = Gdtr64.cb;
        Gdtr.pGdt  = (uintptr_t)Gdtr64.uAddr;
    }
    else
#endif
    {
        RTIDTR Idtr;
        ASMGetGDTR(&Gdtr);
        ASMGetIDTR(&Idtr);
        rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_GDTR_BASE, Gdtr.pGdt);      AssertRCReturn(rc, rc);
        rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_IDTR_BASE, Idtr.pIdt);      AssertRCReturn(rc, rc);

#if HC_ARCH_BITS == 64
        /*
         * Determine if we need to manually need to restore the GDTR and IDTR limits as VT-x zaps them to the
         * maximum limit (0xffff) on every VM-exit.
         */
        if (Gdtr.cbGdt != 0xffff)
        {
            pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_GDTR;
            AssertCompile(sizeof(Gdtr) == sizeof(X86XDTR64));
            memcpy(&pVCpu->hm.s.vmx.RestoreHost.HostGdtr, &Gdtr, sizeof(X86XDTR64));
        }

        /*
         * IDT limit is practically 0xfff. Therefore if the host has the limit as 0xfff, VT-x bloating the limit to 0xffff
         * is not a problem as it's not possible to get at them anyway. See Intel spec. 6.14.1 "64-Bit Mode IDT" and
         * Intel spec. 6.2 "Exception and Interrupt Vectors".
         */
        if (Idtr.cbIdt < 0x0fff)
        {
            pVCpu->hm.s.vmx.fRestoreHostFlags |= VMX_RESTORE_HOST_IDTR;
            AssertCompile(sizeof(Idtr) == sizeof(X86XDTR64));
            memcpy(&pVCpu->hm.s.vmx.RestoreHost.HostIdtr, &Idtr, sizeof(X86XDTR64));
        }
#endif
    }

    /*
     * Host TR base. Verify that TR selector doesn't point past the GDT. Masking off the TI and RPL bits
     * is effectively what the CPU does for "scaling by 8". TI is always 0 and RPL should be too in most cases.
     */
    if ((uSelTR & X86_SEL_MASK) > Gdtr.cbGdt)
    {
        AssertMsgFailed(("hmR0VmxSaveHostSegmentRegs: TR selector exceeds limit. TR=%RTsel cbGdt=%#x\n", uSelTR, Gdtr.cbGdt));
        return VERR_VMX_INVALID_HOST_STATE;
    }

    PCX86DESCHC pDesc = (PCX86DESCHC)(Gdtr.pGdt + (uSelTR & X86_SEL_MASK));
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        /* We need the 64-bit TR base for hybrid darwin. */
        uint64_t u64TRBase = X86DESC64_BASE((PX86DESC64)pDesc);
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_TR_BASE, u64TRBase);
    }
    else
#endif
    {
        uintptr_t uTRBase;
#if HC_ARCH_BITS == 64
        uTRBase = X86DESC64_BASE(pDesc);
#else
        uTRBase = X86DESC_BASE(pDesc);
#endif
        rc = VMXWriteVmcsHstN(VMX_VMCS_HOST_TR_BASE, uTRBase);
    }
    AssertRCReturn(rc, rc);

    /*
     * Host FS base and GS base.
     */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        uint64_t u64FSBase = ASMRdMsr(MSR_K8_FS_BASE);
        uint64_t u64GSBase = ASMRdMsr(MSR_K8_GS_BASE);
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_FS_BASE, u64FSBase);          AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_GS_BASE, u64GSBase);          AssertRCReturn(rc, rc);

# if HC_ARCH_BITS == 64
        /* Store the base if we have to restore FS or GS manually as we need to restore the base as well. */
        if (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_SEL_FS)
            pVCpu->hm.s.vmx.RestoreHost.uHostFSBase = u64FSBase;
        if (pVCpu->hm.s.vmx.fRestoreHostFlags & VMX_RESTORE_HOST_SEL_GS)
            pVCpu->hm.s.vmx.RestoreHost.uHostGSBase = u64GSBase;
# endif
    }
#endif
    return rc;
}


/**
 * Saves certain host MSRs in the VM-Exit MSR-load area and some in the
 * host-state area of the VMCS. Theses MSRs will be automatically restored on
 * the host after every successful VM exit.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
DECLINLINE(int) hmR0VmxSaveHostMsrs(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVCpu);
    AssertPtr(pVCpu->hm.s.vmx.pvHostMsr);

    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
    PVMXMSR  pHostMsr           = (PVMXMSR)pVCpu->hm.s.vmx.pvHostMsr;
    uint32_t cHostMsrs          = 0;
    uint32_t u32HostExtFeatures = pVM->hm.s.cpuid.u32AMDFeatureEDX;

    if (u32HostExtFeatures & (X86_CPUID_EXT_FEATURE_EDX_NX | X86_CPUID_EXT_FEATURE_EDX_LONG_MODE))
    {
        uint64_t u64HostEfer = ASMRdMsr(MSR_K6_EFER);

# if HC_ARCH_BITS == 64
        /* Paranoia. 64-bit code requires these bits to be set always. */
        Assert((u64HostEfer & (MSR_K6_EFER_LMA | MSR_K6_EFER_LME)) == (MSR_K6_EFER_LMA | MSR_K6_EFER_LME));

        /*
         * We currently do not save/restore host EFER, we just make sure it doesn't get modified by VT-x operation.
         * All guest accesses (read, write) on EFER cause VM-exits. If we are to conditionally load the guest EFER for
         * some reason (e.g. allow transparent reads) we would activate the code below.
         */
#  if 0
        /* All our supported 64-bit host platforms must have NXE bit set. Otherwise we can change the below code to save EFER. */
        Assert(u64HostEfer & (MSR_K6_EFER_NXE));
        /* The SCE bit is only applicable in 64-bit mode. Save EFER if it doesn't match what the guest has.
           See Intel spec. 30.10.4.3 "Handling the SYSCALL and SYSRET Instructions". */
        if (CPUMIsGuestInLongMode(pVCpu))
        {
            uint64_t u64GuestEfer;
            rc = CPUMQueryGuestMsr(pVCpu, MSR_K6_EFER, &u64GuestEfer);
            AssertRC(rc);

            if ((u64HostEfer & MSR_K6_EFER_SCE) != (u64GuestEfer & MSR_K6_EFER_SCE))
            {
                pHostMsr->u32IndexMSR = MSR_K6_EFER;
                pHostMsr->u32Reserved = 0;
                pHostMsr->u64Value    = u64HostEfer;
                pHostMsr++; cHostMsrs++;
            }
        }
#  endif
# else  /* HC_ARCH_BITS != 64 */
        pHostMsr->u32IndexMSR = MSR_K6_EFER;
        pHostMsr->u32Reserved = 0;
# if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        if (CPUMIsGuestInLongMode(pVCpu))
        {
            /* Must match the EFER value in our 64 bits switcher. */
            pHostMsr->u64Value = u64HostEfer | MSR_K6_EFER_LME | MSR_K6_EFER_SCE | MSR_K6_EFER_NXE;
        }
        else
#  endif
            pHostMsr->u64Value = u64HostEfer;
        pHostMsr++; cHostMsrs++;
# endif  /* HC_ARCH_BITS == 64 */
    }

# if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        pHostMsr->u32IndexMSR  = MSR_K6_STAR;
        pHostMsr->u32Reserved  = 0;
        pHostMsr->u64Value     = ASMRdMsr(MSR_K6_STAR);              /* legacy syscall eip, cs & ss */
        pHostMsr++; cHostMsrs++;
        pHostMsr->u32IndexMSR  = MSR_K8_LSTAR;
        pHostMsr->u32Reserved  = 0;
        pHostMsr->u64Value     = ASMRdMsr(MSR_K8_LSTAR);             /* 64-bit mode syscall rip */
        pHostMsr++; cHostMsrs++;
        pHostMsr->u32IndexMSR  = MSR_K8_SF_MASK;
        pHostMsr->u32Reserved  = 0;
        pHostMsr->u64Value     = ASMRdMsr(MSR_K8_SF_MASK);           /* syscall flag mask */
        pHostMsr++; cHostMsrs++;
        pHostMsr->u32IndexMSR = MSR_K8_KERNEL_GS_BASE;
        pHostMsr->u32Reserved = 0;
        pHostMsr->u64Value    = ASMRdMsr(MSR_K8_KERNEL_GS_BASE);     /* swapgs exchange value */
        pHostMsr++; cHostMsrs++;
    }
# endif

    /* Shouldn't ever happen but there -is- a number. We're well within the recommended 512. */
    if (RT_UNLIKELY(cHostMsrs > MSR_IA32_VMX_MISC_MAX_MSR(pVM->hm.s.vmx.msr.vmx_misc)))
    {
        LogRel(("cHostMsrs=%u Cpu=%u\n", cHostMsrs, (unsigned)MSR_IA32_VMX_MISC_MAX_MSR(pVM->hm.s.vmx.msr.vmx_misc)));
        return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
    }

    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT, cHostMsrs);
#endif  /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

    /*
     * Host Sysenter MSRs.
     */
    rc = VMXWriteVmcs32(VMX_VMCS32_HOST_SYSENTER_CS,        ASMRdMsr_Low(MSR_IA32_SYSENTER_CS));
    AssertRCReturn(rc, rc);
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    if (HMVMX_IS_64BIT_HOST_MODE())
    {
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_SYSENTER_ESP,     ASMRdMsr(MSR_IA32_SYSENTER_ESP));
        AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs64(VMX_VMCS_HOST_SYSENTER_EIP,     ASMRdMsr(MSR_IA32_SYSENTER_EIP));
    }
    else
    {
        rc = VMXWriteVmcs32(VMX_VMCS_HOST_SYSENTER_ESP,     ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
        AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs32(VMX_VMCS_HOST_SYSENTER_EIP,     ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
    }
#elif HC_ARCH_BITS == 32
    rc = VMXWriteVmcs32(VMX_VMCS_HOST_SYSENTER_ESP,         ASMRdMsr_Low(MSR_IA32_SYSENTER_ESP));
    AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS_HOST_SYSENTER_EIP,         ASMRdMsr_Low(MSR_IA32_SYSENTER_EIP));
#else
    rc = VMXWriteVmcs64(VMX_VMCS_HOST_SYSENTER_ESP,         ASMRdMsr(MSR_IA32_SYSENTER_ESP));
    AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs64(VMX_VMCS_HOST_SYSENTER_EIP,         ASMRdMsr(MSR_IA32_SYSENTER_EIP));
#endif
    AssertRCReturn(rc, rc);

    /** @todo IA32_PERF_GLOBALCTRL, IA32_PAT, IA32_EFER, also see
     *        hmR0VmxSetupExitCtls() !! */
    return rc;
}


/**
 * Sets up VM-entry controls in the VMCS. These controls can affect things done
 * on VM-exit; e.g. "load debug controls", see Intel spec. 24.8.1 "VM-entry
 * controls".
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxLoadGuestEntryCtls(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_VMX_ENTRY_CTLS)
    {
        PVM pVM      = pVCpu->CTX_SUFF(pVM);
        uint32_t val = pVM->hm.s.vmx.msr.vmx_entry.n.disallowed0;            /* Bits set here must be set in the VMCS. */
        uint32_t zap = pVM->hm.s.vmx.msr.vmx_entry.n.allowed1;               /* Bits cleared here must be cleared in the VMCS. */

        /* Load debug controls (DR7 & IA32_DEBUGCTL_MSR). The first VT-x capable CPUs only supports the 1-setting of this bit. */
        val |= VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG;

        /* Set if the guest is in long mode. This will set/clear the EFER.LMA bit on VM-entry. */
        if (CPUMIsGuestInLongModeEx(pMixedCtx))
            val |= VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST;
        else
            Assert(!(val & VMX_VMCS_CTRL_ENTRY_IA32E_MODE_GUEST));

        /*
         * The following should not be set (since we're not in SMM mode):
         * - VMX_VMCS_CTRL_ENTRY_ENTRY_SMM
         * - VMX_VMCS_CTRL_ENTRY_DEACTIVATE_DUALMON
         */

        /** @todo VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_PERF_MSR,
         *        VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_PAT_MSR,
         *  VMX_VMCS_CTRL_ENTRY_LOAD_GUEST_EFER_MSR */

        if ((val & zap) != val)
        {
            LogRel(("hmR0VmxLoadGuestEntryCtls: invalid VM-entry controls combo! cpu=%RX64 val=%RX64 zap=%RX64\n",
                    pVM->hm.s.vmx.msr.vmx_entry.n.disallowed0, val, zap));
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }

        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY, val);
        AssertRCReturn(rc, rc);

        /* Update VCPU with the currently set VM-exit controls. */
        pVCpu->hm.s.vmx.u32EntryCtls = val;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_VMX_ENTRY_CTLS;
    }
    return rc;
}


/**
 * Sets up the VM-exit controls in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks requires EFER.
 */
DECLINLINE(int) hmR0VmxLoadGuestExitCtls(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_VMX_EXIT_CTLS)
    {
        PVM pVM      = pVCpu->CTX_SUFF(pVM);
        uint32_t val = pVM->hm.s.vmx.msr.vmx_exit.n.disallowed0;            /* Bits set here must be set in the VMCS. */
        uint32_t zap = pVM->hm.s.vmx.msr.vmx_exit.n.allowed1;               /* Bits cleared here must be cleared in the VMCS. */

        /* Save debug controls (DR7 & IA32_DEBUGCTL_MSR). The first VT-x CPUs only supported the 1-setting of this bit. */
        val |= VMX_VMCS_CTRL_EXIT_SAVE_DEBUG;

        /*
         * Set the host long mode active (EFER.LMA) bit (which Intel calls "Host address-space size") if necessary.
         * On VM-exit, VT-x sets both the host EFER.LMA and EFER.LME bit to this value. See assertion in hmR0VmxSaveHostMsrs().
         */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        if (HMVMX_IS_64BIT_HOST_MODE())
            val |= VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE;
        else
            Assert(!(val & VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE));
#elif HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
        if (CPUMIsGuestInLongModeEx(pMixedCtx))
            val |= VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE;    /* The switcher goes to long mode. */
        else
            Assert(!(val & VMX_VMCS_CTRL_EXIT_HOST_ADDR_SPACE_SIZE));
#endif

        /* Don't acknowledge external interrupts on VM-exit. We want to let the host do that. */
        Assert(!(val & VMX_VMCS_CTRL_EXIT_ACK_EXT_INT));

        /** @todo VMX_VMCS_CTRL_EXIT_LOAD_PERF_MSR,
         *        VMX_VMCS_CTRL_EXIT_SAVE_GUEST_PAT_MSR,
         *        VMX_VMCS_CTRL_EXIT_LOAD_HOST_PAT_MSR,
         *        VMX_VMCS_CTRL_EXIT_SAVE_GUEST_EFER_MSR,
         *        VMX_VMCS_CTRL_EXIT_LOAD_HOST_EFER_MSR. */

        if (pVM->hm.s.vmx.msr.vmx_exit.n.allowed1 & VMX_VMCS_CTRL_EXIT_SAVE_VMX_PREEMPT_TIMER)
            val |= VMX_VMCS_CTRL_EXIT_SAVE_VMX_PREEMPT_TIMER;

        if ((val & zap) != val)
        {
            LogRel(("hmR0VmxSetupProcCtls: invalid VM-exit controls combo! cpu=%RX64 val=%RX64 zap=%RX64\n",
                    pVM->hm.s.vmx.msr.vmx_exit.n.disallowed0, val, zap));
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }

        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT, val);
        AssertRCReturn(rc, rc);

        /* Update VCPU with the currently set VM-exit controls. */
        pVCpu->hm.s.vmx.u32ExitCtls = val;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_VMX_EXIT_CTLS;
    }
    return rc;
}


/**
 * Loads the guest APIC and related state.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 */
DECLINLINE(int) hmR0VmxLoadGuestApicState(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_VMX_GUEST_APIC_STATE)
    {
        /* Setup TPR shadowing. Also setup TPR patching for 32-bit guests. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
        {
            Assert(pVCpu->hm.s.vmx.HCPhysVirtApic);

            bool    fPendingIntr  = false;
            uint8_t u8Tpr         = 0;
            uint8_t u8PendingIntr = 0;
            rc = PDMApicGetTPR(pVCpu, &u8Tpr, &fPendingIntr, &u8PendingIntr);
            AssertRCReturn(rc, rc);

            /*
             * If there are external interrupts pending but masked by the TPR value, instruct VT-x to cause a VM-exit when
             * the guest lowers its TPR below the highest-priority pending interrupt and we can deliver the interrupt.
             * If there are no external interrupts pending, set threshold to 0 to not cause a VM-exit. We will eventually deliver
             * the interrupt when we VM-exit for other reasons.
             */
            pVCpu->hm.s.vmx.pbVirtApic[0x80] = u8Tpr;            /* Offset 0x80 is TPR in the APIC MMIO range. */
            uint32_t u32TprThreshold = 0;
            if (fPendingIntr)
            {
                /* Bits 3-0 of the TPR threshold field correspond to bits 7-4 of the TPR (which is the Task-Priority Class). */
                const uint8_t u8PendingPriority = (u8PendingIntr >> 4);
                const uint8_t u8TprPriority     = (u8Tpr >> 4) & 7;
                if (u8PendingPriority <= u8TprPriority)
                    u32TprThreshold = u8PendingPriority;
                else
                    u32TprThreshold = u8TprPriority;             /* Required for Vista 64-bit guest, see @bugref{6398}. */
            }
            Assert(!(u32TprThreshold & 0xfffffff0));             /* Bits 31:4 MBZ. */

            rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_TPR_THRESHOLD, u32TprThreshold);
            AssertRCReturn(rc, rc);
        }

        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_VMX_GUEST_APIC_STATE;
    }
    return rc;
}


/**
 * Gets the guest's interruptibility-state ("interrupt shadow" as AMD calls it).
 *
 * @returns Guest's interruptibility-state.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 * @remarks Has side-effects with VMCPU_FF_INHIBIT_INTERRUPTS force-flag.
 */
DECLINLINE(uint32_t) hmR0VmxGetGuestIntrState(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    /*
     * Instructions like STI and MOV SS inhibit interrupts till the next instruction completes. Check if we should
     * inhibit interrupts or clear any existing interrupt-inhibition.
     */
    uint32_t uIntrState = 0;
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
    {
        /* If inhibition is active, RIP & RFLAGS should've been accessed (i.e. read previously from the VMCS or from ring-3). */
        AssertMsg((pVCpu->hm.s.vmx.fUpdatedGuestState & (HMVMX_UPDATED_GUEST_RIP | HMVMX_UPDATED_GUEST_RFLAGS))
                   == (HMVMX_UPDATED_GUEST_RIP | HMVMX_UPDATED_GUEST_RFLAGS), ("%#x\n", pVCpu->hm.s.vmx.fUpdatedGuestState));
        if (pMixedCtx->rip != EMGetInhibitInterruptsPC(pVCpu))
        {
            /*
             * We can clear the inhibit force flag as even if we go back to the recompiler without executing guest code in
             * VT-x, the flag's condition to be cleared is met and thus the cleared state is correct.
             */
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
        }
        else if (pMixedCtx->eflags.Bits.u1IF)
            uIntrState = VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI;
        else
            uIntrState = VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS;
    }
    return uIntrState;
}


/**
 * Loads the guest's interruptibility-state into the guest-state area in the
 * VMCS.
 *
 * @returns VBox status code.
 * @param pVCpu         Pointer to the VMCPU.
 * @param uIntrState    The interruptibility-state to set.
 */
static int hmR0VmxLoadGuestIntrState(PVMCPU pVCpu, uint32_t uIntrState)
{
    AssertMsg(!(uIntrState & 0xfffffff0), ("%#x\n", uIntrState));   /* Bits 31:4 MBZ. */
    Assert((uIntrState & 0x3) != 0x3);                              /* Block-by-STI and MOV SS cannot be simultaneously set. */
    int rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, uIntrState);
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Loads the guest's RIP into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestRip(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_RIP)
    {
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_RIP, pMixedCtx->rip);
        AssertRCReturn(rc, rc);
        Log4(("Load: VMX_VMCS_GUEST_RIP=%#RX64\n", pMixedCtx->rip));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_RIP;
    }
    return rc;
}


/**
 * Loads the guest's RSP into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestRsp(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_RSP)
    {
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_RSP, pMixedCtx->rsp);
        AssertRCReturn(rc, rc);
        Log4(("Load: VMX_VMCS_GUEST_RSP=%#RX64\n", pMixedCtx->rsp));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_RSP;
    }
    return rc;
}


/**
 * Loads the guest's RFLAGS into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestRflags(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_RFLAGS)
    {
        /* Intel spec. 2.3.1 "System Flags and Fields in IA-32e Mode" claims the upper 32-bits of RFLAGS are reserved (MBZ).
           Let us assert it as such and use 32-bit VMWRITE. */
        Assert(!(pMixedCtx->rflags.u64 >> 32));
        X86EFLAGS uEFlags = pMixedCtx->eflags;
        uEFlags.u32 &= VMX_EFLAGS_RESERVED_0;                   /* Bits 22-31, 15, 5 & 3 MBZ. */
        uEFlags.u32 |= VMX_EFLAGS_RESERVED_1;                   /* Bit 1 MB1. */

        /*
         * If we're emulating real-mode using Virtual 8086 mode, save the real-mode eflags so we can restore them on VM exit.
         * Modify the real-mode guest's eflags so that VT-x can run the real-mode guest code under Virtual 8086 mode.
         */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.pRealModeTSS);
            Assert(PDMVmmDevHeapIsEnabled(pVCpu->CTX_SUFF(pVM)));
            pVCpu->hm.s.vmx.RealMode.eflags.u32 = uEFlags.u32; /* Save the original eflags of the real-mode guest. */
            uEFlags.Bits.u1VM   = 1;                           /* Set the Virtual 8086 mode bit. */
            uEFlags.Bits.u2IOPL = 0;                           /* Change IOPL to 0, otherwise certain instructions won't fault. */
        }

        rc = VMXWriteVmcs32(VMX_VMCS_GUEST_RFLAGS, uEFlags.u32);
        AssertRCReturn(rc, rc);

        Log4(("Load: VMX_VMCS_GUEST_RFLAGS=%#RX32\n", uEFlags.u32));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_RFLAGS;
    }
    return rc;
}


/**
 * Loads the guest RIP, RSP and RFLAGS into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxLoadGuestRipRspRflags(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = hmR0VmxLoadGuestRip(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);
    rc     = hmR0VmxLoadGuestRsp(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);
    rc     = hmR0VmxLoadGuestRflags(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Loads the guest control registers (CR0, CR3, CR4) into the guest-state area
 * in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestControlRegs(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    int rc  = VINF_SUCCESS;
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Guest CR0.
     * Guest FPU.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR0)
    {
        Assert(!(pCtx->cr0 >> 32));
        uint32_t u32GuestCR0 = pCtx->cr0;

        /* The guest's view (read access) of its CR0 is unblemished. */
        rc  = VMXWriteVmcs32(VMX_VMCS_CTRL_CR0_READ_SHADOW, u32GuestCR0);
        AssertRCReturn(rc, rc);
        Log4(("Load: VMX_VMCS_CTRL_CR0_READ_SHADOW=%#RX32\n", u32GuestCR0));

        /* Setup VT-x's view of the guest CR0. */
        /* Minimize VM-exits due to CR3 changes when we have NestedPaging. */
        if (pVM->hm.s.fNestedPaging)
        {
            if (CPUMIsGuestPagingEnabledEx(pCtx))
            {
                /* The guest has paging enabled, let it access CR3 without causing a VM exit if supported. */
                pVCpu->hm.s.vmx.u32ProcCtls &= ~(  VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT
                                                 | VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT);
            }
            else
            {
                /* The guest doesn't have paging enabled, make CR3 access to cause VM exits to update our shadow. */
                pVCpu->hm.s.vmx.u32ProcCtls |=   VMX_VMCS_CTRL_PROC_EXEC_CR3_LOAD_EXIT
                                               | VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT;
            }

            /* If we have unrestricted guest execution, we never have to intercept CR3 reads. */
            if (pVM->hm.s.vmx.fUnrestrictedGuest)
                pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_CR3_STORE_EXIT;

            rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
            AssertRCReturn(rc, rc);
        }
        else
            u32GuestCR0 |= X86_CR0_WP;     /* Guest CPL 0 writes to its read-only pages should cause a #PF VM-exit. */

        /*
         * Guest FPU bits.
         * Intel spec. 23.8 "Restrictions on VMX operation" mentions that CR0.NE bit must always be set on the first
         * CPUs to support VT-x and no mention of with regards to UX in VM-entry checks.
         */
        u32GuestCR0 |= X86_CR0_NE;
        bool fInterceptNM = false;
        if (CPUMIsGuestFPUStateActive(pVCpu))
        {
            fInterceptNM = false;              /* Guest FPU active, no need to VM-exit on #NM. */
            /* The guest should still get #NM exceptions when it expects it to, so we should not clear TS & MP bits here.
               We're only concerned about -us- not intercepting #NMs when the guest-FPU is active. Not the guest itself! */
        }
        else
        {
            fInterceptNM = true;               /* Guest FPU inactive, VM-exit on #NM for lazy FPU loading. */
            u32GuestCR0 |=  X86_CR0_TS         /* Guest can task switch quickly and do lazy FPU syncing. */
                          | X86_CR0_MP;        /* FWAIT/WAIT should not ignore CR0.TS and should generate #NM. */
        }

        /* Catch floating point exceptions if we need to report them to the guest in a different way. */
        bool fInterceptMF = false;
        if (!(pCtx->cr0 & X86_CR0_NE))
            fInterceptMF = true;

        /* Finally, intercept all exceptions as we cannot directly inject them in real-mode, see hmR0VmxInjectEventVmcs(). */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            Assert(PDMVmmDevHeapIsEnabled(pVM));
            Assert(pVM->hm.s.vmx.pRealModeTSS);
            pVCpu->hm.s.vmx.u32XcptBitmap |= HMVMX_REAL_MODE_XCPT_MASK;
            fInterceptNM = true;
            fInterceptMF = true;
        }
        else
            pVCpu->hm.s.vmx.u32XcptBitmap &= ~HMVMX_REAL_MODE_XCPT_MASK;

        if (fInterceptNM)
            pVCpu->hm.s.vmx.u32XcptBitmap |= RT_BIT(X86_XCPT_NM);
        else
            pVCpu->hm.s.vmx.u32XcptBitmap &= ~RT_BIT(X86_XCPT_NM);

        if (fInterceptMF)
            pVCpu->hm.s.vmx.u32XcptBitmap |= RT_BIT(X86_XCPT_MF);
        else
            pVCpu->hm.s.vmx.u32XcptBitmap &= ~RT_BIT(X86_XCPT_MF);

        /* Additional intercepts for debugging, define these yourself explicitly. */
#ifdef HMVMX_ALWAYS_TRAP_ALL_XCPTS
        pVCpu->hm.s.vmx.u32XcptBitmap |=   RT_BIT(X86_XCPT_BP)
                                         | RT_BIT(X86_XCPT_DB)
                                         | RT_BIT(X86_XCPT_DE)
                                         | RT_BIT(X86_XCPT_NM)
                                         | RT_BIT(X86_XCPT_UD)
                                         | RT_BIT(X86_XCPT_NP)
                                         | RT_BIT(X86_XCPT_SS)
                                         | RT_BIT(X86_XCPT_GP)
                                         | RT_BIT(X86_XCPT_PF)
                                         | RT_BIT(X86_XCPT_MF);
#elif defined(HMVMX_ALWAYS_TRAP_PF)
        pVCpu->hm.s.vmx.u32XcptBitmap    |= RT_BIT(X86_XCPT_PF);
#endif

        Assert(pVM->hm.s.fNestedPaging || (pVCpu->hm.s.vmx.u32XcptBitmap & RT_BIT(X86_XCPT_PF)));

        /* Set/clear the CR0 specific bits along with their exceptions (PE, PG, CD, NW). */
        uint32_t uSetCR0 = (uint32_t)(pVM->hm.s.vmx.msr.vmx_cr0_fixed0 & pVM->hm.s.vmx.msr.vmx_cr0_fixed1);
        uint32_t uZapCR0 = (uint32_t)(pVM->hm.s.vmx.msr.vmx_cr0_fixed0 | pVM->hm.s.vmx.msr.vmx_cr0_fixed1);
        if (pVM->hm.s.vmx.fUnrestrictedGuest)               /* Exceptions for unrestricted-guests for fixed CR0 bits (PE, PG). */
            uSetCR0 &= ~(X86_CR0_PE | X86_CR0_PG);
        else
            Assert((uSetCR0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG));

        u32GuestCR0 |= uSetCR0;
        u32GuestCR0 &= uZapCR0;
        u32GuestCR0 &= ~(X86_CR0_CD | X86_CR0_NW);          /* Always enable caching. */

        /* Write VT-x's view of the guest CR0 into the VMCS and update the exception bitmap. */
        rc = VMXWriteVmcs32(VMX_VMCS_GUEST_CR0, u32GuestCR0);
        AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, pVCpu->hm.s.vmx.u32XcptBitmap);
        AssertRCReturn(rc, rc);
        Log4(("Load: VMX_VMCS_GUEST_CR0=%#RX32 (uSetCR0=%#RX32 uZapCR0=%#RX32)\n", u32GuestCR0, uSetCR0, uZapCR0));

        /*
         * CR0 is shared between host and guest along with a CR0 read shadow. Therefore, certain bits must not be changed
         * by the guest because VT-x ignores saving/restoring them (namely CD, ET, NW) and for certain other bits
         * we want to be notified immediately of guest CR0 changes (e.g. PG to update our shadow page tables).
         */
        uint32_t u32CR0Mask = 0;
        u32CR0Mask =  X86_CR0_PE
                    | X86_CR0_NE
                    | X86_CR0_WP
                    | X86_CR0_PG
                    | X86_CR0_ET    /* Bit ignored on VM-entry and VM-exit. Don't let the guest modify the host CR0.ET */
                    | X86_CR0_CD    /* Bit ignored on VM-entry and VM-exit. Don't let the guest modify the host CR0.CD */
                    | X86_CR0_NW;   /* Bit ignored on VM-entry and VM-exit. Don't let the guest modify the host CR0.NW */
        if (pVM->hm.s.vmx.fUnrestrictedGuest)
            u32CR0Mask &= ~X86_CR0_PE;
        if (pVM->hm.s.fNestedPaging)
            u32CR0Mask &= ~X86_CR0_WP;

        /* If the guest FPU state is active, don't need to VM-exit on writes to FPU related bits in CR0. */
        if (fInterceptNM)
            u32CR0Mask |=  (X86_CR0_TS | X86_CR0_MP);
        else
            u32CR0Mask &= ~(X86_CR0_TS | X86_CR0_MP);

        /* Write the CR0 mask into the VMCS and update the VCPU's copy of the current CR0 mask. */
        pVCpu->hm.s.vmx.u32CR0Mask = u32CR0Mask;
        rc = VMXWriteVmcs32(VMX_VMCS_CTRL_CR0_MASK, u32CR0Mask);
        AssertRCReturn(rc, rc);

        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR0;
    }

    /*
     * Guest CR2.
     * It's always loaded in the assembler code. Nothing to do here.
     */

    /*
     * Guest CR3.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR3)
    {
        RTGCPHYS GCPhysGuestCR3 = NIL_RTGCPHYS;
        if (pVM->hm.s.fNestedPaging)
        {
            pVCpu->hm.s.vmx.HCPhysEPTP = PGMGetHyperCR3(pVCpu);

            /* Validate. See Intel spec. 28.2.2 "EPT Translation Mechanism" and 24.6.11 "Extended-Page-Table Pointer (EPTP)" */
            Assert(pVCpu->hm.s.vmx.HCPhysEPTP);
            Assert(!(pVCpu->hm.s.vmx.HCPhysEPTP & UINT64_C(0xfff0000000000000)));
            Assert(!(pVCpu->hm.s.vmx.HCPhysEPTP & 0xfff));

            /* VMX_EPT_MEMTYPE_WB support is already checked in hmR0VmxSetupTaggedTlb(). */
            pVCpu->hm.s.vmx.HCPhysEPTP |=   VMX_EPT_MEMTYPE_WB
                                          | (VMX_EPT_PAGE_WALK_LENGTH_DEFAULT << VMX_EPT_PAGE_WALK_LENGTH_SHIFT);

            /* Validate. See Intel spec. 26.2.1 "Checks on VMX Controls" */
            AssertMsg(   ((pVCpu->hm.s.vmx.HCPhysEPTP >> 3) & 0x07) == 3      /* Bits 3:5 (EPT page walk length - 1) must be 3. */
                      && ((pVCpu->hm.s.vmx.HCPhysEPTP >> 6) & 0x3f) == 0,     /* Bits 6:11 MBZ. */
                         ("EPTP %#RX64\n", pVCpu->hm.s.vmx.HCPhysEPTP));

            rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_EPTP_FULL, pVCpu->hm.s.vmx.HCPhysEPTP);
            AssertRCReturn(rc, rc);
            Log4(("Load: VMX_VMCS64_CTRL_EPTP_FULL=%#RX64\n", pVCpu->hm.s.vmx.HCPhysEPTP));

            if (   pVM->hm.s.vmx.fUnrestrictedGuest
                || CPUMIsGuestPagingEnabledEx(pCtx))
            {
                /* If the guest is in PAE mode, pass the PDPEs to VT-x using the VMCS fields. */
                if (CPUMIsGuestInPAEModeEx(pCtx))
                {
                    rc  = PGMGstGetPaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);                         AssertRCReturn(rc, rc);
                    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE0_FULL, pVCpu->hm.s.aPdpes[0].u);     AssertRCReturn(rc, rc);
                    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE1_FULL, pVCpu->hm.s.aPdpes[1].u);     AssertRCReturn(rc, rc);
                    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE2_FULL, pVCpu->hm.s.aPdpes[2].u);     AssertRCReturn(rc, rc);
                    rc = VMXWriteVmcs64(VMX_VMCS64_GUEST_PDPTE3_FULL, pVCpu->hm.s.aPdpes[3].u);     AssertRCReturn(rc, rc);
                }

                /* The guest's view of its CR3 is unblemished with Nested Paging when the guest is using paging or we
                   have Unrestricted Execution to handle the guest when it's not using paging. */
                GCPhysGuestCR3 = pCtx->cr3;
            }
            else
            {
                /*
                 * The guest is not using paging, but the CPU (VT-x) has to. While the guest thinks it accesses physical memory
                 * directly, we use our identity-mapped page table to map guest-linear to guest-physical addresses.
                 * EPT takes care of translating it to host-physical addresses.
                 */
                RTGCPHYS GCPhys;
                Assert(pVM->hm.s.vmx.pNonPagingModeEPTPageTable);
                Assert(PDMVmmDevHeapIsEnabled(pVM));

                /* We obtain it here every time as the guest could have relocated this PCI region. */
                rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                AssertRCReturn(rc, rc);

                GCPhysGuestCR3 = GCPhys;
            }

            Log4(("Load: VMX_VMCS_GUEST_CR3=%#RGv (GstN)\n", GCPhysGuestCR3));
            rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_CR3, GCPhysGuestCR3);
        }
        else
        {
            /* Non-nested paging case, just use the hypervisor's CR3. */
            RTHCPHYS HCPhysGuestCR3 = PGMGetHyperCR3(pVCpu);

            Log4(("Load: VMX_VMCS_GUEST_CR3=%#RHv (HstN)\n", HCPhysGuestCR3));
            rc = VMXWriteVmcsHstN(VMX_VMCS_GUEST_CR3, HCPhysGuestCR3);
        }
        AssertRCReturn(rc, rc);

        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR3;
    }

    /*
     * Guest CR4.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR4)
    {
        Assert(!(pCtx->cr4 >> 32));
        uint32_t u32GuestCR4 = pCtx->cr4;

        /* The guest's view of its CR4 is unblemished. */
        rc = VMXWriteVmcs32(VMX_VMCS_CTRL_CR4_READ_SHADOW, u32GuestCR4);
        AssertRCReturn(rc, rc);
        Log4(("Load: VMX_VMCS_CTRL_CR4_READ_SHADOW=%#RX32\n", u32GuestCR4));

        /* Setup VT-x's view of the guest CR4. */
        /*
         * If we're emulating real-mode using virtual-8086 mode, we want to redirect software interrupts to the 8086 program
         * interrupt handler. Clear the VME bit (the interrupt redirection bitmap is already all 0, see hmR3InitFinalizeR0())
         * See Intel spec. 20.2 "Software Interrupt Handling Methods While in Virtual-8086 Mode".
         */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            Assert(pVM->hm.s.vmx.pRealModeTSS);
            Assert(PDMVmmDevHeapIsEnabled(pVM));
            u32GuestCR4 &= ~X86_CR4_VME;
        }

        if (pVM->hm.s.fNestedPaging)
        {
            if (   !CPUMIsGuestPagingEnabledEx(pCtx)
                && !pVM->hm.s.vmx.fUnrestrictedGuest)
            {
                /* We use 4 MB pages in our identity mapping page table when the guest doesn't have paging. */
                u32GuestCR4 |= X86_CR4_PSE;
                /* Our identity mapping is a 32 bits page directory. */
                u32GuestCR4 &= ~X86_CR4_PAE;
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
                    u32GuestCR4 &= ~X86_CR4_PAE;
                    break;
                }

                case PGMMODE_PAE:               /* PAE paging. */
                case PGMMODE_PAE_NX:            /* PAE paging with NX. */
                {
                    u32GuestCR4 |= X86_CR4_PAE;
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
        uint64_t uSetCR4 = (pVM->hm.s.vmx.msr.vmx_cr4_fixed0 & pVM->hm.s.vmx.msr.vmx_cr4_fixed1);
        uint64_t uZapCR4 = (pVM->hm.s.vmx.msr.vmx_cr4_fixed0 | pVM->hm.s.vmx.msr.vmx_cr4_fixed1);
        u32GuestCR4 |= uSetCR4;
        u32GuestCR4 &= uZapCR4;

        /* Write VT-x's view of the guest CR4 into the VMCS. */
        Log4(("Load: VMX_VMCS_GUEST_CR4=%#RX32 (Set=%#RX32 Zap=%#RX32)\n", u32GuestCR4, uSetCR4, uZapCR4));
        rc = VMXWriteVmcs32(VMX_VMCS_GUEST_CR4, u32GuestCR4);
        AssertRCReturn(rc, rc);

        /* Setup CR4 mask. CR4 flags owned by the host, if the guest attempts to change them, that would cause a VM exit. */
        uint32_t u32CR4Mask = 0;
        u32CR4Mask =  X86_CR4_VME
                    | X86_CR4_PAE
                    | X86_CR4_PGE
                    | X86_CR4_PSE
                    | X86_CR4_VMXE;
        pVCpu->hm.s.vmx.u32CR4Mask = u32CR4Mask;
        rc = VMXWriteVmcs32(VMX_VMCS_CTRL_CR4_MASK, u32CR4Mask);
        AssertRCReturn(rc, rc);

        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR4;
    }
    return rc;
}


/**
 * Loads the guest debug registers into the guest-state area in the VMCS.
 * This also sets up whether #DB and MOV DRx accesses cause VM exits.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestDebugRegs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    if (!(pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_DEBUG))
        return VINF_SUCCESS;

#ifdef VBOX_STRICT
    /* Validate. Intel spec. 26.3.1.1 "Checks on Guest Controls Registers, Debug Registers, MSRs" */
    if (pVCpu->hm.s.vmx.u32EntryCtls & VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG)
    {
        Assert(!(pMixedCtx->dr[7] >> 32));                        /* upper 32 bits are reserved (MBZ). */
        /* Validate. Intel spec. 17.2 "Debug Registers", recompiler paranoia checks. */
        Assert((pMixedCtx->dr[7] & 0xd800) == 0);                 /* bits 15, 14, 12, 11 are reserved (MBZ). */
        Assert((pMixedCtx->dr[7] & 0x400) == 0x400);              /* bit 10 is reserved (MB1). */
    }
#endif

    int rc                = VERR_INTERNAL_ERROR_5;
    PVM pVM               = pVCpu->CTX_SUFF(pVM);
    bool fInterceptDB     = false;
    bool fInterceptMovDRx = false;
    if (DBGFIsStepping(pVCpu))
    {
        /* If the CPU supports the monitor trap flag, use it for single stepping in DBGF and avoid intercepting #DB. */
        if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG)
        {
            pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG;
            rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
            AssertRCReturn(rc, rc);
            Assert(fInterceptDB == false);
        }
        else
        {
            fInterceptDB = true;
            pMixedCtx->eflags.u32 |= X86_EFL_TF;
            pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RFLAGS;
        }
    }

    if (CPUMGetHyperDR7(pVCpu) & (X86_DR7_ENABLED_MASK | X86_DR7_GD))
    {
        if (!CPUMIsHyperDebugStateActive(pVCpu))
        {
            rc = CPUMR0LoadHyperDebugState(pVM, pVCpu, pMixedCtx, true /* include DR6 */);
            AssertRC(rc);
        }
        Assert(CPUMIsHyperDebugStateActive(pVCpu));
        fInterceptMovDRx = true;
    }
    else if (pMixedCtx->dr[7] & (X86_DR7_ENABLED_MASK | X86_DR7_GD))
    {
        if (!CPUMIsGuestDebugStateActive(pVCpu))
        {
            rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pMixedCtx, true /* include DR6 */);
            AssertRC(rc);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxArmed);
        }
        Assert(CPUMIsGuestDebugStateActive(pVCpu));
        Assert(fInterceptMovDRx == false);
    }
    else if (!CPUMIsGuestDebugStateActive(pVCpu))
    {
        /* For the first time we would need to intercept MOV DRx accesses even when the guest debug registers aren't loaded. */
        fInterceptMovDRx = true;
    }

    /* Update the exception bitmap regarding intercepting #DB generated by the guest. */
    if (fInterceptDB)
        pVCpu->hm.s.vmx.u32XcptBitmap |= RT_BIT(X86_XCPT_DB);
    else if (!pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
    {
#ifndef HMVMX_ALWAYS_TRAP_ALL_XCPTS
        pVCpu->hm.s.vmx.u32XcptBitmap &= ~RT_BIT(X86_XCPT_DB);
#endif
    }

    /* Update the processor-based VM-execution controls regarding intercepting MOV DRx instructions. */
    if (fInterceptMovDRx)
        pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT;
    else
        pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT;

    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXCEPTION_BITMAP, pVCpu->hm.s.vmx.u32XcptBitmap);
    AssertRCReturn(rc, rc);
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
    AssertRCReturn(rc, rc);

    /* The guest's view of its DR7 is unblemished. Use 32-bit write as upper 32-bits MBZ as asserted above. */
    rc = VMXWriteVmcs32(VMX_VMCS_GUEST_DR7, (uint32_t)pMixedCtx->dr[7]);
    AssertRCReturn(rc, rc);

    pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_DEBUG;
    return rc;
}


#ifdef VBOX_STRICT
/**
 * Strict function to validate segment registers.
 *
 * @remarks Requires CR0.
 */
static void hmR0VmxValidateSegmentRegs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /* Validate segment registers. See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers". */
    /* NOTE: The reason we check for attribute value 0 and not just the unusable bit here is because hmR0VmxWriteSegmentReg()
     * only updates the VMCS bits with the unusable bit and doesn't change the guest-context value. */
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
        Assert(pCtx->cs.Attr.u && !(pCtx->cs.Attr.u & HMVMX_SEL_UNUSABLE)); /** @todo is this really true even for 64-bit CS?!? */
        if (pCtx->cs.Attr.n.u4Type == 9 || pCtx->cs.Attr.n.u4Type == 11)
            Assert(pCtx->cs.Attr.n.u2Dpl == pCtx->ss.Attr.n.u2Dpl);
        else if (pCtx->cs.Attr.n.u4Type == 13 || pCtx->cs.Attr.n.u4Type == 15)
            Assert(pCtx->cs.Attr.n.u2Dpl <= pCtx->ss.Attr.n.u2Dpl);
        else
            AssertMsgFailed(("Invalid CS Type %#x\n", pCtx->cs.Attr.n.u2Dpl));
        /* SS */
        Assert((pCtx->ss.Sel & X86_SEL_RPL) == (pCtx->cs.Sel & X86_SEL_RPL));
        Assert(pCtx->ss.Attr.n.u2Dpl == (pCtx->ss.Sel & X86_SEL_RPL));
        Assert(!(pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR0));
        if (   !(pCtx->cr0 & X86_CR0_PE)
            || pCtx->cs.Attr.n.u4Type == 3)
        {
            Assert(!pCtx->ss.Attr.n.u2Dpl);
        }
        if (pCtx->ss.Attr.u && !(pCtx->ss.Attr.u & HMVMX_SEL_UNUSABLE))
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
        /* DS, ES, FS, GS - only check for usable selectors, see hmR0VmxWriteSegmentReg(). */
        if (pCtx->ds.Attr.u && !(pCtx->ds.Attr.u & HMVMX_SEL_UNUSABLE))
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
        if (pCtx->es.Attr.u && !(pCtx->es.Attr.u & HMVMX_SEL_UNUSABLE))
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
        if (pCtx->fs.Attr.u && !(pCtx->fs.Attr.u & HMVMX_SEL_UNUSABLE))
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
        if (pCtx->gs.Attr.u && !(pCtx->gs.Attr.u & HMVMX_SEL_UNUSABLE))
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
# if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        Assert(!(pCtx->cs.u64Base >> 32));
        Assert(!pCtx->ss.Attr.u || !(pCtx->ss.u64Base >> 32));
        Assert(!pCtx->ds.Attr.u || !(pCtx->ds.u64Base >> 32));
        Assert(!pCtx->es.Attr.u || !(pCtx->es.u64Base >> 32));
# endif
    }
    else if (   CPUMIsGuestInV86ModeEx(pCtx)
             || (   CPUMIsGuestInRealModeEx(pCtx)
                 && !pVM->hm.s.vmx.fUnrestrictedGuest))
    {
        /* Real and v86 mode checks. */
        /* hmR0VmxWriteSegmentReg() writes the modified in VMCS. We want what we're feeding to VT-x. */
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
# if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        Assert(!(pCtx->cs.u64Base >> 32));
        Assert(!u32SSAttr || !(pCtx->ss.u64Base >> 32));
        Assert(!u32DSAttr || !(pCtx->ds.u64Base >> 32));
        Assert(!u32ESAttr || !(pCtx->es.u64Base >> 32));
# endif
    }
}
#endif /* VBOX_STRICT */


/**
 * Writes a guest segment register into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idxSel      Index of the selector in the VMCS.
 * @param   idxLimit    Index of the segment limit in the VMCS.
 * @param   idxBase     Index of the segment base in the VMCS.
 * @param   idxAccess   Index of the access rights of the segment in the VMCS.
 * @param   pSelReg     Pointer to the segment selector.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxWriteSegmentReg(PVMCPU pVCpu, uint32_t idxSel, uint32_t idxLimit, uint32_t idxBase,
                                       uint32_t idxAccess, PCPUMSELREG pSelReg, PCPUMCTX pCtx)
{
    int rc = VMXWriteVmcs32(idxSel,    pSelReg->Sel);       /* 16-bit guest selector field. */
    AssertRCReturn(rc, rc);
    rc     = VMXWriteVmcs32(idxLimit,  pSelReg->u32Limit);  /* 32-bit guest segment limit field. */
    AssertRCReturn(rc, rc);
    rc     = VMXWriteVmcsGstN(idxBase, pSelReg->u64Base);   /* Natural width guest segment base field.*/
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
         * The way to differentiate between whether this is really a null selector or was just a selector loaded with 0 in
         * real-mode is using the segment attributes. A selector loaded in real-mode with the value 0 is valid and usable in
         * protected-mode and we should -not- mark it as an unusable segment. Both the recompiler & VT-x ensures NULL selectors
         * loaded in protected-mode have their attribute as 0.
         */
        if (!u32Access)
            u32Access = HMVMX_SEL_UNUSABLE;
    }

    /* Validate segment access rights. Refer to Intel spec. "26.3.1.2 Checks on Guest Segment Registers". */
    AssertMsg((u32Access & HMVMX_SEL_UNUSABLE) || (u32Access & X86_SEL_TYPE_ACCESSED),
              ("Access bit not set for usable segment. idx=%#x sel=%#x attr %#x\n", idxBase, pSelReg, pSelReg->Attr.u));

    rc = VMXWriteVmcs32(idxAccess, u32Access);           /* 32-bit guest segment access-rights field. */
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Loads the guest segment registers, GDTR, IDTR, LDTR, (TR, FS and GS bases)
 * into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCPU       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks Requires CR0 (strict builds validation).
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestSegmentRegs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc  = VERR_INTERNAL_ERROR_5;
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Guest Segment registers: CS, SS, DS, ES, FS, GS.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_SEGMENT_REGS)
    {
        /* Save the segment attributes for real-on-v86 mode hack, so we can restore them on VM-exit. */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            pVCpu->hm.s.vmx.RealMode.uAttrCS.u = pMixedCtx->cs.Attr.u;
            pVCpu->hm.s.vmx.RealMode.uAttrSS.u = pMixedCtx->ss.Attr.u;
            pVCpu->hm.s.vmx.RealMode.uAttrDS.u = pMixedCtx->ds.Attr.u;
            pVCpu->hm.s.vmx.RealMode.uAttrES.u = pMixedCtx->es.Attr.u;
            pVCpu->hm.s.vmx.RealMode.uAttrFS.u = pMixedCtx->fs.Attr.u;
            pVCpu->hm.s.vmx.RealMode.uAttrGS.u = pMixedCtx->gs.Attr.u;
        }

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
                Log4(("Load: Switch to protected mode detected!\n"));
                pVCpu->hm.s.vmx.fWasInRealMode = false;
            }
        }
#endif
        rc = hmR0VmxWriteSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_CS, VMX_VMCS32_GUEST_CS_LIMIT, VMX_VMCS_GUEST_CS_BASE,
                                     VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS, &pMixedCtx->cs, pMixedCtx);
        AssertRCReturn(rc, rc);
        rc = hmR0VmxWriteSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_SS, VMX_VMCS32_GUEST_SS_LIMIT, VMX_VMCS_GUEST_SS_BASE,
                                     VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS, &pMixedCtx->ss, pMixedCtx);
        AssertRCReturn(rc, rc);
        rc = hmR0VmxWriteSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_DS, VMX_VMCS32_GUEST_DS_LIMIT, VMX_VMCS_GUEST_DS_BASE,
                                     VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS, &pMixedCtx->ds, pMixedCtx);
        AssertRCReturn(rc, rc);
        rc = hmR0VmxWriteSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_ES, VMX_VMCS32_GUEST_ES_LIMIT, VMX_VMCS_GUEST_ES_BASE,
                                     VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS, &pMixedCtx->es, pMixedCtx);
        AssertRCReturn(rc, rc);
        rc = hmR0VmxWriteSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_FS, VMX_VMCS32_GUEST_FS_LIMIT, VMX_VMCS_GUEST_FS_BASE,
                                     VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS, &pMixedCtx->fs, pMixedCtx);
        AssertRCReturn(rc, rc);
        rc = hmR0VmxWriteSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_GS, VMX_VMCS32_GUEST_GS_LIMIT, VMX_VMCS_GUEST_GS_BASE,
                                     VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS, &pMixedCtx->gs, pMixedCtx);
        AssertRCReturn(rc, rc);

        Log4(("Load: CS=%#RX16 Base=%#RX64 Limit=%#RX32 Attr=%#RX32\n", pMixedCtx->cs.Sel, pMixedCtx->cs.u64Base,
             pMixedCtx->cs.u32Limit, pMixedCtx->cs.Attr.u));
#ifdef VBOX_STRICT
        hmR0VmxValidateSegmentRegs(pVM, pVCpu, pMixedCtx);
#endif
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_SEGMENT_REGS;
    }

    /*
     * Guest TR.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_TR)
    {
        /*
         * Real-mode emulation using virtual-8086 mode with CR4.VME. Interrupt redirection is achieved
         * using the interrupt redirection bitmap (all bits cleared to let the guest handle INT-n's) in the TSS.
         * See hmR3InitFinalizeR0() to see how pRealModeTSS is setup.
         */
        uint16_t u16Sel          = 0;
        uint32_t u32Limit        = 0;
        uint64_t u64Base         = 0;
        uint32_t u32AccessRights = 0;

        if (!pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            u16Sel          = pMixedCtx->tr.Sel;
            u32Limit        = pMixedCtx->tr.u32Limit;
            u64Base         = pMixedCtx->tr.u64Base;
            u32AccessRights = pMixedCtx->tr.Attr.u;
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
        AssertMsg(!(u32AccessRights & HMVMX_SEL_UNUSABLE), ("TR unusable bit is not clear!? %#x\n", u32AccessRights));
        Assert(!(u32AccessRights & RT_BIT(4)));                 /* System MBZ.*/
        Assert(u32AccessRights & RT_BIT(7));                    /* Present MB1.*/
        Assert(!(u32AccessRights & 0xf00));                     /* 11:8 MBZ. */
        Assert(!(u32AccessRights & 0xfffe0000));                /* 31:17 MBZ. */
        Assert(   (u32Limit & 0xfff) == 0xfff
               || !(u32AccessRights & RT_BIT(15)));             /* Granularity MBZ. */
        Assert(   !(pMixedCtx->tr.u32Limit & 0xfff00000)
               || (u32AccessRights & RT_BIT(15)));              /* Granularity MB1. */

        rc = VMXWriteVmcs32(VMX_VMCS16_GUEST_FIELD_TR,         u16Sel);                AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_TR_LIMIT,         u32Limit);              AssertRCReturn(rc, rc);
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_TR_BASE,          u64Base);               AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS, u32AccessRights);       AssertRCReturn(rc, rc);

        Log4(("Load: VMX_VMCS_GUEST_TR_BASE=%#RX64\n", u64Base));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_TR;
    }

    /*
     * Guest GDTR.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_GDTR)
    {
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_GDTR_LIMIT, pMixedCtx->gdtr.cbGdt);        AssertRCReturn(rc, rc);
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_GDTR_BASE,  pMixedCtx->gdtr.pGdt);         AssertRCReturn(rc, rc);

        Assert(!(pMixedCtx->gdtr.cbGdt & 0xffff0000));          /* Bits 31:16 MBZ. */
        Log4(("Load: VMX_VMCS_GUEST_GDTR_BASE=%#RX64\n", pMixedCtx->gdtr.pGdt));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_GDTR;
    }

    /*
     * Guest LDTR.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_LDTR)
    {
        /* The unusable bit is specific to VT-x, if it's a null selector mark it as an unusable segment. */
        uint32_t u32Access = 0;
        if (!pMixedCtx->ldtr.Attr.u)
            u32Access = HMVMX_SEL_UNUSABLE;
        else
            u32Access = pMixedCtx->ldtr.Attr.u;

        rc  = VMXWriteVmcs32(VMX_VMCS16_GUEST_FIELD_LDTR,         pMixedCtx->ldtr.Sel);         AssertRCReturn(rc, rc);
        rc |= VMXWriteVmcs32(VMX_VMCS32_GUEST_LDTR_LIMIT,         pMixedCtx->ldtr.u32Limit);    AssertRCReturn(rc, rc);
        rc |= VMXWriteVmcsGstN(VMX_VMCS_GUEST_LDTR_BASE,          pMixedCtx->ldtr.u64Base);     AssertRCReturn(rc, rc);
        rc |= VMXWriteVmcs32(VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS, u32Access);                   AssertRCReturn(rc, rc);

        /* Validate. */
        if (!(u32Access & HMVMX_SEL_UNUSABLE))
        {
            Assert(!(pMixedCtx->ldtr.Sel & RT_BIT(2)));              /* TI MBZ. */
            Assert(pMixedCtx->ldtr.Attr.n.u4Type == 2);              /* Type MB2 (LDT). */
            Assert(!pMixedCtx->ldtr.Attr.n.u1DescType);              /* System MBZ. */
            Assert(pMixedCtx->ldtr.Attr.n.u1Present == 1);           /* Present MB1. */
            Assert(!pMixedCtx->ldtr.Attr.n.u4LimitHigh);             /* 11:8 MBZ. */
            Assert(!(pMixedCtx->ldtr.Attr.u & 0xfffe0000));          /* 31:17 MBZ. */
            Assert(   (pMixedCtx->ldtr.u32Limit & 0xfff) == 0xfff
                   || !pMixedCtx->ldtr.Attr.n.u1Granularity);        /* Granularity MBZ. */
            Assert(   !(pMixedCtx->ldtr.u32Limit & 0xfff00000)
                   || pMixedCtx->ldtr.Attr.n.u1Granularity);         /* Granularity MB1. */
        }

        Log4(("Load: VMX_VMCS_GUEST_LDTR_BASE=%#RX64\n",  pMixedCtx->ldtr.u64Base));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_LDTR;
    }

    /*
     * Guest IDTR.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_IDTR)
    {
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_IDTR_LIMIT, pMixedCtx->idtr.cbIdt);         AssertRCReturn(rc, rc);
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_IDTR_BASE,  pMixedCtx->idtr.pIdt);          AssertRCReturn(rc, rc);

        Assert(!(pMixedCtx->idtr.cbIdt & 0xffff0000));          /* Bits 31:16 MBZ. */
        Log4(("Load: VMX_VMCS_GUEST_IDTR_BASE=%#RX64\n", pMixedCtx->idtr.pIdt));
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_IDTR;
    }

    return VINF_SUCCESS;
}


/**
 * Loads certain guest MSRs into the VM-entry MSR-load and VM-exit MSR-store
 * areas. These MSRs will automatically be loaded to the host CPU on every
 * successful VM entry and stored from the host CPU on every successful VM exit.
 * Also loads the sysenter MSRs into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestMsrs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    AssertPtr(pVCpu);
    AssertPtr(pVCpu->hm.s.vmx.pvGuestMsr);

    /*
     * MSRs covered by Auto-load/store: EFER, LSTAR, STAR, SF_MASK, TSC_AUX (RDTSCP).
     */
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_VMX_GUEST_AUTO_MSRS)
    {
#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        PVM pVM             = pVCpu->CTX_SUFF(pVM);
        PVMXMSR  pGuestMsr  = (PVMXMSR)pVCpu->hm.s.vmx.pvGuestMsr;
        uint32_t cGuestMsrs = 0;

        /* See Intel spec. 4.1.4 "Enumeration of Paging Features by CPUID". */
        /** @todo r=ramshankar: Optimize this further to do lazy restoration and only
         *        when the guest really is in 64-bit mode. */
        bool fSupportsLongMode = CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
        if (fSupportsLongMode)
        {
            pGuestMsr->u32IndexMSR = MSR_K8_LSTAR;
            pGuestMsr->u32Reserved = 0;
            pGuestMsr->u64Value    = pMixedCtx->msrLSTAR;           /* 64 bits mode syscall rip */
            pGuestMsr++; cGuestMsrs++;
            pGuestMsr->u32IndexMSR = MSR_K6_STAR;
            pGuestMsr->u32Reserved = 0;
            pGuestMsr->u64Value    = pMixedCtx->msrSTAR;            /* legacy syscall eip, cs & ss */
            pGuestMsr++; cGuestMsrs++;
            pGuestMsr->u32IndexMSR = MSR_K8_SF_MASK;
            pGuestMsr->u32Reserved = 0;
            pGuestMsr->u64Value    = pMixedCtx->msrSFMASK;          /* syscall flag mask */
            pGuestMsr++; cGuestMsrs++;
            pGuestMsr->u32IndexMSR = MSR_K8_KERNEL_GS_BASE;
            pGuestMsr->u32Reserved = 0;
            pGuestMsr->u64Value    = pMixedCtx->msrKERNELGSBASE;    /* swapgs exchange value */
            pGuestMsr++; cGuestMsrs++;
        }

        /*
         * RDTSCP requires the TSC_AUX MSR. Host and guest share the physical MSR. So we have to
         * load the guest's copy if the guest can execute RDTSCP without causing VM-exits.
         */
        if (   CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP)
            && (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP))
        {
            pGuestMsr->u32IndexMSR = MSR_K8_TSC_AUX;
            pGuestMsr->u32Reserved = 0;
            rc = CPUMQueryGuestMsr(pVCpu, MSR_K8_TSC_AUX, &pGuestMsr->u64Value);
            AssertRCReturn(rc, rc);
            pGuestMsr++; cGuestMsrs++;
        }

        /* Shouldn't ever happen but there -is- a number. We're well within the recommended 512. */
        if (cGuestMsrs > MSR_IA32_VMX_MISC_MAX_MSR(pVM->hm.s.vmx.msr.vmx_misc))
        {
            LogRel(("CPU autoload/store MSR count in VMCS exceeded cGuestMsrs=%u.\n", cGuestMsrs));
            return VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO;
        }

        /* Update the VCPU's copy of the guest MSR count. */
        pVCpu->hm.s.vmx.cGuestMsrs = cGuestMsrs;
        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT, cGuestMsrs);          AssertRCReturn(rc, rc);
        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT, cGuestMsrs);          AssertRCReturn(rc, rc);
#endif  /* VBOX_WITH_AUTO_MSR_LOAD_RESTORE */

        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_VMX_GUEST_AUTO_MSRS;
    }

    /*
     * Guest Sysenter MSRs.
     * These flags are only set when MSR-bitmaps are not supported by the CPU and we cause
     * VM-exits on WRMSRs for these MSRs.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_SYSENTER_CS_MSR)
    {
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_SYSENTER_CS,   pMixedCtx->SysEnter.cs);    AssertRCReturn(rc, rc);
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_SYSENTER_CS_MSR;
    }
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_SYSENTER_EIP_MSR)
    {
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_SYSENTER_EIP, pMixedCtx->SysEnter.eip);    AssertRCReturn(rc, rc);
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_SYSENTER_EIP_MSR;
    }
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_SYSENTER_ESP_MSR)
    {
        rc = VMXWriteVmcsGstN(VMX_VMCS_GUEST_SYSENTER_ESP, pMixedCtx->SysEnter.esp);    AssertRCReturn(rc, rc);
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_SYSENTER_ESP_MSR;
    }

    return rc;
}


/**
 * Loads the guest activity state into the guest-state area in the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxLoadGuestActivityState(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /** @todo See if we can make use of other states, e.g.
     *        VMX_VMCS_GUEST_ACTIVITY_SHUTDOWN or HLT.  */
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_VMX_GUEST_ACTIVITY_STATE)
    {
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_ACTIVITY_STATE, VMX_VMCS_GUEST_ACTIVITY_ACTIVE);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_VMX_GUEST_ACTIVITY_STATE;
    }
    return rc;
}


/**
 * Sets up the appropriate function to run guest code.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSetupVMRunHandler(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    if (CPUMIsGuestInLongModeEx(pMixedCtx))
    {
#ifndef VBOX_ENABLE_64_BITS_GUESTS
        return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif
        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests);    /* Guaranteed by hmR3InitFinalizeR0(). */
#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        /* 32-bit host. We need to switch to 64-bit before running the 64-bit guest. */
        pVCpu->hm.s.vmx.pfnStartVM = VMXR0SwitcherStartVM64;
#else
        /* 64-bit host or hybrid host. */
        pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM64;
#endif
    }
    else
    {
        /* Guest is not in long mode, use the 32-bit handler. */
        pVCpu->hm.s.vmx.pfnStartVM = VMXR0StartVM32;
    }
    Assert(pVCpu->hm.s.vmx.pfnStartVM);
    return VINF_SUCCESS;
}


/**
 * Wrapper for running the guest code in VT-x.
 *
 * @returns VBox strict status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxRunGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /*
     * 64-bit Windows uses XMM registers in the kernel as the Microsoft compiler expresses floating-point operations
     * using SSE instructions. Some XMM registers (XMM6-XMM15) are callee-saved and thus the need for this XMM wrapper.
     * Refer MSDN docs. "Configuring Programs for 64-bit / x64 Software Conventions / Register Usage" for details.
     */
#ifdef VBOX_WITH_KERNEL_USING_XMM
    return HMR0VMXStartVMWrapXMM(pVCpu->hm.s.fResumeVM, pCtx, &pVCpu->hm.s.vmx.VMCSCache, pVM, pVCpu, pVCpu->hm.s.vmx.pfnStartVM);
#else
    return pVCpu->hm.s.vmx.pfnStartVM(pVCpu->hm.s.fResumeVM, pCtx, &pVCpu->hm.s.vmx.VMCSCache, pVM, pVCpu);
#endif
}


/**
 * Reports world-switch error and dumps some useful debug info.
 *
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   rcVMRun         The return code from VMLAUNCH/VMRESUME.
 * @param   pCtx            Pointer to the guest-CPU context.
 * @param   pVmxTransient   Pointer to the VMX transient structure (only
 *                          exitReason updated).
 */
static void hmR0VmxReportWorldSwitchError(PVM pVM, PVMCPU pVCpu, int rcVMRun, PCPUMCTX pCtx, PVMXTRANSIENT pVmxTransient)
{
    Assert(pVM);
    Assert(pVCpu);
    Assert(pCtx);
    Assert(pVmxTransient);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    Log4(("VM-entry failure: %Rrc\n", rcVMRun));
    switch (rcVMRun)
    {
        case VERR_VMX_INVALID_VMXON_PTR:
            AssertFailed();
            break;
        case VINF_SUCCESS:                  /* VMLAUNCH/VMRESUME succeeded but VM-entry failed... yeah, true story. */
        case VERR_VMX_UNABLE_TO_START_VM:   /* VMLAUNCH/VMRESUME itself failed. */
        {
            int rc = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_REASON, &pVCpu->hm.s.vmx.lasterror.u32ExitReason);
            rc    |= VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &pVCpu->hm.s.vmx.lasterror.u32InstrError);
            rc    |= hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
            AssertRC(rc);

#ifdef VBOX_STRICT
                Log4(("uExitReason        %#RX32 (VmxTransient %#RX16)\n", pVCpu->hm.s.vmx.lasterror.u32ExitReason,
                     pVmxTransient->uExitReason));
                Log4(("Exit Qualification %#RX64\n", pVmxTransient->uExitQualification));
                Log4(("InstrError         %#RX32\n", pVCpu->hm.s.vmx.lasterror.u32InstrError));
                if (pVCpu->hm.s.vmx.lasterror.u32InstrError <= HMVMX_INSTR_ERROR_MAX)
                    Log4(("InstrError Desc.  \"%s\"\n", g_apszVmxInstrErrors[pVCpu->hm.s.vmx.lasterror.u32InstrError]));
                else
                    Log4(("InstrError Desc.    Range exceeded %u\n", HMVMX_INSTR_ERROR_MAX));

                /* VMX control bits. */
                uint32_t        u32Val;
                uint64_t        u64Val;
                HMVMXHCUINTREG  uHCReg;
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PIN_EXEC, &u32Val);                  AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PIN_EXEC                %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, &u32Val);                 AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PROC_EXEC               %#RX32\n", u32Val));
                rc = VMXReadVmcs32(VMX_VMCS32_CTRL_PROC_EXEC2, &u32Val);                AssertRC(rc);
                Log4(("VMX_VMCS32_CTRL_PROC_EXEC2              %#RX32\n", u32Val));
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
                rc = VMXReadVmcs64(VMX_VMCS64_CTRL_EPTP_FULL, &u64Val);                 AssertRC(rc);
                Log4(("VMX_VMCS64_CTRL_EPTP_FULL               %#RX64\n", u64Val));

                /* Guest bits. */
                rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RIP, &u64Val);          AssertRC(rc);
                Log4(("Old Guest Rip %#RX64 New %#RX64\n", pCtx->rip, u64Val));
                rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RSP, &u64Val);          AssertRC(rc);
                Log4(("Old Guest Rsp %#RX64 New %#RX64\n", pCtx->rsp, u64Val));
                rc = VMXReadVmcs32(VMX_VMCS_GUEST_RFLAGS, &u32Val);         AssertRC(rc);
                Log4(("Old Guest Rflags %#RX32 New %#RX32\n", pCtx->eflags.u32, u32Val));
                rc = VMXReadVmcs32(VMX_VMCS16_GUEST_FIELD_VPID, &u32Val);   AssertRC(rc);
                Log4(("VMX_VMCS16_GUEST_FIELD_VPID %u\n", u32Val));

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
                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_CS, &u32Val);      AssertRC(rc);
                Log4(("Host CS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "CS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_DS, &u32Val);      AssertRC(rc);
                Log4(("Host DS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "DS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_ES, &u32Val);      AssertRC(rc);
                Log4(("Host ES %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "ES: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_FS, &u32Val);      AssertRC(rc);
                Log4(("Host FS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "FS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_GS, &u32Val);      AssertRC(rc);
                Log4(("Host GS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "GS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_SS, &u32Val);      AssertRC(rc);
                Log4(("Host SS %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "SS: ");
                }

                rc = VMXReadVmcs32(VMX_VMCS16_HOST_FIELD_TR,  &u32Val);     AssertRC(rc);
                Log4(("Host TR %#08x\n", u32Val));
                if (u32Val < HostGdtr.cbGdt)
                {
                    pDesc  = (PCX86DESCHC)(HostGdtr.pGdt + (u32Val & X86_SEL_MASK));
                    HMR0DumpDescriptor(pDesc, u32Val, "TR: ");
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
# if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
                if (HMVMX_IS_64BIT_HOST_MODE())
                {
                    Log4(("MSR_K6_EFER            = %#RX64\n", ASMRdMsr(MSR_K6_EFER)));
                    Log4(("MSR_K6_STAR            = %#RX64\n", ASMRdMsr(MSR_K6_STAR)));
                    Log4(("MSR_K8_LSTAR           = %#RX64\n", ASMRdMsr(MSR_K8_LSTAR)));
                    Log4(("MSR_K8_CSTAR           = %#RX64\n", ASMRdMsr(MSR_K8_CSTAR)));
                    Log4(("MSR_K8_SF_MASK         = %#RX64\n", ASMRdMsr(MSR_K8_SF_MASK)));
                    Log4(("MSR_K8_KERNEL_GS_BASE  = %#RX64\n", ASMRdMsr(MSR_K8_KERNEL_GS_BASE)));
                }
# endif
#endif /* VBOX_STRICT */
            break;
        }

        default:
            /* Impossible */
            AssertMsgFailed(("hmR0VmxReportWorldSwitchError %Rrc (%#x)\n", rcVMRun, rcVMRun));
            break;
    }
    NOREF(pVM);
}


#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
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
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 * @param   enmOp       The operation to perform.
 * @param   cbParam     Number of parameters.
 * @param   paParam     Array of 32-bit parameters.
 */
VMMR0DECL(int) VMXR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, HM64ON32OP enmOp, uint32_t cbParam,
                                         uint32_t *paParam)
{
    int             rc, rc2;
    PHMGLOBLCPUINFO pCpu;
    RTHCPHYS        HCPhysCpuPage;
    RTCCUINTREG     uOldEFlags;

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
    uOldEFlags = ASMIntDisableFlags();

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    RTCPUID idHostCpu = RTMpCpuId();
    CPUMR0SetLApic(pVM, idHostCpu);
#endif

    pCpu = HMR0GetCurrentCpu();
    HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);

    /* Clear VMCS. Marking it inactive, clearing implementation-specific data and writing VMCS data back to memory. */
    VMXClearVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);

    /* Leave VMX Root Mode. */
    VMXDisable();

    ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVCpu));
    CPUMSetHyperEIP(pVCpu, enmOp);
    for (int i = (int)cbParam - 1; i >= 0; i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatWorldSwitch3264, z);

    /* Call the switcher. */
    rc = pVM->hm.s.pfnHost32ToGuest64R0(pVM, RT_OFFSETOF(VM, aCpus[pVCpu->idCpu].cpum) - RT_OFFSETOF(VM, cpum));
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatWorldSwitch3264, z);

    /** @todo replace with hmR0VmxEnterRootMode() and hmR0VmxLeaveRootMode(). */
    /* Make sure the VMX instructions don't cause #UD faults. */
    ASMSetCR4(ASMGetCR4() | X86_CR4_VMXE);

    /* Re-enter VMX Root Mode */
    rc2 = VMXEnable(HCPhysCpuPage);
    if (RT_FAILURE(rc2))
    {
        ASMSetCR4(ASMGetCR4() & ~X86_CR4_VMXE);
        ASMSetFlags(uOldEFlags);
        return rc2;
    }

    rc2 = VMXActivateVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);
    AssertRC(rc2);
    Assert(!(ASMGetFlags() & X86_EFL_IF));
    ASMSetFlags(uOldEFlags);
    return rc;
}


/**
 * Prepares for and executes VMLAUNCH (64 bits guests) for 32-bit hosts
 * supporting 64-bit guests.
 *
 * @returns VBox status code.
 * @param   fResume     Whether to VMLAUNCH or VMRESUME.
 * @param   pCtx        Pointer to the guest-CPU context.
 * @param   pCache      Pointer to the VMCS cache.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
DECLASM(int) VMXR0SwitcherStartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMCSCACHE pCache, PVM pVM, PVMCPU pVCpu)
{
    uint32_t        aParam[6];
    PHMGLOBLCPUINFO pCpu          = NULL;
    RTHCPHYS        HCPhysCpuPage = 0;
    int             rc            = VERR_INTERNAL_ERROR_5;

    pCpu = HMR0GetCurrentCpu();
    HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCache->uPos = 1;
    pCache->interPD = PGMGetInterPaeCR3(pVM);
    pCache->pSwitcher = (uint64_t)pVM->hm.s.pfnHost32ToGuest64R0;
#endif

#ifdef VBOX_STRICT
    pCache->TestIn.HCPhysCpuPage = 0;
    pCache->TestIn.HCPhysVmcs    = 0;
    pCache->TestIn.pCache        = 0;
    pCache->TestOut.HCPhysVmcs   = 0;
    pCache->TestOut.pCache       = 0;
    pCache->TestOut.pCtx         = 0;
    pCache->TestOut.eflags       = 0;
#endif

    aParam[0] = (uint32_t)(HCPhysCpuPage);                              /* Param 1: VMXON physical address - Lo. */
    aParam[1] = (uint32_t)(HCPhysCpuPage >> 32);                        /* Param 1: VMXON physical address - Hi. */
    aParam[2] = (uint32_t)(pVCpu->hm.s.vmx.HCPhysVmcs);                 /* Param 2: VMCS physical address - Lo. */
    aParam[3] = (uint32_t)(pVCpu->hm.s.vmx.HCPhysVmcs >> 32);           /* Param 2: VMCS physical address - Hi. */
    aParam[4] = VM_RC_ADDR(pVM, &pVM->aCpus[pVCpu->idCpu].hm.s.vmx.VMCSCache);
    aParam[5] = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    pCtx->dr[4] = pVM->hm.s.vmx.pScratchPhys + 16 + 8;
    *(uint32_t *)(pVM->hm.s.vmx.pScratch + 16 + 8) = 1;
#endif
    rc = VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_VMXRCStartVM64, 6, &aParam[0]);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    Assert(*(uint32_t *)(pVM->hm.s.vmx.pScratch + 16 + 8) == 5);
    Assert(pCtx->dr[4] == 10);
    *(uint32_t *)(pVM->hm.s.vmx.pScratch + 16 + 8) = 0xff;
#endif

#ifdef VBOX_STRICT
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
    return rc;
}


/**
 * Initialize the VMCS-Read cache. The VMCS cache is used for 32-bit hosts
 * running 64-bit guests (except 32-bit Darwin which runs with 64-bit paging in
 * 32-bit mode) for 64-bit fields that cannot be accessed in 32-bit mode. Some
 * 64-bit fields -can- be accessed (those that have a 32-bit FULL & HIGH part).
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
static int hmR0VmxInitVmcsReadCache(PVM pVM, PVMCPU pVCpu)
{
#define VMXLOCAL_INIT_READ_CACHE_FIELD(pCache, idxField)       \
{                                                              \
    Assert(pCache->Read.aField[idxField##_CACHE_IDX] == 0);    \
    pCache->Read.aField[idxField##_CACHE_IDX] = idxField;      \
    pCache->Read.aFieldVal[idxField##_CACHE_IDX] = 0;          \
    ++cReadFields;                                             \
}

    AssertPtr(pVM);
    AssertPtr(pVCpu);
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

    /* 64-bit guest-state fields; unused as we use two 32-bit VMREADs for these 64-bit fields (using "FULL" and "HIGH" fields). */
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

    if (pVM->hm.s.fNestedPaging)
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
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   idxField        The VMCS field encoding.
 * @param   u64Val          16, 32 or 64 bits value.
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
        case VMX_VMCS64_HOST_FIELD_PAT_FULL:
        case VMX_VMCS64_HOST_FIELD_EFER_FULL:
        case VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_FULL:
        {
            rc  = VMXWriteVmcs32(idxField, u64Val);
            rc |= VMXWriteVmcs32(idxField + 1, (uint32_t)(u64Val >> 32));
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
            if (!(u64Val >> 32))
            {
                /* If this field is 64-bit, VT-x will zero out the top bits. */
                rc = VMXWriteVmcs32(idxField, (uint32_t)u64Val);
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
 * Queue up a VMWRITE by using the VMCS write cache. This is only used on 32-bit
 * hosts (except darwin) for 64-bit guests.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idxField    The VMCS field encoding.
 * @param   u64Val      16, 32 or 64 bits value.
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

/* Enable later when the assembly code uses these as callbacks. */
#if 0
/*
 * Loads the VMCS write-cache into the CPU (by executing VMWRITEs).
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pCache          Pointer to the VMCS cache.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(void) VMXWriteCachedVmcsLoad(PVMCPU pVCpu, PVMCSCACHE pCache)
{
    AssertPtr(pCache);
    for (uint32_t i = 0; i < pCache->Write.cValidEntries; i++)
    {
        int rc = VMXWriteVmcs64(pCache->Write.aField[i], pCache->Write.aFieldVal[i]);
        AssertRC(rc);
    }
    pCache->Write.cValidEntries = 0;
}


/**
 * Stores the VMCS read-cache from the CPU (by executing VMREADs).
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pCache          Pointer to the VMCS cache.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(void) VMXReadCachedVmcsStore(PVMCPU pVCpu, PVMCSCACHE pCache)
{
    AssertPtr(pCache);
    for (uint32_t i = 0; i < pCache->Read.cValidEntries; i++)
    {
        int rc = VMXReadVmcs64(pCache->Read.aField[i], &pCache->Read.aFieldVal[i]);
        AssertRC(rc);
    }
}
#endif
#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */


/**
 * Sets up the usage of TSC-offsetting and updates the VMCS. If offsetting is
 * not possible, cause VM-exits on RDTSC(P)s. Also sets up the VMX preemption
 * timer.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxUpdateTscOffsettingAndPreemptTimer(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int  rc            = VERR_INTERNAL_ERROR_5;
    bool fOffsettedTsc = false;
    PVM pVM            = pVCpu->CTX_SUFF(pVM);
    if (pVM->hm.s.vmx.fUsePreemptTimer)
    {
        uint64_t cTicksToDeadline = TMCpuTickGetDeadlineAndTscOffset(pVCpu, &fOffsettedTsc, &pVCpu->hm.s.vmx.u64TSCOffset);

        /* Make sure the returned values have sane upper and lower boundaries. */
        uint64_t u64CpuHz  = SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage);
        cTicksToDeadline   = RT_MIN(cTicksToDeadline, u64CpuHz / 64);      /* 1/64th of a second */
        cTicksToDeadline   = RT_MAX(cTicksToDeadline, u64CpuHz / 2048);    /* 1/2048th of a second */
        cTicksToDeadline >>= pVM->hm.s.vmx.cPreemptTimerShift;

        uint32_t cPreemptionTickCount = (uint32_t)RT_MIN(cTicksToDeadline, UINT32_MAX - 16);
        rc = VMXWriteVmcs32(VMX_VMCS32_GUEST_PREEMPT_TIMER_VALUE, cPreemptionTickCount);          AssertRC(rc);
    }
    else
        fOffsettedTsc = TMCpuTickCanUseRealTSC(pVCpu, &pVCpu->hm.s.vmx.u64TSCOffset);

    if (fOffsettedTsc)
    {
        uint64_t u64CurTSC = ASMReadTSC();
        if (u64CurTSC + pVCpu->hm.s.vmx.u64TSCOffset >= TMCpuTickGetLastSeen(pVCpu))
        {
            /* Note: VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT takes precedence over TSC_OFFSET, applies to RDTSCP too. */
            rc = VMXWriteVmcs64(VMX_VMCS64_CTRL_TSC_OFFSET_FULL, pVCpu->hm.s.vmx.u64TSCOffset);   AssertRC(rc);

            pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT;
            rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);          AssertRC(rc);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTscOffset);
        }
        else
        {
            /* VM-exit on RDTSC(P) as we would otherwise pass decreasing TSC values to the guest. */
            pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT;
            rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);          AssertRC(rc);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTscInterceptOverFlow);
        }
    }
    else
    {
        /* We can't use TSC-offsetting (non-fixed TSC, warp drive active etc.), VM-exit on RDTSC(P). */
        pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT;
        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);               AssertRC(rc);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscIntercept);
    }
}


/**
 * Determines if an exception is a contributory exception. Contributory
 * exceptions are ones which can cause double-faults. Page-fault is
 * intentionally not included here as it's a conditional contributory exception.
 *
 * @returns true if the exception is contributory, false otherwise.
 * @param   uVector     The exception vector.
 */
DECLINLINE(bool) hmR0VmxIsContributoryXcpt(const uint32_t uVector)
{
    switch (uVector)
    {
        case X86_XCPT_GP:
        case X86_XCPT_SS:
        case X86_XCPT_NP:
        case X86_XCPT_TS:
        case X86_XCPT_DE:
            return true;
        default:
            break;
    }
    return false;
}


/**
 * Sets an event as a pending event to be injected into the guest.
 *
 * @param   pVCpu               Pointer to the VMCPU.
 * @param   u32IntrInfo         The VM-entry interruption-information field.
 * @param   cbInstr             The VM-entry instruction length in bytes (for software
 *                              interrupts, exceptions and privileged software
 *                              exceptions).
 * @param   u32ErrCode          The VM-entry exception error code.
 * @param   GCPtrFaultAddress   The fault-address (CR2) in case it's a
 *                              page-fault.
 */
DECLINLINE(void) hmR0VmxSetPendingEvent(PVMCPU pVCpu, uint32_t u32IntrInfo, uint32_t cbInstr, uint32_t u32ErrCode,
                                        RTGCUINTPTR GCPtrFaultAddress)
{
    Assert(!pVCpu->hm.s.Event.fPending);
    pVCpu->hm.s.Event.fPending          = true;
    pVCpu->hm.s.Event.u64IntrInfo       = u32IntrInfo;
    pVCpu->hm.s.Event.u32ErrCode        = u32ErrCode;
    pVCpu->hm.s.Event.cbInstr           = cbInstr;
    pVCpu->hm.s.Event.GCPtrFaultAddress = GCPtrFaultAddress;
}


/**
 * Sets a double-fault (#DF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptDF(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    uint32_t u32IntrInfo = X86_XCPT_DF | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    u32IntrInfo         |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
    hmR0VmxSetPendingEvent(pVCpu, u32IntrInfo,  0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Handle a condition that occurred while delivering an event through the guest
 * IDT.
 *
 * @returns VBox status code (informational error codes included).
 * @retval VINF_SUCCESS if we should continue handling the VM-exit.
 * @retval VINF_HM_DOUBLE_FAULT if a #DF condition was detected and we ought to
 *         continue execution of the guest which will delivery the #DF.
 * @retval VINF_EM_RESET if we detected a triple-fault condition.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxCheckExitDueToEventDelivery(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    int rc = hmR0VmxReadIdtVectoringInfoVmcs(pVmxTransient);
    AssertRC(rc);
    if (VMX_IDT_VECTORING_INFO_VALID(pVmxTransient->uIdtVectoringInfo))
    {
        rc = hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
        AssertRCReturn(rc, rc);

        uint32_t uIntType    = VMX_IDT_VECTORING_INFO_TYPE(pVmxTransient->uIdtVectoringInfo);
        uint32_t uExitVector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(pVmxTransient->uExitIntrInfo);
        uint32_t uIdtVector  = VMX_IDT_VECTORING_INFO_VECTOR(pVmxTransient->uIdtVectoringInfo);

        typedef enum
        {
            VMXREFLECTXCPT_XCPT,    /* Reflect the exception to the guest or for further evaluation by VMM. */
            VMXREFLECTXCPT_DF,      /* Reflect the exception as a double-fault to the guest. */
            VMXREFLECTXCPT_TF,      /* Indicate a triple faulted state to the VMM. */
            VMXREFLECTXCPT_NONE     /* Nothing to reflect. */
        } VMXREFLECTXCPT;

        /* See Intel spec. 30.7.1.1 "Reflecting Exceptions to Guest Software". */
        VMXREFLECTXCPT enmReflect = VMXREFLECTXCPT_NONE;
        if (VMX_EXIT_INTERRUPTION_INFO_IS_VALID(pVmxTransient->uExitIntrInfo))
        {
            if (uIntType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT)
            {
                enmReflect = VMXREFLECTXCPT_XCPT;
#ifdef VBOX_STRICT
                if (   hmR0VmxIsContributoryXcpt(uIdtVector)
                    && uExitVector == X86_XCPT_PF)
                {
                    Log4(("IDT: vcpu[%RU32] Contributory #PF uCR2=%#RX64\n", pVCpu->idCpu, pMixedCtx->cr2));
                }
#endif
                if (   uExitVector == X86_XCPT_PF
                    && uIdtVector == X86_XCPT_PF)
                {
                    pVmxTransient->fVectoringPF = true;
                    Log4(("IDT: vcpu[%RU32] Vectoring #PF uCR2=%#RX64\n", pVCpu->idCpu, pMixedCtx->cr2));
                }
                else if (   (pVCpu->hm.s.vmx.u32XcptBitmap & HMVMX_CONTRIBUTORY_XCPT_MASK)
                         && hmR0VmxIsContributoryXcpt(uExitVector)
                         && (   hmR0VmxIsContributoryXcpt(uIdtVector)
                             || uIdtVector == X86_XCPT_PF))
                {
                    enmReflect = VMXREFLECTXCPT_DF;
                }
                else if (uIdtVector == X86_XCPT_DF)
                    enmReflect = VMXREFLECTXCPT_TF;
            }
            else if (   uIntType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT
                     || uIntType == VMX_IDT_VECTORING_INFO_TYPE_EXT_INT
                     || uIntType == VMX_IDT_VECTORING_INFO_TYPE_NMI)
            {
                /*
                 * Ignore software interrupts (INT n), software exceptions (#BP, #OF) and privileged software exception
                 * (whatever they are) as they reoccur when restarting the instruction.
                 */
                enmReflect = VMXREFLECTXCPT_XCPT;
            }
        }
        else
        {
            /*
             * If event delivery caused an EPT violation/misconfig or APIC access VM-exit, then the VM-exit
             * interruption-information will not be valid and we end up here. In such cases, it is sufficient to reflect the
             * original exception to the guest after handling the VM-exit.
             */
            if (   uIntType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT
                || uIntType == VMX_IDT_VECTORING_INFO_TYPE_EXT_INT
                || uIntType == VMX_IDT_VECTORING_INFO_TYPE_NMI)
            {
                enmReflect = VMXREFLECTXCPT_XCPT;
            }
        }

        switch (enmReflect)
        {
            case VMXREFLECTXCPT_XCPT:
            {
                Assert(   uIntType != VMX_IDT_VECTORING_INFO_TYPE_SW_INT
                       && uIntType != VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT
                       && uIntType != VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT);

                uint32_t u32ErrCode = 0;
                if (VMX_IDT_VECTORING_INFO_ERROR_CODE_IS_VALID(pVCpu->hm.s.Event.u64IntrInfo))
                {
                    rc = hmR0VmxReadIdtVectoringErrorCodeVmcs(pVmxTransient);
                    AssertRCReturn(rc, rc);
                    u32ErrCode = pVmxTransient->uIdtVectoringErrorCode;
                }

                /* If uExitVector is #PF, CR2 value will be updated from the VMCS if it's a guest #PF. See hmR0VmxExitXcptPF(). */
                hmR0VmxSetPendingEvent(pVCpu, VMX_ENTRY_INTR_INFO_FROM_EXIT_IDT_INFO(pVmxTransient->uIdtVectoringInfo),
                                       0 /* cbInstr */,  u32ErrCode, pMixedCtx->cr2);
                rc = VINF_SUCCESS;
                Log4(("IDT: vcpu[%RU32] Pending vectoring event %#RX64 Err=%#RX32\n", pVCpu->idCpu,
                      pVCpu->hm.s.Event.u64IntrInfo, pVCpu->hm.s.Event.u32ErrCode));
                break;
            }

            case VMXREFLECTXCPT_DF:
            {
                hmR0VmxSetPendingXcptDF(pVCpu, pMixedCtx);
                rc = VINF_HM_DOUBLE_FAULT;
                Log4(("IDT: vcpu[%RU32] Pending vectoring #DF %#RX64 uIdtVector=%#x uExitVector=%#x\n", pVCpu->idCpu,
                      pVCpu->hm.s.Event.u64IntrInfo, uIdtVector, uExitVector));
                break;
            }

            case VMXREFLECTXCPT_TF:
            {
                rc = VINF_EM_RESET;
                Log4(("IDT: vcpu[%RU32] Pending vectoring triple-fault uIdt=%#x uExit=%#x\n", pVCpu->idCpu, uIdtVector,
                      uExitVector));
                break;
            }

            default:
                Assert(rc == VINF_SUCCESS);
                break;
        }
    }
    Assert(rc == VINF_SUCCESS || rc == VINF_HM_DOUBLE_FAULT || rc == VINF_EM_RESET);
    return rc;
}


/**
 * Saves the guest's CR0 register from the VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestCR0(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_CR0))
    {
        uint32_t uVal    = 0;
        int rc = VMXReadVmcs32(VMX_VMCS_GUEST_CR0,            &uVal);
        AssertRCReturn(rc, rc);
        uint32_t uShadow = 0;
        rc     = VMXReadVmcs32(VMX_VMCS_CTRL_CR0_READ_SHADOW, &uShadow);
        AssertRCReturn(rc, rc);

        uVal = (uShadow & pVCpu->hm.s.vmx.u32CR0Mask) | (uVal & ~pVCpu->hm.s.vmx.u32CR0Mask);
        CPUMSetGuestCR0(pVCpu, uVal);
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_CR0;
    }
    return VINF_SUCCESS;
}


/**
 * Saves the guest's CR4 register from the VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestCR4(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_CR4))
    {
        uint32_t uVal    = 0;
        uint32_t uShadow = 0;
        rc = VMXReadVmcs32(VMX_VMCS_GUEST_CR4,            &uVal);
        AssertRCReturn(rc, rc);
        rc = VMXReadVmcs32(VMX_VMCS_CTRL_CR4_READ_SHADOW, &uShadow);
        AssertRCReturn(rc, rc);

        uVal = (uShadow & pVCpu->hm.s.vmx.u32CR4Mask) | (uVal & ~pVCpu->hm.s.vmx.u32CR4Mask);
        CPUMSetGuestCR4(pVCpu, uVal);
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_CR4;
    }
    return rc;
}


/**
 * Saves the guest's RIP register from the VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestRip(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_RIP))
    {
        uint64_t u64Val = 0;
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RIP, &u64Val);
        AssertRCReturn(rc, rc);

        pMixedCtx->rip = u64Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_RIP;
    }
    return rc;
}


/**
 * Saves the guest's RSP register from the VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestRsp(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_RSP))
    {
        uint64_t u64Val = 0;
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_RSP, &u64Val);
        AssertRCReturn(rc, rc);

        pMixedCtx->rsp = u64Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_RSP;
    }
    return rc;
}


/**
 * Saves the guest's RFLAGS from the VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestRflags(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_RFLAGS))
    {
        uint32_t uVal = 0;
        int rc = VMXReadVmcs32(VMX_VMCS_GUEST_RFLAGS, &uVal);
        AssertRCReturn(rc, rc);

        pMixedCtx->eflags.u32 = uVal;
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)        /* Undo our real-on-v86-mode changes to eflags if necessary. */
        {
            Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.pRealModeTSS);
            Log4(("Saving real-mode EFLAGS VT-x view=%#RX32\n", pMixedCtx->eflags.u32));

            pMixedCtx->eflags.Bits.u1VM   = 0;
            pMixedCtx->eflags.Bits.u2IOPL = pVCpu->hm.s.vmx.RealMode.eflags.Bits.u2IOPL;
        }

        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_RFLAGS;
    }
    return VINF_SUCCESS;
}


/**
 * Wrapper for saving the guest's RIP, RSP and RFLAGS from the VMCS into the
 * guest-CPU context.
 */
DECLINLINE(int) hmR0VmxSaveGuestRipRspRflags(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = hmR0VmxSaveGuestRip(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestRsp(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
    return rc;
}


/**
 * Saves the guest's interruptibility-state ("interrupt shadow" as AMD calls it)
 * from the guest-state area in the VMCS.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0VmxSaveGuestIntrState(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    uint32_t uIntrState = 0;
    int rc = VMXReadVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &uIntrState);
    AssertRC(rc);

    if (!uIntrState)
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
    else
    {
        Assert(   uIntrState == VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI
               || uIntrState == VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS);
        rc  = hmR0VmxSaveGuestRip(pVCpu, pMixedCtx);
        AssertRC(rc);
        rc = hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);    /* for hmR0VmxGetGuestIntrState(). */
        AssertRC(rc);

        EMSetInhibitInterruptsPC(pVCpu, pMixedCtx->rip);
        Assert(VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
    }
}


/**
 * Saves the guest's activity state.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestActivityState(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    /* Nothing to do for now until we make use of different guest-CPU activity state. Just update the flag. */
    pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_ACTIVITY_STATE;
    return VINF_SUCCESS;
}


/**
 * Saves the guest SYSENTER MSRs (SYSENTER_CS, SYSENTER_EIP, SYSENTER_ESP) from
 * the current VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestSysenterMsrs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_SYSENTER_CS_MSR))
    {
        uint32_t u32Val = 0;
        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_SYSENTER_CS, &u32Val);     AssertRCReturn(rc, rc);
        pMixedCtx->SysEnter.cs = u32Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_SYSENTER_CS_MSR;
    }

    uint64_t u64Val = 0;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_SYSENTER_EIP_MSR))
    {
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_SYSENTER_EIP, &u64Val);    AssertRCReturn(rc, rc);
        pMixedCtx->SysEnter.eip = u64Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_SYSENTER_EIP_MSR;
    }
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_SYSENTER_ESP_MSR))
    {
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_SYSENTER_ESP, &u64Val);    AssertRCReturn(rc, rc);
        pMixedCtx->SysEnter.esp = u64Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_SYSENTER_ESP_MSR;
    }
    return rc;
}


/**
 * Saves the guest FS_BASE MSRs from the current VMCS into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestFSBaseMsr(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_FS_BASE_MSR))
    {
        uint64_t u64Val = 0;
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_FS_BASE, &u64Val);   AssertRCReturn(rc, rc);
        pMixedCtx->fs.u64Base = u64Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_FS_BASE_MSR;
    }
    return rc;
}


/**
 * Saves the guest GS_BASE MSRs from the current VMCS into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestGSBaseMsr(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_GS_BASE_MSR))
    {
        uint64_t u64Val = 0;
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_GS_BASE, &u64Val);   AssertRCReturn(rc, rc);
        pMixedCtx->gs.u64Base = u64Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_GS_BASE_MSR;
    }
    return rc;
}


/**
 * Saves the auto load/store'd guest MSRs from the current VMCS into the
 * guest-CPU context. Currently these are LSTAR, STAR, SFMASK, KERNEL-GS BASE
 * and TSC_AUX.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestAutoLoadStoreMsrs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    if (pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_AUTO_LOAD_STORE_MSRS)
        return VINF_SUCCESS;

#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
    for (uint32_t i = 0; i < pVCpu->hm.s.vmx.cGuestMsrs; i++)
    {
        PVMXMSR pMsr = (PVMXMSR)pVCpu->hm.s.vmx.pvGuestMsr;
        pMsr += i;
        switch (pMsr->u32IndexMSR)
        {
            case MSR_K8_LSTAR:          pMixedCtx->msrLSTAR  = pMsr->u64Value;                   break;
            case MSR_K6_STAR:           pMixedCtx->msrSTAR   = pMsr->u64Value;                   break;
            case MSR_K8_SF_MASK:        pMixedCtx->msrSFMASK = pMsr->u64Value;                   break;
            case MSR_K8_TSC_AUX:        CPUMSetGuestMsr(pVCpu, MSR_K8_TSC_AUX, pMsr->u64Value);  break;
            case MSR_K8_KERNEL_GS_BASE: pMixedCtx->msrKERNELGSBASE = pMsr->u64Value;             break;
            case MSR_K6_EFER:          /* EFER can't be changed without causing a VM-exit. */    break;
            default:
            {
                AssertFailed();
                return VERR_HM_UNEXPECTED_LD_ST_MSR;
            }
        }
    }
#endif

    pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_AUTO_LOAD_STORE_MSRS;
    return VINF_SUCCESS;
}


/**
 * Saves the guest control registers from the current VMCS into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestControlRegs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    /* Guest CR0. Guest FPU. */
    int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    /* Guest CR4. */
    rc = hmR0VmxSaveGuestCR4(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    /* Guest CR2 - updated always during the world-switch or in #PF. */
    /* Guest CR3. Only changes with Nested Paging. This must be done -after- saving CR0 and CR4 from the guest! */
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_CR3))
    {
        Assert(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_CR0);
        Assert(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_CR4);

        PVM pVM = pVCpu->CTX_SUFF(pVM);
        if (   pVM->hm.s.vmx.fUnrestrictedGuest
            || (   pVM->hm.s.fNestedPaging
                && CPUMIsGuestPagingEnabledEx(pMixedCtx)))
        {
            uint64_t u64Val = 0;
            rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_CR3, &u64Val);
            if (pMixedCtx->cr3 != u64Val)
            {
                CPUMSetGuestCR3(pVCpu, u64Val);
                if (VMMRZCallRing3IsEnabled(pVCpu))
                {
                    PGMUpdateCR3(pVCpu, u64Val);
                    Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
                }
                else
                {
                    /* Set the force flag to inform PGM about it when necessary. It is cleared by PGMUpdateCR3().*/
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3);
                }
            }

            /* If the guest is in PAE mode, sync back the PDPE's into the guest state. */
            if (CPUMIsGuestInPAEModeEx(pMixedCtx))  /* Reads CR0, CR4 and EFER MSR (EFER is always up-to-date). */
            {
                rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE0_FULL, &pVCpu->hm.s.aPdpes[0].u);        AssertRCReturn(rc, rc);
                rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE1_FULL, &pVCpu->hm.s.aPdpes[1].u);        AssertRCReturn(rc, rc);
                rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE2_FULL, &pVCpu->hm.s.aPdpes[2].u);        AssertRCReturn(rc, rc);
                rc = VMXReadVmcs64(VMX_VMCS64_GUEST_PDPTE3_FULL, &pVCpu->hm.s.aPdpes[3].u);        AssertRCReturn(rc, rc);

                if (VMMRZCallRing3IsEnabled(pVCpu))
                {
                    PGMGstUpdatePaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);
                    Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));
                }
                else
                {
                    /* Set the force flag to inform PGM about it when necessary. It is cleared by PGMGstUpdatePaePdpes(). */
                    VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES);
                }
            }
        }

        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_CR3;
    }

    /*
     * Consider this scenario: VM-exit -> VMMRZCallRing3Enable() -> do stuff that causes a longjmp -> hmR0VmxCallRing3Callback()
     * -> VMMRZCallRing3Disable() -> hmR0VmxSaveGuestState() -> Set VMCPU_FF_HM_UPDATE_CR3 pending -> return from the longjmp
     * -> continue with VM-exit handling -> hmR0VmxSaveGuestControlRegs() and here we are.
     *
     * The longjmp exit path can't check these CR3 force-flags and call code that takes a lock again. We cover for it here.
     */
    if (VMMRZCallRing3IsEnabled(pVCpu))
    {
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
            PGMUpdateCR3(pVCpu, CPUMGetGuestCR3(pVCpu));

        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES))
            PGMGstUpdatePaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);

        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));
    }

    return rc;
}


/**
 * Reads a guest segment register from the current VMCS into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idxSel      Index of the selector in the VMCS.
 * @param   idxLimit    Index of the segment limit in the VMCS.
 * @param   idxBase     Index of the segment base in the VMCS.
 * @param   idxAccess   Index of the access rights of the segment in the VMCS.
 * @param   pSelReg     Pointer to the segment selector.
 *
 * @remarks No-long-jump zone!!!
 * @remarks Never call this function directly!!! Use the VMXLOCAL_READ_SEG()
 *          macro as that takes care of whether to read from the VMCS cache or
 *          not.
 */
DECLINLINE(int) hmR0VmxReadSegmentReg(PVMCPU pVCpu, uint32_t idxSel, uint32_t idxLimit, uint32_t idxBase, uint32_t idxAccess,
                                      PCPUMSELREG pSelReg)
{
    uint32_t u32Val = 0;
    int rc = VMXReadVmcs32(idxSel, &u32Val);
    AssertRCReturn(rc, rc);
    pSelReg->Sel      = (uint16_t)u32Val;
    pSelReg->ValidSel = (uint16_t)u32Val;
    pSelReg->fFlags   = CPUMSELREG_FLAGS_VALID;

    rc = VMXReadVmcs32(idxLimit, &u32Val);
    AssertRCReturn(rc, rc);
    pSelReg->u32Limit = u32Val;

    uint64_t u64Val = 0;
    rc = VMXReadVmcsGstNByIdxVal(idxBase, &u64Val);
    AssertRCReturn(rc, rc);
    pSelReg->u64Base = u64Val;

    rc = VMXReadVmcs32(idxAccess, &u32Val);
    AssertRCReturn(rc, rc);
    pSelReg->Attr.u = u32Val;

    /*
     * If VT-x marks the segment as unusable, the rest of the attributes are undefined with certain exceptions (some bits in
     * CS, SS). Regardless, we have to clear the bits here and only retain the unusable bit because the unusable bit is specific
     * to VT-x, everyone else relies on the attribute being zero and have no clue what the unusable bit is.
     *
     * See Intel spec. 27.3.2 "Saving Segment Registers and Descriptor-Table Registers".
     */
    if (pSelReg->Attr.u & HMVMX_SEL_UNUSABLE)
    {
        Assert(idxSel != VMX_VMCS16_GUEST_FIELD_TR);          /* TR is the only selector that can never be unusable. */
        pSelReg->Attr.u = HMVMX_SEL_UNUSABLE;
    }
    return VINF_SUCCESS;
}


#ifdef VMX_USE_CACHED_VMCS_ACCESSES
#define VMXLOCAL_READ_SEG(Sel, CtxSel) \
    hmR0VmxReadSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_##Sel, VMX_VMCS32_GUEST_##Sel##_LIMIT, \
                          VMX_VMCS_GUEST_##Sel##_BASE_CACHE_IDX, VMX_VMCS32_GUEST_##Sel##_ACCESS_RIGHTS, &pMixedCtx->CtxSel)
#else
#define VMXLOCAL_READ_SEG(Sel, CtxSel) \
    hmR0VmxReadSegmentReg(pVCpu, VMX_VMCS16_GUEST_FIELD_##Sel, VMX_VMCS32_GUEST_##Sel##_LIMIT, \
                          VMX_VMCS_GUEST_##Sel##_BASE, VMX_VMCS32_GUEST_##Sel##_ACCESS_RIGHTS, &pMixedCtx->CtxSel)
#endif


/**
 * Saves the guest segment registers from the current VMCS into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestSegmentRegs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    /* Guest segment registers. */
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_SEGMENT_REGS))
    {
        int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);   AssertRCReturn(rc, rc);
        rc = VMXLOCAL_READ_SEG(CS, cs);                   AssertRCReturn(rc, rc);
        rc = VMXLOCAL_READ_SEG(SS, ss);                   AssertRCReturn(rc, rc);
        rc = VMXLOCAL_READ_SEG(DS, ds);                   AssertRCReturn(rc, rc);
        rc = VMXLOCAL_READ_SEG(ES, es);                   AssertRCReturn(rc, rc);
        rc = VMXLOCAL_READ_SEG(FS, fs);                   AssertRCReturn(rc, rc);
        rc = VMXLOCAL_READ_SEG(GS, gs);                   AssertRCReturn(rc, rc);

        /* Restore segment attributes for real-on-v86 mode hack. */
        if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            pMixedCtx->cs.Attr.u = pVCpu->hm.s.vmx.RealMode.uAttrCS.u;
            pMixedCtx->ss.Attr.u = pVCpu->hm.s.vmx.RealMode.uAttrSS.u;
            pMixedCtx->ds.Attr.u = pVCpu->hm.s.vmx.RealMode.uAttrDS.u;
            pMixedCtx->es.Attr.u = pVCpu->hm.s.vmx.RealMode.uAttrES.u;
            pMixedCtx->fs.Attr.u = pVCpu->hm.s.vmx.RealMode.uAttrFS.u;
            pMixedCtx->gs.Attr.u = pVCpu->hm.s.vmx.RealMode.uAttrGS.u;
        }
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_SEGMENT_REGS;
    }

    return VINF_SUCCESS;
}


/**
 * Saves the guest descriptor table registers and task register from the current
 * VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestTableRegs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;

    /* Guest LDTR. */
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_LDTR))
    {
        rc = VMXLOCAL_READ_SEG(LDTR, ldtr);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_LDTR;
    }

    /* Guest GDTR. */
    uint64_t u64Val = 0;
    uint32_t u32Val = 0;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_GDTR))
    {
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_GDTR_BASE, &u64Val);        AssertRCReturn(rc, rc);
        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_GDTR_LIMIT, &u32Val);       AssertRCReturn(rc, rc);
        pMixedCtx->gdtr.pGdt  = u64Val;
        pMixedCtx->gdtr.cbGdt = u32Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_GDTR;
    }

    /* Guest IDTR. */
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_IDTR))
    {
        rc = VMXReadVmcsGstN(VMX_VMCS_GUEST_IDTR_BASE, &u64Val);        AssertRCReturn(rc, rc);
        rc = VMXReadVmcs32(VMX_VMCS32_GUEST_IDTR_LIMIT, &u32Val);       AssertRCReturn(rc, rc);
        pMixedCtx->idtr.pIdt  = u64Val;
        pMixedCtx->idtr.cbIdt = u32Val;
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_IDTR;
    }

    /* Guest TR. */
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_TR))
    {
        rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
        AssertRCReturn(rc, rc);

        /* For real-mode emulation using virtual-8086 mode we have the fake TSS (pRealModeTSS) in TR, don't save the fake one. */
        if (!pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
        {
            rc = VMXLOCAL_READ_SEG(TR, tr);
            AssertRCReturn(rc, rc);
        }
        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_TR;
    }
    return rc;
}

#undef VMXLOCAL_READ_SEG


/**
 * Saves the guest debug registers from the current VMCS into the guest-CPU
 * context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestDebugRegs(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    int rc = VINF_SUCCESS;
    if (!(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_DEBUG))
    {
        /* Upper 32-bits are always zero. See Intel spec. 2.7.3 "Loading and Storing Debug Registers". */
        uint32_t u32Val;
        rc = VMXReadVmcs32(VMX_VMCS_GUEST_DR7, &u32Val);        AssertRCReturn(rc, rc);
        pMixedCtx->dr[7] = u32Val;

        pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_DEBUG;
    }
    return rc;
}


/**
 * Saves the guest APIC state from the currentl VMCS into the guest-CPU context.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data maybe
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0VmxSaveGuestApicState(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    /* Updating TPR is already done in hmR0VmxPostRunGuest(). Just update the flag. */
    pVCpu->hm.s.vmx.fUpdatedGuestState |= HMVMX_UPDATED_GUEST_APIC_STATE;
    return VINF_SUCCESS;
}


/**
 * Saves the entire guest state from the currently active VMCS into the
 * guest-CPU context. This essentially VMREADs all guest-data.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 */
static int hmR0VmxSaveGuestState(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    Assert(pVCpu);
    Assert(pMixedCtx);

    if (pVCpu->hm.s.vmx.fUpdatedGuestState == HMVMX_UPDATED_GUEST_ALL)
        return VINF_SUCCESS;

    /* Though we can longjmp to ring-3 due to log-flushes here and get recalled again on the ring-3 callback path,
       there is no real need to. */
    if (VMMRZCallRing3IsEnabled(pVCpu))
        VMMR0LogFlushDisable(pVCpu);
    else
        Assert(VMMR0IsLogFlushDisabled(pVCpu));
    Log4Func(("vcpu[%RU32]\n", pVCpu->idCpu));

    int rc = hmR0VmxSaveGuestRipRspRflags(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestRipRspRflags failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestControlRegs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestSegmentRegs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestTableRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestTableRegs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestDebugRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestDebugRegs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestSysenterMsrs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestSysenterMsrs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestFSBaseMsr(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestFSBaseMsr failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestGSBaseMsr(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestGSBaseMsr failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestAutoLoadStoreMsrs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestAutoLoadStoreMsrs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestActivityState(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestActivityState failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    rc = hmR0VmxSaveGuestApicState(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveGuestDebugRegs failed! rc=%Rrc (pVCpu=%p)\n", rc, pVCpu), rc);

    AssertMsg(pVCpu->hm.s.vmx.fUpdatedGuestState == HMVMX_UPDATED_GUEST_ALL,
              ("Missed guest state bits while saving state; residue %RX32\n", pVCpu->hm.s.vmx.fUpdatedGuestState));

    if (VMMRZCallRing3IsEnabled(pVCpu))
        VMMR0LogFlushEnable(pVCpu);

    return rc;
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
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 */
static int hmR0VmxCheckForceFlags(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));

    int rc = VERR_INTERNAL_ERROR_5;
    if (   VM_FF_IS_PENDING(pVM, VM_FF_HM_TO_R3_MASK | VM_FF_REQUEST | VM_FF_PGM_POOL_FLUSH_PENDING | VM_FF_PDM_DMA)
        || VMCPU_FF_IS_PENDING(pVCpu,  VMCPU_FF_HM_TO_R3_MASK | VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
                                     | VMCPU_FF_REQUEST | VMCPU_FF_HM_UPDATE_CR3 | VMCPU_FF_HM_UPDATE_PAE_PDPES))
    {
        /* We need the control registers now, make sure the guest-CPU context is updated. */
        rc = hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
        AssertRCReturn(rc, rc);

        /* Pending HM CR3 sync. */
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3))
        {
            rc = PGMUpdateCR3(pVCpu, pMixedCtx->cr3);
            Assert(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3);
            Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_CR3));
        }

        /* Pending HM PAE PDPEs. */
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES))
        {
            rc = PGMGstUpdatePaePdpes(pVCpu, &pVCpu->hm.s.aPdpes[0]);
            AssertRC(rc);
            Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_UPDATE_PAE_PDPES));
        }

        /* Pending PGM C3 sync. */
        if (VMCPU_FF_IS_PENDING(pVCpu,VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
        {
            rc = PGMSyncCR3(pVCpu, pMixedCtx->cr0, pMixedCtx->cr3, pMixedCtx->cr4, VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
            if (rc != VINF_SUCCESS)
            {
                AssertRC(rc);
                Log4(("hmR0VmxCheckForceFlags: PGMSyncCR3 forcing us back to ring-3. rc=%d\n", rc));
                return rc;
            }
        }

        /* Pending HM-to-R3 operations (critsects, timers, EMT rendezvous etc.) */
        /* -XXX- what was that about single stepping?  */
        if (   VM_FF_IS_PENDING(pVM, VM_FF_HM_TO_R3_MASK)
            || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchHmToR3FF);
            rc = RT_UNLIKELY(VM_FF_IS_PENDING(pVM, VM_FF_PGM_NO_MEMORY)) ? VINF_EM_NO_MEMORY : VINF_EM_RAW_TO_R3;
            Log4(("hmR0VmxCheckForceFlags: HM_TO_R3 forcing us back to ring-3. rc=%d\n", rc));
            return rc;
        }

        /* Pending VM request packets, such as hardware interrupts. */
        if (   VM_FF_IS_PENDING(pVM, VM_FF_REQUEST)
            || VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_REQUEST))
        {
            Log4(("hmR0VmxCheckForceFlags: Pending VM request forcing us back to ring-3\n"));
            return VINF_EM_PENDING_REQUEST;
        }

        /* Pending PGM pool flushes. */
        if (VM_FF_IS_PENDING(pVM, VM_FF_PGM_POOL_FLUSH_PENDING))
        {
            Log4(("hmR0VmxCheckForceFlags: PGM pool flush pending forcing us back to ring-3\n"));
            return VINF_PGM_POOL_FLUSH_PENDING;
        }

        /* Pending DMA requests. */
        if (VM_FF_IS_PENDING(pVM, VM_FF_PDM_DMA))
        {
            Log4(("hmR0VmxCheckForceFlags: Pending DMA request forcing us back to ring-3\n"));
            return VINF_EM_RAW_TO_R3;
        }
    }

    /* Paranoia. */
    Assert(rc != VERR_EM_INTERPRETER);
    return VINF_SUCCESS;
}


/**
 * Converts any TRPM trap into a pending HM event. This is typically used when
 * entering from ring-3 (not longjmp returns).
 *
 * @param   pVCpu           Pointer to the VMCPU.
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

    /* Refer Intel spec. 24.8.3 "VM-entry Controls for Event Injection" for the format of u32IntrInfo. */
    uint32_t u32IntrInfo = uVector | VMX_EXIT_INTERRUPTION_INFO_VALID;
    if (enmTrpmEvent == TRPM_TRAP)
    {
        switch (uVector)
        {
            case X86_XCPT_BP:
            case X86_XCPT_OF:
            {
                u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
                break;
            }

            case X86_XCPT_PF:
            case X86_XCPT_DF:
            case X86_XCPT_TS:
            case X86_XCPT_NP:
            case X86_XCPT_SS:
            case X86_XCPT_GP:
            case X86_XCPT_AC:
                u32IntrInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
                /* no break! */
            default:
            {
                u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
                break;
            }
        }
    }
    else if (enmTrpmEvent == TRPM_HARDWARE_INT)
    {
        if (uVector == X86_XCPT_NMI)
            u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
        else
            u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    }
    else if (enmTrpmEvent == TRPM_SOFTWARE_INT)
        u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    else
        AssertMsgFailed(("Invalid TRPM event type %d\n", enmTrpmEvent));

    rc = TRPMResetTrap(pVCpu);
    AssertRC(rc);
    Log4(("TRPM->HM event: u32IntrInfo=%#RX32 enmTrpmEvent=%d cbInstr=%u uErrCode=%#RX32 GCPtrFaultAddress=%#RGv\n",
         u32IntrInfo, enmTrpmEvent, cbInstr, uErrCode, GCPtrFaultAddress));
    hmR0VmxSetPendingEvent(pVCpu, u32IntrInfo, cbInstr, uErrCode, GCPtrFaultAddress);
}


/**
 * Converts any pending HM event into a TRPM trap. Typically used when leaving
 * VT-x to execute any instruction.
 *
 * @param   pvCpu           Pointer to the VMCPU.
 */
static void hmR0VmxPendingEventToTrpmTrap(PVMCPU pVCpu)
{
    Assert(pVCpu->hm.s.Event.fPending);

    uint32_t uVectorType     = VMX_IDT_VECTORING_INFO_TYPE(pVCpu->hm.s.Event.u64IntrInfo);
    uint32_t uVector         = VMX_IDT_VECTORING_INFO_VECTOR(pVCpu->hm.s.Event.u64IntrInfo);
    bool     fErrorCodeValid = !!VMX_IDT_VECTORING_INFO_ERROR_CODE_IS_VALID(pVCpu->hm.s.Event.u64IntrInfo);
    uint32_t uErrorCode      = pVCpu->hm.s.Event.u32ErrCode;

    /* If a trap was already pending, we did something wrong! */
    Assert(TRPMQueryTrap(pVCpu, NULL /* pu8TrapNo */, NULL /* pEnmType */) == VERR_TRPM_NO_ACTIVE_TRAP);

    TRPMEVENT enmTrapType;
    switch (uVectorType)
    {
        case VMX_IDT_VECTORING_INFO_TYPE_EXT_INT:
        case VMX_IDT_VECTORING_INFO_TYPE_NMI:
           enmTrapType = TRPM_HARDWARE_INT;
           break;
        case VMX_IDT_VECTORING_INFO_TYPE_SW_INT:
            enmTrapType = TRPM_SOFTWARE_INT;
            break;
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
    pVCpu->hm.s.Event.fPending = false;
}


/**
 * Does the necessary state syncing before doing a longjmp to ring-3.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 * @param   rcExit      The reason for exiting to ring-3. Can be
 *                      VINF_VMM_UNKNOWN_RING3_CALL.
 *
 * @remarks No-long-jmp zone!!!
 */
static void hmR0VmxLongJmpToRing3(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx, int rcExit)
{
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(VMMR0IsLogFlushDisabled(pVCpu));

    int rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
    Assert(pVCpu->hm.s.vmx.fUpdatedGuestState == HMVMX_UPDATED_GUEST_ALL);
    AssertRC(rc);

    /* Restore host FPU state if necessary and resync on next R0 reentry .*/
    if (CPUMIsGuestFPUStateActive(pVCpu))
    {
        CPUMR0SaveGuestFPU(pVM, pVCpu, pMixedCtx);
        Assert(!CPUMIsGuestFPUStateActive(pVCpu));
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR0;
    }

    /* Restore host debug registers if necessary and resync on next R0 reentry. */
    if (CPUMIsGuestDebugStateActive(pVCpu))
    {
        CPUMR0SaveGuestDebugState(pVM, pVCpu, pMixedCtx, true /* save DR6 */);
        Assert(!CPUMIsGuestDebugStateActive(pVCpu));
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_DEBUG;
    }
    else if (CPUMIsHyperDebugStateActive(pVCpu))
    {
        CPUMR0LoadHostDebugState(pVM, pVCpu);
        Assert(!CPUMIsHyperDebugStateActive(pVCpu));
        Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT);
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatLoadGuestState, x);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExit1, x);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExit2, x);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitIO, y1);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitMovCRx, y2);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchLongJmpToR3);
    VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_HM, VMCPUSTATE_STARTED_EXEC);
}


/**
 * An action requires us to go back to ring-3. This function does the necessary
 * steps before we can safely return to ring-3. This is not the same as longjmps
 * to ring-3, this is voluntary.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 * @param   rcExit      The reason for exiting to ring-3. Can be
 *                      VINF_VMM_UNKNOWN_RING3_CALL.
 */
static void hmR0VmxExitToRing3(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx, int rcExit)
{
    Assert(pVM);
    Assert(pVCpu);
    Assert(pMixedCtx);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (RT_UNLIKELY(rcExit == VERR_VMX_INVALID_GUEST_STATE))
    {
        /* We want to see what the guest-state was before VM-entry, don't resync here, as we won't continue guest execution. */
        return;
    }
    else if (RT_UNLIKELY(rcExit == VERR_VMX_INVALID_VMCS_PTR))
    {
        VMXGetActivateVMCS(&pVCpu->hm.s.vmx.lasterror.u64VMCSPhys);
        pVCpu->hm.s.vmx.lasterror.u32VMCSRevision = *(uint32_t *)pVCpu->hm.s.vmx.pvVmcs;
        pVCpu->hm.s.vmx.lasterror.idEnteredCpu    = pVCpu->hm.s.idEnteredCpu;
        pVCpu->hm.s.vmx.lasterror.idCurrentCpu    = RTMpCpuId();
        return;
    }

    /* Please, no longjumps here (any logging shouldn't flush jump back to ring-3). NO LOGGING BEFORE THIS POINT! */
    VMMRZCallRing3Disable(pVCpu);
    Log4(("hmR0VmxExitToRing3: pVCpu=%p idCpu=%RU32 rcExit=%d\n", pVCpu, pVCpu->idCpu, rcExit));

    /* We need to do this only while truly exiting the "inner loop" back to ring-3 and -not- for any longjmp to ring3. */
    if (pVCpu->hm.s.Event.fPending)
    {
        hmR0VmxPendingEventToTrpmTrap(pVCpu);
        Assert(!pVCpu->hm.s.Event.fPending);
    }

    /* Sync. the guest state. */
    hmR0VmxLongJmpToRing3(pVM, pVCpu, pMixedCtx, rcExit);
    STAM_COUNTER_DEC(&pVCpu->hm.s.StatSwitchLongJmpToR3);

    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);
    CPUMSetChangedFlags(pVCpu,  CPUM_CHANGED_SYSENTER_MSR
                              | CPUM_CHANGED_LDTR
                              | CPUM_CHANGED_GDTR
                              | CPUM_CHANGED_IDTR
                              | CPUM_CHANGED_TR
                              | CPUM_CHANGED_HIDDEN_SEL_REGS);

    /* On our way back from ring-3 the following needs to be done. */
    /** @todo This can change with preemption hooks. */
    if (rcExit == VINF_EM_RAW_INTERRUPT)
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_HOST_CONTEXT;
    else
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_HOST_CONTEXT | HM_CHANGED_ALL_GUEST;

    STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchExitToR3);
    VMMRZCallRing3Enable(pVCpu);
}


/**
 * VMMRZCallRing3() callback wrapper which saves the guest state before we
 * longjump to ring-3 and possibly get preempted.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   enmOperation    The operation causing the ring-3 longjump.
 * @param   pvUser          The user argument (pointer to the possibly
 *                          out-of-date guest-CPU context).
 *
 * @remarks Must never be called with @a enmOperation ==
 *          VMMCALLRING3_VM_R0_ASSERTION.
 */
DECLCALLBACK(void) hmR0VmxCallRing3Callback(PVMCPU pVCpu, VMMCALLRING3 enmOperation, void *pvUser)
{
    /* VMMRZCallRing3() already makes sure we never get called as a result of an longjmp due to an assertion. */
    Assert(pVCpu);
    Assert(pvUser);
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    VMMRZCallRing3Disable(pVCpu);
    Assert(VMMR0IsLogFlushDisabled(pVCpu));
    Log4(("hmR0VmxCallRing3Callback->hmR0VmxLongJmpToRing3 pVCpu=%p idCpu=%RU32\n", pVCpu, pVCpu->idCpu));
    hmR0VmxLongJmpToRing3(pVCpu->CTX_SUFF(pVM), pVCpu, (PCPUMCTX)pvUser, VINF_VMM_UNKNOWN_RING3_CALL);
    VMMRZCallRing3Enable(pVCpu);
}


/**
 * Sets the interrupt-window exiting control in the VMCS which instructs VT-x to
 * cause a VM-exit as soon as the guest is in a state to receive interrupts.
 *
 * @param pVCpu         Pointer to the VMCPU.
 */
DECLINLINE(void) hmR0VmxSetIntWindowExitVmcs(PVMCPU pVCpu)
{
    if (RT_LIKELY(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT))
    {
        if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT))
        {
            pVCpu->hm.s.vmx.u32ProcCtls |= VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT;
            int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
            AssertRC(rc);
        }
    } /* else we will deliver interrupts whenever the guest exits next and is in a state to receive events. */
}


/**
 * Injects any pending events into the guest if the guest is in a state to
 * receive them.
 *
 * @returns VBox status code (informational status codes included).
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 */
static int hmR0VmxInjectPendingEvent(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    /* Get the current interruptibility-state of the guest and then figure out what can be injected. */
    uint32_t uIntrState = hmR0VmxGetGuestIntrState(pVCpu, pMixedCtx);
    bool fBlockMovSS    = !!(uIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS);
    bool fBlockSti      = !!(uIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);

    Assert(!fBlockSti || (pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_RFLAGS));
    Assert(   !(uIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI)      /* We don't support block-by-NMI and SMI yet.*/
           && !(uIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI));
    Assert(!fBlockSti || pMixedCtx->eflags.Bits.u1IF);       /* Cannot set block-by-STI when interrupts are disabled. */
    Assert(!TRPMHasTrap(pVCpu));

    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.Event.fPending)     /* First, inject any pending HM events. */
    {
        uint32_t uIntrType = VMX_EXIT_INTERRUPTION_INFO_TYPE(pVCpu->hm.s.Event.u64IntrInfo);
        bool fInject = true;
        if (uIntrType == VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT)
        {
            rc = hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
            AssertRCReturn(rc, rc);
            const bool fBlockInt = !(pMixedCtx->eflags.u32 & X86_EFL_IF);
            if (   fBlockInt
                || fBlockSti
                || fBlockMovSS)
            {
                fInject = false;
            }
        }
        else if (   uIntrType == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI
                 && (   fBlockMovSS
                     || fBlockSti))
        {
            /* On some CPUs block-by-STI also blocks NMIs. See Intel spec. 26.3.1.5 "Checks On Guest Non-Register State". */
            fInject = false;
        }

        if (fInject)
        {
            Log4(("Injecting pending event vcpu[%RU32]\n", pVCpu->idCpu));
            rc = hmR0VmxInjectEventVmcs(pVCpu, pMixedCtx, pVCpu->hm.s.Event.u64IntrInfo, pVCpu->hm.s.Event.cbInstr,
                                        pVCpu->hm.s.Event.u32ErrCode, pVCpu->hm.s.Event.GCPtrFaultAddress, &uIntrState);
            AssertRCReturn(rc, rc);
            pVCpu->hm.s.Event.fPending = false;
            STAM_COUNTER_INC(&pVCpu->hm.s.StatIntReinject);
        }
        else
            hmR0VmxSetIntWindowExitVmcs(pVCpu);
    }                                                           /** @todo SMI. SMIs take priority over NMIs. */
    else if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_NMI))    /* NMI. NMIs take priority over regular interrupts . */
    {
        /* On some CPUs block-by-STI also blocks NMIs. See Intel spec. 26.3.1.5 "Checks On Guest Non-Register State". */
        if (   !fBlockMovSS
            && !fBlockSti)
        {
            Log4(("Injecting NMI\n"));
            uint32_t u32IntrInfo = X86_XCPT_NMI | VMX_EXIT_INTERRUPTION_INFO_VALID;
            u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
            rc = hmR0VmxInjectEventVmcs(pVCpu, pMixedCtx, u32IntrInfo, 0 /* cbInstr */, 0 /* u32ErrCode */,
                                        0 /* GCPtrFaultAddress */, &uIntrState);
            AssertRCReturn(rc, rc);
            VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
        }
        else
            hmR0VmxSetIntWindowExitVmcs(pVCpu);
    }
    else if (VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC)))
    {
        /* Check if there are guest external interrupts (PIC/APIC) pending and inject them if the guest can receive them. */
        rc = hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
        AssertRCReturn(rc, rc);
        const bool fBlockInt = !(pMixedCtx->eflags.u32 & X86_EFL_IF);
        if (   !fBlockInt
            && !fBlockSti
            && !fBlockMovSS)
        {
            uint8_t u8Interrupt;
            rc = PDMGetInterrupt(pVCpu, &u8Interrupt);
            if (RT_SUCCESS(rc))
            {
                Log4(("Injecting interrupt u8Interrupt=%#x\n", u8Interrupt));
                uint32_t u32IntrInfo = u8Interrupt | VMX_EXIT_INTERRUPTION_INFO_VALID;
                u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
                rc = hmR0VmxInjectEventVmcs(pVCpu, pMixedCtx, u32IntrInfo, 0 /* cbInstr */,  0 /* u32ErrCode */,
                                            0 /* GCPtrFaultAddress */, &uIntrState);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatIntInject);
            }
            else
            {
                /** @todo Does this actually happen? If not turn it into an assertion. */
                Assert(!VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC)));
                STAM_COUNTER_INC(&pVCpu->hm.s.StatSwitchGuestIrq);
                rc = VINF_SUCCESS;
            }
        }
        else
            hmR0VmxSetIntWindowExitVmcs(pVCpu);
    }

    /*
     * Delivery pending debug exception if the guest is single-stepping. The interruptibility-state could have been changed by
     * hmR0VmxInjectEventVmcs() (e.g. real-on-v86 injecting software interrupts), re-evaluate it and set the BS bit.
     */
    fBlockMovSS = !!(uIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS);
    fBlockSti   = !!(uIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI);
    int rc2 = VINF_SUCCESS;
    if (   fBlockSti
        || fBlockMovSS)
    {
        if (!DBGFIsStepping(pVCpu))
        {
            Assert(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_RFLAGS);
            if (pMixedCtx->eflags.Bits.u1TF)    /* We don't have any IA32_DEBUGCTL MSR for guests. Treat as all bits 0. */
            {
                /*
                 * The pending-debug exceptions field is cleared on all VM-exits except VMX_EXIT_TPR_BELOW_THRESHOLD, VMX_EXIT_MTF
                 * VMX_EXIT_APIC_WRITE, VMX_EXIT_VIRTUALIZED_EOI. See Intel spec. 27.3.4 "Saving Non-Register State".
                 */
                rc2 = VMXWriteVmcs32(VMX_VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_BS);
                AssertRCReturn(rc, rc);
            }
        }
        else
        {
            /* We are single-stepping in the hypervisor debugger, clear interrupt inhibition as setting the BS bit would mean
               delivering a #DB to the guest upon VM-entry when it shouldn't be. */
            uIntrState = 0;
        }
    }

    /*
     * There's no need to clear the VM entry-interruption information field here if we're not injecting anything.
     * VT-x clears the valid bit on every VM-exit. See Intel spec. 24.8.3 "VM-Entry Controls for Event Injection".
     */
    rc2 = hmR0VmxLoadGuestIntrState(pVCpu, uIntrState);
    AssertRC(rc2);

    Assert(rc == VINF_SUCCESS || rc == VINF_EM_RESET);
    return rc;
}


/**
 * Sets an invalid-opcode (#UD) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptUD(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    uint32_t u32IntrInfo = X86_XCPT_UD | VMX_EXIT_INTERRUPTION_INFO_VALID;
    hmR0VmxSetPendingEvent(pVCpu, u32IntrInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Injects a double-fault (#DF) exception into the VM.
 *
 * @returns VBox status code (informational status code included).
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 */
DECLINLINE(int) hmR0VmxInjectXcptDF(PVMCPU pVCpu, PCPUMCTX pMixedCtx, uint32_t *puIntrState)
{
    uint32_t u32IntrInfo = X86_XCPT_DF | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    u32IntrInfo         |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
    return hmR0VmxInjectEventVmcs(pVCpu, pMixedCtx, u32IntrInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */,
                                  puIntrState);
}


/**
 * Sets a debug (#DB) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptDB(PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    uint32_t u32IntrInfo = X86_XCPT_DB | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    hmR0VmxSetPendingEvent(pVCpu, u32IntrInfo, 0 /* cbInstr */, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Sets an overflow (#OF) exception as pending-for-injection into the VM.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   cbInstr         The value of RIP that is to be pushed on the guest
 *                          stack.
 */
DECLINLINE(void) hmR0VmxSetPendingXcptOF(PVMCPU pVCpu, PCPUMCTX pMixedCtx, uint32_t cbInstr)
{
    uint32_t u32IntrInfo = X86_XCPT_OF | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    hmR0VmxSetPendingEvent(pVCpu, u32IntrInfo, cbInstr, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Injects a general-protection (#GP) fault into the VM.
 *
 * @returns VBox status code (informational status code included).
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   u32ErrorCode    The error code associated with the #GP.
 */
DECLINLINE(int) hmR0VmxInjectXcptGP(PVMCPU pVCpu, PCPUMCTX pMixedCtx, bool fErrorCodeValid, uint32_t u32ErrorCode,
                                    uint32_t *puIntrState)
{
    uint32_t u32IntrInfo = X86_XCPT_GP | VMX_EXIT_INTERRUPTION_INFO_VALID;
    u32IntrInfo         |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    if (fErrorCodeValid)
        u32IntrInfo |= VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
    return hmR0VmxInjectEventVmcs(pVCpu, pMixedCtx, u32IntrInfo, 0 /* cbInstr */, u32ErrorCode, 0 /* GCPtrFaultAddress */,
                                  puIntrState);
}


/**
 * Sets a software interrupt (INTn) as pending-for-injection into the VM.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   uVector         The software interrupt vector number.
 * @param   cbInstr         The value of RIP that is to be pushed on the guest
 *                          stack.
 */
DECLINLINE(void) hmR0VmxSetPendingIntN(PVMCPU pVCpu, PCPUMCTX pMixedCtx, uint16_t uVector, uint32_t cbInstr)
{
    uint32_t u32IntrInfo = uVector | VMX_EXIT_INTERRUPTION_INFO_VALID;
    if (   uVector == X86_XCPT_BP
        || uVector == X86_XCPT_OF)
    {
        u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    }
    else
        u32IntrInfo |= (VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT << VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT);
    hmR0VmxSetPendingEvent(pVCpu, u32IntrInfo, cbInstr, 0 /* u32ErrCode */, 0 /* GCPtrFaultAddress */);
}


/**
 * Pushes a 2-byte value onto the real-mode (in virtual-8086 mode) guest's
 * stack.
 *
 * @returns VBox status code (information status code included).
 * @retval VINF_EM_RESET if pushing a value to the stack caused a triple-fault.
 * @param   pVM         Pointer to the VM.
 * @param   pMixedCtx   Pointer to the guest-CPU context.
 * @param   uValue      The value to push to the guest stack.
 */
DECLINLINE(int) hmR0VmxRealModeGuestStackPush(PVM pVM, PCPUMCTX pMixedCtx, uint16_t uValue)
{
    /*
     * The stack limit is 0xffff in real-on-virtual 8086 mode. Real-mode with weird stack limits cannot be run in
     * virtual 8086 mode in VT-x. See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers".
     * See Intel Instruction reference for PUSH and Intel spec. 22.33.1 "Segment Wraparound".
     */
    if (pMixedCtx->sp == 1)
        return VINF_EM_RESET;
    pMixedCtx->sp -= sizeof(uint16_t);       /* May wrap around which is expected behaviour. */
    int rc = PGMPhysSimpleWriteGCPhys(pVM, pMixedCtx->ss.u64Base + pMixedCtx->sp, &uValue, sizeof(uint16_t));
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Injects an event into the guest upon VM-entry by updating the relevant fields
 * in the VM-entry area in the VMCS.
 *
 * @returns VBox status code (informational error codes included).
 * @retval VINF_SUCCESS if the event is successfully injected into the VMCS.
 * @retval VINF_EM_RESET if event injection resulted in a triple-fault.
 *
 * @param   pVCpu               Pointer to the VMCPU.
 * @param   pMixedCtx           Pointer to the guest-CPU context. The data may
 *                              be out-of-sync. Make sure to update the required
 *                              fields before using them.
 * @param   u64IntrInfo         The VM-entry interruption-information field.
 * @param   cbInstr             The VM-entry instruction length in bytes (for
 *                              software interrupts, exceptions and privileged
 *                              software exceptions).
 * @param   u32ErrCode          The VM-entry exception error code.
 * @param   GCPtrFaultAddress   The page-fault address for #PF exceptions.
 * @param   puIntrState         Pointer to the current guest interruptibility-state.
 *                              This interruptibility-state will be updated if
 *                              necessary. This cannot not be NULL.
 *
 * @remarks No-long-jump zone!!!
 * @remarks Requires CR0!
 */
static int hmR0VmxInjectEventVmcs(PVMCPU pVCpu, PCPUMCTX pMixedCtx, uint64_t u64IntrInfo, uint32_t cbInstr,
                                  uint32_t u32ErrCode, RTGCUINTREG GCPtrFaultAddress, uint32_t *puIntrState)
{
    /* Intel spec. 24.8.3 "VM-Entry Controls for Event Injection" specifies the interruption-information field to be 32-bits. */
    AssertMsg(u64IntrInfo >> 32 == 0, ("%#RX64\n", u64IntrInfo));
    Assert(puIntrState);
    uint32_t u32IntrInfo = (uint32_t)u64IntrInfo;

    const uint32_t uVector = VMX_EXIT_INTERRUPTION_INFO_VECTOR(u32IntrInfo);
    const uint32_t uIntrType = VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntrInfo);

    /* Cannot inject an NMI when block-by-MOV SS is in effect. */
    Assert(   uIntrType != VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI
           || !(*puIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS));

    STAM_COUNTER_INC(&pVCpu->hm.s.paStatInjectedIrqsR0[uVector & MASK_INJECT_IRQ_STAT]);

    /* We require CR0 to check if the guest is in real-mode. */
    int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    /*
     * Hardware interrupts & exceptions cannot be delivered through the software interrupt redirection bitmap to the real
     * mode task in virtual-8086 mode. We must jump to the interrupt handler in the (real-mode) guest.
     * See Intel spec. 20.3 "Interrupt and Exception handling in Virtual-8086 Mode" for interrupt & exception classes.
     * See Intel spec. 20.1.4 "Interrupt and Exception Handling" for real-mode interrupt handling.
     */
    if (CPUMIsGuestInRealModeEx(pMixedCtx))
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        if (!pVM->hm.s.vmx.fUnrestrictedGuest)
        {
            Assert(PDMVmmDevHeapIsEnabled(pVM));
            Assert(pVM->hm.s.vmx.pRealModeTSS);

            /* We require RIP, RSP, RFLAGS, CS, IDTR. Save the required ones from the VMCS. */
            rc  = hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
            rc |= hmR0VmxSaveGuestTableRegs(pVCpu, pMixedCtx);
            rc |= hmR0VmxSaveGuestRipRspRflags(pVCpu, pMixedCtx);
            AssertRCReturn(rc, rc);
            Assert(pVCpu->hm.s.vmx.fUpdatedGuestState & HMVMX_UPDATED_GUEST_RIP);

            /* Check if the interrupt handler is present in the IVT (real-mode IDT). IDT limit is (4N - 1). */
            const size_t cbIdtEntry = 4;
            if (uVector * cbIdtEntry + (cbIdtEntry - 1) > pMixedCtx->idtr.cbIdt)
            {
                /* If we are trying to inject a #DF with no valid IDT entry, return a triple-fault. */
                if (uVector == X86_XCPT_DF)
                    return VINF_EM_RESET;
                else if (uVector == X86_XCPT_GP)
                {
                    /* If we're injecting a #GP with no valid IDT entry, inject a double-fault. */
                    return hmR0VmxInjectXcptDF(pVCpu, pMixedCtx, puIntrState);
                }

                /* If we're injecting an interrupt/exception with no valid IDT entry, inject a general-protection fault. */
                /* No error codes for exceptions in real-mode. See Intel spec. 20.1.4 "Interrupt and Exception Handling" */
                return hmR0VmxInjectXcptGP(pVCpu, pMixedCtx, false /* fErrCodeValid */, 0 /* u32ErrCode */, puIntrState);
            }

            /* Software exceptions (#BP and #OF exceptions thrown as a result of INT3 or INTO) */
            uint16_t uGuestIp = pMixedCtx->ip;
            if (VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntrInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT)
            {
                Assert(uVector == X86_XCPT_BP || uVector == X86_XCPT_OF);
                /* #BP and #OF are both benign traps, we need to resume the next instruction. */
                uGuestIp = pMixedCtx->ip + (uint16_t)cbInstr;
            }
            else if (VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntrInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_INT)
                uGuestIp = pMixedCtx->ip + (uint16_t)cbInstr;

            /* Get the code segment selector and offset from the IDT entry for the interrupt handler. */
            uint16_t offIdtEntry    = 0;
            RTSEL    selIdtEntry    = 0;
            RTGCPHYS GCPhysIdtEntry = (RTGCPHYS)pMixedCtx->idtr.pIdt + uVector * cbIdtEntry;
            rc  = PGMPhysSimpleReadGCPhys(pVM, &offIdtEntry, GCPhysIdtEntry,     sizeof(offIdtEntry));
            rc |= PGMPhysSimpleReadGCPhys(pVM, &selIdtEntry, GCPhysIdtEntry + 2, sizeof(selIdtEntry));
            AssertRCReturn(rc, rc);

            /* Construct the stack frame for the interrupt/exception handler. */
            rc  = hmR0VmxRealModeGuestStackPush(pVM, pMixedCtx, pMixedCtx->eflags.u32);
            rc |= hmR0VmxRealModeGuestStackPush(pVM, pMixedCtx, pMixedCtx->cs.Sel);
            rc |= hmR0VmxRealModeGuestStackPush(pVM, pMixedCtx, uGuestIp);
            AssertRCReturn(rc, rc);

            /* Clear the required eflag bits and jump to the interrupt/exception handler. */
            if (rc == VINF_SUCCESS)
            {
                pMixedCtx->eflags.u32 &= ~(X86_EFL_IF | X86_EFL_TF | X86_EFL_RF | X86_EFL_AC);
                pMixedCtx->rip         = offIdtEntry;
                pMixedCtx->cs.Sel      = selIdtEntry;
                pMixedCtx->cs.u64Base  = selIdtEntry << cbIdtEntry;
                if (   VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntrInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT
                    && uVector == X86_XCPT_PF)
                {
                    pMixedCtx->cr2 = GCPtrFaultAddress;
                }
                pVCpu->hm.s.fContextUseFlags |=   HM_CHANGED_GUEST_SEGMENT_REGS
                                                | HM_CHANGED_GUEST_RIP
                                                | HM_CHANGED_GUEST_RFLAGS
                                                | HM_CHANGED_GUEST_RSP;

                /* We're clearing interrupts, which means no block-by-STI interrupt-inhibition. */
                if (*puIntrState & VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI)
                {
                    Assert(   uIntrType != VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI
                           && uIntrType != VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT);
                    Log4(("Clearing inhibition due to STI.\n"));
                    *puIntrState &= ~VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI;
                }
                Log4(("Injecting real-mode: u32IntrInfo=%#x u32ErrCode=%#x instrlen=%#x\n", u32IntrInfo, u32ErrCode, cbInstr));
            }
            Assert(rc == VINF_SUCCESS || rc == VINF_EM_RESET);
            return rc;
        }
        else
        {
            /*
             * For unrestricted execution enabled CPUs running real-mode guests, we must not set the deliver-error-code bit.
             * See Intel spec. 26.2.1.3 "VM-Entry Control Fields".
             */
            u32IntrInfo &= ~VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID;
        }
    }

    /* Validate. */
    Assert(VMX_EXIT_INTERRUPTION_INFO_IS_VALID(u32IntrInfo));       /* Bit 31 (Valid bit) must be set by caller. */
    Assert(!VMX_EXIT_INTERRUPTION_INFO_NMI_UNBLOCK(u32IntrInfo));   /* Bit 12 MBZ. */
    Assert(!(u32IntrInfo & 0x7ffff000));                            /* Bits 30:12 MBZ. */

    /* Inject. */
    rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO, u32IntrInfo);
    if (VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(u32IntrInfo))
        rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE, u32ErrCode);
    rc |= VMXWriteVmcs32(VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH, cbInstr);

    if (   VMX_EXIT_INTERRUPTION_INFO_TYPE(u32IntrInfo) == VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT
        && uVector == X86_XCPT_PF)
    {
        pMixedCtx->cr2 = GCPtrFaultAddress;
    }

    Log4(("Injecting vcpu[%RU32] u32IntrInfo=%#x u32ErrCode=%#x cbInstr=%#x pMixedCtx->uCR2=%#RX64\n", pVCpu->idCpu,
          u32IntrInfo, u32ErrCode, cbInstr, pMixedCtx->cr2));

    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Enters the VT-x session.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCpu        Pointer to the CPU info struct.
 */
VMMR0DECL(int) VMXR0Enter(PVM pVM, PVMCPU pVCpu, PHMGLOBLCPUINFO pCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    Assert(pVM->hm.s.vmx.fSupported);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    NOREF(pCpu);

    LogFlowFunc(("pVM=%p pVCpu=%p\n", pVM, pVCpu));

    /* Make sure we're in VMX root mode. */
    RTCCUINTREG u32HostCR4 = ASMGetCR4();
    if (!(u32HostCR4 & X86_CR4_VMXE))
    {
        LogRel(("VMXR0Enter: X86_CR4_VMXE bit in CR4 is not set!\n"));
        return VERR_VMX_X86_CR4_VMXE_CLEARED;
    }

    /* Load the active VMCS as the current one. */
    int rc = VMXActivateVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);
    if (RT_FAILURE(rc))
        return rc;

    /** @todo this will change with preemption hooks where can can VMRESUME as long
     *        as we're no preempted. */
    pVCpu->hm.s.fResumeVM = false;
    return VINF_SUCCESS;
}


/**
 * Leaves the VT-x session.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 */
VMMR0DECL(int) VMXR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    AssertPtr(pVCpu);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    NOREF(pVM);
    NOREF(pCtx);

    /** @todo this will change with preemption hooks where we only VMCLEAR when
     *        we are actually going to be preempted, not all the time like we
     *        currently do. */

    /* Restore host-state bits that VT-x only restores partially. */
    if (pVCpu->hm.s.vmx.fRestoreHostFlags)
    {
#ifndef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
        /** @todo r=ramshankar: This is broken when
         *        VBOX_WITH_VMMR0_DISABLE_PREEMPTION is not defined. As
         *        VMXRestoreHostState() may unconditionally enables interrupts. */
#error "VMM: Fix Me! Make VMXRestoreHostState() function to skip cli/sti."
#else
        Assert(ASMIntAreEnabled());
        VMXRestoreHostState(pVCpu->hm.s.vmx.fRestoreHostFlags, &pVCpu->hm.s.vmx.RestoreHost);
#endif
        pVCpu->hm.s.vmx.fRestoreHostFlags = 0;
    }

    /*
     * Sync the current VMCS (writes back internal data back into the VMCS region in memory)
     * and mark the VMCS launch-state as "clear".
     */
    int rc = VMXClearVMCS(pVCpu->hm.s.vmx.HCPhysVmcs);
    return rc;
}


/**
 * Saves the host state in the VMCS host-state.
 * Sets up the VM-exit MSR-load area.
 *
 * The CPU state will be loaded from these fields on every successful VM-exit.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(int) VMXR0SaveHostState(PVM pVM, PVMCPU pVCpu)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    LogFlowFunc(("pVM=%p pVCpu=%p\n", pVM, pVCpu));

    /* Nothing to do if the host-state-changed flag isn't set. This will later be optimized when preemption hooks are in place. */
    if (!(pVCpu->hm.s.fContextUseFlags & HM_CHANGED_HOST_CONTEXT))
        return VINF_SUCCESS;

    int rc = hmR0VmxSaveHostControlRegs(pVM, pVCpu);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveHostControlRegisters failed! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxSaveHostSegmentRegs(pVM, pVCpu);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveHostSegmentRegisters failed! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxSaveHostMsrs(pVM, pVCpu);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSaveHostMsrs failed! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_HOST_CONTEXT;
    return rc;
}


/**
 * Loads the guest state into the VMCS guest-state area. The CPU state will be
 * loaded from these fields on every successful VM-entry.
 *
 * Sets up the VM-entry MSR-load and VM-exit MSR-store areas.
 * Sets up the VM-entry controls.
 * Sets up the appropriate VMX non-root function to execute guest code based on
 * the guest CPU mode.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(int) VMXR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    AssertPtr(pMixedCtx);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

#ifdef LOG_ENABLED
    /** @todo r=ramshankar: I'm not able to use VMMRZCallRing3Disable() here,
     *        probably not initialized yet? Anyway this will do for now. */
    bool fCallerDisabledLogFlush = VMMR0IsLogFlushDisabled(pVCpu);
    VMMR0LogFlushDisable(pVCpu);
#endif

    LogFlowFunc(("pVM=%p pVCpu=%p\n", pVM, pVCpu));

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatLoadGuestState, x);

    /* Determine real-on-v86 mode. */
    pVCpu->hm.s.vmx.RealMode.fRealOnV86Active = false;
    if (   !pVM->hm.s.vmx.fUnrestrictedGuest
        && CPUMIsGuestInRealModeEx(pMixedCtx))
    {
        pVCpu->hm.s.vmx.RealMode.fRealOnV86Active = true;
    }

    /*
     * Load the guest-state into the VMCS.
     * Any ordering dependency among the sub-functions below must be explicitly stated using comments.
     * Ideally, assert that the cross-dependent bits are up to date at the point of using it.
     */
    int rc = hmR0VmxLoadGuestEntryCtls(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestEntryCtls! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxLoadGuestExitCtls(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSetupExitCtls failed! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxLoadGuestActivityState(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestActivityState! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxLoadGuestControlRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestControlRegs: rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    /* Must be done after CR0 is loaded (strict builds require CR0 for segment register validation checks). */
    rc = hmR0VmxLoadGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestSegmentRegs: rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxLoadGuestDebugRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestDebugRegs: rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxLoadGuestMsrs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestMsrs! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxLoadGuestApicState(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestApicState! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    /* Must be done after hmR0VmxLoadGuestDebugRegs() as it may update eflags.TF for debugging purposes. */
    rc = hmR0VmxLoadGuestRipRspRflags(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxLoadGuestGprs! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    rc = hmR0VmxSetupVMRunHandler(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0VmxSetupVMRunHandler! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    /* Clear any unused and reserved bits. */
    pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR2;

    AssertMsg(!pVCpu->hm.s.fContextUseFlags,
             ("Missed updating flags while loading guest state. pVM=%p pVCpu=%p idCpu=%RU32 fContextUseFlags=%#RX32\n",
              pVM, pVCpu, pVCpu->idCpu, pVCpu->hm.s.fContextUseFlags));

#ifdef LOG_ENABLED
    /* Only reenable log-flushing if the caller has it enabled. */
    if (!fCallerDisabledLogFlush)
        VMMR0LogFlushEnable(pVCpu);
#endif

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatLoadGuestState, x);
    return rc;
}


/**
 * Does the preparations before executing guest code in VT-x.
 *
 * This may cause longjmps to ring-3 and may even result in rescheduling to the
 * recompiler. We must be cautious what we do here regarding committing
 * guest-state information into the the VMCS assuming we assuredly execute the
 * guest in VT-x. If we fall back to the recompiler after updating the VMCS and
 * clearing the common-state (TRPM/forceflags), we must undo those changes so
 * that the recompiler can (and should) use them when it resumes guest
 * execution. Otherwise such operations must be done when we can no longer
 * exit to ring-3.
 *
 * @returns VBox status code (informational status codes included).
 * @retval VINF_SUCCESS if we can proceed with running the guest.
 * @retval VINF_EM_RESET if a triple-fault occurs while injecting a double-fault
 *         into the guest.
 * @retval VINF_* scheduling changes, we have to go back to ring-3.
 *
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks Called with preemption disabled.
 */
DECLINLINE(int) hmR0VmxPreRunGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PGMRZDynMapFlushAutoSet(pVCpu);
#endif

    /* Check force flag actions that might require us to go back to ring-3. */
    int rc = hmR0VmxCheckForceFlags(pVM, pVCpu, pMixedCtx);
    if (rc != VINF_SUCCESS)
        return rc;

    /* Setup the Virtualized APIC accesses. pMixedCtx->msrApicBase is always up-to-date. It's not part of the VMCS. */
    if (   pVCpu->hm.s.vmx.u64MsrApicBase != pMixedCtx->msrApicBase
        && (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC))
    {
        Assert(pVM->hm.s.vmx.HCPhysApicAccess);
        RTGCPHYS GCPhysApicBase;
        GCPhysApicBase  = pMixedCtx->msrApicBase;
        GCPhysApicBase &= PAGE_BASE_GC_MASK;

        /* Unalias any existing mapping. */
        rc = PGMHandlerPhysicalReset(pVM, GCPhysApicBase);
        AssertRCReturn(rc, rc);

        /* Map the HC APIC-access page into the GC space, this also updates the shadow page tables if necessary. */
        Log4(("Mapped HC APIC-access page into GC: GCPhysApicBase=%#RGv\n", GCPhysApicBase));
        rc = IOMMMIOMapMMIOHCPage(pVM, pVCpu, GCPhysApicBase, pVM->hm.s.vmx.HCPhysApicAccess, X86_PTE_RW | X86_PTE_P);
        AssertRCReturn(rc, rc);

        pVCpu->hm.s.vmx.u64MsrApicBase = pMixedCtx->msrApicBase;
    }

#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    /* We disable interrupts so that we don't miss any interrupts that would flag preemption (IPI/timers etc.) */
    pVmxTransient->uEFlags = ASMIntDisableFlags();
    if (RTThreadPreemptIsPending(NIL_RTTHREAD))
    {
        ASMSetFlags(pVmxTransient->uEFlags);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatPendingHostIrq);
        /* Don't use VINF_EM_RAW_INTERRUPT_HYPER as we can't assume the host does kernel preemption. Maybe some day? */
        return VINF_EM_RAW_INTERRUPT;
    }
    VMCPU_ASSERT_STATE(pVCpu, VMCPUSTATE_STARTED_HM);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);
#endif

    /*
     * Evaluates and injects any pending events, toggling force-flags and updating the guest-interruptibility
     * state (interrupt shadow) in the VMCS. This -can- potentially be reworked to be done before disabling
     * interrupts and handle returning to ring-3 afterwards, but requires very careful state restoration.
     */
    /** @todo Rework event evaluation and injection to be completely separate. */
    if (TRPMHasTrap(pVCpu))
        hmR0VmxTrpmTrapToPendingEvent(pVCpu);

    rc = hmR0VmxInjectPendingEvent(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);
    return rc;
}


/**
 * Prepares to run guest code in VT-x and we've committed to doing so. This
 * means there is no backing out to ring-3 or anywhere else at this
 * point.
 *
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data may be
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks Called with preemption disabled.
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(void) hmR0VmxPreRunGuestCommitted(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    Assert(VMMR0IsLogFlushDisabled(pVCpu));

#ifndef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
    /** @todo I don't see the point of this, VMMR0EntryFast() already disables interrupts for the entire period. */
    pVmxTransient->uEFlags = ASMIntDisableFlags();
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);
#endif

    /* Load the required guest state bits (for guest-state changes in the inner execution loop). */
    Assert(!(pVCpu->hm.s.fContextUseFlags & HM_CHANGED_HOST_CONTEXT));
    Log5(("LoadFlags=%#RX32\n", pVCpu->hm.s.fContextUseFlags));
#ifdef HMVMX_SYNC_FULL_GUEST_STATE
    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_ALL_GUEST;
#endif
    int rc = VINF_SUCCESS;
    if (pVCpu->hm.s.fContextUseFlags == HM_CHANGED_GUEST_RIP)
    {
        rc = hmR0VmxLoadGuestRip(pVCpu, pMixedCtx);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatLoadMinimal);
    }
    else if (pVCpu->hm.s.fContextUseFlags)
    {
        rc = VMXR0LoadGuestState(pVM, pVCpu, pMixedCtx);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatLoadFull);
    }
    AssertRC(rc);
    AssertMsg(!pVCpu->hm.s.fContextUseFlags, ("fContextUseFlags =%#x\n", pVCpu->hm.s.fContextUseFlags));

    /* Cache the TPR-shadow for checking on every VM-exit if it might have changed. */
    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
        pVmxTransient->u8GuestTpr = pVCpu->hm.s.vmx.pbVirtApic[0x80];

    if (   pVmxTransient->fUpdateTscOffsettingAndPreemptTimer
        || HMR0GetCurrentCpu()->idCpu != pVCpu->hm.s.idLastCpu)
    {
        hmR0VmxUpdateTscOffsettingAndPreemptTimer(pVCpu, pMixedCtx);
        pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = false;
    }

    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, true);    /* Used for TLB-shootdowns, set this across the world switch. */
    hmR0VmxFlushTaggedTlb(pVCpu);                               /* Invalidate the appropriate guest entries from the TLB. */
    Assert(HMR0GetCurrentCpu()->idCpu == pVCpu->hm.s.idLastCpu);

#ifndef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
    /*
     * Save the current Host TSC_AUX and write the guest TSC_AUX to the host, so that
     * RDTSCPs (that don't cause exits) reads the guest MSR. See @bugref{3324}.
     */
    if (    (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
        && !(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT))
    {
        pVCpu->hm.s.u64HostTscAux = ASMRdMsr(MSR_K8_TSC_AUX);
        uint64_t u64HostTscAux = 0;
        int rc2 = CPUMQueryGuestMsr(pVCpu, MSR_K8_TSC_AUX, &u64HostTscAux);
        AssertRC(rc2);
        ASMWrMsr(MSR_K8_TSC_AUX, u64HostTscAux);
    }
#endif

    TMNotifyStartOfExecution(pVCpu);                            /* Finally, notify TM to resume its clocks as we're about
                                                                    to start executing. */
    STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatEntry, &pVCpu->hm.s.StatInGC, x);
}


/**
 * Performs some essential restoration of state after running guest code in
 * VT-x.
 *
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data maybe
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 * @param   rcVMRun         Return code of VMLAUNCH/VMRESUME.
 *
 * @remarks Called with interrupts disabled.
 * @remarks No-long-jump zone!!! This function will however re-enable longjmps
 *          unconditionally when it is safe to do so.
 */
DECLINLINE(void) hmR0VmxPostRunGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient, int rcVMRun)
{
    Assert(!VMMRZCallRing3IsEnabled(pVCpu));
    STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatInGC, &pVCpu->hm.s.StatExit1, x);

    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, false);   /* See HMInvalidatePageOnAllVCpus(): used for TLB-shootdowns. */
    ASMAtomicIncU32(&pVCpu->hm.s.cWorldSwitchExits);            /* Initialized in vmR3CreateUVM(): used for TLB-shootdowns. */
    pVCpu->hm.s.vmx.fUpdatedGuestState = 0;                     /* Exits/longjmps to ring-3 requires saving the guest state. */
    pVmxTransient->fVmcsFieldsRead     = 0;                     /* Transient fields need to be read from the VMCS. */
    pVmxTransient->fVectoringPF        = false;                 /* Vectoring page-fault needs to be determined later. */

    if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_RDTSC_EXIT))
    {
#ifndef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
        /* Restore host's TSC_AUX. */
        if (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
            ASMWrMsr(MSR_K8_TSC_AUX, pVCpu->hm.s.u64HostTscAux);
#endif
        /** @todo Find a way to fix hardcoding a guestimate.  */
        TMCpuTickSetLastSeen(pVCpu, ASMReadTSC()
                             + pVCpu->hm.s.vmx.u64TSCOffset - 0x400 /* guestimate of world switch overhead in clock ticks */);
    }

    TMNotifyEndOfExecution(pVCpu);                              /* Notify TM that the guest is no longer running. */
    Assert(!(ASMGetFlags() & X86_EFL_IF));
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_HM);

    ASMSetFlags(pVmxTransient->uEFlags);                        /* Enable interrupts. */
    pVCpu->hm.s.fResumeVM = true;                               /* Use VMRESUME instead of VMLAUNCH in the next run. */

    /* Save the basic VM-exit reason. Refer Intel spec. 24.9.1 "Basic VM-exit Information". */
    uint32_t uExitReason;
    int rc  = VMXReadVmcs32(VMX_VMCS32_RO_EXIT_REASON, &uExitReason);
    rc     |= hmR0VmxReadEntryIntrInfoVmcs(pVmxTransient);
    AssertRC(rc);
    pVmxTransient->uExitReason    = (uint16_t)VMX_EXIT_REASON_BASIC(uExitReason);
    pVmxTransient->fVMEntryFailed = !!VMX_ENTRY_INTERRUPTION_INFO_VALID(pVmxTransient->uEntryIntrInfo);

    VMMRZCallRing3SetNotification(pVCpu, hmR0VmxCallRing3Callback, pMixedCtx);
    VMMRZCallRing3Enable(pVCpu);                                /* It is now safe to do longjmps to ring-3!!! */

    /* If the VMLAUNCH/VMRESUME failed, we can bail out early. This does -not- cover VMX_EXIT_ERR_*. */
    if (RT_UNLIKELY(rcVMRun != VINF_SUCCESS))
    {
        Log4(("VM-entry failure: pVCpu=%p idCpu=%RU32 rcVMRun=%Rrc fVMEntryFailed=%RTbool\n", pVCpu, pVCpu->idCpu, rcVMRun,
              pVmxTransient->fVMEntryFailed));
        return;
    }

    if (RT_LIKELY(!pVmxTransient->fVMEntryFailed))
    {
        /* Update the guest interruptibility-state from the VMCS. */
        hmR0VmxSaveGuestIntrState(pVCpu, pMixedCtx);
#if defined(HMVMX_SYNC_FULL_GUEST_STATE) || defined(HMVMX_SAVE_FULL_GUEST_STATE)
        rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
        AssertRC(rc);
#endif
        /*
         * If the TPR was raised by the guest, it wouldn't cause a VM-exit immediately. Instead we sync the TPR lazily whenever
         * we eventually get a VM-exit for any reason. This maybe expensive as PDMApicSetTPR() can longjmp to ring-3 and which is
         * why it's done here as it's easier and no less efficient to deal with it here than making hmR0VmxSaveGuestState()
         * cope with longjmps safely (see VMCPU_FF_HM_UPDATE_CR3 handling).
         */
        if (   (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
            && pVmxTransient->u8GuestTpr != pVCpu->hm.s.vmx.pbVirtApic[0x80])
        {
            rc = PDMApicSetTPR(pVCpu, pVCpu->hm.s.vmx.pbVirtApic[0x80]);
            AssertRC(rc);
            pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_VMX_GUEST_APIC_STATE;
        }
    }
}


/**
 * Runs the guest code using VT-x.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks Called with preemption disabled.
 */
VMMR0DECL(int) VMXR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    Assert(VMMRZCallRing3IsEnabled(pVCpu));
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    VMXTRANSIENT VmxTransient;
    VmxTransient.fUpdateTscOffsettingAndPreemptTimer = true;
    int          rc     = VERR_INTERNAL_ERROR_5;
    uint32_t     cLoops = 0;

    for (;; cLoops++)
    {
        Assert(!HMR0SuspendPending());
        AssertMsg(pVCpu->hm.s.idEnteredCpu == RTMpCpuId(),
                  ("Illegal migration! Entered on CPU %u Current %u cLoops=%u\n", (unsigned)pVCpu->hm.s.idEnteredCpu,
                  (unsigned)RTMpCpuId(), cLoops));

        /* Preparatory work for running guest code, this may return to ring-3 for some last minute updates. */
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatEntry, x);
        rc = hmR0VmxPreRunGuest(pVM, pVCpu, pCtx, &VmxTransient);
        if (rc != VINF_SUCCESS)
            break;

        /*
         * No longjmps to ring-3 from this point on!!!
         * Asserts() will still longjmp to ring-3 (but won't return), which is intentional, better than a kernel panic.
         * This also disables flushing of the R0-logger instance (if any).
         */
        VMMRZCallRing3Disable(pVCpu);
        VMMRZCallRing3RemoveNotification(pVCpu);
        hmR0VmxPreRunGuestCommitted(pVM, pVCpu, pCtx, &VmxTransient);

        rc = hmR0VmxRunGuest(pVM, pVCpu, pCtx);
        /* The guest-CPU context is now outdated, 'pCtx' is to be treated as 'pMixedCtx' from this point on!!! */

        /*
         * Restore any residual host-state and save any bits shared between host and guest into the guest-CPU state.
         * This will also re-enable longjmps to ring-3 when it has reached a safe point!!!
         */
        hmR0VmxPostRunGuest(pVM, pVCpu, pCtx, &VmxTransient, rc);
        if (RT_UNLIKELY(rc != VINF_SUCCESS))        /* Check for errors with running the VM (VMLAUNCH/VMRESUME). */
        {
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExit1, x);
            hmR0VmxReportWorldSwitchError(pVM, pVCpu, rc, pCtx, &VmxTransient);
            return rc;
        }

        /* Handle the VM-exit. */
        AssertMsg(VmxTransient.uExitReason <= VMX_EXIT_MAX, ("%#x\n", VmxTransient.uExitReason));
        STAM_COUNTER_INC(&pVCpu->hm.s.paStatExitReasonR0[VmxTransient.uExitReason & MASK_EXITREASON_STAT]);
        STAM_PROFILE_ADV_STOP_START(&pVCpu->hm.s.StatExit1, &pVCpu->hm.s.StatExit2, x);
        HMVMX_START_EXIT_DISPATCH_PROF();
#ifdef HMVMX_USE_FUNCTION_TABLE
        rc = g_apfnVMExitHandlers[VmxTransient.uExitReason](pVCpu, pCtx, &VmxTransient);
#else
        rc = hmR0VmxHandleExit(pVCpu, pCtx, &VmxTransient, VmxTransient.uExitReason);
#endif
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExit2, x);
        if (rc != VINF_SUCCESS)
            break;
        else if (cLoops > pVM->hm.s.cMaxResumeLoops)
        {
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMaxResume);
            rc = VINF_EM_RAW_INTERRUPT;
            break;
        }
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatEntry, x);
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;
    else if (rc == VINF_EM_RESET)
        rc = VINF_EM_TRIPLE_FAULT;
    hmR0VmxExitToRing3(pVM, pVCpu, pCtx, rc);
    return rc;
}


#ifndef HMVMX_USE_FUNCTION_TABLE
DECLINLINE(int) hmR0VmxHandleExit(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient, uint32_t rcReason)
{
    int rc;
    switch (rcReason)
    {
        case VMX_EXIT_EPT_MISCONFIG:           rc = hmR0VmxExitEptMisconfig(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_EPT_VIOLATION:           rc = hmR0VmxExitEptViolation(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_IO_INSTR:                rc = hmR0VmxExitIoInstr(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_CPUID:                   rc = hmR0VmxExitCpuid(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_RDTSC:                   rc = hmR0VmxExitRdtsc(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_RDTSCP:                  rc = hmR0VmxExitRdtscp(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_APIC_ACCESS:             rc = hmR0VmxExitApicAccess(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_XCPT_NMI:                rc = hmR0VmxExitXcptNmi(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_MOV_CRX:                 rc = hmR0VmxExitMovCRx(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_EXT_INT:                 rc = hmR0VmxExitExtInt(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_INT_WINDOW:              rc = hmR0VmxExitIntWindow(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_MWAIT:                   rc = hmR0VmxExitMwait(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_MONITOR:                 rc = hmR0VmxExitMonitor(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_TASK_SWITCH:             rc = hmR0VmxExitTaskSwitch(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_PREEMPT_TIMER:           rc = hmR0VmxExitPreemptTimer(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_RDMSR:                   rc = hmR0VmxExitRdmsr(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_WRMSR:                   rc = hmR0VmxExitWrmsr(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_MOV_DRX:                 rc = hmR0VmxExitMovDRx(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_TPR_BELOW_THRESHOLD:     rc = hmR0VmxExitTprBelowThreshold(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_HLT:                     rc = hmR0VmxExitHlt(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_INVD:                    rc = hmR0VmxExitInvd(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_INVLPG:                  rc = hmR0VmxExitInvlpg(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_RSM:                     rc = hmR0VmxExitRsm(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_MTF:                     rc = hmR0VmxExitMtf(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_PAUSE:                   rc = hmR0VmxExitPause(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_XDTR_ACCESS:             rc = hmR0VmxExitXdtrAccess(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_TR_ACCESS:               rc = hmR0VmxExitXdtrAccess(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_WBINVD:                  rc = hmR0VmxExitWbinvd(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_XSETBV:                  rc = hmR0VmxExitXsetbv(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_RDRAND:                  rc = hmR0VmxExitRdrand(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_INVPCID:                 rc = hmR0VmxExitInvpcid(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_GETSEC:                  rc = hmR0VmxExitGetsec(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_RDPMC:                   rc = hmR0VmxExitRdpmc(pVCpu, pMixedCtx, pVmxTransient); break;

        case VMX_EXIT_TRIPLE_FAULT:            rc = hmR0VmxExitTripleFault(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_NMI_WINDOW:              rc = hmR0VmxExitNmiWindow(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_INIT_SIGNAL:             rc = hmR0VmxExitInitSignal(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_SIPI:                    rc = hmR0VmxExitSipi(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_IO_SMI:                  rc = hmR0VmxExitIoSmi(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_SMI:                     rc = hmR0VmxExitSmi(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_ERR_MSR_LOAD:            rc = hmR0VmxExitErrMsrLoad(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_ERR_INVALID_GUEST_STATE: rc = hmR0VmxExitErrInvalidGuestState(pVCpu, pMixedCtx, pVmxTransient); break;
        case VMX_EXIT_ERR_MACHINE_CHECK:       rc = hmR0VmxExitErrMachineCheck(pVCpu, pMixedCtx, pVmxTransient); break;

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
        case VMX_EXIT_INVEPT:
        case VMX_EXIT_INVVPID:
        case VMX_EXIT_VMFUNC:
            rc = hmR0VmxExitSetPendingXcptUD(pVCpu, pMixedCtx, pVmxTransient);
            break;
        default:
            rc = hmR0VmxExitErrUndefined(pVCpu, pMixedCtx, pVmxTransient);
            break;
    }
    return rc;
}
#endif

#ifdef DEBUG
/* Is there some generic IPRT define for this that are not in Runtime/internal/\* ?? */
# define HMVMX_ASSERT_PREEMPT_CPUID_VAR() \
    RTCPUID const idAssertCpu = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId()

# define HMVMX_ASSERT_PREEMPT_CPUID() \
   do \
   { \
        RTCPUID const idAssertCpuNow = RTThreadPreemptIsEnabled(NIL_RTTHREAD) ? NIL_RTCPUID : RTMpCpuId(); \
        AssertMsg(idAssertCpu == idAssertCpuNow,  ("VMX %#x, %#x\n", idAssertCpu, idAssertCpuNow)); \
   } while (0)

# define HMVMX_VALIDATE_EXIT_HANDLER_PARAMS() \
            do { \
                AssertPtr(pVCpu); \
                AssertPtr(pMixedCtx); \
                AssertPtr(pVmxTransient); \
                Assert(pVmxTransient->fVMEntryFailed == false); \
                Assert(ASMIntAreEnabled()); \
                Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD)); \
                HMVMX_ASSERT_PREEMPT_CPUID_VAR(); \
                Log4Func(("vcpu[%RU32] -v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v-v\n", pVCpu->idCpu)); \
                Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD)); \
                if (VMMR0IsLogFlushDisabled(pVCpu)) \
                    HMVMX_ASSERT_PREEMPT_CPUID(); \
                HMVMX_STOP_EXIT_DISPATCH_PROF(); \
            } while (0)

# define HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS() \
            do { \
                Log4Func(("\n")); \
            } while(0)
#else   /* Release builds */
# define HMVMX_VALIDATE_EXIT_HANDLER_PARAMS() do { HMVMX_STOP_EXIT_DISPATCH_PROF(); } while(0)
# define HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS() do { } while(0)
#endif


/**
 * Advances the guest RIP after reading it from the VMCS.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pMixedCtx       Pointer to the guest-CPU context. The data maybe
 *                          out-of-sync. Make sure to update the required fields
 *                          before using them.
 * @param   pVmxTransient   Pointer to the VMX transient structure.
 *
 * @remarks No-long-jump zone!!!
 */
DECLINLINE(int) hmR0VmxAdvanceGuestRip(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    int rc = hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxSaveGuestRip(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    pMixedCtx->rip += pVmxTransient->cbInstr;
    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP;
    return rc;
}


/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- VM-exit handlers -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/**
 * VM-exit handler for external interrupts (VMX_EXIT_EXT_INT).
 */
HMVMX_EXIT_DECL hmR0VmxExitExtInt(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitExtInt);
    /* 32-bit Windows hosts (4 cores) has trouble with this; causes higher interrupt latency. */
#if HC_ARCH_BITS == 64 && defined(VBOX_WITH_VMMR0_DISABLE_PREEMPTION)
    Assert(ASMIntAreEnabled());
    return VINF_SUCCESS;
#else
    return VINF_EM_RAW_INTERRUPT;
#endif
}


/**
 * VM-exit handler for exceptions and NMIs (VMX_EXIT_XCPT_NMI).
 */
HMVMX_EXIT_DECL hmR0VmxExitXcptNmi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitXcptNmi, y3);

    int rc = hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);

    uint32_t uIntrType = VMX_EXIT_INTERRUPTION_INFO_TYPE(pVmxTransient->uExitIntrInfo);
    Assert(   !(pVCpu->hm.s.vmx.u32ExitCtls & VMX_VMCS_CTRL_EXIT_ACK_EXT_INT)
           && uIntrType != VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT_INT);

    if (uIntrType == VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI)
    {
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
        return VINF_EM_RAW_INTERRUPT;
    }

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    rc = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pMixedCtx, pVmxTransient);
    if (RT_UNLIKELY(rc == VINF_HM_DOUBLE_FAULT))
    {
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
        return VINF_SUCCESS;
    }
    else if (RT_UNLIKELY(rc == VINF_EM_RESET))
    {
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
        return rc;
    }

    uint32_t uExitIntrInfo = pVmxTransient->uExitIntrInfo;
    uint32_t uVector       = VMX_EXIT_INTERRUPTION_INFO_VECTOR(uExitIntrInfo);
    switch (uIntrType)
    {
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_SW_XCPT:   /* Software exception. (#BP or #OF) */
            Assert(uVector == X86_XCPT_DB || uVector == X86_XCPT_BP || uVector == X86_XCPT_OF);
            /* no break */
        case VMX_EXIT_INTERRUPTION_INFO_TYPE_HW_XCPT:
        {
            switch (uVector)
            {
                case X86_XCPT_PF: rc = hmR0VmxExitXcptPF(pVCpu, pMixedCtx, pVmxTransient);      break;
                case X86_XCPT_GP: rc = hmR0VmxExitXcptGP(pVCpu, pMixedCtx, pVmxTransient);      break;
                case X86_XCPT_NM: rc = hmR0VmxExitXcptNM(pVCpu, pMixedCtx, pVmxTransient);      break;
                case X86_XCPT_MF: rc = hmR0VmxExitXcptMF(pVCpu, pMixedCtx, pVmxTransient);      break;
                case X86_XCPT_DB: rc = hmR0VmxExitXcptDB(pVCpu, pMixedCtx, pVmxTransient);      break;
                case X86_XCPT_BP: rc = hmR0VmxExitXcptBP(pVCpu, pMixedCtx, pVmxTransient);      break;
#ifdef HMVMX_ALWAYS_TRAP_ALL_XCPTS
                case X86_XCPT_XF: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestXF);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pMixedCtx, pVmxTransient); break;
                case X86_XCPT_DE: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestDE);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pMixedCtx, pVmxTransient); break;
                case X86_XCPT_UD: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestUD);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pMixedCtx, pVmxTransient); break;
                case X86_XCPT_SS: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestSS);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pMixedCtx, pVmxTransient); break;
                case X86_XCPT_NP: STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestNP);
                                  rc = hmR0VmxExitXcptGeneric(pVCpu, pMixedCtx, pVmxTransient); break;
#endif
                default:
                {
                    rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
                    AssertRCReturn(rc, rc);

                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestXcpUnk);
                    if (pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
                    {
                        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.vmx.pRealModeTSS);
                        Assert(PDMVmmDevHeapIsEnabled(pVCpu->CTX_SUFF(pVM)));
                        rc  = hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
                        rc |= hmR0VmxReadExitIntrErrorCodeVmcs(pVCpu, pVmxTransient);
                        AssertRCReturn(rc, rc);
                        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(uExitIntrInfo),
                                               pVmxTransient->cbInstr, pVmxTransient->uExitIntrErrorCode,
                                               0 /* GCPtrFaultAddress */);
                        AssertRCReturn(rc, rc);
                    }
                    else
                    {
                        AssertMsgFailed(("Unexpected VM-exit caused by exception %#x\n", uVector));
                        rc = VERR_VMX_UNEXPECTED_EXCEPTION;
                    }
                    break;
                }
            }
            break;
        }

        case VMX_EXIT_INTERRUPTION_INFO_TYPE_DB_XCPT:
        default:
        {
            rc = VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_CODE;
            AssertMsgFailed(("Unexpected interruption code %#x\n", VMX_EXIT_INTERRUPTION_INFO_TYPE(uExitIntrInfo)));
            break;
        }
    }
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitXcptNmi, y3);
    return rc;
}


/**
 * VM-exit handler for interrupt-window exiting (VMX_EXIT_INT_WINDOW).
 */
HMVMX_EXIT_DECL hmR0VmxExitIntWindow(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* Indicate that we no longer need to VM-exit when the guest is ready to receive interrupts, it is now ready. */
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT);
    pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_INT_WINDOW_EXIT;
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
    AssertRCReturn(rc, rc);

    /* Deliver the pending interrupt via hmR0VmxPreRunGuest()->hmR0VmxInjectEvent() and resume guest execution. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIntWindow);
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for NMI-window exiting (VMX_EXIT_NMI_WINDOW).
 */
HMVMX_EXIT_DECL hmR0VmxExitNmiWindow(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    AssertMsgFailed(("Unexpected NMI-window exit.\n"));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for WBINVD (VMX_EXIT_WBINVD). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitWbinvd(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitWbinvd);
    return hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
}


/**
 * VM-exit handler for INVD (VMX_EXIT_INVD). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitInvd(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitInvd);
    return hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
}


/**
 * VM-exit handler for CPUID (VMX_EXIT_CPUID). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitCpuid(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    int rc = EMInterpretCpuId(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        Assert(pVmxTransient->cbInstr == 2);
    }
    else
    {
        AssertMsgFailed(("hmR0VmxExitCpuid: EMInterpretCpuId failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCpuid);
    return rc;
}


/**
 * VM-exit handler for GETSEC (VMX_EXIT_GETSEC). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitGetsec(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    int rc  = hmR0VmxSaveGuestCR4(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    if (pMixedCtx->cr4 & X86_CR4_SMXE)
        return VINF_EM_RAW_EMULATE_INSTR;

    AssertMsgFailed(("hmR0VmxExitGetsec: unexpected VM-exit when CR4.SMXE is 0.\n"));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for RDTSC (VMX_EXIT_RDTSC). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdtsc(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    int rc = hmR0VmxSaveGuestCR4(pVCpu, pMixedCtx);    /** @todo review if CR4 is really required by EM. */
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = EMInterpretRdtsc(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        Assert(pVmxTransient->cbInstr == 2);
        /* If we get a spurious VM-exit when offsetting is enabled, we must reset offsetting on VM-reentry. See @bugref{6634}. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TSC_OFFSETTING)
            pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;
    }
    else
    {
        AssertMsgFailed(("hmR0VmxExitRdtsc: EMInterpretRdtsc failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdtsc);
    return rc;
}


/**
 * VM-exit handler for RDTSCP (VMX_EXIT_RDTSCP). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdtscp(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    int rc = hmR0VmxSaveGuestCR4(pVCpu, pMixedCtx);                /** @todo review if CR4 is really required by EM. */
    rc    |= hmR0VmxSaveGuestAutoLoadStoreMsrs(pVCpu, pMixedCtx);  /* For MSR_K8_TSC_AUX */
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = EMInterpretRdtscp(pVM, pVCpu, pMixedCtx);
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc  = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        Assert(pVmxTransient->cbInstr == 3);
        /* If we get a spurious VM-exit when offsetting is enabled, we must reset offsetting on VM-reentry. See @bugref{6634}. */
        if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TSC_OFFSETTING)
            pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;
    }
    else
    {
        AssertMsgFailed(("hmR0VmxExitRdtscp: EMInterpretRdtscp failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdtsc);
    return rc;
}


/**
 * VM-exit handler for RDPMC (VMX_EXIT_RDPMC). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdpmc(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    int rc = hmR0VmxSaveGuestCR4(pVCpu, pMixedCtx);    /** @todo review if CR4 is really required by EM. */
    rc    |= hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);    /** @todo review if CR0 is really required by EM. */
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = EMInterpretRdpmc(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        Assert(pVmxTransient->cbInstr == 2);
    }
    else
    {
        AssertMsgFailed(("hmR0VmxExitRdpmc: EMInterpretRdpmc failed with %Rrc\n", rc));
        rc = VERR_EM_INTERPRETER;
    }
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdpmc);
    return rc;
}


/**
 * VM-exit handler for INVLPG (VMX_EXIT_INVLPG). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitInvlpg(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(!pVM->hm.s.fNestedPaging);

    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    VBOXSTRICTRC rc2 = EMInterpretInvlpg(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx), pVmxTransient->uExitQualification);
    rc = VBOXSTRICTRC_VAL(rc2);
    if (RT_LIKELY(rc == VINF_SUCCESS))
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
    else
    {
        AssertMsg(rc == VERR_EM_INTERPRETER, ("hmR0VmxExitInvlpg: EMInterpretInvlpg %#RX64 failed with %Rrc\n",
                                              pVmxTransient->uExitQualification, rc));
    }
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitInvlpg);
    return rc;
}


/**
 * VM-exit handler for MONITOR (VMX_EXIT_MONITOR). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMonitor(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = EMInterpretMonitor(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    if (RT_LIKELY(rc == VINF_SUCCESS))
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
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
HMVMX_EXIT_DECL hmR0VmxExitMwait(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VBOXSTRICTRC rc2 = EMInterpretMWait(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    rc = VBOXSTRICTRC_VAL(rc2);
    if (RT_LIKELY(   rc == VINF_SUCCESS
                  || rc == VINF_EM_HALT))
    {
        int rc3 = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        AssertRCReturn(rc3, rc3);

        if (   rc == VINF_EM_HALT
            && EMShouldContinueAfterHalt(pVCpu, pMixedCtx))
        {
            rc = VINF_SUCCESS;
        }
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
HMVMX_EXIT_DECL hmR0VmxExitRsm(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    /*
     * Execution of RSM outside of SMM mode causes #UD regardless of VMX root or VMX non-root mode. In theory, we should never
     * get this VM-exit. This can happen only if dual-monitor treatment of SMI and VMX is enabled, which can (only?) be done by
     * executing VMCALL in VMX root operation. If we get here something funny is going on.
     * See Intel spec. "33.15.5 Enabling the Dual-Monitor Treatment".
     */
    AssertMsgFailed(("Unexpected RSM VM-exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for SMI (VMX_EXIT_SMI). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitSmi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    /*
     * This can only happen if we support dual-monitor treatment of SMI, which can be activated by executing VMCALL in VMX
     * root operation. If we get there there is something funny going on.
     * See Intel spec. "33.15.6 Activating the Dual-Monitor Treatment" and Intel spec. 25.3 "Other Causes of VM-Exits"
     */
    AssertMsgFailed(("Unexpected SMI VM-exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for IO SMI (VMX_EXIT_IO_SMI). Unconditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitIoSmi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    /* Same treatment as VMX_EXIT_SMI. See comment in hmR0VmxExitSmi(). */
    AssertMsgFailed(("Unexpected IO SMI VM-exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for SIPI (VMX_EXIT_SIPI). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitSipi(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    /*
     * SIPI exits can only occur in VMX non-root operation when the "wait-for-SIPI" guest activity state is used. We currently
     * don't make use of it (see hmR0VmxLoadGuestActivityState()) as our guests don't have direct access to the host LAPIC.
     * See Intel spec. 25.3 "Other Causes of VM-exits".
     */
    AssertMsgFailed(("Unexpected SIPI VM-exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for INIT signal (VMX_EXIT_INIT_SIGNAL). Unconditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitInitSignal(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    /*
     * INIT signals are blocked in VMX root operation by VMXON and by SMI in SMM. See Intel spec. "33.14.1 Default Treatment of
     * SMI Delivery" and "29.3 VMX Instructions" for "VMXON". It is -NOT- blocked in VMX non-root operation so we can potentially
     * still get these exits. See Intel spec. "23.8 Restrictions on VMX operation".
     */
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    return VINF_SUCCESS;    /** @todo r=ramshankar: correct?. */
}


/**
 * VM-exit handler for triple faults (VMX_EXIT_TRIPLE_FAULT). Unconditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitTripleFault(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    return VINF_EM_RESET;
}


/**
 * VM-exit handler for HLT (VMX_EXIT_HLT). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitHlt(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_HLT_EXIT);
    int rc = hmR0VmxSaveGuestRip(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    pMixedCtx->rip++;
    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP;
    if (EMShouldContinueAfterHalt(pVCpu, pMixedCtx))    /* Requires eflags. */
        rc = VINF_SUCCESS;
    else
        rc = VINF_EM_HALT;

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitHlt);
    return rc;
}


/**
 * VM-exit handler for instructions that result in a #UD exception delivered to
 * the guest.
 */
HMVMX_EXIT_DECL hmR0VmxExitSetPendingXcptUD(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    hmR0VmxSetPendingXcptUD(pVCpu, pMixedCtx);
    return VINF_SUCCESS;
}


/**
 * VM-exit handler for expiry of the VMX preemption timer.
 */
HMVMX_EXIT_DECL hmR0VmxExitPreemptTimer(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

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
HMVMX_EXIT_DECL hmR0VmxExitXsetbv(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* We expose XSETBV to the guest, fallback to the recompiler for emulation. */
    /** @todo check if XSETBV is supported by the recompiler. */
    return VERR_EM_INTERPRETER;
}


/**
 * VM-exit handler for INVPCID (VMX_EXIT_INVPCID). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitInvpcid(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* The guest should not invalidate the host CPU's TLBs, fallback to recompiler. */
    /** @todo implement EMInterpretInvpcid() */
    return VERR_EM_INTERPRETER;
}


/**
 * VM-exit handler for invalid-guest-state (VMX_EXIT_ERR_INVALID_GUEST_STATE).
 * Error VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitErrInvalidGuestState(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    uint32_t       uIntrState;
    HMVMXHCUINTREG uHCReg;
    uint64_t       u64Val;
    uint32_t       u32Val;

    int rc  = hmR0VmxReadEntryIntrInfoVmcs(pVmxTransient);
    rc     |= hmR0VmxReadEntryXcptErrorCodeVmcs(pVmxTransient);
    rc     |= hmR0VmxReadEntryInstrLenVmcs(pVCpu, pVmxTransient);
    rc     |= VMXReadVmcs32(VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE, &uIntrState);
    rc     |= hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    Log4(("VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO    %#RX32\n", pVmxTransient->uEntryIntrInfo));
    Log4(("VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE    %#RX32\n", pVmxTransient->uEntryXcptErrorCode));
    Log4(("VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH         %#RX32\n", pVmxTransient->cbEntryInstr));
    Log4(("VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE    %#RX32\n", uIntrState));

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

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    HMDumpRegs(pVM, pVCpu, pMixedCtx);

    return VERR_VMX_INVALID_GUEST_STATE;
}


/**
 * VM-exit handler for VM-entry failure due to an MSR-load
 * (VMX_EXIT_ERR_MSR_LOAD). Error VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitErrMsrLoad(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    AssertMsgFailed(("Unexpected MSR-load exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for VM-entry failure due to a machine-check event
 * (VMX_EXIT_ERR_MACHINE_CHECK). Error VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitErrMachineCheck(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    AssertMsgFailed(("Unexpected machine-check event exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for all undefined reasons. Should never ever happen.. in
 * theory.
 */
HMVMX_EXIT_DECL hmR0VmxExitErrUndefined(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    AssertMsgFailed(("Huh!? Undefined VM-exit reason %d. pVCpu=%p pMixedCtx=%p\n", pVmxTransient->uExitReason, pVCpu, pMixedCtx));
    return VERR_VMX_UNDEFINED_EXIT_CODE;
}


/**
 * VM-exit handler for XDTR (LGDT, SGDT, LIDT, SIDT) accesses
 * (VMX_EXIT_XDTR_ACCESS) and LDT and TR access (LLDT, LTR, SLDT, STR).
 * Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitXdtrAccess(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* By default, we don't enable VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitXdtrAccess);
    if (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT)
        return VERR_EM_INTERPRETER;
    AssertMsgFailed(("Unexpected XDTR access. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for RDRAND (VMX_EXIT_RDRAND). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitRdrand(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* By default, we don't enable VMX_VMCS_CTRL_PROC_EXEC2_RDRAND_EXIT. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdrand);
    if (pVCpu->hm.s.vmx.u32ProcCtls2 & VMX_VMCS_CTRL_PROC_EXEC2_RDRAND_EXIT)
        return VERR_EM_INTERPRETER;
    AssertMsgFailed(("Unexpected RDRAND exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for RDMSR (VMX_EXIT_RDMSR).
 */
HMVMX_EXIT_DECL hmR0VmxExitRdmsr(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* EMInterpretRdmsr() requires CR0, Eflags and SS segment register. */
    int rc  = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    rc     |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
    rc     |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = EMInterpretRdmsr(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    AssertMsg(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER,
              ("hmR0VmxExitRdmsr: failed, invalid error code %Rrc\n", rc));
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitRdmsr);

    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        Assert(pVmxTransient->cbInstr == 2);
    }
    return rc;
}


/**
 * VM-exit handler for WRMSR (VMX_EXIT_WRMSR).
 */
HMVMX_EXIT_DECL hmR0VmxExitWrmsr(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    int rc = VINF_SUCCESS;

    /* EMInterpretWrmsr() requires CR0, EFLAGS and SS segment register. */
    rc  = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);
    Log4(("ecx=%#RX32\n", pMixedCtx->ecx));

    rc = EMInterpretWrmsr(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    AssertMsg(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER, ("hmR0VmxExitWrmsr: failed, invalid error code %Rrc\n", rc));
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitWrmsr);

    if (RT_LIKELY(rc == VINF_SUCCESS))
    {
        rc = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);

        /* If this is an X2APIC WRMSR access, update the APIC state as well. */
        if (   pMixedCtx->ecx >= MSR_IA32_X2APIC_START
            && pMixedCtx->ecx <= MSR_IA32_X2APIC_END)
        {
            /* We've already saved the APIC related guest-state (TPR) in hmR0VmxPostRunGuest(). When full APIC register
             * virtualization is implemented we'll have to make sure APIC state is saved from the VMCS before
               EMInterpretWrmsr() changes it. */
            pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_VMX_GUEST_APIC_STATE;
        }
        else if (pMixedCtx->ecx == MSR_K6_EFER)         /* EFER is the only MSR we auto-load but don't allow write-passthrough. */
        {
            rc = hmR0VmxSaveGuestAutoLoadStoreMsrs(pVCpu, pMixedCtx);
            AssertRCReturn(rc, rc);
            pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_VMX_GUEST_AUTO_MSRS;
        }
        else if (pMixedCtx->ecx == MSR_IA32_TSC)        /* Windows 7 does this during bootup. See @bugref{6398}. */
            pVmxTransient->fUpdateTscOffsettingAndPreemptTimer = true;

        /* Update MSRs that are part of the VMCS when MSR-bitmaps are not supported. */
        if (!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_MSR_BITMAPS))
        {
            switch (pMixedCtx->ecx)
            {
                case MSR_IA32_SYSENTER_CS:  pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_SYSENTER_CS_MSR;  break;
                case MSR_IA32_SYSENTER_EIP: pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_SYSENTER_EIP_MSR; break;
                case MSR_IA32_SYSENTER_ESP: pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_SYSENTER_ESP_MSR; break;
                case MSR_K8_FS_BASE:        /* no break */
                case MSR_K8_GS_BASE:        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_SEGMENT_REGS;     break;
                case MSR_K8_KERNEL_GS_BASE: pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_VMX_GUEST_AUTO_MSRS;    break;
            }
        }
#ifdef VBOX_STRICT
        else
        {
            /* Paranoia. Validate that MSRs in the MSR-bitmaps with write-passthru are not intercepted. */
            switch (pMixedCtx->ecx)
            {
                case MSR_IA32_SYSENTER_CS:
                case MSR_IA32_SYSENTER_EIP:
                case MSR_IA32_SYSENTER_ESP:
                case MSR_K8_FS_BASE:
                case MSR_K8_GS_BASE:
                {
                    AssertMsgFailed(("Unexpected WRMSR for an MSR in the VMCS. ecx=%#RX32\n", pMixedCtx->ecx));
                    return VERR_VMX_UNEXPECTED_EXIT_CODE;
                }

                case MSR_K8_LSTAR:
                case MSR_K6_STAR:
                case MSR_K8_SF_MASK:
                case MSR_K8_TSC_AUX:
                case MSR_K8_KERNEL_GS_BASE:
                {
                    AssertMsgFailed(("Unexpected WRMSR for an MSR in the auto-load/store area in the VMCS. ecx=%#RX32\n",
                                     pMixedCtx->ecx));
                    return VERR_VMX_UNEXPECTED_EXIT_CODE;
                }
            }
        }
#endif  /* VBOX_STRICT */
    }
    return rc;
}


/**
 * VM-exit handler for PAUSE (VMX_EXIT_PAUSE). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitPause(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* By default, we don't enable VMX_VMCS_CTRL_PROC_EXEC_PAUSE_EXIT. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitPause);
    if (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_PAUSE_EXIT)
        return VERR_EM_INTERPRETER;
    AssertMsgFailed(("Unexpected PAUSE exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
    return VERR_VMX_UNEXPECTED_EXIT_CODE;
}


/**
 * VM-exit handler for when the TPR value is lowered below the specified
 * threshold (VMX_EXIT_TPR_BELOW_THRESHOLD). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitTprBelowThreshold(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW);

    /*
     * The TPR has already been updated, see hmR0VMXPostRunGuest(). RIP is also updated as part of the VM-exit by VT-x. Update
     * the threshold in the VMCS, deliver the pending interrupt via hmR0VmxPreRunGuest()->hmR0VmxInjectEvent() and
     * resume guest execution.
     */
    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_VMX_GUEST_APIC_STATE;
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
 *         recompiler.
 */
HMVMX_EXIT_DECL hmR0VmxExitMovCRx(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitMovCRx, y2);
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);

    const RTGCUINTPTR uExitQualification = pVmxTransient->uExitQualification;
    const uint32_t uAccessType           = VMX_EXIT_QUALIFICATION_CRX_ACCESS(uExitQualification);
    PVM pVM                              = pVCpu->CTX_SUFF(pVM);
    switch (uAccessType)
    {
        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_WRITE:       /* MOV to CRx */
        {
#if 0
            /* EMInterpretCRxWrite() references a lot of guest state (EFER, RFLAGS, Segment Registers, etc.) Sync entire state */
            rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
#else
            rc  = hmR0VmxSaveGuestRipRspRflags(pVCpu, pMixedCtx);
            rc |= hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
            rc |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
#endif
            AssertRCReturn(rc, rc);

            rc = EMInterpretCRxWrite(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx),
                                     VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification),
                                     VMX_EXIT_QUALIFICATION_CRX_GENREG(uExitQualification));
            Assert(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER || rc == VINF_PGM_CHANGE_MODE || rc == VINF_PGM_SYNC_CR3);

            switch (VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification))
            {
                case 0: /* CR0 */
                    Log4(("CRX CR0 write rc=%d CR0=%#RX64\n", rc, pMixedCtx->cr0));
                    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR0;
                    break;
                case 2: /* C2 **/
                    /* Nothing to do here, CR2 it's not part of the VMCS. */
                    break;
                case 3: /* CR3 */
                    Assert(!pVM->hm.s.fNestedPaging || !CPUMIsGuestPagingEnabledEx(pMixedCtx));
                    Log4(("CRX CR3 write rc=%d CR3=%#RX64\n", rc, pMixedCtx->cr3));
                    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR3;
                    break;
                case 4: /* CR4 */
                    Log4(("CRX CR4 write rc=%d CR4=%#RX64\n", rc, pMixedCtx->cr4));
                    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR4;
                    break;
                case 8: /* CR8 */
                    Assert(!(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW));
                    /* CR8 contains the APIC TPR. Was updated by EMInterpretCRxWrite(). */
                    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_VMX_GUEST_APIC_STATE;
                    break;
                default:
                    AssertMsgFailed(("Invalid CRx register %#x\n", VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification)));
                    break;
            }

            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCRxWrite[VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification)]);
            break;
        }

        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_READ:        /* MOV from CRx */
        {
            /* EMInterpretCRxRead() requires EFER MSR, CS. */
            rc  = hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
            AssertRCReturn(rc, rc);
            Assert(   !pVM->hm.s.fNestedPaging
                   || !CPUMIsGuestPagingEnabledEx(pMixedCtx)
                   || VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification) != 3);

            /* CR8 reads only cause a VM-exit when the TPR shadow feature isn't enabled. */
            Assert(   VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification) != 8
                   || !(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW));

            rc = EMInterpretCRxRead(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx),
                                    VMX_EXIT_QUALIFICATION_CRX_GENREG(uExitQualification),
                                    VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification));
            Assert(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCRxRead[VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification)]);
            Log4(("CRX CR%d Read access rc=%d\n", VMX_EXIT_QUALIFICATION_CRX_REGISTER(uExitQualification), rc));
            break;
        }

        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_CLTS:        /* CLTS (Clear Task-Switch Flag in CR0) */
        {
            rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
            AssertRCReturn(rc, rc);
            rc = EMInterpretCLTS(pVM, pVCpu);
            AssertRCReturn(rc, rc);
            pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR0;
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitClts);
            Log4(("CRX CLTS write rc=%d\n", rc));
            break;
        }

        case VMX_EXIT_QUALIFICATION_CRX_ACCESS_LMSW:        /* LMSW (Load Machine-Status Word into CR0) */
        {
            rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
            AssertRCReturn(rc, rc);
            rc = EMInterpretLMSW(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx), VMX_EXIT_QUALIFICATION_CRX_LMSW_DATA(uExitQualification));
            if (RT_LIKELY(rc == VINF_SUCCESS))
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR0;
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitLmsw);
            Log4(("CRX LMSW write rc=%d\n", rc));
            break;
        }

        default:
        {
            AssertMsgFailed(("Invalid access-type in Mov CRx exit qualification %#x\n", uAccessType));
            rc = VERR_VMX_UNEXPECTED_EXCEPTION;
        }
    }

    /* Validate possible error codes. */
    Assert(rc == VINF_SUCCESS || rc == VINF_PGM_CHANGE_MODE || rc == VERR_EM_INTERPRETER || rc == VINF_PGM_SYNC_CR3
           || rc == VERR_VMX_UNEXPECTED_EXCEPTION);
    if (RT_SUCCESS(rc))
    {
        int rc2 = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        AssertRCReturn(rc2, rc2);
    }

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitMovCRx, y2);
    return rc;
}


/**
 * VM-exit handler for I/O instructions (VMX_EXIT_IO_INSTR). Conditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitIoInstr(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatExitIO, y1);

    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxSaveGuestRip(pVCpu, pMixedCtx);
    rc    |= hmR0VmxSaveGuestRflags(pVCpu, pMixedCtx);         /* Eflag checks in EMInterpretDisasCurrent(). */
    rc    |= hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);    /* CR0 checks & PGM* in EMInterpretDisasCurrent(). */
    rc    |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);    /* SELM checks in EMInterpretDisasCurrent(). */
    /* EFER also required for longmode checks in EMInterpretDisasCurrent(), but it's always up-to-date. */
    AssertRCReturn(rc, rc);

    Log4(("CS:RIP=%04x:%#RX64\n", pMixedCtx->cs.Sel, pMixedCtx->rip));

    /* Refer Intel spec. 27-5. "Exit Qualifications for I/O Instructions" for the format. */
    uint32_t uIOPort   = VMX_EXIT_QUALIFICATION_IO_PORT(pVmxTransient->uExitQualification);
    uint32_t uIOWidth  = VMX_EXIT_QUALIFICATION_IO_WIDTH(pVmxTransient->uExitQualification);
    bool     fIOWrite  = (VMX_EXIT_QUALIFICATION_IO_DIRECTION(pVmxTransient->uExitQualification)
                          == VMX_EXIT_QUALIFICATION_IO_DIRECTION_OUT);
    bool     fIOString = (VMX_EXIT_QUALIFICATION_IO_STRING(pVmxTransient->uExitQualification) == 1);
    Assert(uIOWidth == 0 || uIOWidth == 1 || uIOWidth == 3);

    /* I/O operation lookup arrays. */
    static const uint32_t s_aIOSize[4]  = { 1, 2, 0, 4 };                   /* Size of the I/O accesses. */
    static const uint32_t s_aIOOpAnd[4] = { 0xff, 0xffff, 0, 0xffffffff };  /* AND masks for saving the result (in AL/AX/EAX). */

    const uint32_t cbSize  = s_aIOSize[uIOWidth];
    const uint32_t cbInstr = pVmxTransient->cbInstr;
    PVM pVM                = pVCpu->CTX_SUFF(pVM);
    if (fIOString)
    {
        /* INS/OUTS - I/O String instruction. */
        PDISCPUSTATE pDis = &pVCpu->hm.s.DisState;
        /** @todo for now manually disassemble later optimize by getting the fields from
         *        the VMCS. VMX_VMCS_RO_EXIT_GUEST_LINEAR_ADDR contains the flat pointer
         *        operand of the instruction. VMX_VMCS32_RO_EXIT_INSTR_INFO contains
         *        segment prefix info. */
        rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, NULL);
        if (RT_SUCCESS(rc))
        {
            if (fIOWrite)
            {
                VBOXSTRICTRC rc2 = IOMInterpretOUTSEx(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx), uIOPort, pDis->fPrefix,
                                                      (DISCPUMODE)pDis->uAddrMode, cbSize);
                rc = VBOXSTRICTRC_VAL(rc2);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOStringWrite);
            }
            else
            {
                VBOXSTRICTRC rc2 = IOMInterpretINSEx(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx), uIOPort, pDis->fPrefix,
                                                     (DISCPUMODE)pDis->uAddrMode, cbSize);
                rc = VBOXSTRICTRC_VAL(rc2);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOStringRead);
            }
        }
        else
        {
            AssertMsg(rc == VERR_EM_INTERPRETER, ("rc=%Rrc RIP %#RX64\n", rc, pMixedCtx->rip));
            rc = VINF_EM_RAW_EMULATE_INSTR;
        }
    }
    else
    {
        /* IN/OUT - I/O instruction. */
        const uint32_t uAndVal = s_aIOOpAnd[uIOWidth];
        Assert(!VMX_EXIT_QUALIFICATION_IO_REP(pVmxTransient->uExitQualification));
        if (fIOWrite)
        {
            VBOXSTRICTRC rc2 = IOMIOPortWrite(pVM, pVCpu, uIOPort, pMixedCtx->eax & uAndVal, cbSize);
            rc = VBOXSTRICTRC_VAL(rc2);
            if (rc == VINF_IOM_R3_IOPORT_WRITE)
                HMR0SavePendingIOPortWrite(pVCpu, pMixedCtx->rip, pMixedCtx->rip + cbInstr, uIOPort, uAndVal, cbSize);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIOWrite);
        }
        else
        {
            uint32_t u32Result = 0;
            VBOXSTRICTRC rc2 = IOMIOPortRead(pVM, pVCpu, uIOPort, &u32Result, cbSize);
            rc = VBOXSTRICTRC_VAL(rc2);
            if (IOM_SUCCESS(rc))
            {
                /* Save result of I/O IN instr. in AL/AX/EAX. */
                pMixedCtx->eax = (pMixedCtx->eax & ~uAndVal) | (u32Result & uAndVal);
            }
            else if (rc == VINF_IOM_R3_IOPORT_READ)
                HMR0SavePendingIOPortRead(pVCpu, pMixedCtx->rip, pMixedCtx->rip + cbInstr, uIOPort, uAndVal, cbSize);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIORead);
        }
    }

    if (IOM_SUCCESS(rc))
    {
        pMixedCtx->rip += cbInstr;
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP;
        if (RT_LIKELY(rc == VINF_SUCCESS))
        {
            rc = hmR0VmxSaveGuestDebugRegs(pVCpu, pMixedCtx);      /* For DR7. */
            AssertRCReturn(rc, rc);

            /* If any IO breakpoints are armed, then we should check if a debug trap needs to be generated. */
            if (pMixedCtx->dr[7] & X86_DR7_ENABLED_MASK)
            {
                STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxIoCheck);
                for (unsigned i = 0; i < 4; i++)
                {
                    uint32_t uBPLen = s_aIOSize[X86_DR7_GET_LEN(pMixedCtx->dr[7], i)];
                    if (   (   uIOPort >= pMixedCtx->dr[i]
                            && uIOPort < pMixedCtx->dr[i] + uBPLen)
                        && (pMixedCtx->dr[7] & (X86_DR7_L(i) | X86_DR7_G(i)))
                        && (pMixedCtx->dr[7] & X86_DR7_RW(i, X86_DR7_RW_IO)) == X86_DR7_RW(i, X86_DR7_RW_IO))
                    {
                        Assert(CPUMIsGuestDebugStateActive(pVCpu));
                        uint64_t uDR6 = ASMGetDR6();

                        /* Clear all breakpoint status flags and set the one we just hit. */
                        uDR6 &= ~(X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3);
                        uDR6 |= (uint64_t)RT_BIT(i);

                        /*
                         * Note: AMD64 Architecture Programmer's Manual 13.1:
                         * Bits 15:13 of the DR6 register is never cleared by the processor and must
                         * be cleared by software after the contents have been read.
                         */
                        ASMSetDR6(uDR6);

                        /* X86_DR7_GD will be cleared if DRx accesses should be trapped inside the guest. */
                        pMixedCtx->dr[7] &= ~X86_DR7_GD;

                        /* Paranoia. */
                        pMixedCtx->dr[7] &= 0xffffffff;                                             /* Upper 32 bits MBZ. */
                        pMixedCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));   /* MBZ. */
                        pMixedCtx->dr[7] |= 0x400;                                                  /* MB1. */

                        /* Resync DR7 */
                        /** @todo probably cheaper to just reload DR7, nothing else needs changing. */
                        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_DEBUG;

                        /* Set #DB to be injected into the VM and continue guest execution. */
                        hmR0VmxSetPendingXcptDB(pVCpu, pMixedCtx);
                        break;
                    }
                }
            }
        }
    }

#ifdef DEBUG
    if (rc == VINF_IOM_R3_IOPORT_READ)
        Assert(!fIOWrite);
    else if (rc == VINF_IOM_R3_IOPORT_WRITE)
        Assert(fIOWrite);
    else
    {
        AssertMsg(   RT_FAILURE(rc)
                  || rc == VINF_SUCCESS
                  || rc == VINF_EM_RAW_EMULATE_INSTR
                  || rc == VINF_EM_RAW_GUEST_TRAP
                  || rc == VINF_TRPM_XCPT_DISPATCHED, ("%Rrc\n", rc));
    }
#endif

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatExitIO, y1);
    return rc;
}


/**
 * VM-exit handler for task switches (VMX_EXIT_TASK_SWITCH). Unconditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitTaskSwitch(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* Check if this task-switch occurred while delivery an event through the guest IDT. */
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);
    if (VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE(pVmxTransient->uExitQualification) == VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE_IDT)
    {
        rc = hmR0VmxReadIdtVectoringInfoVmcs(pVmxTransient);
        AssertRCReturn(rc, rc);
        if (VMX_IDT_VECTORING_INFO_VALID(pVmxTransient->uIdtVectoringInfo))
        {
            uint32_t uIntType = VMX_IDT_VECTORING_INFO_TYPE(pVmxTransient->uIdtVectoringInfo);

            /* Software interrupts and exceptions will be regenerated when the recompiler restarts the instruction. */
            if (   uIntType != VMX_IDT_VECTORING_INFO_TYPE_SW_INT
                && uIntType != VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT
                && uIntType != VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT)
            {
                uint32_t uVector     = VMX_IDT_VECTORING_INFO_VECTOR(pVmxTransient->uIdtVectoringInfo);
                bool fErrorCodeValid = !!VMX_IDT_VECTORING_INFO_ERROR_CODE_IS_VALID(pVmxTransient->uIdtVectoringInfo);

                /* Save it as a pending event and it'll be converted to a TRPM event on the way out to ring-3. */
                Assert(!pVCpu->hm.s.Event.fPending);
                pVCpu->hm.s.Event.fPending = true;
                pVCpu->hm.s.Event.u64IntrInfo = pVmxTransient->uIdtVectoringInfo;
                rc = hmR0VmxReadIdtVectoringErrorCodeVmcs(pVmxTransient);
                AssertRCReturn(rc, rc);
                if (fErrorCodeValid)
                    pVCpu->hm.s.Event.u32ErrCode = pVmxTransient->uIdtVectoringErrorCode;
                else
                    pVCpu->hm.s.Event.u32ErrCode = 0;
                if (   uIntType == VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT
                    && uVector == X86_XCPT_PF)
                {
                    pVCpu->hm.s.Event.GCPtrFaultAddress = pMixedCtx->cr2;
                }

                Log4(("Pending event on TaskSwitch uIntType=%#x uVector=%#x\n", uIntType, uVector));
            }
        }
    }

    /** @todo Emulate task switch someday, currently just going back to ring-3 for
     *        emulation. */
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitTaskSwitch);
    return VERR_EM_INTERPRETER;
}


/**
 * VM-exit handler for monitor-trap-flag (VMX_EXIT_MTF). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMtf(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    Assert(pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG);
    pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_MONITOR_TRAP_FLAG;
    int rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
    AssertRCReturn(rc, rc);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitMtf);
    return VINF_EM_DBG_STOP;
}


/**
 * VM-exit handler for APIC access (VMX_EXIT_APIC_ACCESS). Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitApicAccess(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    int rc = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pMixedCtx, pVmxTransient);
    if (RT_UNLIKELY(rc == VINF_HM_DOUBLE_FAULT))
        return VINF_SUCCESS;
    else if (RT_UNLIKELY(rc == VINF_EM_RESET))
        return rc;

#if 0
    /** @todo Investigate if IOMMMIOPhysHandler() requires a lot of state, for now
     *   just sync the whole thing. */
    rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
#else
    /* Aggressive state sync. for now. */
    rc  = hmR0VmxSaveGuestRipRspRflags(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
#endif
    rc |= hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);

    /* See Intel spec. 27-6 "Exit Qualifications for APIC-access VM-exits from Linear Accesses & Guest-Phyiscal Addresses" */
    uint32_t uAccessType = VMX_EXIT_QUALIFICATION_APIC_ACCESS_TYPE(pVmxTransient->uExitQualification);
    switch (uAccessType)
    {
        case VMX_APIC_ACCESS_TYPE_LINEAR_WRITE:
        case VMX_APIC_ACCESS_TYPE_LINEAR_READ:
        {
            if (  (pVCpu->hm.s.vmx.u32ProcCtls & VMX_VMCS_CTRL_PROC_EXEC_USE_TPR_SHADOW)
                && VMX_EXIT_QUALIFICATION_APIC_ACCESS_OFFSET(pVmxTransient->uExitQualification) == 0x80)
            {
                AssertMsgFailed(("hmR0VmxExitApicAccess: can't access TPR offset while using TPR shadowing.\n"));
            }

            RTGCPHYS GCPhys = pMixedCtx->msrApicBase;   /* Always up-to-date, msrApicBase is not part of the VMCS. */
            GCPhys &= PAGE_BASE_GC_MASK;
            GCPhys += VMX_EXIT_QUALIFICATION_APIC_ACCESS_OFFSET(pVmxTransient->uExitQualification);
            PVM pVM = pVCpu->CTX_SUFF(pVM);
            Log4(("ApicAccess uAccessType=%#x GCPhys=%#RGv Off=%#x\n", uAccessType, GCPhys,
                 VMX_EXIT_QUALIFICATION_APIC_ACCESS_OFFSET(pVmxTransient->uExitQualification)));

            VBOXSTRICTRC rc2 = IOMMMIOPhysHandler(pVM, pVCpu,
                                                  (uAccessType == VMX_APIC_ACCESS_TYPE_LINEAR_READ) ? 0 : X86_TRAP_PF_RW,
                                                  CPUMCTX2CORE(pMixedCtx), GCPhys);
            rc = VBOXSTRICTRC_VAL(rc2);
            Log4(("ApicAccess rc=%d\n", rc));
            if (   rc == VINF_SUCCESS
                || rc == VERR_PAGE_TABLE_NOT_PRESENT
                || rc == VERR_PAGE_NOT_PRESENT)
            {
                pVCpu->hm.s.fContextUseFlags |=   HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS
                                                | HM_CHANGED_VMX_GUEST_APIC_STATE;
                rc = VINF_SUCCESS;
            }
            break;
        }

        default:
            Log4(("ApicAccess uAccessType=%#x\n", uAccessType));
            rc = VINF_EM_RAW_EMULATE_INSTR;
            break;
    }

    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitApicAccess);
    return rc;
}


/**
 * VM-exit handler for debug-register accesses (VMX_EXIT_MOV_DRX). Conditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitMovDRx(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();

    /* We should -not- get this VM-exit if the guest is debugging. */
    if (CPUMIsGuestDebugStateActive(pVCpu))
    {
        AssertMsgFailed(("Unexpected MOV DRx exit. pVCpu=%p pMixedCtx=%p\n", pVCpu, pMixedCtx));
        return VERR_VMX_UNEXPECTED_EXIT_CODE;
    }

    int rc = VERR_INTERNAL_ERROR_5;
    if (   !DBGFIsStepping(pVCpu)
        && !CPUMIsHyperDebugStateActive(pVCpu))
    {
        /* Don't intercept MOV DRx. */
        pVCpu->hm.s.vmx.u32ProcCtls &= ~VMX_VMCS_CTRL_PROC_EXEC_MOV_DR_EXIT;
        rc = VMXWriteVmcs32(VMX_VMCS32_CTRL_PROC_EXEC, pVCpu->hm.s.vmx.u32ProcCtls);
        AssertRCReturn(rc, rc);

        /* Save the host & load the guest debug state, restart execution of the MOV DRx instruction. */
        PVM pVM = pVCpu->CTX_SUFF(pVM);
        rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pMixedCtx, true /* include DR6 */);
        AssertRC(rc);
        Assert(CPUMIsGuestDebugStateActive(pVCpu));

#ifdef VBOX_WITH_STATISTICS
        rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
        AssertRCReturn(rc, rc);
        if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(pVmxTransient->uExitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxWrite);
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxRead);
#endif
        STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxContextSwitch);
        return VINF_SUCCESS;
    }

    /*
     * EMInterpretDRx[Write|Read]() calls CPUMIsGuestIn64BitCode() which requires EFER, CS. EFER is always up-to-date, see
     * hmR0VmxSaveGuestAutoLoadStoreMsrs(). Update only the segment registers from the CPU.
     */
    rc  = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (VMX_EXIT_QUALIFICATION_DRX_DIRECTION(pVmxTransient->uExitQualification) == VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE)
    {
        rc = EMInterpretDRxWrite(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx),
                                 VMX_EXIT_QUALIFICATION_DRX_REGISTER(pVmxTransient->uExitQualification),
                                 VMX_EXIT_QUALIFICATION_DRX_GENREG(pVmxTransient->uExitQualification));
        if (RT_SUCCESS(rc))
            pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_DEBUG;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxWrite);
    }
    else
    {
        rc = EMInterpretDRxRead(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx),
                                VMX_EXIT_QUALIFICATION_DRX_GENREG(pVmxTransient->uExitQualification),
                                VMX_EXIT_QUALIFICATION_DRX_REGISTER(pVmxTransient->uExitQualification));
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitDRxRead);
    }

    Assert(rc == VINF_SUCCESS || rc == VERR_EM_INTERPRETER);
    if (RT_SUCCESS(rc))
    {
        int rc2 = hmR0VmxAdvanceGuestRip(pVCpu, pMixedCtx, pVmxTransient);
        AssertRCReturn(rc2, rc2);
    }
    return rc;
}


/**
 * VM-exit handler for EPT misconfiguration (VMX_EXIT_EPT_MISCONFIG).
 * Conditional VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitEptMisconfig(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    int rc = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pMixedCtx, pVmxTransient);
    if (RT_UNLIKELY(rc == VINF_HM_DOUBLE_FAULT))
        return VINF_SUCCESS;
    else if (RT_UNLIKELY(rc == VINF_EM_RESET))
        return rc;

    RTGCPHYS GCPhys = 0;
    rc = VMXReadVmcs64(VMX_VMCS64_EXIT_GUEST_PHYS_ADDR_FULL, &GCPhys);

#if 0
    rc |= hmR0VmxSaveGuestState(pVCpu, pMixedCtx);     /** @todo Can we do better?  */
#else
    /* Aggressive state sync. for now. */
    rc |= hmR0VmxSaveGuestRipRspRflags(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
#endif
    AssertRCReturn(rc, rc);

    /*
     * If we succeed, resume guest execution.
     * If we fail in interpreting the instruction because we couldn't get the guest physical address
     * of the page containing the instruction via the guest's page tables (we would invalidate the guest page
     * in the host TLB), resume execution which would cause a guest page fault to let the guest handle this
     * weird case. See @bugref{6043}.
     */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VBOXSTRICTRC rc2 = PGMR0Trap0eHandlerNPMisconfig(pVM, pVCpu, PGMMODE_EPT, CPUMCTX2CORE(pMixedCtx), GCPhys, UINT32_MAX);
    rc = VBOXSTRICTRC_VAL(rc2);
    Log4(("EPT misconfig at %#RGv RIP=%#RX64 rc=%d\n", GCPhys, pMixedCtx->rip, rc));
    if (   rc == VINF_SUCCESS
        || rc == VERR_PAGE_TABLE_NOT_PRESENT
        || rc == VERR_PAGE_NOT_PRESENT)
    {
        /* Successfully handled MMIO operation. */
        pVCpu->hm.s.fContextUseFlags |=   HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS
                                        | HM_CHANGED_VMX_GUEST_APIC_STATE;
        rc = VINF_SUCCESS;
    }
    return rc;
}


/**
 * VM-exit handler for EPT violation (VMX_EXIT_EPT_VIOLATION). Conditional
 * VM-exit.
 */
HMVMX_EXIT_DECL hmR0VmxExitEptViolation(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_HANDLER_PARAMS();
    Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fNestedPaging);

    /* If this VM-exit occurred while delivering an event through the guest IDT, handle it accordingly. */
    int rc = hmR0VmxCheckExitDueToEventDelivery(pVCpu, pMixedCtx, pVmxTransient);
    if (RT_UNLIKELY(rc == VINF_HM_DOUBLE_FAULT))
        return VINF_SUCCESS;
    else if (RT_UNLIKELY(rc == VINF_EM_RESET))
        return rc;

    RTGCPHYS GCPhys = 0;
    rc  = VMXReadVmcs64(VMX_VMCS64_EXIT_GUEST_PHYS_ADDR_FULL, &GCPhys);
    rc |= hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
#if 0
    rc |= hmR0VmxSaveGuestState(pVCpu, pMixedCtx);     /** @todo Can we do better?  */
#else
    /* Aggressive state sync. for now. */
    rc |= hmR0VmxSaveGuestRipRspRflags(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestControlRegs(pVCpu, pMixedCtx);
    rc |= hmR0VmxSaveGuestSegmentRegs(pVCpu, pMixedCtx);
#endif
    AssertRCReturn(rc, rc);

    /* Intel spec. Table 27-7 "Exit Qualifications for EPT violations". */
    AssertMsg(((pVmxTransient->uExitQualification >> 7) & 3) != 2, ("%#RX64", pVmxTransient->uExitQualification));

    RTGCUINT uErrorCode = 0;
    if (pVmxTransient->uExitQualification & VMX_EXIT_QUALIFICATION_EPT_INSTR_FETCH)
        uErrorCode |= X86_TRAP_PF_ID;
    if (pVmxTransient->uExitQualification & VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE)
        uErrorCode |= X86_TRAP_PF_RW;
    if (pVmxTransient->uExitQualification & VMX_EXIT_QUALIFICATION_EPT_ENTRY_PRESENT)
        uErrorCode |= X86_TRAP_PF_P;

    TRPMAssertXcptPF(pVCpu, GCPhys, uErrorCode);

    Log4(("EPT violation %#x at %#RX64 ErrorCode %#x CS:EIP=%04x:%#RX64\n", pVmxTransient->uExitQualification, GCPhys,
         uErrorCode, pMixedCtx->cs.Sel, pMixedCtx->rip));

    /* Handle the pagefault trap for the nested shadow table. */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = PGMR0Trap0eHandlerNestedPaging(pVM, pVCpu, PGMMODE_EPT, uErrorCode, CPUMCTX2CORE(pMixedCtx), GCPhys);
    TRPMResetTrap(pVCpu);

    /* Same case as PGMR0Trap0eHandlerNPMisconfig(). See comment above, @bugref{6043}. */
    if (   rc == VINF_SUCCESS
        || rc == VERR_PAGE_TABLE_NOT_PRESENT
        || rc == VERR_PAGE_NOT_PRESENT)
    {
        /* Successfully synced our nested page tables. */
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitReasonNpf);
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS;
        return VINF_SUCCESS;
    }

    Log4(("EPT return to ring-3 rc=%d\n"));
    return rc;
}


/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/* -=-=-=-=-=-=-=-=-=- VM-exit Exception Handlers -=-=-=-=-=-=-=-=-=-=- */
/* -=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-=-=-=-=-=-= */
/**
 * VM-exit exception handler for #MF (Math Fault: floating point exception).
 */
static int hmR0VmxExitXcptMF(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestMF);

    int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    if (!(pMixedCtx->cr0 & X86_CR0_NE))
    {
        /* Old-style FPU error reporting needs some extra work. */
        /** @todo don't fall back to the recompiler, but do it manually. */
        return VERR_EM_INTERPRETER;
    }
    hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                           pVmxTransient->cbInstr, pVmxTransient->uExitIntrErrorCode, 0 /* GCPtrFaultAddress */);
    return rc;
}


/**
 * VM-exit exception handler for #BP (Breakpoint exception).
 */
static int hmR0VmxExitXcptBP(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestBP);

    /** @todo Try optimize this by not saving the entire guest state unless
     *        really needed. */
    int rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = DBGFRZTrap03Handler(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx));
    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        rc  = hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
        rc |= hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
        rc |= hmR0VmxReadExitIntrErrorCodeVmcs(pVCpu, pVmxTransient);
        AssertRCReturn(rc, rc);

        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                               pVmxTransient->cbInstr, pVmxTransient->uExitIntrErrorCode, 0 /* GCPtrFaultAddress */);
    }

    Assert(rc == VINF_SUCCESS || rc == VINF_EM_RAW_GUEST_TRAP || rc == VINF_EM_DBG_BREAKPOINT);
    return rc;
}


/**
 * VM-exit exception handler for #DB (Debug exception).
 */
static int hmR0VmxExitXcptDB(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestDB);

    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);

    /* Refer Intel spec. Table 27-1. "Exit Qualifications for debug exceptions" for the format. */
    uint64_t uDR6 = X86_DR6_INIT_VAL;
    uDR6         |= (pVmxTransient->uExitQualification
                     & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3 | X86_DR6_BD | X86_DR6_BS));
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = DBGFRZTrap01Handler(pVM, pVCpu, CPUMCTX2CORE(pMixedCtx), uDR6);
    if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        /* DR6, DR7.GD and IA32_DEBUGCTL.LBR are not updated yet. See Intel spec. 27.1 "Architectural State before a VM-Exit". */
        pMixedCtx->dr[6] = uDR6;

        if (CPUMIsGuestDebugStateActive(pVCpu))
            ASMSetDR6(pMixedCtx->dr[6]);

        rc = hmR0VmxSaveGuestDebugRegs(pVCpu, pMixedCtx);

        /* X86_DR7_GD will be cleared if DRx accesses should be trapped inside the guest. */
        pMixedCtx->dr[7] &= ~X86_DR7_GD;

        /* Paranoia. */
        pMixedCtx->dr[7] &= 0xffffffff;                                              /* Upper 32 bits MBZ. */
        pMixedCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));    /* MBZ. */
        pMixedCtx->dr[7] |= 0x400;                                                   /* MB1. */

        rc |= VMXWriteVmcs32(VMX_VMCS_GUEST_DR7, (uint32_t)pMixedCtx->dr[7]);
        AssertRCReturn(rc,rc);

        int rc2 = hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
        rc2 |= hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
        rc2 |= hmR0VmxReadExitIntrErrorCodeVmcs(pVCpu, pVmxTransient);
        AssertRCReturn(rc2, rc2);
        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                               pVmxTransient->cbInstr, pVmxTransient->uExitIntrErrorCode, 0 /* GCPtrFaultAddress */);
        rc = VINF_SUCCESS;
    }

    return rc;
}


/**
 * VM-exit exception handler for #NM (Device-not-available exception: floating
 * point exception).
 */
static int hmR0VmxExitXcptNM(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();

#ifndef HMVMX_ALWAYS_TRAP_ALL_XCPTS
    Assert(!CPUMIsGuestFPUStateActive(pVCpu));
#endif

    /* We require CR0 and EFER. EFER is always up-to-date. */
    int rc = hmR0VmxSaveGuestCR0(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    /* Lazy FPU loading; load the guest-FPU state transparently and continue execution of the guest. */
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    rc = CPUMR0LoadGuestFPU(pVM, pVCpu, pMixedCtx);
    if (rc == VINF_SUCCESS)
    {
        Assert(CPUMIsGuestFPUStateActive(pVCpu));
        pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_CR0;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowNM);
        return VINF_SUCCESS;
    }

    /* Forward #NM to the guest. */
    Assert(rc == VINF_EM_RAW_GUEST_TRAP);
    rc = hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);
    hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                           pVmxTransient->cbInstr, 0 /* error code */, 0 /* GCPtrFaultAddress */);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestNM);
    return rc;
}


/**
 * VM-exit exception handler for #GP (General-protection exception).
 *
 * @remarks Requires pVmxTransient->uExitIntrInfo to be up-to-date.
 */
static int hmR0VmxExitXcptGP(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestGP);

    int rc = VERR_INTERNAL_ERROR_5;
    if (!pVCpu->hm.s.vmx.RealMode.fRealOnV86Active)
    {
#ifdef HMVMX_ALWAYS_TRAP_ALL_XCPTS
        /* If the guest is not in real-mode or we have unrestricted execution support, reflect #GP to the guest. */
        rc  = hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
        rc |= hmR0VmxReadExitIntrErrorCodeVmcs(pVCpu, pVmxTransient);
        rc |= hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
        rc |= hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
        AssertRCReturn(rc, rc);
        Log4(("#GP Gst: RIP %#RX64 ErrorCode=%#x CR0=%#RX64 CPL=%u\n", pMixedCtx->rip, pVmxTransient->uExitIntrErrorCode,
             pMixedCtx->cr0, CPUMGetGuestCPL(pVCpu)));
        hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                               pVmxTransient->cbInstr, pVmxTransient->uExitIntrErrorCode, 0 /* GCPtrFaultAddress */);
        return rc;
#else
        /* We don't intercept #GP. */
        AssertMsgFailed(("Unexpected VM-exit caused by #GP exception\n"));
        return VERR_VMX_UNEXPECTED_EXCEPTION;
#endif
    }

    Assert(CPUMIsGuestInRealModeEx(pMixedCtx));
    Assert(!pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fUnrestrictedGuest);

    /* EMInterpretDisasCurrent() requires a lot of the state, save the entire state. */
    rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    PDISCPUSTATE pDis = &pVCpu->hm.s.DisState;
    uint32_t cbOp     = 0;
    PVM pVM           = pVCpu->CTX_SUFF(pVM);
    rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
    if (RT_SUCCESS(rc))
    {
        rc = VINF_SUCCESS;
        Assert(cbOp == pDis->cbInstr);
        Log4(("#GP Disas OpCode=%u CS:EIP %04x:%#RX64\n", pDis->pCurInstr->uOpcode, pMixedCtx->cs.Sel, pMixedCtx->rip));
        switch (pDis->pCurInstr->uOpcode)
        {
            case OP_CLI:
            {
                pMixedCtx->eflags.Bits.u1IF = 0;
                pMixedCtx->rip += pDis->cbInstr;
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS;
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitCli);
                break;
            }

            case OP_STI:
            {
                pMixedCtx->eflags.Bits.u1IF = 1;
                pMixedCtx->rip += pDis->cbInstr;
                EMSetInhibitInterruptsPC(pVCpu, pMixedCtx->rip);
                Assert(VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RFLAGS;
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitSti);
                break;
            }

            case OP_HLT:
            {
                rc = VINF_EM_HALT;
                pMixedCtx->rip += pDis->cbInstr;
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP;
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitHlt);
                break;
            }

            case OP_POPF:
            {
                Log4(("POPF CS:RIP %04x:%#RX64\n", pMixedCtx->cs.Sel, pMixedCtx->rip));
                uint32_t cbParm = 0;
                uint32_t uMask  = 0;
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

                /* Get the stack pointer & pop the contents of the stack onto EFlags. */
                RTGCPTR   GCPtrStack = 0;
                X86EFLAGS uEflags;
                rc = SELMToFlatEx(pVCpu, DISSELREG_SS, CPUMCTX2CORE(pMixedCtx), pMixedCtx->esp & uMask, SELMTOFLAT_FLAGS_CPL0,
                                  &GCPtrStack);
                if (RT_SUCCESS(rc))
                {
                    Assert(sizeof(uEflags.u32) >= cbParm);
                    uEflags.u32 = 0;
                    rc = PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &uEflags.u32, cbParm);
                }
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                Log4(("POPF %x -> %#RX64 mask=%x RIP=%#RX64\n", uEflags.u, pMixedCtx->rsp, uMask, pMixedCtx->rip));
                pMixedCtx->eflags.u32 =   (pMixedCtx->eflags.u32 & ~(X86_EFL_POPF_BITS & uMask))
                                        | (uEflags.u32 & X86_EFL_POPF_BITS & uMask);
                /* The RF bit is always cleared by POPF; see Intel Instruction reference for POPF. */
                pMixedCtx->eflags.Bits.u1RF   = 0;
                pMixedCtx->esp               += cbParm;
                pMixedCtx->esp               &= uMask;
                pMixedCtx->rip               += pDis->cbInstr;
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS;
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitPopf);
                break;
            }

            case OP_PUSHF:
            {
                uint32_t cbParm = 0;
                uint32_t uMask  = 0;
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
                rc = SELMToFlatEx(pVCpu, DISSELREG_SS, CPUMCTX2CORE(pMixedCtx), (pMixedCtx->esp - cbParm) & uMask,
                                  SELMTOFLAT_FLAGS_CPL0, &GCPtrStack);
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                X86EFLAGS uEflags;
                uEflags = pMixedCtx->eflags;
                /* The RF & VM bits are cleared on image stored on stack; see Intel Instruction reference for PUSHF. */
                uEflags.Bits.u1RF = 0;
                uEflags.Bits.u1VM = 0;

                rc = PGMPhysWrite(pVM, (RTGCPHYS)GCPtrStack, &uEflags.u, cbParm);
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                Log4(("PUSHF %x -> %#RGv\n", uEflags.u, GCPtrStack));
                pMixedCtx->esp               -= cbParm;
                pMixedCtx->esp               &= uMask;
                pMixedCtx->rip               += pDis->cbInstr;
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP;
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitPushf);
                break;
            }

            case OP_IRET:
            {
                /** @todo Handle 32-bit operand sizes and check stack limits. See Intel
                 *        instruction reference. */
                RTGCPTR  GCPtrStack = 0;
                uint32_t uMask      = 0xffff;
                uint16_t aIretFrame[3];
                if (pDis->fPrefix & (DISPREFIX_OPSIZE | DISPREFIX_ADDRSIZE))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                rc = SELMToFlatEx(pVCpu, DISSELREG_SS, CPUMCTX2CORE(pMixedCtx), pMixedCtx->esp & uMask, SELMTOFLAT_FLAGS_CPL0,
                                  &GCPtrStack);
                if (RT_SUCCESS(rc))
                    rc = PGMPhysRead(pVM, (RTGCPHYS)GCPtrStack, &aIretFrame[0], sizeof(aIretFrame));
                if (RT_FAILURE(rc))
                {
                    rc = VERR_EM_INTERPRETER;
                    break;
                }
                pMixedCtx->eip                = 0;
                pMixedCtx->ip                 = aIretFrame[0];
                pMixedCtx->cs.Sel             = aIretFrame[1];
                pMixedCtx->cs.ValidSel        = aIretFrame[1];
                pMixedCtx->cs.u64Base         = (uint64_t)pMixedCtx->cs.Sel << 4;
                pMixedCtx->eflags.u32         = (pMixedCtx->eflags.u32 & ~(X86_EFL_POPF_BITS & uMask))
                                                | (aIretFrame[2] & X86_EFL_POPF_BITS & uMask);
                pMixedCtx->sp                += sizeof(aIretFrame);
                pVCpu->hm.s.fContextUseFlags |=   HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_SEGMENT_REGS | HM_CHANGED_GUEST_RSP
                                                | HM_CHANGED_GUEST_RFLAGS;
                Log4(("IRET %#RX32 to %04x:%x\n", GCPtrStack, pMixedCtx->cs.Sel, pMixedCtx->ip));
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitIret);
                break;
            }

            case OP_INT:
            {
                uint16_t uVector = pDis->Param1.uValue & 0xff;
                hmR0VmxSetPendingIntN(pVCpu, pMixedCtx, uVector, pDis->cbInstr);
                STAM_COUNTER_INC(&pVCpu->hm.s.StatExitInt);
                break;
            }

            case OP_INTO:
            {
                if (pMixedCtx->eflags.Bits.u1OF)
                {
                    hmR0VmxSetPendingXcptOF(pVCpu, pMixedCtx, pDis->cbInstr);
                    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitInt);
                }
                break;
            }

            default:
            {
                VBOXSTRICTRC rc2 = EMInterpretInstructionDisasState(pVCpu, pDis, CPUMCTX2CORE(pMixedCtx), 0 /* pvFault */,
                                                                    EMCODETYPE_SUPERVISOR);
                rc = VBOXSTRICTRC_VAL(rc2);
                pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_ALL_GUEST;
                Log4(("#GP rc=%Rrc\n", rc));
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
 * @remarks Requires uExitIntrInfo in the VMX transient structure to be
 *          up-to-date.
 */
static int hmR0VmxExitXcptGeneric(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();

    /* Re-inject the exception into the guest. This cannot be a double-fault condition which would have been handled in
       hmR0VmxCheckExitDueToEventDelivery(). */
    int rc =  hmR0VmxReadExitIntrErrorCodeVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitInstrLenVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);
    Assert(pVmxTransient->fVmcsFieldsRead & HMVMX_UPDATED_TRANSIENT_EXIT_INTERRUPTION_INFO);
    hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                           pVmxTransient->cbInstr, pVmxTransient->uExitIntrErrorCode, 0 /* GCPtrFaultAddress */);
    return VINF_SUCCESS;
}


/**
 * VM-exit exception handler for #PF (Page-fault exception).
 */
static int hmR0VmxExitXcptPF(PVMCPU pVCpu, PCPUMCTX pMixedCtx, PVMXTRANSIENT pVmxTransient)
{
    HMVMX_VALIDATE_EXIT_XCPT_HANDLER_PARAMS();
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    int rc = hmR0VmxReadExitQualificationVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitIntrInfoVmcs(pVCpu, pVmxTransient);
    rc    |= hmR0VmxReadExitIntrErrorCodeVmcs(pVCpu, pVmxTransient);
    AssertRCReturn(rc, rc);

#if defined(HMVMX_ALWAYS_TRAP_ALL_XCPTS) || defined(HMVMX_ALWAYS_TRAP_PF)
    if (pVM->hm.s.fNestedPaging)
    {
        pVCpu->hm.s.Event.fPending = false;                  /* In case it's a contributory or vectoring #PF. */
        if (RT_LIKELY(!pVmxTransient->fVectoringPF))
        {
            pMixedCtx->cr2 = pVmxTransient->uExitQualification;  /* Update here in case we go back to ring-3 before injection. */
            hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                                   0 /* cbInstr */, pVmxTransient->uExitIntrErrorCode, pVmxTransient->uExitQualification);
        }
        else
        {
            /* A guest page-fault occurred during delivery of a page-fault. Inject #DF. */
            hmR0VmxSetPendingXcptDF(pVCpu, pMixedCtx);
            Log4(("Pending #DF due to vectoring #PF. NP\n"));
        }
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestPF);
        return rc;
    }
#else
    Assert(!pVM->hm.s.fNestedPaging);
#endif

    rc = hmR0VmxSaveGuestState(pVCpu, pMixedCtx);
    AssertRCReturn(rc, rc);

    Log4(("#PF: cr2=%#RX64 cs:rip=%#04x:%#RX64 uErrCode %#RX32 cr3=%#RX64\n", pVmxTransient->uExitQualification,
          pMixedCtx->cs.Sel, pMixedCtx->rip, pVmxTransient->uExitIntrErrorCode, pMixedCtx->cr3));

    TRPMAssertXcptPF(pVCpu, pVmxTransient->uExitQualification, (RTGCUINT)pVmxTransient->uExitIntrErrorCode);
    rc = PGMTrap0eHandler(pVCpu, pVmxTransient->uExitIntrErrorCode, CPUMCTX2CORE(pMixedCtx),
                          (RTGCPTR)pVmxTransient->uExitQualification);

    Log4(("#PF: rc=%Rrc\n", rc));
    if (rc == VINF_SUCCESS)
    {
        /* Successfully synced shadow pages tables or emulated an MMIO instruction. */
        /** @todo this isn't quite right, what if guest does lgdt with some MMIO
         *        memory? We don't update the whole state here... */
        pVCpu->hm.s.fContextUseFlags |=   HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS
                                        | HM_CHANGED_VMX_GUEST_APIC_STATE;
        TRPMResetTrap(pVCpu);
        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPF);
        return rc;
    }
    else if (rc == VINF_EM_RAW_GUEST_TRAP)
    {
        if (!pVmxTransient->fVectoringPF)
        {
            /* It's a guest page fault and needs to be reflected to the guest. */
            uint32_t uGstErrorCode = TRPMGetErrorCode(pVCpu);
            TRPMResetTrap(pVCpu);
            pVCpu->hm.s.Event.fPending = false;                 /* In case it's a contributory #PF. */
            pMixedCtx->cr2 = pVmxTransient->uExitQualification; /* Update here in case we go back to ring-3 before injection. */
            hmR0VmxSetPendingEvent(pVCpu, VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(pVmxTransient->uExitIntrInfo),
                                   0 /* cbInstr */, uGstErrorCode, pVmxTransient->uExitQualification);
        }
        else
        {
            /* A guest page-fault occurred during delivery of a page-fault. Inject #DF. */
            TRPMResetTrap(pVCpu);
            pVCpu->hm.s.Event.fPending = false;     /* Clear pending #PF to replace it with #DF. */
            hmR0VmxSetPendingXcptDF(pVCpu, pMixedCtx);
            Log4(("#PF: Pending #DF due to vectoring #PF\n"));
        }

        STAM_COUNTER_INC(&pVCpu->hm.s.StatExitGuestPF);
        return VINF_SUCCESS;
    }

    TRPMResetTrap(pVCpu);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatExitShadowPFEM);
    return rc;
}

