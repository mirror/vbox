/* $Id$ */
/** @file
 * innotek Portable Runtime - Fast Mutex, Generic.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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

