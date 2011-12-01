/* $Id$ */
/** @file
 * IPRT - Request packets
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int  rtReqProcessOne(PRTREQ pReq);



RTDECL(int) RTReqQueueCreate(RTREQQUEUE *phQueue)
{
    PRTREQQUEUEINT pQueue = (PRTREQQUEUEINT)RTMemAllocZ(sizeof(RTREQQUEUEINT));
    if (!pQueue)
        return VERR_NO_MEMORY;
    int rc = RTSemEventCreate(&pQueue->EventSem);
    if (RT_SUCCESS(rc))
    {
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

    RTSemEventDestroy(pQueue->EventSem);
    pQueue->EventSem = NIL_RTSEMEVENT;
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
            Assert(pReq->hQueue == pQueue);
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
            Assert(pReq->enmType == RTREQTYPE_INVALID);
            Assert(pReq->enmState == RTREQSTATE_FREE);
            Assert(pReq->hQueue == pQueue);
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
    pReq->hQueue   = pQueue;
    pReq->enmState = RTREQSTATE_ALLOCATED;
    pReq->iStatus  = VERR_RT_REQUEST_STATUS_STILL_PENDING;
    pReq->fEventSemClear = true;
    pReq->fFlags   = RTREQFLAGS_IPRT_STATUS;
    pReq->enmType  = enmType;

    *ppReq = pReq;
    LogFlow(("RTReqAlloc: returns VINF_SUCCESS *ppReq=%p new\n", pReq));
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTReqQueueAlloc);


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

    PRTREQQUEUEINT pQueue = pReq->hQueue;
    if (pQueue->cReqFree < 128)
    {
        ASMAtomicIncU32(&pQueue->cReqFree);
        PRTREQ volatile *ppHead = &pQueue->apReqFree[ASMAtomicIncU32(&pQueue->iReqFree) % RT_ELEMENTS(pQueue->apReqFree)];
        PRTREQ pNext;
        do
        {
            pNext = *ppHead;
            ASMAtomicWritePtr(&pReq->pNext, pNext);
        } while (!ASMAtomicCmpXchgPtr(ppHead, pReq, pNext));
    }
    else
    {
        RTSemEventDestroy(pReq->EventSem);
        RTMemFree(pReq);
    }
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTReqFree);


RTDECL(int) RTReqSubmit(PRTREQ pReq, RTMSINTERVAL cMillies)
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
    if (   !pReq->hQueue
        ||  pReq->pNext
        ||  !pReq->EventSem)
    {
        AssertMsgFailed(("Invalid request package! Anyone cooking their own packages???\n"));
        return VERR_RT_REQUEST_INVALID_PACKAGE;
    }
    if (    pReq->enmType < RTREQTYPE_INVALID
        || pReq->enmType > RTREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusively. This was verified on alloc too...\n",
                         pReq->enmType, RTREQTYPE_INVALID + 1, RTREQTYPE_MAX - 1));
        return VERR_RT_REQUEST_INVALID_TYPE;
    }

    int rc = VINF_SUCCESS;
    /*
     * Insert it.
     */
    PRTREQQUEUEINT pQueue = ((RTREQ volatile *)pReq)->hQueue;              /* volatile paranoia */
    unsigned fFlags = ((RTREQ volatile *)pReq)->fFlags;                    /* volatile paranoia */
    pReq->enmState = RTREQSTATE_QUEUED;
    PRTREQ pNext;
    do
    {
        pNext = pQueue->pReqs;
        pReq->pNext = pNext;
        ASMAtomicWriteBool(&pQueue->fBusy, true);
    } while (!ASMAtomicCmpXchgPtr(&pQueue->pReqs, pReq, pNext));

    /*
     * Notify queue thread.
     */
    RTSemEventSignal(pQueue->EventSem);

    /*
     * Wait and return.
     */
    if (!(fFlags & RTREQFLAGS_NO_WAIT))
        rc = RTReqWait(pReq, cMillies);
    LogFlow(("RTReqQueue: returns %Rrc\n", rc));
    return rc;
}
RT_EXPORT_SYMBOL(RTReqSubmit);


RTDECL(int) RTReqWait(PRTREQ pReq, RTMSINTERVAL cMillies)
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
    if (    !pReq->hQueue
        ||  !pReq->EventSem)
    {
        AssertMsgFailed(("Invalid request package! Anyone cooking their own packages???\n"));
        return VERR_RT_REQUEST_INVALID_PACKAGE;
    }
    if (    pReq->enmType < RTREQTYPE_INVALID
        ||  pReq->enmType > RTREQTYPE_MAX)
    {
        AssertMsgFailed(("Invalid package type %d valid range %d-%d inclusively. This was verified on alloc and queue too...\n",
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
    LogFlow(("RTReqWait: returns %Rrc\n", rc));
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}
RT_EXPORT_SYMBOL(RTReqWait);


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
#ifndef RT_ARCH_X86
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
#else /* RT_ARCH_X86 */
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
#endif /* RT_ARCH_X86 */
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
        LogFlow(("rtReqProcessOne: Completed request %p: rcReq=%Rrc rcRet=%Rrc - freeing it\n",
                 pReq, rcReq, rcRet));
        RTReqFree(pReq);
    }
    else
    {
        /* Notify the waiter and him free up the packet. */
        LogFlow(("rtReqProcessOne: Completed request %p: rcReq=%Rrc rcRet=%Rrc - notifying waiting thread\n",
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

