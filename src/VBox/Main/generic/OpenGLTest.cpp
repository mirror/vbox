/* $Id$ */
/** @file
 * VBox host opengl support test - generic implementation.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>

bool is3DAccelerationSupported()
{
    static char pszVBoxPath[RTPATH_MAX];
    const char *papszArgs[4] = { NULL, "-test", "3D", NULL};
    int rc;
    RTPROCESS Process;
    RTPROCSTATUS ProcStatus;
    uint64_t StartTS;

    rc = RTPathExecDir(pszVBoxPath, RTPATH_MAX); AssertRCReturn(rc, false);
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    rc = RTPathAppend(pszVBoxPath, RTPATH_MAX, "VBoxTestOGL.exe");
    static char pszVBoxPathArg[RTPATH_MAX];
    pszVBoxPathArg[0] = '"';
    strcpy(pszVBoxPathArg+1, pszVBoxPath);
    char *pszPathEnd = (char *)memchr(pszVBoxPathArg, '\0', RTPATH_MAX);
    pszPathEnd[0] = '"';
    pszPathEnd[1] = '\0';
    papszArgs[0] = pszVBoxPathArg;         /* argv[0] */
#else
    rc = RTPathAppend(pszVBoxPath, RTPATH_MAX, "VBoxTestOGL");
    papszArgs[0] = pszVBoxPath;         /* argv[0] */
#endif
    AssertRCReturn(rc, false);

    rc = RTProcCreate(pszVBoxPath, papszArgs, RTENV_DEFAULT, 0, &Process);
    if (RT_FAILURE(rc))
        return false;

    StartTS = RTTimeMilliTS();

    while (1)
    {
        rc = RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
        if (rc != VERR_PROCESS_RUNNING)
            break;

        if (RTTimeMilliTS() - StartTS > 30*1000 /* 30 sec */)
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

