/** @file
 * PGM - Page Monitor/Monitor.
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

#ifndef ___VBox_pgm_h
#define ___VBox_pgm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/cpum.h>
#include <VBox/vmapi.h>

__BEGIN_DECLS

/** @defgroup grp_pgm   The Page Monitor/Manager API
 * @{
 */

/** Chunk size for dynamically allocated physical memory. */
#define PGM_DYNAMIC_CHUNK_SIZE          (1*1024*1024)
/** Shift GC physical address by 20 bits to get the offset into the pvHCChunkHC array. */
#define PGM_DYNAMIC_CHUNK_SHIFT         20
/** Dynamic chunk offset mask. */
#define PGM_DYNAMIC_CHUNK_OFFSET_MASK   0xfffff
/** Dynamic chunk base mask. */
#define PGM_DYNAMIC_CHUNK_BASE_MASK     (~(RTGCPHYS)PGM_DYNAMIC_CHUNK_OFFSET_MASK)


/** Page flags used for PGMHyperSetPageFlags
 * @deprecated
 * @{ */
#define PGMPAGE_READ                1
#define PGMPAGE_WRITE               2
#define PGMPAGE_USER                4
#define PGMPAGE_SYSTEM              8
#define PGMPAGE_NOTPRESENT          16
/** @} */


/**
 * FNPGMRELOCATE callback mode.
 */
typedef enum PGMRELOCATECALL
{
    /** The callback is for checking if the suggested address is suitable. */
    PGMRELOCATECALL_SUGGEST = 1,
    /** The callback is for executing the relocation. */
    PGMRELOCATECALL_RELOCATE
} PGMRELOCATECALL;


/**
 * Callback function which will be called when PGM is trying to find
 * a new location for the mapping.
 *
 * The callback is called in two modes, 1) the check mode and 2) the relocate mode.
 * In 1) the callback should say if it objects to a suggested new location. If it
 * accepts the new location, it is called again for doing it's relocation.
 *
 *
 * @returns true if the location is ok.
 * @returns false if another location should be found.
 * @param   GCPtrOld    The old virtual address.
 * @param   GCPtrNew    The new virtual address.
 * @param   enmMode     Used to indicate the callback mode.
 * @param   pvUser      User argument.
 * @remark  The return value is no a failure indicator, it's an acceptance
 *          indicator. Relocation can not fail!
 */
typedef DECLCALLBACK(bool) FNPGMRELOCATE(PVM pVM, RTGCPTR GCPtrOld, RTGCPTR GCPtrNew, PGMRELOCATECALL enmMode, void *pvUser);
/** Pointer to a relocation callback function. */
typedef FNPGMRELOCATE *PFNPGMRELOCATE;


/**
 * Physical page access handler type.
 */
typedef enum PGMPHYSHANDLERTYPE
{
    /** MMIO range. Pages are not present, all access is done in interpreter or recompiler. */
    PGMPHYSHANDLERTYPE_MMIO = 1,
    /** Handler all write access to a physical page range. */
    PGMPHYSHANDLERTYPE_PHYSICAL_WRITE,
    /** Handler all access to a physical page range. */
    PGMPHYSHANDLERTYPE_PHYSICAL_ALL

} PGMPHYSHANDLERTYPE;

/**
 * \#PF Handler callback for physical access handler ranges (MMIO among others) in GC.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 *                      NULL on DMA and other non CPU access.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(int) FNPGMGCPHYSHANDLER(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
/** Pointer to PGM access callback. */
typedef FNPGMGCPHYSHANDLER *PFNPGMGCPHYSHANDLER;

/**
 * \#PF Handler callback for physical access handler ranges (MMIO among others) in R0.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 *                      NULL on DMA and other non CPU access.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(int) FNPGMR0PHYSHANDLER(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
/** Pointer to PGM access callback. */
typedef FNPGMR0PHYSHANDLER *PFNPGMR0PHYSHANDLER;

/**
 * Guest Access type
 */
typedef enum PGMACCESSTYPE
{
    /** Read access. */
    PGMACCESSTYPE_READ = 1,
    /** Write access. */
    PGMACCESSTYPE_WRITE
} PGMACCESSTYPE;

/**
 * \#PF Handler callback for physical access handler ranges (MMIO among others) in HC.
 *
 * The handler can not raise any faults, it's mainly for monitoring write access
 * to certain pages.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNPGMR3PHYSHANDLER(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
/** Pointer to PGM access callback. */
typedef FNPGMR3PHYSHANDLER *PFNPGMR3PHYSHANDLER;


/**
 * Virtual access handler type.
 */
typedef enum PGMVIRTHANDLERTYPE
{
    /** Natural traps only. */
    PGMVIRTHANDLERTYPE_NORMAL = 1,
    /** Write access handled. */
    PGMVIRTHANDLERTYPE_WRITE,
    /** All access handled. */
    PGMVIRTHANDLERTYPE_ALL,
    /** By eip - Natural traps only. */
    PGMVIRTHANDLERTYPE_EIP,
    /** Hypervisor write access handled.
     * This is used to catch the guest trying to write to LDT, TSS and any other
     * system structure which the brain dead intel guys let unprivilegde code find. */
    PGMVIRTHANDLERTYPE_HYPERVISOR

} PGMVIRTHANDLERTYPE;

/**
 * \#PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
typedef DECLCALLBACK(int) FNPGMGCVIRTHANDLER(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange);
/** Pointer to PGM access callback. */
typedef FNPGMGCVIRTHANDLER *PFNPGMGCVIRTHANDLER;

/**
 * \#PF Handler callback for virtual access handler ranges.
 *
 * Important to realize that a physical page in a range can have aliases, and
 * for ALL and WRITE handlers these will also trigger.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest is writing to. (not correct if it's an alias!)
 * @param   pvPtr           The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
typedef DECLCALLBACK(int) FNPGMHCVIRTHANDLER(PVM pVM, RTGCPTR GCPtr, void *pvPtr, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
/** Pointer to PGM access callback. */
typedef FNPGMHCVIRTHANDLER *PFNPGMHCVIRTHANDLER;


/**
 * \#PF Handler callback for invalidation of virtual access handler ranges.
 *
 * @param   pVM             VM Handle.
 * @param   GCPtr           The virtual address the guest has changed.
 */
typedef DECLCALLBACK(int) FNPGMHCVIRTINVALIDATE(PVM pVM, RTGCPTR GCPtr);
/** Pointer to PGM invalidation callback. */
typedef FNPGMHCVIRTINVALIDATE *PFNPGMHCVIRTINVALIDATE;

/**
 * Paging mode.
 */
typedef enum PGMMODE
{
    /** The usual invalid value. */
    PGMMODE_INVALID = 0,
    /** Real mode. */
    PGMMODE_REAL,
    /** Protected mode, no paging. */
    PGMMODE_PROTECTED,
    /** 32-bit paging. */
    PGMMODE_32_BIT,
    /** PAE paging. */
    PGMMODE_PAE,
    /** PAE paging with NX enabled. */
    PGMMODE_PAE_NX,
    /** 64-bit AMD paging (long mode). */
    PGMMODE_AMD64,
    /** 64-bit AMD paging (long mode) with NX enabled. */
    PGMMODE_AMD64_NX,
    /** The max number of modes */
    PGMMODE_MAX,
    /** 32bit hackishness. */
    PGMMODE_32BIT_HACK = 0x7fffffff
} PGMMODE;

/**
 * The current ROM page protection.
 */
typedef enum PGMROMPROT
{
    /** The customary invalid value. */
    PGMROMPROT_INVALID = 0,
    /** Read from the virgin ROM page, ignore writes.
     * Map the virgin page, use write access handler to ignore writes. */
    PGMROMPROT_READ_ROM_WRITE_IGNORE,
    /** Read from the virgin ROM page, write to the shadow RAM.
     * Map the virgin page, use write access handler change the RAM. */
    PGMROMPROT_READ_ROM_WRITE_RAM,
    /** Read from the shadow ROM page, ignore writes.
     * Map the shadow page read-only, use write access handler to ignore writes. */
    PGMROMPROT_READ_RAM_WRITE_IGNORE,
    /** Read from the shadow ROM page, ignore writes.
     * Map the shadow page read-write, disabled write access handler. */
    PGMROMPROT_READ_RAM_WRITE_RAM,
    /** The end of valid values. */
    PGMROMPROT_END,
    /** The usual 32-bit type size hack. */
    PGMROMPROT_32BIT_HACK = 0x7fffffff
} PGMROMPROT;

/**
 * Is the ROM mapped (true) or is the shadow RAM mapped (false).
 *
 * @returns boolean.
 * @param   enmProt     The PGMROMPROT value, must be valid.
 */
#define PGMROMPROT_IS_ROM(enmProt) \
    (    (enmProt) == PGMROMPROT_READ_ROM_WRITE_IGNORE \
      || (enmProt) == PGMROMPROT_READ_ROM_WRITE_RAM )


PGMDECL(uint32_t) PGMGetHyperCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetHyper32BitCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetHyperPaeCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetHyperAmd64CR3(PVM pVM);
PGMDECL(uint32_t) PGMGetInterHCCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetInterGCCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetInter32BitCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetInterPaeCR3(PVM pVM);
PGMDECL(uint32_t) PGMGetInterAmd64CR3(PVM pVM);
PGMDECL(int)    PGMTrap0eHandler(PVM pVM, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
PGMDECL(int)    PGMPrefetchPage(PVM pVM, RTGCPTR GCPtrPage);
PGMDECL(int)    PGMVerifyAccess(PVM pVM, RTGCUINTPTR Addr, uint32_t cbSize, uint32_t fAccess);
PGMDECL(int)    PGMIsValidAccess(PVM pVM, RTGCUINTPTR Addr, uint32_t cbSize, uint32_t fAccess);
PGMDECL(int)    PGMInterpretInstruction(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
PGMDECL(int)    PGMMap(PVM pVM, RTGCUINTPTR GCPtr, RTHCPHYS HCPhys, uint32_t cbPages, unsigned fFlags);
PGMDECL(int)    PGMMapSetPage(PVM pVM, RTGCPTR GCPtr, uint64_t cb, uint64_t fFlags);
PGMDECL(int)    PGMMapModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGMDECL(int)    PGMShwGetPage(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys);
PGMDECL(int)    PGMShwSetPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags);
PGMDECL(int)    PGMShwModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGMDECL(int)    PGMGstGetPage(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);
PGMDECL(bool)   PGMGstIsPagePresent(PVM pVM, RTGCPTR GCPtr);
PGMDECL(int)    PGMGstSetPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags);
PGMDECL(int)    PGMGstModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGMDECL(int)    PGMFlushTLB(PVM pVM, uint32_t cr3, bool fGlobal);
PGMDECL(int)    PGMSyncCR3(PVM pVM, uint32_t cr0, uint32_t cr3, uint32_t cr4, bool fGlobal);
PGMDECL(int)    PGMChangeMode(PVM pVM, uint32_t cr0, uint32_t cr4, uint64_t efer);
PGMDECL(PGMMODE) PGMGetGuestMode(PVM pVM);
PGMDECL(PGMMODE) PGMGetShadowMode(PVM pVM);
PGMDECL(const char *) PGMGetModeName(PGMMODE enmMode);
PGMDECL(int)    PGMHandlerPhysicalRegisterEx(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                             R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                             R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                             GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                             R3PTRTYPE(const char *) pszDesc);
PGMDECL(int)    PGMHandlerPhysicalModify(PVM pVM, RTGCPHYS GCPhysCurrent, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast);
PGMDECL(int)    PGMHandlerPhysicalDeregister(PVM pVM, RTGCPHYS GCPhys);
PGMDECL(int)    PGMHandlerPhysicalChangeCallbacks(PVM pVM, RTGCPHYS GCPhys,
                                                  R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                                  R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                                  GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                                  R3PTRTYPE(const char *) pszDesc);
PGMDECL(int)    PGMHandlerPhysicalSplit(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysSplit);
PGMDECL(int)    PGMHandlerPhysicalJoin(PVM pVM, RTGCPHYS GCPhys1, RTGCPHYS GCPhys2);
PGMDECL(int)    PGMHandlerPhysicalPageTempOff(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage);
PGMDECL(int)    PGMHandlerPhysicalReset(PVM pVM, RTGCPHYS GCPhys);
PGMDECL(int)    PGMHandlerPhysicalPageReset(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage);
PGMDECL(bool)   PGMHandlerPhysicalIsRegistered(PVM pVM, RTGCPHYS GCPhys);
PGMDECL(bool)   PGMPhysIsA20Enabled(PVM pVM);
PGMDECL(bool)   PGMPhysIsGCPhysValid(PVM pVM, RTGCPHYS GCPhys);
PGMDECL(bool)   PGMPhysIsGCPhysNormal(PVM pVM, RTGCPHYS GCPhys);
PGMDECL(int)    PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys);
PGMDECL(int)    PGMPhysGCPtr2GCPhys(PVM pVM, RTGCPTR GCPtr, PRTGCPHYS pGCPhys);
PGMDECL(int)    PGMPhysGCPtr2HCPhys(PVM pVM, RTGCPTR GCPtr, PRTHCPHYS pHCPhys);
PDMDECL(void)   PGMPhysInvalidatePageGCMapTLB(PVM pVM);
PDMDECL(void)   PGMPhysInvalidatePageR0MapTLB(PVM pVM);
PDMDECL(void)   PGMPhysInvalidatePageR3MapTLB(PVM pVM);

/**
 * Page mapping lock.
 *
 * @remarks This doesn't work in structures shared between
 *          ring-3, ring-0 and/or GC.
 */
typedef struct PGMPAGEMAPLOCK
{
    /** @todo see PGMPhysIsPageMappingLockValid for possibly incorrect assumptions */
#ifdef IN_GC
    /** Just a dummy for the time being. */
    uint32_t    u32Dummy;
#else
    /** Pointer to the PGMPAGE. */
    void       *pvPage;
    /** Pointer to the PGMCHUNKR3MAP. */
    void       *pvMap;
#endif
} PGMPAGEMAPLOCK;
/** Pointer to a page mapping lock. */
typedef PGMPAGEMAPLOCK *PPGMPAGEMAPLOCK;

PGMDECL(int)    PGMPhysGCPhys2CCPtr(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock);
PGMDECL(int)    PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock);
PGMDECL(int)    PGMPhysGCPtr2CCPtr(PVM pVM, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock);
PGMDECL(int)    PGMPhysGCPtr2CCPtrReadOnly(PVM pVM, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock);
PGMDECL(void)   PGMPhysReleasePageMappingLock(PVM pVM, PPGMPAGEMAPLOCK pLock);

/**
 * Checks if the lock structure is valid
 *
 * @param   pVM         The VM handle.
 * @param   pLock       The lock structure initialized by the mapping function.
 */
DECLINLINE(bool) PGMPhysIsPageMappingLockValid(PVM pVM, PPGMPAGEMAPLOCK pLock)
{
    /** @todo -> complete/change this  */
#ifdef IN_GC
    return !!(pLock->u32Dummy);
#else
    return !!(pLock->pvPage);
#endif
}

PGMDECL(int)    PGMPhysGCPhys2HCPtr(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR pHCPtr);
PGMDECL(int)    PGMPhysGCPtr2HCPtr(PVM pVM, RTGCPTR GCPtr, PRTHCPTR pHCPtr);
PGMDECL(int)    PGMPhysGCPtr2HCPtrByGstCR3(PVM pVM, RTGCPTR GCPtr, uint32_t cr3, unsigned fFlags, PRTHCPTR pHCPtr);
PGMDECL(void)   PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);
PGMDECL(void)   PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);
#ifndef IN_GC /* Only ring 0 & 3. */
PGMDECL(int)    PGMPhysReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb);
PGMDECL(int)    PGMPhysWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb);
PGMDECL(int)    PGMPhysReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);
PGMDECL(int)    PGMPhysWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);
PGMDECL(int)    PGMPhysReadGCPtrSafe(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);
PGMDECL(int)    PGMPhysWriteGCPtrSafe(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);
PGMDECL(int)    PGMPhysWriteGCPtrDirty(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);
PGMDECL(int)    PGMInvalidatePage(PVM pVM, RTGCPTR GCPtrPage);
#endif /* !IN_GC */
PGMDECL(int)    PGMPhysInterpretedRead(PVM pVM, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb);
#ifdef VBOX_STRICT
PGMDECL(unsigned) PGMAssertHandlerAndFlagsInSync(PVM pVM);
PGMDECL(unsigned) PGMAssertNoMappingConflicts(PVM pVM);
PGMDECL(unsigned) PGMAssertCR3(PVM pVM, uint32_t cr3, uint32_t cr4);
#endif /* VBOX_STRICT */


#ifdef IN_GC
/** @defgroup grp_pgm_gc  The PGM Guest Context API
 * @ingroup grp_pgm
 * @{
 */
PGMGCDECL(int)  PGMGCDynMapGCPage(PVM pVM, RTGCPHYS GCPhys, void **ppv);
PGMGCDECL(int)  PGMGCDynMapGCPageEx(PVM pVM, RTGCPHYS GCPhys, void **ppv);
PGMGCDECL(int)  PGMGCDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv);
PGMGCDECL(int)  PGMGCSyncPT(PVM pVM, unsigned iPD, PVBOXPD pPDSrc);
PGMGCDECL(int)  PGMGCInvalidatePage(PVM pVM, RTGCPTR GCPtrPage);
/** @} */
#endif /* IN_GC */


#ifdef IN_RING0
/** @defgroup grp_pgm_r0  The PGM Host Context Ring-0 API
 * @ingroup grp_pgm
 * @{
 */
PGMR0DECL(int)  PGMR0PhysAllocateHandyPages(PVM pVM);
/** @} */
#endif /* IN_RING0 */



#ifdef IN_RING3
/** @defgroup grp_pgm_r3  The PGM Host Context Ring-3 API
 * @ingroup grp_pgm
 * @{
 */
PGMR3DECL(int)  PGMR3Init(PVM pVM);
PGMR3DECL(int)  PGMR3InitDynMap(PVM pVM);
PGMR3DECL(int)  PGMR3InitFinalize(PVM pVM);
PGMR3DECL(void) PGMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
PGMR3DECL(void) PGMR3Reset(PVM pVM);
PGMR3DECL(int)  PGMR3Term(PVM pVM);
PDMR3DECL(int)  PGMR3LockCall(PVM pVM);
PGMR3DECL(int)  PGMR3ChangeShwPDMappings(PVM pVM, bool fEnable);
#ifndef VBOX_WITH_NEW_PHYS_CODE
PGMR3DECL(int)  PGM3PhysGrowRange(PVM pVM, RTGCPHYS GCPhys);
#endif /* !VBOX_WITH_NEW_PHYS_CODE */
PGMR3DECL(int)  PGMR3PhysRegisterRam(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, const char *pszDesc);

/** @group PGMR3PhysRegisterRom flags.
 * @{ */
/** Inidicates that ROM shadowing should be enabled. */
#define PGMPHYS_ROM_FLAG_SHADOWED           RT_BIT_32(0)
/** Indicates that what pvBinary points to won't go away
 * and can be used for strictness checks. */
#define PGMPHYS_ROM_FLAG_PERMANENT_BINARY   RT_BIT_32(1)
/** @} */

PGMR3DECL(int)  PGMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPHYS cb,
                                     const void *pvBinary, uint32_t fFlags, const char *pszDesc);
PGMR3DECL(int)  PGMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, PGMROMPROT enmProt);
PGMR3DECL(int)  PGMR3PhysRegister(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc);
#ifndef VBOX_WITH_NEW_PHYS_CODE
PGMR3DECL(int)  PGMR3PhysRegisterChunk(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc);
#endif /* !VBOX_WITH_NEW_PHYS_CODE */
PGMR3DECL(int)  PGMR3PhysRelocate(PVM pVM, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, size_t cb);
PGMR3DECL(int)  PGMR3PhysSetFlags(PVM pVM, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, unsigned fMask);
PGMDECL(void)   PGMR3PhysSetA20(PVM pVM, bool fEnable);
PGMR3DECL(int)  PGMR3MapPT(PVM pVM, RTGCPTR GCPtr, size_t cb, PFNPGMRELOCATE pfnRelocate, void *pvUser, const char *pszDesc);
PGMR3DECL(int)  PGMR3UnmapPT(PVM pVM, RTGCPTR GCPtr);
PGMR3DECL(int)  PGMR3MappingsSize(PVM pVM, size_t *pcb);
PGMR3DECL(int)  PGMR3MappingsFix(PVM pVM, RTGCPTR GCPtrBase, size_t cb);
PGMR3DECL(int)  PGMR3MappingsUnfix(PVM pVM);
PGMR3DECL(int)  PGMR3MapIntermediate(PVM pVM, RTUINTPTR Addr, RTHCPHYS HCPhys, unsigned cbPages);
PGMR3DECL(bool) PGMR3MapHasConflicts(PVM pVM, uint32_t cr3, bool fRawR0);
PGMR3DECL(int)  PGMR3MapRead(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);
PGMR3DECL(int)  PGMR3HandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                             PFNPGMR3PHYSHANDLER pfnHandlerR3, void *pvUserR3,
                                             const char *pszModR0, const char *pszHandlerR0, RTR0PTR pvUserR0,
                                             const char *pszModGC, const char *pszHandlerGC, RTGCPTR pvUserGC, const char *pszDesc);
PGMDECL(int)    PGMHandlerVirtualRegisterEx(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                            PFNPGMHCVIRTINVALIDATE pfnInvalidateHC,
                                            PFNPGMHCVIRTHANDLER pfnHandlerHC, RTGCPTR pfnHandlerGC,
                                            R3PTRTYPE(const char *) pszDesc);
PGMR3DECL(int)  PGMR3HandlerVirtualRegister(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                            PFNPGMHCVIRTINVALIDATE pfnInvalidateHC,
                                            PFNPGMHCVIRTHANDLER pfnHandlerHC,
                                            const char *pszHandlerGC, const char *pszModGC, const char *pszDesc);
PGMDECL(int)    PGMHandlerVirtualChangeInvalidateCallback(PVM pVM, RTGCPTR GCPtr, PFNPGMHCVIRTINVALIDATE pfnInvalidateHC);
PGMDECL(int)    PGMHandlerVirtualDeregister(PVM pVM, RTGCPTR GCPtr);
PDMR3DECL(int)  PGMR3PoolGrow(PVM pVM);
#ifdef ___VBox_dbgf_h /** @todo fix this! */
PGMR3DECL(int)  PGMR3DumpHierarchyHC(PVM pVM, uint32_t cr3, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp);
#endif
PGMR3DECL(int)  PGMR3DumpHierarchyGC(PVM pVM, uint32_t cr3, uint32_t cr4, RTGCPHYS PhysSearch);
PGMR3DECL(void) PGMR3DumpPD(PVM pVM, PVBOXPD pPD);
PGMR3DECL(void) PGMR3DumpMappings(PVM pVM);

/** @todo r=bird: s/Byte/U8/ s/Word/U16/ s/Dword/U32/ to match other functions names and returned types. */
PGMR3DECL(uint8_t) PGMR3PhysReadByte(PVM pVM, RTGCPHYS GCPhys);
PGMR3DECL(uint16_t) PGMR3PhysReadWord(PVM pVM, RTGCPHYS GCPhys);
PGMR3DECL(uint32_t) PGMR3PhysReadDword(PVM pVM, RTGCPHYS GCPhys);
PGMR3DECL(void) PGMR3PhysWriteByte(PVM pVM, RTGCPHYS GCPhys, uint8_t val);
PGMR3DECL(void) PGMR3PhysWriteWord(PVM pVM, RTGCPHYS GCPhys, uint16_t val);
PGMR3DECL(void) PGMR3PhysWriteDword(PVM pVM, RTGCPHYS GCPhys, uint32_t val);
PDMR3DECL(int)  PGMR3PhysChunkMap(PVM pVM, uint32_t idChunk);
PGMR3DECL(void) PGMR3PhysChunkInvalidateTLB(PVM pVM);
PDMR3DECL(int)  PGMR3PhysAllocateHandyPages(PVM pVM);

PDMR3DECL(int)  PGMR3CheckIntegrity(PVM pVM);

PGMR3DECL(int)  PGMR3DbgHCPtr2GCPhys(PVM pVM, RTHCPTR HCPtr, PRTGCPHYS pGCPhys);
PGMR3DECL(int)  PGMR3DbgHCPtr2HCPhys(PVM pVM, RTHCPTR HCPtr, PRTHCPHYS pHCPhys);
PGMR3DECL(int)  PGMR3DbgHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys);
PDMR3DECL(int)  PGMR3DbgScanPhysical(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCPHYS pGCPhysHit);
PDMR3DECL(int)  PGMR3DbgScanVirtual(PVM pVM, RTGCUINTPTR GCPtr, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCUINTPTR pGCPhysHit);
/** @} */
#endif /* IN_RING3 */

__END_DECLS

/** @} */
#endif

