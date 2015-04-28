/* $Id$ */
/** @file
 * PGM - Page Manager / Monitor, Access Handlers.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
VMM_INT_DECL(int) PGMR3HandlerPhysicalTypeRegisterEx(PVM pVM, PGMPHYSHANDLERKIND enmKind,
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
 * Register a access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   enmType         Handler type. Any of the PGMVIRTHANDLERTYPE_* enums.
 * @param   GCPtr           Start address.
 * @param   GCPtrLast       Last address (inclusive).
 * @param   pfnInvalidateR3 The R3 invalidate callback (can be 0)
 * @param   pfnHandlerR3    The R3 handler.
 * @param   pszHandlerRC    The RC handler symbol name.
 * @param   pszModRC        The RC handler module.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
VMMR3DECL(int) PGMR3HandlerVirtualRegister(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                           PFNPGMR3VIRTINVALIDATE pfnInvalidateR3,
                                           PFNPGMR3VIRTHANDLER pfnHandlerR3,
                                           const char *pszHandlerRC, const char *pszModRC,
                                           const char *pszDesc)
{
    LogFlow(("PGMR3HandlerVirtualRegisterEx: enmType=%d GCPtr=%RGv GCPtrLast=%RGv pszHandlerRC=%p:{%s} pszModRC=%p:{%s} pszDesc=%s\n",
             enmType, GCPtr, GCPtrLast, pszHandlerRC, pszHandlerRC, pszModRC, pszModRC, pszDesc));

    /* Not supported/relevant for VT-x and AMD-V. */
    if (HMIsEnabled(pVM))
        return VERR_NOT_IMPLEMENTED;

    /*
     * Validate input.
     */
    if (!pszModRC)
        pszModRC = VMMGC_MAIN_MODULE_NAME;
    if (!pszModRC || !*pszModRC || !pszHandlerRC || !*pszHandlerRC)
    {
        AssertMsgFailed(("pfnHandlerGC or/and pszModRC is missing\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Resolve the GC handler.
     */
    RTRCPTR pfnHandlerRC;
    int rc = PDMR3LdrGetSymbolRCLazy(pVM, pszModRC, NULL /*pszSearchPath*/, pszHandlerRC, &pfnHandlerRC);
    if (RT_SUCCESS(rc))
        return PGMR3HandlerVirtualRegisterEx(pVM, enmType, GCPtr, GCPtrLast, pfnInvalidateR3,
                                             pfnHandlerR3, pfnHandlerRC, pszDesc);

    AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModRC, pszHandlerRC, rc));
    return rc;
}


/**
 * Register an access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   enmType         Handler type. Any of the PGMVIRTHANDLERTYPE_* enums.
 * @param   GCPtr           Start address.
 * @param   GCPtrLast       Last address (inclusive).
 * @param   pfnInvalidateR3 The R3 invalidate callback (can be 0)
 * @param   pfnHandlerR3    The R3 handler.
 * @param   pfnHandlerRC    The RC handler.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 * @thread  EMT
 */
/** @todo create a template for virtual handlers (see async i/o), we're wasting space
 * duplicating the function pointers now. (Or we will once we add the missing callbacks.) */
VMMDECL(int) PGMR3HandlerVirtualRegisterEx(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                           R3PTRTYPE(PFNPGMR3VIRTINVALIDATE) pfnInvalidateR3,
                                           R3PTRTYPE(PFNPGMR3VIRTHANDLER) pfnHandlerR3,
                                           RCPTRTYPE(PFNPGMRCVIRTHANDLER) pfnHandlerRC,
                                           R3PTRTYPE(const char *) pszDesc)
{
    Log(("PGMR3HandlerVirtualRegister: enmType=%d GCPtr=%RGv GCPtrLast=%RGv pfnInvalidateR3=%RHv pfnHandlerR3=%RHv pfnHandlerRC=%RRv pszDesc=%s\n",
         enmType, GCPtr, GCPtrLast, pfnInvalidateR3, pfnHandlerR3, pfnHandlerRC, pszDesc));

    /* Not supported/relevant for VT-x and AMD-V. */
    if (HMIsEnabled(pVM))
        return VERR_NOT_IMPLEMENTED;

    /*
     * Validate input.
     */
    switch (enmType)
    {
        case PGMVIRTHANDLERTYPE_ALL:
            AssertReleaseMsgReturn(   (GCPtr     & PAGE_OFFSET_MASK) == 0
                                   && (GCPtrLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK,
                                   ("PGMVIRTHANDLERTYPE_ALL: GCPtr=%RGv GCPtrLast=%RGv\n", GCPtr, GCPtrLast),
                                   VERR_NOT_IMPLEMENTED);
            break;
        case PGMVIRTHANDLERTYPE_WRITE:
            if (!pfnHandlerR3)
            {
                AssertMsgFailed(("No HC handler specified!!\n"));
                return VERR_INVALID_PARAMETER;
            }
            break;

        case PGMVIRTHANDLERTYPE_HYPERVISOR:
            if (pfnHandlerR3)
            {
                AssertMsgFailed(("R3 handler specified for hypervisor range!?!\n"));
                return VERR_INVALID_PARAMETER;
            }
            break;
        default:
            AssertMsgFailed(("Invalid enmType! enmType=%d\n", enmType));
            return VERR_INVALID_PARAMETER;
    }
    if (GCPtrLast < GCPtr)
    {
        AssertMsgFailed(("GCPtrLast < GCPtr (%#x < %#x)\n", GCPtrLast, GCPtr));
        return VERR_INVALID_PARAMETER;
    }
    if (!pfnHandlerRC)
    {
        AssertMsgFailed(("pfnHandlerRC is missing\n"));
        return VERR_INVALID_PARAMETER;
    }

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

    pNew->enmType       = enmType;
    pNew->pfnInvalidateR3 = pfnInvalidateR3;
    pNew->pfnHandlerRC  = pfnHandlerRC;
    pNew->pfnHandlerR3  = pfnHandlerR3;
    pNew->pszDesc       = pszDesc;
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
    AVLROGCPTRTREE *pRoot = enmType != PGMVIRTHANDLERTYPE_HYPERVISOR
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
        if (enmType != PGMVIRTHANDLERTYPE_HYPERVISOR)
        {
            PVMCPU pVCpu = VMMGetCpu(pVM);

            pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
        }
        pgmUnlock(pVM);

#ifdef VBOX_WITH_STATISTICS
        rc = STAMR3RegisterF(pVM, &pNew->Stat, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, pszDesc,
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
 * Modify the page invalidation callback handler for a registered virtual range.
 * (add more when needed)
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   GCPtr           Start address.
 * @param   pfnInvalidateR3 The R3 invalidate callback (can be 0)
 * @remarks Doesn't work with the hypervisor access handler type.
 */
VMMDECL(int) PGMHandlerVirtualChangeInvalidateCallback(PVM pVM, RTGCPTR GCPtr, PFNPGMR3VIRTINVALIDATE pfnInvalidateR3)
{
    pgmLock(pVM);
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGet(&pVM->pgm.s.pTreesR3->VirtHandlers, GCPtr);
    if (pCur)
    {
        pCur->pfnInvalidateR3 = pfnInvalidateR3;
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
 * @param   GCPtr       Start address.
 * @thread  EMT
 */
VMMDECL(int) PGMHandlerVirtualDeregister(PVM pVM, RTGCPTR GCPtr)
{
    pgmLock(pVM);

    /*
     * Find the handler.
     * We naturally assume GCPtr is a unique specification.
     */
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, GCPtr);
    if (RT_LIKELY(pCur))
    {
        Log(("PGMHandlerVirtualDeregister: Removing Virtual (%d) Range %RGv-%RGv %s\n", pCur->enmType,
             pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
        Assert(pCur->enmType != PGMVIRTHANDLERTYPE_HYPERVISOR);

        /*
         * Reset the flags and remove phys2virt nodes.
         */
        for (uint32_t iPage = 0; iPage < pCur->cPages; iPage++)
            if (pCur->aPhysToVirt[iPage].offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE)
                pgmHandlerVirtualClearPage(pVM, pCur, iPage);

        /*
         * Schedule CR3 sync.
         */
        PVMCPU pVCpu = VMMGetCpu(pVM);

        pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
    }
    else
    {
        /* must be a hypervisor one then. */
        pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->HyperVirtHandlers, GCPtr);
        if (RT_UNLIKELY(!pCur))
        {
            pgmUnlock(pVM);
#ifndef DEBUG_sander
            AssertMsgFailed(("Range %#x not found!\n", GCPtr));
#endif
            return VERR_INVALID_PARAMETER;
        }

        Log(("PGMHandlerVirtualDeregister: Removing Hyper Virtual (%d) Range %RGv-%RGv %s\n", pCur->enmType,
             pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
        Assert(pCur->enmType == PGMVIRTHANDLERTYPE_HYPERVISOR);
    }

    pgmUnlock(pVM);

#ifdef VBOX_WITH_STATISTICS
    STAMR3DeregisterF(pVM->pUVM, "/PGM/VirtHandler/Calls/%RGv-%RGv", pCur->Core.Key, pCur->Core.KeyLast);
#endif
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
    PPGMVIRTHANDLER     pCur = (PPGMVIRTHANDLER)pNode;
    PPGMHANDLERINFOARG  pArgs= (PPGMHANDLERINFOARG)pvUser;
    PCDBGFINFOHLP       pHlp = pArgs->pHlp;
    const char *pszType;
    switch (pCur->enmType)
    {
        case PGMVIRTHANDLERTYPE_WRITE:      pszType = "Write  "; break;
        case PGMVIRTHANDLERTYPE_ALL:        pszType = "All    "; break;
        case PGMVIRTHANDLERTYPE_HYPERVISOR: pszType = "WriteHyp "; break;
        default:                            pszType = "????"; break;
    }
    pHlp->pfnPrintf(pHlp, "%RGv - %RGv  %RHv  %RRv  %s  %s\n",
        pCur->Core.Key, pCur->Core.KeyLast, pCur->pfnHandlerR3, pCur->pfnHandlerRC, pszType, pCur->pszDesc);
#ifdef VBOX_WITH_STATISTICS
    if (pArgs->fStats)
        pHlp->pfnPrintf(pHlp, "   cPeriods: %9RU64  cTicks: %11RU64  Min: %11RU64  Avg: %11RU64 Max: %11RU64\n",
                        pCur->Stat.cPeriods, pCur->Stat.cTicks, pCur->Stat.cTicksMin,
                        pCur->Stat.cPeriods ? pCur->Stat.cTicks / pCur->Stat.cPeriods : 0, pCur->Stat.cTicksMax);
#endif
    return 0;
}

