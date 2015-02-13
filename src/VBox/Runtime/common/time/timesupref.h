/* $Id$ */
/** @file
 * IPRT - Time using SUPLib, the C Code Template.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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


/**
 * The C reference implementation of the assembly routines.
 *
 * Calculate NanoTS using the information in the global information page (GIP)
 * which the support library (SUPLib) exports.
 *
 * This function guarantees that the returned timestamp is later (in time) than
 * any previous calls in the same thread.
 *
 * @remark  The way the ever increasing time guarantee is currently implemented means
 *          that if you call this function at a frequency higher than 1GHz you're in for
 *          trouble. We currently assume that no idiot will do that for real life purposes.
 *
 * @returns Nanosecond timestamp.
 * @param   pData       Pointer to the data structure.
 */
RTDECL(uint64_t) rtTimeNanoTSInternalRef(PRTTIMENANOTSDATA pData)
{
    uint64_t    u64Delta;
#if IS_GIP_MODE_WITH_DELTA(GIP_MODE)
    int64_t     i64TscDelta;
#endif
    uint32_t    u32NanoTSFactor0;
    uint64_t    u64TSC;
    uint64_t    u64NanoTS;
    uint32_t    u32UpdateIntervalTSC;
    uint64_t    u64PrevNanoTS;

    /*
     * Read the GIP data and the previous value.
     */
    for (;;)
    {
        PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
#ifdef IN_RING3
        if (RT_UNLIKELY(!pGip || pGip->u32Magic != SUPGLOBALINFOPAGE_MAGIC))
            return pData->pfnRediscover(pData);
#endif
#if GIP_MODE == GIP_MODE_ASYNC || IS_GIP_MODE_WITH_DELTA(GIP_MODE)
        uint8_t const u8ApicId  = ASMGetApicId();
        PSUPGIPCPU pGipCpu      = &pGip->aCPUs[pGip->aiCpuFromApicId[u8ApicId]];
#endif

#ifdef NEED_TRANSACTION_ID
# if GIP_MODE == GIP_MODE_ASYNC
        uint32_t u32TransactionId = pGipCpu->u32TransactionId;
# else
        uint32_t u32TransactionId = pGip->aCPUs[0].u32TransactionId;
# endif
# ifdef USE_LFENCE
        ASMReadFenceSSE2();
# else
        ASMReadFence();
# endif
#endif

#if GIP_MODE == GIP_MODE_ASYNC
        u32UpdateIntervalTSC    = pGipCpu->u32UpdateIntervalTSC;
        u64NanoTS               = pGipCpu->u64NanoTS;
        u64TSC                  = pGipCpu->u64TSC;
#else
        u32UpdateIntervalTSC    = pGip->aCPUs[0].u32UpdateIntervalTSC;
        u64NanoTS               = pGip->aCPUs[0].u64NanoTS;
        u64TSC                  = pGip->aCPUs[0].u64TSC;
# if IS_GIP_MODE_WITH_DELTA(GIP_MODE)
        i64TscDelta             = pGipCpu->i64TSCDelta;
# endif
#endif
        u32NanoTSFactor0        = pGip->u32UpdateIntervalNS;
        u64Delta                = ASMReadTSC();
        u64PrevNanoTS           = ASMAtomicReadU64(pData->pu64Prev);

#ifdef NEED_TRANSACTION_ID
# if GIP_MODE == GIP_MODE_ASYNC || IS_GIP_MODE_WITH_DELTA(GIP_MODE)
        if (RT_UNLIKELY(u8ApicId != ASMGetApicId()))
            continue;
# elif defined(USE_LFENCE)
        ASMWriteFenceSSE();
# else
        ASMWriteFence();
# endif
# if GIP_MODE == GIP_MODE_ASYNC
        if (RT_UNLIKELY(   pGipCpu->u32TransactionId != u32TransactionId
                        || (u32TransactionId & 1)))
            continue;
# else
        if (RT_UNLIKELY(   pGip->aCPUs[0].u32TransactionId != u32TransactionId
                        || (u32TransactionId & 1)))
            continue;
# endif
#endif
        break;
    }

    /*
     * Calc NanoTS delta.
     */
    u64Delta -= u64TSC;
#if IS_GIP_MODE_WITH_DELTA(GIP_MODE)
    if (RT_LIKELY(i64TscDelta != INT64_MAX))
        u64Delta -= i64TscDelta;
#endif
    if (RT_UNLIKELY(u64Delta > u32UpdateIntervalTSC))
    {
        /*
         * We've expired the interval, cap it. If we're here for the 2nd
         * time without any GIP update in-between, the checks against
         * *pu64Prev below will force 1ns stepping.
         */
        pData->cExpired++;
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
     * Calculate the time and compare it with the previously returned value.
     */
    u64NanoTS += u64Delta;
    uint64_t u64DeltaPrev = u64NanoTS - u64PrevNanoTS;
    if (RT_LIKELY(   u64DeltaPrev > 0
                  && u64DeltaPrev < UINT64_C(86000000000000) /* 24h */))
        /* Frequent - less than 24h since last call. */;
    else if (RT_LIKELY(   (int64_t)u64DeltaPrev <= 0
                       && (int64_t)u64DeltaPrev + u32NanoTSFactor0 * 2 >= 0))
    {
        /* Occasional - u64NanoTS is in the recent 'past' relative the previous call. */
        ASMAtomicIncU32(&pData->c1nsSteps);
        u64NanoTS = u64PrevNanoTS + 1;
    }
    else if (!u64PrevNanoTS)
        /* We're resuming (see TMVirtualResume). */;
    else
    {
        /* Something has gone bust, if negative offset it's real bad. */
        ASMAtomicIncU32(&pData->cBadPrev);
        pData->pfnBad(pData, u64NanoTS, u64DeltaPrev, u64PrevNanoTS);
    }

    if (RT_UNLIKELY(!ASMAtomicCmpXchgU64(pData->pu64Prev, u64NanoTS, u64PrevNanoTS)))
    {
        /*
         * Attempt updating the previous value, provided we're still ahead of it.
         *
         * There is no point in recalculating u64NanoTS because we got preempted or if
         * we raced somebody while the GIP was updated, since these are events
         * that might occur at any point in the return path as well.
         */
        pData->cUpdateRaces++;
        for (int cTries = 25; cTries > 0; cTries--)
        {
            u64PrevNanoTS = ASMAtomicReadU64(pData->pu64Prev);
            if (u64PrevNanoTS >= u64NanoTS)
                break;
            if (ASMAtomicCmpXchgU64(pData->pu64Prev, u64NanoTS, u64PrevNanoTS))
                break;
        }
    }
    return u64NanoTS;
}

