/* $Id$ */
/** @file
 * IPRT Testcase - Core Dumper.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <iprt/types.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/coredumper.h>


/*******************************************************************************
*   Globals                                                                    *
*******************************************************************************/
static unsigned volatile g_cErrors = 0;

static DECLCALLBACK(int) SleepyThread(RTTHREAD Thread, void *pvUser)
{
    NOREF(pvUser);
    RTThreadSleep(90000000);
    return VINF_SUCCESS;
}

int main()
{
    RTR3Init();
    RTPrintf("tstRTCoreDump: TESTING...\n");

    /*
     * Setup core dumping.
     */
    int rc = RTCoreDumperSetup(NULL, 0);
    if (RT_SUCCESS(rc))
    {
        /*
         * Spawn a few threads.
         */
        RTTHREAD ahThreads[5];
        unsigned i = 0;
        for (; i < RT_ELEMENTS(ahThreads); i++)
        {
            rc = RTThreadCreate(&ahThreads[i], SleepyThread, &ahThreads[i], 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "TEST1");
            if (RT_FAILURE(rc))
            {
                RTPrintf("tstRTCoreDump: FAILURE(%d) - %d RTThreadCreate failed, rc=%Rrc\n", __LINE__, i, rc);
                g_cErrors++;
                ahThreads[i] = NIL_RTTHREAD;
                break;
            }
        }
        RTPrintf("Spawned %d threads.\n", i);
    
        /*
         * Write the core to disk.
         */
        rc = RTCoreDumperTakeDump(NULL);
        if (RT_FAILURE(rc))
        {
            g_cErrors++;
            RTPrintf("RTCoreDumperTakeDump failed. rc=%Rrc\n", rc);
        }
        RTCoreDumperDisable();
    }
    else
    {
        g_cErrors++;
        RTPrintf("RTCoreDumperSetup failed. rc=%Rrc\n", rc);
    }

    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstRTCoreDump: SUCCESS\n");
    else
        RTPrintf("tstRTCoreDump: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

