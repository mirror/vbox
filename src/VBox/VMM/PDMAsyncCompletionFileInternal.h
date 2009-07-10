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

#ifndef ___PDMAsyncCompletionFileInternal_h
#define ___PDMAsyncCompletionFileInternal_h

#include <iprt/types.h>
#include <iprt/file.h>
#include <iprt/thread.h>
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

RT_C_DECLS_BEGIN

/**
 * A few forward declerations.
 */
typedef struct PDMASYNCCOMPLETIONENDPOINTFILE *PPDMASYNCCOMPLETIONENDPOINTFILE;
/** Pointer to a request segment. */
typedef struct PDMACTASKFILESEG *PPDMACTASKFILESEG;
/** Pointer to the endpoint class data. */
typedef struct PDMASYNCCOMPLETIONTASKFILE *PPDMASYNCCOMPLETIONTASKFILE;

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
    /** Thread data */
    RTTHREAD                               Thread;
    /** The async I/O context for this manager. */
    RTFILEAIOCTX                           hAioCtx;
    /** Flag whether the I/O manager is requested to terminate */
    volatile bool                          fShutdown;
    /** Flag whether the I/O manager was woken up. */
    volatile bool                          fWokenUp;
    /** List of endpoints assigned to this manager. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONENDPOINTFILE) pEndpointsHead;
    /** Number of requests active currently. */
    unsigned                               cRequestsActive;
    /** Critical section protecting the blocking event handling. */
    RTCRITSECT                             CritSectBlockingEvent;
    /** Event sempahore for blocking external events.
     * The caller waits on it until the async I/O manager
     * finished processing the event. */
    RTSEMEVENT                             EventSemBlock;
    /** Flag whether a blocking event is pending and needs
     * processing by the I/O manager. */
    volatile bool                          fBlockingEventPending;
    /** Blocking event type */
    volatile PDMACEPFILEAIOMGRBLOCKINGEVENT enmBlockingEvent;
    /** Event type data */
    union
    {
        /** Add endpoint event. */
        struct
        {
            /** The endpoint to be added */
            volatile PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
        } AddEndpoint;
        /** Remove endpoint event. */
        struct
        {
            /** The endpoint to be removed */
            volatile PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
        } RemoveEndpoint;
        /** Close endpoint event. */
        struct
        {
            /** The endpoint to be closed */
            volatile PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
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
    PDMASYNCCOMPLETIONEPCLASS           Core;
    /** Flag whether we use the failsafe method. */
    bool                                fFailsafe;
    /** Critical section protecting the list of async I/O managers. */
    RTCRITSECT                          CritSect;
    /** Pointer to the head of the async I/O managers. */
    R3PTRTYPE(PPDMACEPFILEMGR)          pAioMgrHead;
    /** Number of async I/O managers currently running. */
    unsigned                            cAioMgrs;
    /** Maximum number of segments to cache per endpoint */
    unsigned                            cSegmentsCacheMax;
    /** Maximum number of simultaneous outstandingrequests. */
    uint32_t                            cReqsOutstandingMax;
    /** Bitmask for checking the alignment of a buffer. */
    RTR3UINTPTR                         uBitmaskAlignment;
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
    R3PTRTYPE(volatile PPDMACEPFILEMGR)    pAioMgr;
    /** Filename */
    char                                  *pszFilename;
    /** Flags for opening the file. */
    unsigned                               fFlags;
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

    /** Flag whether a flush request is currently active */
    R3PTRTYPE(PPDMASYNCCOMPLETIONTASKFILE) pFlushReq;

    /** Flag whether the endpoint is currently closed or removed from
     * the active endpoint. */
    bool                                   fRemovedOrClosed;

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
        /** List of pending requests (not submitted due to usage restrictions
         *  or a pending flush request) */
        R3PTRTYPE(PPDMASYNCCOMPLETIONTASK)         pReqsPendingHead;
        /** Tail of pending requests. */
        R3PTRTYPE(PPDMASYNCCOMPLETIONTASK)         pReqsPendingTail;
        /** Number of requests currently being processed for this endpoint
         * (excluded flush requests). */
        unsigned                                   cRequestsActive;
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
    /** Pointer to the task owning the segment. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONTASKFILE) pTask;
    /** Data segment. */
    PDMDATASEG                           DataSeg;
    /** Flag whether this segment uses a bounce buffer
     * because the provided buffer doesn't meet host requirements. */
    bool                                 fBounceBuffer;
    /** Pointer to the used bounce buffer if any. */
    void                                *pvBounceBuffer;
    /** AIO request */
    RTFILEAIOREQ                         hAioReq;
} PDMACTASKFILESEG;

/**
 * Transfer type.
 */
typedef enum PDMACTASKFILETRANSFER
{
    /** Invalid. */
    PDMACTASKFILETRANSFER_INVALID = 0,
    /** Read transfer. */
    PDMACTASKFILETRANSFER_READ,
    /** Write transfer. */
    PDMACTASKFILETRANSFER_WRITE,
    /** Flush transfer. */
    PDMACTASKFILETRANSFER_FLUSH
} PDMACTASKFILETRANSFER;

/**
 * Per task data.
 */
typedef struct PDMASYNCCOMPLETIONTASKFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONTASK Core;
    /** Transfer type. */
    PDMACTASKFILETRANSFER  enmTransferType;
    /** Type dependent data. */
    union
    {
        /** Data for a data transfer */
        struct
        {
            /** Start offset. */
            RTFOFF             off;
            /** Number of bytes to transfer. */
            size_t             cbTransfer;
            /** Number of segments which still needs to be processed before the task
             * completes. */
            unsigned           cSegments;
            /** Head of the request segments list for read and write requests. */
            PPDMACTASKFILESEG  pSegmentsHead;
        } DataTransfer;
    } u;
} PDMASYNCCOMPLETIONTASKFILE;

int pdmacFileAioMgrFailsafe(RTTHREAD ThreadSelf, void *pvUser);
int pdmacFileAioMgrNormal(RTTHREAD ThreadSelf, void *pvUser);

int pdmacFileAioMgrNormalInit(PPDMACEPFILEMGR pAioMgr);
void pdmacFileAioMgrNormalDestroy(PPDMACEPFILEMGR pAioMgr);

PPDMASYNCCOMPLETIONTASK pdmacFileEpGetNewTasks(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint);
PPDMACTASKFILESEG pdmacFileSegmentAlloc(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint);
void pdmacFileSegmentFree(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                          PPDMACTASKFILESEG pSeg);

RT_C_DECLS_END

#endif

