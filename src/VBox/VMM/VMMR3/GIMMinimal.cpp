/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, Minimal implementation.
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

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>

#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/pdmapi.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

VMMR3_INT_DECL(int) GIMR3MinimalInit(PVM pVM)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pVM->gim.s.enmProviderId == GIMPROVIDERID_MINIMAL, VERR_INTERNAL_ERROR_5);

    /*
     * Enable the Hypervisor Present.
     */
    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) GIMR3MinimalInitFinalize(PVM pVM)
{
    /*
     * Expose a generic hypervisor-agnostic leaf (originally defined VMware).
     * The leaves range from  0x40000010 to 0x400000FF.
     *
     * This is done in the finalize routine as we need PDM to be
     * initialized (otherwise PDMApicGetTimerFreq() would fail).
     */
    CPUMCPUIDLEAF HyperLeaf;
    int rc = CPUMR3CpuIdGetLeaf(pVM, &HyperLeaf, 0x40000000, 0 /* uSubLeaf */);
    if (RT_SUCCESS(rc))
    {
        HyperLeaf.uEax         = UINT32_C(0x40000010);  /* Maximum leaf we implement. */
        rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
        AssertLogRelRCReturn(rc, rc);

        /*
         * Add the timing information hypervisor leaf.
         * MacOS X uses this to determine the TSC, bus frequency. See @bugref{7270}.
         *
         * EAX - TSC frequency in KHz.
         * EBX - APIC frequency in KHz.
         * ECX, EDX - Reserved.
         */
        uint64_t uApicFreq;
        rc = PDMApicGetTimerFreq(pVM, &uApicFreq);
        AssertLogRelRCReturn(rc, rc);

        RT_ZERO(HyperLeaf);
        HyperLeaf.uLeaf        = UINT32_C(0x40000010);
        HyperLeaf.uEax         = TMCpuTicksPerSecond(pVM) / UINT64_C(1000);
        HyperLeaf.uEbx         = (uApicFreq + 500) / UINT64_C(1000);
        rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
        AssertLogRelRCReturn(rc, rc);
    }
    else
        LogRel(("GIM: Minimal: failed to get hypervisor leaf 0x40000000.\n"));

    return VINF_SUCCESS;
}

VMMR3_INT_DECL(void) GIMR3MinimalRelocate(PVM pVM, RTGCINTPTR offDelta)
{
    NOREF(pVM); NOREF(offDelta);
}

