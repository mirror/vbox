/* $Id$ */
/** @file
 * IPRT - String Formatter, Single Numbers.
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_STRING
#include <iprt/string.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include "internal/string.h"


RTDECL(ssize_t) RTStrFormatU8(char *pszBuf, size_t cbBuf, uint8_t u8Value, unsigned int uiBase,
                              signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    fFlags &= ~RTSTR_F_BIT_MASK;
    fFlags |= RTSTR_F_8BIT;

    ssize_t cchRet;
    if (cbBuf >= 64)
        cchRet = RTStrFormatNumber(pszBuf, u8Value, uiBase, cchWidth, cchPrecision, fFlags);
    else
    {
        char szTmp[64];
        cchRet = RTStrFormatNumber(szTmp, u8Value, uiBase, cchWidth, cchPrecision, fFlags);
        if ((size_t)cchRet < cbBuf)
            memcpy(pszBuf, szTmp, cchRet + 1);
        else
        {
            if (cbBuf)
            {
                memcpy(pszBuf, szTmp, cbBuf - 1);
                pszBuf[cbBuf - 1] = '\0';
            }
            cchRet = VERR_BUFFER_OVERFLOW;
        }
    }
    return cchRet;
}


RTDECL(ssize_t) RTStrFormatU16(char *pszBuf, size_t cbBuf, uint16_t u16Value, unsigned int uiBase,
                              signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    fFlags &= ~RTSTR_F_BIT_MASK;
    fFlags |= RTSTR_F_16BIT;

    ssize_t cchRet;
    if (cbBuf >= 64)
        cchRet = RTStrFormatNumber(pszBuf, u16Value, uiBase, cchWidth, cchPrecision, fFlags);
    else
    {
        char szTmp[64];
        cchRet = RTStrFormatNumber(szTmp, u16Value, uiBase, cchWidth, cchPrecision, fFlags);
        if ((size_t)cchRet < cbBuf)
            memcpy(pszBuf, szTmp, cchRet + 1);
        else
        {
            if (cbBuf)
            {
                memcpy(pszBuf, szTmp, cbBuf - 1);
                pszBuf[cbBuf - 1] = '\0';
            }
            cchRet = VERR_BUFFER_OVERFLOW;
        }
    }
    return cchRet;
}


RTDECL(ssize_t) RTStrFormatU32(char *pszBuf, size_t cbBuf, uint32_t u32Value, unsigned int uiBase,
                              signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    fFlags &= ~RTSTR_F_BIT_MASK;
    fFlags |= RTSTR_F_32BIT;

    ssize_t cchRet;
    if (cbBuf >= 64)
        cchRet = RTStrFormatNumber(pszBuf, u32Value, uiBase, cchWidth, cchPrecision, fFlags);
    else
    {
        char szTmp[64];
        cchRet = RTStrFormatNumber(szTmp, u32Value, uiBase, cchWidth, cchPrecision, fFlags);
        if ((size_t)cchRet < cbBuf)
            memcpy(pszBuf, szTmp, cchRet + 1);
        else
        {
            if (cbBuf)
            {
                memcpy(pszBuf, szTmp, cbBuf - 1);
                pszBuf[cbBuf - 1] = '\0';
            }
            cchRet = VERR_BUFFER_OVERFLOW;
        }
    }
    return cchRet;
}


RTDECL(ssize_t) RTStrFormatU64(char *pszBuf, size_t cbBuf, uint64_t u64Value, unsigned int uiBase,
                              signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    fFlags &= ~RTSTR_F_BIT_MASK;
    fFlags |= RTSTR_F_64BIT;

    ssize_t cchRet;
    if (cbBuf >= 64)
        cchRet = RTStrFormatNumber(pszBuf, u64Value, uiBase, cchWidth, cchPrecision, fFlags);
    else
    {
        char szTmp[64];
        cchRet = RTStrFormatNumber(szTmp, u64Value, uiBase, cchWidth, cchPrecision, fFlags);
        if ((size_t)cchRet < cbBuf)
            memcpy(pszBuf, szTmp, cchRet + 1);
        else
        {
            if (cbBuf)
            {
                memcpy(pszBuf, szTmp, cbBuf - 1);
                pszBuf[cbBuf - 1] = '\0';
            }
            cchRet = VERR_BUFFER_OVERFLOW;
        }
    }
    return cchRet;
}


RTDECL(ssize_t) RTStrFormatU128(char *pszBuf, size_t cbBuf, PCRTUINT128U pu128, unsigned int uiBase,
                                signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    NOREF(cchWidth); NOREF(cchPrecision);
    if (uiBase != 16)
        fFlags |= RTSTR_F_SPECIAL;
    fFlags &= ~RTSTR_F_BIT_MASK;

    char szTmp[64+32+32+32];
    char *pszTmp = cbBuf >= sizeof(szTmp) ? pszBuf : szTmp;
    size_t cchResult = RTStrFormatNumber(pszTmp, pu128->QWords.qw1, 16, 0, 0, fFlags | RTSTR_F_64BIT);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu128->QWords.qw0, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    if (pszTmp == pszBuf)
        return cchResult;
    int rc = RTStrCopy(pszBuf, cbBuf, pszTmp);
    if (RT_SUCCESS(rc))
        return cchResult;
    return rc;
}


RTDECL(ssize_t) RTStrFormatU256(char *pszBuf, size_t cbBuf, PCRTUINT256U pu256, unsigned int uiBase,
                                signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    NOREF(cchWidth); NOREF(cchPrecision);
    if (uiBase != 16)
        fFlags |= RTSTR_F_SPECIAL;
    fFlags &= ~RTSTR_F_BIT_MASK;

    char szTmp[64+32+32+32];
    char *pszTmp = cbBuf >= sizeof(szTmp) ? pszBuf : szTmp;
    size_t cchResult = RTStrFormatNumber(pszTmp, pu256->QWords.qw3, 16, 0, 0, fFlags | RTSTR_F_64BIT);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu256->QWords.qw2, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu256->QWords.qw1, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu256->QWords.qw0, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    if (pszTmp == pszBuf)
        return cchResult;
    int rc = RTStrCopy(pszBuf, cbBuf, pszTmp);
    if (RT_SUCCESS(rc))
        return cchResult;
    return rc;
}


RTDECL(ssize_t) RTStrFormatU512(char *pszBuf, size_t cbBuf, PCRTUINT512U pu512, unsigned int uiBase,
                                signed int cchWidth, signed int cchPrecision, uint32_t fFlags)
{
    NOREF(cchWidth); NOREF(cchPrecision);
    if (uiBase != 16)
        fFlags |= RTSTR_F_SPECIAL;
    fFlags &= ~RTSTR_F_BIT_MASK;

    char szTmp[64+32+32+32 + 32+32+32+32];
    char *pszTmp = cbBuf >= sizeof(szTmp) ? pszBuf : szTmp;
    size_t cchResult = RTStrFormatNumber(pszTmp, pu512->QWords.qw7, 16, 0, 0, fFlags | RTSTR_F_64BIT);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw6, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw5, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw4, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw3, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw2, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw1, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    cchResult += RTStrFormatNumber(&pszTmp[cchResult], pu512->QWords.qw0, 16, 8, 0,
                                   (fFlags | RTSTR_F_64BIT | RTSTR_F_ZEROPAD) & ~RTSTR_F_SPECIAL);
    if (pszTmp == pszBuf)
        return cchResult;
    int rc = RTStrCopy(pszBuf, cbBuf, pszTmp);
    if (RT_SUCCESS(rc))
        return cchResult;
    return rc;
}


/**
 * Helper for rtStrFormatR80Worker that copies out the resulting string.
 */
static ssize_t rtStrFormatR80CopyOutStr(char *pszBuf, size_t cbBuf, const char *pszSrc, size_t cchSrc)
{
    if (cchSrc < cbBuf)
    {
        memcpy(pszBuf, pszSrc, cchSrc);
        pszBuf[cchSrc] = '\0';
        return cchSrc;
    }
    if (cbBuf)
    {
        memcpy(pszBuf, pszSrc, cbBuf - 1);
        pszBuf[cbBuf - 1] = '\0';
    }
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Common worker for RTStrFormatR80 and RTStrFormatR80u2.
 */
static ssize_t rtStrFormatR80Worker(char *pszBuf, size_t cbBuf, bool const fSign, bool const fInteger,
                                    uint64_t const uFraction, uint16_t const uExponent, uint32_t fFlags)
{
    char szTmp[160];

    /*
     * Output sign first.
     */
    char *pszTmp = szTmp;
    if (fSign)
        *pszTmp++ = '-';
    else
        *pszTmp++ = '+';

    /*
     * Then check for special numbers (indicated by expontent).
     */
    bool fDenormal = false;
    if (uExponent == 0)
    {
        /* Zero? */
        if (   !uFraction
            && !fInteger)
            return fSign
                 ? rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("+0"))
                 : rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("-0"));
        fDenormal = true;
    }
    else if (uExponent == UINT16_C(0x7fff))
    {
        if (!fInteger)
        {
            if (!uFraction)
                return fSign
                     ? rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("+PseudoInf"))
                     : rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("-PseudoInf"));
            if (!(fFlags & RTSTR_F_SPECIAL))
                return fSign
                     ? rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("+PseudoNan"))
                     : rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("-PseudoNan"));
            pszTmp = (char *)memcpy(pszTmp, "PseudoNan[", 10) + 10;
        }
        else if (!(uFraction & RT_BIT_64(62)))
        {
            if (!(uFraction & (RT_BIT_64(62) - 1)))
                return fSign
                     ? rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("+Inf"))
                     : rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("-Inf"));
            if (!(fFlags & RTSTR_F_SPECIAL))
                return rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("Nan"));
            pszTmp = (char *)memcpy(pszTmp, "Nan[", 4) + 4;
        }
        else
        {
            if (!(uFraction & (RT_BIT_64(62) - 1)))
                return fSign
                     ? rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("+Ind"))
                     : rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("-Ind"));
            if (!(fFlags & RTSTR_F_SPECIAL))
                return rtStrFormatR80CopyOutStr(pszBuf, cbBuf, RT_STR_TUPLE("QNan"));
            pszTmp = (char *)memcpy(pszTmp, "QNan[", 5) + 5;
        }
        pszTmp += RTStrFormatNumber(pszTmp, uFraction, 16, 2 + 16, 0,
                                    RTSTR_F_SPECIAL | RTSTR_F_ZEROPAD | RTSTR_F_64BIT);
        *pszTmp++ = ']';
        return rtStrFormatR80CopyOutStr(pszBuf, cbBuf, szTmp, pszTmp - &szTmp[0]);
    }

    /*
     * Format the mantissa and exponent.
     */
    *pszTmp++ = fInteger ? '1' : '0';
    *pszTmp++ = 'm';
    pszTmp += RTStrFormatNumber(pszTmp, uFraction, 16, 2+16, 0,
                                RTSTR_F_SPECIAL | RTSTR_F_ZEROPAD | RTSTR_F_64BIT);

    *pszTmp++ = '^';
    pszTmp += RTStrFormatNumber(pszTmp, (int32_t)uExponent - 16383, 10, 0, 0,
                                RTSTR_F_ZEROPAD | RTSTR_F_32BIT | RTSTR_F_VALSIGNED);
    if (fDenormal && (fFlags & RTSTR_F_SPECIAL))
    {
        if (fInteger)
            pszTmp = (char *)memcpy(pszTmp, "[PDn]", 5) + 5;
        else
            pszTmp = (char *)memcpy(pszTmp, "[Den]", 5) + 5;
    }
    return rtStrFormatR80CopyOutStr(pszBuf, cbBuf, szTmp, pszTmp - &szTmp[0]);
}


RTDECL(ssize_t) RTStrFormatR80u2(char *pszBuf, size_t cbBuf, PCRTFLOAT80U2 pr80Value, signed int cchWidth,
                                 signed int cchPrecision, uint32_t fFlags)
{
    RT_NOREF(cchWidth, cchPrecision);
#ifdef RT_COMPILER_GROKS_64BIT_BITFIELDS
    return rtStrFormatR80Worker(pszBuf, cbBuf, pr80Value->sj64.fSign, pr80Value->sj64.fInteger,
                                pr80Value->sj64.uFraction, pr80Value->sj64.uExponent, fFlags);
#else
    return rtStrFormatR80Worker(pszBuf, cbBuf, pr80Value->sj.fSign, pr80Value->sj.fInteger,
                                RT_MAKE_U64(pr80Value->sj.u32FractionLow, pr80Value->sj.u31FractionHigh),
                                pr80Value->sj.uExponent, fFlags);
#endif
}


RTDECL(ssize_t) RTStrFormatR80(char *pszBuf, size_t cbBuf, PCRTFLOAT80U pr80Value, signed int cchWidth,
                               signed int cchPrecision, uint32_t fFlags)
{
    RT_NOREF(cchWidth, cchPrecision);
    return rtStrFormatR80Worker(pszBuf, cbBuf, pr80Value->s.fSign, pr80Value->s.uMantissa >> 63,
                                pr80Value->s.uMantissa & (RT_BIT_64(63) - 1), pr80Value->s.uExponent, fFlags);
}

