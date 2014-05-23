/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, Microsoft Hyper-V, All Contexts.
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
#include "GIMHvInternal.h"

#include <VBox/err.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/vm.h>

DECLEXPORT(int) GIMHvHypercall(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    return VINF_SUCCESS;
}


DECLEXPORT(int) GIMHvReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue)
{
    NOREF(pRange);
    switch (idMsr)
    {
        case MSR_GIM_HV_TIME_REF_COUNT:
        {
            /* Hyper-V reports the time in 100ns units. */
            uint64_t u64Tsc      = TMCpuTickGet(pVCpu);
            uint64_t u64TscHz    = TMCpuTicksPerSecond(pVCpu->CTX_SUFF(pVM));
            uint64_t u64Tsc100Ns = u64TscHz / UINT64_C(10000000); /* 100 ns */
            *puValue = (u64Tsc / u64Tsc100Ns);
            return VINF_SUCCESS;
        }

        case MSR_GIM_HV_VP_INDEX:
            *puValue = pVCpu->idCpu;
            return VINF_SUCCESS;

        default:
            break;
    }

    LogRel(("GIMHvReadMsr: Unknown/invalid RdMsr %#RX32 -> #GP(0)\n", idMsr));
    return VERR_CPUM_RAISE_GP_0;
}


DECLEXPORT(int) GIMHvWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uValue, uint64_t uRawValue)
{
    LogRel(("GIMHvWriteMsr: Unknown/invalid WrMsr %#RX32 -> #GP(0)\n", idMsr));
    return VERR_CPUM_RAISE_GP_0;
}

