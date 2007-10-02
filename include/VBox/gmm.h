/** @file
 * GMM - The Global Memory Manager.
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

#ifndef ___VBox_gmm_h
#define ___VBox_gmm_h

#include <VBox/types.h>
#include <VBox/gvmm.h>
#include <VBox/sup.h>

__BEGIN_DECLS

/** @defgroup   grp_gmm     GMM - The Global Memory Manager
 * @{
 */

/** @def IN_GMM_R0
 * Used to indicate whether we're inside the same link module as the ring 0
 * part of the Global Memory Manager or not.
 */
/** @def GMMR0DECL
 * Ring 0 GMM export or import declaration.
 * @param   type    The return type of the function declaration.
 */
#ifdef IN_GMM_R0
# define GMMR0DECL(type)    DECLEXPORT(type) VBOXCALL
#else
# define GMMR0DECL(type)    DECLIMPORT(type) VBOXCALL
#endif


/** The chunk shift. (2^20 = 1 MB) */
#define GMM_CHUNK_SHIFT                 20
/** The allocation chunk size. */
#define GMM_CHUNK_SIZE                  (1U << GMM_CHUNK_SHIFT)
/** The allocation chunk size in pages. */
#define GMM_CHUNK_NUM_PAGES             (1U << (GMM_CHUNK_SHIFT - PAGE_SHIFT))
/** The shift factor for converting a page id into a chunk id. */
#define GMM_CHUNKID_SHIFT               (GMM_CHUNK_SHIFT - PAGE_SHIFT)
/** The last valid Chunk ID value. */
#define GMM_CHUNKID_LAST                (GMM_PAGEID_LAST >> GMM_CHUNKID_SHIFT)
/** The last valid Page ID value.
 * The current limit is 2^28 - 1, or almost 1TB if you like.
 * The constraints are currently dictated by PGMPAGE. */
#define GMM_PAGEID_LAST                 (RT_BIT_32(28) - 1)
/** The NIL Chunk ID value. */
#define NIL_GMM_CHUNKID                 0
/** The NIL Page ID value. */
#define NIL_GMM_PAGEID                  0

#if 0 /* wrong - these are guest page pfns and not page ids! */
/** Special Page ID used by unassigned pages. */
#define GMM_PAGEID_UNASSIGNED           0x0fffffffU
/** Special Page ID used by unsharable pages.
 * Like MMIO2, shadow and heap. This is for later, obviously. */
#define GMM_PAGEID_UNSHARABLE           0x0ffffffeU
/** The end of the valid Page IDs. This is the first special one. */
#define GMM_PAGEID_END                  0x0ffffff0U
#endif


/**
 * Over-commitment policy.
 */
typedef enum GMMOCPOLICY
{
    /** The usual invalid 0 value. */
    GMMOCPOLICY_INVALID = 0,
    /** No over-commitment, fully backed.
     * The GMM guarantees that it will be able to allocate all of the
     * guest RAM for a VM with OC policy. */
    GMMOCPOLICY_NO_OC,
    /** to-be-determined. */
    GMMOCPOLICY_TBD,
    /** The end of the valid policy range. */
    GMMOCPOLICY_END,
    /** The usual 32-bit hack. */
    GMMOCPOLICY_32BIT_HACK = 0x7fffffff
} GMMOCPOLICY;

/**
 * VM / Memory priority.
 */
typedef enum GMMPRIORITY
{
    /** The usual invalid 0 value. */
    GMMPRIORITY_INVALID = 0,
    /** High.
     * When ballooning, ask these VMs last.
     * When running out of memory, try not to interrupt these VMs. */
    GMMPRIORITY_HIGH,
    /** Normal.
     * When ballooning, don't wait to ask these.
     * When running out of memory, pause, save and/or kill these VMs. */
    GMMPRIORITY_NORMAL,
    /** Low.
     * When ballooning, maximize these first.
     * When running out of memory, save or kill these VMs. */
    GMMPRIORITY_LOW,
    /** The end of the valid priority range. */
    GMMPRIORITY_END = 0,
    /** The custom 32-bit type blowup. */
    GMMPRIORITY_32BIT_HACK = 0x7fffffff
} GMMPRIORITY;


/**
 * GMM Memory Accounts.
 */
typedef enum GMMACCOUNT
{
    /** The customary invalid zero entry. */
    GMMACCOUNT_INVALID = 0,
    /** Account with the base allocations. */
    GMMACCOUNT_BASE,
    /** Account with the shadow allocations. */
    GMMACCOUNT_SHADOW,
    /** Account with the fixed allocations. */
    GMMACCOUNT_FIXED,
    /** The end of the valid values. */
    GMMACCOUNT_END,
    /** The usual 32-bit value to finish it off. */
    GMMACCOUNT_32BIT_HACK = 0x7fffffff
} GMMACCOUNT;


/**
 * A page descriptor for use when freeing pages.
 * See GMMR0FreePages, GMMR0BalloonedPages.
 */
typedef struct GMMFREEPAGEDESC
{
    /** The Page ID of the page to be freed. */
    uint32_t idPage;
} GMMFREEPAGEDESC;
/** Pointer to a page descriptor for freeing pages. */
typedef GMMFREEPAGEDESC *PGMMFREEPAGEDESC;


/**
 * A page descriptor for use when updating and allocating pages.
 *
 * This is a bit complicated because we want to do as much as possible
 * with the same structure.
 */
typedef struct GMMPAGEDESC
{
    /** The physical address of the page.
     *
     * @input   GMMR0AllocateHandyPages expects the guest physical address
     *          to update the GMMPAGE structure with. Pass GMM_GCPHYS_UNSHAREABLE
     *          when appropriate and NIL_RTHCPHYS when the page wasn't used
     *          for any specific guest address.
     *
     *          GMMR0AllocatePage expects the guest physical address to put in
     *          the GMMPAGE structure for the page it allocates for this entry.
     *          Pass NIL_RTHCPHYS and GMM_GCPHYS_UNSHAREABLE as above.
     *
     * @output  The host physical address of the allocated page.
     *          NIL_RTHCPHYS on allocation failure.
     *
     * ASSUMES: sizeof(RTHCPHYS) >= sizeof(RTGCPHYS).
     */
    RTHCPHYS                    HCPhysGCPhys;

    /** The Page ID.
     *
     * @intput  GMMR0AllocateHandyPages expects the Page ID of the page to
     *          update here. NIL_GMM_PAGEID means no page should be updated.
     *
     *          GMMR0AllocatePages requires this to be initialized to
     *          NIL_GMM_PAGEID currently.
     *
     * @output  The ID of the page, NIL_GMM_PAGEID if the allocation failed.
     */
    uint32_t                    idPage;

    /** The Page ID of the shared page was replaced by this page.
     *
     * @input   GMMR0AllocateHandyPages expects this to indicate a shared
     *          page that has been replaced by this page and should have its
     *          reference counter decremented and perhaps be freed up. Use
     *          NIL_GMM_PAGEID if no shared page was involved.
     *
     *          All other APIs expects NIL_GMM_PAGEID here.
     *
     * @output  All APIs sets this to NIL_GMM_PAGEID.
     */
    uint32_t                    idSharedPage;
} GMMPAGEDESC;
AssertCompileSize(GMMPAGEDESC, 16);
/** Pointer to a page allocation. */
typedef GMMPAGEDESC *PGMMPAGEDESC;

/** GMMPAGEDESC::HCPhysGCPhys value that indicates that the page is shared. */
#define GMM_GCPHYS_UNSHAREABLE  (RTHCPHYS)(0xfffffff0)

GMMR0DECL(int)  GMMR0Init(void);
GMMR0DECL(void) GMMR0Term(void);
GMMR0DECL(void) GMMR0InitPerVMData(PGVM pGVM);
GMMR0DECL(void) GMMR0CleanupVM(PGVM pGVM);
GMMR0DECL(int)  GMMR0InitialReservation(PVM pVM, uint64_t cBasePages, uint32_t cShadowPages, uint32_t cFixedPages,
                                        GMMOCPOLICY enmPolicy, GMMPRIORITY enmPriority);
GMMR0DECL(int)  GMMR0UpdateReservation(PVM pVM, uint64_t cBasePages, uint32_t cShadowPages, uint32_t cFixedPages);
GMMR0DECL(int)  GMMR0AllocateHandyPages(PVM pVM, uint32_t cPagesToUpdate, uint32_t cPagesToAlloc, PGMMPAGEDESC paPages);
GMMR0DECL(int)  GMMR0AllocatePages(PVM pVM, uint32_t cPages, PGMMPAGEDESC paPages, GMMACCOUNT enmAccount);
GMMR0DECL(int)  GMMR0FreePages(PVM pVM, uint32_t cPages, PGMMFREEPAGEDESC paPages, GMMACCOUNT enmAccount);
GMMR0DECL(int)  GMMR0BalloonedPages(PVM pVM, uint32_t cBalloonedPages, uint32_t cPagesToFree, PGMMFREEPAGEDESC paPages, bool fCompleted);
GMMR0DECL(int)  GMMR0DeflatedBalloon(PVM pVM, uint32_t cPages);
GMMR0DECL(int)  GMMR0FreeMapUnmapChunk(PVM pVM, uint32_t idChunkMap, uint32_t idChunkUnmap, PRTR3PTR pvR3);
GMMR0DECL(int)  GMMR0SeedChunk(PVM pVM, RTR3PTR pvR3);


/**
 * Request buffer for GMMR0InitialReservationReq / VMMR0_DO_GMM_INITIAL_RESERVATION.
 * @see GMMR0InitialReservation
 */
typedef struct GMMINITIALRESERVATIONREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    uint64_t        cBasePages;         /**< @see GMMR0InitialReservation */
    uint32_t        cShadowPages;       /**< @see GMMR0InitialReservation */
    uint32_t        cFixedPages;        /**< @see GMMR0InitialReservation */
    GMMOCPOLICY     enmPolicy;          /**< @see GMMR0InitialReservation */
    GMMPRIORITY     enmPriority;        /**< @see GMMR0InitialReservation */
} GMMINITIALRESERVATIONREQ;
/** Pointer to a GMMR0InitialReservationReq / VMMR0_DO_GMM_INITIAL_RESERVATION request buffer. */
typedef GMMINITIALRESERVATIONREQ *PGMMINITIALRESERVATIONREQ;

GMMR0DECL(int)  GMMR0InitialReservationReq(PVM pVM, PGMMINITIALRESERVATIONREQ pReq);


/**
 * Request buffer for GMMR0UpdateReservationReq / VMMR0_DO_GMM_UPDATE_RESERVATION.
 * @see GMMR0UpdateReservation
 */
typedef struct GMMUPDATERESERVATIONREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    uint64_t        cBasePages;         /**< @see GMMR0UpdateReservation */
    uint32_t        cShadowPages;       /**< @see GMMR0UpdateReservation */
    uint32_t        cFixedPages;        /**< @see GMMR0UpdateReservation */
} GMMUPDATERESERVATIONREQ;
/** Pointer to a GMMR0InitialReservationReq / VMMR0_DO_GMM_INITIAL_RESERVATION request buffer. */
typedef GMMUPDATERESERVATIONREQ *PGMMUPDATERESERVATIONREQ;

GMMR0DECL(int)  GMMR0UpdateReservationReq(PVM pVM, PGMMUPDATERESERVATIONREQ pReq);


/**
 * Request buffer for GMMR0AllocatePagesReq / VMMR0_DO_GMM_ALLOCATE_PAGES.
 * @see GMMR0AllocatePages.
 */
typedef struct GMMALLOCATEPAGESREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    /** The account to charge the allocation to. */
    GMMACCOUNT      enmAccount;
    /** The number of pages to allocate. */
    uint32_t        cPages;
    /** Array of page descriptors. */
    GMMPAGEDESC     aPages[1];
} GMMALLOCATEPAGESREQ;
/** Pointer to a GMMR0AllocatePagesReq / VMMR0_DO_GMM_ALLOCATE_PAGES request buffer. */
typedef GMMALLOCATEPAGESREQ *PGMMALLOCATEPAGESREQ;

GMMR0DECL(int)  GMMR0AllocatePagesReq(PVM pVM, PGMMALLOCATEPAGESREQ pReq);


/**
 * Request buffer for GMMR0FreePagesReq / VMMR0_DO_GMM_FREE_PAGES.
 * @see GMMR0FreePages.
 */
typedef struct GMMFREEPAGESREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    /** The account this relates to. */
    GMMACCOUNT      enmAccount;
    /** The number of pages to free. */
    uint32_t        cPages;
    /** Array of free page descriptors. */
    GMMFREEPAGEDESC aPages[1];
} GMMFREEPAGESREQ;
/** Pointer to a GMMR0FreePagesReq / VMMR0_DO_GMM_FREE_PAGES request buffer. */
typedef GMMFREEPAGESREQ *PGMMFREEPAGESREQ;

GMMR0DECL(int)  GMMR0FreePagesReq(PVM pVM, PGMMFREEPAGESREQ pReq);


/**
 * Request buffer for GMMR0BalloonedPagesReq / VMMR0_DO_GMM_BALLOONED_PAGES.
 * @see GMMR0BalloonedPages.
 */
typedef struct GMMBALLOONEDPAGESREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    /** The number of ballooned pages. */
    uint32_t        cBalloonedPages;
    /** The number of pages to free. */
    uint32_t        cPagesToFree;
    /** Whether the ballooning request is completed or more pages are still to come. */
    bool            fCompleted;
    /** Array of free page descriptors. */
    GMMFREEPAGEDESC aPages[1];
} GMMBALLOONEDPAGESREQ;
/** Pointer to a GMMR0BalloonedPagesReq / VMMR0_DO_GMM_BALLOONED_PAGES request buffer. */
typedef GMMBALLOONEDPAGESREQ *PGMMBALLOONEDPAGESREQ;

GMMR0DECL(int)  GMMR0BalloonedPagesReq(PVM pVM, PGMMBALLOONEDPAGESREQ pReq);


/**
 * Request buffer for GMMR0FreeMapUnmapChunkReq / VMMR0_DO_GMM_MAP_UNMAP_CHUNK.
 * @see GMMR0FreeMapUnmapChunk
 */
typedef struct GMMMAPUNMAPCHUNKREQ
{
    /** The header. */
    SUPVMMR0REQHDR  Hdr;
    /** The chunk to map, UINT32_MAX if unmap only. (IN) */
    uint32_t        idChunkMap;
    /** The chunk to unmap, UINT32_MAX if map only. (IN) */
    uint32_t        idChunkUnmap;
    /** Where the mapping address is returned. (OUT) */
    RTR3PTR         pvR3;
} GMMMAPUNMAPCHUNKREQ;
/** Pointer to a GMMR0FreeMapUnmapChunkReq / VMMR0_DO_GMM_MAP_UNMAP_CHUNK request buffer. */
typedef GMMMAPUNMAPCHUNKREQ *PGMMMAPUNMAPCHUNKREQ;

GMMR0DECL(int)  GMMR0FreeMapUnmapChunkReq(PVM pVM, PGMMMAPUNMAPCHUNKREQ pReq);


/** @} */

__END_DECLS

#endif

