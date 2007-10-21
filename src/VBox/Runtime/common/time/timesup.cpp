/* $Id$ */
/** @file
 * innotek Portable Runtime - Time using SUPLib.
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
#define LOG_GROUP RTLOGGROUP_TIME
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <VBox/sup.h>
#include "internal/time.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifndef IN_GUEST
/** The previously returned nano TS.
 * This handles TSC drift on SMP systems and expired interval.
 * This is a valid range u64NanoTS to u64NanoTS + 1000000000 (ie. 1sec).
 */
static uint64_t volatile    s_u64PrevNanoTS = 0;
/**
 * Number of times we've had to resort to 1ns walking. */
static uint32_t volatile    g_c1nsSteps = 0;
#endif


/**
 * Calculate NanoTS using the information in the global information page (GIP)
 * which the support library (SUPLib) exports.
 *
 * This function guarantees that the returned timestamp is later (in time) than
 * any previous calls in the same thread.
 *
 * @returns Nanosecond timestamp.
 *
 * @remark  The way the ever increasing time guarantee is currently implemented means
 *          that if you call this function at a freqency higher than 1GHz you're in for
 *          trouble. We currently assume that no idiot will do that for real life purposes.
 */
DECLINLINE(uint64_t) rtTimeNanoTSInternal(void)
{
#ifndef IN_GUEST
    uint64_t    u64Delta;
    uint32_t    u32NanoTSFactor0;
    uint64_t    u64TSC;
    uint64_t    u64NanoTS;
    uint32_t    u32UpdateIntervalTSC;
    uint32_t    u32TransactionId;
    PSUPGLOBALINFOPAGE pGip;

    /*
     * Read the data.
     */
    for (;;)
    {
        pGip = g_pSUPGlobalInfoPage;
#ifdef IN_RING3
        if (!pGip || pGip->u32Magic != SUPGLOBALINFOPAGE_MAGIC)
            return RTTimeSystemNanoTS();
#endif

        if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        {
            u32TransactionId = pGip->aCPUs[0].u32TransactionId;
#ifdef RT_OS_L4
            Assert((u32TransactionId & 1) == 0);
#endif
            u32UpdateIntervalTSC = pGip->aCPUs[0].u32UpdateIntervalTSC;
            u64NanoTS = pGip->aCPUs[0].u64NanoTS;
            u64TSC = pGip->aCPUs[0].u64TSC;
            u32NanoTSFactor0 = pGip->u32UpdateIntervalNS;
            u64Delta = ASMReadTSC();
            if (RT_UNLIKELY(    pGip->aCPUs[0].u32TransactionId != u32TransactionId
                            ||  (u32TransactionId & 1)))
                continue;
        }
        else
        {
            /* SUPGIPMODE_ASYNC_TSC */
            PSUPGIPCPU pGipCpu;

            uint8_t u8ApicId = ASMGetApicId();
            if (RT_LIKELY(u8ApicId < RT_ELEMENTS(pGip->aCPUs)))
                pGipCpu = &pGip->aCPUs[u8ApicId];
            else
            {
                AssertMsgFailed(("%x\n", u8ApicId));
                pGipCpu = &pGip->aCPUs[0];
            }

            u32TransactionId = pGipCpu->u32TransactionId;
#ifdef RT_OS_L4
            Assert((u32TransactionId & 1) == 0);
#endif
            u32UpdateIntervalTSC = pGipCpu->u32UpdateIntervalTSC;
            u64NanoTS = pGipCpu->u64NanoTS;
            u64TSC = pGipCpu->u64TSC;
            u32NanoTSFactor0 = pGip->u32UpdateIntervalNS;
            u64Delta = ASMReadTSC();
            if (RT_UNLIKELY(u8ApicId != ASMGetApicId()))
                continue;
            if (RT_UNLIKELY(    pGipCpu->u32TransactionId != u32TransactionId
                            ||  (u32TransactionId & 1)))
                continue;
        }
        break;
    }

    /*
     * Calc NanoTS delta.
     */
    u64Delta -= u64TSC;
    if (u64Delta > u32UpdateIntervalTSC)
    {
        /*
         * We've expired the interval. Do 1ns per call until we've
         * got valid TSC deltas again (s_u64PrevNanoTS takes care of this).
         */
        u64Delta = u32UpdateIntervalTSC;
    }
#if !defined(_MSC_VER) || defined(RT_ARCH_AMD64) /* GCC makes very pretty code from these two inline calls, while MSC cannot. */
    u64Delta = ASMMult2xU32RetU64((uint32_t)u64Delta, u32NanoTSFactor0);
    u64Delta = ASMDivU64ByU32RetU32(u64Delta, u32UpdateIntervalTSC);
#else
    __asm
    {
        mov     eax, dword ptr [u64Delta]
        mul     dword ptr [u32NanoTSFactor0]
        div     dword ptr [u32UpdateIntervalTSC]
        mov     dword ptr [u64Delta], eax
        xor     edx, edx
        mov     dword ptr [u64Delta + 4], edx
    }
#endif

    /*
     * The most frequent case is that the delta is either too old
     * or that our timestamp is higher (relative to u64NanoTS) than it.
     */
    uint64_t u64;
    uint64_t u64PrevNanoTS = ASMAtomicReadU64(&s_u64PrevNanoTS);
    uint64_t u64DeltaPrev = u64PrevNanoTS - u64NanoTS;
    if (    u64DeltaPrev > 1000000000                       /* (invalid prev) */
        ||  (uint32_t)u64DeltaPrev < (uint32_t)u64Delta)    /* (we're later) */
    {
        u64 = u64Delta + u64NanoTS;
        if (ASMAtomicCmpXchgU64(&s_u64PrevNanoTS, u64, u64PrevNanoTS))
            return u64;
    }
    else
    {
        /*
         * Our timestamp is lower than the last returned timestamp;
         * advance 1ns beyond that.
         */
        u64Delta = u64DeltaPrev + 1;
        u64 = u64Delta + u64NanoTS;
        ASMAtomicIncU32(&g_c1nsSteps);
    }

    /*
     * Attempt updating the previous value.
     *      u64 == timestamp, u64Delta == delta relative to u64NanoTS.
     */
    for (int cTries = 100;;)
    {
        u64PrevNanoTS = ASMAtomicReadU64(&s_u64PrevNanoTS);
        u64DeltaPrev = u64PrevNanoTS - u64NanoTS;
        if (u64DeltaPrev > u64Delta)
            break;
        if (ASMAtomicCmpXchgU64(&s_u64PrevNanoTS, u64, u64PrevNanoTS))
            break;
        if (--cTries <= 0)
        {
            AssertBreakpoint(); /* (recursion) */
            break;
        }
    }

    return u64;
#else /* IN_GUEST */
    return RTTimeSystemNanoTS();
#endif /* IN_GUEST */
}


/**
 * Gets the current nanosecond timestamp.
 *
 * @returns nanosecond timestamp.
 */
RTDECL(uint64_t) RTTimeNanoTS(void)
{
    return rtTimeNanoTSInternal();
}


/**
 * Gets the current millisecond timestamp.
 *
 * @returns millisecond timestamp.
 */
RTDECL(uint64_t) RTTimeMilliTS(void)
{
    return rtTimeNanoTSInternal() / 1000000;
}


#ifndef IN_GUEST
/**
 * Debugging the time api.
 *
 * @returns the number of 1ns steps which has been applied by rtTimeNanoTSInternal().
 */
RTDECL(uint32_t) RTTime1nsSteps(void)
{
    return g_c1nsSteps;
}
#endif
