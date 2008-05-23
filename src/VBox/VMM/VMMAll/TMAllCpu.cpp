/* $Id$ */
/** @file
 * TM - Timeout Manager, CPU Time, All Contexts.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_TM
#include <VBox/tm.h>
#include "TMInternal.h"
#include <VBox/vm.h>
#include <VBox/sup.h>

#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <VBox/log.h>


/**
 * Gets the raw cpu tick from current virtual time.
 */
DECLINLINE(uint64_t) tmCpuTickGetRawVirtual(PVM pVM, bool fCheckTimers)
{
    uint64_t u64 = TMVirtualSyncGetEx(pVM, fCheckTimers);
    if (u64 != TMCLOCK_FREQ_VIRTUAL)
        u64 = ASMMultU64ByU32DivByU32(u64, pVM->tm.s.cTSCTicksPerSecond, TMCLOCK_FREQ_VIRTUAL);
    return u64;
}


/**
 * Resumes the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMDECL(int) TMCpuTickResume(PVM pVM)
{
    if (!pVM->tm.s.fTSCTicking)
    {
        pVM->tm.s.fTSCTicking = true;
        if (pVM->tm.s.fTSCVirtualized)
        {
            if (pVM->tm.s.fTSCUseRealTSC)
                pVM->tm.s.u64TSCOffset = ASMReadTSC() - pVM->tm.s.u64TSC;
            else
                pVM->tm.s.u64TSCOffset = tmCpuTickGetRawVirtual(pVM, false /* don't check for pending timers */)
                                       - pVM->tm.s.u64TSC;
        }
        return VINF_SUCCESS;
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Pauses the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMDECL(int) TMCpuTickPause(PVM pVM)
{
    if (pVM->tm.s.fTSCTicking)
    {
        pVM->tm.s.u64TSC = TMCpuTickGet(pVM);
        pVM->tm.s.fTSCTicking = false;
        return VINF_SUCCESS;
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Checks if AMD-V / VT-x can use an offsetted hardware TSC or not.
 *
 * @returns true/false accordingly.
 * @param   pVM             The VM handle.
 * @param   poffRealTSC     The offset against the TSC of the current CPU.
 *                          Can be NULL.
 * @thread EMT.
 */
TMDECL(bool) TMCpuTickCanUseRealTSC(PVM pVM, uint64_t *poffRealTSC)
{
    /*
     * We require:
     *     1. A fixed TSC, this is checked at init time.
     *     2. That the TSC is ticking (we shouldn't be here if it isn't)
     *     3. Either that we're using the real TSC as time source or
     *          a) We don't have any lag to catch up.
     *          b) The virtual sync clock hasn't been halted by an expired timer.
     *          c) We're not using warp drive (accelerated virtual guest time).
     */
    if (    pVM->tm.s.fMaybeUseOffsettedHostTSC
        &&  RT_LIKELY(pVM->tm.s.fTSCTicking)
        &&  (   pVM->tm.s.fTSCUseRealTSC
             || (   !pVM->tm.s.fVirtualSyncCatchUp
                 && RT_LIKELY(pVM->tm.s.fVirtualSyncTicking)
                 && !pVM->tm.s.fVirtualWarpDrive))
       )
    {
        if (!pVM->tm.s.fTSCUseRealTSC)
        {
            /* The source is the timer synchronous virtual clock. */
            Assert(pVM->tm.s.fTSCVirtualized);

            if (poffRealTSC)
            {
                uint64_t u64Now = tmCpuTickGetRawVirtual(pVM, false /* don't check for pending timers */)
                                - pVM->tm.s.u64TSCOffset;
                /** @todo When we start collecting statistics on how much time we spend executing
                 * guest code before exiting, we should check this against the next virtual sync
                 * timer timeout. If it's lower than the avg. length, we should trap rdtsc to increase
                 * the chance that we'll get interrupted right after the timer expired. */
                *poffRealTSC = u64Now - ASMReadTSC();
            }
        }
        else if (poffRealTSC)
        {
            /* The source is the real TSC. */
            if (pVM->tm.s.fTSCVirtualized)
                *poffRealTSC = pVM->tm.s.u64TSCOffset;
            else
                *poffRealTSC = 0;
        }
        return true;
    }

    return false;
}


/**
 * Read the current CPU timstamp counter.
 *
 * @returns Gets the CPU tsc.
 * @param   pVM         The VM to operate on.
 */
TMDECL(uint64_t) TMCpuTickGet(PVM pVM)
{
    uint64_t u64;
    if (RT_LIKELY(pVM->tm.s.fTSCTicking))
    {
        if (pVM->tm.s.fTSCVirtualized)
        {
            if (pVM->tm.s.fTSCUseRealTSC)
                u64 = ASMReadTSC();
            else
                u64 = tmCpuTickGetRawVirtual(pVM, true /* check for pending timers */);
            u64 -= pVM->tm.s.u64TSCOffset;
        }
        else
            u64 = ASMReadTSC();
    }
    else
        u64 = pVM->tm.s.u64TSC;
    return u64;
}


/**
 * Sets the current CPU timestamp counter.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   u64Tick     The new timestamp value.
 */
TMDECL(int) TMCpuTickSet(PVM pVM, uint64_t u64Tick)
{
    Assert(!pVM->tm.s.fTSCTicking);
    pVM->tm.s.u64TSC = u64Tick;
    return VINF_SUCCESS;
}


/**
 * Get the timestamp frequency.
 *
 * @returns Number of ticks per second.
 * @param   pVM     The VM.
 */
TMDECL(uint64_t) TMCpuTicksPerSecond(PVM pVM)
{
    if (pVM->tm.s.fTSCUseRealTSC)
    {
        uint64_t cTSCTicksPerSecond = SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage);
        if (RT_LIKELY(cTSCTicksPerSecond != ~(uint64_t)0))
            return cTSCTicksPerSecond;
    }
    return pVM->tm.s.cTSCTicksPerSecond;
}

