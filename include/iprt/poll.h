/** @file
 * IPRT - Polling I/O Handles.
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

#ifndef ___iprt_poll_h
#define ___iprt_poll_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_poll      RTPoll - Polling I/O Handles
 * @ingroup grp_rt
 * @{
 */

/** @name Poll events
 * @{ */
/** Readable without blocking. */
#define RTPOLL_EVT_READ     RT_BIT_32(0)
/** Writable without blocking. */
#define RTPOLL_EVT_WRITE    RT_BIT_32(1)
/** Error condition, hangup, exception or similar. */
#define RTPOLL_EVT_ERROR    RT_BIT_32(2)
/** @} */

/**
 * Polls on the specified poll set until an event occures on one of the handles
 * or the timeout expires.
 *
 * @returns IPRT status code.
 * @param   hPollSet        The set to poll on.
 * @param   cMillies        Number of milliseconds to wait.  Use
 *                          RT_INDEFINITE_WAIT to wait for ever.
 * @param   pHandle         Where to return the info about the handle.
 * @param   pfEvent         Where to return details about the events that
 *                          occured.
 */
RTDECL(int) RTPoll(RTPIPE hPipe, RTMSINTERVAL cMillies, uint32_t *pfEvent);

/**
 * Creates a poll set with zero or more initial members.
 *
 * @returns IPRT status code.
 * @param   phPollSet       Where to return the poll set handle.
 * @param   cHandles        The number of initial members.
 * @param   paHandles       Array with the initial members.
 * @param   pafEvents       Which events to poll for.  This is an array running
 *                          parallel to @a paHandles.  If NULL, we assume
 *                          RTPOLL_EVT_READ and RTPOLL_EVT_ERROR.
 */
RTDECL(int)  RTPollSetCreate(PRTPOLLSET hPollSet, size_t cHandles, PCRTHANDLE paHandles, uint32_t const *pafEvents);

/**
 * Destroys a poll set.
 *
 * @returns IPRT status code.
 * @param   hPollSet        The poll set to destroy.  NIL_POLLSET is quietly
 *                          ignored (VINF_SUCCESS).
 */
RTDECL(int)  RTPollSetDestroy(RTPOLLSET hPollSet);

/**
 * Adds a generic handle to the poll set.
 *
 * @returns IPRT status code
 * @param   hPollSet        The poll set to modify.
 * @param   pHandle         The handle to add.
 * @param   fEvents         Which events to poll for.
 */
RTDECL(int) RTPollSetAdd(RTPOLLSET hPollSet, PCRTHANDLE pHandle, uint32_t fEvents);

/**
 * Removes a generic handle from the poll set.
 *
 * @returns IPRT status code
 * @param   hPollSet        The poll set to modify.
 * @param   pHandle         The handle to remove.
 */
RTDECL(int) RTPollSetRemove(RTPOLLSET hPollSet, PCRTHANDLE pHandle);

/**
 * Adds a pipe handle to the set.
 *
 * @returns IPRT status code.
 * @param   hPollSet        The poll set.
 * @param   hPipe           The pipe handle.
 * @param   fEvents         Which events to poll for.
 *
 * @todo    Maybe we could figure out what to poll for depending on the kind of
 *          pipe we're dealing with.
 */
DECLINLINE(int) RTPollSetAddPipe(RTPOLLSET hPollSet, RTPIPE hPipe, uint32_t fEvents)
{
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipe;
    return RTPollSetAdd(hPollSet, &Handle, fEvents);
}

/**
 * Adds a socket handle to the set.
 *
 * @returns IPRT status code.
 * @param   hPollSet        The poll set.
 * @param   hSocket         The socket handle.
 * @param   fEvents         Which events to poll for.
 */
DECLINLINE(int) RTPollSetAddSocket(RTPOLLSET hPollSet, RTSOCKET hSocket, uint32_t fEvents)
{
    RTHANDLE Handle;
    Handle.enmType   = RTHANDLETYPE_SOCKET;
    Handle.u.hSocket = hSocket;
    return RTPollSetAdd(hPollSet, &Handle, fEvents);
}

/** @} */

RT_C_DECLS_END

#endif

