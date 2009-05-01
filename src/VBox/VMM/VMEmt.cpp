/* $Id$ */
/** @file
 * VM - Virtual Machine, The Emulation Thread.
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
#define LOG_GROUP LOG_GROUP_VM
#include <VBox/tm.h>
#include <VBox/dbgf.h>
#include <VBox/em.h>
#include <VBox/pdmapi.h>
#include <VBox/rem.h>
#include <VBox/tm.h>
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
    PUVMCPU pUVCpu = (PUVMCPU)pvArgs;
    PUVM    pUVM    = pUVCpu->pUVM;
    RTCPUID idCpu   = pUVCpu->idCpu;
    int     rc;

    AssertReleaseMsg(VALID_PTR(pUVM) && pUVM->u32Magic == UVM_MAGIC,
                     ("Invalid arguments to the emulation thread!\n"));

    rc = RTTlsSet(pUVM->vm.s.idxTLS, pUVCpu);
    AssertReleaseMsgRCReturn(rc, ("RTTlsSet %x failed with %Rrc\n", pUVM->vm.s.idxTLS, rc), rc);

    /*
     * The request loop.
     */
    rc = VINF_SUCCESS;
    volatile VMSTATE enmBefore = VMSTATE_CREATING; /* volatile because of setjmp */
    Log(("vmR3EmulationThread: Emulation thread starting the days work... Thread=%#x pUVM=%p\n", ThreadSelf, pUVM));
    for (;;)
    {
        /* Requested to exit the EMT thread out of sync? (currently only VMR3WaitForResume) */
        if (setjmp(pUVCpu->vm.s.emtJumpEnv) != 0)
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

            if (    pUVM->vm.s.pReqs
                &&  pUVCpu->idCpu == 0 /* Only the first VCPU may initialize the VM during early init */)
            {
                /*
                 * Service execute in EMT request.
                 */
                rc = VMR3ReqProcessU(pUVM, VMREQDEST_ANY);
                Log(("vmR3EmulationThread: Req rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pUVM->pVM ? pUVM->pVM->enmVMState : VMSTATE_CREATING));
            }
            else
            if (pUVCpu->vm.s.pReqs)
            {
                /*
                 * Service execute in EMT request.
                 */
                rc = VMR3ReqProcessU(pUVM, (VMREQDEST)pUVCpu->idCpu);
                Log(("vmR3EmulationThread: Req (cpu=%d) rc=%Rrc, VM state %d -> %d\n", pUVCpu->idCpu, rc, enmBefore, pUVM->pVM ? pUVM->pVM->enmVMState : VMSTATE_CREATING));
            }
            else
            {
                /*
                 * Nothing important is pending, so wait for something.
                 */
                rc = VMR3WaitU(pUVCpu);
                if (RT_FAILURE(rc))
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
                rc = VMR3ReqProcessU(pUVM, VMREQDEST_ANY);
                Log(("vmR3EmulationThread: Req rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
            }
            else if (pUVCpu->vm.s.pReqs)
            {
                /*
                 * Service execute in EMT request.
                 */
                rc = VMR3ReqProcessU(pUVM, (VMREQDEST)pUVCpu->idCpu);
                Log(("vmR3EmulationThread: Req (cpu=%d)rc=%Rrc, VM state %d -> %d\n", pUVCpu->idCpu, rc, enmBefore, pVM->enmVMState));
            }
            else if (VM_FF_ISSET(pVM, VM_FF_DBGF))
            {
                /*
                 * Service the debugger request.
                 */
                rc = DBGFR3VMMForcedAction(pVM);
                Log(("vmR3EmulationThread: Dbg rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
            }
            else if (VM_FF_ISSET(pVM, VM_FF_RESET))
            {
                /*
                 * Service a delayed reset request.
                 */
                rc = VMR3Reset(pVM);
                VM_FF_CLEAR(pVM, VM_FF_RESET);
                Log(("vmR3EmulationThread: Reset rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
            }
            else
            {
                /*
                 * Nothing important is pending, so wait for something.
                 */
                rc = VMR3WaitU(pUVCpu);
                if (RT_FAILURE(rc))
                    break;
            }

            /*
             * Check for termination requests, these have extremely high priority.
             */
            if (    rc == VINF_EM_TERMINATE
                ||  VM_FF_ISSET(pVM, VM_FF_TERMINATE)
                ||  pUVM->vm.s.fTerminateEMT)
                break;
        }

        /*
         * Some requests (both VMR3Req* and the DBGF) can potentially
         * resume or start the VM, in that case we'll get a change in
         * VM status indicating that we're now running.
         */
        if (    RT_SUCCESS(rc)
            &&  pUVM->pVM
            &&  enmBefore != pUVM->pVM->enmVMState
            &&  pUVM->pVM->enmVMState == VMSTATE_RUNNING)
        {
            PVM    pVM   = pUVM->pVM;
            PVMCPU pVCpu = &pVM->aCpus[idCpu];

            rc = EMR3ExecuteVM(pVM, pVCpu);
            Log(("vmR3EmulationThread: EMR3ExecuteVM() -> rc=%Rrc, enmVMState=%d\n", rc, pVM->enmVMState));
            if (   EMGetState(pVCpu) == EMSTATE_GURU_MEDITATION
                && pVM->enmVMState == VMSTATE_RUNNING)
                vmR3SetState(pVM, VMSTATE_GURU_MEDITATION);
        }

    } /* forever */


    /*
     * Exiting.
     */
    Log(("vmR3EmulationThread: Terminating emulation thread! Thread=%#x pUVM=%p rc=%Rrc enmBefore=%d enmVMState=%d\n",
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

        pUVCpu->vm.s.NativeThreadEMT = NIL_RTNATIVETHREAD;
    }
    Log(("vmR3EmulationThread: EMT is terminated.\n"));
    return rc;
}


#if 0 /* not used */
/**
 * Wait for VM to be resumed. Handle events like vmR3EmulationThread does.
 * In case the VM is stopped, clean up and long jump to the main EMT loop.
 *
 * @returns VINF_SUCCESS or doesn't return
 * @param   pVM             VM handle.
 */
VMMR3DECL(int) VMR3WaitForResume(PVM pVM)
{
    /*
     * The request loop.
     */
    PUVMCPU pUVCpu;
    PUVM    pUVM = pVM->pUVM;
    VMSTATE enmBefore;
    int     rc;

    pUVCpu = (PUVMCPU)RTTlsGet(pUVM->vm.s.idxTLS);
    AssertReturn(pUVCpu, VERR_INTERNAL_ERROR);

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
            rc = VMR3ReqProcessU(pUVM, VMREQDEST_ANY);
            Log(("vmR3EmulationThread: Req rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else if (pUVCpu->vm.s.pReqs)
        {
            /*
             * Service execute in EMT request.
             */
            rc = VMR3ReqProcessU(pUVM, (VMREQDEST)pUVCpu->idCpu);
            Log(("vmR3EmulationThread: Req (cpu=%d)rc=%Rrc, VM state %d -> %d\n", pUVCpu->idCpu, rc, enmBefore, pVM->enmVMState));
        }
        else if (VM_FF_ISSET(pVM, VM_FF_DBGF))
        {
            /*
             * Service the debugger request.
             */
            rc = DBGFR3VMMForcedAction(pVM);
            Log(("vmR3EmulationThread: Dbg rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else if (VM_FF_ISSET(pVM, VM_FF_RESET))
        {
            /*
             * Service a delay reset request.
             */
            rc = VMR3Reset(pVM);
            VM_FF_CLEAR(pVM, VM_FF_RESET);
            Log(("vmR3EmulationThread: Reset rc=%Rrc, VM state %d -> %d\n", rc, enmBefore, pVM->enmVMState));
        }
        else
        {
            /*
             * Nothing important is pending, so wait for something.
             */
            rc = VMR3WaitU(pUVCpu);
            if (RT_FAILURE(rc))
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
        if (    RT_SUCCESS(rc)
            &&  enmBefore != pVM->enmVMState
            &&  pVM->enmVMState == VMSTATE_RUNNING)
        {
            /* Only valid exit reason. */
            return VINF_SUCCESS;
        }

    } /* forever */

    /* Return to the main loop in vmR3EmulationThread, which will clean up for us. */
    longjmp(pUVCpu->vm.s.emtJumpEnv, 1);
}
#endif

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
 */
static DECLCALLBACK(int) vmR3HaltOldDoHalt(PUVMCPU pUVCpu, const uint32_t fMask, uint64_t /* u64Now*/)
{
    /*
     * Halt loop.
     */
    PVM    pVM   = pUVCpu->pVM;
    PVMCPU pVCpu = pUVCpu->pVCpu;

    int rc = VINF_SUCCESS;
    ASMAtomicWriteBool(&pUVCpu->vm.s.fWait, true);
    //unsigned cLoops = 0;
    for (;;)
    {
        /*
         * Work the timers and check if we can exit.
         * The poll call gives us the ticks left to the next event in
         * addition to perhaps set an FF.
         */
        STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltTimers, b);
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
            break;
        uint64_t u64NanoTS = TMVirtualToNano(pVM, TMTimerPoll(pVM));
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
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
                STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltYield, a);
                RTThreadYield(); /* this is the best we can do here */
                STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltYield, a);
            }
            else if (u64NanoTS < 2000000)
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep 1ms", u64NanoTS, cLoops++);
                STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pUVCpu->vm.s.EventSemWait, 1);
                STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltBlock, a);
            }
            else
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep %dms", u64NanoTS, cLoops++, (uint32_t)RT_MIN((u64NanoTS - 500000) / 1000000, 15));
                STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pUVCpu->vm.s.EventSemWait, RT_MIN((u64NanoTS - 1000000) / 1000000, 15));
                STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltBlock, a);
            }
            //uint64_t u64Slept = RTTimeNanoTS() - u64Start;
            //RTLogPrintf(" -> rc=%Rrc in %RU64 ns / %RI64 ns delta\n", rc, u64Slept, u64NanoTS - u64Slept);
        }
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (RT_FAILURE(rc))
        {
            AssertRC(rc != VERR_INTERRUPTED);
            AssertMsgFailed(("RTSemEventWait->%Rrc\n", rc));
            ASMAtomicUoWriteBool(&pUVCpu->vm.s.fTerminateEMT, true);
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }

    ASMAtomicUoWriteBool(&pUVCpu->vm.s.fWait, false);
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
static DECLCALLBACK(int) vmR3HaltMethod1Halt(PUVMCPU pUVCpu, const uint32_t fMask, uint64_t u64Now)
{
    PUVM    pUVM    = pUVCpu->pUVM;
    PVMCPU  pVCpu   = pUVCpu->pVCpu;
    PVM     pVM     = pUVCpu->pVM;

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
        if (pUVCpu->vm.s.Halt.Method12.u64StartSpinTS)
        {
            fSpinning = TMVirtualSyncGetLag(pVM) >= pUVM->vm.s.Halt.Method12.u32StopSpinningCfg;
            if (fSpinning)
            {
                uint64_t u64Lag = TMVirtualSyncGetLag(pVM);
                fBlockOnce = u64Now - pUVCpu->vm.s.Halt.Method12.u64LastBlockTS
                           > RT_MAX(pUVM->vm.s.Halt.Method12.u32MinBlockIntervalCfg,
                                    RT_MIN(u64Lag / pUVM->vm.s.Halt.Method12.u32LagBlockIntervalDivisorCfg,
                                           pUVM->vm.s.Halt.Method12.u32MaxBlockIntervalCfg));
            }
            else
            {
                //RTLogRelPrintf("Stopped spinning (%u ms)\n", (u64Now - pUVCpu->vm.s.Halt.Method12.u64StartSpinTS) / 1000000);
                pUVCpu->vm.s.Halt.Method12.u64StartSpinTS = 0;
            }
        }
        else
        {
            fSpinning = TMVirtualSyncGetLag(pVM) >= pUVM->vm.s.Halt.Method12.u32StartSpinningCfg;
            if (fSpinning)
                pUVCpu->vm.s.Halt.Method12.u64StartSpinTS = u64Now;
        }
    }
    else if (pUVCpu->vm.s.Halt.Method12.u64StartSpinTS)
    {
        //RTLogRelPrintf("Stopped spinning (%u ms)\n", (u64Now - pUVCpu->vm.s.Halt.Method12.u64StartSpinTS) / 1000000);
        pUVCpu->vm.s.Halt.Method12.u64StartSpinTS = 0;
    }

    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicWriteBool(&pUVCpu->vm.s.fWait, true);
    unsigned cLoops = 0;
    for (;; cLoops++)
    {
        /*
         * Work the timers and check if we can exit.
         */
        STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltTimers, b);
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
            break;

        /*
         * Estimate time left to the next event.
         */
        uint64_t u64NanoTS = TMVirtualToNano(pVM, TMTimerPoll(pVM));
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
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
            const uint64_t Start = pUVCpu->vm.s.Halt.Method12.u64LastBlockTS = RTTimeNanoTS();
            VMMR3YieldStop(pVM);

            uint32_t cMilliSecs = RT_MIN(u64NanoTS / 1000000, 15);
            if (cMilliSecs <= pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLongAvg)
                cMilliSecs = 1;
            else
                cMilliSecs -= pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLongAvg;
            //RTLogRelPrintf("u64NanoTS=%RI64 cLoops=%3d sleep %02dms (%7RU64) ", u64NanoTS, cLoops, cMilliSecs, u64NanoTS);
            STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltBlock, a);
            rc = RTSemEventWait(pUVCpu->vm.s.EventSemWait, cMilliSecs);
            STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltBlock, a);
            if (rc == VERR_TIMEOUT)
                rc = VINF_SUCCESS;
            else if (RT_FAILURE(rc))
            {
                AssertRC(rc != VERR_INTERRUPTED);
                AssertMsgFailed(("RTSemEventWait->%Rrc\n", rc));
                ASMAtomicUoWriteBool(&pUVCpu->vm.s.fTerminateEMT, true);
                VM_FF_SET(pVM, VM_FF_TERMINATE);
                rc = VERR_INTERNAL_ERROR;
                break;
            }

            /*
             * Calc the statistics.
             * Update averages every 16th time, and flush parts of the history every 64th time.
             */
            const uint64_t Elapsed = RTTimeNanoTS() - Start;
            pUVCpu->vm.s.Halt.Method12.cNSBlocked += Elapsed;
            if (Elapsed > u64NanoTS)
                pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLong += Elapsed - u64NanoTS;
            pUVCpu->vm.s.Halt.Method12.cBlocks++;
            if (!(pUVCpu->vm.s.Halt.Method12.cBlocks & 0xf))
            {
                pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLongAvg = pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLong / pUVCpu->vm.s.Halt.Method12.cBlocks;
                if (!(pUVCpu->vm.s.Halt.Method12.cBlocks & 0x3f))
                {
                    pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLong = pUVCpu->vm.s.Halt.Method12.cNSBlockedTooLongAvg * 0x40;
                    pUVCpu->vm.s.Halt.Method12.cBlocks = 0x40;
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

    ASMAtomicUoWriteBool(&pUVCpu->vm.s.fWait, false);
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
static DECLCALLBACK(int) vmR3HaltGlobal1Halt(PUVMCPU pUVCpu, const uint32_t fMask, uint64_t u64Now)
{
    PUVM    pUVM  = pUVCpu->pUVM;
    PVMCPU  pVCpu = pUVCpu->pVCpu;
    PVM     pVM   = pUVCpu->pVM;

    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicWriteBool(&pUVCpu->vm.s.fWait, true);
    unsigned cLoops = 0;
    for (;; cLoops++)
    {
        /*
         * Work the timers and check if we can exit.
         */
        STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltTimers, b);
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
            break;

        /*
         * Estimate time left to the next event.
         */
        uint64_t u64Delta;
        uint64_t u64GipTime = TMTimerPollGIP(pVM, &u64Delta);
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
            break;

        /*
         * Block if we're not spinning and the interval isn't all that small.
         */
        if (u64Delta > 50000 /* 0.050ms */)
        {
            VMMR3YieldStop(pVM);
            if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
                ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
                    break;

            //RTLogRelPrintf("u64NanoTS=%RI64 cLoops=%3d sleep %02dms (%7RU64) ", u64NanoTS, cLoops, cMilliSecs, u64NanoTS);
            STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltBlock, c);
            rc = SUPCallVMMR0Ex(pVM->pVMR0, pVCpu->idCpu, VMMR0_DO_GVMM_SCHED_HALT, u64GipTime, NULL);
            STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltBlock, c);
            if (rc == VERR_INTERRUPTED)
                rc = VINF_SUCCESS;
            else if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("VMMR0_DO_GVMM_SCHED_HALT->%Rrc\n", rc));
                ASMAtomicUoWriteBool(&pUVCpu->vm.s.fTerminateEMT, true);
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
            STAM_REL_PROFILE_START(&pUVCpu->vm.s.StatHaltYield, d);
            rc = SUPCallVMMR0Ex(pVM->pVMR0, pVCpu->idCpu, VMMR0_DO_GVMM_SCHED_POLL, false /* don't yield */, NULL);
            STAM_REL_PROFILE_STOP(&pUVCpu->vm.s.StatHaltYield, d);
        }
    }
    //if (fSpinning) RTLogRelPrintf("spun for %RU64 ns %u loops; lag=%RU64 pct=%d\n", RTTimeNanoTS() - u64Now, cLoops, TMVirtualSyncGetLag(pVM), u32CatchUpPct);

    ASMAtomicUoWriteBool(&pUVCpu->vm.s.fWait, false);
    return rc;
}


/**
 * The global 1 halt method - VMR3Wait() worker.
 *
 * @returns VBox status code.
 * @param   pUVCpu            Pointer to the user mode VMCPU structure.
 */
static DECLCALLBACK(int) vmR3HaltGlobal1Wait(PUVMCPU pUVCpu)
{
    ASMAtomicWriteBool(&pUVCpu->vm.s.fWait, true);

    PVM    pVM   = pUVCpu->pUVM->pVM;
    PVMCPU pVCpu = VMMGetCpu(pVM);

    int rc = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Check Relevant FFs.
         */
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_SUSPENDED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_EXTERNAL_SUSPENDED_MASK))
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        rc = SUPCallVMMR0Ex(pVM->pVMR0, pVCpu->idCpu, VMMR0_DO_GVMM_SCHED_HALT, RTTimeNanoTS() + 1000000000 /* +1s */, NULL);
        if (rc == VERR_INTERRUPTED)
            rc = VINF_SUCCESS;
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Rrc\n", rc));
            ASMAtomicUoWriteBool(&pUVCpu->vm.s.fTerminateEMT, true);
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }

    ASMAtomicUoWriteBool(&pUVCpu->vm.s.fWait, false);
    return rc;
}


/**
 * The global 1 halt method - VMR3NotifyFF() worker.
 *
 * @param   pUVCpu         Pointer to the user mode VMCPU structure.
 * @param   fNotifiedREM    See VMR3NotifyFF().
 */
static DECLCALLBACK(void) vmR3HaltGlobal1NotifyFF(PUVMCPU pUVCpu, bool fNotifiedREM)
{
    if (pUVCpu->vm.s.fWait)
    {
        int rc = SUPCallVMMR0Ex(pUVCpu->pVM->pVMR0, pUVCpu->idCpu, VMMR0_DO_GVMM_SCHED_WAKE_UP, 0, NULL);
        AssertRC(rc);
    }
    else if (!fNotifiedREM)
        REMR3NotifyFF(pUVCpu->pVM);
}


/**
 * Bootstrap VMR3Wait() worker.
 *
 * @returns VBox status code.
 * @param   pUVMCPU            Pointer to the user mode VMCPU structure.
 */
static DECLCALLBACK(int) vmR3BootstrapWait(PUVMCPU pUVCpu)
{
    PUVM pUVM = pUVCpu->pUVM;

    ASMAtomicWriteBool(&pUVCpu->vm.s.fWait, true);

    int rc = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Check Relevant FFs.
         */
        if (pUVM->vm.s.pReqs)   /* global requests pending? */
            break;
        if (pUVCpu->vm.s.pReqs) /* local requests pending? */
            break;

        if (    pUVCpu->pVM
            &&  (   VM_FF_ISPENDING(pUVCpu->pVM, VM_FF_EXTERNAL_SUSPENDED_MASK)
                 || VMCPU_FF_ISPENDING(VMMGetCpu(pUVCpu->pVM), VMCPU_FF_EXTERNAL_SUSPENDED_MASK)
                )
            )
            break;
        if (pUVCpu->vm.s.fTerminateEMT)
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        rc = RTSemEventWait(pUVCpu->vm.s.EventSemWait, 1000);
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Rrc\n", rc));
            ASMAtomicUoWriteBool(&pUVCpu->vm.s.fTerminateEMT, true);
            if (pUVCpu->pVM)
                VM_FF_SET(pUVCpu->pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }

    ASMAtomicUoWriteBool(&pUVCpu->vm.s.fWait, false);
    return rc;
}


/**
 * Bootstrap VMR3NotifyFF() worker.
 *
 * @param   pUVCpu         Pointer to the user mode VMCPU structure.
 * @param   fNotifiedREM    See VMR3NotifyFF().
 */
static DECLCALLBACK(void) vmR3BootstrapNotifyFF(PUVMCPU pUVCpu, bool fNotifiedREM)
{
    if (pUVCpu->vm.s.fWait)
    {
        int rc = RTSemEventSignal(pUVCpu->vm.s.EventSemWait);
        AssertRC(rc);
    }
}


/**
 * Default VMR3Wait() worker.
 *
 * @returns VBox status code.
 * @param   pUVMCPU            Pointer to the user mode VMCPU structure.
 */
static DECLCALLBACK(int) vmR3DefaultWait(PUVMCPU pUVCpu)
{
    ASMAtomicWriteBool(&pUVCpu->vm.s.fWait, true);

    PVM    pVM   = pUVCpu->pVM;
    PVMCPU pVCpu = pUVCpu->pVCpu;
    int    rc    = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Check Relevant FFs.
         */
        if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_SUSPENDED_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_EXTERNAL_SUSPENDED_MASK))
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        rc = RTSemEventWait(pUVCpu->vm.s.EventSemWait, 1000);
        if (rc == VERR_TIMEOUT)
            rc = VINF_SUCCESS;
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("RTSemEventWait->%Rrc\n", rc));
            ASMAtomicUoWriteBool(&pUVCpu->vm.s.fTerminateEMT, true);
            VM_FF_SET(pVM, VM_FF_TERMINATE);
            rc = VERR_INTERNAL_ERROR;
            break;
        }

    }

    ASMAtomicUoWriteBool(&pUVCpu->vm.s.fWait, false);
    return rc;
}


/**
 * Default VMR3NotifyFF() worker.
 *
 * @param   pUVCpu         Pointer to the user mode VMCPU structure.
 * @param   fNotifiedREM    See VMR3NotifyFF().
 */
static DECLCALLBACK(void) vmR3DefaultNotifyFF(PUVMCPU pUVCpu, bool fNotifiedREM)
{
    if (pUVCpu->vm.s.fWait)
    {
        int rc = RTSemEventSignal(pUVCpu->vm.s.EventSemWait);
        AssertRC(rc);
    }
    else if (!fNotifiedREM)
        REMR3NotifyFF(pUVCpu->pVM);
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
    DECLR3CALLBACKMEMBER(int,  pfnHalt,(PUVMCPU pUVCpu, const uint32_t fMask, uint64_t u64Now));
    /** The wait function. */
    DECLR3CALLBACKMEMBER(int,  pfnWait,(PUVMCPU pUVCpu));
    /** The notifyFF function. */
    DECLR3CALLBACKMEMBER(void, pfnNotifyFF,(PUVMCPU pUVCpu, bool fNotifiedREM));
} g_aHaltMethods[] =
{
    { VMHALTMETHOD_BOOTSTRAP, NULL,                 NULL,                   NULL,                   vmR3BootstrapWait,      vmR3BootstrapNotifyFF },
    { VMHALTMETHOD_OLD,     NULL,                   NULL,                   vmR3HaltOldDoHalt,      vmR3DefaultWait,        vmR3DefaultNotifyFF },
    { VMHALTMETHOD_1,       vmR3HaltMethod1Init,    NULL,                   vmR3HaltMethod1Halt,    vmR3DefaultWait,        vmR3DefaultNotifyFF },
    { VMHALTMETHOD_GLOBAL_1,vmR3HaltGlobal1Init,    NULL,                   vmR3HaltGlobal1Halt,    vmR3HaltGlobal1Wait,    vmR3HaltGlobal1NotifyFF },
};


/**
 * Notify the emulation thread (EMT) about pending Forced Action (FF).
 *
 * This function is called by thread other than EMT to make
 * sure EMT wakes up and promptly service an FF request.
 *
 * @param   pVM             VM handle.
 * @param   pVCpu           VMCPU handle (NULL if all/global notification)
 * @param   fNotifiedREM    Set if REM have already been notified. If clear the
 *                          generic REMR3NotifyFF() method is called.
 */
VMMR3DECL(void) VMR3NotifyGlobalFF(PVM pVM, bool fNotifiedREM)
{
    PUVM pUVM = pVM->pUVM;

    LogFlow(("VMR3NotifyFF:\n"));
    /** @todo might want to have a 2nd look at this (SMP) */
    for (unsigned iCpu=0;iCpu<pVM->cCPUs;iCpu++)
    {
        PUVMCPU pUVCpu = pVM->aCpus[iCpu].pUVCpu;
        g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnNotifyFF(pUVCpu, fNotifiedREM);
    }
}

/**
 * Notify the emulation thread (EMT) about pending Forced Action (FF).
 *
 * This function is called by thread other than EMT to make
 * sure EMT wakes up and promptly service an FF request.
 *
 * @param   pVM             VM handle.
 * @param   pVCpu           VMCPU handle (NULL if all/global notification)
 * @param   fNotifiedREM    Set if REM have already been notified. If clear the
 *                          generic REMR3NotifyFF() method is called.
 */
VMMR3DECL(void) VMR3NotifyCpuFF(PVMCPU pVCpu, bool fNotifiedREM)
{
    PUVMCPU pUVCpu = pVCpu->pUVCpu;
    PUVM    pUVM   = pUVCpu->pUVM;

    LogFlow(("VMR3NotifyCpuFF:\n"));
    g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnNotifyFF(pUVCpu, fNotifiedREM);
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
VMMR3DECL(void) VMR3NotifyGlobalFFU(PUVM pUVM, bool fNotifiedREM)
{
    LogFlow(("VMR3NotifyGlobalFFU:\n"));
    /** @todo might want to have a 2nd look at this (SMP) */
    for (unsigned iCpu = 0; iCpu < pUVM->cCpus; iCpu++)
    {
        PUVMCPU pUVCpu = &pUVM->aCpus[iCpu];
        g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnNotifyFF(pUVCpu, fNotifiedREM);
    }
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
VMMR3DECL(void) VMR3NotifyCpuFFU(PUVMCPU pUVCpu, bool fNotifiedREM)
{
    PUVM pUVM = pUVCpu->pUVM;

    LogFlow(("VMR3NotifyCpuFFU:\n"));
    g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnNotifyFF(pUVCpu, fNotifiedREM);
}


/**
 * Halted VM Wait.
 * Any external event will unblock the thread.
 *
 * @returns VINF_SUCCESS unless a fatal error occured. In the latter
 *          case an appropriate status code is returned.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   fIgnoreInterrupts   If set the VM_FF_INTERRUPT flags is ignored.
 * @thread  The emulation thread.
 */
VMMR3DECL(int) VMR3WaitHalted(PVM pVM, PVMCPU pVCpu, bool fIgnoreInterrupts)
{
    LogFlow(("VMR3WaitHalted: fIgnoreInterrupts=%d\n", fIgnoreInterrupts));

    /*
     * Check Relevant FFs.
     */
    const uint32_t fMask = !fIgnoreInterrupts
        ? VMCPU_FF_EXTERNAL_HALTED_MASK
        : VMCPU_FF_EXTERNAL_HALTED_MASK & ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC);
    if (    VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_HALTED_MASK)
        ||  VMCPU_FF_ISPENDING(pVCpu, fMask))
    {
        LogFlow(("VMR3WaitHalted: returns VINF_SUCCESS (FF %#x FFCPU %#x)\n", pVM->fGlobalForcedActions, pVCpu->fLocalForcedActions));
        return VINF_SUCCESS;
    }

    /*
     * The yielder is suspended while we're halting, while TM might have clock(s) running
     * only at certain times and need to be notified..
     */
    VMMR3YieldSuspend(pVM);
    TMNotifyStartOfHalt(pVCpu);

    /*
     * Record halt averages for the last second.
     */
    PUVMCPU pUVCpu = pVCpu->pUVCpu;
    uint64_t u64Now = RTTimeNanoTS();
    int64_t off = u64Now - pUVCpu->vm.s.u64HaltsStartTS;
    if (off > 1000000000)
    {
        if (off > _4G || !pUVCpu->vm.s.cHalts)
        {
            pUVCpu->vm.s.HaltInterval = 1000000000 /* 1 sec */;
            pUVCpu->vm.s.HaltFrequency = 1;
        }
        else
        {
            pUVCpu->vm.s.HaltInterval = (uint32_t)off / pUVCpu->vm.s.cHalts;
            pUVCpu->vm.s.HaltFrequency = ASMMultU64ByU32DivByU32(pUVCpu->vm.s.cHalts, 1000000000, (uint32_t)off);
        }
        pUVCpu->vm.s.u64HaltsStartTS = u64Now;
        pUVCpu->vm.s.cHalts = 0;
    }
    pUVCpu->vm.s.cHalts++;

    /*
     * Do the halt.
     */
    PUVM pUVM = pUVCpu->pUVM;
    int rc = g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnHalt(pUVCpu, fMask, u64Now);

    /*
     * Notify TM and resume the yielder
     */
    TMNotifyEndOfHalt(pVCpu);
    VMMR3YieldResume(pVM);

    LogFlow(("VMR3WaitHalted: returns %Rrc (FF %#x)\n", rc, pVM->fGlobalForcedActions));
    return rc;
}


/**
 * Suspended VM Wait.
 * Only a handful of forced actions will cause the function to
 * return to the caller.
 *
 * @returns VINF_SUCCESS unless a fatal error occured. In the latter
 *          case an appropriate status code is returned.
 * @param   pUVCpu          Pointer to the user mode VMCPU structure.
 * @thread  The emulation thread.
 */
VMMR3DECL(int) VMR3WaitU(PUVMCPU pUVCpu)
{
    LogFlow(("VMR3WaitU:\n"));

    /*
     * Check Relevant FFs.
     */
    PVM    pVM   = pUVCpu->pVM;
    PVMCPU pVCpu = pUVCpu->pVCpu;

    if (    pVM
        &&  (   VM_FF_ISPENDING(pVM, VM_FF_EXTERNAL_SUSPENDED_MASK)
             || VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_EXTERNAL_SUSPENDED_MASK)
            )
        )
    {
        LogFlow(("VMR3Wait: returns VINF_SUCCESS (FF %#x)\n", pVM->fGlobalForcedActions));
        return VINF_SUCCESS;
    }

    /*
     * Do waiting according to the halt method (so VMR3NotifyFF
     * doesn't have to special case anything).
     */
    PUVM pUVM = pUVCpu->pUVM;
    int rc = g_aHaltMethods[pUVM->vm.s.iHaltMethod].pfnWait(pUVCpu);
    LogFlow(("VMR3WaitU: returns %Rrc (FF %#x)\n", rc, pVM ? pVM->fGlobalForcedActions : 0));
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
        if (RT_SUCCESS(rc))
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

