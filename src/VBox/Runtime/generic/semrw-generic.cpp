/* $Id$ */
/** @file
 * IPRT - Read-Write Semaphore, Generic.
 *
 * This is a generic implementation for OSes which don't have
 * native RW semaphores.
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
#include <iprt/semaphore.h>
#include "internal/iprt.h"

#include <iprt/critsect.h>
#include <iprt/alloc.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/stream.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** Internal representation of a Read-Write semaphore for the
 * Generic implementation. */
struct RTSEMRWINTERNAL
{
    /** The usual magic. (RTSEMRW_MAGIC) */
    uint32_t            u32Magic;
    /* Alignment padding. */
    uint32_t            u32Padding;
    /** This critical section serializes the access to and updating of the structure members. */
    RTCRITSECT          CritSect;
    /** The current number of reads. (pure read recursion counts too) */
    uint32_t            cReads;
    /** The current number of writes. (recursion counts too) */
    uint32_t            cWrites;
    /** Number of read recursions by the writer. */
    uint32_t            cWriterReads;
    /** Number of writers waiting. */
    uint32_t            cWritesWaiting;
    /** The write owner of the lock. */
    RTTHREAD            Writer;
    /** The handle of the event object on which the waiting readers block. (manual reset). */
    RTSEMEVENTMULTI     ReadEvent;
    /** The handle of the event object on which the waiting writers block. (automatic reset). */
    RTSEMEVENT          WriteEvent;
    /** Need to reset ReadEvent. */
    bool                fNeedResetReadEvent;
};



RTDECL(int) RTSemRWCreate(PRTSEMRW pRWSem)
{
    int rc;

    /*
     * Allocate memory.
     */
    struct RTSEMRWINTERNAL *pThis = (struct RTSEMRWINTERNAL *)RTMemAlloc(sizeof(struct RTSEMRWINTERNAL));
    if (pThis)
    {
        /*
         * Create the semaphores.
         */
        rc = RTSemEventCreate(&pThis->WriteEvent);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventMultiCreate(&pThis->ReadEvent);
            if (RT_SUCCESS(rc))
            {
                rc = RTCritSectInit(&pThis->CritSect);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Signal the read semaphore and initialize other variables.
                     */
                    rc = RTSemEventMultiSignal(pThis->ReadEvent);
                    if (RT_SUCCESS(rc))
                    {
                        pThis->u32Padding           = UINT32_C(0xa5a55a5a);
                        pThis->cReads               = 0;
                        pThis->cWrites              = 0;
                        pThis->cWriterReads         = 0;
                        pThis->cWritesWaiting       = 0;
                        pThis->Writer               = NIL_RTTHREAD;
                        pThis->fNeedResetReadEvent  = true;
                        pThis->u32Magic             = RTSEMRW_MAGIC;
                        *pRWSem = pThis;
                        return VINF_SUCCESS;
                    }
                    RTCritSectDelete(&pThis->CritSect);
                }
                RTSemEventMultiDestroy(pThis->ReadEvent);
            }
            RTSemEventDestroy(pThis->WriteEvent);
        }
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}
RT_EXPORT_SYMBOL(RTSemRWCreate);


RTDECL(int) RTSemRWDestroy(RTSEMRW RWSem)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    if (pThis == NIL_RTSEMRW)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Check if busy.
     */
    int rc = RTCritSectTryEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (!pThis->cReads && !pThis->cWrites)
        {
            /*
             * Make it invalid and unusable.
             */
            ASMAtomicWriteU32(&pThis->u32Magic, ~RTSEMRW_MAGIC);
            pThis->cReads = ~0;

            /*
             * Do actual cleanup. None of these can now fail.
             */
            rc = RTSemEventMultiDestroy(pThis->ReadEvent);
            AssertMsgRC(rc, ("RTSemEventMultiDestroy failed! rc=%Rrc\n", rc));
            pThis->ReadEvent = NIL_RTSEMEVENTMULTI;

            rc = RTSemEventDestroy(pThis->WriteEvent);
            AssertMsgRC(rc, ("RTSemEventDestroy failed! rc=%Rrc\n", rc));
            pThis->WriteEvent = NIL_RTSEMEVENT;

            RTCritSectLeave(&pThis->CritSect);
            rc = RTCritSectDelete(&pThis->CritSect);
            AssertMsgRC(rc, ("RTCritSectDelete failed! rc=%Rrc\n", rc));

            RTMemFree(pThis);
            rc = VINF_SUCCESS;
        }
        else
        {
            rc = VERR_SEM_BUSY;
            RTCritSectLeave(&pThis->CritSect);
        }
    }
    else
    {
        AssertMsgRC(rc, ("RTCritSectTryEnter failed! rc=%Rrc\n", rc));
        rc = VERR_SEM_BUSY;
    }

    return rc;
}
RT_EXPORT_SYMBOL(RTSemRWDestroy);


RTDECL(int) RTSemRWRequestRead(RTSEMRW RWSem, unsigned cMillies)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    RTTHREAD    Self = (RTTHREAD)RTThreadNativeSelf();
    unsigned    cMilliesInitial = cMillies;
    uint64_t    tsStart = 0;
    if (cMillies != RT_INDEFINITE_WAIT)
        tsStart = RTTimeNanoTS();

    /*
     * Take critsect.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("RTCritSectEnter failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
        return rc;
    }

    for (;;)
    {
        /*
         * Check if the state of affairs allows read access.
         * Do not block further readers if there is a writer waiting, as
         * that will break/deadlock reader recursion.
         */
        if (!pThis->cWrites
            /** @todo && (   pThis->cReads
             *            || !pThis->cWritesWaiting) ?? - making sure not to race waiting
             *        writers.  */
           )
        {
            pThis->cReads++;

            RTCritSectLeave(&pThis->CritSect);
            return VINF_SUCCESS;
        }

        if (pThis->Writer == Self)
        {
            pThis->cWriterReads++;

            RTCritSectLeave(&pThis->CritSect);
            return VINF_SUCCESS;
        }

        RTCritSectLeave(&pThis->CritSect);

        /*
         * Wait till it's ready for reading.
         */
        if (cMillies != RT_INDEFINITE_WAIT)
        {
            int64_t tsDelta = RTTimeNanoTS() - tsStart;
            if (tsDelta >= 1000000)
            {
                cMillies = cMilliesInitial - (unsigned)(tsDelta / 1000000);
                if (cMillies > cMilliesInitial)
                    cMillies = cMilliesInitial ? 1 : 0;
            }
        }
        rc = RTSemEventMultiWait(pThis->ReadEvent, cMillies);
        if (RT_FAILURE(rc) && rc != VERR_TIMEOUT)
        {
            AssertMsgRC(rc, ("RTSemEventMultiWait failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
            break;
        }

        if (pThis->u32Magic != RTSEMRW_MAGIC)
        {
            rc = VERR_SEM_DESTROYED;
            break;
        }

        /*
         * Re-take critsect.
         */
        rc = RTCritSectEnter(&pThis->CritSect);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTCritSectEnter failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
            break;
        }
    }

    return rc;
}
RT_EXPORT_SYMBOL(RTSemRWRequestRead);


RTDECL(int) RTSemRWRequestReadNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    return RTSemRWRequestRead(RWSem, cMillies);
}
RT_EXPORT_SYMBOL(RTSemRWRequestReadNoResume);


RTDECL(int) RTSemRWReleaseRead(RTSEMRW RWSem)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    RTTHREAD Self = (RTTHREAD)RTThreadNativeSelf();

    /*
     * Take critsect.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThis->Writer == Self)
        {
            pThis->cWriterReads--;
        }
        else
        {
            AssertMsg(pThis->Writer == NIL_RTTHREAD, ("Impossible! Writers and Readers are exclusive!\n"));
            pThis->cReads--;

            /* Kick off a writer if appropriate. */
            if (    pThis->cWritesWaiting > 0
                &&  !pThis->cReads)
            {
                rc = RTSemEventSignal(pThis->WriteEvent);
                AssertMsgRC(rc, ("Failed to signal writers on rwsem %p, rc=%Rrc\n", RWSem, rc));
            }
        }

        RTCritSectLeave(&pThis->CritSect);
        return VINF_SUCCESS;
    }
    else
        AssertMsgFailed(("RTCritSectEnter failed on rwsem %p, rc=%Rrc\n", RWSem, rc));

    return rc;
}
RT_EXPORT_SYMBOL(RTSemRWReleaseRead);


RTDECL(int) RTSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    RTTHREAD    Self = (RTTHREAD)RTThreadNativeSelf();
    unsigned    cMilliesInitial = cMillies;
    uint64_t    tsStart = 0;
    if (cMillies != RT_INDEFINITE_WAIT && cMillies != 0)
        tsStart = RTTimeNanoTS();

    /*
     * Take critsect.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("RTCritSectEnter failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
        return rc;
    }

    /*
     * Signal writer presence.
     */
    pThis->cWritesWaiting++;

    for (;;)
    {
        /*
         * Check if the state of affairs allows write access.
         */
        if (!pThis->cReads && (!pThis->cWrites || pThis->Writer == Self))
        {
            /*
             * Reset the reader event semaphore if necessary.
             */
            if (pThis->fNeedResetReadEvent)
            {
                pThis->fNeedResetReadEvent = false;
                rc = RTSemEventMultiReset(pThis->ReadEvent);
                AssertMsgRC(rc, ("Failed to reset readers, rwsem %p, rc=%Rrc.\n", RWSem, rc));
            }

            pThis->cWrites++;
            pThis->Writer = Self;
            /* We're not waiting, so decrease counter. */
            pThis->cWritesWaiting--;
            RTCritSectLeave(&pThis->CritSect);
            return VINF_SUCCESS;
        }

        RTCritSectLeave(&pThis->CritSect);

        /*
         * Wait till it's ready for writing.
         */
        if (cMillies != RT_INDEFINITE_WAIT && cMillies != 0)
        {
            int64_t tsDelta = RTTimeNanoTS() - tsStart;
            if (tsDelta >= 1000000)
            {
                cMillies = cMilliesInitial - (unsigned)(tsDelta / 1000000);
                if (cMillies > cMilliesInitial)
                    cMillies = cMilliesInitial ? 1 : 0;
            }
        }
        rc = RTSemEventWait(pThis->WriteEvent, cMillies);
        if (RT_UNLIKELY(RT_FAILURE_NP(rc) && rc != VERR_TIMEOUT))
        {
            AssertMsgRC(rc, ("RTSemEventWait failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
            break;
        }

        if (RT_UNLIKELY(pThis->u32Magic != RTSEMRW_MAGIC))
        {
            rc = VERR_SEM_DESTROYED;
            break;
        }

        /*
         * Re-take critsect.
         */
        rc = RTCritSectEnter(&pThis->CritSect);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTCritSectEnter failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
            break;
        }
//        AssertMsg(!pThis->cReads, ("We woke up and there are readers around!\n"));
    }

    /*
     * Timeout/error case, clean up.
     */
    if (pThis->u32Magic == RTSEMRW_MAGIC)
    {
        RTCritSectEnter(&pThis->CritSect);
        /* Adjust this counter, whether we got the critsect or not. */
        pThis->cWritesWaiting--;
        RTCritSectLeave(&pThis->CritSect);
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTSemRWRequestWrite);


RTDECL(int) RTSemRWRequestWriteNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    return RTSemRWRequestWrite(RWSem, cMillies);
}
RT_EXPORT_SYMBOL(RTSemRWRequestWriteNoResume);


RTDECL(int) RTSemRWReleaseWrite(RTSEMRW RWSem)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    RTTHREAD Self = (RTTHREAD)RTThreadNativeSelf();

    /*
     * Take critsect.
     */
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("RTCritSectEnter failed on rwsem %p, rc=%Rrc\n", RWSem, rc));
        return rc;
    }

    /*
     * Check if owner.
     */
    if (pThis->Writer != Self)
    {
        RTCritSectLeave(&pThis->CritSect);
        AssertMsgFailed(("Not read-write owner of rwsem %p.\n", RWSem));
        return VERR_NOT_OWNER;
    }

    Assert(pThis->cWrites > 0);
    /*
     * Release ownership and remove ourselves from the writers count.
     */
    pThis->cWrites--;
    if (!pThis->cWrites)
        pThis->Writer = NIL_RTTHREAD;

    /*
     * Release the readers if no more writers waiting, otherwise the writers.
     */
    if (!pThis->cWritesWaiting)
    {
        rc = RTSemEventMultiSignal(pThis->ReadEvent);
        AssertMsgRC(rc, ("RTSemEventMultiSignal failed for rwsem %p, rc=%Rrc.\n", RWSem, rc));
        pThis->fNeedResetReadEvent = true;
    }
    else
    {
        rc = RTSemEventSignal(pThis->WriteEvent);
        AssertMsgRC(rc, ("Failed to signal writers on rwsem %p, rc=%Rrc\n", RWSem, rc));
    }
    RTCritSectLeave(&pThis->CritSect);

    return rc;
}
RT_EXPORT_SYMBOL(RTSemRWReleaseWrite);


RTDECL(bool) RTSemRWIsWriteOwner(RTSEMRW RWSem)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Check ownership.
     */
    RTTHREAD Self = (RTTHREAD)RTThreadNativeSelf();
    RTTHREAD Writer;
    ASMAtomicUoReadSize(&pThis->Writer, &Writer);
    return Writer == Self;
}
RT_EXPORT_SYMBOL(RTSemRWIsWriteOwner);


RTDECL(uint32_t) RTSemRWGetWriteRecursion(RTSEMRW RWSem)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Return the requested data.
     */
    return pThis->cWrites;
}
RT_EXPORT_SYMBOL(RTSemRWGetWriteRecursion);


RTDECL(uint32_t) RTSemRWGetWriterReadRecursion(RTSEMRW RWSem)
{
    struct RTSEMRWINTERNAL *pThis = RWSem;

    /*
     * Validate handle.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMRW_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Return the requested data.
     */
    return pThis->cWriterReads;
}
RT_EXPORT_SYMBOL(RTSemRWGetWriterReadRecursion);
