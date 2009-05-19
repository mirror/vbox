/** @file
 * HWACCM - Intel/AMD VM Hardware Support Manager
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_hwaccm_h
#define ___VBox_hwaccm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <iprt/mp.h>


/** @defgroup grp_hwaccm      The VM Hardware Manager API
 * @{
 */

/**
 * HWACCM state
 */
typedef enum HWACCMSTATE
{
    /* Not yet set */
    HWACCMSTATE_UNINITIALIZED = 0,
    /* Enabled */
    HWACCMSTATE_ENABLED,
    /* Disabled */
    HWACCMSTATE_DISABLED,
    /** The usual 32-bit hack. */
    HWACCMSTATE_32BIT_HACK = 0x7fffffff
} HWACCMSTATE;

__BEGIN_DECLS

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

VMMDECL(int)    HWACCMInvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt);
VMMDECL(bool)   HWACCMHasPendingIrq(PVM pVM);

#ifndef IN_RC
VMMDECL(int)     HWACCMFlushTLB(PVMCPU pVCpu);
VMMDECL(int)     HWACCMFlushAllTLBs(PVM pVM);
VMMDECL(int)     HWACCMInvalidatePhysPage(PVMCPU pVCpu, RTGCPHYS GCPhys);
VMMDECL(bool)    HWACCMIsNestedPagingActive(PVM pVM);
VMMDECL(PGMMODE) HWACCMGetShwPagingMode(PVM pVM);
#else
/* Nop in GC */
# define HWACCMFlushTLB(pVCpu)                  do { } while (0)
# define HWACCMIsNestedPagingActive(pVM)        false
#endif

#ifdef IN_RING0
/** @defgroup grp_hwaccm_r0    The VM Hardware Manager API
 * @ingroup grp_hwaccm
 * @{
 */
VMMR0DECL(int)  HWACCMR0Init(void);
VMMR0DECL(int)  HWACCMR0Term(void);
VMMR0DECL(int)  HWACCMR0InitVM(PVM pVM);
VMMR0DECL(int)  HWACCMR0TermVM(PVM pVM);
VMMR0DECL(int)  HWACCMR0EnableAllCpus(PVM pVM);
VMMR0DECL(int)  HWACCMR0EnterSwitcher(PVM pVM, bool *pfVTxDisabled);
VMMR0DECL(int)  HWACCMR0LeaveSwitcher(PVM pVM, bool fVTxDisabled);

VMMR0DECL(PVMCPU)  HWACCMR0GetVMCPU(PVM pVM);
VMMR0DECL(VMCPUID) HWACCMR0GetVMCPUId(PVM pVM);

/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_hwaccm_r3    The VM Hardware Manager API
 * @ingroup grp_hwaccm
 * @{
 */
VMMR3DECL(bool) HWACCMR3IsEventPending(PVM pVM);
VMMR3DECL(int)  HWACCMR3Init(PVM pVM);
VMMR3DECL(int)  HWACCMR3InitCPU(PVM pVM);
VMMR3DECL(int)  HWACCMR3InitFinalizeR0(PVM pVM);
VMMR3DECL(void) HWACCMR3Relocate(PVM pVM);
VMMR3DECL(int)  HWACCMR3Term(PVM pVM);
VMMR3DECL(int)  HWACCMR3TermCPU(PVM pVM);
VMMR3DECL(void) HWACCMR3Reset(PVM pVM);
VMMR3DECL(void) HWACCMR3CheckError(PVM pVM, int iStatusCode);
VMMR3DECL(bool) HWACCMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx);
VMMR3DECL(void) HWACCMR3NotifyScheduled(PVMCPU pVCpu);
VMMR3DECL(void) HWACCMR3NotifyEmulated(PVMCPU pVCpu);
VMMR3DECL(bool) HWACCMR3IsActive(PVMCPU pVCpu);
VMMR3DECL(bool) HWACCMR3IsNestedPagingActive(PVM pVM);
VMMR3DECL(bool) HWACCMR3IsAllowed(PVM pVM);
VMMR3DECL(void) HWACCMR3PagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode);
VMMR3DECL(bool) HWACCMR3IsVPIDActive(PVM pVM);
VMMR3DECL(int)  HWACCMR3InjectNMI(PVM pVM);
VMMR3DECL(int)  HWACCMR3EmulateIoBlock(PVM pVM, PCPUMCTX pCtx);

/** @} */
#endif /* IN_RING3 */

#ifdef IN_RING0
/** @addtogroup grp_hwaccm_r0
 * @{
 */
VMMR0DECL(int)   HWACCMR0SetupVM(PVM pVM);
VMMR0DECL(int)   HWACCMR0RunGuestCode(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)   HWACCMR0Enter(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)   HWACCMR0Leave(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)   HWACCMR0InvalidatePage(PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)   HWACCMR0FlushTLB(PVM pVM);
VMMR0DECL(bool)  HWACCMR0SuspendPending();

# if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
VMMR0DECL(int)   HWACCMR0SaveFPUState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)   HWACCMR0SaveDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0DECL(int)   HWACCMR0TestSwitcher3264(PVM pVM);
# endif

/** @} */
#endif /* IN_RING0 */


/** @} */
__END_DECLS


#endif

