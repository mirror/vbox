/* $Id$ */

/** @file
 * VBox host opengl support test
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include <VBox/err.h>
#include <iprt/process.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/env.h>
#include <iprt/thread.h>
#include <string.h>
#include <stdio.h>

bool is3DAccelerationSupported()
{
    static char pszVBoxPath[RTPATH_MAX];
    const char *pArgs[1] = {NULL};
    int rc;
    RTPROCESS Process;
    RTPROCSTATUS ProcStatus;
    RTTIMESPEC Start;
    RTTIMESPEC Now;


    RTProcGetExecutableName(pszVBoxPath, RTPATH_MAX);
    RTPathStripFilename(pszVBoxPath);
    strcat(pszVBoxPath,"/VBoxTestOGL");
#ifdef RT_OS_WINDOWS
    strcat(pszVBoxPath,".exe");
#endif

    rc = RTProcCreate(pszVBoxPath, pArgs, RTENV_DEFAULT, 0, &Process);
    if (RT_FAILURE(rc)) return false;

    RTTimeNow(&Start);

    while (1)
    {
        rc = RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
        if (rc != VERR_PROCESS_RUNNING)
            break;

        if (RTTimeSpecGetMilli(RTTimeSpecSub(RTTimeNow(&Now), &Start)) > 30*1000 /* 30 sec */)
        {
            RTProcTerminate(Process);
            RTThreadSleep(100);
            RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
            return false;
        }
        RTThreadSleep(100);
    }

    if (RT_SUCCESS(rc))
    {
        if ((ProcStatus.enmReason==RTPROCEXITREASON_NORMAL) && (ProcStatus.iStatus==0))
        {
            return true;
        }
    }

    return false;
}
