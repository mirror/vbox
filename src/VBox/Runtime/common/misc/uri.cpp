/* $Id$ */
/** @file
 * IPRT - Uniform Resource Identifier handling.
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

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/path.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Internal magic value we use to check if a RTURIPARSED structure has made it thru RTUriParse. */
#define RTURIPARSED_MAGIC   UINT32_C(0x439e0745)


/* General URI format:

    foo://example.com:8042/over/there?name=ferret#nose
    \_/   \______________/\_________/ \_________/ \__/
     |           |             |           |        |
  scheme     authority       path        query   fragment
     |   _____________________|__
    / \ /                        \
    urn:example:animal:ferret:nose
*/


/**
 * The following defines characters which have to be % escaped:
 *  control = 00-1F
 *  space   = ' '
 *  delims  = '<' , '>' , '#' , '%' , '"'
 *  unwise  = '{' , '}' , '|' , '\' , '^' , '[' , ']' , '`'
 */
#define URI_EXCLUDED(a) \
  (   ((a) >= 0x0  && (a) <= 0x20) \
   || ((a) >= 0x5B && (a) <= 0x5E) \
   || ((a) >= 0x7B && (a) <= 0x7D) \
   || (a) == '<' || (a) == '>' || (a) == '#' \
   || (a) == '%' || (a) == '"' || (a) == '`' )

static char *rtUriPercentEncodeN(const char *pszString, size_t cchMax)
{
    if (!pszString)
        return NULL;

    int rc = VINF_SUCCESS;

    size_t cbLen = RT_MIN(strlen(pszString), cchMax);
    /* The new string can be max 3 times in size of the original string. */
    char *pszNew = RTStrAlloc(cbLen * 3 + 1);
    if (!pszNew)
        return NULL;

    char *pszRes = NULL;
    size_t iIn = 0;
    size_t iOut = 0;
    while (iIn < cbLen)
    {
        if (URI_EXCLUDED(pszString[iIn]))
        {
            char szNum[3] = { 0, 0, 0 };
            RTStrFormatU8(&szNum[0], 3, pszString[iIn++], 16, 2, 2, RTSTR_F_CAPITAL | RTSTR_F_ZEROPAD);
            pszNew[iOut++] = '%';
            pszNew[iOut++] = szNum[0];
            pszNew[iOut++] = szNum[1];
        }
        else
            pszNew[iOut++] = pszString[iIn++];
    }
    if (RT_SUCCESS(rc))
    {
        pszNew[iOut] = '\0';
        if (iOut != iIn)
        {
            /* If the source and target strings have different size, recreate
             * the target string with the correct size. */
            pszRes = RTStrDupN(pszNew, iOut);
            RTStrFree(pszNew);
        }
        else
            pszRes = pszNew;
    }
    else
        RTStrFree(pszNew);

    return pszRes;
}


static char *rtUriPercentDecodeN(const char *pszString, size_t cchString)
{
    AssertPtrReturn(pszString, NULL);
    AssertReturn(memchr(pszString, '\0', cchString) == NULL, NULL);

    /*
     * The new string can only get smaller, so use the input length as a
     * staring buffer size.
     */
    char *pszDecoded = RTStrAlloc(cchString + 1);
    if (pszDecoded)
    {
        /*
         * Knowing that the pszString itself is valid UTF-8, we only have to
         * validate the escape sequences.
         */
        size_t      cchLeft = cchString;
        char const *pchSrc  = pszString;
        char       *pchDst  = pszDecoded;
        while (cchLeft > 0)
        {
            const char *pchPct = (const char *)memchr(pchSrc, '%', cchLeft);
            if (pchPct)
            {
                size_t cchBefore = pchPct - pchSrc;
                if (cchBefore)
                {
                    memcpy(pchDst, pchSrc, cchBefore);
                    pchDst  += cchBefore;
                    pchSrc  += cchBefore;
                    cchLeft -= cchBefore;
                }

                char chHigh, chLow;
                if (   cchLeft >= 3
                    && RT_C_IS_XDIGIT(chHigh = pchSrc[1])
                    && RT_C_IS_XDIGIT(chLow  = pchSrc[2]))
                {
                    uint8_t b = RT_C_IS_DIGIT(chHigh) ? chHigh - '0' : (chHigh & ~0x20) - 'A' + 10;
                    b <<= 4;
                    b |= RT_C_IS_DIGIT(chLow) ? chLow - '0' : (chLow & ~0x20) - 'A' + 10;
                    *pchDst++ = (char)b;
                    pchSrc  += 3;
                    cchLeft -= 3;
                }
                else
                {
                    AssertFailed();
                    *pchDst++ = *pchSrc++;
                    cchLeft--;
                }
            }
            else
            {
                memcpy(pchDst, pchSrc, cchLeft);
                pchDst += cchLeft;
                pchSrc += cchLeft;
                cchLeft = 0;
                break;
            }
        }

        *pchDst = '\0';

        /*
         * If we've got lof space room in the result string, reallocate it.
         */
        size_t cchDecoded = pchDst - pszDecoded;
        Assert(cchDecoded <= cchString);
        // if (cchString - cchDecoded > 64)  -  enable later!
            RTStrRealloc(&pszDecoded, cchDecoded + 1);
    }
    return pszDecoded;
}


static int rtUriParse(const char *pszUri, PRTURIPARSED pParsed)
{
    /*
     * Validate the input and clear the output.
     */
    AssertPtrReturn(pParsed, VERR_INVALID_POINTER);
    RT_ZERO(*pParsed);
    pParsed->uAuthorityPort = UINT32_MAX;

    AssertPtrReturn(pszUri, VERR_INVALID_POINTER);

    size_t const cchUri = strlen(pszUri);
    if (RT_LIKELY(cchUri >= 3)) { /* likely */ }
    else return cchUri ? VERR_URI_TOO_SHORT : VERR_URI_EMPTY;

    /*
     * Validating escaped text sequences is much simpler if we know that
     * that the base URI string is valid.  Also, we don't necessarily trust
     * the developer calling us to remember to do this.
     */
    int rc = RTStrValidateEncoding(pszUri);
    AssertRCReturn(rc, rc);

    /*
     * RFC-3986, section 3.1:
     *      scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
     *
     * The scheme ends with a ':', which we also skip here.
     */
    size_t off = 0;
    char ch = pszUri[off++];
    if (RT_LIKELY(RT_C_IS_ALPHA(ch))) { /* likely */ }
    else return VERR_URI_INVALID_SCHEME;
    for (;;)
    {
        ch = pszUri[off];
        if (ch == ':')
            break;
        if (RT_LIKELY(RT_C_IS_ALNUM(ch) || ch == '.' || ch == '-' || ch == '+')) { /* likely */ }
        else return VERR_URI_INVALID_SCHEME;
        off++;
    }
    pParsed->cchScheme = off;

    /* Require the scheme length to be at least two chars so we won't confuse
       it with a path starting with a DOS drive letter specification. */
    if (RT_LIKELY(off >= 2)) { /* likely */ }
    else return VERR_URI_INVALID_SCHEME;

    off++;                              /* (skip colon) */

    /*
     * Find the end of the path, we'll need this several times.
     * Also, while we're potentially scanning the whole thing, check for '%'.
     */
    size_t const offHash         = RTStrOffCharOrTerm(&pszUri[off], '#') + off;
    size_t const offQuestionMark = RTStrOffCharOrTerm(&pszUri[off], '?') + off;

    if (memchr(pszUri, '%', cchUri) != NULL)
        pParsed->fFlags |= RTURIPARSED_F_CONTAINS_ESCAPED_CHARS;

    /*
     * RFC-3986, section 3.2:
     *      The authority component is preceeded by a double slash ("//")...
     */
    if (   pszUri[off] == '/'
        && pszUri[off + 1] == '/')
    {
        off += 2;
        pParsed->offAuthority = pParsed->offAuthorityUsername = pParsed->offAuthorityPassword = pParsed->offAuthorityHost = off;
        pParsed->fFlags |= RTURIPARSED_F_HAVE_AUTHORITY;

        /*
         * RFC-3986, section 3.2:
         *      ...and is terminated by the next slash ("/"), question mark ("?"),
         *       or number sign ("#") character, or by the end of the URI.
         */
        const char *pszAuthority = &pszUri[off];
        size_t      cchAuthority = RTStrOffCharOrTerm(pszAuthority, '/');
        cchAuthority = RT_MIN(cchAuthority, offHash - off);
        cchAuthority = RT_MIN(cchAuthority, offQuestionMark - off);
        pParsed->cchAuthority     = cchAuthority;

        /* The Authority can be empty, like for: file:///usr/bin/grep  */
        if (cchAuthority > 0)
        {
            pParsed->cchAuthorityHost = cchAuthority;

            /*
             * If there is a userinfo part, it is ended by a '@'.
             */
            const char *pszAt = (const char *)memchr(pszAuthority, '@', cchAuthority);
            if (pszAt)
            {
                size_t cchTmp = pszAt - pszAuthority;
                pParsed->offAuthorityHost += cchTmp + 1;
                pParsed->cchAuthorityHost -= cchTmp + 1;

                /* If there is a password part, it's separated from the username with a colon. */
                const char *pszColon = (const char *)memchr(pszAuthority, ':', cchTmp);
                if (pszColon)
                {
                    pParsed->cchAuthorityUsername = pszColon - pszAuthority;
                    pParsed->offAuthorityPassword = &pszColon[1] - pszUri;
                    pParsed->cchAuthorityPassword = pszAt - &pszColon[1];
                }
                else
                {
                    pParsed->cchAuthorityUsername = cchTmp;
                    pParsed->offAuthorityPassword = off + cchTmp;
                }
            }

            /*
             * If there is a port part, its after the last colon in the host part.
             */
            const char *pszColon = (const char *)memrchr(&pszUri[pParsed->offAuthorityHost], ':', pParsed->cchAuthorityHost);
            if (pszColon)
            {
                size_t cchTmp = &pszUri[pParsed->offAuthorityHost + pParsed->cchAuthorityHost] - &pszColon[1];
                pParsed->cchAuthorityHost -= cchTmp + 1;

                pParsed->uAuthorityPort = 0;
                while (cchTmp-- > 0)
                {
                    ch = *++pszColon;
                    if (   RT_C_IS_DIGIT(ch)
                        && pParsed->uAuthorityPort < UINT32_MAX / UINT32_C(10))
                    {
                        pParsed->uAuthorityPort *= 10;
                        pParsed->uAuthorityPort += ch - '0';
                    }
                    else
                        return VERR_URI_INVALID_PORT_NUMBER;
                }
            }
        }

        /* Skip past the authority. */
        off += cchAuthority;
    }
    else
        pParsed->offAuthority = pParsed->offAuthorityUsername = pParsed->offAuthorityPassword = pParsed->offAuthorityHost = off;

    /*
     * RFC-3986, section 3.3: Path
     *      The path is terminated by the first question mark ("?")
     *      or number sign ("#") character, or by the end of the URI.
     */
    pParsed->offPath = off;
    pParsed->cchPath = RT_MIN(offHash, offQuestionMark) - off;
    off += pParsed->cchPath;

    /*
     * RFC-3986, section 3.4: Query
     *      The query component is indicated by the first question mark ("?")
     *      character and terminated by a number sign ("#") character or by the
     *      end of the URI.
     */
    if (   off == offQuestionMark
        && off < cchUri)
    {
        Assert(pszUri[offQuestionMark] == '?');
        pParsed->offQuery = ++off;
        pParsed->cchQuery = offHash - off;
        off = offHash;
    }
    else
    {
        Assert(!pszUri[offQuestionMark]);
        pParsed->offQuery = off;
    }

    /*
     * RFC-3986, section 3.5: Fragment
     *      A fragment identifier component is indicated by the presence of a
     *      number sign ("#") character and terminated by the end of the URI.
     */
    if (   off == offHash
        && off < cchUri)
    {
        pParsed->offFragment = ++off;
        pParsed->cchFragment = cchUri - off;
    }
    else
    {
        Assert(!pszUri[offHash]);
        pParsed->offFragment = off;
    }

    /*
     * If there are any escape sequences, validate them.
     *
     * This is reasonably simple as we already know that the string is valid UTF-8
     * before they get decoded.  Thus we only have to validate the escaped sequences.
     */
    if (pParsed->fFlags & RTURIPARSED_F_CONTAINS_ESCAPED_CHARS)
    {
        const char *pchSrc  = (const char *)memchr(pszUri, '%', cchUri);
        AssertReturn(pchSrc, VERR_INTERNAL_ERROR);
        do
        {
            char        szUtf8Seq[8];
            unsigned    cchUtf8Seq = 0;
            unsigned    cchNeeded  = 0;
            size_t      cchLeft    = &pszUri[cchUri] - pchSrc;
            do
            {
                if (cchLeft >= 3)
                {
                    char chHigh = pchSrc[1];
                    char chLow  = pchSrc[2];
                    if (   RT_C_IS_XDIGIT(chHigh)
                        && RT_C_IS_XDIGIT(chLow))
                    {
                        uint8_t b = RT_C_IS_DIGIT(chHigh) ? chHigh - '0' : (chHigh & ~0x20) - 'A' + 10;
                        b <<= 4;
                        b |= RT_C_IS_DIGIT(chLow) ? chLow - '0' : (chLow & ~0x20) - 'A' + 10;

                        if (!(b & 0x80))
                        {
                            /* We don't want the string to be terminated prematurely. */
                            if (RT_LIKELY(b != 0)) { /* likely */ }
                            else return VERR_URI_ESCAPED_ZERO;

                            /* Check that we're not expecting more UTF-8 bytes. */
                            if (RT_LIKELY(cchNeeded == 0)) { /* likely */ }
                            else return VERR_URI_MISSING_UTF8_CONTINUATION_BYTE;
                        }
                        /* Are we waiting UTF-8 bytes? */
                        else if (cchNeeded > 0)
                        {
                            if (RT_LIKELY(!(b & 0x40))) { /* likely */ }
                            else return VERR_URI_INVALID_ESCAPED_UTF8_CONTINUATION_BYTE;

                            szUtf8Seq[cchUtf8Seq++] = (char)b;
                            if (--cchNeeded == 0)
                            {
                                szUtf8Seq[cchUtf8Seq] = '\0';
                                rc = RTStrValidateEncoding(szUtf8Seq);
                                if (RT_FAILURE(rc))
                                    return VERR_URI_ESCAPED_CHARS_NOT_VALID_UTF8;
                                cchUtf8Seq = 0;
                            }
                        }
                        /* Start a new UTF-8 sequence. */
                        else
                        {
                            if ((b & 0xf8) == 0xf0)
                                cchNeeded = 3;
                            else if ((b & 0xf0) == 0xe0)
                                cchNeeded = 2;
                            else if ((b & 0xe0) == 0xc0)
                                cchNeeded = 1;
                            else
                                return VERR_URI_INVALID_ESCAPED_UTF8_LEAD_BYTE;
                            szUtf8Seq[0] = (char)b;
                            cchUtf8Seq = 1;
                        }
                        pchSrc  += 3;
                        cchLeft -= 3;
                    }
                    else
                        return VERR_URI_INVALID_ESCAPE_SEQ;
                }
                else
                    return VERR_URI_INVALID_ESCAPE_SEQ;
            } while (cchLeft > 0 && pchSrc[0] == '%');

            /* Check that we're not expecting more UTF-8 bytes. */
            if (RT_LIKELY(cchNeeded == 0)) { /* likely */ }
            else return VERR_URI_MISSING_UTF8_CONTINUATION_BYTE;

            /* next */
            pchSrc = (const char *)memchr(pchSrc, '%', cchLeft);
        } while (pchSrc);
    }

    pParsed->u32Magic = RTURIPARSED_MAGIC;
    return VINF_SUCCESS;
}


RTDECL(int) RTUriParse(const char *pszUri, PRTURIPARSED pParsed)
{
    return rtUriParse(pszUri, pParsed);
}


RTDECL(char *) RTUriParsedScheme(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    return RTStrDupN(pszUri, pParsed->cchScheme);
}


RTDECL(char *) RTUriParsedAuthority(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchAuthority || (pParsed->fFlags & RTURIPARSED_F_HAVE_AUTHORITY))
        return rtUriPercentDecodeN(&pszUri[pParsed->offAuthority], pParsed->cchAuthority);
    return NULL;
}


RTDECL(char *) RTUriParsedAuthorityUsername(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchAuthorityUsername)
        return rtUriPercentDecodeN(&pszUri[pParsed->offAuthorityUsername], pParsed->cchAuthorityUsername);
    return NULL;
}


RTDECL(char *) RTUriParsedAuthorityPassword(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchAuthorityPassword)
        return rtUriPercentDecodeN(&pszUri[pParsed->offAuthorityPassword], pParsed->cchAuthorityPassword);
    return NULL;
}


RTDECL(char *) RTUriParsedAuthorityHost(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchAuthorityHost)
        return rtUriPercentDecodeN(&pszUri[pParsed->offAuthorityHost], pParsed->cchAuthorityHost);
    return NULL;
}


RTDECL(uint32_t) RTUriParsedAuthorityPort(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, UINT32_MAX);
    AssertPtrReturn(pParsed, UINT32_MAX);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, UINT32_MAX);
    return pParsed->uAuthorityPort;
}


RTDECL(char *) RTUriParsedPath(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchPath)
        return rtUriPercentDecodeN(&pszUri[pParsed->offPath], pParsed->cchPath);
    return NULL;
}


RTDECL(char *) RTUriParsedQuery(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchQuery)
        return rtUriPercentDecodeN(&pszUri[pParsed->offQuery], pParsed->cchQuery);
    return NULL;
}


RTDECL(char *) RTUriParsedFragment(const char *pszUri, PCRTURIPARSED pParsed)
{
    AssertPtrReturn(pszUri, NULL);
    AssertPtrReturn(pParsed, NULL);
    AssertReturn(pParsed->u32Magic == RTURIPARSED_MAGIC, NULL);
    if (pParsed->cchFragment)
        return rtUriPercentDecodeN(&pszUri[pParsed->offFragment], pParsed->cchFragment);
    return NULL;
}


RTDECL(char *) RTUriCreate(const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery,
                           const char *pszFragment)
{
    if (!pszScheme) /* Scheme is minimum requirement */
        return NULL;

    char *pszResult = 0;
    char *pszAuthority1 = 0;
    char *pszPath1 = 0;
    char *pszQuery1 = 0;
    char *pszFragment1 = 0;

    do
    {
        /* Create the percent encoded strings and calculate the necessary uri
         * length. */
        size_t cbSize = strlen(pszScheme) + 1 + 1; /* plus zero byte */
        if (pszAuthority)
        {
            pszAuthority1 = rtUriPercentEncodeN(pszAuthority, RTSTR_MAX);
            if (!pszAuthority1)
                break;
            cbSize += strlen(pszAuthority1) + 2;
        }
        if (pszPath)
        {
            pszPath1 = rtUriPercentEncodeN(pszPath, RTSTR_MAX);
            if (!pszPath1)
                break;
            cbSize += strlen(pszPath1);
        }
        if (pszQuery)
        {
            pszQuery1 = rtUriPercentEncodeN(pszQuery, RTSTR_MAX);
            if (!pszQuery1)
                break;
            cbSize += strlen(pszQuery1) + 1;
        }
        if (pszFragment)
        {
            pszFragment1 = rtUriPercentEncodeN(pszFragment, RTSTR_MAX);
            if (!pszFragment1)
                break;
            cbSize += strlen(pszFragment1) + 1;
        }

        char *pszTmp = pszResult = (char *)RTStrAlloc(cbSize);
        if (!pszResult)
            break;
        RT_BZERO(pszTmp, cbSize);

        /* Compose the target uri string. */
        RTStrCatP(&pszTmp, &cbSize, pszScheme);
        RTStrCatP(&pszTmp, &cbSize, ":");
        if (pszAuthority1)
        {
            RTStrCatP(&pszTmp, &cbSize, "//");
            RTStrCatP(&pszTmp, &cbSize, pszAuthority1);
        }
        if (pszPath1)
        {
            RTStrCatP(&pszTmp, &cbSize, pszPath1);
        }
        if (pszQuery1)
        {
            RTStrCatP(&pszTmp, &cbSize, "?");
            RTStrCatP(&pszTmp, &cbSize, pszQuery1);
        }
        if (pszFragment1)
        {
            RTStrCatP(&pszTmp, &cbSize, "#");
            RTStrCatP(&pszTmp, &cbSize, pszFragment1);
        }
    } while (0);

    /* Cleanup */
    if (pszAuthority1)
        RTStrFree(pszAuthority1);
    if (pszPath1)
        RTStrFree(pszPath1);
    if (pszQuery1)
        RTStrFree(pszQuery1);
    if (pszFragment1)
        RTStrFree(pszFragment1);

    return pszResult;
}


RTDECL(bool)   RTUriIsSchemeMatch(const char *pszUri, const char *pszScheme)
{
    AssertPtrReturn(pszUri, false);
    size_t const cchScheme = strlen(pszScheme);
    return RTStrNICmp(pszUri, pszScheme, cchScheme) == 0
        && pszUri[cchScheme] == ':';
}


RTDECL(char *) RTUriFileCreate(const char *pszPath)
{
    char *pszResult = NULL;
    if (pszPath)
    {
        /* Check if it's an UNC path. Skip any leading slashes. */
        while (pszPath)
        {
            if (   *pszPath != '\\'
                && *pszPath != '/')
                break;
            pszPath++;
        }

        /* Create the percent encoded strings and calculate the necessary URI length. */
        char *pszPath1 = rtUriPercentEncodeN(pszPath, RTSTR_MAX);
        if (pszPath1)
        {
            /* Always change DOS slashes to Unix slashes. */
            RTPathChangeToUnixSlashes(pszPath1, true); /** @todo Flags? */

            size_t cbSize = 7 /* file:// */ + strlen(pszPath1) + 1; /* plus zero byte */
            if (pszPath1[0] != '/')
                ++cbSize;
            char *pszTmp = pszResult = RTStrAlloc(cbSize);
            if (pszResult)
            {
                /* Compose the target URI string. */
                *pszTmp = '\0';
                RTStrCatP(&pszTmp, &cbSize, "file://");
                if (pszPath1[0] != '/')
                    RTStrCatP(&pszTmp, &cbSize, "/");
                RTStrCatP(&pszTmp, &cbSize, pszPath1);
            }
            RTStrFree(pszPath1);
        }
    }
    else
    {
        char *pszResTmp;
        int cchRes = RTStrAPrintf(&pszResTmp, "file://");
        if (cchRes)
            pszResult = pszResTmp;
    }

    return pszResult;
}


RTDECL(char *) RTUriFilePath(const char *pszUri, uint32_t uFormat)
{
    return RTUriFileNPath(pszUri, uFormat, RTSTR_MAX);
}


RTDECL(char *) RTUriFileNPath(const char *pszUri, uint32_t uFormat, size_t cchMax)
{
    AssertPtrReturn(pszUri, NULL);
    AssertReturn(uFormat == URI_FILE_FORMAT_AUTO || uFormat == URI_FILE_FORMAT_UNIX || uFormat == URI_FILE_FORMAT_WIN, NULL);

    /* Auto is based on the current OS. */
    if (uFormat == URI_FILE_FORMAT_AUTO)
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        uFormat = URI_FILE_FORMAT_WIN;
#else
        uFormat = URI_FILE_FORMAT_UNIX;
#endif

    /* Check that this is a file URI. */
    if (RTStrNICmp(pszUri, RT_STR_TUPLE("file:")) != 0)
        return NULL;

    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
    {
        /* No path detected? Take authority as path then. */
        if (!Parsed.cchPath)
        {
            Parsed.cchPath      = Parsed.cchAuthority;
            Parsed.offPath      = Parsed.offAuthority;
            Parsed.cchAuthority = 0;
        }
    }

    if (   RT_SUCCESS(rc)
        && Parsed.cchPath)
    {
        /*
         * Calculate the size of the encoded result.
         */
        size_t cbResult = 0;

        /* Skip the leading slash if a DOS drive letter (e.g. "C:") is detected right after it. */
        if (   Parsed.cchPath >= 3
            && pszUri[Parsed.offPath]  == '/'        /* Leading slash. */
            && RT_C_IS_ALPHA(pszUri[Parsed.offPath + 1]) /* Drive letter. */
            && pszUri[Parsed.offPath + 2]  == ':')
        {
            Parsed.offPath++;
            Parsed.cchPath--;
        }

        /* Windows: Authority given? Include authority as part of UNC path */
        if (uFormat == URI_FILE_FORMAT_WIN && Parsed.cchAuthority)
        {
            cbResult += 2; /* UNC slashes "\\". */
            cbResult += Parsed.cchAuthority;
        }

        cbResult += Parsed.cchPath;
        cbResult += 1; /* Zero termination. */

        /*
         * Compose encoded string.
         */
        char *pszResult;
        char *pszTmp = pszResult = RTStrAlloc(cbResult);
        if (pszTmp)
        {
            size_t cbTmp = cbResult;

            /* Windows: If an authority is given, add the required UNC prefix. */
            if (uFormat == URI_FILE_FORMAT_WIN && Parsed.cchAuthority)
            {
                rc = RTStrCatP(&pszTmp, &cbTmp, "\\\\");
                if (RT_SUCCESS(rc))
                    rc = RTStrCatPEx(&pszTmp, &cbTmp, &pszUri[Parsed.offAuthority], Parsed.cchAuthority);
            }
            if (RT_SUCCESS(rc))
                rc = RTStrCatPEx(&pszTmp, &cbTmp, &pszUri[Parsed.offPath], Parsed.cchPath);
            AssertRC(rc); /* Shall not happen! */
            if (RT_SUCCESS(rc))
            {
                /*
                 * Decode the string and switch the slashes around the request way before returning.
                 */
                char *pszPath = rtUriPercentDecodeN(pszResult, cbResult - 1 /* Minus termination */);
                if (pszPath)
                {
                    RTStrFree(pszResult);

                    if (uFormat == URI_FILE_FORMAT_UNIX)
                        return RTPathChangeToUnixSlashes(pszPath, true);
                    Assert(uFormat == URI_FILE_FORMAT_WIN);
                    return RTPathChangeToDosSlashes(pszPath, true);
                }

                /* Failed. */
            }
            RTStrFree(pszResult);
        }
    }
    return NULL;
}

