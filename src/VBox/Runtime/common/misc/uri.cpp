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

/* General URI format:

    foo://example.com:8042/over/there?name=ferret#nose
    \_/   \______________/\_________/ \_________/ \__/
     |           |            |            |        |
  scheme     authority       path        query   fragment
     |   _____________________|__
    / \ /                        \
    urn:example:animal:ferret:nose
*/

/**
 * Parsed URI.
 */
typedef struct RTURIPARSED
{
    /** RTURIPARSED_F_XXX. */
    uint32_t    fFlags;

    /** The length of the scheme. */
    size_t      cchScheme;

    /** The offset into the string of the authority. */
    size_t      offAuthority;
    /** The authority length.
     * @remarks The authority component can be zero length, so to check whether
     *          it's there or not consult RTURIPARSED_F_HAVE_AUTHORITY. */
    size_t      cchAuthority;

    /** The offset into the string of the path. */
    size_t      offPath;
    /** The length of the path. */
    size_t      cchPath;

    /** The offset into the string of the query. */
    size_t      offQuery;
    /** The length of the query. */
    size_t      cchQuery;

    /** The offset into the string of the fragment. */
    size_t      offFragment;
    /** The length of the fragment. */
    size_t      cchFragment;

    /** @name Authority subdivisions
     * @{ */
    /** If there is a userinfo part, this is the start of it. Otherwise it's the
     * same as offAuthorityHost. */
    size_t      offAuthorityUsername;
    /** The length of the username (zero if not present). */
    size_t      cchAuthorityUsername;
    /** If there is a userinfo part containing a password, this is the start of it.
     * Otherwise it's the same as offAuthorityHost. */
    size_t      offAuthorityPassword;
    /** The length of the password (zero if not present). */
    size_t      cchAuthorityPassword;
    /** The offset of the host part of the authority. */
    size_t      offAuthorityHost;
    /** The length of the host part of the authority. */
    size_t      cchAuthorityHost;
    /** The authority port number, UINT32_MAX if not present. */
    uint32_t    uAuthorityPort;
    /** @} */
} RTURIPARSED;
/** Pointer to a parsed URI. */
typedef RTURIPARSED *PRTURIPARSED;
/** Set if the URI contains escaped characters. */
#define RTURIPARSED_F_CONTAINS_ESCAPED_CHARS        UINT32_C(0x00000001)
/** Set if the URI have an authority component.  Necessary since the authority
 * component can have a zero length. */
#define RTURIPARSED_F_HAVE_AUTHORITY                UINT32_C(0x00000002)



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

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

static char *rtUriPercentDecodeN(const char *pszString, size_t cchMax)
{
    if (!pszString)
        return NULL;

    /* The new string can only get smaller. */
    size_t cchLen = strlen(pszString);
    cchLen = RT_MIN(cchLen, cchMax);
    char *pszNew = RTStrAlloc(cchLen + 1);
    if (!pszNew)
        return NULL;

    int rc = VINF_SUCCESS;
    char *pszRes = NULL;
    size_t iIn = 0;
    size_t iOut = 0;
    while (iIn < cchLen)
    {
        if (pszString[iIn] == '%')
        {
            /* % encoding means the percent sign and exactly 2 hexadecimal
             * digits describing the ASCII number of the character. */
            ++iIn;
            char szNum[3];
            szNum[0] = pszString[iIn++];
            szNum[1] = pszString[iIn++];
            szNum[2] = '\0';

            uint8_t u8;
            rc = RTStrToUInt8Ex(szNum, NULL, 16, &u8);
            if (RT_FAILURE(rc))
                break;
            pszNew[iOut] = u8;
        }
        else
            pszNew[iOut] = pszString[iIn++];
        ++iOut;
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

static bool rtUriFindSchemeEnd(const char *pszUri, size_t iStart, size_t cbLen, size_t *piEnd)
{
    /* The scheme has to end with ':'. */
    size_t i = iStart;
    while (i < iStart + cbLen)
    {
        if (pszUri[i] == ':')
        {
            *piEnd = i;
            return true;
        }
        ++i;
    }
    return false;
}

static bool rtUriCheckAuthorityStart(const char *pszUri, size_t iStart, size_t cbLen, size_t *piStart)
{
    /* The authority have to start with '//' */
    if (   cbLen >= 2
        && pszUri[iStart    ] == '/'
        && pszUri[iStart + 1] == '/')
    {
        *piStart = iStart + 2;
        return true;
    }

    return false;
}

static bool rtUriFindAuthorityEnd(const char *pszUri, size_t iStart, size_t cbLen, size_t *piEnd)
{
    /* The authority can end with '/' || '?' || '#'. */
    size_t i = iStart;
    while (i < iStart + cbLen)
    {
        if (   pszUri[i] == '/'
            || pszUri[i] == '?'
            || pszUri[i] == '#')
        {
            *piEnd = i;
            return true;
        }
        ++i;
    }
    return false;
}

static bool rtUriCheckPathStart(const char *pszUri, size_t iStart, size_t cbLen, size_t *piStart)
{
    /* The path could start with a '/'. */
    if (   cbLen >= 1
        && pszUri[iStart] == '/')
    {
        *piStart = iStart; /* Including '/' */
        return true;
    }

    /* '?' || '#' means there is no path. */
    if (   cbLen >= 1
        && (   pszUri[iStart] == '?'
            || pszUri[iStart] == '#'))
        return false;

    /* All other values are allowed. */
    *piStart = iStart;
    return true;
}

static bool rtUriFindPathEnd(const char *pszUri, size_t iStart, size_t cbLen, size_t *piEnd)
{
    /* The path can end with '?' || '#'. */
    size_t i = iStart;
    while (i < iStart + cbLen)
    {
        if (   pszUri[i] == '?'
            || pszUri[i] == '#')
        {
            *piEnd = i;
            return true;
        }
        ++i;
    }
    return false;
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

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Generic URI APIs                                                                                                             *
*********************************************************************************************************************************/

RTR3DECL(char *) RTUriCreate(const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery,
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

RTR3DECL(bool)   RTUriHasScheme(const char *pszUri, const char *pszScheme)
{
    bool fRes = false;
    char *pszTmp = RTUriScheme(pszUri);
    if (pszTmp)
    {
        fRes = RTStrNICmp(pszScheme, pszTmp, strlen(pszTmp)) == 0;
        RTStrFree(pszTmp);
    }
    return fRes;
}

RTR3DECL(char *) RTUriScheme(const char *pszUri)
{
    AssertPtrReturn(pszUri, NULL);

    size_t iPos1;
    size_t cbLen = strlen(pszUri);
    if (rtUriFindSchemeEnd(pszUri, 0, cbLen, &iPos1))
        return rtUriPercentDecodeN(pszUri, iPos1);
    return NULL;
}

RTR3DECL(char *) RTUriAuthority(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        if (Parsed.cchAuthority || (Parsed.fFlags & RTURIPARSED_F_HAVE_AUTHORITY))
            return rtUriPercentDecodeN(&pszUri[Parsed.offAuthority], Parsed.cchAuthority);
    return NULL;
}

RTR3DECL(char *) RTUriAuthorityUsername(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        if (Parsed.cchAuthorityUsername)
            return rtUriPercentDecodeN(&pszUri[Parsed.offAuthorityUsername], Parsed.cchAuthorityUsername);
    return NULL;
}

RTR3DECL(char *) RTUriAuthorityPassword(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        if (Parsed.cchAuthorityPassword)
            return rtUriPercentDecodeN(&pszUri[Parsed.offAuthorityPassword], Parsed.cchAuthorityPassword);
    return NULL;
}

RTR3DECL(uint32_t) RTUriAuthorityPort(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        return Parsed.uAuthorityPort;
    return UINT32_MAX;
}

RTR3DECL(char *) RTUriPath(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        if (Parsed.cchPath)
            return rtUriPercentDecodeN(&pszUri[Parsed.offPath], Parsed.cchPath);
    return NULL;
}

RTR3DECL(char *) RTUriQuery(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        if (Parsed.cchQuery)
            return rtUriPercentDecodeN(&pszUri[Parsed.offQuery], Parsed.cchQuery);
    return NULL;
}

RTR3DECL(char *) RTUriFragment(const char *pszUri)
{
    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc))
        if (Parsed.cchFragment)
            return rtUriPercentDecodeN(&pszUri[Parsed.offFragment], Parsed.cchFragment);
    return NULL;
}


/*********************************************************************************************************************************
*   File URI APIs                                                                                                                *
*********************************************************************************************************************************/

RTR3DECL(char *) RTUriFileCreate(const char *pszPath)
{
    char *pszResult = NULL;
    if (pszPath)
    {
        /* Create the percent encoded strings and calculate the necessary uri length. */
        char *pszPath1 = rtUriPercentEncodeN(pszPath, RTSTR_MAX);
        if (pszPath1)
        {
            size_t cbSize = 7 /* file:// */ + strlen(pszPath1) + 1; /* plus zero byte */
            if (pszPath1[0] != '/')
                ++cbSize;
            char *pszTmp = pszResult = RTStrAlloc(cbSize);
            if (pszResult)
            {
                /* Compose the target uri string. */
                *pszTmp = '\0';
                RTStrCatP(&pszTmp, &cbSize, "file://");
                if (pszPath1[0] != '/')
                    RTStrCatP(&pszTmp, &cbSize, "/");
                RTStrCatP(&pszTmp, &cbSize, pszPath1);
            }
            RTStrFree(pszPath1);
        }
    }
    return pszResult;
}

RTR3DECL(char *) RTUriFilePath(const char *pszUri, uint32_t uFormat)
{
    return RTUriFileNPath(pszUri, uFormat, RTSTR_MAX);
}

RTR3DECL(char *) RTUriFileNPath(const char *pszUri, uint32_t uFormat, size_t cchMax)
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

    /* Check that this is a file Uri */
    if (RTStrNICmp(pszUri, RT_STR_TUPLE("file:")) != 0)
        return NULL;

    RTURIPARSED Parsed;
    int rc = rtUriParse(pszUri, &Parsed);
    if (RT_SUCCESS(rc) && Parsed.cchPath)
    {
        /* Special hack for DOS path like file:///c:/WINDOWS/clock.avi where we
           have to drop the leading slash that was used to separate the authority
           from the path. */
        if (  uFormat == URI_FILE_FORMAT_WIN
            && Parsed.cchPath >= 3
            && pszUri[Parsed.offPath] == '/'
            && pszUri[Parsed.offPath + 2] == ':'
            && RT_C_IS_ALPHA(pszUri[Parsed.offPath + 1]) )
        {
            Parsed.offPath++;
            Parsed.cchPath--;
        }

        char *pszPath = rtUriPercentDecodeN(&pszUri[Parsed.offPath], Parsed.cchPath);
        if (uFormat == URI_FILE_FORMAT_UNIX)
            return RTPathChangeToUnixSlashes(pszPath, true);
        Assert(uFormat == URI_FILE_FORMAT_WIN);
        return RTPathChangeToDosSlashes(pszPath, true);
    }
    return NULL;
}

