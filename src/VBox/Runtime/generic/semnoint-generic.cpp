/* $Id$ */
/** @file
 * innotek Portable Runtime - Generic Non-Interruptable Wait and Request Functions.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP RTLOGGROUP_SEM
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <iprt/err.h>
#include <iprt/assert.h>



RTDECL(int) RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    int rc;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        do rc = RTSemEventWaitNoResume(EventSem, cMillies);
        while (rc == VERR_INTERRUPTED);
    }
    else
    {
        const uint64_t u64Start = RTTimeMilliTS();
        rc = RTSemEventWaitNoResume(EventSem, cMillies);
        if (rc == VERR_INTERRUPTED)
        {
            do
            {
                uint64_t u64Elapsed = RTTimeMilliTS() - u64Start;
                if (u64Elapsed >= cMillies)
                    return VERR_TIMEOUT;
                rc = RTSemEventWaitNoResume(EventSem, cMillies - (unsigned)u64Elapsed);
            } while (rc == VERR_INTERRUPTED);
        }
    }
    return rc;
}


RTDECL(int) RTSemEventMultiWait(RTSEMEVENTMULTI EventSem, unsigned cMillies)
{
    int rc;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        do rc = RTSemEventMultiWaitNoResume(EventSem, cMillies);
        while (rc == VERR_INTERRUPTED);
    }
    else
    {
        const uint64_t u64Start = RTTimeMilliTS();
        rc = RTSemEventMultiWaitNoResume(EventSem, cMillies);
        if (rc == VERR_INTERRUPTED)
        {
            do
            {
                uint64_t u64Elapsed = RTTimeMilliTS() - u64Start;
                if (u64Elapsed >= cMillies)
                    return VERR_TIMEOUT;
                rc = RTSemEventMultiWaitNoResume(EventSem, cMillies - (unsigned)u64Elapsed);
            } while (rc == VERR_INTERRUPTED);
        }
    }
    return rc;
}


RTDECL(int) RTSemMutexRequest(RTSEMMUTEX Mutex, unsigned cMillies)
{
    int rc;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        do rc = RTSemMutexRequestNoResume(Mutex, cMillies);
        while (rc == VERR_INTERRUPTED);
    }
    else
    {
        const uint64_t u64Start = RTTimeMilliTS();
        rc = RTSemMutexRequestNoResume(Mutex, cMillies);
        if (rc == VERR_INTERRUPTED)
        {
            do
            {
                uint64_t u64Elapsed = RTTimeMilliTS() - u64Start;
                if (u64Elapsed >= cMillies)
                    return VERR_TIMEOUT;
                rc = RTSemMutexRequestNoResume(Mutex, cMillies - (unsigned)u64Elapsed);
            } while (rc == VERR_INTERRUPTED);
        }
    }
    return rc;
}

