/** @file
 *
 * VM - Virtual Machine, The Emulation Thread.
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
         * VM status indicating that we're no running.
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
     * Exitting.
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
     * The CPU TSC is running while halted,
     * and the yielder is suspended.
     */
    TMCpuTickResume(pVM);
    VMMR3YieldSuspend(pVM);

    /*
     * Halt loop.
     */
    int rc = VINF_SUCCESS;
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 1);
    //unsigned cLoops = 0;
    for (;;)
    {
#ifdef VBOX_HIGH_RES_TIMERS_HACK
        /*
         * Work the timers and check if we can exit.
         * The poll call gives us the ticks left to the next event in
         * addition to perhaps set an FF.
         */
        STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltPoll, a);
        PDMR3Poll(pVM);
        STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltPoll, a);
        STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltTimers, b);
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
            if (u64NanoTS <  870000) /* this is a bit speculative... works fine on linux. */
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d yield\n", u64NanoTS, cLoops++);
                STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltYield, a);
                RTThreadYield(); /* this is the best we can do here */
                STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltYield, a);
            }
            else if (u64NanoTS < 2000000)
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep 1ms\n", u64NanoTS, cLoops++);
                STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pVM->vm.s.EventSemWait, 1);
                STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltBlock, a);
            }
            else
            {
                //RTLogPrintf("u64NanoTS=%RI64 cLoops=%d sleep %dms\n", u64NanoTS, cLoops++, (uint32_t)RT_MIN(u64NanoTS / 1000000, 15));
                STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltBlock, a);
                rc = RTSemEventWait(pVM->vm.s.EventSemWait, RT_MIN(u64NanoTS / 1000000, 15));
                STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltBlock, a);
            }
        }
#else

        /*
         * We have to check if we can exit, run timers, and then recheck.
         */
        /** @todo
         * The other thing we have to check is how long it is till the next timer
         * can be serviced and not wait any longer than that.
         */
        if (VM_FF_ISPENDING(pVM, fMask))
            break;
        STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltTimers, b);
        TMR3TimerQueuesDo(pVM);
        STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltTimers, b);
        if (VM_FF_ISPENDING(pVM, fMask))
            break;
        /* hacking */
        RTThreadYield();
        TMR3TimerQueuesDo(pVM);
        if (VM_FF_ISPENDING(pVM, fMask))
            break;

        /*
         * Wait for a while. Someone will wake us up or interrupt the call if
         * anything needs our attention.
         */
        STAM_PROFILE_ADV_START(&pVM->vm.s.StatHaltBlock, a);
        rc = RTSemEventWait(pVM->vm.s.EventSemWait, 10);
        STAM_PROFILE_ADV_STOP(&pVM->vm.s.StatHaltBlock, a);
#endif
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
    ASMAtomicXchgU32(&pVM->vm.s.fWait, 0);

    /*
     *
     * Pause the TSC, it's restarted when we start executing,
     * and resume the yielder.
     */
    TMCpuTickPause(pVM);
    VMMR3YieldResume(pVM);


    LogFlow(("VMR3WaitHalted: returns %Vrc (FF %#x)\n", rc, pVM->fForcedActions ));
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

