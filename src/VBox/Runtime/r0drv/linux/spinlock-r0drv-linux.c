/* $Id$ */
/** @file
 * IPRT - Spinlocks, Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/spinlock.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/thread.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the spinlock_t structure.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The linux spinlock structure. */
    spinlock_t          Spinlock;
#ifdef RT_MORE_STRICT
    /** The idAssertCpu variable before acquring the lock for asserting after
     *  releasing the spinlock. */
    RTCPUID volatile    idAssertCpu;
    /** The CPU that owns the lock. */
    RTCPUID volatile    idCpuOwner;
#elif !defined(CONFIG_SMP) || defined(RT_ARCH_AMD64)
    /** A placeholder on Uni systems so we won't piss off RTMemAlloc(). */
    void                *pvUniDummy;
#endif
#ifdef RT_STRICT
    /** Set if we've seen any fNoIrqs usage.
     * This must be consistent or it won't work you know. */
    bool                fNoIrqs;
#endif
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;



RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock)
{
    /*
     * Allocate.
     */
    PRTSPINLOCKINTERNAL pThis;
    Assert(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    pThis = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    /*
     * Initialize and return.
     */
    pThis->u32Magic    = RTSPINLOCK_MAGIC;
#ifdef RT_MORE_STRICT
    pThis->idCpuOwner  = NIL_RTCPUID;
    pThis->idAssertCpu = NIL_RTCPUID;
#endif
#ifdef RT_STRICT
    pThis->fNoIrqs     = false;
#endif
    spin_lock_init(&pThis->Spinlock);

    *pSpinlock = pThis;
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSpinlockCreate);


RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock)
{
    /*
     * Validate input.
     */
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    if (pThis->u32Magic != RTSPINLOCK_MAGIC)
    {
        AssertMsgFailed(("Invalid spinlock %p magic=%#x\n", pThis, pThis->u32Magic));
        return VERR_INVALID_PARAMETER;
    }

    ASMAtomicIncU32(&pThis->u32Magic);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSpinlockDestroy);


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_VAR();
    AssertMsg(pThis && pThis->u32Magic == RTSPINLOCK_MAGIC,
              ("pThis=%p u32Magic=%08x\n", pThis, pThis ? (int)pThis->u32Magic : 0));

    spin_lock_irqsave(&pThis->Spinlock, pTmp->flFlags);

    RT_ASSERT_PREEMPT_CPUID_SPIN_ACQUIRED(pThis);
#ifdef RT_STRICT
    pThis->fNoIrqs = true;
#endif
}
RT_EXPORT_SYMBOL(RTSpinlockAcquireNoInts);


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE_VARS();

    AssertMsg(pThis && pThis->u32Magic == RTSPINLOCK_MAGIC,
              ("pThis=%p u32Magic=%08x\n", pThis, pThis ? (int)pThis->u32Magic : 0));
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE(pThis);
    Assert(pThis->fNoIrqs);

    spin_unlock_irqrestore(&pThis->Spinlock, pTmp->flFlags);

    RT_ASSERT_PREEMPT_CPUID();
}
RT_EXPORT_SYMBOL(RTSpinlockReleaseNoInts);


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_VAR();
    AssertMsg(pThis && pThis->u32Magic == RTSPINLOCK_MAGIC,
              ("pThis=%p u32Magic=%08x\n", pThis, pThis ? (int)pThis->u32Magic : 0));
    NOREF(pTmp);
    Assert(!pThis->fNoIrqs || !ASMIntAreEnabled());

    spin_lock(&pThis->Spinlock);

    RT_ASSERT_PREEMPT_CPUID_SPIN_ACQUIRED(pThis);
}
RT_EXPORT_SYMBOL(RTSpinlockAcquire);


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE_VARS();
    AssertMsg(pThis && pThis->u32Magic == RTSPINLOCK_MAGIC,
              ("pThis=%p u32Magic=%08x\n", pThis, pThis ? (int)pThis->u32Magic : 0));
    NOREF(pTmp);
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE(pThis);
    Assert(!pThis->fNoIrqs || !ASMIntAreEnabled());

    spin_unlock(&pThis->Spinlock);

    RT_ASSERT_PREEMPT_CPUID();
}
RT_EXPORT_SYMBOL(RTSpinlockRelease);

