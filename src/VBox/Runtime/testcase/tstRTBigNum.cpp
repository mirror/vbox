/* $Id$ */
/** @file
 * IPRT - Testcase for the RTBigNum* functions.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <iprt/bignum.h>

#include <iprt/test.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTTEST g_hTest;


static uint8_t const g_abLargePositive[] =
{
    0x67,0xcd,0xd6,0x60,0x4e,0xaa,0xe9,0x8e,0x06,0x99,0xde,0xb2,0xf5,0x1c,0xc3,0xfc,
    0xf5,0x17,0x41,0xec,0x42,0x68,0xf0,0xab,0x0e,0xe6,0x79,0xa8,0x32,0x97,0x55,0x00,
    0x49,0x21,0x2b,0x72,0x4b,0x34,0x33,0xe1,0xe2,0xfe,0xa2,0xb8,0x39,0x7a,0x2f,0x17,
    0xae,0x1f,0xbb,0xdb,0x46,0xbc,0x59,0x8b,0x13,0x05,0x28,0x96,0xf6,0xfd,0xc1,0xa4
};
static RTBIGNUM g_LargePositive;
static RTBIGNUM g_LargePositive2; /**< Smaller than g_LargePositive.  */

static uint8_t const g_abLargePositiveMinus1[] =
{
    0x67,0xcd,0xd6,0x60,0x4e,0xaa,0xe9,0x8e,0x06,0x99,0xde,0xb2,0xf5,0x1c,0xc3,0xfc,
    0xf5,0x17,0x41,0xec,0x42,0x68,0xf0,0xab,0x0e,0xe6,0x79,0xa8,0x32,0x97,0x55,0x00,
    0x49,0x21,0x2b,0x72,0x4b,0x34,0x33,0xe1,0xe2,0xfe,0xa2,0xb8,0x39,0x7a,0x2f,0x17,
    0xae,0x1f,0xbb,0xdb,0x46,0xbc,0x59,0x8b,0x13,0x05,0x28,0x96,0xf6,0xfd,0xc1,0xa3
};
static RTBIGNUM g_LargePositiveMinus1; /**< g_LargePositive - 1 */


static uint8_t const g_abLargeNegative[] =
{
    0xf2,0xde,0xbd,0xaf,0x43,0x9e,0x1e,0x88,0xdc,0x64,0x37,0xa9,0xdb,0xb7,0x26,0x31,
    0x92,0x1d,0xf5,0x43,0x4c,0xb0,0x21,0x2b,0x07,0x4e,0xf5,0x94,0x9e,0xce,0x15,0x79,
    0x13,0x0c,0x70,0x68,0x49,0x46,0xcf,0x72,0x2b,0xc5,0x8f,0xab,0x7c,0x88,0x2d,0x1e,
    0x3b,0x43,0x5b,0xdb,0x47,0x45,0x7a,0x25,0x74,0x46,0x1d,0x87,0x24,0xaa,0xab,0x0d,
    0x3e,0xdf,0xd1,0xd8,0x44,0x6f,0x01,0x84,0x01,0x36,0xe0,0x84,0x6e,0x6f,0x41,0xbb,
    0xae,0x1a,0x31,0xef,0x42,0x23,0xfd,0xda,0xda,0x0f,0x7d,0x88,0x8f,0xf5,0x63,0x72,
    0x36,0x9f,0xa9,0xa4,0x4f,0xa0,0xa6,0xb1,0x3b,0xbe,0x0d,0x9d,0x62,0x88,0x98,0x8b
};
static RTBIGNUM g_LargeNegative;
static RTBIGNUM g_LargeNegative2; /**< A few digits less than g_LargeNegative, i.e. larger value.  */

static uint8_t const g_abLargeNegativePluss1[] =
{
    0xf2,0xde,0xbd,0xaf,0x43,0x9e,0x1e,0x88,0xdc,0x64,0x37,0xa9,0xdb,0xb7,0x26,0x31,
    0x92,0x1d,0xf5,0x43,0x4c,0xb0,0x21,0x2b,0x07,0x4e,0xf5,0x94,0x9e,0xce,0x15,0x79,
    0x13,0x0c,0x70,0x68,0x49,0x46,0xcf,0x72,0x2b,0xc5,0x8f,0xab,0x7c,0x88,0x2d,0x1e,
    0x3b,0x43,0x5b,0xdb,0x47,0x45,0x7a,0x25,0x74,0x46,0x1d,0x87,0x24,0xaa,0xab,0x0d,
    0x3e,0xdf,0xd1,0xd8,0x44,0x6f,0x01,0x84,0x01,0x36,0xe0,0x84,0x6e,0x6f,0x41,0xbb,
    0xae,0x1a,0x31,0xef,0x42,0x23,0xfd,0xda,0xda,0x0f,0x7d,0x88,0x8f,0xf5,0x63,0x72,
    0x36,0x9f,0xa9,0xa4,0x4f,0xa0,0xa6,0xb1,0x3b,0xbe,0x0d,0x9d,0x62,0x88,0x98,0x8c
};
static RTBIGNUM g_LargeNegativePluss1; /**< g_LargeNegative + 1 */


static uint8_t const g_ab64BitPositive1[] = { 0x53, 0xe0, 0xdf, 0x11,  0x85, 0x93, 0x06, 0x21 };
static uint64_t g_u64BitPositive1 = UINT64_C(0x53e0df1185930621);
static RTBIGNUM g_64BitPositive1;


static RTBIGNUM g_Zero;
static RTBIGNUM g_One;
static RTBIGNUM g_Two;
static RTBIGNUM g_Three;
static RTBIGNUM g_Four;
static RTBIGNUM g_Five;
static RTBIGNUM g_Ten;
static RTBIGNUM g_FourtyTwo;

static uint8_t const g_abMinus1[] = { 0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff };
static int64_t  g_iBitMinus1 = -1;
static RTBIGNUM g_Minus1;



static void testInitOneLittleEndian(uint8_t const *pb, size_t cb, PRTBIGNUM pBigNum)
{
    uint8_t abLittleEndian[sizeof(g_abLargePositive) + sizeof(g_abLargeNegative)];
    RTTESTI_CHECK_RETV(cb <= sizeof(abLittleEndian));

    size_t         cbLeft = cb;
    uint8_t       *pbDst  = abLittleEndian + cb - 1;
    uint8_t const *pbSrc  = pb;
    while (cbLeft-- > 0)
        *pbDst-- = *pbSrc++;

    RTBIGNUM Num;
    RTTESTI_CHECK_RC_RETV(RTBigNumInit(&Num, RTBIGNUMINIT_F_ENDIAN_LITTLE | RTBIGNUMINIT_F_SIGNED,
                                       abLittleEndian, cb), VINF_SUCCESS);
    RTTESTI_CHECK(Num.fNegative == pBigNum->fNegative);
    RTTESTI_CHECK(Num.cUsed == pBigNum->cUsed);
    RTTESTI_CHECK(RTBigNumCompare(&Num, pBigNum) == 0);
    RTTESTI_CHECK_RC(RTBigNumDestroy(&Num), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTBigNumInit(&Num, RTBIGNUMINIT_F_ENDIAN_LITTLE | RTBIGNUMINIT_F_SIGNED | RTBIGNUMINIT_F_SENSITIVE,
                                       abLittleEndian, cb), VINF_SUCCESS);
    RTTESTI_CHECK(Num.fNegative == pBigNum->fNegative);
    RTTESTI_CHECK(Num.cUsed == pBigNum->cUsed);
    RTTESTI_CHECK(RTBigNumCompare(&Num, pBigNum) == 0);
    RTTESTI_CHECK_RC(RTBigNumDestroy(&Num), VINF_SUCCESS);
}

static void testMoreInit(void)
{
    RTTESTI_CHECK(!g_LargePositive.fNegative);
    RTTESTI_CHECK(!g_LargePositive.fSensitive);
    RTTESTI_CHECK(!g_LargePositive2.fNegative);
    RTTESTI_CHECK(!g_LargePositive2.fSensitive);
    RTTESTI_CHECK(g_LargeNegative.fNegative);
    RTTESTI_CHECK(!g_LargeNegative.fSensitive);
    RTTESTI_CHECK(g_LargeNegative2.fNegative);
    RTTESTI_CHECK(!g_LargeNegative2.fSensitive);

    RTTESTI_CHECK(!g_Zero.fNegative);
    RTTESTI_CHECK(!g_Zero.fSensitive);
    RTTESTI_CHECK(g_Zero.cUsed == 0);

    RTTESTI_CHECK(g_Minus1.fNegative);
    RTTESTI_CHECK(!g_Minus1.fSensitive);
    RTTESTI_CHECK(g_Minus1.cUsed == 1);
    RTTESTI_CHECK(g_Minus1.pauElements[0] == 1);

    RTTESTI_CHECK(g_One.cUsed       == 1 && g_One.pauElements[0]        == 1);
    RTTESTI_CHECK(g_Two.cUsed       == 1 && g_Two.pauElements[0]        == 2);
    RTTESTI_CHECK(g_Three.cUsed     == 1 && g_Three.pauElements[0]      == 3);
    RTTESTI_CHECK(g_Four.cUsed      == 1 && g_Four.pauElements[0]       == 4);
    RTTESTI_CHECK(g_Ten.cUsed       == 1 && g_Ten.pauElements[0]        == 10);
    RTTESTI_CHECK(g_FourtyTwo.cUsed == 1 && g_FourtyTwo.pauElements[0]  == 42);

    /* Test big endian initialization w/ sensitive variation. */
    testInitOneLittleEndian(g_abLargePositive, sizeof(g_abLargePositive), &g_LargePositive);
    testInitOneLittleEndian(g_abLargePositive, sizeof(g_abLargePositive) - 11, &g_LargePositive2);

    testInitOneLittleEndian(g_abLargeNegative, sizeof(g_abLargeNegative), &g_LargeNegative);
    testInitOneLittleEndian(g_abLargeNegative, sizeof(g_abLargeNegative) - 9, &g_LargeNegative2);

    RTTESTI_CHECK(g_Minus1.cUsed == 1);
    testInitOneLittleEndian(g_abMinus1, sizeof(g_abMinus1), &g_Minus1);
    testInitOneLittleEndian(g_abMinus1, 1, &g_Minus1);
    testInitOneLittleEndian(g_abMinus1, 4, &g_Minus1);

}


static void testCompare(void)
{
    RTTestSub(g_hTest, "RTBigNumCompare*");
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive, &g_LargePositive) == 0);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive2, &g_LargePositive) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive, &g_LargePositive2) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_Zero, &g_LargePositive) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive, &g_Zero) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive2, &g_Zero) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive, &g_LargePositiveMinus1) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositiveMinus1, &g_LargePositive) == -1);

    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative, &g_LargeNegative) == 0);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative, &g_LargeNegative2) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative2, &g_LargeNegative) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_Zero, &g_LargeNegative) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative, &g_Zero) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative2, &g_Zero) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative, &g_LargeNegativePluss1) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegativePluss1, &g_LargeNegative) == 1);

    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative, &g_LargePositive) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive, &g_LargeNegative) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative2, &g_LargePositive) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive, &g_LargeNegative2) == 1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative2, &g_LargePositive2) == -1);
    RTTESTI_CHECK(RTBigNumCompare(&g_LargePositive2, &g_LargeNegative2) == 1);

    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_Zero, 0) == 0);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_Zero, 1) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_Zero, UINT32_MAX) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_Zero, UINT64_MAX) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_LargePositive, UINT64_MAX) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_LargePositive2, 0x7213593) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_LargeNegative, 0) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_LargeNegative, 1) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_LargeNegative, UINT64_MAX) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_LargeNegative, 0x80034053) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_64BitPositive1, g_u64BitPositive1) == 0);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_64BitPositive1, g_u64BitPositive1 - 1) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithU64(&g_64BitPositive1, g_u64BitPositive1 + 1) == -1);

    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Zero, 0) == 0);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Zero, 1) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Zero, -1) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Zero, INT32_MAX) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_LargeNegative, INT32_MIN) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_LargeNegative, INT64_MIN) == -1);
    RTTESTI_CHECK(g_u64BitPositive1 < (uint64_t)INT64_MAX);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_64BitPositive1, g_u64BitPositive1) == 0);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_64BitPositive1, g_u64BitPositive1 - 1) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_64BitPositive1, g_u64BitPositive1 + 1) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_64BitPositive1, INT64_MIN) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_64BitPositive1, INT64_MAX) == -1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Minus1, -1) == 0);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Minus1, -2) == 1);
    RTTESTI_CHECK(RTBigNumCompareWithS64(&g_Minus1, 0) == -1);
}


static void testSubtraction(void)
{
    RTTestSub(g_hTest, "RTBigNumSubtract");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Result;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result, fFlags), VINF_SUCCESS);
        RTBIGNUM Result2;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result2, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_Minus1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_Zero, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_Minus1, &g_Zero), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_64BitPositive1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithU64(&Result, g_u64BitPositive1 + 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_Minus1, &g_64BitPositive1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, INT64_C(-1) - g_u64BitPositive1) == 0);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_LargePositive, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);
        RTTESTI_CHECK(Result.cUsed == 1);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_LargePositiveMinus1, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);
        RTTESTI_CHECK(Result.cUsed == 1);

        RTTESTI_CHECK(RTBigNumCompare(&g_LargeNegative, &g_LargeNegativePluss1) < 0);
        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_LargeNegative, &g_LargeNegativePluss1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);
        RTTESTI_CHECK(Result.cUsed == 1);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_LargeNegativePluss1, &g_LargeNegative), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);
        RTTESTI_CHECK(Result.cUsed == 1);

        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result, &g_LargeNegativePluss1, &g_LargeNegativePluss1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);
        RTTESTI_CHECK(Result.cUsed == 0);

        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result2), VINF_SUCCESS);
    }
}


static void testAddition(void)
{
    RTTestSub(g_hTest, "RTBigNumAdd");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Result;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result, fFlags), VINF_SUCCESS);
        RTBIGNUM Result2;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result2, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_Minus1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -2) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_Zero, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_Zero, &g_64BitPositive1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithU64(&Result, g_u64BitPositive1) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_Minus1, &g_64BitPositive1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithU64(&Result, g_u64BitPositive1 - 1) == 0);

        RTTESTI_CHECK(g_u64BitPositive1 * 2 > g_u64BitPositive1);
        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_64BitPositive1, &g_64BitPositive1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithU64(&Result, g_u64BitPositive1 * 2) == 0);


        RTTESTI_CHECK_RC(RTBigNumAssign(&Result2, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumNegateThis(&Result2), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_LargePositive, &Result2), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithU64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &Result2, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithU64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_LargePositiveMinus1, &Result2), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &Result2, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);


        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_LargePositive, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result, &g_LargePositive) > 0);
        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result2, &Result, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result2, &g_LargePositive) == 0);
        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result2, &Result, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result2, &g_LargePositiveMinus1) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_LargePositive, &g_LargeNegative), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result, &g_LargeNegative) > 0);
        RTTESTI_CHECK(RTBigNumCompare(&Result, &g_LargePositive) < 0);
        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result2, &Result, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result2, &g_LargeNegative) == 0);
        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result2, &Result, &g_LargeNegative), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result2, &g_LargePositive) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_LargeNegativePluss1, &g_LargeNegative), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result, &g_LargeNegative) < 0);
        RTTESTI_CHECK_RC(RTBigNumSubtract(&Result2, &Result, &g_LargeNegative), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result2, &g_LargeNegativePluss1) == 0);

        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result2), VINF_SUCCESS);
    }
}

static bool testHexStringToNum(PRTBIGNUM pBigNum, const char *pszHex, uint32_t fFlags)
{
    uint8_t abBuf[_4K];
    size_t  cbHex = strlen(pszHex);
    RTTESTI_CHECK_RET(!(cbHex & 1), false);
    cbHex /= 2;
    RTTESTI_CHECK_RET(cbHex < sizeof(abBuf), false);
    RTTESTI_CHECK_RC_RET(RTStrConvertHexBytes(pszHex, abBuf, cbHex, 0), VINF_SUCCESS, false);
    RTTESTI_CHECK_RC_RET(RTBigNumInit(pBigNum, RTBIGNUMINIT_F_ENDIAN_BIG | fFlags, abBuf, cbHex), VINF_SUCCESS, false);
    return true;
}

static void testMultiplication(void)
{
    RTTestSub(g_hTest, "RTBigNumMultiply");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Result;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result, fFlags), VINF_SUCCESS);
        RTBIGNUM Result2;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result2, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumMultiply(&Result, &g_Minus1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumMultiply(&Result, &g_Zero, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);
        RTTESTI_CHECK_RC(RTBigNumMultiply(&Result, &g_Minus1, &g_Zero), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumMultiply(&Result, &g_Minus1, &g_64BitPositive1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -(int64_t)g_u64BitPositive1) == 0);
        RTTESTI_CHECK_RC(RTBigNumMultiply(&Result, &g_64BitPositive1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -(int64_t)g_u64BitPositive1) == 0);


        static struct
        {
            const char *pszF1, *pszF2, *pszResult;
        } s_aTests[] =
        {
            {
                "29865DBFA717181B9DD4B515BD072DE10A5A314385F6DED735AC553FCD307D30C499",
                "4DD65692F7365B90C55F63988E5B6C448653E7DB9DD941507586BD8CF71398287C",
                "0CA02E8FFDB0EEA37264338A4AAA91C8974E162DDFCBCF804B434A11955671B89B3645AAB75423D60CA3459B0B4F3F28978DA768779FB54CF362FD61924637582F221C"
            },
            {
                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE0000000000000000000000000000000000000001"
            }
        };
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            RTBIGNUM F1, F2, Expected;
            if (   testHexStringToNum(&F1, s_aTests[i].pszF1, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&F2, s_aTests[i].pszF2, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Expected, s_aTests[i].pszResult, RTBIGNUMINIT_F_UNSIGNED | fFlags))
            {
                RTTESTI_CHECK_RC(RTBigNumMultiply(&Result, &F1, &F2), VINF_SUCCESS);
                RTTESTI_CHECK(RTBigNumCompare(&Result, &Expected) == 0);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&F1), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&F2), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Expected), VINF_SUCCESS);
            }
        }
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result2), VINF_SUCCESS);
    }
}


static void testDivision(void)
{
    RTTestSub(g_hTest, "RTBigNumDivide");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Quotient;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Quotient, fFlags), VINF_SUCCESS);
        RTBIGNUM Remainder;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Remainder, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_Minus1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 1) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_Zero, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 0) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_Minus1, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);
        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_LargeNegative, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);
        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_LargePositive, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_Four, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 2) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_Three, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 1) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_Ten, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 5) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, 0) == 0);


        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_LargePositive, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 1) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &g_LargeNegative, &g_LargeNegativePluss1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Quotient, 1) == 0);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Remainder, -1) == 0);


#if 0
        static struct
        {
            const char *pszF1, *pszF2, *pszQuotient, *pszRemainder;
        } s_aTests[] =
        {
            {
                "29865DBFA717181B9DD4B515BD072DE10A5A314385F6DED735AC553FCD307D30C499",
                "4DD65692F7365B90C55F63988E5B6C448653E7DB9DD941507586BD8CF71398287C",
                "0CA02E8FFDB0EEA37264338A4AAA91C8974E162DDFCBCF804B434A11955671B89B3645AAB75423D60CA3459B0B4F3F28978DA768779FB54CF362FD61924637582F221C"
            },
            {
                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
                "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE0000000000000000000000000000000000000001"
            }
        };
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            RTBIGNUM F1, F2, Expected;
            if (   testHexStringToNum(&F1, s_aTests[i].pszF1, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&F2, s_aTests[i].pszF2, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Expected, s_aTests[i].pszResult, RTBIGNUMINIT_F_UNSIGNED | fFlags))
            {
                RTTESTI_CHECK_RC(RTBigNumDivide(&Quotient, &Remainder, &F1, &F2), VINF_SUCCESS);
                RTTESTI_CHECK(RTBigNumCompare(&Quotient, &Expected) == 0);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&F1), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&F2), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Expected), VINF_SUCCESS);
            }
        }
#endif
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Quotient), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Remainder), VINF_SUCCESS);
    }
}


static void testModulo(void)
{
    RTTestSub(g_hTest, "RTBigNumModulo");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Result;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result, fFlags), VINF_SUCCESS);
        RTBIGNUM Tmp;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Tmp, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_Minus1, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_Zero, &g_Minus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_Minus1, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);
        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_LargeNegative, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);
        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_LargePositive, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_Four, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_Three, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_Ten, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_LargePositive, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_LargePositiveMinus1, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result, &g_LargePositiveMinus1) == 0);

        RTTESTI_CHECK_RC(RTBigNumAdd(&Result, &g_LargePositiveMinus1, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumAdd(&Tmp, &g_LargePositive, &Result), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &Tmp, &g_LargePositiveMinus1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 2) == 0);
        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &Tmp, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompare(&Result, &g_LargePositiveMinus1) == 0);

        RTTESTI_CHECK_RC(RTBigNumModulo(&Result, &g_LargeNegative, &g_LargeNegativePluss1), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, -1) == 0);

        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result), VINF_SUCCESS);
    }
}


static void testExponentiation(void)
{
    RTTestSub(g_hTest, "RTBigNumExponentiate");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Result;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result, fFlags), VINF_SUCCESS);
        RTBIGNUM Result2;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result2, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &g_One, &g_One), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &g_Two, &g_One), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 2) == 0);

        RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &g_Two, &g_Two), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 4) == 0);

        RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &g_Two, &g_Ten), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1024) == 0);

        RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &g_Five, &g_Five), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 3125) == 0);

        RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &g_Five, &g_Ten), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 9765625) == 0);

        static struct
        {
            const char *pszBase, *pszExponent, *pszResult;
        } s_aTests[] =
        {
            {
                "180DB4284A119D6133AE4BB0C27C27D1", /*^*/ "3A", /* = */
                "04546412B9E39476F10009F62608F614774C5AE475482434F138C3EA976583ECE09E58F1F03CE41F821A1D5DA59B69D031290B0AC7F7D5058E3AFA2CA3DAA7261D1620CA"
                "D050576C0AFDF51ADBFCB9073B9D8324E816EA6BE4648DF68092F6617ED609045E6BE9D5410AE2CFF725832414E67656233F4DFA952461D321282426D50E2AF524D779EC"
                "0744547E8A4F0768C2C49AF3A5A89D129430CA58456BE4534BC53C67523506C7A8B5770D88CF28B6B3EEBE73F3EA71BA2CE27C4C89BE0D699922B1A1EB20143CB0830A43"
                "D864DDFFF026BA781614C2D55F3EDEA7257B93A0F40824E57D6EDFCFFB4611C316374D0D15698E6584851F1898DCAE75FC4D180908763DDB2FF93766EF144D091274AFE5"
                "6980A1F4F574D577DAD833EA9486A4B499BFCA9C08225D7BDB2C632B4D9B53EF51C02ED419F22657D626064BCC2B083CD664E1A8D68F82F33233A833AC98AA0282B8B88D"
                "A430CF2E581A1C7C4A1D646CA42760ED10C398F7C032A94D53964E6885B5C1CA884EC15081D4C010978627C85767FEC6F93364044EA86567F9610ABFB837808CC995FB5F"
                "710B21CE198E0D4AD9F73C3BD56CB9965C85C790BF3F4B326B5245BFA81783126217BF80687C4A8AA3AE80969A4407191B4F90E71A0ABCCB5FEDD40477CE9D10FBAEF103"
                "8457AB19BD793CECDFF8B29A96F12F590BFED544E08F834A44DEEF461281C40024EFE9388689AAC69BCBAB3D06434172D9319F30754756E1CF77B300679215BEBD27FC20"
                "A2F1D2029BC767D4894A5F7B21BD784CD1DD4F41697839969CB6D2AA1E0AFA5D3D644A792586F681EB36475CAE59EB457E55D6AC2E286E196BFAC000C7389A96C514552D"
                "5D9D3DD962F72DAE4A7575A9A67856646239560A39E50826BB2523598C8F8FF0EC8D09618378E9F362A8FBFE842B55CD1855A95D8A5E93B8B91D31EB8FBBF57113F06171"
                "BB69B81C4240EC4C7D1AC67EA1CE4CEBEE71828917EC1CF500E1AD2F09535F5498CD6E613383810A840A265AED5DD20AE58FFF2D0DEB8EF99FA494B22714F520E8E8B684"
                "5E8521966A7B1699236998A730FDF9F049CE2A4EA44D1EBC3B9754908848540D0DEE64A6D60E2BFBC3362B659C10543BDC20C1BAD3D68B173442C100C2C366CB885E8490"
                "EDB977E49E9D51D4427B73B3B999AF4BA17685387182C3918D20808197A2E3FCDD0F66ECDEC05542C23A08B94C83BDF93606A49E9A0645B002CFCA1EAE1917BEED0D6542"
                "9A0EF00E5FB5F70D61C8C4DF1F1E9DA58188A221"
            },
            {
                "03", /*^*/ "164b", /* = */
                "29ABEC229C2B15C41573F8608D4DCD2DADAACA94CA3C40B42FFAD32D6202E228E16F61E050FF97EC5D45F24A4EB057C2D1A5DA72DFC5944E6941DBEDDE70EF56702BEC35"
                "A3150EFE84E87185E3CBAB1D73F434EB820E41298BDD4F3941230DFFD8DFF1D2E2F3C5D0CB5088505B9C78507A81AAD8073C28B8FA70771C3E04110344328C6B3F38E55A"
                "32B009F4DDA1813232C3FF422DF4E4D12545C803C63D0BE67E2E773B2BAC41CC69D895787B217D7BE9CE80BD4B500AE630AA21B50A06E0A74953F8011E9F23863CA79885"
                "35D5FF0214DBD9B25756BE3D43008A15C018348E6A7C3355F4BECF37595BD530E5AC1AD3B14182862E47AD002097465F6B78F435B0D6365E18490567F508CD3CAAAD340A"
                "E76A218FE8B517F923FE9CCDE61CB35409590CDBC606D89BA33B32A3862DEE7AB99DFBE103D02D2BED6D418B949E6B3C51CAB8AB5BE93AA104FA10D3A02D4CAD6700CD0F"
                "83922EAAB18705915198DE51C1C562984E2B7571F36A4D756C459B61E0A4B7DE268A74E807311273DD51C2863771AB72504044C870E2498F13BF1DE92C13D93008E304D2"
                "879C5D8A646DB5BF7BC64D96BB9E2FBA2EA6BF55CD825ABD995762F661C327133BE01F9A9F298CA096B3CE61CBBD8047A003870B218AC505D72ED6C7BF3B37BE5877B6A1"
                "606A713EE86509C99B2A3627FD74AE7E81FE7F69C34B40E01A6F8B18A328E0F9D18A7911E5645331540538AA76B6D5D591F14313D730CFE30728089A245EE91058748F0C"
                "E3E6CE4DE51D23E233BFF9007E0065AEBAA3FB0D0FACE62A4757FE1C9C7075E2214071197D5074C92AF1E6D853F7DE782F32F1E40507CB981A1C10AC6B1C23AC46C07EF1"
                "EDE857C444902B936771DF75E0EE6C2CB3F0F9DBB387BAD0658E98F42A7338DE45E2F1B012B530FFD66861F74137C041D7558408A4A23B83FBDDE494381D9F9FF0326D44"
                "302F75DE68B91A54CFF6E3C2821D09F2664CA74783C29AF98E2F1D3D84CAC49EAE55BABE3D2CBE8833D50517109E19CB5C63D1DE26E308ACC213D1CBCCF7C3AAE05B06D9"
                "909AB0A1AEFD02A193CFADC7F724D377E1F4E78DC21012BE26D910548CDF55B0AB9CB64756045FF48C3B858E954553267C4087EC5A9C860CFA56CF5CFBB442BDDA298230"
                "D6C000A6A6010D87FB4C3859C3AFAF15C37BCE03EBC392E8149056C489508841110060A991F1EEAF1E7CCF0B279AB2B35F3DAC0FAB4F4A107794E67D305E6D61A27C8FEB"
                "DEA00C3334C888B2092E740DD3EFF7A69F06CE12EF511126EB23D80902D1D54BF4AEE04DF9457D59E8859AA83D6229481E1B1BC7C3ED96F6F7C1CEEF7B904268FD00BE51"
                "1EF69692D593F8A9F7CCC053C343306940A4054A55DBA94D95FF6D02B7A73E110C2DBE6CA29C01B5921420B5BC9C92DAA9D82003829C6AE772FF12135C2E138C6725DC47"
                "7938F3062264575EBBB1CBB359E496DD7A38AE0E33D1B1D9C16BDD87E6DE44DFB832286AE01D00AA14B423DBF7ECCC34A0A06A249707B75C2BA931D7F4F513FDF0F6E516"
                "345B8DA85FEFD218B390828AECADF0C47916FAF44CB29010B0BB2BBA8E120B6DAFB2CC90B9D1B8659C2AFB"
            }
        };
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            RTBIGNUM Base, Exponent, Expected;
            if (   testHexStringToNum(&Base, s_aTests[i].pszBase, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Exponent, s_aTests[i].pszExponent, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Expected, s_aTests[i].pszResult, RTBIGNUMINIT_F_UNSIGNED | fFlags))
            {
                RTTESTI_CHECK_RC(RTBigNumExponentiate(&Result, &Base, &Exponent), VINF_SUCCESS);
                RTTESTI_CHECK(RTBigNumCompare(&Result, &Expected) == 0);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Base), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Exponent), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Expected), VINF_SUCCESS);
            }
        }
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result2), VINF_SUCCESS);
    }
}


static void testModExp(void)
{
    RTTestSub(g_hTest, "RTBigNumModExp");

    for (uint32_t fFlags = 0; fFlags <= RTBIGNUMINIT_F_SENSITIVE; fFlags += RTBIGNUMINIT_F_SENSITIVE)
    {
        RTBIGNUM Result;
        RTTESTI_CHECK_RC_RETV(RTBigNumInitZero(&Result, fFlags), VINF_SUCCESS);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_One, &g_One, &g_One), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_One, &g_One), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_LargePositive, &g_One), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_One, &g_Zero, &g_Five), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 1);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_Zero, &g_Five), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 1);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_Zero, &g_One), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_Zero, &g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 1);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Zero, &g_Zero, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_Zero, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);
        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_LargePositive, &g_LargePositive, &g_Zero), VERR_BIGNUM_DIV_BY_ZERO);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Two, &g_Four, &g_Five), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Two, &g_Four, &g_Three), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 1) == 0);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Three, &g_Three, &g_Three), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 0) == 0);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Three, &g_Three, &g_Five), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 2) == 0);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Three, &g_Five, &g_Five), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 3) == 0);

        RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &g_Three, &g_Five, &g_Four), VINF_SUCCESS);
        RTTESTI_CHECK(RTBigNumCompareWithS64(&Result, 3) == 0);

#if 0
        static struct
        {
            const char *pszBase, *pszExponent, *pszModulus, *pszResult;
        } s_aTests[] =
        {
            {
                "180DB4284A119D6133AE4BB0C27C27D1", /*^*/ "3A", /*mod */ " ",  /* = */
            },
        };
        for (uint32_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
        {
            RTBIGNUM Base, Exponent, Expected, Modulus;
            if (   testHexStringToNum(&Base, s_aTests[i].pszBase, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Exponent, s_aTests[i].pszExponent, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Modulus, s_aTests[i].pszModulus, RTBIGNUMINIT_F_UNSIGNED | fFlags)
                && testHexStringToNum(&Expected, s_aTests[i].pszResult, RTBIGNUMINIT_F_UNSIGNED | fFlags))
            {
                RTTESTI_CHECK_RC(RTBigNumModExp(&Result, &Base, &Exponent, &Modulus), VINF_SUCCESS);
                RTTESTI_CHECK(RTBigNumCompare(&Result, &Expected) == 0);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Base), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Exponent), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Expected), VINF_SUCCESS);
                RTTESTI_CHECK_RC(RTBigNumDestroy(&Modulus), VINF_SUCCESS);
            }
        }
#endif

        RTTESTI_CHECK_RC(RTBigNumDestroy(&Result), VINF_SUCCESS);
    }
}


static void testToBytes(void)
{
    RTTestSub(g_hTest, "RTBigNumToBytes*Endian");
    uint8_t abBuf[sizeof(g_abLargePositive) + sizeof(g_abLargeNegative)];

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Zero, abBuf, 1), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0 && abBuf[1] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Zero, abBuf, 2), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0 && abBuf[1] == 0 && abBuf[2] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Zero, abBuf, 3), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0 && abBuf[1] == 0 && abBuf[2] == 0 && abBuf[3] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Zero, abBuf, 4), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0 && abBuf[1] == 0 && abBuf[2] == 0 && abBuf[3] == 0 && abBuf[4] == 0xcc);


    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Minus1, abBuf, 1), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0xff && abBuf[1] == 0xcc && abBuf[2] == 0xcc && abBuf[3] == 0xcc && abBuf[4] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Minus1, abBuf, 2), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0xff && abBuf[1] == 0xff && abBuf[2] == 0xcc && abBuf[3] == 0xcc && abBuf[4] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Minus1, abBuf, 3), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0xff && abBuf[1] == 0xff && abBuf[2] == 0xff && abBuf[3] == 0xcc && abBuf[4] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_Minus1, abBuf, 4), VINF_SUCCESS);
    RTTESTI_CHECK(abBuf[0] == 0xff && abBuf[1] == 0xff && abBuf[2] == 0xff && abBuf[3] == 0xff && abBuf[4] == 0xcc);


    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_LargePositive, abBuf, sizeof(g_abLargePositive)), VINF_SUCCESS);
    RTTESTI_CHECK(memcmp(abBuf, g_abLargePositive, sizeof(g_abLargePositive)) == 0);
    RTTESTI_CHECK(abBuf[sizeof(g_abLargePositive)] == 0xcc);

    memset(abBuf, 0xcc, sizeof(abBuf));
    RTTESTI_CHECK_RC(RTBigNumToBytesBigEndian(&g_LargePositive, abBuf, sizeof(g_abLargePositive) -1 ), VERR_BUFFER_OVERFLOW);
    RTTESTI_CHECK(memcmp(abBuf, &g_abLargePositive[1], sizeof(g_abLargePositive) - 1) == 0);
    RTTESTI_CHECK(abBuf[sizeof(g_abLargePositive) - 1] == 0xcc);

}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTBigNum", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    /* Init fixed integers. */
    RTTestSub(g_hTest, "RTBigNumInit");
    RTTESTI_CHECK_RC(RTBigNumInit(&g_LargePositive, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abLargePositive, sizeof(g_abLargePositive)), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_LargePositive2, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abLargePositive, sizeof(g_abLargePositive) - 11), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_LargePositiveMinus1, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abLargePositiveMinus1, sizeof(g_abLargePositiveMinus1)), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_LargeNegative, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abLargeNegative, sizeof(g_abLargeNegative)), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_LargeNegative2, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abLargeNegative, sizeof(g_abLargeNegative) - 9), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_LargeNegativePluss1, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abLargeNegativePluss1, sizeof(g_abLargeNegativePluss1)), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_64BitPositive1, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_ab64BitPositive1, sizeof(g_ab64BitPositive1)), VINF_SUCCESS);

    RTTESTI_CHECK_RC(RTBigNumInitZero(&g_Zero, 0), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_One,       RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x01", 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_Two,       RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x02", 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_Three,     RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x03", 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_Four,      RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x04", 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_Five,      RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x05", 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_Ten,       RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x0a", 1), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTBigNumInit(&g_FourtyTwo, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED, "\x2a", 1), VINF_SUCCESS);

    RTTESTI_CHECK_RC(RTBigNumInit(&g_Minus1, RTBIGNUMINIT_F_ENDIAN_BIG | RTBIGNUMINIT_F_SIGNED,
                                  g_abMinus1, sizeof(g_abMinus1)), VINF_SUCCESS);
    testMoreInit();

    if (RTTestIErrorCount() == 0)
    {
        /* Do testing. */
        testCompare();
        testSubtraction();
        testAddition();
        testMultiplication();
        testDivision();
        testModulo();
        testExponentiation();
        testModExp();
        testToBytes();

        /* Cleanups. */
        RTTestSub(g_hTest, "RTBigNumDestroy");
        RTTESTI_CHECK_RC(RTBigNumDestroy(&g_LargePositive), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&g_LargePositive2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&g_LargeNegative), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&g_LargeNegative2), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&g_Zero), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTBigNumDestroy(&g_64BitPositive1), VINF_SUCCESS);
    }

    return RTTestSummaryAndDestroy(g_hTest);
}

