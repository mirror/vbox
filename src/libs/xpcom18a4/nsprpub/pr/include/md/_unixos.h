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

#ifndef prunixos_h___
#define prunixos_h___

/*
 * If FD_SETSIZE is not defined on the command line, set the default value
 * before include select.h
 */
/*
 * Linux: FD_SETSIZE is defined in /usr/include/sys/select.h and should
 * not be redefined.
 */
#if !defined(LINUX) && !defined(DARWIN) && !defined(NEXTSTEP)
#ifndef FD_SETSIZE
#define FD_SETSIZE  4096
#endif
#endif

#include <unistd.h>
#include <stddef.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "prmem.h"
#include "prclist.h"

/*
 * For select(), fd_set, and struct timeval.
 *
 * In The Single UNIX(R) Specification, Version 2,
 * the header file for select() is <sys/time.h>.
 *
 * fd_set is defined in <sys/types.h>.  Usually
 * <sys/time.h> includes <sys/types.h>, but on some
 * older systems <sys/time.h> does not include
 * <sys/types.h>, so we include it explicitly.
 */
#include <sys/time.h>
#include <sys/types.h>

#define PR_DIRECTORY_SEPARATOR		'/'
#define PR_DIRECTORY_SEPARATOR_STR	"/"
#define PR_PATH_SEPARATOR		':'
#define PR_PATH_SEPARATOR_STR		":"
#define GCPTR
typedef int (*FARPROC)();

/*
 * intervals at which GLOBAL threads wakeup to check for pending interrupt
 */
#define _PR_INTERRUPT_CHECK_INTERVAL_SECS 5
extern PRIntervalTime intr_timeout_ticks;

/*
** Make a redzone at both ends of the stack segment. Disallow access
** to those pages of memory. It's ok if the mprotect call's don't
** work - it just means that we don't really have a functional
** redzone.
*/
#include <sys/mman.h>
#ifndef PROT_NONE
#define PROT_NONE 0x0
#endif

#if defined(DEBUG) && !defined(DARWIN) && !defined(NEXTSTEP)
#if !defined(SOLARIS)
#include <string.h>  /* for memset() */
#define _MD_INIT_STACK(ts,REDZONE)					\
    PR_BEGIN_MACRO                 					\
	(void) mprotect((void*)ts->seg->vaddr, REDZONE, PROT_NONE);	\
	(void) mprotect((void*) ((char*)ts->seg->vaddr + REDZONE + ts->stackSize),\
			REDZONE, PROT_NONE);				\
    /*									\
    ** Fill stack memory with something that turns into an illegal	\
    ** pointer value. This will sometimes find runtime references to	\
    ** uninitialized pointers. We don't do this for solaris because we	\
    ** can use purify instead.						\
    */									\
    if (_pr_debugStacks) {						\
	memset(ts->allocBase + REDZONE, 0xf7, ts->stackSize);		\
    }									\
    PR_END_MACRO
#else	/* !SOLARIS	*/
#define _MD_INIT_STACK(ts,REDZONE)					\
    PR_BEGIN_MACRO                 					\
	(void) mprotect((void*)ts->seg->vaddr, REDZONE, PROT_NONE);	\
	(void) mprotect((void*) ((char*)ts->seg->vaddr + REDZONE + ts->stackSize),\
			REDZONE, PROT_NONE);				\
    PR_END_MACRO
#endif	/* !SOLARIS	*/

/*
 * _MD_CLEAR_STACK
 *	Allow access to the redzone pages; the access was turned off in
 *	_MD_INIT_STACK.
 */
#define _MD_CLEAR_STACK(ts)						\
    PR_BEGIN_MACRO                 					\
	(void) mprotect((void*)ts->seg->vaddr, REDZONE, PROT_READ|PROT_WRITE);\
	(void) mprotect((void*) ((char*)ts->seg->vaddr + REDZONE + ts->stackSize),\
			REDZONE, PROT_READ|PROT_WRITE);			\
    PR_END_MACRO

#else	/* DEBUG */

#define _MD_INIT_STACK(ts,REDZONE)
#define _MD_CLEAR_STACK(ts)

#endif	/* DEBUG */

#if !defined(SOLARIS)

#define PR_SET_INTSOFF(newval)

#endif

/************************************************************************/

extern void _PR_UnixInit(void);

/************************************************************************/

#if !defined(HPUX_LW_TIMER)
#define _MD_INTERVAL_INIT()
#endif
#define _MD_INTERVAL_PER_MILLISEC()	(_PR_MD_INTERVAL_PER_SEC() / 1000)
#define _MD_INTERVAL_PER_MICROSEC()	(_PR_MD_INTERVAL_PER_SEC() / 1000000)

/************************************************************************/

#define _MD_ERRNO()             	(errno)
#define _MD_GET_SOCKET_ERROR()		(errno)

/************************************************************************/

/*
 * The standard (XPG4) gettimeofday() (from BSD) takes two arguments.
 * On some SVR4 derivatives, gettimeofday() takes only one argument.
 * The GETTIMEOFDAY macro is intended to hide this difference.
 */
#ifdef HAVE_SVID_GETTOD
#define GETTIMEOFDAY(tp) gettimeofday(tp)
#else
#define GETTIMEOFDAY(tp) gettimeofday((tp), NULL)
#endif

#endif /* prunixos_h___ */
