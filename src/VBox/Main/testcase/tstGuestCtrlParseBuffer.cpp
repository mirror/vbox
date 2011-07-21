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

char szUnterm1[] = { 'a', 's', 'd', 'f' };
char szUnterm2[] = { 'f', 'o', 'o', '3', '=', 'b', 'a', 'r', '3' };

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
    { NULL,                             0,                                                 0,  0,                                         0, VERR_INVALID_POINTER },
    { NULL,                             512,                                               0,  0,                                         0, VERR_INVALID_POINTER },
    { "",                               0,                                                 0,  0,                                         0, VERR_INVALID_PARAMETER },
    { "",                               0,                                                 0,  0,                                         0, VERR_INVALID_PARAMETER },
    { "foo=bar1",                       0,                                                 0,  0,                                         0, VERR_INVALID_PARAMETER },
    { "foo=bar2",                       0,                                                 50, 50,                                        0, VERR_INVALID_PARAMETER },
    /* Empty buffers. */
    { "",                               1,                                                 0, 1,                                          0, VERR_MORE_DATA },
    { "\0",                             1,                                                 0, 1,                                          0, VERR_MORE_DATA },
    /* Incomplete buffer (missing components). */
    { szUnterm1,                        5,                                                 0, 0,                                          0, VERR_MORE_DATA },
    { "foo1",                           sizeof("foo1"),                                    0, 0,                                          0, VERR_MORE_DATA },
    { "=bar\0",                         sizeof("=bar"),                                    0, 0 ,                                         0, VERR_MORE_DATA },
    /* Last sequence is incomplete -- new offset should point to it. */
    { "hug=sub\0incomplete",            sizeof("hug=sub\0incomplete"),                     0,  sizeof("hug=sub"),                         1, VERR_MORE_DATA },
    { "boo=hoo\0baz=boo\0qwer",         sizeof("boo=hoo\0baz=boo\0qwer"),                  0,  sizeof("boo=hoo\0baz=boo"),                2, VERR_MORE_DATA },
    /* Parsing good stuff. */
    { "novalue=",                       sizeof("novalue="),                                0,  sizeof("novalue="),                        1, VINF_SUCCESS },
    { szUnterm2,                        8,                                                 0,  sizeof(szUnterm2),                         1, VINF_SUCCESS },
    { "foo2=",                          sizeof("foo2="),                                   0,  sizeof("foo2="),                           1, VINF_SUCCESS },
    { "har=hor",                        sizeof("har=hor"),                                 0,  sizeof("har=hor"),                         1, VINF_SUCCESS },
    { "foo=bar\0baz=boo",               sizeof("foo=bar\0baz=boo"),                        0,  sizeof("foo=bar\0baz=boo"),                2, VINF_SUCCESS },
    /* Parsing until a different block (two terminations, returning offset to next block). */
    { "off=rab\0a=b\0\0\0\0",           sizeof("off=rab\0a=b\0\0\0"),                      0,  13,                                        2, VERR_MORE_DATA },
    { "off=rab\0\0zab=oob",             sizeof("off=rab\0\0zab=oob"),                      0,  9,                                         1, VERR_MORE_DATA },
    { "\0\0\0\0off=rab\0zab=oob\0\0",   sizeof("\0\0\0\0off=rab\0zab=oob\0\0"),            0,  1,                                         0, VERR_MORE_DATA },
    { "o2=r2\0z3=o3\0\0f3=g3",          sizeof("o2=r2\0z3=o3\0\0f3=g3"),                   0,  13,                                        2, VERR_MORE_DATA }
};

static struct
{
    const char *pbData;
    size_t      cbData;
    /** Number of data blocks retrieved. These are separated by "\0\0". */
    uint32_t    uNumBlocks;
    /** Overall result when done parsing. */
    int         iResult;
} aTests2[] =
{
    { "\0\0\0\0",                                      sizeof("\0\0\0\0"),                                0, VERR_MORE_DATA },
    { "off=rab\0\0zab=oob",                            sizeof("off=rab\0\0zab=oob"),                      2, VINF_SUCCESS },
    { "\0\0\0soo=foo\0goo=loo\0\0zab=oob",             sizeof("\0\0\0soo=foo\0goo=loo\0\0zab=oob"),       2, VINF_SUCCESS },
    { "qoo=uoo\0\0\0\0asdf=\0\0",                      sizeof("qoo=uoo\0\0\0\0asdf=\0\0"),                2, VERR_MORE_DATA },
    { "foo=bar\0\0\0\0\0\0",                           sizeof("foo=bar\0\0\0\0\0\0"),                     1, VERR_MORE_DATA },
    { "qwer=cvbnr\0\0\0gui=uig\0\0\0",                 sizeof("qwer=cvbnr\0\0\0gui=uig\0\0\0"),           2, VERR_MORE_DATA }
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

        /* Search end of current pair (key=value\0). */
        while (uCur++ < cbData)
        {
            if (*pszEnd == '\0')
                break;
            pszEnd++;
        }

        size_t uPairLen = pszEnd - pszStart;
        if (uPairLen)
        {
            const char *pszSep = pszStart;
            while (   *pszSep != '='
                   &&  pszSep != pszEnd)
            {
                pszSep++;
            }

            /* No separator found (or incomplete key=value pair)? */
            if (   pszSep == pszStart
                || pszSep == pszEnd)
            {
                *puOffset =  uCur - uPairLen - 1;
                rc = VERR_MORE_DATA;
            }

            if (RT_FAILURE(rc))
                break;

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
        else /* No pair detected, check for a new block. */
        {
            do
            {
                if (*pszEnd == '\0')
                {
                    *puOffset = uCur;
                    rc = VERR_MORE_DATA;
                    break;
                }
                pszEnd++;
            } while (++uCur < cbData);
        }

        if (RT_FAILURE(rc))
            break;
    }

    RT_CLAMP(*puOffset, 0, cbData);

    return rc;
}

void tstOutputAndDestroyMap(GuestBufferMap &bufMap)
{
    for (GuestBufferMapIter it = bufMap.begin(); it != bufMap.end(); it++)
    {
        RTTestIPrintf(RTTESTLVL_DEBUG, "\t%s -> %s\n",
                      it->first.c_str(), it->second.pszValue ? it->second.pszValue : "<undefined>");

        if (it->second.pszValue)
            RTMemFree(it->second.pszValue);
    }

    bufMap.clear();
}

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstParseBuffer", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    RTTestIPrintf(RTTESTLVL_INFO, "Doing basic tests ...\n");

    if (sizeof("sizecheck") != 10)
        RTTestFailed(hTest, "Basic size test #1 failed (%u <-> 10)", sizeof("sizecheck"));
    if (sizeof("off=rab") != 8)
        RTTestFailed(hTest, "Basic size test #2 failed (%u <-> 7)", sizeof("off=rab"));
    if (sizeof("off=rab\0\0") != 10)
        RTTestFailed(hTest, "Basic size test #3 failed (%u <-> 10)", sizeof("off=rab\0\0"));

    RTTestIPrintf(RTTESTLVL_INFO, "Doing line tests ...\n");

    unsigned iTest = 0;
    for (iTest; iTest < RT_ELEMENTS(aTests); iTest++)
    {
        GuestBufferMap bufMap;
        uint32_t uOffset = aTests[iTest].uOffsetStart;

        int iResult = outputBufferParse((BYTE*)aTests[iTest].pbData, aTests[iTest].cbData,
                                        &uOffset, bufMap);

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
        else if (uOffset != aTests[iTest].uOffsetAfter)
        {
            RTTestFailed(hTest, "\tOffset %u wrong, expected %u",
                         uOffset, aTests[iTest].uOffsetAfter);
        }
        else if (iResult == VERR_MORE_DATA)
        {
            RTTestIPrintf(RTTESTLVL_DEBUG, "\tMore data (Offset: %u)\n", uOffset);

            /* There is remaining data left in the buffer (which needs to be merged
             * with a following buffer) -- print it. */
            size_t uToWrite = aTests[iTest].cbData - uOffset;
            if (uToWrite)
            {
                const char *pszRemaining = aTests[iTest].pbData;
                RTTestIPrintf(RTTESTLVL_DEBUG, "\tRemaining (%u):\n", uToWrite);
                RTStrmWriteEx(g_pStdOut, &aTests[iTest].pbData[uOffset], uToWrite - 1, NULL);
                RTTestIPrintf(RTTESTLVL_DEBUG, "\n");
            }
        }

        tstOutputAndDestroyMap(bufMap);
    }

    RTTestIPrintf(RTTESTLVL_INFO, "Doing block tests ...\n");

    for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests2); iTest++)
    {
        RTTestIPrintf(RTTESTLVL_DEBUG, "=> Block test #%u\n", iTest);

        int iResult;

        GuestBufferMap bufMap;
        uint32_t uOffset = 0;
        uint32_t uNumBlocks = 0;

        while (uOffset < aTests2[iTest].cbData - 1)
        {
            iResult = outputBufferParse((BYTE*)aTests2[iTest].pbData, aTests2[iTest].cbData,
                                        &uOffset, bufMap);
            RTTestIPrintf(RTTESTLVL_DEBUG, "\tReturned with %Rrc\n", iResult);
            if (   iResult == VINF_SUCCESS
                || iResult == VERR_MORE_DATA)
            {
                if (bufMap.size()) /* Only count block which have some valid data. */
                    uNumBlocks++;

                tstOutputAndDestroyMap(bufMap);

                RTTestIPrintf(RTTESTLVL_DEBUG, "\tNext offset %u (total: %u)\n",
                              uOffset, aTests2[iTest].cbData);
            }
            else
                break;

            if (uNumBlocks > 32)
                break; /* Give up if unreasonable big. */
        }

        if (iResult != aTests2[iTest].iResult)
        {
            RTTestFailed(hTest, "\tReturned %Rrc, expected %Rrc",
                         iResult, aTests2[iTest].iResult);
        }
        else if (uNumBlocks != aTests2[iTest].uNumBlocks)
        {
            RTTestFailed(hTest, "\tReturned %u blocks, expected %u\n",
                         uNumBlocks, aTests2[iTest].uNumBlocks);
        }
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

