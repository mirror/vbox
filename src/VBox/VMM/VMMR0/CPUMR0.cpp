/* $Id$ */
/** @file
 * CPUM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/hwaccm.h>
#include <iprt/assert.h>
#include <iprt/asm-amd64-x86.h>
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
# include <iprt/mem.h>
# include <iprt/memobj.h>
# include <VBox/apic.h>
#endif
#include <iprt/x86.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
/**
 * Local APIC mappings.
 */
typedef struct CPUMHOSTLAPIC
{
    /** Indicates that the entry is in use and have valid data. */
    bool        fEnabled;
    /** Has APIC_REG_LVT_THMR. Not used. */
    uint32_t    fHasThermal;
    /** The physical address of the APIC registers. */
    RTHCPHYS    PhysBase;
    /** The memory object entering the physical address. */
    RTR0MEMOBJ  hMemObj;
    /** The mapping object for hMemObj. */
    RTR0MEMOBJ  hMapObj;
    /** The mapping address APIC registers.
     * @remarks Different CPUs may use the same physical address to map their
     *          APICs, so this pointer is only valid when on the CPU owning the
     *          APIC. */
    void       *pv;
} CPUMHOSTLAPIC;
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
static CPUMHOSTLAPIC g_aLApics[RTCPUSET_MAX_CPUS];
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
static int  cpumR0MapLocalApics(void);
static void cpumR0UnmapLocalApics(void);
#endif


/**
 * Does the Ring-0 CPU initialization once during module load.
 * XXX Host-CPU hot-plugging?
 */
VMMR0DECL(int) CPUMR0ModuleInit(void)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    rc = cpumR0MapLocalApics();
#endif
    return rc;
}


/**
 * Terminate the module.
 */
VMMR0DECL(int) CPUMR0ModuleTerm(void)
{
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    cpumR0UnmapLocalApics();
#endif
    return VINF_SUCCESS;
}


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
     * Check for sysenter and syscall usage.
     */
    if (ASMHasCpuId())
    {
        /*
         * SYSENTER/SYSEXIT
         *
         * Intel docs claim you should test both the flag and family, model &
         * stepping because some Pentium Pro CPUs have the SEP cpuid flag set,
         * but don't support it.  AMD CPUs may support this feature in legacy
         * mode, they've banned it from long mode.  Since we switch to 32-bit
         * mode when entering raw-mode context the feature would become
         * accessible again on AMD CPUs, so we have to check regardless of
         * host bitness.
         */
        uint32_t u32CpuVersion;
        uint32_t u32Dummy;
        uint32_t fFeatures;
        ASMCpuId(1, &u32CpuVersion, &u32Dummy, &u32Dummy, &fFeatures);
        uint32_t u32Family   = u32CpuVersion >> 8;
        uint32_t u32Model    = (u32CpuVersion >> 4) & 0xF;
        uint32_t u32Stepping = u32CpuVersion & 0xF;
        if (    (fFeatures & X86_CPUID_FEATURE_EDX_SEP)
            &&  (   u32Family   != 6    /* (> pentium pro) */
                 || u32Model    >= 3
                 || u32Stepping >= 3
                 || !ASMIsIntelCpu())
           )
        {
            /*
             * Read the MSR and see if it's in use or not.
             */
            uint32_t u32 = ASMRdMsr_Low(MSR_IA32_SYSENTER_CS);
            if (u32)
            {
                pVM->cpum.s.fHostUseFlags |= CPUM_USE_SYSENTER;
                Log(("CPUMR0Init: host uses sysenter cs=%08x%08x\n", ASMRdMsr_High(MSR_IA32_SYSENTER_CS), u32));
            }
        }

        /*
         * SYSCALL/SYSRET
         *
         * This feature is indicated by the SEP bit returned in EDX by CPUID
         * function 0x80000001.  Intel CPUs only supports this feature in
         * long mode.  Since we're not running 64-bit guests in raw-mode there
         * are no issues with 32-bit intel hosts.
         */
        uint32_t cExt = 0;
        ASMCpuId(0x80000000, &cExt, &u32Dummy, &u32Dummy, &u32Dummy);
        if (    cExt >= 0x80000001
            &&  cExt <= 0x8000ffff)
        {
            uint32_t fExtFeaturesEDX = ASMCpuId_EDX(0x80000001);
            if (fExtFeaturesEDX & X86_CPUID_AMD_FEATURE_EDX_SEP)
            {
#ifdef RT_ARCH_X86
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
                if (fExtFeaturesEDX & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE)
# else
                if (!ASMIsIntelCpu())
# endif
#endif
                {
                    uint64_t fEfer = ASMRdMsr(MSR_K6_EFER);
                    if (fEfer & MSR_K6_EFER_SCE)
                    {
                        pVM->cpum.s.fHostUseFlags |= CPUM_USE_SYSCALL;
                        Log(("CPUMR0Init: host uses syscall\n"));
                    }
                }
            }
        }
    }


    /*
     * Check if debug registers are armed.
     * This ASSUMES that DR7.GD is not set, or that it's handled transparently!
     */
    uint32_t u32DR7 = ASMGetDR7();
    if (u32DR7 & X86_DR7_ENABLED_MASK)
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
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
# if defined(VBOX_WITH_HYBRID_32BIT_KERNEL) || defined(VBOX_WITH_KERNEL_USING_XMM) /** @todo remove the #else here and move cpumHandleLazyFPUAsm back to VMMGC after branching out 3.0!!. */
        Assert(!(pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE));
        /** @todo Move the FFXR handling down into
         *        cpumR0SaveHostRestoreguestFPUState to optimize the
         *        VBOX_WITH_KERNEL_USING_XMM handling. */
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
    NOREF(pCtx);

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
# ifdef VBOX_WITH_KERNEL_USING_XMM
        /*
         * We've already saved the XMM registers in the assembly wrapper, so
         * we have to save them before saving the entire FPU state and put them
         * back afterwards.
         */
        /** @todo This could be skipped if MSR_K6_EFER_FFXSR is set, but
         *        I'm not able to test such an optimization tonight.
         *        We could just all this in assembly. */
        uint128_t aGuestXmmRegs[16];
        memcpy(&aGuestXmmRegs[0], &pVCpu->cpum.s.Guest.fpu.aXMM[0], sizeof(aGuestXmmRegs));
# endif

        /* Clear MSR_K6_EFER_FFXSR or else we'll be unable to save/restore the XMM state with fxsave/fxrstor. */
        uint64_t oldMsrEFERHost = 0;
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
        {
            oldMsrEFERHost = ASMRdMsr(MSR_K6_EFER);
            ASMWrMsr(MSR_K6_EFER, oldMsrEFERHost & ~MSR_K6_EFER_FFXSR);
        }
        cpumR0SaveGuestRestoreHostFPUState(&pVCpu->cpum.s);

        /* Restore EFER MSR */
        if (pVCpu->cpum.s.fUseFlags & CPUM_MANUAL_XMM_RESTORE)
            ASMWrMsr(MSR_K6_EFER, oldMsrEFERHost | MSR_K6_EFER_FFXSR);

# ifdef VBOX_WITH_KERNEL_USING_XMM
        memcpy(&pVCpu->cpum.s.Guest.fpu.aXMM[0], &aGuestXmmRegs[0], sizeof(aGuestXmmRegs));
# endif

#else  /* CPUM_CAN_HANDLE_NM_TRAPS_IN_KERNEL_MODE */
# ifdef VBOX_WITH_KERNEL_USING_XMM
#  error "Fix all the NM_TRAPS_IN_KERNEL_MODE code path. I'm not going to fix unused code now."
# endif
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
    CPUMR0LoadHostDebugState(pVM, pVCpu);
    Assert(!(pVCpu->cpum.s.fUseFlags & CPUM_USE_DEBUG_REGS));
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
    CPUMR0SaveHostDebugState(pVM, pVCpu);
    Assert(ASMGetDR7() == X86_DR7_INIT_VAL);

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

/**
 * Save the host debug state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 */
VMMR0DECL(int) CPUMR0SaveHostDebugState(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);

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

    return VINF_SUCCESS;
}

/**
 * Load the host debug state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 */
VMMR0DECL(int) CPUMR0LoadHostDebugState(PVM pVM, PVMCPU pVCpu)
{
    Assert(pVCpu->cpum.s.fUseFlags & (CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HYPER));
    NOREF(pVM);

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

    pVCpu->cpum.s.fUseFlags &= ~(CPUM_USE_DEBUG_REGS | CPUM_USE_DEBUG_REGS_HYPER);
    return VINF_SUCCESS;
}


/**
 * Lazily sync in the hypervisor debug state
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 * @param   fDR6        Include DR6 or not
 */
VMMR0DECL(int) CPUMR0LoadHyperDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, bool fDR6)
{
    NOREF(pCtx);

    /* Save the host state. */
    CPUMR0SaveHostDebugState(pVM, pVCpu);
    Assert(ASMGetDR7() == X86_DR7_INIT_VAL);

    /* Activate the guest state DR0-3; DR7 is left to the caller. */
#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
        AssertFailed();
        return VERR_NOT_IMPLEMENTED;
    }
    else
#endif
    {
#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
        AssertFailed();
        return VERR_NOT_IMPLEMENTED;
#else
        ASMSetDR0(CPUMGetHyperDR0(pVCpu));
        ASMSetDR1(CPUMGetHyperDR1(pVCpu));
        ASMSetDR2(CPUMGetHyperDR2(pVCpu));
        ASMSetDR3(CPUMGetHyperDR3(pVCpu));
#endif
        if (fDR6)
            ASMSetDR6(CPUMGetHyperDR6(pVCpu));
    }

    pVCpu->cpum.s.fUseFlags |= CPUM_USE_DEBUG_REGS_HYPER;
    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI

/**
 * Worker for cpumR0MapLocalApics. Check each CPU for a present Local APIC.
 * Play safe and treat each CPU separate.
 */
static void cpumR0MapLocalApicWorker(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    NOREF(pvUser1); NOREF(pvUser2);
    int iCpu = RTMpCpuIdToSetIndex(idCpu);
    AssertReturnVoid(iCpu >= 0 && (unsigned)iCpu < RT_ELEMENTS(g_aLApics));

    uint32_t u32MaxIdx, u32EBX, u32ECX, u32EDX;
    ASMCpuId(0, &u32MaxIdx, &u32EBX, &u32ECX, &u32EDX);
    if (   (   (   u32EBX == X86_CPUID_VENDOR_INTEL_EBX
                && u32ECX == X86_CPUID_VENDOR_INTEL_ECX
                && u32EDX == X86_CPUID_VENDOR_INTEL_EDX)
           ||  (   u32EBX == X86_CPUID_VENDOR_AMD_EBX
                && u32ECX == X86_CPUID_VENDOR_AMD_ECX
                && u32EDX == X86_CPUID_VENDOR_AMD_EDX))
        && u32MaxIdx >= 1)
    {
        ASMCpuId(1, &u32MaxIdx, &u32EBX, &u32ECX, &u32EDX);
        if (    (u32EDX & X86_CPUID_FEATURE_EDX_APIC)
            &&  (u32EDX & X86_CPUID_FEATURE_EDX_MSR))
        {
            uint64_t u64ApicBase = ASMRdMsr(MSR_IA32_APICBASE);
            uint64_t u64Mask     = UINT64_C(0x0000000ffffff000);

            /* see Intel Manual: Local APIC Status and Location: MAXPHYADDR default is bit 36 */
            uint32_t u32MaxExtIdx;
            ASMCpuId(0x80000000, &u32MaxExtIdx, &u32EBX, &u32ECX, &u32EDX);
            if (   u32MaxExtIdx >= UINT32_C(0x80000008)
                && u32MaxExtIdx <  UINT32_C(0x8000ffff))
            {
                uint32_t u32PhysBits;
                ASMCpuId(0x80000008, &u32PhysBits, &u32EBX, &u32ECX, &u32EDX);
                u32PhysBits &= 0xff;
                u64Mask = ((UINT64_C(1) << u32PhysBits) - 1) & UINT64_C(0xfffffffffffff000);
            }

            uint64_t const u64PhysBase = u64ApicBase & u64Mask;
            g_aLApics[iCpu].PhysBase   = (RTHCPHYS)u64PhysBase;
            g_aLApics[iCpu].fEnabled   = g_aLApics[iCpu].PhysBase == u64PhysBase;
        }
    }
}


/**
 * Map the MMIO page of each local APIC in the system.
 */
static int cpumR0MapLocalApics(void)
{
    /*
     * Check that we'll always stay within the array bounds.
     */
    if (RTMpGetArraySize() > RT_ELEMENTS(g_aLApics))
    {
        LogRel(("CPUM: Too many real CPUs/cores/threads - %u, max %u\n", RTMpGetArraySize(), RT_ELEMENTS(g_aLApics)));
        return VERR_TOO_MANY_CPUS;
    }

    /*
     * Create mappings for all online CPUs we think have APICs.
     */
    /** @todo r=bird: This code is not adequately handling CPUs that are
     *        offline or unplugged at init time and later bought into action. */
    int rc = RTMpOnAll(cpumR0MapLocalApicWorker, NULL, NULL);

    for (unsigned iCpu = 0; RT_SUCCESS(rc) && iCpu < RT_ELEMENTS(g_aLApics); iCpu++)
    {
        if (g_aLApics[iCpu].fEnabled)
        {
            rc = RTR0MemObjEnterPhys(&g_aLApics[iCpu].hMemObj, g_aLApics[iCpu].PhysBase,
                                     PAGE_SIZE, RTMEM_CACHE_POLICY_MMIO);
            if (RT_SUCCESS(rc))
            {
                rc = RTR0MemObjMapKernel(&g_aLApics[iCpu].hMapObj, g_aLApics[iCpu].hMemObj, (void *)-1,
                                         PAGE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
                if (RT_SUCCESS(rc))
                {
                    void *pvApicBase = RTR0MemObjAddress(g_aLApics[iCpu].hMapObj);

                    /*
                     * 0x0X       82489 external APIC
                     * 0x1X       Local APIC
                     * 0x2X..0xFF reserved
                     */
                    /** @todo r=bird: The local APIC is usually at the same address for all CPUs,
                     *        and therefore inaccessible by the other CPUs. */
                    uint32_t ApicVersion = ApicRegRead(pvApicBase, APIC_REG_VERSION);
                    if ((APIC_REG_VERSION_GET_VER(ApicVersion) & 0xF0) == 0x10)
                    {
                        g_aLApics[iCpu].fHasThermal = APIC_REG_VERSION_GET_MAX_LVT(ApicVersion) >= 5;
                        g_aLApics[iCpu].pv          = pvApicBase;
                        Log(("CPUM: APIC %02u at %RGp (mapped at %p) - ver %#x, lint0=%#x lint1=%#x pc=%#x thmr=%#x\n",
                             iCpu, g_aLApics[iCpu].PhysBase, g_aLApics[iCpu].pv, ApicVersion,
                             ApicRegRead(pvApicBase, APIC_REG_LVT_LINT0),
                             ApicRegRead(pvApicBase, APIC_REG_LVT_LINT1),
                             ApicRegRead(pvApicBase, APIC_REG_LVT_PC),
                             ApicRegRead(pvApicBase, APIC_REG_LVT_THMR)
                             ));
                        continue;
                    }

                    RTR0MemObjFree(g_aLApics[iCpu].hMapObj, true /* fFreeMappings */);
                }
                RTR0MemObjFree(g_aLApics[iCpu].hMemObj, true /* fFreeMappings */);
            }
            g_aLApics[iCpu].fEnabled = false;
        }
    }
    if (RT_FAILURE(rc))
    {
        cpumR0UnmapLocalApics();
        return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Unmap the Local APIC of all host CPUs.
 */
static void cpumR0UnmapLocalApics(void)
{
    for (unsigned iCpu = RT_ELEMENTS(g_aLApics); iCpu-- > 0;)
    {
        if (g_aLApics[iCpu].pv)
        {
            RTR0MemObjFree(g_aLApics[iCpu].hMapObj, true /* fFreeMappings */);
            RTR0MemObjFree(g_aLApics[iCpu].hMemObj, true /* fFreeMappings */);
            g_aLApics[iCpu].hMapObj  = NIL_RTR0MEMOBJ;
            g_aLApics[iCpu].hMemObj  = NIL_RTR0MEMOBJ;
            g_aLApics[iCpu].fEnabled = false;
            g_aLApics[iCpu].pv       = NULL;
        }
    }
}


/**
 * Write the Local APIC mapping address of the current host CPU to CPUM to be
 * able to access the APIC registers in the raw mode switcher for disabling/
 * re-enabling the NMI. Must be called with disabled preemption or disabled
 * interrupts!
 *
 * @param   pVM         VM handle.
 * @param   idHostCpu   The ID of the current host CPU.
 */
VMMR0DECL(void) CPUMR0SetLApic(PVM pVM, RTCPUID idHostCpu)
{
    pVM->cpum.s.pvApicBase = g_aLApics[RTMpCpuIdToSetIndex(idHostCpu)].pv;
}

#endif /* VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI */

