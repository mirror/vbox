/* $Id$ */
/** @file
 * innotek Portable Runtime - Semaphores, OS/2.
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
#define INCL_DOSSEMAPHORES
#define INCL_ERRORS
#include <os2.h>
#undef RT_MAX

#include <iprt/semaphore.h>
#include <iprt/assert.h>
#include <iprt/err.h>


/** Converts semaphore to OS/2 handle. */
#define SEM2HND(Sem) ((LHANDLE)(uintptr_t)Sem)


RTDECL(int)   RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    /*
     * Create the semaphore.
     * (Auto reset, not signaled, private event object.)
     */
    HEV hev;
    int rc = DosCreateEventSem(NULL, &hev, DCE_AUTORESET | DCE_POSTONE, 0);
    if (!rc)
    {
        *pEventSem = (RTSEMEVENT)(void *)hev;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)   RTSemEventDestroy(RTSEMEVENT EventSem)
{
    /*
     * Close semaphore handle.
     */
    int rc = DosCloseEventSem(SEM2HND(EventSem));
    if (!rc)
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy EventSem %p failed, rc=%d\n", EventSem, rc));
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)   RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc = DosWaitEventSem(SEM2HND(EventSem), cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies);
    switch (rc)
    {
        case NO_ERROR:              return VINF_SUCCESS;
        case ERROR_SEM_TIMEOUT:
        case ERROR_TIMEOUT:         return VERR_TIMEOUT;
        case ERROR_INTERRUPT:       return VERR_INTERRUPTED;
        default:
        {
            AssertMsgFailed(("Wait on EventSem %p failed, rc=%d\n", EventSem, rc));
            return RTErrConvertFromOS2(rc);
        }
    }
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Signal the object.
     */
    int rc = DosPostEventSem(SEM2HND(EventSem));
    switch (rc)
    {
        case NO_ERROR:
        case ERROR_ALREADY_POSTED:
        case ERROR_TOO_MANY_POSTS:
            return VINF_SUCCESS;
        default:
            return RTErrConvertFromOS2(rc);
    }
}




RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    /*
     * Create the semaphore.
     * (Manual reset, not signaled, private event object.)
     */
    HEV hev;
    int rc = DosCreateEventSem(NULL, &hev, 0, FALSE);
    if (!rc)
    {
        *pEventMultiSem = (RTSEMEVENTMULTI)(void *)hev;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Close semaphore handle.
     */
    int rc = DosCloseEventSem(SEM2HND(EventMultiSem));
    if (!rc)
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy EventMultiSem %p failed, rc=%d\n", EventMultiSem, rc));
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Signal the object.
     */
    int rc = DosPostEventSem(SEM2HND(EventMultiSem));
    switch (rc)
    {
        case NO_ERROR:
        case ERROR_ALREADY_POSTED:
        case ERROR_TOO_MANY_POSTS:
            return VINF_SUCCESS;
        default:
            return RTErrConvertFromOS2(rc);
    }
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Reset the object.
     */
    ULONG ulIgnore;
    int rc = DosResetEventSem(SEM2HND(EventMultiSem), &ulIgnore);
    switch (rc)
    {
        case NO_ERROR:
        case ERROR_ALREADY_RESET:
            return VINF_SUCCESS;
        default:
            return RTErrConvertFromOS2(rc);
    }
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc = DosWaitEventSem(SEM2HND(EventMultiSem), cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies);
    switch (rc)
    {
        case NO_ERROR:              return VINF_SUCCESS;
        case ERROR_SEM_TIMEOUT:
        case ERROR_TIMEOUT:         return VERR_TIMEOUT;
        case ERROR_INTERRUPT:       return VERR_INTERRUPTED;
        default:
        {
            AssertMsgFailed(("Wait on EventMultiSem %p failed, rc=%d\n", EventMultiSem, rc));
            return RTErrConvertFromOS2(rc);
        }
    }
}




RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    /*
     * Create the semaphore.
     */
    HMTX hmtx;
    int rc = DosCreateMutexSem(NULL, &hmtx, 0, FALSE);
    if (!rc)
    {
        *pMutexSem = (RTSEMMUTEX)(void *)hmtx;
        return VINF_SUCCESS;
    }

    return RTErrConvertFromOS2(rc);
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Close semaphore handle.
     */
    int rc = DosCloseMutexSem(SEM2HND(MutexSem));
    if (!rc)
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy MutexSem %p failed, rc=%d\n", MutexSem, rc));
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)  RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    /*
     * Lock mutex semaphore.
     */
    int rc = DosRequestMutexSem(SEM2HND(MutexSem), cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies);
    switch (rc)
    {
        case NO_ERROR:              return VINF_SUCCESS;
        case ERROR_SEM_TIMEOUT:
        case ERROR_TIMEOUT:         return VERR_TIMEOUT;
        case ERROR_INTERRUPT:       return VERR_INTERRUPTED;
        case ERROR_SEM_OWNER_DIED:  return VERR_SEM_OWNER_DIED;
        default:
        {
            AssertMsgFailed(("Wait on MutexSem %p failed, rc=%d\n", MutexSem, rc));
            return RTErrConvertFromOS2(rc);
        }
    }
}

RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Unlock mutex semaphore.
     */
    int rc = DosReleaseMutexSem(SEM2HND(MutexSem));
    if (!rc)
        return VINF_SUCCESS;
    AssertMsgFailed(("Release MutexSem %p failed, rc=%d\n", MutexSem, rc));
    return RTErrConvertFromOS2(rc);
}



