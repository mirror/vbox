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
#include "GIMInternal.h"

#include <VBox/err.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmapi.h>

#include <iprt/asm-amd64-x86.h>
#include <iprt/spinlock.h>


/**
 * Handles the Hyper-V hypercall.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pCtx            Pointer to the guest-CPU context.
 */
VMM_INT_DECL(int) GIMHvHypercall(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (!MSR_GIM_HV_HYPERCALL_IS_ENABLED(pVM->gim.s.u.Hv.u64HypercallMsr))
        return VERR_GIM_HYPERCALLS_NOT_ENABLED;

    /** @todo Handle hypercalls. Fail for now */
    return VERR_GIM_IPE_3;
}


/**
 * Returns whether the guest has configured and enabled the use of Hyper-V's
 * hypercall interface.
 *
 * @returns true if hypercalls are enabled, false otherwise.
 * @param   pVCpu       Pointer to the VMCPU.
 */
VMM_INT_DECL(bool) GIMHvAreHypercallsEnabled(PVMCPU pVCpu)
{
    return MSR_GIM_HV_HYPERCALL_IS_ENABLED(pVCpu->CTX_SUFF(pVM)->gim.s.u.Hv.u64HypercallMsr);
}


/**
 * Returns whether the guest has configured and enabled the use of Hyper-V's
 * paravirtualized TSC.
 *
 * @returns true if paravirt. TSC is enabled, false otherwise.
 * @param   pVM     Pointer to the VM.
 */
VMM_INT_DECL(bool) GIMHvIsParavirtTscEnabled(PVM pVM)
{
    return MSR_GIM_HV_REF_TSC_IS_ENABLED(pVM->gim.s.u.Hv.u64TscPageMsr);
}


/**
 * MSR read handler for Hyper-V.
 *
 * @returns Strict VBox status code like CPUMQueryGuestMsr.
 * @retval  VINF_CPUM_R3_MSR_READ
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR being read.
 * @param   pRange      The range this MSR belongs to.
 * @param   puValue     Where to store the MSR value read.
 */
VMM_INT_DECL(VBOXSTRICTRC) GIMHvReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue)
{
    NOREF(pRange);
    PVM    pVM = pVCpu->CTX_SUFF(pVM);
    PGIMHV pHv = &pVM->gim.s.u.Hv;

    switch (idMsr)
    {
        case MSR_GIM_HV_TIME_REF_COUNT:
        {
            /* Hyper-V reports the time in 100 ns units (10 MHz). */
            uint64_t u64Tsc      = TMCpuTickGet(pVCpu);
            uint64_t u64TscHz    = TMCpuTicksPerSecond(pVM);
            uint64_t u64Tsc100Ns = u64TscHz / UINT64_C(10000000); /* 100 ns */
            *puValue = (u64Tsc / u64Tsc100Ns);
            return VINF_SUCCESS;
        }

        case MSR_GIM_HV_VP_INDEX:
            *puValue = pVCpu->idCpu;
            return VINF_SUCCESS;

        case MSR_GIM_HV_TPR:
            PDMApicReadMSR(pVM, pVCpu->idCpu, 0x80, puValue);
            return VINF_SUCCESS;

        case MSR_GIM_HV_EOI:
            PDMApicReadMSR(pVM, pVCpu->idCpu, 0x0B, puValue);
            return VINF_SUCCESS;

        case MSR_GIM_HV_ICR:
            PDMApicReadMSR(pVM, pVCpu->idCpu, 0x30, puValue);
            return VINF_SUCCESS;

        case MSR_GIM_HV_GUEST_OS_ID:
            *puValue = pHv->u64GuestOsIdMsr;
            return VINF_SUCCESS;

        case MSR_GIM_HV_HYPERCALL:
            *puValue = pHv->u64HypercallMsr;
            return VINF_SUCCESS;

        case MSR_GIM_HV_REF_TSC:
            *puValue = pHv->u64TscPageMsr;
            return VINF_SUCCESS;

        case MSR_GIM_HV_TSC_FREQ:
            *puValue = TMCpuTicksPerSecond(pVM);
            return VINF_SUCCESS;

        case MSR_GIM_HV_APIC_FREQ:
        {
            int rc = PDMApicGetTimerFreq(pVM, puValue);
            if (RT_FAILURE(rc))
                return VERR_CPUM_RAISE_GP_0;
            return VINF_SUCCESS;
        }

        case MSR_GIM_HV_RESET:
            *puValue = 0;
            return VINF_SUCCESS;

        default:
#ifdef IN_RING3
            static uint32_t s_cTimes = 0;
            if (s_cTimes++ < 20)
                LogRel(("GIM: HyperV: Unknown/invalid RdMsr (%#x) -> #GP(0)\n", idMsr));
#endif
            LogFunc(("Unknown/invalid RdMsr (%#RX32) -> #GP(0)\n", idMsr));
            break;
    }

    return VERR_CPUM_RAISE_GP_0;
}


/**
 * MSR write handler for Hyper-V.
 *
 * @returns Strict VBox status code like CPUMSetGuestMsr.
 * @retval  VINF_CPUM_R3_MSR_WRITE
 * @retval  VERR_CPUM_RAISE_GP_0
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR being written.
 * @param   pRange      The range this MSR belongs to.
 * @param   uRawValue   The raw value with the ignored bits not masked.
 */
VMM_INT_DECL(VBOXSTRICTRC) GIMHvWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uRawValue)
{
    NOREF(pRange);
    PVM    pVM = pVCpu->CTX_SUFF(pVM);
    PGIMHV pHv = &pVM->gim.s.u.Hv;

    switch (idMsr)
    {
        case MSR_GIM_HV_TPR:
            PDMApicWriteMSR(pVM, pVCpu->idCpu, 0x80, uRawValue);
            return VINF_SUCCESS;

        case MSR_GIM_HV_EOI:
            PDMApicWriteMSR(pVM, pVCpu->idCpu, 0x0B, uRawValue);
            return VINF_SUCCESS;

        case MSR_GIM_HV_ICR:
            PDMApicWriteMSR(pVM, pVCpu->idCpu, 0x30, uRawValue);
            return VINF_SUCCESS;

        case MSR_GIM_HV_GUEST_OS_ID:
        {
#ifndef IN_RING3
            return VINF_CPUM_R3_MSR_WRITE;
#else
            /* Disable the hypercall-page if 0 is written to this MSR. */
            if (!uRawValue)
            {
                GIMR3HvDisableHypercallPage(pVM);
                pHv->u64HypercallMsr &= ~MSR_GIM_HV_HYPERCALL_ENABLE_BIT;
            }
            pHv->u64GuestOsIdMsr = uRawValue;
            return VINF_SUCCESS;
#endif /* IN_RING3 */
        }

        case MSR_GIM_HV_HYPERCALL:
        {
#ifndef IN_RING3
            return VINF_CPUM_R3_MSR_WRITE;
#else  /* IN_RING3 */
            /*
             * For now ignore writes to the hypercall MSR (i.e. keeps it disabled).
             * This is required to boot FreeBSD 10.1 (with Hyper-V enabled ofc),
             * see @bugref{7270} comment #116.
             */
            return VINF_SUCCESS;
# if 0
            /* First, update all but the hypercall enable bit. */
            pHv->u64HypercallMsr = (uRawValue & ~MSR_GIM_HV_HYPERCALL_ENABLE_BIT);

            /* Hypercalls can only be enabled when the guest has set the Guest-OS Id Msr. */
            bool fEnable = RT_BOOL(uRawValue & MSR_GIM_HV_HYPERCALL_ENABLE_BIT);
            if (   fEnable
                && !pHv->u64GuestOsIdMsr)
            {
                return VINF_SUCCESS;
            }

            /* Is the guest disabling the hypercall-page? Allow it regardless of the Guest-OS Id Msr. */
            if (!fEnable)
            {
                GIMR3HvDisableHypercallPage(pVM);
                pHv->u64HypercallMsr = uRawValue;
                return VINF_SUCCESS;
            }

            /* Enable the hypercall-page. */
            RTGCPHYS GCPhysHypercallPage = MSR_GIM_HV_HYPERCALL_GUEST_PFN(uRawValue) << PAGE_SHIFT;
            int rc = GIMR3HvEnableHypercallPage(pVM, GCPhysHypercallPage);
            if (RT_SUCCESS(rc))
            {
                pHv->u64HypercallMsr = uRawValue;
                return VINF_SUCCESS;
            }

            return VERR_CPUM_RAISE_GP_0;
# endif
#endif /* IN_RING3 */
        }

        case MSR_GIM_HV_REF_TSC:
        {
#ifndef IN_RING3
            return VINF_CPUM_R3_MSR_WRITE;
#else  /* IN_RING3 */
            /* First, update all but the TSC-page enable bit. */
            pHv->u64TscPageMsr = (uRawValue & ~MSR_GIM_HV_REF_TSC_ENABLE_BIT);

            /* Is the guest disabling the TSC-page? */
            bool fEnable = RT_BOOL(uRawValue & MSR_GIM_HV_REF_TSC_ENABLE_BIT);
            if (!fEnable)
            {
                GIMR3HvDisableTscPage(pVM);
                pHv->u64TscPageMsr = uRawValue;
                return VINF_SUCCESS;
            }

            /* Enable the TSC-page. */
            RTGCPHYS GCPhysTscPage = MSR_GIM_HV_REF_TSC_GUEST_PFN(uRawValue) << PAGE_SHIFT;
            int rc = GIMR3HvEnableTscPage(pVM, GCPhysTscPage, false /* fUseThisTscSequence */, 0 /* uTscSequence */);
            if (RT_SUCCESS(rc))
            {
                pHv->u64TscPageMsr = uRawValue;
                return VINF_SUCCESS;
            }

            return VERR_CPUM_RAISE_GP_0;
#endif /* IN_RING3 */
        }

        case MSR_GIM_HV_RESET:
        {
#ifndef IN_RING3
            return VINF_CPUM_R3_MSR_WRITE;
#else
            if (MSR_GIM_HV_RESET_IS_SET(uRawValue))
            {
                LogRel(("GIM: HyperV: Reset initiated through MSR.\n"));
                int rc = PDMDevHlpVMReset(pVM->gim.s.pDevInsR3);
                AssertRC(rc);
            }
            /* else: Ignore writes to other bits. */
            return VINF_SUCCESS;
#endif /* IN_RING3 */
        }

        case MSR_GIM_HV_TIME_REF_COUNT:     /* Read-only MSRs. */
        case MSR_GIM_HV_VP_INDEX:
        case MSR_GIM_HV_TSC_FREQ:
        case MSR_GIM_HV_APIC_FREQ:
            LogFunc(("WrMsr on read-only MSR %#RX32 -> #GP(0)\n", idMsr));
            return VERR_CPUM_RAISE_GP_0;

        default:
#ifdef IN_RING3
            static uint32_t s_cTimes = 0;
            if (s_cTimes++ < 20)
                LogRel(("GIM: HyperV: Unknown/invalid WrMsr (%#x,%#x`%08x) -> #GP(0)\n", idMsr, uRawValue & UINT64_C(0xffffffff00000000),
                        uRawValue & UINT64_C(0xffffffff)));
#endif
            LogFunc(("Unknown/invalid WrMsr (%#RX32,%#RX64) -> #GP(0)\n", idMsr, uRawValue));
            break;
    }

    return VERR_CPUM_RAISE_GP_0;
}

