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

#ifndef primpl_h___
#define primpl_h___

/*
 * HP-UX 10.10's pthread.h (DCE threads) includes dce/cma.h, which
 * has:
 *     #define sigaction _sigaction_sys
 * This macro causes chaos if signal.h gets included before pthread.h.
 * To be safe, we include pthread.h first.
 */

#include <pthread.h>

#include "nspr.h"
#include "prpriv.h"

typedef struct PRSegment PRSegment;

#include "md/prosdep.h"

#ifdef _PR_HAVE_POSIX_SEMAPHORES
#include <semaphore.h>
#elif defined(_PR_HAVE_SYSV_SEMAPHORES)
#include <sys/sem.h>
#endif

/*************************************************************************
*****  A Word about Model Dependent Function Naming Convention ***********
*************************************************************************/

/*
NSPR 2.0 must implement its function across a range of platforms
including: MAC, Windows/16, Windows/95, Windows/NT, and several
variants of Unix. Each implementation shares common code as well
as having platform dependent portions. This standard describes how
the model dependent portions are to be implemented.

In header file pr/include/primpl.h, each publicly declared
platform dependent function is declared as:

NSPR_API void _PR_MD_FUNCTION( long arg1, long arg2 );
#define _PR_MD_FUNCTION _MD_FUNCTION

In header file pr/include/md/<platform>/_<platform>.h,
each #define'd macro is redefined as one of:

#define _MD_FUNCTION <blanks>
#define _MD_FUNCTION <expanded macro>
#define _MD_FUNCTION <osFunction>
#define _MD_FUNCTION <_MD_Function>

Where:

<blanks> is no definition at all. In this case, the function is not implemented
and is never called for this platform.
For example:
#define _MD_INIT_CPUS()

<expanded macro> is a C language macro expansion.
For example:
#define        _MD_CLEAN_THREAD(_thread) \
    PR_BEGIN_MACRO \
        PR_DestroyCondVar(_thread->md.asyncIOCVar); \
        PR_DestroyLock(_thread->md.asyncIOLock); \
    PR_END_MACRO

<osFunction> is some function implemented by the host operating system.
For example:
#define _MD_EXIT        exit

<_MD_function> is the name of a function implemented for this platform in
pr/src/md/<platform>/<soruce>.c file.
For example:
#define        _MD_GETFILEINFO         _MD_GetFileInfo

In <source>.c, the implementation is:
PR_IMPLEMENT(PRInt32) _MD_GetFileInfo(const char *fn, PRFileInfo *info);
*/

PR_BEGIN_EXTERN_C

typedef struct _MDLock _MDLock;
typedef struct _MDCVar _MDCVar;
typedef struct _MDSegment _MDSegment;
typedef struct _MDThread _MDThread;
typedef struct _MDThreadStack _MDThreadStack;
typedef struct _MDSemaphore _MDSemaphore;
typedef struct _MDDir _MDDir;
typedef struct _MDFileDesc _MDFileDesc;
typedef struct _MDProcess _MDProcess;
typedef struct _MDFileMap _MDFileMap;

/*
** The following definitions are unique to implementing NSPR using pthreads.
** Since pthreads defines most of the thread and thread synchronization
** stuff, this is a pretty small set.
*/

#define PT_CV_NOTIFIED_LENGTH 6
typedef struct _PT_Notified _PT_Notified;
struct _PT_Notified
{
    PRIntn length;              /* # of used entries in this structure */
    struct
    {
        PRCondVar *cv;          /* the condition variable notified */
        PRIntn times;           /* and the number of times notified */
    } cv[PT_CV_NOTIFIED_LENGTH];
    _PT_Notified *link;         /* link to another of these | NULL */
};

/*
 * bits defined for pthreads 'state' field
 */
#define PT_THREAD_DETACHED  0x01    /* thread can't be joined */
#define PT_THREAD_GLOBAL    0x02    /* a global thread (unlikely) */
#define PT_THREAD_SYSTEM    0x04    /* system (not user) thread */
#define PT_THREAD_PRIMORD   0x08    /* this is the primordial thread */
#define PT_THREAD_ABORTED   0x10    /* thread has been interrupted */
#define PT_THREAD_GCABLE    0x20    /* thread is garbage collectible */
#define PT_THREAD_SUSPENDED 0x40    /* thread has been suspended */
#define PT_THREAD_FOREIGN   0x80    /* thread is not one of ours */
#define PT_THREAD_BOUND     0x100    /* a bound-global thread */

#define _PT_THREAD_INTERRUPTED(thr)					\
		(!(thr->interrupt_blocked) && (thr->state & PT_THREAD_ABORTED))
#define _PT_THREAD_BLOCK_INTERRUPT(thr)				\
		(thr->interrupt_blocked = 1)
#define _PT_THREAD_UNBLOCK_INTERRUPT(thr)			\
		(thr->interrupt_blocked = 0)

#define _PT_IS_GCABLE_THREAD(thr) ((thr)->state & PT_THREAD_GCABLE)

/*
** Possible values for thread's suspend field
** Note that the first two can be the same as they are really mutually exclusive,
** i.e. both cannot be happening at the same time. We have two symbolic names
** just as a mnemonic.
**/
#define PT_THREAD_RESUMED   0x80    /* thread has been resumed */
#define PT_THREAD_SETGCABLE 0x100   /* set the GCAble flag */

/************************************************************************/
/*************************************************************************
** The remainder of the definitions are shared by pthreads and the classic
** NSPR code. These too may be conditionalized.
*************************************************************************/
/************************************************************************/

extern void _PR_InitThreads(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs);

struct PRLock {
    pthread_mutex_t mutex;          /* the underlying lock */
    _PT_Notified notified;          /* array of conditions notified */
    PRBool locked;                  /* whether the mutex is locked */
    pthread_t owner;                /* if locked, current lock owner */
};

extern void _PR_InitLocks(void);

struct PRCondVar {
    PRLock *lock;               /* associated lock that protects the condition */
    pthread_cond_t cv;          /* underlying pthreads condition */
    volatile int32_t notify_pending;     /* CV has destroy pending notification */
};

/************************************************************************/

struct PRMonitor {
    const char* name;           /* monitor name for debugging */
    PRLock lock;                /* the lock structure */
    pthread_t owner;            /* the owner of the lock or invalid */
    PRCondVar *cvar;            /* condition variable queue */
    PRUint32 entryCount;        /* # of times re-entered */
};

/************************************************************************/

struct PRSemaphore {
    PRCondVar *cvar;        /* associated lock and condition variable queue */
    PRUintn count;            /* the value of the counting semaphore */
    PRUint32 waiters;            /* threads waiting on the semaphore */
};

NSPR_API(void) _PR_InitSem(void);

/*************************************************************************/

struct PRSem {
#ifdef _PR_HAVE_POSIX_SEMAPHORES
    sem_t *sem;
#elif defined(_PR_HAVE_SYSV_SEMAPHORES)
    int semid;
#elif defined(WIN32)
    HANDLE sem;
#else
    PRInt8 notused;
#endif
};

/************************************************************************/

typedef void (PR_CALLBACK *_PRStartFn)(void *);

struct PRThread {
    PRUint32 state;                 /* thread's creation state */
    PRThreadPriority priority;      /* apparent priority, loosly defined */

    void *arg;                      /* argument to the client's entry point */
    _PRStartFn startFunc;           /* the root of the client's thread */

    void *environment;              /* pointer to execution environment */

    /*
    ** Per thread private data
    */
    PRUint32 tpdLength;             /* thread's current vector length */
    void **privateData;             /* private data vector or NULL */

    pthread_t id;                   /* pthread identifier for the thread */
    PRBool okToDelete;              /* ok to delete the PRThread struct? */
    PRCondVar *waiting;             /* where the thread is waiting | NULL */
    void *sp;                       /* recorded sp for garbage collection */
    PRThread *next, *prev;          /* simple linked list of all threads */
    PRUint32 suspend;               /* used to store suspend and resume flags */
#ifdef PT_NO_SIGTIMEDWAIT
    pthread_mutex_t suspendResumeMutex;
    pthread_cond_t suspendResumeCV;
#endif
    PRUint32 interrupt_blocked;     /* interrupt blocked */
    struct pollfd *syspoll_list;    /* Unix polling list used by PR_Poll */
    PRUint32 syspoll_count;         /* number of elements in syspoll_list */
};

struct PRProcess {
    _MDProcess md;
};

/************************************************************************/

extern void _PR_InitSegs(void);
extern void _PR_InitStacks(void);
extern void _PR_InitMem(void);
extern void _PR_InitIO(void);
extern void _PR_InitLog(void);
extern void _PR_InitNet(void);
extern void _PR_InitClock(void);
extern void _PR_InitCPUs(void);
extern void _PR_InitMW(void);
extern void _PR_NotifyCondVar(PRCondVar *cvar, PRThread *me);
extern void _PR_CleanupThread(PRThread *thread);
extern void _PR_CleanupMW(void);
extern void _PR_CleanupIO(void);
extern void _PR_CleanupNet(void);
extern void _PR_CleanupLayerCache(void);
extern void _PR_CleanupStacks(void);
#ifdef WINNT
extern void _PR_CleanupCPUs(void);
#endif
extern void _PR_CleanupThreads(void);
extern void _PR_CleanupTPD(void);
extern void _PR_Cleanup(void);
extern void _PR_LogCleanup(void);
extern void _PR_InitLayerCache(void);

extern PRBool _pr_initialized;
extern void _PR_ImplicitInitialization(void);

/************************************************************************/

struct PRSegment {
    void *vaddr;
    PRUint32 size;
    PRUintn flags;
};

/* PRSegment.flags */
#define _PR_SEG_VM    0x1

/************************************************************************/

extern PRInt32 _pr_pageSize;
extern PRInt32 _pr_pageShift;

/*************************************************************************
* External machine-dependent code provided by each OS.                     *                                                                     *
*************************************************************************/

/* Initialization related */
extern void _PR_MD_EARLY_INIT(void);
#define    _PR_MD_EARLY_INIT _MD_EARLY_INIT

extern void _PR_MD_INTERVAL_INIT(void);
#define    _PR_MD_INTERVAL_INIT _MD_INTERVAL_INIT

NSPR_API(void) _PR_MD_FINAL_INIT(void);
#define    _PR_MD_FINAL_INIT _MD_FINAL_INIT

/* Process control */

#ifdef _MD_CREATE_PROCESS_DETACHED
# define _PR_MD_CREATE_PROCESS_DETACHED _MD_CREATE_PROCESS_DETACHED
#endif

/* Current Time */
NSPR_API(PRTime) _PR_MD_NOW(void);
#define    _PR_MD_NOW _MD_NOW

/* Environment related */
extern char* _PR_MD_GET_ENV(const char *name);
#define    _PR_MD_GET_ENV _MD_GET_ENV

extern PRIntn _PR_MD_PUT_ENV(const char *name);
#define    _PR_MD_PUT_ENV _MD_PUT_ENV

/* Atomic operations */

extern void _PR_MD_INIT_ATOMIC(void);
#define    _PR_MD_INIT_ATOMIC _MD_INIT_ATOMIC

extern PRInt32 _PR_MD_ATOMIC_INCREMENT(PRInt32 *);
#define    _PR_MD_ATOMIC_INCREMENT _MD_ATOMIC_INCREMENT

extern PRInt32 _PR_MD_ATOMIC_ADD(PRInt32 *, PRInt32);
#define    _PR_MD_ATOMIC_ADD _MD_ATOMIC_ADD

extern PRInt32 _PR_MD_ATOMIC_DECREMENT(PRInt32 *);
#define    _PR_MD_ATOMIC_DECREMENT _MD_ATOMIC_DECREMENT

extern PRInt32 _PR_MD_ATOMIC_SET(PRInt32 *, PRInt32);
#define    _PR_MD_ATOMIC_SET _MD_ATOMIC_SET

/* Time intervals */

extern PRIntervalTime _PR_MD_GET_INTERVAL(void);
#define _PR_MD_GET_INTERVAL _MD_GET_INTERVAL

extern PRIntervalTime _PR_MD_INTERVAL_PER_SEC(void);
#define _PR_MD_INTERVAL_PER_SEC _MD_INTERVAL_PER_SEC

/* Affinity masks */

extern PRInt32 _PR_MD_SETTHREADAFFINITYMASK(PRThread *thread, PRUint32 mask );
#define _PR_MD_SETTHREADAFFINITYMASK _MD_SETTHREADAFFINITYMASK

extern PRInt32 _PR_MD_GETTHREADAFFINITYMASK(PRThread *thread, PRUint32 *mask);
#define _PR_MD_GETTHREADAFFINITYMASK _MD_GETTHREADAFFINITYMASK

/* File locking */

extern PRStatus _PR_MD_LOCKFILE(PRInt32 osfd);
#define    _PR_MD_LOCKFILE _MD_LOCKFILE

extern PRStatus _PR_MD_TLOCKFILE(PRInt32 osfd);
#define    _PR_MD_TLOCKFILE _MD_TLOCKFILE

extern PRStatus _PR_MD_UNLOCKFILE(PRInt32 osfd);
#define    _PR_MD_UNLOCKFILE _MD_UNLOCKFILE

/* Socket call error code */

NSPR_API(PRInt32) _PR_MD_GET_SOCKET_ERROR(void);
#define    _PR_MD_GET_SOCKET_ERROR _MD_GET_SOCKET_ERROR

/* Get name of current host */
extern PRStatus _PR_MD_GETHOSTNAME(char *name, PRUint32 namelen);
#define    _PR_MD_GETHOSTNAME _MD_GETHOSTNAME

PR_END_EXTERN_C

#endif /* primpl_h___ */
