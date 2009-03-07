/** @file
 * MM - The Memory Manager.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_mm_h
#define ___VBox_mm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/x86.h>
#include <iprt/stdarg.h>
#include <VBox/sup.h>


__BEGIN_DECLS

/** @defgroup grp_mm       The Memory Manager API
 * @{
 */

#ifndef VBOX_WITH_NEW_PHYS_CODE
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

/** Physical backing memory is allocated dynamically. Not set implies a one time static allocation. */
#define MM_RAM_FLAGS_DYNAMIC_ALLOC      RT_BIT(11)
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
#endif /* !VBOX_WITH_NEW_PHYS_CODE */

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

    MM_TAG_CPUM_CTX,

    MM_TAG_DBGF,
    MM_TAG_DBGF_INFO,
    MM_TAG_DBGF_LINE,
    MM_TAG_DBGF_LINE_DUP,
    MM_TAG_DBGF_MODULE,
    MM_TAG_DBGF_OS,
    MM_TAG_DBGF_STACK,
    MM_TAG_DBGF_SYMBOL,
    MM_TAG_DBGF_SYMBOL_DUP,

    MM_TAG_EM,

    MM_TAG_IOM,
    MM_TAG_IOM_STATS,

    MM_TAG_MM,
    MM_TAG_MM_LOOKUP_GUEST,
    MM_TAG_MM_LOOKUP_PHYS,
    MM_TAG_MM_LOOKUP_VIRT,
    MM_TAG_MM_PAGE,

    MM_TAG_PARAV,

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
    MM_TAG_PGM_PHYS,
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

VMMDECL(RTR3PTR)    MMHyperR0ToR3(PVM pVM, RTR0PTR R0Ptr);
VMMDECL(RTRCPTR)    MMHyperR0ToRC(PVM pVM, RTR0PTR R0Ptr);
#ifndef IN_RING0
VMMDECL(void *)     MMHyperR0ToCC(PVM pVM, RTR0PTR R0Ptr);
#endif
VMMDECL(RTR0PTR)    MMHyperR3ToR0(PVM pVM, RTR3PTR R3Ptr);
VMMDECL(RTRCPTR)    MMHyperR3ToRC(PVM pVM, RTR3PTR R3Ptr);
VMMDECL(RTR3PTR)    MMHyperRCToR3(PVM pVM, RTRCPTR RCPtr);
VMMDECL(RTR0PTR)    MMHyperRCToR0(PVM pVM, RTRCPTR RCPtr);

#ifndef IN_RING3
VMMDECL(void *)     MMHyperR3ToCC(PVM pVM, RTR3PTR R3Ptr);
#else
DECLINLINE(void *)  MMHyperR3ToCC(PVM pVM, RTR3PTR R3Ptr)
{
    NOREF(pVM);
    return R3Ptr;
}
#endif


#ifndef IN_RC
VMMDECL(void *)     MMHyperRCToCC(PVM pVM, RTRCPTR RCPtr);
#else
DECLINLINE(void *)  MMHyperRCToCC(PVM pVM, RTRCPTR RCPtr)
{
    NOREF(pVM);
    return (void *)RCPtr;
}
#endif

#ifndef IN_RING3
VMMDECL(RTR3PTR)    MMHyperCCToR3(PVM pVM, void *pv);
#else
DECLINLINE(RTR3PTR) MMHyperCCToR3(PVM pVM, void *pv)
{
    NOREF(pVM);
    return pv;
}
#endif

#ifndef IN_RING0
VMMDECL(RTR0PTR)    MMHyperCCToR0(PVM pVM, void *pv);
#else
DECLINLINE(RTR0PTR) MMHyperCCToR0(PVM pVM, void *pv)
{
    NOREF(pVM);
    return pv;
}
#endif

#ifndef IN_RC
VMMDECL(RTRCPTR)    MMHyperCCToRC(PVM pVM, void *pv);
#else
DECLINLINE(RTRCPTR) MMHyperCCToRC(PVM pVM, void *pv)
{
    NOREF(pVM);
    return (RTRCPTR)pv;
}
#endif


VMMDECL(int)        MMHyperAlloc(PVM pVM, size_t cb, uint32_t uAlignment, MMTAG enmTag, void **ppv);
VMMDECL(int)        MMHyperFree(PVM pVM, void *pv);
VMMDECL(void)       MMHyperHeapCheck(PVM pVM);
#ifdef DEBUG
VMMDECL(void)       MMHyperHeapDump(PVM pVM);
#endif
VMMDECL(size_t)     MMHyperHeapGetFreeSize(PVM pVM);
VMMDECL(size_t)     MMHyperHeapGetSize(PVM pVM);
VMMDECL(RTGCPTR)    MMHyperGetArea(PVM pVM, size_t *pcb);
VMMDECL(bool)       MMHyperIsInsideArea(PVM pVM, RTGCPTR GCPtr);


VMMDECL(RTHCPHYS)   MMPage2Phys(PVM pVM, void *pvPage);
VMMDECL(void *)     MMPagePhys2Page(PVM pVM, RTHCPHYS HCPhysPage);
VMMDECL(int)        MMPagePhys2PageEx(PVM pVM, RTHCPHYS HCPhysPage, void **ppvPage);
VMMDECL(int)        MMPagePhys2PageTry(PVM pVM, RTHCPHYS HCPhysPage, void **ppvPage);


/** @def MMHYPER_RC_ASSERT_RCPTR
 * Asserts that an address is either NULL or inside the hypervisor memory area.
 * This assertion only works while IN_RC, it's a NOP everywhere else.
 * @thread  The Emulation Thread.
 */
#ifdef IN_RC
# define MMHYPER_RC_ASSERT_RCPTR(pVM, RCPtr)   Assert(MMHyperIsInsideArea((pVM), (RTRCUINTPTR)(RCPtr)) || !(RCPtr))
#else
# define MMHYPER_RC_ASSERT_RCPTR(pVM, RCPtr)   do { } while (0)
#endif

/** @} */


#ifdef IN_RING3
/** @defgroup grp_mm_r3    The MM Host Context Ring-3 API
 * @ingroup grp_mm
 * @{
 */

VMMR3DECL(int)      MMR3InitUVM(PUVM pUVM);
VMMR3DECL(int)      MMR3Init(PVM pVM);
VMMR3DECL(int)      MMR3InitPaging(PVM pVM);
VMMR3DECL(int)      MMR3HyperInitFinalize(PVM pVM);
VMMR3DECL(int)      MMR3Term(PVM pVM);
VMMR3DECL(void)     MMR3TermUVM(PUVM pUVM);
VMMR3DECL(void)     MMR3Reset(PVM pVM);
VMMR3DECL(int)      MMR3ReserveHandyPages(PVM pVM, uint32_t cHandyPages);
VMMR3DECL(int)      MMR3IncreaseBaseReservation(PVM pVM, uint64_t cAddBasePages);
VMMR3DECL(int)      MMR3AdjustFixedReservation(PVM pVM, int32_t cDeltaFixedPages, const char *pszDesc);
VMMR3DECL(int)      MMR3UpdateShadowReservation(PVM pVM, uint32_t cShadowPages);

VMMR3DECL(int)      MMR3HCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys, void **ppv);
VMMR3DECL(int)      MMR3ReadGCVirt(PVM pVM, void *pvDst, RTGCPTR GCPtr, size_t cb);
VMMR3DECL(int)      MMR3WriteGCVirt(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);


/** @defgroup grp_mm_r3_hyper  Hypervisor Memory Manager (HC R3 Portion)
 * @ingroup grp_mm_r3
 * @{ */
VMMDECL(int)        MMR3HyperAllocOnceNoRel(PVM pVM, size_t cb, uint32_t uAlignment, MMTAG enmTag, void **ppv);
VMMR3DECL(int)      MMR3HyperMapHCPhys(PVM pVM, void *pvR3, RTR0PTR pvR0, RTHCPHYS HCPhys, size_t cb, const char *pszDesc, PRTGCPTR pGCPtr);
VMMR3DECL(int)      MMR3HyperMapGCPhys(PVM pVM, RTGCPHYS GCPhys, size_t cb, const char *pszDesc, PRTGCPTR pGCPtr);
VMMR3DECL(int)      MMR3HyperMapMMIO2(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb, const char *pszDesc, PRTRCPTR pRCPtr);
VMMR3DECL(int)      MMR3HyperMapPages(PVM pVM, void *pvR3, RTR0PTR pvR0, size_t cPages, PCSUPPAGE paPages, const char *pszDesc, PRTGCPTR pGCPtr);
VMMR3DECL(int)      MMR3HyperReserve(PVM pVM, unsigned cb, const char *pszDesc, PRTGCPTR pGCPtr);
VMMR3DECL(RTHCPHYS) MMR3HyperHCVirt2HCPhys(PVM pVM, void *pvHC);
VMMR3DECL(int)      MMR3HyperHCVirt2HCPhysEx(PVM pVM, void *pvHC, PRTHCPHYS pHCPhys);
VMMR3DECL(void *)   MMR3HyperHCPhys2HCVirt(PVM pVM, RTHCPHYS HCPhys);
VMMR3DECL(int)      MMR3HyperHCPhys2HCVirtEx(PVM pVM, RTHCPHYS HCPhys, void **ppv);
VMMR3DECL(int)      MMR3HyperReadGCVirt(PVM pVM, void *pvDst, RTGCPTR GCPtr, size_t cb);
/** @} */


/** @defgroup grp_mm_phys   Guest Physical Memory Manager
 * @ingroup grp_mm_r3
 * @{ */
#ifndef VBOX_WITH_NEW_PHYS_CODE
VMMR3DECL(int)      MMR3PhysRegisterEx(PVM pVM, void *pvRam, RTGCPHYS GCPhys, unsigned cb, unsigned fFlags, MMPHYSREG enmType, const char *pszDesc);
VMMR3DECL(int)      MMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTUINT cbRange, const void *pvBinary, bool fShadow, const char *pszDesc);
VMMR3DECL(int)      MMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange);
VMMR3DECL(int)      MMR3PhysReserve(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, const char *pszDesc);
#endif
VMMR3DECL(uint64_t) MMR3PhysGetRamSize(PVM pVM);
/** @} */


/** @defgroup grp_mm_page   Physical Page Pool
 * @ingroup grp_mm_r3
 * @{ */
VMMR3DECL(void *)   MMR3PageAlloc(PVM pVM);
VMMR3DECL(RTHCPHYS) MMR3PageAllocPhys(PVM pVM);
VMMR3DECL(void)     MMR3PageFree(PVM pVM, void *pvPage);
VMMR3DECL(void *)   MMR3PageAllocLow(PVM pVM);
VMMR3DECL(void)     MMR3PageFreeLow(PVM pVM, void *pvPage);
VMMR3DECL(void)     MMR3PageFreeByPhys(PVM pVM, RTHCPHYS HCPhysPage);
VMMR3DECL(void *)   MMR3PageDummyHCPtr(PVM pVM);
VMMR3DECL(RTHCPHYS) MMR3PageDummyHCPhys(PVM pVM);
/** @} */


/** @defgroup grp_mm_heap   Heap Manager
 * @ingroup grp_mm_r3
 * @{ */
VMMR3DECL(void *)   MMR3HeapAlloc(PVM pVM, MMTAG enmTag, size_t cbSize);
VMMR3DECL(void *)   MMR3HeapAllocU(PUVM pUVM, MMTAG enmTag, size_t cbSize);
VMMR3DECL(int)      MMR3HeapAllocEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv);
VMMR3DECL(int)      MMR3HeapAllocExU(PUVM pUVM, MMTAG enmTag, size_t cbSize, void **ppv);
VMMR3DECL(void *)   MMR3HeapAllocZ(PVM pVM, MMTAG enmTag, size_t cbSize);
VMMR3DECL(void *)   MMR3HeapAllocZU(PUVM pUVM, MMTAG enmTag, size_t cbSize);
VMMR3DECL(int)      MMR3HeapAllocZEx(PVM pVM, MMTAG enmTag, size_t cbSize, void **ppv);
VMMR3DECL(int)      MMR3HeapAllocZExU(PUVM pUVM, MMTAG enmTag, size_t cbSize, void **ppv);
VMMR3DECL(void *)   MMR3HeapRealloc(void *pv, size_t cbNewSize);
VMMR3DECL(char *)   MMR3HeapStrDup(PVM pVM, MMTAG enmTag, const char *psz);
VMMR3DECL(char *)   MMR3HeapStrDupU(PUVM pUVM, MMTAG enmTag, const char *psz);
VMMR3DECL(char *)   MMR3HeapAPrintf(PVM pVM, MMTAG enmTag, const char *pszFormat, ...);
VMMR3DECL(char *)   MMR3HeapAPrintfU(PUVM pUVM, MMTAG enmTag, const char *pszFormat, ...);
VMMR3DECL(char *)   MMR3HeapAPrintfV(PVM pVM, MMTAG enmTag, const char *pszFormat, va_list va);
VMMR3DECL(char *)   MMR3HeapAPrintfVU(PUVM pUVM, MMTAG enmTag, const char *pszFormat, va_list va);
VMMR3DECL(void)     MMR3HeapFree(void *pv);
/** @} */

/** @} */
#endif /* IN_RING3 */



#ifdef IN_RC
/** @defgroup grp_mm_gc    The MM Guest Context API
 * @ingroup grp_mm
 * @{
 */

VMMRCDECL(void)     MMGCRamRegisterTrapHandler(PVM pVM);
VMMRCDECL(void)     MMGCRamDeregisterTrapHandler(PVM pVM);
VMMRCDECL(int)      MMGCRamReadNoTrapHandler(void *pDst, void *pSrc, size_t cb);
VMMRCDECL(int)      MMGCRamWriteNoTrapHandler(void *pDst, void *pSrc, size_t cb);
VMMRCDECL(int)      MMGCRamRead(PVM pVM, void *pDst, void *pSrc, size_t cb);
VMMRCDECL(int)      MMGCRamWrite(PVM pVM, void *pDst, void *pSrc, size_t cb);

/** @} */
#endif /* IN_RC */

/** @} */
__END_DECLS


#endif

