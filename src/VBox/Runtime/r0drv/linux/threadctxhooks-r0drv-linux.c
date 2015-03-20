/* $Id$ */
/** @file
 * IPRT - Thread-Context Hook, Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
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
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include "internal/thread.h"

/*
 * Linux kernel 2.6.23 introduced thread-context hooks but RedHat 2.6.18 kernels
 * got it backported.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18) && defined(CONFIG_PREEMPT_NOTIFIERS)

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
    /** The reference count for this object. */
    uint32_t volatile           cRefs;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 19) && defined(RT_ARCH_AMD64)
    /** Starting with 3.1.19, the linux kernel doesn't restore kernel RFLAGS during
     * task switch, so we have to do that ourselves. (x86 code is not affected.) */
    RTCCUINTREG                 fSavedRFlags;
#endif
} RTTHREADCTXINT, *PRTTHREADCTXINT;


/**
 * Hook function for the thread-preempting event.
 *
 * @param   pPreemptNotifier    Pointer to the preempt_notifier struct.
 * @param   pNext               Pointer to the task that is preempting the
 *                              current thread.
 *
 * @remarks Called with the rq (runqueue) lock held and with preemption and
 *          interrupts disabled!
 */
static void rtThreadCtxHooksLnxSchedOut(struct preempt_notifier *pPreemptNotifier, struct task_struct *pNext)
{
    PRTTHREADCTXINT pThis = RT_FROM_MEMBER(pPreemptNotifier, RTTHREADCTXINT, hPreemptNotifier);
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    RTCCUINTREG fSavedEFlags = ASMGetFlags();
    stac();
#endif

    AssertPtr(pThis);
    AssertPtr(pThis->pfnThreadCtxHook);
    Assert(pThis->fRegistered);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    pThis->pfnThreadCtxHook(RTTHREADCTXEVENT_PREEMPTING, pThis->pvUser);

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    ASMSetFlags(fSavedEFlags);
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 19) && defined(RT_ARCH_AMD64)
    pThis->fSavedRFlags = fSavedEFlags;
# endif
#endif
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
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    RTCCUINTREG fSavedEFlags = ASMGetFlags();
    stac();
#endif

    AssertPtr(pThis);
    AssertPtr(pThis->pfnThreadCtxHook);
    Assert(pThis->fRegistered);

    pThis->pfnThreadCtxHook(RTTHREADCTXEVENT_RESUMED, pThis->pvUser);

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 19) && defined(RT_ARCH_AMD64)
    fSavedEFlags &= ~RT_BIT_64(18) /*X86_EFL_AC*/;
    fSavedEFlags |= pThis->fSavedRFlags & RT_BIT_64(18) /*X86_EFL_AC*/;
# endif
    ASMSetFlags(fSavedEFlags);
#endif
}


/**
 * Worker function for RTThreadCtxHooks(Deregister|Release)().
 *
 * @param   pThis   Pointer to the internal thread-context object.
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
    pThis->hOwner      = RTThreadNativeSelf();
    pThis->fRegistered = false;
    preempt_notifier_init(&pThis->hPreemptNotifier, &pThis->hPreemptOps);
    pThis->cRefs       = 1;

    *phThreadCtx = pThis;
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksCreate);


RTDECL(uint32_t) RTThreadCtxHooksRetain(RTTHREADCTX hThreadCtx)
{
    /*
     * Validate input.
     */
    uint32_t        cRefs;
    PRTTHREADCTXINT pThis = hThreadCtx;
    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    UINT32_MAX);

    cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    return cRefs;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRetain);



RTDECL(uint32_t) RTThreadCtxHooksRelease(RTTHREADCTX hThreadCtx)
{
    /*
     * Validate input.
     */
    uint32_t        cRefs;
    PRTTHREADCTXINT pThis = hThreadCtx;
    if (pThis == NIL_RTTHREADCTX)
        return 0;

    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    UINT32_MAX);
    Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    cRefs = ASMAtomicDecU32(&pThis->cRefs);
    if (!cRefs)
    {
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

        ASMAtomicWriteU32(&pThis->u32Magic, ~RTTHREADCTXINT_MAGIC);
        RTMemFree(pThis);
    }
    else
        Assert(cRefs < UINT32_MAX / 2);

    return cRefs;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRelease);


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
        return VERR_INVALID_HANDLE;
    AssertPtr(pThis);
    AssertMsgReturn(pThis->u32Magic == RTTHREADCTXINT_MAGIC, ("pThis->u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis),
                    VERR_INVALID_HANDLE);
    Assert(pThis->hOwner == RTThreadNativeSelf());
    Assert(pThis->fRegistered);

    /*
     * Deregister the callback.
     */
    rtThreadCtxHooksDeregister(pThis);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksDeregister);


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

#else    /* Not supported / Not needed */

RTDECL(int) RTThreadCtxHooksCreate(PRTTHREADCTX phThreadCtx)
{
    NOREF(phThreadCtx);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksCreate);


RTDECL(uint32_t) RTThreadCtxHooksRetain(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return UINT32_MAX;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRetain);


RTDECL(uint32_t) RTThreadCtxHooksRelease(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return UINT32_MAX;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRelease);


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


RTDECL(bool) RTThreadCtxHooksAreRegistered(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return false;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksAreRegistered);

#endif   /* Not supported / Not needed */

