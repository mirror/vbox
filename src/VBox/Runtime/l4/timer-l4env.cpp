/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Timer, L4.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/timer.h>
#include <iprt/time.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/err.h>

#include <l4/sys/ipc.h>
#include <l4/thread/thread.h>

#include <stdio.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The internal representation of a timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed as a sanity check. */
    volatile uint32_t       u32Magic;
//    /** l4env timer thread id. */
//    l4thread_t              Thread;
    /** and L4 system ID of the same */
    l4_threadid_t           ThreadL4;
    /** The timer thread. */
    RTTHREAD                Thread;
    /** User argument. */
    void                   *pvUser;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** The timeout values for the timer. */
    unsigned int            timeout;
    /** The cpu time of the last scheduled timeout */
    uint64_t                next_timeout;
} RTTIMER;
/** Timer handle magic. */
#define RTTIMER_MAGIC       0x42424242

/**
  * This function calculates an L4 timeout structure from
  * a timer value in microseconds.
  */

static inline l4_timeout_t
micros_to_timeout(int mant)
{
    int exp = 15;

    if (mant > 0)
      {
        /* Calculate rather complicated timeout structure */
        while (mant > 255)
          {
            mant >>= 2;
            exp--;
          }
      }
    else mant = 0;     /* We want to call IPC whatever happens in case someone
                            has decided to terminate us. */
    return L4_IPC_TIMEOUT(0, 0, mant, exp, 0, 0);
}


/**
 * This is the thread we use to emulate a timer.  The thread simply loops, doing
 * timed out IPC waits, and executing the timer callback in between if enough time
 * has passed.  If someone sends the "magic" message, we will exit our loop and the
 * thread.
 */
static DECLCALLBACK(int)
rttimerCallback(RTTHREAD, void *pvArg)
{
    PRTTIMER pTimer = (PRTTIMER)(void *)pvArg;
    Assert(pTimer->u32Magic == RTTIMER_MAGIC);
    l4_umword_t rcvd0 = 0, rcvd1 = 0;
    uint64_t time = RTTimeNanoTS() / 1000;
    pTimer->next_timeout = time + pTimer->timeout;
    l4_msgdope_t result;
    l4_threadid_t src;
    do {
        int micros = pTimer->next_timeout - time;
        l4_ipc_wait(&src, L4_IPC_SHORT_MSG, &rcvd0, &rcvd1, micros_to_timeout(micros), &result);  /* in fact, the return value may be
           ignored, since the only possible cases are success (received a message),
           timeout (what we want) and cancelled for some reason (in which case we
           restart). */
        time = RTTimeNanoTS() / 1000;
        if (time >= pTimer->next_timeout) {
            pTimer->pfnTimer(pTimer, pTimer->pvUser);
            while (time > pTimer->next_timeout)
                pTimer->next_timeout += pTimer->timeout;
        } /* I.E. don't try to catch up lost timer "beats" */
    } while (rcvd0 != RTTIMER_MAGIC && rcvd1 != RTTIMER_MAGIC);
    /* And confirm that we received the message by sending back the magic numbers. */
    return RTErrConvertFromL4Errno(l4_ipc_send(src, L4_IPC_SHORT_MSG, RTTIMER_MAGIC, RTTIMER_MAGIC, L4_IPC_NEVER, &result));
}


/**
 * Create a recurring timer.
 *
 * Since l4env does not support such a thing natively, we simulate it by
 * creating a thread which repeatedly calls the timer function and does
 * timed out sleeps in between.  These sleeps are done using timed out
 * L4 IPC receives, and should the thread actually receive a message in the
 * correct format from another thread in the course of that IPC, it will
 * exit.
 *
 * @returns iprt status code.
 * @param   ppTimer             Where to store the timer handle.
 * @param   uMilliesInterval    Milliseconds between the timer ticks.
 *                              This is rounded up to the system granularity.
 * @param   pfnCallback         Callback function which shall be scheduled for execution
 *                              on every timer tick.
 * @param   pvUser              User argument for the callback.
 */
RTR3DECL(int)
RTTimerCreate(PRTTIMER *ppTimer, unsigned uMilliesInterval, PFNRTTIMER pfnTimer, void *pvUser)
{
    /*
     * Create new timer.
     */
    Assert(uMilliesInterval);
    PRTTIMER pTimer = (PRTTIMER)RTMemAlloc(sizeof(*pTimer));
    if (!pTimer)
        return VERR_NO_MEMORY;
    pTimer->u32Magic    = RTTIMER_MAGIC;
    pTimer->pvUser      = pvUser;
    pTimer->pfnTimer    = pfnTimer;
    pTimer->timeout     = uMilliesInterval * 1000;
    int rc = RTThreadCreate(&pTimer->Thread, rttimerCallback, pTimer, 256*1024, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "TIMER");
    if (rc != VINF_SUCCESS) {
        RTMemFree(pTimer);
        AssertMsgFailed(("Failed to create timer uMilliesInterval=%d. rc=%d\n", uMilliesInterval, rc));
        return rc;
    }
    pTimer->ThreadL4 = l4thread_l4_id(RTThreadGetNative(pTimer->Thread));
    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


/**
 * Stops and destroys a running timer.
 *
 * After doing safety checks, this function sends an IPC message to the specified
 * timer thread to tell it to exit gracefully.  If the timer thread does not
 * respond within 30 seconds (15 for IPC send, 15 for a reply), the thread is shot
 * down.
 *
 * @returns iprt status code.
 * @param   pTimer      Timer to stop and destroy.
 */
RTR3DECL(int)
RTTimerDestroy(PRTTIMER pTimer)
{
    if (!pTimer)
        return VINF_SUCCESS;

    if (!pTimer || ASMAtomicXchgU32(&pTimer->u32Magic, RTTIMER_MAGIC + 1) != RTTIMER_MAGIC) {
        if (pTimer) {
            AssertMsgFailed(("Timer %p is already being destroyed!\n", pTimer));
            return VERR_INVALID_MAGIC;
        }
        AssertMsgFailed(("An attempt was made to destroy a NULL timer!\n"));
        return VERR_INVALID_POINTER;
    }
    l4_umword_t rcvd0, rcvd1;
    l4_msgdope_t result;
    int mant = 228, exp = 7;      /* A 15 second timeout in a
                                   * rather complicated notation
                                   */
    int error = l4_ipc_call(pTimer->ThreadL4, L4_IPC_SHORT_MSG, RTTIMER_MAGIC, RTTIMER_MAGIC, L4_IPC_SHORT_MSG,
                            &rcvd0, &rcvd1, L4_IPC_TIMEOUT(mant, exp, mant, exp, 0, 0), &result);
    if (error == L4_IPC_ENOT_EXISTENT) {
        RTThreadWait(pTimer->Thread, 0, NULL);  /* In case it terminated without our noticing */
        return VERR_INVALID_PARAMETER;
    }
    if (!(error == 0 && rcvd0 == rcvd1 && rcvd0 == RTTIMER_MAGIC)) {
        if (error == 0)
            AssertMsgFailed(("Our thread returned invalid magic when we told it to die.\nIt isn't supposed to do that.\n"));
        if (l4thread_shutdown(RTThreadGetNative(pTimer->Thread)) != 0)  /* Now try brute force */
            return VERR_INVALID_PARAMETER;  /* Only fails if the thread ID is invalid */
    }
    int waitresult;
    int rc = RTThreadWait(pTimer->Thread, 30000, &waitresult);
    return rc;
}

