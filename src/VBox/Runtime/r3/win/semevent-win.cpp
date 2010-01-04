/* $Id$ */
/** @file
 * IPRT - Event Sempahore, Windows.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#define LOG_GROUP RTLOGGROUP_SEMAPHORE
#include <Windows.h>

#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/strict.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Converts semaphore to win32 handle. */
#define SEM2HND(Sem) ((HANDLE)(uintptr_t)Sem)


/* Undefine debug mappings. */
#undef RTSemEventWait
#undef RTSemEventWaitNoResume


RTDECL(int)   RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    /*
     * Create the semaphore.
     * (Auto reset, not signaled, private event object.)
     */
    HANDLE hev = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hev)
    {
        *pEventSem = (RTSEMEVENT)(void *)hev;
        Assert(*pEventSem != NIL_RTSEMEVENT);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)   RTSemEventDestroy(RTSEMEVENT EventSem)
{
    if (EventSem == NIL_RTSEMEVENT)     /* don't bitch */
        return VERR_INVALID_HANDLE;

    /*
     * Close semaphore handle.
     */
    if (CloseHandle(SEM2HND(EventSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy EventSem %p failed, lasterr=%d\n", EventSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)   RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc = WaitForSingleObjectEx(SEM2HND(EventSem), cMillies == RT_INDEFINITE_WAIT ? INFINITE : cMillies, TRUE);
    switch (rc)
    {
        case WAIT_OBJECT_0:         return VINF_SUCCESS;
        case WAIT_TIMEOUT:          return VERR_TIMEOUT;
        case WAIT_IO_COMPLETION:    return VERR_INTERRUPTED;
        case WAIT_ABANDONED:        return VERR_SEM_OWNER_DIED;
        default:
        {
            AssertMsgFailed(("Wait on EventSem %p failed, rc=%d lasterr=%d\n", EventSem, rc, GetLastError()));
            int rc2 = RTErrConvertFromWin32(GetLastError());
            if (rc2)
                return rc2;

            AssertMsgFailed(("WaitForSingleObject(event) -> rc=%d while converted lasterr=%d\n", rc, rc2));
            return VERR_INTERNAL_ERROR;
        }
    }
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Signal the object.
     */
    if (SetEvent(SEM2HND(EventSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Signaling EventSem %p failed, lasterr=%d\n", EventSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(void) RTSemEventSetSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{
/** @todo implement RTSemEventSetSignaller and friends for NT. */
}


RTDECL(void) RTSemEventAddSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{

}


RTDECL(void) RTSemEventRemoveSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{

}

