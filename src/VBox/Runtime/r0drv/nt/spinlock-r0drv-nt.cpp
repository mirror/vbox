/* $Id$ */
/** @file
 * innotek Portable Runtime - Spinlocks, Ring-0 Driver, NT.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-nt-kernel.h"

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
    /** The NT spinlock structure. */
    KSPIN_LOCK          Spinlock;
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;



RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock)
{
    /*
     * Allocate.
     */
    Assert(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pSpinlockInt));
    if (!pSpinlockInt)
        return VERR_NO_MEMORY;

    /*
     * Initialize & return.
     */
    pSpinlockInt->u32Magic = RTSPINLOCK_MAGIC;
    KeInitializeSpinLock(&pSpinlockInt->Spinlock);
    Assert(sizeof(KIRQL) == sizeof(unsigned char));

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
    Assert(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    KeAcquireSpinLock(&pSpinlockInt->Spinlock, &pTmp->uchIrqL);
    pTmp->uFlags = ASMGetFlags();
    ASMIntDisable();
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    Assert(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    ASMSetFlags(pTmp->uFlags);
    KeReleaseSpinLock(&pSpinlockInt->Spinlock, pTmp->uchIrqL);
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    Assert(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    KeAcquireSpinLock(&pSpinlockInt->Spinlock, &pTmp->uchIrqL);
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    Assert(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC);

    KeReleaseSpinLock(&pSpinlockInt->Spinlock, pTmp->uchIrqL);
}

