/* $Id$ */
/** @file
 * Guest Interface Manager (GIM), KVM - Host Context Ring-0.
 */

/*
 * Copyright (C) 2015-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_GIM
#include <VBox/vmm/gim.h>
#include <VBox/vmm/tm.h>
#include "GIMInternal.h"
#include "GIMKvmInternal.h"
#include <VBox/vmm/vmcc.h>

#include <VBox/err.h>

#include <iprt/spinlock.h>
#include <iprt/asm-math.h>


/**
 * Updates KVM's system time information globally for all VCPUs.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @thread  EMT.
 * @remarks Can be called with preemption disabled!
 */
VMM_INT_DECL(int) gimR0KvmUpdateSystemTime(PVMCC pVM, PVMCPUCC pVCpu)
{
    /*
     * Validate.
     */
    Assert(GIMIsEnabled(pVM));
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    AssertReturn(pKvm->hSpinlockR0 != NIL_RTSPINLOCK, VERR_GIM_IPE_3);

    /*
     * Record TSC and derive the virtual time from that (to ensure that
     * the two quantities are in perfect sync).
     */
    uint64_t uTsc;
    uint64_t uVirtNanoTS;

    uTsc        = TMCpuTickGetNoCheck(pVCpu);
    uVirtNanoTS = ASMMultU64ByU32DivByU32(uTsc, RT_NS_1SEC, pKvm->cTscTicksPerSecond);

    /*
     * Update VCPUs with this information. The first VCPU's values
     * will be applied to the remaining.
     */
    RTSpinlockAcquire(pKvm->hSpinlockR0);
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PGIMKVMCPU pKvmCpu = &VMCC_GET_CPU(pVM, idCpu)->gim.s.u.KvmCpu;
        pKvmCpu->uTsc        = uTsc;
        pKvmCpu->uVirtNanoTS = uVirtNanoTS;
    }
    RTSpinlockRelease(pKvm->hSpinlockR0);

    return VINF_SUCCESS;
}


/**
 * Does ring-0 per-VM GIM KVM initialization.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR0_INT_DECL(int) gimR0KvmInitVM(PVMCC pVM)
{
    AssertPtr(pVM);
    Assert(GIMIsEnabled(pVM));

    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    Assert(pKvm->hSpinlockR0 == NIL_RTSPINLOCK);

    int rc = RTSpinlockCreate(&pKvm->hSpinlockR0, RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE, "KVM");
    return rc;
}


/**
 * Does ring-0 per-VM GIM KVM termination.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR0_INT_DECL(int) gimR0KvmTermVM(PVMCC pVM)
{
    AssertPtr(pVM);
    Assert(GIMIsEnabled(pVM));

    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    RTSpinlockDestroy(pKvm->hSpinlockR0);
    pKvm->hSpinlockR0 = NIL_RTSPINLOCK;

    return VINF_SUCCESS;
}

