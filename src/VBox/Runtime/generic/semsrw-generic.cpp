/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Read-Write Semaphore, Generic.
 *
 * This is a generic implementation for OSes which don't have
 * native RW semaphores.
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

/** @todo fix generic RW sems. (reimplement) */
#define USE_CRIT_SECT


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#ifdef USE_CRIT_SECT
#include <iprt/critsect.h>
#endif



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Internal representation of a Read-Write semaphore for the
 * Generic implementation. */
struct RTSEMRWINTERNAL
{
#ifdef USE_CRIT_SECT
    /** Critical section. */
    RTCRITSECT  CritSect;
#else
    /** Magic (RTSEMRW_MAGIC). */
    uint32_t                u32Magic;
    /** This critical section serializes the access to and updating of the structure members. */
    RTCRITSECT              CritSect;
    /** The current number of readers. */
    uint32_t                cReaders;
    /** The number of readers waiting. */
    uint32_t                cReadersWaiting;
    /** The current number of waiting writers. */
    uint32_t                cWritersWaiting;
    /** The handle of the event object on which the waiting readers block. (manual reset). */
    RTSEMEVENTMULTI         EventReaders;
    /** The handle of the event object on which the waiting writers block. (manual reset). */
    RTSEMEVENTMULTI         EventWriters;
    /** The current state of the read-write lock. */
    KPRF_TYPE(,RWLOCKSTATE) enmState;

#endif
};


/**
 * Validate a read-write semaphore handle passed to one of the interface.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pIntRWSem   Pointer to the read-write semaphore to validate.
 */
inline bool rtsemRWValid(struct RTSEMRWINTERNAL *pIntRWSem)
{
    if (!VALID_PTR(pIntRWSem))
        return false;

#ifdef USE_CRIT_SECT
    if (pIntRWSem->CritSect.u32Magic != RTCRITSECT_MAGIC)
        return false;
#else
    if (pIntRWSem->u32Check != (uint32_t)~0)
        return false;
#endif
    return true;
}


RTDECL(int)   RTSemRWCreate(PRTSEMRW pRWSem)
{
    int rc;

    /*
     * Allocate memory.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = (struct RTSEMRWINTERNAL *)RTMemAlloc(sizeof(struct RTSEMRWINTERNAL));
    if (pIntRWSem)
    {
#ifdef USE_CRIT_SECT
        rc = RTCritSectInit(&pIntRWSem->CritSect);
        if (RT_SUCCESS(rc))
        {
            *pRWSem = pIntRWSem;
            return VINF_SUCCESS;
        }
#else
        /*
         * Create the semaphores.
         */
        rc = RTSemEventCreate(&pIntRWSem->WriteEvent);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventMultiCreate(&pIntRWSem->ReadEvent);
            if (RT_SUCCESS(rc))
            {
                rc = RTSemMutexCreate(&pIntRWSem->Mutex);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Signal the read semaphore and initialize other variables.
                     */
                    rc = RTSemEventMultiSignal(pIntRWSem->ReadEvent);
                    if (RT_SUCCESS(rc))
                    {
                        pIntRWSem->cReaders = 0;
                        pIntRWSem->cWriters = 0;
                        pIntRWSem->WROwner  = NIL_RTTHREAD;
                        pIntRWSem->u32Check = ~0;
                        *pRWSem = pIntRWSem;
                        return VINF_SUCCESS;
                    }
                    RTSemMutexDestroy(pIntRWSem->Mutex);
                }
                RTSemEventMultiDestroy(pIntRWSem->ReadEvent);
            }
            RTSemEventDestroy(pIntRWSem->WriteEvent);
        }
#endif
        RTMemFree(pIntRWSem);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int)   RTSemRWDestroy(RTSEMRW RWSem)
{
    /*
     * Validate handle.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

#ifdef USE_CRIT_SECT
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    int rc = RTCritSectDelete(&pIntRWSem->CritSect);
    if (RT_SUCCESS(rc))
        RTMemFree(pIntRWSem);
    return rc;
#else

    /*
     * Check if busy.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    int rc = RTSemMutexRequest(pIntRWSem->Mutex, 32);
    if (RT_SUCCESS(rc))
    {
        if (!pIntRWSem->cReaders && !pIntRWSem->cWriters)
        {
            /*
             * Make it invalid and unusable.
             */
            ASMAtomicXchgU32(&pIntRWSem->u32Check, 0);
            ASMAtomicXchgU32(&pIntRWSem->cReaders, ~0);

            /*
             * Do actual cleanup.
             * None of these can now fail excep for the mutex which
             * can be a little bit busy.
             */
            rc = RTSemEventMultiDestroy(pIntRWSem->ReadEvent);
            AssertMsg(RT_SUCCESS(rc), ("RTSemEventMultiDestroy failed! rc=%d\n", rc)); NOREF(rc);
            pIntRWSem->ReadEvent = NIL_RTSEMEVENTMULTI;

            rc = RTSemEventDestroy(pIntRWSem->WriteEvent);
            AssertMsg(RT_SUCCESS(rc), ("RTSemEventDestroy failed! rc=%d\n", rc)); NOREF(rc);
            pIntRWSem->WriteEvent = NIL_RTSEMEVENT;

            RTSemMutexRelease(pIntRWSem->Mutex);
            for (unsigned i = 32; i > 0; i--)
            {
                rc = RTSemMutexDestroy(pIntRWSem->Mutex);
                if (RT_SUCCESS(rc))
                    break;
                RTThreadSleep(1);
            }
            AssertMsg(RT_SUCCESS(rc), ("RTSemMutexDestroy failed! rc=%d\n", rc)); NOREF(rc);
            pIntRWSem->Mutex = (RTSEMMUTEX)0;

            RTMemFree(pIntRWSem);
            rc = VINF_SUCCESS;
        }
        else
        {
            rc = VERR_SEM_BUSY;
            RTSemMutexRelease(pIntRWSem->Mutex);
        }
    }
    else
        rc = rc == VERR_TIMEOUT ? VERR_SEM_BUSY : rc;

    return VINF_SUCCESS;
#endif
}


RTDECL(int)   RTSemRWRequestRead(RTSEMRW RWSem, unsigned cMillies)
{
    /*
     * Validate handle.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

#ifdef USE_CRIT_SECT
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    return RTCritSectEnter(&pIntRWSem->CritSect);
#else

    /*
     * Take mutex and check if already reader.
     */
    //RTTHREAD    Self = RTThreadSelf();
    RTTHREAD    Self = (RTTHREAD)RTThreadNativeSelf();
    unsigned    cMilliesInitial = cMillies;
    uint64_t    tsStart = 0;
    if (cMillies != RTSEM_INDEFINITE_WAIT)
        tsStart = RTTimeNanoTS();

    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    int rc = RTSemMutexRequest(pIntRWSem->Mutex, RTSEM_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("RTSemMutexRequest failed on rwsem %p, rc=%d\n", RWSem, rc));
        return rc;
    }

    unsigned i = pIntRWSem->cReaders;
    while (i-- > 0)
    {
        if (pIntRWSem->aReaders[i].Thread == Self)
        {
            if (pIntRWSem->aReaders[i].cNesting + 1 < (unsigned)~0)
                pIntRWSem->aReaders[i].cNesting++;
            else
            {
                AssertMsgFailed(("Too many requests for one thread!\n"));
                rc = RTSemMutexRelease(pIntRWSem->Mutex);
                AssertMsg(RT_SUCCESS(rc), ("RTSemMutexRelease failed rc=%d\n", rc));
                return VERR_TOO_MANY_SEM_REQUESTS;
            }
        }
    }


    for (;;)
    {
        /*
         * Check if the stat of the affairs allow read access.
         */
        if (pIntRWSem->u32Check == (uint32_t)~0)
        {
            if (pIntRWSem->cWriters == 0)
            {
                if (pIntRWSem->cReaders < ELEMENTS(pIntRWSem->aReaders))
                {
                    /*
                     * Add ourselves to the list of readers and return.
                     */
                    i = pIntRWSem->cReaders;
                    pIntRWSem->aReaders[i].Thread   = Self;
                    pIntRWSem->aReaders[i].cNesting = 1;
                    ASMAtomicXchgU32(&pIntRWSem->cReaders, i + 1);

                    RTSemMutexRelease(pIntRWSem->Mutex);
                    return VINF_SUCCESS;
                }
                else
                {
                    AssertMsgFailed(("Too many readers! How come we have so many threads!?!\n"));
                    rc = VERR_TOO_MANY_SEM_REQUESTS;
                }
            }
            #if 0 /* any action here shouldn't be necessary */
            else
            {
                rc = RTSemEventMultiReset(pIntRWSem->ReadEvent);
                AssertMsg(RT_SUCCESS(rc), ("RTSemEventMultiReset failed on RWSem %p, rc=%d\n", RWSem, rc));
            }
            #endif
        }
        else
            rc = VERR_SEM_DESTROYED;
        RTSemMutexRelease(pIntRWSem->Mutex);
        if (RT_FAILURE(rc))
            break;


        /*
         * Wait till it's ready for reading.
         */
        if (cMillies != RTSEM_INDEFINITE_WAIT)
        {
            int64_t     tsDelta = RTTimeNanoTS() - tsStart;
            if (tsDelta >= 1000000)
            {
                cMillies = cMilliesInitial - (unsigned)(tsDelta / 1000000);
                if (cMillies > cMilliesInitial)
                    cMillies = cMilliesInitial ? 1 : 0;
            }
        }
        rc = RTSemEventMultiWait(pIntRWSem->ReadEvent, cMillies);
        if (RT_FAILURE(rc))
        {
            AssertMsg(rc == VERR_TIMEOUT, ("RTSemEventMultiWait failed on rwsem %p, rc=%d\n", RWSem, rc));
            break;
        }

        /*
         * Get Mutex.
         */
        rc = RTSemMutexRequest(pIntRWSem->Mutex, RTSEM_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemMutexRequest failed on rwsem %p, rc=%d\n", RWSem, rc));
            break;
        }
    }

    return rc;
#endif
}


RTDECL(int)   RTSemRWRequestReadNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    return RTSemRWRequestRead(RWSem, cMillies);
}


RTDECL(int)   RTSemRWReleaseRead(RTSEMRW RWSem)
{
    /*
     * Validate handle.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

#ifdef USE_CRIT_SECT
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    return RTCritSectLeave(&pIntRWSem->CritSect);
#else

    /*
     * Take Mutex.
     */
    //RTTHREAD Self = RTThreadSelf();
    RTTHREAD Self = (RTTHREAD)RTThreadNativeSelf();
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    int rc = RTSemMutexRequest(pIntRWSem->Mutex, RTSEM_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        unsigned i = pIntRWSem->cReaders;
        while (i-- > 0)
        {
            if (pIntRWSem->aReaders[i].Thread == Self)
            {
                AssertMsg(pIntRWSem->WROwner == NIL_RTTHREAD, ("Impossible! Writers and Readers are exclusive!\n"));

                if (pIntRWSem->aReaders[i].cNesting <= 1)
                {
                    pIntRWSem->aReaders[i] = pIntRWSem->aReaders[pIntRWSem->cReaders - 1];
                    ASMAtomicXchgU32(&pIntRWSem->cReaders, pIntRWSem->cReaders - 1);

                    /* Kick off writers? */
                    if (    pIntRWSem->cWriters > 0
                        &&  pIntRWSem->cReaders == 0)
                    {
                        rc = RTSemEventSignal(pIntRWSem->WriteEvent);
                        AssertMsg(RT_SUCCESS(rc), ("Failed to signal writers on rwsem %p, rc=%d\n", RWSem, rc));
                    }
                }
                else
                    pIntRWSem->aReaders[i].cNesting--;

                RTSemMutexRelease(pIntRWSem->Mutex);
                return VINF_SUCCESS;
            }
        }

        RTSemMutexRelease(pIntRWSem->Mutex);
        rc = VERR_NOT_OWNER;
        AssertMsgFailed(("Not reader of rwsem %p\n", RWSem));
    }
    else
        AssertMsgFailed(("RTSemMutexRequest failed on rwsem %p, rc=%d\n", RWSem, rc));

    return rc;
#endif
}



RTDECL(int) RTSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies)
{
    /*
     * Validate handle.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

#ifdef USE_CRIT_SECT
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    return RTCritSectEnter(&pIntRWSem->CritSect);
#else

    /*
     * Get Mutex.
     */
    //RTTHREAD    Self = RTThreadSelf();
    RTTHREAD    Self = (RTTHREAD)RTThreadNativeSelf();
    unsigned    cMilliesInitial = cMillies;
    uint64_t    tsStart = 0;
    if (cMillies != RTSEM_INDEFINITE_WAIT)
        tsStart = RTTimeNanoTS();

    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    int rc = RTSemMutexRequest(pIntRWSem->Mutex, RTSEM_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("RTSemMutexWait failed on rwsem %p, rc=%d\n", RWSem, rc));
        return rc;
    }

    /*
     * Check that we're not a reader.
     */
    unsigned i = pIntRWSem->cReaders;
    while (i-- > 0)
    {
        if (pIntRWSem->aReaders[i].Thread == Self)
        {
            AssertMsgFailed(("Deadlock - requested write access while being a reader! rwsem %p.\n", RWSem));
            RTSemMutexRelease(pIntRWSem->Mutex);
            return VERR_DEADLOCK;
        }
    }


    /*
     * Reset the reader event semaphore and increment number of readers.
     */
    rc = RTSemEventMultiReset(pIntRWSem->ReadEvent);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to reset readers, rwsem %p, rc=%d.\n", RWSem, rc));
        RTSemMutexRelease(pIntRWSem->Mutex);
        return rc;
    }
    ASMAtomicXchgU32(&pIntRWSem->cWriters, pIntRWSem->cWriters + 1);

    /*
     * Wait while there are other threads owning this sem.
     */
    while (     pIntRWSem->WROwner != NIL_RTTHREAD
           ||   pIntRWSem->cReaders > 0)
    {
        AssertMsg(pIntRWSem->WROwner == NIL_RTTHREAD || pIntRWSem->cWriters > 1,
                  ("The lock is write owned by there is only one waiter...\n"));

        /*
         * Release the mutex and wait on the writer semaphore.
         */
        rc = RTSemMutexRelease(pIntRWSem->Mutex);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemMutexRelease failed on rwsem %p, rc=%d\n", RWSem, rc));
            return VERR_SEM_DESTROYED;
        }

        /*
         * Wait.
         */
        if (cMillies != RTSEM_INDEFINITE_WAIT)
        {
            int64_t     tsDelta = RTTimeNanoTS() - tsStart;
            if (tsDelta >= 1000000)
            {
                cMillies = cMilliesInitial - (unsigned)(tsDelta / 1000000);
                if (cMillies > cMilliesInitial)
                    cMillies = cMilliesInitial ? 1 : 0;
            }
        }
        rc = RTSemEventWait(pIntRWSem->WriteEvent, cMillies);

        /*
         * Check that the semaphore wasn't destroyed while we waited.
         */
        if (    rc == VERR_SEM_DESTROYED
            ||  pIntRWSem->u32Check != (uint32_t)~0)
            return VERR_SEM_DESTROYED;

        /*
         * Attempt take the mutex.
         */
        int rc2 = RTSemMutexRequest(pIntRWSem->Mutex, RTSEM_INDEFINITE_WAIT);
        if (RT_FAILURE(rc) || RT_FAILURE(rc2))
        {
            AssertMsg(RT_SUCCESS(rc2), ("RTSemMutexRequest failed on rwsem %p, rc=%d\n", RWSem, rc2));
            if (RT_SUCCESS(rc))
                rc = rc2;
            else
                AssertMsg(rc == VERR_TIMEOUT, ("RTSemEventWait failed on rwsem %p, rc=%d\n", RWSem, rc));

            /*
             * Remove our selves from the writers queue.
             */
            /** @todo write an atomic dec function! (it's too late for that kind of stuff tonight) */
            if (pIntRWSem->cWriters > 0)
                ASMAtomicXchgU32(&pIntRWSem->cWriters, pIntRWSem->cWriters - 1);
            if (!pIntRWSem->cWriters)
                RTSemEventMultiSignal(pIntRWSem->ReadEvent);
            if (RT_SUCCESS(rc2))
                RTSemMutexRelease(pIntRWSem->Mutex);
            return rc;
        }

        AssertMsg(pIntRWSem->WROwner == NIL_RTTHREAD, ("We woke up an there is owner! %#x\n", pIntRWSem->WROwner));
        AssertMsg(!pIntRWSem->cReaders, ("We woke up an there are readers around!\n"));
    }

    /*
     * If we get here we own the mutex and we are ready to take
     * the read-write ownership.
     */
    ASMAtomicXchgPtr((void * volatile *)&pIntRWSem->WROwner, (void *)Self);
    rc = RTSemMutexRelease(pIntRWSem->Mutex);
    AssertMsg(RT_SUCCESS(rc), ("RTSemMutexRelease failed. rc=%d\n", rc)); NOREF(rc);

    return VINF_SUCCESS;
#endif
}


RTDECL(int) RTSemRWRequestWriteNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    return RTSemRWRequestWrite(RWSem, cMillies);
}



RTDECL(int)   RTSemRWReleaseWrite(RTSEMRW RWSem)
{
    /*
     * Validate handle.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

#ifdef USE_CRIT_SECT
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    return RTCritSectLeave(&pIntRWSem->CritSect);
#else

    /*
     * Check if owner.
     */
    //RTTHREAD  Self = RTThreadSelf();
    RTTHREAD  Self = (RTTHREAD)RTThreadNativeSelf();
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    if (pIntRWSem->WROwner != Self)
    {
        AssertMsgFailed(("Not read-write owner of rwsem %p.\n", RWSem));
        return VERR_NOT_OWNER;
    }

    /*
     * Request the mutex.
     */
    int rc = RTSemMutexRequest(pIntRWSem->Mutex, RTSEM_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("RTSemMutexWait failed on rwsem %p, rc=%d\n", RWSem, rc));
        return rc;
    }

    /*
     * Release ownership and remove ourselves from the writers count.
     */
    ASMAtomicXchgPtr((void * volatile *)&pIntRWSem->WROwner, (void *)NIL_RTTHREAD);
    Assert(pIntRWSem->cWriters > 0);
    ASMAtomicXchgU32(&pIntRWSem->cWriters, pIntRWSem->cWriters - 1);

    /*
     * Release the readers if no more writers.
     */
    if (!pIntRWSem->cWriters)
    {
        rc = RTSemEventMultiSignal(pIntRWSem->ReadEvent);
        AssertMsg(RT_SUCCESS(rc), ("RTSemEventMultiSignal failed for rwsem %p, rc=%d.\n", RWSem, rc)); NOREF(rc);
    }
    rc = RTSemMutexRelease(pIntRWSem->Mutex);
    AssertMsg(RT_SUCCESS(rc), ("RTSemEventMultiSignal failed for rwsem %p, rc=%d.\n", RWSem, rc)); NOREF(rc);

    return VINF_SUCCESS;
#endif
}

