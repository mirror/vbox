/* $Id$ */
/** @file
 * IPRT - Thread-Context Hook, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"
#include "internal/iprt.h"

#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "internal/thread.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The internal thread-context object.
 */
typedef struct RTTHREADCTXINT
{
    /** Magic value (RTTHREADCTXINT_MAGIC). */
    uint32_t volatile           u32Magic;
    /** The thread handle (owner) for which the context-hooks are registered. */
    RTTHREAD                    hOwner;
    /** Pointer to the registered thread-context hook. */
    PFNRTTHREADCTXHOOK          pfnThreadCtxHook;
    /** User argument passed to the thread-context hook. */
    void                       *pvUser;
    /** Whether this handle has any hooks registered or not. */
    bool                        fRegistered;
} RTTHREADCTXINT, *PRTTHREADCTXINT;



/**
 * Hook function for the thread-preempting event.
 *
 * @param   pvThreadCtxInt  Opaque pointer to the internal thread-context
 *                          object.
 *
 * @remarks Called with the with preemption disabled!
 */
static void rtThreadCtxHooksSolPreempting(void *pvThreadCtxInt)
{
    PRTTHREADCTXINT pThis = (PRTTHREADCTXINT)pvThreadCtxInt;
    AssertPtr(pThis);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pThis->fRegistered)
    {
        Assert(pThis->pfnThreadCtxHook);
        pThis->pfnThreadCtxHook(RTTHREADCTXEVENT_PREEMPTING, pThis->pvUser);
    }
}


/**
 * Hook function for the thread-resumed event.
 *
 * @param   pvThreadCtxInt  Opaque pointer to the internal thread-context
 *                          object.
 */
static void rtThreadCtxHooksSolResumed(void *pvThreadCtxInt)
{
    PRTTHREADCTXINT pThis = (PRTTHREADCTXINT)pvThreadCtxInt;
    AssertPtr(pThis);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    if (pThis->fRegistered)
    {
        Assert(pThis->pfnThreadCtxHook);
        pThis->pfnThreadCtxHook(RTTHREADCTXEVENT_RESUMED, pThis->pvUser);
    }
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

    /*
     * installctx() allocates memory and thus cannot be used in RTThreadCtxHooksRegister() which can be used
     * with preemption disabled. We allocate the context-hooks here and use 'fRegistered' to determine if we can
     * invoke the consumer's hook or not.
     */
    installctx(curthread,
               pThis,
               rtThreadCtxHooksSolPreempting,
               rtThreadCtxHooksSolResumed,
               NULL,                            /* fork */
               NULL,                            /* lwp_create */
               NULL,                            /* exit */
               NULL);                           /* free */

    *phThreadCtx = pThis;
    return VINF_SUCCESS;
}


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
    Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Deregister the hook.
     */
    int rc = removectx(curthread,
                       pThis,
                       rtThreadCtxHooksSolPreempting,
                       rtThreadCtxHooksSolResumed,
                       NULL,                            /* fork */
                       NULL,                            /* lwp_create */
                       NULL,                            /* exit */
                       NULL);                           /* free */
    AssertMsg(rc, ("removectx failed. rc=%d\n", rc));
    NOREF(rc);

    /*
     * Destroy the object.
     */
    ASMAtomicWriteU32(&pThis->u32Magic, ~RTTHREADCTXINT_MAGIC);
    RTMemFree(pThis);
}


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

    /*
     * Register the callback.
     */
    pThis->pvUser           = pvUser;
    pThis->pfnThreadCtxHook = pfnThreadCtxHook;
    pThis->fRegistered      = true;

    return VINF_SUCCESS;
}


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
    pThis->fRegistered = false;

    return VINF_SUCCESS;
}

