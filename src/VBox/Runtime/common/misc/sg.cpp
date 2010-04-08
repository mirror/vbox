/* $Id$ */
/** @file
 * IPRT - S/G buffer handling.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/sg.h>
#include <iprt/string.h>
#include <iprt/assert.h>


static void *sgBufGet(PRTSGBUF pSgBuf, size_t *pcbData)
{
    size_t cbData = RT_MIN(*pcbData, pSgBuf->cbSegLeft);
    void *pvBuf = pSgBuf->pvSegCurr;

    pSgBuf->cbSegLeft -= cbData;

    /* Advance to the next segment if required. */
    if (!pSgBuf->cbSegLeft)
    {
        pSgBuf->idxSeg++;

        if (RT_UNLIKELY(pSgBuf->idxSeg == pSgBuf->cSeg))
        {
            pSgBuf->cbSegLeft = 0;
            pSgBuf->pvSegCurr = NULL;
        }
        else
        {
            pSgBuf->pvSegCurr = pSgBuf->pcaSeg[pSgBuf->idxSeg].pvSeg;
            pSgBuf->cbSegLeft = pSgBuf->pcaSeg[pSgBuf->idxSeg].cbSeg;
        }

        *pcbData = cbData;
    }
    else
        pSgBuf->pvSegCurr = (void *)((uintptr_t)pSgBuf->pvSegCurr + cbData);

    return pvBuf;
}


RTDECL(void) RTSgBufInit(PRTSGBUF pSgBuf, PCRTSGSEG pcaSeg, unsigned cSeg)
{
    AssertPtrReturnVoid(pSgBuf);
    AssertPtrReturnVoid(pcaSeg);
    AssertReturnVoid(cSeg > 0);

    pSgBuf->pcaSeg    = pcaSeg;
    pSgBuf->cSeg      = cSeg;
    pSgBuf->idxSeg    = 0;
    pSgBuf->pvSegCurr = pcaSeg[0].pvSeg;
    pSgBuf->cbSegLeft = pcaSeg[0].cbSeg;
}


RTDECL(void) RTSgBufReset(PRTSGBUF pSgBuf)
{
    AssertPtrReturnVoid(pSgBuf);

    pSgBuf->idxSeg    = 0;
    pSgBuf->pvSegCurr = pSgBuf->pcaSeg[0].pvSeg;
    pSgBuf->cbSegLeft = pSgBuf->pcaSeg[0].cbSeg;
}


RTDECL(void) RTSgBufClone(PRTSGBUF pSgBufTo, PCRTSGBUF pSgBufFrom)
{
    AssertPtrReturnVoid(pSgBufTo);
    AssertPtrReturnVoid(pSgBufFrom);

    pSgBufTo->pcaSeg    = pSgBufFrom->pcaSeg;
    pSgBufTo->cSeg      = pSgBufFrom->cSeg;
    pSgBufTo->idxSeg    = pSgBufFrom->idxSeg;
    pSgBufTo->pvSegCurr = pSgBufFrom->pvSegCurr;
    pSgBufTo->cbSegLeft = pSgBufFrom->cbSegLeft;
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

        if (!cbCopy)
            break;

        pvBufDst = sgBufGet(pSgBufDst, &cbTmp);
        Assert(cbTmp == cbThisCopy);
        pvBufSrc = sgBufGet(pSgBufDst, &cbTmp);
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

        int rc = memcmp(pvBuf1, pvBuf1, cbThisCmp);
        if (rc)
            return rc;

        cbLeft -= cbThisCmp;
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
    AssertPtrReturn(paSeg, 0);
    AssertPtrReturn(pcSeg, 0);

    size_t   cb = 0;
    unsigned cSeg = 0;

    while (   cbData
           && cSeg < *pcSeg)
    {
        size_t  cbThisSeg = cbData;
        void   *pvSeg     = NULL;

        pvSeg = sgBufGet(pSgBuf, &cbThisSeg);

        AssertMsg(cbThisSeg <= cbData, ("Impossible!\n"));

        paSeg[cSeg].cbSeg = cbThisSeg;
        paSeg[cSeg].pvSeg = pvSeg;
        cSeg++;
        cbData -= cbThisSeg;
        cb     += cbThisSeg;
    }

    *pcSeg = cSeg;

    return cb;
}

