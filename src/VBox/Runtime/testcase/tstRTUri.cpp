/* $Id$ */
/** @file
 * IPRT Testcase - URI parsing and creation.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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
#include <iprt/uri.h>

#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Test data                                                                                                                    *
*********************************************************************************************************************************/

static struct
{
    const char *pszUri;
    const char *pszScheme;
    const char *pszAuthority;
    const char *pszPath;
    const char *pszQuery;
    const char *pszFragment;

    const char *pszUsername;
    const char *pszPassword;
    const char *pszHost;
    uint32_t    uPort;

    const char *pszCreated;
} g_aTests[] =
{
    {   /* #0 */
        "foo://tt:yt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #1 */
        "foo://tt:yt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #2 */
        "foo://tt:yt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #3 */
        "foo:tt@example.com",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ NULL,
        /*.pszPath      =*/ "tt@example.com",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #4 */
        "foo:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ NULL,
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #5 */
        "foo:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ NULL,
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #6 */
        "urn:example:animal:ferret:nose",
        /*.pszScheme    =*/ "urn",
        /*.pszAuthority =*/ NULL,
        /*.pszPath      =*/ "example:animal:ferret:nose",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #7 */
        "foo:?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ NULL,
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #8 */
        "foo:#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ NULL,
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #9 */
        "foo://tt:yt@example.com:8042/?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ "/",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #10 */
        "foo://tt:yt@example.com:8042/",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ "/",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #11 */
        "foo://tt:yt@example.com:8042?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #12 */
        "foo://tt:yt@example.com:8042#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #13 */
        "foo://tt:yt@example.com:8042",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:yt@example.com:8042",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "yt",
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ 8042,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #14 */
        "file:///dir/dir/file",
        /*.pszScheme    =*/ "file",
        /*.pszAuthority =*/ "",
        /*.pszPath      =*/ "/dir/dir/file",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #15 */
        "foo:///",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "",
        /*.pszPath      =*/ "/",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #16 */
        "foo://",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ NULL /* same as pszUri*/,
    },
    {   /* #17 - UTF-8 escape sequences. */
        "http://example.com/%ce%b3%ce%bb%cf%83%ce%b1%20%e0%a4%95\xe0\xa4\x95",
        /*.pszScheme    =*/ "http",
        /*.pszAuthority =*/ "example.com",
        /*.pszPath      =*/ "/\xce\xb3\xce\xbb\xcf\x83\xce\xb1 \xe0\xa4\x95\xe0\xa4\x95",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.pszHost      =*/ "example.com",
        /*.uPort        =*/ UINT32_MAX,
        /*.pszCreated   =*/ "http://example.com/\xce\xb3\xce\xbb\xcf\x83\xce\xb1%20\xe0\xa4\x95\xe0\xa4\x95",
    },
};


struct URIFILETEST
{
    const char *pcszPath;
    const char *pcszUri;
    uint32_t uFormat;
}
g_apCreateFileURIs[] =
{
    {
        "C:\\over\\ <>#%\"{}|^[]`\\there",
        "file:///C:%5Cover%5C%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60%5Cthere",
        URI_FILE_FORMAT_WIN
    },
    {
        "/over/ <>#%\"{}|^[]`/there",
        "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
        URI_FILE_FORMAT_UNIX
    },
    {
        "/",
        "file:///",
        URI_FILE_FORMAT_UNIX
    },
    {
        "/C:/over/ <>#%\"{}|^[]`/there",
        "file:///C:%5Cover%5C%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60%5Cthere",
        URI_FILE_FORMAT_UNIX
    },
    {
        "\\over\\ <>#%\"{}|^[]`\\there",
        "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
        URI_FILE_FORMAT_WIN
    },
    {
        "/",
        "file:///",
        URI_FILE_FORMAT_UNIX
    },
    {
        "\\",
        "file:///",
        URI_FILE_FORMAT_WIN
    },
};



static void tstCreate(size_t idxTest, const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery, const char *pszFragment, const char *pszTest)
{
    char *pszResult = RTUriCreate(pszScheme, pszAuthority, pszPath, pszQuery, pszFragment);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
    return;
}

static void tstFileCreate(size_t idxTest, const char *pszPath, const char *pszTest)
{
    char *pszResult = RTUriFileCreate(pszPath);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
    return;
}

static void tstFilePath(size_t idxTest, const char *pszUri, const char *pszTest, uint32_t uFormat)
{
    char *pszResult = RTUriFilePath(pszUri, uFormat);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
    return;
}

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTUri", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

#define CHECK_STR_API(a_Call, a_pszExpected) \
        do { \
            char *pszTmp = a_Call; \
            if (a_pszExpected) \
            { \
                if (!pszTmp) \
                    RTTestIFailed("#%u: %s returns NULL, expected '%s'", i, #a_Call, a_pszExpected); \
                else if (strcmp(pszTmp, a_pszExpected)) \
                        RTTestIFailed("#%u: %s returns '%s', expected '%s'", i, #a_Call, pszTmp, a_pszExpected); \
            } \
            else if (pszTmp) \
                RTTestIFailed("#%u: %s returns '%s', expected NULL", i, #a_Call, pszTmp); \
            RTStrFree(pszTmp); \
        } while (0)

    RTTestISub("RTUriParse & RTUriParsed*");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
    {
        RTURIPARSED Parsed;
        RTTESTI_CHECK_RC(rc = RTUriParse(g_aTests[i].pszUri, &Parsed), VINF_SUCCESS);
        if (RT_SUCCESS(rc))
        {
            CHECK_STR_API(RTUriParsedScheme(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszScheme);
            CHECK_STR_API(RTUriParsedAuthority(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszAuthority);
            CHECK_STR_API(RTUriParsedAuthorityUsername(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszUsername);
            CHECK_STR_API(RTUriParsedAuthorityPassword(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszPassword);
            CHECK_STR_API(RTUriParsedAuthorityHost(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszHost);
            CHECK_STR_API(RTUriParsedPath(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszPath);
            CHECK_STR_API(RTUriParsedQuery(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszQuery);
            CHECK_STR_API(RTUriParsedFragment(g_aTests[i].pszUri, &Parsed), g_aTests[i].pszFragment);
            uint32_t uPort = RTUriParsedAuthorityPort(g_aTests[i].pszUri, &Parsed);
            if (uPort != g_aTests[i].uPort)
                RTTestIFailed("#%u: RTUriParsedAuthorityPort returns %#x, expected %#x", i, uPort, g_aTests[i].uPort);
        }
    }

    /* Creation */
    RTTestISub("RTUriCreate");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
        CHECK_STR_API(RTUriCreate(g_aTests[i].pszScheme, g_aTests[i].pszAuthority, g_aTests[i].pszPath,
                                  g_aTests[i].pszQuery, g_aTests[i].pszFragment),
                      g_aTests[i].pszCreated ? g_aTests[i].pszCreated : g_aTests[i].pszUri);

    /* File Uri path */
    RTTestISub("RTUriFilePath");
    for (size_t i = 0; i < RT_ELEMENTS(g_apCreateFileURIs); ++i)
        tstFilePath(i, g_apCreateFileURIs[i].pcszUri, g_apCreateFileURIs[i].pcszPath, g_apCreateFileURIs[i].uFormat);

    /* File Uri creation */
    RTTestISub("RTUriFileCreate");
    for (size_t i = 0; i < 3; ++i)
        tstFileCreate(i, g_apCreateFileURIs[i].pcszPath, g_apCreateFileURIs[i].pcszUri);

    return RTTestSummaryAndDestroy(hTest);
}

