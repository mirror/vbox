/* $Id$ */
/** @file
 * IPRT - Threads (Part 1), Ring-0 Driver, FreeBSD.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-freebsd-kernel.h"

#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>

#include "internal/thread.h"


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)curthread;
}


RTDECL(int) RTThreadSleep(unsigned cMillies)
{
    int rc;
    int cTicks;

    /*
     * 0 ms sleep -> yield.
     */
    if (!cMillies)
    {
        RTThreadYield();
        return VINF_SUCCESS;
    }

    /*
     * Translate milliseconds into ticks and go to sleep.
     */
    if (cMillies != RT_INDEFINITE_WAIT)
    {
        if (hz == 1000)
            cTicks = cMillies;
        else if (hz == 100)
            cTicks = cMillies / 10;
        else
        {
            int64_t cTicks64 = ((uint64_t)cMillies * hz) / 1000;
            cTicks = (int)cTicks64;
            if (cTicks != cTicks64)
                cTicks = INT_MAX;
        }
    }
    else
        cTicks = 0;     /* requires giant lock! */

    rc = tsleep((void *)RTThreadSleep,
                PZERO | PCATCH,
                "iprtsl",           /* max 6 chars */
                cTicks);
    switch (rc)
    {
        case 0:
            return VINF_SUCCESS;
        case EWOULDBLOCK:
            return VERR_TIMEOUT;
        case EINTR:
        case ERESTART:
            return VERR_INTERRUPTED;
        default:
            AssertMsgFailed(("%d\n", rc));
            return VERR_NO_TRANSLATION;
    }
}


RTDECL(bool) RTThreadYield(void)
{
    uio_yield();
    return false; /** @todo figure this one ... */
}

