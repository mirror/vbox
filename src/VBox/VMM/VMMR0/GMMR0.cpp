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
#define LOG_GROUP LOG_GROUP_GMM
#include <VBox/gmm.h>
#include "GMMR0Internal.h"
#include <VBox/gvm.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <iprt/avl.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>


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
 * are used to get and set some of the data.
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
        uint32_t    hGVM : 16;
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
        /** Reserved. Checksum or something? Two hGVMs for forking? */
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
    /** Unsigned integer view. */
    uint32_t u;

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
        uint32_t    hGVM : 7;
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
    uint16_t        hGVM;
    /** The number of private pages. */
    uint16_t        cPrivate;
    /** The number of shared pages. */
    uint16_t        cShared;
    /** Reserved for later. */
    uint16_t        au16Reserved;
    /** The pages. */
    GMMPAGE         aPages[GMM_CHUNK_SIZE >> PAGE_SHIFT];
} GMMCHUNK;


/**
 * An allocation chunk TLB entry.
 */
typedef struct GMMCHUNKTLBE
{
    /** The chunk id. */
    uint32_t        idChunk;
    /** Pointer to the chunk. */
    PGMMCHUNK       pChunk;
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
    /** Magic / eye catcher. GMM_MAGIC */
    uint32_t            u32Magic;
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
    /** The number of active VMs. */
    uint16_t            cActiveVMs;
} GMM;
/** Pointer to the GMM instance. */
typedef GMM *PGMM;

/** The value of GMM::u32Magic (Katsuhiro Otomo). */
#define GMM_MAGIC       0x19540414


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the GMM instance data. */
static PGMM g_pGMM = NULL;

/** Macro for obtaining and validating the g_pGMM pointer.
 * On failure it will return from the invoking function with the specified return value.
 *
 * @param   pGMM    The name of the pGMM variable.
 * @param   rc      The return value on failure. Use VERR_INTERNAL_ERROR for
 *                  VBox status codes.
 */
#define GMM_GET_VALID_INSTANCE(pGMM, rc) \
    do { \
        (pGMM) = g_pGMM; \
        AssertPtrReturn((pGMM), (rc)); \
        AssertMsgReturn((pGMM)->u32Magic == GMM_MAGIC, ("%p - %#x\n", (pGMM), (pGMM)->u32Magic), (rc)); \
    } while (0)

/** Macro for obtaining and validating the g_pGMM pointer, void function variant.
 * On failure it will return from the invoking function.
 *
 * @param   pGMM    The name of the pGMM variable.
 */
#define GMM_GET_VALID_INSTANCE_VOID(pGMM) \
    do { \
        (pGMM) = g_pGMM; \
        AssertPtrReturnVoid((pGMM)); \
        AssertMsgReturnVoid((pGMM)->u32Magic == GMM_MAGIC, ("%p - %#x\n", (pGMM), (pGMM)->u32Magic)); \
    } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) gmmR0TermDestroyChunk(PAVLU32NODECORE pNode, void *pvGMM);
static DECLCALLBACK(int) gmmR0FreeVMPagesInChunk(PAVLU32NODECORE pNode, void *pvhGVM);



/**
 * Initializes the GMM component.
 *
 * This is called when the VMMR0.r0 module is loaded and protected by the
 * loader semaphore.
 *
 * @returns VBox status code.
 */
GMMR0DECL(int) GMMR0Init(void)
{
    LogFlow(("GMMInit:\n"));

    /*
     * Allocate the instance data and the lock(s).
     */
    PGMM pGMM = (PGMM)RTMemAllocZ(sizeof(*pGMM));
    if (!pGMM)
        return VERR_NO_MEMORY;
    pGMM->u32Magic = GMM_MAGIC;
    for (unsigned i = 0; i < RT_ELEMENTS(pGMM->ChunkTLB.aEntries); i++)
        pGMM->ChunkTLB.aEntries[i].idChunk = NIL_GMM_CHUNKID;

    int rc = RTSemFastMutexCreate(&pGMM->Mtx);
    if (RT_SUCCESS(rc))
    {
        /*
         * Check and see if RTR0MemObjAllocPhysNC works.
         */
        RTR0MEMOBJ MemObj;
        rc = RTR0MemObjAllocPhysNC(&MemObj, _64K, NIL_RTHCPHYS);
        if (RT_SUCCESS(rc))
        {
            rc = RTR0MemObjFree(MemObj, true);
            AssertRC(rc);
        }
        else if (rc == VERR_NOT_SUPPORTED)
            pGMM->fLegacyMode = true;
        else
            SUPR0Printf("GMMR0Init: RTR0MemObjAllocPhysNC(,64K,Any) -> %d!\n", rc);

        g_pGMM = pGMM;
        LogFlow(("GMMInit: pGMM=%p fLegacy=%RTbool\n", pGMM, pGMM->fLegacyMode));
        return VINF_SUCCESS;
    }

    RTMemFree(pGMM);
    SUPR0Printf("GMMR0Init: failed! rc=%d\n", rc);
    return rc;
}


/**
 * Terminates the GMM component.
 */
GMMR0DECL(void) GMMR0Term(void)
{
    LogFlow(("GMMTerm:\n"));

    /*
     * Take care / be paranoid...
     */
    PGMM pGMM = g_pGMM;
    if (!VALID_PTR(pGMM))
        return;
    if (pGMM->u32Magic != GMM_MAGIC)
    {
        SUPR0Printf("GMMR0Term: u32Magic=%#x\n", pGMM->u32Magic);
        return;
    }

    /*
     * Undo what init did and free any resources we've acquired.
     */
    /* Destroy the fundamentals. */
    g_pGMM = NULL;
    pGMM->u32Magic++;
    RTSemFastMutexDestroy(pGMM->Mtx);
    pGMM->Mtx = NIL_RTSEMFASTMUTEX;

    /* free any chunks still hanging around. */
    RTAvlU32Destroy(&pGMM->pChunks, gmmR0TermDestroyChunk, pGMM);

    /* finally the instance data itself. */
    RTMemFree(pGMM);
    LogFlow(("GMMTerm: done\n"));
}


/**
 * RTAvlU32Destroy callback.
 *
 * @returns 0
 * @param   pNode   The node to destroy.
 * @param   pvGMM   The GMM handle.
 */
static DECLCALLBACK(int) gmmR0TermDestroyChunk(PAVLU32NODECORE pNode, void *pvGMM)
{
    PGMMCHUNK pChunk = (PGMMCHUNK)pNode;

    if (pChunk->cFree != (GMM_CHUNK_SIZE >> PAGE_SHIFT))
        SUPR0Printf("GMMR0Term: %p/%#x: cFree=%d cPrivate=%d cShared=%d cMappings=%d\n", pChunk,
                    pChunk->Core.Key, pChunk->cFree, pChunk->cPrivate, pChunk->cShared, pChunk->cMappings);

    int rc = RTR0MemObjFree(pChunk->MemObj, true /* fFreeMappings */);
    if (RT_FAILURE(rc))
    {
        SUPR0Printf("GMMR0Term: %p/%#x: RTRMemObjFree(%p,true) -> %d (cMappings=%d)\n", pChunk,
                    pChunk->Core.Key, pChunk->MemObj, rc, pChunk->cMappings);
        AssertRC(rc);
    }
    pChunk->MemObj = NIL_RTR0MEMOBJ;

    RTMemFree(pChunk->paMappings);
    pChunk->paMappings = NULL;

    RTMemFree(pChunk);
    NOREF(pvGMM);
    return 0;
}


/**
 * Initializes the per-VM data for the GMM.
 *
 * This is called from within the GVMM lock (from GVMMR0CreateVM)
 * and should only initialize the data members so GMMR0CleanupVM
 * can deal with them. We reserve no memory or anything here,
 * that's done later in GMMR0InitVM.
 *
 * @param   pGVM    Pointer to the Global VM structure.
 */
GMMR0DECL(void) GMMR0InitPerVMData(PGVM pGVM)
{
    AssertCompile(RT_SIZEOFMEMB(GVM,gmm.s) <= RT_SIZEOFMEMB(GVM,gmm.padding));
    AssertRelease(RT_SIZEOFMEMB(GVM,gmm.s) <= RT_SIZEOFMEMB(GVM,gmm.padding));

    //pGVM->gmm.s.cBasePages = 0;
    //pGVM->gmm.s.cPrivatePages = 0;
    //pGVM->gmm.s.cSharedPages = 0;
    pGVM->gmm.s.enmPolicy = GMMOCPOLICY_INVALID;
    pGVM->gmm.s.enmPriority = GMMPRIORITY_INVALID;
    pGVM->gmm.s.fMayAllocate = false;
}


/**
 * Cleans up when a VM is terminating.
 *
 * @param   pGVM    Pointer to the Global VM structure.
 */
GMMR0DECL(void) GMMR0CleanupVM(PGVM pGVM)
{
    LogFlow(("GMMR0CleanupVM: pGVM=%p:{.pVM=%p, .hSelf=%#x}\n", pGVM, pGVM->pVM, pGVM->hSelf));

    /*
     * The policy is 'INVALID' until the initial reservation
     * request has been serviced.
     */
    if (    pGVM->gmm.s.enmPolicy <= GMMOCPOLICY_INVALID
        ||  pGVM->gmm.s.enmPolicy >= GMMOCPOLICY_END)
    {
    }


    PGMM pGMM;
    GMM_GET_VALID_INSTANCE_VOID(pGMM);

    int rc = RTSemFastMutexRequest(pGMM->Mtx);
    AssertRC(rc);

    /*
     * If it's the last VM around, we can skip walking all the chunk looking
     * for the pages owned by this VM and instead flush the whole shebang.
     *
     * This takes care of the eventuality that a VM has left shared page
     * references behind (shouldn't happen of course, but you never know).
     */
    pGMM->cActiveVMs--;
    if (!pGMM->cActiveVMs)
    {


    }
    else if (0)//pGVM->gmm.s.cPrivatePages)
    {
        /*
         * Walk the entire pool looking for pages that belongs to this VM.
         * This is slow but necessary. Of course it won't work for shared
         * pages, but we'll deal with that later.
         */
        RTAvlU32DoWithAll(&pGMM->pChunks, true /* fFromLeft */, gmmR0FreeVMPagesInChunk, (void *)pGVM->hSelf);

        /*
         * Update over-commitment management and free chunks that are no
         * longer needed.
         */

    }

    RTSemFastMutexRelease(pGMM->Mtx);

    /* trash the data */
//    pGVM->gmm.s.cBasePages = 0;
//    pGVM->gmm.s.cPrivatePages = 0;
//    pGVM->gmm.s.cSharedPages = 0;
    pGVM->gmm.s.enmPolicy = GMMOCPOLICY_INVALID;
    pGVM->gmm.s.enmPriority = GMMPRIORITY_INVALID;

    LogFlow(("GMMR0CleanupVM: returns\n"));
}


/**
 * RTAvlU32DoWithAll callback.
 *
 * @returns 0
 * @param   pNode   The node to destroy.
 * @param   pvhGVM  The GVM::hSelf value.
 */
static DECLCALLBACK(int) gmmR0FreeVMPagesInChunk(PAVLU32NODECORE pNode, void *pvhGVM)
{
    PGMMCHUNK pChunk = (PGMMCHUNK)pNode;
    uint16_t hGVM = (uintptr_t)pvhGVM;

#ifndef VBOx_STRICT
    if (pChunk->cFree != (GMM_CHUNK_SIZE >> PAGE_SHIFT))
#endif
    {
        /*
         * Perform some internal checks while we're scanning.
         */
        unsigned cPrivate = 0;
        unsigned cShared = 0;
        unsigned cFree = 0;

        unsigned iPage = (GMM_CHUNK_SIZE >> PAGE_SHIFT);
        while (iPage-- > 0)
            if (GMM_PAGE_IS_PRIVATE(&pChunk->aPages[iPage]))

            {
                if (pChunk->aPages[iPage].Private.hGVM == hGVM)
                {
                    /* Free it. */
                    pChunk->aPages[iPage].u = 0;
                    pChunk->aPages[iPage].Free.idNext = pChunk->idFreeHead;
                    pChunk->aPages[iPage].Free.u2State = GMM_PAGE_STATE_FREE;
                    pChunk->idFreeHead = iPage;
                    pChunk->cPrivate--;
                    pChunk->cFree++;
                    cFree++;
                }
                else
                    cPrivate++;
            }
            else if (GMM_PAGE_IS_FREE(&pChunk->aPages[iPage]))
                cFree++;
            else
                cShared++;

        /*
         * Did it add up?
         */
        if (RT_UNLIKELY(    pChunk->cFree != cFree
                        ||  pChunk->cPrivate != cPrivate
                        ||  pChunk->cShared != cShared))
        {
            SUPR0Printf("GMM: Chunk %p/%#x has bogus stats - free=%d/%d private=%d/%d shared=%d/%d\n",
                        pChunk->cFree, cFree, pChunk->cPrivate, cPrivate, pChunk->cShared, cShared);
            pChunk->cFree = cFree;
            pChunk->cPrivate = cPrivate;
            pChunk->cShared = cShared;
        }
    }

    return 0;
}


/**
 * The initial resource reservations.
 *
 * This will make memory reservations according to policy and priority. If there isn't
 * sufficient resources available to sustain the VM this function will fail and all
 * future allocations requests will fail as well.
 *
 * @returns VBox status code.
 * @retval  VERR_GMM_NOT_SUFFICENT_MEMORY
 * @retval  VERR_GMM_
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   cBasePages      The number of pages of RAM. This is *only* the RAM, it does
 *                          not take additional pages for the ROMs and MMIO2 into account.
 *                          These allocations will have to be reported when the initialization
 *                          process has completed.
 * @param   cShadowPages    The max number of pages that will be allocated for shadow pageing structures.
 * @param   enmPolicy       The OC policy to use on this VM.
 * @param   enmPriority     The priority in an out-of-memory situation.
 */
GMMR0DECL(int) GMMR0InitialReservation(PVM pVM, uint64_t cBasePages, uint32_t cShadowPages, GMMOCPOLICY enmPolicy, GMMPRIORITY enmPriority)
{
    LogFlow(("GMMR0InitialReservation: pVM=%p cBasePages=%#llx cShadowPages=%#x enmPolicy=%d enmPriority=%d\n",
             pVM, cBasePages, cShadowPages, enmPolicy, enmPriority));

    /*
     * Validate, get basics and take the semaphore.
     */
    PGMM pGMM;
    GMM_GET_VALID_INSTANCE(pGMM, VERR_INTERNAL_ERROR);
    PGVM pGVM = GVMMR0ByVM(pVM);
    if (!pVM)
        return VERR_INVALID_PARAMETER;

    int rc = RTSemFastMutexRequest(pGMM->Mtx);
    AssertRC(rc);


    pGMM->cActiveVMs++;



    RTSemFastMutexRelease(pGMM->Mtx);
    LogFlow(("GMMR0InitialReservation: returns %Rrc\n", rc));
    return rc;
}


/**
 * This updates the memory reservation with the additional MMIO2 and ROM pages.
 *
 * @returns VBox status code.
 * @retval  VERR_GMM_NOT_SUFFICENT_MEMORY
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   cExtraPages     The number of pages that makes up ROM ranges outside the normal
 *                          RAM and MMIO2 memory.
 * @param   cMisc           Miscellaneous fixed commitments like the heap, VM structure and such.
 */
GMMR0DECL(int) GMMR0ReservationUpdate(PVM pVM, uint64_t cExtraPages)
{
    LogFlow(("GMMR0ReservationUpdate: pVM=%p cExtraPages=%#llx\n", pVM, cExtraPages));

    /*
     * Validate, get basics and take the semaphore.
     */
    PGMM pGMM;
    GMM_GET_VALID_INSTANCE(pGMM, VERR_INTERNAL_ERROR);
    PGVM pGVM = GVMMR0ByVM(pVM);
    if (!pVM)
        return VERR_INVALID_PARAMETER;

    int rc = RTSemFastMutexRequest(pGMM->Mtx);
    AssertRC(rc);



    RTSemFastMutexRelease(pGMM->Mtx);
    LogFlow(("GMMR0ReservationUpdate: returns %Rrc\n", rc));
    return rc;
}



