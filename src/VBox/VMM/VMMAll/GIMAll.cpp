/* $Id$ */
/** @file
 * GIM - Guest Interface Manager - All Contexts.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
 * @retval  @c true if used.
 * @retval  @c false if no GIM provider ("none") is used.
 *
 * @param   pVM       Pointer to the VM.
 */
VMMDECL(bool) GIMIsEnabled(PVM pVM)
{
    return pVM->gim.s.enmProviderId != GIMPROVIDERID_NONE;
}


/**
 * Gets the GIM provider configured for this VM.
 *
 * @returns The GIM provider Id.
 * @param   pVM     Pointer to the VM.
 */
VMMDECL(GIMPROVIDERID) GIMGetProvider(PVM pVM)
{
    return pVM->gim.s.enmProviderId;
}


/**
 * Returns whether the guest has configured and enabled calls to the hypervisor.
 *
 * @returns true if hypercalls are enabled and usable, false otherwise.
 * @param   pVCpu           Pointer to the VMCPU.
 */
VMM_INT_DECL(bool) GIMAreHypercallsEnabled(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (!GIMIsEnabled(pVM))
        return false;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMHvAreHypercallsEnabled(pVCpu);

        default:
            return false;
    }
}


/**
 * Implements a GIM hypercall with the provider configured for the VM.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 */
VMM_INT_DECL(int) GIMHypercall(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    if (RT_UNLIKELY(!GIMIsEnabled(pVM)))
        return VERR_GIM_NOT_ENABLED;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMHvHypercall(pVCpu, pCtx);

        default:
            AssertMsgFailed(("GIMHypercall: for unknown provider %u\n", pVM->gim.s.enmProviderId));
            return VERR_GIM_IPE_3;
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
 * @param   pVM         Pointer to the VM.
 */
VMM_INT_DECL(bool) GIMIsParavirtTscEnabled(PVM pVM)
{
    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMHvIsParavirtTscEnabled(pVM);

        default:
            break;
    }
    return false;
}


/**
 * Invokes the read-MSR handler for the GIM provider configured for the VM.
 *
 * @returns Strict VBox status code like CPUMQueryGuestMsr.
 * @retval  VINF_CPUM_R3_MSR_READ
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       Pointer to the VMCPU.
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
            return GIMHvReadMsr(pVCpu, idMsr, pRange, puValue);

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
 * @param   pVCpu       Pointer to the VMCPU.
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
            return GIMHvWriteMsr(pVCpu, idMsr, pRange, uRawValue);

        default:
            AssertMsgFailed(("GIMWriteMsr: for unknown provider %u idMsr=%#RX32 -> #GP(0)", pVM->gim.s.enmProviderId, idMsr));
            return VERR_CPUM_RAISE_GP_0;
    }
}

