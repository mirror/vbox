/* $Id$ */
/** @file
 * IPRT - Semaphores, implementation for Windows host platform.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/assert.h>
#include <iprt/err.h>


/** Converts semaphore to win32 handle. */
#define SEM2HND(Sem) ((HANDLE)(uintptr_t)Sem)



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
        return VINF_SUCCESS;
    }
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)   RTSemEventDestroy(RTSEMEVENT EventSem)
{
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




RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    /*
     * Create the semaphore.
     * (Manual reset, not signaled, private event object.)
     */
    HANDLE hev = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hev)
    {
        *pEventMultiSem = (RTSEMEVENTMULTI)(void *)hev;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Close semaphore handle.
     */
    if (CloseHandle(SEM2HND(EventMultiSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy EventMultiSem %p failed, lasterr=%d\n", EventMultiSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Signal the object.
     */
    if (SetEvent(SEM2HND(EventMultiSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Signaling EventMultiSem %p failed, lasterr=%d\n", EventMultiSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Reset the object.
     */
    if (ResetEvent(SEM2HND(EventMultiSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Resetting EventMultiSem %p failed, lasterr=%d\n", EventMultiSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc = WaitForSingleObjectEx(SEM2HND(EventMultiSem), cMillies == RT_INDEFINITE_WAIT ? INFINITE : cMillies, TRUE);
    switch (rc)
    {
        case WAIT_OBJECT_0:         return VINF_SUCCESS;
        case WAIT_TIMEOUT:          return VERR_TIMEOUT;
        case WAIT_IO_COMPLETION:    return VERR_INTERRUPTED;
        case WAIT_ABANDONED:        return VERR_SEM_OWNER_DIED;
        default:
        {
            AssertMsgFailed(("Wait on EventMultiSem %p failed, rc=%d lasterr=%d\n", EventMultiSem, rc, GetLastError()));
            int rc2 = RTErrConvertFromWin32(GetLastError());
            if (rc2)
                return rc2;

            AssertMsgFailed(("WaitForSingleObject(event) -> rc=%d while converted lasterr=%d\n", rc, rc2));
            return VERR_INTERNAL_ERROR;
        }
    }
}




RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    /*
     * Create the semaphore.
     */
    HANDLE hmtx = CreateMutex(NULL, FALSE, NULL);
    if (hmtx)
    {
        *pMutexSem = (RTSEMMUTEX)(void *)hmtx;
        return VINF_SUCCESS;
    }

    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Close semaphore handle.
     */
    if (CloseHandle(SEM2HND(MutexSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy MutexSem %p failed, lasterr=%d\n", MutexSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}


RTDECL(int)  RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    /*
     * Lock mutex semaphore.
     */
    int rc = WaitForSingleObjectEx(SEM2HND(MutexSem), cMillies == RT_INDEFINITE_WAIT ? INFINITE : cMillies, TRUE);
    switch (rc)
    {
        case WAIT_OBJECT_0:         return VINF_SUCCESS;
        case WAIT_TIMEOUT:          return VERR_TIMEOUT;
        case WAIT_IO_COMPLETION:    return VERR_INTERRUPTED;
        case WAIT_ABANDONED:        return VERR_SEM_OWNER_DIED;
        default:
        {
            AssertMsgFailed(("Wait on MutexSem %p failed, rc=%d lasterr=%d\n", MutexSem, rc, GetLastError()));
            int rc2 = RTErrConvertFromWin32(GetLastError());
            if (rc2 != 0)
                return rc2;

            AssertMsgFailed(("WaitForSingleObject(event) -> rc=%d while converted lasterr=%d\n", rc, rc2));
            return VERR_INTERNAL_ERROR;
        }
    }
}

RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Unlock mutex semaphore.
     */
    if (ReleaseMutex(SEM2HND(MutexSem)))
        return VINF_SUCCESS;
    AssertMsgFailed(("Release MutexSem %p failed, lasterr=%d\n", MutexSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}



