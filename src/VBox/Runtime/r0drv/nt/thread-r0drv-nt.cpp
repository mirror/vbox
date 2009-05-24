/* $Id$ */
/** @file
 * IPRT - Threads, Ring-0 Driver, NT.
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
#include "the-nt-kernel.h"

#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

#include "internal-r0drv-nt.h"

__BEGIN_DECLS
NTSTATUS NTAPI ZwYieldExecution(void);
__END_DECLS


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)PsGetCurrentThread();
}


RTDECL(int)   RTThreadSleep(unsigned cMillies)
{
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t)cMillies * 10000;
    NTSTATUS rcNt = KeDelayExecutionThread(KernelMode, TRUE, &Interval);
    switch (rcNt)
    {
        case STATUS_SUCCESS:
            return VINF_SUCCESS;
        case STATUS_ALERTED:
        case STATUS_USER_APC:
            return VERR_INTERRUPTED;
        default:
            return RTErrConvertFromNtStatus(rcNt);
    }
}


RTDECL(bool) RTThreadYield(void)
{
    return ZwYieldExecution() != STATUS_NO_YIELD_PERFORMED;
}


RTDECL(bool) RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
    KIRQL Irql = KeGetCurrentIrql();
    if (Irql > APC_LEVEL)
        return false;
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
    if (!(ASMGetFlags() & 0x00000200 /* X86_EFL_IF */))
        return false;
#endif
    return true;
}


RTDECL(bool) RTThreadPreemptIsPending(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);

    /*
     * Read the globals and check if they are useful.
     */
    uint32_t const offQuantumEnd     = g_offrtNtPbQuantumEnd;
    uint32_t const cbQuantumEnd      = g_cbrtNtPbQuantumEnd;
    uint32_t const offDpcQueueDepth  = g_offrtNtPbDpcQueueDepth;
    if (!offQuantumEnd && !cbQuantumEnd && !offDpcQueueDepth)
        return false;
    Assert((offQuantumEnd && cbQuantumEnd) || (!offQuantumEnd && !cbQuantumEnd));

    /*
     * Disable interrupts so we won't be messed around.
     */
    bool            fPending;
    RTCCUINTREG     fSavedFlags  = ASMIntDisableFlags();

#ifdef RT_ARCH_X86
    PKPCR       pPcr   = (PKPCR)__readfsdword(RT_OFFSETOF(KPCR,SelfPcr));
    uint8_t    *pbPrcb = (uint8_t *)pPcr->Prcb;

#elif defined(RT_ARCH_AMD64)
    /* HACK ALERT! The offset is from windbg/vista64. */
    PKPCR       pPcr   = (PKPCR)__readgsqword(RT_OFFSETOF(KPCR,Self));
    uint8_t    *pbPrcb = (uint8_t *)pPcr->CurrentPrcb;

#else
# error "port me"
#endif

    /* Check QuantumEnd. */
    if (cbQuantumEnd == 1)
    {
        uint8_t volatile *pbQuantumEnd = (uint8_t volatile *)(pbPrcb + offQuantumEnd);
        fPending = *pbQuantumEnd == TRUE;
    }
    else if (cbQuantumEnd == sizeof(uint32_t))
    {
        uint32_t volatile *pu32QuantumEnd = (uint32_t volatile *)(pbPrcb + offQuantumEnd);
        fPending = *pu32QuantumEnd != 0;
    }

    /* Check DpcQueueDepth. */
    if (    !fPending
        &&  offDpcQueueDepth)
    {
        uint32_t volatile *pu32DpcQueueDepth = (uint32_t volatile *)(pbPrcb + offDpcQueueDepth);
        fPending = *pu32DpcQueueDepth > 0;
    }

    ASMSetFlags(fSavedFlags);
    return fPending;
}


RTDECL(void) RTThreadPreemptDisable(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->uchOldIrql == 255);
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);

    KeRaiseIrql(DISPATCH_LEVEL, &pState->uchOldIrql);
}


RTDECL(void) RTThreadPreemptRestore(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);

    KeLowerIrql(pState->uchOldIrql);
    pState->uchOldIrql = 255;
}

