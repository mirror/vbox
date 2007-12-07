/* $Id$ */
/** @file
 * MM - Memory Monitor(/Manager) - Page Pool.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_MM_POOL
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/stam.h>
#include "MMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#define USE_INLINE_ASM_BIT_OPS
#ifdef USE_INLINE_ASM_BIT_OPS
# include <iprt/asm.h>
#endif



/**
 * Converts a pool address to a physical address.
 * The specified allocation type must match with the address.
 *
 * @returns Physical address.
 * @returns NIL_RTHCPHYS if not found or eType is not matching.
 * @param   pPool   Pointer to the page pool.
 * @param   pv      The address to convert.
 * @thread  The Emulation Thread.
 */
MMDECL(RTHCPHYS) mmPagePoolPtr2Phys(PMMPAGEPOOL pPool, void *pv)
{
#ifdef IN_RING3
    VM_ASSERT_EMT(pPool->pVM);
#endif
    /*
     * Lookup the virtual address.
     */
    PMMPPLOOKUPHCPTR pLookup = (PMMPPLOOKUPHCPTR)RTAvlPVGetBestFit(&pPool->pLookupVirt, pv, false);
    if (pLookup)
    {
        unsigned iPage = ((char *)pv - (char *)pLookup->pSubPool->pvPages) >> PAGE_SHIFT;
        if (iPage < pLookup->pSubPool->cPages)
        {
            /*
             * Convert the virtual address to a physical address.
             */
            STAM_COUNTER_INC(&pPool->cToPhysCalls);
            AssertMsg(     pLookup->pSubPool->paPhysPages[iPage].Phys
                      &&   !(pLookup->pSubPool->paPhysPages[iPage].Phys & PAGE_OFFSET_MASK),
                      ("Phys=%#x\n", pLookup->pSubPool->paPhysPages[iPage].Phys));
            AssertMsg((uintptr_t)pLookup->pSubPool == pLookup->pSubPool->paPhysPages[iPage].uReserved,
                      ("pSubPool=%p uReserved=%p\n", pLookup->pSubPool, pLookup->pSubPool->paPhysPages[iPage].uReserved));
            return pLookup->pSubPool->paPhysPages[iPage].Phys + ((uintptr_t)pv & PAGE_OFFSET_MASK);
        }
    }
    return NIL_RTHCPHYS;
}

/**
 * Converts a pool physical address to a linear address.
 * The specified allocation type must match with the address.
 *
 * @returns Physical address.
 * @returns NULL if not found or eType is not matching.
 * @param   pPool       Pointer to the page pool.
 * @param   HCPhys      The address to convert.
 * @thread  The Emulation Thread.
 */
MMDECL(void *) mmPagePoolPhys2Ptr(PMMPAGEPOOL pPool, RTHCPHYS HCPhys)
{
#if 0 /** @todo have to fix the debugger, but until then this is going on my nevers. */
#ifdef IN_RING3
    VM_ASSERT_EMT(pPool->pVM);
#endif
#endif

    /*
     * Lookup the virtual address.
     */
    PMMPPLOOKUPHCPHYS pLookup = (PMMPPLOOKUPHCPHYS)RTAvlHCPhysGet(&pPool->pLookupPhys, HCPhys & X86_PTE_PAE_PG_MASK);
    if (pLookup)
    {
        STAM_COUNTER_INC(&pPool->cToVirtCalls);
        PSUPPAGE        pPhysPage = pLookup->pPhysPage;
        PMMPAGESUBPOOL  pSubPool = (PMMPAGESUBPOOL)pPhysPage->uReserved;
        unsigned        iPage = pPhysPage - pSubPool->paPhysPages;
        return (char *)pSubPool->pvPages + (HCPhys & PAGE_OFFSET_MASK) + (iPage << PAGE_SHIFT);
    }
    return NULL;
}


/**
 * Convert a page in the page pool to a HC physical address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns Physical address for the specified page table.
 * @param   pVM         VM handle.
 * @param   pvPage      Page which physical address we query.
 * @thread  The Emulation Thread.
 */
MMDECL(RTHCPHYS) MMPage2Phys(PVM pVM, void *pvPage)
{
    RTHCPHYS HCPhys = mmPagePoolPtr2Phys(pVM->mm.s.pPagePool, pvPage);
    if (HCPhys == NIL_RTHCPHYS)
    {
        HCPhys = mmPagePoolPtr2Phys(pVM->mm.s.pPagePoolLow, pvPage);
        if (HCPhys == NIL_RTHCPHYS)
        {
            STAM_COUNTER_INC(&pVM->mm.s.pPagePool->cErrors);
            AssertMsgFailed(("Invalid pvPage=%p specified\n", pvPage));
        }
    }
    return HCPhys;
}


/**
 * Convert physical address of a page to a HC virtual address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns Pointer to the page at that physical address.
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of a page.
 * @thread  The Emulation Thread.
 */
MMDECL(void *) MMPagePhys2Page(PVM pVM, RTHCPHYS HCPhysPage)
{
    void *pvPage = mmPagePoolPhys2Ptr(pVM->mm.s.pPagePool, HCPhysPage);
    if (!pvPage)
    {
        pvPage = mmPagePoolPhys2Ptr(pVM->mm.s.pPagePoolLow, HCPhysPage);
        if (!pvPage)
        {
            STAM_COUNTER_INC(&pVM->mm.s.pPagePool->cErrors);
            AssertMsg(pvPage, ("Invalid HCPhysPage=%VHp specified\n", HCPhysPage));
        }
    }
    return pvPage;
}


/**
 * Convert physical address of a page to a HC virtual address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of a page.
 * @param   ppvPage     Where to store the address corresponding to HCPhysPage.
 * @thread  The Emulation Thread.
 */
MMDECL(int) MMPagePhys2PageEx(PVM pVM, RTHCPHYS HCPhysPage, void **ppvPage)
{
    void *pvPage = mmPagePoolPhys2Ptr(pVM->mm.s.pPagePool, HCPhysPage);
    if (!pvPage)
    {
        pvPage = mmPagePoolPhys2Ptr(pVM->mm.s.pPagePoolLow, HCPhysPage);
        if (!pvPage)
        {
            STAM_COUNTER_INC(&pVM->mm.s.pPagePool->cErrors);
            AssertMsg(pvPage, ("Invalid HCPhysPage=%VHp specified\n", HCPhysPage));
            return VERR_INVALID_POINTER;
        }
    }
    *ppvPage = pvPage;
    return VINF_SUCCESS;
}


/**
 * Try convert physical address of a page to a HC virtual address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of a page.
 * @param   ppvPage     Where to store the address corresponding to HCPhysPage.
 * @thread  The Emulation Thread.
 */
MMDECL(int) MMPagePhys2PageTry(PVM pVM, RTHCPHYS HCPhysPage, void **ppvPage)
{
    void *pvPage = mmPagePoolPhys2Ptr(pVM->mm.s.pPagePool, HCPhysPage);
    if (!pvPage)
    {
        pvPage = mmPagePoolPhys2Ptr(pVM->mm.s.pPagePoolLow, HCPhysPage);
        if (!pvPage)
            return VERR_INVALID_POINTER;
    }
    *ppvPage = pvPage;
    return VINF_SUCCESS;
}
