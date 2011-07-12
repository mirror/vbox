/* $Id$ */

/** @file
 *
 * Output stream parsing test cases.
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
 */

#include <map>

#include <VBox/com/string.h>

#include <iprt/string.h>

#include <iprt/test.h>
#include <iprt/stream.h>

using namespace com;

/** @todo Use original source of GuestCtrlImpl.cpp! */

typedef struct VBOXGUESTCTRL_BUFFER_VALUE
{
    char *pszValue;
} VBOXGUESTCTRL_BUFFER_VALUE, *PVBOXGUESTCTRL_BUFFER_VALUE;
typedef std::map< Utf8Str, VBOXGUESTCTRL_BUFFER_VALUE > GuestBufferMap;
typedef std::map< Utf8Str, VBOXGUESTCTRL_BUFFER_VALUE >::iterator GuestBufferMapIter;
typedef std::map< Utf8Str, VBOXGUESTCTRL_BUFFER_VALUE >::const_iterator GuestBufferMapIterConst;

char pszUnterm1[] = { 'a', 's', 'd', 'f' };

static struct
{
    const char *pbData;
    size_t      cbData;
    uint32_t    uOffset;
    uint32_t    uMapElements;
    int         iResult;
} aTests[] =
{
    /* Invalid stuff. */
    { NULL,         0,                  0, 0, VERR_INVALID_POINTER },
    { NULL,         512,                0, 0, VERR_INVALID_POINTER },
    { "",           0,                  0, 0, VERR_INVALID_PARAMETER },
    { "",           0,                  5, 0, VERR_INVALID_PARAMETER },
    { "",           1,                  0, 0, VINF_SUCCESS },
    { pszUnterm1,   5,                  0, 0, VINF_SUCCESS },
    { "foo=bar",    0,                  0, 0, VERR_INVALID_PARAMETER },

    /* Parsing stuff. */
    { "foo",                    sizeof("foo"),                          0, 0, VINF_SUCCESS },
    { "foo=",                   sizeof("foo="),                         0, 1, VINF_SUCCESS },
    { "=bar",                   sizeof("=bar"),                         0, 0, VINF_SUCCESS },
    { "foo=bar",                sizeof("foo=bar"),                      0, 1, VINF_SUCCESS },
    { "foo=bar\0baz=boo",       16,                                     0, 2, VINF_SUCCESS }
};

int outputBufferParse(const BYTE *pbData, size_t cbData, uint32_t *puOffset, GuestBufferMap& mapBuf)
{
    AssertPtrReturn(pbData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puOffset, VERR_INVALID_POINTER);
    AssertReturn(*puOffset < cbData, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    size_t uCur = *puOffset;
    for (uCur = 0; uCur < cbData; uCur++)
    {
        const char *pszStart = (char*)&pbData[uCur];
        const char *pszEnd = pszStart;

        /* Search and of current pair (key=value\0). */
        while (   *pszEnd != '\0'
               && ++uCur < cbData)
        {
            pszEnd++;
        }

        size_t uLen = pszEnd - pszStart;
        if (!uLen)
            continue;

        const char *pszSep = pszStart;
        while (   *pszSep != '='
               &&  pszSep != pszEnd)
        {
            pszSep++;
        }

        if (   pszSep == pszStart
            || pszSep == pszEnd)
        {
            continue;
        }

        size_t uKeyLen = pszSep - pszStart;
        size_t uValLen = pszEnd - (pszSep + 1);

        if (uKeyLen)
        {
            Assert(pszSep > pszStart);
            char *pszKey = (char*)RTMemAllocZ(uKeyLen + 1);
            if (!pszKey)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            memcpy(pszKey, pszStart, uKeyLen);

            mapBuf[Utf8Str(pszKey)].pszValue = NULL;

            if (uValLen)
            {
                Assert(pszEnd > pszSep);
                char *pszVal = (char*)RTMemAllocZ(uValLen + 1);
                if (!pszVal)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                memcpy(pszVal, pszSep + 1, uValLen);

                mapBuf[Utf8Str(pszKey)].pszValue = pszVal;
            }

            RTMemFree(pszKey);
        }
    }

    return rc;
}

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstParseBuffer", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests); iTest++)
    {
        GuestBufferMap map;

        int iResult = outputBufferParse((BYTE*)aTests[iTest].pbData, aTests[iTest].cbData,
                                        &aTests[iTest].uOffset, map);
        if (iResult != aTests[iTest].iResult)
        {
            RTTestFailed(hTest, "#%u: Returned %Rrc, expected %Rrc",
                         iTest, iResult, aTests[iTest].iResult);
        }
        else if (map.size() != aTests[iTest].uMapElements)
        {
            RTTestFailed(hTest, "#%u: Map has %u elements, expected %u",
                         iTest, map.size(), aTests[iTest].uMapElements);
        }

        if (map.size())
            RTTestIPrintf(RTTESTLVL_DEBUG, "Output for test #%u\n", iTest);
        for (GuestBufferMapIter it = map.begin(); it != map.end(); it++)
        {
            RTTestIPrintf(RTTESTLVL_DEBUG, "\t%s -> %s\n",
                          it->first.c_str(), it->second.pszValue ? it->second.pszValue : "<undefined>");

            if (it->second.pszValue)
                RTMemFree(it->second.pszValue);
        }
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

