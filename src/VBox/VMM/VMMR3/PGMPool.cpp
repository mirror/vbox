/* $Id$ */
/** @file
 * PGM Shadow Page Pool.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/mm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"

#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/dbg.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pgmR3PoolAccessHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
#ifdef VBOX_WITH_DEBUGGER
static DECLCALLBACK(int)  pgmR3PoolCmdCheck(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs);
#endif

#ifdef VBOX_WITH_DEBUGGER
/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,  cArgsMin, cArgsMax, paArgDesc, cArgDescs, fFlags, pfnHandler          pszSyntax,  ....pszDescription */
    { "pgmpoolcheck",  0, 0,        NULL,      0,         0,      pgmR3PoolCmdCheck,  "",         "Check the pgm pool pages." },
};
#endif

/**
 * Initializes the pool
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
int pgmR3PoolInit(PVM pVM)
{
    int rc;

    AssertCompile(NIL_PGMPOOL_IDX == 0);
    /* pPage->cLocked is an unsigned byte. */
    AssertCompile(VMM_MAX_CPU_COUNT <= 255);

    /*
     * Query Pool config.
     */
    PCFGMNODE pCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/PGM/Pool");

    /* Default pgm pool size is 1024 pages (4MB). */
    uint16_t cMaxPages = 1024;

    /* Adjust it up relative to the RAM size, using the nested paging formula. */
    uint64_t cbRam;
    rc = CFGMR3QueryU64Def(CFGMR3GetRoot(pVM), "RamSize", &cbRam, 0); AssertRCReturn(rc, rc);
    uint64_t u64MaxPages = (cbRam >> 9)
                         + (cbRam >> 18)
                         + (cbRam >> 27)
                         + 32 * PAGE_SIZE;
    u64MaxPages >>= PAGE_SHIFT;
    if (u64MaxPages > PGMPOOL_IDX_LAST)
        cMaxPages = PGMPOOL_IDX_LAST;
    else
        cMaxPages = (uint16_t)u64MaxPages;

    /** @cfgm{/PGM/Pool/MaxPages, uint16_t, #pages, 16, 0x3fff, F(ram-size)}
     * The max size of the shadow page pool in pages. The pool will grow dynamically
     * up to this limit.
     */
    rc = CFGMR3QueryU16Def(pCfg, "MaxPages", &cMaxPages, cMaxPages);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelMsgReturn(cMaxPages <= PGMPOOL_IDX_LAST && cMaxPages >= RT_ALIGN(PGMPOOL_IDX_FIRST, 16),
                          ("cMaxPages=%u (%#x)\n", cMaxPages, cMaxPages), VERR_INVALID_PARAMETER);
    cMaxPages = RT_ALIGN(cMaxPages, 16);
    if (cMaxPages > PGMPOOL_IDX_LAST)
        cMaxPages = PGMPOOL_IDX_LAST;
    LogRel(("PGMPool: cMaxPages=%u (u64MaxPages=%llu)\n", cMaxPages, u64MaxPages));

    /** todo:
     * We need to be much more careful with our allocation strategy here.
     * For nested paging we don't need pool user info nor extents at all, but
     * we can't check for nested paging here (too early during init to get a
     * confirmation it can be used).  The default for large memory configs is a
     * bit large for shadow paging, so I've restricted the extent maximum to 8k
     * (8k * 16 = 128k of hyper heap).
     *
     * Also when large page support is enabled, we typically don't need so much,
     * although that depends on the availability of 2 MB chunks on the host.
     */

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

    /** @cfgm{/PGM/Pool/MaxPhysExts, uint16_t, #extents, 16, MaxPages * 2, MIN(MaxPages*2,8192)}
     * The max number of extents for tracking aliased guest pages.
     */
    uint16_t cMaxPhysExts;
    rc = CFGMR3QueryU16Def(pCfg, "MaxPhysExts", &cMaxPhysExts,
                           RT_MIN(cMaxPages * 2, 8192 /* 8Ki max as this eat too much hyper heap */));
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelMsgReturn(cMaxPhysExts >= 16 && cMaxPhysExts <= PGMPOOL_IDX_LAST,
                          ("cMaxPhysExts=%u (%#x)\n", cMaxPhysExts, cMaxPhysExts), VERR_INVALID_PARAMETER);

    /** @cfgm{/PGM/Pool/ChacheEnabled, bool, true}
     * Enables or disabling caching of shadow pages. Caching means that we will try
     * reuse shadow pages instead of recreating them everything SyncCR3, SyncPT or
     * SyncPage requests one. When reusing a shadow page, we can save time
     * reconstructing it and it's children.
     */
    bool fCacheEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "CacheEnabled", &fCacheEnabled, true);
    AssertLogRelRCReturn(rc, rc);

    LogRel(("pgmR3PoolInit: cMaxPages=%#RX16 cMaxUsers=%#RX16 cMaxPhysExts=%#RX16 fCacheEnable=%RTbool\n",
             cMaxPages, cMaxUsers, cMaxPhysExts, fCacheEnabled));

    /*
     * Allocate the data structures.
     */
    uint32_t cb = RT_OFFSETOF(PGMPOOL, aPages[cMaxPages]);
    cb += cMaxUsers * sizeof(PGMPOOLUSER);
    cb += cMaxPhysExts * sizeof(PGMPOOLPHYSEXT);
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
        paPhysExts[i].apte[0] = NIL_PGMPOOL_PHYSEXT_IDX_PTE;
        paPhysExts[i].aidx[1] = NIL_PGMPOOL_IDX;
        paPhysExts[i].apte[1] = NIL_PGMPOOL_PHYSEXT_IDX_PTE;
        paPhysExts[i].aidx[2] = NIL_PGMPOOL_IDX;
        paPhysExts[i].apte[2] = NIL_PGMPOOL_PHYSEXT_IDX_PTE;
    }
    paPhysExts[cMaxPhysExts - 1].iNext = NIL_PGMPOOL_PHYSEXT_INDEX;
    for (unsigned i = 0; i < RT_ELEMENTS(pPool->aiHash); i++)
        pPool->aiHash[i] = NIL_PGMPOOL_IDX;
    pPool->iAgeHead = NIL_PGMPOOL_IDX;
    pPool->iAgeTail = NIL_PGMPOOL_IDX;
    pPool->fCacheEnabled = fCacheEnabled;
    pPool->pfnAccessHandlerR3 = pgmR3PoolAccessHandler;
    pPool->pszAccessHandler = "Guest Paging Access Handler";
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
        pPool->aPages[iPage].iUserHead      = NIL_PGMPOOL_USER_INDEX;
        pPool->aPages[iPage].iModifiedNext  = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iModifiedPrev  = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iMonitoredNext = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iMonitoredNext = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iAgeNext       = NIL_PGMPOOL_IDX;
        pPool->aPages[iPage].iAgePrev       = NIL_PGMPOOL_IDX;
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
    STAM_REG(pVM, &pPool->StatClearAll,                 STAMTYPE_PROFILE,   "/PGM/Pool/ClearAll",       STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmR3PoolClearAll.");
    STAM_REG(pVM, &pPool->StatR3Reset,                  STAMTYPE_PROFILE,   "/PGM/Pool/R3Reset",        STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmR3PoolReset.");
    STAM_REG(pVM, &pPool->StatFlushPage,                STAMTYPE_PROFILE,   "/PGM/Pool/FlushPage",      STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolFlushPage.");
    STAM_REG(pVM, &pPool->StatFree,                     STAMTYPE_PROFILE,   "/PGM/Pool/Free",           STAMUNIT_TICKS_PER_CALL,    "Profiling of pgmPoolFree.");
    STAM_REG(pVM, &pPool->StatForceFlushPage,           STAMTYPE_COUNTER,   "/PGM/Pool/FlushForce",     STAMUNIT_OCCURENCES,        "Counting explicit flushes by PGMPoolFlushPage().");
    STAM_REG(pVM, &pPool->StatForceFlushDirtyPage,      STAMTYPE_COUNTER,   "/PGM/Pool/FlushForceDirty",     STAMUNIT_OCCURENCES,   "Counting explicit flushes of dirty pages by PGMPoolFlushPage().");
    STAM_REG(pVM, &pPool->StatForceFlushReused,         STAMTYPE_COUNTER,   "/PGM/Pool/FlushReused",    STAMUNIT_OCCURENCES,        "Counting flushes for reused pages.");
    STAM_REG(pVM, &pPool->StatZeroPage,                 STAMTYPE_PROFILE,   "/PGM/Pool/ZeroPage",       STAMUNIT_TICKS_PER_CALL,    "Profiling time spent zeroing pages. Overlaps with Alloc.");
    STAM_REG(pVM, &pPool->cMaxUsers,                    STAMTYPE_U16,       "/PGM/Pool/Track/cMaxUsers",            STAMUNIT_COUNT,      "Max user tracking records.");
    STAM_REG(pVM, &pPool->cPresent,                     STAMTYPE_U32,       "/PGM/Pool/Track/cPresent",             STAMUNIT_COUNT,      "Number of present page table entries.");
    STAM_REG(pVM, &pPool->StatTrackDeref,               STAMTYPE_PROFILE,   "/PGM/Pool/Track/Deref",                STAMUNIT_TICKS_PER_CALL, "Profiling of pgmPoolTrackDeref.");
    STAM_REG(pVM, &pPool->StatTrackFlushGCPhysPT,       STAMTYPE_PROFILE,   "/PGM/Pool/Track/FlushGCPhysPT",        STAMUNIT_TICKS_PER_CALL, "Profiling of pgmPoolTrackFlushGCPhysPT.");
    STAM_REG(pVM, &pPool->StatTrackFlushGCPhysPTs,      STAMTYPE_PROFILE,   "/PGM/Pool/Track/FlushGCPhysPTs",       STAMUNIT_TICKS_PER_CALL, "Profiling of pgmPoolTrackFlushGCPhysPTs.");
    STAM_REG(pVM, &pPool->StatTrackFlushGCPhysPTsSlow,  STAMTYPE_PROFILE,   "/PGM/Pool/Track/FlushGCPhysPTsSlow",   STAMUNIT_TICKS_PER_CALL, "Profiling of pgmPoolTrackFlushGCPhysPTsSlow.");
    STAM_REG(pVM, &pPool->StatTrackFlushEntry,          STAMTYPE_COUNTER,   "/PGM/Pool/Track/Entry/Flush",          STAMUNIT_COUNT,          "Nr of flushed entries.");
    STAM_REG(pVM, &pPool->StatTrackFlushEntryKeep,      STAMTYPE_COUNTER,   "/PGM/Pool/Track/Entry/Update",         STAMUNIT_COUNT,          "Nr of updated entries.");
    STAM_REG(pVM, &pPool->StatTrackFreeUpOneUser,       STAMTYPE_COUNTER,   "/PGM/Pool/Track/FreeUpOneUser",        STAMUNIT_TICKS_PER_CALL, "The number of times we were out of user tracking records.");
    STAM_REG(pVM, &pPool->StatTrackDerefGCPhys,         STAMTYPE_PROFILE,   "/PGM/Pool/Track/DrefGCPhys",           STAMUNIT_TICKS_PER_CALL, "Profiling deref activity related tracking GC physical pages.");
    STAM_REG(pVM, &pPool->StatTrackLinearRamSearches,   STAMTYPE_COUNTER,   "/PGM/Pool/Track/LinearRamSearches",    STAMUNIT_OCCURENCES, "The number of times we had to do linear ram searches.");
    STAM_REG(pVM, &pPool->StamTrackPhysExtAllocFailures,STAMTYPE_COUNTER,   "/PGM/Pool/Track/PhysExtAllocFailures", STAMUNIT_OCCURENCES, "The number of failing pgmPoolTrackPhysExtAlloc calls.");
    STAM_REG(pVM, &pPool->StatMonitorRZ,                STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ",                 STAMUNIT_TICKS_PER_CALL, "Profiling the RC/R0 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorRZEmulateInstr,    STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/EmulateInstr",    STAMUNIT_OCCURENCES,     "Times we've failed interpreting the instruction.");
    STAM_REG(pVM, &pPool->StatMonitorRZFlushPage,       STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ/FlushPage",       STAMUNIT_TICKS_PER_CALL, "Profiling the pgmPoolFlushPage calls made from the RC/R0 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorRZFlushReinit,     STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/FlushReinit",     STAMUNIT_OCCURENCES,     "Times we've detected a page table reinit.");
    STAM_REG(pVM, &pPool->StatMonitorRZFlushModOverflow,STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/FlushOverflow",   STAMUNIT_OCCURENCES,     "Counting flushes for pages that are modified too often.");
    STAM_REG(pVM, &pPool->StatMonitorRZFork,            STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/Fork",            STAMUNIT_OCCURENCES,     "Times we've detected fork().");
    STAM_REG(pVM, &pPool->StatMonitorRZHandled,         STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ/Handled",         STAMUNIT_TICKS_PER_CALL, "Profiling the RC/R0 access we've handled (except REP STOSD).");
    STAM_REG(pVM, &pPool->StatMonitorRZIntrFailPatch1,  STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/IntrFailPatch1",  STAMUNIT_OCCURENCES,     "Times we've failed interpreting a patch code instruction.");
    STAM_REG(pVM, &pPool->StatMonitorRZIntrFailPatch2,  STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/IntrFailPatch2",  STAMUNIT_OCCURENCES,     "Times we've failed interpreting a patch code instruction during flushing.");
    STAM_REG(pVM, &pPool->StatMonitorRZRepPrefix,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/RepPrefix",       STAMUNIT_OCCURENCES,     "The number of times we've seen rep prefixes we can't handle.");
    STAM_REG(pVM, &pPool->StatMonitorRZRepStosd,        STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/RZ/RepStosd",        STAMUNIT_TICKS_PER_CALL, "Profiling the REP STOSD cases we've handled.");
    STAM_REG(pVM, &pPool->StatMonitorRZFaultPT,         STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/Fault/PT",        STAMUNIT_OCCURENCES,     "Nr of handled PT faults.");
    STAM_REG(pVM, &pPool->StatMonitorRZFaultPD,         STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/Fault/PD",        STAMUNIT_OCCURENCES,     "Nr of handled PD faults.");
    STAM_REG(pVM, &pPool->StatMonitorRZFaultPDPT,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/Fault/PDPT",      STAMUNIT_OCCURENCES,     "Nr of handled PDPT faults.");
    STAM_REG(pVM, &pPool->StatMonitorRZFaultPML4,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/RZ/Fault/PML4",      STAMUNIT_OCCURENCES,     "Nr of handled PML4 faults.");
    STAM_REG(pVM, &pPool->StatMonitorR3,                STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3",                 STAMUNIT_TICKS_PER_CALL, "Profiling the R3 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorR3EmulateInstr,    STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/EmulateInstr",    STAMUNIT_OCCURENCES,     "Times we've failed interpreting the instruction.");
    STAM_REG(pVM, &pPool->StatMonitorR3FlushPage,       STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3/FlushPage",       STAMUNIT_TICKS_PER_CALL, "Profiling the pgmPoolFlushPage calls made from the R3 access handler.");
    STAM_REG(pVM, &pPool->StatMonitorR3FlushReinit,     STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/FlushReinit",     STAMUNIT_OCCURENCES,     "Times we've detected a page table reinit.");
    STAM_REG(pVM, &pPool->StatMonitorR3FlushModOverflow,STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/FlushOverflow",   STAMUNIT_OCCURENCES,     "Counting flushes for pages that are modified too often.");
    STAM_REG(pVM, &pPool->StatMonitorR3Fork,            STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Fork",            STAMUNIT_OCCURENCES,     "Times we've detected fork().");
    STAM_REG(pVM, &pPool->StatMonitorR3Handled,         STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3/Handled",         STAMUNIT_TICKS_PER_CALL, "Profiling the R3 access we've handled (except REP STOSD).");
    STAM_REG(pVM, &pPool->StatMonitorR3RepPrefix,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/RepPrefix",       STAMUNIT_OCCURENCES,     "The number of times we've seen rep prefixes we can't handle.");
    STAM_REG(pVM, &pPool->StatMonitorR3RepStosd,        STAMTYPE_PROFILE,   "/PGM/Pool/Monitor/R3/RepStosd",        STAMUNIT_TICKS_PER_CALL, "Profiling the REP STOSD cases we've handled.");
    STAM_REG(pVM, &pPool->StatMonitorR3FaultPT,         STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Fault/PT",        STAMUNIT_OCCURENCES,     "Nr of handled PT faults.");
    STAM_REG(pVM, &pPool->StatMonitorR3FaultPD,         STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Fault/PD",        STAMUNIT_OCCURENCES,     "Nr of handled PD faults.");
    STAM_REG(pVM, &pPool->StatMonitorR3FaultPDPT,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Fault/PDPT",      STAMUNIT_OCCURENCES,     "Nr of handled PDPT faults.");
    STAM_REG(pVM, &pPool->StatMonitorR3FaultPML4,       STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Fault/PML4",      STAMUNIT_OCCURENCES,     "Nr of handled PML4 faults.");
    STAM_REG(pVM, &pPool->StatMonitorR3Async,           STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/R3/Async",           STAMUNIT_OCCURENCES,     "Times we're called in an async thread and need to flush.");
    STAM_REG(pVM, &pPool->cModifiedPages,               STAMTYPE_U16,       "/PGM/Pool/Monitor/cModifiedPages",     STAMUNIT_PAGES,          "The current cModifiedPages value.");
    STAM_REG(pVM, &pPool->cModifiedPagesHigh,           STAMTYPE_U16_RESET, "/PGM/Pool/Monitor/cModifiedPagesHigh", STAMUNIT_PAGES,          "The high watermark for cModifiedPages.");
    STAM_REG(pVM, &pPool->StatResetDirtyPages,          STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/Dirty/Resets",       STAMUNIT_OCCURENCES,     "Times we've called pgmPoolResetDirtyPages (and there were dirty page).");
    STAM_REG(pVM, &pPool->StatDirtyPage,                STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/Dirty/Pages",        STAMUNIT_OCCURENCES,     "Times we've called pgmPoolAddDirtyPage.");
    STAM_REG(pVM, &pPool->StatDirtyPageDupFlush,        STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/Dirty/FlushDup",     STAMUNIT_OCCURENCES,     "Times we've had to flush duplicates for dirty page management.");
    STAM_REG(pVM, &pPool->StatDirtyPageOverFlowFlush,   STAMTYPE_COUNTER,   "/PGM/Pool/Monitor/Dirty/FlushOverflow",STAMUNIT_OCCURENCES,     "Times we've had to flush because of overflow.");
    STAM_REG(pVM, &pPool->StatCacheHits,                STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Hits",                 STAMUNIT_OCCURENCES, "The number of pgmPoolAlloc calls satisfied by the cache.");
    STAM_REG(pVM, &pPool->StatCacheMisses,              STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Misses",               STAMUNIT_OCCURENCES, "The number of pgmPoolAlloc calls not statisfied by the cache.");
    STAM_REG(pVM, &pPool->StatCacheKindMismatches,      STAMTYPE_COUNTER,   "/PGM/Pool/Cache/KindMismatches",       STAMUNIT_OCCURENCES, "The number of shadow page kind mismatches. (Better be low, preferably 0!)");
    STAM_REG(pVM, &pPool->StatCacheFreeUpOne,           STAMTYPE_COUNTER,   "/PGM/Pool/Cache/FreeUpOne",            STAMUNIT_OCCURENCES, "The number of times the cache was asked to free up a page.");
    STAM_REG(pVM, &pPool->StatCacheCacheable,           STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Cacheable",            STAMUNIT_OCCURENCES, "The number of cacheable allocations.");
    STAM_REG(pVM, &pPool->StatCacheUncacheable,         STAMTYPE_COUNTER,   "/PGM/Pool/Cache/Uncacheable",          STAMUNIT_OCCURENCES, "The number of uncacheable allocations.");
#endif /* VBOX_WITH_STATISTICS */

#ifdef VBOX_WITH_DEBUGGER
    /*
     * Debugger commands.
     */
    static bool s_fRegisteredCmds = false;
    if (!s_fRegisteredCmds)
    {
        rc = DBGCRegisterCommands(&g_aCmds[0], RT_ELEMENTS(g_aCmds));
        if (RT_SUCCESS(rc))
            s_fRegisteredCmds = true;
    }
#endif

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
    pVM->pgm.s.pPoolR3->paUsersRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pPoolR3->paUsersR3);
    pVM->pgm.s.pPoolR3->paPhysExtsRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pPoolR3->paPhysExtsR3);
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "pgmPoolAccessHandler", &pVM->pgm.s.pPoolR3->pfnAccessHandlerRC);
    AssertReleaseRC(rc);
    /* init order hack. */
    if (!pVM->pgm.s.pPoolR3->pfnAccessHandlerR0)
    {
        rc = PDMR3LdrGetSymbolR0(pVM, NULL, "pgmPoolAccessHandler", &pVM->pgm.s.pPoolR3->pfnAccessHandlerR0);
        AssertReleaseRC(rc);
    }
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
    AssertReturn(pPool->cCurPages < pPool->cMaxPages, VERR_PGM_POOL_MAXED_OUT_ALREADY);

    /* With 32-bit guests and no EPT, the CR3 limits the root pages to low
       (below 4 GB) memory. */
    /** @todo change the pool to handle ROOT page allocations specially when
     *        required. */
    bool fCanUseHighMemory = HWACCMIsNestedPagingActive(pVM)
                          && HWACCMGetShwPagingMode(pVM) == PGMMODE_EPT;

    pgmLock(pVM);

    /*
     * How much to grow it by?
     */
    uint32_t cPages = pPool->cMaxPages - pPool->cCurPages;
    cPages = RT_MIN(PGMPOOL_CFG_MAX_GROW, cPages);
    LogFlow(("PGMR3PoolGrow: Growing the pool by %d (%#x) pages. fCanUseHighMemory=%RTbool\n", cPages, cPages, fCanUseHighMemory));

    for (unsigned i = pPool->cCurPages; cPages-- > 0; i++)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[i];

        if (fCanUseHighMemory)
            pPage->pvPageR3 = MMR3PageAlloc(pVM);
        else
            pPage->pvPageR3 = MMR3PageAllocLow(pVM);
        if (!pPage->pvPageR3)
        {
            Log(("We're out of memory!! i=%d fCanUseHighMemory=%RTbool\n", i, fCanUseHighMemory));
            pgmUnlock(pVM);
            return i ? VINF_SUCCESS : VERR_NO_PAGE_MEMORY;
        }
        pPage->Core.Key  = MMPage2Phys(pVM, pPage->pvPageR3);
        AssertFatal(pPage->Core.Key < _4G || fCanUseHighMemory);
        pPage->GCPhys    = NIL_RTGCPHYS;
        pPage->enmKind   = PGMPOOLKIND_FREE;
        pPage->idx       = pPage - &pPool->aPages[0];
        LogFlow(("PGMR3PoolGrow: insert page #%#x - %RHp\n", pPage->idx, pPage->Core.Key));
        pPage->iNext     = pPool->iFreeHead;
        pPage->iUserHead = NIL_PGMPOOL_USER_INDEX;
        pPage->iModifiedNext  = NIL_PGMPOOL_IDX;
        pPage->iModifiedPrev  = NIL_PGMPOOL_IDX;
        pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
        pPage->iMonitoredNext = NIL_PGMPOOL_IDX;
        pPage->iAgeNext  = NIL_PGMPOOL_IDX;
        pPage->iAgePrev  = NIL_PGMPOOL_IDX;
        /* commit it */
        bool fRc = RTAvloHCPhysInsert(&pPool->HCPhysTree, &pPage->Core); Assert(fRc); NOREF(fRc);
        pPool->iFreeHead = i;
        pPool->cCurPages = i + 1;
    }

    pgmUnlock(pVM);
    Assert(pPool->cCurPages <= pPool->cMaxPages);
    return VINF_SUCCESS;
}



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
 * @returns VINF_SUCCESS if the handler has carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) pgmR3PoolAccessHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf,
                                                PGMACCESSTYPE enmAccessType, void *pvUser)
{
    STAM_PROFILE_START(&pVM->pgm.s.pPoolR3->StatMonitorR3, a);
    PPGMPOOL        pPool = pVM->pgm.s.pPoolR3;
    PPGMPOOLPAGE    pPage = (PPGMPOOLPAGE)pvUser;
    PVMCPU          pVCpu = VMMGetCpu(pVM);
    LogFlow(("pgmR3PoolAccessHandler: GCPhys=%RGp %p:{.Core=%RHp, .idx=%d, .GCPhys=%RGp, .enmType=%d}\n",
             GCPhys, pPage, pPage->Core.Key, pPage->idx, pPage->GCPhys, pPage->enmKind));

    NOREF(pvBuf); NOREF(enmAccessType);

    /*
     * We don't have to be very sophisticated about this since there are relativly few calls here.
     * However, we must try our best to detect any non-cpu accesses (disk / networking).
     *
     * Just to make life more interesting, we'll have to deal with the async threads too.
     * We cannot flush a page if we're in an async thread because of REM notifications.
     */
    pgmLock(pVM);
    if (PHYS_PAGE_ADDRESS(GCPhys) != PHYS_PAGE_ADDRESS(pPage->GCPhys))
    {
        /* Pool page changed while we were waiting for the lock; ignore. */
        Log(("CPU%d: pgmR3PoolAccessHandler pgm pool page for %RGp changed (to %RGp) while waiting!\n", pVCpu->idCpu, PHYS_PAGE_ADDRESS(GCPhys), PHYS_PAGE_ADDRESS(pPage->GCPhys)));
        pgmUnlock(pVM);
        return VINF_PGM_HANDLER_DO_DEFAULT;
    }

    Assert(pPage->enmKind != PGMPOOLKIND_FREE);

    /* @todo this code doesn't make any sense. remove the if (!pVCpu) block */
    if (!pVCpu) /** @todo This shouldn't happen any longer, all access handlers will be called on an EMT. All ring-3 handlers, except MMIO, already own the PGM lock. @bugref{3170} */
    {
        Log(("pgmR3PoolAccessHandler: async thread, requesting EMT to flush the page: %p:{.Core=%RHp, .idx=%d, .GCPhys=%RGp, .enmType=%d}\n",
             pPage, pPage->Core.Key, pPage->idx, pPage->GCPhys, pPage->enmKind));
        STAM_COUNTER_INC(&pPool->StatMonitorR3Async);
        if (!pPage->fReusedFlushPending)
        {
            pgmUnlock(pVM);
            int rc = VMR3ReqCallVoidNoWait(pPool->pVMR3, VMCPUID_ANY, (PFNRT)pgmR3PoolFlushReusedPage, 2, pPool, pPage);
            AssertRCReturn(rc, rc);
            pgmLock(pVM);
            pPage->fReusedFlushPending = true;
            pPage->cModifications += 0x1000;
        }

        pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhys, pvPhys, 0 /* unknown write size */);
        /** @todo r=bird: making unsafe assumption about not crossing entries here! */
        while (cbBuf > 4)
        {
            cbBuf -= 4;
            pvPhys = (uint8_t *)pvPhys + 4;
            GCPhys += 4;
            pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhys, pvPhys, 0 /* unknown write size */);
        }
        STAM_PROFILE_STOP(&pPool->StatMonitorR3, a);
    }
    else if (    (   pPage->cModifications < 96 /* it's cheaper here. */
                  || pgmPoolIsPageLocked(pPage)
                  )
             &&  cbBuf <= 4)
    {
        /* Clear the shadow entry. */
        if (!pPage->cModifications++)
            pgmPoolMonitorModifiedInsert(pPool, pPage);
        /** @todo r=bird: making unsafe assumption about not crossing entries here! */
        pgmPoolMonitorChainChanging(pVCpu, pPool, pPage, GCPhys, pvPhys, 0 /* unknown write size */);
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


/**
 * Rendezvous callback used by pgmR3PoolClearAll that clears all shadow pages
 * and all modification counters.
 *
 * This is only called on one of the EMTs while the other ones are waiting for
 * it to complete this function.
 *
 * @returns VINF_SUCCESS (VBox strict status code).
 * @param   pVM     The VM handle.
 * @param   pVCpu   The VMCPU for the EMT we're being called on. Unused.
 * @param   fpvFlushRemTlb  When not NULL, we'll flush the REM TLB as well.
 *                          (This is the pvUser, so it has to be void *.)
 *
 */
DECLCALLBACK(VBOXSTRICTRC) pgmR3PoolClearAllRendezvous(PVM pVM, PVMCPU pVCpu, void *fpvFlushRemTbl)
{
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    STAM_PROFILE_START(&pPool->StatClearAll, c);
    NOREF(pVCpu);

    pgmLock(pVM);
    Log(("pgmR3PoolClearAllRendezvous: cUsedPages=%d fpvFlushRemTbl=%RTbool\n", pPool->cUsedPages, !!fpvFlushRemTbl));

    /*
     * Iterate all the pages until we've encountered all that are in use.
     * This is a simple but not quite optimal solution.
     */
    unsigned cModifiedPages = 0; NOREF(cModifiedPages);
    unsigned cLeft = pPool->cUsedPages;
    uint32_t iPage = pPool->cCurPages;
    while (--iPage >= PGMPOOL_IDX_FIRST)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[iPage];
        if (pPage->GCPhys != NIL_RTGCPHYS)
        {
            switch (pPage->enmKind)
            {
                /*
                 * We only care about shadow page tables that reference physical memory
                 */
#ifdef PGM_WITH_LARGE_PAGES
                case PGMPOOLKIND_EPT_PD_FOR_PHYS: /* Large pages reference 2 MB of physical memory, so we must clear them. */
                    if (pPage->cPresent)
                    {
                        PX86PDPAE pShwPD = (PX86PDPAE)PGMPOOL_PAGE_2_PTR_V2(pPool->CTX_SUFF(pVM), pVCpu, pPage);
                        for (unsigned i = 0; i < RT_ELEMENTS(pShwPD->a); i++)
                        {
                            if (    pShwPD->a[i].n.u1Present
                                &&  pShwPD->a[i].b.u1Size)
                            {
                                Assert(!(pShwPD->a[i].u & PGM_PDFLAGS_MAPPING));
                                pShwPD->a[i].u = 0;
                                Assert(pPage->cPresent);
                                pPage->cPresent--;
                            }
                        }
                        if (pPage->cPresent == 0)
                            pPage->iFirstPresent = NIL_PGMPOOL_PRESENT_INDEX;
                    }
                    goto default_case;

                case PGMPOOLKIND_PAE_PD_PHYS:   /* Large pages reference 2 MB of physical memory, so we must clear them. */
                    if (pPage->cPresent)
                    {
                        PEPTPD pShwPD = (PEPTPD)PGMPOOL_PAGE_2_PTR_V2(pPool->CTX_SUFF(pVM), pVCpu, pPage);
                        for (unsigned i = 0; i < RT_ELEMENTS(pShwPD->a); i++)
                        {
                            Assert((pShwPD->a[i].u & UINT64_C(0xfff0000000000f80)) == 0);
                            if (    pShwPD->a[i].n.u1Present
                                &&  pShwPD->a[i].b.u1Size)
                            {
                                Assert(!(pShwPD->a[i].u & PGM_PDFLAGS_MAPPING));
                                pShwPD->a[i].u = 0;
                                Assert(pPage->cPresent);
                                pPage->cPresent--;
                            }
                        }
                        if (pPage->cPresent == 0)
                            pPage->iFirstPresent = NIL_PGMPOOL_PRESENT_INDEX;
                    }
                    goto default_case;
#endif /* PGM_WITH_LARGE_PAGES */

                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                case PGMPOOLKIND_EPT_PT_FOR_PHYS:
                {
                    if (pPage->cPresent)
                    {
                        void *pvShw = PGMPOOL_PAGE_2_PTR_V2(pPool->CTX_SUFF(pVM), pVCpu, pPage);
                        STAM_PROFILE_START(&pPool->StatZeroPage, z);
#if 0
                        /* Useful check for leaking references; *very* expensive though. */
                        switch (pPage->enmKind)
                        {
                            case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                            case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                            case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                            case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                            case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                            {
                                bool fFoundFirst = false;
                                PPGMSHWPTPAE pPT = (PPGMSHWPTPAE)pvShw;
                                for (unsigned ptIndex = 0; ptIndex < RT_ELEMENTS(pPT->a); ptIndex++)
                                {
                                    if (pPT->a[ptIndex].u)
                                    {
                                        if (!fFoundFirst)
                                        {
                                            AssertFatalMsg(pPage->iFirstPresent <= ptIndex, ("ptIndex = %d first present = %d\n", ptIndex, pPage->iFirstPresent));
                                            if (pPage->iFirstPresent != ptIndex)
                                                Log(("ptIndex = %d first present = %d\n", ptIndex, pPage->iFirstPresent));
                                            fFoundFirst = true;
                                        }
                                        if (PGMSHWPTEPAE_IS_P(pPT->a[ptIndex]))
                                        {
                                            pgmPoolTracDerefGCPhysHint(pPool, pPage, PGMSHWPTEPAE_GET_HCPHYS(pPT->a[ptIndex]), NIL_RTGCPHYS);
                                            if (pPage->iFirstPresent == ptIndex)
                                                pPage->iFirstPresent = NIL_PGMPOOL_PRESENT_INDEX;
                                        }
                                    }
                                }
                                AssertFatalMsg(pPage->cPresent == 0, ("cPresent = %d pPage = %RGv\n", pPage->cPresent, pPage->GCPhys));
                                break;
                            }
                            default:
                                break;
                        }
#endif
                        ASMMemZeroPage(pvShw);
                        STAM_PROFILE_STOP(&pPool->StatZeroPage, z);
                        pPage->cPresent = 0;
                        pPage->iFirstPresent = NIL_PGMPOOL_PRESENT_INDEX;
                    }
                }
                /* fall thru */

#ifdef PGM_WITH_LARGE_PAGES
                default_case:
#endif
                default:
                    Assert(!pPage->cModifications || ++cModifiedPages);
                    Assert(pPage->iModifiedNext == NIL_PGMPOOL_IDX || pPage->cModifications);
                    Assert(pPage->iModifiedPrev == NIL_PGMPOOL_IDX || pPage->cModifications);
                    pPage->iModifiedNext = NIL_PGMPOOL_IDX;
                    pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
                    pPage->cModifications = 0;
                    break;

            }
            if (!--cLeft)
                break;
        }
    }

    /* swipe the special pages too. */
    for (iPage = PGMPOOL_IDX_FIRST_SPECIAL; iPage < PGMPOOL_IDX_FIRST; iPage++)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[iPage];
        if (pPage->GCPhys != NIL_RTGCPHYS)
        {
            Assert(!pPage->cModifications || ++cModifiedPages);
            Assert(pPage->iModifiedNext == NIL_PGMPOOL_IDX || pPage->cModifications);
            Assert(pPage->iModifiedPrev == NIL_PGMPOOL_IDX || pPage->cModifications);
            pPage->iModifiedNext = NIL_PGMPOOL_IDX;
            pPage->iModifiedPrev = NIL_PGMPOOL_IDX;
            pPage->cModifications = 0;
        }
    }

#ifndef DEBUG_michael
    AssertMsg(cModifiedPages == pPool->cModifiedPages, ("%d != %d\n", cModifiedPages, pPool->cModifiedPages));
#endif
    pPool->iModifiedHead = NIL_PGMPOOL_IDX;
    pPool->cModifiedPages = 0;

    /*
     * Clear all the GCPhys links and rebuild the phys ext free list.
     */
    for (PPGMRAMRANGE pRam = pPool->CTX_SUFF(pVM)->pgm.s.CTX_SUFF(pRamRangesX);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            PGM_PAGE_SET_TRACKING(pVM, &pRam->aPages[iPage], 0);
    }

    pPool->iPhysExtFreeHead = 0;
    PPGMPOOLPHYSEXT paPhysExts = pPool->CTX_SUFF(paPhysExts);
    const unsigned cMaxPhysExts = pPool->cMaxPhysExts;
    for (unsigned i = 0; i < cMaxPhysExts; i++)
    {
        paPhysExts[i].iNext = i + 1;
        paPhysExts[i].aidx[0] = NIL_PGMPOOL_IDX;
        paPhysExts[i].apte[0] = NIL_PGMPOOL_PHYSEXT_IDX_PTE;
        paPhysExts[i].aidx[1] = NIL_PGMPOOL_IDX;
        paPhysExts[i].apte[1] = NIL_PGMPOOL_PHYSEXT_IDX_PTE;
        paPhysExts[i].aidx[2] = NIL_PGMPOOL_IDX;
        paPhysExts[i].apte[2] = NIL_PGMPOOL_PHYSEXT_IDX_PTE;
    }
    paPhysExts[cMaxPhysExts - 1].iNext = NIL_PGMPOOL_PHYSEXT_INDEX;


#ifdef PGMPOOL_WITH_OPTIMIZED_DIRTY_PT
    /* Reset all dirty pages to reactivate the page monitoring. */
    /* Note: we must do this *after* clearing all page references and shadow page tables as there might be stale references to
     *       recently removed MMIO ranges around that might otherwise end up asserting in pgmPoolTracDerefGCPhysHint
     */
    for (unsigned i = 0; i < RT_ELEMENTS(pPool->aDirtyPages); i++)
    {
        PPGMPOOLPAGE pPage;
        unsigned     idxPage;

        if (pPool->aDirtyPages[i].uIdx == NIL_PGMPOOL_IDX)
            continue;

        idxPage = pPool->aDirtyPages[i].uIdx;
        AssertRelease(idxPage != NIL_PGMPOOL_IDX);
        pPage = &pPool->aPages[idxPage];
        Assert(pPage->idx == idxPage);
        Assert(pPage->iMonitoredNext == NIL_PGMPOOL_IDX && pPage->iMonitoredPrev == NIL_PGMPOOL_IDX);

        AssertMsg(pPage->fDirty, ("Page %RGp (slot=%d) not marked dirty!", pPage->GCPhys, i));

        Log(("Reactivate dirty page %RGp\n", pPage->GCPhys));

        /* First write protect the page again to catch all write accesses. (before checking for changes -> SMP) */
        int rc = PGMHandlerPhysicalReset(pVM, pPage->GCPhys & PAGE_BASE_GC_MASK);
        AssertRCSuccess(rc);
        pPage->fDirty = false;

        pPool->aDirtyPages[i].uIdx = NIL_PGMPOOL_IDX;
    }

    /* Clear all dirty pages. */
    pPool->idxFreeDirtyPage = 0;
    pPool->cDirtyPages      = 0;
#endif

    /* Clear the PGM_SYNC_CLEAR_PGM_POOL flag on all VCPUs to prevent redundant flushes. */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        pVM->aCpus[idCpu].pgm.s.fSyncFlags &= ~PGM_SYNC_CLEAR_PGM_POOL;

    /* Flush job finished. */
    VM_FF_CLEAR(pVM, VM_FF_PGM_POOL_FLUSH_PENDING);
    pPool->cPresent = 0;
    pgmUnlock(pVM);

    PGM_INVL_ALL_VCPU_TLBS(pVM);

    if (fpvFlushRemTbl)
        for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
            CPUMSetChangedFlags(&pVM->aCpus[idCpu], CPUM_CHANGED_GLOBAL_TLB_FLUSH);

    STAM_PROFILE_STOP(&pPool->StatClearAll, c);
    return VINF_SUCCESS;
}


/**
 * Clears the shadow page pool.
 *
 * @param   pVM             The VM handle.
 * @param   fFlushRemTlb    When set, the REM TLB is scheduled for flushing as
 *                          well.
 */
void pgmR3PoolClearAll(PVM pVM, bool fFlushRemTlb)
{
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, pgmR3PoolClearAllRendezvous, &fFlushRemTlb);
    AssertRC(rc);
}

/**
 * Protect all pgm pool page table entries to monitor writes
 *
 * @param   pVM         The VM handle.
 *
 * Remark: assumes the caller will flush all TLBs (!!)
 */
void pgmR3PoolWriteProtectPages(PVM pVM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    unsigned cLeft = pPool->cUsedPages;
    unsigned iPage = pPool->cCurPages;
    while (--iPage >= PGMPOOL_IDX_FIRST)
    {
        PPGMPOOLPAGE pPage = &pPool->aPages[iPage];
        if (    pPage->GCPhys != NIL_RTGCPHYS
            &&  pPage->cPresent)
        {
            union
            {
                void           *pv;
                PX86PT          pPT;
                PPGMSHWPTPAE    pPTPae;
                PEPTPT          pPTEpt;
            } uShw;
            uShw.pv = PGMPOOL_PAGE_2_PTR(pVM, pPage);

            switch (pPage->enmKind)
            {
                /*
                 * We only care about shadow page tables.
                 */
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_32BIT_PT_FOR_PHYS:
                    for (unsigned iShw = 0; iShw < RT_ELEMENTS(uShw.pPT->a); iShw++)
                    {
                        if (uShw.pPT->a[iShw].n.u1Present)
                            uShw.pPT->a[iShw].n.u1Write = 0;
                    }
                    break;

                case PGMPOOLKIND_PAE_PT_FOR_32BIT_PT:
                case PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_PT:
                case PGMPOOLKIND_PAE_PT_FOR_PAE_2MB:
                case PGMPOOLKIND_PAE_PT_FOR_PHYS:
                    for (unsigned iShw = 0; iShw < RT_ELEMENTS(uShw.pPTPae->a); iShw++)
                    {
                        if (PGMSHWPTEPAE_IS_P(uShw.pPTPae->a[iShw]))
                            PGMSHWPTEPAE_SET_RO(uShw.pPTPae->a[iShw]);
                    }
                    break;

                case PGMPOOLKIND_EPT_PT_FOR_PHYS:
                    for (unsigned iShw = 0; iShw < RT_ELEMENTS(uShw.pPTEpt->a); iShw++)
                    {
                        if (uShw.pPTEpt->a[iShw].n.u1Present)
                            uShw.pPTEpt->a[iShw].n.u1Write = 0;
                    }
                    break;

                default:
                    break;
            }
            if (!--cLeft)
                break;
        }
    }
}

#ifdef VBOX_WITH_DEBUGGER
/**
 * The '.pgmpoolcheck' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) pgmR3PoolCmdCheck(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    DBGC_CMDHLP_REQ_VM_RET(pCmdHlp, pCmd, pVM);
    DBGC_CMDHLP_ASSERT_PARSER_RET(pCmdHlp, pCmd, -1, cArgs == 0);
    uint32_t cErrors = 0;
    NOREF(paArgs);

    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    for (unsigned i = 0; i < pPool->cCurPages; i++)
    {
        PPGMPOOLPAGE    pPage     = &pPool->aPages[i];
        bool            fFirstMsg = true;

        /* Todo: cover other paging modes too. */
        if (pPage->enmKind == PGMPOOLKIND_PAE_PT_FOR_PAE_PT)
        {
            PPGMSHWPTPAE pShwPT = (PPGMSHWPTPAE)PGMPOOL_PAGE_2_PTR(pPool->CTX_SUFF(pVM), pPage);
            {
                PX86PTPAE       pGstPT;
                PGMPAGEMAPLOCK  LockPage;
                int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, pPage->GCPhys, (const void **)&pGstPT, &LockPage);     AssertReleaseRC(rc);

                /* Check if any PTEs are out of sync. */
                for (unsigned j = 0; j < RT_ELEMENTS(pShwPT->a); j++)
                {
                    if (PGMSHWPTEPAE_IS_P(pShwPT->a[j]))
                    {
                        RTHCPHYS HCPhys = NIL_RTHCPHYS;
                        rc = PGMPhysGCPhys2HCPhys(pPool->CTX_SUFF(pVM), pGstPT->a[j].u & X86_PTE_PAE_PG_MASK, &HCPhys);
                        if (   rc != VINF_SUCCESS
                            || PGMSHWPTEPAE_GET_HCPHYS(pShwPT->a[j]) != HCPhys)
                        {
                            if (fFirstMsg)
                            {
                                DBGCCmdHlpPrintf(pCmdHlp, "Check pool page %RGp\n", pPage->GCPhys);
                                fFirstMsg = false;
                            }
                            DBGCCmdHlpPrintf(pCmdHlp, "Mismatch HCPhys: rc=%Rrc idx=%d guest %RX64 shw=%RX64 vs %RHp\n", rc, j, pGstPT->a[j].u, PGMSHWPTEPAE_GET_LOG(pShwPT->a[j]), HCPhys);
                            cErrors++;
                        }
                        else if (   PGMSHWPTEPAE_IS_RW(pShwPT->a[j])
                                 && !pGstPT->a[j].n.u1Write)
                        {
                            if (fFirstMsg)
                            {
                                DBGCCmdHlpPrintf(pCmdHlp, "Check pool page %RGp\n", pPage->GCPhys);
                                fFirstMsg = false;
                            }
                            DBGCCmdHlpPrintf(pCmdHlp, "Mismatch r/w gst/shw: idx=%d guest %RX64 shw=%RX64 vs %RHp\n", j, pGstPT->a[j].u, PGMSHWPTEPAE_GET_LOG(pShwPT->a[j]), HCPhys);
                            cErrors++;
                        }
                    }
                }
                PGMPhysReleasePageMappingLock(pVM, &LockPage);
            }

            /* Make sure this page table can't be written to from any shadow mapping. */
            RTHCPHYS HCPhysPT = NIL_RTHCPHYS;
            int rc = PGMPhysGCPhys2HCPhys(pPool->CTX_SUFF(pVM), pPage->GCPhys, &HCPhysPT);
            AssertMsgRC(rc, ("PGMPhysGCPhys2HCPhys failed with rc=%d for %RGp\n", rc, pPage->GCPhys));
            if (rc == VINF_SUCCESS)
            {
                for (unsigned j = 0; j < pPool->cCurPages; j++)
                {
                    PPGMPOOLPAGE pTempPage = &pPool->aPages[j];

                    if (pTempPage->enmKind == PGMPOOLKIND_PAE_PT_FOR_PAE_PT)
                    {
                        PPGMSHWPTPAE pShwPT2 = (PPGMSHWPTPAE)PGMPOOL_PAGE_2_PTR(pPool->CTX_SUFF(pVM), pTempPage);

                        for (unsigned k = 0; k < RT_ELEMENTS(pShwPT->a); k++)
                        {
                            if (    PGMSHWPTEPAE_IS_P_RW(pShwPT2->a[k])
# ifdef PGMPOOL_WITH_OPTIMIZED_DIRTY_PT
                                &&  !pPage->fDirty
# endif
                                &&  PGMSHWPTEPAE_GET_HCPHYS(pShwPT2->a[k]) == HCPhysPT)
                            {
                                if (fFirstMsg)
                                {
                                    DBGCCmdHlpPrintf(pCmdHlp, "Check pool page %RGp\n", pPage->GCPhys);
                                    fFirstMsg = false;
                                }
                                DBGCCmdHlpPrintf(pCmdHlp, "Mismatch: r/w: GCPhys=%RGp idx=%d shw %RX64 %RX64\n", pTempPage->GCPhys, k, PGMSHWPTEPAE_GET_LOG(pShwPT->a[k]), PGMSHWPTEPAE_GET_LOG(pShwPT2->a[k]));
                                cErrors++;
                            }
                        }
                    }
                }
            }
        }
    }
    if (cErrors > 0)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "Found %#x errors", cErrors);
    return VINF_SUCCESS;
}
#endif /* VBOX_WITH_DEBUGGER */
