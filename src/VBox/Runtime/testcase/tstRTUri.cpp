/* $Id$ */
/** @file
 * IPRT Testcase - URI parsing and creation.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/uri.h>

#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/test.h>

/*******************************************************************************
*   Test data                                                                  *
*******************************************************************************/

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

static const char *g_apcszSchemeResult[] =
{
    "foo",
    "foo",
    "foo",
    "foo",
    "foo",
    "foo",
    "urn",
    "foo",
    "foo",
    "foo",
    "foo",
    "foo",
    "foo",
    "foo",
    "foo",
    "foo"
};

static const char *g_apcszAuthorityResult[] =
{
    "tt:tt@example.com:8042",
    "tt:tt@example.com:8042",
    "tt:tt@example.com:8042",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "tt:tt@example.com:8042",
    "tt:tt@example.com:8042",
    "tt:tt@example.com:8042",
    "tt:tt@example.com:8042",
    "tt:tt@example.com:8042",
    NULL,
    NULL
};

static const char *g_apcszPathResult[] =
{
    "/over/ <>#%\"{}|^[]`/there",
    "/over/ <>#%\"{}|^[]`/there",
    "/over/ <>#%\"{}|^[]`/there",
    "tt@example.com",
    "/over/ <>#%\"{}|^[]`/there",
    "/over/ <>#%\"{}|^[]`/there",
    "example:animal:ferret:nose",
    NULL,
    NULL,
    "/",
    "/",
    NULL,
    NULL,
    NULL,
    "/",
    NULL
};

static const char *g_apcszQueryResult[] =
{
    "name= <>#%\"{}|^[]`ferret",
    "name= <>#%\"{}|^[]`ferret",
    NULL,
    NULL,
    "name= <>#%\"{}|^[]`ferret",
    NULL,
    NULL,
    "name= <>#%\"{}|^[]`ferret",
    NULL,
    "name= <>#%\"{}|^[]`ferret",
    NULL,
    "name= <>#%\"{}|^[]`ferret",
    NULL,
    NULL,
    NULL,
    NULL
};

static const char *g_apcszFragmentResult[] =
{
    "nose <>#%\"{}|^[]`",
    NULL,
    NULL,
    NULL,
    "nose <>#%\"{}|^[]`",
    "nose <>#%\"{}|^[]`",
    NULL,
    "nose <>#%\"{}|^[]`",
    "nose <>#%\"{}|^[]`",
    "nose <>#%\"{}|^[]`",
    NULL,
    "nose <>#%\"{}|^[]`",
    "nose <>#%\"{}|^[]`",
    NULL,
    NULL,
    NULL
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
    { "over\\ <>#%\"{}|^[]`\\there", "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there", URI_FILE_FORMAT_WIN }
};

/**
 * Basic API checks.
 */
static void tstScheme(size_t iCount, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriScheme(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstAuthority(size_t iCount, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriAuthority(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstPath(size_t iCount, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriPath(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstQuery(size_t iCount, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriQuery(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstFragment(size_t iCount, const char *pszUri, const char *pszTest)
{
    char *pszResult = RTUriFragment(pszUri);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
}

static void tstCreate(size_t iCount, const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery, const char *pszFragment, const char *pszTest)
{
    char *pszResult = RTUriCreate(pszScheme, pszAuthority, pszPath, pszQuery, pszFragment);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
    return;
}

static void tstFileCreate(size_t iCount, const char *pszPath, const char *pszTest)
{
    char *pszResult = RTUriFileCreate(pszPath);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

    if (pszResult)
        RTStrFree(pszResult);
    return;
}

static void tstFilePath(size_t iCount, const char *pszUri, const char *pszTest, uint32_t uFormat)
{
    char *pszResult = RTUriFilePath(pszUri, uFormat);
    if (pszTest)
    {
        RTTESTI_CHECK_MSG_RETV(pszResult, ("Result '%s' != '%s'", pszResult, pszTest));
        RTTESTI_CHECK_MSG(RTStrCmp(pszResult, pszTest) == 0, ("Result '%s' != '%s'", pszResult, pszTest));
    }
    else
        RTTESTI_CHECK_MSG(!pszResult, ("Result '%s' != '%s'", pszResult, pszTest));

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
    RTTestISubF("RTUriScheme");
    Assert(RT_ELEMENTS(g_apcszTestURIs) == RT_ELEMENTS(g_apcszSchemeResult));
    for (size_t i = 0; i < RT_ELEMENTS(g_apcszTestURIs); ++i)
        tstScheme(i+1, g_apcszTestURIs[i], g_apcszSchemeResult[i]);

    /* Authority */
    RTTestISubF("RTUriAuthority");
    Assert(RT_ELEMENTS(g_apcszTestURIs) == RT_ELEMENTS(g_apcszAuthorityResult));
    for (size_t i = 0; i < RT_ELEMENTS(g_apcszTestURIs); ++i)
        tstAuthority(i+1, g_apcszTestURIs[i], g_apcszAuthorityResult[i]);

    /* Path */
    RTTestISubF("RTUriPath");
    Assert(RT_ELEMENTS(g_apcszTestURIs) == RT_ELEMENTS(g_apcszPathResult));
    for (size_t i = 0; i < RT_ELEMENTS(g_apcszTestURIs); ++i)
        tstPath(i+1, g_apcszTestURIs[i], g_apcszPathResult[i]);

    /* Query */
    RTTestISubF("RTUriQuery");
    Assert(RT_ELEMENTS(g_apcszTestURIs) == RT_ELEMENTS(g_apcszQueryResult));
    for (size_t i = 0; i < RT_ELEMENTS(g_apcszTestURIs); ++i)
        tstQuery(i+1, g_apcszTestURIs[i], g_apcszQueryResult[i]);

    /* Fragment */
    RTTestISubF("RTUriFragment");
    Assert(RT_ELEMENTS(g_apcszTestURIs) == RT_ELEMENTS(g_apcszFragmentResult));
    for (size_t i = 0; i < RT_ELEMENTS(g_apcszTestURIs); ++i)
        tstFragment(i+1, g_apcszTestURIs[i], g_apcszFragmentResult[i]);

    /* Creation */
    RTTestISubF("RTUriCreate");
    Assert(RT_ELEMENTS(g_apcszTestURIs) == RT_ELEMENTS(g_apcszCreateURIs));
    for (size_t i = 0; i < RT_ELEMENTS(g_apcszTestURIs); ++i)
        tstCreate(i+1, g_apcszCreateURIs[i][0], g_apcszCreateURIs[i][1], g_apcszCreateURIs[i][2],
                  g_apcszCreateURIs[i][3], g_apcszCreateURIs[i][4], g_apcszTestURIs[i]);

    /* File Uri path */
    RTTestISubF("RTUriFilePath");
    for (size_t i = 0; i < RT_ELEMENTS(g_apCreateFileURIs); ++i)
        tstFilePath(i+1, g_apCreateFileURIs[i].pcszUri, g_apCreateFileURIs[i].pcszPath, g_apCreateFileURIs[i].uFormat);

    /* File Uri creation */
    RTTestISubF("RTUriFileCreate");
    for (size_t i = 0; i < 3; ++i)
        tstFileCreate(i+1, g_apCreateFileURIs[i].pcszPath, g_apCreateFileURIs[i].pcszUri);

    return RTTestSummaryAndDestroy(hTest);
}

