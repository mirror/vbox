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


/**
 * Handles the Hyper-V hypercall.
 *
 * @returns VBox status code.
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   pCtx            Pointer to the guest-CPU context.
 */
VMMDECL(int) GIMHvHypercall(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    return VINF_SUCCESS;
}


/**
 * Updates Hyper-V's reference TSC page.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   u64Offset   The computed TSC offset.
 * @thread EMT(pVCpu)
 */
VMMDECL(int) GIMHvUpdateParavirtTsc(PVM pVM, uint64_t u64Offset)
{
    Assert(GIMIsEnabled(pVM));
    bool fHvTscEnabled = MSR_GIM_HV_REF_TSC_IS_ENABLED(pVM->gim.s.u.Hv.u64TscPageMsr);
    if (!fHvTscEnabled)
        return VERR_GIM_PVTSC_NOT_ENABLED;

    PGIMHV          pHv      = &pVM->gim.s.u.Hv;
    PGIMMMIO2REGION pRegion  = &pHv->aMmio2Regions[GIM_HV_HYPERCALL_PAGE_REGION_IDX];
    PGIMHVREFTSC    pRefTsc  = (PGIMHVREFTSC)pRegion->CTX_SUFF(pvPage);
    Assert(pRefTsc);

    /** @todo Protect this with a spinlock! */
    pRefTsc->u64TscScale  = UINT64_C(0x1000000000000000);
    pRefTsc->u64TscOffset = u64Offset;
    ASMAtomicIncU32(&pRefTsc->u32TscSequence);

    return VINF_SUCCESS;
}


/**
 * MSR read handler for Hyper-V.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR being read.
 * @param   pRange      The range this MSR belongs to.
 * @param   puValue     Where to store the MSR value read.
 */
VMMDECL(int) GIMHvReadMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t *puValue)
{
    NOREF(pRange);
    PVM    pVM = pVCpu->CTX_SUFF(pVM);
    PGIMHV pHv = &pVM->gim.s.u.Hv;

    switch (idMsr)
    {
        case MSR_GIM_HV_TIME_REF_COUNT:
        {
            /* Hyper-V reports the time in 100ns units. */
            uint64_t u64Tsc      = TMCpuTickGet(pVCpu);
            uint64_t u64TscHz    = TMCpuTicksPerSecond(pVM);
            uint64_t u64Tsc100Ns = u64TscHz / UINT64_C(10000000); /* 100 ns */
            *puValue = (u64Tsc / u64Tsc100Ns);
            return VINF_SUCCESS;
        }

        case MSR_GIM_HV_VP_INDEX:
            *puValue = pVCpu->idCpu;
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
            /** @todo Fix this later! Get the information from DevApic. */
            *puValue = UINT32_C(1000000000); /* TMCLOCK_FREQ_VIRTUAL */
            return VINF_SUCCESS;

        default:
            break;
    }

    LogRel(("GIMHvReadMsr: Unknown/invalid RdMsr %#RX32 -> #GP(0)\n", idMsr));
    return VERR_CPUM_RAISE_GP_0;
}


/**
 * MSR write handler for Hyper-V.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   idMsr       The MSR being written.
 * @param   pRange      The range this MSR belongs to.
 * @param   uValue      The value to set, ignored bits masked.
 * @param   uRawValue   The raw value with the ignored bits not masked.
 */
VMMDECL(int) GIMHvWriteMsr(PVMCPU pVCpu, uint32_t idMsr, PCCPUMMSRRANGE pRange, uint64_t uValue, uint64_t uRawValue)
{
    NOREF(pRange); NOREF(uValue);
    PVM    pVM = pVCpu->CTX_SUFF(pVM);
    PGIMHV pHv = &pVM->gim.s.u.Hv;

    switch (idMsr)
    {
        case MSR_GIM_HV_GUEST_OS_ID:
        {
#ifndef IN_RING3
            return VERR_EM_INTERPRETER;
#else
            /* Disable the hypercall-page if 0 is written to this MSR. */
            if (!uRawValue)
            {
                GIMR3Mmio2Unmap(pVM, &pHv->aMmio2Regions[GIM_HV_HYPERCALL_PAGE_REGION_IDX]);
                pHv->u64HypercallMsr &= ~MSR_GIM_HV_HYPERCALL_ENABLE_BIT;
                Log4Func(("Disabled hypercalls\n"));
            }
            pHv->u64GuestOsIdMsr = uRawValue;
            return VINF_SUCCESS;
#endif  /* !IN_RING3 */
        }

        case MSR_GIM_HV_HYPERCALL:
        {
#ifndef IN_RING3
            return VERR_EM_INTERPRETER;
#else   /* IN_RING3 */
            /* First, update all but the hypercall enable bit. */
            pHv->u64HypercallMsr = (uRawValue & ~MSR_GIM_HV_HYPERCALL_ENABLE_BIT);

            /* Hypercalls can only be enabled when the guest has set the Guest-OS Id Msr. */
            bool fEnable = RT_BOOL(uRawValue & MSR_GIM_HV_HYPERCALL_ENABLE_BIT);
            if (   fEnable
                && !pHv->u64GuestOsIdMsr)
            {
                return VINF_SUCCESS;
            }

            PPDMDEVINSR3    pDevIns = pVM->gim.s.pDevInsR3;
            PGIMMMIO2REGION pRegion = &pHv->aMmio2Regions[GIM_HV_HYPERCALL_PAGE_REGION_IDX];
            AssertPtr(pDevIns);
            AssertPtr(pRegion);

            /*
             * Is the guest disabling the hypercall-page? Allow it regardless of the Guest-OS Id Msr.
             */
            if (!fEnable)
            {
                GIMR3Mmio2Unmap(pVM, pRegion);
                pHv->u64HypercallMsr = uRawValue;
                Log4Func(("Disabled hypercalls\n"));
                return VINF_SUCCESS;
            }

            /*
             * Map the hypercall-page.
             */
            RTGCPHYS GCPhysHypercallPage = MSR_GIM_HV_HYPERCALL_GUEST_PFN(uRawValue) << PAGE_SHIFT;
            int rc = GIMR3Mmio2Map(pVM, pRegion, GCPhysHypercallPage, "Hyper-V Hypercall-page");
            if (RT_SUCCESS(rc))
            {
                /*
                 * Patch the hypercall-page.
                 */
                if (HMIsEnabled(pVM))
                {
                    size_t cbWritten = 0;
                    rc = HMPatchHypercall(pVM, pRegion->pvPageR3, PAGE_SIZE, &cbWritten);
                    if (   RT_SUCCESS(rc)
                        && cbWritten < PAGE_SIZE - 1)
                    {
                        uint8_t *pbLast = (uint8_t *)pRegion->pvPageR3 + cbWritten;
                        *pbLast = 0xc3;  /* RET */

                        pHv->u64HypercallMsr = uRawValue;
                        LogRelFunc(("Enabled hypercalls at %#RGp\n", GCPhysHypercallPage));
                        LogRelFunc(("%.*Rhxd\n", cbWritten + 1, (uint8_t *)pRegion->pvPageR3));
                        return VINF_SUCCESS;
                    }

                    LogFunc(("MSR_GIM_HV_HYPERCALL: HMPatchHypercall failed. rc=%Rrc cbWritten=%u\n", rc, cbWritten));
                }
                else
                {
                    /** @todo Handle raw-mode hypercall page patching. */
                    LogRelFunc(("MSR_GIM_HV_HYPERCALL: raw-mode not yet implemented!\n"));
                }

                GIMR3Mmio2Unmap(pVM, pRegion);
            }
            else
                LogFunc(("MSR_GIM_HV_HYPERCALL: GIMR3Mmio2Map failed. rc=%Rrc -> #GP(0)\n", rc));

            return VERR_CPUM_RAISE_GP_0;
#endif  /* !IN_RING3 */
        }

        case MSR_GIM_HV_REF_TSC:
        {
#ifndef IN_RING3
            return VERR_EM_INTERPRETER;
#else   /* IN_RING3 */
            /* First, update all but the TSC-page enable bit. */
            pHv->u64TscPageMsr = (uRawValue & ~MSR_GIM_HV_REF_TSC_ENABLE_BIT);

            PPDMDEVINSR3    pDevIns = pVM->gim.s.pDevInsR3;
            PGIMMMIO2REGION pRegion = &pHv->aMmio2Regions[GIM_HV_REF_TSC_PAGE_REGION_IDX];
            AssertPtr(pDevIns);
            AssertPtr(pRegion);

            /*
             * Is the guest disabling the TSC-page?
             */
            bool fEnable = RT_BOOL(uRawValue & MSR_GIM_HV_REF_TSC_ENABLE_BIT);
            if (!fEnable)
            {
                GIMR3Mmio2Unmap(pVM, pRegion);
                Log4Func(("Disabled TSC-page\n"));
                return VINF_SUCCESS;
            }

            /*
             * Map the TSC-page.
             */
            RTGCPHYS GCPhysTscPage = MSR_GIM_HV_REF_TSC_GUEST_PFN(uRawValue) << PAGE_SHIFT;
            int rc = GIMR3Mmio2Map(pVM, pRegion, GCPhysTscPage, "Hyper-V TSC-page");
            if (RT_SUCCESS(rc))
            {
                pHv->u64TscPageMsr = uRawValue;
                Log4Func(("MSR_GIM_HV_REF_TSC: Enabled Hyper-V TSC page at %#RGp\n", GCPhysTscPage));
                return VINF_SUCCESS;
            }
            else
                LogFunc(("MSR_GIM_HV_REF_TSC: GIMR3Mmio2Map failed. rc=%Rrc -> #GP(0)\n", rc));

            return VERR_CPUM_RAISE_GP_0;
#endif  /* !IN_RING3 */
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
                LogRel(("GIM: Unknown/invalid WrMsr (%#x,%#x`%08x) -> #GP(0)\n", idMsr, uRawValue & UINT64_C(0xffffffff00000000),
                        uRawValue & UINT64_C(0xffffffff)));
#endif
            LogFunc(("Unknown/invalid WrMsr (%#RX32,%#RX64) -> #GP(0)\n", idMsr, uRawValue));
            break;
    }

    return VERR_CPUM_RAISE_GP_0;
}

