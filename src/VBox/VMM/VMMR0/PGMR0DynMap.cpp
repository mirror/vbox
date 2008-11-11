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
    /** The number of references. */
    uint32_t volatile           cRefs;
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
    RTSPINLOCK                  hSpinLock;
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
static PPGMR0DYNMAP *g_pPGMR0DynMap;




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
 * Signals the start of a new set of mappings.
 *
 * Mostly for strictness. PGMDynMapHCPage won't work unless this
 * API is called.
 *
 * @param   pVCpu       The shared data for the current virtual CPU.
 */
VMMDECL(void) PGMDynMapStartAutoSet(PVMCPU pVCpu)
{

}


/**
 * Releases the dynamic memory mappings made by PGMDynMapHCPage and associates
 * since the PGMDynMapStartAutoSet call.
 *
 * @param   pVCpu       The shared data for the current virtual CPU.
 */
VMMDECL(void) PGMDynMapReleaseAutoSet(PVMCPU pVCpu)
{

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

}


/* documented elsewhere - a bit of a mess. */
VMMDECL(int) PGMDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    AssertMsg(!(HCPhys & PAGE_OFFSET_MASK), ("HCPhys=%RHp\n", HCPhys));
    AssertMsgFailed(("Not implemented\n"));
    return VERR_NOT_IMPLEMENTED;
}


