/* $Id$ */
/** @file
 * IPRT - Request Pool.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/req.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/critsect.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include "internal/req.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTREQPOOLTHREAD
{
    /** Node in the  RTREQPOOLINT::IdleThreads list. */
    RTLISTNODE              IdleNode;
    /** Node in the  RTREQPOOLINT::WorkerThreads list. */
    RTLISTNODE              ListNode;

    /** The submit timestamp of the pending request. */
    uint64_t                uPendingNanoTs;
    /** The submit timestamp of the request processing. */
    uint64_t                uProcessingNanoTs;
    /** When this CPU went idle the last time. */
    uint64_t                uIdleNanoTs;
    /** The number of requests processed by this thread. */
    uint64_t                cReqProcessed;
    /** Total time the requests processed by this thread took to process. */
    uint64_t                cNsTotalReqProcessing;
    /** Total time the requests processed by this thread had to wait in
     * the queue before being scheduled. */
    uint64_t                cNsTotalReqQueued;
    /** The CPU this was scheduled last time we checked. */
    RTCPUID                 idLastCpu;

    /** The submitter will put an incoming request here when scheduling an idle
     * thread.  */
    PRTREQINT volatile      pTodoReq;
    /** The request the thread is currently processing. */
    PRTREQINT volatile      pPendingReq;

    /** The thread handle. */
    RTTHREAD                hThread;
    /** Nano seconds timestamp representing the birth time of the thread.  */
    uint64_t                uBirthNanoTs;
    /** Pointer to the request thread pool instance the thread is associated
     *  with. */
    struct RTREQPOOLINT    *pPool;
} RTREQPOOLTHREAD;
/** Pointer to a worker thread. */
typedef RTREQPOOLTHREAD *PRTREQPOOLTHREAD;

/**
 * Request thread pool instance data.
 */
typedef struct RTREQPOOLINT
{
    /** Magic value (RTREQPOOL_MAGIC). */
    uint32_t                u32Magic;

    /** @name Config
     * @{  */
    /** The worker thread type. */
    RTTHREADTYPE            enmThreadType;
    /** The maximum number of worker threads. */
    uint32_t                cMaxThreads;
    /** The number of threads which should be spawned before throttling kicks
     * in. */
    uint32_t                cThreadsThreshold;
    /** The minimum number of worker threads. */
    uint32_t                cMinThreads;
    /** The number of milliseconds a thread needs to be idle before it is
     * considered for retirement. */
    uint32_t                cMsMinIdle;
    /** The max number of milliseconds to push back a submitter before creating
     * a new worker thread once the threshold has been reached. */
    uint32_t                cMsMaxPushBack;
    /** The minimum number of milliseconds to push back a submitter before
     * creating a new worker thread once the threshold has been reached. */
    uint32_t                cMsMinPushBack;
    /** The max number of free requests in the recycle LIFO. */
    uint32_t                cMaxFreeRequests;
    /** @}  */

    /** Signaled by terminating worker threads. */
    RTSEMEVENT              hThreadTermEvt;

    /** Destruction indicator.  The worker threads checks in their loop. */
    bool volatile           fDestructing;

    /** The current submitter push back in milliseconds.
     * This is recalculated when worker threads come and go.  */
    uint32_t                cMsCurPushBack;
    /** The current number of worker threads. */
    uint32_t                cCurThreads;
    /** Statistics: The total number of threads created. */
    uint32_t                cThreadsCreated;
    /** Statistics: The timestamp when the last thread was created. */
    uint64_t                uLastThreadCreateNanoTs;
    /** Linked list of worker threads. */
    RTLISTANCHOR            WorkerThreads;

    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** The number of idle thread or threads in the process of becoming
     * idle.  This is increased before the to-be-idle thread tries to enter
     * the critical section and add itself to the list. */
    uint32_t volatile       cIdleThreads;
    /** Linked list of idle threads. */
    RTLISTANCHOR            IdleThreads;

    /** Head of the request FIFO. */
    PRTREQINT               pPendingRequests;
    /** Where to insert the next request. */
    PRTREQINT              *ppPendingRequests;

    /** Head of the request recycling LIFO. */
    PRTREQINT               pFreeRequests;
    /** The number of requests in the recycling LIFO.  This is read without
     * entering the critical section, thus volatile. */
    uint32_t volatile       cCurFreeRequests;

    /** Critical section serializing access to members of this structure.  */
    RTCRITSECT              CritSect;

} RTREQPOOLINT;


static void rtReqPoolRecalcPushBack(PRTREQPOOLINT pPool)
{
    uint32_t const cMsRange = pPool->cMsMaxPushBack - pPool->cMsMinPushBack;
    uint32_t const cSteps   = pPool->cMaxThreads - pPool->cThreadsThreshold;
    uint32_t const iStep    = pPool->cCurThreads - pPool->cThreadsThreshold;

    uint32_t cMsCurPushBack;
    if ((cMsRange >> 2) >= cSteps)
        cMsCurPushBack = cMsRange / cSteps * iStep;
    else
        cMsCurPushBack = (uint32_t)( (uint64_t)cMsRange * RT_NS_1MS  / cSteps * iStep / RT_NS_1MS );
    cMsCurPushBack += pPool->cMsMinPushBack;

    pPool->cMsCurPushBack = cMsCurPushBack;
}



static void rtReqPoolThreadProcessRequest(PRTREQPOOLTHREAD pThread, PRTREQINT pReq)
{
    /*
     * Update thread state.
     */
    pThread->uProcessingNanoTs  = RTTimeNanoTS();
    pThread->uPendingNanoTs     = pReq->uSubmitNanoTs;
    pThread->pPendingReq        = pReq;
    Assert(pReq->u32Magic == RTREQ_MAGIC);

    /*
     * Do the actual processing.
     */
    /** @todo  */

    /*
     * Update thread statistics and state.
     */
    uint64_t const uNsTsEnd = RTTimeNanoTS();
    pThread->cNsTotalReqProcessing += uNsTsEnd - pThread->uProcessingNanoTs;
    pThread->cNsTotalReqQueued     += uNsTsEnd - pThread->uPendingNanoTs;
    pThread->cReqProcessed++;
}



static DECLCALLBACK(int) rtReqPoolThreadProc(RTTHREAD hThreadSelf, void *pvArg)
{
    PRTREQPOOLTHREAD    pThread = (PRTREQPOOLTHREAD)pvArg;
    PRTREQPOOLINT       pPool   = pThread->pPool;

/** @todo rework this... */

    /*
     * The work loop.
     */
    uint64_t            cPrevReqProcessed = 0;
    while (pPool->fDestructing)
    {
        /*
         * Pending work?
         */
        PRTREQINT pReq = ASMAtomicXchgPtrT(&pThread->pTodoReq, NULL, PRTREQINT);
        if (pReq)
            rtReqPoolThreadProcessRequest(pThread, pReq);
        else
        {
            ASMAtomicIncU32(&pPool->cIdleThreads);
            RTCritSectEnter(&pPool->CritSect);

            /* Recheck the todo request pointer after entering the critsect. */
            pReq = ASMAtomicXchgPtrT(&pThread->pTodoReq, NULL, PRTREQINT);
            if (!pReq)
            {
                /* Any pending requests in the queue? */
                pReq = pPool->pPendingRequests;
                if (pReq)
                {
                    pPool->pPendingRequests = pReq->pNext;
                    if (pReq->pNext == NULL)
                        pPool->ppPendingRequests = &pPool->pPendingRequests;
                }
            }

            if (pReq)
            {
                /*
                 * Un-idle ourselves and process the request.
                 */
                if (!RTListIsEmpty(&pThread->IdleNode))
                {
                    RTListNodeRemove(&pThread->IdleNode);
                    RTListInit(&pThread->IdleNode);
                }
                ASMAtomicDecU32(&pPool->cIdleThreads);
                RTCritSectLeave(&pPool->CritSect);

                rtReqPoolThreadProcessRequest(pThread, pReq);
            }
            else
            {
                /*
                 * Nothing to do, go idle.
                 */
                if (cPrevReqProcessed != pThread->cReqProcessed)
                {
                    pThread->cReqProcessed = cPrevReqProcessed;
                    pThread->uIdleNanoTs   = RTTimeNanoTS();
                }

                if (RTListIsEmpty(&pThread->IdleNode))
                    RTListPrepend(&pPool->IdleThreads, &pThread->IdleNode);
                RTThreadUserReset(hThreadSelf);

                RTCritSectLeave(&pPool->CritSect);

                RTThreadUserWait(hThreadSelf, 0);



            }
        }
    }

    /*
     * Clean up on the way out.
     */
    RTCritSectEnter(&pPool->CritSect);

    /** @todo ....  */

    rtReqPoolRecalcPushBack(pPool);

    RTCritSectLeave(&pPool->CritSect);

    return VINF_SUCCESS;
}


/**
 * Create a new worker thread.
 *
 * @param   pPool               The pool needing new worker thread.
 * @remarks Caller owns the critical section
 */
static void rtReqPoolCreateNewWorker(RTREQPOOL pPool)
{
    PRTREQPOOLTHREAD pThread = (PRTREQPOOLTHREAD)RTMemAllocZ(sizeof(RTREQPOOLTHREAD));
    if (!pThread)
        return;

    pThread->uBirthNanoTs = RTTimeNanoTS();
    pThread->pPool        = pPool;
    pThread->idLastCpu    = NIL_RTCPUID;
    pThread->hThread      = NIL_RTTHREAD;
    RTListInit(&pThread->IdleNode);
    RTListAppend(&pPool->WorkerThreads, &pThread->ListNode);
    pPool->cCurThreads++;
    pPool->cThreadsCreated++;

    static uint32_t s_idThread = 0;
    int rc = RTThreadCreateF(&pThread->hThread, rtReqPoolThreadProc, pThread, 0 /*default stack size*/,
                             pPool->enmThreadType, 0 /*fFlags*/, "REQPT%02u", ++s_idThread);
    if (RT_SUCCESS(rc))
        pPool->uLastThreadCreateNanoTs = pThread->uBirthNanoTs;
    else
    {
        pPool->cCurThreads--;
        RTListNodeRemove(&pThread->ListNode);
        RTMemFree(pThread);
    }
}


/**
 * Repel the submitter, giving the worker threads a chance to process the
 * incoming request.
 *
 * @returns Success if a worker picked up the request, failure if not.  The
 *          critical section has been left on success, while we'll be inside it
 *          on failure.
 * @param   pPool               The pool.
 * @param   pReq                The incoming request.
 */
static int rtReqPoolPushBack(PRTREQPOOLINT pPool, PRTREQINT pReq)
{
    /*
     * Lazily create the push back semaphore that we'll be blociing on.
     */
    int rc;
    RTSEMEVENTMULTI hEvt = pReq->hPushBackEvt;
    if (hEvt == NIL_RTSEMEVENTMULTI)
    {
        rc = RTSemEventMultiCreate(&hEvt);
        if (RT_FAILURE(rc))
            return rc;
        pReq->hPushBackEvt = hEvt;
    }

    /*
     * Prepare the request and semaphore.
     */
    uint32_t const cMsTimeout = pPool->cMsCurPushBack;
    pReq->fSignalPushBack = true;
    RTReqRetain(pReq);
    RTSemEventMultiReset(hEvt);

    RTCritSectLeave(&pPool->CritSect);

    /*
     * Block.
     */
    rc = RTSemEventMultiWait(hEvt, cMsTimeout);
    if (RT_FAILURE(rc))
    {
        AssertMsg(rc == VERR_TIMEOUT, ("%Rrc\n", rc));
        RTCritSectEnter(&pPool->CritSect);
    }
    RTReqRelease(pReq);
    return rc;
}



DECLHIDDEN(void) rtReqPoolSubmit(PRTREQPOOLINT pPool, PRTREQINT pReq)
{
    RTCritSectEnter(&pPool->CritSect);

    /*
     * Try schedule the request to a thread that's currently idle.
     */
    PRTREQPOOLTHREAD pThread = RTListGetFirst(&pPool->IdleThreads, RTREQPOOLTHREAD, IdleNode);
    if (pThread)
    {
        /** @todo CPU affinity... */
        ASMAtomicWritePtr(&pThread->pTodoReq, pReq);

        RTListNodeRemove(&pThread->IdleNode);
        RTListInit(&pThread->IdleNode);
        ASMAtomicDecU32(&pPool->cIdleThreads);

        RTThreadUserSignal(pThread->hThread);

        RTCritSectLeave(&pPool->CritSect);
        return;
    }
    Assert(RTListIsEmpty(&pPool->IdleThreads));

    /*
     * Put the request in the pending queue.
     */
    pReq->pNext = NULL;
    *pPool->ppPendingRequests = pReq;
    pPool->ppPendingRequests  = (PRTREQINT*)&pReq->pNext;

    /*
     * If there is an incoming worker thread already or we've reached the
     * maximum number of worker threads, we're done.
     */
    if (   pPool->cIdleThreads > 0
        || pPool->cCurThreads >= pPool->cMaxThreads)
    {
        RTCritSectLeave(&pPool->CritSect);
        return;
    }

    /*
     * Push back before creating a new worker thread.
     */
    if (   pPool->cCurThreads > pPool->cThreadsThreshold
        && (RTTimeNanoTS() - pReq->uSubmitNanoTs) / RT_NS_1MS >= pPool->cMsCurPushBack )
    {
        int rc = rtReqPoolPushBack(pPool, pReq);
        if (RT_SUCCESS(rc))
            return;
    }

    /*
     * Create a new thread for processing the request.
     * For simplicity, we don't bother leaving the critical section while doing so.
     */
    rtReqPoolCreateNewWorker(pPool);

    RTCritSectLeave(&pPool->CritSect);
    return;
}


/**
 * Frees a requst.
 *
 * @returns true if recycled, false if not.
 * @param   pPool               The request thread pool.
 * @param   pReq                The request.
 */
DECLHIDDEN(bool) rtReqPoolRecycle(PRTREQPOOLINT pPool, PRTREQINT pReq)
{
    if (   pPool
        && ASMAtomicReadU32(&pPool->cCurFreeRequests) < pPool->cMaxFreeRequests)
    {
        RTCritSectEnter(&pPool->CritSect);
        if (pPool->cCurFreeRequests < pPool->cMaxFreeRequests)
        {
            pReq->pNext = pPool->pFreeRequests;
            pPool->pFreeRequests = pReq;
            ASMAtomicIncU32(&pPool->cCurFreeRequests);

            RTCritSectLeave(&pPool->CritSect);
            return true;
        }

        RTCritSectLeave(&pPool->CritSect);
    }
    return false;
}


RTDECL(uint32_t) RTReqPoolRetain(RTREQPOOL hPool)
{
    PRTREQPOOLINT pPool = hPool;
    AssertPtrReturn(pPool, UINT32_MAX);
    AssertReturn(pPool->u32Magic == RTREQPOOL_MAGIC, UINT32_MAX);

    return ASMAtomicIncU32(&pPool->cRefs);
}
RT_EXPORT_SYMBOL(RTReqPoolRetain);


RTDECL(uint32_t) RTReqPoolRelease(RTREQPOOL hPool)
{
    /*
     * Ignore NULL and validate the request.
     */
    if (!hPool)
        return 0;
    PRTREQPOOLINT pPool = hPool;
    AssertPtrReturn(pPool, UINT32_MAX);
    AssertReturn(pPool->u32Magic == RTREQPOOL_MAGIC, UINT32_MAX);

    /*
     * Drop a reference, free it when it reaches zero.
     */
    uint32_t cRefs = ASMAtomicDecU32(&pPool->cRefs);
    if (cRefs == 0)
    {
        AssertReturn(ASMAtomicCmpXchgU32(&pPool->u32Magic, RTREQPOOL_MAGIC_DEAD, RTREQPOOL_MAGIC), UINT32_MAX);

        RTCritSectEnter(&pPool->CritSect);

        /* Indicate to the worker threads that we're shutting down. */
        ASMAtomicWriteBool(&pPool->fDestructing, true);
        PRTREQPOOLTHREAD pThread;
        RTListForEach(&pPool->WorkerThreads, pThread, RTREQPOOLTHREAD, ListNode)
        {
            RTThreadUserSignal(pThread->hThread);
        }

        /* Cancel pending requests. */
        Assert(!pPool->pPendingRequests);
        while (pPool->pPendingRequests)
        {
            PRTREQINT pReq = pPool->pPendingRequests;
            pPool->pPendingRequests = pReq->pNext;

            pReq->enmState = RTREQSTATE_COMPLETED;
            ASMAtomicWriteS32(&pReq->iStatusX, VERR_CANCELLED);
            if (pReq->hPushBackEvt != NIL_RTSEMEVENTMULTI)
                RTSemEventMultiSignal(pReq->hPushBackEvt);
            RTSemEventSignal(pReq->EventSem);

            pReq->uOwner.hPool = NULL;
            RTReqRelease(pReq);
        }
        pPool->ppPendingRequests = NULL;

        /* Wait for the workers to shut down. */
        while (!RTListIsEmpty(&pPool->WorkerThreads))
        {
            RTCritSectLeave(&pPool->CritSect);
            RTSemEventWait(pPool->hThreadTermEvt, RT_MS_1MIN);
            RTCritSectEnter(&pPool->CritSect);
            /** @todo should we wait forever here? */
        }

        /* Free recycled requests. */
        for (;;)
        {
            PRTREQINT pReq = pPool->pFreeRequests;
            if (!pReq)
                break;
            pPool->pFreeRequests = pReq->pNext;
            pPool->cCurFreeRequests--;
            rtReqFreeIt(pReq);
        }

        /* Finally, free the handle. */
        RTMemFree(pPool);
    }

    return cRefs;
}
RT_EXPORT_SYMBOL(RTReqPoolRelease);


RTDECL(int) RTReqPoolAlloc(RTREQPOOL hPool, RTREQTYPE enmType, PRTREQ *phReq)
{
    PRTREQPOOLINT pPool = hPool;
    AssertPtrReturn(pPool, VERR_INVALID_HANDLE);
    AssertReturn(pPool->u32Magic == RTREQPOOL_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Try recycle old requests.
     */
    if (ASMAtomicReadU32(&pPool->cCurFreeRequests) > 0)
    {
        RTCritSectEnter(&pPool->CritSect);
        PRTREQINT pReq = pPool->pFreeRequests;
        if (pReq)
        {
            ASMAtomicDecU32(&pPool->cCurFreeRequests);
            pPool->pFreeRequests = pReq->pNext;

            RTCritSectLeave(&pPool->CritSect);

            Assert(pReq->fPoolOrQueue);
            Assert(pReq->uOwner.hPool == pPool);

            int rc = rtReqReInit(pReq, enmType);
            if (RT_SUCCESS(rc))
            {
                *phReq = pReq;
                LogFlow(("RTReqPoolAlloc: returns VINF_SUCCESS *phReq=%p recycled\n", pReq));
                return rc;
            }
        }
        else
            RTCritSectLeave(&pPool->CritSect);
    }

    /*
     * Allocate a new request.
     */
    int rc = rtReqAlloc(enmType, true /*fPoolOrQueue*/, pPool, phReq);
    LogFlow(("RTReqPoolAlloc: returns %Rrc *phReq=%p\n", rc, *phReq));
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTReqPoolAlloc);

