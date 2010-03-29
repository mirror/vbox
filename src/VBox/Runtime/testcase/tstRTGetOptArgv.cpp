/* $Id$ */
/** @file
 * IPRT Testcase - RTGetOptArgv*.
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
#include <iprt/path.h>

#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/getopt.h>
#include <iprt/string.h>
#include <iprt/test.h>


static void tst2(void)
{
    RTTestISub("RTGetOptArgvToString / MS_CRT");

    static const struct
    {
        const char * const      apszArgs[5];
        const char             *pszCmdLine;
    } s_aTests[] =
    {
        {
            { "abcd", "a ", " b", " c ", NULL },
            "abcd \"a \" \" b\" \" c \""
        },
        {
            { "a\\\\\\b", "de fg", "h", NULL, NULL },
            "a\\\\\\b \"de fg\" h"
        },
        {
            { "a\\\"b", "c", "d", "\"", NULL },
            "\"a\\\\\\\"b\" c d \"\\\"\""
        },
        {
            { "a\\\\b c", "d", "e", " \\", NULL },
            "\"a\\\\b c\" d e \" \\\\\""
        },
    };

    for (size_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        char *pszCmdLine = NULL;
        int rc = RTGetOptArgvToString(&pszCmdLine, s_aTests[i].apszArgs, RTGETOPTARGV_CNV_QUOTE_MS_CRT);
        RTTESTI_CHECK_RC_RETV(rc, VINF_SUCCESS);
        if (strcmp(s_aTests[i].pszCmdLine, pszCmdLine))
            RTTestIFailed("g_aTest[%i] failed:\n"
                          " got      '%s'\n"
                          " expected '%s'\n",
                          i, pszCmdLine, s_aTests[i].pszCmdLine);
        RTStrFree(pszCmdLine);
    }
}

static void tst1(void)
{
    RTTestISub("RTGetOptArgvFromString");
    char **papszArgs = NULL;
    int    cArgs = -1;
    RTTESTI_CHECK_RC_RETV(RTGetOptArgvFromString(&papszArgs, &cArgs, "", NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cArgs == 0);
    RTTESTI_CHECK_RETV(papszArgs);
    RTTESTI_CHECK_RETV(!papszArgs[0]);
    RTGetOptArgvFree(papszArgs);

    RTTESTI_CHECK_RC_RETV(RTGetOptArgvFromString(&papszArgs, &cArgs, "0 1 \"\"2'' '3' 4 5 '''''6' 7 8 9 10 11", NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cArgs == 12);
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[0], "0"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[1], "1"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[2], "2"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[3], "3"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[4], "4"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[5], "5"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[6], "6"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[7], "7"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[8], "8"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[9], "9"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[10], "10"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[11], "11"));
    RTTESTI_CHECK_RETV(!papszArgs[12]);
    RTGetOptArgvFree(papszArgs);

    RTTESTI_CHECK_RC_RETV(RTGetOptArgvFromString(&papszArgs, &cArgs, "\t\" asdf \"  '\"'xyz  \"\t\"  '\n'  '\"'  \"'\"\n\r ", NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cArgs == 6);
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[0], " asdf "));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[1], "\"xyz"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[2], "\t"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[3], "\n"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[4], "\""));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[5], "\'"));
    RTTESTI_CHECK_RETV(!papszArgs[6]);
    RTGetOptArgvFree(papszArgs);

    RTTESTI_CHECK_RC_RETV(RTGetOptArgvFromString(&papszArgs, &cArgs, ":0::1::::2:3:4:5:", ":"), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cArgs == 6);
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[0], "0"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[1], "1"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[2], "2"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[3], "3"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[4], "4"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[5], "5"));
    RTTESTI_CHECK_RETV(!papszArgs[6]);
    RTGetOptArgvFree(papszArgs);

    RTTESTI_CHECK_RC_RETV(RTGetOptArgvFromString(&papszArgs, &cArgs, "0:1;2:3;4:5", ";;;;;;;;;;;;;;;;;;;;;;:"), VINF_SUCCESS);
    RTTESTI_CHECK_RETV(cArgs == 6);
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[0], "0"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[1], "1"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[2], "2"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[3], "3"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[4], "4"));
    RTTESTI_CHECK_RETV(!strcmp(papszArgs[5], "5"));
    RTTESTI_CHECK_RETV(!papszArgs[6]);
    RTGetOptArgvFree(papszArgs);
}

int main()
{
    /*
     * Init RT+Test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTGetOptArgv", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * The test.
     */
    tst1();
    tst2();

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

