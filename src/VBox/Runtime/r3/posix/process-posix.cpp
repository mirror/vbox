/* $Id$ */
/** @file
 * IPRT - Process, POSIX.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_PROCESS
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>

#include <iprt/process.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/pipe.h>
#include <iprt/socket.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include "internal/process.h"


RTR3DECL(int)   RTProcWait(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    int rc;
    do rc = RTProcWaitNoResume(Process, fFlags, pProcStatus);
    while (rc == VERR_INTERRUPTED);
    return rc;
}


RTR3DECL(int)   RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    /*
     * Validate input.
     */
    if (Process <= 0)
    {
        AssertMsgFailed(("Invalid Process=%d\n", Process));
        return VERR_INVALID_PARAMETER;
    }
    if (fFlags & ~(RTPROCWAIT_FLAGS_NOBLOCK | RTPROCWAIT_FLAGS_BLOCK))
    {
        AssertMsgFailed(("Invalid flags %#x\n", fFlags));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Perform the wait.
     */
    int iStatus = 0;
    int rc = waitpid(Process, &iStatus, fFlags & RTPROCWAIT_FLAGS_NOBLOCK ? WNOHANG : 0);
    if (rc > 0)
    {
        /*
         * Fill in the status structure.
         */
        if (pProcStatus)
        {
            if (WIFEXITED(iStatus))
            {
                pProcStatus->enmReason = RTPROCEXITREASON_NORMAL;
                pProcStatus->iStatus = WEXITSTATUS(iStatus);
            }
            else if (WIFSIGNALED(iStatus))
            {
                pProcStatus->enmReason = RTPROCEXITREASON_SIGNAL;
                pProcStatus->iStatus = WTERMSIG(iStatus);
            }
            else
            {
                Assert(!WIFSTOPPED(iStatus));
                pProcStatus->enmReason = RTPROCEXITREASON_ABEND;
                pProcStatus->iStatus = iStatus;
            }
        }
        return VINF_SUCCESS;
    }

    /*
     * Child running?
     */
    if (!rc)
    {
        Assert(fFlags & RTPROCWAIT_FLAGS_NOBLOCK);
        return VERR_PROCESS_RUNNING;
    }

    /*
     * Figure out which error to return.
     */
    int iErr = errno;
    if (iErr == ECHILD)
        return VERR_PROCESS_NOT_FOUND;
    return RTErrConvertFromErrno(iErr);
}


RTR3DECL(int) RTProcTerminate(RTPROCESS Process)
{
    if (Process == NIL_RTPROCESS)
        return VINF_SUCCESS;

    if (!kill(Process, SIGKILL))
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(uint64_t) RTProcGetAffinityMask(void)
{
    /// @todo
    return 1;
}


RTR3DECL(int) RTProcQueryUsername(RTPROCESS hProcess, char *pszUser, size_t cbUser,
                                  size_t *pcbUser)
{
    AssertReturn(   (pszUser && cbUser > 0)
                 || (!pszUser && !cbUser), VERR_INVALID_PARAMETER);

    if (hProcess != RTProcSelf())
        return VERR_NOT_SUPPORTED;

    int32_t cbPwdMax = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (cbPwdMax == -1)
        return RTErrConvertFromErrno(errno);

    char *pbBuf = (char *)RTMemAllocZ(cbPwdMax);
    if (!pbBuf)
        return VERR_NO_MEMORY;

    struct passwd Pwd, *pPwd;
    int rc = getpwuid_r(geteuid(), &Pwd, pbBuf, cbPwdMax, &pPwd);
    if (!rc)
    {
        size_t cbPwdUser = strlen(pPwd->pw_name) + 1;

        if (pcbUser)
            *pcbUser = cbPwdUser;

        if (cbPwdUser > cbUser)
            rc = VERR_BUFFER_OVERFLOW;
        else
        {
            memcpy(pszUser, pPwd->pw_name, cbPwdUser);
            rc = VINF_SUCCESS;
        }
    }
    else
        rc = RTErrConvertFromErrno(rc);

    RTMemFree(pbBuf);
    return rc;
}


