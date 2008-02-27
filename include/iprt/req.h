/** @file
 * innotek Portable Runtime - Request packets
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___iprt_req_h
#define ___iprt_req_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_rt_req    RTReq - Request Packet Management
 * @ingroup grp_rt
 * @{
 */

/**
 * Request type.
 */
typedef enum RTREQTYPE
{
    /** Invalid request. */
    RTREQTYPE_INVALID = 0,
    /** RT: Internal. */
    RTREQTYPE_INTERNAL,
    /** Maximum request type (exclusive). Used for validation. */
    RTREQTYPE_MAX
} RTREQTYPE;

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
 * Request flags.
 */
typedef enum RTREQFLAGS
{
    /** The request returns a iprt status code. */
    RTREQFLAGS_IPRT_STATUS  = 0,
    /** The request is a void request and have no status code. */
    RTREQFLAGS_VOID         = 1,
    /** Return type mask. */
    RTREQFLAGS_RETURN_MASK  = 1,
    /** Caller does not wait on the packet, Queue process thread will free it. */
    RTREQFLAGS_NO_WAIT      = 2
} RTREQFLAGS;

/** Pointer to a request queue. */
typedef struct RTREQQUEUE *PRTREQQUEUE;

/**
 * RT Request packet.
 *
 * This is used to request an action in the queue handler thread.
 */
typedef struct RTREQ
{
    /** Pointer to the next request in the chain. */
    struct RTREQ * volatile pNext;
    /** Pointer to the queue this packet belongs to. */
    PRTREQQUEUE             pQueue;
    /** Request state. */
    volatile RTREQSTATE     enmState;
    /** iprt status code for the completed request. */
    volatile int            iStatus;
    /** Requester event sem.
     * The request can use this event semaphore to wait/poll for completion
     * of the request.
     */
    RTSEMEVENT              EventSem;
    /** Set if the event semaphore is clear. */
    volatile bool           fEventSemClear;
    /** Flags, RTREQ_FLAGS_*. */
    unsigned                fFlags;
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
            unsigned            cArgs;
            /** Array of arguments. */
            uintptr_t           aArgs[64];
        } Internal;
    } u;
} RTREQ;
/** Pointer to an RT request packet. */
typedef RTREQ *PRTREQ;

/** @todo hide this */
typedef struct RTREQQUEUE
{
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
} RTREQQUEUE;

#ifdef IN_RING3

/**
 * Create a request packet queueu
 *
 * @returns iprt status code.
 * @param   ppQueue         Where to store the request queue pointer.
 */
RTDECL(int) RTReqCreateQueue(PRTREQQUEUE *ppQueue);


/**
 * Destroy a request packet queueu
 *
 * @returns iprt status code.
 * @param   pQueue          The request queue.
 */
RTDECL(int) RTReqDestroyQueue(PRTREQQUEUE pQueue);


/**
 * Process one or more request packets
 *
 * @returns iprt status code.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being added.
 *
 * @param   pQueue          The request queue.
 * @param   cMillies        Number of milliseconds to wait for a pending request.
 *                          Use RT_INDEFINITE_WAIT to only wait till one is added.
 */
RTDECL(int) RTReqProcess(PRTREQQUEUE pQueue, unsigned cMillies);


/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt statuscode.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
RTDECL(int) RTReqCall(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);


/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
RTDECL(int) RTReqCallVoid(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, PFNRT pfnFunction, unsigned cArgs, ...);


/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains RTREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the RTREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   ...             Function arguments.
 */
RTDECL(int) RTReqCallEx(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...);


/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happends, unless fFlags
 *                          contains RTREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the RTREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 *                          Not possible to pass 64-bit arguments!
 * @param   Args            Variable argument vector.
 */
RTDECL(int) RTReqCallV(PRTREQQUEUE pQueue, PRTREQ *ppReq, unsigned cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args);


/**
 * Allocates a request packet.
 *
 * The caller allocates a request packet, fills in the request data
 * union and queues the request.
 *
 * @returns iprt status code.
 *
 * @param   pQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the allocated packet.
 * @param   enmType         Package type.
 */
RTDECL(int) RTReqAlloc(PRTREQQUEUE pQueue, PRTREQ *ppReq, RTREQTYPE enmType);


/**
 * Free a request packet.
 *
 * @returns iprt status code.
 *
 * @param   pReq            Package to free.
 * @remark  The request packet must be in allocated or completed state!
 */
RTDECL(int) RTReqFree(PRTREQ pReq);


/**
 * Queue a request.
 *
 * The quest must be allocated using RTReqAlloc() and contain
 * all the required data.
 * If it's disired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completation. In the other case
 * use RT_INDEFINITE_WAIT.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to queue.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 */
RTDECL(int) RTReqQueue(PRTREQ pReq, unsigned cMillies);


/**
 * Wait for a request to be completed.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   pReq            The request to wait for.
 * @param   cMillies        Number of milliseconds to wait.
 *                          Use RT_INDEFINITE_WAIT to only wait till it's completed.
 */
RTDECL(int) RTReqWait(PRTREQ pReq, unsigned cMillies);


#endif /* IN_RING3 */


/** @} */

__END_DECLS

#endif

