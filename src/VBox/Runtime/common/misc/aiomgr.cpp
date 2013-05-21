/* $Id$ */
/** @file
 * IPRT - Async I/O manager.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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

#include <iprt/aiomgr.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/file.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/semaphore.h>
#include <iprt/queueatomic.h>

#include "internal/magics.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** Pointer to an internal async I/O file instance. */
typedef struct RTAIOMGRFILEINT *PRTAIOMGRFILEINT;

/**
 * Blocking event types.
 */
typedef enum RTAIOMGREVENT
{
    /** Invalid tye */
    RTAIOMGREVENT_INVALID = 0,
    /** An endpoint is added to the manager. */
    RTAIOMGREVENT_ADD_FILE,
    /** An endpoint is about to be closed. */
    RTAIOMGREVENT_CLOSE_FILE,
    /** 32bit hack */
    RTAIOMGREVENT_32BIT_HACK = 0x7fffffff
} RTAIOMGREVENT;

/**
 * Async I/O manager instance data.
 */
typedef struct RTAIOMGRINT
{
    /** Magic value. */
    uint32_t                      u32Magic;
    /** Reference count. */
    volatile uint32_t             cRefs;
    /** Running flag. */
    volatile bool                 fRunning;
    /** Async I/O context handle. */
    RTFILEAIOCTX                  hAioCtx;
    /** async I/O thread. */
    RTTHREAD                      hThread;
    /** List of files assigned to this manager. */
    PRTAIOMGRFILEINT              pFilesHead;
    /** Number of requests active currently. */
    unsigned                      cReqsActive;
    /** Number of maximum requests active. */
    uint32_t                      cReqsActiveMax;
    /** Pointer to an array of free async I/O request handles. */
    RTFILEAIOREQ                 *pahReqsFree;
    /** Index of the next free entry in the cache. */
    uint32_t                      iFreeEntry;
    /** Size of the array. */
    unsigned                      cReqEntries;
    /** Critical section protecting the blocking event handling. */
    RTCRITSECT                    CritSectBlockingEvent;
    /** Event semaphore for blocking external events.
     * The caller waits on it until the async I/O manager
     * finished processing the event. */
    RTSEMEVENT                    EventSemBlock;
    /** Flag whether a blocking event is pending and needs
     * processing by the I/O manager. */
    volatile bool                 fBlockingEventPending;
    /** Blocking event type */
    volatile RTAIOMGREVENT        enmBlockingEvent;
    /** Event type data */
    union
    {
        /** The file to be added */
        volatile PRTAIOMGRFILEINT pFileAdd;
        /** The file to be closed */
        volatile PRTAIOMGRFILEINT pFileClose;
    } BlockingEventData;
} RTAIOMGRINT;
/** Pointer to an internal async I/O manager instance. */
typedef RTAIOMGRINT *PRTAIOMGRINT;

/**
 * Async I/O manager file instance data.
 */
typedef struct RTAIOMGRFILEINT
{
    /** Magic value. */
    uint32_t                      u32Magic;
    /** Reference count. */
    volatile uint32_t             cRefs;
    /** File handle. */
    RTFILE                        hFile;
    /** async I/O manager this file belongs to. */
    PRTAIOMGRINT                  pAioMgr;
    /** Work queue for new requests. */
    RTQUEUEATOMIC                 QueueReqs;
    /** Data for exclusive use by the assigned async I/O manager. */
    struct
    {
        /** Pointer to the next file assigned to the manager. */
        PRTAIOMGRFILEINT          pNext;
#if 0
        /** List of pending requests (not submitted due to usage restrictions
         *  or a pending flush request) */
        R3PTRTYPE(PPDMACTASKFILE) pReqsPendingHead;
        /** Tail of pending requests. */
        R3PTRTYPE(PPDMACTASKFILE) pReqsPendingTail;
        /** Tree of currently locked ranges.
         * If a write task is enqueued the range gets locked and any other
         * task writing to that range has to wait until the task completes.
         */
        PAVLRFOFFTREE             pTreeRangesLocked;
        /** Number of requests with a range lock active. */
        unsigned                  cLockedReqsActive;
        /** Number of requests currently being processed for this endpoint
         * (excluded flush requests). */
        unsigned                  cRequestsActive;
#endif
    } AioMgr;
} RTAIOMGRFILEINT;

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTAIOMGR_VALID_RETURN_RC(a_hAioMgr, a_rc) \
    do { \
        AssertPtrReturn((a_hAioMgr), (a_rc)); \
        AssertReturn((a_hAioMgr)->u32Magic == RTAIOMGR_MAGIC, (a_rc)); \
    } while (0)

/** Validates a handle and returns VERR_INVALID_HANDLE if not valid. */
#define RTAIOMGR_VALID_RETURN(a_hAioMgr) RTAIOMGR_VALID_RETURN_RC((hAioMgr), VERR_INVALID_HANDLE)

/** Validates a handle and returns (void) if not valid. */
#define RTAIOMGR_VALID_RETURN_VOID(a_hAioMgr) \
    do { \
        AssertPtrReturnVoid(a_hAioMgr); \
        AssertReturnVoid((a_hAioMgr)->u32Magic == RTAIOMGR_MAGIC); \
    } while (0)

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * async I/O manager worker loop.
 *
 * @returns IPRT status code.
 * @param   hThreadSelf    The thread handle this worker belongs to.
 * @param   pvUser         Opaque user data (Pointer to async I/O manager instance).
 */
static DECLCALLBACK(int) rtAioMgrWorker(RTTHREAD hThreadSelf, void *pvUser)
{
    PRTAIOMGRINT pThis = (PRTAIOMGRINT)pvUser;
    bool fRunning = true;

    do
    {
        uint32_t cReqsCompleted = 0;
        RTFILEAIOREQ ahReqsCompleted[32];
        int rc = RTFileAioCtxWait(pThis->hAioCtx, 1, RT_INDEFINITE_WAIT, &ahReqsCompleted[0],
                                  RT_ELEMENTS(ahReqsCompleted), &cReqsCompleted);
        if (RT_FAILURE(rc) && rc != VERR_INTERRUPTED)
        {
        }
        fRunning = ASMAtomicUoReadBool(&pThis->fRunning);
    } while (fRunning);

    return VINF_SUCCESS;
}

/**
 * Destroys an async I/O manager.
 *
 * @returns nothing.
 * @param   pThis             The async I/O manager instance to destroy.
 */
static void rtAioMgrDestroy(PRTAIOMGRINT pThis)
{
    int rc;

    ASMAtomicXchgBool(&pThis->fRunning, false);
    rc = RTFileAioCtxWakeup(pThis->hAioCtx);
    AssertRC(rc);

    rc = RTThreadWait(pThis->hThread, RT_INDEFINITE_WAIT, NULL);
    AssertRC(rc);

    rc = RTFileAioCtxDestroy(pThis->hAioCtx);
    AssertRC(rc);

    pThis->hThread = NIL_RTTHREAD;
    pThis->hAioCtx  = NIL_RTFILEAIOCTX;
    pThis->u32Magic = ~RTAIOMGR_MAGIC;
    RTMemFree(pThis);
}

/**
 * Destroys an async I/O manager file.
 *
 * @returns nothing.
 * @param   pThis             The async I/O manager file.
 */
static void rtAioMgrFileDestroy(PRTAIOMGRFILEINT pThis)
{
    pThis->u32Magic = ~RTAIOMGRFILE_MAGIC;
    RTMemFree(pThis);
}

RTDECL(int) RTAioMgrCreate(PRTAIOMGR phAioMgr, uint32_t cReqsMax)
{
    int rc = VINF_SUCCESS;
    PRTAIOMGRINT pThis;

    AssertPtrReturn(phAioMgr, VERR_INVALID_POINTER);
    AssertReturn(cReqsMax > 0, VERR_INVALID_PARAMETER);

    pThis = (PRTAIOMGRINT)RTMemAllocZ(sizeof(RTAIOMGRINT));
    if (pThis)
    {
        pThis->u32Magic = RTAIOMGR_MAGIC;
        pThis->cRefs    = 1;
        pThis->fRunning = true;
        rc = RTFileAioCtxCreate(&pThis->hAioCtx, cReqsMax == UINT32_MAX
                                                 ? RTFILEAIO_UNLIMITED_REQS
                                                 : cReqsMax,
                                RTFILEAIOCTX_FLAGS_WAIT_WITHOUT_PENDING_REQUESTS);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadCreateF(&pThis->hThread, rtAioMgrWorker, pThis, 0, RTTHREADTYPE_IO,
                                 RTTHREADFLAGS_WAITABLE, "AioMgr-%p", pThis);
            if (RT_FAILURE(rc))
            {
                rc = RTFileAioCtxDestroy(pThis->hAioCtx);
                AssertRC(rc);
            }
        }
        if (RT_FAILURE(rc))
            RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
        *phAioMgr = pThis;

    return rc;
}

RTDECL(uint32_t) RTAioMgrRetain(RTAIOMGR hAioMgr)
{
    PRTAIOMGRINT pThis = hAioMgr;
    AssertReturn(hAioMgr != NIL_RTAIOMGR, UINT32_MAX);
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTAIOMGR_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1G, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

RTDECL(uint32_t) RTAioMgrRelease(RTAIOMGR hAioMgr)
{
    PRTAIOMGRINT pThis = hAioMgr;
    if (pThis == NIL_RTAIOMGR)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTAIOMGR_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1G, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtAioMgrDestroy(pThis);
    return cRefs;
}

RTDECL(int) RTAioMgrFileCreate(RTAIOMGR hAioMgr, RTFILE hFile, PFNRTAIOMGRREQCOMPLETE pfnReqComplete,
                               PRTAIOMGRFILE phAioMgrFile)
{
    int rc = VINF_SUCCESS;
    PRTAIOMGRFILEINT pThis;

    AssertReturn(hAioMgr != NIL_RTAIOMGR, VERR_INVALID_HANDLE);
    AssertReturn(hFile != NIL_RTFILE, VERR_INVALID_HANDLE);
    AssertPtrReturn(pfnReqComplete, VERR_INVALID_POINTER);
    AssertPtrReturn(phAioMgrFile, VERR_INVALID_POINTER);

    pThis = (PRTAIOMGRFILEINT)RTMemAllocZ(sizeof(RTAIOMGRFILEINT));
    if (pThis)
    {
        pThis->u32Magic = RTAIOMGRFILE_MAGIC;
        pThis->cRefs    = 1;
        pThis->hFile    = hFile;
        pThis->pAioMgr  = hAioMgr;
        rc = RTFileAioCtxAssociateWithFile(pThis->pAioMgr->hAioCtx, hFile);
        if (RT_FAILURE(rc))
            rtAioMgrFileDestroy(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
        *phAioMgrFile = pThis;

    return rc;
}

RTDECL(uint32_t) RTAioMgrFileRetain(RTAIOMGRFILE hAioMgrFile)
{
    PRTAIOMGRFILEINT pThis = hAioMgrFile;
    AssertReturn(hAioMgrFile != NIL_RTAIOMGRFILE, UINT32_MAX);
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTAIOMGRFILE_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1G, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

RTDECL(uint32_t) RTAioMgrFileRelease(RTAIOMGRFILE hAioMgrFile)
{
    PRTAIOMGRFILEINT pThis = hAioMgrFile;
    if (pThis == NIL_RTAIOMGRFILE)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTAIOMGRFILE_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1G, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtAioMgrFileDestroy(pThis);
    return cRefs;
}

RTDECL(int) RTAioMgrFileRead(RTAIOMGRFILE hAioMgrFile, RTFOFF off,
                             PRTSGBUF pSgBuf, size_t cbRead, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int) RTAioMgrFileWrite(RTAIOMGRFILE hAioMgrFile, RTFOFF off,
                              PRTSGBUF pSgBuf, size_t cbWrite, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(int) RTAioMgrFileFlush(RTAIOMGRFILE hAioMgrFile, void *pvUser)
{
    return VERR_NOT_IMPLEMENTED;
}

