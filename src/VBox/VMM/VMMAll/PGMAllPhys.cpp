/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/** @def PGM_IGNORE_RAM_FLAGS_RESERVED
 * Don't respect the MM_RAM_FLAGS_RESERVED flag when converting to HC addresses.
 *
 * Since this flag is currently incorrectly kept set for ROM regions we will
 * have to ignore it for now so we don't break stuff.
 */
#define PGM_IGNORE_RAM_FLAGS_RESERVED


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM_PHYS
#include <VBox/pgm.h>
#include <VBox/trpm.h>
#include <VBox/vmm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <VBox/log.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif



/**
 * Checks if Address Gate 20 is enabled or not.
 *
 * @returns true if enabled.
 * @returns false if disabled.
 * @param   pVM     VM handle.
 */
PGMDECL(bool) PGMPhysIsA20Enabled(PVM pVM)
{
    LogFlow(("PGMPhysIsA20Enabled %d\n", pVM->pgm.s.fA20Enabled));
    return !!pVM->pgm.s.fA20Enabled ; /* stupid MS compiler doesn't trust me. */
}


/**
 * Validates a GC physical address.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The physical address to validate.
 */
PGMDECL(bool) PGMPhysIsGCPhysValid(PVM pVM, RTGCPHYS GCPhys)
{
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
            return true;
    }
    return false;
}


/**
 * Checks if a GC physical address is a normal page,
 * i.e. not ROM, MMIO or reserved.
 *
 * @returns true if normal.
 * @returns false if invalid, ROM, MMIO or reserved page.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The physical address to check.
 */
PGMDECL(bool) PGMPhysIsGCPhysNormal(PVM pVM, RTGCPHYS GCPhys)
{
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
            return !(pRam->aHCPhys[off >> PAGE_SHIFT] & (MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO2));
    }
    return false;
}


/**
 * Converts a GC physical address to a HC physical address.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
PGMDECL(int) PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys)
{
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            if (    pRam->pvHC
                ||  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
            {
                unsigned iPage = off >> PAGE_SHIFT;
                if (RT_UNLIKELY(!(pRam->aHCPhys[iPage] & X86_PTE_PAE_PG_MASK)))
                {
#ifdef IN_RING3
                    int rc = pgmr3PhysGrowRange(pVM, GCPhys);
#else
                    int rc = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhys);
#endif
                    if (rc != VINF_SUCCESS)
                        return rc;
                }

                RTHCPHYS HCPhys = pRam->aHCPhys[off >> PAGE_SHIFT];
#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
                if (!(HCPhys & MM_RAM_FLAGS_RESERVED))
#endif
                {
                    *pHCPhys = (HCPhys & X86_PTE_PAE_PG_MASK)
                             | (off & PAGE_OFFSET_MASK);
                    return VINF_SUCCESS;
                }
            }
            return VERR_PGM_PHYS_PAGE_RESERVED;
        }
    }
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Converts a GC physical address to a HC pointer.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @returns VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY if the range crosses
 *          a dynamic ram chunk boundary
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to convert.
 * @param   cbRange Physical range
 * @param   pHCPtr  Where to store the HC pointer on success.
 */
PGMDECL(int) PGMPhysGCPhys2HCPtr(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR pHCPtr)
{
#ifdef PGM_DYNAMIC_RAM_ALLOC
    if ((GCPhys & PGM_DYNAMIC_CHUNK_BASE_MASK) != ((GCPhys+cbRange-1) & PGM_DYNAMIC_CHUNK_BASE_MASK))
    {
        AssertMsgFailed(("PGMPhysGCPhys2HCPtr %VGp - %VGp crosses a chunk boundary!!\n", GCPhys, GCPhys+cbRange));
        return VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY;
    }
#endif

    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
            {
                unsigned iPage = off >> PAGE_SHIFT;
                if (RT_UNLIKELY(!(pRam->aHCPhys[iPage] & X86_PTE_PAE_PG_MASK)))
                {
#ifdef IN_RING3
                    int rc = pgmr3PhysGrowRange(pVM, GCPhys);
#else
                    int rc = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhys);
#endif
                    if (rc != VINF_SUCCESS)
                        return rc;
                }
                unsigned idx = (off >> PGM_DYNAMIC_CHUNK_SHIFT);
                *pHCPtr = (RTHCPTR)((RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[idx] + (off & PGM_DYNAMIC_CHUNK_OFFSET_MASK));
                return VINF_SUCCESS;
            }
            if (pRam->pvHC)
            {
#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
                if (!(pRam->aHCPhys[off >> PAGE_SHIFT] & MM_RAM_FLAGS_RESERVED))
#endif
                {
                    *pHCPtr = (RTHCPTR)((RTHCUINTPTR)pRam->pvHC + off);
                    return VINF_SUCCESS;
                }
            }
            return VERR_PGM_PHYS_PAGE_RESERVED;
        }
    }
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Validates a HC pointer.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM     The VM handle.
 * @param   HCPtr   The pointer to validate.
 */
PGMDECL(bool) PGMPhysIsHCPtrValid(PVM pVM, RTHCPTR HCPtr)
{
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            /** @note this is quite slow */
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                        return true;
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;

            if (off < pRam->cb)
                return true;
        }
    }
    return false;
}


/**
 * Converts a HC pointer to a GC physical address.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_INVALID_POINTER if the pointer is not within the
 *          GC physical memory.
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMDECL(int) PGMPhysHCPtr2GCPhys(PVM pVM, RTHCPTR HCPtr, PRTGCPHYS pGCPhys)
{
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            /** @note this is quite slow */
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                    {
                        *pGCPhys = pRam->GCPhys + iChunk*PGM_DYNAMIC_CHUNK_SIZE + off;
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;
            if (off < pRam->cb)
            {
                *pGCPhys = pRam->GCPhys + off;
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_POINTER;
}


/**
 * Converts a HC pointer to a GC physical address.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_INVALID_POINTER if the pointer is not within the
 *          GC physical memory.
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
PGMDECL(int) PGMPhysHCPtr2HCPhys(PVM pVM, RTHCPTR HCPtr, PRTHCPHYS pHCPhys)
{
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            /** @note this is quite slow */
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                    {
                        RTHCPHYS HCPhys = pRam->aHCPhys[off >> PAGE_SHIFT];
#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
                        if (!(HCPhys & MM_RAM_FLAGS_RESERVED))
#endif
                        {
                            *pHCPhys = (HCPhys & X86_PTE_PAE_PG_MASK)
                                     | (off & PAGE_OFFSET_MASK);
                            return VINF_SUCCESS;
                        }
                        return VERR_PGM_PHYS_PAGE_RESERVED;
                    }
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;
            if (off < pRam->cb)
            {
                RTHCPHYS HCPhys = pRam->aHCPhys[off >> PAGE_SHIFT];
#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
                if (!(HCPhys & MM_RAM_FLAGS_RESERVED))
#endif
                {
                    *pHCPhys = (HCPhys & X86_PTE_PAE_PG_MASK)
                             | (off & PAGE_OFFSET_MASK);
                    return VINF_SUCCESS;
                }
                return VERR_PGM_PHYS_PAGE_RESERVED;
            }
        }
    }
    return VERR_INVALID_POINTER;
}


/**
 * Validates a HC Physical address.
 *
 * This is an extremely slow API, don't use it!
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM     The VM handle.
 * @param   HCPhys  The physical address to validate.
 */
PGMDECL(bool) PGMPhysIsHCPhysValid(PVM pVM, RTHCPHYS HCPhys)
{
    RTGCPHYS GCPhys;
    int rc = PGMPhysHCPhys2GCPhys(pVM, HCPhys, &GCPhys);
    return VBOX_SUCCESS(rc);
}


/**
 * Converts a HC physical address to a GC physical address.
 *
 * This is an extremely slow API, don't use it!
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_INVALID_POINTER if the HC physical address is
 *          not within the GC physical memory.
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMDECL(int) PGMPhysHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys)
{
    unsigned off = HCPhys & PAGE_OFFSET_MASK;
    HCPhys &= X86_PTE_PAE_PG_MASK;
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (    pRam->pvHC
            ||  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
        {
            unsigned iPage = pRam->cb >> PAGE_SHIFT;
            while (iPage-- > 0)
#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
                if ((pRam->aHCPhys[iPage] & (X86_PTE_PAE_PG_MASK | MM_RAM_FLAGS_RESERVED)) == HCPhys)
#else
                if ((pRam->aHCPhys[iPage] & (X86_PTE_PAE_PG_MASK)) == HCPhys)
#endif
                {
                    *pGCPhys = pRam->GCPhys + (iPage << PAGE_SHIFT) + off;
                    return VINF_SUCCESS;
                }
        }
    }
    return VERR_INVALID_POINTER;
}


/**
 * Converts a HC physical address to a HC pointer.
 *
 * This is an extremely slow API, don't use it!
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_INVALID_POINTER if the HC physical address is
 *          not within the GC physical memory.
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address to convert.
 * @param   pHCPtr  Where to store the HC pointer on success.
 */
PGMDECL(int) PGMPhysHCPhys2HCPtr(PVM pVM, RTHCPHYS HCPhys, PRTHCPTR pHCPtr)
{
    unsigned off = HCPhys & PAGE_OFFSET_MASK;
    HCPhys &= X86_PTE_PAE_PG_MASK;
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXSUFF(pRam->pNext))
    {
        if (    pRam->pvHC
            ||  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
        {
            unsigned iPage = pRam->cb >> PAGE_SHIFT;
            while (iPage-- > 0)
#ifndef PGM_IGNORE_RAM_FLAGS_RESERVED
                if ((pRam->aHCPhys[iPage] & (X86_PTE_PAE_PG_MASK | MM_RAM_FLAGS_RESERVED)) == HCPhys)
#else
                if ((pRam->aHCPhys[iPage] & (X86_PTE_PAE_PG_MASK)) == HCPhys)
#endif
                {
                    if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
                    {
                        unsigned idx = (iPage >> (PGM_DYNAMIC_CHUNK_SHIFT - PAGE_SHIFT));

                        *pHCPtr = (RTHCPTR)((RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[idx] + ((iPage << PAGE_SHIFT) & PGM_DYNAMIC_CHUNK_OFFSET_MASK) + off);
                    }
                    else
                        *pHCPtr = (RTHCPTR)((RTHCUINTPTR)pRam->pvHC + (iPage << PAGE_SHIFT) + off);

                    return VINF_SUCCESS;
                }
        }
    }
    return VERR_INVALID_POINTER;
}


/**
 * Converts a guest pointer to a GC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pGCPhys     Where to store the HC physical address.
 */
PGMDECL(int) PGMPhysGCPtr2GCPhys(PVM pVM, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    int rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtr, NULL, pGCPhys);
    /** @todo real mode & protected mode? */
    return rc;
}


/**
 * Converts a guest pointer to a HC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pHCPhys     Where to store the HC physical address.
 */
PGMDECL(int) PGMPhysGCPtr2HCPhys(PVM pVM, RTGCPTR GCPtr, PRTHCPHYS pHCPhys)
{
    RTGCPHYS GCPhys;
    int rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtr, NULL, &GCPhys);
    if (VBOX_SUCCESS(rc))
        rc = PGMPhysGCPhys2HCPhys(pVM, GCPhys | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), pHCPhys);
    /** @todo real mode & protected mode? */
    return rc;
}


/**
 * Converts a guest pointer to a HC pointer.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pHCPtr      Where to store the HC virtual address.
 */
PGMDECL(int) PGMPhysGCPtr2HCPtr(PVM pVM, RTGCPTR GCPtr, PRTHCPTR pHCPtr)
{
    RTGCPHYS GCPhys;
    int rc = PGM_GST_PFN(GetPage,pVM)(pVM, (RTGCUINTPTR)GCPtr, NULL, &GCPhys);
    if (VBOX_SUCCESS(rc))
        rc = PGMPhysGCPhys2HCPtr(pVM, GCPhys | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), 1 /* we always stay within one page */, pHCPtr);
    /** @todo real mode & protected mode? */
    return rc;
}


/**
 * Converts a guest virtual address to a HC pointer by specfied CR3 and flags.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   cr3         The guest CR3.
 * @param   fFlags      Flags used for interpreting the PD correctly: X86_CR4_PSE and X86_CR4_PAE
 * @param   pHCPtr      Where to store the HC pointer.
 *
 * @remark  This function is used by the REM at a time where PGM could
 *          potentially not be in sync. It could also be used by a
 *          future DBGF API to cpu state independent conversions.
 */
PGMDECL(int) PGMPhysGCPtr2HCPtrByGstCR3(PVM pVM, RTGCPTR GCPtr, uint32_t cr3, unsigned fFlags, PRTHCPTR pHCPtr)
{
    /*
     * PAE or 32-bit?
     */
    int rc;
    if (!(fFlags & X86_CR4_PAE))
    {
        PX86PD pPD;
        rc = PGM_GCPHYS_2_PTR(pVM, cr3 & X86_CR3_PAGE_MASK, &pPD);
        if (VBOX_SUCCESS(rc))
        {
            VBOXPDE Pde = pPD->a[(RTGCUINTPTR)GCPtr >> X86_PD_SHIFT];
            if (Pde.n.u1Present)
            {
                if ((fFlags & X86_CR4_PSE) && Pde.b.u1Size)
                {   /* (big page) */
                    rc = PGMPhysGCPhys2HCPtr(pVM, (Pde.u & X86_PDE4M_PG_MASK) | ((RTGCUINTPTR)GCPtr & X86_PAGE_4M_OFFSET_MASK), 1 /* we always stay within one page */, pHCPtr);
                }
                else
                {   /* (normal page) */
                    PVBOXPT pPT;
                    rc = PGM_GCPHYS_2_PTR(pVM, Pde.u & X86_PDE_PG_MASK, &pPT);
                    if (VBOX_SUCCESS(rc))
                    {
                        VBOXPTE Pte = pPT->a[((RTGCUINTPTR)GCPtr >> X86_PT_SHIFT) & X86_PT_MASK];
                        if (Pte.n.u1Present)
                            return PGMPhysGCPhys2HCPtr(pVM, (Pte.u & X86_PTE_PG_MASK) | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), 1 /* we always stay within one page */, pHCPtr);
                        rc = VERR_PAGE_NOT_PRESENT;
                    }
                }
            }
            else
                rc = VERR_PAGE_TABLE_NOT_PRESENT;
        }
    }
    else
    {
        /** @todo long mode! */
        PX86PDPTR pPdptr;
        rc = PGM_GCPHYS_2_PTR(pVM, cr3 & X86_CR3_PAE_PAGE_MASK, &pPdptr);
        if (VBOX_SUCCESS(rc))
        {
            X86PDPE Pdpe = pPdptr->a[((RTGCUINTPTR)GCPtr >> X86_PDPTR_SHIFT) & X86_PDPTR_MASK];
            if (Pdpe.n.u1Present)
            {
                PX86PDPAE pPD;
                rc = PGM_GCPHYS_2_PTR(pVM, Pdpe.u & X86_PDPE_PG_MASK, &pPD);
                if (VBOX_SUCCESS(rc))
                {
                    X86PDEPAE Pde = pPD->a[((RTGCUINTPTR)GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK];
                    if (Pde.n.u1Present)
                    {
                        if ((fFlags & X86_CR4_PSE) && Pde.b.u1Size)
                        {   /* (big page) */
                            rc = PGMPhysGCPhys2HCPtr(pVM, (Pde.u & X86_PDE4M_PAE_PG_MASK) | ((RTGCUINTPTR)GCPtr & X86_PAGE_4M_OFFSET_MASK), 1 /* we always stay within one page */, pHCPtr);
                        }
                        else
                        {   /* (normal page) */
                            PX86PTPAE pPT;
                            rc = PGM_GCPHYS_2_PTR(pVM, (Pde.u & X86_PDE_PAE_PG_MASK), &pPT);
                            if (VBOX_SUCCESS(rc))
                            {
                                X86PTEPAE Pte = pPT->a[((RTGCUINTPTR)GCPtr >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK];
                                if (Pte.n.u1Present)
                                    return PGMPhysGCPhys2HCPtr(pVM, (Pte.u & X86_PTE_PAE_PG_MASK) | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), 1 /* we always stay within one page */, pHCPtr);
                                rc = VERR_PAGE_NOT_PRESENT;
                            }
                        }
                    }
                    else
                        rc = VERR_PAGE_TABLE_NOT_PRESENT;
                }
            }
            else
                rc = VERR_PAGE_TABLE_NOT_PRESENT;
        }
    }
    return rc;
}


#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_PGM_PHYS_ACCESS


#ifdef IN_RING3
/**
 * Cache PGMPhys memory access
 *
 * @param   pVM             VM Handle.
 * @param   pCache          Cache structure pointer
 * @param   GCPhys          GC physical address
 * @param   pbHC            HC pointer corresponding to physical page
 */
static void pgmPhysCacheAdd(PVM pVM, PGMPHYSCACHE *pCache, RTGCPHYS GCPhys, uint8_t *pbHC)
{
    uint32_t iCacheIndex;

    GCPhys = PAGE_ADDRESS(GCPhys);
    pbHC   = (uint8_t *)PAGE_ADDRESS(pbHC);

    iCacheIndex = ((GCPhys >> PAGE_SHIFT) & PGM_MAX_PHYSCACHE_ENTRIES_MASK);

    ASMBitSet(&pCache->aEntries, iCacheIndex);

    pCache->Entry[iCacheIndex].GCPhys = GCPhys;
    pCache->Entry[iCacheIndex].pbHC   = pbHC;
}
#endif

/**
 * Read physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 * @param   pvBuf           Where to put the read bits.
 * @param   cbRead          How many bytes to read.
 */
PGMDECL(void) PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
#ifdef IN_RING3
    bool fGrabbedLock = false;
#endif

    AssertMsg(cbRead > 0, ("don't even think about reading zero bytes!\n"));
    if (cbRead == 0)
        return;

    LogFlow(("PGMPhysRead: %VGp %d\n", GCPhys, cbRead));

#ifdef IN_RING3
    if (!VM_IS_EMT(pVM))
    {
        pgmLock(pVM);
        fGrabbedLock = true;
    }
#endif

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE    pCur = CTXSUFF(pVM->pgm.s.pRamRanges);
    for (;;)
    {
        /* Find range. */
        while (pCur && GCPhys > pCur->GCPhysLast)
            pCur = CTXSUFF(pCur->pNext);
        /* Inside range or not? */
        if (pCur && GCPhys >= pCur->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPHYS off = GCPhys - pCur->GCPhys;
            while (off < pCur->cb)
            {
                unsigned iPage = off >> PAGE_SHIFT;

                /* Physical chunk in dynamically allocated range not present? */
                if (RT_UNLIKELY(!(pCur->aHCPhys[iPage] & X86_PTE_PAE_PG_MASK)))
                {
                    int rc;
#ifdef IN_RING3
                    if (fGrabbedLock)
                    {
                        pgmUnlock(pVM);
                        rc = pgmr3PhysGrowRange(pVM, GCPhys);
                        if (rc == VINF_SUCCESS)
                            PGMPhysRead(pVM, GCPhys, pvBuf, cbRead); /* try again; can't assume pCur is still valid (paranoia) */
                        return;
                    }
                    rc = pgmr3PhysGrowRange(pVM, GCPhys);
#else
                    rc = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhys);
#endif
                    if (rc != VINF_SUCCESS)
                        goto end;
                }

                size_t   cb;
                RTHCPHYS HCPhys = pCur->aHCPhys[iPage];
                switch (HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_VIRTUAL_ALL | MM_RAM_FLAGS_PHYSICAL_ALL | MM_RAM_FLAGS_ROM))
                {
                    /*
                     * Normal memory or ROM.
                     */
                    case 0:
                    case MM_RAM_FLAGS_ROM:
                    case MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_RESERVED:
                    case MM_RAM_FLAGS_PHYSICAL_WRITE:
                    case MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_PHYSICAL_WRITE:
                    case MM_RAM_FLAGS_VIRTUAL_WRITE:
                    {
#ifdef IN_GC
                        void *pvSrc = NULL;
                        PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvSrc);
                        pvSrc = (char *)pvSrc + (off & PAGE_OFFSET_MASK);
#else
                        void *pvSrc = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                        if (cb >= cbRead)
                        {
#if defined(IN_RING3) && defined(PGM_PHYSMEMACCESS_CACHING)
                            if (cbRead <= 4)
                                pgmPhysCacheAdd(pVM, &pVM->pgm.s.pgmphysreadcache, GCPhys, (uint8_t*)pvSrc);
#endif /* IN_RING3 && PGM_PHYSMEMACCESS_CACHING */
                            memcpy(pvBuf, pvSrc, cbRead);
                            goto end;
                        }
                        memcpy(pvBuf, pvSrc, cb);
                        break;
                    }

                    /*
                     * All reserved, nothing there.
                     */
                    case MM_RAM_FLAGS_RESERVED:
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                        if (cb >= cbRead)
                        {
                            memset(pvBuf, 0, cbRead);
                            goto end;
                        }
                        memset(pvBuf, 0, cb);
                        break;

                    /*
                     * Physical handler.
                     */
                    case MM_RAM_FLAGS_PHYSICAL_ALL:
                    case MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_PHYSICAL_ALL: /** r=bird: MMIO2 isn't in the mask! */
                    {
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */

                        /* find and call the handler */
                        PPGMPHYSHANDLER pNode = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.pTreesHC->PhysHandlers, GCPhys);
                        if (pNode && pNode->pfnHandlerR3)
                        {
                            size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbRead)
                                cb = cbRead;

                            void *pvSrc = PGMRAMRANGE_GETHCPTR(pCur, off)

                            /** @note Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                            rc = pNode->pfnHandlerR3(pVM, GCPhys, pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, pNode->pvUserR3);
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#ifdef IN_GC
                            void *pvSrc = NULL;
                            PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvSrc);
                            pvSrc = (char *)pvSrc + (off & PAGE_OFFSET_MASK);
#else
                            void *pvSrc = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif

                            if (cb >= cbRead)
                            {
                                memcpy(pvBuf, pvSrc, cbRead);
                                goto end;
                            }
                            memcpy(pvBuf, pvSrc, cb);
                        }
                        else if (cb >= cbRead)
                            goto end;
                        break;
                    }

                    case MM_RAM_FLAGS_VIRTUAL_ALL:
                    {
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                        /* Search the whole tree for matching physical addresses (rather expensive!) */
                        PPGMVIRTHANDLER pNode;
                        unsigned iPage;
                        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pNode, &iPage);
                        if (VBOX_SUCCESS(rc2) && pNode->pfnHandlerHC)
                        {
                            size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbRead)
                                cb = cbRead;
                            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pNode->GCPtr & PAGE_BASE_GC_MASK)
                                              + (iPage << PAGE_SHIFT) + (off & PAGE_OFFSET_MASK);

                            void *pvSrc = PGMRAMRANGE_GETHCPTR(pCur, off)

                            /** @note Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                            rc = pNode->pfnHandlerHC(pVM, (RTGCPTR)GCPtr, pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, 0);
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#ifdef IN_GC
                            void *pvSrc = NULL;
                            PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvSrc);
                            pvSrc = (char *)pvSrc + (off & PAGE_OFFSET_MASK);
#else
                            void *pvSrc = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif
                            if (cb >= cbRead)
                            {
                                memcpy(pvBuf, pvSrc, cbRead);
                                goto end;
                            }
                            memcpy(pvBuf, pvSrc, cb);
                        }
                        else if (cb >= cbRead)
                            goto end;
                        break;
                    }

                    /*
                     * The rest needs to be taken more carefully.
                     */
                    default:
#if 1                   /** @todo r=bird: Can you do this properly please. */
                        /** @todo Try MMIO; quick hack */
                        if (cbRead <= 4 && IOMMMIORead(pVM, GCPhys, (uint32_t *)pvBuf, cbRead) == VINF_SUCCESS)
                            goto end;
#endif

                        /** @todo fix me later. */
                        AssertReleaseMsgFailed(("Unknown read at %VGp size %d implement the complex physical reading case %x\n",
                                                GCPhys, cbRead,
                                                HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_VIRTUAL_ALL | MM_RAM_FLAGS_PHYSICAL_ALL | MM_RAM_FLAGS_ROM)));
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                        break;
                }

                cbRead -= cb;
                off    += cb;
                pvBuf   = (char *)pvBuf + cb;
            }

            GCPhys = pCur->GCPhysLast + 1;
        }
        else
        {
            LogFlow(("PGMPhysRead: Unassigned %VGp size=%d\n", GCPhys, cbRead));

            /*
             * Unassigned address space.
             */
            size_t cb;
            if (    !pCur
                ||  (cb = pCur->GCPhys - GCPhys) >= cbRead)
            {
                memset(pvBuf, 0, cbRead);
                goto end;
            }

            memset(pvBuf, 0, cb);
            cbRead -= cb;
            pvBuf   = (char *)pvBuf + cb;
            GCPhys += cb;
        }
    }
end:
#ifdef IN_RING3
    if (fGrabbedLock)
        pgmUnlock(pVM);
#endif
    return;
}

/**
 * Write to physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   pvBuf           What to write.
 * @param   cbWrite         How many bytes to write.
 */
PGMDECL(void) PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
#ifdef IN_RING3
    bool fGrabbedLock = false;
#endif

    AssertMsg(cbWrite > 0, ("don't even think about writing zero bytes!\n"));
    if (cbWrite == 0)
        return;

    LogFlow(("PGMPhysWrite: %VGp %d\n", GCPhys, cbWrite));

#ifdef IN_RING3
    if (!VM_IS_EMT(pVM))
    {
        pgmLock(pVM);
        fGrabbedLock = true;
    }
#endif
    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE    pCur = CTXSUFF(pVM->pgm.s.pRamRanges);
    for (;;)
    {
        /* Find range. */
        while (pCur && GCPhys > pCur->GCPhysLast)
            pCur = CTXSUFF(pCur->pNext);
        /* Inside range or not? */
        if (pCur && GCPhys >= pCur->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            unsigned off = GCPhys - pCur->GCPhys;
            while (off < pCur->cb)
            {
                unsigned iPage = off >> PAGE_SHIFT;

                /* Physical chunk in dynamically allocated range not present? */
                if (RT_UNLIKELY(!(pCur->aHCPhys[iPage] & X86_PTE_PAE_PG_MASK)))
                {
                    int rc;
#ifdef IN_RING3
                    if (fGrabbedLock)
                    {
                        pgmUnlock(pVM);
                        rc = pgmr3PhysGrowRange(pVM, GCPhys);
                        if (rc == VINF_SUCCESS)
                            PGMPhysWrite(pVM, GCPhys, pvBuf, cbWrite); /* try again; can't assume pCur is still valid (paranoia) */
                        return;
                    }
                    rc = pgmr3PhysGrowRange(pVM, GCPhys);
#else
                    rc = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhys);
#endif
                    if (rc != VINF_SUCCESS)
                        goto end;
                }

                size_t   cb;
                RTHCPHYS HCPhys = pCur->aHCPhys[iPage];
                /** @todo r=bird: missing MM_RAM_FLAGS_ROM here, we shall not allow anyone to overwrite the ROM! */
                switch (HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_VIRTUAL_ALL | MM_RAM_FLAGS_VIRTUAL_WRITE | MM_RAM_FLAGS_PHYSICAL_ALL | MM_RAM_FLAGS_PHYSICAL_WRITE))
                {
                    /*
                     * Normal memory.
                     */
                    case 0:
                    case MM_RAM_FLAGS_MMIO2:
                    {
#ifdef IN_GC
                        void *pvDst = NULL;
                        PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvDst);
                        pvDst = (char *)pvDst + (off & PAGE_OFFSET_MASK);
#else
                        void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                        if (cb >= cbWrite)
                        {
#if defined(IN_RING3) && defined(PGM_PHYSMEMACCESS_CACHING)
                            if (cbWrite <= 4)
                                pgmPhysCacheAdd(pVM, &pVM->pgm.s.pgmphyswritecache, GCPhys, (uint8_t*)pvDst);
#endif /* IN_RING3 && PGM_PHYSMEMACCESS_CACHING */
                            memcpy(pvDst, pvBuf, cbWrite);
                            goto end;
                        }
                        memcpy(pvDst, pvBuf, cb);
                        break;
                    }

                    /*
                     * All reserved, nothing there.
                     */
                    case MM_RAM_FLAGS_RESERVED:
                    case MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO2:
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                        if (cb >= cbWrite)
                            goto end;
                        break;

                    /*
                     * Physical handler.
                     */
                    case MM_RAM_FLAGS_PHYSICAL_ALL:
                    case MM_RAM_FLAGS_PHYSICAL_WRITE:
                    case MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_PHYSICAL_ALL:
                    case MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_PHYSICAL_WRITE:
                    {
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                        /* find and call the handler */
                        PPGMPHYSHANDLER pNode = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.pTreesHC->PhysHandlers, GCPhys);
                        if (pNode && pNode->pfnHandlerR3)
                        {
                            size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)

                            /** @note Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                            rc = pNode->pfnHandlerR3(pVM, GCPhys, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, pNode->pvUserR3);
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#ifdef IN_GC
                            void *pvDst = NULL;
                            PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvDst);
                            pvDst = (char *)pvDst + (off & PAGE_OFFSET_MASK);
#else
                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif
                            if (cb >= cbWrite)
                            {
                                memcpy(pvDst, pvBuf, cbWrite);
                                goto end;
                            }
                            memcpy(pvDst, pvBuf, cb);
                        }
                        else if (cb >= cbWrite)
                            goto end;
                        break;
                    }

                    case MM_RAM_FLAGS_VIRTUAL_ALL:
                    case MM_RAM_FLAGS_VIRTUAL_WRITE:
                    {
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3
/** @todo deal with this in GC and R0! */
                        /* Search the whole tree for matching physical addresses (rather expensive!) */
                        PPGMVIRTHANDLER pNode;
                        unsigned iPage;
                        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pNode, &iPage);
                        if (VBOX_SUCCESS(rc2) && pNode->pfnHandlerHC)
                        {
                            size_t cbRange = pNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;
                            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pNode->GCPtr & PAGE_BASE_GC_MASK)
                                              + (iPage << PAGE_SHIFT) + (off & PAGE_OFFSET_MASK);

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)

                            /** @note Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                            rc = pNode->pfnHandlerHC(pVM, (RTGCPTR)GCPtr, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, 0);
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#ifdef IN_GC
                            void *pvDst = NULL;
                            PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvDst);
                            pvDst = (char *)pvDst + (off & PAGE_OFFSET_MASK);
#else
                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif
                            if (cb >= cbWrite)
                            {
                                memcpy(pvDst, pvBuf, cbWrite);
                                goto end;
                            }
                            memcpy(pvDst, pvBuf, cb);
                        }
                        else if (cb >= cbWrite)
                            goto end;
                        break;
                    }

                    /*
                     * Physical write handler + virtual write handler.
                     * Consider this a quick workaround for the CSAM + shadow caching problem.
                     *
                     * We hand it to the shadow caching first since it requires the unchanged
                     * data. CSAM will have to put up with it already being changed.
                     */
                    case MM_RAM_FLAGS_PHYSICAL_WRITE | MM_RAM_FLAGS_VIRTUAL_WRITE:
                    {
                        int rc = VINF_PGM_HANDLER_DO_DEFAULT;
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
#ifdef IN_RING3 /** @todo deal with this in GC and R0! */
                        /* 1. The physical handler */
                        PPGMPHYSHANDLER pPhysNode = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.pTreesHC->PhysHandlers, GCPhys);
                        if (pPhysNode && pPhysNode->pfnHandlerR3)
                        {
                            size_t cbRange = pPhysNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)

                            /** @note Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                            rc = pPhysNode->pfnHandlerR3(pVM, GCPhys, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, pPhysNode->pvUserR3);
                        }

                        /* 2. The virtual handler (will see incorrect data) */
                        PPGMVIRTHANDLER pVirtNode;
                        unsigned iPage;
                        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pVirtNode, &iPage);
                        if (VBOX_SUCCESS(rc2) && pVirtNode->pfnHandlerHC)
                        {
                            size_t cbRange = pVirtNode->Core.KeyLast - GCPhys + 1;
                            if (cbRange < cb)
                                cb = cbRange;
                            if (cb > cbWrite)
                                cb = cbWrite;
                            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirtNode->GCPtr & PAGE_BASE_GC_MASK)
                                              + (iPage << PAGE_SHIFT) + (off & PAGE_OFFSET_MASK);

                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)

                            /** @note Dangerous assumption that HC handlers don't do anything that really requires an EMT lock! */
                            rc2 = pVirtNode->pfnHandlerHC(pVM, (RTGCPTR)GCPtr, pvDst, (void *)pvBuf, cb, PGMACCESSTYPE_WRITE, 0);
                            if (    (   rc2 != VINF_PGM_HANDLER_DO_DEFAULT
                                     && rc == VINF_PGM_HANDLER_DO_DEFAULT)
                                ||  (   VBOX_FAILURE(rc2)
                                     && VBOX_SUCCESS(rc)))
                                rc = rc2;
                        }
#endif /* IN_RING3 */
                        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                        {
#ifdef IN_GC
                            void *pvDst = NULL;
                            PGMGCDynMapHCPage(pVM, HCPhys & X86_PTE_PAE_PG_MASK, &pvDst);
                            pvDst = (char *)pvDst + (off & PAGE_OFFSET_MASK);
#else
                            void *pvDst = PGMRAMRANGE_GETHCPTR(pCur, off)
#endif
                            if (cb >= cbWrite)
                            {
                                memcpy(pvDst, pvBuf, cbWrite);
                                goto end;
                            }
                            memcpy(pvDst, pvBuf, cb);
                        }
                        else if (cb >= cbWrite)
                            goto end;
                        break;
                    }


                    /*
                     * The rest needs to be taken more carefully.
                     */
                    default:
#if 1                   /** @todo r=bird: Can you do this properly please. */
                        /** @todo Try MMIO; quick hack */
                        if (cbWrite <= 4 && IOMMMIOWrite(pVM, GCPhys, *(uint32_t *)pvBuf, cbWrite) == VINF_SUCCESS)
                            goto end;
#endif

                        /** @todo fix me later. */
                        AssertReleaseMsgFailed(("Unknown write at %VGp size %d implement the complex physical writing case %x\n",
                                                GCPhys, cbWrite,
                                                (HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_VIRTUAL_ALL | MM_RAM_FLAGS_VIRTUAL_WRITE | MM_RAM_FLAGS_PHYSICAL_ALL | MM_RAM_FLAGS_PHYSICAL_WRITE))));
                        cb = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                        break;
                }

                cbWrite -= cb;
                off    += cb;
                pvBuf   = (const char *)pvBuf + cb;
            }

            GCPhys = pCur->GCPhysLast + 1;
        }
        else
        {
            /*
             * Unassigned address space.
             */
            size_t cb;
            if (    !pCur
                ||  (cb = pCur->GCPhys - GCPhys) >= cbWrite)
                goto end;

            cbWrite -= cb;
            pvBuf   = (const char *)pvBuf + cb;
            GCPhys += cb;
        }
    }
end:
#ifdef IN_RING3
    if (fGrabbedLock)
        pgmUnlock(pVM);
#endif
    return;
}

#ifndef IN_GC /* Ring 0 & 3 only */

/**
 * Read from guest physical memory by GC physical address, bypassing
 * MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPhysSrc   The source address (GC physical address).
 * @param   cb          The number of bytes to read.
 */
PGMDECL(int) PGMPhysReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb)
{
    /*
     * Anything to be done?
     */
    if (!cb)
        return VINF_SUCCESS;

    /*
     * Loop ram ranges.
     */
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = pRam->CTXSUFF(pNext))
    {
        RTGCPHYS off = GCPhysSrc - pRam->GCPhys;
        if (off < pRam->cb)
        {
            if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
            {
                /* Copy page by page as we're not dealing with a linear HC range. */
                for (;;)
                {
                    /* convert */
                    void *pvSrc;
                    int rc = PGMRamGCPhys2HCPtr(pVM, pRam, GCPhysSrc, &pvSrc);
                    if (VBOX_FAILURE(rc))
                        return rc;

                    /* copy */
                    size_t cbRead = PAGE_SIZE - ((RTGCUINTPTR)GCPhysSrc & PAGE_OFFSET_MASK);
                    if (cbRead >= cb)
                    {
                        memcpy(pvDst, pvSrc, cb);
                        return VINF_SUCCESS;
                    }
                    memcpy(pvDst, pvSrc, cbRead);

                    /* next */
                    cb         -= cbRead;
                    pvDst       = (uint8_t *)pvDst + cbRead;
                    GCPhysSrc  += cbRead;
                }
            }
            else if (pRam->pvHC)
            {
                /* read */
                size_t cbRead = pRam->cb - off;
                if (cbRead >= cb)
                {
                    memcpy(pvDst, (uint8_t *)pRam->pvHC + off, cb);
                    return VINF_SUCCESS;
                }
                memcpy(pvDst, (uint8_t *)pRam->pvHC + off, cbRead);

                /* next */
                cb        -= cbRead;
                pvDst      = (uint8_t *)pvDst + cbRead;
                GCPhysSrc += cbRead;
            }
            else
                return VERR_PGM_PHYS_PAGE_RESERVED;
        }
        else if (GCPhysSrc < pRam->GCPhysLast)
            break;
    }
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Write to guest physical memory referenced by GC pointer.
 * Write memory to GC physical address in guest physical memory.
 *
 * This will bypass MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhysDst   The GC physical address of the destination.
 * @param   pvSrc       The source buffer.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb)
{
    /*
     * Anything to be done?
     */
    if (!cb)
        return VINF_SUCCESS;

    LogFlow(("PGMPhysWriteGCPhys: %VGp %d\n", GCPhysDst, cb));

    /*
     * Loop ram ranges.
     */
    for (PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = pRam->CTXSUFF(pNext))
    {
        RTGCPHYS off = GCPhysDst - pRam->GCPhys;
        if (off < pRam->cb)
        {
            if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
            {
                /* Copy page by page as we're not dealing with a linear HC range. */
                for (;;)
                {
                    /* convert */
                    void *pvDst;
                    int rc = PGMRamGCPhys2HCPtr(pVM, pRam, GCPhysDst, &pvDst);
                    if (VBOX_FAILURE(rc))
                        return rc;

                    /* copy */
                    size_t cbWrite = PAGE_SIZE - ((RTGCUINTPTR)GCPhysDst & PAGE_OFFSET_MASK);
                    if (cbWrite >= cb)
                    {
                        memcpy(pvDst, pvSrc, cb);
                        return VINF_SUCCESS;
                    }
                    memcpy(pvDst, pvSrc, cbWrite);

                    /* next */
                    cb         -= cbWrite;
                    pvSrc       = (uint8_t *)pvSrc + cbWrite;
                    GCPhysDst  += cbWrite;
                }
            }
            else if (pRam->pvHC)
            {
                /* write */
                size_t cbWrite = pRam->cb - off;
                if (cbWrite >= cb)
                {
                    memcpy((uint8_t *)pRam->pvHC + off, pvSrc, cb);
                    return VINF_SUCCESS;
                }
                memcpy((uint8_t *)pRam->pvHC + off, pvSrc, cbWrite);

                /* next */
                cb         -= cbWrite;
                GCPhysDst  += cbWrite;
                pvSrc       = (uint8_t *)pvSrc + cbWrite;
            }
            else
                return VERR_PGM_PHYS_PAGE_RESERVED;
        }
        else if (GCPhysDst < pRam->GCPhysLast)
            break;
    }
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set any accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 */
PGMDECL(int) PGMPhysReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb)
{
    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    /*
     * Optimize reads within a single page.
     */
    if (((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        void *pvSrc;
        int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrSrc, &pvSrc);
        if (VBOX_FAILURE(rc))
            return rc;
        memcpy(pvDst, pvSrc, cb);
        return VINF_SUCCESS;
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* convert */
        void *pvSrc;
        int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrSrc, &pvSrc);
        if (VBOX_FAILURE(rc))
            return rc;

        /* copy */
        size_t cbRead = PAGE_SIZE - ((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK);
        if (cbRead >= cb)
        {
            memcpy(pvDst, pvSrc, cb);
            return VINF_SUCCESS;
        }
        memcpy(pvDst, pvSrc, cbRead);

        /* next */
        cb         -= cbRead;
        pvDst       = (uint8_t *)pvDst + cbRead;
        GCPtrSrc   += cbRead;
    }
}


/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set dirty or accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    LogFlow(("PGMPhysWriteGCPtr: %VGv %d\n", GCPtrDst, cb));

    /*
     * Optimize writes within a single page.
     */
    if (((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        void *pvDst;
        int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrDst, &pvDst);
        if (VBOX_FAILURE(rc))
            return rc;
        memcpy(pvDst, pvSrc, cb);
        return VINF_SUCCESS;
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* convert */
        void *pvDst;
        int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrDst, &pvDst);
        if (VBOX_FAILURE(rc))
            return rc;

        /* copy */
        size_t cbWrite = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
        if (cbWrite >= cb)
        {
            memcpy(pvDst, pvSrc, cb);
            return VINF_SUCCESS;
        }
        memcpy(pvDst, pvSrc, cbWrite);

        /* next */
        cb         -= cbWrite;
        pvSrc       = (uint8_t *)pvSrc + cbWrite;
        GCPtrDst   += cbWrite;
    }
}


/**
 * Write to guest physical memory referenced by GC pointer and update the PTE.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and set any dirty and accessed bits in the PTE.
 *
 * If you don't want to set the dirty bit, use PGMPhysWriteGCPtr().
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPtrDirty(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    /*
     * Optimize writes within a single page.
     */
    if (((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        void *pvDst;
        int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrDst, &pvDst);
        if (VBOX_FAILURE(rc))
            return rc;
        memcpy(pvDst, pvSrc, cb);
        rc = PGMGstModifyPage(pVM, GCPtrDst, cb, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
        AssertRC(rc);
        return VINF_SUCCESS;
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* convert */
        void *pvDst;
        int rc = PGMPhysGCPtr2HCPtr(pVM, GCPtrDst, &pvDst);
        if (VBOX_FAILURE(rc))
            return rc;

        /* mark the guest page as accessed and dirty. */
        rc = PGMGstModifyPage(pVM, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
        AssertRC(rc);

        /* copy */
        size_t  cbWrite = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
        if (cbWrite >= cb)
        {
            memcpy(pvDst, pvSrc, cb);
            return VINF_SUCCESS;
        }
        memcpy(pvDst, pvSrc, cbWrite);

        /* next */
        cb         -= cbWrite;
        GCPtrDst   += cbWrite;
        pvSrc       = (char *)pvSrc + cbWrite;
    }
}

#endif /* !IN_GC */



/**
 * Performs a read of guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the access bits.
 *
 * The current implementation will bypass all access handlers. It may later be
 * changed to at least respect MMIO.
 *
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   pvDst       Where to put the bytes we've read.
 * @param   GCPtrSrc    The source address.
 * @param   cb          The number of bytes to read. Not more than a page.
 *
 * @remark  This function will dynamically map physical pages in GC. This may unmap
 *          mappings done by the caller. Be careful!
 */
PGMDECL(int) PGMPhysInterpretedRead(PVM pVM, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb)
{
    Assert(cb <= PAGE_SIZE);

/** @todo r=bird: This isn't perfect!
 *  -# It's not checking for reserved bits being 1.
 *  -# It's not correctly dealing with the access bit.
 *  -# It's not respecting MMIO memory or any other access handlers.
 */
    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the read operation.
     * 4. Set access bits if required.
     */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS GCPhys;
        uint64_t fFlags;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc, &fFlags, &GCPhys);
        if (VBOX_SUCCESS(rc))
        {
            /** @todo we should check reserved bits ... */
            void *pvSrc;
            rc = PGM_GCPHYS_2_PTR(pVM, GCPhys, &pvSrc);
            switch (rc)
            {
                case VINF_SUCCESS:
Log(("PGMPhysInterpretedRead: pvDst=%p pvSrc=%p cb=%d\n", pvDst, (uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb));
                    memcpy(pvDst, (uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb);
                    break;
                case VERR_PGM_PHYS_PAGE_RESERVED:
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset(pvDst, 0, cb);
                    break;
                default:
                    return rc;
            }

            /** @todo access bit emulation isn't 100% correct. */
            if (!(fFlags & X86_PTE_A))
            {
                rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            return VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        unsigned cb2 = cb - cb1;
        uint64_t fFlags1;
        RTGCPHYS GCPhys1;
        uint64_t fFlags2;
        RTGCPHYS GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc, &fFlags1, &GCPhys1);
        if (VBOX_SUCCESS(rc))
            rc = PGM_GST_PFN(GetPage,pVM)(pVM, GCPtrSrc + cb1, &fFlags2, &GCPhys2);
        if (VBOX_SUCCESS(rc))
        {
            /** @todo we should check reserved bits ... */
AssertMsgFailed(("cb=%d cb1=%d cb2=%d GCPtrSrc=%VGv\n", cb, cb1, cb2, GCPtrSrc));
            void *pvSrc1;
            rc = PGM_GCPHYS_2_PTR(pVM, GCPhys1, &pvSrc1);
            switch (rc)
            {
                case VINF_SUCCESS:
                    memcpy(pvDst, (uint8_t *)pvSrc1 + (GCPtrSrc & PAGE_OFFSET_MASK), cb1);
                    break;
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset(pvDst, 0, cb1);
                    break;
                default:
                    return rc;
            }

            void *pvSrc2;
            rc = PGM_GCPHYS_2_PTR(pVM, GCPhys2, &pvSrc2);
            switch (rc)
            {
                case VINF_SUCCESS:
                    memcpy((uint8_t *)pvDst + cb2, pvSrc2, cb2);
                    break;
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset((uint8_t *)pvDst + cb2, 0, cb2);
                    break;
                default:
                    return rc;
            }

            if (!(fFlags1 & X86_PTE_A))
            {
                rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            if (!(fFlags2 & X86_PTE_A))
            {
                rc = PGM_GST_PFN(ModifyPage,pVM)(pVM, GCPtrSrc + cb1, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            return VINF_SUCCESS;
        }
    }

    /*
     * Raise a #PF.
     */
    uint32_t uErr;

    /* Get the current privilege level. */
    uint32_t cpl = CPUMGetGuestCPL(pVM, pCtxCore);
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Vrc GCPtrSrc=%VGv cb=%#x\n", rc, GCPtrSrc, cb));
            return rc;
    }
    Log(("PGMPhysInterpretedRead: GCPtrSrc=%VGv cb=%#x -> #PF(%#x)\n", GCPtrSrc, cb, uErr));
    return TRPMRaiseXcptErrCR2(pVM, pCtxCore, X86_XCPT_PF, uErr, GCPtrSrc);
}

/// @todo PGMDECL(int) PGMPhysInterpretedWrite(PVM pVM, PCPUMCTXCORE pCtxCore, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)

