/* $Id$ */
/** @file
 * TM - Timeout Manager, Virtual Time, All Contexts.
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
#ifdef IN_RING3
# include <VBox/rem.h>
# include <iprt/thread.h>
#endif
#include "TMInternal.h"
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/sup.h>

#include <iprt/time.h>
#include <iprt/assert.h>
#include <iprt/asm.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) tmVirtualSetWarpDrive(PVM pVM, uint32_t u32Percent);


/**
 * Helper function that's used by the assembly routines when something goes bust.
 *
 * @param   pData           Pointer to the data structure.
 * @param   u64NanoTS       The calculated nano ts.
 * @param   u64DeltaPrev    The delta relative to the previously returned timestamp.
 * @param   u64PrevNanoTS   The previously returned timestamp (as it was read it).
 */
DECLEXPORT(void) tmVirtualNanoTSBad(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS)
{
    //PVM pVM = (PVM)((uint8_t *)pData - RT_OFFSETOF(VM, CTXALLSUFF(s.tm.VirtualGetRawData)));
    pData->cBadPrev++;
    if ((int64_t)u64DeltaPrev < 0)
        LogRel(("TM: u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64\n",
                u64DeltaPrev, u64PrevNanoTS, u64NanoTS));
    else
        Log(("TM: u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64 (debugging?)\n",
             u64DeltaPrev, u64PrevNanoTS, u64NanoTS));
}


/**
 * Called the first time somebody asks for the time or when the GIP
 * is mapped/unmapped.
 *
 * This should never ever happen.
 */
DECLEXPORT(uint64_t) tmVirtualNanoTSRediscover(PRTTIMENANOTSDATA pData)
{
    //PVM pVM = (PVM)((uint8_t *)pData - RT_OFFSETOF(VM, CTXALLSUFF(s.tm.VirtualGetRawData)));
    PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
    AssertFatalMsgFailed(("pGip=%p u32Magic=%#x\n", pGip, VALID_PTR(pGip) ? pGip->u32Magic : 0));
}


#if 1

/**
 * Wrapper around the IPRT GIP time methods.
 */
DECLINLINE(uint64_t) tmVirtualGetRawNanoTS(PVM pVM)
{
#ifdef IN_RING3
    return CTXALLSUFF(pVM->tm.s.pfnVirtualGetRaw)(&CTXALLSUFF(pVM->tm.s.VirtualGetRawData));
# else  /* !IN_RING3 */
    uint32_t cPrevSteps = pVM->tm.s.CTX_SUFF(VirtualGetRawData).c1nsSteps;
    uint64_t u64 = pVM->tm.s.CTX_SUFF(pfnVirtualGetRaw)(&pVM->tm.s.CTX_SUFF(VirtualGetRawData));
    if (cPrevSteps != pVM->tm.s.CTX_SUFF(VirtualGetRawData).c1nsSteps)
        VMCPU_FF_SET(VMMGetCpu(pVM), VMCPU_FF_TO_R3);
    return u64;
# endif /* !IN_RING3 */
}

#else

/**
 * This is (mostly) the same as rtTimeNanoTSInternal() except
 * for the two globals which live in TM.
 *
 * @returns Nanosecond timestamp.
 * @param   pVM     The VM handle.
 */
static uint64_t tmVirtualGetRawNanoTS(PVM pVM)
{
    uint64_t    u64Delta;
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
        uint32_t u32TransactionId;
        PSUPGLOBALINFOPAGE pGip = g_pSUPGlobalInfoPage;
#ifdef IN_RING3
        if (RT_UNLIKELY(!pGip || pGip->u32Magic != SUPGLOBALINFOPAGE_MAGIC))
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
            u64PrevNanoTS = ASMAtomicReadU64(&pVM->tm.s.u64VirtualRawPrev);
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
            u64PrevNanoTS = ASMAtomicReadU64(&pVM->tm.s.u64VirtualRawPrev);
#ifdef IN_RC
            Assert(!(ASMGetFlags() & X86_EFL_IF));
#else
            if (RT_UNLIKELY(u8ApicId != ASMGetApicId()))
                continue;
            if (RT_UNLIKELY(    pGipCpu->u32TransactionId != u32TransactionId
                            ||  (u32TransactionId & 1)))
                continue;
#endif
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
         * We've expired the interval, cap it. If we're here for the 2nd
         * time without any GIP update inbetween, the checks against
         * pVM->tm.s.u64VirtualRawPrev below will force 1ns stepping.
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
     * Calculate the time and compare it with the previously returned value.
     *
     * Since this function is called *very* frequently when the VM is running
     * and then mostly on EMT, we can restrict the valid range of the delta
     * (-1s to 2*GipUpdates) and simplify/optimize the default path.
     */
    u64NanoTS += u64Delta;
    uint64_t u64DeltaPrev = u64NanoTS - u64PrevNanoTS;
    if (RT_LIKELY(u64DeltaPrev < 1000000000 /* 1s */))
        /* frequent - less than 1s since last call. */;
    else if (   (int64_t)u64DeltaPrev < 0
             && (int64_t)u64DeltaPrev + u32NanoTSFactor0 * 2 > 0)
    {
        /* occasional - u64NanoTS is in the 'past' relative to previous returns. */
        ASMAtomicIncU32(&pVM->tm.s.CTX_SUFF(VirtualGetRawData).c1nsSteps);
        u64NanoTS = u64PrevNanoTS + 1;
#ifndef IN_RING3
        VM_FF_SET(pVM, VM_FF_TO_R3); /* S10 hack */
#endif
    }
    else if (u64PrevNanoTS)
    {
        /* Something has gone bust, if negative offset it's real bad. */
        ASMAtomicIncU32(&pVM->tm.s.CTX_SUFF(VirtualGetRawData).cBadPrev);
        if ((int64_t)u64DeltaPrev < 0)
            LogRel(("TM: u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64 u64Delta=%#RX64\n",
                    u64DeltaPrev, u64PrevNanoTS, u64NanoTS, u64Delta));
        else
            Log(("TM: u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64 u64Delta=%#RX64 (debugging?)\n",
                 u64DeltaPrev, u64PrevNanoTS, u64NanoTS, u64Delta));
#ifdef DEBUG_bird
        /** @todo there are some hickups during boot and reset that can cause 2-5 seconds delays. Investigate... */
        AssertMsg(u64PrevNanoTS > UINT64_C(100000000000) /* 100s */,
                  ("u64DeltaPrev=%RI64 u64PrevNanoTS=0x%016RX64 u64NanoTS=0x%016RX64 u64Delta=%#RX64\n",
                  u64DeltaPrev, u64PrevNanoTS, u64NanoTS, u64Delta));
#endif
    }
    /* else: We're resuming (see TMVirtualResume). */
    if (RT_LIKELY(ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualRawPrev, u64NanoTS, u64PrevNanoTS)))
        return u64NanoTS;

    /*
     * Attempt updating the previous value, provided we're still ahead of it.
     *
     * There is no point in recalculating u64NanoTS because we got preemted or if
     * we raced somebody while the GIP was updated, since these are events
     * that might occure at any point in the return path as well.
     */
    for (int cTries = 50;;)
    {
        u64PrevNanoTS = ASMAtomicReadU64(&pVM->tm.s.u64VirtualRawPrev);
        if (u64PrevNanoTS >= u64NanoTS)
            break;
        if (ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualRawPrev, u64NanoTS, u64PrevNanoTS))
            break;
        AssertBreak(--cTries <= 0);
        if (cTries < 25 && !VM_IS_EMT(pVM)) /* give up early */
            break;
    }

    return u64NanoTS;
}

#endif


/**
 * Get the time when we're not running at 100%
 *
 * @returns The timestamp.
 * @param   pVM     The VM handle.
 */
static uint64_t tmVirtualGetRawNonNormal(PVM pVM)
{
    /*
     * Recalculate the RTTimeNanoTS() value for the period where
     * warp drive has been enabled.
     */
    uint64_t u64 = tmVirtualGetRawNanoTS(pVM);
    u64 -= pVM->tm.s.u64VirtualWarpDriveStart;
    u64 *= pVM->tm.s.u32VirtualWarpDrivePercentage;
    u64 /= 100;
    u64 += pVM->tm.s.u64VirtualWarpDriveStart;

    /*
     * Now we apply the virtual time offset.
     * (Which is the negated tmVirtualGetRawNanoTS() value for when the virtual
     * machine started if it had been running continuously without any suspends.)
     */
    u64 -= pVM->tm.s.u64VirtualOffset;
    return u64;
}


/**
 * Get the raw virtual time.
 *
 * @returns The current time stamp.
 * @param   pVM     The VM handle.
 */
DECLINLINE(uint64_t) tmVirtualGetRaw(PVM pVM)
{
    if (RT_LIKELY(!pVM->tm.s.fVirtualWarpDrive))
        return tmVirtualGetRawNanoTS(pVM) - pVM->tm.s.u64VirtualOffset;
    return tmVirtualGetRawNonNormal(pVM);
}


/**
 * Inlined version of tmVirtualGetEx.
 */
DECLINLINE(uint64_t) tmVirtualGet(PVM pVM, bool fCheckTimers)
{
    uint64_t u64;
    if (RT_LIKELY(pVM->tm.s.cVirtualTicking))
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGet);
        u64 = tmVirtualGetRaw(pVM);

        /*
         * Use the chance to check for expired timers.
         */
        if (    fCheckTimers
            &&  !VM_FF_ISSET(pVM, VM_FF_TIMER)
            &&  (   pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64
                 || (   pVM->tm.s.fVirtualSyncTicking
                     && pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire <= u64 - pVM->tm.s.offVirtualSync
                    )
                )
           )
        {
            VM_FF_SET(pVM, VM_FF_TIMER);
            STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSetFF);
#ifdef IN_RING3
            REMR3NotifyTimerPending(pVM);
            VMR3NotifyGlobalFFU(pVM->pUVM, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
        }
    }
    else
        u64 = pVM->tm.s.u64Virtual;
    return u64;
}


/**
 * Gets the current TMCLOCK_VIRTUAL time
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 *
 * @remark  While the flow of time will never go backwards, the speed of the
 *          progress varies due to inaccurate RTTimeNanoTS and TSC. The latter can be
 *          influenced by power saving (SpeedStep, PowerNow!), while the former
 *          makes use of TSC and kernel timers.
 */
VMMDECL(uint64_t) TMVirtualGet(PVM pVM)
{
    return TMVirtualGetEx(pVM, true /* check timers */);
}


/**
 * Gets the current TMCLOCK_VIRTUAL time
 *
 * @returns The timestamp.
 * @param   pVM             VM handle.
 * @param   fCheckTimers    Check timers or not
 *
 * @remark  While the flow of time will never go backwards, the speed of the
 *          progress varies due to inaccurate RTTimeNanoTS and TSC. The latter can be
 *          influenced by power saving (SpeedStep, PowerNow!), while the former
 *          makes use of TSC and kernel timers.
 */
VMMDECL(uint64_t) TMVirtualGetEx(PVM pVM, bool fCheckTimers)
{
    return tmVirtualGet(pVM, fCheckTimers);
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 * @param   fCheckTimers    Check timers or not
 * @thread  EMT.
 */
VMMDECL(uint64_t) TMVirtualSyncGetEx(PVM pVM, bool fCheckTimers)
{
    VM_ASSERT_EMT(pVM);

    uint64_t u64;
    if (pVM->tm.s.fVirtualSyncTicking)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSync);

        /*
         * Query the virtual clock and do the usual expired timer check.
         */
        Assert(pVM->tm.s.cVirtualTicking);
        u64 = tmVirtualGetRaw(pVM);
        if (    fCheckTimers
            &&  !VM_FF_ISSET(pVM, VM_FF_TIMER)
            &&  pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64)
        {
            VM_FF_SET(pVM, VM_FF_TIMER);
#ifdef IN_RING3
            REMR3NotifyTimerPending(pVM);
            VMR3NotifyGlobalFFU(pVM->pUVM, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
            STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSyncSetFF);
        }

        /*
         * Read the offset and adjust if we're playing catch-up.
         *
         * The catch-up adjusting work by us decrementing the offset by a percentage of
         * the time elapsed since the previous TMVirtualGetSync call.
         *
         * It's possible to get a very long or even negative interval between two read
         * for the following reasons:
         *  - Someone might have suspended the process execution, frequently the case when
         *    debugging the process.
         *  - We might be on a different CPU which TSC isn't quite in sync with the
         *    other CPUs in the system.
         *  - Another thread is racing us and we might have been preemnted while inside
         *    this function.
         *
         * Assuming nano second virtual time, we can simply ignore any intervals which has
         * any of the upper 32 bits set.
         */
        AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
        uint64_t off = pVM->tm.s.offVirtualSync;
        if (pVM->tm.s.fVirtualSyncCatchUp)
        {
            const uint64_t u64Prev = pVM->tm.s.u64VirtualSyncCatchUpPrev;
            uint64_t u64Delta = u64 - u64Prev;
            if (RT_LIKELY(!(u64Delta >> 32)))
            {
                uint64_t u64Sub = ASMMultU64ByU32DivByU32(u64Delta, pVM->tm.s.u32VirtualSyncCatchUpPercentage, 100);
                if (off > u64Sub + pVM->tm.s.offVirtualSyncGivenUp)
                {
                    off -= u64Sub;
                    ASMAtomicXchgU64(&pVM->tm.s.offVirtualSync, off);
                    pVM->tm.s.u64VirtualSyncCatchUpPrev = u64;
                    Log4(("TM: %RU64/%RU64: sub %RU32\n", u64 - off, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp, u64Sub));
                }
                else
                {
                    /* we've completely caught up. */
                    STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);
                    off = pVM->tm.s.offVirtualSyncGivenUp;
                    ASMAtomicXchgU64(&pVM->tm.s.offVirtualSync, off);
                    ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncCatchUp, false);
                    pVM->tm.s.u64VirtualSyncCatchUpPrev = u64;
                    Log4(("TM: %RU64/0: caught up\n", u64));
                }
            }
            else
            {
                /* More than 4 seconds since last time (or negative), ignore it. */
                if (!(u64Delta & RT_BIT_64(63)))
                    pVM->tm.s.u64VirtualSyncCatchUpPrev = u64;
                Log(("TMVirtualGetSync: u64Delta=%RX64\n", u64Delta));
            }
        }

        /*
         * Complete the calculation of the current TMCLOCK_VIRTUAL_SYNC time. The current
         * approach is to never pass the head timer. So, when we do stop the clock and
         * set the timer pending flag.
         */
        u64 -= off;
        const uint64_t u64Expire = pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire;
        if (u64 >= u64Expire)
        {
            u64 = u64Expire;
            ASMAtomicXchgU64(&pVM->tm.s.u64VirtualSync, u64);
            ASMAtomicXchgBool(&pVM->tm.s.fVirtualSyncTicking, false);
            if (    fCheckTimers
                &&  !VM_FF_ISSET(pVM, VM_FF_TIMER))
            {
                VM_FF_SET(pVM, VM_FF_TIMER);
#ifdef IN_RING3
                REMR3NotifyTimerPending(pVM);
                VMR3NotifyGlobalFFU(pVM->pUVM, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
                STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSyncSetFF);
                Log4(("TM: %RU64/%RU64: exp tmr=>ff\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
            }
            else
                Log4(("TM: %RU64/%RU64: exp tmr\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
        }
    }
    else
    {
        u64 = pVM->tm.s.u64VirtualSync;

        /*
         * If it looks like a halt caused by pending timers, make sure the FF is raised.
         * This is a safeguard against timer queue runner leaving the virtual sync clock stopped.
         */
        if (    fCheckTimers
            &&  pVM->tm.s.cVirtualTicking
            &&  !VM_FF_ISSET(pVM, VM_FF_TIMER))
        {
            const uint64_t u64Expire = pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire;
            if (u64 >= u64Expire)
            {
                VM_FF_SET(pVM, VM_FF_TIMER);
#ifdef IN_RING3
                REMR3NotifyTimerPending(pVM);
                VMR3NotifyGlobalFFU(pVM->pUVM, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
                STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSyncSetFF);
                Log4(("TM: %RU64/%RU64: exp tmr=>ff (!)\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
            }
        }
    }
    return u64;
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM             VM handle.
 * @thread  EMT.
 */
VMMDECL(uint64_t) TMVirtualSyncGet(PVM pVM)
{
    return TMVirtualSyncGetEx(pVM, true /* check timers */);
}


/**
 * Gets the current lag of the synchronous virtual clock (relative to the virtual clock).
 *
 * @return  The current lag.
 * @param   pVM     VM handle.
 */
VMMDECL(uint64_t) TMVirtualSyncGetLag(PVM pVM)
{
    return pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp;
}


/**
 * Get the current catch-up percent.
 *
 * @return  The current catch0up percent. 0 means running at the same speed as the virtual clock.
 * @param   pVM     VM handle.
 */
VMMDECL(uint32_t) TMVirtualSyncGetCatchUpPct(PVM pVM)
{
    if (pVM->tm.s.fVirtualSyncCatchUp)
        return pVM->tm.s.u32VirtualSyncCatchUpPercentage;
    return 0;
}


/**
 * Gets the current TMCLOCK_VIRTUAL frequency.
 *
 * @returns The freqency.
 * @param   pVM     VM handle.
 */
VMMDECL(uint64_t) TMVirtualGetFreq(PVM pVM)
{
    return TMCLOCK_FREQ_VIRTUAL;
}


/**
 * Resumes the virtual clock.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_INTERNAL_ERROR and VBOX_STRICT assertion if called out of order.
 * @param   pVM     VM handle.
 */
VMMDECL(int) TMVirtualResume(PVM pVM)
{
    /** @note this is done only in specific cases (vcpu 0 init, termination, debug, out of memory conditions;
     *  there is at least a race for fVirtualSyncTicking.
     */
    if (ASMAtomicIncU32(&pVM->tm.s.cVirtualTicking) == 1)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualResume);
        pVM->tm.s.u64VirtualRawPrev         = 0;
        pVM->tm.s.u64VirtualWarpDriveStart  = tmVirtualGetRawNanoTS(pVM);
        pVM->tm.s.u64VirtualOffset          = pVM->tm.s.u64VirtualWarpDriveStart - pVM->tm.s.u64Virtual;
        pVM->tm.s.fVirtualSyncTicking       = true;
        return VINF_SUCCESS;
    }
    AssertReturn(pVM->tm.s.cVirtualTicking < pVM->cCPUs, VERR_INTERNAL_ERROR);
    return VINF_SUCCESS;
}


/**
 * Pauses the virtual clock.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_INTERNAL_ERROR and VBOX_STRICT assertion if called out of order.
 * @param   pVM     VM handle.
 */
VMMDECL(int) TMVirtualPause(PVM pVM)
{
    /** @note this is done only in specific cases (vcpu 0 init, termination, debug, out of memory conditions;
     *  there is at least a race for fVirtualSyncTicking.
     */
    if (ASMAtomicDecU32(&pVM->tm.s.cVirtualTicking) == 0)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualPause);
        pVM->tm.s.u64Virtual            = tmVirtualGetRaw(pVM);
        pVM->tm.s.fVirtualSyncTicking   = false;
        return VINF_SUCCESS;
    }
    AssertReturn(pVM->tm.s.cVirtualTicking < pVM->cCPUs, VERR_INTERNAL_ERROR);
    return VINF_SUCCESS;
}


/**
 * Gets the current warp drive percent.
 *
 * @returns The warp drive percent.
 * @param   pVM         The VM handle.
 */
VMMDECL(uint32_t) TMVirtualGetWarpDrive(PVM pVM)
{
    return pVM->tm.s.u32VirtualWarpDrivePercentage;
}


/**
 * Sets the warp drive percent of the virtual time.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   u32Percent  The new percentage. 100 means normal operation.
 */
VMMDECL(int) TMVirtualSetWarpDrive(PVM pVM, uint32_t u32Percent)
{
/** @todo This isn't a feature specific to virtual time, move to TM level. (It
 * should affect the TMR3UCTNow as well! */
#ifdef IN_RING3
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)tmVirtualSetWarpDrive, 2, pVM, u32Percent);
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);
    return rc;
#else

    return tmVirtualSetWarpDrive(pVM, u32Percent);
#endif
}


/**
 * EMT worker for tmVirtualSetWarpDrive.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   u32Percent  See TMVirtualSetWarpDrive().
 * @internal
 */
static DECLCALLBACK(int) tmVirtualSetWarpDrive(PVM pVM, uint32_t u32Percent)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /*
     * Validate it.
     */
    AssertMsgReturn(u32Percent >= 2 && u32Percent <= 20000,
                    ("%RX32 is not between 2 and 20000 (inclusive).\n", u32Percent),
                    VERR_INVALID_PARAMETER);

    /*
     * If the time is running we'll have to pause it before we can change
     * the warp drive settings.
     */
    bool fPaused = !!pVM->tm.s.cVirtualTicking;
    if (fPaused)
    {
        int rc = TMVirtualPause(pVM);
        AssertRCReturn(rc, rc);
        rc = TMCpuTickPause(pVCpu);
        AssertRCReturn(rc, rc);
    }

    pVM->tm.s.u32VirtualWarpDrivePercentage = u32Percent;
    pVM->tm.s.fVirtualWarpDrive = u32Percent != 100;
    LogRel(("TM: u32VirtualWarpDrivePercentage=%RI32 fVirtualWarpDrive=%RTbool\n",
            pVM->tm.s.u32VirtualWarpDrivePercentage, pVM->tm.s.fVirtualWarpDrive));

    if (fPaused)
    {
        int rc = TMVirtualResume(pVM);
        AssertRCReturn(rc, rc);
        rc = TMCpuTickResume(pVCpu);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}


/**
 * Converts from virtual ticks to nanoseconds.
 *
 * @returns nanoseconds.
 * @param   pVM             The VM handle.
 * @param   u64VirtualTicks The virtual ticks to convert.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
VMMDECL(uint64_t) TMVirtualToNano(PVM pVM, uint64_t u64VirtualTicks)
{
    AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64VirtualTicks;
}


/**
 * Converts from virtual ticks to microseconds.
 *
 * @returns microseconds.
 * @param   pVM             The VM handle.
 * @param   u64VirtualTicks The virtual ticks to convert.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
VMMDECL(uint64_t) TMVirtualToMicro(PVM pVM, uint64_t u64VirtualTicks)
{
    AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64VirtualTicks / 1000;
}


/**
 * Converts from virtual ticks to milliseconds.
 *
 * @returns milliseconds.
 * @param   pVM             The VM handle.
 * @param   u64VirtualTicks The virtual ticks to convert.
 * @remark  There could be rounding errors here. We just do a simple integere divide
 *          without any adjustments.
 */
VMMDECL(uint64_t) TMVirtualToMilli(PVM pVM, uint64_t u64VirtualTicks)
{
        AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64VirtualTicks / 1000000;
}


/**
 * Converts from nanoseconds to virtual ticks.
 *
 * @returns virtual ticks.
 * @param   pVM             The VM handle.
 * @param   u64NanoTS       The nanosecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
VMMDECL(uint64_t) TMVirtualFromNano(PVM pVM, uint64_t u64NanoTS)
{
    AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64NanoTS;
}


/**
 * Converts from microseconds to virtual ticks.
 *
 * @returns virtual ticks.
 * @param   pVM             The VM handle.
 * @param   u64MicroTS      The microsecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
VMMDECL(uint64_t) TMVirtualFromMicro(PVM pVM, uint64_t u64MicroTS)
{
    AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64MicroTS * 1000;
}


/**
 * Converts from milliseconds to virtual ticks.
 *
 * @returns virtual ticks.
 * @param   pVM             The VM handle.
 * @param   u64MilliTS      The millisecond value ticks to convert.
 * @remark  There could be rounding and overflow errors here.
 */
VMMDECL(uint64_t) TMVirtualFromMilli(PVM pVM, uint64_t u64MilliTS)
{
    AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64MilliTS * 1000000;
}

