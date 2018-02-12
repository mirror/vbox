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
#if defined(VBOX_WITH_NATIVE_NEM) && defined(IN_RING3)
    if (VM_IS_NEM_ENABLED(pVM))
        nemR3NativeNotifyHandlerPhysicalRegister(pVM, enmKind, GCPhys, cb);
#else
    RT_NOREF(pVM, enmKind, GCPhys, cb);
#endif
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        int fRestoreAsRAM, bool fRestoreAsRAM2)
{
#if defined(VBOX_WITH_NATIVE_NEM) && defined(IN_RING3)
    if (VM_IS_NEM_ENABLED(pVM))
        nemR3NativeNotifyHandlerPhysicalDeregister(pVM, enmKind, GCPhys, cb, fRestoreAsRAM, fRestoreAsRAM2);
#else
    RT_NOREF(pVM, enmKind, GCPhys, cb, fRestoreAsRAM, fRestoreAsRAM2);
#endif
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalModify(PVM pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                                    RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
#if defined(VBOX_WITH_NATIVE_NEM) && defined(IN_RING3)
    if (VM_IS_NEM_ENABLED(pVM))
        nemR3NativeNotifyHandlerPhysicalModify(pVM, enmKind, GCPhysOld, GCPhysNew, cb, fRestoreAsRAM);
#else
    RT_NOREF(pVM, enmKind, GCPhysOld, GCPhysNew, cb, fRestoreAsRAM);
#endif
}


VMM_INT_DECL(int)  NEMHCNotifyPhysPageAllocated(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                                PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#if defined(VBOX_WITH_NATIVE_NEM) && defined(IN_RING3)
    return nemR3NativeNotifyPhysPageAllocated(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
#else
    RT_NOREF(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
    return VINF_SUCCESS;
#endif
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#if defined(VBOX_WITH_NATIVE_NEM) && defined(IN_RING3)
    nemR3NativeNotifyPhysPageProtChanged(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
#else
    RT_NOREF(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);
#endif
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVM pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#if defined(VBOX_WITH_NATIVE_NEM) && defined(IN_RING3)
    nemR3NativeNotifyPhysPageChanged(pVM, GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, pu2State);
#else
    RT_NOREF(pVM, GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, pu2State);
#endif
}

