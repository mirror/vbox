/* $Id$ */
/** @file
 * IPRT Testcase - File I/O.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <iprt/test.h>
#include <iprt/file.h>
#include <iprt/errcore.h>
#include <iprt/string.h>
#include <iprt/stream.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char g_szTestStr[] = "Sausages and bacon for breakfast again!\n";
static char       g_szTestStr2[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut "
"enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor "
"in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non "
"proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n"
"\n"
"Curabitur pretium tincidunt lacus. Nulla gravida orci a odio. Nullam varius, turpis et commodo pharetra, est eros bibendum "
"elit, nec luctus magna felis sollicitudin mauris. Integer in mauris eu nibh euismod gravida. Duis ac tellus et risus "
"vulputate vehicula. Donec lobortis risus a elit. Etiam tempor. Ut ullamcorper, ligula eu tempor congue, eros est euismod "
"turpis, id tincidunt sapien risus a quam. Maecenas fermentum consequat mi. Donec fermentum. Pellentesque malesuada nulla a mi. "
"Duis sapien sem, aliquet nec, commodo eget, consequat quis, neque. Aliquam faucibus, elit ut dictum aliquet, felis nisl "
"adipiscing sapien, sed malesuada diam lacus eget erat. Cras mollis scelerisque nunc. Nullam arcu. Aliquam consequat. Curabitur "
"augue lorem, dapibus quis, laoreet et, pretium ac, nisi. Aenean magna nisl, mollis quis, molestie eu, feugiat in, orci. In hac "
"habitasse platea dictumst.\n";


static void tstAppend(RTFILE hFile)
{
    char achBuf[sizeof(g_szTestStr2) * 4];

    /*
     * Write some stuff and read it back.
     */
    size_t const cbWrite1 = sizeof(g_szTestStr2)  / 4;
    RTTESTI_CHECK_RC_RETV(RTFileWrite(hFile, g_szTestStr2, sizeof(g_szTestStr2) - 1, NULL), VINF_SUCCESS);

    size_t const offWrite2 = cbWrite1;
    size_t const cbWrite2  = sizeof(g_szTestStr2) / 2;
    RTTESTI_CHECK_RC_RETV(RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTFileWrite(hFile, &g_szTestStr2[offWrite2], cbWrite2, NULL), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTFileRead(hFile, achBuf, cbWrite1 + cbWrite2, NULL), VINF_SUCCESS);
    if (memcmp(achBuf, g_szTestStr2, cbWrite1 + cbWrite2) != 0)
        RTTestIFailed("Read back #1 failed (%#zx + %#zx)", cbWrite1, cbWrite2);

#if 1 //ndef RT_OS_WINDOWS
    /*
     * Truncate the file and write some more. This is problematic on windows,
     * we currently have a questionable hack in place to make this work.
     */
    RTTESTI_CHECK_RC_RETV(RTFileSetSize(hFile, 0), VINF_SUCCESS);

    size_t const offWrite3 = cbWrite1 + cbWrite2;
    size_t const cbWrite3  = sizeof(g_szTestStr2) - 1 - offWrite3;
    RTTESTI_CHECK_RC_RETV(RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTFileWrite(hFile, &g_szTestStr2[offWrite3], cbWrite3, NULL), VINF_SUCCESS);

    RTTESTI_CHECK_RC_RETV(RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, NULL), VINF_SUCCESS);
    RTTESTI_CHECK_RC_RETV(RTFileRead(hFile, achBuf, cbWrite3, NULL), VINF_SUCCESS);
    if (memcmp(achBuf, &g_szTestStr2[offWrite3], cbWrite3) != 0)
        RTTestIFailed("Read back #2 failed (%#zx)", cbWrite3);
#endif
}


static void tstBasics(RTFILE hFile)
{
    RTFOFF cbMax = -2;
    int rc = RTFileQueryMaxSizeEx(hFile, &cbMax);
    if (rc != VERR_NOT_IMPLEMENTED)
    {
        if (rc != VINF_SUCCESS)
            RTTestIFailed("RTFileQueryMaxSizeEx failed: %Rrc", rc);
        else
        {
            RTTESTI_CHECK_MSG(cbMax > 0, ("cbMax=%RTfoff", cbMax));
            RTTESTI_CHECK_MSG(cbMax == RTFileGetMaxSize(hFile),
                              ("cbMax=%RTfoff, RTFileGetMaxSize->%RTfoff", cbMax, RTFileGetMaxSize(hFile)));
        }
    }

    /* grow file beyond 2G */
    rc = RTFileSetSize(hFile, _2G + _1M);
    if (RT_FAILURE(rc))
        RTTestIFailed("Failed to grow file #1 to 2.001GB. rc=%Rrc", rc);
    else
    {
        uint64_t cb;
        RTTESTI_CHECK_RC(RTFileQuerySize(hFile, &cb), VINF_SUCCESS);
        RTTESTI_CHECK_MSG(cb == _2G + _1M, ("RTFileQuerySize return %RX64 bytes, expected %RX64.", cb, _2G + _1M));

        /*
         * Try some writes at the beginning of the file.
         */
        uint64_t offFile = RTFileTell(hFile);
        RTTESTI_CHECK_MSG(offFile == 0, ("RTFileTell -> %#RX64, expected 0 (#1)", offFile));

        size_t cbWritten = 0;
        while (cbWritten < sizeof(g_szTestStr))
        {
            size_t cbWrittenPart;
            rc = RTFileWrite(hFile, &g_szTestStr[cbWritten], sizeof(g_szTestStr) - cbWritten, &cbWrittenPart);
            if (RT_FAILURE(rc))
                break;
            cbWritten += cbWrittenPart;
        }
        if (RT_FAILURE(rc))
            RTTestIFailed("Failed to write to file #1 at offset 0. rc=%Rrc\n", rc);
        else
        {
            /* check that it was written correctly. */
            rc = RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, NULL);
            if (RT_FAILURE(rc))
                RTTestIFailed("Failed to seek offset 0 in file #1. rc=%Rrc\n", rc);
            else
            {
                char        szReadBuf[sizeof(g_szTestStr)];
                size_t      cbRead = 0;
                while (cbRead < sizeof(g_szTestStr))
                {
                    size_t cbReadPart;
                    rc = RTFileRead(hFile, &szReadBuf[cbRead], sizeof(g_szTestStr) - cbRead, &cbReadPart);
                    if (RT_FAILURE(rc))
                        break;
                    cbRead += cbReadPart;
                }
                if (RT_FAILURE(rc))
                    RTTestIFailed("Failed to read from file #1 at offset 0. rc=%Rrc\n", rc);
                else
                {
                    if (!memcmp(szReadBuf, g_szTestStr, sizeof(g_szTestStr)))
                        RTPrintf("tstFile: head write ok\n");
                    else
                        RTTestIFailed("Data read from file #1 at offset 0 differs from what we wrote there.\n");
                }
            }
        }

        /*
         * Try some writes at the end of the file.
         */
        rc = RTFileSeek(hFile, _2G + _1M, RTFILE_SEEK_BEGIN, NULL);
        if (RT_FAILURE(rc))
            RTTestIFailed("Failed to seek to _2G + _1M in file #1. rc=%Rrc\n", rc);
        else
        {
            offFile = RTFileTell(hFile);
            if (offFile != _2G + _1M)
                RTTestIFailed("RTFileTell -> %#llx, expected %#llx (#2)\n", offFile, _2G + _1M);
            else
            {
                cbWritten = 0;
                while (cbWritten < sizeof(g_szTestStr))
                {
                    size_t cbWrittenPart;
                    rc = RTFileWrite(hFile, &g_szTestStr[cbWritten], sizeof(g_szTestStr) - cbWritten, &cbWrittenPart);
                    if (RT_FAILURE(rc))
                        break;
                    cbWritten += cbWrittenPart;
                }
                if (RT_FAILURE(rc))
                    RTTestIFailed("Failed to write to file #1 at offset 2G + 1M.  rc=%Rrc\n", rc);
                else
                {
                    rc = RTFileSeek(hFile, offFile, RTFILE_SEEK_BEGIN, NULL);
                    if (RT_FAILURE(rc))
                        RTTestIFailed("Failed to seek offset %RX64 in file #1. rc=%Rrc\n", offFile, rc);
                    else
                    {
                        char        szReadBuf[sizeof(g_szTestStr)];
                        size_t      cbRead = 0;
                        while (cbRead < sizeof(g_szTestStr))
                        {
                            size_t cbReadPart;
                            rc = RTFileRead(hFile, &szReadBuf[cbRead], sizeof(g_szTestStr) - cbRead, &cbReadPart);
                            if (RT_FAILURE(rc))
                                break;
                            cbRead += cbReadPart;
                        }
                        if (RT_FAILURE(rc))
                            RTTestIFailed("Failed to read from file #1 at offset 2G + 1M.  rc=%Rrc\n", rc);
                        else
                        {
                            if (!memcmp(szReadBuf, g_szTestStr, sizeof(g_szTestStr)))
                                RTPrintf("tstFile: tail write ok\n");
                            else
                                RTTestIFailed("Data read from file #1 at offset 2G + 1M differs from what we wrote there.\n");
                        }
                    }
                }
            }
        }

        /*
         * Some general seeking around.
         */
        rc = RTFileSeek(hFile, _2G + 1, RTFILE_SEEK_BEGIN, NULL);
        if (RT_FAILURE(rc))
            RTTestIFailed("Failed to seek to _2G + 1 in file #1. rc=%Rrc\n", rc);
        else
        {
            offFile = RTFileTell(hFile);
            if (offFile != _2G + 1)
                RTTestIFailed("RTFileTell -> %#llx, expected %#llx (#3)\n", offFile, _2G + 1);
        }

        /* seek end */
        rc = RTFileSeek(hFile, 0, RTFILE_SEEK_END, NULL);
        if (RT_FAILURE(rc))
            RTTestIFailed("Failed to seek to end of file #1. rc=%Rrc\n", rc);
        else
        {
            offFile = RTFileTell(hFile);
            if (offFile != _2G + _1M + sizeof(g_szTestStr)) /* assuming tail write was ok. */
                RTTestIFailed("RTFileTell -> %#RX64, expected %#RX64 (#4)\n", offFile, _2G + _1M + sizeof(g_szTestStr));
        }

        /* seek start */
        rc = RTFileSeek(hFile, 0, RTFILE_SEEK_BEGIN, NULL);
        if (RT_FAILURE(rc))
            RTTestIFailed("Failed to seek to end of file #1. rc=%Rrc\n", rc);
        else
        {
            offFile = RTFileTell(hFile);
            if (offFile != 0)
                RTTestIFailed("RTFileTell -> %#llx, expected 0 (#5)\n", offFile);
        }
    }
}

int main()
{
    RTTEST      hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTFile", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * Some basic tests.
     */
    RTTestSub(hTest, "Basics");
    int rc = -1;
    RTFILE hFile = NIL_RTFILE;
    RTTESTI_CHECK_RC(rc = RTFileOpen(&hFile, "tstFile#1.tst", RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE),
                     VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        tstBasics(hFile);
        RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileDelete("tstFile#1.tst"), VINF_SUCCESS);
    }

    /*
     * Test appending & truncation.
     */
    RTTestSub(hTest, "Append");
    hFile = NIL_RTFILE;
    RTTESTI_CHECK_RC(rc = RTFileOpen(&hFile, "tstFile#2.tst",
                                     RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_APPEND),
                     VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        tstAppend(hFile);
        RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);
        RTTESTI_CHECK_RC(RTFileDelete("tstFile#2.tst"), VINF_SUCCESS);
    }

    /*
     * Done.
     */
    return RTTestSummaryAndDestroy(hTest);
}

