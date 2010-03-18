/* $Id$ */
/** @file
 * IPRT - Polling I/O Handles, POSIX Implementation.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/poll.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include "internal/magics.h"

#include <limits.h>
#include <errno.h>
#include <sys/poll.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Handle entry in a poll set.
 */
typedef struct RTPOLLSETHNDENT
{
    /** The handle type. */
    RTHANDLETYPE    enmType;
    /** The handle ID. */
    uint32_t        id;
    /** The handle union. */
    RTHANDLEUNION   u;
} RTPOLLSETHNDENT;
/** Pointer to a handle entry. */
typedef RTPOLLSETHNDENT *PRTPOLLSETHNDENT;

/**
 * Poll set data, POSIX.
 */
typedef struct RTPOLLSETINTERNAL
{
    /** The magic value (RTPOLLSET_MAGIC). */
    uint32_t            u32Magic;
    /** Set when someone is polling or making changes. */
    bool volatile       fBusy;

    /** The number of valid handles in the set. */
    uint32_t            cHandles;
    /** The number of allocated handles. */
    uint32_t            cHandlesAllocated;

    /** Pointer to an array of pollfd structures. */
    struct pollfd      *paPollFds;
    /** Pointer to an array of handles and IDs. */
    PRTPOLLSETHNDENT    paHandles;
} RTPOLLSETINTERNAL;


/**
 * Common worker for RTPoll and RTPollNoResume
 */
static int rtPollNoResumeWorker(RTPOLLSETINTERNAL *pThis, RTMSINTERVAL cMillies, uint32_t *pfEvents, uint32_t *pid)
{
    if (RT_UNLIKELY(pThis->cHandles == 0 && cMillies == RT_INDEFINITE_WAIT))
        return VERR_DEADLOCK;

    /* clear the revents. */
    uint32_t i = pThis->cHandles;
    while (i-- > 0)
        pThis->paPollFds[i].revents = 0;

    int rc = poll(&pThis->paPollFds[0], pThis->cHandles,
                  cMillies == RT_INDEFINITE_WAIT || cMillies >= INT_MAX
                  ? -1
                  : (int)cMillies);
    if (rc == 0)
        return VERR_TIMEOUT;
    if (rc < 0)
        return RTErrConvertFromErrno(errno);

    for (i = 0; i < pThis->cHandles; i++)
        if (pThis->paPollFds[i].revents)
        {
            if (pfEvents)
            {
                *pfEvents = 0;
                if (pThis->paPollFds[i].revents & (POLLIN
#ifdef POLLRDNORM
                                                   | POLLRDNORM     /* just in case */
#endif
#ifdef POLLRDBAND
                                                   | POLLRDBAND     /* ditto */
#endif
#ifdef POLLPRI
                                                   | POLLPRI        /* ditto */
#endif
#ifdef POLLMSG
                                                   | POLLMSG        /* ditto */
#endif
#ifdef POLLWRITE
                                                   | POLLWRITE       /* ditto */
#endif
#ifdef POLLEXTEND
                                                   | POLLEXTEND      /* ditto */
#endif
                                                   )
                   )
                    *pfEvents |= RTPOLL_EVT_READ;

                if (pThis->paPollFds[i].revents & (POLLOUT
#ifdef POLLWRNORM
                                                   | POLLWRNORM     /* just in case */
#endif
#ifdef POLLWRBAND
                                                   | POLLWRBAND     /* ditto */
#endif
                                                   )
                   )
                    *pfEvents |= RTPOLL_EVT_WRITE;

                if (pThis->paPollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL
#ifdef POLLRDHUP
                                                   | POLLRDHUP
#endif
                                                   )
                   )
                    *pfEvents |= RTPOLL_EVT_ERROR;
            }
            if (pid)
                *pid = pThis->paHandles[i].id;
            return VINF_SUCCESS;
        }

    AssertFailed();
    RTThreadYield();
    return VERR_INTERRUPTED;
}


RTDECL(int) RTPoll(RTPOLLSET hPollSet, RTMSINTERVAL cMillies, uint32_t *pfEvents, uint32_t *pid)
{
    RTPOLLSETINTERNAL *pThis = hPollSet;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrNull(pfEvents);
    AssertPtrNull(pid);

    /*
     * Set the busy flag and do the job.
     */
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), VERR_WRONG_ORDER);

    int rc;
    if (cMillies == RT_INDEFINITE_WAIT || cMillies == 0)
    {
        do rc = rtPollNoResumeWorker(pThis, cMillies, pfEvents, pid);
        while (rc == VERR_INTERRUPTED);
    }
    else
    {
        uint64_t MsStart = RTTimeMilliTS();
        rc = rtPollNoResumeWorker(pThis, cMillies, pfEvents, pid);
        while (RT_UNLIKELY(rc == VERR_INTERRUPTED))
        {
            if (RTTimeMilliTS() - MsStart >= cMillies)
            {
                rc = VERR_TIMEOUT;
                break;
            }
            rc = rtPollNoResumeWorker(pThis, cMillies, pfEvents, pid);
        }
    }

    ASMAtomicWriteBool(&pThis->fBusy, false);

    return rc;
}


RTDECL(int) RTPollNoResume(RTPOLLSET hPollSet, RTMSINTERVAL cMillies, uint32_t *pfEvents, uint32_t *pid)
{
    RTPOLLSETINTERNAL *pThis = hPollSet;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrNull(pfEvents);
    AssertPtrNull(pid);

    /*
     * Set the busy flag and do the job.
     */
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), VERR_WRONG_ORDER);

    int rc = rtPollNoResumeWorker(pThis, cMillies, pfEvents, pid);

    ASMAtomicWriteBool(&pThis->fBusy, false);

    return rc;
}


RTDECL(int)  RTPollSetCreate(PRTPOLLSET phPollSet)
{
    AssertPtrReturn(phPollSet, VERR_INVALID_POINTER);
    RTPOLLSETINTERNAL *pThis = (RTPOLLSETINTERNAL *)RTMemAlloc(sizeof(RTPOLLSETINTERNAL));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic             = RTPOLLSET_MAGIC;
    pThis->fBusy                = false;
    pThis->cHandles             = 0;
    pThis->cHandlesAllocated    = 0;
    pThis->paPollFds            = NULL;
    pThis->paHandles            = NULL;

    *phPollSet = pThis;
    return VINF_SUCCESS;
}


RTDECL(int)  RTPollSetDestroy(RTPOLLSET hPollSet)
{
    RTPOLLSETINTERNAL *pThis = hPollSet;
    if (pThis == NIL_RTPOLLSET)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), VERR_WRONG_ORDER);

    ASMAtomicWriteU32(&pThis->u32Magic, ~RTPOLLSET_MAGIC);
    RTMemFree(pThis->paPollFds);
    pThis->paPollFds = NULL;
    RTMemFree(pThis->paHandles);
    pThis->paHandles = NULL;
    RTMemFree(pThis);

    return VINF_SUCCESS;
}


RTDECL(int) RTPollSetAdd(RTPOLLSET hPollSet, PCRTHANDLE pHandle, uint32_t fEvents, uint32_t id)
{
    /*
     * Validate the input (tedious).
     */
    RTPOLLSETINTERNAL *pThis = hPollSet;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!(fEvents & ~RTPOLL_EVT_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(fEvents, VERR_INVALID_PARAMETER);
    AssertReturn(id != UINT32_MAX, VERR_INVALID_PARAMETER);

    if (!pHandle)
        return VINF_SUCCESS;
    AssertPtrReturn(pHandle, VERR_INVALID_POINTER);
    AssertReturn(pHandle->enmType > RTHANDLETYPE_INVALID && pHandle->enmType < RTHANDLETYPE_END, VERR_INVALID_PARAMETER);

    /*
     * Set the busy flag and do the job.
     */
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), VERR_WRONG_ORDER);

    int rc = VINF_SUCCESS;
    int fd = -1;
    switch (pHandle->enmType)
    {
        case RTHANDLETYPE_PIPE:
            if (pHandle->u.hPipe != NIL_RTPIPE)
                fd = RTPipeToNative(pHandle->u.hPipe);
            break;

        //case RTHANDLETYPE_SOCKET:
        //    if (pHandle->u.hSocket != NIL_RTSOCKET)
       //         fd = (int)pHandle->u.hSocket; //fd = RTTcpToNative(pHandle->u.hSocket);
       //     break;

        case RTHANDLETYPE_FILE:
            AssertMsgFailed(("Files are always ready for reading/writing and thus not pollable. Use native APIs for special devices.\n"));
            rc = VERR_POLL_HANDLE_NOT_POLLABLE;
            break;

        case RTHANDLETYPE_THREAD:
            AssertMsgFailed(("Thread handles are currently not pollable\n"));
            rc = VERR_POLL_HANDLE_NOT_POLLABLE;
            break;

        default:
            AssertMsgFailed(("\n"));
            rc = VERR_POLL_HANDLE_NOT_POLLABLE;
            break;
    }
    if (fd != -1)
    {
        uint32_t const i = pThis->cHandles;

        /* Check that the handle ID doesn't exist already. */
        uint32_t j = i;
        while (j-- > 0)
            if (pThis->paHandles[j].id == id)
            {
                rc = VERR_POLL_HANDLE_ID_EXISTS;
                break;
            }
        if (RT_SUCCESS(rc))
        {
            /* Grow the tables if necessary. */
            if (i + 1 > pThis->cHandlesAllocated)
            {
                uint32_t const  c = pThis->cHandlesAllocated + 32;
                void           *pvNew;
                pvNew = RTMemRealloc(pThis->paHandles, c * sizeof(pThis->paHandles[0]));
                if (pvNew)
                {
                    pThis->paHandles = (PRTPOLLSETHNDENT)pvNew;
                    pvNew = RTMemRealloc(pThis->paPollFds, c * sizeof(pThis->paPollFds[0]));
                    if (pvNew)
                        pThis->paPollFds = (struct pollfd *)pvNew;
                    else
                        rc = VERR_NO_MEMORY;
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            if (RT_SUCCESS(rc))
            {
                /* Add it to the poll file descriptor array and call poll to
                   validate the event flags. */
                pThis->paPollFds[i].fd      = fd;
                pThis->paPollFds[i].revents = 0;
                pThis->paPollFds[i].events  = 0;
                if (fEvents & RTPOLL_EVT_READ)
                    pThis->paPollFds[i].events |= POLLIN;
                if (fEvents & RTPOLL_EVT_WRITE)
                    pThis->paPollFds[i].events |= POLLOUT;
                if (fEvents & RTPOLL_EVT_ERROR)
                    pThis->paPollFds[i].events |= POLLERR;

                if (poll(&pThis->paPollFds[i], 1, 0) >= 0)
                {
                    /* Add the handle info and close the transaction. */
                    pThis->paHandles[i].enmType = pHandle->enmType;
                    pThis->paHandles[i].u       = pHandle->u;
                    pThis->paHandles[i].id      = id;

                    pThis->cHandles = i + 1;
                    rc = VINF_SUCCESS;
                }
                else
                {
                    rc = RTErrConvertFromErrno(errno);
                    pThis->paPollFds[i].fd = -1;
                }
            }
        }
    }

    ASMAtomicWriteBool(&pThis->fBusy, false);
    return rc;
}


RTDECL(int) RTPollSetRemove(RTPOLLSET hPollSet, uint32_t id)
{
    /*
     * Validate the input.
     */
    RTPOLLSETINTERNAL *pThis = hPollSet;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(id != UINT32_MAX, VERR_INVALID_PARAMETER);

    /*
     * Set the busy flag and do the job.
     */
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), VERR_WRONG_ORDER);

    int         rc = VERR_POLL_HANDLE_ID_NOT_FOUND;
    uint32_t    i  = pThis->cHandles;
    while (i-- > 0)
        if (pThis->paHandles[i].id == id)
        {
            pThis->cHandles--;
            size_t const cToMove = pThis->cHandles - i;
            if (cToMove)
            {
                memmove(&pThis->paHandles[i], &pThis->paHandles[i + 1], cToMove * sizeof(pThis->paHandles[i]));
                memmove(&pThis->paPollFds[i], &pThis->paPollFds[i + 1], cToMove * sizeof(pThis->paPollFds[i]));
            }
            rc = VINF_SUCCESS;
            break;
        }

    ASMAtomicWriteBool(&pThis->fBusy, false);
    return rc;
}


RTDECL(int) RTPollSetQueryHandle(RTPOLLSET hPollSet, uint32_t id, PRTHANDLE pHandle)
{
    /*
     * Validate the input.
     */
    RTPOLLSETINTERNAL *pThis = hPollSet;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(id != UINT32_MAX, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pHandle, VERR_INVALID_POINTER);

    /*
     * Set the busy flag and do the job.
     */
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), VERR_WRONG_ORDER);

    int         rc = VERR_POLL_HANDLE_ID_NOT_FOUND;
    uint32_t    i  = pThis->cHandles;
    while (i-- > 0)
        if (pThis->paHandles[i].id == id)
        {
            if (pHandle)
            {
                pHandle->enmType = pThis->paHandles[i].enmType;
                pHandle->u       = pThis->paHandles[i].u;
            }
            rc = VINF_SUCCESS;
            break;
        }

    ASMAtomicWriteBool(&pThis->fBusy, false);
    return rc;
}


RTDECL(uint32_t) RTPollSetCount(RTPOLLSET hPollSet)
{
    /*
     * Validate the input.
     */
    RTPOLLSETINTERNAL *pThis = hPollSet;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTPOLLSET_MAGIC, UINT32_MAX);

    /*
     * Set the busy flag and do the job.
     */
    AssertReturn(ASMAtomicCmpXchgBool(&pThis->fBusy, true,  false), UINT32_MAX);
    uint32_t cHandles = pThis->cHandles;
    ASMAtomicWriteBool(&pThis->fBusy, false);

    return cHandles;
}

