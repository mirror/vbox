/* $Id: VMReq.cpp 17451 2007-01-15 14:08:28Z bird $ */
/** @file
 * IPRT - Request packets
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

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/req.h>
#include <iprt/log.h>
#include <iprt/mem.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  rtReqProcessOne(PRTREQ pReq);



/**
 * Create a request packet queueu
 *
 * @returns iprt status code.
 * @param   ppQueue         Where to store the request queue pointer.
 */
RTDECL(int) RTReqCreateQueue(PRTREQQUEUE *ppQueue)
{
    *ppQueue = (PRTREQQUEUE)RTMemAllocZ(sizeof(RTREQQUEUE));
    if (!ppQueue)
        return VERR_NO_MEMORY;

    int rc = RTSemEventCreate(&(*ppQueue)->EventSem);
    if (rc != VINF_SUCCESS)
        RTMemFree(*ppQueue);

    return rc;
}


/**
 * Destroy a request packet queueu
 *
 * @returns iprt status code.
 * @param   pQueue          The request queue.
 */
RTDECL(int) RTReqDestroyQueue(PRTREQQUEUE pQueue)
{
    /*
     * Check input.
     */
    if (!pQueue)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }
    RTSemEventDestroy(pQueue->EventSem);
    RTMemFree(pQueue);
    return VINF_SUCCESS;
}

/**
 * Process one or more request packets
 *
 * @returns iprt status code.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being added.
 *
 * @param   pQueue          The request queue.
 * @param   cMillies        Number of milliseconds to wait for a pending request.
 *                          Use RT_INDEFINITE_WAIT to only wait till one is added.
 */
RTDECL(int) RTReqProcess(PRTREQQUEUE pQueue, unsigned cMillies)
{
    LogFlow(("RTReqProcess %x\n", pQueue));
    /*
     * Check input.
     */
    if (!pQueue)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Process loop.
     *
     * We do not repeat the outer loop if we've got an informationtional status code
     * since that code needs processing by our caller.
     */
    int rc = VINF_SUCCESS;
    while (rc <= VINF_SUCCESS)
    {
        /*
         * Get pending requests.
         */
        PRTREQ pReqs = (PRTREQ)ASMAtomicXchgPtr((void * volatile *)&pQueue->pReqs, NULL);
        if (!pReqs)
        {
            /** @note We currently don't care if the entire time wasted here is larger than cMillies */
            rc = RTSemEventWait(pQueue->EventSem, cMillies);
            if (rc != VINF_SUCCESS)
                break;
            continue;
        }

        /*
         * Reverse the list to process it in FIFO order.
         */
        PRTREQ pReq = pReqs;
        if (pReq->pNext)
            Log2(("RTReqProcess: 2+ requests: %p %p %p\n", pReq, pReq->pNext, pReq->pNext->pNext));
        pReqs = NULL;
        while (pReq)
        {
            Assert(pReq->enmState == RTREQSTATE_QUEUED);
            Assert(pReq->pQueue == pQueue);
            PRTREQ pCur = pReq;
            pReq = pReq->pNext;
            pCur->pNext = pReqs;
            pReqs = pCur;
        }


        /*
         * Process the requests.
         */
        while (pReqs)
        {
            /* Unchain the first request and advance the list. */
            pReq = pReqs;
            pReqs = pReqs->pNext;
            pReq->pNext = NULL;

            /* Process the request */
            rc = rtReqProcessOne(pReq);
            AssertRC(rc);
            if (rc != VINF_SUCCESS)
                break;
        }
    }

    LogFlow(("RTReqProcess: returns %Vrc\n", rc));
    return rc;
}

/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt statuscode.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happens.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
RTDECL(int) RTReqCall(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = RTReqCallV(pQueue, ppReq, cMillies, RTREQFLAGS_IPRT_STATUS, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}


/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
RTDECL(int) RTReqCallVoid(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = RTReqCallV(pQueue, ppReq, cMillies, RTREQFLAGS_VOID, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}


/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains RTREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the RTREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
RTDECL(int) RTReqCallEx(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = RTReqCallV(pQueue, ppReq, cMillies, fFlags, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}


/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains RTREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the RTREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   Args            Variable argument vector.
 */
RTDECL(int) RTReqCallV(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args)
{
    LogFlow(("RTReqCallV: cMillies=%d fFlags=%#x pfnFunction=%p cArgs=%d\n", cMillies, fFlags, pfnFunction, cArgs));

    /*
     * Check input.
     */
    if (!pfnFunction || !pQueue || (fFlags & ~(RTREQFLAGS_RETURN_MASK | RTREQFLAGS_NO_WAIT)))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }
    if (!(fFlags & RTREQFLAGS_NO_WAIT) || ppReq)
    {
        Assert(ppReq);
        *ppReq = NULL;
    }
    PRTREQ pReq = NULL;
    if (cArgs * sizeof(uintptr_t) > sizeof(pReq->u.Internal.aArgs))
    {
        AssertMsgFailed(("cArg=%d\n", cArgs));
        return VERR_TOO_MUCH_DATA;
    }

    /*
     * Allocate request
     */
    int rc = RTReqAlloc(pQueue, &pReq, RTREQTYPE_INTERNAL);
    if (rc != VINF_SUCCESS)
        return rc;

    /*
     * Initialize the request data.
     */
    pReq->fFlags         = fFlags;
    pReq->u.Internal.pfn = pfnFunction;
    pReq->u.Internal.cArgs = cArgs;
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
        pReq->u.Internal.aArgs[iArg] = va_arg(Args, uintptr_t);

    /*
     * Queue the request and return.
     */
    rc = RTReqQueue(pReq, cMillies);
    if (   rc != VINF_SUCCESS
        && rc != VERR_TIMEOUT)
    {
        RTReqFree(pReq);
        pReq = NULL;
    }
    if (!(fFlags & RTREQFLAGS_NO_WAIT))
    {
        *ppReq = pReq;
        LogFlow(("RTReqCallV: returns %Vrc *ppReq=%p\n", rc, pReq));
    }
    else
        LogFlow(("RTReqCallV: returns %Vrc\n", rc));
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


/**
 * Joins the list pList with whatever is linked up at *pHead.
 */
static void vmr3ReqJoinFreeSub(volatile PRTREQ *ppHead, PRTREQ pList)
{
    for (unsigned cIterations = 0;; cIterations++)
    {
        PRTREQ pHead = (PRTREQ)ASMAtomicXchgPtr((void * volatile *)ppHead, pList);
        if (!pHead)
            return;
        PRTREQ pTail = pHead;
        while (pTail->pNext)
            pTail = pTail->pNext;
        pTail->pNext = pList;
        if (ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (void *)pHead, pList))
            return;
        pTail->pNext = NULL;
        if (ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (void *)pHead, NULL))
            return;
        pList = pHead;
        Assert(cIterations != 32);
        Assert(cIterations != 64);
    }
}


/**
 * Joins the list pList with whatever is linked up at *pHead.
 */
static void vmr3ReqJoinFree(PRTREQQUEUE pQueue, PRTREQ pList)
{
    /*
     * Split the list if it's too long.
     */
    unsigned cReqs = 1;
    PRTREQ pTail = pList;
    while (pTail->pNext)
    {
        if (cReqs++ > 25)
        {
            const uint32_t i = pQueue->iReqFree;
            vmr3ReqJoinFreeSub(&pQueue->apReqFree[(i + 2) % ELEMENTS(pQueue->apReqFree)], pTail->pNext);

            pTail->pNext = NULL;
            vmr3ReqJoinFreeSub(&pQueue->apReqFree[(i + 2 + (i == pQueue->iReqFree)) % ELEMENTS(pQueue->apReqFree)], pTail->pNext);
            return;
        }
        pTail = pTail->pNext;
    }
    vmr3ReqJoinFreeSub(&pQueue->apReqFree[(pQueue->iReqFree + 2) % ELEMENTS(pQueue->apReqFree)], pList);
}


/**
 * Allocates a request packet.
 *
 * The caller allocates a request packet, fills in the request data
 * union and queues the request.
 *
 * @returns iprt status code.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the allocated packet.
 * @param   enmType         Package type.
 */
RTDECL(int) RTReqAlloc(PRTREQQUEUE pQueue, PRTREQ *ppReq, RTREQTYPE enmType)
{
    /*
     * Validate input.
     */
    if (    enmType < RTREQTYPE_INVALID
        ||  enmType > RTREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusivly.\n",
                         enmType, RTREQTYPE_INVALID + 1, RTREQTYPE_MAX - 1));
        return VERR_RT_REQUEST_INVALID_TYPE;
    }

    /*
     * Try get a recycled packet.
     * While this could all be solved with a single list with a lock, it's a sport
     * of mine to avoid locks.
     */
    int cTries = ELEMENTS(pQueue->apReqFree) * 2;
    while (--cTries >= 0)
    {
        PRTREQ volatile *ppHead = &pQueue->apReqFree[ASMAtomicIncU32(&pQueue->iReqFree) % ELEMENTS(pQueue->apReqFree)];
#if 0 /* sad, but this won't work safely because the reading of pReq->pNext. */
        PRTREQ pNext = NULL;
        PRTREQ pReq = *ppHead;
        if (    pReq
            &&  !ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (pNext = pReq->pNext), pReq)
            &&  (pReq = *ppHead)
            &&  !ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (pNext = pReq->pNext), pReq))
            pReq = NULL;
        if (pReq)
        {
            Assert(pReq->pNext == pNext); NOREF(pReq);
#else
        PRTREQ pReq = (PRTREQ)ASMAtomicXchgPtr((void * volatile *)ppHead, NULL);
        if (pReq)
        {
            PRTREQ pNext = pReq->pNext;
            if (    pNext
                &&  !ASMAtomicCmpXchgPtr((void * volatile *)ppHead, pNext, NULL))
            {
                vmr3ReqJoinFree(pQueue, pReq->pNext);
            }
#endif
            ASMAtomicDecU32(&pQueue->cReqFree);

            /*
             * Make sure the event sem is not signaled.
             */
            if (!pReq->fEventSemClear)
            {
                int rc = RTSemEventWait(pReq->EventSem, 0);
                if (rc != VINF_SUCCESS && rc != VERR_TIMEOUT)
                {
                    /*
                     * This shall not happen, but if it does we'll just destroy
                     * the semaphore and create a new one.
                     */
                    AssertMsgFailed(("rc=%Vrc from RTSemEventWait(%#x).\n", rc, pReq->EventSem));
                    RTSemEventDestroy(pReq->EventSem);
                    rc = RTSemEventCreate(&pReq->EventSem);
                    AssertRC(rc);
                    if (rc != VINF_SUCCESS)
                        return rc;
                }
                pReq->fEventSemClear = true;
            }
            else
                Assert(RTSemEventWait(pReq->EventSem, 0) == VERR_TIMEOUT);

            /*
             * Initialize the packet and return it.
             */
            Assert(pReq->enmType == RTREQTYPE_INVALID);
            Assert(pReq->enmState == RTREQSTATE_FREE);
            Assert(pReq->pQueue == pQueue);
            ASMAtomicXchgSize(&pReq->pNext, NULL);
            pReq->enmState = RTREQSTATE_ALLOCATED;
            pReq->iStatus  = VERR_RT_REQUEST_STATUS_STILL_PENDING;
            pReq->fFlags   = RTREQFLAGS_IPRT_STATUS;
            pReq->enmType  = enmType;

            *ppReq = pReq;
            LogFlow(("RTReqAlloc: returns VINF_SUCCESS *ppReq=%p recycled\n", pReq));
            return VINF_SUCCESS;
        }
    }

    /*
     * Ok allocate one.
     */
    PRTREQ pReq = (PRTREQ)RTMemAllocZ(sizeof(*pReq));
    if (!pReq)
        return VERR_NO_MEMORY;

    /*
     * Create the semaphore.
     */
    int rc = RTSemEventCreate(&pReq->EventSem);
    AssertRC(rc);
    if (rc != VINF_SUCCESS)
    {
        RTMemFree(pReq);
        return rc;
    }

    /*
     * Initialize the packet and return it.
     */
    pReq->pNext    = NULL;
    pReq->pQueue   = pQueue;
    pReq->enmState = RTREQSTATE_ALLOCATED;
    pReq->iStatus  = VERR_RT_REQUEST_STATUS_STILL_PENDING;
    pReq->fEventSemClear = true;
    pReq->fFlags   = RTREQFLAGS_IPRT_STATUS;
    pReq->enmType  = enmType;

    *ppReq = pReq;
    LogFlow(("RTReqAlloc: returns VINF_SUCCESS *ppReq=%p new\n", pReq));
    return VINF_SUCCESS;
}


/**
 * Free a request packet.
 *
 * @returns iprt status code.
 *
 * @param   pReq            Package to free.
 * @remark  The request packet must be in allocated or completed state!
 */
RTDECL(int) RTReqFree(PRTREQ pReq)
{
    /*
     * Ignore NULL (all free functions should do this imho).
     */
    if (!pReq)
        return VINF_SUCCESS;


    /*
     * Check packet state.
     */
    switch (pReq->enmState)
    {
        case RTREQSTATE_ALLOCATED:
        case RTREQSTATE_COMPLETED:
            break;
        default:
            AssertMsgFailed(("Invalid state %d!\n", pReq->enmState));
            return VERR_RT_REQUEST_STATE;
    }

    /*
     * Make it a free packet and put it into one of the free packet lists.
     */
    pReq->enmState = RTREQSTATE_FREE;
    pReq->iStatus  = VERR_RT_REQUEST_STATUS_FREED;
    pReq->enmType  = RTREQTYPE_INVALID;

    PRTREQQUEUE pQueue = pReq->pQueue;
    if (pQueue->cReqFree < 128)
    {
        ASMAtomicIncU32(&pQueue->cReqFree);
        PRTREQ volatile *ppHead = &pQueue->apReqFree[ASMAtomicIncU32(&pQueue->iReqFree) % ELEMENTS(pQueue->apReqFree)];
        PRTREQ pNext;
        do
        {
            pNext = *ppHead;
            ASMAtomicXchgPtr((void * volatile *)&pReq->pNext, pNext);
        } while (!ASMAtomicCmpXchgPtr((void * volatile *)ppHead, (void *)pReq, (void *)pNext));
    }
    else
    {
        RTSemEventDestroy(pReq->EventSem);
        RTMemFree(pReq);
    }
    return VINF_SUCCESS;
}


/**
 * Queue a request.
 *
 * The quest must be allocated using RTReqAlloc() and contain
 * all the required data.
 * If it's disired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to queue.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 */
RTDECL(int) RTReqQueue(PRTREQ pReq, unsigned cMillies)
{
    LogFlow(("RTReqQueue: pReq=%p cMillies=%d\n", pReq, cMillies));
    /*
     * Verify the supplied package.
     */
    if (pReq->enmState != RTREQSTATE_ALLOCATED)
    {
        AssertMsgFailed(("Invalid state %d\n", pReq->enmState));
        return VERR_RT_REQUEST_STATE;
    }
    if (   !pReq->pQueue
        ||  pReq->pNext
        ||  !pReq->EventSem)
    {
        AssertMsgFailed(("Invalid request package! Anyone cooking their own packages???\n"));
        return VERR_RT_REQUEST_INVALID_PACKAGE;
    }
    if (    pReq->enmType < RTREQTYPE_INVALID
        || pReq->enmType > RTREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusivly. This was verified on alloc too...\n",
                         pReq->enmType, RTREQTYPE_INVALID + 1, RTREQTYPE_MAX - 1));
        return VERR_RT_REQUEST_INVALID_TYPE;
    }

    int rc = VINF_SUCCESS;
    /*
     * Insert it.
     */
    pReq->enmState = RTREQSTATE_QUEUED;
    PRTREQ pNext;
    do
    {
        pNext = pReq->pQueue->pReqs;
        pReq->pNext = pNext;
    } while (!ASMAtomicCmpXchgPtr((void * volatile *)&pReq->pQueue->pReqs, (void *)pReq, (void *)pNext));

    /*
     * Notify queue thread.
     */
    RTSemEventSignal(pReq->pQueue->EventSem);

    /*
     * Wait and return.
     */
    if (!(pReq->fFlags & RTREQFLAGS_NO_WAIT))
        rc = RTReqWait(pReq, cMillies);
    LogFlow(("RTReqQueue: returns %Vrc\n", rc));
    return rc;
}


/**
 * Wait for a request to be completed.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to wait for.
 * @param   cMillies        Number of milliseconds to wait.
 *                          Use RT_INDEFINITE_WAIT to only wait till it's completed.
 */
RTDECL(int) RTReqWait(PRTREQ pReq, unsigned cMillies)
{
    LogFlow(("RTReqWait: pReq=%p cMillies=%d\n", pReq, cMillies));

    /*
     * Verify the supplied package.
     */
    if (    pReq->enmState != RTREQSTATE_QUEUED
        &&  pReq->enmState != RTREQSTATE_PROCESSING
        &&  pReq->enmState != RTREQSTATE_COMPLETED)
    {
        AssertMsgFailed(("Invalid state %d\n", pReq->enmState));
        return VERR_RT_REQUEST_STATE;
    }
    if (    !pReq->pQueue
        ||  !pReq->EventSem)
    {
        AssertMsgFailed(("Invalid request package! Anyone cooking their own packages???\n"));
        return VERR_RT_REQUEST_INVALID_PACKAGE;
    }
    if (    pReq->enmType < RTREQTYPE_INVALID
        ||  pReq->enmType > RTREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusivly. This was verified on alloc and queue too...\n",
                         pReq->enmType, RTREQTYPE_INVALID + 1, RTREQTYPE_MAX - 1));
        return VERR_RT_REQUEST_INVALID_TYPE;
    }

    /*
     * Wait on the package.
     */
    int rc;
    if (cMillies != RT_INDEFINITE_WAIT)
        rc = RTSemEventWait(pReq->EventSem, cMillies);
    else
    {
        do
        {
            rc = RTSemEventWait(pReq->EventSem, RT_INDEFINITE_WAIT);
            Assert(rc != VERR_TIMEOUT);
        } while (pReq->enmState != RTREQSTATE_COMPLETED);
    }
    if (rc == VINF_SUCCESS)
        ASMAtomicXchgSize(&pReq->fEventSemClear, true);
    if (pReq->enmState == RTREQSTATE_COMPLETED)
        rc = VINF_SUCCESS;
    LogFlow(("RTReqWait: returns %Vrc\n", rc));
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


/**
 * Process one request.
 *
 * @returns IPRT status code.
 *
 * @param   pReq        Request packet to process.
 */
static int  rtReqProcessOne(PRTREQ pReq)
{
    LogFlow(("rtReqProcessOne: pReq=%p type=%d fFlags=%#x\n", pReq, pReq->enmType, pReq->fFlags));

    /*
     * Process the request.
     */
    Assert(pReq->enmState == RTREQSTATE_QUEUED);
    pReq->enmState = RTREQSTATE_PROCESSING;
    int     rcRet = VINF_SUCCESS;           /* the return code of this function. */
    int     rcReq = VERR_NOT_IMPLEMENTED;   /* the request status. */
    switch (pReq->enmType)
    {
        /*
         * A packed down call frame.
         */
        case RTREQTYPE_INTERNAL:
        {
            uintptr_t *pauArgs = &pReq->u.Internal.aArgs[0];
            union
            {
                PFNRT pfn;
                DECLCALLBACKMEMBER(int, pfn00)(void);
                DECLCALLBACKMEMBER(int, pfn01)(uintptr_t);
                DECLCALLBACKMEMBER(int, pfn02)(uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn03)(uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn04)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn05)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn06)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn07)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn08)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn09)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn10)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn11)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                DECLCALLBACKMEMBER(int, pfn12)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
            } u;
            u.pfn = pReq->u.Internal.pfn;
#ifdef RT_ARCH_AMD64
            switch (pReq->u.Internal.cArgs)
            {
                case 0:  rcRet = u.pfn00(); break;
                case 1:  rcRet = u.pfn01(pauArgs[0]); break;
                case 2:  rcRet = u.pfn02(pauArgs[0], pauArgs[1]); break;
                case 3:  rcRet = u.pfn03(pauArgs[0], pauArgs[1], pauArgs[2]); break;
                case 4:  rcRet = u.pfn04(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3]); break;
                case 5:  rcRet = u.pfn05(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4]); break;
                case 6:  rcRet = u.pfn06(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5]); break;
                case 7:  rcRet = u.pfn07(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6]); break;
                case 8:  rcRet = u.pfn08(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7]); break;
                case 9:  rcRet = u.pfn09(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8]); break;
                case 10: rcRet = u.pfn10(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8], pauArgs[9]); break;
                case 11: rcRet = u.pfn11(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8], pauArgs[9], pauArgs[10]); break;
                case 12: rcRet = u.pfn12(pauArgs[0], pauArgs[1], pauArgs[2], pauArgs[3], pauArgs[4], pauArgs[5], pauArgs[6], pauArgs[7], pauArgs[8], pauArgs[9], pauArgs[10], pauArgs[11]); break;
                default:
                    AssertReleaseMsgFailed(("cArgs=%d\n", pReq->u.Internal.cArgs));
                    rcRet = rcReq = VERR_INTERNAL_ERROR;
                    break;
            }
#else /* x86: */
            size_t cbArgs = pReq->u.Internal.cArgs * sizeof(uintptr_t);
# ifdef __GNUC__
            __asm__ __volatile__("movl  %%esp, %%edx\n\t"
                                 "subl  %2, %%esp\n\t"
                                 "andl  $0xfffffff0, %%esp\n\t"
                                 "shrl  $2, %2\n\t"
                                 "movl  %%esp, %%edi\n\t"
                                 "rep movsl\n\t"
                                 "movl  %%edx, %%edi\n\t"
                                 "call  *%%eax\n\t"
                                 "mov   %%edi, %%esp\n\t"
                                 : "=a" (rcRet),
                                   "=S" (pauArgs),
                                   "=c" (cbArgs)
                                 : "0" (u.pfn),
                                   "1" (pauArgs),
                                   "2" (cbArgs)
                                 : "edi", "edx");
# else
            __asm
            {
                xor     edx, edx        /* just mess it up. */
                mov     eax, u.pfn
                mov     ecx, cbArgs
                shr     ecx, 2
                mov     esi, pauArgs
                mov     ebx, esp
                sub     esp, cbArgs
                and     esp, 0xfffffff0
                mov     edi, esp
                rep movsd
                call    eax
                mov     esp, ebx
                mov     rcRet, eax
            }
# endif
#endif /* x86 */
            if ((pReq->fFlags & (RTREQFLAGS_RETURN_MASK)) == RTREQFLAGS_VOID)
                rcRet = VINF_SUCCESS;
            rcReq = rcRet;
            break;
        }

        default:
            AssertMsgFailed(("pReq->enmType=%d\n", pReq->enmType));
            rcReq = VERR_NOT_IMPLEMENTED;
            break;
    }

    /*
     * Complete the request.
     */
    pReq->iStatus  = rcReq;
    pReq->enmState = RTREQSTATE_COMPLETED;
    if (pReq->fFlags & RTREQFLAGS_NO_WAIT)
    {
        /* Free the packet, nobody is waiting. */
        LogFlow(("rtReqProcessOne: Completed request %p: rcReq=%Vrc rcRet=%Vrc - freeing it\n",
                 pReq, rcReq, rcRet));
        RTReqFree(pReq);
    }
    else
    {
        /* Notify the waiter and him free up the packet. */
        LogFlow(("rtReqProcessOne: Completed request %p: rcReq=%Vrc rcRet=%Vrc - notifying waiting thread\n",
                 pReq, rcReq, rcRet));
        ASMAtomicXchgSize(&pReq->fEventSemClear, false);
        int rc2 = RTSemEventSignal(pReq->EventSem);
        if (rc2 != VINF_SUCCESS)
        {
            AssertRC(rc2);
            rcRet = rc2;
        }
    }
    return rcRet;
}




