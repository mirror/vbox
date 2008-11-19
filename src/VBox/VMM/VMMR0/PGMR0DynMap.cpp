/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, ring-0 dynamic mapping cache.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
*   Internal Functions                                                         *
*******************************************************************************/
#include <VBox/pgm.h>
#include "../PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/cpuset.h>
#include <iprt/spinlock.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Ring-0 dynamic mapping cache segment.
 *
 * The dynamic mapping cache can be extended with additional segments if the
 * load is found to be too high.  This done the next time a VM is created, under
 * the protection of the init mutex.  The arrays is reallocated and the new
 * segment is added to the end of these.  Nothing is rehashed of course, as the
 * indexes / addresses must remain unchanged.
 *
 * This structure is only modified while owning the init mutex or during module
 * init / term.
 */
typedef struct PGMR0DYNMAPSEG
{
    /** Pointer to the next segment. */
    struct PGMR0DYNMAPSEG      *pNext;
    /** The memory object for the virtual address range that we're abusing. */
    RTR0MEMOBJ                  hMemObj;
    /** The memory object for the page tables. */
    RTR0MEMOBJ                  hMemObjPT;
    /** The start page in the cache. (I.e. index into the arrays.) */
    uint32_t                    iPage;
    /** The number of pages this segment contributes. */
    uint32_t                    cPages;
} PGMR0DYNMAPSEG;
/** Pointer to a ring-0 dynamic mapping cache segment. */
typedef PGMR0DYNMAPSEG *PPGMR0DYNMAPSEG;


/**
 * Ring-0 dynamic mapping cache entry.
 *
 * This structure tracks
 */
typedef struct PGMR0DYNMAPENTRY
{
    /** The physical address of the currently mapped page.
     * This is duplicate for three reasons: cache locality, cache policy of the PT
     * mappings and sanity checks.   */
    RTHCPHYS                    HCPhys;
    /** Pointer to the page. */
    void                       *pvPage;
    /** The number of references. */
    int32_t volatile            cRefs;
    /** PTE pointer union. */
    union PGMR0DYNMAPENTRY_PPTE
    {
        /** PTE pointer, 32-bit legacy version. */
        PX86PTE                 pLegacy;
        /** PTE pointer, PAE version. */
        PX86PTEPAE              pPae;
    } uPte;
    /** CPUs that haven't invalidated this entry after it's last update. */
    RTCPUSET                    PendingSet;
} PGMR0DYNMAPENTRY;
/** Pointer to a ring-0 dynamic mapping cache entry. */
typedef PGMR0DYNMAPENTRY *PPGMR0DYNMAPENTRY;


/**
 * Ring-0 dynamic mapping cache.
 *
 * This is initialized during VMMR0 module init but no segments are allocated at
 * that time.  Segments will be added when the first VM is started and removed
 * again when the last VM shuts down, thus avoid consuming memory while dormant.
 * At module termination, the remaining bits will be freed up.
 */
typedef struct PGMR0DYNMAP
{
    /** The usual magic number / eye catcher. */
    uint32_t                    u32Magic;
    /** Spinlock serializing the normal operation of the cache. */
    RTSPINLOCK                  hSpinlock;
    /** Array for tracking and managing the pages.  */
    PPGMR0DYNMAPENTRY           paPages;
    /** The cache size given as a number of pages. */
    uint32_t                    cPages;
    /** Whether it's 32-bit legacy or PAE/AMD64 paging mode. */
    bool                        fLegacyMode;
    /** The current load. */
    uint32_t                    cLoad;
    /** The max load.
     * This is maintained to get trigger adding of more mapping space. */
    uint32_t                    cMaxLoad;
    /** Initialization / termination lock. */
    RTSEMFASTMUTEX              hInitLock;
    /** The number of users (protected by hInitLock). */
    uint32_t                    cUsers;
    /** Array containing a copy of the original page tables.
     * The entries are either X86PTE or X86PTEPAE according to fLegacyMode. */
    void                       *pvSavedPTs;
} PGMR0DYNMAP;
/** Pointer to the ring-0 dynamic mapping cache */
typedef PGMR0DYNMAP *PPGMR0DYNMAP;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the ring-0 dynamic mapping cache. */
static PPGMR0DYNMAP g_pPGMR0DynMap;




/**
 * Initializes the ring-0 dynamic mapping cache.
 *
 * @returns VBox status code.
 */
VMMR0DECL(int) PGMR0DynMapInit(void)
{
    return VINF_SUCCESS;
}


/**
 * Terminates the ring-0 dynamic mapping cache.
 */
VMMR0DECL(void) PGMR0DynMapTerm(void)
{
}


/**
 * Initializes the dynamic mapping cache for a new VM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the shared VM structure.
 */
VMMR0DECL(int) PGMR0DynMapInitVM(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * Terminates the dynamic mapping cache usage for a VM.
 *
 * @param   pVM         Pointer to the shared VM structure.
 */
VMMR0DECL(void) PGMR0DynMapTermVM(PVM pVM)
{
    NOREF(pVM);
}


/**
 * Release references to a page, caller owns the spin lock.
 *
 * @param   pThis       The dynamic mapping cache instance.
 * @param   iPage       The page.
 * @param   cRefs       The number of references to release.
 */
DECLINLINE(void) pgmR0DynMapReleasePageLocked(PPGMR0DYNMAP pThis, uint32_t iPage, int32_t cRefs)
{
    cRefs = ASMAtomicSubS32(&pThis->paPages[iPage].cRefs, cRefs);
    AssertMsg(cRefs >= 0, ("%d\n", cRefs));
    if (!cRefs)
        pThis->cLoad--;
}


/**
 * Release references to a page, caller does not own the spin lock.
 *
 * @param   pThis       The dynamic mapping cache instance.
 * @param   iPage       The page.
 * @param   cRefs       The number of references to release.
 */
static void pgmR0DynMapReleasePage(PPGMR0DYNMAP pThis, uint32_t iPage, uint32_t cRefs)
{
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    pgmR0DynMapReleasePageLocked(pThis, iPage, cRefs);
    RTSpinlockRelease(pThis->hSpinlock, &Tmp);
}


/**
 * pgmR0DynMapPage worker that deals with the tedious bits.
 *
 * @returns The page index on success, UINT32_MAX on failure.
 * @param   pThis       The dynamic mapping cache instance.
 * @param   HCPhys      The address of the page to be mapped.
 * @param   iPage       The page index pgmR0DynMapPage hashed HCPhys to.
 */
static uint32_t pgmR0DynMapPageSlow(PPGMR0DYNMAP pThis, RTHCPHYS HCPhys, uint32_t iPage)
{
    /*
     * Check if any of the first 5 pages are unreferenced since the caller
     * already has made sure they aren't matching.
     */
    uint32_t const      cPages  = cPages;
    PPGMR0DYNMAPENTRY   paPages = pThis->paPages;
    uint32_t            iFreePage;
    if (!paPages[iPage].cRefs)
        iFreePage = iPage;
    else if (!paPages[(iPage + 1) % cPages].cRefs)
        iFreePage = iPage;
    else if (!paPages[(iPage + 2) % cPages].cRefs)
        iFreePage = iPage;
    else if (!paPages[(iPage + 3) % cPages].cRefs)
        iFreePage = iPage;
    else if (!paPages[(iPage + 4) % cPages].cRefs)
        iFreePage = iPage;
    else
    {
        /*
         * Search for an unused or matching entry.
         */
        iFreePage = (iPage + 5) % pThis->cPages;
        for (;;)
        {
            if (paPages[iFreePage].HCPhys == HCPhys)
                return iFreePage;
            if (!paPages[iFreePage].cRefs)
                break;

            /* advance */
            iFreePage = (iFreePage + 1) % cPages;
            if (RT_UNLIKELY(iFreePage != iPage))
                return UINT32_MAX;
        }
    }

    /*
     * Setup the new entry.
     */
    paPages[iFreePage].HCPhys = HCPhys;
    RTCpuSetFill(&paPages[iFreePage].PendingSet);
    if (pThis->fLegacyMode)
        paPages[iFreePage].uPte.pLegacy->u = (paPages[iFreePage].uPte.pLegacy->u & X86_PTE_G | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT)
                                           | X86_PTE_P | X86_PTE_A | X86_PTE_D
                                           | (HCPhys & X86_PTE_PG_MASK);
    else
        paPages[iFreePage].uPte.pPae->u    = (paPages[iFreePage].uPte.pPae->u    & X86_PTE_G | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT)
                                           | X86_PTE_P | X86_PTE_A | X86_PTE_D
                                           | (HCPhys & X86_PTE_PAE_PG_MASK);
    return iFreePage;
}


/**
 * Maps a page into the pool.
 *
 * @returns Pointer to the mapping.
 * @param   pThis       The dynamic mapping cache instance.
 * @param   HCPhys      The address of the page to be mapped.
 * @param   piPage      Where to store the page index.
 */
DECLINLINE(void *) pgmR0DynMapPage(PPGMR0DYNMAP pThis, RTHCPHYS HCPhys, uint32_t *piPage)
{
    RTSPINLOCKTMP   Tmp       = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);
    AssertMsg(!(HCPhys & PAGE_OFFSET_MASK), ("HCPhys=%RHp\n", HCPhys));

    /*
     * Find an entry, if possible a matching one. The HCPhys address is hashed
     * down to a page index, collisions are handled by linear searching. Optimize
     * for a hit in the first 5 pages.
     *
     * To the cheap hits here and defer the tedious searching and inserting
     * to a helper function.
     */
    uint32_t const      cPages  = cPages;
    uint32_t            iPage   = (HCPhys >> PAGE_SHIFT) % cPages;
    PPGMR0DYNMAPENTRY   paPages = pThis->paPages;
    if (paPages[iPage].HCPhys != HCPhys)
    {
        uint32_t    iPage2 = (iPage + 1) % cPages;
        if (paPages[iPage2].HCPhys != HCPhys)
        {
            iPage2 = (iPage + 2) % cPages;
            if (paPages[iPage2].HCPhys != HCPhys)
            {
                iPage2 = (iPage + 3) % cPages;
                if (paPages[iPage2].HCPhys != HCPhys)
                {
                    iPage2 = (iPage + 4) % cPages;
                    if (paPages[iPage2].HCPhys != HCPhys)
                    {
                        iPage = pgmR0DynMapPageSlow(pThis, HCPhys, iPage);
                        if (RT_UNLIKELY(iPage == UINT32_MAX))
                        {
                            RTSpinlockRelease(pThis->hSpinlock, &Tmp);
                            return NULL;
                        }
                    }
                    else
                        iPage = iPage2;
                }
                else
                    iPage = iPage2;
            }
            else
                iPage = iPage2;
        }
        else
            iPage = iPage2;
    }

    /*
     * Reference it, update statistics and get the return address.
     */
    if (ASMAtomicIncS32(&paPages[iPage].cRefs) == 1)
    {
        pThis->cLoad++;
        if (pThis->cLoad > pThis->cMaxLoad)
            pThis->cMaxLoad = pThis->cLoad;
        Assert(pThis->cLoad <= pThis->cPages);
    }
    void *pvPage = paPages[iPage].pvPage;

    /*
     * Invalidate the entry?
     */
    RTCPUID idRealCpu = RTMpCpuId();
    bool fInvalidateIt = RTCpuSetIsMember(&paPages[iPage].PendingSet, idRealCpu);
    if (fInvalidateIt)
        RTCpuSetDel(&paPages[iPage].PendingSet, idRealCpu);

    RTSpinlockRelease(pThis->hSpinlock, &Tmp);

    /*
     * Do the actual invalidation outside the spinlock.
     */
    ASMInvalidatePage(pvPage);

    *piPage = iPage;
    return pvPage;
}


/**
 * Signals the start of a new set of mappings.
 *
 * Mostly for strictness. PGMDynMapHCPage won't work unless this
 * API is called.
 *
 * @param   pVCpu       The shared data for the current virtual CPU.
 */
VMMDECL(void) PGMDynMapStartAutoSet(PVMCPU pVCpu)
{
    Assert(pVCpu->pgm.s.AutoSet.cEntries == PGMMAPSET_CLOSED);
    pVCpu->pgm.s.AutoSet.cEntries = 0;
}


/**
 * Releases the dynamic memory mappings made by PGMDynMapHCPage and associates
 * since the PGMDynMapStartAutoSet call.
 *
 * @param   pVCpu       The shared data for the current virtual CPU.
 */
VMMDECL(void) PGMDynMapReleaseAutoSet(PVMCPU pVCpu)
{
    PPGMMAPSET  pSet = &pVCpu->pgm.s.AutoSet;

    /* close the set */
    uint32_t    i = pVCpu->pgm.s.AutoSet.cEntries;
    AssertMsg(i <= RT_ELEMENTS(pVCpu->pgm.s.AutoSet.aEntries), ("%u\n", i));
    pVCpu->pgm.s.AutoSet.cEntries = PGMMAPSET_CLOSED;

    /* release any pages we're referencing. */
    if (i != 0 && RT_LIKELY(i <= RT_ELEMENTS(pVCpu->pgm.s.AutoSet.aEntries)))
    {
        PPGMR0DYNMAP    pThis = g_pPGMR0DynMap;
        RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquire(pThis->hSpinlock, &Tmp);

        while (i-- > 0)
        {
            uint32_t iPage = pSet->aEntries[i].iPage;
            Assert(iPage < pThis->cPages);
            int32_t  cRefs = pSet->aEntries[i].cRefs;
            Assert(cRefs > 0);
            pgmR0DynMapReleasePageLocked(pThis, iPage, cRefs);
        }

        Assert(pThis->cLoad <= pThis->cPages);
        RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    }
}


/**
 * Migrates the automatic mapping set of the current vCPU if necessary.
 *
 * This is called when re-entering the hardware assisted execution mode after a
 * nip down to ring-3.  We run the risk that the CPU might have change and we
 * will therefore make sure all the cache entries currently in the auto set will
 * be valid on the new CPU.  If the cpu didn't change nothing will happen as all
 * the entries will have been flagged as invalidated.
 *
 * @param   pVCpu       The shared data for the current virtual CPU.
 * @thread  EMT
 */
VMMDECL(void) PGMDynMapMigrateAutoSet(PVMCPU pVCpu)
{
    PPGMMAPSET  pSet = &pVCpu->pgm.s.AutoSet;
    uint32_t    i = pVCpu->pgm.s.AutoSet.cEntries;
    AssertMsg(i <= RT_ELEMENTS(pVCpu->pgm.s.AutoSet.aEntries), ("%u\n", i));
    if (i != 0 && RT_LIKELY(i <= RT_ELEMENTS(pVCpu->pgm.s.AutoSet.aEntries)))
    {
        PPGMR0DYNMAP    pThis = g_pPGMR0DynMap;
        RTCPUID         idRealCpu = RTMpCpuId();

        while (i-- > 0)
        {
            Assert(pSet->aEntries[i].cRefs > 0);
            uint32_t iPage = pSet->aEntries[i].iPage;
            Assert(iPage < pThis->cPages);
            if (RTCpuSetIsMember(&pThis->paPages[iPage].PendingSet, idRealCpu))
            {
                RTCpuSetDel(&pThis->paPages[iPage].PendingSet, idRealCpu);
                ASMInvalidatePage(pThis->paPages[iPage].pvPage);
            }
        }
    }
}


/**
 * As a final resort for a full auto set, try merge duplicate entries.
 *
 * @param   pSet        The set.
 */
static void pgmDynMapOptimizeAutoSet(PPGMMAPSET pSet)
{
    for (uint32_t i = 0 ; i < pSet->cEntries; i++)
    {
        uint16_t const  iPage = pSet->aEntries[i].iPage;
        uint32_t        j     = i + 1;
        while (j < pSet->cEntries)
        {
            if (pSet->aEntries[j].iPage != iPage)
                j++;
            else
            {
                /* merge j with i removing j. */
                pSet->aEntries[i].cRefs += pSet->aEntries[j].cRefs;
                pSet->cEntries--;
                if (j < pSet->cEntries)
                {
                    pSet->aEntries[j] = pSet->aEntries[pSet->cEntries];
                    pSet->aEntries[pSet->cEntries].iPage = UINT16_MAX;
                    pSet->aEntries[pSet->cEntries].cRefs = 0;
                }
                else
                {
                    pSet->aEntries[j].iPage = UINT16_MAX;
                    pSet->aEntries[j].cRefs = 0;
                }
            }
        }
    }
}


/* documented elsewhere - a bit of a mess. */
VMMDECL(int) PGMDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    AssertMsg(!(HCPhys & PAGE_OFFSET_MASK), ("HCPhys=%RHp\n", HCPhys));

    /*
     * Map it.
     */
    uint32_t        iPage;
    void           *pvPage  = pgmR0DynMapPage(g_pPGMR0DynMap, HCPhys, &iPage);
    if (RT_UNLIKELY(!pvPage))
    {
        static uint32_t s_cBitched = 0;
        if (++s_cBitched < 10)
            LogRel(("PGMDynMapHCPage: cLoad=%u/%u cPages=%u\n",
                    g_pPGMR0DynMap->cLoad, g_pPGMR0DynMap->cMaxLoad, g_pPGMR0DynMap->cPages));
        return VERR_PGM_DYNMAP_FAILED;
    }

    /*
     * Add the page to the auto reference set.
     * If it's less than half full, don't bother looking for duplicates.
     */
    PVMCPU          pVCpu   = VMMGetCpu(pVM);
    PPGMMAPSET      pSet    = &pVCpu->pgm.s.AutoSet;
    if (pSet->cEntries < RT_ELEMENTS(pSet->aEntries) / 2)
    {
        pSet->aEntries[pSet->cEntries].cRefs = 1;
        pSet->aEntries[pSet->cEntries].iPage = iPage;
    }
    else
    {
        Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));
        int32_t     i = pSet->cEntries;
        while (i-- > 0)
            if (pSet->aEntries[i].iPage)
            {
                pSet->aEntries[i].cRefs++;
                break;
            }
        if (i < 0)
        {
            if (RT_UNLIKELY(pSet->cEntries >= RT_ELEMENTS(pSet->aEntries)))
                pgmDynMapOptimizeAutoSet(pSet);
            if (RT_LIKELY(pSet->cEntries < RT_ELEMENTS(pSet->aEntries)))
            {
                pSet->aEntries[pSet->cEntries].cRefs = 1;
                pSet->aEntries[pSet->cEntries].iPage = iPage;
            }
            else
            {
                /* We're screwed. */
                pgmR0DynMapReleasePage(g_pPGMR0DynMap, iPage, 1);

                static uint32_t s_cBitched = 0;
                if (++s_cBitched < 10)
                    LogRel(("PGMDynMapHCPage: set is full!\n"));
                return VERR_PGM_DYNMAP_FULL_SET;
            }
        }
    }

    return VINF_SUCCESS;
}


