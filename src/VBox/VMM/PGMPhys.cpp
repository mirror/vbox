/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
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
 * PGMR3PhysReadByte/Word/Dword
 * PGMR3PhysWriteByte/Word/Dword
 */
/** @todo rename and add U64. */

#define PGMPHYSFN_READNAME  PGMR3PhysReadByte
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteByte
#define PGMPHYS_DATASIZE    1
#define PGMPHYS_DATATYPE    uint8_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadWord
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteWord
#define PGMPHYS_DATASIZE    2
#define PGMPHYS_DATATYPE    uint16_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadDword
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteDword
#define PGMPHYS_DATASIZE    4
#define PGMPHYS_DATATYPE    uint32_t
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
 * Links a new RAM range into the list.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pRam        Pointer to the new list entry.
 * @param   pPrev       Pointer to the previous list entry. If NULL, insert as head.
 */
static void pgmR3PhysUnlinkRamRange(PVM pVM, PPGMRAMRANGE pRam, PPGMRAMRANGE pPrev)
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
        pVM->pgm.s.pRamRangesR3 = pNext;
        pVM->pgm.s.pRamRangesR0 = pNext ? MMHyperCCToR0(pVM, pNext) : NIL_RTR0PTR;
        pVM->pgm.s.pRamRangesGC = pNext ? MMHyperCCToGC(pVM, pNext) : NIL_RTGCPTR;
    }

    pgmUnlock(pVM);
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
        if (    GCPhys     <= pRam->GCPhysLast
            &&  GCPhysLast >= pRam->GCPhys)
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
    AssertLogRelMsgRCReturn(rc, ("cbRamRange=%zd\n", cbRamRange), rc);

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
    {
#ifdef VBOX_WITH_NEW_PHYS_CODE
        pNew->aPages[iPage].HCPhys = pVM->pgm.s.HCPhysZeroPg;
#else
        pNew->aPages[iPage].HCPhys = 0;
#endif
        pNew->aPages[iPage].fWrittenTo = 0;
        pNew->aPages[iPage].fSomethingElse = 0;
        pNew->aPages[iPage].u29B = 0;
        PGM_PAGE_SET_TYPE(&pNew->aPages[iPage],   PGMPAGETYPE_RAM);
        PGM_PAGE_SET_STATE(&pNew->aPages[iPage],  PGM_PAGE_STATE_ZERO);
        PGM_PAGE_SET_PAGEID(&pNew->aPages[iPage], NIL_GMM_PAGEID);
    }

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
    rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMROMRANGE, aPages[cPages]), 0, MM_TAG_PGM_PHYS, (void **)pRomNew);
    if (RT_SUCCESS(rc))
    {
        PPGMRAMRANGE pRamNew = NULL;
        if (!fRamExists)
            rc = MMHyperAlloc(pVM, RT_OFFSETOF(PGMRAMRANGE, aPages[cPages]), sizeof(PGMPAGE), MM_TAG_PGM_PHYS, (void **)pRamNew);
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
                    pPage->fWrittenTo = 0;
                    pPage->fSomethingElse = 0;
                    pPage->u29B = 0;
                    PGM_PAGE_SET_TYPE(pPage,   PGMPAGETYPE_ROM);
                    PGM_PAGE_SET_HCPHYS(pPage, pReq->aPages[iPage].HCPhysGCPhys);
                    PGM_PAGE_SET_STATE(pPage,  PGM_PAGE_STATE_ALLOCATED);
                    PGM_PAGE_SET_PAGEID(pPage, pReq->aPages[iPage].idPage);

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

                        pPage->Shadow.HCPhys = 0;
                        pPage->Shadow.fWrittenTo = 0;
                        pPage->Shadow.fSomethingElse = 0;
                        pPage->Shadow.u29B = 0;
                        PGM_PAGE_SET_TYPE(  &pPage->Shadow, PGMPAGETYPE_ROM_SHADOW);
                        PGM_PAGE_SET_STATE( &pPage->Shadow, PGM_PAGE_STATE_ZERO);
                        PGM_PAGE_SET_PAGEID(&pPage->Shadow, pReq->aPages[iPage].idPage);

                        pRomNew->aPages[iPage].enmProt = PGMROMPROT_READ_ROM_WRITE_IGNORE;
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

            pgmR3PhysUnlinkRamRange(pVM, pRamNew, pRamPrev);
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
                        {
                            PGM_PAGE_SET_STATE( &pRom->aPages[iPage].Shadow, PGM_PAGE_STATE_ZERO);
                            PGM_PAGE_SET_HCPHYS(&pRom->aPages[iPage].Shadow, pVM->pgm.s.HCPhysZeroPg);
                            PGM_PAGE_SET_PAGEID(&pRom->aPages[iPage].Shadow, NIL_GMM_PAGEID);
                            pRom->aPages[iPage].Shadow.fWrittenTo = false;
                            iReqPage++;
                        }
                }
            }
            else
            {
                /* clear all the pages. */
                pgmLock(pVM);
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
                    memset(pvDstPage, 0, PAGE_SIZE);
                }
                pgmUnlock(pVM);
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
                    /** @todo sync the volatile flags (handlers) when these have been moved out of HCPhys. */
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
                pNew->aPages[iPage].HCPhys = (paPages[iPage].Phys & X86_PTE_PAE_PG_MASK) | fFlags; /** @todo PAGE FLAGS */
                pNew->aPages[iPage].fWrittenTo = 0;
                pNew->aPages[iPage].fSomethingElse = 0;
                pNew->aPages[iPage].u29B = 0;
                PGM_PAGE_SET_PAGEID(&pNew->aPages[iPage],   NIL_GMM_PAGEID);
                PGM_PAGE_SET_TYPE(&pNew->aPages[iPage],     fFlags & MM_RAM_FLAGS_MMIO2 ? PGMPAGETYPE_MMIO2 : PGMPAGETYPE_RAM);
                PGM_PAGE_SET_STATE(&pNew->aPages[iPage],    PGM_PAGE_STATE_ALLOCATED);
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
                pNew->aPages[iPage].HCPhys = fFlags; /** @todo PAGE FLAGS */
                pNew->aPages[iPage].fWrittenTo = 0;
                pNew->aPages[iPage].fSomethingElse = 0;
                pNew->aPages[iPage].u29B = 0;
                PGM_PAGE_SET_PAGEID(&pNew->aPages[iPage],   NIL_GMM_PAGEID);
                PGM_PAGE_SET_TYPE(&pNew->aPages[iPage],     PGMPAGETYPE_RAM);
                PGM_PAGE_SET_STATE(&pNew->aPages[iPage],    PGM_PAGE_STATE_ZERO);
            }
        }
        else
        {
            Assert(fFlags == (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO));
            RTHCPHYS HCPhysDummyPage = (MMR3PageDummyHCPhys(pVM) & X86_PTE_PAE_PG_MASK) | fFlags; /** @todo PAGE FLAGS */
            while (iPage-- > 0)
            {
                pNew->aPages[iPage].HCPhys = HCPhysDummyPage; /** @todo PAGE FLAGS */
                pNew->aPages[iPage].fWrittenTo = 0;
                pNew->aPages[iPage].fSomethingElse = 0;
                pNew->aPages[iPage].u29B = 0;
                PGM_PAGE_SET_PAGEID(&pNew->aPages[iPage],   NIL_GMM_PAGEID);
                PGM_PAGE_SET_TYPE(&pNew->aPages[iPage],     PGMPAGETYPE_MMIO);
                PGM_PAGE_SET_STATE(&pNew->aPages[iPage],    PGM_PAGE_STATE_ZERO);
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
PGMR3DECL(int) PGM3PhysGrowRange(PVM pVM, RTGCPHYS GCPhys)
{
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

        AssertMsg(!PDMCritSectIsOwner(&pVM->pgm.s.CritSect), ("We own the PGM lock -> deadlock danger!!\n"));

        rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)PGM3PhysGrowRange, 2, pVM, GCPhys);
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
PGMR3DECL(int) PGMR3PhysRelocate(PVM pVM, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, size_t cb)
{
    /*
     * Validate input.
     * (Not so important because callers are only MMR3PhysRelocate(),
     *  but anyway...)
     */
    Log(("PGMR3PhysRelocate Old %VGp New %VGp (%#x bytes)\n", GCPhysOld, GCPhysNew, cb));

    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_T(GCPhysOld, PAGE_SIZE, RTGCPHYS) == GCPhysOld);
    Assert(RT_ALIGN_T(GCPhysNew, PAGE_SIZE, RTGCPHYS) == GCPhysNew);
    RTGCPHYS GCPhysLast;
    GCPhysLast = GCPhysOld + (cb - 1);
    if (GCPhysLast < GCPhysOld)
    {
        AssertMsgFailed(("The old range wraps! GCPhys=%VGp cb=%#x\n", GCPhysOld, cb));
        return VERR_INVALID_PARAMETER;
    }
    GCPhysLast = GCPhysNew + (cb - 1);
    if (GCPhysLast < GCPhysNew)
    {
        AssertMsgFailed(("The new range wraps! GCPhys=%VGp cb=%#x\n", GCPhysNew, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find and remove old range location.
     */
    pgmLock(pVM);
    PPGMRAMRANGE    pPrev = NULL;
    PPGMRAMRANGE    pCur = pVM->pgm.s.pRamRangesR3;
    while (pCur)
    {
        if (pCur->GCPhys == GCPhysOld && pCur->cb == cb)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }
    if (pPrev)
    {
        pPrev->pNextR3 = pCur->pNextR3;
        pPrev->pNextR0 = pCur->pNextR0;
        pPrev->pNextGC = pCur->pNextGC;
    }
    else
    {
        pVM->pgm.s.pRamRangesR3 = pCur->pNextR3;
        pVM->pgm.s.pRamRangesR0 = pCur->pNextR0;
        pVM->pgm.s.pRamRangesGC = pCur->pNextGC;
    }

    /*
     * Update the range.
     */
    pCur->GCPhys    = GCPhysNew;
    pCur->GCPhysLast= GCPhysLast;
    PPGMRAMRANGE    pNew = pCur;

    /*
     * Find range location and check for conflicts.
     */
    pPrev = NULL;
    pCur = pVM->pgm.s.pRamRangesR3;
    while (pCur)
    {
        if (GCPhysNew <= pCur->GCPhysLast && GCPhysLast >= pCur->GCPhys)
        {
            AssertMsgFailed(("Conflict! This cannot happen!\n"));
            pgmUnlock(pVM);
            return VERR_PGM_RAM_CONFLICT;
        }
        if (GCPhysLast < pCur->GCPhys)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextR3;
    }

    /*
     * Reinsert the RAM range.
     */
    pNew->pNextR3 = pCur;
    pNew->pNextR0 = pCur ? MMHyperCCToR0(pVM, pCur) : 0;
    pNew->pNextGC = pCur ? MMHyperCCToGC(pVM, pCur) : 0;
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
    return VINF_SUCCESS;
}


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

