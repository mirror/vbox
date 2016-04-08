/* $Id$ */
/** @file
 * PDM Critical Sections
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM
#include "PDMInternal.h"
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>

#include "PDMInline.h"
#include "dtrace/VBoxVMM.h"



/**
 * Gets the pending interrupt.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pu8Interrupt    Where to store the interrupt on success.
 */
VMMDECL(int) PDMGetInterrupt(PVMCPU pVCpu, uint8_t *pu8Interrupt)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

#ifndef VBOX_WITH_NEW_APIC
    pdmLock(pVM);
#endif

    /*
     * The local APIC has a higher priority than the PIC.
     */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_APIC))
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnGetInterrupt));
        uint32_t uTagSrc;
        int i = pVM->pdm.s.Apic.CTX_SUFF(pfnGetInterrupt)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu, &uTagSrc);
        AssertMsg(i <= 255 && i >= 0, ("i=%d\n", i));
        if (i >= 0)
        {
#ifndef VBOX_WITH_NEW_APIC
            pdmUnlock(pVM);
#endif
            *pu8Interrupt = (uint8_t)i;
            VBOXVMM_PDM_IRQ_GET(pVCpu, RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc), i);
            return VINF_SUCCESS;
        }
    }

#ifdef VBOX_WITH_NEW_APIC
    pdmLock(pVM);
#endif

    /*
     * Check the PIC.
     */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC))
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
        Assert(pVM->pdm.s.Pic.CTX_SUFF(pDevIns));
        Assert(pVM->pdm.s.Pic.CTX_SUFF(pfnGetInterrupt));
        uint32_t uTagSrc;
        int i = pVM->pdm.s.Pic.CTX_SUFF(pfnGetInterrupt)(pVM->pdm.s.Pic.CTX_SUFF(pDevIns), &uTagSrc);
        AssertMsg(i <= 255 && i >= 0, ("i=%d\n", i));
        if (i >= 0)
        {
            pdmUnlock(pVM);
            *pu8Interrupt = (uint8_t)i;
            VBOXVMM_PDM_IRQ_GET(pVCpu, RT_LOWORD(uTagSrc), RT_HIWORD(uTagSrc), i);
            return VINF_SUCCESS;
        }
    }

    /** @todo Figure out exactly why we can get here without anything being set. (REM) */

    pdmUnlock(pVM);
    return VERR_NO_DATA;
}


/**
 * Sets the pending interrupt coming from ISA source or HPET.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level.
 * @param   uTagSrc         The IRQ tag and source tracer ID.
 */
VMMDECL(int) PDMIsaSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level, uint32_t uTagSrc)
{
    pdmLock(pVM);

    /** @todo put the IRQ13 code elsewhere to avoid this unnecessary bloat. */
    if (!uTagSrc && (u8Level & PDM_IRQ_LEVEL_HIGH)) /* FPU IRQ */
    {
        if (u8Level == PDM_IRQ_LEVEL_HIGH)
            VBOXVMM_PDM_IRQ_HIGH(VMMGetCpu(pVM), 0, 0);
        else
            VBOXVMM_PDM_IRQ_HILO(VMMGetCpu(pVM), 0, 0);
    }

    int rc = VERR_PDM_NO_PIC_INSTANCE;
    if (pVM->pdm.s.Pic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Pic.CTX_SUFF(pfnSetIrq));
        pVM->pdm.s.Pic.CTX_SUFF(pfnSetIrq)(pVM->pdm.s.Pic.CTX_SUFF(pDevIns), u8Irq, u8Level, uTagSrc);
        rc = VINF_SUCCESS;
    }

    if (pVM->pdm.s.IoApic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq));

        /*
         * Apply Interrupt Source Override rules.
         * See ACPI 4.0 specification 5.2.12.4 and 5.2.12.5 for details on
         * interrupt source override.
         * Shortly, ISA IRQ0 is electically connected to pin 2 on IO-APIC, and some OSes,
         * notably recent OS X rely upon this configuration.
         * If changing, also update override rules in MADT and MPS.
         */
        /* ISA IRQ0 routed to pin 2, all others ISA sources are identity mapped */
        if (u8Irq == 0)
            u8Irq = 2;

        pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq)(pVM->pdm.s.IoApic.CTX_SUFF(pDevIns), u8Irq, u8Level, uTagSrc);
        rc = VINF_SUCCESS;
    }

    if (!uTagSrc && u8Level == PDM_IRQ_LEVEL_LOW)
        VBOXVMM_PDM_IRQ_LOW(VMMGetCpu(pVM), 0, 0);
    pdmUnlock(pVM);
    return rc;
}


/**
 * Sets the pending I/O APIC interrupt.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   u8Irq       The IRQ line.
 * @param   u8Level     The new level.
 * @param   uTagSrc     The IRQ tag and source tracer ID.
 */
VMM_INT_DECL(int) PDMIoApicSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level, uint32_t uTagSrc)
{
    if (pVM->pdm.s.IoApic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq));
        pdmLock(pVM);
        pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq)(pVM->pdm.s.IoApic.CTX_SUFF(pDevIns), u8Irq, u8Level, uTagSrc);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_PIC_INSTANCE;
}

/**
 * Send a MSI to an I/O APIC.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   GCAddr      Request address.
 * @param   uValue      Request value.
 * @param   uTagSrc     The IRQ tag and source tracer ID.
 */
VMM_INT_DECL(int) PDMIoApicSendMsi(PVM pVM, RTGCPHYS GCAddr, uint32_t uValue, uint32_t uTagSrc)
{
    if (pVM->pdm.s.IoApic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTX_SUFF(pfnSendMsi));
        pdmLock(pVM);
        pVM->pdm.s.IoApic.CTX_SUFF(pfnSendMsi)(pVM->pdm.s.IoApic.CTX_SUFF(pDevIns), GCAddr, uValue, uTagSrc);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_PIC_INSTANCE;
}



/**
 * Returns the presence of an IO-APIC.
 *
 * @returns VBox true if an IO-APIC is present.
 * @param   pVM         The cross context VM structure.
 */
VMM_INT_DECL(bool) PDMHasIoApic(PVM pVM)
{
    return pVM->pdm.s.IoApic.CTX_SUFF(pDevIns) != NULL;
}


/**
 * Returns the presence of a Local APIC.
 *
 * @returns VBox true if a Local APIC is present.
 * @param   pVM         The cross context VM structure.
 */
VMM_INT_DECL(bool) PDMHasApic(PVM pVM)
{
    return pVM->pdm.s.Apic.CTX_SUFF(pDevIns) != NULL;
}


/**
 * Set the APIC base.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   u64Base     The new base.
 */
VMMDECL(VBOXSTRICTRC) PDMApicSetBaseMsr(PVMCPU pVCpu, uint64_t u64Base)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnSetBaseMsr));
#ifndef VBOX_WITH_NEW_APIC
        pdmLock(pVM);
#endif
        VBOXSTRICTRC rcStrict = pVM->pdm.s.Apic.CTX_SUFF(pfnSetBaseMsr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu, u64Base);

        /* Update CPUM's copy of the APIC base. */
        PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);
        Assert(pCtx);
        pCtx->msrApicBase = pVM->pdm.s.Apic.CTX_SUFF(pfnGetBaseMsr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu);

#ifndef VBOX_WITH_NEW_APIC
        pdmUnlock(pVM);
#endif
        return rcStrict;
    }

#ifdef IN_RING3
    LogRelMax(5, ("PDM: APIC%U: Writing APIC base MSR (%#x) invalid since there isn't an APIC -> #GP(0)\n", pVCpu->idCpu,
                  MSR_IA32_APICBASE));
    return VERR_CPUM_RAISE_GP_0;
#else
    return VINF_CPUM_R3_MSR_WRITE;
#endif
}


/**
 * Get the APIC base MSR from the APIC device.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pu64Base        Where to store the APIC base.
 * @param   fIgnoreErrors   Whether to ignore errors (i.e. not a real guest MSR
 *                          access).
 */
VMMDECL(VBOXSTRICTRC) PDMApicGetBaseMsr(PVMCPU pVCpu, uint64_t *pu64Base, bool fIgnoreErrors)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnGetBaseMsr));
#ifdef VBOX_WITH_NEW_APIC
        VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);
        *pu64Base = pVM->pdm.s.Apic.CTX_SUFF(pfnGetBaseMsr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu);
#else
        pdmLock(pVM);
        *pu64Base = pVM->pdm.s.Apic.CTX_SUFF(pfnGetBaseMsr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu);
        pdmUnlock(pVM);
#endif
        return VINF_SUCCESS;
    }

    *pu64Base = 0;
    if (fIgnoreErrors)
        return VINF_SUCCESS;

#ifdef IN_RING3
    LogRelMax(5, ("PDM: APIC%u: Reading APIC base MSR (%#x) invalid without an APIC instance -> #GP(0)\n", pVCpu->idCpu,
                  MSR_IA32_APICBASE));
    return VERR_CPUM_RAISE_GP_0;
#else
    return VINF_CPUM_R3_MSR_WRITE;
#endif
}


/**
 * Check if the APIC has a pending interrupt/if a TPR change would active one.
 *
 * @returns VINF_SUCCESS or VERR_PDM_NO_APIC_INSTANCE.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pfPending   Pending state (out).
 */
VMM_INT_DECL(int) PDMApicHasPendingIrq(PVMCPU pVCpu, bool *pfPending)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnHasPendingIrq));
        pdmLock(pVM);
        *pfPending = pVM->pdm.s.Apic.CTX_SUFF(pfnHasPendingIrq)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu, NULL /*pu8PendingIrq*/);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Set the TPR (task priority register).
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u8TPR           The new TPR.
 */
VMMDECL(int) PDMApicSetTPR(PVMCPU pVCpu, uint8_t u8TPR)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnSetTpr));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTX_SUFF(pfnSetTpr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu, u8TPR);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Get the TPR (task priority register).
 *
 * @returns VINF_SUCCESS or VERR_PDM_NO_APIC_INSTANCE.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pu8TPR          Where to store the TRP.
 * @param   pfPending       Pending interrupt state (out, optional).
 * @param   pu8PendingIrq   Where to store the highest-priority pending IRQ
 *                          (out, optional).
 *
 * @remarks No-long-jump zone!!!
 */
VMMDECL(int) PDMApicGetTPR(PVMCPU pVCpu, uint8_t *pu8TPR, bool *pfPending, uint8_t *pu8PendingIrq)
{
    PVM        pVM      = pVCpu->CTX_SUFF(pVM);
    PPDMDEVINS pApicIns = pVM->pdm.s.Apic.CTX_SUFF(pDevIns);
    if (pApicIns)
    {
        /*
         * Note! We don't acquire the PDM lock here as we're just reading
         *       information. Doing so causes massive contention as this
         *       function is called very often by each and every VCPU.
         */
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnGetTpr));
        *pu8TPR = pVM->pdm.s.Apic.CTX_SUFF(pfnGetTpr)(pApicIns, pVCpu);
        if (pfPending)
            *pfPending = pVM->pdm.s.Apic.CTX_SUFF(pfnHasPendingIrq)(pApicIns, pVCpu, pu8PendingIrq);
        return VINF_SUCCESS;
    }
    *pu8TPR = 0;
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Write a MSR in APIC range.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          MSR to write.
 * @param   u64Value        Value to write.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicWriteMsr(PVMCPU pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        AssertPtr(pVM->pdm.s.Apic.CTX_SUFF(pfnWriteMsr));
        return pVM->pdm.s.Apic.CTX_SUFF(pfnWriteMsr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu, u32Reg, u64Value);
    }
    return VERR_CPUM_RAISE_GP_0;
}


/**
 * Read a MSR in APIC range.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          MSR to read.
 * @param   pu64Value       Where to store the value read.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicReadMsr(PVMCPU pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        AssertPtr(pVM->pdm.s.Apic.CTX_SUFF(pfnReadMsr));
        return pVM->pdm.s.Apic.CTX_SUFF(pfnReadMsr)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), pVCpu, u32Reg, pu64Value);
    }
    return VERR_CPUM_RAISE_GP_0;
}


/**
 * Gets the frequency of the APIC timer.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pu64Value       Where to store the frequency.
 */
VMM_INT_DECL(int) PDMApicGetTimerFreq(PVM pVM, uint64_t *pu64Value)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        AssertPtr(pVM->pdm.s.Apic.CTX_SUFF(pfnGetTimerFreq));
        *pu64Value = pVM->pdm.s.Apic.CTX_SUFF(pfnGetTimerFreq)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Locks PDM.
 * This might call back to Ring-3 in order to deal with lock contention in GC and R3.
 *
 * @param   pVM     The cross context VM structure.
 */
void pdmLock(PVM pVM)
{
#ifdef IN_RING3
    int rc = PDMCritSectEnter(&pVM->pdm.s.CritSect, VERR_IGNORED);
#else
    int rc = PDMCritSectEnter(&pVM->pdm.s.CritSect, VERR_GENERAL_FAILURE);
    if (rc == VERR_GENERAL_FAILURE)
        rc = VMMRZCallRing3NoCpu(pVM, VMMCALLRING3_PDM_LOCK, 0);
#endif
    AssertRC(rc);
}


/**
 * Locks PDM but don't go to ring-3 if it's owned by someone.
 *
 * @returns VINF_SUCCESS on success.
 * @returns rc if we're in GC or R0 and can't get the lock.
 * @param   pVM     The cross context VM structure.
 * @param   rc      The RC to return in GC or R0 when we can't get the lock.
 */
int pdmLockEx(PVM pVM, int rc)
{
    return PDMCritSectEnter(&pVM->pdm.s.CritSect, rc);
}


/**
 * Unlocks PDM.
 *
 * @param   pVM     The cross context VM structure.
 */
void pdmUnlock(PVM pVM)
{
    PDMCritSectLeave(&pVM->pdm.s.CritSect);
}


/**
 * Converts ring 3 VMM heap pointer to a guest physical address
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pv              Ring-3 pointer.
 * @param   pGCPhys         GC phys address (out).
 */
VMM_INT_DECL(int) PDMVmmDevHeapR3ToGCPhys(PVM pVM, RTR3PTR pv, RTGCPHYS *pGCPhys)
{
    if (RT_LIKELY(pVM->pdm.s.GCPhysVMMDevHeap != NIL_RTGCPHYS))
    {
        RTR3UINTPTR const offHeap = (RTR3UINTPTR)pv - (RTR3UINTPTR)pVM->pdm.s.pvVMMDevHeap;
        if (RT_LIKELY(offHeap < pVM->pdm.s.cbVMMDevHeap))
        {
            *pGCPhys = pVM->pdm.s.GCPhysVMMDevHeap + offHeap;
            return VINF_SUCCESS;
        }

        /* Don't assert here as this is called before we can catch ring-0 assertions. */
        Log(("PDMVmmDevHeapR3ToGCPhys: pv=%p pvVMMDevHeap=%p cbVMMDevHeap=%#x\n",
             pv, pVM->pdm.s.pvVMMDevHeap, pVM->pdm.s.cbVMMDevHeap));
    }
    else
        Log(("PDMVmmDevHeapR3ToGCPhys: GCPhysVMMDevHeap=%RGp (pv=%p)\n", pVM->pdm.s.GCPhysVMMDevHeap, pv));
    return VERR_PDM_DEV_HEAP_R3_TO_GCPHYS;
}


/**
 * Checks if the vmm device heap is enabled (== vmm device's pci region mapped)
 *
 * @returns dev heap enabled status (true/false)
 * @param   pVM             The cross context VM structure.
 */
VMM_INT_DECL(bool) PDMVmmDevHeapIsEnabled(PVM pVM)
{
    return pVM->pdm.s.GCPhysVMMDevHeap != NIL_RTGCPHYS;
}
