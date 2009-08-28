/* $Id$ */
/** @file
 * IPRT - Spinlocks, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/spinlock.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/thread.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the struct mutex type.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** A Solaris spinlock. */
    kmutex_t            Mtx;
#ifdef RT_MORE_STRICT
    /** The idAssertCpu variable before acquring the lock for asserting after
     *  releasing the spinlock. */
    RTCPUID volatile    idAssertCpu;
    /** The CPU that owns the lock. */
    RTCPUID volatile    idCpuOwner;
#endif
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;



RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock)
{
    /*
     * Allocate.
     */
    RT_ASSERT_PREEMPTIBLE();
    AssertCompile(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    /*
     * Initialize & return.
     */
    pThis->u32Magic = RTSPINLOCK_MAGIC;
    mutex_init(&pThis->Mtx, "IPRT Spinlock", MUTEX_SPIN, (void *)ipltospl(DISP_LEVEL));
    *pSpinlock = pThis;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock)
{
    /*
     * Validate input.
     */
    RT_ASSERT_INTS_ON();
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(pThis->u32Magic == RTSPINLOCK_MAGIC,
                    ("Invalid spinlock %p magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_PARAMETER);

    /*
     * Make the lock invalid and release the memory.
     */
    ASMAtomicIncU32(&pThis->u32Magic);
    mutex_destroy(&pThis->Mtx);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_VAR();

    AssertPtr(pThis);
    Assert(pThis->u32Magic == RTSPINLOCK_MAGIC);

    pTmp->uFlags = ASMIntDisableFlags();
    mutex_enter(&pThis->Mtx);

    Assert(!ASMIntAreEnabled());
    RT_ASSERT_PREEMPT_CPUID_SPIN_ACQUIRED(pThis);
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE_VARS();

    AssertPtr(pThis);
    Assert(pThis->u32Magic == RTSPINLOCK_MAGIC);
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE(pThis);
    NOREF(pTmp);

    mutex_exit(&pThis->Mtx);
    ASMSetFlags(pTmp->uFlags);

    RT_ASSERT_PREEMPT_CPUID();
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_VAR();
    AssertPtr(pThis);
    Assert(pThis->u32Magic == RTSPINLOCK_MAGIC);
    NOREF(pTmp);
#ifdef RT_STRICT
    bool fIntsOn = ASMIntAreEnabled();
#endif

    mutex_enter(&pThis->Mtx);

    AssertMsg(fIntsOn == ASMIntAreEnabled(), ("fIntsOn=%RTbool\n", fIntsOn));

    RT_ASSERT_PREEMPT_CPUID_SPIN_ACQUIRED(pThis);
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE_VARS();

    AssertPtr(pThis);
    Assert(pThis->u32Magic == RTSPINLOCK_MAGIC);
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE(pThis);
    NOREF(pTmp);
#ifdef RT_STRICT
    bool fIntsOn = ASMIntAreEnabled();
#endif

    mutex_exit(&pThis->Mtx);

    AssertMsg(fIntsOn == ASMIntAreEnabled(), ("fIntsOn=%RTbool\n", fIntsOn));
    RT_ASSERT_PREEMPT_CPUID();
}

