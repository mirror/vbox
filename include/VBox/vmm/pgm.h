/** @file
 * PGM - Page Monitor / Monitor.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifndef VBOX_INCLUDED_vmm_pgm_h
#define VBOX_INCLUDED_vmm_pgm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/sup.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/gmm.h>               /* for PGMMREGISTERSHAREDMODULEREQ */
#include <iprt/x86.h>
#include <VBox/param.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_pgm   The Page Monitor / Manager API
 * @ingroup grp_vmm
 * @{
 */

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
 * @param   pVM         The cross context VM structure.
 * @param   GCPtrOld    The old virtual address.
 * @param   GCPtrNew    The new virtual address.
 * @param   enmMode     Used to indicate the callback mode.
 * @param   pvUser      User argument.
 * @remark  The return value is no a failure indicator, it's an acceptance
 *          indicator. Relocation can not fail!
 */
typedef DECLCALLBACKTYPE(bool, FNPGMRELOCATE,(PVM pVM, RTGCPTR GCPtrOld, RTGCPTR GCPtrNew, PGMRELOCATECALL enmMode, void *pvUser));
/** Pointer to a relocation callback function. */
typedef FNPGMRELOCATE *PFNPGMRELOCATE;


/**
 * Memory access origin.
 */
typedef enum PGMACCESSORIGIN
{
    /** Invalid zero value. */
    PGMACCESSORIGIN_INVALID = 0,
    /** IEM is access memory. */
    PGMACCESSORIGIN_IEM,
    /** HM is access memory. */
    PGMACCESSORIGIN_HM,
    /** Some device is access memory. */
    PGMACCESSORIGIN_DEVICE,
    /** Someone debugging is access memory. */
    PGMACCESSORIGIN_DEBUGGER,
    /** SELM is access memory. */
    PGMACCESSORIGIN_SELM,
    /** FTM is access memory. */
    PGMACCESSORIGIN_FTM,
    /** REM is access memory. */
    PGMACCESSORIGIN_REM,
    /** IOM is access memory. */
    PGMACCESSORIGIN_IOM,
    /** End of valid values. */
    PGMACCESSORIGIN_END,
    /** Type size hack. */
    PGMACCESSORIGIN_32BIT_HACK = 0x7fffffff
} PGMACCESSORIGIN;


/**
 * Physical page access handler kind.
 */
typedef enum PGMPHYSHANDLERKIND
{
    /** MMIO range. Pages are not present, all access is done in interpreter or recompiler. */
    PGMPHYSHANDLERKIND_MMIO = 1,
    /** Handler all write access to a physical page range. */
    PGMPHYSHANDLERKIND_WRITE,
    /** Handler all access to a physical page range. */
    PGMPHYSHANDLERKIND_ALL

} PGMPHYSHANDLERKIND;

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


/** @def PGM_ALL_CB_DECL
 * Macro for declaring a handler callback for all contexts.  The handler
 * callback is static in ring-3, and exported in RC and R0.
 * @sa PGM_ALL_CB2_DECL.
 */
#if defined(IN_RC) || defined(IN_RING0)
# ifdef __cplusplus
#  define PGM_ALL_CB_DECL(type)     extern "C" DECLCALLBACK(DECLEXPORT(type))
# else
#  define PGM_ALL_CB_DECL(type)     DECLCALLBACK(DECLEXPORT(type))
# endif
#else
# define PGM_ALL_CB_DECL(type)      static DECLCALLBACK(type)
#endif

/** @def PGM_ALL_CB2_DECL
 * Macro for declaring a handler callback for all contexts.  The handler
 * callback is hidden in ring-3, and exported in RC and R0.
 * @sa PGM_ALL_CB2_DECL.
 */
#if defined(IN_RC) || defined(IN_RING0)
# ifdef __cplusplus
#  define PGM_ALL_CB2_DECL(type)    extern "C" DECLCALLBACK(DECLEXPORT(type))
# else
#  define PGM_ALL_CB2_DECL(type)    DECLCALLBACK(DECLEXPORT(type))
# endif
#else
# define PGM_ALL_CB2_DECL(type)     DECL_HIDDEN_CALLBACK(type)
#endif

/** @def PGM_ALL_CB2_PROTO
 * Macro for declaring a handler callback for all contexts.  The handler
 * callback is hidden in ring-3, and exported in RC and R0.
 * @param   fnType      The callback function type.
 * @sa PGM_ALL_CB2_DECL.
 */
#if defined(IN_RC) || defined(IN_RING0)
# ifdef __cplusplus
#  define PGM_ALL_CB2_PROTO(fnType)    extern "C" DECLEXPORT(fnType)
# else
#  define PGM_ALL_CB2_PROTO(fnType)    DECLEXPORT(fnType)
# endif
#else
# define PGM_ALL_CB2_PROTO(fnType)     DECLHIDDEN(fnType)
#endif


/**
 * \#PF Handler callback for physical access handler ranges in RC and R0.
 *
 * @returns Strict VBox status code (appropriate for ring-0 and raw-mode).
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 *                      NULL on DMA and other non CPU access.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 * @thread  EMT(pVCpu)
 */
typedef DECLCALLBACKTYPE(VBOXSTRICTRC, FNPGMRZPHYSPFHANDLER,(PVMCC pVM, PVMCPUCC pVCpu, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame,
                                                             RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser));
/** Pointer to PGM access callback. */
typedef FNPGMRZPHYSPFHANDLER *PFNPGMRZPHYSPFHANDLER;


/**
 * Access handler callback for physical access handler ranges.
 *
 * The handler can not raise any faults, it's mainly for monitoring write access
 * to certain pages (like MMIO).
 *
 * @returns Strict VBox status code in ring-0 and raw-mode context, in ring-3
 *          the only supported informational status code is
 *          VINF_PGM_HANDLER_DO_DEFAULT.
 * @retval  VINF_SUCCESS if the handler have carried out the operation.
 * @retval  VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the
 *          access operation.
 * @retval  VINF_EM_XXX in ring-0 and raw-mode context.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure of the calling EMT.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   enmOrigin       The origin of this call.
 * @param   pvUser          User argument.
 * @thread  EMT(pVCpu)
 */
typedef DECLCALLBACKTYPE(VBOXSTRICTRC, FNPGMPHYSHANDLER,(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys, void *pvPhys,
                                                         void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType,
                                                         PGMACCESSORIGIN enmOrigin, void *pvUser));
/** Pointer to PGM access callback. */
typedef FNPGMPHYSHANDLER *PFNPGMPHYSHANDLER;


/**
 * Paging mode.
 *
 * @note    Part of saved state.  Change with extreme care.
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
    /** 32-bit nested paging mode (shadow only; guest physical to host physical). */
    PGMMODE_NESTED_32BIT,
    /** PAE nested paging mode (shadow only; guest physical to host physical). */
    PGMMODE_NESTED_PAE,
    /** AMD64 nested paging mode (shadow only; guest physical to host physical). */
    PGMMODE_NESTED_AMD64,
    /** Extended paging (Intel) mode. */
    PGMMODE_EPT,
    /** Special mode used by NEM to indicate no shadow paging necessary. */
    PGMMODE_NONE,
    /** The max number of modes */
    PGMMODE_MAX,
    /** 32bit hackishness. */
    PGMMODE_32BIT_HACK = 0x7fffffff
} PGMMODE;

/**
 * Second level address translation mode.
 */
typedef enum PGMSLAT
{
    /** The usual invalid value. */
    PGMSLAT_INVALID = 0,
    /** No second level translation. */
    PGMSLAT_DIRECT,
    /** Intel Extended Page Tables (EPT). */
    PGMSLAT_EPT,
    /** AMD-V Nested Paging 32-bit. */
    PGMSLAT_32BIT,
    /** AMD-V Nested Paging PAE. */
    PGMSLAT_PAE,
    /** AMD-V Nested Paging 64-bit. */
    PGMSLAT_AMD64,
    /** 32bit hackishness. */
    PGMSLAT_32BIT_HACK = 0x7fffffff
} PGMSLAT;

/** Macro for checking if the guest is using paging.
 * @param enmMode   PGMMODE_*.
 * @remark  ASSUMES certain order of the PGMMODE_* values.
 */
#define PGMMODE_WITH_PAGING(enmMode) ((enmMode) >= PGMMODE_32_BIT)

/** Macro for checking if it's one of the long mode modes.
 * @param enmMode   PGMMODE_*.
 */
#define PGMMODE_IS_LONG_MODE(enmMode) ((enmMode) == PGMMODE_AMD64_NX || (enmMode) == PGMMODE_AMD64)

/** Macro for checking if it's one of the AMD64 nested modes.
 * @param enmMode   PGMMODE_*.
 */
#define PGMMODE_IS_NESTED(enmMode)  (   (enmMode) == PGMMODE_NESTED_32BIT \
                                     || (enmMode) == PGMMODE_NESTED_PAE \
                                     || (enmMode) == PGMMODE_NESTED_AMD64)

/** Macro for checking if it's one of the PAE modes.
 * @param enmMode   PGMMODE_*.
 */
#define PGMMODE_IS_PAE(enmMode)     (   (enmMode) == PGMMODE_PAE \
                                     || (enmMode) == PGMMODE_PAE_NX)

/**
 * Is the ROM mapped (true) or is the shadow RAM mapped (false).
 *
 * @returns boolean.
 * @param   enmProt     The PGMROMPROT value, must be valid.
 */
#define PGMROMPROT_IS_ROM(enmProt) \
    (    (enmProt) == PGMROMPROT_READ_ROM_WRITE_IGNORE \
      || (enmProt) == PGMROMPROT_READ_ROM_WRITE_RAM )


VMMDECL(bool)           PGMIsLockOwner(PVMCC pVM);

VMMDECL(int)            PGMRegisterStringFormatTypes(void);
VMMDECL(void)           PGMDeregisterStringFormatTypes(void);
VMMDECL(RTHCPHYS)       PGMGetHyperCR3(PVMCPU pVCpu);
VMMDECL(int)            PGMTrap0eHandler(PVMCPUCC pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
VMMDECL(int)            PGMPrefetchPage(PVMCPUCC pVCpu, RTGCPTR GCPtrPage);
VMMDECL(int)            PGMVerifyAccess(PVMCPUCC pVCpu, RTGCPTR Addr, uint32_t cbSize, uint32_t fAccess);
VMMDECL(int)            PGMIsValidAccess(PVMCPUCC pVCpu, RTGCPTR Addr, uint32_t cbSize, uint32_t fAccess);
VMMDECL(VBOXSTRICTRC)   PGMInterpretInstruction(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
VMMDECL(int)            PGMShwGetPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys);
VMMDECL(int)            PGMShwMakePageReadonly(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fFlags);
VMMDECL(int)            PGMShwMakePageWritable(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fFlags);
VMMDECL(int)            PGMShwMakePageNotPresent(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fFlags);
/** @name Flags for PGMShwMakePageReadonly, PGMShwMakePageWritable and
 *        PGMShwMakePageNotPresent
 * @{ */
/** The call is from an access handler for dealing with the a faulting write
 * operation.  The virtual address is within the same page. */
#define PGM_MK_PG_IS_WRITE_FAULT     RT_BIT(0)
/** The page is an MMIO2. */
#define PGM_MK_PG_IS_MMIO2           RT_BIT(1)
/** @}*/
VMMDECL(int)        PGMGstGetPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);
VMMDECL(bool)       PGMGstIsPagePresent(PVMCPUCC pVCpu, RTGCPTR GCPtr);
VMMDECL(int)        PGMGstSetPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags);
VMMDECL(int)        PGMGstModifyPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
VMM_INT_DECL(bool)  PGMGstArePaePdpesValid(PVMCPUCC pVCpu, PCX86PDPE paPaePdpes);
VMM_INT_DECL(int)   PGMGstMapPaePdpes(PVMCPUCC pVCpu, PCX86PDPE paPaePdpes);
VMM_INT_DECL(int)   PGMGstMapPaePdpesAtCr3(PVMCPUCC pVCpu, uint64_t cr3);

VMMDECL(int)        PGMInvalidatePage(PVMCPUCC pVCpu, RTGCPTR GCPtrPage);
VMMDECL(int)        PGMFlushTLB(PVMCPUCC pVCpu, uint64_t cr3, bool fGlobal, bool fPdpesMapped);
VMMDECL(int)        PGMSyncCR3(PVMCPUCC pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal);
VMMDECL(int)        PGMUpdateCR3(PVMCPUCC pVCpu, uint64_t cr3, bool fPdpesMapped);
VMMDECL(int)        PGMChangeMode(PVMCPUCC pVCpu, uint64_t cr0, uint64_t cr4, uint64_t efer);
VMM_INT_DECL(int)   PGMHCChangeMode(PVMCC pVM, PVMCPUCC pVCpu, PGMMODE enmGuestMode);
VMMDECL(void)       PGMCr0WpEnabled(PVMCPUCC pVCpu);
VMMDECL(PGMMODE)    PGMGetGuestMode(PVMCPU pVCpu);
VMMDECL(PGMMODE)    PGMGetShadowMode(PVMCPU pVCpu);
VMMDECL(PGMMODE)    PGMGetHostMode(PVM pVM);
VMMDECL(const char *) PGMGetModeName(PGMMODE enmMode);
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX_EPT
VMM_INT_DECL(const char *) PGMGetSlatModeName(PGMSLAT enmSlatMode);
#endif
VMM_INT_DECL(RTGCPHYS) PGMGetGuestCR3Phys(PVMCPU pVCpu);
VMM_INT_DECL(void)  PGMNotifyNxeChanged(PVMCPU pVCpu, bool fNxe);
VMMDECL(bool)       PGMHasDirtyPages(PVM pVM);
VMM_INT_DECL(void)  PGMSetGuestEptPtr(PVMCPUCC pVCpu, uint64_t uEptPtr);

/** PGM physical access handler type registration handle (heap offset, valid
 * cross contexts without needing fixing up).  Callbacks and handler type is
 * associated with this and it is shared by all handler registrations. */
typedef uint32_t PGMPHYSHANDLERTYPE;
/** Pointer to a PGM physical handler type registration handle. */
typedef PGMPHYSHANDLERTYPE *PPGMPHYSHANDLERTYPE;
/** NIL value for PGM physical access handler type handle. */
#define NIL_PGMPHYSHANDLERTYPE  UINT32_MAX
VMMDECL(uint32_t)   PGMHandlerPhysicalTypeRelease(PVMCC pVM, PGMPHYSHANDLERTYPE hType);
VMMDECL(uint32_t)   PGMHandlerPhysicalTypeRetain(PVM pVM, PGMPHYSHANDLERTYPE hType);

VMMDECL(int)        PGMHandlerPhysicalRegister(PVMCC pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast, PGMPHYSHANDLERTYPE hType,
                                               RTR3PTR pvUserR3, RTR0PTR pvUserR0, RTRCPTR pvUserRC,
                                               R3PTRTYPE(const char *) pszDesc);
VMMDECL(int)        PGMHandlerPhysicalModify(PVMCC pVM, RTGCPHYS GCPhysCurrent, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast);
VMMDECL(int)        PGMHandlerPhysicalDeregister(PVMCC pVM, RTGCPHYS GCPhys);
VMMDECL(int)        PGMHandlerPhysicalChangeUserArgs(PVMCC pVM, RTGCPHYS GCPhys, RTR3PTR pvUserR3, RTR0PTR pvUserR0);
VMMDECL(int)        PGMHandlerPhysicalSplit(PVMCC pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysSplit);
VMMDECL(int)        PGMHandlerPhysicalJoin(PVMCC pVM, RTGCPHYS GCPhys1, RTGCPHYS GCPhys2);
VMMDECL(int)        PGMHandlerPhysicalPageTempOff(PVMCC pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage);
VMMDECL(int)        PGMHandlerPhysicalPageAliasMmio2(PVMCC pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage,
                                                     PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2, RTGCPHYS offMMio2PageRemap);
VMMDECL(int)        PGMHandlerPhysicalPageAliasHC(PVMCC pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage, RTHCPHYS HCPhysPageRemap);
VMMDECL(int)        PGMHandlerPhysicalReset(PVMCC pVM, RTGCPHYS GCPhys);
VMMDECL(bool)       PGMHandlerPhysicalIsRegistered(PVMCC pVM, RTGCPHYS GCPhys);


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
    /** Special page aliased over an MMIO page. (RWX)
     * See PGMHandlerPhysicalPageAliasHC(), but this is generally only used for
     * VT-x's APIC access page at the moment.  Treated as MMIO by everyone except
     * the shadow paging code. */
    PGMPAGETYPE_SPECIAL_ALIAS_MMIO,
    /** Shadowed ROM. (RWX) */
    PGMPAGETYPE_ROM_SHADOW,
    /** ROM page. (R-X) */
    PGMPAGETYPE_ROM,
    /** MMIO page. (---) */
    PGMPAGETYPE_MMIO,
    /** End of valid entries. */
    PGMPAGETYPE_END
} PGMPAGETYPE;
AssertCompile(PGMPAGETYPE_END == 8);

/** @name PGM page type predicates.
 * @{ */
#define PGMPAGETYPE_IS_READABLE(a_enmType)  ( (a_enmType) <= PGMPAGETYPE_ROM )
#define PGMPAGETYPE_IS_WRITEABLE(a_enmType) ( (a_enmType) <= PGMPAGETYPE_ROM_SHADOW )
#define PGMPAGETYPE_IS_RWX(a_enmType)       ( (a_enmType) <= PGMPAGETYPE_ROM_SHADOW )
#define PGMPAGETYPE_IS_ROX(a_enmType)       ( (a_enmType) == PGMPAGETYPE_ROM )
#define PGMPAGETYPE_IS_NP(a_enmType)        ( (a_enmType) == PGMPAGETYPE_MMIO )
/** @} */


VMM_INT_DECL(PGMPAGETYPE) PGMPhysGetPageType(PVMCC pVM, RTGCPHYS GCPhys);

VMM_INT_DECL(int)   PGMPhysGCPhys2HCPhys(PVMCC pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys);
VMM_INT_DECL(int)   PGMPhysGCPtr2HCPhys(PVMCPUCC pVCpu, RTGCPTR GCPtr, PRTHCPHYS pHCPhys);
VMM_INT_DECL(int)   PGMPhysGCPhys2CCPtr(PVMCC pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock);
VMM_INT_DECL(int)   PGMPhysGCPhys2CCPtrReadOnly(PVMCC pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock);
VMM_INT_DECL(int)   PGMPhysGCPtr2CCPtr(PVMCPU pVCpu, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock);
VMM_INT_DECL(int)   PGMPhysGCPtr2CCPtrReadOnly(PVMCPUCC pVCpu, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock);

VMMDECL(bool)       PGMPhysIsA20Enabled(PVMCPU pVCpu);
VMMDECL(bool)       PGMPhysIsGCPhysValid(PVMCC pVM, RTGCPHYS GCPhys);
VMMDECL(bool)       PGMPhysIsGCPhysNormal(PVMCC pVM, RTGCPHYS GCPhys);
VMMDECL(int)        PGMPhysGCPtr2GCPhys(PVMCPUCC pVCpu, RTGCPTR GCPtr, PRTGCPHYS pGCPhys);
VMMDECL(void)       PGMPhysReleasePageMappingLock(PVMCC pVM, PPGMPAGEMAPLOCK pLock);
VMMDECL(void)       PGMPhysBulkReleasePageMappingLocks(PVMCC pVM, uint32_t cPages, PPGMPAGEMAPLOCK paLock);

/** @def PGM_PHYS_RW_IS_SUCCESS
 * Check whether a PGMPhysRead, PGMPhysWrite, PGMPhysReadGCPtr or
 * PGMPhysWriteGCPtr call completed the given task.
 *
 * @returns true if completed, false if not.
 * @param   a_rcStrict      The status code.
 * @sa      IOM_SUCCESS
 */
#ifdef IN_RING3
# define PGM_PHYS_RW_IS_SUCCESS(a_rcStrict) \
    (   (a_rcStrict) == VINF_SUCCESS \
     || (a_rcStrict) == VINF_EM_DBG_STOP \
     || (a_rcStrict) == VINF_EM_DBG_EVENT \
     || (a_rcStrict) == VINF_EM_DBG_BREAKPOINT \
    )
#elif defined(IN_RING0)
# define PGM_PHYS_RW_IS_SUCCESS(a_rcStrict) \
    (   (a_rcStrict) == VINF_SUCCESS \
     || (a_rcStrict) == VINF_IOM_R3_MMIO_COMMIT_WRITE \
     || (a_rcStrict) == VINF_EM_OFF \
     || (a_rcStrict) == VINF_EM_SUSPEND \
     || (a_rcStrict) == VINF_EM_RESET \
     || (a_rcStrict) == VINF_EM_HALT \
     || (a_rcStrict) == VINF_EM_DBG_STOP \
     || (a_rcStrict) == VINF_EM_DBG_EVENT \
     || (a_rcStrict) == VINF_EM_DBG_BREAKPOINT \
    )
#elif defined(IN_RC)
# define PGM_PHYS_RW_IS_SUCCESS(a_rcStrict) \
    (   (a_rcStrict) == VINF_SUCCESS \
     || (a_rcStrict) == VINF_IOM_R3_MMIO_COMMIT_WRITE \
     || (a_rcStrict) == VINF_EM_OFF \
     || (a_rcStrict) == VINF_EM_SUSPEND \
     || (a_rcStrict) == VINF_EM_RESET \
     || (a_rcStrict) == VINF_EM_HALT \
     || (a_rcStrict) == VINF_SELM_SYNC_GDT \
     || (a_rcStrict) == VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT \
     || (a_rcStrict) == VINF_EM_DBG_STOP \
     || (a_rcStrict) == VINF_EM_DBG_EVENT \
     || (a_rcStrict) == VINF_EM_DBG_BREAKPOINT \
    )
#endif
/** @def PGM_PHYS_RW_DO_UPDATE_STRICT_RC
 * Updates the return code with a new result.
 *
 * Both status codes must be successes according to PGM_PHYS_RW_IS_SUCCESS.
 *
 * @param   a_rcStrict      The current return code, to be updated.
 * @param   a_rcStrict2     The new return code to merge in.
 */
#ifdef IN_RING3
# define PGM_PHYS_RW_DO_UPDATE_STRICT_RC(a_rcStrict, a_rcStrict2) \
    do { \
        Assert(rcStrict == VINF_SUCCESS); \
        Assert(rcStrict2 == VINF_SUCCESS); \
    } while (0)
#elif defined(IN_RING0)
# define PGM_PHYS_RW_DO_UPDATE_STRICT_RC(a_rcStrict, a_rcStrict2) \
    do { \
        Assert(PGM_PHYS_RW_IS_SUCCESS(rcStrict)); \
        Assert(PGM_PHYS_RW_IS_SUCCESS(rcStrict2)); \
        AssertCompile(VINF_IOM_R3_MMIO_COMMIT_WRITE > VINF_EM_LAST); \
        if ((a_rcStrict2) == VINF_SUCCESS || (a_rcStrict) == (a_rcStrict2)) \
        { /* likely */ } \
        else if (   (a_rcStrict) == VINF_SUCCESS \
                 || (a_rcStrict) > (a_rcStrict2)) \
            (a_rcStrict) = (a_rcStrict2); \
    } while (0)
#elif defined(IN_RC)
# define PGM_PHYS_RW_DO_UPDATE_STRICT_RC(a_rcStrict, a_rcStrict2) \
    do { \
        Assert(PGM_PHYS_RW_IS_SUCCESS(rcStrict)); \
        Assert(PGM_PHYS_RW_IS_SUCCESS(rcStrict2)); \
        AssertCompile(VINF_SELM_SYNC_GDT > VINF_EM_LAST); \
        AssertCompile(VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT > VINF_EM_LAST); \
        AssertCompile(VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT < VINF_SELM_SYNC_GDT); \
        AssertCompile(VINF_IOM_R3_MMIO_COMMIT_WRITE > VINF_EM_LAST); \
        AssertCompile(VINF_IOM_R3_MMIO_COMMIT_WRITE > VINF_SELM_SYNC_GDT); \
        AssertCompile(VINF_IOM_R3_MMIO_COMMIT_WRITE > VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT); \
        if ((a_rcStrict2) == VINF_SUCCESS || (a_rcStrict) == (a_rcStrict2)) \
        { /* likely */ } \
        else if ((a_rcStrict) == VINF_SUCCESS) \
            (a_rcStrict) = (a_rcStrict2); \
        else if (   (   (a_rcStrict) > (a_rcStrict2) \
                     && (   (a_rcStrict2) <= VINF_EM_RESET  \
                         || (a_rcStrict) != VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT) ) \
                 || (   (a_rcStrict2) == VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT \
                     && (a_rcStrict) > VINF_EM_RESET) ) \
            (a_rcStrict) = (a_rcStrict2); \
    } while (0)
#endif

VMMDECL(VBOXSTRICTRC) PGMPhysRead(PVMCC pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead, PGMACCESSORIGIN enmOrigin);
VMMDECL(VBOXSTRICTRC) PGMPhysWrite(PVMCC pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, PGMACCESSORIGIN enmOrigin);
VMMDECL(VBOXSTRICTRC) PGMPhysReadGCPtr(PVMCPUCC pVCpu, void *pvDst, RTGCPTR GCPtrSrc, size_t cb, PGMACCESSORIGIN enmOrigin);
VMMDECL(VBOXSTRICTRC) PGMPhysWriteGCPtr(PVMCPUCC pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb, PGMACCESSORIGIN enmOrigin);

VMMDECL(int)        PGMPhysSimpleReadGCPhys(PVMCC pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb);
VMMDECL(int)        PGMPhysSimpleWriteGCPhys(PVMCC pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb);
VMMDECL(int)        PGMPhysSimpleReadGCPtr(PVMCPUCC pVCpu, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);
VMMDECL(int)        PGMPhysSimpleWriteGCPtr(PVMCPUCC pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);
VMMDECL(int)        PGMPhysSimpleDirtyWriteGCPtr(PVMCPUCC pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb);
VMMDECL(int)        PGMPhysInterpretedRead(PVMCPUCC pVCpu, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCPTR GCPtrSrc, size_t cb);
VMMDECL(int)        PGMPhysInterpretedReadNoHandlers(PVMCPUCC pVCpu, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb, bool fRaiseTrap);
VMMDECL(int)        PGMPhysInterpretedWriteNoHandlers(PVMCPUCC pVCpu, PCPUMCTXCORE pCtxCore, RTGCPTR GCPtrDst, void const *pvSrc, size_t cb, bool fRaiseTrap);

VMM_INT_DECL(int)   PGMPhysIemGCPhys2Ptr(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys, bool fWritable, bool fByPassHandlers, void **ppv, PPGMPAGEMAPLOCK pLock);
VMM_INT_DECL(int)   PGMPhysIemQueryAccess(PVMCC pVM, RTGCPHYS GCPhys, bool fWritable, bool fByPassHandlers);
VMM_INT_DECL(int)   PGMPhysIemGCPhys2PtrNoLock(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys, uint64_t const volatile *puTlbPhysRev,
#if defined(IN_RC)
                                               R3PTRTYPE(uint8_t *) *ppb,
#else
                                               R3R0PTRTYPE(uint8_t *) *ppb,
#endif
                                               uint64_t *pfTlb);
/** @name Flags returned by PGMPhysIemGCPhys2PtrNoLock
 * @{ */
#define PGMIEMGCPHYS2PTR_F_NO_WRITE     RT_BIT_32(3)    /**< Not writable (IEMTLBE_F_PG_NO_WRITE). */
#define PGMIEMGCPHYS2PTR_F_NO_READ      RT_BIT_32(4)    /**< Not readable (IEMTLBE_F_PG_NO_READ). */
#define PGMIEMGCPHYS2PTR_F_NO_MAPPINGR3 RT_BIT_32(7)    /**< No ring-3 mapping (IEMTLBE_F_NO_MAPPINGR3). */
/** @} */

/** Information returned by PGMPhysNemQueryPageInfo. */
typedef struct PGMPHYSNEMPAGEINFO
{
    /** The host physical address of the page, NIL_HCPHYS if invalid page. */
    RTHCPHYS            HCPhys;
    /** The NEM access mode for the page, NEM_PAGE_PROT_XXX  */
    uint32_t            fNemProt : 8;
    /** The NEM state associated with the PAGE. */
    uint32_t            u2NemState : 2;
    /** The NEM state associated with the PAGE before pgmPhysPageMakeWritable was called. */
    uint32_t            u2OldNemState : 2;
    /** Set if the page has handler. */
    uint32_t            fHasHandlers : 1;
    /** Set if is the zero page backing it. */
    uint32_t            fZeroPage : 1;
    /** Set if the page has handler. */
    PGMPAGETYPE         enmType;
} PGMPHYSNEMPAGEINFO;
/** Pointer to page information for NEM. */
typedef PGMPHYSNEMPAGEINFO *PPGMPHYSNEMPAGEINFO;
/**
 * Callback for checking that the page is in sync while under the PGM lock.
 *
 * NEM passes this callback to PGMPhysNemQueryPageInfo to check that the page is
 * in-sync between PGM and the native hypervisor API in an atomic fashion.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context per virtual CPU structure.  Optional,
 *                      see PGMPhysNemQueryPageInfo.
 * @param   GCPhys      The guest physical address (not A20 masked).
 * @param   pInfo       The page info structure.  This function updates the
 *                      u2NemState memory and the caller will update the PGMPAGE
 *                      copy accordingly.
 * @param   pvUser      Callback user argument.
 */
typedef DECLCALLBACKTYPE(int, FNPGMPHYSNEMCHECKPAGE,(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys, PPGMPHYSNEMPAGEINFO pInfo, void *pvUser));
/** Pointer to a FNPGMPHYSNEMCHECKPAGE function. */
typedef FNPGMPHYSNEMCHECKPAGE *PFNPGMPHYSNEMCHECKPAGE;

VMM_INT_DECL(int)   PGMPhysNemPageInfoChecker(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys, bool fMakeWritable,
                                              PPGMPHYSNEMPAGEINFO pInfo, PFNPGMPHYSNEMCHECKPAGE pfnChecker, void *pvUser);

/**
 * Callback for use with PGMPhysNemEnumPagesByState.
 * @returns VBox status code.
 *          Failure status will stop enumeration immediately and return.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context per virtual CPU structure.  Optional,
 *                      see PGMPhysNemEnumPagesByState.
 * @param   GCPhys      The guest physical address (not A20 masked).
 * @param   pu2NemState Pointer to variable with the NEM state.  This can be
 *                      update.
 * @param   pvUser      The user argument.
 */
typedef DECLCALLBACKTYPE(int, FNPGMPHYSNEMENUMCALLBACK,(PVMCC pVM, PVMCPUCC pVCpu, RTGCPHYS GCPhys,
                                                        uint8_t *pu2NemState, void *pvUser));
/** Pointer to a FNPGMPHYSNEMENUMCALLBACK function. */
typedef FNPGMPHYSNEMENUMCALLBACK *PFNPGMPHYSNEMENUMCALLBACK;
VMM_INT_DECL(int) PGMPhysNemEnumPagesByState(PVMCC pVM, PVMCPUCC VCpu, uint8_t uMinState,
                                             PFNPGMPHYSNEMENUMCALLBACK pfnCallback, void *pvUser);


#ifdef VBOX_STRICT
VMMDECL(unsigned)   PGMAssertHandlerAndFlagsInSync(PVMCC pVM);
VMMDECL(unsigned)   PGMAssertNoMappingConflicts(PVM pVM);
VMMDECL(unsigned)   PGMAssertCR3(PVMCC pVM, PVMCPUCC pVCpu, uint64_t cr3, uint64_t cr4);
#endif /* VBOX_STRICT */

VMMDECL(int)        PGMSetLargePageUsage(PVMCC pVM, bool fUseLargePages);

/**
 * Query large page usage state
 *
 * @returns 0 - disabled, 1 - enabled
 * @param   pVM         The cross context VM structure.
 */
#define PGMIsUsingLargePages(pVM)   ((pVM)->pgm.s.fUseLargePages)


#ifdef IN_RING0
/** @defgroup grp_pgm_r0  The PGM Host Context Ring-0 API
 * @{
 */
VMMR0_INT_DECL(int)  PGMR0InitPerVMData(PGVM pGVM);
VMMR0_INT_DECL(int)  PGMR0InitVM(PGVM pGVM);
VMMR0_INT_DECL(void) PGMR0CleanupVM(PGVM pGVM);
VMMR0_INT_DECL(int)  PGMR0PhysAllocateHandyPages(PGVM pGVM, VMCPUID idCpu);
VMMR0_INT_DECL(int)  PGMR0PhysFlushHandyPages(PGVM pGVM, VMCPUID idCpu);
VMMR0_INT_DECL(int)  PGMR0PhysAllocateLargeHandyPage(PGVM pGVM, VMCPUID idCpu);
VMMR0_INT_DECL(int)  PGMR0PhysMMIO2MapKernel(PGVM pGVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2,
                                             size_t offSub, size_t cbSub, void **ppvMapping);
VMMR0_INT_DECL(int)  PGMR0PhysSetupIoMmu(PGVM pGVM);
VMMR0DECL(int)       PGMR0SharedModuleCheck(PVMCC pVM, PGVM pGVM, VMCPUID idCpu, PGMMSHAREDMODULE pModule,
                                            PCRTGCPTR64 paRegionsGCPtrs);
VMMR0DECL(int)       PGMR0Trap0eHandlerNestedPaging(PGVM pGVM, PGVMCPU pGVCpu, PGMMODE enmShwPagingMode, RTGCUINT uErr,
                                                    PCPUMCTXCORE pRegFrame, RTGCPHYS pvFault);
VMMR0DECL(VBOXSTRICTRC) PGMR0Trap0eHandlerNPMisconfig(PGVM pGVM, PGVMCPU pGVCpu, PGMMODE enmShwPagingMode,
                                                      PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, uint32_t uErr);
VMMR0_INT_DECL(int)  PGMR0PoolGrow(PGVM pGVM, VMCPUID idCpu);
/** @} */
#endif /* IN_RING0 */



#ifdef IN_RING3
/** @defgroup grp_pgm_r3  The PGM Host Context Ring-3 API
 * @{
 */
VMMR3_INT_DECL(void)    PGMR3EnableNemMode(PVM pVM);
VMMR3DECL(int)      PGMR3Init(PVM pVM);
VMMR3DECL(int)      PGMR3InitFinalize(PVM pVM);
VMMR3_INT_DECL(int) PGMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3DECL(void)     PGMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3DECL(void)     PGMR3ResetCpu(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(void)    PGMR3Reset(PVM pVM);
VMMR3_INT_DECL(void)    PGMR3ResetNoMorePhysWritesFlag(PVM pVM);
VMMR3_INT_DECL(void)    PGMR3MemSetup(PVM pVM, bool fReset);
VMMR3DECL(int)      PGMR3Term(PVM pVM);

VMMR3DECL(int)      PGMR3PhysRegisterRam(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, const char *pszDesc);
VMMR3DECL(int)      PGMR3PhysChangeMemBalloon(PVM pVM, bool fInflate, unsigned cPages, RTGCPHYS *paPhysPage);
VMMR3DECL(int)      PGMR3PhysWriteProtectRAM(PVM pVM);
VMMR3DECL(uint32_t) PGMR3PhysGetRamRangeCount(PVM pVM);
VMMR3DECL(int)      PGMR3PhysGetRange(PVM pVM, uint32_t iRange, PRTGCPHYS pGCPhysStart, PRTGCPHYS pGCPhysLast,
                                      const char **ppszDesc, bool *pfIsMmio);
VMMR3DECL(int)      PGMR3QueryMemoryStats(PUVM pUVM, uint64_t *pcbTotalMem, uint64_t *pcbPrivateMem, uint64_t *pcbSharedMem, uint64_t *pcbZeroMem);
VMMR3DECL(int)      PGMR3QueryGlobalMemoryStats(PUVM pUVM, uint64_t *pcbAllocMem, uint64_t *pcbFreeMem, uint64_t *pcbBallonedMem, uint64_t *pcbSharedMem);

VMMR3DECL(int)      PGMR3PhysMMIORegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, PGMPHYSHANDLERTYPE hType,
                                          RTR3PTR pvUserR3, RTR0PTR pvUserR0, RTRCPTR pvUserRC, const char *pszDesc);
VMMR3DECL(int)      PGMR3PhysMMIODeregister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb);

/** @name PGMPHYS_MMIO2_FLAGS_XXX - MMIO2 registration flags.
 * @see PGMR3PhysMmio2Register, PDMDevHlpMmio2Create
 * @{ */
/** Track dirty pages.
 * @see PGMR3PhysMmio2QueryAndResetDirtyBitmap(), PGMR3PhysMmio2ControlDirtyPageTracking(). */
#define PGMPHYS_MMIO2_FLAGS_TRACK_DIRTY_PAGES       RT_BIT_32(0)
/** Valid flags. */
#define PGMPHYS_MMIO2_FLAGS_VALID_MASK              UINT32_C(0x00000001)
/** @} */

VMMR3_INT_DECL(int) PGMR3PhysMmio2Register(PVM pVM, PPDMDEVINS pDevIns, uint32_t iSubDev, uint32_t iRegion, RTGCPHYS cb,
                                           uint32_t fFlags, const char *pszDesc, void **ppv, PGMMMIO2HANDLE *phRegion);
VMMR3_INT_DECL(int) PGMR3PhysMmio2Deregister(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2);
VMMR3_INT_DECL(int) PGMR3PhysMmio2Map(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2, RTGCPHYS GCPhys);
VMMR3_INT_DECL(int) PGMR3PhysMmio2Unmap(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2, RTGCPHYS GCPhys);
VMMR3_INT_DECL(int) PGMR3PhysMmio2Reduce(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2, RTGCPHYS cbRegion);
VMMR3_INT_DECL(int) PGMR3PhysMmio2ValidateHandle(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2);
VMMR3_INT_DECL(RTGCPHYS) PGMR3PhysMmio2GetMappingAddress(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2);
VMMR3_INT_DECL(int) PGMR3PhysMmio2ChangeRegionNo(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2, uint32_t iNewRegion);
VMMR3_INT_DECL(int) PGMR3PhysMmio2QueryAndResetDirtyBitmap(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2,
                                                           void *pvBitmap, size_t cbBitmap);
VMMR3_INT_DECL(int) PGMR3PhysMmio2ControlDirtyPageTracking(PVM pVM, PPDMDEVINS pDevIns, PGMMMIO2HANDLE hMmio2, bool fEnabled);

/** @name PGMPHYS_ROM_FLAGS_XXX - ROM registration flags.
 * @see PGMR3PhysRegisterRom, PDMDevHlpROMRegister
 * @{ */
/** Inidicates that ROM shadowing should be enabled. */
#define PGMPHYS_ROM_FLAGS_SHADOWED                  UINT8_C(0x01)
/** Indicates that what pvBinary points to won't go away
 * and can be used for strictness checks. */
#define PGMPHYS_ROM_FLAGS_PERMANENT_BINARY          UINT8_C(0x02)
/** Indicates that the ROM is allowed to be missing from saved state.
 * @note This is a hack for EFI, see @bugref{6940}   */
#define PGMPHYS_ROM_FLAGS_MAYBE_MISSING_FROM_STATE  UINT8_C(0x04)
/** Valid flags.   */
#define PGMPHYS_ROM_FLAGS_VALID_MASK                UINT8_C(0x07)
/** @} */

VMMR3DECL(int)      PGMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPHYS cb,
                                         const void *pvBinary, uint32_t cbBinary, uint8_t fFlags, const char *pszDesc);
VMMR3DECL(int)      PGMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, PGMROMPROT enmProt);
VMMDECL(void)       PGMR3PhysSetA20(PVMCPU pVCpu, bool fEnable);

VMMR3_INT_DECL(int) PGMR3HandlerPhysicalTypeRegisterEx(PVM pVM, PGMPHYSHANDLERKIND enmKind, bool fKeepPgmLock,
                                                       PFNPGMPHYSHANDLER pfnHandlerR3,
                                                       R0PTRTYPE(PFNPGMPHYSHANDLER)     pfnHandlerR0,
                                                       R0PTRTYPE(PFNPGMRZPHYSPFHANDLER) pfnPfHandlerR0,
                                                       const char *pszDesc, PPGMPHYSHANDLERTYPE phType);
VMMR3DECL(int)      PGMR3HandlerPhysicalTypeRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind, bool fKeepPgmLock,
                                                     R3PTRTYPE(PFNPGMPHYSHANDLER) pfnHandlerR3,
                                                     const char *pszModR0, const char *pszHandlerR0, const char *pszPfHandlerR0,
                                                     const char *pszModRC, const char *pszHandlerRC, const char *pszPfHandlerRC,
                                                     const char *pszDesc,
                                                     PPGMPHYSHANDLERTYPE phType);
VMMR3_INT_DECL(int) PGMR3PoolGrow(PVM pVM, PVMCPU pVCpu);

VMMR3DECL(int)      PGMR3PhysTlbGCPhys2Ptr(PVM pVM, RTGCPHYS GCPhys, bool fWritable, void **ppv);
VMMR3DECL(uint8_t)  PGMR3PhysReadU8(PVM pVM, RTGCPHYS GCPhys, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(uint16_t) PGMR3PhysReadU16(PVM pVM, RTGCPHYS GCPhys, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(uint32_t) PGMR3PhysReadU32(PVM pVM, RTGCPHYS GCPhys, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(uint64_t) PGMR3PhysReadU64(PVM pVM, RTGCPHYS GCPhys, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(void)     PGMR3PhysWriteU8(PVM pVM, RTGCPHYS GCPhys, uint8_t Value, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(void)     PGMR3PhysWriteU16(PVM pVM, RTGCPHYS GCPhys, uint16_t Value, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(void)     PGMR3PhysWriteU32(PVM pVM, RTGCPHYS GCPhys, uint32_t Value, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(void)     PGMR3PhysWriteU64(PVM pVM, RTGCPHYS GCPhys, uint64_t Value, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(int)      PGMR3PhysReadExternal(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(int)      PGMR3PhysWriteExternal(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite, PGMACCESSORIGIN enmOrigin);
VMMR3DECL(int)      PGMR3PhysGCPhys2CCPtrExternal(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock);
VMMR3DECL(int)      PGMR3PhysGCPhys2CCPtrReadOnlyExternal(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock);
VMMR3DECL(int)      PGMR3PhysBulkGCPhys2CCPtrExternal(PVM pVM, uint32_t cPages, PCRTGCPHYS paGCPhysPages,
                                                      void **papvPages, PPGMPAGEMAPLOCK paLocks);
VMMR3DECL(int)      PGMR3PhysBulkGCPhys2CCPtrReadOnlyExternal(PVM pVM, uint32_t cPages, PCRTGCPHYS paGCPhysPages,
                                                              void const **papvPages, PPGMPAGEMAPLOCK paLocks);
VMMR3DECL(void)     PGMR3PhysChunkInvalidateTLB(PVM pVM);
VMMR3DECL(int)      PGMR3PhysAllocateHandyPages(PVM pVM);
VMMR3_INT_DECL(int) PGMR3PhysAllocateLargePage(PVM pVM, RTGCPHYS GCPhys);

VMMR3DECL(int)      PGMR3CheckIntegrity(PVM pVM);

VMMR3DECL(int)      PGMR3DbgR3Ptr2GCPhys(PUVM pUVM, RTR3PTR R3Ptr, PRTGCPHYS pGCPhys);
VMMR3DECL(int)      PGMR3DbgR3Ptr2HCPhys(PUVM pUVM, RTR3PTR R3Ptr, PRTHCPHYS pHCPhys);
VMMR3DECL(int)      PGMR3DbgHCPhys2GCPhys(PUVM pUVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys);
VMMR3_INT_DECL(int) PGMR3DbgReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb, uint32_t fFlags, size_t *pcbRead);
VMMR3_INT_DECL(int) PGMR3DbgWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb, uint32_t fFlags, size_t *pcbWritten);
VMMR3_INT_DECL(int) PGMR3DbgReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb, uint32_t fFlags, size_t *pcbRead);
VMMR3_INT_DECL(int) PGMR3DbgWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, void const *pvSrc, size_t cb, uint32_t fFlags, size_t *pcbWritten);
VMMR3_INT_DECL(int) PGMR3DbgScanPhysical(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cbRange, RTGCPHYS GCPhysAlign, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCPHYS pGCPhysHit);
VMMR3_INT_DECL(int) PGMR3DbgScanVirtual(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, RTGCPTR cbRange, RTGCPTR GCPtrAlign, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCUINTPTR pGCPhysHit);
VMMR3_INT_DECL(int) PGMR3DumpHierarchyShw(PVM pVM, uint64_t cr3, uint32_t fFlags, uint64_t u64FirstAddr, uint64_t u64LastAddr, uint32_t cMaxDepth, PCDBGFINFOHLP pHlp);
VMMR3_INT_DECL(int) PGMR3DumpHierarchyGst(PVM pVM, uint64_t cr3, uint32_t fFlags, RTGCPTR FirstAddr, RTGCPTR LastAddr, uint32_t cMaxDepth, PCDBGFINFOHLP pHlp);


/** @name Page sharing
 * @{ */
VMMR3DECL(int)     PGMR3SharedModuleRegister(PVM pVM, VBOXOSFAMILY enmGuestOS, char *pszModuleName, char *pszVersion,
                                             RTGCPTR GCBaseAddr, uint32_t cbModule,
                                             uint32_t cRegions, VMMDEVSHAREDREGIONDESC const *paRegions);
VMMR3DECL(int)     PGMR3SharedModuleUnregister(PVM pVM, char *pszModuleName, char *pszVersion,
                                               RTGCPTR GCBaseAddr, uint32_t cbModule);
VMMR3DECL(int)     PGMR3SharedModuleCheckAll(PVM pVM);
VMMR3DECL(int)     PGMR3SharedModuleGetPageState(PVM pVM, RTGCPTR GCPtrPage, bool *pfShared, uint64_t *pfPageFlags);
/** @} */

/** @} */
#endif /* IN_RING3 */

RT_C_DECLS_END

/** @} */
#endif /* !VBOX_INCLUDED_vmm_pgm_h */

