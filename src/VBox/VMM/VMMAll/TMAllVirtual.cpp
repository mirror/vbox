/** @file
 *
 * TM - Timeout Manager, Virtual Time, All Contexts.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_TM
#include <VBox/tm.h>
#ifdef IN_RING3
# include <VBox/rem.h>
#endif
#include "TMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/sup.h>

#include <iprt/time.h>
#include <iprt/assert.h>
#include <iprt/asm.h>




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
TMDECL(uint64_t) TMVirtualGet(PVM pVM)
{
    uint64_t u64;
    if (pVM->tm.s.fVirtualTicking)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGet);
        u64 = RTTimeNanoTS() - pVM->tm.s.u64VirtualOffset;

        /*
         * Use the chance to check for expired timers.
         */
        if (    !VM_FF_ISSET(pVM, VM_FF_TIMER)
            &&  (   pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64
                 || (   pVM->tm.s.fVirtualSyncTicking
                     && pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire <= u64 - pVM->tm.s.u64VirtualSyncOffset
                    )
                )
           )
        {
            VM_FF_SET(pVM, VM_FF_TIMER);
#ifdef IN_RING3
            REMR3NotifyTimerPending(pVM);
            VMR3NotifyFF(pVM, true);
#endif
        }
    }
    else
        u64 = pVM->tm.s.u64Virtual;
    return u64;
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 */
TMDECL(uint64_t) TMVirtualGetSync(PVM pVM)
{
    uint64_t u64;
    if (pVM->tm.s.fVirtualSyncTicking)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSync);

        /*
         * Do TMVirtualGet() to get the current TMCLOCK_VIRTUAL time.
         */
        Assert(pVM->tm.s.fVirtualTicking);
        u64 = RTTimeNanoTS() - pVM->tm.s.u64VirtualOffset;
        if (    !VM_FF_ISSET(pVM, VM_FF_TIMER)
            &&  pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64)
        {
            VM_FF_SET(pVM, VM_FF_TIMER);
#ifdef IN_RING3
            REMR3NotifyTimerPending(pVM);
            VMR3NotifyFF(pVM, true);
#endif
        }

        /*
         * Read the offset and adjust if we're playing catch-up.
         *
         * The catch-up adjusting work by us decrementing the offset by a percentage of
         * the time elapsed since the previous TMVritualGetSync call. We take some simple
         * precautions against racing other threads here, but assume that this isn't going
         * to be much of a problem since calls to this function is unlikely from threads
         * other than the EMT.
         *
         * It's possible to get a very long or even negative interval between two read
         * for the following reasons:
         *  - Someone might have suspended the process execution, frequently the case when
         *    debugging the process.
         *  - We might be on a different CPU which TSC isn't quite in sync with the
         *    other CPUs in the system.
         *  - RTTimeNanoTS() is returning sligtly different values in GC, R0 and R3 because
         *    of the static variable it uses with the previous read time.
         *  - Another thread is racing us and we might have been preemnted while inside
         *    this function.
         *
         * Assuming nano second virtual time, we can simply ignore any intervals which has
         * any of the upper 32 bits set. This will have the nice sideeffect of allowing us
         * to use (faster) 32-bit math.
         */
        AssertCompile(TMCLOCK_FREQ_VIRTUAL <= 2000000000); /* (assumes low 32-bit >= 2 seconds) */
        uint64_t u64Offset = pVM->tm.s.u64VirtualSyncOffset;
        if (pVM->tm.s.fVirtualSyncCatchUp)
        {
            const uint64_t u64Prev = pVM->tm.s.u64VirtualSyncCatchUpPrev;
            uint64_t u64Delta = u64 - u64Prev;
            if (!(u64Delta >> 32))
            {
                uint32_t u32Sub = ASMDivU64ByU32RetU32(ASMMult2xU32RetU64((uint32_t)u64Delta, pVM->tm.s.u32VirtualSyncCatchupPrecentage),
                                                       100);
                if (u32Sub < (uint32_t)u64Delta)
                {
                    const uint64_t u64NewOffset = u64Offset - u32Sub;
                    if (ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualSyncCatchUpPrev, u64, u64Prev))
                        ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualSyncOffset, u64NewOffset, u64Offset);
                    u64Offset = u64NewOffset;
                }
                else
                {
                    /* we've completely caught up. */
                    if (    ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualSyncCatchUpPrev, u64, u64Prev)
                        &&  ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualSyncOffset, 0, u64Offset))
                        ASMAtomicXchgSize(&pVM->tm.s.fVirtualSyncCatchUp, false);
                }
            }
            else
            {
                /* Update the previous TMVirtualGetSync time it's not a negative delta. */
                if (!(u64Delta >> 63))
                    ASMAtomicCmpXchgU64(&pVM->tm.s.u64VirtualSyncCatchUpPrev, u64, u64Prev);
                Log(("TMVirtualGetSync: u64Delta=%VRU64\n", u64Delta));
            }
        }

        /*
         * Complete the calculation of the current TMCLOCK_VIRTUAL_SYNC time.
         * The current approach will not let us pass any expired timer.
         */
        u64 -= u64Offset;
        if (pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire <= u64)
        {
            if (!VM_FF_ISSET(pVM, VM_FF_TIMER))
            {
                VM_FF_SET(pVM, VM_FF_TIMER);
#ifdef IN_RING3
                REMR3NotifyTimerPending(pVM);
                VMR3NotifyFF(pVM, true);
#endif
            }
            const uint64_t u64Expire = pVM->tm.s.CTXALLSUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire;
            if (u64Expire < u64)
                u64 = u64Expire;
        }
    }
    else
        u64 = pVM->tm.s.u64VirtualSync;
    return u64;
}


/**
 * Gets the current TMCLOCK_VIRTUAL frequency.
 *
 * @returns The freqency.
 * @param   pVM     VM handle.
 */
TMDECL(uint64_t) TMVirtualGetFreq(PVM pVM)
{
    return TMCLOCK_FREQ_VIRTUAL;
}


//#define TM_CONTINUOUS_TIME

/**
 * Resumes the virtual clock.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_INTERNAL_ERROR and VBOX_STRICT assertion if called out of order.
 * @param   pVM     VM handle.
 */
TMDECL(int) TMVirtualResume(PVM pVM)
{
    if (!pVM->tm.s.fVirtualTicking)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualResume);
        pVM->tm.s.u64VirtualOffset = RTTimeNanoTS() - pVM->tm.s.u64Virtual;
        pVM->tm.s.fVirtualTicking = true;
        pVM->tm.s.fVirtualSyncTicking = true;
        return VINF_SUCCESS;
    }

#ifndef TM_CONTINUOUS_TIME
    AssertFailed();
    return VERR_INTERNAL_ERROR;
#else
    return VINF_SUCCESS;
#endif
}


/**
 * Pauses the virtual clock.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VINF_INTERNAL_ERROR and VBOX_STRICT assertion if called out of order.
 * @param   pVM     VM handle.
 */
TMDECL(int) TMVirtualPause(PVM pVM)
{
    if (pVM->tm.s.fVirtualTicking)
    {
#ifndef TM_CONTINUOUS_TIME
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualPause);
        pVM->tm.s.u64Virtual = RTTimeNanoTS() - pVM->tm.s.u64VirtualOffset;
        pVM->tm.s.fVirtualSyncTicking = false;
        pVM->tm.s.fVirtualTicking = false;
#endif
        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_INTERNAL_ERROR;
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
TMDECL(uint64_t) TMVirtualToNano(PVM pVM, uint64_t u64VirtualTicks)
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
TMDECL(uint64_t) TMVirtualToMicro(PVM pVM, uint64_t u64VirtualTicks)
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
TMDECL(uint64_t) TMVirtualToMilli(PVM pVM, uint64_t u64VirtualTicks)
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
TMDECL(uint64_t) TMVirtualFromNano(PVM pVM, uint64_t u64NanoTS)
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
TMDECL(uint64_t) TMVirtualFromMicro(PVM pVM, uint64_t u64MicroTS)
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
TMDECL(uint64_t) TMVirtualFromMilli(PVM pVM, uint64_t u64MilliTS)
{
    AssertCompile(TMCLOCK_FREQ_VIRTUAL == 1000000000);
    return u64MilliTS * 1000000;
}

