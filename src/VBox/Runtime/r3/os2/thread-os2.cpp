/* $Id$ */
/** @file
 * IPRT - Threads, OS/2.
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
#define LOG_GROUP RTLOGGROUP_THREAD
#define INCL_BASE
#include <os2.h>
#undef RT_MAX

#include <errno.h>
#include <process.h>
#include <stdlib.h>
#include <signal.h>
#include <InnoTekLIBC/FastInfoBlocks.h>
#include <InnoTekLIBC/thread.h>

#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include "internal/thread.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to thread local memory which points to the current thread. */
static PRTTHREADINT *g_ppCurThread;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtThreadNativeMain(void *pvArgs);


int rtThreadNativeInit(void)
{
    /*
     * Allocate thread local memory.
     */
    PULONG pul;
    int rc = DosAllocThreadLocalMemory(1, &pul);
    if (rc)
        return VERR_NO_TLS_FOR_SELF;
    g_ppCurThread = (PRTTHREADINT *)(void *)pul;
    return VINF_SUCCESS;
}


int rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    /*
     * Block SIGALRM - required for timer-posix.cpp.
     * This is done to limit harm done by OSes which doesn't do special SIGALRM scheduling.
     * It will not help much if someone creates threads directly using pthread_create. :/
     */
    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGALRM);
    sigprocmask(SIG_BLOCK, &SigSet, NULL);

    *g_ppCurThread = pThread;
    return VINF_SUCCESS;
}


/**
 * Wrapper which unpacks the params and calls thread function.
 */
static void rtThreadNativeMain(void *pvArgs)
{
    /*
     * Block SIGALRM - required for timer-posix.cpp.
     * This is done to limit harm done by OSes which doesn't do special SIGALRM scheduling.
     * It will not help much if someone creates threads directly using pthread_create. :/
     */
    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGALRM);
    sigprocmask(SIG_BLOCK, &SigSet, NULL);

    /*
     * Call common main.
     */
    PRTTHREADINT  pThread = (PRTTHREADINT)pvArgs;
    *g_ppCurThread = pThread;

#ifdef fibGetTidPid
    rtThreadMain(pThread, fibGetTidPid(), &pThread->szName[0]);
#else
    rtThreadMain(pThread, _gettid(), &pThread->szName[0]);
#endif

    *g_ppCurThread = NULL;
    _endthread();
}


int rtThreadNativeCreate(PRTTHREADINT pThread, PRTNATIVETHREAD pNativeThread)
{
    /*
     * Default stack size.
     */
    if (!pThread->cbStack)
        pThread->cbStack = 512*1024;

    /*
     * Create the thread.
     */
    int iThreadId = _beginthread(rtThreadNativeMain, NULL, pThread->cbStack, pThread);
    if (iThreadId > 0)
    {
#ifdef fibGetTidPid
        *pNativeThread = iThreadId | (fibGetPid() << 16);
#else
        *pNativeThread = iThreadId;
#endif
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    PRTTHREADINT pThread = *g_ppCurThread;
    if (pThread)
        return (RTTHREAD)pThread;
    /** @todo import alien threads? */
    return NULL;
}


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
#ifdef fibGetTidPid
    return fibGetTidPid();
#else
    return _gettid();
#endif
}


RTDECL(int)   RTThreadSleep(unsigned cMillies)
{
    LogFlow(("RTThreadSleep: cMillies=%d\n", cMillies));
    DosSleep(cMillies);
    LogFlow(("RTThreadSleep: returning (cMillies=%d)\n", cMillies));
    return VINF_SUCCESS;
}


RTDECL(bool) RTThreadYield(void)
{
    uint64_t u64TS = ASMReadTSC();
    DosSleep(0);
    u64TS = ASMReadTSC() - u64TS;
    bool fRc = u64TS > 1750;
    LogFlow(("RTThreadYield: returning %d (%llu ticks)\n", fRc, u64TS));
    return fRc;
}


RTDECL(uint64_t) RTThreadGetAffinity(void)
{
    union
    {
        uint64_t u64;
        MPAFFINITY mpaff;
    } u;

    int rc = DosQueryThreadAffinity(AFNTY_THREAD, &u.mpaff);
    if (rc)
        u.u64 = 1;
    return u.u64;
}


RTDECL(int) RTThreadSetAffinity(uint64_t u64Mask)
{
    union
    {
        uint64_t u64;
        MPAFFINITY mpaff;
    } u;
    u.u64 = u64Mask;
    int rc = DosSetThreadAffinity(&u.mpaff);
    if (!rc)
        return VINF_SUCCESS;
    return RTErrConvertFromOS2(rc);
}


RTR3DECL(int) RTTlsAlloc(void)
{
    AssertCompile(NIL_RTTLS == -1);
    return __libc_TLSAlloc();
}


RTR3DECL(int) RTTlsAllocEx(PRTTLS piTls, PFNRTTLSDTOR pfnDestructor)
{
    int rc;
    int iTls = __libc_TLSAlloc();
    if (iTls != -1)
    {
        if (    !pfnDestructor
            ||  __libc_TLSDestructor(iTls, (void (*)(void *, int, unsigned))pfnDestructor, 0) != -1)
        {
            *piTls = iTls;
            return VINF_SUCCESS;
        }

        rc = RTErrConvertFromErrno(errno);
        __libc_TLSFree(iTls);
    }
    else
        rc = RTErrConvertFromErrno(errno);

    *piTls = NIL_RTTLS;
    return rc;
}


RTR3DECL(int) RTTlsFree(RTTLS iTls)
{
    if (iTls == NIL_RTTLS)
        return VINF_SUCCESS;
    if (__libc_TLSFree(iTls) != -1)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(void *) RTTlsGet(RTTLS iTls)
{
    return __libc_TLSGet(iTls);
}


RTR3DECL(int) RTTlsGetEx(RTTLS iTls, void **ppvValue)
{
    int rc = VINF_SUCCESS;
    void *pv = __libc_TLSGet(iTls);
    if (RT_UNLIKELY(!pv))
    {
        errno = 0;
        pv = __libc_TLSGet(iTls);
        if (!pv && errno)
            rc = RTErrConvertFromErrno(errno);
    }

    *ppvValue = pv;
    return rc;
}


RTR3DECL(int) RTTlsSet(RTTLS iTls, void *pvValue)
{
    if (__libc_TLSSet(iTls, pvValue) != -1)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}

