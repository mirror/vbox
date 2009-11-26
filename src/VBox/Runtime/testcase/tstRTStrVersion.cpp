/* $Id$ */
/** @file
 * IPRT Testcase - Version String Comparison.
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
#include <iprt/string.h>

#include <iprt/test.h>
#include <iprt/stream.h>


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTStrVersion", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    RTTestSub(hTest, "RTStrVersionCompare");
    static struct
    {
        const char *pszVer1;
        const char *pszVer2;
        int         iResult;
    } const aTests[] =
    {
        { "",           "",                 0 },
        { "asdf",       "",                 1 },
        { "asdf234",    "1.4.5",            1 },
        { "12.foo006",  "12.6",             1 },
        { "1",          "1",                0 },
        { "1",          "100",              -1},
        { "100",        "1",                1 },
        { "3",          "4",                -1},
        { "1",          "0.1",              1 },
        { "1",          "0.0.0.0.10000",    1 },
        { "0100",       "100",              0 },
        { "1.0.0",      "1",                0 },
        { "1.0.0",      "100.0.0",          -1},
        { "1",          "1.0.3.0",          -1},
        { "1.4.5",      "1.2.3",            1 },
        { "1.2.3",      "1.4.5",            -1},
        { "1.2.3",      "4.5.6",            -1},
        { "1.0.4",      "1.0.3",            1 },
        { "0.1",        "0.0.1",            1 },
        { "0.0.1",      "0.1.1",            -1},
        { "3.1.0",      "3.0.14",           1 },
        { "2.0.12",     "3.0.14",           -1},
        { "3.1",        "3.0.22",           1 },
        { "3.0.14",     "3.1.0",            -1},
        { "45.63",      "04.560.30",        1 },
        { "45.006",     "45.6",             0 },
        { "23.206",     "23.06",            1 },
        { "23.2",       "23.060",           -1},

        { "VirtualBox-2.0.8-Beta2",     "VirtualBox-2.0.8_Beta3-r12345",    -1 },
        { "VirtualBox-2.2.4-Beta2",     "VirtualBox-2.2.2",                  1 },
        { "VirtualBox-2.2.4-Beta3",     "VirtualBox-2.2.2-Beta4",            1 },
        { "VirtualBox-3.1.8-Alpha1",    "VirtualBox-3.1.8-Alpha1-r61454",   -1 },
        { "VirtualBox-3.1.0",           "VirtualBox-3.1.2_Beta1",           -1 },
        { "3.1.0_BETA-r12345",          "3.1.2",                            -1 },
        { "3.1.0_BETA1r12345",          "3.1.0",                            -1 },
        { "3.1.0_BETAr12345",           "3.1.0",                             1 }, /* not considered a beta because of missing punctuation */
        { "3.1.0_BETA-r12345",          "3.1.0",                            -1 },
        { "3.1.0_BETA-r12345",          "3.1.0",                            -1 },
        { "3.1.0_BETA-r12345",          "3.1.0.0",                          -1 },
        { "3.1.0_BETA",                 "3.1.0.0",                          -1 },
        { "3.1.0_BETA1",                "3.1.0",                            -1 },
        { "3.1.0_BETA-r12345",          "3.1.0r12345",                      -1 },
        { "3.1.0_BETA1-r12345",         "3.1.0_BETA-r12345",                 0 },
        { "3.1.0_BETA1-r12345",         "3.1.0_BETA1-r12345",                0 },
        { "3.1.0_BETA2-r12345",         "3.1.0_BETA1-r12345",                1 },
        { "3.1.0_BETA2-r12345",         "3.1.0_BETA999-r12345",             -1 },
        { "3.1.0_BETA2",                "3.1.0_ABC",                        -1 }, /* ABC isn't indicating a prerelease, BETA does */
        { "3.1.0_BETA",                 "3.1.0_ATEB",                       -1 },
    };
    for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests); iTest++)
    {
        int iResult = RTStrVersionCompare(aTests[iTest].pszVer1, aTests[iTest].pszVer2);
        if (iResult != aTests[iTest].iResult)
            RTTestFailed(hTest, "#%u: '%s' <-> '%s' -> %d, expected %d",
                         iTest, aTests[iTest].pszVer1, aTests[iTest].pszVer2, iResult, aTests[iTest].iResult);

        iResult = -RTStrVersionCompare(aTests[iTest].pszVer2, aTests[iTest].pszVer1);
        if (iResult != aTests[iTest].iResult)
            RTTestFailed(hTest, "#%u: '%s' <-> '%s' -> %d, expected %d [inv]",
                         iTest, aTests[iTest].pszVer1, aTests[iTest].pszVer2, iResult, aTests[iTest].iResult);
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

