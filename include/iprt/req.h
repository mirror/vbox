/** @file
 * IPRT - Request Queue & Pool.
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

#ifndef ___iprt_req_h
#define ___iprt_req_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

#include <iprt/stdarg.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_req    RTReq - Request Queue & Pool.
 * @ingroup grp_rt
 * @{
 */

/** Request queue handle. */
typedef struct RTREQQUEUEINT *RTREQQUEUE;
/** Pointer to a request queue handle. */
typedef RTREQQUEUE *PRTREQQUEUE;
/** NIL request queue handle. */
#define NIL_RTREQQUEUE      ((RTREQQUEUE)0)

/** Request thread pool handle. */
typedef struct RTREQPOOLINT *RTREQPOOL;
/** Poiner to a request thread pool handle. */
typedef RTREQPOOL *PRTREQPOOL;
/** NIL request pool handle. */
#define NIL_RTREQPOOL       ((RTREQPOOL)0)


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
    RTREQQUEUE              hQueue;
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


#ifdef IN_RING3

/**
 * Create a request packet queue
 *
 * @returns iprt status code.
 * @param   phQueue         Where to store the request queue handle.
 */
RTDECL(int) RTReqQueueCreate(PRTREQQUEUE phQueue);

/**
 * Destroy a request packet queue
 *
 * @returns iprt status code.
 * @param   hQueue          The request queue.
 */
RTDECL(int) RTReqQueueDestroy(RTREQQUEUE hQueue);

/**
 * Process one or more request packets
 *
 * @returns iprt status code.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being added.
 *
 * @param   hQueue          The request queue.
 * @param   cMillies        Number of milliseconds to wait for a pending request.
 *                          Use RT_INDEFINITE_WAIT to only wait till one is added.
 */
RTDECL(int) RTReqQueueProcess(RTREQQUEUE hQueue, RTMSINTERVAL cMillies);

/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completion. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt statuscode.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   hQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happens.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 * @param   ...             Function arguments.
 *
 * @remarks See remarks on RTReqQueueCallV.
 */
RTDECL(int) RTReqQueueCall(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, PFNRT pfnFunction, unsigned cArgs, ...);

/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completion. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   hQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happens.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 * @param   ...             Function arguments.
 *
 * @remarks See remarks on RTReqQueueCallV.
 */
RTDECL(int) RTReqQueueCallVoid(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, PFNRT pfnFunction, unsigned cArgs, ...);

/**
 * Allocate and queue a call request to a void function.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completion. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   hQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happens, unless fFlags
 *                          contains RTREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the RTREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 * @param   ...             Function arguments.
 *
 * @remarks See remarks on RTReqQueueCallV.
 */
RTDECL(int) RTReqQueueCallEx(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, ...);

/**
 * Allocate and queue a call request.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completion. In the other case
 * use RT_INDEFINITE_WAIT.
 * The returned request packet must be freed using RTReqFree().
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @returns VERR_TIMEOUT if cMillies was reached without the packet being completed.
 *
 * @param   hQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the request.
 *                          This will be NULL or a valid request pointer not matter what happens, unless fFlags
 *                          contains RTREQFLAGS_NO_WAIT when it will be optional and always NULL.
 * @param   cMillies        Number of milliseconds to wait for the request to
 *                          be completed. Use RT_INDEFINITE_WAIT to only
 *                          wait till it's completed.
 * @param   fFlags          A combination of the RTREQFLAGS values.
 * @param   pfnFunction     Pointer to the function to call.
 * @param   cArgs           Number of arguments following in the ellipsis.
 * @param   Args            Variable argument vector.
 *
 * @remarks Caveats:
 *              - Do not pass anything which is larger than an uintptr_t.
 *              - 64-bit integers are larger than uintptr_t on 32-bit hosts.
 *                Pass integers > 32-bit by reference (pointers).
 *              - Don't use NULL since it should be the integer 0 in C++ and may
 *                therefore end up with garbage in the bits 63:32 on 64-bit
 *                hosts because 'int' is 32-bit.
 *                Use (void *)NULL or (uintptr_t)0 instead of NULL.
 */
RTDECL(int) RTReqQueueCallV(RTREQQUEUE hQueue, PRTREQ *ppReq, RTMSINTERVAL cMillies, unsigned fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args);

/**
 * Checks if the queue is busy or not.
 *
 * The caller is responsible for dealing with any concurrent submitts.
 *
 * @returns true if busy, false if idle.
 * @param   hQueue              The queue.
 */
RTDECL(bool) RTReqQueueIsBusy(RTREQQUEUE hQueue);

/**
 * Allocates a request packet.
 *
 * The caller allocates a request packet, fills in the request data
 * union and queues the request.
 *
 * @returns iprt status code.
 *
 * @param   hQueue          The request queue.
 * @param   ppReq           Where to store the pointer to the allocated packet.
 * @param   enmType         Package type.
 */
RTDECL(int) RTReqQueueAlloc(RTREQQUEUE hQueue, PRTREQ *ppReq, RTREQTYPE enmType);


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
 * The quest must be allocated using RTReqQueueAlloc() or RTReqPoolAlloc() and
 * contain all the required data.
 *
 * If it's desired to poll on the completion of the request set cMillies
 * to 0 and use RTReqWait() to check for completion. In the other case
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
RTDECL(int) RTReqSubmit(PRTREQ pReq, RTMSINTERVAL cMillies);


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
RTDECL(int) RTReqWait(PRTREQ pReq, RTMSINTERVAL cMillies);


#endif /* IN_RING3 */


/** @} */

RT_C_DECLS_END

#endif

