/* $Id$ */
/** @file
 * GIM - Guest Interface Manager - All Contexts.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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
#include "GIMInternal.h"
#include <VBox/err.h>
#include <VBox/vmm/vm.h>

/* Include all the providers. */
#include "GIMHvInternal.h"
#include "GIMMinimalInternal.h"


/**
 * Checks whether GIM is being used by this VM.
 *
 * @retval  true if used.
 * @retval  false if no GIM provider ("none") is used.
 *
 * @param   pVM       The cross context VM structure.
 */
VMMDECL(bool) GIMIsEnabled(PVM pVM)
{
    return pVM->gim.s.enmProviderId != GIMPROVIDERID_NONE;
}


/**
 * Gets the GIM provider configured for this VM.
 *
 * @returns The GIM provider Id.
 * @param   pVM     The cross context VM structure.
 */
VMMDECL(GIMPROVIDERID) GIMGetProvider(PVM pVM)
{
    return pVM->gim.s.enmProviderId;
}


/**
 * Returns whether the guest has configured and enabled calls to the hypervisor.
 *
 * @returns true if hypercalls are enabled and usable, false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
VMM_INT_DECL(bool) GIMAreHypercallsEnabled(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (!GIMIsEnabled(pVM))
        return false;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimHvAreHypercallsEnabled(pVCpu);

        case GIMPROVIDERID_KVM:
            return gimKvmAreHypercallsEnabled(pVCpu);

        default:
            return false;
    }
}


/**
 * Implements a GIM hypercall with the provider configured for the VM.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @thread  EMT.
 */
VMM_INT_DECL(int) GIMHypercall(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    if (RT_UNLIKELY(!GIMIsEnabled(pVM)))
        return VERR_GIM_NOT_ENABLED;

    STAM_REL_COUNTER_INC(&pVM->gim.s.StatHypercalls);
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimHvHypercall(pVCpu, pCtx);

        case GIMPROVIDERID_KVM:
            return gimKvmHypercall(pVCpu, pCtx);

        default:
            AssertMsgFailed(("GIMHypercall: for provider %u not available/implemented\n", pVM->gim.s.enmProviderId));
            return VERR_GIM_HYPERCALLS_NOT_AVAILABLE;
    }
}


/**
 * Returns whether the guest has configured and setup the use of paravirtualized
 * TSC.
 *
 * Paravirtualized TSCs are per-VM and the rest of the execution engine logic
 * relies on that.
 *
 * @returns true if enabled and usable, false otherwise.
 * @param   pVM         The cross context VM structure.
 */
VMM_INT_DECL(bool) GIMIsParavirtTscEnabled(PVM pVM)
{
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimHvIsParavirtTscEnabled(pVM);

        case GIMPROVIDERID_KVM:
            return gimKvmIsParavirtTscEnabled(pVM);

        default:
            break;
    }
    return false;
}


/**
 * Whether \#UD exceptions in the guest needs to be intercepted by the GIM
 * provider.
 *
 * At the moment, the reason why this isn't a more generic interface wrt to
 * exceptions is because of performance (each VM-exit would have to manually
 * check whether or not GIM needs to be notified). Left as a todo for later if
 * really required.
 *
 * @returns true if needed, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMM_INT_DECL(bool) GIMShouldTrapXcptUD(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (!GIMIsEnabled(pVM))
        return false;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_KVM:
            return gimKvmShouldTrapXcptUD(pVCpu);

        case GIMPROVIDERID_HYPERV:
            return gimHvShouldTrapXcptUD(pVCpu);

        default:
            return false;
    }
}


/**
 * Exception handler for \#UD when requested by the GIM provider.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        Pointer to the guest-CPU context.
 * @param   pDis        Pointer to the disassembled instruction state at RIP.
 *                      Optional, can be NULL.
 */
VMM_INT_DECL(int) GIMXcptUD(PVMCPU pVCpu, PCPUMCTX pCtx, PDISCPUSTATE pDis)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(GIMIsEnabled(pVM));

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_KVM:
            return gimKvmXcptUD(pVCpu, pCtx, pDis);

        case GIMPROVIDERID_HYPERV:
            return gimHvXcptUD(pVCpu, pCtx, pDis);

        default:
            return VERR_GIM_OPERATION_FAILED;
    }
}


/**
 * Invokes the read-MSR handler for the GIM provider configured for the VM.
 *
 * @returns Strict VBox status code like CPUMQueryGuestMsr.
 * @retval  VINF_CPUM_R3_MSR_READ
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idMsr       The MSR to read.
 * @param   pRange      The range this MSR belongs to.
 * @param   puValue     Where to store the MSR value read.
 */
VMM_INT_DECL(VBOXSTRICTRC) GIMReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue)
{
    Assert(pVCpu);
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(GIMIsEnabled(pVM));
    VMCPU_ASSERT_EMT(pVCpu);

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimHvReadMsr(pVCpu, idMsr, pRange, puValue);

        case GIMPROVIDERID_KVM:
            return gimKvmReadMsr(pVCpu, idMsr, pRange, puValue);

        default:
            AssertMsgFailed(("GIMReadMsr: for unknown provider %u idMsr=%#RX32 -> #GP(0)", pVM->gim.s.enmProviderId, idMsr));
            return VERR_CPUM_RAISE_GP_0;
    }
}


/**
 * Invokes the write-MSR handler for the GIM provider configured for the VM.
 *
 * @returns Strict VBox status code like CPUMSetGuestMsr.
 * @retval  VINF_CPUM_R3_MSR_WRITE
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   idMsr       The MSR to write.
 * @param   pRange      The range this MSR belongs to.
 * @param   uValue      The value to set, ignored bits masked.
 * @param   uRawValue   The raw value with the ignored bits not masked.
 */
VMM_INT_DECL(VBOXSTRICTRC) GIMWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uValue, uint64_t uRawValue)
{
    AssertPtr(pVCpu);
    NOREF(uValue);

    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(GIMIsEnabled(pVM));
    VMCPU_ASSERT_EMT(pVCpu);

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return gimHvWriteMsr(pVCpu, idMsr, pRange, uRawValue);

        case GIMPROVIDERID_KVM:
            return gimKvmWriteMsr(pVCpu, idMsr, pRange, uRawValue);

        default:
            AssertMsgFailed(("GIMWriteMsr: for unknown provider %u idMsr=%#RX32 -> #GP(0)", pVM->gim.s.enmProviderId, idMsr));
            return VERR_CPUM_RAISE_GP_0;
    }
}

