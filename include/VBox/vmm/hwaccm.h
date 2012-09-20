/** @file
 * HWACCM - Intel/AMD VM Hardware Support Manager (VMM)
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

#ifndef ___VBox_vmm_hwaccm_h
#define ___VBox_vmm_hwaccm_h

#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vmm.h>
#include <iprt/mp.h>


/** @defgroup grp_hwaccm      The VM Hardware Manager API
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * Query HWACCM state (enabled/disabled)
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The VM to operate on.
 */
#define HWACCMIsEnabled(pVM)    ((pVM)->fHWACCMEnabled)

 /**
 * Check if the current CPU state is valid for emulating IO blocks in the recompiler
 *
 * @returns boolean
 * @param   pCtx        CPU context
 */
#define HWACCMCanEmulateIoBlock(pVCpu)     (!CPUMIsGuestInPagedProtectedMode(pVCpu))
#define HWACCMCanEmulateIoBlockEx(pCtx)    (!CPUMIsGuestInPagedProtectedModeEx(pCtx))

VMMDECL(int)            HWACCMInvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt);
VMMDECL(bool)           HWACCMHasPendingIrq(PVM pVM);

#ifndef IN_RC
VMMDECL(int)            HWACCMFlushTLB(PVMCPU pVCpu);
VMMDECL(int)            HWACCMFlushTLBOnAllVCpus(PVM pVM);
VMMDECL(int)            HWACCMInvalidatePageOnAllVCpus(PVM pVM, RTGCPTR GCVirt);
VMMDECL(int)            HWACCMInvalidatePhysPage(PVM pVM, RTGCPHYS GCPhys);
VMMDECL(bool)           HWACCMIsNestedPagingActive(PVM pVM);
VMMDECL(PGMMODE)        HWACCMGetShwPagingMode(PVM pVM);
#else
/* Nop in GC */
# define HWACCMFlushTLB(pVCpu)                  do { } while (0)
# define HWACCMIsNestedPagingActive(pVM)        false
# define HWACCMFlushTLBOnAllVCpus(pVM)          do { } while (0)
#endif

#ifdef IN_RING0
/** @defgroup grp_hwaccm_r0    The VM Hardware Manager API
 * @ingroup grp_hwaccm
 * @{
 */
VMMR0DECL(int)          HWACCMR0Init(void);
VMMR0DECL(int)          HWACCMR0Term(void);
VMMR0DECL(int)          HWACCMR0InitVM(PVM pVM);
VMMR0DECL(int)          HWACCMR0TermVM(PVM pVM);
VMMR0DECL(int)          HWACCMR0EnableAllCpus(PVM pVM);
VMMR0DECL(int)          HWACCMR0EnterSwitcher(PVM pVM, VMMSWITCHER enmSwitcher, bool *pfVTxDisabled);
VMMR0DECL(void)         HWACCMR0LeaveSwitcher(PVM pVM, bool fVTxDisabled);

VMMR0DECL(void)         HWACCMR0SavePendingIOPortWrite(PVMCPU pVCpu, RTGCPTR GCPtrRip, RTGCPTR GCPtrRipNext, unsigned uPort, unsigned uAndVal, unsigned cbSize);
VMMR0DECL(void)         HWACCMR0SavePendingIOPortRead(PVMCPU pVCpu, RTGCPTR GCPtrRip, RTGCPTR GCPtrRipNext, unsigned uPort, unsigned uAndVal, unsigned cbSize);

/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_hwaccm_r3    The VM Hardware Manager API
 * @ingroup grp_hwaccm
 * @{
 */
VMMR3DECL(bool)         HWACCMR3IsEventPending(PVMCPU pVCpu);
VMMR3DECL(int)          HWACCMR3Init(PVM pVM);
VMMR3_INT_DECL(int)     HWACCMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3DECL(void)         HWACCMR3Relocate(PVM pVM);
VMMR3DECL(int)          HWACCMR3Term(PVM pVM);
VMMR3DECL(void)         HWACCMR3Reset(PVM pVM);
VMMR3DECL(void)         HWACCMR3ResetCpu(PVMCPU pVCpu);
VMMR3DECL(void)         HWACCMR3CheckError(PVM pVM, int iStatusCode);
VMMR3DECL(bool)         HWACCMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(void)         HWACCMR3NotifyScheduled(PVMCPU pVCpu);
VMMR3DECL(void)         HWACCMR3NotifyEmulated(PVMCPU pVCpu);
VMMR3DECL(bool)         HWACCMR3IsActive(PVMCPU pVCpu);
VMMR3DECL(bool)         HWACCMR3IsNestedPagingActive(PVM pVM);
VMMR3DECL(bool)         HWACCMR3IsAllowed(PVM pVM);
VMMR3DECL(void)         HWACCMR3PagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode);
VMMR3DECL(bool)         HWACCMR3IsVPIDActive(PVM pVM);
VMMR3DECL(int)          HWACCMR3InjectNMI(PVM pVM);
VMMR3DECL(int)          HWACCMR3EmulateIoBlock(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(VBOXSTRICTRC) HWACCMR3RestartPendingIOInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR3DECL(int)          HWACMMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3DECL(int)          HWACMMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3DECL(int)          HWACCMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR3DECL(bool)         HWACCMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(bool)         HWACCMR3IsVmxPreemptionTimerUsed(PVM pVM);

/** @} */
#endif /* IN_RING3 */

#ifdef IN_RING0
/** @addtogroup grp_hwaccm_r0
 * @{
 */
VMMR0DECL(int)          HWACCMR0SetupVM(PVM pVM);
VMMR0DECL(int)          HWACCMR0RunGuestCode(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HWACCMR0Enter(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HWACCMR0Leave(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HWACCMR0InvalidatePage(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          HWACCMR0FlushTLB(PVM pVM);
VMMR0DECL(bool)         HWACCMR0SuspendPending(void);

# if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
VMMR0DECL(int)          HWACCMR0SaveFPUState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          HWACCMR0SaveDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)          HWACCMR0TestSwitcher3264(PVM pVM);
# endif

/** @} */
#endif /* IN_RING0 */


/** @} */
RT_C_DECLS_END


#endif

