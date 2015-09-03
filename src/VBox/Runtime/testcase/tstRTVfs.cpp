/* $Id$ */
/** @file
 * IPRT Testcase - IPRT Virtual File System (VFS) API
 */

/*
 * Copyright (C) 2015 Oracle Corporation
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
#include <iprt/filesystem.h>
#include <iprt/vfs.h>
#include <iprt/err.h>
#include <iprt/test.h>
#include <iprt/file.h>
#include <iprt/string.h>

/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

static const char *standardHandleToString(RTHANDLESTD enmHandle)
{
    switch (enmHandle)
    {
        case RTHANDLESTD_INPUT:  return "STDIN";
        case RTHANDLESTD_OUTPUT: return "STDOUT";
        case RTHANDLESTD_ERROR:  return "STDERR";
        default: break;
    }

    return "unknown";
}

static int tstVfsIoFromStandardHandle(RTTEST hTest, RTHANDLESTD enmHandle)
{
    RTTestPrintf(hTest, RTTESTLVL_SUB_TEST, "Testing: %s\n", standardHandleToString(enmHandle));

    RTVFSIOSTREAM hVfs = NIL_RTVFSIOSTREAM;
    int rc = RTVfsIoStrmFromStdHandle(enmHandle, 0, true /*fLeaveOpen*/, &hVfs);
    if (RT_SUCCESS(rc))
    {
        bool fOutput =    enmHandle == RTHANDLESTD_OUTPUT
                       || enmHandle == RTHANDLESTD_ERROR;
        if (fOutput)
        {
            RTTestPrintf(hTest, RTTESTLVL_SUB_TEST, "Output for %s:\n", standardHandleToString(enmHandle));

            char *pszBufWritten;
            int cchBuf = RTStrAPrintf(&pszBufWritten, "Testing %s\n", standardHandleToString(enmHandle));
            Assert(cchBuf);
            AssertPtr(pszBufWritten);

            size_t cbWritten;
            rc = RTVfsIoStrmWrite(hVfs, pszBufWritten, strlen(pszBufWritten), true /*fBlocking*/, &cbWritten);
            if (RT_SUCCESS(rc))
            {
                rc = cbWritten == strlen(pszBufWritten) ? VINF_SUCCESS : VERR_NOT_EQUAL;
                /** @todo Compare written + read output. */
            }

            RTStrFree(pszBufWritten);
        }
        else
        {

        }
    }
    else
        RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Error creating VFS I/O stream for %s: %Rrc\n",
                     standardHandleToString(enmHandle), rc);

    return rc;
}

int main(int argc, char **argv)
{
    /*
     * Initialize IPRT and create the test.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTVfs", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    do
    {
        rc = tstVfsIoFromStandardHandle(hTest, RTHANDLESTD_INPUT);
        RTTESTI_CHECK_BREAK(rc == VINF_SUCCESS);

        rc = tstVfsIoFromStandardHandle(hTest, RTHANDLESTD_OUTPUT);
        RTTESTI_CHECK_BREAK(rc == VINF_SUCCESS);

        rc = tstVfsIoFromStandardHandle(hTest, RTHANDLESTD_ERROR);
        RTTESTI_CHECK_BREAK(rc == VINF_SUCCESS);

    } while (0);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

