/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GICv3) - All Contexts.
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
#include "GICInternal.h"
#include <VBox/vmm/gic.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/vmcpuset.h>
#ifdef IN_RING0
# include <VBox/vmm/gvmm.h>
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Reads a GIC register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReadRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, offReg);

    *puValue = 0;
    return VINF_SUCCESS;
}


/**
 * Writes a GIC register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicWriteRegister(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, offReg, uValue);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    return rcStrict;
}


/**
 * Reads a GIC system register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The system register being read.
 * @param   pu64Value       Where to store the read value.
 */
VMM_INT_DECL(VBOXSTRICTRC) GICReadSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pu64Value);

    *pu64Value = 0;
    LogFlowFunc(("pVCpu=%p u32Reg=%#x pu64Value=%RX64\n", pVCpu, u32Reg, *pu64Value));
    return VINF_SUCCESS;
}


/**
 * Writes an GIC system register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The system register being written (IPRT system  register identifier).
 * @param   u64Value        The value to write.
 */
VMM_INT_DECL(VBOXSTRICTRC) GICWriteSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pVCpu, u32Reg, u64Value);
    LogFlowFunc(("pVCpu=%p u32Reg=%#x u64Value=%RX64\n", pVCpu, u32Reg, u64Value));

    return VINF_SUCCESS;
}


/**
 * Initializes per-VCPU GIC to the state following a power-up or hardware
 * reset.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 */
DECLHIDDEN(void) gicResetCpu(PVMCPUCC pVCpu)
{
    LogFlowFunc(("GIC%u\n", pVCpu->idCpu));
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    NOREF(pvUser);
    //Assert(!(off & 0xf));
    //Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = off & 0xff0;
    uint32_t uValue   = 0;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioRead));

    VBOXSTRICTRC rc = VBOXSTRICTRC_VAL(gicReadRegister(pDevIns, pVCpu, offReg, &uValue));
    *(uint32_t *)pv = uValue;

    Log2(("GIC%u: gicReadMmio: offReg=%#RX16 uValue=%#RX32\n", pVCpu->idCpu, offReg, uValue));
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    //Assert(!(off & 0xf));
    //Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = off & 0xff0;
    uint32_t uValue   = *(uint32_t *)pv;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioWrite));

    Log2(("GIC%u: gicWriteMmio: offReg=%#RX16 uValue=%#RX32\n", pVCpu->idCpu, offReg, uValue));
    return gicWriteRegister(pDevIns, pVCpu, offReg, uValue);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicReDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    NOREF(pvUser);
    //Assert(!(off & 0xf));
    //Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = off & 0xff0;
    uint32_t uValue   = 0;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioRead));

    VBOXSTRICTRC rc = VBOXSTRICTRC_VAL(gicReadRegister(pDevIns, pVCpu, offReg, &uValue));
    *(uint32_t *)pv = uValue;

    Log2(("GIC%u: gicReadMmio: offReg=%#RX16 uValue=%#RX32\n", pVCpu->idCpu, offReg, uValue));
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicReDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    //Assert(!(off & 0xf));
    //Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = off & 0xff0;
    uint32_t uValue   = *(uint32_t *)pv;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioWrite));

    Log2(("GIC%u: gicWriteMmio: offReg=%#RX16 uValue=%#RX32\n", pVCpu->idCpu, offReg, uValue));
    return gicWriteRegister(pDevIns, pVCpu, offReg, uValue);
}


#ifndef IN_RING3

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) gicRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    AssertReleaseFailed();
    return VINF_SUCCESS;
}
#endif /* !IN_RING3 */

/**
 * GIC device registration structure.
 */
const PDMDEVREG g_DeviceGIC =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "gic",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIC,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(GICDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Generic Interrupt Controller",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           gicR3Construct,
    /* .pfnDestruct = */            gicR3Destruct,
    /* .pfnRelocate = */            gicR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               gicR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           gicRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           gicRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

