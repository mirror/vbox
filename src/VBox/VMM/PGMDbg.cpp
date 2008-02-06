/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor - Debugger & Debugging APIs.
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
#include <VBox/stam.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>

/** The max needle size that we will bother searching for
 * This must not be more than half a page! */
#define MAX_NEEDLE_SIZE     256


/**
 * Converts a HC pointer to a GC physical address.
 *
 * Only for the debugger.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 *
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPtr2GCPhys(PVM pVM, RTHCPTR HCPtr, PRTGCPHYS pGCPhys)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
    *pGCPhys = NIL_RTGCPHYS;
    return VERR_NOT_IMPLEMENTED;

#else
    for (PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXALLSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                    {
                        *pGCPhys = pRam->GCPhys + iChunk*PGM_DYNAMIC_CHUNK_SIZE + off;
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;
            if (off < pRam->cb)
            {
                *pGCPhys = pRam->GCPhys + off;
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_POINTER;
#endif
}


/**
 * Converts a HC pointer to a GC physical address.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pHCPhys is set.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical page but has no physical backing.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 *
 * @param   pVM     The VM handle.
 * @param   HCPtr   The HC pointer to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPtr2HCPhys(PVM pVM, RTHCPTR HCPtr, PRTHCPHYS pHCPhys)
{
#ifdef VBOX_WITH_NEW_PHYS_CODE
    *pHCPhys = NIL_RTHCPHYS;
    return VERR_NOT_IMPLEMENTED;

#else
    for (PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXALLSUFF(pRam->pNext))
    {
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (CTXSUFF(pRam->pavHCChunk)[iChunk])
                {
                    RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)CTXSUFF(pRam->pavHCChunk)[iChunk];
                    if (off < PGM_DYNAMIC_CHUNK_SIZE)
                    {
                        PPGMPAGE pPage = &pRam->aPages[off >> PAGE_SHIFT];
                        if (PGM_PAGE_IS_RESERVED(pPage))
                            return VERR_PGM_PHYS_PAGE_RESERVED;
                        *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage)
                                 | (off & PAGE_OFFSET_MASK);
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else if (pRam->pvHC)
        {
            RTHCUINTPTR off = (RTHCUINTPTR)HCPtr - (RTHCUINTPTR)pRam->pvHC;
            if (off < pRam->cb)
            {
                PPGMPAGE pPage = &pRam->aPages[off >> PAGE_SHIFT];
                if (PGM_PAGE_IS_RESERVED(pPage))
                    return VERR_PGM_PHYS_PAGE_RESERVED;
                *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage)
                         | (off & PAGE_OFFSET_MASK);
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_POINTER;
#endif
}


/**
 * Converts a HC physical address to a GC physical address.
 *
 * Only for the debugger.
 *
 * @returns VBox status code
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the HC physical address is not within the GC physical memory.
 *
 * @param   pVM     The VM handle.
 * @param   HCPhys  The HC physical address to convert.
 * @param   pGCPhys Where to store the GC physical address on success.
 */
PGMR3DECL(int) PGMR3DbgHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys)
{
    /*
     * Validate and adjust the input a bit.
     */
    if (HCPhys == NIL_RTHCPHYS)
        return VERR_INVALID_POINTER;
    unsigned off = HCPhys & PAGE_OFFSET_MASK;
    HCPhys &= X86_PTE_PAE_PG_MASK;
    if (HCPhys == 0)
        return VERR_INVALID_POINTER;

    for (PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXALLSUFF(pRam->pNext))
    {
        uint32_t iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (    PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys
                &&  !PGM_PAGE_IS_RESERVED(&pRam->aPages[iPage]))
            {
                *pGCPhys = pRam->GCPhys + (iPage << PAGE_SHIFT) + off;
                return VINF_SUCCESS;
            }
    }
    return VERR_INVALID_POINTER;
}


/**
 * Scans a page for a byte string, keeping track of potential
 * cross page matches.
 *
 * @returns true and *poff on match.
 *          false on mismatch.
 * @param   pbPage          Pointer to the current page.
 * @param   poff            Input: The offset into the page.
 *                          Output: The page offset of the match on success.
 * @param   cb              The number of bytes to search, starting of *poff.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string.
 * @param   pabPrev         The buffer that keeps track of a partial match that we
 *                          bring over from the previous page. This buffer must be
 *                          at least cbNeedle - 1 big.
 * @param   pcbPrev         Input: The number of partial matching bytes from the previous page.
 *                          Output: The number of partial matching bytes from this page.
 *                          Initialize to 0 before the first call to this function.
 */
static bool pgmR3DbgScanPage(const uint8_t *pbPage, int32_t *poff, uint32_t cb,
                             const uint8_t *pabNeedle, size_t cbNeedle,
                             uint8_t *pabPrev, size_t *pcbPrev)
{
    /*
     * Try complete any partial match from the previous page.
     */
    if (*pcbPrev > 0)
    {
        size_t cbPrev = *pcbPrev;
        Assert(!*poff);
        Assert(cbPrev < cbNeedle);
        if (!memcmp(pbPage, pabNeedle + cbPrev, cbNeedle - cbPrev))
        {
            if (cbNeedle - cbPrev > cb)
                return false;
            *poff = -(int32_t)cbPrev;
            return true;
        }

        /* check out the remainder of the previous page. */
        const uint8_t *pb = pabPrev;
        while (cbPrev-- > 0)
        {
            pb = (const uint8_t *)memchr(pb + 1, *pabNeedle, cbPrev);
            if (!pb)
                break;
            cbPrev = *pcbPrev - (pb - pabPrev);
            if (    !memcmp(pb + 1, &pabNeedle[1], cbPrev - 1)
                &&  !memcmp(pbPage, pabNeedle + cbPrev, cbNeedle - cbPrev))
            {
                if (cbNeedle - cbPrev > cb)
                    return false;
                *poff = -(int32_t)cbPrev;
                return true;
            }
        }

        *pcbPrev = 0;
    }

    /*
     * Match the body of the page.
     */
    const uint8_t *pb = pbPage + *poff;
    const uint8_t *pbEnd = pb + cb;
    for (;;)
    {
        pb = (const uint8_t *)memchr(pb, *pabNeedle, cb);
        if (!pb)
            break;
        cb = pbEnd - pb;
        if (cb >= cbNeedle)
        {
            /* match? */
            if (!memcmp(pb + 1, &pabNeedle[1], cbNeedle - 1))
            {
                *poff = pb - pbPage;
                return true;
            }
        }
        else
        {
            /* paritial match at the end of the page? */
            if (!memcmp(pb + 1, &pabNeedle[1], cb - 1))
            {
                /* We're copying one byte more that we really need here, but wtf. */
                memcpy(pabPrev, pb, cb);
                *pcbPrev = cb;
                return false;
            }
        }

        /* no match, skip a byte ahead. */
        if (cb <= 1)
            break;
        pb++;
        cb--;
    }

    return false;
}


/**
 * Scans guest physical memory for a byte string.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS and *pGCPtrHit on success.
 * @retval  VERR_DBGF_MEM_NOT_FOUND if not found.
 * @retval  VERR_INVALID_POINTER if any of the pointer arguments are invalid.
 * @retval  VERR_INVALID_ARGUMENT if any other arguments are invalid.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPhys          Where to start searching.
 * @param   cbRange         The number of bytes to search.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string. Max 256 bytes.
 * @param   pGCPhysHit      Where to store the address of the first occurence on success.
 */
PDMR3DECL(int) PGMR3DbgScanPhysical(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCPHYS pGCPhysHit)
{
    /*
     * Validate and adjust the input a bit.
     */
    if (!VALID_PTR(pGCPhysHit))
        return VERR_INVALID_POINTER;
    *pGCPhysHit = NIL_RTGCPHYS;

    if (    !VALID_PTR(pabNeedle)
        ||  GCPhys == NIL_RTGCPHYS)
        return VERR_INVALID_POINTER;
    if (!cbNeedle)
        return VERR_INVALID_PARAMETER;
    if (cbNeedle > MAX_NEEDLE_SIZE)
        return VERR_INVALID_PARAMETER;

    if (!cbRange)
        return VERR_DBGF_MEM_NOT_FOUND;
    if (GCPhys + cbNeedle - 1 < GCPhys)
        return VERR_DBGF_MEM_NOT_FOUND;

    const RTGCPHYS GCPhysLast = GCPhys + cbRange - 1 >= GCPhys
                              ? GCPhys + cbRange - 1
                              : ~(RTGCPHYS)0;

    /*
     * Search the memory - ignore MMIO and zero pages, also don't
     * bother to match across ranges.
     */
    for (PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXALLSUFF(pRam->pNext))
    {
        /*
         * If the search range starts prior to the current ram range record,
         * adjust the search range and possibly conclude the search.
         */
        RTGCPHYS off;
        if (GCPhys < pRam->GCPhys)
        {
            if (GCPhysLast < pRam->GCPhys)
                break;
            GCPhys = pRam->GCPhys;
            off = 0;
        }
        else
            off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            /*
             * Iterate the relevant pages.
             */
            uint8_t abPrev[MAX_NEEDLE_SIZE];
            size_t  cbPrev = 0;
            const uint32_t cPages = pRam->cb >> PAGE_SHIFT;
            for (uint32_t iPage = off >> PAGE_SHIFT; iPage < cPages; iPage++)
            {
                PPGMPAGE pPage = &pRam->aPages[iPage];
                if (    /** @todo !PGM_PAGE_IS_ZERO(pPage)
                    &&*/  !PGM_PAGE_IS_MMIO(pPage))
                {
                    void const *pvPage;
                    PGMPAGEMAPLOCK Lock;
                    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK, &pvPage, &Lock);
                    if (RT_SUCCESS(rc))
                    {
                        int32_t  offPage = (GCPhys & PAGE_OFFSET_MASK);
                        uint32_t cbSearch = (GCPhys ^ GCPhysLast) & ~(RTGCPHYS)PAGE_OFFSET_MASK
                                          ? PAGE_SIZE                           - (uint32_t)offPage
                                          : (GCPhysLast & PAGE_OFFSET_MASK) + 1 - (uint32_t)offPage;
                        bool fRc = pgmR3DbgScanPage((uint8_t const *)pvPage, &offPage, cbSearch,
                                                    pabNeedle, cbNeedle, &abPrev[0], &cbPrev);
                        PGMPhysReleasePageMappingLock(pVM, &Lock);
                        if (fRc)
                        {
                            *pGCPhysHit = (GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK) + offPage;
                            return VINF_SUCCESS;
                        }
                    }
                    else
                        cbPrev = 0; /* ignore error. */
                }
                else
                    cbPrev = 0;

                /* advance to the the next page. */
                GCPhys |= PAGE_OFFSET_MASK;
                if (GCPhys++ >= GCPhysLast)
                    return VERR_DBGF_MEM_NOT_FOUND;
            }
        }
    }
    return VERR_DBGF_MEM_NOT_FOUND;
}


/**
 * Scans (guest) virtual memory for a byte string.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS and *pGCPtrHit on success.
 * @retval  VERR_DBGF_MEM_NOT_FOUND if not found.
 * @retval  VERR_INVALID_POINTER if any of the pointer arguments are invalid.
 * @retval  VERR_INVALID_ARGUMENT if any other arguments are invalid.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   GCPtr           Where to start searching.
 * @param   cbRange         The number of bytes to search. Max 256 bytes.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string.
 * @param   pGCPtrHit       Where to store the address of the first occurence on success.
 */
PDMR3DECL(int) PGMR3DbgScanVirtual(PVM pVM, RTGCUINTPTR GCPtr, RTGCUINTPTR cbRange, const uint8_t *pabNeedle, size_t cbNeedle, PRTGCUINTPTR pGCPtrHit)
{
    /*
     * Validate and adjust the input a bit.
     */
    if (!VALID_PTR(pGCPtrHit))
        return VERR_INVALID_POINTER;
    *pGCPtrHit = 0;

    if (!VALID_PTR(pabNeedle))
        return VERR_INVALID_POINTER;
    if (!cbNeedle)
        return VERR_INVALID_PARAMETER;
    if (cbNeedle > MAX_NEEDLE_SIZE)
        return VERR_INVALID_PARAMETER;

    if (!cbRange)
        return VERR_DBGF_MEM_NOT_FOUND;
    if (GCPtr + cbNeedle - 1 < GCPtr)
        return VERR_DBGF_MEM_NOT_FOUND;

    /*
     * Search the memory - ignore MMIO, zero and not-present pages.
     */
    uint8_t             abPrev[MAX_NEEDLE_SIZE];
    size_t              cbPrev = 0;
    const RTGCUINTPTR   GCPtrLast = GCPtr + cbRange - 1 >= GCPtr
                                  ? GCPtr + cbRange - 1
                                  : ~(RTGCUINTPTR)0;
    RTGCUINTPTR         cPages = (((GCPtrLast - GCPtr) + (GCPtr & PAGE_OFFSET_MASK)) >> PAGE_SHIFT) + 1;
    while (cPages-- > 0)
    {
        RTGCPHYS GCPhys;
        int rc = PGMPhysGCPtr2GCPhys(pVM, GCPtr, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
            if (    pPage
///@todo                 &&  !PGM_PAGE_IS_ZERO(pPage)
                &&  !PGM_PAGE_IS_MMIO(pPage))
            {
                void const *pvPage;
                PGMPAGEMAPLOCK Lock;
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys & ~(RTGCUINTPTR)PAGE_OFFSET_MASK, &pvPage, &Lock);
                if (RT_SUCCESS(rc))
                {
                    int32_t  offPage = (GCPtr & PAGE_OFFSET_MASK);
                    uint32_t cbSearch = cPages > 0
                                      ? PAGE_SIZE                          - (uint32_t)offPage
                                      : (GCPtrLast & PAGE_OFFSET_MASK) + 1 - (uint32_t)offPage;
                    bool fRc = pgmR3DbgScanPage((uint8_t const *)pvPage, &offPage, cbSearch,
                                                pabNeedle, cbNeedle, &abPrev[0], &cbPrev);
                    PGMPhysReleasePageMappingLock(pVM, &Lock);
                    if (fRc)
                    {
                        *pGCPtrHit = (GCPtr & ~(RTGCUINTPTR)PAGE_OFFSET_MASK) + offPage;
                        return VINF_SUCCESS;
                    }
                }
                else
                    cbPrev = 0; /* ignore error. */
            }
            else
                cbPrev = 0;
        }
        else
            cbPrev = 0; /* ignore error. */

        /* advance to the the next page. */
        GCPtr |= PAGE_OFFSET_MASK;
        GCPtr++;
    }
    return VERR_DBGF_MEM_NOT_FOUND;
}

