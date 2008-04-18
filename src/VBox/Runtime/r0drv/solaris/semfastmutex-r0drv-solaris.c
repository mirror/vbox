/* $Id$ */
/** @file
 * innotek Portable Runtime - Fast Mutex Semaphores, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/semaphore.h>
#include <iprt/err.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the Solaris mutex.
 */
typedef struct RTSEMFASTMUTEXINTERNAL
{
    /** Magic value (RTSEMFASTMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** The Solaris mutex. */
    kmutex_t            Mtx;
} RTSEMFASTMUTEXINTERNAL, *PRTSEMFASTMUTEXINTERNAL;


RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem)
{
    AssertCompile(sizeof(RTSEMFASTMUTEXINTERNAL) > sizeof(void *));
    AssertPtrReturn(pMutexSem, VERR_INVALID_POINTER);

    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)RTMemAlloc(sizeof(*pFastInt));
    if (pFastInt)
    {
        pFastInt->u32Magic = RTSEMFASTMUTEX_MAGIC;
        mutex_init (&pFastInt->Mtx, "IPRT Fast Mutex Semaphore", MUTEX_DRIVER, NULL);
        *pMutexSem = pFastInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem)
{
    if (MutexSem == NIL_RTSEMFASTMUTEX)
        return VERR_INVALID_PARAMETER;
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    ASMAtomicXchgU32(&pFastInt->u32Magic, RTSEMFASTMUTEX_MAGIC_DEAD);
    mutex_destroy(&pFastInt->Mtx);
    RTMemFree(pFastInt);

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    mutex_enter(&pFastInt->Mtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    mutex_exit(&pFastInt->Mtx);
    return VINF_SUCCESS;
}

