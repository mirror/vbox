/* $Id$ */
/** @file
 * Support Library Testcase - Ring-3 Semaphore interface.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <VBox/sup.h>

#include <VBox/param.h>
#include <iprt/initterm.h>
#include <iprt/test.h>
#include <iprt/stream.h>
#include <iprt/string.h>


int main(int argc, char **argv)
{
    /*
     * Init.
     */
    int rc = RTR3InitAndSUPLib();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSupSem: fatal error: RTR3InitAndSUPLib failed with rc=%Rrc\n", rc);
        return 1;
    }
    RTTEST hTest;
    rc = RTTestCreate("tstSupSem", &hTest);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSupSem: fatal error: RTTestCreate failed with rc=%Rrc\n", rc);
        return 1;
    }
    PSUPDRVSESSION pSession;
    rc = SUPR3Init(&pSession);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "SUPR3Init failed with rc=%Rrc\n", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    /*
     * Basic API checks.
     */
    RTTestSub(hTest, "Single Release Event API");
    SUPSEMEVENT hEvent = NIL_SUPSEMEVENT;
    RTTESTI_CHECK_RC(SUPSemEventCreate(pSession, &hEvent),          VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent,20),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent,20),  VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent,20),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, hEvent),            VINF_OBJECT_DESTROYED);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, hEvent),            VERR_INVALID_HANDLE);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, NIL_SUPSEMEVENT),   VINF_SUCCESS);

    RTTestSub(hTest, "Multiple Release Event API");
    SUPSEMEVENTMULTI hEventMulti = NIL_SUPSEMEVENT;
    RTTESTI_CHECK_RC(SUPSemEventMultiCreate(pSession, &hEventMulti),            VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti,20),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiReset(pSession, hEventMulti),              VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti,20),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, hEventMulti),              VINF_OBJECT_DESTROYED);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, hEventMulti),              VERR_INVALID_HANDLE);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, NIL_SUPSEMEVENTMULTI),     VINF_SUCCESS);

    /*
     * Done.
     */
    return RTTestSummaryAndDestroy(hTest);
}

