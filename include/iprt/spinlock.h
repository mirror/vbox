/** @file
 * IPRT - Spinlocks.
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

#ifndef ___iprt_spinlock_h
#define ___iprt_spinlock_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS


/** @defgroup grp_rt_spinlock   RTSpinlock - Spinlocks
 * @ingroup grp_rt
 * @{
 */

/**
 * Temporary spinlock state variable.
 * All members are undefined and highly platform specific.
 */
typedef struct RTSPINLOCKTMP
{
#ifdef IN_RING0
# ifdef RT_OS_LINUX
    /** The saved [R|E]FLAGS. */
    unsigned long   flFlags;
#  define RTSPINLOCKTMP_INITIALIZER { 0 }

# elif defined(RT_OS_WINDOWS)
    /** The saved [R|E]FLAGS. */
    RTUINTREG       uFlags;
    /** The KIRQL. */
    unsigned char   uchIrqL;
#  define RTSPINLOCKTMP_INITIALIZER { 0, 0 }

# elif defined(__L4__)
    /** The saved [R|E]FLAGS. */
    unsigned long   flFlags;
#  define RTSPINLOCKTMP_INITIALIZER { 0 }

# elif defined(RT_OS_DARWIN)
    /** The saved [R|E]FLAGS. */
    RTUINTREG       uFlags;
#  define RTSPINLOCKTMP_INITIALIZER { 0 }

# elif defined(RT_OS_OS2) || defined(RT_OS_FREEBSD) || defined(RT_OS_SOLARIS)
    /** The saved [R|E]FLAGS. (dummy) */
    RTUINTREG       uFlags;
#  define RTSPINLOCKTMP_INITIALIZER { 0 }

# else
#  error "Your OS is not supported.\n"
    /** The saved [R|E]FLAGS. */
    RTUINTREG       uFlags;
# endif

#else /* !IN_RING0 */
    /** The saved [R|E]FLAGS.
     * (RT spinlocks will by definition disable interrupts.) */
    RTUINTREG       uFlags;
# define RTSPINLOCKTMP_INITIALIZER { 0 }
#endif /* !IN_RING0 */
} RTSPINLOCKTMP;
/** Pointer to a temporary spinlock state variable. */
typedef RTSPINLOCKTMP *PRTSPINLOCKTMP;
/** Pointer to a const temporary spinlock state variable. */
typedef const RTSPINLOCKTMP *PCRTSPINLOCKTMP;

/** @def RTSPINLOCKTMP_INITIALIZER
 * What to assign to a RTSPINLOCKTMP at definition.
 */
#ifdef DOXYGEN_RUNNING
# define RTSPINLOCKTMP_INITIALIZER
#endif



/**
 * Creates a spinlock.
 *
 * @returns iprt status code.
 * @param   pSpinlock   Where to store the spinlock handle.
 */
RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock);

/**
 * Destroys a spinlock created by RTSpinlockCreate().
 *
 * @returns iprt status code.
 * @param   Spinlock    Spinlock returned by RTSpinlockCreate().
 */
RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock);

/**
 * Acquires the spinlock.
 * Interrupts are disabled upon return.
 *
 * @param   Spinlock    The spinlock to acquire.
 * @param   pTmp        Where to save the state.
 */
RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);

/**
 * Releases the spinlock.
 *
 * @param   Spinlock    The spinlock to acquire.
 * @param   pTmp        The state to restore. (This better be the same as for the RTSpinlockAcquire() call!)
 */
RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);

/**
 * Acquires the spinlock.
 *
 * @param   Spinlock    The spinlock to acquire.
 * @param   pTmp        Where to save the state.
 */
RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);

/**
 * Releases the spinlock.
 *
 * @param   Spinlock    The spinlock to acquire.
 * @param   pTmp        The state to restore. (This better be the same as for the RTSpinlockAcquire() call!)
 */
RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp);


/** @} */

__END_DECLS

#endif

