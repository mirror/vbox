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

#include <VBox/cfgm.h>
#include <VBox/stam.h>
#include <iprt/types.h>
#include <iprt/file.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <iprt/avl.h>

#include "PDMAsyncCompletionInternal.h"

/** Enable the 2Q cache alogrithm. */
#define VBOX_WITH_2Q_CACHE 1

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
typedef struct PDMACTASKFILE *PPDMACTASKFILE;
/** Pointer to the endpoint class data. */
typedef struct PDMASYNCCOMPLETIONTASKFILE *PPDMASYNCCOMPLETIONTASKFILE;
/** Pointer to a cache LRU list. */
typedef struct PDMACFILELRULIST *PPDMACFILELRULIST;
/** Pointer to the global cache structure. */
typedef struct PDMACFILECACHEGLOBAL *PPDMACFILECACHEGLOBAL;

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
    /** The manager is requested to resume */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_RESUME,
    /** 32bit hack */
    PDMACEPFILEAIOMGRBLOCKINGEVENT_32BIT_HACK = 0x7fffffff
} PDMACEPFILEAIOMGRBLOCKINGEVENT;

/**
 * States of the I/O manager.
 */
typedef enum PDMACEPFILEMGRSTATE
{
    /** Invalid state. */
    PDMACEPFILEMGRSTATE_INVALID = 0,
    /** Normal running state accepting new requests
     * and processing them.
     */
    PDMACEPFILEMGRSTATE_RUNNING,
    /** Fault state - not accepting new tasks for endpoints but waiting for
     * remaining ones to finish.
     */
    PDMACEPFILEMGRSTATE_FAULT,
    /** Suspending state - not accepting new tasks for endpoints but waiting
     * for remaining ones to finish.
     */
    PDMACEPFILEMGRSTATE_SUSPENDING,
    /** Shutdown state - not accepting new tasks for endpoints but waiting
     * for remaining ones to finish.
     */
    PDMACEPFILEMGRSTATE_SHUTDOWN,
    /** 32bit hack */
    PDMACEPFILEMGRSTATE_32BIT_HACK = 0x7fffffff
} PDMACEPFILEMGRSTATE;

/**
 * State of a async I/O manager.
 */
typedef struct PDMACEPFILEMGR
{
    /** Next Aio manager in the list. */
    R3PTRTYPE(struct PDMACEPFILEMGR *)     pNext;
    /** Previous Aio manager in the list. */
    R3PTRTYPE(struct PDMACEPFILEMGR *)     pPrev;
    /** Current state of the manager. */
    PDMACEPFILEMGRSTATE                    enmState;
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
    /** Flag whether the I/O manager was woken up. */
    volatile bool                          fWokenUp;
    /** List of endpoints assigned to this manager. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONENDPOINTFILE) pEndpointsHead;
    /** Number of endpoints assigned to the manager. */
    unsigned                               cEndpoints;
    /** Number of requests active currently. */
    unsigned                               cRequestsActive;
    /** Pointer to an array of free async I/O request handles. */
    RTFILEAIOREQ                          *pahReqsFree;
    /** Next free position for a free request handle. */
    unsigned                               iFreeEntryNext;
    /** Position of the next free task handle */
    unsigned                               iFreeReqNext;
    /** Size of the array. */
    unsigned                               cReqEntries;
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
 * Data for one request segment waiting for cache entry.
 */
typedef struct PDMACFILETASKSEG
{
    /** Next task segment in the list. */
    struct PDMACFILETASKSEG    *pNext;
    /** Task this segment is for. */
    PPDMASYNCCOMPLETIONTASKFILE pTask;
    /** Offset into the cache entry buffer to start reading from. */
    uint32_t                    uBufOffset;
    /** Number of bytes to transfer. */
    size_t                      cbTransfer;
    /** Pointer to the buffer. */
    void                       *pvBuf;
    /** Flag whether this entry writes data to the cache. */
    bool                        fWrite;
} PDMACFILETASKSEG, *PPDMACFILETASKSEG;

/**
 * A cache entry
 */
typedef struct PDMACFILECACHEENTRY
{
    /** The AVL entry data. */
    AVLRFOFFNODECORE                Core;
    /** Pointer to the previous element. Used in one of the LRU lists.*/
    struct PDMACFILECACHEENTRY     *pPrev;
    /** Pointer to the next element. Used in one of the LRU lists.*/
    struct PDMACFILECACHEENTRY     *pNext;
    /** Pointer to the list the entry is in. */
    PPDMACFILELRULIST               pList;
    /** Pointer to the global cache structure. */
    PPDMACFILECACHEGLOBAL           pCache;
    /** Endpoint the entry belongs to. */
    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint;
    /** Flags for this entry. Combinations of PDMACFILECACHE_* #defines */
    uint32_t                        fFlags;
    /** Reference counter. Prevents eviction of the entry if > 0. */
    volatile uint32_t               cRefs;
    /** Size of the entry. */
    size_t                          cbData;
    /** Pointer to the memory containing the data. */
    uint8_t                        *pbData;
    /** Pointer to the buffer replacing the current one
     * if the deprecated flag is set. */
    uint8_t                        *pbDataReplace;
    /** Head of list of tasks waiting for this one to finish. */
    PPDMACFILETASKSEG               pWaitingHead;
    /** Tail of list of tasks waiting for this one to finish. */
    PPDMACFILETASKSEG               pWaitingTail;
} PDMACFILECACHEENTRY, *PPDMACFILECACHEENTRY;
/** I/O is still in progress for this entry. This entry is not evictable. */
#define PDMACFILECACHE_ENTRY_IO_IN_PROGRESS RT_BIT(0)
/** Entry is locked and thus not evictable. */
#define PDMACFILECACHE_ENTRY_LOCKED         RT_BIT(1)
/** Entry is dirty */
#define PDMACFILECACHE_ENTRY_IS_DIRTY       RT_BIT(2)
/** The current buffer used for the entry is deprecated.
 * The new one is available and will be replaced as soon as the file update
 * completed.
 */
#define PDMACFILECACHE_ENTRY_IS_DEPRECATED  RT_BIT(3)
/** Entry is not evictable. */
#define PDMACFILECACHE_NOT_EVICTABLE  (PDMACFILECACHE_ENTRY_LOCKED | PDMACFILECACHE_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DEPRECATED)

/**
 * LRU list data
 */
typedef struct PDMACFILELRULIST
{
    /** Head of the list. */
    PPDMACFILECACHEENTRY pHead;
    /** Tail of the list. */
    PPDMACFILECACHEENTRY pTail;
    /** Number of bytes cached in the list. */
    uint32_t             cbCached;
} PDMACFILELRULIST;

/**
 * Global cache data.
 */
typedef struct PDMACFILECACHEGLOBAL
{
    /** Maximum size of the cache in bytes. */
    uint32_t         cbMax;
    /** Current size of the cache in bytes. */
    uint32_t         cbCached;
    /** Critical section protecting the cache. */
    RTCRITSECT       CritSect;
#ifdef VBOX_WITH_2Q_CACHE
    uint32_t         cbRecentlyUsedInMax;
    uint32_t         cbRecentlyUsedOutMax;
    PDMACFILELRULIST LruRecentlyUsedIn;
    PDMACFILELRULIST LruRecentlyUsedOut;
    PDMACFILELRULIST LruFrequentlyUsed;
#else
    /** Adaption parameter (p) */
    uint32_t         uAdaptVal;
    /** LRU list for recently used entries (T1) */
    PDMACFILELRULIST LruRecentlyUsed;
    /** LRU list for frequently used entries (T2) */
    PDMACFILELRULIST LruFrequentlyUsed;
    /** LRU list for recently evicted entries (B1) */
    PDMACFILELRULIST LruRecentlyGhost;
    /** LRU list for evicted entries from T2 (B2) */
    PDMACFILELRULIST LruFrequentlyGhost;
#endif
#ifdef VBOX_WITH_STATISTICS
    /** Hit counter. */
    STAMCOUNTER      cHits;
    /** Partial hit counter. */
    STAMCOUNTER      cPartialHits;
    /** Miss counter. */
    STAMCOUNTER      cMisses;
    /** Bytes read from cache. */
    STAMCOUNTER      StatRead;
    /** Bytes written to the cache. */
    STAMCOUNTER      StatWritten;
    /** Time spend to get an entry in the AVL tree. */
    STAMPROFILEADV   StatTreeGet;
    /** Time spend to insert an entry in the AVL tree. */
    STAMPROFILEADV   StatTreeInsert;
    /** Time spend to remove an entry in the AVL tree. */
    STAMPROFILEADV   StatTreeRemove;
    /** Number of times a buffer could be reused. */
    STAMCOUNTER      StatBuffersReused;
#endif
} PDMACFILECACHEGLOBAL;

/**
 * Per endpoint cache data.
 */
typedef struct PDMACFILEENDPOINTCACHE
{
    /** AVL tree managing cache entries. */
    PAVLRFOFFTREE         pTree;
    /** R/W semaphore protecting cached entries for this endpoint. */
    RTSEMRW               SemRWEntries;
    /** Pointer to the gobal cache data */
    PPDMACFILECACHEGLOBAL pCache;

#ifdef VBOX_WITH_STATISTICS
    /** Number of times a write was deferred because the cache entry was still in progress */
    STAMCOUNTER           StatWriteDeferred;
#endif
} PDMACFILEENDPOINTCACHE, *PPDMACFILEENDPOINTCACHE;

/**
 * Global data for the file endpoint class.
 */
typedef struct PDMASYNCCOMPLETIONEPCLASSFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONEPCLASS           Core;
    /** Flag whether we use the failsafe method. */
    bool                                fFailsafe;
    /** Flag whether the file data cache is enabled. */
    bool                                fCacheEnabled;
    /** Flag whether the host cache should be used too. */
    bool                                fHostCacheEnabled;
    /** Critical section protecting the list of async I/O managers. */
    RTCRITSECT                          CritSect;
    /** Pointer to the head of the async I/O managers. */
    R3PTRTYPE(PPDMACEPFILEMGR)          pAioMgrHead;
    /** Number of async I/O managers currently running. */
    unsigned                            cAioMgrs;
    /** Maximum number of segments to cache per endpoint */
    unsigned                            cTasksCacheMax;
    /** Maximum number of simultaneous outstandingrequests. */
    uint32_t                            cReqsOutstandingMax;
    /** Bitmask for checking the alignment of a buffer. */
    RTR3UINTPTR                         uBitmaskAlignment;
    /** Global cache data. */
    PDMACFILECACHEGLOBAL                Cache;
    /** Flag whether the out of resources warning was printed already. */
    bool                                fOutOfResourcesWarningPrinted;
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
 * States of the endpoint.
 */
typedef enum PDMASYNCCOMPLETIONENDPOINTFILESTATE
{
    /** Invalid state. */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE_INVALID = 0,
    /** Normal running state accepting new requests
     * and processing them.
     */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE_ACTIVE,
    /** The endpoint is about to be closed - not accepting new tasks for endpoints but waiting for
     *  remaining ones to finish.
     */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE_CLOSING,
    /** Removing from current I/O manager state - not processing new tasks for endpoints but waiting
     * for remaining ones to finish.
     */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE_REMOVING,
    /** The current endpoint will be migrated to another I/O manager. */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE_MIGRATING,
    /** 32bit hack */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE_32BIT_HACK = 0x7fffffff
} PDMASYNCCOMPLETIONENDPOINTFILESTATE;

/**
 * Data for the file endpoint.
 */
typedef struct PDMASYNCCOMPLETIONENDPOINTFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONENDPOINT             Core;
    /** Current state of the endpoint. */
    PDMASYNCCOMPLETIONENDPOINTFILESTATE    enmState;
    /** async I/O manager this endpoint is assigned to. */
    R3PTRTYPE(volatile PPDMACEPFILEMGR)    pAioMgr;
    /** Flags for opening the file. */
    unsigned                               fFlags;
    /** File handle. */
    RTFILE                                 File;
    /** Size of the underlying file.
     * Updated while data is appended. */
    volatile uint64_t                      cbFile;
    /** Flag whether caching is enabled for this file. */
    bool                                   fCaching;
    /** Flag whether the file was opened readonly. */
    bool                                   fReadonly;
    /** List of new tasks. */
    R3PTRTYPE(volatile PPDMACTASKFILE)     pTasksNewHead;

    /** Head of the small cache for allocated task segments for exclusive
     * use by this endpoint. */
    R3PTRTYPE(volatile PPDMACTASKFILE)     pTasksFreeHead;
    /** Tail of the small cache for allocated task segments for exclusive
     * use by this endpoint. */
    R3PTRTYPE(volatile PPDMACTASKFILE)     pTasksFreeTail;
    /** Number of elements in the cache. */
    volatile uint32_t                      cTasksCached;

    /** Cache of endpoint data. */
    PDMACFILEENDPOINTCACHE                 DataCache;

    /** Flag whether a flush request is currently active */
    PPDMACTASKFILE                         pFlushReq;

    /** Event sempahore for blocking external events.
     * The caller waits on it until the async I/O manager
     * finished processing the event. */
    RTSEMEVENT                             EventSemBlock;
    /** Flag whether a blocking event is pending and needs
     * processing by the I/O manager. */
    bool                                   fBlockingEventPending;
    /** Blocking event type */
    PDMACEPFILEBLOCKINGEVENT               enmBlockingEvent;

#ifdef VBOX_WITH_STATISTICS
    /** Time spend in a read. */
    STAMPROFILEADV                         StatRead;
    /** Time spend in a write. */
    STAMPROFILEADV                         StatWrite;
#endif

    /** Additional data needed for the event types. */
    union
    {
        /** Cancelation event. */
        struct
        {
            /** The task to cancel. */
            PPDMACTASKFILE                 pTask;
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
        R3PTRTYPE(PPDMACTASKFILE)                  pReqsPendingHead;
        /** Tail of pending requests. */
        R3PTRTYPE(PPDMACTASKFILE)                  pReqsPendingTail;
        /** Number of requests currently being processed for this endpoint
         * (excluded flush requests). */
        unsigned                                   cRequestsActive;
        /** Number of requests processed during the last second. */
        unsigned                                   cReqsPerSec;
        /** Current number of processed requests for the current update period. */
        unsigned                                   cReqsProcessed;
        /** Flag whether the endpoint is about to be moved to another manager. */
        bool                                       fMoving;
        /** Destination I/O manager. */
        PPDMACEPFILEMGR                            pAioMgrDst;
    } AioMgr;
} PDMASYNCCOMPLETIONENDPOINTFILE;
/** Pointer to the endpoint class data. */
typedef PDMASYNCCOMPLETIONENDPOINTFILE *PPDMASYNCCOMPLETIONENDPOINTFILE;

/** Request completion function */
typedef DECLCALLBACK(void)   FNPDMACTASKCOMPLETED(PPDMACTASKFILE pTask, void *pvUser);
/** Pointer to a request completion function. */
typedef FNPDMACTASKCOMPLETED *PFNPDMACTASKCOMPLETED;

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
 * Data of a request.
 */
typedef struct PDMACTASKFILE
{
    /** next task in the list. */
    struct PDMACTASKFILE                *pNext;
    /** Endpoint */
    PPDMASYNCCOMPLETIONENDPOINTFILE      pEndpoint;
    /** Transfer type. */
    PDMACTASKFILETRANSFER                enmTransferType;
    /** Start offset */
    RTFOFF                               Off;
    /** Data segment. */
    PDMDATASEG                           DataSeg;
    /** Flag whether this segment uses a bounce buffer
     * because the provided buffer doesn't meet host requirements. */
    bool                                 fBounceBuffer;
    /** Pointer to the used bounce buffer if any. */
    void                                *pvBounceBuffer;
    /** Start offset in the bounce buffer to copy from. */
    uint32_t                             uBounceBufOffset;
    /** Flag whether this is a prefetch request. */
    bool                                 fPrefetch;
    /** Completion function to call on completion. */
    PFNPDMACTASKCOMPLETED                pfnCompleted;
    /** User data */
    void                                *pvUser;
} PDMACTASKFILE;

/**
 * Per task data.
 */
typedef struct PDMASYNCCOMPLETIONTASKFILE
{
    /** Common data. */
    PDMASYNCCOMPLETIONTASK Core;
    /** Number of bytes to transfer until this task completes. */
    volatile int32_t      cbTransferLeft;
    /** Flag whether the task completed. */
    volatile bool         fCompleted;
} PDMASYNCCOMPLETIONTASKFILE;

int pdmacFileAioMgrFailsafe(RTTHREAD ThreadSelf, void *pvUser);
int pdmacFileAioMgrNormal(RTTHREAD ThreadSelf, void *pvUser);

int pdmacFileAioMgrNormalInit(PPDMACEPFILEMGR pAioMgr);
void pdmacFileAioMgrNormalDestroy(PPDMACEPFILEMGR pAioMgr);

int pdmacFileAioMgrCreate(PPDMASYNCCOMPLETIONEPCLASSFILE pEpClass, PPPDMACEPFILEMGR ppAioMgr, bool fFailsafe);

int pdmacFileAioMgrAddEndpoint(PPDMACEPFILEMGR pAioMgr, PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint);

PPDMACTASKFILE pdmacFileEpGetNewTasks(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint);
PPDMACTASKFILE pdmacFileTaskAlloc(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint);
void pdmacFileTaskFree(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                       PPDMACTASKFILE pTask);

int pdmacFileEpAddTask(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMACTASKFILE pTask);

void pdmacFileEpTaskCompleted(PPDMACTASKFILE pTask, void *pvUser);

int pdmacFileCacheInit(PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile, PCFGMNODE pCfgNode);
void pdmacFileCacheDestroy(PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile);
int pdmacFileEpCacheInit(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile);
void pdmacFileEpCacheDestroy(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint);

int pdmacFileEpCacheRead(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask,
                         RTFOFF off, PCPDMDATASEG paSegments, size_t cSegments,
                         size_t cbRead);
int pdmacFileEpCacheWrite(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask,
                          RTFOFF off, PCPDMDATASEG paSegments, size_t cSegments,
                          size_t cbWrite);

RT_C_DECLS_END

#endif

