/* $Id$ */
/** @file
 * IPRT Testcase - S/G Buffers.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/sg.h>

#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST g_hTest;



static void testEmpty(void)
{
    RTTestSub(g_hTest, "empty");

    RTSGBUF SgBuf;
    RTSgBufInit(&SgBuf, NULL, 0);
    RTTESTI_CHECK(RTSgBufCalcTotalLength(&SgBuf) == 0);
    RTTESTI_CHECK(RTSgBufCalcLengthLeft(&SgBuf) == 0);
    RTTESTI_CHECK(RTSgBufIsAtStart(&SgBuf));
    RTTESTI_CHECK(RTSgBufIsAtEnd(&SgBuf));
    RTTESTI_CHECK(!RTSgBufIsAtStartOfSegment(&SgBuf));
    size_t cbIgn;
    RTTESTI_CHECK(RTSgBufGetCurrentSegment(&SgBuf, _1K, &cbIgn) == NULL);
    RTTESTI_CHECK(RTSgBufGetCurrentSegment(&SgBuf, 0, &cbIgn) == NULL);

    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufCalcTotalLength(&SgBuf) == 0);
    RTTESTI_CHECK(RTSgBufCalcLengthLeft(&SgBuf) == 0);
    RTTESTI_CHECK(RTSgBufIsAtStart(&SgBuf));
    RTTESTI_CHECK(RTSgBufIsAtEnd(&SgBuf));
    RTTESTI_CHECK(!RTSgBufIsAtStartOfSegment(&SgBuf));

    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufAdvance(&SgBuf, _1K) == 0);
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufAdvance(&SgBuf, 0) == 0);
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufSet(&SgBuf, 0xf6, _1K) == 0);
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufSet(&SgBuf, 0xf6, 0) == 0);
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufGetNextSegment(&SgBuf, &cbIgn) == NULL);
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufIsZero(&SgBuf, 0));
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufIsZero(&SgBuf, _1K));
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufIsZero(&SgBuf, ~(size_t)0));

    RTSGBUF SgBuf2;
    RTSgBufInit(&SgBuf2, NULL, 0);

    RTTESTI_CHECK(RTSgBufCmp(&SgBuf, &SgBuf2, _1K) == 0);
    RTTESTI_CHECK(RTSgBufCmp(&SgBuf, &SgBuf2, 64) == 0);
    RTTESTI_CHECK(RTSgBufCmp(&SgBuf, &SgBuf2, 0) == 0);

    uint8_t abTmp[64] = { 0xf7, };
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufCopyToBuf(&SgBuf, abTmp, sizeof(abTmp)) == 0);
    RTSgBufReset(&SgBuf);
    RTTESTI_CHECK(RTSgBufCopyFromBuf(&SgBuf, abTmp, sizeof(abTmp)) == 0);
}


static PRTSGBUF createRandBuffer(unsigned cMaxSegs, size_t cbMin, size_t cbMax, bool fAllowZeroSegs, size_t *pcbTotal)
{
    unsigned const cSegs   = RTRandU32Ex(1, cMaxSegs);
    size_t const   cbTotal = (size_t)RTRandU64Ex(RT_MAX(cbMin, fAllowZeroSegs ? 1 : cSegs), cbMax);
    PRTSGBUF       pSgBuf  = (PRTSGBUF)RTMemAlloc(cSegs * sizeof(RTSGSEG) + sizeof(*pSgBuf) + cbTotal);
    if (pSgBuf)
    {
        PRTSGSEG   paSegs   = (PRTSGSEG)(pSgBuf + 1);
        uint8_t   *pbBytes  = (uint8_t *)&paSegs[cSegs];
        RTRandBytes(pbBytes, cbTotal);

        size_t     cbLeft   = cbTotal;
        unsigned   idxSeg;
        for (idxSeg = 0; idxSeg < cSegs - 1; idxSeg++)
        {
            size_t const cbSeg = cbLeft ? (size_t)RTRandU64Ex(fAllowZeroSegs ? 0 : 1, cbLeft) : 0;
            paSegs[idxSeg].cbSeg = cbSeg;
            paSegs[idxSeg].pvSeg = pbBytes;
            pbBytes += cbSeg;
            cbLeft  -= cbSeg;
        }
        paSegs[idxSeg].cbSeg = cbLeft;
        paSegs[idxSeg].pvSeg = pbBytes;

        RTSgBufInit(pSgBuf, paSegs, cSegs);
        RTTESTI_CHECK(RTSgBufCalcTotalLength(pSgBuf) == cbTotal);

        *pcbTotal = cbTotal;
        return pSgBuf;
    }
    RTTestFailed(g_hTest, "RTMemAlloc failed: cSegs=%u cbTotal=%#zx", cSegs, cbTotal);
    *pcbTotal = 0;
    return NULL;
}


static void testBasic(void)
{
    RTTestSub(g_hTest, "basics");

    for (unsigned iBufVar = 0; iBufVar < 64; iBufVar++)
    {
        size_t   cbSgBuf1;
        PRTSGBUF pSgBuf1 = createRandBuffer(16, 32, _1M, true, &cbSgBuf1);
        RTTESTI_CHECK_RETV(pSgBuf1);

        /* Do random advancing using RTSgBufAdvance. */
        for (unsigned iRun = 0; iRun < _1K; iRun++)
        {
            RTTESTI_CHECK(RTSgBufIsAtStart(pSgBuf1));
            RTTESTI_CHECK(RTSgBufIsAtStartOfSegment(pSgBuf1));
            RTTESTI_CHECK(RTSgBufCalcLengthLeft(pSgBuf1) == cbSgBuf1);

            size_t cbLeft = cbSgBuf1;
            while (cbLeft > 0)
            {
                size_t cbToAdv = (size_t)RTRandU64Ex(0, iRun & 1 ? cbSgBuf1 * 2 : cbLeft);
                size_t cbActual = RTSgBufAdvance(pSgBuf1, cbToAdv);
                RTTESTI_CHECK(cbActual <= cbToAdv);
                RTTESTI_CHECK(cbActual <= cbLeft);
                cbLeft -= cbActual;
            }

            RTTESTI_CHECK(!RTSgBufIsAtStart(pSgBuf1));
            if (   !RTSgBufIsAtEnd(pSgBuf1)
                || RTSgBufCalcLengthLeft(pSgBuf1) != 0
                || RTSgBufIsAtStartOfSegment(pSgBuf1))
            {
                RTTESTI_CHECK(RTSgBufIsAtEnd(pSgBuf1));
                RTTESTI_CHECK(RTSgBufCalcLengthLeft(pSgBuf1) == 0);
                RTTESTI_CHECK(!RTSgBufIsAtStartOfSegment(pSgBuf1));
                RTTestIFailureDetails("iBufVar=%u iRun=%u cSegs=%u cbSgBuf1=%#zx - idxSeg=%u cbSegLeft=%#zx:\n",
                                      iBufVar, iRun, pSgBuf1->cSegs, cbSgBuf1, pSgBuf1->idxSeg, pSgBuf1->cbSegLeft);
                for (unsigned idxSeg = 0; idxSeg < pSgBuf1->cSegs; idxSeg++)
                    RTTestIFailureDetails(" paSegs[%u]: %p LB %#zx\n",
                                          idxSeg, pSgBuf1->paSegs[idxSeg].pvSeg, pSgBuf1->paSegs[idxSeg].cbSeg);
                break;
            }

            RTSgBufReset(pSgBuf1);
        }

        /* Use RTSgBufGetNextSegment with random starting point. */
        for (unsigned iRun = 0; iRun < _1K; iRun++)
        {
            RTSgBufReset(pSgBuf1);
            size_t cbLeft = cbSgBuf1;
            if (iRun > 1)
            {
                size_t const cbInitial = (size_t)RTRandU64Ex(iRun, cbSgBuf1);
                RTTESTI_CHECK(RTSgBufAdvance(pSgBuf1, cbInitial) == cbInitial);
                cbLeft -= cbInitial;
            }
            for (;;)
            {
                RTTESTI_CHECK(RTSgBufCalcLengthLeft(pSgBuf1) == cbLeft);
                size_t cbThisSeg = 0;
                void *pvThisSeg = RTSgBufGetNextSegment(pSgBuf1, &cbThisSeg);
                if (!pvThisSeg)
                    break;
                RTTESTI_CHECK(cbThisSeg > 0);
                RTTESTI_CHECK(cbThisSeg <= cbLeft);
                cbLeft -= cbThisSeg;
            }
            RTTESTI_CHECK(cbLeft == 0);
            RTTESTI_CHECK(RTSgBufIsAtEnd(pSgBuf1));
            RTTESTI_CHECK(RTSgBufCalcLengthLeft(pSgBuf1) == 0);
            RTTESTI_CHECK(!RTSgBufIsAtStartOfSegment(pSgBuf1));
        }

        /* Copy out of the S/G buffer and into a flat buffer w/ electric guard page: */
        bool const fHead = RTRandU32Ex(0, 1) == 0;
        void *pvTmp = NULL;
        int rc = RTTestGuardedAlloc(g_hTest, cbSgBuf1, 1, fHead, &pvTmp);
        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        if (RT_SUCCESS(rc))
        {
            uint8_t const *pbSrc = (uint8_t const *)pSgBuf1->paSegs[0].pvSeg;
            for (unsigned iRun = 0; iRun < _1K; iRun++)
            {
                RTSgBufReset(pSgBuf1);
                size_t cbLeft    = cbSgBuf1;
                size_t cbInitial = 0;
                if (iRun > 1)
                {
                    cbInitial = (size_t)RTRandU64Ex(iRun, cbSgBuf1);
                    RTTESTI_CHECK(RTSgBufAdvance(pSgBuf1, cbInitial) == cbInitial);
                    cbLeft -= cbInitial;
                }

                size_t cbToCopy = cbLeft;
                if (iRun  > _1K / 4 * 3)
                    cbToCopy = (size_t)RTRandU64Ex(0, iRun & 1 ? cbToCopy : cbSgBuf1);

                uint8_t *pbDst = (uint8_t *)pvTmp;
                if (!fHead)
                    pbDst += cbSgBuf1 - RT_MIN(cbToCopy, cbLeft);
                size_t cbCopied = RTSgBufCopyToBuf(pSgBuf1, pbDst, cbToCopy);

                RTTESTI_CHECK(cbCopied <= cbLeft);
                RTTESTI_CHECK(cbCopied <= cbToCopy);
                RTTESTI_CHECK(cbCopied == RT_MIN(cbToCopy, cbLeft));
                RTTESTI_CHECK(memcmp(&pbSrc[cbInitial], pbDst, cbCopied) == 0);

                RTTESTI_CHECK(RTSgBufCalcLengthLeft(pSgBuf1) == cbLeft - cbCopied);
            }

            RTTestGuardedFree(g_hTest, pvTmp);
        }

        RTMemFree(pSgBuf1);
    }
}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTSg", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    testEmpty();
    testBasic();

    return RTTestSummaryAndDestroy(g_hTest);
}

