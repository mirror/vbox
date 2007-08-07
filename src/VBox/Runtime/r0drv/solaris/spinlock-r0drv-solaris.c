/* $Id$ */
/** @file
 * innotek Portable Runtime - Spinlocks, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the struct mutex type.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** A Solaris spinlock. */
    struct mutex        Mtx;
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;


RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock)
{
    /*
     * Allocate.
     */
    AssertCompile(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pSpinlockInt));
    if (!pSpinlockInt)
        return VERR_NO_MEMORY;

    /*
     * Initialize & return.
     */
    pSpinlockInt->u32Magic = RTSPINLOCK_MAGIC;
    mutex_init(&pSpinlockInt->Mtx, "iprt-spinlock", MUTEX_SPIN, NULL);
    *pSpinlock = pSpinlockInt;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock)
{
    /*
     * Validate input.
     */
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    if (!pSpinlockInt)
        return VERR_INVALID_PARAMETER;
    AssertMsgReturn(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
                    ("Invalid spinlock %p magic=%#x\n", pSpinlockInt, pSpinlockInt->u32Magic),
                    VERR_INVALID_PARAMETER);

    /*
     * Make the lock invalid and release the memory.
     */
    ASMAtomicIncU32(&pSpinlockInt->u32Magic);
    mutex_destroy(&pSpinlockInt->Mtx);
    RTMemFree(pSpinlockInt);
    return VINF_SUCCESS;
}


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    NOREF(pTmp);

    mutex_enter(&pSpinlockInt->Mtx);
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    NOREF(pTmp);

    mutex_exit(&pSpinlockInt->Mtx);
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    NOREF(pTmp);

    mutex_enter(&pSpinlockInt->Mtx);
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);
    NOREF(pTmp);

    mutex_exit(&pSpinlockInt->Mtx);
}

