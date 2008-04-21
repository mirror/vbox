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


__BEGIN_DECLS

/** @defgroup grp_rt_sems    RTSem - Semaphores
 * @ingroup grp_rt
 * @{
 */


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


/**
 * Create a fast mutex semaphore.
 *
 * @returns iprt status code.
 * @param   pMutexSem   Where to store the mutex semaphore handle.
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
 * Destroys a Ping-Pong construct.
 *
 * @returns iprt status code.
 * @param   pPP         Pointer to the ping-pong structure which is to be destroyed.
 *                      (I.e. put into uninitialized state.)
 */
RTDECL(int) RTSemPingPongDestroy(PRTPINGPONG pPP);

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

/** @} */

__END_DECLS

#endif

