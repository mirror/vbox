/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor - Debugger & Debugging APIs.
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
#include <VBox/pgm.h>
#include <VBox/stam.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include "PGMInline.h"
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The max needle size that we will bother searching for
 * This must not be more than half a page! */
#define MAX_NEEDLE_SIZE     256


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * State structure for the paging hierarchy dumpers.
 */
typedef struct PGMR3DUMPHIERARCHYSTATE
{
    /** The VM handle. */
    PVM             pVM;
    /** Output helpers. */
    PCDBGFINFOHLP   pHlp;
    /** Set if PSE, PAE or long mode is enabled. */
    bool            fPse;
    /** Set if PAE or long mode is enabled. */
    bool            fPae;
    /** Set if long mode is enabled. */
    bool            fLme;
    /** The number or chars the address needs. */
    uint8_t         cchAddress;
    bool            afReserved[4];
    /** The current address.  */
    uint64_t        u64Address;
    /** The last address to dump structures for. */
    uint64_t        u64FirstAddress;
    /** The last address to dump structures for. */
    uint64_t        u64LastAddress;
    /** The number of leaf entries that we've printed. */
    uint64_t        cLeafs;
} PGMR3DUMPHIERARCHYSTATE;
/** Pointer to the paging hierarchy dumper state. */
typedef PGMR3DUMPHIERARCHYSTATE *PPGMR3DUMPHIERARCHYSTATE;



/**
 * Converts a R3 pointer to a GC physical address.
 *
 * Only for the debugger.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pGCPhys is set.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 *
 * @param   pVM         The VM handle.
 * @param   R3Ptr       The R3 pointer to convert.
 * @param   pGCPhys     Where to store the GC physical address on success.
 */
VMMR3DECL(int) PGMR3DbgR3Ptr2GCPhys(PVM pVM, RTR3PTR R3Ptr, PRTGCPHYS pGCPhys)
{
    *pGCPhys = NIL_RTGCPHYS;
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Converts a R3 pointer to a HC physical address.
 *
 * Only for the debugger.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, *pHCPhys is set.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical page but has no physical backing.
 * @retval  VERR_INVALID_POINTER if the pointer is not within the GC physical memory.
 *
 * @param   pVM         The VM handle.
 * @param   R3Ptr       The R3 pointer to convert.
 * @param   pHCPhys     Where to store the HC physical address on success.
 */
VMMR3DECL(int) PGMR3DbgR3Ptr2HCPhys(PVM pVM, RTR3PTR R3Ptr, PRTHCPHYS pHCPhys)
{
    *pHCPhys = NIL_RTHCPHYS;
    return VERR_NOT_IMPLEMENTED;
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
VMMR3DECL(int) PGMR3DbgHCPhys2GCPhys(PVM pVM, RTHCPHYS HCPhys, PRTGCPHYS pGCPhys)
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

    for (PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        uint32_t iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
            if (PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys)
            {
                *pGCPhys = pRam->GCPhys + (iPage << PAGE_SHIFT) + off;
                return VINF_SUCCESS;
            }
    }
    return VERR_INVALID_POINTER;
}


/**
 * Read physical memory API for the debugger, similar to
 * PGMPhysSimpleReadGCPhys.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   pvDst       Where to store what's read.
 * @param   GCPhysDst   Where to start reading from.
 * @param   cb          The number of bytes to attempt reading.
 * @param   fFlags      Flags, MBZ.
 * @param   pcbRead     For store the actual number of bytes read, pass NULL if
 *                      partial reads are unwanted.
 * @todo    Unused?
 */
VMMR3DECL(int) PGMR3DbgReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb, uint32_t fFlags, size_t *pcbRead)
{
    /* validate */
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* try simple first. */
    int rc = PGMPhysSimpleReadGCPhys(pVM, pvDst, GCPhysSrc, cb);
    if (RT_SUCCESS(rc) || !pcbRead)
        return rc;

    /* partial read that failed, chop it up in pages. */
    *pcbRead = 0;
    size_t const cbReq = cb;
    rc = VINF_SUCCESS;
    while (cb > 0)
    {
        size_t cbChunk = PAGE_SIZE;
        cbChunk -= GCPhysSrc & PAGE_OFFSET_MASK;
        if (cbChunk > cb)
            cbChunk = cb;

        rc = PGMPhysSimpleReadGCPhys(pVM, pvDst, GCPhysSrc, cbChunk);

        /* advance */
        if (RT_FAILURE(rc))
            break;
        *pcbRead  += cbChunk;
        cb        -= cbChunk;
        GCPhysSrc += cbChunk;
        pvDst = (uint8_t *)pvDst + cbChunk;
    }

    return *pcbRead && RT_FAILURE(rc) ? -rc : rc;
}


/**
 * Write physical memory API for the debugger, similar to
 * PGMPhysSimpleWriteGCPhys.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   GCPhysDst   Where to start writing.
 * @param   pvSrc       What to write.
 * @param   cb          The number of bytes to attempt writing.
 * @param   fFlags      Flags, MBZ.
 * @param   pcbWritten  For store the actual number of bytes written, pass NULL
 *                      if partial writes are unwanted.
 * @todo    Unused?
 */
VMMR3DECL(int) PGMR3DbgWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb, uint32_t fFlags, size_t *pcbWritten)
{
    /* validate */
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* try simple first. */
    int rc = PGMPhysSimpleWriteGCPhys(pVM, GCPhysDst, pvSrc, cb);
    if (RT_SUCCESS(rc) || !pcbWritten)
        return rc;

    /* partial write that failed, chop it up in pages. */
    *pcbWritten = 0;
    rc = VINF_SUCCESS;
    while (cb > 0)
    {
        size_t cbChunk = PAGE_SIZE;
        cbChunk -= GCPhysDst & PAGE_OFFSET_MASK;
        if (cbChunk > cb)
            cbChunk = cb;

        rc = PGMPhysSimpleWriteGCPhys(pVM, GCPhysDst, pvSrc, cbChunk);

        /* advance */
        if (RT_FAILURE(rc))
            break;
        *pcbWritten += cbChunk;
        cb          -= cbChunk;
        GCPhysDst   += cbChunk;
        pvSrc = (uint8_t const *)pvSrc + cbChunk;
    }

    return *pcbWritten && RT_FAILURE(rc) ? -rc : rc;

}


/**
 * Read virtual memory API for the debugger, similar to PGMPhysSimpleReadGCPtr.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   pvDst       Where to store what's read.
 * @param   GCPtrDst    Where to start reading from.
 * @param   cb          The number of bytes to attempt reading.
 * @param   fFlags      Flags, MBZ.
 * @param   pcbRead     For store the actual number of bytes read, pass NULL if
 *                      partial reads are unwanted.
 * @todo    Unused?
 */
VMMR3DECL(int) PGMR3DbgReadGCPtr(PVM pVM, void *pvDst, RTGCPTR GCPtrSrc, size_t cb, uint32_t fFlags, size_t *pcbRead)
{
    /* validate */
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* @todo SMP support! */
    PVMCPU pVCpu = &pVM->aCpus[0];

/** @todo deal with HMA */
    /* try simple first. */
    int rc = PGMPhysSimpleReadGCPtr(pVCpu, pvDst, GCPtrSrc, cb);
    if (RT_SUCCESS(rc) || !pcbRead)
        return rc;

    /* partial read that failed, chop it up in pages. */
    *pcbRead = 0;
    rc = VINF_SUCCESS;
    while (cb > 0)
    {
        size_t cbChunk = PAGE_SIZE;
        cbChunk -= GCPtrSrc & PAGE_OFFSET_MASK;
        if (cbChunk > cb)
            cbChunk = cb;

        rc = PGMPhysSimpleReadGCPtr(pVCpu, pvDst, GCPtrSrc, cbChunk);

        /* advance */
        if (RT_FAILURE(rc))
            break;
        *pcbRead  += cbChunk;
        cb        -= cbChunk;
        GCPtrSrc  += cbChunk;
        pvDst = (uint8_t *)pvDst + cbChunk;
    }

    return *pcbRead && RT_FAILURE(rc) ? -rc : rc;

}


/**
 * Write virtual memory API for the debugger, similar to
 * PGMPhysSimpleWriteGCPtr.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM handle.
 * @param   GCPtrDst    Where to start writing.
 * @param   pvSrc       What to write.
 * @param   cb          The number of bytes to attempt writing.
 * @param   fFlags      Flags, MBZ.
 * @param   pcbWritten  For store the actual number of bytes written, pass NULL
 *                      if partial writes are unwanted.
 * @todo    Unused?
 */
VMMR3DECL(int) PGMR3DbgWriteGCPtr(PVM pVM, RTGCPTR GCPtrDst, void const *pvSrc, size_t cb, uint32_t fFlags, size_t *pcbWritten)
{
    /* validate */
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* @todo SMP support! */
    PVMCPU pVCpu = &pVM->aCpus[0];

/** @todo deal with HMA */
    /* try simple first. */
    int rc = PGMPhysSimpleWriteGCPtr(pVCpu, GCPtrDst, pvSrc, cb);
    if (RT_SUCCESS(rc) || !pcbWritten)
        return rc;

    /* partial write that failed, chop it up in pages. */
    *pcbWritten = 0;
    rc = VINF_SUCCESS;
    while (cb > 0)
    {
        size_t cbChunk = PAGE_SIZE;
        cbChunk -= GCPtrDst & PAGE_OFFSET_MASK;
        if (cbChunk > cb)
            cbChunk = cb;

        rc = PGMPhysSimpleWriteGCPtr(pVCpu, GCPtrDst, pvSrc, cbChunk);

        /* advance */
        if (RT_FAILURE(rc))
            break;
        *pcbWritten += cbChunk;
        cb          -= cbChunk;
        GCPtrDst    += cbChunk;
        pvSrc = (uint8_t const *)pvSrc + cbChunk;
    }

    return *pcbWritten && RT_FAILURE(rc) ? -rc : rc;

}


/**
 * memchr() with alignment considerations.
 *
 * @returns Pointer to matching byte, NULL if none found.
 * @param   pb                  Where to search. Aligned.
 * @param   b                   What to search for.
 * @param   cb                  How much to search .
 * @param   uAlign              The alignment restriction of the result.
 */
static const uint8_t *pgmR3DbgAlignedMemChr(const uint8_t *pb, uint8_t b, size_t cb, uint32_t uAlign)
{
    const uint8_t *pbRet;
    if (uAlign <= 32)
    {
        pbRet = (const uint8_t *)memchr(pb, b, cb);
        if ((uintptr_t)pbRet & (uAlign - 1))
        {
            do
            {
                pbRet++;
                size_t cbLeft = cb - (pbRet - pb);
                if (!cbLeft)
                {
                    pbRet = NULL;
                    break;
                }
                pbRet = (const uint8_t *)memchr(pbRet, b, cbLeft);
            } while ((uintptr_t)pbRet & (uAlign - 1));
        }
    }
    else
    {
        pbRet = NULL;
        if (cb)
        {
            for (;;)
            {
                if (*pb == b)
                {
                    pbRet = pb;
                    break;
                }
                if (cb <= uAlign)
                    break;
                cb -= uAlign;
                pb += uAlign;
            }
        }
    }
    return pbRet;
}


/**
 * Scans a page for a byte string, keeping track of potential
 * cross page matches.
 *
 * @returns true and *poff on match.
 *          false on mismatch.
 * @param   pbPage          Pointer to the current page.
 * @param   poff            Input: The offset into the page (aligned).
 *                          Output: The page offset of the match on success.
 * @param   cb              The number of bytes to search, starting of *poff.
 * @param   uAlign          The needle alignment. This is of course less than a page.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string.
 * @param   pabPrev         The buffer that keeps track of a partial match that we
 *                          bring over from the previous page. This buffer must be
 *                          at least cbNeedle - 1 big.
 * @param   pcbPrev         Input: The number of partial matching bytes from the previous page.
 *                          Output: The number of partial matching bytes from this page.
 *                          Initialize to 0 before the first call to this function.
 */
static bool pgmR3DbgScanPage(const uint8_t *pbPage, int32_t *poff, uint32_t cb, uint32_t uAlign,
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
        for (;;)
        {
            if (cbPrev <= uAlign)
                break;
            cbPrev -= uAlign;
            pb = pgmR3DbgAlignedMemChr(pb + uAlign, *pabNeedle, cbPrev, uAlign);
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
        pb = pgmR3DbgAlignedMemChr(pb, *pabNeedle, cb, uAlign);
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

        /* no match, skip ahead. */
        if (cb <= uAlign)
            break;
        pb += uAlign;
        cb -= uAlign;
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
 * @param   GCPhysAlign     The alignment of the needle. Must be a power of two
 *                          and less or equal to 4GB.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string. Max 256 bytes.
 * @param   pGCPhysHit      Where to store the address of the first occurence on success.
 */
VMMR3DECL(int) PGMR3DbgScanPhysical(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cbRange, RTGCPHYS GCPhysAlign,
                                    const uint8_t *pabNeedle, size_t cbNeedle, PRTGCPHYS pGCPhysHit)
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

    if (!GCPhysAlign)
        return VERR_INVALID_PARAMETER;
    if (GCPhysAlign > UINT32_MAX)
        return VERR_NOT_POWER_OF_TWO;
    if (GCPhysAlign & (GCPhysAlign - 1))
        return VERR_INVALID_PARAMETER;

    if (GCPhys & (GCPhysAlign - 1))
    {
        RTGCPHYS Adj = GCPhysAlign - (GCPhys & (GCPhysAlign - 1));
        if (    cbRange <= Adj
            ||  GCPhys + Adj < GCPhys)
            return VERR_DBGF_MEM_NOT_FOUND;
        GCPhys  += Adj;
        cbRange -= Adj;
    }

    const bool      fAllZero   = ASMMemIsAll8(pabNeedle, cbNeedle, 0) == NULL;
    const uint32_t  cIncPages  = GCPhysAlign <= PAGE_SIZE
                               ? 1
                               : GCPhysAlign >> PAGE_SHIFT;
    const RTGCPHYS  GCPhysLast = GCPhys + cbRange - 1 >= GCPhys
                               ? GCPhys + cbRange - 1
                               : ~(RTGCPHYS)0;

    /*
     * Search the memory - ignore MMIO and zero pages, also don't
     * bother to match across ranges.
     */
    pgmLock(pVM);
    for (PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
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
            uint8_t         abPrev[MAX_NEEDLE_SIZE];
            size_t          cbPrev   = 0;
            const uint32_t  cPages   = pRam->cb >> PAGE_SHIFT;
            uint32_t        iPage    = off >> PAGE_SHIFT;
            uint32_t        offPage  = GCPhys & PAGE_OFFSET_MASK;
            GCPhys &= ~(RTGCPHYS)PAGE_OFFSET_MASK;
            for (;; offPage = 0)
            {
                PPGMPAGE pPage = &pRam->aPages[iPage];
                if (    (   !PGM_PAGE_IS_ZERO(pPage)
                         || fAllZero)
                    &&  !PGM_PAGE_IS_BALLOONED(pPage)
                    &&  !PGM_PAGE_IS_MMIO(pPage))
                {
                    void const     *pvPage;
                    PGMPAGEMAPLOCK  Lock;
                    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, &pvPage, &Lock);
                    if (RT_SUCCESS(rc))
                    {
                        int32_t     offHit = offPage;
                        bool        fRc;
                        if (GCPhysAlign < PAGE_SIZE)
                        {
                            uint32_t cbSearch = (GCPhys ^ GCPhysLast) & ~(RTGCPHYS)PAGE_OFFSET_MASK
                                              ? PAGE_SIZE                           - (uint32_t)offPage
                                              : (GCPhysLast & PAGE_OFFSET_MASK) + 1 - (uint32_t)offPage;
                            fRc = pgmR3DbgScanPage((uint8_t const *)pvPage, &offHit, cbSearch, (uint32_t)GCPhysAlign,
                                                   pabNeedle, cbNeedle, &abPrev[0], &cbPrev);
                        }
                        else
                            fRc = memcmp(pvPage, pabNeedle, cbNeedle) == 0
                               && (GCPhysLast - GCPhys) >= cbNeedle;
                        PGMPhysReleasePageMappingLock(pVM, &Lock);
                        if (fRc)
                        {
                            *pGCPhysHit = GCPhys + offHit;
                            pgmUnlock(pVM);
                            return VINF_SUCCESS;
                        }
                    }
                    else
                        cbPrev = 0; /* ignore error. */
                }
                else
                    cbPrev = 0;

                /* advance to the next page. */
                GCPhys += (RTGCPHYS)cIncPages << PAGE_SHIFT;
                if (GCPhys >= GCPhysLast) /* (may not always hit, but we're run out of ranges.) */
                {
                    pgmUnlock(pVM);
                    return VERR_DBGF_MEM_NOT_FOUND;
                }
                iPage += cIncPages;
                if (    iPage < cIncPages
                    ||  iPage >= cPages)
                    break;
            }
        }
    }
    pgmUnlock(pVM);
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
 * @param   pVCpu           The CPU context to search in.
 * @param   GCPtr           Where to start searching.
 * @param   GCPtrAlign      The alignment of the needle. Must be a power of two
 *                          and less or equal to 4GB.
 * @param   cbRange         The number of bytes to search. Max 256 bytes.
 * @param   pabNeedle       The byte string to search for.
 * @param   cbNeedle        The length of the byte string.
 * @param   pGCPtrHit       Where to store the address of the first occurence on success.
 */
VMMR3DECL(int) PGMR3DbgScanVirtual(PVM pVM, PVMCPU pVCpu, RTGCPTR GCPtr, RTGCPTR cbRange, RTGCPTR GCPtrAlign,
                                   const uint8_t *pabNeedle, size_t cbNeedle, PRTGCUINTPTR pGCPtrHit)
{
    VMCPU_ASSERT_EMT(pVCpu);

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

    if (!GCPtrAlign)
        return VERR_INVALID_PARAMETER;
    if (GCPtrAlign > UINT32_MAX)
        return VERR_NOT_POWER_OF_TWO;
    if (GCPtrAlign & (GCPtrAlign - 1))
        return VERR_INVALID_PARAMETER;

    if (GCPtr & (GCPtrAlign - 1))
    {
        RTGCPTR Adj = GCPtrAlign - (GCPtr & (GCPtrAlign - 1));
        if (    cbRange <= Adj
            ||  GCPtr + Adj < GCPtr)
            return VERR_DBGF_MEM_NOT_FOUND;
        GCPtr   += Adj;
        cbRange -= Adj;
    }

    /*
     * Search the memory - ignore MMIO, zero and not-present pages.
     */
    const bool      fAllZero  = ASMMemIsAll8(pabNeedle, cbNeedle, 0) == NULL;
    PGMMODE         enmMode   = PGMGetGuestMode(pVCpu);
    RTGCPTR         GCPtrMask = PGMMODE_IS_LONG_MODE(enmMode) ? UINT64_MAX : UINT32_MAX;
    uint8_t         abPrev[MAX_NEEDLE_SIZE];
    size_t          cbPrev    = 0;
    const uint32_t  cIncPages = GCPtrAlign <= PAGE_SIZE
                              ? 1
                              : GCPtrAlign >> PAGE_SHIFT;
    const RTGCPTR   GCPtrLast = GCPtr + cbRange - 1 >= GCPtr
                              ? (GCPtr + cbRange - 1) & GCPtrMask
                              : GCPtrMask;
    RTGCPTR         cPages    = (((GCPtrLast - GCPtr) + (GCPtr & PAGE_OFFSET_MASK)) >> PAGE_SHIFT) + 1;
    uint32_t        offPage   = GCPtr & PAGE_OFFSET_MASK;
    GCPtr &= ~(RTGCPTR)PAGE_OFFSET_MASK;
    for (;; offPage = 0)
    {
        RTGCPHYS GCPhys;
        int rc = PGMPhysGCPtr2GCPhys(pVCpu, GCPtr, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
            if (    pPage
                &&  (   !PGM_PAGE_IS_ZERO(pPage)
                     || fAllZero)
                &&  !PGM_PAGE_IS_BALLOONED(pPage)
                &&  !PGM_PAGE_IS_MMIO(pPage))
            {
                void const *pvPage;
                PGMPAGEMAPLOCK Lock;
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, &pvPage, &Lock);
                if (RT_SUCCESS(rc))
                {
                    int32_t offHit = offPage;
                    bool    fRc;
                    if (GCPtrAlign < PAGE_SIZE)
                    {
                        uint32_t cbSearch = cPages > 0
                                          ? PAGE_SIZE                          - (uint32_t)offPage
                                          : (GCPtrLast & PAGE_OFFSET_MASK) + 1 - (uint32_t)offPage;
                        fRc = pgmR3DbgScanPage((uint8_t const *)pvPage, &offHit, cbSearch, (uint32_t)GCPtrAlign,
                                               pabNeedle, cbNeedle, &abPrev[0], &cbPrev);
                    }
                    else
                        fRc = memcmp(pvPage, pabNeedle, cbNeedle) == 0
                           && (GCPtrLast - GCPtr) >= cbNeedle;
                    PGMPhysReleasePageMappingLock(pVM, &Lock);
                    if (fRc)
                    {
                        *pGCPtrHit = GCPtr + offHit;
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

        /* advance to the next page. */
        if (cPages <= cIncPages)
            break;
        cPages -= cIncPages;
        GCPtr += (RTGCPTR)cIncPages << PAGE_SHIFT;
    }
    return VERR_DBGF_MEM_NOT_FOUND;
}


/**
 * Dumps a PAE shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pArgs       Dumper state.
 * @param   pPT         Pointer to the page table.
 * @param   pHlp        Pointer to the output functions.
 */
static int  pgmR3DumpHierarchyHCPaePT(PPGMR3DUMPHIERARCHYSTATE pState, RTHCPHYS HCPhys, bool fIsMapping)
{
    PPGMSHWPTPAE pPT = NULL;
    if (!fIsMapping)
        pPT = (PPGMSHWPTPAE)MMPagePhys2Page(pState->pVM, HCPhys);
    else
    {
        for (PPGMMAPPING pMap = pState->pVM->pgm.s.pMappingsR3; pMap; pMap = pMap->pNextR3)
        {
            uint64_t off = pState->u64Address - pMap->GCPtr;
            if (off < pMap->cb)
            {
                const int iPDE = (uint32_t)(off >> X86_PD_SHIFT);
                const int iSub = (int)((off >> X86_PD_PAE_SHIFT) & 1); /* MSC is a pain sometimes */
                if ((iSub ? pMap->aPTs[iPDE].HCPhysPaePT1 : pMap->aPTs[iPDE].HCPhysPaePT0) != HCPhys)
                    pState->pHlp->pfnPrintf(pState->pHlp,
                                            "%0*llx error! Mapping error! PT %d has HCPhysPT=%RHp not %RHp is in the PD.\n",
                                            pState->cchAddress, pState->u64Address, iPDE,
                                            iSub ? pMap->aPTs[iPDE].HCPhysPaePT1 : pMap->aPTs[iPDE].HCPhysPaePT0, HCPhys);
                pPT = &pMap->aPTs[iPDE].paPaePTsR3[iSub];
                break;
            }
        }
    }
    if (!pPT)
    {
        pState->pHlp->pfnPrintf(pState->pHlp, "%0*llx error! Page table at HCPhys=%RHp was not found in the page pool!\n",
                                pState->cchAddress, pState->u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
        if (PGMSHWPTEPAE_IS_P(pPT->a[i]))
        {
            X86PTEPAE Pte;
            Pte.u = PGMSHWPTEPAE_GET_U(pPT->a[i]);
            pState->pHlp->pfnPrintf(pState->pHlp,
                                    pState->fLme  /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                    ? "%016llx 3    | P %c %c %c %c %c %s %s %s %s 4K %c%c%c  %016llx\n"
                                    :  "%08llx 2   |  P %c %c %c %c %c %s %s %s %s 4K %c%c%c  %016llx\n",
                                    pState->u64Address + ((uint64_t)i << X86_PT_PAE_SHIFT),
                                    Pte.n.u1Write       ? 'W'  : 'R',
                                    Pte.n.u1User        ? 'U'  : 'S',
                                    Pte.n.u1Accessed    ? 'A'  : '-',
                                    Pte.n.u1Dirty       ? 'D'  : '-',
                                    Pte.n.u1Global      ? 'G'  : '-',
                                    Pte.n.u1WriteThru   ? "WT" : "--",
                                    Pte.n.u1CacheDisable? "CD" : "--",
                                    Pte.n.u1PAT         ? "AT" : "--",
                                    Pte.n.u1NoExecute   ? "NX" : "--",
                                    Pte.u & PGM_PTFLAGS_TRACK_DIRTY   ? 'd' : '-',
                                    Pte.u & RT_BIT(10)                ? '1' : '0',
                                    Pte.u & PGM_PTFLAGS_CSAM_VALIDATED? 'v' : '-',
                                    Pte.u & X86_PTE_PAE_PG_MASK);

            pState->cLeafs++;
        }
        else if (PGMSHWPTEPAE_GET_U(pPT->a[i]) & X86_PTE_P)
        {
            if (   (PGMSHWPTEPAE_GET_U(pPT->a[i]) & (pState->pVM->pgm.s.HCPhysInvMmioPg | X86_PTE_PAE_MBZ_MASK_NO_NX))
                ==                                  (pState->pVM->pgm.s.HCPhysInvMmioPg | X86_PTE_PAE_MBZ_MASK_NO_NX))
                pState->pHlp->pfnPrintf(pState->pHlp,
                                        pState->fLme
                                        ? "%016llx 3    | invalid / MMIO optimization\n"
                                        :  "%08llx 2   |  invalid / MMIO optimization\n",
                                        pState->u64Address + ((uint64_t)i << X86_PT_PAE_SHIFT));
            else
                pState->pHlp->pfnPrintf(pState->pHlp,
                                        pState->fLme
                                        ? "%016llx 3    | invalid: %RX64\n"
                                        :  "%08llx 2   |  invalid: %RX64\n",
                                        pState->u64Address + ((uint64_t)i << X86_PT_PAE_SHIFT),
                                        PGMSHWPTEPAE_GET_U(pPT->a[i]));
            pState->cLeafs++;
        }
    return VINF_SUCCESS;
}


/**
 * Dumps a PAE shadow page directory table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   HCPhys      The physical address of the page directory table.
 * @param   u64Address  The virtual address of the page table starts.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   fLongMode   Set if this a long mode table; clear if it's a legacy mode table.
 * @param   cMaxDepth   The maxium depth.
 * @param   pHlp        Pointer to the output functions.
 */
static int  pgmR3DumpHierarchyHCPaePD(PPGMR3DUMPHIERARCHYSTATE pState, RTHCPHYS HCPhys, unsigned cMaxDepth)
{
    Assert(cMaxDepth > 0);
    cMaxDepth--;

    PX86PDPAE pPD = (PX86PDPAE)MMPagePhys2Page(pState->pVM, HCPhys);
    if (!pPD)
    {
        pState->pHlp->pfnPrintf(pState->pHlp,
                                "%0*llx error! Page directory at HCPhys=%RHp was not found in the page pool!\n",
                                pState->cchAddress, pState->u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int             rc              = VINF_SUCCESS;
    const uint64_t  u64BaseAddress  = pState->u64Address;
    for (unsigned i = 0; i < RT_ELEMENTS(pPD->a); i++)
    {
        X86PDEPAE Pde = pPD->a[i];
        pState->u64Address = u64BaseAddress + ((uint64_t)i << X86_PD_PAE_SHIFT);
        if (    Pde.n.u1Present
            &&  pState->u64Address >= pState->u64FirstAddress
            &&  pState->u64Address <= pState->u64LastAddress)
        {
            if (Pde.b.u1Size)
            {
                pState->pHlp->pfnPrintf(pState->pHlp,
                                        pState->fLme    /*P R  S  A  D  G  WT CD AT NX 2M a  p ?  phys*/
                                        ? "%016llx 2   |  P %c %c %c %c %c %s %s %s %s 2M %c%c%c  %016llx"
                                        :  "%08llx 1  |   P %c %c %c %c %c %s %s %s %s 2M %c%c%c  %016llx",
                                        pState->u64Address,
                                        Pde.b.u1Write       ? 'W'  : 'R',
                                        Pde.b.u1User        ? 'U'  : 'S',
                                        Pde.b.u1Accessed    ? 'A'  : '-',
                                        Pde.b.u1Dirty       ? 'D'  : '-',
                                        Pde.b.u1Global      ? 'G'  : '-',
                                        Pde.b.u1WriteThru   ? "WT" : "--",
                                        Pde.b.u1CacheDisable? "CD" : "--",
                                        Pde.b.u1PAT         ? "AT" : "--",
                                        Pde.b.u1NoExecute   ? "NX" : "--",
                                        Pde.u & RT_BIT_64(9)            ? '1' : '0',
                                        Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                        Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                        Pde.u & X86_PDE2M_PAE_PG_MASK);
                if ((Pde.u >> 52) & 0x7ff)
                    pState->pHlp->pfnPrintf(pState->pHlp, " 62:52=%03llx!", (Pde.u >> 52) & 0x7ff);
                if ((Pde.u >> 13) & 0xff)
                    pState->pHlp->pfnPrintf(pState->pHlp, " 20:13=%02llx!", (Pde.u >> 13) & 0xff);
                pState->pHlp->pfnPrintf(pState->pHlp, "\n");

                pState->cLeafs++;
            }
            else
            {
                pState->pHlp->pfnPrintf(pState->pHlp,
                                        pState->fLme    /*P R  S  A  D  G  WT CD AT NX 4M a  p ?  phys */
                                        ? "%016llx 2   |  P %c %c %c %c %c %s %s .. %s 4K %c%c%c  %016llx"
                                        :  "%08llx 1  |   P %c %c %c %c %c %s %s .. %s 4K %c%c%c  %016llx",
                                        pState->u64Address,
                                        Pde.n.u1Write       ? 'W'  : 'R',
                                        Pde.n.u1User        ? 'U'  : 'S',
                                        Pde.n.u1Accessed    ? 'A'  : '-',
                                        Pde.n.u1Reserved0   ? '?'  : '.', /* ignored */
                                        Pde.n.u1Reserved1   ? '?'  : '.', /* ignored */
                                        Pde.n.u1WriteThru   ? "WT" : "--",
                                        Pde.n.u1CacheDisable? "CD" : "--",
                                        Pde.n.u1NoExecute   ? "NX" : "--",
                                        Pde.u & RT_BIT_64(9)            ? '1' : '0',
                                        Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                        Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                        Pde.u & X86_PDE_PAE_PG_MASK_FULL);
                if ((Pde.u >> 52) & 0x7ff)
                    pState->pHlp->pfnPrintf(pState->pHlp, " 62:52=%03llx!", (Pde.u >> 52) & 0x7ff);
                pState->pHlp->pfnPrintf(pState->pHlp, "\n");

                if (cMaxDepth)
                {
                    int rc2 = pgmR3DumpHierarchyHCPaePT(pState, Pde.u & X86_PDE_PAE_PG_MASK_FULL, !!(Pde.u & PGM_PDFLAGS_MAPPING));
                    if (rc2 < rc && RT_SUCCESS(rc))
                        rc = rc2;
                }
                else
                    pState->cLeafs++;
            }
        }
    }
    return rc;
}


/**
 * Dumps a PAE shadow page directory pointer table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   HCPhys      The physical address of the page directory pointer table.
 * @param   u64Address  The virtual address of the page table starts.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   fLongMode   Set if this a long mode table; clear if it's a legacy mode table.
 * @param   cMaxDepth   The maxium depth.
 * @param   pHlp        Pointer to the output functions.
 */
static int  pgmR3DumpHierarchyHCPaePDPT(PPGMR3DUMPHIERARCHYSTATE pState, RTHCPHYS HCPhys, unsigned cMaxDepth)
{
    Assert(cMaxDepth > 0);
    cMaxDepth--;

    PX86PDPT pPDPT = (PX86PDPT)MMPagePhys2Page(pState->pVM, HCPhys);
    if (!pPDPT)
    {
        pState->pHlp->pfnPrintf(pState->pHlp,
                                "%0*llx error! Page directory pointer table at HCPhys=%RHp was not found in the page pool!\n",
                                pState->cchAddress, pState->u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int             rc              = VINF_SUCCESS;
    const uint64_t  u64BaseAddress  = pState->u64Address;
    const unsigned  c               = pState->fLme ? RT_ELEMENTS(pPDPT->a) : X86_PG_PAE_PDPE_ENTRIES;
    for (unsigned i = 0; i < c; i++)
    {
        X86PDPE Pdpe = pPDPT->a[i];
        pState->u64Address = u64BaseAddress + ((uint64_t)i << X86_PDPT_SHIFT);
        if (    Pdpe.n.u1Present
            &&  pState->u64Address >= pState->u64FirstAddress
            &&  pState->u64Address <= pState->u64LastAddress)
        {
            if (pState->fLme)
            {
                pState->pHlp->pfnPrintf(pState->pHlp, /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                        "%016llx 1  |   P %c %c %c %c %c %s %s %s %s .. %c%c%c  %016llx",
                                        pState->u64Address,
                                        Pdpe.lm.u1Write       ? 'W'  : 'R',
                                        Pdpe.lm.u1User        ? 'U'  : 'S',
                                        Pdpe.lm.u1Accessed    ? 'A'  : '-',
                                        Pdpe.lm.u3Reserved & 1? '?'  : '.', /* ignored */
                                        Pdpe.lm.u3Reserved & 4? '!'  : '.', /* mbz */
                                        Pdpe.lm.u1WriteThru   ? "WT" : "--",
                                        Pdpe.lm.u1CacheDisable? "CD" : "--",
                                        Pdpe.lm.u3Reserved & 2? "!"  : "..",/* mbz */
                                        Pdpe.lm.u1NoExecute   ? "NX" : "--",
                                        Pdpe.u & RT_BIT(9)                ? '1' : '0',
                                        Pdpe.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                        Pdpe.u & RT_BIT(11)               ? '1' : '0',
                                        Pdpe.u & X86_PDPE_PG_MASK_FULL);
                if ((Pdpe.u >> 52) & 0x7ff)
                    pState->pHlp->pfnPrintf(pState->pHlp, " 62:52=%03llx\n", (Pdpe.u >> 52) & 0x7ff);
            }
            else
            {
                pState->pHlp->pfnPrintf(pState->pHlp,/*P             G  WT CD AT NX 4M a p ?  */
                                        "%08llx 0 |    P             %c %s %s %s %s .. %c%c%c  %016llx",
                                        pState->u64Address,
                                        Pdpe.n.u4Reserved & 1? '!'  : '.', /* mbz */
                                        Pdpe.n.u4Reserved & 4? '!'  : '.', /* mbz */
                                        Pdpe.n.u1WriteThru   ? "WT" : "--",
                                        Pdpe.n.u1CacheDisable? "CD" : "--",
                                        Pdpe.n.u4Reserved & 2? "!"  : "..",/* mbz */
                                        Pdpe.u & RT_BIT(9)                ? '1' : '0',
                                        Pdpe.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                        Pdpe.u & RT_BIT(11)               ? '1' : '0',
                                        Pdpe.u & X86_PDPE_PG_MASK_FULL);
                if ((Pdpe.u >> 52) & 0xfff)
                    pState->pHlp->pfnPrintf(pState->pHlp, " 63:52=%03llx\n", (Pdpe.u >> 52) & 0xfff);
            }
            pState->pHlp->pfnPrintf(pState->pHlp, "\n");
            if (cMaxDepth)
            {
                int rc2 = pgmR3DumpHierarchyHCPaePD(pState, Pdpe.u & X86_PDPE_PG_MASK_FULL, cMaxDepth);
                if (rc2 < rc && RT_SUCCESS(rc))
                    rc = rc2;
            }
            else
                pState->cLeafs++;
        }
    }
    return rc;
}


/**
 * Dumps a 32-bit shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   HCPhys      The physical address of the table.
 * @param   cMaxDepth   The maxium depth.
 */
static int pgmR3DumpHierarchyHcPaePML4(PPGMR3DUMPHIERARCHYSTATE pState, RTHCPHYS HCPhys, unsigned cMaxDepth)
{
    PX86PML4 pPML4 = (PX86PML4)MMPagePhys2Page(pState->pVM, HCPhys);
    if (!pPML4)
    {
        pState->pHlp->pfnPrintf(pState->pHlp, "Page map level 4 at HCPhys=%RHp was not found in the page pool!\n", HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int         rc      = VINF_SUCCESS;
    uint32_t    cFound  = 0;
    for (uint32_t i = (pState->u64LastAddress >> X86_PML4_SHIFT) & X86_PML4_MASK; i < RT_ELEMENTS(pPML4->a); i++)
    {
        X86PML4E Pml4e = pPML4->a[i];
        pState->u64Address = ((uint64_t)i << X86_PML4_SHIFT)
                           | (((uint64_t)i >> (X86_PML4_SHIFT - X86_PDPT_SHIFT - 1)) * UINT64_C(0xffff000000000000));
        if (    Pml4e.n.u1Present
            &&  pState->u64Address >= pState->u64FirstAddress
            &&  pState->u64Address <= pState->u64LastAddress
           )
        {
            pState->pHlp->pfnPrintf(pState->pHlp, /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                    "%016llx 0 |    P %c %c %c %c %c %s %s %s %s .. %c%c%c  %016llx",
                                    pState->u64Address,
                                    Pml4e.n.u1Write       ? 'W'  : 'R',
                                    Pml4e.n.u1User        ? 'U'  : 'S',
                                    Pml4e.n.u1Accessed    ? 'A'  : '-',
                                    Pml4e.n.u3Reserved & 1? '?'  : '.', /* ignored */
                                    Pml4e.n.u3Reserved & 4? '!'  : '.', /* mbz */
                                    Pml4e.n.u1WriteThru   ? "WT" : "--",
                                    Pml4e.n.u1CacheDisable? "CD" : "--",
                                    Pml4e.n.u3Reserved & 2? "!"  : "..",/* mbz */
                                    Pml4e.n.u1NoExecute   ? "NX" : "--",
                                    Pml4e.u & RT_BIT(9)                ? '1' : '0',
                                    Pml4e.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                    Pml4e.u & RT_BIT(11)               ? '1' : '0',
                                    Pml4e.u & X86_PML4E_PG_MASK);
            /** @todo Dump the shadow page referenced? */
            if ((Pml4e.u >> 52) & 0x7ff)
                pState->pHlp->pfnPrintf(pState->pHlp, " 62:52=%03llx!", (Pml4e.u >> 52) & 0x7ff);
            pState->pHlp->pfnPrintf(pState->pHlp, "\n");

            if (cMaxDepth >= 1)
            {
                int rc2 = pgmR3DumpHierarchyHCPaePDPT(pState, Pml4e.u & X86_PML4E_PG_MASK, cMaxDepth - 1);
                if (rc2 < rc && RT_SUCCESS(rc))
                    rc = rc2;
            }
            else
                pState->cLeafs++;
        }
    }
    return rc;
}


/**
 * Dumps a 32-bit shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   pPT         Pointer to the page table.
 * @param   u32Address  The virtual address this table starts at.
 * @param   pHlp        Pointer to the output functions.
 */
static int pgmR3DumpHierarchyHC32BitPT(PPGMR3DUMPHIERARCHYSTATE pState, RTHCPHYS HCPhys, bool fMapping)
{
    /** @todo what about using the page pool for mapping PTs? */
    PX86PT pPT = NULL;
    if (!fMapping)
        pPT = (PX86PT)MMPagePhys2Page(pState->pVM, HCPhys);
    else
    {
        for (PPGMMAPPING pMap = pState->pVM->pgm.s.pMappingsR3; pMap; pMap = pMap->pNextR3)
            if (pState->u64Address - pMap->GCPtr < pMap->cb)
            {
                int iPDE = (pState->u64Address - pMap->GCPtr) >> X86_PD_SHIFT;
                if (pMap->aPTs[iPDE].HCPhysPT != HCPhys)
                    pState->pHlp->pfnPrintf(pState->pHlp,
                                            "%08llx error! Mapping error! PT %d has HCPhysPT=%RHp not %RHp is in the PD.\n",
                                            pState->u64Address, iPDE, pMap->aPTs[iPDE].HCPhysPT, HCPhys);
                pPT = pMap->aPTs[iPDE].pPTR3;
            }
    }
    if (!pPT)
    {
        pState->pHlp->pfnPrintf(pState->pHlp,
                                "%08llx error! Page table at %#x was not found in the page pool!\n",
                                pState->u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }


    for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
    {
        X86PTE Pte = pPT->a[i];
        if (Pte.n.u1Present)
        {
            uint64_t u64Address = pState->u64Address + (i << X86_PT_SHIFT);
            if (   u64Address < pState->u64FirstAddress
                || u64Address < pState->u64LastAddress)
                continue;
            pState->pHlp->pfnPrintf(pState->pHlp,/*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                                    "%08llx 1  |   P %c %c %c %c %c %s %s %s .. 4K %c%c%c  %08x\n",
                                    u64Address,
                                    Pte.n.u1Write       ? 'W'  : 'R',
                                    Pte.n.u1User        ? 'U'  : 'S',
                                    Pte.n.u1Accessed    ? 'A'  : '-',
                                    Pte.n.u1Dirty       ? 'D'  : '-',
                                    Pte.n.u1Global      ? 'G'  : '-',
                                    Pte.n.u1WriteThru   ? "WT" : "--",
                                    Pte.n.u1CacheDisable? "CD" : "--",
                                    Pte.n.u1PAT         ? "AT" : "--",
                                    Pte.u & PGM_PTFLAGS_TRACK_DIRTY     ? 'd' : '-',
                                    Pte.u & RT_BIT(10)                  ? '1' : '0',
                                    Pte.u & PGM_PTFLAGS_CSAM_VALIDATED  ? 'v' : '-',
                                    Pte.u & X86_PDE_PG_MASK);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Dumps a 32-bit shadow page directory and page tables.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   cMaxDepth   How deep into the hierarchy the dumper should go.
 * @param   pHlp        Pointer to the output functions.
 */
static int pgmR3DumpHierarchyHC32BitPD(PPGMR3DUMPHIERARCHYSTATE pState, RTHCPHYS HCPhys, unsigned cMaxDepth)
{
    Assert(cMaxDepth > 0);
    cMaxDepth--;

    PX86PD pPD = (PX86PD)MMPagePhys2Page(pState->pVM, HCPhys);
    if (!pPD)
    {
        pState->pHlp->pfnPrintf(pState->pHlp,
                                "Page directory at %#x was not found in the page pool!\n", HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int             rc              = VINF_SUCCESS;
    const uint64_t  u64BaseAddress  = pState->u64Address;
    for (unsigned i = 0; i < RT_ELEMENTS(pPD->a); i++)
    {
        X86PDE Pde = pPD->a[i];
        if (Pde.n.u1Present)
        {
            pState->u64Address = (uint32_t)i << X86_PD_SHIFT;
            if (   pState->u64Address < pState->u64FirstAddress
                && pState->u64Address > pState->u64LastAddress)
                continue;

            if (Pde.b.u1Size && pState->fPse)
            {
                pState->pHlp->pfnPrintf(pState->pHlp,/*P R  S  A  D  G  WT CD AT NX 4M a m d   phys */
                                        "%08llx 0 |    P %c %c %c %c %c %s %s %s .. 4M %c%c%c  %08llx\n",
                                        pState->u64Address,
                                        Pde.b.u1Write       ? 'W'  : 'R',
                                        Pde.b.u1User        ? 'U'  : 'S',
                                        Pde.b.u1Accessed    ? 'A'  : '-',
                                        Pde.b.u1Dirty       ? 'D'  : '-',
                                        Pde.b.u1Global      ? 'G'  : '-',
                                        Pde.b.u1WriteThru   ? "WT" : "--",
                                        Pde.b.u1CacheDisable? "CD" : "--",
                                        Pde.b.u1PAT         ? "AT" : "--",
                                        Pde.u & RT_BIT_32(9)            ? '1' : '0',
                                        Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                        Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                          ((Pde.u & X86_PDE4M_PG_HIGH_MASK) << X86_PDE4M_PG_HIGH_SHIFT)
                                        | (Pde.u & X86_PDE4M_PG_MASK) );
                pState->cLeafs++;
            }
            else
            {
                pState->pHlp->pfnPrintf(pState->pHlp,/*P R  S  A  D  G  WT CD AT NX 4M a m d   phys */
                                        "%08llx 0 |    P %c %c %c %c %c %s %s .. .. 4K %c%c%c  %08x\n",
                                        pState->u64Address,
                                        Pde.n.u1Write       ? 'W'  : 'R',
                                        Pde.n.u1User        ? 'U'  : 'S',
                                        Pde.n.u1Accessed    ? 'A'  : '-',
                                        Pde.n.u1Reserved0   ? '?'  : '.', /* ignored */
                                        Pde.n.u1Reserved1   ? '?'  : '.', /* ignored */
                                        Pde.n.u1WriteThru   ? "WT" : "--",
                                        Pde.n.u1CacheDisable? "CD" : "--",
                                        Pde.u & RT_BIT_32(9)            ? '1' : '0',
                                        Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                        Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                        Pde.u & X86_PDE_PG_MASK);
                if (cMaxDepth)
                {
                    int rc2 = pgmR3DumpHierarchyHC32BitPT(pState, Pde.u & X86_PDE_PG_MASK, !!(Pde.u & PGM_PDFLAGS_MAPPING));
                    if (rc2 < rc && RT_SUCCESS(rc))
                        rc = rc2;
                }
                else
                    pState->cLeafs++;
            }
        }
    }

    return rc;
}


/**
 * Internal worker that initiates the actual dump.
 *
 * @returns VBox status code.
 * @param   pState              The dumper state.
 * @param   cr3                 The CR3 value.
 * @param   cMaxDepth           The max depth.
 */
static int pdmR3DumpHierarchyHcDoIt(PPGMR3DUMPHIERARCHYSTATE pState, uint64_t cr3, unsigned cMaxDepth)
{
    const unsigned cch = pState->cchAddress;
    pState->pHlp->pfnPrintf(pState->pHlp,
                            "cr3=%0*llx %s\n"
                            "%-*s        P - Present\n"
                            "%-*s        | R/W - Read (0) / Write (1)\n"
                            "%-*s        | | U/S - User (1) / Supervisor (0)\n"
                            "%-*s        | | | A - Accessed\n"
                            "%-*s        | | | | D - Dirty\n"
                            "%-*s        | | | | | G - Global\n"
                            "%-*s        | | | | | | WT - Write thru\n"
                            "%-*s        | | | | | | |  CD - Cache disable\n"
                            "%-*s        | | | | | | |  |  AT - Attribute table (PAT)\n"
                            "%-*s        | | | | | | |  |  |  NX - No execute (K8)\n"
                            "%-*s        | | | | | | |  |  |  |  4K/4M/2M - Page size.\n"
                            "%-*s        | | | | | | |  |  |  |  |  AVL - a=allocated; m=mapping; d=track dirty;\n"
                            "%-*s        | | | | | | |  |  |  |  |  |     p=permanent; v=validated;\n"
                            "%-*s Level  | | | | | | |  |  |  |  |  |    Page\n"
                          /* xxxx n **** P R S A D G WT CD AT NX 4M AVL xxxxxxxxxxxxx
                                         - W U - - - -- -- -- -- -- 010 */
                            ,
                            cch, cr3,
                            pState->fLme ? "Long Mode" : pState->fPae ? "PAE" : pState->fPse ? "32-bit w/ PSE" : "32-bit",
                            cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "",
                            cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "Address");
    if (pState->fLme)
        return pgmR3DumpHierarchyHcPaePML4(pState, cr3 & X86_CR3_PAGE_MASK,     cMaxDepth);
    if (pState->fPae)
        return pgmR3DumpHierarchyHCPaePDPT(pState, cr3 & X86_CR3_PAE_PAGE_MASK, cMaxDepth);
    return     pgmR3DumpHierarchyHC32BitPD(pState, cr3 & X86_CR3_PAGE_MASK,     cMaxDepth);
}


/**
 * Dumps a page table hierarchy use only physical addresses and cr4/lm flags.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The cr4, only PAE and PSE is currently used.
 * @param   fLongMode   Set if long mode, false if not long mode.
 * @param   cMaxDepth   Number of levels to dump.
 * @param   pHlp        Pointer to the output functions.
 */
VMMR3DECL(int) PGMR3DumpHierarchyHC(PVM pVM, uint64_t cr3, uint64_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    if (!cMaxDepth)
        return VINF_SUCCESS;

    PGMR3DUMPHIERARCHYSTATE State;
    State.pVM               = pVM;
    State.pHlp              = pHlp ? pHlp : DBGFR3InfoLogHlp();
    State.fPse              = (cr4 & X86_CR4_PSE) || (cr4 & X86_CR4_PAE) || fLongMode;
    State.fPae              = (cr4 & X86_CR4_PAE) || fLongMode;
    State.fLme              = fLongMode;
    State.cchAddress        = fLongMode ? 16 : 8;
    State.u64Address        = 0;
    State.u64FirstAddress   = 0;
    State.u64LastAddress    = fLongMode ? UINT64_MAX : UINT32_MAX;
    State.cLeafs            = 0;
    return pdmR3DumpHierarchyHcDoIt(&State, cr3, cMaxDepth);
}



/**
 * Dumps a 32-bit shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   pPT         Pointer to the page table.
 * @param   u32Address  The virtual address this table starts at.
 * @param   PhysSearch  Address to search for.
 */
int pgmR3DumpHierarchyGC32BitPT(PVM pVM, PX86PT pPT, uint32_t u32Address, RTGCPHYS PhysSearch)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
    {
        X86PTE Pte = pPT->a[i];
        if (Pte.n.u1Present)
        {
            Log((           /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                 "%08x 1  |   P %c %c %c %c %c %s %s %s .. 4K %c%c%c  %08x\n",
                 u32Address + (i << X86_PT_SHIFT),
                 Pte.n.u1Write       ? 'W'  : 'R',
                 Pte.n.u1User        ? 'U'  : 'S',
                 Pte.n.u1Accessed    ? 'A'  : '-',
                 Pte.n.u1Dirty       ? 'D'  : '-',
                 Pte.n.u1Global      ? 'G'  : '-',
                 Pte.n.u1WriteThru   ? "WT" : "--",
                 Pte.n.u1CacheDisable? "CD" : "--",
                 Pte.n.u1PAT         ? "AT" : "--",
                 Pte.u & PGM_PTFLAGS_TRACK_DIRTY     ? 'd' : '-',
                 Pte.u & RT_BIT(10)                  ? '1' : '0',
                 Pte.u & PGM_PTFLAGS_CSAM_VALIDATED  ? 'v' : '-',
                 Pte.u & X86_PDE_PG_MASK));

            if ((Pte.u & X86_PDE_PG_MASK) == PhysSearch)
            {
                uint64_t fPageShw = 0;
                RTHCPHYS pPhysHC = 0;

                /** @todo SMP support!! */
                PGMShwGetPage(&pVM->aCpus[0], (RTGCPTR)(u32Address + (i << X86_PT_SHIFT)), &fPageShw, &pPhysHC);
                Log(("Found %RGp at %RGv -> flags=%llx\n", PhysSearch, (RTGCPTR)(u32Address + (i << X86_PT_SHIFT)), fPageShw));
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Dumps a 32-bit guest page directory and page tables.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   PhysSearch  Address to search for.
 */
VMMR3DECL(int) PGMR3DumpHierarchyGC(PVM pVM, uint64_t cr3, uint64_t cr4, RTGCPHYS PhysSearch)
{
    bool fLongMode = false;
    const unsigned cch = fLongMode ? 16 : 8; NOREF(cch);
    PX86PD pPD = 0;
    PGMPAGEMAPLOCK LockCr3;

    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, cr3 & X86_CR3_PAGE_MASK, (const void **)&pPD, &LockCr3);
    if (    RT_FAILURE(rc)
        ||  !pPD)
    {
        Log(("Page directory at %#x was not found in the page pool!\n", cr3 & X86_CR3_PAGE_MASK));
        return VERR_INVALID_PARAMETER;
    }

    Log(("cr3=%08x cr4=%08x%s\n"
         "%-*s        P - Present\n"
         "%-*s        | R/W - Read (0) / Write (1)\n"
         "%-*s        | | U/S - User (1) / Supervisor (0)\n"
         "%-*s        | | | A - Accessed\n"
         "%-*s        | | | | D - Dirty\n"
         "%-*s        | | | | | G - Global\n"
         "%-*s        | | | | | | WT - Write thru\n"
         "%-*s        | | | | | | |  CD - Cache disable\n"
         "%-*s        | | | | | | |  |  AT - Attribute table (PAT)\n"
         "%-*s        | | | | | | |  |  |  NX - No execute (K8)\n"
         "%-*s        | | | | | | |  |  |  |  4K/4M/2M - Page size.\n"
         "%-*s        | | | | | | |  |  |  |  |  AVL - a=allocated; m=mapping; d=track dirty;\n"
         "%-*s        | | | | | | |  |  |  |  |  |     p=permanent; v=validated;\n"
         "%-*s Level  | | | | | | |  |  |  |  |  |    Page\n"
       /* xxxx n **** P R S A D G WT CD AT NX 4M AVL xxxxxxxxxxxxx
                      - W U - - - -- -- -- -- -- 010 */
         , cr3, cr4, fLongMode ? " Long Mode" : "",
         cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "",
         cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "Address"));

    for (unsigned i = 0; i < RT_ELEMENTS(pPD->a); i++)
    {
        X86PDE Pde = pPD->a[i];
        if (Pde.n.u1Present)
        {
            const uint32_t u32Address = i << X86_PD_SHIFT;

            if ((cr4 & X86_CR4_PSE) && Pde.b.u1Size)
                Log((           /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                     "%08x 0 |    P %c %c %c %c %c %s %s %s .. 4M %c%c%c  %08x\n",
                     u32Address,
                     Pde.b.u1Write       ? 'W'  : 'R',
                     Pde.b.u1User        ? 'U'  : 'S',
                     Pde.b.u1Accessed    ? 'A'  : '-',
                     Pde.b.u1Dirty       ? 'D'  : '-',
                     Pde.b.u1Global      ? 'G'  : '-',
                     Pde.b.u1WriteThru   ? "WT" : "--",
                     Pde.b.u1CacheDisable? "CD" : "--",
                     Pde.b.u1PAT         ? "AT" : "--",
                     Pde.u & RT_BIT(9)      ? '1' : '0',
                     Pde.u & RT_BIT(10)     ? '1' : '0',
                     Pde.u & RT_BIT(11)     ? '1' : '0',
                     pgmGstGet4MBPhysPage(&pVM->pgm.s, Pde)));
            /** @todo PhysSearch */
            else
            {
                Log((           /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                     "%08x 0 |    P %c %c %c %c %c %s %s .. .. 4K %c%c%c  %08x\n",
                     u32Address,
                     Pde.n.u1Write       ? 'W'  : 'R',
                     Pde.n.u1User        ? 'U'  : 'S',
                     Pde.n.u1Accessed    ? 'A'  : '-',
                     Pde.n.u1Reserved0   ? '?'  : '.', /* ignored */
                     Pde.n.u1Reserved1   ? '?'  : '.', /* ignored */
                     Pde.n.u1WriteThru   ? "WT" : "--",
                     Pde.n.u1CacheDisable? "CD" : "--",
                     Pde.u & RT_BIT(9)      ? '1' : '0',
                     Pde.u & RT_BIT(10)     ? '1' : '0',
                     Pde.u & RT_BIT(11)     ? '1' : '0',
                     Pde.u & X86_PDE_PG_MASK));
                ////if (cMaxDepth >= 1)
                {
                    /** @todo what about using the page pool for mapping PTs? */
                    RTGCPHYS GCPhys = Pde.u & X86_PDE_PG_MASK;
                    PX86PT pPT = NULL;
                    PGMPAGEMAPLOCK LockPT;

                    rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, (const void **)&pPT, &LockPT);

                    int rc2 = VERR_INVALID_PARAMETER;
                    if (pPT)
                        rc2 = pgmR3DumpHierarchyGC32BitPT(pVM, pPT, u32Address, PhysSearch);
                    else
                        Log(("%08x error! Page table at %#x was not found in the page pool!\n", u32Address, GCPhys));

                    if (rc == VINF_SUCCESS)
                        PGMPhysReleasePageMappingLock(pVM, &LockPT);

                    if (rc2 < rc && RT_SUCCESS(rc))
                        rc = rc2;
                }
            }
        }
    }
    PGMPhysReleasePageMappingLock(pVM, &LockCr3);
    return rc;
}

