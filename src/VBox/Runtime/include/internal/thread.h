/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Internal RTThread header.
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

#ifndef __thread_h__
#define __thread_h__

#include <iprt/types.h>
#include <iprt/thread.h>
#ifdef IN_RING3
# include <iprt/process.h>
# include <iprt/critsect.h>
# include <iprt/avl.h>
#endif

__BEGIN_DECLS


#ifdef IN_RING3


/**
 * The thread state.
 */
typedef enum RTTHREADSTATE
{
    /** The usual invalid 0 value. */
    RTTHREADSTATE_INVALID = 0,
    /** The thread is being initialized. */
    RTTHREADSTATE_INITIALIZING,
    /** The thread has terminated */
    RTTHREADSTATE_TERMINATED,
    /** Probably running. */
    RTTHREADSTATE_RUNNING,
    /** Waiting on a critical section. */
    RTTHREADSTATE_CRITSECT,
    /** Waiting on a mutex. */
    RTTHREADSTATE_MUTEX,
    /** Waiting on a event semaphore. */
    RTTHREADSTATE_EVENT,
    /** Waiting on a event multiple wakeup semaphore. */
    RTTHREADSTATE_EVENTMULTI,
    /** The thread is sleeping. */
    RTTHREADSTATE_SLEEP,
    /** The usual 32-bit size hack. */
    RTTHREADSTATE_32BIT_HACK = 0x7fffffff
} RTTHREADSTATE;


/** Checks if a thread state indicates that the thread is sleeping. */
#define RTTHREAD_IS_SLEEPING(enmState) (    (enmState) == RTTHREADSTATE_CRITSECT \
                                        ||  (enmState) == RTTHREADSTATE_MUTEX \
                                        ||  (enmState) == RTTHREADSTATE_EVENT \
                                        ||  (enmState) == RTTHREADSTATE_EVENTMULTI \
                                        ||  (enmState) == RTTHREADSTATE_SLEEP \
                                       )

/** Max thread name length. */
#define RTTHREAD_NAME_LEN 16

/**
 * Internal represenation of a thread.
 */
typedef struct RTTHREADINT
{
    /** Avl node core - the key is the native thread id. */
    AVLPVNODECORE           Core;
    /** Magic value (RTTHREADINT_MAGIC). */
    uint32_t                u32Magic;
    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** The current thread state. */
    RTTHREADSTATE volatile  enmState;
#ifdef __WIN__
    /** The thread handle.
     * This is not valid until the create function has returned! */
    uintptr_t               hThread;
#endif
    /** The user event semaphore. */
    RTSEMEVENTMULTI         EventUser;
    /** The terminated event semaphore. */
    RTSEMEVENTMULTI         EventTerminated;
    /** The thread type. */
    RTTHREADTYPE            enmType;
    /** The thread creation flags. (RTTHREADFLAGS) */
    unsigned                fFlags;
    /** Internal flags. (RTTHREADINT_FLAGS_ *) */
    uint32_t                fIntFlags;
    /** The result code. */
    int                     rc;
    /** Thread function. */
    PFNRTTHREAD             pfnThread;
    /** Thread function argument. */
    void                   *pvUser;
    /** Actual stack size. */
    size_t                  cbStack;
    /** What we're blocking on. */
    union RTTHREADINTBLOCKID
    {
        uint64_t            u64;
        PRTCRITSECT         pCritSect;
        RTSEMEVENT          Event;
        RTSEMEVENTMULTI     EventMulti;
        RTSEMMUTEX          Mutex;
    } Block;
    /** Where we're blocking. */
    const char volatile    *pszBlockFile;
    /** Where we're blocking. */
    unsigned volatile       uBlockLine;
    /** Where we're blocking. */
    RTUINTPTR volatile      uBlockId;
    /** Thread name. */
    char                    szName[RTTHREAD_NAME_LEN];
} RTTHREADINT, *PRTTHREADINT;

/** RTTHREADINT::u32Magic value. (Gilbert Keith Chesterton) */
#define RTTHREADINT_MAGIC       0x18740529
/** RTTHREADINT::u32Magic value for a dead thread. */
#define RTTHREADINT_MAGIC_DEAD  0x19360614

/** @name RTTHREADINT::fIntFlags Masks and Bits.
 * @{ */
/** Set if the thread is an alien thread.
 * Clear if the thread was created by IPRT. */
#define RTTHREADINT_FLAGS_ALIEN      BIT(0)
/** Set if the thread has terminated.
 * Clear if the thread is running. */
#define RTTHREADINT_FLAGS_TERMINATED BIT(1)
/** This bit is set if the thread is in the AVL tree. */
#define RTTHREADINT_FLAG_IN_TREE_BIT 2
/** @copydoc RTTHREADINT_FLAG_IN_TREE_BIT */
#define RTTHREADINT_FLAG_IN_TREE     BIT(RTTHREADINT_FLAG_IN_TREE_BIT)
/** @} */


/**
 * Initialize the native part of the thread management.
 *
 * Generally a TLS entry will be allocated at this point.
 *
 * @returns iprt status code.
 */
int rtThreadNativeInit(void);

/**
 * Sets the priority of the thread according to the thread type
 * and current process priority.
 *
 * The RTTHREADINT::enmType member has not yet been updated and will be updated by
 * the caller on a successful return.
 *
 * @returns iprt status code.
 * @param   Thread      The thread in question.
 * @param   enmType     The thread type.
 * @remark  Located in sched.
 */
int rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType);

/**
 * Create a native thread.
 * This creates the thread as described in pThreadInt and stores the thread id in *pThread.
 *
 * @returns iprt status code.
 * @param   pThreadInt      The thread data structure for the thread.
 * @param   pNativeThread   Where to store the native thread identifier.
 */
int rtThreadNativeCreate(PRTTHREADINT pThreadInt, PRTNATIVETHREAD pNativeThread);

/**
 * Adopts a thread, this is called immediately after allocating the
 * thread structure.
 *
 * @param   pThread     Pointer to the thread structure.
 */
int rtThreadNativeAdopt(PRTTHREADINT pThread);

#ifdef __WIN__
/**
 * Callback for when a native thread is detaching.
 *
 * It give the Win32/64 backend a chance to terminate alien
 * threads properly.
 */
void rtThreadNativeDetach(void);
#endif

int rtThreadInit(void);
int rtThreadMain(PRTTHREADINT pThread, RTNATIVETHREAD NativeThread);
void rtThreadInsert(PRTTHREADINT pThread, RTNATIVETHREAD NativeThread);
PRTTHREADINT rtThreadGetByNative(RTNATIVETHREAD NativeThread);
PRTTHREADINT rtThreadGet(RTTHREAD Thread);
uint32_t rtThreadRelease(PRTTHREADINT pThread);
int rtThreadDoSetProcPriority(RTPROCPRIORITY enmPriority);
void rtThreadTerminate(PRTTHREADINT pThread, int rc);
void rtThreadBlocking(PRTTHREADINT pThread, RTTHREADSTATE enmState, uint64_t u64Block,
                      const char *pszFile, unsigned uLine, RTUINTPTR uId);
void rtThreadUnblocked(PRTTHREADINT pThread, RTTHREADSTATE enmCurState);


#elif defined(IN_RING0)

/**
 * Argument package for a ring-0 thread.
 */
typedef struct RTR0THREADARGS
{
    /** The thread function. */
    PFNRTTHREAD     pfnThread;
    /** The thread function argument. */
    void           *pvUser;
    /** The thread type. */
    RTTHREADTYPE    enmType;
} RTR0THREADARGS, *PRTR0THREADARGS;



int rtThreadMain(PRTR0THREADARGS pArgs, RTNATIVETHREAD NativeThread);

/**
 * Do the actual thread creation.
 *
 * @returns IPRT status code.
 *          On failure, no thread has been created.
 * @param   pArgs           The argument package.
 * @param   pNativeThread   Where to return the native thread handle.
 */
int rtThreadNativeCreate(PRTR0THREADARGS pArgs, PRTNATIVETHREAD pNativeThread);

/**
 * Do the actual thread priority change.
 *
 * @returns IPRT status code.
 * @param   Thread      The thread which priority should be changed.
 *                      This is currently restricted to the current thread.
 * @param   enmType     The new thread priority type (valid).
 */
int rtThreadNativeSetPriority(RTTHREAD Thread, RTTHREADTYPE enmType);

#endif /* !IN_RING0 */

__END_DECLS

#endif
