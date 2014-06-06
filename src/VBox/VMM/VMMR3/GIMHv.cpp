/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, Hyper-V implementation.
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
#include <iprt/string.h>
#include <iprt/mem.h>

#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/version.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
//#define GIMHV_HYPERCALL                 "GIMHvHypercall"
#ifdef VBOX_WITH_STATISTICS
# define GIMHV_MSRRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumMsrRdFn_Gim, kCpumMsrWrFn_Gim, 0, 0, 0, 0, 0, a_szName, { 0 }, { 0 }, { 0 }, { 0 } }
#else
# define GIMHV_MSRRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumMsrRdFn_Gim, kCpumMsrWrFn_Gim, 0, 0, 0, 0, 0, a_szName }
#endif

/**
 * Array of MSR ranges supported by Hyper-V.
 */
static CPUMMSRRANGE const g_aMsrRanges_HyperV[] =
{
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE0_START,  MSR_GIM_HV_RANGE0_END,  "Hyper-V range 0"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE1_START,  MSR_GIM_HV_RANGE1_END,  "Hyper-V range 1"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE2_START,  MSR_GIM_HV_RANGE2_END,  "Hyper-V range 2"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE3_START,  MSR_GIM_HV_RANGE3_END,  "Hyper-V range 3"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE4_START,  MSR_GIM_HV_RANGE4_END,  "Hyper-V range 4"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE5_START,  MSR_GIM_HV_RANGE5_END,  "Hyper-V range 5"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE6_START,  MSR_GIM_HV_RANGE6_END,  "Hyper-V range 6"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE7_START,  MSR_GIM_HV_RANGE7_END,  "Hyper-V range 7"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE8_START,  MSR_GIM_HV_RANGE8_END,  "Hyper-V range 8"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE9_START,  MSR_GIM_HV_RANGE9_END,  "Hyper-V range 9"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE10_START, MSR_GIM_HV_RANGE10_END, "Hyper-V range 10"),
    GIMHV_MSRRANGE(MSR_GIM_HV_RANGE11_START, MSR_GIM_HV_RANGE11_END, "Hyper-V range 11")
};
#undef GIMHV_MSR


/**
 * Initializes the Hyper-V GIM provider.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   uVersion    The interface version this VM should use.
 */
VMMR3_INT_DECL(int) GIMR3HvInit(PVM pVM)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pVM->gim.s.enmProviderId == GIMPROVIDERID_HYPERV, VERR_INTERNAL_ERROR_5);

    int rc;
    PGIMHV pHv = &pVM->gim.s.u.Hv;

    /*
     * Determine interface capabilities based on the version.
     */
    if (!pVM->gim.s.u32Version)
    {
        pHv->uBaseFeat = 0
                       //| GIM_HV_BASE_FEAT_VP_RUNTIME_MSR
                       | GIM_HV_BASE_FEAT_PART_TIME_REF_COUNT_MSR
                       //| GIM_HV_BASE_FEAT_BASIC_SYNTH_IC
                       //| GIM_HV_BASE_FEAT_SYNTH_TIMER_MSRS
                       //| GIM_HV_BASE_FEAT_APIC_ACCESS_MSRS
                       | GIM_HV_BASE_FEAT_HYPERCALL_MSRS
                       | GIM_HV_BASE_FEAT_VP_ID_MSR
                       //| GIM_HV_BASE_FEAT_VIRT_SYS_RESET_MSR
                       //| GIM_HV_BASE_FEAT_STAT_PAGES_MSR
                       | GIM_HV_BASE_FEAT_PART_REF_TSC_MSR
                       //| GIM_HV_BASE_FEAT_GUEST_IDLE_STATE_MSR
                       | GIM_HV_BASE_FEAT_TIMER_FREQ_MSRS
                       //| GIM_HV_BASE_FEAT_DEBUG_MSRS
                       ;

        pHv->uMiscFeat = GIM_HV_MISC_FEAT_TIMER_FREQ;
    }

    /*
     * Populate the required fields in MMIO2 region records for registering.
     */
    AssertCompile(GIM_HV_PAGE_SIZE == PAGE_SIZE);
    PGIMMMIO2REGION pRegion = &pHv->aMmio2Regions[GIM_HV_HYPERCALL_PAGE_REGION_IDX];
    pRegion->iRegion    = GIM_HV_HYPERCALL_PAGE_REGION_IDX;
    pRegion->cbRegion   = PAGE_SIZE;
    pRegion->GCPhysPage = NIL_RTGCPHYS;
    RTStrCopy(pRegion->szDescription, sizeof(pRegion->szDescription), "Hypercall Page");
    Assert(!pRegion->fRCMapping);
    Assert(!pRegion->fMapped);

    pRegion = &pHv->aMmio2Regions[GIM_HV_REF_TSC_PAGE_REGION_IDX];
    pRegion->iRegion    = GIM_HV_REF_TSC_PAGE_REGION_IDX;
    pRegion->cbRegion   = PAGE_SIZE;
    pRegion->GCPhysPage = NIL_RTGCPHYS;
    RTStrCopy(pRegion->szDescription, sizeof(pRegion->szDescription), "TSC Page");

    /*
     * Make sure the CPU ID bit are in accordance to the Hyper-V
     * requirement and other paranoia checks.
     * See "Requirements for implementing the Microsoft hypervisor interface" spec.
     */
    Assert(!(pHv->uPartFlags & (  GIM_HV_PART_FLAGS_CREATE_PART
                                | GIM_HV_PART_FLAGS_ACCESS_MEMORY_POOL
                                | GIM_HV_PART_FLAGS_ACCESS_PART_ID
                                | GIM_HV_PART_FLAGS_ADJUST_MSG_BUFFERS
                                | GIM_HV_PART_FLAGS_CREATE_PORT
                                | GIM_HV_PART_FLAGS_ACCESS_STATS
                                | GIM_HV_PART_FLAGS_CPU_MGMT
                                | GIM_HV_PART_FLAGS_CPU_PROFILER)));
    Assert((pHv->uBaseFeat & (GIM_HV_BASE_FEAT_HYPERCALL_MSRS | GIM_HV_BASE_FEAT_VP_ID_MSR))
                          == (GIM_HV_BASE_FEAT_HYPERCALL_MSRS | GIM_HV_BASE_FEAT_VP_ID_MSR));
    for (unsigned i = 0; i < RT_ELEMENTS(pHv->aMmio2Regions); i++)
    {
        PCGIMMMIO2REGION pcCur = &pHv->aMmio2Regions[i];
        Assert(!pcCur->fRCMapping);
        Assert(!pcCur->fMapped);
        Assert(pcCur->GCPhysPage == NIL_RTGCPHYS);
    }

    /*
     * Expose HVP (Hypervisor Present) bit to the guest.
     */
    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

    /*
     * Modify the standard hypervisor leaves for Hyper-V.
     */
    CPUMCPUIDLEAF HyperLeaf;
    RT_ZERO(HyperLeaf);
    HyperLeaf.uLeaf        = UINT32_C(0x40000000);
    HyperLeaf.uEax         = UINT32_C(0x40000005); /* Minimum value for Hyper-V */
    HyperLeaf.uEbx         = 0x7263694D;           /* 'Micr' */
    HyperLeaf.uEcx         = 0x666F736F;           /* 'osof' */
    HyperLeaf.uEdx         = 0x76482074;           /* 't Hv' */
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    HyperLeaf.uLeaf        = UINT32_C(0x40000001);
    HyperLeaf.uEax         = 0x31237648;           /* 'Hv#1' */
    HyperLeaf.uEbx         = 0;                    /* Reserved */
    HyperLeaf.uEcx         = 0;                    /* Reserved */
    HyperLeaf.uEdx         = 0;                    /* Reserved */
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Add Hyper-V specific leaves.
     */
    HyperLeaf.uLeaf        = UINT32_C(0x40000002); /* MBZ until MSR_GIM_HV_GUEST_OS_ID is set by the guest. */
    HyperLeaf.uEax         = 0;
    HyperLeaf.uEbx         = 0;
    HyperLeaf.uEcx         = 0;
    HyperLeaf.uEdx         = 0;
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    HyperLeaf.uLeaf        = UINT32_C(0x40000003);
    HyperLeaf.uEax         = pHv->uBaseFeat;
    HyperLeaf.uEbx         = pHv->uPartFlags;
    HyperLeaf.uEcx         = pHv->uPowMgmtFeat;
    HyperLeaf.uEdx         = pHv->uMiscFeat;
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Insert all MSR ranges of Hyper-V.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aMsrRanges_HyperV); i++)
    {
        rc = CPUMR3MsrRangesInsert(pVM, &g_aMsrRanges_HyperV[i]);
        AssertLogRelRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}


#if 0
VMMR3_INT_DECL(int) GIMR3HvInitFinalize(PVM pVM)
{
    pVM->gim.s.pfnHypercallR3 = &GIMHvHypercall;
    if (!HMIsEnabled(pVM))
    {
        rc = PDMR3LdrGetSymbolRC(pVM, NULL /* pszModule */, GIMHV_HYPERCALL, &pVM->gim.s.pfnHypercallRC);
        AssertRCReturn(rc, rc);
    }
    rc = PDMR3LdrGetSymbolR0(pVM, NULL /* pszModule */, GIMHV_HYPERCALL, &pVM->gim.s.pfnHypercallR0);
    AssertRCReturn(rc, rc);
}
#endif


VMMR3_INT_DECL(void) GIMR3HvRelocate(PVM pVM, RTGCINTPTR offDelta)
{
#if 0
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL /* pszModule */, GIMHV_HYPERCALL, &pVM->gim.s.pfnHypercallRC);
    AssertFatalRC(rc);
#endif
}


/**
 * The VM is being reset. This resets Hyper-V provider MSRs and unmaps whatever
 * Hyper-V regions that the guest may have mapped.
 *
 * @param   pVM     Pointer to the VM.
 */
VMMR3_INT_DECL(void) GIMR3HvReset(PVM pVM)
{
    /*
     * Unmap MMIO2 pages that the guest may have setup.
     */
    PGIMHV pHv = &pVM->gim.s.u.Hv;
    for (unsigned i = 0; i < RT_ELEMENTS(pHv->aMmio2Regions); i++)
    {
        PGIMMMIO2REGION pRegion = &pHv->aMmio2Regions[i];
        GIMR3Mmio2Unmap(pVM, pRegion);
    }

    /*
     * Reset MSRs.
     */
    pHv->u64GuestOsIdMsr = 0;
    pHv->u64HypercallMsr = 0;
    pHv->u64TscPageMsr   = 0;
}


/**
 * Returns a pointer to the MMIO2 regions supported by Hyper-V.
 *
 * @returns Pointer to an array of MMIO2 regions.
 * @param   pVM         Pointer to the VM.
 * @param   pcRegions   Where to store the number of regions in the array.
 */
VMMR3_INT_DECL(PGIMMMIO2REGION) GIMR3HvGetMmio2Regions(PVM pVM, uint32_t *pcRegions)
{
    Assert(GIMIsEnabled(pVM));
    PGIMHV pHv = &pVM->gim.s.u.Hv;

    *pcRegions = RT_ELEMENTS(pHv->aMmio2Regions);
    Assert(*pcRegions <= UINT8_MAX);    /* See PGMR3PhysMMIO2Register(). */
    return pHv->aMmio2Regions;
}

