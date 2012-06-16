/* $Id$ */
/** @file
 * PGM - Page Manager / Monitor, Access Handlers.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <VBox/vmm/iom.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/dbgf.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/vmm/selm.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(PVM pVM, PPGMPHYSHANDLER pCur, PPGMRAMRANGE pRam);
static void pgmHandlerPhysicalDeregisterNotifyREM(PVM pVM, PPGMPHYSHANDLER pCur);
static void pgmHandlerPhysicalResetRamFlags(PVM pVM, PPGMPHYSHANDLER pCur);



/**
 * Register a access handler for a physical range.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS when successfully installed.
 * @retval  VINF_PGM_GCPHYS_ALIASED when the shadow PTs could be updated because
 *          the guest page aliased or/and mapped by multiple PTs. A CR3 sync has been
 *          flagged together with a pool clearing.
 * @retval  VERR_PGM_HANDLER_PHYSICAL_CONFLICT if the range conflicts with an existing
 *          one. A debug assertion is raised.
 *
 * @param   pVM             VM Handle.
 * @param   enmType         Handler type. Any of the PGMPHYSHANDLERTYPE_PHYSICAL* enums.
 * @param   GCPhys          Start physical address.
 * @param   GCPhysLast      Last physical address. (inclusive)
 * @param   pfnHandlerR3    The R3 handler.
 * @param   pvUserR3        User argument to the R3 handler.
 * @param   pfnHandlerR0    The R0 handler.
 * @param   pvUserR0        User argument to the R0 handler.
 * @param   pfnHandlerRC    The RC handler.
 * @param   pvUserRC        User argument to the RC handler. This can be a value
 *                          less that 0x10000 or a (non-null) pointer that is
 *                          automatically relocated.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
VMMDECL(int) PGMHandlerPhysicalRegisterEx(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                          R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                          R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                          RCPTRTYPE(PFNPGMRCPHYSHANDLER) pfnHandlerRC, RTRCPTR pvUserRC,
                                          R3PTRTYPE(const char *) pszDesc)
{
    Log(("PGMHandlerPhysicalRegisterEx: enmType=%d GCPhys=%RGp GCPhysLast=%RGp pfnHandlerR3=%RHv pvUserR3=%RHv pfnHandlerR0=%RHv pvUserR0=%RHv pfnHandlerGC=%RRv pvUserGC=%RRv pszDesc=%s\n",
          enmType, GCPhys, GCPhysLast, pfnHandlerR3, pvUserR3, pfnHandlerR0, pvUserR0, pfnHandlerRC, pvUserRC, R3STRING(pszDesc)));

    /*
     * Validate input.
     */
    AssertMsgReturn(GCPhys < GCPhysLast, ("GCPhys >= GCPhysLast (%#x >= %#x)\n", GCPhys, GCPhysLast), VERR_INVALID_PARAMETER);
    switch (enmType)
    {
        case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE:
            break;
        case PGMPHYSHANDLERTYPE_MMIO:
        case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:
            /* Simplification for PGMPhysRead, PGMR0Trap0eHandlerNPMisconfig and others. */
            AssertMsgReturn(!(GCPhys & PAGE_OFFSET_MASK), ("%RGp\n", GCPhys), VERR_INVALID_PARAMETER);
            AssertMsgReturn((GCPhysLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK, ("%RGp\n", GCPhysLast), VERR_INVALID_PARAMETER);
            break;
        default:
            AssertMsgFailed(("Invalid input enmType=%d!\n", enmType));
            return VERR_INVALID_PARAMETER;
    }
    AssertMsgReturn(    (RTRCUINTPTR)pvUserRC < 0x10000
                    ||  MMHyperR3ToRC(pVM, MMHyperRCToR3(pVM, pvUserRC)) == pvUserRC,
                    ("Not RC pointer! pvUserRC=%RRv\n", pvUserRC),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(    (RTR0UINTPTR)pvUserR0 < 0x10000
                    ||  MMHyperR3ToR0(pVM, MMHyperR0ToR3(pVM, pvUserR0)) == pvUserR0,
                    ("Not R0 pointer! pvUserR0=%RHv\n", pvUserR0),
                    VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfnHandlerR3, VERR_INVALID_POINTER);
    AssertReturn(pfnHandlerR0, VERR_INVALID_PARAMETER);
    AssertReturn(pfnHandlerRC, VERR_INVALID_PARAMETER);

    /*
     * We require the range to be within registered ram.
     * There is no apparent need to support ranges which cover more than one ram range.
     */
    PPGMRAMRANGE pRam = pgmPhysGetRange(pVM, GCPhys);
    if (   !pRam
        || GCPhysLast < pRam->GCPhys
        || GCPhys > pRam->GCPhysLast)
    {
#ifdef IN_RING3
        DBGFR3Info(pVM, "phys", NULL, NULL);
#endif
        AssertMsgFailed(("No RAM range for %RGp-%RGp\n", GCPhys, GCPhysLast));
        return VERR_PGM_HANDLER_PHYSICAL_NO_RAM_RANGE;
    }

    /*
     * Allocate and initialize the new entry.
     */
    PPGMPHYSHANDLER pNew;
    int rc = MMHyperAlloc(pVM, sizeof(*pNew), 0, MM_TAG_PGM_HANDLERS, (void **)&pNew);
    if (RT_FAILURE(rc))
        return rc;

    pNew->Core.Key      = GCPhys;
    pNew->Core.KeyLast  = GCPhysLast;
    pNew->enmType       = enmType;
    pNew->cPages        = (GCPhysLast - (GCPhys & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
    pNew->cAliasedPages = 0;
    pNew->cTmpOffPages  = 0;
    pNew->pfnHandlerR3  = pfnHandlerR3;
    pNew->pvUserR3      = pvUserR3;
    pNew->pfnHandlerR0  = pfnHandlerR0;
    pNew->pvUserR0      = pvUserR0;
    pNew->pfnHandlerRC  = pfnHandlerRC;
    pNew->pvUserRC      = pvUserRC;
    pNew->pszDesc       = pszDesc;

    pgmLock(pVM);

    /*
     * Try insert into list.
     */
    if (RTAvlroGCPhysInsert(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, &pNew->Core))
    {
        rc = pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(pVM, pNew, pRam);
        if (rc == VINF_PGM_SYNC_CR3)
            rc = VINF_PGM_GCPHYS_ALIASED;
        pgmUnlock(pVM);
#ifdef VBOX_WITH_REM
# ifndef IN_RING3
        REMNotifyHandlerPhysicalRegister(pVM, enmType, GCPhys, GCPhysLast - GCPhys + 1, !!pfnHandlerR3);
# else
        REMR3NotifyHandlerPhysicalRegister(pVM, enmType, GCPhys, GCPhysLast - GCPhys + 1, !!pfnHandlerR3);
# endif
#endif
        if (rc != VINF_SUCCESS)
            Log(("PGMHandlerPhysicalRegisterEx: returns %Rrc (%RGp-%RGp)\n", rc, GCPhys, GCPhysLast));
        return rc;
    }

    pgmUnlock(pVM);

#if defined(IN_RING3) && defined(VBOX_STRICT)
    DBGFR3Info(pVM, "handlers", "phys nostats", NULL);
#endif
    AssertMsgFailed(("Conflict! GCPhys=%RGp GCPhysLast=%RGp pszDesc=%s\n", GCPhys, GCPhysLast, pszDesc));
    MMHyperFree(pVM, pNew);
    return VERR_PGM_HANDLER_PHYSICAL_CONFLICT;
}


/**
 * Sets ram range flags and attempts updating shadow PTs.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS when shadow PTs was successfully updated.
 * @retval  VINF_PGM_SYNC_CR3 when the shadow PTs could be updated because
 *          the guest page aliased or/and mapped by multiple PTs. FFs set.
 * @param   pVM     Pointer to the VM.
 * @param   pCur    The physical handler.
 * @param   pRam    The RAM range.
 */
static int pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(PVM pVM, PPGMPHYSHANDLER pCur, PPGMRAMRANGE pRam)
{
    /*
     * Iterate the guest ram pages updating the flags and flushing PT entries
     * mapping the page.
     */
    bool            fFlushTLBs = false;
    int             rc         = VINF_SUCCESS;
    const unsigned  uState     = pgmHandlerPhysicalCalcState(pCur);
    uint32_t        cPages     = pCur->cPages;
    uint32_t        i          = (pCur->Core.Key - pRam->GCPhys) >> PAGE_SHIFT;
    for (;;)
    {
        PPGMPAGE pPage = &pRam->aPages[i];
        AssertMsg(pCur->enmType != PGMPHYSHANDLERTYPE_MMIO || PGM_PAGE_IS_MMIO(pPage),
                  ("%RGp %R[pgmpage]\n", pRam->GCPhys + (i << PAGE_SHIFT), pPage));

        /* Only do upgrades. */
        if (PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) < uState)
        {
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, uState);

            int rc2 = pgmPoolTrackUpdateGCPhys(pVM, pRam->GCPhys + (i << PAGE_SHIFT), pPage,
                                               false /* allow updates of PTEs (instead of flushing) */, &fFlushTLBs);
            if (rc2 != VINF_SUCCESS && rc == VINF_SUCCESS)
                rc = rc2;
        }

        /* next */
        if (--cPages == 0)
            break;
        i++;
    }

    if (fFlushTLBs)
    {
        PGM_INVL_ALL_VCPU_TLBS(pVM);
        Log(("pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs: flushing guest TLBs; rc=%d\n", rc));
    }
    else
        Log(("pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs: doesn't flush guest TLBs. rc=%Rrc; sync flags=%x VMCPU_FF_PGM_SYNC_CR3=%d\n", rc, VMMGetCpu(pVM)->pgm.s.fSyncFlags, VMCPU_FF_ISSET(VMMGetCpu(pVM), VMCPU_FF_PGM_SYNC_CR3)));

    return rc;
}


/**
 * Register a physical page access handler.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle.
 * @param   GCPhys      Start physical address.
 */
VMMDECL(int)  PGMHandlerPhysicalDeregister(PVM pVM, RTGCPHYS GCPhys)
{
    /*
     * Find the handler.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (pCur)
    {
        LogFlow(("PGMHandlerPhysicalDeregister: Removing Range %RGp-%RGp %s\n",
                 pCur->Core.Key, pCur->Core.KeyLast, R3STRING(pCur->pszDesc)));

        /*
         * Clear the page bits, notify the REM about this change and clear
         * the cache.
         */
        pgmHandlerPhysicalResetRamFlags(pVM, pCur);
        pgmHandlerPhysicalDeregisterNotifyREM(pVM, pCur);
        pVM->pgm.s.pLastPhysHandlerR0 = 0;
        pVM->pgm.s.pLastPhysHandlerR3 = 0;
        pVM->pgm.s.pLastPhysHandlerRC = 0;
        MMHyperFree(pVM, pCur);
        pgmUnlock(pVM);
        return VINF_SUCCESS;
    }
    pgmUnlock(pVM);

    AssertMsgFailed(("Didn't find range starting at %RGp\n", GCPhys));
    return VERR_PGM_HANDLER_NOT_FOUND;
}


/**
 * Shared code with modify.
 */
static void pgmHandlerPhysicalDeregisterNotifyREM(PVM pVM, PPGMPHYSHANDLER pCur)
{
    RTGCPHYS    GCPhysStart = pCur->Core.Key;
    RTGCPHYS    GCPhysLast = pCur->Core.KeyLast;

    /*
     * Page align the range.
     *
     * Since we've reset (recalculated) the physical handler state of all pages
     * we can make use of the page states to figure out whether a page should be
     * included in the REM notification or not.
     */
    if (    (pCur->Core.Key & PAGE_OFFSET_MASK)
        ||  ((pCur->Core.KeyLast + 1) & PAGE_OFFSET_MASK))
    {
        Assert(pCur->enmType != PGMPHYSHANDLERTYPE_MMIO);

        if (GCPhysStart & PAGE_OFFSET_MASK)
        {
            PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhysStart);
            if (    pPage
                &&  PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) != PGM_PAGE_HNDL_PHYS_STATE_NONE)
            {
                RTGCPHYS GCPhys = (GCPhysStart + (PAGE_SIZE - 1)) & X86_PTE_PAE_PG_MASK;
                if (    GCPhys > GCPhysLast
                    ||  GCPhys < GCPhysStart)
                    return;
                GCPhysStart = GCPhys;
            }
            else
                GCPhysStart &= X86_PTE_PAE_PG_MASK;
            Assert(!pPage || PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO); /* these are page aligned atm! */
        }

        if (GCPhysLast & PAGE_OFFSET_MASK)
        {
            PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhysLast);
            if (    pPage
                &&  PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) != PGM_PAGE_HNDL_PHYS_STATE_NONE)
            {
                RTGCPHYS GCPhys = (GCPhysLast & X86_PTE_PAE_PG_MASK) - 1;
                if (    GCPhys < GCPhysStart
                    ||  GCPhys > GCPhysLast)
                    return;
                GCPhysLast = GCPhys;
            }
            else
                GCPhysLast |= PAGE_OFFSET_MASK;
            Assert(!pPage || PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO); /* these are page aligned atm! */
        }
    }

    /*
     * Tell REM.
     */
    const bool fRestoreAsRAM = pCur->pfnHandlerR3
                            && pCur->enmType != PGMPHYSHANDLERTYPE_MMIO; /** @todo this isn't entirely correct. */
#ifdef VBOX_WITH_REM
# ifndef IN_RING3
    REMNotifyHandlerPhysicalDeregister(pVM, pCur->enmType, GCPhysStart, GCPhysLast - GCPhysStart + 1, !!pCur->pfnHandlerR3, fRestoreAsRAM);
# else
    REMR3NotifyHandlerPhysicalDeregister(pVM, pCur->enmType, GCPhysStart, GCPhysLast - GCPhysStart + 1, !!pCur->pfnHandlerR3, fRestoreAsRAM);
# endif
#endif
}


/**
 * pgmHandlerPhysicalResetRamFlags helper that checks for other handlers on
 * edge pages.
 */
DECLINLINE(void) pgmHandlerPhysicalRecalcPageState(PVM pVM, RTGCPHYS GCPhys, bool fAbove, PPGMRAMRANGE *ppRamHint)
{
    /*
     * Look for other handlers.
     */
    unsigned uState = PGM_PAGE_HNDL_PHYS_STATE_NONE;
    for (;;)
    {
        PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys, fAbove);
        if (    !pCur
            ||  ((fAbove ? pCur->Core.Key : pCur->Core.KeyLast) >> PAGE_SHIFT) != (GCPhys >> PAGE_SHIFT))
            break;
        unsigned uThisState = pgmHandlerPhysicalCalcState(pCur);
        uState = RT_MAX(uState, uThisState);

        /* next? */
        RTGCPHYS GCPhysNext = fAbove
                            ? pCur->Core.KeyLast + 1
                            : pCur->Core.Key - 1;
        if ((GCPhysNext >> PAGE_SHIFT) != (GCPhys >> PAGE_SHIFT))
            break;
        GCPhys = GCPhysNext;
    }

    /*
     * Update if we found something that is a higher priority
     * state than the current.
     */
    if (uState != PGM_PAGE_HNDL_PHYS_STATE_NONE)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pPage, ppRamHint);
        if (    RT_SUCCESS(rc)
            &&  PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) < uState)
        {
            /* This should normally not be necessary. */
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, uState);
            bool fFlushTLBs ;
            rc = pgmPoolTrackUpdateGCPhys(pVM, GCPhys, pPage, false /*fFlushPTEs*/, &fFlushTLBs);
            if (RT_SUCCESS(rc) && fFlushTLBs)
                PGM_INVL_ALL_VCPU_TLBS(pVM);
            else
                AssertRC(rc);
        }
        else
            AssertRC(rc);
    }
}


/**
 * Resets an aliased page.
 *
 * @param   pVM             The VM.
 * @param   pPage           The page.
 * @param   GCPhysPage      The page address in case it comes in handy.
 * @param   fDoAccounting   Whether to perform accounting.  (Only set during
 *                          reset where pgmR3PhysRamReset doesn't have the
 *                          handler structure handy.)
 */
void pgmHandlerPhysicalResetAliasedPage(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhysPage, bool fDoAccounting)
{
    Assert(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO);
    Assert(PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) == PGM_PAGE_HNDL_PHYS_STATE_DISABLED);

    /*
     * Flush any shadow page table references *first*.
     */
    bool fFlushTLBs = false;
    int rc = pgmPoolTrackUpdateGCPhys(pVM, GCPhysPage, pPage, true /*fFlushPTEs*/, &fFlushTLBs);
    AssertLogRelRCReturnVoid(rc);
# ifdef IN_RC
    if (fFlushTLBs && rc != VINF_PGM_SYNC_CR3)
        PGM_INVL_VCPU_TLBS(VMMGetCpu0(pVM));
# else
    HWACCMFlushTLBOnAllVCpus(pVM);
# endif

    /*
     * Make it an MMIO/Zero page.
     */
    PGM_PAGE_SET_HCPHYS(pVM, pPage, pVM->pgm.s.HCPhysZeroPg);
    PGM_PAGE_SET_TYPE(pVM, pPage, PGMPAGETYPE_MMIO);
    PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ZERO);
    PGM_PAGE_SET_PAGEID(pVM, pPage, NIL_GMM_PAGEID);
    PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_ALL);

    /* Flush its TLB entry. */
    pgmPhysInvalidatePageMapTLBEntry(pVM, GCPhysPage);

    /*
     * Do accounting for pgmR3PhysRamReset.
     */
    if (fDoAccounting)
    {
        PPGMPHYSHANDLER pHandler = pgmHandlerPhysicalLookup(pVM, GCPhysPage);
        if (RT_LIKELY(pHandler))
        {
            Assert(pHandler->cAliasedPages > 0);
            pHandler->cAliasedPages--;
        }
        else
            AssertFailed();
    }
}


/**
 * Resets ram range flags.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS when shadow PTs was successfully updated.
 * @param   pVM     Pointer to the VM.
 * @param   pCur    The physical handler.
 *
 * @remark  We don't start messing with the shadow page tables, as we've
 *          already got code in Trap0e which deals with out of sync handler
 *          flags (originally conceived for global pages).
 */
static void pgmHandlerPhysicalResetRamFlags(PVM pVM, PPGMPHYSHANDLER pCur)
{
    /*
     * Iterate the guest ram pages updating the state.
     */
    RTUINT          cPages   = pCur->cPages;
    RTGCPHYS        GCPhys   = pCur->Core.Key;
    PPGMRAMRANGE    pRamHint = NULL;
    for (;;)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pPage, &pRamHint);
        if (RT_SUCCESS(rc))
        {
            /* Reset MMIO2 for MMIO pages to MMIO, since this aliasing is our business.
               (We don't flip MMIO to RAM though, that's PGMPhys.cpp's job.)  */
            if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO)
            {
                Assert(pCur->cAliasedPages > 0);
                pgmHandlerPhysicalResetAliasedPage(pVM, pPage, GCPhys, false /*fDoAccounting*/);
                pCur->cAliasedPages--;
            }
            AssertMsg(pCur->enmType != PGMPHYSHANDLERTYPE_MMIO || PGM_PAGE_IS_MMIO(pPage), ("%RGp %R[pgmpage]\n", GCPhys, pPage));
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_NONE);
        }
        else
            AssertRC(rc);

        /* next */
        if (--cPages == 0)
            break;
        GCPhys += PAGE_SIZE;
    }

    pCur->cAliasedPages = 0;
    pCur->cTmpOffPages  = 0;

    /*
     * Check for partial start and end pages.
     */
    if (pCur->Core.Key & PAGE_OFFSET_MASK)
        pgmHandlerPhysicalRecalcPageState(pVM, pCur->Core.Key - 1, false /* fAbove */, &pRamHint);
    if ((pCur->Core.KeyLast & PAGE_OFFSET_MASK) != PAGE_OFFSET_MASK)
        pgmHandlerPhysicalRecalcPageState(pVM, pCur->Core.KeyLast + 1, true /* fAbove */, &pRamHint);
}


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
VMMDECL(int) PGMHandlerPhysicalModify(PVM pVM, RTGCPHYS GCPhysCurrent, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast)
{
    /*
     * Remove it.
     */
    int rc;
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhysCurrent);
    if (pCur)
    {
        /*
         * Clear the ram flags. (We're gonna move or free it!)
         */
        pgmHandlerPhysicalResetRamFlags(pVM, pCur);
        const bool fRestoreAsRAM = pCur->pfnHandlerR3
                                && pCur->enmType != PGMPHYSHANDLERTYPE_MMIO; /** @todo this isn't entirely correct. */

        /*
         * Validate the new range, modify and reinsert.
         */
        if (GCPhysLast >= GCPhys)
        {
            /*
             * We require the range to be within registered ram.
             * There is no apparent need to support ranges which cover more than one ram range.
             */
            PPGMRAMRANGE pRam = pgmPhysGetRange(pVM, GCPhys);
            if (   pRam
                && GCPhys <= pRam->GCPhysLast
                && GCPhysLast >= pRam->GCPhys)
            {
                pCur->Core.Key      = GCPhys;
                pCur->Core.KeyLast  = GCPhysLast;
                pCur->cPages        = (GCPhysLast - (GCPhys & X86_PTE_PAE_PG_MASK) + 1) >> PAGE_SHIFT;

                if (RTAvlroGCPhysInsert(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, &pCur->Core))
                {
                    PGMPHYSHANDLERTYPE  enmType       = pCur->enmType;
                    RTGCPHYS            cb            = GCPhysLast - GCPhys + 1;
                    bool                fHasHCHandler = !!pCur->pfnHandlerR3;

                    /*
                     * Set ram flags, flush shadow PT entries and finally tell REM about this.
                     */
                    rc = pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(pVM, pCur, pRam);
                    pgmUnlock(pVM);

#ifdef VBOX_WITH_REM
# ifndef IN_RING3
                    REMNotifyHandlerPhysicalModify(pVM, enmType, GCPhysCurrent, GCPhys, cb,
                                                   fHasHCHandler, fRestoreAsRAM);
# else
                    REMR3NotifyHandlerPhysicalModify(pVM, enmType, GCPhysCurrent, GCPhys, cb,
                                                     fHasHCHandler, fRestoreAsRAM);
# endif
#endif
                    PGM_INVL_ALL_VCPU_TLBS(pVM);
                    Log(("PGMHandlerPhysicalModify: GCPhysCurrent=%RGp -> GCPhys=%RGp GCPhysLast=%RGp\n",
                         GCPhysCurrent, GCPhys, GCPhysLast));
                    return VINF_SUCCESS;
                }

                AssertMsgFailed(("Conflict! GCPhys=%RGp GCPhysLast=%RGp\n", GCPhys, GCPhysLast));
                rc = VERR_PGM_HANDLER_PHYSICAL_CONFLICT;
            }
            else
            {
                AssertMsgFailed(("No RAM range for %RGp-%RGp\n", GCPhys, GCPhysLast));
                rc = VERR_PGM_HANDLER_PHYSICAL_NO_RAM_RANGE;
            }
        }
        else
        {
            AssertMsgFailed(("Invalid range %RGp-%RGp\n", GCPhys, GCPhysLast));
            rc = VERR_INVALID_PARAMETER;
        }

        /*
         * Invalid new location, flush the cache and free it.
         * We've only gotta notify REM and free the memory.
         */
        pgmHandlerPhysicalDeregisterNotifyREM(pVM, pCur);
        pVM->pgm.s.pLastPhysHandlerR0 = 0;
        pVM->pgm.s.pLastPhysHandlerR3 = 0;
        pVM->pgm.s.pLastPhysHandlerRC = 0;
        MMHyperFree(pVM, pCur);
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %RGp\n", GCPhysCurrent));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }

    pgmUnlock(pVM);
    return rc;
}


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
 * @param   pfnHandlerRC    The RC handler.
 * @param   pvUserRC        User argument to the RC handler. Values larger or
 *                          equal to 0x10000 will be relocated automatically.
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
VMMDECL(int) PGMHandlerPhysicalChangeCallbacks(PVM pVM, RTGCPHYS GCPhys,
                                               R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                               R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                               RCPTRTYPE(PFNPGMRCPHYSHANDLER) pfnHandlerRC, RTRCPTR pvUserRC,
                                               R3PTRTYPE(const char *) pszDesc)
{
    /*
     * Get the handler.
     */
    int rc = VINF_SUCCESS;
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (pCur)
    {
        /*
         * Change callbacks.
         */
        pCur->pfnHandlerR3  = pfnHandlerR3;
        pCur->pvUserR3      = pvUserR3;
        pCur->pfnHandlerR0  = pfnHandlerR0;
        pCur->pvUserR0      = pvUserR0;
        pCur->pfnHandlerRC  = pfnHandlerRC;
        pCur->pvUserRC      = pvUserRC;
        pCur->pszDesc       = pszDesc;
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %RGp\n", GCPhys));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * Splits a physical access handler in two.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys          Start physical address of the handler.
 * @param   GCPhysSplit     The split address.
 */
VMMDECL(int) PGMHandlerPhysicalSplit(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysSplit)
{
    AssertReturn(GCPhys < GCPhysSplit, VERR_INVALID_PARAMETER);

    /*
     * Do the allocation without owning the lock.
     */
    PPGMPHYSHANDLER pNew;
    int rc = MMHyperAlloc(pVM, sizeof(*pNew), 0, MM_TAG_PGM_HANDLERS, (void **)&pNew);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Get the handler.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (RT_LIKELY(pCur))
    {
        if (RT_LIKELY(GCPhysSplit <= pCur->Core.KeyLast))
        {
            /*
             * Create new handler node for the 2nd half.
             */
            *pNew = *pCur;
            pNew->Core.Key      = GCPhysSplit;
            pNew->cPages        = (pNew->Core.KeyLast - (pNew->Core.Key & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;

            pCur->Core.KeyLast  = GCPhysSplit - 1;
            pCur->cPages        = (pCur->Core.KeyLast - (pCur->Core.Key & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;

            if (RT_LIKELY(RTAvlroGCPhysInsert(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, &pNew->Core)))
            {
                LogFlow(("PGMHandlerPhysicalSplit: %RGp-%RGp and %RGp-%RGp\n",
                         pCur->Core.Key, pCur->Core.KeyLast, pNew->Core.Key, pNew->Core.KeyLast));
                pgmUnlock(pVM);
                return VINF_SUCCESS;
            }
            AssertMsgFailed(("whu?\n"));
            rc = VERR_PGM_PHYS_HANDLER_IPE;
        }
        else
        {
            AssertMsgFailed(("outside range: %RGp-%RGp split %RGp\n", pCur->Core.Key, pCur->Core.KeyLast, GCPhysSplit));
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %RGp\n", GCPhys));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }
    pgmUnlock(pVM);
    MMHyperFree(pVM, pNew);
    return rc;
}


/**
 * Joins up two adjacent physical access handlers which has the same callbacks.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys1         Start physical address of the first handler.
 * @param   GCPhys2         Start physical address of the second handler.
 */
VMMDECL(int) PGMHandlerPhysicalJoin(PVM pVM, RTGCPHYS GCPhys1, RTGCPHYS GCPhys2)
{
    /*
     * Get the handlers.
     */
    int rc;
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur1 = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys1);
    if (RT_LIKELY(pCur1))
    {
        PPGMPHYSHANDLER pCur2 = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys2);
        if (RT_LIKELY(pCur2))
        {
            /*
             * Make sure that they are adjacent, and that they've got the same callbacks.
             */
            if (RT_LIKELY(pCur1->Core.KeyLast + 1 == pCur2->Core.Key))
            {
                if (RT_LIKELY(    pCur1->pfnHandlerRC == pCur2->pfnHandlerRC
                              &&  pCur1->pfnHandlerR0 == pCur2->pfnHandlerR0
                              &&  pCur1->pfnHandlerR3 == pCur2->pfnHandlerR3))
                {
                    PPGMPHYSHANDLER pCur3 = (PPGMPHYSHANDLER)RTAvlroGCPhysRemove(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys2);
                    if (RT_LIKELY(pCur3 == pCur2))
                    {
                        pCur1->Core.KeyLast  = pCur2->Core.KeyLast;
                        pCur1->cPages        = (pCur1->Core.KeyLast - (pCur1->Core.Key & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
                        LogFlow(("PGMHandlerPhysicalJoin: %RGp-%RGp %RGp-%RGp\n",
                                 pCur1->Core.Key, pCur1->Core.KeyLast, pCur2->Core.Key, pCur2->Core.KeyLast));
                        pVM->pgm.s.pLastPhysHandlerR0 = 0;
                        pVM->pgm.s.pLastPhysHandlerR3 = 0;
                        pVM->pgm.s.pLastPhysHandlerRC = 0;
                        MMHyperFree(pVM, pCur2);
                        pgmUnlock(pVM);
                        return VINF_SUCCESS;
                    }

                    Assert(pCur3 == pCur2);
                    rc = VERR_PGM_PHYS_HANDLER_IPE;
                }
                else
                {
                    AssertMsgFailed(("mismatching handlers\n"));
                    rc = VERR_ACCESS_DENIED;
                }
            }
            else
            {
                AssertMsgFailed(("not adjacent: %RGp-%RGp %RGp-%RGp\n",
                                 pCur1->Core.Key, pCur1->Core.KeyLast, pCur2->Core.Key, pCur2->Core.KeyLast));
                rc = VERR_INVALID_PARAMETER;
            }
        }
        else
        {
            AssertMsgFailed(("Didn't find range starting at %RGp\n", GCPhys2));
            rc = VERR_PGM_HANDLER_NOT_FOUND;
        }
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %RGp\n", GCPhys1));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }
    pgmUnlock(pVM);
    return rc;

}


/**
 * Resets any modifications to individual pages in a physical page access
 * handler region.
 *
 * This is used in pair with PGMHandlerPhysicalPageTempOff(),
 * PGMHandlerPhysicalPageAlias() or PGMHandlerPhysicalPageAliasHC().
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   GCPhys      The start address of the handler regions, i.e. what you
 *                      passed to PGMR3HandlerPhysicalRegister(),
 *                      PGMHandlerPhysicalRegisterEx() or
 *                      PGMHandlerPhysicalModify().
 */
VMMDECL(int) PGMHandlerPhysicalReset(PVM pVM, RTGCPHYS GCPhys)
{
    LogFlow(("PGMHandlerPhysicalReset GCPhys=%RGp\n", GCPhys));
    pgmLock(pVM);

    /*
     * Find the handler.
     */
    int rc;
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (RT_LIKELY(pCur))
    {
        /*
         * Validate type.
         */
        switch (pCur->enmType)
        {
            case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE:
            case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:
            case PGMPHYSHANDLERTYPE_MMIO: /* NOTE: Only use when clearing MMIO ranges with aliased MMIO2 pages! */
            {
                STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysHandlerReset)); /**@Todo move out of switch */
                PPGMRAMRANGE pRam = pgmPhysGetRange(pVM, GCPhys);
                Assert(pRam);
                Assert(pRam->GCPhys     <= pCur->Core.Key);
                Assert(pRam->GCPhysLast >= pCur->Core.KeyLast);

                if (pCur->enmType == PGMPHYSHANDLERTYPE_MMIO)
                {
                    /*
                     * Reset all the PGMPAGETYPE_MMIO2_ALIAS_MMIO pages first and that's it.
                     * This could probably be optimized a bit wrt to flushing, but I'm too lazy
                     * to do that now...
                     */
                    if (pCur->cAliasedPages)
                    {
                        PPGMPAGE    pPage = &pRam->aPages[(pCur->Core.Key - pRam->GCPhys) >> PAGE_SHIFT];
                        uint32_t    cLeft = pCur->cPages;
                        while (cLeft-- > 0)
                        {
                            if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO)
                            {
                                Assert(pCur->cAliasedPages > 0);
                                pgmHandlerPhysicalResetAliasedPage(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)cLeft << PAGE_SHIFT),
                                                                   false /*fDoAccounting*/);
                                --pCur->cAliasedPages;
#ifndef VBOX_STRICT
                                if (pCur->cAliasedPages == 0)
                                    break;
#endif
                            }
                            Assert(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO);
                            pPage++;
                        }
                        Assert(pCur->cAliasedPages == 0);
                    }
                }
                else if (pCur->cTmpOffPages > 0)
                {
                    /*
                     * Set the flags and flush shadow PT entries.
                     */
                    rc = pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(pVM, pCur, pRam);
                }

                pCur->cAliasedPages = 0;
                pCur->cTmpOffPages  = 0;

                rc = VINF_SUCCESS;
                break;
            }

            /*
             * Invalid.
             */
            default:
                AssertMsgFailed(("Invalid type %d! Corruption!\n",  pCur->enmType));
                rc = VERR_PGM_PHYS_HANDLER_IPE;
                break;
        }
    }
    else
    {
        AssertMsgFailed(("Didn't find MMIO Range starting at %#x\n", GCPhys));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * Temporarily turns off the access monitoring of a page within a monitored
 * physical write/all page access handler region.
 *
 * Use this when no further \#PFs are required for that page. Be aware that
 * a page directory sync might reset the flags, and turn on access monitoring
 * for the page.
 *
 * The caller must do required page table modifications.
 *
 * @returns VBox status code.
 * @param   pVM                 VM Handle
 * @param   GCPhys              The start address of the access handler. This
 *                              must be a fully page aligned range or we risk
 *                              messing up other handlers installed for the
 *                              start and end pages.
 * @param   GCPhysPage          The physical address of the page to turn off
 *                              access monitoring for.
 */
VMMDECL(int)  PGMHandlerPhysicalPageTempOff(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage)
{
    LogFlow(("PGMHandlerPhysicalPageTempOff GCPhysPage=%RGp\n", GCPhysPage));

    pgmLock(pVM);
    /*
     * Validate the range.
     */
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (RT_LIKELY(pCur))
    {
        if (RT_LIKELY(    GCPhysPage >= pCur->Core.Key
                      &&  GCPhysPage <= pCur->Core.KeyLast))
        {
            Assert(!(pCur->Core.Key & PAGE_OFFSET_MASK));
            Assert((pCur->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK);

            AssertReturnStmt(   pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE
                             || pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_ALL,
                             pgmUnlock(pVM), VERR_ACCESS_DENIED);

            /*
             * Change the page status.
             */
            PPGMPAGE pPage;
            int rc = pgmPhysGetPageEx(pVM, GCPhysPage, &pPage);
            AssertReturnStmt(RT_SUCCESS_NP(rc), pgmUnlock(pVM), rc);
            if (PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) != PGM_PAGE_HNDL_PHYS_STATE_DISABLED)
            {
                PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_DISABLED);
                pCur->cTmpOffPages++;
            }
            pgmUnlock(pVM);
            return VINF_SUCCESS;
        }
        pgmUnlock(pVM);
        AssertMsgFailed(("The page %#x is outside the range %#x-%#x\n",
                         GCPhysPage, pCur->Core.Key, pCur->Core.KeyLast));
        return VERR_INVALID_PARAMETER;
    }
    pgmUnlock(pVM);
    AssertMsgFailed(("Specified physical handler start address %#x is invalid.\n", GCPhys));
    return VERR_PGM_HANDLER_NOT_FOUND;
}


/**
 * Replaces an MMIO page with an MMIO2 page.
 *
 * This is a worker for IOMMMIOMapMMIO2Page that works in a similar way to
 * PGMHandlerPhysicalPageTempOff but for an MMIO page. Since an MMIO page has no
 * backing, the caller must provide a replacement page. For various reasons the
 * replacement page must be an MMIO2 page.
 *
 * The caller must do required page table modifications. You can get away
 * without making any modifications since it's an MMIO page, the cost is an extra
 * \#PF which will the resync the page.
 *
 * Call PGMHandlerPhysicalReset() to restore the MMIO page.
 *
 * The caller may still get handler callback even after this call and must be
 * able to deal correctly with such calls. The reason for these callbacks are
 * either that we're executing in the recompiler (which doesn't know about this
 * arrangement) or that we've been restored from saved state (where we won't
 * save the change).
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle
 * @param   GCPhys              The start address of the access handler. This
 *                              must be a fully page aligned range or we risk
 *                              messing up other handlers installed for the
 *                              start and end pages.
 * @param   GCPhysPage          The physical address of the page to turn off
 *                              access monitoring for.
 * @param   GCPhysPageRemap     The physical address of the MMIO2 page that
 *                              serves as backing memory.
 *
 * @remark  May cause a page pool flush if used on a page that is already
 *          aliased.
 *
 * @note    This trick does only work reliably if the two pages are never ever
 *          mapped in the same page table. If they are the page pool code will
 *          be confused should either of them be flushed. See the special case
 *          of zero page aliasing mentioned in #3170.
 *
 */
VMMDECL(int)  PGMHandlerPhysicalPageAlias(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage, RTGCPHYS GCPhysPageRemap)
{
///    Assert(!IOMIsLockOwner(pVM)); /* We mustn't own any other locks when calling this */

    pgmLock(pVM);
    /*
     * Lookup and validate the range.
     */
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (RT_LIKELY(pCur))
    {
        if (RT_LIKELY(    GCPhysPage >= pCur->Core.Key
                      &&  GCPhysPage <= pCur->Core.KeyLast))
        {
            AssertReturnStmt(pCur->enmType == PGMPHYSHANDLERTYPE_MMIO, pgmUnlock(pVM), VERR_ACCESS_DENIED);
            AssertReturnStmt(!(pCur->Core.Key & PAGE_OFFSET_MASK), pgmUnlock(pVM), VERR_INVALID_PARAMETER);
            AssertReturnStmt((pCur->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK, pgmUnlock(pVM), VERR_INVALID_PARAMETER);

            /*
             * Get and validate the two pages.
             */
            PPGMPAGE pPageRemap;
            int rc = pgmPhysGetPageEx(pVM, GCPhysPageRemap, &pPageRemap);
            AssertReturnStmt(RT_SUCCESS_NP(rc), pgmUnlock(pVM), rc);
            AssertMsgReturnStmt(PGM_PAGE_GET_TYPE(pPageRemap) == PGMPAGETYPE_MMIO2,
                            ("GCPhysPageRemap=%RGp %R[pgmpage]\n", GCPhysPageRemap, pPageRemap),
                            pgmUnlock(pVM), VERR_PGM_PHYS_NOT_MMIO2);

            PPGMPAGE pPage;
            rc = pgmPhysGetPageEx(pVM, GCPhysPage, &pPage);
            AssertReturnStmt(RT_SUCCESS_NP(rc), pgmUnlock(pVM), rc);
            if (PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO)
            {
                AssertMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO,
                                ("GCPhysPage=%RGp %R[pgmpage]\n", GCPhysPage, pPage),
                                VERR_PGM_PHYS_NOT_MMIO2);
                if (PGM_PAGE_GET_HCPHYS(pPage) == PGM_PAGE_GET_HCPHYS(pPageRemap))
                {
                    pgmUnlock(pVM);
                    return VINF_PGM_HANDLER_ALREADY_ALIASED;
                }

                /*
                 * The page is already mapped as some other page, reset it
                 * to an MMIO/ZERO page before doing the new mapping.
                 */
                Log(("PGMHandlerPhysicalPageAlias: GCPhysPage=%RGp (%R[pgmpage]; %RHp -> %RHp\n",
                     GCPhysPage, pPage, PGM_PAGE_GET_HCPHYS(pPage), PGM_PAGE_GET_HCPHYS(pPageRemap)));
                pgmHandlerPhysicalResetAliasedPage(pVM, pPage, GCPhysPage, false /*fDoAccounting*/);
                pCur->cAliasedPages--;
            }
            Assert(PGM_PAGE_IS_ZERO(pPage));

            /*
             * Do the actual remapping here.
             * This page now serves as an alias for the backing memory specified.
             */
            LogFlow(("PGMHandlerPhysicalPageAlias: %RGp (%R[pgmpage]) alias for %RGp (%R[pgmpage])\n",
                     GCPhysPage, pPage, GCPhysPageRemap, pPageRemap ));
            PGM_PAGE_SET_HCPHYS(pVM, pPage, PGM_PAGE_GET_HCPHYS(pPageRemap));
            PGM_PAGE_SET_TYPE(pVM, pPage, PGMPAGETYPE_MMIO2_ALIAS_MMIO);
            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
            PGM_PAGE_SET_PAGEID(pVM, pPage, PGM_PAGE_GET_PAGEID(pPageRemap));
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_DISABLED);
            pCur->cAliasedPages++;
            Assert(pCur->cAliasedPages <= pCur->cPages);

            /* Flush its TLB entry. */
            pgmPhysInvalidatePageMapTLBEntry(pVM, GCPhysPage);

            LogFlow(("PGMHandlerPhysicalPageAlias: => %R[pgmpage]\n", pPage));
            pgmUnlock(pVM);
            return VINF_SUCCESS;
        }

        pgmUnlock(pVM);
        AssertMsgFailed(("The page %#x is outside the range %#x-%#x\n",
                         GCPhysPage, pCur->Core.Key, pCur->Core.KeyLast));
        return VERR_INVALID_PARAMETER;
    }

    pgmUnlock(pVM);
    AssertMsgFailed(("Specified physical handler start address %#x is invalid.\n", GCPhys));
    return VERR_PGM_HANDLER_NOT_FOUND;
}

/**
 * Replaces an MMIO page with an arbitrary HC page.
 *
 * This is a worker for IOMMMIOMapMMIO2Page that works in a similar way to
 * PGMHandlerPhysicalPageTempOff but for an MMIO page. Since an MMIO page has no
 * backing, the caller must provide a replacement page. For various reasons the
 * replacement page must be an MMIO2 page.
 *
 * The caller must do required page table modifications. You can get away
 * without making any modifications since it's an MMIO page, the cost is an extra
 * \#PF which will the resync the page.
 *
 * Call PGMHandlerPhysicalReset() to restore the MMIO page.
 *
 * The caller may still get handler callback even after this call and must be
 * able to deal correctly with such calls. The reason for these callbacks are
 * either that we're executing in the recompiler (which doesn't know about this
 * arrangement) or that we've been restored from saved state (where we won't
 * save the change).
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle
 * @param   GCPhys              The start address of the access handler. This
 *                              must be a fully page aligned range or we risk
 *                              messing up other handlers installed for the
 *                              start and end pages.
 * @param   GCPhysPage          The physical address of the page to turn off
 *                              access monitoring for.
 * @param   HCPhysPageRemap     The physical address of the HC page that
 *                              serves as backing memory.
 *
 * @remark  May cause a page pool flush if used on a page that is already
 *          aliased.
 */
VMMDECL(int)  PGMHandlerPhysicalPageAliasHC(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysPage, RTHCPHYS HCPhysPageRemap)
{
///    Assert(!IOMIsLockOwner(pVM)); /* We mustn't own any other locks when calling this */

    /*
     * Lookup and validate the range.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhys);
    if (RT_LIKELY(pCur))
    {
        if (RT_LIKELY(    GCPhysPage >= pCur->Core.Key
                      &&  GCPhysPage <= pCur->Core.KeyLast))
        {
            AssertReturnStmt(pCur->enmType == PGMPHYSHANDLERTYPE_MMIO, pgmUnlock(pVM), VERR_ACCESS_DENIED);
            AssertReturnStmt(!(pCur->Core.Key & PAGE_OFFSET_MASK), pgmUnlock(pVM), VERR_INVALID_PARAMETER);
            AssertReturnStmt((pCur->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK, pgmUnlock(pVM), VERR_INVALID_PARAMETER);

            /*
             * Get and validate the pages.
             */
            PPGMPAGE pPage;
            int rc = pgmPhysGetPageEx(pVM, GCPhysPage, &pPage);
            AssertReturnStmt(RT_SUCCESS_NP(rc), pgmUnlock(pVM), rc);
            if (PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO)
            {
                pgmUnlock(pVM);
                AssertMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO,
                                ("GCPhysPage=%RGp %R[pgmpage]\n", GCPhysPage, pPage),
                                VERR_PGM_PHYS_NOT_MMIO2);
                return VINF_PGM_HANDLER_ALREADY_ALIASED;
            }
            Assert(PGM_PAGE_IS_ZERO(pPage));

            /*
             * Do the actual remapping here.
             * This page now serves as an alias for the backing memory specified.
             */
            LogFlow(("PGMHandlerPhysicalPageAlias: %RGp (%R[pgmpage]) alias for %RHp\n",
                     GCPhysPage, pPage, HCPhysPageRemap));
            PGM_PAGE_SET_HCPHYS(pVM, pPage, HCPhysPageRemap);
            PGM_PAGE_SET_TYPE(pVM, pPage, PGMPAGETYPE_MMIO2_ALIAS_MMIO);
            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
            /** @todo hack alert
             *  This needs to be done properly. Currently we get away with it as the recompiler directly calls
             *  IOM read and write functions. Access through PGMPhysRead/Write will crash the process.
             */
            PGM_PAGE_SET_PAGEID(pVM, pPage, NIL_GMM_PAGEID);
            PGM_PAGE_SET_HNDL_PHYS_STATE(pPage, PGM_PAGE_HNDL_PHYS_STATE_DISABLED);
            pCur->cAliasedPages++;
            Assert(pCur->cAliasedPages <= pCur->cPages);

            /* Flush its TLB entry. */
            pgmPhysInvalidatePageMapTLBEntry(pVM, GCPhysPage);

            LogFlow(("PGMHandlerPhysicalPageAliasHC: => %R[pgmpage]\n", pPage));
            pgmUnlock(pVM);
            return VINF_SUCCESS;
        }
        pgmUnlock(pVM);
        AssertMsgFailed(("The page %#x is outside the range %#x-%#x\n",
                         GCPhysPage, pCur->Core.Key, pCur->Core.KeyLast));
        return VERR_INVALID_PARAMETER;
    }
    pgmUnlock(pVM);

    AssertMsgFailed(("Specified physical handler start address %#x is invalid.\n", GCPhys));
    return VERR_PGM_HANDLER_NOT_FOUND;
}


/**
 * Checks if a physical range is handled
 *
 * @returns boolean
 * @param   pVM         VM Handle.
 * @param   GCPhys      Start physical address earlier passed to PGMR3HandlerPhysicalRegister().
 * @remarks Caller must take the PGM lock...
 * @thread  EMT.
 */
VMMDECL(bool) PGMHandlerPhysicalIsRegistered(PVM pVM, RTGCPHYS GCPhys)
{
    /*
     * Find the handler.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = pgmHandlerPhysicalLookup(pVM, GCPhys);
    if (pCur)
    {
        Assert(GCPhys >= pCur->Core.Key && GCPhys <= pCur->Core.KeyLast);
        Assert(     pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE
               ||   pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_ALL
               ||   pCur->enmType == PGMPHYSHANDLERTYPE_MMIO);
        pgmUnlock(pVM);
        return true;
    }
    pgmUnlock(pVM);
    return false;
}


/**
 * Checks if it's an disabled all access handler or write access handler at the
 * given address.
 *
 * @returns true if it's an all access handler, false if it's a write access
 *          handler.
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The address of the page with a disabled handler.
 *
 * @remarks The caller, PGMR3PhysTlbGCPhys2Ptr, must hold the PGM lock.
 */
bool pgmHandlerPhysicalIsAll(PVM pVM, RTGCPHYS GCPhys)
{
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = pgmHandlerPhysicalLookup(pVM, GCPhys);
    if (!pCur)
    {
        pgmUnlock(pVM);
        AssertFailed();
        return true;
    }
    Assert(     pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE
           ||   pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_ALL
           ||   pCur->enmType == PGMPHYSHANDLERTYPE_MMIO); /* sanity */
    /* Only whole pages can be disabled. */
    Assert(   pCur->Core.Key     <= (GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK)
           && pCur->Core.KeyLast >= (GCPhys | PAGE_OFFSET_MASK));

    bool bRet = pCur->enmType != PGMPHYSHANDLERTYPE_PHYSICAL_WRITE;
    pgmUnlock(pVM);
    return bRet;
}


/**
 * Check if particular guest's VA is being monitored.
 *
 * @returns true or false
 * @param   pVM             VM handle.
 * @param   GCPtr           Virtual address.
 * @remarks Will acquire the PGM lock.
 * @thread  Any.
 */
VMMDECL(bool) PGMHandlerVirtualIsRegistered(PVM pVM, RTGCPTR GCPtr)
{
    pgmLock(pVM);
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrGet(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, GCPtr);
    pgmUnlock(pVM);

    return pCur != NULL;
}


/**
 * Search for virtual handler with matching physical address
 *
 * @returns VBox status code
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      GC physical address to search for.
 * @param   ppVirt      Where to store the pointer to the virtual handler structure.
 * @param   piPage      Where to store the pointer to the index of the cached physical page.
 */
int pgmHandlerVirtualFindByPhysAddr(PVM pVM, RTGCPHYS GCPhys, PPGMVIRTHANDLER *ppVirt, unsigned *piPage)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,VirtHandlerSearchByPhys), a);
    Assert(ppVirt);

    pgmLock(pVM);
    PPGMPHYS2VIRTHANDLER pCur;
    pCur = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers, GCPhys);
    if (pCur)
    {
        /* found a match! */
        *ppVirt = (PPGMVIRTHANDLER)((uintptr_t)pCur + pCur->offVirtHandler);
        *piPage = pCur - &(*ppVirt)->aPhysToVirt[0];
        pgmUnlock(pVM);

#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
        AssertRelease(pCur->offNextAlias & PGMPHYS2VIRTHANDLER_IS_HEAD);
#endif
        LogFlow(("PHYS2VIRT: found match for %RGp -> %RGv *piPage=%#x\n", GCPhys, (*ppVirt)->Core.Key, *piPage));
        STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,VirtHandlerSearchByPhys), a);
        return VINF_SUCCESS;
    }

    pgmUnlock(pVM);
    *ppVirt = NULL;
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,VirtHandlerSearchByPhys), a);
    return VERR_PGM_HANDLER_NOT_FOUND;
}


/**
 * Deal with aliases in phys2virt.
 *
 * As pointed out by the various todos, this currently only deals with
 * aliases where the two ranges match 100%.
 *
 * @param   pVM             Pointer to the VM.
 * @param   pPhys2Virt      The node we failed insert.
 */
static void pgmHandlerVirtualInsertAliased(PVM pVM, PPGMPHYS2VIRTHANDLER pPhys2Virt)
{
    /*
     * First find the node which is conflicting with us.
     */
    /** @todo Deal with partial overlapping. (Unlikely situation, so I'm too lazy to do anything about it now.) */
    /** @todo check if the current head node covers the ground we do. This is highly unlikely
     * and I'm too lazy to implement this now as it will require sorting the list and stuff like that. */
    PPGMPHYS2VIRTHANDLER pHead = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
    AssertReleaseMsg(pHead != pPhys2Virt, ("%RGp-%RGp offVirtHandler=%#RX32\n",
                                           pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler));
#endif
    if (RT_UNLIKELY(!pHead || pHead->Core.KeyLast != pPhys2Virt->Core.KeyLast))
    {
        /** @todo do something clever here... */
        LogRel(("pgmHandlerVirtualInsertAliased: %RGp-%RGp\n", pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast));
        pPhys2Virt->offNextAlias = 0;
        return;
    }

    /*
     * Insert ourselves as the next node.
     */
    if (!(pHead->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK))
        pPhys2Virt->offNextAlias = PGMPHYS2VIRTHANDLER_IN_TREE;
    else
    {
        PPGMPHYS2VIRTHANDLER pNext = (PPGMPHYS2VIRTHANDLER)((intptr_t)pHead + (pHead->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
        pPhys2Virt->offNextAlias = ((intptr_t)pNext - (intptr_t)pPhys2Virt)
                                 | PGMPHYS2VIRTHANDLER_IN_TREE;
    }
    pHead->offNextAlias = ((intptr_t)pPhys2Virt - (intptr_t)pHead)
                        | (pHead->offNextAlias & ~PGMPHYS2VIRTHANDLER_OFF_MASK);
    Log(("pgmHandlerVirtualInsertAliased: %RGp-%RGp offNextAlias=%#RX32\n", pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias));
}


/**
 * Resets one virtual handler range.
 *
 * This is called by HandlerVirtualUpdate when it has detected some kind of
 * problem and have started clearing the virtual handler page states (or
 * when there have been registration/deregistrations). For this reason this
 * function will only update the page status if it's lower than desired.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMVIRTHANDLER.
 * @param   pvUser  Pointer to the VM.
 */
DECLCALLBACK(int) pgmHandlerVirtualResetOne(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)pNode;
    PVM             pVM = (PVM)pvUser;

    PGM_LOCK_ASSERT_OWNER(pVM);

    /*
     * Iterate the pages and apply the new state.
     */
    unsigned        uState   = pgmHandlerVirtualCalcState(pCur);
    PPGMRAMRANGE    pRamHint = NULL;
    RTGCUINTPTR     offPage  = ((RTGCUINTPTR)pCur->Core.Key & PAGE_OFFSET_MASK);
    RTGCUINTPTR     cbLeft   = pCur->cb;
    for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
    {
        PPGMPHYS2VIRTHANDLER pPhys2Virt = &pCur->aPhysToVirt[iPage];
        if (pPhys2Virt->Core.Key != NIL_RTGCPHYS)
        {
            /*
             * Update the page state wrt virtual handlers.
             */
            PPGMPAGE pPage;
            int rc = pgmPhysGetPageWithHintEx(pVM, pPhys2Virt->Core.Key, &pPage, &pRamHint);
            if (    RT_SUCCESS(rc)
                &&  PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) < uState)
                PGM_PAGE_SET_HNDL_VIRT_STATE(pPage, uState);
            else
                AssertRC(rc);

            /*
             * Need to insert the page in the Phys2Virt lookup tree?
             */
            if (pPhys2Virt->Core.KeyLast == NIL_RTGCPHYS)
            {
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                AssertRelease(!pPhys2Virt->offNextAlias);
#endif
                unsigned cbPhys = cbLeft;
                if (cbPhys > PAGE_SIZE - offPage)
                    cbPhys = PAGE_SIZE - offPage;
                else
                    Assert(iPage == pCur->cPages - 1);
                pPhys2Virt->Core.KeyLast = pPhys2Virt->Core.Key + cbPhys - 1; /* inclusive */
                pPhys2Virt->offNextAlias = PGMPHYS2VIRTHANDLER_IS_HEAD | PGMPHYS2VIRTHANDLER_IN_TREE;
                if (!RTAvlroGCPhysInsert(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers, &pPhys2Virt->Core))
                    pgmHandlerVirtualInsertAliased(pVM, pPhys2Virt);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                else
                    AssertReleaseMsg(RTAvlroGCPhysGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key) == &pPhys2Virt->Core,
                                     ("%RGp-%RGp offNextAlias=%#RX32\n",
                                      pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias));
#endif
                Log2(("PHYS2VIRT: Insert physical range %RGp-%RGp offNextAlias=%#RX32 %s\n",
                      pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias, R3STRING(pCur->pszDesc)));
            }
        }
        cbLeft -= PAGE_SIZE - offPage;
        offPage = 0;
    }

    return 0;
}

#if defined(VBOX_STRICT) || defined(LOG_ENABLED)

/**
 * Worker for pgmHandlerVirtualDumpPhysPages.
 *
 * @returns 0 (continue enumeration).
 * @param   pNode       The virtual handler node.
 * @param   pvUser      User argument, unused.
 */
static DECLCALLBACK(int) pgmHandlerVirtualDumpPhysPagesCallback(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMPHYS2VIRTHANDLER pCur = (PPGMPHYS2VIRTHANDLER)pNode;
    PPGMVIRTHANDLER      pVirt = (PPGMVIRTHANDLER)((uintptr_t)pCur + pCur->offVirtHandler);
    NOREF(pvUser); NOREF(pVirt);

    Log(("PHYS2VIRT: Range %RGp-%RGp for virtual handler: %s\n", pCur->Core.Key, pCur->Core.KeyLast, pVirt->pszDesc));
    return 0;
}


/**
 * Assertion / logging helper for dumping all the
 * virtual handlers to the log.
 *
 * @param   pVM         Pointer to the VM.
 */
void pgmHandlerVirtualDumpPhysPages(PVM pVM)
{
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers, true /* from left */,
                           pgmHandlerVirtualDumpPhysPagesCallback, 0);
}

#endif /* VBOX_STRICT || LOG_ENABLED */
#ifdef VBOX_STRICT

/**
 * State structure used by the PGMAssertHandlerAndFlagsInSync() function
 * and its AVL enumerators.
 */
typedef struct PGMAHAFIS
{
    /** The current physical address. */
    RTGCPHYS    GCPhys;
    /** The state we've calculated. */
    unsigned    uVirtStateFound;
    /** The state we're matching up to. */
    unsigned    uVirtState;
    /** Number of errors. */
    unsigned    cErrors;
    /** Pointer to the VM. */
    PVM         pVM;
} PGMAHAFIS, *PPGMAHAFIS;


#if 0 /* unused */
/**
 * Verify virtual handler by matching physical address.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMVIRTHANDLER.
 * @param   pvUser  Pointer to user parameter.
 */
static DECLCALLBACK(int) pgmHandlerVirtualVerifyOneByPhysAddr(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)pNode;
    PPGMAHAFIS      pState = (PPGMAHAFIS)pvUser;

    for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
    {
        if ((pCur->aPhysToVirt[iPage].Core.Key & X86_PTE_PAE_PG_MASK) == pState->GCPhys)
        {
            unsigned uState = pgmHandlerVirtualCalcState(pCur);
            if (pState->uVirtState < uState)
            {
                error
            }

            if (pState->uVirtState == uState)
                break; //??
        }
    }
    return 0;
}
#endif /* unused */


/**
 * Verify a virtual handler (enumeration callback).
 *
 * Called by PGMAssertHandlerAndFlagsInSync to check the sanity of all
 * the virtual handlers, esp. that the physical addresses matches up.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMVIRTHANDLER.
 * @param   pvUser  Pointer to a PPGMAHAFIS structure.
 */
static DECLCALLBACK(int) pgmHandlerVirtualVerifyOne(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pVirt   = (PPGMVIRTHANDLER)pNode;
    PPGMAHAFIS      pState  = (PPGMAHAFIS)pvUser;
    PVM             pVM     = pState->pVM;

    /*
     * Validate the type and calc state.
     */
    switch (pVirt->enmType)
    {
        case PGMVIRTHANDLERTYPE_WRITE:
        case PGMVIRTHANDLERTYPE_ALL:
            break;
        default:
            AssertMsgFailed(("unknown/wrong enmType=%d\n", pVirt->enmType));
            pState->cErrors++;
            return 0;
    }
    const unsigned uState = pgmHandlerVirtualCalcState(pVirt);

    /*
     * Check key alignment.
     */
    if (    (pVirt->aPhysToVirt[0].Core.Key & PAGE_OFFSET_MASK) != ((RTGCUINTPTR)pVirt->Core.Key & PAGE_OFFSET_MASK)
        &&  pVirt->aPhysToVirt[0].Core.Key != NIL_RTGCPHYS)
    {
        AssertMsgFailed(("virt handler phys has incorrect key! %RGp %RGv %s\n",
                         pVirt->aPhysToVirt[0].Core.Key, pVirt->Core.Key, R3STRING(pVirt->pszDesc)));
        pState->cErrors++;
    }

    if (    (pVirt->aPhysToVirt[pVirt->cPages - 1].Core.KeyLast & PAGE_OFFSET_MASK) != ((RTGCUINTPTR)pVirt->Core.KeyLast & PAGE_OFFSET_MASK)
        &&  pVirt->aPhysToVirt[pVirt->cPages - 1].Core.Key != NIL_RTGCPHYS)
    {
        AssertMsgFailed(("virt handler phys has incorrect key! %RGp %RGv %s\n",
                         pVirt->aPhysToVirt[pVirt->cPages - 1].Core.KeyLast, pVirt->Core.KeyLast, R3STRING(pVirt->pszDesc)));
        pState->cErrors++;
    }

    /*
     * Check pages for sanity and state.
     */
    RTGCUINTPTR   GCPtr = (RTGCUINTPTR)pVirt->Core.Key;
    for (unsigned iPage = 0; iPage < pVirt->cPages; iPage++, GCPtr += PAGE_SIZE)
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            RTGCPHYS   GCPhysGst;
            uint64_t   fGst;
            int rc = PGMGstGetPage(pVCpu, (RTGCPTR)GCPtr, &fGst, &GCPhysGst);
            if (    rc == VERR_PAGE_NOT_PRESENT
                ||  rc == VERR_PAGE_TABLE_NOT_PRESENT)
            {
                if (pVirt->aPhysToVirt[iPage].Core.Key != NIL_RTGCPHYS)
                {
                    AssertMsgFailed(("virt handler phys out of sync. %RGp GCPhysNew=~0 iPage=%#x %RGv %s\n",
                                    pVirt->aPhysToVirt[iPage].Core.Key, iPage, GCPtr, R3STRING(pVirt->pszDesc)));
                    pState->cErrors++;
                }
                continue;
            }

            AssertRCReturn(rc, 0);
            if ((pVirt->aPhysToVirt[iPage].Core.Key & X86_PTE_PAE_PG_MASK) != GCPhysGst)
            {
                AssertMsgFailed(("virt handler phys out of sync. %RGp GCPhysGst=%RGp iPage=%#x %RGv %s\n",
                                pVirt->aPhysToVirt[iPage].Core.Key, GCPhysGst, iPage, GCPtr, R3STRING(pVirt->pszDesc)));
                pState->cErrors++;
                continue;
            }

            PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhysGst);
            if (!pPage)
            {
                AssertMsgFailed(("virt handler getting ram flags. GCPhysGst=%RGp iPage=%#x %RGv %s\n",
                                GCPhysGst, iPage, GCPtr, R3STRING(pVirt->pszDesc)));
                pState->cErrors++;
                continue;
            }

            if (PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) < uState)
            {
                AssertMsgFailed(("virt handler state mismatch. pPage=%R[pgmpage] GCPhysGst=%RGp iPage=%#x %RGv state=%d expected>=%d %s\n",
                                pPage, GCPhysGst, iPage, GCPtr, PGM_PAGE_GET_HNDL_VIRT_STATE(pPage), uState, R3STRING(pVirt->pszDesc)));
                pState->cErrors++;
                continue;
            }
        } /* for each VCPU */
    } /* for pages in virtual mapping. */

    return 0;
}


/**
 * Asserts that the handlers+guest-page-tables == ramrange-flags and
 * that the physical addresses associated with virtual handlers are correct.
 *
 * @returns Number of mismatches.
 * @param   pVM     Pointer to the VM.
 */
VMMDECL(unsigned) PGMAssertHandlerAndFlagsInSync(PVM pVM)
{
    PPGM        pPGM = &pVM->pgm.s;
    PGMAHAFIS   State;
    State.GCPhys = 0;
    State.uVirtState = 0;
    State.uVirtStateFound = 0;
    State.cErrors = 0;
    State.pVM = pVM;

    PGM_LOCK_ASSERT_OWNER(pVM);

    /*
     * Check the RAM flags against the handlers.
     */
    for (PPGMRAMRANGE pRam = pPGM->CTX_SUFF(pRamRangesX); pRam; pRam = pRam->CTX_SUFF(pNext))
    {
        const uint32_t cPages = pRam->cb >> PAGE_SHIFT;
        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
            PGMPAGE const *pPage = &pRam->aPages[iPage];
            if (PGM_PAGE_HAS_ANY_HANDLERS(pPage))
            {
                State.GCPhys = pRam->GCPhys + (iPage << PAGE_SHIFT);

                /*
                 * Physical first - calculate the state based on the handlers
                 *                  active on the page, then compare.
                 */
                if (PGM_PAGE_HAS_ANY_PHYSICAL_HANDLERS(pPage))
                {
                    /* the first */
                    PPGMPHYSHANDLER pPhys = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pPGM->CTX_SUFF(pTrees)->PhysHandlers, State.GCPhys);
                    if (!pPhys)
                    {
                        pPhys = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pPGM->CTX_SUFF(pTrees)->PhysHandlers, State.GCPhys, true);
                        if (    pPhys
                            &&  pPhys->Core.Key > (State.GCPhys + PAGE_SIZE - 1))
                            pPhys = NULL;
                        Assert(!pPhys || pPhys->Core.Key >= State.GCPhys);
                    }
                    if (pPhys)
                    {
                        unsigned uState = pgmHandlerPhysicalCalcState(pPhys);

                        /* more? */
                        while (pPhys->Core.KeyLast < (State.GCPhys | PAGE_OFFSET_MASK))
                        {
                            PPGMPHYSHANDLER pPhys2 = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pPGM->CTX_SUFF(pTrees)->PhysHandlers,
                                                                                              pPhys->Core.KeyLast + 1, true);
                            if (    !pPhys2
                                ||  pPhys2->Core.Key > (State.GCPhys | PAGE_OFFSET_MASK))
                                break;
                            unsigned uState2 = pgmHandlerPhysicalCalcState(pPhys2);
                            uState = RT_MAX(uState, uState2);
                            pPhys = pPhys2;
                        }

                        /* compare.*/
                        if (    PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) != uState
                            &&  PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) != PGM_PAGE_HNDL_PHYS_STATE_DISABLED)
                        {
                            AssertMsgFailed(("ram range vs phys handler flags mismatch. GCPhys=%RGp state=%d expected=%d %s\n",
                                             State.GCPhys, PGM_PAGE_GET_HNDL_PHYS_STATE(pPage), uState, pPhys->pszDesc));
                            State.cErrors++;
                        }

#ifdef VBOX_WITH_REM
# ifdef IN_RING3
                        /* validate that REM is handling it. */
                        if (    !REMR3IsPageAccessHandled(pVM, State.GCPhys)
                                /* ignore shadowed ROM for the time being. */
                            &&  PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_ROM_SHADOW)
                        {
                            AssertMsgFailed(("ram range vs phys handler REM mismatch. GCPhys=%RGp state=%d %s\n",
                                             State.GCPhys, PGM_PAGE_GET_HNDL_PHYS_STATE(pPage), pPhys->pszDesc));
                            State.cErrors++;
                        }
# endif
#endif
                    }
                    else
                    {
                        AssertMsgFailed(("ram range vs phys handler mismatch. no handler for GCPhys=%RGp\n", State.GCPhys));
                        State.cErrors++;
                    }
                }

                /*
                 * Virtual handlers.
                 */
                if (PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage))
                {
                    State.uVirtState = PGM_PAGE_GET_HNDL_VIRT_STATE(pPage);
#if 1
                    /* locate all the matching physical ranges. */
                    State.uVirtStateFound = PGM_PAGE_HNDL_VIRT_STATE_NONE;
                    RTGCPHYS GCPhysKey = State.GCPhys;
                    for (;;)
                    {
                        PPGMPHYS2VIRTHANDLER pPhys2Virt = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers,
                                                                                                        GCPhysKey, true /* above-or-equal */);
                        if (    !pPhys2Virt
                            ||  (pPhys2Virt->Core.Key & X86_PTE_PAE_PG_MASK) != State.GCPhys)
                            break;

                        /* the head */
                        GCPhysKey = pPhys2Virt->Core.KeyLast;
                        PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)((uintptr_t)pPhys2Virt + pPhys2Virt->offVirtHandler);
                        unsigned uState = pgmHandlerVirtualCalcState(pCur);
                        State.uVirtStateFound = RT_MAX(State.uVirtStateFound, uState);

                        /* any aliases */
                        while (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK)
                        {
                            pPhys2Virt = (PPGMPHYS2VIRTHANDLER)((uintptr_t)pPhys2Virt + (pPhys2Virt->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
                            pCur = (PPGMVIRTHANDLER)((uintptr_t)pPhys2Virt + pPhys2Virt->offVirtHandler);
                            uState = pgmHandlerVirtualCalcState(pCur);
                            State.uVirtStateFound = RT_MAX(State.uVirtStateFound, uState);
                        }

                        /* done? */
                        if ((GCPhysKey & X86_PTE_PAE_PG_MASK) != State.GCPhys)
                            break;
                    }
#else
                    /* very slow */
                    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, true, pgmHandlerVirtualVerifyOneByPhysAddr, &State);
#endif
                    if (State.uVirtState != State.uVirtStateFound)
                    {
                        AssertMsgFailed(("ram range vs virt handler flags mismatch. GCPhys=%RGp uVirtState=%#x uVirtStateFound=%#x\n",
                                         State.GCPhys, State.uVirtState, State.uVirtStateFound));
                        State.cErrors++;
                    }
                }
            }
        } /* foreach page in ram range. */
    } /* foreach ram range. */

    /*
     * Check that the physical addresses of the virtual handlers matches up
     * and that they are otherwise sane.
     */
    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, true, pgmHandlerVirtualVerifyOne, &State);

    /*
     * Do the reverse check for physical handlers.
     */
    /** @todo */

    return State.cErrors;
}

#endif /* VBOX_STRICT */

