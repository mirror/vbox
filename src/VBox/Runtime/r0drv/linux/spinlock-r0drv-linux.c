/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Spinlocks, Ring-0 Driver, Linux.
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
#include "the-linux-kernel.h"

#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include "internal/magics.h"

#include <linux/spinlock.h> /** @todo why is this here and not in the-linux-kernel.h? */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the spinlock_t structure.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The linux spinlock structure. */
    spinlock_t          Spinlock;
#if !defined(CONFIG_SMP) || defined(__AMD64__)
    /** A placeholder on Uni systems so we won't piss off RTMemAlloc(). */
    void                *pvUniDummy;
#endif
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;



RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock)
{
    /*
     * Allocate.
     */
    PRTSPINLOCKINTERNAL pSpinlockInt;
    Assert(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    pSpinlockInt = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pSpinlockInt));
    if (!pSpinlockInt)
        return VERR_NO_MEMORY;
    /*
     * Initialize and return.
     */
    pSpinlockInt->u32Magic = RTSPINLOCK_MAGIC;
    spin_lock_init(&pSpinlockInt->Spinlock);

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
    if (pSpinlockInt->u32Magic != RTSPINLOCK_MAGIC)
    {
        AssertMsgFailed(("Invalid spinlock %p magic=%#x\n", pSpinlockInt, pSpinlockInt->u32Magic));
        return VERR_INVALID_PARAMETER;
    }

    ASMAtomicIncU32(&pSpinlockInt->u32Magic);
    RTMemFree(pSpinlockInt);
    return VINF_SUCCESS;
}


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt);

    spin_lock_irqsave(&pSpinlockInt->Spinlock, pTmp->flFlags);
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt);

    spin_unlock_irqrestore(&pSpinlockInt->Spinlock, pTmp->flFlags);
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt); NOREF(pTmp);

    spin_lock(&pSpinlockInt->Spinlock);
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt); NOREF(pTmp);

    spin_unlock(&pSpinlockInt->Spinlock);
}

