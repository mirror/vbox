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

int pdmacFileAioMgrNormalInit(PPDMACEPFILEMGR pAioMgr)
{
    int rc = VINF_SUCCESS;

    rc = RTFileAioCtxCreate(&pAioMgr->hAioCtx, RTFILEAIO_UNLIMITED_REQS);
    if (rc == VERR_OUT_OF_RANGE)
        rc = RTFileAioCtxCreate(&pAioMgr->hAioCtx, 128); /* @todo: Find better solution wrt. the request number*/

    return rc;
}

void pdmacFileAioMgrNormalDestroy(PPDMACEPFILEMGR pAioMgr)
{
    RTFileAioCtxDestroy(pAioMgr->hAioCtx);
}

/**
 * Error handler which will create the failsafe managers and destroy the failed I/O manager.
 *
 * @returns VBox status code
 * @param   pAioMgr    The I/O manager the error ocurred on.
 * @param   rc         The error code.
 */
static int pdmacFileAioMgrNormalErrorHandler(PPDMACEPFILEMGR pAioMgr, int rc)
{
    AssertMsgFailed(("Implement\n"));
    return VINF_SUCCESS;
}

static int pdmacFileAioMgrNormalProcessTaskList(PPDMASYNCCOMPLETIONTASK pTaskHead,
                                                PPDMACEPFILEMGR pAioMgr,
                                                PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    RTFILEAIOREQ  apReqs[20];
    unsigned      cRequests = 0;
    int           rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

    /* Go through the list and queue the requests until we get a flush request */
    while (pTaskHead && !pEndpoint->pFlushReq)
    {
        PPDMASYNCCOMPLETIONTASKFILE pTaskFile = (PPDMASYNCCOMPLETIONTASKFILE)pTaskHead;

        pTaskHead = pTaskHead->pNext;

        switch (pTaskFile->enmTransferType)
        {
            case PDMACTASKFILETRANSFER_FLUSH:
            {
                /* If there is no data transfer request this flush request finished immediately. */
                if (!pEndpoint->AioMgr.cRequestsActive)
                {
                    /* Task completed. Notify owner */
                    pdmR3AsyncCompletionCompleteTask(&pTaskFile->Core);
                }
                else
                {
                    pEndpoint->pFlushReq = pTaskFile;

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
                PPDMACTASKFILESEG pSeg = pTaskFile->u.DataTransfer.pSegmentsHead;
                RTFOFF offCurr = pTaskFile->u.DataTransfer.off;
                size_t cbTransfer = pTaskFile->u.DataTransfer.cbTransfer;

                AssertPtr(pSeg);

                do
                {
                    void *pvBuf = pSeg->DataSeg.pvSeg;

                    rc = RTFileAioReqCreate(&pSeg->hAioReq);
                    AssertRC(rc);

                    /* Check if the alignment requirements are met. */
                    if ((pEpClassFile->uBitmaskAlignment & (RTR3UINTPTR)pvBuf) != (RTR3UINTPTR)pvBuf)
                    {
                        /* Create bounce buffer. */
                        pSeg->fBounceBuffer = true;

                        /** @todo: I think we need something like a RTMemAllocAligned method here.
                         * Current assumption is that the maximum alignment is 4096byte
                         * (GPT disk on Windows)
                         * so we can use RTMemPageAlloc here.
                         */
                        pSeg->pvBounceBuffer = RTMemPageAlloc(pSeg->DataSeg.cbSeg);
                        AssertPtr(pSeg->pvBounceBuffer);
                        pvBuf = pSeg->pvBounceBuffer;

                        if (pTaskFile->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
                            memcpy(pvBuf, pSeg->DataSeg.pvSeg, pSeg->DataSeg.cbSeg);
                    }
                    else
                        pSeg->fBounceBuffer = false;

                    AssertMsg((pEpClassFile->uBitmaskAlignment & (RTR3UINTPTR)pvBuf) == (RTR3UINTPTR)pvBuf,
                              ("AIO: Alignment restrictions not met!\n"));

                    if (pTaskFile->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
                        rc = RTFileAioReqPrepareWrite(pSeg->hAioReq, pEndpoint->File,
                                                      offCurr, pvBuf, pSeg->DataSeg.cbSeg, pSeg);
                    else
                        rc = RTFileAioReqPrepareRead(pSeg->hAioReq, pEndpoint->File,
                                                     offCurr, pvBuf, pSeg->DataSeg.cbSeg, pSeg);
                    AssertRC(rc);

                    apReqs[cRequests] = pSeg->hAioReq;
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

                    offCurr    += pSeg->DataSeg.cbSeg;
                    cbTransfer -= pSeg->DataSeg.cbSeg;
                    pSeg = pSeg->pNext;
                } while (pSeg && RT_SUCCESS(rc));

                AssertMsg(!cbTransfer, ("Incomplete transfer cbTransfer=%u\n", cbTransfer));

                break;
            }
            default:
                AssertMsgFailed(("Invalid transfer type %d\n", pTaskFile->enmTransferType));
        }
    }

    if (cRequests)
    {
        pAioMgr->cRequestsActive += cRequests;
        rc = RTFileAioCtxSubmit(pAioMgr->hAioCtx, apReqs, cRequests);
        if (RT_FAILURE(rc))
        {
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
    PPDMASYNCCOMPLETIONTASK pTasksHead = NULL;

    Assert(!pEndpoint->pFlushReq);

    /* Check the pending list first */
    if (pEndpoint->AioMgr.pReqsPendingHead)
    {
        pTasksHead = pEndpoint->AioMgr.pReqsPendingHead;
        /*
         * Clear the list as the processing routine will insert them into the list
         * again if it gets aflush request.
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
    bool fNotifyWaiter = true;

    Assert(pAioMgr->fBlockingEventPending);

    switch (pAioMgr->enmBlockingEvent)
    {
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_ADD_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointNew = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.AddEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointNew), ("Adding endpoint event without a endpoint to add\n"));

            pEndpointNew->AioMgr.pEndpointNext = pAioMgr->pEndpointsHead;
            pEndpointNew->AioMgr.pEndpointPrev = NULL;
            if (pAioMgr->pEndpointsHead)
                pAioMgr->pEndpointsHead->AioMgr.pEndpointPrev = pEndpointNew;
            pAioMgr->pEndpointsHead = pEndpointNew;

            /* Assign the completion point to this file. */
            rc = RTFileAioCtxAssociateWithFile(pAioMgr->hAioCtx, pEndpointNew->File);
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointRemove = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.RemoveEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointRemove), ("Removing endpoint event without a endpoint to remove\n"));

            PPDMASYNCCOMPLETIONENDPOINTFILE pPrev = pEndpointRemove->AioMgr.pEndpointPrev;
            PPDMASYNCCOMPLETIONENDPOINTFILE pNext = pEndpointRemove->AioMgr.pEndpointNext;

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
                rc = RTFileOpen(&pEndpointRemove->File, pEndpointRemove->pszFilename, pEndpointRemove->fFlags);
                AssertRC(rc);
            }
            else
            {
                /* Mark the endpoint as removed and wait until all pending requests are finished. */
                pEndpointRemove->fRemovedOrClosed = true;

                /* We can't release the waiting thread here. */
                fNotifyWaiter = false;
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

            if (!pEndpointClose->AioMgr.cRequestsActive)
            {
                Assert(!pEndpointClose->pFlushReq);

                /* Reopen the file to deassociate it from the endpoint. */
                RTFileClose(pEndpointClose->File);
                rc = RTFileOpen(&pEndpointClose->File, pEndpointClose->pszFilename, pEndpointClose->fFlags);
                AssertRC(rc);
            }
            else
            {
                /* Mark the endpoint as removed and wait until all pending requests are finished. */
                pEndpointClose->fRemovedOrClosed = true;

                /* We can't release the waiting thread here. */
                fNotifyWaiter = false;
            }
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_SHUTDOWN:
            break;
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_SUSPEND:
            break;
        default:
            AssertReleaseMsgFailed(("Invalid event type %d\n", pAioMgr->enmBlockingEvent));
    }

    if (fNotifyWaiter)
    {
        ASMAtomicWriteBool(&pAioMgr->fBlockingEventPending, false);

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
        int rc2 = pdmacFileAioMgrNormalErrorHandler(pAioMgr, rc);\
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
    int rc = VINF_SUCCESS;
    PPDMACEPFILEMGR pAioMgr = (PPDMACEPFILEMGR)pvUser;

    while (!pAioMgr->fShutdown)
    {
        ASMAtomicWriteBool(&pAioMgr->fWaitingEventSem, true);
        if (!ASMAtomicReadBool(&pAioMgr->fWokenUp))
            rc = RTSemEventWait(pAioMgr->EventSem, RT_INDEFINITE_WAIT);
        ASMAtomicWriteBool(&pAioMgr->fWaitingEventSem, false);
        AssertRC(rc);

        LogFlow(("Got woken up\n"));

        /* Check for an external blocking event first. */
        if (pAioMgr->fBlockingEventPending)
        {
            rc = pdmacFileAioMgrNormalProcessBlockingEvent(pAioMgr);
            CHECK_RC(pAioMgr, rc);
        }

        /* Check the assigned endpoints for new tasks if there isn't a flush request active at the moment. */
        PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = pAioMgr->pEndpointsHead;

        while (pEndpoint)
        {
            if (!pEndpoint->pFlushReq)
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
                size_t cbTransfered = 0;
                int rcReq = RTFileAioReqGetRC(apReqs[i], &cbTransfered);
                PPDMACTASKFILESEG pTaskSeg        = (PPDMACTASKFILESEG)RTFileAioReqGetUser(apReqs[i]);
                PPDMASYNCCOMPLETIONTASKFILE pTask = pTaskSeg->pTask;
                PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = (PPDMASYNCCOMPLETIONENDPOINTFILE)pTask->Core.pEndpoint;

                AssertMsg(   RT_SUCCESS(rcReq)
                          && (cbTransfered == pTaskSeg->DataSeg.cbSeg),
                          ("Task didn't completed successfully (rc=%Rrc) or was incomplete (cbTransfered=%u)\n", rc, cbTransfered));

                if (pTaskSeg->fBounceBuffer)
                {
                    if (pTask->enmTransferType == PDMACTASKFILETRANSFER_READ)
                        memcpy(pTaskSeg->DataSeg.pvSeg, pTaskSeg->pvBounceBuffer, pTaskSeg->DataSeg.cbSeg);

                    RTMemPageFree(pTaskSeg->pvBounceBuffer);
                }

                pTask->u.DataTransfer.cSegments--;
                pAioMgr->cRequestsActive--;
                if (!pTask->u.DataTransfer.cSegments)
                {
                    /* Free all segments. */
                    PPDMACTASKFILESEG pSegCurr = pTask->u.DataTransfer.pSegmentsHead;
                    while (pSegCurr)
                    {
                        PPDMACTASKFILESEG pSegFree = pSegCurr;

                        pSegCurr = pSegCurr->pNext;

                        RTFileAioReqDestroy(pSegFree->hAioReq);
                        pdmacFileSegmentFree(pEndpoint, pSegFree);
                    }

                    pEndpoint->AioMgr.cRequestsActive--;

                    /* Task completed. Notify owner */
                    pdmR3AsyncCompletionCompleteTask(&pTask->Core);
                }

                /*
                 * If there is no request left on the endpoint but a flush request is set
                 * it completed now and we notify the owner.
                 * Furthermore we look for new requests and continue.
                 */
                if (!pEndpoint->AioMgr.cRequestsActive && pEndpoint->pFlushReq)
                {
                    pdmR3AsyncCompletionCompleteTask(&pEndpoint->pFlushReq->Core);
                    pEndpoint->pFlushReq = NULL;
                }

                if (!pEndpoint->fRemovedOrClosed)
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
                    pEndpoint->fRemovedOrClosed = false;

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
        }
    }

    return rc;
}

#undef CHECK_RC

