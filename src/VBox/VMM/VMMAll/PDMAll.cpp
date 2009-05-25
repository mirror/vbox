/* $Id$ */
/** @file
 * PDM Critical Sections
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>


/**
 * Gets the pending interrupt.
 *
 * @returns VBox status code.
 * @param   pVCpu           VMCPU handle.
 * @param   pu8Interrupt    Where to store the interrupt on success.
 */
VMMDECL(int) PDMGetInterrupt(PVMCPU pVCpu, uint8_t *pu8Interrupt)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    pdmLock(pVM);

    /*
     * The local APIC has a higer priority than the PIC.
     */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_APIC))
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_APIC);
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnGetInterrupt));
        int i = pVM->pdm.s.Apic.CTX_SUFF(pfnGetInterrupt)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        AssertMsg(i <= 255 && i >= 0, ("i=%d\n", i));
        if (i >= 0)
        {
            pdmUnlock(pVM);
            *pu8Interrupt = (uint8_t)i;
            return VINF_SUCCESS;
        }
    }

    /*
     * Check the PIC.
     */
    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_INTERRUPT_PIC))
    {
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
        Assert(pVM->pdm.s.Pic.CTX_SUFF(pDevIns));
        Assert(pVM->pdm.s.Pic.CTX_SUFF(pfnGetInterrupt));
        int i = pVM->pdm.s.Pic.CTX_SUFF(pfnGetInterrupt)(pVM->pdm.s.Pic.CTX_SUFF(pDevIns));
        AssertMsg(i <= 255 && i >= 0, ("i=%d\n", i));
        if (i >= 0)
        {
            pdmUnlock(pVM);
            *pu8Interrupt = (uint8_t)i;
            return VINF_SUCCESS;
        }
    }

    /** @todo Figure out exactly why we can get here without anything being set. (REM) */

    pdmUnlock(pVM);
    return VERR_NO_DATA;
}


/**
 * Sets the pending interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level.
 */
VMMDECL(int) PDMIsaSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level)
{
    pdmLock(pVM);

    int rc = VERR_PDM_NO_PIC_INSTANCE;
    if (pVM->pdm.s.Pic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Pic.CTX_SUFF(pfnSetIrq));
        pVM->pdm.s.Pic.CTX_SUFF(pfnSetIrq)(pVM->pdm.s.Pic.CTX_SUFF(pDevIns), u8Irq, u8Level);
        rc = VINF_SUCCESS;
    }

    if (pVM->pdm.s.IoApic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq));
        pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq)(pVM->pdm.s.IoApic.CTX_SUFF(pDevIns), u8Irq, u8Level);
        rc = VINF_SUCCESS;
    }

    pdmUnlock(pVM);
    return rc;
}


/**
 * Sets the pending I/O APIC interrupt.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8Irq           The IRQ line.
 * @param   u8Level         The new level.
 */
VMMDECL(int) PDMIoApicSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level)
{
    if (pVM->pdm.s.IoApic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq));
        pdmLock(pVM);
        pVM->pdm.s.IoApic.CTX_SUFF(pfnSetIrq)(pVM->pdm.s.IoApic.CTX_SUFF(pDevIns), u8Irq, u8Level);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_PIC_INSTANCE;
}


/**
 * Returns presence of an IO-APIC
 *
 * @returns VBox true if IO-APIC is present
 * @param   pVM             VM handle.
 */
VMMDECL(bool) PDMHasIoApic(PVM pVM)
{
    return pVM->pdm.s.IoApic.CTX_SUFF(pDevIns) != NULL;
}


/**
 * Set the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u64Base         The new base.
 */
VMMDECL(int) PDMApicSetBase(PVM pVM, uint64_t u64Base)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnSetBase));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTX_SUFF(pfnSetBase)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), u64Base);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Get the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pu64Base        Where to store the APIC base.
 */
VMMDECL(int) PDMApicGetBase(PVM pVM, uint64_t *pu64Base)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnGetBase));
        pdmLock(pVM);
        *pu64Base = pVM->pdm.s.Apic.CTX_SUFF(pfnGetBase)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    *pu64Base = 0;
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Check if the APIC has a pending interrupt/if a TPR change would active one.
 *
 * @returns VINF_SUCCESS or VERR_PDM_NO_APIC_INSTANCE.
 * @param   pDevIns         Device instance of the APIC.
 * @param   pfPending       Pending state (out).
 */
VMMDECL(int) PDMApicHasPendingIrq(PVM pVM, bool *pfPending)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnSetTPR));
        pdmLock(pVM);
        *pfPending = pVM->pdm.s.Apic.CTX_SUFF(pfnHasPendingIrq)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Set the TPR (task priority register?).
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8TPR           The new TPR.
 */
VMMDECL(int) PDMApicSetTPR(PVM pVM, uint8_t u8TPR)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnSetTPR));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTX_SUFF(pfnSetTPR)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), u8TPR);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Get the TPR (task priority register).
 *
 * @returns The current TPR.
 * @param   pVM             VM handle.
 * @param   pu8TPR          Where to store the TRP.
 * @param   pfPending       Pending interrupt state (out).
*/
VMMDECL(int) PDMApicGetTPR(PVM pVM, uint8_t *pu8TPR, bool *pfPending)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnGetTPR));
        pdmLock(pVM);
        *pu8TPR = pVM->pdm.s.Apic.CTX_SUFF(pfnGetTPR)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        if (pfPending)
            *pfPending = pVM->pdm.s.Apic.CTX_SUFF(pfnHasPendingIrq)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns));
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    *pu8TPR = 0;
    return VERR_PDM_NO_APIC_INSTANCE;
}

/**
 * Write MSR in APIC range.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   iCpu            Target CPU.
 * @param   u32Reg          MSR to write.
 * @param   u64Value        Value to write.
 */
VMMDECL(int) PDMApicWriteMSR(PVM pVM, VMCPUID iCpu, uint32_t u32Reg, uint64_t u64Value)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnWriteMSR));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTX_SUFF(pfnWriteMSR)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), iCpu, u32Reg, u64Value);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}

/**
 * Read MSR in APIC range.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   iCpu            Target CPU.
 * @param   u32Reg          MSR to read.
 * @param   pu64Value       Value read.
 */
VMMDECL(int) PDMApicReadMSR(PVM pVM, VMCPUID iCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    if (pVM->pdm.s.Apic.CTX_SUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTX_SUFF(pfnReadMSR));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTX_SUFF(pfnReadMSR)(pVM->pdm.s.Apic.CTX_SUFF(pDevIns), iCpu, u32Reg, pu64Value);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Locks PDM.
 * This might call back to Ring-3 in order to deal with lock contention in GC and R3.
 *
 * @param   pVM     The VM handle.
 */
void pdmLock(PVM pVM)
{
#ifdef IN_RING3
    int rc = PDMCritSectEnter(&pVM->pdm.s.CritSect, VERR_INTERNAL_ERROR);
#else
    int rc = PDMCritSectEnter(&pVM->pdm.s.CritSect, VERR_GENERAL_FAILURE);
    if (rc == VERR_GENERAL_FAILURE)
    {
# ifdef IN_RC
        rc = VMMGCCallHost(pVM, VMMCALLHOST_PDM_LOCK, 0);
# else
        rc = VMMR0CallHost(pVM, VMMCALLHOST_PDM_LOCK, 0);
# endif
    }
#endif
    AssertRC(rc);
}


/**
 * Locks PDM but don't go to ring-3 if it's owned by someone.
 *
 * @returns VINF_SUCCESS on success.
 * @returns rc if we're in GC or R0 and can't get the lock.
 * @param   pVM     The VM handle.
 * @param   rc      The RC to return in GC or R0 when we can't get the lock.
 */
int pdmLockEx(PVM pVM, int rc)
{
    return PDMCritSectEnter(&pVM->pdm.s.CritSect, rc);
}


/**
 * Unlocks PDM.
 *
 * @param   pVM     The VM handle.
 */
void pdmUnlock(PVM pVM)
{
    PDMCritSectLeave(&pVM->pdm.s.CritSect);
}


/**
 * Converts ring 3 VMM heap pointer to a guest physical address
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pv              Ring-3 pointer.
 * @param   pGCPhys         GC phys address (out).
 */
VMMDECL(int) PDMVMMDevHeapR3ToGCPhys(PVM pVM, RTR3PTR pv, RTGCPHYS *pGCPhys)
{
    AssertReturn(pv >= pVM->pdm.s.pvVMMDevHeap && (RTR3UINTPTR)pv < (RTR3UINTPTR)pVM->pdm.s.pvVMMDevHeap + pVM->pdm.s.cbVMMDevHeap, VERR_INVALID_PARAMETER);

    *pGCPhys = (pVM->pdm.s.GCPhysVMMDevHeap + ((RTR3UINTPTR)pv - (RTR3UINTPTR)pVM->pdm.s.pvVMMDevHeap));
    return VINF_SUCCESS;
}
