/* $Id$ */
/** @file
 * Output stream parsing test cases.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/err.h>
#include <VBox/log.h>

#include "../include/GuestCtrlImplPrivate.h"

using namespace com;

#include <iprt/env.h>
#include <iprt/test.h>
#include <iprt/stream.h>

#ifndef BYTE
# define BYTE uint8_t
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define STR_SIZE(a_sz) RT_STR_TUPLE(a_sz)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct VBOXGUESTCTRL_BUFFER_VALUE
{
    char *pszValue;
} VBOXGUESTCTRL_BUFFER_VALUE, *PVBOXGUESTCTRL_BUFFER_VALUE;
typedef std::map< RTCString, VBOXGUESTCTRL_BUFFER_VALUE > GuestBufferMap;
typedef std::map< RTCString, VBOXGUESTCTRL_BUFFER_VALUE >::iterator GuestBufferMapIter;
typedef std::map< RTCString, VBOXGUESTCTRL_BUFFER_VALUE >::const_iterator GuestBufferMapIterConst;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
char szUnterm1[] = { 'a', 's', 'd', 'f' };
char szUnterm2[] = { 'f', 'o', 'o', '3', '=', 'b', 'a', 'r', '3' };

static struct
{
    const char *pbData;
    size_t      cbData;
    uint32_t    offStart;
    uint32_t    offAfter;
    uint32_t    cMapElements;
    int         iResult;
} g_aTestBlocks[] =
{
    /*
     * Single object parsing.
     * An object is represented by one or multiple key=value pairs which are
     * separated by a single "\0". If this termination is missing it will be assumed
     * that we need to collect more data to do a successful parsing.
     */
    /* Invalid stuff. */
    { NULL,                             0,          0,  0,                                         0, VERR_INVALID_POINTER },
    { NULL,                             512,        0,  0,                                         0, VERR_INVALID_POINTER },
    { "",                               0,          0,  0,                                         0, VERR_INVALID_PARAMETER },
    { "",                               0,          0,  0,                                         0, VERR_INVALID_PARAMETER },
    { "foo=bar1",                       0,          0,  0,                                         0, VERR_INVALID_PARAMETER },
    { "foo=bar2",                       0,          50, 50,                                        0, VERR_INVALID_PARAMETER },
    /* Empty buffers. */
    { "",                               1,          0,  1,                                         0, VINF_SUCCESS },
    { "\0",                             1,          0,  1,                                         0, VINF_SUCCESS },
    /* Unterminated values (missing "\0"). */
    { STR_SIZE("test1"),                            0,  0,                                         0, VERR_MORE_DATA },
    { STR_SIZE("test2="),                           0,  0,                                         0, VERR_MORE_DATA },
    { STR_SIZE("test3=test3"),                      0,  0,                                         0, VERR_MORE_DATA },
    { STR_SIZE("test4=test4\0t41"),                 0,  sizeof("test4=test4\0") - 1,               1, VERR_MORE_DATA },
    { STR_SIZE("test5=test5\0t51=t51"),             0,  sizeof("test5=test5\0") - 1,               1, VERR_MORE_DATA },
    /* Next block unterminated. */
    { STR_SIZE("t51=t51\0t52=t52\0\0t53=t53"),      0,  sizeof("t51=t51\0t52=t52\0") - 1,          2, VINF_SUCCESS },
    { STR_SIZE("test6=test6\0\0t61=t61"),           0,  sizeof("test6=test6\0") - 1,               1, VINF_SUCCESS },
    /* Good stuff. */
    { STR_SIZE("test61=\0test611=test611\0"),       0,  sizeof("test61=\0test611=test611\0") - 1,  2, VINF_SUCCESS },
    { STR_SIZE("test7=test7\0\0"),                  0,  sizeof("test7=test7\0") - 1,               1, VINF_SUCCESS },
    { STR_SIZE("test8=test8\0t81=t81\0\0"),         0,  sizeof("test8=test8\0t81=t81\0") - 1,      2, VINF_SUCCESS },
    /* Good stuff, but with a second block -- should be *not* taken into account since
     * we're only interested in parsing/handling the first object. */
    { STR_SIZE("t9=t9\0t91=t91\0\0t92=t92\0\0"),    0,  sizeof("t9=t9\0t91=t91\0") - 1,            2, VINF_SUCCESS },
    /* Nasty stuff. */
        /* iso 8859-1 encoding (?) of 'aou' all with diaeresis '=f' and 'ao' with diaeresis. */
    { STR_SIZE("\xe4\xf6\xfc=\x66\xe4\xf6\0\0"),    0,  sizeof("\xe4\xf6\xfc=\x66\xe4\xf6\0") - 1, 1, VINF_SUCCESS },
        /* Like above, but after the first '\0' it adds 'ooo=aaa' all letters with diaeresis. */
    { STR_SIZE("\xe4\xf6\xfc=\x66\xe4\xf6\0\xf6\xf6\xf6=\xe4\xe4\xe4"),
                                                    0,  sizeof("\xe4\xf6\xfc=\x66\xe4\xf6\0") - 1, 1, VERR_MORE_DATA },
    /* Some "real world" examples. */
    { STR_SIZE("hdr_id=vbt_stat\0hdr_ver=1\0name=foo.txt\0\0"), 0, sizeof("hdr_id=vbt_stat\0hdr_ver=1\0name=foo.txt\0") - 1,
                                                                                                                                          3, VINF_SUCCESS }
};

static struct
{
    const char *pbData;
    size_t      cbData;
    /** Number of data blocks retrieved. These are separated by "\0\0". */
    uint32_t    cBlocks;
    /** Overall result when done parsing. */
    int         iResult;
} const g_aTestStream[] =
{
    /* No blocks. */
    { "\0\0\0\0",                                      sizeof("\0\0\0\0"),                                0, VERR_NO_DATA },
    /* Good stuff. */
    { "\0b1=b1\0\0",                                   sizeof("\0b1=b1\0\0"),                             1, VERR_NO_DATA },
    { "b1=b1\0\0",                                     sizeof("b1=b1\0\0"),                               1, VERR_NO_DATA },
    { "b1=b1\0b2=b2\0\0",                              sizeof("b1=b1\0b2=b2\0\0"),                        1, VERR_NO_DATA },
    { "b1=b1\0b2=b2\0\0\0",                            sizeof("b1=b1\0b2=b2\0\0\0"),                      1, VERR_NO_DATA }
};

int manualTest(void)
{
    int rc = VINF_SUCCESS;
    static struct
    {
        const char *pbData;
        size_t      cbData;
        uint32_t    offStart;
        uint32_t    offAfter;
        uint32_t    cMapElements;
        int         iResult;
    } const s_aTest[] =
    {
        { "test5=test5\0t51=t51",           sizeof("test5=test5\0t51=t51"),                            0,  sizeof("test5=test5\0") - 1,                   1, VERR_MORE_DATA },
        { "\0\0test5=test5\0t51=t51",       sizeof("\0\0test5=test5\0t51=t51"),                        0,  sizeof("\0\0test5=test5\0") - 1,               1, VERR_MORE_DATA },
    };

    for (unsigned iTest = 0; iTest < RT_ELEMENTS(s_aTest); iTest++)
    {
        RTTestIPrintf(RTTESTLVL_DEBUG, "Manual test #%d\n", iTest);

        GuestToolboxStream stream;
        rc = stream.AddData((BYTE *)s_aTest[iTest].pbData, s_aTest[iTest].cbData);

        for (;;)
        {
            GuestToolboxStreamBlock block;
            rc = stream.ParseBlock(block);
            RTTestIPrintf(RTTESTLVL_DEBUG, "\tReturned with rc=%Rrc, numItems=%ld\n",
                          rc, block.GetCount());

            if (block.GetCount())
                break;
        }
    }

    return rc;
}

int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstParseBuffer", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    RTTestIPrintf(RTTESTLVL_DEBUG, "Initializing COM...\n");
    HRESULT hrc = com::Initialize();
    if (FAILED(hrc))
    {
        RTTestFailed(hTest, "Failed to initialize COM (%Rhrc)!\n", hrc);
        return RTEXITCODE_FAILURE;
    }

#ifdef DEBUG_andy
    int rc = manualTest();
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;
#endif

    AssertCompile(sizeof("sizecheck")   == 10);
    AssertCompile(sizeof("off=rab")     == 8);
    AssertCompile(sizeof("off=rab\0\0") == 10);

    RTTestSub(hTest, "Lines");
    for (unsigned iTest = 0; iTest < RT_ELEMENTS(g_aTestBlocks); iTest++)
    {
        RTTestIPrintf(RTTESTLVL_DEBUG, "=> Test #%u\n", iTest);

        GuestToolboxStream stream;
        if (RT_FAILURE(g_aTestBlocks[iTest].iResult))
            RTTestDisableAssertions(hTest);
        int iResult = stream.AddData((BYTE *)g_aTestBlocks[iTest].pbData, g_aTestBlocks[iTest].cbData);
        if (RT_FAILURE(g_aTestBlocks[iTest].iResult))
            RTTestRestoreAssertions(hTest);
        if (RT_SUCCESS(iResult))
        {
            GuestToolboxStreamBlock curBlock;
            iResult = stream.ParseBlock(curBlock);
            if (iResult != g_aTestBlocks[iTest].iResult)
                RTTestFailed(hTest, "Block #%u: Returned %Rrc, expected %Rrc", iTest, iResult, g_aTestBlocks[iTest].iResult);
            else if (stream.GetOffset() != g_aTestBlocks[iTest].offAfter)
                RTTestFailed(hTest, "Block #%uOffset %zu wrong, expected %u\n",
                             iTest, stream.GetOffset(), g_aTestBlocks[iTest].offAfter);
            else if (iResult == VERR_MORE_DATA)
                RTTestIPrintf(RTTESTLVL_DEBUG, "\tMore data (Offset: %zu)\n", stream.GetOffset());

            if (RT_SUCCESS(iResult) || iResult == VERR_MORE_DATA)
                if (curBlock.GetCount() != g_aTestBlocks[iTest].cMapElements)
                    RTTestFailed(hTest, "Block #%u: Map has %u elements, expected %u\n",
                                 iTest, curBlock.GetCount(), g_aTestBlocks[iTest].cMapElements);

            /* There is remaining data left in the buffer (which needs to be merged
             * with a following buffer) -- print it. */
            size_t off = stream.GetOffset();
            size_t cbToWrite = g_aTestBlocks[iTest].cbData - off;
            if (cbToWrite)
            {
                RTTestIPrintf(RTTESTLVL_DEBUG, "\tRemaining (%u):\n", cbToWrite);

                /* How to properly get the current RTTESTLVL (aka IPRT_TEST_MAX_LEVEL) here?
                 * Hack alert: Using RTEnvGet for now. */
                if (!RTStrICmp(RTEnvGet("IPRT_TEST_MAX_LEVEL"), "debug"))
                    RTStrmWriteEx(g_pStdOut, &g_aTestBlocks[iTest].pbData[off], cbToWrite - 1, NULL);
            }
        }
    }

    RTTestSub(hTest, "Blocks");
    for (unsigned iTest = 0; iTest < RT_ELEMENTS(g_aTestStream); iTest++)
    {
        RTTestIPrintf(RTTESTLVL_DEBUG, "=> Block test #%u\n", iTest);

        GuestToolboxStream stream;
        int iResult = stream.AddData((BYTE*)g_aTestStream[iTest].pbData, g_aTestStream[iTest].cbData);
        if (RT_SUCCESS(iResult))
        {
            uint32_t cBlocks = 0;
            uint8_t uSafeCouunter = 0;
            do
            {
                GuestToolboxStreamBlock curBlock;
                iResult = stream.ParseBlock(curBlock);
                RTTestIPrintf(RTTESTLVL_DEBUG, "Block #%u: Returned with %Rrc", iTest, iResult);
                if (RT_SUCCESS(iResult))
                {
                    /* Only count block which have at least one pair. */
                    if (curBlock.GetCount())
                        cBlocks++;
                }
                if (uSafeCouunter++ > 32)
                    break;
            } while (RT_SUCCESS(iResult));

            if (iResult != g_aTestStream[iTest].iResult)
                RTTestFailed(hTest, "Block #%uReturned %Rrc, expected %Rrc", iTest, iResult, g_aTestStream[iTest].iResult);
            else if (cBlocks != g_aTestStream[iTest].cBlocks)
                RTTestFailed(hTest, "Block #%uReturned %u blocks, expected %u", iTest, cBlocks, g_aTestStream[iTest].cBlocks);
        }
        else
            RTTestFailed(hTest, "Block #%u: Adding data failed with %Rrc", iTest, iResult);
    }

    RTTestIPrintf(RTTESTLVL_DEBUG, "Shutting down COM...\n");
    com::Shutdown();

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

