/* $Id$ */
/** @file
 * innotek Portable Runtime -  Single Release Event Semaphores, Ring-0 Driver, NT.
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
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-nt-kernel.h"
#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * NT event semaphore.
 */
typedef struct RTSEMEVENTINTERNAL
{
    /** Magic value (RTSEMEVENT_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The NT Event object. */
    KEVENT              Event;
} RTSEMEVENTINTERNAL, *PRTSEMEVENTINTERNAL;


RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    Assert(sizeof(RTSEMEVENTINTERNAL) > sizeof(void *));
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)RTMemAlloc(sizeof(*pEventInt));
    if (pEventInt)
    {
        pEventInt->u32Magic = RTSEMEVENT_MAGIC;
        KeInitializeEvent(&pEventInt->Event, SynchronizationEvent, FALSE);
        *pEventSem = pEventInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    if (!pEventInt)
        return VERR_INVALID_PARAMETER;
    if (pEventInt->u32Magic != RTSEMEVENT_MAGIC)
    {
        AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p\n", pEventInt->u32Magic, pEventInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicIncU32(&pEventInt->u32Magic);
    KeSetEvent(&pEventInt->Event, 0xfff, FALSE);
    RTMemFree(pEventInt);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    if (!pEventInt)
        return VERR_INVALID_PARAMETER;
    if (    !pEventInt
        ||  pEventInt->u32Magic != RTSEMEVENT_MAGIC)
    {
        AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p\n", pEventInt ? pEventInt->u32Magic : 0, pEventInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Signal the event object.
     */
    KeSetEvent(&pEventInt->Event, 1, FALSE);
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT EventSem, unsigned cMillies, bool fInterruptible)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    if (!pEventInt)
        return VERR_INVALID_PARAMETER;
    if (    !pEventInt
        ||  pEventInt->u32Magic != RTSEMEVENT_MAGIC)
    {
        AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p\n", pEventInt ? pEventInt->u32Magic : 0, pEventInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Wait for it.
     */
    NTSTATUS rcNt;
    if (cMillies == RT_INDEFINITE_WAIT)
        rcNt = KeWaitForSingleObject(&pEventInt->Event, Executive, KernelMode, fInterruptible, NULL);
    else
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -(int64_t)cMillies * 10000;
        rcNt = KeWaitForSingleObject(&pEventInt->Event, Executive, KernelMode, fInterruptible, &Timeout);
    }
    switch (rcNt)
    {
        case STATUS_SUCCESS:
            if (pEventInt->u32Magic == RTSEMEVENT_MAGIC)
                return VINF_SUCCESS;
            return VERR_SEM_DESTROYED;
        case STATUS_ALERTED:
            return VERR_INTERRUPTED;
        case STATUS_USER_APC:
            return VERR_INTERRUPTED;
        case STATUS_TIMEOUT:
            return VERR_TIMEOUT;
        default:
            AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p: wait returned %lx!\n",
                             pEventInt->u32Magic, pEventInt, (long)rcNt));
            return VERR_INTERNAL_ERROR;
    }
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, false /* fInterruptible */);
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, true /* fInterruptible */);
}

