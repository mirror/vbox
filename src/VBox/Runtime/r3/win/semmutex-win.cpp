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
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/lockvalidator.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include "internal/magics.h"
#include "internal/strict.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Posix internal representation of a Mutex semaphore. */
struct RTSEMMUTEXINTERNAL
{
    /** Magic value (RTSEMMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** The mutex handle. */
    HANDLE              hMtx;
#ifdef RTSEMMUTEX_STRICT
    /** Lock validator record associated with this mutex. */
    RTLOCKVALRECEXCL    ValidatorRec;
#endif
};


/* Undefine debug mappings. */
#undef RTSemMutexRequest
#undef RTSemMutexRequestNoResume


RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    int rc;

    /*
     * Create the semaphore.
     */
    HANDLE hMtx = CreateMutex(NULL, FALSE, NULL);
    if (hMtx)
    {
        RTSEMMUTEXINTERNAL *pThis = (RTSEMMUTEXINTERNAL *)RTMemAlloc(sizeof(*pThis));
        if (pThis)
        {
            pThis->u32Magic = RTSEMMUTEX_MAGIC;
            pThis->hMtx = hMtx;
#ifdef RTSEMMUTEX_STRICT
            RTLockValidatorRecExclInit(&pThis->ValidatorRec,  NIL_RTLOCKVALIDATORCLASS, RTLOCKVALIDATOR_SUB_CLASS_NONE, "RTSemMutex", pThis);
#endif
            *pMutexSem = pThis;
            return VINF_SUCCESS;
        }

        rc = VERR_NO_MEMORY;
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
    return rc;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Validate.
     */
    RTSEMMUTEXINTERNAL *pThis = MutexSem;
    if (pThis == NIL_RTSEMMUTEX)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Close semaphore handle.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTSEMMUTEX_MAGIC_DEAD, RTSEMMUTEX_MAGIC), VERR_INVALID_HANDLE);
    HANDLE hMtx = pThis->hMtx;
    ASMAtomicWritePtr((void * volatile *)&pThis->hMtx, (void *)INVALID_HANDLE_VALUE);

    int rc = VINF_SUCCESS;
    if (!CloseHandle(hMtx))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        AssertMsgFailed(("%p rc=%d lasterr=%d\n", pThis->hMtx, rc, GetLastError()));
    }

#ifdef RTSEMMUTEX_STRICT
    RTLockValidatorRecExclDelete(&pThis->ValidatorRec);
#endif
    RTMemFree(pThis);
    return rc;
}


/**
 * Internal worker for RTSemMutexRequestNoResume and it's debug companion.
 *
 * @returns Same as RTSEmMutexRequestNoResume
 * @param   MutexSem            The mutex handle.
 * @param   cMillies            The number of milliseconds to wait.
 * @param   pSrcPos             The source position of the caller.
 */
DECL_FORCE_INLINE(int) rtSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies, PCRTLOCKVALSRCPOS pSrcPos)
{
    /*
     * Validate.
     */
    RTSEMMUTEXINTERNAL *pThis = MutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, VERR_INVALID_HANDLE);

#ifdef RTSEMMUTEX_STRICT
    RTTHREAD hThreadSelf = RTThreadSelfAutoAdopt();
    RTLockValidatorCheckOrder(&pThis->ValidatorRec, hThreadSelf, pSrcPos);
#else
    RTTHREAD hThreadSelf = RTThreadSelf();
#endif

    /*
     * Lock mutex semaphore.
     */
    if (cMillies > 0)
    {
#ifdef RTSEMMUTEX_STRICT
        int rc9 = RTLockValidatorCheckBlocking(&pThis->ValidatorRec, hThreadSelf,
                                               RTTHREADSTATE_MUTEX, true, pSrcPos);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        RTThreadBlocking(hThreadSelf, RTTHREADSTATE_MUTEX);
    }
    int rc = WaitForSingleObjectEx(pThis->hMtx,
                                   cMillies == RT_INDEFINITE_WAIT ? INFINITE : cMillies,
                                   TRUE /*bAlertable*/);
    RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_MUTEX);
    switch (rc)
    {
        case WAIT_OBJECT_0:
#ifdef RTSEMMUTEX_STRICT
            RTLockValidatorSetOwner(&pThis->ValidatorRec, hThreadSelf, pSrcPos);
#endif
            return VINF_SUCCESS;

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


RTDECL(int) RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies)
{
#ifndef RTSEMMUTEX_STRICT
    return rtSemMutexRequestNoResume(MutexSem, cMillies, NULL);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return rtSemMutexRequestNoResume(MutexSem, cMillies, &SrcPos);
#endif
}


RTDECL(int) RTSemMutexRequestNoResumeDebug(RTSEMMUTEX MutexSem, unsigned cMillies, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return rtSemMutexRequestNoResume(MutexSem, cMillies, &SrcPos);
}


RTDECL(int) RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Validate.
     */
    RTSEMMUTEXINTERNAL *pThis = MutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Unlock mutex semaphore.
     */
#ifdef RTSEMMUTEX_STRICT
    if (   pThis->ValidatorRec.hThread != NIL_RTTHREAD
        && pThis->ValidatorRec.hThread == RTThreadSelf())
        RTLockValidatorUnsetOwner(&pThis->ValidatorRec);
    else
        AssertMsgFailed(("%p hThread=%RTthrd\n", pThis, pThis->ValidatorRec.hThread));
#endif
    if (ReleaseMutex(pThis->hMtx))
        return VINF_SUCCESS;
    int rc = RTErrConvertFromWin32(GetLastError());
    AssertMsgFailed(("%p/%p, rc=%Rrc lasterr=%d\n", pThis, pThis->hMtx, rc, GetLastError()));
    return rc;
}

