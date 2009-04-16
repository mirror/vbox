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
#include <VBox/hwaccm.h>
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
VMMR0DECL(int) CPUMR0Init(PVM pVM)
{
    LogFlow(("CPUMR0Init: %p\n", pVM));

    /*
     * Check CR0 & CR4 flags.
     */
    uint32_t u32CR0 = ASMGetCR0();
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
            uint32_t u32 = ASMRdMsr_Low(MSR_IA32_SYSENTER_CS);
            if (u32)
            {
                for (unsigned i=0;i<pVM->cCPUs;i++)
                    pVM->aCpus[i].cpum.s.fUseFlags |= CPUM_USE_SYSENTER;

                Log(("CPUMR0Init: host uses sysenter cs=%08x%08x\n", ASMRdMsr_High(MSR_IA32_SYSENTER_CS), u32));
            }
        }

        /** @todo check for AMD and syscall!!!!!! */
    }


    /*
     * Check if debug registers are armed.
     * This ASSUMES that DR7.GD is not set, or that it's handled transparently!
     */
    uint32_t u32DR7 = ASMGetDR7();
    if (u32DR7 & X86_DR7_ENABLED_MASK)
    {
        for (unsigned i=0;i<pVM->cCPUs;i++)
            pVM->aCpus[i].cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS_HOST;
        Log(("CPUMR0Init: host uses debug registers (dr7=%x)\n", u32DR7));
    }

    return VINF_SUCCESS;
}


/**
 * Lazily sync in the FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int) CPUMR0LoadGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    Assert(pVM->cpum.s.CPUFeatures.edx.u1FXSR);
    Assert(ASMGetCR4() & X86_CR4_OSFSXR);

    /* If the FPU state has already been loaded, then it's a guest trap. */
    if (pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU)
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

    switch (pCtx->cr0 & (X86_CR0_MP | X86_CR0_EM | X86_CR0_TS))
    {
        case X86_CR0_MP | X86_CR0_TS:
        case X86_CR0_MP | X86_CR0_EM | X86_CR0_TS:
            return VINF_EM_RAW_GUEST_TRAP;
        default:
            break;
    }

#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
        Assert(!(pVCpu->cpum.s.fUseFlags & CPUM_SYNC_FPU_STATE));

        /* Save the host state and record the fact (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM). */
        cpumR0SaveHostFPUState(&pVCpu->cpum.s);

        /* Restore the state on entry as we need to be in 64 bits mode to access the full state. */
        pVCpu->cpum.s.fUseFlags |= CPUM_SYNC_FPU_STATE;
    }
    else
#endif
    {
#ifndef CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL /** @todo remove the #else here and move cpumHandleLazyFPUAsm back to VMMGC after branching out 2.1. */
        /* Clear MSR_K6_EFER_FFXSR or else we'll be unable to save/restore the XMM state with fxsave/fxrstor. */
        uint64_t SavedEFER = 0;
        if (pVM->cpum.s.CPUFeaturesExt.edx & X86_CPUID_AMD_FEATURE_EDX_FFXSR)
        {
            SavedEFER = ASMRdMsr(MSR_K6_EFER);
            if (SavedEFER & MSR_K6_EFER_FFXSR)
            {
                ASMWrMsr(MSR_K6_EFER, SavedEFER & ~MSR_K6_EFER_FFXSR);
                pVCpu->cpum.s.fUseFlags |= CPUM_MANUAL_XMM_RESTORE;
            }
        }

        /* Do the job and record that we've switched FPU state. */
        cpumR0SaveHostRestoreGuestFPUState(&pVCpu->cpum.s);

        /* Restore EFER. */
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
            ASMWrMsr(MSR_K6_EFER, SavedEFER);

# else
        uint64_t oldMsrEFERHost = 0;
        uint32_t oldCR0 = ASMGetCR0();

        /* Clear MSR_K6_EFER_FFXSR or else we'll be unable to save/restore the XMM state with fxsave/fxrstor. */
        if (pVM->cpum.s.CPUFeaturesExt.edx & X86_CPUID_AMD_FEATURE_EDX_FFXSR)
        {
            /** @todo Do we really need to read this every time?? The host could change this on the fly though.
             *  bird: what about starting by skipping the ASMWrMsr below if we didn't
             *        change anything? Ditto for the stuff in CPUMR0SaveGuestFPU. */
            oldMsrEFERHost = ASMRdMsr(MSR_K6_EFER);
            if (oldMsrEFERHost & MSR_K6_EFER_FFXSR)
            {
                ASMWrMsr(MSR_K6_EFER, oldMsrEFERHost & ~MSR_K6_EFER_FFXSR);
                pVCpu->cpum.s.fUseFlags |= CPUM_MANUAL_XMM_RESTORE;
            }
        }

        /* If we sync the FPU/XMM state on-demand, then we can continue execution as if nothing has happened. */
        int rc = CPUMHandleLazyFPU(pVCpu);
        AssertRC(rc);
        Assert(CPUMIsGuestFPUStateActive(pVCpu));

        /* Restore EFER MSR */
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
            ASMWrMsr(MSR_K6_EFER, oldMsrEFERHost);

        /* CPUMHandleLazyFPU could have changed CR0; restore it. */
        ASMSetCR0(oldCR0);
# endif

#else  /* CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE */

        /*
         * Save the FPU control word and MXCSR, so we can restore the state properly afterwards.
         * We don't want the guest to be able to trigger floating point/SSE exceptions on the host.
         */
        pVCpu->cpum.s.Host.fpu.FCW = CPUMGetFCW();
        if (pVM->cpum.s.CPUFeatures.edx.u1SSE)
            pVCpu->cpum.s.Host.fpu.MXCSR = CPUMGetMXCSR();

        cpumR0LoadFPU(pCtx);

        /*
         * The MSR_K6_EFER_FFXSR feature is AMD only so far, but check the cpuid just in case Intel adds it in the future.
         *
         * MSR_K6_EFER_FFXSR changes the behaviour of fxsave and fxrstore: the XMM state isn't saved/restored
         */
        if (pVM->cpum.s.CPUFeaturesExt.edx & X86_CPUID_AMD_FEATURE_EDX_FFXSR)
        {
            /** @todo Do we really need to read this every time?? The host could change this on the fly though. */
            uint64_t msrEFERHost = ASMRdMsr(MSR_K6_EFER);

            if (msrEFERHost & MSR_K6_EFER_FFXSR)
            {
                /* fxrstor doesn't restore the XMM state! */
                cpumR0LoadXMM(pCtx);
                pVCpu->cpum.s.fUseFlags |= CPUM_MANUAL_XMM_RESTORE;
            }
        }

#endif /* CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE */
    }

    Assert((pVCpu->cpum.s.fUseFlags & (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM)) == (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM));
    return VINF_SUCCESS;
}


/**
 * Save guest FPU/XMM state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int) CPUMR0SaveGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    Assert(pVM->cpum.s.CPUFeatures.edx.u1FXSR);
    Assert(ASMGetCR4() & X86_CR4_OSFSXR);
    AssertReturn((pVCpu->cpum.s.fUseFlags & CPUM_USED_FPU), VINF_SUCCESS);

#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
        if (!(pVCpu->cpum.s.fUseFlags & CPUM_SYNC_FPU_STATE))
        {
            HWACCMR0SaveFPUState(pVM, pVCpu, pCtx);
            cpumR0RestoreHostFPUState(&pVCpu->cpum.s);
        }
        /* else nothing to do; we didn't perform a world switch */
    }
    else
#endif
    {
#ifndef CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE
        uint64_t oldMsrEFERHost = 0;

        /* Clear MSR_K6_EFER_FFXSR or else we'll be unable to save/restore the XMM state with fxsave/fxrstor. */
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
        {
            oldMsrEFERHost = ASMRdMsr(MSR_K6_EFER);
            ASMWrMsr(MSR_K6_EFER, oldMsrEFERHost & ~MSR_K6_EFER_FFXSR);
        }
        cpumR0SaveGuestRestoreHostFPUState(&pVCpu->cpum.s);

        /* Restore EFER MSR */
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
            ASMWrMsr(MSR_K6_EFER, oldMsrEFERHost | MSR_K6_EFER_FFXSR);

#else  /* CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE */
        cpumR0SaveFPU(pCtx);
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
        {
            /* fxsave doesn't save the XMM state! */
            cpumR0SaveXMM(pCtx);
        }

        /*
         * Restore the original FPU control word and MXCSR.
         * We don't want the guest to be able to trigger floating point/SSE exceptions on the host.
         */
        cpumR0SetFCW(pVCpu->cpum.s.Host.fpu.FCW);
        if (pVM->cpum.s.CPUFeatures.edx.u1SSE)
            cpumR0SetMXCSR(pVCpu->cpum.s.Host.fpu.MXCSR);
#endif /* CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE */
    }

    pVCpu->cpum.s.fUseFlags &= ~(CPUM_USED_FPU | CPUM_SYNC_FPU_STATE | CPUM_MANUAL_XMM_RESTORE);
    return VINF_SUCCESS;
}


/**
 * Save guest debug state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 * @param   fDR6        Include DR6 or not
 */
VMMR0DECL(int) CPUMR0SaveGuestDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6)
{
    Assert(pVCpu->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS);

    /* Save the guest's debug state. The caller is responsible for DR7. */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
        if (!(pVCpu->cpum.s.fUseFlags & CPUM_SYNC_DEBUG_STATE))
        {
            uint64_t dr6 = pCtx->dr[6];

            HWACCMR0SaveDebugState(pVM, pVCpu, pCtx);
            if (!fDR6) /* dr6 was already up-to-date */
                pCtx->dr[6] = dr6;
        }
    }
    else
#endif
    {
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        cpumR0SaveDRx(&pCtx->dr[0]);
#else
        pCtx->dr[0] = ASMGetDR0();
        pCtx->dr[1] = ASMGetDR1();
        pCtx->dr[2] = ASMGetDR2();
        pCtx->dr[3] = ASMGetDR3();
#endif
        if (fDR6)
            pCtx->dr[6] = ASMGetDR6();
    }

    /*
     * Restore the host's debug state. DR0-3, DR6 and only then DR7!
     * DR7 contains 0x400 right now.
     */
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    AssertCompile((uintptr_t)&pVCpu->cpum.s.Host.dr3 - (uintptr_t)&pVCpu->cpum.s.Host.dr0 == sizeof(uint64_t) * 3);
    cpumR0LoadDRx(&pVCpu->cpum.s.Host.dr0);
#else
    ASMSetDR0(pVCpu->cpum.s.Host.dr0);
    ASMSetDR1(pVCpu->cpum.s.Host.dr1);
    ASMSetDR2(pVCpu->cpum.s.Host.dr2);
    ASMSetDR3(pVCpu->cpum.s.Host.dr3);
#endif
    ASMSetDR6(pVCpu->cpum.s.Host.dr6);
    ASMSetDR7(pVCpu->cpum.s.Host.dr7);

    pVCpu->cpum.s.fUseFlags &= ~CPUM_USE_DEBUG_REGS;
    return VINF_SUCCESS;
}


/**
 * Lazily sync in the debug state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 * @param   fDR6        Include DR6 or not
 */
VMMR0DECL(int) CPUMR0LoadGuestDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6)
{
    /* Save the host state. */
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    AssertCompile((uintptr_t)&pVCpu->cpum.s.Host.dr3 - (uintptr_t)&pVCpu->cpum.s.Host.dr0 == sizeof(uint64_t) * 3);
    cpumR0SaveDRx(&pVCpu->cpum.s.Host.dr0);
#else
    pVCpu->cpum.s.Host.dr0 = ASMGetDR0();
    pVCpu->cpum.s.Host.dr1 = ASMGetDR1();
    pVCpu->cpum.s.Host.dr2 = ASMGetDR2();
    pVCpu->cpum.s.Host.dr3 = ASMGetDR3();
#endif
    pVCpu->cpum.s.Host.dr6 = ASMGetDR6();
    /** @todo dr7 might already have been changed to 0x400; don't care right now as it's harmless. */
    pVCpu->cpum.s.Host.dr7 = ASMGetDR7();
    /* Make sure DR7 is harmless or else we could trigger breakpoints when restoring dr0-3 (!) */
    ASMSetDR7(X86_DR7_INIT_VAL);

    /* Activate the guest state DR0-3; DR7 is left to the caller. */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
        /* Restore the state on entry as we need to be in 64 bits mode to access the full state. */
        pVCpu->cpum.s.fUseFlags |= CPUM_SYNC_DEBUG_STATE;
    }
    else
#endif
    {
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        cpumR0LoadDRx(&pCtx->dr[0]);
#else
        ASMSetDR0(pCtx->dr[0]);
        ASMSetDR1(pCtx->dr[1]);
        ASMSetDR2(pCtx->dr[2]);
        ASMSetDR3(pCtx->dr[3]);
#endif
        if (fDR6)
            ASMSetDR6(pCtx->dr[6]);
    }

    pVCpu->cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS;
    return VINF_SUCCESS;
}


