/* $Id$ */
/** @file
 * PGM Shadow Page Pool.
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

/** @page pg_pgm_pool       PGM Shadow Page Pool
 *
 * Motivations:
 *      -# Relationship between shadow page tables and physical guest pages. This
 *         should allow us to skip most of the global flushes now following access
 *         handler changes. The main expense is flushing shadow pages.
 *      -# Limit the pool size if necessary (default is kind of limitless).
 *      -# Allocate shadow pages from RC. We use to only do this in SyncCR3.
 *      -# Required for 64-bit guests.
 *      -# Combining the PD cache and page pool in order to simplify caching.
 *
 *
 * @section sec_pgm_pool_outline    Design Outline
 *
 * The shadow page pool tracks pages used for shadowing paging structures (i.e.
 * page tables, page directory, page directory pointer table and page map
 * level-4). Each page in the pool has an unique identifier. This identifier is
 * used to link a guest physical page to a shadow PT. The identifier is a
 * non-zero value and has a relativly low max value - say 14 bits. This makes it
 * possible to fit it into the upper bits of the of the aHCPhys entries in the
 * ram range.
 *
 * By restricting host physical memory to the first 48 bits (which is the
 * announced physical memory range of the K8L chip (scheduled for 2008)), we
 * can safely use the upper 16 bits for shadow page ID and reference counting.
 *
 * Update: The 48 bit assumption will be lifted with the new physical memory
 * management (PGMPAGE), so we won't have any trouble when someone stuffs 2TB
 * into a box in some years.
 *
 * Now, it's possible for a page to be aliased, i.e. mapped by more than one PT
 * or PD. This is solved by creating a list of physical cross reference extents
 * when ever this happens. Each node in the list (extent) is can contain 3 page
 * pool indexes. The list it self is chained using indexes into the paPhysExt
 * array.
 *
 *
 * @section sec_pgm_pool_life       Life Cycle of a Shadow Page
 *
 * -# The SyncPT function requests a page from the pool.
 *    The request includes the kind of page it is (PT/PD, PAE/legacy), the
 *    address of the page it's shadowing, and more.
 * -# The pool responds to the request by allocating a new page.
 *    When the cache is enabled, it will first check if it's in the cache.
 *    Should the pool be exhausted, one of two things can be done:
 *      -# Flush the whole pool and current CR3.
 *      -# Use the cache to find a page which can be flushed (~age).
 * -# The SyncPT function will sync one or more pages and insert it into the
 *    shadow PD.
 * -# The SyncPage function may sync more pages on a later \#PFs.
 * -# The page is freed / flushed in SyncCR3 (perhaps) and some other cases.
 *    When caching is enabled, the page isn't flush but remains in the cache.
 *
 *
 * @section sec_pgm_pool_impl       Monitoring
 *
 * We always monitor PAGE_SIZE chunks of memory. When we've got multiple shadow
 * pages for the same PAGE_SIZE of guest memory (PAE and mixed PD/PT) the pages
 * sharing the monitor get linked using the iMonitoredNext/Prev. The head page
 * is the pvUser to the access handlers.
 *
 *
 * @section sec_pgm_pool_impl       Implementation
 *
 * The pool will take pages from the MM page pool. The tracking data
 * (attributes, bitmaps and so on) are allocated from the hypervisor heap. The
 * pool content can be accessed both by using the page id and the physical
 * address (HC). The former is managed by means of an array, the latter by an
 * offset based AVL tree.
 *
 * Flushing of a pool page means that we iterate the content (we know what kind
 * it is) and updates the link information in the ram range.
 *
 * ...
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM_POOL
#include <VBox/pgm.h>
#include <VBox/mm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef PGMPOOL_WITH_MONITORING
static DECLCALLBACK(int) pgmR3PoolAccessHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
#endif /* PGMPOOL_WITH_MONITORING */


/**
 * Initalizes the pool
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
int pgmR3PoolInit(PVM pVM)
{
    AssertCompile(NIL_PGMPOOL_IDX == 0);
    /* pPage->cLocked is an unsigned byte. */
    AssertCompile(VMM_MAX_CPU_COUNT <= 255);

    /*
     * Query Pool config.
     */
    PCFGMNODE pCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/PGM/Pool");

    /** @cfgm{/PGM/Pool/MaxPages, uint16_t, #pages, 16, 0x3fff, 1024}
     * The max size of the shadow page pool in pages. The pool will grow dynamically
     * up to this limit.
     */
    uint16_t cMaxPages;
    int rc = CFGMR3QueryU16Def(pCfg, "MaxPages", &cMaxPages, 4*_1M >> PAGE_SHIFT);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelMsgReturn(cMaxPages <= PGMPOOL_IDX_LAST && cMaxPages >= RT_ALIGN(PGMPOOL_IDX_FIRST, 16),
                          ("cMaxPages=%u (%#x)\n", cMaxPages, cMaxPages), VERR_INVALID_PARAMETER);
    cMaxPages = RT_ALIGN(cMaxPages, 16);

    /** @cfgm{/PGM/Pool/MaxUsers, uint16_t, #users, MaxUsers, 32K, MaxPages*2}
     * The max number of shadow page user tracking records. Each shadow page has
     * zero of other shadow pages (or CR3s) that references it, or uses it if you
     * like. The structures describing these relationships are allocated from a
     * fixed sized pool. This configuration variable defines the pool size.
     */
    uint16_t cMaxUsers;
    rc = CFGMR3QueryU16Def(pCfg, "MaxUsers", &cMaxUsers, cMaxPages * 2);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelMsgReturn(cMaxUsers >= cMaxPages && cMaxPages <= _32K,
                          ("cMaxUsers=%u (%#x)\n", cMaxUsers, cMaxUsers), VERR_INVALID_PARAMETER);

    /** @cfgm{/PGM/Pool/MaxPhysExts, uint16_t, #extents, 16, MaxPages * 2, MAX(MaxPages*2,0x3fff)}
     * The max number of extents for tracking aliased guest pages.
     */
    uint16_t cMaxPhysExts;
    rc = CFGMR3QueryU16Def(pCfg, "MaxPhysExts", &cMaxPhysExts, RT_MAX(cMaxPages * 2, PGMPOOL_IDX_LAST));
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelMsgReturn(cMaxPhysExts >= 16 && cMaxPages <= PGMPOOL_IDX_LAST,
                          ("cMaxPhysExts=%u (%#x)\n", cMaxPhysExts, cMaxPhysExts), VERR_INVALID_PARAMETER);

    /** @cfgm{/PGM/Pool/ChacheEnabled, bool, true}
     * Enables or disabling caching of shadow pages. Chaching means that we will try
     * reuse shadow pages instead of recreating them everything SyncCR3, SyncPT or
     * SyncPage requests one. When reusing a shadow page, we can save time
     * reconstructing it and it's children.
     */
    bool fCacheEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "CacheEnabled", &fCacheEnabled, true);
    AssertLogRelRCReturn(rc, rc);

    Log(("pgmR3PoolInit: cMaxPages=%#RX16 cMaxUsers=%#RX16 cMaxPhysExts=%#RX16 fCacheEnable=%RTbool\n",
         cMaxPages, cMaxUsers, cMaxPhysExts, fCacheEnabled));

    /*
     * Allocate the data structures.
     */
    uint32_t cb = RT_OFFSETOF(PGMPOOL, aPages[cMaxPages]);
#ifdef PGMPOOL_WITH_USER_TRACKING
    cb += cMaxUsers * sizeof(PGMPOOLUSER);
#endif
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    cb += cMaxPhysExts * sizeof(PGMPOOLPHYSEXT);
#endif
    PPGMPOOL pPool;
    rc = MMR3HyperAllocOnceNoRel(pVM, cb, 0, MM_TAG_PGM_POOL, (void **)&pPool);
    if (RT_FAILURE(rc))
        return rc;
    pVM->pgm.s.pPoolR3 = pPool;
    pVM->pgm.s.pPoolR0 = MMHyperR3ToR0(pVM, pPool);
    pVM->pgm.s.pPoolRC = MMHyperR3ToRC(pVM, pPool);

    /*
     * Initialize it.
     */
    pPool->pVMR3     = pVM;
    pPool->pVMR0     = pVM->pVMR0;
    pPool->pVMRC     = pVM->pVMRC;
    pPool->cMaxPages = cMaxPages;
    pPool->cCurPages = PGMPOOL_IDX_FIRST;
#ifdef PGMPOOL_WITH_USER_TRACKING
    pPool->iUserFreeHead = 0;
    pPool->cMaxUsers = cMaxUsers;
    PPGMPOOLUSER paUsers = (PPGMPOOLUSER)&pPool->aPages[pPool->cMaxPages];
    pPool->paUsersR3 = paUsers;
    pPool->paUsersR0 = MMHyperR3ToR0(pVM, paUsers);
    pPool->paUsersRC = MMHyperR3ToRC(pVM, paUsers);
    for (unsigned i = 0; i < cMaxUsers; i++)
    {
        paUsers[i].iNext = i + 1;
        paUsers[i].iUser = NIL_PGMPOOL_IDX;
        paUsers[i].iUserTable = 0xfffffffe;
    }
    paUsers[cMaxUsers - 1].iNext = NIL_PGMPOOL_USER_INDEX;
#endif
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    pPool->iPhysExtFreeHead = 0;
    pPool->cMaxPhysExts = cMaxPhysExts;
    PPGMPOOLPHYSEXT paPhysExts = (PPGMPOOLPHYSEXT)&paUsers[cMaxUsers];
    pPool->paPhysExtsR3 = paPhysExts;
    pPool->paPhysExtsR0 = MMHyperR3ToR0(pVM, paPhysExts);
    pPool->paPhysExtsRC = MMHyperR3ToRC(pVM, paPhysExts);
    for (unsigned i = 0; i < cMaxPhysExts; i++)
    {
        paPhysExts[i].iNext = i + 1;
        paPhysExts[i].aidx[0] = NIL_PGMPOOL_IDX;
        paPhysExts[i].aidx[1] = NIL_PGMPOOL_IDX;
        paPhysExts[i].aidx[2] = NIL_PGMPOOL_IDX;
    }
    paPhysExts[cMaxPhysExts - 1].iNext = NIL_PGMPOOL_PHYSEXT_INDEX;
#endif
#ifdef PGMPOOL_WITH_CACHE
    for (unsigned i = 0; i < RT_ELEMENTS(pPool->aiHash); i++)
        pPool->aiHash[i] = NIL_PGMPOOL_IDX;
    pPool->iAgeHead = NIL_PGMPOOL_IDX;
    pPool->iAgeTail = NIL_PGMPOOL_IDX;
    pPool->fCacheEnabled = fCacheEnabled;
#endif
#ifdef PGMPOOL_WITH_MONITORING
    pPool->pfnAccessHandlerR3 = pgmR3PoolAccessHandler;
    pPool->pszAccessHandler = "Guest Paging Access Handler";
#endif
    pPool->HCPhysTree = 0;

    /* The NIL entry. */
    Assert(NIL_PGMPOOL_IDX == 0);
    pPool->aPages[NIL_PGMPOOL_IDX].enmKind = PGMPOOLKIND_INVALID;

    /* The Shadow 32-bit PD. (32 bits guest paging) */
    pPool->aPages[PGMPOOL_IDX_PD].Core.Key  = NIL_RTHCPHYS;
    pPool->aPages[PGMPOOL_IDX_PD].GCPhys    = NIL_RTGCPHYS;
    pPool->aPages[PGMPOOL_IDX_PD].pvPageR3  = 0;
    pPool->aPages[PGMPOOL_IDX_PD].enmKind   = PGMPOOLKIND_32BIT_PD;
    pPool->aPages[PGMPOOL_IDX_PD].idx       = PGMPOOL_IDX_PD;

    /* The Shadow PDPT. */
    pPool->aPages[PGMPOOL_IDX_PDPT].Core.Key  = NIL_RTHCPHYS;
    pPool->aPages[PGMPOOL_IDX_PDPT].GCPhys    = NIL_RTGCPHYS;
    pPool->aPages[PGMPOOL_IDX_PDPT].pvPageR3  = 0;
    pPool->aPages[PGMPOOL_IDX_PDPT].enmKind   = PGMPOOLKIND_PAE_PDPT;
    pPool->aPages[PGMPOOL_IDX_PDPT].idx       = PGMPOOL_IDX_PDPT;

    /* The Shadow AMD64 CR3. */
    pPool->aPages[PGMPOOL_IDX_AMD64_CR3].Core.Key  = NIL_RTHCPHYS;
    pPool->aPages[PGMPOOL_IDX_AMD64_CR3].GCPhys    = NIL_RTGCPHYS;
    pPool->aPages[PGMPOOL_IDX_AMD64_CR3].pvPageR3  = 0;
    pPool->aPages[PGMPOOL_IDX_AMD64_CR3].enmKind   = PGMPOOLKIND_64BIT_PML4;
    pPool->aPages[PGMPOOL_IDX_AMD64_CR3].idx       = PGMPOOL_IDX_AMD64_CR3;

    /* The Nested Paging CR3. */
    pPool->aPages[PGMPOOL_IDX_NESTED_ROOT].Core.Key  = NIL_RTHCPHYS;
    pPool->aPages[PGMPOOL_IDX_NESTED_ROOT].GCPhys    = NIL_RTGCPHYS;
    pPool->aPages[PGMPOOL_IDX_NESTED_ROOT].pvPageR3  = 0;
    pPool->aPages[PGMPOOL_IDX_NESTED_ROOT].enmKind   = PGMPOOLKIND_ROOT_NESTED;
    pPool->aPages[PGMPOOL_IDX_NESTED_ROOT].idx       = PGMPOOL_IDX_NESTED_ROOT;

    /*
     * Set common stuff.
     */
    for (unsigned iPage = 1; iPage < PGMPOOL_IDX_FIRST; iPage++)
    {
        pPool->aPages[iPage].iNext          = NIL_PGMPOOL_IDX;
#ifdef PGMPOOL_WITH_USER_TRACKING
        pPool->aPages[iPage].iUserHead      = NIL_PGMPOOL_USER_INDEX;
#endif
#ifdef PGMPOOL_WITH_MONITORING
        pPool->aPages[iPage].iModifiedNext  = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iModifiedPrev  = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iMonitoredNext = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iMonitoredNext = NIL_PGMPOOL_IDX;
#endif
#ifdef PGMPOOL_WITH_CACHE
        pPool->aPages[iPage].iAgeNext       = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iAgePrev       = NIL_PGMPOOL_IDX;
#endif
        Assert(pPool->aPages[iPage].idx == iPage);
        Assert(pPool->aPages[iPage].GCPhys == NIL_RTGCPHYS);
        Assert(!pPool->aPages[iPage].fSeenNonGlobal);
        Assert(!pPool->aPages[iPage].fMonitored);
        Assert(!pPool->aPages[iPage].fCached);
        Assert(!pPool->aPages[iPage].fZeroed);
        Assert(!pPool->aPages[iPage].fReusedFlushPending);
    }

#ifdef VBOX_WITH_STATISTICS
    /*
     * Register statistics.
     */
    STAM_REG(pVM, &pPool->cCurPages,                    STAMTYPE_U16,       "/PGM/Pool/cCurPages",      STAMUNIT_PAGES,             "Current pool size.");
    STAM_REG(pVM, &pPool->cMaxPages,                    STAMTYPE_U16,       "/PGM/Pool/cMaxPages",      STAMUNIT_PAGES,             "Max pool size.");
    STAM_REG(pVM, &pPool->cUsedPages,                   STAMTYPE_U16,       "/PGM/Pool/cUsedPages",     STAMUNIT_PAGES,             "The number of pages currently in use.");
    STAM_REG(pVM, &pPool->cUsedPagesHigh,               STAMTYPE_U16_RESET, "/PGM/Pool/cUsedPagesHigh", STAMUNIT_PAGES,             "The high watermark for cUsedPages.");
    STAM_REG(pVM, &pPool->StatAlloc,                  STAMTYPE_PROFILE_ADV, "/PGM/Pool/Alloc",          STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolAlloc.");
    STAM_REG(pVM, &pPool->StatClearAll,                 STAMTYPE_PROFILE,   "/PGM/Pool/ClearAll",       STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolClearAll.");
    STAM_REG(pVM, &pPool->StatFlushAllInt,              STAMTYPE_PROFILE,   "/PGM/Pool/FlushAllInt",    STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolFlushAllInt.");
    STAM_REG(pVM, &pPool->StatFlushPage,                STAMTYPE_PROFILE,   "/PGM/Pool/FlushPage",      STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolFlushPage.");
    STAM_REG(pVM, &pPool->StatFree,                     STAMTYPE_PROFILE,   "/PGM/Pool/Free",           STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolFree.");
    STAM_REG(pVM, &pPool->StatZeroPage,                 STAMTYPE_PROFILE,   "/PGM/Pool/ZeroPage",       STAMUNIT_TICKS_PER_CALL,    "Profiling time spent zeroing pages. Overlaps with Alloc.");
# ifdef PGMPOOL_WITH_USER_TRACKING
    STAM_REG(pVM, &pPool->cMaxUsers,                    STAMTYPE_U16,       "/PGM/Pool/Track/cMaxUsers",            STAMUNIT_COUNT,      "Max user tracking records.");
    STAM_REG(pVM, &pPool->cPresent,                     STAMTYPE_U32,       "/PGM/Pool/Track/cPresent",             STAMUNIT_COUNT,      "Number of present page table entries.");
    STAM_REG(pVM, &pPool->StatTrackDeref,               STAMTYPE_PROFILE,   "/PGM/Pool/Track/Deref",                STAMUNIT_OCCURENCES, "Profiling of pgmPoolTrackDeref.");
    STAM_REG(pVM, &pPool->StatTrackFlushGCPhysPT,       STAMTYPE_PROFILE,   "/PGM/Pool/Track/FlushGCPhysPT",        STAMUNIT_OCCURENCES, "Profiling of pgmPoolTrackFlushGCPhysPT.");
    STAM_REG(pVM, &pPool->StatTrackFlushGCPhysPTs,      STAMTYPE_PROFILE,   "/PGM/Pool/Track/FlushGCPhysPTs",       STAMUNIT_OCCURENCES, "Profiling of pgmPoolTrackFlushGCPhysPTs.");
    STAM_REG(pVM, &pPool->StatTrackFlushGCPhysPTsSlow,  STAMTYPE_PROFILE,   "/PGM/Pool/Track/FlushGCPhysPTsSlow",   STAMUNIT_OCCURENCES, "Profiling of pgmPoolTrackFlushGCPhysPTsSlow.");
    STAM_REG(pVM, &pPool->StatTrackFreeUpOneUser,       STAMTYPE_COUNTER,   "/PGM/Pool/Track/FreeUpOneUser",        STAMUNIT_OCCURENCES, "The number of times we were out of user tracking records.");
# endif
# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    STAM_REG(pVM, &pPool->StatTrackDerefGCPhys,         STAMTYPE_PROFILE,   "/PGM/Pool/Track/DrefGCPhys",           STAMUNIT_OCCURENCES, "Profiling deref activity related tracking GC physical pages.");
    STAM_REG(pVM, &pPool->StatTrackLinearRamSearches,   STAMTYPE_COUNTER,   "/PGM/Pool/Track/LinearRamSearches",    STAMUNIT_OCCURENCES, "The number of times we had to do linear ram searches.");
    STAM_REG(pVM, &pPool->StamTrackPhysExtAllocFailures,STAMTYPE_COUNTER,   "/PGM/Pool/Track/PhysExtAllocFailures", STAMUNIT_OCCURENCES, "The number of failing pgmPoolTrackPhysExtAlloc calls.");
# endif
# ifdef PGMPOOL_WITH_MONITORING
    STAM_REG(pVM, &pPool->StatMonitorRZ,                STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ",                 STAMUNIT_TICKS_PER_CALL, "Profiling the RC/R0 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorRZEmulateInstr,    STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/EmulateInstr",    STAMUNIT_OCCURENCES,     "Times we've failed interpreting the instruction.");
    STAM_REG(pVM, &pPool->StatMonitorRZFlushPage,       STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ/FlushPage",       STAMUNIT_TICKS_PER_CALL, "Profiling the pgmPoolFlushPage calls made from the RC/R0 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorRZFork,            STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/Fork",            STAMUNIT_OCCURENCES,     "Times we've detected fork().");
    STAM_REG(pVM, &pPool->StatMonitorRZHandled,         STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ/Handled",         STAMUNIT_TICKS_PER_CALL, "Profiling the RC/R0 access we've handled (except REP STOSD).");
    STAM_REG(pVM, &pPool->StatMonitorRZIntrFailPatch1,  STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/IntrFailPatch1",  STAMUNIT_OCCURENCES,     "Times we've failed interpreting a patch code instruction.");
    STAM_REG(pVM, &pPool->StatMonitorRZIntrFailPatch2,  STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/IntrFailPatch2",  STAMUNIT_OCCURENCES,     "Times we've failed interpreting a patch code instruction during flushing.");
    STAM_REG(pVM, &pPool->StatMonitorRZRepPrefix,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/RepPrefix",       STAMUNIT_OCCURENCES,     "The number of times we've seen rep prefixes we can't handle.");
    STAM_REG(pVM, &pPool->StatMonitorRZRepStosd,        STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ/RepStosd",        STAMUNIT_TICKS_PER_CALL, "Profiling the REP STOSD cases we've handled.");
    STAM_REG(pVM, &pPool->StatMonitorR3,                STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3",                 STAMUNIT_TICKS_PER_CALL, "Profiling the R3 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorR3EmulateInstr,    STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/EmulateInstr",    STAMUNIT_OCCURENCES,     "Times we've failed interpreting the instruction.");
    STAM_REG(pVM, &pPool->StatMonitorR3FlushPage,       STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3/FlushPage",       STAMUNIT_TICKS_PER_CALL, "Profiling the pgmPoolFlushPage calls made from the R3 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorR3Fork,            STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Fork",            STAMUNIT_OCCURENCES,     "Times we've detected fork().");
    STAM_REG(pVM, &pPool->StatMonitorR3Handled,         STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3/Handled",         STAMUNIT_TICKS_PER_CALL, "Profiling the R3 access we've handled (except REP STOSD).");
    STAM_REG(pVM, &pPool->StatMonitorR3RepPrefix,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/RepPrefix",       STAMUNIT_OCCURENCES,     "The number of times we've seen rep prefixes we can't handle.");
    STAM_REG(pVM, &pPool->StatMonitorR3RepStosd,        STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3/RepStosd",        STAMUNIT_TICKS_PER_CALL, "Profiling the REP STOSD cases we've handled.");
    STAM_REG(pVM, &pPool->StatMonitorR3Async,           STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Async",           STAMUNIT_OCCURENCES,     "Times we're called in an async thread and need to flush.");
    STAM_REG(pVM, &pPool->cModifiedPages,               STAMTYPE_U16,       "/PGM/Pool/Monitor/cModifiedPages",     STAMUNIT_PAGES,          "The current cModifiedPages value.");
    STAM_REG(pVM, &pPool->cModifiedPagesHigh,           STAMTYPE_U16_RESET, "/PGM/Pool/Monitor/cModifiedPagesHigh", STAMUNIT_PAGES,          "The high watermark for cModifiedPages.");
# endif
# ifdef PGMPOOL_WITH_CACHE
    STAM_REG(pVM, &pPool->StatCacheHits,                STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Hits",                 STAMUNIT_OCCURENCES, "The number of pgmPoolAlloc calls satisfied by the cache.");
    STAM_REG(pVM, &pPool->StatCacheMisses,              STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Misses",               STAMUNIT_OCCURENCES, "The number of pgmPoolAlloc calls not statisfied by the cache.");
    STAM_REG(pVM, &pPool->StatCacheKindMismatches,      STAMTYPE_COUNTER,   "/PGM/Pool/Cache/KindMismatches",       STAMUNIT_OCCURENCES, "The number of shadow page kind mismatches. (Better be low, preferably 0!)");
    STAM_REG(pVM, &pPool->StatCacheFreeUpOne,           STAMTYPE_COUNTER,   "/PGM/Pool/Cache/FreeUpOne",            STAMUNIT_OCCURENCES, "The number of times the cache was asked to free up a page.");
    STAM_REG(pVM, &pPool->StatCacheCacheable,           STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Cacheable",            STAMUNIT_OCCURENCES, "The number of cacheable allocations.");
    STAM_REG(pVM, &pPool->StatCacheUncacheable,         STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Uncacheable",          STAMUNIT_OCCURENCES, "The number of uncacheable allocations.");
# endif
#endif /* VBOX_WITH_STATISTICS */

    return VINF_SUCCESS;
}


/**
 * Relocate the page pool data.
 *
 * @param   pVM     The VM handle.
 */
void pgmR3PoolRelocate(PVM pVM)
{
    pVM->pgm.s.pPoolRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pPoolR3);
    pVM->pgm.s.pPoolR3->pVMRC = pVM->pVMRC;
#ifdef PGMPOOL_WITH_USER_TRACKING
    pVM->pgm.s.pPoolR3->paUsersRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pPoolR3->paUsersR3);
#endif
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    pVM->pgm.s.pPoolR3->paPhysExtsRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pPoolR3->paPhysExtsR3);
#endif
#ifdef PGMPOOL_WITH_MONITORING
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "pgmPoolAccessHandler", &pVM->pgm.s.pPoolR3->pfnAccessHandlerRC);
    AssertReleaseRC(rc);
    /* init order hack. */
    if (!pVM->pgm.s.pPoolR3->pfnAccessHandlerR0)
    {
        rc = PDMR3LdrGetSymbolR0(pVM, NULL, "pgmPoolAccessHandler", &pVM->pgm.s.pPoolR3->pfnAccessHandlerR0);
        AssertReleaseRC(rc);
    }
#endif
}


/**
 * Reset notification.
 *
 * This will flush the pool.
 * @param   pVM     The VM handle.
 */
void pgmR3PoolReset(PVM pVM)
{
    pgmPoolFlushAll(pVM);
}


/**
 * Grows the shadow page pool.
 *
 * I.e. adds more pages to it, assuming that hasn't reached cMaxPages yet.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int) PGMR3PoolGrow(PVM pVM)
{
    PPGMPOOL pPool = pVM->pgm.s.pPoolR3;
    AssertReturn(pPool->cCurPages < pPool->cMaxPages, VERR_INTERNAL_ERROR);

    pgmLock(pVM);

    /*
     * How much to grow it by?
     */
    uint32_t cPages = pPool->cMaxPages - pPool->cCurPages;
    cPages = RT_MIN(PGMPOOL_CFG_MAX_GROW, cPages);
    LogFlow(("PGMR3PoolGrow: Growing the pool by %d (%#x) pages.\n", cPages, cPages));

    for (unsigned i = pPool->cCurPages; cPages-- > 0; i++)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[i];

        /* Allocate all pages in low (below 4 GB) memory as 32 bits guests need a page table root in low memory. */
        pPage->pvPageR3 = MMR3PageAllocLow(pVM);
        if (!pPage->pvPageR3)
        {
            Log(("We're out of memory!! i=%d\n", i));
            pgmUnlock(pVM);
            return i ? VINF_SUCCESS : VERR_NO_PAGE_MEMORY;
        }
        pPage->Core.Key  = MMPage2Phys(pVM, pPage->pvPageR3);
        pPage->GCPhys    = NIL_RTGCPHYS;
        pPage->enmKind   = PGMPOOLKIND_FREE;
        pPage->idx       = pPage - &pPool->aPages[0];
        LogFlow(("PGMR3PoolGrow: insert page #%#x - %RHp\n", pPage->idx, pPage->Core.Key));
        pPage->iNext     = pPool->iFreeHead;
#ifdef PGMPOOL_WITH_USER_TRACKING
        pPage->iUserHead = NIL_PGMPOOL_USER_INDEX;
#endif
#ifdef PGMPOOL_WITH_MONITORING
        pPage->iModifiedNext  = NIL_PGMPOOL_IDX;
        pPage->iModifiedPrev  = NIL_PGMPOOL_IDX;
        pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
        pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
#endif
#ifdef PGMPOOL_WITH_CACHE
        pPage->iAgeNext  = NIL_PGMPOOL_IDX;
        pPage->iAgePrev  = NIL_PGMPOOL_IDX;
#endif
        /* commit it */
        bool fRc = RTAvloHCPhysInsert(&pPool->HCPhysTree, &pPage->Core); Assert(fRc); NOREF(fRc);
        pPool->iFreeHead = i;
        pPool->cCurPages = i + 1;
    }

    pgmUnlock(pVM);
    Assert(pPool->cCurPages <= pPool->cMaxPages);
    return VINF_SUCCESS;
}


#ifdef PGMPOOL_WITH_MONITORING

/**
 * Worker used by pgmR3PoolAccessHandler when it's invoked by an async thread.
 *
 * @param   pPool   The pool.
 * @param   pPage   The page.
 */
static DECLCALLBACK(void) pgmR3PoolFlushReusedPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    /* for the present this should be safe enough I think... */
    pgmLock(pPool->pVMR3);
    if (    pPage->fReusedFlushPending
        &&  pPage->enmKind != PGMPOOLKIND_FREE)
        pgmPoolFlushPage(pPool, pPage);
    pgmUnlock(pPool->pVMR3);
}


/**
 * \#PF Handler callback for PT write accesses.
 *
 * The handler can not raise any faults, it's mainly for monitoring write access
 * to certain pages.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) pgmR3PoolAccessHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    STAM_PROFILE_START(&pVM->pgm.s.pPoolR3->StatMonitorR3, a);
    PPGMPOOL pPool = pVM->pgm.s.pPoolR3;
    PPGMPOOLPAGE pPage = (PPGMPOOLPAGE)pvUser;
    LogFlow(("pgmR3PoolAccessHandler: GCPhys=%RGp %p:{.Core=%RHp, .idx=%d, .GCPhys=%RGp, .enmType=%d}\n",
             GCPhys, pPage, pPage->Core.Key, pPage->idx, pPage->GCPhys, pPage->enmKind));

    PVMCPU pVCpu = VMMGetCpu(pVM);

    /*
     * We don't have to be very sophisticated about this since there are relativly few calls here.
     * However, we must try our best to detect any non-cpu accesses (disk / networking).
     *
     * Just to make life more interesting, we'll have to deal with the async threads too.
     * We cannot flush a page if we're in an async thread because of REM notifications.
     */
    pgmLock(pVM);
    if (!pVCpu) /** @todo This shouldn't happen any longer, all access handlers will be called on an EMT. All ring-3 handlers, except MMIO, already owns the PGM lock. @bugref{3170} */
    {
        Log(("pgmR3PoolAccessHandler: async thread, requesting EMT to flush the page: %p:{.Core=%RHp, .idx=%d, .GCPhys=%RGp, .enmType=%d}\n",
             pPage, pPage->Core.Key, pPage->idx, pPage->GCPhys, pPage->enmKind));
        STAM_COUNTER_INC(&pPool->StatMonitorR3Async);
        if (!pPage->fReusedFlushPending)
        {
            pgmUnlock(pVM);
            int rc = VMR3ReqCallEx(pPool->pVMR3, VMCPUID_ANY, NULL, 0, VMREQFLAGS_NO_WAIT | VMREQFLAGS_VOID, (PFNRT)pgmR3PoolFlushReusedPage, 2, pPool, pPage);
            AssertRCReturn(rc, rc);
            pgmLock(pVM);
            pPage->fReusedFlushPending = true;
            pPage->cModifications += 0x1000;
        }

        pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhys, pvPhys, NULL);
        /** @todo r=bird: making unsafe assumption about not crossing entries here! */
        while (cbBuf > 4)
        {
            cbBuf -= 4;
            pvPhys = (uint8_t *)pvPhys + 4;
            GCPhys += 4;
            pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhys, pvPhys, NULL);
        }
        STAM_PROFILE_STOP(&pPool->StatMonitorR3, a);
    }
    else if (    (   pPage->cModifications < 96 /* it's cheaper here. */
                  || pgmPoolIsPageLocked(&pVM->pgm.s, pPage)
                  )
             &&  cbBuf <= 4)
    {
        /* Clear the shadow entry. */
        if (!pPage->cModifications++)
            pgmPoolMonitorModifiedInsert(pPool, pPage);
        /** @todo r=bird: making unsafe assumption about not crossing entries here! */
        pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhys, pvPhys, NULL);
        STAM_PROFILE_STOP(&pPool->StatMonitorR3, a);
    }
    else
    {
        pgmPoolMonitorChainFlush(pPool, pPage); /* ASSUME that VERR_PGM_POOL_CLEARED can be ignored here and that FFs will deal with it in due time. */
        STAM_PROFILE_STOP_EX(&pPool->StatMonitorR3, &pPool->StatMonitorR3FlushPage, a);
    }
    pgmUnlock(pVM);
    return VINF_PGM_HANDLER_DO_DEFAULT;
}

#endif /* PGMPOOL_WITH_MONITORING */

