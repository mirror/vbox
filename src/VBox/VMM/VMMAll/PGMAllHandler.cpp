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
#include <VBox/iom.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/rem.h>
#include <VBox/dbgf.h>
#include <VBox/rem.h>
#include "PGMInternal.h"
#include <VBox/vm.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/selm.h>


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
 * @param   pfnHandlerGC    The GC handler.
 * @param   pvUserGC        User argument to the GC handler.
 *                          This must be a GC pointer because it will be relocated!
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMDECL(int) PGMHandlerPhysicalRegisterEx(PVM pVM, PGMPHYSHANDLERTYPE enmType, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast,
                                          R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                          R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                          GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                          R3PTRTYPE(const char *) pszDesc)
{
    Log(("PGMHandlerPhysicalRegisterEx: enmType=%d GCPhys=%VGp GCPhysLast=%VGp pfnHandlerR3=%VHv pvUserR3=%VHv pfnHandlerR0=%VHv pvUserR0=%VHv pfnHandlerGC=%VGv pvUserGC=%VGv pszDesc=%s\n",
          enmType, GCPhys, GCPhysLast, pfnHandlerR3, pvUserR3, pfnHandlerR0, pvUserR0, pfnHandlerGC, pvUserGC, HCSTRING(pszDesc)));

    /*
     * Validate input.
     */
    if (GCPhys >= GCPhysLast)
    {
        AssertMsgFailed(("GCPhys >= GCPhysLast (%#x >= %#x)\n", GCPhys, GCPhysLast));
        return VERR_INVALID_PARAMETER;
    }
    switch (enmType)
    {
        case PGMPHYSHANDLERTYPE_MMIO:
        case PGMPHYSHANDLERTYPE_PHYSICAL:
        case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE:
        case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:
            break;
        default:
            AssertMsgFailed(("Invalid input enmType=%d!\n", enmType));
            return VERR_INVALID_PARAMETER;
    }
    if (    (RTGCUINTPTR)pvUserGC >= 0x10000
        &&  MMHyperHC2GC(pVM, MMHyperGC2HC(pVM, pvUserGC)) != pvUserGC)
    {
        AssertMsgFailed(("Not GC pointer! pvUserGC=%VGv\n", pvUserGC));
        return VERR_INVALID_PARAMETER;
    }
    AssertReturn(pfnHandlerR3 || pfnHandlerR0 || pfnHandlerGC, VERR_INVALID_PARAMETER);

    /*
     * We require the range to be within registered ram.
     * There is no apparent need to support ranges which cover more than one ram range.
     */
    PPGMRAMRANGE    pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
    while (pRam && GCPhys > pRam->GCPhysLast)
        pRam = CTXALLSUFF(pRam->pNext);
    if (    !pRam
        ||  GCPhysLast < pRam->GCPhys
        ||  GCPhys > pRam->GCPhysLast)
    {
#ifdef IN_RING3
        /*
         * If this is an MMIO registration, we'll just add a range for it.
         */
        if (    enmType == PGMPHYSHANDLERTYPE_MMIO
            &&  (   !pRam
                 || GCPhysLast < pRam->GCPhys)
           )
        {
            size_t cb = GCPhysLast - GCPhys + 1;
            Assert(cb == RT_ALIGN_Z(cb, PAGE_SIZE));
            int rc = PGMR3PhysRegister(pVM, NULL, GCPhys, cb, MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO, NULL, pszDesc);
            if (VBOX_FAILURE(rc))
                return rc;

            /* search again. */
            pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
            while (pRam && GCPhys > pRam->GCPhysLast)
                pRam = CTXALLSUFF(pRam->pNext);
        }

        if (    !pRam
            ||  GCPhysLast < pRam->GCPhys
            ||  GCPhys > pRam->GCPhysLast)
#endif /* IN_RING3 */
        {
#ifdef IN_RING3
            DBGFR3Info(pVM, "phys", NULL, NULL);
#endif
            AssertMsgFailed(("No RAM range for %VGp-%VGp\n", GCPhys, GCPhysLast));
            return VERR_PGM_HANDLER_PHYSICAL_NO_RAM_RANGE;
        }
    }

    /*
     * Allocate and initialize the new entry.
     */
    PPGMPHYSHANDLER pNew;
    int rc = MMHyperAlloc(pVM, sizeof(*pNew), 0, MM_TAG_PGM_HANDLERS, (void **)&pNew);
    if (VBOX_FAILURE(rc))
        return rc;

    pNew->Core.Key      = GCPhys;
    pNew->Core.KeyLast  = GCPhysLast;
    pNew->enmType       = enmType;
    pNew->cPages        = (GCPhysLast - (GCPhys & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
    pNew->pfnHandlerR3  = pfnHandlerR3;
    pNew->pvUserR3      = pvUserR3;
    pNew->pfnHandlerR0  = pfnHandlerR0;
    pNew->pvUserR0      = pvUserR0;
    pNew->pfnHandlerGC  = pfnHandlerGC;
    pNew->pvUserGC      = pvUserGC;
    pNew->pszDesc       = pszDesc;

    pgmLock(pVM);

    /*
     * Try insert into list.
     */
    if (RTAvlroGCPhysInsert(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, &pNew->Core))
    {
        rc = pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(pVM, pNew, pRam);
        if (rc == VINF_PGM_GCPHYS_ALIASED)
        {
            pVM->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        }
        pVM->pgm.s.fPhysCacheFlushPending = true;
#ifndef IN_RING3
        REMNotifyHandlerPhysicalRegister(pVM, enmType, GCPhys, GCPhysLast - GCPhys + 1, !!pfnHandlerR3);
#else
        REMR3NotifyHandlerPhysicalRegister(pVM, enmType, GCPhys, GCPhysLast - GCPhys + 1, !!pfnHandlerR3);
#endif
        pgmUnlock(pVM);
        if (rc != VINF_SUCCESS)
            Log(("PGMHandlerPhysicalRegisterEx: returns %Vrc (%VGp-%VGp)\n", rc, GCPhys, GCPhysLast));
        return rc;
    }
    pgmUnlock(pVM);

#if defined(IN_RING3) && defined(VBOX_STRICT)
    DBGFR3Info(pVM, "handlers", "phys nostats", NULL);
#endif
    AssertMsgFailed(("Conflict! GCPhys=%VGp GCPhysLast=%VGp pszDesc=%s\n", GCPhys, GCPhysLast, pszDesc));
    MMHyperFree(pVM, pNew);
    return VERR_PGM_HANDLER_PHYSICAL_CONFLICT;
}


/**
 * Sets ram range flags and attempts updating shadow PTs.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS when shadow PTs was successfully updated.
 * @retval  VINF_PGM_GCPHYS_ALIASED when the shadow PTs could be updated because
 *          the guest page aliased or/and mapped by multiple PTs.
 * @param   pVM     The VM handle.
 * @param   pCur    The physical handler.
 */
static int pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(PVM pVM, PPGMPHYSHANDLER pCur, PPGMRAMRANGE pRam)
{
    /*
     * Iterate the guest ram pages updating the flags and flushing PT entries
     * mapping the page.
     */
    bool            fFlushTLBs = false;
#if defined(PGMPOOL_WITH_GCPHYS_TRACKING) || defined(PGMPOOL_WITH_CACHE)
    int             rc = VINF_SUCCESS;
#else
    const int       rc = VINF_PGM_GCPHYS_ALIASED;
#endif
    const unsigned  fFlags = pgmHandlerPhysicalCalcFlags(pCur); Assert(!(fFlags & X86_PTE_PAE_PG_MASK));
    RTUINT          cPages = pCur->cPages;
    RTUINT          i = (pCur->Core.Key - pRam->GCPhys) >> PAGE_SHIFT;
    for (;;)
    {
        /* Physical chunk in dynamically allocated range not present? */
        if (RT_UNLIKELY(!PGM_PAGE_GET_HCPHYS(&pRam->aPages[i])))
        {
            RTGCPHYS GCPhys = pRam->GCPhys + (i << PAGE_SHIFT);
#ifdef IN_RING3
            int rc2 = pgmr3PhysGrowRange(pVM, GCPhys);
#else
            int rc2 = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhys);
#endif
            if (rc2 != VINF_SUCCESS)
                return rc2;
        }

        if ((pRam->aPages[i].HCPhys & fFlags) != fFlags) /** @todo PAGE FLAGS */
        {
            pRam->aPages[i].HCPhys |= fFlags; /** @todo PAGE FLAGS */

            Assert(PGM_PAGE_GET_HCPHYS(&pRam->aPages[i]));

#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
            /* This code also makes ASSUMPTIONS about the cRefs and stuff. */
            Assert(MM_RAM_FLAGS_IDX_SHIFT < MM_RAM_FLAGS_CREFS_SHIFT);
            const uint16_t u16 = pRam->aPages[i].HCPhys >> MM_RAM_FLAGS_IDX_SHIFT; /** @todo PAGE FLAGS */
            if (u16)
            {
                if ((u16 >> (MM_RAM_FLAGS_CREFS_SHIFT - MM_RAM_FLAGS_IDX_SHIFT)) != MM_RAM_FLAGS_CREFS_PHYSEXT)
                    pgmPoolTrackFlushGCPhysPT(pVM,
                                              &pRam->aPages[i],
                                              u16 & MM_RAM_FLAGS_IDX_MASK,
                                              u16 >> (MM_RAM_FLAGS_CREFS_SHIFT - MM_RAM_FLAGS_IDX_SHIFT));
                else if (u16 != ((MM_RAM_FLAGS_CREFS_PHYSEXT << (MM_RAM_FLAGS_CREFS_SHIFT - MM_RAM_FLAGS_IDX_SHIFT)) | MM_RAM_FLAGS_IDX_OVERFLOWED))
                    pgmPoolTrackFlushGCPhysPTs(pVM, &pRam->aPages[i], u16 & MM_RAM_FLAGS_IDX_MASK);
                else
                    rc = pgmPoolTrackFlushGCPhysPTsSlow(pVM, &pRam->aPages[i]);
                fFlushTLBs = true;
            }
#elif defined(PGMPOOL_WITH_CACHE)
            rc = pgmPoolTrackFlushGCPhysPTsSlow(pVM, &pRam->aPages[i]);
            fFlushTLBs = true;
#endif
        }

        /* next */
        if (--cPages == 0)
            break;
        i++;
    }

    if (fFlushTLBs && rc == VINF_SUCCESS)
    {
        PGM_INVL_GUEST_TLBS();
        Log(("pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs: flushing guest TLBs\n"));
    }
    else
        Log(("pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs: doesn't flush guest TLBs. rc=%Vrc\n", rc));
    return rc;
}


/**
 * Register a physical page access handler.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle.
 * @param   GCPhys      Start physical address.
 */
PGMDECL(int)  PGMHandlerPhysicalDeregister(PVM pVM, RTGCPHYS GCPhys)
{
    /*
     * Find the handler.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRemove(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys);
    if (pCur)
    {
        LogFlow(("PGMHandlerPhysicalDeregister: Removing Range %#VGp-%#VGp %s\n",
                 pCur->Core.Key, pCur->Core.KeyLast, HCSTRING(pCur->pszDesc)));

        /*
         * Clear the page bits and notify the REM about this change.
         */
        pgmHandlerPhysicalResetRamFlags(pVM, pCur);
        pgmHandlerPhysicalDeregisterNotifyREM(pVM, pCur);
        pgmUnlock(pVM);
        MMHyperFree(pVM, pCur);
        return VINF_SUCCESS;
    }
    pgmUnlock(pVM);

    AssertMsgFailed(("Didn't find range starting at %VGp\n", GCPhys));
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
     */
    if (    (pCur->Core.Key & PAGE_OFFSET_MASK)
        ||  ((pCur->Core.KeyLast + 1) & PAGE_OFFSET_MASK))
    {
        if (GCPhysStart & PAGE_OFFSET_MASK)
        {
            if (pgmRamTestFlags(&pVM->pgm.s, GCPhysStart, MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_PHYSICAL_HANDLER | MM_RAM_FLAGS_PHYSICAL_ALL | MM_RAM_FLAGS_PHYSICAL_WRITE | MM_RAM_FLAGS_PHYSICAL_TEMP_OFF))
            {
                RTGCPHYS GCPhys = (GCPhysStart + (PAGE_SIZE - 1)) & X86_PTE_PAE_PG_MASK;
                if (    GCPhys > GCPhysLast
                    ||  GCPhys < GCPhysStart)
                    return;
                GCPhysStart = GCPhys;
            }
            else
                GCPhysStart = GCPhysStart & X86_PTE_PAE_PG_MASK;
        }
        if (GCPhysLast & PAGE_OFFSET_MASK)
        {
            if (pgmRamTestFlags(&pVM->pgm.s, GCPhysLast, MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_PHYSICAL_HANDLER | MM_RAM_FLAGS_PHYSICAL_ALL | MM_RAM_FLAGS_PHYSICAL_WRITE | MM_RAM_FLAGS_PHYSICAL_TEMP_OFF))
            {
                RTGCPHYS GCPhys = (GCPhysStart & X86_PTE_PAE_PG_MASK) - 1;
                if (    GCPhys < GCPhysStart
                    ||  GCPhys > GCPhysLast)
                    return;
                GCPhysLast = GCPhys;
            }
            else
                GCPhysLast += PAGE_SIZE - 1 - (GCPhysLast & PAGE_OFFSET_MASK);
        }
    }

    /*
     * Tell REM.
     */
    const bool fRestoreAsRAM = pCur->pfnHandlerR3
                            && pCur->enmType != PGMPHYSHANDLERTYPE_MMIO; /** @todo this isn't entirely correct. */
#ifndef IN_RING3
    REMNotifyHandlerPhysicalDeregister(pVM, pCur->enmType, GCPhysStart, GCPhysLast - GCPhysStart + 1, !!pCur->pfnHandlerR3, fRestoreAsRAM);
#else
    REMR3NotifyHandlerPhysicalDeregister(pVM, pCur->enmType, GCPhysStart, GCPhysLast - GCPhysStart + 1, !!pCur->pfnHandlerR3, fRestoreAsRAM);
#endif
}


/**
 * Resets ram range flags.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS when shadow PTs was successfully updated.
 * @param   pVM     The VM handle.
 * @param   pCur    The physical handler.
 *
 * @remark  We don't start messing with the shadow page tables, as we've already got code
 *          in Trap0e which deals with out of sync handler flags (originally conceived for
 *          global pages).
 */
static void pgmHandlerPhysicalResetRamFlags(PVM pVM, PPGMPHYSHANDLER pCur)
{
    /*
     * Iterate the guest ram pages updating the flags and flushing PT entries
     * mapping the page.
     */
    RTUINT          cPages = pCur->cPages;
    RTGCPHYS        GCPhys = pCur->Core.Key;
    PPGMRAMRANGE    pRamHint = NULL;
    PPGM            pPGM = &pVM->pgm.s;
    for (;;)
    {
        pgmRamFlagsClearByGCPhysWithHint(pPGM, GCPhys,
                                         MM_RAM_FLAGS_PHYSICAL_HANDLER | MM_RAM_FLAGS_PHYSICAL_WRITE | MM_RAM_FLAGS_PHYSICAL_ALL,
                                         &pRamHint);
        /* next */
        if (--cPages == 0)
            break;
        GCPhys += PAGE_SIZE;
    }

    /*
     * Check for partial start page.
     */
    if (pCur->Core.Key & PAGE_OFFSET_MASK)
    {
        RTGCPHYS GCPhys = pCur->Core.Key - 1;
        for (;;)
        {
            PPGMPHYSHANDLER pBelow = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys, false);
            if (    !pBelow
                ||  (pBelow->Core.KeyLast >> PAGE_SHIFT) != (pCur->Core.Key >> PAGE_SHIFT))
                break;
            pgmRamFlagsSetByGCPhysWithHint(pPGM, GCPhys, pgmHandlerPhysicalCalcFlags(pCur), &pRamHint);

            /* next? */
            if (    (pBelow->Core.Key >> PAGE_SHIFT) != (pCur->Core.Key >> PAGE_SHIFT)
                ||  !(pBelow->Core.Key & PAGE_OFFSET_MASK))
                break;
            GCPhys = pBelow->Core.Key - 1;
        }
    }

    /*
     * Check for partial end page.
     */
    if ((pCur->Core.KeyLast & PAGE_OFFSET_MASK) != PAGE_SIZE - 1)
    {
        RTGCPHYS GCPhys = pCur->Core.KeyLast + 1;
        for (;;)
        {
            PPGMPHYSHANDLER pAbove = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys, true);
            if (    !pAbove
                ||  (pAbove->Core.Key >> PAGE_SHIFT) != (pCur->Core.KeyLast >> PAGE_SHIFT))
                break;
            pgmRamFlagsSetByGCPhysWithHint(pPGM, GCPhys, pgmHandlerPhysicalCalcFlags(pCur), &pRamHint);

            /* next? */
            if (    (pAbove->Core.KeyLast >> PAGE_SHIFT) != (pCur->Core.KeyLast >> PAGE_SHIFT)
                ||  (pAbove->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_SIZE - 1)
                break;
            GCPhys = pAbove->Core.KeyLast + 1;
        }
    }
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
PGMDECL(int) PGMHandlerPhysicalModify(PVM pVM, RTGCPHYS GCPhysCurrent, RTGCPHYS GCPhys, RTGCPHYS GCPhysLast)
{
    /*
     * Remove it.
     */
    int rc;
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRemove(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhysCurrent);
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
            PPGMRAMRANGE    pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
            while (pRam && GCPhys > pRam->GCPhysLast)
                pRam = CTXALLSUFF(pRam->pNext);
            if (    pRam
                &&  GCPhys <= pRam->GCPhysLast
                &&  GCPhysLast >= pRam->GCPhys)
            {
                pCur->Core.Key      = GCPhys;
                pCur->Core.KeyLast  = GCPhysLast;
                pCur->cPages        = (GCPhysLast - (GCPhys & X86_PTE_PAE_PG_MASK) + 1) >> PAGE_SHIFT;

                if (RTAvlroGCPhysInsert(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, &pCur->Core))
                {
                    /*
                     * Set ram flags, flush shadow PT entries and finally tell REM about this.
                     */
                    rc = pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(pVM, pCur, pRam);
                    if (rc == VINF_PGM_GCPHYS_ALIASED)
                    {
                        pVM->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
                        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                    }
                    pVM->pgm.s.fPhysCacheFlushPending = true;

#ifndef IN_RING3
                    REMNotifyHandlerPhysicalModify(pVM, pCur->enmType, GCPhysCurrent, GCPhys,
                                                   pCur->Core.KeyLast - GCPhys + 1, !!pCur->pfnHandlerR3, fRestoreAsRAM);
#else
                    REMR3NotifyHandlerPhysicalModify(pVM, pCur->enmType, GCPhysCurrent, GCPhys,
                                                     pCur->Core.KeyLast - GCPhys + 1, !!pCur->pfnHandlerR3, fRestoreAsRAM);
#endif
                    pgmUnlock(pVM);
                    Log(("PGMHandlerPhysicalModify: GCPhysCurrent=%VGp -> GCPhys=%VGp GCPhysLast=%VGp\n",
                         GCPhysCurrent, GCPhys, GCPhysLast));
                    return VINF_SUCCESS;
                }

                AssertMsgFailed(("Conflict! GCPhys=%VGp GCPhysLast=%VGp\n", GCPhys, GCPhysLast));
                rc = VERR_PGM_HANDLER_PHYSICAL_CONFLICT;
            }
            else
            {
                AssertMsgFailed(("No RAM range for %VGp-%VGp\n", GCPhys, GCPhysLast));
                rc = VERR_PGM_HANDLER_PHYSICAL_NO_RAM_RANGE;
            }
        }
        else
        {
            AssertMsgFailed(("Invalid range %VGp-%VGp\n", GCPhys, GCPhysLast));
            rc = VERR_INVALID_PARAMETER;
        }

        /*
         * Invalid new location, free it.
         * We've only gotta notify REM and free the memory.
         */
        pgmHandlerPhysicalDeregisterNotifyREM(pVM, pCur);
        MMHyperFree(pVM, pCur);
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %VGp\n", GCPhysCurrent));
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
 * @param   pfnHandlerGC    The GC handler.
 * @param   pvUserGC        User argument to the GC handler.
 *                          This must be a GC pointer because it will be relocated!
 * @param   pszDesc         Pointer to description string. This must not be freed.
 */
PGMDECL(int) PGMHandlerPhysicalChangeCallbacks(PVM pVM, RTGCPHYS GCPhys,
                                               R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                               R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                               GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                               R3PTRTYPE(const char *) pszDesc)
{
    /*
     * Get the handler.
     */
    int rc = VINF_SUCCESS;
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys);
    if (pCur)
    {
        /*
         * Change callbacks.
         */
        pCur->pfnHandlerR3  = pfnHandlerR3;
        pCur->pvUserR3      = pvUserR3;
        pCur->pfnHandlerR0  = pfnHandlerR0;
        pCur->pvUserR0      = pvUserR0;
        pCur->pfnHandlerGC  = pfnHandlerGC;
        pCur->pvUserGC      = pvUserGC;
        pCur->pszDesc       = pszDesc;
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %VGp\n", GCPhys));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * Splitts a physical access handler in two.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   GCPhys          Start physical address of the handler.
 * @param   GCPhysSplit     The split address.
 */
PGMDECL(int) PGMHandlerPhysicalSplit(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS GCPhysSplit)
{
    AssertReturn(GCPhys < GCPhysSplit, VERR_INVALID_PARAMETER);

    /*
     * Do the allocation without owning the lock.
     */
    PPGMPHYSHANDLER pNew;
    int rc = MMHyperAlloc(pVM, sizeof(*pNew), 0, MM_TAG_PGM_HANDLERS, (void **)&pNew);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Get the handler.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys);
    if (pCur)
    {
        if (GCPhysSplit <= pCur->Core.KeyLast)
        {
            /*
             * Create new handler node for the 2nd half.
             */
            *pNew = *pCur;
            pNew->Core.Key      = GCPhysSplit;
            pNew->cPages        = (pNew->Core.KeyLast - (pNew->Core.Key & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;

            pCur->Core.KeyLast  = GCPhysSplit - 1;
            pCur->cPages        = (pCur->Core.KeyLast - (pCur->Core.Key & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;

            if (RTAvlroGCPhysInsert(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, &pNew->Core))
            {
                LogFlow(("PGMHandlerPhysicalSplit: %VGp-%VGp and %VGp-%VGp\n",
                         pCur->Core.Key, pCur->Core.KeyLast, pNew->Core.Key, pNew->Core.KeyLast));
                pgmUnlock(pVM);
                return VINF_SUCCESS;
            }
            AssertMsgFailed(("whu?\n"));
            rc = VERR_INTERNAL_ERROR;
        }
        else
        {
            AssertMsgFailed(("outside range: %VGp-%VGp split %VGp\n", pCur->Core.Key, pCur->Core.KeyLast, GCPhysSplit));
            rc = VERR_INVALID_PARAMETER;
        }
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %VGp\n", GCPhys));
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
PGMDECL(int) PGMHandlerPhysicalJoin(PVM pVM, RTGCPHYS GCPhys1, RTGCPHYS GCPhys2)
{
    /*
     * Get the handlers.
     */
    int rc;
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur1 = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys1);
    if (pCur1)
    {
        PPGMPHYSHANDLER pCur2 = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys2);
        if (pCur2)
        {
            /*
             * Make sure that they are adjacent, and that they've got the same callbacks.
             */
            if (pCur1->Core.KeyLast + 1 == pCur2->Core.Key)
            {
                if (    pCur1->pfnHandlerGC == pCur2->pfnHandlerGC
                    &&  pCur1->pfnHandlerR0 == pCur2->pfnHandlerR0
                    &&  pCur1->pfnHandlerR3 == pCur2->pfnHandlerR3)
                {
                    PPGMPHYSHANDLER pCur3 = (PPGMPHYSHANDLER)RTAvlroGCPhysRemove(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys2);
                    if (pCur3 == pCur2)
                    {
                        pCur1->Core.KeyLast  = pCur2->Core.KeyLast;
                        pCur1->cPages        = (pCur1->Core.KeyLast - (pCur1->Core.Key & X86_PTE_PAE_PG_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
                        LogFlow(("PGMHandlerPhysicalJoin: %VGp-%VGp %VGp-%VGp\n",
                                 pCur1->Core.Key, pCur1->Core.KeyLast, pCur2->Core.Key, pCur2->Core.KeyLast));
                        pgmUnlock(pVM);
                        MMHyperFree(pVM, pCur2);
                        return VINF_SUCCESS;
                    }
                    Assert(pCur3 == pCur2);
                    rc = VERR_INTERNAL_ERROR;
                }
                else
                {
                    AssertMsgFailed(("mismatching handlers\n"));
                    rc = VERR_ACCESS_DENIED;
                }
            }
            else
            {
                AssertMsgFailed(("not adjacent: %VGp-%VGp %VGp-%VGp\n",
                                 pCur1->Core.Key, pCur1->Core.KeyLast, pCur2->Core.Key, pCur2->Core.KeyLast));
                rc = VERR_INVALID_PARAMETER;
            }
        }
        else
        {
            AssertMsgFailed(("Didn't find range starting at %VGp\n", GCPhys2));
            rc = VERR_PGM_HANDLER_NOT_FOUND;
        }
    }
    else
    {
        AssertMsgFailed(("Didn't find range starting at %VGp\n", GCPhys1));
        rc = VERR_PGM_HANDLER_NOT_FOUND;
    }
    pgmUnlock(pVM);
    return rc;

}


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
PGMDECL(int)  PGMHandlerPhysicalReset(PVM pVM, RTGCPHYS GCPhys)
{
    /*
     * Find the handler.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysHandlers, GCPhys);
    if (pCur)
    {
        /*
         * Validate type.
         */
        switch (pCur->enmType)
        {
            case PGMPHYSHANDLERTYPE_PHYSICAL_WRITE:
            case PGMPHYSHANDLERTYPE_PHYSICAL_ALL:
            {
                /*
                 * Set the flags and flush shadow PT entries.
                 */
                STAM_COUNTER_INC(&pVM->pgm.s.StatHandlePhysicalReset);
                PPGMRAMRANGE    pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
                while (pRam && GCPhys > pRam->GCPhysLast)
                    pRam = CTXALLSUFF(pRam->pNext);
                int rc = pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs(pVM, pCur, pRam);
                if (rc == VINF_PGM_GCPHYS_ALIASED)
                {
                    pVM->pgm.s.fSyncFlags |= PGM_SYNC_CLEAR_PGM_POOL;
                    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                }
                pVM->pgm.s.fPhysCacheFlushPending = true;
                pgmUnlock(pVM);
                return VINF_SUCCESS;
            }

            /*
             * Invalid.
             */
            case PGMPHYSHANDLERTYPE_PHYSICAL:
            case PGMPHYSHANDLERTYPE_MMIO:
                AssertMsgFailed(("Can't reset type %d!\n",  pCur->enmType));
                pgmUnlock(pVM);
                return VERR_INTERNAL_ERROR;

            default:
                AssertMsgFailed(("Invalid type %d! Corruption!\n",  pCur->enmType));
                pgmUnlock(pVM);
                return VERR_INTERNAL_ERROR;
        }
    }
    pgmUnlock(pVM);
    AssertMsgFailed(("Didn't find MMIO Range starting at %#x\n", GCPhys));
    return VERR_PGM_HANDLER_NOT_FOUND;
}


/**
 * Search for virtual handler with matching physical address
 *
 * @returns VBox status code
 * @param   pVM         The VM handle.
 * @param   GCPhys      GC physical address to search for.
 * @param   ppVirt      Where to store the pointer to the virtual handler structure.
 * @param   piPage      Where to store the pointer to the index of the cached physical page.
 */
int pgmHandlerVirtualFindByPhysAddr(PVM pVM, RTGCPHYS GCPhys, PPGMVIRTHANDLER *ppVirt, unsigned *piPage)
{
    STAM_PROFILE_START(CTXSUFF(&pVM->pgm.s.StatVirtHandleSearchByPhys), a);
    Assert(ppVirt);

    PPGMPHYS2VIRTHANDLER pCur;
    pCur = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysRangeGet(&CTXSUFF(pVM->pgm.s.pTrees)->PhysToVirtHandlers, GCPhys);
    if (pCur)
    {
        /* found a match! */
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
        AssertRelease(pCur->offNextAlias & PGMPHYS2VIRTHANDLER_IS_HEAD);
#endif
        *ppVirt = (PPGMVIRTHANDLER)((uintptr_t)pCur + pCur->offVirtHandler);
        *piPage = pCur - &(*ppVirt)->aPhysToVirt[0];

        LogFlow(("PHYS2VIRT: found match for %VGp -> %VGv *piPage=%#x\n",
                 GCPhys, (*ppVirt)->GCPtr, *piPage));
        STAM_PROFILE_STOP(CTXSUFF(&pVM->pgm.s.StatVirtHandleSearchByPhys), a);
        return VINF_SUCCESS;
    }

    *ppVirt = NULL;
    STAM_PROFILE_STOP(CTXSUFF(&pVM->pgm.s.StatVirtHandleSearchByPhys), a);
    return VERR_PGM_HANDLER_NOT_FOUND;
}


/**
 * Deal with aliases in phys2virt.
 *
 * @param   pVM             The VM handle.
 * @param   pPhys2Virt      The node we failed insert.
 */
static void pgmHandlerVirtualInsertAliased(PVM pVM, PPGMPHYS2VIRTHANDLER pPhys2Virt)
{
    /*
     * First find the node which is conflicting with us.
     */
    /** @todo Deal with partial overlapping. (Unlikly situation, so I'm too lazy to do anything about it now.) */
    PPGMPHYS2VIRTHANDLER pHead = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key);
    if (!pHead)
    {
        /** @todo do something clever here... */
#ifdef IN_RING3
        LogRel(("pgmHandlerVirtualInsertAliased: %VGp-%VGp\n", pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast));
#endif
        pPhys2Virt->offNextAlias = 0;
        return;
    }
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
    AssertReleaseMsg(pHead != pPhys2Virt, ("%VGp-%VGp offVirtHandler=%#RX32\n",
                                           pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offVirtHandler));
#endif

    /** @todo check if the current head node covers the ground we do. This is highly unlikely
     * and I'm too lazy to implement this now as it will require sorting the list and stuff like that. */

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
    Log(("pgmHandlerVirtualInsertAliased: %VGp-%VGp offNextAlias=%#RX32\n", pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias));
}


/**
 * Resets one virtual handler range.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMVIRTHANDLER.
 * @param   pvUser  The VM handle.
 */
DECLCALLBACK(int) pgmHandlerVirtualResetOne(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)pNode;
    PVM             pVM = (PVM)pvUser;

    /*
     * Calc flags.
     */
    unsigned        fFlags;
    switch (pCur->enmType)
    {
        case PGMVIRTHANDLERTYPE_EIP:
        case PGMVIRTHANDLERTYPE_NORMAL: fFlags = MM_RAM_FLAGS_VIRTUAL_HANDLER; break;
        case PGMVIRTHANDLERTYPE_WRITE:  fFlags = MM_RAM_FLAGS_VIRTUAL_HANDLER | MM_RAM_FLAGS_VIRTUAL_WRITE; break;
        case PGMVIRTHANDLERTYPE_ALL:    fFlags = MM_RAM_FLAGS_VIRTUAL_HANDLER | MM_RAM_FLAGS_VIRTUAL_ALL; break;
        /* hypervisor handlers need no flags and wouldn't have nowhere to put them in any case. */
        case PGMVIRTHANDLERTYPE_HYPERVISOR:
            return 0;
        default:
            AssertMsgFailed(("Invalid type %d\n", pCur->enmType));
            return 0;
    }

    /*
     * Iterate the pages and apply the flags.
     */
    PPGMRAMRANGE    pRamHint = NULL;
    RTGCUINTPTR     offPage  = ((RTGCUINTPTR)pCur->GCPtr & PAGE_OFFSET_MASK);
    RTGCUINTPTR     cbLeft   = pCur->cb;
    for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
    {
        PPGMPHYS2VIRTHANDLER pPhys2Virt = &pCur->aPhysToVirt[iPage];
        if (pPhys2Virt->Core.Key != NIL_RTGCPHYS)
        {
            /* Update the flags. */
            int rc = pgmRamFlagsSetByGCPhysWithHint(&pVM->pgm.s, pPhys2Virt->Core.Key, fFlags, &pRamHint);
            AssertRC(rc);

            /* Need to insert the page in the Phys2Virt lookup tree? */
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
                if (!RTAvlroGCPhysInsert(&pVM->pgm.s.CTXSUFF(pTrees)->PhysToVirtHandlers, &pPhys2Virt->Core))
                    pgmHandlerVirtualInsertAliased(pVM, pPhys2Virt);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                else
                    AssertReleaseMsg(RTAvlroGCPhysGet(&pVM->pgm.s.CTXSUFF(pTrees)->PhysToVirtHandlers, pPhys2Virt->Core.Key) == &pPhys2Virt->Core,
                                     ("%VGp-%VGp offNextAlias=%#RX32\n",
                                      pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias));
#endif
                Log2(("PHYS2VIRT: Insert physical range %VGp-%VGp offNextAlias=%#RX32 %s\n",
                      pPhys2Virt->Core.Key, pPhys2Virt->Core.KeyLast, pPhys2Virt->offNextAlias, R3STRING(pCur->pszDesc)));
            }
        }
        cbLeft -= PAGE_SIZE - offPage;
        offPage = 0;
    }

    return 0;
}

