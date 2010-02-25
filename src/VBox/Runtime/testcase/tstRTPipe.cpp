/* $Id$ */
/** @file
 * IPRT Testcase - RTPipe.
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
#include <iprt/pipe.h>

#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/test.h>


static void tstRTPipe3(void)
{
    RTTestISub("Full write buffer");

    RTPIPE  hPipeR = (RTPIPE)999999;
    RTPIPE  hPipeW = (RTPIPE)999999;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);

    static char s_abBuf[_256K];
    int         rc      = VINF_SUCCESS;
    size_t      cbTotal = 0;
    memset(s_abBuf, 0xff, sizeof(s_abBuf));
    for (;;)
    {
        RTTESTI_CHECK(cbTotal < _1G);
        if (cbTotal > _1G)
            break;

        size_t cbWritten = _1G;
        rc = RTPipeWrite(hPipeW, s_abBuf, sizeof(s_abBuf), &cbWritten);
        RTTESTI_CHECK(rc == VINF_SUCCESS || rc == VINF_TRY_AGAIN);
        if (rc != VINF_SUCCESS)
            break;
        cbTotal += cbWritten;
    }

    if (rc == VINF_TRY_AGAIN)
    {
        RTTestIPrintf(RTTESTLVL_ALWAYS, "cbTotal=%zu (%#zx)\n", cbTotal, cbTotal);
        RTTESTI_CHECK_RC(RTPipeSelectOne(hPipeW, 0), VERR_TIMEOUT);
        RTTESTI_CHECK_RC(RTPipeSelectOne(hPipeW, 1), VERR_TIMEOUT);
        size_t cbRead;
        RTTESTI_CHECK_RC(RTPipeRead(hPipeR, s_abBuf, RT_MIN(sizeof(s_abBuf), cbTotal) / 2, &cbRead), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTPipeSelectOne(hPipeW, 0), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTPipeSelectOne(hPipeW, 1), VINF_SUCCESS);

        size_t cbWritten = _1G;
        rc = RTPipeWrite(hPipeW, s_abBuf, sizeof(s_abBuf), &cbWritten);
        RTTESTI_CHECK(rc == VINF_SUCCESS);
    }

    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);
}

static void tstRTPipe2(void)
{
    RTTestISub("Negative");

    RTPIPE  hPipeR = (RTPIPE)1;
    RTPIPE  hPipeW = (RTPIPE)1;
    RTTESTI_CHECK_RC(RTPipeCreate(&hPipeR, &hPipeW, ~0), VERR_INVALID_PARAMETER);
    RTTESTI_CHECK_RC(RTPipeCreate(NULL, &hPipeW, 0), VERR_INVALID_POINTER);
    RTTESTI_CHECK_RC(RTPipeCreate(&hPipeR, NULL, 0), VERR_INVALID_POINTER);
    RTTESTI_CHECK(hPipeR == (RTPIPE)1);
    RTTESTI_CHECK(hPipeW == (RTPIPE)1);


    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);

    char abBuf[_4K];
    size_t cbRead = ~(size_t)3;
    RTTESTI_CHECK_RC(RTPipeRead(hPipeW, abBuf, 0, &cbRead), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTPipeRead(hPipeW, abBuf, 1, &cbRead), VERR_ACCESS_DENIED);
    RTTESTI_CHECK(cbRead == ~(size_t)3);
    RTTESTI_CHECK_RC(RTPipeReadBlocking(hPipeW, abBuf, 0), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTPipeReadBlocking(hPipeW, abBuf, 1), VERR_ACCESS_DENIED);

    size_t cbWrite = ~(size_t)5;
    RTTESTI_CHECK_RC(RTPipeWrite(hPipeR, "asdf", 0, &cbWrite), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTPipeWrite(hPipeR, "asdf", 4, &cbWrite), VERR_ACCESS_DENIED);
    RTTESTI_CHECK(cbWrite == ~(size_t)5);
    RTTESTI_CHECK_RC(RTPipeWriteBlocking(hPipeR, "asdf", 0), VERR_ACCESS_DENIED);
    RTTESTI_CHECK_RC(RTPipeWriteBlocking(hPipeR, "asdf", 4), VERR_ACCESS_DENIED);

    RTTESTI_CHECK_RC(RTPipeFlush(hPipeR), VERR_ACCESS_DENIED);

    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);
}


static void tstRTPipe1(void)
{
    RTTestISub("Basics");

    RTPIPE  hPipeR = NIL_RTPIPE;
    RTPIPE  hPipeW = NIL_RTPIPE;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(hPipeR != NIL_RTPIPE);
    RTTESTI_CHECK_RETV(hPipeW != NIL_RTPIPE);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(NIL_RTPIPE), VINF_SUCCESS);

    hPipeR = NIL_RTPIPE;
    hPipeW = NIL_RTPIPE;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_READ | RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    int rc = RTPipeFlush(hPipeW);
    RTTESTI_CHECK_MSG(rc == VERR_NOT_SUPPORTED || rc == VINF_SUCCESS, ("%Rrc\n", rc));
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_READ), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeSelectOne(hPipeR, 0), VERR_TIMEOUT);
    RTTESTI_CHECK_RC_RETV(RTPipeSelectOne(hPipeR, 1), VERR_TIMEOUT);
    RTTESTI_CHECK_RC_RETV(RTPipeSelectOne(hPipeW, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeSelectOne(hPipeW, 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_READ), VINF_SUCCESS);

    size_t  cbRead = ~(size_t)0;
    char    abBuf[_64K + _4K];
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, sizeof(abBuf), &cbRead), VINF_TRY_AGAIN);
    RTTESTI_CHECK_RETV(cbRead == 0);

    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, 1, &cbRead), VINF_TRY_AGAIN);
    RTTESTI_CHECK_RETV(cbRead == 0);

    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, 0, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 0);

    size_t cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC_RETV(RTPipeWrite(hPipeW, abBuf, 0, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbWritten == 0);

    /* We can write a number of bytes without blocking (see PIPE_BUF on
       POSIX systems). */
    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC_RETV(RTPipeWrite(hPipeW, "42", 2, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbWritten == 2);
    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC_RETV(RTPipeWrite(hPipeW, "!", 1, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbWritten == 1);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, 3, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 3);
    RTTESTI_CHECK_RETV(!memcmp(abBuf, "42!", 3));

    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC_RETV(RTPipeWrite(hPipeW, "BigQ", 4, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbWritten == 4);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeSelectOne(hPipeR, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeSelectOne(hPipeR, 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, sizeof(abBuf), &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 4);
    RTTESTI_CHECK_RETV(!memcmp(abBuf, "BigQ", 4));

    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC_RETV(RTPipeWrite(hPipeW, "H2G2", 4, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbWritten == 4);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, &abBuf[0], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 1);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, &abBuf[1], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 1);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, &abBuf[2], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 1);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, &abBuf[3], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cbRead == 1);
    RTTESTI_CHECK_RETV(!memcmp(abBuf, "H2G2", 4));

    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);


    RTTestISub("VERR_BROKEN_PIPE");
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeR), VINF_SUCCESS);
    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC(RTPipeWrite(hPipeW, "", 0, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK(cbWritten == 0);
    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC(RTPipeWrite(hPipeW, "4", 1, &cbWritten), VERR_BROKEN_PIPE);
    RTTESTI_CHECK(cbWritten == ~(size_t)2);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC(RTPipeRead(hPipeR, abBuf, 0, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == 0);
    cbRead = ~(size_t)3;
    RTTESTI_CHECK_RC(RTPipeRead(hPipeR, abBuf, sizeof(abBuf), &cbRead), VERR_BROKEN_PIPE);
    RTTESTI_CHECK(cbRead == ~(size_t)3);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);
    cbWritten = ~(size_t)2;
    RTTESTI_CHECK_RC(RTPipeWrite(hPipeW, "42", 2, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK(cbWritten == 2);
    RTTESTI_CHECK_RC_RETV(RTPipeClose(hPipeW), VINF_SUCCESS);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, 0, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == 0);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC(RTPipeRead(hPipeR, &abBuf[0], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == 1);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC(RTPipeRead(hPipeR, &abBuf[1], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == 1);
    RTTESTI_CHECK(!memcmp(abBuf, "42", 2));
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC(RTPipeRead(hPipeR, abBuf, 0, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == 0);
    cbRead = ~(size_t)3;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, abBuf, sizeof(abBuf), &cbRead), VERR_BROKEN_PIPE);
    RTTESTI_CHECK(cbRead == ~(size_t)3);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTTestISub("Blocking");
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeWrite(hPipeW, "42!", 3, &cbWritten), VINF_SUCCESS);
    RTTESTI_CHECK(cbWritten == 3);
    RTTESTI_CHECK_RC_RETV(RTPipeReadBlocking(hPipeR, abBuf, 3), VINF_SUCCESS);
    RTTESTI_CHECK(!memcmp(abBuf, "42!", 3));
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeReadBlocking(hPipeR, &abBuf[0], 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeReadBlocking(hPipeR, &abBuf[0], 1), VERR_BROKEN_PIPE);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeWriteBlocking(hPipeW, "42!", 3), VINF_SUCCESS);
    RTTESTI_CHECK(cbWritten == 3);
    cbRead = ~(size_t)0;
    RTTESTI_CHECK_RC_RETV(RTPipeRead(hPipeR, &abBuf[0], 1, &cbRead), VINF_SUCCESS);
    RTTESTI_CHECK(cbRead == 1);
    RTTESTI_CHECK_RC_RETV(RTPipeReadBlocking(hPipeR, &abBuf[1], 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeReadBlocking(hPipeR, &abBuf[2], 1), VINF_SUCCESS);
    RTTESTI_CHECK(!memcmp(abBuf, "42!", 3));
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeWriteBlocking(hPipeW, "", 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTPipeWriteBlocking(hPipeW, "42", 2), VERR_BROKEN_PIPE);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);
}

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTPipe", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * The tests.
     */
    tstRTPipe1();
    if (RTTestErrorCount(hTest) == 0)
    {
        bool fMayPanic = RTAssertMayPanic();
        bool fQuiet    = RTAssertAreQuiet();
        RTAssertSetMayPanic(false);
        RTAssertSetQuiet(true);
        tstRTPipe2();
        RTAssertSetQuiet(fQuiet);
        RTAssertSetMayPanic(fMayPanic);

        tstRTPipe3();
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}


