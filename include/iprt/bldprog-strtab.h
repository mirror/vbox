/** @file
 * IPRT - Build Program - String Table Generator, Accessors.
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

#ifndef IPRT_INCLUDED_bldprog_strtab_h
#define IPRT_INCLUDED_bldprog_strtab_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>


/**
 * The default build program string table reference.
 */
typedef struct RTBLDPROGSTRREF
{
    /** Offset of the string in the string table. */
    uint32_t off : 22;
    /** The length of the string. */
    uint32_t cch : 10;
} RTBLDPROGSTRREF;
AssertCompileSize(RTBLDPROGSTRREF, sizeof(uint32_t));
/** Pointer to a build program string table reference. */
typedef RTBLDPROGSTRREF const *PCRTBLDPROGSTRREF;


typedef struct RTBLDPROGSTRTAB
{
    const char         *pchStrTab;
    uint32_t            cchStrTab;
    uint8_t             cCompDict;
    PCRTBLDPROGSTRREF   paCompDict;
} RTBLDPROGSTRTAB;
typedef const RTBLDPROGSTRTAB *PCRTBLDPROGSTRTAB;


/**
 * Tries to ensure the buffer is terminated when failing.
 */
DECLINLINE(ssize_t) RTBldProgStrTabQueryStringFail(int rc, char *pszDstStart, char *pszDst, size_t cbDst)
{
    if (cbDst)
        *pszDst = '\0';
    else if (pszDstStart != pszDst)
        pszDst[-1] = '\0';
    return rc;
}


/**
 * Retrieves the decompressed string.
 *
 * @returns The string size on success, IPRT status code on failure.
 * @param   pStrTab         The string table.
 * @param   offString       The offset of the string.
 * @param   cchString       The length of the string.
 * @param   pszDst          The return buffer.
 * @param   cbDst           The size of the return buffer.
 */
DECLINLINE(ssize_t) RTBldProgStrTabQueryString(PCRTBLDPROGSTRTAB pStrTab, uint32_t offString, size_t cchString,
                                               char *pszDst, size_t cbDst)
{
    AssertReturn(offString < pStrTab->cchStrTab, VERR_OUT_OF_RANGE);
    AssertReturn(offString + cchString <= pStrTab->cchStrTab, VERR_OUT_OF_RANGE);

    if (pStrTab->cCompDict)
    {
        /*
         * Could be compressed, decompress it.
         */
        char * const pchDstStart = pszDst;
        const char  *pchSrc = &pStrTab->pchStrTab[offString];
        while (cchString-- > 0)
        {
            unsigned char uch = *pchSrc++;
            if (!(uch & 0x80))
            {
                /*
                 * Plain text.
                 */
                AssertReturn(cbDst > 1, RTBldProgStrTabQueryStringFail(VERR_BUFFER_OVERFLOW, pchDstStart, pszDst, cbDst));
                cbDst    -= 1;
                *pszDst++ = (char)uch;
                Assert(uch != 0);
            }
            else if (uch != 0xff)
            {
                /*
                 * Dictionary reference. (No UTF-8 unescaping necessary here.)
                 */
                PCRTBLDPROGSTRREF   pWord   = &pStrTab->paCompDict[uch & 0x7f];
                size_t const        cchWord = pWord->cch;
                AssertReturn((size_t)pWord->off + cchWord <= pStrTab->cchStrTab,
                             RTBldProgStrTabQueryStringFail(VERR_INVALID_PARAMETER, pchDstStart, pszDst, cbDst));
                AssertReturn(cbDst > cchWord,
                             RTBldProgStrTabQueryStringFail(VERR_BUFFER_OVERFLOW, pchDstStart, pszDst, cbDst));
                memcpy(pszDst, &pStrTab->pchStrTab[pWord->off], cchWord);
                pszDst += cchWord;
                cbDst  -= cchWord;
            }
            else
            {
                /*
                 * UTF-8 encoded unicode codepoint.
                 */
                size_t  cchCp;
                RTUNICP uc = ' ';
                int rc = RTStrGetCpNEx(&pchSrc, &cchString, &uc);
                AssertStmt(RT_SUCCESS(rc), (uc = '?', pchSrc++, cchString--));

                cchCp = RTStrCpSize(uc);
                AssertReturn(cbDst > cchCp,
                             RTBldProgStrTabQueryStringFail(VERR_BUFFER_OVERFLOW, pchDstStart, pszDst, cbDst));

                RTStrPutCp(pszDst, uc);
                pszDst += cchCp;
                cbDst  -= cchCp;
            }
        }
        AssertReturn(cbDst > 0, RTBldProgStrTabQueryStringFail(VERR_BUFFER_OVERFLOW, pchDstStart, pszDst, cbDst));
        *pszDst = '\0';
        return pszDst - pchDstStart;
    }

    /*
     * Not compressed.
     */
    if (cbDst > cchString)
    {
        memcpy(pszDst, &pStrTab->pchStrTab[offString], cchString);
        pszDst[cchString] = '\0';
        return (ssize_t)cchString;
    }
    if (cbDst > 0)
    {
        memcpy(pszDst, &pStrTab->pchStrTab[offString], cbDst - 1);
        pszDst[cbDst - 1] = '\0';
    }
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Outputs the decompressed string.
 *
 * @returns The sum of the pfnOutput return values.
 * @param   pStrTab         The string table.
 * @param   offString       The offset of the string.
 * @param   cchString       The length of the string.
 * @param   pfnOutput       The output function.
 * @param   pvArgOutput     The argument to pass to the output function.
 *
 */
DECLINLINE(size_t) RTBldProgStrTabQueryOutput(PCRTBLDPROGSTRTAB pStrTab, uint32_t offString, size_t cchString,
                                              PFNRTSTROUTPUT pfnOutput, void *pvArgOutput)
{
    AssertReturn(offString < pStrTab->cchStrTab, 0);
    AssertReturn(offString + cchString <= pStrTab->cchStrTab, 0);

    if (pStrTab->cCompDict)
    {
        /*
         * Could be compressed, decompress it.
         */
        size_t      cchRet = 0;
        const char *pchSrc = &pStrTab->pchStrTab[offString];
        while (cchString-- > 0)
        {
            unsigned char uch = *pchSrc++;
            if (!(uch & 0x80))
            {
                /*
                 * Plain text.
                 */
                Assert(uch != 0);
                cchRet += pfnOutput(pvArgOutput, (const char *)&uch, 1);
            }
            else if (uch != 0xff)
            {
                /*
                 * Dictionary reference. (No UTF-8 unescaping necessary here.)
                 */
                PCRTBLDPROGSTRREF   pWord   = &pStrTab->paCompDict[uch & 0x7f];
                size_t const        cchWord = pWord->cch;
                AssertReturn((size_t)pWord->off + cchWord <= pStrTab->cchStrTab, cchRet);

                cchRet += pfnOutput(pvArgOutput, &pStrTab->pchStrTab[pWord->off], cchWord);
            }
            else
            {
                /*
                 * UTF-8 encoded unicode codepoint.
                 */
                const char * const pchUtf8Seq = pchSrc;
                RTUNICP uc = ' ';
                int rc = RTStrGetCpNEx(&pchSrc, &cchString, &uc);
                if (RT_SUCCESS(rc))
                    cchRet += pfnOutput(pvArgOutput, pchUtf8Seq, pchSrc - pchUtf8Seq);
                else
                    cchRet += pfnOutput(pvArgOutput, "?", 1);
            }
        }
        return cchRet;
    }

    /*
     * Not compressed.
     */
    return pfnOutput(pvArgOutput, &pStrTab->pchStrTab[offString], cchString);
}


#endif /* !IPRT_INCLUDED_bldprog_strtab_h */

