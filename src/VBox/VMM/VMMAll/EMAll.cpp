/* $Id$ */
/** @file
 * EM - Execution Monitor(/Manager) - All contexts
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/vmm/em.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/stam.h>
#include "EMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>




/**
 * Get the current execution manager status.
 *
 * @returns Current status.
 * @param   pVCpu         The cross context virtual CPU structure.
 */
VMM_INT_DECL(EMSTATE) EMGetState(PVMCPU pVCpu)
{
    return pVCpu->em.s.enmState;
}


/**
 * Sets the current execution manager status. (use only when you know what you're doing!)
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmNewState The new state, EMSTATE_WAIT_SIPI or EMSTATE_HALTED.
 */
VMM_INT_DECL(void)    EMSetState(PVMCPU pVCpu, EMSTATE enmNewState)
{
    /* Only allowed combination: */
    Assert(pVCpu->em.s.enmState == EMSTATE_WAIT_SIPI && enmNewState == EMSTATE_HALTED);
    pVCpu->em.s.enmState = enmNewState;
}


/**
 * Sets the PC for which interrupts should be inhibited.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   PC          The PC.
 */
VMMDECL(void) EMSetInhibitInterruptsPC(PVMCPU pVCpu, RTGCUINTPTR PC)
{
    pVCpu->em.s.GCPtrInhibitInterrupts = PC;
    VMCPU_FF_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);
}


/**
 * Gets the PC for which interrupts should be inhibited.
 *
 * There are a few instructions which inhibits or delays interrupts
 * for the instruction following them. These instructions are:
 *      - STI
 *      - MOV SS, r/m16
 *      - POP SS
 *
 * @returns The PC for which interrupts should be inhibited.
 * @param   pVCpu       The cross context virtual CPU structure.
 *
 */
VMMDECL(RTGCUINTPTR) EMGetInhibitInterruptsPC(PVMCPU pVCpu)
{
    return pVCpu->em.s.GCPtrInhibitInterrupts;
}


/**
 * Enables / disable hypercall instructions.
 *
 * This interface is used by GIM to tell the execution monitors whether the
 * hypercall instruction (VMMCALL & VMCALL) are allowed or should \#UD.
 *
 * @param   pVCpu       The cross context virtual CPU structure this applies to.
 * @param   fEnabled    Whether hypercall instructions are enabled (true) or not.
 */
VMMDECL(void) EMSetHypercallInstructionsEnabled(PVMCPU pVCpu, bool fEnabled)
{
    pVCpu->em.s.fHypercallEnabled = fEnabled;
}


/**
 * Checks if hypercall instructions (VMMCALL & VMCALL) are enabled or not.
 *
 * @returns true if enabled, false if not.
 * @param   pVCpu   The cross context virtual CPU structure.
 *
 * @note    If this call becomes a performance factor, we can make the data
 *          field available thru a read-only view in VMCPU.  See VM::cpum.ro.
 */
VMMDECL(bool) EMAreHypercallInstructionsEnabled(PVMCPU pVCpu)
{
    return pVCpu->em.s.fHypercallEnabled;
}


/**
 * Prepare an MWAIT - essentials of the MONITOR instruction.
 *
 * @returns VINF_SUCCESS
 * @param   pVCpu               The cross context virtual CPU structure of the calling EMT.
 * @param   rax                 The content of RAX.
 * @param   rcx                 The content of RCX.
 * @param   rdx                 The content of RDX.
 * @param   GCPhys              The physical address corresponding to rax.
 */
VMM_INT_DECL(int) EMMonitorWaitPrepare(PVMCPU pVCpu, uint64_t rax, uint64_t rcx, uint64_t rdx, RTGCPHYS GCPhys)
{
    pVCpu->em.s.MWait.uMonitorRAX = rax;
    pVCpu->em.s.MWait.uMonitorRCX = rcx;
    pVCpu->em.s.MWait.uMonitorRDX = rdx;
    pVCpu->em.s.MWait.fWait |= EMMWAIT_FLAG_MONITOR_ACTIVE;
    /** @todo Make use of GCPhys. */
    NOREF(GCPhys);
    /** @todo Complete MONITOR implementation.  */
    return VINF_SUCCESS;
}


/**
 * Checks if the monitor hardware is armed / active.
 *
 * @returns true if armed, false otherwise.
 * @param   pVCpu               The cross context virtual CPU structure of the calling EMT.
 */
VMM_INT_DECL(bool) EMMonitorIsArmed(PVMCPU pVCpu)
{
    return RT_BOOL(pVCpu->em.s.MWait.fWait & EMMWAIT_FLAG_MONITOR_ACTIVE);
}


/**
 * Performs an MWAIT.
 *
 * @returns VINF_SUCCESS
 * @param   pVCpu               The cross context virtual CPU structure of the calling EMT.
 * @param   rax                 The content of RAX.
 * @param   rcx                 The content of RCX.
 */
VMM_INT_DECL(int) EMMonitorWaitPerform(PVMCPU pVCpu, uint64_t rax, uint64_t rcx)
{
    pVCpu->em.s.MWait.uMWaitRAX = rax;
    pVCpu->em.s.MWait.uMWaitRCX = rcx;
    pVCpu->em.s.MWait.fWait |= EMMWAIT_FLAG_ACTIVE;
    if (rcx)
        pVCpu->em.s.MWait.fWait |= EMMWAIT_FLAG_BREAKIRQIF0;
    else
        pVCpu->em.s.MWait.fWait &= ~EMMWAIT_FLAG_BREAKIRQIF0;
    /** @todo not completely correct?? */
    return VINF_EM_HALT;
}



/**
 * Determine if we should continue execution in HM after encountering an mwait
 * instruction.
 *
 * Clears MWAIT flags if returning @c true.
 *
 * @returns true if we should continue, false if we should halt.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            Current CPU context.
 */
VMM_INT_DECL(bool) EMMonitorWaitShouldContinue(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (   pCtx->eflags.Bits.u1IF
        || (   (pVCpu->em.s.MWait.fWait & (EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0))
            ==                            (EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0)) )
    {
        if (VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_UPDATE_APIC | VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC)))
        {
            pVCpu->em.s.MWait.fWait &= ~(EMMWAIT_FLAG_ACTIVE | EMMWAIT_FLAG_BREAKIRQIF0);
            return true;
        }
    }

    return false;
}


/**
 * Determine if we should continue execution in HM after encountering a hlt
 * instruction.
 *
 * @returns true if we should continue, false if we should halt.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pCtx            Current CPU context.
 */
VMM_INT_DECL(bool) EMShouldContinueAfterHalt(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /** @todo Shouldn't we be checking GIF here? */
    if (pCtx->eflags.Bits.u1IF)
        return VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_UPDATE_APIC | VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC));
    return false;
}


/**
 * Unhalts and wakes up the given CPU.
 *
 * This is an API for assisting the KVM hypercall API in implementing KICK_CPU.
 * It sets VMCPU_FF_UNHALT for @a pVCpuDst and makes sure it is woken up.   If
 * the CPU isn't currently in a halt, the next HLT instruction it executes will
 * be affected.
 *
 * @returns GVMMR0SchedWakeUpEx result or VINF_SUCCESS depending on context.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpuDst        The cross context virtual CPU structure of the
 *                          CPU to unhalt and wake up.  This is usually not the
 *                          same as the caller.
 * @thread  EMT
 */
VMM_INT_DECL(int) EMUnhaltAndWakeUp(PVM pVM, PVMCPU pVCpuDst)
{
    /*
     * Flag the current(/next) HLT to unhalt immediately.
     */
    VMCPU_FF_SET(pVCpuDst, VMCPU_FF_UNHALT);

    /*
     * Wake up the EMT (technically should be abstracted by VMM/VMEmt, but
     * just do it here for now).
     */
#ifdef IN_RING0
    /* We might be here with preemption disabled or enabled (i.e. depending on
       thread-context hooks being used), so don't try obtaining the GVMMR0 used
       lock here. See @bugref{7270#c148}. */
    int rc = GVMMR0SchedWakeUpNoGVMNoLock(pVM, pVCpuDst->idCpu);
    AssertRC(rc);

#elif defined(IN_RING3)
    int rc = SUPR3CallVMMR0(pVM->pVMR0, pVCpuDst->idCpu, VMMR0_DO_GVMM_SCHED_WAKE_UP, NULL /* pvArg */);
    AssertRC(rc);

#else
    /* Nothing to do for raw-mode, shouldn't really be used by raw-mode guests anyway. */
    Assert(pVM->cCpus == 1); NOREF(pVM);
    int rc = VINF_SUCCESS;
#endif
    return rc;
}

#ifndef IN_RING3

/**
 * Makes an I/O port write pending for ring-3 processing.
 *
 * @returns VINF_EM_PENDING_R3_IOPORT_READ
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uPort           The I/O port.
 * @param   cbInstr         The instruction length (for RIP updating).
 * @param   cbValue         The write size.
 * @param   uValue          The value being written.
 * @sa      emR3ExecutePendingIoPortWrite
 *
 * @note    Must not be used when I/O port breakpoints are pending or when single stepping.
 */
VMMRZ_INT_DECL(VBOXSTRICTRC)
EMRZSetPendingIoPortWrite(PVMCPU pVCpu, RTIOPORT uPort, uint8_t cbInstr, uint8_t cbValue, uint32_t uValue)
{
    Assert(pVCpu->em.s.PendingIoPortAccess.cbValue == 0);
    pVCpu->em.s.PendingIoPortAccess.uPort     = uPort;
    pVCpu->em.s.PendingIoPortAccess.cbValue   = cbValue;
    pVCpu->em.s.PendingIoPortAccess.cbInstr   = cbInstr;
    pVCpu->em.s.PendingIoPortAccess.uValue    = uValue;
    return VINF_EM_PENDING_R3_IOPORT_WRITE;
}


/**
 * Makes an I/O port read pending for ring-3 processing.
 *
 * @returns VINF_EM_PENDING_R3_IOPORT_READ
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uPort           The I/O port.
 * @param   cbInstr         The instruction length (for RIP updating).
 * @param   cbValue         The read size.
 * @sa      emR3ExecutePendingIoPortRead
 *
 * @note    Must not be used when I/O port breakpoints are pending or when single stepping.
 */
VMMRZ_INT_DECL(VBOXSTRICTRC)
EMRZSetPendingIoPortRead(PVMCPU pVCpu, RTIOPORT uPort, uint8_t cbInstr, uint8_t cbValue)
{
    Assert(pVCpu->em.s.PendingIoPortAccess.cbValue == 0);
    pVCpu->em.s.PendingIoPortAccess.uPort     = uPort;
    pVCpu->em.s.PendingIoPortAccess.cbValue   = cbValue;
    pVCpu->em.s.PendingIoPortAccess.cbInstr   = cbInstr;
    pVCpu->em.s.PendingIoPortAccess.uValue    = UINT32_C(0x52454144); /* 'READ' */
    return VINF_EM_PENDING_R3_IOPORT_READ;
}

#endif /* IN_RING3 */


/**
 * Worker for EMHistoryExec that checks for ring-3 returns and flags
 * continuation of the EMHistoryExec run there.
 */
DECL_FORCE_INLINE(void) emHistoryExecSetContinueExitRecIdx(PVMCPU pVCpu, VBOXSTRICTRC rcStrict, PCEMEXITREC pExitRec)
{
    pVCpu->em.s.idxContinueExitRec = UINT16_MAX;
#ifdef IN_RING3
    RT_NOREF_PV(rcStrict); RT_NOREF_PV(pExitRec);
#else
    switch (VBOXSTRICTRC_VAL(rcStrict))
    {
        case VINF_SUCCESS:
        default:
            break;

        /*
         * Only status codes that EMHandleRCTmpl.h will resume EMHistoryExec with.
         */
        case VINF_IOM_R3_IOPORT_READ:           /* -> emR3ExecuteIOInstruction */
        case VINF_IOM_R3_IOPORT_WRITE:          /* -> emR3ExecuteIOInstruction */
        case VINF_IOM_R3_IOPORT_COMMIT_WRITE:   /* -> VMCPU_FF_IOM -> VINF_EM_RESUME_R3_HISTORY_EXEC -> emR3ExecuteIOInstruction */
        case VINF_IOM_R3_MMIO_READ:             /* -> emR3ExecuteInstruction */
        case VINF_IOM_R3_MMIO_WRITE:            /* -> emR3ExecuteInstruction */
        case VINF_IOM_R3_MMIO_READ_WRITE:       /* -> emR3ExecuteInstruction */
        case VINF_IOM_R3_MMIO_COMMIT_WRITE:     /* -> VMCPU_FF_IOM -> VINF_EM_RESUME_R3_HISTORY_EXEC -> emR3ExecuteIOInstruction */
        case VINF_CPUM_R3_MSR_READ:             /* -> emR3ExecuteInstruction */
        case VINF_CPUM_R3_MSR_WRITE:            /* -> emR3ExecuteInstruction */
        case VINF_GIM_R3_HYPERCALL:             /* -> emR3ExecuteInstruction */
            pVCpu->em.s.idxContinueExitRec = (uint16_t)(pExitRec - &pVCpu->em.s.aExitRecords[0]);
            break;
    }
#endif /* !IN_RING3 */
}

#ifndef IN_RC

/**
 * Execute using history.
 *
 * This function will be called when EMHistoryAddExit() and friends returns a
 * non-NULL result.  This happens in response to probing or when probing has
 * uncovered adjacent exits which can more effectively be reached by using IEM
 * than restarting execution using the main execution engine and fielding an
 * regular exit.
 *
 * @returns VBox strict status code, see IEMExecForExits.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pExitRec        The exit record return by a previous history add
 *                          or update call.
 * @param   fWillExit       Flags indicating to IEM what will cause exits, TBD.
 */
VMM_INT_DECL(VBOXSTRICTRC) EMHistoryExec(PVMCPU pVCpu, PCEMEXITREC pExitRec, uint32_t fWillExit)
{
    Assert(pExitRec);
    VMCPU_ASSERT_EMT(pVCpu);
    IEMEXECFOREXITSTATS ExecStats;
    switch (pExitRec->enmAction)
    {
        /*
         * Executes multiple instruction stopping only when we've gone a given
         * number without perceived exits.
         */
        case EMEXITACTION_EXEC_WITH_MAX:
        {
            STAM_REL_PROFILE_START(&pVCpu->em.s.StatHistoryExec, a);
            LogFlow(("EMHistoryExec/EXEC_WITH_MAX: %RX64, max %u\n", pExitRec->uFlatPC, pExitRec->cMaxInstructionsWithoutExit));
            VBOXSTRICTRC rcStrict = IEMExecForExits(pVCpu, fWillExit,
                                                    pExitRec->cMaxInstructionsWithoutExit /* cMinInstructions*/,
                                                    pVCpu->em.s.cHistoryExecMaxInstructions,
                                                    pExitRec->cMaxInstructionsWithoutExit,
                                                    &ExecStats);
            LogFlow(("EMHistoryExec/EXEC_WITH_MAX: %Rrc cExits=%u cMaxExitDistance=%u cInstructions=%u\n",
                     VBOXSTRICTRC_VAL(rcStrict), ExecStats.cExits, ExecStats.cMaxExitDistance, ExecStats.cInstructions));
            emHistoryExecSetContinueExitRecIdx(pVCpu, rcStrict, pExitRec);

            /* Ignore instructions IEM doesn't know about. */
            if (   (   rcStrict != VERR_IEM_INSTR_NOT_IMPLEMENTED
                    && rcStrict != VERR_IEM_ASPECT_NOT_IMPLEMENTED)
                || ExecStats.cInstructions == 0)
            { /* likely */ }
            else
                rcStrict = VINF_SUCCESS;

            if (ExecStats.cExits > 1)
                STAM_REL_COUNTER_ADD(&pVCpu->em.s.StatHistoryExecSavedExits, ExecStats.cExits - 1);
            STAM_REL_COUNTER_ADD(&pVCpu->em.s.StatHistoryExecInstructions, ExecStats.cInstructions);
            STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatHistoryExec, a);
            return rcStrict;
        }

        /*
         * Probe a exit for close by exits.
         */
        case EMEXITACTION_EXEC_PROBE:
        {
            STAM_REL_PROFILE_START(&pVCpu->em.s.StatHistoryProbe, b);
            LogFlow(("EMHistoryExec/EXEC_PROBE: %RX64\n", pExitRec->uFlatPC));
            PEMEXITREC   pExitRecUnconst = (PEMEXITREC)pExitRec;
            VBOXSTRICTRC rcStrict = IEMExecForExits(pVCpu, fWillExit,
                                                    pVCpu->em.s.cHistoryProbeMinInstructions,
                                                    pVCpu->em.s.cHistoryExecMaxInstructions,
                                                    pVCpu->em.s.cHistoryProbeMaxInstructionsWithoutExit,
                                                    &ExecStats);
            LogFlow(("EMHistoryExec/EXEC_PROBE: %Rrc cExits=%u cMaxExitDistance=%u cInstructions=%u\n",
                     VBOXSTRICTRC_VAL(rcStrict), ExecStats.cExits, ExecStats.cMaxExitDistance, ExecStats.cInstructions));
            emHistoryExecSetContinueExitRecIdx(pVCpu, rcStrict, pExitRecUnconst);
            if (   ExecStats.cExits >= 2
                && RT_SUCCESS(rcStrict))
            {
                Assert(ExecStats.cMaxExitDistance > 0 && ExecStats.cMaxExitDistance <= 32);
                pExitRecUnconst->cMaxInstructionsWithoutExit = ExecStats.cMaxExitDistance;
                pExitRecUnconst->enmAction = EMEXITACTION_EXEC_WITH_MAX;
                LogFlow(("EMHistoryExec/EXEC_PROBE: -> EXEC_WITH_MAX %u\n", ExecStats.cMaxExitDistance));
                STAM_REL_COUNTER_INC(&pVCpu->em.s.StatHistoryProbedExecWithMax);
            }
#ifndef IN_RING3
            else if (   pVCpu->em.s.idxContinueExitRec != UINT16_MAX
                     && RT_SUCCESS(rcStrict))
            {
                STAM_REL_COUNTER_INC(&pVCpu->em.s.StatHistoryProbedToRing3);
                LogFlow(("EMHistoryExec/EXEC_PROBE: -> ring-3\n"));
            }
#endif
            else
            {
                pExitRecUnconst->enmAction = EMEXITACTION_NORMAL_PROBED;
                pVCpu->em.s.idxContinueExitRec = UINT16_MAX;
                LogFlow(("EMHistoryExec/EXEC_PROBE: -> PROBED\n"));
                STAM_REL_COUNTER_INC(&pVCpu->em.s.StatHistoryProbedNormal);
                if (   rcStrict == VERR_IEM_INSTR_NOT_IMPLEMENTED
                    || rcStrict == VERR_IEM_ASPECT_NOT_IMPLEMENTED)
                    rcStrict = VINF_SUCCESS;
            }
            STAM_REL_COUNTER_ADD(&pVCpu->em.s.StatHistoryProbeInstructions, ExecStats.cInstructions);
            STAM_REL_PROFILE_STOP(&pVCpu->em.s.StatHistoryProbe, b);
            return rcStrict;
        }

        /* We shouldn't ever see these here! */
        case EMEXITACTION_FREE_RECORD:
        case EMEXITACTION_NORMAL:
        case EMEXITACTION_NORMAL_PROBED:
            break;

        /* No default case, want compiler warnings. */
    }
    AssertLogRelFailedReturn(VERR_EM_INTERNAL_ERROR);
}


/**
 * Worker for emHistoryAddOrUpdateRecord.
 */
DECL_FORCE_INLINE(PCEMEXITREC) emHistoryRecordInit(PEMEXITREC pExitRec, uint64_t uFlatPC, uint32_t uFlagsAndType, uint64_t uExitNo)
{
    pExitRec->uFlatPC                     = uFlatPC;
    pExitRec->uFlagsAndType               = uFlagsAndType;
    pExitRec->enmAction                   = EMEXITACTION_NORMAL;
    pExitRec->bUnused                     = 0;
    pExitRec->cMaxInstructionsWithoutExit = 64;
    pExitRec->uLastExitNo                 = uExitNo;
    pExitRec->cHits                       = 1;
    return NULL;
}


/**
 * Worker for emHistoryAddOrUpdateRecord.
 */
DECL_FORCE_INLINE(PCEMEXITREC) emHistoryRecordInitNew(PVMCPU pVCpu, PEMEXITENTRY pHistEntry, uintptr_t idxSlot,
                                                      PEMEXITREC pExitRec, uint64_t uFlatPC,
                                                      uint32_t uFlagsAndType, uint64_t uExitNo)
{
    pHistEntry->idxSlot = (uint32_t)idxSlot;
    pVCpu->em.s.cExitRecordUsed++;
    LogFlow(("emHistoryRecordInitNew: [%#x] = %#07x %016RX64; (%u of %u used)\n", idxSlot, uFlagsAndType, uFlatPC,
             pVCpu->em.s.cExitRecordUsed, RT_ELEMENTS(pVCpu->em.s.aExitRecords) ));
    return emHistoryRecordInit(pExitRec, uFlatPC, uFlagsAndType, uExitNo);
}


/**
 * Worker for emHistoryAddOrUpdateRecord.
 */
DECL_FORCE_INLINE(PCEMEXITREC) emHistoryRecordInitReplacement(PEMEXITENTRY pHistEntry, uintptr_t idxSlot,
                                                              PEMEXITREC pExitRec, uint64_t uFlatPC,
                                                              uint32_t uFlagsAndType, uint64_t uExitNo)
{
    pHistEntry->idxSlot = (uint32_t)idxSlot;
    LogFlow(("emHistoryRecordInitReplacement: [%#x] = %#07x %016RX64 replacing %#07x %016RX64 with %u hits, %u exits old\n",
             idxSlot, uFlagsAndType, uFlatPC, pExitRec->uFlagsAndType, pExitRec->uFlatPC, pExitRec->cHits,
             uExitNo - pExitRec->uLastExitNo));
    return emHistoryRecordInit(pExitRec, uFlatPC, uFlagsAndType, uExitNo);
}


/**
 * Adds or updates the EMEXITREC for this PC/type and decide on an action.
 *
 * @returns Pointer to an exit record if special action should be taken using
 *          EMHistoryExec().  Take normal exit action when NULL.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFlagsAndType   Combined flags and type, EMEXIT_F_KIND_EM set and
 *                          both EMEXIT_F_CS_EIP and EMEXIT_F_UNFLATTENED_PC are clear.
 * @param   uFlatPC         The flattened program counter.
 * @param   pHistEntry      The exit history entry.
 * @param   uExitNo         The current exit number.
 */
static PCEMEXITREC emHistoryAddOrUpdateRecord(PVMCPU pVCpu, uint64_t uFlagsAndType, uint64_t uFlatPC,
                                              PEMEXITENTRY pHistEntry, uint64_t uExitNo)
{
# ifdef IN_RING0
    /* Disregard the hm flag. */
    uFlagsAndType &= ~EMEXIT_F_HM;
# endif

    /*
     * Work the hash table.
     */
    AssertCompile(RT_ELEMENTS(pVCpu->em.s.aExitRecords) == 1024);
# define EM_EXIT_RECORDS_IDX_MASK 0x3ff
    uintptr_t  idxSlot  = ((uintptr_t)uFlatPC >> 1) & EM_EXIT_RECORDS_IDX_MASK;
    PEMEXITREC pExitRec = &pVCpu->em.s.aExitRecords[idxSlot];
    if (pExitRec->uFlatPC == uFlatPC)
    {
        Assert(pExitRec->enmAction != EMEXITACTION_FREE_RECORD);
        pHistEntry->idxSlot = (uint32_t)idxSlot;
        if (pExitRec->uFlagsAndType == uFlagsAndType)
        {
            pExitRec->uLastExitNo = uExitNo;
            STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecHits[0]);
        }
        else
        {
            STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecTypeChanged[0]);
            return emHistoryRecordInit(pExitRec, uFlatPC, uFlagsAndType, uExitNo);
        }
    }
    else if (pExitRec->enmAction == EMEXITACTION_FREE_RECORD)
    {
        STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecNew[0]);
        return emHistoryRecordInitNew(pVCpu, pHistEntry, idxSlot, pExitRec, uFlatPC, uFlagsAndType, uExitNo);
    }
    else
    {
        /*
         * Collision.  We calculate a new hash for stepping away from the first,
         * doing up to 8 steps away before replacing the least recently used record.
         */
        uintptr_t idxOldest     = idxSlot;
        uint64_t  uOldestExitNo = pExitRec->uLastExitNo;
        unsigned  iOldestStep   = 0;
        unsigned  iStep         = 1;
        uintptr_t const idxAdd  = (uintptr_t)(uFlatPC >> 11) & (EM_EXIT_RECORDS_IDX_MASK / 4);
        for (;;)
        {
            Assert(iStep < RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecHits));
            AssertCompile(RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecNew)         == RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecHits));
            AssertCompile(RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecReplaced)    == RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecHits));
            AssertCompile(RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecTypeChanged) == RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecHits));

            /* Step to the next slot. */
            idxSlot += idxAdd;
            idxSlot &= EM_EXIT_RECORDS_IDX_MASK;
            pExitRec = &pVCpu->em.s.aExitRecords[idxSlot];

            /* Does it match? */
            if (pExitRec->uFlatPC == uFlatPC)
            {
                Assert(pExitRec->enmAction != EMEXITACTION_FREE_RECORD);
                pHistEntry->idxSlot = (uint32_t)idxSlot;
                if (pExitRec->uFlagsAndType == uFlagsAndType)
                {
                    pExitRec->uLastExitNo = uExitNo;
                    STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecHits[iStep]);
                    break;
                }
                STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecTypeChanged[iStep]);
                return emHistoryRecordInit(pExitRec, uFlatPC, uFlagsAndType, uExitNo);
            }

            /* Is it free? */
            if (pExitRec->enmAction == EMEXITACTION_FREE_RECORD)
            {
                STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecNew[iStep]);
                return emHistoryRecordInitNew(pVCpu, pHistEntry, idxSlot, pExitRec, uFlatPC, uFlagsAndType, uExitNo);
            }

            /* Is it the least recently used one? */
            if (pExitRec->uLastExitNo < uOldestExitNo)
            {
                uOldestExitNo = pExitRec->uLastExitNo;
                idxOldest     = idxSlot;
                iOldestStep   = iStep;
            }

            /* Next iteration? */
            iStep++;
            Assert(iStep < RT_ELEMENTS(pVCpu->em.s.aStatHistoryRecReplaced));
            if (RT_LIKELY(iStep < 8 + 1))
            { /* likely */ }
            else
            {
                /* Replace the least recently used slot. */
                STAM_REL_COUNTER_INC(&pVCpu->em.s.aStatHistoryRecReplaced[iOldestStep]);
                pExitRec = &pVCpu->em.s.aExitRecords[idxOldest];
                return emHistoryRecordInitReplacement(pHistEntry, idxOldest, pExitRec, uFlatPC, uFlagsAndType, uExitNo);
            }
        }
    }

    /*
     * Found an existing record.
     */
    switch (pExitRec->enmAction)
    {
        case EMEXITACTION_NORMAL:
        {
            uint64_t const cHits = ++pExitRec->cHits;
            if (cHits < 256)
                return NULL;
            LogFlow(("emHistoryAddOrUpdateRecord: [%#x] %#07x %16RX64: -> EXEC_PROBE\n", idxSlot, uFlagsAndType, uFlatPC));
            pExitRec->enmAction = EMEXITACTION_EXEC_PROBE;
            return pExitRec;
        }

        case EMEXITACTION_NORMAL_PROBED:
            pExitRec->cHits += 1;
            return NULL;

        default:
            pExitRec->cHits += 1;
            return pExitRec;

        /* This will happen if the caller ignores or cannot serve the probe
           request (forced to ring-3, whatever).  We retry this 256 times. */
        case EMEXITACTION_EXEC_PROBE:
        {
            uint64_t const cHits = ++pExitRec->cHits;
            if (cHits < 512)
                return pExitRec;
            pExitRec->enmAction = EMEXITACTION_NORMAL_PROBED;
            LogFlow(("emHistoryAddOrUpdateRecord: [%#x] %#07x %16RX64: -> PROBED\n", idxSlot, uFlagsAndType, uFlatPC));
            return NULL;
        }
    }
}

#endif /* !IN_RC */

/**
 * Adds an exit to the history for this CPU.
 *
 * @returns Pointer to an exit record if special action should be taken using
 *          EMHistoryExec().  Take normal exit action when NULL.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFlagsAndType   Combined flags and type (see EMEXIT_MAKE_FLAGS_AND_TYPE).
 * @param   uFlatPC         The flattened program counter (RIP).  UINT64_MAX if not available.
 * @param   uTimestamp      The TSC value for the exit, 0 if not available.
 * @thread  EMT(pVCpu)
 */
VMM_INT_DECL(PCEMEXITREC) EMHistoryAddExit(PVMCPU pVCpu, uint32_t uFlagsAndType, uint64_t uFlatPC, uint64_t uTimestamp)
{
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Add the exit history entry.
     */
    AssertCompile(RT_ELEMENTS(pVCpu->em.s.aExitHistory) == 256);
    uint64_t uExitNo = pVCpu->em.s.iNextExit++;
    PEMEXITENTRY pHistEntry = &pVCpu->em.s.aExitHistory[(uintptr_t)uExitNo & 0xff];
    pHistEntry->uFlatPC       = uFlatPC;
    pHistEntry->uTimestamp    = uTimestamp;
    pHistEntry->uFlagsAndType = uFlagsAndType;
    pHistEntry->idxSlot       = UINT32_MAX;

#ifndef IN_RC
    /*
     * If common exit type, we will insert/update the exit into the exit record hash table.
     */
    if (   (uFlagsAndType & (EMEXIT_F_KIND_MASK | EMEXIT_F_CS_EIP | EMEXIT_F_UNFLATTENED_PC)) == EMEXIT_F_KIND_EM
# ifdef IN_RING0
        && pVCpu->em.s.fExitOptimizationEnabledR0
        && ( !(uFlagsAndType & EMEXIT_F_HM) || pVCpu->em.s.fExitOptimizationEnabledR0PreemptDisabled)
# else
        && pVCpu->em.s.fExitOptimizationEnabled
# endif
        && uFlatPC != UINT64_MAX
       )
        return emHistoryAddOrUpdateRecord(pVCpu, uFlagsAndType, uFlatPC, pHistEntry, uExitNo);
#endif
    return NULL;
}


#ifdef IN_RC
/**
 * Special raw-mode interface for adding an exit to the history.
 *
 * Currently this is only for recording, not optimizing, so no return value.  If
 * we start seriously caring about raw-mode again, we may extend it.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFlagsAndType   Combined flags and type (see EMEXIT_MAKE_FLAGS_AND_TYPE).
 * @param   uCs             The CS.
 * @param   uEip            The EIP.
 * @param   uTimestamp      The TSC value for the exit, 0 if not available.
 * @thread  EMT(0)
 */
VMMRC_INT_DECL(void) EMRCHistoryAddExitCsEip(PVMCPU pVCpu, uint32_t uFlagsAndType, uint16_t uCs, uint32_t uEip, uint64_t uTimestamp)
{
    AssertCompile(RT_ELEMENTS(pVCpu->em.s.aExitHistory) == 256);
    PEMEXITENTRY pHistEntry = &pVCpu->em.s.aExitHistory[(uintptr_t)(pVCpu->em.s.iNextExit++) & 0xff];
    pHistEntry->uFlatPC       = ((uint64_t)uCs << 32) |  uEip;
    pHistEntry->uTimestamp    = uTimestamp;
    pHistEntry->uFlagsAndType = uFlagsAndType | EMEXIT_F_CS_EIP;
    pHistEntry->idxSlot       = UINT32_MAX;
}
#endif


#ifdef IN_RING0
/**
 * Interface that VT-x uses to supply the PC of an exit when CS:RIP is being read.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFlatPC         The flattened program counter (RIP).
 * @param   fFlattened      Set if RIP was subjected to CS.BASE, clear if not.
 */
VMMR0_INT_DECL(void) EMR0HistoryUpdatePC(PVMCPU pVCpu, uint64_t uFlatPC, bool fFlattened)
{
    AssertCompile(RT_ELEMENTS(pVCpu->em.s.aExitHistory) == 256);
    uint64_t     uExitNo    = pVCpu->em.s.iNextExit - 1;
    PEMEXITENTRY pHistEntry = &pVCpu->em.s.aExitHistory[(uintptr_t)uExitNo & 0xff];
    pHistEntry->uFlatPC = uFlatPC;
    if (fFlattened)
        pHistEntry->uFlagsAndType &= ~EMEXIT_F_UNFLATTENED_PC;
    else
        pHistEntry->uFlagsAndType |= EMEXIT_F_UNFLATTENED_PC;
}
#endif


/**
 * Interface for convering a engine specific exit to a generic one and get guidance.
 *
 * @returns Pointer to an exit record if special action should be taken using
 *          EMHistoryExec().  Take normal exit action when NULL.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFlagsAndType   Combined flags and type (see EMEXIT_MAKE_FLAGS_AND_TYPE).
 * @thread  EMT(pVCpu)
 */
VMM_INT_DECL(PCEMEXITREC) EMHistoryUpdateFlagsAndType(PVMCPU pVCpu, uint32_t uFlagsAndType)
{
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Do the updating.
     */
    AssertCompile(RT_ELEMENTS(pVCpu->em.s.aExitHistory) == 256);
    uint64_t     uExitNo    = pVCpu->em.s.iNextExit - 1;
    PEMEXITENTRY pHistEntry = &pVCpu->em.s.aExitHistory[(uintptr_t)uExitNo & 0xff];
    pHistEntry->uFlagsAndType = uFlagsAndType | (pHistEntry->uFlagsAndType & (EMEXIT_F_CS_EIP | EMEXIT_F_UNFLATTENED_PC));

#ifndef IN_RC
    /*
     * If common exit type, we will insert/update the exit into the exit record hash table.
     */
    if (   (uFlagsAndType & (EMEXIT_F_KIND_MASK | EMEXIT_F_CS_EIP | EMEXIT_F_UNFLATTENED_PC)) == EMEXIT_F_KIND_EM
# ifdef IN_RING0
        && pVCpu->em.s.fExitOptimizationEnabledR0
        && ( !(uFlagsAndType & EMEXIT_F_HM) || pVCpu->em.s.fExitOptimizationEnabledR0PreemptDisabled)
# else
        && pVCpu->em.s.fExitOptimizationEnabled
# endif
        && pHistEntry->uFlatPC != UINT64_MAX
       )
        return emHistoryAddOrUpdateRecord(pVCpu, uFlagsAndType, pHistEntry->uFlatPC, pHistEntry, uExitNo);
#endif
    return NULL;
}


/**
 * Interface for convering a engine specific exit to a generic one and get
 * guidance, supplying flattened PC too.
 *
 * @returns Pointer to an exit record if special action should be taken using
 *          EMHistoryExec().  Take normal exit action when NULL.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uFlagsAndType   Combined flags and type (see EMEXIT_MAKE_FLAGS_AND_TYPE).
 * @param   uFlatPC         The flattened program counter (RIP).
 * @thread  EMT(pVCpu)
 */
VMM_INT_DECL(PCEMEXITREC) EMHistoryUpdateFlagsAndTypeAndPC(PVMCPU pVCpu, uint32_t uFlagsAndType, uint64_t uFlatPC)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(uFlatPC != UINT64_MAX);

    /*
     * Do the updating.
     */
    AssertCompile(RT_ELEMENTS(pVCpu->em.s.aExitHistory) == 256);
    uint64_t     uExitNo    = pVCpu->em.s.iNextExit - 1;
    PEMEXITENTRY pHistEntry = &pVCpu->em.s.aExitHistory[(uintptr_t)uExitNo & 0xff];
    pHistEntry->uFlagsAndType = uFlagsAndType;
    pHistEntry->uFlatPC       = uFlatPC;

#ifndef IN_RC
    /*
     * If common exit type, we will insert/update the exit into the exit record hash table.
     */
    if (   (uFlagsAndType & (EMEXIT_F_KIND_MASK | EMEXIT_F_CS_EIP | EMEXIT_F_UNFLATTENED_PC)) == EMEXIT_F_KIND_EM
# ifdef IN_RING0
        && pVCpu->em.s.fExitOptimizationEnabledR0
        && ( !(uFlagsAndType & EMEXIT_F_HM) || pVCpu->em.s.fExitOptimizationEnabledR0PreemptDisabled)
# else
        && pVCpu->em.s.fExitOptimizationEnabled
# endif
       )
        return emHistoryAddOrUpdateRecord(pVCpu, uFlagsAndType, uFlatPC, pHistEntry, uExitNo);
#endif
    return NULL;
}


/**
 * Locks REM execution to a single VCPU.
 *
 * @param   pVM         The cross context VM structure.
 */
VMMDECL(void) EMRemLock(PVM pVM)
{
#ifdef VBOX_WITH_REM
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return;     /* early init */

    Assert(!PGMIsLockOwner(pVM));
    Assert(!IOMIsLockWriteOwner(pVM));
    int rc = PDMCritSectEnter(&pVM->em.s.CritSectREM, VERR_SEM_BUSY);
    AssertRCSuccess(rc);
#else
    RT_NOREF(pVM);
#endif
}


/**
 * Unlocks REM execution
 *
 * @param   pVM         The cross context VM structure.
 */
VMMDECL(void) EMRemUnlock(PVM pVM)
{
#ifdef VBOX_WITH_REM
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return;     /* early init */

    PDMCritSectLeave(&pVM->em.s.CritSectREM);
#else
    RT_NOREF(pVM);
#endif
}


/**
 * Check if this VCPU currently owns the REM lock.
 *
 * @returns bool owner/not owner
 * @param   pVM         The cross context VM structure.
 */
VMMDECL(bool) EMRemIsLockOwner(PVM pVM)
{
#ifdef VBOX_WITH_REM
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return true;   /* early init */

    return PDMCritSectIsOwner(&pVM->em.s.CritSectREM);
#else
    RT_NOREF(pVM);
    return true;
#endif
}


/**
 * Try to acquire the REM lock.
 *
 * @returns VBox status code
 * @param   pVM         The cross context VM structure.
 */
VMM_INT_DECL(int) EMRemTryLock(PVM pVM)
{
#ifdef VBOX_WITH_REM
    if (!PDMCritSectIsInitialized(&pVM->em.s.CritSectREM))
        return VINF_SUCCESS; /* early init */

    return PDMCritSectTryEnter(&pVM->em.s.CritSectREM);
#else
    RT_NOREF(pVM);
    return VINF_SUCCESS;
#endif
}


/**
 * @callback_method_impl{FNDISREADBYTES}
 */
static DECLCALLBACK(int) emReadBytes(PDISCPUSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead)
{
    PVMCPU      pVCpu    = (PVMCPU)pDis->pvUser;
#if defined(VBOX_WITH_RAW_MODE) && (defined(IN_RC) || defined(IN_RING3))
    PVM         pVM      = pVCpu->CTX_SUFF(pVM);
#endif
    RTUINTPTR   uSrcAddr = pDis->uInstrAddr + offInstr;
    int         rc;

    /*
     * Figure how much we can or must read.
     */
    size_t      cbToRead = PAGE_SIZE - (uSrcAddr & PAGE_OFFSET_MASK);
    if (cbToRead > cbMaxRead)
        cbToRead = cbMaxRead;
    else if (cbToRead < cbMinRead)
        cbToRead = cbMinRead;

#if defined(VBOX_WITH_RAW_MODE) && (defined(IN_RC) || defined(IN_RING3))
    /*
     * We might be called upon to interpret an instruction in a patch.
     */
    if (PATMIsPatchGCAddr(pVM, uSrcAddr))
    {
# ifdef IN_RC
        memcpy(&pDis->abInstr[offInstr], (void *)(uintptr_t)uSrcAddr, cbToRead);
# else
        memcpy(&pDis->abInstr[offInstr], PATMR3GCPtrToHCPtr(pVM, uSrcAddr), cbToRead);
# endif
        rc = VINF_SUCCESS;
    }
    else
#endif
    {
# ifdef IN_RC
        /*
         * Try access it thru the shadow page tables first. Fall back on the
         * slower PGM method if it fails because the TLB or page table was
         * modified recently.
         */
        rc = MMGCRamRead(pVCpu->pVMRC, &pDis->abInstr[offInstr], (void *)(uintptr_t)uSrcAddr, cbToRead);
        if (rc == VERR_ACCESS_DENIED && cbToRead > cbMinRead)
        {
            cbToRead = cbMinRead;
            rc = MMGCRamRead(pVCpu->pVMRC, &pDis->abInstr[offInstr], (void *)(uintptr_t)uSrcAddr, cbToRead);
        }
        if (rc == VERR_ACCESS_DENIED)
#endif
        {
            rc = PGMPhysSimpleReadGCPtr(pVCpu, &pDis->abInstr[offInstr], uSrcAddr, cbToRead);
            if (RT_FAILURE(rc))
            {
                if (cbToRead > cbMinRead)
                {
                    cbToRead = cbMinRead;
                    rc = PGMPhysSimpleReadGCPtr(pVCpu, &pDis->abInstr[offInstr], uSrcAddr, cbToRead);
                }
                if (RT_FAILURE(rc))
                {
#ifndef IN_RC
                    /*
                     * If we fail to find the page via the guest's page tables
                     * we invalidate the page in the host TLB (pertaining to
                     * the guest in the NestedPaging case). See @bugref{6043}.
                     */
                    if (rc == VERR_PAGE_TABLE_NOT_PRESENT || rc == VERR_PAGE_NOT_PRESENT)
                    {
                        HMInvalidatePage(pVCpu, uSrcAddr);
                        if (((uSrcAddr + cbToRead - 1) >> PAGE_SHIFT) !=  (uSrcAddr >> PAGE_SHIFT))
                            HMInvalidatePage(pVCpu, uSrcAddr + cbToRead - 1);
                    }
#endif
                }
            }
        }
    }

    pDis->cbCachedInstr = offInstr + (uint8_t)cbToRead;
    return rc;
}



/**
 * Disassembles the current instruction.
 *
 * @returns VBox status code, see SELMToFlatEx and EMInterpretDisasOneEx for
 *          details.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pDis            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
VMM_INT_DECL(int) EMInterpretDisasCurrent(PVM pVM, PVMCPU pVCpu, PDISCPUSTATE pDis, unsigned *pcbInstr)
{
    PCPUMCTXCORE pCtxCore = CPUMCTX2CORE(CPUMQueryGuestCtxPtr(pVCpu));
    RTGCPTR GCPtrInstr;
#if 0
    int rc = SELMToFlatEx(pVCpu, DISSELREG_CS, pCtxCore, pCtxCore->rip, 0, &GCPtrInstr);
#else
/** @todo Get the CPU mode as well while we're at it! */
    int rc = SELMValidateAndConvertCSAddr(pVCpu, pCtxCore->eflags, pCtxCore->ss.Sel, pCtxCore->cs.Sel, &pCtxCore->cs,
                                          pCtxCore->rip, &GCPtrInstr);
#endif
    if (RT_FAILURE(rc))
    {
        Log(("EMInterpretDisasOne: Failed to convert %RTsel:%RGv (cpl=%d) - rc=%Rrc !!\n",
             pCtxCore->cs.Sel, (RTGCPTR)pCtxCore->rip, pCtxCore->ss.Sel & X86_SEL_RPL, rc));
        return rc;
    }
    return EMInterpretDisasOneEx(pVM, pVCpu, (RTGCUINTPTR)GCPtrInstr, pCtxCore, pDis, pcbInstr);
}


/**
 * Disassembles one instruction.
 *
 * This is used by internally by the interpreter and by trap/access handlers.
 *
 * @returns VBox status code.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   GCPtrInstr      The flat address of the instruction.
 * @param   pCtxCore        The context core (used to determine the cpu mode).
 * @param   pDis            Where to return the parsed instruction info.
 * @param   pcbInstr        Where to return the instruction size. (optional)
 */
VMM_INT_DECL(int) EMInterpretDisasOneEx(PVM pVM, PVMCPU pVCpu, RTGCUINTPTR GCPtrInstr, PCCPUMCTXCORE pCtxCore,
                                        PDISCPUSTATE pDis, unsigned *pcbInstr)
{
    NOREF(pVM);
    Assert(pCtxCore == CPUMGetGuestCtxCore(pVCpu)); NOREF(pCtxCore);
    DISCPUMODE enmCpuMode = CPUMGetGuestDisMode(pVCpu);
    /** @todo Deal with too long instruction (=> \#GP), opcode read errors (=>
     *        \#PF, \#GP, \#??), undefined opcodes (=> \#UD), and such. */
    int rc = DISInstrWithReader(GCPtrInstr, enmCpuMode, emReadBytes, pVCpu, pDis, pcbInstr);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    AssertMsg(rc == VERR_PAGE_NOT_PRESENT || rc == VERR_PAGE_TABLE_NOT_PRESENT, ("DISCoreOne failed to GCPtrInstr=%RGv rc=%Rrc\n", GCPtrInstr, rc));
    return rc;
}


/**
 * Interprets the current instruction.
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 *                      Updates the EIP if an instruction was executed successfully.
 * @param   pvFault     The fault address (CR2).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 */
VMM_INT_DECL(VBOXSTRICTRC) EMInterpretInstruction(PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault)
{
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    LogFlow(("EMInterpretInstruction %RGv fault %RGv\n", (RTGCPTR)pRegFrame->rip, pvFault));
    NOREF(pvFault);

    VBOXSTRICTRC rc = IEMExecOneBypassEx(pVCpu, pRegFrame, NULL);
    if (RT_UNLIKELY(   rc == VERR_IEM_ASPECT_NOT_IMPLEMENTED
                    || rc == VERR_IEM_INSTR_NOT_IMPLEMENTED))
        rc = VERR_EM_INTERPRETER;
    if (rc != VINF_SUCCESS)
        Log(("EMInterpretInstruction: returns %Rrc\n", VBOXSTRICTRC_VAL(rc)));

    return rc;
}


/**
 * Interprets the current instruction.
 *
 * @returns VBox status code.
 * @retval  VINF_*                  Scheduling instructions.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pRegFrame   The register frame.
 *                      Updates the EIP if an instruction was executed successfully.
 * @param   pvFault     The fault address (CR2).
 * @param   pcbWritten  Size of the write (if applicable).
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 */
VMM_INT_DECL(VBOXSTRICTRC) EMInterpretInstructionEx(PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, uint32_t *pcbWritten)
{
    LogFlow(("EMInterpretInstructionEx %RGv fault %RGv\n", (RTGCPTR)pRegFrame->rip, pvFault));
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    NOREF(pvFault);

    VBOXSTRICTRC rc = IEMExecOneBypassEx(pVCpu, pRegFrame, pcbWritten);
    if (RT_UNLIKELY(   rc == VERR_IEM_ASPECT_NOT_IMPLEMENTED
                    || rc == VERR_IEM_INSTR_NOT_IMPLEMENTED))
        rc = VERR_EM_INTERPRETER;
    if (rc != VINF_SUCCESS)
        Log(("EMInterpretInstructionEx: returns %Rrc\n", VBOXSTRICTRC_VAL(rc)));

    return rc;
}


/**
 * Interprets the current instruction using the supplied DISCPUSTATE structure.
 *
 * IP/EIP/RIP *IS* updated!
 *
 * @returns VBox strict status code.
 * @retval  VINF_*                  Scheduling instructions. When these are returned, it
 *                                  starts to get a bit tricky to know whether code was
 *                                  executed or not... We'll address this when it becomes a problem.
 * @retval  VERR_EM_INTERPRETER     Something we can't cope with.
 * @retval  VERR_*                  Fatal errors.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pDis        The disassembler cpu state for the instruction to be
 *                      interpreted.
 * @param   pRegFrame   The register frame. IP/EIP/RIP *IS* changed!
 * @param   pvFault     The fault address (CR2).
 * @param   enmCodeType Code type (user/supervisor)
 *
 * @remark  Invalid opcode exceptions have a higher priority than GP (see Intel
 *          Architecture System Developers Manual, Vol 3, 5.5) so we don't need
 *          to worry about e.g. invalid modrm combinations (!)
 *
 * @todo    At this time we do NOT check if the instruction overwrites vital information.
 *          Make sure this can't happen!! (will add some assertions/checks later)
 */
VMM_INT_DECL(VBOXSTRICTRC) EMInterpretInstructionDisasState(PVMCPU pVCpu, PDISCPUSTATE pDis, PCPUMCTXCORE pRegFrame,
                                                            RTGCPTR pvFault, EMCODETYPE enmCodeType)
{
    LogFlow(("EMInterpretInstructionDisasState %RGv fault %RGv\n", (RTGCPTR)pRegFrame->rip, pvFault));
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    NOREF(pDis); NOREF(pvFault); NOREF(enmCodeType);

    VBOXSTRICTRC rc = IEMExecOneBypassWithPrefetchedByPC(pVCpu, pRegFrame, pRegFrame->rip, pDis->abInstr, pDis->cbCachedInstr);
    if (RT_UNLIKELY(   rc == VERR_IEM_ASPECT_NOT_IMPLEMENTED
                    || rc == VERR_IEM_INSTR_NOT_IMPLEMENTED))
        rc = VERR_EM_INTERPRETER;

    if (rc != VINF_SUCCESS)
        Log(("EMInterpretInstructionDisasState: returns %Rrc\n", VBOXSTRICTRC_VAL(rc)));

    return rc;
}

#ifdef IN_RC

DECLINLINE(int) emRCStackRead(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCPTR GCPtrSrc, uint32_t cb)
{
    int rc = MMGCRamRead(pVM, pvDst, (void *)(uintptr_t)GCPtrSrc, cb);
    if (RT_LIKELY(rc != VERR_ACCESS_DENIED))
        return rc;
    return PGMPhysInterpretedReadNoHandlers(pVCpu, pCtxCore, pvDst, GCPtrSrc, cb, /*fMayTrap*/ false);
}


/**
 * Interpret IRET (currently only to V86 code) - PATM only.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 *
 */
VMM_INT_DECL(int) EMInterpretIretV86ForPatm(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    RTGCUINTPTR pIretStack = (RTGCUINTPTR)pRegFrame->esp;
    RTGCUINTPTR eip, cs, esp, ss, eflags, ds, es, fs, gs, uMask;
    int         rc;

    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    Assert(!CPUMIsGuestIn64BitCode(pVCpu));
    /** @todo Rainy day: Test what happens when VERR_EM_INTERPRETER is returned by
     *        this function.  Fear that it may guru on us, thus not converted to
     *        IEM. */

    rc  = emRCStackRead(pVM, pVCpu, pRegFrame, &eip,      (RTGCPTR)pIretStack      , 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &cs,       (RTGCPTR)(pIretStack + 4), 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &eflags,   (RTGCPTR)(pIretStack + 8), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);
    AssertReturn(eflags & X86_EFL_VM, VERR_EM_INTERPRETER);

    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &esp,      (RTGCPTR)(pIretStack + 12), 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &ss,       (RTGCPTR)(pIretStack + 16), 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &es,       (RTGCPTR)(pIretStack + 20), 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &ds,       (RTGCPTR)(pIretStack + 24), 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &fs,       (RTGCPTR)(pIretStack + 28), 4);
    rc |= emRCStackRead(pVM, pVCpu, pRegFrame, &gs,       (RTGCPTR)(pIretStack + 32), 4);
    AssertRCReturn(rc, VERR_EM_INTERPRETER);

    pRegFrame->eip    = eip & 0xffff;
    pRegFrame->cs.Sel = cs;

    /* Mask away all reserved bits */
    uMask = X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT | X86_EFL_RF | X86_EFL_VM | X86_EFL_AC | X86_EFL_VIF | X86_EFL_VIP | X86_EFL_ID;
    eflags &= uMask;

    CPUMRawSetEFlags(pVCpu, eflags);
    Assert((pRegFrame->eflags.u32 & (X86_EFL_IF|X86_EFL_IOPL)) == X86_EFL_IF);

    pRegFrame->esp      = esp;
    pRegFrame->ss.Sel   = ss;
    pRegFrame->ds.Sel   = ds;
    pRegFrame->es.Sel   = es;
    pRegFrame->fs.Sel   = fs;
    pRegFrame->gs.Sel   = gs;

    return VINF_SUCCESS;
}


#endif /* IN_RC */



/*
 *
 * Old interpreter primitives used by HM, move/eliminate later.
 * Old interpreter primitives used by HM, move/eliminate later.
 * Old interpreter primitives used by HM, move/eliminate later.
 * Old interpreter primitives used by HM, move/eliminate later.
 * Old interpreter primitives used by HM, move/eliminate later.
 *
 */


/**
 * Interpret CPUID given the parameters in the CPU context.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 *
 */
VMM_INT_DECL(int) EMInterpretCpuId(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    uint32_t iLeaf    = pRegFrame->eax;
    uint32_t iSubLeaf = pRegFrame->ecx;
    NOREF(pVM);

    /* cpuid clears the high dwords of the affected 64 bits registers. */
    pRegFrame->rax = 0;
    pRegFrame->rbx = 0;
    pRegFrame->rcx = 0;
    pRegFrame->rdx = 0;

    /* Note: operates the same in 64 and non-64 bits mode. */
    CPUMGetGuestCpuId(pVCpu, iLeaf, iSubLeaf, &pRegFrame->eax, &pRegFrame->ebx, &pRegFrame->ecx, &pRegFrame->edx);
    Log(("Emulate: CPUID %x/%x -> %08x %08x %08x %08x\n", iLeaf, iSubLeaf, pRegFrame->eax, pRegFrame->ebx, pRegFrame->ecx, pRegFrame->edx));
    return VINF_SUCCESS;
}


/**
 * Interpret RDPMC.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 *
 */
VMM_INT_DECL(int) EMInterpretRdpmc(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    uint32_t uCR4 = CPUMGetGuestCR4(pVCpu);

    /* If X86_CR4_PCE is not set, then CPL must be zero. */
    if (    !(uCR4 & X86_CR4_PCE)
        &&  CPUMGetGuestCPL(pVCpu) != 0)
    {
        Assert(CPUMGetGuestCR0(pVCpu) & X86_CR0_PE);
        return VERR_EM_INTERPRETER; /* genuine #GP */
    }

    /* Just return zero here; rather tricky to properly emulate this, especially as the specs are a mess. */
    pRegFrame->rax = 0;
    pRegFrame->rdx = 0;
    /** @todo We should trigger a \#GP here if the CPU doesn't support the index in
     *        ecx but see @bugref{3472}! */

    NOREF(pVM);
    return VINF_SUCCESS;
}


/**
 * MWAIT Emulation.
 */
VMM_INT_DECL(VBOXSTRICTRC) EMInterpretMWait(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    uint32_t u32Dummy, u32ExtFeatures, cpl, u32MWaitFeatures;
    NOREF(pVM);

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVCpu);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVCpu, 1, 0, &u32Dummy, &u32Dummy, &u32ExtFeatures, &u32Dummy);
    if (!(u32ExtFeatures & X86_CPUID_FEATURE_ECX_MONITOR))
        return VERR_EM_INTERPRETER; /* not supported */

    /*
     * CPUID.05H.ECX[0] defines support for power management extensions (eax)
     * CPUID.05H.ECX[1] defines support for interrupts as break events for mwait even when IF=0
     */
    CPUMGetGuestCpuId(pVCpu, 5, 0, &u32Dummy, &u32Dummy, &u32MWaitFeatures, &u32Dummy);
    if (pRegFrame->ecx > 1)
    {
        Log(("EMInterpretMWait: unexpected ecx value %x -> recompiler\n", pRegFrame->ecx));
        return VERR_EM_INTERPRETER; /* illegal value. */
    }

    if (pRegFrame->ecx && !(u32MWaitFeatures & X86_CPUID_MWAIT_ECX_BREAKIRQIF0))
    {
        Log(("EMInterpretMWait: unsupported X86_CPUID_MWAIT_ECX_BREAKIRQIF0 -> recompiler\n"));
        return VERR_EM_INTERPRETER; /* illegal value. */
    }

    return EMMonitorWaitPerform(pVCpu, pRegFrame->rax, pRegFrame->rcx);
}


/**
 * MONITOR Emulation.
 */
VMM_INT_DECL(int) EMInterpretMonitor(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    uint32_t u32Dummy, u32ExtFeatures, cpl;
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    NOREF(pVM);

    if (pRegFrame->ecx != 0)
    {
        Log(("emInterpretMonitor: unexpected ecx=%x -> recompiler!!\n", pRegFrame->ecx));
        return VERR_EM_INTERPRETER; /* illegal value. */
    }

    /* Get the current privilege level. */
    cpl = CPUMGetGuestCPL(pVCpu);
    if (cpl != 0)
        return VERR_EM_INTERPRETER; /* supervisor only */

    CPUMGetGuestCpuId(pVCpu, 1, 0, &u32Dummy, &u32Dummy, &u32ExtFeatures, &u32Dummy);
    if (!(u32ExtFeatures & X86_CPUID_FEATURE_ECX_MONITOR))
        return VERR_EM_INTERPRETER; /* not supported */

    EMMonitorWaitPrepare(pVCpu, pRegFrame->rax, pRegFrame->rcx, pRegFrame->rdx, NIL_RTGCPHYS);
    return VINF_SUCCESS;
}


/* VT-x only: */

/**
 * Interpret INVLPG.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 * @param   pAddrGC     Operand address.
 *
 */
VMM_INT_DECL(VBOXSTRICTRC) EMInterpretInvlpg(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCPTR pAddrGC)
{
    /** @todo is addr always a flat linear address or ds based
     * (in absence of segment override prefixes)????
     */
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    NOREF(pVM); NOREF(pRegFrame);
#ifdef IN_RC
    LogFlow(("RC: EMULATE: invlpg %RGv\n", pAddrGC));
#endif
    VBOXSTRICTRC rc = PGMInvalidatePage(pVCpu, pAddrGC);
    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_SYNC_CR3 /* we can rely on the FF */)
        return VINF_SUCCESS;
    AssertMsgReturn(rc == VINF_EM_RAW_EMULATE_INSTR,
                    ("%Rrc addr=%RGv\n", VBOXSTRICTRC_VAL(rc), pAddrGC),
                    VERR_EM_INTERPRETER);
    return rc;
}


/**
 * Interpret DRx write.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 * @param   DestRegDrx  DRx register index (USE_REG_DR*)
 * @param   SrcRegGen   General purpose register index (USE_REG_E**))
 *
 */
VMM_INT_DECL(int) EMInterpretDRxWrite(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegDrx, uint32_t SrcRegGen)
{
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    uint64_t uNewDrX;
    int      rc;
    NOREF(pVM);

    if (CPUMIsGuestIn64BitCode(pVCpu))
        rc = DISFetchReg64(pRegFrame, SrcRegGen, &uNewDrX);
    else
    {
        uint32_t val32;
        rc = DISFetchReg32(pRegFrame, SrcRegGen, &val32);
        uNewDrX = val32;
    }

    if (RT_SUCCESS(rc))
    {
        if (DestRegDrx == 6)
        {
            uNewDrX |= X86_DR6_RA1_MASK;
            uNewDrX &= ~X86_DR6_RAZ_MASK;
        }
        else if (DestRegDrx == 7)
        {
            uNewDrX |= X86_DR7_RA1_MASK;
            uNewDrX &= ~X86_DR7_RAZ_MASK;
        }

        /** @todo we don't fail if illegal bits are set/cleared for e.g. dr7 */
        rc = CPUMSetGuestDRx(pVCpu, DestRegDrx, uNewDrX);
        if (RT_SUCCESS(rc))
            return rc;
        AssertMsgFailed(("CPUMSetGuestDRx %d failed\n", DestRegDrx));
    }
    return VERR_EM_INTERPRETER;
}


/**
 * Interpret DRx read.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pRegFrame   The register frame.
 * @param   DestRegGen  General purpose register index (USE_REG_E**))
 * @param   SrcRegDrx   DRx register index (USE_REG_DR*)
 */
VMM_INT_DECL(int) EMInterpretDRxRead(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, uint32_t DestRegGen, uint32_t SrcRegDrx)
{
    uint64_t val64;
    Assert(pRegFrame == CPUMGetGuestCtxCore(pVCpu));
    NOREF(pVM);

    int rc = CPUMGetGuestDRx(pVCpu, SrcRegDrx, &val64);
    AssertMsgRCReturn(rc, ("CPUMGetGuestDRx %d failed\n", SrcRegDrx), VERR_EM_INTERPRETER);
    if (CPUMIsGuestIn64BitCode(pVCpu))
        rc = DISWriteReg64(pRegFrame, DestRegGen, val64);
    else
        rc = DISWriteReg32(pRegFrame, DestRegGen, (uint32_t)val64);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    return VERR_EM_INTERPRETER;
}

