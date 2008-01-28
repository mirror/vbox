/* $Id$ */
/** @file
 * MM - Internal header file.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___MMInternal_h
#define ___MMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/stam.h>
#include <iprt/avl.h>
#include <iprt/critsect.h>


#if !defined(IN_MM_R3) && !defined(IN_MM_R0) && !defined(IN_MM_GC)
# error "Not in MM! This is an internal header!"
#endif


/** @defgroup grp_mm_int   Internals
 * @internal
 * @ingroup grp_mm
 * @{
 */

/** @name VM Ring-3 Heap Internals
 * @{
 */

/** @def MMR3HEAP_WITH_STATISTICS
 * Enable MMR3Heap statistics.
 */
#if !defined(MMR3HEAP_WITH_STATISTICS) && defined(VBOX_WITH_STATISTICS)
# define MMR3HEAP_WITH_STATISTICS
#endif

/** @def MMR3HEAP_SIZE_ALIGNMENT
 * The allocation size alignment of the MMR3Heap.
 */
#define MMR3HEAP_SIZE_ALIGNMENT     16

/**
 * Heap statistics record.
 * There is one global and one per allocation tag.
 */
typedef struct MMHEAPSTAT
{
    /** Core avl node, key is the tag. */
    AVLULNODECORE   Core;
    /** Pointer to the heap the memory belongs to. */
    struct MMHEAP  *pHeap;
#ifdef MMR3HEAP_WITH_STATISTICS
    /** Number of allocation. */
    uint64_t        cAllocations;
    /** Number of reallocations. */
    uint64_t        cReallocations;
    /** Number of frees. */
    uint64_t        cFrees;
    /** Failures. */
    uint64_t        cFailures;
    /** Number of bytes allocated (sum). */
    uint64_t        cbAllocated;
    /** Number of bytes freed. */
    uint64_t        cbFreed;
    /** Number of bytes currently allocated. */
    size_t          cbCurAllocated;
#endif
} MMHEAPSTAT;
/** Pointer to heap statistics record. */
typedef MMHEAPSTAT *PMMHEAPSTAT;



/**
 * Additional heap block header for relating allocations to the VM.
 */
typedef struct MMHEAPHDR
{
    /** Pointer to the next record. */
    struct MMHEAPHDR   *pNext;
    /** Pointer to the previous record. */
    struct MMHEAPHDR   *pPrev;
    /** Pointer to the heap statistics record.
     * (Where the a PVM can be found.) */
    PMMHEAPSTAT         pStat;
    /** Size of the allocation (including this header). */
    size_t              cbSize;
} MMHEAPHDR;
/** Pointer to MM heap header. */
typedef MMHEAPHDR *PMMHEAPHDR;


/** MM Heap structure. */
typedef struct MMHEAP
{
    /** Lock protecting the heap. */
    RTCRITSECT          Lock;
    /** Heap block list head. */
    PMMHEAPHDR          pHead;
    /** Heap block list tail. */
    PMMHEAPHDR          pTail;
    /** Heap per tag statistics tree. */
    PAVLULNODECORE      pStatTree;
    /** The VM handle. */
    PVM                 pVM;
    /** Heap global statistics. */
    MMHEAPSTAT          Stat;
} MMHEAP;
/** Pointer to MM Heap structure. */
typedef MMHEAP *PMMHEAP;

/** @} */



/** @name Hypervisor Heap Internals
 * @{
 */

/** @def MMHYPER_HEAP_FREE_DELAY
 * If defined, it indicates the number of frees that should be delayed.
 */
#if defined(__DOXYGEN__)
# define MMHYPER_HEAP_FREE_DELAY   64
#endif

/** @def MMHYPER_HEAP_FREE_POISON
 * If defined, it indicates that freed memory should be poisoned
 * with the value it has.
 */
#if defined(VBOX_STRICT) || defined(__DOXYGEN__)
# define MMHYPER_HEAP_FREE_POISON   0xCB
#endif

/**
 * Hypervisor heap statistics record.
 * There is one global and one per allocation tag.
 */
typedef struct MMHYPERSTAT
{
    /** Core avl node, key is the tag.
     * @todo The type is wrong! Get your lazy a$$ over and create that offsetted uint32_t version we need here!  */
    AVLOGCPHYSNODECORE Core;
    /** Aligning the 64-bit fields on a 64-bit line. */
    uint32_t        u32Padding0;
    /** Indicator for whether these statistics are registered with STAM or not. */
    bool            fRegistered;
    /** Number of allocation. */
    uint64_t        cAllocations;
    /** Number of frees. */
    uint64_t        cFrees;
    /** Failures. */
    uint64_t        cFailures;
    /** Number of bytes allocated (sum). */
    uint64_t        cbAllocated;
    /** Number of bytes freed (sum). */
    uint64_t        cbFreed;
    /** Number of bytes currently allocated. */
    uint32_t        cbCurAllocated;
    /** Max number of bytes allocated. */
    uint32_t        cbMaxAllocated;
} MMHYPERSTAT;
/** Pointer to hypervisor heap statistics record. */
typedef MMHYPERSTAT *PMMHYPERSTAT;

/**
 * Hypervisor heap chunk.
 */
typedef struct MMHYPERCHUNK
{
    /** Previous block in the list of all blocks.
     * This is relative to the start of the heap. */
    uint32_t                offNext;
    /** Offset to the previous block relative to this one. */
    int32_t                 offPrev;
    /** The statistics record this allocation belongs to (self relative). */
    int32_t                 offStat;
    /** Offset to the heap block (self relative). */
    int32_t                 offHeap;
} MMHYPERCHUNK;
/** Pointer to a hypervisor heap chunk. */
typedef MMHYPERCHUNK *PMMHYPERCHUNK;


/**
 * Hypervisor heap chunk.
 */
typedef struct MMHYPERCHUNKFREE
{
    /** Main list. */
    MMHYPERCHUNK            core;
    /** Offset of the next chunk in the list of free nodes. */
    uint32_t                offNext;
    /** Offset of the previous chunk in the list of free nodes. */
    int32_t                 offPrev;
    /** Size of the block. */
    uint32_t                cb;
} MMHYPERCHUNKFREE;
/** Pointer to a free hypervisor heap chunk. */
typedef MMHYPERCHUNKFREE *PMMHYPERCHUNKFREE;


/**
 * The hypervisor heap.
 */
typedef struct MMHYPERHEAP
{
    /** The typical magic (MMHYPERHEAP_MAGIC). */
    uint32_t                u32Magic;
    /** The heap size. (This structure is not included!) */
    uint32_t                cbHeap;
    /** The HC Ring-3 address of the VM. */
    R3PTRTYPE(PVM)          pVMHC;
    /** The HC Ring-3 address of the heap. */
    R3R0PTRTYPE(uint8_t *)  pbHeapHC;
    /** The GC address of the heap. */
    GCPTRTYPE(uint8_t *)    pbHeapGC;
    /** The GC address of the VM. */
    GCPTRTYPE(PVM)          pVMGC;
    /** The amount of free memory in the heap. */
    uint32_t                cbFree;
    /** Offset of the first free chunk in the heap.
     * The offset is relative to the start of the heap. */
    uint32_t                offFreeHead;
    /** Offset of the last free chunk in the heap.
     * The offset is relative to the start of the heap. */
    uint32_t                offFreeTail;
    /** Offset of the first page aligned block in the heap.
     * The offset is equal to cbHeap initially. */
    uint32_t                offPageAligned;
    /** Tree of hypervisor heap statistics. */
    AVLOGCPHYSTREE          HyperHeapStatTree;
#ifdef MMHYPER_HEAP_FREE_DELAY
    /** Where to insert the next free. */
    uint32_t                iDelayedFree;
    /** Array of delayed frees. Circular. Offsets relative to this structure. */
    struct
    {
        /** The free caller address. */
        RTUINTPTR           uCaller;
        /** The offset of the freed chunk. */
        uint32_t            offChunk;
    }                   aDelayedFrees[MMHYPER_HEAP_FREE_DELAY];
#else
    /** Padding the structure to a 64-bit aligned size. */
    uint32_t            u32Padding0;
#endif
} MMHYPERHEAP;
/** Pointer to the hypervisor heap. */
typedef MMHYPERHEAP *PMMHYPERHEAP;

/** Magic value for MMHYPERHEAP. (C. S. Lewis) */
#define MMHYPERHEAP_MAGIC   0x18981129


/**
 * Hypervisor heap minimum alignment (16 bytes).
 */
#define MMHYPER_HEAP_ALIGN_MIN          16

/**
 * The aligned size of the the MMHYPERHEAP structure.
 */
#define MMYPERHEAP_HDR_SIZE             RT_ALIGN_Z(sizeof(MMHYPERHEAP), MMHYPER_HEAP_ALIGN_MIN * 4)

/** @name Hypervisor heap chunk flags.
 * The flags are put in the first bits of the MMHYPERCHUNK::offPrev member.
 * These bits aren't used anyway because of the chunk minimal alignment (16 bytes).
 * @{ */
/** The chunk is free. (The code ASSUMES this is 0!) */
#define MMHYPERCHUNK_FLAGS_FREE         0x0
/** The chunk is in use. */
#define MMHYPERCHUNK_FLAGS_USED         0x1
/** The type mask. */
#define MMHYPERCHUNK_FLAGS_TYPE_MASK    0x1
/** The flag mask */
#define MMHYPERCHUNK_FLAGS_MASK         0x1

/** Checks if the chunk is free. */
#define MMHYPERCHUNK_ISFREE(pChunk)                 ( (((pChunk)->offPrev) & MMHYPERCHUNK_FLAGS_TYPE_MASK) == MMHYPERCHUNK_FLAGS_FREE )
/** Checks if the chunk is used. */
#define MMHYPERCHUNK_ISUSED(pChunk)                 ( (((pChunk)->offPrev) & MMHYPERCHUNK_FLAGS_TYPE_MASK) == MMHYPERCHUNK_FLAGS_USED )
/** Toggles FREE/USED flag of a chunk. */
#define MMHYPERCHUNK_SET_TYPE(pChunk, type)         do { (pChunk)->offPrev = ((pChunk)->offPrev & ~MMHYPERCHUNK_FLAGS_TYPE_MASK) | ((type) & MMHYPERCHUNK_FLAGS_TYPE_MASK); } while (0)

/** Gets the prev offset without the flags. */
#define MMHYPERCHUNK_GET_OFFPREV(pChunk)            ((int32_t)((pChunk)->offPrev & ~MMHYPERCHUNK_FLAGS_MASK))
/** Sets the prev offset without changing the flags. */
#define MMHYPERCHUNK_SET_OFFPREV(pChunk, off)       do { (pChunk)->offPrev = (off) | ((pChunk)->offPrev & MMHYPERCHUNK_FLAGS_MASK); } while (0)
#if 0
/** Clears one or more flags. */
#define MMHYPERCHUNK_FLAGS_OP_CLEAR(pChunk, fFlags) do { ((pChunk)->offPrev) &= ~((fFlags) & MMHYPERCHUNK_FLAGS_MASK); } while (0)
/** Sets one or more flags. */
#define MMHYPERCHUNK_FLAGS_OP_SET(pChunk, fFlags)   do { ((pChunk)->offPrev) |=  ((fFlags) & MMHYPERCHUNK_FLAGS_MASK); } while (0)
/** Checks if one is set. */
#define MMHYPERCHUNK_FLAGS_OP_ISSET(pChunk, fFlag)  (!!(((pChunk)->offPrev) & ((fFlag) & MMHYPERCHUNK_FLAGS_MASK)))
#endif
/** @} */

/** @} */


/** @name Page Pool Internals
 * @{
 */

/**
 * Page sub pool
 *
 * About the allocation of this structrue. To keep the number of heap blocks,
 * the number of heap calls, and fragmentation low we allocate all the data
 * related to a MMPAGESUBPOOL node in one chunk. That means that after the
 * bitmap (which is of variable size) comes the SUPPAGE records and then
 * follows the lookup tree nodes.
 */
typedef struct MMPAGESUBPOOL
{
    /** Pointer to next sub pool. */
    struct MMPAGESUBPOOL   *pNext;
    /** Pointer to next sub pool in the free chain.
     * This is NULL if we're not in the free chain or at the end of it. */
    struct MMPAGESUBPOOL   *pNextFree;
    /** Pointer to array of lock ranges.
     * This is allocated together with the MMPAGESUBPOOL and thus needs no freeing.
     * It follows immediately after the bitmap.
     * The reserved field is a pointer to this structure.
     */
    PSUPPAGE                paPhysPages;
    /** Pointer to the first page. */
    void                   *pvPages;
    /** Size of the subpool. */
    unsigned                cPages;
    /** Number of free pages. */
    unsigned                cPagesFree;
    /** The allocation bitmap.
     * This may extend beyond the end of the defined array size.
     */
    unsigned                auBitmap[1];
    /* ... SUPPAGE          aRanges[1]; */
} MMPAGESUBPOOL;
/** Pointer to page sub pool. */
typedef MMPAGESUBPOOL *PMMPAGESUBPOOL;

/**
 * Page pool.
 */
typedef struct MMPAGEPOOL
{
    /** List of subpools. */
    PMMPAGESUBPOOL      pHead;
    /** Head of subpools with free pages. */
    PMMPAGESUBPOOL      pHeadFree;
    /** AVLPV tree for looking up HC virtual addresses.
     * The tree contains MMLOOKUPVIRTPP records.
     */
    PAVLPVNODECORE      pLookupVirt;
    /** Tree for looking up HC physical addresses.
     * The tree contains MMLOOKUPPHYSHC records.
     */
    AVLHCPHYSTREE       pLookupPhys;
    /** Pointer to the VM this pool belongs. */
    PVM                 pVM;
    /** Flag indicating the allocation method.
     * Set: SUPLowAlloc().
     * Clear: SUPPageAlloc() + SUPPageLock(). */
    bool                fLow;
    /** Number of subpools. */
    uint32_t            cSubPools;
    /** Number of pages in pool. */
    uint32_t            cPages;
#ifdef VBOX_WITH_STATISTICS
    /** Number of free pages in pool. */
    uint32_t            cFreePages;
    /** Number of alloc calls. */
    STAMCOUNTER         cAllocCalls;
    /** Number of free calls. */
    STAMCOUNTER         cFreeCalls;
    /** Number of to phys conversions. */
    STAMCOUNTER         cToPhysCalls;
    /** Number of to virtual conversions. */
    STAMCOUNTER         cToVirtCalls;
    /** Number of real errors. */
    STAMCOUNTER         cErrors;
#endif
} MMPAGEPOOL;
/** Pointer to page pool. */
typedef MMPAGEPOOL     *PMMPAGEPOOL;

/**
 * Lookup record for HC virtual memory in the page pool.
 */
typedef struct MMPPLOOKUPHCPTR
{
    /** The key is virtual address. */
    AVLPVNODECORE   Core;
    /** Pointer to subpool if lookup record for a pool. */
    struct MMPAGESUBPOOL   *pSubPool;
} MMPPLOOKUPHCPTR;
/** Pointer to virtual memory lookup record. */
typedef MMPPLOOKUPHCPTR *PMMPPLOOKUPHCPTR;

/**
 * Lookup record for HC physical memory.
 */
typedef struct MMPPLOOKUPHCPHYS
{
    /** The key is physical address. */
    AVLHCPHYSNODECORE   Core;
    /** Pointer to SUPPAGE record for this physical address. */
    PSUPPAGE            pPhysPage;
} MMPPLOOKUPHCPHYS;
/** Pointer to physical memory lookup record. */
typedef MMPPLOOKUPHCPHYS *PMMPPLOOKUPHCPHYS;

/** @} */



/**
 * Type of memory that's locked.
 */
typedef enum MMLOCKEDTYPE
{
    /** Hypervisor: Ring-3 memory locked by MM. */
    MM_LOCKED_TYPE_HYPER,
    /** Hypervisor: Ring-3 memory locked by MM that shouldn't be freed up. */
    MM_LOCKED_TYPE_HYPER_NOFREE,
    /** Hypervisor: Pre-locked ring-3 pages. */
    MM_LOCKED_TYPE_HYPER_PAGES,
    /** Guest: Physical VM memory (RAM & MMIO2). */
    MM_LOCKED_TYPE_PHYS
} MMLOCKEDTYPE;
/** Pointer to memory type. */
typedef MMLOCKEDTYPE *PMMLOCKEDTYPE;


/**
 * Converts a SUPPAGE pointer to a MMLOCKEDMEM pointer.
 * @returns Pointer to the MMLOCKEDMEM record the range is associated with.
 * @param   pSupPage    Pointer to SUPPAGE structure managed by MM.
 */
#define MM_SUPRANGE_TO_MMLOCKEDMEM(pSupPage) ((PMMLOCKEDMEM)pSupPage->uReserved)


/**
 * Locked memory record.
 */
typedef struct MMLOCKEDMEM
{
    /** Address (host mapping). */
    void               *pv;
    /** Size. */
    size_t              cb;
    /** Next record. */
    struct MMLOCKEDMEM *pNext;
    /** Record type. */
    MMLOCKEDTYPE        eType;
    /** Type specific data. */
    union
    {
        /** Data for MM_LOCKED_TYPE_HYPER, MM_LOCKED_TYPE_HYPER_NOFREE and MM_LOCKED_TYPE_HYPER_PAGES. */
        struct
        {
            unsigned    uNothing;
        } hyper;

        /** Data for MM_LOCKED_TYPE_PHYS. */
        struct
        {
            /** The GC physical address.
             * (Assuming that this is a linear range of GC physical pages.)
             */
            RTGCPHYS    GCPhys;
        } phys;
    } u;

    /** Physical Page Array. (Variable length.)
     * The uReserved field contains pointer to the MMLOCKMEM record.
     * Use the macro MM_SUPPAGE_TO_MMLOCKEDMEM() to convert.
     *
     * For MM_LOCKED_TYPE_PHYS the low 12 bits of the pvPhys member
     * are bits (MM_RAM_FLAGS_*) and not part of the physical address.
     */
    SUPPAGE             aPhysPages[1];
} MMLOCKEDMEM;
/** Pointer to locked memory. */
typedef MMLOCKEDMEM *PMMLOCKEDMEM;


/**
 * A registered Rom range.
 *
 * This is used to track ROM registrations both for debug reasons
 * and for resetting shadow ROM at reset.
 *
 * This is allocated of the MMR3Heap and thus only accessibel from ring-3.
 */
typedef struct MMROMRANGE
{
    /** Pointer to the next */
    struct MMROMRANGE  *pNext;
    /** Address of the range. */
    RTGCPHYS            GCPhys;
    /** Size of the range. */
    uint32_t            cbRange;
    /** Shadow ROM? */
    bool                fShadow;
    /** Is the shadow ROM currently wriable? */
    bool                fWritable;
    /** The address of the virgin ROM image for shadow ROM. */
    const void         *pvBinary;
    /** The address of the guest RAM that's shadowing the ROM. (lazy bird) */
    void               *pvCopy;
    /** The ROM description. */
    const char         *pszDesc;
} MMROMRANGE;
/** Pointer to a ROM range. */
typedef MMROMRANGE *PMMROMRANGE;


/**
 * Hypervisor memory mapping type.
 */
typedef enum MMLOOKUPHYPERTYPE
{
    /** Invalid record. This is used for record which are incomplete. */
    MMLOOKUPHYPERTYPE_INVALID = 0,
    /** Mapping of locked memory. */
    MMLOOKUPHYPERTYPE_LOCKED,
    /** Mapping of contiguous HC physical memory. */
    MMLOOKUPHYPERTYPE_HCPHYS,
    /** Mapping of contiguous GC physical memory. */
    MMLOOKUPHYPERTYPE_GCPHYS,
    /** Dynamic mapping area (MMR3HyperReserve).
     * A conversion will require to check what's in the page table for the pages. */
    MMLOOKUPHYPERTYPE_DYNAMIC
} MMLOOKUPHYPERTYPE;

/**
 * Lookup record for the hypervisor memory area.
 */
typedef struct MMLOOKUPHYPER
{
    /** Byte offset from the start of this record to the next.
     * If the value is NIL_OFFSET the chain is terminated. */
    int32_t                 offNext;
    /** Offset into the hypvervisor memory area. */
    uint32_t                off;
    /** Size of this part. */
    uint32_t                cb;
    /** Locking type. */
    MMLOOKUPHYPERTYPE       enmType;
    /** Type specific data */
    union
    {
        /** Locked memory. */
        struct
        {
            /** Host context pointer. */
            R3PTRTYPE(void *)       pvHC;
            /** Host context ring-0 pointer. */
            RTR0PTR                 pvR0;
            /** Pointer to the locked mem record. */
            R3PTRTYPE(PMMLOCKEDMEM) pLockedMem;
        } Locked;

        /** Contiguous physical memory. */
        struct
        {
            /** Host context pointer. */
            R3PTRTYPE(void *)       pvHC;
            /** HC physical address corresponding to pvHC. */
            RTHCPHYS                HCPhys;
        } HCPhys;
        /** Contiguous guest physical memory. */
        struct
        {
            /** HC physical address corresponding to pvHC. */
            RTGCPHYS                GCPhys;
        } GCPhys;
    } u;
    /** Description. */
    R3PTRTYPE(const char *) pszDesc;
} MMLOOKUPHYPER;
/** Pointer to a hypervisor memory lookup record. */
typedef MMLOOKUPHYPER *PMMLOOKUPHYPER;


/**
 * Converts a MM pointer into a VM pointer.
 * @returns Pointer to the VM structure the MM is part of.
 * @param   pMM   Pointer to MM instance data.
 */
#define MM2VM(pMM)  ( (PVM)((char*)pMM - pMM->offVM) )


/**
 * MM Data (part of VM)
 */
typedef struct MM
{
    /** Offset to the VM structure.
     * See MM2VM(). */
    RTINT                       offVM;

    /** Set if PGM has been initialized and we can safely call PGMR3Map(). */
    bool                        fPGMInitialized;
#if GC_ARCH_BITS == 64 || HC_ARCH_BITS == 64
    uint32_t                    u32Padding1; /**< alignment padding. */
#endif

    /** Lookup list for the Hypervisor Memory Area.
     * The offset is relative to the start of the heap.
     * Use pHyperHeapHC or pHyperHeapGC to calculate the address.
     */
    RTUINT                      offLookupHyper;

    /** The offset of the next static mapping in the Hypervisor Memory Area. */
    RTUINT                      offHyperNextStatic;
    /** The size of the HMA.
     * Starts at 12MB and will be fixed late in the init process. */
    RTUINT                      cbHyperArea;

    /** Guest address of the Hypervisor Memory Area. */
    RTGCPTR                     pvHyperAreaGC;

    /** The hypervisor heap (GC Ptr). */
    GCPTRTYPE(PMMHYPERHEAP)     pHyperHeapGC;
    /** The hypervisor heap (HC Ptr). */
    R3R0PTRTYPE(PMMHYPERHEAP)   pHyperHeapHC;

    /** List of memory locks. (HC only) */
    R3PTRTYPE(PMMLOCKEDMEM)     pLockedMem;

    /** Page pool. (HC only) */
    R3R0PTRTYPE(PMMPAGEPOOL)    pPagePool;
    /** Page pool pages in low memory. (HC only) */
    R3R0PTRTYPE(PMMPAGEPOOL)    pPagePoolLow;

    /** Pointer to the dummy page.
     * The dummy page is a paranoia thingy used for instance for pure MMIO RAM ranges
     * to make sure any bugs will not harm whatever the system stores in the first
     * physical page. */
    R3PTRTYPE(void *)           pvDummyPage;
    /** Physical address of the dummy page. */
    RTHCPHYS                    HCPhysDummyPage;

    /** Size of the currently allocated guest RAM.
     * Mark that this is the actual size, not the end address. */
    RTUINT                      cbRAMSize;
    /** Size of the base RAM in bytes. */
    RTUINT                      cbRamBase;
    /** Pointer to the base RAM. */
    R3PTRTYPE(void *)           pvRamBaseHC;
    /** The head of the ROM ranges. */
    R3PTRTYPE(PMMROMRANGE)      pRomHead;

    /** Pointer to the MM R3 Heap. */
    R3PTRTYPE(PMMHEAP)          pHeap;

} MM;
/** Pointer to MM Data (part of VM). */
typedef MM *PMM;

__BEGIN_DECLS


int  mmR3PagePoolInit(PVM pVM);
void mmR3PagePoolTerm(PVM pVM);

int  mmR3HeapCreate(PVM pVM, PMMHEAP *ppHeap);
void mmR3HeapDestroy(PMMHEAP pHeap);

int  mmR3HyperInit(PVM pVM);
int  mmR3HyperInitPaging(PVM pVM);

int  mmR3LockMem(PVM pVM, void *pv, size_t cb, MMLOCKEDTYPE eType, PMMLOCKEDMEM *ppLockedMem, bool fSilentFailure);
int  mmR3MapLocked(PVM pVM, PMMLOCKEDMEM pLockedMem, RTGCPTR Addr, unsigned iPage, size_t cPages, unsigned fFlags);

const char *mmR3GetTagName(MMTAG enmTag);

void mmR3PhysRomReset(PVM pVM);

/**
 * Converts a pool address to a physical address.
 * The specified allocation type must match with the address.
 *
 * @returns Physical address.
 * @returns NIL_RTHCPHYS if not found or eType is not matching.
 * @param   pPool   Pointer to the page pool.
 * @param   pv      The address to convert.
 * @thread  The Emulation Thread.
 */
MMDECL(RTHCPHYS) mmPagePoolPtr2Phys(PMMPAGEPOOL pPool, void *pv);

/**
 * Converts a pool physical address to a linear address.
 * The specified allocation type must match with the address.
 *
 * @returns Physical address.
 * @returns NULL if not found or eType is not matching.
 * @param   pPool       Pointer to the page pool.
 * @param   HCPhys      The address to convert.
 * @thread  The Emulation Thread.
 */
MMDECL(void *) mmPagePoolPhys2Ptr(PMMPAGEPOOL pPool, RTHCPHYS HCPhys);

__END_DECLS

/** @} */

#endif
