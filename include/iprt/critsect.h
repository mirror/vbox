/** @file
 * IPRT - Critical Sections.
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

#ifndef ___iprt_critsect_h
#define ___iprt_critsect_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#ifdef IN_RING3
#include <iprt/thread.h>
#endif


__BEGIN_DECLS

/** @defgroup grp_rt_critsect       RTCritSect - Critical Sections
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
    volatile uint32_t       u32Magic;
    /** Number of lockers.
     * -1 if the section is free. */
    volatile int32_t        cLockers;
    /** The owner thread. */
    volatile RTNATIVETHREAD NativeThreadOwner;
    /** Number of nested enter operations performed.
     * Greater or equal to 1 if owned, 0 when free.
     */
    volatile int32_t        cNestings;
    /** Section flags - the RTCRITSECT_FLAGS_* \#defines. */
    uint32_t                fFlags;
    /** The semaphore to wait for. */
    RTSEMEVENT              EventSem;

    /** Data only used in strict mode for detecting and debugging deadlocks. */
    struct RTCRITSECTSTRICT
    {
        /** Strict: The current owner thread. */
        RTTHREAD volatile                   ThreadOwner;
        /** Strict: Where the section was entered. */
        R3PTRTYPE(const char * volatile)    pszEnterFile;
        /** Strict: Where the section was entered. */
        uint32_t volatile                   u32EnterLine;
#if HC_ARCH_BITS == 64 || GC_ARCH_BITS == 64
        /** Padding for correct alignment. */
        uint32_t                            u32Padding;
#endif
        /** Strict: Where the section was entered. */
        RTUINTPTR volatile                  uEnterId;
    } Strict;
} RTCRITSECT;
/** Pointer to a critical section. */
typedef RTCRITSECT *PRTCRITSECT;
/** Pointer to a const critical section. */
typedef const RTCRITSECT *PCRTCRITSECT;

/** RTCRITSECT::u32Magic value. */
#define RTCRITSECT_MAGIC    0x778899aa

/** If set, nesting(/recursion) is not allowed. */
#define RTCRITSECT_FLAGS_NO_NESTING     1

#ifdef IN_RING3

/**
 * Initialize a critical section.
 */
RTDECL(int) RTCritSectInit(PRTCRITSECT pCritSect);

/**
 * Initialize a critical section.
 *
 * @returns iprt status code.
 * @param   pCritSect   Pointer to the critical section structure.
 * @param   fFlags      Flags, any combination of the RTCRITSECT_FLAGS \#defines.
 */
RTDECL(int) RTCritSectInitEx(PRTCRITSECT pCritSect, uint32_t fFlags);

/**
 * Enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectEnter(PRTCRITSECT pCritSect);

/**
 * Enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 * @param   pszFile     Where we're entering the section.
 * @param   uLine       Where we're entering the section.
 * @param   uId         Where we're entering the section.
 */
RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, const char *pszFile, unsigned uLine, RTUINTPTR uId);

/* in debug mode we'll redefine the enter call. */
#ifdef RT_STRICT
# define RTCritSectEnter(pCritSect) RTCritSectEnterDebug(pCritSect, __FILE__,  __LINE__, 0)
#endif

/**
 * Try enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_BUSY if the critsect was owned.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectTryEnter(PRTCRITSECT pCritSect);

/**
 * Try enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_BUSY if the critsect was owned.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 * @param   pszFile     Where we're entering the section.
 * @param   uLine       Where we're entering the section.
 * @param   uId         Where we're entering the section.
 */
RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, const char *pszFile, unsigned uLine, RTUINTPTR uId);

/* in debug mode we'll redefine the try-enter call. */
#ifdef RT_STRICT
# define RTCritSectTryEnter(pCritSect) RTCritSectTryEnterDebug(pCritSect, __FILE__,  __LINE__, 0)
#endif

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
RTDECL(int) RTCritSectEnterMultiple(unsigned cCritSects, PRTCRITSECT *papCritSects);

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
 * @param   pszFile         Where we're entering the section.
 * @param   uLine           Where we're entering the section.
 * @param   uId             Where we're entering the section.
 *
 * @remark  See RTCritSectEnterMultiple().
 */
RTDECL(int) RTCritSectEnterMultipleDebug(unsigned cCritSects, PRTCRITSECT *papCritSects, const char *pszFile, unsigned uLine, RTUINTPTR uId);

/* in debug mode we'll redefine the enter-multiple call. */
#ifdef RT_STRICT
# define RTCritSectEnterMultiple(cCritSects, pCritSect) RTCritSectEnterMultipleDebug((cCritSects), (pCritSect), __FILE__,  __LINE__, 0)
#endif

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
RTDECL(int) RTCritSectLeaveMultiple(unsigned cCritSects, PRTCRITSECT *papCritSects);

/**
 * Deletes a critical section.
 *
 * @returns VINF_SUCCESS.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectDelete(PRTCRITSECT pCritSect);


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
 * Gets the recursion depth.
 *
 * @returns The recursion depth.
 * @param   pCritSect       The Critical section
 */
DECLINLINE(uint32_t) RTCritSectGetRecursion(PCRTCRITSECT pCritSect)
{
    return pCritSect->cNestings;
}

#endif /* IN_RING3 */
/** @} */

__END_DECLS

#endif

