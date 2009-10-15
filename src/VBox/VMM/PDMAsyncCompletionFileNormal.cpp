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
#include <iprt/asm.h>
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
 * Sorts the endpoint list with insertion sort.
 */
static void pdmacFileAioMgrNormalEndpointsSortByLoad(PPDMACEPFILEMGR pAioMgr)
{
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpPrev, pEpCurr, pEpNextToSort;

    pEpPrev = pAioMgr->pEndpointsHead;
    pEpCurr = pEpPrev->AioMgr.pEndpointNext;

    while (pEpCurr)
    {
        /* Remember the next element to sort because the list might change. */
        pEpNextToSort = pEpCurr->AioMgr.pEndpointNext;

        /* Unlink the current element from the list. */
        PPDMASYNCCOMPLETIONENDPOINTFILE pPrev = pEpCurr->AioMgr.pEndpointPrev;
        PPDMASYNCCOMPLETIONENDPOINTFILE pNext = pEpCurr->AioMgr.pEndpointNext;

        if (pPrev)
            pPrev->AioMgr.pEndpointNext = pNext;
        else
            pAioMgr->pEndpointsHead = pNext;

        if (pNext)
            pNext->AioMgr.pEndpointPrev = pPrev;

        /* Go back until we reached the place to insert the current endpoint into. */
        while (pEpPrev && (pEpPrev->AioMgr.cReqsPerSec < pEpCurr->AioMgr.cReqsPerSec))
            pEpPrev = pEpPrev->AioMgr.pEndpointPrev;

        /* Link the endpoint into the list. */
        if (pEpPrev)
            pNext = pEpPrev->AioMgr.pEndpointNext;
        else
            pNext = pAioMgr->pEndpointsHead;

        pEpCurr->AioMgr.pEndpointNext = pNext;
        pEpCurr->AioMgr.pEndpointPrev = pEpPrev;
        pNext->AioMgr.pEndpointPrev = pEpCurr;
        if (pEpPrev)
            pEpPrev->AioMgr.pEndpointNext = pEpCurr;
        else
            pAioMgr->pEndpointsHead = pEpCurr;

        pEpCurr = pEpNextToSort;
    }

#ifdef DEBUG
    /* Validate sorting alogrithm */
    unsigned cEndpoints = 0;
    pEpCurr = pAioMgr->pEndpointsHead;

    AssertMsg(pEpCurr, ("No endpoint in the list?\n"));
    AssertMsg(!pEpCurr->AioMgr.pEndpointPrev, ("First element in the list points to previous element\n"));

    while (pEpCurr)
    {
        cEndpoints++;

        PPDMASYNCCOMPLETIONENDPOINTFILE pNext = pEpCurr->AioMgr.pEndpointNext;
        PPDMASYNCCOMPLETIONENDPOINTFILE pPrev = pEpCurr->AioMgr.pEndpointPrev;

        Assert(!pNext || pNext->AioMgr.cReqsPerSec <= pEpCurr->AioMgr.cReqsPerSec);
        Assert(!pPrev || pPrev->AioMgr.cReqsPerSec >= pEpCurr->AioMgr.cReqsPerSec);

        pEpCurr = pNext;
    }

    AssertMsg(cEndpoints == pAioMgr->cEndpoints, ("Endpoints lost during sort!\n"));

#endif
}

/**
 * Removes an endpoint from the currently assigned manager.
 *
 * @returns TRUE if there are still requests pending on the current manager for this endpoint.
 *          FALSE otherwise.
 * @param   pEndpointRemove    The endpoint to remove.
 */
static bool pdmacFileAioMgrNormalRemoveEndpoint(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointRemove)
{
    PPDMASYNCCOMPLETIONENDPOINTFILE pPrev   = pEndpointRemove->AioMgr.pEndpointPrev;
    PPDMASYNCCOMPLETIONENDPOINTFILE pNext   = pEndpointRemove->AioMgr.pEndpointNext;
    PPDMACEPFILEMGR                 pAioMgr = pEndpointRemove->pAioMgr;

    pAioMgr->cEndpoints--;

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
        int rc = RTFileOpen(&pEndpointRemove->File, pEndpointRemove->Core.pszUri, pEndpointRemove->fFlags);
        AssertRC(rc);
        return false;
    }

    return true;
}

/**
 * Creates a new I/O manager and spreads the I/O load of the endpoints
 * between the given I/O manager and the new one.
 *
 * @returns nothing.
 * @param   pAioMgr    The I/O manager with high I/O load.
 */
static void pdmacFileAioMgrNormalBalanceLoad(PPDMACEPFILEMGR pAioMgr)
{
    PPDMACEPFILEMGR pAioMgrNew = NULL;
    int rc = VINF_SUCCESS;

    /* Splitting can't be done with only one open endpoint. */
    if (pAioMgr->cEndpoints > 1)
    {
        rc = pdmacFileAioMgrCreate((PPDMASYNCCOMPLETIONEPCLASSFILE)pAioMgr->pEndpointsHead->Core.pEpClass,
                                   &pAioMgrNew);
        if (RT_SUCCESS(rc))
        {
            /* We will sort the list by request count per second. */
            pdmacFileAioMgrNormalEndpointsSortByLoad(pAioMgr);

            /* Now move some endpoints to the new manager. */
            unsigned cReqsHere  = pAioMgr->pEndpointsHead->AioMgr.cReqsPerSec;
            unsigned cReqsOther = 0;
            PPDMASYNCCOMPLETIONENDPOINTFILE pCurr = pAioMgr->pEndpointsHead->AioMgr.pEndpointNext;

            while (pCurr)
            {
                if (cReqsHere <= cReqsOther)
                {
                    /*
                     * The other manager has more requests to handle now.
                     * We will keep the current endpoint.
                     */
                    Log(("Keeping endpoint %#p{%s} with %u reqs/s\n", pCurr->Core.pszUri, pCurr->AioMgr.cReqsPerSec));
                    cReqsHere += pCurr->AioMgr.cReqsPerSec;
                    pCurr = pCurr->AioMgr.pEndpointNext;
                }
                else
                {
                    /* Move to other endpoint. */
                    Log(("Moving endpoint %#p{%s} with %u reqs/s to other manager\n", pCurr, pCurr->Core.pszUri, pCurr->AioMgr.cReqsPerSec));
                    cReqsOther += pCurr->AioMgr.cReqsPerSec;

                    PPDMASYNCCOMPLETIONENDPOINTFILE pMove = pCurr;

                    pCurr = pCurr->AioMgr.pEndpointNext;

                    bool fReqsPending = pdmacFileAioMgrNormalRemoveEndpoint(pMove);

                    if (fReqsPending)
                    {
                        pMove->enmState          = PDMASYNCCOMPLETIONENDPOINTFILESTATE_REMOVING;
                        pMove->AioMgr.fMoving    = true;
                        pMove->AioMgr.pAioMgrDst = pAioMgrNew;
                    }
                    else
                    {
                        pMove->AioMgr.fMoving    = false;
                        pMove->AioMgr.pAioMgrDst = NULL;
                        pdmacFileAioMgrAddEndpoint(pAioMgrNew, pMove);
                    }
                }
            }
        }
        else
        {
            /* Don't process further but leave a log entry about reduced performance. */
            LogRel(("AIOMgr: Could not create new I/O manager (rc=%Rrc). Expect reduced performance\n", rc));
        }
    }
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

/**
 * Put a list of tasks in the pending request list of an endpoint.
 */
DECLINLINE(void) pdmacFileAioMgrEpAddTaskList(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMACTASKFILE pTaskHead)
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

/**
 * Put one task in the pending request list of an endpoint.
 */
DECLINLINE(void) pdmacFileAioMgrEpAddTask(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMACTASKFILE pTask)
{
    /* Add the rest of the tasks to the pending list */
    if (!pEndpoint->AioMgr.pReqsPendingHead)
    {
        Assert(!pEndpoint->AioMgr.pReqsPendingTail);
        pEndpoint->AioMgr.pReqsPendingHead = pTask;
    }
    else
    {
        Assert(pEndpoint->AioMgr.pReqsPendingTail);
        pEndpoint->AioMgr.pReqsPendingTail->pNext = pTask;
    }

    pEndpoint->AioMgr.pReqsPendingTail = pTask;
}

/**
 * Wrapper around RTFIleAioCtxSubmit() which is also doing error handling.
 */
static int pdmacFileAioMgrNormalReqsEnqueue(PPDMACEPFILEMGR pAioMgr,
                                            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                                            PRTFILEAIOREQ pahReqs, size_t cReqs)
{
    int rc;

    pAioMgr->cRequestsActive += cReqs;
    pEndpoint->AioMgr.cRequestsActive += cReqs;

    LogFlow(("Enqueuing %d requests. I/O manager has a total of %d active requests now\n", cReqs, pAioMgr->cRequestsActive));
    LogFlow(("Endpoint has a total of %d active requests now\n", pEndpoint->AioMgr.cRequestsActive));

    rc = RTFileAioCtxSubmit(pAioMgr->hAioCtx, pahReqs, cReqs);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_FILE_AIO_INSUFFICIENT_RESSOURCES)
        {
            PPDMASYNCCOMPLETIONEPCLASSFILE pEpClass = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

            /*
             * We run out of resources.
             * Need to check which requests got queued
             * and put the rest on the pending list again.
             */
            if (RT_UNLIKELY(!pEpClass->fOutOfResourcesWarningPrinted))
            {
                pEpClass->fOutOfResourcesWarningPrinted = true;
                LogRel(("AIOMgr: The operating system doesn't have enough resources "
                        "to handle the I/O load of the VM. Expect reduced I/O performance\n"));
            }

            for (size_t i = 0; i < cReqs; i++)
            {
                int rcReq = RTFileAioReqGetRC(pahReqs[i], NULL);

                if (rcReq != VERR_FILE_AIO_IN_PROGRESS)
                {
                    AssertMsg(rcReq == VERR_FILE_AIO_NOT_SUBMITTED,
                              ("Request returned unexpected return code: rc=%Rrc\n", rcReq));

                    PPDMACTASKFILE pTask = (PPDMACTASKFILE)RTFileAioReqGetUser(pahReqs[i]);

                    /* Put the entry on the free array */
                    pAioMgr->pahReqsFree[pAioMgr->iFreeEntryNext] = pahReqs[i];
                    pAioMgr->iFreeEntryNext = (pAioMgr->iFreeEntryNext + 1) % pAioMgr->cReqEntries;

                    pdmacFileAioMgrEpAddTask(pEndpoint, pTask);
                    pAioMgr->cRequestsActive--;
                    pEndpoint->AioMgr.cRequestsActive--;
                }
            }
            LogFlow(("Removed requests. I/O manager has a total of %d active requests now\n", pAioMgr->cRequestsActive));
            LogFlow(("Endpoint has a total of %d active requests now\n", pEndpoint->AioMgr.cRequestsActive));
        }
        else
            AssertMsgFailed(("Unexpected return code rc=%Rrc\n", rc));
    }

    return rc;
}

static int pdmacFileAioMgrNormalProcessTaskList(PPDMACTASKFILE pTaskHead,
                                                PPDMACEPFILEMGR pAioMgr,
                                                PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    RTFILEAIOREQ  apReqs[20];
    unsigned      cRequests = 0;
    unsigned      cMaxRequests = PDMACEPFILEMGR_REQS_MAX - pAioMgr->cRequestsActive;
    int           rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

    AssertMsg(pEndpoint->enmState == PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE,
              ("Trying to process request lists of a non active endpoint!\n"));

    /* Go through the list and queue the requests until we get a flush request */
    while (   pTaskHead
           && !pEndpoint->pFlushReq
           && (cMaxRequests > 0)
           && RT_SUCCESS(rc))
    {
        PPDMACTASKFILE pCurr = pTaskHead;

        pTaskHead = pTaskHead->pNext;

        pCurr->pNext = NULL;

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

                /* Check if the alignment requirements are met.
                 * Offset, transfer size and buffer address
                 * need to be on a 512 boundary. */
                RTFOFF offStart = pCurr->Off & ~(RTFOFF)(512-1);
                size_t cbToTransfer = RT_ALIGN_Z(pCurr->DataSeg.cbSeg + (pCurr->Off - offStart), 512);
                PDMACTASKFILETRANSFER enmTransferType = pCurr->enmTransferType;

                AssertMsg(   pCurr->enmTransferType == PDMACTASKFILETRANSFER_WRITE
                          || (uint64_t)(offStart + cbToTransfer) <= pEndpoint->cbFile,
                          ("Read exceeds file size offStart=%RTfoff cbToTransfer=%d cbFile=%llu\n",
                          offStart, cbToTransfer, pEndpoint->cbFile));

                pCurr->fPrefetch = false;

                if (   RT_UNLIKELY(cbToTransfer != pCurr->DataSeg.cbSeg)
                    || RT_UNLIKELY(offStart != pCurr->Off)
                    || ((pEpClassFile->uBitmaskAlignment & (RTR3UINTPTR)pvBuf) != (RTR3UINTPTR)pvBuf))
                {
                    LogFlow(("Using bounce buffer for task %#p cbToTransfer=%zd cbSeg=%zd offStart=%RTfoff off=%RTfoff\n",
                             pCurr, cbToTransfer, pCurr->DataSeg.cbSeg, offStart, pCurr->Off));

                    /* Create bounce buffer. */
                    pCurr->fBounceBuffer = true;

                    AssertMsg(pCurr->Off >= offStart, ("Overflow in calculation Off=%llu offStart=%llu\n",
                              pCurr->Off, offStart));
                    pCurr->uBounceBufOffset = pCurr->Off - offStart;

                    /** @todo: I think we need something like a RTMemAllocAligned method here.
                     * Current assumption is that the maximum alignment is 4096byte
                     * (GPT disk on Windows)
                     * so we can use RTMemPageAlloc here.
                     */
                    pCurr->pvBounceBuffer = RTMemPageAlloc(cbToTransfer);
                    AssertPtr(pCurr->pvBounceBuffer);
                    pvBuf = pCurr->pvBounceBuffer;

                    if (pCurr->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
                    {
                        if (   RT_UNLIKELY(cbToTransfer != pCurr->DataSeg.cbSeg)
                            || RT_UNLIKELY(offStart != pCurr->Off))
                        {
                            /* We have to fill the buffer first before we can update the data. */
                            LogFlow(("Prefetching data for task %#p\n", pCurr));
                            pCurr->fPrefetch = true;
                            enmTransferType = PDMACTASKFILETRANSFER_READ;
                        }
                        else
                            memcpy(pvBuf, pCurr->DataSeg.pvSeg, pCurr->DataSeg.cbSeg);
                    }
                }
                else
                    pCurr->fBounceBuffer = false;

                AssertMsg((pEpClassFile->uBitmaskAlignment & (RTR3UINTPTR)pvBuf) == (RTR3UINTPTR)pvBuf,
                          ("AIO: Alignment restrictions not met! pvBuf=%p uBitmaskAlignment=%p\n", pvBuf, pEpClassFile->uBitmaskAlignment));

                if (enmTransferType == PDMACTASKFILETRANSFER_WRITE)
                {
                    /* Grow the file if needed. */
                    if (RT_UNLIKELY((uint64_t)(pCurr->Off + pCurr->DataSeg.cbSeg) > pEndpoint->cbFile))
                    {
                        ASMAtomicWriteU64(&pEndpoint->cbFile, pCurr->Off + pCurr->DataSeg.cbSeg);
                        RTFileSetSize(pEndpoint->File, pCurr->Off + pCurr->DataSeg.cbSeg);
                    }

                    rc = RTFileAioReqPrepareWrite(hReq, pEndpoint->File,
                                                  offStart, pvBuf, cbToTransfer, pCurr);
                }
                else
                    rc = RTFileAioReqPrepareRead(hReq, pEndpoint->File,
                                                 offStart, pvBuf, cbToTransfer, pCurr);
                AssertRC(rc);

                apReqs[cRequests] = hReq;
                pEndpoint->AioMgr.cReqsProcessed++;
                cMaxRequests--;
                cRequests++;
                if (cRequests == RT_ELEMENTS(apReqs))
                {
                    rc = pdmacFileAioMgrNormalReqsEnqueue(pAioMgr, pEndpoint, apReqs, cRequests);
                    cRequests = 0;
                    AssertMsg(RT_SUCCESS(rc) || (rc == VERR_FILE_AIO_INSUFFICIENT_RESSOURCES),
                              ("Unexpected return code\n"));
                }
                break;
            }
            default:
                AssertMsgFailed(("Invalid transfer type %d\n", pCurr->enmTransferType));
        }
    }

    if (cRequests)
    {
        rc = pdmacFileAioMgrNormalReqsEnqueue(pAioMgr, pEndpoint, apReqs, cRequests);
        AssertMsg(RT_SUCCESS(rc) || (rc == VERR_FILE_AIO_INSUFFICIENT_RESSOURCES),
                  ("Unexpected return code rc=%Rrc\n", rc));
    }

    if (pTaskHead)
    {
        /* Add the rest of the tasks to the pending list */
        pdmacFileAioMgrEpAddTaskList(pEndpoint, pTaskHead);

        if (RT_UNLIKELY(!cMaxRequests && !pEndpoint->pFlushReq))
        {
            /*
             * The I/O manager has no room left for more requests
             * but there are still requests to process.
             * Create a new I/O manager and let it handle some endpoints.
             */
            pdmacFileAioMgrNormalBalanceLoad(pAioMgr);
        }
    }

    /* Insufficient resources are not fatal. */
    if (rc == VERR_FILE_AIO_INSUFFICIENT_RESSOURCES)
        rc = VINF_SUCCESS;

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
        LogFlow(("Queuing pending requests first\n"));

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

    if (!pEndpoint->pFlushReq && !pEndpoint->AioMgr.pReqsPendingHead)
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

    LogFlowFunc((": Enter\n"));

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
            pAioMgr->cEndpoints++;
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointRemove = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.RemoveEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointRemove), ("Removing endpoint event without a endpoint to remove\n"));

            pEndpointRemove->enmState = PDMASYNCCOMPLETIONENDPOINTFILESTATE_REMOVING;
            fNotifyWaiter = !pdmacFileAioMgrNormalRemoveEndpoint(pEndpointRemove);
            break;
        }
        case PDMACEPFILEAIOMGRBLOCKINGEVENT_CLOSE_ENDPOINT:
        {
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointClose = (PPDMASYNCCOMPLETIONENDPOINTFILE)ASMAtomicReadPtr((void * volatile *)&pAioMgr->BlockingEventData.CloseEndpoint.pEndpoint);
            AssertMsg(VALID_PTR(pEndpointClose), ("Close endpoint event without a endpoint to close\n"));

            LogFlowFunc((": Closing endpoint %#p{%s}\n", pEndpointClose, pEndpointClose->Core.pszUri));

            /* Make sure all tasks finished. Process the queues a last time first. */
            rc = pdmacFileAioMgrNormalQueueReqs(pAioMgr, pEndpointClose);
            AssertRC(rc);

            pEndpointClose->enmState = PDMASYNCCOMPLETIONENDPOINTFILESTATE_CLOSING;
            fNotifyWaiter = !pdmacFileAioMgrNormalRemoveEndpoint(pEndpointClose);
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
        LogFlow(("Signalling waiter\n"));
        rc = RTSemEventSignal(pAioMgr->EventSemBlock);
        AssertRC(rc);
    }

    LogFlowFunc((": Leave\n"));
    return rc;
}

/**
 * Checks all endpoints for pending events or new requests.
 *
 * @returns VBox status code.
 * @param   pAioMgr    The I/O manager handle.
 */
static int pdmacFileAioMgrNormalCheckEndpoints(PPDMACEPFILEMGR pAioMgr)
{
    /* Check the assigned endpoints for new tasks if there isn't a flush request active at the moment. */
    int rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = pAioMgr->pEndpointsHead;

    while (pEndpoint)
    {
        if (!pEndpoint->pFlushReq && (pEndpoint->enmState == PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE))
        {
            rc = pdmacFileAioMgrNormalQueueReqs(pAioMgr, pEndpoint);
            if (RT_FAILURE(rc))
                return rc;
        }
        else if (!pEndpoint->AioMgr.cRequestsActive)
        {
            /* Reopen the file so that the new endpoint can reassociate with the file */
            RTFileClose(pEndpoint->File);
            rc = RTFileOpen(&pEndpoint->File, pEndpoint->Core.pszUri, pEndpoint->fFlags);
            AssertRC(rc);

            if (pEndpoint->AioMgr.fMoving)
            {
                pEndpoint->AioMgr.fMoving = false;
                pdmacFileAioMgrAddEndpoint(pEndpoint->AioMgr.pAioMgrDst, pEndpoint);
            }
            else
            {
                Assert(pAioMgr->fBlockingEventPending);
                ASMAtomicWriteBool(&pAioMgr->fBlockingEventPending, false);

                /* Release the waiting thread. */
                LogFlow(("Signalling waiter\n"));
                rc = RTSemEventSignal(pAioMgr->EventSemBlock);
                AssertRC(rc);    
            }
        }

        pEndpoint = pEndpoint->AioMgr.pEndpointNext;
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
            /* We got woken up because an endpoint issued new requests. Queue them. */
            rc = pdmacFileAioMgrNormalCheckEndpoints(pAioMgr);
            CHECK_RC(pAioMgr, rc);

            while (pAioMgr->cRequestsActive)
            {
                RTFILEAIOREQ  apReqs[20];
                uint32_t cReqsCompleted = 0;
                size_t cReqsWait;

                if (pAioMgr->cRequestsActive > RT_ELEMENTS(apReqs))
                    cReqsWait = RT_ELEMENTS(apReqs);
                else
                    cReqsWait = pAioMgr->cRequestsActive;

                LogFlow(("Waiting for %d of %d tasks to complete\n", pAioMgr->cRequestsActive, cReqsWait));

                rc = RTFileAioCtxWait(pAioMgr->hAioCtx,
                                      cReqsWait,
                                      RT_INDEFINITE_WAIT, apReqs,
                                      RT_ELEMENTS(apReqs), &cReqsCompleted);
                if (RT_FAILURE(rc) && (rc != VERR_INTERRUPTED))
                    CHECK_RC(pAioMgr, rc);

                LogFlow(("%d tasks completed\n", cReqsCompleted));

                for (uint32_t i = 0; i < cReqsCompleted; i++)
                {
                    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
                    size_t cbTransfered  = 0;
                    int rcReq            = RTFileAioReqGetRC(apReqs[i], &cbTransfered);
                    PPDMACTASKFILE pTask = (PPDMACTASKFILE)RTFileAioReqGetUser(apReqs[i]);

                    pEndpoint = pTask->pEndpoint;

                    AssertMsg(   RT_SUCCESS(rcReq)
                              && (   (cbTransfered == pTask->DataSeg.cbSeg)
                                  || (pTask->fBounceBuffer)),
                              ("Task didn't completed successfully (rc=%Rrc) or was incomplete (cbTransfered=%u)\n", rcReq, cbTransfered));

                    if (pTask->fPrefetch)
                    {
                        Assert(pTask->enmTransferType == PDMACTASKFILETRANSFER_WRITE);
                        Assert(pTask->fBounceBuffer);

                        memcpy(((uint8_t *)pTask->pvBounceBuffer) + pTask->uBounceBufOffset,
                                pTask->DataSeg.pvSeg,
                                pTask->DataSeg.cbSeg);

                        /* Write it now. */
                        pTask->fPrefetch = false;
                        size_t cbToTransfer = RT_ALIGN_Z(pTask->DataSeg.cbSeg, 512);
                        RTFOFF offStart = pTask->Off & ~(RTFOFF)(512-1);

                        /* Grow the file if needed. */
                        if (RT_UNLIKELY((uint64_t)(pTask->Off + pTask->DataSeg.cbSeg) > pEndpoint->cbFile))
                        {
                            ASMAtomicWriteU64(&pEndpoint->cbFile, pTask->Off + pTask->DataSeg.cbSeg);
                            RTFileSetSize(pEndpoint->File, pTask->Off + pTask->DataSeg.cbSeg);
                        }

                        rc = RTFileAioReqPrepareWrite(apReqs[i], pEndpoint->File,
                                                      offStart, pTask->pvBounceBuffer, cbToTransfer, pTask);
                        AssertRC(rc);
                        rc = RTFileAioCtxSubmit(pAioMgr->hAioCtx, &apReqs[i], 1);
                        AssertRC(rc);
                    }
                    else
                    {
                        if (pTask->fBounceBuffer)
                        {
                            if (pTask->enmTransferType == PDMACTASKFILETRANSFER_READ)
                                memcpy(pTask->DataSeg.pvSeg,
                                       ((uint8_t *)pTask->pvBounceBuffer) + pTask->uBounceBufOffset,
                                       pTask->DataSeg.cbSeg);

                            RTMemPageFree(pTask->pvBounceBuffer);
                        }

                        /* Put the entry on the free array */
                        pAioMgr->pahReqsFree[pAioMgr->iFreeEntryNext] = apReqs[i];
                        pAioMgr->iFreeEntryNext = (pAioMgr->iFreeEntryNext + 1) % pAioMgr->cReqEntries;

                        pAioMgr->cRequestsActive--;
                        pEndpoint->AioMgr.cRequestsActive--;
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

                /* Check endpoints for new requests. */
                rc = pdmacFileAioMgrNormalCheckEndpoints(pAioMgr);
                CHECK_RC(pAioMgr, rc);
            } /* while requests are active. */
        } /* if still running */
    } /* while running */

    return rc;
}

#undef CHECK_RC

