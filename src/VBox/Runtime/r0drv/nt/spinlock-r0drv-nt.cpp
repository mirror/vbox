/* $Id$ */
/** @file
 * IPRT - Spinlocks, Ring-0 Driver, NT.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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

#include <iprt/asm.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>

#include "internal/magics.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Apply the NoIrq hack if defined. */
#define RTSPINLOCK_NT_HACK_NOIRQ

#ifdef RTSPINLOCK_NT_HACK_NOIRQ
/** Indicates that the spinlock is taken. */
# define RTSPINLOCK_NT_HACK_NOIRQ_TAKEN  UINT32(0x00c0ffee)
/** Indicates that the spinlock is taken. */
# define RTSPINLOCK_NT_HACK_NOIRQ_FREE   UINT32(0xfe0000fe)
#endif


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
#ifdef RTSPINLOCK_NT_HACK_NOIRQ
    /** Spinlock hack. */
    uint32_t volatile   u32Hack;
#endif
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
#ifdef RTSPINLOCK_NT_HACK_NOIRQ
    pSpinlockInt->u32Hack  = RTSPINLOCK_NT_HACK_NOIRQ_FREE;
#endif
    KeInitializeSpinLock(&pSpinlockInt->Spinlock);
    Assert(sizeof(KIRQL) == sizeof(unsigned char));
    AssertCompile(sizeof(KIRQL) == sizeof(unsigned char));

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
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC, ("magic=%#x\n", pSpinlockInt->u32Magic));

#ifndef RTSPINLOCK_NT_HACK_NOIRQ
    KeAcquireSpinLock(&pSpinlockInt->Spinlock, &pTmp->uchIrqL);
    pTmp->uFlags = ASMGetFlags();
    ASMIntDisable();
#else
    pTmp->uchIrqL = KeGetCurrentIrql();
    if (pTmp->uchIrqL < DISPATCH_LEVEL)
    {
        KeRaiseIrql(DISPATCH_LEVEL, &pTmp->uchIrqL);
        Assert(pTmp->uchIrqL < DISPATCH_LEVEL);
    }
    pTmp->uFlags = ASMGetFlags();
    ASMIntDisable();
    if (!ASMAtomicCmpXchgU32(&pSpinlockInt->u32Hack, RTSPINLOCK_NT_HACK_NOIRQ_TAKEN, RTSPINLOCK_NT_HACK_NOIRQ_FREE))
    {
        while (!ASMAtomicCmpXchgU32(&pSpinlockInt->u32Hack, RTSPINLOCK_NT_HACK_NOIRQ_TAKEN, RTSPINLOCK_NT_HACK_NOIRQ_FREE))
            ASMNopPause();
    }
#endif
}


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC, ("magic=%#x\n", pSpinlockInt->u32Magic));

#ifndef RTSPINLOCK_NT_HACK_NOIRQ
    ASMSetFlags(pTmp->uFlags);
    KeReleaseSpinLock(&pSpinlockInt->Spinlock, pTmp->uchIrqL);
#else
    Assert(pSpinlockInt->u32Hack == RTSPINLOCK_NT_HACK_NOIRQ_TAKEN);
    ASMAtomicWriteU32(&pSpinlockInt->u32Hack, RTSPINLOCK_NT_HACK_NOIRQ_FREE);
    ASMSetFlags(pTmp->uFlags);
    if (pTmp->uchIrqL < DISPATCH_LEVEL)
        KeLowerIrql(pTmp->uchIrqL);
#endif
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC, ("magic=%#x\n", pSpinlockInt->u32Magic));

    KeAcquireSpinLock(&pSpinlockInt->Spinlock, &pTmp->uchIrqL);
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC, ("magic=%#x\n", pSpinlockInt->u32Magic));

    KeReleaseSpinLock(&pSpinlockInt->Spinlock, pTmp->uchIrqL);
}
