/** @file
 * HM - Intel/AMD VM Hardware Assisted Virtualization Manager (VMM)
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_vmm_hm_h
#define VBOX_INCLUDED_vmm_hm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/trpm.h>
#include <iprt/mp.h>


/** @defgroup grp_hm      The Hardware Assisted Virtualization Manager API
 * @ingroup grp_vmm
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * Checks whether HM (VT-x/AMD-V) is being used by this VM.
 *
 * @retval  true if used.
 * @retval  false if software virtualization (raw-mode) or NEM is used.
 *
 * @param   a_pVM       The cross context VM structure.
 * @deprecated Please use VM_IS_RAW_MODE_ENABLED, VM_IS_HM_OR_NEM_ENABLED, or
 *             VM_IS_HM_ENABLED instead.
 * @internal
 */
#if defined(VBOX_STRICT) && defined(IN_RING3)
# define HMIsEnabled(a_pVM)                 HMIsEnabledNotMacro(a_pVM)
#else
# define HMIsEnabled(a_pVM)                 ((a_pVM)->fHMEnabled)
#endif

/**
 * Checks whether raw-mode context is required for HM purposes
 *
 * @retval  true if required by HM for doing switching the cpu to 64-bit mode.
 * @retval  false if not required by HM.
 *
 * @param   a_pVM       The cross context VM structure.
 * @internal
 */
#if HC_ARCH_BITS == 64
# define HMIsRawModeCtxNeeded(a_pVM)        (false)
#else
# define HMIsRawModeCtxNeeded(a_pVM)        ((a_pVM)->fHMNeedRawModeCtx)
#endif

/**
 * Checks whether we're in the special hardware virtualization context.
 * @returns true / false.
 * @param   a_pVCpu     The caller's cross context virtual CPU structure.
 * @thread  EMT
 */
#ifdef IN_RING0
# define HMIsInHwVirtCtx(a_pVCpu)           (VMCPU_GET_STATE(a_pVCpu) == VMCPUSTATE_STARTED_HM)
#else
# define HMIsInHwVirtCtx(a_pVCpu)           (false)
#endif

/**
 * Checks whether we're in the special hardware virtualization context and we
 * cannot perform long jump without guru meditating and possibly messing up the
 * host and/or guest state.
 *
 * This is after we've turned interrupts off and such.
 *
 * @returns true / false.
 * @param   a_pVCpu     The caller's cross context virtual CPU structure.
 * @thread  EMT
 */
#ifdef IN_RING0
# define HMIsInHwVirtNoLongJmpCtx(a_pVCpu)  (VMCPU_GET_STATE(a_pVCpu) == VMCPUSTATE_STARTED_EXEC)
#else
# define HMIsInHwVirtNoLongJmpCtx(a_pVCpu)  (false)
#endif

/** @name All-context HM API.
 * @{ */
VMMDECL(bool)                   HMIsEnabledNotMacro(PVM pVM);
VMMDECL(bool)                   HMCanExecuteGuest(PVMCC pVM, PVMCPUCC pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(int)               HMInvalidatePage(PVMCPUCC pVCpu, RTGCPTR GCVirt);
VMM_INT_DECL(bool)              HMHasPendingIrq(PVMCC pVM);
VMM_INT_DECL(PX86PDPE)          HMGetPaePdpes(PVMCPU pVCpu);
VMM_INT_DECL(bool)              HMSetSingleInstruction(PVMCC pVM, PVMCPUCC pVCpu, bool fEnable);
VMM_INT_DECL(bool)              HMIsSvmActive(PVM pVM);
VMM_INT_DECL(bool)              HMIsVmxActive(PVM pVM);
VMM_INT_DECL(const char *)      HMGetVmxDiagDesc(VMXVDIAG enmDiag);
VMM_INT_DECL(const char *)      HMGetVmxExitName(uint32_t uExit);
VMM_INT_DECL(const char *)      HMGetSvmExitName(uint32_t uExit);
VMM_INT_DECL(void)              HMDumpHwvirtVmxState(PVMCPU pVCpu);
VMM_INT_DECL(void)              HMHCChangedPagingMode(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode);
VMM_INT_DECL(void)              HMGetVmxMsrsFromHwvirtMsrs(PCSUPHWVIRTMSRS pMsrs, PVMXMSRS pVmxMsrs);
VMM_INT_DECL(void)              HMGetSvmMsrsFromHwvirtMsrs(PCSUPHWVIRTMSRS pMsrs, PSVMMSRS pSvmMsrs);
/** @} */

/** @name All-context VMX helpers.
 *
 * These are hardware-assisted VMX functions (used by IEM/REM/CPUM and HM). Helpers
 * based purely on the Intel VT-x specification (used by IEM/REM and HM) can be
 * found in CPUM.
 * @{ */
VMM_INT_DECL(bool)              HMIsSubjectToVmxPreemptTimerErratum(void);
VMM_INT_DECL(bool)              HMCanExecuteVmxGuest(PVMCC pVM, PVMCPUCC pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(TRPMEVENT)         HMVmxEventTypeToTrpmEventType(uint32_t uIntInfo);
VMM_INT_DECL(uint32_t)          HMTrpmEventTypeToVmxEventType(uint8_t uVector, TRPMEVENT enmTrpmEvent, bool fIcebp);
/** @} */

/** @name All-context SVM helpers.
 *
 * These are hardware-assisted SVM functions (used by IEM/REM/CPUM and HM). Helpers
 * based purely on the AMD SVM specification (used by IEM/REM and HM) can be found
 * in CPUM.
 * @{ */
VMM_INT_DECL(TRPMEVENT)         HMSvmEventToTrpmEventType(PCSVMEVENT pSvmEvent, uint8_t uVector);
/** @} */

#ifndef IN_RC

/** @name R0, R3 HM (VMX/SVM agnostic) handlers.
 * @{ */
VMM_INT_DECL(int)               HMFlushTlb(PVMCPU pVCpu);
VMM_INT_DECL(int)               HMFlushTlbOnAllVCpus(PVMCC pVM);
VMM_INT_DECL(int)               HMInvalidatePageOnAllVCpus(PVMCC pVM, RTGCPTR GCVirt);
VMM_INT_DECL(int)               HMInvalidatePhysPage(PVMCC pVM, RTGCPHYS GCPhys);
VMM_INT_DECL(bool)              HMAreNestedPagingAndFullGuestExecEnabled(PVM pVM);
VMM_INT_DECL(bool)              HMIsLongModeAllowed(PVM pVM);
VMM_INT_DECL(bool)              HMIsNestedPagingActive(PVM pVM);
VMM_INT_DECL(bool)              HMIsMsrBitmapActive(PVM pVM);
# ifdef VBOX_WITH_NESTED_HWVIRT_VMX
VMM_INT_DECL(void)              HMNotifyVmxNstGstVmexit(PVMCPU pVCpu);
VMM_INT_DECL(void)              HMNotifyVmxNstGstCurrentVmcsChanged(PVMCPU pVCpu);
# endif
/** @} */

/** @name R0, R3 SVM handlers.
 * @{ */
VMM_INT_DECL(bool)              HMIsSvmVGifActive(PCVM pVM);
# ifdef VBOX_WITH_NESTED_HWVIRT_SVM
VMM_INT_DECL(void)              HMNotifySvmNstGstVmexit(PVMCPUCC pVCpu, PCPUMCTX pCtx);
# endif
VMM_INT_DECL(int)               HMIsSubjectToSvmErratum170(uint32_t *pu32Family, uint32_t *pu32Model, uint32_t *pu32Stepping);
VMM_INT_DECL(int)               HMHCMaybeMovTprSvmHypercall(PVMCC pVM, PVMCPUCC pVCpu);
/** @} */

#else /* Nops in RC: */

/** @name RC HM (VMX/SVM agnostic) handlers.
 * @{ */
# define HMFlushTlb(pVCpu)                                            do { } while (0)
# define HMFlushTlbOnAllVCpus(pVM)                                    do { } while (0)
# define HMInvalidatePageOnAllVCpus(pVM, GCVirt)                      do { } while (0)
# define HMInvalidatePhysPage(pVM,  GCVirt)                           do { } while (0)
# define HMAreNestedPagingAndFullGuestExecEnabled(pVM)                false
# define HMIsLongModeAllowed(pVM)                                     false
# define HMIsNestedPagingActive(pVM)                                  false
# define HMIsMsrBitmapsActive(pVM)                                    false
/** @} */

/** @name RC SVM handlers.
 * @{ */
# define HMIsSvmVGifActive(pVM)                                       false
# define HMNotifySvmNstGstVmexit(pVCpu, pCtx)                         do { } while (0)
# define HMIsSubjectToSvmErratum170(puFamily, puModel, puStepping)    false
# define HMHCMaybeMovTprSvmHypercall(pVM, pVCpu)                      do { } while (0)
/** @} */

#endif

#ifdef IN_RING0
/** @defgroup grp_hm_r0    The HM ring-0 Context API
 * @{
 */
VMMR0_INT_DECL(int)             HMR0Init(void);
VMMR0_INT_DECL(int)             HMR0Term(void);
VMMR0_INT_DECL(int)             HMR0InitVM(PVMCC pVM);
VMMR0_INT_DECL(int)             HMR0TermVM(PVMCC pVM);
VMMR0_INT_DECL(int)             HMR0EnableAllCpus(PVMCC pVM);
# ifdef VBOX_WITH_RAW_MODE
VMMR0_INT_DECL(int)             HMR0EnterSwitcher(PVMCC pVM, VMMSWITCHER enmSwitcher, bool *pfVTxDisabled);
VMMR0_INT_DECL(void)            HMR0LeaveSwitcher(PVMCC pVM, bool fVTxDisabled);
# endif

VMMR0_INT_DECL(int)             HMR0SetupVM(PVMCC pVM);
VMMR0_INT_DECL(int)             HMR0RunGuestCode(PVMCC pVM, PVMCPUCC pVCpu);
VMMR0_INT_DECL(int)             HMR0Enter(PVMCPUCC pVCpu);
VMMR0_INT_DECL(int)             HMR0LeaveCpu(PVMCPUCC pVCpu);
VMMR0_INT_DECL(void)            HMR0ThreadCtxCallback(RTTHREADCTXEVENT enmEvent, void *pvUser);
VMMR0_INT_DECL(void)            HMR0NotifyCpumUnloadedGuestFpuState(PVMCPUCC VCpu);
VMMR0_INT_DECL(void)            HMR0NotifyCpumModifiedHostCr0(PVMCPUCC VCpu);
VMMR0_INT_DECL(bool)            HMR0SuspendPending(void);
VMMR0_INT_DECL(int)             HMR0InvalidatePage(PVMCPUCC pVCpu, RTGCPTR GCVirt);
VMMR0_INT_DECL(int)             HMR0ImportStateOnDemand(PVMCPUCC pVCpu, uint64_t fWhat);

/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_hm_r3    The HM ring-3 Context API
 * @{
 */
VMMR3DECL(bool)                 HMR3IsEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsNestedPagingActive(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsVirtApicRegsEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsPostedIntrsEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsVpidActive(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsUXActive(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsSvmEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsVmxEnabled(PUVM pUVM);

VMMR3_INT_DECL(int)             HMR3Init(PVM pVM);
VMMR3_INT_DECL(int)             HMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3_INT_DECL(void)            HMR3Relocate(PVM pVM);
VMMR3_INT_DECL(int)             HMR3Term(PVM pVM);
VMMR3_INT_DECL(void)            HMR3Reset(PVM pVM);
VMMR3_INT_DECL(void)            HMR3ResetCpu(PVMCPU pVCpu);
VMMR3_INT_DECL(void)            HMR3CheckError(PVM pVM, int iStatusCode);
VMMR3_INT_DECL(void)            HMR3NotifyDebugEventChanged(PVM pVM);
VMMR3_INT_DECL(void)            HMR3NotifyDebugEventChangedPerCpu(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(bool)            HMR3IsActive(PCVMCPU pVCpu);
VMMR3_INT_DECL(int)             HMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3_INT_DECL(int)             HMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3_INT_DECL(int)             HMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(bool)            HMR3IsRescheduleRequired(PVM pVM, PCCPUMCTX pCtx);
VMMR3_INT_DECL(bool)            HMR3IsVmxPreemptionTimerUsed(PVM pVM);
/** @} */
#endif /* IN_RING3 */

/** @} */
RT_C_DECLS_END


#endif /* !VBOX_INCLUDED_vmm_hm_h */

