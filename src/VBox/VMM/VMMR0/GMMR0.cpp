/* $Id$ */
/** @file
 * GMM - Global Memory Manager.
 */

/*
 * Copyright (C) 2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

/** @page pg_gmm    GMM - The Global Memory Manager
 *
 * As the name indicates, this component is responsible for global memory
 * management. Currently only guest RAM is allocated from the GMM, but this
 * may change to include shadow page tables and other bits later.
 *
 * Guest RAM is managed as individual pages, but allocated from the host OS
 * in chunks for reasons of portability / efficiency. To minimize the memory
 * footprint all tracking structure must be as small as possible without
 * unnecessary performance penalties.
 *
 *
 * The allocation chunks has fixed sized, the size defined at compile time
 * by the GMM_CHUNK_SIZE \#define.
 *
 * Each chunk is given an unquie ID. Each page also has a unique ID. The
 * relation ship between the two IDs is:
 * @verbatim
       (idChunk << GMM_CHUNK_SHIFT) | iPage
 @endverbatim
 * Where GMM_CHUNK_SHIFT is log2(GMM_CHUNK_SIZE / PAGE_SIZE) and iPage is
 * the index of the page within the chunk. This ID scheme permits for efficient
 * chunk and page lookup, but it relies on the chunk size to be set at compile
 * time. The chunks are organized in an AVL tree with their IDs being the keys.
 *
 * The physical address of each page in an allocation chunk is maintained by
 * the RTR0MEMOBJ and obtained using RTR0MemObjGetPagePhysAddr. There is no
 * need to duplicate this information (it'll cost 8-bytes per page if we did).
 *
 * So what do we need to track per page? Most importantly we need to know what
 * state the page is in:
 *   - Private - Allocated for (eventually) backing one particular VM page.
 *   - Shared  - Readonly page that is used by one or more VMs and treated
 *               as COW by PGM.
 *   - Free    - Not used by anyone.
 *
 * For the page replacement operations (sharing, defragmenting and freeing)
 * to be somewhat efficient, private pages needs to be associated with a
 * particular page in a particular VM.
 *
 * Tracking the usage of shared pages is impractical and expensive, so we'll
 * settle for a reference counting system instead.
 *
 * Free pages will be chained on LIFOs
 *
 * On 64-bit systems we will use a 64-bit bitfield per page, while on 32-bit
 * systems a 32-bit bitfield will have to suffice because of address space
 * limitations. The GMMPAGE structure shows the details.
 *
 *
 * @section sec_gmm_costs       Page Allocation Strategy
 *
 * The strategy for allocating pages has to take fragmentation and shared
 * pages into account, or we may end up with with 2000 chunks with only
 * a few pages in each. The fragmentation wrt shared pages is that unlike
 * private pages they cannot easily be reallocated. Private pages can be
 * reallocated by a defragmentation thread in the same manner that sharing
 * is done.
 *
 * The first approach is to manage the free pages in two sets depending on
 * whether they are mainly for the allocation of shared or private pages.
 * In the initial implementation there will be almost no possibility for
 * mixing shared and private pages in the same chunk (only if we're really
 * stressed on memory), but when we implement forking of VMs and have to
 * deal with lots of COW pages it'll start getting kind of interesting.
 *
 * The sets are lists of chunks with approximately the same number of
 * free pages. Say the chunk size is 1MB, meaning 256 pages, and a set
 * consists of 16 lists. So, the first list will contain the chunks with
 * 1-7 free pages, the second covers 8-15, and so on. The chunks will be
 * moved between the lists as pages are freed up or allocated.
 *
 *
 * @section sec_gmm_costs       Costs
 *
 * The per page cost in kernel space is 32-bit plus whatever RTR0MEMOBJ
 * entails. In addition there is the chunk cost of approximately
 * (sizeof(RT0MEMOBJ) + sizof(CHUNK)) / 2^CHUNK_SHIFT bytes per page.
 *
 * On Windows the per page RTR0MEMOBJ cost is 32-bit on 32-bit windows
 * and 64-bit on 64-bit windows (a PFN_NUMBER in the MDL). So, 64-bit per page.
 * The cost on Linux is identical, but here it's because of sizeof(struct page *).
 *
 *
 * @section sec_gmm_legacy      Legacy Mode for Non-Tier-1 Platforms
 *
 * In legacy mode the page source is locked user pages and not
 * RTR0MemObjAllocPhysNC, this means that a page can only be allocated
 * by the VM that locked it. We will make no attempt at implementing
 * page sharing on these systems, just do enough to make it all work.
 *
 *
 * @subsection sub_gmm_locking  Serializing
 *
 * One simple fast mutex will be employed in the initial implementation, not
 * two as metioned in @ref subsec_pgmPhys_Serializing.
 *
 * @see subsec_pgmPhys_Serializing
 *
 *
 * @section sec_gmm_overcommit  Memory Over-Commitment Management
 *
 * The GVM will have to do the system wide memory over-commitment
 * management. My current ideas are:
 *      - Per VM oc policy that indicates how much to initially commit
 *        to it and what to do in a out-of-memory situation.
 *      - Prevent overtaxing the host.
 *
 * There are some challenges here, the main ones are configurability and
 * security. Should we for instance permit anyone to request 100% memory
 * commitment? Who should be allowed to do runtime adjustments of the
 * config. And how to prevent these settings from being lost when the last
 * VM process exits? The solution is probably to have an optional root
 * daemon the will keep VMMR0.r0 in memory and enable the security measures.
 *
 * This will not be implemented this week. :-)
 *
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "../PGMInternal.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The per-page tracking structure employed by the GMM.
 *
 * On 32-bit hosts we'll some trickery is necessary to compress all
 * the information into 32-bits. When the fSharedFree member is set,
 * the 30th bit decides whether it's a free page or not.
 *
 * Because of the different layout on 32-bit and 64-bit hosts, macros
 * are used to set and get the data.
 */
typedef union GMMPAGE
{
#if HC_ARCH_BITS == 64
    /** Unsigned integer view. */
    uint64_t u;

    /** The common view. */
    struct GMMPAGECOMMON
    {
        uint32_t uStuff1 : 32;
        uint32_t uStuff2 : 20;
        /** The page state. */
        uint32_t    u2State : 2;
    } Common;

    /** The view of a private page. */
    struct GMMPAGEPRIVATE
    {
        /** The guest page frame number. (Max addressable: 2 ^ 44) */
        uint32_t    pfn;
        /** The GVM handle. (64K VMs) */
        uint32_t    iGVM : 16;
        /** Reserved. */
        uint32_t    u16Reserved : 14;
        /** The page state. */
        uint32_t    u2State : 2;
    } Private;

    /** The view of a shared page. */
    struct GMMPAGESHARED
    {
        /** The reference count. */
        uint32_t    cRefs;
        /** Reserved. Checksum or something? Two iGVMs for forking? */
        uint32_t    u30Reserved : 30;
        /** The page state. */
        uint32_t    u2State : 2;
    } Shared;

    /** The view of a free page. */
    struct GMMPAGEFREE
    {
        /** The id of the next page in the free list. */
        uint32_t    idNext;
        /** Reserved. Checksum or something? */
        uint32_t    u30Reserved : 30;
        /** The page state. */
        uint32_t    u2State : 2;
    } Free;

#else /* 32-bit */
    /** The common view. */
    struct GMMPAGECOMMON
    {
        uint32_t    uStuff : 30;
        /** The page state. */
        uint32_t    u2State : 2;
    } Common;

    /** The view of a private page. */
    struct GMMPAGEPRIVATE
    {
        /** The guest page frame number. (Max addressable: 2 ^ 36) */
        uint32_t    pfn : 24;
        /** The GVM handle. (127 VMs) */
        uint32_t    iGVM : 7;
        /** The top page state bit, MBZ. */
        uint32_t    fZero : 1;
    } Private;

    /** The view of a shared page. */
    struct GMMPAGESHARED
    {
        /** The reference count. */
        uint32_t    cRefs : 30;
        /** The page state. */
        uint32_t    u2State : 2;
    } Shared;

    /** The view of a free page. */
    struct GMMPAGEFREE
    {
        /** The id of the next page in the free list. */
        uint32_t    idNext;
        /** Reserved. Checksum or something? */
        uint32_t    u30Reserved : 30;
        /** The page state. */
        uint32_t    u2State : 2;
    } Free;
#endif
} GMMPAGE;
/** Pointer to a GMMPAGE. */
typedef GMMPAGE *PGMMPAGE;


/** @name The Page States.
 * @{ */
/** A private page. */
#define GMM_PAGE_STATE_PRIVATE          0
/** A private page - alternative value used on the 32-bit implemenation.
 * This will never be used on 64-bit hosts. */
#define GMM_PAGE_STATE_PRIVATE_32       1
/** A shared page. */
#define GMM_PAGE_STATE_SHARED           2
/** A free page. */
#define GMM_PAGE_STATE_FREE             3
/** @} */


/** @def GMMPAGE_IS_PRIVATE
 *
 * @returns true if free, false if not.
 * @param   pPage       The GMM page.
 */
#if HC_ARCH_BITS == 64
# define GMM_PAGE_IS_PRIVATE(pPage) ( (pPage)->Common.u2State == GMM_PAGE_STATE_PRIVATE )
#else
# define GMM_PAGE_IS_PRIVATE(pPage) ( (pPage)->Private.fZero == 0 )
#endif

/** @def GMMPAGE_IS_FREE
 *
 * @returns true if free, false if not.
 * @param   pPage       The GMM page.
 */
#define GMM_PAGE_IS_SHARED(pPage)   ( (pPage)->Common.u2State == GMM_PAGE_STATE_SHARED )

/** @def GMMPAGE_IS_FREE
 *
 * @returns true if free, false if not.
 * @param   pPage       The GMM page.
 */
#define GMM_PAGE_IS_FREE(pPage)     ( (pPage)->Common.u2State == GMM_PAGE_STATE_FREE )


/**
 * A GMM allocation chunk ring-3 mapping record.
 *
 * This should really be associated with a session and not a VM, but
 * it's simpler to associated with a VM and cleanup with the VM object
 * is destroyed.
 */
typedef struct GMMCHUNKMAP
{
    /** The mapping object. */
    RTR0MEMOBJ      MapObj;
    /** The VM owning the mapping. */
    PVM             pVM;
} GMMCHUNKMAP;
/** Pointer to a GMM allocation chunk mapping. */
typedef struct GMMCHUNKMAP *PGMMCHUNKMAP;


/** Pointer to a GMM allocation chunk. */
typedef struct GMMCHUNK *PGMMCHUNK;

/**
 * A GMM allocation chunk.
 */
typedef struct GMMCHUNK
{
    /** The AVL node core.
     * The Key is the chunk ID. */
    AVLU32NODECORE  Core;
    /** The memory object.
     * This is either a */
    RTR0MEMOBJ      MemObj;
    /** Pointer to the next chunk in the free list. */
    PGMMCHUNK       pFreeNext;
    /** Pointer to the previous chunk in the free list. */
    PGMMCHUNK       pFreePrev;
    /** Pointer to an array of mappings. */
    PGMMCHUNKMAP    paMappings;
    /** The number of mappings. */
    uint16_t        cMappings;
    /** The head of the list of free pages. */
    uint16_t        idFreeHead;
    /** The number of free pages. */
    uint16_t        cFree;
    /** The GVM handle of the VM that first allocated pages from this chunk, this
     * is used as a preference when there are several chunks to choose from.
     * When in legacy mode this isn't a preference any longer. */
    uint16_t        iGVM;
    /** The number of private pages. */
    uint16_t        cPrivate;
    /** The number of shared pages. */
    uint16_t        cShared;
    /** Reserved for later. */
    uint16_t        au16Reserved;
    /** The pages. */
    GMMPAGE         aPages[GMM_CHUNK_SIZE >> PAGE_SIZE];
} GMMCHUNK;


/**
 * An allocation chunk TLB entry.
 */
typedef struct GMMCHUNKTLBE
{
    /** The chunk id. */
    uint32_t        idChunk;
    /** Pointer to the chunk. */
    PGGMCHUNK       pChunk;
} GMMCHUNKTLBE;
/** Pointer to an allocation chunk TLB entry. */
typedef GMMCHUNKTLBE *PGMMCHUNKTLBE;


/**
 * An allocation chunk TLB.
 */
typedef struct GMMCHUNKTLB
{
    /** The TLB entries. */
    GMMCHUNKTLBE    aEntries[32];
} GMMCHUNKTLB;
/** Pointer to an allocation chunk TLB. */
typedef GMMCHUNKTLB *PGMMCHUNKTLB;


/**
 * A set of free chunks.
 */
typedef struct GMMCHUNKFREESET
{
    /** The number of free pages in the set. */
    uint64_t        cPages;
    /**  */
    PGMMCHUNK       apLists[16];
} GMMCHUNKFREESET;
/** Pointer to set of free chunks.  */
typedef GMMCHUNKFREESET *PGMMCHUNKFREESET;


/**
 * The GMM instance data.
 */
typedef struct GMM
{
    /** The fast mutex protecting the GMM.
     * More fine grained locking can be implemented later if necessary. */
    RTSEMFASTMUTEX      Mtx;
    /** The chunk tree. */
    PAVLU32NODECORE     pChunks;
    /** The chunk TLB. */
    GMMCHUNKTLB         ChunkTLB;
    /** The private free set. */
    GMMCHUNKFREESET     Private;
    /** The shared free set. */
    GMMCHUNKFREESET     Shared;

    /** The number of allocated pages. */
    uint64_t            cPages;
    /** The legacy mode indicator.
     * This is determined at initialization time. */
    bool                fLegacyMode;
} GMM;
/** Pointer to the GMM instance. */
typedef GMM *PGMM;


