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
 * @param   pVM             VM handle.
 * @param   pu8Interrupt    Where to store the interrupt on success.
 */
PDMDECL(int) PDMGetInterrupt(PVM pVM, uint8_t *pu8Interrupt)
{
    pdmLock(pVM);

    /*
     * The local APIC has a higer priority than the PIC.
     */
    if (VM_FF_ISSET(pVM, VM_FF_INTERRUPT_APIC))
    {
        VM_FF_CLEAR(pVM, VM_FF_INTERRUPT_APIC);
        Assert(pVM->pdm.s.Apic.CTXALLSUFF(pDevIns));
        Assert(pVM->pdm.s.Apic.CTXALLSUFF(pfnGetInterrupt));
        int i = pVM->pdm.s.Apic.CTXALLSUFF(pfnGetInterrupt)(pVM->pdm.s.Apic.CTXALLSUFF(pDevIns));
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
    if (VM_FF_ISSET(pVM, VM_FF_INTERRUPT_PIC))
    {
        VM_FF_CLEAR(pVM, VM_FF_INTERRUPT_PIC);
        Assert(pVM->pdm.s.Pic.CTXALLSUFF(pDevIns));
        Assert(pVM->pdm.s.Pic.CTXALLSUFF(pfnGetInterrupt));
        int i = pVM->pdm.s.Pic.CTXALLSUFF(pfnGetInterrupt)(pVM->pdm.s.Pic.CTXALLSUFF(pDevIns));
        AssertMsg(i <= 255 && i >= 0, ("i=%d\n", i));
        if (i >= 0)
        {
            pdmUnlock(pVM);
            *pu8Interrupt = (uint8_t)i;
            return VINF_SUCCESS;
        }
    }

#ifndef VBOX_WITH_PDM_LOCK /** @todo Figure out exactly why we can get here without anything being set. (REM) */
    /* Shouldn't get here! Noone should call us without cause. */
    Assert(VM_FF_ISPENDING(pVM, VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC));
#endif
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
PDMDECL(int) PDMIsaSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level)
{
    pdmLock(pVM);

    int rc = VERR_PDM_NO_PIC_INSTANCE;
    if (pVM->pdm.s.Pic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Pic.CTXALLSUFF(pfnSetIrq));
        pVM->pdm.s.Pic.CTXALLSUFF(pfnSetIrq)(pVM->pdm.s.Pic.CTXALLSUFF(pDevIns), u8Irq, u8Level);
        rc = VINF_SUCCESS;
    }

    if (pVM->pdm.s.IoApic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTXALLSUFF(pfnSetIrq));
        pVM->pdm.s.IoApic.CTXALLSUFF(pfnSetIrq)(pVM->pdm.s.IoApic.CTXALLSUFF(pDevIns), u8Irq, u8Level);
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
PDMDECL(int) PDMIoApicSetIrq(PVM pVM, uint8_t u8Irq, uint8_t u8Level)
{
    if (pVM->pdm.s.IoApic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.IoApic.CTXALLSUFF(pfnSetIrq));
        pdmLock(pVM);
        pVM->pdm.s.IoApic.CTXALLSUFF(pfnSetIrq)(pVM->pdm.s.IoApic.CTXALLSUFF(pDevIns), u8Irq, u8Level);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_PIC_INSTANCE;
}


/**
 * Set the APIC base.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u64Base         The new base.
 */
PDMDECL(int) PDMApicSetBase(PVM pVM, uint64_t u64Base)
{
    if (pVM->pdm.s.Apic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTXALLSUFF(pfnSetBase));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTXALLSUFF(pfnSetBase)(pVM->pdm.s.Apic.CTXALLSUFF(pDevIns), u64Base);
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
PDMDECL(int) PDMApicGetBase(PVM pVM, uint64_t *pu64Base)
{
    if (pVM->pdm.s.Apic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTXALLSUFF(pfnGetBase));
        pdmLock(pVM);
        *pu64Base = pVM->pdm.s.Apic.CTXALLSUFF(pfnGetBase)(pVM->pdm.s.Apic.CTXALLSUFF(pDevIns));
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    *pu64Base = 0;
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Set the TPR (task priority register?).
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   u8TPR           The new TPR.
 */
PDMDECL(int) PDMApicSetTPR(PVM pVM, uint8_t u8TPR)
{
    if (pVM->pdm.s.Apic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTXALLSUFF(pfnSetTPR));
        pdmLock(pVM);
        pVM->pdm.s.Apic.CTXALLSUFF(pfnSetTPR)(pVM->pdm.s.Apic.CTXALLSUFF(pDevIns), u8TPR);
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * Get the TPR (task priority register?).
 *
 * @returns The current TPR.
 * @param   pVM             VM handle.
 * @param   pu8TPR          Where to store the TRP.
 */
PDMDECL(int) PDMApicGetTPR(PVM pVM, uint8_t *pu8TPR)
{
    if (pVM->pdm.s.Apic.CTXALLSUFF(pDevIns))
    {
        Assert(pVM->pdm.s.Apic.CTXALLSUFF(pfnGetTPR));
        pdmLock(pVM);
        *pu8TPR = pVM->pdm.s.Apic.CTXALLSUFF(pfnGetTPR)(pVM->pdm.s.Apic.CTXALLSUFF(pDevIns));
        pdmUnlock(pVM);
        return VINF_SUCCESS;
    }
    *pu8TPR = 0;
    return VERR_PDM_NO_APIC_INSTANCE;
}


#ifdef VBOX_WITH_PDM_LOCK
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
# ifdef IN_GC
        rc = VMMGCCallHost(pVM, VMMCALLHOST_PDM_LOCK, 0);
#else
        rc = VMMR0CallHost(pVM, VMMCALLHOST_PDM_LOCK, 0);
#endif
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
#endif /* VBOX_WITH_PDM_LOCK */

