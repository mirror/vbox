/* $Id$ */
/** @file
 * PGM - Page Manager / Monitor, Access Handlers.
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/iom.h>
#include <VBox/sup.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/dbgf.h>
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
#include <iprt/errcore.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <VBox/param.h>
#include <VBox/vmm/hm.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneClear(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3HandlerPhysicalOneSet(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3InfoHandlersPhysicalOne(PAVLROGCPHYSNODECORE pNode, void *pvUser);




/**
 * Register a physical page access handler type, extended version.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   enmKind         The kind of access handler.
 * @param   pfnHandlerR3    Pointer to the ring-3 handler callback.
 * @param   pfnHandlerR0    Pointer to the ring-0 handler callback.
 * @param   pfnPfHandlerR0  Pointer to the ring-0 \#PF handler callback.
 *                          callback.
 * @param   pszDesc         The type description.
 * @param   phType          Where to return the type handle (cross context
 *                          safe).
 */
VMMR3_INT_DECL(int) PGMR3HandlerPhysicalTypeRegisterEx(PVM pVM, PGMPHYSHANDLERKIND enmKind,
                                                       PFNPGMPHYSHANDLER pfnHandlerR3,
                                                       R0PTRTYPE(PFNPGMPHYSHANDLER) pfnHandlerR0,
                                                       R0PTRTYPE(PFNPGMRZPHYSPFHANDLER) pfnPfHandlerR0,
                                                       const char *pszDesc, PPGMPHYSHANDLERTYPE phType)
{
    AssertPtrReturn(pfnHandlerR3, VERR_INVALID_POINTER);
    AssertReturn(pfnHandlerR0   != NIL_RTR0PTR, VERR_INVALID_POINTER);
    AssertReturn(pfnPfHandlerR0 != NIL_RTR0PTR, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(   enmKind == PGMPHYSHANDLERKIND_WRITE
                 || enmKind == PGMPHYSHANDLERKIND_ALL
                 || enmKind == PGMPHYSHANDLERKIND_MMIO,
                 VERR_INVALID_PARAMETER);

    PPGMPHYSHANDLERTYPEINT pType;
    int rc = MMHyperAlloc(pVM, sizeof(*pType), 0, MM_TAG_PGM_HANDLER_TYPES, (void **)&pType);
    if (RT_SUCCESS(rc))
    {
        pType->u32Magic         = PGMPHYSHANDLERTYPEINT_MAGIC;
        pType->cRefs            = 1;
        pType->enmKind          = enmKind;
        pType->uState           = enmKind == PGMPHYSHANDLERKIND_WRITE
                                ? PGM_PAGE_HNDL_PHYS_STATE_WRITE : PGM_PAGE_HNDL_PHYS_STATE_ALL;
        pType->pfnHandlerR3     = pfnHandlerR3;
        pType->pfnHandlerR0     = pfnHandlerR0;
        pType->pfnPfHandlerR0   = pfnPfHandlerR0;
        pType->pszDesc          = pszDesc;

        pgmLock(pVM);
        RTListOff32Append(&pVM->pgm.s.CTX_SUFF(pTrees)->HeadPhysHandlerTypes, &pType->ListNode);
        pgmUnlock(pVM);

        *phType = MMHyperHeapPtrToOffset(pVM, pType);
        LogFlow(("PGMR3HandlerPhysicalTypeRegisterEx: %p/%#x: enmKind=%d pfnHandlerR3=%RHv pfnHandlerR0=%RHv pszDesc=%s\n",
                 pType, *phType, enmKind, pfnHandlerR3, pfnPfHandlerR0, pszDesc));
        return VINF_SUCCESS;
    }
    *phType = NIL_PGMPHYSHANDLERTYPE;
    return rc;
}


/**
 * Register a physical page access handler type.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   enmKind         The kind of access handler.
 * @param   pfnHandlerR3    Pointer to the ring-3 handler callback.
 * @param   pszModR0        The name of the ring-0 module, NULL is an alias for
 *                          the main ring-0 module.
 * @param   pszHandlerR0    The name of the ring-0 handler, NULL if the ring-3
 *                          handler should be called.
 * @param   pszPfHandlerR0  The name of the ring-0 \#PF handler, NULL if the
 *                          ring-3 handler should be called.
 * @param   pszModRC        The name of the raw-mode context module, NULL is an
 *                          alias for the main RC module.
 * @param   pszHandlerRC    The name of the raw-mode context handler, NULL if
 *                          the ring-3 handler should be called.
 * @param   pszPfHandlerRC  The name of the raw-mode context \#PF handler, NULL
 *                          if the ring-3 handler should be called.
 * @param   pszDesc         The type description.
 * @param   phType          Where to return the type handle (cross context
 *                          safe).
 */
VMMR3DECL(int) PGMR3HandlerPhysicalTypeRegister(PVM pVM, PGMPHYSHANDLERKIND enmKind,
                                                R3PTRTYPE(PFNPGMPHYSHANDLER) pfnHandlerR3,
                                                const char *pszModR0, const char *pszHandlerR0, const char *pszPfHandlerR0,
                                                const char *pszModRC, const char *pszHandlerRC, const char *pszPfHandlerRC,
                                                const char *pszDesc, PPGMPHYSHANDLERTYPE phType)
{
    LogFlow(("PGMR3HandlerPhysicalTypeRegister: enmKind=%d pfnHandlerR3=%RHv pszModR0=%s pszHandlerR0=%s pszPfHandlerR0=%s pszModRC=%s pszHandlerRC=%s pszPfHandlerRC=%s pszDesc=%s\n",
             enmKind, pfnHandlerR3, pszModR0, pszHandlerR0, pszPfHandlerR0, pszModRC, pszHandlerRC, pszPfHandlerRC, pszDesc));

    /*
     * Validate input.
     */
    AssertPtrReturn(pfnHandlerR3, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszModR0, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszHandlerR0, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszPfHandlerR0, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszModRC, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszHandlerRC, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszPfHandlerRC, VERR_INVALID_POINTER);

    /*
     * Resolve the R0 handlers.
     */
    R0PTRTYPE(PFNPGMPHYSHANDLER) pfnHandlerR0 = NIL_RTR0PTR;
    int rc = PDMR3LdrGetSymbolR0Lazy(pVM, pszHandlerR0 ? pszModR0 : NULL, NULL /*pszSearchPath*/,
                                     pszHandlerR0 ? pszHandlerR0 : "pgmPhysHandlerRedirectToHC", &pfnHandlerR0);
    if (RT_SUCCESS(rc))
    {
        R0PTRTYPE(PFNPGMR0PHYSPFHANDLER) pfnPfHandlerR0 = NIL_RTR0PTR;
        rc = PDMR3LdrGetSymbolR0Lazy(pVM, pszPfHandlerR0 ? pszModR0 : NULL, NULL /*pszSearchPath*/,
                                     pszPfHandlerR0 ? pszPfHandlerR0 : "pgmPhysPfHandlerRedirectToHC", &pfnPfHandlerR0);
        if (RT_SUCCESS(rc))
        {
            /*
             * Resolve the GC handler.
             */
            RTRCPTR pfnHandlerRC   = NIL_RTRCPTR;
            RTRCPTR pfnPfHandlerRC = NIL_RTRCPTR;
            if (VM_IS_RAW_MODE_ENABLED(pVM))
            {
                rc = PDMR3LdrGetSymbolRCLazy(pVM, pszHandlerRC ? pszModRC : NULL, NULL /*pszSearchPath*/,
                                             pszHandlerRC ? pszHandlerRC : "pgmPhysHandlerRedirectToHC", &pfnHandlerRC);
                if (RT_SUCCESS(rc))
                {
                    rc = PDMR3LdrGetSymbolRCLazy(pVM, pszPfHandlerRC ? pszModRC : NULL, NULL /*pszSearchPath*/,
                                                 pszPfHandlerRC ? pszPfHandlerRC : "pgmPhysPfHandlerRedirectToHC", &pfnPfHandlerRC);
                    AssertMsgRC(rc, ("Failed to resolve %s.%s, rc=%Rrc.\n", pszPfHandlerRC ? pszModRC : VMMRC_MAIN_MODULE_NAME,
                                     pszPfHandlerRC ? pszPfHandlerRC : "pgmPhysPfHandlerRedirectToHC", rc));
                }
                else
                    AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszHandlerRC ? pszModRC : VMMRC_MAIN_MODULE_NAME,
                                     pszHandlerRC ? pszHandlerRC : "pgmPhysHandlerRedirectToHC", rc));

            }
            if (RT_SUCCESS(rc))
                return PGMR3HandlerPhysicalTypeRegisterEx(pVM, enmKind, pfnHandlerR3,
                                                          pfnHandlerR0, pfnPfHandlerR0, pszDesc, phType);
        }
        else
            AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszPfHandlerR0 ? pszModR0 : VMMR0_MAIN_MODULE_NAME,
                             pszPfHandlerR0 ? pszPfHandlerR0 : "pgmPhysHandlerRedirectToHC", rc));
    }
    else
        AssertMsgFailed(("Failed to resolve %s.%s, rc=%Rrc.\n", pszHandlerR0 ? pszModR0 : VMMR0_MAIN_MODULE_NAME,
                         pszHandlerR0 ? pszHandlerR0 : "pgmPhysHandlerRedirectToHC", rc));

    return rc;
}


/**
 * Updates the physical page access handlers.
 *
 * @param   pVM     The cross context VM structure.
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
        {
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_NONE);

            /* Tell NEM about the protection change. */
            if (VM_IS_NEM_ENABLED(pVM))
            {
                uint8_t     u2State = PGM_PAGE_GET_NEM_STATE(pPage);
                PGMPAGETYPE enmType = (PGMPAGETYPE)PGM_PAGE_GET_TYPE(pPage);
                NEMHCNotifyPhysPageProtChanged(pVM, GCPhys, PGM_PAGE_GET_HCPHYS(pPage),
                                               pgmPhysPageCalcNemProtection(pPage, enmType), enmType, &u2State);
                PGM_PAGE_SET_NEM_STATE(pPage, u2State);
            }
        }
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
        {
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, uState);

            /* Tell NEM about the protection change. */
            if (VM_IS_NEM_ENABLED(pVM))
            {
                uint8_t     u2State = PGM_PAGE_GET_NEM_STATE(pPage);
                PGMPAGETYPE enmType = (PGMPAGETYPE)PGM_PAGE_GET_TYPE(pPage);
                NEMHCNotifyPhysPageProtChanged(pVM, GCPhys, PGM_PAGE_GET_HCPHYS(pPage),
                                               pgmPhysPageCalcNemProtection(pPage, enmType), enmType, &u2State);
                PGM_PAGE_SET_NEM_STATE(pPage, u2State);
            }
        }
        else
            AssertRC(rc);

        if (--cPages == 0)
            return 0;
        GCPhys += PAGE_SIZE;
    }
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
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments. phys or virt.
 */
DECLCALLBACK(void) pgmR3InfoHandlers(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /*
     * Parse options.
     */
    PGMHANDLERINFOARG Args = { pHlp, pVM, /* .fStats = */ true };
    if (pszArgs)
        Args.fStats = strstr(pszArgs, "nost") == NULL;

    /*
     * Dump the handlers.
     */
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
                    "%RGp - %RGp  %RHv  %RHv  %RHv  %RHv  %s  %s\n",
                    pCur->Core.Key, pCur->Core.KeyLast, pCurType->pfnHandlerR3, pCur->pvUserR3,
                    pCurType->pfnPfHandlerR0, pCur->pvUserR0, pszType, pCur->pszDesc);
#ifdef VBOX_WITH_STATISTICS
    if (pArgs->fStats)
        pHlp->pfnPrintf(pHlp, "   cPeriods: %9RU64  cTicks: %11RU64  Min: %11RU64  Avg: %11RU64 Max: %11RU64\n",
                        pCur->Stat.cPeriods, pCur->Stat.cTicks, pCur->Stat.cTicksMin,
                        pCur->Stat.cPeriods ? pCur->Stat.cTicks / pCur->Stat.cPeriods : 0, pCur->Stat.cTicksMax);
#endif
    return 0;
}

