/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Memory Allocation, Ring-0 Driver, OS/2.
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
#include "the-os2-kernel.h"

#include <iprt/timer.h>
#include <iprt/time.h>
#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The internal representation of an OS/2 timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed to indicate clearly that thread should exit. */
    uint32_t volatile       u32Magic;
    /** The next timer in the timer list. */
    PRTTIMER                pNext;
    /** Flag indicating the the timer is suspended. */
    uint8_t volatile        fSuspended;
    /** Cleared at the start of timer processing, set when calling pfnTimer.
     * If any timer changes occures while doing the callback this will be used to resume the cycle. */
    bool                    fDone;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** User argument. */
    void                   *pvUser;
    /** The timer interval. 0 if one-shot. */
    uint64_t                u64NanoInterval;
    /** The start of the current run.
     * This is used to calculate when the timer ought to fire the next time. */
    uint64_t volatile       u64StartTS;
    /** The start of the current run.
     * This is used to calculate when the timer ought to fire the next time. */
    uint64_t volatile       u64NextTS;
    /** The current tick number (since u64StartTS). */
    uint64_t volatile       iTick;
} RTTIMER;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Spinlock protecting the timers. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** The timer head. */
static PRTTIMER volatile    g_pTimerHead = NULL;
/** The number of active timers. */
static uint32_t volatile    g_cActiveTimers = 0;
/** The number of active timers. */
static uint32_t volatile    g_cTimers = 0;
/** The change number.
 * This is used to detect list changes during the timer callback loop. */
static uint32_t volatile    g_u32ChangeNo;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
DECLASM(void) rtTimerOs2Tick(void);
DECLASM(int) rtTimerOs2Arm(void);
DECLASM(int) rtTimerOs2Dearm(void);
__END_DECLS



RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    *ppTimer = NULL;

    /*
     * Lazy initialize the spinlock.
     */
    if (g_Spinlock == NIL_RTSPINLOCK)
    {
        RTSPINLOCK Spinlock;
        int rc = RTSpinlockCreate(&Spinlock);
        AssertRCReturn(rc, rc);
        //bool fRc;
        //ASMAtomicCmpXchgSize(&g_Spinlock, Spinlock, NIL_RTSPINLOCK, fRc);
        //if (!fRc)
        if (!ASMAtomicCmpXchgPtr((void * volatile *)&g_Spinlock, Spinlock, NIL_RTSPINLOCK))
            RTSpinlockDestroy(Spinlock);
    }

    /*
     * Allocate and initialize the timer handle.
     */
    PRTTIMER pTimer = (PRTTIMER)RTMemAlloc(sizeof(*pTimer));
    if (!pTimer)
        return VERR_NO_MEMORY;

    pTimer->u32Magic = RTTIMER_MAGIC;
    pTimer->pNext = NULL;
    pTimer->fSuspended = true;
    pTimer->pfnTimer = pfnTimer;
    pTimer->pvUser = pvUser;
    pTimer->u64NanoInterval = u64NanoInterval;
    pTimer->u64StartTS = 0;

    /*
     * Insert the timer into the list (LIFO atm).
     */
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    g_u32ChangeNo++;
    pTimer->pNext = g_pTimerHead;
    g_pTimerHead = pTimer;
    g_cTimers++;
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


/**
 * Validates the timer handle.
 *
 * @returns true if valid, false if invalid.
 * @param   pTimer  The handle.
 */
DECLINLINE(bool) rtTimerIsValid(PRTTIMER pTimer)
{
    AssertReturn(VALID_PTR(pTimer), false);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, false);
    return true;
}


RTDECL(int) RTTimerDestroy(PRTTIMER pTimer)
{
    /* It's ok to pass NULL pointer. */
    if (pTimer == /*NIL_RTTIMER*/ NULL)
        return VINF_SUCCESS;
    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;

    /*
     * Remove it from the list.
     */
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    g_u32ChangeNo++;
    if (g_pTimerHead == pTimer)
        g_pTimerHead = pTimer->pNext;
    else
    {
        PRTTIMER pPrev = g_pTimerHead;
        while (pPrev->pNext != pTimer)
        {
            pPrev = pPrev->pNext;
            if (RT_UNLIKELY(!pPrev))
            {
                RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
                return VERR_INVALID_HANDLE;
            }
        }
        pPrev->pNext = pTimer->pNext;
    }
    Assert(g_cTimers > 0);
    g_cTimers--;
    if (!pTimer->fSuspended)
    {
        Assert(g_cActiveTimers > 0);
        g_cActiveTimers--;
        if (!g_cActiveTimers)
            rtTimerOs2Dearm();
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);

    /*
     * Free the associated resources.
     */
    pTimer->u32Magic++;
    RTMemFree(pTimer);
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;
    if (!pTimer->fSuspended)
        return VERR_TIMER_ACTIVE;

    /*
     * Calc when it should start fireing and give the thread a kick so it get going.
     */
    u64First += RTTimeNanoTS();

    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    g_u32ChangeNo++;
    if (!g_cActiveTimers)
    {
        int rc = rtTimerOs2Arm();
        if (RT_FAILURE(rc))
        {
            RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
            return rc;
        }
    }
    g_cActiveTimers++;
    pTimer->fSuspended = false;
    pTimer->fDone = true;               /* next tick, not current! */
    pTimer->iTick = 0;
    pTimer->u64StartTS = u64First;
    pTimer->u64NextTS = u64First;
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;
    if (pTimer->fSuspended)
        return VERR_TIMER_SUSPENDED;

    /*
     * Suspend the timer.
     */
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    g_u32ChangeNo++;
    pTimer->fSuspended = true;
    Assert(g_cActiveTimers > 0);
    g_cActiveTimers--;
    if (!g_cActiveTimers)
        rtTimerOs2Dearm();
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);

    return VINF_SUCCESS;
}


DECLASM(void) rtTimerOs2Tick(void)
{
    /*
     * Query the current time and then take the lock.
     */
    const uint64_t u64NanoTS = RTTimeNanoTS();

    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);

    /*
     * Clear the fDone flag.
     */
    PRTTIMER pTimer;
    for (pTimer = g_pTimerHead; pTimer; pTimer = pTimer->pNext)
        pTimer->fDone = false;

    /*
     * Walk the timer list and do the callbacks for any active timer.
     */
    uint32_t u32CurChangeNo = g_u32ChangeNo;
    pTimer = g_pTimerHead;
    while (pTimer)
    {
        PRTTIMER pNext = pTimer->pNext;
        if (    !pTimer->fSuspended
            &&  !pTimer->fDone
            &&  pTimer->u64NextTS <= u64NanoTS)
        {
            pTimer->fDone = true;

            /* calculate the next timeout */
            if (!pTimer->u64NanoInterval)
                pTimer->fSuspended = true;
            else
            {
                pTimer->u64NextTS = pTimer->u64StartTS + pTimer->iTick * pTimer->u64NanoInterval;
                if (pTimer->u64NextTS < u64NanoTS)
                    pTimer->u64NextTS = u64NanoTS + RTTimerGetSystemGranularity() / 2;
            }

            /* do the callout */
            PFNRTTIMER  pfnTimer = pTimer->pfnTimer;
            void       *pvUser   = pTimer->pvUser;
            RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
            pfnTimer(pTimer, pvUser);

            RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);

            /* check if anything changed. */
            if (u32CurChangeNo != g_u32ChangeNo)
            {
                u32CurChangeNo = g_u32ChangeNo;
                pNext = g_pTimerHead;
            }
        }

        /* next */
        pTimer = pNext;
    }

    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
}


RTDECL(uint32_t) RTTimerGetSystemGranularity(void)
{
    return 32000000; /* 32ms */
}


RTDECL(int) RTTimerRequestSystemGranularity(uint32_t u32Request, uint32_t *pu32Granted)
{
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTTimerReleaseSystemGranularity(uint32_t u32Granted)
{
    return VERR_NOT_SUPPORTED;
}

