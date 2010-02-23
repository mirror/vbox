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

/**
 * Polls on the specified poll set until timeout or one of them becomes ready.
 *
 * @returns IPRT status code.
 * @param   hPollSet        The set to poll on.
 * @param   cMillies        Number of milliseconds to wait.  Use
 *                          RT_INDEFINITE_WAIT to wait for ever.
 * @param   pHandle         Where to return the info for the read handle.
 * @param   pfWhy           Where to return details about why the handle is
 *                          considered ready.
 */
RTDECL(int) RTPoll(RTPIPE hPipe, RTMSINTERVAL cMillies);

/**
 * Creates a poll set with zero or more initial members.
 *
 * @returns IPRT status code.
 * @param   phPollSet       Where to return the poll set handle.
 * @param   cHandles        The number of initial members.
 * @param   paHandles       Array with the initial members.
 */
RTDECL(int)  RTPollCreateSet(PRTPOLLSET hPollSet, size_t cHandles, PCRTHANDLE paHandles);

/**
 * Destroys a poll set.
 *
 * @returns IPRT status code.
 * @param   hPollSet        The poll set to destroy.  NIL_POLLSET is quietly
 *                          ignored (VINF_SUCCESS).
 */
RTDECL(int)  RTPollDestroySet(RTPOLLSET hPollSet);

/**
 * Adds a handle to the poll set.
 *
 * @returns IPRT status code
 * @param   hPollSet        The poll set to modify.
 * @param   pHandle         The handle to add.
 */
RTDECL(int) RTPollAddToSet(RTPOLLSET hPollSet, PCRTHANDLE pHandle);

/**
 * Removes a handle from the poll set.
 *
 * @returns IPRT status code
 * @param   hPollSet        The poll set to modify.
 * @param   pHandle         The handle to remove.
 */
RTDECL(int) RTPollRemoveFromSet(RTPOLLSET hPollSet, PCRTHANDLE pHandle);

/** @} */

RT_C_DECLS_END

#endif



