/* $Id$ */
/** @file
 * IPRT Testcase - String splitting.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/string.h>

#include <iprt/test.h>
#include <iprt/stream.h>


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTStrSplit", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /* Invalid stuff. */
    RTTEST_CHECK_RC(hTest, RTStrSplit(NULL, 0, NULL, NULL, NULL), VERR_INVALID_POINTER);
    RTTEST_CHECK_RC(hTest, RTStrSplit("foo", 0, NULL, NULL, NULL), VERR_INVALID_PARAMETER);
    RTTEST_CHECK_RC(hTest, RTStrSplit("foo", 42, NULL, NULL, NULL), VERR_INVALID_POINTER);

    char **papszStrings = NULL;
    size_t cStrings = 0;

    /* Empty stuff. */
    const char szEmpty[] = "";
    RTTEST_CHECK_RC(hTest, RTStrSplit(szEmpty, sizeof(szEmpty), "\r\n", &papszStrings, &cStrings), VINF_SUCCESS);
    RTTEST_CHECK(hTest, cStrings == 0);
    RTTEST_CHECK(hTest, papszStrings == NULL);

    /* No separator given. */
    const char szNoSep[] = "foo";
    RTTEST_CHECK_RC(hTest, RTStrSplit(szNoSep, sizeof(szNoSep), "\r\n", &papszStrings, &cStrings), VINF_SUCCESS);
    RTTEST_CHECK(hTest, cStrings == 0);
    RTTEST_CHECK(hTest, papszStrings == NULL);

    /* Single string w/ separator. */
    const char szWithSep[] = "foo\r\n";
    RTTEST_CHECK_RC(hTest, RTStrSplit(szWithSep, sizeof(szWithSep), "\r\n", &papszStrings, &cStrings), VINF_SUCCESS);
    RTTEST_CHECK(hTest, cStrings == 1);
    RTTEST_CHECK(hTest, papszStrings && RTStrICmp(papszStrings[0], "foo") == 0);

    /* Multiple strings w/ separators. */
    const char szWithSep2[] = "foo\r\nbar\r\n";
    RTTEST_CHECK_RC(hTest, RTStrSplit(szWithSep2, sizeof(szWithSep2), "\r\n", &papszStrings, &cStrings), VINF_SUCCESS);
    RTTEST_CHECK(hTest, cStrings == 2);
    RTTEST_CHECK(hTest,    cStrings == 2
                        && papszStrings
                        && RTStrICmp(papszStrings[0], "foo") == 0
                        && RTStrICmp(papszStrings[1], "bar") == 0);

    /* Multiple strings w/ two consequtive separators. */
    const char szWithSep3[] = "foo\r\nbar\r\n\r\n";
    RTTEST_CHECK_RC(hTest, RTStrSplit(szWithSep3, sizeof(szWithSep3), "\r\n", &papszStrings, &cStrings), VINF_SUCCESS);
    RTTEST_CHECK(hTest, cStrings == 2);
    RTTEST_CHECK(hTest,    cStrings == 2
                        && papszStrings
                        && RTStrICmp(papszStrings[0], "foo") == 0
                        && RTStrICmp(papszStrings[1], "bar") == 0);

    /* Multiple strings w/ two consequtive separators. */
    const char szWithSep4[] = "foo\r\nbar\r\n\r\nbaz\r\n";
    RTTEST_CHECK_RC(hTest, RTStrSplit(szWithSep4, sizeof(szWithSep4), "\r\n", &papszStrings, &cStrings), VINF_SUCCESS);
    RTTEST_CHECK(hTest, cStrings == 3);
    RTTEST_CHECK(hTest,    cStrings == 3
                        && papszStrings
                        && RTStrICmp(papszStrings[0], "foo") == 0
                        && RTStrICmp(papszStrings[1], "bar") == 0
                        && RTStrICmp(papszStrings[2], "baz") == 0);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

