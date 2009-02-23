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
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/process.h>
#include <iprt/param.h>
#include <iprt/path.h>



int main(int argc, char **argv)
{
    RTR3Init();

    int rc = VERR_GENERAL_FAILURE;

    char szExecPath[RTPATH_MAX] = { "vbox-5b05e1ff-6ae2-4d10-885a-7d25018c4c5b" };
    /* Check for a definitely not running process. */
    RTPrintf("tstRTProcIsRunningByName: Searching for a not running process with the name %s...\n", szExecPath);
    bool fRunning = RTProcIsRunningByName(szExecPath);
    if (!fRunning)
    {
        RTPrintf("tstRTProcIsRunningByName: Success!\n");
        rc = VINF_SUCCESS;
    }
    else
        RTPrintf("tstRTProcIsRunningByName: Expected failure but got success!\n");
    /* If the first test succeeded, check for a running process. For that we
     * use the name of this testcase. */
    if (RT_SUCCESS(rc))
    {
        /* Reset */
        rc = VERR_GENERAL_FAILURE;
        if (RTProcGetExecutableName(szExecPath, RTPATH_MAX))
        {
            /* Strip any path components */
            char *pszFilename;
            if ((pszFilename = RTPathFilename(szExecPath)))
            {
                RTPrintf("tstRTProcIsRunningByName: Searching for a running process with the name %s...\n", pszFilename);
                bool fRunning = RTProcIsRunningByName(pszFilename);
                if (fRunning)
                {
                    RTPrintf("tstRTProcIsRunningByName: Success!\n");
                    rc = VINF_SUCCESS;
                }
                else
                    RTPrintf("tstRTProcIsRunningByName: Expected success but got failure!\n");
            }
            else
                RTPrintf("tstRTProcIsRunningByName: RTPathFilename failed!\n", rc);
        }
        else
            RTPrintf("tstRTProcIsRunningByName: RTProcGetExecutableName failed!\n", rc);
    }

    return RT_SUCCESS(rc) ? 0 : 1;
}
