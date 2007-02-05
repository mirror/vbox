/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Process, POSIX.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <l4/vboxserver/vboxserver.h>


RTR3DECL(int)   RTProcCreate(const char *pszExec, const char * const *papszArgs, const char * const *papszEnv, unsigned fFlags, PRTPROCESS pProcess)
{
    return VERR_NOT_IMPLEMENTED;
}

RTR3DECL(int)   RTProcWait(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    int rc;
    do rc = RTProcWaitNoResume(Process, fFlags, pProcStatus);
    while (rc == VERR_INTERRUPTED);
    return rc;
}


RTR3DECL(int)   RTProcWaitNoResume(RTPROCESS Process, unsigned fFlags, PRTPROCSTATUS pProcStatus)
{
    return VINF_SUCCESS;
}


RTR3DECL(int) RTProcTerminate(RTPROCESS Process)
{
    return VERR_NOT_IMPLEMENTED;
}



RTR3DECL(uint64_t) RTProcGetAffinityMask()
{
    return 1;
}


RTR3DECL(char *) RTProcGetExecutableName(char *pszExecName, size_t cchExecName)
{
    if (!_execname(pszExecName, cchExecName))
        return pszExecName;
    return NULL;
}

