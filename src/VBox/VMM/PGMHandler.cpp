/* $Id$ */
/** @file
 * PGM - Page Manager / Monitor, Access Handlers.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/csam.h>
#include <VBox/rem.h>
#include <VBox/dbgf.h>
#include <VBox/rem.h>
#include <VBox/selm.h>
#include <VBox/ssm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/hwaccm.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneClear(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneSet(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3InfoHandlersPhysicalOne(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3InfoHandlersVirtualOne(PAVLROGCPTRNODECORE pNode, void *pvUser);



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
 * @param   pszModR0        The R0 handler module. NULL means the default R0 module.
 * @param   pszHandlerR0    The R0 handler symbol name.
 * @param   pvUserR0        User argument to the R0 handler.
 * @param   pszModRC        The RC handler module. NULL means the default RC
 *                          module.
 * @param   pszHandlerRC    The RC handler symbol name.
 * @param   pvUserRC        User argument to the RC handler. Values less than
 *                          0x10000 will not be relocated.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
VMMR3DECL(int) PGMR3HandlerPhysicalRegister(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                            PFNPGMR3PHYSHANDLER pfnHandlerR3, void *pvUserR3,
                                            const char *pszModR0, const char *pszHandlerR0, RTR0PTR pvUserR0,
                                            const char *pszModRC, const char *pszHandlerRC, RTRCPTR pvUserRC, const char *pszDesc)
{
    LogFlow(("PGMR3HandlerPhysicalRegister: enmType=%d GCPhys=%RGp GCPhysLast=%RGp pfnHandlerR3=%RHv pvUserHC=%RHv pszModR0=%s pszHandlerR0=%s pvUserR0=%RHv pszModRC=%s pszHandlerRC=%s pvUser=%RRv pszDesc=%s\n",
             enmType, GCPhys, GCPhysLast, pfnHandlerR3, pvUserR3, pszModR0, pszHandlerR0, pvUserR0, pszHandlerRC, pszModRC, pvUserRC, pszDesc));

    /*
     * Validate input.
     */
    if (!pszModRC)
        pszModRC = VMMGC_MAIN_MODULE_NAME;
    if (!pszModR0)
        pszModR0 = VMMR0_MAIN_MODULE_NAME;
    AssertPtrReturn(pfnHandlerR3, VERR_INVALID_POINTER);
    AssertPtrReturn(pszHandlerR0, VERR_INVALID_POINTER);
    AssertPtrReturn(pszHandlerRC, VERR_INVALID_POINTER);

    /*
     * Resolve the R0 handler.
     */
    R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0 = NIL_RTR0PTR;
    int rc = VINF_SUCCESS;
    rc = PDMR3LdrGetSymbolR0Lazy(pVM, pszModR0, pszHandlerR0, &pfnHandlerR0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Resolve the GC handler.
         */
        RTRCPTR pfnHandlerRC = NIL_RTRCPTR;
        rc = PDMR3LdrGetSymbolRCLazy(pVM, pszModRC, pszHandlerRC, &pfnHandlerRC);
        if (RT_SUCCESS(rc))
            return PGMHandlerPhysicalRegisterEx(pVM, enmType, GCPhys, GCPhysLast, pfnHandlerR3, pvUserR3,
                                                pfnHandlerR0, pvUserR0, pfnHandlerRC, pvUserRC, pszDesc);

        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModRC, pszHandlerRC, rc));
    }
    else
        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModR0, pszHandlerR0, rc));

    return rc;
}


/**
 * Updates the physical page access handlers.
 *
 * @param   pVM     VM handle.
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
 * @param   pvUser  VM handle.
 */
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneClear(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)pNode;
    PPGMRAMRANGE    pRamHint = NULL;
    RTGCPHYS        GCPhys = pCur->Core.Key;
    RTUINT          cPages = pCur->cPages;
    PPGM            pPGM = &((PVM)pvUser)->pgm.s;
    for (;;)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageWithHintEx(pPGM, GCPhys, &pPage, &pRamHint);
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
 * @param   pvUser  VM handle.
 */
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneSet(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)pNode;
    unsigned        uState = pgmHandlerPhysicalCalcState(pCur);
    PPGMRAMRANGE    pRamHint = NULL;
    RTGCPHYS        GCPhys = pCur->Core.Key;
    RTUINT          cPages = pCur->cPages;
    PPGM            pPGM = &((PVM)pvUser)->pgm.s;
    for (;;)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageWithHintEx(pPGM, GCPhys, &pPage, &pRamHint);
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
 * @param   pVM             VM handle.
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
    int rc = PDMR3LdrGetSymbolRCLazy(pVM, pszModRC, pszHandlerRC, &pfnHandlerRC);
    if (RT_SUCCESS(rc))
        return PGMR3HandlerVirtualRegisterEx(pVM, enmType, GCPtr, GCPtrLast, pfnInvalidateR3, pfnHandlerR3, pfnHandlerRC, pszDesc);

    AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszModRC, pszHandlerRC, rc));
    return rc;
}


/**
 * Register an access handler for a virtual range.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
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
            pVM->pgm.s.fPhysCacheFlushPending = true;
            for (unsigned i=0;i<pVM->cCPUs;i++)
            {
                PVMCPU pVCpu = &pVM->aCpus[i];

                pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
                VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            }
        }
        pgmUnlock(pVM);

#ifdef VBOX_WITH_STATISTICS
        char szPath[256];
        RTStrPrintf(szPath, sizeof(szPath), "/PGM/VirtHandler/Calls/%RGv-%RGv", pNew->Core.Key, pNew->Core.KeyLast);
        rc = STAMR3Register(pVM, &pNew->Stat, STAMTYPE_PROFILE, STAMVISIBILITY_USED, szPath, STAMUNIT_TICKS_PER_CALL, pszDesc);
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
 * @param   pVM             VM handle.
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
 * @param   pVM         VM handle.
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
        PPGM pPGM = &pVM->pgm.s;
        for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
            if (pCur->aPhysToVirt[iPage].offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE)
                pgmHandlerVirtualClearPage(pPGM, pCur, iPage);

        /*
         * Schedule CR3 sync.
         */
        for (unsigned i=0;i<pVM->cCPUs;i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        }
    }
    else
    {
        /* must be a hypervisor one then. */
        pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->HyperVirtHandlers, GCPtr);
        if (RT_UNLIKELY(!pCur))
        {
            pgmUnlock(pVM);
            AssertMsgFailed(("Range %#x not found!\n", GCPtr));
            return VERR_INVALID_PARAMETER;
        }

        Log(("PGMHandlerVirtualDeregister: Removing Hyper Virtual (%d) Range %RGv-%RGv %s\n", pCur->enmType,
             pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
        Assert(pCur->enmType == PGMVIRTHANDLERTYPE_HYPERVISOR);
    }

    pgmUnlock(pVM);

    STAM_DEREG(pVM, &pCur->Stat);
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
    PGMHANDLERINFOARG Args = { pHlp, /* .fStats = */ true };
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
            "From     - To (incl) HandlerHC UserHC    HandlerGC UserGC    Type     Description\n",
            pVM->pgm.s.pTreesR3->PhysHandlers, pVM->pgm.s.pTreesR3->PhysHandlers);
        RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysHandlers, true, pgmR3InfoHandlersPhysicalOne, &Args);
    }

    if (fVirtual)
    {
        pHlp->pfnPrintf(pHlp,
            "Virtual handlers:\n"
            "From     - To (excl) HandlerHC HandlerGC Type     Description\n");
        RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesR3->VirtHandlers, true, pgmR3InfoHandlersVirtualOne, &Args);
    }

    if (fHyper)
    {
        pHlp->pfnPrintf(pHlp,
            "Hypervisor Virtual handlers:\n"
            "From     - To (excl) HandlerHC HandlerGC Type     Description\n");
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
    PPGMPHYSHANDLER     pCur = (PPGMPHYSHANDLER)pNode;
    PPGMHANDLERINFOARG  pArgs= (PPGMHANDLERINFOARG)pvUser;
    PCDBGFINFOHLP       pHlp = pArgs->pHlp;
    const char *pszType;
    switch (pCur->enmType)
    {
        case PGMPHYSHANDLERTYPE_MMIO:           pszType = "MMIO   "; break;
        case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE: pszType = "Write  "; break;
        case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:   pszType = "All    "; break;
        default:                                pszType = "????"; break;
    }
    pHlp->pfnPrintf(pHlp,
        "%RGp - %RGp  %RHv  %RHv  %RRv  %RRv  %s  %s\n",
        pCur->Core.Key, pCur->Core.KeyLast, pCur->pfnHandlerR3, pCur->pvUserR3, pCur->pfnHandlerRC, pCur->pvUserRC, pszType, pCur->pszDesc);
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

