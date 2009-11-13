/* $Id$ */
/** @file
 * IPRT Testcase - String To Version Number Conversion.
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

#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/err.h>


#include <iprt/assert.h>

int RTStrVersionToUInt32(const char *pszVer, uint32_t *pu32)
{
    char *str = RTStrDup(pszVer);
    AssertPtr(pu32);
    AssertPtr(str);

    char *c = RTStrStrip(str);
    char *next;
    char n = '.';
    int rc;
    uint32_t t;

    *pu32 = 0;

    char szDelim[] = { '-', '_' };

    /* Get trailing content separated by dash (-) or underscore (_)*/
    //c = RTStrStr(c, &n)
    c = strchr(c, '-');
    if (c)
        c++; /* Skip dash */
    else
        c = str;

    /* Get last digit */
    rc = RTStrToUInt32Ex(c,
                         &next,
                         10 /* number base */,
                         &t);
    *pu32 += t;
    RTStrFree(str);
    return rc;
}


struct TstU32
{
    const char *psz;
    int         rc;
    uint32_t    Result;
};


#define TEST(Test, Type, Fmt, Fun, iTest) \
    do \
    { \
        Type Result; \
        int rc = Fun(Test.psz, &Result); \
        if (Result != Test.Result) \
        { \
            RTPrintf("failure: '%s' -> " Fmt " expected " Fmt ". (%s/%u)\n", Test.psz, Result, Test.Result, #Fun, iTest); \
            cErrors++; \
        } \
        else if (rc != Test.rc) \
        { \
            RTPrintf("failure: '%s' -> rc=%Rrc expected %Rrc. (%s/%u)\n", Test.psz, rc, Test.rc, #Fun, iTest); \
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
    int cErrors = 0;
    int rc;

    uint32_t l;
    rc = RTStrVersionToUInt32("asdf--234", &l);
    RTPrintf("num: %ld\n", l);


    static const struct TstU32 aTstU32[] =
    {
        { "asdf",                       VERR_NO_DIGITS, 0 },
        { "123",                        VINF_SUCCESS, 123 },
        { "45.63",                      VINF_SUCCESS, 4563 },
        { "68.54.123",                  VINF_SUCCESS, 6854123 },
        { "aasdf-1",                    VINF_SUCCESS, 1 },
        { "qwer-123.34",                VINF_SUCCESS, 12334 },
        { "aasdf45-r4545-5",            VINF_SUCCESS, 5 },
        { "foo41-r2431-6.9.8",          VINF_SUCCESS, 698 },
        { "bar43-r3517-7.1.2-beta",     VINF_SUCCESS, 712 },
        { "bar43-r3517-7.1.2.534-beta", VWRN_NUMBER_TOO_BIG, 0 },
    };
    RUN_TESTS(aTstU32, uint32_t, "%#ld", RTStrVersionToUInt32);

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstStrToVer: SUCCESS\n");
    else
        RTPrintf("tstStrToVer: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
