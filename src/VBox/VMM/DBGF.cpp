/* $Id$ */
/** @file
 * DBGF - Debugger Facility.
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


/** @page   pg_dbgf     DBGF - The Debugger Facility
 *
 * The purpose of the DBGF is to provide an interface for debuggers to
 * manipulate the VMM without having to mess up the source code for each of
 * them. The DBGF is always built in and will always work when a debugger
 * attaches to the VM. The DBGF provides the basic debugger features, such as
 * halting execution, handling breakpoints, single step execution, instruction
 * disassembly, info querying, OS specific diggers, symbol and module
 * management.
 *
 * The interface is working in a manner similar to the win32, linux and os2
 * debugger interfaces. It interface has an asynchronous nature. This comes from
 * the fact that the VMM and the Debugger are running in different threads. They
 * are refered to as the "emulation thread" and the "debugger thread", or as the
 * "ping thread" and the "pong thread, respectivly. (The last set of names comes
 * from the use of the Ping-Pong synchronization construct from the RTSem API.)
 *
 * @see grp_dbgf
 *
 *
 * @section sec_dbgf_scenario   Usage Scenario
 *
 * The debugger starts by attaching to the VM. For pratical reasons we limit the
 * number of concurrently attached debuggers to 1 per VM. The action of
 * attaching to the VM causes the VM to check and generate debug events.
 *
 * The debugger then will wait/poll for debug events and issue commands.
 *
 * The waiting and polling is done by the DBGFEventWait() function. It will wait
 * for the emulation thread to send a ping, thus indicating that there is an
 * event waiting to be processed.
 *
 * An event can be a respons to an command issued previously, the hitting of a
 * breakpoint, or running into a bad/fatal VMM condition. The debugger now have
 * the ping and must respond to the event at hand - the VMM is waiting. This
 * usually means that the user of the debugger must do something, but it doesn't
 * have to. The debugger is free to call any DBGF function (nearly at least)
 * while processing the event.
 *
 * Typically the user will issue a request for the execution to be resumed, so
 * the debugger calls DBGFResume() and goes back to waiting/polling for events.
 *
 * When the user eventually terminates the debugging session or selects another
 * VM, the debugger detaches from the VM. This means that breakpoints are
 * disabled and that the emulation thread no longer polls for debugger commands.
 *
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include <VBox/rem.h>
#include <VBox/em.h>
#include <VBox/hwaccm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/asm.h>
#include <iprt/time.h>
#include <iprt/assert.h>
#include <iprt/stream.h>
#include <iprt/env.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int dbgfR3VMMWait(PVM pVM);
static int dbgfR3VMMCmd(PVM pVM, DBGFCMD enmCmd, PDBGFCMDDATA pCmdData, bool *pfResumeExecution);
static DECLCALLBACK(int) dbgfR3Attach(PVM pVM);


/**
 * Sets the VMM Debug Command variable.
 *
 * @returns Previous command.
 * @param   pVM     VM Handle.
 * @param   enmCmd  The command.
 */
DECLINLINE(DBGFCMD) dbgfR3SetCmd(PVM pVM, DBGFCMD enmCmd)
{
    DBGFCMD rc;
    if (enmCmd == DBGFCMD_NO_COMMAND)
    {
        Log2(("DBGF: Setting command to %d (DBGFCMD_NO_COMMAND)\n", enmCmd));
        rc = (DBGFCMD)ASMAtomicXchgU32((uint32_t volatile *)(void *)&pVM->dbgf.s.enmVMMCmd, enmCmd);
        VM_FF_CLEAR(pVM, VM_FF_DBGF);
    }
    else
    {
        Log2(("DBGF: Setting command to %d\n", enmCmd));
        AssertMsg(pVM->dbgf.s.enmVMMCmd == DBGFCMD_NO_COMMAND, ("enmCmd=%d enmVMMCmd=%d\n", enmCmd, pVM->dbgf.s.enmVMMCmd));
        rc = (DBGFCMD)ASMAtomicXchgU32((uint32_t volatile *)(void *)&pVM->dbgf.s.enmVMMCmd, enmCmd);
        VM_FF_SET(pVM, VM_FF_DBGF);
        VMR3NotifyGlobalFF(pVM, false /* didn't notify REM */);
    }
    return rc;
}


/**
 * Initializes the DBGF.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
VMMR3DECL(int) DBGFR3Init(PVM pVM)
{
    int rc = dbgfR3InfoInit(pVM);
    if (RT_SUCCESS(rc))
        rc = dbgfR3SymInit(pVM);
    if (RT_SUCCESS(rc))
        rc = dbgfR3BpInit(pVM);
    return rc;
}


/**
 * Termiantes and cleans up resources allocated by the DBGF.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 */
VMMR3DECL(int) DBGFR3Term(PVM pVM)
{
    int rc;

    /*
     * Send a termination event to any attached debugger.
     */
    /* wait to become the speaker (we should already be that). */
    if (    pVM->dbgf.s.fAttached
        &&  RTSemPingShouldWait(&pVM->dbgf.s.PingPong))
        RTSemPingWait(&pVM->dbgf.s.PingPong, 5000);

    /* now, send the event if we're the speaker. */
    if (    pVM->dbgf.s.fAttached
        &&  RTSemPingIsSpeaker(&pVM->dbgf.s.PingPong))
    {
        DBGFCMD enmCmd = dbgfR3SetCmd(pVM, DBGFCMD_NO_COMMAND);
        if (enmCmd == DBGFCMD_DETACH_DEBUGGER)
            /* the debugger beat us to initiating the detaching. */
            rc = VINF_SUCCESS;
        else
        {
            /* ignore the command (if any). */
            enmCmd = DBGFCMD_NO_COMMAND;
            pVM->dbgf.s.DbgEvent.enmType = DBGFEVENT_TERMINATING;
            pVM->dbgf.s.DbgEvent.enmCtx  = DBGFEVENTCTX_OTHER;
            rc = RTSemPing(&pVM->dbgf.s.PingPong);
        }

        /*
         * Process commands until we get a detached command.
         */
        while (RT_SUCCESS(rc) && enmCmd != DBGFCMD_DETACHED_DEBUGGER)
        {
            if (enmCmd != DBGFCMD_NO_COMMAND)
            {
                /* process command */
                bool fResumeExecution;
                DBGFCMDDATA CmdData = pVM->dbgf.s.VMMCmdData;
                rc = dbgfR3VMMCmd(pVM, enmCmd, &CmdData, &fResumeExecution);
                enmCmd = DBGFCMD_NO_COMMAND;
            }
            else
            {
                /* wait for new command. */
                rc = RTSemPingWait(&pVM->dbgf.s.PingPong, RT_INDEFINITE_WAIT);
                if (RT_SUCCESS(rc))
                    enmCmd = dbgfR3SetCmd(pVM, DBGFCMD_NO_COMMAND);
            }
        }
    }

    /*
     * Terminate the other bits.
     */
    dbgfR3OSTerm(pVM);
    dbgfR3InfoTerm(pVM);
    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM         VM handle.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMMR3DECL(void) DBGFR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
}


/**
 * Waits a little while for a debuggger to attach.
 *
 * @returns True is a debugger have attached.
 * @param   pVM         VM handle.
 * @param   enmEvent    Event.
 */
bool dbgfR3WaitForAttach(PVM pVM, DBGFEVENTTYPE enmEvent)
{
    /*
     * First a message.
     */
#ifndef RT_OS_L4

# if !defined(DEBUG) || defined(DEBUG_sandervl) || defined(DEBUG_frank)
    int cWait = 10;
# else
    int cWait = HWACCMIsEnabled(pVM)
             && (   enmEvent == DBGFEVENT_ASSERTION_HYPER
                 || enmEvent == DBGFEVENT_FATAL_ERROR)
             && !RTEnvExist("VBOX_DBGF_WAIT_FOR_ATTACH")
              ? 10
              : 150;
# endif
    RTStrmPrintf(g_pStdErr, "DBGF: No debugger attached, waiting %d second%s for one to attach (event=%d)\n",
                 cWait / 10, cWait != 10 ? "s" : "", enmEvent);
    RTStrmFlush(g_pStdErr);
    while (cWait > 0)
    {
        RTThreadSleep(100);
        if (pVM->dbgf.s.fAttached)
        {
            RTStrmPrintf(g_pStdErr, "Attached!\n");
            RTStrmFlush(g_pStdErr);
            return true;
        }

        /* next */
        if (!(cWait % 10))
        {
            RTStrmPrintf(g_pStdErr, "%d.", cWait / 10);
            RTStrmFlush(g_pStdErr);
        }
        cWait--;
    }
#endif

    RTStrmPrintf(g_pStdErr, "Stopping the VM!\n");
    RTStrmFlush(g_pStdErr);
    return false;
}


/**
 * Forced action callback.
 * The VMM will call this from it's main loop when VM_FF_DBGF is set.
 *
 * The function checks and executes pending commands from the debugger.
 *
 * @returns VINF_SUCCESS normally.
 * @returns VERR_DBGF_RAISE_FATAL_ERROR to pretend a fatal error happend.
 * @param   pVM         VM Handle.
 */
VMMR3DECL(int) DBGFR3VMMForcedAction(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /*
     * Clear the FF DBGF request flag.
     */
    Assert(pVM->fGlobalForcedActions & VM_FF_DBGF);
    VM_FF_CLEAR(pVM, VM_FF_DBGF);

    /*
     * Commands?
     */
    int rc = VINF_SUCCESS;
    if (pVM->dbgf.s.enmVMMCmd != DBGFCMD_NO_COMMAND)
    {
        /** @todo stupid GDT/LDT sync hack. go away! */
        SELMR3UpdateFromCPUM(pVM, pVCpu);

        /*
         * Process the command.
         */
        bool            fResumeExecution;
        DBGFCMDDATA     CmdData = pVM->dbgf.s.VMMCmdData;
        DBGFCMD         enmCmd = dbgfR3SetCmd(pVM, DBGFCMD_NO_COMMAND);
        rc = dbgfR3VMMCmd(pVM, enmCmd, &CmdData, &fResumeExecution);
        if (!fResumeExecution)
            rc = dbgfR3VMMWait(pVM);
    }
    return rc;
}


/**
 * Flag whether the event implies that we're stopped in the hypervisor code
 * and have to block certain operations.
 *
 * @param   pVM         The VM handle.
 * @param   enmEvent    The event.
 */
static void dbgfR3EventSetStoppedInHyperFlag(PVM pVM, DBGFEVENTTYPE enmEvent)
{
    switch (enmEvent)
    {
        case DBGFEVENT_STEPPED_HYPER:
        case DBGFEVENT_ASSERTION_HYPER:
        case DBGFEVENT_BREAKPOINT_HYPER:
            pVM->dbgf.s.fStoppedInHyper = true;
            break;
        default:
            pVM->dbgf.s.fStoppedInHyper = false;
            break;
    }
}


/**
 * Try to determine the event context.
 *
 * @returns debug event context.
 * @param   pVM         The VM handle.
 */
static DBGFEVENTCTX dbgfR3FigureEventCtx(PVM pVM)
{
    /** @todo SMP support! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    switch (EMGetState(pVCpu))
    {
        case EMSTATE_RAW:
        case EMSTATE_DEBUG_GUEST_RAW:
            return DBGFEVENTCTX_RAW;

        case EMSTATE_REM:
        case EMSTATE_DEBUG_GUEST_REM:
            return DBGFEVENTCTX_REM;

        case EMSTATE_DEBUG_HYPER:
        case EMSTATE_GURU_MEDITATION:
            return DBGFEVENTCTX_HYPER;

        default:
            return DBGFEVENTCTX_OTHER;
    }
}

/**
 * The common event prologue code.
 * It will set the 'stopped-in-hyper' flag, make sure someone's attach,
 * and perhaps process any high priority pending actions (none yet).
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   enmEvent    The event to be sent.
 */
static int dbgfR3EventPrologue(PVM pVM, DBGFEVENTTYPE enmEvent)
{
    /** @todo SMP */
    PVMCPU pVCpu = VMMGetCpu(pVM);

    /*
     * Check if a debugger is attached.
     */
    if (    !pVM->dbgf.s.fAttached
        &&  !dbgfR3WaitForAttach(pVM, enmEvent))
    {
        Log(("DBGFR3VMMEventSrc: enmEvent=%d - debugger not attached\n", enmEvent));
        return VERR_DBGF_NOT_ATTACHED;
    }

    /*
     * Sync back the state from the REM.
     */
    dbgfR3EventSetStoppedInHyperFlag(pVM, enmEvent);
    if (!pVM->dbgf.s.fStoppedInHyper)
        REMR3StateUpdate(pVM, pVCpu);

    /*
     * Look thru pending commands and finish those which make sense now.
     */
    /** @todo Process/purge pending commands. */
    //int rc = DBGFR3VMMForcedAction(pVM);
    return VINF_SUCCESS;
}


/**
 * Sends the event in the event buffer.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
static int dbgfR3SendEvent(PVM pVM)
{
    int rc = RTSemPing(&pVM->dbgf.s.PingPong);
    if (RT_SUCCESS(rc))
        rc = dbgfR3VMMWait(pVM);

    pVM->dbgf.s.fStoppedInHyper = false;
    /** @todo sync VMM -> REM after exitting the debugger. everything may change while in the debugger! */
    return rc;
}


/**
 * Send a generic debugger event which takes no data.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   enmEvent    The event to send.
 */
VMMR3DECL(int) DBGFR3Event(PVM pVM, DBGFEVENTTYPE enmEvent)
{
    int rc = dbgfR3EventPrologue(pVM, enmEvent);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Send the event and process the reply communication.
     */
    pVM->dbgf.s.DbgEvent.enmType = enmEvent;
    pVM->dbgf.s.DbgEvent.enmCtx  = dbgfR3FigureEventCtx(pVM);
    return dbgfR3SendEvent(pVM);
}


/**
 * Send a debugger event which takes the full source file location.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   enmEvent    The event to send.
 * @param   pszFile     Source file.
 * @param   uLine       Line number in source file.
 * @param   pszFunction Function name.
 * @param   pszFormat   Message which accompanies the event.
 * @param   ...         Message arguments.
 */
VMMR3DECL(int) DBGFR3EventSrc(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszFile, unsigned uLine, const char *pszFunction, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    int rc = DBGFR3EventSrcV(pVM, enmEvent, pszFile, uLine, pszFunction, pszFormat, args);
    va_end(args);
    return rc;
}


/**
 * Send a debugger event which takes the full source file location.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   enmEvent    The event to send.
 * @param   pszFile     Source file.
 * @param   uLine       Line number in source file.
 * @param   pszFunction Function name.
 * @param   pszFormat   Message which accompanies the event.
 * @param   args        Message arguments.
 */
VMMR3DECL(int) DBGFR3EventSrcV(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszFile, unsigned uLine, const char *pszFunction, const char *pszFormat, va_list args)
{
    int rc = dbgfR3EventPrologue(pVM, enmEvent);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Format the message.
     */
    char   *pszMessage = NULL;
    char    szMessage[8192];
    if (pszFormat && *pszFormat)
    {
        pszMessage = &szMessage[0];
        RTStrPrintfV(szMessage, sizeof(szMessage), pszFormat, args);
    }

    /*
     * Send the event and process the reply communication.
     */
    pVM->dbgf.s.DbgEvent.enmType = enmEvent;
    pVM->dbgf.s.DbgEvent.enmCtx  = dbgfR3FigureEventCtx(pVM);
    pVM->dbgf.s.DbgEvent.u.Src.pszFile      = pszFile;
    pVM->dbgf.s.DbgEvent.u.Src.uLine        = uLine;
    pVM->dbgf.s.DbgEvent.u.Src.pszFunction  = pszFunction;
    pVM->dbgf.s.DbgEvent.u.Src.pszMessage   = pszMessage;
    return dbgfR3SendEvent(pVM);
}


/**
 * Send a debugger event which takes the two assertion messages.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   enmEvent    The event to send.
 * @param   pszMsg1     First assertion message.
 * @param   pszMsg2     Second assertion message.
 */
VMMR3DECL(int) DBGFR3EventAssertion(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszMsg1, const char *pszMsg2)
{
    int rc = dbgfR3EventPrologue(pVM, enmEvent);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Send the event and process the reply communication.
     */
    pVM->dbgf.s.DbgEvent.enmType = enmEvent;
    pVM->dbgf.s.DbgEvent.enmCtx  = dbgfR3FigureEventCtx(pVM);
    pVM->dbgf.s.DbgEvent.u.Assert.pszMsg1 = pszMsg1;
    pVM->dbgf.s.DbgEvent.u.Assert.pszMsg2 = pszMsg2;
    return dbgfR3SendEvent(pVM);
}


/**
 * Breakpoint was hit somewhere.
 * Figure out which breakpoint it is and notify the debugger.
 *
 * @returns VBox status.
 * @param   pVM         The VM handle.
 * @param   enmEvent    DBGFEVENT_BREAKPOINT_HYPER or DBGFEVENT_BREAKPOINT.
 */
VMMR3DECL(int) DBGFR3EventBreakpoint(PVM pVM, DBGFEVENTTYPE enmEvent)
{
    int rc = dbgfR3EventPrologue(pVM, enmEvent);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Send the event and process the reply communication.
     */
    /** @todo SMP */
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    pVM->dbgf.s.DbgEvent.enmType = enmEvent;
    RTUINT iBp = pVM->dbgf.s.DbgEvent.u.Bp.iBp = pVCpu->dbgf.s.iActiveBp;
    pVCpu->dbgf.s.iActiveBp = ~0U;
    if (iBp != ~0U)
        pVM->dbgf.s.DbgEvent.enmCtx = DBGFEVENTCTX_RAW;
    else
    {
        /* REM breakpoints has be been searched for. */
#if 0   /** @todo get flat PC api! */
        uint32_t eip = CPUMGetGuestEIP(pVM);
#else
        /* @todo SMP support!! */
        PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(VMMGetCpu(pVM));
        RTGCPTR  eip = pCtx->rip + pCtx->csHid.u64Base;
#endif
        for (iBp = 0; iBp < RT_ELEMENTS(pVM->dbgf.s.aBreakpoints); iBp++)
            if (    pVM->dbgf.s.aBreakpoints[iBp].enmType == DBGFBPTYPE_REM
                &&  pVM->dbgf.s.aBreakpoints[iBp].GCPtr == eip)
            {
                pVM->dbgf.s.DbgEvent.u.Bp.iBp = iBp;
                break;
            }
        AssertMsg(pVM->dbgf.s.DbgEvent.u.Bp.iBp != ~0U, ("eip=%08x\n", eip));
        pVM->dbgf.s.DbgEvent.enmCtx = DBGFEVENTCTX_REM;
    }
    return dbgfR3SendEvent(pVM);
}


/**
 * Waits for the debugger to respond.
 *
 * @returns VBox status. (clearify)
 * @param   pVM     VM handle.
 */
static int dbgfR3VMMWait(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    LogFlow(("dbgfR3VMMWait:\n"));

    /** @todo stupid GDT/LDT sync hack. go away! */
    SELMR3UpdateFromCPUM(pVM, pVCpu);
    int rcRet = VINF_SUCCESS;

    /*
     * Waits for the debugger to reply (i.e. issue an command).
     */
    for (;;)
    {
        /*
         * Wait.
         */
        for (;;)
        {
            int rc = RTSemPingWait(&pVM->dbgf.s.PingPong, 250);
            if (RT_SUCCESS(rc))
                break;
            if (rc != VERR_TIMEOUT)
            {
                LogFlow(("dbgfR3VMMWait: returns %Rrc\n", rc));
                return rc;
            }

            if (    VM_FF_ISSET(pVM, VM_FF_REQUEST)
                ||  VMCPU_FF_ISSET(pVCpu, VMCPU_FF_REQUEST))
            {
                LogFlow(("dbgfR3VMMWait: Processes requests...\n"));
                rc = VMR3ReqProcessU(pVM->pUVM, VMCPUID_ANY);
                if (rc == VINF_SUCCESS)
                    rc = VMR3ReqProcessU(pVM->pUVM, pVCpu->idCpu);
                LogFlow(("dbgfR3VMMWait: VMR3ReqProcess -> %Rrc rcRet=%Rrc\n", rc, rcRet));
                if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
                {
                    switch (rc)
                    {
                        case VINF_EM_DBG_BREAKPOINT:
                        case VINF_EM_DBG_STEPPED:
                        case VINF_EM_DBG_STEP:
                        case VINF_EM_DBG_STOP:
                            AssertMsgFailed(("rc=%Rrc\n", rc));
                            break;

                        /* return straight away */
                        case VINF_EM_TERMINATE:
                        case VINF_EM_OFF:
                            LogFlow(("dbgfR3VMMWait: returns %Rrc\n", rc));
                            return rc;

                        /* remember return code. */
                        default:
                            AssertReleaseMsgFailed(("rc=%Rrc is not in the switch!\n", rc));
                        case VINF_EM_RESET:
                        case VINF_EM_SUSPEND:
                        case VINF_EM_HALT:
                        case VINF_EM_RESUME:
                        case VINF_EM_RESCHEDULE:
                        case VINF_EM_RESCHEDULE_REM:
                        case VINF_EM_RESCHEDULE_RAW:
                            if (rc < rcRet || rcRet == VINF_SUCCESS)
                                rcRet = rc;
                            break;
                    }
                }
                else if (RT_FAILURE(rc))
                {
                    LogFlow(("dbgfR3VMMWait: returns %Rrc\n", rc));
                    return rc;
                }
            }
        }

        /*
         * Process the command.
         */
        bool            fResumeExecution;
        DBGFCMDDATA     CmdData = pVM->dbgf.s.VMMCmdData;
        DBGFCMD         enmCmd = dbgfR3SetCmd(pVM, DBGFCMD_NO_COMMAND);
        int rc = dbgfR3VMMCmd(pVM, enmCmd, &CmdData, &fResumeExecution);
        if (fResumeExecution)
        {
            if (RT_FAILURE(rc))
                rcRet = rc;
            else if (    rc >= VINF_EM_FIRST
                     &&  rc <= VINF_EM_LAST
                     &&  (rc < rcRet || rcRet == VINF_SUCCESS))
                rcRet = rc;
            LogFlow(("dbgfR3VMMWait: returns %Rrc\n", rcRet));
            return rcRet;
        }
    }
}


/**
 * Executes command from debugger.
 * The caller is responsible for waiting or resuming execution based on the
 * value returned in the *pfResumeExecution indicator.
 *
 * @returns VBox status. (clearify!)
 * @param   pVM                 VM Handle.
 * @param   enmCmd              The command in question.
 * @param   pCmdData            Pointer to the command data.
 * @param   pfResumeExecution   Where to store the resume execution / continue waiting indicator.
 */
static int dbgfR3VMMCmd(PVM pVM, DBGFCMD enmCmd, PDBGFCMDDATA pCmdData, bool *pfResumeExecution)
{
    bool    fSendEvent;
    bool    fResume;
    int     rc = VINF_SUCCESS;

    switch (enmCmd)
    {
        /*
         * Halt is answered by an event say that we've halted.
         */
        case DBGFCMD_HALT:
        {
            pVM->dbgf.s.DbgEvent.enmType = DBGFEVENT_HALT_DONE;
            pVM->dbgf.s.DbgEvent.enmCtx  = dbgfR3FigureEventCtx(pVM);
            fSendEvent = true;
            fResume = false;
            break;
        }


        /*
         * Resume is not answered we'll just resume execution.
         */
        case DBGFCMD_GO:
        {
            fSendEvent = false;
            fResume = true;
            break;
        }

        /** @todo implement (and define) the rest of the commands. */

        /*
         * Disable breakpoints and stuff.
         * Send an everythings cool event to the debugger thread and resume execution.
         */
        case DBGFCMD_DETACH_DEBUGGER:
        {
            ASMAtomicWriteBool(&pVM->dbgf.s.fAttached, false);
            pVM->dbgf.s.DbgEvent.enmType = DBGFEVENT_DETACH_DONE;
            pVM->dbgf.s.DbgEvent.enmCtx  = DBGFEVENTCTX_OTHER;
            fSendEvent = true;
            fResume = true;
            break;
        }

        /*
         * The debugger has detached successfully.
         * There is no reply to this event.
         */
        case DBGFCMD_DETACHED_DEBUGGER:
        {
            fSendEvent = false;
            fResume = true;
            break;
        }

        /*
         * Single step, with trace into.
         */
        case DBGFCMD_SINGLE_STEP:
        {
            Log2(("Single step\n"));
            rc = VINF_EM_DBG_STEP;
            /** @todo SMP */
            PVMCPU pVCpu = VMMGetCpu0(pVM);
            pVCpu->dbgf.s.fSingleSteppingRaw = true;
            fSendEvent = false;
            fResume = true;
            break;
        }

        /*
         * Default is to send an invalid command event.
         */
        default:
        {
            pVM->dbgf.s.DbgEvent.enmType = DBGFEVENT_INVALID_COMMAND;
            pVM->dbgf.s.DbgEvent.enmCtx  = dbgfR3FigureEventCtx(pVM);
            fSendEvent = true;
            fResume = false;
            break;
        }
    }

    /*
     * Send pending event.
     */
    if (fSendEvent)
    {
        Log2(("DBGF: Emulation thread: sending event %d\n", pVM->dbgf.s.DbgEvent.enmType));
        int rc2 = RTSemPing(&pVM->dbgf.s.PingPong);
        if (RT_FAILURE(rc2))
        {
            AssertRC(rc2);
            *pfResumeExecution = true;
            return rc2;
        }
    }

    /*
     * Return.
     */
    *pfResumeExecution = fResume;
    return rc;
}


/**
 * Attaches a debugger to the specified VM.
 *
 * Only one debugger at a time.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 */
VMMR3DECL(int) DBGFR3Attach(PVM pVM)
{
    /*
     * Some validations first.
     */
    if (!VALID_PTR(pVM))
    {
        Log(("DBGFR3Attach: bad VM handle: %p\n", pVM));
        return VERR_INVALID_HANDLE;
    }
    VMSTATE enmVMState = pVM->enmVMState;
    if (    enmVMState >= VMSTATE_DESTROYING
        ||  enmVMState <  VMSTATE_CREATING)
    {
        Log(("DBGFR3Attach: Invalid VM state: %s\n", VMGetStateName(enmVMState)));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Call the VM, use EMT for serialization.
     */
    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3Attach, 1, pVM);
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * EMT worker for DBGFR3Attach.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the shared VM structure.
 */
static DECLCALLBACK(int) dbgfR3Attach(PVM pVM)
{
    if (pVM->dbgf.s.fAttached)
    {
        Log(("dbgR3Attach: Debugger already attached\n"));
        return VERR_DBGF_ALREADY_ATTACHED;
    }

    /*
     * Create the Ping-Pong structure.
     */
    int rc = RTSemPingPongInit(&pVM->dbgf.s.PingPong);
    AssertRCReturn(rc, rc);

    /*
     * Set the attached flag.
     */
    ASMAtomicWriteBool(&pVM->dbgf.s.fAttached, true);
    return VINF_SUCCESS;
}


/**
 * Detaches a debugger from the specified VM.
 *
 * Caller must be attached to the VM.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 */
VMMR3DECL(int) DBGFR3Detach(PVM pVM)
{
    LogFlow(("DBGFR3Detach:\n"));
    int rc;

    /*
     * Check if attached.
     */
    AssertReturn(pVM->dbgf.s.fAttached, VERR_DBGF_NOT_ATTACHED);

    /*
     * Try send the detach command.
     * Keep in mind that we might be racing EMT, so, be extra careful.
     */
    DBGFCMD enmCmd = dbgfR3SetCmd(pVM, DBGFCMD_DETACH_DEBUGGER);
    if (RTSemPongIsSpeaker(&pVM->dbgf.s.PingPong))
    {
        rc = RTSemPong(&pVM->dbgf.s.PingPong);
        AssertMsgRCReturn(rc, ("Failed to signal emulation thread. rc=%Rrc\n", rc), rc);
        LogRel(("DBGFR3Detach: enmCmd=%d (pong -> ping)\n", enmCmd));
    }

    /*
     * Wait for the OK event.
     */
    rc = RTSemPongWait(&pVM->dbgf.s.PingPong, RT_INDEFINITE_WAIT);
    AssertLogRelMsgRCReturn(rc, ("Wait on detach command failed, rc=%Rrc\n", rc), rc);

    /*
     * Send the notification command indicating that we're really done.
     */
    enmCmd = dbgfR3SetCmd(pVM, DBGFCMD_DETACHED_DEBUGGER);
    rc = RTSemPong(&pVM->dbgf.s.PingPong);
    AssertMsgRCReturn(rc, ("Failed to signal emulation thread. rc=%Rrc\n", rc), rc);

    LogFlowFunc(("returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Wait for a debug event.
 *
 * @returns VBox status. Will not return VBOX_INTERRUPTED.
 * @param   pVM         VM handle.
 * @param   cMillies    Number of millies to wait.
 * @param   ppEvent     Where to store the event pointer.
 */
VMMR3DECL(int) DBGFR3EventWait(PVM pVM, unsigned cMillies, PCDBGFEVENT *ppEvent)
{
    /*
     * Check state.
     */
    AssertReturn(pVM->dbgf.s.fAttached, VERR_DBGF_NOT_ATTACHED);
    *ppEvent = NULL;

    /*
     * Wait.
     */
    int rc = RTSemPongWait(&pVM->dbgf.s.PingPong, cMillies);
    if (RT_SUCCESS(rc))
    {
        *ppEvent = &pVM->dbgf.s.DbgEvent;
        Log2(("DBGF: Debugger thread: receiving event %d\n", (*ppEvent)->enmType));
        return VINF_SUCCESS;
    }

    return rc;
}


/**
 * Halts VM execution.
 *
 * After calling this the VM isn't actually halted till an DBGFEVENT_HALT_DONE
 * arrives. Until that time it's not possible to issue any new commands.
 *
 * @returns VBox status.
 * @param   pVM     VM handle.
 */
VMMR3DECL(int) DBGFR3Halt(PVM pVM)
{
    /*
     * Check state.
     */
    AssertReturn(pVM->dbgf.s.fAttached, VERR_DBGF_NOT_ATTACHED);
    RTPINGPONGSPEAKER enmSpeaker = pVM->dbgf.s.PingPong.enmSpeaker;
    if (   enmSpeaker == RTPINGPONGSPEAKER_PONG
        || enmSpeaker == RTPINGPONGSPEAKER_PONG_SIGNALED)
        return VWRN_DBGF_ALREADY_HALTED;

    /*
     * Send command.
     */
    dbgfR3SetCmd(pVM, DBGFCMD_HALT);

    return VINF_SUCCESS;
}


/**
 * Checks if the VM is halted by the debugger.
 *
 * @returns True if halted.
 * @returns False if not halted.
 * @param   pVM     VM handle.
 */
VMMR3DECL(bool) DBGFR3IsHalted(PVM pVM)
{
    AssertReturn(pVM->dbgf.s.fAttached, false);
    RTPINGPONGSPEAKER enmSpeaker = pVM->dbgf.s.PingPong.enmSpeaker;
    return enmSpeaker == RTPINGPONGSPEAKER_PONG_SIGNALED
        || enmSpeaker == RTPINGPONGSPEAKER_PONG;
}


/**
 * Checks if the debugger can wait for events or not.
 *
 * This function is only used by lazy, multiplexing debuggers. :-)
 *
 * @returns True if waitable.
 * @returns False if not waitable.
 * @param   pVM     VM handle.
 */
VMMR3DECL(bool) DBGFR3CanWait(PVM pVM)
{
    AssertReturn(pVM->dbgf.s.fAttached, false);
    return RTSemPongShouldWait(&pVM->dbgf.s.PingPong);
}


/**
 * Resumes VM execution.
 *
 * There is no receipt event on this command.
 *
 * @returns VBox status.
 * @param   pVM     VM handle.
 */
VMMR3DECL(int) DBGFR3Resume(PVM pVM)
{
    /*
     * Check state.
     */
    AssertReturn(pVM->dbgf.s.fAttached, VERR_DBGF_NOT_ATTACHED);
    AssertReturn(RTSemPongIsSpeaker(&pVM->dbgf.s.PingPong), VERR_SEM_OUT_OF_TURN);

    /*
     * Send the ping back to the emulation thread telling it to run.
     */
    dbgfR3SetCmd(pVM, DBGFCMD_GO);
    int rc = RTSemPong(&pVM->dbgf.s.PingPong);
    AssertRC(rc);

    return rc;
}


/**
 * Step Into.
 *
 * A single step event is generated from this command.
 * The current implementation is not reliable, so don't rely on the event comming.
 *
 * @returns VBox status.
 * @param   pVM     VM handle.
 * @param   idCpu   The ID of the CPU to single step on.
 */
VMMR3DECL(int) DBGFR3Step(PVM pVM, VMCPUID idCpu)
{
    /*
     * Check state.
     */
    AssertReturn(pVM->dbgf.s.fAttached, VERR_DBGF_NOT_ATTACHED);
    AssertReturn(RTSemPongIsSpeaker(&pVM->dbgf.s.PingPong), VERR_SEM_OUT_OF_TURN);
    AssertReturn(idCpu < pVM->cCPUs, VERR_INVALID_PARAMETER);

    /*
     * Send the ping back to the emulation thread telling it to run.
     */
/** @todo SMP (idCpu) */
    dbgfR3SetCmd(pVM, DBGFCMD_SINGLE_STEP);
    int rc = RTSemPong(&pVM->dbgf.s.PingPong);
    AssertRC(rc);
    return rc;
}


/**
 * Call this to single step programatically.
 *
 * You must pass down the return code to the EM loop! That's
 * where the actual single stepping take place (at least in the
 * current implementation).
 *
 * @returns VINF_EM_DBG_STEP
 *
 * @param   pVCpu       The virtual CPU handle.
 *
 * @thread  VCpu EMT
 */
VMMR3DECL(int) DBGFR3PrgStep(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT(pVCpu);

    pVCpu->dbgf.s.fSingleSteppingRaw = true;
    return VINF_EM_DBG_STEP;
}

