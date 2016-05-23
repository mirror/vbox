/* $Id$ */
/** @file
 * CPUM - Raw-mode and ring-0 context.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
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
#include <VBox/vmm/vm.h>
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
VMMRZ_INT_DECL(void)    CPUMRZFpuStatePrepareHostCpuForUse(PVMCPU pVCpu)
{
    pVCpu->cpum.s.fChanged |= CPUM_CHANGED_FPU_REM;
    switch (pVCpu->cpum.s.fUseFlags & (CPUM_USED_FPU_GUEST | CPUM_USED_FPU_HOST))
    {
        case 0:
            cpumRZSaveHostFPUState(&pVCpu->cpum.s);
#ifdef IN_RC
            VMCPU_FF_SET(pVCpu, VMCPU_FF_CPUM); /* Must recalc CR0 before executing more code! */
#endif
            break;

        case CPUM_USED_FPU_HOST:
#if defined(IN_RING0) && ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
            if (pVCpu->cpum.s.fUseFlags | CPUM_SYNC_FPU_STATE)
            {
                pVCpu->cpum.s.fUseFlags &= ~CPUM_SYNC_FPU_STATE;
                HMR0NotifyCpumUnloadedGuestFpuState(pVCpu);
            }
#endif
            break;

        case CPUM_USED_FPU_GUEST | CPUM_USED_FPU_HOST:
#if defined(IN_RING0) && ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
            Assert(!(pVCpu->cpum.s.fUseFlags & CPUM_SYNC_FPU_STATE));
            if (CPUMIsGuestInLongModeEx(&pVCpu->cpum.s.Guest))
                HMR0SaveFPUState(pVCpu->CTX_SUFF(pVM), pVCpu, &pVCpu->cpum.s.Guest);
            else
#endif
                cpumRZSaveGuestFpuState(&pVCpu->cpum.s, true /*fLeaveFpuAccessible*/);
#ifdef IN_RING0
            HMR0NotifyCpumUnloadedGuestFpuState(pVCpu);
#endif
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
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeForChange(PVMCPU pVCpu)
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
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeForRead(PVMCPU pVCpu)
{
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_GUEST)
    {
#if defined(IN_RING0) && ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
        Assert(!(pVCpu->cpum.s.fUseFlags & CPUM_SYNC_FPU_STATE));
        if (CPUMIsGuestInLongModeEx(&pVCpu->cpum.s.Guest))
            HMR0SaveFPUState(pVCpu->CTX_SUFF(pVM), pVCpu, &pVCpu->cpum.s.Guest);
        else
#endif
            cpumRZSaveGuestFpuState(&pVCpu->cpum.s, false /*fLeaveFpuAccessible*/);
        pVCpu->cpum.s.fUseFlags |= CPUM_USED_FPU_GUEST;
    }
}


/**
 * Makes sure the XMM0..XMM15 state in CPUMCPU::Guest is up to date.
 *
 * This will not cause CPUM_USED_FPU_GUEST to change.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeSseForRead(PVMCPU pVCpu)
{
#if defined(VBOX_WITH_KERNEL_USING_XMM) && HC_ARCH_BITS == 64
    NOREF(pVCpu);
#error "do NOT commit this"
#else
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU_GUEST)
    {
# if defined(IN_RING0) && ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
        if (CPUMIsGuestInLongModeEx(&pVCpu->cpum.s.Guest))
        {
            Assert(!(pVCpu->cpum.s.fUseFlags & CPUM_SYNC_FPU_STATE));
            HMR0SaveFPUState(pVCpu->CTX_SUFF(pVM), pVCpu, &pVCpu->cpum.s.Guest);
            pVCpu->cpum.s.fUseFlags |= CPUM_USED_FPU_GUEST;
        }
        else
# endif
        {
RTLogPrintf("calling cpumRZSaveGuestSseRegisters\n");
            cpumRZSaveGuestSseRegisters(&pVCpu->cpum.s);
        }
    }
#endif
}

