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

#include "PDMAsyncCompletionInternal.h"

/** @todo: Revise the caching of tasks. We have currently four caches:
 *  Per endpoint task cache
 *  Per class cache
 *  Per endpoint task segment cache
 *  Per class task segment cache
 *
 *  We could use the RT heap for this probably or extend MMR3Heap (uses RTMemAlloc
 *  instead of managing larger blocks) to have this global for the whole VM.
 */

/**
 * A few forward declerations.
 */
typedef struct PDMASYNCCOMPLETIONENDPOINTFILE *PPDMASYNCCOMPLETIONENDPOINTFILE;
/** Pointer to a request segment. */
typedef struct PDMACTASKFILESEG *PPDMACTASKFILESEG;

/**
 * Blocking event types.
 */
typedef enum PDMACEPFILEAIOMGRBLOCKINGEVENT
{
    /** Invalid tye */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_INVALID = 0,
    /** An endpoint is added to the manager. */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_ADD_ENDPOINT,
    /** An endpoint is removed from the manager. */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT,
    /** An endpoint is about to be closed. */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_CLOSE_ENDPOINT,
    /** The manager is requested to terminate */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_SHUTDOWN,
    /** The manager is requested to suspend */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_SUSPEND,
    /** 32bit hack */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_32BIT_HACK = 0x7fffffff
} PDMACEPFILEAIOMGRBLOCKINGEVENT;

/**
 * State of a async I/O manager.
 */
typedef struct PDMACEPFILEMGR
{
    /** Next Aio manager in the list. */
    R3PTRTYPE(struct PDMACEPFILEMGR *)     pNext;
    /** Previous Aio manager in the list. */
    R3PTRTYPE(struct PDMACEPFILEMGR *)     pPrev;
    /** Event semaphore the manager sleeps on when waiting for new requests. */
    RTSEMEVENT                             EventSem;
    /** Flag whether the thread waits in the event semaphore. */
    volatile bool                          fWaitingEventSem;
   /** Flag whether this manager uses the failsafe method. */
    bool                                   fFailsafe;
    /** Flag whether the thread is waiting for I/O to complete. */
    volatile bool                          fWaitingForIo;
    /** Thread data */
    RTTHREAD                               Thread;
    /** Flag whether the I/O manager is requested to terminate */
    volatile bool                          fShutdown;
    /** Flag whether the I/O manager was woken up. */
    volatile bool                          fWokenUp;
    /** List of endpoints assigned to this manager. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONENDPOINTFILE) pEndpointsHead;
    /** Critical section protecting the blocking event handling. */
    RTCRITSECT                             CritSectBlockingEvent;
    /** Event sempahore for blocking external events.
     * The caller waits on it until the async I/O manager
     * finished processing the event. */
    RTSEMEVENT                             EventSemBlock;
    /** Flag whether a blocking event is pending and needs
     * processing by the I/O manager. */
    bool                                   fBlockingEventPending;
    /** Blocking event type */
    PDMACEPFILEAIOMGRBLOCKINGEVENT         enmBlockingEvent;
    /** Event type data */
    union
    {
        /** Add endpoint event. */
        struct
        {
            /** The endpoint to be added */
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
        } AddEndpoint;
        /** Remove endpoint event. */
        struct
        {
            /** The endpoint to be added */
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
        } RemoveEndpoint;
        /** Close endpoint event. */
        struct
        {
            /** The endpoint to be closed */
            PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
        } CloseEndpoint;
    } BlockingEventData;
} PDMACEPFILEMGR;
/** Pointer to a async I/O manager state. */
typedef PDMACEPFILEMGR *PPDMACEPFILEMGR;
/** Pointer to a async I/O manager state pointer. */
typedef PPDMACEPFILEMGR *PPPDMACEPFILEMGR;

/**
 * Global data for the file endpoint class.
 */
typedef struct PDMASYNCCOMPLETIONEPCLASSFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONEPCLASS  Core;
    /** Flag whether we use the failsafe method. */
    bool                       fFailsafe;
    /** Critical section protecting the list of async I/O managers. */
    RTCRITSECT                 CritSect;
    /** Pointer to the head of the async I/O managers. */
    R3PTRTYPE(PPDMACEPFILEMGR) pAioMgrHead;
    /** Number of async I/O managers currently running. */
    unsigned                   cAioMgrs;
    /** Maximum number of segments to cache per endpoint */
    unsigned                   cSegmentsCacheMax;
} PDMASYNCCOMPLETIONEPCLASSFILE;
/** Pointer to the endpoint class data. */
typedef PDMASYNCCOMPLETIONEPCLASSFILE *PPDMASYNCCOMPLETIONEPCLASSFILE;

typedef enum PDMACEPFILEBLOCKINGEVENT
{
    /** The invalid event type */
    PDMACEPFILEBLOCKINGEVENT_INVALID = 0,
    /** A task is about to be canceled */
    PDMACEPFILEBLOCKINGEVENT_CANCEL,
    /** Usual 32bit hack */
    PDMACEPFILEBLOCKINGEVENT_32BIT_HACK = 0x7fffffff
} PDMACEPFILEBLOCKINGEVENT;

/**
 * Data for the file endpoint.
 */
typedef struct PDMASYNCCOMPLETIONENDPOINTFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONENDPOINT             Core;
    /** async I/O manager this endpoint is assigned to. */
    R3PTRTYPE(PPDMACEPFILEMGR)             pAioMgr;
    /** File handle. */
    RTFILE                                 File;
    /** Flag whether caching is enabled for this file. */
    bool                                   fCaching;
    /** List of new tasks. */
    R3PTRTYPE(volatile PPDMASYNCCOMPLETIONTASK) pTasksNewHead;

    /** Head of the small cache for allocated task segments for exclusive
     * use by this endpoint. */
    R3PTRTYPE(volatile PPDMACTASKFILESEG)  pSegmentsFreeHead;
    /** Tail of the small cache for allocated task segments for exclusive
     * use by this endpoint. */
    R3PTRTYPE(volatile PPDMACTASKFILESEG)  pSegmentsFreeTail;
    /** Number of elements in the cache. */
    volatile uint32_t                      cSegmentsCached;

    /** Event sempahore for blocking external events.
     * The caller waits on it until the async I/O manager
     * finished processing the event. */
    RTSEMEVENT                             EventSemBlock;
    /** Flag whether a blocking event is pending and needs
     * processing by the I/O manager. */
    bool                                   fBlockingEventPending;
    /** Blocking event type */
    PDMACEPFILEBLOCKINGEVENT               enmBlockingEvent;
    /** Additional data needed for the event types. */
    union
    {
        /** Cancelation event. */
        struct
        {
            /** The task to cancel. */
            PPDMASYNCCOMPLETIONTASK        pTask;
        } Cancel;
    } BlockingEventData;
    /** Data for exclusive use by the assigned async I/O manager. */
    struct
    {
        /** Pointer to the next endpoint assigned to the manager. */
        R3PTRTYPE(PPDMASYNCCOMPLETIONENDPOINTFILE) pEndpointNext;
        /** Pointer to the previous endpoint assigned to the manager. */
        R3PTRTYPE(PPDMASYNCCOMPLETIONENDPOINTFILE) pEndpointPrev;
    } AioMgr;
} PDMASYNCCOMPLETIONENDPOINTFILE;
/** Pointer to the endpoint class data. */
typedef PDMASYNCCOMPLETIONENDPOINTFILE *PPDMASYNCCOMPLETIONENDPOINTFILE;

/**
 * Segment data of a request.
 */
typedef struct PDMACTASKFILESEG
{
    /** Pointer to the next segment in the list. */
    R3PTRTYPE(struct PDMACTASKFILESEG *) pNext;
    /** Pointer to the previous segment in the list. */
    R3PTRTYPE(struct PDMACTASKFILESEG *) pPrev;
    /** Data for the filesafe and normal manager. */
    union
    {
        /** AIO request */
        RTFILEAIOREQ AioReq;
        /** Data for the failsafe manager. */
        struct
        {
            /** Flag whether this is a re request. False for write */
            bool     fRead;
            /** Offset to start from */
            RTFOFF   off;
            /** Size of the transfer */
            size_t   cbTransfer;
            /** Pointer to the buffer. */
            void    *pvBuf;
        } Failsafe;
    } u;
} PDMACTASKFILESEG;

/**
 * Per task data.
 */
typedef struct PDMASYNCCOMPLETIONTASKFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONTASK Core;
    /** Flag whether this is a flush request. */
    bool                   fFlush;
    /** Type dependent data. */
    union
    {
        /** AIO request for the flush. */
        RTFILEAIOREQ       AioReq;
        /** Data for a data transfer */
        struct
        {
            /** Number of segments which still needs to be processed before the task
             * completes. */
            unsigned           cSegments;
            /** Head of the request segments list for read and write requests. */
            PPDMACTASKFILESEG  pSegmentsHead;
        } DataTransfer;
    } u;
} PDMASYNCCOMPLETIONTASKFILE;
/** Pointer to the endpoint class data. */
typedef PDMASYNCCOMPLETIONTASKFILE *PPDMASYNCCOMPLETIONTASKFILE;

/**
 * Frees a task segment
 *
 * @returns nothing.
 * @param   pEndpoint    Pointer to the endpoint the segment was for.
 * @param   pSeg         The segment to free.
 */
static void pdmacFileSegmentFree(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
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
static PPDMACTASKFILESEG pdmacFileSegmentAlloc(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
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

static PPDMASYNCCOMPLETIONTASK pdmacFileEpGetNewTasks(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
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

static int pdmacFileAioMgrFailsafeProcessEndpoint(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONTASK pTasks = pdmacFileEpGetNewTasks(pEndpoint);

    while (pTasks)
    {
        PPDMASYNCCOMPLETIONTASKFILE pTaskFile = (PPDMASYNCCOMPLETIONTASKFILE)pTasks;

        if (pTasks->pNext)
            AssertMsg(pTasks->uTaskId < pTasks->pNext->uTaskId,
                      ("The task IDs are not ordered Curr=%u Next=%u\n", pTasks->uTaskId, pTasks->pNext->uTaskId));

        if (pTaskFile->fFlush)
        {
            rc = RTFileFlush(pEndpoint->File);
        }
        else
        {
            PPDMACTASKFILESEG pSeg = pTaskFile->u.DataTransfer.pSegmentsHead;

            while(pSeg)
            {
                if (pSeg->u.Failsafe.fRead)
                {
                    rc = RTFileReadAt(pEndpoint->File, pSeg->u.Failsafe.off,
                                        pSeg->u.Failsafe.pvBuf,
                                        pSeg->u.Failsafe.cbTransfer,
                                        NULL);
                }
                else
                {
                    rc = RTFileWriteAt(pEndpoint->File, pSeg->u.Failsafe.off,
                                        pSeg->u.Failsafe.pvBuf,
                                        pSeg->u.Failsafe.cbTransfer,
                                        NULL);
                }

                /* Free the segment. */
                PPDMACTASKFILESEG pCur = pSeg;
                pSeg = pSeg->pNext;

                pdmacFileSegmentFree(pEndpoint, pCur);
            }
        }

        AssertRC(rc);
        pTasks = pTasks->pNext;

        /* Notify task owner */
        pdmR3AsyncCompletionCompleteTask(&pTaskFile->Core);
    }

    return rc;
}

/**
 * A fallback method in case something goes wrong with the normal
 * I/O manager.
 */
static int pdmacFileAioMgrFailsafe(RTTHREAD ThreadSelf, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PPDMACEPFILEMGR pAioMgr = (PPDMACEPFILEMGR)pvUser;

    while (!pAioMgr->fShutdown)
    {
        if (!ASMAtomicReadBool(&pAioMgr->fWokenUp))
        {
            ASMAtomicWriteBool(&pAioMgr->fWaitingEventSem, true);
            rc = RTSemEventWait(pAioMgr->EventSem, RT_INDEFINITE_WAIT);
            ASMAtomicWriteBool(&pAioMgr->fWaitingEventSem, false);
            AssertRC(rc);
        }
        ASMAtomicXchgBool(&pAioMgr->fWokenUp, false);

        /* Process endpoint events first. */
        PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = pAioMgr->pEndpointsHead;
        while (pEndpoint)
        {
            rc = pdmacFileAioMgrFailsafeProcessEndpoint(pEndpoint);
            AssertRC(rc);
            pEndpoint = pEndpoint->AioMgr.pEndpointNext;
        }

        /* Now check for an external blocking event. */
        if (pAioMgr->fBlockingEventPending)
        {
            switch (pAioMgr->enmBlockingEvent)
            {
                case PDMACEPFILEAIOMGRBLOCKINGEVENT_ADD_ENDPOINT:
                {
                    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointNew = pAioMgr->BlockingEventData.AddEndpoint.pEndpoint;
                    AssertMsg(VALID_PTR(pEndpointNew), ("Adding endpoint event without a endpoint to add\n"));

                    pEndpointNew->AioMgr.pEndpointNext = pAioMgr->pEndpointsHead;
                    pEndpointNew->AioMgr.pEndpointPrev = NULL;
                    if (pAioMgr->pEndpointsHead)
                        pAioMgr->pEndpointsHead->AioMgr.pEndpointPrev = pEndpointNew;
                    pAioMgr->pEndpointsHead = pEndpointNew;
                    break;
                }
                case PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT:
                {
                    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointRemove = pAioMgr->BlockingEventData.RemoveEndpoint.pEndpoint;
                    AssertMsg(VALID_PTR(pEndpointRemove), ("Removing endpoint event without a endpoint to remove\n"));

                    PPDMASYNCCOMPLETIONENDPOINTFILE pPrev = pEndpointRemove->AioMgr.pEndpointPrev;
                    PPDMASYNCCOMPLETIONENDPOINTFILE pNext = pEndpointRemove->AioMgr.pEndpointNext;

                    if (pPrev)
                        pPrev->AioMgr.pEndpointNext = pNext;
                    else
                        pAioMgr->pEndpointsHead = pNext;

                    if (pNext)
                        pNext->AioMgr.pEndpointPrev = pPrev;
                    break;
                }
                case PDMACEPFILEAIOMGRBLOCKINGEVENT_CLOSE_ENDPOINT:
                {
                    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpointClose = pAioMgr->BlockingEventData.CloseEndpoint.pEndpoint;
                    AssertMsg(VALID_PTR(pEndpointClose), ("Close endpoint event without a endpoint to Close\n"));

                    /* Make sure all tasks finished. */
                    rc = pdmacFileAioMgrFailsafeProcessEndpoint(pEndpointClose);
                    AssertRC(rc);
                }
                case PDMACEPFILEAIOMGRBLOCKINGEVENT_SHUTDOWN:
                    break;
                case PDMACEPFILEAIOMGRBLOCKINGEVENT_SUSPEND:
                    break;
                default:
                    AssertMsgFailed(("Invalid event type %d\n", pAioMgr->enmBlockingEvent));
            }

            /* Release the waiting thread. */
            rc = RTSemEventSignal(pAioMgr->EventSemBlock);
            AssertRC(rc);
        }
    }

    return rc;
}

static int pdmacFileAioMgrNormal(RTTHREAD ThreadSelf, void *pvUser)
{
    AssertMsgFailed(("Implement\n"));
    return VERR_NOT_IMPLEMENTED;
}

static void pdmacFileAioMgrWakeup(PPDMACEPFILEMGR pAioMgr)
{
    bool fWokenUp = ASMAtomicXchgBool(&pAioMgr->fWokenUp, true);

    if (!fWokenUp)
    {
        int rc = VINF_SUCCESS;
        bool fWaitingEventSem = ASMAtomicReadBool(&pAioMgr->fWaitingEventSem);
        bool fWaitingForIo    = ASMAtomicReadBool(&pAioMgr->fWaitingForIo);

        if (fWaitingEventSem)
            rc = RTSemEventSignal(pAioMgr->EventSem);
        else if (fWaitingForIo)
            rc = RTThreadPoke(pAioMgr->Thread);

        AssertRC(rc);
    }
}

static int pdmacFileAioMgrWaitForBlockingEvent(PPDMACEPFILEMGR pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT enmEvent)
{
    int rc = VINF_SUCCESS;

    pAioMgr->enmBlockingEvent = enmEvent;
    pAioMgr->fBlockingEventPending = true;

    /* Wakeup the async I/O manager */
    pdmacFileAioMgrWakeup(pAioMgr);

    /* Wait for completion. */
    rc = RTSemEventWait(pAioMgr->EventSemBlock, RT_INDEFINITE_WAIT);
    AssertRC(rc);

    pAioMgr->fBlockingEventPending = false;

    return rc;
}

static int pdmacFileAioMgrAddEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc;

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    pAioMgr->BlockingEventData.AddEndpoint.pEndpoint = pEndpoint;
    rc = pdmacFileAioMgrWaitForBlockingEvent(pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT_ADD_ENDPOINT);

    RTCritSectLeave(&pAioMgr->CritSectBlockingEvent);

    return rc;
}

static int pdmacFileAioMgrRemoveEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc;

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    pAioMgr->BlockingEventData.RemoveEndpoint.pEndpoint = pEndpoint;
    rc = pdmacFileAioMgrWaitForBlockingEvent(pAioMgr, PDMACEPFILEAIOMGRBLOCKINGEVENT_REMOVE_ENDPOINT);

    RTCritSectLeave(&pAioMgr->CritSectBlockingEvent);

    return rc;
}

static int pdmacFileAioMgrCloseEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    int rc;

    rc = RTCritSectEnter(&pAioMgr->CritSectBlockingEvent);
    AssertRCReturn(rc, rc);

    pAioMgr->BlockingEventData.CloseEndpoint.pEndpoint = pEndpoint;
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

    pdmacFileAioMgrWakeup(pEndpoint->pAioMgr);

    return VINF_SUCCESS;
}

static int pdmacFileEpTaskInitiate(PPDMASYNCCOMPLETIONTASK pTask,
                                   PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                                   PCPDMDATASEG paSegments, size_t cSegments,
                                   size_t cbTransfer, bool fRead)
{
    int rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;
    PPDMASYNCCOMPLETIONTASKFILE pTaskFile = (PPDMASYNCCOMPLETIONTASKFILE)pTask;
    PPDMACEPFILEMGR pAioMgr = pEpFile->pAioMgr;

    Assert(pAioMgr->fFailsafe); /** @todo change */

    pTaskFile->u.DataTransfer.cSegments     = cSegments;
    pTaskFile->u.DataTransfer.pSegmentsHead = NULL;

    PPDMACTASKFILESEG pSeg = pdmacFileSegmentAlloc(pEpFile);

    pTaskFile->u.DataTransfer.pSegmentsHead = pSeg;

    for (unsigned i = 0; i < cSegments; i++)
    {
        pSeg->u.Failsafe.fRead      = fRead;
        pSeg->u.Failsafe.off        = off;
        pSeg->u.Failsafe.cbTransfer = paSegments[i].cbSeg;
        pSeg->u.Failsafe.pvBuf      = paSegments[i].pvSeg;

        off        += paSegments[i].cbSeg;
        cbTransfer -= paSegments[i].cbSeg;

        if (i < (cSegments-1))
        {
            /* Allocate new segment. */
            PPDMACTASKFILESEG pSegNext = pdmacFileSegmentAlloc(pEpFile);
            AssertPtr(pSeg);
            pSeg->pNext = pSegNext;
            pSeg = pSegNext;
        }
    }

    AssertMsg(!cbTransfer, ("Incomplete task cbTransfer=%u\n", cbTransfer));

    /* Send it off */
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
                    rc = RTThreadCreateF(&pAioMgrNew->Thread,
                                         pAioMgrNew->fFailsafe
                                         ? pdmacFileAioMgrFailsafe
                                         : pdmacFileAioMgrNormal,
                                         pAioMgrNew,
                                         0,
                                         RTTHREADTYPE_IO,
                                         0,
                                         "AioMgr%d-%s", pEpClass->cAioMgrs,
                                         pEpClass->fFailsafe
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
    MMR3HeapFree(pAioMgr);
}

static int pdmacFileInitialize(PPDMASYNCCOMPLETIONEPCLASS pClassGlobals, PCFGMNODE pCfgNode)
{
    int rc = VINF_SUCCESS;
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pClassGlobals;

    /** @todo: Change when the RTFileAio* API is used */
    pEpClassFile->fFailsafe = true;

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

    unsigned fFileFlags = fFlags & PDMACEP_FILE_FLAGS_READ_ONLY
                         ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                         : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE;

    if (!pEpClassFile->fFailsafe)
        fFlags |= RTFILE_O_ASYNC_IO;

    rc = RTFileOpen(&pEpFile->File, pszUri, fFileFlags);
    if (RT_SUCCESS(rc))
    {
        /* Initialize the cache */
        rc = MMR3HeapAllocZEx(pEpClassFile->Core.pVM, MM_TAG_PDM_ASYNC_COMPLETION,
                              sizeof(PDMACTASKFILESEG),
                              (void **)&pEpFile->pSegmentsFreeHead);
        if (RT_SUCCESS(rc))
        {
            /** @todo Check caching flag. */
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
                /* Check for an idling one or create new if not found */
                AssertMsgFailed(("Implement\n"));
            }

            /* Assign the endpoint to the thread. */
            pEpFile->pAioMgr = pAioMgr;
            rc = pdmacFileAioMgrAddEndpoint(pAioMgr, pEpFile);
        }

        if (RT_FAILURE(rc))
            RTFileClose(pEpFile->File);
    }

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
    return pdmacFileEpTaskInitiate(pTask, pEndpoint, off, paSegments, cSegments, cbRead, true);
}

static int pdmacFileEpWrite(PPDMASYNCCOMPLETIONTASK pTask,
                            PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                            PCPDMDATASEG paSegments, size_t cSegments,
                            size_t cbWrite)
{
    return pdmacFileEpTaskInitiate(pTask, pEndpoint, off, paSegments, cSegments, cbWrite, false);
}

static int pdmacFileEpFlush(PPDMASYNCCOMPLETIONTASK pTask,
                            PPDMASYNCCOMPLETIONENDPOINT pEndpoint)
{
    PPDMASYNCCOMPLETIONENDPOINTFILE pEpFile = (PPDMASYNCCOMPLETIONENDPOINTFILE)pEndpoint;
    PPDMASYNCCOMPLETIONTASKFILE pTaskFile = (PPDMASYNCCOMPLETIONTASKFILE)pTask;

    pTaskFile->fFlush = true;
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

