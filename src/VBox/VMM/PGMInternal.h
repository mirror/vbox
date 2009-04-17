/* $Id$ */
/** @file
 * PGM - Internal header file.
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

#ifndef ___PGMInternal_h
#define ___PGMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/stam.h>
#include <VBox/param.h>
#include <VBox/vmm.h>
#include <VBox/mm.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmapi.h>
#include <VBox/dis.h>
#include <VBox/dbgf.h>
#include <VBox/log.h>
#include <VBox/gmm.h>
#include <VBox/hwaccm.h>
#include <iprt/avl.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>



/** @defgroup grp_pgm_int   Internals
 * @ingroup grp_pgm
 * @internal
 * @{
 */


/** @name PGM Compile Time Config
 * @{
 */

/**
 * Solve page is out of sync issues inside Guest Context (in PGMGC.cpp).
 * Comment it if it will break something.
 */
#define PGM_OUT_OF_SYNC_IN_GC

/**
 * Check and skip global PDEs for non-global flushes
 */
#define PGM_SKIP_GLOBAL_PAGEDIRS_ON_NONGLOBAL_FLUSH

/**
 * Sync N pages instead of a whole page table
 */
#define PGM_SYNC_N_PAGES

/**
 * Number of pages to sync during a page fault
 *
 * When PGMPOOL_WITH_GCPHYS_TRACKING is enabled using high values here
 * causes a lot of unnecessary extents and also is slower than taking more \#PFs.
 */
#define PGM_SYNC_NR_PAGES               8

/**
 * Number of PGMPhysRead/Write cache entries (must be <= sizeof(uint64_t))
 */
#define PGM_MAX_PHYSCACHE_ENTRIES       64
#define PGM_MAX_PHYSCACHE_ENTRIES_MASK  (PGM_MAX_PHYSCACHE_ENTRIES-1)

/**
 * Enable caching of PGMR3PhysRead/WriteByte/Word/Dword
 */
#define PGM_PHYSMEMACCESS_CACHING

/** @def PGMPOOL_WITH_CACHE
 * Enable agressive caching using the page pool.
 *
 * This requires PGMPOOL_WITH_USER_TRACKING and PGMPOOL_WITH_MONITORING.
 */
#define PGMPOOL_WITH_CACHE

/** @def PGMPOOL_WITH_MIXED_PT_CR3
 * When defined, we'll deal with 'uncachable' pages.
 */
#ifdef PGMPOOL_WITH_CACHE
# define PGMPOOL_WITH_MIXED_PT_CR3
#endif

/** @def PGMPOOL_WITH_MONITORING
 * Monitor the guest pages which are shadowed.
 * When this is enabled, PGMPOOL_WITH_CACHE or PGMPOOL_WITH_GCPHYS_TRACKING must
 * be enabled as well.
 * @remark doesn't really work without caching now. (Mixed PT/CR3 change.)
 */
#ifdef PGMPOOL_WITH_CACHE
# define PGMPOOL_WITH_MONITORING
#endif

/** @def PGMPOOL_WITH_GCPHYS_TRACKING
 * Tracking the of shadow pages mapping guest physical pages.
 *
 * This is very expensive, the current cache prototype is trying to figure out
 * whether it will be acceptable with an agressive caching policy.
 */
#if defined(PGMPOOL_WITH_CACHE) || defined(PGMPOOL_WITH_MONITORING)
# define PGMPOOL_WITH_GCPHYS_TRACKING
#endif

/** @def PGMPOOL_WITH_USER_TRACKING
 * Tracking users of shadow pages. This is required for the linking of shadow page
 * tables and physical guest addresses.
 */
#if defined(PGMPOOL_WITH_GCPHYS_TRACKING) || defined(PGMPOOL_WITH_CACHE) || defined(PGMPOOL_WITH_MONITORING)
# define PGMPOOL_WITH_USER_TRACKING
#endif

/** @def PGMPOOL_CFG_MAX_GROW
 * The maximum number of pages to add to the pool in one go.
 */
#define PGMPOOL_CFG_MAX_GROW            (_256K >> PAGE_SHIFT)

/** @def VBOX_STRICT_PGM_HANDLER_VIRTUAL
 * Enables some extra assertions for virtual handlers (mainly phys2virt related).
 */
#ifdef VBOX_STRICT
# define VBOX_STRICT_PGM_HANDLER_VIRTUAL
#endif

/** @def VBOX_WITH_NEW_LAZY_PAGE_ALLOC
 * Enables the experimental lazy page allocation code. */
/*# define VBOX_WITH_NEW_LAZY_PAGE_ALLOC */

/** @} */


/** @name PDPT and PML4 flags.
 * These are placed in the three bits available for system programs in
 * the PDPT and PML4 entries.
 * @{ */
/** The entry is a permanent one and it's must always be present.
 * Never free such an entry. */
#define PGM_PLXFLAGS_PERMANENT          RT_BIT_64(10)
/** Mapping (hypervisor allocated pagetable). */
#define PGM_PLXFLAGS_MAPPING            RT_BIT_64(11)
/** @} */

/** @name Page directory flags.
 * These are placed in the three bits available for system programs in
 * the page directory entries.
 * @{ */
/** Mapping (hypervisor allocated pagetable). */
#define PGM_PDFLAGS_MAPPING             RT_BIT_64(10)
/** Made read-only to facilitate dirty bit tracking. */
#define PGM_PDFLAGS_TRACK_DIRTY         RT_BIT_64(11)
/** @} */

/** @name Page flags.
 * These are placed in the three bits available for system programs in
 * the page entries.
 * @{ */
/** Made read-only to facilitate dirty bit tracking. */
#define PGM_PTFLAGS_TRACK_DIRTY         RT_BIT_64(9)

#ifndef PGM_PTFLAGS_CSAM_VALIDATED
/** Scanned and approved by CSAM (tm).
 * NOTE: Must be identical to the one defined in CSAMInternal.h!!
 * @todo Move PGM_PTFLAGS_* and PGM_PDFLAGS_* to VBox/pgm.h. */
#define PGM_PTFLAGS_CSAM_VALIDATED      RT_BIT_64(11)
#endif

/** @} */

/** @name Defines used to indicate the shadow and guest paging in the templates.
 * @{ */
#define PGM_TYPE_REAL                   1
#define PGM_TYPE_PROT                   2
#define PGM_TYPE_32BIT                  3
#define PGM_TYPE_PAE                    4
#define PGM_TYPE_AMD64                  5
#define PGM_TYPE_NESTED                 6
#define PGM_TYPE_EPT                    7
#define PGM_TYPE_MAX                    PGM_TYPE_EPT
/** @} */

/** Macro for checking if the guest is using paging.
 * @param uGstType     PGM_TYPE_*
 * @param uShwType     PGM_TYPE_*
 * @remark  ASSUMES certain order of the PGM_TYPE_* values.
 */
#define PGM_WITH_PAGING(uGstType, uShwType)  \
    (   (uGstType) >= PGM_TYPE_32BIT \
     && (uShwType) != PGM_TYPE_NESTED \
     && (uShwType) != PGM_TYPE_EPT)

/** Macro for checking if the guest supports the NX bit.
 * @param uGstType     PGM_TYPE_*
 * @param uShwType     PGM_TYPE_*
 * @remark  ASSUMES certain order of the PGM_TYPE_* values.
 */
#define PGM_WITH_NX(uGstType, uShwType)  \
    (   (uGstType) >= PGM_TYPE_PAE \
     && (uShwType) != PGM_TYPE_NESTED \
     && (uShwType) != PGM_TYPE_EPT)


/** @def PGM_HCPHYS_2_PTR
 * Maps a HC physical page pool address to a virtual address.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address to map to a virtual one.
 * @param   ppv     Where to store the virtual address. No need to cast this.
 *
 * @remark  In GC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#ifdef IN_RC
# define PGM_HCPHYS_2_PTR(pVM, HCPhys, ppv) \
     PGMDynMapHCPage(pVM, HCPhys, (void **)(ppv))
#elif defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define PGM_HCPHYS_2_PTR(pVM, HCPhys, ppv) \
     pgmR0DynMapHCPageInlined(&(pVM)->pgm.s, HCPhys, (void **)(ppv))
#else
# define PGM_HCPHYS_2_PTR(pVM, HCPhys, ppv) \
     MMPagePhys2PageEx(pVM, HCPhys, (void **)(ppv))
#endif

/** @def PGM_HCPHYS_2_PTR_BY_PGM
 * Maps a HC physical page pool address to a virtual address.
 *
 * @returns VBox status code.
 * @param   pPGM    The PGM instance data.
 * @param   HCPhys  The HC physical address to map to a virtual one.
 * @param   ppv     Where to store the virtual address. No need to cast this.
 *
 * @remark  In GC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
# define PGM_HCPHYS_2_PTR_BY_PGM(pPGM, HCPhys, ppv) \
     pgmR0DynMapHCPageInlined(pPGM, HCPhys, (void **)(ppv))
#else
# define PGM_HCPHYS_2_PTR_BY_PGM(pPGM, HCPhys, ppv) \
     PGM_HCPHYS_2_PTR(PGM2VM(pPGM), HCPhys, (void **)(ppv))
#endif

/** @def PGM_GCPHYS_2_PTR
 * Maps a GC physical page address to a virtual address.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to map to a virtual one.
 * @param   ppv     Where to store the virtual address. No need to cast this.
 *
 * @remark  In GC this uses PGMGCDynMapGCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#ifdef IN_RC
# define PGM_GCPHYS_2_PTR(pVM, GCPhys, ppv) \
     PGMDynMapGCPage(pVM, GCPhys, (void **)(ppv))
#elif defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define PGM_GCPHYS_2_PTR(pVM, GCPhys, ppv) \
     pgmR0DynMapGCPageInlined(&(pVM)->pgm.s, GCPhys, (void **)(ppv))
#else
# define PGM_GCPHYS_2_PTR(pVM, GCPhys, ppv) \
     PGMPhysGCPhys2R3Ptr(pVM, GCPhys, 1 /* one page only */, (PRTR3PTR)(ppv)) /** @todo this isn't asserting, use PGMRamGCPhys2HCPtr! */
#endif

/** @def PGM_GCPHYS_2_PTR_BY_PGMCPU
 * Maps a GC physical page address to a virtual address.
 *
 * @returns VBox status code.
 * @param   pPGM    Pointer to the PGM instance data.
 * @param   GCPhys  The GC physical address to map to a virtual one.
 * @param   ppv     Where to store the virtual address. No need to cast this.
 *
 * @remark  In GC this uses PGMGCDynMapGCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
# define PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, GCPhys, ppv) \
     pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), GCPhys, (void **)(ppv))
#else
# define PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, GCPhys, ppv) \
     PGM_GCPHYS_2_PTR(PGMCPU2VM(pPGM), GCPhys, ppv)
#endif

/** @def PGM_GCPHYS_2_PTR_EX
 * Maps a unaligned GC physical page address to a virtual address.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to map to a virtual one.
 * @param   ppv     Where to store the virtual address. No need to cast this.
 *
 * @remark  In GC this uses PGMGCDynMapGCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define PGM_GCPHYS_2_PTR_EX(pVM, GCPhys, ppv) \
     PGMDynMapGCPageOff(pVM, GCPhys, (void **)(ppv))
#else
# define PGM_GCPHYS_2_PTR_EX(pVM, GCPhys, ppv) \
     PGMPhysGCPhys2R3Ptr(pVM, GCPhys, 1 /* one page only */, (PRTR3PTR)(ppv)) /** @todo this isn't asserting, use PGMRamGCPhys2HCPtr! */
#endif

/** @def PGM_INVL_PG
 * Invalidates a page when in GC does nothing in HC.
 *
 * @param   GCVirt      The virtual address of the page to invalidate.
 */
#ifdef IN_RC
# define PGM_INVL_PG(GCVirt)            ASMInvalidatePage((void *)(GCVirt))
#elif defined(IN_RING0)
# define PGM_INVL_PG(GCVirt)            HWACCMInvalidatePage(pVM, (RTGCPTR)(GCVirt))
#else
# define PGM_INVL_PG(GCVirt)            HWACCMInvalidatePage(pVM, (RTGCPTR)(GCVirt))
#endif

/** @def PGM_INVL_BIG_PG
 * Invalidates a 4MB page directory entry when in GC does nothing in HC.
 *
 * @param   GCVirt      The virtual address within the page directory to invalidate.
 */
#ifdef IN_RC
# define PGM_INVL_BIG_PG(GCVirt)        ASMReloadCR3()
#elif defined(IN_RING0)
# define PGM_INVL_BIG_PG(GCVirt)        HWACCMFlushTLB(pVM)
#else
# define PGM_INVL_BIG_PG(GCVirt)        HWACCMFlushTLB(pVM)
#endif

/** @def PGM_INVL_GUEST_TLBS()
 * Invalidates all guest TLBs.
 */
#ifdef IN_RC
# define PGM_INVL_GUEST_TLBS()          ASMReloadCR3()
#elif defined(IN_RING0)
# define PGM_INVL_GUEST_TLBS()          HWACCMFlushTLB(pVM)
#else
# define PGM_INVL_GUEST_TLBS()          HWACCMFlushTLB(pVM)
#endif

/** Size of the GCPtrConflict array in PGMMAPPING.
 * @remarks Must be a power of two. */
#define PGMMAPPING_CONFLICT_MAX         8

/**
 * Structure for tracking GC Mappings.
 *
 * This structure is used by linked list in both GC and HC.
 */
typedef struct PGMMAPPING
{
    /** Pointer to next entry. */
    R3PTRTYPE(struct PGMMAPPING *)      pNextR3;
    /** Pointer to next entry. */
    R0PTRTYPE(struct PGMMAPPING *)      pNextR0;
    /** Pointer to next entry. */
    RCPTRTYPE(struct PGMMAPPING *)      pNextRC;
    /** Indicate whether this entry is finalized. */
    bool                                fFinalized;
    /** Start Virtual address. */
    RTGCPTR                             GCPtr;
    /** Last Virtual address (inclusive). */
    RTGCPTR                             GCPtrLast;
    /** Range size (bytes). */
    RTGCPTR                             cb;
    /** Pointer to relocation callback function. */
    R3PTRTYPE(PFNPGMRELOCATE)           pfnRelocate;
    /** User argument to the callback. */
    R3PTRTYPE(void *)                   pvUser;
    /** Mapping description / name. For easing debugging. */
    R3PTRTYPE(const char *)             pszDesc;
    /** Last 8 addresses that caused conflicts. */
    RTGCPTR                             aGCPtrConflicts[PGMMAPPING_CONFLICT_MAX];
    /** Number of conflicts for this hypervisor mapping. */
    uint32_t                            cConflicts;
    /** Number of page tables. */
    uint32_t                            cPTs;

    /** Array of page table mapping data. Each entry
     * describes one page table. The array can be longer
     * than the declared length.
     */
    struct
    {
        /** The HC physical address of the page table. */
        RTHCPHYS                        HCPhysPT;
        /** The HC physical address of the first PAE page table. */
        RTHCPHYS                        HCPhysPaePT0;
        /** The HC physical address of the second PAE page table. */
        RTHCPHYS                        HCPhysPaePT1;
        /** The HC virtual address of the 32-bit page table. */
        R3PTRTYPE(PX86PT)               pPTR3;
        /** The HC virtual address of the two PAE page table. (i.e 1024 entries instead of 512) */
        R3PTRTYPE(PX86PTPAE)            paPaePTsR3;
        /** The GC virtual address of the 32-bit page table. */
        RCPTRTYPE(PX86PT)               pPTRC;
        /** The GC virtual address of the two PAE page table. */
        RCPTRTYPE(PX86PTPAE)            paPaePTsRC;
        /** The GC virtual address of the 32-bit page table. */
        R0PTRTYPE(PX86PT)               pPTR0;
        /** The GC virtual address of the two PAE page table. */
        R0PTRTYPE(PX86PTPAE)            paPaePTsR0;
    } aPTs[1];
} PGMMAPPING;
/** Pointer to structure for tracking GC Mappings. */
typedef struct PGMMAPPING *PPGMMAPPING;


/**
 * Physical page access handler structure.
 *
 * This is used to keep track of physical address ranges
 * which are being monitored in some kind of way.
 */
typedef struct PGMPHYSHANDLER
{
    AVLROGCPHYSNODECORE                 Core;
    /** Access type. */
    PGMPHYSHANDLERTYPE                  enmType;
    /** Number of pages to update. */
    uint32_t                            cPages;
    /** Pointer to R3 callback function. */
    R3PTRTYPE(PFNPGMR3PHYSHANDLER)      pfnHandlerR3;
    /** User argument for R3 handlers. */
    R3PTRTYPE(void *)                   pvUserR3;
    /** Pointer to R0 callback function. */
    R0PTRTYPE(PFNPGMR0PHYSHANDLER)      pfnHandlerR0;
    /** User argument for R0 handlers. */
    R0PTRTYPE(void *)                   pvUserR0;
    /** Pointer to GC callback function. */
    RCPTRTYPE(PFNPGMRCPHYSHANDLER)      pfnHandlerRC;
    /** User argument for RC handlers. */
    RCPTRTYPE(void *)                   pvUserRC;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)             pszDesc;
#ifdef VBOX_WITH_STATISTICS
    /** Profiling of this handler. */
    STAMPROFILE                         Stat;
#endif
} PGMPHYSHANDLER;
/** Pointer to a physical page access handler structure. */
typedef PGMPHYSHANDLER *PPGMPHYSHANDLER;


/**
 * Cache node for the physical addresses covered by a virtual handler.
 */
typedef struct PGMPHYS2VIRTHANDLER
{
    /** Core node for the tree based on physical ranges. */
    AVLROGCPHYSNODECORE                 Core;
    /** Offset from this struct to the PGMVIRTHANDLER structure. */
    int32_t                             offVirtHandler;
    /** Offset of the next alias relative to this one.
     * Bit 0 is used for indicating whether we're in the tree.
     * Bit 1 is used for indicating that we're the head node.
     */
    int32_t                             offNextAlias;
} PGMPHYS2VIRTHANDLER;
/** Pointer to a phys to virtual handler structure. */
typedef PGMPHYS2VIRTHANDLER *PPGMPHYS2VIRTHANDLER;

/** The bit in PGMPHYS2VIRTHANDLER::offNextAlias used to indicate that the
 * node is in the tree. */
#define PGMPHYS2VIRTHANDLER_IN_TREE     RT_BIT(0)
/** The bit in PGMPHYS2VIRTHANDLER::offNextAlias used to indicate that the
 * node is in the head of an alias chain.
 * The PGMPHYS2VIRTHANDLER_IN_TREE is always set if this bit is set. */
#define PGMPHYS2VIRTHANDLER_IS_HEAD     RT_BIT(1)
/** The mask to apply to PGMPHYS2VIRTHANDLER::offNextAlias to get the offset. */
#define PGMPHYS2VIRTHANDLER_OFF_MASK    (~(int32_t)3)


/**
 * Virtual page access handler structure.
 *
 * This is used to keep track of virtual address ranges
 * which are being monitored in some kind of way.
 */
typedef struct PGMVIRTHANDLER
{
    /** Core node for the tree based on virtual ranges. */
    AVLROGCPTRNODECORE                  Core;
    /** Size of the range (in bytes). */
    RTGCPTR                             cb;
    /** Number of cache pages. */
    uint32_t                            cPages;
    /** Access type. */
    PGMVIRTHANDLERTYPE                  enmType;
    /** Pointer to the RC callback function. */
    RCPTRTYPE(PFNPGMRCVIRTHANDLER)      pfnHandlerRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                             padding;
#endif
    /** Pointer to the R3 callback function for invalidation. */
    R3PTRTYPE(PFNPGMR3VIRTINVALIDATE)   pfnInvalidateR3;
    /** Pointer to the R3 callback function. */
    R3PTRTYPE(PFNPGMR3VIRTHANDLER)      pfnHandlerR3;
    /** Description / Name. For easing debugging. */
    R3PTRTYPE(const char *)             pszDesc;
#ifdef VBOX_WITH_STATISTICS
    /** Profiling of this handler. */
    STAMPROFILE                         Stat;
#endif
    /** Array of cached physical addresses for the monitored ranged.  */
    PGMPHYS2VIRTHANDLER                 aPhysToVirt[HC_ARCH_BITS == 32 ? 1 : 2];
} PGMVIRTHANDLER;
/** Pointer to a virtual page access handler structure. */
typedef PGMVIRTHANDLER *PPGMVIRTHANDLER;


/**
 * Page type.
 *
 * @remarks This enum has to fit in a 3-bit field (see PGMPAGE::u3Type).
 * @remarks This is used in the saved state, so changes to it requires bumping
 *          the saved state version.
 * @todo    So, convert to \#defines!
 */
typedef enum PGMPAGETYPE
{
    /** The usual invalid zero entry. */
    PGMPAGETYPE_INVALID = 0,
    /** RAM page. (RWX) */
    PGMPAGETYPE_RAM,
    /** MMIO2 page. (RWX) */
    PGMPAGETYPE_MMIO2,
    /** MMIO2 page aliased over an MMIO page. (RWX)
     * See PGMHandlerPhysicalPageAlias(). */
    PGMPAGETYPE_MMIO2_ALIAS_MMIO,
    /** Shadowed ROM. (RWX) */
    PGMPAGETYPE_ROM_SHADOW,
    /** ROM page. (R-X) */
    PGMPAGETYPE_ROM,
    /** MMIO page. (---) */
    PGMPAGETYPE_MMIO,
    /** End of valid entries. */
    PGMPAGETYPE_END
} PGMPAGETYPE;
AssertCompile(PGMPAGETYPE_END <= 7);

/** @name Page type predicates.
 * @{ */
#define PGMPAGETYPE_IS_READABLE(type)   ( (type) <= PGMPAGETYPE_ROM )
#define PGMPAGETYPE_IS_WRITEABLE(type)  ( (type) <= PGMPAGETYPE_ROM_SHADOW )
#define PGMPAGETYPE_IS_RWX(type)        ( (type) <= PGMPAGETYPE_ROM_SHADOW )
#define PGMPAGETYPE_IS_ROX(type)        ( (type) == PGMPAGETYPE_ROM )
#define PGMPAGETYPE_IS_NP(type)         ( (type) == PGMPAGETYPE_MMIO )
/** @} */


/**
 * A Physical Guest Page tracking structure.
 *
 * The format of this structure is complicated because we have to fit a lot
 * of information into as few bits as possible. The format is also subject
 * to change (there is one comming up soon). Which means that for we'll be
 * using PGM_PAGE_GET_*, PGM_PAGE_IS_ and PGM_PAGE_SET_* macros for *all*
 * accessess to the structure.
 */
typedef struct PGMPAGE
{
    /** The physical address and a whole lot of other stuff. All bits are used! */
    RTHCPHYS    HCPhysX;
    /** The page state. */
    uint32_t    u2StateX : 2;
    /** Flag indicating that a write monitored page was written to when set. */
    uint32_t    fWrittenToX : 1;
    /** For later. */
    uint32_t    fSomethingElse : 1;
    /** The Page ID.
     * @todo  Merge with HCPhysX once we've liberated HCPhysX of its stuff.
     *        The HCPhysX will then be 100% static. */
    uint32_t    idPageX : 28;
    /** The page type (PGMPAGETYPE). */
    uint32_t    u3Type : 3;
    /** The physical handler state (PGM_PAGE_HNDL_PHYS_STATE*) */
    uint32_t    u2HandlerPhysStateX : 2;
    /** The virtual handler state (PGM_PAGE_HNDL_VIRT_STATE*) */
    uint32_t    u2HandlerVirtStateX : 2;
    uint32_t    u29B : 25;
} PGMPAGE;
AssertCompileSize(PGMPAGE, 16);
/** Pointer to a physical guest page. */
typedef PGMPAGE *PPGMPAGE;
/** Pointer to a const physical guest page. */
typedef const PGMPAGE *PCPGMPAGE;
/** Pointer to a physical guest page pointer. */
typedef PPGMPAGE *PPPGMPAGE;


/**
 * Clears the page structure.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_CLEAR(pPage) \
    do { \
        (pPage)->HCPhysX        = 0; \
        (pPage)->u2StateX       = 0; \
        (pPage)->fWrittenToX    = 0; \
        (pPage)->fSomethingElse = 0; \
        (pPage)->idPageX        = 0; \
        (pPage)->u3Type         = 0; \
        (pPage)->u29B           = 0; \
    } while (0)

/**
 * Initializes the page structure.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_INIT(pPage, _HCPhys, _idPage, _uType, _uState) \
    do { \
        (pPage)->HCPhysX        = (_HCPhys); \
        (pPage)->u2StateX       = (_uState); \
        (pPage)->fWrittenToX    = 0; \
        (pPage)->fSomethingElse = 0; \
        (pPage)->idPageX        = (_idPage); \
        /*(pPage)->u3Type         = (_uType); - later */ \
        PGM_PAGE_SET_TYPE(pPage, _uType); \
        (pPage)->u29B           = 0; \
    } while (0)

/**
 * Initializes the page structure of a ZERO page.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_INIT_ZERO(pPage, pVM, _uType)  \
    PGM_PAGE_INIT(pPage, (pVM)->pgm.s.HCPhysZeroPg, NIL_GMM_PAGEID, (_uType), PGM_PAGE_STATE_ZERO)
/** Temporary hack. Replaced by PGM_PAGE_INIT_ZERO once the old code is kicked out. */
# define PGM_PAGE_INIT_ZERO_REAL(pPage, pVM, _uType)  \
    PGM_PAGE_INIT(pPage, (pVM)->pgm.s.HCPhysZeroPg, NIL_GMM_PAGEID, (_uType), PGM_PAGE_STATE_ZERO)


/** @name The Page state, PGMPAGE::u2StateX.
 * @{ */
/** The zero page.
 * This is a per-VM page that's never ever mapped writable. */
#define PGM_PAGE_STATE_ZERO             0
/** A allocated page.
 * This is a per-VM page allocated from the page pool (or wherever
 * we get MMIO2 pages from if the type is MMIO2).
 */
#define PGM_PAGE_STATE_ALLOCATED        1
/** A allocated page that's being monitored for writes.
 * The shadow page table mappings are read-only. When a write occurs, the
 * fWrittenTo member is set, the page remapped as read-write and the state
 * moved back to allocated. */
#define PGM_PAGE_STATE_WRITE_MONITORED  2
/** The page is shared, aka. copy-on-write.
 * This is a page that's shared with other VMs. */
#define PGM_PAGE_STATE_SHARED           3
/** @} */


/**
 * Gets the page state.
 * @returns page state (PGM_PAGE_STATE_*).
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_STATE(pPage)       ( (pPage)->u2StateX )

/**
 * Sets the page state.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 * @param   _uState     The new page state.
 */
#define PGM_PAGE_SET_STATE(pPage, _uState) \
                                        do { (pPage)->u2StateX = (_uState); } while (0)


/**
 * Gets the host physical address of the guest page.
 * @returns host physical address (RTHCPHYS).
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_HCPHYS(pPage)      ( (pPage)->HCPhysX & UINT64_C(0x0000fffffffff000) )

/**
 * Sets the host physical address of the guest page.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 * @param   _HCPhys     The new host physical address.
 */
#define PGM_PAGE_SET_HCPHYS(pPage, _HCPhys) \
                                        do { (pPage)->HCPhysX = (((pPage)->HCPhysX) & UINT64_C(0xffff000000000fff)) \
                                                              | ((_HCPhys) & UINT64_C(0x0000fffffffff000)); } while (0)

/**
 * Get the Page ID.
 * @returns The Page ID; NIL_GMM_PAGEID if it's a ZERO page.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_PAGEID(pPage)      ( (pPage)->idPageX )
/* later:
#define PGM_PAGE_GET_PAGEID(pPage)      (   ((uint32_t)(pPage)->HCPhysX >> (48 - 12))
                                         |  ((uint32_t)(pPage)->HCPhysX & 0xfff) )
*/
/**
 * Sets the Page ID.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_SET_PAGEID(pPage, _idPage)  do { (pPage)->idPageX = (_idPage); } while (0)
/* later:
#define PGM_PAGE_SET_PAGEID(pPage, _idPage)  do { (pPage)->HCPhysX = (((pPage)->HCPhysX) & UINT64_C(0x0000fffffffff000)) \
                                                                   | ((_idPage) & 0xfff) \
                                                                   | (((_idPage) & 0x0ffff000) << (48-12)); } while (0)
*/

/**
 * Get the Chunk ID.
 * @returns The Chunk ID; NIL_GMM_CHUNKID if it's a ZERO page.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_CHUNKID(pPage)     ( (pPage)->idPageX >> GMM_CHUNKID_SHIFT )
/* later:
#if GMM_CHUNKID_SHIFT == 12
# define PGM_PAGE_GET_CHUNKID(pPage)    ( (uint32_t)((pPage)->HCPhysX >> 48) )
#elif GMM_CHUNKID_SHIFT > 12
# define PGM_PAGE_GET_CHUNKID(pPage)    ( (uint32_t)((pPage)->HCPhysX >> (48 + (GMM_CHUNKID_SHIFT - 12)) )
#elif GMM_CHUNKID_SHIFT < 12
# define PGM_PAGE_GET_CHUNKID(pPage)    (   ( (uint32_t)((pPage)->HCPhysX >> 48)   << (12 - GMM_CHUNKID_SHIFT) ) \
                                         |  ( (uint32_t)((pPage)->HCPhysX & 0xfff) >> GMM_CHUNKID_SHIFT ) )
#else
# error "GMM_CHUNKID_SHIFT isn't defined or something."
#endif
*/

/**
 * Get the index of the page within the allocaiton chunk.
 * @returns The page index.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_PAGE_IN_CHUNK(pPage)   ( (pPage)->idPageX & GMM_PAGEID_IDX_MASK )
/* later:
#if GMM_CHUNKID_SHIFT <= 12
# define PGM_PAGE_GET_PAGE_IN_CHUNK(pPage)  ( (uint32_t)((pPage)->HCPhysX & GMM_PAGEID_IDX_MASK) )
#else
# define PGM_PAGE_GET_PAGE_IN_CHUNK(pPage)  (   (uint32_t)((pPage)->HCPhysX & 0xfff) \
                                             |  ( (uint32_t)((pPage)->HCPhysX >> 48) & (RT_BIT_32(GMM_CHUNKID_SHIFT - 12) - 1) ) )
#endif
*/


/**
 * Gets the page type.
 * @returns The page type.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_TYPE(pPage)        (pPage)->u3Type

/**
 * Sets the page type.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 * @param   _enmType    The new page type (PGMPAGETYPE).
 */
#define PGM_PAGE_SET_TYPE(pPage, _enmType) \
    do { (pPage)->u3Type = (_enmType); } while (0)

/**
 * Checks if the page is marked for MMIO.
 * @returns true/false.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_IS_MMIO(pPage)         ( (pPage)->u3Type == PGMPAGETYPE_MMIO )

/**
 * Checks if the page is backed by the ZERO page.
 * @returns true/false.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_IS_ZERO(pPage)         ( (pPage)->u2StateX == PGM_PAGE_STATE_ZERO )

/**
 * Checks if the page is backed by a SHARED page.
 * @returns true/false.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_IS_SHARED(pPage)       ( (pPage)->u2StateX == PGM_PAGE_STATE_SHARED )


/**
 * Marks the paget as written to (for GMM change monitoring).
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_SET_WRITTEN_TO(pPage)      do { (pPage)->fWrittenToX = 1; } while (0)

/**
 * Clears the written-to indicator.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_CLEAR_WRITTEN_TO(pPage)    do { (pPage)->fWrittenToX = 0; } while (0)

/**
 * Checks if the page was marked as written-to.
 * @returns true/false.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_IS_WRITTEN_TO(pPage)       ( (pPage)->fWrittenToX )


/** @name Physical Access Handler State values (PGMPAGE::u2HandlerPhysStateX).
 *
 * @remarks The values are assigned in order of priority, so we can calculate
 *          the correct state for a page with different handlers installed.
 * @{ */
/** No handler installed. */
#define PGM_PAGE_HNDL_PHYS_STATE_NONE       0
/** Monitoring is temporarily disabled. */
#define PGM_PAGE_HNDL_PHYS_STATE_DISABLED   1
/** Write access is monitored. */
#define PGM_PAGE_HNDL_PHYS_STATE_WRITE      2
/** All access is monitored. */
#define PGM_PAGE_HNDL_PHYS_STATE_ALL        3
/** @} */

/**
 * Gets the physical access handler state of a page.
 * @returns PGM_PAGE_HNDL_PHYS_STATE_* value.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_HNDL_PHYS_STATE(pPage)     ( (pPage)->u2HandlerPhysStateX )

/**
 * Sets the physical access handler state of a page.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 * @param   _uState     The new state value.
 */
#define PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, _uState) \
    do { (pPage)->u2HandlerPhysStateX = (_uState); } while (0)

/**
 * Checks if the page has any physical access handlers, including temporariliy disabled ones.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ANY_PHYSICAL_HANDLERS(pPage)      ( (pPage)->u2HandlerPhysStateX != PGM_PAGE_HNDL_PHYS_STATE_NONE )

/**
 * Checks if the page has any active physical access handlers.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage)   ( (pPage)->u2HandlerPhysStateX >= PGM_PAGE_HNDL_PHYS_STATE_WRITE )


/** @name Virtual Access Handler State values (PGMPAGE::u2HandlerVirtStateX).
 *
 * @remarks The values are assigned in order of priority, so we can calculate
 *          the correct state for a page with different handlers installed.
 * @{ */
/** No handler installed. */
#define PGM_PAGE_HNDL_VIRT_STATE_NONE       0
/* 1 is reserved so the lineup is identical with the physical ones. */
/** Write access is monitored. */
#define PGM_PAGE_HNDL_VIRT_STATE_WRITE      2
/** All access is monitored. */
#define PGM_PAGE_HNDL_VIRT_STATE_ALL        3
/** @} */

/**
 * Gets the virtual access handler state of a page.
 * @returns PGM_PAGE_HNDL_VIRT_STATE_* value.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) ( (pPage)->u2HandlerVirtStateX )

/**
 * Sets the virtual access handler state of a page.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 * @param   _uState     The new state value.
 */
#define PGM_PAGE_SET_HNDL_VIRT_STATE(pPage, _uState) \
    do { (pPage)->u2HandlerVirtStateX = (_uState); } while (0)

/**
 * Checks if the page has any virtual access handlers.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ANY_VIRTUAL_HANDLERS(pPage)    ( (pPage)->u2HandlerVirtStateX != PGM_PAGE_HNDL_VIRT_STATE_NONE )

/**
 * Same as PGM_PAGE_HAS_ANY_VIRTUAL_HANDLERS - can't disable pages in
 * virtual handlers.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage) PGM_PAGE_HAS_ANY_VIRTUAL_HANDLERS(pPage)



/**
 * Checks if the page has any access handlers, including temporarily disabled ones.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ANY_HANDLERS(pPage) \
    (   (pPage)->u2HandlerPhysStateX != PGM_PAGE_HNDL_PHYS_STATE_NONE \
     || (pPage)->u2HandlerVirtStateX != PGM_PAGE_HNDL_VIRT_STATE_NONE )

/**
 * Checks if the page has any active access handlers.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage) \
    (   (pPage)->u2HandlerPhysStateX >= PGM_PAGE_HNDL_PHYS_STATE_WRITE \
     || (pPage)->u2HandlerVirtStateX >= PGM_PAGE_HNDL_VIRT_STATE_WRITE )

/**
 * Checks if the page has any active access handlers catching all accesses.
 * @returns true/false
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage) \
    (   (pPage)->u2HandlerPhysStateX == PGM_PAGE_HNDL_PHYS_STATE_ALL \
     || (pPage)->u2HandlerVirtStateX == PGM_PAGE_HNDL_VIRT_STATE_ALL )




/** @def PGM_PAGE_GET_TRACKING
 * Gets the packed shadow page pool tracking data associated with a guest page.
 * @returns uint16_t containing the data.
 * @param   pPage       Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_TRACKING(pPage) \
    ( *((uint16_t *)&(pPage)->HCPhysX + 3) )

/** @def PGM_PAGE_SET_TRACKING
 * Sets the packed shadow page pool tracking data associated with a guest page.
 * @param   pPage               Pointer to the physical guest page tracking structure.
 * @param   u16TrackingData     The tracking data to store.
 */
#define PGM_PAGE_SET_TRACKING(pPage, u16TrackingData) \
    do { *((uint16_t *)&(pPage)->HCPhysX + 3) = (u16TrackingData); } while (0)

/** @def PGM_PAGE_GET_TD_CREFS
 * Gets the @a cRefs tracking data member.
 * @returns cRefs.
 * @param   pPage               Pointer to the physical guest page tracking structure.
 */
#define PGM_PAGE_GET_TD_CREFS(pPage) \
    ((PGM_PAGE_GET_TRACKING(pPage) >> PGMPOOL_TD_CREFS_SHIFT) & PGMPOOL_TD_CREFS_MASK)

#define PGM_PAGE_GET_TD_IDX(pPage) \
    ((PGM_PAGE_GET_TRACKING(pPage) >> PGMPOOL_TD_IDX_SHIFT)   & PGMPOOL_TD_IDX_MASK)

/**
 * Ram range for GC Phys to HC Phys conversion.
 *
 * Can be used for HC Virt to GC Phys and HC Virt to HC Phys
 * conversions too, but we'll let MM handle that for now.
 *
 * This structure is used by linked lists in both GC and HC.
 */
typedef struct PGMRAMRANGE
{
    /** Start of the range. Page aligned. */
    RTGCPHYS                            GCPhys;
    /** Size of the range. (Page aligned of course). */
    RTGCPHYS                            cb;
    /** Pointer to the next RAM range - for R3. */
    R3PTRTYPE(struct PGMRAMRANGE *)     pNextR3;
    /** Pointer to the next RAM range - for R0. */
    R0PTRTYPE(struct PGMRAMRANGE *)     pNextR0;
    /** Pointer to the next RAM range - for RC. */
    RCPTRTYPE(struct PGMRAMRANGE *)     pNextRC;
    /** PGM_RAM_RANGE_FLAGS_* flags. */
    uint32_t                            fFlags;
    /** Last address in the range (inclusive). Page aligned (-1). */
    RTGCPHYS                            GCPhysLast;
    /** Start of the HC mapping of the range. This is only used for MMIO2. */
    R3PTRTYPE(void *)                   pvR3;
    /** The range description. */
    R3PTRTYPE(const char *)             pszDesc;
    /** Pointer to self - R0 pointer. */
    R0PTRTYPE(struct PGMRAMRANGE *)     pSelfR0;
    /** Pointer to self - RC pointer. */
    RCPTRTYPE(struct PGMRAMRANGE *)     pSelfRC;
    /** Padding to make aPage aligned on sizeof(PGMPAGE). */
    uint32_t                            au32Alignment2[HC_ARCH_BITS == 32 ? 2 : 1];
    /** Array of physical guest page tracking structures. */
    PGMPAGE                             aPages[1];
} PGMRAMRANGE;
/** Pointer to Ram range for GC Phys to HC Phys conversion. */
typedef PGMRAMRANGE *PPGMRAMRANGE;

/** @name PGMRAMRANGE::fFlags
 * @{ */
/** The RAM range is floating around as an independent guest mapping. */
#define PGM_RAM_RANGE_FLAGS_FLOATING        RT_BIT(20)
/** @} */


/**
 * Per page tracking structure for ROM image.
 *
 * A ROM image may have a shadow page, in which case we may have
 * two pages backing it. This structure contains the PGMPAGE for
 * both while PGMRAMRANGE have a copy of the active one. It is
 * important that these aren't out of sync in any regard other
 * than page pool tracking data.
 */
typedef struct PGMROMPAGE
{
    /** The page structure for the virgin ROM page. */
    PGMPAGE     Virgin;
    /** The page structure for the shadow RAM page. */
    PGMPAGE     Shadow;
    /** The current protection setting. */
    PGMROMPROT  enmProt;
    /** Pad the structure size to a multiple of 8. */
    uint32_t    u32Padding;
} PGMROMPAGE;
/** Pointer to a ROM page tracking structure. */
typedef PGMROMPAGE *PPGMROMPAGE;


/**
 * A registered ROM image.
 *
 * This is needed to keep track of ROM image since they generally
 * intrude into a PGMRAMRANGE. It also keeps track of additional
 * info like the two page sets (read-only virgin and read-write shadow),
 * the current state of each page.
 *
 * Because access handlers cannot easily be executed in a different
 * context, the ROM ranges needs to be accessible and in all contexts.
 */
typedef struct PGMROMRANGE
{
    /** Pointer to the next range - R3. */
    R3PTRTYPE(struct PGMROMRANGE *)     pNextR3;
    /** Pointer to the next range - R0. */
    R0PTRTYPE(struct PGMROMRANGE *)     pNextR0;
    /** Pointer to the next range - RC. */
    RCPTRTYPE(struct PGMROMRANGE *)     pNextRC;
    /** Pointer alignment */
    RTRCPTR                             GCPtrAlignment;
    /** Address of the range. */
    RTGCPHYS                            GCPhys;
    /** Address of the last byte in the range. */
    RTGCPHYS                            GCPhysLast;
    /** Size of the range. */
    RTGCPHYS                            cb;
    /** The flags (PGMPHYS_ROM_FLAG_*). */
    uint32_t                            fFlags;
    /** Alignment padding ensuring that aPages is sizeof(PGMROMPAGE) aligned. */
    uint32_t                            au32Alignemnt[HC_ARCH_BITS == 32 ? 7 : 3];
    /** Pointer to the original bits when PGMPHYS_ROM_FLAGS_PERMANENT_BINARY was specified.
     * This is used for strictness checks. */
    R3PTRTYPE(const void *)             pvOriginal;
    /** The ROM description. */
    R3PTRTYPE(const char *)             pszDesc;
    /** The per page tracking structures. */
    PGMROMPAGE                          aPages[1];
} PGMROMRANGE;
/** Pointer to a ROM range. */
typedef PGMROMRANGE *PPGMROMRANGE;


/**
 * A registered MMIO2 (= Device RAM) range.
 *
 * There are a few reason why we need to keep track of these
 * registrations. One of them is the deregistration & cleanup
 * stuff, while another is that the PGMRAMRANGE associated with
 * such a region may have to be removed from the ram range list.
 *
 * Overlapping with a RAM range has to be 100% or none at all. The
 * pages in the existing RAM range must not be ROM nor MMIO. A guru
 * meditation will be raised if a partial overlap or an overlap of
 * ROM pages is encountered. On an overlap we will free all the
 * existing RAM pages and put in the ram range pages instead.
 */
typedef struct PGMMMIO2RANGE
{
    /** The owner of the range. (a device) */
    PPDMDEVINSR3                        pDevInsR3;
    /** Pointer to the ring-3 mapping of the allocation. */
    RTR3PTR                             pvR3;
    /** Pointer to the next range - R3. */
    R3PTRTYPE(struct PGMMMIO2RANGE *)   pNextR3;
    /** Whether it's mapped or not. */
    bool                                fMapped;
    /** Whether it's overlapping or not. */
    bool                                fOverlapping;
    /** The PCI region number.
     * @remarks This ASSUMES that nobody will ever really need to have multiple
     *          PCI devices with matching MMIO region numbers on a single device. */
    uint8_t                             iRegion;
    /** Alignment padding for putting the ram range on a PGMPAGE alignment boundrary. */
    uint8_t                             abAlignemnt[HC_ARCH_BITS == 32 ? 1 : 5];
    /** The associated RAM range. */
    PGMRAMRANGE                         RamRange;
} PGMMMIO2RANGE;
/** Pointer to a MMIO2 range. */
typedef PGMMMIO2RANGE *PPGMMMIO2RANGE;




/**
 * PGMPhysRead/Write cache entry
 */
typedef struct PGMPHYSCACHEENTRY
{
    /** R3 pointer to physical page. */
    R3PTRTYPE(uint8_t *)                pbR3;
    /** GC Physical address for cache entry */
    RTGCPHYS                            GCPhys;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    RTGCPHYS                            u32Padding0; /**< alignment padding. */
#endif
} PGMPHYSCACHEENTRY;

/**
 * PGMPhysRead/Write cache to reduce REM memory access overhead
 */
typedef struct PGMPHYSCACHE
{
    /** Bitmap of valid cache entries */
    uint64_t                            aEntries;
    /** Cache entries */
    PGMPHYSCACHEENTRY                   Entry[PGM_MAX_PHYSCACHE_ENTRIES];
} PGMPHYSCACHE;


/** Pointer to an allocation chunk ring-3 mapping. */
typedef struct PGMCHUNKR3MAP *PPGMCHUNKR3MAP;
/** Pointer to an allocation chunk ring-3 mapping pointer. */
typedef PPGMCHUNKR3MAP *PPPGMCHUNKR3MAP;

/**
 * Ring-3 tracking structore for an allocation chunk ring-3 mapping.
 *
 * The primary tree (Core) uses the chunk id as key.
 * The secondary tree (AgeCore) is used for ageing and uses ageing sequence number as key.
 */
typedef struct PGMCHUNKR3MAP
{
    /** The key is the chunk id. */
    AVLU32NODECORE                      Core;
    /** The key is the ageing sequence number. */
    AVLLU32NODECORE                     AgeCore;
    /** The current age thingy. */
    uint32_t                            iAge;
    /** The current reference count. */
    uint32_t volatile                   cRefs;
    /** The current permanent reference count. */
    uint32_t volatile                   cPermRefs;
    /** The mapping address. */
    void                               *pv;
} PGMCHUNKR3MAP;

/**
 * Allocation chunk ring-3 mapping TLB entry.
 */
typedef struct PGMCHUNKR3MAPTLBE
{
    /** The chunk id. */
    uint32_t volatile                   idChunk;
#if HC_ARCH_BITS == 64
    uint32_t                            u32Padding; /**< alignment padding. */
#endif
    /** The chunk map. */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    R3PTRTYPE(PPGMCHUNKR3MAP) volatile  pChunk;
#else
    R3R0PTRTYPE(PPGMCHUNKR3MAP) volatile  pChunk;
#endif
} PGMCHUNKR3MAPTLBE;
/** Pointer to the an allocation chunk ring-3 mapping TLB entry. */
typedef PGMCHUNKR3MAPTLBE *PPGMCHUNKR3MAPTLBE;

/** The number of TLB entries in PGMCHUNKR3MAPTLB.
 * @remark Must be a power of two value. */
#define PGM_CHUNKR3MAPTLB_ENTRIES   32

/**
 * Allocation chunk ring-3 mapping TLB.
 *
 * @remarks We use a TLB to speed up lookups by avoiding walking the AVL.
 *          At first glance this might look kinda odd since AVL trees are
 *          supposed to give the most optimial lookup times of all trees
 *          due to their balancing. However, take a tree with 1023 nodes
 *          in it, that's 10 levels, meaning that most searches has to go
 *          down 9 levels before they find what they want. This isn't fast
 *          compared to a TLB hit. There is the factor of cache misses,
 *          and of course the problem with trees and branch prediction.
 *          This is why we use TLBs in front of most of the trees.
 *
 * @todo    Generalize this TLB + AVL stuff, shouldn't be all that
 *          difficult when we switch to the new inlined AVL trees (from kStuff).
 */
typedef struct PGMCHUNKR3MAPTLB
{
    /** The TLB entries. */
    PGMCHUNKR3MAPTLBE   aEntries[PGM_CHUNKR3MAPTLB_ENTRIES];
} PGMCHUNKR3MAPTLB;

/**
 * Calculates the index of a guest page in the Ring-3 Chunk TLB.
 * @returns Chunk TLB index.
 * @param   idChunk         The Chunk ID.
 */
#define PGM_CHUNKR3MAPTLB_IDX(idChunk)     ( (idChunk) & (PGM_CHUNKR3MAPTLB_ENTRIES - 1) )


/**
 * Ring-3 guest page mapping TLB entry.
 * @remarks used in ring-0 as well at the moment.
 */
typedef struct PGMPAGER3MAPTLBE
{
    /** Address of the page. */
    RTGCPHYS volatile                   GCPhys;
    /** The guest page. */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    R3PTRTYPE(PPGMPAGE) volatile        pPage;
#else
    R3R0PTRTYPE(PPGMPAGE) volatile      pPage;
#endif
    /** Pointer to the page mapping tracking structure, PGMCHUNKR3MAP. */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    R3PTRTYPE(PPGMCHUNKR3MAP) volatile  pMap;
#else
    R3R0PTRTYPE(PPGMCHUNKR3MAP) volatile pMap;
#endif
    /** The address */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    R3PTRTYPE(void *) volatile          pv;
#else
    R3R0PTRTYPE(void *) volatile        pv;
#endif
#if HC_ARCH_BITS == 32
    uint32_t                            u32Padding; /**< alignment padding. */
#endif
} PGMPAGER3MAPTLBE;
/** Pointer to an entry in the HC physical TLB. */
typedef PGMPAGER3MAPTLBE *PPGMPAGER3MAPTLBE;


/** The number of entries in the ring-3 guest page mapping TLB.
 * @remarks The value must be a power of two. */
#define PGM_PAGER3MAPTLB_ENTRIES 64

/**
 * Ring-3 guest page mapping TLB.
 * @remarks used in ring-0 as well at the moment.
 */
typedef struct PGMPAGER3MAPTLB
{
    /** The TLB entries. */
    PGMPAGER3MAPTLBE            aEntries[PGM_PAGER3MAPTLB_ENTRIES];
} PGMPAGER3MAPTLB;
/** Pointer to the ring-3 guest page mapping TLB. */
typedef PGMPAGER3MAPTLB *PPGMPAGER3MAPTLB;

/**
 * Calculates the index of the TLB entry for the specified guest page.
 * @returns Physical TLB index.
 * @param   GCPhys      The guest physical address.
 */
#define PGM_PAGER3MAPTLB_IDX(GCPhys)    ( ((GCPhys) >> PAGE_SHIFT) & (PGM_PAGER3MAPTLB_ENTRIES - 1) )


/**
 * Mapping cache usage set entry.
 *
 * @remarks 16-bit ints was choosen as the set is not expected to be used beyond
 *          the dynamic ring-0 and (to some extent) raw-mode context mapping
 *          cache. If it's extended to include ring-3, well, then something will
 *          have be changed here...
 */
typedef struct PGMMAPSETENTRY
{
    /** The mapping cache index. */
    uint16_t                    iPage;
    /** The number of references.
     * The max is UINT16_MAX - 1. */
    uint16_t                    cRefs;
#if HC_ARCH_BITS == 64
    uint32_t                    alignment;
#endif
    /** Pointer to the page. */
    RTR0PTR                     pvPage;
    /** The physical address for this entry. */
    RTHCPHYS                    HCPhys;
} PGMMAPSETENTRY;
/** Pointer to a mapping cache usage set entry. */
typedef PGMMAPSETENTRY *PPGMMAPSETENTRY;

/**
 * Mapping cache usage set.
 *
 * This is used in ring-0 and the raw-mode context to track dynamic mappings
 * done during exits / traps.  The set is
 */
typedef struct PGMMAPSET
{
    /** The number of occupied entries.
     * This is PGMMAPSET_CLOSED if the set is closed and we're not supposed to do
     * dynamic mappings. */
    uint32_t                    cEntries;
    /** The start of the current subset.
     * This is UINT32_MAX if no subset is currently open. */
    uint32_t                    iSubset;
    /** The index of the current CPU, only valid if the set is open. */
    int32_t                     iCpu;
#if HC_ARCH_BITS == 64
    uint32_t                    alignment;
#endif
    /** The entries. */
    PGMMAPSETENTRY              aEntries[64];
    /** HCPhys -> iEntry fast lookup table.
     * Use PGMMAPSET_HASH for hashing.
     * The entries may or may not be valid, check against cEntries. */
    uint8_t                     aiHashTable[128];
} PGMMAPSET;
/** Pointer to the mapping cache set. */
typedef PGMMAPSET *PPGMMAPSET;

/** PGMMAPSET::cEntries value for a closed set. */
#define PGMMAPSET_CLOSED            UINT32_C(0xdeadc0fe)

/** Hash function for aiHashTable. */
#define PGMMAPSET_HASH(HCPhys)      (((HCPhys) >> PAGE_SHIFT) & 127)

/** The max fill size (strict builds). */
#define PGMMAPSET_MAX_FILL          (64U * 80U / 100U)


/** @name Context neutrual page mapper TLB.
 *
 * Hoping to avoid some code and bug duplication parts of the GCxxx->CCPtr
 * code is writting in a kind of context neutrual way. Time will show whether
 * this actually makes sense or not...
 *
 * @todo this needs to be reconsidered and dropped/redone since the ring-0
 *       context ends up using a global mapping cache on some platforms
 *       (darwin).
 *
 * @{ */
/** @typedef PPGMPAGEMAPTLB
 * The page mapper TLB pointer type for the current context. */
/** @typedef PPGMPAGEMAPTLB
 * The page mapper TLB entry pointer type for the current context. */
/** @typedef PPGMPAGEMAPTLB
 * The page mapper TLB entry pointer pointer type for the current context. */
/** @def PGM_PAGEMAPTLB_ENTRIES
 * The number of TLB entries in the page mapper TLB for the current context. */
/** @def PGM_PAGEMAPTLB_IDX
 * Calculate the TLB index for a guest physical address.
 * @returns The TLB index.
 * @param   GCPhys      The guest physical address. */
/** @typedef PPGMPAGEMAP
 * Pointer to a page mapper unit for current context. */
/** @typedef PPPGMPAGEMAP
 * Pointer to a page mapper unit pointer for current context. */
#ifdef IN_RC
// typedef PPGMPAGEGCMAPTLB               PPGMPAGEMAPTLB;
// typedef PPGMPAGEGCMAPTLBE              PPGMPAGEMAPTLBE;
// typedef PPGMPAGEGCMAPTLBE             *PPPGMPAGEMAPTLBE;
# define PGM_PAGEMAPTLB_ENTRIES         PGM_PAGEGCMAPTLB_ENTRIES
# define PGM_PAGEMAPTLB_IDX(GCPhys)     PGM_PAGEGCMAPTLB_IDX(GCPhys)
 typedef void *                         PPGMPAGEMAP;
 typedef void **                        PPPGMPAGEMAP;
//#elif IN_RING0
// typedef PPGMPAGER0MAPTLB               PPGMPAGEMAPTLB;
// typedef PPGMPAGER0MAPTLBE              PPGMPAGEMAPTLBE;
// typedef PPGMPAGER0MAPTLBE             *PPPGMPAGEMAPTLBE;
//# define PGM_PAGEMAPTLB_ENTRIES         PGM_PAGER0MAPTLB_ENTRIES
//# define PGM_PAGEMAPTLB_IDX(GCPhys)     PGM_PAGER0MAPTLB_IDX(GCPhys)
// typedef PPGMCHUNKR0MAP                 PPGMPAGEMAP;
// typedef PPPGMCHUNKR0MAP                PPPGMPAGEMAP;
#else
 typedef PPGMPAGER3MAPTLB               PPGMPAGEMAPTLB;
 typedef PPGMPAGER3MAPTLBE              PPGMPAGEMAPTLBE;
 typedef PPGMPAGER3MAPTLBE             *PPPGMPAGEMAPTLBE;
# define PGM_PAGEMAPTLB_ENTRIES         PGM_PAGER3MAPTLB_ENTRIES
# define PGM_PAGEMAPTLB_IDX(GCPhys)     PGM_PAGER3MAPTLB_IDX(GCPhys)
 typedef PPGMCHUNKR3MAP                 PPGMPAGEMAP;
 typedef PPPGMCHUNKR3MAP                PPPGMPAGEMAP;
#endif
/** @} */


/** @name PGM Pool Indexes.
 * Aka. the unique shadow page identifier.
 * @{ */
/** NIL page pool IDX. */
#define NIL_PGMPOOL_IDX                 0
/** The first normal index. */
#define PGMPOOL_IDX_FIRST_SPECIAL       1
/** Page directory (32-bit root). */
#define PGMPOOL_IDX_PD                  1
/** Page Directory Pointer Table (PAE root). */
#define PGMPOOL_IDX_PDPT                2
/** AMD64 CR3 level index.*/
#define PGMPOOL_IDX_AMD64_CR3           3
/** Nested paging root.*/
#define PGMPOOL_IDX_NESTED_ROOT         4
/** The first normal index. */
#define PGMPOOL_IDX_FIRST               5
/** The last valid index. (inclusive, 14 bits) */
#define PGMPOOL_IDX_LAST                0x3fff
/** @} */

/** The NIL index for the parent chain. */
#define NIL_PGMPOOL_USER_INDEX          ((uint16_t)0xffff)

/**
 * Node in the chain linking a shadowed page to it's parent (user).
 */
#pragma pack(1)
typedef struct PGMPOOLUSER
{
    /** The index to the next item in the chain. NIL_PGMPOOL_USER_INDEX is no next. */
    uint16_t            iNext;
    /** The user page index. */
    uint16_t            iUser;
    /** Index into the user table. */
    uint32_t            iUserTable;
} PGMPOOLUSER, *PPGMPOOLUSER;
typedef const PGMPOOLUSER *PCPGMPOOLUSER;
#pragma pack()


/** The NIL index for the phys ext chain. */
#define NIL_PGMPOOL_PHYSEXT_INDEX       ((uint16_t)0xffff)

/**
 * Node in the chain of physical cross reference extents.
 * @todo Calling this an 'extent' is not quite right, find a better name.
 */
#pragma pack(1)
typedef struct PGMPOOLPHYSEXT
{
    /** The index to the next item in the chain. NIL_PGMPOOL_PHYSEXT_INDEX is no next. */
    uint16_t            iNext;
    /** The user page index. */
    uint16_t            aidx[3];
} PGMPOOLPHYSEXT, *PPGMPOOLPHYSEXT;
typedef const PGMPOOLPHYSEXT *PCPGMPOOLPHYSEXT;
#pragma pack()


/**
 * The kind of page that's being shadowed.
 */
typedef enum PGMPOOLKIND
{
    /** The virtual invalid 0 entry. */
    PGMPOOLKIND_INVALID = 0,
    /** The entry is free (=unused). */
    PGMPOOLKIND_FREE,

    /** Shw: 32-bit page table;     Gst: no paging  */
    PGMPOOLKIND_32BIT_PT_FOR_PHYS,
    /** Shw: 32-bit page table;     Gst: 32-bit page table.  */
    PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT,
    /** Shw: 32-bit page table;     Gst: 4MB page.  */
    PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB,
    /** Shw: PAE page table;        Gst: no paging  */
    PGMPOOLKIND_PAE_PT_FOR_PHYS,
    /** Shw: PAE page table;        Gst: 32-bit page table. */
    PGMPOOLKIND_PAE_PT_FOR_32BIT_PT,
    /** Shw: PAE page table;        Gst: Half of a 4MB page.  */
    PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB,
    /** Shw: PAE page table;        Gst: PAE page table. */
    PGMPOOLKIND_PAE_PT_FOR_PAE_PT,
    /** Shw: PAE page table;        Gst: 2MB page.  */
    PGMPOOLKIND_PAE_PT_FOR_PAE_2MB,

    /** Shw: 32-bit page directory. Gst: 32-bit page directory. */
    PGMPOOLKIND_32BIT_PD,
    /** Shw: 32-bit page directory. Gst: no paging. */
    PGMPOOLKIND_32BIT_PD_PHYS,
    /** Shw: PAE page directory 0;  Gst: 32-bit page directory. */
    PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD,
    /** Shw: PAE page directory 1;  Gst: 32-bit page directory. */
    PGMPOOLKIND_PAE_PD1_FOR_32BIT_PD,
    /** Shw: PAE page directory 2;  Gst: 32-bit page directory. */
    PGMPOOLKIND_PAE_PD2_FOR_32BIT_PD,
    /** Shw: PAE page directory 3;  Gst: 32-bit page directory. */
    PGMPOOLKIND_PAE_PD3_FOR_32BIT_PD,
    /** Shw: PAE page directory;    Gst: PAE page directory. */
    PGMPOOLKIND_PAE_PD_FOR_PAE_PD,
    /** Shw: PAE page directory;    Gst: no paging. */
    PGMPOOLKIND_PAE_PD_PHYS,

    /** Shw: PAE page directory pointer table (legacy, 4 entries);  Gst 32 bits paging. */
    PGMPOOLKIND_PAE_PDPT_FOR_32BIT,
    /** Shw: PAE page directory pointer table (legacy, 4 entries);  Gst PAE PDPT. */
    PGMPOOLKIND_PAE_PDPT,
    /** Shw: PAE page directory pointer table (legacy, 4 entries);  Gst: no paging. */
    PGMPOOLKIND_PAE_PDPT_PHYS,

    /** Shw: 64-bit page directory pointer table;   Gst: 64-bit page directory pointer table. */
    PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT,
    /** Shw: 64-bit page directory pointer table;   Gst: no paging  */
    PGMPOOLKIND_64BIT_PDPT_FOR_PHYS,
    /** Shw: 64-bit page directory table;           Gst: 64-bit page directory table. */
    PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD,
    /** Shw: 64-bit page directory table;           Gst: no paging  */
    PGMPOOLKIND_64BIT_PD_FOR_PHYS, /* 22 */

    /** Shw: 64-bit PML4;                           Gst: 64-bit PML4. */
    PGMPOOLKIND_64BIT_PML4,

    /** Shw: EPT page directory pointer table;      Gst: no paging  */
    PGMPOOLKIND_EPT_PDPT_FOR_PHYS,
    /** Shw: EPT page directory table;              Gst: no paging  */
    PGMPOOLKIND_EPT_PD_FOR_PHYS,
    /** Shw: EPT page table;                        Gst: no paging  */
    PGMPOOLKIND_EPT_PT_FOR_PHYS,

    /** Shw: Root Nested paging table. */
    PGMPOOLKIND_ROOT_NESTED,

    /** The last valid entry. */
    PGMPOOLKIND_LAST = PGMPOOLKIND_ROOT_NESTED
} PGMPOOLKIND;


/**
 * The tracking data for a page in the pool.
 */
typedef struct PGMPOOLPAGE
{
    /** AVL node code with the (R3) physical address of this page. */
    AVLOHCPHYSNODECORE  Core;
    /** Pointer to the R3 mapping of the page. */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    R3PTRTYPE(void *)   pvPageR3;
#else
    R3R0PTRTYPE(void *) pvPageR3;
#endif
    /** The guest physical address. */
#if HC_ARCH_BITS == 32 && GC_ARCH_BITS == 64
    uint32_t            Alignment0;
#endif
    RTGCPHYS            GCPhys;
    /** The kind of page we're shadowing. (This is really a PGMPOOLKIND enum.) */
    uint8_t             enmKind;
    uint8_t             bPadding;
    /** The index of this page. */
    uint16_t            idx;
    /** The next entry in the list this page currently resides in.
     * It's either in the free list or in the GCPhys hash. */
    uint16_t            iNext;
#ifdef PGMPOOL_WITH_USER_TRACKING
    /** Head of the user chain. NIL_PGMPOOL_USER_INDEX if not currently in use. */
    uint16_t            iUserHead;
    /** The number of present entries. */
    uint16_t            cPresent;
    /** The first entry in the table which is present. */
    uint16_t            iFirstPresent;
#endif
#ifdef PGMPOOL_WITH_MONITORING
    /** The number of modifications to the monitored page. */
    uint16_t            cModifications;
    /** The next modified page. NIL_PGMPOOL_IDX if tail. */
    uint16_t            iModifiedNext;
    /** The previous modified page. NIL_PGMPOOL_IDX if head. */
    uint16_t            iModifiedPrev;
    /** The next page sharing access handler. NIL_PGMPOOL_IDX if tail. */
    uint16_t            iMonitoredNext;
    /** The previous page sharing access handler. NIL_PGMPOOL_IDX if head. */
    uint16_t            iMonitoredPrev;
#endif
#ifdef PGMPOOL_WITH_CACHE
    /** The next page in the age list. */
    uint16_t            iAgeNext;
    /** The previous page in the age list. */
    uint16_t            iAgePrev;
#endif /* PGMPOOL_WITH_CACHE */
    /** Used to indicate that the page is zeroed. */
    bool                fZeroed;
    /** Used to indicate that a PT has non-global entries. */
    bool                fSeenNonGlobal;
    /** Used to indicate that we're monitoring writes to the guest page. */
    bool                fMonitored;
    /** Used to indicate that the page is in the cache (e.g. in the GCPhys hash).
     * (All pages are in the age list.) */
    bool                fCached;
    /** This is used by the R3 access handlers when invoked by an async thread.
     * It's a hack required because of REMR3NotifyHandlerPhysicalDeregister. */
    bool volatile       fReusedFlushPending;
    /** Used to indicate that this page can't be flushed. Important for cr3 root pages or shadow pae pd pages). */
    bool                fLocked;
} PGMPOOLPAGE, *PPGMPOOLPAGE, **PPPGMPOOLPAGE;
/** Pointer to a const pool page. */
typedef PGMPOOLPAGE const *PCPGMPOOLPAGE;


#ifdef PGMPOOL_WITH_CACHE
/** The hash table size. */
# define PGMPOOL_HASH_SIZE      0x40
/** The hash function. */
# define PGMPOOL_HASH(GCPhys)   ( ((GCPhys) >> PAGE_SHIFT) & (PGMPOOL_HASH_SIZE - 1) )
#endif


/**
 * The shadow page pool instance data.
 *
 * It's all one big allocation made at init time, except for the
 * pages that is. The user nodes follows immediatly after the
 * page structures.
 */
typedef struct PGMPOOL
{
    /** The VM handle - R3 Ptr. */
    PVMR3                       pVMR3;
    /** The VM handle - R0 Ptr. */
    PVMR0                       pVMR0;
    /** The VM handle - RC Ptr. */
    PVMRC                       pVMRC;
    /** The max pool size. This includes the special IDs.  */
    uint16_t                    cMaxPages;
    /** The current pool size. */
    uint16_t                    cCurPages;
    /** The head of the free page list. */
    uint16_t                    iFreeHead;
    /* Padding. */
    uint16_t                    u16Padding;
#ifdef PGMPOOL_WITH_USER_TRACKING
    /** Head of the chain of free user nodes. */
    uint16_t                    iUserFreeHead;
    /** The number of user nodes we've allocated. */
    uint16_t                    cMaxUsers;
    /** The number of present page table entries in the entire pool. */
    uint32_t                    cPresent;
    /** Pointer to the array of user nodes - RC pointer. */
    RCPTRTYPE(PPGMPOOLUSER)     paUsersRC;
    /** Pointer to the array of user nodes - R3 pointer. */
    R3PTRTYPE(PPGMPOOLUSER)     paUsersR3;
    /** Pointer to the array of user nodes - R0 pointer. */
    R0PTRTYPE(PPGMPOOLUSER)     paUsersR0;
#endif /* PGMPOOL_WITH_USER_TRACKING */
#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    /** Head of the chain of free phys ext nodes. */
    uint16_t                    iPhysExtFreeHead;
    /** The number of user nodes we've allocated. */
    uint16_t                    cMaxPhysExts;
    /** Pointer to the array of physical xref extent - RC pointer. */
    RCPTRTYPE(PPGMPOOLPHYSEXT)  paPhysExtsRC;
    /** Pointer to the array of physical xref extent nodes - R3 pointer. */
    R3PTRTYPE(PPGMPOOLPHYSEXT)  paPhysExtsR3;
    /** Pointer to the array of physical xref extent nodes - R0 pointer. */
    R0PTRTYPE(PPGMPOOLPHYSEXT)  paPhysExtsR0;
#endif /* PGMPOOL_WITH_GCPHYS_TRACKING */
#ifdef PGMPOOL_WITH_CACHE
    /** Hash table for GCPhys addresses. */
    uint16_t                    aiHash[PGMPOOL_HASH_SIZE];
    /** The head of the age list. */
    uint16_t                    iAgeHead;
    /** The tail of the age list. */
    uint16_t                    iAgeTail;
    /** Set if the cache is enabled. */
    bool                        fCacheEnabled;
#endif /* PGMPOOL_WITH_CACHE */
#ifdef PGMPOOL_WITH_MONITORING
    /** Head of the list of modified pages. */
    uint16_t                    iModifiedHead;
    /** The current number of modified pages. */
    uint16_t                    cModifiedPages;
    /** Access handler, RC. */
    RCPTRTYPE(PFNPGMRCPHYSHANDLER)  pfnAccessHandlerRC;
    /** Access handler, R0. */
    R0PTRTYPE(PFNPGMR0PHYSHANDLER)  pfnAccessHandlerR0;
    /** Access handler, R3. */
    R3PTRTYPE(PFNPGMR3PHYSHANDLER)  pfnAccessHandlerR3;
    /** The access handler description (HC ptr). */
    R3PTRTYPE(const char *)         pszAccessHandler;
#endif /* PGMPOOL_WITH_MONITORING */
    /** The number of pages currently in use. */
    uint16_t                    cUsedPages;
#ifdef VBOX_WITH_STATISTICS
    /** The high wather mark for cUsedPages. */
    uint16_t                    cUsedPagesHigh;
    uint32_t                    Alignment1;         /**< Align the next member on a 64-bit boundrary. */
    /** Profiling pgmPoolAlloc(). */
    STAMPROFILEADV              StatAlloc;
    /** Profiling pgmPoolClearAll(). */
    STAMPROFILE                 StatClearAll;
    /** Profiling pgmPoolFlushAllInt(). */
    STAMPROFILE                 StatFlushAllInt;
    /** Profiling pgmPoolFlushPage(). */
    STAMPROFILE                 StatFlushPage;
    /** Profiling pgmPoolFree(). */
    STAMPROFILE                 StatFree;
    /** Profiling time spent zeroing pages. */
    STAMPROFILE                 StatZeroPage;
# ifdef PGMPOOL_WITH_USER_TRACKING
    /** Profiling of pgmPoolTrackDeref. */
    STAMPROFILE                 StatTrackDeref;
    /** Profiling pgmTrackFlushGCPhysPT. */
    STAMPROFILE                 StatTrackFlushGCPhysPT;
    /** Profiling pgmTrackFlushGCPhysPTs. */
    STAMPROFILE                 StatTrackFlushGCPhysPTs;
    /** Profiling pgmTrackFlushGCPhysPTsSlow. */
    STAMPROFILE                 StatTrackFlushGCPhysPTsSlow;
    /** Number of times we've been out of user records. */
    STAMCOUNTER                 StatTrackFreeUpOneUser;
# endif
# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    /** Profiling deref activity related tracking GC physical pages. */
    STAMPROFILE                 StatTrackDerefGCPhys;
    /** Number of linear searches for a HCPhys in the ram ranges. */
    STAMCOUNTER                 StatTrackLinearRamSearches;
    /** The number of failing pgmPoolTrackPhysExtAlloc calls. */
    STAMCOUNTER                 StamTrackPhysExtAllocFailures;
# endif
# ifdef PGMPOOL_WITH_MONITORING
    /** Profiling the RC/R0 access handler. */
    STAMPROFILE                 StatMonitorRZ;
    /** Times we've failed interpreting the instruction. */
    STAMCOUNTER                 StatMonitorRZEmulateInstr;
    /** Profiling the pgmPoolFlushPage calls made from the RC/R0 access handler. */
    STAMPROFILE                 StatMonitorRZFlushPage;
    /** Times we've detected fork(). */
    STAMCOUNTER                 StatMonitorRZFork;
    /** Profiling the RC/R0 access we've handled (except REP STOSD). */
    STAMPROFILE                 StatMonitorRZHandled;
    /** Times we've failed interpreting a patch code instruction. */
    STAMCOUNTER                 StatMonitorRZIntrFailPatch1;
    /** Times we've failed interpreting a patch code instruction during flushing. */
    STAMCOUNTER                 StatMonitorRZIntrFailPatch2;
    /** The number of times we've seen rep prefixes we can't handle. */
    STAMCOUNTER                 StatMonitorRZRepPrefix;
    /** Profiling the REP STOSD cases we've handled. */
    STAMPROFILE                 StatMonitorRZRepStosd;

    /** Profiling the R3 access handler. */
    STAMPROFILE                 StatMonitorR3;
    /** Times we've failed interpreting the instruction. */
    STAMCOUNTER                 StatMonitorR3EmulateInstr;
    /** Profiling the pgmPoolFlushPage calls made from the R3 access handler. */
    STAMPROFILE                 StatMonitorR3FlushPage;
    /** Times we've detected fork(). */
    STAMCOUNTER                 StatMonitorR3Fork;
    /** Profiling the R3 access we've handled (except REP STOSD). */
    STAMPROFILE                 StatMonitorR3Handled;
    /** The number of times we've seen rep prefixes we can't handle. */
    STAMCOUNTER                 StatMonitorR3RepPrefix;
    /** Profiling the REP STOSD cases we've handled. */
    STAMPROFILE                 StatMonitorR3RepStosd;
    /** The number of times we're called in an async thread an need to flush. */
    STAMCOUNTER                 StatMonitorR3Async;
    /** The high wather mark for cModifiedPages. */
    uint16_t                    cModifiedPagesHigh;
    uint16_t                    Alignment2[3];      /**< Align the next member on a 64-bit boundrary. */
# endif
# ifdef PGMPOOL_WITH_CACHE
    /** The number of cache hits. */
    STAMCOUNTER                 StatCacheHits;
    /** The number of cache misses. */
    STAMCOUNTER                 StatCacheMisses;
    /** The number of times we've got a conflict of 'kind' in the cache. */
    STAMCOUNTER                 StatCacheKindMismatches;
    /** Number of times we've been out of pages. */
    STAMCOUNTER                 StatCacheFreeUpOne;
    /** The number of cacheable allocations. */
    STAMCOUNTER                 StatCacheCacheable;
    /** The number of uncacheable allocations. */
    STAMCOUNTER                 StatCacheUncacheable;
# endif
#elif HC_ARCH_BITS == 64
    uint32_t                    Alignment3;         /**< Align the next member on a 64-bit boundrary. */
#endif
    /** The AVL tree for looking up a page by its HC physical address. */
    AVLOHCPHYSTREE              HCPhysTree;
    uint32_t                    Alignment4;         /**< Align the next member on a 64-bit boundrary. */
    /** Array of pages. (cMaxPages in length)
     * The Id is the index into thist array.
     */
    PGMPOOLPAGE                 aPages[PGMPOOL_IDX_FIRST];
} PGMPOOL, *PPGMPOOL, **PPPGMPOOL;


/** @def PGMPOOL_PAGE_2_PTR
 * Maps a pool page pool into the current context.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   pPage   The pool page.
 *
 * @remark  In RC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#if defined(IN_RC)
# define PGMPOOL_PAGE_2_PTR(pVM, pPage)  pgmPoolMapPageInlined(&(pVM)->pgm.s, (pPage))
#elif defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define PGMPOOL_PAGE_2_PTR(pVM, pPage)  pgmPoolMapPageInlined(&(pVM)->pgm.s, (pPage))
#elif defined(VBOX_STRICT)
# define PGMPOOL_PAGE_2_PTR(pVM, pPage)  pgmPoolMapPageStrict(pPage)
DECLINLINE(void *) pgmPoolMapPageStrict(PPGMPOOLPAGE pPage)
{
    Assert(pPage && pPage->pvPageR3);
    return pPage->pvPageR3;
}
#else
# define PGMPOOL_PAGE_2_PTR(pVM, pPage)  ((pPage)->pvPageR3)
#endif

/** @def PGMPOOL_PAGE_2_PTR_BY_PGM
 * Maps a pool page pool into the current context.
 *
 * @returns VBox status code.
 * @param   pPGM    Pointer to the PGM instance data.
 * @param   pPage   The pool page.
 *
 * @remark  In RC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#if defined(IN_RC)
# define PGMPOOL_PAGE_2_PTR_BY_PGM(pPGM, pPage)  pgmPoolMapPageInlined(pPGM, (pPage))
#elif defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define PGMPOOL_PAGE_2_PTR_BY_PGM(pPGM, pPage)  pgmPoolMapPageInlined(pPGM, (pPage))
#else
# define PGMPOOL_PAGE_2_PTR_BY_PGM(pPGM, pPage)  PGMPOOL_PAGE_2_PTR(PGM2VM(pPGM), pPage)
#endif

/** @def PGMPOOL_PAGE_2_PTR_BY_PGMCPU
 * Maps a pool page pool into the current context.
 *
 * @returns VBox status code.
 * @param   pPGM    Pointer to the PGMCPU instance data.
 * @param   pPage   The pool page.
 *
 * @remark  In RC this uses PGMGCDynMapHCPage(), so it will consume of the
 *          small page window employeed by that function. Be careful.
 * @remark  There is no need to assert on the result.
 */
#if defined(IN_RC)
# define PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPage)  pgmPoolMapPageInlined(PGMCPU2PGM(pPGM), (pPage))
#elif defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
# define PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPage)  pgmPoolMapPageInlined(PGMCPU2PGM(pPGM), (pPage))
#else
# define PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPage)  PGMPOOL_PAGE_2_PTR(PGMCPU2VM(pPGM), pPage)
#endif


/** @name Per guest page tracking data.
 * This is currently as a 16-bit word in the PGMPAGE structure, the idea though
 * is to use more bits for it and split it up later on. But for now we'll play
 * safe and change as little as possible.
 *
 * The 16-bit word has two parts:
 *
 * The first 14-bit forms the @a idx field. It is either the index of a page in
 * the shadow page pool, or and index into the extent list.
 *
 * The 2 topmost bits makes up the @a cRefs field, which counts the number of
 * shadow page pool references to the page. If cRefs equals
 * PGMPOOL_CREFS_PHYSEXT, then the @a idx field is an indext into the extent
 * (misnomer) table and not the shadow page pool.
 *
 * See PGM_PAGE_GET_TRACKING and PGM_PAGE_SET_TRACKING for how to get and set
 * the 16-bit word.
 *
 * @{ */
/** The shift count for getting to the cRefs part. */
#define PGMPOOL_TD_CREFS_SHIFT          14
/** The mask applied after shifting the tracking data down by
 * PGMPOOL_TD_CREFS_SHIFT. */
#define PGMPOOL_TD_CREFS_MASK           0x3
/** The cRef value used to indiciate that the idx is the head of a
 * physical cross reference list. */
#define PGMPOOL_TD_CREFS_PHYSEXT        PGMPOOL_TD_CREFS_MASK
/** The shift used to get idx. */
#define PGMPOOL_TD_IDX_SHIFT            0
/** The mask applied to the idx after shifting down by PGMPOOL_TD_IDX_SHIFT. */
#define PGMPOOL_TD_IDX_MASK             0x3fff
/** The idx value when we're out of of PGMPOOLPHYSEXT entries or/and there are
 * simply too many mappings of this page. */
#define PGMPOOL_TD_IDX_OVERFLOWED       PGMPOOL_TD_IDX_MASK

/** @def PGMPOOL_TD_MAKE
 * Makes a 16-bit tracking data word.
 *
 * @returns tracking data.
 * @param   cRefs       The @a cRefs field. Must be within bounds!
 * @param   idx         The @a idx field. Must also be within bounds! */
#define PGMPOOL_TD_MAKE(cRefs, idx)     ( ((cRefs) << PGMPOOL_TD_CREFS_SHIFT) | (idx) )

/** @def PGMPOOL_TD_GET_CREFS
 * Get the @a cRefs field from a tracking data word.
 *
 * @returns The @a cRefs field
 * @param   u16         The tracking data word. */
#define PGMPOOL_TD_GET_CREFS(u16)       ( ((u16) >> PGMPOOL_TD_CREFS_SHIFT) & PGMPOOL_TD_CREFS_MASK )

/** @def PGMPOOL_TD_GET_IDX
 * Get the @a idx field from a tracking data word.
 *
 * @returns The @a idx field
 * @param   u16         The tracking data word. */
#define PGMPOOL_TD_GET_IDX(u16)         ( ((u16) >> PGMPOOL_TD_IDX_SHIFT)   & PGMPOOL_TD_IDX_MASK   )
/** @} */


/**
 * Trees are using self relative offsets as pointers.
 * So, all its data, including the root pointer, must be in the heap for HC and GC
 * to have the same layout.
 */
typedef struct PGMTREES
{
    /** Physical access handlers (AVL range+offsetptr tree). */
    AVLROGCPHYSTREE                 PhysHandlers;
    /** Virtual access handlers (AVL range + GC ptr tree). */
    AVLROGCPTRTREE                  VirtHandlers;
    /** Virtual access handlers (Phys range AVL range + offsetptr tree). */
    AVLROGCPHYSTREE                 PhysToVirtHandlers;
    /** Virtual access handlers for the hypervisor (AVL range + GC ptr tree). */
    AVLROGCPTRTREE                  HyperVirtHandlers;
} PGMTREES;
/** Pointer to PGM trees. */
typedef PGMTREES *PPGMTREES;


/** @name Paging mode macros
 * @{ */
#ifdef IN_RC
# define PGM_CTX(a,b)                   a##RC##b
# define PGM_CTX_STR(a,b)               a "GC" b
# define PGM_CTX_DECL(type)             VMMRCDECL(type)
#else
# ifdef IN_RING3
#  define PGM_CTX(a,b)                   a##R3##b
#  define PGM_CTX_STR(a,b)               a "R3" b
#  define PGM_CTX_DECL(type)             DECLCALLBACK(type)
# else
#  define PGM_CTX(a,b)                   a##R0##b
#  define PGM_CTX_STR(a,b)               a "R0" b
#  define PGM_CTX_DECL(type)             VMMDECL(type)
# endif
#endif

#define PGM_GST_NAME_REAL(name)         PGM_CTX(pgm,GstReal##name)
#define PGM_GST_NAME_RC_REAL_STR(name)  "pgmRCGstReal" #name
#define PGM_GST_NAME_R0_REAL_STR(name)  "pgmR0GstReal" #name
#define PGM_GST_NAME_PROT(name)         PGM_CTX(pgm,GstProt##name)
#define PGM_GST_NAME_RC_PROT_STR(name)  "pgmRCGstProt" #name
#define PGM_GST_NAME_R0_PROT_STR(name)  "pgmR0GstProt" #name
#define PGM_GST_NAME_32BIT(name)        PGM_CTX(pgm,Gst32Bit##name)
#define PGM_GST_NAME_RC_32BIT_STR(name) "pgmRCGst32Bit" #name
#define PGM_GST_NAME_R0_32BIT_STR(name) "pgmR0Gst32Bit" #name
#define PGM_GST_NAME_PAE(name)          PGM_CTX(pgm,GstPAE##name)
#define PGM_GST_NAME_RC_PAE_STR(name)   "pgmRCGstPAE" #name
#define PGM_GST_NAME_R0_PAE_STR(name)   "pgmR0GstPAE" #name
#define PGM_GST_NAME_AMD64(name)        PGM_CTX(pgm,GstAMD64##name)
#define PGM_GST_NAME_RC_AMD64_STR(name) "pgmRCGstAMD64" #name
#define PGM_GST_NAME_R0_AMD64_STR(name) "pgmR0GstAMD64" #name
#define PGM_GST_PFN(name, pVM)          ((pVCpu)->pgm.s.PGM_CTX(pfn,Gst##name))
#define PGM_GST_DECL(type, name)        PGM_CTX_DECL(type) PGM_GST_NAME(name)

#define PGM_SHW_NAME_32BIT(name)        PGM_CTX(pgm,Shw32Bit##name)
#define PGM_SHW_NAME_RC_32BIT_STR(name) "pgmRCShw32Bit" #name
#define PGM_SHW_NAME_R0_32BIT_STR(name) "pgmR0Shw32Bit" #name
#define PGM_SHW_NAME_PAE(name)          PGM_CTX(pgm,ShwPAE##name)
#define PGM_SHW_NAME_RC_PAE_STR(name)   "pgmRCShwPAE" #name
#define PGM_SHW_NAME_R0_PAE_STR(name)   "pgmR0ShwPAE" #name
#define PGM_SHW_NAME_AMD64(name)        PGM_CTX(pgm,ShwAMD64##name)
#define PGM_SHW_NAME_RC_AMD64_STR(name) "pgmRCShwAMD64" #name
#define PGM_SHW_NAME_R0_AMD64_STR(name) "pgmR0ShwAMD64" #name
#define PGM_SHW_NAME_NESTED(name)        PGM_CTX(pgm,ShwNested##name)
#define PGM_SHW_NAME_RC_NESTED_STR(name) "pgmRCShwNested" #name
#define PGM_SHW_NAME_R0_NESTED_STR(name) "pgmR0ShwNested" #name
#define PGM_SHW_NAME_EPT(name)          PGM_CTX(pgm,ShwEPT##name)
#define PGM_SHW_NAME_RC_EPT_STR(name)   "pgmRCShwEPT" #name
#define PGM_SHW_NAME_R0_EPT_STR(name)   "pgmR0ShwEPT" #name
#define PGM_SHW_DECL(type, name)        PGM_CTX_DECL(type) PGM_SHW_NAME(name)
#define PGM_SHW_PFN(name, pVM)          ((pVCpu)->pgm.s.PGM_CTX(pfn,Shw##name))

/*                   Shw_Gst */
#define PGM_BTH_NAME_32BIT_REAL(name)   PGM_CTX(pgm,Bth32BitReal##name)
#define PGM_BTH_NAME_32BIT_PROT(name)   PGM_CTX(pgm,Bth32BitProt##name)
#define PGM_BTH_NAME_32BIT_32BIT(name)  PGM_CTX(pgm,Bth32Bit32Bit##name)
#define PGM_BTH_NAME_PAE_REAL(name)     PGM_CTX(pgm,BthPAEReal##name)
#define PGM_BTH_NAME_PAE_PROT(name)     PGM_CTX(pgm,BthPAEProt##name)
#define PGM_BTH_NAME_PAE_32BIT(name)    PGM_CTX(pgm,BthPAE32Bit##name)
#define PGM_BTH_NAME_PAE_PAE(name)      PGM_CTX(pgm,BthPAEPAE##name)
#define PGM_BTH_NAME_AMD64_PROT(name)   PGM_CTX(pgm,BthAMD64Prot##name)
#define PGM_BTH_NAME_AMD64_AMD64(name)  PGM_CTX(pgm,BthAMD64AMD64##name)
#define PGM_BTH_NAME_NESTED_REAL(name)  PGM_CTX(pgm,BthNestedReal##name)
#define PGM_BTH_NAME_NESTED_PROT(name)  PGM_CTX(pgm,BthNestedProt##name)
#define PGM_BTH_NAME_NESTED_32BIT(name) PGM_CTX(pgm,BthNested32Bit##name)
#define PGM_BTH_NAME_NESTED_PAE(name)   PGM_CTX(pgm,BthNestedPAE##name)
#define PGM_BTH_NAME_NESTED_AMD64(name) PGM_CTX(pgm,BthNestedAMD64##name)
#define PGM_BTH_NAME_EPT_REAL(name)     PGM_CTX(pgm,BthEPTReal##name)
#define PGM_BTH_NAME_EPT_PROT(name)     PGM_CTX(pgm,BthEPTProt##name)
#define PGM_BTH_NAME_EPT_32BIT(name)    PGM_CTX(pgm,BthEPT32Bit##name)
#define PGM_BTH_NAME_EPT_PAE(name)      PGM_CTX(pgm,BthEPTPAE##name)
#define PGM_BTH_NAME_EPT_AMD64(name)    PGM_CTX(pgm,BthEPTAMD64##name)

#define PGM_BTH_NAME_RC_32BIT_REAL_STR(name)    "pgmRCBth32BitReal" #name
#define PGM_BTH_NAME_RC_32BIT_PROT_STR(name)    "pgmRCBth32BitProt" #name
#define PGM_BTH_NAME_RC_32BIT_32BIT_STR(name)   "pgmRCBth32Bit32Bit" #name
#define PGM_BTH_NAME_RC_PAE_REAL_STR(name)      "pgmRCBthPAEReal" #name
#define PGM_BTH_NAME_RC_PAE_PROT_STR(name)      "pgmRCBthPAEProt" #name
#define PGM_BTH_NAME_RC_PAE_32BIT_STR(name)     "pgmRCBthPAE32Bit" #name
#define PGM_BTH_NAME_RC_PAE_PAE_STR(name)       "pgmRCBthPAEPAE" #name
#define PGM_BTH_NAME_RC_AMD64_AMD64_STR(name)   "pgmRCBthAMD64AMD64" #name
#define PGM_BTH_NAME_RC_NESTED_REAL_STR(name)   "pgmRCBthNestedReal" #name
#define PGM_BTH_NAME_RC_NESTED_PROT_STR(name)   "pgmRCBthNestedProt" #name
#define PGM_BTH_NAME_RC_NESTED_32BIT_STR(name)  "pgmRCBthNested32Bit" #name
#define PGM_BTH_NAME_RC_NESTED_PAE_STR(name)    "pgmRCBthNestedPAE" #name
#define PGM_BTH_NAME_RC_NESTED_AMD64_STR(name)  "pgmRCBthNestedAMD64" #name
#define PGM_BTH_NAME_RC_EPT_REAL_STR(name)      "pgmRCBthEPTReal" #name
#define PGM_BTH_NAME_RC_EPT_PROT_STR(name)      "pgmRCBthEPTProt" #name
#define PGM_BTH_NAME_RC_EPT_32BIT_STR(name)     "pgmRCBthEPT32Bit" #name
#define PGM_BTH_NAME_RC_EPT_PAE_STR(name)       "pgmRCBthEPTPAE" #name
#define PGM_BTH_NAME_RC_EPT_AMD64_STR(name)     "pgmRCBthEPTAMD64" #name
#define PGM_BTH_NAME_R0_32BIT_REAL_STR(name)    "pgmR0Bth32BitReal" #name
#define PGM_BTH_NAME_R0_32BIT_PROT_STR(name)    "pgmR0Bth32BitProt" #name
#define PGM_BTH_NAME_R0_32BIT_32BIT_STR(name)   "pgmR0Bth32Bit32Bit" #name
#define PGM_BTH_NAME_R0_PAE_REAL_STR(name)      "pgmR0BthPAEReal" #name
#define PGM_BTH_NAME_R0_PAE_PROT_STR(name)      "pgmR0BthPAEProt" #name
#define PGM_BTH_NAME_R0_PAE_32BIT_STR(name)     "pgmR0BthPAE32Bit" #name
#define PGM_BTH_NAME_R0_PAE_PAE_STR(name)       "pgmR0BthPAEPAE" #name
#define PGM_BTH_NAME_R0_AMD64_PROT_STR(name)    "pgmR0BthAMD64Prot" #name
#define PGM_BTH_NAME_R0_AMD64_AMD64_STR(name)   "pgmR0BthAMD64AMD64" #name
#define PGM_BTH_NAME_R0_NESTED_REAL_STR(name)   "pgmR0BthNestedReal" #name
#define PGM_BTH_NAME_R0_NESTED_PROT_STR(name)   "pgmR0BthNestedProt" #name
#define PGM_BTH_NAME_R0_NESTED_32BIT_STR(name)  "pgmR0BthNested32Bit" #name
#define PGM_BTH_NAME_R0_NESTED_PAE_STR(name)    "pgmR0BthNestedPAE" #name
#define PGM_BTH_NAME_R0_NESTED_AMD64_STR(name)  "pgmR0BthNestedAMD64" #name
#define PGM_BTH_NAME_R0_EPT_REAL_STR(name)      "pgmR0BthEPTReal" #name
#define PGM_BTH_NAME_R0_EPT_PROT_STR(name)      "pgmR0BthEPTProt" #name
#define PGM_BTH_NAME_R0_EPT_32BIT_STR(name)     "pgmR0BthEPT32Bit" #name
#define PGM_BTH_NAME_R0_EPT_PAE_STR(name)       "pgmR0BthEPTPAE" #name
#define PGM_BTH_NAME_R0_EPT_AMD64_STR(name)     "pgmR0BthEPTAMD64" #name

#define PGM_BTH_DECL(type, name)        PGM_CTX_DECL(type) PGM_BTH_NAME(name)
#define PGM_BTH_PFN(name, pVM)          ((pVCpu)->pgm.s.PGM_CTX(pfn,Bth##name))
/** @} */

/**
 * Data for each paging mode.
 */
typedef struct PGMMODEDATA
{
    /** The guest mode type. */
    uint32_t                        uGstType;
    /** The shadow mode type. */
    uint32_t                        uShwType;

    /** @name Function pointers for Shadow paging.
     * @{
     */
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwRelocate,(PVM pVM, PVMCPU pVCpu, RTGCPTR offDelta));
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwExit,(PVM pVM, PVMCPU pVCpu));
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys));
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));

    DECLRCCALLBACKMEMBER(int,       pfnRCShwGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys));
    DECLRCCALLBACKMEMBER(int,       pfnRCShwModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));

    DECLR0CALLBACKMEMBER(int,       pfnR0ShwGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys));
    DECLR0CALLBACKMEMBER(int,       pfnR0ShwModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    /** @} */

    /** @name Function pointers for Guest paging.
     * @{
     */
    DECLR3CALLBACKMEMBER(int,       pfnR3GstRelocate,(PVM pVM, PVMCPU pVCpu, RTGCPTR offDelta));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstExit,(PVM pVM, PVMCPU pVCpu));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstGetPDE,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPde));
    DECLRCCALLBACKMEMBER(int,       pfnRCGstGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys));
    DECLRCCALLBACKMEMBER(int,       pfnRCGstModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    DECLRCCALLBACKMEMBER(int,       pfnRCGstGetPDE,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPde));
    DECLR0CALLBACKMEMBER(int,       pfnR0GstGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys));
    DECLR0CALLBACKMEMBER(int,       pfnR0GstModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    DECLR0CALLBACKMEMBER(int,       pfnR0GstGetPDE,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPde));
    /** @} */

    /** @name Function pointers for Both Shadow and Guest paging.
     * @{
     */
    DECLR3CALLBACKMEMBER(int,       pfnR3BthRelocate,(PVM pVM, PVMCPU pVCpu, RTGCPTR offDelta));
    /*                           no pfnR3BthTrap0eHandler */
    DECLR3CALLBACKMEMBER(int,       pfnR3BthInvalidatePage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthSyncCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthSyncPage,(PVM pVM, PVMCPU pVCpu, X86PDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uError));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthPrefetchPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthVerifyAccessSyncPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fFlags, unsigned uError));
#ifdef VBOX_STRICT
    DECLR3CALLBACKMEMBER(unsigned,  pfnR3BthAssertCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb));
#endif
    DECLR3CALLBACKMEMBER(int,       pfnR3BthMapCR3,(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysCR3));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthUnmapCR3,(PVM pVM, PVMCPU pVCpu));

    DECLRCCALLBACKMEMBER(int,       pfnRCBthTrap0eHandler,(PVM pVM, PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthInvalidatePage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthSyncCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthSyncPage,(PVM pVM, PVMCPU pVCpu, X86PDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uError));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthPrefetchPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthVerifyAccessSyncPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fFlags, unsigned uError));
#ifdef VBOX_STRICT
    DECLRCCALLBACKMEMBER(unsigned,  pfnRCBthAssertCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb));
#endif
    DECLRCCALLBACKMEMBER(int,       pfnRCBthMapCR3,(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysCR3));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthUnmapCR3,(PVM pVM, PVMCPU pVCpu));

    DECLR0CALLBACKMEMBER(int,       pfnR0BthTrap0eHandler,(PVM pVM, PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthInvalidatePage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthSyncCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthSyncPage,(PVM pVM, PVMCPU pVCpu, X86PDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uError));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthPrefetchPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthVerifyAccessSyncPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fFlags, unsigned uError));
#ifdef VBOX_STRICT
    DECLR0CALLBACKMEMBER(unsigned,  pfnR0BthAssertCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb));
#endif
    DECLR0CALLBACKMEMBER(int,       pfnR0BthMapCR3,(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysCR3));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthUnmapCR3,(PVM pVM, PVMCPU pVCpu));
    /** @} */
} PGMMODEDATA, *PPGMMODEDATA;



/**
 * Converts a PGM pointer into a VM pointer.
 * @returns Pointer to the VM structure the PGM is part of.
 * @param   pPGM   Pointer to PGM instance data.
 */
#define PGM2VM(pPGM)  ( (PVM)((char*)pPGM - pPGM->offVM) )

/**
 * PGM Data (part of VM)
 */
typedef struct PGM
{
    /** Offset to the VM structure. */
    RTINT                           offVM;
    /** Offset of the PGMCPU structure relative to VMCPU. */
    RTINT                           offVCpuPGM;

    /** @cfgm{PGM/RamPreAlloc, bool, false}
     * Whether to preallocate all the guest RAM or not. */
    bool                            fRamPreAlloc;
    /** Alignment padding. */
    bool                            afAlignment0[7];

    /** What needs syncing (PGM_SYNC_*).
     * This is used to queue operations for PGMSyncCR3, PGMInvalidatePage,
     * PGMFlushTLB, and PGMR3Load. */
    RTUINT                          fGlobalSyncFlags;

    /*
     * This will be redefined at least two more times before we're done, I'm sure.
     * The current code is only to get on with the coding.
     *   - 2004-06-10: initial version, bird.
     *   - 2004-07-02: 1st time, bird.
     *   - 2004-10-18: 2nd time, bird.
     *   - 2005-07-xx: 3rd time, bird.
     */

    /** Pointer to the page table entries for the dynamic page mapping area - GCPtr. */
    RCPTRTYPE(PX86PTE)              paDynPageMap32BitPTEsGC;
    /** Pointer to the page table entries for the dynamic page mapping area - GCPtr. */
    RCPTRTYPE(PX86PTEPAE)           paDynPageMapPaePTEsGC;

    /** The host paging mode. (This is what SUPLib reports.) */
    SUPPAGINGMODE                   enmHostMode;

    /** 4 MB page mask; 32 or 36 bits depending on PSE-36 (identical for all VCPUs) */
    RTGCPHYS                        GCPhys4MBPSEMask;

    /** Pointer to the list of RAM ranges (Phys GC -> Phys HC conversion) - for R3.
     * This is sorted by physical address and contains no overlapping ranges. */
    R3PTRTYPE(PPGMRAMRANGE)         pRamRangesR3;
    /** R0 pointer corresponding to PGM::pRamRangesR3. */
    R0PTRTYPE(PPGMRAMRANGE)         pRamRangesR0;
    /** RC pointer corresponding to PGM::pRamRangesR3. */
    RCPTRTYPE(PPGMRAMRANGE)         pRamRangesRC;
    RTRCPTR                         alignment4; /**< structure alignment. */

    /** Pointer to the list of ROM ranges - for R3.
     * This is sorted by physical address and contains no overlapping ranges. */
    R3PTRTYPE(PPGMROMRANGE)         pRomRangesR3;
    /** R0 pointer corresponding to PGM::pRomRangesR3. */
    R0PTRTYPE(PPGMROMRANGE)         pRomRangesR0;
    /** RC pointer corresponding to PGM::pRomRangesR3. */
    RCPTRTYPE(PPGMROMRANGE)         pRomRangesRC;
    /** Alignment padding. */
    RTRCPTR                         GCPtrPadding2;

    /** Pointer to the list of MMIO2 ranges - for R3.
     * Registration order. */
    R3PTRTYPE(PPGMMMIO2RANGE)       pMmio2RangesR3;

    /** PGM offset based trees - R3 Ptr. */
    R3PTRTYPE(PPGMTREES)            pTreesR3;
    /** PGM offset based trees - R0 Ptr. */
    R0PTRTYPE(PPGMTREES)            pTreesR0;
    /** PGM offset based trees - RC Ptr. */
    RCPTRTYPE(PPGMTREES)            pTreesRC;

    /** Linked list of GC mappings - for RC.
     * The list is sorted ascending on address.
     */
    RCPTRTYPE(PPGMMAPPING)          pMappingsRC;
    /** Linked list of GC mappings - for HC.
     * The list is sorted ascending on address.
     */
    R3PTRTYPE(PPGMMAPPING)          pMappingsR3;
    /** Linked list of GC mappings - for R0.
     * The list is sorted ascending on address.
     */
    R0PTRTYPE(PPGMMAPPING)          pMappingsR0;

    /** Pointer to the 5 page CR3 content mapping.
     * The first page is always the CR3 (in some form) while the 4 other pages
     * are used of the PDs in PAE mode. */
    RTGCPTR                         GCPtrCR3Mapping;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    uint32_t                        u32Alignment;
#endif

    /** Indicates that PGMR3FinalizeMappings has been called and that further
     * PGMR3MapIntermediate calls will be rejected. */
    bool                            fFinalizedMappings;
    /** If set no conflict checks are required.  (boolean) */
    bool                            fMappingsFixed;
    /** If set, then no mappings are put into the shadow page table. (boolean) */
    bool                            fDisableMappings;
    /** Size of fixed mapping */
    uint32_t                        cbMappingFixed;
    /** Base address (GC) of fixed mapping */
    RTGCPTR                         GCPtrMappingFixed;
    /** The address of the previous RAM range mapping. */
    RTGCPTR                         GCPtrPrevRamRangeMapping;

    /** @name Intermediate Context
     * @{ */
    /** Pointer to the intermediate page directory - Normal. */
    R3PTRTYPE(PX86PD)               pInterPD;
    /** Pointer to the intermedate page tables - Normal.
     * There are two page tables, one for the identity mapping and one for
     * the host context mapping (of the core code). */
    R3PTRTYPE(PX86PT)               apInterPTs[2];
    /** Pointer to the intermedate page tables - PAE. */
    R3PTRTYPE(PX86PTPAE)            apInterPaePTs[2];
    /** Pointer to the intermedate page directory - PAE. */
    R3PTRTYPE(PX86PDPAE)            apInterPaePDs[4];
    /** Pointer to the intermedate page directory - PAE. */
    R3PTRTYPE(PX86PDPT)             pInterPaePDPT;
    /** Pointer to the intermedate page-map level 4 - AMD64. */
    R3PTRTYPE(PX86PML4)             pInterPaePML4;
    /** Pointer to the intermedate page directory - AMD64. */
    R3PTRTYPE(PX86PDPT)             pInterPaePDPT64;
    /** The Physical Address (HC) of the intermediate Page Directory - Normal. */
    RTHCPHYS                        HCPhysInterPD;
    /** The Physical Address (HC) of the intermediate Page Directory Pointer Table - PAE. */
    RTHCPHYS                        HCPhysInterPaePDPT;
    /** The Physical Address (HC) of the intermediate Page Map Level 4 table - AMD64. */
    RTHCPHYS                        HCPhysInterPaePML4;
    /** @} */

    /** Base address of the dynamic page mapping area.
     * The array is MM_HYPER_DYNAMIC_SIZE bytes big.
     */
    RCPTRTYPE(uint8_t *)            pbDynPageMapBaseGC;
    /** The index of the last entry used in the dynamic page mapping area. */
    RTUINT                          iDynPageMapLast;
    /** Cache containing the last entries in the dynamic page mapping area.
     * The cache size is covering half of the mapping area. */
    RTHCPHYS                        aHCPhysDynPageMapCache[MM_HYPER_DYNAMIC_SIZE >> (PAGE_SHIFT + 1)];
    /** Keep a lock counter for the full (!) mapping area. */
    uint32_t                        aLockedDynPageMapCache[MM_HYPER_DYNAMIC_SIZE >> (PAGE_SHIFT)];

    /** The address of the ring-0 mapping cache if we're making use of it.  */
    RTR0PTR                         pvR0DynMapUsed;
#if HC_ARCH_BITS == 32
    RTR0PTR                         R0PtrPadding0;  /**< Alignment. */
#endif

    /** PGM critical section.
     * This protects the physical & virtual access handlers, ram ranges,
     * and the page flag updating (some of it anyway).
     */
    PDMCRITSECT                     CritSect;

    /** Pointer to SHW+GST mode data (function pointers).
     * The index into this table is made up from */
    R3PTRTYPE(PPGMMODEDATA)         paModeData;

    /** Shadow Page Pool - R3 Ptr. */
    R3PTRTYPE(PPGMPOOL)             pPoolR3;
    /** Shadow Page Pool - R0 Ptr. */
    R0PTRTYPE(PPGMPOOL)             pPoolR0;
    /** Shadow Page Pool - RC Ptr. */
    RCPTRTYPE(PPGMPOOL)             pPoolRC;

    /** We're not in a state which permits writes to guest memory.
     * (Only used in strict builds.) */
    bool                            fNoMorePhysWrites;

    /** Flush the cache on the next access. */
    bool                            fPhysCacheFlushPending;
/** @todo r=bird: Fix member names!*/
    /** PGMPhysRead cache */
    PGMPHYSCACHE                    pgmphysreadcache;
    /** PGMPhysWrite cache */
    PGMPHYSCACHE                    pgmphyswritecache;

    /**
     * Data associated with managing the ring-3 mappings of the allocation chunks.
     */
    struct
    {
        /** The chunk tree, ordered by chunk id. */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        R3PTRTYPE(PAVLU32NODECORE)  pTree;
#else
        R3R0PTRTYPE(PAVLU32NODECORE) pTree;
#endif
        /** The chunk mapping TLB. */
        PGMCHUNKR3MAPTLB            Tlb;
        /** The number of mapped chunks. */
        uint32_t                    c;
        /** The maximum number of mapped chunks.
         * @cfgm    PGM/MaxRing3Chunks */
        uint32_t                    cMax;
        /** The chunk age tree, ordered by ageing sequence number. */
        R3PTRTYPE(PAVLLU32NODECORE) pAgeTree;
        /** The current time. */
        uint32_t                    iNow;
        /** Number of pgmR3PhysChunkFindUnmapCandidate calls left to the next ageing. */
        uint32_t                    AgeingCountdown;
    }                               ChunkR3Map;

    /**
     * The page mapping TLB for ring-3 and (for the time being) ring-0.
     */
    PGMPAGER3MAPTLB                 PhysTlbHC;

    /** @name   The zero page.
     * @{ */
    /** The host physical address of the zero page. */
    RTHCPHYS                        HCPhysZeroPg;
    /** The ring-3 mapping of the zero page. */
    RTR3PTR                         pvZeroPgR3;
    /** The ring-0 mapping of the zero page. */
    RTR0PTR                         pvZeroPgR0;
    /** The GC mapping of the zero page. */
    RTGCPTR                         pvZeroPgRC;
#if GC_ARCH_BITS != 32
    uint32_t                        u32ZeroAlignment; /**< Alignment padding. */
#endif
    /** @}*/

    /** The number of handy pages. */
    uint32_t                        cHandyPages;
    /**
     * Array of handy pages.
     *
     * This array is used in a two way communication between pgmPhysAllocPage
     * and GMMR0AllocateHandyPages, with PGMR3PhysAllocateHandyPages serving as
     * an intermediary.
     *
     * The size of this array is important, see pgmPhysEnsureHandyPage for details.
     * (The current size of 32 pages, means 128 KB of handy memory.)
     */
    GMMPAGEDESC                     aHandyPages[PGM_HANDY_PAGES];

    /** @name   Error injection.
     * @{ */
    /** Inject handy page allocation errors pretending we're completely out of
     * memory. */
    bool volatile                   fErrInjHandyPages;
    /** Padding. */
    bool                            afReserved[7];
    /** @} */

    /** @name Release Statistics
     * @{ */
    uint32_t                        cAllPages;          /**< The total number of pages. (Should be Private + Shared + Zero.) */
    uint32_t                        cPrivatePages;      /**< The number of private pages. */
    uint32_t                        cSharedPages;       /**< The number of shared pages. */
    uint32_t                        cZeroPages;         /**< The number of zero backed pages. */

    /** The number of times we were forced to change the hypervisor region location. */
    STAMCOUNTER                     cRelocations;
    /** @} */

#ifdef VBOX_WITH_STATISTICS /** @todo move this chunk to the heap.  */
    /* R3 only: */
    STAMCOUNTER StatR3DetectedConflicts;            /**< R3: Number of times PGMR3MapHasConflicts() detected a conflict. */
    STAMPROFILE StatR3ResolveConflict;              /**< R3: pgmR3SyncPTResolveConflict() profiling (includes the entire relocation). */

    STAMCOUNTER StatRZChunkR3MapTlbHits;            /**< RC/R0: Ring-3/0 chunk mapper TLB hits. */
    STAMCOUNTER StatRZChunkR3MapTlbMisses;          /**< RC/R0: Ring-3/0 chunk mapper TLB misses. */
    STAMCOUNTER StatRZPageMapTlbHits;               /**< RC/R0: Ring-3/0 page mapper TLB hits. */
    STAMCOUNTER StatRZPageMapTlbMisses;             /**< RC/R0: Ring-3/0 page mapper TLB misses. */
    STAMCOUNTER StatR3ChunkR3MapTlbHits;            /**< R3: Ring-3/0 chunk mapper TLB hits. */
    STAMCOUNTER StatR3ChunkR3MapTlbMisses;          /**< R3: Ring-3/0 chunk mapper TLB misses. */
    STAMCOUNTER StatR3PageMapTlbHits;               /**< R3: Ring-3/0 page mapper TLB hits. */
    STAMCOUNTER StatR3PageMapTlbMisses;             /**< R3: Ring-3/0 page mapper TLB misses. */
    STAMPROFILE StatRZSyncCR3HandlerVirtualReset;   /**< RC/R0: Profiling of the virtual handler resets. */
    STAMPROFILE StatRZSyncCR3HandlerVirtualUpdate;  /**< RC/R0: Profiling of the virtual handler updates. */
    STAMPROFILE StatR3SyncCR3HandlerVirtualReset;   /**< R3: Profiling of the virtual handler resets. */
    STAMPROFILE StatR3SyncCR3HandlerVirtualUpdate;  /**< R3: Profiling of the virtual handler updates. */
    STAMCOUNTER StatR3PhysHandlerReset;             /**< R3: The number of times PGMHandlerPhysicalReset is called. */
    STAMCOUNTER StatRZPhysHandlerReset;             /**< RC/R0: The number of times PGMHandlerPhysicalReset is called. */
    STAMPROFILE StatRZVirtHandlerSearchByPhys;      /**< RC/R0: Profiling of pgmHandlerVirtualFindByPhysAddr. */
    STAMPROFILE StatR3VirtHandlerSearchByPhys;      /**< R3: Profiling of pgmHandlerVirtualFindByPhysAddr. */
    STAMCOUNTER StatRZPageReplaceShared;            /**< RC/R0: Times a shared page has been replaced by a private one. */
    STAMCOUNTER StatRZPageReplaceZero;              /**< RC/R0: Times the zero page has been replaced by a private one. */
/// @todo    STAMCOUNTER StatRZPageHandyAllocs;              /**< RC/R0: The number of times we've executed GMMR3AllocateHandyPages. */
    STAMCOUNTER StatR3PageReplaceShared;            /**< R3: Times a shared page has been replaced by a private one. */
    STAMCOUNTER StatR3PageReplaceZero;              /**< R3: Times the zero page has been replaced by a private one. */
/// @todo    STAMCOUNTER StatR3PageHandyAllocs;              /**< R3: The number of times we've executed GMMR3AllocateHandyPages. */

    /* RC only: */
    STAMCOUNTER StatRCDynMapCacheMisses;            /**< RC: The number of dynamic page mapping cache hits */
    STAMCOUNTER StatRCDynMapCacheHits;              /**< RC: The number of dynamic page mapping cache misses */
    STAMCOUNTER StatRCInvlPgConflict;               /**< RC: Number of times PGMInvalidatePage() detected a mapping conflict. */
    STAMCOUNTER StatRCInvlPgSyncMonCR3;             /**< RC: Number of times PGMInvalidatePage() ran into PGM_SYNC_MONITOR_CR3. */

# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    STAMCOUNTER StatTrackVirgin;                    /**< The number of first time shadowings. */
    STAMCOUNTER StatTrackAliased;                   /**< The number of times switching to cRef2, i.e. the page is being shadowed by two PTs. */
    STAMCOUNTER StatTrackAliasedMany;               /**< The number of times we're tracking using cRef2. */
    STAMCOUNTER StatTrackAliasedLots;               /**< The number of times we're hitting pages which has overflowed cRef2. */
    STAMCOUNTER StatTrackOverflows;                 /**< The number of times the extent list grows to long. */
    STAMPROFILE StatTrackDeref;                     /**< Profiling of SyncPageWorkerTrackDeref (expensive). */
# endif
#endif
} PGM;
/** Pointer to the PGM instance data. */
typedef PGM *PPGM;


/**
 * Converts a PGMCPU pointer into a VM pointer.
 * @returns Pointer to the VM structure the PGM is part of.
 * @param   pPGM   Pointer to PGMCPU instance data.
 */
#define PGMCPU2VM(pPGM)  ( (PVM)((char*)pPGM - pPGM->offVM) )

/**
 * Converts a PGMCPU pointer into a PGM pointer.
 * @returns Pointer to the VM structure the PGM is part of.
 * @param   pPGM   Pointer to PGMCPU instance data.
 */
#define PGMCPU2PGM(pPGMCpu)  ( (PPGM)((char*)pPGMCpu - pPGMCpu->offPGM) )

/**
 * PGMCPU Data (part of VMCPU).
 */
typedef struct PGMCPU
{
    /** Offset to the VM structure. */
    RTINT                           offVM;
    /** Offset to the VMCPU structure. */
    RTINT                           offVCpu;
    /** Offset of the PGM structure relative to VMCPU. */
    RTINT                           offPGM;
    RTINT                           uPadding0;      /**< structure size alignment. */

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    /** Automatically tracked physical memory mapping set.
     * Ring-0 and strict raw-mode builds. */
    PGMMAPSET                       AutoSet;
#endif

    /** A20 gate mask.
     * Our current approach to A20 emulation is to let REM do it and don't bother
     * anywhere else. The interesting Guests will be operating with it enabled anyway.
     * But whould need arrise, we'll subject physical addresses to this mask. */
    RTGCPHYS                        GCPhysA20Mask;
    /** A20 gate state - boolean! */
    bool                            fA20Enabled;

    /** What needs syncing (PGM_SYNC_*).
     * This is used to queue operations for PGMSyncCR3, PGMInvalidatePage,
     * PGMFlushTLB, and PGMR3Load. */
    RTUINT                          fSyncFlags;

    /** The shadow paging mode. */
    PGMMODE                         enmShadowMode;
    /** The guest paging mode. */
    PGMMODE                         enmGuestMode;

    /** The current physical address representing in the guest CR3 register. */
    RTGCPHYS                        GCPhysCR3;

    /** @name 32-bit Guest Paging.
     * @{ */
    /** The guest's page directory, R3 pointer. */
    R3PTRTYPE(PX86PD)               pGst32BitPdR3;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    /** The guest's page directory, R0 pointer. */
    R0PTRTYPE(PX86PD)               pGst32BitPdR0;
#endif
    /** The guest's page directory, static RC mapping. */
    RCPTRTYPE(PX86PD)               pGst32BitPdRC;
    /** @} */

    /** @name PAE Guest Paging.
     * @{ */
    /** The guest's page directory pointer table, static RC mapping. */
    RCPTRTYPE(PX86PDPT)             pGstPaePdptRC;
    /** The guest's page directory pointer table, R3 pointer. */
    R3PTRTYPE(PX86PDPT)             pGstPaePdptR3;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    /** The guest's page directory pointer table, R0 pointer. */
    R0PTRTYPE(PX86PDPT)             pGstPaePdptR0;
#endif

    /** The guest's page directories, R3 pointers.
     * These are individual pointers and don't have to be adjecent.
     * These don't have to be up-to-date - use pgmGstGetPaePD() to access them. */
    R3PTRTYPE(PX86PDPAE)            apGstPaePDsR3[4];
    /** The guest's page directories, R0 pointers.
     * Same restrictions as apGstPaePDsR3. */
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    R0PTRTYPE(PX86PDPAE)            apGstPaePDsR0[4];
#endif
    /** The guest's page directories, static GC mapping.
     * Unlike the R3/R0 array the first entry can be accessed as a 2048 entry PD.
     * These don't have to be up-to-date - use pgmGstGetPaePD() to access them. */
    RCPTRTYPE(PX86PDPAE)            apGstPaePDsRC[4];
    /** The physical addresses of the guest page directories (PAE) pointed to by apGstPagePDsHC/GC. */
    RTGCPHYS                        aGCPhysGstPaePDs[4];
    /** The physical addresses of the monitored guest page directories (PAE). */
    RTGCPHYS                        aGCPhysGstPaePDsMonitored[4];
    /** @} */

    /** @name AMD64 Guest Paging.
     * @{ */
    /** The guest's page directory pointer table, R3 pointer. */
    R3PTRTYPE(PX86PML4)             pGstAmd64Pml4R3;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    /** The guest's page directory pointer table, R0 pointer. */
    R0PTRTYPE(PX86PML4)             pGstAmd64Pml4R0;
#endif
    /** @} */

    /** Pointer to the page of the current active CR3 - R3 Ptr. */
    R3PTRTYPE(PPGMPOOLPAGE)         pShwPageCR3R3;
    /** Pointer to the page of the current active CR3 - R0 Ptr. */
    R0PTRTYPE(PPGMPOOLPAGE)         pShwPageCR3R0;
    /** Pointer to the page of the current active CR3 - RC Ptr. */
    RCPTRTYPE(PPGMPOOLPAGE)         pShwPageCR3RC;
    /* The shadow page pool index of the user table as specified during allocation; useful for freeing root pages */
    uint32_t                        iShwUser;
    /* The index into the user table (shadowed) as specified during allocation; useful for freeing root pages. */
    uint32_t                        iShwUserTable;
# if HC_ARCH_BITS == 64
    RTRCPTR                         alignment6; /**< structure size alignment. */
# endif
    /** @} */

    /** @name Function pointers for Shadow paging.
     * @{
     */
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwRelocate,(PVM pVM, PVMCPU pVCpu, RTGCPTR offDelta));
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwExit,(PVM pVM, PVMCPU pVCpu));
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys));
    DECLR3CALLBACKMEMBER(int,       pfnR3ShwModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));

    DECLRCCALLBACKMEMBER(int,       pfnRCShwGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys));
    DECLRCCALLBACKMEMBER(int,       pfnRCShwModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));

    DECLR0CALLBACKMEMBER(int,       pfnR0ShwGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys));
    DECLR0CALLBACKMEMBER(int,       pfnR0ShwModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));

    /** @} */

    /** @name Function pointers for Guest paging.
     * @{
     */
    DECLR3CALLBACKMEMBER(int,       pfnR3GstRelocate,(PVM pVM, PVMCPU pVCpu, RTGCPTR offDelta));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstExit,(PVM pVM, PVMCPU pVCpu));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    DECLR3CALLBACKMEMBER(int,       pfnR3GstGetPDE,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPde));
    DECLRCCALLBACKMEMBER(int,       pfnRCGstGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys));
    DECLRCCALLBACKMEMBER(int,       pfnRCGstModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    DECLRCCALLBACKMEMBER(int,       pfnRCGstGetPDE,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPde));
#if HC_ARCH_BITS == 64
    RTRCPTR                         alignment3; /**< structure size alignment. */
#endif

    DECLR0CALLBACKMEMBER(int,       pfnR0GstGetPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys));
    DECLR0CALLBACKMEMBER(int,       pfnR0GstModifyPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask));
    DECLR0CALLBACKMEMBER(int,       pfnR0GstGetPDE,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDEPAE pPde));
    /** @} */

    /** @name Function pointers for Both Shadow and Guest paging.
     * @{
     */
    DECLR3CALLBACKMEMBER(int,       pfnR3BthRelocate,(PVM pVM, PVMCPU pVCpu, RTGCPTR offDelta));
    /*                           no pfnR3BthTrap0eHandler */
    DECLR3CALLBACKMEMBER(int,       pfnR3BthInvalidatePage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthSyncCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthSyncPage,(PVM pVM, PVMCPU pVCpu, X86PDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uError));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthPrefetchPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthVerifyAccessSyncPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fFlags, unsigned uError));
    DECLR3CALLBACKMEMBER(unsigned,  pfnR3BthAssertCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthMapCR3,(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysCR3));
    DECLR3CALLBACKMEMBER(int,       pfnR3BthUnmapCR3,(PVM pVM, PVMCPU pVCpu));

    DECLR0CALLBACKMEMBER(int,       pfnR0BthTrap0eHandler,(PVM pVM, PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthInvalidatePage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthSyncCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthSyncPage,(PVM pVM, PVMCPU pVCpu, X86PDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uError));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthPrefetchPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthVerifyAccessSyncPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fFlags, unsigned uError));
    DECLR0CALLBACKMEMBER(unsigned,  pfnR0BthAssertCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthMapCR3,(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysCR3));
    DECLR0CALLBACKMEMBER(int,       pfnR0BthUnmapCR3,(PVM pVM, PVMCPU pVCpu));

    DECLRCCALLBACKMEMBER(int,       pfnRCBthTrap0eHandler,(PVM pVM, PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthInvalidatePage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthSyncCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthSyncPage,(PVM pVM, PVMCPU pVCpu, X86PDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uError));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthPrefetchPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthVerifyAccessSyncPage,(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fFlags, unsigned uError));
    DECLRCCALLBACKMEMBER(unsigned,  pfnRCBthAssertCR3,(PVM pVM, PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthMapCR3,(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhysCR3));
    DECLRCCALLBACKMEMBER(int,       pfnRCBthUnmapCR3,(PVM pVM, PVMCPU pVCpu));
#if HC_ARCH_BITS == 64
    RTRCPTR                         alignment2; /**< structure size alignment. */
#endif
    /** @} */

    /** @name Release Statistics
     * @{ */
    /** The number of times the guest has switched mode since last reset or statistics reset. */
    STAMCOUNTER                     cGuestModeChanges;
    /** @} */

#ifdef VBOX_WITH_STATISTICS /** @todo move this chunk to the heap.  */
    /** @name Statistics
     * @{ */
    /** RC: Which statistic this \#PF should be attributed to. */
    RCPTRTYPE(PSTAMPROFILE)         pStatTrap0eAttributionRC;
    RTRCPTR                         padding0;
    /** R0: Which statistic this \#PF should be attributed to. */
    R0PTRTYPE(PSTAMPROFILE)         pStatTrap0eAttributionR0;
    RTR0PTR                         padding1;

    /* Common */
    STAMCOUNTER StatSyncPtPD[X86_PG_ENTRIES];       /**< SyncPT - PD distribution. */
    STAMCOUNTER StatSyncPagePD[X86_PG_ENTRIES];     /**< SyncPage - PD distribution. */

    /* R0 only: */
    STAMCOUNTER StatR0DynMapMigrateInvlPg;          /**< R0: invlpg in PGMDynMapMigrateAutoSet. */
    STAMPROFILE StatR0DynMapGCPageInl;              /**< R0: Calls to pgmR0DynMapGCPageInlined. */
    STAMCOUNTER StatR0DynMapGCPageInlHits;          /**< R0: Hash table lookup hits. */
    STAMCOUNTER StatR0DynMapGCPageInlMisses;        /**< R0: Misses that falls back to code common with PGMDynMapHCPage. */
    STAMCOUNTER StatR0DynMapGCPageInlRamHits;       /**< R0: 1st ram range hits. */
    STAMCOUNTER StatR0DynMapGCPageInlRamMisses;     /**< R0: 1st ram range misses, takes slow path. */
    STAMPROFILE StatR0DynMapHCPageInl;              /**< R0: Calls to pgmR0DynMapHCPageInlined. */
    STAMCOUNTER StatR0DynMapHCPageInlHits;          /**< R0: Hash table lookup hits. */
    STAMCOUNTER StatR0DynMapHCPageInlMisses;        /**< R0: Misses that falls back to code common with PGMDynMapHCPage. */
    STAMPROFILE StatR0DynMapHCPage;                 /**< R0: Calls to PGMDynMapHCPage. */
    STAMCOUNTER StatR0DynMapSetOptimize;            /**< R0: Calls to pgmDynMapOptimizeAutoSet. */
    STAMCOUNTER StatR0DynMapSetSearchFlushes;       /**< R0: Set search restorting to subset flushes. */
    STAMCOUNTER StatR0DynMapSetSearchHits;          /**< R0: Set search hits. */
    STAMCOUNTER StatR0DynMapSetSearchMisses;        /**< R0: Set search misses. */
    STAMCOUNTER StatR0DynMapPage;                   /**< R0: Calls to pgmR0DynMapPage. */
    STAMCOUNTER StatR0DynMapPageHits0;              /**< R0: Hits at iPage+0. */
    STAMCOUNTER StatR0DynMapPageHits1;              /**< R0: Hits at iPage+1. */
    STAMCOUNTER StatR0DynMapPageHits2;              /**< R0: Hits at iPage+2. */
    STAMCOUNTER StatR0DynMapPageInvlPg;             /**< R0: invlpg. */
    STAMCOUNTER StatR0DynMapPageSlow;               /**< R0: Calls to pgmR0DynMapPageSlow. */
    STAMCOUNTER StatR0DynMapPageSlowLoopHits;       /**< R0: Hits in the pgmR0DynMapPageSlow search loop. */
    STAMCOUNTER StatR0DynMapPageSlowLoopMisses;     /**< R0: Misses in the pgmR0DynMapPageSlow search loop. */
    //STAMCOUNTER StatR0DynMapPageSlowLostHits;       /**< R0: Lost hits. */
    STAMCOUNTER StatR0DynMapSubsets;                /**< R0: Times PGMDynMapPushAutoSubset was called. */
    STAMCOUNTER StatR0DynMapPopFlushes;             /**< R0: Times PGMDynMapPopAutoSubset flushes the subset. */
    STAMCOUNTER aStatR0DynMapSetSize[11];           /**< R0: Set size distribution. */

    /* RZ only: */
    STAMPROFILE StatRZTrap0e;                       /**< RC/R0: PGMTrap0eHandler() profiling. */
    STAMPROFILE StatRZTrap0eTimeCheckPageFault;
    STAMPROFILE StatRZTrap0eTimeSyncPT;
    STAMPROFILE StatRZTrap0eTimeMapping;
    STAMPROFILE StatRZTrap0eTimeOutOfSync;
    STAMPROFILE StatRZTrap0eTimeHandlers;
    STAMPROFILE StatRZTrap0eTime2CSAM;              /**< RC/R0: Profiling of the Trap0eHandler body when the cause is CSAM. */
    STAMPROFILE StatRZTrap0eTime2DirtyAndAccessed;  /**< RC/R0: Profiling of the Trap0eHandler body when the cause is dirty and/or accessed bit emulation. */
    STAMPROFILE StatRZTrap0eTime2GuestTrap;         /**< RC/R0: Profiling of the Trap0eHandler body when the cause is a guest trap. */
    STAMPROFILE StatRZTrap0eTime2HndPhys;           /**< RC/R0: Profiling of the Trap0eHandler body when the cause is a physical handler. */
    STAMPROFILE StatRZTrap0eTime2HndVirt;           /**< RC/R0: Profiling of the Trap0eHandler body when the cause is a virtual handler. */
    STAMPROFILE StatRZTrap0eTime2HndUnhandled;      /**< RC/R0: Profiling of the Trap0eHandler body when the cause is access outside the monitored areas of a monitored page. */
    STAMPROFILE StatRZTrap0eTime2Misc;              /**< RC/R0: Profiling of the Trap0eHandler body when the cause is not known. */
    STAMPROFILE StatRZTrap0eTime2OutOfSync;         /**< RC/R0: Profiling of the Trap0eHandler body when the cause is an out-of-sync page. */
    STAMPROFILE StatRZTrap0eTime2OutOfSyncHndPhys;  /**< RC/R0: Profiling of the Trap0eHandler body when the cause is an out-of-sync physical handler page. */
    STAMPROFILE StatRZTrap0eTime2OutOfSyncHndVirt;  /**< RC/R0: Profiling of the Trap0eHandler body when the cause is an out-of-sync virtual handler page. */
    STAMPROFILE StatRZTrap0eTime2OutOfSyncHndObs;   /**< RC/R0: Profiling of the Trap0eHandler body when the cause is an obsolete handler page. */
    STAMPROFILE StatRZTrap0eTime2SyncPT;            /**< RC/R0: Profiling of the Trap0eHandler body when the cause is lazy syncing of a PT. */
    STAMCOUNTER StatRZTrap0eConflicts;              /**< RC/R0: The number of times \#PF was caused by an undetected conflict. */
    STAMCOUNTER StatRZTrap0eHandlersMapping;        /**< RC/R0: Number of traps due to access handlers in mappings. */
    STAMCOUNTER StatRZTrap0eHandlersOutOfSync;      /**< RC/R0: Number of out-of-sync handled pages. */
    STAMCOUNTER StatRZTrap0eHandlersPhysical;       /**< RC/R0: Number of traps due to physical access handlers. */
    STAMCOUNTER StatRZTrap0eHandlersVirtual;        /**< RC/R0: Number of traps due to virtual access handlers. */
    STAMCOUNTER StatRZTrap0eHandlersVirtualByPhys;  /**< RC/R0: Number of traps due to virtual access handlers found by physical address. */
    STAMCOUNTER StatRZTrap0eHandlersVirtualUnmarked;/**< RC/R0: Number of traps due to virtual access handlers found by virtual address (without proper physical flags). */
    STAMCOUNTER StatRZTrap0eHandlersUnhandled;      /**< RC/R0: Number of traps due to access outside range of monitored page(s). */
    STAMCOUNTER StatRZTrap0eHandlersInvalid;        /**< RC/R0: Number of traps due to access to invalid physical memory. */
    STAMCOUNTER StatRZTrap0eUSNotPresentRead;       /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eUSNotPresentWrite;      /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eUSWrite;                /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eUSReserved;             /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eUSNXE;                  /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eUSRead;                 /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eSVNotPresentRead;       /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eSVNotPresentWrite;      /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eSVWrite;                /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eSVReserved;             /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eSNXE;                   /**< RC/R0: #PF err kind */
    STAMCOUNTER StatRZTrap0eGuestPF;                /**< RC/R0: Real guest #PFs. */
    STAMCOUNTER StatRZTrap0eGuestPFUnh;             /**< RC/R0: Real guest #PF ending up at the end of the #PF code. */
    STAMCOUNTER StatRZTrap0eGuestPFMapping;         /**< RC/R0: Real guest #PF to HMA or other mapping. */
    STAMCOUNTER StatRZTrap0eWPEmulInRZ;             /**< RC/R0: WP=0 virtualization trap, handled. */
    STAMCOUNTER StatRZTrap0eWPEmulToR3;             /**< RC/R0: WP=0 virtualization trap, chickened out. */
    STAMCOUNTER StatRZTrap0ePD[X86_PG_ENTRIES];     /**< RC/R0: PD distribution of the #PFs. */
    STAMCOUNTER StatRZGuestCR3WriteHandled;         /**< RC/R0: The number of times WriteHandlerCR3() was successfully called. */
    STAMCOUNTER StatRZGuestCR3WriteUnhandled;       /**< RC/R0: The number of times WriteHandlerCR3() was called and we had to fall back to the recompiler. */
    STAMCOUNTER StatRZGuestCR3WriteConflict;        /**< RC/R0: The number of times WriteHandlerCR3() was called and a conflict was detected. */
    STAMCOUNTER StatRZGuestROMWriteHandled;         /**< RC/R0: The number of times pgmPhysRomWriteHandler() was successfully called. */
    STAMCOUNTER StatRZGuestROMWriteUnhandled;       /**< RC/R0: The number of times pgmPhysRomWriteHandler() was called and we had to fall back to the recompiler */

    /* HC - R3 and (maybe) R0: */

    /* RZ & R3: */
    STAMPROFILE StatRZSyncCR3;                      /**< RC/R0: PGMSyncCR3() profiling. */
    STAMPROFILE StatRZSyncCR3Handlers;              /**< RC/R0: Profiling of the PGMSyncCR3() update handler section. */
    STAMCOUNTER StatRZSyncCR3Global;                /**< RC/R0: The number of global CR3 syncs. */
    STAMCOUNTER StatRZSyncCR3NotGlobal;             /**< RC/R0: The number of non-global CR3 syncs. */
    STAMCOUNTER StatRZSyncCR3DstCacheHit;           /**< RC/R0: The number of times we got some kind of cache hit on a page table. */
    STAMCOUNTER StatRZSyncCR3DstFreed;              /**< RC/R0: The number of times we've had to free a shadow entry. */
    STAMCOUNTER StatRZSyncCR3DstFreedSrcNP;         /**< RC/R0: The number of times we've had to free a shadow entry for which the source entry was not present. */
    STAMCOUNTER StatRZSyncCR3DstNotPresent;         /**< RC/R0: The number of times we've encountered a not present shadow entry for a present guest entry. */
    STAMCOUNTER StatRZSyncCR3DstSkippedGlobalPD;    /**< RC/R0: The number of times a global page directory wasn't flushed. */
    STAMCOUNTER StatRZSyncCR3DstSkippedGlobalPT;    /**< RC/R0: The number of times a page table with only global entries wasn't flushed. */
    STAMPROFILE StatRZSyncPT;                       /**< RC/R0: PGMSyncPT() profiling. */
    STAMCOUNTER StatRZSyncPTFailed;                 /**< RC/R0: The number of times PGMSyncPT() failed. */
    STAMCOUNTER StatRZSyncPT4K;                     /**< RC/R0: Number of 4KB syncs. */
    STAMCOUNTER StatRZSyncPT4M;                     /**< RC/R0: Number of 4MB syncs. */
    STAMCOUNTER StatRZSyncPagePDNAs;                /**< RC/R0: The number of time we've marked a PD not present from SyncPage to virtualize the accessed bit. */
    STAMCOUNTER StatRZSyncPagePDOutOfSync;          /**< RC/R0: The number of time we've encountered an out-of-sync PD in SyncPage. */
    STAMCOUNTER StatRZAccessedPage;                 /**< RC/R0: The number of pages marked not present for accessed bit emulation. */
    STAMPROFILE StatRZDirtyBitTracking;             /**< RC/R0: Profiling the dirty bit tracking in CheckPageFault().. */
    STAMCOUNTER StatRZDirtyPage;                    /**< RC/R0: The number of pages marked read-only for dirty bit tracking. */
    STAMCOUNTER StatRZDirtyPageBig;                 /**< RC/R0: The number of pages marked read-only for dirty bit tracking. */
    STAMCOUNTER StatRZDirtyPageSkipped;             /**< RC/R0: The number of pages already dirty or readonly. */
    STAMCOUNTER StatRZDirtyPageTrap;                /**< RC/R0: The number of traps generated for dirty bit tracking. */
    STAMCOUNTER StatRZDirtyTrackRealPF;             /**< RC/R0: The number of real pages faults during dirty bit tracking. */
    STAMCOUNTER StatRZDirtiedPage;                  /**< RC/R0: The number of pages marked dirty because of write accesses. */
    STAMCOUNTER StatRZPageAlreadyDirty;             /**< RC/R0: The number of pages already marked dirty because of write accesses. */
    STAMPROFILE StatRZInvalidatePage;               /**< RC/R0: PGMInvalidatePage() profiling. */
    STAMCOUNTER StatRZInvalidatePage4KBPages;       /**< RC/R0: The number of times PGMInvalidatePage() was called for a 4KB page. */
    STAMCOUNTER StatRZInvalidatePage4MBPages;       /**< RC/R0: The number of times PGMInvalidatePage() was called for a 4MB page. */
    STAMCOUNTER StatRZInvalidatePage4MBPagesSkip;   /**< RC/R0: The number of times PGMInvalidatePage() skipped a 4MB page. */
    STAMCOUNTER StatRZInvalidatePagePDMappings;     /**< RC/R0: The number of times PGMInvalidatePage() was called for a page directory containing mappings (no conflict). */
    STAMCOUNTER StatRZInvalidatePagePDNAs;          /**< RC/R0: The number of times PGMInvalidatePage() was called for a not accessed page directory. */
    STAMCOUNTER StatRZInvalidatePagePDNPs;          /**< RC/R0: The number of times PGMInvalidatePage() was called for a not present page directory. */
    STAMCOUNTER StatRZInvalidatePagePDOutOfSync;    /**< RC/R0: The number of times PGMInvalidatePage() was called for an out of sync page directory. */
    STAMCOUNTER StatRZInvalidatePageSkipped;        /**< RC/R0: The number of times PGMInvalidatePage() was skipped due to not present shw or pending pending SyncCR3. */
    STAMCOUNTER StatRZPageOutOfSyncUser;            /**< RC/R0: The number of times user page is out of sync was detected in #PF or VerifyAccessSyncPage. */
    STAMCOUNTER StatRZPageOutOfSyncSupervisor;      /**< RC/R0: The number of times supervisor page is out of sync was detected in in #PF or VerifyAccessSyncPage. */
    STAMPROFILE StatRZPrefetch;                     /**< RC/R0: PGMPrefetchPage. */
    STAMPROFILE StatRZFlushTLB;                     /**< RC/R0: Profiling of the PGMFlushTLB() body. */
    STAMCOUNTER StatRZFlushTLBNewCR3;               /**< RC/R0: The number of times PGMFlushTLB was called with a new CR3, non-global. (switch) */
    STAMCOUNTER StatRZFlushTLBNewCR3Global;         /**< RC/R0: The number of times PGMFlushTLB was called with a new CR3, global. (switch) */
    STAMCOUNTER StatRZFlushTLBSameCR3;              /**< RC/R0: The number of times PGMFlushTLB was called with the same CR3, non-global. (flush) */
    STAMCOUNTER StatRZFlushTLBSameCR3Global;        /**< RC/R0: The number of times PGMFlushTLB was called with the same CR3, global. (flush) */
    STAMPROFILE StatRZGstModifyPage;                /**< RC/R0: Profiling of the PGMGstModifyPage() body */

    STAMPROFILE StatR3SyncCR3;                      /**< R3: PGMSyncCR3() profiling. */
    STAMPROFILE StatR3SyncCR3Handlers;              /**< R3: Profiling of the PGMSyncCR3() update handler section. */
    STAMCOUNTER StatR3SyncCR3Global;                /**< R3: The number of global CR3 syncs. */
    STAMCOUNTER StatR3SyncCR3NotGlobal;             /**< R3: The number of non-global CR3 syncs. */
    STAMCOUNTER StatR3SyncCR3DstFreed;              /**< R3: The number of times we've had to free a shadow entry. */
    STAMCOUNTER StatR3SyncCR3DstFreedSrcNP;         /**< R3: The number of times we've had to free a shadow entry for which the source entry was not present. */
    STAMCOUNTER StatR3SyncCR3DstNotPresent;         /**< R3: The number of times we've encountered a not present shadow entry for a present guest entry. */
    STAMCOUNTER StatR3SyncCR3DstSkippedGlobalPD;    /**< R3: The number of times a global page directory wasn't flushed. */
    STAMCOUNTER StatR3SyncCR3DstSkippedGlobalPT;    /**< R3: The number of times a page table with only global entries wasn't flushed. */
    STAMCOUNTER StatR3SyncCR3DstCacheHit;           /**< R3: The number of times we got some kind of cache hit on a page table. */
    STAMPROFILE StatR3SyncPT;                       /**< R3: PGMSyncPT() profiling. */
    STAMCOUNTER StatR3SyncPTFailed;                 /**< R3: The number of times PGMSyncPT() failed. */
    STAMCOUNTER StatR3SyncPT4K;                     /**< R3: Number of 4KB syncs. */
    STAMCOUNTER StatR3SyncPT4M;                     /**< R3: Number of 4MB syncs. */
    STAMCOUNTER StatR3SyncPagePDNAs;                /**< R3: The number of time we've marked a PD not present from SyncPage to virtualize the accessed bit. */
    STAMCOUNTER StatR3SyncPagePDOutOfSync;          /**< R3: The number of time we've encountered an out-of-sync PD in SyncPage. */
    STAMCOUNTER StatR3AccessedPage;                 /**< R3: The number of pages marked not present for accessed bit emulation. */
    STAMPROFILE StatR3DirtyBitTracking;             /**< R3: Profiling the dirty bit tracking in CheckPageFault(). */
    STAMCOUNTER StatR3DirtyPage;                    /**< R3: The number of pages marked read-only for dirty bit tracking. */
    STAMCOUNTER StatR3DirtyPageBig;                 /**< R3: The number of pages marked read-only for dirty bit tracking. */
    STAMCOUNTER StatR3DirtyPageSkipped;             /**< R3: The number of pages already dirty or readonly. */
    STAMCOUNTER StatR3DirtyPageTrap;                /**< R3: The number of traps generated for dirty bit tracking. */
    STAMCOUNTER StatR3DirtyTrackRealPF;             /**< R3: The number of real pages faults during dirty bit tracking. */
    STAMCOUNTER StatR3DirtiedPage;                  /**< R3: The number of pages marked dirty because of write accesses. */
    STAMCOUNTER StatR3PageAlreadyDirty;             /**< R3: The number of pages already marked dirty because of write accesses. */
    STAMPROFILE StatR3InvalidatePage;               /**< R3: PGMInvalidatePage() profiling. */
    STAMCOUNTER StatR3InvalidatePage4KBPages;       /**< R3: The number of times PGMInvalidatePage() was called for a 4KB page. */
    STAMCOUNTER StatR3InvalidatePage4MBPages;       /**< R3: The number of times PGMInvalidatePage() was called for a 4MB page. */
    STAMCOUNTER StatR3InvalidatePage4MBPagesSkip;   /**< R3: The number of times PGMInvalidatePage() skipped a 4MB page. */
    STAMCOUNTER StatR3InvalidatePagePDNAs;          /**< R3: The number of times PGMInvalidatePage() was called for a not accessed page directory. */
    STAMCOUNTER StatR3InvalidatePagePDNPs;          /**< R3: The number of times PGMInvalidatePage() was called for a not present page directory. */
    STAMCOUNTER StatR3InvalidatePagePDMappings;     /**< R3: The number of times PGMInvalidatePage() was called for a page directory containing mappings (no conflict). */
    STAMCOUNTER StatR3InvalidatePagePDOutOfSync;    /**< R3: The number of times PGMInvalidatePage() was called for an out of sync page directory. */
    STAMCOUNTER StatR3InvalidatePageSkipped;        /**< R3: The number of times PGMInvalidatePage() was skipped due to not present shw or pending pending SyncCR3. */
    STAMCOUNTER StatR3PageOutOfSyncUser;            /**< R3: The number of times user page is out of sync was detected in #PF or VerifyAccessSyncPage. */
    STAMCOUNTER StatR3PageOutOfSyncSupervisor;      /**< R3: The number of times supervisor page is out of sync was detected in in #PF or VerifyAccessSyncPage. */
    STAMPROFILE StatR3Prefetch;                     /**< R3: PGMPrefetchPage. */
    STAMPROFILE StatR3FlushTLB;                     /**< R3: Profiling of the PGMFlushTLB() body. */
    STAMCOUNTER StatR3FlushTLBNewCR3;               /**< R3: The number of times PGMFlushTLB was called with a new CR3, non-global. (switch) */
    STAMCOUNTER StatR3FlushTLBNewCR3Global;         /**< R3: The number of times PGMFlushTLB was called with a new CR3, global. (switch) */
    STAMCOUNTER StatR3FlushTLBSameCR3;              /**< R3: The number of times PGMFlushTLB was called with the same CR3, non-global. (flush) */
    STAMCOUNTER StatR3FlushTLBSameCR3Global;        /**< R3: The number of times PGMFlushTLB was called with the same CR3, global. (flush) */
    STAMPROFILE StatR3GstModifyPage;                /**< R3: Profiling of the PGMGstModifyPage() body */
    /** @} */
#endif /* VBOX_WITH_STATISTICS */
} PGMCPU;
/** Pointer to the per-cpu PGM data. */
typedef PGMCPU *PPGMCPU;


/** @name PGM::fSyncFlags Flags
 * @{
 */
/** Updates the virtual access handler state bit in PGMPAGE. */
#define PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL        RT_BIT(0)
/** Always sync CR3. */
#define PGM_SYNC_ALWAYS                         RT_BIT(1)
/** Check monitoring on next CR3 (re)load and invalidate page.
 * @todo This is obsolete now. Remove after 2.2.0 is branched off. */
#define PGM_SYNC_MONITOR_CR3                    RT_BIT(2)
/** Check guest mapping in SyncCR3. */
#define PGM_SYNC_MAP_CR3                        RT_BIT(3)
/** Clear the page pool (a light weight flush). */
#define PGM_GLOBAL_SYNC_CLEAR_PGM_POOL          RT_BIT(8)
/** @} */


__BEGIN_DECLS

int             pgmLock(PVM pVM);
void            pgmUnlock(PVM pVM);

int             pgmR3SyncPTResolveConflict(PVM pVM, PPGMMAPPING pMapping, PX86PD pPDSrc, RTGCPTR GCPtrOldMapping);
int             pgmR3SyncPTResolveConflictPAE(PVM pVM, PPGMMAPPING pMapping, RTGCPTR GCPtrOldMapping);
PPGMMAPPING     pgmGetMapping(PVM pVM, RTGCPTR GCPtr);
void            pgmR3MapRelocate(PVM pVM, PPGMMAPPING pMapping, RTGCPTR GCPtrOldMapping, RTGCPTR GCPtrNewMapping);
DECLCALLBACK(void) pgmR3MapInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);

void            pgmR3HandlerPhysicalUpdateAll(PVM pVM);
bool            pgmHandlerPhysicalIsAll(PVM pVM, RTGCPHYS GCPhys);
void            pgmHandlerPhysicalResetAliasedPage(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhysPage);
int             pgmHandlerVirtualFindByPhysAddr(PVM pVM, RTGCPHYS GCPhys, PPGMVIRTHANDLER *ppVirt, unsigned *piPage);
DECLCALLBACK(int) pgmHandlerVirtualResetOne(PAVLROGCPTRNODECORE pNode, void *pvUser);
#if defined(VBOX_STRICT) || defined(LOG_ENABLED)
void            pgmHandlerVirtualDumpPhysPages(PVM pVM);
#else
# define pgmHandlerVirtualDumpPhysPages(a) do { } while (0)
#endif
DECLCALLBACK(void) pgmR3InfoHandlers(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


int             pgmPhysAllocPage(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys);
int             pgmPhysPageLoadIntoTlb(PPGM pPGM, RTGCPHYS GCPhys);
int             pgmPhysPageLoadIntoTlbWithPage(PPGM pPGM, PPGMPAGE pPage, RTGCPHYS GCPhys);
int             pgmPhysPageMakeWritable(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys);
int             pgmPhysPageMakeWritableUnlocked(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys);
int             pgmPhysPageMap(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPPGMPAGEMAP ppMap, void **ppv);
int             pgmPhysPageMapByPageID(PVM pVM, uint32_t idPage, RTHCPHYS HCPhys, void **ppv);
int             pgmPhysGCPhys2CCPtrInternal(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void **ppv);
int             pgmPhysGCPhys2CCPtrInternalReadOnly(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, const void **ppv);
VMMDECL(int)    pgmPhysRomWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
#ifdef IN_RING3
void            pgmR3PhysRelinkRamRanges(PVM pVM);
int             pgmR3PhysRamPreAllocate(PVM pVM);
int             pgmR3PhysRamReset(PVM pVM);
int             pgmR3PhysRomReset(PVM pVM);
int             pgmR3PhysChunkMap(PVM pVM, uint32_t idChunk, PPPGMCHUNKR3MAP ppChunk);

int             pgmR3PoolInit(PVM pVM);
void            pgmR3PoolRelocate(PVM pVM);
void            pgmR3PoolReset(PVM pVM);

#endif /* IN_RING3 */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
int             pgmR0DynMapHCPageCommon(PVM pVM, PPGMMAPSET pSet, RTHCPHYS HCPhys, void **ppv);
#endif
int             pgmPoolAlloc(PVM pVM, RTGCPHYS GCPhys, PGMPOOLKIND enmKind, uint16_t iUser, uint32_t iUserTable, PPPGMPOOLPAGE ppPage);
PPGMPOOLPAGE    pgmPoolGetPageByHCPhys(PVM pVM, RTHCPHYS HCPhys);
void            pgmPoolFree(PVM pVM, RTHCPHYS HCPhys, uint16_t iUser, uint32_t iUserTable);
void            pgmPoolFreeByPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage, uint16_t iUser, uint32_t iUserTable);
int             pgmPoolFlushPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage);
void            pgmPoolFlushAll(PVM pVM);
void            pgmPoolClearAll(PVM pVM);
int             pgmPoolSyncCR3(PVM pVM);
int             pgmPoolTrackFlushGCPhys(PVM pVM, PPGMPAGE pPhysPage, bool *pfFlushTLBs);
void            pgmPoolTrackFlushGCPhysPT(PVM pVM, PPGMPAGE pPhysPage, uint16_t iShw, uint16_t cRefs);
void            pgmPoolTrackFlushGCPhysPTs(PVM pVM, PPGMPAGE pPhysPage, uint16_t iPhysExt);
int             pgmPoolTrackFlushGCPhysPTsSlow(PVM pVM, PPGMPAGE pPhysPage);
PPGMPOOLPHYSEXT pgmPoolTrackPhysExtAlloc(PVM pVM, uint16_t *piPhysExt);
void            pgmPoolTrackPhysExtFree(PVM pVM, uint16_t iPhysExt);
void            pgmPoolTrackPhysExtFreeList(PVM pVM, uint16_t iPhysExt);
uint16_t        pgmPoolTrackPhysExtAddref(PVM pVM, uint16_t u16, uint16_t iShwPT);
void            pgmPoolTrackPhysExtDerefGCPhys(PPGMPOOL pPool, PPGMPOOLPAGE pPoolPage, PPGMPAGE pPhysPage);
#ifdef PGMPOOL_WITH_MONITORING
void            pgmPoolMonitorChainChanging(PVMCPU pVCpu, PPGMPOOL pPool, PPGMPOOLPAGE pPage, RTGCPHYS GCPhysFault, CTXTYPE(RTGCPTR, RTHCPTR, RTGCPTR) pvAddress, PDISCPUSTATE pCpu);
int             pgmPoolMonitorChainFlush(PPGMPOOL pPool, PPGMPOOLPAGE pPage);
void            pgmPoolMonitorModifiedInsert(PPGMPOOL pPool, PPGMPOOLPAGE pPage);
void            pgmPoolMonitorModifiedClearAll(PVM pVM);
int             pgmPoolMonitorMonitorCR3(PPGMPOOL pPool, uint16_t idxRoot, RTGCPHYS GCPhysCR3);
int             pgmPoolMonitorUnmonitorCR3(PPGMPOOL pPool, uint16_t idxRoot);
#endif

int             pgmR3ExitShadowModeBeforePoolFlush(PVM pVM);
int             pgmR3ReEnterShadowModeAfterPoolFlush(PVM pVM);

void            pgmMapSetShadowPDEs(PVM pVM, PPGMMAPPING pMap, unsigned iNewPDE);
void            pgmMapClearShadowPDEs(PVM pVM, PPGMPOOLPAGE pShwPageCR3, PPGMMAPPING pMap, unsigned iOldPDE, bool fDeactivateCR3);
int             pgmMapActivateCR3(PVM pVM, PPGMPOOLPAGE pShwPageCR3);
int             pgmMapDeactivateCR3(PVM pVM, PPGMPOOLPAGE pShwPageCR3);

int             pgmShwSyncPaePDPtr(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, PX86PDPE pGstPdpe, PX86PDPAE *ppPD);
#ifndef IN_RC
int             pgmShwSyncLongModePDPtr(PVM pVM, PVMCPU pVCpu, RTGCPTR64 GCPtr, PX86PML4E pGstPml4e, PX86PDPE pGstPdpe, PX86PDPAE *ppPD);
#endif
int             pgmShwGetEPTPDPtr(PVM pVM, PVMCPU pVCpu, RTGCPTR64 GCPtr, PEPTPDPT *ppPdpt, PEPTPD *ppPD);

PX86PD          pgmGstLazyMap32BitPD(PPGMCPU pPGM);
PX86PDPT        pgmGstLazyMapPaePDPT(PPGMCPU pPGM);
PX86PDPAE       pgmGstLazyMapPaePD(PPGMCPU pPGM, uint32_t iPdpt);
PX86PML4        pgmGstLazyMapPml4(PPGMCPU pPGM);

__END_DECLS


/**
 * Gets the PGMRAMRANGE structure for a guest page.
 *
 * @returns Pointer to the RAM range on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 */
DECLINLINE(PPGMRAMRANGE) pgmPhysGetRange(PPGM pPGM, RTGCPHYS GCPhys)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
                break;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    return pRam;
}


/**
 * Gets the PGMPAGE structure for a guest page.
 *
 * @returns Pointer to the page on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 */
DECLINLINE(PPGMPAGE) pgmPhysGetPage(PPGM pPGM, RTGCPHYS GCPhys)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
                return NULL;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    return &pRam->aPages[off >> PAGE_SHIFT];
}


/**
 * Gets the PGMPAGE structure for a guest page.
 *
 * Old Phys code: Will make sure the page is present.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS and a valid *ppPage on success.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if the address isn't valid.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppPage      Where to store the page poitner on success.
 */
DECLINLINE(int) pgmPhysGetPageEx(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGE ppPage)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
            {
                *ppPage = NULL; /* avoid incorrect and very annoying GCC warnings */
                return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
            }
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
    return VINF_SUCCESS;
}




/**
 * Gets the PGMPAGE structure for a guest page.
 *
 * Old Phys code: Will make sure the page is present.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS and a valid *ppPage on success.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if the address isn't valid.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppPage      Where to store the page poitner on success.
 * @param   ppRamHint   Where to read and store the ram list hint.
 *                      The caller initializes this to NULL before the call.
 */
DECLINLINE(int) pgmPhysGetPageWithHintEx(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRamHint)
{
    RTGCPHYS off;
    PPGMRAMRANGE pRam = *ppRamHint;
    if (    !pRam
        ||  RT_UNLIKELY((off = GCPhys - pRam->GCPhys) >= pRam->cb))
    {
        pRam = pPGM->CTX_SUFF(pRamRanges);
        off = GCPhys - pRam->GCPhys;
        if (RT_UNLIKELY(off >= pRam->cb))
        {
            do
            {
                pRam = pRam->CTX_SUFF(pNext);
                if (RT_UNLIKELY(!pRam))
                {
                    *ppPage = NULL; /* Kill the incorrect and extremely annoying GCC warnings. */
                    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
                }
                off = GCPhys - pRam->GCPhys;
            } while (off >= pRam->cb);
        }
        *ppRamHint = pRam;
    }
    *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
    return VINF_SUCCESS;
}


/**
 * Gets the PGMPAGE structure for a guest page together with the PGMRAMRANGE.
 *
 * @returns Pointer to the page on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppRam       Where to store the pointer to the PGMRAMRANGE.
 */
DECLINLINE(PPGMPAGE) pgmPhysGetPageAndRange(PPGM pPGM, RTGCPHYS GCPhys, PPGMRAMRANGE *ppRam)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
                return NULL;
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    *ppRam = pRam;
    return &pRam->aPages[off >> PAGE_SHIFT];
}


/**
 * Gets the PGMPAGE structure for a guest page together with the PGMRAMRANGE.
 *
 * @returns Pointer to the page on success.
 * @returns NULL on a VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS condition.
 *
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   ppPage      Where to store the pointer to the PGMPAGE structure.
 * @param   ppRam       Where to store the pointer to the PGMRAMRANGE structure.
 */
DECLINLINE(int) pgmPhysGetPageAndRangeEx(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam)
{
    /*
     * Optimize for the first range.
     */
    PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb))
    {
        do
        {
            pRam = pRam->CTX_SUFF(pNext);
            if (RT_UNLIKELY(!pRam))
            {
                *ppRam = NULL;  /* Shut up silly GCC warnings. */
                *ppPage = NULL; /* ditto */
                return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
            }
            off = GCPhys - pRam->GCPhys;
        } while (off >= pRam->cb);
    }
    *ppRam = pRam;
    *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
    return VINF_SUCCESS;
}


/**
 * Convert GC Phys to HC Phys.
 *
 * @returns VBox status.
 * @param   pPGM        PGM handle.
 * @param   GCPhys      The GC physical address.
 * @param   pHCPhys     Where to store the corresponding HC physical address.
 *
 * @deprecated  Doesn't deal with zero, shared or write monitored pages.
 *              Avoid when writing new code!
 */
DECLINLINE(int) pgmRamGCPhys2HCPhys(PPGM pPGM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys)
{
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageEx(pPGM, GCPhys, &pPage);
    if (RT_FAILURE(rc))
        return rc;
    *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK);
    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0

/**
 * Inlined version of the ring-0 version of PGMDynMapHCPage that
 * optimizes access to pages already in the set.
 *
 * @returns VINF_SUCCESS. Will bail out to ring-3 on failure.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   HCPhys      The physical address of the page.
 * @param   ppv         Where to store the mapping address.
 */
DECLINLINE(int) pgmR0DynMapHCPageInlined(PPGM pPGM, RTHCPHYS HCPhys, void **ppv)
{
    PVM         pVM     = PGM2VM(pPGM);
    PPGMCPU     pPGMCPU = (PPGMCPU)((uint8_t *)VMMGetCpu(pVM) + pPGM->offVCpuPGM); /* very pretty ;-) */
    PPGMMAPSET  pSet    = &pPGMCPU->AutoSet; 

    STAM_PROFILE_START(&pPGMCPU->StatR0DynMapHCPageInl, a);
    Assert(!(HCPhys & PAGE_OFFSET_MASK));
    Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));

    unsigned    iHash   = PGMMAPSET_HASH(HCPhys);
    unsigned    iEntry  = pSet->aiHashTable[iHash];
    if (    iEntry < pSet->cEntries
        &&  pSet->aEntries[iEntry].HCPhys == HCPhys)
    {
        *ppv = pSet->aEntries[iEntry].pvPage;
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapHCPageInlHits);
    }
    else
    {
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapHCPageInlMisses);
        pgmR0DynMapHCPageCommon(pVM, pSet, HCPhys, ppv);
    }

    STAM_PROFILE_STOP(&pPGMCPU->StatR0DynMapHCPageInl, a);
    return VINF_SUCCESS;
}


/**
 * Inlined version of the ring-0 version of PGMDynMapGCPage that optimizes
 * access to pages already in the set.
 *
 * @returns See PGMDynMapGCPage.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   HCPhys      The physical address of the page.
 * @param   ppv         Where to store the mapping address.
 */
DECLINLINE(int) pgmR0DynMapGCPageInlined(PPGM pPGM, RTGCPHYS GCPhys, void **ppv)
{
    PVM     pVM     = PGM2VM(pPGM);
    PPGMCPU pPGMCPU = (PPGMCPU)((uint8_t *)VMMGetCpu(pVM) + pPGM->offVCpuPGM); /* very pretty ;-) */

    STAM_PROFILE_START(&pPGMCPU->StatR0DynMapGCPageInl, a);
    Assert(!(GCPhys & PAGE_OFFSET_MASK));

    /*
     * Get the ram range.
     */
    PPGMRAMRANGE    pRam = pPGM->CTX_SUFF(pRamRanges);
    RTGCPHYS        off = GCPhys - pRam->GCPhys;
    if (RT_UNLIKELY(off >= pRam->cb
        /** @todo   || page state stuff */))
    {
        /* This case is not counted into StatR0DynMapGCPageInl. */
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlRamMisses);
        return PGMDynMapGCPage(pVM, GCPhys, ppv);
    }

    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[off >> PAGE_SHIFT]);
    STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlRamHits);

    /*
     * pgmR0DynMapHCPageInlined with out stats.
     */
    PPGMMAPSET pSet = &pPGMCPU->AutoSet; 
    Assert(!(HCPhys & PAGE_OFFSET_MASK));
    Assert(pSet->cEntries <= RT_ELEMENTS(pSet->aEntries));

    unsigned    iHash   = PGMMAPSET_HASH(HCPhys);
    unsigned    iEntry  = pSet->aiHashTable[iHash];
    if (    iEntry < pSet->cEntries
        &&  pSet->aEntries[iEntry].HCPhys == HCPhys)
    {
        *ppv = pSet->aEntries[iEntry].pvPage;
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlHits);
    }
    else
    {
        STAM_COUNTER_INC(&pPGMCPU->StatR0DynMapGCPageInlMisses);
        pgmR0DynMapHCPageCommon(pVM, pSet, HCPhys, ppv);
    }

    STAM_PROFILE_STOP(&pPGMCPU->StatR0DynMapGCPageInl, a);
    return VINF_SUCCESS;
}

#endif /* VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

/**
 * Maps the page into current context (RC and maybe R0).
 *
 * @returns pointer to the mapping.
 * @param   pVM         Pointer to the PGM instance data.
 * @param   pPage       The page.
 */
DECLINLINE(void *) pgmPoolMapPageInlined(PPGM pPGM, PPGMPOOLPAGE pPage)
{
    if (pPage->idx >= PGMPOOL_IDX_FIRST)
    {
        Assert(pPage->idx < pPGM->CTX_SUFF(pPool)->cCurPages);
        void *pv;
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        pgmR0DynMapHCPageInlined(pPGM, pPage->Core.Key, &pv);
# else
        PGMDynMapHCPage(PGM2VM(pPGM), pPage->Core.Key, &pv);
# endif
        return pv;
    }
    AssertFatalMsgFailed(("pgmPoolMapPageInlined invalid page index %x\n", pPage->idx));
}

/**
 * Temporarily maps one host page specified by HC physical address, returning
 * pointer within the page.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns The address corresponding to HCPhys.
 * @param   pPGM        Pointer to the PVM instance data.
 * @param   HCPhys      HC Physical address of the page.
 */
DECLINLINE(void *) pgmDynMapHCPageOff(PPGM pPGM, RTHCPHYS HCPhys)
{
    void *pv;
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    pgmR0DynMapHCPageInlined(pPGM, HCPhys & ~(RTHCPHYS)PAGE_OFFSET_MASK, &pv);
# else
    PGMDynMapHCPage(PGM2VM(pPGM), HCPhys & ~(RTHCPHYS)PAGE_OFFSET_MASK, &pv);
# endif
    pv = (void *)((uintptr_t)pv | (HCPhys & PAGE_OFFSET_MASK));
    return pv;
}

#endif /*  VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 || IN_RC */
#ifndef IN_RC

/**
 * Queries the Physical TLB entry for a physical guest page,
 * attempting to load the TLB entry if necessary.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pPGM        The PGM instance handle.
 * @param   GCPhys      The address of the guest page.
 * @param   ppTlbe      Where to store the pointer to the TLB entry.
 */
DECLINLINE(int) pgmPhysPageQueryTlbe(PPGM pPGM, RTGCPHYS GCPhys, PPPGMPAGEMAPTLBE ppTlbe)
{
    int rc;
    PPGMPAGEMAPTLBE pTlbe = &pPGM->CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (pTlbe->GCPhys == (GCPhys & X86_PTE_PAE_PG_MASK))
    {
        STAM_COUNTER_INC(&pPGM->CTX_MID_Z(Stat,PageMapTlbHits));
        rc = VINF_SUCCESS;
    }
    else
        rc = pgmPhysPageLoadIntoTlb(pPGM, GCPhys);
    *ppTlbe = pTlbe;
    return rc;
}


/**
 * Queries the Physical TLB entry for a physical guest page,
 * attempting to load the TLB entry if necessary.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pPGM        The PGM instance handle.
 * @param   pPage       Pointer to the PGMPAGE structure corresponding to
 *                      GCPhys.
 * @param   GCPhys      The address of the guest page.
 * @param   ppTlbe      Where to store the pointer to the TLB entry.
 */
DECLINLINE(int) pgmPhysPageQueryTlbeWithPage(PPGM pPGM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPPGMPAGEMAPTLBE ppTlbe)
{
    int rc;
    PPGMPAGEMAPTLBE pTlbe = &pPGM->CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (pTlbe->GCPhys == (GCPhys & X86_PTE_PAE_PG_MASK))
    {
        STAM_COUNTER_INC(&pPGM->CTX_MID_Z(Stat,PageMapTlbHits));
        rc = VINF_SUCCESS;
    }
    else
        rc = pgmPhysPageLoadIntoTlbWithPage(pPGM, pPage, GCPhys);
    *ppTlbe = pTlbe;
    return rc;
}

#endif /* !IN_RC */

/**
 * Calculated the guest physical address of the large (4 MB) page in 32 bits paging mode.
 * Takes PSE-36 into account.
 *
 * @returns guest physical address
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   Pde         Guest Pde
 */
DECLINLINE(RTGCPHYS) pgmGstGet4MBPhysPage(PPGM pPGM, X86PDE Pde)
{
    RTGCPHYS GCPhys = Pde.u & X86_PDE4M_PG_MASK;
    GCPhys |= (RTGCPHYS)Pde.b.u8PageNoHigh << 32;

    return GCPhys & pPGM->GCPhys4MBPSEMask;
}


/**
 * Gets the page directory entry for the specified address (32-bit paging).
 *
 * @returns The page directory entry in question.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDE) pgmGstGet32bitPDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PCX86PD pGuestPD = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPD);
    if (RT_FAILURE(rc))
    {
        X86PDE ZeroPde = {0};
        AssertMsgFailedReturn(("%Rrc\n", rc), ZeroPde);
    }
#else
    PX86PD pGuestPD = pPGM->CTX_SUFF(pGst32BitPd);
# ifdef IN_RING3
    if (!pGuestPD)
        pGuestPD = pgmGstLazyMap32BitPD(pPGM);
# endif
#endif
    return pGuestPD->a[GCPtr >> X86_PD_SHIFT];
}


/**
 * Gets the address of a specific page directory entry (32-bit paging).
 *
 * @returns Pointer the page directory entry in question.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDE) pgmGstGet32bitPDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PD  pGuestPD = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPD);
    AssertRCReturn(rc, NULL);
#else
    PX86PD pGuestPD = pPGM->CTX_SUFF(pGst32BitPd);
# ifdef IN_RING3
    if (!pGuestPD)
        pGuestPD = pgmGstLazyMap32BitPD(pPGM);
# endif
#endif
    return &pGuestPD->a[GCPtr >> X86_PD_SHIFT];
}


/**
 * Gets the address the guest page directory (32-bit paging).
 *
 * @returns Pointer the page directory entry in question.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PD) pgmGstGet32bitPDPtr(PPGMCPU pPGM)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PD  pGuestPD = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPD);
    AssertRCReturn(rc, NULL);
#else
    PX86PD pGuestPD = pPGM->CTX_SUFF(pGst32BitPd);
# ifdef IN_RING3
    if (!pGuestPD)
        pGuestPD = pgmGstLazyMap32BitPD(pPGM);
# endif
#endif
    return pGuestPD;
}


/**
 * Gets the guest page directory pointer table.
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PDPT) pgmGstGetPaePDPTPtr(PPGMCPU pPGM)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PDPT pGuestPDPT = NULL;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPDPT);
    AssertRCReturn(rc, NULL);
#else
    PX86PDPT pGuestPDPT = pPGM->CTX_SUFF(pGstPaePdpt);
# ifdef IN_RING3
    if (!pGuestPDPT)
        pGuestPDPT = pgmGstLazyMapPaePDPT(pPGM);
# endif
#endif
    return pGuestPDPT;
}


/**
 * Gets the guest page directory pointer table entry for the specified address.
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPE) pgmGstGetPaePDPEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PDPT pGuestPDPT = 0;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPDPT);
    AssertRCReturn(rc, 0);
#else
    PX86PDPT pGuestPDPT = pPGM->CTX_SUFF(pGstPaePdpt);
# ifdef IN_RING3
    if (!pGuestPDPT)
        pGuestPDPT = pgmGstLazyMapPaePDPT(pPGM);
# endif
#endif
    return &pGuestPDPT->a[(GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE];
}


/**
 * Gets the page directory for the specified address.
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPAE) pgmGstGetPaePD(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);

    PX86PDPT        pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    AssertReturn(pGuestPDPT, NULL);
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    if (pGuestPDPT->a[iPdpt].n.u1Present)
    {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        PX86PDPAE   pGuestPD = NULL;
        int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
        AssertRCReturn(rc, NULL);
#else
        PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
        if (    !pGuestPD
            ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
            pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
        return pGuestPD;
        /* returning NULL is ok if we assume it's just an invalid page of some kind emulated as all 0s. (not quite true) */
    }
    return NULL;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns Pointer to the page directory entry in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDEPAE) pgmGstGetPaePDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);

    PX86PDPT        pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    AssertReturn(pGuestPDPT, NULL);
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    if (pGuestPDPT->a[iPdpt].n.u1Present)
    {
        const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        PX86PDPAE   pGuestPD = NULL;
        int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
        AssertRCReturn(rc, NULL);
#else
        PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
        if (    !pGuestPD
            ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
            pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
        return &pGuestPD->a[iPD];
        /* returning NIL_RTGCPHYS is ok if we assume it's just an invalid page or something which we'll emulate as all 0s. (not quite true) */
    }
    return NULL;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns The page directory entry in question.
 * @returns A non-present entry if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDEPAE) pgmGstGetPaePDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    AssertGCPtr32(GCPtr);
    X86PDEPAE   ZeroPde = {0};
    PX86PDPT    pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    if (RT_LIKELY(pGuestPDPT))
    {
        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
        if (pGuestPDPT->a[iPdpt].n.u1Present)
        {
            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
            PX86PDPAE   pGuestPD = NULL;
            int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
            AssertRCReturn(rc, ZeroPde);
#else
            PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
            if (    !pGuestPD
                ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
                pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
            return pGuestPD->a[iPD];
        }
    }
    return ZeroPde;
}


/**
 * Gets the page directory pointer table entry for the specified address
 * and returns the index into the page directory
 *
 * @returns Pointer to the page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   piPD        Receives the index into the returned page directory
 * @param   pPdpe       Receives the page directory pointer entry. Optional.
 */
DECLINLINE(PX86PDPAE) pgmGstGetPaePDPtr(PPGMCPU pPGM, RTGCPTR GCPtr, unsigned *piPD, PX86PDPE pPdpe)
{
    AssertGCPtr32(GCPtr);

    PX86PDPT        pGuestPDPT = pgmGstGetPaePDPTPtr(pPGM);
    AssertReturn(pGuestPDPT, NULL);
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    if (pPdpe)
        *pPdpe = pGuestPDPT->a[iPdpt];
    if (pGuestPDPT->a[iPdpt].n.u1Present)
    {
        const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
        PX86PDPAE   pGuestPD = NULL;
        int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK, (void **)&pGuestPD);
        AssertRCReturn(rc, NULL);
#else
        PX86PDPAE   pGuestPD = pPGM->CTX_SUFF(apGstPaePDs)[iPdpt];
        if (    !pGuestPD
            ||  (pGuestPDPT->a[iPdpt].u & X86_PDPE_PG_MASK) != pPGM->aGCPhysGstPaePDs[iPdpt])
            pGuestPD = pgmGstLazyMapPaePD(pPGM, iPdpt);
#endif
        *piPD = iPD;
        return pGuestPD;
        /* returning NIL_RTGCPHYS is ok if we assume it's just an invalid page of some kind emulated as all 0s. */
    }
    return NULL;
}

#ifndef IN_RC

/**
 * Gets the page map level-4 pointer for the guest.
 *
 * @returns Pointer to the PML4 page.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PML4) pgmGstGetLongModePML4Ptr(PPGMCPU pPGM)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PML4 pGuestPml4;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPml4);
    AssertRCReturn(rc, NULL);
#else
    PX86PML4 pGuestPml4 = pPGM->CTX_SUFF(pGstAmd64Pml4);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R3
    if (!pGuestPml4)
        pGuestPml4 = pgmGstLazyMapPml4(pPGM);
# endif
    Assert(pGuestPml4);
#endif
    return pGuestPml4;
}


/**
 * Gets the pointer to a page map level-4 entry.
 *
 * @returns Pointer to the PML4 entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPml4       The index.
 */
DECLINLINE(PX86PML4E) pgmGstGetLongModePML4EPtr(PPGMCPU pPGM, unsigned int iPml4)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PML4 pGuestPml4;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPml4);
    AssertRCReturn(rc, NULL);
#else
    PX86PML4 pGuestPml4 = pPGM->CTX_SUFF(pGstAmd64Pml4);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R3
    if (!pGuestPml4)
        pGuestPml4 = pgmGstLazyMapPml4(pPGM);
# endif
    Assert(pGuestPml4);
#endif
    return &pGuestPml4->a[iPml4];
}


/**
 * Gets a page map level-4 entry.
 *
 * @returns The PML4 entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPml4       The index.
 */
DECLINLINE(X86PML4E) pgmGstGetLongModePML4E(PPGMCPU pPGM, unsigned int iPml4)
{
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    PX86PML4 pGuestPml4;
    int rc = pgmR0DynMapGCPageInlined(PGMCPU2PGM(pPGM), pPGM->GCPhysCR3, (void **)&pGuestPml4);
    if (RT_FAILURE(rc))
    {
        X86PML4E ZeroPml4e = {0};
        AssertMsgFailedReturn(("%Rrc\n", rc), ZeroPml4e);
    }
#else
    PX86PML4 pGuestPml4 = pPGM->CTX_SUFF(pGstAmd64Pml4);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R3
    if (!pGuestPml4)
        pGuestPml4 = pgmGstLazyMapPml4(pPGM);
# endif
    Assert(pGuestPml4);
#endif
    return pGuestPml4->a[iPml4];
}


/**
 * Gets the page directory pointer entry for the specified address.
 *
 * @returns Pointer to the page directory pointer entry in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppPml4e     Page Map Level-4 Entry (out)
 */
DECLINLINE(PX86PDPE) pgmGstGetLongModePDPTPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e)
{
    PX86PML4        pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = *ppPml4e = &pGuestPml4->a[iPml4];
    if (pPml4e->n.u1Present)
    {
        PX86PDPT pPdpt;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPml4e->u & X86_PML4E_PG_MASK, &pPdpt);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        return &pPdpt->a[iPdpt];
    }
    return NULL;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns The page directory entry in question.
 * @returns A non-present entry if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppPml4e     Page Map Level-4 Entry (out)
 * @param   pPdpe       Page directory pointer table entry (out)
 */
DECLINLINE(X86PDEPAE) pgmGstGetLongModePDEEx(PPGMCPU pPGM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e, PX86PDPE pPdpe)
{
    X86PDEPAE       ZeroPde = {0};
    PX86PML4        pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = *ppPml4e = &pGuestPml4->a[iPml4];
    if (pPml4e->n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPml4e->u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, ZeroPde);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        *pPdpe = pPdptTemp->a[iPdpt];
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PCX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, ZeroPde);

            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD->a[iPD];
        }
    }

    return ZeroPde;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns The page directory entry in question.
 * @returns A non-present entry if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDEPAE) pgmGstGetLongModePDE(PPGMCPU pPGM, RTGCPTR64 GCPtr)
{
    X86PDEPAE       ZeroPde = {0};
    PCX86PML4       pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4 = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    if (pGuestPml4->a[iPml4].n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pGuestPml4->a[iPml4].u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, ZeroPde);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PCX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, ZeroPde);

            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD->a[iPD];
        }
    }
    return ZeroPde;
}


/**
 * Gets the page directory entry for the specified address.
 *
 * @returns Pointer to the page directory entry in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDEPAE) pgmGstGetLongModePDEPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr)
{
    PCX86PML4       pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4 = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    if (pGuestPml4->a[iPml4].n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pGuestPml4->a[iPml4].u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, NULL);

            const unsigned iPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return &pPD->a[iPD];
        }
    }
    return NULL;
}


/**
 * Gets the GUEST page directory pointer for the specified address.
 *
 * @returns The page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppPml4e     Page Map Level-4 Entry (out)
 * @param   pPdpe       Page directory pointer table entry (out)
 * @param   piPD        Receives the index into the returned page directory
 */
DECLINLINE(PX86PDPAE) pgmGstGetLongModePDPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e, PX86PDPE pPdpe, unsigned *piPD)
{
    PX86PML4        pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = *ppPml4e = &pGuestPml4->a[iPml4];
    if (pPml4e->n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPml4e->u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        *pPdpe = pPdptTemp->a[iPdpt];
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, NULL);

            *piPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD;
        }
    }
    return 0;
}

#endif /* !IN_RC */

/**
 * Gets the shadow page directory, 32-bit.
 *
 * @returns Pointer to the shadow 32-bit PD.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PD) pgmShwGet32BitPDPtr(PPGMCPU pPGM)
{
    return (PX86PD)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
}


/**
 * Gets the shadow page directory entry for the specified address, 32-bit.
 *
 * @returns Shadow 32-bit PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDE) pgmShwGet32BitPDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned iPd = (GCPtr >> X86_PD_SHIFT) & X86_PD_MASK;

    PX86PD pShwPde = pgmShwGet32BitPDPtr(pPGM);
    if (!pShwPde)
    {
        X86PDE ZeroPde = {0};
        return ZeroPde;
    }
    return pShwPde->a[iPd];
}


/**
 * Gets the pointer to the shadow page directory entry for the specified
 * address, 32-bit.
 *
 * @returns Pointer to the shadow 32-bit PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDE) pgmShwGet32BitPDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned iPd = (GCPtr >> X86_PD_SHIFT) & X86_PD_MASK;

    PX86PD pPde = pgmShwGet32BitPDPtr(pPGM);
    AssertReturn(pPde, NULL);
    return &pPde->a[iPd];
}


/**
 * Gets the shadow page pointer table, PAE.
 *
 * @returns Pointer to the shadow PAE PDPT.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PDPT) pgmShwGetPaePDPTPtr(PPGMCPU pPGM)
{
    return (PX86PDPT)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
}


/**
 * Gets the shadow page directory for the specified address, PAE.
 *
 * @returns Pointer to the shadow PD.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPAE) pgmShwGetPaePDPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    PX86PDPT        pPdpt = pgmShwGetPaePDPTPtr(pPGM);

    if (!pPdpt->a[iPdpt].n.u1Present)
        return NULL;

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pgmPoolGetPageByHCPhys(PGMCPU2VM(pPGM), pPdpt->a[iPdpt].u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPde, NULL);

    return (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pShwPde);
}


/**
 * Gets the shadow page directory for the specified address, PAE.
 *
 * @returns Pointer to the shadow PD.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDPAE) pgmShwGetPaePDPtr(PPGMCPU pPGM, PX86PDPT pPdpt, RTGCPTR GCPtr)
{
    const unsigned  iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;

    if (!pPdpt->a[iPdpt].n.u1Present)
        return NULL;

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pgmPoolGetPageByHCPhys(PGMCPU2VM(pPGM), pPdpt->a[iPdpt].u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPde, NULL);

    return (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pShwPde);
}


/**
 * Gets the shadow page directory entry, PAE.
 *
 * @returns PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PDEPAE) pgmShwGetPaePDE(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPd   = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;

    PX86PDPAE pShwPde = pgmShwGetPaePDPtr(pPGM, GCPtr);
    if (!pShwPde)
    {
        X86PDEPAE ZeroPde = {0};
        return ZeroPde;
    }
    return pShwPde->a[iPd];
}


/**
 * Gets the pointer to the shadow page directory entry for an address, PAE.
 *
 * @returns Pointer to the PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(PX86PDEPAE) pgmShwGetPaePDEPtr(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPd   = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;

    PX86PDPAE pPde = pgmShwGetPaePDPtr(pPGM, GCPtr);
    AssertReturn(pPde, NULL);
    return &pPde->a[iPd];
}

#ifndef IN_RC

/**
 * Gets the shadow page map level-4 pointer.
 *
 * @returns Pointer to the shadow PML4.
 * @param   pPGM        Pointer to the PGM instance data.
 */
DECLINLINE(PX86PML4) pgmShwGetLongModePML4Ptr(PPGMCPU pPGM)
{
    return (PX86PML4)PGMPOOL_PAGE_2_PTR_BY_PGMCPU(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
}


/**
 * Gets the shadow page map level-4 entry for the specified address.
 *
 * @returns The entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 */
DECLINLINE(X86PML4E) pgmShwGetLongModePML4E(PPGMCPU pPGM, RTGCPTR GCPtr)
{
    const unsigned  iPml4 = ((RTGCUINTPTR64)GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PX86PML4        pShwPml4 = pgmShwGetLongModePML4Ptr(pPGM);

    if (!pShwPml4)
    {
        X86PML4E ZeroPml4e = {0};
        return ZeroPml4e;
    }
    return pShwPml4->a[iPml4];
}


/**
 * Gets the pointer to the specified shadow page map level-4 entry.
 *
 * @returns The entry.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPml4       The PML4 index.
 */
DECLINLINE(PX86PML4E) pgmShwGetLongModePML4EPtr(PPGMCPU pPGM, unsigned int iPml4)
{
    PX86PML4 pShwPml4 = pgmShwGetLongModePML4Ptr(pPGM);
    if (!pShwPml4)
        return NULL;
    return &pShwPml4->a[iPml4];
}


/**
 * Gets the GUEST page directory pointer for the specified address.
 *
 * @returns The page directory in question.
 * @returns NULL if the page directory is not present or on an invalid page.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   piPD        Receives the index into the returned page directory
 */
DECLINLINE(PX86PDPAE) pgmGstGetLongModePDPtr(PPGMCPU pPGM, RTGCPTR64 GCPtr, unsigned *piPD)
{
    PCX86PML4       pGuestPml4 = pgmGstGetLongModePML4Ptr(pPGM);
    const unsigned  iPml4  = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    if (pGuestPml4->a[iPml4].n.u1Present)
    {
        PCX86PDPT   pPdptTemp;
        int rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pGuestPml4->a[iPml4].u & X86_PML4E_PG_MASK, &pPdptTemp);
        AssertRCReturn(rc, NULL);

        const unsigned iPdpt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
        if (pPdptTemp->a[iPdpt].n.u1Present)
        {
            PX86PDPAE pPD;
            rc = PGM_GCPHYS_2_PTR_BY_PGMCPU(pPGM, pPdptTemp->a[iPdpt].u & X86_PDPE_PG_MASK, &pPD);
            AssertRCReturn(rc, NULL);

            *piPD = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
            return pPD;
        }
    }
    return NULL;
}

#endif /* !IN_RC */

/**
 * Gets the page state for a physical handler.
 *
 * @returns The physical handler page state.
 * @param   pCur    The physical handler in question.
 */
DECLINLINE(unsigned) pgmHandlerPhysicalCalcState(PPGMPHYSHANDLER pCur)
{
    switch (pCur->enmType)
    {
        case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE:
            return PGM_PAGE_HNDL_PHYS_STATE_WRITE;

        case PGMPHYSHANDLERTYPE_MMIO:
        case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:
            return PGM_PAGE_HNDL_PHYS_STATE_ALL;

        default:
            AssertFatalMsgFailed(("Invalid type %d\n", pCur->enmType));
    }
}


/**
 * Gets the page state for a virtual handler.
 *
 * @returns The virtual handler page state.
 * @param   pCur    The virtual handler in question.
 * @remarks This should never be used on a hypervisor access handler.
 */
DECLINLINE(unsigned) pgmHandlerVirtualCalcState(PPGMVIRTHANDLER pCur)
{
    switch (pCur->enmType)
    {
        case PGMVIRTHANDLERTYPE_WRITE:
            return PGM_PAGE_HNDL_VIRT_STATE_WRITE;
        case PGMVIRTHANDLERTYPE_ALL:
            return PGM_PAGE_HNDL_VIRT_STATE_ALL;
        default:
            AssertFatalMsgFailed(("Invalid type %d\n", pCur->enmType));
    }
}


/**
 * Clears one physical page of a virtual handler
 *
 * @param   pPGM    Pointer to the PGM instance.
 * @param   pCur    Virtual handler structure
 * @param   iPage   Physical page index
 *
 * @remark  Only used when PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL is being set, so no
 *          need to care about other handlers in the same page.
 */
DECLINLINE(void) pgmHandlerVirtualClearPage(PPGM pPGM, PPGMVIRTHANDLER pCur, unsigned iPage)
{
    const PPGMPHYS2VIRTHANDLER pPhys2Virt = &pCur->aPhysToVirt[iPage];

    /*
     * Remove the node from the tree (it's supposed to be in the tree if we get here!).
     */
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
    AssertReleaseMsg(pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE,
                     ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                      pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias));
#endif
    if (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_IS_HEAD)
    {
        /* We're the head of the alias chain. */
        PPGMPHYS2VIRTHANDLER pRemove = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysRemove(&pPGM->CTX_SUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key); NOREF(pRemove);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
        AssertReleaseMsg(pRemove != NULL,
                         ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                          pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias));
        AssertReleaseMsg(pRemove == pPhys2Virt,
                         ("wanted: pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                          "   got:    pRemove=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                          pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias,
                          pRemove, pRemove->Core.Key, pRemove->Core.KeyLast, pRemove->offVirtHandler, pRemove->offNextAlias));
#endif
        if (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK)
        {
            /* Insert the next list in the alias chain into the tree. */
            PPGMPHYS2VIRTHANDLER pNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pPhys2Virt + (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
            AssertReleaseMsg(pNext->offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE,
                             ("pNext=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                             pNext, pNext->Core.Key, pNext->Core.KeyLast, pNext->offVirtHandler, pNext->offNextAlias));
#endif
            pNext->offNextAlias |= PGMPHYS2VIRTHANDLER_IS_HEAD;
            bool fRc = RTAvlroGCPhysInsert(&pPGM->CTX_SUFF(pTrees)->PhysToVirtHandlers, &pNext->Core);
            AssertRelease(fRc);
        }
    }
    else
    {
        /* Locate the previous node in the alias chain. */
        PPGMPHYS2VIRTHANDLER pPrev = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGet(&pPGM->CTX_SUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
        AssertReleaseMsg(pPrev != pPhys2Virt,
                         ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} pPrev=%p\n",
                          pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias, pPrev));
#endif
        for (;;)
        {
            PPGMPHYS2VIRTHANDLER pNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pPrev + (pPrev->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
            if (pNext == pPhys2Virt)
            {
                /* unlink. */
                LogFlow(("pgmHandlerVirtualClearPage: removed %p:{.offNextAlias=%#RX32} from alias chain. prev %p:{.offNextAlias=%#RX32} [%RGp-%RGp]\n",
                         pPhys2Virt, pPhys2Virt->offNextAlias, pPrev, pPrev->offNextAlias, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast));
                if (!(pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK))
                    pPrev->offNextAlias &= ~PGMPHYS2VIRTHANDLER_OFF_MASK;
                else
                {
                    PPGMPHYS2VIRTHANDLER pNewNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pPhys2Virt + (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
                    pPrev->offNextAlias = ((intptr_t)pNewNext - (intptr_t)pPrev)
                                        | (pPrev->offNextAlias & ~PGMPHYS2VIRTHANDLER_OFF_MASK);
                }
                break;
            }

            /* next */
            if (pNext == pPrev)
            {
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                AssertReleaseMsg(pNext != pPrev,
                                 ("pPhys2Virt=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} pPrev=%p\n",
                                  pPhys2Virt, pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler, pPhys2Virt->offNextAlias, pPrev));
#endif
                break;
            }
            pPrev = pNext;
        }
    }
    Log2(("PHYS2VIRT: Removing %RGp-%RGp %#RX32 %s\n",
          pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias, R3STRING(pCur->pszDesc)));
    pPhys2Virt->offNextAlias = 0;
    pPhys2Virt->Core.KeyLast = NIL_RTGCPHYS; /* require reinsert */

    /*
     * Clear the ram flags for this page.
     */
    PPGMPAGE pPage = pgmPhysGetPage(pPGM, pPhys2Virt->Core.Key);
    AssertReturnVoid(pPage);
    PGM_PAGE_SET_HNDL_VIRT_STATE(pPage, PGM_PAGE_HNDL_VIRT_STATE_NONE);
}


/**
 * Internal worker for finding a 'in-use' shadow page give by it's physical address.
 *
 * @returns Pointer to the shadow page structure.
 * @param   pPool       The pool.
 * @param   HCPhys      The HC physical address of the shadow page.
 */
DECLINLINE(PPGMPOOLPAGE) pgmPoolGetPage(PPGMPOOL pPool, RTHCPHYS HCPhys)
{
    /*
     * Look up the page.
     */
    PPGMPOOLPAGE pPage = (PPGMPOOLPAGE)RTAvloHCPhysGet(&pPool->HCPhysTree, HCPhys & X86_PTE_PAE_PG_MASK);
    AssertFatalMsg(pPage && pPage->enmKind != PGMPOOLKIND_FREE, ("HCPhys=%RHp pPage=%p idx=%d\n", HCPhys, pPage, (pPage) ? pPage->idx : 0));
    return pPage;
}


/**
 * Internal worker for finding a 'in-use' shadow page give by it's physical address.
 *
 * @returns Pointer to the shadow page structure.
 * @param   pPool       The pool.
 * @param   idx         The pool page index.
 */
DECLINLINE(PPGMPOOLPAGE) pgmPoolGetPageByIdx(PPGMPOOL pPool, unsigned idx)
{
    AssertFatalMsg(idx >= PGMPOOL_IDX_FIRST && idx < pPool->cCurPages, ("idx=%d\n", idx));
    return &pPool->aPages[idx];
}


#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
/**
 * Clear references to guest physical memory.
 *
 * @param   pPool       The pool.
 * @param   pPoolPage   The pool page.
 * @param   pPhysPage   The physical guest page tracking structure.
 */
DECLINLINE(void) pgmTrackDerefGCPhys(PPGMPOOL pPool, PPGMPOOLPAGE pPoolPage, PPGMPAGE pPhysPage)
{
    /*
     * Just deal with the simple case here.
     */
# ifdef LOG_ENABLED
    const unsigned uOrg = PGM_PAGE_GET_TRACKING(pPhysPage);
# endif
    const unsigned cRefs = PGM_PAGE_GET_TD_CREFS(pPhysPage);
    if (cRefs == 1)
    {
        Assert(pPoolPage->idx == PGM_PAGE_GET_TD_IDX(pPhysPage));
        PGM_PAGE_SET_TRACKING(pPhysPage, 0);
    }
    else
        pgmPoolTrackPhysExtDerefGCPhys(pPool, pPoolPage, pPhysPage);
    Log2(("pgmTrackDerefGCPhys: %x -> %x pPhysPage=%R[pgmpage]\n", uOrg, PGM_PAGE_GET_TRACKING(pPhysPage), pPhysPage ));
}
#endif /* PGMPOOL_WITH_GCPHYS_TRACKING */


#ifdef PGMPOOL_WITH_CACHE
/**
 * Moves the page to the head of the age list.
 *
 * This is done when the cached page is used in one way or another.
 *
 * @param   pPool       The pool.
 * @param   pPage       The cached page.
 */
DECLINLINE(void) pgmPoolCacheUsed(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    /*
     * Move to the head of the age list.
     */
    if (pPage->iAgePrev != NIL_PGMPOOL_IDX)
    {
        /* unlink */
        pPool->aPages[pPage->iAgePrev].iAgeNext = pPage->iAgeNext;
        if (pPage->iAgeNext != NIL_PGMPOOL_IDX)
            pPool->aPages[pPage->iAgeNext].iAgePrev = pPage->iAgePrev;
        else
            pPool->iAgeTail = pPage->iAgePrev;

        /* insert at head */
        pPage->iAgePrev = NIL_PGMPOOL_IDX;
        pPage->iAgeNext = pPool->iAgeHead;
        Assert(pPage->iAgeNext != NIL_PGMPOOL_IDX); /* we would've already been head then */
        pPool->iAgeHead = pPage->idx;
        pPool->aPages[pPage->iAgeNext].iAgePrev = pPage->idx;
    }
}
#endif /* PGMPOOL_WITH_CACHE */

/**
 * Locks a page to prevent flushing (important for cr3 root pages or shadow pae pd pages).
 *
 * @param   pVM         VM Handle.
 * @param   pPage       PGM pool page
 */
DECLINLINE(void) pgmPoolLockPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Assert(!pPage->fLocked);
    pPage->fLocked = true;
}


/**
 * Unlocks a page to allow flushing again
 *
 * @param   pVM         VM Handle.
 * @param   pPage       PGM pool page
 */
DECLINLINE(void) pgmPoolUnlockPage(PPGMPOOL pPool, PPGMPOOLPAGE pPage)
{
    Assert(pPage->fLocked);
    pPage->fLocked = false;
}


/**
 * Checks if the page is locked (e.g. the active CR3 or one of the four PDs of a PAE PDPT)
 *
 * @returns VBox status code.
 * @param   pPage       PGM pool page
 */
DECLINLINE(bool) pgmPoolIsPageLocked(PPGM pPGM, PPGMPOOLPAGE pPage)
{
    if (pPage->fLocked)
    {
        LogFlow(("pgmPoolIsPageLocked found root page %d\n", pPage->enmKind));
        if (pPage->cModifications)
            pPage->cModifications = 1; /* reset counter (can't use 0, or else it will be reinserted in the modified list) */
        return true;
    }
    return false;
}

/**
 * Tells if mappings are to be put into the shadow page table or not
 *
 * @returns boolean result
 * @param   pVM         VM handle.
 */
DECLINLINE(bool) pgmMapAreMappingsEnabled(PPGM pPGM)
{
#ifdef IN_RING0
    /* There are no mappings in VT-x and AMD-V mode. */
    Assert(pPGM->fDisableMappings);
    return false;
#else
    return !pPGM->fDisableMappings;
#endif
}

/** @} */

#endif


