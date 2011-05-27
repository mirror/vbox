/* $Id$ */
/** @file
 * GMM - The Global Memory Manager, Internal Header.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___GMMR0Internal_h
#define ___GMMR0Internal_h

#include <VBox/vmm/gmm.h>
#include <iprt/avl.h>

/**
 * The allocation sizes.
 */
typedef struct GMMVMSIZES
{
    /** The number of pages of base memory.
     * This is the sum of RAM, ROMs and handy pages. */
    uint64_t        cBasePages;
    /** The number of pages for the shadow pool. (Can be squeezed for memory.) */
    uint32_t        cShadowPages;
    /** The number of pages for fixed allocations like MMIO2 and the hyper heap. */
    uint32_t        cFixedPages;
} GMMVMSIZES;
/** Pointer to a GMMVMSIZES. */
typedef GMMVMSIZES *PGMMVMSIZES;


/**
 * Shared module registration info (per VM)
 */
typedef struct GMMSHAREDMODULEPERVM
{
    /** Tree node. */
    AVLGCPTRNODECORE            Core;

    /** Pointer to global shared module info. */
    PGMMSHAREDMODULE            pGlobalModule;

    /** Set if another VM registered a different shared module at the same base address. */
    bool                        fCollision;
    /** Alignment. */
    bool                        bAlignment[3];

    /** Number of included region descriptors */
    uint32_t                    cRegions;

    /** Shared region descriptor(s). */
    GMMSHAREDREGIONDESC         aRegions[1];
} GMMSHAREDMODULEPERVM;
/** Pointer to a GMMSHAREDMODULEPERVM. */
typedef GMMSHAREDMODULEPERVM *PGMMSHAREDMODULEPERVM;


/** Pointer to a GMM allocation chunk. */
typedef struct GMMCHUNK *PGMMCHUNK;


/** The GMMCHUNK::cFree shift count employed by gmmR0SelectFreeSetList. */
#define GMM_CHUNK_FREE_SET_SHIFT    4
/** Index of the list containing completely unused chunks.
 * The code ASSUMES this is the last list. */
#define GMM_CHUNK_FREE_SET_UNUSED_LIST  (GMM_CHUNK_NUM_PAGES >> GMM_CHUNK_FREE_SET_SHIFT)

/**
 * A set of free chunks.
 */
typedef struct GMMCHUNKFREESET
{
    /** The number of free pages in the set. */
    uint64_t            cFreePages;
    /** The generation ID for the set.  This is incremented whenever
     *  something is linked or unlinked from this set. */
    uint64_t            idGeneration;
    /** Chunks ordered by increasing number of free pages.
     *  In the final list the chunks are completely unused. */
    PGMMCHUNK           apLists[GMM_CHUNK_FREE_SET_UNUSED_LIST + 1];
} GMMCHUNKFREESET;



/**
 * The per-VM GMM data.
 */
typedef struct GMMPERVM
{
    /** Free set for use in bound mode. */
    GMMCHUNKFREESET     Private;

    /** The reservations. */
    GMMVMSIZES          Reserved;
    /** The actual allocations.
     * This includes both private and shared page allocations. */
    GMMVMSIZES          Allocated;

    /** The current number of private pages. */
    uint64_t            cPrivatePages;
    /** The current number of shared pages. */
    uint64_t            cSharedPages;
    /** The current over-commitment policy. */
    GMMOCPOLICY         enmPolicy;
    /** The VM priority for arbitrating VMs in low and out of memory situation.
     * Like which VMs to start squeezing first. */
    GMMPRIORITY         enmPriority;

    /** The current number of ballooned pages. */
    uint64_t            cBalloonedPages;
    /** The max number of pages that can be ballooned. */
    uint64_t            cMaxBalloonedPages;
    /** The number of pages we've currently requested the guest to give us.
     * This is 0 if no pages currently requested. */
    uint64_t            cReqBalloonedPages;
    /** The number of pages the guest has given us in response to the request.
     * This is not reset on request completed and may be used in later decisions. */
    uint64_t            cReqActuallyBalloonedPages;
    /** The number of pages we've currently requested the guest to take back. */
    uint64_t            cReqDeflatePages;

    /** Shared module tree (per-vm). */
    PAVLGCPTRNODECORE   pSharedModuleTree;

    /** Whether ballooning is enabled or not. */
    bool                fBallooningEnabled;

    /** Whether shared paging is enabled or not. */
    bool                fSharedPagingEnabled;

    /** Whether the VM is allowed to allocate memory or not.
     * This is used when the reservation update request fails or when the VM has
     * been told to suspend/save/die in an out-of-memory case. */
    bool                fMayAllocate;
} GMMPERVM;
/** Pointer to the per-VM GMM data. */
typedef GMMPERVM *PGMMPERVM;

#endif

