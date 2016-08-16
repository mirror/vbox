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
    unsigned            cBins;
    /** Array of bins. */
    PIOBUFMGRBIN        paBins;
    /** Array of pointer entries for the various bins - variable in size. */
    void               *apvObj[1];
} IOBUFMGRINT;

/* Must be included after IOBUFDESCINT was defined. */
#define IOBUFDESCINT_DECLARED
#include "IOBufMgmt.h"

DECLINLINE(unsigned) iobufMgrGetBinCount(size_t cbMin, size_t cbMax)
{
    unsigned u32Max = ASMBitLastSetU32((uint32_t)cbMax);
    unsigned u32Min = ASMBitLastSetU32((uint32_t)cbMin);

    Assert(cbMax >= cbMin && cbMax != 0 && cbMin != 0);
    return u32Max - u32Min + 1;
}

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
        pThis->fFlags = fFlags;
        pThis->cbMax  = cbMax;
        pThis->cbFree = cbMax;
        pThis->cBins  = cBins;
        pThis->u32OrderMin = ASMBitLastSetU32(IOBUFMGR_BIN_SIZE_MIN) - 1;
        pThis->u32OrderMax = ASMBitLastSetU32(IOBUFMGR_BIN_SIZE_MAX) - 1;
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
                /* Init the bins. */
                size_t   iObj  = 0;
                uint32_t cbBin = IOBUFMGR_BIN_SIZE_MIN;
                for (unsigned i = 0; i < cBins; i++)
                {
                    PIOBUFMGRBIN pBin = &pThis->paBins[i];
                    pBin->iFree = 0;
                    pBin->papvFree = &pThis->apvObj[iObj];
                    iObj += cbMax / cbBin;

                    /* Init the biggest possible bin with the free objects. */
                    if (   (cbBin << 1) > cbMax
                        || i == cBins - 1)
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
    /** @todo Defragment in case there is enough memory free but we can't find something in our bin. */
    if (   pBin->iFree == 0
        && iBin > 0)
    {
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

DECLHIDDEN(int) IOBUFMgrAllocBuf(IOBUFMGR hIoBufMgr, PIOBUFDESC pIoBufDesc, size_t cbIoBuf,
                                 size_t *pcbIoBufAllocated)
{
    PIOBUFMGRINT pThis = hIoBufMgr;

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

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
        RTCritSectLeave(&pThis->CritSectAlloc);
    }

    pIoBufDesc->Int.cSegsUsed = 0;
}

