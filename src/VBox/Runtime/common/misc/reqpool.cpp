/* $Id$ */
/** @file
 * IPRT - Request Pool.
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
#include <iprt/critsect.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include "internal/req.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTREQPOOLTHREAD
{
    /** Node in the  RTREQPOOLINT::IdleThreads list. */
    RTLISTNODE              IdleNode;
    /** Node in the  RTREQPOOLINT::WorkerThreads list. */
    RTLISTNODE              ListNode;

    /** The submit timestamp of the pending request. */
    uint64_t                uPendingNanoTs;
    /** When this CPU went idle the last time. */
    uint64_t                uIdleNanoTs;
    /** The number of requests processed by this thread. */
    uint64_t                cReqProcessed;
    /** Total time the requests processed by this thread took to process. */
    uint64_t                cNsTotalReqProcessing;
    /** Total time the requests processed by this thread had to wait in
     * the queue before being scheduled. */
    uint64_t                cNsTotalReqQueued;
    /** The CPU this was scheduled last time we checked. */
    RTCPUID                 idLastCpu;

    /** The thread handle. */
    RTTHREAD                hThread;

    /** The submitter will put an incoming request here when scheduling an idle
     * thread.  */
    PRTREQINT volatile      pTodoReq;
    /** The request the thread is currently processing. */
    PRTREQINT volatile      pPendingReq;

} RTREQPOOLTHREAD;

typedef struct RTREQPOOLINT
{
    /** Magic value (RTREQPOOL_MAGIC). */
    uint32_t                u32Magic;

    /** The current number of worker threads. */
    uint32_t                cCurThreads;
    /** The maximum number of worker threads. */
    uint32_t                cMaxThreads;
    /** The number of threads which should be spawned before throttling kicks
     * in. */
    uint32_t                cThreadsThreshold;
    /** The minimum number of worker threads. */
    uint32_t                cMinThreads;
    /** The number of milliseconds a thread needs to be idle before it is
     * considered for retirement. */
    uint32_t                cMsMinIdle;

    /** Statistics: The total number of threads created. */
    uint32_t                cThreadsCreated;
    /** Statistics: The timestamp when the last thread was created. */
    uint64_t                uLastThreadCreateNanoTs;
    /** Linked list of worker threads. */
    RTLISTANCHOR            WorkerThreads;

    /** Critical section serializing access to members of this structure.  */
    RTCRITSECT              CritSect;

    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** Linked list of idle threads. */
    RTLISTANCHOR            IdleThreads;


} RTREQPOOLINT;
/** Pointer to a request thread pool instance. */
typedef RTREQPOOLINT *PRTREQPOOLINT;

