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
 * Sets the interrupt pending force-flag and pokes the EMT if required.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   enmType         The IRQ type.
 */
static void gicSetInterruptFF(PVMCPUCC pVCpu, bool fIrq, bool fFiq)
{
    Assert(fIrq || fFiq);

#ifdef IN_RING3
    /* IRQ state should be loaded as-is by "LoadExec". Changes can be made from LoadDone. */
    Assert(pVCpu->pVMR3->enmVMState != VMSTATE_LOADING || PDMR3HasLoadedState(pVCpu->pVMR3));
#endif

    if (fIrq)
        VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ);
    if (fFiq)
        VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_FIQ);

    /*
     * We need to wake up the target CPU if we're not on EMT.
     */
    /** @todo We could just use RTThreadNativeSelf() here, couldn't we? */
#if defined(IN_RING0)
    PVMCC   pVM   = pVCpu->CTX_SUFF(pVM);
    VMCPUID idCpu = pVCpu->idCpu;
    if (VMMGetCpuId(pVM) != idCpu)
    {
        switch (VMCPU_GET_STATE(pVCpu))
        {
            case VMCPUSTATE_STARTED_EXEC:
                Log7Func(("idCpu=%u VMCPUSTATE_STARTED_EXEC\n", idCpu));
                GVMMR0SchedPokeNoGVMNoLock(pVM, idCpu);
                break;

            case VMCPUSTATE_STARTED_HALTED:
                Log7Func(("idCpu=%u VMCPUSTATE_STARTED_HALTED\n", idCpu));
                GVMMR0SchedWakeUpNoGVMNoLock(pVM, idCpu);
                break;

            default:
                Log7Func(("idCpu=%u enmState=%d\n", idCpu, pVCpu->enmState));
                break; /* nothing to do in other states. */
        }
    }
#elif defined(IN_RING3)
    PVMCC   pVM   = pVCpu->CTX_SUFF(pVM);
    VMCPUID idCpu = pVCpu->idCpu;
    if (VMMGetCpuId(pVM) != idCpu)
    {
        Log7Func(("idCpu=%u enmState=%d\n", idCpu, pVCpu->enmState));
        VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_POKE);
    }
#endif
}


/**
 * Clears the interrupt pending force-flag.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   fIrq            Flag whether to clear the IRQ flag.
 * @param   fFiq            Flag whether to clear the FIQ flag.
 */
DECLINLINE(void) gicClearInterruptFF(PVMCPUCC pVCpu, bool fIrq, bool fFiq)
{
    Assert(fIrq || fFiq);

#ifdef IN_RING3
    /* IRQ state should be loaded as-is by "LoadExec". Changes can be made from LoadDone. */
    Assert(pVCpu->pVMR3->enmVMState != VMSTATE_LOADING || PDMR3HasLoadedState(pVCpu->pVMR3));
#endif

    if (fIrq)
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_IRQ);
    if (fFiq)
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_FIQ);
}


/**
 * Updates the internal IRQ state and sets or clears the appropirate force action flags.
 *
 * @returns Strict VBox status code.
 * @param   pThis           The GIC re-distributor state for the associated vCPU.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static VBOXSTRICTRC gicReDistUpdateIrqState(PGICCPU pThis, PVMCPUCC pVCpu)
{
    /* Read the interrupt state. */
    uint32_t u32RegIGrp0  = ASMAtomicReadU32(&pThis->u32RegIGrp0);
    uint32_t bmIntEnabled = ASMAtomicReadU32(&pThis->bmIntEnabled);
    uint32_t bmIntPending = ASMAtomicReadU32(&pThis->bmIntPending);
    uint32_t bmIntActive  = ASMAtomicReadU32(&pThis->bmIntActive);
    bool fIrqGrp0Enabled  = ASMAtomicReadBool(&pThis->fIrqGrp0Enabled);
    bool fIrqGrp1Enabled  = ASMAtomicReadBool(&pThis->fIrqGrp1Enabled);

    /* Is anything enabled at all? */
    uint32_t bmIntForward = (bmIntPending & bmIntEnabled) & ~bmIntActive; /* Exclude the currently active interrupt. */
    if (bmIntForward)
    {
        /* Determine whether we have to assert the IRQ or FIQ line. */
        bool fIrq = RT_BOOL(bmIntForward & u32RegIGrp0) && fIrqGrp1Enabled;
        bool fFiq = RT_BOOL(bmIntForward & ~u32RegIGrp0) && fIrqGrp0Enabled;

        if (fIrq || fFiq)
            gicSetInterruptFF(pVCpu, fIrq, fFiq);

        if (!fIrq || !fFiq)
            gicClearInterruptFF(pVCpu, !fIrq, !fFiq);
    }
    else
        gicClearInterruptFF(pVCpu, true /*fIrq*/, true /*fFiq*/);

    return VINF_SUCCESS;
}


/**
 * Sets the given SGI/PPI interrupt ID on the re-distributor of the given vCPU.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uIntId          The SGI/PPI interrupt identifier.
 * @param   fAsserted       Flag whether the SGI/PPI interrupt is asserted or not.
 */
static int gicReDistInterruptSet(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    PGICCPU pThis = VMCPU_TO_GICCPU(pVCpu);

    /* Update the interrupts pending state. */
    if (fAsserted)
        ASMAtomicOrU32(&pThis->bmIntPending, RT_BIT_32(uIntId));
    else
        ASMAtomicAndU32(&pThis->bmIntPending, ~RT_BIT_32(uIntId));

    return VBOXSTRICTRC_VAL(gicReDistUpdateIrqState(pThis, pVCpu));
}


/**
 * Reads a GIC distributor register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicDistRegisterRead(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, offReg);

    switch (offReg)
    {
        case GIC_DIST_REG_TYPER_OFF:
            *puValue =   GIC_DIST_REG_TYPER_NUM_ITLINES_SET(1)  /** @todo 32 SPIs for now. */
                       | GIC_DIST_REG_TYPER_NUM_PES_SET(0)      /* 1 PE */
                       /*| GIC_DIST_REG_TYPER_ESPI*/            /** @todo */
                       /*| GIC_DIST_REG_TYPER_NMI*/             /** @todo Non-maskable interrupts */
                       /*| GIC_DIST_REG_TYPER_SECURITY_EXTN */  /** @todo */
                       /*| GIC_DIST_REG_TYPER_MBIS */           /** @todo Message based interrupts */
                       /*| GIC_DIST_REG_TYPER_LPIS */           /** @todo Support LPIs */
                       | GIC_DIST_REG_TYPER_IDBITS_SET(16);
            break;
        case GIC_DIST_REG_PIDR2_OFF:
            *puValue = GIC_REDIST_REG_PIDR2_ARCH_REV_SET(GIC_REDIST_REG_PIDR2_ARCH_REV_GICV3);
            break;
        case GIC_DIST_REG_IIDR_OFF:
        case GIC_DIST_REG_TYPER2_OFF:
        default:
            *puValue = 0;
    }
    return VINF_SUCCESS;
}


/**
 * Writes a GIC distributor register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicDistRegisterWrite(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, offReg, uValue);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    return rcStrict;
}


/**
 * Reads a GIC redistributor register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistRegisterRead(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, offReg);

    switch (offReg)
    {
        case GIC_REDIST_REG_PIDR2_OFF:
            *puValue = GIC_REDIST_REG_PIDR2_ARCH_REV_SET(GIC_REDIST_REG_PIDR2_ARCH_REV_GICV3);
            break;
        default:
            *puValue = 0;
    }

    return VINF_SUCCESS;
}


/**
 * Reads a GIC redistributor SGI/PPI frame register.
 *
 * @returns VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being read.
 * @param   puValue         Where to store the register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistSgiPpiRegisterRead(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t *puValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns);

    PGICCPU pThis = VMCPU_TO_GICCPU(pVCpu);
    switch (offReg)
    {
        case GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF:
        case GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->bmIntEnabled);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF:
        case GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->bmIntPending);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF:
        case GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->bmIntActive);
            break;
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START +  4:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START +  8:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 12:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 16:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 20:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 24:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 28:
        {
            /* Figure out the register whch is written. */
            uint8_t idxPrio = offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));

            uint32_t u32Value = 0;
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
                u32Value |= pThis->abIntPriority[i] << ((i - idxPrio) * 8);

            *puValue = u32Value;
            break;
        }
        case GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF:
            *puValue = ASMAtomicReadU32(&pThis->u32RegICfg0);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICFGR1_OFF:
            *puValue = ASMAtomicReadU32(&pThis->u32RegICfg1);
            break;
        default:
            AssertReleaseFailed();
            *puValue = 0;
    }

    return VINF_SUCCESS;
}


/**
 * Writes a GIC redistributor frame register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistRegisterWrite(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns, pVCpu, offReg, uValue);

    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    return rcStrict;
}


/**
 * Writes a GIC redistributor SGI/PPI frame register.
 *
 * @returns Strict VBox status code.
 * @param   pDevIns         The device instance.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   offReg          The offset of the register being written.
 * @param   uValue          The register value.
 */
DECLINLINE(VBOXSTRICTRC) gicReDistSgiPpiRegisterWrite(PPDMDEVINS pDevIns, PVMCPUCC pVCpu, uint16_t offReg, uint32_t uValue)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pDevIns);

    PGICCPU pThis = VMCPU_TO_GICCPU(pVCpu);
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;
    switch (offReg)
    {
        case GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF:
            ASMAtomicOrU32(&pThis->u32RegIGrp0, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF:
            ASMAtomicOrU32(&pThis->bmIntEnabled, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF:
            ASMAtomicAndU32(&pThis->bmIntEnabled, ~uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF:
            ASMAtomicOrU32(&pThis->bmIntPending, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF:
            ASMAtomicAndU32(&pThis->bmIntPending, ~uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF:
            ASMAtomicOrU32(&pThis->bmIntActive, uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF:
            ASMAtomicAndU32(&pThis->bmIntActive, ~uValue);
            rcStrict = gicReDistUpdateIrqState(pThis, pVCpu);
            break;
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START +  4:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START +  8:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 12:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 16:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 20:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 24:
        case GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START + 28:
        {
            /* Figure out the register whch is written. */
            uint8_t idxPrio = offReg - GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START;
            Assert(idxPrio <= RT_ELEMENTS(pThis->abIntPriority) - sizeof(uint32_t));
            for (uint32_t i = idxPrio; i < idxPrio + sizeof(uint32_t); i++)
            {
                pThis->abIntPriority[i] = (uint8_t)(uValue & 0xff);
                uValue >>= 8;
            }
            break;
        }
        case GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF:
            ASMAtomicWriteU32(&pThis->u32RegICfg0, uValue);
            break;
        case GIC_REDIST_SGI_PPI_REG_ICFGR1_OFF:
            ASMAtomicWriteU32(&pThis->u32RegICfg1, uValue);
            break;
        default:
            AssertReleaseFailed();
    }

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
    PGICCPU pThis = VMCPU_TO_GICCPU(pVCpu);
    switch (u32Reg)
    {
        case ARMV8_AARCH64_SYSREG_ICC_PMR_EL1:
            *pu64Value = pThis->bInterruptPriority;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_EOIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR0_EL1:
            *pu64Value = pThis->bBinaryPointGrp0 & 0x7;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R2_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R3_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R2_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R3_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_NMIAR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_DIR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_RPR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_ASGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI0R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR1_EL1:
        {
            /** @todo Figure out the highest priority interrupt. */
            uint32_t bmPending = ASMAtomicReadU32(&pThis->bmIntPending);
            int32_t idxIntPending = ASMBitFirstSet(&bmPending, sizeof(bmPending) * 8);
            if (idxIntPending > -1)
            {
                /* Mark the interrupt as active. */
                ASMAtomicOrU32(&pThis->bmIntActive, idxIntPending);
                *pu64Value = idxIntPending;
            }
            else
                *pu64Value = GIC_INTID_RANGE_SPECIAL_NO_INTERRUPT;
            break;
        }
        case ARMV8_AARCH64_SYSREG_ICC_EOIR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR1_EL1:
            *pu64Value = pThis->bBinaryPointGrp1 & 0x7;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_CTLR_EL1:
            *pu64Value =   ARMV8_ICC_CTLR_EL1_AARCH64_PMHE
                         | ARMV8_ICC_CTLR_EL1_AARCH64_PRIBITS_SET(4)
                         | ARMV8_ICC_CTLR_EL1_AARCH64_IDBITS_SET(ARMV8_ICC_CTLR_EL1_AARCH64_IDBITS_16BITS);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SRE_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN0_EL1:
            *pu64Value = ASMAtomicReadBool(&pThis->fIrqGrp0Enabled) ? ARMV8_ICC_IGRPEN0_EL1_AARCH64_ENABLE : 0;
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN1_EL1:
            *pu64Value = ASMAtomicReadBool(&pThis->fIrqGrp1Enabled) ? ARMV8_ICC_IGRPEN1_EL1_AARCH64_ENABLE : 0;
            break;
        default:
            AssertReleaseFailed();
            break;
    }

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

    PGICCPU pThis = VMCPU_TO_GICCPU(pVCpu);
    switch (u32Reg)
    {
        case ARMV8_AARCH64_SYSREG_ICC_PMR_EL1:
            ASMAtomicWriteU8(&pThis->bInterruptPriority, (uint8_t)u64Value);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_EOIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR0_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR0_EL1:
            pThis->bBinaryPointGrp0 = (uint8_t)(u64Value & 0x7);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R0_EL1:
            /** @todo */
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R2_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP0R3_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R0_EL1:
            /** @todo */
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R2_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_AP1R3_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_NMIAR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_DIR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_RPR_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_ASGI1R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SGI0R_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IAR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_EOIR1_EL1:
        {
            /* Mark the interrupt as not active anymore, though it might still be pending. */
            Assert(u64Value < GIC_INTID_RANGE_SPI_START);
            ASMAtomicAndU32(&pThis->bmIntActive, (uint32_t)u64Value);
            break;
        }
        case ARMV8_AARCH64_SYSREG_ICC_HPPIR1_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_BPR1_EL1:
            pThis->bBinaryPointGrp0 = (uint8_t)(u64Value & 0x7);
            break;
        case ARMV8_AARCH64_SYSREG_ICC_CTLR_EL1:
            u64Value &= ARMV8_ICC_CTLR_EL1_RW;
            /** @todo */
            break;
        case ARMV8_AARCH64_SYSREG_ICC_SRE_EL1:
            AssertReleaseFailed();
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN0_EL1:
            ASMAtomicWriteBool(&pThis->fIrqGrp0Enabled, RT_BOOL(u64Value & ARMV8_ICC_IGRPEN0_EL1_AARCH64_ENABLE));
            break;
        case ARMV8_AARCH64_SYSREG_ICC_IGRPEN1_EL1:
            ASMAtomicWriteBool(&pThis->fIrqGrp1Enabled, RT_BOOL(u64Value & ARMV8_ICC_IGRPEN1_EL1_AARCH64_ENABLE));
            break;
        default:
            AssertReleaseFailed();
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Sets the specified shared peripheral interrupt starting.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context virtual machine structure.
 * @param   uIntId              The SPI ID (minus GIC_INTID_RANGE_SPI_START) to assert/de-assert.
 * @param   fAsserted           Flag whether to mark the interrupt as asserted/de-asserted.
 */
VMM_INT_DECL(int) GICSpiSet(PVMCC pVM, uint32_t uIntId, bool fAsserted)
{
    RT_NOREF(pVM, uIntId, fAsserted);
    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Sets the specified private peripheral interrupt starting.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uIntId              The PPI ID (minus GIC_INTID_RANGE_PPI_START) to assert/de-assert.
 * @param   fAsserted           Flag whether to mark the interrupt as asserted/de-asserted.
 */
VMM_INT_DECL(int) GICPpiSet(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    AssertReturn(uIntId >= 0 && uIntId <= (GIC_INTID_RANGE_PPI_LAST - GIC_INTID_RANGE_PPI_START), VERR_INVALID_PARAMETER);
    return gicReDistInterruptSet(pVCpu, uIntId + GIC_INTID_RANGE_PPI_START, fAsserted);
}


/**
 * Sets the specified software generated interrupt starting.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uIntId              The PPI ID (minus GIC_INTID_RANGE_SGI_START) to assert/de-assert.
 * @param   fAsserted           Flag whether to mark the interrupt as asserted/de-asserted.
 */
VMM_INT_DECL(int) GICSgiSet(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    AssertReturn(uIntId >= 0 && uIntId <= (GIC_INTID_RANGE_SGI_LAST - GIC_INTID_RANGE_SGI_START), VERR_INVALID_PARAMETER);
    return gicReDistInterruptSet(pVCpu, uIntId + GIC_INTID_RANGE_SGI_START, fAsserted);
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
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = off & 0xfffc;
    uint32_t uValue   = 0;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioRead));

    VBOXSTRICTRC rc = VBOXSTRICTRC_VAL(gicDistRegisterRead(pDevIns, pVCpu, offReg, &uValue));
    *(uint32_t *)pv = uValue;

    Log2(("GIC%u: gicDistMmioRead: offReg=%#RX16 uValue=%#RX32\n", pVCpu->idCpu, offReg, uValue));
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);
    uint16_t offReg   = off & 0xfffc;
    uint32_t uValue   = *(uint32_t *)pv;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioWrite));

    Log2(("GIC%u: gicDistMmioWrite: offReg=%#RX16 uValue=%#RX32\n", pVCpu->idCpu, offReg, uValue));
    return gicDistRegisterWrite(pDevIns, pVCpu, offReg, uValue);
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicReDistMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu    = PDMDevHlpGetVMCPU(pDevIns);

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioRead));

    /*
     * Determine the redistributor being targeted. Each redistributor takes GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE bytes
     * and the redistributors are adjacent.
     */
    uint32_t idReDist = off / (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);
    off %= (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);

    /* Redistributor or SGI/PPI frame? */
    uint16_t offReg = off & 0xfffc;
    uint32_t uValue = 0;
    VBOXSTRICTRC rcStrict;
    if (off < GIC_REDIST_REG_FRAME_SIZE)
        rcStrict = gicReDistRegisterRead(pDevIns, pVCpu, offReg, &uValue);
    else
        rcStrict = gicReDistSgiPpiRegisterRead(pDevIns, pVCpu, offReg, &uValue);

    *(uint32_t *)pv = uValue;
    Log2(("GICReDist%u: gicReDistMmioRead: off=%RGp idReDist=%u offReg=%#RX16 uValue=%#RX32 -> %Rrc\n",
          pVCpu->idCpu, off, idReDist, offReg, uValue, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gicReDistMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    NOREF(pvUser);
    Assert(!(off & 0x3));
    Assert(cb == 4); RT_NOREF_PV(cb);

    PVMCPUCC pVCpu  = PDMDevHlpGetVMCPU(pDevIns);
    uint32_t uValue = *(uint32_t *)pv;

    STAM_COUNTER_INC(&pVCpu->gic.s.CTX_SUFF_Z(StatMmioWrite));

    /*
     * Determine the redistributor being targeted. Each redistributor takes GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE bytes
     * and the redistributors are adjacent.
     */
    uint32_t idReDist = off / (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);
    off %= (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE);

    /* Redistributor or SGI/PPI frame? */
    uint16_t offReg = off & 0xfffc;
    VBOXSTRICTRC rcStrict;
    if (off < GIC_REDIST_REG_FRAME_SIZE)
        rcStrict = gicReDistRegisterWrite(pDevIns, pVCpu, offReg, uValue);
    else
        rcStrict = gicReDistSgiPpiRegisterWrite(pDevIns, pVCpu, offReg, uValue);

    Log2(("GICReDist%u: gicReDistMmioWrite: off=%RGp idReDist=%u offReg=%#RX16 uValue=%#RX32 -> %Rrc\n",
          pVCpu->idCpu, off, idReDist, offReg, uValue, VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
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

