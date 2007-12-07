/* $Id$ */
/** @file
 * Excerpt from kProfileR3.cpp.
 */

/*
 * Copyright (C) 2005-2007 knut st. osmundsen <bird-src-spam@anduin.net>
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
#if defined(RT_OS_WINDOWS)
# include <windows.h>
# include <psapi.h>
# include <malloc.h>
# define IN_RING3
# include <iprt/stdint.h> /* Temporary IPRT convenience */
# if _MSC_VER >= 1400
#  include <intrin.h>
#  define HAVE_INTRIN
# endif

#elif defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
# define KPRF_USE_PTHREAD
# include <pthread.h>
# include <stdint.h>
# define KPRF_USE_MMAN
# include <sys/mman.h>
# include <sys/fcntl.h>
# include <unistd.h>
# include <stdlib.h>
# ifndef O_BINARY
#  define O_BINARY 0
# endif

#elif defined(RT_OS_OS2)
# define INCL_BASE
# include <os2s.h>
# include <stdint.h>
# include <sys/fmutex.h>

#else
# error "not ported to this OS..."
#endif


/*
 * Instantiate the header.
 */
#define KPRF_NAME(Suffix)               KPrf##Suffix
#define KPRF_TYPE(Prefix,Suffix)        Prefix##KPRF##Suffix
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define KPRF_DECL_FUNC(type, name)     extern "C"  __declspec(dllexport) type __cdecl KPRF_NAME(name)
#else
# define KPRF_DECL_FUNC(type, name)     extern "C" type KPRF_NAME(name)
#endif
#if 1
# ifdef __GNUC__
#  define KPRF_ASSERT(expr) do { if (!(expr)) { __asm__ __volatile__("int3\n\tnop\n\t");} } while (0)
# else
#  define KPRF_ASSERT(expr) do { if (!(expr)) { __asm int 3 \
                            } } while (0)
# endif
#else
# define KPRF_ASSERT(expr) do { } while (0)
#endif

#include "prfcore.h.h"



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Mutex lock type. */
#if defined(KPRF_USE_PTHREAD)
typedef pthread_mutex_t     KPRF_TYPE(,MUTEX);
#elif defined(RT_OS_WINDOWS)
typedef CRITICAL_SECTION    KPRF_TYPE(,MUTEX);
#elif defined(RT_OS_OS2)
typedef struct _fmutex      KPRF_TYPE(,MUTEX);
#endif
/** Pointer to a mutex lock. */
typedef KPRF_TYPE(,MUTEX)  *KPRF_TYPE(P,MUTEX);


#if defined(KPRF_USE_PTHREAD)
/** Read/Write lock type. */
typedef pthread_rwlock_t    KPRF_TYPE(,RWLOCK);
#elif defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
/** Read/Write lock state. */
typedef enum KPRF_TYPE(,RWLOCKSTATE)
{
    RWLOCK_STATE_UNINITIALIZED = 0,
    RWLOCK_STATE_SHARED,
    RWLOCK_STATE_LOCKING,
    RWLOCK_STATE_EXCLUSIVE,
    RWLOCK_STATE_32BIT_HACK = 0x7fffffff
} KPRF_TYPE(,RWLOCKSTATE);
/** Update the state. */
#define KPRF_RWLOCK_SETSTATE(pRWLock, enmNewState) \
    kPrfAtomicSet32((volatile uint32_t *)&(pRWLock)->enmState, (uint32_t)(enmNewState))

/** Read/Write lock type. */
typedef struct KPRF_TYPE(,RWLOCK)
{
    /** This mutex serialize the access and updating of the members
     * of this structure. */
    KPRF_TYPE(,MUTEX)       Mutex;
    /** The current number of readers. */
    uint32_t                cReaders;
    /** The number of readers waiting. */
    uint32_t                cReadersWaiting;
    /** The current number of waiting writers. */
    uint32_t                cWritersWaiting;
# if defined(RT_OS_WINDOWS)
    /** The handle of the event object on which the waiting readers block. (manual reset). */
    HANDLE                  hevReaders;
    /** The handle of the event object on which the waiting writers block. (manual reset). */
    HANDLE                  hevWriters;
# elif defined(RT_OS_OS2)
    /** The handle of the event semaphore on which the waiting readers block. */
    HEV                     hevReaders;
    /** The handle of the event semaphore on which the waiting writers block. */
    HEV                     hevWriters;
# endif
    /** The current state of the read-write lock. */
    KPRF_TYPE(,RWLOCKSTATE) enmState;
} KPRF_TYPE(,RWLOCK);
#endif
/** Pointer to a Read/Write lock. */
typedef KPRF_TYPE(,RWLOCK) *KPRF_TYPE(P,RWLOCK);



/**
 * Initializes a mutex.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 * @param   pMutex      The mutex to init.
 */
static int kPrfMutexInit(KPRF_TYPE(P,MUTEX) pMutex)
{
#if defined(KPRF_USE_PTHREAD)
    if (!pthread_mutex_init(pMutex, NULL));
        return 0;
    return -1;

#elif defined(RT_OS_WINDOWS)
    InitializeCriticalSection(pMutex);
    return 0;

#elif defined(RT_OS_OS2)
    if (!_fmutex_create(pMutex, 0))
        return 0;
    return -1;
#endif
}

/**
 * Deletes a mutex.
 *
 * @param   pMutex      The mutex to delete.
 */
static void kPrfMutexDelete(KPRF_TYPE(P,MUTEX) pMutex)
{
#if defined(KPRF_USE_PTHREAD)
    pthread_mutex_destroy(pMutex);

#elif defined(RT_OS_WINDOWS)
    DeleteCriticalSection(pMutex);

#elif defined(RT_OS_OS2)
    _fmutex_close(pMutex);
#endif
}

/**
 * Locks a mutex.
 * @param   pMutex      The mutex lock.
 */
static inline void kPrfMutexAcquire(KPRF_TYPE(P,MUTEX) pMutex)
{
#if defined(RT_OS_WINDOWS)
    EnterCriticalSection(pMutex);

#elif defined(KPRF_USE_PTHREAD)
    pthread_mutex_lock(pMutex);

#elif defined(RT_OS_OS2)
    fmutex_request(pMutex);
#endif
}


/**
 * Unlocks a mutex.
 * @param   pMutex      The mutex lock.
 */
static inline void kPrfMutexRelease(KPRF_TYPE(P,MUTEX) pMutex)
{
#if defined(RT_OS_WINDOWS)
    LeaveCriticalSection(pMutex);

#elif defined(KPRF_USE_PTHREAD)
    pthread_mutex_lock(pMutex);

#elif defined(RT_OS_OS2)
    fmutex_request(pMutex);
#endif
}



/**
 * Initializes a read-write lock.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 * @param   pRWLock     The read-write lock to initialize.
 */
static inline int kPrfRWLockInit(KPRF_TYPE(P,RWLOCK) pRWLock)
{
#if defined(KPRF_USE_PTHREAD)
    if (!pthread_rwlock_init(pRWLock, NULL))
        return 0;
    return -1;

#elif defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (kPrfMutexInit(&pRWLock->Mutex))
        return -1;
    pRWLock->cReaders = 0;
    pRWLock->cReadersWaiting = 0;
    pRWLock->cWritersWaiting = 0;
    pRWLock->enmState = RWLOCK_STATE_SHARED;
# if defined(RT_OS_WINDOWS)
    pRWLock->hevReaders = CreateEvent(NULL, TRUE, TRUE, NULL);
    pRWLock->hevWriters = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (    pRWLock->hevReaders != INVALID_HANDLE_VALUE
        &&  pRWLock->hevWriters != INVALID_HANDLE_VALUE)
        return 0;
    CloseHandle(pRWLock->hevReaders);
    CloseHandle(pRWLock->hevWriters);

# elif defined(RT_OS_OS2)
    APIRET rc = DosCreateEventSem(NULL, &pRWLock->hevReaders, 0, TRUE);
    if (!rc)
    {
        rc = DosCreateEventSem(NULL, &pRWLock->hevWriters, 0, TRUE);
        if (!rc)
            return 0;
        pRWLock->hevWriters = NULLHANDLE;
        DosCloseEventSem(pRWLock->hevReaders);
    }
    pRWLock->hevReaders = NULLHANDLE;
# endif

    pRWLock->enmState = RWLOCK_STATE_UNINITIALIZED;
    kPrfMutexDelete(&pRWLock->Mutex);
    return -1;
#endif
}


/**
 * Deleters a read-write lock.
 *
 * @param   pRWLock     The read-write lock to delete.
 */
static inline void kPrfRWLockDelete(KPRF_TYPE(P,RWLOCK) pRWLock)
{
#if defined(KPRF_USE_PTHREAD)
    pthread_rwlock_destroy(pRWLock);

#elif defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (pRWLock->enmState == RWLOCK_STATE_UNINITIALIZED)
        return;

    pRWLock->enmState = RWLOCK_STATE_UNINITIALIZED;
    kPrfMutexDelete(&pRWLock->Mutex);
    pRWLock->cReaders = 0;
    pRWLock->cReadersWaiting = 0;
    pRWLock->cWritersWaiting = 0;
# if defined(RT_OS_WINDOWS)
    CloseHandle(pRWLock->hevReaders);
    pRWLock->hevReaders = INVALID_HANDLE_VALUE;
    CloseHandle(pRWLock->hevWriters);
    pRWLock->hevWriters = INVALID_HANDLE_VALUE;

# elif defined(RT_OS_OS2)
    DosCloseEventSem(pRWLock->hevReaders);
    pRWLock->hevReaders = NULLHANDLE;
    DosCloseEventSem(pRWLock->hevWriters);
    pRWLock->hevWriters = NULLHANDLE;
# endif
#endif
}


/**
 * Acquires read access to the read-write lock.
 * @param   pRWLock     The read-write lock.
 */
static inline void kPrfRWLockAcquireRead(KPRF_TYPE(P,RWLOCK) pRWLock)
{
#if defined(KPRF_USE_PTHREAD)
    pthread_rwlock_rdlock(pRWLock);

#elif defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (pRWLock->enmState == RWLOCK_STATE_UNINITIALIZED)
        return;

    kPrfMutexAcquire(&pRWLock->Mutex);
    if (pRWLock->enmState == RWLOCK_STATE_SHARED)
    {
        KPRF_ATOMIC_INC32(&pRWLock->cReaders);
        kPrfMutexRelease(&pRWLock->Mutex);
        return;
    }

    for (;;)
    {
        /* have to wait */
        KPRF_ATOMIC_INC32(&pRWLock->cReadersWaiting);
# if defined(RT_OS_WINDOWS)
        HANDLE hev = pRWLock->hevReaders;
        ResetEvent(hev);

# elif defined(RT_OS_OS2)
        HEV    hev = pRWLock->hevReaders;
        ULONG cIgnored;
        DosResetEventSem(hev, &cIgnored);

# endif
        kPrfMutexRelease(&pRWLock->Mutex);

# if defined(RT_OS_WINDOWS)
        switch (WaitForSingleObject(hev, INFINITE))
        {
            case WAIT_IO_COMPLETION:
            case WAIT_TIMEOUT:
            case WAIT_OBJECT_0:
                break;
            case WAIT_ABANDONED:
            default:
                return;
        }

# elif defined(RT_OS_OS2)
        switch (DosWaitEventSem(hev, SEM_INDEFINITE_WAIT))
        {
            case NO_ERROR:
            case ERROR_SEM_TIMEOUT:
            case ERROR_TIMEOUT:
            case ERROR_INTERRUPT:
                break;
            default:
                return;
        }
# endif

        kPrfMutexAcquire(&pRWLock->Mutex);
        if (pRWLock->enmState == RWLOCK_STATE_SHARED)
        {
            KPRF_ATOMIC_INC32(&pRWLock->cReaders);
            KPRF_ATOMIC_DEC32(&pRWLock->cReadersWaiting);
            kPrfMutexRelease(&pRWLock->Mutex);
            return;
        }
    }
#endif
}


/**
 * Releases read access to the read-write lock.
 * @param   pRWLock     The read-write lock.
 */
static inline void kPrfRWLockReleaseRead(KPRF_TYPE(P,RWLOCK) pRWLock)
{
#if defined(KPRF_USE_PTHREAD)
    pthread_rwlock_unlock(pRWLock);

#elif defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (pRWLock->enmState == RWLOCK_STATE_UNINITIALIZED)
        return;

    /*
     * If we're still in the shared state, or if there
     * are more readers out there, or if there are no
     * waiting writers, all we have to do is decrement an leave.
     *
     * That's the most frequent, thing and should be fast.
     */
    kPrfMutexAcquire(&pRWLock->Mutex);
    KPRF_ATOMIC_DEC32(&pRWLock->cReaders);
    if (    pRWLock->enmState == RWLOCK_STATE_SHARED
        ||  pRWLock->cReaders
        ||  !pRWLock->cWritersWaiting)
    {
        kPrfMutexRelease(&pRWLock->Mutex);
        return;
    }

    /*
     * Wake up one (or more on OS/2) waiting writers.
     */
# if defined(RT_OS_WINDOWS)
    SetEvent(pRWLock->hevWriters);
# elif defined(RT_OS_OS2)
    DosPostEvent(pRWLock->hevwriters);
# endif
    kPrfMutexRelease(&pRWLock->Mutex);

#endif
}


/**
 * Acquires write access to the read-write lock.
 * @param   pRWLock     The read-write lock.
 */
static inline void kPrfRWLockAcquireWrite(KPRF_TYPE(P,RWLOCK) pRWLock)
{
#if defined(KPRF_USE_PTHREAD)
    pthread_rwlock_wrlock(pRWLock);

#elif defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (pRWLock->enmState == RWLOCK_STATE_UNINITIALIZED)
        return;

    kPrfMutexAcquire(&pRWLock->Mutex);
    if (    !pRWLock->cReaders
        &&  (   pRWLock->enmState == RWLOCK_STATE_SHARED
             || pRWLock->enmState == RWLOCK_STATE_LOCKING)
        )
    {
        KPRF_RWLOCK_SETSTATE(pRWLock, RWLOCK_STATE_EXCLUSIVE);
        kPrfMutexRelease(&pRWLock->Mutex);
        return;
    }

    /*
     * We'll have to wait.
     */
    if (pRWLock->enmState == RWLOCK_STATE_SHARED)
        KPRF_RWLOCK_SETSTATE(pRWLock, RWLOCK_STATE_LOCKING);
    KPRF_ATOMIC_INC32(&pRWLock->cWritersWaiting);
    for (;;)
    {
# if defined(RT_OS_WINDOWS)
        HANDLE hev = pRWLock->hevWriters;
# elif defined(RT_OS_OS2)
        HEV    hev = pRWLock->hevWriters;
# endif
        kPrfMutexRelease(&pRWLock->Mutex);
# if defined(RT_OS_WINDOWS)
        switch (WaitForSingleObject(hev, INFINITE))
        {
            case WAIT_IO_COMPLETION:
            case WAIT_TIMEOUT:
            case WAIT_OBJECT_0:
                break;
            case WAIT_ABANDONED:
            default:
                KPRF_ATOMIC_DEC32(&pRWLock->cWritersWaiting);
                return;
        }

# elif defined(RT_OS_OS2)
        switch (DosWaitEventSem(hev, SEM_INDEFINITE_WAIT))
        {
            case NO_ERROR:
            case ERROR_SEM_TIMEOUT:
            case ERROR_TIMEOUT:
            case ERROR_INTERRUPT:
                break;
            default:
                KPRF_ATOMIC_DEC32(&pRWLock->cWritersWaiting);
                return;
        }
        ULONG cIgnored;
        DosResetEventSem(hev, &cIgnored);
# endif

        /*
         * Try acquire the lock.
         */
        kPrfMutexAcquire(&pRWLock->Mutex);
        if (    !pRWLock->cReaders
            &&  (   pRWLock->enmState == RWLOCK_STATE_SHARED
                 || pRWLock->enmState == RWLOCK_STATE_LOCKING)
            )
        {
            KPRF_RWLOCK_SETSTATE(pRWLock, RWLOCK_STATE_EXCLUSIVE);
            KPRF_ATOMIC_DEC32(&pRWLock->cWritersWaiting);
            kPrfMutexRelease(&pRWLock->Mutex);
            return;
        }
    }
#endif
}


/**
 * Releases write access to the read-write lock.
 * @param   pRWLock     The read-write lock.
 */
static inline void kPrfRWLockReleaseWrite(KPRF_TYPE(P,RWLOCK) pRWLock)
{
#if defined(KPRF_USE_PTHREAD)
    pthread_rwlock_unlock(pRWLock);

#elif defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (pRWLock->enmState == RWLOCK_STATE_UNINITIALIZED)
        return;

    /*
     * The common thing is that there are noone waiting.
     * But, before that usual paranoia.
     */
    kPrfMutexAcquire(&pRWLock->Mutex);
    if (pRWLock->enmState != RWLOCK_STATE_EXCLUSIVE)
    {
        kPrfMutexRelease(&pRWLock->Mutex);
        return;
    }
    if (    !pRWLock->cReadersWaiting
        &&  !pRWLock->cWritersWaiting)
    {
        KPRF_RWLOCK_SETSTATE(pRWLock, RWLOCK_STATE_SHARED);
        kPrfMutexRelease(&pRWLock->Mutex);
        return;
    }

    /*
     * Someone is waiting, wake them up as we change the state.
     */
# if defined(RT_OS_WINDOWS)
    HANDLE hev = INVALID_HANDLE_VALUE;
# elif defined(RT_OS_OS2)
    HEV    hev = NULLHANDLE;
# endif

    if (pRWLock->cWritersWaiting)
    {
        KPRF_RWLOCK_SETSTATE(pRWLock, RWLOCK_STATE_LOCKING);
        hev = pRWLock->hevWriters;
    }
    else
    {
        KPRF_RWLOCK_SETSTATE(pRWLock, RWLOCK_STATE_SHARED);
        hev = pRWLock->hevReaders;
    }
# if defined(RT_OS_WINDOWS)
    SetEvent(hev);
# elif defined(RT_OS_OS2)
    DosPostEvent(pRWLock->hevwriters);
# endif
    kPrfMutexRelease(&pRWLock->Mutex);

#endif
}

