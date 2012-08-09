/* $Id$ */
/** @file
 * IPRT Testcase - Temporary files and directories.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static char g_szTempPath[RTPATH_MAX - 50];


static void tstDirCreateTemp(const char *pszSubTest, const char *pszTemplate, unsigned cTimes, bool fSkipXCheck)
{
    RTTestISub(pszSubTest);

    /* Allocate the result array. */
    char **papszNames = (char **)RTMemTmpAllocZ(cTimes * sizeof(char *));
    RTTESTI_CHECK_RETV(papszNames != NULL);

    /* The test loop. */
    unsigned i;
    for (i = 0; i < cTimes; i++)
    {
        int rc;
        char szName[RTPATH_MAX];
        RTTESTI_CHECK_RC(rc = RTPathAppend(strcpy(szName, g_szTempPath), sizeof(szName), pszTemplate), VINF_SUCCESS);
        if (RT_FAILURE(rc))
            break;

        RTTESTI_CHECK(papszNames[i] = RTStrDup(szName));
        if (!papszNames[i])
            break;

        rc = RTDirCreateTemp(papszNames[i], 0700);
        if (rc != VINF_SUCCESS)
        {
            RTTestIFailed("RTDirCreateTemp(%s) call #%u -> %Rrc\n", szName, i, rc);
            RTStrFree(papszNames[i]);
            papszNames[i] = NULL;
            break;
        }
        RTTestIPrintf(RTTESTLVL_DEBUG, "%s\n", papszNames[i]);
        RTTESTI_CHECK_MSG(strlen(szName) == strlen(papszNames[i]), ("szName   %s\nReturned %s\n", szName, papszNames[i]));
        if (!fSkipXCheck)
            RTTESTI_CHECK_MSG(strchr(RTPathFilename(papszNames[i]), 'X') == NULL, ("szName   %s\nReturned %s\n", szName, papszNames[i]));
    }

    /* cleanup */
    while (i-- > 0)
    {
        RTTESTI_CHECK_RC(RTDirRemove(papszNames[i]), VINF_SUCCESS);
        RTStrFree(papszNames[i]);
    }
    RTMemTmpFree(papszNames);
}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTTemp", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Get the temp directory (this is essential to the testcase).
     */
    RTTESTI_CHECK_RC(rc = RTPathTemp(g_szTempPath, sizeof(g_szTempPath)), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);

    /*
     * Create N temporary directories using RTDirCreateTemp.
     */
    tstDirCreateTemp("RTDirCreateTemp #1 (standard)",   "rtRTTemp-XXXXXX",              128, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #2 (long)",       "rtRTTemp-XXXXXXXXXXXXXXXXX",   128, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #3 (short)",      "rtRTTemp-XX",                  128, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #4 (very short)", "rtRTTemp-X",                 26+10, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #5 (in-name)",    "rtRTTemp-XXXt",                  2, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #6 (in-name)",    "XXX-rtRTTemp",                   2, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #7 (in-name)",    "rtRTTemp-XXXXXXXXX.tmp",       128, false /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #8 (in-name)",    "rtRTTemp-XXXXXXX-X.tmp",       128, true /*fSkipXCheck*/);
    tstDirCreateTemp("RTDirCreateTemp #9 (in-name)",    "rtRTTemp-XXXXXX-XX.tmp",       128, true /*fSkipXCheck*/);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

