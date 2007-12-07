/* $Id$ */
/** @file
 * innotek Portable Runtime - Threads, common routines.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP RTLOGGROUP_THREAD
#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/avl.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#ifdef IN_RING0
# include <iprt/spinlock.h>
#endif
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/thread.h"
#include "internal/sched.h"
#include "internal/process.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef IN_RING0
# define RT_THREAD_LOCK_TMP(Tmp)    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER
# define RT_THREAD_LOCK_RW(Tmp)     RTSpinlockAcquireNoInts(g_ThreadSpinlock, &(Tmp))
# define RT_THREAD_UNLOCK_RW(Tmp)   RTSpinlockReleaseNoInts(g_ThreadSpinlock, &(Tmp))
# define RT_THREAD_LOCK_RD(Tmp)     RTSpinlockAcquireNoInts(g_ThreadSpinlock, &(Tmp))
# define RT_THREAD_UNLOCK_RD(Tmp)   RTSpinlockReleaseNoInts(g_ThreadSpinlock, &(Tmp))
#else
# define RT_THREAD_LOCK_TMP(Tmp)
# define RT_THREAD_LOCK_RW(Tmp)     rtThreadLockRW()
# define RT_THREAD_UNLOCK_RW(Tmp)   rtThreadUnLockRW()
# define RT_THREAD_LOCK_RD(Tmp)     rtThreadLockRD()
# define RT_THREAD_UNLOCK_RD(Tmp)   rtThreadUnLockRD()
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The AVL thread containing the threads. */
static PAVLPVNODECORE   g_ThreadTree;
#ifdef IN_RING3
/** The RW lock protecting the tree. */
static RTSEMRW          g_ThreadRWSem = NIL_RTSEMRW;
#else
/** The spinlocks protecting the tree. */
static RTSPINLOCK       g_ThreadSpinlock = NIL_RTSPINLOCK;
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtThreadDestroy(PRTTHREADINT pThread);
static int rtThreadAdopt(RTTHREADTYPE enmType, unsigned fFlags, const char *pszName);
static void rtThreadRemoveLocked(PRTTHREADINT pThread);
static PRTTHREADINT rtThreadAlloc(RTTHREADTYPE enmType, unsigned fFlags, unsigned fIntFlags, const char *pszName);


/** @page pg_rt_thread  IPRT Thread Internals
 *
 * IPRT provides interface to whatever native threading that the host provides,
 * preferably using a CRT level interface to better integrate with other libraries.
 *
 * Internally IPRT keeps track of threads by means of the RTTHREADINT structure.
 * All the RTTHREADINT structures are kept in a AVL tree which is protected by a
 * read/write lock for efficient access. A thread is inserted into the tree in
 * three places in the code. The main thread is 'adopted' by IPRT on RTR3Init()
 * by rtThreadAdopt(). When creating a new thread there the child and the parent
 * race inserting the thread, this is rtThreadMain() and RTThreadCreate.
 *
 * RTTHREADINT objects are using reference counting as a mean of sticking around
 * till no-one needs them any longer. Waitable threads is created with one extra
 * reference so they won't go away until they are waited on. This introduces a
 * major problem if we use the host thread identifier as key in the AVL tree - the
 * host may reuse the thread identifier before the thread was waited on. So, on
 * most platforms we are using the RTTHREADINT pointer as key and not the
 * thread id. RTThreadSelf() then have to be implemented using a pointer stored
 * in thread local storage (TLS).
 *
 * In Ring-0 we only try keep track of kernel threads created by RTCreateThread
 * at the moment. There we really only need the 'join' feature, but doing things
 * the same way allow us to name threads and similar stuff.
 */


/**
 * Initializes the thread database.
 *
 * @returns iprt status code.
 */
int rtThreadInit(void)
{
#ifdef IN_RING3
    int rc = VINF_ALREADY_INITIALIZED;
    if (g_ThreadRWSem == NIL_RTSEMRW)
    {
        /*
         * We assume the caller is the 1st thread, which we'll call 'main'.
         * But first, we'll create the semaphore.
         */
        int rc = RTSemRWCreate(&g_ThreadRWSem);
        if (RT_SUCCESS(rc))
        {
            rc = rtThreadNativeInit();
#ifdef IN_RING3
            if (RT_SUCCESS(rc))
                rc = rtThreadAdopt(RTTHREADTYPE_DEFAULT, 0, "main");
            if (RT_SUCCESS(rc))
                rc = rtSchedNativeCalcDefaultPriority(RTTHREADTYPE_DEFAULT);
#endif
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;

            /* failed, clear out */
            RTSemRWDestroy(g_ThreadRWSem);
            g_ThreadRWSem = NIL_RTSEMRW;
        }
    }

#elif defined(IN_RING0)

    /*
     * Create the spinlock and to native init.
     */
    Assert(g_ThreadSpinlock == NIL_RTSPINLOCK);
    int rc = RTSpinlockCreate(&g_ThreadSpinlock);
    if (RT_SUCCESS(rc))
    {
        rc = rtThreadNativeInit();
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;

        /* failed, clear out */
        RTSpinlockDestroy(g_ThreadSpinlock);
        g_ThreadSpinlock = NIL_RTSPINLOCK;
    }
#else
# error "!IN_RING0 && !IN_RING3"
#endif
    return rc;
}


/**
 * Terminates the thread database.
 */
void rtThreadTerm(void)
{
#ifdef IN_RING3
    /* we don't cleanup here yet */

#elif defined(IN_RING0)
    /* just destroy the spinlock and assume the thread is fine... */
    RTSpinlockDestroy(g_ThreadSpinlock);
    g_ThreadSpinlock = NIL_RTSPINLOCK;
    if (g_ThreadTree != NULL)
        AssertMsg2("WARNING: g_ThreadTree=%p\n", g_ThreadTree);
#endif
}



#ifdef IN_RING3

inline void rtThreadLockRW(void)
{
    if (g_ThreadRWSem == NIL_RTSEMRW)
        rtThreadInit();
    int rc = RTSemRWRequestWrite(g_ThreadRWSem, RT_INDEFINITE_WAIT);
    AssertReleaseRC(rc);
}


inline void rtThreadLockRD(void)
{
    if (g_ThreadRWSem == NIL_RTSEMRW)
        rtThreadInit();
    int rc = RTSemRWRequestRead(g_ThreadRWSem, RT_INDEFINITE_WAIT);
    AssertReleaseRC(rc);
}


inline void rtThreadUnLockRW(void)
{
    int rc = RTSemRWReleaseWrite(g_ThreadRWSem);
    AssertReleaseRC(rc);
}


inline void rtThreadUnLockRD(void)
{
    int rc = RTSemRWReleaseRead(g_ThreadRWSem);
    AssertReleaseRC(rc);
}

#endif /* IN_RING3 */


/**
 * Adopts the calling thread.
 * No locks are taken or released by this function.
 */
static int rtThreadAdopt(RTTHREADTYPE enmType, unsigned fFlags, const char *pszName)
{
    Assert(!(fFlags & RTTHREADFLAGS_WAITABLE));
    fFlags &= ~RTTHREADFLAGS_WAITABLE;

    /*
     * Allocate and insert the thread.
     */
    int rc = VERR_NO_MEMORY;
    PRTTHREADINT pThread = rtThreadAlloc(enmType, fFlags, RTTHREADINT_FLAGS_ALIEN, pszName);
    if (pThread)
    {
        RTNATIVETHREAD NativeThread = RTThreadNativeSelf();
        rc = rtThreadNativeAdopt(pThread);
        if (RT_SUCCESS(rc))
        {
            rtThreadInsert(pThread, NativeThread);
            pThread->enmState = RTTHREADSTATE_RUNNING;
            rtThreadRelease(pThread);
        }
    }
    return rc;
}


/**
 * Adopts a non-IPRT thread.
 *
 * @returns IPRT status code.
 * @param   enmType         The thread type.
 * @param   fFlags          The thread flags. RTTHREADFLAGS_WAITABLE is not currently allowed.
 * @param   pszName         The thread name. Optional.
 * @param   pThread         Where to store the thread handle. Optional.
 */
RTDECL(int) RTThreadAdopt(RTTHREADTYPE enmType, unsigned fFlags, const char *pszName, PRTTHREAD pThread)
{
    AssertReturn(!(fFlags & RTTHREADFLAGS_WAITABLE), VERR_INVALID_PARAMETER);
    AssertReturn(!pszName || VALID_PTR(pszName), VERR_INVALID_POINTER);
    AssertReturn(!pThread || VALID_PTR(pThread), VERR_INVALID_POINTER);

    int      rc = VINF_SUCCESS;
    RTTHREAD Thread = RTThreadSelf();
    if (Thread == NIL_RTTHREAD)
    {
        /* generate a name if none was given. */
        char szName[RTTHREAD_NAME_LEN];
        if (!pszName || !*pszName)
        {
            static uint32_t s_i32AlienId = 0;
            uint32_t i32Id = ASMAtomicIncU32(&s_i32AlienId);
            RTStrPrintf(szName, sizeof(szName), "ALIEN-%RX32", i32Id);
            pszName = szName;
        }

        /* try adopt it */
        rc = rtThreadAdopt(enmType, fFlags, pszName);
        Thread = RTThreadSelf();
        Log(("RTThreadAdopt: %RTthrd %RTnthrd '%s' enmType=%d fFlags=%#x rc=%Rrc\n",
             Thread, RTThreadNativeSelf(), pszName, enmType, fFlags, rc));
    }
    else
        Log(("RTThreadAdopt: %RTthrd %RTnthrd '%s' enmType=%d fFlags=%#x - already adopted!\n",
             Thread, RTThreadNativeSelf(), pszName, enmType, fFlags));

    if (pThread)
        *pThread = Thread;
    return rc;
}


/**
 * Allocates a per thread data structure and initializes the basic fields.
 *
 * @returns Pointer to per thread data structure.
 *          This is reference once.
 * @returns NULL on failure.
 * @param   enmType     The thread type.
 * @param   fFlags      The thread flags.
 * @param   fIntFlags   The internal thread flags.
 * @param   pszName     Pointer to the thread name.
 */
PRTTHREADINT rtThreadAlloc(RTTHREADTYPE enmType, unsigned fFlags, unsigned fIntFlags, const char *pszName)
{
    PRTTHREADINT pThread = (PRTTHREADINT)RTMemAllocZ(sizeof(RTTHREADINT));
    if (pThread)
    {
        pThread->Core.Key   = (void*)NIL_RTTHREAD;
        pThread->u32Magic   = RTTHREADINT_MAGIC;
        size_t cchName = strlen(pszName);
        if (cchName >= RTTHREAD_NAME_LEN)
            cchName = RTTHREAD_NAME_LEN - 1;
        memcpy(pThread->szName, pszName, cchName);
        pThread->szName[cchName] = '\0';
        pThread->cRefs      = 2 + !!(fFlags & RTTHREADFLAGS_WAITABLE); /* And extra reference if waitable. */
        pThread->rc         = VERR_PROCESS_RUNNING; /** @todo get a better error code! */
        pThread->enmType    = enmType;
        pThread->fFlags     = fFlags;
        pThread->fIntFlags  = fIntFlags;
        pThread->enmState   = RTTHREADSTATE_INITIALIZING;
        int rc = RTSemEventMultiCreate(&pThread->EventUser);
        if (RT_SUCCESS(rc))
        {
            rc = RTSemEventMultiCreate(&pThread->EventTerminated);
            if (RT_SUCCESS(rc))
                return pThread;
            RTSemEventMultiDestroy(pThread->EventUser);
        }
        RTMemFree(pThread);
    }
    return NULL;
}


/**
 * Insert the per thread data structure into the tree.
 *
 * This can be called from both the thread it self and the parent,
 * thus it must handle insertion failures in a nice manner.
 *
 * @param   pThread         Pointer to thread structure allocated by rtThreadAlloc().
 * @param   NativeThread    The native thread id.
 */
void rtThreadInsert(PRTTHREADINT pThread, RTNATIVETHREAD NativeThread)
{
    Assert(pThread);
    Assert(pThread->u32Magic == RTTHREADINT_MAGIC);

    RT_THREAD_LOCK_TMP(Tmp);
    RT_THREAD_LOCK_RW(Tmp);

    /*
     * Before inserting we must check if there is a thread with this id
     * in the tree already. We're racing parent and child on insert here
     * so that the handle is valid in both ends when they return / start.
     *
     * If it's not ourself we find, it's a dead alien thread and we will
     * unlink it from the tree. Alien threads will be released at this point.
     */
    PRTTHREADINT pThreadOther = (PRTTHREADINT)RTAvlPVGet(&g_ThreadTree, (void *)NativeThread);
    if (pThreadOther != pThread)
    {
        /* remove dead alien if any */
        if (pThreadOther)
        {
            Assert(pThreadOther->fIntFlags & RTTHREADINT_FLAGS_ALIEN);
            ASMAtomicBitClear(&pThread->fIntFlags, RTTHREADINT_FLAG_IN_TREE_BIT);
            rtThreadRemoveLocked(pThreadOther);
            if (pThreadOther->fIntFlags & RTTHREADINT_FLAGS_ALIEN)
                rtThreadRelease(pThreadOther);
        }

        /* insert the thread */
        pThread->Core.Key = (void *)NativeThread;
        bool fRc = RTAvlPVInsert(&g_ThreadTree, &pThread->Core);
        ASMAtomicOrU32(&pThread->fIntFlags, RTTHREADINT_FLAG_IN_TREE);

        AssertReleaseMsg(fRc, ("Lock problem? %p (%RTnthrd) %s\n", pThread, NativeThread, pThread->szName));
        NOREF(fRc);
    }

    RT_THREAD_UNLOCK_RW(Tmp);
}


/**
 * Removes the thread from the AVL tree, call owns the tree lock
 * and has cleared the RTTHREADINT_FLAG_IN_TREE bit.
 *
 * @param   pThread     The thread to remove.
 */
static void rtThreadRemoveLocked(PRTTHREADINT pThread)
{
    PRTTHREADINT pThread2 = (PRTTHREADINT)RTAvlPVRemove(&g_ThreadTree, pThread->Core.Key);
    AssertMsg(pThread2 == pThread, ("%p(%s) != %p (%p/%s)\n", pThread2, pThread2  ? pThread2->szName : "<null>",
                                    pThread, pThread->Core.Key, pThread->szName));
    NOREF(pThread2);
}


/**
 * Removes the thread from the AVL tree.
 *
 * @param   pThread     The thread to remove.
 */
static void rtThreadRemove(PRTTHREADINT pThread)
{
    RT_THREAD_LOCK_TMP(Tmp);
    RT_THREAD_LOCK_RW(Tmp);
    if (ASMAtomicBitTestAndClear(&pThread->fIntFlags, RTTHREADINT_FLAG_IN_TREE_BIT))
        rtThreadRemoveLocked(pThread);
    RT_THREAD_UNLOCK_RW(Tmp);
}


/**
 * Checks if a thread is alive or not.
 *
 * @returns true if the thread is alive (or we don't really know).
 * @returns false if the thread has surely terminate.
 */
DECLINLINE(bool) rtThreadIsAlive(PRTTHREADINT pThread)
{
    return !(pThread->fIntFlags & RTTHREADINT_FLAGS_TERMINATED);
}


/**
 * Gets a thread by it's native ID.
 *
 * @returns pointer to the thread structure.
 * @returns NULL if not a thread IPRT knows.
 * @param   NativeThread    The native thread id.
 */
PRTTHREADINT rtThreadGetByNative(RTNATIVETHREAD NativeThread)
{
    /*
     * Simple tree lookup.
     */
    RT_THREAD_LOCK_TMP(Tmp);
    RT_THREAD_LOCK_RD(Tmp);
    PRTTHREADINT pThread = (PRTTHREADINT)RTAvlPVGet(&g_ThreadTree, (void *)NativeThread);
    RT_THREAD_UNLOCK_RD(Tmp);
    return pThread;
}


/**
 * Gets the per thread data structure for a thread handle.
 *
 * @returns Pointer to the per thread data structure for Thread.
 *          The caller must release the thread using rtThreadRelease().
 * @returns NULL if Thread was not found.
 * @param   Thread      Thread id which structure is to be returned.
 */
PRTTHREADINT rtThreadGet(RTTHREAD Thread)
{
    if (    Thread != NIL_RTTHREAD
        &&  VALID_PTR(Thread))
    {
        PRTTHREADINT pThread = (PRTTHREADINT)Thread;
        if (    pThread->u32Magic == RTTHREADINT_MAGIC
            &&  pThread->cRefs > 0)
        {
            ASMAtomicIncU32(&pThread->cRefs);
            return pThread;
        }
    }

    AssertMsgFailed(("Thread=%RTthrd\n", Thread));
    return NULL;
}


/**
 * Release a per thread data structure.
 *
 * @returns New reference count.
 * @param   pThread     The thread structure to release.
 */
uint32_t rtThreadRelease(PRTTHREADINT pThread)
{
    Assert(pThread);
    uint32_t cRefs;
    if (pThread->cRefs >= 1)
    {
        cRefs = ASMAtomicDecU32(&pThread->cRefs);
        if (!cRefs)
            rtThreadDestroy(pThread);
    }
    else
        cRefs = 0;
    return cRefs;
}


/**
 * Destroys the per thread data.
 *
 * @param   pThread     The thread to destroy.
 */
static void rtThreadDestroy(PRTTHREADINT pThread)
{
    /*
     * Mark it dead and remove it from the tree.
     */
    ASMAtomicXchgU32(&pThread->u32Magic, RTTHREADINT_MAGIC_DEAD);
    rtThreadRemove(pThread);

    /*
     * Free resources.
     */
    pThread->Core.Key   = (void *)NIL_RTTHREAD;
    pThread->enmType    = RTTHREADTYPE_INVALID;
    RTSemEventMultiDestroy(pThread->EventUser);
    pThread->EventUser  = NIL_RTSEMEVENTMULTI;
    if (pThread->EventTerminated != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(pThread->EventTerminated);
        pThread->EventTerminated = NIL_RTSEMEVENTMULTI;
    }
    RTMemFree(pThread);
}


/**
 * Terminates the thread.
 * Called by the thread wrapper function when the thread terminates.
 *
 * @param   pThread     The thread structure.
 * @param   rc          The thread result code.
 */
void rtThreadTerminate(PRTTHREADINT pThread, int rc)
{
    Assert(pThread->cRefs >= 1);

    /*
     * Set the rc, mark it terminated and signal anyone waiting.
     */
    pThread->rc = rc;
    ASMAtomicXchgSize(&pThread->enmState, RTTHREADSTATE_TERMINATED);
    ASMAtomicOrU32(&pThread->fIntFlags, RTTHREADINT_FLAGS_TERMINATED);
    if (pThread->EventTerminated != NIL_RTSEMEVENTMULTI)
        RTSemEventMultiSignal(pThread->EventTerminated);

    /*
     * Remove the thread from the tree so that there will be no
     * key clashes in the AVL tree and release our reference to ourself.
     */
    rtThreadRemove(pThread);
    rtThreadRelease(pThread);
}


/**
 * The common thread main function.
 * This is called by rtThreadNativeMain().
 *
 * @returns The status code of the thread.
 *          pThread is dereference by the thread before returning!
 * @param   pThread         The thread structure.
 * @param   NativeThread    The native thread id.
 * @param   pszThreadName   The name of the thread (purely a dummy for backtrace).
 */
int rtThreadMain(PRTTHREADINT pThread, RTNATIVETHREAD NativeThread, const char *pszThreadName)
{
    NOREF(pszThreadName);
    rtThreadInsert(pThread, NativeThread);
    Log(("rtThreadMain: Starting: pThread=%p NativeThread=%RTnthrd Name=%s pfnThread=%p pvUser=%p\n",
         pThread, NativeThread, pThread->szName, pThread->pfnThread, pThread->pvUser));

    /*
     * Change the priority.
     */
    int rc = rtThreadNativeSetPriority(pThread, pThread->enmType);
#ifdef IN_RING3
    AssertMsgRC(rc, ("Failed to set priority of thread %p (%RTnthrd / %s) to enmType=%d enmPriority=%d rc=%Vrc\n",
                     pThread, NativeThread, pThread->szName, pThread->enmType, g_enmProcessPriority, rc));
#else
    AssertMsgRC(rc, ("Failed to set priority of thread %p (%RTnthrd / %s) to enmType=%d rc=%Vrc\n",
                     pThread, NativeThread, pThread->szName, pThread->enmType, rc));
#endif

    /*
     * Call thread function and terminate when it returns.
     */
    pThread->enmState = RTTHREADSTATE_RUNNING;
    rc = pThread->pfnThread(pThread, pThread->pvUser);

    Log(("rtThreadMain: Terminating: rc=%d pThread=%p NativeThread=%RTnthrd Name=%s pfnThread=%p pvUser=%p\n",
         rc, pThread, NativeThread, pThread->szName, pThread->pfnThread, pThread->pvUser));
    rtThreadTerminate(pThread, rc);
    return rc;
}


/**
 * Create a new thread.
 *
 * @returns iprt status code.
 * @param   pThread     Where to store the thread handle to the new thread. (optional)
 * @param   pfnThread   The thread function.
 * @param   pvUser      User argument.
 * @param   cbStack     The size of the stack for the new thread.
 *                      Use 0 for the default stack size.
 * @param   enmType     The thread type. Used for deciding scheduling attributes
 *                      of the thread.
 * @param   fFlags      Flags of the RTTHREADFLAGS type (ORed together).
 * @param   pszName     Thread name.
 */
RTDECL(int) RTThreadCreate(PRTTHREAD pThread, PFNRTTHREAD pfnThread, void *pvUser, size_t cbStack,
                           RTTHREADTYPE enmType, unsigned fFlags, const char *pszName)
{
    LogFlow(("RTThreadCreate: pThread=%p pfnThread=%p pvUser=%p cbStack=%#x enmType=%d fFlags=%#x pszName=%p:{%s}\n",
             pThread, pfnThread, pvUser, cbStack, enmType, fFlags, pszName, pszName));

    /*
     * Validate input.
     */
    if (!VALID_PTR(pThread) && pThread)
    {
        Assert(VALID_PTR(pThread));
        return VERR_INVALID_PARAMETER;
    }
    if (!VALID_PTR(pfnThread))
    {
        Assert(VALID_PTR(pfnThread));
        return VERR_INVALID_PARAMETER;
    }
    if (!pszName || !*pszName || strlen(pszName) >= RTTHREAD_NAME_LEN)
    {
        AssertMsgFailed(("pszName=%s (max len is %d because of logging)\n", pszName, RTTHREAD_NAME_LEN - 1));
        return VERR_INVALID_PARAMETER;
    }
    if (fFlags & ~RTTHREADFLAGS_MASK)
    {
        AssertMsgFailed(("fFlags=%#x\n", fFlags));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate thread argument.
     */
    int             rc;
    PRTTHREADINT    pThreadInt = rtThreadAlloc(enmType, fFlags, 0, pszName);
    if (pThreadInt)
    {
        pThreadInt->pfnThread = pfnThread;
        pThreadInt->pvUser    = pvUser;
        pThreadInt->cbStack   = cbStack;

        RTNATIVETHREAD NativeThread;
        rc = rtThreadNativeCreate(pThreadInt, &NativeThread);
        if (RT_SUCCESS(rc))
        {
            rtThreadInsert(pThreadInt, NativeThread);
            rtThreadRelease(pThreadInt);
            Log(("RTThreadCreate: Created thread %p (%p) %s\n", pThreadInt, NativeThread, pszName));
            if (pThread)
                *pThread = pThreadInt;
            return VINF_SUCCESS;
        }

        pThreadInt->cRefs = 1;
        rtThreadRelease(pThreadInt);
    }
    else
        rc = VERR_NO_TMP_MEMORY;
    LogFlow(("RTThreadCreate: Failed to create thread, rc=%Vrc\n", rc));
    AssertReleaseRC(rc);
    return rc;
}


/**
 * Gets the native thread id of a IPRT thread.
 *
 * @returns The native thread id.
 * @param   Thread      The IPRT thread.
 */
RTDECL(RTNATIVETHREAD) RTThreadGetNative(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (pThread)
    {
        RTNATIVETHREAD NativeThread = (RTNATIVETHREAD)pThread->Core.Key;
        rtThreadRelease(pThread);
        return NativeThread;
    }
    return NIL_RTNATIVETHREAD;
}


/**
 * Gets the IPRT thread of a native thread.
 *
 * @returns The IPRT thread handle
 * @returns NIL_RTTHREAD if not a thread known to IPRT.
 * @param   NativeThread        The native thread handle/id.
 */
RTDECL(RTTHREAD) RTThreadFromNative(RTNATIVETHREAD NativeThread)
{
    PRTTHREADINT pThread = rtThreadGetByNative(NativeThread);
    if (pThread)
        return pThread;
    return NIL_RTTHREAD;
}


/**
 * Gets the name of the current thread thread.
 *
 * @returns Pointer to readonly name string.
 * @returns NULL on failure.
 */
RTDECL(const char *) RTThreadSelfName(void)
{
    RTTHREAD Thread = RTThreadSelf();
    if (Thread != NIL_RTTHREAD)
    {
        PRTTHREADINT pThread = rtThreadGet(Thread);
        if (pThread)
        {
            const char *szName = pThread->szName;
            rtThreadRelease(pThread);
            return szName;
        }
    }
    return NULL;
}


/**
 * Gets the name of a thread.
 *
 * @returns Pointer to readonly name string.
 * @returns NULL on failure.
 * @param   Thread      Thread handle of the thread to query the name of.
 */
RTDECL(const char *) RTThreadGetName(RTTHREAD Thread)
{
    if (Thread == NIL_RTTHREAD)
        return NULL;
    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (pThread)
    {
        const char *szName = pThread->szName;
        rtThreadRelease(pThread);
        return szName;
    }
    return NULL;
}


/**
 * Sets the name of a thread.
 *
 * @returns iprt status code.
 * @param   Thread      Thread handle of the thread to query the name of.
 * @param   pszName     The thread name.
 */
RTDECL(int) RTThreadSetName(RTTHREAD Thread, const char *pszName)
{
    /*
     * Validate input.
     */
    size_t cchName = strlen(pszName);
    if (cchName >= RTTHREAD_NAME_LEN)
    {
        AssertMsgFailed(("pszName=%s is too long, max is %d\n", pszName, RTTHREAD_NAME_LEN - 1));
        return VERR_INVALID_PARAMETER;
    }
    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (!pThread)
        return VERR_INVALID_HANDLE;

    /*
     * Update the name.
     */
    pThread->szName[cchName] = '\0';    /* paranoia */
    memcpy(pThread->szName, pszName, cchName);
    rtThreadRelease(pThread);
    return VINF_SUCCESS;
}


/**
 * Signal the user event.
 *
 * @returns     iprt status code.
 */
RTDECL(int) RTThreadUserSignal(RTTHREAD Thread)
{
    int             rc;
    PRTTHREADINT    pThread = rtThreadGet(Thread);
    if (pThread)
    {
        rc = RTSemEventMultiSignal(pThread->EventUser);
        rtThreadRelease(pThread);
    }
    else
        rc = VERR_INVALID_HANDLE;
    return rc;
}


/**
 * Wait for the user event, resume on interruption.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 */
RTDECL(int) RTThreadUserWait(RTTHREAD Thread, unsigned cMillies)
{
    int             rc;
    PRTTHREADINT    pThread = rtThreadGet(Thread);
    if (pThread)
    {
        rc = RTSemEventMultiWait(pThread->EventUser, cMillies);
        rtThreadRelease(pThread);
    }
    else
        rc = VERR_INVALID_HANDLE;
    return rc;
}


/**
 * Wait for the user event, return on interruption.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 */
RTDECL(int) RTThreadUserWaitNoResume(RTTHREAD Thread, unsigned cMillies)
{
    int             rc;
    PRTTHREADINT    pThread = rtThreadGet(Thread);
    if (pThread)
    {
        rc = RTSemEventMultiWaitNoResume(pThread->EventUser, cMillies);
        rtThreadRelease(pThread);
    }
    else
        rc = VERR_INVALID_HANDLE;
    return rc;
}


/**
 * Reset the user event.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to reset.
 */
RTDECL(int) RTThreadUserReset(RTTHREAD Thread)
{
    int     rc;
    PRTTHREADINT  pThread = rtThreadGet(Thread);
    if (pThread)
    {
        rc = RTSemEventMultiReset(pThread->EventUser);
        rtThreadRelease(pThread);
    }
    else
        rc = VERR_INVALID_HANDLE;
    return rc;
}


/**
 * Wait for the thread to terminate.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 * @param       prc             Where to store the return code of the thread. Optional.
 * @param       fAutoResume     Whether or not to resume the wait on VERR_INTERRUPTED.
 */
static int rtThreadWait(RTTHREAD Thread, unsigned cMillies, int *prc, bool fAutoResume)
{
    int rc = VERR_INVALID_HANDLE;
    if (Thread != NIL_RTTHREAD)
    {
        PRTTHREADINT pThread = rtThreadGet(Thread);
        if (pThread)
        {
            if (pThread->fFlags & RTTHREADFLAGS_WAITABLE)
            {
                if (fAutoResume)
                    rc = RTSemEventMultiWait(pThread->EventTerminated, cMillies);
                else
                    rc = RTSemEventMultiWaitNoResume(pThread->EventTerminated, cMillies);
                if (RT_SUCCESS(rc))
                {
                    if (prc)
                        *prc = pThread->rc;

                    /*
                     * If the thread is marked as waitable, we'll do one additional
                     * release in order to free up the thread structure (see how we
                     * init cRef in rtThreadAlloc()).
                     */
                    if (ASMAtomicBitTestAndClear(&pThread->fFlags, RTTHREADFLAGS_WAITABLE_BIT))
                        rtThreadRelease(pThread);
                }
            }
            else
            {
                rc = VERR_THREAD_NOT_WAITABLE;
                AssertRC(rc);
            }
            rtThreadRelease(pThread);
        }
    }
    return rc;
}


/**
 * Wait for the thread to terminate, resume on interruption.
 *
 * @returns     iprt status code.
 *              Will not return VERR_INTERRUPTED.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 * @param       prc             Where to store the return code of the thread. Optional.
 */
RTDECL(int) RTThreadWait(RTTHREAD Thread, unsigned cMillies, int *prc)
{
    int rc = rtThreadWait(Thread, cMillies, prc, true);
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


/**
 * Wait for the thread to terminate, return on interruption.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 * @param       prc             Where to store the return code of the thread. Optional.
 */
RTDECL(int) RTThreadWaitNoResume(RTTHREAD Thread, unsigned cMillies, int *prc)
{
    return rtThreadWait(Thread, cMillies, prc, false);
}


/**
 * Changes the type of the specified thread.
 *
 * @returns iprt status code.
 * @param   Thread      The thread which type should be changed.
 * @param   enmType     The new thread type.
 */
RTDECL(int) RTThreadSetType(RTTHREAD Thread, RTTHREADTYPE enmType)
{
    /*
     * Validate input.
     */
    int     rc;
    if (    enmType > RTTHREADTYPE_INVALID
        &&  enmType < RTTHREADTYPE_END)
    {
        PRTTHREADINT pThread = rtThreadGet(Thread);
        if (pThread)
        {
            if (rtThreadIsAlive(pThread))
            {
                /*
                 * Do the job.
                 */
                RT_THREAD_LOCK_TMP(Tmp);
                RT_THREAD_LOCK_RW(Tmp);
                rc = rtThreadNativeSetPriority(pThread, enmType);
                if (RT_SUCCESS(rc))
                    ASMAtomicXchgSize(&pThread->enmType, enmType);
                RT_THREAD_UNLOCK_RW(Tmp);
                if (RT_FAILURE(rc))
                    Log(("RTThreadSetType: failed on thread %p (%s), rc=%Vrc!!!\n", Thread, pThread->szName, rc));
            }
            else
                rc = VERR_THREAD_IS_DEAD;
            rtThreadRelease(pThread);
        }
        else
            rc = VERR_INVALID_HANDLE;
    }
    else
    {
        AssertMsgFailed(("enmType=%d\n", enmType));
        rc = VERR_INVALID_PARAMETER;
    }
    return rc;
}


/**
 * Gets the type of the specified thread.
 *
 * @returns The thread type.
 * @returns RTTHREADTYPE_INVALID if the thread handle is invalid.
 * @param   Thread      The thread in question.
 */
RTDECL(RTTHREADTYPE) RTThreadGetType(RTTHREAD Thread)
{
    RTTHREADTYPE enmType = RTTHREADTYPE_INVALID;
    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (pThread)
    {
        enmType = pThread->enmType;
        rtThreadRelease(pThread);
    }
    return enmType;
}


#ifdef IN_RING3

/**
 * Recalculates scheduling attributes for the the default process
 * priority using the specified priority type for the calling thread.
 *
 * The scheduling attributes are targeted at threads and they are protected
 * by the thread read-write semaphore, that's why RTProc is forwarding the
 * operation to RTThread.
 *
 * @returns iprt status code.
 */
int rtThreadDoCalcDefaultPriority(RTTHREADTYPE enmType)
{
    RT_THREAD_LOCK_TMP(Tmp);
    RT_THREAD_LOCK_RW(Tmp);
    int rc = rtSchedNativeCalcDefaultPriority(enmType);
    RT_THREAD_UNLOCK_RW(Tmp);
    return rc;
}


/**
 * Thread enumerator - sets the priority of one thread.
 *
 * @returns 0 to continue.
 * @returns !0 to stop. In our case a VERR_ code.
 * @param   pNode   The thread node.
 * @param   pvUser  The new priority.
 */
static DECLCALLBACK(int) rtThreadSetPriorityOne(PAVLPVNODECORE pNode, void *pvUser)
{
    PRTTHREADINT pThread = (PRTTHREADINT)pNode;
    if (!rtThreadIsAlive(pThread))
        return VINF_SUCCESS;
    int rc = rtThreadNativeSetPriority(pThread, pThread->enmType);
    if (RT_SUCCESS(rc)) /* hide any warnings */
        return VINF_SUCCESS;
    return rc;
}


/**
 * Attempts to alter the priority of the current process.
 *
 * The scheduling attributes are targeted at threads and they are protected
 * by the thread read-write semaphore, that's why RTProc is forwarding the
 * operation to RTThread. This operation also involves updating all thread
 * which is much faster done from RTThread.
 *
 * @returns iprt status code.
 * @param   enmPriority     The new priority.
 */
int rtThreadDoSetProcPriority(RTPROCPRIORITY enmPriority)
{
    LogFlow(("rtThreadDoSetProcPriority: enmPriority=%d\n", enmPriority));

    /*
     * First validate that we're allowed by the OS to use all the
     * scheduling attributes defined by the specified process priority.
     */
    RT_THREAD_LOCK_TMP(Tmp);
    RT_THREAD_LOCK_RW(Tmp);
    int rc = rtProcNativeSetPriority(enmPriority);
    if (RT_SUCCESS(rc))
    {
        /*
         * Update the priority of existing thread.
         */
        rc = RTAvlPVDoWithAll(&g_ThreadTree, true, rtThreadSetPriorityOne, NULL);
        if (RT_SUCCESS(rc))
            ASMAtomicXchgSize(&g_enmProcessPriority, enmPriority);
        else
        {
            /*
             * Failed, restore the priority.
             */
            rtProcNativeSetPriority(g_enmProcessPriority);
            RTAvlPVDoWithAll(&g_ThreadTree, true, rtThreadSetPriorityOne, NULL);
        }
    }
    RT_THREAD_UNLOCK_RW(Tmp);
    LogFlow(("rtThreadDoSetProcPriority: returns %Vrc\n", rc));
    return rc;
}


/**
 * Bitch about a deadlock.
 *
 * @param   pThread     This thread.
 * @param   pCur        The thread we're deadlocking with.
 * @param   enmState    The sleep state.
 * @param   u64Block    The block data. A pointer or handle.
 * @param   pszFile     Where we are gonna block.
 * @param   uLine       Where we are gonna block.
 * @param   uId         Where we are gonna block.
 */
static void rtThreadDeadlock(PRTTHREADINT pThread, PRTTHREADINT pCur, RTTHREADSTATE enmState, uint64_t u64Block,
                             const char *pszFile, unsigned uLine, RTUINTPTR uId)
{
    AssertMsg1(pCur == pThread ? "!!Deadlock detected!!" : "!!Deadlock exists!!", uLine, pszFile, "");

    /*
     * Print the threads and locks involved.
     */
    PRTTHREADINT    apSeenThreads[8] = {0,0,0,0,0,0,0,0};
    unsigned        iSeenThread = 0;
    pCur = pThread;
    for (unsigned iEntry = 0; pCur && iEntry < 256; iEntry++)
    {
        /*
         * Print info on pCur. Determin next while doing so.
         */
        AssertMsg2(" #%d: %RTthrd/%RTnthrd %s: %s(%u) %RTptr\n",
                   iEntry, pCur, pCur->Core.Key, pCur->szName,
                   pCur->pszBlockFile, pCur->uBlockLine, pCur->uBlockId);
        PRTTHREADINT pNext = NULL;
        switch (pCur->enmState)
        {
            case RTTHREADSTATE_CRITSECT:
            {
                PRTCRITSECT pCritSect = pCur->Block.pCritSect;
                if (pCur->enmState != RTTHREADSTATE_CRITSECT)
                {
                    AssertMsg2("Impossible!!!\n");
                    break;
                }
                if (VALID_PTR(pCritSect) && RTCritSectIsInitialized(pCritSect))
                {
                    AssertMsg2("     Waiting on CRITSECT %p: Entered %s(%u) %RTptr\n",
                               pCritSect, pCritSect->Strict.pszEnterFile,
                               pCritSect->Strict.u32EnterLine, pCritSect->Strict.uEnterId);
                    pNext = pCritSect->Strict.ThreadOwner;
                }
                else
                    AssertMsg2("     Waiting on CRITSECT %p: invalid pointer or uninitialized critsect\n", pCritSect);
                break;
            }

            default:
                AssertMsg2(" Impossible!!! enmState=%d\n", pCur->enmState);
                break;
        }

        /*
         * Check for cycle.
         */
        if (iEntry && pCur == pThread)
            break;
        for (unsigned i = 0; i < ELEMENTS(apSeenThreads); i++)
            if (apSeenThreads[i] == pCur)
            {
                AssertMsg2(" Cycle!\n");
                pNext = NULL;
                break;
            }

        /*
         * Advance to the next thread.
         */
        iSeenThread = (iSeenThread + 1) % ELEMENTS(apSeenThreads);
        apSeenThreads[iSeenThread] = pCur;
        pCur = pNext;
    }
    AssertBreakpoint();
}


/**
 * Change the thread state to blocking and do deadlock detection.
 *
 * This is a RT_STRICT method for debugging locks and detecting deadlocks.
 *
 * @param   pThread     This thread.
 * @param   enmState    The sleep state.
 * @param   u64Block    The block data. A pointer or handle.
 * @param   pszFile     Where we are blocking.
 * @param   uLine       Where we are blocking.
 * @param   uId         Where we are blocking.
 */
void rtThreadBlocking(PRTTHREADINT pThread, RTTHREADSTATE enmState, uint64_t u64Block,
                     const char *pszFile, unsigned uLine, RTUINTPTR uId)
{
    Assert(RTTHREAD_IS_SLEEPING(enmState));
    if (pThread && pThread->enmState == RTTHREADSTATE_RUNNING)
    {
        /** @todo This has to be serialized! The deadlock detection isn't 100% safe!!! */
        pThread->Block.u64      = u64Block;
        pThread->pszBlockFile   = pszFile;
        pThread->uBlockLine     = uLine;
        pThread->uBlockId       = uId;
        ASMAtomicXchgSize(&pThread->enmState, enmState);

        /*
         * Do deadlock detection.
         *
         * Since we're missing proper serialization, we don't declare it a
         * deadlock until we've got three runs with the same list length.
         * While this isn't perfect, it should avoid out the most obvious
         * races on SMP boxes.
         */
        PRTTHREADINT    pCur;
        unsigned        cPrevLength = ~0U;
        unsigned        cEqualRuns = 0;
        unsigned        iParanoia = 256;
        do
        {
            unsigned cLength = 0;
            pCur = pThread;
            for (;;)
            {
                /*
                 * Get the next thread.
                 */
                for (;;)
                {
                    switch (pCur->enmState)
                    {
                        case RTTHREADSTATE_CRITSECT:
                        {
                            PRTCRITSECT pCritSect = pCur->Block.pCritSect;
                            if (pCur->enmState != RTTHREADSTATE_CRITSECT)
                                continue;
                            pCur = pCritSect->Strict.ThreadOwner;
                            break;
                        }

                        default:
                            pCur = NULL;
                            break;
                    }
                    break;
                }
                if (!pCur)
                    return;

                /*
                 * If we've got back to the blocking thread id we've got a deadlock.
                 * If we've got a chain of more than 256 items, there is some kind of cycle
                 * in the list, which means that there is already a deadlock somewhere.
                 */
                if (pCur == pThread || cLength >= 256)
                    break;
                cLength++;
            }

            /* compare with previous list run. */
            if (cLength != cPrevLength)
            {
                cPrevLength = cLength;
                cEqualRuns = 0;
            }
            else
                cEqualRuns++;
        } while (cEqualRuns < 3 && --iParanoia > 0);

        /*
         * Ok, if we ever get here, it's most likely a genuine deadlock.
         */
        rtThreadDeadlock(pThread, pCur, enmState, u64Block, pszFile, uLine, uId);
    }
}


/**
 * Unblocks a thread.
 *
 * This function is paired with rtThreadBlocking.
 *
 * @param   pThread     The current thread.
 * @param   enmCurState The current state, used to check for nested blocking.
 *                      The new state will be running.
 */
void rtThreadUnblocked(PRTTHREADINT pThread, RTTHREADSTATE enmCurState)
{
    if (pThread && pThread->enmState == enmCurState)
        ASMAtomicXchgSize(&pThread->enmState, RTTHREADSTATE_RUNNING);
}

#endif /* IN_RING3 */

