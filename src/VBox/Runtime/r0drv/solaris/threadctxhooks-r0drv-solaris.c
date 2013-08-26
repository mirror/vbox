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
#include <iprt/log.h>
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
    RTNATIVETHREAD              hOwner;
    /** Pointer to the registered thread-context hook. */
    PFNRTTHREADCTXHOOK          pfnThreadCtxHook;
    /** User argument passed to the thread-context hook. */
    void                       *pvUser;
    /** Whether this handle has any hooks registered or not. */
    bool volatile               fRegistered;
    /** Number of references to this object. */
    uint32_t volatile           cRefs;
} RTTHREADCTXINT, *PRTTHREADCTXINT;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a thread-context hook handle and returns rc if not valid. */
#define RTTHREADCTX_VALID_RETURN_RC(pThis, rc) \
    do { \
        AssertPtrReturn((pThis), (rc)); \
        AssertReturn((pThis)->u32Magic == RTTHREADCTXINT_MAGIC, (rc)); \
        AssertReturn((pThis)->cRefs > 0, (rc)); \
    } while (0)


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


/**
 * Hook function for the thread-free event.
 *
 * @param   pvThreadCtxInt      Opaque pointer to the internal thread-context
 *                              object.
 * @param   fIsExec             Whether this event is triggered due to exec().
 */
static void rtThreadCtxHooksSolFree(void *pvThreadCtxInt, int fIsExec)
{
    PRTTHREADCTXINT pThis = (PRTTHREADCTXINT)pvThreadCtxInt;
    AssertPtrReturnVoid(pThis);
    AssertMsgReturnVoid(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis));

    uint32_t cRefs = ASMAtomicReadU32(&pThis->cRefs);
    if (RT_UNLIKELY(!cRefs))
    {
        /* Should never happen. */
        AssertMsgFailed(("rtThreadCtxHooksSolFree with cRefs=0 pThis=%p\n", pThis));
        return;
    }

    cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (!cRefs)
    {
        Assert(!pThis->fRegistered);
        ASMAtomicWriteU32(&pThis->u32Magic, ~RTTHREADCTXINT_MAGIC);
        RTMemFree(pThis);
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
    pThis->hOwner      = RTThreadNativeSelf();
    pThis->fRegistered = false;
    pThis->cRefs       = 2;               /* One reference for the thread, one for the hook object. */

    /*
     * installctx() allocates memory and thus cannot be used in RTThreadCtxHooksRegister() which can be used
     * with preemption disabled. We allocate the context-hooks here and use 'fRegistered' to determine if we can
     * invoke the consumer's hook or not.
     */
    if (g_frtSolOldThreadCtx)
    {
        g_rtSolThreadCtx.Install.pfnSol_installctx_old(curthread,
                                                       pThis,
                                                       rtThreadCtxHooksSolPreempting,
                                                       rtThreadCtxHooksSolResumed,
                                                       NULL,                          /* fork */
                                                       NULL,                          /* lwp_create */
                                                       rtThreadCtxHooksSolFree);
    }
    else
    {
        g_rtSolThreadCtx.Install.pfnSol_installctx(curthread,
                                                   pThis,
                                                   rtThreadCtxHooksSolPreempting,
                                                   rtThreadCtxHooksSolResumed,
                                                   NULL,                              /* fork */
                                                   NULL,                              /* lwp_create */
                                                   NULL,                              /* exit */
                                                   rtThreadCtxHooksSolFree);
    }

    *phThreadCtx = pThis;
    return VINF_SUCCESS;
}


RTDECL(uint32_t) RTThreadCtxHooksRetain(RTTHREADCTX hThreadCtx)
{
    PRTTHREADCTXINT pThis = hThreadCtx;
    RTTHREADCTX_VALID_RETURN_RC(hThreadCtx, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    return cRefs;
}


RTDECL(uint32_t) RTThreadCtxHooksRelease(RTTHREADCTX hThreadCtx)
{
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return 0;

    RTTHREADCTX_VALID_RETURN_RC(hThreadCtx, UINT32_MAX);
    Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    ASMAtomicWriteBool(&pThis->fRegistered, false);
    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);

    if (   cRefs == 1
        && pThis->hOwner == RTThreadNativeSelf())
    {
        /*
         * removectx() will invoke rtThreadCtxHooksSolFree() and there is no way to bypass it and still use
         * rtThreadCtxHooksSolFree() at the same time.  Hence the convulated reference counting.
         *
         * When this function is called from the owner thread and is the last reference, we call removectx() which
         * will invoke rtThreadCtxHooksSolFree() with cRefs = 1 and that will then free the hook object.
         *
         * When the function is called from a different thread, we simply decrement the reference. Whenever the
         * ring-0 thread dies, Solaris will call rtThreadCtxHooksSolFree() which will free the hook object.
         */
        int rc;
        if (g_frtSolOldThreadCtx)
        {
            rc = g_rtSolThreadCtx.Remove.pfnSol_removectx_old(curthread,
                                                              pThis,
                                                              rtThreadCtxHooksSolPreempting,
                                                              rtThreadCtxHooksSolResumed,
                                                              NULL,                          /* fork */
                                                              NULL,                          /* lwp_create */
                                                              rtThreadCtxHooksSolFree);
        }
        else
        {
            rc = g_rtSolThreadCtx.Remove.pfnSol_removectx(curthread,
                                                          pThis,
                                                          rtThreadCtxHooksSolPreempting,
                                                          rtThreadCtxHooksSolResumed,
                                                          NULL,                              /* fork */
                                                          NULL,                              /* lwp_create */
                                                          NULL,                              /* exit */
                                                          rtThreadCtxHooksSolFree);
        }
        AssertMsg(rc, ("removectx() failed. rc=%d\n", rc));
        NOREF(rc);

#ifdef VBOX_STRICT
        cRefs = ASMAtomicReadU32(&pThis->cRefs);
        Assert(!cRefs);
#endif
        cRefs = 0;
    }
    else if (!cRefs)
    {
        /*
         * The ring-0 thread for this hook object has already died. Free up the object as we have no more references.
         */
        Assert(pThis->hOwner != RTThreadNativeSelf());
        ASMAtomicWriteU32(&pThis->u32Magic, ~RTTHREADCTXINT_MAGIC);
        RTMemFree(pThis);
    }

    return cRefs;
}


RTDECL(int) RTThreadCtxHooksRegister(RTTHREADCTX hThreadCtx, PFNRTTHREADCTXHOOK pfnThreadCtxHook, void *pvUser)
{
    /*
     * Validate input.
     */
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return VERR_INVALID_HANDLE;
    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    VERR_INVALID_HANDLE);
    Assert(pThis->hOwner == RTThreadNativeSelf());

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
        return VERR_INVALID_HANDLE;
    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    VERR_INVALID_HANDLE);
    Assert(pThis->hOwner == RTThreadNativeSelf());
    Assert(pThis->fRegistered);

    /*
     * Deregister the callback.
     */
    pThis->fRegistered = false;

    return VINF_SUCCESS;
}


RTDECL(bool) RTThreadCtxHooksAreRegistered(RTTHREADCTX hThreadCtx)
{
    /*
     * Validate input.
     */
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return false;
    AssertPtr(pThis);
    AssertMsg(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis));

    return pThis->fRegistered;
}

