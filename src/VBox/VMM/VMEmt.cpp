/* $Id$ */
/** @file
 * VM - Virtual Machine, The Emulation Thread.
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VM
#include <VBox/tm.h>
#include <VBox/dbgf.h>
#include <VBox/em.h>
#include <VBox/pdm.h>
#include <VBox/rem.h>
#include "VMInternal.h"
#include <VBox/vm.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/time.h>



/**
 * The emulation thread.
 *
 * @returns Thread exit code.
 * @param   ThreadSelf  The handle to the executing thread.
 * @param   pvArgs      Pointer to a VMEMULATIONTHREADARGS structure.
 */
DECLCALLBACK(int) vmR3EmulationThread(RTTHREAD ThreadSelf, void *pvArgs)
{
    PVMEMULATIONTHREADARGS pArgs = (PVMEMULATIONTHREADARGS)pvArgs;
    AssertReleaseMsg(pArgs && pArgs->pVM, ("Invalid arguments to the emulation thread!\n"));

    /*
     * Init the native thread member.
     */
    PVM pVM = pArgs->pVM;
    pVM->NativeThreadEMT = RTThreadGetNative(ThreadSelf);

    /*
     * The request loop.
     */
    VMSTATE enmBefore;
    int     rc;
    Log(("vmR3EmulationThread: Emulation thread starting the days work... Thread=%#x pVM=%p\n", ThreadSelf, pVM));
    for (;;)
    {
        /* Requested to exit the EMT thread out of sync? (currently only VMR3WaitForResume) */
        if (setjmp(pVM->vm.s.emtJumpEnv) != 0)
        {
            rc = VINF_SUCCESS;
            break;
        }

        /*
         * Pending requests which needs servicing?
         *
         * We check for state changes in addition to status codes when
         * servicing requests. (Look after the ifs.)
         */
        enmBefore = pVM->enmVMState;
        if (VM_FF_ISSET(pVM, VM_FF_TERMINATE))
        {
            rc = VINF_EM_TERMINATE;
            break;
        }
        else if (pVM->vm.s.pReqs)
        {
            /*
             * Service execute in EMT request.
             */
            rc = VMR3ReqProcess(pVM);
            Log(("vmR3EmulationThread: Req rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else if (VM_FF_ISSET(pVM, VM_FF_DBGF))
        {
            /*
             * Service the debugger request.
             */
            rc = DBGFR3VMMForcedAction(pVM);
            Log(("vmR3EmulationThread: Dbg rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else if (VM_FF_ISSET(pVM, VM_FF_RESET))
        {
            /*
             * Service a delay reset request.
             */
            rc = VMR3Reset(pVM);
            VM_FF_CLEAR(pVM, VM_FF_RESET);
            Log(("vmR3EmulationThread: Reset rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else
        {
            /*
             * Nothing important is pending, so wait for something.
             */
            rc = VMR3Wait(pVM);
            if (VBOX_FAILURE(rc))
                break;
        }

        /*
         * Check for termination requests, these are extremely high priority.
         */
        if (    rc == VINF_EM_TERMINATE
            ||  VM_FF_ISSET(pVM, VM_FF_TERMINATE))
            break;

        /*
         * Some requests (both VMR3Req* and the DBGF) can potentially
         * resume or start the VM, in that case we'll get a change in
         * VM status indicating that we're now running.
         */
        if (    VBOX_SUCCESS(rc)
            &&  enmBefore != pVM->enmVMState
            &&  (pVM->enmVMState == VMSTATE_RUNNING))
        {
            rc = EMR3ExecuteVM(pVM);
            Log(("vmR3EmulationThread: EMR3ExecuteVM() -> rc=%Vrc, enmVMState=%d\n", rc, pVM->enmVMState));
        }

    } /* forever */


    /*
     * Exiting.
     */
    Log(("vmR3EmulationThread: Terminating emulation thread! Thread=%#x pVM=%p rc=%Vrc enmBefore=%d enmVMState=%d\n",
         ThreadSelf, pVM, rc, enmBefore, pVM->enmVMState));
    if (pVM->vm.s.fEMTDoesTheCleanup)
    {
        Log(("vmR3EmulationThread: executing delayed Destroy\n"));
        vmR3Destroy(pVM);
        vmR3DestroyFinalBit(pVM);
        Log(("vmR3EmulationThread: EMT is terminated.\n"));
    }
    else
    {
        /* we don't reset ThreadEMT here because it's used in waiting. */
        pVM->NativeThreadEMT = NIL_RTNATIVETHREAD;
    }
    return rc;
}

/**
 * Wait for VM to be resumed. Handle events like vmR3EmulationThread does.
 * In case the VM is stopped, clean up and long jump to the main EMT loop.
 *
 * @returns VINF_SUCCESS or doesn't return
 * @param   pVM             VM handle.
 */
VMR3DECL(int) VMR3WaitForResume(PVM pVM)
{
    /*
     * The request loop.
     */
    VMSTATE enmBefore;
    int     rc;
    for (;;)
    {

        /*
         * Pending requests which needs servicing?
         *
         * We check for state changes in addition to status codes when
         * servicing requests. (Look after the ifs.)
         */
        enmBefore = pVM->enmVMState;
        if (VM_FF_ISSET(pVM, VM_FF_TERMINATE))
        {
            rc = VINF_EM_TERMINATE;
            break;
        }
        else if (pVM->vm.s.pReqs)
        {
            /*
             * Service execute in EMT request.
             */
            rc = VMR3ReqProcess(pVM);
            Log(("vmR3EmulationThread: Req rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else if (VM_FF_ISSET(pVM, VM_FF_DBGF))
        {
            /*
             * Service the debugger request.
             */
            rc = DBGFR3VMMForcedAction(pVM);
            Log(("vmR3EmulationThread: Dbg rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else if (VM_FF_ISSET(pVM, VM_FF_RESET))
        {
            /*
             * Service a delay reset request.
             */
            rc = VMR3Reset(pVM);
            VM_FF_CLEAR(pVM, VM_FF_RESET);
            Log(("vmR3EmulationThread: Reset rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else
        {
            /*
             * Nothing important is pending, so wait for something.
             */
            rc = VMR3Wait(pVM);
            if (VBOX_FAILURE(rc))
                break;
        }

        /*
         * Check for termination requests, these are extremely high priority.
         */
        if (    rc == VINF_EM_TERMINATE
            ||  VM_FF_ISSET(pVM, VM_FF_TERMINATE))
            break;

        /*
         * Some requests (both VMR3Req* and the DBGF) can potentially
         * resume or start the VM, in that case we'll get a change in
         * VM status indicating that we're now running.
         */
        if (    VBOX_SUCCESS(rc)
            &&  enmBefore != pVM->enmVMState
            &&  (pVM->enmVMState == VMSTATE_RUNNING))
        {
            /* Only valid exit reason. */
            return VINF_SUCCESS;
        }

    } /* forever */

    /* Return to the main loop in vmR3EmulationThread, which will clean up for us. */
    longjmp(pVM->vm.s.emtJumpEnv, 1);
}

/**
 * Notify the emulation thread (EMT) about pending Forced Action (FF).
 *
 * This function is called by thread other than EMT to make
 * sure EMT wakes up and promptly service an FF request.
 *
 * @param   pVM             VM handle.
 * @param   fNotifiedREM    Set if REM have already been notified. If clear the
 *                          generic REMR3NotifyFF() method is called.
 */
VMR3DECL(void) VMR3NotifyFF(PVM pVM, bool fNotifiedREM)
{
    LogFlow(("VMR3NotifyFF:\n"));
    if (pVM->vm.s.fWait)
    {
        int rc = RTSemEventSignal(pVM->vm.s.EventSemWait);
        AssertRC(rc);
    }
    else if (!fNotifiedREM)
        REMR3NotifyFF(pVM);
}


/**
 * The old halt loop.
 */
DECLCALLBACK(int) vmR3WaitHaltedOld(PVM pVM, const uint32_t fMask)
{
    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 1);
    //unsigned cLoops = 0;
    for (;;)
    {
        /*
         * Work the timers and check if we can exit.
         * The poll call gives us the ticks left to the next event in
         * addition to perhaps set an FF.
         */
        STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltPoll, a);
        PDMR3Poll(pVM);
        STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltPoll, a);
        STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltTimers, b);
        if (VM_FF_ISPENDING(pVM, fMask))
            break;
        uint64_t u64NanoTS = TMVirtualToNano(pVM, TMTimerPoll(pVM));
        if (VM_FF_ISPENDING(pVM, fMask))
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        if (u64NanoTS < 50000)
        {
            //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d spin\n", u64NanoTS, cLoops++);
            /* spin */;
        }
        else
        {
            VMMR3YieldStop(pVM);
            //uint64_t u64Start = RTTimeNanoTS();
            if (u64NanoTS <  870000) /* this is a bit speculative... works fine on linux. */
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d yield", u64NanoTS, cLoops++);
                STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltYield, a);
                RTThreadYield(); /* this is the best we can do here */
                STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltYield, a);
            }
            else if (u64NanoTS < 2000000)
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep 1ms", u64NanoTS, cLoops++);
                STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pVM->vm.s.EventSemWait, 1);
                STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltBlock, a);
            }
            else
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep %dms", u64NanoTS, cLoops++, (uint32_t)RT_MIN((u64NanoTS - 500000) / 1000000, 15));
                STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pVM->vm.s.EventSemWait, RT_MIN((u64NanoTS - 1000000) / 1000000, 15));
                STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltBlock, a);
            }
            //uint64_t u64Slept = RTTimeNanoTS() - u64Start;
            //RTLogPrintf(" -> rc=%Vrc in %RU64 ns / %RI64 ns delta\n", rc, u64Slept, u64NanoTS - u64Slept);
        }
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (VBOX_FAILURE(rc))
        {
            AssertRC(rc != VERR_INTERRUPTED);
            AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }

    return rc;
}


/**
 * Method 1 - Block whenever possible, and when lagging behind
 * switch to spinning for 10-30ms with occational blocking until
 * the lag has been eliminated.
 */
DECLCALLBACK(int) vmR3WaitHaltedMethod1(PVM pVM, const uint32_t fMask, uint64_t u64Now)
{
    /*
     * To simplify things, we decide up-front whether we should switch
     * to spinning or not. This makes some assumptions about the cause
     * of the spinning (PIT/RTC/PCNet) and that it will generate interrupts
     * or other events that means we should exit the halt loop.
     */
    bool fBlockOnce = false;
    bool fSpinning = false;
    uint32_t u32CatchUpPct = TMVirtualSyncGetCatchUpPct(pVM);
    if (u32CatchUpPct /* non-zero if catching up */)
    {
        if (pVM->vm.s.Halt.Method12.u64StartSpinTS)
        {
            fSpinning = TMVirtualSyncGetLag(pVM) >= 2*1000000;
            if (fSpinning)
            {
                uint64_t u64Lag = TMVirtualSyncGetLag(pVM);
                fBlockOnce = u64Now - pVM->vm.s.Halt.Method12.u64LastBlockTS
                           > RT_MAX(5*1000000, RT_MIN(u64Lag / 4, 200*1000000));
            }
            else
            {
RTLogRelPrintf("Stopped spinning (%u ms)\n", (u64Now - pVM->vm.s.Halt.Method12.u64StartSpinTS) / 1000000);
                pVM->vm.s.Halt.Method12.u64StartSpinTS = 0;
            }
        }
        else
        {
            fSpinning = TMVirtualSyncGetLag(pVM) >= 20*1000000;
            if (fSpinning)
                pVM->vm.s.Halt.Method12.u64StartSpinTS = u64Now;
        }
    }
    else if (pVM->vm.s.Halt.Method12.u64StartSpinTS)
    {
RTLogRelPrintf("Stopped spinning (%u ms)\n", (u64Now - pVM->vm.s.Halt.Method12.u64StartSpinTS) / 1000000);
        pVM->vm.s.Halt.Method12.u64StartSpinTS = 0;
    }

    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 1);
    unsigned cLoops = 0;
    for (;; cLoops++)
    {
        /*
         * Work the timers and check if we can exit.
         */
        STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltPoll, a);
        PDMR3Poll(pVM);
        STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltPoll, a);
        STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltTimers, b);
        if (VM_FF_ISPENDING(pVM, fMask))
            break;

        /*
         * Estimate time left to the next event.
         */
        uint64_t u64NanoTS = TMVirtualToNano(pVM, TMTimerPoll(pVM));
        if (VM_FF_ISPENDING(pVM, fMask))
            break;

        /*
         * Block if we're not spinning and the interval isn't all that small.
         */
        if (    (   !fSpinning
                 || fBlockOnce)
            &&  u64NanoTS >= 250000) /* 0.250 ms */
        {
            const uint64_t Start = pVM->vm.s.Halt.Method12.u64LastBlockTS = RTTimeNanoTS();
            VMMR3YieldStop(pVM);

            uint32_t cMilliSecs = RT_MIN(u64NanoTS / 1000000, 15);
            if (cMilliSecs <= pVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg)
                cMilliSecs = 1;
            else
                cMilliSecs -= pVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg;
RTLogRelPrintf("u64NanoTS=%RI64 cLoops=%3d sleep %02dms (%7RU64) ", u64NanoTS, cLoops, cMilliSecs, u64NanoTS);
            STAM_REL_PROFILE_START(&pVM->vm.s.StatHaltBlock, a);
            rc = RTSemEventWait(pVM->vm.s.EventSemWait, cMilliSecs);
            STAM_REL_PROFILE_STOP(&pVM->vm.s.StatHaltBlock, a);
            if (rc == VERR_TIMEOUT)
                rc = VINF_SUCCESS;
            else if (VBOX_FAILURE(rc))
            {
                AssertRC(rc != VERR_INTERRUPTED);
                AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
                VM_FF_SET(pVM, VM_FF_TERMINATE);
                rc = VERR_INTERNAL_ERROR;
                break;
            }

            /*
             * Calc the statistics.
             * Update averages every 16th time, and flush parts of the history every 64th time.
             */
            const uint64_t Elapsed = RTTimeNanoTS() - Start;
            pVM->vm.s.Halt.Method12.cNSBlocked += Elapsed;
            if (Elapsed > u64NanoTS)
                pVM->vm.s.Halt.Method12.cNSBlockedTooLong += Elapsed - u64NanoTS;
            pVM->vm.s.Halt.Method12.cBlocks++;
            if (!(pVM->vm.s.Halt.Method12.cBlocks & 0xf))
            {
                pVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg = pVM->vm.s.Halt.Method12.cNSBlockedTooLong / pVM->vm.s.Halt.Method12.cBlocks;
                if (!(pVM->vm.s.Halt.Method12.cBlocks & 0x3f))
                {
                    pVM->vm.s.Halt.Method12.cNSBlockedTooLong = pVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg * 0x40;
                    pVM->vm.s.Halt.Method12.cBlocks = 0x40;
                }
            }
RTLogRelPrintf(" -> %7RU64 ns / %7RI64 ns delta%s\n", Elapsed, Elapsed - u64NanoTS, fBlockOnce ? " (block once)" : "");

            /*
             * Clear the block once flag if we actually blocked.
             */
            if (    fBlockOnce
                &&  Elapsed > 100000 /* 0.1 ms */)
                fBlockOnce = false;
        }
    }
if (fSpinning) RTLogRelPrintf("spun for %RU64 ns %u loops; lag=%RU64 pct=%d\n", RTTimeNanoTS() - u64Now, cLoops, TMVirtualSyncGetLag(pVM), u32CatchUpPct);

    return rc;
}


/**
 * Halted VM Wait.
 * Any external event will unblock the thread.
 *
 * @returns VINF_SUCCESS unless a fatal error occured. In the latter
 *          case an appropriate status code is returned.
 * @param   pVM         VM handle.
 * @param   fIgnoreInterrupts   If set the VM_FF_INTERRUPT flags is ignored.
 * @thread  The emulation thread.
 */
VMR3DECL(int) VMR3WaitHalted(PVM pVM, bool fIgnoreInterrupts)
{
    LogFlow(("VMR3WaitHalted: fIgnoreInterrupts=%d\n", fIgnoreInterrupts));

    /*
     * Check Relevant FFs.
     */
    const uint32_t fMask = !fIgnoreInterrupts
        ? VM_FF_EXTERNAL_HALTED_MASK
        : VM_FF_EXTERNAL_HALTED_MASK & ~(VM_FF_INTERRUPT_APIC | VM_FF_INTERRUPT_PIC);
    if (VM_FF_ISPENDING(pVM, fMask))
    {
        LogFlow(("VMR3WaitHalted: returns VINF_SUCCESS (FF %#x)\n", pVM->fForcedActions));
        return VINF_SUCCESS;
    }

    /*
     * The yielder is suspended while we're halting.
     */
    VMMR3YieldSuspend(pVM);

    /*
     * Record halt averages for the last second.
     */
    uint64_t u64Now = RTTimeNanoTS();
    int64_t off = u64Now - pVM->vm.s.u64HaltsStartTS;
    if (off > 1000000000)
    {
        if (off > _4G || !pVM->vm.s.cHalts)
        {
            pVM->vm.s.HaltInterval = 1000000000 /* 1 sec */;
            pVM->vm.s.HaltFrequency = 1;
        }
        else
        {
            pVM->vm.s.HaltInterval = (uint32_t)off / pVM->vm.s.cHalts;
            pVM->vm.s.HaltFrequency = ASMMultU64ByU32DivByU32(pVM->vm.s.cHalts, 1000000000, (uint32_t)off);
        }
        pVM->vm.s.u64HaltsStartTS = u64Now;
        pVM->vm.s.cHalts = 0;
    }
    pVM->vm.s.cHalts++;

    /*
     * Do the halt.
     */
#if 1
    int rc = vmR3WaitHaltedOld(pVM, fMask);
#elif 0 /* work in progress */
    int rc = vmR3WaitHaltedMethod1(pVM, fMask, u64Now);
#else
# error "misconfigured halt"
#endif

    /*
     * Resume the yielder and tell the world we're not blocking.
     */
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 0);
    VMMR3YieldResume(pVM);

    LogFlow(("VMR3WaitHalted: returns %Vrc (FF %#x)\n", rc, pVM->fForcedActions));
    return rc;
}


/**
 * Suspended VM Wait.
 * Only a handful of forced actions will cause the function to
 * return to the caller.
 *
 * @returns VINF_SUCCESS unless a fatal error occured. In the latter
 *          case an appropriate status code is returned.
 * @param   pVM         VM handle.
 * @thread  The emulation thread.
 */
VMR3DECL(int) VMR3Wait(PVM pVM)
{
    LogFlow(("VMR3Wait:\n"));

    /*
     * Check Relevant FFs.
     */
    if (VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_SUSPENDED_MASK))
    {
        LogFlow(("VMR3Wait: returns VINF_SUCCESS (FF %#x)\n", pVM->fForcedActions));
        return VINF_SUCCESS;
    }

    int rc = VINF_SUCCESS;
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 1);
    for (;;)
    {
        /*
         * Check Relevant FFs.
         */
        if (VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_SUSPENDED_MASK))
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        rc = RTSemEventWait(pVM->vm.s.EventSemWait, 1000);
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 0);

    LogFlow(("VMR3Wait: returns %Vrc (FF %#x)\n", rc, pVM->fForcedActions));
    return rc;
}

