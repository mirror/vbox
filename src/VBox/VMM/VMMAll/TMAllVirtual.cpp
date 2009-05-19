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
        if (fCheckTimers)
        {
            PVMCPU pVCpuDst = &pVM->aCpus[pVM->tm.s.idTimerCpu];
            if (    !VMCPU_FF_ISSET(pVCpuDst, VMCPU_FF_TIMER)
                &&  !pVM->tm.s.fRunningQueues
                &&  (   pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64
                     || (   pVM->tm.s.fVirtualSyncTicking
                         && pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire <= u64 - pVM->tm.s.offVirtualSync
                        )
                    )
                &&  !pVM->tm.s.fRunningQueues
               )
            {
                STAM_COUNTER_INC(&pVM->tm.s.StatVirtualGetSetFF);
                Log5(("TMAllVirtual(%u): FF: %d -> 1\n", __LINE__, VMCPU_FF_ISPENDING(pVCpuDst, VMCPU_FF_TIMER)));
                VMCPU_FF_SET(pVCpuDst, VMCPU_FF_TIMER);
#ifdef IN_RING3
                REMR3NotifyTimerPending(pVM, pVCpuDst);
                VMR3NotifyCpuFFU(pVCpuDst->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
            }
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
    return tmVirtualGet(pVM, true /* check timers */);
}


/**
 * Gets the current TMCLOCK_VIRTUAL time without checking
 * timers or anything.
 *
 * Meaning, this has no side effect on FFs like TMVirtualGet may have.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 *
 * @remarks See TMVirtualGet.
 */
VMMDECL(uint64_t) TMVirtualGetNoCheck(PVM pVM)
{
    return tmVirtualGet(pVM, false /*fCheckTimers*/);
}


/**
 * tmVirtualSyncGetLocked worker for handling catch-up when owning the lock.
 *
 * @returns The timestamp.
 * @param   pVM                 VM handle.
 * @param   u64                 raw virtual time.
 * @param   off                 offVirtualSync.
 */
DECLINLINE(uint64_t) tmVirtualSyncGetHandleCatchUpLocked(PVM pVM, uint64_t u64, uint64_t off)
{
    STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetLocked);

    /*
     * Don't make updates untill
     */
    bool            fUpdatePrev = true;
    bool            fUpdateOff  = true;
    bool            fStop       = false;
    const uint64_t  u64Prev     = pVM->tm.s.u64VirtualSyncCatchUpPrev;
    uint64_t        u64Delta    = u64 - u64Prev;
    if (RT_LIKELY(!(u64Delta >> 32)))
    {
        uint64_t u64Sub = ASMMultU64ByU32DivByU32(u64Delta, pVM->tm.s.u32VirtualSyncCatchUpPercentage, 100);
        if (off > u64Sub + pVM->tm.s.offVirtualSyncGivenUp)
        {
            off -= u64Sub;
            Log4(("TM: %RU64/%RU64: sub %RU32\n", u64 - off, off - pVM->tm.s.offVirtualSyncGivenUp, u64Sub));
        }
        else
        {
            /* we've completely caught up. */
            STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);
            off = pVM->tm.s.offVirtualSyncGivenUp;
            fStop = true;
            Log4(("TM: %RU64/0: caught up\n", u64));
        }
    }
    else
    {
        /* More than 4 seconds since last time (or negative), ignore it. */
        fUpdateOff = false;
        fUpdatePrev = !(u64Delta & RT_BIT_64(63));
        Log(("TMVirtualGetSync: u64Delta=%RX64\n", u64Delta));
    }

    /*
     * Complete the calculation of the current TMCLOCK_VIRTUAL_SYNC time. The current
     * approach is to never pass the head timer. So, when we do stop the clock and
     * set the timer pending flag.
     */
    u64 -= off;
    uint64_t u64Expire = ASMAtomicReadU64(&pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire);
    if (u64 < u64Expire)
    {
        if (fUpdateOff)
            ASMAtomicWriteU64(&pVM->tm.s.offVirtualSync, off);
        if (fStop)
            ASMAtomicWriteBool(&pVM->tm.s.fVirtualSyncCatchUp, false);
        if (fUpdatePrev)
            ASMAtomicWriteU64(&pVM->tm.s.u64VirtualSyncCatchUpPrev, u64);
        tmVirtualSyncUnlock(pVM);
    }
    else
    {
        u64 = u64Expire;
        ASMAtomicWriteU64(&pVM->tm.s.u64VirtualSync, u64);
        ASMAtomicWriteBool(&pVM->tm.s.fVirtualSyncTicking, false);

        VM_FF_SET(pVM, VM_FF_TM_VIRTUAL_SYNC);
        PVMCPU pVCpuDst = &pVM->aCpus[pVM->tm.s.idTimerCpu];
        VMCPU_FF_SET(pVCpuDst, VMCPU_FF_TIMER);
        Log5(("TMAllVirtual(%u): FF: %d -> 1\n", __LINE__, VMCPU_FF_ISPENDING(pVCpuDst, VMCPU_FF_TIMER)));
        Log4(("TM: %RU64/%RU64: exp tmr=>ff\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
        tmVirtualSyncUnlock(pVM);

#ifdef IN_RING3
        REMR3NotifyTimerPending(pVM, pVCpuDst);
        VMR3NotifyCpuFFU(pVCpuDst->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetSetFF);
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetExpired);
    }
    STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetLocked);

    return u64;
}


/**
 * tmVirtualSyncGetEx worker for when we get the lock.
 *
 * @returns timesamp.
 * @param   pVM                 The VM handle.
 * @param   u64                 The virtual clock timestamp.
 */
DECLINLINE(uint64_t) tmVirtualSyncGetLocked(PVM pVM, uint64_t u64)
{
    /*
     * Not ticking?
     */
    if (!pVM->tm.s.fVirtualSyncTicking)
    {
        u64 = ASMAtomicUoReadU64(&pVM->tm.s.u64VirtualSync);
        tmVirtualSyncUnlock(pVM);
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetLocked);
        return u64;
    }

    /*
     * Handle catch up in a separate function.
     */
    uint64_t off = ASMAtomicUoReadU64(&pVM->tm.s.offVirtualSync);
    if (ASMAtomicUoReadBool(&pVM->tm.s.fVirtualSyncCatchUp))
        return tmVirtualSyncGetHandleCatchUpLocked(pVM, u64, off);

    /*
     * Complete the calculation of the current TMCLOCK_VIRTUAL_SYNC time. The current
     * approach is to never pass the head timer. So, when we do stop the clock and
     * set the timer pending flag.
     */
    u64 -= off;
    uint64_t u64Expire = ASMAtomicReadU64(&pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire);
    if (u64 < u64Expire)
        tmVirtualSyncUnlock(pVM);
    else
    {
        u64 = u64Expire;
        ASMAtomicWriteU64(&pVM->tm.s.u64VirtualSync, u64);
        ASMAtomicWriteBool(&pVM->tm.s.fVirtualSyncTicking, false);

        VM_FF_SET(pVM, VM_FF_TM_VIRTUAL_SYNC);
        PVMCPU pVCpuDst = &pVM->aCpus[pVM->tm.s.idTimerCpu];
        VMCPU_FF_SET(pVCpuDst, VMCPU_FF_TIMER);
        Log5(("TMAllVirtual(%u): FF: %d -> 1\n", __LINE__, VMCPU_FF_ISPENDING(pVCpuDst, VMCPU_FF_TIMER)));
        Log4(("TM: %RU64/%RU64: exp tmr=>ff\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
        tmVirtualSyncUnlock(pVM);

#ifdef IN_RING3
        REMR3NotifyTimerPending(pVM, pVCpuDst);
        VMR3NotifyCpuFFU(pVCpuDst->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetSetFF);
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetExpired);
    }
    STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetLocked);
    return u64;
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 * @param   fCheckTimers    Check timers or not
 * @thread  EMT.
 */
DECLINLINE(uint64_t) tmVirtualSyncGetEx(PVM pVM, bool fCheckTimers)
{
    STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGet);

    if (!pVM->tm.s.fVirtualSyncTicking)
        return pVM->tm.s.u64VirtualSync;

    /*
     * Query the virtual clock and do the usual expired timer check.
     */
    Assert(pVM->tm.s.cVirtualTicking);
    uint64_t u64 = tmVirtualGetRaw(pVM);
    if (fCheckTimers)
    {
        PVMCPU pVCpuDst = &pVM->aCpus[pVM->tm.s.idTimerCpu];
        if (    !VMCPU_FF_ISSET(pVCpuDst, VMCPU_FF_TIMER)
            &&  pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL].u64Expire <= u64)
        {
            Log5(("TMAllVirtual(%u): FF: 0 -> 1\n", __LINE__));
            VMCPU_FF_SET(pVCpuDst, VMCPU_FF_TIMER);
#ifdef IN_RING3
            REMR3NotifyTimerPending(pVM, pVCpuDst);
            VMR3NotifyCpuFFU(pVCpuDst->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM /** @todo |VMNOTIFYFF_FLAGS_POKE*/);
#endif
            STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetSetFF);
        }
    }

    /*
     * When the clock is ticking, not doing catch ups and not running into an
     * expired time, we can get away without locking. Try this first.
     */
    uint64_t off;
    if (ASMAtomicUoReadBool(&pVM->tm.s.fVirtualSyncTicking))
    {
        if (!ASMAtomicUoReadBool(&pVM->tm.s.fVirtualSyncCatchUp))
        {
            off = ASMAtomicReadU64(&pVM->tm.s.offVirtualSync);
            if (RT_LIKELY(   ASMAtomicUoReadBool(&pVM->tm.s.fVirtualSyncTicking)
                          && !ASMAtomicUoReadBool(&pVM->tm.s.fVirtualSyncCatchUp)
                          && off == ASMAtomicReadU64(&pVM->tm.s.offVirtualSync)))
            {
                off = u64 - off;
                if (off < ASMAtomicReadU64(&pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire))
                {
                    STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetLockless);
                    return off;
                }
            }
        }
    }
    else
    {
        off = ASMAtomicReadU64(&pVM->tm.s.u64VirtualSync);
        if (RT_LIKELY(!ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncTicking)))
        {
            STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetLockless);
            return off;
        }
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
    int cOuterTries = 42;
    for (;; cOuterTries--)
    {
        /* Try grab the lock, things get simpler when owning the lock. */
        int rcLock = tmVirtualSyncTryLock(pVM);
        if (RT_SUCCESS_NP(rcLock))
            return tmVirtualSyncGetLocked(pVM, u64);

        /* Re-check the ticking flag. */
        if (!ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncTicking))
        {
            off = ASMAtomicReadU64(&pVM->tm.s.u64VirtualSync);
            if (   ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncTicking)
                && cOuterTries > 0)
                continue;
            return off;
        }

        off = ASMAtomicReadU64(&pVM->tm.s.offVirtualSync);
        if (ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncCatchUp))
        {
            /* No changes allowed, try get a consistent set of parameters. */
            uint64_t const u64Prev    = ASMAtomicReadU64(&pVM->tm.s.u64VirtualSyncCatchUpPrev);
            uint64_t const offGivenUp = ASMAtomicReadU64(&pVM->tm.s.offVirtualSyncGivenUp);
            uint32_t const u32Pct     = ASMAtomicReadU32(&pVM->tm.s.u32VirtualSyncCatchUpPercentage);
            if (    (   u64Prev    == ASMAtomicReadU64(&pVM->tm.s.u64VirtualSyncCatchUpPrev)
                     && offGivenUp == ASMAtomicReadU64(&pVM->tm.s.offVirtualSyncGivenUp)
                     && u32Pct     == ASMAtomicReadU32(&pVM->tm.s.u32VirtualSyncCatchUpPercentage)
                     && ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncCatchUp))
                ||  cOuterTries <= 0)
            {
                uint64_t u64Delta = u64 - u64Prev;
                if (RT_LIKELY(!(u64Delta >> 32)))
                {
                    uint64_t u64Sub = ASMMultU64ByU32DivByU32(u64Delta, u32Pct, 100);
                    if (off > u64Sub + offGivenUp)
                    {
                        off -= u64Sub;
                        Log4(("TM: %RU64/%RU64: sub %RU32 (NoLock)\n", u64 - off, pVM->tm.s.offVirtualSync - offGivenUp, u64Sub));
                    }
                    else
                    {
                        /* we've completely caught up. */
                        STAM_PROFILE_ADV_STOP(&pVM->tm.s.StatVirtualSyncCatchup, c);
                        off = offGivenUp;
                        Log4(("TM: %RU64/0: caught up\n", u64));
                    }
                }
                else
                    /* More than 4 seconds since last time (or negative), ignore it. */
                    Log(("TMVirtualGetSync: u64Delta=%RX64 (NoLock)\n", u64Delta));

                /* Check that we're still running and in catch up. */
                if (    ASMAtomicUoReadBool(&pVM->tm.s.fVirtualSyncTicking)
                    &&  ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncCatchUp))
                    break;
                if (cOuterTries <= 0)
                    break; /* enough */
            }
        }
        else if (   off == ASMAtomicReadU64(&pVM->tm.s.offVirtualSync)
                 && !ASMAtomicReadBool(&pVM->tm.s.fVirtualSyncCatchUp))
            break; /* Got an consistent offset */
        else if (cOuterTries <= 0)
            break; /* enough */
    }
    if (cOuterTries <= 0)
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetELoop);

    /*
     * Complete the calculation of the current TMCLOCK_VIRTUAL_SYNC time. The current
     * approach is to never pass the head timer. So, when we do stop the clock and
     * set the timer pending flag.
     */
    u64 -= off;
    uint64_t u64Expire = ASMAtomicReadU64(&pVM->tm.s.CTX_SUFF(paTimerQueues)[TMCLOCK_VIRTUAL_SYNC].u64Expire);
    if (u64 >= u64Expire)
    {
        PVMCPU pVCpuDst = &pVM->aCpus[pVM->tm.s.idTimerCpu];
        if (!VMCPU_FF_ISSET(pVCpuDst, VMCPU_FF_TIMER))
        {
            Log5(("TMAllVirtual(%u): FF: %d -> 1 (NoLock)\n", __LINE__, VMCPU_FF_ISPENDING(pVCpuDst, VMCPU_FF_TIMER)));
            VM_FF_SET(pVM, VM_FF_TM_VIRTUAL_SYNC); /* Hmm? */
            VMCPU_FF_SET(pVCpuDst, VMCPU_FF_TIMER);
#ifdef IN_RING3
            REMR3NotifyTimerPending(pVM, pVCpuDst);
            VMR3NotifyCpuFFU(pVCpuDst->pUVCpu, VMNOTIFYFF_FLAGS_DONE_REM);
#endif
            STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetSetFF);
            Log4(("TM: %RU64/%RU64: exp tmr=>ff (NoLock)\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
        }
        else
            Log4(("TM: %RU64/%RU64: exp tmr (NoLock)\n", u64, pVM->tm.s.offVirtualSync - pVM->tm.s.offVirtualSyncGivenUp));
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualSyncGetExpired);
    }

    return u64;
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM             VM handle.
 * @thread  EMT.
 * @remarks May set the timer and virtual sync FFs.
 */
VMMDECL(uint64_t) TMVirtualSyncGet(PVM pVM)
{
    return tmVirtualSyncGetEx(pVM, true /* check timers */);
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time without checking timers running on
 * TMCLOCK_VIRTUAL.
 *
 * @returns The timestamp.
 * @param   pVM             VM handle.
 * @thread  EMT.
 * @remarks May set the timer and virtual sync FFs.
 */
VMMDECL(uint64_t) TMVirtualSyncGetNoCheck(PVM pVM)
{
    return tmVirtualSyncGetEx(pVM, false /* check timers */);
}


/**
 * Gets the current TMCLOCK_VIRTUAL_SYNC time.
 *
 * @returns The timestamp.
 * @param   pVM     VM handle.
 * @param   fCheckTimers    Check timers on the virtual clock or not.
 * @thread  EMT.
 * @remarks May set the timer and virtual sync FFs.
 */
VMMDECL(uint64_t) TMVirtualSyncGetEx(PVM pVM, bool fCheckTimers)
{
    return tmVirtualSyncGetEx(pVM, fCheckTimers);
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
 * Worker for TMR3PauseClocks.
 *
 * @returns VINF_SUCCESS or VERR_INTERNAL_ERROR (asserted).
 * @param   pVM     The VM handle.
 */
int tmVirtualPauseLocked(PVM pVM)
{
    uint32_t c = ASMAtomicDecU32(&pVM->tm.s.cVirtualTicking);
    AssertMsgReturn(c < pVM->cCPUs, ("%u vs %u\n", c, pVM->cCPUs), VERR_INTERNAL_ERROR);
    if (c == 0)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualPause);
        pVM->tm.s.u64Virtual = tmVirtualGetRaw(pVM);
        ASMAtomicWriteBool(&pVM->tm.s.fVirtualSyncTicking, false);
    }
    return VINF_SUCCESS;
}


/**
 * Worker for TMR3ResumeClocks.
 *
 * @returns VINF_SUCCESS or VERR_INTERNAL_ERROR (asserted).
 * @param   pVM     The VM handle.
 */
int tmVirtualResumeLocked(PVM pVM)
{
    uint32_t c = ASMAtomicIncU32(&pVM->tm.s.cVirtualTicking);
    AssertMsgReturn(c <= pVM->cCPUs, ("%u vs %u\n", c, pVM->cCPUs), VERR_INTERNAL_ERROR);
    if (c == 1)
    {
        STAM_COUNTER_INC(&pVM->tm.s.StatVirtualResume);
        pVM->tm.s.u64VirtualRawPrev         = 0;
        pVM->tm.s.u64VirtualWarpDriveStart  = tmVirtualGetRawNanoTS(pVM);
        pVM->tm.s.u64VirtualOffset          = pVM->tm.s.u64VirtualWarpDriveStart - pVM->tm.s.u64Virtual;
        ASMAtomicWriteBool(&pVM->tm.s.fVirtualSyncTicking, true);
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

