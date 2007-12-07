/* $Id$ */
/** @file
 * innotek Portable Runtime - UUID, Generic.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <iprt/uuid.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/rand.h>


/* WARNING: This implementation ASSUMES little endian. */


/**
 * Generates a new UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated uuid.
 */
RTDECL(int)  RTUuidCreate(PRTUUID pUuid)
{
    /* validate input. */
    AssertReturn(pUuid, VERR_INVALID_PARAMETER);

    RTRandBytes(pUuid, sizeof(*pUuid));
    pUuid->Gen.u16ClockSeq = (pUuid->Gen.u16ClockSeq & 0x3fff) | 0x8000;
    pUuid->Gen.u16TimeHiAndVersion = (pUuid->Gen.u16TimeHiAndVersion & 0x0fff) | 0x4000;

    return VINF_SUCCESS;
}


/**
 * Makes a null UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated null uuid.
 */
RTDECL(int)  RTUuidClear(PRTUUID pUuid)
{
    AssertReturn(pUuid, VERR_INVALID_PARAMETER);
    pUuid->au64[0] = 0;
    pUuid->au64[1] = 0;
    return VINF_SUCCESS;
}


/**
 * Checks if UUID is null.
 *
 * @returns true if UUID is null.
 * @param   pUuid           uuid to check.
 */
RTDECL(int)  RTUuidIsNull(PCRTUUID pUuid)
{
    AssertReturn(pUuid, VERR_INVALID_PARAMETER);
    return !pUuid->au64[0]
        && !pUuid->au64[1];
}


/**
 * Compares two UUID values.
 *
 * @returns 0 if eq, < 0 or > 0.
 * @param   pUuid1          First value to compare.
 * @param   pUuid2          Second value to compare.
 */
RTDECL(int)  RTUuidCompare(PCRTUUID pUuid1, PCRTUUID pUuid2)
{
    /*
     * Special cases.
     */
    if (pUuid1 == pUuid2)
        return 0;
    if (!pUuid1)
        return RTUuidIsNull(pUuid2) ? 0 : -1;
    if (!pUuid2)
        return RTUuidIsNull(pUuid1) ? 0 : 1;

    /*
     * Standard cases.
     */
    if (pUuid1->Gen.u32TimeLow != pUuid2->Gen.u32TimeLow)
        return pUuid1->Gen.u32TimeLow < pUuid2->Gen.u32TimeLow ? -1 : 1;
    if (pUuid1->Gen.u16TimeMid != pUuid2->Gen.u16TimeMid)
        return pUuid1->Gen.u16TimeMid < pUuid2->Gen.u16TimeMid ? -1 : 1;
    if (pUuid1->Gen.u16TimeHiAndVersion != pUuid2->Gen.u16TimeHiAndVersion)
        return pUuid1->Gen.u16TimeHiAndVersion < pUuid2->Gen.u16TimeHiAndVersion ? -1 : 1;
    if (pUuid1->Gen.u16ClockSeq != pUuid2->Gen.u16ClockSeq)
        return pUuid1->Gen.u16ClockSeq < pUuid2->Gen.u16ClockSeq ? -1 : 1;
    if (pUuid1->Gen.au8Node[0] != pUuid2->Gen.au8Node[0])
        return pUuid1->Gen.au8Node[0] < pUuid2->Gen.au8Node[0] ? -1 : 1;
    if (pUuid1->Gen.au8Node[1] != pUuid2->Gen.au8Node[1])
        return pUuid1->Gen.au8Node[1] < pUuid2->Gen.au8Node[1] ? -1 : 1;
    if (pUuid1->Gen.au8Node[2] != pUuid2->Gen.au8Node[2])
        return pUuid1->Gen.au8Node[2] < pUuid2->Gen.au8Node[2] ? -1 : 1;
    if (pUuid1->Gen.au8Node[3] != pUuid2->Gen.au8Node[3])
        return pUuid1->Gen.au8Node[3] < pUuid2->Gen.au8Node[3] ? -1 : 1;
    if (pUuid1->Gen.au8Node[4] != pUuid2->Gen.au8Node[4])
        return pUuid1->Gen.au8Node[4] < pUuid2->Gen.au8Node[4] ? -1 : 1;
    if (pUuid1->Gen.au8Node[5] != pUuid2->Gen.au8Node[5])
        return pUuid1->Gen.au8Node[5] < pUuid2->Gen.au8Node[5] ? -1 : 1;
    return 0;
}


/**
 * Converts binary UUID to its string representation.
 *
 * @returns iprt status code.
 * @param   pUuid           Uuid to convert.
 * @param   pszString       Where to store result string.
 * @param   cchString       pszString buffer length, must be >= RTUUID_STR_LENGTH.
 */
RTDECL(int)  RTUuidToStr(PCRTUUID pUuid, char *pszString, unsigned cchString)
{
    /* validate parameters */
    AssertReturn(pUuid, VERR_INVALID_PARAMETER);
    AssertReturn(pszString, VERR_INVALID_PARAMETER);
    AssertReturn(cchString >= RTUUID_STR_LENGTH, VERR_INVALID_PARAMETER);

    /*
     * RTStrPrintf(,,"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
     *             pUuid->Gen.u32TimeLow,
     *             pUuid->Gen.u16TimeMin,
     *             pUuid->Gen.u16TimeHiAndVersion,
     *             pUuid->Gen.u16ClockSeq & 0xff,
     *             pUuid->Gen.u16ClockSeq >> 8,
     *             pUuid->Gen.au8Node[0],
     *             pUuid->Gen.au8Node[1],
     *             pUuid->Gen.au8Node[2],
     *             pUuid->Gen.au8Node[3],
     *             pUuid->Gen.au8Node[4],
     *             pUuid->Gen.au8Node[5]);
     */
    static const char s_achDigits[17] = "0123456789abcdef";
    uint32_t u32TimeLow = pUuid->Gen.u32TimeLow;
    pszString[ 0] = s_achDigits[(u32TimeLow >> 28)/*& 0xf*/];
    pszString[ 1] = s_achDigits[(u32TimeLow >> 24) & 0xf];
    pszString[ 2] = s_achDigits[(u32TimeLow >> 20) & 0xf];
    pszString[ 3] = s_achDigits[(u32TimeLow >> 16) & 0xf];
    pszString[ 4] = s_achDigits[(u32TimeLow >> 12) & 0xf];
    pszString[ 5] = s_achDigits[(u32TimeLow >>  8) & 0xf];
    pszString[ 6] = s_achDigits[(u32TimeLow >>  4) & 0xf];
    pszString[ 7] = s_achDigits[(u32TimeLow/*>>0*/)& 0xf];
    pszString[ 8] = '-';
    unsigned u = pUuid->Gen.u16TimeMid;
    pszString[ 9] = s_achDigits[(u >> 12)/*& 0xf*/];
    pszString[10] = s_achDigits[(u >>  8) & 0xf];
    pszString[11] = s_achDigits[(u >>  4) & 0xf];
    pszString[12] = s_achDigits[(u/*>>0*/)& 0xf];
    pszString[13] = '-';
    u = pUuid->Gen.u16TimeHiAndVersion;
    pszString[14] = s_achDigits[(u >> 12)/*& 0xf*/];
    pszString[15] = s_achDigits[(u >>  8) & 0xf];
    pszString[16] = s_achDigits[(u >>  4) & 0xf];
    pszString[17] = s_achDigits[(u/*>>0*/)& 0xf];
    pszString[18] = '-';
    u = pUuid->Gen.u16ClockSeq;
    pszString[19] = s_achDigits[(u >>  4) & 0xf];
    pszString[20] = s_achDigits[(u/*>>0*/)& 0xf];
    pszString[21] = s_achDigits[(u >> 12)/*& 0xf*/];
    pszString[22] = s_achDigits[(u >>  8) & 0xf];
    pszString[23] = '-';
    pszString[24] = s_achDigits[pUuid->Gen.au8Node[0] >> 4];
    pszString[25] = s_achDigits[pUuid->Gen.au8Node[0] & 0xf];
    pszString[26] = s_achDigits[pUuid->Gen.au8Node[1] >> 4];
    pszString[27] = s_achDigits[pUuid->Gen.au8Node[1] & 0xf];
    pszString[28] = s_achDigits[pUuid->Gen.au8Node[2] >> 4];
    pszString[29] = s_achDigits[pUuid->Gen.au8Node[2] & 0xf];
    pszString[30] = s_achDigits[pUuid->Gen.au8Node[3] >> 4];
    pszString[31] = s_achDigits[pUuid->Gen.au8Node[3] & 0xf];
    pszString[32] = s_achDigits[pUuid->Gen.au8Node[4] >> 4];
    pszString[33] = s_achDigits[pUuid->Gen.au8Node[4] & 0xf];
    pszString[34] = s_achDigits[pUuid->Gen.au8Node[5] >> 4];
    pszString[35] = s_achDigits[pUuid->Gen.au8Node[5] & 0xf];
    pszString[36] = '\0';

    return VINF_SUCCESS;
}


/**
 * Converts UUID from its string representation to binary format.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store result Uuid.
 * @param   pszString       String with UUID text data.
 */
RTDECL(int)  RTUuidFromStr(PRTUUID pUuid, const char *pszString)
{
    /* 0xff if not a hex number, otherwise the value. (Assumes UTF-8 encoded strings.) */
    static const uint8_t s_aDigits[256] =
    {
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 0..0f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 10..1f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 20..2f */
        0x00,0x01,0x02,0x03, 0x04,0x05,0x06,0x07,  0x08,0x09,0xff,0xff, 0xff,0xff,0xff,0xff, /* 30..3f */
        0xff,0x0a,0x0b,0x0c, 0x0d,0x0e,0x0f,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 40..4f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 50..5f */
        0xff,0x0a,0x0b,0x0c, 0x0d,0x0e,0x0f,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 60..6f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 70..7f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 80..8f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* 90..9f */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* a0..af */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* b0..bf */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* c0..cf */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* d0..df */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* e0..ef */
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,  0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, /* f0..ff */
    };

    /*
     * Validate parameters.
     */
    AssertReturn(pUuid, VERR_INVALID_PARAMETER);
    AssertReturn(pszString, VERR_INVALID_PARAMETER);

#define MY_CHECK(expr) do { if (RT_UNLIKELY(!(expr))) return VERR_INVALID_UUID_FORMAT; } while (0)
#define MY_ISXDIGIT(ch) (s_aDigits[(ch) & 0xff] != 0xff)
    MY_CHECK(MY_ISXDIGIT(pszString[ 0]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 1]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 2]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 3]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 4]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 5]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 6]));
    MY_CHECK(MY_ISXDIGIT(pszString[ 7]));
    MY_CHECK(pszString[ 8] == '-');
    MY_CHECK(MY_ISXDIGIT(pszString[ 9]));
    MY_CHECK(MY_ISXDIGIT(pszString[10]));
    MY_CHECK(MY_ISXDIGIT(pszString[11]));
    MY_CHECK(MY_ISXDIGIT(pszString[12]));
    MY_CHECK(pszString[13] == '-');
    MY_CHECK(MY_ISXDIGIT(pszString[14]));
    MY_CHECK(MY_ISXDIGIT(pszString[15]));
    MY_CHECK(MY_ISXDIGIT(pszString[16]));
    MY_CHECK(MY_ISXDIGIT(pszString[17]));
    MY_CHECK(pszString[18] == '-');
    MY_CHECK(MY_ISXDIGIT(pszString[19]));
    MY_CHECK(MY_ISXDIGIT(pszString[20]));
    MY_CHECK(MY_ISXDIGIT(pszString[21]));
    MY_CHECK(MY_ISXDIGIT(pszString[22]));
    MY_CHECK(pszString[23] == '-');
    MY_CHECK(MY_ISXDIGIT(pszString[24]));
    MY_CHECK(MY_ISXDIGIT(pszString[25]));
    MY_CHECK(MY_ISXDIGIT(pszString[26]));
    MY_CHECK(MY_ISXDIGIT(pszString[27]));
    MY_CHECK(MY_ISXDIGIT(pszString[28]));
    MY_CHECK(MY_ISXDIGIT(pszString[29]));
    MY_CHECK(MY_ISXDIGIT(pszString[30]));
    MY_CHECK(MY_ISXDIGIT(pszString[31]));
    MY_CHECK(MY_ISXDIGIT(pszString[32]));
    MY_CHECK(MY_ISXDIGIT(pszString[33]));
    MY_CHECK(MY_ISXDIGIT(pszString[34]));
    MY_CHECK(MY_ISXDIGIT(pszString[35]));
    MY_CHECK(!pszString[36]);
#undef MY_ISXDIGIT
#undef MY_CHECK

    /*
     * Inverse of RTUuidToStr (see above).
     */
#define MY_TONUM(ch) (s_aDigits[(ch) & 0xff])
    pUuid->Gen.u32TimeLow = (uint32_t)MY_TONUM(pszString[ 0]) << 28
                          | (uint32_t)MY_TONUM(pszString[ 1]) << 24
                          | (uint32_t)MY_TONUM(pszString[ 2]) << 20
                          | (uint32_t)MY_TONUM(pszString[ 3]) << 16
                          | (uint32_t)MY_TONUM(pszString[ 4]) << 12
                          | (uint32_t)MY_TONUM(pszString[ 5]) <<  8
                          | (uint32_t)MY_TONUM(pszString[ 6]) <<  4
                          | (uint32_t)MY_TONUM(pszString[ 7]);
    pUuid->Gen.u16TimeMid = (uint16_t)MY_TONUM(pszString[ 9]) << 12
                          | (uint16_t)MY_TONUM(pszString[10]) << 8
                          | (uint16_t)MY_TONUM(pszString[11]) << 4
                          | (uint16_t)MY_TONUM(pszString[12]);
    pUuid->Gen.u16TimeHiAndVersion =
                            (uint16_t)MY_TONUM(pszString[14]) << 12
                          | (uint16_t)MY_TONUM(pszString[15]) << 8
                          | (uint16_t)MY_TONUM(pszString[16]) << 4
                          | (uint16_t)MY_TONUM(pszString[17]);
    pUuid->Gen.u16ClockSeq =(uint16_t)MY_TONUM(pszString[19]) << 4
                          | (uint16_t)MY_TONUM(pszString[20])
                          | (uint16_t)MY_TONUM(pszString[21]) << 12
                          | (uint16_t)MY_TONUM(pszString[22]) << 8;
    pUuid->Gen.au8Node[0] = (uint8_t)MY_TONUM(pszString[24]) << 4
                          | (uint8_t)MY_TONUM(pszString[25]);
    pUuid->Gen.au8Node[1] = (uint8_t)MY_TONUM(pszString[26]) << 4
                          | (uint8_t)MY_TONUM(pszString[27]);
    pUuid->Gen.au8Node[2] = (uint8_t)MY_TONUM(pszString[28]) << 4
                          | (uint8_t)MY_TONUM(pszString[29]);
    pUuid->Gen.au8Node[3] = (uint8_t)MY_TONUM(pszString[30]) << 4
                          | (uint8_t)MY_TONUM(pszString[31]);
    pUuid->Gen.au8Node[4] = (uint8_t)MY_TONUM(pszString[32]) << 4
                          | (uint8_t)MY_TONUM(pszString[33]);
    pUuid->Gen.au8Node[5] = (uint8_t)MY_TONUM(pszString[34]) << 4
                          | (uint8_t)MY_TONUM(pszString[35]);
#undef MY_TONUM
    return VINF_SUCCESS;
}

