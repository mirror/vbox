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
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VM
#include <VBox/tm.h>
#include <VBox/dbgf.h>
#include <VBox/em.h>
#include <VBox/pdmapi.h>
#include <VBox/rem.h>
#include "VMInternal.h"
#include <VBox/vm.h>
#include <VBox/uvm.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>




/**
 * The emulation thread.
 *
 * @returns Thread exit code.
 * @param   ThreadSelf  The handle to the executing thread.
 * @param   pvArgs      Pointer to the user mode VM structure (UVM).
 */
DECLCALLBACK(int) vmR3EmulationThread(RTTHREAD ThreadSelf, void *pvArgs)
{
    PUVM pUVM = (PUVM)pvArgs;
    AssertReleaseMsg(VALID_PTR(pUVM) && pUVM->u32Magic == UVM_MAGIC,
                     ("Invalid arguments to the emulation thread!\n"));

    /*
     * Init the native thread member.
     */
    pUVM->vm.s.NativeThreadEMT = RTThreadGetNative(ThreadSelf);

    /*
     * The request loop.
     */
    int     rc = VINF_SUCCESS;
    VMSTATE enmBefore = VMSTATE_CREATING;
    Log(("vmR3EmulationThread: Emulation thread starting the days work... Thread=%#x pUVM=%p\n", ThreadSelf, pUVM));
    for (;;)
    {
        /* Requested to exit the EMT thread out of sync? (currently only VMR3WaitForResume) */
        if (setjmp(pUVM->vm.s.emtJumpEnv) != 0)
        {
            rc = VINF_SUCCESS;
            break;
        }

        /*
         * During early init there is no pVM, so make a special path
         * for that to keep things clearly separate.
         */
        if (!pUVM->pVM)
        {
            /*
             * Check for termination first.
             */
            if (pUVM->vm.s.fTerminateEMT)
            {
                rc = VINF_EM_TERMINATE;
                break;
            }
            if (pUVM->vm.s.pReqs)
            {
                /*
                 * Service execute in EMT request.
                 */
                rc = VMR3ReqProcessU(pUVM);
                Log(("vmR3EmulationThread: Req rc=%Vrc, VM state %d -> %d\n", rc, enmBefore, pUVM->pVM ? pUVM->pVM->enmVMState : VMSTATE_CREATING));
            }
            else
            {
                /*
                 * Nothing important is pending, so wait for something.
                 */
                rc = VMR3WaitU(pUVM);
                if (VBOX_FAILURE(rc))
                    break;
            }
        }
        else
        {

            /*
             * Pending requests which needs servicing?
             *
             * We check for state changes in addition to status codes when
             * servicing requests. (Look after the ifs.)
             */
            PVM pVM = pUVM->pVM;
            enmBefore = pVM->enmVMState;
            if (    VM_FF_ISSET(pVM, VM_FF_TERMINATE)
                ||  pUVM->vm.s.fTerminateEMT)
            {
                rc = VINF_EM_TERMINATE;
                break;
            }
            if (pUVM->vm.s.pReqs)
            {
                /*
                 * Service execute in EMT request.
                 */
                rc = VMR3ReqProcessU(pUVM);
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
                 * Service a delayed reset request.
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
                rc = VMR3WaitU(pUVM);
                if (VBOX_FAILURE(rc))
                    break;
            }

            /*
             * Check for termination requests, these have extremely high priority.
             */
            if (    rc == VINF_EM_TERMINATE
                ||  VM_FF_ISSET(pVM, VM_FF_TERMINATE)
                ||  pUVM->vm.s.fTerminateEMT)
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
                if (EMGetState(pVM) == EMSTATE_GURU_MEDITATION)
                    vmR3SetState(pVM, VMSTATE_GURU_MEDITATION);
            }
        }
    } /* forever */


    /*
     * Exiting.
     */
    Log(("vmR3EmulationThread: Terminating emulation thread! Thread=%#x pUVM=%p rc=%Vrc enmBefore=%d enmVMState=%d\n",
         ThreadSelf, pUVM, rc, enmBefore, pUVM->pVM ? pUVM->pVM->enmVMState : VMSTATE_TERMINATED));
    if (pUVM->vm.s.fEMTDoesTheCleanup)
    {
        Log(("vmR3EmulationThread: executing delayed Destroy\n"));
        Assert(pUVM->pVM);
        vmR3Destroy(pUVM->pVM);
        vmR3DestroyFinalBitFromEMT(pUVM);
    }
    else
    {
        vmR3DestroyFinalBitFromEMT(pUVM);

        /* we don't reset ThreadEMT here because it's used in waiting. */
        pUVM->vm.s.NativeThreadEMT = NIL_RTNATIVETHREAD;
    }
    Log(("vmR3EmulationThread: EMT is terminated.\n"));
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
    PUVM    pUVM = pVM->pUVM;
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
        if (    VM_FF_ISSET(pVM, VM_FF_TERMINATE)
            ||  pUVM->vm.s.fTerminateEMT)
        {
            rc = VINF_EM_TERMINATE;
            break;
        }
        else if (pUVM->vm.s.pReqs)
        {
            /*
             * Service execute in EMT request.
             */
            rc = VMR3ReqProcessU(pUVM);
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
            rc = VMR3WaitU(pUVM);
            if (VBOX_FAILURE(rc))
                break;
        }

        /*
         * Check for termination requests, these are extremely high priority.
         */
        if (    rc == VINF_EM_TERMINATE
            ||  VM_FF_ISSET(pVM, VM_FF_TERMINATE)
            ||  pUVM->vm.s.fTerminateEMT)
            break;

        /*
         * Some requests (both VMR3Req* and the DBGF) can potentially
         * resume or start the VM, in that case we'll get a change in
         * VM status indicating that we're now running.
         */
        if (    VBOX_SUCCESS(rc)
            &&  enmBefore != pVM->enmVMState
            &&  pVM->enmVMState == VMSTATE_RUNNING)
        {
            /* Only valid exit reason. */
            return VINF_SUCCESS;
        }

    } /* forever */

    /* Return to the main loop in vmR3EmulationThread, which will clean up for us. */
    longjmp(pUVM->vm.s.emtJumpEnv, 1);
}


/**
 * Gets the name of a halt method.
 *
 * @returns Pointer to a read only string.
 * @param   enmMethod   The method.
 */
static const char *vmR3GetHaltMethodName(VMHALTMETHOD enmMethod)
{
    switch (enmMethod)
    {
        case VMHALTMETHOD_BOOTSTRAP:    return "bootstrap";
        case VMHALTMETHOD_DEFAULT:      return "default";
        case VMHALTMETHOD_OLD:          return "old";
        case VMHALTMETHOD_1:            return "method1";
        //case VMHALTMETHOD_2:            return "method2";
        case VMHALTMETHOD_GLOBAL_1:     return "global1";
        default:                        return "unknown";
    }
}


/**
 * The old halt loop.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 */
static DECLCALLBACK(int) vmR3HaltOldDoHalt(PUVM pUVM, const uint32_t fMask, uint64_t /* u64Now*/)
{
    /*
     * Halt loop.
     */
    PVM pVM = pUVM->pVM;
    int rc = VINF_SUCCESS;
    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, true);
    //unsigned cLoops = 0;
    for (;;)
    {
        /*
         * Work the timers and check if we can exit.
         * The poll call gives us the ticks left to the next event in
         * addition to perhaps set an FF.
         */
        STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltPoll, a);
        PDMR3Poll(pVM);
        STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltPoll, a);
        STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltTimers, b);
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
                STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltYield, a);
                RTThreadYield(); /* this is the best we can do here */
                STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltYield, a);
            }
            else if (u64NanoTS < 2000000)
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep 1ms", u64NanoTS, cLoops++);
                STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pUVM->vm.s.EventSemWait, 1);
                STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltBlock, a);
            }
            else
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep %dms", u64NanoTS, cLoops++, (uint32_t)RT_MIN((u64NanoTS - 500000) / 1000000, 15));
                STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pUVM->vm.s.EventSemWait, RT_MIN((u64NanoTS - 1000000) / 1000000, 15));
                STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltBlock, a);
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
            ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }

    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, false);
    return rc;
}


/**
 * Initialize the configuration of halt method 1 & 2.
 *
 * @return VBox status code. Failure on invalid CFGM data.
 * @param   pVM     The VM handle.
 */
static int vmR3HaltMethod12ReadConfigU(PUVM pUVM)
{
    /*
     * The defaults.
     */
#if 1 /* DEBUGGING STUFF - REMOVE LATER */
    pUVM->vm.s.Halt.Method12.u32LagBlockIntervalDivisorCfg = 4;
    pUVM->vm.s.Halt.Method12.u32MinBlockIntervalCfg =   2*1000000;
    pUVM->vm.s.Halt.Method12.u32MaxBlockIntervalCfg =  75*1000000;
    pUVM->vm.s.Halt.Method12.u32StartSpinningCfg    =  30*1000000;
    pUVM->vm.s.Halt.Method12.u32StopSpinningCfg     =  20*1000000;
#else
    pUVM->vm.s.Halt.Method12.u32LagBlockIntervalDivisorCfg = 4;
    pUVM->vm.s.Halt.Method12.u32MinBlockIntervalCfg =   5*1000000;
    pUVM->vm.s.Halt.Method12.u32MaxBlockIntervalCfg = 200*1000000;
    pUVM->vm.s.Halt.Method12.u32StartSpinningCfg    =  20*1000000;
    pUVM->vm.s.Halt.Method12.u32StopSpinningCfg     =   2*1000000;
#endif

    /*
     * Query overrides.
     *
     * I don't have time to bother with niceities such as invalid value checks
     * here right now. sorry.
     */
    PCFGMNODE pCfg = CFGMR3GetChild(CFGMR3GetRoot(pUVM->pVM), "/VMM/HaltedMethod1");
    if (pCfg)
    {
        uint32_t u32;
        if (RT_SUCCESS(CFGMR3QueryU32(pCfg, "LagBlockIntervalDivisor", &u32)))
            pUVM->vm.s.Halt.Method12.u32LagBlockIntervalDivisorCfg = u32;
        if (RT_SUCCESS(CFGMR3QueryU32(pCfg, "MinBlockInterval", &u32)))
            pUVM->vm.s.Halt.Method12.u32MinBlockIntervalCfg = u32;
        if (RT_SUCCESS(CFGMR3QueryU32(pCfg, "MaxBlockInterval", &u32)))
            pUVM->vm.s.Halt.Method12.u32MaxBlockIntervalCfg = u32;
        if (RT_SUCCESS(CFGMR3QueryU32(pCfg, "StartSpinning", &u32)))
            pUVM->vm.s.Halt.Method12.u32StartSpinningCfg = u32;
        if (RT_SUCCESS(CFGMR3QueryU32(pCfg, "StopSpinning", &u32)))
            pUVM->vm.s.Halt.Method12.u32StopSpinningCfg = u32;
        LogRel(("HaltedMethod1 config: %d/%d/%d/%d/%d\n",
                pUVM->vm.s.Halt.Method12.u32LagBlockIntervalDivisorCfg,
                pUVM->vm.s.Halt.Method12.u32MinBlockIntervalCfg,
                pUVM->vm.s.Halt.Method12.u32MaxBlockIntervalCfg,
                pUVM->vm.s.Halt.Method12.u32StartSpinningCfg,
                pUVM->vm.s.Halt.Method12.u32StopSpinningCfg));
    }

    return VINF_SUCCESS;
}


/**
 * Initialize halt method 1.
 *
 * @return VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 */
static DECLCALLBACK(int) vmR3HaltMethod1Init(PUVM pUVM)
{
    return vmR3HaltMethod12ReadConfigU(pUVM);
}


/**
 * Method 1 - Block whenever possible, and when lagging behind
 * switch to spinning for 10-30ms with occational blocking until
 * the lag has been eliminated.
 */
static DECLCALLBACK(int) vmR3HaltMethod1Halt(PUVM pUVM, const uint32_t fMask, uint64_t u64Now)
{
    PVM pVM = pUVM->pVM;

    /*
     * To simplify things, we decide up-front whether we should switch to spinning or
     * not. This makes some ASSUMPTIONS about the cause of the spinning (PIT/RTC/PCNet)
     * and that it will generate interrupts or other events that will cause us to exit
     * the halt loop.
     */
    bool fBlockOnce = false;
    bool fSpinning = false;
    uint32_t u32CatchUpPct = TMVirtualSyncGetCatchUpPct(pVM);
    if (u32CatchUpPct /* non-zero if catching up */)
    {
        if (pUVM->vm.s.Halt.Method12.u64StartSpinTS)
        {
            fSpinning = TMVirtualSyncGetLag(pVM) >= pUVM->vm.s.Halt.Method12.u32StopSpinningCfg;
            if (fSpinning)
            {
                uint64_t u64Lag = TMVirtualSyncGetLag(pVM);
                fBlockOnce = u64Now - pUVM->vm.s.Halt.Method12.u64LastBlockTS
                           > RT_MAX(pUVM->vm.s.Halt.Method12.u32MinBlockIntervalCfg,
                                    RT_MIN(u64Lag / pUVM->vm.s.Halt.Method12.u32LagBlockIntervalDivisorCfg,
                                           pUVM->vm.s.Halt.Method12.u32MaxBlockIntervalCfg));
            }
            else
            {
                //RTLogRelPrintf("Stopped spinning (%u ms)\n", (u64Now - pUVM->vm.s.Halt.Method12.u64StartSpinTS) / 1000000);
                pUVM->vm.s.Halt.Method12.u64StartSpinTS = 0;
            }
        }
        else
        {
            fSpinning = TMVirtualSyncGetLag(pVM) >= pUVM->vm.s.Halt.Method12.u32StartSpinningCfg;
            if (fSpinning)
                pUVM->vm.s.Halt.Method12.u64StartSpinTS = u64Now;
        }
    }
    else if (pUVM->vm.s.Halt.Method12.u64StartSpinTS)
    {
        //RTLogRelPrintf("Stopped spinning (%u ms)\n", (u64Now - pUVM->vm.s.Halt.Method12.u64StartSpinTS) / 1000000);
        pUVM->vm.s.Halt.Method12.u64StartSpinTS = 0;
    }

    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, true);
    unsigned cLoops = 0;
    for (;; cLoops++)
    {
        /*
         * Work the timers and check if we can exit.
         */
        STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltPoll, a);
        PDMR3Poll(pVM);
        STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltPoll, a);
        STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltTimers, b);
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
#if 1 /* DEBUGGING STUFF - REMOVE LATER */
            &&  u64NanoTS >= 100000) /* 0.100 ms */
#else
            &&  u64NanoTS >= 250000) /* 0.250 ms */
#endif
        {
            const uint64_t Start = pUVM->vm.s.Halt.Method12.u64LastBlockTS = RTTimeNanoTS();
            VMMR3YieldStop(pVM);

            uint32_t cMilliSecs = RT_MIN(u64NanoTS / 1000000, 15);
            if (cMilliSecs <= pUVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg)
                cMilliSecs = 1;
            else
                cMilliSecs -= pUVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg;
            //RTLogRelPrintf("u64NanoTS=%RI64 cLoops=%3d sleep %02dms (%7RU64) ", u64NanoTS, cLoops, cMilliSecs, u64NanoTS);
            STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltBlock, a);
            rc = RTSemEventWait(pUVM->vm.s.EventSemWait, cMilliSecs);
            STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltBlock, a);
            if (rc == VERR_TIMEOUT)
                rc = VINF_SUCCESS;
            else if (VBOX_FAILURE(rc))
            {
                AssertRC(rc != VERR_INTERRUPTED);
                AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
                ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
                VM_FF_SET(pVM, VM_FF_TERMINATE);
                rc = VERR_INTERNAL_ERROR;
                break;
            }

            /*
             * Calc the statistics.
             * Update averages every 16th time, and flush parts of the history every 64th time.
             */
            const uint64_t Elapsed = RTTimeNanoTS() - Start;
            pUVM->vm.s.Halt.Method12.cNSBlocked += Elapsed;
            if (Elapsed > u64NanoTS)
                pUVM->vm.s.Halt.Method12.cNSBlockedTooLong += Elapsed - u64NanoTS;
            pUVM->vm.s.Halt.Method12.cBlocks++;
            if (!(pUVM->vm.s.Halt.Method12.cBlocks & 0xf))
            {
                pUVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg = pUVM->vm.s.Halt.Method12.cNSBlockedTooLong / pUVM->vm.s.Halt.Method12.cBlocks;
                if (!(pUVM->vm.s.Halt.Method12.cBlocks & 0x3f))
                {
                    pUVM->vm.s.Halt.Method12.cNSBlockedTooLong = pUVM->vm.s.Halt.Method12.cNSBlockedTooLongAvg * 0x40;
                    pUVM->vm.s.Halt.Method12.cBlocks = 0x40;
                }
            }
            //RTLogRelPrintf(" -> %7RU64 ns / %7RI64 ns delta%s\n", Elapsed, Elapsed - u64NanoTS, fBlockOnce ? " (block once)" : "");

            /*
             * Clear the block once flag if we actually blocked.
             */
            if (    fBlockOnce
                &&  Elapsed > 100000 /* 0.1 ms */)
                fBlockOnce = false;
        }
    }
    //if (fSpinning) RTLogRelPrintf("spun for %RU64 ns %u loops; lag=%RU64 pct=%d\n", RTTimeNanoTS() - u64Now, cLoops, TMVirtualSyncGetLag(pVM), u32CatchUpPct);

    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, false);
    return rc;
}


/**
 * Initialize the global 1 halt method.
 *
 * @return VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 */
static DECLCALLBACK(int) vmR3HaltGlobal1Init(PUVM pUVM)
{
    return VINF_SUCCESS;
}


/**
 * The global 1 halt method - Block in GMM (ring-0) and let it
 * try take care of the global scheduling of EMT threads.
 */
static DECLCALLBACK(int) vmR3HaltGlobal1Halt(PUVM pUVM, const uint32_t fMask, uint64_t u64Now)
{
    PVM pVM = pUVM->pVM;

    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, true);
    unsigned cLoops = 0;
    for (;; cLoops++)
    {
        /*
         * Work the timers and check if we can exit.
         */
        STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltPoll, a);
        PDMR3Poll(pVM);
        STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltPoll, a);
        STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltTimers, b);
        if (VM_FF_ISPENDING(pVM, fMask))
            break;

        /*
         * Estimate time left to the next event.
         */
        uint64_t u64Delta;
        uint64_t u64GipTime = TMTimerPollGIP(pVM, &u64Delta);
        if (VM_FF_ISPENDING(pVM, fMask))
            break;

        /*
         * Block if we're not spinning and the interval isn't all that small.
         */
        if (u64Delta > 50000 /* 0.050ms */)
        {
            VMMR3YieldStop(pVM);
            if (VM_FF_ISPENDING(pVM, fMask))
                break;

            //RTLogRelPrintf("u64NanoTS=%RI64 cLoops=%3d sleep %02dms (%7RU64) ", u64NanoTS, cLoops, cMilliSecs, u64NanoTS);
            STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltBlock, c);
            rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GVMM_SCHED_HALT, u64GipTime, NULL);
            STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltBlock, c);
            if (rc == VERR_INTERRUPTED)
                rc = VINF_SUCCESS;
            else if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("VMMR0_DO_GVMM_SCHED_HALT->%Vrc\n", rc));
                ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
                VM_FF_SET(pVM, VM_FF_TERMINATE);
                rc = VERR_INTERNAL_ERROR;
                break;
            }
        }
        /*
         * When spinning call upon the GVMM and do some wakups once
         * in a while, it's not like we're actually busy or anything.
         */
        else if (!(cLoops & 0x1fff))
        {
            STAM_REL_PROFILE_START(&pUVM->vm.s.StatHaltYield, d);
            rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GVMM_SCHED_POLL, false /* don't yield */, NULL);
            STAM_REL_PROFILE_STOP(&pUVM->vm.s.StatHaltYield, d);
        }
    }
    //if (fSpinning) RTLogRelPrintf("spun for %RU64 ns %u loops; lag=%RU64 pct=%d\n", RTTimeNanoTS() - u64Now, cLoops, TMVirtualSyncGetLag(pVM), u32CatchUpPct);

    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, false);
    return rc;
}


/**
 * The global 1 halt method - VMR3Wait() worker.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 */
static DECLCALLBACK(int) vmR3HaltGlobal1Wait(PUVM pUVM)
{
    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, true);

    PVM pVM = pUVM->pVM;
    int rc = VINF_SUCCESS;
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
        rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GVMM_SCHED_HALT, RTTimeNanoTS() + 1000000000 /* +1s */, NULL);
        if (rc == VERR_INTERRUPTED)
            rc = VINF_SUCCESS;
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
            ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }

    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, false);
    return rc;
}


/**
 * The global 1 halt method - VMR3NotifyFF() worker.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   fNotifiedREM    See VMR3NotifyFF().
 */
static DECLCALLBACK(void) vmR3HaltGlobal1NotifyFF(PUVM pUVM, bool fNotifiedREM)
{
    if (pUVM->vm.s.fWait)
    {
        int rc = SUPCallVMMR0Ex(pUVM->pVM->pVMR0, VMMR0_DO_GVMM_SCHED_WAKE_UP, 0, NULL);
        AssertRC(rc);
    }
    else if (!fNotifiedREM)
        REMR3NotifyFF(pUVM->pVM);
}


/**
 * Bootstrap VMR3Wait() worker.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 */
static DECLCALLBACK(int) vmR3BootstrapWait(PUVM pUVM)
{
    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, true);

    int rc = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Check Relevant FFs.
         */
        if (pUVM->vm.s.pReqs)
            break;
        if (    pUVM->pVM
            &&  VM_FF_ISPENDING(pUVM->pVM, VM_FF_EXTERNAL_SUSPENDED_MASK))
            break;
        if (pUVM->vm.s.fTerminateEMT)
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        rc = RTSemEventWait(pUVM->vm.s.EventSemWait, 1000);
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
            ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
            if (pUVM->pVM)
                VM_FF_SET(pUVM->pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }

    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, false);
    return rc;
}


/**
 * Bootstrap VMR3NotifyFF() worker.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   fNotifiedREM    See VMR3NotifyFF().
 */
static DECLCALLBACK(void) vmR3BootstrapNotifyFF(PUVM pUVM, bool fNotifiedREM)
{
    if (pUVM->vm.s.fWait)
    {
        int rc = RTSemEventSignal(pUVM->vm.s.EventSemWait);
        AssertRC(rc);
    }
}



/**
 * Default VMR3Wait() worker.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 */
static DECLCALLBACK(int) vmR3DefaultWait(PUVM pUVM)
{
    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, true);

    PVM pVM = pUVM->pVM;
    int rc = VINF_SUCCESS;
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
        rc = RTSemEventWait(pUVM->vm.s.EventSemWait, 1000);
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Vrc\n", rc));
            ASMAtomicUoWriteBool(&pUVM->vm.s.fTerminateEMT, true);
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }

    ASMAtomicUoWriteBool(&pUVM->vm.s.fWait, false);
    return rc;
}


/**
 * Default VMR3NotifyFF() worker.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   fNotifiedREM    See VMR3NotifyFF().
 */
static DECLCALLBACK(void) vmR3DefaultNotifyFF(PUVM pUVM, bool fNotifiedREM)
{
    if (pUVM->vm.s.fWait)
    {
        int rc = RTSemEventSignal(pUVM->vm.s.EventSemWait);
        AssertRC(rc);
    }
    else if (!fNotifiedREM)
        REMR3NotifyFF(pUVM->pVM);
}


/**
 * Array with halt method descriptors.
 * VMINT::iHaltMethod contains an index into this array.
 */
static const struct VMHALTMETHODDESC
{
    /** The halt method id. */
    VMHALTMETHOD enmHaltMethod;
    /** The init function for loading config and initialize variables. */
    DECLR3CALLBACKMEMBER(int,  pfnInit,(PUVM pUVM));
    /** The term function. */
    DECLR3CALLBACKMEMBER(void, pfnTerm,(PUVM pUVM));
    /** The halt function. */
    DECLR3CALLBACKMEMBER(int,  pfnHalt,(PUVM pUVM, const uint32_t fMask, uint64_t u64Now));
    /** The wait function. */
    DECLR3CALLBACKMEMBER(int,  pfnWait,(PUVM pUVM));
    /** The notifyFF function. */
    DECLR3CALLBACKMEMBER(void, pfnNotifyFF,(PUVM pUVM, bool fNotifiedREM));
} g_aHaltMethods[] =
{
    { VMHALTMETHOD_BOOTSTRAP, NULL,                 NULL,                   NULL,                   vmR3BootstrapWait,      vmR3BootstrapNotifyFF },
    { VMHALTMETHOD_OLD,     NULL,                   NULL,                   vmR3HaltOldDoHalt,      vmR3DefaultWait,        vmR3DefaultNotifyFF },
    { VMHALTMETHOD_1,       vmR3HaltMethod1Init,    NULL,                   vmR3HaltMethod1Halt,    vmR3DefaultWait,        vmR3DefaultNotifyFF },
  //{ VMHALTMETHOD_2,       vmR3HaltMethod2Init,    vmR3HaltMethod2Term,    vmR3HaltMethod2DoHalt,  vmR3HaltMethod2Wait,    vmR3HaltMethod2NotifyFF },
    { VMHALTMETHOD_GLOBAL_1,vmR3HaltGlobal1Init,    NULL,                   vmR3HaltGlobal1Halt,    vmR3HaltGlobal1Wait,    vmR3HaltGlobal1NotifyFF },
};


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
    PUVM pUVM = pVM->pUVM;
    g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnNotifyFF(pUVM, fNotifiedREM);
}


/**
 * Notify the emulation thread (EMT) about pending Forced Action (FF).
 *
 * This function is called by thread other than EMT to make
 * sure EMT wakes up and promptly service an FF request.
 *
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   fNotifiedREM    Set if REM have already been notified. If clear the
 *                          generic REMR3NotifyFF() method is called.
 */
VMR3DECL(void) VMR3NotifyFFU(PUVM pUVM, bool fNotifiedREM)
{
    LogFlow(("VMR3NotifyFF:\n"));
    g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnNotifyFF(pUVM, fNotifiedREM);
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
    PUVM pUVM = pVM->pUVM;
    uint64_t u64Now = RTTimeNanoTS();
    int64_t off = u64Now - pUVM->vm.s.u64HaltsStartTS;
    if (off > 1000000000)
    {
        if (off > _4G || !pUVM->vm.s.cHalts)
        {
            pUVM->vm.s.HaltInterval = 1000000000 /* 1 sec */;
            pUVM->vm.s.HaltFrequency = 1;
        }
        else
        {
            pUVM->vm.s.HaltInterval = (uint32_t)off / pUVM->vm.s.cHalts;
            pUVM->vm.s.HaltFrequency = ASMMultU64ByU32DivByU32(pUVM->vm.s.cHalts, 1000000000, (uint32_t)off);
        }
        pUVM->vm.s.u64HaltsStartTS = u64Now;
        pUVM->vm.s.cHalts = 0;
    }
    pUVM->vm.s.cHalts++;

    /*
     * Do the halt.
     */
    int rc = g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnHalt(pUVM, fMask, u64Now);

    /*
     * Resume the yielder.
     */
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
 * @param   pUVM            Pointer to the user mode VM structure.
 * @thread  The emulation thread.
 */
VMR3DECL(int) VMR3WaitU(PUVM pUVM)
{
    LogFlow(("VMR3WaitU:\n"));

    /*
     * Check Relevant FFs.
     */
    PVM pVM = pUVM->pVM;
    if (    pVM
        &&  VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_SUSPENDED_MASK))
    {
        LogFlow(("VMR3Wait: returns VINF_SUCCESS (FF %#x)\n", pVM->fForcedActions));
        return VINF_SUCCESS;
    }

    /*
     * Do waiting according to the halt method (so VMR3NotifyFF
     * doesn't have to special case anything).
     */
    int rc = g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnWait(pUVM);
    LogFlow(("VMR3WaitU: returns %Vrc (FF %#x)\n", rc, pVM ? pVM->fForcedActions : 0));
    return rc;
}


/**
 * Changes the halt method.
 *
 * @returns VBox status code.
 * @param   pUVM            Pointer to the user mode VM structure.
 * @param   enmHaltMethod   The new halt method.
 * @thread  EMT.
 */
int vmR3SetHaltMethodU(PUVM pUVM, VMHALTMETHOD enmHaltMethod)
{
    PVM pVM = pUVM->pVM; Assert(pVM);
    VM_ASSERT_EMT(pVM);
    AssertReturn(enmHaltMethod > VMHALTMETHOD_INVALID && enmHaltMethod < VMHALTMETHOD_END, VERR_INVALID_PARAMETER);

    /*
     * Resolve default (can be overridden in the configuration).
     */
    if (enmHaltMethod == VMHALTMETHOD_DEFAULT)
    {
        uint32_t u32;
        int rc = CFGMR3QueryU32(CFGMR3GetChild(CFGMR3GetRoot(pVM), "VM"), "HaltMethod", &u32);
        if (VBOX_SUCCESS(rc))
        {
            enmHaltMethod = (VMHALTMETHOD)u32;
            if (enmHaltMethod <= VMHALTMETHOD_INVALID || enmHaltMethod >= VMHALTMETHOD_END)
                return VMSetError(pVM, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("Invalid VM/HaltMethod value %d"), enmHaltMethod);
        }
        else if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_CHILD_NOT_FOUND)
            return VMSetError(pVM, rc, RT_SRC_POS, N_("Failed to Query VM/HaltMethod as uint32_t"));
        else
            enmHaltMethod = VMHALTMETHOD_GLOBAL_1;
            //enmHaltMethod = VMHALTMETHOD_1;
            //enmHaltMethod = VMHALTMETHOD_OLD;
    }
    LogRel(("VM: Halt method %s (%d)\n", vmR3GetHaltMethodName(enmHaltMethod), enmHaltMethod));

    /*
     * Find the descriptor.
     */
    unsigned i = 0;
    while (     i < RT_ELEMENTS(g_aHaltMethods)
           &&   g_aHaltMethods[i].enmHaltMethod != enmHaltMethod)
        i++;
    AssertReturn(i < RT_ELEMENTS(g_aHaltMethods), VERR_INVALID_PARAMETER);

    /*
     * Terminate the old one.
     */
    if (    pUVM->vm.s.enmHaltMethod != VMHALTMETHOD_INVALID
        &&  g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnTerm)
    {
        g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnTerm(pUVM);
        pUVM->vm.s.enmHaltMethod = VMHALTMETHOD_INVALID;
    }

    /*
     * Init the new one.
     */
    memset(&pUVM->vm.s.Halt, 0, sizeof(pUVM->vm.s.Halt));
    if (g_aHaltMethods[i].pfnInit)
    {
        int rc = g_aHaltMethods[i].pfnInit(pUVM);
        AssertRCReturn(rc, rc);
    }
    pUVM->vm.s.enmHaltMethod = enmHaltMethod;

    ASMAtomicWriteU32(&pUVM->vm.s.iHaltMethod, i);
    return VINF_SUCCESS;
}

