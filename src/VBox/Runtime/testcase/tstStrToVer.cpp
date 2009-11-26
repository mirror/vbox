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


#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/stream.h>


struct TstU8
{
    const char *pszVer1;
    const char *pszVer2;
    uint8_t     Result;
};


#define TEST(Test, Type, Fmt, Fun, iTest) \
    do \
    { \
        Type Result = Fun(Test.pszVer1, Test.pszVer2); \
        if (Result != Test.Result) \
        { \
            RTPrintf("failure: '%s' <-> '%s' -> " Fmt ", expected " Fmt ". (%s/%u)\n", Test.pszVer1, Test.pszVer2, Result, Test.Result, #Fun, iTest); \
            cErrors++; \
        } \
    } while (0)

#define RUN_TESTS(aTests, Type, Fmt, Fun) \
    do \
    { \
        for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests); iTest++) \
        { \
            TEST(aTests[iTest], Type, Fmt, Fun, iTest); \
        } \
    } while (0)

int main()
{
    RTR3Init();

    int cErrors = 0;
    static const struct TstU8 aTstU8[] =
    {
        { "", "",                         0 },
        { "asdf", "",                     1 }, /* "asdf" is bigger than "" */
        { "asdf234", "1.4.5",             1 },
        { "12.foo006", "12.6",            1 }, /* "12.foo006" is bigger than "12.6" */
        { "1", "1",                       0 },
        { "1", "100",                     2 },
        { "100", "1",                     1 },
        { "3", "4",                       2 },
        { "1", "0.1",                     1 },
        { "1", "0.0.0.0.10000",           1 },
        { "0100", "100",                  0 },
        { "1.0.0", "1",                   0 },
        { "1.0.0", "100.0.0",             2 },
        { "1", "1.0.3.0",                 2 },
        { "1.4.5", "1.2.3",               1 },
        { "1.2.3", "1.4.5",               2 },
        { "1.2.3", "4.5.6",               2 },
        { "1.0.4", "1.0.3",               1 },
        { "0.1", "0.0.1",                 1 },
        { "0.0.1", "0.1.1",               2 },
        { "3.1.0", "3.0.14",              1 },
        { "2.0.12", "3.0.14",             2 },
        { "3.1", "3.0.22",                1 },
        { "3.0.14", "3.1.0",              2 },
        { "45.63", "04.560.30",           1 },
        { "45.006", "45.6",               0 },
        { "23.206", "23.06",              1 },
        { "23.2", "23.060",               2 },

        { "VirtualBox-2.0.8-Beta2", "VirtualBox-2.0.8_Beta3-r12345", 2 },
        { "VirtualBox-2.2.4-Beta2", "VirtualBox-2.2.2", 1 },
        { "VirtualBox-2.2.4-Beta3", "VirtualBox-2.2.2-Beta4", 1 },
        { "VirtualBox-3.1.8-Alpha1", "VirtualBox-3.1.8-Alpha1-r61454", 2 },
        { "VirtualBox-3.1.0", "VirtualBox-3.1.2_Beta1", 2 },
        { "3.1.0_BETA-r12345", "3.1.2", 2 },
    };
    RUN_TESTS(aTstU8, int, "%#d", RTStrVersionCompare);

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstStrToVer: SUCCESS\n");
    else
        RTPrintf("tstStrToVer: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
