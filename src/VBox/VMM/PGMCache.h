/** @file
 *
 * VBox - PGM PD Cache Manager
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

#ifndef __PGMCache_h__
#define __PGMCache_h__

#if !defined(IN_PGM_R3) && !defined(IN_PGM_R0) && !defined(IN_PGM_GC)
# error "Not in PGM! This is an internal header!"
#endif

#define PGMCACHE_PHYSREG_DESCRIPTION            "PGM Cache PT write handler"

#define PGMCACHE_MAX_CACHED_PDES                32
#define PGMCACHE_MAX_CACHED_ENTRIES             8
#define PGMCACHE_INVALID_INDEX                  0xffffffff

#define PGMCACHE_FILL_THRESHOLD                 5000
//#define PGMCACHE_FILL_THRESHOLD                 500
#define PGMCACHE_INSERT_BOOST                   500

#define PGMCACHE_SYNCPT_USER_INC                10
#define PGMCACHE_SYNCPT_SUPERVISOR_INC          20
#define PGMCACHE_SYNCPAGE_USER_INC              1
#define PGMCACHE_SYNCPAGE_SUPERVISOR_INC        2
#define PGMCACHE_CACHE_HIT_INC                  50
#define PGMCACHE_LEAVEGC_DEC                    -1
#define PGMCACHE_SIGNIFICANT_PTWRITE_DEC        -5
#define PGMCACHE_PTWRITE_DEC                    -2
#define PGMCACHE_UNKNOWN_PTWRITE_DEC            -100
#define PGMCACHE_PDE_CHANGE_DEC                 -250
#define PGMCACHE_PDE_CR3_MAPPING                -2500
#define PGMCACHE_PDE_CONFLICT_DEC               -1000
#define PGMCACHE_PDE_CHANGE_HIT_INC             (PGMCACHE_CACHE_HIT_INC)
#define PGMCACHE_MAX_DIRTY_PAGES                4


/* define to enable PD caching */
//#define PGM_PD_CACHING_ENABLED
#ifdef VBOX_STRICT
/* define to enable strict PD checking */
//#define PGM_CACHE_STRICT
/* define to enable paranoid checking */
//#define PGM_CACHE_VERY_STRICT
// #define PGM_CACHE_EXTREMELY_STRICT
#endif

#define PGMCACHE_GET_ACTIVE_ENTRY(pLine) \
         (pLine->iActiveEntry != PGMCACHE_INVALID_INDEX) ? &pLine->entry[pLine->iActiveEntry] : NULL

typedef struct
{
    /* Caching score */
    unsigned    u29Score        : 29;
    /* PD is waiting to be cached */
    unsigned    u1Queued        : 1;
    /* PD is cached */
    unsigned    u1Cached        : 1;
    /* Cache disabled flag */
    unsigned    u1CacheDisabled : 1;
} PDESTAT, *PPDESTAT;

typedef struct
{
    /* Copy of guest PDE */
    X86PDEPAE   PdeGst;
    /* Shadow PT physical address */
    RTGCPHYS    PtShwPhys;

    /* Multiple dirty accesses -> flush entry */
    bool        fDirtyFlush;
    /* Dirty flag */
    bool        fDirty;
    /* Dirty state is pending until the PD is activated */
    bool        fDirtyPending;

    /* GC address of PT write */
    RTGCUINTPTR pvFaultDirty[PGMCACHE_MAX_DIRTY_PAGES];
    uint32_t    cNumDirtyPages;

    /* Cache entry score */
    uint32_t    uScore;

    /* Cache line index */
    uint32_t    iLine;
    /* Cache entry index */
    uint32_t    iEntry;
} PDCACHEENTRY, *PPDCACHEENTRY;

typedef struct
{
    /* Shadow page directory index (0-2047, PGMCACHE_INVALID_INDEX when unused) */
    uint32_t    iPDShw;
    /* Guest context address that corresponds to the shadow iPD */
    RTGCUINTPTR GCPtrPDE;

    /* Set when the cache entry is modified */
    bool        fRefresh;

    uint32_t    iPDShwNew;
    uint32_t    iPDShwOld;
    /* Currently used cache entry */
    uint32_t    iActiveEntry;
    /* Cache index line */
    uint32_t    iLine;

    PDCACHEENTRY entry[PGMCACHE_MAX_CACHED_ENTRIES];
} PDCACHELINE, *PPDCACHELINE;


typedef struct
{
    /* Cached PDEs */
    PDCACHELINE     line[PGMCACHE_MAX_CACHED_PDES];

    /* Lowest score of all cached PDEs. */
    uint32_t        ulLowestScore;
    /* Caching statistics for all PDEs. */
    PDESTAT         pd[2048];
    /* Set when a cache entry is modified. */
    bool            fChanged;
    /* Caching enabled. */
    bool            fEnabled;
    /* Max nr of host PDEs (PAE vs 32 bits). */
    uint32_t        cbMaxPDEs;
    /* PD shift for host paging. */
    uint32_t        ShwPDShift;
    /* PD size for host paging. */
    uint32_t        cbShwPD;

#ifdef DEBUG
    uint32_t        cr3;
    SUPPAGINGMODE   enmHostMode;
#endif
} PDCACHE, *PPDCACHE;

/**
 * Initialise PGM caching subsystem
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   enmHostMode Host paging mode
 */
int pgmr3CacheInit(PVM pVM, SUPPAGINGMODE enmHostMode);


/**
 * Reset PGM caching subsystem
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   enmHostMode Host paging mode
 */
int pgmr3CacheReset(PVM pVM, SUPPAGINGMODE enmHostMode);


/**
 * Inform the PGM Cache Manager when exiting the guest context
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   pCtx        Current CPU context
 * @param   u32Reason   Reason for exiting the guest context
 *                      Currently only for VINF_EM_RAW_INTERRUPT!
 */
int pgmr3CacheLeaveGC(PVM pVM, PCPUMCTX pCtx, uint32_t u32Reason);


/**
 * Inform the PGM Cache Manager about a pending Sync PD action (called
 * from PGMR3SyncPD only!)
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   cr0         Current CR0 register value
 * @param   cr3         Current CR3 register value
 * @param   cr4         Current CR4 register value
 */
int pgmr3CacheSyncPD(PVM pVM, uint32_t cr0, uint32_t cr3, uint32_t cr4);


/**
 * Mark all cache lines and entries as dirty.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 */
int pgmr3CacheGlobalFlush(PVM pVM);


#ifdef VBOX_STRICT
/**
 * Checks the guest and shadow tables for consistency.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   cr0         Current CR0 register value
 * @param   cr3         Current CR3 register value
 * @param   cr4         Current CR4 register value
 */
void pgmCacheCheckPD(PVM pVM, uint32_t cr0, uint32_t cr3, uint32_t cr4);

/**
 * Checks the cache line if it is marked dirty.
 *
 * @returns boolean; true if the line is marked dirty, otherwise false
 * @param   pVM         The virtual machine.
 * @param   iPDShw         Page directory entry
 */
bool pgmCacheIsLineDirty(PVM pVM, uint32_t iPDShw);

#else
#define pgmr3CacheCheckPD(a, b, c, d)
#define pgmCacheIsLineDirty(a, b)    false
#define pgmCacheCheckPDE(a, b)
#endif

#ifdef PGM_CACHE_EXTREMELY_STRICT
/**
 * Check cache line integrity
 *
 * @param   pVM         The virtual machine.
 * @param   pLine       Cache line
 * @param   pEntry      Cache entry
 */
void pgmCacheCheckIntegrity(PVM pVM, PPDCACHELINE pLine, PPDCACHEENTRY pEntry);
#else
#define pgmCacheCheckIntegrity(a, b, c)
#endif

/**
 * Cache PDE
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iPDShw         Page directory index
 */
int pgmCacheInsertPD(PVM pVM, uint32_t iPDShw);

/**
 * Invalidate cached PDE.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iLine       Cache line index
 * @param   penalty     Score penalty for invalidation
 */
int pgmCacheInvalidateLine(PVM pVM, uint32_t iLine, int penalty);


/**
 * Invalidate cached PDE
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iPDShw         PDE index
 * @param   penalty     Score penalty for invalidation
 */
int pgmCacheInvalidate(PVM pVM, uint32_t iPDShw, int penalty);


/**
 * Invalidate all cached PDEs
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 */
int pgmCacheInvalidateAll(PVM pVM);


/**
 * Update record for specified PD
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iPDShw         Page directory index
 * @param   delta       Score increment/decrement
 */
int pgmCacheUpdatePD(PVM pVM, uint32_t iPDShw, int delta);


/**
 * Update record for specified PD
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iLine       Cache line index
 * @param   iEntry      Cache entry index
 * @param   delta       Score increment/decrement
 */
int pgmCacheUpdateEntry(PVM pVM, uint32_t iLine, uint32_t iEntry, int delta);


/**
 * Update record for specified PD + all cache entries
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iPDShw         Page directory index
 * @param   delta       Score increment/decrement
 */
int pgmCacheUpdateAll(PVM pVM, uint32_t iPDShw, int delta);


/**
 * Remove the handler for the supplied physical page
 * (@todo maybe this should be a physical range instead )
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   GCPhys      GC physical address
 */
int pgmCacheRemovePhyshandler(PVM pVM, RTGCPHYS GCPhys);


/**
 * Check if a page directory entry is cached
 *
 * @returns true if cached, otherwise false
 * @param   pPdCache    PGM cache record pointer
 * @param   iPDShw         Page directory index
 */
inline bool pgmCacheIsPDCached(PPDCACHE pPdCache, uint32_t iPDShw)
{
#ifdef PGM_PD_CACHING_ENABLED
    if (iPDShw < pPdCache->cbMaxPDEs)
    {
        return !!pPdCache->pd[iPDShw].u1Cached;
    }
#endif
    return false;
}


/**
 * Check if a page directory entry is cached
 *
 * @returns true if cached, otherwise false
 * @param   pVM         The virtual machine.
 * @param   iPDShw         Page directory index
 */
bool pgmCacheIsPDCached(PVM pVM, uint32_t iPDShw);

/**
 * Checks if the PDE has been changed
 *
 * @returns boolean
 * @param   pVM         The virtual machine.
 * @param   iPDShw      Page directory entry
 */
bool pgmCacheHasPDEChanged(PVM pVM, uint32_t iPDShw);

/**
 * Checks if the PDE has been changed
 *
 * @returns boolean
 * @param   pVM         The virtual machine.
 * @param   pLine       Cache line
 */
bool pgmCacheHasPDEChanged(PVM pVM, PPDCACHELINE pLine);

/**
 * Invalidate entry in shadow page directory
 *
 * @returns VBox status code.
 * @param   iPDShw         PDE index
 * @param   iPDShw         Page directory index
 */
void pgmInvalidatePD(PVM pVM, uint32_t iPDShw);


/**
 * Activate a cached PD (called by SyncPT when it is synced for the first time).
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   iPDShw     PDE index
 */
int pgmCacheActivatePD(PVM pVM, uint32_t iPDShw);

/**
 * Clean up dirty cache entry
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   pLine       Cache line pointer
 * @param   pEntry      Cache entry pointer
 */
int pgmCacheHandleDirtyEntry(PVM pVM, PPDCACHELINE pLine, PPDCACHEENTRY pEntry);

DECLEXPORT(int) pgmCachePTWrite(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);

#endif /* !__PGMCache_h__ */

