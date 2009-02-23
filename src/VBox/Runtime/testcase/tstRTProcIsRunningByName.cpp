/* $Id$ */
/** @file
 * IPRT Testcase - RTProcIsRunningByName
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/process.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>



int main(int argc, char **argv)
{
    int cErrors = 0;

    RTR3Init();
    RTPrintf("tstRTPRocIsRunningByName: TESTING...\n");

    /*
     * Test 1: Check for a definitely not running process.
     */
    int rc = VERR_GENERAL_FAILURE;
    char szExecPath[RTPATH_MAX] = { "vbox-5b05e1ff-6ae2-4d10-885a-7d25018c4c5b" };
    bool fRunning = RTProcIsRunningByName(szExecPath);
    if (!fRunning)
        RTPrintf("tstRTProcIsRunningByName: Process '%s' is not running (expected).\n", szExecPath);
    else
    {
        RTPrintf("tstRTProcIsRunningByName: FAILURE - '%s' is running! (test 1)\n", szExecPath);
        cErrors++;
    }

    /*
     * Test 2: Check for our own process.
     */
    if (RTProcGetExecutableName(szExecPath, RTPATH_MAX))
    {
        /* Strip any path components */
        char *pszFilename = RTPathFilename(szExecPath);
        if (pszFilename)
        {
            bool fRunning = RTProcIsRunningByName(pszFilename);
            if (fRunning)
                RTPrintf("tstRTProcIsRunningByName: Process '%s' (self) is running\n", pszFilename);
            else
            {
                RTPrintf("tstRTProcIsRunningByName: FAILURE - Process '%s' (self) is not running!\n", pszFilename);
                cErrors++;
            }
        }
        else
        {
            RTPrintf("tstRTProcIsRunningByName: FAILURE - RTPathFilename failed!\n");
            cErrors++;
        }
    }
    else
    {
        RTPrintf("tstRTProcIsRunningByName: FAILURE - RTProcGetExecutableName failed!\n");
        cErrors++;
    }

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstRTProcIsRunningByName: SUCCESS\n");
    else
        RTPrintf("tstRTProcIsRunningByName: FAILURE - %d errors\n", cErrors);
    return cErrors ? 1 : 0;
}
