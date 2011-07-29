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

#include <stdio.h>
#include <stdlib.h>

#include "../include/GuestCtrlImplPrivate.h"

using namespace com;

#define LOG_ENABLED
#define LOG_GROUP LOG_GROUP_MAIN
#define LOG_INSTANCE NULL
#include <VBox/log.h>

#include <iprt/test.h>
#include <iprt/stream.h>

#ifndef BYTE
# define BYTE uint8_t
#endif

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
    { "",                               1,                                                 0,  1,                                         0, VERR_MORE_DATA },
    { "\0",                             1,                                                 0,  1,                                         0, VERR_MORE_DATA },
    /* Incomplete buffer (missing components). */
    { szUnterm1,                        5,                                                 0,  0,                                         0, VERR_MORE_DATA },
    { "foo1",                           sizeof("foo1"),                                    0,  0,                                         0, VERR_MORE_DATA },
    { "=bar\0",                         sizeof("=bar"),                                    0,  0,                                         0, VERR_MORE_DATA },
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
    { "\0\0\0\0",                                      sizeof("\0\0\0\0"),                                0, VINF_SUCCESS },
    { "off=rab\0\0zab=oob",                            sizeof("off=rab\0\0zab=oob"),                      2, VINF_SUCCESS },
    { "\0\0\0soo=foo\0goo=loo\0\0zab=oob",             sizeof("\0\0\0soo=foo\0goo=loo\0\0zab=oob"),       2, VINF_SUCCESS },
    { "qoo=uoo\0\0\0\0asdf=\0\0",                      sizeof("qoo=uoo\0\0\0\0asdf=\0\0"),                2, VINF_SUCCESS },
    { "foo=bar\0\0\0\0\0\0",                           sizeof("foo=bar\0\0\0\0\0\0"),                     1, VINF_SUCCESS },
    { "qwer=cvbnr\0\0\0gui=uig\0\0\0",                 sizeof("qwer=cvbnr\0\0\0gui=uig\0\0\0"),           2, VINF_SUCCESS }
};

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstParseBuffer", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    RTTestIPrintf(RTTESTLVL_DEBUG, "Initializing COM...\n");
    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("ERROR: failed to initialize COM!\n");
        return rc;
    }

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
        uint32_t uOffset = aTests[iTest].uOffsetStart;

        RTTestIPrintf(RTTESTLVL_DEBUG, "=> Test #%u\n", iTest);

        GuestProcessStream stream;
        int iResult = stream.AddData((BYTE*)aTests[iTest].pbData, aTests[iTest].cbData);
        if (RT_SUCCESS(iResult))
        {
            iResult = stream.ParseBlock();
            if (iResult != aTests[iTest].iResult)
            {
                RTTestFailed(hTest, "\tReturned %Rrc, expected %Rrc",
                             iResult, aTests[iTest].iResult);
            }
            else if (stream.GetNumPairs() != aTests[iTest].uMapElements)
            {
                RTTestFailed(hTest, "\tMap has %u elements, expected %u",
                             stream.GetNumPairs(), aTests[iTest].uMapElements);
            }
            else if (stream.GetOffset() != aTests[iTest].uOffsetAfter)
            {
                RTTestFailed(hTest, "\tOffset %u wrong, expected %u",
                             stream.GetOffset(), aTests[iTest].uOffsetAfter);
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
        }
    }

    RTTestIPrintf(RTTESTLVL_INFO, "Doing block tests ...\n");

    for (unsigned iTest = 0; iTest < RT_ELEMENTS(aTests2); iTest++)
    {
        RTTestIPrintf(RTTESTLVL_DEBUG, "=> Block test #%u\n", iTest);

        GuestProcessStream stream;
        int iResult = stream.AddData((BYTE*)aTests2[iTest].pbData, aTests2[iTest].cbData);
        if (RT_SUCCESS(iResult))
        {
            uint32_t uNumBlocks = 0;

            do
            {
                iResult = stream.ParseBlock();
                RTTestIPrintf(RTTESTLVL_DEBUG, "\tReturned with %Rrc\n", iResult);
                if (   iResult == VINF_SUCCESS
 	                || iResult == VERR_MORE_DATA)
                {
                    /* Only count block which have at least one pair. */
                    if (stream.GetNumPairs())
                    {
                        uNumBlocks++;
                        stream.ClearPairs();
                    }
                }
                if (uNumBlocks > 32)
                    break; /* Give up if unreasonable big. */
            } while (iResult == VERR_MORE_DATA);

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
        else
            RTTestFailed(hTest, "\tAdding data failed with %Rrc", iResult);
    }

    RTTestIPrintf(RTTESTLVL_DEBUG, "Shutting down COM...\n");
    com::Shutdown();

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

