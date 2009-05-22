/* $Id$ */
/** @file
 * IPRT - Thread Preemption, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "the-darwin-kernel.h"
#include <iprt/thread.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mp.h>
#include <iprt/asm.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTDARWINPREEMPTHACK
{
    /** The spinlock we exploit for disabling preemption. */
    lck_spin_t         *pSpinLock;
    /** The preemption count for this CPU, to guard against nested calls. */
    uint32_t            cRecursion;
} RTDARWINPREEMPTHACK;
typedef RTDARWINPREEMPTHACK *PRTDARWINPREEMPTHACK;



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTDARWINPREEMPTHACK  g_aPreemptHacks[16]; /* see MAX_CPUS in i386/mp.h */


/**
 * Allocates the per-cpu spin locks used to disable preemption.
 *
 * Called by rtR0InitNative.
 */
int rtThreadPreemptDarwinInit(void)
{
    Assert(g_pDarwinLockGroup);

    for (size_t i = 0; i < RT_ELEMENTS(g_aPreemptHacks); i++)
    {
        g_aPreemptHacks[i].pSpinLock = lck_spin_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (!g_aPreemptHacks[i].pSpinLock)
            return VERR_NO_MEMORY; /* (The caller will invoke rtThreadPreemptDarwinTerm) */
    }
    return VINF_SUCCESS;
}


/**
 * Frees the per-cpu spin locks used to disable preemption.
 *
 * Called by rtR0TermNative.
 */
void rtThreadPreemptDarwinTerm(void)
{
    for (size_t i = 0; i < RT_ELEMENTS(g_aPreemptHacks); i++)
        if (g_aPreemptHacks[i].pSpinLock)
        {
            lck_spin_free(g_aPreemptHacks[i].pSpinLock, g_pDarwinLockGroup);
            g_aPreemptHacks[i].pSpinLock = NULL;
        }
}


RTDECL(bool) RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
    return preemption_enabled();
}


RTDECL(bool) RTThreadPreemptIsPending(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);

    /* HACK ALERT! This ASSUMES that the cpu_pending_ast member of cpu_data_t doesn't move. */
    uint32_t ast_pending;
#if 1
    __asm__ volatile("movl %%gs:%P1,%0\n\t"
                     : "=r" (ast_pending)
                     : "i"  (7*sizeof(void*) + 7*sizeof(int)));
#else
    cpu_data_t *pCpu = current_cpu_datap(void);
    AssertCompileMemberOffset(cpu_data_t, cpu_pending_ast, 7*sizeof(void*) + 7*sizeof(int));
    cpu_pending_ast = pCpu->cpu_pending_ast;
#endif
    AssertMsg(!(ast_pending & UINT32_C(0xfffff000)),("%#x\n", ast_pending));
    return (ast_pending & (AST_PREEMPT | AST_URGENT)) != 0;
}


RTDECL(void) RTThreadPreemptDisable(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->uchDummy != 42);
    pState->uchDummy = 42;

    /*
     * Disable to prevent preemption while we grab the per-cpu spin lock.
     * Note! Only take the lock on the first call or we end up spinning for ever.
     */
    RTCCUINTREG fSavedFlags = ASMIntDisableFlags();
    RTCPUID     idCpu       = RTMpCpuId();
    if (RT_UNLIKELY(idCpu < RT_ELEMENTS(g_aPreemptHacks)))
    {
        Assert(g_aPreemptHacks[idCpu].cRecursion < UINT32_MAX / 2);
        if (++g_aPreemptHacks[idCpu].cRecursion == 1)
        {
            lck_spin_t *pSpinLock = g_aPreemptHacks[idCpu].pSpinLock;
            if (pSpinLock)
                lck_spin_lock(pSpinLock);
            else
                AssertFailed();
        }
    }
    ASMSetFlags(fSavedFlags);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
}


RTDECL(void) RTThreadPreemptRestore(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->uchDummy == 42);
    pState->uchDummy = 0;

    RTCPUID idCpu = RTMpCpuId();
    if (RT_UNLIKELY(idCpu < RT_ELEMENTS(g_aPreemptHacks)))
    {
        Assert(g_aPreemptHacks[idCpu].cRecursion > 0);
        if (--g_aPreemptHacks[idCpu].cRecursion == 0)
        {
            lck_spin_t *pSpinLock = g_aPreemptHacks[idCpu].pSpinLock;
            if (pSpinLock)
                lck_spin_unlock(pSpinLock);
            else
                AssertFailed();
        }
    }
}


