/* $Id$ */
/** @file
 * EM - Execution Monitor / Manager - hardware virtualization
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_em         EM - The Execution Monitor / Manager
 *
 * The Execution Monitor/Manager is responsible for running the VM, scheduling
 * the right kind of execution (Raw-mode, Hardware Assisted, Recompiled or
 * Interpreted), and keeping the CPU states in sync. The function
 * EMR3ExecuteVM() is the 'main-loop' of the VM, while each of the execution
 * modes has different inner loops (emR3RawExecute, emR3HwAccExecute, and
 * emR3RemExecute).
 *
 * The interpreted execution is only used to avoid switching between
 * raw-mode/hwaccm and the recompiler when fielding virtualization traps/faults.
 * The interpretation is thus implemented as part of EM.
 *
 * @see grp_em
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/vmm/em.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/rem.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/pdmqueue.h>
#include <VBox/vmm/hwaccm.h>
#include "EMInternal.h"
#include "internal/em.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/cpumdis.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/vmm/dbgf.h>

#include <iprt/asm.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if 0 /* Disabled till after 2.1.0 when we've time to test it. */
#define EM_NOTIFY_HWACCM
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
DECLINLINE(int) emR3ExecuteInstruction(PVM pVM, PVMCPU pVCpu, const char *pszPrefix, int rcGC = VINF_SUCCESS);
static int emR3ExecuteIOInstruction(PVM pVM, PVMCPU pVCpu);
static int emR3HwaccmForcedActions(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);

#define EMHANDLERC_WITH_HWACCM
#include "EMHandleRCTmpl.h"


#if defined(DEBUG) && defined(SOME_UNUSED_FUNCTIONS)

/**
 * Steps hardware accelerated mode.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 * @param   pVCpu   The VMCPU handle.
 */
static int emR3HwAccStep(PVM pVM, PVMCPU pVCpu)
{
    Assert(pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_HWACC);

    int         rc;
    PCPUMCTX    pCtx   = pVCpu->em.s.pCtx;
    VMCPU_FF_CLEAR(pVCpu, (VMCPU_FF_SELM_SYNC_GDT | VMCPU_FF_SELM_SYNC_LDT | VMCPU_FF_TRPM_SYNC_IDT | VMCPU_FF_SELM_SYNC_TSS));

    /*
     * Check vital forced actions, but ignore pending interrupts and timers.
     */
    if (    VM_FF_ISPENDING(pVM, VM_FF_HIGH_PRIORITY_PRE_RAW_MASK)
        ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HIGH_PRIORITY_PRE_RAW_MASK))
    {
        rc = emR3HwaccmForcedActions(pVM, pVCpu, pCtx);
        if (rc != VINF_SUCCESS)
            return rc;
    }
    /*
     * Set flags for single stepping.
     */
    CPUMSetGuestEFlags(pVCpu, CPUMGetGuestEFlags(pVCpu) | X86_EFL_TF | X86_EFL_RF);

    /*
     * Single step.
     * We do not start time or anything, if anything we should just do a few nanoseconds.
     */
    do
    {
        rc = VMMR3HwAccRunGC(pVM, pVCpu);
    } while (   rc == VINF_SUCCESS
             || rc == VINF_EM_RAW_INTERRUPT);
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_RESUME_GUEST_MASK);

    /*
     * Make sure the trap flag is cleared.
     * (Too bad if the guest is trying to single step too.)
     */
    CPUMSetGuestEFlags(pVCpu, CPUMGetGuestEFlags(pVCpu) & ~X86_EFL_TF);

    /*
     * Deal with the return codes.
     */
    rc = emR3HighPriorityPostForcedActions(pVM, pVCpu, rc);
    rc = emR3HwaccmHandleRC(pVM, pVCpu, pCtx, rc);
    return rc;
}


static int emR3SingleStepExecHwAcc(PVM pVM, PVMCPU pVCpu, uint32_t cIterations)
{
    int     rc          = VINF_SUCCESS;
    EMSTATE enmOldState = pVCpu->em.s.enmState;
    pVCpu->em.s.enmState  = EMSTATE_DEBUG_GUEST_HWACC;

    Log(("Single step BEGIN:\n"));
    for (uint32_t i = 0; i < cIterations; i++)
    {
        DBGFR3PrgStep(pVCpu);
        DBGFR3DisasInstrCurrentLog(pVCpu, "RSS: ");
        rc = emR3HwAccStep(pVM, pVCpu);
        if (    rc != VINF_SUCCESS
            ||  !HWACCMR3CanExecuteGuest(pVM, pVCpu->em.s.pCtx))
            break;
    }
    Log(("Single step END: rc=%Rrc\n", rc));
    CPUMSetGuestEFlags(pVCpu, CPUMGetGuestEFlags(pVCpu) & ~X86_EFL_TF);
    pVCpu->em.s.enmState = enmOldState;
    return rc == VINF_SUCCESS ? VINF_EM_RESCHEDULE_REM : rc;
}

#endif /* DEBUG */


/**
 * Executes one (or perhaps a few more) instruction(s).
 *
 * @returns VBox status code suitable for EM.
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle
 * @param   rcRC        Return code from RC.
 * @param   pszPrefix   Disassembly prefix. If not NULL we'll disassemble the
 *                      instruction and prefix the log output with this text.
 */
#ifdef LOG_ENABLED
static int emR3ExecuteInstructionWorker(PVM pVM, PVMCPU pVCpu, int rcRC, const char *pszPrefix)
#else
static int emR3ExecuteInstructionWorker(PVM pVM, PVMCPU pVCpu, int rcRC)
#endif
{
#ifdef LOG_ENABLED
    PCPUMCTX pCtx = pVCpu->em.s.pCtx;
#endif
    int      rc;
    NOREF(rcRC);

    /*
     *
     * The simple solution is to use the recompiler.
     * The better solution is to disassemble the current instruction and
     * try handle as many as possible without using REM.
     *
     */

#ifdef LOG_ENABLED
    /*
     * Disassemble the instruction if requested.
     */
    if (pszPrefix)
    {
        DBGFR3InfoLog(pVM, "cpumguest", pszPrefix);
        DBGFR3DisasInstrCurrentLog(pVCpu, pszPrefix);
    }
#endif /* LOG_ENABLED */

#if 0
    /* Try our own instruction emulator before falling back to the recompiler. */
    DISCPUSTATE Cpu;
    rc = CPUMR3DisasmInstrCPU(pVM, pVCpu, pCtx, pCtx->rip, &Cpu, "GEN EMU");
    if (RT_SUCCESS(rc))
    {
        uint32_t size;

        switch (Cpu.pCurInstr->opcode)
        {
        /* @todo we can do more now */
        case OP_MOV:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_POP:
        case OP_INC:
        case OP_DEC:
        case OP_XCHG:
            STAM_PROFILE_START(&pVCpu->em.s.StatMiscEmu, a);
            rc = EMInterpretInstructionCPU(pVM, pVCpu, &Cpu, CPUMCTX2CORE(pCtx), 0, &size);
            if (RT_SUCCESS(rc))
            {
                pCtx->rip += Cpu.opsize;
#ifdef EM_NOTIFY_HWACCM
                if (pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_HWACC)
                    HWACCMR3NotifyEmulated(pVCpu);
#endif
                STAM_PROFILE_STOP(&pVCpu->em.s.StatMiscEmu, a);
                return rc;
            }
            if (rc != VERR_EM_INTERPRETER)
                AssertMsgFailedReturn(("rc=%Rrc\n", rc), rc);
            STAM_PROFILE_STOP(&pVCpu->em.s.StatMiscEmu, a);
            break;
        }
    }
#endif /* 0 */
    STAM_PROFILE_START(&pVCpu->em.s.StatREMEmu, a);
    Log(("EMINS: %04x:%RGv RSP=%RGv\n", pCtx->cs, (RTGCPTR)pCtx->rip, (RTGCPTR)pCtx->rsp));
    EMRemLock(pVM);
    /* Flush the recompiler TLB if the VCPU has changed. */
    if (pVM->em.s.idLastRemCpu != pVCpu->idCpu)
        CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_ALL);
    pVM->em.s.idLastRemCpu = pVCpu->idCpu;

    rc = REMR3EmulateInstruction(pVM, pVCpu);
    EMRemUnlock(pVM);
    STAM_PROFILE_STOP(&pVCpu->em.s.StatREMEmu, a);

#ifdef EM_NOTIFY_HWACCM
    if (pVCpu->em.s.enmState == EMSTATE_DEBUG_GUEST_HWACC)
        HWACCMR3NotifyEmulated(pVCpu);
#endif
    return rc;
}


/**
 * Executes one (or perhaps a few more) instruction(s).
 * This is just a wrapper for discarding pszPrefix in non-logging builds.
 *
 * @returns VBox status code suitable for EM.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pszPrefix   Disassembly prefix. If not NULL we'll disassemble the
 *                      instruction and prefix the log output with this text.
 * @param   rcGC        GC return code
 */
DECLINLINE(int) emR3ExecuteInstruction(PVM pVM, PVMCPU pVCpu, const char *pszPrefix, int rcGC)
{
#ifdef LOG_ENABLED
    return emR3ExecuteInstructionWorker(pVM, pVCpu, rcGC, pszPrefix);
#else
    return emR3ExecuteInstructionWorker(pVM, pVCpu, rcGC);
#endif
}

/**
 * Executes one (or perhaps a few more) IO instruction(s).
 *
 * @returns VBox status code suitable for EM.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 */
static int emR3ExecuteIOInstruction(PVM pVM, PVMCPU pVCpu)
{
    PCPUMCTX pCtx = pVCpu->em.s.pCtx;

    STAM_PROFILE_START(&pVCpu->em.s.StatIOEmu, a);

    /* Try to restart the io instruction that was refused in ring-0. */
    VBOXSTRICTRC rcStrict = HWACCMR3RestartPendingIOInstr(pVM, pVCpu, pCtx);
    if (IOM_SUCCESS(rcStrict))
    {
        STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->StatIoRestarted);
        STAM_PROFILE_STOP(&pVCpu->em.s.StatIOEmu, a);
        return VBOXSTRICTRC_TODO(rcStrict);     /* rip already updated. */
    }
    AssertMsgReturn(rcStrict == VERR_NOT_FOUND, ("%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)),
                    RT_SUCCESS_NP(rcStrict) ? VERR_INTERNAL_ERROR_5 : VBOXSTRICTRC_TODO(rcStrict));

    /** @todo probably we should fall back to the recompiler; otherwise we'll go back and forth between HC & GC
     *   as io instructions tend to come in packages of more than one
     */
    DISCPUSTATE Cpu;
    int rc2 = CPUMR3DisasmInstrCPU(pVM, pVCpu, pCtx, pCtx->rip, &Cpu, "IO EMU");
    if (RT_SUCCESS(rc2))
    {
        rcStrict = VINF_EM_RAW_EMULATE_INSTR;

        if (!(Cpu.prefix & (PREFIX_REP | PREFIX_REPNE)))
        {
            switch (Cpu.pCurInstr->opcode)
            {
                case OP_IN:
                {
                    STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->StatIn);
                    rcStrict = IOMInterpretIN(pVM, CPUMCTX2CORE(pCtx), &Cpu);
                    break;
                }

                case OP_OUT:
                {
                    STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->StatOut);
                    rcStrict = IOMInterpretOUT(pVM, CPUMCTX2CORE(pCtx), &Cpu);
                    break;
                }
            }
        }
        else if (Cpu.prefix & PREFIX_REP)
        {
            switch (Cpu.pCurInstr->opcode)
            {
                case OP_INSB:
                case OP_INSWD:
                {
                    STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->StatIn);
                    rcStrict = IOMInterpretINS(pVM, CPUMCTX2CORE(pCtx), &Cpu);
                    break;
                }

                case OP_OUTSB:
                case OP_OUTSWD:
                {
                    STAM_COUNTER_INC(&pVCpu->em.s.CTX_SUFF(pStats)->StatOut);
                    rcStrict = IOMInterpretOUTS(pVM, CPUMCTX2CORE(pCtx), &Cpu);
                    break;
                }
            }
        }

        /*
         * Handled the I/O return codes.
         * (The unhandled cases end up with rcStrict == VINF_EM_RAW_EMULATE_INSTR.)
         */
        if (IOM_SUCCESS(rcStrict))
        {
            pCtx->rip += Cpu.opsize;
            STAM_PROFILE_STOP(&pVCpu->em.s.StatIOEmu, a);
            return VBOXSTRICTRC_TODO(rcStrict);
        }

        if (rcStrict == VINF_EM_RAW_GUEST_TRAP)
        {
            /* The active trap will be dispatched. */
            Assert(TRPMHasTrap(pVCpu));
            STAM_PROFILE_STOP(&pVCpu->em.s.StatIOEmu, a);
            return VINF_SUCCESS;
        }
        AssertMsg(rcStrict != VINF_TRPM_XCPT_DISPATCHED, ("Handle VINF_TRPM_XCPT_DISPATCHED\n"));

        if (RT_FAILURE(rcStrict))
        {
            STAM_PROFILE_STOP(&pVCpu->em.s.StatIOEmu, a);
            return VBOXSTRICTRC_TODO(rcStrict);
        }
        AssertMsg(rcStrict == VINF_EM_RAW_EMULATE_INSTR || rcStrict == VINF_EM_RESCHEDULE_REM, ("rcStrict=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    }

    STAM_PROFILE_STOP(&pVCpu->em.s.StatIOEmu, a);
    return emR3ExecuteInstruction(pVM, pVCpu, "IO: ");
}


/**
 * Process raw-mode specific forced actions.
 *
 * This function is called when any FFs in the VM_FF_HIGH_PRIORITY_PRE_RAW_MASK is pending.
 *
 * @returns VBox status code. May return VINF_EM_NO_MEMORY but none of the other
 *          EM statuses.
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pCtx        The guest CPUM register context.
 */
static int emR3HwaccmForcedActions(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /*
     * Sync page directory.
     */
    if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL))
    {
        Assert(pVCpu->em.s.enmState != EMSTATE_WAIT_SIPI);
        int rc = PGMSyncCR3(pVCpu, pCtx->cr0, pCtx->cr3, pCtx->cr4, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
        if (RT_FAILURE(rc))
            return rc;

        Assert(!VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_SELM_SYNC_GDT | VMCPU_FF_SELM_SYNC_LDT));

        /* Prefetch pages for EIP and ESP. */
        /** @todo This is rather expensive. Should investigate if it really helps at all. */
        rc = PGMPrefetchPage(pVCpu, SELMToFlat(pVM, DIS_SELREG_CS, CPUMCTX2CORE(pCtx), pCtx->rip));
        if (rc == VINF_SUCCESS)
            rc = PGMPrefetchPage(pVCpu, SELMToFlat(pVM, DIS_SELREG_SS, CPUMCTX2CORE(pCtx), pCtx->rsp));
        if (rc != VINF_SUCCESS)
        {
            if (rc != VINF_PGM_SYNC_CR3)
            {
                AssertLogRelMsgReturn(RT_FAILURE(rc), ("%Rrc\n", rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                return rc;
            }
            rc = PGMSyncCR3(pVCpu, pCtx->cr0, pCtx->cr3, pCtx->cr4, VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
            if (RT_FAILURE(rc))
                return rc;
        }
        /** @todo maybe prefetch the supervisor stack page as well */
        Assert(!VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_SELM_SYNC_GDT | VMCPU_FF_SELM_SYNC_LDT));
    }

    /*
     * Allocate handy pages (just in case the above actions have consumed some pages).
     */
    if (VM_FF_IS_PENDING_EXCEPT(pVM, VM_FF_PGM_NEED_HANDY_PAGES, VM_FF_PGM_NO_MEMORY))
    {
        int rc = PGMR3PhysAllocateHandyPages(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Check whether we're out of memory now.
     *
     * This may stem from some of the above actions or operations that has been executed
     * since we ran FFs. The allocate handy pages must for instance always be followed by
     * this check.
     */
    if (VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY))
        return VINF_EM_NO_MEMORY;

    return VINF_SUCCESS;
}


/**
 * Executes hardware accelerated raw code. (Intel VT-x & AMD-V)
 *
 * This function contains the raw-mode version of the inner
 * execution loop (the outer loop being in EMR3ExecuteVM()).
 *
 * @returns VBox status code. The most important ones are: VINF_EM_RESCHEDULE, VINF_EM_RESCHEDULE_RAW,
 *          VINF_EM_RESCHEDULE_REM, VINF_EM_SUSPEND, VINF_EM_RESET and VINF_EM_TERMINATE.
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pfFFDone    Where to store an indicator telling whether or not
 *                      FFs were done before returning.
 */
int emR3HwAccExecute(PVM pVM, PVMCPU pVCpu, bool *pfFFDone)
{
    int      rc = VERR_INTERNAL_ERROR;
    PCPUMCTX pCtx = pVCpu->em.s.pCtx;

    LogFlow(("emR3HwAccExecute%d: (cs:eip=%04x:%RGv)\n", pVCpu->idCpu, pCtx->cs, (RTGCPTR)pCtx->rip));
    *pfFFDone = false;

    STAM_COUNTER_INC(&pVCpu->em.s.StatHwAccExecuteEntry);

#ifdef EM_NOTIFY_HWACCM
    HWACCMR3NotifyScheduled(pVCpu);
#endif

    /*
     * Spin till we get a forced action which returns anything but VINF_SUCCESS.
     */
    for (;;)
    {
        STAM_PROFILE_ADV_START(&pVCpu->em.s.StatHwAccEntry, a);

        /* Check if a forced reschedule is pending. */
        if (HWACCMR3IsRescheduleRequired(pVM, pCtx))
        {
            rc = VINF_EM_RESCHEDULE;
            break;
        }

        /*
         * Process high priority pre-execution raw-mode FFs.
         */
        VMCPU_FF_CLEAR(pVCpu, (VMCPU_FF_SELM_SYNC_GDT | VMCPU_FF_SELM_SYNC_LDT | VMCPU_FF_TRPM_SYNC_IDT | VMCPU_FF_SELM_SYNC_TSS)); /* not relevant in HWACCM mode; shouldn't be set really. */
        if (    VM_FF_ISPENDING(pVM, VM_FF_HIGH_PRIORITY_PRE_RAW_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HIGH_PRIORITY_PRE_RAW_MASK))
        {
            rc = emR3HwaccmForcedActions(pVM, pVCpu, pCtx);
            if (rc != VINF_SUCCESS)
                break;
        }

#ifdef LOG_ENABLED
        /*
         * Log important stuff before entering GC.
         */
        if (TRPMHasTrap(pVCpu))
            Log(("CPU%d: Pending hardware interrupt=0x%x cs:rip=%04X:%RGv\n", pVCpu->idCpu, TRPMGetTrapNo(pVCpu), pCtx->cs, (RTGCPTR)pCtx->rip));

        uint32_t cpl = CPUMGetGuestCPL(pVCpu, CPUMCTX2CORE(pCtx));

        if (pVM->cCpus == 1)
        {
            if (pCtx->eflags.Bits.u1VM)
                Log(("HWV86: %08X IF=%d\n", pCtx->eip, pCtx->eflags.Bits.u1IF));
            else if (CPUMIsGuestIn64BitCodeEx(pCtx))
                Log(("HWR%d: %04X:%RGv ESP=%RGv IF=%d IOPL=%d CR0=%x CR4=%x EFER=%x\n", cpl, pCtx->cs, (RTGCPTR)pCtx->rip, pCtx->rsp, pCtx->eflags.Bits.u1IF, pCtx->eflags.Bits.u2IOPL, (uint32_t)pCtx->cr0, (uint32_t)pCtx->cr4, (uint32_t)pCtx->msrEFER));
            else
                Log(("HWR%d: %04X:%08X ESP=%08X IF=%d IOPL=%d CR0=%x CR4=%x EFER=%x\n", cpl, pCtx->cs,          pCtx->eip, pCtx->esp, pCtx->eflags.Bits.u1IF, pCtx->eflags.Bits.u2IOPL, (uint32_t)pCtx->cr0, (uint32_t)pCtx->cr4, (uint32_t)pCtx->msrEFER));
        }
        else
        {
            if (pCtx->eflags.Bits.u1VM)
                Log(("HWV86-CPU%d: %08X IF=%d\n", pVCpu->idCpu, pCtx->eip, pCtx->eflags.Bits.u1IF));
            else if (CPUMIsGuestIn64BitCodeEx(pCtx))
                Log(("HWR%d-CPU%d: %04X:%RGv ESP=%RGv IF=%d IOPL=%d CR0=%x CR4=%x EFER=%x\n", cpl, pVCpu->idCpu, pCtx->cs, (RTGCPTR)pCtx->rip, pCtx->rsp, pCtx->eflags.Bits.u1IF, pCtx->eflags.Bits.u2IOPL, (uint32_t)pCtx->cr0, (uint32_t)pCtx->cr4, (uint32_t)pCtx->msrEFER));
            else
                Log(("HWR%d-CPU%d: %04X:%08X ESP=%08X IF=%d IOPL=%d CR0=%x CR4=%x EFER=%x\n", cpl, pVCpu->idCpu, pCtx->cs,          pCtx->eip, pCtx->esp, pCtx->eflags.Bits.u1IF, pCtx->eflags.Bits.u2IOPL, (uint32_t)pCtx->cr0, (uint32_t)pCtx->cr4, (uint32_t)pCtx->msrEFER));
        }
#endif /* LOG_ENABLED */

        /*
         * Execute the code.
         */
        STAM_PROFILE_ADV_STOP(&pVCpu->em.s.StatHwAccEntry, a);

        if (RT_LIKELY(EMR3IsExecutionAllowed(pVM, pVCpu)))
        {
            STAM_PROFILE_START(&pVCpu->em.s.StatHwAccExec, x);
            rc = VMMR3HwAccRunGC(pVM, pVCpu);
            STAM_PROFILE_STOP(&pVCpu->em.s.StatHwAccExec, x);
        }
        else
        {
            /* Give up this time slice; virtual time continues */
            STAM_REL_PROFILE_ADV_START(&pVCpu->em.s.StatCapped, u);
            RTThreadSleep(5);
            STAM_REL_PROFILE_ADV_STOP(&pVCpu->em.s.StatCapped, u);
            rc = VINF_SUCCESS;
        }


        /*
         * Deal with high priority post execution FFs before doing anything else.
         */
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_RESUME_GUEST_MASK);
        if (    VM_FF_ISPENDING(pVM, VM_FF_HIGH_PRIORITY_POST_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_HIGH_PRIORITY_POST_MASK))
            rc = emR3HighPriorityPostForcedActions(pVM, pVCpu, rc);

        /*
         * Process the returned status code.
         */
        if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
            break;

        rc = emR3HwaccmHandleRC(pVM, pVCpu, pCtx, rc);
        if (rc != VINF_SUCCESS)
            break;

        /*
         * Check and execute forced actions.
         */
#ifdef VBOX_HIGH_RES_TIMERS_HACK
        TMTimerPollVoid(pVM, pVCpu);
#endif
        if (    VM_FF_ISPENDING(pVM, VM_FF_ALL_MASK)
            ||  VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_ALL_MASK))
        {
            rc = emR3ForcedActions(pVM, pVCpu, rc);
            if (    rc != VINF_SUCCESS
                &&  rc != VINF_EM_RESCHEDULE_HWACC)
            {
                *pfFFDone = true;
                break;
            }
        }
    }

    /*
     * Return to outer loop.
     */
#if defined(LOG_ENABLED) && defined(DEBUG)
    RTLogFlush(NULL);
#endif
    return rc;
}

