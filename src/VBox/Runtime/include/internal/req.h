/* $Id$ */
/** @file
 * IPRT - Internal RTReq header.
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

#ifndef ___internal_req_h
#define ___internal_req_h

#include <iprt/types.h>


RT_C_DECLS_BEGIN

/**
 * Request state.
 */
typedef enum RTREQSTATE
{
    /** The state is invalid. */
    RTREQSTATE_INVALID = 0,
    /** The request have been allocated and is in the process of being filed. */
    RTREQSTATE_ALLOCATED,
    /** The request is queued by the requester. */
    RTREQSTATE_QUEUED,
    /** The request is begin processed. */
    RTREQSTATE_PROCESSING,
    /** The request is completed, the requester is begin notified. */
    RTREQSTATE_COMPLETED,
    /** The request packet is in the free chain. (The requester */
    RTREQSTATE_FREE
} RTREQSTATE;


/**
 * RT Request packet.
 *
 * This is used to request an action in the queue handler thread.
 */
struct RTREQ
{
    /** Magic number (RTREQ_MAGIC). */
    uint32_t                u32Magic;
    /** Set if the event semaphore is clear. */
    volatile bool           fEventSemClear;
    /** Set if pool, clear if queue. */
    volatile bool           fPoolOrQueue;
    /** IPRT status code for the completed request. */
    volatile int32_t        iStatusX;
    /** Request state. */
    volatile RTREQSTATE     enmState;

    /** Pointer to the next request in the chain. */
    struct RTREQ * volatile pNext;

    union
    {
        /** Pointer to the pool this packet belongs to. */
        RTREQPOOL           hPool;
        /** Pointer to the queue this packet belongs to. */
        RTREQQUEUE          hQueue;
    } uOwner;

    /** Requester event sem.
     * The request can use this event semaphore to wait/poll for completion
     * of the request.
     */
    RTSEMEVENT              EventSem;
    /** Flags, RTREQ_FLAGS_*. */
    uint32_t                fFlags;
    /** Request type. */
    RTREQTYPE               enmType;
    /** Request specific data. */
    union RTREQ_U
    {
        /** RTREQTYPE_INTERNAL. */
        struct
        {
            /** Pointer to the function to be called. */
            PFNRT               pfn;
            /** Number of arguments. */
            uint32_t            cArgs;
            /** Array of arguments. */
            uintptr_t           aArgs[64];
        } Internal;
    } u;
};


/**
 * Internal queue instance.
 */
typedef struct RTREQQUEUEINT
{
    /** Magic value (RTREQQUEUE_MAGIC). */
    uint32_t                u32Magic;
    /** Set if busy (pending or processing requests). */
    bool volatile           fBusy;
    /** Head of the request queue. Atomic. */
    volatile PRTREQ         pReqs;
    /** The last index used during alloc/free. */
    volatile uint32_t       iReqFree;
    /** Number of free request packets. */
    volatile uint32_t       cReqFree;
    /** Array of pointers to lists of free request packets. Atomic. */
    volatile PRTREQ         apReqFree[9];
    /** Requester event sem.
     * The request can use this event semaphore to wait/poll for new requests.
     */
    RTSEMEVENT              EventSem;
} RTREQQUEUEINT;

/** Pointer to an internal queue instance. */
typedef RTREQQUEUEINT *PRTREQQUEUEINT;



DECLHIDDEN(int) rtReqProcessOne(PRTREQ pReq);

RT_C_DECLS_END

#endif

