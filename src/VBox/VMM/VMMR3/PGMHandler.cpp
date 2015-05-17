/* $Id$ */
/** @file
 * PGM - Page Manager / Monitor, Access Handlers.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/iom.h>
#include <VBox/sup.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/csam.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/dbgf.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/selm.h>
#include <VBox/vmm/ssm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"
#include <VBox/dbg.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/vmm/hm.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneClear(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneSet(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3InfoHandlersPhysicalOne(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3InfoHandlersVirtualOne(PAVLROGCPTRNODECORE pNode, void *pvUser);




/**
 * Register a physical page access handler type, extended version.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   enmKind         The kind of access handler.
 * @param   pfnHandlerR3    Pointer to the ring-3 handler callback.
 * @param   pfnHandlerR0    Pointer to the ring-0 handler callback.
 * @param   pfnHandlerRC    Pointer to the raw-mode context handler callback.
 * @param   pszDesc         The type description.
 * @param   phType          Where to return the type handle (cross context
 *                          safe).
 */
VMMR3_INT_DECL(int) PGMR3HandlerPhysicalTypeRegisterEx(PVM pVM, PGMPHYSHANDLERKIND enmKind,
                                                       PFNPGMR3PHYSHANDLER pfnHandlerR3,
                                                       R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0,
                                                       RCPTRTYPE(PFNPGMRCPHYSHANDLER) pfnHandlerRC,
                                                       const char *pszDesc, PPGMPHYSHANDLERTYPE phType)
{
    AssertPtrReturn(pfnHandlerR3, VERR_INVALID_POINTER);
    AssertReturn(pfnHandlerR0 != NIL_RTR0PTR, VERR_INVALID_POINTER);
    AssertReturn(pfnHandlerRC != NIL_RTRCPTR || HMIsEnabled(pVM), VERR_INVALID_POINTER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(   enmKind == PGMPHYSHANDLERKIND_WRITE
                 || enmKind == PGMPHYSHANDLERKIND_ALL
                 || enmKind == PGMPHYSHANDLERKIND_MMIO,
                 VERR_INVALID_PARAMETER);

    PPGMPHYSHANDLERTYPEINT pType;
    int rc = MMHyperAlloc(pVM, sizeof(*pType), 0, MM_TAG_PGM_HANDLER_TYPES, (void **)&pType);
    if (RT_SUCCESS(rc))
    {
        pType->u32Magic     = PGMPHYSHANDLERTYPEINT_MAGIC;
        pType->cRefs        = 1;
        pType->enmKind      = enmKind;
        pType->uState       = enmKind == PGMPHYSHANDLERKIND_WRITE ? PGM_PAGE_HNDL_PHYS_STATE_WRITE : PGM_PAGE_HNDL_PHYS_STATE_ALL;
        pType->pfnHandlerR3 = pfnHandlerR3;
        pType->pfnHandlerR0 = pfnHandlerR0;
        pType->pfnHandlerRC = pfnHandlerRC;
        pType->pszDesc      = pszDesc;

        pgmLock(pVM);
        RTListOff32Append(&pVM->pgm.s.CTX_SUFF(pTrees)->HeadPhysHandlerTypes, &pType->ListNode);
        pgmUnlock(pVM);

        *phType = MMHyperHeapPtrToOffset(pVM, pType);
        LogFlow(("PGMR3HandlerPhysicalTypeRegisterEx: %p/%#x: enmKind=%d pfnHandlerR3=%RHv pfnHandlerR0=%RHv pfnHandlerRC=%RRv pszDesc=%s\n",
                 pType, *phType, enmKind, pfnHandlerR3, pfnHandlerR0, pfnHandlerRC, pszDesc));
        return VINF_SUCCESS;
    }
    *phType = NIL_PGMPHYSHANDLERTYPE;
    return rc;
}


/**
 * Register a physical page access handler type.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   enmKind         The kind of access handler.
 * @param   pfnHandlerR3    Pointer to the ring-3 handler callback.
 * @param   pszModR0        The name of the ring-0 module, NULL is an alias for
 *                          the main ring-0 module.
 * @param   pszHandlerR0    The name of the ring-0 handler, NULL if the ring-3
 *                          handler should be called.
 * @param   pszModRC        The name of the raw-mode context module, NULL is an
 *                          alias for the main RC module.
 * @param   pszHandlerRC    The name of the raw-mode context handler, NULL if
 *                          the ring-3 handler should be called.
 * @param   pszDesc         The type description.
 * @param   phType          Where to return the type handle (cross context
 *                          safe).
 */
VMMR3DECL(int) PGMR3HandlerPhysicalTypeRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind,
                                                R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3,
                                                const char *pszModR0, const char *pszHandlerR0,
                                                const char *pszModRC, const char *pszHandlerRC, const char *pszDesc,
                                                PPGMPHYSHANDLERTYPE phType)
{
    LogFlow(("PGMR3HandlerPhysicalTypeRegister: enmKind=%d pfnHandlerR3=%RHv pszModR0=%s pszHandlerR0=%s pszModRC=%s pszHandlerRC=%s pszDesc=%s\n",
             enmKind, pfnHandlerR3, pszModR0, pszHandlerR0, pszHandlerRC, pszModRC, pszDesc));

    /*
     * Validate input.
     */
    if (!pszModRC)
        pszModRC = VMMGC_MAIN_MODULE_NAME;
    if (!pszModR0)
        pszModR0 = VMMR0_MAIN_MODULE_NAME;
    if (!pszHandlerR0)
        pszHandlerR0 = "pgmPhysHandlerRedirectToHC";
    if (!pszHandlerRC)
        pszHandlerRC = "pgmPhysHandlerRedirectToHC";
    AssertPtrReturn(pfnHandlerR3, VERR_INVALID_POINTER);
    AssertPtrReturn(pszHandlerR0, VERR_INVALID_POINTER);
    AssertPtrReturn(pszHandlerRC, VERR_INVALID_POINTER);

    /*
     * Resolve the R0 handler.
     */
    R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0 = NIL_RTR0PTR;
    int rc = PDMR3LdrGetSymbolR0Lazy(pVM, pszModR0, NULL /*pszSearchPath*/, pszHandlerR0, &pfnHandlerR0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Resolve the GC handler.
         */
        RTRCPTR pfnHandlerRC = NIL_RTRCPTR;
        if (!HMIsEnabled(pVM))
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pszModRC, NULL /*pszSearchPath*/, pszHandlerRC, &pfnHandlerRC);
        if (RT_SUCCESS(rc))
            return PGMR3HandlerPhysicalTypeRegisterEx(pVM, enmKind, pfnHandlerR3, pfnHandlerR0, pfnHandlerRC, pszDesc, phType);

        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModRC, pszHandlerRC, rc));
    }
    else
        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModR0, pszHandlerR0, rc));

    return rc;
}


/**
 * Updates the physical page access handlers.
 *
 * @param   pVM     Pointer to the VM.
 * @remark  Only used when restoring a saved state.
 */
void pgmR3HandlerPhysicalUpdateAll(PVM pVM)
{
    LogFlow(("pgmHandlerPhysicalUpdateAll:\n"));

    /*
     * Clear and set.
     * (the right -> left on the setting pass is just bird speculating on cache hits)
     */
    pgmLock(pVM);
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers,  true, pgmR3HandlerPhysicalOneClear, pVM);
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, false, pgmR3HandlerPhysicalOneSet, pVM);
    pgmUnlock(pVM);
}


/**
 * Clears all the page level flags for one physical handler range.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMPHYSHANDLER.
 * @param   pvUser  Pointer to the VM.
 */
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneClear(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMPHYSHANDLER pCur     = (PPGMPHYSHANDLER)pNode;
    PPGMRAMRANGE    pRamHint = NULL;
    RTGCPHYS        GCPhys   = pCur->Core.Key;
    RTUINT          cPages   = pCur->cPages;
    PVM             pVM      = (PVM)pvUser;
    for (;;)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pPage, &pRamHint);
        if (RT_SUCCESS(rc))
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_NONE);
        else
            AssertRC(rc);

        if (--cPages == 0)
            return 0;
        GCPhys += PAGE_SIZE;
    }
}


/**
 * Sets all the page level flags for one physical handler range.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMPHYSHANDLER.
 * @param   pvUser  Pointer to the VM.
 */
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneSet(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PVM                     pVM       = (PVM)pvUser;
    PPGMPHYSHANDLER         pCur      = (PPGMPHYSHANDLER)pNode;
    PPGMPHYSHANDLERTYPEINT  pCurType  = PGMPHYSHANDLER_GET_TYPE(pVM, pCur);
    unsigned                uState    = pCurType->uState;
    PPGMRAMRANGE            pRamHint  = NULL;
    RTGCPHYS                GCPhys    = pCur->Core.Key;
    RTUINT                  cPages    = pCur->cPages;
    for (;;)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pPage, &pRamHint);
        if (RT_SUCCESS(rc))
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, uState);
        else
            AssertRC(rc);

        if (--cPages == 0)
            return 0;
        GCPhys += PAGE_SIZE;
    }
}


/**
 * Register a virtual page access handler type, extended version.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   enmKind         The kind of access handler.
 * @param   fRelocUserRC    Whether the pvUserRC argument should be
 *                          automatically relocated or not.
 * @param   pfnInvalidateR3 Pointer to the ring-3 invalidation handler callback.
 * @param   pfnHandlerR3    Pointer to the ring-3 handler callback.
 * @param   pfnHandlerRC    Pointer to the raw-mode context handler callback.
 * @param   pszDesc         The type description.
 * @param   phType          Where to return the type handle (cross context
 *                          safe).
 * @remarks No virtual handlers when executing using HM (i.e. ring-0).
 */
VMMR3_INT_DECL(int) PGMR3HandlerVirtualTypeRegisterEx(PVM pVM, PGMVIRTHANDLERKIND enmKind, bool fRelocUserRC,
                                                      PFNPGMR3VIRTINVALIDATE pfnInvalidateR3,
                                                      PFNPGMR3VIRTHANDLER pfnHandlerR3,
                                                      RCPTRTYPE(PFNPGMRCVIRTHANDLER) pfnHandlerRC,
                                                      const char *pszDesc, PPGMVIRTHANDLERTYPE phType)
{
    AssertReturn(!HMIsEnabled(pVM), VERR_NOT_AVAILABLE); /* Not supported/relevant for VT-x and AMD-V. */
    AssertReturn(RT_VALID_PTR(pfnHandlerR3) || enmKind == PGMVIRTHANDLERKIND_HYPERVISOR, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnInvalidateR3, VERR_INVALID_POINTER);
    AssertReturn(pfnHandlerRC != NIL_RTRCPTR, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(   enmKind == PGMVIRTHANDLERKIND_WRITE
                 || enmKind == PGMVIRTHANDLERKIND_ALL
                 || enmKind == PGMVIRTHANDLERKIND_HYPERVISOR,
                 VERR_INVALID_PARAMETER);

    PPGMVIRTHANDLERTYPEINT pType;
    int rc = MMHyperAlloc(pVM, sizeof(*pType), 0, MM_TAG_PGM_HANDLER_TYPES, (void **)&pType);
    if (RT_SUCCESS(rc))
    {
        pType->u32Magic         = PGMVIRTHANDLERTYPEINT_MAGIC;
        pType->cRefs            = 1;
        pType->enmKind          = enmKind;
        pType->fRelocUserRC     = fRelocUserRC;
        pType->uState           = enmKind == PGMVIRTHANDLERKIND_ALL
                                ? PGM_PAGE_HNDL_VIRT_STATE_ALL : PGM_PAGE_HNDL_VIRT_STATE_WRITE;
        pType->pfnInvalidateR3  = pfnInvalidateR3;
        pType->pfnHandlerR3     = pfnHandlerR3;
        pType->pfnHandlerRC     = pfnHandlerRC;
        pType->pszDesc          = pszDesc;

        pgmLock(pVM);
        RTListOff32Append(&pVM->pgm.s.CTX_SUFF(pTrees)->HeadVirtHandlerTypes, &pType->ListNode);
        pgmUnlock(pVM);

        *phType = MMHyperHeapPtrToOffset(pVM, pType);
        LogFlow(("PGMR3HandlerVirtualTypeRegisterEx: %p/%#x: enmKind=%d pfnInvalidateR3=%RHv pfnHandlerR3=%RHv pfnHandlerRC=%RRv pszDesc=%s\n",
                 pType, *phType, enmKind, pfnInvalidateR3, pfnHandlerR3, pfnHandlerRC, pszDesc));
        return VINF_SUCCESS;
    }
    *phType = NIL_PGMVIRTHANDLERTYPE;
    return rc;
}


/**
 * Register a physical page access handler type.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the cross context VM structure.
 * @param   enmKind         The kind of access handler.
 * @param   fRelocUserRC    Whether the pvUserRC argument should be
 *                          automatically relocated or not.
 * @param   pfnInvalidateR3 Pointer to the ring-3 invalidateion callback
 *                          (optional, can be NULL).
 * @param   pfnHandlerR3    Pointer to the ring-3 handler callback.
 * @param   pszModRC        The name of the raw-mode context module, NULL is an
 *                          alias for the main RC module.
 * @param   pszHandlerRC    The name of the raw-mode context handler, NULL if
 *                          the ring-3 handler should be called.
 * @param   pszDesc         The type description.
 * @param   phType          Where to return the type handle (cross context
 *                          safe).
 * @remarks No virtual handlers when executing using HM (i.e. ring-0).
 */
VMMR3_INT_DECL(int) PGMR3HandlerVirtualTypeRegister(PVM pVM, PGMVIRTHANDLERKIND enmKind, bool fRelocUserRC,
                                                    PFNPGMR3VIRTINVALIDATE pfnInvalidateR3,
                                                    PFNPGMR3VIRTHANDLER pfnHandlerR3,
                                                    const char *pszHandlerRC, const char *pszModRC, const char *pszDesc,
                                                    PPGMVIRTHANDLERTYPE phType)
{
    LogFlow(("PGMR3HandlerVirtualTypeRegister: enmKind=%d pfnInvalidateR3=%RHv pfnHandlerR3=%RHv pszModRC=%s pszHandlerRC=%s pszDesc=%s\n",
             enmKind, pfnInvalidateR3, pfnHandlerR3, pszHandlerRC, pszModRC, pszDesc));

    /*
     * Validate input.
     */
    if (!pszModRC)
        pszModRC = VMMGC_MAIN_MODULE_NAME;
    if (!pszHandlerRC)
        pszHandlerRC = "pgmVirtHandlerRedirectToHC";
    AssertPtrReturn(pszHandlerRC, VERR_INVALID_POINTER);

    /*
     * Resolve the GC handler.
     */
    RTRCPTR pfnHandlerRC = NIL_RTRCPTR;
    int rc = PDMR3LdrGetSymbolRCLazy(pVM, pszModRC, NULL /*pszSearchPath*/, pszHandlerRC, &pfnHandlerRC);
    if (RT_SUCCESS(rc))
        return PGMR3HandlerVirtualTypeRegisterEx(pVM, enmKind, fRelocUserRC,
                                                 pfnInvalidateR3, pfnHandlerR3,
                                                 pfnHandlerRC,
                                                 pszDesc, phType);

    AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModRC, pszHandlerRC, rc));
    return rc;
}


/**
 * Register a access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   hType           The handler type.
 * @param   GCPtr           Start address.
 * @param   GCPtrLast       Last address (inclusive).
 * @param   pvUserR3        The ring-3 context user argument.
 * @param   pvUserRC        The raw-mode context user argument.  Whether this is
 *                          automatically relocated or not depends on the type.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
VMMR3_INT_DECL(int) PGMR3HandlerVirtualRegister(PVM pVM, PVMCPU pVCpu, PGMVIRTHANDLERTYPE hType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                                void *pvUserR3, RTRCPTR pvUserRC, const char *pszDesc)
{
    AssertReturn(!HMIsEnabled(pVM), VERR_NOT_AVAILABLE); /* Not supported/relevant for VT-x and AMD-V. */
    PPGMVIRTHANDLERTYPEINT pType = PGMVIRTHANDLERTYPEINT_FROM_HANDLE(pVM, hType);
    Log(("PGMR3HandlerVirtualRegister: GCPhys=%RGp GCPhysLast=%RGp pvUserR3=%RHv pvUserGC=%RRv hType=%#x (%d, %s) pszDesc=%RHv:%s\n",
         GCPtr, GCPtrLast, pvUserR3, pvUserRC, hType, pType->enmKind, R3STRING(pType->pszDesc), pszDesc, R3STRING(pszDesc)));

    /*
     * Validate input.
     */
    AssertReturn(pType->u32Magic == PGMVIRTHANDLERTYPEINT_MAGIC, VERR_INVALID_HANDLE);
    AssertMsgReturn(GCPtr < GCPtrLast, ("GCPtr >= GCPtrLast (%RGp >= %RGp)\n", GCPtr, GCPtrLast), VERR_INVALID_PARAMETER);
    switch (pType->enmKind)
    {
        case PGMVIRTHANDLERKIND_ALL:
            AssertReleaseMsgReturn(   (GCPtr     & PAGE_OFFSET_MASK) == 0
                                   && (GCPtrLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK,
                                   ("PGMVIRTHANDLERKIND_ALL: GCPtr=%RGv GCPtrLast=%RGv\n", GCPtr, GCPtrLast),
                                   VERR_NOT_IMPLEMENTED);
            break;
        case PGMVIRTHANDLERKIND_WRITE:
        case PGMVIRTHANDLERKIND_HYPERVISOR:
            break;
        default:
            AssertMsgFailedReturn(("Invalid enmKind=%d!\n", pType->enmKind), VERR_INVALID_PARAMETER);
    }
    AssertMsgReturn(   (RTRCUINTPTR)pvUserRC < 0x10000
                    || MMHyperR3ToRC(pVM, MMHyperRCToR3(pVM, pvUserRC)) == pvUserRC,
                    ("Not RC pointer! pvUserRC=%RRv\n", pvUserRC),
                    VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize a new entry.
     */
    unsigned cPages = (RT_ALIGN(GCPtrLast + 1, PAGE_SIZE) - (GCPtr & PAGE_BASE_GC_MASK)) >> PAGE_SHIFT;
    PPGMVIRTHANDLER pNew;
    int rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMVIRTHANDLER, aPhysToVirt[cPages]), 0, MM_TAG_PGM_HANDLERS, (void **)&pNew); /** @todo r=bird: incorrect member name PhysToVirt? */
    if (RT_FAILURE(rc))
        return rc;

    pNew->Core.Key      = GCPtr;
    pNew->Core.KeyLast  = GCPtrLast;

    pNew->hType         = hType;
    pNew->pvUserRC      = pvUserRC;
    pNew->pvUserR3      = pvUserR3;
    pNew->pszDesc       = pszDesc ? pszDesc : pType->pszDesc;
    pNew->cb            = GCPtrLast - GCPtr + 1;
    pNew->cPages        = cPages;
    /* Will be synced at next guest execution attempt. */
    while (cPages-- > 0)
    {
        pNew->aPhysToVirt[cPages].Core.Key = NIL_RTGCPHYS;
        pNew->aPhysToVirt[cPages].Core.KeyLast = NIL_RTGCPHYS;
        pNew->aPhysToVirt[cPages].offVirtHandler = -RT_OFFSETOF(PGMVIRTHANDLER, aPhysToVirt[cPages]);
        pNew->aPhysToVirt[cPages].offNextAlias = 0;
    }

    /*
     * Try to insert it into the tree.
     *
     * The current implementation doesn't allow multiple handlers for
     * the same range this makes everything much simpler and faster.
     */
    AVLROGCPTRTREE *pRoot = pType->enmKind != PGMVIRTHANDLERKIND_HYPERVISOR
                          ? &pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers
                          : &pVM->pgm.s.CTX_SUFF(pTrees)->HyperVirtHandlers;
    pgmLock(pVM);
    if (*pRoot != 0)
    {
        PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGetBestFit(pRoot, pNew->Core.Key, true);
        if (    !pCur
            ||  GCPtr     > pCur->Core.KeyLast
            ||  GCPtrLast < pCur->Core.Key)
            pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGetBestFit(pRoot, pNew->Core.Key, false);
        if (    pCur
            &&  GCPtr     <= pCur->Core.KeyLast
            &&  GCPtrLast >= pCur->Core.Key)
        {
            /*
             * The LDT sometimes conflicts with the IDT and LDT ranges while being
             * updated on linux. So, we don't assert simply log it.
             */
            Log(("PGMR3HandlerVirtualRegister: Conflict with existing range %RGv-%RGv (%s), req. %RGv-%RGv (%s)\n",
                 pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc, GCPtr, GCPtrLast, pszDesc));
            MMHyperFree(pVM, pNew);
            pgmUnlock(pVM);
            return VERR_PGM_HANDLER_VIRTUAL_CONFLICT;
        }
    }
    if (RTAvlroGCPtrInsert(pRoot, &pNew->Core))
    {
        if (pType->enmKind != PGMVIRTHANDLERKIND_HYPERVISOR)
        {
            PVMCPU pVCpu = VMMGetCpu(pVM);

            pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
        }
        PGMHandlerVirtualTypeRetain(pVM, hType);
        pgmUnlock(pVM);

#ifdef VBOX_WITH_STATISTICS
        rc = STAMR3RegisterF(pVM, &pNew->Stat, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pNew->pszDesc,
                             "/PGM/VirtHandler/Calls/%RGv-%RGv", pNew->Core.Key, pNew->Core.KeyLast);
        AssertRC(rc);
#endif
        return VINF_SUCCESS;
    }

    pgmUnlock(pVM);
    AssertFailed();
    MMHyperFree(pVM, pNew);
    return VERR_PGM_HANDLER_VIRTUAL_CONFLICT;

}


/**
 * Changes the type of a virtual handler.
 *
 * The new and old type must have the same access kind.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   GCPtr           Start address of the virtual handler.
 * @param   hNewType        The new handler type.
 */
VMMR3_INT_DECL(int) PGMHandlerVirtualChangeType(PVM pVM, RTGCPTR GCPtr, PGMVIRTHANDLERTYPE hNewType)
{
    PPGMVIRTHANDLERTYPEINT pNewType = PGMVIRTHANDLERTYPEINT_FROM_HANDLE(pVM, hNewType);
    AssertReturn(pNewType->u32Magic == PGMVIRTHANDLERTYPEINT_MAGIC, VERR_INVALID_HANDLE);

    pgmLock(pVM);
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGet(&pVM->pgm.s.pTreesR3->VirtHandlers, GCPtr);
    if (pCur)
    {
        PGMVIRTHANDLERTYPE     hOldType = pCur->hType;
        PPGMVIRTHANDLERTYPEINT pOldType = PGMVIRTHANDLERTYPEINT_FROM_HANDLE(pVM, hOldType);
        if (pOldType != pNewType)
        {
            AssertReturnStmt(pNewType->enmKind == pOldType->enmKind, pgmUnlock(pVM), VERR_ACCESS_DENIED);
            PGMHandlerVirtualTypeRetain(pVM, hNewType);
            pCur->hType = hNewType;
            PGMHandlerVirtualTypeRelease(pVM, hOldType);
        }
        pgmUnlock(pVM);
        return VINF_SUCCESS;
    }
    pgmUnlock(pVM);
    AssertMsgFailed(("Range %#x not found!\n", GCPtr));
    return VERR_INVALID_PARAMETER;
}


/**
 * Deregister an access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the cross context CPU structure for the
 *                      calling EMT.
 * @param   GCPtr       Start address.
 * @param   fHypervisor Set if PGMVIRTHANDLERKIND_HYPERVISOR, false if not.
 * @thread  EMT(pVCpu)
 */
VMM_INT_DECL(int) PGMHandlerVirtualDeregister(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, bool fHypervisor)
{
    pgmLock(pVM);

    PPGMVIRTHANDLER pCur;
    if (!fHypervisor)
    {
        /*
         * Normal guest handler.
         */
        pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, GCPtr);
        AssertMsgReturnStmt(pCur, ("GCPtr=%RGv\n", GCPtr), pgmUnlock(pVM), VERR_INVALID_PARAMETER);
        Assert(PGMVIRTANDLER_GET_TYPE(pVM, pCur)->enmKind != PGMVIRTHANDLERKIND_HYPERVISOR);

        Log(("PGMHandlerVirtualDeregister: Removing Virtual (%d) Range %RGv-%RGv %s\n",
             PGMVIRTANDLER_GET_TYPE(pVM, pCur)->enmKind, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));

        /* Reset the flags and remove phys2virt nodes. */
        for (uint32_t iPage = 0; iPage < pCur->cPages; iPage++)
            if (pCur->aPhysToVirt[iPage].offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE)
                pgmHandlerVirtualClearPage(pVM, pCur, iPage);

        /* Schedule CR3 sync. */
        pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
    }
    else
    {
        /*
         * Hypervisor one (hypervisor relocation or termination only).
         */
        pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->HyperVirtHandlers, GCPtr);
        AssertMsgReturnStmt(pCur, ("GCPtr=%RGv\n", GCPtr), pgmUnlock(pVM), VERR_INVALID_PARAMETER);
        Assert(PGMVIRTANDLER_GET_TYPE(pVM, pCur)->enmKind == PGMVIRTHANDLERKIND_HYPERVISOR);

        Log(("PGMHandlerVirtualDeregister: Removing Hyper Virtual Range %RGv-%RGv %s\n",
             pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    }

    pgmUnlock(pVM);

    /*
     * Free it.
     */
#ifdef VBOX_WITH_STATISTICS
    STAMR3DeregisterF(pVM->pUVM, "/PGM/VirtHandler/Calls/%RGv-%RGv", pCur->Core.Key, pCur->Core.KeyLast);
#endif
    PGMHandlerVirtualTypeRelease(pVM, pCur->hType);
    MMHyperFree(pVM, pCur);

    return VINF_SUCCESS;
}


/**
 * Arguments for pgmR3InfoHandlersPhysicalOne and pgmR3InfoHandlersVirtualOne.
 */
typedef struct PGMHANDLERINFOARG
{
    /** The output helpers.*/
    PCDBGFINFOHLP   pHlp;
    /** Pointer to the cross context VM handle. */
    PVM             pVM;
    /** Set if statistics should be dumped. */
    bool            fStats;
} PGMHANDLERINFOARG, *PPGMHANDLERINFOARG;


/**
 * Info callback for 'pgmhandlers'.
 *
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments. phys or virt.
 */
DECLCALLBACK(void) pgmR3InfoHandlers(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /*
     * Test input.
     */
    PGMHANDLERINFOARG Args = { pHlp, pVM, /* .fStats = */ true };
    bool fPhysical = !pszArgs  || !*pszArgs;
    bool fVirtual = fPhysical;
    bool fHyper = fPhysical;
    if (!fPhysical)
    {
        bool fAll = strstr(pszArgs, "all") != NULL;
        fPhysical   = fAll || strstr(pszArgs, "phys") != NULL;
        fVirtual    = fAll || strstr(pszArgs, "virt") != NULL;
        fHyper      = fAll || strstr(pszArgs, "hyper")!= NULL;
        Args.fStats = strstr(pszArgs, "nost") == NULL;
    }

    /*
     * Dump the handlers.
     */
    if (fPhysical)
    {
        pHlp->pfnPrintf(pHlp,
            "Physical handlers: (PhysHandlers=%d (%#x))\n"
            "%*s %*s %*s %*s HandlerGC UserGC    Type     Description\n",
            pVM->pgm.s.pTreesR3->PhysHandlers, pVM->pgm.s.pTreesR3->PhysHandlers,
            - (int)sizeof(RTGCPHYS) * 2,     "From",
            - (int)sizeof(RTGCPHYS) * 2 - 3, "- To (incl)",
            - (int)sizeof(RTHCPTR)  * 2 - 1, "HandlerHC",
            - (int)sizeof(RTHCPTR)  * 2 - 1, "UserHC");
        RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysHandlers, true, pgmR3InfoHandlersPhysicalOne, &Args);
    }

    if (fVirtual)
    {
        pHlp->pfnPrintf(pHlp,
            "Virtual handlers:\n"
            "%*s %*s %*s %*s Type       Description\n",
            - (int)sizeof(RTGCPTR) * 2,     "From",
            - (int)sizeof(RTGCPTR) * 2 - 3, "- To (excl)",
            - (int)sizeof(RTHCPTR) * 2 - 1, "HandlerHC",
            - (int)sizeof(RTRCPTR) * 2 - 1, "HandlerGC");
        RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesR3->VirtHandlers, true, pgmR3InfoHandlersVirtualOne, &Args);
    }

    if (fHyper)
    {
        pHlp->pfnPrintf(pHlp,
            "Hypervisor Virtual handlers:\n"
            "%*s %*s %*s %*s Type       Description\n",
            - (int)sizeof(RTGCPTR) * 2,     "From",
            - (int)sizeof(RTGCPTR) * 2 - 3, "- To (excl)",
            - (int)sizeof(RTHCPTR) * 2 - 1, "HandlerHC",
            - (int)sizeof(RTRCPTR) * 2 - 1, "HandlerGC");
        RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesR3->HyperVirtHandlers, true, pgmR3InfoHandlersVirtualOne, &Args);
    }
}


/**
 * Displays one physical handler range.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMPHYSHANDLER.
 * @param   pvUser  Pointer to command helper functions.
 */
static DECLCALLBACK(int) pgmR3InfoHandlersPhysicalOne(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMPHYSHANDLER        pCur     = (PPGMPHYSHANDLER)pNode;
    PPGMHANDLERINFOARG     pArgs    = (PPGMHANDLERINFOARG)pvUser;
    PCDBGFINFOHLP          pHlp     = pArgs->pHlp;
    PPGMPHYSHANDLERTYPEINT pCurType = PGMPHYSHANDLER_GET_TYPE(pArgs->pVM, pCur);
    const char *pszType;
    switch (pCurType->enmKind)
    {
        case PGMPHYSHANDLERKIND_MMIO:   pszType = "MMIO   "; break;
        case PGMPHYSHANDLERKIND_WRITE:  pszType = "Write  "; break;
        case PGMPHYSHANDLERKIND_ALL:    pszType = "All    "; break;
        default:                        pszType = "????"; break;
    }
    pHlp->pfnPrintf(pHlp,
        "%RGp - %RGp  %RHv  %RHv  %RRv  %RRv  %s  %s\n",
        pCur->Core.Key, pCur->Core.KeyLast, pCurType->pfnHandlerR3, pCur->pvUserR3, pCurType->pfnHandlerRC, pCur->pvUserRC,
                    pszType, pCur->pszDesc);
#ifdef VBOX_WITH_STATISTICS
    if (pArgs->fStats)
        pHlp->pfnPrintf(pHlp, "   cPeriods: %9RU64  cTicks: %11RU64  Min: %11RU64  Avg: %11RU64 Max: %11RU64\n",
                        pCur->Stat.cPeriods, pCur->Stat.cTicks, pCur->Stat.cTicksMin,
                        pCur->Stat.cPeriods ? pCur->Stat.cTicks / pCur->Stat.cPeriods : 0, pCur->Stat.cTicksMax);
#endif
    return 0;
}


/**
 * Displays one virtual handler range.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMVIRTHANDLER.
 * @param   pvUser  Pointer to command helper functions.
 */
static DECLCALLBACK(int) pgmR3InfoHandlersVirtualOne(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER         pCur     = (PPGMVIRTHANDLER)pNode;
    PPGMHANDLERINFOARG      pArgs    = (PPGMHANDLERINFOARG)pvUser;
    PCDBGFINFOHLP           pHlp     = pArgs->pHlp;
    PPGMVIRTHANDLERTYPEINT  pCurType = PGMVIRTANDLER_GET_TYPE(pArgs->pVM, pCur);
    const char *pszType;
    switch (pCurType->enmKind)
    {
        case PGMVIRTHANDLERKIND_WRITE:      pszType = "Write  "; break;
        case PGMVIRTHANDLERKIND_ALL:        pszType = "All    "; break;
        case PGMVIRTHANDLERKIND_HYPERVISOR: pszType = "WriteHyp "; break;
        default:                            pszType = "????"; break;
    }
    pHlp->pfnPrintf(pHlp, "%RGv - %RGv  %RHv  %RRv  %s  %s\n",
        pCur->Core.Key, pCur->Core.KeyLast, pCurType->pfnHandlerR3, pCurType->pfnHandlerRC, pszType, pCur->pszDesc);
#ifdef VBOX_WITH_STATISTICS
    if (pArgs->fStats)
        pHlp->pfnPrintf(pHlp, "   cPeriods: %9RU64  cTicks: %11RU64  Min: %11RU64  Avg: %11RU64 Max: %11RU64\n",
                        pCur->Stat.cPeriods, pCur->Stat.cTicks, pCur->Stat.cTicksMin,
                        pCur->Stat.cPeriods ? pCur->Stat.cTicks / pCur->Stat.cPeriods : 0, pCur->Stat.cTicksMax);
#endif
    return 0;
}

