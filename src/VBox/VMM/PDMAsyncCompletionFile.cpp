/* $Id$ */
/** @file
 * PDM Async I/O - Transport data asynchronous in R3 using EMT.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_ASYNC_COMPLETION
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include "PDMAsyncCompletionFileInternal.h"

/**
 * Frees a task segment
 *
 * @returns nothing.
 * @param   pEndpoint    Pointer to the endpoint the segment was for.
 * @param   pSeg         The segment to free.
 */
void pdmacFileSegmentFree(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                          PPDMACTASKFILESEG pSeg)
{
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClass = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

    LogFlowFunc((": pEndpoint=%p pSeg=%p\n", pEndpoint, pSeg));

    /* Try the per endpoint cache first. */
    if (pEndpoint->cSegmentsCached < pEpClass->cSegmentsCacheMax)
    {
        /* Add it to the list. */
        pSeg->pPrev = NULL;
        pEndpoint->pSegmentsFreeTail->pNext = pSeg;
        pEndpoint->pSegmentsFreeTail        = pSeg;
        ASMAtomicIncU32(&pEndpoint->cSegmentsCached);
    }
    else if (false)
    {
        /* Bigger class cache */
    }
    else
    {
        Log(("Freeing segment %p because all caches are full\n", pSeg));
        MMR3HeapFree(pSeg);
    }
}

/**
 * Allocates a task segment
 *
 * @returns Pointer to the new task segment or NULL
 * @param   pEndpoint    Pointer to the endpoint
 */
PPDMACTASKFILESEG pdmacFileSegmentAlloc(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    PPDMACTASKFILESEG pSeg = NULL;

    /* Try the small per endpoint cache first. */
    if (pEndpoint->pSegmentsFreeHead == pEndpoint->pSegmentsFreeTail)
    {
        /* Try the bigger endpoint class cache. */
        PPDMASYNCCOMPLETIONEPCLASSFILE pEndpointClass = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

#if 0
        /* We start with the assigned slot id to distribute the load when allocating new tasks. */
        unsigned iSlot = pEndpoint->iSlotStart;
        do
        {
            pTask = (PPDMASYNCCOMPLETIONTASK)ASMAtomicXchgPtr((void * volatile *)&pEndpointClass->apTaskCache[iSlot], NULL);
            if (pTask)
                break;

            iSlot = (iSlot + 1) % RT_ELEMENTS(pEndpointClass->apTaskCache);
        } while (iSlot != pEndpoint->iSlotStart);
#endif
        if (!pSeg)
        {
            /*
             * Allocate completely new.
             * If this fails we return NULL.
             */
            int rc = MMR3HeapAllocZEx(pEndpointClass->Core.pVM, MM_TAG_PDM_ASYNC_COMPLETION,
                                      sizeof(PDMACTASKFILESEG),
                                      (void **)&pSeg);
            if (RT_FAILURE(rc))
                pSeg = NULL;

            LogFlow(("Allocated segment %p\n", pSeg));
        }
#if 0
        else
        {
            /* Remove the first element and put the rest into the slot again. */
            PPDMASYNCCOMPLETIONTASK pTaskHeadNew = pTask->pNext;

            pTaskHeadNew->pPrev = NULL;

            /* Put back into the list adding any new tasks. */
            while (true)
            {
                bool fChanged = ASMAtomicCmpXchgPtr((void * volatile *)&pEndpointClass->apTaskCache[iSlot], pTaskHeadNew, NULL);

                if (fChanged)
                    break;

                PPDMASYNCCOMPLETIONTASK pTaskHead = (PPDMASYNCCOMPLETIONTASK)ASMAtomicXchgPtr((void * volatile *)&pEndpointClass->apTaskCache[iSlot], NULL);

                /* The new task could be taken inbetween */
                if (pTaskHead)
                {
                    /* Go to the end of the probably much shorter new list. */
                    PPDMASYNCCOMPLETIONTASK pTaskTail = pTaskHead;
                    while (pTaskTail->pNext)
                        pTaskTail = pTaskTail->pNext;

                    /* Concatenate */
                    pTaskTail->pNext = pTaskHeadNew;

                    pTaskHeadNew = pTaskHead;
                }
                /* Another round trying to change the list. */
            }
            /* We got a task from the global cache so decrement the counter */
            ASMAtomicDecU32(&pEndpointClass->cTasksCached);
        }
#endif
    }
    else
    {
        /* Grab a free task from the head. */
        AssertMsg(pEndpoint->cSegmentsCached > 0, ("No segments cached but list contains more than one element\n"));

        pSeg = pEndpoint->pSegmentsFreeHead;
        pEndpoint->pSegmentsFreeHead = pSeg->pNext;
        ASMAtomicDecU32(&pEndpoint->cSegmentsCached);
    }

    pSeg->pNext = NULL;
    pSeg->pPrev = NULL;

    return pSeg;
}

PPDMASYNCCOMPLETIONTASK pdmacFileEpGetNewTasks(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    PPDMASYNCCOMPLETIONTASK pTasks = NULL;

    /*
     * Get pending tasks.
     */
    pTasks = (PPDMASYNCCOMPLETIONTASK)ASMAtomicXchgPtr((void * volatile *)&pEndpoint->pTasksNewHead, NULL);

    /* Reverse the list to process in FIFO order. */
    if (pTasks)
    {
        PPDMASYNCCOMPLETIONTASK pTask = pTasks;

        pTasks = NULL;

        while (pTask)
        {
            PPDMASYNCCOMPLETIONTASK pCur = pTask;
            pTask = pTask->pNext;
            pCur->pNext = pTasks;
            pTasks = pCur;
        }
    }

    return pTasks;
}

static void pdmacFileAioMgrWakeup(PPDMACEPFILEMGR pAioMgr)
{
    bool fWokenUp = ASMAtomicXchgBool(&pAioMgr->fWokenUp, true);

    if (!fWokenUp)
    {
        int rc = VINF_SUCCESS;
        bool fWaitingEventSem = ASMAtomicReadBool(&pAioMgr->fWaitingEventSem);

        if (fWaitingEventSem)
            rc = RTSemEventSignal(pAioMgr->EventSem);

        AssertRC(rc);
    }
}

static int pdmacFileAioMgrWaitForBlockingEvent(PPDMACEPFILEMGR pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT enmEvent)
{
    int rc = VINF_SUCCESS;

    ASMAtomicWriteU32((volatile uint32_t *)&pAioMgr->enmBlockingEvent, enmEvent);
    ASMAtomicXchgBool(&pAioMgr->fBlockingEventPending, true);

    /* Wakeup the async I/O manager */
    pdmacFileAioMgrWakeup(pAioMgr);

    /* Wait for completion. */
    rc = RTSemEventWait(pAioMgr->EventSemBlock, RT_INDEFINITE_WAIT);
    AssertRC(rc);

    ASMAtomicXchgBool(&pAioMgr->fBlockingEventPending, false);
    ASMAtomicWriteU32((volatile uint32_t *)&pAioMgr->enmBlockingEvent, PDMACEPFILEAIOMGRBLOCKINGEVENT_INVALID);

    return rc;
}

static int pdmacFileAioMgrAddEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc;

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    ASMAtomicWritePtr((void * volatile *)&pAioMgr->BlockingEventData.AddEndpoint.pEndpoint, pEndpoint);
    rc = pdmacFileAioMgrWaitForBlockingEvent(pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT_ADD_ENDPOINT);

    RTCritSectLeave(&pAioMgr->CritSectBlockingEvent);

    if (RT_SUCCESS(rc))
        ASMAtomicWritePtr((void * volatile *)&pEndpoint->pAioMgr, pAioMgr);

    return rc;
}

static int pdmacFileAioMgrRemoveEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc;

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    ASMAtomicWritePtr((void * volatile *)&pAioMgr->BlockingEventData.RemoveEndpoint.pEndpoint, pEndpoint);
    rc = pdmacFileAioMgrWaitForBlockingEvent(pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT);

    RTCritSectLeave(&pAioMgr->CritSectBlockingEvent);

    return rc;
}

static int pdmacFileAioMgrCloseEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc;

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    ASMAtomicWritePtr((void * volatile *)&pAioMgr->BlockingEventData.CloseEndpoint.pEndpoint, pEndpoint);
    rc = pdmacFileAioMgrWaitForBlockingEvent(pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT_CLOSE_ENDPOINT);

    RTCritSectLeave(&pAioMgr->CritSectBlockingEvent);

    return rc;
}

static int pdmacFileAioMgrShutdown(PPDMACEPFILEMGR pAioMgr)
{
    int rc;

    ASMAtomicXchgBool(&pAioMgr->fShutdown, true);

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    rc = pdmacFileAioMgrWaitForBlockingEvent(pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT_SHUTDOWN);

    RTCritSectLeave(&pAioMgr->CritSectBlockingEvent);

    return rc;
}

static int pdmacFileEpAddTask(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask)
{
    PPDMASYNCCOMPLETIONTASK pNext;
    do
    {
        pNext = pEndpoint->pTasksNewHead;
        pTask->Core.pNext = pNext;
    } while (!ASMAtomicCmpXchgPtr((void * volatile *)&pEndpoint->pTasksNewHead, (void *)pTask, (void *)pNext));

    pdmacFileAioMgrWakeup((PPDMACEPFILEMGR)ASMAtomicReadPtr((void * volatile *)&pEndpoint->pAioMgr));

    return VINF_SUCCESS;
}

static int pdmacFileEpTaskInitiate(PPDMASYNCCOMPLETIONTASK pTask,
                                   PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                                   PCPDMDATASEG paSegments, size_t cSegments,
                                   size_t cbTransfer, PDMACTASKFILETRANSFER enmTransfer)
{
    int rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;
    PPDMASYNCCOMPLETIONTASKFILE pTaskFile = (PPDMASYNCCOMPLETIONTASKFILE)pTask;
    PPDMACEPFILEMGR pAioMgr = pEpFile->pAioMgr;

    Assert(   (enmTransfer == PDMACTASKFILETRANSFER_READ)
           || (enmTransfer == PDMACTASKFILETRANSFER_WRITE));

    pTaskFile->enmTransferType              = enmTransfer;
    pTaskFile->u.DataTransfer.cSegments     = cSegments;
    pTaskFile->u.DataTransfer.pSegmentsHead = NULL;
    pTaskFile->u.DataTransfer.off           = off;
    pTaskFile->u.DataTransfer.cbTransfer    = cbTransfer;

    PPDMACTASKFILESEG pSeg = pdmacFileSegmentAlloc(pEpFile);

    pTaskFile->u.DataTransfer.pSegmentsHead = pSeg;

    for (unsigned i = 0; i < cSegments; i++)
    {
        pSeg->DataSeg.cbSeg = paSegments[i].cbSeg;
        pSeg->DataSeg.pvSeg = paSegments[i].pvSeg;
        pSeg->pTask         = pTaskFile;

        if (i < (cSegments-1))
        {
            /* Allocate new segment. */
            PPDMACTASKFILESEG pSegNext = pdmacFileSegmentAlloc(pEpFile);
            AssertPtr(pSeg);
            pSeg->pNext = pSegNext;
            pSeg = pSegNext;
        }
    }

    /* Send it off to the I/O manager. */
    pdmacFileEpAddTask(pEpFile, pTaskFile);

    return VINF_SUCCESS;
}

/**
 * Creates a new async I/O manager.
 *
 * @returns VBox status code.
 * @param   pEpClass    Pointer to the endpoint class data.
 * @param   ppAioMgr    Where to store the pointer to the new async I/O manager on success.
 */
static int pdmacFileAioMgrCreate(PPDMASYNCCOMPLETIONEPCLASSFILE pEpClass, PPPDMACEPFILEMGR ppAioMgr)
{
    int rc = VINF_SUCCESS;
    PPDMACEPFILEMGR pAioMgrNew;

    LogFlowFunc((": Entered\n"));

    rc = MMR3HeapAllocZEx(pEpClass->Core.pVM, MM_TAG_PDM_ASYNC_COMPLETION, sizeof(PDMACEPFILEMGR), (void **)&pAioMgrNew);
    if (RT_SUCCESS(rc))
    {
        pAioMgrNew->fFailsafe = pEpClass->fFailsafe;

        rc = RTSemEventCreate(&pAioMgrNew->EventSem);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventCreate(&pAioMgrNew->EventSemBlock);
            if (RT_SUCCESS(rc))
            {
                rc = RTCritSectInit(&pAioMgrNew->CritSectBlockingEvent);
                if (RT_SUCCESS(rc))
                {
                    /* Init the rest of the manager. */
                    rc = pdmacFileAioMgrNormalInit(pAioMgrNew);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTThreadCreateF(&pAioMgrNew->Thread,
                                             pAioMgrNew->fFailsafe
                                             ? pdmacFileAioMgrFailsafe
                                             : pdmacFileAioMgrNormal,
                                             pAioMgrNew,
                                             0,
                                             RTTHREADTYPE_IO,
                                             0,
                                             "AioMgr%d-%s", pEpClass->cAioMgrs,
                                             pAioMgrNew->fFailsafe
                                             ? "F"
                                             : "N");
                        if (RT_SUCCESS(rc))
                        {
                            /* Link it into the list. */
                            RTCritSectEnter(&pEpClass->CritSect);
                            pAioMgrNew->pNext = pEpClass->pAioMgrHead;
                            if (pEpClass->pAioMgrHead)
                                pEpClass->pAioMgrHead->pPrev = pAioMgrNew;
                            pEpClass->pAioMgrHead = pAioMgrNew;
                            pEpClass->cAioMgrs++;
                            RTCritSectLeave(&pEpClass->CritSect);

                            *ppAioMgr = pAioMgrNew;

                            Log(("PDMAC: Successfully created new file AIO Mgr {%s}\n", RTThreadGetName(pAioMgrNew->Thread)));
                            return VINF_SUCCESS;
                        }
                        pdmacFileAioMgrNormalDestroy(pAioMgrNew);
                    }
                    RTCritSectDelete(&pAioMgrNew->CritSectBlockingEvent);
                }
                RTSemEventDestroy(pAioMgrNew->EventSem);
            }
            RTSemEventDestroy(pAioMgrNew->EventSemBlock);
        }
        MMR3HeapFree(pAioMgrNew);
    }

    LogFlowFunc((": Leave rc=%Rrc\n", rc));

    return rc;
}

/**
 * Destroys a async I/O manager.
 *
 * @returns nothing.
 * @param   pAioMgr    The async I/O manager to destroy.
 */
static void pdmacFileAioMgrDestroy(PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile, PPDMACEPFILEMGR pAioMgr)
{
    /* A normal manager may have still endpoints attached and has to return them. */
    Assert(pAioMgr->fFailsafe);
    int rc = pdmacFileAioMgrShutdown(pAioMgr);
    AssertRC(rc);

    /* Unlink from the list. */
    rc = RTCritSectEnter(&pEpClassFile->CritSect);
    AssertRC(rc);

    PPDMACEPFILEMGR pPrev = pAioMgr->pPrev;
    PPDMACEPFILEMGR pNext = pAioMgr->pNext;

    if (pPrev)
        pPrev->pNext = pNext;
    else
        pEpClassFile->pAioMgrHead = pNext;

    if (pNext)
        pNext->pPrev = pPrev;

    pEpClassFile->cAioMgrs--;

    rc = RTCritSectLeave(&pEpClassFile->CritSect);
    AssertRC(rc);

    /* Free the ressources. */
    RTCritSectDelete(&pAioMgr->CritSectBlockingEvent);
    RTSemEventDestroy(pAioMgr->EventSem);
    if (!pAioMgr->fFailsafe)
        pdmacFileAioMgrNormalDestroy(pAioMgr);

    MMR3HeapFree(pAioMgr);
}

static int pdmacFileInitialize(PPDMASYNCCOMPLETIONEPCLASS pClassGlobals, PCFGMNODE pCfgNode)
{
    int rc = VINF_SUCCESS;
    RTFILEAIOLIMITS AioLimits; /** < Async I/O limitations. */

    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pClassGlobals;

    rc = RTFileAioGetLimits(&AioLimits);
    if (RT_FAILURE(rc))
    {
        LogRel(("AIO: Async I/O manager not supported (rc=%Rrc). Falling back to failsafe manager\n",
                rc));
        pEpClassFile->fFailsafe = true;
    }
    else
    {
        pEpClassFile->uBitmaskAlignment   = ~((RTR3UINTPTR)AioLimits.cbBufferAlignment - 1);
        pEpClassFile->cReqsOutstandingMax = AioLimits.cReqsOutstandingMax;
        pEpClassFile->fFailsafe = false;
    }

    /* Init critical section. */
    rc = RTCritSectInit(&pEpClassFile->CritSect);
    return rc;
}

static void pdmacFileTerminate(PPDMASYNCCOMPLETIONEPCLASS pClassGlobals)
{
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pClassGlobals;

    /* All endpoints should be closed at this point. */
    AssertMsg(!pEpClassFile->Core.pEndpointsHead, ("There are still endpoints left\n"));

    /* Destroy all left async I/O managers. */
    while (pEpClassFile->pAioMgrHead)
        pdmacFileAioMgrDestroy(pEpClassFile, pEpClassFile->pAioMgrHead);

    RTCritSectDelete(&pEpClassFile->CritSect);
}

static int pdmacFileEpInitialize(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                 const char *pszUri, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->pEpClass;

    AssertMsgReturn((fFlags & ~(PDMACEP_FILE_FLAGS_READ_ONLY | PDMACEP_FILE_FLAGS_CACHING)) == 0,
                    ("PDMAsyncCompletion: Invalid flag specified\n"), VERR_INVALID_PARAMETER);

    unsigned fFileFlags = fFlags & PDMACEP_FILE_FLAGS_READ_ONLY
                         ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                         : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE;

    if (!pEpClassFile->fFailsafe)
        fFileFlags |= (RTFILE_O_ASYNC_IO | RTFILE_O_NO_CACHE);

    pEpFile->pszFilename = RTStrDup(pszUri);
    if (!pEpFile->pszFilename)
        return VERR_NO_MEMORY;
    pEpFile->fFlags = fFileFlags;

    rc = RTFileOpen(&pEpFile->File, pszUri, fFileFlags);
    if (RT_SUCCESS(rc))
    {
        /* Initialize the segment cache */
        rc = MMR3HeapAllocZEx(pEpClassFile->Core.pVM, MM_TAG_PDM_ASYNC_COMPLETION,
                              sizeof(PDMACTASKFILESEG),
                              (void **)&pEpFile->pSegmentsFreeHead);
        if (RT_SUCCESS(rc))
        {
            PPDMACEPFILEMGR pAioMgr = NULL;

            pEpFile->pSegmentsFreeTail = pEpFile->pSegmentsFreeHead;
            pEpFile->cSegmentsCached = 0;

            if (pEpClassFile->fFailsafe)
            {
                /* Safe mode. Every file has its own async I/O manager. */
                rc = pdmacFileAioMgrCreate(pEpClassFile, &pAioMgr);
                AssertRC(rc);
            }
            else
            {
                if (fFlags & PDMACEP_FILE_FLAGS_CACHING)
                {
                    pEpFile->fCaching = true;
                }

                /* Check for an idling one or create new if not found */
                if (!pEpClassFile->pAioMgrHead)
                {
                    rc = pdmacFileAioMgrCreate(pEpClassFile, &pAioMgr);
                    AssertRC(rc);
                }
                else
                {
                    pAioMgr = pEpClassFile->pAioMgrHead;
                }
            }

            /* Assign the endpoint to the thread. */
            rc = pdmacFileAioMgrAddEndpoint(pAioMgr, pEpFile);
            if (RT_FAILURE(rc))
                MMR3HeapFree(pEpFile->pSegmentsFreeHead);
        }

        if (RT_FAILURE(rc))
            RTFileClose(pEpFile->File);
    }

    if (RT_FAILURE(rc))
        RTStrFree(pEpFile->pszFilename);

    return rc;
}

static int pdmacFileEpClose(PPDMASYNCCOMPLETIONENDPOINT pEndpoint)
{
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->pEpClass;

    /* Make sure that all tasks finished for this endpoint. */
    int rc = pdmacFileAioMgrCloseEndpoint(pEpFile->pAioMgr, pEpFile);
    AssertRC(rc);

    /* Remove the endpoint from the thread. */
    rc = pdmacFileAioMgrRemoveEndpoint(pEpFile->pAioMgr, pEpFile);
    AssertRC(rc);

    /*
     * If the async I/O manager is in failsafe mode this is the only endpoint
     * he processes and thus can be destroyed now.
     */
    if (pEpFile->pAioMgr->fFailsafe)
        pdmacFileAioMgrDestroy(pEpClassFile, pEpFile->pAioMgr);

    /* Free cached segments. */
    PPDMACTASKFILESEG pSeg = pEpFile->pSegmentsFreeHead;

    while (pSeg)
    {
        PPDMACTASKFILESEG pSegFree = pSeg;
        pSeg = pSeg->pNext;
        MMR3HeapFree(pSegFree);
    }

    /* Free the cached data. */
    Assert(!pEpFile->fCaching);

    return VINF_SUCCESS;
}

static int pdmacFileEpRead(PPDMASYNCCOMPLETIONTASK pTask,
                           PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                           PCPDMDATASEG paSegments, size_t cSegments,
                           size_t cbRead)
{
    return pdmacFileEpTaskInitiate(pTask, pEndpoint, off, paSegments, cSegments, cbRead,
                                   PDMACTASKFILETRANSFER_READ);
}

static int pdmacFileEpWrite(PPDMASYNCCOMPLETIONTASK pTask,
                            PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                            PCPDMDATASEG paSegments, size_t cSegments,
                            size_t cbWrite)
{
    return pdmacFileEpTaskInitiate(pTask, pEndpoint, off, paSegments, cSegments, cbWrite,
                                   PDMACTASKFILETRANSFER_WRITE);
}

static int pdmacFileEpFlush(PPDMASYNCCOMPLETIONTASK pTask,
                            PPDMASYNCCOMPLETIONENDPOINT pEndpoint)
{
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;
    PPDMASYNCCOMPLETIONTASKFILE pTaskFile = (PPDMASYNCCOMPLETIONTASKFILE)pTask;

    pTaskFile->enmTransferType = PDMACTASKFILETRANSFER_FLUSH;
    pdmacFileEpAddTask(pEpFile, pTaskFile);

    return VINF_SUCCESS;
}

static int pdmacFileEpGetSize(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, uint64_t *pcbSize)
{
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;

    return RTFileGetSize(pEpFile->File, pcbSize);
}

const PDMASYNCCOMPLETIONEPCLASSOPS g_PDMAsyncCompletionEndpointClassFile =
{
    /* u32Version */
    PDMAC_EPCLASS_OPS_VERSION,
    /* pcszName */
    "File",
    /* enmClassType */
    PDMASYNCCOMPLETIONEPCLASSTYPE_FILE,
    /* cbEndpointClassGlobal */
    sizeof(PDMASYNCCOMPLETIONEPCLASSFILE),
    /* cbEndpoint */
    sizeof(PDMASYNCCOMPLETIONENDPOINTFILE),
    /* cbTask */
    sizeof(PDMASYNCCOMPLETIONTASKFILE),
    /* pfnInitialize */
    pdmacFileInitialize,
    /* pfnTerminate */
    pdmacFileTerminate,
    /* pfnEpInitialize. */
    pdmacFileEpInitialize,
    /* pfnEpClose */
    pdmacFileEpClose,
    /* pfnEpRead */
    pdmacFileEpRead,
    /* pfnEpWrite */
    pdmacFileEpWrite,
    /* pfnEpFlush */
    pdmacFileEpFlush,
    /* pfnEpGetSize */
    pdmacFileEpGetSize,
    /* u32VersionEnd */
    PDMAC_EPCLASS_OPS_VERSION
};

