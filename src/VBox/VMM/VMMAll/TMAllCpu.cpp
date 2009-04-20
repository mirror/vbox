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
#include "../TMInternal.h"
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
 * @param   pVCpu       The VMCPU to operate on.
 * @internal
 */
int tmCpuTickResume(PVM pVM, PVMCPU pVCpu)
{
    if (!pVCpu->tm.s.fTSCTicking)
    {
        pVCpu->tm.s.fTSCTicking = true;
        if (pVM->tm.s.fTSCVirtualized)
        {
            if (pVM->tm.s.fTSCUseRealTSC)
                pVCpu->tm.s.u64TSCOffset = ASMReadTSC() - pVCpu->tm.s.u64TSC;
            else
                pVCpu->tm.s.u64TSCOffset = tmCpuTickGetRawVirtual(pVM, false /* don't check for pending timers */)
                                         - pVCpu->tm.s.u64TSC;
        }
        return VINF_SUCCESS;
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Resumes the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVCpu       The VMCPU to operate on.
 * @todo replace this with TMNotifyResume
 */
VMMDECL(int) TMCpuTickResume(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    if (!pVM->tm.s.fTSCTiedToExecution)
        return tmCpuTickResume(pVM, pVCpu);
    /* ignored */
    return VINF_SUCCESS;
}


/**
 * Pauses the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @internal
 */
int tmCpuTickPause(PVM pVM, PVMCPU pVCpu)
{
    if (pVCpu->tm.s.fTSCTicking)
    {
        pVCpu->tm.s.u64TSC = TMCpuTickGet(pVCpu);
        pVCpu->tm.s.fTSCTicking = false;
        return VINF_SUCCESS;
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Pauses the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVCpu       The VMCPU to operate on.
 * @todo replace this with TMNotifySuspend
 */
VMMDECL(int) TMCpuTickPause(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    if (!pVM->tm.s.fTSCTiedToExecution)
        return tmCpuTickPause(pVM, pVCpu);
    /* ignored */
    return VINF_SUCCESS;
}


/**
 * Checks if AMD-V / VT-x can use an offsetted hardware TSC or not.
 *
 * @returns true/false accordingly.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   poffRealTSC     The offset against the TSC of the current CPU.
 *                          Can be NULL.
 * @thread EMT.
 */
VMMDECL(bool) TMCpuTickCanUseRealTSC(PVMCPU pVCpu, uint64_t *poffRealTSC)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * We require:
     *     1. A fixed TSC, this is checked at init time.
     *     2. That the TSC is ticking (we shouldn't be here if it isn't)
     *     3. Either that we're using the real TSC as time source or
     *          a) we don't have any lag to catch up, and
     *          b) the virtual sync clock hasn't been halted by an expired timer, and
     *          c) we're not using warp drive (accelerated virtual guest time).
     */
    if (    pVM->tm.s.fMaybeUseOffsettedHostTSC
        &&  RT_LIKELY(pVCpu->tm.s.fTSCTicking)
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
                                - pVCpu->tm.s.u64TSCOffset;
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
                *poffRealTSC = pVCpu->tm.s.u64TSCOffset;
            else
                *poffRealTSC = 0;
        }
        /** @todo count this? */
        return true;
    }

#ifdef VBOX_WITH_STATISTICS
    /* Sample the reason for refusing. */
    if (!pVM->tm.s.fMaybeUseOffsettedHostTSC)
       STAM_COUNTER_INC(&pVM->tm.s.StatTSCNotFixed);
    else if (!pVCpu->tm.s.fTSCTicking)
       STAM_COUNTER_INC(&pVM->tm.s.StatTSCNotTicking);
    else if (!pVM->tm.s.fTSCUseRealTSC)
    {
        if (pVM->tm.s.fVirtualSyncCatchUp)
        {
           if (pVM->tm.s.u32VirtualSyncCatchUpPercentage <= 10)
               STAM_COUNTER_INC(&pVM->tm.s.StatTSCCatchupLE010);
           else if (pVM->tm.s.u32VirtualSyncCatchUpPercentage <= 25)
               STAM_COUNTER_INC(&pVM->tm.s.StatTSCCatchupLE025);
           else if (pVM->tm.s.u32VirtualSyncCatchUpPercentage <= 100)
               STAM_COUNTER_INC(&pVM->tm.s.StatTSCCatchupLE100);
           else
               STAM_COUNTER_INC(&pVM->tm.s.StatTSCCatchupOther);
        }
        else if (!pVM->tm.s.fVirtualSyncTicking)
           STAM_COUNTER_INC(&pVM->tm.s.StatTSCSyncNotTicking);
        else if (pVM->tm.s.fVirtualWarpDrive)
           STAM_COUNTER_INC(&pVM->tm.s.StatTSCWarp);
    }
#endif
    return false;
}


/**
 * Read the current CPU timstamp counter.
 *
 * @returns Gets the CPU tsc.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMDECL(uint64_t) TMCpuTickGet(PVMCPU pVCpu)
{
    PVM      pVM = pVCpu->CTX_SUFF(pVM);
    uint64_t u64;

    if (RT_LIKELY(pVCpu->tm.s.fTSCTicking))
    {
        if (pVM->tm.s.fTSCVirtualized)
        {
            if (pVM->tm.s.fTSCUseRealTSC)
                u64 = ASMReadTSC();
            else
                u64 = tmCpuTickGetRawVirtual(pVM, true /* check for pending timers */);
            u64 -= pVCpu->tm.s.u64TSCOffset;
        }
        else
            u64 = ASMReadTSC();
    }
    else
        u64 = pVCpu->tm.s.u64TSC;
    return u64;
}


/**
 * Sets the current CPU timestamp counter.
 *
 * @returns VBox status code.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   u64Tick     The new timestamp value.
 */
VMMDECL(int) TMCpuTickSet(PVMCPU pVCpu, uint64_t u64Tick)
{
    Assert(!pVCpu->tm.s.fTSCTicking);
    pVCpu->tm.s.u64TSC = u64Tick;
    return VINF_SUCCESS;
}


/**
 * Get the timestamp frequency.
 *
 * @returns Number of ticks per second.
 * @param   pVM     The VM.
 */
VMMDECL(uint64_t) TMCpuTicksPerSecond(PVM pVM)
{
    if (pVM->tm.s.fTSCUseRealTSC)
    {
        uint64_t cTSCTicksPerSecond = SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage);
        if (RT_LIKELY(cTSCTicksPerSecond != ~(uint64_t)0))
            return cTSCTicksPerSecond;
    }
    return pVM->tm.s.cTSCTicksPerSecond;
}

