/* $Id$ */
/** @file
 * IPRT Testcase - iprt::MiniString.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/cpp/ministring.h>

#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/uni.h>


static void test1Hlp1(const char *pszExpect, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    iprt::MiniString strTst(pszFormat, va);
    va_end(va);
    RTTESTI_CHECK_MSG(strTst.equals(pszExpect),  ("strTst='%s' expected='%s'\n",  strTst.c_str(), pszExpect));
}

static void test1(RTTEST hTest)
{
    RTTestSub(hTest, "Basics");

#define CHECK(expr) RTTESTI_CHECK(expr)
#define CHECK_DUMP(expr, value) \
    do { \
        if (!(expr)) \
            RTTestFailed(hTest, "%d: FAILED %s, got \"%s\"", __LINE__, #expr, value); \
    } while (0)

#define CHECK_DUMP_I(expr) \
    do { \
        if (!(expr)) \
            RTTestFailed(hTest, "%d: FAILED %s, got \"%d\"", __LINE__, #expr, expr); \
    } while (0)

    iprt::MiniString empty;
    CHECK(empty.length() == 0);
    CHECK(empty.capacity() == 0);

    iprt::MiniString sixbytes("12345");
    CHECK(sixbytes.length() == 5);
    CHECK(sixbytes.capacity() == 6);

    sixbytes.append(iprt::MiniString("678"));
    CHECK(sixbytes.length() == 8);
    CHECK(sixbytes.capacity() >= 9);

    sixbytes.append("9a");
    CHECK(sixbytes.length() == 10);
    CHECK(sixbytes.capacity() >= 11);

    char *psz = sixbytes.mutableRaw();
        // 123456789a
        //       ^
        // 0123456
    psz[6] = '\0';
    sixbytes.jolt();
    CHECK(sixbytes.length() == 6);
    CHECK(sixbytes.capacity() == 7);

    iprt::MiniString morebytes("tobereplaced");
    morebytes = "newstring ";
    morebytes.append(sixbytes);

    CHECK_DUMP(morebytes == "newstring 123456", morebytes.c_str());

    iprt::MiniString third(morebytes);
    third.reserve(100 * 1024);      // 100 KB
    CHECK_DUMP(third == "newstring 123456", morebytes.c_str() );
    CHECK(third.capacity() == 100 * 1024);
    CHECK(third.length() == morebytes.length());          // must not have changed

    iprt::MiniString copy1(morebytes);
    iprt::MiniString copy2 = morebytes;
    CHECK(copy1 == copy2);

    copy1 = NULL;
    CHECK(copy1.length() == 0);

    copy1 = "";
    CHECK(copy1.length() == 0);

    CHECK(iprt::MiniString("abc") <  iprt::MiniString("def"));
    CHECK(iprt::MiniString("abc") != iprt::MiniString("def"));
    CHECK_DUMP_I(iprt::MiniString("def") > iprt::MiniString("abc"));
    CHECK(iprt::MiniString("abc") == iprt::MiniString("abc"));

    CHECK(iprt::MiniString("abc") <  "def");
    CHECK(iprt::MiniString("abc") != "def");
    CHECK_DUMP_I(iprt::MiniString("def") > "abc");
    CHECK(iprt::MiniString("abc") == "abc");

    CHECK(iprt::MiniString("abc").equals("abc"));
    CHECK(!iprt::MiniString("abc").equals("def"));
    CHECK(iprt::MiniString("abc").equalsIgnoreCase("Abc"));
    CHECK(iprt::MiniString("abc").equalsIgnoreCase("ABc"));
    CHECK(iprt::MiniString("abc").equalsIgnoreCase("ABC"));
    CHECK(!iprt::MiniString("abc").equalsIgnoreCase("dBC"));
    CHECK(iprt::MiniString("").equals(""));
    CHECK(iprt::MiniString("").equalsIgnoreCase(""));

    copy2.setNull();
    for (int i = 0; i < 100; ++i)
    {
        copy2.reserve(50);      // should be ignored after 50 loops
        copy2.append("1");
    }
    CHECK(copy2.length() == 100);

    copy2.setNull();
    for (int i = 0; i < 100; ++i)
    {
        copy2.reserve(50);      // should be ignored after 50 loops
        copy2.append('1');
    }
    CHECK(copy2.length() == 100);

    /* printf */
    iprt::MiniString StrFmt;
    CHECK(StrFmt.printf("%s-%s-%d", "abc", "def", 42).equals("abc-def-42"));
    test1Hlp1("abc-42-def", "%s-%d-%s", "abc", 42, "def");
    test1Hlp1("", "");
    test1Hlp1("1", "1");
    test1Hlp1("foobar", "%s", "foobar");

#undef CHECK
#undef CHECK_DUMP
#undef CHECK_DUMP_I
}


static int mymemcmp(const char *psz1, const char *psz2, size_t cch)
{
    for (size_t off = 0; off < cch; off++)
        if (psz1[off] != psz2[off])
        {
            RTTestIFailed("off=%#x  psz1=%.*Rhxs  psz2=%.*Rhxs\n", off,
                          RT_MIN(cch - off, 8), &psz1[off],
                          RT_MIN(cch - off, 8), &psz2[off]);
            return psz1[off] > psz2[off] ? 1 : -1;
        }
    return 0;
}

static void test2(RTTEST hTest)
{
    RTTestSub(hTest, "UTF-8 upper/lower encoding assumption");

#define CHECK_EQUAL(str1, str2) \
    do \
    { \
        RTTESTI_CHECK(strlen((str1).c_str()) == (str1).length()); \
        RTTESTI_CHECK((str1).length() == (str2).length()); \
        RTTESTI_CHECK(mymemcmp((str1).c_str(), (str2).c_str(), (str2).length() + 1) == 0); \
    } while (0)

    iprt::MiniString strTmp;
    char szDst[16];

    /* Collect all upper and lower case code points. */
    iprt::MiniString strLower("");
    strLower.reserve(_4M);

    iprt::MiniString strUpper("");
    strUpper.reserve(_4M);

    for (RTUNICP uc = 1; uc <= 0x10fffd; uc++)
    {
        if (RTUniCpIsLower(uc))
        {
            RTTESTI_CHECK_MSG(uc < 0xd800 || (uc > 0xdfff && uc != 0xfffe && uc != 0xffff), ("%#x\n", uc));
            strLower.appendCodePoint(uc);
        }
        if (RTUniCpIsUpper(uc))
        {
            RTTESTI_CHECK_MSG(uc < 0xd800 || (uc > 0xdfff && uc != 0xfffe && uc != 0xffff), ("%#x\n", uc));
            strUpper.appendCodePoint(uc);
        }
    }
    RTTESTI_CHECK(strlen(strLower.c_str()) == strLower.length());
    RTTESTI_CHECK(strlen(strUpper.c_str()) == strUpper.length());

    /* Fold each code point in the lower case string and check that it encodes
       into the same or less number of bytes. */
    size_t      cch    = 0;
    const char *pszCur = strLower.c_str();
    iprt::MiniString    strUpper2("");
    strUpper2.reserve(strLower.length() + 64);
    for (;;)
    {
        RTUNICP             ucLower;
        const char * const  pszPrev   = pszCur;
        RTTESTI_CHECK_RC_BREAK(RTStrGetCpEx(&pszCur, &ucLower), VINF_SUCCESS);
        size_t const        cchSrc    = pszCur - pszPrev;
        if (!ucLower)
            break;

        RTUNICP const       ucUpper   = RTUniCpToUpper(ucLower);
        const char         *pszDstEnd = RTStrPutCp(szDst, ucUpper);
        size_t const        cchDst    = pszDstEnd - &szDst[0];
        RTTESTI_CHECK_MSG(cchSrc >= cchDst,
                          ("ucLower=%#x %u bytes;  ucUpper=%#x %u bytes\n",
                           ucLower, cchSrc, ucUpper, cchDst));
        cch += cchDst;
        strUpper2.appendCodePoint(ucUpper);

        /* roundtrip stability */
        RTUNICP const       ucUpper2  = RTUniCpToUpper(ucUpper);
        RTTESTI_CHECK_MSG(ucUpper2 == ucUpper, ("ucUpper2=%#x ucUpper=%#x\n", ucUpper2, ucUpper));

        RTUNICP const       ucLower2  = RTUniCpToLower(ucUpper);
        RTUNICP const       ucUpper3  = RTUniCpToUpper(ucLower2);
        RTTESTI_CHECK_MSG(ucUpper3 == ucUpper, ("ucUpper3=%#x ucUpper=%#x\n", ucUpper3, ucUpper));

        pszDstEnd = RTStrPutCp(szDst, ucLower2);
        size_t const        cchLower2 = pszDstEnd - &szDst[0];
        RTTESTI_CHECK_MSG(cchDst == cchLower2,
                          ("ucLower2=%#x %u bytes;  ucUpper=%#x %u bytes\n",
                           ucLower2, cchLower2, ucUpper, cchDst));
    }
    RTTESTI_CHECK(strlen(strUpper2.c_str()) == strUpper2.length());
    RTTESTI_CHECK_MSG(cch == strUpper2.length(), ("cch=%u length()=%u\n", cch, strUpper2.length()));

    /* the toUpper method shall do the same thing. */
    strTmp = strLower;      CHECK_EQUAL(strTmp, strLower);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);

    /* Ditto for the upper case string. */
    cch    = 0;
    pszCur = strUpper.c_str();
    iprt::MiniString    strLower2("");
    strLower2.reserve(strUpper.length() + 64);
    for (;;)
    {
        RTUNICP             ucUpper;
        const char * const  pszPrev   = pszCur;
        RTTESTI_CHECK_RC_BREAK(RTStrGetCpEx(&pszCur, &ucUpper), VINF_SUCCESS);
        size_t const        cchSrc    = pszCur - pszPrev;
        if (!ucUpper)
            break;

        RTUNICP const       ucLower   = RTUniCpToLower(ucUpper);
        const char         *pszDstEnd = RTStrPutCp(szDst, ucLower);
        size_t const        cchDst    = pszDstEnd - &szDst[0];
        RTTESTI_CHECK_MSG(cchSrc >= cchDst,
                          ("ucUpper=%#x %u bytes;  ucLower=%#x %u bytes\n",
                           ucUpper, cchSrc, ucLower, cchDst));

        cch += cchDst;
        strLower2.appendCodePoint(ucLower);

        /* roundtrip stability */
        RTUNICP const       ucLower2  = RTUniCpToLower(ucLower);
        RTTESTI_CHECK_MSG(ucLower2 == ucLower, ("ucLower2=%#x ucLower=%#x\n", ucLower2, ucLower));

        RTUNICP const       ucUpper2  = RTUniCpToUpper(ucLower);
        RTUNICP const       ucLower3  = RTUniCpToLower(ucUpper2);
        RTTESTI_CHECK_MSG(ucLower3 == ucLower, ("ucLower3=%#x ucLower=%#x\n", ucLower3, ucLower));

        pszDstEnd = RTStrPutCp(szDst, ucUpper2);
        size_t const        cchUpper2 = pszDstEnd - &szDst[0];
        RTTESTI_CHECK_MSG(cchDst == cchUpper2,
                          ("ucUpper2=%#x %u bytes;  ucLower=%#x %u bytes\n",
                           ucUpper2, cchUpper2, ucLower, cchDst));
    }
    RTTESTI_CHECK(strlen(strLower2.c_str()) == strLower2.length());
    RTTESTI_CHECK_MSG(cch == strLower2.length(), ("cch=%u length()=%u\n", cch, strLower2.length()));

    strTmp = strUpper;      CHECK_EQUAL(strTmp, strUpper);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);

    /* Checks of folding stability when nothing shall change. */
    strTmp = strUpper;      CHECK_EQUAL(strTmp, strUpper);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper);

    strTmp = strUpper2;     CHECK_EQUAL(strTmp, strUpper2);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);

    strTmp = strLower;      CHECK_EQUAL(strTmp, strLower);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower);

    strTmp = strLower2;     CHECK_EQUAL(strTmp, strLower2);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);

    /* Check folding stability for roundtrips. */
    strTmp = strUpper;      CHECK_EQUAL(strTmp, strUpper);
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);
    strTmp.toUpper();
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);
    strTmp.toUpper();
    strTmp.toLower();       CHECK_EQUAL(strTmp, strLower2);

    strTmp = strLower;      CHECK_EQUAL(strTmp, strLower);
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);
    strTmp.toLower();
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);
    strTmp.toLower();
    strTmp.toUpper();       CHECK_EQUAL(strTmp, strUpper2);
}


int main()
{
    RTTEST      hTest;
    RTEXITCODE  rcExit = RTTestInitAndCreate("tstIprtMiniString", &hTest);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        RTTestBanner(hTest);

        test1(hTest);
        test2(hTest);

        rcExit = RTTestSummaryAndDestroy(hTest);
    }
    return rcExit;
}

