/* $Id$ */
/** @file
 * IPRT - Request Queue.
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
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include "internal/req.h"
#include "internal/magics.h"



RTDECL(int) RTReqQueueCreate(RTREQQUEUE *phQueue)
{
    PRTREQQUEUEINT pQueue = (PRTREQQUEUEINT)RTMemAllocZ(sizeof(RTREQQUEUEINT));
    if (!pQueue)
        return VERR_NO_MEMORY;
    int rc = RTSemEventCreate(&pQueue->EventSem);
    if (RT_SUCCESS(rc))
    {
        pQueue->u32Magic = RTREQQUEUE_MAGIC;

        *phQueue = pQueue;
        return VINF_SUCCESS;
    }

    RTMemFree(pQueue);
    return rc;
}
RT_EXPORT_SYMBOL(RTReqQueueCreate);


RTDECL(int) RTReqQueueDestroy(RTREQQUEUE hQueue)
{
    /*
     * Check input.
     */
    if (hQueue == NIL_RTREQQUEUE)
        return VINF_SUCCESS;
    PRTREQQUEUEINT pQueue = hQueue;
    AssertPtrReturn(pQueue, VERR_INVALID_HANDLE);
    AssertReturn(ASMAtomicCmpXchgU32(&pQueue->u32Magic, RTREQQUEUE_MAGIC_DEAD, RTREQQUEUE_MAGIC), VERR_INVALID_HANDLE);

    RTSemEventDestroy(pQueue->EventSem);
    pQueue->EventSem = NIL_RTSEMEVENT;

    for (unsigned i = 0; i < RT_ELEMENTS(pQueue->apReqFree); i++)
    {
        PRTREQ pReq = (PRTREQ)ASMAtomicXchgPtr(&pQueue->apReqFree[i], NULL);
        while (pReq)
        {
            PRTREQ pNext = pReq->pNext;

            pReq->u32Magic = RTREQ_MAGIC_DEAD;
            RTSemEventDestroy(pReq->EventSem);
            pReq->EventSem = NIL_RTSEMEVENT;
            RTMemFree(pReq);

            pReq = pNext;
        }
    }

    RTMemFree(pQueue);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTReqQueueDestroy);


RTDECL(int) RTReqQueueProcess(RTREQQUEUE hQueue, RTMSINTERVAL cMillies)
{
    LogFlow(("RTReqProcess %x\n", hQueue));

    /*
     * Check input.
     */
    PRTREQQUEUEINT pQueue = hQueue;
    AssertPtrReturn(pQueue, VERR_INVALID_HANDLE);
    AssertReturn(pQueue->u32Magic == RTREQQUEUE_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Process loop.
     *
     * We do not repeat the outer loop if we've got an informational status code
     * since that code needs processing by our caller.
     */
    int rc = VINF_SUCCESS;
    while (rc <= VINF_SUCCESS)
    {
        /*
         * Get pending requests.
         */
        PRTREQ pReqs = ASMAtomicXchgPtrT(&pQueue->pReqs, NULL, PRTREQ);
        if (!pReqs)
        {
            ASMAtomicWriteBool(&pQueue->fBusy, false); /* this aint 100% perfect, but it's good enough for now... */
            /** @todo We currently don't care if the entire time wasted here is larger than
             *        cMillies */
            rc = RTSemEventWait(pQueue->EventSem, cMillies);
            if (rc != VINF_SUCCESS)
                break;
            continue;
        }
        ASMAtomicWriteBool(&pQueue->fBusy, true);

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
            Assert(pReq->uOwner.hQueue == pQueue);
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
                break; /** @todo r=bird: we're dropping requests here! Add 2nd queue that can hold them. (will fix when writing a testcase)  */
        }
    }

    LogFlow(("RTReqProcess: returns %Rrc\n", rc));
    return rc;
}
RT_EXPORT_SYMBOL(RTReqQueueProcess);


RTDECL(int) RTReqQueueCall(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = RTReqQueueCallV(hQueue, ppReq, cMillies, RTREQFLAGS_IPRT_STATUS, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}
RT_EXPORT_SYMBOL(RTReqQueueCall);


RTDECL(int) RTReqQueueCallVoid(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = RTReqQueueCallV(hQueue, ppReq, cMillies, RTREQFLAGS_VOID, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}
RT_EXPORT_SYMBOL(RTReqQueueCallVoid);


RTDECL(int) RTReqQueueCallEx(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...)
{
    va_list va;
    va_start(va, cArgs);
    int rc = RTReqQueueCallV(hQueue, ppReq, cMillies, fFlags, pfnFunction, cArgs, va);
    va_end(va);
    return rc;
}
RT_EXPORT_SYMBOL(RTReqQueueCallEx);


RTDECL(int) RTReqQueueCallV(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args)
{
    LogFlow(("RTReqCallV: cMillies=%d fFlags=%#x pfnFunction=%p cArgs=%d\n", cMillies, fFlags, pfnFunction, cArgs));

    /*
     * Check input.
     */
    PRTREQQUEUEINT pQueue = hQueue;
    AssertPtrReturn(pQueue, VERR_INVALID_HANDLE);
    AssertReturn(pQueue->u32Magic == RTREQQUEUE_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pfnFunction, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~(RTREQFLAGS_RETURN_MASK | RTREQFLAGS_NO_WAIT)), VERR_INVALID_PARAMETER);

    if (!(fFlags & RTREQFLAGS_NO_WAIT) || ppReq)
    {
        AssertPtrReturn(ppReq, VERR_INVALID_POINTER);
        *ppReq = NULL;
    }

    PRTREQ pReq = NULL;
    AssertMsgReturn(cArgs * sizeof(uintptr_t) <= sizeof(pReq->u.Internal.aArgs), ("cArgs=%u\n", cArgs), VERR_TOO_MUCH_DATA);

    /*
     * Allocate request
     */
    int rc = RTReqQueueAlloc(pQueue, &pReq, RTREQTYPE_INTERNAL);
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
    rc = RTReqSubmit(pReq, cMillies);
    if (   rc != VINF_SUCCESS
        && rc != VERR_TIMEOUT)
    {
        RTReqFree(pReq);
        pReq = NULL;
    }
    if (!(fFlags & RTREQFLAGS_NO_WAIT))
    {
        *ppReq = pReq;
        LogFlow(("RTReqCallV: returns %Rrc *ppReq=%p\n", rc, pReq));
    }
    else
        LogFlow(("RTReqCallV: returns %Rrc\n", rc));
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}
RT_EXPORT_SYMBOL(RTReqQueueCallV);


RTDECL(bool) RTReqQueueIsBusy(RTREQQUEUE hQueue)
{
    PRTREQQUEUEINT pQueue = hQueue;
    AssertPtrReturn(pQueue, false);

    if (ASMAtomicReadBool(&pQueue->fBusy))
        return true;
    if (ASMAtomicReadPtrT(&pQueue->pReqs, PRTREQ) != NULL)
        return true;
    if (ASMAtomicReadBool(&pQueue->fBusy))
        return true;
    return false;
}
RT_EXPORT_SYMBOL(RTReqQueueIsBusy);


/**
 * Joins the list pList with whatever is linked up at *pHead.
 */
static void vmr3ReqJoinFreeSub(volatile PRTREQ *ppHead, PRTREQ pList)
{
    for (unsigned cIterations = 0;; cIterations++)
    {
        PRTREQ pHead = ASMAtomicXchgPtrT(ppHead, pList, PRTREQ);
        if (!pHead)
            return;
        PRTREQ pTail = pHead;
        while (pTail->pNext)
            pTail = pTail->pNext;
        pTail->pNext = pList;
        if (ASMAtomicCmpXchgPtr(ppHead, pHead, pList))
            return;
        pTail->pNext = NULL;
        if (ASMAtomicCmpXchgPtr(ppHead, pHead, NULL))
            return;
        pList = pHead;
        Assert(cIterations != 32);
        Assert(cIterations != 64);
    }
}


/**
 * Joins the list pList with whatever is linked up at *pHead.
 */
static void vmr3ReqJoinFree(PRTREQQUEUEINT pQueue, PRTREQ pList)
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
            vmr3ReqJoinFreeSub(&pQueue->apReqFree[(i + 2) % RT_ELEMENTS(pQueue->apReqFree)], pTail->pNext);

            pTail->pNext = NULL;
            vmr3ReqJoinFreeSub(&pQueue->apReqFree[(i + 2 + (i == pQueue->iReqFree)) % RT_ELEMENTS(pQueue->apReqFree)], pTail->pNext);
            return;
        }
        pTail = pTail->pNext;
    }
    vmr3ReqJoinFreeSub(&pQueue->apReqFree[(pQueue->iReqFree + 2) % RT_ELEMENTS(pQueue->apReqFree)], pList);
}


RTDECL(int) RTReqQueueAlloc(RTREQQUEUE hQueue, PRTREQ *ppReq, RTREQTYPE enmType)
{
    /*
     * Validate input.
     */
    PRTREQQUEUEINT pQueue = hQueue;
    AssertPtrReturn(pQueue, VERR_INVALID_HANDLE);
    AssertReturn(pQueue->u32Magic == RTREQQUEUE_MAGIC, VERR_INVALID_HANDLE);
    AssertMsgReturn(enmType > RTREQTYPE_INVALID && enmType < RTREQTYPE_MAX, ("%d\n", enmType), VERR_RT_REQUEST_INVALID_TYPE);

    /*
     * Try get a recycled packet.
     * While this could all be solved with a single list with a lock, it's a sport
     * of mine to avoid locks.
     */
    int cTries = RT_ELEMENTS(pQueue->apReqFree) * 2;
    while (--cTries >= 0)
    {
        PRTREQ volatile *ppHead = &pQueue->apReqFree[ASMAtomicIncU32(&pQueue->iReqFree) % RT_ELEMENTS(pQueue->apReqFree)];
#if 0 /* sad, but this won't work safely because the reading of pReq->pNext. */
        PRTREQ pNext = NULL;
        PRTREQ pReq = *ppHead;
        if (    pReq
            &&  !ASMAtomicCmpXchgPtr(ppHead, (pNext = pReq->pNext), pReq)
            &&  (pReq = *ppHead)
            &&  !ASMAtomicCmpXchgPtr(ppHead, (pNext = pReq->pNext), pReq))
            pReq = NULL;
        if (pReq)
        {
            Assert(pReq->pNext == pNext); NOREF(pReq);
#else
        PRTREQ pReq = ASMAtomicXchgPtrT(ppHead, NULL, PRTREQ);
        if (pReq)
        {
            PRTREQ pNext = pReq->pNext;
            if (    pNext
                &&  !ASMAtomicCmpXchgPtr(ppHead, pNext, NULL))
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
                    AssertMsgFailed(("rc=%Rrc from RTSemEventWait(%#x).\n", rc, pReq->EventSem));
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
            Assert(pReq->u32Magic == RTREQ_MAGIC);
            Assert(pReq->enmType  == RTREQTYPE_INVALID);
            Assert(pReq->enmState == RTREQSTATE_FREE);
            Assert(!pReq->fPoolOrQueue);
            Assert(pReq->uOwner.hQueue == pQueue);
            ASMAtomicWriteNullPtr(&pReq->pNext);
            pReq->iStatus  = VERR_RT_REQUEST_STATUS_STILL_PENDING;
            pReq->enmState = RTREQSTATE_ALLOCATED;
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
    pReq->u32Magic      = RTREQ_MAGIC;
    pReq->fEventSemClear= true;
    pReq->fPoolOrQueue  = false;
    pReq->iStatus       = VERR_RT_REQUEST_STATUS_STILL_PENDING;
    pReq->enmState      = RTREQSTATE_ALLOCATED;
    pReq->pNext         = NULL;
    pReq->uOwner.hQueue = pQueue;
    pReq->fFlags        = RTREQFLAGS_IPRT_STATUS;
    pReq->enmType       = enmType;

    *ppReq = pReq;
    LogFlow(("RTReqAlloc: returns VINF_SUCCESS *ppReq=%p new\n", pReq));
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTReqQueueAlloc);

