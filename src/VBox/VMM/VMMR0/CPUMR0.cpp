/* $Id$ */
/** @file
 * CPUM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>
#include <VBox/x86.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>




/**
 * Does Ring-0 CPUM initialization.
 *
 * This is mainly to check that the Host CPU mode is compatible
 * with VBox.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
CPUMR0DECL(int) CPUMR0Init(PVM pVM)
{
    LogFlow(("CPUMR0Init: %p\n", pVM));

    /*
     * Check CR0 & CR4 flags.
     */
    uint32_t    u32CR0 = ASMGetCR0();
    if ((u32CR0 & (X86_CR0_PE | X86_CR0_PG)) != (X86_CR0_PE | X86_CR0_PG)) /* a bit paranoid perhaps.. */
    {
        Log(("CPUMR0Init: PE or PG not set. cr0=%#x\n", u32CR0));
        return VERR_UNSUPPORTED_CPU_MODE;
    }

    /*
     * Check for sysenter if it's used.
     */
    if (ASMHasCpuId())
    {
        uint32_t u32CpuVersion;
        uint32_t u32Dummy;
        uint32_t u32Features;
        ASMCpuId(1, &u32CpuVersion, &u32Dummy, &u32Dummy, &u32Features);
        uint32_t u32Family   = u32CpuVersion >> 8;
        uint32_t u32Model    = (u32CpuVersion >> 4) & 0xF;
        uint32_t u32Stepping = u32CpuVersion & 0xF;

        /*
         * Intel docs claim you should test both the flag and family, model & stepping.
         * Some Pentium Pro cpus have the SEP cpuid flag set, but don't support it.
         */
        if (    (u32Features & X86_CPUID_FEATURE_EDX_SEP)
            && !(u32Family == 6 && u32Model < 3 && u32Stepping < 3))
        {
            /*
             * Read the MSR and see if it's in use or not.
             */
            uint32_t    u32 = ASMRdMsr_Low(MSR_IA32_SYSENTER_CS);
            if (u32)
            {
                pVM->cpum.s.fUseFlags |= CPUM_USE_SYSENTER;
                Log(("CPUMR0Init: host uses sysenter cs=%08x%08x\n", ASMRdMsr_High(MSR_IA32_SYSENTER_CS), u32));
            }
        }

        /** @todo check for AMD and syscall!!!!!! */
    }


    /*
     * Check if debug registers are armed.
     */
    uint32_t    u32DR7 = ASMGetDR7();
    if (u32DR7 & X86_DR7_ENABLED_MASK)
    {
        pVM->cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS_HOST;
        Log(("CPUMR0Init: host uses debug registers (dr7=%x)\n", u32DR7));
    }

    return VINF_SUCCESS;
}


/**
 * Lazily sync in the FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pCtx        CPU context
 */
CPUMR0DECL(int) CPUMR0LoadGuestFPU(PVM pVM, PCPUMCTX pCtx)
{
    Assert(pVM->cpum.s.CPUFeatures.edx.u1FXSR);
    Assert(ASMGetCR4() & X86_CR4_OSFSXR);

    /* If the FPU state has already been loaded, then it's a guest trap. */
    if (pVM->cpum.s.fUseFlags & CPUM_USED_FPU)
    {
        Assert(    ((pCtx->cr0 & (X86_CR0_MP | X86_CR0_EM | X86_CR0_TS)) == (X86_CR0_MP | X86_CR0_EM | X86_CR0_TS))
               ||  ((pCtx->cr0 & (X86_CR0_MP | X86_CR0_EM | X86_CR0_TS)) == (X86_CR0_MP | X86_CR0_TS)));
        return VINF_EM_RAW_GUEST_TRAP;
    }

    /*
     * There are two basic actions:
     *   1. Save host fpu and restore guest fpu.
     *   2. Generate guest trap.
     *
     * When entering the hypervisor we'll always enable MP (for proper wait
     * trapping) and TS (for intercepting all fpu/mmx/sse stuff). The EM flag
     * is taken from the guest OS in order to get proper SSE handling.
     *
     *
     * Actions taken depending on the guest CR0 flags:
     *
     *   3    2    1
     *  TS | EM | MP | FPUInstr | WAIT :: VMM Action
     * ------------------------------------------------------------------------
     *   0 |  0 |  0 | Exec     | Exec :: Clear TS & MP, Save HC, Load GC.
     *   0 |  0 |  1 | Exec     | Exec :: Clear TS, Save HC, Load GC.
     *   0 |  1 |  0 | #NM      | Exec :: Clear TS & MP, Save HC, Load GC.
     *   0 |  1 |  1 | #NM      | Exec :: Clear TS, Save HC, Load GC.
     *   1 |  0 |  0 | #NM      | Exec :: Clear MP, Save HC, Load GC. (EM is already cleared.)
     *   1 |  0 |  1 | #NM      | #NM  :: Go to guest taking trap there.
     *   1 |  1 |  0 | #NM      | Exec :: Clear MP, Save HC, Load GC. (EM is already set.)
     *   1 |  1 |  1 | #NM      | #NM  :: Go to guest taking trap there.
     */

    switch(pCtx->cr0 & (X86_CR0_MP | X86_CR0_EM | X86_CR0_TS))
    {
    case X86_CR0_MP | X86_CR0_TS:
    case X86_CR0_MP | X86_CR0_EM | X86_CR0_TS:
        return VINF_EM_RAW_GUEST_TRAP;

    default:
        break;
    }
    CPUMLoadFPUAsm(pCtx);

    /* The MSR_K6_EFER_FFXSR feature is AMD only so far, but check the cpuid just in case Intel adds it in the future.
     *
     * MSR_K6_EFER_FFXSR changes the behaviour of fxsave and fxrstore: the XMM state isn't saved/restored
     *
     * If the guest uses the MSR_K6_EFER_FFXSR as well, then it has to manually restore the XMM state. 
     * In that case we don't care.
     *
     * We would end up saving/restoring too much if the guest has it enabled, but the host hasn't.
     */
    if (    (pVM->cpum.s.aGuestCpuIdExt[1].edx & X86_CPUID_AMD_FEATURE_EDX_FFXSR) /* 0x80000001 */
        &&  !(pCtx->msrEFER & MSR_K6_EFER_FFXSR))
    {
        /* @todo Do we really need to read this every time?? The host could change this on the fly though. */
        uint64_t msrEFERHost = ASMRdMsr(MSR_K6_EFER);

        if (msrEFERHost & MSR_K6_EFER_FFXSR)
        {
            /* fxrstor doesn't restore the XMM state! */
            CPUMLoadXMMAsm(pCtx);
            pVM->cpum.s.fUseFlags |= CPUM_MANUAL_XMM_RESTORE;
        }
    }

    pVM->cpum.s.fUseFlags |= CPUM_USED_FPU;
    return VINF_SUCCESS;
}


/**
 * Save guest FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pCtx        CPU context
 */
CPUMR0DECL(int) CPUMR0SaveGuestFPU(PVM pVM, PCPUMCTX pCtx)
{
    Assert(pVM->cpum.s.CPUFeatures.edx.u1FXSR);
    Assert(ASMGetCR4() & X86_CR4_OSFSXR);
    AssertReturn((pVM->cpum.s.fUseFlags & CPUM_USED_FPU), VINF_SUCCESS);

    CPUMSaveFPUAsm(pCtx);
    if (pVM->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
    {
        /* fxsave doesn't save the XMM state! */
        CPUMSaveXMMAsm(pCtx);
    }
    pVM->cpum.s.fUseFlags &= ~(CPUM_USED_FPU|CPUM_MANUAL_XMM_RESTORE);
    return VINF_SUCCESS;
}
