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
        char const *apszPaths[3];
        char const *pszCommon;
        uint32_t    fFlags;
    } aTests[] =
    {
        /* Simple stuff first. */
        { { "",                     "",                 "" },                       "",          RTPATH_STR_F_STYLE_UNIX },
        { { "none",                 "none",             "" },                       "",          RTPATH_STR_F_STYLE_UNIX },
        /* Missing start slash. */
        { { "/path/to/stuff1",      "path/to/stuff2",   "" },                       "",          RTPATH_STR_F_STYLE_UNIX },
        /* Working stuff. */
        { { "/path/to/stuff1",      "/path/to/stuff2",  "/path/to/stuff3" },        "/path/to/", RTPATH_STR_F_STYLE_UNIX },
        { { "/path/to/stuff1",      "/path/to/",        "/path/" },                 "/path/",    RTPATH_STR_F_STYLE_UNIX },
        { { "/path/to/stuff1",      "/",                "/path/" },                 "",          RTPATH_STR_F_STYLE_UNIX },
        { { "/path/to/../stuff1",   "./../",            "/path/to/stuff2/.." },     "",          RTPATH_STR_F_STYLE_UNIX },
    };

    for (size_t i = 0; i < RT_ELEMENTS(aTests); i++)
    {
        size_t cPaths = RT_ELEMENTS(aTests[i].apszPaths);
        while (cPaths > 0 && aTests[i].apszPaths[cPaths - 1] == NULL)
            cPaths--;

        size_t const cchCommon = RTPathFindCommonEx(cPaths, aTests[i].apszPaths, aTests[i].fFlags);
        size_t const cchExpect = strlen(aTests[i].pszCommon);
        if (cchCommon != cchExpect)
            RTTestFailed(hTest,
                         "Test %zu failed: got %zu, expected %zu (cPaths=%zu: %s %s %s)", i, cchCommon, cchExpect, cPaths,
                         aTests[i].apszPaths[0], aTests[i].apszPaths[1], aTests[i].apszPaths[2]);
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

