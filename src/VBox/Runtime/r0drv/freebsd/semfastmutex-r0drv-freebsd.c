/* $Id$ */
/** @file
 * IPRT - Fast Mutex Semaphores, Ring-0 Driver, FreeBSD.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-freebsd-kernel.h"

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
 * Wrapper for the FreeBSD (sleep) mutex.
 */
typedef struct RTSEMFASTMUTEXINTERNAL
{
    /** Magic value (RTSEMFASTMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** The FreeBSD (sleep) mutex. */
    struct mtx          Mtx;
} RTSEMFASTMUTEXINTERNAL, *PRTSEMFASTMUTEXINTERNAL;


RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem)
{
    AssertCompile(sizeof(RTSEMFASTMUTEXINTERNAL) > sizeof(void *));
    AssertPtrReturn(pMutexSem, VERR_INVALID_POINTER);

    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)RTMemAllocZ(sizeof(*pFastInt));
    if (pFastInt)
    {
        pFastInt->u32Magic = RTSEMFASTMUTEX_MAGIC;
        mtx_init(&pFastInt->Mtx, "IPRT Fast Mutex Semaphore", NULL, MTX_DEF | MTX_RECURSE);
        *pMutexSem = pFastInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem)
{
    if (MutexSem == NIL_RTSEMFASTMUTEX) /* don't bitch */
        return VERR_INVALID_PARAMETER;
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    ASMAtomicXchgU32(&pFastInt->u32Magic, RTSEMFASTMUTEX_MAGIC_DEAD);
    mtx_destroy(&pFastInt->Mtx);
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

    mtx_lock(&pFastInt->Mtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    mtx_unlock(&pFastInt->Mtx);
    return VINF_SUCCESS;
}

