/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Spinlocks, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
#include "the-darwin-kernel.h"
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
 * Wrapper for the KSPIN_LOCK type.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The Darwin spinlock structure. */
    lck_spin_t         *pSpinLock;
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
    Assert(g_pDarwinLockGroup);
    pSpinlockInt->pSpinLock = lck_spin_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
    if (!pSpinlockInt->pSpinLock)
    {
        RTMemFree(pSpinlockInt);
        return VERR_NO_MEMORY;
    }

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

    Assert(g_pDarwinLockGroup);
    lck_spin_destroy(pSpinlockInt->pSpinLock, g_pDarwinLockGroup);
    pSpinlockInt->pSpinLock = NULL;

    RTMemFree(pSpinlockInt);
    return VINF_SUCCESS;
}


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    lck_spin_lock(pSpinlockInt->pSpinLock);

    pTmp->uFlags = ASMGetFlags();
    ASMIntDisable();
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    ASMSetFlags(pTmp->uFlags);
    lck_spin_unlock(pSpinlockInt->pSpinLock);
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    lck_spin_lock(pSpinlockInt->pSpinLock);
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertPtr(pSpinlockInt);
    Assert(pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    lck_spin_unlock(pSpinlockInt->pSpinLock);
}

