/* $Id$ */
/** @file
 * IPRT - Mutex Semaphores, Windows.
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
#undef RTSemMutexRequest
#undef RTSemMutexRequestNoResume


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
        case WAIT_OBJECT_0:
        {
#ifdef RTSEMMUTEX_STRICT
            RTTHREAD Thread = RTThreadSelf();
            if (Thread != NIL_RTTHREAD)
                RTThreadWriteLockInc(Thread);
#endif
            return VINF_SUCCESS;
        }

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


RTDECL(int)  RTSemMutexRequestNoResumeDebug(RTSEMMUTEX MutexSem, unsigned cMillies, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    return RTSemMutexRequestNoResume(MutexSem, cMillies);
}


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Unlock mutex semaphore.
     */
#ifdef RTSEMMUTEX_STRICT
    RTTHREAD Thread = RTThreadSelf();
    if (Thread != NIL_RTTHREAD)
        RTThreadWriteLockDec(Thread);
#endif
    if (ReleaseMutex(SEM2HND(MutexSem)))
        return VINF_SUCCESS;

#ifdef RTSEMMUTEX_STRICT
    if (Thread != NIL_RTTHREAD)
        RTThreadWriteLockInc(Thread);
#endif
    AssertMsgFailed(("Release MutexSem %p failed, lasterr=%d\n", MutexSem, GetLastError()));
    return RTErrConvertFromWin32(GetLastError());
}

