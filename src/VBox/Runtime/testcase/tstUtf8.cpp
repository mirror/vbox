/* $Id$ */
/** @file
 * IPRT Testcase - UTF-8 and UTF-16 string conversions.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/uni.h>
#include <iprt/initterm.h>
#include <iprt/uuid.h>
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/test.h>

#include <stdlib.h>



/**
 * Generate a random codepoint for simple UTF-16 encoding.
 */
static RTUTF16 GetRandUtf16(void)
{
    RTUTF16 wc;
    do
    {
        wc = (RTUTF16)((long long)rand() * 0xffff / RAND_MAX);
    } while ((wc >= 0xd800 && wc <= 0xdfff) || wc == 0);
    return wc;
}


/**
 *
 */
static void test1(RTTEST hTest)
{
    static const char s_szBadString1[] = "Bad \xe0\x13\x0";
    static const char s_szBadString2[] = "Bad \xef\xbf\xc3";
    int rc;
    char *pszUtf8;
    char *pszCurrent;
    PRTUTF16 pwsz;
    PRTUTF16 pwszRand;

    /*
     * Invalid UTF-8 to UCS-2 test.
     */
    RTTestSub(hTest, "Feeding bad UTF-8 to RTStrToUtf16");
    rc = RTStrToUtf16(s_szBadString1, &pwsz);
    RTTEST_CHECK_MSG(hTest, rc == rc != VERR_NO_TRANSLATION || rc == VERR_INVALID_UTF8_ENCODING,
                     (hTest, RTTESTLVL_FAILURE, "Conversion of first bad UTF-8 string to UTF-16 apparantly succeeded. It shouldn't. rc=%Rrc\n", rc));
    rc = RTStrToUtf16(s_szBadString2, &pwsz);
    RTTEST_CHECK_MSG(hTest, rc == rc != VERR_NO_TRANSLATION || rc == VERR_INVALID_UTF8_ENCODING,
                     (hTest, RTTESTLVL_FAILURE, "Conversion of second bad UTF-8 strings to UTF-16 apparantly succeeded. It shouldn't. rc=%Rrc\n", rc));

    /*
     * Test current CP convertion.
     */
    RTTestSub(hTest, "Rand UTF-16 -> UTF-8 -> CP -> UTF-8");
    pwszRand = (PRTUTF16)RTMemAlloc(31 * sizeof(*pwsz));
    srand((unsigned)RTTimeNanoTS());
    for (int i = 0; i < 30; i++)
        pwszRand[i] = GetRandUtf16();
    pwszRand[30] = 0;

    rc = RTUtf16ToUtf8(pwszRand, &pszUtf8);
    if (rc == VINF_SUCCESS)
    {
        rc = RTStrUtf8ToCurrentCP(&pszCurrent, pszUtf8);
        if (rc == VINF_SUCCESS)
        {
            rc = RTStrCurrentCPToUtf8(&pszUtf8, pszCurrent);
            if (rc == VINF_SUCCESS)
                RTTestPassed(hTest, "Random UTF-16 -> UTF-8 -> Current -> UTF-8 successful.\n");
            else
                RTTestFailed(hTest, "%d: The third part of random UTF-16 -> UTF-8 -> Current -> UTF-8 failed with return value %Rrc.",
                             __LINE__, rc);
        }
        else if (rc == VERR_NO_TRANSLATION)
            RTTestPassed(hTest, "The second part of random UTF-16 -> UTF-8 -> Current -> UTF-8 returned VERR_NO_TRANSLATION.  This is probably as it should be.\n");
        else
            RTTestFailed(hTest, "%d: The second part of random UTF-16 -> UTF-8 -> Current -> UTF-8 failed with return value %Rrc.",
                         __LINE__, rc);
    }
    else
        RTTestFailed(hTest, "%d: The first part of random UTF-16 -> UTF-8 -> Current -> UTF-8 failed with return value %Rrc.",
                     __LINE__, rc);

    /*
     * Generate a new random string.
     */
    RTTestSub(hTest, "Random UTF-16 -> UTF-8 -> UTF-16");
    pwszRand = (PRTUTF16)RTMemAlloc(31 * sizeof(*pwsz));
    srand((unsigned)RTTimeNanoTS());
    for (int i = 0; i < 30; i++)
        pwszRand[i] = GetRandUtf16();
    pwszRand[30] = 0;
    rc = RTUtf16ToUtf8(pwszRand, &pszUtf8);
    if (rc == VINF_SUCCESS)
    {
        rc = RTStrToUtf16(pszUtf8, &pwsz);
        if (rc == VINF_SUCCESS)
        {
            int i;
            for (i = 0; pwszRand[i] == pwsz[i] && pwsz[i] != 0; i++)
                /* nothing */;
            if (pwszRand[i] == pwsz[i] && pwsz[i] == 0)
                RTTestPassed(hTest, "Random UTF-16 -> UTF-8 -> UTF-16 successful.\n");
            else
            {
                RTTestFailed(hTest, "%d: The second part of random UTF-16 -> UTF-8 -> UTF-16 failed.", __LINE__);
                RTTestPrintf(hTest, RTTESTLVL_FAILURE, "First differing character is at position %d and has the value %x.\n", i, pwsz[i]);
            }
        }
        else
            RTTestFailed(hTest, "%d: The second part of random UTF-16 -> UTF-8 -> UTF-16 failed with return value %Rrc.",
                         __LINE__, rc);
    }
    else
        RTTestFailed(hTest, "%d: The first part of random UTF-16 -> UTF-8 -> UTF-16 failed with return value %Rrc.",
                     __LINE__, rc);

    /*
     * Generate yet another random string and convert it to a buffer.
     */
    RTTestSub(hTest, "Random RTUtf16ToUtf8Ex + RTStrToUtf16");
    pwszRand = (PRTUTF16)RTMemAlloc(31 * sizeof(*pwsz));
    srand((unsigned)RTTimeNanoTS());
    for (int i = 0; i < 30; i++)
        pwszRand[i] = GetRandUtf16();
    pwszRand[30] = 0;

    char szUtf8Array[120];
    char *pszUtf8Array  = szUtf8Array;
    rc = RTUtf16ToUtf8Ex(pwszRand, RTSTR_MAX, &pszUtf8Array, 120, NULL);
    if (rc == 0)
    {
        rc = RTStrToUtf16(pszUtf8Array, &pwsz);
        if (rc == 0)
        {
            int i;
            for (i = 0; pwszRand[i] == pwsz[i] && pwsz[i] != 0; i++)
                ;
            if (pwsz[i] == 0 && i >= 8)
                RTTestPassed(hTest, "Random UTF-16 -> fixed length UTF-8 -> UTF-16 successful.\n");
            else
            {
                RTTestFailed(hTest, "%d: Incorrect conversion of UTF-16 -> fixed length UTF-8 -> UTF-16.\n", __LINE__);
                RTTestPrintf(hTest, RTTESTLVL_FAILURE, "First differing character is at position %d and has the value %x.\n", i, pwsz[i]);
            }
        }
        else
            RTTestFailed(hTest, "%d: The second part of random UTF-16 -> fixed length UTF-8 -> UTF-16 failed with return value %Rrc.\n", __LINE__, rc);
    }
    else
        RTTestFailed(hTest, "%d: The first part of random UTF-16 -> fixed length UTF-8 -> UTF-16 failed with return value %Rrc.\n", __LINE__, rc);

    /*
     * And again.
     */
    RTTestSub(hTest, "Random RTUtf16ToUtf8 + RTStrToUtf16Ex");
    pwszRand = (PRTUTF16)RTMemAlloc(31 * sizeof(*pwsz));
    srand((unsigned)RTTimeNanoTS());
    for (int i = 0; i < 30; i++)
        pwszRand[i] = GetRandUtf16();
    pwszRand[30] = 0;

    RTUTF16     wszBuf[70];
    PRTUTF16    pwsz2Buf = wszBuf;
    rc = RTUtf16ToUtf8(pwszRand, &pszUtf8);
    if (rc == 0)
    {
        rc = RTStrToUtf16Ex(pszUtf8, RTSTR_MAX, &pwsz2Buf, 70, NULL);
        if (rc == 0)
        {
            int i;
            for (i = 0; pwszRand[i] == pwsz2Buf[i] && pwsz2Buf[i] != 0; i++)
                ;
            if (pwszRand[i] == 0 && pwsz2Buf[i] == 0)
                RTTestPassed(hTest, "Random UTF-16 -> UTF-8 -> fixed length UTF-16 successful.\n");
            else
            {
                RTTestFailed(hTest, "%d: Incorrect conversion of random UTF-16 -> UTF-8 -> fixed length UTF-16.\n", __LINE__);
                RTTestPrintf(hTest, RTTESTLVL_FAILURE, "First differing character is at position %d and has the value %x.\n", i, pwsz2Buf[i]);
            }
        }
        else
            RTTestFailed(hTest, "%d: The second part of random UTF-16 -> UTF-8 -> fixed length UTF-16 failed with return value %Rrc.\n", __LINE__, rc);
    }
    else
        RTTestFailed(hTest, "%d: The first part of random UTF-16 -> UTF-8 -> fixed length UTF-16 failed with return value %Rrc.\n",
                     __LINE__, rc);
    pwszRand = (PRTUTF16)RTMemAlloc(31 * sizeof(*pwsz));
    srand((unsigned)RTTimeNanoTS());
    for (int i = 0; i < 30; i++)
        pwszRand[i] = GetRandUtf16();
    pwszRand[30] = 0;

    rc = RTUtf16ToUtf8Ex(pwszRand, RTSTR_MAX, &pszUtf8Array, 20, NULL);
    if (rc == VERR_BUFFER_OVERFLOW)
        RTTestPassed(hTest, "Random UTF-16 -> fixed length UTF-8 with too short buffer successfully rejected.\n");
    else
        RTTestFailed(hTest, "%d: Random UTF-16 -> fixed length UTF-8 with too small buffer returned value %d instead of VERR_BUFFER_OVERFLOW.\n",
                     __LINE__, rc);

    /*
     * last time...
     */
    RTTestSub(hTest, "Random RTUtf16ToUtf8 + RTStrToUtf16Ex");
    pwszRand = (PRTUTF16)RTMemAlloc(31 * sizeof(*pwsz));
    srand((unsigned)RTTimeNanoTS());
    for (int i = 0; i < 30; i++)
        pwszRand[i] = GetRandUtf16();
    pwszRand[30] = 0;

    rc = RTUtf16ToUtf8(pwszRand, &pszUtf8);
    if (rc == VINF_SUCCESS)
    {
        rc = RTStrToUtf16Ex(pszUtf8, RTSTR_MAX, &pwsz2Buf, 20, NULL);
        if (rc == VERR_BUFFER_OVERFLOW)
            RTTestPassed(hTest, "Random UTF-16 -> UTF-8 -> fixed length UTF-16 with too short buffer successfully rejected.\n");
        else
            RTTestFailed(hTest, "%d: The second part of random UTF-16 -> UTF-8 -> fixed length UTF-16 with too short buffer returned value %Rrc instead of VERR_BUFFER_OVERFLOW.\n",
                         __LINE__, rc);
    }
    else
        RTTestFailed(hTest, "%d:The first part of random UTF-16 -> UTF-8 -> fixed length UTF-16 failed with return value %Rrc.\n",
                     __LINE__, rc);


    RTTestSubDone(hTest);
}


static RTUNICP g_uszAll[0x110000 - 1 - 0x800 - 2 + 1];
static RTUTF16 g_wszAll[0xfffe - (0xe000 - 0xd800) + (0x110000 - 0x10000) * 2];
static char     g_szAll[0x7f + (0x800 - 0x80) * 2 + (0xfffe - 0x800 - (0xe000 - 0xd800))* 3 + (0x110000 - 0x10000) * 4 + 1];

static void whereami(int cBits, size_t off)
{
    if (cBits == 8)
    {
        if (off < 0x7f)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-8 U+%#x\n", off + 1);
        else if (off < 0xf7f)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-8 U+%#x\n", (off - 0x7f) / 2 + 0x80);
        else if (off < 0x27f7f)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-8 U+%#x\n", (off - 0xf7f) / 3 + 0x800);
        else if (off < 0x2df79)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-8 U+%#x\n", (off - 0x27f7f) / 3 + 0xe000);
        else if (off < 0x42df79)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-8 U+%#x\n", (off - 0x2df79) / 4 + 0x10000);
        else
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-8 ???\n");
    }
    else if (cBits == 16)
    {
        if (off < 0xd7ff*2)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-16 U+%#x\n", off / 2 + 1);
        else if (off < 0xf7fd*2)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-16 U+%#x\n", (off - 0xd7ff*2) / 2 + 0xe000);
        else if (off < 0x20f7fd)
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-16 U+%#x\n", (off - 0xf7fd*2) / 4 + 0x10000);
        else
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "UTF-16 ???\n");
    }
    else
    {
        if (off < (0xd800 - 1) * sizeof(RTUNICP))
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "RTUNICP U+%#x\n", off / sizeof(RTUNICP) + 1);
        else if (off < (0xfffe - 0x800 - 1) * sizeof(RTUNICP))
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "RTUNICP U+%#x\n", off / sizeof(RTUNICP) + 0x800 + 1);
        else
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "RTUNICP U+%#x\n", off / sizeof(RTUNICP) + 0x800 + 1 + 2);
    }
}

int mymemcmp(const void *pv1, const void *pv2, size_t cb, int cBits)
{
    const uint8_t  *pb1 = (const uint8_t *)pv1;
    const uint8_t  *pb2 = (const uint8_t *)pv2;
    for (size_t off = 0; off < cb; off++)
    {
        if (pb1[off] != pb2[off])
        {
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "mismatch at %#x: ", off);
            whereami(cBits, off);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off-1, pb1[off-1], pb2[off-1]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, "*%#x: %02x != %02x!\n", off, pb1[off], pb2[off]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+1, pb1[off+1], pb2[off+1]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+2, pb1[off+2], pb2[off+2]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+3, pb1[off+3], pb2[off+3]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+4, pb1[off+4], pb2[off+4]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+5, pb1[off+5], pb2[off+5]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+6, pb1[off+6], pb2[off+6]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+7, pb1[off+7], pb2[off+7]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+8, pb1[off+8], pb2[off+8]);
            RTTestPrintf(NIL_RTTEST, RTTESTLVL_FAILURE, " %#x: %02x != %02x!\n", off+9, pb1[off+9], pb2[off+9]);
            return 1;
        }
    }
    return 0;
}


void InitStrings()
{
    /*
     * Generate unicode string containing all the legal UTF-16 codepoints, both UTF-16 and UTF-8 version.
     */
    /* the simple code point array first */
    unsigned i = 0;
    RTUNICP  uc = 1;
    while (uc < 0xd800)
        g_uszAll[i++] = uc++;
    uc = 0xe000;
    while (uc < 0xfffe)
        g_uszAll[i++] = uc++;
    uc = 0x10000;
    while (uc < 0x110000)
        g_uszAll[i++] = uc++;
    g_uszAll[i++] = 0;
    Assert(RT_ELEMENTS(g_uszAll) == i);

    /* the utf-16 one */
    i = 0;
    uc = 1;
    //RTPrintf("tstUtf8: %#x=%#x", i, uc);
    while (uc < 0xd800)
        g_wszAll[i++] = uc++;
    uc = 0xe000;
    //RTPrintf(" %#x=%#x", i, uc);
    while (uc < 0xfffe)
        g_wszAll[i++] = uc++;
    uc = 0x10000;
    //RTPrintf(" %#x=%#x", i, uc);
    while (uc < 0x110000)
    {
        g_wszAll[i++] = 0xd800 | ((uc - 0x10000) >> 10);
        g_wszAll[i++] = 0xdc00 | ((uc - 0x10000) & 0x3ff);
        uc++;
    }
    //RTPrintf(" %#x=%#x\n", i, uc);
    g_wszAll[i++] = '\0';
    Assert(RT_ELEMENTS(g_wszAll) == i);

    /*
     * The utf-8 one
     */
    i = 0;
    uc = 1;
    //RTPrintf("tstUtf8: %#x=%#x", i, uc);
    while (uc < 0x80)
        g_szAll[i++] = uc++;
    //RTPrintf(" %#x=%#x", i, uc);
    while (uc < 0x800)
    {
        g_szAll[i++] = 0xc0 | (uc >> 6);
        g_szAll[i++] = 0x80 | (uc & 0x3f);
        Assert(!((uc >> 6) & ~0x1f));
        uc++;
    }
    //RTPrintf(" %#x=%#x", i, uc);
    while (uc < 0xd800)
    {
        g_szAll[i++] = 0xe0 |  (uc >> 12);
        g_szAll[i++] = 0x80 | ((uc >>  6) & 0x3f);
        g_szAll[i++] = 0x80 |  (uc & 0x3f);
        Assert(!((uc >> 12) & ~0xf));
        uc++;
    }
    uc = 0xe000;
    //RTPrintf(" %#x=%#x", i, uc);
    while (uc < 0xfffe)
    {
        g_szAll[i++] = 0xe0 |  (uc >> 12);
        g_szAll[i++] = 0x80 | ((uc >>  6) & 0x3f);
        g_szAll[i++] = 0x80 |  (uc & 0x3f);
        Assert(!((uc >> 12) & ~0xf));
        uc++;
    }
    uc = 0x10000;
    //RTPrintf(" %#x=%#x", i, uc);
    while (uc < 0x110000)
    {
        g_szAll[i++] = 0xf0 |  (uc >> 18);
        g_szAll[i++] = 0x80 | ((uc >> 12) & 0x3f);
        g_szAll[i++] = 0x80 | ((uc >>  6) & 0x3f);
        g_szAll[i++] = 0x80 |  (uc & 0x3f);
        Assert(!((uc >> 18) & ~0x7));
        uc++;
    }
    //RTPrintf(" %#x=%#x\n", i, uc);
    g_szAll[i++] = '\0';
    Assert(RT_ELEMENTS(g_szAll) == i);
}


void test2(RTTEST hTest)
{
    /*
     * Convert to UTF-8 and back.
     */
    RTTestSub(hTest, "UTF-16 -> UTF-8 -> UTF-16");
    char *pszUtf8;
    int rc = RTUtf16ToUtf8(&g_wszAll[0], &pszUtf8);
    if (rc == VINF_SUCCESS)
    {
        if (mymemcmp(pszUtf8, g_szAll, sizeof(g_szAll), 8))
            RTTestFailed(hTest, "UTF-16 -> UTF-8 mismatch!");

        PRTUTF16 pwszUtf16;
        rc = RTStrToUtf16(pszUtf8, &pwszUtf16);
        if (rc == VINF_SUCCESS)
        {
            if (mymemcmp(pwszUtf16, g_wszAll, sizeof(g_wszAll), 16))
                RTTestFailed(hTest, "UTF-8 -> UTF-16 failed compare!");
            RTUtf16Free(pwszUtf16);
        }
        else
            RTTestFailed(hTest, "UTF-8 -> UTF-16 failed, rc=%Rrc.", rc);
        RTStrFree(pszUtf8);
    }
    else
        RTTestFailed(hTest, "UTF-16 -> UTF-8 failed, rc=%Rrc.", rc);


    /*
     * Convert to UTF-16 and back. (just in case the above test fails)
     */
    RTTestSub(hTest, "UTF-8 -> UTF-16 -> UTF-8");
    PRTUTF16 pwszUtf16;
    rc = RTStrToUtf16(&g_szAll[0], &pwszUtf16);
    if (rc == VINF_SUCCESS)
    {
        if (mymemcmp(pwszUtf16, g_wszAll, sizeof(g_wszAll), 16))
            RTTestFailed(hTest, "UTF-8 -> UTF-16 failed compare!");

        char *pszUtf8;
        rc = RTUtf16ToUtf8(pwszUtf16, &pszUtf8);
        if (rc == VINF_SUCCESS)
        {
            if (mymemcmp(pszUtf8, g_szAll, sizeof(g_szAll), 8))
                RTTestFailed(hTest, "UTF-16 -> UTF-8 failed compare!");
            RTStrFree(pszUtf8);
        }
        else
            RTTestFailed(hTest, "UTF-16 -> UTF-8 failed, rc=%Rrc.", rc);
        RTUtf16Free(pwszUtf16);
    }
    else
        RTTestFailed(hTest, "UTF-8 -> UTF-16 failed, rc=%Rrc.", rc);

    /*
     * Convert UTF-8 to CPs.
     */
    RTTestSub(hTest, "UTF-8 -> UNI -> UTF-8");
    PRTUNICP paCps;
    rc = RTStrToUni(g_szAll, &paCps);
    if (rc == VINF_SUCCESS)
    {
        if (mymemcmp(paCps, g_uszAll, sizeof(g_uszAll), 32))
            RTTestFailed(hTest, "UTF-8 -> UTF-16 failed, rc=%Rrc.", rc);

        size_t cCps;
        rc = RTStrToUniEx(g_szAll, RTSTR_MAX, &paCps, RT_ELEMENTS(g_uszAll), &cCps);
        if (rc == VINF_SUCCESS)
        {
            if (cCps != RT_ELEMENTS(g_uszAll) - 1)
                RTTestFailed(hTest, "wrong Code Point count %zu, expected %zu\n", cCps, RT_ELEMENTS(g_uszAll) - 1);
        }
        else
            RTTestFailed(hTest, "UTF-8 -> Code Points failed, rc=%Rrc.\n", rc);

        /** @todo RTCpsToUtf8 or something. */
    }
    else
        RTTestFailed(hTest, "UTF-8 -> Code Points failed, rc=%Rrc.\n", rc);

    /*
     * Check the various string lengths.
     */
    RTTestSub(hTest, "Lengths");
    size_t cuc1 = RTStrCalcUtf16Len(g_szAll);
    size_t cuc2 = RTUtf16Len(g_wszAll);
    if (cuc1 != cuc2)
        RTTestFailed(hTest, "cuc1=%zu != cuc2=%zu\n", cuc1, cuc2);
    //size_t cuc3 = RTUniLen(g_uszAll);


    /*
     * Enumerate the strings.
     */
    RTTestSub(hTest, "Code Point Getters and Putters");
    char *pszPut1Base = (char *)RTMemAlloc(sizeof(g_szAll));
    AssertRelease(pszPut1Base);
    char *pszPut1 = pszPut1Base;
    PRTUTF16 pwszPut2Base = (PRTUTF16)RTMemAlloc(sizeof(g_wszAll));
    AssertRelease(pwszPut2Base);
    PRTUTF16 pwszPut2 = pwszPut2Base;
    const char *psz1 = g_szAll;
    const char *psz2 = g_szAll;
    PCRTUTF16   pwsz3 = g_wszAll;
    PCRTUTF16   pwsz4 = g_wszAll;
    for (;;)
    {
        /*
         * getters
         */
        RTUNICP uc1;
        rc = RTStrGetCpEx(&psz1, &uc1);
        if (RT_FAILURE(rc))
        {
            RTTestFailed(hTest, "RTStrGetCpEx failed with rc=%Rrc at %.10Rhxs", rc, psz2);
            whereami(8, psz2 - &g_szAll[0]);
            break;
        }
        char *pszPrev1 = RTStrPrevCp(g_szAll, psz1);
        if (pszPrev1 != psz2)
        {
            RTTestFailed(hTest, "RTStrPrevCp returned %p expected %p!", pszPrev1, psz2);
            whereami(8, psz2 - &g_szAll[0]);
            break;
        }
        RTUNICP uc2 = RTStrGetCp(psz2);
        if (uc2 != uc1)
        {
            RTTestFailed(hTest, "RTStrGetCpEx and RTStrGetCp returned different CPs: %RTunicp != %RTunicp", uc2, uc1);
            whereami(8, psz2 - &g_szAll[0]);
            break;
        }
        psz2 = RTStrNextCp(psz2);
        if (psz2 != psz1)
        {
            RTTestFailed(hTest, "RTStrGetCpEx and RTStrGetNext returned different next pointer!");
            whereami(8, psz2 - &g_szAll[0]);
            break;
        }

        RTUNICP uc3;
        rc = RTUtf16GetCpEx(&pwsz3, &uc3);
        if (RT_FAILURE(rc))
        {
            RTTestFailed(hTest, "RTUtf16GetCpEx failed with rc=%Rrc at %.10Rhxs", rc, pwsz4);
            whereami(16, pwsz4 - &g_wszAll[0]);
            break;
        }
        if (uc3 != uc2)
        {
            RTTestFailed(hTest, "RTUtf16GetCpEx and RTStrGetCp returned different CPs: %RTunicp != %RTunicp", uc3, uc2);
            whereami(16, pwsz4 - &g_wszAll[0]);
            break;
        }
        RTUNICP uc4 = RTUtf16GetCp(pwsz4);
        if (uc3 != uc4)
        {
            RTTestFailed(hTest, "RTUtf16GetCpEx and RTUtf16GetCp returned different CPs: %RTunicp != %RTunicp", uc3, uc4);
            whereami(16, pwsz4 - &g_wszAll[0]);
            break;
        }
        pwsz4 = RTUtf16NextCp(pwsz4);
        if (pwsz4 != pwsz3)
        {
            RTTestFailed(hTest, "RTUtf16GetCpEx and RTUtf16GetNext returned different next pointer!");
            whereami(8, pwsz4 - &g_wszAll[0]);
            break;
        }


        /*
         * putters
         */
        pszPut1 = RTStrPutCp(pszPut1, uc1);
        if (pszPut1 - pszPut1Base != psz1 - &g_szAll[0])
        {
            RTTestFailed(hTest, "RTStrPutCp is not at the same offset! %p != %p",
                         pszPut1 - pszPut1Base, psz1 - &g_szAll[0]);
            whereami(8, psz2 - &g_szAll[0]);
            break;
        }

        pwszPut2 = RTUtf16PutCp(pwszPut2, uc3);
        if (pwszPut2 - pwszPut2Base != pwsz3 - &g_wszAll[0])
        {
            RTTestFailed(hTest, "RTStrPutCp is not at the same offset! %p != %p",
                         pwszPut2 - pwszPut2Base, pwsz3 - &g_wszAll[0]);
            whereami(8, pwsz4 - &g_wszAll[0]);
            break;
        }


        /* the end? */
        if (!uc1)
            break;
    }

    /* check output if we seems to have made it thru it all. */
    if (psz2 == &g_szAll[sizeof(g_szAll)])
    {
        if (mymemcmp(pszPut1Base, g_szAll, sizeof(g_szAll), 8))
            RTTestFailed(hTest, "RTStrPutCp encoded the string incorrectly.");
        if (mymemcmp(pwszPut2Base, g_wszAll, sizeof(g_wszAll), 16))
            RTTestFailed(hTest, "RTUtf16PutCp encoded the string incorrectly.");
    }

    RTMemFree(pszPut1Base);
    RTMemFree(pwszPut2Base);

    RTTestSubDone(hTest);
}


/**
 * Check case insensitivity.
 */
void test3(RTTEST hTest)
{
    RTTestSub(hTest, "Case Sensitivitity");

    if (    RTUniCpToLower('a') != 'a'
        ||  RTUniCpToLower('A') != 'a'
        ||  RTUniCpToLower('b') != 'b'
        ||  RTUniCpToLower('B') != 'b'
        ||  RTUniCpToLower('Z') != 'z'
        ||  RTUniCpToLower('z') != 'z'
        ||  RTUniCpToUpper('c') != 'C'
        ||  RTUniCpToUpper('C') != 'C'
        ||  RTUniCpToUpper('z') != 'Z'
        ||  RTUniCpToUpper('Z') != 'Z')
        RTTestFailed(hTest, "RTUniToUpper/Lower failed basic tests.\n");

    if (RTUtf16ICmp(g_wszAll, g_wszAll))
        RTTestFailed(hTest, "RTUtf16ICmp failed the basic test.\n");

    if (RTUtf16Cmp(g_wszAll, g_wszAll))
        RTTestFailed(hTest, "RTUtf16Cmp failed the basic test.\n");

    static RTUTF16 s_wszTst1a[] = { 'a', 'B', 'c', 'D', 'E', 'f', 'g', 'h', 'i', 'j', 'K', 'L', 'm', 'N', 'o', 'P', 'q', 'r', 'S', 't', 'u', 'V', 'w', 'x', 'Y', 'Z', 0xc5, 0xc6, 0xf8, 0 };
    static RTUTF16 s_wszTst1b[] = { 'A', 'B', 'c', 'd', 'e', 'F', 'G', 'h', 'i', 'J', 'k', 'l', 'M', 'n', 'O', 'p', 'Q', 'R', 's', 't', 'U', 'v', 'w', 'X', 'y', 'z', 0xe5, 0xe6, 0xd8, 0 };
    if (    RTUtf16ICmp(s_wszTst1b, s_wszTst1b)
        ||  RTUtf16ICmp(s_wszTst1a, s_wszTst1a)
        ||  RTUtf16ICmp(s_wszTst1a, s_wszTst1b)
        ||  RTUtf16ICmp(s_wszTst1b, s_wszTst1a)
        )
        RTTestFailed(hTest, "RTUtf16ICmp failed the alphabet test.\n");

    if (    RTUtf16Cmp(s_wszTst1b, s_wszTst1b)
        ||  RTUtf16Cmp(s_wszTst1a, s_wszTst1a)
        ||  !RTUtf16Cmp(s_wszTst1a, s_wszTst1b)
        ||  !RTUtf16Cmp(s_wszTst1b, s_wszTst1a)
        )
        RTTestFailed(hTest, "RTUtf16Cmp failed the alphabet test.\n");

    RTTestSubDone(hTest);
}


/**
 * Test the RTStr*Cmp functions.
 */
void TstRTStrXCmp(RTTEST hTest)
{
#define CHECK_DIFF(expr, op) \
    do \
    { \
        int iDiff = expr; \
        if (!(iDiff op 0)) \
            RTTestFailed(hTest, "%d: %d " #op " 0: %s\n", __LINE__, iDiff, #expr); \
    } while (0)

/** @todo test the non-ascii bits. */

    RTTestSub(hTest, "RTStrCmp");
    CHECK_DIFF(RTStrCmp(NULL, NULL), == );
    CHECK_DIFF(RTStrCmp(NULL, ""), < );
    CHECK_DIFF(RTStrCmp("", NULL), > );
    CHECK_DIFF(RTStrCmp("", ""), == );
    CHECK_DIFF(RTStrCmp("abcdef", "abcdef"), == );
    CHECK_DIFF(RTStrCmp("abcdef", "abcde"), > );
    CHECK_DIFF(RTStrCmp("abcde", "abcdef"), < );
    CHECK_DIFF(RTStrCmp("abcdeg", "abcdef"), > );
    CHECK_DIFF(RTStrCmp("abcdef", "abcdeg"), < );
    CHECK_DIFF(RTStrCmp("abcdeF", "abcdef"), < );
    CHECK_DIFF(RTStrCmp("abcdef", "abcdeF"), > );


    RTTestSub(hTest, "RTStrNCmp");
    CHECK_DIFF(RTStrNCmp(NULL, NULL, RTSTR_MAX), == );
    CHECK_DIFF(RTStrNCmp(NULL, "", RTSTR_MAX), < );
    CHECK_DIFF(RTStrNCmp("", NULL, RTSTR_MAX), > );
    CHECK_DIFF(RTStrNCmp("", "", RTSTR_MAX), == );
    CHECK_DIFF(RTStrNCmp("abcdef", "abcdef", RTSTR_MAX), == );
    CHECK_DIFF(RTStrNCmp("abcdef", "abcde", RTSTR_MAX), > );
    CHECK_DIFF(RTStrNCmp("abcde", "abcdef", RTSTR_MAX), < );
    CHECK_DIFF(RTStrNCmp("abcdeg", "abcdef", RTSTR_MAX), > );
    CHECK_DIFF(RTStrNCmp("abcdef", "abcdeg", RTSTR_MAX), < );
    CHECK_DIFF(RTStrNCmp("abcdeF", "abcdef", RTSTR_MAX), < );
    CHECK_DIFF(RTStrNCmp("abcdef", "abcdeF", RTSTR_MAX), > );

    CHECK_DIFF(RTStrNCmp("abcdef", "fedcba", 0), ==);
    CHECK_DIFF(RTStrNCmp("abcdef", "abcdeF", 5), ==);
    CHECK_DIFF(RTStrNCmp("abcdef", "abcdeF", 6), > );


    RTTestSub(hTest, "RTStrICmp");
    CHECK_DIFF(RTStrICmp(NULL, NULL), == );
    CHECK_DIFF(RTStrICmp(NULL, ""), < );
    CHECK_DIFF(RTStrICmp("", NULL), > );
    CHECK_DIFF(RTStrICmp("", ""), == );
    CHECK_DIFF(RTStrICmp("abcdef", "abcdef"), == );
    CHECK_DIFF(RTStrICmp("abcdef", "abcde"), > );
    CHECK_DIFF(RTStrICmp("abcde", "abcdef"), < );
    CHECK_DIFF(RTStrICmp("abcdeg", "abcdef"), > );
    CHECK_DIFF(RTStrICmp("abcdef", "abcdeg"), < );

    CHECK_DIFF(RTStrICmp("abcdeF", "abcdef"), ==);
    CHECK_DIFF(RTStrICmp("abcdef", "abcdeF"), ==);
    CHECK_DIFF(RTStrICmp("ABCDEF", "abcdef"), ==);
    CHECK_DIFF(RTStrICmp("abcdef", "ABCDEF"), ==);
    CHECK_DIFF(RTStrICmp("AbCdEf", "aBcDeF"), ==);
    CHECK_DIFF(RTStrICmp("AbCdEg", "aBcDeF"), > );
    CHECK_DIFF(RTStrICmp("AbCdEG", "aBcDef"), > ); /* diff performed on the lower case cp. */



    RTTestSub(hTest, "RTStrNICmp");
    CHECK_DIFF(RTStrNICmp(NULL, NULL, RTSTR_MAX), == );
    CHECK_DIFF(RTStrNICmp(NULL, "", RTSTR_MAX), < );
    CHECK_DIFF(RTStrNICmp("", NULL, RTSTR_MAX), > );
    CHECK_DIFF(RTStrNICmp("", "", RTSTR_MAX), == );
    CHECK_DIFF(RTStrNICmp(NULL, NULL, 0), == );
    CHECK_DIFF(RTStrNICmp(NULL, "", 0), == );
    CHECK_DIFF(RTStrNICmp("", NULL, 0), == );
    CHECK_DIFF(RTStrNICmp("", "", 0), == );
    CHECK_DIFF(RTStrNICmp("abcdef", "abcdef", RTSTR_MAX), == );
    CHECK_DIFF(RTStrNICmp("abcdef", "abcde", RTSTR_MAX), > );
    CHECK_DIFF(RTStrNICmp("abcde", "abcdef", RTSTR_MAX), < );
    CHECK_DIFF(RTStrNICmp("abcdeg", "abcdef", RTSTR_MAX), > );
    CHECK_DIFF(RTStrNICmp("abcdef", "abcdeg", RTSTR_MAX), < );

    CHECK_DIFF(RTStrNICmp("abcdeF", "abcdef", RTSTR_MAX), ==);
    CHECK_DIFF(RTStrNICmp("abcdef", "abcdeF", RTSTR_MAX), ==);
    CHECK_DIFF(RTStrNICmp("ABCDEF", "abcdef", RTSTR_MAX), ==);
    CHECK_DIFF(RTStrNICmp("abcdef", "ABCDEF", RTSTR_MAX), ==);
    CHECK_DIFF(RTStrNICmp("AbCdEf", "aBcDeF", RTSTR_MAX), ==);
    CHECK_DIFF(RTStrNICmp("AbCdEg", "aBcDeF", RTSTR_MAX), > );
    CHECK_DIFF(RTStrNICmp("AbCdEG", "aBcDef", RTSTR_MAX), > ); /* diff performed on the lower case cp. */

    CHECK_DIFF(RTStrNICmp("ABCDEF", "fedcba", 0), ==);
    CHECK_DIFF(RTStrNICmp("AbCdEg", "aBcDeF", 5), ==);
    CHECK_DIFF(RTStrNICmp("AbCdEf", "aBcDeF", 5), ==);
    CHECK_DIFF(RTStrNICmp("AbCdE",  "aBcDe", 5), ==);
    CHECK_DIFF(RTStrNICmp("AbCdE",  "aBcDeF", 5), ==);
    CHECK_DIFF(RTStrNICmp("AbCdEf", "aBcDe", 5), ==);
    CHECK_DIFF(RTStrNICmp("AbCdEg", "aBcDeF", 6), > );
    CHECK_DIFF(RTStrNICmp("AbCdEG", "aBcDef", 6), > ); /* diff performed on the lower case cp. */
    /* We should continue using byte comparison when we hit the invalid CP.  Will assert in debug builds. */
    // CHECK_DIFF(RTStrNICmp("AbCd\xff""eg", "aBcD\xff""eF", 6), ==);

    RTTestSubDone(hTest);
}



/**
 * Benchmark stuff.
 */
void Benchmarks(RTTEST hTest)
{
    static union
    {
        RTUTF16 wszBuf[sizeof(g_wszAll)];
        char szBuf[sizeof(g_szAll)];
    } s_Buf;

    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Benchmarking RTStrToUtf16Ex:  "); /** @todo figure this stuff into the test framework. */
    PRTUTF16 pwsz = &s_Buf.wszBuf[0];
    int rc = RTStrToUtf16Ex(&g_szAll[0], RTSTR_MAX, &pwsz, RT_ELEMENTS(s_Buf.wszBuf), NULL);
    if (RT_SUCCESS(rc))
    {
        int i;
        uint64_t u64Start = RTTimeNanoTS();
        for (i = 0; i < 100; i++)
        {
            rc = RTStrToUtf16Ex(&g_szAll[0], RTSTR_MAX, &pwsz, RT_ELEMENTS(s_Buf.wszBuf), NULL);
            if (RT_FAILURE(rc))
            {
                RTTestFailed(hTest, "UTF-8 -> UTF-16 benchmark failed at i=%d, rc=%Rrc\n", i, rc);
                break;
            }
        }
        uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "%d in %RI64ns\n", i, u64Elapsed);
    }

    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Benchmarking RTUtf16ToUtf8Ex: ");
    char *psz = &s_Buf.szBuf[0];
    rc = RTUtf16ToUtf8Ex(&g_wszAll[0], RTSTR_MAX, &psz, RT_ELEMENTS(s_Buf.szBuf), NULL);
    if (RT_SUCCESS(rc))
    {
        int i;
        uint64_t u64Start = RTTimeNanoTS();
        for (i = 0; i < 100; i++)
        {
            rc = RTUtf16ToUtf8Ex(&g_wszAll[0], RTSTR_MAX, &psz, RT_ELEMENTS(s_Buf.szBuf), NULL);
            if (RT_FAILURE(rc))
            {
                RTTestFailed(hTest, "UTF-16 -> UTF-8 benchmark failed at i=%d, rc=%Rrc\n", i, rc);
                break;
            }
        }
        uint64_t u64Elapsed = RTTimeNanoTS() - u64Start;
        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "%d in %RI64ns\n", i, u64Elapsed);
    }

}


/**
 * Tests RTStrStr and RTStrIStr.
 */
static void testStrStr(RTTEST hTest)
{
#define CHECK_NULL(expr) \
    do { \
        const char *pszRet = expr; \
        if (pszRet != NULL) \
            RTTestFailed(hTest, "%d: %#x -> %s expected NULL", __LINE__, #expr, pszRet); \
    } while (0)

#define CHECK(expr, expect) \
    do { \
        const char *pszRet = expr; \
        if (    pszRet != expect \
            &&  (   (expect) == NULL  \
                 || pszRet == NULL \
                 || strcmp(pszRet, (expect)) ) \
            ) \
            RTTestFailed(hTest, "%d: %#x -> %s expected %s", __LINE__, #expr, pszRet, (expect)); \
    } while (0)


    RTTestSub(hTest, "RTStrStr");
    CHECK(RTStrStr("abcdef", ""), "abcdef");
    CHECK_NULL(RTStrStr("abcdef", NULL));
    CHECK_NULL(RTStrStr(NULL, ""));
    CHECK_NULL(RTStrStr(NULL, NULL));
    CHECK(RTStrStr("abcdef", "abcdef"), "abcdef");
    CHECK(RTStrStr("abcdef", "b"), "bcdef");
    CHECK(RTStrStr("abcdef", "bcdef"), "bcdef");
    CHECK(RTStrStr("abcdef", "cdef"), "cdef");
    CHECK(RTStrStr("abcdef", "cde"), "cdef");
    CHECK(RTStrStr("abcdef", "cd"), "cdef");
    CHECK(RTStrStr("abcdef", "c"), "cdef");
    CHECK(RTStrStr("abcdef", "f"), "f");
    CHECK(RTStrStr("abcdef", "ef"), "ef");
    CHECK(RTStrStr("abcdef", "e"), "ef");
    CHECK_NULL(RTStrStr("abcdef", "z"));
    CHECK_NULL(RTStrStr("abcdef", "A"));
    CHECK_NULL(RTStrStr("abcdef", "F"));

    RTTestSub(hTest, "RTStrIStr");
    CHECK(RTStrIStr("abcdef", ""), "abcdef");
    CHECK_NULL(RTStrIStr("abcdef", NULL));
    CHECK_NULL(RTStrIStr(NULL, ""));
    CHECK_NULL(RTStrIStr(NULL, NULL));
    CHECK(RTStrIStr("abcdef", "abcdef"), "abcdef");
    CHECK(RTStrIStr("abcdef", "Abcdef"), "abcdef");
    CHECK(RTStrIStr("abcdef", "ABcDeF"), "abcdef");
    CHECK(RTStrIStr("abcdef", "b"), "bcdef");
    CHECK(RTStrIStr("abcdef", "B"), "bcdef");
    CHECK(RTStrIStr("abcdef", "bcdef"), "bcdef");
    CHECK(RTStrIStr("abcdef", "BCdEf"), "bcdef");
    CHECK(RTStrIStr("abcdef", "bCdEf"), "bcdef");
    CHECK(RTStrIStr("abcdef", "bcdEf"), "bcdef");
    CHECK(RTStrIStr("abcdef", "BcdEf"), "bcdef");
    CHECK(RTStrIStr("abcdef", "cdef"), "cdef");
    CHECK(RTStrIStr("abcdef", "cde"), "cdef");
    CHECK(RTStrIStr("abcdef", "cd"), "cdef");
    CHECK(RTStrIStr("abcdef", "c"), "cdef");
    CHECK(RTStrIStr("abcdef", "f"), "f");
    CHECK(RTStrIStr("abcdeF", "F"), "F");
    CHECK(RTStrIStr("abcdef", "F"), "f");
    CHECK(RTStrIStr("abcdef", "ef"), "ef");
    CHECK(RTStrIStr("EeEef", "e"), "EeEef");
    CHECK(RTStrIStr("EeEef", "E"), "EeEef");
    CHECK(RTStrIStr("EeEef", "EE"), "EeEef");
    CHECK(RTStrIStr("EeEef", "EEE"), "EeEef");
    CHECK(RTStrIStr("EeEef", "EEEF"), "eEef");
    CHECK_NULL(RTStrIStr("EeEef", "z"));

#undef CHECK
#undef CHECK_NULL
    RTTestSubDone(hTest);
}


int main()
{
    /*
     * Init the runtime and stuff.
     */
    RTTEST hTest;
    if (    RT_FAILURE(RTR3Init())
        ||  RT_FAILURE(RTTestCreate("tstUtf8", &hTest)))
    {
        RTPrintf("tstBitstUtf8: fatal initialization error\n");
        return 1;
    }
    RTTestBanner(hTest);

    InitStrings();
    test1(hTest);
    test2(hTest);
    test3(hTest);
    TstRTStrXCmp(hTest);
    testStrStr(hTest);
    Benchmarks(hTest);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}
