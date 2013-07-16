/* $Id$ */
/** @file
 * IPRT - Thread-Context Hook, Ring-0 Driver, Linux.
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
#include "the-linux-kernel.h"
#include "internal/iprt.h"

#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "internal/thread.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23) && defined(CONFIG_PREEMPT_NOTIFIERS)

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The internal thread-context event.
 */
typedef struct RTTHREADCTXINT
{
    /** Magic value (RTTHREADCTXINT_MAGIC). */
    uint32_t volatile           u32Magic;
    /** The thread handle (owner) for which the context-hooks are registered. */
    RTTHREAD                    hOwner;
    /** The preemption notifier object. */
    struct preempt_notifier     hPreemptNotifier;
    /** Whether this handle has any hooks registered or not. */
    bool                        fRegistered;
    /** Pointer to the registered thread-context hook. */
    PFNRTTHREADCTXHOOK          pfnThreadCtxHook;
    /** User argument passed to the thread-context hook. */
    void                       *pvUser;
    /** The thread-context operations. */
    struct preempt_ops          hPreemptOps;
} RTTHREADCTXINT, *PRTTHREADCTXINT;


/**
 * Hook function for the thread-preempting event.
 *
 * @param   pPreemptNotifier    Pointer to the preempt_notifier struct.
 * @param   pNext               Pointer to the task that is preempting the
 *                              current thread.
 *
 * @remarks Called with the rq (runqueue) lock held and with preemption
 *          disabled!
 */
static void rtThreadCtxHooksLnxSchedOut(struct preempt_notifier *pPreemptNotifier, struct task_struct *pNext)
{
    PRTTHREADCTXINT pThis = RT_FROM_MEMBER(pPreemptNotifier, RTTHREADCTXINT, hPreemptNotifier);
    AssertPtr(pThis);
    AssertPtr(pThis->pfnThreadCtxHook);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    pThis->pfnThreadCtxHook(RTTHREADCTXEVENT_PREEMPTING, pThis->pvUser);
}


/**
 * Hook function for the thread-resumed event.
 *
 * @param   pPreemptNotifier    Pointer to the preempt_notifier struct.
 * @param   iCpu                The CPU this thread is scheduled on.
 *
 * @remarks Called without holding the rq (runqueue) lock and with preemption
 *          enabled!
 */
static void rtThreadCtxHooksLnxSchedIn(struct preempt_notifier *pPreemptNotifier, int iCpu)
{
    PRTTHREADCTXINT pThis = RT_FROM_MEMBER(pPreemptNotifier, RTTHREADCTXINT, hPreemptNotifier);
    AssertPtr(pThis);
    AssertPtr(pThis->pfnThreadCtxHook);

    pThis->pfnThreadCtxHook(RTTHREADCTXEVENT_RESUMED, pThis->pvUser);
}


/**
 * Worker function for RTThreadCtxHooks(Deregister|Destroy)().
 *
 * @param   pThis   Pointer to the internal thread-context struct.
 */
DECLINLINE(void) rtThreadCtxHooksDeregister(PRTTHREADCTXINT pThis)
{
    preempt_notifier_unregister(&pThis->hPreemptNotifier);
    pThis->hPreemptOps.sched_out = NULL;
    pThis->hPreemptOps.sched_in  = NULL;
    pThis->fRegistered           = false;
}


RTDECL(int) RTThreadCtxHooksCreate(PRTTHREADCTX phThreadCtx)
{
    PRTTHREADCTXINT pThis;
    Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    pThis = (PRTTHREADCTXINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_UNLIKELY(!pThis))
        return VERR_NO_MEMORY;
    pThis->u32Magic    = RTTHREADCTXINT_MAGIC;
    pThis->hOwner      = RTThreadSelf();
    pThis->fRegistered = false;
    preempt_notifier_init(&pThis->hPreemptNotifier, &pThis->hPreemptOps);

    *phThreadCtx = pThis;
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksCreate);


RTDECL(void) RTThreadCtxHooksDestroy(RTTHREADCTX hThreadCtx)
{
    /*
     * Validate input.
     */
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return;
    AssertPtr(pThis);
    AssertMsgReturnVoid(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis));
    Assert(pThis->hOwner == RTThreadSelf());

    /*
     * If there's still a registered thread-context hook, deregister it now before destroying the object.
     */
    if (pThis->fRegistered)
        rtThreadCtxHooksDeregister(pThis);

    /*
     * Paranoia... but since these are ring-0 threads we can't be too careful.
     */
    Assert(!pThis->fRegistered);
    Assert(!pThis->hPreemptOps.sched_out);
    Assert(!pThis->hPreemptOps.sched_in);

    /*
     * Destroy the object.
     */
    ASMAtomicWriteU32(&pThis->u32Magic, ~RTTHREADCTXINT_MAGIC);
    RTMemFree(pThis);
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksDestroy);


RTDECL(int) RTThreadCtxHooksRegister(RTTHREADCTX hThreadCtx, PFNRTTHREADCTXHOOK pfnThreadCtxHook, void *pvUser)
{
    /*
     * Validate input.
     */
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return VINF_SUCCESS;
    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    VERR_INVALID_HANDLE);
    Assert(pThis->hOwner == RTThreadSelf());
    Assert(!pThis->hPreemptOps.sched_out);
    Assert(!pThis->hPreemptOps.sched_in);

    /*
     * Register the callback.
     */
    pThis->hPreemptOps.sched_out = rtThreadCtxHooksLnxSchedOut;
    pThis->hPreemptOps.sched_in  = rtThreadCtxHooksLnxSchedIn;
    pThis->pvUser                = pvUser;
    pThis->pfnThreadCtxHook      = pfnThreadCtxHook;
    pThis->fRegistered           = true;
    preempt_notifier_register(&pThis->hPreemptNotifier);

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRegister);


RTDECL(int) RTThreadCtxHooksDeregister(RTTHREADCTX hThreadCtx)
{
    /*
     * Validate input.
     */
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return VINF_SUCCESS;
    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    VERR_INVALID_HANDLE);
    Assert(pThis->hOwner == RTThreadSelf());
    Assert(pThis->fRegistered);

    /*
     * Deregister the callback.
     */
    rtThreadCtxHooksDeregister(pThis);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksDeregister);

#else    /* Not supported / Not needed */

RTDECL(int) RTThreadCtxHooksCreate(PRTTHREADCTX phThreadCtx)
{
    NOREF(phThreadCtx);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksCreate);


RTDECL(void) RTThreadCtxHooksDestroy(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksDestroy);


RTDECL(int) RTThreadCtxHooksRegister(RTTHREADCTX hThreadCtx, PFNRTTHREADCTXHOOK pfnThreadCtxHook, void *pvUser)
{
    NOREF(hThreadCtx);
    NOREF(pfnThreadCtxHook);
    NOREF(pvUser);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRegister);


RTDECL(int) RTThreadCtxHooksDeregister(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksDeregister);

#endif   /* Not supported / Not needed */

