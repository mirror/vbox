/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
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
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/stam.h>
#include <VBox/rem.h>
#include <VBox/csam.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <VBox/log.h>
#include <iprt/thread.h>
#include <iprt/string.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/*static - shut up warning */
DECLCALLBACK(int) pgmR3PhysRomWriteHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);



/*
 * PGMR3PhysReadU8-64
 * PGMR3PhysWriteU8-64
 */
#define PGMPHYSFN_READNAME  PGMR3PhysReadU8
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU8
#define PGMPHYS_DATASIZE    1
#define PGMPHYS_DATATYPE    uint8_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadU16
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU16
#define PGMPHYS_DATASIZE    2
#define PGMPHYS_DATATYPE    uint16_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadU32
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU32
#define PGMPHYS_DATASIZE    4
#define PGMPHYS_DATATYPE    uint32_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadU64
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteU64
#define PGMPHYS_DATASIZE    8
#define PGMPHYS_DATATYPE    uint64_t
#include "PGMPhys.h"



/**
 * Links a new RAM range into the list.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pNew        Pointer to the new list entry.
 * @param   pPrev       Pointer to the previous list entry. If NULL, insert as head.
 */
static void pgmR3PhysLinkRamRange(PVM pVM, PPGMRAMRANGE pNew, PPGMRAMRANGE pPrev)
{
    pgmLock(pVM);

    PPGMRAMRANGE pRam = pPrev ? pPrev->pNextR3 : pVM->pgm.s.pRamRangesR3;
    pNew->pNextR3 = pRam;
    pNew->pNextR0 = pRam ? MMHyperCCToR0(pVM, pRam) : NIL_RTR0PTR;
    pNew->pNextGC = pRam ? MMHyperCCToGC(pVM, pRam) : NIL_RTGCPTR;

    if (pPrev)
    {
        pPrev->pNextR3 = pNew;
        pPrev->pNextR0 = MMHyperCCToR0(pVM, pNew);
        pPrev->pNextGC = MMHyperCCToGC(pVM, pNew);
    }
    else
    {
        pVM->pgm.s.pRamRangesR3 = pNew;
        pVM->pgm.s.pRamRangesR0 = MMHyperCCToR0(pVM, pNew);
        pVM->pgm.s.pRamRangesGC = MMHyperCCToGC(pVM, pNew);
    }

    pgmUnlock(pVM);
}


/**
 * Unlink an existing RAM range from the list.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pRam        Pointer to the new list entry.
 * @param   pPrev       Pointer to the previous list entry. If NULL, insert as head.
 */
static void pgmR3PhysUnlinkRamRange2(PVM pVM, PPGMRAMRANGE pRam, PPGMRAMRANGE pPrev)
{
    Assert(pPrev ? pPrev->pNextR3 == pRam : pVM->pgm.s.pRamRangesR3 == pRam);

    pgmLock(pVM);

    PPGMRAMRANGE pNext = pRam->pNextR3;
    if (pPrev)
    {
        pPrev->pNextR3 = pNext;
        pPrev->pNextR0 = pNext ? MMHyperCCToR0(pVM, pNext) : NIL_RTR0PTR;
        pPrev->pNextGC = pNext ? MMHyperCCToGC(pVM, pNext) : NIL_RTGCPTR;
    }
    else
    {
        Assert(pVM->pgm.s.pRamRangesR3 == pRam);
        pVM->pgm.s.pRamRangesR3 = pNext;
        pVM->pgm.s.pRamRangesR0 = pNext ? MMHyperCCToR0(pVM, pNext) : NIL_RTR0PTR;
        pVM->pgm.s.pRamRangesGC = pNext ? MMHyperCCToGC(pVM, pNext) : NIL_RTGCPTR;
    }

    pgmUnlock(pVM);
}


/**
 * Unlink an existing RAM range from the list.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pRam        Pointer to the new list entry.
 */
static void pgmR3PhysUnlinkRamRange(PVM pVM, PPGMRAMRANGE pRam)
{
    /* find prev. */
    PPGMRAMRANGE pPrev = NULL;
    PPGMRAMRANGE pCur = pVM->pgm.s.pRamRangesR3;
    while (pCur != pRam)
    {
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }
    AssertFatal(pCur);

    pgmR3PhysUnlinkRamRange2(pVM, pRam, pPrev);
}



/**
 * Sets up a range RAM.
 *
 * This will check for conflicting registrations, make a resource
 * reservation for the memory (with GMM), and setup the per-page
 * tracking structures (PGMPAGE).
 *
 * @returns VBox stutus code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPhys          The physical address of the RAM.
 * @param   cb              The size of the RAM.
 * @param   pszDesc         The description - not copied, so, don't free or change it.
 */
PGMR3DECL(int) PGMR3PhysRegisterRam(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, const char *pszDesc)
{
   /*
     * Validate input.
     */
    Log(("PGMR3PhysRegisterRam: GCPhys=%RGp cb=%RGp pszDesc=%s\n", GCPhys, cb, pszDesc));
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb, VERR_INVALID_PARAMETER);
    AssertReturn(cb > 0, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertMsgReturn(GCPhysLast > GCPhys, ("The range wraps! GCPhys=%RGp cb=%RGp\n", GCPhys, cb), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);

    /*
     * Find range location and check for conflicts.
     * (We don't lock here because the locking by EMT is only required on update.)
     */
    PPGMRAMRANGE    pPrev = NULL;
    PPGMRAMRANGE    pRam = pVM->pgm.s.pRamRangesR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhysLast >= pRam->GCPhys
            &&  GCPhys     <= pRam->GCPhysLast)
            AssertLogRelMsgFailedReturn(("%RGp-%RGp (%s) conflicts with existing %RGp-%RGp (%s)\n",
                                         GCPhys, GCPhysLast, pszDesc,
                                         pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                        VERR_PGM_RAM_CONFLICT);

        /* next */
        pPrev = pRam;
        pRam = pRam->pNextR3;
    }

    /*
     * Register it with GMM (the API bitches).
     */
    const RTGCPHYS cPages = cb >> PAGE_SHIFT;
    int rc = MMR3IncreaseBaseReservation(pVM, cPages);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate RAM range.
     */
    const size_t cbRamRange = RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]);
    PPGMRAMRANGE pNew;
    rc = MMR3HyperAllocOnceNoRel(pVM, cbRamRange, 0, MM_TAG_PGM_PHYS, (void **)&pNew);
    AssertLogRelMsgRCReturn(rc, ("cbRamRange=%zu\n", cbRamRange), rc);

    /*
     * Initialize the range.
     */
    pNew->GCPhys        = GCPhys;
    pNew->GCPhysLast    = GCPhysLast;
    pNew->pszDesc       = pszDesc;
    pNew->cb            = cb;
    pNew->fFlags        = 0;

    pNew->pvHC          = NULL;
    pNew->pavHCChunkHC  = NULL;
    pNew->pavHCChunkGC  = 0;

#ifndef VBOX_WITH_NEW_PHYS_CODE
    /* Allocate memory for chunk to HC ptr lookup array. */
    rc = MMHyperAlloc(pVM, (cb >> PGM_DYNAMIC_CHUNK_SHIFT) * sizeof(void *), 16, MM_TAG_PGM, (void **)&pNew->pavHCChunkHC);
    AssertRCReturn(rc, rc);
    pNew->pavHCChunkGC = MMHyperCCToGC(pVM, pNew->pavHCChunkHC);
    pNew->fFlags |= MM_RAM_FLAGS_DYNAMIC_ALLOC;

#endif
    RTGCPHYS iPage = cPages;
    while (iPage-- > 0)
        PGM_PAGE_INIT_ZERO(&pNew->aPages[iPage], pVM, PGMPAGETYPE_RAM);

    /*
     * Insert the new RAM range.
     */
    pgmR3PhysLinkRamRange(pVM, pNew, pPrev);

    /*
     * Notify REM.
     */
#ifdef VBOX_WITH_NEW_PHYS_CODE
    REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, 0);
#else
    REMR3NotifyPhysRamRegister(pVM, GCPhys, cb, MM_RAM_FLAGS_DYNAMIC_ALLOC);
#endif

    return VINF_SUCCESS;
}


/**
 * Resets (zeros) the RAM.
 *
 * ASSUMES that the caller owns the PGM lock.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the shared VM structure.
 */
int pgmR3PhysRamReset(PVM pVM)
{
    /*
     * Walk the ram ranges.
     */
    for (PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesR3; pRam; pRam = pRam->pNextR3)
    {
        uint32_t iPage = pRam->cb >> PAGE_SHIFT; Assert((RTGCPHYS)iPage << PAGE_SHIFT == pRam->cb);
#ifdef VBOX_WITH_NEW_PHYS_CODE
        if (!pVM->pgm.f.fRamPreAlloc)
        {
            /* Replace all RAM pages by ZERO pages. */
            while (iPage-- > 0)
            {
                PPGMPAGE pPage = &pRam->aPages[iPage];
                switch (PGM_PAGE_GET_TYPE(pPage))
                {
                    case PGMPAGETYPE_RAM:
                        if (!PGM_PAGE_IS_ZERO(pPage))
                            pgmPhysFreePage(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)i << PAGE_SHIFT));
                        break;

                    case PGMPAGETYPE_MMIO2:
                    case PGMPAGETYPE_ROM_SHADOW: /* handled by pgmR3PhysRomReset. */
                    case PGMPAGETYPE_ROM:
                    case PGMPAGETYPE_MMIO:
                        break;
                    default:
                        AssertFailed();
                }
            } /* for each page */
        }
        else
#endif
        {
            /* Zero the memory. */
            while (iPage-- > 0)
            {
                PPGMPAGE pPage = &pRam->aPages[iPage];
                switch (PGM_PAGE_GET_TYPE(pPage))
                {
#ifndef VBOX_WITH_NEW_PHYS_CODE
                    case PGMPAGETYPE_INVALID:
                    case PGMPAGETYPE_RAM:
                        if (pRam->aPages[iPage].HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)) /** @todo PAGE FLAGS */
                        {
                            /* shadow ram is reloaded elsewhere. */
                            Log4(("PGMR3Reset: not clearing phys page %RGp due to flags %RHp\n", pRam->GCPhys + (iPage << PAGE_SHIFT), pRam->aPages[iPage].HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO))); /** @todo PAGE FLAGS */
                            continue;
                        }
                        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
                        {
                            unsigned iChunk = iPage >> (PGM_DYNAMIC_CHUNK_SHIFT - PAGE_SHIFT);
                            if (pRam->pavHCChunkHC[iChunk])
                                ASMMemZero32((char *)pRam->pavHCChunkHC[iChunk] + ((iPage << PAGE_SHIFT) & PGM_DYNAMIC_CHUNK_OFFSET_MASK), PAGE_SIZE);
                        }
                        else
                            ASMMemZero32((char *)pRam->pvHC + (iPage << PAGE_SHIFT), PAGE_SIZE);
                        break;
#else /* VBOX_WITH_NEW_PHYS_CODE */
                    case PGMPAGETYPE_RAM:
                        switch (PGM_PAGE_GET_STATE(pPage))
                        {
                            case PGM_PAGE_STATE_ZERO:
                                break;
                            case PGM_PAGE_STATE_SHARED:
                            case PGM_PAGE_STATE_WRITE_MONITORED:
                                rc = pgmPhysPageMakeWritable(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)i << PAGE_SHIFT));
                                AssertLogRelRCReturn(rc, rc);
                            case PGM_PAGE_STATE_ALLOCATED:
                            {
                                void *pvPage;
                                PPGMPAGEMAP pMapIgnored;
                                rc = pgmPhysPageMap(pVM, pPage, pRam->GCPhys + ((RTGCPHYS)i << PAGE_SHIFT), &pMapIgnored, &pvPage);
                                AssertLogRelRCReturn(rc, rc);
                                ASMMemZeroPage(pvPage);
                                break;
                            }
                        }
                        break;
#endif /* VBOX_WITH_NEW_PHYS_CODE */

                    case PGMPAGETYPE_MMIO2:
                    case PGMPAGETYPE_ROM_SHADOW:
                    case PGMPAGETYPE_ROM:
                    case PGMPAGETYPE_MMIO:
                        break;
                    default:
                        AssertFailed();

                }
            } /* for each page */
        }

    }

    return VINF_SUCCESS;
}


/**
 * This is the interface IOM is using to register an MMIO region.
 *
 * It will check for conflicts and ensure that a RAM range structure
 * is present before calling the PGMR3HandlerPhysicalRegister API to
 * register the callbacks.
 *
 * @returns VBox status code.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPhys          The start of the MMIO region.
 * @param   cb              The size of the MMIO region.
 * @param   pfnHandlerR3    The address of the ring-3 handler. (IOMR3MMIOHandler)
 * @param   pvUserR3        The user argument for R3.
 * @param   pfnHandlerR0    The address of the ring-0 handler. (IOMMMIOHandler)
 * @param   pvUserR0        The user argument for R0.
 * @param   pfnHandlerGC    The address of the GC handler. (IOMMMIOHandler)
 * @param   pvUserGC        The user argument for GC.
 * @param   pszDesc         The description of the MMIO region.
 */
PDMR3DECL(int) PGMR3PhysMMIORegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb,
                                     R3PTRTYPE(PFNPGMR3PHYSHANDLER) pfnHandlerR3, RTR3PTR pvUserR3,
                                     R0PTRTYPE(PFNPGMR0PHYSHANDLER) pfnHandlerR0, RTR0PTR pvUserR0,
                                     GCPTRTYPE(PFNPGMGCPHYSHANDLER) pfnHandlerGC, RTGCPTR pvUserGC,
                                     R3PTRTYPE(const char *) pszDesc)
{
    /*
     * Assert on some assumption.
     */
    VM_ASSERT_EMT(pVM);
    AssertReturn(!(cb & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(*pszDesc, VERR_INVALID_PARAMETER);

    /*
     * Make sure there's a RAM range structure for the region.
     */
    int rc;
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    bool fRamExists = false;
    PPGMRAMRANGE pRamPrev = NULL;
    PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhysLast >= pRam->GCPhys
            &&  GCPhys     <= pRam->GCPhysLast)
        {
            /* Simplification: all within the same range. */
            AssertLogRelMsgReturn(   GCPhys     >= pRam->GCPhys
                                  && GCPhysLast <= pRam->GCPhysLast,
                                  ("%RGp-%RGp (MMIO/%s) falls partly outside %RGp-%RGp (%s)\n",
                                   GCPhys, GCPhysLast, pszDesc,
                                   pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                  VERR_PGM_RAM_CONFLICT);

            /* Check that it's all RAM or MMIO pages. */
            PCPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
            uint32_t cLeft = cb >> PAGE_SHIFT;
            while (cLeft-- > 0)
            {
                AssertLogRelMsgReturn(   PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM
                                      || PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO,
                                      ("%RGp-%RGp (MMIO/%s): %RGp is not a RAM or MMIO page - type=%d desc=%s\n",
                                       GCPhys, GCPhysLast, pszDesc, PGM_PAGE_GET_TYPE(pPage), pRam->pszDesc),
                                      VERR_PGM_RAM_CONFLICT);
                pPage++;
            }

            /* Looks good. */
            fRamExists = true;
            break;
        }

        /* next */
        pRamPrev = pRam;
        pRam = pRam->pNextR3;
    }
    PPGMRAMRANGE pNew;
    if (fRamExists)
        pNew = NULL;
    else
    {
        /*
         * No RAM range, insert an ad-hoc one.
         *
         * Note that we don't have to tell REM about this range because
         * PGMHandlerPhysicalRegisterEx will do that for us.
         */
        Log(("PGMR3PhysMMIORegister: Adding ad-hoc MMIO range for %RGp-%RGp %s\n", GCPhys, GCPhysLast, pszDesc));

        const uint32_t cPages = cb >> PAGE_SHIFT;
        const size_t cbRamRange = RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]);
        rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]), 16, MM_TAG_PGM_PHYS, (void **)&pNew);
        AssertLogRelMsgRCReturn(rc, ("cbRamRange=%zu\n", cbRamRange), rc);

        /* Initialize the range. */
        pNew->GCPhys        = GCPhys;
        pNew->GCPhysLast    = GCPhysLast;
        pNew->pszDesc       = pszDesc;
        pNew->cb            = cb;
        pNew->fFlags        = 0; /* Some MMIO flag here? */

        pNew->pvHC          = NULL;
        pNew->pavHCChunkHC  = NULL;
        pNew->pavHCChunkGC  = 0;

        uint32_t iPage = cPages;
        while (iPage-- > 0)
            PGM_PAGE_INIT_ZERO_REAL(&pNew->aPages[iPage], pVM, PGMPAGETYPE_MMIO);
        Assert(PGM_PAGE_GET_TYPE(&pNew->aPages[0]) == PGMPAGETYPE_MMIO);

        /* link it */
        pgmR3PhysLinkRamRange(pVM, pNew, pRamPrev);
    }

    /*
     * Register the access handler.
     */
    rc = PGMHandlerPhysicalRegisterEx(pVM, PGMPHYSHANDLERTYPE_MMIO, GCPhys, GCPhysLast,
                                      pfnHandlerR3, pvUserR3,
                                      pfnHandlerR0, pvUserR0,
                                      pfnHandlerGC, pvUserGC, pszDesc);
    if (    RT_FAILURE(rc)
        &&  !fRamExists)
    {
        /* remove the ad-hoc range. */
        pgmR3PhysUnlinkRamRange2(pVM, pNew, pRamPrev);
        pNew->cb = pNew->GCPhys = pNew->GCPhysLast = NIL_RTGCPHYS;
        MMHyperFree(pVM, pRam);
    }

    return rc;
}


/**
 * This is the interface IOM is using to register an MMIO region.
 *
 * It will take care of calling PGMHandlerPhysicalDeregister and clean up
 * any ad-hoc PGMRAMRANGE left behind.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPhys          The start of the MMIO region.
 * @param   cb              The size of the MMIO region.
 */
PDMR3DECL(int) PGMR3PhysMMIODeregister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    VM_ASSERT_EMT(pVM);

    /*
     * First deregister the handler, then check if we should remove the ram range.
     */
    int rc = PGMHandlerPhysicalDeregister(pVM, GCPhys);
    if (RT_SUCCESS(rc))
    {
        RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
        PPGMRAMRANGE pRamPrev = NULL;
        PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesR3;
        while (pRam && GCPhysLast >= pRam->GCPhys)
        {
            /*if (    GCPhysLast >= pRam->GCPhys
                &&  GCPhys     <= pRam->GCPhysLast) - later */
            if (    GCPhysLast == pRam->GCPhysLast
                &&  GCPhys     == pRam->GCPhys)
            {
                Assert(pRam->cb == cb);

                /*
                 * See if all the pages are dead MMIO pages.
                 */
                bool fAllMMIO = true;
                PPGMPAGE pPage = &pRam->aPages[0];
                uint32_t cLeft = cb >> PAGE_SHIFT;
                while (cLeft-- > 0)
                {
                    if (    PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_MMIO
                        /*|| not-out-of-action later */)
                    {
                        fAllMMIO = false;
                        break;
                    }
                    pPage++;
                }

                /*
                 * Unlink it and free if it's all MMIO.
                 */
                if (fAllMMIO)
                {
                    Log(("PGMR3PhysMMIODeregister: Freeing ad-hoc MMIO range for %RGp-%RGp %s\n",
                         GCPhys, GCPhysLast, pRam->pszDesc));

                    pgmR3PhysUnlinkRamRange2(pVM, pRam, pRamPrev);
                    pRam->cb = pRam->GCPhys = pRam->GCPhysLast = NIL_RTGCPHYS;
                    MMHyperFree(pVM, pRam);
                }
                break;
            }

            /* next */
            pRamPrev = pRam;
            pRam = pRam->pNextR3;
        }
    }

    return rc;
}


/**
 * Locate a MMIO2 range.
 *
 * @returns Pointer to the MMIO2 range.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The device instance owning the region.
 * @param   iRegion         The region.
 */
DECLINLINE(PPGMMMIO2RANGE) pgmR3PhysMMIO2Find(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion)
{
    /*
     * Search the list.
     */
    for (PPGMMMIO2RANGE pCur = pVM->pgm.s.pMmio2RangesR3; pCur; pCur = pCur->pNextR3)
        if (pCur->pDevInsR3 == pDevIns)
            return pCur;
    return NULL;
}


/**
 * Allocate and register a MMIO2 region.
 *
 * As mentioned elsewhere, MMIO2 is just RAM spelled differently. It's
 * RAM associated with a device. It is also non-shared memory with a
 * permanent ring-3 mapping and page backing (presently).
 *
 * A MMIO2 range may overlap with base memory if a lot of RAM
 * is configured for the VM, in which case we'll drop the base
 * memory pages. Presently we will make no attempt to preserve
 * anything that happens to be present in the base memory that
 * is replaced, this is of course incorrectly but it's too much
 * effort.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *ppv pointing to the R3 mapping of the memory.
 * @retval  VERR_ALREADY_EXISTS if the region already exists.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The device instance owning the region.
 * @param   iRegion         The region number. If the MMIO2 memory is a PCI I/O region
 *                          this number has to be the number of that region. Otherwise
 *                          it can be any number safe UINT8_MAX.
 * @param   cb              The size of the region. Must be page aligned.
 * @param   fFlags          Reserved for future use, must be zero.
 * @param   ppv             Where to store the pointer to the ring-3 mapping of the memory.
 * @param   pszDesc         The description.
 */
PDMR3DECL(int) PGMR3PhysMMIO2Register(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS cb, uint32_t fFlags, void **ppv, const char *pszDesc)
{
    /*
     * Validate input.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppv, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(*pszDesc, VERR_INVALID_PARAMETER);
    AssertReturn(pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion) == NULL, VERR_ALREADY_EXISTS);
    AssertReturn(!(cb & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(cb, VERR_INVALID_PARAMETER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

    const uint32_t cPages = cb >> PAGE_SHIFT;
    AssertLogRelReturn((RTGCPHYS)cPages << PAGE_SHIFT == cb, VERR_INVALID_PARAMETER);
    AssertLogRelReturn(cPages <= INT32_MAX / 2, VERR_NO_MEMORY);

    /*
     * Try reserve and allocate the backing memory first as this is what is
     * most likely to fail.
     */
    int rc = MMR3AdjustFixedReservation(pVM, cPages, pszDesc);
    if (RT_FAILURE(rc))
        return rc;

    void *pvPages;
    PSUPPAGE paPages = (PSUPPAGE)RTMemTmpAlloc(cPages * sizeof(SUPPAGE));
    if (RT_SUCCESS(rc))
        rc = SUPPageAllocLockedEx(cPages, &pvPages, paPages);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the MMIO2 range record for it.
         */
        const size_t cbRange = RT_OFFSETOF(PGMMMIO2RANGE, RamRange.aPages[cPages]);
        PPGMMMIO2RANGE pNew;
        rc = MMR3HyperAllocOnceNoRel(pVM, cbRange, 0, MM_TAG_PGM_PHYS, (void **)&pNew);
        AssertLogRelMsgRC(rc, ("cbRamRange=%zu\n", cbRange));
        if (RT_SUCCESS(rc))
        {
            pNew->pDevInsR3 = pDevIns;
            pNew->pvR3 = pvPages;
            //pNew->pNext = NULL;
            //pNew->fMapped = false;
            //pNew->fOverlapping = false;
            pNew->iRegion = iRegion;
            pNew->RamRange.GCPhys = NIL_RTGCPHYS;
            pNew->RamRange.GCPhysLast = NIL_RTGCPHYS;
            pNew->RamRange.pszDesc = pszDesc;
            pNew->RamRange.cb = cb;
            //pNew->RamRange.fFlags = 0;

            pNew->RamRange.pvHC = pvPages;      ///@todo remove this
            pNew->RamRange.pavHCChunkHC = NULL; ///@todo remove this
            pNew->RamRange.pavHCChunkGC = 0;    ///@todo remove this

            uint32_t iPage = cPages;
            while (iPage-- > 0)
            {
                PGM_PAGE_INIT(&pNew->RamRange.aPages[iPage],
                              paPages[iPage].Phys & X86_PTE_PAE_PG_MASK, NIL_GMM_PAGEID,
                              PGMPAGETYPE_MMIO2, PGM_PAGE_STATE_ALLOCATED);
            }

            /*
             * Link it into the list.
             * Since there is no particular order, just push it.
             */
            pNew->pNextR3 = pVM->pgm.s.pMmio2RangesR3;
            pVM->pgm.s.pMmio2RangesR3 = pNew;

            *ppv = pvPages;
            RTMemTmpFree(paPages);
            return VINF_SUCCESS;
        }

        SUPPageFreeLocked(pvPages, cPages);
    }
    RTMemTmpFree(paPages);
    MMR3AdjustFixedReservation(pVM, -cPages, pszDesc);
    return rc;
}


/**
 * Deregisters and frees a MMIO2 region.
 *
 * Any physical (and virtual) access handlers registered for the region must
 * be deregistered before calling this function.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The device instance owning the region.
 * @param   iRegion         The region. If it's UINT32_MAX it'll be a wildcard match.
 */
PDMR3DECL(int) PGMR3PhysMMIO2Deregister(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion)
{
    /*
     * Validate input.
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX || iRegion == UINT32_MAX, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    unsigned cFound = 0;
    PPGMMMIO2RANGE pPrev = NULL;
    PPGMMMIO2RANGE pCur = pVM->pgm.s.pMmio2RangesR3;
    while (pCur)
    {
        if (    pCur->pDevInsR3 == pDevIns
            &&  (   iRegion == UINT32_MAX
                 || pCur->iRegion == iRegion))
        {
            cFound++;

            /*
             * Unmap it if it's mapped.
             */
            if (pCur->fMapped)
            {
                int rc2 = PGMR3PhysMMIO2Unmap(pVM, pCur->pDevInsR3, pCur->iRegion, pCur->RamRange.GCPhys);
                AssertRC(rc2);
                if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                    rc = rc2;
            }

            /*
             * Unlink it
             */
            PPGMMMIO2RANGE pNext = pCur->pNextR3;
            if (pPrev)
                pPrev->pNextR3 = pNext;
            else
                pVM->pgm.s.pMmio2RangesR3 = pNext;
            pCur->pNextR3 = NULL;

            /*
             * Free the memory.
             */
            int rc2 = SUPPageFreeLocked(pCur->pvR3, pCur->RamRange.cb >> PAGE_SHIFT);
            AssertRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;

            rc2 = MMR3AdjustFixedReservation(pVM, -(pCur->RamRange.cb >> PAGE_SHIFT), pCur->RamRange.pszDesc);
            AssertRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;

            /* we're leaking hyper memory here if done at runtime. */
            Assert(   VMR3GetState(pVM) == VMSTATE_OFF
                   || VMR3GetState(pVM) == VMSTATE_DESTROYING
                   || VMR3GetState(pVM) == VMSTATE_TERMINATED);
            /*rc = MMHyperFree(pVM, pCur);
            AssertRCReturn(rc, rc); - not safe, see the alloc call. */

            /* next */
            pCur = pNext;
        }
        else
        {
            pPrev = pCur;
            pCur = pCur->pNextR3;
        }
    }

    return !cFound && iRegion != UINT32_MAX ? VERR_NOT_FOUND : rc;
}


/**
 * Maps a MMIO2 region.
 *
 * This is done when a guest / the bios / state loading changes the
 * PCI config. The replacing of base memory has the same restrictions
 * as during registration, of course.
 *
 * @returns VBox status code.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The
 */
PDMR3DECL(int) PGMR3PhysMMIO2Map(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != NIL_RTGCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != 0, VERR_INVALID_PARAMETER);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);

    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(!pCur->fMapped, VERR_WRONG_ORDER);
    Assert(pCur->RamRange.GCPhys == NIL_RTGCPHYS);
    Assert(pCur->RamRange.GCPhysLast == NIL_RTGCPHYS);

    const RTGCPHYS GCPhysLast = GCPhys + pCur->RamRange.cb - 1;
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);

    /*
     * Find our location in the ram range list, checking for
     * restriction we don't bother implementing yet (partially overlapping).
     */
    bool fRamExists = false;
    PPGMRAMRANGE pRamPrev = NULL;
    PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhys     <= pRam->GCPhysLast
            &&  GCPhysLast >= pRam->GCPhys)
        {
            /* completely within? */
            AssertLogRelMsgReturn(   GCPhys     >= pRam->GCPhys
                                  && GCPhysLast <= pRam->GCPhysLast,
                                  ("%RGp-%RGp (MMIO2/%s) falls partly outside %RGp-%RGp (%s)\n",
                                   GCPhys, GCPhysLast, pCur->RamRange.pszDesc,
                                   pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            fRamExists = true;
            break;
        }

        /* next */
        pRamPrev = pRam;
        pRam = pRam->pNextR3;
    }
    if (fRamExists)
    {
        PPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            AssertLogRelMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM,
                                  ("%RGp isn't a RAM page (%d) - mapping %RGp-%RGp (MMIO2/%s).\n",
                                   GCPhys, PGM_PAGE_GET_TYPE(pPage), GCPhys, GCPhysLast, pCur->RamRange.pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            pPage++;
        }
    }
    Log(("PGMR3PhysMMIO2Map: %RGp-%RGp fRamExists=%RTbool %s\n",
         GCPhys, GCPhysLast, fRamExists, pCur->RamRange.pszDesc));

    /*
     * Make the changes.
     */
    pgmLock(pVM);

    pCur->RamRange.GCPhys = GCPhys;
    pCur->RamRange.GCPhysLast = GCPhysLast;
    pCur->fMapped = true;
    pCur->fOverlapping = fRamExists;

    if (fRamExists)
    {
        /* replace the pages, freeing all present RAM pages. */
        PPGMPAGE pPageSrc = &pCur->RamRange.aPages[0];
        PPGMPAGE pPageDst = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            pgmPhysFreePage(pVM, pPageDst, GCPhys);

            RTHCPHYS const HCPhys = PGM_PAGE_GET_HCPHYS(pPageSrc);
            PGM_PAGE_SET_HCPHYS(pPageDst, HCPhys);
            PGM_PAGE_SET_TYPE(pPageDst, PGMPAGETYPE_MMIO2);
            PGM_PAGE_SET_STATE(pPageDst, PGM_PAGE_STATE_ALLOCATED);

            GCPhys += PAGE_SIZE;
            pPageSrc++;
            pPageDst++;
        }
    }
    else
    {
        /* link in the ram range */
        pgmR3PhysLinkRamRange(pVM, &pCur->RamRange, pRamPrev);
        REMR3NotifyPhysRamRegister(pVM, GCPhys, pCur->RamRange.cb, 0);
    }

    pgmUnlock(pVM);

    return VINF_SUCCESS;
}


/**
 * Unmaps a MMIO2 region.
 *
 * This is done when a guest / the bios / state loading changes the
 * PCI config. The replacing of base memory has the same restrictions
 * as during registration, of course.
 */
PDMR3DECL(int) PGMR3PhysMMIO2Unmap(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != NIL_RTGCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(GCPhys != 0, VERR_INVALID_PARAMETER);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);

    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(pCur->fMapped, VERR_WRONG_ORDER);
    AssertReturn(pCur->RamRange.GCPhys == GCPhys, VERR_INVALID_PARAMETER);
    Assert(pCur->RamRange.GCPhysLast != NIL_RTGCPHYS);

    Log(("PGMR3PhysMMIO2Unmap: %RGp-%RGp %s\n",
         pCur->RamRange.GCPhys, pCur->RamRange.GCPhysLast, pCur->RamRange.pszDesc));

    /*
     * Unmap it.
     */
    pgmLock(pVM);

    if (pCur->fOverlapping)
    {
        /* Restore the RAM pages we've replaced. */
        PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesR3;
        while (pRam->GCPhys > pCur->RamRange.GCPhysLast)
            pRam = pRam->pNextR3;

        RTHCPHYS const HCPhysZeroPg = pVM->pgm.s.HCPhysZeroPg;
        Assert(HCPhysZeroPg != 0 && HCPhysZeroPg != NIL_RTHCPHYS);
        PPGMPAGE pPageDst = &pRam->aPages[(pCur->RamRange.GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = pCur->RamRange.cb >> PAGE_SHIFT;
        while (cPagesLeft-- > 0)
        {
            PGM_PAGE_SET_HCPHYS(pPageDst, pVM->pgm.s.HCPhysZeroPg);
            PGM_PAGE_SET_TYPE(pPageDst, PGMPAGETYPE_RAM);
            PGM_PAGE_SET_STATE(pPageDst, PGM_PAGE_STATE_ZERO);

            pPageDst++;
        }
    }
    else
    {
        REMR3NotifyPhysReserve(pVM, pCur->RamRange.GCPhys, pCur->RamRange.cb);
        pgmR3PhysUnlinkRamRange(pVM, &pCur->RamRange);
    }

    pCur->RamRange.GCPhys = NIL_RTGCPHYS;
    pCur->RamRange.GCPhysLast = NIL_RTGCPHYS;
    pCur->fOverlapping = false;
    pCur->fMapped = false;

    pgmUnlock(pVM);

    return VINF_SUCCESS;
}


/**
 * Checks if the given address is an MMIO2 base address or not.
 *
 * @returns true/false accordingly.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The owner of the memory, optional.
 * @param   GCPhys          The address to check.
 */
PDMR3DECL(bool) PGMR3PhysMMIO2IsBase(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, false);
    AssertPtrReturn(pDevIns, false);
    AssertReturn(GCPhys != NIL_RTGCPHYS, false);
    AssertReturn(GCPhys != 0, false);
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), false);

    /*
     * Search the list.
     */
    for (PPGMMMIO2RANGE pCur = pVM->pgm.s.pMmio2RangesR3; pCur; pCur = pCur->pNextR3)
        if (pCur->RamRange.GCPhys == GCPhys)
        {
            Assert(pCur->fMapped);
            return true;
        }
    return false;
}


/**
 * Gets the HC physical address of a page in the MMIO2 region.
 *
 * This is API is intended for MMHyper and shouldn't be called
 * by anyone else...
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The owner of the memory, optional.
 * @param   iRegion         The region.
 * @param   off             The page expressed an offset into the MMIO2 region.
 * @param   pHCPhys         Where to store the result.
 */
PDMR3DECL(int) PGMR3PhysMMIO2GetHCPhys(PVM pVM, PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, PRTHCPHYS pHCPhys)
{
    /*
     * Validate input
     */
    VM_ASSERT_EMT_RETURN(pVM, VERR_VM_THREAD_NOT_EMT);
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(iRegion <= UINT8_MAX, VERR_INVALID_PARAMETER);

    PPGMMMIO2RANGE pCur = pgmR3PhysMMIO2Find(pVM, pDevIns, iRegion);
    AssertReturn(pCur, VERR_NOT_FOUND);
    AssertReturn(off < pCur->RamRange.cb, VERR_INVALID_PARAMETER);

    PCPGMPAGE pPage = &pCur->RamRange.aPages[off >> PAGE_SHIFT];
    *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage);
    return VINF_SUCCESS;
}


/**
 * Registers a ROM image.
 *
 * Shadowed ROM images requires double the amount of backing memory, so,
 * don't use that unless you have to. Shadowing of ROM images is process
 * where we can select where the reads go and where the writes go. On real
 * hardware the chipset provides means to configure this. We provide
 * PGMR3PhysProtectROM() for this purpose.
 *
 * A read-only copy of the ROM image will always be kept around while we
 * will allocate RAM pages for the changes on demand (unless all memory
 * is configured to be preallocated).
 *
 * @returns VBox status.
 * @param   pVM                 VM Handle.
 * @param   pDevIns             The device instance owning the ROM.
 * @param   GCPhys              First physical address in the range.
 *                              Must be page aligned!
 * @param   cbRange             The size of the range (in bytes).
 *                              Must be page aligned!
 * @param   pvBinary            Pointer to the binary data backing the ROM image.
 *                              This must be exactly \a cbRange in size.
 * @param   fFlags              Mask of flags. PGMPHYS_ROM_FLAG_SHADOWED
 *                              and/or PGMPHYS_ROM_FLAG_PERMANENT_BINARY.
 * @param   pszDesc             Pointer to description string. This must not be freed.
 *
 * @remark  There is no way to remove the rom, automatically on device cleanup or
 *          manually from the device yet. This isn't difficult in any way, it's
 *          just not something we expect to be necessary for a while.
 */
PGMR3DECL(int) PGMR3PhysRomRegister(PVM pVM, PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTGCPHYS cb,
                                    const void *pvBinary, uint32_t fFlags, const char *pszDesc)
{
    Log(("PGMR3PhysRomRegister: pDevIns=%p GCPhys=%RGp(-%RGp) cb=%RGp pvBinary=%p fFlags=%#x pszDesc=%s\n",
         pDevIns, GCPhys, GCPhys + cb, cb, pvBinary, fFlags, pszDesc));

    /*
     * Validate input.
     */
    AssertPtrReturn(pDevIns, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(RT_ALIGN_T(cb, PAGE_SIZE, RTGCPHYS) == cb, VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pvBinary, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDesc, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~(PGMPHYS_ROM_FLAG_SHADOWED | PGMPHYS_ROM_FLAG_PERMANENT_BINARY)), VERR_INVALID_PARAMETER);
    VM_ASSERT_STATE_RETURN(pVM, VMSTATE_CREATING, VERR_VM_INVALID_VM_STATE);

    const uint32_t cPages = cb >> PAGE_SHIFT;

    /*
     * Find the ROM location in the ROM list first.
     */
    PPGMROMRANGE    pRomPrev = NULL;
    PPGMROMRANGE    pRom = pVM->pgm.s.pRomRangesR3;
    while (pRom && GCPhysLast >= pRom->GCPhys)
    {
        if (    GCPhys     <= pRom->GCPhysLast
            &&  GCPhysLast >= pRom->GCPhys)
            AssertLogRelMsgFailedReturn(("%RGp-%RGp (%s) conflicts with existing %RGp-%RGp (%s)\n",
                                         GCPhys, GCPhysLast, pszDesc,
                                         pRom->GCPhys, pRom->GCPhysLast, pRom->pszDesc),
                                        VERR_PGM_RAM_CONFLICT);
        /* next */
        pRomPrev = pRom;
        pRom = pRom->pNextR3;
    }

    /*
     * Find the RAM location and check for conflicts.
     *
     * Conflict detection is a bit different than for RAM
     * registration since a ROM can be located within a RAM
     * range. So, what we have to check for is other memory
     * types (other than RAM that is) and that we don't span
     * more than one RAM range (layz).
     */
    bool            fRamExists = false;
    PPGMRAMRANGE    pRamPrev = NULL;
    PPGMRAMRANGE    pRam = pVM->pgm.s.pRamRangesR3;
    while (pRam && GCPhysLast >= pRam->GCPhys)
    {
        if (    GCPhys     <= pRam->GCPhysLast
            &&  GCPhysLast >= pRam->GCPhys)
        {
            /* completely within? */
            AssertLogRelMsgReturn(   GCPhys     >= pRam->GCPhys
                                  && GCPhysLast <= pRam->GCPhysLast,
                                  ("%RGp-%RGp (%s) falls partly outside %RGp-%RGp (%s)\n",
                                   GCPhys, GCPhysLast, pszDesc,
                                   pRam->GCPhys, pRam->GCPhysLast, pRam->pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            fRamExists = true;
            break;
        }

        /* next */
        pRamPrev = pRam;
        pRam = pRam->pNextR3;
    }
    if (fRamExists)
    {
        PPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
        uint32_t cPagesLeft = cPages;
        while (cPagesLeft-- > 0)
        {
            AssertLogRelMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM,
                                  ("%RGp isn't a RAM page (%d) - registering %RGp-%RGp (%s).\n",
                                   GCPhys, PGM_PAGE_GET_TYPE(pPage), GCPhys, GCPhysLast, pszDesc),
                                  VERR_PGM_RAM_CONFLICT);
            Assert(PGM_PAGE_IS_ZERO(pPage));
            pPage++;
        }
    }

    /*
     * Update the base memory reservation if necessary.
     */
    uint32_t cExtraBaseCost = fRamExists ? cPages : 0;
    if (fFlags & PGMPHYS_ROM_FLAG_SHADOWED)
        cExtraBaseCost += cPages;
    if (cExtraBaseCost)
    {
        int rc = MMR3IncreaseBaseReservation(pVM, cExtraBaseCost);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Allocate memory for the virgin copy of the RAM.
     */
    PGMMALLOCATEPAGESREQ pReq;
    int rc = GMMR3AllocatePagesPrepare(pVM, &pReq, cPages, GMMACCOUNT_BASE);
    AssertRCReturn(rc, rc);

    for (uint32_t iPage = 0; iPage < cPages; iPage++)
    {
        pReq->aPages[iPage].HCPhysGCPhys = GCPhys + (iPage << PAGE_SHIFT);
        pReq->aPages[iPage].idPage = NIL_GMM_PAGEID;
        pReq->aPages[iPage].idSharedPage = NIL_GMM_PAGEID;
    }

    pgmLock(pVM);
    rc = GMMR3AllocatePagesPerform(pVM, pReq);
    pgmUnlock(pVM);
    if (RT_FAILURE(rc))
    {
        GMMR3AllocatePagesCleanup(pReq);
        return rc;
    }

    /*
     * Allocate the new ROM range and RAM range (if necessary).
     */
    PPGMROMRANGE pRomNew;
    rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMROMRANGE, aPages[cPages]), 0, MM_TAG_PGM_PHYS, (void **)&pRomNew);
    if (RT_SUCCESS(rc))
    {
        PPGMRAMRANGE pRamNew = NULL;
        if (!fRamExists)
            rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]), sizeof(PGMPAGE), MM_TAG_PGM_PHYS, (void **)&pRamNew);
        if (RT_SUCCESS(rc))
        {
            pgmLock(pVM);

            /*
             * Initialize and insert the RAM range (if required).
             */
            PPGMROMPAGE pRomPage = &pRomNew->aPages[0];
            if (!fRamExists)
            {
                pRamNew->GCPhys        = GCPhys;
                pRamNew->GCPhysLast    = GCPhysLast;
                pRamNew->pszDesc       = pszDesc;
                pRamNew->cb            = cb;
                pRamNew->fFlags        = 0;
                pRamNew->pvHC          = NULL;

                PPGMPAGE pPage = &pRamNew->aPages[0];
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pPage++, pRomPage++)
                {
                    PGM_PAGE_INIT(pPage,
                                  pReq->aPages[iPage].HCPhysGCPhys,
                                  pReq->aPages[iPage].idPage,
                                  PGMPAGETYPE_ROM,
                                  PGM_PAGE_STATE_ALLOCATED);

                    pRomPage->Virgin = *pPage;
                }

                pgmR3PhysLinkRamRange(pVM, pRamNew, pRamPrev);
            }
            else
            {
                PPGMPAGE pPage = &pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT];
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pPage++, pRomPage++)
                {
                    PGM_PAGE_SET_TYPE(pPage,   PGMPAGETYPE_ROM);
                    PGM_PAGE_SET_HCPHYS(pPage, pReq->aPages[iPage].HCPhysGCPhys);
                    PGM_PAGE_SET_STATE(pPage,  PGM_PAGE_STATE_ALLOCATED);
                    PGM_PAGE_SET_PAGEID(pPage, pReq->aPages[iPage].idPage);

                    pRomPage->Virgin = *pPage;
                }

                pRamNew = pRam;
            }
            pgmUnlock(pVM);


            /*
             * Register the write access handler for the range (PGMROMPROT_READ_ROM_WRITE_IGNORE).
             */
            rc = PGMR3HandlerPhysicalRegister(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhys, GCPhysLast,
#if 0 /** @todo we actually need a ring-3 write handler here for shadowed ROMs, so hack REM! */
                                              pgmR3PhysRomWriteHandler, pRomNew,
#else
                                              NULL, NULL,
#endif
                                              NULL, "pgmPhysRomWriteHandler", MMHyperCCToR0(pVM, pRomNew),
                                              NULL, "pgmPhysRomWriteHandler", MMHyperCCToGC(pVM, pRomNew), pszDesc);
            if (RT_SUCCESS(rc))
            {
                pgmLock(pVM);

                /*
                 * Copy the image over to the virgin pages.
                 * This must be done after linking in the RAM range.
                 */
                PPGMPAGE pRamPage = &pRamNew->aPages[(GCPhys - pRamNew->GCPhys) >> PAGE_SHIFT];
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pRamPage++)
                {
                    void *pvDstPage;
                    PPGMPAGEMAP pMapIgnored;
                    rc = pgmPhysPageMap(pVM, pRamPage, GCPhys + (iPage << PAGE_SHIFT), &pMapIgnored, &pvDstPage);
                    if (RT_FAILURE(rc))
                    {
                        VMSetError(pVM, rc, RT_SRC_POS, "Failed to map virgin ROM page at %RGp", GCPhys);
                        break;
                    }
                    memcpy(pvDstPage, (const uint8_t *)pvBinary + (iPage << PAGE_SHIFT), PAGE_SIZE);
                }
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Initialize the ROM range.
                     * Note that the Virgin member of the pages has already been initialized above.
                     */
                    pRomNew->GCPhys = GCPhys;
                    pRomNew->GCPhysLast = GCPhysLast;
                    pRomNew->cb = cb;
                    pRomNew->fFlags = fFlags;
                    pRomNew->pvOriginal = fFlags & PGMPHYS_ROM_FLAG_PERMANENT_BINARY ? pvBinary : NULL;
                    pRomNew->pszDesc = pszDesc;

                    for (unsigned iPage = 0; iPage < cPages; iPage++)
                    {
                        PPGMROMPAGE pPage = &pRomNew->aPages[iPage];
                        pPage->enmProt = PGMROMPROT_READ_ROM_WRITE_IGNORE;
                        PGM_PAGE_INIT_ZERO_REAL(&pPage->Shadow, pVM, PGMPAGETYPE_ROM_SHADOW);
                    }

                    /*
                     * Insert the ROM range, tell REM and return successfully.
                     */
                    pRomNew->pNextR3 = pRom;
                    pRomNew->pNextR0 = pRom ? MMHyperCCToR0(pVM, pRom) : NIL_RTR0PTR;
                    pRomNew->pNextGC = pRom ? MMHyperCCToGC(pVM, pRom) : NIL_RTGCPTR;

                    if (pRomPrev)
                    {
                        pRomPrev->pNextR3 = pRomNew;
                        pRomPrev->pNextR0 = MMHyperCCToR0(pVM, pRomNew);
                        pRomPrev->pNextGC = MMHyperCCToGC(pVM, pRomNew);
                    }
                    else
                    {
                        pVM->pgm.s.pRomRangesR3 = pRomNew;
                        pVM->pgm.s.pRomRangesR0 = MMHyperCCToR0(pVM, pRomNew);
                        pVM->pgm.s.pRomRangesGC = MMHyperCCToGC(pVM, pRomNew);
                    }

                    REMR3NotifyPhysRomRegister(pVM, GCPhys, cb, NULL, false); /** @todo fix shadowing and REM. */

                    GMMR3AllocatePagesCleanup(pReq);
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }

                /* bail out */

                pgmUnlock(pVM);
                int rc2 = PGMHandlerPhysicalDeregister(pVM, GCPhys);
                AssertRC(rc2);
                pgmLock(pVM);
            }

            pgmR3PhysUnlinkRamRange2(pVM, pRamNew, pRamPrev);
            if (pRamNew)
                MMHyperFree(pVM, pRamNew);
        }
        MMHyperFree(pVM, pRomNew);
    }

    /** @todo Purge the mapping cache or something... */
    GMMR3FreeAllocatedPages(pVM, pReq);
    GMMR3AllocatePagesCleanup(pReq);
    pgmUnlock(pVM);
    return rc;
}


/**
 * \#PF Handler callback for ROM write accesses.
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
/*static - shut up warning */
 DECLCALLBACK(int) pgmR3PhysRomWriteHandler(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    PPGMROMRANGE    pRom = (PPGMROMRANGE)pvUser;
    const uint32_t  iPage = GCPhys - pRom->GCPhys;
    Assert(iPage < (pRom->cb >> PAGE_SHIFT));
    PPGMROMPAGE     pRomPage = &pRom->aPages[iPage];
    switch (pRomPage->enmProt)
    {
        /*
         * Ignore.
         */
        case PGMROMPROT_READ_ROM_WRITE_IGNORE:
        case PGMROMPROT_READ_RAM_WRITE_IGNORE:
            return VINF_SUCCESS;

        /*
         * Write to the ram page.
         */
        case PGMROMPROT_READ_ROM_WRITE_RAM:
        case PGMROMPROT_READ_RAM_WRITE_RAM: /* yes this will get here too, it's *way* simpler that way. */
        {
            /* This should be impossible now, pvPhys doesn't work cross page anylonger. */
            Assert(((GCPhys - pRom->GCPhys + cbBuf - 1) >> PAGE_SHIFT) == iPage);

            /*
             * Take the lock, do lazy allocation, map the page and copy the data.
             *
             * Note that we have to bypass the mapping TLB since it works on
             * guest physical addresses and entering the shadow page would
             * kind of screw things up...
             */
            int rc = pgmLock(pVM);
            AssertRC(rc);

            if (RT_UNLIKELY(PGM_PAGE_GET_STATE(&pRomPage->Shadow) != PGM_PAGE_STATE_ALLOCATED))
            {
                rc = pgmPhysPageMakeWritable(pVM, &pRomPage->Shadow, GCPhys);
                if (RT_FAILURE(rc))
                {
                    pgmUnlock(pVM);
                    return rc;
                }
            }

            void *pvDstPage;
            PPGMPAGEMAP pMapIgnored;
            rc = pgmPhysPageMap(pVM, &pRomPage->Shadow, GCPhys & X86_PTE_PG_MASK, &pMapIgnored, &pvDstPage);
            if (RT_SUCCESS(rc))
                memcpy((uint8_t *)pvDstPage + (GCPhys & PAGE_OFFSET_MASK), pvBuf, cbBuf);

            pgmUnlock(pVM);
            return rc;
        }

        default:
            AssertMsgFailedReturn(("enmProt=%d iPage=%d GCPhys=%RGp\n",
                                   pRom->aPages[iPage].enmProt, iPage, GCPhys),
                                  VERR_INTERNAL_ERROR);
    }
}



/**
 * Called by PGMR3Reset to reset the shadow, switch to the virgin,
 * and verify that the virgin part is untouched.
 *
 * This is done after the normal memory has been cleared.
 *
 * ASSUMES that the caller owns the PGM lock.
 *
 * @param   pVM         The VM handle.
 */
int pgmR3PhysRomReset(PVM pVM)
{
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        const uint32_t cPages = pRom->cb >> PAGE_SHIFT;

        if (pRom->fFlags & PGMPHYS_ROM_FLAG_SHADOWED)
        {
            /*
             * Reset the physical handler.
             */
            int rc = PGMR3PhysRomProtect(pVM, pRom->GCPhys, pRom->cb, PGMROMPROT_READ_ROM_WRITE_IGNORE);
            AssertRCReturn(rc, rc);

            /*
             * What we do with the shadow pages depends on the memory
             * preallocation option. If not enabled, we'll just throw
             * out all the dirty pages and replace them by the zero page.
             */
            if (1)///@todo !pVM->pgm.f.fRamPreAlloc)
            {
                /* Count dirty shadow pages. */
                uint32_t cDirty = 0;
                uint32_t iPage = cPages;
                while (iPage-- > 0)
                    if (PGM_PAGE_GET_STATE(&pRom->aPages[iPage].Shadow) != PGM_PAGE_STATE_ZERO)
                        cDirty++;
                if (cDirty)
                {
                    /* Free the dirty pages. */
                    PGMMFREEPAGESREQ pReq;
                    rc = GMMR3FreePagesPrepare(pVM, &pReq, cDirty, GMMACCOUNT_BASE);
                    AssertRCReturn(rc, rc);

                    uint32_t iReqPage = 0;
                    for (iPage = 0; iPage < cPages; iPage++)
                        if (PGM_PAGE_GET_STATE(&pRom->aPages[iPage].Shadow) != PGM_PAGE_STATE_ZERO)
                        {
                            pReq->aPages[iReqPage].idPage = PGM_PAGE_GET_PAGEID(&pRom->aPages[iPage].Shadow);
                            iReqPage++;
                        }

                    rc = GMMR3FreePagesPerform(pVM, pReq);
                    GMMR3FreePagesCleanup(pReq);
                    AssertRCReturn(rc, rc);

                    /* setup the zero page. */
                    for (iPage = 0; iPage < cPages; iPage++)
                        if (PGM_PAGE_GET_STATE(&pRom->aPages[iPage].Shadow) != PGM_PAGE_STATE_ZERO)
                            PGM_PAGE_INIT_ZERO_REAL(&pRom->aPages[iPage].Shadow, pVM, PGMPAGETYPE_ROM_SHADOW);
                }
            }
            else
            {
                /* clear all the pages. */
                for (uint32_t iPage = 0; iPage < cPages; iPage++)
                {
                    const RTGCPHYS GCPhys = pRom->GCPhys + (iPage << PAGE_SHIFT);
                    rc = pgmPhysPageMakeWritable(pVM, &pRom->aPages[iPage].Shadow, GCPhys);
                    if (RT_FAILURE(rc))
                        break;

                    void *pvDstPage;
                    PPGMPAGEMAP pMapIgnored;
                    rc = pgmPhysPageMap(pVM, &pRom->aPages[iPage].Shadow, GCPhys, &pMapIgnored, &pvDstPage);
                    if (RT_FAILURE(rc))
                        break;
                    ASMMemZeroPage(pvDstPage);
                }
                AssertRCReturn(rc, rc);
            }
        }

#ifdef VBOX_STRICT
        /*
         * Verify that the virgin page is unchanged if possible.
         */
        if (pRom->pvOriginal)
        {
            uint8_t const *pbSrcPage = (uint8_t const *)pRom->pvOriginal;
            for (uint32_t iPage = 0; iPage < cPages; iPage++, pbSrcPage += PAGE_SIZE)
            {
                const RTGCPHYS GCPhys = pRom->GCPhys + (iPage << PAGE_SHIFT);
                PPGMPAGEMAP pMapIgnored;
                void *pvDstPage;
                int rc = pgmPhysPageMap(pVM, &pRom->aPages[iPage].Virgin, GCPhys, &pMapIgnored, &pvDstPage);
                if (RT_FAILURE(rc))
                    break;
                if (memcmp(pvDstPage, pbSrcPage, PAGE_SIZE))
                    LogRel(("pgmR3PhysRomReset: %RGp rom page changed (%s) - loaded saved state?\n",
                            GCPhys, pRom->pszDesc));
            }
        }
#endif
    }

    return VINF_SUCCESS;
}


/**
 * Change the shadowing of a range of ROM pages.
 *
 * This is intended for implementing chipset specific memory registers
 * and will not be very strict about the input. It will silently ignore
 * any pages that are not the part of a shadowed ROM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the shared VM structure.
 * @param   GCPhys      Where to start. Page aligned.
 * @param   cb          How much to change. Page aligned.
 * @param   enmProt     The new ROM protection.
 */
PGMR3DECL(int) PGMR3PhysRomProtect(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, PGMROMPROT enmProt)
{
    /*
     * Check input
     */
    if (!cb)
        return VINF_SUCCESS;
    AssertReturn(!(GCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(cb & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);
    AssertReturn(enmProt >= PGMROMPROT_INVALID && enmProt <= PGMROMPROT_END, VERR_INVALID_PARAMETER);

    /*
     * Process the request.
     */
    bool fFlushedPool = false;
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
        if (    GCPhys     <= pRom->GCPhysLast
            &&  GCPhysLast >= pRom->GCPhys)
        {
            /*
             * Iterate the relevant pages and the ncessary make changes.
             */
            bool fChanges = false;
            uint32_t const cPages = pRom->GCPhysLast > GCPhysLast
                                  ? pRom->cb >> PAGE_SHIFT
                                  : (GCPhysLast - pRom->GCPhys) >> PAGE_SHIFT;
            for (uint32_t iPage = (GCPhys - pRom->GCPhys) >> PAGE_SHIFT;
                 iPage < cPages;
                 iPage++)
            {
                PPGMROMPAGE pRomPage = &pRom->aPages[iPage];
                if (PGMROMPROT_IS_ROM(pRomPage->enmProt) != PGMROMPROT_IS_ROM(enmProt))
                {
                    fChanges = true;

                    /* flush the page pool first so we don't leave any usage references dangling. */
                    if (!fFlushedPool)
                    {
                        pgmPoolFlushAll(pVM);
                        fFlushedPool = true;
                    }

                    PPGMPAGE pOld = PGMROMPROT_IS_ROM(pRomPage->enmProt) ? &pRomPage->Virgin : &pRomPage->Shadow;
                    PPGMPAGE pNew = PGMROMPROT_IS_ROM(pRomPage->enmProt) ? &pRomPage->Shadow : &pRomPage->Virgin;
                    PPGMPAGE pRamPage = pgmPhysGetPage(&pVM->pgm.s, pRom->GCPhys + (iPage << PAGE_SHIFT));

                    *pOld = *pRamPage;
                    *pRamPage = *pNew;
                    /** @todo preserve the volatile flags (handlers) when these have been moved out of HCPhys! */
                }
            }

            /*
             * Reset the access handler if we made changes, no need
             * to optimize this.
             */
            if (fChanges)
            {
                int rc = PGMHandlerPhysicalReset(pVM, pRom->GCPhys);
                AssertRCReturn(rc, rc);
            }

            /* Advance - cb isn't updated. */
            GCPhys = pRom->GCPhys + (cPages << PAGE_SHIFT);
        }

    return VINF_SUCCESS;
}


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
PGMR3DECL(int) PGMR3PhysRegister(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc)
{
    /*
     * Validate input.
     * (Not so important because callers are only MMR3PhysRegister()
     *  and PGMR3HandlerPhysicalRegisterEx(), but anyway...)
     */
    Log(("PGMR3PhysRegister %08X %x bytes flags %x %s\n", GCPhys, cb, fFlags, pszDesc));

    Assert((fFlags & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_DYNAMIC_ALLOC)) || paPages);
    /*Assert(!(fFlags & MM_RAM_FLAGS_RESERVED) || !paPages);*/
    Assert((fFlags == (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO)) || (fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC) || pvRam);
    /*Assert(!(fFlags & MM_RAM_FLAGS_RESERVED) || !pvRam);*/
    Assert(!(fFlags & ~0xfff));
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_P(pvRam, PAGE_SIZE) == pvRam);
    Assert(!(fFlags & ~(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_DYNAMIC_ALLOC)));
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    if (GCPhysLast < GCPhys)
    {
        AssertMsgFailed(("The range wraps! GCPhys=%VGp cb=%#x\n", GCPhys, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find range location and check for conflicts.
     */
    PPGMRAMRANGE    pPrev = NULL;
    PPGMRAMRANGE    pCur = pVM->pgm.s.pRamRangesR3;
    while (pCur)
    {
        if (GCPhys <= pCur->GCPhysLast && GCPhysLast >= pCur->GCPhys)
        {
            AssertMsgFailed(("Conflict! This cannot happen!\n"));
            return VERR_PGM_RAM_CONFLICT;
        }
        if (GCPhysLast < pCur->GCPhys)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }

    /*
     * Allocate RAM range.
     * Small ranges are allocated from the heap, big ones have separate mappings.
     */
    size_t          cbRam = RT_OFFSETOF(PGMRAMRANGE, aPages[cb >> PAGE_SHIFT]);
    PPGMRAMRANGE    pNew;
    RTGCPTR         GCPtrNew;
    int             rc = VERR_NO_MEMORY;
    if (cbRam > PAGE_SIZE / 2)
    {   /* large */
        cbRam = RT_ALIGN_Z(cbRam, PAGE_SIZE);
        rc = SUPPageAlloc(cbRam >> PAGE_SHIFT, (void **)&pNew);
        if (VBOX_SUCCESS(rc))
        {
            rc = MMR3HyperMapHCRam(pVM, pNew, cbRam, true, pszDesc, &GCPtrNew);
            if (VBOX_SUCCESS(rc))
            {
                Assert(MMHyperHC2GC(pVM, pNew) == GCPtrNew);
                rc = MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);
            }
            else
            {
                AssertMsgFailed(("MMR3HyperMapHCRam(,,%#x,,,) -> %Vrc\n", cbRam, rc));
                SUPPageFree(pNew, cbRam >> PAGE_SHIFT);
            }
        }
        else
            AssertMsgFailed(("SUPPageAlloc(%#x,,) -> %Vrc\n", cbRam >> PAGE_SHIFT, rc));

    }
/** @todo Make VGA and VMMDev register their memory at init time before the hma size is fixated. */
    if (RT_FAILURE(rc))
    {   /* small + fallback (vga) */
        rc = MMHyperAlloc(pVM, cbRam, 16, MM_TAG_PGM, (void **)&pNew);
        if (VBOX_SUCCESS(rc))
            GCPtrNew = MMHyperHC2GC(pVM, pNew);
        else
            AssertMsgFailed(("MMHyperAlloc(,%#x,,,) -> %Vrc\n", cbRam, cb));
    }
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Initialize the range.
         */
        pNew->pvHC          = pvRam;
        pNew->GCPhys        = GCPhys;
        pNew->GCPhysLast    = GCPhysLast;
        pNew->cb            = cb;
        pNew->fFlags        = fFlags;
        pNew->pavHCChunkHC  = NULL;
        pNew->pavHCChunkGC  = 0;

        unsigned iPage = cb >> PAGE_SHIFT;
        if (paPages)
        {
            while (iPage-- > 0)
            {
                PGM_PAGE_INIT(&pNew->aPages[iPage], paPages[iPage].Phys & X86_PTE_PAE_PG_MASK, NIL_GMM_PAGEID,
                              fFlags & MM_RAM_FLAGS_MMIO2 ? PGMPAGETYPE_MMIO2 : PGMPAGETYPE_RAM,
                              PGM_PAGE_STATE_ALLOCATED);
                pNew->aPages[iPage].HCPhys |= fFlags; /** @todo PAGE FLAGS*/
            }
        }
        else if (fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            /* Allocate memory for chunk to HC ptr lookup array. */
            rc = MMHyperAlloc(pVM, (cb >> PGM_DYNAMIC_CHUNK_SHIFT) * sizeof(void *), 16, MM_TAG_PGM, (void **)&pNew->pavHCChunkHC);
            AssertMsgReturn(rc == VINF_SUCCESS, ("MMHyperAlloc(,%#x,,,) -> %Vrc\n", cbRam, cb), rc);

            pNew->pavHCChunkGC = MMHyperHC2GC(pVM, pNew->pavHCChunkHC);
            Assert(pNew->pavHCChunkGC);

            /* Physical memory will be allocated on demand. */
            while (iPage-- > 0)
            {
                PGM_PAGE_INIT(&pNew->aPages[iPage], 0, NIL_GMM_PAGEID, PGMPAGETYPE_RAM, PGM_PAGE_STATE_ZERO);
                pNew->aPages[iPage].HCPhys = fFlags; /** @todo PAGE FLAGS */
            }
        }
        else
        {
            Assert(fFlags == (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO));
            RTHCPHYS HCPhysDummyPage = MMR3PageDummyHCPhys(pVM);
            while (iPage-- > 0)
            {
                PGM_PAGE_INIT(&pNew->aPages[iPage], HCPhysDummyPage, NIL_GMM_PAGEID, PGMPAGETYPE_MMIO, PGM_PAGE_STATE_ZERO);
                pNew->aPages[iPage].HCPhys |= fFlags; /** @todo PAGE FLAGS*/
            }
        }

        /*
         * Insert the new RAM range.
         */
        pgmLock(pVM);
        pNew->pNextR3 = pCur;
        pNew->pNextR0 = pCur ? MMHyperCCToR0(pVM, pCur) : NIL_RTR0PTR;
        pNew->pNextGC = pCur ? MMHyperCCToGC(pVM, pCur) : NIL_RTGCPTR;
        if (pPrev)
        {
            pPrev->pNextR3 = pNew;
            pPrev->pNextR0 = MMHyperCCToR0(pVM, pNew);
            pPrev->pNextGC = GCPtrNew;
        }
        else
        {
            pVM->pgm.s.pRamRangesR3 = pNew;
            pVM->pgm.s.pRamRangesR0 = MMHyperCCToR0(pVM, pNew);
            pVM->pgm.s.pRamRangesGC = GCPtrNew;
        }
        pgmUnlock(pVM);
    }
    return rc;
}

#ifndef VBOX_WITH_NEW_PHYS_CODE

/**
 * Register a chunk of a the physical memory range with PGM. MM is responsible
 * for the toplevel things - allocation and locking - while PGM is taking
 * care of all the details and implements the physical address space virtualization.
 *
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
PGMR3DECL(int) PGMR3PhysRegisterChunk(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc)
{
    NOREF(pszDesc);

    /*
     * Validate input.
     * (Not so important because callers are only MMR3PhysRegister()
     *  and PGMR3HandlerPhysicalRegisterEx(), but anyway...)
     */
    Log(("PGMR3PhysRegisterChunk %08X %x bytes flags %x %s\n", GCPhys, cb, fFlags, pszDesc));

    Assert(paPages);
    Assert(pvRam);
    Assert(!(fFlags & ~0xfff));
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_P(pvRam, PAGE_SIZE) == pvRam);
    Assert(!(fFlags & ~(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_DYNAMIC_ALLOC)));
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(VM_IS_EMT(pVM));
    Assert(!(GCPhys & PGM_DYNAMIC_CHUNK_OFFSET_MASK));
    Assert(cb == PGM_DYNAMIC_CHUNK_SIZE);

    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    if (GCPhysLast < GCPhys)
    {
        AssertMsgFailed(("The range wraps! GCPhys=%VGp cb=%#x\n", GCPhys, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find existing range location.
     */
    PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (    off < pRam->cb
            &&  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
            break;

        pRam = CTXALLSUFF(pRam->pNext);
    }
    AssertReturn(pRam, VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS);

    unsigned off = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
    unsigned iPage = cb >> PAGE_SHIFT;
    if (paPages)
    {
        while (iPage-- > 0)
            pRam->aPages[off + iPage].HCPhys = (paPages[iPage].Phys & X86_PTE_PAE_PG_MASK) | fFlags;  /** @todo PAGE FLAGS */
    }
    off >>= (PGM_DYNAMIC_CHUNK_SHIFT - PAGE_SHIFT);
    pRam->pavHCChunkHC[off] = pvRam;

    /* Notify the recompiler. */
    REMR3NotifyPhysRamChunkRegister(pVM, GCPhys, PGM_DYNAMIC_CHUNK_SIZE, (RTHCUINTPTR)pvRam, fFlags);

    return VINF_SUCCESS;
}


/**
 * Allocate missing physical pages for an existing guest RAM range.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 */
PGMR3DECL(int) PGM3PhysGrowRange(PVM pVM, PCRTGCPHYS pGCPhys)
{
    RTGCPHYS GCPhys = *pGCPhys;

    /*
     * Walk range list.
     */
    pgmLock(pVM);

    PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (    off < pRam->cb
            &&  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
        {
            bool     fRangeExists = false;
            unsigned off = (GCPhys - pRam->GCPhys) >> PGM_DYNAMIC_CHUNK_SHIFT;

            /** @note A request made from another thread may end up in EMT after somebody else has already allocated the range. */
            if (pRam->pavHCChunkHC[off])
                fRangeExists = true;

            pgmUnlock(pVM);
            if (fRangeExists)
                return VINF_SUCCESS;
            return pgmr3PhysGrowRange(pVM, GCPhys);
        }

        pRam = CTXALLSUFF(pRam->pNext);
    }
    pgmUnlock(pVM);
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Allocate missing physical pages for an existing guest RAM range.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pRamRange       RAM range
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 */
int pgmr3PhysGrowRange(PVM pVM, RTGCPHYS GCPhys)
{
    void *pvRam;
    int   rc;

    /* We must execute this function in the EMT thread, otherwise we'll run into problems. */
    if (!VM_IS_EMT(pVM))
    {
        PVMREQ pReq;
        const RTGCPHYS GCPhysParam = GCPhys;

        AssertMsg(!PDMCritSectIsOwner(&pVM->pgm.s.CritSect), ("We own the PGM lock -> deadlock danger!!\n"));

        rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)PGM3PhysGrowRange, 2, pVM, &GCPhysParam);
        if (VBOX_SUCCESS(rc))
        {
            rc = pReq->iStatus;
            VMR3ReqFree(pReq);
        }
        return rc;
    }

    /* Round down to chunk boundary */
    GCPhys = GCPhys & PGM_DYNAMIC_CHUNK_BASE_MASK;

    STAM_COUNTER_INC(&pVM->pgm.s.StatDynRamGrow);
    STAM_COUNTER_ADD(&pVM->pgm.s.StatDynRamTotal, PGM_DYNAMIC_CHUNK_SIZE/(1024*1024));

    Log(("pgmr3PhysGrowRange: allocate chunk of size 0x%X at %VGp\n", PGM_DYNAMIC_CHUNK_SIZE, GCPhys));

    unsigned cPages = PGM_DYNAMIC_CHUNK_SIZE >> PAGE_SHIFT;

    for (;;)
    {
        rc = SUPPageAlloc(cPages, &pvRam);
        if (VBOX_SUCCESS(rc))
        {

            rc = MMR3PhysRegisterEx(pVM, pvRam, GCPhys, PGM_DYNAMIC_CHUNK_SIZE, 0, MM_PHYS_TYPE_DYNALLOC_CHUNK, "Main Memory");
            if (VBOX_SUCCESS(rc))
                return rc;

            SUPPageFree(pvRam, cPages);
        }

        VMSTATE enmVMState = VMR3GetState(pVM);
        if (enmVMState != VMSTATE_RUNNING)
        {
            AssertMsgFailed(("Out of memory while trying to allocate a guest RAM chunk at %VGp!\n", GCPhys));
            LogRel(("PGM: Out of memory while trying to allocate a guest RAM chunk at %VGp (VMstate=%s)!\n", GCPhys, VMR3GetStateName(enmVMState)));
            return rc;
        }

        LogRel(("pgmr3PhysGrowRange: out of memory. pause until the user resumes execution.\n"));

        /* Pause first, then inform Main. */
        rc = VMR3SuspendNoSave(pVM);
        AssertRC(rc);

        VMSetRuntimeError(pVM, false, "HostMemoryLow", "Unable to allocate and lock memory. The virtual machine will be paused. Please close applications to free up memory or close the VM.");

        /* Wait for resume event; will only return in that case. If the VM is stopped, the EMT thread will be destroyed. */
        rc = VMR3WaitForResume(pVM);

        /* Retry */
        LogRel(("pgmr3PhysGrowRange: VM execution resumed -> retry.\n"));
    }
}

#endif /* !VBOX_WITH_NEW_PHYS_CODE */


/**
 * Interface MMR3RomRegister() and MMR3PhysReserve calls to update the
 * flags of existing RAM ranges.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          The Or flags, MM_RAM_* \#defines.
 * @param   fMask           The and mask for the flags.
 */
PGMR3DECL(int) PGMR3PhysSetFlags(PVM pVM, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, unsigned fMask)
{
    Log(("PGMR3PhysSetFlags %08X %x %x %x\n", GCPhys, cb, fFlags, fMask));

    /*
     * Validate input.
     * (Not so important because caller is always MMR3RomRegister() and MMR3PhysReserve(), but anyway...)
     */
    Assert(!(fFlags & ~(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)));
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);

    /*
     * Lookup the range.
     */
    PPGMRAMRANGE    pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
    while (pRam && GCPhys > pRam->GCPhysLast)
        pRam = CTXALLSUFF(pRam->pNext);
    if (    !pRam
        ||  GCPhys > pRam->GCPhysLast
        ||  GCPhysLast < pRam->GCPhys)
    {
        AssertMsgFailed(("No RAM range for %VGp-%VGp\n", GCPhys, GCPhysLast));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Update the requested flags.
     */
    RTHCPHYS fFullMask = ~(RTHCPHYS)(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)
                         | fMask;
    unsigned iPageEnd = (GCPhysLast - pRam->GCPhys + 1) >> PAGE_SHIFT;
    unsigned iPage    = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
    for ( ; iPage < iPageEnd; iPage++)
        pRam->aPages[iPage].HCPhys = (pRam->aPages[iPage].HCPhys & fFullMask) | fFlags; /** @todo PAGE FLAGS */

    return VINF_SUCCESS;
}


/**
 * Sets the Address Gate 20 state.
 *
 * @param   pVM         VM handle.
 * @param   fEnable     True if the gate should be enabled.
 *                      False if the gate should be disabled.
 */
PGMDECL(void) PGMR3PhysSetA20(PVM pVM, bool fEnable)
{
    LogFlow(("PGMR3PhysSetA20 %d (was %d)\n", fEnable, pVM->pgm.s.fA20Enabled));
    if (pVM->pgm.s.fA20Enabled != (RTUINT)fEnable)
    {
        pVM->pgm.s.fA20Enabled = fEnable;
        pVM->pgm.s.GCPhysA20Mask = ~(RTGCPHYS)(!fEnable << 20);
        REMR3A20Set(pVM, fEnable);
    }
}


/**
 * Tree enumeration callback for dealing with age rollover.
 * It will perform a simple compression of the current age.
 */
static DECLCALLBACK(int) pgmR3PhysChunkAgeingRolloverCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    /* Age compression - ASSUMES iNow == 4. */
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)pNode;
    if (pChunk->iAge >= UINT32_C(0xffffff00))
        pChunk->iAge = 3;
    else if (pChunk->iAge >= UINT32_C(0xfffff000))
        pChunk->iAge = 2;
    else if (pChunk->iAge)
        pChunk->iAge = 1;
    else /* iAge = 0 */
        pChunk->iAge = 4;

    /* reinsert */
    PVM pVM = (PVM)pvUser;
    RTAvllU32Remove(&pVM->pgm.s.ChunkR3Map.pAgeTree, pChunk->AgeCore.Key);
    pChunk->AgeCore.Key = pChunk->iAge;
    RTAvllU32Insert(&pVM->pgm.s.ChunkR3Map.pAgeTree, &pChunk->AgeCore);
    return 0;
}


/**
 * Tree enumeration callback that updates the chunks that have
 * been used since the last
 */
static DECLCALLBACK(int) pgmR3PhysChunkAgeingCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)pNode;
    if (!pChunk->iAge)
    {
        PVM pVM = (PVM)pvUser;
        RTAvllU32Remove(&pVM->pgm.s.ChunkR3Map.pAgeTree, pChunk->AgeCore.Key);
        pChunk->AgeCore.Key = pChunk->iAge = pVM->pgm.s.ChunkR3Map.iNow;
        RTAvllU32Insert(&pVM->pgm.s.ChunkR3Map.pAgeTree, &pChunk->AgeCore);
    }

    return 0;
}


/**
 * Performs ageing of the ring-3 chunk mappings.
 *
 * @param   pVM         The VM handle.
 */
PGMR3DECL(void) PGMR3PhysChunkAgeing(PVM pVM)
{
    pVM->pgm.s.ChunkR3Map.AgeingCountdown = RT_MIN(pVM->pgm.s.ChunkR3Map.cMax / 4, 1024);
    pVM->pgm.s.ChunkR3Map.iNow++;
    if (pVM->pgm.s.ChunkR3Map.iNow == 0)
    {
        pVM->pgm.s.ChunkR3Map.iNow = 4;
        RTAvlU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pTree, true /*fFromLeft*/, pgmR3PhysChunkAgeingRolloverCallback, pVM);
    }
    else
        RTAvlU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pTree, true /*fFromLeft*/, pgmR3PhysChunkAgeingCallback, pVM);
}


/**
 * The structure passed in the pvUser argument of pgmR3PhysChunkUnmapCandidateCallback().
 */
typedef struct PGMR3PHYSCHUNKUNMAPCB
{
    PVM                 pVM;            /**< The VM handle. */
    PPGMCHUNKR3MAP      pChunk;         /**< The chunk to unmap. */
} PGMR3PHYSCHUNKUNMAPCB, *PPGMR3PHYSCHUNKUNMAPCB;


/**
 * Callback used to find the mapping that's been unused for
 * the longest time.
 */
static DECLCALLBACK(int) pgmR3PhysChunkUnmapCandidateCallback(PAVLLU32NODECORE pNode, void *pvUser)
{
    do
    {
        PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)((uint8_t *)pNode - RT_OFFSETOF(PGMCHUNKR3MAP, AgeCore));
        if (    pChunk->iAge
            &&  !pChunk->cRefs)
        {
            /*
             * Check that it's not in any of the TLBs.
             */
            PVM pVM = ((PPGMR3PHYSCHUNKUNMAPCB)pvUser)->pVM;
            for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
                if (pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].pChunk == pChunk)
                {
                    pChunk = NULL;
                    break;
                }
            if (pChunk)
                for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.PhysTlbHC.aEntries); i++)
                    if (pVM->pgm.s.PhysTlbHC.aEntries[i].pMap == pChunk)
                    {
                        pChunk = NULL;
                        break;
                    }
            if (pChunk)
            {
                ((PPGMR3PHYSCHUNKUNMAPCB)pvUser)->pChunk = pChunk;
                return 1; /* done */
            }
        }

        /* next with the same age - this version of the AVL API doesn't enumerate the list, so we have to do it. */
        pNode = pNode->pList;
    } while (pNode);
    return 0;
}


/**
 * Finds a good candidate for unmapping when the ring-3 mapping cache is full.
 *
 * The candidate will not be part of any TLBs, so no need to flush
 * anything afterwards.
 *
 * @returns Chunk id.
 * @param   pVM         The VM handle.
 */
static int32_t pgmR3PhysChunkFindUnmapCandidate(PVM pVM)
{
    /*
     * Do tree ageing first?
     */
    if (pVM->pgm.s.ChunkR3Map.AgeingCountdown-- == 0)
        PGMR3PhysChunkAgeing(pVM);

    /*
     * Enumerate the age tree starting with the left most node.
     */
    PGMR3PHYSCHUNKUNMAPCB Args;
    Args.pVM = pVM;
    Args.pChunk = NULL;
    if (RTAvllU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pAgeTree, true /*fFromLeft*/, pgmR3PhysChunkUnmapCandidateCallback, pVM))
        return Args.pChunk->Core.Key;
    return INT32_MAX;
}


/**
 * Maps the given chunk into the ring-3 mapping cache.
 *
 * This will call ring-0.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk in question.
 * @param   ppChunk     Where to store the chunk tracking structure.
 *
 * @remarks Called from within the PGM critical section.
 */
int pgmR3PhysChunkMap(PVM pVM, uint32_t idChunk, PPPGMCHUNKR3MAP ppChunk)
{
    int rc;
    /*
     * Allocate a new tracking structure first.
     */
#if 0 /* for later when we've got a separate mapping method for ring-0. */
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)MMR3HeapAlloc(pVM, MM_TAG_PGM_CHUNK_MAPPING, sizeof(*pChunk));
    AssertReturn(pChunk, VERR_NO_MEMORY);
#else
    PPGMCHUNKR3MAP pChunk;
    rc = MMHyperAlloc(pVM, sizeof(*pChunk), 0, MM_TAG_PGM_CHUNK_MAPPING, (void **)&pChunk);
    AssertRCReturn(rc, rc);
#endif
    pChunk->Core.Key = idChunk;
    pChunk->AgeCore.Key = pVM->pgm.s.ChunkR3Map.iNow;
    pChunk->iAge = 0;
    pChunk->cRefs = 0;
    pChunk->cPermRefs = 0;
    pChunk->pv = NULL;

    /*
     * Request the ring-0 part to map the chunk in question and if
     * necessary unmap another one to make space in the mapping cache.
     */
    GMMMAPUNMAPCHUNKREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.pvR3 = NULL;
    Req.idChunkMap = idChunk;
    Req.idChunkUnmap = INT32_MAX;
    if (pVM->pgm.s.ChunkR3Map.c >= pVM->pgm.s.ChunkR3Map.cMax)
        Req.idChunkUnmap = pgmR3PhysChunkFindUnmapCandidate(pVM);
    rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_MAP_UNMAP_CHUNK, 0, &Req.Hdr);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Update the tree.
         */
        /* insert the new one. */
        AssertPtr(Req.pvR3);
        pChunk->pv = Req.pvR3;
        bool fRc = RTAvlU32Insert(&pVM->pgm.s.ChunkR3Map.pTree, &pChunk->Core);
        AssertRelease(fRc);
        pVM->pgm.s.ChunkR3Map.c++;

        fRc = RTAvllU32Insert(&pVM->pgm.s.ChunkR3Map.pAgeTree, &pChunk->AgeCore);
        AssertRelease(fRc);

        /* remove the unmapped one. */
        if (Req.idChunkUnmap != INT32_MAX)
        {
            PPGMCHUNKR3MAP pUnmappedChunk = (PPGMCHUNKR3MAP)RTAvlU32Remove(&pVM->pgm.s.ChunkR3Map.pTree, Req.idChunkUnmap);
            AssertRelease(pUnmappedChunk);
            pUnmappedChunk->pv = NULL;
            pUnmappedChunk->Core.Key = UINT32_MAX;
#if 0 /* for later when we've got a separate mapping method for ring-0. */
            MMR3HeapFree(pUnmappedChunk);
#else
            MMHyperFree(pVM, pUnmappedChunk);
#endif
            pVM->pgm.s.ChunkR3Map.c--;
        }
    }
    else
    {
        AssertRC(rc);
#if 0 /* for later when we've got a separate mapping method for ring-0. */
        MMR3HeapFree(pChunk);
#else
        MMHyperFree(pVM, pChunk);
#endif
        pChunk = NULL;
    }

    *ppChunk = pChunk;
    return rc;
}


/**
 * For VMMCALLHOST_PGM_MAP_CHUNK, considered internal.
 *
 * @returns see pgmR3PhysChunkMap.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk to map.
 */
PDMR3DECL(int) PGMR3PhysChunkMap(PVM pVM, uint32_t idChunk)
{
    PPGMCHUNKR3MAP pChunk;
    return pgmR3PhysChunkMap(pVM, idChunk, &pChunk);
}


/**
 * Invalidates the TLB for the ring-3 mapping cache.
 *
 * @param   pVM         The VM handle.
 */
PGMR3DECL(void) PGMR3PhysChunkInvalidateTLB(PVM pVM)
{
    pgmLock(pVM);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
    {
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].idChunk = NIL_GMM_CHUNKID;
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].pChunk = NULL;
    }
    pgmUnlock(pVM);
}


/**
 * Response to VM_FF_PGM_NEED_HANDY_PAGES and VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is not cleared in this case.
 *
 * @param   pVM         The VM handle.
 */
PDMR3DECL(int) PGMR3PhysAllocateHandyPages(PVM pVM)
{
    pgmLock(pVM);
    int rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES, 0, NULL);
    if (rc == VERR_GMM_SEED_ME)
    {
        void *pvChunk;
        rc = SUPPageAlloc(GMM_CHUNK_SIZE >> PAGE_SHIFT, &pvChunk);
        if (VBOX_SUCCESS(rc))
            rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_SEED_CHUNK, (uintptr_t)pvChunk, NULL);
        if (VBOX_FAILURE(rc))
        {
            LogRel(("PGM: GMM Seeding failed, rc=%Vrc\n", rc));
            rc = VINF_EM_NO_MEMORY;
        }
    }
    pgmUnlock(pVM);
    Assert(rc == VINF_SUCCESS || rc == VINF_EM_NO_MEMORY);
    return rc;
}

