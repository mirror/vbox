/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, KVM, All Contexts.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
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
#include "GIMKvmInternal.h"
#include "GIMInternal.h"

#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/sup.h>

#include <iprt/asm-amd64-x86.h>


/**
 * Handles the KVM hypercall.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pCtx            Pointer to the guest-CPU context.
 */
VMM_INT_DECL(int) gimKvmHypercall(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /*
     * Get the hypercall operation and arguments.
     */
    bool const fIs64BitMode = CPUMIsGuestIn64BitCodeEx(pCtx);
    uint64_t uHyperOp       = pCtx->rax;
    uint64_t uHyperArg0     = pCtx->rbx;
    uint64_t uHyperArg1     = pCtx->rcx;
    uint64_t uHyperArg2     = pCtx->rdi;
    uint64_t uHyperArg3     = pCtx->rsi;
    uint64_t uHyperRet      = KVM_HYPERCALL_RET_ENOSYS;
    uint64_t uAndMask       = UINT64_C(0xffffffffffffffff);
    if (!fIs64BitMode)
    {
        uAndMask    = UINT64_C(0xffffffff);
        uHyperOp   &= UINT64_C(0xffffffff);
        uHyperArg0 &= UINT64_C(0xffffffff);
        uHyperArg1 &= UINT64_C(0xffffffff);
        uHyperArg2 &= UINT64_C(0xffffffff);
        uHyperArg3 &= UINT64_C(0xffffffff);
        uHyperRet  &= UINT64_C(0xffffffff);
    }

    /*
     * Verify that guest ring-0 is the one making the hypercall.
     */
    uint32_t uCpl = CPUMGetGuestCPL(pVCpu);
    if (uCpl)
    {
        pCtx->rax = KVM_HYPERCALL_RET_EPERM & uAndMask;
        return VINF_SUCCESS;
    }

    /*
     * Do the work.
     */
    switch (uHyperOp)
    {
        case KVM_HYPERCALL_OP_KICK_CPU:
        {
            PVM pVM = pVCpu->CTX_SUFF(pVM);
            if (uHyperArg1 < pVM->cCpus)
            {
                PVMCPU pVCpuTarget = &pVM->aCpus[uHyperArg1];   /** ASSUMES pVCpu index == ApicId of the VCPU. */
                VMCPU_FF_SET(pVCpuTarget, VMCPU_FF_UNHALT);
#ifdef IN_RING0
                GVMMR0SchedWakeUp(pVM, pVCpuTarget->idCpu);
#elif defined(IN_RING3)
                int rc2 = SUPR3CallVMMR0(pVM->pVMR0, pVCpuTarget->idCpu, VMMR0_DO_GVMM_SCHED_WAKE_UP, NULL);
                AssertRC(rc2);
#endif
                uHyperRet = KVM_HYPERCALL_RET_SUCCESS;
            }
            break;
        }

        case KVM_HYPERCALL_OP_VAPIC_POLL_IRQ:
            uHyperRet = KVM_HYPERCALL_RET_SUCCESS;
            break;

        default:
            break;
    }

    /*
     * Place the result in rax/eax.
     */
    pCtx->rax = uHyperRet & uAndMask;
    return VINF_SUCCESS;
}


/**
 * Returns whether the guest has configured and enabled the use of KVM's
 * hypercall interface.
 *
 * @returns true if hypercalls are enabled, false otherwise.
 * @param   pVCpu       Pointer to the VMCPU.
 */
VMM_INT_DECL(bool) gimKvmAreHypercallsEnabled(PVMCPU pVCpu)
{
    /* KVM paravirt interface doesn't have hypercall control bits like Hyper-V does
       that guests can control. It's always enabled. */
    return true;
}


/**
 * Returns whether the guest has configured and enabled the use of KVM's
 * paravirtualized TSC.
 *
 * @returns true if paravirt. TSC is enabled, false otherwise.
 * @param   pVM     Pointer to the VM.
 */
VMM_INT_DECL(bool) gimKvmIsParavirtTscEnabled(PVM pVM)
{
    uint32_t cCpus = pVM->cCpus;
    for (uint32_t i = 0; i < cCpus; i++)
    {
        PVMCPU     pVCpu      = &pVM->aCpus[i];
        PGIMKVMCPU pGimKvmCpu = &pVCpu->gim.s.u.KvmCpu;
        if (MSR_GIM_KVM_SYSTEM_TIME_IS_ENABLED(pGimKvmCpu->u64SystemTimeMsr))
            return true;
    }
    return false;
}


/**
 * MSR read handler for KVM.
 *
 * @returns Strict VBox status code like CPUMQueryGuestMsr().
 * @retval  VINF_CPUM_R3_MSR_READ
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR being read.
 * @param   pRange      The range this MSR belongs to.
 * @param   puValue     Where to store the MSR value read.
 */
VMM_INT_DECL(VBOXSTRICTRC) gimKvmReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue)
{
    NOREF(pRange);
    PVM     pVM        = pVCpu->CTX_SUFF(pVM);
    PGIMKVM pKvm       = &pVM->gim.s.u.Kvm;
    PGIMKVMCPU pKvmCpu = &pVCpu->gim.s.u.KvmCpu;

    switch (idMsr)
    {
        case MSR_GIM_KVM_SYSTEM_TIME:
        case MSR_GIM_KVM_SYSTEM_TIME_OLD:
            *puValue = pKvmCpu->u64SystemTimeMsr;
            return VINF_SUCCESS;

        case MSR_GIM_KVM_WALL_CLOCK:
        case MSR_GIM_KVM_WALL_CLOCK_OLD:
            *puValue = pKvm->u64WallClockMsr;
            return VINF_SUCCESS;

        default:
        {
#ifdef IN_RING3
            static uint32_t s_cTimes = 0;
            if (s_cTimes++ < 20)
                LogRel(("GIM: KVM: Unknown/invalid RdMsr (%#x) -> #GP(0)\n", idMsr));
#endif
            LogFunc(("Unknown/invalid RdMsr (%#RX32) -> #GP(0)\n", idMsr));
            break;
        }
    }

    return VERR_CPUM_RAISE_GP_0;
}


/**
 * MSR write handler for KVM.
 *
 * @returns Strict VBox status code like CPUMSetGuestMsr().
 * @retval  VINF_CPUM_R3_MSR_WRITE
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR being written.
 * @param   pRange      The range this MSR belongs to.
 * @param   uRawValue   The raw value with the ignored bits not masked.
 */
VMM_INT_DECL(VBOXSTRICTRC) gimKvmWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uRawValue)
{
    NOREF(pRange);
    PVM     pVM  = pVCpu->CTX_SUFF(pVM);
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    PGIMKVMCPU pKvmCpu = &pVCpu->gim.s.u.KvmCpu;

    switch (idMsr)
    {
        case MSR_GIM_KVM_SYSTEM_TIME:
        case MSR_GIM_KVM_SYSTEM_TIME_OLD:
        {
            bool fEnable = RT_BOOL(uRawValue & MSR_GIM_KVM_SYSTEM_TIME_ENABLE_BIT);
#ifndef IN_RING3
            if (fEnable)
            {
                RTCCUINTREG fEFlags  = ASMIntDisableFlags();
                pKvmCpu->uTsc        = TMCpuTickGetNoCheck(pVCpu);
                pKvmCpu->uVirtNanoTS = TMVirtualGetNoCheck(pVM);
                ASMSetFlags(fEFlags);
            }
            return VINF_CPUM_R3_MSR_WRITE;
#else
            if (!fEnable)
            {
                gimR3KvmDisableSystemTime(pVM);
                pKvmCpu->u64SystemTimeMsr = uRawValue;
                return VINF_SUCCESS;
            }

            /* Is the system-time struct. already enabled? If so, get flags that need preserving. */
            uint8_t fFlags = 0;
            GIMKVMSYSTEMTIME SystemTime;
            RT_ZERO(SystemTime);
            if (   MSR_GIM_KVM_SYSTEM_TIME_IS_ENABLED(pKvmCpu->u64SystemTimeMsr)
                && MSR_GIM_KVM_SYSTEM_TIME_GUEST_GPA(uRawValue) == pKvmCpu->GCPhysSystemTime)
            {
                int rc2 = PGMPhysSimpleReadGCPhys(pVM, &SystemTime, pKvmCpu->GCPhysSystemTime, sizeof(GIMKVMSYSTEMTIME));
                if (RT_SUCCESS(rc2))
                    fFlags = (SystemTime.fFlags & GIM_KVM_SYSTEM_TIME_FLAGS_GUEST_PAUSED);
            }

            /* Enable and populate the system-time struct. */
            pKvmCpu->u64SystemTimeMsr     = uRawValue;
            pKvmCpu->GCPhysSystemTime     = MSR_GIM_KVM_SYSTEM_TIME_GUEST_GPA(uRawValue);
            pKvmCpu->u32SystemTimeVersion = pKvmCpu->u32SystemTimeVersion + 2;
            int rc = gimR3KvmEnableSystemTime(pVM, pVCpu, pKvmCpu, fFlags);
            if (RT_FAILURE(rc))
            {
                pKvmCpu->u64SystemTimeMsr = 0;
                return VERR_CPUM_RAISE_GP_0;
            }
            return VINF_SUCCESS;
#endif /* IN_RING3 */
        }

        case MSR_GIM_KVM_WALL_CLOCK:
        case MSR_GIM_KVM_WALL_CLOCK_OLD:
        {
#ifndef IN_RING3

            return VINF_CPUM_R3_MSR_WRITE;
#else
            /* Enable the wall-clock struct. */
            RTGCPHYS GCPhysWallClock = MSR_GIM_KVM_WALL_CLOCK_GUEST_GPA(uRawValue);
            if (RT_LIKELY(RT_ALIGN_64(GCPhysWallClock, 4) == GCPhysWallClock))
            {
                uint32_t uVersion = 2;
                int rc = gimR3KvmEnableWallClock(pVM, GCPhysWallClock, uVersion);
                if (RT_SUCCESS(rc))
                {
                    pKvm->u64WallClockMsr     = uRawValue;
                    return VINF_SUCCESS;
                }
            }
            return VERR_CPUM_RAISE_GP_0;
#endif /* IN_RING3 */
        }

        default:
        {
#ifdef IN_RING3
            static uint32_t s_cTimes = 0;
            if (s_cTimes++ < 20)
                LogRel(("GIM: KVM: Unknown/invalid WrMsr (%#x,%#x`%08x) -> #GP(0)\n", idMsr,
                        uRawValue & UINT64_C(0xffffffff00000000), uRawValue & UINT64_C(0xffffffff)));
#endif
            LogFunc(("Unknown/invalid WrMsr (%#RX32,%#RX64) -> #GP(0)\n", idMsr, uRawValue));
            break;
        }
    }

    return VERR_CPUM_RAISE_GP_0;
}


/**
 * Whether we need to trap #UD exceptions in the guest.
 *
 * On AMD-V we need to trap them because paravirtualized Linux/KVM guests use
 * the Intel VMCALL instruction to make hypercalls and we need to trap and
 * optionally patch them to the AMD-V VMMCALL instruction and handle the
 * hypercall.
 *
 * I guess this was done so that guest teleporation between an AMD and an Intel
 * machine would working without any changes at the time of teleporation.
 * However, this also means we -always- need to intercept #UD exceptions on one
 * of the two CPU models (Intel or AMD). Hyper-V solves this problem more
 * elegantly by letting the hypervisor supply an opaque hypercall page.
 *
 * For raw-mode VMs, this function will always return true. See gimR3KvmInit().
 *
 * @param   pVM         Pointer to the VM.
 */
VMM_INT_DECL(bool) gimKvmShouldTrapXcptUD(PVM pVM)
{
    return pVM->gim.s.u.Kvm.fTrapXcptUD;
}


/**
 * Exception handler for #UD.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 * @param   pDis        Pointer to the disassembled instruction state at RIP.
 *                      Optional, can be NULL.
 */
VMM_INT_DECL(int) gimKvmXcptUD(PVMCPU pVCpu, PCPUMCTX pCtx, PDISCPUSTATE pDis)
{
    /*
     * If we didn't ask for #UD to be trapped, bail.
     */
    PVM     pVM  = pVCpu->CTX_SUFF(pVM);
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    if (RT_UNLIKELY(!pVM->gim.s.u.Kvm.fTrapXcptUD))
        return VERR_GIM_OPERATION_FAILED;

    /*
     * Make sure guest ring-0 is the one making the hypercall.
     */
    if (CPUMGetGuestCPL(pVCpu))
        return VERR_GIM_HYPERCALL_ACCESS_DENIED;

    int rc = VINF_SUCCESS;
    if (!pDis)
    {
        /*
         * Disassemble the instruction at RIP to figure out if it's the Intel
         * VMCALL instruction and if so, handle it as a hypercall.
         */
        DISCPUSTATE Dis;
        rc = EMInterpretDisasCurrent(pVM, pVCpu, &Dis, NULL /* pcbInstr */);
        pDis = &Dis;
    }

    if (RT_SUCCESS(rc))
    {
        /*
         * Patch the instruction to so we don't have to spend time disassembling it each time.
         * Makes sense only for HM as with raw-mode we will be getting a #UD regardless.
         */
        if (   pDis->pCurInstr->uOpcode == OP_VMCALL
            || pDis->pCurInstr->uOpcode == OP_VMMCALL)
        {
            uint8_t abHypercall[3];
            if (   pDis->pCurInstr->uOpcode != pKvm->uOpCodeNative
                && HMIsEnabled(pVM))
            {
                size_t  cbWritten = 0;
                rc = VMMPatchHypercall(pVM, &abHypercall, sizeof(abHypercall), &cbWritten);
                AssertRC(rc);
                Assert(sizeof(abHypercall) == pDis->cbInstr);
                Assert(sizeof(abHypercall) == cbWritten);

                rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, &abHypercall, sizeof(abHypercall));
            }

            /*
             * Perform the hypercall and update RIP.
             *
             * For HM, we can simply resume guest execution without perform the hypercall now and
             * do it on the next VMCALL/VMMCALL exit handler on the patched instruction.
             *
             * For raw-mode we need to do this now anyway. So we do it here regardless with an added
             * advantage is that it saves one world-switch for the HM case.
             */
            if (RT_SUCCESS(rc))
            {
                int rc2 = gimKvmHypercall(pVCpu, pCtx);
                AssertRC(rc2);
                pCtx->rip += pDis->cbInstr;
            }
            return rc;
        }
    }

    return VERR_GIM_OPERATION_FAILED;
}

