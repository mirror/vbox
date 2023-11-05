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

#ifndef nspr_solaris_defs_h___
#define nspr_solaris_defs_h___

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"solaris"
#define _PR_SI_SYSNAME	"SOLARIS"
#ifdef sparc
#define _PR_SI_ARCHITECTURE	"sparc"
#elif defined(i386)
#define _PR_SI_ARCHITECTURE	"x86"
#elif defined(__x86_64)
#define _PR_SI_ARCHITECTURE     "x86-64"
#else
#error unknown processor
#endif
#define PR_DLL_SUFFIX		".so"

#define _PR_VMBASE		0x30000000
#define _PR_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	(2*65536L)
#define _MD_MMAP_FLAGS          MAP_SHARED

#undef  HAVE_STACK_GROWING_UP

#ifndef HAVE_WEAK_IO_SYMBOLS
#define	HAVE_WEAK_IO_SYMBOLS
#endif

#undef	HAVE_WEAK_MALLOC_SYMBOLS
#define	HAVE_DLL
#define	USE_DLFCN
#define NEED_STRFTIME_LOCK

#if defined(_LARGEFILE64_SOURCE)                                    /* vbox */
#define _PR_HAVE_OFF64_T                                            /* vbox */
#elif defined(_LP64) || _FILE_OFFSET_BITS == 32                     /* vbox */
#define _PR_HAVE_LARGE_OFF_T                                        /* vbox */
#else                                                               /* vbox */
#define _PR_NO_LARGE_FILES                                          /* vbox */
#endif                                                              /* vbox */


/*
 * Intel x86 has atomic instructions.
 *
 * Sparc v8 does not have instructions to efficiently implement
 * atomic increment/decrement operations.  In the local threads
 * only and pthreads versions, we use the default atomic routine
 * implementation in pratom.c.  The obsolete global threads only
 * version uses a global mutex_t to implement the atomic routines
 * in solaris.c, which is actually equivalent to the default
 * implementation.
 *
 * 64-bit Solaris requires sparc v9, which has atomic instructions.
 */
#if defined(i386) || defined(IS_64)
#define _PR_HAVE_ATOMIC_OPS
#endif

#if defined(_PR_PTHREADS)
/*
 * We have assembly language implementation of atomic
 * stacks for the 32-bit sparc and x86 architectures only.
 *
 * Note: We ran into thread starvation problem with the
 * 32-bit sparc assembly language implementation of atomic
 * stacks, so we do not use it now. (Bugzilla bug 113740)
 */
#if !defined(sparc) && !defined(__x86_64)
#define _PR_HAVE_ATOMIC_CAS
#endif
#endif

#define _PR_POLL_AVAILABLE
#define _PR_USE_POLL
#define _PR_STAT_HAS_ST_ATIM
#ifdef SOLARIS2_5
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY
#else
#define _PR_HAVE_POSIX_SEMAPHORES
#define PR_HAVE_POSIX_NAMED_SHARED_MEMORY
#endif
#define _PR_HAVE_GETIPNODEBYNAME
#define _PR_HAVE_GETIPNODEBYADDR
#define _PR_HAVE_GETADDRINFO
#define _PR_INET6_PROBE
#define _PR_ACCEPT_INHERIT_NONBLOCK
#if !defined(_XPG4_2) || defined(_XPG6) || defined(__EXTENSIONS__)  /* vbox */
#define _PR_INET6                                                   /* vbox */
#endif                                                              /* vbox */
#ifdef _PR_INET6
#define _PR_HAVE_INET_NTOP
#else
#define AF_INET6 26
struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;
};
#define AI_CANONNAME 0x0010
#define AI_V4MAPPED 0x0001 
#define AI_ALL      0x0002
#define AI_ADDRCONFIG   0x0004
#define _PR_HAVE_MD_SOCKADDR_IN6
/* isomorphic to struct in6_addr on Solaris 8 */
struct _md_in6_addr {
    union {
        PRUint8  _S6_u8[16];
        PRUint32 _S6_u32[4];
        PRUint32 __S6_align;
    } _S6_un;
};
/* isomorphic to struct sockaddr_in6 on Solaris 8 */
struct _md_sockaddr_in6 {
    PRUint16 sin6_family;
    PRUint16 sin6_port;
    PRUint32 sin6_flowinfo;
    struct _md_in6_addr sin6_addr;
    PRUint32 sin6_scope_id;
    PRUint32 __sin6_src_id;
};
#endif
#if defined(_PR_PTHREADS)
#define _PR_HAVE_GETHOST_R
#define _PR_HAVE_GETHOST_R_POINTER
#endif

#include "prinrval.h"
NSPR_API(PRIntervalTime) _MD_Solaris_GetInterval(void);
#define _MD_GET_INTERVAL                  _MD_Solaris_GetInterval
NSPR_API(PRIntervalTime) _MD_Solaris_TicksPerSecond(void);
#define _MD_INTERVAL_PER_SEC              _MD_Solaris_TicksPerSecond

#include "_iprt_atomic.h"

NSPR_API(void)		_MD_EarlyInit(void);

#define _MD_EARLY_INIT		_MD_EarlyInit
#define _MD_FINAL_INIT		_PR_UnixInit

extern void _MD_solaris_map_sendfile_error(int err);

#endif /* nspr_solaris_defs_h___ */

