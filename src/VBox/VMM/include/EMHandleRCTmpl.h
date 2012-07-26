/* $Id$ */
/** @file
 * EM - emR3[Raw|Hwaccm]HandleRC template.
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

#ifndef ___EMHandleRCTmpl_h
#define ___EMHandleRCTmpl_h

/**
 * Process a subset of the raw-mode and hwaccm return codes.
 *
 * Since we have to share this with raw-mode single stepping, this inline
 * function has been created to avoid code duplication.
 *
 * @returns VINF_SUCCESS if it's ok to continue raw mode.
 * @returns VBox status code to return to the EM main loop.
 *
 * @param   pVM     Pointer to the VM.
 * @param   pVCpu   Pointer to the VMCPU.
 * @param   rc      The return code.
 * @param   pCtx    Pointer to the guest CPU context.
 */
#ifdef EMHANDLERC_WITH_PATM
int emR3RawHandleRC(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, int rc)
#elif defined(EMHANDLERC_WITH_HWACCM)
int emR3HwaccmHandleRC(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, int rc)
#endif
{
    switch (rc)
    {
        /*
         * Common & simple ones.
         */
        case VINF_SUCCESS:
            break;
        case VINF_EM_RESCHEDULE_RAW:
        case VINF_EM_RESCHEDULE_HWACC:
        case VINF_EM_RAW_INTERRUPT:
        case VINF_EM_RAW_TO_R3:
        case VINF_EM_RAW_TIMER_PENDING:
        case VINF_EM_PENDING_REQUEST:
            rc = VINF_SUCCESS;
            break;

#ifdef EMHANDLERC_WITH_PATM
        /*
         * Privileged instruction.
         */
        case VINF_EM_RAW_EXCEPTION_PRIVILEGED:
        case VINF_PATM_PATCH_TRAP_GP:
            rc = emR3RawPrivileged(pVM, pVCpu);
            break;

        case VINF_EM_RAW_GUEST_TRAP:
            /*
             * Got a trap which needs dispatching.
             */
            if (PATMR3IsInsidePatchJump(pVM, pCtx->eip, NULL))
            {
                AssertReleaseMsgFailed(("FATAL ERROR: executing random instruction inside generated patch jump %08X\n", CPUMGetGuestEIP(pVCpu)));
                rc = VERR_EM_RAW_PATCH_CONFLICT;
                break;
            }
            rc = emR3RawGuestTrap(pVM, pVCpu);
            break;

        /*
         * Trap in patch code.
         */
        case VINF_PATM_PATCH_TRAP_PF:
        case VINF_PATM_PATCH_INT3:
            rc = emR3PatchTrap(pVM, pVCpu, pCtx, rc);
            break;

        case VINF_PATM_DUPLICATE_FUNCTION:
            Assert(PATMIsPatchGCAddr(pVM, pCtx->eip));
            rc = PATMR3DuplicateFunctionRequest(pVM, pCtx);
            AssertRC(rc);
            rc = VINF_SUCCESS;
            break;

        case VINF_PATM_CHECK_PATCH_PAGE:
            rc = PATMR3HandleMonitoredPage(pVM);
            AssertRC(rc);
            rc = VINF_SUCCESS;
            break;

        /*
         * Patch manager.
         */
        case VERR_EM_RAW_PATCH_CONFLICT:
            AssertReleaseMsgFailed(("%Rrc handling is not yet implemented\n", rc));
            break;
#endif /* EMHANDLERC_WITH_PATM */

#ifdef EMHANDLERC_WITH_PATM
        /*
         * Memory mapped I/O access - attempt to patch the instruction
         */
        case VINF_PATM_HC_MMIO_PATCH_READ:
            rc = PATMR3InstallPatch(pVM, SELMToFlat(pVM, DISSELREG_CS, CPUMCTX2CORE(pCtx), pCtx->eip),
                                      PATMFL_MMIO_ACCESS
                                    | (CPUMGetGuestCodeBits(pVCpu) == 32 ? PATMFL_CODE32 : 0));
            if (RT_FAILURE(rc))
                rc = emR3ExecuteInstruction(pVM, pVCpu, "MMIO");
            break;

        case VINF_PATM_HC_MMIO_PATCH_WRITE:
            AssertFailed(); /* not yet implemented. */
            rc = emR3ExecuteInstruction(pVM, pVCpu, "MMIO");
            break;
#endif /* EMHANDLERC_WITH_PATM */

        /*
         * Conflict or out of page tables.
         *
         * VM_FF_PGM_SYNC_CR3 is set by the hypervisor and all we need to
         * do here is to execute the pending forced actions.
         */
        case VINF_PGM_SYNC_CR3:
            AssertMsg(VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL),
                      ("VINF_PGM_SYNC_CR3 and no VMCPU_FF_PGM_SYNC_CR3*!\n"));
            rc = VINF_SUCCESS;
            break;

        /*
         * PGM pool flush pending (guest SMP only).
         */
        /** @todo jumping back and forth between ring 0 and 3 can burn a lot of cycles
         * if the EMT thread that's supposed to handle the flush is currently not active
         * (e.g. waiting to be scheduled) -> fix this properly!
         *
         * bird: Since the clearing is global and done via a rendezvous any CPU can do
         *       it. They would have to choose who to call VMMR3EmtRendezvous and send
         *       the rest to VMMR3EmtRendezvousFF ... Hmm ... that's not going to work
         *       all that well since the latter will race the setup done by the
         *       first.  Guess that means we need some new magic in that area for
         *       handling this case. :/
         */
        case VINF_PGM_POOL_FLUSH_PENDING:
            rc = VINF_SUCCESS;
            break;

        /*
         * Paging mode change.
         */
        case VINF_PGM_CHANGE_MODE:
            rc = PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);
            if (rc == VINF_SUCCESS)
                rc = VINF_EM_RESCHEDULE;
            AssertMsg(RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST), ("%Rrc\n", rc));
            break;

#ifdef EMHANDLERC_WITH_PATM
        /*
         * CSAM wants to perform a task in ring-3. It has set an FF action flag.
         */
        case VINF_CSAM_PENDING_ACTION:
            rc = VINF_SUCCESS;
            break;

        /*
         * Invoked Interrupt gate - must directly (!) go to the recompiler.
         */
        case VINF_EM_RAW_INTERRUPT_PENDING:
        case VINF_EM_RAW_RING_SWITCH_INT:
            Assert(TRPMHasTrap(pVCpu));
            Assert(!PATMIsPatchGCAddr(pVM, pCtx->eip));

            if (TRPMHasTrap(pVCpu))
            {
                /* If the guest gate is marked unpatched, then we will check again if we can patch it. */
                uint8_t u8Interrupt = TRPMGetTrapNo(pVCpu);
                if (TRPMR3GetGuestTrapHandler(pVM, u8Interrupt) == TRPM_INVALID_HANDLER)
                {
                    CSAMR3CheckGates(pVM, u8Interrupt, 1);
                    Log(("emR3RawHandleRC: recheck gate %x -> valid=%d\n", u8Interrupt, TRPMR3GetGuestTrapHandler(pVM, u8Interrupt) != TRPM_INVALID_HANDLER));
                    /* Note: If it was successful, then we could go back to raw mode, but let's keep things simple for now. */
                }
            }
            rc = VINF_EM_RESCHEDULE_REM;
            break;

        /*
         * Other ring switch types.
         */
        case VINF_EM_RAW_RING_SWITCH:
            rc = emR3RawRingSwitch(pVM, pVCpu);
            break;
#endif  /* EMHANDLERC_WITH_PATM */

        /*
         * I/O Port access - emulate the instruction.
         */
        case VINF_IOM_R3_IOPORT_READ:
        case VINF_IOM_R3_IOPORT_WRITE:
            rc = emR3ExecuteIOInstruction(pVM, pVCpu);
            break;

        /*
         * Memory mapped I/O access - emulate the instruction.
         */
        case VINF_IOM_R3_MMIO_READ:
        case VINF_IOM_R3_MMIO_WRITE:
        case VINF_IOM_R3_MMIO_READ_WRITE:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "MMIO");
            break;

#ifdef EMHANDLERC_WITH_HWACCM
        /*
         * (MM)IO intensive code block detected; fall back to the recompiler for better performance
         */
        case VINF_EM_RAW_EMULATE_IO_BLOCK:
            rc = HWACCMR3EmulateIoBlock(pVM, pCtx);
            break;

        case VINF_EM_HWACCM_PATCH_TPR_INSTR:
            rc = HWACCMR3PatchTprInstr(pVM, pVCpu, pCtx);
            break;
#endif

#ifdef EMHANDLERC_WITH_PATM
        /*
         * Execute instruction.
         */
        case VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "LDT FAULT: ");
            break;
        case VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "GDT FAULT: ");
            break;
        case VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "IDT FAULT: ");
            break;
        case VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "TSS FAULT: ");
            break;
        case VINF_EM_RAW_EMULATE_INSTR_PD_FAULT:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "PD FAULT: ");
            break;
        case VINF_EM_RAW_EMULATE_INSTR_HLT:
            /** @todo skip instruction and go directly to the halt state. (see REM for implementation details) */
            rc = emR3RawPrivileged(pVM, pVCpu);
            break;
#endif

#ifdef EMHANDLERC_WITH_PATM
        case VINF_PATM_PENDING_IRQ_AFTER_IRET:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "EMUL: ", VINF_PATM_PENDING_IRQ_AFTER_IRET);
            break;

        case VINF_PATCH_EMULATE_INSTR:
#else
        case VINF_EM_RAW_GUEST_TRAP:
#endif
        case VINF_EM_RAW_EMULATE_INSTR:
            rc = emR3ExecuteInstruction(pVM, pVCpu, "EMUL: ");
            break;

#ifdef EMHANDLERC_WITH_PATM
        /*
         * Stale selector and iret traps => REM.
         */
        case VINF_EM_RAW_STALE_SELECTOR:
        case VINF_EM_RAW_IRET_TRAP:
            /* We will not go to the recompiler if EIP points to patch code. */
            if (PATMIsPatchGCAddr(pVM, pCtx->eip))
            {
                pCtx->eip = PATMR3PatchToGCPtr(pVM, (RTGCPTR)pCtx->eip, 0);
            }
            LogFlow(("emR3RawHandleRC: %Rrc -> %Rrc\n", rc, VINF_EM_RESCHEDULE_REM));
            rc = VINF_EM_RESCHEDULE_REM;
            break;

        /*
         * Conflict in GDT, resync and continue.
         */
        case VINF_SELM_SYNC_GDT:
            AssertMsg(VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_SELM_SYNC_GDT), ("VINF_SELM_SYNC_GDT without VMCPU_FF_SELM_SYNC_GDT!\n"));
            rc = VINF_SUCCESS;
            break;
#endif

        /*
         * Up a level.
         */
        case VINF_EM_TERMINATE:
        case VINF_EM_OFF:
        case VINF_EM_RESET:
        case VINF_EM_SUSPEND:
        case VINF_EM_HALT:
        case VINF_EM_RESUME:
        case VINF_EM_NO_MEMORY:
        case VINF_EM_RESCHEDULE:
        case VINF_EM_RESCHEDULE_REM:
        case VINF_EM_WAIT_SIPI:
            break;

        /*
         * Up a level and invoke the debugger.
         */
        case VINF_EM_DBG_STEPPED:
        case VINF_EM_DBG_BREAKPOINT:
        case VINF_EM_DBG_STEP:
        case VINF_EM_DBG_HYPER_BREAKPOINT:
        case VINF_EM_DBG_HYPER_STEPPED:
        case VINF_EM_DBG_HYPER_ASSERTION:
        case VINF_EM_DBG_STOP:
            break;

        /*
         * Up a level, dump and debug.
         */
        case VERR_TRPM_DONT_PANIC:
        case VERR_TRPM_PANIC:
        case VERR_VMM_RING0_ASSERTION:
        case VERR_VMM_HYPER_CR3_MISMATCH:
        case VERR_VMM_RING3_CALL_DISABLED:
        case VERR_IEM_INSTR_NOT_IMPLEMENTED:
        case VERR_IEM_ASPECT_NOT_IMPLEMENTED:
            break;

#ifdef EMHANDLERC_WITH_HWACCM
        /*
         * Up a level, after HwAccM have done some release logging.
         */
        case VERR_VMX_INVALID_VMCS_FIELD:
        case VERR_VMX_INVALID_VMCS_PTR:
        case VERR_VMX_INVALID_VMXON_PTR:
        case VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_CODE:
        case VERR_VMX_UNEXPECTED_EXCEPTION:
        case VERR_VMX_UNEXPECTED_EXIT_CODE:
        case VERR_VMX_INVALID_GUEST_STATE:
        case VERR_VMX_UNABLE_TO_START_VM:
        case VERR_VMX_UNABLE_TO_RESUME_VM:
            HWACCMR3CheckError(pVM, rc);
            break;

        /* Up a level; fatal */
        case VERR_VMX_IN_VMX_ROOT_MODE:
        case VERR_SVM_IN_USE:
        case VERR_SVM_UNABLE_TO_START_VM:
            break;
#endif

        /*
         * Anything which is not known to us means an internal error
         * and the termination of the VM!
         */
        default:
            AssertMsgFailed(("Unknown GC return code: %Rra\n", rc));
            break;
    }
    return rc;
}

#endif

