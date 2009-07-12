/** @file
 * IPRT - Semaphore.
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

#ifndef ___iprt_semaphore_h
#define ___iprt_semaphore_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_sems    RTSem - Semaphores
 * @ingroup grp_rt
 * @{
 */


/** @defgroup grp_rt_sems_event    RTSemEvent - Single Release Event Semaphores
 * @{ */

/**
 * Create a event semaphore.
 *
 * @returns iprt status code.
 * @param   pEventSem    Where to store the event semaphore handle.
 */
RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem);

/**
 * Destroy an event semaphore.
 *
 * @returns iprt status code.
 * @param   EventSem    Handle of the
 */
RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem);

/**
 * Signal an event semaphore.
 *
 * The event semaphore will be signaled and automatically reset
 * after exactly one thread have successfully returned from
 * RTSemEventWait() after waiting/polling on that semaphore.
 *
 * @returns iprt status code.
 * @param   EventSem    The event semaphore to signal.
 */
RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem);

/**
 * Wait for the event semaphore to be signaled, resume on interruption.
 *
 * This function will resume if the wait is interrupted by an async
 * system event (like a unix signal) or similar.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   EventSem    The event semaphore to wait on.
 * @param   cMillies    Number of milliseconds to wait.
 */
RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies);

/**
 * Wait for the event semaphore to be signaled, return on interruption.
 *
 * This function will not resume the wait if interrupted.
 *
 * @returns iprt status code.
 * @param   EventSem    The event semaphore to wait on.
 * @param   cMillies    Number of milliseconds to wait.
 */
RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies);

/** @} */


/** @defgroup grp_rt_sems_event_multi   RTSemEventMulti - Multiple Release Event Semaphores
 * @{ */

/**
 * Create a event multi semaphore.
 *
 * @returns iprt status code.
 * @param   pEventMultiSem  Where to store the event multi semaphore handle.
 */
RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem);

/**
 * Destroy an event multi semaphore.
 *
 * @returns iprt status code.
 * @param   EventMultiSem   The event multi sempahore to destroy.
 */
RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem);

/**
 * Signal an event multi semaphore.
 *
 * @returns iprt status code.
 * @param   EventMultiSem   The event multi semaphore to signal.
 */
RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem);

/**
 * Resets an event multi semaphore to non-signaled state.
 *
 * @returns iprt status code.
 * @param   EventMultiSem   The event multi semaphore to reset.
 */
RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem);

/**
 * Wait for the event multi semaphore to be signaled, resume on interruption.
 *
 * This function will resume if the wait is interrupted by an async
 * system event (like a unix signal) or similar.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   EventMultiSem   The event multi semaphore to wait on.
 * @param   cMillies        Number of milliseconds to wait.
 */
RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies);


/**
 * Wait for the event multi semaphore to be signaled, return on interruption.
 *
 * This function will not resume the wait if interrupted.
 *
 * @returns iprt status code.
 * @param   EventMultiSem   The event multi semaphore to wait on.
 * @param   cMillies        Number of milliseconds to wait.
 */
RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies);

/** @} */


/** @defgroup grp_rt_sems_mutex     RTMutex - Mutex semaphores.
 *
 * @remarks These can be pretty heavy handed. Fast mutexes or critical sections
 *          is usually what you need.
 *
 * @{ */

/**
 * Create a mutex semaphore.
 *
 * @returns iprt status code.
 * @param   pMutexSem   Where to store the mutex semaphore handle.
 */
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem);

/**
 * Destroy a mutex semaphore.
 *
 * @returns iprt status code.
 * @param   MutexSem    The mutex semaphore to destroy.
 */
RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem);

/**
 * Request ownership of a mutex semaphore, resume on interruption.
 *
 * This function will resume if the wait is interrupted by an async
 * system event (like a unix signal) or similar.
 *
 * The same thread may request a mutex semaphore multiple times,
 * a nested counter is kept to make sure it's released on the right
 * RTSemMutexRelease() call.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   MutexSem    The mutex semaphore to request ownership over.
 * @param   cMillies    The number of milliseconds to wait.
 */
RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies);

/**
 * Request ownership of a mutex semaphore, return on interruption.
 *
 * This function will not resume the wait if interrupted.
 *
 * The same thread may request a mutex semaphore multiple times,
 * a nested counter is kept to make sure it's released on the right
 * RTSemMutexRelease() call.
 *
 * @returns iprt status code.
 * @param   MutexSem    The mutex semaphore to request ownership over.
 * @param   cMillies    The number of milliseconds to wait.
 */
RTDECL(int)  RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies);

/**
 * Release the ownership of a mutex semaphore.
 *
 * @returns iprt status code.
 * @param   MutexSem    The mutex to release the ownership of.
 *                      It goes without saying the the calling thread must own it.
 */
RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem);

/** @} */


/** @defgroup grp_rt_sems_fast_mutex    RTSemFastMutex - Fast Mutex Semaphores
 * @{ */

/**
 * Create a fast mutex semaphore.
 *
 * @returns iprt status code.
 * @param   pMutexSem   Where to store the mutex semaphore handle.
 *
 * @remarks Fast mutex semaphores are not recursive.
 */
RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem);

/**
 * Destroy a fast mutex semaphore.
 *
 * @returns iprt status code.
 * @param   MutexSem    The mutex semaphore to destroy.
 */
RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem);

/**
 * Request ownership of a fast mutex semaphore.
 *
 * The same thread may request a mutex semaphore multiple times,
 * a nested counter is kept to make sure it's released on the right
 * RTSemMutexRelease() call.
 *
 * @returns iprt status code.
 * @param   MutexSem    The mutex semaphore to request ownership over.
 */
RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX MutexSem);

/**
 * Release the ownership of a fast mutex semaphore.
 *
 * @returns iprt status code.
 * @param   MutexSem    The mutex to release the ownership of.
 *                      It goes without saying the the calling thread must own it.
 */
RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem);

/** @} */


/** @defgroup grp_rt_sems_spin_mutex RTSemSpinMutex - Spinning Mutex Semaphores
 *
 * Very adaptive kind of mutex semaphore tailored for the ring-0 logger.
 *
 * @{ */

/**
 * Creates a spinning mutex semaphore.
 *
 * @returns iprt status code.
 * @retval  VERR_INVALID_PARAMETER on invalid flags.
 * @retval  VERR_NO_MEMORY if out of memory for the semaphore structure and
 *          handle.
 *
 * @param   phSpinMtx   Where to return the handle to the create semaphore.
 * @param   fFlags      Flags, see RTSEMSPINMUTEX_FLAGS_XXX.
 */
RTDECL(int) RTSemSpinMutexCreate(PRTSEMSPINMUTEX phSpinMtx, uint32_t fFlags);

/** @name RTSemSpinMutexCreate flags.
 * @{ */
/** Always take the semaphore in a IRQ safe way.
 * (In plain words: always disable interrupts.) */
#define RTSEMSPINMUTEX_FLAGS_IRQ_SAFE       RT_BIT_32(0)
/** Mask of valid flags. */
#define RTSEMSPINMUTEX_FLAGS_VALID_MASK     UINT32_C(0x00000001)
/** @} */

/**
 * Destroys a spinning mutex semaphore.
 *
 * @returns iprt status code.
 * @retval  VERR_INVALID_HANDLE (or crash) if the handle is invalid. (NIL will
 *          not cause this status.)
 *
 * @param   hSpinMtx    The semaphore handle. NIL_RTSEMSPINMUTEX is ignored
 *                      quietly (VINF_SUCCESS).
 */
RTDECL(int) RTSemSpinMutexDestroy(RTSEMSPINMUTEX hSpinMtx);

/**
 * Request the spinning mutex semaphore.
 *
 * This may block if the context we're called in allows this. If not it will
 * spin. If called in an interrupt context, we will only spin if the current
 * owner isn't interrupted. Also, on some systems it is not always possible to
 * wake up blocking threads in all contexts, so, which will either be indicated
 * by returning VERR_SEM_BAD_CONTEXT or by temporarily switching the semaphore
 * into pure spinlock state.
 *
 * Preemption will be disabled upon return. IRQs may also be disabled.
 *
 * @returns iprt status code.
 * @retval  VERR_SEM_BAD_CONTEXT if the context it's called in isn't suitable
 *          for releasing it if someone is sleeping on it.
 * @retval  VERR_SEM_DESTROYED if destroyed.
 * @retval  VERR_SEM_NESTED if held by the caller. Asserted.
 * @retval  VERR_INVALID_HANDLE if the handle is invalid. Asserted
 *
 * @param   hSpinMtx    The semaphore handle.
 */
RTDECL(int) RTSemSpinMutexRequest(RTSEMSPINMUTEX hSpinMtx);

/**
 * Like RTSemSpinMutexRequest but it won't block or spin if the semaphore is
 * held by someone else.
 *
 * @returns iprt status code.
 * @retval  VERR_SEM_BUSY if held by someone else.
 * @retval  VERR_SEM_DESTROYED if destroyed.
 * @retval  VERR_SEM_NESTED if held by the caller. Asserted.
 * @retval  VERR_INVALID_HANDLE if the handle is invalid. Asserted
 *
 * @param   hSpinMtx    The semaphore handle.
 */
RTDECL(int) RTSemSpinMutexTryRequest(RTSEMSPINMUTEX hSpinMtx);

/**
 * Releases the semaphore previously acquired by RTSemSpinMutexRequest or
 * RTSemSpinMutexTryRequest.
 *
 * @returns iprt status code.
 * @retval  VERR_SEM_DESTROYED if destroyed.
 * @retval  VERR_NOT_OWNER if not owner. Asserted.
 * @retval  VERR_INVALID_HANDLE if the handle is invalid. Asserted.
 *
 * @param   hSpinMtx    The semaphore handle.
 */
RTDECL(int) RTSemSpinMutexRelease(RTSEMSPINMUTEX hSpinMtx);

/** @} */


/** @defgroup grp_rt_sem_rw             RTSemRW - Read / Write Semaphores
 * @{ */

/**
 * Creates a read/write semaphore.
 *
 * @returns iprt status code.
 * @param   pRWSem      Where to store the handle to the created RW semaphore.
 */
RTDECL(int)   RTSemRWCreate(PRTSEMRW pRWSem);

/**
 * Destroys a read/write semaphore.
 *
 * @returns iprt status code.
 * @param   RWSem       The Read/Write semaphore to destroy.
 */
RTDECL(int)   RTSemRWDestroy(RTSEMRW RWSem);

/**
 * Request read access to a read/write semaphore, resume on interruption
 *
 * @returns iprt status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INTERRUPT if the wait was interrupted.
 * @retval  VERR_INVALID_HANDLE if RWSem is invalid.
 *
 * @param   RWSem       The Read/Write semaphore to request read access to.
 * @param   cMillies    The number of milliseconds to wait.
 */
RTDECL(int)   RTSemRWRequestRead(RTSEMRW RWSem, unsigned cMillies);

/**
 * Request read access to a read/write semaphore, return on interruption
 *
 * @returns iprt status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INTERRUPT if the wait was interrupted.
 * @retval  VERR_INVALID_HANDLE if RWSem is invalid.
 *
 * @param   RWSem       The Read/Write semaphore to request read access to.
 * @param   cMillies    The number of milliseconds to wait.
 */
RTDECL(int)   RTSemRWRequestReadNoResume(RTSEMRW RWSem, unsigned cMillies);

/**
 * Release read access to a read/write semaphore.
 *
 * @returns iprt status code.
 * @param   RWSem       The Read/Write sempahore to release read access to.
 *                      Goes without saying that caller must have read access to the sem.
 */
RTDECL(int)   RTSemRWReleaseRead(RTSEMRW RWSem);

/**
 * Request write access to a read/write semaphore, resume on interruption.
 *
 * @returns iprt status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_DEADLOCK if the caller owned the read lock.
 * @retval  VERR_INVALID_HANDLE if RWSem is invalid.
 *
 * @param   RWSem       The Read/Write semaphore to request write access to.
 * @param   cMillies    The number of milliseconds to wait.
 */
RTDECL(int)   RTSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies);

/**
 * Request write access to a read/write semaphore, return on interruption.
 *
 * @returns iprt status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INTERRUPT if the wait was interrupted.
 * @retval  VERR_DEADLOCK if the caller owned the read lock.
 * @retval  VERR_INVALID_HANDLE if RWSem is invalid.
 *
 * @param   RWSem       The Read/Write semaphore to request write access to.
 * @param   cMillies    The number of milliseconds to wait.
 */
RTDECL(int)   RTSemRWRequestWriteNoResume(RTSEMRW RWSem, unsigned cMillies);

/**
 * Release write access to a read/write semaphore.
 *
 * @returns iprt status code.
 * @param   RWSem       The Read/Write sempahore to release read access to.
 *                      Goes without saying that caller must have write access to the sem.
 */
RTDECL(int)   RTSemRWReleaseWrite(RTSEMRW RWSem);

/**
 * Checks if the caller is the exclusive semaphore owner.
 *
 * @returns true / false accoringly.
 * @param   RWSem       The Read/Write semaphore in question.
 */
RTDECL(bool)  RTSemRWIsWriteOwner(RTSEMRW RWSem);

/**
 * Gets the write recursion count.
 *
 * @returns The write recursion count (0 if bad semaphore handle).
 * @param   RWSem       The Read/Write semaphore in question.
 */
RTDECL(uint32_t) RTSemRWGetWriteRecursion(RTSEMRW RWSem);

/**
 * Gets the read recursion count of the current writer.
 *
 * @returns The read recursion count (0 if bad semaphore handle).
 * @param   RWSem       The Read/Write semaphore in question.
 */
RTDECL(uint32_t) RTSemRWGetWriterReadRecursion(RTSEMRW RWSem);

/** @} */


/** @defgroup grp_rt_sems_pingpong      RTSemPingPong - Ping-Pong Construct
 * @{ */

/**
 * Ping-pong speaker
 */
typedef enum RTPINGPONGSPEAKER
{
    /** Not initialized. */
    RTPINGPONGSPEAKER_UNINITIALIZE = 0,
    /** Ping is speaking, Pong is waiting. */
    RTPINGPONGSPEAKER_PING,
    /** Pong is signaled, Ping is waiting. */
    RTPINGPONGSPEAKER_PONG_SIGNALED,
    /** Pong is speaking, Ping is waiting. */
    RTPINGPONGSPEAKER_PONG,
    /** Ping is signaled, Pong is waiting. */
    RTPINGPONGSPEAKER_PING_SIGNALED,
    /** Hack to ensure that it's at least 32-bits wide. */
    RTPINGPONGSPEAKER_HACK = 0x7fffffff
} RTPINGPONGSPEAKER;

/**
 * Ping-Pong construct.
 *
 * Two threads, one saying Ping and the other saying Pong. The construct
 * makes sure they don't speak out of turn and that they can wait and poll
 * on the conversation.
 */
typedef struct RTPINGPONG
{
    /** The semaphore the Ping thread waits on. */
    RTSEMEVENT                  Ping;
    /** The semaphore the Pong thread waits on. */
    RTSEMEVENT                  Pong;
    /** The current speaker. */
    volatile RTPINGPONGSPEAKER  enmSpeaker;
#if HC_ARCH_BITS == 64
    /** Padding the structure to become a multiple of sizeof(RTHCPTR). */
    uint32_t                    u32Padding;
#endif
} RTPINGPONG;
/** Pointer to Ping-Pong construct. */
typedef RTPINGPONG *PRTPINGPONG;

/**
 * Init a Ping-Pong construct.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure which needs initialization.
 */
RTDECL(int) RTSemPingPongInit(PRTPINGPONG pPP);

/**
 * Deletes a Ping-Pong construct.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure which is to be destroyed.
 *                      (I.e. put into uninitialized state.)
 */
RTDECL(int) RTSemPingPongDelete(PRTPINGPONG pPP);

/**
 * Signals the pong thread in a ping-pong construct. (I.e. sends ping.)
 * This is called by the ping thread.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure to ping.
 */
RTDECL(int) RTSemPing(PRTPINGPONG pPP);

/**
 * Signals the ping thread in a ping-pong construct. (I.e. sends pong.)
 * This is called by the pong thread.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure to pong.
 */
RTDECL(int) RTSemPong(PRTPINGPONG pPP);

/**
 * Wait function for the ping thread.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   pPP         Pointer to the ping-pong structure to wait on.
 * @param   cMillies    Number of milliseconds to wait.
 */
RTDECL(int) RTSemPingWait(PRTPINGPONG pPP, unsigned cMillies);

/**
 * Wait function for the pong thread.
 *
 * @returns iprt status code.
 *          Will not return VERR_INTERRUPTED.
 * @param   pPP         Pointer to the ping-pong structure to wait on.
 * @param   cMillies    Number of milliseconds to wait.
 */
RTDECL(int) RTSemPongWait(PRTPINGPONG pPP, unsigned cMillies);


/**
 * Checks if the pong thread is speaking.
 *
 * @returns true / false.
 * @param   pPP         Pointer to the ping-pong structure.
 * @remark  This is NOT the same as !RTSemPongIsSpeaker().
 */
DECLINLINE(bool) RTSemPingIsSpeaker(PRTPINGPONG pPP)
{
    RTPINGPONGSPEAKER enmSpeaker = pPP->enmSpeaker;
    return enmSpeaker == RTPINGPONGSPEAKER_PING;
}


/**
 * Checks if the pong thread is speaking.
 *
 * @returns true / false.
 * @param   pPP         Pointer to the ping-pong structure.
 * @remark  This is NOT the same as !RTSemPingIsSpeaker().
 */
DECLINLINE(bool) RTSemPongIsSpeaker(PRTPINGPONG pPP)
{
    RTPINGPONGSPEAKER enmSpeaker = pPP->enmSpeaker;
    return enmSpeaker == RTPINGPONGSPEAKER_PONG;
}


/**
 * Checks whether the ping thread should wait.
 *
 * @returns true / false.
 * @param   pPP         Pointer to the ping-pong structure.
 * @remark  This is NOT the same as !RTSemPongShouldWait().
 */
DECLINLINE(bool) RTSemPingShouldWait(PRTPINGPONG pPP)
{
    RTPINGPONGSPEAKER enmSpeaker = pPP->enmSpeaker;
    return enmSpeaker == RTPINGPONGSPEAKER_PONG
        || enmSpeaker == RTPINGPONGSPEAKER_PONG_SIGNALED
        || enmSpeaker == RTPINGPONGSPEAKER_PING_SIGNALED;
}


/**
 * Checks whether the pong thread should wait.
 *
 * @returns true / false.
 * @param   pPP         Pointer to the ping-pong structure.
 * @remark  This is NOT the same as !RTSemPingShouldWait().
 */
DECLINLINE(bool) RTSemPongShouldWait(PRTPINGPONG pPP)
{
    RTPINGPONGSPEAKER enmSpeaker = pPP->enmSpeaker;
    return enmSpeaker == RTPINGPONGSPEAKER_PING
        || enmSpeaker == RTPINGPONGSPEAKER_PING_SIGNALED
        || enmSpeaker == RTPINGPONGSPEAKER_PONG_SIGNALED;
}

/** @} */

/** @} */

RT_C_DECLS_END

#endif

