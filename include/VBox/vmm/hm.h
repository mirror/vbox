/** @file
 * HM - Intel/AMD VM Hardware Support Manager (VMM)
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#ifndef ___VBox_vmm_hm_h
#define ___VBox_vmm_hm_h

#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vmm.h>
#include <iprt/mp.h>


/** @defgroup grp_hm      The VM Hardware Manager API
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * Query HM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
#define HMIsEnabled(pVM)    ((pVM)->fHMEnabled)

 /**
 * Check if the current CPU state is valid for emulating IO blocks in the recompiler
 *
 * @returns boolean
 * @param   pCtx        CPU context
 */
#define HMCanEmulateIoBlock(pVCpu)     (!CPUMIsGuestInPagedProtectedMode(pVCpu))
#define HMCanEmulateIoBlockEx(pCtx)    (!CPUMIsGuestInPagedProtectedModeEx(pCtx))

VMMDECL(int)            HMInvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt);
VMMDECL(bool)           HMHasPendingIrq(PVM pVM);
VMMDECL(PX86PDPE)       HMGetPaePdpes(PVMCPU pVCpu);

#ifndef IN_RC
VMMDECL(int)            HMFlushTLB(PVMCPU pVCpu);
VMMDECL(int)            HMFlushTLBOnAllVCpus(PVM pVM);
VMMDECL(int)            HMInvalidatePageOnAllVCpus(PVM pVM, RTGCPTR GCVirt);
VMMDECL(int)            HMInvalidatePhysPage(PVM pVM, RTGCPHYS GCPhys);
VMMDECL(bool)           HMIsNestedPagingActive(PVM pVM);
VMMDECL(PGMMODE)        HMGetShwPagingMode(PVM pVM);
#else
/* Nop in GC */
# define HMFlushTLB(pVCpu)                  do { } while (0)
# define HMIsNestedPagingActive(pVM)        false
# define HMFlushTLBOnAllVCpus(pVM)          do { } while (0)
#endif

#ifdef IN_RING0
/** @defgroup grp_hm_r0    The VM Hardware Manager API
 * @ingroup grp_hm
 * @{
 */
VMMR0DECL(int)          HMR0Init(void);
VMMR0DECL(int)          HMR0Term(void);
VMMR0DECL(int)          HMR0InitVM(PVM pVM);
VMMR0DECL(int)          HMR0TermVM(PVM pVM);
VMMR0DECL(int)          HMR0EnableAllCpus(PVM pVM);
VMMR0DECL(int)          HMR0EnterSwitcher(PVM pVM, VMMSWITCHER enmSwitcher, bool *pfVTxDisabled);
VMMR0DECL(void)         HMR0LeaveSwitcher(PVM pVM, bool fVTxDisabled);

VMMR0DECL(void)         HMR0SavePendingIOPortWrite(PVMCPU pVCpu, RTGCPTR GCPtrRip, RTGCPTR GCPtrRipNext, unsigned uPort, unsigned uAndVal, unsigned cbSize);
VMMR0DECL(void)         HMR0SavePendingIOPortRead(PVMCPU pVCpu, RTGCPTR GCPtrRip, RTGCPTR GCPtrRipNext, unsigned uPort, unsigned uAndVal, unsigned cbSize);

/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_hm_r3    The VM Hardware Manager API
 * @ingroup grp_hm
 * @{
 */
VMMR3DECL(bool)         HMR3IsEventPending(PVMCPU pVCpu);
VMMR3DECL(int)          HMR3Init(PVM pVM);
VMMR3_INT_DECL(int)     HMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3DECL(void)         HMR3Relocate(PVM pVM);
VMMR3DECL(int)          HMR3Term(PVM pVM);
VMMR3DECL(void)         HMR3Reset(PVM pVM);
VMMR3DECL(void)         HMR3ResetCpu(PVMCPU pVCpu);
VMMR3DECL(void)         HMR3CheckError(PVM pVM, int iStatusCode);
VMMR3DECL(bool)         HMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(void)         HMR3NotifyScheduled(PVMCPU pVCpu);
VMMR3DECL(void)         HMR3NotifyEmulated(PVMCPU pVCpu);
VMMR3DECL(bool)         HMR3IsActive(PVMCPU pVCpu);
VMMR3DECL(bool)         HMR3IsNestedPagingActive(PVM pVM);
VMMR3DECL(bool)         HMR3IsAllowed(PVM pVM);
VMMR3DECL(void)         HMR3PagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode);
VMMR3DECL(bool)         HMR3IsVPIDActive(PVM pVM);
VMMR3DECL(int)          HMR3InjectNMI(PVM pVM);
VMMR3DECL(int)          HMR3EmulateIoBlock(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(VBOXSTRICTRC) HMR3RestartPendingIOInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR3DECL(int)          HMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3DECL(int)          HMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3DECL(int)          HMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR3DECL(bool)         HMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(bool)         HMR3IsVmxPreemptionTimerUsed(PVM pVM);

/** @} */
#endif /* IN_RING3 */

#ifdef IN_RING0
/** @addtogroup grp_hm_r0
 * @{
 */
VMMR0DECL(int)          HMR0SetupVM(PVM pVM);
VMMR0DECL(int)          HMR0RunGuestCode(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HMR0Enter(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HMR0Leave(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HMR0InvalidatePage(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HMR0FlushTLB(PVM pVM);
VMMR0DECL(bool)         HMR0SuspendPending(void);

# if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
VMMR0DECL(int)          HMR0SaveFPUState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          HMR0SaveDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          HMR0TestSwitcher3264(PVM pVM);
# endif

/** @} */
#endif /* IN_RING0 */


/** @} */
RT_C_DECLS_END


#endif

