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

#include <iprt/string.h>
#include <iprt/cpp/ministring.h>

#include <iprt/test.h>
#include <iprt/stream.h>

#ifndef BYTE
# define BYTE uint8_t
#endif

/** @todo Use original source of GuestCtrlImpl.cpp! */

typedef struct VBOXGUESTCTRL_BUFFER_VALUE
{
    char *pszValue;
} VBOXGUESTCTRL_BUFFER_VALUE, *PVBOXGUESTCTRL_BUFFER_VALUE;
typedef std::map< RTCString, VBOXGUESTCTRL_BUFFER_VALUE > GuestBufferMap;
typedef std::map< RTCString, VBOXGUESTCTRL_BUFFER_VALUE >::iterator GuestBufferMapIter;
typedef std::map< RTCString, VBOXGUESTCTRL_BUFFER_VALUE >::const_iterator GuestBufferMapIterConst;

char pszUnterm1[] = { 'a', 's', 'd', 'f' };
char pszUnterm2[] = { 'f', 'o', 'o', '3', '=', 'b', 'a', 'r', '3' };

static struct
{
    const char *pbData;
    size_t      cbData;
    uint32_t    uOffsetStart;
    uint32_t    uOffsetAfter;
    uint32_t    uMapElements;
    int         iResult;
} aTests[] =
{
    /* Invalid stuff. */
    { NULL,                             0,                                                 0,  0,       0, VERR_INVALID_POINTER },
    { NULL,                             512,                                               0,  0,       0, VERR_INVALID_POINTER },
    { "",                               0,                                                 0,  0,       0, VERR_INVALID_PARAMETER },
    { "",                               0,                                                 0,  0,       0, VERR_INVALID_PARAMETER },
    { "foo=bar1",                       0,                                                 0,  0,       0, VERR_INVALID_PARAMETER },
    { "foo=bar2",                       0,                                                 50, 50,      0, VERR_INVALID_PARAMETER },
    /* Incomplete buffer (missing \0 termination). */
    { "",                               1,                                                 0, 0,       0, VERR_MORE_DATA },
    { "\0",                             1,                                                 0, 0,       0, VERR_MORE_DATA },
    { pszUnterm1,                       5,                                                 0, 0,       0, VERR_MORE_DATA },
    { "foo1",                           sizeof("foo1"),                                    0, 0,       0, VERR_MORE_DATA },
    { pszUnterm2,                       8,                                                 0, 0,       0, VERR_MORE_DATA },
    /* Incomplete buffer (missing components). */
    { "=bar\0",                         sizeof("=bar"),                                    0,  0,                                         0, VERR_MORE_DATA },
    /* Last sequence is incomplete -- new offset should point to it. */
    { "hug=sub\0incomplete",            sizeof("hug=sub\0incomplete"),                     0,  sizeof("hug=sub"),                         1, VERR_MORE_DATA },
    { "boo=hoo\0baz=boo\0qwer",         sizeof("boo=hoo\0baz=boo\0qwer"),                  0,  sizeof("boo=hoo\0baz=boo"),                2, VERR_MORE_DATA },
    /* Parsing good stuff. */
    { "foo2=",                          sizeof("foo2="),                                   0,  sizeof("foo2="),                           1, VINF_SUCCESS },
    { "har=hor",                        sizeof("har=hor"),                                 0,  sizeof("har=hor"),                         1, VINF_SUCCESS },
    { "foo=bar\0baz=boo",               sizeof("foo=bar\0baz=boo"),                        0,  sizeof("foo=bar\0baz=boo"),                2, VINF_SUCCESS },
    /* Parsing until a different block (two terminations, returning offset to next block). */
    { "off=rab\0\0zab=oob",             sizeof("off=rab\0\0zab=oob"),                      0,  sizeof("zab=oob"),                         1, VERR_MORE_DATA }
};

int outputBufferParse(const BYTE *pbData, size_t cbData, uint32_t *puOffset, GuestBufferMap& mapBuf)
{
    AssertPtrReturn(pbData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puOffset, VERR_INVALID_POINTER);
    AssertReturn(*puOffset < cbData, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    size_t uCur = *puOffset;
    for (;uCur < cbData;)
    {
        const char *pszStart = (char*)&pbData[uCur];
        const char *pszEnd = pszStart;

        /* Search and of current pair (key=value\0). */
        while (uCur++ < cbData)
        {
            if (*pszEnd == '\0')
                break;
            pszEnd++;
        }

        size_t uPairLen = pszEnd - pszStart;
        if (   *pszEnd != '\0'
            || !uPairLen)
        {
            rc = VERR_MORE_DATA;
            break;
        }

        const char *pszSep = pszStart;
        while (   *pszSep != '='
               &&  pszSep != pszEnd)
        {
            pszSep++;
        }

        if (   pszSep == pszStart
            || pszSep == pszEnd)
        {
            rc = VERR_MORE_DATA;
            break;
        }

        size_t uKeyLen = pszSep - pszStart;
        size_t uValLen = pszEnd - (pszSep + 1);

        /* Get key (if present). */
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

            mapBuf[RTCString(pszKey)].pszValue = NULL;

            /* Get value (if present). */
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

                mapBuf[RTCString(pszKey)].pszValue = pszVal;
            }

            RTMemFree(pszKey);

            *puOffset += uCur - *puOffset;
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

    if (sizeof("sizecheck") != 10)
        RTTestFailed(hTest, "Basic size test failed (%u <-> 10)", sizeof("sizecheck"));

    for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests); iTest++)
    {
        GuestBufferMap bufMap;

        int iResult = outputBufferParse((BYTE*)aTests[iTest].pbData, aTests[iTest].cbData,
                                        &aTests[iTest].uOffsetStart, bufMap);

        RTTestIPrintf(RTTESTLVL_DEBUG, "=> Test #%u\n", iTest);

        if (iResult != aTests[iTest].iResult)
        {
            RTTestFailed(hTest, "\tReturned %Rrc, expected %Rrc",
                         iResult, aTests[iTest].iResult);
        }
        else if (bufMap.size() != aTests[iTest].uMapElements)
        {
            RTTestFailed(hTest, "\tMap has %u elements, expected %u",
                         bufMap.size(), aTests[iTest].uMapElements);
        }
        else if (aTests[iTest].uOffsetStart != aTests[iTest].uOffsetAfter)
        {
            RTTestFailed(hTest, "\tOffset %u wrong, expected %u",
                         aTests[iTest].uOffsetStart, aTests[iTest].uOffsetAfter);
        }
        else if (iResult == VERR_MORE_DATA)
        {
            /* There is remaining data left in the buffer (which needs to be merged
             * with a following buffer) -- print it. */
            const char *pszRemaining = aTests[iTest].pbData;
            size_t uOffsetNew = aTests[iTest].uOffsetStart;
            size_t uToWrite = aTests[iTest].cbData - uOffsetNew;
            if (pszRemaining && uOffsetNew)
            {
                RTTestIPrintf(RTTESTLVL_DEBUG, "\tRemaining (%u):\n", uToWrite);
                RTStrmWriteEx(g_pStdOut, &aTests[iTest].pbData[uOffsetNew], uToWrite - 1, NULL);
                RTTestIPrintf(RTTESTLVL_DEBUG, "\n");
            }
        }

        for (GuestBufferMapIter it = bufMap.begin(); it != bufMap.end(); it++)
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

