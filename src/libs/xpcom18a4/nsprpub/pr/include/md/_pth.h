/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nspr_pth_defs_h_
#define nspr_pth_defs_h_

/*
** Appropriate definitions of entry points not used in a pthreads world
*/
#define _PR_MD_BLOCK_CLOCK_INTERRUPTS()
#define _PR_MD_UNBLOCK_CLOCK_INTERRUPTS()
#define _PR_MD_DISABLE_CLOCK_INTERRUPTS()
#define _PR_MD_ENABLE_CLOCK_INTERRUPTS()

#define _PT_PTHREAD_MUTEXATTR_INIT        pthread_mutexattr_init
#define _PT_PTHREAD_MUTEXATTR_DESTROY     pthread_mutexattr_destroy
#define _PT_PTHREAD_MUTEX_INIT(m, a)      pthread_mutex_init(&(m), &(a))
#define _PT_PTHREAD_MUTEX_IS_LOCKED(m)    (EBUSY == pthread_mutex_trylock(&(m)))
#if defined(DARWIN)
#define _PT_PTHREAD_CONDATTR_INIT(x)      0
#else
#define _PT_PTHREAD_CONDATTR_INIT         pthread_condattr_init
#endif
#define _PT_PTHREAD_CONDATTR_DESTROY      pthread_condattr_destroy
#define _PT_PTHREAD_COND_INIT(m, a)       pthread_cond_init(&(m), &(a))

/* The pthreads standard does not specify an invalid value for the
 * pthread_t handle.  (0 is usually an invalid pthread identifier
 * but there are exceptions, for example, DG/UX.)  These macros
 * define a way to set the handle to or compare the handle with an
 * invalid identifier.  These macros are not portable and may be
 * more of a problem as we adapt to more pthreads implementations.
 * They are only used in the PRMonitor functions.  Do not use them
 * in new code.
 *
 * Unfortunately some of our clients depend on certain properties
 * of our PRMonitor implementation, preventing us from replacing
 * it by a portable implementation.
 * - High-performance servers like the fact that PR_EnterMonitor
 *   only calls PR_Lock and PR_ExitMonitor only calls PR_Unlock.
 *   (A portable implementation would use a PRLock and a PRCondVar
 *   to implement the recursive lock in a monitor and call both
 *   PR_Lock and PR_Unlock in PR_EnterMonitor and PR_ExitMonitor.)
 *   Unfortunately this forces us to read the monitor owner field
 *   without holding a lock.
 * - One way to make it safe to read the monitor owner field
 *   without holding a lock is to make that field a PRThread*
 *   (one should be able to read a pointer with a single machine
 *   instruction).  However, PR_GetCurrentThread calls calloc if
 *   it is called by a thread that was not created by NSPR.  The
 *   malloc tracing tools in the Mozilla client use PRMonitor for
 *   locking in their malloc, calloc, and free functions.  If
 *   PR_EnterMonitor calls any of these functions, infinite
 *   recursion ensues.
 */
#if defined(IRIX) || defined(OSF1) || defined(AIX) || defined(SOLARIS) \
	|| defined(HPUX) || defined(LINUX) || defined(FREEBSD) \
	|| defined(NETBSD) || defined(OPENBSD) || defined(BSDI) \
	|| defined(VMS) || defined(NTO) || defined(DARWIN) \
	|| defined(UNIXWARE)
#define _PT_PTHREAD_INVALIDATE_THR_HANDLE(t)  (t) = 0
#define _PT_PTHREAD_THR_HANDLE_IS_INVALID(t)  (t) == 0
#define _PT_PTHREAD_COPY_THR_HANDLE(st, dt)   (dt) = (st)
#else 
#error "pthreads is not supported for this architecture"
#endif

#if defined(_PR_PTHREADS)
#define _PT_PTHREAD_ATTR_INIT            pthread_attr_init
#define _PT_PTHREAD_ATTR_DESTROY         pthread_attr_destroy
#define _PT_PTHREAD_CREATE(t, a, f, r)   pthread_create(t, &a, f, r) 
#define _PT_PTHREAD_KEY_CREATE           pthread_key_create
#define _PT_PTHREAD_ATTR_SETSCHEDPOLICY  pthread_attr_setschedpolicy
#define _PT_PTHREAD_ATTR_GETSTACKSIZE(a, s) pthread_attr_getstacksize(a, s)
#define _PT_PTHREAD_GETSPECIFIC(k, r)    (r) = pthread_getspecific(k)
#else
#error "Cannot determine pthread strategy"
#endif

#define PT_TRYLOCK_SUCCESS 0
#define PT_TRYLOCK_BUSY    EBUSY

/*
 * These platforms don't have sigtimedwait()
 */
#if (defined(AIX) && !defined(AIX4_3_PLUS)) || defined(LINUX) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) || defined(VMS) || defined(UNIXWARE) \
	|| defined(DARWIN)
#define PT_NO_SIGTIMEDWAIT
#endif

/*
 * These platforms don't have pthread_kill()
 */
#if defined(DARWIN) && !defined(_DARWIN_FEATURE_UNIX_CONFORMANCE)
#define pthread_kill(thread, sig) ENOSYS
#endif

#if defined(LINUX) || defined(FREEBSD)
#define PT_PRIO_MIN            sched_get_priority_min(SCHED_OTHER)
#define PT_PRIO_MAX            sched_get_priority_max(SCHED_OTHER)
#elif defined(SOLARIS)
/*
 * Solaris doesn't seem to have macros for the min/max priorities.
 * The range of 0-127 is mentioned in the pthread_setschedparam(3T)
 * man pages, and pthread_setschedparam indeed allows 0-127.  However,
 * pthread_attr_setschedparam does not allow 0; it allows 1-127.
 */
#define PT_PRIO_MIN            1
#define PT_PRIO_MAX            127
#elif defined(OPENBSD)
#define PT_PRIO_MIN            0
#define PT_PRIO_MAX            31
#elif defined(NETBSD) \
	|| defined(BSDI) || defined(DARWIN) || defined(UNIXWARE) /* XXX */
#define PT_PRIO_MIN            0
#define PT_PRIO_MAX            126
#else
#error "pthreads is not supported for this architecture"
#endif

/*
 * The _PT_PTHREAD_YIELD function is called from a signal handler.
 * Needed for garbage collection -- Look at PR_Suspend/PR_Resume
 * implementation.
 */
#if defined(HPUX) || defined(LINUX) || defined(SOLARIS) \
	|| defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
	|| defined(BSDI) || defined(NTO) || defined(DARWIN) \
	|| defined(UNIXWARE)
#define _PT_PTHREAD_YIELD()            	sched_yield()
#else
#error "Need to define _PT_PTHREAD_YIELD for this platform"
#endif

#endif /* nspr_pth_defs_h_ */
