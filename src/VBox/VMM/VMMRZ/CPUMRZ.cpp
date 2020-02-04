/* $Id$ */
/** @file
 * CPUM - Raw-mode and ring-0 context.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vmcc.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/hm.h>
#include <iprt/assert.h>
#include <iprt/x86.h>




/**
 * Prepares the host FPU/SSE/AVX stuff for IEM action.
 *
 * This will make sure the FPU/SSE/AVX guest state is _not_ loaded in the CPU.
 * This will make sure the FPU/SSE/AVX host state is saved.
 * Finally, it will make sure the FPU/SSE/AVX host features can be safely
 * accessed.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStatePrepareHostCpuForUse(PVMCPUCC pVCpu)
{
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_FPU_REM;
    switch (pVCpu->cpum.s.fUseFlags & (CPUM_USED_FPU_GUEST | CPUM_USED_FPU_HOST))
    {
        case 0:
            if (cpumRZSaveHostFPUState(&pVCpu->cpum.s) == VINF_CPUM_HOST_CR0_MODIFIED)
                HMR0NotifyCpumModifiedHostCr0(pVCpu);
            Log6(("CPUMRZFpuStatePrepareHostCpuForUse: #0 - %#x\n", ASMGetCR0()));
            break;

        case CPUM_USED_FPU_HOST:
            Log6(("CPUMRZFpuStatePrepareHostCpuForUse: #1 - %#x\n", ASMGetCR0()));
            break;

        case CPUM_USED_FPU_GUEST | CPUM_USED_FPU_HOST:
            cpumRZSaveGuestFpuState(&pVCpu->cpum.s, true /*fLeaveFpuAccessible*/);
#ifdef IN_RING0
            HMR0NotifyCpumUnloadedGuestFpuState(pVCpu);
#endif
            Log6(("CPUMRZFpuStatePrepareHostCpuForUse: #2 - %#x\n", ASMGetCR0()));
            break;

        default:
            AssertFailed();
    }

}


/**
 * Makes sure the FPU/SSE/AVX guest state is saved in CPUMCPU::Guest and will be
 * reloaded before direct use.
 *
 * No promisses about the FPU/SSE/AVX host features are made.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeForChange(PVMCPUCC pVCpu)
{
    CPUMRZFpuStatePrepareHostCpuForUse(pVCpu);
}


/**
 * Makes sure the FPU/SSE/AVX state in CPUMCPU::Guest is up to date.
 *
 * This will not cause CPUM_USED_FPU_GUEST to change.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeForRead(PVMCPUCC pVCpu)
{
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_GUEST)
    {
        cpumRZSaveGuestFpuState(&pVCpu->cpum.s, false /*fLeaveFpuAccessible*/);
        pVCpu->cpum.s.fUseFlags |= CPUM_USED_FPU_GUEST;
        Log7(("CPUMRZFpuStateActualizeForRead\n"));
    }
}


/**
 * Makes sure the XMM0..XMM15 and MXCSR state in CPUMCPU::Guest is up to date.
 *
 * This will not cause CPUM_USED_FPU_GUEST to change.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeSseForRead(PVMCPUCC pVCpu)
{
#if defined(VBOX_WITH_KERNEL_USING_XMM) && HC_ARCH_BITS == 64
    NOREF(pVCpu);
#else
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_GUEST)
    {
        cpumRZSaveGuestSseRegisters(&pVCpu->cpum.s);
        Log7(("CPUMRZFpuStateActualizeSseForRead\n"));
    }
#endif
}


/**
 * Makes sure the YMM0..YMM15 and MXCSR state in CPUMCPU::Guest is up to date.
 *
 * This will not cause CPUM_USED_FPU_GUEST to change.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeAvxForRead(PVMCPUCC pVCpu)
{
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_GUEST)
    {
        cpumRZSaveGuestAvxRegisters(&pVCpu->cpum.s);
        Log7(("CPUMRZFpuStateActualizeAvxForRead\n"));
    }
}

