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
#include <iprt/log.h>
#ifndef IN_GUEST
# include <VBox/sup.h>
# include <VBox/x86.h>
#endif
#include "internal/time.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifndef IN_GUEST
static DECLCALLBACK(void)     rtTimeNanoTSInternalBitch(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS);
static DECLCALLBACK(uint64_t) rtTimeNanoTSInternalFallback(PRTTIMENANOTSDATA pData);
static DECLCALLBACK(uint64_t) rtTimeNanoTSInternalRediscover(PRTTIMENANOTSDATA pData);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifndef IN_GUEST
/** The RTTimeNanoTS data structure that's passed down to the worker functions.  */
static RTTIMENANOTSDATA g_TimeNanoTSData =
{
    /* .u64Prev       = */ 0,
    /* .c1nsSteps     = */ 0,
    /* .cExpired      = */ 0,
    /* .cBadPrev      = */ 0,
    /* .cUpdateRaces  = */ 0,
    /* .pfnBad        = */ rtTimeNanoTSInternalBitch,
    /* .pfnRediscover = */ rtTimeNanoTSInternalRediscover
};

/** The index into g_apfnWorkers for the function to use.
 * This cannot be a pointer because that'll break down in GC due to code relocation. */
static uint32_t             g_iWorker = 0;
/** Array of rtTimeNanoTSInternal worker functions.
 * This array is indexed by g_iWorker. */
static const PFNTIMENANOTSINTERNAL g_apfnWorkers[] =
{
#define RTTIMENANO_WORKER_DETECT        0
    rtTimeNanoTSInternalRediscover,
#define RTTIMENANO_WORKER_SYNC_CPUID    1
    RTTimeNanoTSLegacySync,
#define RTTIMENANO_WORKER_ASYNC_CPUID   2
    RTTimeNanoTSLegacyAsync,
#define RTTIMENANO_WORKER_SYNC_LFENCE   3
    RTTimeNanoTSLFenceSync,
#define RTTIMENANO_WORKER_ASYNC_LFENCE  4
    RTTimeNanoTSLFenceAsync,
#define RTTIMENANO_WORKER_FALLBACK      5
    rtTimeNanoTSInternalFallback,
};


/**
 * Helper function that's used by the assembly routines when something goes bust.
 *
 * @param   pData           Pointer to the data structure.
 * @param   u64NanoTS       The calculated nano ts.
 * @param   u64DeltaPrev    The delta relative to the previously returned timestamp.
 * @param   u64PrevNanoTS   The previously returned timestamp (as it was read it).
 */
static DECLCALLBACK(void) rtTimeNanoTSInternalBitch(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS)
{
    pData->cBadPrev++;
    if ((int64_t)u64DeltaPrev < 0)
        LogRel(("TM: u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64\n",
                u64DeltaPrev, u64PrevNanoTS, u64NanoTS));
    else
        Log(("TM: u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64 (debugging?)\n",
             u64DeltaPrev, u64PrevNanoTS, u64NanoTS));
}

/**
 * Fallback function.
 */
static DECLCALLBACK(uint64_t) rtTimeNanoTSInternalFallback(PRTTIMENANOTSDATA pData)
{
    PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
    if (    pGip
        &&  pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC
        &&  (   pGip->u32Mode == SUPGIPMODE_SYNC_TSC
             || pGip->u32Mode == SUPGIPMODE_ASYNC_TSC))
        return rtTimeNanoTSInternalRediscover(pData);
    NOREF(pData);
    return RTTimeSystemNanoTS();
}


/**
 * Called the first time somebody asks for the time or when the GIP
 * is mapped/unmapped.
 */
static DECLCALLBACK(uint64_t) rtTimeNanoTSInternalRediscover(PRTTIMENANOTSDATA pData)
{
    uint32_t iWorker;
    PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
    if (    pGip
        &&  pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC
        &&  (   pGip->u32Mode == SUPGIPMODE_SYNC_TSC
             || pGip->u32Mode == SUPGIPMODE_ASYNC_TSC))
    {
        if (ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_SSE2)
            iWorker = pGip->u32Mode == SUPGIPMODE_SYNC_TSC
                    ? RTTIMENANO_WORKER_SYNC_LFENCE
                    : RTTIMENANO_WORKER_ASYNC_LFENCE;
        else
            iWorker = pGip->u32Mode == SUPGIPMODE_SYNC_TSC
                    ? RTTIMENANO_WORKER_SYNC_CPUID
                    : RTTIMENANO_WORKER_ASYNC_CPUID;
    }
    else
        iWorker = RTTIMENANO_WORKER_FALLBACK;

    ASMAtomicXchgU32((uint32_t volatile *)&g_iWorker, iWorker);
    return g_apfnWorkers[iWorker](pData);
}

#endif /* !IN_GUEST */


/**
 * Internal worker for getting the current nanosecond timestamp.
 */
DECLINLINE(uint64_t) rtTimeNanoTSInternal(void)
{
#ifndef IN_GUEST
    return g_apfnWorkers[g_iWorker](&g_TimeNanoTSData);
#else
    return RTTimeSystemNanoTS();
#endif
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
 * @returns the number of 1ns steps which has been applied by RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgSteps(void)
{
    return g_TimeNanoTSData.c1nsSteps;
}


/**
 * Debugging the time api.
 *
 * @returns the number of times the TSC interval expired RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgExpired(void)
{
    return g_TimeNanoTSData.cExpired;
}


/**
 * Debugging the time api.
 *
 * @returns the number of bad previous values encountered by RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgBad(void)
{
    return g_TimeNanoTSData.cBadPrev;
}


/**
 * Debugging the time api.
 *
 * @returns the number of update races in RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgRaces(void)
{
    return g_TimeNanoTSData.cUpdateRaces;
}
#endif
