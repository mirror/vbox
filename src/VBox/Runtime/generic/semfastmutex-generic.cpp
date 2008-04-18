/* $Id$ */
/** @file
 * innotek Portable Runtime - Fast Mutex, Generic.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/err.h>
#include <iprt/critsect.h>



RTDECL(int) RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem)
{
    PRTCRITSECT pCritSect = (PRTCRITSECT)RTMemAlloc(sizeof(RTCRITSECT));
    if (!pCritSect)
        return VERR_NO_MEMORY;
    int rc = RTCritSectInit(pCritSect);
    if (RT_SUCCESS(rc))
        *pMutexSem = (RTSEMFASTMUTEX)pCritSect;
    return rc;
}


RTDECL(int) RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem)
{
    if (MutexSem == NIL_RTSEMFASTMUTEX)
        return VERR_INVALID_PARAMETER;
    PRTCRITSECT pCritSect = (PRTCRITSECT)MutexSem;
    int rc = RTCritSectDelete(pCritSect);
    if (RT_SUCCESS(rc))
        RTMemFree(pCritSect);
    return rc;
}


RTDECL(int) RTSemFastMutexRequest(RTSEMFASTMUTEX MutexSem)
{
    return RTCritSectEnter((PRTCRITSECT)MutexSem);
}


RTDECL(int) RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem)
{
    return RTCritSectLeave((PRTCRITSECT)MutexSem);
}

