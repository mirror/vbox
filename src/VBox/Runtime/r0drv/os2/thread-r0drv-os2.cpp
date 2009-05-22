/* $Id$ */
/** @file
 * IPRT - Threads (Part 1), Ring-0 Driver, OS/2.
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
#include "the-os2-kernel.h"

#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/thread.h"


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    PLINFOSEG pLIS = (PLINFOSEG)RTR0Os2Virt2Flat(g_fpLIS);
    AssertReturn(pLIS, NIL_RTNATIVETHREAD);
    return pLIS->tidCurrent | (pLIS->pidCurrent << 16);
}


RTDECL(int) RTThreadSleep(unsigned cMillies)
{
    int rc = KernBlock((ULONG)RTThreadSleep,
                       cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies,
                       0, NULL, NULL);
    switch (rc)
    {
        case NO_ERROR:
            return VINF_SUCCESS;
        case ERROR_TIMEOUT:
            return VERR_TIMEOUT;
        case ERROR_INTERRUPT:
            return VERR_INTERRUPTED;
        default:
            AssertMsgFailed(("%d\n", rc));
            return VERR_NO_TRANSLATION;
    }
}


RTDECL(bool) RTThreadYield(void)
{
    /** @todo implement me (requires a devhelp) */
    return false;
}


RTDECL(bool) RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
    return false;
}


RTDECL(bool) RTThreadPreemptIsPending(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);

    union
    {
        RTFAR16 fp;
        uint8_t fResched;
    } u;
    int rc = RTR0Os2DHQueryDOSVar(DHGETDOSV_YIELDFLAG, 0, &u.fp);
    AssertReturn(rc == 0, false);
    if (u.fResched)
        return true;

    /** @todo Check if DHGETDOSV_YIELDFLAG includes TCYIELDFLAG. */
    rc = RTR0Os2DHQueryDOSVar(DHGETDOSV_TCYIELDFLAG, 0, &u.fp);
    AssertReturn(rc == 0, false);
    if (u.fResched)
        return true;
    return false;
}


RTDECL(void) RTThreadPreemptDisable(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->uchDummy != 42);
    pState->uchDummy = 42;
    /* Nothing to do here as OS/2 doesn't preempt kernel threads. */
}


RTDECL(void) RTThreadPreemptRestore(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->uchDummy == 42);
    pState->uchDummy = 0;
}

