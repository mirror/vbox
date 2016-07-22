/* $Id$ */
/** @file
 * IPRT Testcase - IPv4.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
#include <iprt/net.h>

#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define CHECKADDR(String, rcExpected, ExpectedAddr)                     \
    do {                                                                \
        RTNETADDRIPV4 Addr;                                             \
        int rc2 = RTNetStrToIPv4Addr(String, &Addr);                    \
        if ((rcExpected) && !rc2)                                       \
        {                                                               \
            RTTestIFailed("at line %d: '%s': expected %Rrc got %Rrc\n", \
                          __LINE__, String, (rcExpected), rc2);         \
        }                                                               \
        else if (   (rcExpected) != rc2                                 \
                 || (   rc2 == VINF_SUCCESS                             \
                     && RT_H2N_U32_C(ExpectedAddr) != Addr.u))          \
        {                                                               \
            RTTestIFailed("at line %d: '%s': expected %Rrc got %Rrc,"   \
                          " expected address %RTnaipv4 got %RTnaipv4\n", \
                          __LINE__, String, rcExpected, rc2,            \
                          RT_H2N_U32_C(ExpectedAddr), Addr.u);          \
        }                                                               \
    } while (0)

#define GOODADDR(String, ExpectedAddr) \
    CHECKADDR(String, VINF_SUCCESS, ExpectedAddr)

#define BADADDR(String) \
    CHECKADDR(String, VERR_INVALID_PARAMETER, 0)


#define CHECKADDREX(String, Trailer, rcExpected, ExpectedAddr)          \
    do {                                                                \
        RTNETADDRIPV4 Addr;                                             \
        const char *strAll = String /* concat */ Trailer;               \
        const char *pTrailer = strAll + sizeof(String) - 1;             \
        char *pNext = NULL;                                             \
        int rc2 = RTNetStrToIPv4AddrEx(strAll, &Addr, &pNext);          \
        if ((rcExpected) && !rc2)                                       \
        {                                                               \
            RTTestIFailed("at line %d: '%s': expected %Rrc got %Rrc\n", \
                          __LINE__, String, (rcExpected), rc2);         \
        }                                                               \
        else if ((rcExpected) != rc2                                    \
                 || (rc2 == VINF_SUCCESS                                \
                     && (RT_H2N_U32_C(ExpectedAddr) != Addr.u           \
                         || pTrailer != pNext)))                        \
        {                                                               \
            RTTestIFailed("at line %d: '%s': expected %Rrc got %Rrc,"   \
                          " expected address %RTnaipv4 got %RTnaipv4"   \
                          " expected trailer \"%s\" got %s%s%s"         \
                          "\n",                                         \
                          __LINE__, String, rcExpected, rc2,            \
                          RT_H2N_U32_C(ExpectedAddr), Addr.u,           \
                          pTrailer,                                     \
                          pNext ? "\"" : "",                            \
                          pNext ? pNext : "(null)",                     \
                          pNext ? "\"" : "");                           \
        }                                                               \
    } while (0)


#define CHECKANY(String, fExpected)                                     \
    do {                                                                \
        bool fRc = RTNetStrIsIPv4AddrAny(String);                       \
        if (fRc != fExpected)                                           \
        {                                                               \
            RTTestIFailed("at line %d: '%s':"                           \
                          " expected %RTbool got %RTbool\n",            \
                          __LINE__, (String), fExpected, fRc);          \
        }                                                               \
    } while (0)

#define IS_ANY(String)  CHECKANY((String), true)
#define NOT_ANY(String) CHECKANY((String), false)


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTNetIPv4", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    GOODADDR("1.2.3.4",         0x01020304);
    GOODADDR("0.0.0.0",         0x00000000);
    GOODADDR("255.255.255.255", 0xFFFFFFFF);

    /* leading and trailing whitespace is allowed */
    GOODADDR(" 1.2.3.4 ",       0x01020304);
    GOODADDR("\t1.2.3.4\t",     0x01020304);

    BADADDR("1.2.3.4x");
    BADADDR("1.2.3.4.");
    BADADDR("1.2.3");
    BADADDR("0x1.2.3.4");
    BADADDR("666.2.3.4");
    BADADDR("1.666.3.4");
    BADADDR("1.2.666.4");
    BADADDR("1.2.3.666");

    /*
     * Parsing itself is covered by the tests above, here we only
     * check trailers
     */
    CHECKADDREX("1.2.3.4",  "",   VINF_SUCCESS,           0x01020304);
    CHECKADDREX("1.2.3.4",  " ",  VINF_SUCCESS,           0x01020304);
    CHECKADDREX("1.2.3.4",  "x",  VINF_SUCCESS,           0x01020304);
    CHECKADDREX("1.2.3.444", "",  VERR_INVALID_PARAMETER,          0);


    IS_ANY("0.0.0.0");
    IS_ANY("\t 0.0.0.0 \t");

    NOT_ANY("1.1.1.1");         /* good address, but not INADDR_ANY */
    NOT_ANY("0.0.0.0x");        /* bad address */

    return RTTestSummaryAndDestroy(hTest);
}
