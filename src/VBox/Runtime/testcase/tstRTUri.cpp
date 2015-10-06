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

#if 0 && defined(RT_OS_WINDOWS) /* Enable for windows API reference results. */
# define TSTRTURI_WITH_WINDOWS_REFERENCE_RESULTS
# include <Shlwapi.h>
# include <iprt/stream.h>
#endif


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


static struct URIFILETEST
{
    const char     *pszPath;
    const char     *pszUri;
    uint32_t        uFormat;
    const char     *pszCreatedPath;
    const char     *pszCreatedUri;
} g_aCreateFileURIs[] =
{
    {   /* #0: */
        /* .pszPath          =*/ "C:\\over\\ <>#%\"{}|^[]`\\there",
        /* .pszUri           =*/ "file:///C:%5Cover%5C%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60%5Cthere",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "C:\\over\\ <>#%\"{}|^[]`\\there" - same */
        /* UrlCreateFromPath =   "file:///C:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there" - differs */
    },
    {   /* #1: */
        /* .pszPath          =*/ "/over/ <>#%\"{}|^[]`/there",
        /* .pszUri           =*/ "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\over\\ <>#%\"{}|^[]`\\there" - differs */
        /* UrlCreateFromPath =   "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there" - same */
    },
    {   /* #2: */
        /* .pszPath          =*/ NULL,
        /* .pszUri           =*/ "file://",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "" - differs */
        /* UrlCreateFromPath => 0x80070057 (E_INVALIDARG) */
    },
    {   /* #3: */
        /* .pszPath          =*/ NULL,
        /* .pszUri           =*/ "file://",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "" - differs */
        /* UrlCreateFromPath => 0x80070057 (E_INVALIDARG) */
    },
    {   /* #4: */
        /* .pszPath          =*/ "/",
        /* .pszUri           =*/ "file:///",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "" - differs */
        /* UrlCreateFromPath =   "file:///" - same */
    },
    {   /* #5: */
        /* .pszPath          =*/ "\\",
        /* .pszUri           =*/ "file:///",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "" - differs */
        /* UrlCreateFromPath =   "file:///" - same */
    },
    {   /* #6: */
        /* .pszPath          =*/ "/foo/bar",
        /* .pszUri           =*/ "file:///foo/bar",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\foo\\bar" - differs */
        /* UrlCreateFromPath =   "file:///foo/bar" - same */
    },
    {   /* #7: */
        /* .pszPath          =*/ "\\foo\\bar",
        /* .pszUri           =*/ "file:///foo%5Cbar",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\foo\\bar" - same */
        /* UrlCreateFromPath =   "file:///foo/bar" - differs */
    },
    {   /* #8: */
        /* .pszPath          =*/ "C:/over/ <>#%\"{}|^[]`/there",
        /* .pszUri           =*/ "file:///C:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "C:\\over\\ <>#%\"{}|^[]`\\there" - differs */
        /* UrlCreateFromPath =   "file:///C:/over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there" - same */
    },
    {   /* #9: */
        /* .pszPath          =*/ "\\over\\ <>#%\"{}|^[]`\\there",
        /* .pszUri           =*/ "file:///over%5C%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60%5Cthere",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\over\\ <>#%\"{}|^[]`\\there" - same */
        /* UrlCreateFromPath =   "file:///over/%20%3C%3E%23%25%22%7B%7D%7C%5E%5B%5D%60/there" - differs */
    },
    {   /* #10: */
        /* .pszPath          =*/ "/usr/bin/grep",
        /* .pszUri           =*/ "file:///usr/bin/grep",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\usr\\bin\\grep" - differs */
        /* UrlCreateFromPath =   "file:///usr/bin/grep" - same */
    },
    {   /* #11: */
        /* .pszPath          =*/ "\\usr\\bin\\grep",
        /* .pszUri           =*/ "file:///usr%5Cbin%5Cgrep",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\usr\\bin\\grep" - same */
        /* UrlCreateFromPath =   "file:///usr/bin/grep" - differs */
    },
    {   /* #12: */
        /* .pszPath          =*/ "/somerootsubdir/isos/files.lst",
        /* .pszUri           =*/ "file:///somerootsubdir/isos/files.lst",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\somerootsubdir\\isos\\files.lst" - differs */
        /* UrlCreateFromPath =   "file:///somerootsubdir/isos/files.lst" - same */
    },
    {   /* #13: */
        /* .pszPath          =*/ "\\not-a-cifsserver\\isos\\files.lst",
        /* .pszUri           =*/ "file:///not-a-cifsserver%5Cisos%5Cfiles.lst",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\not-a-cifsserver\\isos\\files.lst" - same */
        /* UrlCreateFromPath =   "file:///not-a-cifsserver/isos/files.lst" - differs */
    },
    {   /* #14: */
        /* .pszPath          =*/ "/rootsubdir/isos/files.lst",
        /* .pszUri           =*/ "file:///rootsubdir/isos/files.lst",
        /* .uFormat          =*/ URI_FILE_FORMAT_UNIX,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\rootsubdir\\isos\\files.lst" - differs */
        /* UrlCreateFromPath =   "file:///rootsubdir/isos/files.lst" - same */
    },
    {   /* #15: */
        /* .pszPath          =*/ "\\not-a-cifsserver-either\\isos\\files.lst",
        /* .pszUri           =*/ "file:///not-a-cifsserver-either%5Cisos%5Cfiles.lst",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ NULL,    /* Same as pszUri. */
        /* PathCreateFromUrl =   "\\not-a-cifsserver-either\\isos\\files.lst" - same */
        /* UrlCreateFromPath =   "file:///not-a-cifsserver-either/isos/files.lst" - differs */
    },
#if 0 /** @todo r=bird: this ain't working right. It's in the wikipedia article on file:// ... */
    {   /* #16: */
        /* .pszPath          =*/ "\\\\cifsserver\\isos\\files.lst",
        /* .pszUri           =*/ "file:////cifsserver/isos/files.lst",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ "file://cifsserver/isos/files.lst",
        /* PathCreateFromUrl =   "\\\\cifsserver\\isos\\files.lst" - same */
        /* UrlCreateFromPath =   "file:////cifsserver/isos/files.lst" - differs */
    },
    {   /* #17: */
        /* .pszPath          =*/ "c:boot.ini",
        /* .pszUri           =*/ "file://localhost/c:boot.ini",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ "file:///c:boot.ini",
        /* PathCreateFromUrl =   "c:boot.ini" - same */
        /* UrlCreateFromPath =   "file:///c:boot.ini" - same */
    },
    {   /* #18: */
        /* .pszPath          =*/ "c:boot.ini",
        /* .pszUri           =*/ "file:///c|boot.ini",
        /* .uFormat          =*/ URI_FILE_FORMAT_WIN,
        /* .pszCreatedPath   =*/ NULL,    /* Same as pszPath. */
        /* .pszCreatedUri    =*/ "file:///c:boot.ini",
        /* PathCreateFromUrl =   "c:boot.ini" - same */
        /* UrlCreateFromPath =   "file:///c:boot.ini" - same */
    },
#endif
};

#ifdef TSTRTURI_WITH_WINDOWS_REFERENCE_RESULTS

static void tstPrintCString(const char *pszString)
{
    if (pszString)
    {
        char ch;
        RTPrintf("\"");
        while ((ch = *pszString++) != '\0')
        {
            if (ch >= 0x20 && ch < 0x7f)
                switch (ch)
                {
                    default:
                        RTPrintf("%c", ch);
                        break;
                    case '\\':
                    case '"':
                        RTPrintf("\\%c", ch);
                        break;
                }
            else
                RTPrintf("\\x%02X", ch); /* good enough */
        }
        RTPrintf("\"");
    }
    else
        RTPrintf("NULL");
}

static void tstWindowsReferenceResults(void)
{
    /*
     * Feed the g_aCreateFileURIs values as input to the Windows
     * PathCreateFromUrl and URlCreateFromPath APIs and print the results.
     *
     * We reproduce the entire source file content of g_aCreateFileURIs here.
     */
    for (size_t i = 0; i < RT_ELEMENTS(g_aCreateFileURIs); ++i)
    {
        RTPrintf("    {   /* #%u: */\n", i);
        RTPrintf("        /* .pszPath          =*/ ");
        tstPrintCString(g_aCreateFileURIs[i].pszPath);
        RTPrintf(",\n");
        RTPrintf("        /* .pszUri           =*/ ");
        tstPrintCString(g_aCreateFileURIs[i].pszUri);
        RTPrintf(",\n");
        RTPrintf("        /* .uFormat          =*/ %s,\n",
                   g_aCreateFileURIs[i].uFormat == URI_FILE_FORMAT_WIN  ? "URI_FILE_FORMAT_WIN"
                 : g_aCreateFileURIs[i].uFormat == URI_FILE_FORMAT_UNIX ? "URI_FILE_FORMAT_UNIX"
                 : g_aCreateFileURIs[i].uFormat == URI_FILE_FORMAT_AUTO ? "URI_FILE_FORMAT_AUTO" : "URI_FILE_FORMAT_INVALID");
        RTPrintf("        /* .pszCreatedPath   =*/ ");
        if (g_aCreateFileURIs[i].pszCreatedPath == NULL)
            RTPrintf("NULL,    /* Same as pszPath. */\n");
        else
        {
            tstPrintCString(g_aCreateFileURIs[i].pszCreatedPath);
            RTPrintf(",\n");
        }
        RTPrintf("        /* .pszCreatedUri    =*/ ");
        if (g_aCreateFileURIs[i].pszCreatedUri == NULL)
            RTPrintf("NULL,    /* Same as pszUri. */\n");
        else
        {
            tstPrintCString(g_aCreateFileURIs[i].pszCreatedUri);
            RTPrintf(",\n");
        }

        /*
         * PathCreateFromUrl
         */
        PRTUTF16 pwszInput = NULL;
        if (g_aCreateFileURIs[i].pszUri)
            RTTESTI_CHECK_RC_OK_RETV(RTStrToUtf16(g_aCreateFileURIs[i].pszUri, &pwszInput));
        WCHAR wszResult[_1K];
        DWORD cwcResult = RT_ELEMENTS(wszResult);
        RT_ZERO(wszResult);
        HRESULT hrc = PathCreateFromUrlW(pwszInput, wszResult, &cwcResult, 0 /*dwFlags*/);
        RTUtf16Free(pwszInput);

        if (SUCCEEDED(hrc))
        {
            char *pszResult;
            RTTESTI_CHECK_RC_OK_RETV(RTUtf16ToUtf8(wszResult, &pszResult));
            RTPrintf("        /* PathCreateFromUrl =   ");
            tstPrintCString(pszResult);
            if (   g_aCreateFileURIs[i].pszPath
                && strcmp(pszResult, g_aCreateFileURIs[i].pszCreatedPath
                                     ? g_aCreateFileURIs[i].pszCreatedPath : g_aCreateFileURIs[i].pszPath) == 0)
                RTPrintf(" - same */\n");
            else
                RTPrintf(" - differs */\n");
            RTStrFree(pszResult);
        }
        else
            RTPrintf("        /* PathCreateFromUrl => %#x (%Rhrc) */\n", hrc, hrc);

        /*
         * UrlCreateFromPath + UrlEscape
         */
        pwszInput = NULL;
        if (g_aCreateFileURIs[i].pszPath)
            RTTESTI_CHECK_RC_OK_RETV(RTStrToUtf16(g_aCreateFileURIs[i].pszPath, &pwszInput));
        RT_ZERO(wszResult);
        cwcResult = RT_ELEMENTS(wszResult);
        hrc = UrlCreateFromPathW(pwszInput, wszResult, &cwcResult, 0 /*dwFlags*/);
        RTUtf16Free(pwszInput);

        if (SUCCEEDED(hrc))
        {
            WCHAR wszResult2[_1K];
            DWORD cwcResult2 = RT_ELEMENTS(wszResult2);
            hrc = UrlEscapeW(wszResult, wszResult2, &cwcResult2, URL_DONT_ESCAPE_EXTRA_INFO );
            if (SUCCEEDED(hrc))
            {
                char *pszResult;
                RTTESTI_CHECK_RC_OK_RETV(RTUtf16ToUtf8(wszResult2, &pszResult));
                RTPrintf("        /* UrlCreateFromPath =   ");
                tstPrintCString(pszResult);
                if (   g_aCreateFileURIs[i].pszUri
                    && strcmp(pszResult, g_aCreateFileURIs[i].pszCreatedUri
                                         ? g_aCreateFileURIs[i].pszCreatedUri : g_aCreateFileURIs[i].pszUri) == 0)
                    RTPrintf(" - same */\n");
                else
                    RTPrintf(" - differs */\n");
                RTStrFree(pszResult);
            }
            else
                RTPrintf("        /* UrlEscapeW        => %#x (%Rhrc) */\n", hrc, hrc);
        }
        else
            RTPrintf("        /* UrlCreateFromPath => %#x (%Rhrc) */\n", hrc, hrc);
        RTPrintf("    },\n");
    }
}

#endif /* TSTRTURI_WITH_WINDOWS_REFERENCE_RESULTS */


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

#ifdef TSTRTURI_WITH_WINDOWS_REFERENCE_RESULTS
    tstWindowsReferenceResults();
#endif

    /* File Uri path */
    RTTestISub("RTUriFilePath");
    for (size_t i = 0; i < RT_ELEMENTS(g_aCreateFileURIs); ++i)
        CHECK_STR_API(RTUriFilePath(g_aCreateFileURIs[i].pszUri, g_aCreateFileURIs[i].uFormat),
                      g_aCreateFileURIs[i].pszCreatedPath ? g_aCreateFileURIs[i].pszCreatedPath : g_aCreateFileURIs[i].pszPath);

    /* File Uri creation */
    RTTestISub("RTUriFileCreate");
    for (size_t i = 0; i < RT_ELEMENTS(g_aCreateFileURIs); ++i)
        CHECK_STR_API(RTUriFileCreate(g_aCreateFileURIs[i].pszPath),
                      g_aCreateFileURIs[i].pszCreatedUri ? g_aCreateFileURIs[i].pszCreatedUri : g_aCreateFileURIs[i].pszUri);

    return RTTestSummaryAndDestroy(hTest);
}

