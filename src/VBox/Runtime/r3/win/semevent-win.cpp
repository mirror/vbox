/* $Id$ */
/** @file
 * IPRT - Event Sempahore, Windows.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/lockvalidator.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include "internal/magics.h"
#include "internal/strict.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
struct RTSEMEVENTINTERNAL
{
    /** Magic value (RTSEMEVENT_MAGIC). */
    uint32_t            u32Magic;
    /** The event handle. */
    HANDLE              hev;
#ifdef RTSEMEVENT_STRICT
    /** Signallers. */
    RTLOCKVALRECSHRD    Signallers;
    /** Indicates that lock validation should be performed. */
    bool volatile       fEverHadSignallers;
#endif
};


/* Undefine debug mappings. */
#undef RTSemEventWait
#undef RTSemEventWaitNoResume


RTDECL(int)   RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    struct RTSEMEVENTINTERNAL *pThis = (struct RTSEMEVENTINTERNAL *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    /*
     * Create the semaphore.
     * (Auto reset, not signaled, private event object.)
     */
    pThis->hev = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (pThis->hev != NULL) /* not INVALID_HANDLE_VALUE */
    {
        pThis->u32Magic = RTSEMEVENT_MAGIC;
#ifdef RTSEMEVENT_STRICT
        RTLockValidatorRecSharedInit(&pThis->Signallers,
                                     NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_ANY,
                                     "RTSemEvent", pThis, true /*fSignaller*/);
        pThis->fEverHadSignallers = false;
#endif

        *pEventSem = pThis;
        return VINF_SUCCESS;
    }

    DWORD dwErr = GetLastError();
    RTMemFree(pThis);
    return RTErrConvertFromWin32(dwErr);
}


RTDECL(int)   RTSemEventDestroy(RTSEMEVENT EventSem)
{
    struct RTSEMEVENTINTERNAL *pThis = EventSem;
    if (pThis == NIL_RTSEMEVENT)        /* don't bitch */
        return VERR_INVALID_HANDLE;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Invalidate the handle and close the semaphore.
     */
    int rc = VINF_SUCCESS;
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, ~RTSEMEVENT_MAGIC, RTSEMEVENT_MAGIC), VERR_INVALID_HANDLE);
    if (CloseHandle(pThis->hev))
    {
#ifdef RTSEMEVENT_STRICT
        RTLockValidatorRecSharedDelete(&pThis->Signallers);
#endif
        RTMemFree(pThis);
    }
    else
    {
        DWORD dwErr = GetLastError();
        rc = RTErrConvertFromWin32(dwErr);
        AssertMsgFailed(("Destroy EventSem %p failed, lasterr=%u (%Rrc)\n", pThis, dwErr, rc));
        /* Leak it. */
    }

    return rc;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pThis = EventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, VERR_INVALID_HANDLE);

#ifdef RTSEMEVENT_STRICT
    if (pThis->fEverHadSignallers)
    {
        int rc9 = RTLockValidatorRecSharedCheckSignaller(&pThis->Signallers, NIL_RTTHREAD);
        if (RT_FAILURE(rc9))
            return rc9;
    }
#endif

    /*
     * Signal the object.
     */
    if (SetEvent(pThis->hev))
        return VINF_SUCCESS;
    DWORD dwErr = GetLastError();
    AssertMsgFailed(("Signaling EventSem %p failed, lasterr=%d\n", pThis, dwErr));
    return RTErrConvertFromWin32(dwErr);
}


/** Goto avoidance. */
DECL_FORCE_INLINE(int) rtSemEventWaitHandleStatus(struct RTSEMEVENTINTERNAL *pThis, DWORD rc)
{
    switch (rc)
    {
        case WAIT_OBJECT_0:         return VINF_SUCCESS;
        case WAIT_TIMEOUT:          return VERR_TIMEOUT;
        case WAIT_IO_COMPLETION:    return VERR_INTERRUPTED;
        case WAIT_ABANDONED:        return VERR_SEM_OWNER_DIED;
        default:
            AssertMsgFailed(("%u\n", rc));
        case WAIT_FAILED:
        {
            int rc2 = RTErrConvertFromWin32(GetLastError());
            AssertMsgFailed(("Wait on EventSem %p failed, rc=%d lasterr=%d\n", pThis, rc, GetLastError()));
            if (rc2)
                return rc2;

            AssertMsgFailed(("WaitForSingleObject(event) -> rc=%d while converted lasterr=%d\n", rc, rc2));
            return VERR_INTERNAL_ERROR;
        }
    }
}


RTDECL(int)   RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    PCRTLOCKVALSRCPOS pSrcPos = NULL;

    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pThis = EventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Wait for condition.
     */
#ifdef RTSEMEVENT_STRICT
    RTTHREAD hThreadSelf = RTThreadSelfAutoAdopt();
    if (pThis->fEverHadSignallers)
    {
        DWORD rc = WaitForSingleObjectEx(pThis->hev,
                                         0 /*Timeout*/,
                                         TRUE /*fAlertable*/);
        if (rc != WAIT_TIMEOUT || cMillies == 0)
            return rtSemEventWaitHandleStatus(pThis, rc);
        int rc9 = RTLockValidatorRecSharedCheckBlocking(&pThis->Signallers, hThreadSelf, pSrcPos, false,
                                                        RTTHREADSTATE_EVENT, true);
        if (RT_FAILURE(rc9))
            return rc9;
    }
#else
    RTTHREAD hThreadSelf = RTThreadSelf();
#endif
    RTThreadBlocking(hThreadSelf, RTTHREADSTATE_EVENT, true);
    DWORD rc = WaitForSingleObjectEx(pThis->hev,
                                     cMillies == RT_INDEFINITE_WAIT ? INFINITE : cMillies,
                                     TRUE /*fAlertable*/);
    RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_EVENT);
    return rtSemEventWaitHandleStatus(pThis, rc);
}


RTDECL(void) RTSemEventSetSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{
#ifdef RTSEMEVENT_STRICT
    struct RTSEMEVENTINTERNAL *pThis = hEventSem;
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == RTSEMEVENT_MAGIC);

    ASMAtomicWriteBool(&pThis->fEverHadSignallers, true);
    RTLockValidatorRecSharedResetOwner(&pThis->Signallers, hThread, NULL);
#endif
}


RTDECL(void) RTSemEventAddSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{
#ifdef RTSEMEVENT_STRICT
    struct RTSEMEVENTINTERNAL *pThis = hEventSem;
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == RTSEMEVENT_MAGIC);

    ASMAtomicWriteBool(&pThis->fEverHadSignallers, true);
    RTLockValidatorRecSharedAddOwner(&pThis->Signallers, hThread, NULL);
#endif
}


RTDECL(void) RTSemEventRemoveSignaller(RTSEMEVENT hEventSem, RTTHREAD hThread)
{
#ifdef RTSEMEVENT_STRICT
    struct RTSEMEVENTINTERNAL *pThis = hEventSem;
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == RTSEMEVENT_MAGIC);

    RTLockValidatorRecSharedRemoveOwner(&pThis->Signallers, hThread);
#endif
}

