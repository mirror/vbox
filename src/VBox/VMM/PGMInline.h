/* $Id$ */
/** @file
 * PGM - Inlined functions.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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

#ifndef ___PGMInline_h
#define ___PGMInline_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/stam.h>
#include <VBox/param.h>
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmapi.h>
#include <VBox/dis.h>
#include <VBox/dbgf.h>
#include <VBox/log.h>
#include <VBox/gmm.h>
#include <VBox/hwaccm.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/critsect.h>
#include <iprt/sha.h>



/** @addtogroup grp_pgm_int   Internals
 * @internal
 * @{
 */

/** @todo Split out all the inline stuff into a separate file.  Then we can
 *        include it later when VM and VMCPU are defined and so avoid all that
 *        &pVM->pgm.s and &pVCpu->pgm.s stuff.  It also chops ~1600 lines off
 *        this file and will make it somewhat easier to navigate... */

/**
 * Gets the PGMRAMRANGE structure for a guest page.
 *
 * @returns Pointer to the RAM range on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 */
DECLINLINE(PPGMRAMRANGE) pgmPhysGetRange(PPGM pPGM, RTGCPHYS GCPhys)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
                break;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    return pRam;
}


/**
 * Gets the PGMPAGE structure for a guest page.
 *
 * @returns Pointer to the page on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 */
DECLINLINE(PPGMPAGE) pgmPhysGetPage(PPGM pPGM, RTGCPHYS GCPhys)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
                return NULL;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    return &pRam->aPages[off >> PAGE_SHIFT];
}


/**
 * Gets the PGMPAGE structure for a guest page.
 *
 * Old Phys code: Will make sure the page is present.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS and a valid *ppPage on success.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if the address isn't valid.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppPage      Where to store the page pointer on success.
 */
DECLINLINE(int) pgmPhysGetPageEx(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGE ppPage)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
            {
                *ppPage = NULL; /* avoid incorrect and very annoying GCC warnings */
                return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
            }
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
    return VINF_SUCCESS;
}




/**
 * Gets the PGMPAGE structure for a guest page.
 *
 * Old Phys code: Will make sure the page is present.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS and a valid *ppPage on success.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if the address isn't valid.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppPage      Where to store the page pointer on success.
 * @param   ppRamHint   Where to read and store the ram list hint.
 *                      The caller initializes this to NULL before the call.
 */
DECLINLINE(int) pgmPhysGetPageWithHintEx(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRamHint)
{
    RTGCPHYS off;
    PPGMRAMRANGE pRam = *ppRamHint;
    if (    !pRam
        ||  RT_UNLIKELY((off = GCPhys - pRam->GCPhys) >= pRam->cb))
    {
        pRam = pPGM->CTX_SUFF(pRamRanges);
        off = GCPhys - pRam->GCPhys;
        if (RT_UNLIKELY(off >= pRam->cb))
        {
            do
            {
                pRam = pRam->CTX_SUFF(pNext);
                if (RT_UNLIKELY(!pRam))
                {
                    *ppPage = NULL; /* Kill the incorrect and extremely annoying GCC warnings. */
                    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
                }
                off = GCPhys - pRam->GCPhys;
            } while (off >= pRam->cb);
        }
        *ppRamHint = pRam;
    }
    *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
    return VINF_SUCCESS;
}


/**
 * Gets the PGMPAGE structure for a guest page together with the PGMRAMRANGE.
 *
 * @returns Pointer to the page on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppRam       Where to store the pointer to the PGMRAMRANGE.
 */
DECLINLINE(PPGMPAGE) pgmPhysGetPageAndRange(PPGM pPGM, RTGCPHYS GCPhys, PPGMRAMRANGE *ppRam)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
                return NULL;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    *ppRam = pRam;
    return &pRam->aPages[off >> PAGE_SHIFT];
}


/**
 * Gets the PGMPAGE structure for a guest page together with the PGMRAMRANGE.
 *
 * @returns Pointer to the page on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppPage      Where to store the pointer to the PGMPAGE structure.
 * @param   ppRam       Where to store the pointer to the PGMRAMRANGE structure.
 */
DECLINLINE(int) pgmPhysGetPageAndRangeEx(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
            {
                *ppRam = NULL;  /* Shut up silly GCC warnings. */
                *ppPage = NULL; /* ditto */
                return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
            }
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    *ppRam = pRam;
    *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
    return VINF_SUCCESS;
}


/**
 * Convert GC Phys to HC Phys.
 *
 * @returns VBox status.
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   pHCPhys     Where to store the corresponding HC physical address.
 *
 * @deprecated  Doesn't deal with zero, shared or write monitored pages.
 *              Avoid when writing new code!
 */
DECLINLINE(int) pgmRamGCPhys2HCPhys(PPGM pPGM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys)
{
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageEx(pPGM, GCPhys, &pPage);
    if (RT_FAILURE(rc))
        return rc;
    *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK);
    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0

/**
 * Inlined version of the ring-0 version of PGMDynMapHCPage that
 * optimizes access to pages already in the set.
 *
 * @returns VINF_SUCCESS. Will bail out to ring-3 on failure.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   HCPhys      The physical address of the page.
 * @param   ppv         Where to store the mapping address.
 */
DECLINLINE(int) pgmR0DynMapHCPageInlined(PPGM pPGM, RTHCPHYS HCPhys, void **ppv)
{
    PVM         pVM     = PGM2VM(pPGM);
    PPGMCPU     pPGMCPU = (PPGMCPU)((uint8_t *)VMMGetCpu(pVM) + pPGM->offVCpuPGM); /* very pretty ;-) */
    PPGMMAPSET  pSet    = &pPGMCPU->AutoSet;

    STAM_PROFILE_START(&pPGMCPU->StatR0DynMapHCPageInl, a);
    Assert(!(HCPhys & PAGE_OFFSET_MASK));
    Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));

    unsigned    iHash   = PGMMAPSET_HASH(HCPhys);
    unsigned    iEntry  = pSet->aiHashTable[iHash];
    if (    iEntry < pSet->cEntries
        &&  pSet->aEntries[iEntry].HCPhys == HCPhys)
    {
        *ppv = pSet->aEntries[iEntry].pvPage;
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapHCPageInlHits);
    }
    else
    {
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapHCPageInlMisses);
        pgmR0DynMapHCPageCommon(pVM, pSet, HCPhys, ppv);
    }

    STAM_PROFILE_STOP(&pPGMCPU->StatR0DynMapHCPageInl, a);
    return VINF_SUCCESS;
}


/**
 * Inlined version of the ring-0 version of PGMDynMapGCPage that optimizes
 * access to pages already in the set.
 *
 * @returns See PGMDynMapGCPage.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   GCPhys      The guest physical address of the page.
 * @param   ppv         Where to store the mapping address.
 */
DECLINLINE(int) pgmR0DynMapGCPageInlined(PPGM pPGM, RTGCPHYS GCPhys, void **ppv)
{
    PVM     pVM     = PGM2VM(pPGM);
    PPGMCPU pPGMCPU = (PPGMCPU)((uint8_t *)VMMGetCpu(pVM) + pPGM->offVCpuPGM); /* very pretty ;-) */

    STAM_PROFILE_START(&pPGMCPU->StatR0DynMapGCPageInl, a);
    AssertMsg(!(GCPhys & PAGE_OFFSET_MASK), ("%RGp\n", GCPhys));

    /*
     * Get the ram range.
     */
    PPGMRAMRANGE    pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS        off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb
        /** @todo   || page state stuff */))
    {
        /* This case is not counted into StatR0DynMapGCPageInl. */
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlRamMisses);
        return PGMDynMapGCPage(pVM, GCPhys, ppv);
    }

    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[off >> PAGE_SHIFT]);
    STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlRamHits);

    /*
     * pgmR0DynMapHCPageInlined with out stats.
     */
    PPGMMAPSET pSet = &pPGMCPU->AutoSet;
    Assert(!(HCPhys & PAGE_OFFSET_MASK));
    Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));

    unsigned    iHash   = PGMMAPSET_HASH(HCPhys);
    unsigned    iEntry  = pSet->aiHashTable[iHash];
    if (    iEntry < pSet->cEntries
        &&  pSet->aEntries[iEntry].HCPhys == HCPhys)
    {
        *ppv = pSet->aEntries[iEntry].pvPage;
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlHits);
    }
    else
    {
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlMisses);
        pgmR0DynMapHCPageCommon(pVM, pSet, HCPhys, ppv);
    }

    STAM_PROFILE_STOP(&pPGMCPU->StatR0DynMapGCPageInl, a);
    return VINF_SUCCESS;
}


/**
 * Inlined version of the ring-0 version of PGMDynMapGCPageOff that optimizes
 * access to pages already in the set.
 *
 * @returns See PGMDynMapGCPage.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   HCPhys      The physical address of the page.
 * @param   ppv         Where to store the mapping address.
 */
DECLINLINE(int) pgmR0DynMapGCPageOffInlined(PPGM pPGM, RTGCPHYS GCPhys, void **ppv)
{
    PVM     pVM     = PGM2VM(pPGM);
    PPGMCPU pPGMCPU = (PPGMCPU)((uint8_t *)VMMGetCpu(pVM) + pPGM->offVCpuPGM); /* very pretty ;-) */

    STAM_PROFILE_START(&pPGMCPU->StatR0DynMapGCPageInl, a);

    /*
     * Get the ram range.
     */
    PPGMRAMRANGE    pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS        off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb
        /** @todo   || page state stuff */))
    {
        /* This case is not counted into StatR0DynMapGCPageInl. */
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlRamMisses);
        return PGMDynMapGCPageOff(pVM, GCPhys, ppv);
    }

    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[off >> PAGE_SHIFT]);
    STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlRamHits);

    /*
     * pgmR0DynMapHCPageInlined with out stats.
     */
    PPGMMAPSET pSet = &pPGMCPU->AutoSet;
    Assert(!(HCPhys & PAGE_OFFSET_MASK));
    Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));

    unsigned    iHash   = PGMMAPSET_HASH(HCPhys);
    unsigned    iEntry  = pSet->aiHashTable[iHash];
    if (    iEntry < pSet->cEntries
        &&  pSet->aEntries[iEntry].HCPhys == HCPhys)
    {
        *ppv = (void *)((uintptr_t)pSet->aEntries[iEntry].pvPage | (PAGE_OFFSET_MASK & (uintptr_t)GCPhys));
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlHits);
    }
    else
    {
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlMisses);
        pgmR0DynMapHCPageCommon(pVM, pSet, HCPhys, ppv);
        *ppv = (void *)((uintptr_t)*ppv | (PAGE_OFFSET_MASK & (uintptr_t)GCPhys));
    }

    STAM_PROFILE_STOP(&pPGMCPU->StatR0DynMapGCPageInl, a);
    return VINF_SUCCESS;
}

#endif /* VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

/**
 * Maps the page into current context (RC and maybe R0).
 *
 * @returns pointer to the mapping.
 * @param   pVM         Pointer to the PGM instance data.
 * @param   pPage       The page.
 */
DECLINLINE(void *) pgmPoolMapPageInlined(PPGM pPGM, PPGMPOOLPAGE pPage)
{
    if (pPage->idx >= PGMPOOL_IDX_FIRST)
    {
        Assert(pPage->idx < pPGM->CTX_SUFF(pPool)->cCurPages);
        void *pv;
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        pgmR0DynMapHCPageInlined(pPGM, pPage->Core.Key, &pv);
# else
        PGMDynMapHCPage(PGM2VM(pPGM), pPage->Core.Key, &pv);
# endif
        return pv;
    }
    AssertFatalMsgFailed(("pgmPoolMapPageInlined invalid page index %x\n", pPage->idx));
}

/**
 * Temporarily maps one host page specified by HC physical address, returning
 * pointer within the page.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns The address corresponding to HCPhys.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   HCPhys      HC Physical address of the page.
 */
DECLINLINE(void *) pgmDynMapHCPageOff(PPGM pPGM, RTHCPHYS HCPhys)
{
    void *pv;
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    pgmR0DynMapHCPageInlined(pPGM, HCPhys & ~(RTHCPHYS)PAGE_OFFSET_MASK, &pv);
# else
    PGMDynMapHCPage(PGM2VM(pPGM), HCPhys & ~(RTHCPHYS)PAGE_OFFSET_MASK, &pv);
# endif
    pv = (void *)((uintptr_t)pv | ((uintptr_t)HCPhys & PAGE_OFFSET_MASK));
    return pv;
}

#endif /*  VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 || IN_RC */
#ifndef IN_RC

/**
 * Queries the Physical TLB entry for a physical guest page,
 * attempting to load the TLB entry if necessary.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pPGM        The PGM instance handle.
 * @param   GCPhys      The address of the guest page.
 * @param   ppTlbe      Where to store the pointer to the TLB entry.
 */
DECLINLINE(int) pgmPhysPageQueryTlbe(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGEMAPTLBE ppTlbe)
{
    int rc;
    PPGMPAGEMAPTLBE pTlbe = &pPGM->CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (pTlbe->GCPhys == (GCPhys & X86_PTE_PAE_PG_MASK))
    {
        STAM_COUNTER_INC(&pPGM->CTX_MID_Z(Stat,PageMapTlbHits));
        rc = VINF_SUCCESS;
    }
    else
        rc = pgmPhysPageLoadIntoTlb(pPGM, GCPhys);
    *ppTlbe = pTlbe;
    return rc;
}


/**
 * Queries the Physical TLB entry for a physical guest page,
 * attempting to load the TLB entry if necessary.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pPGM        The PGM instance handle.
 * @param   pPage       Pointer to the PGMPAGE structure corresponding to
 *                      GCPhys.
 * @param   GCPhys      The address of the guest page.
 * @param   ppTlbe      Where to store the pointer to the TLB entry.
 */
DECLINLINE(int) pgmPhysPageQueryTlbeWithPage(PPGM pPGM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPPGMPAGEMAPTLBE ppTlbe)
{
    int rc;
    PPGMPAGEMAPTLBE pTlbe = &pPGM->CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (pTlbe->GCPhys == (GCPhys & X86_PTE_PAE_PG_MASK))
    {
        STAM_COUNTER_INC(&pPGM->CTX_MID_Z(Stat,PageMapTlbHits));
        rc = VINF_SUCCESS;
    }
    else
        rc = pgmPhysPageLoadIntoTlbWithPage(pPGM, pPage, GCPhys);
    *ppTlbe = pTlbe;
    return rc;
}

#endif /* !IN_RC */

/**
 * Calculated the guest physical address of the large (4 MB) page in 32 bits paging mode.
 * Takes PSE-36 into account.
 *
 * @returns guest physical address
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   Pde         Guest Pde
 */
DECLINLINE(RTGCPHYS) pgmGstGet4MBPhysPage(PPGM pPGM, X86PDE Pde)
{
    RTGCPHYS GCPhys = Pde.u & X86_PDE4M_PG_MASK;
    GCPhys |= (RTGCPHYS)Pde.b.u8PageNoHigh << 32;

    return GCPhys & pPGM->GCPhys4MBPSEMask;
}


/**
 * Gets the page directory entry for the specified address (32-bit paging).
 *
 * @returns The page directory entry in question.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDE) pgmGstGet32bitPDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PCX86PD pGuestPD = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPD);
    if (RT_FAILURE(rc))
    {
        X86PDE ZeroPde = {0};
        AssertMsgFailedReturn(("%Rrc\n", rc), ZeroPde);
    }
#else
    PX86PD pGuestPD = pPGM->CTX_SUFF(pGst32BitPd);
# ifdef IN_RING3
    if (!pGuestPD)
        pGuestPD = pgmGstLazyMap32BitPD(pPGM);
# endif
#endif
    return pGuestPD->a[GCPtr >> X86_PD_SHIFT];
}


/**
 * Gets the address of a specific page directory entry (32-bit paging).
 *
 * @returns Pointer the page directory entry in question.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDE) pgmGstGet32bitPDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PD  pGuestPD = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPD);
    AssertRCReturn(rc, NULL);
#else
    PX86PD pGuestPD = pPGM->CTX_SUFF(pGst32BitPd);
# ifdef IN_RING3
    if (!pGuestPD)
        pGuestPD = pgmGstLazyMap32BitPD(pPGM);
# endif
#endif
    return &pGuestPD->a[GCPtr >> X86_PD_SHIFT];
}


/**
 * Gets the address the guest page directory (32-bit paging).
 *
 * @returns Pointer the page directory entry in question.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PD) pgmGstGet32bitPDPtr(PPGMCPU pPGM)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PD  pGuestPD = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPD);
    AssertRCReturn(rc, NULL);
#else
    PX86PD pGuestPD = pPGM->CTX_SUFF(pGst32BitPd);
# ifdef IN_RING3
    if (!pGuestPD)
        pGuestPD = pgmGstLazyMap32BitPD(pPGM);
# endif
#endif
    return pGuestPD;
}


/**
 * Gets the guest page directory pointer table.
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PDPT) pgmGstGetPaePDPTPtr(PPGMCPU pPGM)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PDPT pGuestPDPT = NULL;
    int rc = pgmR0DynMapGCPageOffInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPDPT);
    AssertRCReturn(rc, NULL);
#else
    PX86PDPT pGuestPDPT = pPGM->CTX_SUFF(pGstPaePdpt);
# ifdef IN_RING3
    if (!pGuestPDPT)
        pGuestPDPT = pgmGstLazyMapPaePDPT(pPGM);
# endif
#endif
    return pGuestPDPT;
}


/**
 * Gets the guest page directory pointer table entry for the specified address.
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPE) pgmGstGetPaePDPEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PDPT pGuestPDPT = 0;
    int rc = pgmR0DynMapGCPageOffInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPDPT);
    AssertRCReturn(rc, 0);
#else
    PX86PDPT pGuestPDPT = pPGM->CTX_SUFF(pGstPaePdpt);
# ifdef IN_RING3
    if (!pGuestPDPT)
        pGuestPDPT = pgmGstLazyMapPaePDPT(pPGM);
# endif
#endif
    return &pGuestPDPT->a[(GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE];
}


/**
 * Gets the page directory for the specified address.
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPAE) pgmGstGetPaePD(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);

    PX86PDPT        pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    AssertReturn(pGuestPDPT, NULL);
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    if (pGuestPDPT->a[iPdpt].n.u1Present)
    {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        PX86PDPAE   pGuestPD = NULL;
        int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
        AssertRCReturn(rc, NULL);
#else
        PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
        if (    !pGuestPD
            ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
            pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
        return pGuestPD;
        /* returning NULL is ok if we assume it's just an invalid page of some kind emulated as all 0s. (not quite true) */
    }
    return NULL;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns Pointer to the page directory entry in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDEPAE) pgmGstGetPaePDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);

    PX86PDPT        pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    AssertReturn(pGuestPDPT, NULL);
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    if (pGuestPDPT->a[iPdpt].n.u1Present)
    {
        const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        PX86PDPAE   pGuestPD = NULL;
        int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
        AssertRCReturn(rc, NULL);
#else
        PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
        if (    !pGuestPD
            ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
            pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
        return &pGuestPD->a[iPD];
        /* returning NIL_RTGCPHYS is ok if we assume it's just an invalid page or something which we'll emulate as all 0s. (not quite true) */
    }
    return NULL;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns The page directory entry in question.
 * @returns A non-present entry if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDEPAE) pgmGstGetPaePDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);
    X86PDEPAE   ZeroPde = {0};
    PX86PDPT    pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    if (RT_LIKELY(pGuestPDPT))
    {
        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
        if (pGuestPDPT->a[iPdpt].n.u1Present)
        {
            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
            PX86PDPAE   pGuestPD = NULL;
            int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
            AssertRCReturn(rc, ZeroPde);
#else
            PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
            if (    !pGuestPD
                ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
                pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
            return pGuestPD->a[iPD];
        }
    }
    return ZeroPde;
}


/**
 * Gets the page directory pointer table entry for the specified address
 * and returns the index into the page directory
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   piPD        Receives the index into the returned page directory
 * @param   pPdpe       Receives the page directory pointer entry. Optional.
 */
DECLINLINE(PX86PDPAE) pgmGstGetPaePDPtr(PPGMCPU pPGM, RTGCPTR GCPtr, unsigned *piPD, PX86PDPE pPdpe)
{
    AssertGCPtr32(GCPtr);

    PX86PDPT        pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    AssertReturn(pGuestPDPT, NULL);
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    if (pPdpe)
        *pPdpe = pGuestPDPT->a[iPdpt];
    if (pGuestPDPT->a[iPdpt].n.u1Present)
    {
        const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        PX86PDPAE   pGuestPD = NULL;
        int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
        AssertRCReturn(rc, NULL);
#else
        PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
        if (    !pGuestPD
            ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
            pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
        *piPD = iPD;
        return pGuestPD;
        /* returning NIL_RTGCPHYS is ok if we assume it's just an invalid page of some kind emulated as all 0s. */
    }
    return NULL;
}

#ifndef IN_RC

/**
 * Gets the page map level-4 pointer for the guest.
 *
 * @returns Pointer to the PML4 page.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PML4) pgmGstGetLongModePML4Ptr(PPGMCPU pPGM)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PML4 pGuestPml4;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPml4);
    AssertRCReturn(rc, NULL);
#else
    PX86PML4 pGuestPml4 = pPGM->CTX_SUFF(pGstAmd64Pml4);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R3
    if (!pGuestPml4)
        pGuestPml4 = pgmGstLazyMapPml4(pPGM);
# endif
    Assert(pGuestPml4);
#endif
    return pGuestPml4;
}


/**
 * Gets the pointer to a page map level-4 entry.
 *
 * @returns Pointer to the PML4 entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPml4       The index.
 */
DECLINLINE(PX86PML4E) pgmGstGetLongModePML4EPtr(PPGMCPU pPGM, unsigned int iPml4)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PML4 pGuestPml4;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPml4);
    AssertRCReturn(rc, NULL);
#else
    PX86PML4 pGuestPml4 = pPGM->CTX_SUFF(pGstAmd64Pml4);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R3
    if (!pGuestPml4)
        pGuestPml4 = pgmGstLazyMapPml4(pPGM);
# endif
    Assert(pGuestPml4);
#endif
    return &pGuestPml4->a[iPml4];
}


/**
 * Gets a page map level-4 entry.
 *
 * @returns The PML4 entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPml4       The index.
 */
DECLINLINE(X86PML4E) pgmGstGetLongModePML4E(PPGMCPU pPGM, unsigned int iPml4)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PML4 pGuestPml4;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPml4);
    if (RT_FAILURE(rc))
    {
        X86PML4E ZeroPml4e = {0};
        AssertMsgFailedReturn(("%Rrc\n", rc), ZeroPml4e);
    }
#else
    PX86PML4 pGuestPml4 = pPGM->CTX_SUFF(pGstAmd64Pml4);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R3
    if (!pGuestPml4)
        pGuestPml4 = pgmGstLazyMapPml4(pPGM);
# endif
    Assert(pGuestPml4);
#endif
    return pGuestPml4->a[iPml4];
}


/**
 * Gets the page directory pointer entry for the specified address.
 *
 * @returns Pointer to the page directory pointer entry in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppPml4e     Page Map Level-4 Entry (out)
 */
DECLINLINE(PX86PDPE) pgmGstGetLongModePDPTPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e)
{
    PX86PML4        pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = *ppPml4e = &pGuestPml4->a[iPml4];
    if (pPml4e->n.u1Present)
    {
        PX86PDPT pPdpt;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPml4e->u & X86_PML4E_PG_MASK, &pPdpt);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        return &pPdpt->a[iPdpt];
    }
    return NULL;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns The page directory entry in question.
 * @returns A non-present entry if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppPml4e     Page Map Level-4 Entry (out)
 * @param   pPdpe       Page directory pointer table entry (out)
 */
DECLINLINE(X86PDEPAE) pgmGstGetLongModePDEEx(PPGMCPU pPGM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e, PX86PDPE pPdpe)
{
    X86PDEPAE       ZeroPde = {0};
    PX86PML4        pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = *ppPml4e = &pGuestPml4->a[iPml4];
    if (pPml4e->n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPml4e->u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, ZeroPde);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        *pPdpe = pPdptTemp->a[iPdpt];
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PCX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, ZeroPde);

            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD->a[iPD];
        }
    }

    return ZeroPde;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns The page directory entry in question.
 * @returns A non-present entry if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDEPAE) pgmGstGetLongModePDE(PPGMCPU pPGM, RTGCPTR64 GCPtr)
{
    X86PDEPAE       ZeroPde = {0};
    PCX86PML4       pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4 = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    if (pGuestPml4->a[iPml4].n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pGuestPml4->a[iPml4].u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, ZeroPde);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PCX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, ZeroPde);

            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD->a[iPD];
        }
    }
    return ZeroPde;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns Pointer to the page directory entry in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDEPAE) pgmGstGetLongModePDEPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr)
{
    PCX86PML4       pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4 = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    if (pGuestPml4->a[iPml4].n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pGuestPml4->a[iPml4].u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, NULL);

            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return &pPD->a[iPD];
        }
    }
    return NULL;
}


/**
 * Gets the GUEST page directory pointer for the specified address.
 *
 * @returns The page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppPml4e     Page Map Level-4 Entry (out)
 * @param   pPdpe       Page directory pointer table entry (out)
 * @param   piPD        Receives the index into the returned page directory
 */
DECLINLINE(PX86PDPAE) pgmGstGetLongModePDPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e, PX86PDPE pPdpe, unsigned *piPD)
{
    PX86PML4        pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = *ppPml4e = &pGuestPml4->a[iPml4];
    if (pPml4e->n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPml4e->u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        *pPdpe = pPdptTemp->a[iPdpt];
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, NULL);

            *piPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD;
        }
    }
    return 0;
}

#endif /* !IN_RC */

/**
 * Gets the shadow page directory, 32-bit.
 *
 * @returns Pointer to the shadow 32-bit PD.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PD) pgmShwGet32BitPDPtr(PPGMCPU pPGM)
{
    return (PX86PD)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
}


/**
 * Gets the shadow page directory entry for the specified address, 32-bit.
 *
 * @returns Shadow 32-bit PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDE) pgmShwGet32BitPDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned iPd = (GCPtr >> X86_PD_SHIFT) & X86_PD_MASK;

    PX86PD pShwPde = pgmShwGet32BitPDPtr(pPGM);
    if (!pShwPde)
    {
        X86PDE ZeroPde = {0};
        return ZeroPde;
    }
    return pShwPde->a[iPd];
}


/**
 * Gets the pointer to the shadow page directory entry for the specified
 * address, 32-bit.
 *
 * @returns Pointer to the shadow 32-bit PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDE) pgmShwGet32BitPDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned iPd = (GCPtr >> X86_PD_SHIFT) & X86_PD_MASK;

    PX86PD pPde = pgmShwGet32BitPDPtr(pPGM);
    AssertReturn(pPde, NULL);
    return &pPde->a[iPd];
}


/**
 * Gets the shadow page pointer table, PAE.
 *
 * @returns Pointer to the shadow PAE PDPT.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PDPT) pgmShwGetPaePDPTPtr(PPGMCPU pPGM)
{
    return (PX86PDPT)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
}


/**
 * Gets the shadow page directory for the specified address, PAE.
 *
 * @returns Pointer to the shadow PD.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPAE) pgmShwGetPaePDPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    PX86PDPT        pPdpt = pgmShwGetPaePDPTPtr(pPGM);

    if (!pPdpt->a[iPdpt].n.u1Present)
        return NULL;

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE pShwPde = pgmPoolGetPage(PGMCPU2PGM(pPGM)->CTX_SUFF(pPool), pPdpt->a[iPdpt].u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPde, NULL);

    return (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pShwPde);
}


/**
 * Gets the shadow page directory for the specified address, PAE.
 *
 * @returns Pointer to the shadow PD.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPAE) pgmShwGetPaePDPtr(PPGMCPU pPGM, PX86PDPT pPdpt, RTGCPTR GCPtr)
{
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;

    if (!pPdpt->a[iPdpt].n.u1Present)
        return NULL;

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pgmPoolGetPage(PGMCPU2PGM(pPGM)->CTX_SUFF(pPool), pPdpt->a[iPdpt].u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPde, NULL);

    return (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pShwPde);
}


/**
 * Gets the shadow page directory entry, PAE.
 *
 * @returns PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDEPAE) pgmShwGetPaePDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPd   = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;

    PX86PDPAE pShwPde = pgmShwGetPaePDPtr(pPGM, GCPtr);
    if (!pShwPde)
    {
        X86PDEPAE ZeroPde = {0};
        return ZeroPde;
    }
    return pShwPde->a[iPd];
}


/**
 * Gets the pointer to the shadow page directory entry for an address, PAE.
 *
 * @returns Pointer to the PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDEPAE) pgmShwGetPaePDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPd   = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;

    PX86PDPAE pPde = pgmShwGetPaePDPtr(pPGM, GCPtr);
    AssertReturn(pPde, NULL);
    return &pPde->a[iPd];
}

#ifndef IN_RC

/**
 * Gets the shadow page map level-4 pointer.
 *
 * @returns Pointer to the shadow PML4.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PML4) pgmShwGetLongModePML4Ptr(PPGMCPU pPGM)
{
    return (PX86PML4)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
}


/**
 * Gets the shadow page map level-4 entry for the specified address.
 *
 * @returns The entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PML4E) pgmShwGetLongModePML4E(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPml4 = ((RTGCUINTPTR64)GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PX86PML4        pShwPml4 = pgmShwGetLongModePML4Ptr(pPGM);

    if (!pShwPml4)
    {
        X86PML4E ZeroPml4e = {0};
        return ZeroPml4e;
    }
    return pShwPml4->a[iPml4];
}


/**
 * Gets the pointer to the specified shadow page map level-4 entry.
 *
 * @returns The entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPml4       The PML4 index.
 */
DECLINLINE(PX86PML4E) pgmShwGetLongModePML4EPtr(PPGMCPU pPGM, unsigned int iPml4)
{
    PX86PML4 pShwPml4 = pgmShwGetLongModePML4Ptr(pPGM);
    if (!pShwPml4)
        return NULL;
    return &pShwPml4->a[iPml4];
}


/**
 * Gets the GUEST page directory pointer for the specified address.
 *
 * @returns The page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   piPD        Receives the index into the returned page directory
 */
DECLINLINE(PX86PDPAE) pgmGstGetLongModePDPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr, unsigned *piPD)
{
    PCX86PML4       pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    if (pGuestPml4->a[iPml4].n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pGuestPml4->a[iPml4].u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, NULL);

            *piPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD;
        }
    }
    return NULL;
}

#endif /* !IN_RC */

/**
 * Gets the page state for a physical handler.
 *
 * @returns The physical handler page state.
 * @param   pCur    The physical handler in question.
 */
DECLINLINE(unsigned) pgmHandlerPhysicalCalcState(PPGMPHYSHANDLER pCur)
{
    switch (pCur->enmType)
    {
        case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE:
            return PGM_PAGE_HNDL_PHYS_STATE_WRITE;

        case PGMPHYSHANDLERTYPE_MMIO:
        case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:
            return PGM_PAGE_HNDL_PHYS_STATE_ALL;

        default:
            AssertFatalMsgFailed(("Invalid type %d\n", pCur->enmType));
    }
}


/**
 * Gets the page state for a virtual handler.
 *
 * @returns The virtual handler page state.
 * @param   pCur    The virtual handler in question.
 * @remarks This should never be used on a hypervisor access handler.
 */
DECLINLINE(unsigned) pgmHandlerVirtualCalcState(PPGMVIRTHANDLER pCur)
{
    switch (pCur->enmType)
    {
        case PGMVIRTHANDLERTYPE_WRITE:
            return PGM_PAGE_HNDL_VIRT_STATE_WRITE;
        case PGMVIRTHANDLERTYPE_ALL:
            return PGM_PAGE_HNDL_VIRT_STATE_ALL;
        default:
            AssertFatalMsgFailed(("Invalid type %d\n", pCur->enmType));
    }
}


/**
 * Clears one physical page of a virtual handler
 *
 * @param   pPGM    Pointer to the PGM instance.
 * @param   pCur    Virtual handler structure
 * @param   iPage   Physical page index
 *
 * @remark  Only used when PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL is being set, so no
 *          need to care about other handlers in the same page.
 */
DECLINLINE(void) pgmHandlerVirtualClearPage(PPGM pPGM, PPGMVIRTHANDLER pCur, unsigned iPage)
{
    const PPGMPHYS2VIRTHANDLER pPhys2Virt = &pCur->aPhysToVirt[iPage];

    /*
     * Remove the node from the tree (it's supposed to be in the tree if we get here!).
     */
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
    AssertReleaseMsg(pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE,
                     ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                      pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias));
#endif
    if (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_IS_HEAD)
    {
        /* We're the head of the alias chain. */
        PPGMPHYS2VIRTHANDLER pRemove = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysRemove(&pPGM->CTX_SUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key); NOREF(pRemove);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
        AssertReleaseMsg(pRemove != NULL,
                         ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                          pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias));
        AssertReleaseMsg(pRemove == pPhys2Virt,
                         ("wanted: pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                          "   got:    pRemove=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                          pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias,
                          pRemove, pRemove->Core.Key, pRemove->Core.KeyLast, pRemove->offVirtHandler, pRemove->offNextAlias));
#endif
        if (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK)
        {
            /* Insert the next list in the alias chain into the tree. */
            PPGMPHYS2VIRTHANDLER pNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pPhys2Virt + (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
            AssertReleaseMsg(pNext->offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE,
                             ("pNext=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                             pNext, pNext->Core.Key, pNext->Core.KeyLast, pNext->offVirtHandler, pNext->offNextAlias));
#endif
            pNext->offNextAlias |= PGMPHYS2VIRTHANDLER_IS_HEAD;
            bool fRc = RTAvlroGCPhysInsert(&pPGM->CTX_SUFF(pTrees)->PhysToVirtHandlers, &pNext->Core);
            AssertRelease(fRc);
        }
    }
    else
    {
        /* Locate the previous node in the alias chain. */
        PPGMPHYS2VIRTHANDLER pPrev = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGet(&pPGM->CTX_SUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
        AssertReleaseMsg(pPrev != pPhys2Virt,
                         ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} pPrev=%p\n",
                          pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias, pPrev));
#endif
        for (;;)
        {
            PPGMPHYS2VIRTHANDLER pNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pPrev + (pPrev->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
            if (pNext == pPhys2Virt)
            {
                /* unlink. */
                LogFlow(("pgmHandlerVirtualClearPage: removed %p:{.offNextAlias=%#RX32} from alias chain. prev %p:{.offNextAlias=%#RX32} [%RGp-%RGp]\n",
                         pPhys2Virt, pPhys2Virt->offNextAlias, pPrev, pPrev->offNextAlias, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast));
                if (!(pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK))
                    pPrev->offNextAlias &= ~PGMPHYS2VIRTHANDLER_OFF_MASK;
                else
                {
                    PPGMPHYS2VIRTHANDLER pNewNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pPhys2Virt + (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
                    pPrev->offNextAlias = ((intptr_t)pNewNext - (intptr_t)pPrev)
                                        | (pPrev->offNextAlias & ~PGMPHYS2VIRTHANDLER_OFF_MASK);
                }
                break;
            }

            /* next */
            if (pNext == pPrev)
            {
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                AssertReleaseMsg(pNext != pPrev,
                                 ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} pPrev=%p\n",
                                  pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias, pPrev));
#endif
                break;
            }
            pPrev = pNext;
        }
    }
    Log2(("PHYS2VIRT: Removing %RGp-%RGp %#RX32 %s\n",
          pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias, R3STRING(pCur->pszDesc)));
    pPhys2Virt->offNextAlias = 0;
    pPhys2Virt->Core.KeyLast = NIL_RTGCPHYS; /* require reinsert */

    /*
     * Clear the ram flags for this page.
     */
    PPGMPAGE pPage = pgmPhysGetPage(pPGM, pPhys2Virt->Core.Key);
    AssertReturnVoid(pPage);
    PGM_PAGE_SET_HNDL_VIRT_STATE(pPage, PGM_PAGE_HNDL_VIRT_STATE_NONE);
}


/**
 * Internal worker for finding a 'in-use' shadow page give by it's physical address.
 *
 * @returns Pointer to the shadow page structure.
 * @param   pPool       The pool.
 * @param   idx         The pool page index.
 */
DECLINLINE(PPGMPOOLPAGE) pgmPoolGetPageByIdx(PPGMPOOL pPool, unsigned idx)
{
    AssertFatalMsg(idx >= PGMPOOL_IDX_FIRST && idx < pPool->cCurPages, ("idx=%d\n", idx));
    return &pPool->aPages[idx];
}


/**
 * Clear references to guest physical memory.
 *
 * @param   pPool       The pool.
 * @param   pPoolPage   The pool page.
 * @param   pPhysPage   The physical guest page tracking structure.
 * @param   iPte        Shadow PTE index
 */
DECLINLINE(void) pgmTrackDerefGCPhys(PPGMPOOL pPool, PPGMPOOLPAGE pPoolPage, PPGMPAGE pPhysPage, uint16_t iPte)
{
    /*
     * Just deal with the simple case here.
     */
# ifdef LOG_ENABLED
    const unsigned uOrg = PGM_PAGE_GET_TRACKING(pPhysPage);
# endif
    const unsigned cRefs = PGM_PAGE_GET_TD_CREFS(pPhysPage);
    if (cRefs == 1)
    {
        Assert(pPoolPage->idx == PGM_PAGE_GET_TD_IDX(pPhysPage));
        Assert(iPte == PGM_PAGE_GET_PTE_INDEX(pPhysPage));
        /* Invalidate the tracking data. */
        PGM_PAGE_SET_TRACKING(pPhysPage, 0);
    }
    else
        pgmPoolTrackPhysExtDerefGCPhys(pPool, pPoolPage, pPhysPage, iPte);
    Log2(("pgmTrackDerefGCPhys: %x -> %x pPhysPage=%R[pgmpage]\n", uOrg, PGM_PAGE_GET_TRACKING(pPhysPage), pPhysPage ));
}


/**
 * Moves the page to the head of the age list.
 *
 * This is done when the cached page is used in one way or another.
 *
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 */
DECLINLINE(void) pgmPoolCacheUsed(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    PVM pVM = pPool->CTX_SUFF(pVM);
    pgmLock(pVM);

    /*
     * Move to the head of the age list.
     */
    if (pPage->iAgePrev != NIL_PGMPOOL_IDX)
    {
        /* unlink */
        pPool->aPages[pPage->iAgePrev].iAgeNext = pPage->iAgeNext;
        if (pPage->iAgeNext != NIL_PGMPOOL_IDX)
            pPool->aPages[pPage->iAgeNext].iAgePrev = pPage->iAgePrev;
        else
            pPool->iAgeTail = pPage->iAgePrev;

        /* insert at head */
        pPage->iAgePrev = NIL_PGMPOOL_IDX;
        pPage->iAgeNext = pPool->iAgeHead;
        Assert(pPage->iAgeNext != NIL_PGMPOOL_IDX); /* we would've already been head then */
        pPool->iAgeHead = pPage->idx;
        pPool->aPages[pPage->iAgeNext].iAgePrev = pPage->idx;
    }
    pgmUnlock(pVM);
}

/**
 * Locks a page to prevent flushing (important for cr3 root pages or shadow pae pd pages).
 *
 * @param   pVM         VM Handle.
 * @param   pPage       PGM pool page
 */
DECLINLINE(void) pgmPoolLockPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Assert(PGMIsLockOwner(pPool->CTX_SUFF(pVM)));
    ASMAtomicIncU32(&pPage->cLocked);
}


/**
 * Unlocks a page to allow flushing again
 *
 * @param   pVM         VM Handle.
 * @param   pPage       PGM pool page
 */
DECLINLINE(void) pgmPoolUnlockPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Assert(PGMIsLockOwner(pPool->CTX_SUFF(pVM)));
    Assert(pPage->cLocked);
    ASMAtomicDecU32(&pPage->cLocked);
}


/**
 * Checks if the page is locked (e.g. the active CR3 or one of the four PDs of a PAE PDPT)
 *
 * @returns VBox status code.
 * @param   pPage       PGM pool page
 */
DECLINLINE(bool) pgmPoolIsPageLocked(PPGM pPGM, PPGMPOOLPAGE pPage)
{
    if (pPage->cLocked)
    {
        LogFlow(("pgmPoolIsPageLocked found root page %d\n", pPage->enmKind));
        if (pPage->cModifications)
            pPage->cModifications = 1; /* reset counter (can't use 0, or else it will be reinserted in the modified list) */
        return true;
    }
    return false;
}


/**
 * Tells if mappings are to be put into the shadow page table or not.
 *
 * @returns boolean result
 * @param   pVM         VM handle.
 */
DECL_FORCE_INLINE(bool) pgmMapAreMappingsEnabled(PPGM pPGM)
{
#ifdef PGM_WITHOUT_MAPPINGS
    /* There are no mappings in VT-x and AMD-V mode. */
    Assert(pPGM->fMappingsDisabled);
    return false;
#else
    return !pPGM->fMappingsDisabled;
#endif
}


/**
 * Checks if the mappings are floating and enabled.
 *
 * @returns true / false.
 * @param   pVM         The VM handle.
 */
DECL_FORCE_INLINE(bool) pgmMapAreMappingsFloating(PPGM pPGM)
{
#ifdef PGM_WITHOUT_MAPPINGS
    /* There are no mappings in VT-x and AMD-V mode. */
    Assert(pPGM->fMappingsDisabled);
    return false;
#else
    return !pPGM->fMappingsDisabled
        && !pPGM->fMappingsFixed;
#endif
}

/** @} */

#endif

