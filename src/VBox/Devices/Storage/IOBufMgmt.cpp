/* $Id$ */
/** @file
 * VBox storage devices: I/O buffer management API.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <VBox/cdefs.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/memsafer.h>
#include <iprt/sg.h>
#include <iprt/asm.h>

/** The minimum bin size to create - power of two!. */
#define IOBUFMGR_BIN_SIZE_MIN _4K
/** The maximum bin size to create - power of two!. */
#define IOBUFMGR_BIN_SIZE_MAX _1M

/** Pointer to the internal I/O buffer manager data. */
typedef struct IOBUFMGRINT *PIOBUFMGRINT;

/**
 * Internal I/O buffer descriptor data.
 */
typedef struct IOBUFDESCINT
{
    /** Data segments. */
    RTSGSEG      aSegs[10];
    /** Data segments used for the current allocation. */
    unsigned     cSegsUsed;
    /** Pointer to I/O buffer manager. */
    PIOBUFMGRINT pIoBufMgr;
} IOBUFDESCINT;

/**
 * A
 */
typedef struct IOBUFMGRBIN
{
    /** Index of the next free entry. */
    unsigned            iFree;
    /** Pointer to the array of free objects for this bin. */
    void              **papvFree;
} IOBUFMGRBIN;
typedef IOBUFMGRBIN *PIOBUFMGRBIN;

/**
 * Internal I/O buffer manager data.
 */
typedef struct IOBUFMGRINT
{
    /** Critical section protecting the allocation path. */
    RTCRITSECT          CritSectAlloc;
    /** Flags the manager was created with. */
    uint32_t            fFlags;
    /** Maximum size of I/O memory to allocate. */
    size_t              cbMax;
    /** Amount of free memory. */
    size_t              cbFree;
    /** The order of smallest bin. */
    uint32_t            u32OrderMin;
    /** The order of largest bin. */
    uint32_t            u32OrderMax;
    /** Pointer to the base memory of the allocation. */
    void               *pvMem;
    /** Number of bins for free objects. */
    uint32_t            cBins;
    /** Flag whether allocation is on hold waiting for everything to be free
     * to be able to defragment the memory. */
    bool                fAllocSuspended;
    /** Array of bins. */
    PIOBUFMGRBIN        paBins;
    /** Array of pointer entries for the various bins - variable in size. */
    void               *apvObj[1];
} IOBUFMGRINT;

/* Must be included after IOBUFDESCINT was defined. */
#define IOBUFDESCINT_DECLARED
#include "IOBufMgmt.h"

/**
 * Gets the number of bins required between the given minimum and maximum size
 * to have a bin for every power of two size inbetween.
 *
 * @returns The number of bins required.
 * @param   cbMin       The size of the smallest bin.
 * @param   cbMax       The size of the largest bin.
 */
DECLINLINE(uint32_t) iobufMgrGetBinCount(uint32_t cbMin, uint32_t cbMax)
{
    uint32_t u32Max = ASMBitLastSetU32(cbMax);
    uint32_t u32Min = ASMBitLastSetU32(cbMin);

    Assert(cbMax >= cbMin && cbMax != 0 && cbMin != 0);
    return u32Max - u32Min + 1;
}

/**
 * Returns the number of entries required in the object array to cover all bins.
 *
 * @returns Number of entries required in the object array.
 * @param   cbMem       Size of the memory buffer.
 * @param   cBins       Number of bins available.
 * @param   cbBinMin    Minimum object size.
 */
DECLINLINE(uint32_t) iobufMgrGetObjCount(size_t cbMem, unsigned cBins, size_t cbMinBin)
{
    size_t cObjs = 0;
    size_t cbBin = cbMinBin;

    while (cBins-- > 0)
    {
        cObjs += cbMem / cbBin;
        cbBin <<= 1; /* The size doubles for every bin. */
    }

    Assert((uint32_t)cObjs == cObjs);
    return (uint32_t)cObjs;
}

/**
 * Resets the bins to factory default (memory resigin in the largest bin).
 *
 * @returns nothing.
 * @param   pThis       The I/O buffer manager instance.
 */
static void iobufMgrResetBins(PIOBUFMGRINT pThis)
{
    /* Init the bins. */
    size_t   cbMax = pThis->cbMax;
    size_t   iObj  = 0;
    uint32_t cbBin = IOBUFMGR_BIN_SIZE_MIN;
    for (unsigned i = 0; i < pThis->cBins; i++)
    {
        PIOBUFMGRBIN pBin = &pThis->paBins[i];
        pBin->iFree = 0;
        pBin->papvFree = &pThis->apvObj[iObj];
        iObj += cbMax / cbBin;

        /* Init the biggest possible bin with the free objects. */
        if (   (cbBin << 1) > cbMax
            || i == pThis->cBins - 1)
        {
            uint8_t *pbMem = (uint8_t *)pThis->pvMem;
            while (cbMax)
            {
                pBin->papvFree[pBin->iFree] = pbMem;
                cbMax -= cbBin;
                pbMem += cbBin;
                pBin->iFree++;

                if (cbMax < cbBin) /** @todo Populate smaller bins and don't waste memory. */
                    break;
            }

            /* Limit the number of available bins. */
            pThis->cBins = i + 1;
            break;
        }

        cbBin <<= 1;
    }
}

/**
 * Allocate one segment from the manager.
 *
 * @returns Number of bytes allocated, 0 if there is no free memory.
 * @param   pThis       The I/O buffer manager instance.
 * @param   pSeg        The segment to fill in on success.
 * @param   cb          Maximum number of bytes to allocate.
 */
static size_t iobufMgrAllocSegment(PIOBUFMGRINT pThis, PRTSGSEG pSeg, size_t cb)
{
    size_t cbAlloc = 0;

    /* Round to the next power of two and get the bin to try first. */
    uint32_t u32Order = ASMBitLastSetU32((uint32_t)cb) - 1;
    if (cbAlloc & ~RT_BIT_32(u32Order))
        u32Order <<= 1;

    u32Order = RT_CLAMP(u32Order, pThis->u32OrderMin, pThis->u32OrderMax);
    unsigned iBin = u32Order - pThis->u32OrderMin;

    /*
     * Check whether the bin can satisfy the request. If not try the next bigger
     * bin and so on. If there is nothing to find try the smaller bins.
     */
    Assert(iBin < pThis->cBins);

    PIOBUFMGRBIN pBin = &pThis->paBins[iBin];
    if (pBin->iFree == 0)
    {
        unsigned iBinCur = iBin;
        PIOBUFMGRBIN pBinCur = &pThis->paBins[iBinCur];

        while (iBinCur < pThis->cBins)
        {
            if (pBinCur->iFree != 0)
            {
                pBinCur->iFree--;
                uint8_t *pbMem = (uint8_t *)pBinCur->papvFree[pBinCur->iFree];
                AssertPtr(pbMem);

                /* Always split into half. */
                while (iBinCur > iBin)
                {
                    iBinCur--;
                    pBinCur = &pThis->paBins[iBinCur];
                    pBinCur->papvFree[pBinCur->iFree] = pbMem + (size_t)RT_BIT(iBinCur + pThis->u32OrderMin); /* (RT_BIT causes weird MSC warning without cast) */
                    pBinCur->iFree++;
                }

                /* For the last bin we will get two new memory blocks. */
                pBinCur->papvFree[pBinCur->iFree] = pbMem;
                pBinCur->iFree++;
                Assert(pBin == pBinCur);
                break;
            }

            pBinCur++;
            iBinCur++;
        }
    }

    /*
     * If we didn't find something in the higher bins try to accumulate as much as possible from the smaller bins.
     */
    if (   pBin->iFree == 0
        && iBin > 0)
    {
#if 1
        pThis->fAllocSuspended = true;
#else
        do
        {
            iBin--;
            pBin = &pThis->paBins[iBin];

            if (pBin->iFree != 0)
            {
                pBin->iFree--;
                pSeg->pvSeg = pBin->papvFree[pBin->iFree];
                pSeg->cbSeg = (size_t)RT_BIT_32(iBin + pThis->u32OrderMin);
                AssertPtr(pSeg->pvSeg);
                cbAlloc = pSeg->cbSeg;
                break;
            }
        }
        while (iBin > 0);
#endif
    }
    else if (pBin->iFree != 0)
    {
        pBin->iFree--;
        pSeg->pvSeg = pBin->papvFree[pBin->iFree];
        pSeg->cbSeg = (size_t)RT_BIT_32(u32Order);
        cbAlloc = pSeg->cbSeg;
        AssertPtr(pSeg->pvSeg);
    }

    return cbAlloc;
}

DECLHIDDEN(int) IOBUFMgrCreate(PIOBUFMGR phIoBufMgr, size_t cbMax, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;

    AssertPtrReturn(phIoBufMgr, VERR_INVALID_POINTER);
    AssertReturn(cbMax, VERR_NOT_IMPLEMENTED);

    /* Allocate the basic structure in one go. */
    unsigned cBins = iobufMgrGetBinCount(IOBUFMGR_BIN_SIZE_MIN, IOBUFMGR_BIN_SIZE_MAX);
    uint32_t cObjs = iobufMgrGetObjCount(cbMax, cBins, IOBUFMGR_BIN_SIZE_MIN);
    PIOBUFMGRINT pThis = (PIOBUFMGRINT)RTMemAllocZ(RT_OFFSETOF(IOBUFMGRINT, apvObj[cObjs]) + cBins * sizeof(IOBUFMGRBIN));
    if (RT_LIKELY(pThis))
    {
        pThis->fFlags          = fFlags;
        pThis->cbMax           = cbMax;
        pThis->cbFree          = cbMax;
        pThis->cBins           = cBins;
        pThis->fAllocSuspended = false;
        pThis->u32OrderMin     = ASMBitLastSetU32(IOBUFMGR_BIN_SIZE_MIN) - 1;
        pThis->u32OrderMax     = ASMBitLastSetU32(IOBUFMGR_BIN_SIZE_MAX) - 1;
        pThis->paBins = (PIOBUFMGRBIN)((uint8_t *)pThis + RT_OFFSETOF(IOBUFMGRINT, apvObj[cObjs]));

        rc = RTCritSectInit(&pThis->CritSectAlloc);
        if (RT_SUCCESS(rc))
        {
            if (pThis->fFlags & IOBUFMGR_F_REQUIRE_NOT_PAGABLE)
                rc = RTMemSaferAllocZEx(&pThis->pvMem, RT_ALIGN_Z(pThis->cbMax, _4K),
                                        RTMEMSAFER_F_REQUIRE_NOT_PAGABLE);
            else
                pThis->pvMem = RTMemPageAllocZ(RT_ALIGN_Z(pThis->cbMax, _4K));

            if (   RT_LIKELY(pThis->pvMem)
                && RT_SUCCESS(rc))
            {
                iobufMgrResetBins(pThis);

                *phIoBufMgr = pThis;
                return VINF_SUCCESS;
            }
            else
                rc = VERR_NO_MEMORY;

            RTCritSectDelete(&pThis->CritSectAlloc);
        }

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

DECLHIDDEN(int) IOBUFMgrDestroy(IOBUFMGR hIoBufMgr)
{
    PIOBUFMGRINT pThis = hIoBufMgr;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    int rc = RTCritSectEnter(&pThis->CritSectAlloc);
    if (RT_SUCCESS(rc))
    {
        if (pThis->cbFree == pThis->cbMax)
        {
            if (pThis->fFlags & IOBUFMGR_F_REQUIRE_NOT_PAGABLE)
                RTMemSaferFree(pThis->pvMem, RT_ALIGN_Z(pThis->cbMax, _4K));
            else
                RTMemPageFree(pThis->pvMem, RT_ALIGN_Z(pThis->cbMax, _4K));

            RTCritSectLeave(&pThis->CritSectAlloc);
            RTCritSectDelete(&pThis->CritSectAlloc);
            RTMemFree(pThis);
        }
        else
        {
            rc = VERR_INVALID_STATE;
            RTCritSectLeave(&pThis->CritSectAlloc);
        }
    }

    return rc;
}

DECLHIDDEN(int) IOBUFMgrAllocBuf(IOBUFMGR hIoBufMgr, PIOBUFDESC pIoBufDesc, size_t cbIoBuf,
                                 size_t *pcbIoBufAllocated)
{
    PIOBUFMGRINT pThis = hIoBufMgr;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(cbIoBuf > 0, VERR_INVALID_PARAMETER);

    if (!pThis->cbFree)
        return VERR_NO_MEMORY;

    int rc = RTCritSectEnter(&pThis->CritSectAlloc);
    if (RT_SUCCESS(rc))
    {
        unsigned iSeg = 0;
        size_t   cbLeft = cbIoBuf;
        PRTSGSEG pSeg = &pIoBufDesc->Int.aSegs[0];

        while (   iSeg < RT_ELEMENTS(pIoBufDesc->Int.aSegs)
               && cbLeft)
        {
            size_t cbAlloc = iobufMgrAllocSegment(pThis, pSeg, cbLeft);
            if (!cbAlloc)
                break;

            iSeg++;
            pSeg++;
            cbLeft -= RT_MIN(cbAlloc, cbLeft);
        }

        if (iSeg)
            RTSgBufInit(&pIoBufDesc->SgBuf, &pIoBufDesc->Int.aSegs[0], iSeg);
        else
            rc = VERR_NO_MEMORY;

        pIoBufDesc->Int.cSegsUsed = iSeg;
        pIoBufDesc->Int.pIoBufMgr = pThis;
        *pcbIoBufAllocated = cbIoBuf - cbLeft;
        Assert(   (RT_SUCCESS(rc) && *pcbIoBufAllocated > 0)
               || RT_FAILURE(rc));

        pThis->cbFree -= cbIoBuf - cbLeft;

        RTCritSectLeave(&pThis->CritSectAlloc);
    }

    return rc;
}

DECLHIDDEN(void) IOBUFMgrFreeBuf(PIOBUFDESC pIoBufDesc)
{
    PIOBUFMGRINT pThis = pIoBufDesc->Int.pIoBufMgr;

    AssertPtr(pThis);

    int rc = RTCritSectEnter(&pThis->CritSectAlloc);
    AssertRC(rc);

    if (RT_SUCCESS(rc))
    {
        for (unsigned i = 0; i < pIoBufDesc->Int.cSegsUsed; i++)
        {
            PRTSGSEG pSeg = &pIoBufDesc->Int.aSegs[i];

            uint32_t u32Order = ASMBitLastSetU32((uint32_t)pSeg->cbSeg) - 1;
            unsigned iBin = u32Order - pThis->u32OrderMin;

            Assert(iBin < pThis->cBins);
            PIOBUFMGRBIN pBin = &pThis->paBins[iBin];
            pBin->papvFree[pBin->iFree] = pSeg->pvSeg;
            pBin->iFree++;
            pThis->cbFree += (size_t)RT_BIT_32(u32Order);
        }

        if (   pThis->cbFree == pThis->cbMax
            && pThis->fAllocSuspended)
        {
            iobufMgrResetBins(pThis);
            pThis->fAllocSuspended = false;
        }

        RTCritSectLeave(&pThis->CritSectAlloc);
    }

    pIoBufDesc->Int.cSegsUsed = 0;
}

