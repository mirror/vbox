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
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include "../PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/cpuset.h>
#include <iprt/memobj.h>
#include <iprt/mp.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The max size of the mapping cache (in pages). */
#define PGMR0DYNMAP_MAX_PAGES               ((8*_1M) >> PAGE_SHIFT)
/** The small segment size that is adopted on out-of-memory conditions with a
 * single big segment. */
#define PGMR0DYNMAP_SMALL_SEG_PAGES         128
/** The number of pages we reserve per CPU. */
#define PGMR0DYNMAP_PAGES_PER_CPU           64
/** Calcs the overload threshold. Current set at 50%. */
#define PGMR0DYNMAP_CALC_OVERLOAD(cPages)   ((cPages) / 2)


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
    /** The start page in the cache. (I.e. index into the arrays.) */
    uint16_t                    iPage;
    /** The number of pages this segment contributes. */
    uint16_t                    cPages;
    /** The number of page tables. */
    uint16_t                    cPTs;
    /** The memory objects for the page tables. */
    RTR0MEMOBJ                  ahMemObjPTs[1];
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
        /** PTE pointer, the void version. */
        void                   *pv;
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
    /** The usual magic number / eye catcher (PGMR0DYNMAP_MAGIC). */
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
    /** The max load ever.
     * This is maintained to get trigger adding of more mapping space. */
    uint32_t                    cMaxLoad;
    /** Initialization / termination lock. */
    RTSEMFASTMUTEX              hInitLock;
    /** The number of users (protected by hInitLock). */
    uint32_t                    cUsers;
    /** Array containing a copy of the original page tables.
     * The entries are either X86PTE or X86PTEPAE according to fLegacyMode. */
    void                       *pvSavedPTEs;
    /** List of segments. */
    PPGMR0DYNMAPSEG             pSegHead;
    /** The paging mode. */
    SUPPAGINGMODE               enmPgMode;
} PGMR0DYNMAP;
/** Pointer to the ring-0 dynamic mapping cache */
typedef PGMR0DYNMAP *PPGMR0DYNMAP;

/** PGMR0DYNMAP::u32Magic. (Jens Christian Bugge Wesseltoft) */
#define PGMR0DYNMAP_MAGIC       0x19640201


/**
 * Paging level data.
 */
typedef struct PGMR0DYNMAPPGLVL
{
    uint32_t            cLevels;    /**< The number of levels. */
    struct
    {
        RTHCPHYS        HCPhys;     /**< The address of the page for the current level,
                                     *  i.e. what hMemObj/hMapObj is currently mapping. */
        RTHCPHYS        fPhysMask;  /**< Mask for extracting HCPhys from uEntry. */
        RTR0MEMOBJ      hMemObj;    /**< Memory object for HCPhys, PAGE_SIZE. */
        RTR0MEMOBJ      hMapObj;    /**< Mapping object for hMemObj. */
        uint32_t        fPtrShift;  /**< The pointer shift count. */
        uint64_t        fPtrMask;   /**< The mask to apply to the shifted pointer to get the table index. */
        uint64_t        fAndMask;   /**< And mask to check entry flags. */
        uint64_t        fResMask;   /**< The result from applying fAndMask. */
        union
        {
            void        *pv;        /**< hMapObj address. */
            PX86PGUINT   paLegacy;  /**< Legacy table view. */
            PX86PGPAEUINT paPae;    /**< PAE/AMD64 table view. */
        } u;
    } a[4];
} PGMR0DYNMAPPGLVL;
/** Pointer to paging level data. */
typedef PGMR0DYNMAPPGLVL *PPGMR0DYNMAPPGLVL;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the ring-0 dynamic mapping cache. */
static PPGMR0DYNMAP g_pPGMR0DynMap;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void pgmR0DynMapReleasePage(PPGMR0DYNMAP pThis, uint32_t iPage, uint32_t cRefs);
static int  pgmR0DynMapSetup(PPGMR0DYNMAP pThis);
static int  pgmR0DynMapExpand(PPGMR0DYNMAP pThis);
static void pgmR0DynMapTearDown(PPGMR0DYNMAP pThis);
#ifdef DEBUG
static int  pgmR0DynMapTest(PVM pVM);
#endif


/**
 * Initializes the ring-0 dynamic mapping cache.
 *
 * @returns VBox status code.
 */
VMMR0DECL(int) PGMR0DynMapInit(void)
{
    Assert(!g_pPGMR0DynMap);

    /*
     * Create and initialize the cache instance.
     */
    PPGMR0DYNMAP pThis = (PPGMR0DYNMAP)RTMemAllocZ(sizeof(*pThis));
    AssertLogRelReturn(pThis, VERR_NO_MEMORY);
    int             rc = VINF_SUCCESS;
    pThis->enmPgMode = SUPR0GetPagingMode();
    switch (pThis->enmPgMode)
    {
        case SUPPAGINGMODE_32_BIT:
        case SUPPAGINGMODE_32_BIT_GLOBAL:
            pThis->fLegacyMode = false;
            break;
        case SUPPAGINGMODE_PAE:
        case SUPPAGINGMODE_PAE_GLOBAL:
        case SUPPAGINGMODE_PAE_NX:
        case SUPPAGINGMODE_PAE_GLOBAL_NX:
        case SUPPAGINGMODE_AMD64:
        case SUPPAGINGMODE_AMD64_GLOBAL:
        case SUPPAGINGMODE_AMD64_NX:
        case SUPPAGINGMODE_AMD64_GLOBAL_NX:
            pThis->fLegacyMode = false;
            break;
        default:
            rc = VERR_INTERNAL_ERROR;
            break;
    }
    if (RT_SUCCESS(rc))
    {
        rc = RTSemFastMutexCreate(&pThis->hInitLock);
        if (RT_SUCCESS(rc))
        {
            rc = RTSpinlockCreate(&pThis->hSpinlock);
            if (RT_SUCCESS(rc))
            {
                pThis->u32Magic = PGMR0DYNMAP_MAGIC;
                g_pPGMR0DynMap = pThis;
                return VINF_SUCCESS;
            }
            RTSemFastMutexDestroy(pThis->hInitLock);
        }
    }
    RTMemFree(pThis);
    return rc;
}


/**
 * Terminates the ring-0 dynamic mapping cache.
 */
VMMR0DECL(void) PGMR0DynMapTerm(void)
{
    /*
     * Destroy the cache.
     *
     * There is not supposed to be any races here, the loader should
     * make sure about that. So, don't bother locking anything.
     *
     * The VM objects should all be destroyed by now, so there is no
     * dangling users or anything like that to clean up. This routine
     * is just a mirror image of PGMR0DynMapInit.
     */
    PPGMR0DYNMAP pThis = g_pPGMR0DynMap;
    if (pThis)
    {
        AssertPtr(pThis);
        g_pPGMR0DynMap = NULL;

        AssertLogRelMsg(!pThis->cUsers && !pThis->paPages && !pThis->pvSavedPTEs && !pThis->cPages,
                        ("cUsers=%d paPages=%p pvSavedPTEs=%p cPages=%#x\n",
                         pThis->cUsers, pThis->paPages, pThis->pvSavedPTEs, pThis->cPages));

        /* Free the associated resources. */
        RTSemFastMutexDestroy(pThis->hInitLock);
        pThis->hInitLock = NIL_RTSEMFASTMUTEX;
        RTSpinlockDestroy(pThis->hSpinlock);
        pThis->hSpinlock = NIL_RTSPINLOCK;
        pThis->u32Magic = UINT32_MAX;
        RTMemFree(pThis);
    }
}


/**
 * Initializes the dynamic mapping cache for a new VM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the shared VM structure.
 */
VMMR0DECL(int) PGMR0DynMapInitVM(PVM pVM)
{
    AssertMsgReturn(!pVM->pgm.s.pvR0DynMapUsed, ("%p (pThis=%p)\n", pVM->pgm.s.pvR0DynMapUsed, g_pPGMR0DynMap), VERR_WRONG_ORDER);

    /*
     * Initialize the auto sets.
     */
    VMCPUID idCpu = pVM->cCPUs;
    AssertReturn(idCpu > 0 && idCpu <= VMCPU_MAX_CPU_COUNT, VERR_INTERNAL_ERROR);
    while (idCpu-- > 0)
    {
        PPGMMAPSET pSet = &pVM->aCpus[idCpu].pgm.s.AutoSet;
        uint32_t j = RT_ELEMENTS(pSet->aEntries);
        while (j-- > 0)
        {
            pSet->aEntries[j].iPage = UINT16_MAX;
            pSet->aEntries[j].cRefs = 0;
        }
        pSet->cEntries = PGMMAPSET_CLOSED;
    }

    /*
     * Do we need the cache? Skip the last bit if we don't.
     */
#if 1
    if (!VMMIsHwVirtExtForced(pVM))
        return VINF_SUCCESS;
#endif

    /*
     * Reference and if necessary setup or expand the cache.
     */
    PPGMR0DYNMAP pThis = g_pPGMR0DynMap;
    AssertPtrReturn(pThis, VERR_INTERNAL_ERROR);
    int rc = RTSemFastMutexRequest(pThis->hInitLock);
    AssertLogRelRCReturn(rc, rc);

    pThis->cUsers++;
    if (pThis->cUsers == 1)
    {
        rc = pgmR0DynMapSetup(pThis);
#ifdef DEBUG
        if (RT_SUCCESS(rc))
        {
            rc = pgmR0DynMapTest(pVM);
            if (RT_FAILURE(rc))
                pgmR0DynMapTearDown(pThis);
        }
#endif
    }
    else if (pThis->cMaxLoad > PGMR0DYNMAP_CALC_OVERLOAD(pThis->cPages))
        rc = pgmR0DynMapExpand(pThis);
    if (RT_SUCCESS(rc))
        pVM->pgm.s.pvR0DynMapUsed = pThis;
    else
        pThis->cUsers--;

    RTSemFastMutexRelease(pThis->hInitLock);
    return rc;
}


/**
 * Terminates the dynamic mapping cache usage for a VM.
 *
 * @param   pVM         Pointer to the shared VM structure.
 */
VMMR0DECL(void) PGMR0DynMapTermVM(PVM pVM)
{
    /*
     * Return immediately if we're not using the cache.
     */
    if (!pVM->pgm.s.pvR0DynMapUsed)
        return;

    PPGMR0DYNMAP pThis = g_pPGMR0DynMap;
    AssertPtrReturnVoid(pThis);

    int rc = RTSemFastMutexRequest(pThis->hInitLock);
    AssertLogRelRCReturnVoid(rc);

    if (pVM->pgm.s.pvR0DynMapUsed == pThis)
    {
        pVM->pgm.s.pvR0DynMapUsed = NULL;

        /*
         * Clean up and check the auto sets.
         */
        VMCPUID idCpu = pVM->cCPUs;
        while (idCpu-- > 0)
        {
            PPGMMAPSET pSet = &pVM->aCpus[idCpu].pgm.s.AutoSet;
            uint32_t j = pSet->cEntries;
            if (j <= RT_ELEMENTS(pSet->aEntries))
            {
                /*
                 * The set is open, close it.
                 */
                while (j-- > 0)
                {
                    int32_t  cRefs = pSet->aEntries[j].cRefs;
                    uint32_t iPage = pSet->aEntries[j].iPage;
                    LogRel(("PGMR0DynMapTermVM: %d dangling refs to %#x\n", cRefs, iPage));
                    if (iPage < pThis->cPages && cRefs > 0)
                        pgmR0DynMapReleasePage(pThis, iPage, cRefs);
                    else
                        AssertLogRelMsgFailed(("cRefs=%d iPage=%#x cPages=%u\n", cRefs, iPage, pThis->cPages));

                    pSet->aEntries[j].iPage = UINT16_MAX;
                    pSet->aEntries[j].cRefs = 0;
                }
                pSet->cEntries = PGMMAPSET_CLOSED;
            }
            else
                AssertMsg(j == PGMMAPSET_CLOSED, ("cEntries=%#x\n", j));

            j = RT_ELEMENTS(pSet->aEntries);
            while (j-- > 0)
            {
                Assert(pSet->aEntries[j].iPage == UINT16_MAX);
                Assert(!pSet->aEntries[j].cRefs);
            }
        }

        /*
         * Release our reference to the mapping cache.
         */
        Assert(pThis->cUsers > 0);
        pThis->cUsers--;
        if (!pThis->cUsers)
            pgmR0DynMapTearDown(pThis);
    }
    else
        AssertLogRelMsgFailed(("pvR0DynMapUsed=%p pThis=%p\n", pVM->pgm.s.pvR0DynMapUsed, pThis));

    RTSemFastMutexRelease(pThis->hInitLock);
}


/**
 * Calculate the new cache size based on cMaxLoad statistics.
 *
 * @returns Number of pages.
 * @param   pThis       The dynamic mapping cache instance.
 * @param   pcMinPages  The minimal size in pages.
 */
static uint32_t pgmR0DynMapCalcNewSize(PPGMR0DYNMAP pThis, uint32_t *pcMinPages)
{
    Assert(pThis->cPages <= PGMR0DYNMAP_MAX_PAGES);

    /* cCpus * PGMR0DYNMAP_PAGES_PER_CPU (/2). */
    RTCPUID     cCpus     = RTMpGetCount();
    AssertReturn(cCpus > 0 && cCpus <= RTCPUSET_MAX_CPUS, 0);
    uint32_t    cPages    = cCpus * PGMR0DYNMAP_PAGES_PER_CPU;
    uint32_t    cMinPages = cCpus * (PGMR0DYNMAP_PAGES_PER_CPU / 2);

    /* adjust against cMaxLoad. */
    AssertMsg(pThis->cMaxLoad <= PGMR0DYNMAP_MAX_PAGES, ("%#x\n", pThis->cMaxLoad));
    if (pThis->cMaxLoad > PGMR0DYNMAP_MAX_PAGES)
        pThis->cMaxLoad = 0;

    while (pThis->cMaxLoad > PGMR0DYNMAP_CALC_OVERLOAD(cPages))
        cPages += PGMR0DYNMAP_PAGES_PER_CPU;

    if (pThis->cMaxLoad > cMinPages)
        cMinPages = pThis->cMaxLoad;

    /* adjust against max and current size. */
    if (cPages < pThis->cPages)
        cPages = pThis->cPages;
    if (cPages > PGMR0DYNMAP_MAX_PAGES)
        cPages = PGMR0DYNMAP_MAX_PAGES;

    if (cMinPages < pThis->cPages)
        cMinPages = pThis->cPages;
    if (cMinPages > PGMR0DYNMAP_MAX_PAGES)
        cMinPages = PGMR0DYNMAP_MAX_PAGES;

    Assert(cMinPages);
    *pcMinPages = cMinPages;
    return cPages;
}


/**
 * Initializes the paging level data.
 *
 * @param   pThis       The dynamic mapping cache instance.
 * @param   pPgLvl      The paging level data.
 */
void pgmR0DynMapPagingArrayInit(PPGMR0DYNMAP pThis, PPGMR0DYNMAPPGLVL pPgLvl)
{
    RTCCUINTREG     cr4 = ASMGetCR4();
    switch (pThis->enmPgMode)
    {
        case SUPPAGINGMODE_32_BIT:
        case SUPPAGINGMODE_32_BIT_GLOBAL:
            pPgLvl->cLevels = 2;
            pPgLvl->a[0].fPhysMask = X86_CR3_PAGE_MASK;
            pPgLvl->a[0].fAndMask  = X86_PDE_P | X86_PDE_RW | (cr4 & X86_CR4_PSE ? X86_PDE_PS : 0);
            pPgLvl->a[0].fResMask  = X86_PDE_P | X86_PDE_RW;
            pPgLvl->a[0].fPtrMask  = X86_PD_MASK;
            pPgLvl->a[0].fPtrShift = X86_PD_SHIFT;

            pPgLvl->a[1].fPhysMask = X86_PDE_PG_MASK;
            pPgLvl->a[1].fAndMask  = X86_PTE_P | X86_PTE_RW;
            pPgLvl->a[1].fResMask  = X86_PTE_P | X86_PTE_RW;
            pPgLvl->a[1].fPtrMask  = X86_PT_MASK;
            pPgLvl->a[1].fPtrShift = X86_PT_SHIFT;
            break;

        case SUPPAGINGMODE_PAE:
        case SUPPAGINGMODE_PAE_GLOBAL:
        case SUPPAGINGMODE_PAE_NX:
        case SUPPAGINGMODE_PAE_GLOBAL_NX:
            pPgLvl->cLevels = 3;
            pPgLvl->a[0].fPhysMask = X86_CR3_PAE_PAGE_MASK;
            pPgLvl->a[0].fPtrMask  = X86_PDPT_MASK_PAE;
            pPgLvl->a[0].fPtrShift = X86_PDPT_SHIFT;
            pPgLvl->a[0].fAndMask  = X86_PDPE_P;
            pPgLvl->a[0].fResMask  = X86_PDPE_P;

            pPgLvl->a[1].fPhysMask = X86_PDPE_PG_MASK;
            pPgLvl->a[1].fPtrMask  = X86_PD_PAE_MASK;
            pPgLvl->a[1].fPtrShift = X86_PD_PAE_SHIFT;
            pPgLvl->a[1].fAndMask  = X86_PDE_P | X86_PDE_RW | (cr4 & X86_CR4_PSE ? X86_PDE_PS : 0);
            pPgLvl->a[1].fResMask  = X86_PDE_P | X86_PDE_RW;

            pPgLvl->a[2].fPhysMask = X86_PDE_PAE_PG_MASK;
            pPgLvl->a[2].fPtrMask  = X86_PT_PAE_MASK;
            pPgLvl->a[2].fPtrShift = X86_PT_PAE_SHIFT;
            pPgLvl->a[2].fAndMask  = X86_PTE_P | X86_PTE_RW;
            pPgLvl->a[2].fResMask  = X86_PTE_P | X86_PTE_RW;
            break;

        case SUPPAGINGMODE_AMD64:
        case SUPPAGINGMODE_AMD64_GLOBAL:
        case SUPPAGINGMODE_AMD64_NX:
        case SUPPAGINGMODE_AMD64_GLOBAL_NX:
            pPgLvl->cLevels = 4;
            pPgLvl->a[0].fPhysMask = X86_CR3_AMD64_PAGE_MASK;
            pPgLvl->a[0].fPtrShift = X86_PML4_SHIFT;
            pPgLvl->a[0].fPtrMask  = X86_PML4_MASK;
            pPgLvl->a[0].fAndMask  = X86_PML4E_P | X86_PML4E_RW;
            pPgLvl->a[0].fResMask  = X86_PML4E_P | X86_PML4E_RW;

            pPgLvl->a[1].fPhysMask = X86_PML4E_PG_MASK;
            pPgLvl->a[1].fPtrShift = X86_PDPT_SHIFT;
            pPgLvl->a[1].fPtrMask  = X86_PDPT_MASK_AMD64;
            pPgLvl->a[1].fAndMask  = X86_PDPE_P | X86_PDPE_RW /** @todo check for X86_PDPT_PS support. */;
            pPgLvl->a[1].fResMask  = X86_PDPE_P | X86_PDPE_RW;

            pPgLvl->a[2].fPhysMask = X86_PDPE_PG_MASK;
            pPgLvl->a[2].fPtrShift = X86_PD_PAE_SHIFT;
            pPgLvl->a[2].fPtrMask  = X86_PD_PAE_MASK;
            pPgLvl->a[2].fAndMask  = X86_PDE_P | X86_PDE_RW | (cr4 & X86_CR4_PSE ? X86_PDE_PS : 0);
            pPgLvl->a[2].fResMask  = X86_PDE_P | X86_PDE_RW;

            pPgLvl->a[3].fPhysMask = X86_PDE_PAE_PG_MASK;
            pPgLvl->a[3].fPtrShift = X86_PT_PAE_SHIFT;
            pPgLvl->a[3].fPtrMask  = X86_PT_PAE_MASK;
            pPgLvl->a[3].fAndMask  = X86_PTE_P | X86_PTE_RW;
            pPgLvl->a[3].fResMask  = X86_PTE_P | X86_PTE_RW;
            break;

        default:
            AssertFailed();
            pPgLvl->cLevels = 0;
            break;
    }

    for (uint32_t i = 0; i < 4; i++) /* ASSUMING array size. */
    {
        pPgLvl->a[i].HCPhys = NIL_RTHCPHYS;
        pPgLvl->a[i].hMapObj = NIL_RTR0MEMOBJ;
        pPgLvl->a[i].hMemObj = NIL_RTR0MEMOBJ;
        pPgLvl->a[i].u.pv = NULL;
    }
}


/**
 * Maps a PTE.
 *
 * This will update the segment structure when new PTs are mapped.
 *
 * It also assumes that we (for paranoid reasons) wish to establish a mapping
 * chain from CR3 to the PT that all corresponds to the processor we're
 * currently running on, and go about this by running with interrupts disabled
 * and restarting from CR3 for every change.
 *
 * @returns VBox status code, VINF_TRY_AGAIN if we changed any mappings and had
 *          to re-enable interrupts.
 * @param   pThis       The dynamic mapping cache instance.
 * @param   pPgLvl      The paging level structure.
 * @param   pvPage      The page.
 * @param   pSeg        The segment.
 * @param   cMaxPTs     The max number of PTs expected in the segment.
 * @param   ppvPTE      Where to store the PTE address.
 */
static int pgmR0DynMapPagingArrayMapPte(PPGMR0DYNMAP pThis, PPGMR0DYNMAPPGLVL pPgLvl, void *pvPage,
                                        PPGMR0DYNMAPSEG pSeg, uint32_t cMaxPTs, void **ppvPTE)
{
    Assert(!(ASMGetFlags() & X86_EFL_IF));
    void           *pvEntry = NULL;
    X86PGPAEUINT    uEntry = ASMGetCR3();
    for (uint32_t i = 0; i < pPgLvl->cLevels; i++)
    {
        RTHCPHYS HCPhys = uEntry & pPgLvl->a[i].fPhysMask;
        if (pPgLvl->a[i].HCPhys != HCPhys)
        {
            /*
             * Need to remap this level.
             * The final level, the PT, will not be freed since that is what it's all about.
             */
            ASMIntEnable();
            if (i + 1 == pPgLvl->cLevels)
                AssertReturn(pSeg->cPTs < cMaxPTs, VERR_INTERNAL_ERROR);
            else
            {
                int rc2 = RTR0MemObjFree(pPgLvl->a[i].hMemObj, true /* fFreeMappings */); AssertRC(rc2);
                pPgLvl->a[i].hMemObj = pPgLvl->a[i].hMapObj = NIL_RTR0MEMOBJ;
            }

            int rc = RTR0MemObjEnterPhys(&pPgLvl->a[i].hMemObj, HCPhys, PAGE_SIZE);
            if (RT_SUCCESS(rc))
            {
                rc = RTR0MemObjMapKernel(&pPgLvl->a[i].hMapObj, pPgLvl->a[i].hMemObj,
                                         (void *)-1 /* pvFixed */, 0 /* cbAlignment */,
                                         RTMEM_PROT_WRITE | RTMEM_PROT_READ);
                if (RT_SUCCESS(rc))
                {
                    pPgLvl->a[i].u.pv   = RTR0MemObjAddress(pPgLvl->a[i].hMapObj);
                    AssertMsg(((uintptr_t)pPgLvl->a[i].u.pv & ~(uintptr_t)PAGE_OFFSET_MASK), ("%p\n", pPgLvl->a[i].u.pv));
                    pPgLvl->a[i].HCPhys = HCPhys;
                    if (i + 1 == pPgLvl->cLevels)
                        pSeg->ahMemObjPTs[pSeg->cPTs++] = pPgLvl->a[i].hMemObj;
                    ASMIntDisable();
                    return VINF_TRY_AGAIN;
                }

                pPgLvl->a[i].hMapObj = NIL_RTR0MEMOBJ;
            }
            else
                pPgLvl->a[i].hMemObj = NIL_RTR0MEMOBJ;
            pPgLvl->a[i].HCPhys = NIL_RTHCPHYS;
            return rc;
        }

        /*
         * The next level.
         */
        uint32_t iEntry = ((uint64_t)(uintptr_t)pvPage >> pPgLvl->a[i].fPtrShift) & pPgLvl->a[i].fPtrMask;
        if (pThis->fLegacyMode)
        {
            pvEntry = &pPgLvl->a[i].u.paLegacy[iEntry];
            uEntry  = pPgLvl->a[i].u.paLegacy[iEntry];
        }
        else
        {
            pvEntry = &pPgLvl->a[i].u.paPae[iEntry];
            uEntry  = pPgLvl->a[i].u.paPae[iEntry];
        }

        if ((uEntry & pPgLvl->a[i].fAndMask) != pPgLvl->a[i].fResMask)
        {
            LogRel(("PGMR0DynMap: internal error - iPgLvl=%u cLevels=%u uEntry=%#llx fAnd=%#llx fRes=%#llx got=%#llx\n"
                    "PGMR0DynMap: pv=%p pvPage=%p iEntry=%#x fLegacyMode=%RTbool\n",
                    i, pPgLvl->cLevels, uEntry, pPgLvl->a[i].fAndMask, pPgLvl->a[i].fResMask, uEntry & pPgLvl->a[i].fAndMask,
                    pPgLvl->a[i].u.pv, pvPage, iEntry, pThis->fLegacyMode));
            return VERR_INTERNAL_ERROR;
        }
        Log(("#%d: iEntry=%d uEntry=%#llx pvEntry=%p HCPhys=%RHp \n", i, iEntry, uEntry, pvEntry, pPgLvl->a[i].HCPhys));
    }

    /* made it thru without needing to remap anything. */
    *ppvPTE = pvEntry;
    return VINF_SUCCESS;
}


/**
 * Adds a new segment of the specified size.
 *
 * @returns VBox status code.
 * @param   pThis       The dynamic mapping cache instance.
 * @param   cPages      The size of the new segment, give as a page count.
 */
static int pgmR0DynMapAddSeg(PPGMR0DYNMAP pThis, uint32_t cPages)
{
    int rc2;
    AssertReturn(ASMGetFlags() & X86_EFL_IF, VERR_PREEMPT_DISABLED);

    /*
     * Do the array rellocation first.
     * (The pages array has to be replaced behind the spinlock of course.)
     */
    void *pvSavedPTEs = RTMemRealloc(pThis->pvSavedPTEs, (pThis->fLegacyMode ? sizeof(X86PGUINT) : sizeof(X86PGPAEUINT)) * (pThis->cPages + cPages));
    if (!pvSavedPTEs)
        return VERR_NO_MEMORY;
    pThis->pvSavedPTEs = pvSavedPTEs;

    void *pvPages = RTMemAllocZ(sizeof(pThis->paPages[0]) * (pThis->cPages + cPages));
    if (!pvPages)
    {
        pvSavedPTEs = RTMemRealloc(pThis->pvSavedPTEs, (pThis->fLegacyMode ? sizeof(X86PGUINT) : sizeof(X86PGPAEUINT)) * pThis->cPages);
        if (pvSavedPTEs)
            pThis->pvSavedPTEs = pvSavedPTEs;
        return VERR_NO_MEMORY;
    }

    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquire(pThis->hSpinlock, &Tmp);

    memcpy(pvPages, pThis->paPages, sizeof(pThis->paPages[0]) * pThis->cPages);
    void *pvToFree = pThis->paPages;
    pThis->paPages = (PPGMR0DYNMAPENTRY)pvPages;

    RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    RTMemFree(pvToFree);

    /*
     * Allocate the segment structure and pages of memory, then touch all the pages (paranoia).
     */
    uint32_t cMaxPTs = cPages / (pThis->fLegacyMode ? X86_PG_ENTRIES : X86_PG_PAE_ENTRIES) + 2;
    PPGMR0DYNMAPSEG pSeg = (PPGMR0DYNMAPSEG)RTMemAllocZ(RT_UOFFSETOF(PGMR0DYNMAPSEG, ahMemObjPTs[cMaxPTs]));
    if (!pSeg)
        return VERR_NO_MEMORY;
    pSeg->pNext  = NULL;
    pSeg->cPages = cPages;
    pSeg->iPage  = pThis->cPages;
    pSeg->cPTs   = 0;
    int rc = RTR0MemObjAllocPage(&pSeg->hMemObj, cPages << PAGE_SHIFT, false);
    if (RT_SUCCESS(rc))
    {
        uint8_t            *pbPage = (uint8_t *)RTR0MemObjAddress(pSeg->hMemObj);
        AssertMsg(VALID_PTR(pbPage) && !((uintptr_t)pbPage & PAGE_OFFSET_MASK), ("%p\n", pbPage));
        memset(pbPage, 0xfe, cPages << PAGE_SHIFT);

        /*
         * Walk thru the pages and set them up with a mapping of their PTE and everything.
         */
        ASMIntDisable();
        PGMR0DYNMAPPGLVL    PgLvl;
        pgmR0DynMapPagingArrayInit(pThis, &PgLvl);
        uint32_t            iEndPage = pThis->cPages + cPages;
        for (uint32_t iPage = pThis->cPages;
             iPage < iEndPage;
             iPage++, pbPage += PAGE_SIZE)
        {
            /* Initialize the page data. */
            pThis->paPages[iPage].HCPhys = NIL_RTHCPHYS;
            pThis->paPages[iPage].pvPage = pbPage;
            pThis->paPages[iPage].cRefs  = 0;
            pThis->paPages[iPage].uPte.pPae = 0;
            RTCpuSetFill(&pThis->paPages[iPage].PendingSet);

            /* Map its page table, retry until we've got a clean run (paranoia). */
            do
                rc = pgmR0DynMapPagingArrayMapPte(pThis, &PgLvl, pbPage, pSeg, cMaxPTs,
                                                  &pThis->paPages[iPage].uPte.pv);
            while (rc == VINF_TRY_AGAIN);
            if (RT_FAILURE(rc))
                break;

            /* Save the PTE. */
            if (pThis->fLegacyMode)
                ((PX86PGUINT)pThis->pvSavedPTEs)[iPage]    = pThis->paPages[iPage].uPte.pLegacy->u;
            else
                ((PX86PGPAEUINT)pThis->pvSavedPTEs)[iPage] = pThis->paPages[iPage].uPte.pPae->u;

#ifdef VBOX_STRICT
            /* Check that we've got the right entry. */
            RTHCPHYS HCPhysPage = RTR0MemObjGetPagePhysAddr(pSeg->hMemObj, iPage - pSeg->iPage);
            RTHCPHYS HCPhysPte  = pThis->fLegacyMode
                                ? pThis->paPages[iPage].uPte.pLegacy->u & X86_PTE_PG_MASK
                                : pThis->paPages[iPage].uPte.pPae->u    & X86_PTE_PAE_PG_MASK;
            if (HCPhysPage != HCPhysPte)
            {
                LogRel(("pgmR0DynMapAddSeg: internal error - page #%u HCPhysPage=%RHp HCPhysPte=%RHp pbPage=%p pvPte=%p\n",
                        iPage - pSeg->iPage, HCPhysPage, HCPhysPte, pbPage, pThis->paPages[iPage].uPte.pv));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
#endif
        } /* for each page */
        ASMIntEnable();

        /* cleanup non-PT mappings */
        for (uint32_t i = 0; i < PgLvl.cLevels - 1; i++)
            RTR0MemObjFree(PgLvl.a[i].hMemObj, true /* fFreeMappings */);

        if (RT_SUCCESS(rc))
        {
            /** @todo setup guard pages here later (strict builds should leave every
             *        second page and the start/end pages not present).  */

            /*
             * Commit it by adding the segment to the list and updating the page count.
             */
            pSeg->pNext = pThis->pSegHead;
            pThis->pSegHead = pSeg;
            pThis->cPages += cPages;
            return VINF_SUCCESS;
        }

        /*
         * Bail out.
         */
        while (pSeg->cPTs-- > 0)
        {
            rc2 = RTR0MemObjFree(pSeg->ahMemObjPTs[pSeg->cPTs], true /* fFreeMappings */);
            AssertRC(rc2);
            pSeg->ahMemObjPTs[pSeg->cPTs] = NIL_RTR0MEMOBJ;
        }

        rc2 = RTR0MemObjFree(pSeg->hMemObj, true /* fFreeMappings */);
        AssertRC(rc2);
        pSeg->hMemObj = NIL_RTR0MEMOBJ;
    }
    RTMemFree(pSeg);

    /* Don't bother resizing the arrays, but free them if we're the only user. */
    if (!pThis->cPages)
    {
        RTMemFree(pThis->paPages);
        pThis->paPages = NULL;
        RTMemFree(pThis->pvSavedPTEs);
        pThis->pvSavedPTEs = NULL;
    }
    return rc;
}


/**
 * Called by PGMR0DynMapInitVM under the init lock.
 *
 * @returns VBox status code.
 * @param   pThis       The dynamic mapping cache instance.
 */
static int pgmR0DynMapSetup(PPGMR0DYNMAP pThis)
{
    /*
     * Calc the size and add a segment of that size.
     */
    uint32_t cMinPages;
    uint32_t cPages = pgmR0DynMapCalcNewSize(pThis, &cMinPages);
    AssertReturn(cPages, VERR_INTERNAL_ERROR);
    int rc = pgmR0DynMapAddSeg(pThis, cPages);
    if (rc == VERR_NO_MEMORY)
    {
        /*
         * Try adding smaller segments.
         */
        do
            rc = pgmR0DynMapAddSeg(pThis, PGMR0DYNMAP_SMALL_SEG_PAGES);
        while (RT_SUCCESS(rc) && pThis->cPages < cPages);
        if (rc == VERR_NO_MEMORY && pThis->cPages >= cMinPages)
            rc = VINF_SUCCESS;
        if (rc == VERR_NO_MEMORY)
        {
            if (pThis->cPages)
                pgmR0DynMapTearDown(pThis);
            rc = VERR_PGM_DYNMAP_SETUP_ERROR;
        }
    }
    Assert(ASMGetFlags() & X86_EFL_IF);
    return rc;
}


/**
 * Called by PGMR0DynMapInitVM under the init lock.
 *
 * @returns VBox status code.
 * @param   pThis       The dynamic mapping cache instance.
 */
static int pgmR0DynMapExpand(PPGMR0DYNMAP pThis)
{
    /*
     * Calc the new target size and add a segment of the appropriate size.
     */
    uint32_t cMinPages;
    uint32_t cPages = pgmR0DynMapCalcNewSize(pThis, &cMinPages);
    AssertReturn(cPages, VERR_INTERNAL_ERROR);
    if (pThis->cPages >= cPages)
        return VINF_SUCCESS;

    uint32_t cAdd = cPages - pThis->cPages;
    int rc = pgmR0DynMapAddSeg(pThis, cAdd);
    if (rc == VERR_NO_MEMORY)
    {
        /*
         * Try adding smaller segments.
         */
        do
            rc = pgmR0DynMapAddSeg(pThis, PGMR0DYNMAP_SMALL_SEG_PAGES);
        while (RT_SUCCESS(rc) && pThis->cPages < cPages);
        if (rc == VERR_NO_MEMORY && pThis->cPages >= cMinPages)
            rc = VINF_SUCCESS;
        if (rc == VERR_NO_MEMORY)
            rc = VERR_PGM_DYNMAP_EXPAND_ERROR;
    }
    Assert(ASMGetFlags() & X86_EFL_IF);
    return rc;
}


/**
 * Shoots down the TLBs for all the cache pages, pgmR0DynMapTearDown helper.
 *
 * @param   idCpu           The current CPU.
 * @param   pvUser1         The dynamic mapping cache instance.
 * @param   pvUser2         Unused, NULL.
 */
static DECLCALLBACK(void) pgmR0DynMapShootDownTlbs(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    Assert(!pvUser2);
    PPGMR0DYNMAP        pThis   = (PPGMR0DYNMAP)pvUser1;
    Assert(pThis == g_pPGMR0DynMap);
    PPGMR0DYNMAPENTRY   paPages = pThis->paPages;
    uint32_t            iPage   = pThis->cPages;
    while (iPage-- > 0)
        ASMInvalidatePage(paPages[iPage].pvPage);
}


/**
 * Called by PGMR0DynMapTermVM under the init lock.
 *
 * @returns VBox status code.
 * @param   pThis       The dynamic mapping cache instance.
 */
static void pgmR0DynMapTearDown(PPGMR0DYNMAP pThis)
{
    /*
     * Restore the original page table entries
     */
    PPGMR0DYNMAPENTRY   paPages = pThis->paPages;
    uint32_t            iPage   = pThis->cPages;
    if (pThis->fLegacyMode)
    {
        X86PGUINT const    *paSavedPTEs = (X86PGUINT const *)pThis->pvSavedPTEs;
        while (iPage-- > 0)
        {
            X86PGUINT       uOld  = paPages[iPage].uPte.pLegacy->u;
            X86PGUINT       uOld2 = uOld; NOREF(uOld2);
            X86PGUINT       uNew  = paSavedPTEs[iPage];
            while (!ASMAtomicCmpXchgExU32(&paPages[iPage].uPte.pLegacy->u, uNew, uOld, &uOld))
                AssertMsgFailed(("uOld=%#x uOld2=%#x uNew=%#x\n", uOld, uOld2, uNew));
        }
    }
    else
    {
        X86PGPAEUINT const *paSavedPTEs = (X86PGPAEUINT const *)pThis->pvSavedPTEs;
        while (iPage-- > 0)
        {
            X86PGPAEUINT    uOld  = paPages[iPage].uPte.pPae->u;
            X86PGPAEUINT    uOld2 = uOld; NOREF(uOld2);
            X86PGPAEUINT    uNew  = paSavedPTEs[iPage];
            while (!ASMAtomicCmpXchgExU64(&paPages[iPage].uPte.pPae->u, uNew, uOld, &uOld))
                AssertMsgFailed(("uOld=%#llx uOld2=%#llx uNew=%#llx\n", uOld, uOld2, uNew));
        }
    }

    /*
     * Shoot down the TLBs on all CPUs before freeing them.
     * If RTMpOnAll fails, make sure the TLBs are invalidated on the current CPU at least.
     */
    int rc = RTMpOnAll(pgmR0DynMapShootDownTlbs, pThis, NULL);
    AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        iPage = pThis->cPages;
        while (iPage-- > 0)
            ASMInvalidatePage(paPages[iPage].pvPage);
    }

    /*
     * Free the segments.
     */
    while (pThis->pSegHead)
    {
        PPGMR0DYNMAPSEG pSeg = pThis->pSegHead;
        pThis->pSegHead = pSeg->pNext;

        uint32_t iPT = pSeg->cPTs;
        while (iPT-- > 0)
        {
            rc = RTR0MemObjFree(pSeg->ahMemObjPTs[iPT], true /* fFreeMappings */); AssertRC(rc);
            pSeg->ahMemObjPTs[iPT] = NIL_RTR0MEMOBJ;
        }
        rc = RTR0MemObjFree(pSeg->hMemObj, true /* fFreeMappings */); AssertRC(rc);
        pSeg->hMemObj   = NIL_RTR0MEMOBJ;
        pSeg->pNext     = NULL;
        pSeg->iPage     = UINT16_MAX;
        pSeg->cPages    = 0;
        pSeg->cPTs      = 0;
        RTMemFree(pSeg);
    }

    /*
     * Free the arrays and restore the initial state.
     * The cLoadMax value is left behind for the next setup.
     */
    RTMemFree(pThis->paPages);
    pThis->paPages = NULL;
    RTMemFree(pThis->pvSavedPTEs);
    pThis->pvSavedPTEs = NULL;
    pThis->cPages = 0;
    pThis->cLoad = 0;
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
    uint32_t const      cPages  = pThis->cPages;
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
    {
        X86PGUINT       uOld  = paPages[iFreePage].uPte.pLegacy->u;
        X86PGUINT       uOld2 = uOld; NOREF(uOld2);
        X86PGUINT       uNew  = (uOld & X86_PTE_G | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT)
                              | X86_PTE_P | X86_PTE_RW | X86_PTE_A | X86_PTE_D
                              | (HCPhys & X86_PTE_PG_MASK);
        while (!ASMAtomicCmpXchgExU32(&paPages[iFreePage].uPte.pLegacy->u, uNew, uOld, &uOld))
            AssertMsgFailed(("uOld=%#x uOld2=%#x uNew=%#x\n", uOld, uOld2, uNew));
    }
    else
    {
        X86PGPAEUINT    uOld  = paPages[iFreePage].uPte.pPae->u;
        X86PGPAEUINT    uOld2 = uOld; NOREF(uOld2);
        X86PGPAEUINT    uNew  = (uOld & X86_PTE_G | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT)
                              | X86_PTE_P | X86_PTE_RW | X86_PTE_A | X86_PTE_D
                              | (HCPhys & X86_PTE_PAE_PG_MASK);
        while (!ASMAtomicCmpXchgExU64(&paPages[iFreePage].uPte.pPae->u, uNew, uOld, &uOld))
            AssertMsgFailed(("uOld=%#llx uOld2=%#llx uNew=%#llx\n", uOld, uOld2, uNew));
        Log6(("pgmR0DynMapPageSlow: #%x - %RHp %p %#llx\n", iFreePage, HCPhys, paPages[iFreePage].pvPage, uNew));
    }
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
    uint32_t const      cPages  = pThis->cPages;
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
    int32_t cRefs = ASMAtomicIncS32(&paPages[iPage].cRefs);
    if (cRefs == 1)
    {
        pThis->cLoad++;
        if (pThis->cLoad > pThis->cMaxLoad)
            pThis->cMaxLoad = pThis->cLoad;
        Assert(pThis->cLoad <= pThis->cPages);
    }
    else if (RT_UNLIKELY(cRefs <= 0))
    {
        ASMAtomicDecS32(&paPages[iPage].cRefs);
        RTSpinlockRelease(pThis->hSpinlock, &Tmp);
        AssertLogRelMsgFailedReturn(("cRefs=%d iPage=%p HCPhys=%RHp\n", cRefs, iPage, HCPhys), NULL);
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
    Log6(("PGMDynMapStartAutoSet\n"));
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
    uint32_t    i = pSet->cEntries;
    AssertMsg(i <= RT_ELEMENTS(pSet->aEntries), ("%#x (%u)\n", i, i));
    pSet->cEntries = PGMMAPSET_CLOSED;

    /* release any pages we're referencing. */
    if (i != 0 && RT_LIKELY(i <= RT_ELEMENTS(pSet->aEntries)))
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

            pSet->aEntries[i].iPage = UINT16_MAX;
            pSet->aEntries[i].cRefs = 0;
        }

        Assert(pThis->cLoad <= pThis->cPages);
        RTSpinlockRelease(pThis->hSpinlock, &Tmp);
    }
    Log6(("PGMDynMapReleaseAutoSet\n"));
}


/**
 * Migrates the automatic mapping set of the current vCPU if it's active and
 * necessary.
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
    uint32_t    i = pSet->cEntries;
    if (i != PGMMAPSET_CLOSED)
    {
        AssertMsg(i <= RT_ELEMENTS(pSet->aEntries), ("%#x (%u)\n", i, i));
        if (i != 0 && RT_LIKELY(i <= RT_ELEMENTS(pSet->aEntries)))
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
        Log6(("PGMDynMapMigrateAutoSet\n"));
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
            else if ((uint32_t)pSet->aEntries[i].cRefs + (uint32_t)pSet->aEntries[j].cRefs < UINT16_MAX)
            {
                /* merge j into i removing j. */
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
            else
            {
                /* migrate the max number of refs from j into i and quit the inner loop. */
                uint32_t cMigrate = UINT16_MAX - 1 - pSet->aEntries[i].cRefs;
                Assert(pSet->aEntries[j].cRefs > cMigrate);
                pSet->aEntries[j].cRefs -= cMigrate;
                pSet->aEntries[i].cRefs = UINT16_MAX - 1;
                break;
            }
        }
    }
}


/* documented elsewhere - a bit of a mess. */
VMMDECL(int) PGMDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    /*
     * Validate state.
     */
    AssertPtr(ppv);
    *ppv = NULL;
    AssertMsgReturn(pVM->pgm.s.pvR0DynMapUsed == g_pPGMR0DynMap,
                    ("%p != %p\n", pVM->pgm.s.pvR0DynMapUsed, g_pPGMR0DynMap),
                    VERR_ACCESS_DENIED);
    AssertMsg(!(HCPhys & PAGE_OFFSET_MASK), ("HCPhys=%RHp\n", HCPhys));
    PVMCPU          pVCpu   = VMMGetCpu(pVM);
    PPGMMAPSET      pSet    = &pVCpu->pgm.s.AutoSet;
    AssertPtrReturn(pVCpu, VERR_INTERNAL_ERROR);
    AssertMsgReturn(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries),
                    ("%#x (%u)\n", pSet->cEntries, pSet->cEntries), VERR_WRONG_ORDER);

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
    *ppv = pvPage;

    /*
     * Add the page to the auto reference set.
     * If it's less than half full, don't bother looking for duplicates.
     */
    if (pSet->cEntries < RT_ELEMENTS(pSet->aEntries) / 2)
    {
        pSet->aEntries[pSet->cEntries].cRefs = 1;
        pSet->aEntries[pSet->cEntries].iPage = iPage;
        pSet->cEntries++;
    }
    else
    {
        Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));
        int32_t     i = pSet->cEntries;
        while (i-- > 0)
            if (    pSet->aEntries[i].iPage == iPage
                &&  pSet->aEntries[i].cRefs < UINT16_MAX - 1)
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
                pSet->cEntries++;
            }
            else
            {
                /* We're screwed. */
                pgmR0DynMapReleasePage(g_pPGMR0DynMap, iPage, 1);

                static uint32_t s_cBitched = 0;
                if (++s_cBitched < 10)
                    LogRel(("PGMDynMapHCPage: set is full!\n"));
                *ppv = NULL;
                return VERR_PGM_DYNMAP_FULL_SET;
            }
        }
    }

    return VINF_SUCCESS;
}


#ifdef DEBUG
/** For pgmR0DynMapTest3PerCpu. */
typedef struct PGMR0DYNMAPTEST
{
    uint32_t            u32Expect;
    uint32_t           *pu32;
    uint32_t volatile   cFailures;
} PGMR0DYNMAPTEST;
typedef PGMR0DYNMAPTEST *PPGMR0DYNMAPTEST;

/**
 * Checks that the content of the page is the same on all CPUs, i.e. that there
 * are no CPU specfic PTs or similar nasty stuff involved.
 *
 * @param   idCpu           The current CPU.
 * @param   pvUser1         Pointer a PGMR0DYNMAPTEST structure.
 * @param   pvUser2         Unused, ignored.
 */
static DECLCALLBACK(void) pgmR0DynMapTest3PerCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PPGMR0DYNMAPTEST    pTest = (PPGMR0DYNMAPTEST)pvUser1;
    ASMInvalidatePage(pTest->pu32);
    if (*pTest->pu32 != pTest->u32Expect)
        ASMAtomicIncU32(&pTest->cFailures);
    NOREF(pvUser2); NOREF(idCpu);
}


/**
 * Performs some basic tests in debug builds.
 */
static int pgmR0DynMapTest(PVM pVM)
{
    LogRel(("pgmR0DynMapTest: ****** START ******\n"));
    PPGMR0DYNMAP    pThis = g_pPGMR0DynMap;
    PPGMMAPSET      pSet  = &pVM->aCpus[0].pgm.s.AutoSet;
    uint32_t        i;
    void           *pvR0DynMapUsedSaved = pVM->pgm.s.pvR0DynMapUsed;
    pVM->pgm.s.pvR0DynMapUsed = pThis;

    /*
     * Simple test, map CR3 twice and check that we're getting the
     * same mapping address back.
     */
    LogRel(("Test #1\n"));
    ASMIntDisable();
    PGMDynMapStartAutoSet(&pVM->aCpus[0]);

    uint64_t cr3 = ASMGetCR3() & ~(uint64_t)PAGE_OFFSET_MASK;
    void    *pv  = (void *)(intptr_t)-1;
    void    *pv2 = (void *)(intptr_t)-2;
    int      rc  = PGMDynMapHCPage(pVM, cr3, &pv);
    int      rc2 = PGMDynMapHCPage(pVM, cr3, &pv2);
    ASMIntEnable();
    if (    RT_SUCCESS(rc2)
        &&  RT_SUCCESS(rc)
        &&  pv == pv2)
    {
        LogRel(("Load=%u/%u/%u Set=%u/%u\n", pThis->cLoad, pThis->cMaxLoad, pThis->cPages, pSet->cEntries, RT_ELEMENTS(pSet->aEntries)));

        /*
         * Check that the simple set overflow code works by filling it
         * with more CR3 mappings.
         */
        LogRel(("Test #2\n"));
        ASMIntDisable();
        for (i = 0 ; i < UINT16_MAX*2 + RT_ELEMENTS(pSet->aEntries) / 2 && RT_SUCCESS(rc) && pv2 == pv; i++)
        {
            pv2 = (void *)(intptr_t)-4;
            rc = PGMDynMapHCPage(pVM, cr3, &pv2);
        }
        ASMIntEnable();
        if (RT_FAILURE(rc) || pv != pv2)
        {
            LogRel(("failed(%d): rc=%Rrc; pv=%p pv2=%p i=%p\n", __LINE__, rc, pv, pv2, i));
            if (RT_SUCCESS(rc)) rc = VERR_INTERNAL_ERROR;
        }
        else if (pSet->cEntries != RT_ELEMENTS(pSet->aEntries) / 2)
        {
            LogRel(("failed(%d): cEntries=%d expected %d\n", __LINE__, pSet->cEntries, RT_ELEMENTS(pSet->aEntries) / 2));
            rc = VERR_INTERNAL_ERROR;
        }
        else if (   pSet->aEntries[(RT_ELEMENTS(pSet->aEntries) / 2) - 1].cRefs != UINT16_MAX - 1
                 || pSet->aEntries[(RT_ELEMENTS(pSet->aEntries) / 2) - 2].cRefs != UINT16_MAX - 1
                 || pSet->aEntries[(RT_ELEMENTS(pSet->aEntries) / 2) - 3].cRefs != 2+2+3
                 || pSet->aEntries[(RT_ELEMENTS(pSet->aEntries) / 2) - 4].cRefs != 1)
        {
            LogRel(("failed(%d): bad set dist: ", __LINE__));
            for (i = 0; i < pSet->cEntries; i++)
                LogRel(("[%d]=%d, ", i, pSet->aEntries[i].cRefs));
            LogRel(("\n"));
            rc = VERR_INTERNAL_ERROR;
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Trigger an set optimization run (exactly).
             */
            LogRel(("Test #3\n"));
            ASMIntDisable();
            pv2 = NULL;
            for (i = 0 ; i < RT_ELEMENTS(pSet->aEntries) / 2 && RT_SUCCESS(rc) && pv2 != pv; i++)
            {
                pv2 = (void *)(intptr_t)(-5 - i);
                rc = PGMDynMapHCPage(pVM, cr3 + PAGE_SIZE * (i + 5), &pv2);
            }
            ASMIntEnable();
            if (RT_FAILURE(rc) || pv == pv2)
            {
                LogRel(("failed(%d): rc=%Rrc; pv=%p pv2=%p i=%d\n", __LINE__, rc, pv, pv2, i));
                if (RT_SUCCESS(rc)) rc = VERR_INTERNAL_ERROR;
            }
            else if (pSet->cEntries != RT_ELEMENTS(pSet->aEntries))
            {
                LogRel(("failed(%d): cEntries=%d expected %d\n", __LINE__, pSet->cEntries, RT_ELEMENTS(pSet->aEntries)));
                rc = VERR_INTERNAL_ERROR;
            }
            LogRel(("Load=%u/%u/%u Set=%u/%u\n", pThis->cLoad, pThis->cMaxLoad, pThis->cPages, pSet->cEntries, RT_ELEMENTS(pSet->aEntries)));
            if (RT_SUCCESS(rc))
            {
                /*
                 * Trigger an overflow error.
                 */
                LogRel(("Test #4\n"));
                ASMIntDisable();
                for (i = 0 ; i < RT_ELEMENTS(pSet->aEntries) / 2 - 3 + 1 && RT_SUCCESS(rc) && pv2 != pv; i++)
                    rc = PGMDynMapHCPage(pVM, cr3 + PAGE_SIZE * -(i + 5), &pv2);
                ASMIntEnable();
                if (rc == VERR_PGM_DYNMAP_FULL_SET)
                {
                    rc = VINF_SUCCESS;

                    /* flush the set. */
                    ASMIntDisable();
                    PGMDynMapMigrateAutoSet(&pVM->aCpus[0]);
                    PGMDynMapReleaseAutoSet(&pVM->aCpus[0]);
                    PGMDynMapStartAutoSet(&pVM->aCpus[0]);
                    ASMIntEnable();
                }
                else
                {
                    LogRel(("failed(%d): rc=%Rrc, wanted %d ; pv2=%p Set=%u/%u\n", __LINE__,
                            rc, VERR_PGM_DYNMAP_FULL_SET, pv2, pSet->cEntries, RT_ELEMENTS(pSet->aEntries)));
                    if (RT_SUCCESS(rc)) rc = VERR_INTERNAL_ERROR;
                }
            }
        }
    }
    else
    {
        LogRel(("failed(%d): rc=%Rrc rc2=%Rrc; pv=%p pv2=%p\n", __LINE__, rc, rc2, pv, pv2));
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    /*
     * Check that everyone sees the same stuff.
     */
    if (RT_SUCCESS(rc))
    {
        LogRel(("Test #5\n"));
        ASMIntDisable();
        RTHCPHYS  HCPhysPT = RTR0MemObjGetPagePhysAddr(pThis->pSegHead->ahMemObjPTs[0], 0);
        rc  = PGMDynMapHCPage(pVM, HCPhysPT, &pv);
        if (RT_SUCCESS(rc))
        {
            PGMR0DYNMAPTEST Test;
            uint32_t       *pu32Real = &pThis->paPages[pThis->pSegHead->iPage].uPte.pLegacy->u;
            Test.pu32       = (uint32_t *)((uintptr_t)pv | ((uintptr_t)pu32Real & PAGE_OFFSET_MASK));
            Test.u32Expect  = *pu32Real;
            ASMAtomicWriteU32(&Test.cFailures, 0);
            ASMIntEnable();

            rc = RTMpOnAll(pgmR0DynMapTest3PerCpu, &Test, NULL);
            if (RT_FAILURE(rc))
                LogRel(("failed(%d): RTMpOnAll rc=%Rrc\n", __LINE__, rc));
            else if (Test.cFailures)
            {
                LogRel(("failed(%d): cFailures=%d pu32Real=%p pu32=%p u32Expect=%#x *pu32=%#x\n", __LINE__,
                        Test.cFailures, pu32Real, Test.pu32, Test.u32Expect, *Test.pu32));
                rc = VERR_INTERNAL_ERROR;
            }
            else
                LogRel(("pu32Real=%p pu32=%p u32Expect=%#x *pu32=%#x\n",
                        pu32Real, Test.pu32, Test.u32Expect, *Test.pu32));
        }
        else
        {
            ASMIntEnable();
            LogRel(("failed(%d): rc=%Rrc\n", rc));
        }
    }

    /*
     * Clean up.
     */
    LogRel(("Cleanup.\n"));
    ASMIntDisable();
    PGMDynMapMigrateAutoSet(&pVM->aCpus[0]);
    PGMDynMapReleaseAutoSet(&pVM->aCpus[0]);
    ASMIntEnable();

    LogRel(("Result: rc=%Rrc Load=%u/%u/%u Set=%#x/%u\n", rc,
            pThis->cLoad, pThis->cMaxLoad, pThis->cPages, pSet->cEntries, RT_ELEMENTS(pSet->aEntries)));
    pVM->pgm.s.pvR0DynMapUsed = pvR0DynMapUsedSaved;
    LogRel(("pgmR0DynMapTest: ****** END ******\n"));
    return rc;
}
#endif /* DEBUG */

