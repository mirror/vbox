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

#ifndef nspr_l4v2_defs_h___
#define nspr_l4v2_defs_h___

/**
 * I have "#if 0"ed a lot of this out, and plan to add most of it manually as the
 * compiler complains about it in order to get a better idea of what is here and
 * what it does.  Not to mention which bits are needed by the runtime itself, and
 * which only by the unix port.
 */

#include "prthread.h"

/*
 * Internal configuration macros
 */

#define PR_LINKER_ARCH	"l4v2"
#define _PR_SI_SYSNAME  "L4ENV"
#define _PR_SI_ARCHITECTURE "x86"
#define PR_DLL_SUFFIX		".s.so"

#define _PR_VMBASE              0x30000000  /* not used */
#define _PR_STACK_VMBASE	0x50000000  /* only used in an unused fn */
#define _MD_DEFAULT_STACK_SIZE	65536L
#define _MD_MMAP_FLAGS          MAP_PRIVATE

#undef	HAVE_STACK_GROWING_UP

/*
 * L4env supports dl* functions
 */
#define HAVE_DLL
#define USE_DLFCN

#define USE_SETJMP
#undef _PR_USE_POLL
#define _PR_STAT_HAS_ONLY_ST_ATIME
#define _PR_HAVE_OFF64_T

#include <setjmp.h>

#define PR_CONTEXT_TYPE	jmp_buf

#define CONTEXT(_th) ((_th)->md.context)

/* Now come the things specifically added for the l4v2 target */

/* Don't know quite what this means yet, except that we don't have "it" */
/* #define TCP_NODELAY         _PR_NO_SUCH_SOCKOPT */

/* And what is this? Something to do with memory... Used in
  xpcom18a4/nsprpub/pr/src/memory/prseg.c */
/**
 * definitions and macros for l4env/memory.c
 */

struct _MDSegment {
    PRInt8 notused;
};


extern void
_MD_InitSegs(void);
#define _MD_INIT_SEGS     _MD_InitSegs

extern PRStatus
_MD_AllocSegment(PRSegment *seg, PRUint32 size, void *vaddr);
#define _MD_ALLOC_SEGMENT _MD_AllocSegment

extern void
_MD_FreeSegment(PRSegment *seg);
#define _MD_FREE_SEGMENT  _MD_FreeSegment

/**
 * definitions and macros for l4env/clock.c
 */
extern void
_MD_IntervalInit(void);
#define _MD_INTERVAL_INIT()

/**
 * definitions and macros for l4env/l4env.c
 */

/* See xpcom18a4/nsprpub/pr/src/misc/prinit.c regarding the following function */
extern void
_MD_EarlyInit(void);
#define _MD_EARLY_INIT()               _MD_EarlyInit()

extern void
_MD_StartInterrupts(void);

extern void
_MD_StopInterrupts(void);

extern void
_MD_DisableClockInterrupts(void);

extern void
_MD_EnableClockInterrupts(void);

extern void
_MD_BlockClockInterrupts(void);

extern void
_MD_UnblockClockInterrupts(void);

#define __USE_SVID  /* for now, to get putenv */
#include <stdlib.h>

/**
 * definitions and macros for l4env/atomic.c
 */
#if defined(__i386__)
#define _PR_HAVE_ATOMIC_OPS
#define _MD_INIT_ATOMIC()

extern PRInt32
_PR_x86_AtomicIncrement(PRInt32 *val);
#define _MD_ATOMIC_INCREMENT          _PR_x86_AtomicIncrement

extern PRInt32
_PR_x86_AtomicDecrement(PRInt32 *val);
#define _MD_ATOMIC_DECREMENT          _PR_x86_AtomicDecrement

extern PRInt32
_PR_x86_AtomicAdd(PRInt32 *ptr, PRInt32 val);

#define _MD_ATOMIC_ADD                _PR_x86_AtomicAdd
extern PRInt32
_PR_x86_AtomicSet(PRInt32 *val, PRInt32 newval);
#define _MD_ATOMIC_SET                _PR_x86_AtomicSet
#endif

/**
 * definitions and macros for l4env/threads.c
 */
#define _PR_LOCAL_THREADS_ONLY
#if 0
struct _MDThread {
    PRInt8 notused;
};

struct _MDThreadStack {
    PRInt8 notused;
};
#endif /* 0 */

#ifndef _PR_LOCAL_THREADS_ONLY
extern PR_IMPLEMENT(PRThread*)
PR_GetCurrentThread(void);
#define _MD_CURRENT_THREAD()     PR_GetCurrentThread()
#endif /* _PR_LOCAL_THREADS_ONLY */

#define _MD_EXIT_THREAD(thread)

#ifndef _PR_LOCAL_THREADS_ONLY
extern PR_IMPLEMENT(struct _PRCPU *)
PR_GetCurrentCPU(void);
#define _MD_CURRENT_CPU()        PR_GetCurrentCPU()
#endif /* _PR_LOCAL_THREADS_ONLY */

#if 0
#define _L4_GET_NUMBER_OF_CPUS() 1
#define _MD_SET_INTSOFF(_val)
#define _MD_GET_INTSOFF()	 1
#endif /* 0 */

#if 0  /* these are the Win32 versions of the above macros - all other versions are defined 
          as here. */
#define _MD_GET_INTSOFF() \
    (_pr_use_static_tls ? _pr_ints_off \
    : (PRUintn) TlsGetValue(_pr_intsOffIndex))

#define _MD_SET_INTSOFF(_val) \
    PR_BEGIN_MACRO \
        if (_pr_use_static_tls) { \
            _pr_ints_off = (_val); \
        } else { \
            TlsSetValue(_pr_intsOffIndex, (LPVOID) (_val)); \
        } \
    PR_END_MACRO
#endif /* 0 */

/**
 * definitions and macros for file l4env/mmap.c
 */

/* Memory-mapped files */

extern PRStatus
_MD_CreateFileMap(struct PRFileMap *fmap, PRInt64 size);
#define _MD_CREATE_FILE_MAP _MD_CreateFileMap

extern PRStatus
_MD_CloseFileMap(struct PRFileMap *fmap);
#define _MD_CLOSE_FILE_MAP _MD_CloseFileMap

#define _MD_GET_MEM_MAP_ALIGNMENT() PR_GetPageSize()

extern void *
_MD_MemMap(struct PRFileMap *fmap, PRInt64 offset,
        PRUint32 len);
#define _MD_MEM_MAP _MD_MemMap

extern PRStatus
_MD_MemUnmap(void *addr, PRUint32 size);
#define _MD_MEM_UNMAP _MD_MemUnmap

/**
 * definitions and macros for file l4env/fileio.c
 */
#define PR_DIRECTORY_SEPARATOR		'/'
#define PR_DIRECTORY_SEPARATOR_STR	"/"
#define PR_PATH_SEPARATOR		':'
#define PR_PATH_SEPARATOR_STR		":"
#define GCPTR

#if 0
typedef int (*FARPROC)();  /* Where is this used? */
#endif

extern PRInt32
_MD_write(PRFileDesc *fd, const void *buf, PRInt32 amount);
#define _MD_WRITE(fd,buf,amount)	    _MD_write(fd,buf,amount)

extern void
_MD_query_fd_inheritable(PRFileDesc *fd);
#define _MD_QUERY_FD_INHERITABLE _MD_query_fd_inheritable

/**
 * definitions and macros for file l4env/sockets.c
 */
extern PRStatus
_MD_getsockopt(PRFileDesc *fd, PRInt32 level,
               PRInt32 optname, char* optval, PRInt32* optlen);
#define _MD_GETSOCKOPT		_MD_getsockopt
extern PRStatus
_MD_setsockopt(PRFileDesc *fd, PRInt32 level,
               PRInt32 optname, const char* optval, PRInt32 optlen);
#define _MD_SETSOCKOPT		_MD_setsockopt

extern PRStatus
_MD_gethostname(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME		_MD_gethostname

/**
 * definitions and macros for file l4env/locks.c
 */
struct _MDSemaphore {
    PRInt8 notused;
};

struct _MDCVar {
    PRInt8 notused;
};

struct _MDLock {
    int unused;
};


/* Intel based Linux, err sorry, L4 */
#define _MD_GET_SP(_t) CONTEXT(_t)[0].__jmpbuf[JB_SP]
#define _MD_SET_FP(_t, val) (CONTEXT(_t)[0].__jmpbuf[JB_BP] = val)
#define _MD_GET_SP_PTR(_t) &(_MD_GET_SP(_t))
#define _MD_GET_FP_PTR(_t) &(CONTEXT(_t)[0].__jmpbuf[JB_BP])
#define _MD_SP_TYPE __ptr_t
#define PR_NUM_GCREGS   6

/*
** Initialize a thread context to run "_main()" when started
*/
#define _MD_INIT_CONTEXT(_thread, _sp, _main, status)  \
{  \
    *status = PR_TRUE;  \
    if (sigsetjmp(CONTEXT(_thread), 1)) {  \
        _main();  \
    }  \
    _MD_GET_SP(_thread) = (_MD_SP_TYPE) ((_sp) - 64); \
	_thread->md.sp = _MD_GET_SP_PTR(_thread); \
	_thread->md.fp = _MD_GET_FP_PTR(_thread); \
    _MD_SET_FP(_thread, 0); \
}

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!sigsetjmp(CONTEXT(_thread), 1)) {  \
	(_thread)->md.errcode = errno;  \
	_PR_Schedule();  \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{   \
    errno = (_thread)->md.errcode;  \
    _MD_SET_CURRENT_THREAD(_thread);  \
    siglongjmp(CONTEXT(_thread), 1);  \
}

/* Machine-dependent (MD) data structures */

struct _MDThread {
    PR_CONTEXT_TYPE context;
/* The next two are purely for debugging purposes */
    int sp;
    int fp;
    int id;
    int errcode;
};

struct _MDThreadStack {
    PRInt8 notused;
};


/*
 * md-specific cpu structure field
 */

#include <sys/time.h>  /* for FD_SETSIZE */
#define _PR_MD_MAX_OSFD FD_SETSIZE

struct _MDCPU_Unix {
    PRCList ioQ;
    PRUint32 ioq_timeout;
    PRInt32 ioq_max_osfd;
    PRInt32 ioq_osfd_cnt;
#ifndef _PR_USE_POLL
    fd_set fd_read_set, fd_write_set, fd_exception_set;
    PRInt16 fd_read_cnt[_PR_MD_MAX_OSFD],fd_write_cnt[_PR_MD_MAX_OSFD],
				fd_exception_cnt[_PR_MD_MAX_OSFD];
#else
	struct pollfd *ioq_pollfds;
	int ioq_pollfds_size;
#endif	/* _PR_USE_POLL */
};

#define _PR_IOQ(_cpu)			((_cpu)->md.md_unix.ioQ)
#define _PR_ADD_TO_IOQ(_pq, _cpu) PR_APPEND_LINK(&_pq.links, &_PR_IOQ(_cpu))
#define _PR_FD_READ_SET(_cpu)		((_cpu)->md.md_unix.fd_read_set)
#define _PR_FD_READ_CNT(_cpu)		((_cpu)->md.md_unix.fd_read_cnt)
#define _PR_FD_WRITE_SET(_cpu)		((_cpu)->md.md_unix.fd_write_set)
#define _PR_FD_WRITE_CNT(_cpu)		((_cpu)->md.md_unix.fd_write_cnt)
#define _PR_FD_EXCEPTION_SET(_cpu)	((_cpu)->md.md_unix.fd_exception_set)
#define _PR_FD_EXCEPTION_CNT(_cpu)	((_cpu)->md.md_unix.fd_exception_cnt)
#define _PR_IOQ_TIMEOUT(_cpu)		((_cpu)->md.md_unix.ioq_timeout)
#define _PR_IOQ_MAX_OSFD(_cpu)		((_cpu)->md.md_unix.ioq_max_osfd)
#define _PR_IOQ_OSFD_CNT(_cpu)		((_cpu)->md.md_unix.ioq_osfd_cnt)
#define _PR_IOQ_POLLFDS(_cpu)		((_cpu)->md.md_unix.ioq_pollfds)
#define _PR_IOQ_POLLFDS_SIZE(_cpu)	((_cpu)->md.md_unix.ioq_pollfds_size)

#define _PR_IOQ_MIN_POLLFDS_SIZE(_cpu)	32

struct _MDCPU {
	struct _MDCPU_Unix md_unix;
};

#define _MD_INIT_LOCKS()
#define _MD_NEW_LOCK(lock) PR_SUCCESS
#define _MD_FREE_LOCK(lock)
#define _MD_LOCK(lock)
#define _MD_UNLOCK(lock)
#define _MD_INIT_IO()
#define _MD_IOQ_LOCK()
#define _MD_IOQ_UNLOCK()

#if 0
extern PRStatus _MD_InitializeThread(PRThread *thread);
#endif /* 0 */

#define _MD_INIT_RUNNING_CPU(cpu)       _MD_unix_init_running_cpu(cpu)

#define _MD_INIT_THREAD                 _MD_InitializeThread

#if 0
#define _MD_EXIT_THREAD(thread)
#define _MD_SUSPEND_THREAD(thread)      _MD_suspend_thread
#define _MD_RESUME_THREAD(thread)       _MD_resume_thread
#endif /* 0 */

#define _MD_CLEAN_THREAD(_thread)

#if 0
extern PRStatus _MD_CREATE_THREAD(
    PRThread *thread,
    void (*start) (void *),
    PRThreadPriority priority,
    PRThreadScope scope,
    PRThreadState state,
    PRUint32 stackSize);
extern void _MD_SET_PRIORITY(struct _MDThread *thread, PRUintn newPri);
#endif /* 0 */

extern PRStatus _MD_WAIT(PRThread *, PRIntervalTime timeout);
extern PRStatus _MD_WAKEUP_WAITER(PRThread *);

#if 0
extern void _MD_YIELD(void);

extern void _MD_EarlyInit(void);
#endif /* 0 */

extern PRIntervalTime _PR_UNIX_GetInterval(void);
extern PRIntervalTime _PR_UNIX_TicksPerSecond(void);

#if 0
#define _MD_EARLY_INIT                  _MD_EarlyInit
#endif /* 0 */
#define _MD_FINAL_INIT					_PR_UnixInit
#define _MD_GET_INTERVAL                _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC            _PR_UNIX_TicksPerSecond

/*
 * We wrapped the select() call.  _MD_SELECT refers to the built-in,
 * unwrapped version.
 */
#if 0
#define _MD_SELECT __select
#else
#define _MD_SELECT select
#endif

#ifdef _PR_POLL_AVAILABLE
#include <sys/poll.h>
extern int __syscall_poll(struct pollfd *ufds, unsigned long int nfds,
	int timeout);
#define _MD_POLL __syscall_poll
#endif

#if 0
/* For writev() */
#include <sys/uio.h>

extern void _MD_l4_map_sendfile_error(int err);

#include <sys/types.h>
#include <dirent.h>

#include "prio.h"
#include "prmem.h"
#include "prclist.h"
#endif /* 0 */

#if 0
/*
 * intervals at which GLOBAL threads wakeup to check for pending interrupt
 */
#define _PR_INTERRUPT_CHECK_INTERVAL_SECS 5
extern PRIntervalTime intr_timeout_ticks;

#define _PR_POLLQUEUE_PTR(_qp) \
    ((PRPollQueue*) ((char*) (_qp) - offsetof(PRPollQueue,links)))

extern void _PR_Unblock_IO_Wait(struct PRThread *thr);

#endif /* 0 */

#if 0

struct _MDDir {
	DIR *d;
};

struct _PRCPU;
#endif /* 0 */

/*
** Make a redzone at both ends of the stack segment. Disallow access
** to those pages of memory. It's ok if the mprotect call's don't
** work - it just means that we don't really have a functional
** redzone.
*/
#if 0
#include <sys/mman.h>
#ifndef PROT_NONE
#define PROT_NONE 0x0
#endif

#define _MD_INIT_STACK(ts,REDZONE)
#define _MD_CLEAR_STACK(ts)

#define PR_SET_INTSOFF(newval)
#endif

#if 0

struct PRProcess;
struct PRProcessAttr;

/* Create a new process (fork() + exec()) */
#define _MD_CREATE_PROCESS _MD_l4CreateProcess
    /* When the compiler complains about this, we will do something about it. */

#define _MD_DETACH_PROCESS _MD_l4DetachProcess

/* Wait for a child process to terminate */
#define _MD_WAIT_PROCESS _MD_l4WaitProcess

#define _MD_KILL_PROCESS _MD_l4KillProcess
#endif

/************************************************************************/

#if 0
extern void _MD_EnableClockInterrupts(void);
extern void _MD_DisableClockInterrupts(void);

#define _MD_START_INTERRUPTS			_MD_StartInterrupts
#define _MD_STOP_INTERRUPTS				_MD_StopInterrupts
#define _MD_DISABLE_CLOCK_INTERRUPTS	_MD_DisableClockInterrupts
#define _MD_ENABLE_CLOCK_INTERRUPTS		_MD_EnableClockInterrupts
#define _MD_BLOCK_CLOCK_INTERRUPTS		_MD_BlockClockInterrupts
#define _MD_UNBLOCK_CLOCK_INTERRUPTS	_MD_UnblockClockInterrupts
#endif

/************************************************************************/

#if 0
extern void		_MD_InitCPUS(void);
#define _MD_INIT_CPUS           _MD_InitCPUS

extern void		_MD_Wakeup_CPUs(void);
#define _MD_WAKEUP_CPUS _MD_Wakeup_CPUs

#define _MD_PAUSE_CPU			_MD_PauseCPU

#if defined(_PR_LOCAL_THREADS_ONLY) || defined(_PR_GLOBAL_THREADS_ONLY)
#define _MD_CLEANUP_BEFORE_EXIT()
#endif

#define _MD_EXIT(status)		_exit(status)
#endif

/************************************************************************/

#if 0
#define _MD_GET_ENV				getenv
#define _MD_PUT_ENV				putenv
#endif /* 0 */

/************************************************************************/

#if 0
#define _MD_INIT_FILEDESC(fd)

extern void		_MD_MakeNonblock(PRFileDesc *fd);
#define _MD_MAKE_NONBLOCK			_MD_MakeNonblock		
#endif

/************************************************************************/

#if 0
#if !defined(_PR_PTHREADS)

extern void		_MD_InitSegs(void);
extern PRStatus	_MD_AllocSegment(PRSegment *seg, PRUint32 size,
				void *vaddr);
extern void		_MD_FreeSegment(PRSegment *seg);

#define _MD_INIT_SEGS			_MD_InitSegs
#define _MD_ALLOC_SEGMENT		_MD_AllocSegment
#define _MD_FREE_SEGMENT		_MD_FreeSegment

#endif /* !defined(_PR_PTHREADS) */
#endif /* 0 */

/************************************************************************/

#if 0
#define _MD_INTERVAL_INIT()
#define _MD_INTERVAL_PER_MILLISEC()	(_PR_MD_INTERVAL_PER_SEC() / 1000)
#define _MD_INTERVAL_PER_MICROSEC()	(_PR_MD_INTERVAL_PER_SEC() / 1000000)
#endif

/************************************************************************/

#if 0
#define _MD_ERRNO()             	(errno)
#define _MD_GET_SOCKET_ERROR()		(errno)
#endif

/************************************************************************/

#if 0
extern PRInt32 _MD_AvailableSocket(PRInt32 osfd);

extern void _MD_StartInterrupts(void);
extern void _MD_StopInterrupts(void);
extern void _MD_DisableClockInterrupts(void);
extern void _MD_BlockClockInterrupts(void);
extern void _MD_UnblockClockInterrupts(void);
extern void _MD_PauseCPU(PRIntervalTime timeout);
#endif /* 0 */

#if 0
extern PRStatus _MD_open_dir(struct _MDDir *, const char *);
extern PRInt32  _MD_close_dir(struct _MDDir *);
extern char *   _MD_read_dir(struct _MDDir *, PRIntn);
extern PRInt32  _MD_open(const char *name, PRIntn osflags, PRIntn mode);
extern PRInt32	_MD_delete(const char *name);
extern PRInt32	_MD_getfileinfo(const char *fn, PRFileInfo *info);
extern PRInt32  _MD_getfileinfo64(const char *fn, PRFileInfo64 *info);
extern PRInt32  _MD_getopenfileinfo(const PRFileDesc *fd, PRFileInfo *info);
extern PRInt32  _MD_getopenfileinfo64(const PRFileDesc *fd, PRFileInfo64 *info);
extern PRInt32	_MD_rename(const char *from, const char *to);
extern PRInt32	_MD_access(const char *name, PRAccessHow how);
extern PRInt32	_MD_mkdir(const char *name, PRIntn mode);
extern PRInt32	_MD_rmdir(const char *name);
extern PRInt32	_MD_accept_read(PRInt32 sock, PRInt32 *newSock,
				PRNetAddr **raddr, void *buf, PRInt32 amount);
extern PRInt32 	_PR_UnixSendFile(PRFileDesc *sd, PRSendFileData *sfd,
			PRTransmitFileFlags flags, PRIntervalTime timeout);

extern PRStatus _MD_LockFile(PRInt32 osfd);
extern PRStatus _MD_TLockFile(PRInt32 osfd);
extern PRStatus _MD_UnlockFile(PRInt32 osfd);

#define _MD_OPEN_DIR(dir, name)		    _MD_open_dir(dir, name)
#define _MD_CLOSE_DIR(dir)		        _MD_close_dir(dir)
#define _MD_READ_DIR(dir, flags)	    _MD_read_dir(dir, flags)
#define _MD_OPEN(name, osflags, mode)	_MD_open(name, osflags, mode)
#define _MD_OPEN_FILE(name, osflags, mode)	_MD_open(name, osflags, mode)
extern PRInt32 _MD_read(PRFileDesc *fd, void *buf, PRInt32 amount);
#define _MD_READ(fd,buf,amount)		    _MD_read(fd,buf,amount)
extern PRInt32 _MD_write(PRFileDesc *fd, const void *buf, PRInt32 amount);
#define _MD_WRITE(fd,buf,amount)	    _MD_write(fd,buf,amount)
#define _MD_DELETE(name)		        _MD_delete(name)
#define _MD_GETFILEINFO(fn, info)	    _MD_getfileinfo(fn, info)
#define _MD_GETFILEINFO64(fn, info)	    _MD_getfileinfo64(fn, info)
#define _MD_GETOPENFILEINFO(fd, info)	_MD_getopenfileinfo(fd, info)
#define _MD_GETOPENFILEINFO64(fd, info)	_MD_getopenfileinfo64(fd, info)
#define _MD_RENAME(from, to)		    _MD_rename(from, to)
#define _MD_ACCESS(name, how)		    _MD_access(name, how)
#define _MD_MKDIR(name, mode)		    _MD_mkdir(name, mode)
#define _MD_MAKE_DIR(name, mode)		_MD_mkdir(name, mode)
#define _MD_RMDIR(name)			        _MD_rmdir(name)
#define _MD_ACCEPT_READ(sock, newSock, raddr, buf, amount)	_MD_accept_read(sock, newSock, raddr, buf, amount)

#define _MD_LOCKFILE _MD_LockFile
#define _MD_TLOCKFILE _MD_TLockFile
#define _MD_UNLOCKFILE _MD_UnlockFile
#endif /* 0 */

#if 0
extern PRInt32		_MD_socket(int af, int type, int flags);
#define _MD_SOCKET	_MD_socket
extern PRInt32		_MD_connect(PRFileDesc *fd, const PRNetAddr *addr,
								PRUint32 addrlen, PRIntervalTime timeout);
#define _MD_CONNECT	_MD_connect
extern PRInt32		_MD_accept(PRFileDesc *fd, PRNetAddr *addr, PRUint32 *addrlen,
													PRIntervalTime timeout);
#define _MD_ACCEPT	_MD_accept
extern PRInt32		_MD_bind(PRFileDesc *fd, const PRNetAddr *addr, PRUint32 addrlen);
#define _MD_BIND	_MD_bind
extern PRInt32		_MD_listen(PRFileDesc *fd, PRIntn backlog);
#define _MD_LISTEN	_MD_listen
extern PRInt32		_MD_shutdown(PRFileDesc *fd, PRIntn how);
#define _MD_SHUTDOWN	_MD_shutdown

extern PRInt32		_MD_recv(PRFileDesc *fd, void *buf, PRInt32 amount, 
                               PRIntn flags, PRIntervalTime timeout);
#define _MD_RECV	_MD_recv
extern PRInt32		_MD_send(PRFileDesc *fd, const void *buf, PRInt32 amount,
									PRIntn flags, PRIntervalTime timeout);
#define _MD_SEND	_MD_send
extern PRInt32		_MD_recvfrom(PRFileDesc *fd, void *buf, PRInt32 amount,
						PRIntn flags, PRNetAddr *addr, PRUint32 *addrlen,
											PRIntervalTime timeout);
#define _MD_RECVFROM	_MD_recvfrom
extern PRInt32 _MD_sendto(PRFileDesc *fd, const void *buf, PRInt32 amount,
							PRIntn flags, const PRNetAddr *addr, PRUint32 addrlen,
												PRIntervalTime timeout);
#define _MD_SENDTO	_MD_sendto
extern PRInt32		_MD_writev(PRFileDesc *fd, const struct PRIOVec *iov,
								PRInt32 iov_size, PRIntervalTime timeout);
#define _MD_WRITEV	_MD_writev

extern PRInt32		_MD_socketavailable(PRFileDesc *fd);
#define	_MD_SOCKETAVAILABLE		_MD_socketavailable
extern PRInt64		_MD_socketavailable64(PRFileDesc *fd);
#define	_MD_SOCKETAVAILABLE64		_MD_socketavailable64

#define	_MD_PIPEAVAILABLE		_MD_socketavailable

extern PRInt32 _MD_pr_poll(PRPollDesc *pds, PRIntn npds,
												PRIntervalTime timeout);
#define _MD_PR_POLL		_MD_pr_poll
#endif /* 0 */

#if 0
extern PRInt32		_MD_close(PRInt32 osfd);
#define _MD_CLOSE_FILE	_MD_close
extern PRInt32		_MD_lseek(PRFileDesc*, PRInt32, PRSeekWhence);
#define _MD_LSEEK	_MD_lseek
extern PRInt64		_MD_lseek64(PRFileDesc*, PRInt64, PRSeekWhence);
#define _MD_LSEEK64	_MD_lseek64
extern PRInt32		_MD_fsync(PRFileDesc *fd);
#define _MD_FSYNC	_MD_fsync
#endif /* 0 */

#if 0
extern PRInt32 _MD_socketpair(int af, int type, int flags, PRInt32 *osfd);
#define _MD_SOCKETPAIR		_MD_socketpair

#define _MD_CLOSE_SOCKET	_MD_close

#ifndef NO_NSPR_10_SUPPORT
#define _MD_STAT	stat
#endif

extern PRStatus _MD_getpeername(PRFileDesc *fd, PRNetAddr *addr,
											PRUint32 *addrlen);
#define _MD_GETPEERNAME _MD_getpeername
extern PRStatus _MD_getsockname(PRFileDesc *fd, PRNetAddr *addr,
											PRUint32 *addrlen);
#define _MD_GETSOCKNAME _MD_getsockname

extern PRStatus _MD_getsockopt(PRFileDesc *fd, PRInt32 level,
						PRInt32 optname, char* optval, PRInt32* optlen);
#define _MD_GETSOCKOPT		_MD_getsockopt
extern PRStatus _MD_setsockopt(PRFileDesc *fd, PRInt32 level,
					PRInt32 optname, const char* optval, PRInt32 optlen);
#define _MD_SETSOCKOPT		_MD_setsockopt

extern PRStatus _MD_set_fd_inheritable(PRFileDesc *fd, PRBool inheritable);
#define _MD_SET_FD_INHERITABLE _MD_set_fd_inheritable

extern void _MD_init_fd_inheritable(PRFileDesc *fd, PRBool imported);
#define _MD_INIT_FD_INHERITABLE _MD_init_fd_inheritable

extern void _MD_query_fd_inheritable(PRFileDesc *fd);
#define _MD_QUERY_FD_INHERITABLE _MD_query_fd_inheritable

extern PRStatus _MD_gethostname(char *name, PRUint32 namelen);
#define _MD_GETHOSTNAME		_MD_gethostname

extern PRStatus _MD_getsysinfo(PRSysInfo cmd, char *name, PRUint32 namelen);
#define _MD_GETSYSINFO		_MD_getsysinfo
#endif /* 0 */

#if 0
struct _MDFileMap {
    PRIntn prot;
    PRIntn flags;
    PRBool isAnonFM; /* when true, PR_CloseFileMap() must close the related fd */
};

extern PRStatus _MD_CreateFileMap(struct PRFileMap *fmap, PRInt64 size);
#define _MD_CREATE_FILE_MAP _MD_CreateFileMap

#define _MD_GET_MEM_MAP_ALIGNMENT() PR_GetPageSize()

extern void * _MD_MemMap(struct PRFileMap *fmap, PRInt64 offset,
        PRUint32 len);
#define _MD_MEM_MAP _MD_MemMap

extern PRStatus _MD_MemUnmap(void *addr, PRUint32 size);
#define _MD_MEM_UNMAP _MD_MemUnmap

extern PRStatus _MD_CloseFileMap(struct PRFileMap *fmap);
#define _MD_CLOSE_FILE_MAP _MD_CloseFileMap
#endif /* 0 */

#if 0
#define GETTIMEOFDAY(tp) gettimeofday((tp), NULL)

#if defined(_PR_PTHREADS) && !defined(_PR_POLL_AVAILABLE)
#define _PR_NEED_FAKE_POLL
#endif

/*
** A vector of the UNIX I/O calls we use. These are here to smooth over
** the rough edges needed for large files. All of NSPR's implmentaions
** go through this vector using syntax of the form
**      result = _md_iovector.xxx64(args);
*/

typedef struct stat _MDStat64;
typedef off_t _MDOff64_t;

typedef PRIntn (*_MD_Fstat64)(PRIntn osfd, _MDStat64 *buf);
typedef PRIntn (*_MD_Open64)(const char *path, int oflag, ...);
typedef PRIntn (*_MD_Stat64)(const char *path, _MDStat64 *buf);
typedef _MDOff64_t (*_MD_Lseek64)(PRIntn osfd, _MDOff64_t, PRIntn whence);
typedef void* (*_MD_Mmap64)(
    void *addr, PRSize len, PRIntn prot, PRIntn flags,
    PRIntn fildes, _MDOff64_t offset);
struct _MD_IOVector
{
    _MD_Open64 _open64;
    _MD_Mmap64 _mmap64;
    _MD_Stat64 _stat64;
    _MD_Fstat64 _fstat64;
    _MD_Lseek64 _lseek64;
};
extern struct _MD_IOVector _md_iovector;
#endif /* 0 */

#endif /* nspr_l4v2_defs_h___ */
