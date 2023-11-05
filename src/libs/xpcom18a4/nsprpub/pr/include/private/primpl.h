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

#ifdef XP_MAC
#include "prosdep.h"
#include "probslet.h"
#else
#include "md/prosdep.h"
#include "obsolete/probslet.h"
#endif  /* XP_MAC */

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

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PT_FPrintStats VBoxNsprPT_FPrintStats
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

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

#if defined(DEBUG)

typedef struct PTDebug
{
    PRTime timeStarted;
    PRUintn locks_created, locks_destroyed;
    PRUintn locks_acquired, locks_released;
    PRUintn cvars_created, cvars_destroyed;
    PRUintn cvars_notified, delayed_cv_deletes;
} PTDebug;

#endif /* defined(DEBUG) */

NSPR_API(void) PT_FPrintStats(PRFileDesc *fd, const char *msg);

/************************************************************************/
/*************************************************************************
** The remainder of the definitions are shared by pthreads and the classic
** NSPR code. These too may be conditionalized.
*************************************************************************/
/************************************************************************/

extern PROffset32 _PR_MD_LSEEK(PRFileDesc *fd, PROffset32 offset, PRSeekWhence whence);
#define    _PR_MD_LSEEK _MD_LSEEK

extern PROffset64 _PR_MD_LSEEK64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence whence);
#define    _PR_MD_LSEEK64 _MD_LSEEK64

extern PRInt32 _PR_MD_GETFILEINFO(const char *fn, PRFileInfo *info);
#define _PR_MD_GETFILEINFO _MD_GETFILEINFO

extern PRInt32 _PR_MD_GETFILEINFO64(const char *fn, PRFileInfo64 *info);
#define _PR_MD_GETFILEINFO64 _MD_GETFILEINFO64

extern PRInt32 _PR_MD_GETOPENFILEINFO(const PRFileDesc *fd, PRFileInfo *info);
#define _PR_MD_GETOPENFILEINFO _MD_GETOPENFILEINFO

extern PRInt32 _PR_MD_GETOPENFILEINFO64(const PRFileDesc *fd, PRFileInfo64 *info);
#define _PR_MD_GETOPENFILEINFO64 _MD_GETOPENFILEINFO64


/*****************************************************************************/
/************************** File descriptor caching **************************/
/*****************************************************************************/
extern void _PR_InitFdCache(void);
extern void _PR_CleanupFdCache(void);
extern PRFileDesc *_PR_Getfd(void);
extern void _PR_Putfd(PRFileDesc *fd);

/*
 * These flags are used by NSPR temporarily in the poll
 * descriptor's out_flags field to record the mapping of
 * NSPR's poll flags to the system poll flags.
 *
 * If _PR_POLL_READ_SYS_WRITE bit is set, it means the
 * PR_POLL_READ flag specified by the topmost layer is
 * mapped to the WRITE flag at the system layer.  Similarly
 * for the other three _PR_POLL_XXX_SYS_YYY flags.  It is
 * assumed that the PR_POLL_EXCEPT flag doesn't get mapped
 * to other flags.
 */
#define _PR_POLL_READ_SYS_READ     0x1
#define _PR_POLL_READ_SYS_WRITE    0x2
#define _PR_POLL_WRITE_SYS_READ    0x4
#define _PR_POLL_WRITE_SYS_WRITE   0x8

/*
** These methods are coerced into file descriptor methods table
** when the intended service is inappropriate for the particular
** type of file descriptor.
*/
extern PRIntn _PR_InvalidInt(void);
extern PRInt16 _PR_InvalidInt16(void);
extern PRInt64 _PR_InvalidInt64(void);
extern PRStatus _PR_InvalidStatus(void);
extern PRFileDesc *_PR_InvalidDesc(void);

extern PRIOMethods _pr_faulty_methods;

/*
** The PR_NETADDR_SIZE macro can only be called on a PRNetAddr union
** whose 'family' field is set.  It returns the size of the union
** member corresponding to the specified address family.
*/

extern PRUintn _PR_NetAddrSize(const PRNetAddr* addr);

#if defined(_PR_INET6)

#define PR_NETADDR_SIZE(_addr) _PR_NetAddrSize(_addr)

#elif defined(_PR_HAVE_MD_SOCKADDR_IN6)

/*
** Under the following conditions:
** 1. _PR_INET6 is not defined;
** 2. _PR_INET6_PROBE is defined;
** 3. struct sockaddr_in6 has nonstandard fields at the end
**    (e.g., on Solaris 8),
** (_addr)->ipv6 is smaller than struct sockaddr_in6, and
** hence we can't pass sizeof((_addr)->ipv6) to socket
** functions such as connect because they would fail with
** EINVAL.
**
** To pass the correct socket address length to socket
** functions, define the macro _PR_HAVE_MD_SOCKADDR_IN6 and
** define struct _md_sockaddr_in6 to be isomorphic to
** struct sockaddr_in6.
*/

#if defined(XP_UNIX) || defined(XP_OS2_EMX)
#define PR_NETADDR_SIZE(_addr) 					\
        ((_addr)->raw.family == PR_AF_INET		\
        ? sizeof((_addr)->inet)					\
        : ((_addr)->raw.family == PR_AF_INET6	\
        ? sizeof(struct _md_sockaddr_in6)		\
        : sizeof((_addr)->local)))
#else
#define PR_NETADDR_SIZE(_addr) 					\
        ((_addr)->raw.family == PR_AF_INET		\
        ? sizeof((_addr)->inet)					\
        : sizeof(struct _md_sockaddr_in6)
#endif /* defined(XP_UNIX) */

#else

#if defined(XP_UNIX) || defined(XP_OS2_EMX)
#define PR_NETADDR_SIZE(_addr) 					\
        ((_addr)->raw.family == PR_AF_INET		\
        ? sizeof((_addr)->inet)					\
        : ((_addr)->raw.family == PR_AF_INET6	\
        ? sizeof((_addr)->ipv6)					\
        : sizeof((_addr)->local)))
#else
#define PR_NETADDR_SIZE(_addr) 					\
        ((_addr)->raw.family == PR_AF_INET		\
        ? sizeof((_addr)->inet)					\
        : sizeof((_addr)->ipv6))
#endif /* defined(XP_UNIX) */

#endif /* defined(_PR_INET6) */

extern PRStatus _PR_MapOptionName(
    PRSockOption optname, PRInt32 *level, PRInt32 *name);
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
    PRInt32 notify_pending;     /* CV has destroy pending notification */
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

/*************************************************************************/

struct PRStackStr {
    /* head MUST be at offset 0; assembly language code relies on this */
#if defined(AIX)
    volatile PRStackElem prstk_head;
#else
    PRStackElem prstk_head;
#endif

    PRLock *prstk_lock;
    char *prstk_name;
};

/************************************************************************/

/* XXX this needs to be exported (sigh) */
struct PRThreadStack {
    PRCList links;
    PRUintn flags;

    char *allocBase;            /* base of stack's allocated memory */
    PRUint32 allocSize;         /* size of stack's allocated memory */
    char *stackBottom;          /* bottom of stack from C's point of view */
    char *stackTop;             /* top of stack from C's point of view */
    PRUint32 stackSize;         /* size of usable portion of the stack */

    PRSegment *seg;
        PRThread* thr;          /* back pointer to thread owning this stack */
};

extern void _PR_DestroyThreadPrivate(PRThread*);

typedef void (PR_CALLBACK *_PRStartFn)(void *);

struct PRThread {
    PRUint32 state;                 /* thread's creation state */
    PRThreadPriority priority;      /* apparent priority, loosly defined */

    void *arg;                      /* argument to the client's entry point */
    _PRStartFn startFunc;           /* the root of the client's thread */

    PRThreadStack *stack;           /* info about thread's stack (for GC) */
    void *environment;              /* pointer to execution environment */

    PRThreadDumpProc dump;          /* dump thread info out */
    void *dumpArg;                  /* argument for the dump function */

    /*
    ** Per thread private data
    */
    PRUint32 tpdLength;             /* thread's current vector length */
    void **privateData;             /* private data vector or NULL */
    PRErrorCode errorCode;          /* current NSPR error code | zero */
    PRInt32 osErrorCode;            /* mapping of errorCode | zero */
    PRIntn  errorStringLength;      /* textLength from last call to PR_SetErrorText() */
    PRInt32 errorStringSize;        /* malloc()'d size of buffer | zero */
    char *errorString;              /* current error string | NULL */

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
#if defined(_PR_POLL_WITH_SELECT)
    int *selectfd_list;             /* Unix fd's that PR_Poll selects on */
    PRUint32 selectfd_count;        /* number of elements in selectfd_list */
#endif
};

struct PRProcessAttr {
    PRFileDesc *stdinFd;
    PRFileDesc *stdoutFd;
    PRFileDesc *stderrFd;
    char *currentDirectory;
    char *fdInheritBuffer;
    PRSize fdInheritBufferSize;
    PRSize fdInheritBufferUsed;
};

struct PRProcess {
    _MDProcess md;
};

struct PRFileMap {
    PRFileDesc *fd;
    PRFileMapProtect prot;
    _MDFileMap md;
};

/************************************************************************/

/*
** File descriptors of the NSPR layer can be in one of the
** following states (stored in the 'state' field of struct
** PRFilePrivate):
** - _PR_FILEDESC_OPEN: The OS fd is open.
** - _PR_FILEDESC_CLOSED: The OS fd is closed.  The PRFileDesc
**   is still open but is unusable.  The only operation allowed
**   on the PRFileDesc is PR_Close().
** - _PR_FILEDESC_FREED: The OS fd is closed and the PRFileDesc
**   structure is freed.
*/

#define _PR_FILEDESC_OPEN       0xaaaaaaaa    /* 1010101... */
#define _PR_FILEDESC_CLOSED     0x55555555    /* 0101010... */
#define _PR_FILEDESC_FREED      0x11111111

/*
** A boolean type with an additional "unknown" state
*/

typedef enum {
    _PR_TRI_TRUE = 1,
    _PR_TRI_FALSE = 0,
    _PR_TRI_UNKNOWN = -1
} _PRTriStateBool;

struct PRFilePrivate {
    PRInt32 state;
    PRBool nonblocking;
    _PRTriStateBool inheritable;
    PRFileDesc *next;
    PRIntn lockCount;   /*   0: not locked
                         *  -1: a native lockfile call is in progress
                         * > 0: # times the file is locked */
#ifdef _PR_HAVE_PEEK_BUFFER
    char *peekBuffer;
    PRInt32 peekBufSize;
    PRInt32 peekBytes;
#endif
#if !defined(XP_UNIX)   /* BugZilla: 4090 */
    PRBool  appendMode;
#endif
    _MDFileDesc md;
#ifdef _PR_STRICT_ADDR_LEN
    PRUint16 af;        /* If the platform requires passing the exact
                         * length of the sockaddr structure for the
                         * address family of the socket to socket
                         * functions like accept(), we need to save
                         * the address family of the socket. */
#endif
};

struct PRDir {
    PRDirEntry d;
    _MDDir md;
};

extern void _PR_InitSegs(void);
extern void _PR_InitStacks(void);
extern void _PR_InitTPD(void);
extern void _PR_InitMem(void);
extern void _PR_InitEnv(void);
extern void _PR_InitCMon(void);
extern void _PR_InitIO(void);
extern void _PR_InitLog(void);
extern void _PR_InitNet(void);
extern void _PR_InitClock(void);
extern void _PR_InitLinker(void);
extern void _PR_InitAtomic(void);
extern void _PR_InitCPUs(void);
extern void _PR_InitDtoa(void);
extern void _PR_InitMW(void);
extern void _PR_NotifyCondVar(PRCondVar *cvar, PRThread *me);
extern void _PR_CleanupThread(PRThread *thread);
extern void _PR_CleanupCallOnce(void);
extern void _PR_CleanupMW(void);
extern void _PR_CleanupDtoa(void);
extern void _PR_ShutdownLinker(void);
extern void _PR_CleanupEnv(void);
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
extern PRBool _PR_Obsolete(const char *obsolete, const char *preferred);

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

extern PRLogModuleInfo *_pr_clock_lm;
extern PRLogModuleInfo *_pr_cmon_lm;
extern PRLogModuleInfo *_pr_io_lm;
extern PRLogModuleInfo *_pr_cvar_lm;
extern PRLogModuleInfo *_pr_mon_lm;
extern PRLogModuleInfo *_pr_linker_lm;
extern PRLogModuleInfo *_pr_sched_lm;
extern PRLogModuleInfo *_pr_thread_lm;
extern PRLogModuleInfo *_pr_gc_lm;

extern PRFileDesc *_pr_stdin;
extern PRFileDesc *_pr_stdout;
extern PRFileDesc *_pr_stderr;


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

extern PRProcess * _PR_MD_CREATE_PROCESS(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr);
#define    _PR_MD_CREATE_PROCESS _MD_CREATE_PROCESS

#ifdef _MD_CREATE_PROCESS_DETACHED
# define _PR_MD_CREATE_PROCESS_DETACHED _MD_CREATE_PROCESS_DETACHED
#endif

extern PRStatus _PR_MD_DETACH_PROCESS(PRProcess *process);
#define    _PR_MD_DETACH_PROCESS _MD_DETACH_PROCESS

extern PRStatus _PR_MD_WAIT_PROCESS(PRProcess *process, PRInt32 *exitCode);
#define    _PR_MD_WAIT_PROCESS _MD_WAIT_PROCESS

extern PRStatus _PR_MD_KILL_PROCESS(PRProcess *process);
#define    _PR_MD_KILL_PROCESS _MD_KILL_PROCESS

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

/* Memory-mapped files */

extern PRStatus _PR_MD_CREATE_FILE_MAP(PRFileMap *fmap, PRInt64 size);
#define _PR_MD_CREATE_FILE_MAP _MD_CREATE_FILE_MAP

extern PRInt32 _PR_MD_GET_MEM_MAP_ALIGNMENT(void);
#define _PR_MD_GET_MEM_MAP_ALIGNMENT _MD_GET_MEM_MAP_ALIGNMENT

extern void * _PR_MD_MEM_MAP(
    PRFileMap *fmap,
    PROffset64 offset,
    PRUint32 len);
#define _PR_MD_MEM_MAP _MD_MEM_MAP

extern PRStatus _PR_MD_MEM_UNMAP(void *addr, PRUint32 size);
#define _PR_MD_MEM_UNMAP _MD_MEM_UNMAP

extern PRStatus _PR_MD_CLOSE_FILE_MAP(PRFileMap *fmap);
#define _PR_MD_CLOSE_FILE_MAP _MD_CLOSE_FILE_MAP

/* Named Shared Memory */

/*
** Declare PRSharedMemory.
*/
struct PRSharedMemory
{
    char        *ipcname; /* after conversion to native */
    PRSize      size;  /* from open */
    PRIntn      mode;  /* from open */
    PRIntn      flags; /* from open */
#if defined(PR_HAVE_POSIX_NAMED_SHARED_MEMORY)
    int         id;
#elif defined(PR_HAVE_SYSV_NAMED_SHARED_MEMORY)
    int         id;
#elif defined(PR_HAVE_WIN32_NAMED_SHARED_MEMORY)
    HANDLE      handle;
#else
    PRUint32    nothing; /* placeholder, nothing behind here */
#endif
    PRUint32    ident; /* guard word at end of struct */
#define _PR_SHM_IDENT 0xdeadbad
};

extern PRSharedMemory * _MD_OpenSharedMemory(
    const char *name,
    PRSize      size,
    PRIntn      flags,
    PRIntn      mode
);
#define _PR_MD_OPEN_SHARED_MEMORY _MD_OpenSharedMemory

extern void * _MD_AttachSharedMemory( PRSharedMemory *shm, PRIntn flags );
#define _PR_MD_ATTACH_SHARED_MEMORY _MD_AttachSharedMemory

extern PRStatus _MD_DetachSharedMemory( PRSharedMemory *shm, void *addr );
#define _PR_MD_DETACH_SHARED_MEMORY _MD_DetachSharedMemory

extern PRStatus _MD_CloseSharedMemory( PRSharedMemory *shm );
#define _PR_MD_CLOSE_SHARED_MEMORY _MD_CloseSharedMemory

extern PRStatus _MD_DeleteSharedMemory( const char *name );
#define _PR_MD_DELETE_SHARED_MEMORY  _MD_DeleteSharedMemory

extern PRFileMap* _md_OpenAnonFileMap(
    const char *dirName,
    PRSize      size,
    PRFileMapProtect prot
);
#define _PR_MD_OPEN_ANON_FILE_MAP _md_OpenAnonFileMap

extern PRStatus _md_ExportFileMapAsString(
    PRFileMap *fm,
    PRSize    bufSize,
    char      *buf
);
#define _PR_MD_EXPORT_FILE_MAP_AS_STRING _md_ExportFileMapAsString

extern PRFileMap * _md_ImportFileMapFromString(
    const char *fmstring
);
#define _PR_MD_IMPORT_FILE_MAP_FROM_STRING _md_ImportFileMapFromString



/* Interprocess communications (IPC) */

/*
 * The maximum length of an NSPR IPC name, including the
 * terminating null byte.
 */
#define PR_IPC_NAME_SIZE 1024

/*
 * Types of NSPR IPC objects
 */
typedef enum {
    _PRIPCSem,  /* semaphores */
    _PRIPCShm   /* shared memory segments */
} _PRIPCType;

/*
 * Make a native IPC name from an NSPR IPC name.
 */
extern PRStatus _PR_MakeNativeIPCName(
    const char *name,  /* NSPR IPC name */
    char *result,      /* result buffer */
    PRIntn size,       /* size of result buffer */
    _PRIPCType type    /* type of IPC object */
);

/* Socket call error code */

NSPR_API(PRInt32) _PR_MD_GET_SOCKET_ERROR(void);
#define    _PR_MD_GET_SOCKET_ERROR _MD_GET_SOCKET_ERROR

/* Get name of current host */
extern PRStatus _PR_MD_GETHOSTNAME(char *name, PRUint32 namelen);
#define    _PR_MD_GETHOSTNAME _MD_GETHOSTNAME

/* File descriptor inheritance */

/*
 * If fd->secret->inheritable is _PR_TRI_UNKNOWN and we need to
 * know the inheritable attribute of the fd, call this function
 * to find that out.  This typically requires a system call.
 */
extern void _PR_MD_QUERY_FD_INHERITABLE(PRFileDesc *fd);
#define    _PR_MD_QUERY_FD_INHERITABLE _MD_QUERY_FD_INHERITABLE

#ifdef XP_BEOS

extern PRLock *_connectLock;

typedef struct _ConnectListNode {
	PRInt32		osfd;
	PRNetAddr	addr;
	PRUint32	addrlen;
	PRIntervalTime	timeout;
} ConnectListNode;

extern ConnectListNode connectList[64];

extern PRUint32 connectCount;

#endif /* XP_BEOS */

PR_END_EXTERN_C

#endif /* primpl_h___ */
