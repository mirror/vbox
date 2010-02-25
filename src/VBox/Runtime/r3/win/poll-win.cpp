/* $Id$ */
/** @file
 * IPRT - Polling I/O Handles, Windows Implementation.
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

#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/magics.h"


RTDECL(int) RTPoll(RTPOLLSET hPollSet, RTMSINTERVAL cMillies, uint32_t *pfEvents, uint32_t *pid)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPollNoResume(RTPOLLSET hPollSet, RTMSINTERVAL cMillies, uint32_t *pfEvents, uint32_t *pid)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int)  RTPollSetCreate(PRTPOLLSET hPollSet)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int)  RTPollSetDestroy(RTPOLLSET hPollSet)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPollSetAdd(RTPOLLSET hPollSet, PCRTHANDLE pHandle, uint32_t fEvents, uint32_t id)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPollSetRemove(RTPOLLSET hPollSet, uint32_t id)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTPollSetQueryHandle(RTPOLLSET hPollSet, uint32_t id, PRTHANDLE pHandle)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(uint32_t) RTPollSetCount(RTPOLLSET hPollSet)
{
    return UINT32_MAX;
}

