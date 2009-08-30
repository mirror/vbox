/* $Id$ */
/** @file
 * IPRT - Spinlocks, Ring-0 Driver, FreeBSD.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-freebsd-kernel.h"
#include "internal/iprt.h"

#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/mp.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the struct mtx type.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The spinlock. */
    uint32_t volatile   fLocked;
    /** Reserved to satisfy compile assertion below. */
    uint32_t            uReserved;
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
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)RTMemAllocZ(sizeof(*pSpinlockInt));
    if (!pSpinlockInt)
        return VERR_NO_MEMORY;

    /*
     * Initialize & return.
     */
    pSpinlockInt->u32Magic = RTSPINLOCK_MAGIC;
    pSpinlockInt->fLocked  = 0;
    *pSpinlock = pSpinlockInt;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock)
{
    /*
     * Validate input.
     */
    RT_ASSERT_INTS_ON();
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    if (!pSpinlockInt)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
                    ("Invalid spinlock %p magic=%#x\n", pSpinlockInt, pSpinlockInt->u32Magic),
                    VERR_INVALID_PARAMETER);

    /*
     * Make the lock invalid and release the memory.
     */
    ASMAtomicIncU32(&pSpinlockInt->u32Magic);
    RTMemFree(pSpinlockInt);
    return VINF_SUCCESS;
}


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    RT_ASSERT_PREEMPT_CPUID_VAR();

    for (;;)
    {
        pTmp->uFlags = ASMGetFlags();
        ASMIntDisable();
        critical_enter();

        for (int c = 50; c > 0; c--)
            if (ASMAtomicCmpXchgU32(&pSpinlockInt->fLocked, 1, 0))
            {
                RT_ASSERT_PREEMPT_CPUID_SPIN_ACQUIRED(pSpinlockInt);
                return;
            }
            else
                cpu_spinwait();

        /* Enable interrupts while we sleep. */
        ASMSetFlags(pTmp->uFlags);
        critical_exit();
        DELAY(1);
    }
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE_VARS();

    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE(pSpinlockInt);
    NOREF(pTmp);

    if (!ASMAtomicCmpXchgU32(&pSpinlockInt->fLocked, 0, 1))
        AssertMsgFailed(("Spinlock %p was not locked!\n", pSpinlockInt));
    ASMSetFlags(pTmp->uFlags);

    critical_exit();
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_VAR();
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    NOREF(pTmp);

    for (;;)
    {
        critical_enter();
        for (int c = 50; c > 0; c--)
            if (ASMAtomicCmpXchgU32(&pSpinlockInt->fLocked, 1, 0))
            {
                RT_ASSERT_PREEMPT_CPUID_SPIN_ACQUIRED(pSpinlockInt);
                return;
            }
            else
                cpu_spinwait();

        critical_exit();
        DELAY(1);
    }
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE_VARS();

    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    RT_ASSERT_PREEMPT_CPUID_SPIN_RELEASE(pSpinlockInt);

    NOREF(pTmp);

    if (!ASMAtomicCmpXchgU32(&pSpinlockInt->fLocked, 0, 1))
        AssertMsgFailed(("Spinlock %p was not locked!\n", pSpinlockInt));

    critical_exit();
}

