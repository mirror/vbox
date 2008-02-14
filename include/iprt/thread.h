/** @file
 * innotek Portable Runtime - Threads.
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

#ifndef ___iprt_thread_h
#define ___iprt_thread_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

#ifdef IN_GC
# error "There are no threading APIs available Guest Context!"
#endif



__BEGIN_DECLS

/** @defgroup grp_rt_thread    RTThread - Thread Management
 * @ingroup grp_rt
 * @{
 */

/**
 * Get the thread handle of the current thread.
 *
 * @returns Thread handle.
 */
RTDECL(RTTHREAD) RTThreadSelf(void);

/**
 * Get the native thread handle of the current thread.
 *
 * @returns Native thread handle.
 */
RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void);

/**
 * Millisecond granular sleep function.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_INTERRUPTED if a signal or other asynchronous stuff happend
 *          which interrupt the peaceful sleep.
 * @param   cMillies    Number of milliseconds to sleep.
 *                      0 milliseconds means yielding the timeslice - deprecated!
 * @remark  See RTThreadNanoSleep() for sleeping for smaller periods of time.
 */
RTDECL(int) RTThreadSleep(unsigned cMillies);

/**
 * Yields the CPU.
 *
 * @returns true if we yielded.
 * @returns false if it's probable that we didn't yield.
 */
RTDECL(bool) RTThreadYield(void);



/**
 * Thread function.
 *
 * @returns 0 on success.
 * @param   ThreadSelf  Thread handle to this thread.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(int) FNRTTHREAD(RTTHREAD ThreadSelf, void *pvUser);
/** Pointer to a FNRTTHREAD(). */
typedef FNRTTHREAD *PFNRTTHREAD;

/**
 * Thread types.
 * Besides identifying the purpose of the thread, the thread type is
 * used to select the scheduling properties.
 *
 * The types in are placed in a rough order of ascending priority.
 */
typedef enum RTTHREADTYPE
{
    /** Invalid type. */
    RTTHREADTYPE_INVALID = 0,
    /** Infrequent poller thread.
     * This type of thread will sleep for the most of the time, and do
     * infrequent polls on resources at 0.5 sec or higher intervals.
     */
    RTTHREADTYPE_INFREQUENT_POLLER,
    /** Main heavy worker thread.
     * Thread of this type is driving asynchronous tasks in the Main
     * API which takes a long time and might involve a bit of CPU. Like
     * for instance creating a fixed sized VDI.
     */
    RTTHREADTYPE_MAIN_HEAVY_WORKER,
    /** The emulation thread type.
     * While being a thread with very high workload it still is vital
     * that it gets scheduled frequently. When possible all other thread
     * types except DEFAULT and GUI should interrupt this one ASAP when
     * they become ready.
     */
    RTTHREADTYPE_EMULATION,
    /** The default thread type.
     * Since it doesn't say much about the purpose of the thread
     * nothing special is normally done to the scheduling. This type
     * should be avoided.
     * The main thread is registered with default type during RTR3Init()
     * and that's what the default process priority is derived from.
     */
    RTTHREADTYPE_DEFAULT,
    /** The GUI thread type
     * The GUI normally have a low workload but is frequently scheduled
     * to handle events. When possible the scheduler should not leave
     * threads of this kind waiting for too long (~50ms).
     */
    RTTHREADTYPE_GUI,
    /** Main worker thread.
     * Thread of this type is driving asynchronous tasks in the Main API.
     * In most cases this means little work an a lot of waiting.
     */
    RTTHREADTYPE_MAIN_WORKER,
    /** VRDP I/O thread.
     * These threads are I/O threads in the RDP server will hang around
     * waiting for data, process it and pass it on.
     */
    RTTHREADTYPE_VRDP_IO,
    /** The debugger type.
     * Threads involved in servicing the debugger. It must remain
     * responsive even when things are running wild in.
     */
    RTTHREADTYPE_DEBUGGER,
    /** Message pump thread.
     * Thread pumping messages from one thread/process to another
     * thread/process. The workload is very small, most of the time
     * it's blocked waiting for messages to be procduced or processed.
     * This type of thread will be favored after I/O threads.
     */
    RTTHREADTYPE_MSG_PUMP,
    /** The I/O thread type.
     * Doing I/O means shuffling data, waiting for request to arrive and
     * for them to complete. The thread should be favored when competing
     * with any other threads except timer threads.
     */
    RTTHREADTYPE_IO,
    /** The timer thread type.
     * A timer thread is mostly waiting for the timer to tick
     * and then perform a little bit of work. Accuracy is important here,
     * so the thread should be favoured over all threads. If premention can
     * be configured at thread level, it could be made very short.
     */
    RTTHREADTYPE_TIMER,
    /** Only used for validation. */
    RTTHREADTYPE_END
} RTTHREADTYPE;


/**
 * Thread creation flags.
 */
typedef enum RTTHREADFLAGS
{
    /**
     * This flag is used to keep the thread structure around so it can
     * be waited on after termination.
     */
    RTTHREADFLAGS_WAITABLE = RT_BIT(0),
    /** The bit number corresponding to the RTTHREADFLAGS_WAITABLE mask. */
    RTTHREADFLAGS_WAITABLE_BIT = 0,

    /** Mask of valid flags, use for validation. */
    RTTHREADFLAGS_MASK = RT_BIT(0)
} RTTHREADFLAGS;


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
 *
 * @remark  When called in Ring-0, this API will create a new kernel thread and not a thread in
 *          the context of the calling process.
 */
RTDECL(int) RTThreadCreate(PRTTHREAD pThread, PFNRTTHREAD pfnThread, void *pvUser, size_t cbStack,
                           RTTHREADTYPE enmType, unsigned fFlags, const char *pszName);

/**
 * Gets the native thread id of a IPRT thread.
 *
 * @returns The native thread id.
 * @param   Thread      The IPRT thread.
 */
RTDECL(RTNATIVETHREAD) RTThreadGetNative(RTTHREAD Thread);

/**
 * Gets the IPRT thread of a native thread.
 *
 * @returns The IPRT thread handle
 * @returns NIL_RTTHREAD if not a thread known to IPRT.
 * @param   NativeThread        The native thread handle/id.
 */
RTDECL(RTTHREAD) RTThreadFromNative(RTNATIVETHREAD NativeThread);

/**
 * Changes the type of the specified thread.
 *
 * @returns iprt status code.
 * @param   Thread      The thread which type should be changed.
 * @param   enmType     The new thread type.
 * @remark  In Ring-0 it only works if Thread == RTThreadSelf().
 */
RTDECL(int) RTThreadSetType(RTTHREAD Thread, RTTHREADTYPE enmType);

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
RTDECL(int) RTThreadWait(RTTHREAD Thread, unsigned cMillies, int *prc);

/**
 * Wait for the thread to terminate, return on interruption.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 * @param       prc             Where to store the return code of the thread. Optional.
 */
RTDECL(int) RTThreadWaitNoResume(RTTHREAD Thread, unsigned cMillies, int *prc);

/**
 * Gets the name of the current thread thread.
 *
 * @returns Pointer to readonly name string.
 * @returns NULL on failure.
 */
RTDECL(const char *) RTThreadSelfName(void);

/**
 * Gets the name of a thread.
 *
 * @returns Pointer to readonly name string.
 * @returns NULL on failure.
 * @param   Thread      Thread handle of the thread to query the name of.
 */
RTDECL(const char *) RTThreadGetName(RTTHREAD Thread);

/**
 * Gets the type of the specified thread.
 *
 * @returns The thread type.
 * @returns RTTHREADTYPE_INVALID if the thread handle is invalid.
 * @param   Thread      The thread in question.
 */
RTDECL(RTTHREADTYPE) RTThreadGetType(RTTHREAD Thread);

/**
 * Sets the name of a thread.
 *
 * @returns iprt status code.
 * @param   Thread      Thread handle of the thread to query the name of.
 * @param   pszName     The thread name.
 */
RTDECL(int) RTThreadSetName(RTTHREAD Thread, const char *pszName);

/**
 * Signal the user event.
 *
 * @returns     iprt status code.
 */
RTDECL(int) RTThreadUserSignal(RTTHREAD Thread);

/**
 * Wait for the user event.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 */
RTDECL(int) RTThreadUserWait(RTTHREAD Thread, unsigned cMillies);

/**
 * Wait for the user event, return on interruption.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to wait for.
 * @param       cMillies        The number of milliseconds to wait. Use RT_INDEFINITE_WAIT for
 *                              an indefinite wait.
 */
RTDECL(int) RTThreadUserWaitNoResume(RTTHREAD Thread, unsigned cMillies);

/**
 * Reset the user event.
 *
 * @returns     iprt status code.
 * @param       Thread          The thread to reset.
 */
RTDECL(int) RTThreadUserReset(RTTHREAD Thread);


#ifdef IN_RING3

/**
 * Adopts a non-IPRT thread.
 *
 * @returns IPRT status code.
 * @param   enmType         The thread type.
 * @param   fFlags          The thread flags. RTTHREADFLAGS_WAITABLE is not currently allowed.
 * @param   pszName         The thread name. Optional
 * @param   pThread         Where to store the thread handle. Optional.
 */
RTDECL(int) RTThreadAdopt(RTTHREADTYPE enmType, unsigned fFlags, const char *pszName, PRTTHREAD pThread);

/**
 * Gets the affinity mask of the current thread.
 *
 * @returns The affinity mask (bit 0 = logical cpu 0).
 */
RTR3DECL(uint64_t) RTThreadGetAffinity(void);

/**
 * Sets the affinity mask of the current thread.
 *
 * @returns iprt status code.
 * @param   u64Mask         Affinity mask (bit 0 = logical cpu 0).
 */
RTR3DECL(int) RTThreadSetAffinity(uint64_t u64Mask);


/** @name Thread Local Storage
 * @{
 */
/**
 * Thread termination callback for destroying a non-zero TLS entry.
 *
 * @remarks It is not permittable to use any RTTls APIs at this time. Doing so
 *          may lead to endless loops, crashes, and other bad stuff.
 *
 * @param   pvValue     The current value.
 */
typedef DECLCALLBACK(void) FNRTTLSDTOR(void *pvValue);
/** Pointer to a FNRTTLSDTOR. */
typedef FNRTTLSDTOR *PFNRTTLSDTOR;

/**
 * Allocates a TLS entry.
 *
 * @returns the index of the allocated TLS entry.
 * @returns NIL_RTTLS on failure.
 */
RTR3DECL(RTTLS) RTTlsAlloc(void);

/**
 * Allocates a TLS entry (with status code).
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if pfnDestructor is non-NULL and the platform
 *          doesn't support this feature.
 *
 * @param   piTls           Where to store the index of the allocated TLS entry.
 *                          This is set to NIL_RTTLS on failure.
 * @param   pfnDestructor   Optional callback function for cleaning up on
 *                          thread termination. WARNING! This feature may not
 *                          be implemented everywhere.
 */
RTR3DECL(int) RTTlsAllocEx(PRTTLS piTls, PFNRTTLSDTOR pfnDestructor);

/**
 * Frees a TLS entry.
 *
 * @returns IPRT status code.
 * @param   iTls        The index of the TLS entry.
 */
RTR3DECL(int) RTTlsFree(RTTLS iTls);

/**
 * Get the value stored in a TLS entry.
 *
 * @returns value in given TLS entry.
 * @returns NULL on failure.
 * @param   iTls        The index of the TLS entry.
 */
RTR3DECL(void *) RTTlsGet(RTTLS iTls);

/**
 * Get the value stored in a TLS entry.
 *
 * @returns IPRT status code.
 * @param   iTls        The index of the TLS entry.
 * @param   ppvValue    Where to store the value.
 */
RTR3DECL(int) RTTlsGetEx(RTTLS iTls, void **ppvValue);

/**
 * Set the value stored in an allocated TLS entry.
 *
 * @returns IPRT status.
 * @param   iTls        The index of the TLS entry.
 * @param   pvValue     The value to store.
 *
 * @remarks Note that NULL is considered to special value.
 */
RTR3DECL(int) RTTlsSet(RTTLS iTls, void *pvValue);

/** @} */

#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif

