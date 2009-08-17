/* $Id$ */
/** @file
 * PDM Async I/O - Transport data asynchronous in R3 using EMT.
 * Async File I/O manager.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#define LOG_GROUP LOG_GROUP_PDM_ASYNC_COMPLETION
#include <iprt/types.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <VBox/log.h>

#include "PDMAsyncCompletionFileInternal.h"

/** The update period for the I/O load statistics in ms. */
#define PDMACEPFILEMGR_LOAD_UPDATE_PERIOD 1000
/** Maximum number of requests a manager will handle. */
#define PDMACEPFILEMGR_REQS_MAX 512 /* @todo: Find better solution wrt. the request number*/

int pdmacFileAioMgrNormalInit(PPDMACEPFILEMGR pAioMgr)
{
    int rc = VINF_SUCCESS;

    rc = RTFileAioCtxCreate(&pAioMgr->hAioCtx, RTFILEAIO_UNLIMITED_REQS);
    if (rc == VERR_OUT_OF_RANGE)
        rc = RTFileAioCtxCreate(&pAioMgr->hAioCtx, PDMACEPFILEMGR_REQS_MAX);

    if (RT_SUCCESS(rc))
    {
        /* Initialize request handle array. */
        pAioMgr->iFreeEntryNext = 0;
        pAioMgr->iFreeReqNext   = 0;
        pAioMgr->cReqEntries    = PDMACEPFILEMGR_REQS_MAX + 1;
        pAioMgr->pahReqsFree    = (RTFILEAIOREQ *)RTMemAllocZ(pAioMgr->cReqEntries * sizeof(RTFILEAIOREQ));

        if (pAioMgr->pahReqsFree)
        {
            return VINF_SUCCESS;
        }
        else
        {
            RTFileAioCtxDestroy(pAioMgr->hAioCtx);
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

void pdmacFileAioMgrNormalDestroy(PPDMACEPFILEMGR pAioMgr)
{
    RTFileAioCtxDestroy(pAioMgr->hAioCtx);

    while (pAioMgr->iFreeReqNext != pAioMgr->iFreeEntryNext)
    {
        RTFileAioReqDestroy(pAioMgr->pahReqsFree[pAioMgr->iFreeReqNext]);
        pAioMgr->iFreeReqNext = (pAioMgr->iFreeReqNext + 1) % pAioMgr->cReqEntries;
    }

    RTMemFree(pAioMgr->pahReqsFree);
}

/**
 * Error handler which will create the failsafe managers and destroy the failed I/O manager.
 *
 * @returns VBox status code
 * @param   pAioMgr    The I/O manager the error ocurred on.
 * @param   rc         The error code.
 */
static int pdmacFileAioMgrNormalErrorHandler(PPDMACEPFILEMGR pAioMgr, int rc, RT_SRC_POS_DECL)
{
    LogRel(("AIOMgr: I/O manager %#p encountered a critical error (rc=%Rrc) during operation. Falling back to failsafe mode. Expect reduced performance\n",
            pAioMgr, rc));
    LogRel(("AIOMgr: Error happened in %s:(%u){%s}\n", RT_SRC_POS_ARGS));
    LogRel(("AIOMgr: Please contact the product vendor\n"));

    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pAioMgr->pEndpointsHead->Core.pEpClass;

    pAioMgr->enmState = PDMACEPFILEMGRSTATE_FAULT;
    ASMAtomicWriteBool(&pEpClassFile->fFailsafe, true);

    AssertMsgFailed(("Implement\n"));
    return VINF_SUCCESS;
}

static int pdmacFileAioMgrNormalProcessTaskList(PPDMACTASKFILE pTaskHead,
                                                PPDMACEPFILEMGR pAioMgr,
                                                PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    RTFILEAIOREQ  apReqs[20];
    unsigned      cRequests = 0;
    int           rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

    AssertMsg(pEndpoint->enmState == PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE,
              ("Trying to process request lists of a non active endpoint!\n"));

    /* Go through the list and queue the requests until we get a flush request */
    while (pTaskHead && !pEndpoint->pFlushReq)
    {
        PPDMACTASKFILE pCurr = pTaskHead;

        pTaskHead = pTaskHead->pNext;

        AssertMsg(VALID_PTR(pCurr->pEndpoint) && (pCurr->pEndpoint == pEndpoint),
                  ("Endpoints do not match\n"));

        switch (pCurr->enmTransferType)
        {
            case PDMACTASKFILETRANSFER_FLUSH:
            {
                /* If there is no data transfer request this flush request finished immediately. */
                if (!pEndpoint->AioMgr.cRequestsActive)
                {
                    pCurr->pfnCompleted(pCurr, pCurr->pvUser);
                    pdmacFileTaskFree(pEndpoint, pCurr);
                }
                else
                {
                    pEndpoint->pFlushReq = pCurr;

                    if (pTaskHead)
                    {
                        /* Add the rest of the tasks to the pending list */
                        if (!pEndpoint->AioMgr.pReqsPendingHead)
                        {
                            Assert(!pEndpoint->AioMgr.pReqsPendingTail);
                            pEndpoint->AioMgr.pReqsPendingHead = pTaskHead;
                        }
                        else
                        {
                            Assert(pEndpoint->AioMgr.pReqsPendingTail);
                            pEndpoint->AioMgr.pReqsPendingTail->pNext = pTaskHead;
                        }

                        /* Update the tail. */
                        while (pTaskHead->pNext)
                            pTaskHead = pTaskHead->pNext;

                        pEndpoint->AioMgr.pReqsPendingTail = pTaskHead;
                    }
                }
                break;
            }
            case PDMACTASKFILETRANSFER_READ:
            case PDMACTASKFILETRANSFER_WRITE:
            {
                RTFILEAIOREQ hReq = NIL_RTFILEAIOREQ;
                void *pvBuf = pCurr->DataSeg.pvSeg;

                /* Get a request handle. */
                if (pAioMgr->iFreeReqNext != pAioMgr->iFreeEntryNext)
                {
                    hReq = pAioMgr->pahReqsFree[pAioMgr->iFreeReqNext];
                    pAioMgr->pahReqsFree[pAioMgr->iFreeReqNext] = NIL_RTFILEAIOREQ;
                    pAioMgr->iFreeReqNext = (pAioMgr->iFreeReqNext + 1) % pAioMgr->cReqEntries;
                }
                else
                {
                    rc = RTFileAioReqCreate(&hReq);
                    AssertRC(rc);
                }

                AssertMsg(hReq != NIL_RTFILEAIOREQ, ("Out of request handles\n"));

                /* Check if the alignment requirements are met. */
                if ((pEpClassFile->uBitmaskAlignment & (RTR3UINTPTR)pvBuf) != (RTR3UINTPTR)pvBuf)
                {
                    /* Create bounce buffer. */
                    pCurr->fBounceBuffer = true;

                    /** @todo: I think we need something like a RTMemAllocAligned method here.
                     * Current assumption is that the maximum alignment is 4096byte
                     * (GPT disk on Windows)
                     * so we can use RTMemPageAlloc here.
                     */
                    pCurr->pvBounceBuffer = RTMemPageAlloc(pCurr->DataSeg.cbSeg);
                    AssertPtr(pCurr->pvBounceBuffer);
                    pvBuf = pCurr->pvBounceBuffer;

                    if (pCurr->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
                        memcpy(pvBuf, pCurr->DataSeg.pvSeg, pCurr->DataSeg.cbSeg);
                }
                else
                    pCurr->fBounceBuffer = false;

                AssertMsg((pEpClassFile->uBitmaskAlignment & (RTR3UINTPTR)pvBuf) == (RTR3UINTPTR)pvBuf,
                            ("AIO: Alignment restrictions not met!\n"));

                if (pCurr->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
                    rc = RTFileAioReqPrepareWrite(hReq, pEndpoint->File,
                                                  pCurr->Off, pvBuf, pCurr->DataSeg.cbSeg, pCurr);
                else
                    rc = RTFileAioReqPrepareRead(hReq, pEndpoint->File,
                                                 pCurr->Off, pvBuf, pCurr->DataSeg.cbSeg, pCurr);
                AssertRC(rc);

                apReqs[cRequests] = hReq;
                pEndpoint->AioMgr.cReqsProcessed++;
                cRequests++;
                if (cRequests == RT_ELEMENTS(apReqs))
                {
                    pAioMgr->cRequestsActive += cRequests;
                    rc = RTFileAioCtxSubmit(pAioMgr->hAioCtx, apReqs, cRequests);
                    if (RT_FAILURE(rc))
                    {
                        /* @todo implement */
                        AssertMsgFailed(("Implement\n"));
                    }

                    cRequests = 0;
                }
                break;
            }
            default:
                AssertMsgFailed(("Invalid transfer type %d\n", pCurr->enmTransferType));
        }
    }

    if (cRequests)
    {
        pAioMgr->cRequestsActive += cRequests;
        rc = RTFileAioCtxSubmit(pAioMgr->hAioCtx, apReqs, cRequests);
        if (RT_FAILURE(rc))
        {
            /* Not enough ressources on this context anymore. */
            /* @todo implement */
            AssertMsgFailed(("Implement\n"));
        }
    }

    return rc;
}

/**
 * Adds all pending requests for the given endpoint
 * until a flush request is encountered or there is no
 * request anymore.
 *
 * @returns VBox status code.
 * @param   pAioMgr    The async I/O manager for the endpoint
 * @param   pEndpoint  The endpoint to get the requests from.
 */
static int pdmacFileAioMgrNormalQueueReqs(PPDMACEPFILEMGR pAioMgr,
                                          PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc = VINF_SUCCESS;
    PPDMACTASKFILE pTasksHead = NULL;

    AssertMsg(pEndpoint->enmState == PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE,
              ("Trying to process request lists of a non active endpoint!\n"));

    Assert(!pEndpoint->pFlushReq);

    /* Check the pending list first */
    if (pEndpoint->AioMgr.pReqsPendingHead)
    {
        pTasksHead = pEndpoint->AioMgr.pReqsPendingHead;
        /*
         * Clear the list as the processing routine will insert them into the list
         * again if it gets a flush request.
         */
        pEndpoint->AioMgr.pReqsPendingHead = NULL;
        pEndpoint->AioMgr.pReqsPendingTail = NULL;
        rc = pdmacFileAioMgrNormalProcessTaskList(pTasksHead, pAioMgr, pEndpoint);
        AssertRC(rc);
    }

    if (!pEndpoint->pFlushReq)
    {
        /* Now the request queue. */
        pTasksHead = pdmacFileEpGetNewTasks(pEndpoint);
        if (pTasksHead)
        {
            rc = pdmacFileAioMgrNormalProcessTaskList(pTasksHead, pAioMgr, pEndpoint);
            AssertRC(rc);
        }
    }

    return rc;
}

static int pdmacFileAioMgrNormalProcessBlockingEvent(PPDMACEPFILEMGR pAioMgr)
{
    int rc = VINF_SUCCESS;
    bool fNotifyWaiter = false;

    Assert(pAioMgr->fBlockingEventPending);

    switch (pAioMgr->enmBlockingEvent)
    {
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_ADD_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointNew = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.AddEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointNew), ("Adding endpoint event without a endpoint to add\n"));

            pEndpointNew->enmState = PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE;

            pEndpointNew->AioMgr.pEndpointNext = pAioMgr->pEndpointsHead;
            pEndpointNew->AioMgr.pEndpointPrev = NULL;
            if (pAioMgr->pEndpointsHead)
                pAioMgr->pEndpointsHead->AioMgr.pEndpointPrev = pEndpointNew;
            pAioMgr->pEndpointsHead = pEndpointNew;

            /* Assign the completion point to this file. */
            rc = RTFileAioCtxAssociateWithFile(pAioMgr->hAioCtx, pEndpointNew->File);
            fNotifyWaiter = true;
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointRemove = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.RemoveEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointRemove), ("Removing endpoint event without a endpoint to remove\n"));

            PPDMASYNCCOMPLETIONENDPOINTFILE pPrev = pEndpointRemove->AioMgr.pEndpointPrev;
            PPDMASYNCCOMPLETIONENDPOINTFILE pNext = pEndpointRemove->AioMgr.pEndpointNext;

            pEndpointRemove->enmState = PDMASYNCCOMPLETIONENDPOINTFILESTATE_REMOVING;

            if (pPrev)
                pPrev->AioMgr.pEndpointNext = pNext;
            else
                pAioMgr->pEndpointsHead = pNext;

            if (pNext)
                pNext->AioMgr.pEndpointPrev = pPrev;

            /* Make sure that there is no request pending on this manager for the endpoint. */
            if (!pEndpointRemove->AioMgr.cRequestsActive)
            {
                Assert(!pEndpointRemove->pFlushReq);

                /* Reopen the file so that the new endpoint can reassociate with the file */
                RTFileClose(pEndpointRemove->File);
                rc = RTFileOpen(&pEndpointRemove->File, pEndpointRemove->Core.pszUri, pEndpointRemove->fFlags);
                AssertRC(rc);
            }
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_CLOSE_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointClose = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.CloseEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointClose), ("Close endpoint event without a endpoint to close\n"));

            /* Make sure all tasks finished. Process the queues a last time first. */
            rc = pdmacFileAioMgrNormalQueueReqs(pAioMgr, pEndpointClose);
            AssertRC(rc);

            pEndpointClose->enmState = PDMASYNCCOMPLETIONENDPOINTFILESTATE_CLOSING;

            PPDMASYNCCOMPLETIONENDPOINTFILE pPrev = pEndpointClose->AioMgr.pEndpointPrev;
            PPDMASYNCCOMPLETIONENDPOINTFILE pNext = pEndpointClose->AioMgr.pEndpointNext;

            if (pPrev)
                pPrev->AioMgr.pEndpointNext = pNext;
            else
                pAioMgr->pEndpointsHead = pNext;

            if (pNext)
                pNext->AioMgr.pEndpointPrev = pPrev;

            if (!pEndpointClose->AioMgr.cRequestsActive)
            {
                Assert(!pEndpointClose->pFlushReq);

                /* Reopen the file to deassociate it from the endpoint. */
                RTFileClose(pEndpointClose->File);
                rc = RTFileOpen(&pEndpointClose->File, pEndpointClose->Core.pszUri, pEndpointClose->fFlags);
                AssertRC(rc);
                fNotifyWaiter = true;
            }
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_SHUTDOWN:
        {
            pAioMgr->enmState = PDMACEPFILEMGRSTATE_SHUTDOWN;
            if (!pAioMgr->cRequestsActive)
                fNotifyWaiter = true;
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_SUSPEND:
        {
            pAioMgr->enmState = PDMACEPFILEMGRSTATE_SUSPENDING;
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_RESUME:
        {
            pAioMgr->enmState = PDMACEPFILEMGRSTATE_RUNNING;
            fNotifyWaiter = true;
            break;
        }
        default:
            AssertReleaseMsgFailed(("Invalid event type %d\n", pAioMgr->enmBlockingEvent));
    }

    if (fNotifyWaiter)
    {
        ASMAtomicWriteBool(&pAioMgr->fBlockingEventPending, false);
        pAioMgr->enmBlockingEvent = PDMACEPFILEAIOMGRBLOCKINGEVENT_INVALID;

        /* Release the waiting thread. */
        rc = RTSemEventSignal(pAioMgr->EventSemBlock);
        AssertRC(rc);
    }

    return rc;
}

/** Helper macro for checking for error codes. */
#define CHECK_RC(pAioMgr, rc) \
    if (RT_FAILURE(rc)) \
    {\
        int rc2 = pdmacFileAioMgrNormalErrorHandler(pAioMgr, rc, RT_SRC_POS);\
        return rc2;\
    }

/**
 * The normal I/O manager using the RTFileAio* API
 *
 * @returns VBox status code.
 * @param ThreadSelf    Handle of the thread.
 * @param pvUser        Opaque user data.
 */
int pdmacFileAioMgrNormal(RTTHREAD ThreadSelf, void *pvUser)
{
    int rc                  = VINF_SUCCESS;
    PPDMACEPFILEMGR pAioMgr = (PPDMACEPFILEMGR)pvUser;
    uint64_t uMillisEnd     = RTTimeMilliTS() + PDMACEPFILEMGR_LOAD_UPDATE_PERIOD;

    while (   (pAioMgr->enmState == PDMACEPFILEMGRSTATE_RUNNING)
           || (pAioMgr->enmState == PDMACEPFILEMGRSTATE_SUSPENDING))
    {
        ASMAtomicWriteBool(&pAioMgr->fWaitingEventSem, true);
        if (!ASMAtomicReadBool(&pAioMgr->fWokenUp))
            rc = RTSemEventWait(pAioMgr->EventSem, RT_INDEFINITE_WAIT);
        ASMAtomicWriteBool(&pAioMgr->fWaitingEventSem, false);
        AssertRC(rc);

        LogFlow(("Got woken up\n"));
        ASMAtomicWriteBool(&pAioMgr->fWokenUp, false);

        /* Check for an external blocking event first. */
        if (pAioMgr->fBlockingEventPending)
        {
            rc = pdmacFileAioMgrNormalProcessBlockingEvent(pAioMgr);
            CHECK_RC(pAioMgr, rc);
        }

        if (RT_LIKELY(pAioMgr->enmState == PDMACEPFILEMGRSTATE_RUNNING))
        {
            /* Check the assigned endpoints for new tasks if there isn't a flush request active at the moment. */
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = pAioMgr->pEndpointsHead;

            while (pEndpoint)
            {
                if (!pEndpoint->pFlushReq && (pEndpoint->enmState == PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE))
                {
                    rc = pdmacFileAioMgrNormalQueueReqs(pAioMgr, pEndpoint);
                    CHECK_RC(pAioMgr, rc);
                }

                pEndpoint = pEndpoint->AioMgr.pEndpointNext;
            }

            while (pAioMgr->cRequestsActive)
            {
                RTFILEAIOREQ  apReqs[20];
                uint32_t cReqsCompleted = 0;

                rc = RTFileAioCtxWait(pAioMgr->hAioCtx, 1, RT_INDEFINITE_WAIT, apReqs,
                                      RT_ELEMENTS(apReqs), &cReqsCompleted);
                CHECK_RC(pAioMgr, rc);

                for (uint32_t i = 0; i < cReqsCompleted; i++)
                {
                    size_t cbTransfered  = 0;
                    int rcReq            = RTFileAioReqGetRC(apReqs[i], &cbTransfered);
                    PPDMACTASKFILE pTask = (PPDMACTASKFILE)RTFileAioReqGetUser(apReqs[i]);

                    pEndpoint = pTask->pEndpoint;

                    AssertMsg(   RT_SUCCESS(rcReq)
                              && (cbTransfered == pTask->DataSeg.cbSeg),
                              ("Task didn't completed successfully (rc=%Rrc) or was incomplete (cbTransfered=%u)\n", rc, cbTransfered));

                    if (pTask->fBounceBuffer)
                    {
                        if (pTask->enmTransferType == PDMACTASKFILETRANSFER_READ)
                            memcpy(pTask->DataSeg.pvSeg, pTask->pvBounceBuffer, pTask->DataSeg.cbSeg);

                        RTMemPageFree(pTask->pvBounceBuffer);
                    }

                    /* Put the entry on the free array */
                    pAioMgr->pahReqsFree[pAioMgr->iFreeEntryNext] = apReqs[i];
                    pAioMgr->iFreeEntryNext = (pAioMgr->iFreeEntryNext + 1) %pAioMgr->cReqEntries;

                    pAioMgr->cRequestsActive--;
                    pEndpoint->AioMgr.cReqsProcessed++;

                    /* Call completion callback */
                    pTask->pfnCompleted(pTask, pTask->pvUser);
                    pdmacFileTaskFree(pEndpoint, pTask);

                    /*
                     * If there is no request left on the endpoint but a flush request is set
                     * it completed now and we notify the owner.
                     * Furthermore we look for new requests and continue.
                     */
                    if (!pEndpoint->AioMgr.cRequestsActive && pEndpoint->pFlushReq)
                    {
                        /* Call completion callback */
                        pTask = pEndpoint->pFlushReq;
                        pEndpoint->pFlushReq = NULL;

                        AssertMsg(pTask->pEndpoint == pEndpoint, ("Endpoint of the flush request does not match assigned one\n"));

                        pTask->pfnCompleted(pTask, pTask->pvUser);
                        pdmacFileTaskFree(pEndpoint, pTask);
                    }

                    if (pEndpoint->enmState == PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE)
                    {
                        if (!pEndpoint->pFlushReq)
                        {
                            /* Check if there are events on the endpoint. */
                            rc = pdmacFileAioMgrNormalQueueReqs(pAioMgr, pEndpoint);
                            CHECK_RC(pAioMgr, rc);
                        }
                    }
                    else if (!pEndpoint->AioMgr.cRequestsActive)
                    {
                        Assert(pAioMgr->fBlockingEventPending);
                        ASMAtomicWriteBool(&pAioMgr->fBlockingEventPending, false);

                        /* Release the waiting thread. */
                        rc = RTSemEventSignal(pAioMgr->EventSemBlock);
                        AssertRC(rc);
                    }
                }

                /* Check for an external blocking event before we go to sleep again. */
                if (pAioMgr->fBlockingEventPending)
                {
                    rc = pdmacFileAioMgrNormalProcessBlockingEvent(pAioMgr);
                    CHECK_RC(pAioMgr, rc);
                }

                /* Update load statistics. */
                uint64_t uMillisCurr = RTTimeMilliTS();
                if (uMillisCurr > uMillisEnd)
                {
                    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointCurr = pAioMgr->pEndpointsHead;

                    /* Calculate timespan. */
                    uMillisCurr -= uMillisEnd;

                    while (pEndpointCurr)
                    {
                        pEndpointCurr->AioMgr.cReqsPerSec    = pEndpointCurr->AioMgr.cReqsProcessed / (uMillisCurr + PDMACEPFILEMGR_LOAD_UPDATE_PERIOD);
                        pEndpointCurr->AioMgr.cReqsProcessed = 0;
                        pEndpointCurr = pEndpointCurr->AioMgr.pEndpointNext;
                    }

                    /* Set new update interval */
                    uMillisEnd = RTTimeMilliTS() + PDMACEPFILEMGR_LOAD_UPDATE_PERIOD;
                }
            }
        }
    }

    return rc;
}

#undef CHECK_RC

