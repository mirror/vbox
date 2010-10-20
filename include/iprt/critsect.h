/** @file
 * IPRT - Critical Sections.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#ifndef ___iprt_critsect_h
#define ___iprt_critsect_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif
#ifdef RT_LOCK_STRICT_ORDER
# include <iprt/lockvalidator.h>
#endif

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_critsect       RTCritSect - Critical Sections
 *
 * "Critical section" synchronization primitives can be used to
 * protect a section of code or data to which access must be exclusive;
 * only one thread can hold access to a critical section at one time.
 *
 * A critical section is a fast recursive write lock; if the critical
 * section is not acquired, then entering it is fast (requires no system
 * call). IPRT uses the Windows terminology here; on other platform, this
 * might be called a "futex" or a "fast mutex". As opposed to IPRT
 * "fast mutexes" (see @ref grp_rt_sems_fast_mutex ), critical sections
 * are recursive.
 *
 * Use RTCritSectInit to initialize a critical section; use RTCritSectEnter
 * and RTCritSectLeave to acquire and release access.
 *
 * For an overview of all types of synchronization primitives provided
 * by IPRT (event, mutex/fast mutex/read-write mutex semaphores), see
 * @ref grp_rt_sems .
 *
 * @ingroup grp_rt
 * @{
 */

/**
 * Critical section.
 */
typedef struct RTCRITSECT
{
    /** Magic used to validate the section state.
     * RTCRITSECT_MAGIC is the value of an initialized & operational section. */
    volatile uint32_t                   u32Magic;
    /** Number of lockers.
     * -1 if the section is free. */
    volatile int32_t                    cLockers;
    /** The owner thread. */
    volatile RTNATIVETHREAD             NativeThreadOwner;
    /** Number of nested enter operations performed.
     * Greater or equal to 1 if owned, 0 when free.
     */
    volatile int32_t                    cNestings;
    /** Section flags - the RTCRITSECT_FLAGS_* \#defines. */
    uint32_t                            fFlags;
    /** The semaphore to block on. */
    RTSEMEVENT                          EventSem;
    /** Lock validator record.  Only used in strict builds. */
    R3R0PTRTYPE(PRTLOCKVALRECEXCL)      pValidatorRec;
    /** Alignmnet padding. */
    RTHCPTR                             Alignment;
} RTCRITSECT;
AssertCompileSize(RTCRITSECT, HC_ARCH_BITS == 32 ? 32 : 48);

/** RTCRITSECT::u32Magic value. (Hiromi Uehara) */
#define RTCRITSECT_MAGIC                UINT32_C(0x19790326)

/** @name RTCritSectInitEx flags / RTCRITSECT::fFlags
 * @{ */
/** If set, nesting(/recursion) is not allowed. */
#define RTCRITSECT_FLAGS_NO_NESTING     UINT32_C(0x00000001)
/** Disables lock validation. */
#define RTCRITSECT_FLAGS_NO_LOCK_VAL    UINT32_C(0x00000002)
/** Bootstrap hack for use with certain memory allocator locks only! */
#define RTCRITSECT_FLAGS_BOOTSTRAP_HACK UINT32_C(0x00000004)
/** @} */

#ifdef IN_RING3

/**
 * Initialize a critical section.
 */
RTDECL(int) RTCritSectInit(PRTCRITSECT pCritSect);

/**
 * Initialize a critical section.
 *
 * @returns iprt status code.
 * @param   pCritSect           Pointer to the critical section structure.
 * @param   fFlags              Flags, any combination of the RTCRITSECT_FLAGS
 *                              \#defines.
 * @param   hClass              The class (no reference consumed).  If NIL, no
 *                              lock order validation will be performed on this
 *                              lock.
 * @param   uSubClass           The sub-class.  This is used to define lock
 *                              order within a class.  RTLOCKVAL_SUB_CLASS_NONE
 *                              is the recommended value here.
 * @param   pszNameFmt          Name format string for the lock validator,
 *                              optional (NULL).  Max length is 32 bytes.
 * @param   ...                 Format string arguments.
 */
RTDECL(int) RTCritSectInitEx(PRTCRITSECT pCritSect, uint32_t fFlags,
                             RTLOCKVALCLASS hClass, uint32_t uSubClass, const char *pszNameFmt, ...);

/**
 * Changes the lock validator sub-class of the critical section.
 *
 * It is recommended to try make sure that nobody is using this critical section
 * while changing the value.
 *
 * @returns The old sub-class.  RTLOCKVAL_SUB_CLASS_INVALID is returns if the
 *          lock validator isn't compiled in or either of the parameters are
 *          invalid.
 * @param   pCritSect           The critical section.
 * @param   uSubClass           The new sub-class value.
 */
RTDECL(uint32_t) RTCritSectSetSubClass(PRTCRITSECT pCritSect, uint32_t uSubClass);

/**
 * Enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect       The critical section.
 */
RTDECL(int) RTCritSectEnter(PRTCRITSECT pCritSect);

/**
 * Enter a critical section.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 *
 * @param   pCritSect       The critical section.
 * @param   uId             Where we're entering the section.
 * @param   pszFile         The source position - file.
 * @param   iLine           The source position - line.
 * @param   pszFunction     The source position - function.
 */
RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL);

/**
 * Try enter a critical section.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 *
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectTryEnter(PRTCRITSECT pCritSect);

/**
 * Try enter a critical section.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 *
 * @param   pCritSect       The critical section.
 * @param   uId             Where we're entering the section.
 * @param   pszFile         The source position - file.
 * @param   iLine           The source position - line.
 * @param   pszFunction     The source position - function.
 */
RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL);

/**
 * Enter multiple critical sections.
 *
 * This function will enter ALL the specified critical sections before returning.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   cCritSects      Number of critical sections in the array.
 * @param   papCritSects    Array of critical section pointers.
 *
 * @remark  Please note that this function will not necessarily come out favourable in a
 *          fight with other threads which are using the normal RTCritSectEnter() function.
 *          Therefore, avoid having to enter multiple critical sections!
 */
RTDECL(int) RTCritSectEnterMultiple(size_t cCritSects, PRTCRITSECT *papCritSects);

/**
 * Enter multiple critical sections.
 *
 * This function will enter ALL the specified critical sections before returning.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 *
 * @param   cCritSects      Number of critical sections in the array.
 * @param   papCritSects    Array of critical section pointers.
 * @param   uId             Where we're entering the section.
 * @param   pszFile         The source position - file.
 * @param   iLine           The source position - line.
 * @param   pszFunction     The source position - function.
 *
 * @remark  See RTCritSectEnterMultiple().
 */
RTDECL(int) RTCritSectEnterMultipleDebug(size_t cCritSects, PRTCRITSECT *papCritSects, RTUINTPTR uId, RT_SRC_POS_DECL);

/**
 * Leave a critical section.
 *
 * @returns VINF_SUCCESS.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectLeave(PRTCRITSECT pCritSect);

/**
 * Leave multiple critical sections.
 *
 * @returns VINF_SUCCESS.
 * @param   cCritSects      Number of critical sections in the array.
 * @param   papCritSects    Array of critical section pointers.
 */
RTDECL(int) RTCritSectLeaveMultiple(size_t cCritSects, PRTCRITSECT *papCritSects);

/**
 * Deletes a critical section.
 *
 * @returns VINF_SUCCESS.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectDelete(PRTCRITSECT pCritSect);

/**
 * Checks the caller is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pCritSect   The critical section.
 */
DECLINLINE(bool) RTCritSectIsOwner(PCRTCRITSECT pCritSect)
{
    return pCritSect->NativeThreadOwner == RTThreadNativeSelf();
}

#endif /* IN_RING3 */

/**
 * Checks the section is owned by anyone.
 *
 * @returns true if owned.
 * @returns false if not owned.
 * @param   pCritSect   The critical section.
 */
DECLINLINE(bool) RTCritSectIsOwned(PCRTCRITSECT pCritSect)
{
    return pCritSect->NativeThreadOwner != NIL_RTNATIVETHREAD;
}

/**
 * Gets the thread id of the critical section owner.
 *
 * @returns Thread id of the owner thread if owned.
 * @returns NIL_RTNATIVETHREAD is not owned.
 * @param   pCritSect   The critical section.
 */
DECLINLINE(RTNATIVETHREAD) RTCritSectGetOwner(PCRTCRITSECT pCritSect)
{
    return pCritSect->NativeThreadOwner;
}

/**
 * Checks if a critical section is initialized or not.
 *
 * @returns true if initialized.
 * @returns false if not initialized.
 * @param   pCritSect   The critical section.
 */
DECLINLINE(bool) RTCritSectIsInitialized(PCRTCRITSECT pCritSect)
{
    return pCritSect->u32Magic == RTCRITSECT_MAGIC;
}

/**
 * Gets the recursion depth.
 *
 * @returns The recursion depth.
 * @param   pCritSect       The Critical section
 */
DECLINLINE(uint32_t) RTCritSectGetRecursion(PCRTCRITSECT pCritSect)
{
    return pCritSect->cNestings;
}

/**
 * Gets the waiter count
 *
 * @returns The waiter count
 * @param   pCritSect       The Critical section
 */
DECLINLINE(int32_t) RTCritSectGetWaiters(PCRTCRITSECT pCritSect)
{
    return pCritSect->cLockers;
}

/* Lock strict build: Remap the three enter calls to the debug versions. */
#ifdef RT_LOCK_STRICT
# ifdef ___iprt_asm_h
#  define RTCritSectEnter(pCritSect)                        RTCritSectEnterDebug(pCritSect, (uintptr_t)ASMReturnAddress(), RT_SRC_POS)
#  define RTCritSectTryEnter(pCritSect)                     RTCritSectTryEnterDebug(pCritSect, (uintptr_t)ASMReturnAddress(), RT_SRC_POS)
#  define RTCritSectEnterMultiple(cCritSects, pCritSect)    RTCritSectEnterMultipleDebug((cCritSects), (pCritSect), (uintptr_t)ASMReturnAddress(), RT_SRC_POS)
# else
#  define RTCritSectEnter(pCritSect)                        RTCritSectEnterDebug(pCritSect, 0, RT_SRC_POS)
#  define RTCritSectTryEnter(pCritSect)                     RTCritSectTryEnterDebug(pCritSect, 0, RT_SRC_POS)
#  define RTCritSectEnterMultiple(cCritSects, pCritSect)    RTCritSectEnterMultipleDebug((cCritSects), (pCritSect), 0, RT_SRC_POS)
# endif
#endif

/* Strict lock order: Automatically classify locks by init location. */
#if defined(RT_LOCK_STRICT_ORDER) && defined(IN_RING3)
# define RTCritSectInit(pCritSect) \
    RTCritSectInitEx((pCritSect), 0 /*fFlags*/, \
                     RTLockValidatorClassForSrcPos(RT_SRC_POS, NULL), \
                     RTLOCKVAL_SUB_CLASS_NONE, NULL)
#endif

/** @} */

RT_C_DECLS_END

#endif

