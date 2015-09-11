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
    uint32_t    uPort;
} g_aTests[] =
{
    {   /* #0 */
        "foo://tt:tt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #1 */
        "foo://tt:tt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #2 */
        "foo://tt:tt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ "/over/ <>#%\"{}|^[]`/there",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
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
        /*.uPort        =*/ UINT32_MAX,
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
        /*.uPort        =*/ UINT32_MAX,
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
        /*.uPort        =*/ UINT32_MAX,
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
        /*.uPort        =*/ UINT32_MAX,
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
        /*.uPort        =*/ UINT32_MAX,
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
        /*.uPort        =*/ UINT32_MAX,
    },
    {   /* #9 */
        "foo://tt:tt@example.com:8042/?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ "/",
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #10 */
        "foo://tt:tt@example.com:8042/",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ "/",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #11 */
        "foo://tt:tt@example.com:8042?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ "name= <>#%\"{}|^[]`ferret",
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #12 */
        "foo://tt:tt@example.com:8042#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ "nose <>#%\"{}|^[]`",
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #13 */
        "foo://tt:tt@example.com:8042",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "tt:tt@example.com:8042",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ "tt",
        /*.pszPassword  =*/ "tt",
        /*.uPort        =*/ 8042,
    },
    {   /* #14 */
        "foo:///",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "",
        /*.pszPath      =*/ "/",
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
    },
    {   /* #15 */
        "foo://",
        /*.pszScheme    =*/ "foo",
        /*.pszAuthority =*/ "",
        /*.pszPath      =*/ NULL,
        /*.pszQuery     =*/ NULL,
        /*.pszFragment  =*/ NULL,
        /*.pszUsername  =*/ NULL,
        /*.pszPassword  =*/ NULL,
        /*.uPort        =*/ UINT32_MAX,
    },
};


static const char *g_apcszTestURIs[] =
{
    "foo://tt:tt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo://tt:tt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret",
    "foo://tt:tt@example.com:8042/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
    "foo:tt@example.com",
    "foo:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "urn:example:animal:ferret:nose",
    "foo:?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo:#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo://tt:tt@example.com:8042/?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo://tt:tt@example.com:8042/",
    "foo://tt:tt@example.com:8042?name=%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60ferret#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo://tt:tt@example.com:8042#nose%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60",
    "foo://tt:tt@example.com:8042",
    "foo:///",
    "foo://"
};

static const char *g_apcszCreateURIs[][5] =
{
    { "foo", "tt:tt@example.com:8042", "/over/ <>#%\"{}|^[]`/there", "name= <>#%\"{}|^[]`ferret", "nose <>#%\"{}|^[]`" },
    { "foo", "tt:tt@example.com:8042", "/over/ <>#%\"{}|^[]`/there", "name= <>#%\"{}|^[]`ferret", NULL },
    { "foo", "tt:tt@example.com:8042", "/over/ <>#%\"{}|^[]`/there", NULL, NULL },
    { "foo", NULL, "tt@example.com", NULL, NULL },
    { "foo", NULL, "/over/ <>#%\"{}|^[]`/there", "name= <>#%\"{}|^[]`ferret", "nose <>#%\"{}|^[]`" },
    { "foo", NULL, "/over/ <>#%\"{}|^[]`/there", NULL,  "nose <>#%\"{}|^[]`" },
    { "urn", NULL, "example:animal:ferret:nose", NULL, NULL },
    { "foo", NULL, NULL, "name= <>#%\"{}|^[]`ferret", "nose <>#%\"{}|^[]`" },
    { "foo", NULL, NULL, NULL, "nose <>#%\"{}|^[]`" },
    { "foo", "tt:tt@example.com:8042", "/", "name= <>#%\"{}|^[]`ferret", "nose <>#%\"{}|^[]`" },
    { "foo", "tt:tt@example.com:8042", "/", NULL, NULL },
    { "foo", "tt:tt@example.com:8042", NULL, "name= <>#%\"{}|^[]`ferret", "nose <>#%\"{}|^[]`" },
    { "foo", "tt:tt@example.com:8042", NULL, NULL, "nose <>#%\"{}|^[]`" },
    { "foo", "tt:tt@example.com:8042", NULL, NULL, NULL },
    { "foo", "", "/", NULL, NULL },
    { "foo", "", NULL, NULL, NULL }
};

struct URIFILETEST
{
    const char *pcszPath;
    const char *pcszUri;
    uint32_t uFormat;
}
g_apCreateFileURIs[] =
{
    { "C:\\over\\ <>#%\"{}|^[]`\\there", "file:///C:%5Cover%5C%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60%5Cthere", URI_FILE_FORMAT_WIN },
    { "/over/ <>#%\"{}|^[]`/there", "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there", URI_FILE_FORMAT_UNIX },
    { "/", "file:///", URI_FILE_FORMAT_UNIX },
    { "/C:/over/ <>#%\"{}|^[]`/there", "file:///C:%5Cover%5C%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60%5Cthere", URI_FILE_FORMAT_UNIX },
    { "\\over\\ <>#%\"{}|^[]`\\there", "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there", URI_FILE_FORMAT_WIN }
};

/**
 * Basic API checks.
 */
static void tstScheme(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriScheme(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstAuthority(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriAuthority(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstAuthorityUsername(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriAuthorityUsername(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    RTStrFree(pszResult);
}

static void tstAuthorityPassword(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriAuthorityPassword(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    RTStrFree(pszResult);
}

static void tstAuthorityPort(size_t idxTest, const char *pszUri, uint32_t uTest)
{
    uint32_t uResult = RTUriAuthorityPort(pszUri);
    RTTESTI_CHECK_MSG_RETV(uResult == uTest, ("#%u: Result %#x != %#x (%s)", idxTest, uResult, uTest, pszUri));
}

static void tstPath(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriPath(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstQuery(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriQuery(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstFragment(size_t idxTest, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriFragment(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("#%u: Result '%s' != '%s'", idxTest, pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

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

    /* Scheme */
    RTTestISub("RTUriScheme");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstScheme(i, g_aTests[i].pszUri, g_aTests[i].pszScheme);

    /* Authority */
    RTTestISub("RTUriAuthority");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
    {
        tstAuthority(i, g_aTests[i].pszUri, g_aTests[i].pszAuthority);
        tstAuthorityUsername(i, g_aTests[i].pszUri, g_aTests[i].pszUsername);
        tstAuthorityPassword(i, g_aTests[i].pszUri, g_aTests[i].pszPassword);
        tstAuthorityPort(i, g_aTests[i].pszUri, g_aTests[i].uPort);
    }

    /* Path */
    RTTestISub("RTUriPath");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstPath(i, g_aTests[i].pszUri, g_aTests[i].pszPath);

    /* Query */
    RTTestISub("RTUriQuery");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstQuery(i, g_aTests[i].pszUri, g_aTests[i].pszQuery);

    /* Fragment */
    RTTestISub("RTUriFragment");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstFragment(i, g_aTests[i].pszUri, g_aTests[i].pszFragment);

    /* Creation */
    RTTestISub("RTUriCreate");
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aTests); i++)
        tstCreate(i, g_aTests[i].pszScheme, g_aTests[i].pszAuthority, g_aTests[i].pszPath,
                  g_aTests[i].pszQuery, g_aTests[i].pszFragment, g_aTests[i].pszUri);

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

