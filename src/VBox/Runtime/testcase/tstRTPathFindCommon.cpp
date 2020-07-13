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
#include <iprt/path.h>

#include <iprt/test.h>
#include <iprt/stream.h>
#include <iprt/string.h>


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTPathFindCommon", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    struct
    {
        char const *papszPath1;
        char const *papszPath2;
        char const *papszPath3;
        char const *papszPatCommon;
    } aTests[] =
    {
        /* Simple stuff first. */
        { "", "", "", "" },
        { "none", "none", "", "" },
        /* Missing start slash. */
        { "/path/to/stuff1", "path/to/stuff2", "", "" },
        /* Working stuff. */
        { "/path/to/stuff1", "/path/to/stuff2", "/path/to/stuff3", "/path/to/" },
        { "/path/to/stuff1", "/path/to/", "/path/", "/path/" },
        { "/path/to/stuff1", "/", "/path/", "" },
        { "/path/to/../stuff1", "./../", "/path/to/stuff2/..", "" }
    };

    const size_t cNumPaths = 3; /* Number of paths to compare. */

    size_t cchRes;
    for (size_t i = 0; i < RT_ELEMENTS(aTests); i++)
        RTTEST_CHECK_MSG(hTest, (cchRes = RTPathFindCommonEx((const char * const *)&aTests[i], cNumPaths, '/')) == strlen(aTests[i].papszPatCommon),
                         (hTest, "Test %zu failed: Got %zu, expected %zu\n", i, cchRes, strlen(aTests[i].papszPatCommon)));

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

