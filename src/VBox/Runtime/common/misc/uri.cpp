/* $Id$ */
/** @file
 * IPRT - Uniform Resource Identifier handling.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <iprt/uri.h>

#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/stream.h>

/* General URI format:

    foo://example.com:8042/over/there?name=ferret#nose
    \_/   \______________/\_________/ \_________/ \__/
     |           |            |            |        |
  scheme     authority       path        query   fragment
     |   _____________________|__
    / \ /                        \
    urn:example:animal:ferret:nose
*/


/*******************************************************************************
*   Private RTUri helper                                                        *
*******************************************************************************/

/* The following defines characters which have to be % escaped:
   control = 00-1F
   space   = ' '
   delims  = '<' , '>' , '#' , '%' , '"'
   unwise  = '{' , '}' , '|' , '\' , '^' , '[' , ']' , '`'
*/
#define URI_EXCLUDED(a) \
     ((a) >= 0x0  && (a) <= 0x20) \
  || ((a) >= 0x5B && (a) <= 0x5E) \
  || ((a) >= 0x7B && (a) <= 0x7D) \
  || (a) == '<' || (a) == '>' || (a) == '#' \
  || (a) == '%' || (a) == '"' || (a) == '`'

static char *rtUriPercentEncodeN(const char *pszString, size_t cchMax)
{
    if (!pszString)
        return NULL;

    int rc = VINF_SUCCESS;

    size_t cbLen = RT_MIN(strlen(pszString), cchMax);
    /* The new string can be max 3 times in size of the original string. */
    char *pszNew = (char*)RTMemAlloc(cbLen * 3 + 1);
    if (!pszNew)
        return NULL;
    char *pszRes = NULL;
    size_t iIn = 0;
    size_t iOut = 0;
    while(iIn < cbLen)
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

    int rc = VINF_SUCCESS;
    size_t cbLen = RT_MIN(strlen(pszString), cchMax);
    /* The new string can only get smaller. */
    char *pszNew = (char*)RTMemAlloc(cbLen + 1);
    if (!pszNew)
        return NULL;
    char *pszRes = NULL;
    size_t iIn = 0;
    size_t iOut = 0;
    while(iIn < cbLen)
    {
        if (pszString[iIn] == '%')
        {
            /* % encoding means the percent sign and exactly 2 hexadecimal
             * digits describing the ASCII number of the character. */
            ++iIn;
            char szNum[] = { pszString[iIn++], pszString[iIn++], '\0' };
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
    size_t i = iStart;
    /* The scheme has to end with ':'. */
    while(i < iStart + cbLen)
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
    size_t i = iStart;
    /* The authority can end with '/' || '?' || '#'. */
    while(i < iStart + cbLen)
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
    size_t i = iStart;
    /* The path can end with '?' || '#'. */
    while(i < iStart + cbLen)
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

static bool rtUriCheckQueryStart(const char *pszUri, size_t iStart, size_t cbLen, size_t *piStart)
{
    /* The query start with a '?'. */
    if (   cbLen >= 1
        && pszUri[iStart] == '?')
    {
        *piStart = iStart + 1; /* Excluding '?' */
        return true;
    }
    return false;
}

static bool rtUriFindQueryEnd(const char *pszUri, size_t iStart, size_t cbLen, size_t *piEnd)
{
    size_t i = iStart;
    /* The query can end with '?' || '#'. */
    while(i < iStart + cbLen)
    {
        if (pszUri[i] == '#')
        {
            *piEnd = i;
            return true;
        }
        ++i;
    }
    return false;
}

static bool rtUriCheckFragmentStart(const char *pszUri, size_t iStart, size_t cbLen, size_t *piStart)
{
    /* The fragment start with a '#'. */
    if (   cbLen >= 1
        && pszUri[iStart] == '#')
    {
        *piStart = iStart + 1; /* Excluding '#' */
        return true;
    }
    return false;
}

/*******************************************************************************
*   Public RTUri interface                                                     *
*******************************************************************************/

/*******************************************************************************
*   Generic Uri methods                                                        *
*******************************************************************************/

RTR3DECL(char *) RTUriCreate(const char *pszScheme, const char *pszAuthority, const char *pszPath, const char *pszQuery, const char *pszFragment)
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

        char *pszTmp = pszResult = (char*)RTMemAllocZ(cbSize);
        if (!pszResult)
            break;
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
    }while (0);

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
    AssertPtrReturn(pszUri, NULL);

    size_t iPos1;
    size_t cbLen = strlen(pszUri);
    /* Find the end of the scheme. */
    if (!rtUriFindSchemeEnd(pszUri, 0, cbLen, &iPos1))
        return NULL; /* no URI */
    else
        ++iPos1; /* Skip ':' */

    size_t iPos2;
    /* Find the start of the authority. */
    if (rtUriCheckAuthorityStart(pszUri, iPos1, cbLen - iPos1, &iPos2))
    {
        size_t iPos3 = cbLen;
        /* Find the end of the authority. If not found, the rest of the string
         * is used. */
        rtUriFindAuthorityEnd(pszUri, iPos2, cbLen - iPos2, &iPos3);
        if (iPos3 > iPos2) /* Length check */
            return rtUriPercentDecodeN(&pszUri[iPos2], iPos3 - iPos2);
        else
            return NULL;
    }
    return NULL;
}

RTR3DECL(char *) RTUriPath(const char *pszUri)
{
    AssertPtrReturn(pszUri, NULL);

    size_t iPos1;
    size_t cbLen = strlen(pszUri);
    /* Find the end of the scheme. */
    if (!rtUriFindSchemeEnd(pszUri, 0, cbLen, &iPos1))
        return NULL; /* no URI */
    else
        ++iPos1; /* Skip ':' */

    size_t iPos2;
    size_t iPos3 = iPos1; /* Skip if no authority is found */
    /* Find the start of the authority. */
    if (rtUriCheckAuthorityStart(pszUri, iPos1, cbLen - iPos1, &iPos2))
    {
        /* Find the end of the authority. If not found, then there is no path
         * component, cause the authority is the rest of the string. */
        if (!rtUriFindAuthorityEnd(pszUri, iPos2, cbLen - iPos2, &iPos3))
            return NULL; /* no path! */
    }

    size_t iPos4;
    /* Find the start of the path */
    if (rtUriCheckPathStart(pszUri, iPos3, cbLen - iPos3, &iPos4))
    {
        /* Search for the end of the scheme. */
        size_t iPos5 = cbLen;
        rtUriFindPathEnd(pszUri, iPos4, cbLen - iPos4, &iPos5);
        if (iPos5 > iPos4) /* Length check */
            return rtUriPercentDecodeN(&pszUri[iPos4], iPos5 - iPos4);
    }

    return NULL;
}

RTR3DECL(char *) RTUriQuery(const char *pszUri)
{
    AssertPtrReturn(pszUri, NULL);

    size_t iPos1;
    size_t cbLen = strlen(pszUri);
    /* Find the end of the scheme. */
    if (!rtUriFindSchemeEnd(pszUri, 0, cbLen, &iPos1))
        return NULL; /* no URI */
    else
        ++iPos1; /* Skip ':' */

    size_t iPos2;
    size_t iPos3 = iPos1; /* Skip if no authority is found */
    /* Find the start of the authority. */
    if (rtUriCheckAuthorityStart(pszUri, iPos1, cbLen - iPos1, &iPos2))
    {
        /* Find the end of the authority. If not found, then there is no path
         * component, cause the authority is the rest of the string. */
        if (!rtUriFindAuthorityEnd(pszUri, iPos2, cbLen - iPos2, &iPos3))
            return NULL; /* no path! */
    }

    size_t iPos4;
    size_t iPos5 = iPos3; /* Skip if no path is found */
    /* Find the start of the path */
    if (rtUriCheckPathStart(pszUri, iPos3, cbLen - iPos3, &iPos4))
    {
        /* Find the end of the path. If not found, then there is no query
         * component, cause the path is the rest of the string. */
        if (!rtUriFindPathEnd(pszUri, iPos4, cbLen - iPos4, &iPos5))
            return NULL; /* no query! */
    }

    size_t iPos6;
    /* Find the start of the query */
    if (rtUriCheckQueryStart(pszUri, iPos5, cbLen - iPos5, &iPos6))
    {
        /* Search for the end of the query. */
        size_t iPos7 = cbLen;
        rtUriFindQueryEnd(pszUri, iPos6, cbLen - iPos6, &iPos7);
        if (iPos7 > iPos6) /* Length check */
            return rtUriPercentDecodeN(&pszUri[iPos6], iPos7 - iPos6);
    }

    return NULL;
}

RTR3DECL(char *) RTUriFragment(const char *pszUri)
{
    AssertPtrReturn(pszUri, NULL);

    size_t iPos1;
    size_t cbLen = strlen(pszUri);
    /* Find the end of the scheme. */
    if (!rtUriFindSchemeEnd(pszUri, 0, cbLen, &iPos1))
        return NULL; /* no URI */
    else
        ++iPos1; /* Skip ':' */

    size_t iPos2;
    size_t iPos3 = iPos1; /* Skip if no authority is found */
    /* Find the start of the authority. */
    if (rtUriCheckAuthorityStart(pszUri, iPos1, cbLen - iPos1, &iPos2))
    {
        /* Find the end of the authority. If not found, then there is no path
         * component, cause the authority is the rest of the string. */
        if (!rtUriFindAuthorityEnd(pszUri, iPos2, cbLen - iPos2, &iPos3))
            return NULL; /* no path! */
    }

    size_t iPos4;
    size_t iPos5 = iPos3; /* Skip if no path is found */
    /* Find the start of the path */
    if (rtUriCheckPathStart(pszUri, iPos3, cbLen - iPos3, &iPos4))
    {
        /* Find the end of the path. If not found, then there is no query
         * component, cause the path is the rest of the string. */
        if (!rtUriFindPathEnd(pszUri, iPos4, cbLen - iPos4, &iPos5))
            return NULL; /* no query! */
    }

    size_t iPos6;
    size_t iPos7 = iPos5; /* Skip if no query is found */
    /* Find the start of the query */
    if (rtUriCheckQueryStart(pszUri, iPos5, cbLen - iPos5, &iPos6))
    {
        /* Find the end of the query If not found, then there is no fragment
         * component, cause the query is the rest of the string. */
        if (!rtUriFindQueryEnd(pszUri, iPos6, cbLen - iPos6, &iPos7))
            return NULL; /* no query! */
    }


    size_t iPos8;
    /* Find the start of the fragment */
    if (rtUriCheckFragmentStart(pszUri, iPos7, cbLen - iPos7, &iPos8))
    {
        /* There could be nothing behind a fragment. So use the rest of the
         * string. */
        if (cbLen > iPos8) /* Length check */
            return rtUriPercentDecodeN(&pszUri[iPos8], cbLen - iPos8);
    }
    return NULL;
}

/*******************************************************************************
*   File Uri methods                                                           *
*******************************************************************************/

RTR3DECL(char *) RTUriFileCreate(const char *pszPath)
{
    if (!pszPath)
        return NULL;

    char *pszResult = 0;
    char *pszPath1 = 0;

    do
    {
        /* Create the percent encoded strings and calculate the necessary uri
         * length. */
        pszPath1 = rtUriPercentEncodeN(pszPath, RTSTR_MAX);
        if (!pszPath1)
            break;
        size_t cbSize = 7 /* file:// */ + strlen(pszPath1) + 1; /* plus zero byte */
        if (pszPath1[0] != '/')
            ++cbSize;
        char *pszTmp = pszResult = (char*)RTMemAllocZ(cbSize);
        if (!pszResult)
            break;
        /* Compose the target uri string. */
        RTStrCatP(&pszTmp, &cbSize, "file://");
        if (pszPath1[0] != '/')
            RTStrCatP(&pszTmp, &cbSize, "/");
        RTStrCatP(&pszTmp, &cbSize, pszPath1);
    }while (0);

    /* Cleanup */
    if (pszPath1)
        RTStrFree(pszPath1);

    return pszResult;
}

RTR3DECL(char *) RTUriFilePath(const char *pszUri, uint32_t uFormat)
{
    return RTUriFileNPath(pszUri, uFormat, RTSTR_MAX);
}

RTR3DECL(char *) RTUriFileNPath(const char *pszUri, uint32_t uFormat, size_t cchMax)
{
    AssertPtrReturn(pszUri, NULL);

    size_t iPos1;
    size_t cbLen = RT_MIN(strlen(pszUri), cchMax);
    /* Find the end of the scheme. */
    if (!rtUriFindSchemeEnd(pszUri, 0, cbLen, &iPos1))
        return NULL; /* no URI */
    else
        ++iPos1; /* Skip ':' */

    /* Check that this is a file Uri */
    if (RTStrNICmp(pszUri, "file:", iPos1) != 0)
        return NULL;

    size_t iPos2;
    size_t iPos3 = iPos1; /* Skip if no authority is found */
    /* Find the start of the authority. */
    if (rtUriCheckAuthorityStart(pszUri, iPos1, cbLen - iPos1, &iPos2))
    {
        /* Find the end of the authority. If not found, then there is no path
         * component, cause the authority is the rest of the string. */
        if (!rtUriFindAuthorityEnd(pszUri, iPos2, cbLen - iPos2, &iPos3))
            return NULL; /* no path! */
    }

    size_t iPos4;
    /* Find the start of the path */
    if (rtUriCheckPathStart(pszUri, iPos3, cbLen - iPos3, &iPos4))
    {
        uint32_t uFIntern = uFormat;
        /* Auto is based on the current host OS. */
        if (uFormat == URI_FILE_FORMAT_AUTO)
#ifdef RT_OS_WINDOWS
            uFIntern = URI_FILE_FORMAT_WIN;
#else /* RT_OS_WINDOWS */
            uFIntern = URI_FILE_FORMAT_UNIX;
#endif /* !RT_OS_WINDOWS */

        if (   uFIntern != URI_FILE_FORMAT_UNIX
            && pszUri[iPos4] == '/')
            ++iPos4;
        /* Search for the end of the scheme. */
        size_t iPos5 = cbLen;
        rtUriFindPathEnd(pszUri, iPos4, cbLen - iPos4, &iPos5);
        if (iPos5 > iPos4) /* Length check */
        {
            char *pszPath = rtUriPercentDecodeN(&pszUri[iPos4], iPos5 - iPos4);
            if (uFIntern == URI_FILE_FORMAT_UNIX)
                return RTPathChangeToUnixSlashes(pszPath, true);
            else if (uFIntern == URI_FILE_FORMAT_WIN)
                return RTPathChangeToDosSlashes(pszPath, true);
            else
            {
                RTStrFree(pszPath);
                AssertMsgFailed(("Unknown uri file format %u", uFIntern));
                return NULL;
            }
        }
    }

    return NULL;
}

