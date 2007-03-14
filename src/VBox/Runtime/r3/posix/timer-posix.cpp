/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Timer, POSIX.
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
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/err.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#ifdef __LINUX__
# include <linux/rtc.h>
#endif
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#ifndef __OS2__
# include <pthread.h>
#endif


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
     * is destroyed to indicate clearly that thread should exit. */
    volatile uint32_t       u32Magic;
    /** Win32 timer id. */
    RTTHREAD                Thread;
    /** User argument. */
    void                   *pvUser;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** The timeout values for the timer. */
    struct itimerval TimerVal;
    /** The error/status of the timer.
     * Initially -1, set to 0 when the timer have been successfully started, and
     * to errno on failure in starting the timer. */
    volatile int            iError;

} RTTIMER;
/** Timer handle magic. */
#define RTTIMER_MAGIC       0x42424242


/**
 * Signal handler which ignore everything it gets.
 *
 * @param   iSignal     The signal number.
 */
static void rttimerSignalIgnore(int iSignal)
{
    //AssertBreakpoint();
}


/**
 * SIGALRM wait thread.
 */
static DECLCALLBACK(int) rttimerThread(RTTHREAD Thread, void *pvArg)
{
    PRTTIMER pTimer = (PRTTIMER)(void *)pvArg;
    RTTIMER Timer = *pTimer;
    Assert(pTimer->u32Magic == RTTIMER_MAGIC);

    /*
     * Install signal handler.
     */
    struct sigaction SigAct;
    memset(&SigAct, 0, sizeof(SigAct));
    SigAct.sa_flags = SA_RESTART;
    sigemptyset(&SigAct.sa_mask);
    SigAct.sa_handler = rttimerSignalIgnore;
    if (sigaction(SIGALRM, &SigAct, NULL))
    {
        SigAct.sa_flags &= ~SA_RESTART;
        if (sigaction(SIGALRM, &SigAct, NULL))
            AssertMsgFailed(("sigaction failed, errno=%d\n", errno));
    }

    /*
     * Mask most signals except those which might be used during
     * termination is by a pthread implementation.
     */
    sigset_t SigSet;
    sigfillset(&SigSet);
    sigdelset(&SigSet, SIGTERM);
    sigdelset(&SigSet, SIGHUP);
    sigdelset(&SigSet, SIGINT);
    sigdelset(&SigSet, SIGABRT);
    sigdelset(&SigSet, SIGKILL);
#ifdef SIGRTMIN
    for (int iSig = SIGRTMIN; iSig < SIGRTMAX; iSig++)
        sigdelset(&SigSet, iSig);
#endif
    if (sigprocmask(SIG_SETMASK, &SigSet, NULL))
    {
        int rc = pTimer->iError = RTErrConvertFromErrno(errno);
        AssertMsgFailed(("sigprocmask -> errno=%d\n", errno));
        return rc;
    }

    /*
     * Start the timer.
     *
     * For some SunOS (/SysV?) threading compatibility Linux will only
     * deliver the SIGALRM to the thread calling setitimer(). Therefore
     * we have to call it here.
     *
     * It turns out this might not always be the case, see SIGALRM killing
     * processes on RH 2.4.21.
     */
    if (setitimer(ITIMER_REAL, &pTimer->TimerVal, NULL))
    {
        pTimer->iError = RTErrConvertFromErrno(errno);
        RTThreadUserSignal(Thread);
        return errno;
    }

    /*
     * Signal wait loop-forever.
     */
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGALRM);
    RTThreadUserSignal(Thread);
    while (pTimer->u32Magic == RTTIMER_MAGIC)
    {
        siginfo_t SigInfo = {0};
#ifdef __DARWIN__
        if (sigwait(&SigSet, &SigInfo.si_signo) >= 0)
        {
#else
        if (sigwaitinfo(&SigSet, &SigInfo) >= 0)
        {
            if (    SigInfo.si_signo == SIGALRM
                &&  pTimer->u32Magic == RTTIMER_MAGIC)
#endif
                pTimer->pfnTimer(pTimer, pTimer->pvUser);
        }
        else if (errno != EINTR)
            AssertMsgFailed(("sigwaitinfo -> errno=%d\n", errno));
    }

    /*
     * Disable the timer.
     */
    struct itimerval TimerVal = {{0,0}, {0,0}};
    if (setitimer(ITIMER_REAL, &TimerVal, NULL))
        AssertMsgFailed(("setitimer(ITIMER_REAL,&{0}, NULL) failed, errno=%d\n", errno));

    /*
     * Exit.
     */
    RTThreadUserSignal(Thread);
    return VINF_SUCCESS;
}


/**
 * Create a recurring timer.
 *
 * @returns iprt status code.
 * @param   ppTimer             Where to store the timer handle.
 * @param   uMilliesInterval    Milliseconds between the timer ticks.
 *                              This is rounded up to the system granularity.
 * @param   pfnCallback         Callback function which shall be scheduled for execution
 *                              on every timer tick.
 * @param   pvUser              User argument for the callback.
 */
RTR3DECL(int)     RTTimerCreate(PRTTIMER *ppTimer, unsigned uMilliesInterval, PFNRTTIMER pfnTimer, void *pvUser)
{
    /*
     * Check if timer is busy.
     */
    struct itimerval TimerVal;
    if (getitimer(ITIMER_REAL, &TimerVal))
    {
        AssertMsgFailed(("getitimer() -> errno=%d\n", errno));
        return VERR_NOT_IMPLEMENTED;
    }
    if (    TimerVal.it_value.tv_usec || TimerVal.it_value.tv_sec
        ||  TimerVal.it_interval.tv_usec || TimerVal.it_interval.tv_sec
        )
    {
        AssertMsgFailed(("A timer is running. System limit is one timer per process!\n"));
        return VERR_TIMER_BUSY;
    }

    /*
     * Block SIGALRM from calling thread.
     */
#if defined(__FREEBSD__) /* sighold is missing and I don't wish to break anything atm. */
    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGALRM);
    sigprocmask(SIG_BLOCK, &SigSet, NULL);
#else     
    sighold(SIGALRM);
#endif    
    static bool fDoneRTC;
    if (!fDoneRTC)
    {
        fDoneRTC = true;
        /* check resolution. */
        TimerVal.it_interval.tv_sec = 0;
        TimerVal.it_interval.tv_usec = 1000;
        TimerVal.it_value = TimerVal.it_interval;
        if (    setitimer(ITIMER_REAL, &TimerVal, NULL)
            ||  getitimer(ITIMER_REAL, &TimerVal)
            ||  TimerVal.it_interval.tv_usec > 1000)
        {
            /*
             * Try open /dev/rtc to set the irq rate to 1024 and
             * turn periodic
             */
            Log(("RTTimerCreate: interval={%ld,%ld} trying to adjust /dev/rtc!\n", TimerVal.it_interval.tv_sec, TimerVal.it_interval.tv_usec));
#ifdef __LINUX__
            int fh = open("/dev/rtc", O_RDONLY);
            if (fh >= 0)
            {
                if (    ioctl(fh, RTC_IRQP_SET, 1024) < 0
                    ||  ioctl(fh, RTC_PIE_ON, 0) < 0)
                    Log(("RTTimerCreate: couldn't configure rtc! errno=%d\n", errno));
                ioctl(fh, F_SETFL, O_ASYNC);
                ioctl(fh, F_SETOWN, getpid());
                /* not so sure if closing it is a good idea... */
                //close(fh);
            }
            else
                Log(("RTTimerCreate: couldn't configure rtc! open failed with errno=%d\n", errno));
#endif
        }
        /* disable it */
        TimerVal.it_interval.tv_sec = 0;
        TimerVal.it_interval.tv_usec = 0;
        TimerVal.it_value = TimerVal.it_interval;
        setitimer(ITIMER_REAL, &TimerVal, NULL);
    }

    /*
     * Create new timer.
     */
    int rc;
    PRTTIMER pTimer = (PRTTIMER)RTMemAlloc(sizeof(*pTimer));
    if (pTimer)
    {
        pTimer->u32Magic    = RTTIMER_MAGIC;
        pTimer->iError      = 0;
        pTimer->pvUser      = pvUser;
        pTimer->pfnTimer    = pfnTimer;
        pTimer->TimerVal.it_interval.tv_sec = uMilliesInterval / 1000;
        pTimer->TimerVal.it_interval.tv_usec = (uMilliesInterval % 1000) * 1000;
        pTimer->TimerVal.it_value = pTimer->TimerVal.it_interval;
        rc = RTThreadCreate(&pTimer->Thread, rttimerThread, pTimer, 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "Timer");
        if (RT_SUCCESS(rc))
        {
            /*
             * Wait for the timer to successfully create the timer
             */
            /** @todo something is may cause this to take very long. We're waiting 30 seconds now and hope that'll workaround it... */
            rc = RTThreadUserWait(pTimer->Thread, 30*1000);
            if (RT_SUCCESS(rc))
            {
                rc = pTimer->iError;
                if (RT_SUCCESS(rc))
                {
                    RTThreadYield(); /* Horrible hack to make tstTimer work. Something is really fucked related to scheduling here! (2.6.12) */
                    *ppTimer = pTimer;
                    return VINF_SUCCESS;
                }
            }
            ASMAtomicXchgU32(&pTimer->u32Magic, RTTIMER_MAGIC + 1);
        }

        AssertMsgFailed(("Failed to create timer uMilliesInterval=%d. rc=%Vrc\n", uMilliesInterval, rc));
        RTMemFree(pTimer);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    /// @todo implement
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Stops and destroys a running timer.
 *
 * @returns iprt status code.
 * @param   pTimer      Timer to stop and destroy.
 */
RTR3DECL(int)     RTTimerDestroy(PRTTIMER pTimer)
{
    LogFlow(("RTTimerDestroy: pTimer=%p\n", pTimer));

    /* NULL is ok. */
    if (!pTimer)
        return VINF_SUCCESS;

    /*
     * Validate input.
     */
    int rc = VINF_SUCCESS;
    if (VALID_PTR(pTimer))
    {
        /*
         * Modify the magic and kick it.
         */
        if (ASMAtomicXchgU32(&pTimer->u32Magic, RTTIMER_MAGIC + 1) == RTTIMER_MAGIC)
        {
#ifndef __OS2__
            pthread_kill((pthread_t)RTThreadGetNative(pTimer->Thread), SIGALRM);
#endif

            /*
             * Wait for the thread to exit.
             */
            rc = RTThreadWait(pTimer->Thread, 30 * 1000, NULL);
            if (    RT_SUCCESS(rc)
                ||  rc == VERR_INVALID_HANDLE /* we don't keep handles around, you gotta wait before it really exits! */)
            {
                RTMemFree(pTimer);
                return VINF_SUCCESS;
            }
            AssertMsgFailed(("Failed to destroy timer %p. rc=%Vrc\n", pTimer, rc));
        }
        else
        {
            AssertMsgFailed(("Timer %p is already being destroyed!\n", pTimer));
            rc = VERR_INVALID_MAGIC;
        }
    }
    else
    {
        AssertMsgFailed(("Bad pTimer pointer %p!\n", pTimer));
        rc = VERR_INVALID_HANDLE;
    }
    return rc;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    /// @todo implement
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    /// @todo implement
    return VERR_NOT_IMPLEMENTED;
}
