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
    /** Handle all normal page faults for a physical page range. */
    PGMPHYSHANDLERTYPE_PHYSICAL,
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
 * Gets the current CR3 register value for the shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetHyperCR3(PVM pVM);

/**
 * Gets the CR3 register value for the 32-Bit shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetHyper32BitCR3(PVM pVM);

/**
 * Gets the CR3 register value for the PAE shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetHyperPaeCR3(PVM pVM);

/**
 * Gets the CR3 register value for the AMD64 shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetHyperAmd64CR3(PVM pVM);

/**
 * Gets the current CR3 register value for the HC intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetInterHCCR3(PVM pVM);

/**
 * Gets the current CR3 register value for the GC intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetInterGCCR3(PVM pVM);

/**
 * Gets the CR3 register value for the 32-Bit intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetInter32BitCR3(PVM pVM);

/**
 * Gets the CR3 register value for the PAE intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetInterPaeCR3(PVM pVM);

/**
 * Gets the CR3 register value for the AMD64 intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
PGMDECL(uint32_t) PGMGetInterAmd64CR3(PVM pVM);

/**
 * \#PF Handler.
 *
 * @returns VBox status code (appropriate for GC return).
 * @param   pVM         VM Handle.
 * @param   uErr        The trap error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address.
 */
PGMDECL(int) PGMTrap0eHandler(PVM pVM, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);

/**
 * Prefetch a page/set of pages.
 *
 * Typically used to sync commonly used pages before entering raw mode
 * after a CR3 reload.
 *
 * @returns VBox status code suitable for scheduling.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_SYNC_CR3 if we're out of shadow pages or something like that.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to prefetch.
 */
PGMDECL(int) PGMPrefetchPage(PVM pVM, RTGCPTR GCPtrPage);

/**
 * Verifies a range of pages for read or write access.
 *
 * Supports handling of pages marked for dirty bit tracking and CSAM.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   Addr        Guest virtual address to check.
 * @param   cbSize      Access size.
 * @param   fAccess     Access type (r/w, user/supervisor (X86_PTE_*).
 */
PGMDECL(int) PGMVerifyAccess(PVM pVM, RTGCUINTPTR Addr, uint32_t cbSize, uint32_t fAccess);

/**
 * Verifies a range of pages for read or write access
 *
 * Only checks the guest's page tables
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   Addr        Guest virtual address to check
 * @param   cbSize      Access size
 * @param   fAccess     Access type (r/w, user/supervisor (X86_PTE_*))
 */
PGMDECL(int) PGMIsValidAccess(PVM pVM, RTGCUINTPTR Addr, uint32_t cbSize, uint32_t fAccess);

/**
 * Executes an instruction using the interpreter.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM handle.
 * @param   pRegFrame   Register frame.
 * @param   pvFault     Fault address.
 */
PGMDECL(int) PGMInterpretInstruction(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);

/**
 * Maps a range of physical pages at a given virtual address
 * in the guest context.
 *
 * The GC virtual address range must be within an existing mapping.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   GCPtr       Where to map the page(s). Must be page aligned.
 * @param   HCPhys      Start of the range of physical pages. Must be page aligned.
 * @param   cbPages     Number of bytes to map. Must be page aligned.
 * @param   fFlags      Page flags (X86_PTE_*).
 */
PGMDECL(int) PGMMap(PVM pVM, RTGCUINTPTR GCPtr, RTHCPHYS HCPhys, uint32_t cbPages, unsigned fFlags);

/**
 * Sets (replaces) the page flags for a range of pages in a mapping.
 *
 * The pages must be mapped pages, it's not possible to change the flags of
 * Guest OS pages.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      Page flags X86_PTE_*, excluding the page mask of course.
 */
PGMDECL(int) PGMMapSetPage(PVM pVM, RTGCPTR GCPtr, uint64_t cb, uint64_t fFlags);

/**
 * Modify page flags for a range of pages in a mapping.
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*.
 *                      Be very CAREFUL when ~'ing constants which could be 32-bit!
 */
PGMDECL(int) PGMMapModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);

/**
 * Gets effective page information (from the VMM page directory).
 *
 * @returns VBox status.
 * @param   pVM         VM Handle.
 * @param   GCPtr       Guest Context virtual address of the page.
 * @param   pfFlags     Where to store the flags. These are X86_PTE_*.
 * @param   pHCPhys     Where to store the HC physical address of the page.
 *                      This is page aligned.
 * @remark  You should use PGMMapGetPage() for pages in a mapping.
 */
PGMDECL(int) PGMShwGetPage(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys);

/**
 * Sets (replaces) the page flags for a range of pages in the shadow context.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address of the first page.
 * @param   cb          The size of the range in bytes.
 * @param   fFlags      Page flags X86_PTE_*, excluding the page mask of course.
 * @remark  You must use PGMMapSetPage() for pages in a mapping.
 */
PGMDECL(int) PGMShwSetPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags);

/**
 * Modify page flags for a range of pages in the shadow context.
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*.
 *                      Be very CAREFUL when ~'ing constants which could be 32-bit!
 * @remark  You must use PGMMapModifyPage() for pages in a mapping.
 */
PGMDECL(int)  PGMShwModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);

/**
 * Gets effective Guest OS page information.
 *
 * When GCPtr is in a big page, the function will return as if it was a normal
 * 4KB page. If the need for distinguishing between big and normal page becomes
 * necessary at a later point, a PGMGstGetPageEx() will be created for that
 * purpose.
 *
 * @returns VBox status.
 * @param   pVM         VM Handle.
 * @param   GCPtr       Guest Context virtual address of the page.
 * @param   pfFlags     Where to store the flags. These are X86_PTE_*, even for big pages.
 * @param   pGCPhys     Where to store the GC physical address of the page.
 *                      This is page aligned. The fact that the
 */
PGMDECL(int) PGMGstGetPage(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);

/**
 * Checks if the page is present.
 *
 * @returns true if the page is present.
 * @returns false if the page is not present.
 * @param   pVM         The VM handle.
 * @param   GCPtr       Address within the page.
 */
PGMDECL(bool) PGMGstIsPagePresent(PVM pVM, RTGCPTR GCPtr);

/**
 * Sets (replaces) the page flags for a range of pages in the guest's tables.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address of the first page.
 * @param   cb          The size of the range in bytes.
 * @param   fFlags      Page flags X86_PTE_*, excluding the page mask of course.
 */
PGMDECL(int)  PGMGstSetPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags);

/**
 * Modify page flags for a range of pages in the guest's tables
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*, excluding the page mask of course.
 *                      Be very CAREFUL when ~'ing constants which could be 32-bit!
 */
PGMDECL(int)  PGMGstModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);

/**
 * Performs and schedules necessary updates following a CR3 load or reload.
 *
 * This will normally involve mapping the guest PD or nPDPTR
 *
 * @returns VBox status code.
 * @retval  VINF_PGM_SYNC_CR3 if monitoring requires a CR3 sync. This can
 *          safely be ignored and overridden since the FF will be set too then.
 * @param   pVM         VM handle.
 * @param   cr3         The new cr3.
 * @param   fGlobal     Indicates whether this is a global flush or not.
 */
PGMDECL(int) PGMFlushTLB(PVM pVM, uint32_t cr3, bool fGlobal);

/**
 * Synchronize the paging structures.
 *
 * This function is called in response to the VM_FF_PGM_SYNC_CR3 and
 * VM_FF_PGM_SYNC_CR3_NONGLOBAL. Those two force action flags are set
 * in several places, most importantly whenever the CR3 is loaded.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   cr0         Guest context CR0 register
 * @param   cr3         Guest context CR3 register
 * @param   cr4         Guest context CR4 register
 * @param   fGlobal     Including global page directories or not
 */
PGMDECL(int) PGMSyncCR3(PVM pVM, uint32_t cr0, uint32_t cr3, uint32_t cr4, bool fGlobal);

/**
 * Called whenever CR0 or CR4 in a way which may change
 * the paging mode.
 *
 * @returns VBox status code fit for scheduling in GC and R0.
 * @retval  VINF_SUCCESS if the was no change, or it was successfully dealt with.
 * @retval  VINF_PGM_CHANGE_MODE if we're in GC or R0 and the mode changes.
 * @param   pVM         VM handle.
 * @param   cr0         The new cr0.
 * @param   cr4         The new cr4.
 * @param   efer        The new extended feature enable register.
 */
PGMDECL(int) PGMChangeMode(PVM pVM, uint32_t cr0, uint32_t cr4, uint64_t efer);

/**
 * Gets the current guest paging mode.
 *
 * If you just need the CPU mode (real/protected/long), use CPUMGetGuestMode().
 *
 * @returns The current paging mode.
 * @param   pVM             The VM handle.
 */
PGMDECL(PGMMODE) PGMGetGuestMode(PVM pVM);

/**
 * Gets the current shadow paging mode.
 *
 * @returns The current paging mode.
 * @param   pVM             The VM handle.
 */
PGMDECL(PGMMODE) PGMGetShadowMode(PVM pVM);

/**
 * Get mode name.
 *
 * @returns read-only name string.
 * @param   enmMode     The mode which name is desired.
 */
PGMDECL(const char *) PGMGetModeName(PGMMODE enmMode);

/**
 * Register a access handler for a physical range.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type. Any of the PGMPHYSHANDLERTYPE_PHYSICAL* enums.
 * @param   GCPhys          Start physical address.
 * @param   GCPhysLast      Last physical address. (inclusive)
 * @param   pfnHandlerR3    The R3 handler.
 * @param   pvUserR3        User argument to the R3 handler.
 * @param   pfnHandlerR0    The R0 handler.
 * @param   pvUserR0        User argument to the R0 handler.
 * @param   pfnHandlerGC    The GC handler.
 * @param   pvUserGC        User argument to the GC handler.
 *                          This must be a GC pointer because it will be relocated!
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMDECL(int) PGMHandlerPhysicalRegisterEx(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                          R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                          R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                          GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                          R3PTRTYPE(const char *) pszDesc);

/**
 * Modify a physical page access handler.
 *
 * Modification can only be done to the range it self, not the type or anything else.
 *
 * @returns VBox status code.
 *          For all return codes other than VERR_PGM_HANDLER_NOT_FOUND and VINF_SUCCESS the range is deregistered
 *          and a new registration must be performed!
 * @param   pVM             VM handle.
 * @param   GCPhysCurrent   Current location.
 * @param   GCPhys          New location.
 * @param   GCPhysLast      New last location.
 */
PGMDECL(int) PGMHandlerPhysicalModify(PVM pVM, RTGCPHYS GCPhysCurrent, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast);

/**
 * Register a physical page access handler.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle.
 * @param   GCPhys      Start physical address earlier passed to PGMR3HandlerPhysicalRegister().
 */
PGMDECL(int) PGMHandlerPhysicalDeregister(PVM pVM, RTGCPHYS GCPhys);

/**
 * Changes the callbacks associated with a physical access handler.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys          Start physical address.
 * @param   pfnHandlerR3    The R3 handler.
 * @param   pvUserR3        User argument to the R3 handler.
 * @param   pfnHandlerR0    The R0 handler.
 * @param   pvUserR0        User argument to the R0 handler.
 * @param   pfnHandlerGC    The GC handler.
 * @param   pvUserGC        User argument to the GC handler.
 *                          This must be a GC pointer because it will be relocated!
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMDECL(int) PGMHandlerPhysicalChangeCallbacks(PVM pVM, RTGCPHYS GCPhys,
                                               R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                               R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                               GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                               R3PTRTYPE(const char *) pszDesc);

/**
 * Splitts a physical access handler in two.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys          Start physical address of the handler.
 * @param   GCPhysSplit     The split address.
 */
PGMDECL(int) PGMHandlerPhysicalSplit(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysSplit);

/**
 * Joins up two adjacent physical access handlers which has the same callbacks.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys1         Start physical address of the first handler.
 * @param   GCPhys2         Start physical address of the second handler.
 */
PGMDECL(int) PGMHandlerPhysicalJoin(PVM pVM, RTGCPHYS GCPhys1, RTGCPHYS GCPhys2);

/**
 * Temporarily turns off the access monitoring of a page within a monitored
 * physical write/all page access handler region.
 *
 * Use this when no further #PFs are required for that page. Be aware that
 * a page directory sync might reset the flags, and turn on access monitoring
 * for the page.
 *
 * The caller must do required page table modifications.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   GCPhys      Start physical address earlier passed to PGMR3HandlerPhysicalRegister().
 * @param   GCPhysPage  Physical address of the page to turn off access monitoring for.
 */
PGMDECL(int)  PGMHandlerPhysicalPageTempOff(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage);


/**
 * Resets any modifications to individual pages in a physical
 * page access handler region.
 *
 * This is used in pair with PGMHandlerPhysicalModify().
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   GCPhys      Start physical address earlier passed to PGMR3HandlerPhysicalRegister().
 */
PGMDECL(int) PGMHandlerPhysicalReset(PVM pVM, RTGCPHYS GCPhys);

/**
 * Turns access monitoring of a page within a monitored
 * physical write/all page access handler region back on.
 *
 * The caller must do required page table modifications.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   GCPhys      Start physical address earlier passed to PGMR3HandlerPhysicalRegister().
 * @param   GCPhysPage  Physical address of the page to turn on access monitoring for.
 */
PGMDECL(int)  PGMHandlerPhysicalPageReset(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage);

/**
 * Checks if a physical range is handled
 *
 * @returns boolean.
 * @param   pVM         VM Handle
 * @param   GCPhys      Start physical address earlier passed to PGMR3HandlerPhysicalRegister().
 */
PGMDECL(bool)  PGMHandlerPhysicalIsRegistered(PVM pVM, RTGCPHYS GCPhys);

/**
 * Checks if Address Gate 20 is enabled or not.
 *
 * @returns true if enabled.
 * @returns false if disabled.
 * @param   pVM     VM handle.
 */
PGMDECL(bool) PGMPhysIsA20Enabled(PVM pVM);

/**
 * Validates a GC physical address.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The physical address to validate.
 */
PGMDECL(bool) PGMPhysIsGCPhysValid(PVM pVM, RTGCPHYS GCPhys);

/**
 * Checks if a GC physical address is a normal page,
 * i.e. not ROM, MMIO or reserved.
 *
 * @returns true if normal.
 * @returns false if invalid, ROM, MMIO or reserved page.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The physical address to check.
 */
PGMDECL(bool) PGMPhysIsGCPhysNormal(PVM pVM, RTGCPHYS GCPhys);

/**
 * Converts a GC physical address to a HC physical address.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
PGMDECL(int) PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys);

/**
 * Converts a guest pointer to a GC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pGCPhys     Where to store the GC physical address.
 */
PGMDECL(int) PGMPhysGCPtr2GCPhys(PVM pVM, RTGCPTR GCPtr, PRTGCPHYS pGCPhys);

/**
 * Converts a guest pointer to a HC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pHCPhys     Where to store the HC physical address.
 */
PGMDECL(int) PGMPhysGCPtr2HCPhys(PVM pVM, RTGCPTR GCPtr, PRTHCPHYS pHCPhys);


/**
 * Invalidates the GC page mapping TLB.
 *
 * @param   pVM     The VM handle.
 */
PDMDECL(void) PGMPhysInvalidatePageGCMapTLB(PVM pVM);

/**
 * Invalidates the ring-0 page mapping TLB.
 *
 * @param   pVM     The VM handle.
 */
PDMDECL(void) PGMPhysInvalidatePageR0MapTLB(PVM pVM);

/**
 * Invalidates the ring-3 page mapping TLB.
 *
 * @param   pVM     The VM handle.
 */
PDMDECL(void) PGMPhysInvalidatePageR3MapTLB(PVM pVM);

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

/**
 * Requests the mapping of a guest page into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify
 * the page, use the PGMPhysGCPhys2CCPtrReadOnly() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  Any thread.
 */
PGMDECL(int) PGMPhysGCPhys2CCPtr(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock);

/**
 * Requests the mapping of a guest page into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  Any thread.
 */
PGMDECL(int) PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock);

/**
 * Requests the mapping of a guest page given by virtual address into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify
 * the page, use the PGMPhysGCPtr2CCPtrReadOnly() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT if the page directory for the virtual address isn't present.
 * @retval  VERR_PAGE_NOT_PRESENT if the page at the virtual address isn't present.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  EMT
 */
PGMDECL(int) PGMPhysGCPtr2CCPtr(PVM pVM, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock);

/**
 * Requests the mapping of a guest page given by virtual address into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT if the page directory for the virtual address isn't present.
 * @retval  VERR_PAGE_NOT_PRESENT if the page at the virtual address isn't present.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  EMT
 */
PGMDECL(int) PGMPhysGCPtr2CCPtrReadOnly(PVM pVM, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock);

/**
 * Release the mapping of a guest page.
 *
 * This is the counter part of PGMPhysGCPhys2CCPtr, PGMPhysGCPhys2CCPtrReadOnly
 * PGMPhysGCPtr2CCPtr and PGMPhysGCPtr2CCPtrReadOnly.
 *
 * @param   pVM         The VM handle.
 * @param   pLock       The lock structure initialized by the mapping function.
 */
PGMDECL(void) PGMPhysReleasePageMappingLock(PVM pVM, PPGMPAGEMAPLOCK pLock);


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

/**
 * Converts a GC physical address to a HC pointer.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @param   pVM     The VM handle.
 * @param   GCPhys  The GC physical address to convert.
 * @param   cbRange Physical range
 * @param   pHCPtr  Where to store the HC pointer on success.
 *
 * @remark  Do *not* assume this mapping will be around forever!
 */
PGMDECL(int) PGMPhysGCPhys2HCPtr(PVM pVM, RTGCPHYS GCPhys, RTUINT cbRange, PRTHCPTR pHCPtr);

/**
 * Converts a guest pointer to a HC pointer.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   pHCPtr      Where to store the HC virtual address.
 *
 * @remark  Do *not* assume this mapping will be around forever!
 */
PGMDECL(int) PGMPhysGCPtr2HCPtr(PVM pVM, RTGCPTR GCPtr, PRTHCPTR pHCPtr);

/**
 * Converts a guest virtual address to a HC pointer by specfied CR3 and flags.
 *
 * @returns VBox status code.
 * @param   pVM         The VM Handle
 * @param   GCPtr       The guest pointer to convert.
 * @param   cr3         The guest CR3.
 * @param   fFlags      Flags used for interpreting the PD correctly: X86_CR4_PSE and X86_CR4_PAE
 * @param   pHCPtr      Where to store the HC pointer.
 *
 * @remark  Do *not* assume this mapping will be around forever!
 * @remark  This function is used by the REM at a time where PGM could
 *          potentially not be in sync. It could also be used by a
 *          future DBGF API to cpu state independent conversions.
 */
PGMDECL(int) PGMPhysGCPtr2HCPtrByGstCR3(PVM pVM, RTGCPTR GCPtr, uint32_t cr3, unsigned fFlags, PRTHCPTR pHCPtr);

/**
 * Read physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 * @param   pvBuf           Where to put the read bits.
 * @param   cbRead          How many bytes to read.
 */
PGMDECL(void) PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead);

/**
 * Write to physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   pvBuf           What to write.
 * @param   cbWrite         How many bytes to write.
 */
PGMDECL(void) PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite);


#ifndef IN_GC /* Only ring 0 & 3. */

/**
 * Read from guest physical memory by GC physical address, bypassing
 * MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPhysSrc   The source address (GC physical address).
 * @param   cb          The number of bytes to read.
 */
PGMDECL(int) PGMPhysReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb);

/**
 * Write to guest physical memory referenced by GC pointer.
 * Write memory to GC physical address in guest physical memory.
 *
 * This will bypass MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhysDst   The GC physical address of the destination.
 * @param   pvSrc       The source buffer.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb);

/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set any accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 */
PGMDECL(int) PGMPhysReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);

/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set dirty or accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);

/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * respect access handlers and set accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 */
PGMDECL(int) PGMPhysReadGCPtrSafe(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);

/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * respect access handlers and set dirty and accessed bits.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPtrSafe(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);

/**
 * Write to guest physical memory referenced by GC pointer and update the PTE.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and set any dirty and accessed bits in the PTE.
 *
 * If you don't want to set the dirty bit, use PGMR3PhysWriteGCPtr().
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
PGMDECL(int) PGMPhysWriteGCPtrDirty(PVM pVM, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);

/**
 * Emulation of the invlpg instruction (HC only actually).
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 * @remark  ASSUMES the page table entry or page directory is
 *          valid. Fairly safe, but there could be edge cases!
 * @todo    Flush page or page directory only if necessary!
 */
PGMDECL(int) PGMInvalidatePage(PVM pVM, RTGCPTR GCPtrPage);

#endif /* !IN_GC */

/**
 * Performs a read of guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the access bits.
 *
 * The current implementation will bypass all access handlers. It may later be
 * changed to at least respect MMIO.
 *
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVM         The VM handle.
 * @param   pCtxCore    The context core.
 * @param   pvDst       Where to put the bytes we've read.
 * @param   GCPtrSrc    The source address.
 * @param   cb          The number of bytes to read. Not more than a page.
 *
 * @remark  This function will dynamically map physical pages in GC. This may unmap
 *          mappings done by the caller. Be careful!
 */
PGMDECL(int) PGMPhysInterpretedRead(PVM pVM, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb);

#ifdef VBOX_STRICT
/**
 * Asserts that the handlers+guest-page-tables == ramrange-flags and
 * that the physical addresses associated with virtual handlers are correct.
 *
 * @returns Number of mismatches.
 * @param   pVM     The VM handle.
 */
PGMDECL(unsigned) PGMAssertHandlerAndFlagsInSync(PVM pVM);

/**
 * Asserts that there are no mapping conflicts.
 *
 * @returns Number of conflicts.
 * @param   pVM     The VM Handle.
 */
PGMDECL(unsigned) PGMAssertNoMappingConflicts(PVM pVM);

/**
 * Asserts that everything related to the guest CR3 is correctly shadowed.
 *
 * This will call PGMAssertNoMappingConflicts() and PGMAssertHandlerAndFlagsInSync(),
 * and assert the correctness of the guest CR3 mapping before asserting that the
 * shadow page tables is in sync with the guest page tables.
 *
 * @returns Number of conflicts.
 * @param   pVM     The VM Handle.
 * @param   cr3     The current guest CR3 register value.
 * @param   cr4     The current guest CR4 register value.
 */
PGMDECL(unsigned) PGMAssertCR3(PVM pVM, uint32_t cr3, uint32_t cr4);
#endif /* VBOX_STRICT */


#ifdef IN_GC

/** @defgroup grp_pgm_gc  The PGM Guest Context API
 * @ingroup grp_pgm
 * @{
 */

/**
 * Temporarily maps one guest page specified by GC physical address.
 * These pages must have a physical mapping in HC, i.e. they cannot be MMIO pages.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhys      GC Physical address of the page.
 * @param   ppv         Where to store the address of the mapping.
 */
PGMGCDECL(int) PGMGCDynMapGCPage(PVM pVM, RTGCPHYS GCPhys, void **ppv);

/**
 * Temporarily maps one guest page specified by unaligned GC physical address.
 * These pages must have a physical mapping in HC, i.e. they cannot be MMIO pages.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * The caller is aware that only the speicifed page is mapped and that really bad things
 * will happen if writing beyond the page!
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhys      GC Physical address within the page to be mapped.
 * @param   ppv         Where to store the address of the mapping address corresponding to GCPhys.
 */
PGMGCDECL(int) PGMGCDynMapGCPageEx(PVM pVM, RTGCPHYS GCPhys, void **ppv);

/**
 * Temporarily maps one host page specified by HC physical address.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   HCPhys      HC Physical address of the page.
 * @param   ppv         Where to store the address of the mapping.
 */
PGMGCDECL(int) PGMGCDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv);

/**
 * Syncs a guest os page table.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   iPD         Page directory index.
 * @param   pPDSrc      Source page directory (i.e. Guest OS page directory).
 *                      Assume this is a temporary mapping.
 */
PGMGCDECL(int) PGMGCSyncPT(PVM pVM, unsigned iPD, PVBOXPD pPDSrc);

/**
 * Emulation of the invlpg instruction.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 */
PGMGCDECL(int) PGMGCInvalidatePage(PVM pVM, RTGCPTR GCPtrPage);

/** @} */
#endif


#ifdef IN_RING0
/** @defgroup grp_pgm_r0  The PGM Host Context Ring-0 API
 * @ingroup grp_pgm
 * @{
 */
/**
 * Worker function for PGMR3PhysAllocateHandyPages and pgmPhysEnsureHandyPage.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is set in this case.
 *
 * @param   pVM         The VM handle.
 *
 * @remarks Must be called from within the PGM critical section.
 */
PGMR0DECL(int) PGMR0PhysAllocateHandyPages(PVM pVM);

/** @} */
#endif



#ifdef IN_RING3
/** @defgroup grp_pgm_r3  The PGM Host Context Ring-3 API
 * @ingroup grp_pgm
 * @{
 */
/**
 * Initiates the paging of VM.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to VM structure.
 */
PGMR3DECL(int) PGMR3Init(PVM pVM);

/**
 * Init the PGM bits that rely on VMMR0 and MM to be fully initialized.
 *
 * The dynamic mapping area will also be allocated and initialized at this
 * time. We could allocate it during PGMR3Init of course, but the mapping
 * wouldn't be allocated at that time preventing us from setting up the
 * page table entries with the dummy page.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
PGMR3DECL(int) PGMR3InitDynMap(PVM pVM);

/**
 * Ring-3 init finalizing.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
PGMR3DECL(int) PGMR3InitFinalize(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
PGMR3DECL(void) PGMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * The VM is being reset.
 *
 * For the PGM component this means that any PD write monitors
 * needs to be removed.
 *
 * @param   pVM     VM handle.
 */
PGMR3DECL(void) PGMR3Reset(PVM pVM);

/**
 * Terminates the PGM.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to VM structure.
 */
PGMR3DECL(int) PGMR3Term(PVM pVM);

/**
 * Serivce a VMMCALLHOST_PGM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PGMR3LockCall(PVM pVM);

/**
 * Inform PGM if we want all mappings to be put into the shadow page table. (necessary for e.g. VMX)
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   fEnable     Enable or disable shadow mappings
 */
PGMR3DECL(int) PGMR3ChangeShwPDMappings(PVM pVM, bool fEnable);

/**
 * Allocate missing physical pages for an existing guest RAM range.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 */
PGMR3DECL(int) PGM3PhysGrowRange(PVM pVM, RTGCPHYS GCPhys);

/**
 * Interface that the MMR3RamRegister(), MMR3RomRegister() and MMIO handler
 * registration APIs calls to inform PGM about memory registrations.
 *
 * It registers the physical memory range with PGM. MM is responsible
 * for the toplevel things - allocation and locking - while PGM is taking
 * care of all the details and implements the physical address space virtualization.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pvRam           HC virtual address of the RAM range. (page aligned)
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          Flags, MM_RAM_*.
 * @param   paPages         Pointer an array of physical page descriptors.
 * @param   pszDesc         Description string.
 */
PGMR3DECL(int) PGMR3PhysRegister(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc);

/**
 * Register a chunk of a the physical memory range with PGM. MM is responsible
 * for the toplevel things - allocation and locking - while PGM is taking
 * care of all the details and implements the physical address space virtualization.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pvRam           HC virtual address of the RAM range. (page aligned)
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          Flags, MM_RAM_*.
 * @param   paPages         Pointer an array of physical page descriptors.
 * @param   pszDesc         Description string.
 */
PGMR3DECL(int) PGMR3PhysRegisterChunk(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc);

/**
 * Interface MMIO handler relocation calls.
 *
 * It relocates an existing physical memory range with PGM.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhysOld       Previous GC physical address of the RAM range. (page aligned)
 * @param   GCPhysNew       New GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 */
PGMR3DECL(int) PGMR3PhysRelocate(PVM pVM, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, size_t cb);

/**
 * Interface MMR3RomRegister() and MMR3PhysReserve calls to update the
 * flags of existing RAM ranges.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          The Or flags, MM_RAM_* #defines.
 * @param   fMask           The and mask for the flags.
 */
PGMR3DECL(int) PGMR3PhysSetFlags(PVM pVM, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, unsigned fMask);

/**
 * Sets the Address Gate 20 state.
 *
 * @param   pVM         VM handle.
 * @param   fEnable     True if the gate should be enabled.
 *                      False if the gate should be disabled.
 */
PGMDECL(void) PGMR3PhysSetA20(PVM pVM, bool fEnable);

/**
 * Creates a page table based mapping in GC.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPtr           Virtual Address. (Page table aligned!)
 * @param   cb              Size of the range. Must be a 4MB aligned!
 * @param   pfnRelocate     Relocation callback function.
 * @param   pvUser          User argument to the callback.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMR3DECL(int)  PGMR3MapPT(PVM pVM, RTGCPTR GCPtr, size_t cb, PFNPGMRELOCATE pfnRelocate, void *pvUser, const char *pszDesc);

/**
 * Removes a page table based mapping.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   GCPtr   Virtual Address. (Page table aligned!)
 */
PGMR3DECL(int)  PGMR3UnmapPT(PVM pVM, RTGCPTR GCPtr);

/**
 * Gets the size of the current guest mappings if they were to be
 * put next to oneanother.
 *
 * @returns VBox status code.
 * @param   pVM     The VM.
 * @param   pcb     Where to store the size.
 */
PGMR3DECL(int) PGMR3MappingsSize(PVM pVM, size_t *pcb);

/**
 * Fixes the guest context mappings in a range reserved from the Guest OS.
 *
 * @returns VBox status code.
 * @param   pVM         The VM.
 * @param   GCPtrBase   The address of the reserved range of guest memory.
 * @param   cb          The size of the range starting at GCPtrBase.
 */
PGMR3DECL(int) PGMR3MappingsFix(PVM pVM, RTGCPTR GCPtrBase, size_t cb);

/**
 * Unfixes the mappings.
 * After calling this function mapping conflict detection will be enabled.
 *
 * @returns VBox status code.
 * @param   pVM         The VM.
 */
PGMR3DECL(int) PGMR3MappingsUnfix(PVM pVM);

/**
 * Map pages into the intermediate context (switcher code).
 * These pages are mapped at both the give virtual address and at
 * the physical address (for identity mapping).
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   Addr        Intermediate context address of the mapping.
 * @param   HCPhys      Start of the range of physical pages. This must be entriely below 4GB!
 * @param   cbPages     Number of bytes to map.
 *
 * @remark  This API shall not be used to anything but mapping the switcher code.
 */
PGMR3DECL(int) PGMR3MapIntermediate(PVM pVM, RTUINTPTR Addr, RTHCPHYS HCPhys, unsigned cbPages);

/**
 * Checks guest PD for conflicts with VMM GC mappings.
 *
 * @returns true if conflict detected.
 * @returns false if not.
 * @param   pVM         The virtual machine.
 * @param   cr3         Guest context CR3 register.
 * @param   fRawR0      Whether RawR0 is enabled or not.
 */
PGMR3DECL(bool) PGMR3MapHasConflicts(PVM pVM, uint32_t cr3, bool fRawR0);

/**
 * Read memory from the guest mappings.
 *
 * This will use the page tables associated with the mappings to
 * read the memory. This means that not all kind of memory is readable
 * since we don't necessarily know how to convert that physical address
 * to a HC virtual one.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   pvDst       The destination address (HC of course).
 * @param   GCPtrSrc    The source address (GC virtual address).
 * @param   cb          Number of bytes to read.
 */
PGMR3DECL(int) PGMR3MapRead(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);

/**
 * Register a access handler for a physical range.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmType         Handler type. Any of the PGMPHYSHANDLERTYPE_PHYSICAL* enums.
 * @param   GCPhys          Start physical address.
 * @param   GCPhysLast      Last physical address. (inclusive)
 * @param   pfnHandlerR3    The R3 handler.
 * @param   pvUserR3        User argument to the R3 handler.
 * @param   pszModR0        The R0 handler module. NULL means default R0 module.
 * @param   pszHandlerR0    The R0 handler symbol name.
 * @param   pvUserR0        User argument to the R0 handler.
 * @param   pszModGC        The GC handler module. NULL means default GC module.
 * @param   pszHandlerGC    The GC handler symbol name.
 * @param   pvUserGC        User argument to the GC handler.
 *                          This must be a GC pointer because it will be relocated!
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMR3DECL(int) PGMR3HandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                            PFNPGMR3PHYSHANDLER pfnHandlerR3, void *pvUserR3,
                                            const char *pszModR0, const char *pszHandlerR0, RTR0PTR pvUserR0,
                                            const char *pszModGC, const char *pszHandlerGC, RTGCPTR pvUserGC, const char *pszDesc);

/**
 * Register an access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmType         Handler type. Any of the PGMVIRTHANDLERTYPE_* enums.
 * @param   GCPtr           Start address.
 * @param   GCPtrLast       Last address. (inclusive)
 * @param   pfnInvalidateHC The HC invalidate callback (can be 0)
 * @param   pfnHandlerHC    The HC handler.
 * @param   pfnHandlerGC    The GC handler.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMDECL(int) PGMHandlerVirtualRegisterEx(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                         PFNPGMHCVIRTINVALIDATE pfnInvalidateHC,
                                         PFNPGMHCVIRTHANDLER pfnHandlerHC, RTGCPTR pfnHandlerGC,
                                         R3PTRTYPE(const char *) pszDesc);

/**
 * Register a access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmType         Handler type. Any of the PGMVIRTHANDLERTYPE_* enums.
 * @param   GCPtr           Start address.
 * @param   GCPtrLast       Last address. (inclusive)
 * @param   pfnInvalidateHC The HC invalidate callback (can be 0)
 * @param   pfnHandlerHC    The HC handler.
 * @param   pszHandlerGC    The GC handler symbol name.
 * @param   pszModGC        The GC handler module.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMR3DECL(int) PGMR3HandlerVirtualRegister(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                           PFNPGMHCVIRTINVALIDATE pfnInvalidateHC,
                                           PFNPGMHCVIRTHANDLER pfnHandlerHC,
                                           const char *pszHandlerGC, const char *pszModGC, const char *pszDesc);

/**
 * Modify the page invalidation callback handler for a registered virtual range
 * (add more when needed)
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   GCPtr           Start address.
 * @param   pfnInvalidateHC The HC invalidate callback (can be 0)
 */
PGMDECL(int) PGMHandlerVirtualChangeInvalidateCallback(PVM pVM, RTGCPTR GCPtr, PFNPGMHCVIRTINVALIDATE pfnInvalidateHC);


/**
 * Deregister an access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Start address.
 */
PGMDECL(int) PGMHandlerVirtualDeregister(PVM pVM, RTGCPTR GCPtr);

/**
 * Grows the shadow page pool.
 *
 * I.e. adds more pages to it, assuming that hasn't reached cMaxPages yet.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PGMR3PoolGrow(PVM pVM);

#ifdef ___VBox_dbgf_h /** @todo fix this! */
/**
 * Dumps a page table hierarchy use only physical addresses and cr4/lm flags.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The cr4, only PAE and PSE is currently used.
 * @param   fLongMode   Set if long mode, false if not long mode.
 * @param   cMaxDepth   Number of levels to dump.
 * @param   pHlp        Pointer to the output functions.
 */
PGMR3DECL(int) PGMR3DumpHierarchyHC(PVM pVM, uint32_t cr3, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp);
#endif

/**
 * Dumps a 32-bit guest page directory and page tables.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   PhysSearch  Address to search for.
 */
PGMR3DECL(int) PGMR3DumpHierarchyGC(PVM pVM, uint32_t cr3, uint32_t cr4, RTGCPHYS PhysSearch);

/**
 * Debug helper - Dumps the supplied page directory.
 *
 * @internal
 */
PGMR3DECL(void) PGMR3DumpPD(PVM pVM, PVBOXPD pPD);

/**
 * Dumps the the PGM mappings..
 *
 * @param   pVM     VM handle.
 */
PGMR3DECL(void) PGMR3DumpMappings(PVM pVM);

/** @todo r=bird: s/Byte/U8/ s/Word/U16/ s/Dword/U32/ to match other functions names and returned types. */
/**
 * Read physical memory. (one byte)
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 */
PGMR3DECL(uint8_t) PGMR3PhysReadByte(PVM pVM, RTGCPHYS GCPhys);

/**
 * Read physical memory. (one word)
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 */
PGMR3DECL(uint16_t) PGMR3PhysReadWord(PVM pVM, RTGCPHYS GCPhys);

/**
 * Read physical memory. (one dword)
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address start reading from.
 */
PGMR3DECL(uint32_t) PGMR3PhysReadDword(PVM pVM, RTGCPHYS GCPhys);

/**
 * Write to physical memory. (one byte)
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   val             What to write.
 */
PGMR3DECL(void) PGMR3PhysWriteByte(PVM pVM, RTGCPHYS GCPhys, uint8_t val);

/**
 * Write to physical memory. (one word)
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   val             What to write.
 */
PGMR3DECL(void) PGMR3PhysWriteWord(PVM pVM, RTGCPHYS GCPhys, uint16_t val);

/**
 * Write to physical memory. (one dword)
 *
 * This API respects access handlers and MMIO. Use PGMPhysReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             VM Handle.
 * @param   GCPhys          Physical address to write to.
 * @param   val             What to write.
 */
PGMR3DECL(void) PGMR3PhysWriteDword(PVM pVM, RTGCPHYS GCPhys, uint32_t val);

/**
 * For VMMCALLHOST_PGM_MAP_CHUNK, considered internal.
 *
 * @returns see pgmR3PhysChunkMap.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk to map.
 */
PDMR3DECL(int) PGMR3PhysChunkMap(PVM pVM, uint32_t idChunk);

/**
 * Invalidates the TLB for the ring-3 mapping cache.
 *
 * @param   pVM         The VM handle.
 */
PGMR3DECL(void) PGMR3PhysChunkInvalidateTLB(PVM pVM);

/**
 * Response to VM_FF_PGM_NEED_HANDY_PAGES and VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is not cleared in this case.
 *
 * @param   pVM         The VM handle.
 */
PDMR3DECL(int) PGMR3PhysAllocateHandyPages(PVM pVM);

/**
 * Perform an integrity check on the PGM component.
 *
 * @returns VINF_SUCCESS if everything is fine.
 * @returns VBox error status after asserting on integrity breach.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PGMR3CheckIntegrity(PVM pVM);

/**
 * Converts a HC pointer to a GC physical address.
 *
 * Only for the debugger.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 *
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPtr2GCPhys(PVM pVM, RTHCPTR HCPtr, PRTGCPHYS pGCPhys);

/**
 * Converts a HC pointer to a GC physical address.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pHCPhys is set.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical page but has no physical backing.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 *
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPtr2HCPhys(PVM pVM, RTHCPTR HCPtr, PRTHCPHYS pHCPhys);

/**
 * Converts a HC physical address to a GC physical address.
 *
 * Only for the debugger.
 *
 * @returns VBox status code
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the HC physical address is not within the GC physical memory.
 *
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys);

/**
 * Scans guest physical memory for a byte string.
 *
 * Only for the debugger.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS and *pGCPtrHit on success.
 * @retval  VERR_DBGF_MEM_NOT_FOUND if not found.
 * @retval  VERR_INVALID_POINTER if any of the pointer arguments are invalid.
 * @retval  VERR_INVALID_ARGUMENT if any other arguments are invalid.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPhys          Where to start searching.
 * @param   cbRange         The number of bytes to search. Max 256 bytes.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string.
 * @param   pGCPhysHit      Where to store the address of the first occurence on success.
 */
PDMR3DECL(int) PGMR3DbgScanPhysical(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCPHYS pGCPhysHit);

/**
 * Scans virtual memory for a byte string.
 *
 * Only for the debugger.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS and *pGCPtrHit on success.
 * @retval  VERR_DBGF_MEM_NOT_FOUND if not found.
 * @retval  VERR_INVALID_POINTER if any of the pointer arguments are invalid.
 * @retval  VERR_INVALID_ARGUMENT if any other arguments are invalid.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPtr           Where to start searching.
 * @param   cbRange         The number of bytes to search. Max 256 bytes.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string.
 * @param   pGCPtrHit       Where to store the address of the first occurence on success.
 */
PDMR3DECL(int) PGMR3DbgScanVirtual(PVM pVM, RTGCUINTPTR GCPtr, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCUINTPTR pGCPhysHit);

/** @} */

#endif /* IN_RING3 */

__END_DECLS

/** @} */
#endif

