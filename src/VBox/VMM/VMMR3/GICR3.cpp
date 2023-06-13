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
 * Dumps basic APIC state.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) gicR3Info(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pVM, pHlp, pszArgs);
}


/**
 * Dumps GIC Distributor information.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) gicR3InfoDist(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);

    PGIC pGic = VM_TO_GIC(pVM);
    PPDMDEVINS pDevIns = pGic->CTX_SUFF(pDevIns);
    PGICDEV    pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);

    pHlp->pfnPrintf(pHlp, "GICv3 Distributor:\n");
    pHlp->pfnPrintf(pHlp, "  IGRP0            = %#RX32\n", pGicDev->u32RegIGrp0);
    pHlp->pfnPrintf(pHlp, "  ICFG0            = %#RX32\n", pGicDev->u32RegICfg0);
    pHlp->pfnPrintf(pHlp, "  ICFG1            = %#RX32\n", pGicDev->u32RegICfg1);
    pHlp->pfnPrintf(pHlp, "  bmIntEnabled     = %#RX32\n", pGicDev->bmIntEnabled);
    pHlp->pfnPrintf(pHlp, "  bmIntPending     = %#RX32\n", pGicDev->bmIntPending);
    pHlp->pfnPrintf(pHlp, "  bmIntActive      = %#RX32\n", pGicDev->bmIntActive);
    pHlp->pfnPrintf(pHlp, " Interrupt priorities:\n");
    for (uint32_t i = 0; i < RT_ELEMENTS(pGicDev->abIntPriority); i++)
        pHlp->pfnPrintf(pHlp, "     INTID %u    = %u\n", GIC_INTID_RANGE_SPI_START + i, pGicDev->abIntPriority[i]);

    pHlp->pfnPrintf(pHlp, "  fIrqGrp0Enabled    = %RTbool\n", pGicDev->fIrqGrp0Enabled);
    pHlp->pfnPrintf(pHlp, "  fIrqGrp1Enabled    = %RTbool\n", pGicDev->fIrqGrp1Enabled);
}


/**
 * Dumps the GIC Redistributor information.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) gicR3InfoReDist(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = pVM->apCpusR3[0];

    PGICCPU pGicVCpu = VMCPU_TO_GICCPU(pVCpu);

    pHlp->pfnPrintf(pHlp, "VCPU[%u] Redistributor:\n", pVCpu->idCpu);
    pHlp->pfnPrintf(pHlp, "  IGRP0            = %#RX32\n", pGicVCpu->u32RegIGrp0);
    pHlp->pfnPrintf(pHlp, "  ICFG0            = %#RX32\n", pGicVCpu->u32RegICfg0);
    pHlp->pfnPrintf(pHlp, "  ICFG1            = %#RX32\n", pGicVCpu->u32RegICfg1);
    pHlp->pfnPrintf(pHlp, "  bmIntEnabled     = %#RX32\n", pGicVCpu->bmIntEnabled);
    pHlp->pfnPrintf(pHlp, "  bmIntPending     = %#RX32\n", pGicVCpu->bmIntPending);
    pHlp->pfnPrintf(pHlp, "  bmIntActive      = %#RX32\n", pGicVCpu->bmIntActive);
    pHlp->pfnPrintf(pHlp, " Interrupt priorities:\n");
    for (uint32_t i = 0; i < RT_ELEMENTS(pGicVCpu->abIntPriority); i++)
        pHlp->pfnPrintf(pHlp, "     INTID %u    = %u\n", i, pGicVCpu->abIntPriority[i]);

    pHlp->pfnPrintf(pHlp, "VCPU[%u] ICC state:\n", pVCpu->idCpu);
    pHlp->pfnPrintf(pHlp, "  fIrqGrp0Enabled    = %RTbool\n", pGicVCpu->fIrqGrp0Enabled);
    pHlp->pfnPrintf(pHlp, "  fIrqGrp1Enabled    = %RTbool\n", pGicVCpu->fIrqGrp1Enabled);
    pHlp->pfnPrintf(pHlp, "  bInterruptPriority = %u\n",      pGicVCpu->bInterruptPriority);
    pHlp->pfnPrintf(pHlp, "  bBinaryPointGrp0   = %u\n",      pGicVCpu->bBinaryPointGrp0);
    pHlp->pfnPrintf(pHlp, "  bBinaryPointGrp1   = %u\n",      pGicVCpu->bBinaryPointGrp1);
    pHlp->pfnPrintf(pHlp, "  idxRunningPriority = %u\n",      pGicVCpu->idxRunningPriority);
    pHlp->pfnPrintf(pHlp, "  Running priority   = %u\n",      pGicVCpu->abRunningPriorities[pGicVCpu->idxRunningPriority]);
}


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
     * Register debugger info callbacks.
     *
     * We use separate callbacks rather than arguments so they can also be
     * dumped in an automated fashion while collecting crash diagnostics and
     * not just used during live debugging via the VM debugger.
     */
    DBGFR3InfoRegisterInternalEx(pVM, "gic",       "Dumps GIC basic information.",          gicR3Info,       DBGFINFO_FLAGS_ALL_EMTS);
    DBGFR3InfoRegisterInternalEx(pVM, "gicdist",   "Dumps GIC Distributor information.",    gicR3InfoDist,   DBGFINFO_FLAGS_ALL_EMTS);
    DBGFR3InfoRegisterInternalEx(pVM, "gicredist", "Dumps GIC Redistributor information.",  gicR3InfoReDist, DBGFINFO_FLAGS_ALL_EMTS);

    /*
     * Statistics.
     */
#define GIC_REG_COUNTER(a_pvReg, a_pszNameFmt, a_pszDesc) \
        PDMDevHlpSTAMRegisterF(pDevIns, a_pvReg, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, \
                               STAMUNIT_OCCURENCES, a_pszDesc, a_pszNameFmt, idCpu)
#define GIC_PROF_COUNTER(a_pvReg, a_pszNameFmt, a_pszDesc) \
        PDMDevHlpSTAMRegisterF(pDevIns, a_pvReg, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, \
                               STAMUNIT_TICKS_PER_CALL, a_pszDesc, a_pszNameFmt, idCpu)

#ifdef VBOX_WITH_STATISTICS
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU  pVCpu     = pVM->apCpusR3[idCpu];
        PGICCPU pGicCpu  = VMCPU_TO_GICCPU(pVCpu);

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
    }
#endif

# undef GIC_PROF_COUNTER

    gicR3Reset(pDevIns);
    return VINF_SUCCESS;
}

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

