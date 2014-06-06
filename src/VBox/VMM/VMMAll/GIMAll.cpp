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
    return pVM->gim.s.fEnabled;
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
    Assert(GIMIsEnabled(pVM));
    VMCPU_ASSERT_EMT(pVCpu);

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
 * Updates the paravirtualized TSC supported by the GIM provider.
 *
 * @returns VBox status code.
 * @retval VINF_SUCCESS if the paravirt. TSC is setup and in use.
 * @retval VERR_GIM_NOT_ENABLED if no GIM provider is configured for this VM.
 * @retval VERR_GIM_PVTSC_NOT_AVAILABLE if the GIM provider does not support any
 *         paravirt. TSC.
 * @retval VERR_GIM_PVTSC_NOT_IN_USE if the GIM provider supports paravirt. TSC
 *         but the guest isn't currently using it.
 *
 * @param   pVM         Pointer to the VM.
 * @param   u64Offset   The computed TSC offset.
 *
 * @thread EMT(pVCpu)
 */
VMMDECL(int) GIMUpdateParavirtTsc(PVM pVM, uint64_t u64Offset)
{
    if (!pVM->gim.s.fEnabled)
        return VERR_GIM_NOT_ENABLED;

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMHvUpdateParavirtTsc(pVM, u64Offset);

        default:
            break;
    }
    return VERR_GIM_PVTSC_NOT_AVAILABLE;
}


/**
 * Invokes the read-MSR handler for the GIM provider configured for the VM.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR to read.
 * @param   pRange      The range this MSR belongs to.
 * @param   puValue     Where to store the MSR value read.
 */
VMM_INT_DECL(int) GIMReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue)
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
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR to write.
 * @param   pRange      The range this MSR belongs to.
 * @param   uValue      The value to set, ignored bits masked.
 * @param   uRawValue   The raw value with the ignored bits not masked.
 */
VMM_INT_DECL(int) GIMWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uValue, uint64_t uRawValue)
{
    Assert(pVCpu);
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(GIMIsEnabled(pVM));
    VMCPU_ASSERT_EMT(pVCpu);

    switch (pVM->gim.s.enmProviderId)
    {
        case GIMPROVIDERID_HYPERV:
            return GIMHvWriteMsr(pVCpu, idMsr, pRange, uValue, uRawValue);

        default:
            AssertMsgFailed(("GIMWriteMsr: for unknown provider %u idMsr=%#RX32 -> #GP(0)", pVM->gim.s.enmProviderId, idMsr));
            return VERR_CPUM_RAISE_GP_0;
    }
}

