/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GICv3).
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include <VBox/log.h>
#include "GICInternal.h"
#include <VBox/vmm/gic.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/vm.h>

#include <iprt/armv8.h>


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
# define GIC_SYSREGRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumSysRegRdFn_GicV3Icc, kCpumSysRegWrFn_GicV3Icc, 0, 0, 0, 0, 0, 0, a_szName, { 0 }, { 0 }, { 0 }, { 0 } }


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * System register ranges for the GICv3.
 */
static CPUMSYSREGRANGE const g_aSysRegRanges_GICv3[] =
{
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_PMR_EL1,   ARMV8_AARCH64_SYSREG_ICC_PMR_EL1,     "ICC_PMR_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_IAR0_EL1,  ARMV8_AARCH64_SYSREG_ICC_AP0R3_EL1,   "ICC_IAR0_EL1 - ICC_AP0R3_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_AP1R0_EL1, ARMV8_AARCH64_SYSREG_ICC_NMIAR1_EL1,  "ICC_AP1R0_EL1 - ICC_NMIAR1_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_DIR_EL1,   ARMV8_AARCH64_SYSREG_ICC_SGI0R_EL1,   "ICC_DIR_EL1 - ICC_SGI0R_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_IAR1_EL1,  ARMV8_AARCH64_SYSREG_ICC_IGRPEN1_EL1, "ICC_IAR1_EL1 - ICC_IGRPEN1_EL1"),
};


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
DECLCALLBACK(void) gicR3Reset(PPDMDEVINS pDevIns)
{
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    VM_ASSERT_EMT0(pVM);
    VM_ASSERT_IS_NOT_RUNNING(pVM);

    LogFlow(("GIC: gicR3Reset\n"));

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU  pVCpuDest = pVM->apCpusR3[idCpu];

        gicResetCpu(pVCpuDest);
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
DECLCALLBACK(void) gicR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(pDevIns, offDelta);
}


/**
 * Initializes the GIC state.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
static int gicR3InitState(PVM pVM)
{
    LogFlowFunc(("pVM=%p\n", pVM));

    RT_NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
DECLCALLBACK(int) gicR3Destruct(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("pDevIns=%p\n", pDevIns));
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) gicR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PGICDEV         pGicDev  = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    PCPDMDEVHLPR3   pHlp     = pDevIns->pHlpR3;
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    PGIC            pGic     = VM_TO_GIC(pVM);
    Assert(iInstance == 0); NOREF(iInstance);

    /*
     * Init the data.
     */
    pGic->pDevInsR3     = pDevIns;

    /*
     * Validate GIC settings.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DistributorMmioBase|RedistributorMmioBase", "");

#if 0
    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);
#else
    int rc;
#endif

    /*
     * Register the GIC with PDM.
     */
    rc = PDMDevHlpApicRegister(pDevIns);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Initialize the GIC state.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aSysRegRanges_GICv3); i++)
    {
        rc = CPUMR3SysRegRangesInsert(pVM, &g_aSysRegRanges_GICv3[i]);
        AssertLogRelRCReturn(rc, rc);
    }

    /* Finally, initialize the state. */
    rc = gicR3InitState(pVM);
    AssertRCReturn(rc, rc);

    /*
     * Register the MMIO ranges.
     */
    RTGCPHYS GCPhysMmioBase = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "DistributorMmioBase", &GCPhysMmioBase);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DistributorMmioBase\" value"));

    rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmioBase, GIC_DIST_REG_FRAME_SIZE, gicDistMmioWrite, gicDistMmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED, "GICv3_Dist", &pGicDev->hMmioDist);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnCFGMQueryU64(pCfg, "RedistributorMmioBase", &GCPhysMmioBase);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"RedistributorMmioBase\" value"));

    RTGCPHYS cbRegion = pVM->cCpus * (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE); /* Adjacent and per vCPU. */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmioBase, cbRegion, gicReDistMmioWrite, gicReDistMmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED, "GICv3_ReDist", &pGicDev->hMmioReDist);
    AssertRCReturn(rc, rc);

    /*
     * Statistics.
     */
#define GIC_REG_COUNTER(a_pvReg, a_pszNameFmt, a_pszDesc) \
        PDMDevHlpSTAMRegisterF(pDevIns, a_pvReg, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, \
                               STAMUNIT_OCCURENCES, a_pszDesc, a_pszNameFmt, idCpu)
#define GIC_PROF_COUNTER(a_pvReg, a_pszNameFmt, a_pszDesc) \
        PDMDevHlpSTAMRegisterF(pDevIns, a_pvReg, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, \
                               STAMUNIT_TICKS_PER_CALL, a_pszDesc, a_pszNameFmt, idCpu)

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU  pVCpu     = pVM->apCpusR3[idCpu];
        PGICCPU pGicCpu  = VMCPU_TO_GICCPU(pVCpu);

#ifdef VBOX_WITH_STATISTICS
# if 0 /* No R0 for now. */
        GIC_REG_COUNTER(&pGicCpu->StatMmioReadRZ,    "%u/RZ/MmioRead",    "Number of APIC MMIO reads in RZ.");
        GIC_REG_COUNTER(&pGicCpu->StatMmioWriteRZ,   "%u/RZ/MmioWrite",   "Number of APIC MMIO writes in RZ.");
        GIC_REG_COUNTER(&pGicCpu->StatMsrReadRZ,     "%u/RZ/MsrRead",     "Number of APIC MSR reads in RZ.");
        GIC_REG_COUNTER(&pGicCpu->StatMsrWriteRZ,    "%u/RZ/MsrWrite",    "Number of APIC MSR writes in RZ.");
# endif

        GIC_REG_COUNTER(&pGicCpu->StatMmioReadR3,    "%u/R3/MmioRead",    "Number of APIC MMIO reads in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatMmioWriteR3,   "%u/R3/MmioWrite",   "Number of APIC MMIO writes in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSysRegReadR3,  "%u/R3/SysRegRead",  "Number of GIC system register reads in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSysRegWriteR3, "%u/R3/SysRegWrite", "Number of GIC system register writes in R3.");
#endif
    }

# undef GIC_PROF_COUNTER

    return VINF_SUCCESS;
}

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

