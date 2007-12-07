/** @file
 * MM - The Memory Manager.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_mm_h
#define ___VBox_mm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>
#include <VBox/sup.h>


__BEGIN_DECLS

/** @defgroup grp_mm       The Memory Manager API
 * @{
 */

/** @name RAM Page Flags
 * Since internal ranges have a byte granularity it's possible for a
 * page be flagged for several uses. The access virtualization in PGM
 * will choose the most restricted one and use EM to emulate access to
 * the less restricted areas of the page.
 *
 * Bits 0-11 only since they are fitted into the offset part of a physical memory address.
 * @{
 */
/** Reserved - Not RAM, ROM nor MMIO2.
 * If this bit is cleared the memory is assumed to be some kind of RAM.
 * Normal MMIO may set it but that depends on whether the RAM range was
 * created specially for the MMIO or not.
 *
 * @remarks The current implementation will always reserve backing
 *          memory for reserved  ranges to simplify things.
 */
#define MM_RAM_FLAGS_RESERVED           RT_BIT(0)
/** ROM - Read Only Memory.
 * The page have a HC physical address which contains the BIOS code. All write
 * access is trapped and ignored.
 *
 * HACK: Writable shadow ROM is indicated by both ROM and MMIO2 being
 *       set. (We're out of bits.)
 */
#define MM_RAM_FLAGS_ROM                RT_BIT(1)
/** MMIO - Memory Mapped I/O.
 * All access is trapped and emulated. No physical backing is required, but
 * might for various reasons be present.
 */
#define MM_RAM_FLAGS_MMIO               RT_BIT(2)
/** MMIO2 - Memory Mapped I/O, variation 2.
 * The virtualization is performed using real memory and only catching
 * a few accesses for like keeping track for dirty pages.
 * @remark Involved in the shadow ROM hack.
 */
#define MM_RAM_FLAGS_MMIO2              RT_BIT(3)

/** PGM has virtual page access handler(s) defined for pages with this flag. */
#define MM_RAM_FLAGS_VIRTUAL_HANDLER    RT_BIT(4)
/** PGM has virtual page access handler(s) for write access. */
#define MM_RAM_FLAGS_VIRTUAL_WRITE      RT_BIT(5)
/** PGM has virtual page access handler(s) for all access. */
#define MM_RAM_FLAGS_VIRTUAL_ALL        RT_BIT(6)
/** PGM has physical page access handler(s) defined for pages with this flag. */
#define MM_RAM_FLAGS_PHYSICAL_HANDLER   RT_BIT(7)
/** PGM has physical page access handler(s) for write access. */
#define MM_RAM_FLAGS_PHYSICAL_WRITE     RT_BIT(8)
/** PGM has physical page access handler(s) for all access. */
#define MM_RAM_FLAGS_PHYSICAL_ALL       RT_BIT(9)
/** PGM has physical page access handler(s) for this page and has temporarily disabled it. */
#define MM_RAM_FLAGS_PHYSICAL_TEMP_OFF  RT_BIT(10)
#ifndef NEW_PHYS_CODE
/** Physical backing memory is allocated dynamically. Not set implies a one time static allocation. */
#define MM_RAM_FLAGS_DYNAMIC_ALLOC      RT_BIT(11)
#endif /* !NEW_PHYS_CODE */

/** The shift used to get the reference count. */
#define MM_RAM_FLAGS_CREFS_SHIFT        62
/** The mask applied to the the page pool idx after using MM_RAM_FLAGS_CREFS_SHIFT to shift it down. */
#define MM_RAM_FLAGS_CREFS_MASK         0x3
/** The (shifted) cRef value used to indiciate that the idx is the head of a
 * physical cross reference extent list. */
#define MM_RAM_FLAGS_CREFS_PHYSEXT      MM_RAM_FLAGS_CREFS_MASK
/** The shift used to get the page pool idx. (Apply MM_RAM_FLAGS_IDX_MASK to the result when shifting down). */
#define MM_RAM_FLAGS_IDX_SHIFT          48
/** The mask applied to the the page pool idx after using MM_RAM_FLAGS_IDX_SHIFT to shift it down. */
#define MM_RAM_FLAGS_IDX_MASK           0x3fff
/** The idx value when we're out of of extents or there are simply too many mappings of this page. */
#define MM_RAM_FLAGS_IDX_OVERFLOWED     MM_RAM_FLAGS_IDX_MASK

/** Mask for masking off any references to the page. */
#define MM_RAM_FLAGS_NO_REFS_MASK       UINT64_C(0x0000ffffffffffff)
/** @} */

/** @name MMR3PhysRegisterEx registration type
 * @{
 */
typedef enum
{
    /** Normal physical region (flags specify exact page type) */
    MM_PHYS_TYPE_NORMAL               = 0,
    /** Allocate part of a dynamically allocated physical region */
    MM_PHYS_TYPE_DYNALLOC_CHUNK,

    MM_PHYS_TYPE_32BIT_HACK = 0x7fffffff
} MMPHYSREG;
/** @} */

/**
 * Memory Allocation Tags.
 * For use with MMHyperAlloc(), MMR3HeapAlloc(), MMR3HeapAllocEx(),
 * MMR3HeapAllocZ() and MMR3HeapAllocZEx().
 *
 * @remark Don't forget to update the dump command in MMHeap.cpp!
 */
typedef enum MMTAG
{
    MM_TAG_INVALID = 0,

    MM_TAG_CFGM,
    MM_TAG_CFGM_BYTES,
    MM_TAG_CFGM_STRING,
    MM_TAG_CFGM_USER,

    MM_TAG_CSAM,
    MM_TAG_CSAM_PATCH,

    MM_TAG_DBGF,
    MM_TAG_DBGF_INFO,
    MM_TAG_DBGF_LINE,
    MM_TAG_DBGF_LINE_DUP,
    MM_TAG_DBGF_STACK,
    MM_TAG_DBGF_SYMBOL,
    MM_TAG_DBGF_SYMBOL_DUP,
    MM_TAG_DBGF_MODULE,

    MM_TAG_EM,

    MM_TAG_IOM,
    MM_TAG_IOM_STATS,

    MM_TAG_MM,
    MM_TAG_MM_LOOKUP_GUEST,
    MM_TAG_MM_LOOKUP_PHYS,
    MM_TAG_MM_LOOKUP_VIRT,
    MM_TAG_MM_PAGE,

    MM_TAG_PATM,
    MM_TAG_PATM_PATCH,

    MM_TAG_PDM,
    MM_TAG_PDM_ASYNC_COMPLETION,
    MM_TAG_PDM_DEVICE,
    MM_TAG_PDM_DEVICE_USER,
    MM_TAG_PDM_DRIVER,
    MM_TAG_PDM_DRIVER_USER,
    MM_TAG_PDM_USB,
    MM_TAG_PDM_USB_USER,
    MM_TAG_PDM_LUN,
    MM_TAG_PDM_QUEUE,
    MM_TAG_PDM_THREAD,

    MM_TAG_PGM,
    MM_TAG_PGM_CHUNK_MAPPING,
    MM_TAG_PGM_HANDLERS,
    MM_TAG_PGM_POOL,

    MM_TAG_REM,

    MM_TAG_SELM,

    MM_TAG_SSM,

    MM_TAG_STAM,

    MM_TAG_TM,

    MM_TAG_TRPM,

    MM_TAG_VM,
    MM_TAG_VM_REQ,

    MM_TAG_VMM,

    MM_TAG_HWACCM,

    MM_TAG_32BIT_HACK = 0x7fffffff
} MMTAG;




/** @defgroup grp_mm_hyper  Hypervisor Memory Management
 * @ingroup grp_mm
 * @{ */

/**
 * Converts a ring-0 host context address in the Hypervisor memory region to a ring-3 host context address.
 *
 * @returns ring-3 host context address.
 * @param   pVM         The VM to operate on.
 * @param   R0Ptr       The ring-0 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR3PTR) MMHyperR0ToR3(PVM pVM, RTR0PTR R0Ptr);

/**
 * Converts a ring-0 host context address in the Hypervisor memory region to a guest context address.
 *
 * @returns guest context address.
 * @param   pVM         The VM to operate on.
 * @param   R0Ptr       The ring-0 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTGCPTR) MMHyperR0ToGC(PVM pVM, RTR0PTR R0Ptr);

/**
 * Converts a ring-0 host context address in the Hypervisor memory region to a current context address.
 *
 * @returns current context address.
 * @param   pVM         The VM to operate on.
 * @param   R0Ptr       The ring-0 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING0
MMDECL(void *) MMHyperR0ToCC(PVM pVM, RTR0PTR R0Ptr);
#endif


/**
 * Converts a ring-3 host context address in the Hypervisor memory region to a ring-0 host context address.
 *
 * @returns ring-0 host context address.
 * @param   pVM         The VM to operate on.
 * @param   R3Ptr       The ring-3 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR0PTR) MMHyperR3ToR0(PVM pVM, RTR3PTR R3Ptr);

/**
 * Converts a ring-3 host context address in the Hypervisor memory region to a guest context address.
 *
 * @returns guest context address.
 * @param   pVM         The VM to operate on.
 * @param   R3Ptr       The ring-3 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTGCPTR) MMHyperR3ToGC(PVM pVM, RTR3PTR R3Ptr);

/**
 * Converts a ring-3 host context address in the Hypervisor memory region to a current context address.
 *
 * @returns current context address.
 * @param   pVM         The VM to operate on.
 * @param   R3Ptr       The ring-3 host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING3
MMDECL(void *) MMHyperR3ToCC(PVM pVM, RTR3PTR R3Ptr);
#else
DECLINLINE(void *) MMHyperR3ToCC(PVM pVM, RTR3PTR R3Ptr)
{
    NOREF(pVM);
    return R3Ptr;
}
#endif


/**
 * Converts a guest context address in the Hypervisor memory region to a ring-3 context address.
 *
 * @returns ring-3 host context address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR3PTR) MMHyperGCToR3(PVM pVM, RTGCPTR GCPtr);

/**
 * Converts a guest context address in the Hypervisor memory region to a ring-0 host context address.
 *
 * @returns ring-0 host context address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
MMDECL(RTR0PTR) MMHyperGCToR0(PVM pVM, RTGCPTR GCPtr);

/**
 * Converts a guest context address in the Hypervisor memory region to a current context address.
 *
 * @returns current context address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_GC
MMDECL(void *) MMHyperGCToCC(PVM pVM, RTGCPTR GCPtr);
#else
DECLINLINE(void *) MMHyperGCToCC(PVM pVM, RTGCPTR GCPtr)
{
    NOREF(pVM);
    return GCPtr;
}
#endif



/**
 * Converts a current context address in the Hypervisor memory region to a ring-3 host context address.
 *
 * @returns ring-3 host context address.
 * @param   pVM         The VM to operate on.
 * @param   pv          The current context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING3
MMDECL(RTR3PTR) MMHyperCCToR3(PVM pVM, void *pv);
#else
DECLINLINE(RTR3PTR) MMHyperCCToR3(PVM pVM, void *pv)
{
    NOREF(pVM);
    return pv;
}
#endif

/**
 * Converts a current context address in the Hypervisor memory region to a ring-0 host context address.
 *
 * @returns ring-0 host context address.
 * @param   pVM         The VM to operate on.
 * @param   pv          The current context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_RING0
MMDECL(RTR0PTR) MMHyperCCToR0(PVM pVM, void *pv);
#else
DECLINLINE(RTR0PTR) MMHyperCCToR0(PVM pVM, void *pv)
{
    NOREF(pVM);
    return pv;
}
#endif

/**
 * Converts a current context address in the Hypervisor memory region to a guest context address.
 *
 * @returns guest context address.
 * @param   pVM         The VM to operate on.
 * @param   pv          The current context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 */
#ifndef IN_GC
MMDECL(RTGCPTR) MMHyperCCToGC(PVM pVM, void *pv);
#else
DECLINLINE(RTGCPTR) MMHyperCCToGC(PVM pVM, void *pv)
{
    NOREF(pVM);
    return pv;
}
#endif



/**
 * Converts a current context address in the Hypervisor memory region to a HC address.
 * The memory must have been allocated with MMHyperAlloc().
 *
 * @returns HC address.
 * @param   pVM         The VM to operate on.
 * @param   Ptr         The current context address.
 * @thread  The Emulation Thread.
 * @deprecated
 */
#ifdef IN_GC
MMDECL(RTHCPTR) MMHyper2HC(PVM pVM, uintptr_t Ptr);
#else
DECLINLINE(RTHCPTR) MMHyper2HC(PVM pVM, uintptr_t Ptr)
{
    NOREF(pVM);
    return (RTHCPTR)Ptr;
}
#endif

/**
 * Converts a current context address in the Hypervisor memory region to a GC address.
 * The memory must have been allocated with MMHyperAlloc().
 *
 * @returns HC address.
 * @param   pVM         The VM to operate on.
 * @param   Ptr         The current context address.
 * @thread  The Emulation Thread.
 */
#ifndef IN_GC
MMDECL(RTGCPTR) MMHyper2GC(PVM pVM, uintptr_t Ptr);
#else
DECLINLINE(RTGCPTR) MMHyper2GC(PVM pVM, uintptr_t Ptr)
{
    NOREF(pVM);
    return (RTGCPTR)Ptr;
}
#endif

/**
 * Converts a HC address in the Hypervisor memory region to a GC address.
 * The memory must have been allocated with MMGCHyperAlloc() or MMR3HyperAlloc().
 *
 * @returns GC address.
 * @param   pVM         The VM to operate on.
 * @param   HCPtr       The host context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 * @deprecated
 */
MMDECL(RTGCPTR) MMHyperHC2GC(PVM pVM, RTHCPTR HCPtr);

/**
 * Converts a GC address in the Hypervisor memory region to a HC address.
 * The memory must have been allocated with MMGCHyperAlloc() or MMR3HyperAlloc().
 *
 * @returns HC address.
 * @param   pVM         The VM to operate on.
 * @param   GCPtr       The guest context address.
 *                      You'll be damned if this is not in the HMA! :-)
 * @thread  The Emulation Thread.
 * @deprecated
 */
MMDECL(RTHCPTR) MMHyperGC2HC(PVM pVM, RTGCPTR GCPtr);


/**
 * Allocates memory in the Hypervisor (GC VMM) area.
 * The returned memory is of course zeroed.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   cb          Number of bytes to allocate.
 * @param   uAlignment  Required memory alignment in bytes.
 *                      Values are 0,8,16,32 and PAGE_SIZE.
 *                      0 -> default alignment, i.e. 8 bytes.
 * @param   enmTag      The statistics tag.
 * @param   ppv         Where to store the address to the allocated
 *                      memory.
 * @remark  This is assumed not to be used at times when serialization is required.
 */
MMDECL(int) MMHyperAlloc(PVM pVM, size_t cb, uint32_t uAlignment, MMTAG enmTag, void **ppv);

/**
 * Free memory allocated using MMHyperAlloc().
 *
 * It's not possible to free memory which is page aligned!
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pv          The memory to free.
 * @remark  Try avoid freeing hyper memory.
 * @thread  The Emulation Thread.
 */
MMDECL(int) MMHyperFree(PVM pVM, void *pv);

#ifdef DEBUG
/**
 * Dumps the hypervisor heap to Log.
 * @param pVM       VM Handle.
 * @thread  The Emulation Thread.
 */
MMDECL(void) MMHyperHeapDump(PVM pVM);
#endif

/**
 * Query the amount of free memory in the hypervisor heap.
 *
 * @returns Number of free bytes in the hypervisor heap.
 * @thread  Any.
 */
MMDECL(size_t) MMHyperHeapGetFreeSize(PVM pVM);

/**
 * Query the size the hypervisor heap.
 *
 * @returns The size of the hypervisor heap in bytes.
 * @thread  Any.
 */
MMDECL(size_t) MMHyperHeapGetSize(PVM pVM);


/**
 * Query the address and size the hypervisor memory area.
 *
 * @returns Base address of the hypervisor area.
 * @param   pVM         VM Handle.
 * @param   pcb         Where to store the size of the hypervisor area. (out)
 * @thread  Any.
 */
MMDECL(RTGCPTR) MMHyperGetArea(PVM pVM, size_t *pcb);

/**
 * Checks if an address is within the hypervisor memory area.
 *
 * @returns true if inside.
 * @returns false if outside.
 * @param   pVM         VM handle.
 * @param   GCPtr       The pointer to check.
 * @thread  The Emulation Thread.
 */
MMDECL(bool) MMHyperIsInsideArea(PVM pVM, RTGCPTR GCPtr);

/**
 * Convert a page in the page pool to a HC physical address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns Physical address for the specified page table.
 * @param   pVM         VM handle.
 * @param   pvPage      Page which physical address we query.
 * @thread  The Emulation Thread.
 */
MMDECL(RTHCPHYS) MMPage2Phys(PVM pVM, void *pvPage);

/**
 * Convert physical address of a page to a HC virtual address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns Pointer to the page at that physical address.
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of a page.
 * @thread  The Emulation Thread.
 */
MMDECL(void *) MMPagePhys2Page(PVM pVM, RTHCPHYS HCPhysPage);


/**
 * Convert physical address of a page to a HC virtual address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of a page.
 * @param   ppvPage     Where to store the address corresponding to HCPhysPage.
 * @thread  The Emulation Thread.
 */
MMDECL(int) MMPagePhys2PageEx(PVM pVM, RTHCPHYS HCPhysPage, void **ppvPage);


/**
 * Try convert physical address of a page to a HC virtual address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of a page.
 * @param   ppvPage     Where to store the address corresponding to HCPhysPage.
 * @thread  The Emulation Thread.
 */
MMDECL(int) MMPagePhys2PageTry(PVM pVM, RTHCPHYS HCPhysPage, void **ppvPage);

/**
 * Convert GC physical address to HC virtual address.
 *
 * @returns HC virtual address.
 * @param   pVM         VM Handle
 * @param   GCPhys      Guest context physical address.
 * @param   cbRange     Physical range
 * @thread  The Emulation Thread.
 * @deprecated
 */
MMDECL(void *) MMPhysGCPhys2HCVirt(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange);


/** @def MMHYPER_GC_ASSERT_GCPTR
 * Asserts that an address is either NULL or inside the hypervisor memory area.
 * This assertion only works while IN_GC, it's a NOP everywhere else.
 * @thread  The Emulation Thread.
 */
#ifdef IN_GC
# define MMHYPER_GC_ASSERT_GCPTR(pVM, GCPtr)   Assert(MMHyperIsInsideArea((pVM), (GCPtr)) || !(GCPtr))
#else
# define MMHYPER_GC_ASSERT_GCPTR(pVM, GCPtr)   do { } while (0)
#endif

/** @} */


#ifdef IN_RING3
/** @defgroup grp_mm_r3    The MM Host Context Ring-3 API
 * @ingroup grp_mm
 * @{
 */

/**
 * Initialization of MM (save anything depending on PGM).
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3Init(PVM pVM);

/**
 * Initializes the MM parts which depends on PGM being initialized.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3InitPaging(PVM pVM);

/**
 * Finalizes the HMA mapping.
 *
 * This is called later during init, most (all) HMA allocations should be done
 * by the time this function is called.
 *
 * @returns VBox status.
 */
MMR3DECL(int) MMR3HyperInitFinalize(PVM pVM);

/**
 * Terminates the MM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3Term(PVM pVM);

/**
 * Reset notification.
 *
 * MM will reload shadow ROMs into RAM at this point and make
 * the ROM writable.
 *
 * @param   pVM             The VM handle.
 */
MMR3DECL(void) MMR3Reset(PVM pVM);

/**
 * Convert HC Physical address to HC Virtual address.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   HCPhys      The host context virtual address.
 * @param   ppv         Where to store the resulting address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys, void **ppv);

/**
 * Read memory from GC virtual address using the current guest CR3.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       Destination address (HC of course).
 * @param   GCPtr       GC virtual address.
 * @param   cb          Number of bytes to read.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3ReadGCVirt(PVM pVM, void *pvDst, RTGCPTR GCPtr, size_t cb);

/**
 * Write to memory at GC virtual address translated using the current guest CR3.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    GC virtual address.
 * @param   pvSrc       The source address (HC of course).
 * @param   cb          Number of bytes to read.
 */
MMR3DECL(int) MMR3WriteGCVirt(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);


/** @defgroup grp_mm_r3_hyper  Hypervisor Memory Manager (HC R3 Portion)
 * @ingroup grp_mm_r3
 * @{ */
/**
 * Allocates memory in the Hypervisor (GC VMM) area which never will
 * be freed and don't have any offset based relation to other heap blocks.
 *
 * The latter means that two blocks allocated by this API will not have the
 * same relative position to each other in GC and HC. In short, never use
 * this API for allocating nodes for an offset based AVL tree!
 *
 * The returned memory is of course zeroed.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   cb          Number of bytes to allocate.
 * @param   uAlignment  Required memory alignment in bytes.
 *                      Values are 0,8,16,32 and PAGE_SIZE.
 *                      0 -> default alignment, i.e. 8 bytes.
 * @param   enmTag      The statistics tag.
 * @param   ppv         Where to store the address to the allocated
 *                      memory.
 * @remark  This is assumed not to be used at times when serialization is required.
 */
MMDECL(int) MMR3HyperAllocOnceNoRel(PVM pVM, size_t cb, uint32_t uAlignment, MMTAG enmTag, void **ppv);

/**
 * Maps contiguous HC physical memory into the hypervisor region in the GC.
 *
 * @return VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   pvHC        Host context address of the memory. Must be page aligned!
 * @param   HCPhys      Host context physical address of the memory to be mapped. Must be page aligned!
 * @param   cb          Size of the memory. Will be rounded up to nearest page.
 * @param   pszDesc     Description.
 * @param   pGCPtr      Where to store the GC address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HyperMapHCPhys(PVM pVM, void *pvHC, RTHCPHYS HCPhys, size_t cb, const char *pszDesc, PRTGCPTR pGCPtr);

/**
 * Maps contiguous GC physical memory into the hypervisor region in the GC.
 *
 * @return VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   GCPhys      Guest context physical address of the memory to be mapped. Must be page aligned!
 * @param   cb          Size of the memory. Will be rounded up to nearest page.
 * @param   pszDesc     Mapping description.
 * @param   pGCPtr      Where to store the GC address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HyperMapGCPhys(PVM pVM, RTGCPHYS GCPhys, size_t cb, const char *pszDesc, PRTGCPTR pGCPtr);

/**
 * Locks and Maps HC virtual memory into the hypervisor region in the GC.
 *
 * @return VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   pvHC        Host context address of the memory (may be not page aligned).
 * @param   cb          Size of the memory. Will be rounded up to nearest page.
 * @param   fFree       Set this if MM is responsible for freeing the memory using SUPPageFree.
 * @param   pszDesc     Mapping description.
 * @param   pGCPtr      Where to store the GC address corresponding to pvHC.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HyperMapHCRam(PVM pVM, void *pvHC, size_t cb, bool fFree, const char *pszDesc, PRTGCPTR pGCPtr);

/**
 * Maps locked R3 virtual memory into the hypervisor region in the GC.
 *
 * @return VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   pvR3        The ring-3 address of the memory, must be page aligned.
 * @param   pvR0        The ring-0 address of the memory, must be page aligned. (optional)
 * @param   cPages      The number of pages.
 * @param   paPages     The page descriptors.
 * @param   pszDesc     Mapping description.
 * @param   pGCPtr      Where to store the GC address corresponding to pvHC.
 */
MMR3DECL(int) MMR3HyperMapPages(PVM pVM, void *pvR3, RTR0PTR pvR0, size_t cPages, PCSUPPAGE paPages, const char *pszDesc, PRTGCPTR pGCPtr);

/**
 * Reserves a hypervisor memory area.
 * Most frequent usage is fence pages and dynamically mappings like the guest PD and PDPTR.
 *
 * @return VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   cb          Size of the memory. Will be rounded up to nearest page.
 * @param   pszDesc     Mapping description.
 * @param   pGCPtr      Where to store the assigned GC address. Optional.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HyperReserve(PVM pVM, unsigned cb, const char *pszDesc, PRTGCPTR pGCPtr);


/**
 * Convert hypervisor HC virtual address to HC physical address.
 *
 * @returns HC physical address.
 * @param   pVM         VM Handle
 * @param   pvHC        Host context physical address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(RTHCPHYS) MMR3HyperHCVirt2HCPhys(PVM pVM, void *pvHC);
/**
 * Convert hypervisor HC virtual address to HC physical address.
 *
 * @returns HC physical address.
 * @param   pVM         VM Handle
 * @param   pvHC        Host context physical address.
 * @param   pHCPhys     Where to store the HC physical address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HyperHCVirt2HCPhysEx(PVM pVM, void *pvHC, PRTHCPHYS pHCPhys);
/**
 * Convert hypervisor HC physical address to HC virtual address.
 *
 * @returns HC virtual address.
 * @param   pVM         VM Handle
 * @param   HCPhys      Host context physical address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void *) MMR3HyperHCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys);
/**
 * Convert hypervisor HC physical address to HC virtual address.
 *
 * @returns VBox status.
 * @param   pVM         VM Handle
 * @param   HCPhys      Host context physical address.
 * @param   ppv         Where to store the HC virtual address.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int)   MMR3HyperHCPhys2HCVirtEx(PVM pVM, RTHCPHYS HCPhys, void **ppv);

/**
 * Read hypervisor memory from GC virtual address.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       Destination address (HC of course).
 * @param   GCPtr       GC virtual address.
 * @param   cb          Number of bytes to read.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3HyperReadGCVirt(PVM pVM, void *pvDst, RTGCPTR GCPtr, size_t cb);

/** @} */


/** @defgroup grp_mm_phys   Guest Physical Memory Manager
 * @ingroup grp_mm_r3
 * @{ */

/**
 * Register externally allocated RAM for the virtual machine.
 *
 * The memory registered with the VM thru this interface must not be freed
 * before the virtual machine has been destroyed. Bad things may happen... :-)
 *
 * @return VBox status code.
 * @param   pVM         VM handle.
 * @param   pvRam       Virtual address of the guest's physical memory range Must be page aligned.
 * @param   GCPhys      The physical address the ram shall be registered at.
 * @param   cb          Size of the memory. Must be page aligend.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 * @param   pszDesc     Description of the memory.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3PhysRegister(PVM pVM, void *pvRam, RTGCPHYS GCPhys, unsigned cb, unsigned fFlags, const char *pszDesc);

/**
 * Register externally allocated RAM for the virtual machine.
 *
 * The memory registered with the VM thru this interface must not be freed
 * before the virtual machine has been destroyed. Bad things may happen... :-)
 *
 * @return VBox status code.
 * @param   pVM         VM handle.
 * @param   pvRam       Virtual address of the guest's physical memory range Must be page aligned.
 * @param   GCPhys      The physical address the ram shall be registered at.
 * @param   cb          Size of the memory. Must be page aligend.
 * @param   fFlags      Flags of the MM_RAM_FLAGS_* defines.
 * @param   enmType     Physical range type (MM_PHYS_TYPE_*)
 * @param   pszDesc     Description of the memory.
 * @thread  The Emulation Thread.
 * @todo update this description.
 */
MMR3DECL(int) MMR3PhysRegisterEx(PVM pVM, void *pvRam, RTGCPHYS GCPhys, unsigned cb, unsigned fFlags, MMPHYSREG enmType, const char *pszDesc);

/**
 * Register previously registered externally allocated RAM for the virtual machine.
 *
 * Use this only for MMIO ranges or the guest will become very confused.
 * The memory registered with the VM thru this interface must not be freed
 * before the virtual machine has been destroyed. Bad things may happen... :-)
 *
 * @return VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPhysOld   The physical address the ram was registered at.
 * @param   GCPhysNew   The physical address the ram shall be registered at.
 * @param   cb          Size of the memory. Must be page aligend.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3PhysRelocate(PVM pVM, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, unsigned cb);

/**
 * Register a ROM (BIOS) region.
 *
 * It goes without saying that this is read-only memory. The memory region must be
 * in unassigned memory. I.e. from the top of the address space or on the PC in
 * the 0xa0000-0xfffff range.
 *
 * @returns VBox status.
 * @param   pVM                 VM Handle.
 * @param   pDevIns             The device instance owning the ROM region.
 * @param   GCPhys              First physical address in the range.
 *                              Must be page aligned!
 * @param   cbRange             The size of the range (in bytes).
 *                              Must be page aligned!
 * @param   pvBinary            Pointer to the binary data backing the ROM image.
 *                              This must be cbRange bytes big.
 *                              It will be copied and doesn't have to stick around.
 *                              It will be copied and doesn't have to stick around if fShadow is clear.
 * @param   fShadow             Whether to emulate ROM shadowing. This involves leaving
 *                              the ROM writable for a while during the POST and refreshing
 *                              it at reset. When this flag is set, the memory pointed to by
 *                              pvBinary has to stick around for the lifespan of the VM.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 * @remark  There is no way to remove the rom, automatically on device cleanup or
 *          manually from the device yet. At present I doubt we need such features...
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const void *pvBinary, bool fShadow, const char *pszDesc);

/**
 * Write-protects a shadow ROM range.
 *
 * This is called late in the POST for shadow ROM ranges.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   GCPhys      Start of the registered shadow ROM range
 * @param   cbRange     The length of the registered shadow ROM range.
 *                      This can be NULL (not sure about the BIOS interface yet).
 */
MMR3DECL(int) MMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange);

/**
 * Reserve physical address space for ROM and MMIO ranges.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys          Start physical address.
 * @param   cbRange         The size of the range.
 * @param   pszDesc         Description string.
 * @thread  The Emulation Thread.
 */
MMR3DECL(int) MMR3PhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc);

/**
 * Get the size of the base RAM.
 * This usually means the size of the first contigous block of physical memory.
 *
 * @returns
 * @param   pVM
 * @thread  Any.
 */
MMR3DECL(uint64_t) MMR3PhysGetRamSize(PVM pVM);


/** @} */


/** @defgroup grp_mm_page   Physical Page Pool
 * @ingroup grp_mm_r3
 * @{ */
/**
 * Allocates a page from the page pool.
 *
 * This function may returns pages which has physical addresses any
 * where. If you require a page to be within the first 4GB of physical
 * memory, use MMR3PageAllocLow().
 *
 * @returns Pointer to the allocated page page.
 * @returns NULL on failure.
 * @param   pVM         VM handle.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void *) MMR3PageAlloc(PVM pVM);

/**
 * Allocates a page from the page pool and return its physical address.
 *
 * This function may returns pages which has physical addresses any
 * where. If you require a page to be within the first 4GB of physical
 * memory, use MMR3PageAllocLow().
 *
 * @returns Pointer to the allocated page page.
 * @returns NIL_RTHCPHYS on failure.
 * @param   pVM         VM handle.
 * @thread  The Emulation Thread.
 */
MMR3DECL(RTHCPHYS) MMR3PageAllocPhys(PVM pVM);

/**
 * Frees a page allocated from the page pool by MMR3PageAlloc() and MMR3PageAllocPhys().
 *
 * @param   pVM         VM handle.
 * @param   pvPage      Pointer to the page.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void) MMR3PageFree(PVM pVM, void *pvPage);

/**
 * Allocates a page from the low page pool.
 *
 * @returns Pointer to the allocated page.
 * @returns NULL on failure.
 * @param   pVM         VM handle.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void *) MMR3PageAllocLow(PVM pVM);

/**
 * Frees a page allocated from the page pool by MMR3PageAllocLow().
 *
 * @param   pVM         VM handle.
 * @param   pvPage      Pointer to the page.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void) MMR3PageFreeLow(PVM pVM, void *pvPage);

/**
 * Free a page allocated from the page pool by physical address.
 * This works for pages allocated by MMR3PageAlloc(), MMR3PageAllocPhys()
 * and MMR3PageAllocLow().
 *
 * @param   pVM         VM handle.
 * @param   HCPhysPage  The physical address of the page to be freed.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void) MMR3PageFreeByPhys(PVM pVM, RTHCPHYS HCPhysPage);

/**
 * Gets the HC pointer to the dummy page.
 *
 * The dummy page is used as a place holder to prevent potential bugs
 * from doing really bad things to the system.
 *
 * @returns Pointer to the dummy page.
 * @param   pVM         VM handle.
 * @thread  The Emulation Thread.
 */
MMR3DECL(void *) MMR3PageDummyHCPtr(PVM pVM);

/**
 * Gets the HC Phys to the dummy page.
 *
 * The dummy page is used as a place holder to prevent potential bugs
 * from doing really bad things to the system.
 *
 * @returns Pointer to the dummy page.
 * @param   pVM         VM handle.
 * @thread  The Emulation Thread.
 */
MMR3DECL(RTHCPHYS) MMR3PageDummyHCPhys(PVM pVM);


#if 1 /* these are temporary wrappers and will be removed soon */
/**
 * Allocates a Page Table.
 *
 * @returns Pointer to page table.
 * @returns NULL on failure.
 * @param   pVM         VM handle.
 * @deprecated  Use MMR3PageAlloc().
 */
DECLINLINE(PVBOXPT) MMR3PTAlloc(PVM pVM)
{
    return (PVBOXPT)MMR3PageAlloc(pVM);
}

/**
 * Free a Page Table.
 *
 * @param   pVM         VM handle.
 * @param   pPT         Pointer to the page table as returned by MMR3PTAlloc().
 * @deprecated Use MMR3PageFree().
 */
DECLINLINE(void) MMR3PTFree(PVM pVM, PVBOXPT pPT)
{
    MMR3PageFree(pVM, pPT);
}

/**
 * Free a Page Table by physical address.
 *
 * @param   pVM         VM handle.
 * @param   HCPhysPT    The physical address of the page table to be freed.
 * @deprecated Use MMR3PageFreeByPhys().
 */
DECLINLINE(void) MMR3PTFreeByPhys(PVM pVM, RTHCPHYS HCPhysPT)
{
    MMR3PageFreeByPhys(pVM, HCPhysPT);
}

/**
 * Convert a Page Table address to a HC physical address.
 *
 * @returns Physical address for the specified page table.
 * @param   pVM         VM handle.
 * @param   pPT         Page table which physical address we query.
 * @deprecated Use MMR3Page2Phys().
 */
DECLINLINE(RTHCPHYS) MMR3PT2Phys(PVM pVM, PVBOXPT pPT)
{
    return MMPage2Phys(pVM, pPT);
}

/**
 * Convert a physical address to a page table address
 *
 * @returns Pointer to the page table at that physical address.
 * @param   pVM         VM handle.
 * @param   PhysPT      Page table which physical address we query.
 * @deprecated Use MMR3PagePhys2Page().
 */
DECLINLINE(PVBOXPT) MMR3Phys2PT(PVM pVM, RTHCPHYS PhysPT)
{
    return (PVBOXPT)MMPagePhys2Page(pVM, PhysPT);
}

/**
 * Allocate a Page Directory.
 *
 * @returns Pointer to the page directory.
 * @returns NULL on failure.
 * @param   pVM         VM handle.
 * @deprecated Use MMR3PageAlloc().
 */
DECLINLINE(PVBOXPD)  MMR3PDAlloc(PVM pVM)
{
    return (PVBOXPD)MMR3PageAlloc(pVM);
}

/**
 * Free a Page Directory.
 *
 * @param   pVM         VM handle.
 * @param   pPD         Pointer to the page directory allocated by MMR3PDAlloc().
 * @deprecated Use MMR3PageFree().
 */
DECLINLINE(void) MMR3PDFree(PVM pVM, PVBOXPD pPD)
{
    MMR3PageFree(pVM, pPD);
}

/**
 * Convert a Page Directory address to a physical address.
 *
 * @returns Physical address for the specified page directory.
 * @param   pVM         VM handle.
 * @param   pPD         Page directory which physical address we query.
 *                      Allocated by MMR3PDAlloc().
 * @deprecated Use MMR3Page2Phys().
 */
DECLINLINE(RTHCPHYS)  MMR3PD2Phys(PVM pVM, PVBOXPD pPD)
{
    return MMPage2Phys(pVM, pPD);
}

/**
 * Convert a physical address to a page directory address
 *
 * @returns Pointer to the page directory at that physical address.
 * @param   pVM         VM handle.
 * @param   PhysPD      Physical address of page directory.
 *                      Allocated by MMR3PDAlloc().
 * @deprecated Use MMR3PageAlloc().
 */
DECLINLINE(PVBOXPD) MMR3Phys2PD(PVM pVM, RTHCPHYS PhysPD)
{
    return (PVBOXPD)MMPagePhys2Page(pVM, PhysPD);
}

/** @deprecated */
DECLINLINE(void *) MMR3DummyPageHCPtr(PVM pVM) { return MMR3PageDummyHCPtr(pVM); }
/** @deprecated */
DECLINLINE(RTHCPHYS) MMR3DummyPageHCPhys(PVM pVM) { return MMR3PageDummyHCPhys(pVM); }

#endif /* will be removed */

/** @} */


/** @defgroup grp_mm_heap   Heap Manager
 * @ingroup grp_mm_r3
 * @{ */

/**
 * Allocate memory associating it with the VM for collective cleanup.
 *
 * The memory will be allocated from the default heap but a header
 * is added in which we keep track of which VM it belongs to and chain
 * all the allocations together so they can be freed in a one go.
 *
 * This interface is typically used for memory block which will not be
 * freed during the life of the VM.
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @thread  Any thread.
 */
MMR3DECL(void *) MMR3HeapAlloc(PVM pVM, MMTAG enmTag, size_t cbSize);

/**
 * Same as MMR3HeapAlloc().
 *
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   ppv         Where to store the pointer to the allocated memory on success.
 * @thread  Any thread.
 */
MMR3DECL(int) MMR3HeapAllocEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv);

/**
 * Same as MMR3HeapAlloc() only the memory is zeroed.
 *
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @thread  Any thread.
 */
MMR3DECL(void *) MMR3HeapAllocZ(PVM pVM, MMTAG enmTag, size_t cbSize);

/**
 * Same as MMR3HeapAllocZ().
 *
 *
 * @returns Pointer to allocated memory.
 * @param   pVM         VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   cbSize      Size of the block.
 * @param   ppv         Where to store the pointer to the allocated memory on success.
 * @thread  Any thread.
 */
MMR3DECL(int) MMR3HeapAllocZEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv);

/**
 * Reallocate memory allocated with MMR3HeapAlloc() or MMR3HeapRealloc().
 *
 * @returns Pointer to reallocated memory.
 * @param   pv          Pointer to the memory block to reallocate.
 *                      Must not be NULL!
 * @param   cbNewSize   New block size.
 * @thread  Any thread.
 */
MMR3DECL(void *) MMR3HeapRealloc(void *pv, size_t cbNewSize);

/**
 * Duplicates the specified string.
 *
 * @returns Pointer to the duplicate.
 * @returns NULL on failure or when input NULL.
 * @param   pVM         The VM handle.
 * @param   enmTag      Statistics tag. Statistics are collected on a per tag
 *                      basis in addition to a global one. Thus we can easily
 *                      identify how memory is used by the VM.
 * @param   psz         The string to duplicate. NULL is allowed.
 */
MMR3DECL(char *) MMR3HeapStrDup(PVM pVM, MMTAG enmTag, const char *psz);

/**
 * Releases memory allocated with MMR3HeapAlloc() or MMR3HeapRealloc().
 *
 * @param   pv          Pointer to the memory block to free.
 * @thread  Any thread.
 */
MMR3DECL(void) MMR3HeapFree(void *pv);

/** @} */

/** @} */
#endif



#ifdef IN_GC
/** @defgroup grp_mm_gc    The MM Guest Context API
 * @ingroup grp_mm
 * @{
 */

/**
 * Install MMGCRam Hypervisor page fault handler for normal working
 * of MMGCRamRead and MMGCRamWrite calls.
 * This handler will be automatically removed at page fault.
 * In other case it must be removed by MMGCRamDeregisterTrapHandler call.
 *
 * @param   pVM         VM handle.
 */
MMGCDECL(void) MMGCRamRegisterTrapHandler(PVM pVM);

/**
 * Remove MMGCRam Hypervisor page fault handler.
 * See description of MMGCRamRegisterTrapHandler call.
 *
 * @param   pVM         VM handle.
 */
MMGCDECL(void) MMGCRamDeregisterTrapHandler(PVM pVM);

/**
 * Read data in guest context with \#PF control.
 * MMRamGC page fault handler must be installed prior this call for safe operation.
 * Use MMGCRamRegisterTrapHandler() call for this task.
 *
 * @returns VBox status.
 * @param   pDst        Where to store the readed data.
 * @param   pSrc        Pointer to the data to read.
 * @param   cb          Size of data to read, only 1/2/4/8 is valid.
 */
MMGCDECL(int) MMGCRamReadNoTrapHandler(void *pDst, void *pSrc, size_t cb);

/**
 * Write data in guest context with \#PF control.
 * MMRamGC page fault handler must be installed prior this call for safe operation.
 * Use MMGCRamRegisterTrapHandler() call for this task.
 *
 * @returns VBox status.
 * @param   pDst        Where to write the data.
 * @param   pSrc        Pointer to the data to write.
 * @param   cb          Size of data to write, only 1/2/4 is valid.
 */
MMGCDECL(int) MMGCRamWriteNoTrapHandler(void *pDst, void *pSrc, size_t cb);

/**
 * Read data in guest context with \#PF control.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pDst        Where to store the readed data.
 * @param   pSrc        Pointer to the data to read.
 * @param   cb          Size of data to read, only 1/2/4/8 is valid.
 */
MMGCDECL(int) MMGCRamRead(PVM pVM, void *pDst, void *pSrc, size_t cb);

/**
 * Write data in guest context with \#PF control.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   pDst        Where to write the data.
 * @param   pSrc        Pointer to the data to write.
 * @param   cb          Size of data to write, only 1/2/4 is valid.
 */
MMGCDECL(int) MMGCRamWrite(PVM pVM, void *pDst, void *pSrc, size_t cb);

/** @} */
#endif

/** @} */
__END_DECLS


#endif

