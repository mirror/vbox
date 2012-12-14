/* $Id$ */
/** @file
 * IPRT - S/G buffer handling.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/sg.h>
#include <iprt/string.h>
#include <iprt/assert.h>


static void *sgBufGet(PRTSGBUF pSgBuf, size_t *pcbData)
{
    size_t cbData;
    void *pvBuf;

    /* Check that the S/G buffer has memory left. */
    if (RT_UNLIKELY(   pSgBuf->idxSeg == pSgBuf->cSegs
                    && !pSgBuf->cbSegLeft))
    {
        *pcbData = 0;
        return NULL;
    }

    AssertReleaseMsg(      pSgBuf->cbSegLeft <= 32 * _1M
                     &&    (uintptr_t)pSgBuf->pvSegCur                     >= (uintptr_t)pSgBuf->paSegs[pSgBuf->idxSeg].pvSeg
                     &&    (uintptr_t)pSgBuf->pvSegCur + pSgBuf->cbSegLeft <= (uintptr_t)pSgBuf->paSegs[pSgBuf->idxSeg].pvSeg + pSgBuf->paSegs[pSgBuf->idxSeg].cbSeg,
                     ("pSgBuf->idxSeg=%d pSgBuf->cSegs=%d pSgBuf->pvSegCur=%p pSgBuf->cbSegLeft=%zd pSgBuf->paSegs[%d].pvSeg=%p pSgBuf->paSegs[%d].cbSeg=%zd\n",
                      pSgBuf->idxSeg, pSgBuf->cSegs, pSgBuf->pvSegCur, pSgBuf->cbSegLeft,
                      pSgBuf->idxSeg, pSgBuf->paSegs[pSgBuf->idxSeg].pvSeg, pSgBuf->idxSeg, pSgBuf->paSegs[pSgBuf->idxSeg].cbSeg));

    cbData = RT_MIN(*pcbData, pSgBuf->cbSegLeft);
    pvBuf  = pSgBuf->pvSegCur;
    pSgBuf->cbSegLeft -= cbData;

    /* Advance to the next segment if required. */
    if (!pSgBuf->cbSegLeft)
    {
        pSgBuf->idxSeg++;

        if (pSgBuf->idxSeg < pSgBuf->cSegs)
        {
            pSgBuf->pvSegCur  = pSgBuf->paSegs[pSgBuf->idxSeg].pvSeg;
            pSgBuf->cbSegLeft = pSgBuf->paSegs[pSgBuf->idxSeg].cbSeg;
        }

        *pcbData = cbData;
    }
    else
        pSgBuf->pvSegCur = (uint8_t *)pSgBuf->pvSegCur + cbData;

    return pvBuf;
}


RTDECL(void) RTSgBufInit(PRTSGBUF pSgBuf, PCRTSGSEG paSegs, size_t cSegs)
{
    AssertPtr(pSgBuf);
    AssertPtr(paSegs);
    Assert(cSegs > 0);
    Assert(cSegs < (~(unsigned)0 >> 1));

    pSgBuf->paSegs    = paSegs;
    pSgBuf->cSegs     = (unsigned)cSegs;
    pSgBuf->idxSeg    = 0;
    pSgBuf->pvSegCur  = paSegs[0].pvSeg;
    pSgBuf->cbSegLeft = paSegs[0].cbSeg;
}


RTDECL(void) RTSgBufReset(PRTSGBUF pSgBuf)
{
    AssertPtrReturnVoid(pSgBuf);

    pSgBuf->idxSeg    = 0;
    pSgBuf->pvSegCur  = pSgBuf->paSegs[0].pvSeg;
    pSgBuf->cbSegLeft = pSgBuf->paSegs[0].cbSeg;
}


RTDECL(void) RTSgBufClone(PRTSGBUF pSgBufTo, PCRTSGBUF pSgBufFrom)
{
    AssertPtr(pSgBufTo);
    AssertPtr(pSgBufFrom);

    pSgBufTo->paSegs    = pSgBufFrom->paSegs;
    pSgBufTo->cSegs     = pSgBufFrom->cSegs;
    pSgBufTo->idxSeg    = pSgBufFrom->idxSeg;
    pSgBufTo->pvSegCur  = pSgBufFrom->pvSegCur;
    pSgBufTo->cbSegLeft = pSgBufFrom->cbSegLeft;
}


RTDECL(void *) RTSgBufGetNextSegment(PRTSGBUF pSgBuf, size_t *pcbSeg)
{
    AssertPtrReturn(pSgBuf, NULL);
    AssertPtrReturn(pcbSeg, NULL);

    if (!*pcbSeg)
        *pcbSeg = pSgBuf->cbSegLeft;

    return sgBufGet(pSgBuf, pcbSeg);
}


RTDECL(size_t) RTSgBufCopy(PRTSGBUF pSgBufDst, PRTSGBUF pSgBufSrc, size_t cbCopy)
{
    AssertPtrReturn(pSgBufDst, 0);
    AssertPtrReturn(pSgBufSrc, 0);

    size_t cbLeft = cbCopy;

    while (cbLeft)
    {
        size_t cbThisCopy = RT_MIN(RT_MIN(pSgBufDst->cbSegLeft, cbLeft), pSgBufSrc->cbSegLeft);
        size_t cbTmp = cbThisCopy;
        void *pvBufDst;
        void *pvBufSrc;

        if (!cbThisCopy)
            break;

        pvBufDst = sgBufGet(pSgBufDst, &cbTmp);
        Assert(cbTmp == cbThisCopy);
        pvBufSrc = sgBufGet(pSgBufSrc, &cbTmp);
        Assert(cbTmp == cbThisCopy);

        memcpy(pvBufDst, pvBufSrc, cbThisCopy);

        cbLeft -= cbThisCopy;
    }

    return cbCopy - cbLeft;
}


RTDECL(int) RTSgBufCmp(PCRTSGBUF pSgBuf1, PCRTSGBUF pSgBuf2, size_t cbCmp)
{
    AssertPtrReturn(pSgBuf1, 0);
    AssertPtrReturn(pSgBuf2, 0);

    size_t cbLeft = cbCmp;
    RTSGBUF SgBuf1;
    RTSGBUF SgBuf2;

    /* Set up the temporary buffers */
    RTSgBufClone(&SgBuf1, pSgBuf1);
    RTSgBufClone(&SgBuf2, pSgBuf2);

    while (cbLeft)
    {
        size_t cbThisCmp = RT_MIN(RT_MIN(SgBuf1.cbSegLeft, cbLeft), SgBuf2.cbSegLeft);
        size_t cbTmp = cbThisCmp;
        void *pvBuf1;
        void *pvBuf2;

        if (!cbCmp)
            break;

        pvBuf1 = sgBufGet(&SgBuf1, &cbTmp);
        Assert(cbTmp == cbThisCmp);
        pvBuf2 = sgBufGet(&SgBuf2, &cbTmp);
        Assert(cbTmp == cbThisCmp);

        int rc = memcmp(pvBuf1, pvBuf2, cbThisCmp);
        if (rc)
            return rc;

        cbLeft -= cbThisCmp;
    }

    return 0;
}


RTDECL(int) RTSgBufCmpEx(PRTSGBUF pSgBuf1, PRTSGBUF pSgBuf2, size_t cbCmp,
                         size_t *pcbOff, bool fAdvance)
{
    AssertPtrReturn(pSgBuf1, 0);
    AssertPtrReturn(pSgBuf2, 0);

    size_t cbLeft = cbCmp;
    size_t cbOff  = 0;
    RTSGBUF SgBuf1Tmp;
    RTSGBUF SgBuf2Tmp;
    PRTSGBUF pSgBuf1Tmp;
    PRTSGBUF pSgBuf2Tmp;

    if (!fAdvance)
    {
        /* Set up the temporary buffers */
        RTSgBufClone(&SgBuf1Tmp, pSgBuf1);
        RTSgBufClone(&SgBuf2Tmp, pSgBuf2);
        pSgBuf1Tmp = &SgBuf1Tmp;
        pSgBuf2Tmp = &SgBuf2Tmp;
    }
    else
    {
        pSgBuf1Tmp = pSgBuf1;
        pSgBuf2Tmp = pSgBuf2;
    }

    while (cbLeft)
    {
        size_t cbThisCmp = RT_MIN(RT_MIN(pSgBuf1Tmp->cbSegLeft, cbLeft), pSgBuf2Tmp->cbSegLeft);
        size_t cbTmp = cbThisCmp;
        uint8_t *pbBuf1;
        uint8_t *pbBuf2;

        if (!cbCmp)
            break;

        pbBuf1 = (uint8_t *)sgBufGet(pSgBuf1Tmp, &cbTmp);
        Assert(cbTmp == cbThisCmp);
        pbBuf2 = (uint8_t *)sgBufGet(pSgBuf2Tmp, &cbTmp);
        Assert(cbTmp == cbThisCmp);

        int rc = memcmp(pbBuf1, pbBuf2, cbThisCmp);
        if (rc)
        {
            if (pcbOff)
            {
                /* Search for the correct offset */
                while (   cbThisCmp-- > 0
                       && (*pbBuf1 == *pbBuf2))
                {
                    pbBuf1++;
                    pbBuf2++;
                    cbOff++;
                }

                *pcbOff = cbOff;
            }
            return rc;
        }

        cbLeft -= cbThisCmp;
        cbOff  += cbThisCmp;
    }

    return 0;
}


RTDECL(size_t) RTSgBufSet(PRTSGBUF pSgBuf, uint8_t ubFill, size_t cbSet)
{
    AssertPtrReturn(pSgBuf, 0);

    size_t cbLeft = cbSet;

    while (cbLeft)
    {
        size_t cbThisSet = cbLeft;
        void *pvBuf = sgBufGet(pSgBuf, &cbThisSet);

        if (!cbThisSet)
            break;

        memset(pvBuf, ubFill, cbThisSet);

        cbLeft -= cbThisSet;
    }

    return cbSet - cbLeft;
}


RTDECL(size_t) RTSgBufCopyToBuf(PRTSGBUF pSgBuf, void *pvBuf, size_t cbCopy)
{
    AssertPtrReturn(pSgBuf, 0);
    AssertPtrReturn(pvBuf, 0);

    size_t cbLeft = cbCopy;

    while (cbLeft)
    {
        size_t cbThisCopy = cbLeft;
        void *pvSrc = sgBufGet(pSgBuf, &cbThisCopy);

        if (!cbThisCopy)
            break;

        memcpy(pvBuf, pvSrc, cbThisCopy);

        cbLeft -= cbThisCopy;
        pvBuf = (void *)((uintptr_t)pvBuf + cbThisCopy);
    }

    return cbCopy - cbLeft;
}


RTDECL(size_t) RTSgBufCopyFromBuf(PRTSGBUF pSgBuf, void *pvBuf, size_t cbCopy)
{
    AssertPtrReturn(pSgBuf, 0);
    AssertPtrReturn(pvBuf, 0);

    size_t cbLeft = cbCopy;

    while (cbLeft)
    {
        size_t cbThisCopy = cbLeft;
        void *pvDst = sgBufGet(pSgBuf, &cbThisCopy);

        if (!cbThisCopy)
            break;

        memcpy(pvDst, pvBuf, cbThisCopy);

        cbLeft -= cbThisCopy;
        pvBuf = (void *)((uintptr_t)pvBuf + cbThisCopy);
    }

    return cbCopy - cbLeft;
}


RTDECL(size_t) RTSgBufAdvance(PRTSGBUF pSgBuf, size_t cbAdvance)
{
    AssertPtrReturn(pSgBuf, 0);

    size_t cbLeft = cbAdvance;

    while (cbLeft)
    {
        size_t cbThisAdvance = cbLeft;
        void *pv = sgBufGet(pSgBuf, &cbThisAdvance);

        NOREF(pv);

        if (!cbThisAdvance)
            break;

        cbLeft -= cbThisAdvance;
    }

    return cbAdvance - cbLeft;
}


RTDECL(size_t) RTSgBufSegArrayCreate(PRTSGBUF pSgBuf, PRTSGSEG paSeg, unsigned *pcSeg, size_t cbData)
{
    AssertPtrReturn(pSgBuf, 0);
    AssertPtrReturn(pcSeg, 0);

    unsigned cSeg = 0;
    size_t   cb = 0;

    if (!paSeg)
    {
        if (pSgBuf->cbSegLeft > 0)
        {
            size_t idx = pSgBuf->idxSeg;
            cSeg = 1;

            cb     += RT_MIN(pSgBuf->cbSegLeft, cbData);
            cbData -= RT_MIN(pSgBuf->cbSegLeft, cbData);

            while (   cbData
                   && idx < pSgBuf->cSegs - 1)
            {
                idx++;
                cSeg++;
                cb     += RT_MIN(pSgBuf->paSegs[idx].cbSeg, cbData);
                cbData -= RT_MIN(pSgBuf->paSegs[idx].cbSeg, cbData);
            }
        }
    }
    else
    {
        while (   cbData
               && cSeg < *pcSeg)
        {
            size_t  cbThisSeg = cbData;
            void   *pvSeg     = NULL;

            pvSeg = sgBufGet(pSgBuf, &cbThisSeg);

            if (!cbThisSeg)
            {
                Assert(!pvSeg);
                break;
            }

            AssertMsg(cbThisSeg <= cbData, ("Impossible!\n"));

            paSeg[cSeg].cbSeg = cbThisSeg;
            paSeg[cSeg].pvSeg = pvSeg;
            cSeg++;
            cbData -= cbThisSeg;
            cb     += cbThisSeg;
        }
    }

    *pcSeg = cSeg;

    return cb;
}

