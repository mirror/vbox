/* $Id$ */
/** @file
 * NEM - Native execution manager, R0 and R3 context code.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_NEM
#include <VBox/vmm/nem.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>


/**
 * Checks if this VM is in NEM mode and is long-mode capable.
 *
 * Use VMR3IsLongModeAllowed() instead of this, when possible.
 *
 * @returns true if long mode is allowed, false otherwise.
 * @param   pVM         The cross context VM structure.
 * @sa      VMR3IsLongModeAllowed, HMIsLongModeAllowed
 */
VMM_INT_DECL(bool) NEMHCIsLongModeAllowed(PVM pVM)
{
    return pVM->nem.s.fAllow64BitGuests && VM_IS_NEM_ENABLED(pVM);
}


/**
 * Physical access handler registration notification.
 *
 * @param   pVM         The cross context VM structure.
 * @param   enmKind     The kind of access handler.
 * @param   GCPhys      Start of the access handling range.
 * @param   cb          Length of the access handling range.
 *
 * @note    Called while holding down the PGM lock.
 */
VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (VM_IS_NEM_ENABLED(pVM))
        nemHCNativeNotifyHandlerPhysicalRegister(pVM, enmKind, GCPhys, cb);
#else
    RT_NOREF(pVM, enmKind, GCPhys, cb);
#endif
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        int fRestoreAsRAM, bool fRestoreAsRAM2)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (VM_IS_NEM_ENABLED(pVM))
        nemHCNativeNotifyHandlerPhysicalDeregister(pVM, enmKind, GCPhys, cb, fRestoreAsRAM, fRestoreAsRAM2);
#else
    RT_NOREF(pVM, enmKind, GCPhys, cb, fRestoreAsRAM, fRestoreAsRAM2);
#endif
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                                    RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (VM_IS_NEM_ENABLED(pVM))
        nemHCNativeNotifyHandlerPhysicalModify(pVM, enmKind, GCPhysOld, GCPhysNew, cb, fRestoreAsRAM);
#else
    RT_NOREF(pVM, enmKind, GCPhysOld, GCPhysNew, cb, fRestoreAsRAM);
#endif
}


VMM_INT_DECL(int)  NEMHCNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                                PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    return nemHCNativeNotifyPhysPageAllocated(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
#else
    RT_NOREF(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
    return VINF_SUCCESS;
#endif
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    nemHCNativeNotifyPhysPageProtChanged(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
#else
    RT_NOREF(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
#endif
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    nemHCNativeNotifyPhysPageChanged(pVM, GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, pu2State);
#else
    RT_NOREF(pVM, GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, pu2State);
#endif
}


#ifndef VBOX_WITH_NATIVE_NEM
VMM_INT_DECL(int) NEMImportStateOnDemand(PVMCPU pVCpu, uint64_t fWhat)
{
    RT_NOREF(pVCpu, fWhat);
    return VERR_NEM_IPE_9;
}
#endif


#ifndef VBOX_WITH_NATIVE_NEM
VMM_INT_DECL(int) NEMHCQueryCpuTick(PVMCPU pVCpu, uint64_t *pcTicks, uint32_t *puAux)
{
    RT_NOREF(pVCpu, pcTicks, puAux);
    AssertFailed();
    return VERR_NEM_IPE_9;
}
#endif


#ifndef VBOX_WITH_NATIVE_NEM
VMM_INT_DECL(int) NEMHCResumeCpuTickOnAll(PVM pVM, PVMCPU pVCpu, uint64_t uPausedTscValue)
{
    RT_NOREF(pVM, pVCpu, uPausedTscValue);
    AssertFailed();
    return VERR_NEM_IPE_9;
}
#endif

