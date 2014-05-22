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
#include "GIMHvInternal.h"
#include "GIMInternal.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/version.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define GIMHV_HYPERCALL                 "GIMHvHypercall"


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
#if 0
    pVM->gim.s.pfnHypercallR3 = &GIMHvHypercall;
    if (!HMIsEnabled(pVM))
    {
        rc = PDMR3LdrGetSymbolRC(pVM, NULL /* pszModule */, GIMHV_HYPERCALL, &pVM->gim.s.pfnHypercallRC);
        AssertRCReturn(rc, rc);
    }
    rc = PDMR3LdrGetSymbolR0(pVM, NULL /* pszModule */, GIMHV_HYPERCALL, &pVM->gim.s.pfnHypercallR0);
    AssertRCReturn(rc, rc);
#endif

    /*
     * Determine interface capabilities based on the version.
     */
    uint32_t uBaseFeat    = 0;
    uint32_t uPartFlags   = 0;
    uint32_t uPowMgmtFeat = 0;
    uint32_t uMiscFeat    = 0;
    uint32_t uHyperHints  = 0;
    if (!pVM->gim.s.u32Version)
    {
        uBaseFeat = GIM_HV_BASE_FEAT_PART_REF_COUNT_MSR;
        pVM->gim.s.u.hv.u16HyperIdVersionMajor = VBOX_VERSION_MAJOR;
        pVM->gim.s.u.hv.u16HyperIdVersionMajor = VBOX_VERSION_MINOR;
        pVM->gim.s.u.hv.u32HyperIdBuildNumber  = VBOX_VERSION_BUILD;
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
    HyperLeaf.uLeaf        = UINT32_C(0x40000002); /* MBZ until MSR_HV_GUEST_OS_ID is set by the guest. */
    HyperLeaf.uEax         = 0;
    HyperLeaf.uEbx         = 0;
    HyperLeaf.uEcx         = 0;
    HyperLeaf.uEdx         = 0;
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    HyperLeaf.uLeaf        = UINT32_C(0x40000003);
    HyperLeaf.uEax         = uBaseFeat;
    HyperLeaf.uEbx         = uPartFlags;
    HyperLeaf.uEcx         = uPowMgmtFeat;
    HyperLeaf.uEdx         = uMiscFeat;
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register the complete MSR range for Hyper-V.
     */
    CPUMMSRRANGE MsrRange;
    RT_ZERO(MsrRange);
    MsrRange.uFirst     = UINT32_C(0x40000000);
    MsrRange.uLast      = UINT32_C(0x40000105);
    MsrRange.enmRdFn    = kCpumMsrRdFn_Gim;
    MsrRange.enmWrFn    = kCpumMsrWrFn_Gim;
    RTStrCopy(MsrRange.szName, sizeof(MsrRange.szName), "Hyper-V");
    rc = CPUMR3MsrRangesInsert(pVM, &MsrRange);
    AssertLogRelRCReturn(rc, rc);


    return VINF_SUCCESS;
}


VMMR3_INT_DECL(void) GIMR3HvRelocate(PVM pVM, RTGCINTPTR offDelta)
{
//    int rc = PDMR3LdrGetSymbolRC(pVM, NULL /* pszModule */, GIMHV_HYPERCALL, &pVM->gim.s.pfnHypercallRC);
//    AssertFatalRC(rc);
}

