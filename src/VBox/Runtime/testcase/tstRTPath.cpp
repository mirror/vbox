/* $Id$ */
/** @file
 * IPRT Testcase - Test various path functions.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>


static void testParserAndSplitter(RTTEST hTest)
{
    static struct
    {
        uint16_t    cComps;
        uint16_t    cchPath;
        uint16_t    offSuffix;
        int16_t     offName;    /**< RTPathParseSimple */
        uint16_t    cchDir;     /**< RTPathParseSimple */
        const char *pszPath;
        uint16_t    fProps;
        uint32_t    fFlags;
    } const s_aTests[] =
    {
        { 2,  5,  5,  -1,  4, "/bin/",            RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_DIR_SLASH,                                                RTPATH_STR_F_STYLE_UNIX },
        { 2, 13,  9,   3,  3, "C:/Config.sys",    RTPATH_PROP_VOLUME | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX,       RTPATH_STR_F_STYLE_DOS },
        { 2, 13, 10,   4,  4, "C://Config.sys",   RTPATH_PROP_VOLUME | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX | RTPATH_PROP_EXTRA_SLASHES, RTPATH_STR_F_STYLE_DOS },
        { 2, 12,  8,   2,  2, "C:Config.sys",     RTPATH_PROP_VOLUME | RTPATH_PROP_RELATIVE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX,                                RTPATH_STR_F_STYLE_DOS },
        { 1, 10,  6,   0,  0, "Config.sys",       RTPATH_PROP_RELATIVE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX,                                                     RTPATH_STR_F_STYLE_DOS },
        { 3, 15, 11,   7,  6, "C:/Win/file.ext",  RTPATH_PROP_VOLUME | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX ,      RTPATH_STR_F_STYLE_DOS },
        { 1,  4,  4,  -1,  4, "//./",             RTPATH_PROP_UNC | RTPATH_PROP_SPECIAL_UNC | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE,                                                      RTPATH_STR_F_STYLE_DOS },
        { 2,  5,  5,   4,  4, "//./f",            RTPATH_PROP_UNC | RTPATH_PROP_SPECIAL_UNC | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                               RTPATH_STR_F_STYLE_DOS },
        { 2,  5,  6,   5,  5, "//.//f",           RTPATH_PROP_UNC | RTPATH_PROP_SPECIAL_UNC | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_EXTRA_SLASHES,   RTPATH_STR_F_STYLE_DOS },
        { 3,  7,  7,   6,  5, "//././f",          RTPATH_PROP_UNC | RTPATH_PROP_SPECIAL_UNC | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_DOT_REFS,        RTPATH_STR_F_STYLE_DOS },
        { 3,  8,  8,   7,  6, "//.././f",         RTPATH_PROP_UNC | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_DOT_REFS,        RTPATH_STR_F_STYLE_DOS },
        { 3,  9,  9,   8,  7, "//../../f",        RTPATH_PROP_UNC | RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_RELATIVE | RTPATH_PROP_FILENAME | RTPATH_PROP_DOTDOT_REFS,     RTPATH_STR_F_STYLE_DOS },
        { 1,  1,  1,  -1,  1, "/",                RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE,                                                                        RTPATH_STR_F_STYLE_UNIX },
        { 2,  4,  4,   1,  1, "/bin",             RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 2,  5,  5,  -1,  4, "/bin/",            RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_DIR_SLASH,                                                RTPATH_STR_F_STYLE_UNIX },
        { 3,  7,  7,   5,  4, "/bin/ls",          RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 3,  12, 7,   5,  4, "/etc/rc.conf",     RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX,                            RTPATH_STR_F_STYLE_UNIX },
        { 1,  1,  2,  -1,  2, "//",               RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_EXTRA_SLASHES,                                            RTPATH_STR_F_STYLE_UNIX },
        { 1,  1,  3,  -1,  3, "///",              RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_EXTRA_SLASHES,                                            RTPATH_STR_F_STYLE_UNIX },
        { 3,  6,  7,   4,  2, "/.//bin",          RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_EXTRA_SLASHES | RTPATH_PROP_DOT_REFS | RTPATH_PROP_FILENAME, RTPATH_STR_F_STYLE_UNIX },
        { 1,  3,  3,   0,  0, "bin",              RTPATH_PROP_RELATIVE | RTPATH_PROP_FILENAME,                                                                          RTPATH_STR_F_STYLE_UNIX },
        { 1,  4,  4,  -1,  3, "bin/",             RTPATH_PROP_RELATIVE | RTPATH_PROP_DIR_SLASH,                                                                         RTPATH_STR_F_STYLE_UNIX },
        { 1,  4,  7,  -1,  3, "bin////",          RTPATH_PROP_RELATIVE | RTPATH_PROP_DIR_SLASH | RTPATH_PROP_EXTRA_SLASHES,                                             RTPATH_STR_F_STYLE_UNIX },
        { 3, 10, 10,   7,  6, "bin/../usr",       RTPATH_PROP_RELATIVE | RTPATH_PROP_DOTDOT_REFS | RTPATH_PROP_FILENAME,                                                RTPATH_STR_F_STYLE_UNIX },
        { 4, 11, 11,   8,  7, "/bin/../usr",      RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_RELATIVE | RTPATH_PROP_DOTDOT_REFS | RTPATH_PROP_FILENAME,                       RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   7,  6, "/a/.../u",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   7,  6, "/a/.b./u",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   7,  6, "/a/..c/u",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   7,  6, "/a/d../u",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   6,  5, "/a/.e/.u",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   6,  5, "/a/.f/.u",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 4, 11,  7,   6,  5, "/a/.f/u.ext",      RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME | RTPATH_PROP_SUFFIX,                            RTPATH_STR_F_STYLE_UNIX },
        { 4,  8,  8,   6,  5, "/a/.g/u.",         RTPATH_PROP_ROOT_SLASH | RTPATH_PROP_ABSOLUTE | RTPATH_PROP_FILENAME,                                                 RTPATH_STR_F_STYLE_UNIX },
        { 3,  9, 10,   5,  4, "/a/h/u.ext",       RTPATH_PROP_EXTRA_SLASHES | RTPATH_PROP_RELATIVE,                                                                     RTPATH_STR_F_STYLE_UNIX | RTPATH_STR_F_MIDDLE },
        { 3,  9,  9,   5,  3, "a/h/u.ext",        RTPATH_PROP_RELATIVE,                                                                                                 RTPATH_STR_F_STYLE_UNIX | RTPATH_STR_F_MIDDLE },
        { 3,  9, 10,  -1, 10, "a/h/u.ext/",       RTPATH_PROP_EXTRA_SLASHES | RTPATH_PROP_RELATIVE,                                                                     RTPATH_STR_F_STYLE_UNIX | RTPATH_STR_F_MIDDLE },
    };

    char szPath1[RTPATH_MAX];
    union
    {
        RTPATHPARSED    Parsed;
        RTPATHSPLIT     Split;
        uint8_t         ab[4096];
    } u;

    RTTestSub(hTest, "RTPathParse");
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        memset(&u, i & 1 ? 0xff : 0, sizeof(u));
        int rc = RTPathParse(s_aTests[i].pszPath, &u.Parsed, sizeof(u), s_aTests[i].fFlags);
        if (   rc != VINF_SUCCESS
            || s_aTests[i].cComps    != u.Parsed.cComps
            || s_aTests[i].fProps    != u.Parsed.fProps
            || s_aTests[i].offSuffix != u.Parsed.offSuffix
            || s_aTests[i].cchPath   != u.Parsed.cchPath)
        {
            RTTestFailed(hTest, "i=%d rc=%Rrc %s", i, rc, s_aTests[i].pszPath);
            RTTestFailureDetails(hTest,
                                 "  cComps    %u, got %u\n"
                                 "  fProps    %#x, got %#x, xor=>%#x\n"
                                 "  offSuffix %u, got %u\n"
                                 "  cchPath   %u, got %u\n"
                                 ,
                                 s_aTests[i].cComps,    u.Parsed.cComps,
                                 s_aTests[i].fProps,    u.Parsed.fProps, s_aTests[i].fProps ^ u.Parsed.fProps,
                                 s_aTests[i].offSuffix, u.Parsed.offSuffix,
                                 s_aTests[i].cchPath,   u.Parsed.cchPath);
        }
        else
        {
            rc = RTPathParsedReassemble(s_aTests[i].pszPath, &u.Parsed, s_aTests[i].fFlags & ~RTPATH_STR_F_MIDDLE,
                                        szPath1, sizeof(szPath1));
            if (rc == VINF_SUCCESS)
            {
                RTTESTI_CHECK_MSG(strlen(szPath1) == s_aTests[i].cchPath, ("%s\n", szPath1));
                if (   !(u.Parsed.fProps & RTPATH_PROP_EXTRA_SLASHES)
                    && (s_aTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) != RTPATH_STR_F_STYLE_DOS)
                    RTTESTI_CHECK_MSG(strcmp(szPath1, s_aTests[i].pszPath) == 0, ("%s\n", szPath1));
            }
            else
                RTTestIFailed("RTPathParsedReassemble -> %Rrc", rc);
        }
    }

    RTTestSub(hTest, "RTPathSplit");
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        memset(&u, i & 1 ? 0xff : 0, sizeof(u));
        int rc = RTPathSplit(s_aTests[i].pszPath, &u.Split, sizeof(u), s_aTests[i].fFlags);
        if (   rc != VINF_SUCCESS
            || s_aTests[i].cComps    != u.Split.cComps
            || s_aTests[i].fProps    != u.Split.fProps
            || s_aTests[i].cchPath   != u.Split.cchPath)
        {
            RTTestFailed(hTest, "i=%d rc=%Rrc %s", i, rc, s_aTests[i].pszPath);
            RTTestFailureDetails(hTest,
                                 "  cComps    %u, got %u\n"
                                 "  fProps    %#x, got %#x, xor=>%#x\n"
                                 "  cchPath   %u, got %u\n"
                                 ,
                                 s_aTests[i].cComps,    u.Split.cComps,
                                 s_aTests[i].fProps,    u.Split.fProps, s_aTests[i].fProps ^ u.Split.fProps,
                                 s_aTests[i].cchPath,   u.Split.cchPath);
        }
        else
        {
            RTTESTI_CHECK_MSG(*u.Split.pszSuffix == '\0' || *u.Split.pszSuffix == '.', ("%s", u.Split.pszSuffix));
            for (uint32_t idxComp = RTPATH_PROP_HAS_ROOT_SPEC(u.Split.fProps); idxComp < u.Split.cComps; idxComp++)
                if ( (s_aTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) == RTPATH_STR_F_STYLE_DOS
                    ? strpbrk(u.Split.apszComps[idxComp], "/\\")
                    : strchr(u.Split.apszComps[idxComp], RTPATH_SLASH) )
                    RTTestFailed(hTest, "i=%d idxComp=%d '%s'", i, idxComp, u.Split.apszComps[idxComp]);

            PRTPATHSPLIT pSplit = NULL;
            RTTESTI_CHECK_RC(rc = RTPathSplitA(s_aTests[i].pszPath, &pSplit, s_aTests[i].fFlags), VINF_SUCCESS);
            if (RT_SUCCESS(rc))
            {
                RTTESTI_CHECK(pSplit);
                RTTESTI_CHECK(pSplit->cComps   == u.Split.cComps);
                RTTESTI_CHECK(pSplit->fProps   == u.Split.fProps);
                RTTESTI_CHECK(pSplit->cchPath  == u.Split.cchPath);
                RTTESTI_CHECK(pSplit->cbNeeded == u.Split.cbNeeded);
                RTTESTI_CHECK(!strcmp(pSplit->pszSuffix, u.Split.pszSuffix));
                for (uint32_t idxComp = 0; idxComp < u.Split.cComps; idxComp++)
                    RTTESTI_CHECK(!strcmp(pSplit->apszComps[idxComp], u.Split.apszComps[idxComp]));
                RTPathSplitFree(pSplit);
            }

            rc = RTPathSplitReassemble(&u.Split, s_aTests[i].fFlags & ~RTPATH_STR_F_MIDDLE, szPath1, sizeof(szPath1));
            if (rc == VINF_SUCCESS)
            {
                RTTESTI_CHECK_MSG(strlen(szPath1) == s_aTests[i].cchPath, ("%s\n", szPath1));
                if (   !(u.Parsed.fProps & RTPATH_PROP_EXTRA_SLASHES)
                    && (s_aTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) != RTPATH_STR_F_STYLE_DOS)
                    RTTESTI_CHECK_MSG(strcmp(szPath1, s_aTests[i].pszPath) == 0, ("%s\n", szPath1));
            }
            else
                RTTestIFailed("RTPathSplitReassemble -> %Rrc", rc);
        }
    }

    RTTestSub(hTest, "RTPathParseSimple");
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        if (   (s_aTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) != RTPATH_STR_F_STYLE_HOST
            && (s_aTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) != RTPATH_STYLE)
            continue;
        if (s_aTests[i].fFlags & ~RTPATH_STR_F_STYLE_MASK)
            continue;

        size_t const cchPathIn = strlen(s_aTests[i].pszPath);
        size_t  cchDir  = ~(size_t)1;
        ssize_t offName = -97;
        ssize_t offSuff = -99;
        size_t  cchPath = RTPathParseSimple(s_aTests[i].pszPath, &cchDir, &offName, &offSuff);
        if (   cchPath != cchPathIn
            || offSuff != (s_aTests[i].offSuffix >= cchPathIn ? -1 : s_aTests[i].offSuffix)
            || offName != s_aTests[i].offName
            || cchDir  != s_aTests[i].cchDir)
        {
            RTTestFailed(hTest, "i=%u %s", i, s_aTests[i].pszPath);
            RTTestFailureDetails(hTest,
                                 "  cchPath   %zu, got %zu\n"
                                 "  cchDir    %u, got %zu\n"
                                 "  offName   %d, got %zd\n"
                                 "  offSuff   %d, got %zd\n"
                                 ,
                                 cchPathIn, cchPath,
                                 s_aTests[i].cchDir, cchDir,
                                 s_aTests[i].offName, offName,
                                 (s_aTests[i].offSuffix >= cchPathIn ? -1 : s_aTests[i].offSuffix), offSuff);
        }
    }
}


static void testParentLength(RTTEST hTest)
{
    static struct
    {
        const char *pszPath;
        uint32_t    cchNonParent;
        uint32_t    fFlags;
    } const s_aTests[] =
    {
        { "/usr/bin", 3,                 RTPATH_STR_F_STYLE_UNIX },
        { "/usr/bin", 3,                 RTPATH_STR_F_STYLE_DOS },
        { "\\usr\\bin", 3,               RTPATH_STR_F_STYLE_DOS },
        { "/usr/bin/", 4,                RTPATH_STR_F_STYLE_UNIX },
        { "/usr/bin/", 4,                RTPATH_STR_F_STYLE_DOS },
        { "\\usr\\bin\\", 4,             RTPATH_STR_F_STYLE_DOS },
        { "A:\\usr\\bin\\", 4,           RTPATH_STR_F_STYLE_DOS },
        { "/bin",  3,                    RTPATH_STR_F_STYLE_UNIX },
        { "/bin",  3,                    RTPATH_STR_F_STYLE_DOS },
        { "\\bin",  3,                   RTPATH_STR_F_STYLE_DOS },
        { "A:\\bin",  3,                 RTPATH_STR_F_STYLE_DOS },
        { "A:/bin",  3,                  RTPATH_STR_F_STYLE_DOS },
        { "A:bin",  3,                   RTPATH_STR_F_STYLE_DOS },
        { "/bin/", 4,                    RTPATH_STR_F_STYLE_UNIX },
        { "/bin/", 4,                    RTPATH_STR_F_STYLE_DOS },
        { "A:\\bin\\", 4,                RTPATH_STR_F_STYLE_DOS },
        { "A:/bin\\", 4,                 RTPATH_STR_F_STYLE_DOS },
        { "A:bin\\", 4,                  RTPATH_STR_F_STYLE_DOS },
        { "/", 0,                        RTPATH_STR_F_STYLE_UNIX },
        { "/", 0,                        RTPATH_STR_F_STYLE_DOS },
        { "\\", 0,                       RTPATH_STR_F_STYLE_DOS },
        { "A:\\", 0,                     RTPATH_STR_F_STYLE_DOS },
        { "A:", 0,                       RTPATH_STR_F_STYLE_DOS },
        { "bin", 3,                      RTPATH_STR_F_STYLE_UNIX },
        { "bin", 3,                      RTPATH_STR_F_STYLE_DOS },
        { "//unc/bin/bin", 3,            RTPATH_STR_F_STYLE_DOS },
        { "//unc/bin/bin/", 4,           RTPATH_STR_F_STYLE_DOS },
        { "//unc/bin", 3,                RTPATH_STR_F_STYLE_DOS },
        { "//unc/bin/", 4,               RTPATH_STR_F_STYLE_DOS },
        { "//unc/", 0,                   RTPATH_STR_F_STYLE_DOS },
        { "//unc", 0,                    RTPATH_STR_F_STYLE_DOS },
    };

    RTTestSub(hTest, "RTPathParentLength");
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        size_t const cchParent = RTPathParentLengthEx(s_aTests[i].pszPath, s_aTests[i].fFlags);
        size_t const cchExpected = strlen(s_aTests[i].pszPath) - s_aTests[i].cchNonParent;
        if (cchParent != cchExpected)
            RTTestFailed(hTest, "sub-test #%u: got %u, expected %u (%s)",
                         i, cchParent, cchExpected, s_aTests[i].pszPath);
        if (s_aTests[i].fFlags == RTPATH_STYLE)
        {
            size_t const cchParent2 = RTPathParentLength(s_aTests[i].pszPath);
            if (cchParent2 != cchExpected)
                RTTestFailed(hTest, "sub-test #%u: RTPathParentLength returned %u, expected %u (%s)",
                             i, cchParent2, cchExpected, s_aTests[i].pszPath);
        }
    }
}


static void testPurgeFilename(RTTEST hTest)
{
    static struct
    {
        const char *pszIn, *pszOut;
        uint32_t    fFlags;
    } const s_aTests[] =
    {
        { "start///end",            "start___end",      RTPATH_STR_F_STYLE_UNIX },
        { "start///end",            "start___end",      RTPATH_STR_F_STYLE_DOS },
        { "start///end",            "start___end",      RTPATH_STR_F_STYLE_HOST },
        { "1:<>\\9",                "1:<>\\9",          RTPATH_STR_F_STYLE_UNIX },
        { "1:<>\\9",                "1____9",           RTPATH_STR_F_STYLE_DOS },
        { "\t\r\n",                 "\t\r\n",           RTPATH_STR_F_STYLE_UNIX },
        { "\t\r\n",                 "___",              RTPATH_STR_F_STYLE_DOS },
    };
    RTTestSub(hTest, "RTPathPurgeFilename");
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        char szPath[RTPATH_MAX];
        strcpy(szPath, s_aTests[i].pszIn);
        char *pszRet = RTPathPurgeFilename(szPath, s_aTests[i].fFlags);
        RTTEST_CHECK(hTest, pszRet == &szPath[0]);
        if (strcmp(szPath, s_aTests[i].pszOut) != 0)
            RTTestFailed(hTest, "sub-test #%u: got '%s', expected '%s' (style %#x)",
                         i, szPath, s_aTests[i].pszOut, s_aTests[i].fFlags);
    }
}


static void testEnsureTrailingSeparator(RTTEST hTest)
{
    static struct
    {
        const char *pszIn, *pszOut;
        uint32_t    fFlags;
    } const s_aTests[] =
    {
        { "/foo",                   "/foo/",                RTPATH_STR_F_STYLE_UNIX },
        { "/foo\\",                 "/foo\\/",              RTPATH_STR_F_STYLE_UNIX },
        { "/foo:",                  "/foo:/",               RTPATH_STR_F_STYLE_UNIX },
        { "/foo/",                  "/foo/",                RTPATH_STR_F_STYLE_UNIX },
        { "D:/foo",                 "D:/foo\\",             RTPATH_STR_F_STYLE_DOS },
        { "D:/foo\\",               "D:/foo\\",             RTPATH_STR_F_STYLE_DOS },
        { "",                       "./",                   RTPATH_STR_F_STYLE_UNIX},
        { "",                       ".\\",                  RTPATH_STR_F_STYLE_DOS },
        { "",                       "." RTPATH_SLASH_STR,   RTPATH_STR_F_STYLE_HOST },
        { ".",                      "." RTPATH_SLASH_STR,   RTPATH_STR_F_STYLE_HOST },
        { "x",                      "x" RTPATH_SLASH_STR,   RTPATH_STR_F_STYLE_HOST },
        { "y" RTPATH_SLASH_STR,     "y" RTPATH_SLASH_STR,   RTPATH_STR_F_STYLE_HOST },
    };
    RTTestSub(hTest, "RTPathEnsureTrailingSeparatorEx");
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        char szPath[RTPATH_MAX];
        strcpy(szPath, s_aTests[i].pszIn);
        size_t cchRet = RTPathEnsureTrailingSeparatorEx(szPath, sizeof(szPath), s_aTests[i].fFlags);
        RTTEST_CHECK(hTest, cchRet == strlen(s_aTests[i].pszOut));
        if (strcmp(szPath, s_aTests[i].pszOut) != 0)
            RTTestFailed(hTest, "sub-test #%u: got '%s', expected '%s' (style %#x)",
                         i, szPath, s_aTests[i].pszOut, s_aTests[i].fFlags);
    }
}


int main()
{
    char szPath[RTPATH_MAX];

    /*
     * Init RT+Test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTPath", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    RTTestSub(hTest, "Environment");
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
    RTTESTI_CHECK(RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS);
# if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS
# else
    RTTestIFailed("#if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS");
# endif
    RTTESTI_CHECK(strcmp(RTPATH_SLASH_STR, "\\") == 0);
    RTTESTI_CHECK(RTPATH_SLASH == '\\');
    RTTESTI_CHECK(RTPATH_IS_SEP('/'));
    RTTESTI_CHECK(RTPATH_IS_SEP('\\'));
    RTTESTI_CHECK(RTPATH_IS_SEP(':'));

#else
    RTTESTI_CHECK(RTPATH_STYLE == RTPATH_STR_F_STYLE_UNIX);
# if RTPATH_STYLE == RTPATH_STR_F_STYLE_UNIX
# else
    RTTestIFailed("#if RTPATH_STYLE == RTPATH_STR_F_STYLE_UNIX");
# endif
    RTTESTI_CHECK(strcmp(RTPATH_SLASH_STR, "/") == 0);
    RTTESTI_CHECK(RTPATH_SLASH == '/');
    RTTESTI_CHECK(RTPATH_IS_SEP('/'));
    RTTESTI_CHECK(!RTPATH_IS_SEP('\\'));
    RTTESTI_CHECK(!RTPATH_IS_SEP(':'));
#endif

    /*
     * RTPathExecDir, RTPathUserHome and RTProcGetExecutablePath.
     */
    RTTestSub(hTest, "RTPathExecDir");
    RTTESTI_CHECK_RC(rc = RTPathExecDir(szPath, sizeof(szPath)), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        RTTestIPrintf(RTTESTLVL_INFO, "ExecDir={%s}\n", szPath);

    RTTestSub(hTest, "RTProcGetExecutablePath");
    if (RTProcGetExecutablePath(szPath, sizeof(szPath)) == szPath)
        RTTestIPrintf(RTTESTLVL_INFO, "ExecutableName={%s}\n", szPath);
    else
        RTTestIFailed("RTProcGetExecutablePath -> NULL");

    RTTestSub(hTest, "RTPathUserHome");
    RTTESTI_CHECK_RC(rc = RTPathUserHome(szPath, sizeof(szPath)), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        RTTestIPrintf(RTTESTLVL_INFO, "UserHome={%s}\n", szPath);

    RTTestSub(hTest, "RTPathUserDocuments");
    RTTESTI_CHECK_RC(rc = RTPathUserDocuments(szPath, sizeof(szPath)), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        RTTestIPrintf(RTTESTLVL_INFO, "UserDocuments={%s}\n", szPath);

    RTTestSub(hTest, "RTPathTemp");
    RTTESTI_CHECK_RC(rc = RTPathTemp(szPath, sizeof(szPath)), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        RTTestIPrintf(RTTESTLVL_INFO, "PathTemp={%s}\n", szPath);
    size_t cch = strlen(szPath);
    RTTESTI_CHECK_RC(RTPathTemp(szPath, cch), VERR_BUFFER_OVERFLOW);
    RTTESTI_CHECK_RC(RTPathTemp(szPath, cch+1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPathTemp(szPath, cch+2), VINF_SUCCESS);


    /*
     * RTPathAbsEx.
     */
    RTTestSub(hTest, "RTPathAbsEx");
    static const struct
    {
        uint32_t    fFlags;
        const char *pcszInputBase;
        const char *pcszInputPath;
        int rc;
        const char *pcszOutput;
    }
    s_aRTPathAbsExTests[] =
    {
        { RTPATH_STR_F_STYLE_HOST,  NULL, "", VERR_PATH_ZERO_LENGTH, NULL },
        { RTPATH_STR_F_STYLE_HOST,  NULL, ".", VINF_SUCCESS, "%p" },
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\", VINF_SUCCESS, "%d\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\..", VINF_SUCCESS, "%d\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "/absolute/..", VINF_SUCCESS, "%d\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "/absolute\\\\../..", VINF_SUCCESS, "%d\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "/absolute//../path\\", VINF_SUCCESS, "%d\\path\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "/absolute/../../path", VINF_SUCCESS, "%d\\path" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "relative/../dir\\.\\.\\.\\file.txt", VINF_SUCCESS, "%p\\dir\\file.txt" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\data\\", VINF_SUCCESS, "%d\\data\\" },
        { RTPATH_STR_F_STYLE_DOS,   "relative_base/dir\\", "\\from_root", VINF_SUCCESS, "%d\\from_root" },
        { RTPATH_STR_F_STYLE_DOS,   "relative_base/dir/", "relative_also", VINF_SUCCESS, "%p\\relative_base\\dir\\relative_also" },
#else
        { RTPATH_STR_F_STYLE_UNIX,  NULL, ".", VINF_SUCCESS, "%p" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "relative/../dir/./././file.txt", VINF_SUCCESS, "%p/dir/file.txt" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "relative/../dir\\.\\.\\.\\file.txt", VINF_SUCCESS, "%p/dir\\.\\.\\.\\file.txt" },  /* linux-specific */
        { RTPATH_STR_F_STYLE_UNIX,  "relative_base/dir/", "/from_root", VINF_SUCCESS, "/from_root" },
        { RTPATH_STR_F_STYLE_UNIX,  "relative_base/dir/", "relative_also", VINF_SUCCESS, "%p/relative_base/dir/relative_also" },
#endif
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/", VINF_SUCCESS, "/" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/..", VINF_SUCCESS, "/" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/absolute/..", VINF_SUCCESS, "/" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/absolute\\\\../..", VINF_SUCCESS, "/" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/absolute//../path/", VINF_SUCCESS, "/path/" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/absolute/../../path", VINF_SUCCESS, "/path" },
        { RTPATH_STR_F_STYLE_UNIX,  NULL, "/data/", VINF_SUCCESS, "/data/" },
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        { RTPATH_STR_F_STYLE_DOS,   NULL, "C:\\", VINF_SUCCESS, "C:\\" },
        { RTPATH_STR_F_STYLE_DOS,   "C:\\", "..", VINF_SUCCESS, "C:\\" },
        { RTPATH_STR_F_STYLE_DOS,   "C:\\temp", "..", VINF_SUCCESS, "C:\\" },
        { RTPATH_STR_F_STYLE_DOS,   "C:\\VirtualBox/Machines", "..\\VirtualBox.xml", VINF_SUCCESS, "C:\\VirtualBox\\VirtualBox.xml" },
        { RTPATH_STR_F_STYLE_DOS,   "C:\\MustDie", "\\from_root/dir/..", VINF_SUCCESS, "C:\\from_root" },
        { RTPATH_STR_F_STYLE_DOS,   "C:\\temp", "D:\\data", VINF_SUCCESS, "D:\\data" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\server\\..\\share", VINF_SUCCESS, "\\\\server\\..\\share" /* kind of strange */ },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\server/", VINF_SUCCESS, "\\\\server\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\", VINF_SUCCESS, "\\\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\\\something", VINF_SUCCESS, "\\\\\\something" /* kind of strange */ },
        { RTPATH_STR_F_STYLE_DOS,   "\\\\server\\share_as_base", "/from_root", VINF_SUCCESS, "\\\\server\\share_as_base\\from_root" },
        { RTPATH_STR_F_STYLE_DOS,   "\\\\just_server", "/from_root", VINF_SUCCESS, "\\\\just_server\\from_root" },
        { RTPATH_STR_F_STYLE_DOS,   "\\\\server\\share_as_base", "relative\\data", VINF_SUCCESS, "\\\\server\\share_as_base\\relative\\data" },
        { RTPATH_STR_F_STYLE_DOS,   "base", "\\\\?\\UNC\\relative/edwef/..", VINF_SUCCESS, "\\\\?\\UNC\\relative" },
        { RTPATH_STR_F_STYLE_DOS,   "\\\\?\\UNC\\base", "/from_root", VINF_SUCCESS, "\\\\?\\from_root" },
        { RTPATH_STR_F_STYLE_DOS,   "\\\\?\\UNC\\base", "./..", VINF_SUCCESS, "\\\\?\\UNC" },
        { RTPATH_STR_F_STYLE_DOS | RTPATHABS_F_STOP_AT_BASE, "\\\\?\\UNC\\base", "./..", VINF_SUCCESS, "\\\\?\\UNC\\base" },
        { RTPATH_STR_F_STYLE_DOS | RTPATHABS_F_STOP_AT_BASE, "\\\\?\\UNC\\base", "/..", VINF_SUCCESS, "\\\\?\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\.\\asdf\\..", VINF_SUCCESS, "\\\\.\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\?\\asdf\\..", VINF_SUCCESS, "\\\\?\\" },
        { RTPATH_STR_F_STYLE_DOS,   NULL, "\\\\x\\asdf\\..", VINF_SUCCESS, "\\\\x\\asdf" },
#else
        { RTPATH_STR_F_STYLE_UNIX,  "\\temp", "\\data", VINF_SUCCESS, "%p/\\temp/\\data" },
#endif
        { RTPATH_STR_F_STYLE_UNIX,  "/VirtualBox/Machines", "../VirtualBox.xml", VINF_SUCCESS, "/VirtualBox/VirtualBox.xml" },
        { RTPATH_STR_F_STYLE_UNIX,  "/MustDie", "/from_root/dir/..", VINF_SUCCESS, "/from_root" },
        { RTPATH_STR_F_STYLE_UNIX,  "/temp", "..", VINF_SUCCESS, "/" },
    };

    char *pszGuardedBuf = NULL;
    rc = RTTestGuardedAlloc(hTest, RTPATH_MAX, 0, false /*fHead*/, (void **)&pszGuardedBuf);
    if (RT_FAILURE(rc))
        pszGuardedBuf = szPath;

    for (unsigned i = 0; i < RT_ELEMENTS(s_aRTPathAbsExTests); ++ i)
    {
        if (RT_FAILURE(s_aRTPathAbsExTests[i].rc))
            RTTestDisableAssertions(hTest);

        size_t cbAbsPath = sizeof(szPath);
        rc = RTPathAbsEx(s_aRTPathAbsExTests[i].pcszInputBase,
                         s_aRTPathAbsExTests[i].pcszInputPath,
                         s_aRTPathAbsExTests[i].fFlags,
                         szPath, &cbAbsPath);

        if (RT_FAILURE(s_aRTPathAbsExTests[i].rc))
            RTTestRestoreAssertions(hTest);

        if (rc != s_aRTPathAbsExTests[i].rc)
        {
            RTTestIFailed("#%u: unexpected result code!\n"
                          "        flags: %#x\n"
                          "   input base: '%s'\n"
                          "   input path: '%s'\n"
                          "       output: '%s'\n"
                          "           rc: %Rrc\n"
                          "  expected rc: %Rrc",
                          i,
                          s_aRTPathAbsExTests[i].fFlags,
                          s_aRTPathAbsExTests[i].pcszInputBase,
                          s_aRTPathAbsExTests[i].pcszInputPath,
                          szPath, rc,
                          s_aRTPathAbsExTests[i].rc);
            continue;
        }

        char szTmp[RTPATH_MAX];
        char *pszExpected = NULL;
        if (s_aRTPathAbsExTests[i].pcszOutput != NULL)
        {
            if (s_aRTPathAbsExTests[i].pcszOutput[0] == '%')
            {
                RTTESTI_CHECK_RC(rc = RTPathGetCurrent(szTmp, sizeof(szTmp)), VINF_SUCCESS);
                if (RT_FAILURE(rc))
                    break;

                pszExpected = szTmp;

                if (s_aRTPathAbsExTests[i].pcszOutput[1] == 'p')
                {
                    cch = strlen(szTmp);
                    if (cch + strlen(s_aRTPathAbsExTests[i].pcszOutput) - 2 <= sizeof(szTmp))
                        strcpy(szTmp + cch, s_aRTPathAbsExTests[i].pcszOutput + 2);
                }
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
                else if (s_aRTPathAbsExTests[i].pcszOutput[1] == 'd')
                {
                    if (2 + strlen(s_aRTPathAbsExTests[i].pcszOutput) - 2 <= sizeof(szTmp))
                        strcpy(szTmp + 2, s_aRTPathAbsExTests[i].pcszOutput + 2);
                }
#endif
            }
            else
            {
                strcpy(szTmp, s_aRTPathAbsExTests[i].pcszOutput);
                pszExpected = szTmp;
            }

            if (   strcmp(szPath, pszExpected)
                || strlen(szPath) != cbAbsPath)
            {
                RTTestIFailed("#%u: Unexpected result\n"
                              "        flags: %#x\n"
                              "   input base: '%s'\n"
                              "   input path: '%s'\n"
                              "       output: '%s'\n"
                              "     expected: '%s' ('%s')\n"
                              "    cchResult: %#x, actual %#x",
                              i,
                              s_aRTPathAbsExTests[i].fFlags,
                              s_aRTPathAbsExTests[i].pcszInputBase,
                              s_aRTPathAbsExTests[i].pcszInputPath,
                              szPath,
                              pszExpected, s_aRTPathAbsExTests[i].pcszOutput,
                              cbAbsPath, strlen(szPath));
                continue;
            }

            if (RT_SUCCESS(s_aRTPathAbsExTests[i].rc))
            {
                /* Test the RTPATHABS_F_ENSURE_TRAILING_SLASH flag: */
                cbAbsPath = sizeof(szPath);
                rc = RTPathAbsEx(s_aRTPathAbsExTests[i].pcszInputBase,
                                 s_aRTPathAbsExTests[i].pcszInputPath,
                                 s_aRTPathAbsExTests[i].fFlags | RTPATHABS_F_ENSURE_TRAILING_SLASH,
                                 szPath, &cbAbsPath);
                char chSlash = (s_aRTPathAbsExTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) == RTPATH_STR_F_STYLE_DOS  ? '\\'
                             : (s_aRTPathAbsExTests[i].fFlags & RTPATH_STR_F_STYLE_MASK) == RTPATH_STR_F_STYLE_UNIX ? '/'
                             : RTPATH_SLASH;
                if (   RT_FAILURE(rc)
                    || strlen(szPath) != cbAbsPath
                    || szPath[cbAbsPath - 1] != chSlash)
                   RTTestIFailed("#%u: Unexpected RTPATHABS_F_ENSURE_TRAILING_SLASH result: %Rrc\n"
                                 "        flags: %#x | RTPATHABS_F_ENSURE_TRAILING_SLASH\n"
                                 "   input base: '%s'\n"
                                 "   input path: '%s'\n"
                                 "       output: '%s' ('%c' vs '%c')\n"
                                 "    cchResult: %#x, actual %#x",
                                 i, rc,
                                 s_aRTPathAbsExTests[i].fFlags,
                                 s_aRTPathAbsExTests[i].pcszInputBase,
                                 s_aRTPathAbsExTests[i].pcszInputPath,
                                 szPath, szPath[cbAbsPath - 1], chSlash,
                                 cbAbsPath, strlen(szPath));

                /* Do overflow testing: */
                size_t const cbNeeded = strlen(pszExpected) + 1;
                for (size_t cbBuf = 0; cbBuf < cbNeeded + 64; cbBuf++)
                {
                    char *pszBuf = &pszGuardedBuf[RTPATH_MAX - cbBuf];
                    memset(pszBuf, 0x33, cbBuf);
                    cbAbsPath = cbBuf;
                    rc = RTPathAbsEx(s_aRTPathAbsExTests[i].pcszInputBase, s_aRTPathAbsExTests[i].pcszInputPath,
                                     s_aRTPathAbsExTests[i].fFlags, pszBuf, &cbAbsPath);
                    if (   cbBuf < cbNeeded
                        && (   rc != VERR_BUFFER_OVERFLOW
                            || cbAbsPath < cbNeeded))
                        RTTestIFailed("#%u: Unexpected overflow result: %Rrc%s\n"
                                      "        flags: %#x\n"
                                      "   input base: '%s'\n"
                                      "   input path: '%s'\n"
                                      "    cbBuf[in]: %#x\n"
                                      "   cbBuf[out]: %#x\n"
                                      "     cbNeeded: %#x\n",
                                      i, rc, rc != VERR_BUFFER_OVERFLOW ? " - expected VERR_BUFFER_OVERFLOW" : "",
                                      s_aRTPathAbsExTests[i].fFlags,
                                      s_aRTPathAbsExTests[i].pcszInputBase,
                                      s_aRTPathAbsExTests[i].pcszInputPath,
                                      cbBuf,
                                      cbAbsPath,
                                      cbNeeded);
                    else if (   cbBuf >= cbNeeded
                             && (   rc != s_aRTPathAbsExTests[i].rc
                                 || cbAbsPath != cbNeeded - 1
                                 || strcmp(pszBuf, pszExpected)
                                 || strlen(pszBuf) != cbAbsPath))
                        RTTestIFailed("#%u: Unexpected result: %Rrc (expected %Rrc)\n"
                                      "        flags: %#x\n"
                                      "   input base: '%s'\n"
                                      "   input path: '%s'\n"
                                      "    cbBuf[in]: %#x\n"
                                      "   cbBuf[out]: %#x\n"
                                      "     cbNeeded: %#x\n",
                                      i, rc, s_aRTPathAbsExTests[i].rc,
                                      s_aRTPathAbsExTests[i].fFlags,
                                      s_aRTPathAbsExTests[i].pcszInputBase,
                                      s_aRTPathAbsExTests[i].pcszInputPath,
                                      cbBuf,
                                      cbAbsPath,
                                      cbNeeded);

                }
            }

            /* RTPathAbsExDup */
            char *pszDup = RTPathAbsExDup(s_aRTPathAbsExTests[i].pcszInputBase,
                                          s_aRTPathAbsExTests[i].pcszInputPath,
                                          s_aRTPathAbsExTests[i].fFlags);
            if (   (RT_SUCCESS(s_aRTPathAbsExTests[i].rc) ? pszDup == NULL : pszDup != NULL)
                || RTStrCmp(pszDup, pszExpected))
                RTTestIFailed("#%u: Unexpected RTPathAbsExDup result: %p%s\n"
                              "        flags: %#x\n"
                              "   input base: '%s'\n"
                              "   input path: '%s'\n"
                              "       output: '%s'\n"
                              "     expected: '%s' ('%s')\n",
                              i, pszDup,
                              (RT_SUCCESS(s_aRTPathAbsExTests[i].rc) ? pszDup == NULL : pszDup != NULL) ? pszDup ? "NULL" : "!NULL" : "",
                              s_aRTPathAbsExTests[i].fFlags,
                              s_aRTPathAbsExTests[i].pcszInputBase,
                              s_aRTPathAbsExTests[i].pcszInputPath,
                              pszDup,
                              pszExpected, s_aRTPathAbsExTests[i].pcszOutput);
            RTStrFree(pszDup);
        }
    }

    if (pszGuardedBuf != szPath)
        RTTestGuardedFree(hTest, pszGuardedBuf);


    /*
     * RTPathStripFilename
     */
    RTTestSub(hTest, "RTPathStripFilename");
    static const char *s_apszStripFilenameTests[] =
    {
        "/usr/include///",              "/usr/include//",
        "/usr/include/",                "/usr/include",
        "/usr/include",                 "/usr",
        "/usr",                         "/",
        "usr",                          ".",
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        "c:/windows",                   "c:/",
        "c:/",                          "c:/",
        "D:",                           "D:",
        "C:\\OS2\\DLLS",                "C:\\OS2",
#endif
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszStripFilenameTests); i += 2)
    {
        const char *pszInput  = s_apszStripFilenameTests[i];
        const char *pszExpect = s_apszStripFilenameTests[i + 1];
        strcpy(szPath, pszInput);
        RTPathStripFilename(szPath);
        if (strcmp(szPath, pszExpect))
        {
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, szPath, pszExpect);
        }
    }

    /*
     * RTPathAppend.
     */
    RTTestSub(hTest, "RTPathAppend");
    static const char *s_apszAppendTests[] =
    {
        /* base                 append                  result */
        "/",                    "",                     "/",
        "",                     "/",                    "/",
        "/",                    "/",                    "/",
        "/x",                   "",                     "/x",
        "/x",                   "/",                    "/x/",
        "/",                    "x",                    "/x",
        "dir",                  "file",                 "dir" RTPATH_SLASH_STR "file",
        "dir",                  "/file",                "dir/file",
        "dir",                  "//file",               "dir/file",
        "dir",                  "///file",              "dir/file",
        "dir/",                 "/file",                "dir/file",
        "dir/",                 "//file",               "dir/file",
        "dir/",                 "///file",              "dir/file",
        "dir//",                "file",                 "dir/file",
        "dir//",                "/file",                "dir/file",
        "dir//",                "//file",               "dir/file",
        "dir///",               "///file",              "dir/file",
        "/bin/testcase",        "foo.r0",               "/bin/testcase" RTPATH_SLASH_STR "foo.r0",
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        "/",                    "\\",                   "/",
        "\\",                   "/",                    "\\",
        "\\\\srv\\shr",         "dir//",                "\\\\srv\\shr" RTPATH_SLASH_STR "dir//",
        "\\\\srv\\shr",         "dir//file",            "\\\\srv\\shr" RTPATH_SLASH_STR "dir//file",
        "\\\\srv\\shr",         "//dir//",              "\\\\srv\\shr/dir//",
        "\\\\srv\\shr",         "/\\dir//",             "\\\\srv\\shr\\dir//",
        "\\\\",                 "not-srv/not-shr/file", "\\not-srv/not-shr/file",
        "C:",                   "autoexec.bat",         "C:autoexec.bat",
        "C:",                   "/autoexec.bat",        "C:/autoexec.bat",
        "C:",                   "\\autoexec.bat",       "C:\\autoexec.bat",
        "C:\\",                 "/autoexec.bat",        "C:\\autoexec.bat",
        "C:\\\\",               "autoexec.bat",         "C:\\autoexec.bat",
        "E:\\bin\\testcase",    "foo.r0",               "E:\\bin\\testcase" RTPATH_SLASH_STR "foo.r0",
#endif
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszAppendTests); i += 3)
    {
        const char *pszInput  = s_apszAppendTests[i];
        const char *pszAppend = s_apszAppendTests[i + 1];
        const char *pszExpect = s_apszAppendTests[i + 2];
        strcpy(szPath, pszInput);
        RTTESTI_CHECK_RC(rc = RTPathAppend(szPath, sizeof(szPath), pszAppend), VINF_SUCCESS);
        if (RT_FAILURE(rc))
            continue;
        if (strcmp(szPath, pszExpect))
        {
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s'\n"
                          "  append: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, pszAppend, szPath, pszExpect);
        }
        else
        {
            size_t const cchResult = strlen(szPath);

            strcpy(szPath, pszInput);
            RTTESTI_CHECK_RC(rc = RTPathAppend(szPath, cchResult + 2, pszAppend), VINF_SUCCESS);
            RTTESTI_CHECK(RT_FAILURE(rc) || !strcmp(szPath, pszExpect));

            strcpy(szPath, pszInput);
            RTTESTI_CHECK_RC(rc = RTPathAppend(szPath, cchResult + 1, pszAppend), VINF_SUCCESS);
            RTTESTI_CHECK(RT_FAILURE(rc) || !strcmp(szPath, pszExpect));

            if (strlen(pszInput) < cchResult)
            {
                strcpy(szPath, pszInput);
                RTTESTI_CHECK_RC(RTPathAppend(szPath, cchResult, pszAppend), VERR_BUFFER_OVERFLOW);
            }
        }
    }

    /*
     * RTPathJoin - reuse the append tests.
     */
    RTTestSub(hTest, "RTPathJoin");
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszAppendTests); i += 3)
    {
        const char *pszInput  = s_apszAppendTests[i];
        const char *pszAppend = s_apszAppendTests[i + 1];
        const char *pszExpect = s_apszAppendTests[i + 2];

        memset(szPath, 'a', sizeof(szPath)); szPath[sizeof(szPath) - 1] = '\0';

        RTTESTI_CHECK_RC(rc = RTPathJoin(szPath, sizeof(szPath), pszInput, pszAppend), VINF_SUCCESS);
        if (RT_FAILURE(rc))
            continue;
        if (strcmp(szPath, pszExpect))
        {
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s'\n"
                          "  append: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, pszAppend, szPath, pszExpect);
        }
        else
        {
            size_t const cchResult = strlen(szPath);

            memset(szPath, 'a', sizeof(szPath)); szPath[sizeof(szPath) - 1] = '\0';
            RTTESTI_CHECK_RC(rc = RTPathJoin(szPath, cchResult + 2, pszInput, pszAppend), VINF_SUCCESS);
            RTTESTI_CHECK(RT_FAILURE(rc) || !strcmp(szPath, pszExpect));

            memset(szPath, 'a', sizeof(szPath)); szPath[sizeof(szPath) - 1] = '\0';
            RTTESTI_CHECK_RC(rc = RTPathJoin(szPath, cchResult + 1, pszInput, pszAppend), VINF_SUCCESS);
            RTTESTI_CHECK(RT_FAILURE(rc) || !strcmp(szPath, pszExpect));

            RTTESTI_CHECK_RC(rc = RTPathJoin(szPath, cchResult, pszInput, pszAppend), VERR_BUFFER_OVERFLOW);
        }
    }

    /*
     * RTPathJoinA - reuse the append tests.
     */
    RTTestSub(hTest, "RTPathJoinA");
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszAppendTests); i += 3)
    {
        const char *pszInput  = s_apszAppendTests[i];
        const char *pszAppend = s_apszAppendTests[i + 1];
        const char *pszExpect = s_apszAppendTests[i + 2];

        char *pszPathDst;
        RTTESTI_CHECK(pszPathDst = RTPathJoinA(pszInput, pszAppend));
        if (!pszPathDst)
            continue;
        if (strcmp(pszPathDst, pszExpect))
        {
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s'\n"
                          "  append: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, pszAppend, pszPathDst, pszExpect);
        }
        RTStrFree(pszPathDst);
    }

    /*
     * RTPathStripTrailingSlash
     */
    static const char *s_apszStripTrailingSlash[] =
    {
     /* input                   result */
        "/",                    "/",
        "//",                   "/",
        "////////////////////", "/",
        "/tmp",                 "/tmp",
        "/tmp////////////////", "/tmp",
        "tmp",                  "tmp",
        "tmp////////////////",  "tmp",
        "./",                   ".",
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        "////////////////////", "/",
        "D:",                   "D:",
        "D:/",                  "D:/",
        "D:\\",                 "D:\\",
        "D:\\/\\",              "D:\\",
        "D:/\\/\\",             "D:/",
        "C:/Temp",              "C:/Temp",
        "C:/Temp/",             "C:/Temp",
        "C:/Temp\\/",           "C:/Temp",
#endif
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszStripTrailingSlash); i += 2)
    {
        const char *pszInput  = s_apszStripTrailingSlash[i];
        const char *pszExpect = s_apszStripTrailingSlash[i + 1];

        strcpy(szPath, pszInput);
        cch = RTPathStripTrailingSlash(szPath);
        if (strcmp(szPath, pszExpect))
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, szPath, pszExpect);
        else
            RTTESTI_CHECK(cch == strlen(szPath));
    }

    /*
     * RTPathCountComponents
     */
    RTTestSub(hTest, "RTPathCountComponents");
    RTTESTI_CHECK(RTPathCountComponents("") == 0);
    RTTESTI_CHECK(RTPathCountComponents("/") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//////////////") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//////////////bin") == 2);
    RTTESTI_CHECK(RTPathCountComponents("//////////////bin/") == 2);
    RTTESTI_CHECK(RTPathCountComponents("//////////////bin/////") == 2);
    RTTESTI_CHECK(RTPathCountComponents("..") == 1);
    RTTESTI_CHECK(RTPathCountComponents("../") == 1);
    RTTESTI_CHECK(RTPathCountComponents("../..") == 2);
    RTTESTI_CHECK(RTPathCountComponents("../../") == 2);
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
    RTTESTI_CHECK(RTPathCountComponents("d:") == 1);
    RTTESTI_CHECK(RTPathCountComponents("d:/") == 1);
    RTTESTI_CHECK(RTPathCountComponents("d:/\\") == 1);
    RTTESTI_CHECK(RTPathCountComponents("d:\\") == 1);
    RTTESTI_CHECK(RTPathCountComponents("c:\\config.sys") == 2);
    RTTESTI_CHECK(RTPathCountComponents("c:\\windows") == 2);
    RTTESTI_CHECK(RTPathCountComponents("c:\\windows\\") == 2);
    RTTESTI_CHECK(RTPathCountComponents("c:\\windows\\system32") == 3);
    RTTESTI_CHECK(RTPathCountComponents("//./C$") == 1);
    RTTESTI_CHECK(RTPathCountComponents("\\\\.\\C$") == 1);
    RTTESTI_CHECK(RTPathCountComponents("/\\.\\C$") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//myserver") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/share") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/share/") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/share\\") == 1);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/share\\x") == 2);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/share\\x\\y") == 3);
    RTTESTI_CHECK(RTPathCountComponents("//myserver/share\\x\\y\\") == 3);
#endif

    /*
     * RTPathCopyComponents
     */
    struct
    {
        const char *pszSrc;
        size_t      cComponents;
        const char *pszResult;
    } s_aCopyComponents[] =
    {
        { "",                           0, "" },
        { "",                           5, "" },
        { "/",                          0, "" },
        { "/",                          1, "/" },
        { "/",                          2, "/" },
        { "/usr/bin/sed",               0, "" },
        { "/usr/bin/sed",               1, "/" },
        { "/usr/bin/sed",               2, "/usr/" },
        { "/usr/bin/sed",               3, "/usr/bin/" },
        { "/usr/bin/sed",               4, "/usr/bin/sed" },
        { "/usr/bin/sed",               5, "/usr/bin/sed" },
        { "/usr/bin/sed",               6, "/usr/bin/sed" },
        { "/usr///bin/sed",             2, "/usr///" },
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_aCopyComponents); i++)
    {
        const char *pszInput    = s_aCopyComponents[i].pszSrc;
        size_t      cComponents = s_aCopyComponents[i].cComponents;
        const char *pszResult   = s_aCopyComponents[i].pszResult;

        memset(szPath, 'a', sizeof(szPath));
        rc = RTPathCopyComponents(szPath, sizeof(szPath), pszInput, cComponents);
        RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
        if (RT_SUCCESS(rc) && strcmp(szPath, pszResult))
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s' cComponents=%u\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, cComponents, szPath, pszResult);
        else if (RT_SUCCESS(rc))
        {
            RTTESTI_CHECK_RC(RTPathCopyComponents(szPath, strlen(pszResult) + 1, pszInput, cComponents), VINF_SUCCESS);
            RTTESTI_CHECK_RC(RTPathCopyComponents(szPath, strlen(pszResult), pszInput, cComponents), VERR_BUFFER_OVERFLOW);
        }
    }


    /*
     * RTPathStripSuffix
     */
    RTTestSub(hTest, "RTPathStripSuffix");
    struct
    {
        const char *pszSrc;
        const char *pszResult;
    } s_aStripExt[] =
    {
        { "filename.ext",               "filename" },
        { "filename.ext1.ext2.ext3",    "filename.ext1.ext2" },
        { "filename..ext",              "filename." },
        { "filename.ext.",              "filename.ext." },
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_aStripExt); i++)
    {
        const char *pszInput    = s_aStripExt[i].pszSrc;
        const char *pszResult   = s_aStripExt[i].pszResult;

        strcpy(szPath, pszInput);
        RTPathStripSuffix(szPath);
        if (strcmp(szPath, pszResult))
            RTTestIFailed("Unexpected result\n"
                          "   input: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszInput, szPath, pszResult);
    }

    /*
     * RTPathCalcRelative
     */
    RTTestSub(hTest, "RTPathCalcRelative");
    struct
    {
        const char *pszFrom;
        bool fFromFile;
        const char *pszTo;
        int rc;
        const char *pszExpected;
    } s_aRelPath[] =
    {
        { "/home/test.ext",     true,           "/home/test2.ext",        VINF_SUCCESS, "test2.ext" },
        { "/dir/test.ext",      true,           "/dir/dir2/test2.ext",    VINF_SUCCESS, "dir2/test2.ext" },
        { "/dir/dir2/test.ext", true,           "/dir/test2.ext",         VINF_SUCCESS, ".." RTPATH_SLASH_STR "test2.ext" },
        { "/dir/dir2/test.ext", true,           "/dir/dir3/test2.ext",    VINF_SUCCESS, ".." RTPATH_SLASH_STR "dir3/test2.ext" },
        { "/dir/dir2",          false,          "/dir/dir3/test2.ext",    VINF_SUCCESS, ".." RTPATH_SLASH_STR "dir3/test2.ext" },
        { "/dir/dir2",          false,          "/dir/dir3//test2.ext",   VINF_SUCCESS, ".." RTPATH_SLASH_STR "dir3//test2.ext" },
        { "/dir/dir2/",         false,          "/dir/dir3/test2.ext",    VINF_SUCCESS, ".." RTPATH_SLASH_STR "dir3/test2.ext" },
        { "/dir/dir2////",      false,          "/dir//dir3/test2.ext",   VINF_SUCCESS, ".." RTPATH_SLASH_STR "dir3/test2.ext" },
        { "/include/iprt",      false,          "/include/iprt/cdefs.h",  VINF_SUCCESS, "cdefs.h" },
        { "/include/iprt/",     false,          "/include/iprt/cdefs.h",  VINF_SUCCESS, "cdefs.h" },
        { "/include/iprt/tt.h", true,           "/include/iprt/cdefs.h",  VINF_SUCCESS, "cdefs.h" },
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
        { "\\\\server\\share\\test.ext", true,  "\\\\server\\share2\\test2.ext", VERR_NOT_SUPPORTED, "" },
        { "c:\\dir\\test.ext",  true,           "f:\\dir\\test.ext",      VERR_NOT_SUPPORTED, "" },
        { "F:\\dir\\test.ext",  false,          "f:/dir//test.ext",      VINF_SUCCESS, "." } ,
        { "F:\\diR\\Test.exT",  true,           "f:/dir//test.ext",      VINF_SUCCESS, "Test.exT" } ,
        { "F:\\K\xc3\x85RE\\Test.exT", true,    "f:/k\xc3\xa5re//test.ext", VINF_SUCCESS, "Test.exT" } ,
#endif
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_aRelPath); i++)
    {
        const char *pszFrom   = s_aRelPath[i].pszFrom;
        bool        fFromFile = s_aRelPath[i].fFromFile;
        const char *pszTo     = s_aRelPath[i].pszTo;

        rc = RTPathCalcRelative(szPath, sizeof(szPath), pszFrom, fFromFile, pszTo);
        if (rc != s_aRelPath[i].rc)
            RTTestIFailed("Unexpected return code for %s .. %s\n"
                          "     got: %Rrc\n"
                          "expected: %Rrc",
                          pszFrom, pszTo, rc, s_aRelPath[i].rc);
        else if (   RT_SUCCESS(rc)
                 && strcmp(szPath, s_aRelPath[i].pszExpected))
            RTTestIFailed("Unexpected result\n"
                          "    from: '%s' (%s)\n"
                          "      to: '%s'\n"
                          "  output: '%s'\n"
                          "expected: '%s'",
                          pszFrom, fFromFile ? "file" : "dir", pszTo, szPath, s_aRelPath[i].pszExpected);
    }

    testParserAndSplitter(hTest);
    testParentLength(hTest);
    testPurgeFilename(hTest);
    testEnsureTrailingSeparator(hTest);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

