/* $Id$ */
/** @file
 * PGM - Page Manager / Monitor, Access Handlers.
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
                                            const char *pszModGC, const char *pszHandlerGC, RTGCPTR pvUserGC, const char *pszDesc)
{
    LogFlow(("PGMR3HandlerPhysicalRegister: enmType=%d GCPhys=%VGv GCPhysLast=%VGv pfnHandlerR3=%VHv pvUserHC=%VHv pszModGC=%p:{%s} pszHandlerGC=%p:{%s} pvUser=%VGv pszDesc=%s\n",
             enmType, GCPhys, GCPhysLast, pfnHandlerR3, pvUserR3, pszModGC, pszModGC, pszHandlerGC, pszHandlerGC, pvUserGC, pszDesc));

    /*
     * Validate input.
     */
    if (!pszModGC)
        pszModGC = VMMGC_MAIN_MODULE_NAME;

    if (!pszModR0)
        pszModR0 = VMMR0_MAIN_MODULE_NAME;

    /*
     * Resolve the R0 handler.
     */
    R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0 = NIL_RTR0PTR;
    int rc = VINF_SUCCESS;
    if (pszHandlerR0 && HWACCMR3IsAllowed(pVM))
        rc = PDMR3GetSymbolR0Lazy(pVM, pszModR0, pszHandlerR0, &pfnHandlerR0);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Resolve the GC handler.
         */
        RTGCPTR pfnHandlerGC = NIL_RTGCPTR;
        if (pszHandlerGC)
            rc = PDMR3GetSymbolGCLazy(pVM, pszModGC, pszHandlerGC, &pfnHandlerGC);

        if (VBOX_SUCCESS(rc))
            return PGMHandlerPhysicalRegisterEx(pVM, enmType, GCPhys, GCPhysLast, pfnHandlerR3, pvUserR3,
                                                pfnHandlerR0, pvUserR0, pfnHandlerGC, pvUserGC, pszDesc);

        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Vrc.\n", pszModGC, pszHandlerGC, rc));
    }
    else
        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Vrc.\n", pszModR0, pszHandlerR0, rc));

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
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers,  true, pgmR3HandlerPhysicalOneClear, pVM);
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, false, pgmR3HandlerPhysicalOneSet, pVM);
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
        pgmRamFlagsClearByGCPhysWithHint(pPGM, GCPhys, MM_RAM_FLAGS_PHYSICAL_HANDLER | MM_RAM_FLAGS_PHYSICAL_WRITE | MM_RAM_FLAGS_PHYSICAL_ALL, &pRamHint);
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
    unsigned        fFlags = pgmHandlerPhysicalCalcFlags(pCur);
    PPGMRAMRANGE    pRamHint = NULL;
    RTGCPHYS        GCPhys = pCur->Core.Key;
    RTUINT          cPages = pCur->cPages;
    PPGM            pPGM = &((PVM)pvUser)->pgm.s;
    for (;;)
    {
        pgmRamFlagsSetByGCPhysWithHint(pPGM, GCPhys, fFlags, &pRamHint);
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
 * @param   pfnInvalidateHC The HC invalidate callback (can be 0)
 * @param   pfnHandlerHC    The HC handler.
 * @param   pszHandlerGC    The GC handler symbol name.
 * @param   pszModGC        The GC handler module.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
/** @todo rename this function to PGMR3HandlerVirtualRegister */
PGMR3DECL(int) PGMR3HandlerVirtualRegister(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                           PFNPGMHCVIRTINVALIDATE pfnInvalidateHC,
                                           PFNPGMHCVIRTHANDLER pfnHandlerHC,
                                           const char *pszHandlerGC, const char *pszModGC,
                                           const char *pszDesc)
{
    LogFlow(("PGMR3HandlerVirtualRegisterEx: enmType=%d GCPtr=%VGv GCPtrLast=%VGv pszHandlerGC=%p:{%s} pszModGC=%p:{%s} pszDesc=%s\n",
             enmType, GCPtr, GCPtrLast, pszHandlerGC, pszHandlerGC, pszModGC, pszModGC, pszDesc));

    /*
     * Validate input.
     */
    if (!pszModGC)
        pszModGC = VMMGC_MAIN_MODULE_NAME;
    if (!pszModGC || !*pszModGC || !pszHandlerGC || !*pszHandlerGC)
    {
        AssertMsgFailed(("pfnHandlerGC or/and pszModGC is missing\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Resolve the GC handler.
     */
    RTGCPTR pfnHandlerGC;
    int rc = PDMR3GetSymbolGCLazy(pVM, pszModGC, pszHandlerGC, &pfnHandlerGC);
    if (VBOX_SUCCESS(rc))
        return PGMHandlerVirtualRegisterEx(pVM, enmType, GCPtr, GCPtrLast, pfnInvalidateHC, pfnHandlerHC, pfnHandlerGC, pszDesc);

    AssertMsgFailed(("Failed to resolve %s.%s, rc=%Vrc.\n", pszModGC, pszHandlerGC, rc));
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
 * @param   pfnInvalidateHC The HC invalidate callback (can be 0)
 * @param   pfnHandlerHC    The HC handler.
 * @param   pfnHandlerGC    The GC handler.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
/** @todo rename this to PGMR3HandlerVirtualRegisterEx. */
PGMDECL(int) PGMHandlerVirtualRegisterEx(PVM pVM, PGMVIRTHANDLERTYPE enmType, RTGCPTR GCPtr, RTGCPTR GCPtrLast,
                                         PFNPGMHCVIRTINVALIDATE pfnInvalidateHC,
                                         PFNPGMHCVIRTHANDLER pfnHandlerHC, RTGCPTR pfnHandlerGC,
                                         R3PTRTYPE(const char *) pszDesc)
{
    Log(("PGMR3HandlerVirtualRegister: enmType=%d GCPtr=%RGv GCPtrLast=%RGv pfnHandlerGC=%RGv pszDesc=%s\n", enmType, GCPtr, GCPtrLast, pfnHandlerGC, pszDesc));

    /*
     * Validate input.
     */
    switch (enmType)
    {
        case PGMVIRTHANDLERTYPE_NORMAL:
        case PGMVIRTHANDLERTYPE_ALL:
        case PGMVIRTHANDLERTYPE_WRITE:
        case PGMVIRTHANDLERTYPE_EIP:
            if (!pfnHandlerHC)
            {
                AssertMsgFailed(("No HC handler specified!!\n"));
                return VERR_INVALID_PARAMETER;
            }
            break;

        case PGMVIRTHANDLERTYPE_HYPERVISOR:
            if (pfnHandlerHC)
            {
                AssertMsgFailed(("HC handler specified for hypervisor range!?!\n"));
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
    if (!pfnHandlerGC)
    {
        AssertMsgFailed(("pfnHandlerGC is missing\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate and initialize a new entry.
     */
    unsigned cPages = (RT_ALIGN((RTGCUINTPTR)GCPtrLast + 1, PAGE_SIZE) - ((RTGCUINTPTR)GCPtr & PAGE_BASE_GC_MASK)) >> PAGE_SHIFT;
    PPGMVIRTHANDLER pNew;
    int rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMVIRTHANDLER, aPhysToVirt[cPages]), 0, MM_TAG_PGM_HANDLERS, (void **)&pNew); /** @todo r=bird: incorrect member name PhysToVirt? */
    if (VBOX_FAILURE(rc))
        return rc;

    pNew->Core.Key      = GCPtr;
    pNew->Core.KeyLast  = GCPtrLast;

    pNew->enmType       = enmType;
    pNew->pfnInvalidateHC = pfnInvalidateHC;
    pNew->pfnHandlerGC  = pfnHandlerGC;
    pNew->pfnHandlerHC  = pfnHandlerHC;
    pNew->pszDesc       = pszDesc;
    pNew->GCPtr         = GCPtr;
    pNew->GCPtrLast     = GCPtrLast;
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
    pgmLock(pVM);
    if (pVM->pgm.s.pTreesHC->VirtHandlers != 0)
    {
        PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGetBestFit(&pVM->pgm.s.CTXSUFF(pTrees)->VirtHandlers, pNew->Core.Key, true);
        if (!pCur || GCPtr > pCur->GCPtrLast || GCPtrLast < pCur->GCPtr)
            pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGetBestFit(&pVM->pgm.s.CTXSUFF(pTrees)->VirtHandlers, pNew->Core.Key, false);
        if (pCur && GCPtr <= pCur->GCPtrLast && GCPtrLast >= pCur->GCPtr)
        {
            /*
             * The LDT sometimes conflicts with the IDT and LDT ranges while being
             * updated on linux. So, we don't assert simply log it.
             */
            Log(("PGMR3HandlerVirtualRegister: Conflict with existing range %RGv-%RGv (%s), req. %RGv-%RGv (%s)\n",
                 pCur->GCPtr, pCur->GCPtrLast, pCur->pszDesc, GCPtr, GCPtrLast, pszDesc));
            MMHyperFree(pVM, pNew);
            pgmUnlock(pVM);
            return VERR_PGM_HANDLER_VIRTUAL_CONFLICT;
        }
    }
    if (RTAvlroGCPtrInsert(&pVM->pgm.s.CTXSUFF(pTrees)->VirtHandlers, &pNew->Core))
    {
        if (enmType != PGMVIRTHANDLERTYPE_HYPERVISOR)
        {
            pVM->pgm.s.fPhysCacheFlushPending = true;
            pVM->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        }
        pgmUnlock(pVM);

#ifdef VBOX_WITH_STATISTICS
        char szPath[256];
        RTStrPrintf(szPath, sizeof(szPath), "/PGM/VirtHandler/Calls/%VGv-%VGv", pNew->GCPtr, pNew->GCPtrLast);
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
 * Modify the page invalidation callback handler for a registered virtual range
 * (add more when needed)
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   GCPtr           Start address.
 * @param   pfnInvalidateHC The HC invalidate callback (can be 0)
 */
PGMDECL(int) PGMHandlerVirtualChangeInvalidateCallback(PVM pVM, RTGCPTR GCPtr, PFNPGMHCVIRTINVALIDATE pfnInvalidateHC)
{
    pgmLock(pVM);
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGet(&pVM->pgm.s.pTreesHC->VirtHandlers, GCPtr);
    if (pCur)
    {
        pCur->pfnInvalidateHC = pfnInvalidateHC;
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
 */
PGMDECL(int) PGMHandlerVirtualDeregister(PVM pVM, RTGCPTR GCPtr)
{
    /*
     * Find the handler.
     * We naturally assume GCPtr is a unique specification.
     */
    pgmLock(pVM);
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRemove(&pVM->pgm.s.CTXSUFF(pTrees)->VirtHandlers, GCPtr);
    if (pCur)
    {
        Log(("PGMR3HandlerVirtualDeregister: Removing Virtual (%d) Range %#x-%#x %s\n", pCur->enmType,
             pCur->GCPtr, pCur->GCPtrLast, pCur->pszDesc));

        /*
         * Reset the flags and remove phys2virt nodes.
         */
        PPGM pPGM = &pVM->pgm.s;
        for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
            if (pCur->aPhysToVirt[iPage].offNextAlias & PGMPHYS2VIRTHANDLER_IN_TREE)
                pgmHandlerVirtualClearPage(pPGM, pCur, iPage);

        /*
         * Schedule CR3 sync (if required) and the memory.
         */
        STAM_DEREG(pVM, &pCur->Stat);
        if (pCur->enmType != PGMVIRTHANDLERTYPE_HYPERVISOR)
        {
            pVM->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL | PGM_SYNC_CLEAR_PGM_POOL;
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        }
        MMHyperFree(pVM, pCur);
        pgmUnlock(pVM);
        return VINF_SUCCESS;
    }
    pgmUnlock(pVM);
    AssertMsgFailed(("Range %#x not found!\n", GCPtr));
    return VERR_INVALID_PARAMETER;
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
    PGMHANDLERINFOARG Args = { pHlp, true };
    bool fPhysical = !pszArgs  || !*pszArgs;
    bool fVirtual = fPhysical;
    if (!fPhysical)
    {
        fPhysical = strstr(pszArgs, "phys") != NULL;
        fVirtual = strstr(pszArgs, "virt") != NULL;
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
            pVM->pgm.s.pTreesHC->PhysHandlers, pVM->pgm.s.pTreesHC->PhysHandlers);
        RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesHC->PhysHandlers, true, pgmR3InfoHandlersPhysicalOne, &Args);
    }

    if (fVirtual)
    {
        pHlp->pfnPrintf(pHlp,
            "Virtual handlers:\n"
            "From     - To (excl) HandlerHC HandlerGC Type     Description\n");
        RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesHC->VirtHandlers, true, pgmR3InfoHandlersVirtualOne, &Args);
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
        case PGMPHYSHANDLERTYPE_PHYSICAL:       pszType = "Natural"; break;
        case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE: pszType = "Write  "; break;
        case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:   pszType = "All    "; break;
        default:                                pszType = "????"; break;
    }
    pHlp->pfnPrintf(pHlp,
        "%VGp - %VGp  %VHv  %VHv  %VGv  %VGv  %s  %s\n",
        pCur->Core.Key, pCur->Core.KeyLast, pCur->pfnHandlerR3, pCur->pvUserR3, pCur->pfnHandlerGC, pCur->pvUserGC, pszType, pCur->pszDesc);
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
        case PGMVIRTHANDLERTYPE_NORMAL: pszType = "Natural"; break;
        case PGMVIRTHANDLERTYPE_WRITE:  pszType = "Write  "; break;
        case PGMVIRTHANDLERTYPE_ALL:    pszType = "All    "; break;
        case PGMVIRTHANDLERTYPE_EIP:    pszType = "EIP    "; break;
        case PGMVIRTHANDLERTYPE_HYPERVISOR: pszType = "WriteHyp "; break;
        default:                        pszType = "????"; break;
    }
    pHlp->pfnPrintf(pHlp, "%08x - %08x  %08x  %08x  %s  %s\n",
        pCur->GCPtr, pCur->GCPtrLast, pCur->pfnHandlerHC, pCur->pfnHandlerGC, pszType, pCur->pszDesc);
#ifdef VBOX_WITH_STATISTICS
    if (pArgs->fStats)
        pHlp->pfnPrintf(pHlp, "   cPeriods: %9RU64  cTicks: %11RU64  Min: %11RU64  Avg: %11RU64 Max: %11RU64\n",
                        pCur->Stat.cPeriods, pCur->Stat.cTicks, pCur->Stat.cTicksMin,
                        pCur->Stat.cPeriods ? pCur->Stat.cTicks / pCur->Stat.cPeriods : 0, pCur->Stat.cTicksMax);
#endif
    return 0;
}

