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

/*
** File:   ptio.c
** Descritpion:  Implemenation of I/O methods for pthreads
*/

#include <pthread.h>
#include <string.h>  /* for memset() */
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if defined(DARWIN)
#include <sys/utsname.h> /* for uname */
#endif
#if defined(SOLARIS)
#include <sys/filio.h>  /* to pick up FIONREAD */
#endif
#ifdef _PR_POLL_AVAILABLE
#include <poll.h>
#endif
/* To pick up getrlimit() etc. */
#include <sys/time.h>
#include <sys/resource.h>

#include "primpl.h"

#if defined(VBOX) && defined(_PR_POLL_AVAILABLE)
# include <poll.h>
#endif

#include <iprt/assert.h>
#include <iprt/thread.h>

static PRLock *_pr_rename_lock;  /* For PR_Rename() */

/**************************************************************************/

/*****************************************************************************/
/************************* I/O Continuation machinery ************************/
/*****************************************************************************/

/*
 * The polling interval defines the maximum amount of time that a thread
 * might hang up before an interrupt is noticed.
 */
#define PT_DEFAULT_POLL_MSEC 5000

typedef struct pt_Continuation pt_Continuation;
typedef PRBool (*ContinuationFn)(pt_Continuation *op, PRInt16 revents);

typedef enum pr_ContuationStatus
{
    pt_continuation_pending,
    pt_continuation_done
} pr_ContuationStatus;

struct pt_Continuation
{
    /* The building of the continuation operation */
    ContinuationFn function;                /* what function to continue */
    union { PRIntn osfd; } arg1;            /* #1 - the op's fd */
    union { void* buffer; } arg2;           /* #2 - primary transfer buffer */
    union {
        PRSize amount;                      /* #3 - size of 'buffer', or */
    } arg3;
    union { PRIntn flags; } arg4;           /* #4 - read/write flags */
    union { PRNetAddr *addr; } arg5;        /* #5 - send/recv address */

    PRIntervalTime timeout;                 /* client (relative) timeout */

    PRInt16 event;                           /* flags for poll()'s events */

    /*
    ** The representation and notification of the results of the operation.
    ** These function can either return an int return code or a pointer to
    ** some object.
    */
    union { PRSize code; void *object; } result;

    PRIntn syserrno;                        /* in case it failed, why (errno) */
    pr_ContuationStatus status;             /* the status of the operation */
};

/*
** Allocate a file descriptor from the heap.
*/
static PRFileDesc *_PR_Getfd(void)
{
    PRFileDesc *fd = PR_NEW(PRFileDesc);
    if (NULL != fd)
    {
        fd->secret = PR_NEW(PRFilePrivate);
        if (NULL == fd->secret) PR_DELETE(fd);
    }
    if (fd == NULL)
        return NULL;

    fd->dtor = NULL;
    fd->lower = fd->higher = NULL;
    memset(fd->secret, 0, sizeof(PRFilePrivate));
    return fd;
}  /* _PR_Getfd */

static void _PR_Putfd(PRFileDesc *fd)
{
    PR_Free(fd->secret);
    PR_Free(fd);
}  /* _PR_Putfd */

static void pt_poll_now(pt_Continuation *op)
{
    PRInt32 msecs;
	PRIntervalTime epoch, now, elapsed, remaining;
	PRBool wait_for_remaining;
    PRThread *self = PR_GetCurrentThread();

	Assert(PR_INTERVAL_NO_WAIT != op->timeout);

    switch (op->timeout) {
        case PR_INTERVAL_NO_TIMEOUT:
			msecs = PT_DEFAULT_POLL_MSEC;
			do
			{
				PRIntn rv;
				struct pollfd tmp_pfd;

				tmp_pfd.revents = 0;
				tmp_pfd.fd = op->arg1.osfd;
				tmp_pfd.events = op->event;

				rv = poll(&tmp_pfd, 1, msecs);

				if (self->state & PT_THREAD_ABORTED)
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if ((-1 == rv) && ((errno == EINTR) || (errno == EAGAIN)))
					continue; /* go around the loop again */

				if (rv > 0)
				{
					PRInt16 events = tmp_pfd.events;
					PRInt16 revents = tmp_pfd.revents;

					if ((revents & POLLNVAL)  /* busted in all cases */
					|| ((events & POLLOUT) && (revents & POLLHUP)))
						/* write op & hup */
					{
						op->result.code = -1;
						if (POLLNVAL & revents) op->syserrno = EBADF;
						else if (POLLHUP & revents) op->syserrno = EPIPE;
						op->status = pt_continuation_done;
					} else {
						if (op->function(op, revents))
							op->status = pt_continuation_done;
					}
				} else if (rv == -1) {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
				/* else, poll timed out */
			} while (pt_continuation_done != op->status);
			break;
        default:
            now = epoch = PR_IntervalNow();
            remaining = op->timeout;
			do
			{
				PRIntn rv;
				struct pollfd tmp_pfd;

				tmp_pfd.revents = 0;
				tmp_pfd.fd = op->arg1.osfd;
				tmp_pfd.events = op->event;

    			wait_for_remaining = PR_TRUE;
    			msecs = (PRInt32)PR_IntervalToMilliseconds(remaining);
				if (msecs > PT_DEFAULT_POLL_MSEC)
				{
					wait_for_remaining = PR_FALSE;
					msecs = PT_DEFAULT_POLL_MSEC;
				}
				rv = poll(&tmp_pfd, 1, msecs);

				if (self->state & PT_THREAD_ABORTED)
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if (rv > 0)
				{
					PRInt16 events = tmp_pfd.events;
					PRInt16 revents = tmp_pfd.revents;

					if ((revents & POLLNVAL)  /* busted in all cases */
						|| ((events & POLLOUT) && (revents & POLLHUP)))
											/* write op & hup */
					{
						op->result.code = -1;
						if (POLLNVAL & revents) op->syserrno = EBADF;
						else if (POLLHUP & revents) op->syserrno = EPIPE;
						op->status = pt_continuation_done;
					} else {
						if (op->function(op, revents))
						{
							op->status = pt_continuation_done;
						}
					}
				} else if ((rv == 0) ||
						((errno == EINTR) || (errno == EAGAIN))) {
					if (rv == 0)	/* poll timed out */
					{
						if (wait_for_remaining)
							now += remaining;
						else
							now += PR_MillisecondsToInterval(msecs);
					}
					else
						now = PR_IntervalNow();
					elapsed = (PRIntervalTime) (now - epoch);
					if (elapsed >= op->timeout) {
						op->result.code = -1;
						op->syserrno = ETIMEDOUT;
						op->status = pt_continuation_done;
					} else
						remaining = op->timeout - elapsed;
				} else {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
			} while (pt_continuation_done != op->status);
            break;
    }

}  /* pt_poll_now */

static PRIntn pt_Continue(pt_Continuation *op)
{
    op->status = pt_continuation_pending;  /* set default value */
	/*
	 * let each thread call poll directly
	 */
	pt_poll_now(op);
	Assert(pt_continuation_done == op->status);
    return op->result.code;
}  /* pt_Continue */

/*****************************************************************************/
/*********************** specific continuation functions *********************/
/*****************************************************************************/
static PRBool pt_read_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
    op->result.code = read(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    return ((-1 == op->result.code) &&
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_read_cont */

static PRBool pt_write_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
    bytes = write(op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_write_cont */

static PRBool pt_writev_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    struct iovec *iov = (struct iovec*)op->arg2.buffer;
    /*
     * Same rules as write, but continuing seems to be a bit more
     * complicated. As the number of bytes sent grows, we have to
     * redefine the vector we're pointing at. We might have to
     * modify an individual vector parms or we might have to eliminate
     * a pair altogether.
     */
    bytes = writev(op->arg1.osfd, iov, op->arg3.amount);
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        PRIntn iov_index;
        op->result.code += bytes;  /* accumulate the number sent */
        for (iov_index = 0; iov_index < op->arg3.amount; ++iov_index)
        {
            /* how much progress did we make in the i/o vector? */
            if (bytes < iov[iov_index].iov_len)
            {
                /* this element's not done yet */
                char **bp = (char**)&(iov[iov_index].iov_base);
                iov[iov_index].iov_len -= bytes;  /* there's that much left */
                *bp += bytes;  /* starting there */
                break;  /* go off and do that */
            }
            bytes -= iov[iov_index].iov_len;  /* that element's consumed */
        }
        op->arg2.buffer = &iov[iov_index];  /* new start of array */
        op->arg3.amount -= iov_index;  /* and array length */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_writev_cont */

void _PR_InitIO(void)
{
    _pr_rename_lock = PR_NewLock();
    Assert(NULL != _pr_rename_lock);
}  /* _PR_InitIO */

void _PR_CleanupIO(void)
{
    if (_pr_rename_lock)
    {
        PR_DestroyLock(_pr_rename_lock);
        _pr_rename_lock = NULL;
    }
}  /* _PR_CleanupIO */

/*****************************************************************************/
/***************************** I/O private methods ***************************/
/*****************************************************************************/

static PRBool pt_TestAbort(void)
{
    PRThread *me = PR_CurrentThread();
    if(_PT_THREAD_INTERRUPTED(me))
    {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        me->state &= ~PT_THREAD_ABORTED;
        return PR_TRUE;
    }
    return PR_FALSE;
}  /* pt_TestAbort */

static void pt_MapError(void (*mapper)(PRIntn), PRIntn syserrno)
{
    switch (syserrno)
    {
        case EINTR:
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0); break;
        case ETIMEDOUT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0); break;
        default:
            mapper(syserrno);
    }
}  /* pt_MapError */

static PRStatus pt_Close(PRFileDesc *fd)
{
    if ((NULL == fd) || (NULL == fd->secret)
        || ((_PR_FILEDESC_OPEN != fd->secret->state)
        && (_PR_FILEDESC_CLOSED != fd->secret->state)))
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if (pt_TestAbort()) return PR_FAILURE;

    if (_PR_FILEDESC_OPEN == fd->secret->state)
    {
        if (-1 == close(fd->secret->md.osfd))
        {
#ifdef OSF1
            /*
             * Bug 86941: On Tru64 UNIX V5.0A and V5.1, the close()
             * system call, when called to close a TCP socket, may
             * return -1 with errno set to EINVAL but the system call
             * does close the socket successfully.  An application
             * may safely ignore the EINVAL error.  This bug is fixed
             * on Tru64 UNIX V5.1A and later.  The defect tracking
             * number is QAR 81431.
             */
            if (PR_DESC_SOCKET_TCP != fd->methods->file_type
            || EINVAL != errno)
            {
                pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
                return PR_FAILURE;
            }
#else
            pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
            return PR_FAILURE;
#endif
        }
        fd->secret->state = _PR_FILEDESC_CLOSED;
    }
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* pt_Close */

static PRInt32 pt_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return bytes;

    bytes = read(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.function = pt_read_cont;
        op.event = POLLIN | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_READ_ERROR, syserrno);
    return bytes;
}  /* pt_Read */

static PRInt32 pt_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;

    if (pt_TestAbort()) return bytes;

    bytes = write(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        buf = (char *) buf + bytes;
        amount -= bytes;
        fNeedContinue = PR_TRUE;
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        bytes = 0;
        fNeedContinue = PR_TRUE;
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_write_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_WRITE_ERROR, syserrno);
    return bytes;
}  /* pt_Write */

static PRInt32 pt_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK(fd, offset, whence);
}  /* pt_Seek */

static PRInt64 pt_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK64(fd, offset, whence);
}  /* pt_Seek64 */

static PRInt32 pt_Available_f(PRFileDesc *fd)
{
    PRInt32 result, cur, end;

    cur = _PR_MD_LSEEK(fd, 0, PR_SEEK_CUR);

    if (cur >= 0)
        end = _PR_MD_LSEEK(fd, 0, PR_SEEK_END);

    if ((cur < 0) || (end < 0)) {
        return -1;
    }

    result = end - cur;
    _PR_MD_LSEEK(fd, cur, PR_SEEK_SET);

    return result;
}  /* pt_Available_f */

static PRStatus pt_FileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo */

static PRStatus pt_FileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO64(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo64 */

static PRStatus pt_Fsync(PRFileDesc *fd)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = fsync(fd->secret->md.osfd);
    if (rv < 0) {
        pt_MapError(_PR_MD_MAP_FSYNC_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Fsync */

static PRInt16 pt_Poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;
    return in_flags;
}  /* pt_Poll */

/*****************************************************************************/
/****************************** I/O method objects ***************************/
/*****************************************************************************/

static PRIOMethods _pr_file_methods = {
    PR_DESC_FILE,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available_f,
    pt_Fsync,
    pt_Seek,
    pt_Seek64,
    pt_FileInfo,
    pt_FileInfo64,
    (PRWritevFN)_PR_InvalidInt,
    (PRConnectFN)_PR_InvalidStatus,
    (PRAcceptFN)_PR_InvalidDesc,
    (PRBindFN)_PR_InvalidStatus,
    (PRListenFN)_PR_InvalidStatus,
    (PRShutdownFN)_PR_InvalidStatus,
    (PRRecvFN)_PR_InvalidInt,
    (PRSendFN)_PR_InvalidInt,
    pt_Poll,
    (PRGetsocknameFN)_PR_InvalidStatus,
    (PRGetpeernameFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

static PRFileDesc *pt_SetMethods(
    PRIntn osfd, PRDescType type, PRBool isAcceptedSocket, PRBool imported)
{
    PRFileDesc *fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->state = _PR_FILEDESC_OPEN;
        if (imported) fd->secret->inheritable = _PR_TRI_UNKNOWN;
        else
        {
            /* By default, a Unix fd is not closed on exec. */
            PRIntn flags;
            flags = fcntl(osfd, F_GETFD, 0);
            fd->secret->inheritable = flags & FD_CLOEXEC ? _PR_TRI_FALSE : _PR_TRI_TRUE;
        }
        switch (type)
        {
            case PR_DESC_FILE:
                fd->methods = &_pr_file_methods;
                break;
            default:
                break;
        }
    }
    return fd;
}  /* pt_SetMethods */

/*****************************************************************************/
/****************************** I/O public methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_OpenFile(
    const char *name, PRIntn flags, PRIntn mode)
{
    PRFileDesc *fd = NULL;
    PRIntn syserrno, osfd = -1, osflags = 0;;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (flags & PR_RDONLY) osflags |= O_RDONLY;
    if (flags & PR_WRONLY) osflags |= O_WRONLY;
    if (flags & PR_RDWR) osflags |= O_RDWR;
    if (flags & PR_APPEND) osflags |= O_APPEND;
    if (flags & PR_TRUNCATE) osflags |= O_TRUNC;
    if (flags & PR_EXCL) osflags |= O_EXCL;
    if (flags & PR_SYNC)
    {
#if defined(O_SYNC)
        osflags |= O_SYNC;
#elif defined(O_FSYNC)
        osflags |= O_FSYNC;
#else
#error "Neither O_SYNC nor O_FSYNC is defined on this platform"
#endif
    }

    /*
    ** We have to hold the lock across the creation in order to
    ** enforce the sematics of PR_Rename(). (see the latter for
    ** more details)
    */
    if (flags & PR_CREATE_FILE)
    {
        osflags |= O_CREAT;
        if (NULL !=_pr_rename_lock)
            PR_Lock(_pr_rename_lock);
    }

    osfd = _md_iovector._open64(name, osflags, mode);
    syserrno = errno;

    if ((flags & PR_CREATE_FILE) && (NULL !=_pr_rename_lock))
        PR_Unlock(_pr_rename_lock);

    if (osfd == -1)
        pt_MapError(_PR_MD_MAP_OPEN_ERROR, syserrno);
    else
    {
        fd = pt_SetMethods(osfd, PR_DESC_FILE, PR_FALSE, PR_FALSE);
        if (fd == NULL) close(osfd);  /* $$$ whoops! this is bad $$$ */
    }
    return fd;
}  /* PR_OpenFile */

PR_IMPLEMENT(PRFileDesc*) PR_Open(const char *name, PRIntn flags, PRIntn mode)
{
    return PR_OpenFile(name, flags, mode);
}  /* PR_Open */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo64(const char *fn, PRFileInfo64 *info)
{
    PRInt32 rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    rv = _PR_MD_GETFILEINFO64(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo64 */

/* ptio.c */
