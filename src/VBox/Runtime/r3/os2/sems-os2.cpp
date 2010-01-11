/* $Id$ */
/** @file
 * IPRT - Semaphores, OS/2.
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



RTDECL(int)  RTSemEventCreate(PRTSEMEVENT phEventSem)
{
    return RTSemEventCreateEx(phEventSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventCreateEx(PRTSEMEVENT phEventSem, uint32_t fFlags, RTLOCKVALCLASS hClass, const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMEVENT_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);

    /*
     * Create the semaphore.
     * (Auto reset, not signaled, private event object.)
     */
    HEV hev;
    int rc = DosCreateEventSem(NULL, &hev, DCE_AUTORESET | DCE_POSTONE, 0);
    if (!rc)
    {
        *phEventSem = (RTSEMEVENT)(void *)hev;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)   RTSemEventDestroy(RTSEMEVENT hEventSem)
{
    if (hEventSem == NIL_RTSEMEVENT)
        return VINF_SUCCESS;

    /*
     * Close semaphore handle.
     */
    int rc = DosCloseEventSem(SEM2HND(hEventSem));
    if (!rc)
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy hEventSem %p failed, rc=%d\n", hEventSem, rc));
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)   RTSemEventWaitNoResume(RTSEMEVENT hEventSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc = DosWaitEventSem(SEM2HND(hEventSem), cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies);
    switch (rc)
    {
        case NO_ERROR:              return VINF_SUCCESS;
        case ERROR_SEM_TIMEOUT:
        case ERROR_TIMEOUT:         return VERR_TIMEOUT;
        case ERROR_INTERRUPT:       return VERR_INTERRUPTED;
        default:
        {
            AssertMsgFailed(("Wait on hEventSem %p failed, rc=%d\n", hEventSem, rc));
            return RTErrConvertFromOS2(rc);
        }
    }
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT hEventSem)
{
    /*
     * Signal the object.
     */
    int rc = DosPostEventSem(SEM2HND(hEventSem));
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


RTDECL(void) RTSemEventSetSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{
/** @todo implement RTSemEventSetSignaller and friends for OS/2 */
}


RTDECL(void) RTSemEventAddSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{

}


RTDECL(void) RTSemEventRemoveSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{

}




RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI phEventMultiSem)
{
    return RTSemEventMultiCreateEx(phEventMultiSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventMultiCreateEx(PRTSEMEVENTMULTI phEventMultiSem, uint32_t fFlags, RTLOCKVALCLASS hClass,
                                     const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMEVENTMULTI_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);

    /*
     * Create the semaphore.
     * (Manual reset, not signaled, private event object.)
     */
    HEV hev;
    int rc = DosCreateEventSem(NULL, &hev, 0, FALSE);
    if (!rc)
    {
        *phEventMultiSem = (RTSEMEVENTMULTI)(void *)hev;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI hEventMultiSem)
{
    if (hEventMultiSem == NIL_RTSEMEVENTMULTI)
        return VINF_SUCCESS;

    /*
     * Close semaphore handle.
     */
    int rc = DosCloseEventSem(SEM2HND(hEventMultiSem));
    if (!rc)
        return VINF_SUCCESS;
    AssertMsgFailed(("Destroy hEventMultiSem %p failed, rc=%d\n", hEventMultiSem, rc));
    return RTErrConvertFromOS2(rc);
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI hEventMultiSem)
{
    /*
     * Signal the object.
     */
    int rc = DosPostEventSem(SEM2HND(hEventMultiSem));
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


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI hEventMultiSem)
{
    /*
     * Reset the object.
     */
    ULONG ulIgnore;
    int rc = DosResetEventSem(SEM2HND(hEventMultiSem), &ulIgnore);
    switch (rc)
    {
        case NO_ERROR:
        case ERROR_ALREADY_RESET:
            return VINF_SUCCESS;
        default:
            return RTErrConvertFromOS2(rc);
    }
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc = DosWaitEventSem(SEM2HND(hEventMultiSem), cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies);
    switch (rc)
    {
        case NO_ERROR:              return VINF_SUCCESS;
        case ERROR_SEM_TIMEOUT:
        case ERROR_TIMEOUT:         return VERR_TIMEOUT;
        case ERROR_INTERRUPT:       return VERR_INTERRUPTED;
        default:
        {
            AssertMsgFailed(("Wait on hEventMultiSem %p failed, rc=%d\n", hEventMultiSem, rc));
            return RTErrConvertFromOS2(rc);
        }
    }
}




#undef RTSemMutexCreate
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX phMutexSem)
{
    return RTSemMutexCreateEx(phMutexSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_NONE, NULL);
}


RTDECL(int) RTSemMutexCreateEx(PRTSEMMUTEX phMutexSem, uint32_t fFlags,
                               RTLOCKVALCLASS hClass, uint32_t uSubClass, const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMMUTEX_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);

    /*
     * Create the semaphore.
     */
    HMTX hmtx;
    int rc = DosCreateMutexSem(NULL, &hmtx, 0, FALSE);
    if (!rc)
    {
        /** @todo implement lock validation of OS/2 mutex semaphores. */
        *phMutexSem = (RTSEMMUTEX)(void *)hmtx;
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



RTDECL(uint32_t) RTSemMutexSetSubClass(RTSEMMUTEX hMutexSem, uint32_t uSubClass)
{
#if 0 /** @todo def RTSEMMUTEX_STRICT */
    /*
     * Validate.
     */
    RTSEMMUTEXINTERNAL *pThis = hMutexSem;
    AssertPtrReturn(pThis, RTLOCKVAL_SUB_CLASS_INVALID);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, RTLOCKVAL_SUB_CLASS_INVALID);

    return RTLockValidatorRecExclSetSubClass(&pThis->ValidatorRec, uSubClass);
#else
    return RTLOCKVAL_SUB_CLASS_INVALID;
#endif
}


#undef RTSemMutexRequestNoResume
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


RTDECL(bool) RTSemMutexIsOwned(RTSEMMUTEX hMutex);
{
    /*
     * Unlock mutex semaphore.
     */
    PID     pid;
    TID     tid;
    ULONG   cRecursions;
    int rc = DosQueryMutexSem(SEM2HND(MutexSem), &pid, &tid, &cRecursions);
    if (!rc)
        return cRecursions != 0;
    AssertMsgFailed(("DosQueryMutexSem %p failed, rc=%d\n", MutexSem, rc));
    return rc == ERROR_SEM_OWNER_DIED;
}


RTDECL(void) RTSemEventMultiSetSignaller(RTSEMEVENTMULTI hEventMultiSem, RTTHREAD hThread)
{
    /** @todo implement RTSemEventMultiSetSignaller on OS/2 */
}


RTDECL(void) RTSemEventMultiAddSignaller(RTSEMEVENTMULTI hEventMultiSem, RTTHREAD hThread)
{
}


RTDECL(void) RTSemEventMultiRemoveSignaller(RTSEMEVENTMULTI hEventMultiSem, RTTHREAD hThread)
{
}

