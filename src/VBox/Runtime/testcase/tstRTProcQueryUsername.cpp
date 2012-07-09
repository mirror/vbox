/* $Id$ */
/** @file
 * IPRT Testcase - RTProcQueryUsername.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/initterm.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/test.h>

static void tstRTProcQueryUsername(void)
{
    char abUser[1024];
    size_t cbUser;
    char *pszUser = NULL;

    RTTestISub("Basics");

    memset(abUser, 0, sizeof(abUser));
    RTTESTI_CHECK_RC(RTProcQueryUsername(RTProcSelf(), NULL, 8, &cbUser), VERR_INVALID_PARAMETER);
    RTTESTI_CHECK_RC(RTProcQueryUsername(RTProcSelf(), abUser, 0, &cbUser), VERR_INVALID_PARAMETER);
    RTTESTI_CHECK_RC(RTProcQueryUsername(RTProcSelf(), NULL, 0, NULL), VERR_BUFFER_OVERFLOW);
    RTTESTI_CHECK_RC(RTProcQueryUsername(RTProcSelf(), NULL, 0, &cbUser), VERR_BUFFER_OVERFLOW);

    RTTESTI_CHECK_RC(RTProcQueryUsername(RTProcSelf(), abUser, sizeof(abUser), &cbUser), VINF_SUCCESS);
    RTTestPrintf(NULL, RTTESTLVL_ALWAYS, "Username: %s\n", abUser);
    RTTESTI_CHECK_RC(RTProcQueryUsername(RTProcSelf(), abUser, cbUser - 1, &cbUser), VERR_BUFFER_OVERFLOW);

    RTTESTI_CHECK_RC(RTProcQueryUsernameA(RTProcSelf(), NULL), VERR_INVALID_POINTER);
    RTTESTI_CHECK_RC(RTProcQueryUsernameA(RTProcSelf(), &pszUser), VINF_SUCCESS);
    RTTestPrintf(NULL, RTTESTLVL_ALWAYS, "Username: %s\n", pszUser);
    RTStrFree(pszUser);
}

int main(int argc, char **argv)
{
    /*
     * Initialize IPRT and create the test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTProcQueryUsername", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tstRTProcQueryUsername();

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}
